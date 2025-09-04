/**
 * @file pci.c
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan-OS/Kernel - Operating System Kernel
 * Copyright (C) 2023-2025 awewsomegamer
 *
 * This file is part of Arctan-OS/Kernel.
 *
 * Arctan is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * @DESCRIPTION
*/
#include "arch/pager.h"
#include "drivers/dri_defs.h"
#include "drivers/sysdev/nvme/nvme.h"
#include "drivers/sysdev/nvme/pci.h"
#include "global.h"
#include "lib/mutex.h"
#include "lib/util.h"
#include "mm/pmm.h"

int nvme_pci_submit_command(struct controller_state *state, int queue, struct qs_entry *cmd) {
	if (state == NULL || cmd == NULL) {
		return -1;
	}

	struct qpair_list_entry *qpair = state->admin_entry;
	if (queue != ADMIN_QUEUE) {
		qpair = state->list;
		while (qpair != NULL && qpair->id != queue) {
			printf("%d\n", qpair->id);
			qpair = qpair->next;
		}
	}

	if (qpair == NULL) {
		return -2;
	}

	size_t ptr = ringbuffer_allocate(qpair->submission_queue, 1);

	if (queue == ADMIN_QUEUE) {
		cmd->cdw0.cid = (1 << 15) | (ptr & 0xFF);
	} else {
		cmd->cdw0.cid = (queue & 0x3F) | ((ptr & 0xFF) << 6);
	}

	ringbuffer_write(qpair->submission_queue, ptr, cmd);

	uint32_t *doorbell = (uint32_t *)SQnTDBL(state->properties, queue + 1);
	*doorbell = ((uint32_t)ptr) + 1;

	return 0;
}

int nvme_pci_poll_completion(struct controller_state *state, struct qs_entry *cmd, struct qc_entry *ret) {
	if (state == NULL || cmd == NULL) {
		return -1;
	}

	struct qpair_list_entry *qpair = state->admin_entry;
	int qpair_id = (cmd->cdw0.cid >> 15) & 1 ? ADMIN_QUEUE : cmd->cdw0.cid & 0x3F;
	int cmd_idx = (qpair_id == ADMIN_QUEUE) ? (cmd->cdw0.cid) : (cmd->cdw0.cid >> 6) & 0xFF;

	if (qpair_id != ADMIN_QUEUE) {
		qpair = state->list;
		while (qpair != NULL && qpair->id != qpair_id) {
			qpair = qpair->next;
		}
	}

	if (qpair == NULL) {
		return -2;
	}

	struct qc_entry *qc = (struct qc_entry *)qpair->completion_queue->base;

	size_t i = qpair->completion_queue->idx;
	while (1) {
		i = qpair->completion_queue->idx;

		if (qc[i].phase == qpair->phase && qc[i].cid == cmd->cdw0.cid) {
			break;
		}
	}

	int status = qc[i].status;

	if (ret != NULL) {
		memcpy(ret, &qc[i], sizeof(*ret));
	}

	size_t idx = ringbuffer_allocate(qpair->completion_queue, 1);

	if (idx == qpair->completion_queue->objs - 1) {
		qpair->phase = !qpair->phase;
	}

	uint32_t *doorbell = (uint32_t *)CQnHDBL(state->properties, qpair_id + 1);
	*doorbell = ((uint32_t)idx) + 1;

	ringbuffer_free(qpair->submission_queue, cmd_idx);

	return status;
}

