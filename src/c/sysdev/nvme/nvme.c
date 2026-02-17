#include "drivers/sysdev/nvme/nvme.h"
#include "arch/smp.h"
#include "drivers/resource.h"
#include "drivers/dri_defs.h"
#include "lib/ringbuffer.h"
#include "mm/allocator.h"
#include "mm/pmm.h"
#include <stddef.h>

typedef struct nvme_namespace {
        struct nvme_namespace *next;
        nvme_namespace_args_t arg;
} nvme_namespace_t;

static int nvme_identify_controller(nvme_driver_state_t *state) {
	uint8_t *data = (uint8_t *)pmm_fast_page_alloc();

	qs_entry_t cmd = {
	        .cdw0.opcode = 0x6,
		.prp.entry1 = ARC_HHDM_TO_PHYS(data),
		.cdw10 = 0x1,
        };

	qs_wrap_t wrap = state->submit(state->transport, NULL, &cmd);
	state->poll(state->transport, &wrap, NULL);

	// 77    Maximum Data Transfer Size in 2 << cap.mpsmin
	//       CTRATT mem bit cleared, includes the size of the interleaved metadata
	//       mem bit set then this is size of user data only
	state->ctrl_iden.max_transfer_size = data[77];

	// 79:78 CNTLID
	state->ctrl_iden.id = *(uint16_t *)(&data[78]);

	// 83:80 VERS
	state->ctrl_iden.version = *(uint32_t *)(&data[80]);

	// 99:96 CTRATT bit 16 is MEM for 77
	//              bit 11 is multi-domain subsystem
	//              bit 10 UUID list 1: supported
	state->ctrl_iden.ctratt = *(uint32_t *)(&data[96]);

	// 111   Controller type (0: resv, 1: IO, 2: discovery, 3 ADMIN, all else resv)
	state->ctrl_iden.type = data[111];

	// TODO: CRDTs

	cmd.cdw10 = 0x2;
	wrap = state->submit(state->transport, NULL, &cmd);
	state->poll(state->transport, &wrap, NULL);

	pmm_fast_page_free(data);

	return 0;
}

static uint16_t nvme_request_io_queues(nvme_driver_state_t *state, uint16_t qcount) {
        // Request qcount qpairs
	struct qs_entry cmd = {
	        .cdw0.opcode = 0x9,
		.cdw10 = 0x7,
		.cdw11 = (qcount - 1) | ((qcount - 1) << 16)
        };

	qs_wrap_t wrap = state->submit(state->transport, NULL, &cmd);
        qc_entry_t ret = { 0 };
	state->poll(state->transport, &wrap, &ret);

	return min(MASKED_READ(ret.dw0, 0, 0xFFFF), MASKED_READ(ret.dw0, 16, 0xFFFF)) + 1;
}

static int nvme_create_io_qpairs(nvme_driver_state_t *state, uint16_t count, size_t qsize) {                
        nvme_qpair_t *io_qpairs = alloc(sizeof(*io_qpairs) * count);
        
        if (io_qpairs == NULL) {
                ARC_DEBUG(ERR, "Failed to allocate io qpairs\n");
                free(state);
                return -1;
        }

        uint16_t i = 0;
        for (; i < count; i++) {
                void *base = pmm_alloc(qsize * 2);

                if (base == NULL) {
                        ARC_DEBUG(ERR, "Failed to allocate base for io qpair %d\n", i);
                        break;
                }

                //memset(base, 0, qsize * 2);
                
                ARC_Ringbuffer *sub = init_ringbuffer(base, qsize / sizeof(qs_entry_t), sizeof(qs_entry_t));
                if (sub == NULL) {
                        ARC_DEBUG(ERR, "Failed to create ringbuffer structure for io qpair %d (submission)\n", i);
                        break;
                }
                
                ARC_Ringbuffer *cmp = init_ringbuffer(base + qsize, qsize / sizeof(qc_entry_t), sizeof(qc_entry_t));

                if (cmp == NULL) {
                        ARC_DEBUG(ERR, "Failed to create ringbuffer structure for io qpair %d (completion)\n", i);
                        break;
                }
                
                io_qpairs[i].phase = 1;
                io_qpairs[i].id = i + 1;
                io_qpairs[i].subq = sub;
                io_qpairs[i].cmpq = cmp;

                ARC_DEBUG(INFO, "Create qpair %d with base %p\n", i, base);
        }

        if (i != count) {
                for (int _i = i; _i >= 0; _i--) {
                        pmm_free(io_qpairs[_i].subq->base);
                }
                free(io_qpairs);
                
                return -1;
        }

        state->qpairs.qs = io_qpairs;

        return 0;
}

