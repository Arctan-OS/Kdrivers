#include "arch/info.h"
#include "arch/x86-64/util.h"
#include "drivers/sysdev/nvme/nvme.h"
#include "drivers/dri_defs.h"
#include "drivers/resource.h"
#include "mm/pmm.h"
#include "mm/allocator.h"
#include "drivers/sysdev/nvme/nvme.h"
#include "arch/pager.h"
#include "lib/ringbuffer.h"

#define SQnTDBL(_properties, _n) ((uintptr_t)_properties->data + ((2 * (_n)) * (4 << MASKED_READ(_properties->cap, 32, 0b1111))))
#define CQnHDBL(_properties, _n) ((uintptr_t)_properties->data + ((2 * (_n) + 1) * (4 << MASKED_READ(_properties->cap, 32, 0b1111))))

typedef struct driver_state {
        ctrl_props_t *props;
        int exposed;
        nvme_qpair_t adminq;
} driver_state_t;

static qs_wrap_t nvme_pci_submit_command(ARC_Resource *transport, nvme_qpair_t *qpair, qs_entry_t *cmd) {
	if (transport == NULL || cmd == NULL) {
		return (qs_wrap_t){ 0 };
	}

        bool I = arch_interrupts_enabled();
        ARC_DISABLE_INTERRUPT;
        
        driver_state_t *state = transport->driver_state;
        bool admin = false;
        if (qpair == NULL) {
                qpair = &state->adminq;
                admin = true;
        }

	size_t ptr = ringbuffer_allocate(qpair->subq, 1);

	if (admin) {
		cmd->cdw0.cid = (1 << 15) | (ptr & 0xFF);
	} else {
		cmd->cdw0.cid = (qpair->id & 0x3F) | ((ptr & 0xFF) << 6);
	}

	ringbuffer_write(qpair->subq, ptr, cmd);
        
	uint32_t *doorbell = (uint32_t *)SQnTDBL(state->props, qpair->id);
	*doorbell = ((uint32_t)ptr) + 1;
        
        if (I) {
                ARC_ENABLE_INTERRUPT;
        }
        
	return (qs_wrap_t){ .cmd = cmd, .qpair = qpair };
}

static int nvme_pci_poll_completion(ARC_Resource *transport, qs_wrap_t *wrap, qc_entry_t *ret) {
	if (transport == NULL || wrap->cmd == NULL) {
		return -1;
	}

        nvme_qpair_t *qpair = wrap->qpair;
        qs_entry_t *cmd = wrap->cmd;
	qc_entry_t *qc = (struct qc_entry *)qpair->cmpq->base;
        
        bool I = arch_interrupts_enabled();
        //ARC_ENABLE_INTERRUPT; // Causes a double fault
        
	size_t i = 0;
	while (1) {
		i = qpair->cmpq->idx;

		if (qc[i].phase == qpair->phase && qc[i].cid == cmd->cdw0.cid) {
			break;
		}
	}

        if (!I) {
                ARC_DISABLE_INTERRUPT;
        }
        
	int status = qc[i].status;

	if (ret != NULL) {
		memcpy(ret, &qc[i], sizeof(*ret));
	}

	size_t idx = ringbuffer_allocate(qpair->cmpq, 1);

	if (idx == qpair->cmpq->objs - 1) {
		qpair->phase = !qpair->phase;
	}

        driver_state_t *state = transport->driver_state;
	uint32_t *doorbell = (uint32_t *)CQnHDBL(state->props, qpair->id);
	*doorbell = ((uint32_t)idx) + 1;

        int cmd_idx = qpair->id ? (cmd->cdw0.cid >> 6) & 0xFF : (cmd->cdw0.cid);
	ringbuffer_free(qpair->subq, cmd_idx);

	return status;
}

