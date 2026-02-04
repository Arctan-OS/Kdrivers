#include "drivers/sysdev/nvme/nvme.h"
#include "arch/smp.h"
#include "drivers/resource.h"
#include "drivers/dri_defs.h"
#include "lib/ringbuffer.h"
#include "mm/allocator.h"
#include "mm/pmm.h"
#include <stddef.h>

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
	// Configure IO sub / comp queue sizes
        ARC_Resource *transport = state->transport;
        ARC_ControlPacketInstruction _cmd = { .command = NVME_TRANSPORT_CTRL_TO_PROPS };
        transport->driver->control(transport, &_cmd);

        ctrl_props_t *props = alloc(sizeof(*props));

        if (props == NULL) {
                return -1;
        }

        transport->driver->read(props, sizeof(*props), 1, NULL, transport);
	MASKED_WRITE(props->cc, 6, 16, 0xF);
	MASKED_WRITE(props->cc, 4, 20, 0xF);
        transport->driver->write(props, sizeof(*props), 1, NULL, transport);
        
        free(props);

        // Request qcount qpairs
	struct qs_entry cmd = {
	        .cdw0.opcode = 0x9,
		.cdw10 = 0x7,
		.cdw11 = (qcount - 1) | ((qcount - 1) << 16)
        };

	qs_wrap_t wrap = state->submit(state->transport, NULL, &cmd);
        qc_entry_t ret = { 0 };
	state->poll(state->transport, &wrap, &ret);

	return min(MASKED_READ(ret.dw0, 0, 0xFFFF), MASKED_READ(ret.dw0, 16, 0xFFFF));
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

                io_qpairs[i].phase = 0;
                io_qpairs[i].id = i + 1;
                io_qpairs[i].subq = sub;
                io_qpairs[i].cmpq = cmp;
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
                free(state);
                return -4;
        }

        ARC_ControlPacketResponse resp = transport->driver->control(transport, &cmd);

        if (resp.data == NULL) {
                free(state);
                return -5;
        }

        
        state->poll = ident.poll;
        state->submit = ident.submit;
        
        nvme_identify_controller(state);
        
        // TODO: Get a list of namespaces
        uint16_t namespaces = 0;
        uint16_t requested = namespaces * Arc_ProcessorCounter;
        uint16_t granted = nvme_request_io_queues(state, requested);

        if (granted == 0) {
                free(state);
                return -6;
        }
        
        size_t qsize = 0x1000;
        if (nvme_create_io_qpairs(state, granted, qsize) != 0) {
                free(state);
                return -7;
        }
        
        state->qpairs.requested = requested;
        state->qpairs.granted = granted;
        // TODO: Initialize those namespaces
        
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
