#include "arch/smp.h"
#include "arch/x86-64/config.h"
#include "config.h"
#include "drivers/resource.h"
#include "drivers/dri_defs.h"
#include "drivers/sysdev/nvme/nvme.h"
#include "mm/allocator.h"
#include "mm/pmm.h"
#include "util.h"
#include "fs/vfs.h"
#include <stdint.h>

typedef struct driver_state {
        nvme_driver_state_t *nvm_state;

        size_t block_size; // The size of block which will be read by the
                           // RW functions (default=PAGE_SIZE)
        
        size_t lba_size;
        bool meta_follows_lba;
        size_t meta_size;

        size_t nsze;
        size_t ncap;
        
        int namespace;
        int nvm_set;
        
        int command_set;
        int qpair_base;
        int qpair_count;

        ARC_Cache *l2;
} driver_state_t;

static int nvme_register_io_qpair(nvme_driver_state_t *state, nvme_qpair_t *qpair, uint8_t nvm_set, int irq) {
        if (state == NULL || qpair == NULL || qpair->cmpq == NULL || qpair->subq == NULL) {
                ARC_DEBUG(ERR, "Improper parameters (qpair=%p, qpair->cmpq=%p, qpair->subq=%p)\n", qpair, qpair == NULL ? NULL : qpair->cmpq, qpair == NULL ? NULL : qpair->subq);
		return -1;
	}
        
	qs_entry_t cmd = {
		.cdw0.opcode = 0x5,
                .prp.entry1 = ARC_HHDM_TO_PHYS(qpair->cmpq->base),
		.cdw10 = qpair->id | ((qpair->cmpq->objs - 1) << 16),
		.cdw11 = 1 | ((irq > 31) << 1) | ((irq & 0xFFFF) << 16),
		.cdw12 = nvm_set
        };
        
        qs_wrap_t wrap = state->submit(state->transport, NULL, &cmd);

        int status = state->poll(state->transport, &wrap, NULL);
        if (status != 0) {
                return status | (1 << 16);
        }
        
	cmd.cdw0.opcode = 0x1;
	cmd.prp.entry1 = ARC_HHDM_TO_PHYS(qpair->subq->base);
	cmd.cdw10 = qpair->id | ((qpair->subq->objs - 1) << 16);
	cmd.cdw11 = 1 | (qpair->id << 16);
        
        wrap = state->submit(state->transport, NULL, &cmd);
	status = state->poll(state->transport, &wrap, NULL);
        
        if (status != 0) {
                return status | (2 << 16);
        }
        
        return 0;
}


static int namespace_get_info(driver_state_t *state) {
        nvme_driver_state_t *nvm_state = state->nvm_state;
        
        uint8_t *data = pmm_fast_page_alloc();

        if (data == NULL) {
                ARC_DEBUG(ERR, "Failed to allocate data buffer\n");
                return -1;
        }

        qs_entry_t cmd = {
                .cdw0.opcode = 0x6,
                .prp.entry1 = ARC_HHDM_TO_PHYS(data),
                .cdw10 = 0,
                .cdw11 = (state->command_set & 0xFF) << 24,
                .nsid = state->namespace
        };
        
        qs_wrap_t wrap = nvm_state->submit(nvm_state->transport, NULL, &cmd);
        int status = nvm_state->poll(nvm_state->transport, &wrap, NULL);

        if (status != 0) {
                return status | (1 << 16);
        }
        
        uint8_t format_idx = MASKED_READ(data[26], 0, 0xF) | (MASKED_READ(data[26], 5, 0b11) << 4);
	state->meta_follows_lba = MASKED_READ(data[26], 4, 1);

	uint32_t lbaf = *(uint32_t *)&data[128 + format_idx];
	uint8_t lba_exp = MASKED_READ(lbaf, 16, 0xFF);

	state->lba_size = 1 << lba_exp;
	state->meta_size = MASKED_READ(lbaf, 0, 0xFFFF);
        
	// Set some base information
	state->nvm_set = data[100];
	state->nsze = *(uint64_t *)data;
	state->ncap = *(uint64_t *)&data[8];

        cmd.cdw10 = 0x5;
        wrap = nvm_state->submit(nvm_state->transport, NULL, &cmd);
        status = nvm_state->poll(nvm_state->transport, &wrap, NULL);

        if (status != 0) {
                return status | (2 << 16);
        }
        
        cmd.cdw10 = 0x6;
        wrap = nvm_state->submit(nvm_state->transport, NULL, &cmd);
        status = nvm_state->poll(nvm_state->transport, &wrap, NULL);

        if (status != 0) {
                return status | (3 << 16);
        }
        
        cmd.cdw10 = 0x8;
        cmd.cdw11 = 0x0;
        wrap = nvm_state->submit(nvm_state->transport, NULL, &cmd);
        status = nvm_state->poll(nvm_state->transport, &wrap, NULL);

        if (status != 0) {
                return status | (4 << 16);
        }
        
        return 0;
}