static uint64_t nvme_set_command_sets(nvme_driver_state_t *state) {
        ARC_Resource *transport = state->transport;
        ARC_ControlPacketInstruction _cmd = { .command = NVME_TRANSPORT_CTRL_TO_PROPS };
        transport->driver->control(transport, &_cmd);

        ctrl_props_t *props = alloc(sizeof(*props));

        if (props == NULL) {
                return -1;
        }

        transport->driver->read(props, sizeof(*props), 1, NULL, transport);

	uint64_t cap = props->cap;
	uint64_t cc =  props->cc;

        free(props);
        
	if (MASKED_READ(cap, 43, 1) == 1) {
		// CAP.CSS.IOCSS not set
		uint64_t *iocs_struct = (uint64_t *)pmm_fast_page_alloc();

		struct qs_entry iocs_struct_cmd = {
	                .cdw0.opcode = 0x6,
			.prp.entry1 = ARC_HHDM_TO_PHYS(iocs_struct),
	                .cdw10 = 0x1C | (state->ctrl_iden.id << 16),
                };

                qs_wrap_t wrap = state->submit(state->transport, NULL, &iocs_struct_cmd);
                state->poll(state->transport, &wrap, NULL);
                
		uint32_t i = 0;
		uint64_t enabled_cmd_sets = 0;

                // TODO: This is just refactored old code, but this does need to be gone
                //       over, why would there be 512 possible entries and only one of them
                //       used?
		for (; i < 512; i++) {
			if (iocs_struct[i] != 0) {
				enabled_cmd_sets = iocs_struct[i];
				break;
			}
		}

		pmm_fast_page_free(iocs_struct);

		struct qs_entry set_cmd = {
		        .cdw0.opcode = 0x9,
			.cdw10 = 0x19,
			.cdw11 = i & 0xFF,
	        };

                wrap = state->submit(state->transport, NULL, &set_cmd);
                qc_entry_t ret = { 0 };
		state->poll(state->transport, &wrap, &ret);

		if ((ret.dw0 & 0xFF) != i) {
			ARC_DEBUG(ERR, "Command set not set to desired command set (TODO)\n");
		}

		return enabled_cmd_sets;
	} else if (MASKED_READ(cc, 1, 0b111) == 0) {
		// NVM Command Set is enabled
		return 0x1;
	}

	return 0;
}

static int nvme_list_namespaces(nvme_driver_state_t *state, nvme_namespace_t **ret, uint64_t sets) {
        int ns_count = 0;
        
        while (sets != 0) {
		int idx = __builtin_ffs(sets) - 1;

		uint32_t *namespaces = (uint32_t *)pmm_fast_page_alloc();

		struct qs_entry get_ns_cmd = {
	                .cdw0.opcode = 0x6,
			.prp.entry1 = ARC_HHDM_TO_PHYS(namespaces),
	                .cdw10 = 0x7 | (state->ctrl_iden.id << 16),
			.cdw11 = (idx & 0xFF) << 24,
			.nsid = 0x0,
                };

                uint32_t _t = 0;
                
                do {
                        get_ns_cmd.nsid = _t;
                        memset(namespaces, 0, PAGE_SIZE);
                        
                        qs_wrap_t wrap = state->submit(state->transport, NULL, &get_ns_cmd);
                        qc_entry_t cmp = { 0 };
                        state->poll(state->transport, &wrap, &cmp);

                        if (cmp.status != 0) {
                                ARC_DEBUG(ERR, "Failed to get list of 1024 active namespaces from namespace %d (idx=%d)\n", _t, idx);
                                break;
                        }
                        
                        for (int i = 0; i < 1024; i++) {
                                _t = namespaces[i];
                                
                                if (namespaces[i] == 0) {
                                        break;
                                }

                                ARC_DEBUG(INFO, "Found active namespace %d in command set idx=%d\n", _t, idx);
                                
                                nvme_namespace_t *ns = alloc(sizeof(*ns));
                                
                                if (ns == NULL) {
                                        ARC_DEBUG(ERR, "Failed to allocate space for namespace arguments (%\n");
                                        break;
                                }
                                
                                ns->next = *ret;
                                ns->arg.command_set = idx;
                                ns->arg.namespace = namespaces[i];
                                ns->arg.state = state;
                                *ret = ns;
                                ns_count++;
                        }
                } while(_t != 0);
                
		pmm_fast_page_free(namespaces);

		sets &= ~(1 << (idx));
	}
 
	return ns_count;
}

