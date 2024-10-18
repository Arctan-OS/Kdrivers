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

struct controller_properties {
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
}__attribute__((packed));
STATIC_ASSERT(sizeof(struct controller_properties) == 0x1000, "Controller properties size mismatch");

int empty_nvme() {
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

	pager_map(ARC_PHYS_TO_HHDM(mem_registers_base), mem_registers_base, 0x1000, 1 << ARC_PAGER_4K | 1 << ARC_PAGER_NX | 1 << ARC_PAGER_RW | ARC_PAGER_PAT_UC);

	ARC_DEBUG(INFO, "DSTRD: %d\n", (properties->cap >> 32) & 0b111);
	ARC_DEBUG(INFO, "Max Queue Entries: %d\n", properties->cap & 0xFFFF);
	return 0;
}

int uninit_nvme() {
	return 0;
};

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
	.read = empty_nvme,
	.write = empty_nvme,
	.seek = empty_nvme,
	.rename = empty_nvme,
	.open = empty_nvme,
	.close = empty_nvme,
	.pci_codes = pci_codes
};