static int init_nvme_namespace(ARC_Resource *res, void *_arg) {
        nvme_namespace_args_t *arg = _arg;

        driver_state_t *state = alloc(sizeof(*state));

        if (state == NULL) {
                ARC_DEBUG(ERR, "Failed to state for namespace %d\n", arg->namespace);
                return -1;
        }

        state->block_size = PAGE_SIZE;
        
        state->nvm_state = arg->state;        
        state->namespace = arg->namespace;
        state->command_set = arg->command_set;

        state->qpair_base = arg->state->qpairs.init;
        int q_count = min(Arc_ProcessorCounter, arg->state->qpairs.granted);
        state->qpair_count = q_count;
        arg->state->qpairs.init += q_count;

        int r = namespace_get_info(state);
        if (r != 0) {
                ARC_DEBUG(ERR, "Failed to get information about namespace (r=%04X)\n", r);
                return -2;
        }

        for (int i = state->qpair_base; i < state->qpair_base + q_count; i++) {
                nvme_qpair_t *qpair = &arg->state->qpairs.qs[i];
                int irq = 0;
                ARC_DEBUG(INFO, "Registering io qpair %d {%p} (%d, %d)\n", i, qpair, arg->command_set, irq);
             
                int r = nvme_register_io_qpair(arg->state, qpair, state->nvm_set, irq);
                if (r != 0) {
                        ARC_DEBUG(ERR, "Failed to register io qpair (r=%04X)\n", r);
                        ARC_DEBUG(WARN, "Figure out what to do\n");
                }
        }
        
        res->driver_state = state;

        char path[64] = { 0 };
        sprintf(path, "/dev/nvme%dn%d", state->nvm_state->ctrl_iden.id, state->namespace);
        vfs_create(path, S_IFDIR | ARC_STD_PERM, res);
        
        return 0;
}

int uninit_nvme_namespace(ARC_Resource *resource) {
        (void)resource;
        return 0;
}

static int namespace_rw_page(bool write, driver_state_t *state, uint64_t bstart, void **data) {
        if (*data == NULL) {
                *data = pmm_alloc(state->block_size);
                memset(*data, 0, PAGE_SIZE);        
        }
        
        // TODO: Attempt to read/write cache
        
        void *meta = pmm_fast_page_alloc();

        // TODO: What if lba->size > PAGE_SIZE?
        //       What if we should use a larger data size?
        qs_entry_t cmd = {
                .cdw0.opcode = write ? 0x1 : 0x2,
                .prp.entry1 = ARC_HHDM_TO_PHYS(*data),
                .mptr = ARC_HHDM_TO_PHYS(meta),
                .cdw12 = (PAGE_SIZE / state->lba_size) - 1,
                .cdw10 = bstart & UINT32_MAX,
                .cdw11 = bstart >> 32,
                .nsid = state->namespace,
        };

        nvme_qpair_t *qpair = &state->nvm_state->qpairs.qs[smp_get_processor_id() + state->qpair_base];
        
        nvme_driver_state_t *nvm_state = state->nvm_state;
        qs_wrap_t wrap = nvm_state->submit(nvm_state->transport, qpair, &cmd);
        int status = nvm_state->poll(nvm_state->transport, &wrap, NULL);
        
        pmm_fast_page_free(meta);
        
        return status;
}

static size_t read_nvme_namespace(void *buffer, size_t size, size_t count, ARC_File *file, ARC_Resource *res) {
        driver_state_t *state = res->driver_state;
        
        size_t read = 0;        
        void *page = NULL;
        
        while (read < size * count) {
                size_t read_offset = ALIGN_DOWN(file->offset + read, state->lba_size);
                size_t page_offset = read + file->offset - read_offset;
                size_t to_read = min(state->block_size, (size * count) - read);
                
                if (page_offset > 0) {
                        to_read = state->block_size - (file->offset - read_offset);
                        to_read = min(to_read, size * count);
                }
                
                int lba =  read_offset / state->lba_size;
                
                if (namespace_rw_page(false, state, lba, &page) != 0) {
                        ARC_DEBUG(ERR, "Failed to read lba=%d for %lu bytes\n", lba, to_read);
                        break;
                }
                
                memcpy(buffer + read, page + page_offset, to_read);

                read += to_read;
        }

        pmm_free(page);
        
        return read;
}

static size_t write_nvme_namespace(void *buffer, size_t size, size_t count, ARC_File *file, ARC_Resource *res) {
        driver_state_t *state = res->driver_state;
        
        size_t written = 0;
        void *page = NULL;

        while (written < size * count) {
                size_t write_offset = ALIGN_DOWN(file->offset + written, state->lba_size);
                size_t page_offset = written + file->offset - write_offset;
                size_t to_write = min(state->block_size, (size * count) - written);
                
                if (page_offset > 0) {
                        to_write = state->block_size - (file->offset - write_offset);
                        to_write = min(to_write, size * count);
                }
                
                int lba =  write_offset / state->lba_size;

                if (to_write < PAGE_SIZE && namespace_rw_page(false, state, lba, &page) != 0) {
                        ARC_DEBUG(ERR, "Failed to read lba=%d for %lu bytes\n", lba, to_write);
                        break;
                }
                
                memcpy(page + page_offset, buffer + written, to_write);
                
                if (namespace_rw_page(true, state, lba, &page) != 0) {
                        ARC_DEBUG(ERR, "Failed to write lba=%d for %lu bytes\n", lba, to_write);
                        break;
                }
                
                written += to_write;
        }

        pmm_free(page);
        
        return written;
}

static int stat_nvme_namespace(ARC_Resource *res, char *filename, struct stat *stat) {
	(void)res;
	(void)filename;
	(void)stat;

        if (res == NULL || stat == NULL) {
                return -1;
        }

        driver_state_t *state = res->driver_state;

        stat->st_blksize = state->lba_size;
        stat->st_size = state->nsze;
        
        return 0;
}

ARC_REGISTER_DRIVER(ARC_DRIGRP_DEV, nvme_namespace) = {
        .init = init_nvme_namespace,
	.uninit = uninit_nvme_namespace,
	.read = read_nvme_namespace,
	.write = write_nvme_namespace,
	.seek = dridefs_int_func_empty,
	.rename = dridefs_int_func_empty,
	.stat = stat_nvme_namespace,
	.codes = NULL
};