int init_nvme(ARC_Resource *res, void *arg) {
        if (arg == NULL) {
                ARC_DEBUG(ERR, "NULL pointer given for transport resource\n");
                return -1;
        }
        
        nvme_driver_state_t *state = alloc(sizeof(*state));

        if (state == NULL) {
                ARC_DEBUG(INFO, "Failed to allocate driver state\n");
                return -2;
        }

        memset(state, 0, sizeof(*state));

        ARC_DEBUG(INFO, "Initializing general NVME driver with transport=%p\n", arg);
        
        ARC_Resource *transport = arg;
        state->transport = transport;
        
        nvme_transport_iden_t ident = { 0 };
        ARC_ControlPacketInstruction cmd = { .command = NVME_TRANSPORT_CTRL_IDEN,
                                             .data    = &ident };

        if (transport->driver->control == NULL) {
                ARC_DEBUG(ERR, "No control function specified by transport\n");
                free(state);
                return -4;
        }

        ARC_ControlPacketResponse resp = transport->driver->control(transport, &cmd);

        if (resp.data == NULL) {
                ARC_DEBUG(ERR, "Failed to identify transport\n");
                free(state);
                return -5;
        }
        
        state->poll = ident.poll;
        state->submit = ident.submit;
        
        nvme_identify_controller(state);
        int sets = nvme_set_command_sets(state);
        
        nvme_namespace_t *namespaces = NULL;
        uint16_t ns_count = nvme_list_namespaces(state, &namespaces, sets);
        uint16_t requested = ns_count * Arc_ProcessorCounter;
        uint16_t granted = nvme_request_io_queues(state, requested);

        ARC_DEBUG(INFO, "ns_count: %d, requested: %d, granted: %d\n", ns_count, requested, granted);
        
        if (granted == 0) {
                ARC_DEBUG(ERR, "No queues were granted\n");
                free(state);
                return -6;
        }
        
        size_t qsize = 0x1000;
        if (nvme_create_io_qpairs(state, min(requested, granted), qsize) != 0) {
                ARC_DEBUG(ERR, "Failed to create all io qpairs\n");
                free(state);
                return -7;
        }
        
        state->qpairs.requested = requested;
        state->qpairs.granted = granted;

        nvme_namespace_t *namespace = namespaces;
        while (namespace != NULL) {
                init_resource(ARC_DRIGRP_DEV, ARC_DRIDEF_DEV_NVME_NAMESPACE, &namespace->arg);
                namespace = namespace->next;
        }
        
        res->driver_state = state;
        
        return 0;
}

int uninit_nvme(ARC_Resource *resource) {
        (void)resource;
        return 0;
}

static size_t read_nvme(void *buffer, size_t size, size_t count, ARC_File *file, ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
	}

        return 0;
}

static size_t write_nvme(void *buffer, size_t size, size_t count, ARC_File *file, ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
	}
        
        return 0;
}

static int stat_nvme_pci(ARC_Resource *res, char *filename, struct stat *stat) {
	(void)res;
	(void)filename;
	(void)stat;
        return 0;
}

ARC_REGISTER_DRIVER(ARC_DRIGRP_DEV, nvme) = {
        .init = init_nvme,
	.uninit = uninit_nvme,
	.read = read_nvme,
	.write = write_nvme,
	.seek = dridefs_int_func_empty,
	.rename = dridefs_int_func_empty,
	.stat = stat_nvme_pci,
	.codes = NULL
};
