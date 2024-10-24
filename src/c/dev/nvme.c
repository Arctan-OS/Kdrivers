/**
 * @file nvme.c
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
#include <arctan.h>
#include <lib/resource.h>
#include <stdint.h>
#include <global.h>
#include <lib/util.h>
#include <drivers/dri_defs.h>
#include <lib/perms.h>
#include <arch/pci/pci.h>
#include <arch/pager.h>
#include <mm/allocator.h>
#include <mm/pmm.h>
#include <arch/x86-64/apic/apic.h>
#include <drivers/dev/nvme.h>

struct qpair_list_entry {
	struct qs_entry *sub;
	size_t sub_len;
	size_t sub_doorbell;
	struct qc_entry *comp;
	size_t comp_len;
	size_t comp_doorbell;
	struct qpair_list_entry *next;
	int id;
};

struct driver_state {
	struct ARC_Resource *res;
	struct controller_properties *properties;
	uint32_t flags; // Bit | Description
			// 0   | 1: Kernel Initialized
	struct qpair_list_entry *list;
	struct qpair_list_entry *admin_entry;
	uint64_t id_bmp;
	size_t max_queue_count;
	size_t max_transfer_size;
	uint32_t ctratt;
	uint32_t controller_version;
	uint16_t controller_id;
	uint8_t controller_type;

};

int empty_nvme() {
	return 0;
}

static struct qpair_list_entry *create_qpair(struct driver_state *state, uintptr_t sub, size_t sub_len, uintptr_t comp, size_t comp_len) {
	if (state == NULL || state->id_bmp == 0 || comp_len == 0 || sub_len == 0) {
		return NULL;
	}

	struct qpair_list_entry *entry = (struct qpair_list_entry *)alloc(sizeof(*entry));

	entry->id = __builtin_ffs(state->id_bmp) - 1;
	state->id_bmp &= ~(1 << entry->id);

	entry->sub = (struct qs_entry *)sub;
	entry->sub_len = sub_len;
	entry->comp = (struct qc_entry *)comp;
	entry->comp_len = comp_len;
	entry->next = state->list;

	state->list = entry;

	return entry;
}

static int delete_qpair(struct qpair_list_entry *qpair) {
	(void)qpair;
	ARC_DEBUG(WARN, "Implement\n");

	return 0;
}

static int delete_all_qpairs(struct driver_state *state) {
	state->id_bmp = -1;
	ARC_DEBUG(WARN, "Implement\n");

	return 0;
}

static int submit_queue_command(struct driver_state *state, struct qs_entry *command, int queue) {
	if (state == NULL || command == NULL) {
		ARC_DEBUG(ERR, "State or command is NULL\n");
		return -1;
	}

	struct qpair_list_entry *entry = queue == -1 ? state->list : state->admin_entry;
	while (queue != -1 && entry != NULL && entry->id != queue) {
		entry = entry->next;
	}

	if (entry == NULL) {
		ARC_DEBUG(ERR, "Found entry is NULL\n");
		return -1;
	}

	size_t ent = entry->sub_doorbell++; // TODO: Atomize
	size_t wrap = entry->sub_doorbell % entry->sub_len; // TODO: Atomize

	// NOTE: One is added to queue to convert from an internal qpair ID to an index.
	uint32_t *doorbell = (uint32_t *)SQnTDBL(state->properties, queue + 1);
	memcpy(&entry->sub[ent], command, sizeof(*command));

	entry->sub_doorbell = wrap;
	*doorbell = wrap;

	return 0;
}

static struct qc_entry *find_completion_entry(struct driver_state *state, struct qs_entry *command) {
	(void)state;
	(void)command;
	ARC_DEBUG(WARN, "Implement\n");

	return NULL;
}

static void *identify_queue(struct driver_state *state, uint8_t cns, int queue) {
	void *buffer = pmm_alloc();

	if (buffer == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate return buffer\n");
		return NULL;
	}

	struct qs_entry cmd = {
	        .cdw0.opcode = 0x06,
		.cdw10 = cns,
		.prp.entry1 = ARC_HHDM_TO_PHYS(buffer),
        };

	if (submit_queue_command(state, &cmd, queue) != 0) {
		ARC_DEBUG(ERR, "Command submission failed\n");
		pmm_free(buffer);
		return NULL;
	}

	// TODO: Wait for completion

	return buffer;
}

static int configure_controller(struct driver_state *state) {
	if (state == NULL || !(state->flags & 1)) {
		ARC_DEBUG(ERR, "Cannot configure controller, either not initialized or state does not exist\n");
		return -1;
	}

	uint8_t *controller_conf = identify_queue(state, 0x1, ADMIN_QUEUE);

	for (int i = 0; i < PAGE_SIZE; i++) {
		printf("%02X ", *(controller_conf + i));
	}
	printf("\n");

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

	// Request 64 IO completion and
	struct qs_entry cmd = {
	        .cdw0.opcode = 0x9,
	        .cdw10 = 0x7,
		.cdw11 = 64 | (64 << 16)
        };
	submit_queue_command(state, &cmd, ADMIN_QUEUE);

	// Check to see how many we got
	cmd.cdw0.opcode = 0xA;
	cmd.cdw10 = 0x7;

	submit_queue_command(state, &cmd, ADMIN_QUEUE);

	state->max_queue_count = min(0, 0);

	return 0;
}

static int reset_controller(struct driver_state *state) {
	if (state == NULL || state->properties == NULL) {
		ARC_DEBUG(ERR, "Failed to reset controller, state or properties NULL\n");
		return -1;
	}

	struct controller_properties *properties = state->properties;

	properties->cc.en = 0;

	while (properties->csts.rdy);

	// Create adminstrator queue
	void *queues = pmm_contig_alloc(2);

	if (queues == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate adminstrator queues\n");
		return -1;
	}

	delete_all_qpairs(state);

	if (state->flags & 1) {
		// TODO: Uninitialize stuff
	}

	memset(queues, 0, PAGE_SIZE * 2);
	properties->asq = ARC_HHDM_TO_PHYS(queues);
	properties->acq = ARC_HHDM_TO_PHYS(queues) + 0x1000;
	*(uint32_t *)((uintptr_t)properties + 0x24) = 63 | (0xFF << 16);

	state->admin_entry = create_qpair(state, ARC_PHYS_TO_HHDM(properties->asq), properties->aqa.asqs + 1, ARC_PHYS_TO_HHDM(properties->acq), properties->aqa.acqs + 1);

	if (state->admin_entry == NULL) {
		ARC_DEBUG(ERR, "Failed to create adminstrator queue pair\n");
		return -2;
	}

	// Clear bitmap, as admin queue is not part of IO queues
	state->id_bmp = -1;
	state->admin_entry->id = -1;

	if ((properties->cap.css >> 7) & 1) {
		properties->cc.css = 0b111;
	} else if ((properties->cap.css >> 6) & 1) {
		properties->cc.css = 0b110;
	} else {
		properties->cc.css = 0;
	}

	properties->cc.ams = 0;
	properties->cc.mps = 0;

	properties->cc.en = 1;

	while (!properties->csts.rdy);

	state->flags |= 1;

	return configure_controller(state);
}

static int create_io_queue(struct driver_state *state, uint16_t command_set, uint16_t interrupt) {
	if (state == NULL) {
		return 0;
	}

	const int qpages = 3;
	void *queues = pmm_contig_alloc(qpages);
	uintptr_t sub = ARC_HHDM_TO_PHYS(queues);
	uintptr_t comp = sub + ((qpages - 1) * PAGE_SIZE);

	size_t sub_len = ((qpages - 1) * PAGE_SIZE) / sizeof(struct qs_entry);
	size_t comp_len = ((qpages - (qpages - 1)) * PAGE_SIZE) / sizeof(struct qs_entry);

	struct qpair_list_entry *entry = create_qpair(state, ARC_PHYS_TO_HHDM(sub), sub_len, ARC_PHYS_TO_HHDM(comp), comp_len);

	if (entry == NULL) {
		pmm_contig_free(queues, qpages);
		return -1;
	}

	// Create submission queue
	struct qs_entry cmd = {
	        .cdw0.opcode = 0x01,
	        .prp.entry1 = sub,
		.cdw10 = entry->id | (sub_len << 16),
		.cdw11 = 0b111 | (entry->id << 16),
		.cdw12 = command_set
	};

	submit_queue_command(state, &cmd, ADMIN_QUEUE);

	// Create completion queue
	cmd.cdw0.opcode = 0x05;
	cmd.prp.entry1 = comp;
	cmd.cdw10 = entry->id | (comp_len << 16);
	cmd.cdw11 = 0b1 | ((interrupt > 31) << 1) | ((interrupt & 0xFFFF) << 16);
	cmd.cdw12 = 0;

	submit_queue_command(state, &cmd, ADMIN_QUEUE);

	return entry->id;
}

static int delete_io_queue(struct driver_state *state) {
	return 0;
}

int init_nvme(struct ARC_Resource *res, void *arg) {
	if (res == NULL || arg == NULL) {
		ARC_DEBUG(ERR, "No resource / argument provided\n");
		return -1;
	}

	struct ARC_PCIHdrCommon *common = (struct ARC_PCIHdrCommon *)arg;

	uint64_t mem_registers_base = 0;
	uint64_t idx_data_pair_base = 0;
	(void)idx_data_pair_base;

	switch (common->header_type) {
		case 0: {
			struct ARC_PCIHdr0 *header = (struct ARC_PCIHdr0 *)arg;

			mem_registers_base = header->bar0 & ~0x3FFF;
			mem_registers_base |= (uint64_t)header->bar1 << 32;

			if (ARC_BAR_IS_IOSPACE(header->bar2)) {
				idx_data_pair_base = header->bar2 & ~0b111;
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

	struct driver_state *state = (struct driver_state *)alloc(sizeof(*state));

	if (state == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate state\n");
		return -1;
	}

	if (pager_map(ARC_PHYS_TO_HHDM(mem_registers_base), mem_registers_base, 0x2000, 1 << ARC_PAGER_4K | 1 << ARC_PAGER_NX | 1 << ARC_PAGER_RW | ARC_PAGER_PAT_UC) != 0) {
		ARC_DEBUG(ERR, "Failed to map register space\n");
		return -1;
	}

	state->properties = properties;
	res->driver_state = state;

	reset_controller(state);

	return 0;
}

int uninit_nvme() {
	return 0;
};

int read_nvme(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	(void)buffer;
	(void)size;
	(void)count;
	(void)file;
	(void)res;

	return 0;
}

int write_nvme(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	(void)buffer;
	(void)size;
	(void)count;
	(void)file;
	(void)res;

	return 0;
}

static uint32_t pci_codes[] = {
        0x1b360010,
        ARC_DRI_PCI_TERMINATOR
};

ARC_REGISTER_DRIVER(3, nvme_driver) = {
        .index = 0,
	.instance_counter = 0,
	.name_format = "nvme%d",
        .init = init_nvme,
	.uninit = uninit_nvme,
	.read = read_nvme,
	.write = write_nvme,
	.seek = empty_nvme,
	.rename = empty_nvme,
	.open = empty_nvme,
	.close = empty_nvme,
	.pci_codes = pci_codes
};
