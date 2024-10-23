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

#define SQnTDBL(__properties, __n) ((uintptr_t)__properties->data + ((2 * __n) * (4 << __properties->cap.dstrd)))
#define CQnHDBL(__properties, __n) ((uintptr_t)__properties->data + ((2 * __n + 1) * (4 << __properties->cap.dstrd)))

struct controller_properties {
	struct {
		uint16_t mqes : 16;
		uint8_t cqr : 1;
		uint8_t ams : 2;
		uint8_t resv0 : 5;
		uint8_t to : 8;
		uint8_t dstrd : 4;
		uint8_t nssrs : 1;
		uint8_t css : 8;
		uint8_t bps : 1;
		uint8_t cps : 2;
		uint8_t mpsmin : 4;
		uint8_t mpsmax : 4;
		uint8_t pmrs : 1;
		uint8_t cmbs : 1;
		uint8_t nsss : 1;
		uint8_t crms : 2;
		uint8_t nsses : 1;
		uint8_t resv1 : 2;
	}__attribute__((packed)) cap;

	struct {
		uint8_t tmp;
		uint8_t min;
		uint16_t maj;
	}__attribute__((packed)) vs;

	uint32_t intms;
	uint32_t intmc;

	struct  {
		uint8_t en : 1;
		uint8_t resv0 : 3;
		uint8_t css : 3;
		uint8_t mps : 4;
		uint8_t ams : 3;
		uint8_t shn : 2;
		uint8_t iosqes : 4;
		uint8_t iocqes : 4;
		uint8_t crime : 1;
		uint16_t resv1 : 7;
	}__attribute__((packed)) cc;

	uint32_t resv0;

	struct {
		uint8_t rdy : 1;
		uint8_t cfs : 1;
		uint8_t shst : 2;
		uint8_t nssro :  1;
		uint8_t pp : 1;
		uint8_t st : 1;
		uint32_t resv0 : 25;
	}__attribute__((packed)) csts;

	uint32_t nssr;

	struct {
		uint16_t asqs : 12;
		uint8_t resv0 : 4;
		uint16_t acqs : 12;
		uint8_t resv1 : 4;
	}__attribute__((packed)) aqa;

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
}__attribute__((packed));
STATIC_ASSERT(sizeof(struct controller_properties) == 0x1000, "Controller properties size mismatch");

struct qs_entry {
	struct {
		uint8_t opcode;
		uint8_t fuse : 2;
		uint8_t resv0 : 4;
		uint8_t psdt : 2;
		uint16_t cid;
	}__attribute__((packed)) cdw0;

	uint32_t nsid;
	uint32_t cdw2;
	uint32_t cdw3;
	uint64_t mptr;
	// If CDW0.PSDT is set to 01 or 10, the following
	// two are SGL1, otherwise they are PRP1 and PRP2
	// respectively
	struct {
		uint64_t entry1;
		uint64_t entry2;
	}__attribute__((packed)) prp;
	uint32_t cdw10;
	uint32_t cdw11;
	uint32_t cdw12;
	uint32_t cdw13;
	uint32_t cdw14;
	uint32_t cdw15;
}__attribute__((packed));
STATIC_ASSERT(sizeof(struct qs_entry) == 64, "Submission Queue Entry size mismatch");

struct qc_entry {
	uint32_t dw0;
	uint32_t dw1;
	uint16_t sq_head_ptr;
	uint16_t sq_ident;
	uint16_t cid;
	uint8_t phase : 1;
	uint16_t status : 15;
}__attribute__((packed));
STATIC_ASSERT(sizeof(struct qc_entry) == 16, "Completeion Queue Entry Size mismatch");

struct qpair_list_entry {
	struct qs_entry *sub;
	size_t sub_len;
	size_t sub_doorbell;
	struct qc_entry *comp;
	size_t comp_len;
	size_t comp_doorbell;
	struct qpair_list_entry *next;
	int i;
};

struct driver_state {
	struct ARC_Resource *res;
	struct controller_properties *properties;
	uint32_t flags; // Bit | Description
			// 0   | 1: Kernel Initialized
	struct qpair_list_entry *list;
	int queue_count;
};

int empty_nvme() {
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

	if ((state->flags & 1) == 0) {
		void *queues = pmm_contig_alloc(2);

		memset(queues, 0, PAGE_SIZE * 2);
		properties->asq = ARC_HHDM_TO_PHYS(queues);
		properties->acq = ARC_HHDM_TO_PHYS(queues) + 0x1000;
		*(uint32_t *)((uintptr_t)properties + 0x24) = 63 | (0xFF << 16);

		struct qpair_list_entry *entry = (struct qpair_list_entry *)alloc(sizeof(*entry));

		entry->i = 0;
		entry->sub = (struct qs_entry *)ARC_PHYS_TO_HHDM(properties->asq);
		entry->sub_len = properties->aqa.asqs + 1;
		entry->comp = (struct qc_entry *)ARC_PHYS_TO_HHDM(properties->acq);
		entry->comp_len = properties->aqa.acqs + 1;
		entry->next = NULL;

		state->list = entry;
		state->queue_count++;
	}

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

	return 0;
}

static int submit_queue_command(struct driver_state *state, struct qs_entry *command, int queue) {
	if (state == NULL || command == NULL) {
		return -1;
	}

	struct qpair_list_entry *entry = state->list;
	while (entry != NULL && entry->i != queue) {
		entry = entry->next;
	}

	if (entry == NULL) {
		return -1;
	}

	size_t ent = entry->sub_doorbell++; // TODO: Atomize
	size_t wrap = entry->sub_doorbell % entry->sub_len; // TODO: Atomize

	uint32_t *doorbell = (uint32_t *)SQnTDBL(state->properties, queue);
	memcpy(&entry->sub[ent], command, sizeof(*command));

	entry->sub_doorbell = wrap;
	*doorbell = wrap;

	return 0;
}

static void *identify_queue(struct driver_state *state, int queue) {
	void *buffer = pmm_alloc();

	if (buffer == NULL) {
		return NULL;
	}

	struct qs_entry cmd = {
	        .cdw0.opcode = 0x06,
		.cdw10 = 0x01,
		.prp.entry1 = ARC_HHDM_TO_PHYS(buffer),
        };

	submit_queue_command(state, &cmd, queue);

	// TODO: Wait for completion

	return buffer;
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
	uint8_t *data = identify_queue(state, 0);

	for (int i = 0; i < PAGE_SIZE; i++) {
		printf("%02X ", *(data + i));
	}
	printf("\n");

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
