/**
 * @file pci.c
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan - Operating System Kernel
 * Copyright (C) 2023-2024 awewsomegamer
 *
 * This file is part of Arctan.
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
#include <drivers/dev/nvme/pci.h>
#include <drivers/dev/nvme/nvme.h>
#include <arch/pager.h>
#include <mm/pmm.h>
#include <lib/util.h>

int nvme_pci_submit_command(struct controller_state *state, int queue, struct qs_entry *cmd) {
	if (state == NULL || cmd == NULL) {
		return -1;
	}

	struct qpair_list_entry *qpair = state->admin_entry;
	if (queue != ADMIN_QUEUE) {
		qpair = state->list;
		while (qpair != NULL && qpair->id != queue) {
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

	if (ret != NULL) {
		memcpy(ret, &qpair->completion_queue[i], sizeof(*ret));
	}

	size_t idx = ringbuffer_allocate(qpair->completion_queue, 1);

	if (idx == qpair->completion_queue->objs - 1) {
		qpair->phase = !qpair->phase;
	}

	uint32_t *doorbell = (uint32_t *)CQnHDBL(state->properties, qpair_id + 1);
	*doorbell = ((uint32_t)idx) + 1;

	return 0;
}

/*
**
static int configure_controller(struct controller_state *state) {
	if (state == NULL || !(state->flags & 1)) {
		ARC_DEBUG(ERR, "Cannot configure controller, either not initialized or state does not exist\n");
		return -1;
	}

	uint8_t *controller_conf = identify_queue(state, ADMIN_QUEUE, 0x1, 0, 0);

	if (controller_conf == NULL) {
		ARC_DEBUG(ERR, "Failed to configure, returned controller configuration is NULL\n");
		return -1;
	}

	// 77    Maximum Data Transfer Size in 2 << cap.mpsmin
	//       CTRATT mem bit cleared, includes the size of the interleaved metadata
	//       mem bit set then this is size of user data only
	state->max_transfer_size = controller_conf[77];

	// 79:78 CNTLID
	state->controller_id = *(uint16_t *)(&controller_conf[78]);

	// 83:80 VERS
	state->controller_version = *(uint32_t *)(&controller_conf[80]);

	// 99:96 CTRATT bit 16 is MEM for 77
	//              bit 11 is multi-domain subsystem
	//              bit 10 UUID list 1: supported
	state->ctratt = *(uint32_t *)(&controller_conf[96]);

	// 111   Controller type (0: resv, 1: IO, 2: discovery, 3 ADMIN, all else resv)
	state->controller_type = controller_conf[111];

	// TODO: CRDTs

	// Configure the command set
	if (MASKED_READ(state->properties->cap, 43, 1)) {
		uint64_t *command_sets = identify_queue(state, ADMIN_QUEUE, 0x1C, 0, 0);

		for (int i = 0; i < PAGE_SIZE; i++) {
			printf("%02X ", *(command_sets + i));
		}
		printf("\n");

		if (command_sets == NULL) {
			// TODO: Error handle
			return -1;
		}

		// Select command set
		// TODO: Maybe specify a desired vector?
		int index = 0;
		struct qs_entry sel_cmd = {
	                .cdw0.opcode = 0x9,
			.cdw10 = 0x19,
			.cdw11 = index
                };

		submit_queue_command(state, &sel_cmd, ADMIN_QUEUE);
		poll_completion_queue(state, &sel_cmd);

		uint64_t command_set_vec = command_sets[index];

		while (command_set_vec != 0) {
			int supported_command_set = __builtin_ffs(command_set_vec) - 1;

			uint32_t *cns7 = identify_queue(state, ADMIN_QUEUE, 7, (supported_command_set << 24), 0);
			printf("NSIDs\n");
			for (int i = 0; i < PAGE_SIZE; i++) {
				printf("%02X ", *(cns7 + i));
			}
			printf("\n");
			int nsid_counter = 0;
			while (cns7[nsid_counter]) {
				// If statement for I/O command sets based on NVM Command Set
				printf(">NSID: %d\n", cns7[nsid_counter]);

				if (supported_command_set == 0 || supported_command_set == 2) {
 					identify_queue(state, ADMIN_QUEUE, 0, 0, nsid_counter + 1);
				}

				nsid_counter++;
			}

			pmm_free(cns7);


			command_set_vec &= ~(1 << (supported_command_set));
		}
	} else {
		// TODO: Do another thing
	}

	MASKED_WRITE(state->properties->cc, 6, 16, 0b1111);
	MASKED_WRITE(state->properties->cc, 4, 20, 0b1111);

	// Request 64 IO completion and
	struct qs_entry cmd = {
	        .cdw0.opcode = 0x9,
	        .cdw10 = 0x7,
		.cdw11 = 64 | (64 << 16)
        };
	submit_queue_command(state, &cmd, ADMIN_QUEUE);
	struct qc_entry *res = poll_completion_queue(state, &cmd);

	state->max_ioqpair_count = min(res->dw0 & 0xFFFF, (res->dw0 >> 16) & 0xFFFF) + 1;

	printf("%d\n", state->max_ioqpair_count);

	free(res);

	return 0;
}
*/

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
	void *queues = pmm_contig_alloc(2);

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
	} else if ((cap_css >> 6) & 1) {
		MASKED_WRITE(properties->cc, 0b110, 4, 0b111);
	} else {
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

	printf("%"PRIx64"\n", mem_registers_base);

	struct controller_properties *properties = (struct controller_properties *)ARC_PHYS_TO_HHDM(mem_registers_base);

	if (properties == NULL) {
		ARC_DEBUG(ERR, "NVMe properties are NULL\n");
		return -1;
	}

	if (pager_map(ARC_PHYS_TO_HHDM(mem_registers_base), mem_registers_base, 0x2000, 1 << ARC_PAGER_4K | 1 << ARC_PAGER_NX | 1 << ARC_PAGER_RW | ARC_PAGER_PAT_UC) != 0) {
		ARC_DEBUG(ERR, "Failed to map register space\n");
		return -1;
	}

	init_static_mutex(&state->qpair_lock);

	state->properties = properties;

	reset_controller(state);

	// Identify
	uint8_t *data = pmm_alloc();
	struct qs_entry cmd = {
	        .cdw0.opcode = 0x6,
		.prp.entry1 = ARC_HHDM_TO_PHYS(data),
		.cdw10 = (state->controller_id << 16) | 0x1,
		.cdw11 = 0
        };

	nvme_submit_command(state, ADMIN_QUEUE, &cmd);
	nvme_poll_completion(state, &cmd, NULL);

	printf("Data:\n");
	for (int i = 0; i < PAGE_SIZE; i++) {
		printf("%02X ", *(data + i));
	}
	printf("\n");

	return 0;
}