static int create_admin_qpair(driver_state_t *state, size_t qsize) {
	void *queues = pmm_alloc(qsize * 2);

	if (queues == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate adminstrator queues\n");
		return -1;
	}

        ARC_Ringbuffer *sub = init_ringbuffer(queues, NVME_ADMIN_QUEUE_SUB_LEN, sizeof(qs_entry_t));

        if (sub == NULL) {
                ARC_DEBUG(ERR, "Failed to create ringbuffer for submission queue\n");
                return -2;
        }
        
        ARC_Ringbuffer *comp = init_ringbuffer(queues + qsize, NVME_ADMIN_QUEUE_SUB_LEN, sizeof(qs_entry_t));

        if (comp == NULL) {
                ARC_DEBUG(ERR, "Failed to create ringbuffer for completion queue\n");
                // TODO: Delete ringbuffer
                return -3;
        }

        state->adminq.id = 0;
        state->adminq.cmpq = comp;
        state->adminq.subq = sub;

        ctrl_props_t *props = state->props;
        
	memset(queues, 0, PAGE_SIZE * 2);
	props->asq = ARC_HHDM_TO_PHYS(queues);
	props->acq = ARC_HHDM_TO_PHYS(queues) + qsize;

	MASKED_WRITE(props->aqa, NVME_ADMIN_QUEUE_SUB_LEN - 1, 0, 0xFFF);
	MASKED_WRITE(props->aqa, NVME_ADMIN_QUEUE_COMP_LEN - 1, 16, 0xFFF);
        
        return 0;
}

static int reset_controller(driver_state_t *state) {
        if (state == NULL) {
		ARC_DEBUG(ERR, "Failed to reset controller, state or properties NULL\n");
		return -1;
	}

        ctrl_props_t *props = state->props;
        
	// Disable
	MASKED_WRITE(props->cc, 0, 0, 1);

        // TODO: Portability?
        // TODO: A timeout?
	while (MASKED_READ(props->csts, 0, 1)) __builtin_ia32_pause();
        
        if (create_admin_qpair(state, PAGE_SIZE) != 0) {
                return -2;
        }
        
	// Set CC.CSS
	uint8_t cap_css = MASKED_READ(props->cap, 37, 0xFF);

	if ((cap_css >> 7) & 1) {
		MASKED_WRITE(props->cc, 0b111, 4, 0b111);
	}

	if ((cap_css >> 6) & 1) {
		MASKED_WRITE(props->cc, 0b110, 4, 0b111);
	}

	if (((cap_css >> 6) & 1) == 0 && ((cap_css >> 0) & 1) == 1) {
		MASKED_WRITE(props->cc, 0b000, 4, 0b111);
	}

	// Set MPS and AMS
	MASKED_WRITE(props->cc, 0, 7, 0b1111);
	MASKED_WRITE(props->cc, 0, 11, 0b111);
        
	// Enable
	MASKED_WRITE(props->cc, 1, 0, 1);

        // TODO: Portability?
        // TODO: A timeout?
	while (!MASKED_READ(props->csts, 0, 1)) __builtin_ia32_pause();
        
	return 0;
}