static int reset_controller(struct controller_state *state) {
	if (state == NULL || state->properties == NULL) {
		ARC_DEBUG(ERR, "Failed to reset controller, state or properties NULL\n");
		return -1;
	}

	struct controller_properties *properties = state->properties;

	// Disable
	MASKED_WRITE(properties->cc, 0, 0, 1);

	while (MASKED_READ(properties->csts, 0, 1));

	// Create adminstrator queue
	void *queues = pmm_alloc(PAGE_SIZE * 2);

	if (queues == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate adminstrator queues\n");
		return -1;
	}

	nvme_delete_all_qpairs(state);

	if (MASKED_READ(state->flags, 0, 1)) {
		// TODO: Uninitialize stuff
	}

	memset(queues, 0, PAGE_SIZE * 2);
	properties->asq = ARC_HHDM_TO_PHYS(queues);
	properties->acq = ARC_HHDM_TO_PHYS(queues) + PAGE_SIZE;

	MASKED_WRITE(properties->aqa, ADMIN_QUEUE_SUB_LEN - 1, 0, 0xFFF);
	MASKED_WRITE(properties->aqa, ADMIN_QUEUE_COMP_LEN - 1, 16, 0xFFF);

	state->admin_entry = nvme_create_qpair(state, ARC_PHYS_TO_HHDM(properties->asq), ADMIN_QUEUE_SUB_LEN, ARC_PHYS_TO_HHDM(properties->acq), ADMIN_QUEUE_SUB_LEN);

	if (state->admin_entry == NULL) {
		ARC_DEBUG(ERR, "Failed to create adminstrator queue pair\n");
		return -2;
	}

	// Clear bitmap, as admin queue is not part of IO queues
	state->id_bmp = -1;
	state->admin_entry->id = -1;

	// Set CC.CSS
	uint8_t cap_css = MASKED_READ(properties->cap, 37, 0xFF);

	if ((cap_css >> 7) & 1) {
		MASKED_WRITE(properties->cc, 0b111, 4, 0b111);
	}

	if ((cap_css >> 6) & 1) {
		MASKED_WRITE(properties->cc, 0b110, 4, 0b111);
	}

	if (((cap_css >> 6) & 1) == 0 && ((cap_css >> 0) & 1) == 1) {
		MASKED_WRITE(properties->cc, 0b000, 4, 0b111);
	}

	// Set MPS and AMS
	MASKED_WRITE(properties->cc, 0, 7, 0b1111);
	MASKED_WRITE(properties->cc, 0, 11, 0b111);

	// Enable
	MASKED_WRITE(properties->cc, 1, 0, 1);

	// TODO: Add something to do here like __asm__("pause")
	while (!MASKED_READ(properties->csts, 0, 1));

	state->flags |= 1;

	return 0;
}

int init_nvme_pci(struct controller_state *state, struct ARC_PCIHeader *header) {
	if (state == NULL || header == NULL) {
		return -1;
	}

	uint64_t mem_registers_base = 0;
	uint64_t idx_data_pair_base = 0;
	(void)idx_data_pair_base;

	switch (header->common.header_type) {
		case 0: {
			struct ARC_PCIHdr0 header0 = header->headers.header0;

			mem_registers_base = header0.bar0 & ~0x3FFF;
			mem_registers_base |= (uint64_t)header0.bar1 << 32;

			if (ARC_BAR_IS_IOSPACE(header0.bar2)) {
				idx_data_pair_base = header0.bar2 & ~0b111;
			}

			break;
		}
		case 1: {
			break;
		}
	}

	struct controller_properties *properties = (struct controller_properties *)ARC_PHYS_TO_HHDM(mem_registers_base);

	if (properties == NULL) {
		ARC_DEBUG(ERR, "NVMe properties are NULL\n");
		return -1;
	}

	if (pager_map(NULL, ARC_PHYS_TO_HHDM(mem_registers_base), mem_registers_base, 0x2000, 1 << ARC_PAGER_4K | 1 << ARC_PAGER_NX | 1 << ARC_PAGER_RW | ARC_PAGER_PAT_UC) != 0) {
		ARC_DEBUG(ERR, "Failed to map register space\n");
		return -1;
	}

	init_static_mutex(&state->qpair_lock);

	state->properties = properties;

	// XXX: There is a triple fault occurring in this function
	// reset_controller(state);

	return 0;
}
