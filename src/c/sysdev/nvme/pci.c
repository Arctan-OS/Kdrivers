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

typedef struct ctrl_props {
	uint64_t cap;
	uint32_t vs;
	uint32_t intms;
	uint32_t intmc;
	uint32_t cc;
	uint32_t resv0;
	uint32_t csts;
	uint32_t nssr;
	uint32_t aqa;
	uint64_t asq;
	uint64_t acq;
	uint32_t cmbloc;
	uint32_t cmbsz;
	uint32_t bpinfo;
	uint32_t bprsel;
	uint64_t bpmbl;
	uint64_t cmbmsc;
	uint32_t cmbsts;
	uint32_t cmbebs;
	uint32_t cmbswtp;
	uint32_t nssd;
	uint32_t crto;
	uint8_t resv1[0xD94];
	uint32_t pmrcap;
	uint32_t pmrctl;
	uint32_t pmrsts;
	uint32_t pmrebs;
	uint32_t pmrswtp;
	uint32_t pmrmscl;
	uint32_t pmrmscu;
	uint8_t resv2[0x1E4];
	uint8_t data[];
} __attribute__((packed)) ctrl_props_t;
STATIC_ASSERT(sizeof(ctrl_props_t) == 0x1000, "Controller properties size mismatch");

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

static int nvme_pci_poll_completion(ARC_Resource *transport, qs_wrap_t *wrap, qc_entry_t **ret) {
	if (transport == NULL || wrap->cmd == NULL) {
		return -1;
	}

        nvme_qpair_t *qpair = wrap->qpair;
        qs_entry_t *cmd = wrap->cmd;
	qc_entry_t *qc = (struct qc_entry *)qpair->cmpq->base;
        
        bool I = arch_interrupts_enabled();
        ARC_ENABLE_INTERRUPT; // Causes a double fault, if commented, waits forever
        
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

        printf("made it here\n");
        
	int status = qc[i].status;

	if (ret != NULL) {
		memcpy(ret, &qc[i], sizeof(**ret));
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

static int create_admin_queues(driver_state_t *state, size_t qsize) {
	void *queues = pmm_alloc(qsize * 2);

	if (queues == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate adminstrator queues\n");
		return -1;
	}

        ARC_Ringbuffer *sub = init_ringbuffer(queues, NVME_ADMIN_QUEUE_SUB_LEN, sizeof(qs_entry_t));

        if (sub == NULL) {
                return -2;
        }
        
        ARC_Ringbuffer *comp = init_ringbuffer(queues, NVME_ADMIN_QUEUE_SUB_LEN, sizeof(qs_entry_t));

        if (comp == NULL) {
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

        if (create_admin_queues(state, PAGE_SIZE) != 0) {
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

        res->driver_state = state;
        
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
                                return -1;
                        }

                        state->props = (void *)mem_registers_base;
                        ARC_DEBUG(INFO, "BAR is MMapped=%p\n", state->props);
                }
                
                break;
		}
        default:
                ARC_DEBUG(ERR, "Unhandled header type %d\n", meta->header->common.header_type);
                break;
	}
	
	reset_controller(state);

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
        
	return 1;
}

static size_t write_nvme_pci(void *buffer, size_t size, size_t count, ARC_File *file, ARC_Resource *res) {
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

static ARC_ControlPacketResponse *control_nvme_pci(ARC_Resource *res, ARC_ControlPacketInstruction *inst) {
        if (res == NULL || inst == NULL) {
                return NULL;
        }

        ARC_ControlPacketResponse *resp = alloc(sizeof(*resp));

        if (resp == NULL) {
                return NULL;
        }

        memset(resp, 0, sizeof(*resp));
        
        switch (inst->command) {
        case NVME_TRANSPORT_CTRL_IDEN: {
                nvme_transport_iden_t *iden = inst->data;

                if (iden == NULL) {
                        goto err;
                }
                
                iden->submit = nvme_pci_submit_command;
                iden->poll = nvme_pci_poll_completion;
                iden->type = NVME_TRANSPORT_TYPE_PCI;

                resp->type = inst->command;
                resp->data = iden;
                resp->size = sizeof(*iden);
                
                return resp;
        }
        }

 err:
        free(resp);
        ARC_DEBUG(ERR, "Unhandled command %d\n", inst->command);
        return NULL;
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