static int uninit_nvme_pci(ARC_Resource *);
int init_nvme_pci(ARC_Resource *res, void *arg) {
        driver_state_t *state = alloc(sizeof(*state));

        if (state == NULL) {
                ARC_DEBUG(ERR, "Failed to allocate state\n");
                return -1;
        }
        
        uint64_t mem_registers_base = 0;
	uint64_t idx_data_pair_base = 0;
	(void)idx_data_pair_base;

        ARC_PCIHeaderMeta *meta = arg;
	switch (meta->header->common.header_type) {
        case ARC_PCI_HEADER_DEVICE: {
                ARC_DEBUG(INFO, "PCI header type: device\n");
                
                ARC_PCIHdrDevice header = meta->header->s.device;
                                
                if (ARC_BAR_IS_IOSPACE(header.bar2)) {
                        idx_data_pair_base = header.bar2 & ~0b111;
                        ARC_DEBUG(INFO, "BAR is in IO-space=0x%x\n", idx_data_pair_base);
                } else {
                        mem_registers_base = header.bar0 & ~0x3FFF;
                        mem_registers_base |= (uint64_t)header.bar1 << 32;

                        size_t size =  0x2000;
                        uint32_t attrs = 1 << ARC_PAGER_4K | 1 << ARC_PAGER_NX | 1 << ARC_PAGER_RW | ARC_PAGER_PAT_UC;
                        if (pager_map(NULL, mem_registers_base, mem_registers_base, size, attrs) != 0) {
                                ARC_DEBUG(ERR, "Failed to map register space\n");
                                free(state);
                                return -2;
                        }

                        state->props = (void *)mem_registers_base;
                        ARC_DEBUG(INFO, "BAR is MMapped=%p\n", state->props);
                }
                
                break;
		}
        default:
                ARC_DEBUG(ERR, "Unhandled header type %d\n", meta->header->common.header_type);
                free(state);
                return -3;
	}
	
	if (reset_controller(state) != 0) {
                free(state);
                return -4;
        }

        res->driver_state = state;
        
        ARC_Resource *nvme = init_resource(ARC_DRIGRP_DEV, ARC_DRIDEF_DEV_NVME, res);

        if (nvme == NULL) {
                uninit_nvme_pci(res);
                return -2;
        }
        
	return 0;
}

int uninit_nvme_pci(ARC_Resource *resource) {
        (void)resource;
        ARC_DEBUG(WARN, "Uninitializing NVME-PCI\n");
        
	return 0;
}

// TODO: These should probably just write out to the state->props
static size_t read_nvme_pci(void *buffer, size_t size, size_t count, ARC_File *file, ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
	}

        ARC_DEBUG(WARN, "Need to implement reading for properties\n");
        
	return 0;
}

static size_t write_nvme_pci(void *buffer, size_t size, size_t count, ARC_File *file, ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
	}

        ARC_DEBUG(WARN, "Need to implement writing for properties\n");
        
	return 0;
}

static int stat_nvme_pci(ARC_Resource *res, char *filename, struct stat *stat) {
	(void)res;
	(void)filename;
	(void)stat;

	return 0;
}

static ARC_ControlPacketResponse control_nvme_pci(ARC_Resource *res, ARC_ControlPacketInstruction *inst) {
        ARC_ControlPacketResponse resp = { 0 };
        
        if (res == NULL || inst == NULL) {
                return resp;
        }

        driver_state_t *state = res->driver_state;
        
        switch (inst->command) {
        case NVME_TRANSPORT_CTRL_IDEN: {
                nvme_transport_iden_t *iden = inst->data;

                if (iden == NULL) {
                        goto err;
                }
                
                iden->submit = nvme_pci_submit_command;
                iden->poll = nvme_pci_poll_completion;
                iden->type = NVME_TRANSPORT_TYPE_PCI;

                resp.type = inst->command;
                resp.data = iden;
                resp.size = sizeof(*iden);
                
                return resp;
        }

        case NVME_TRANSPORT_CTRL_TO_PROPS: {
                state->exposed = NVME_TRANSPORT_CTRL_TO_PROPS;
                return resp;
        }
        }

 err:
        ARC_DEBUG(ERR, "Unhandled command %d\n", inst->command);
        return (ARC_ControlPacketResponse) { 0 };
}

static uint64_t pci_codes[] = {
        0x1b360010,
        ARC_DRIDEF_CODES_TERMINATOR
};

ARC_REGISTER_DRIVER(ARC_DRIGRP_DEV_PCI, nvme) = {
        .init = init_nvme_pci,
	.uninit = uninit_nvme_pci,
	.read = read_nvme_pci,
	.write = write_nvme_pci,
	.seek = dridefs_int_func_empty,
	.rename = dridefs_int_func_empty,
	.stat = stat_nvme_pci,
        .control = control_nvme_pci,
	.codes = pci_codes
};
