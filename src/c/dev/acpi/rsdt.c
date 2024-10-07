/**
 * @file rsdt.c
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
#include <arch/acpi/acpi.h>
#include <fs/vfs.h>
#include <drivers/dri_defs.h>
#include <lib/perms.h>

struct rsdp {
	char signature[8];
	uint8_t checksum;
	uint8_t OEMID[6];
	uint8_t revision;
	uint32_t rsdt_addr;
	uint32_t length;
	uint64_t xsdt_addr;
	uint8_t ext_checksum;
	uint8_t resv0[3];
}__attribute__((packed));

struct rsdt {
	struct ARC_RSDTBaseEntry base;
	uint32_t entries[];
}__attribute__((packed));

struct xsdt {
	struct ARC_RSDTBaseEntry base;
	uint64_t entries[];
}__attribute__((packed));

int do_rsdt(void *address) {
	struct rsdt *table = (struct rsdt *)address;

	if (table->base.signature != ARC_ACPI_TBLSIG_RSDT) {
		ARC_DEBUG(ERR, "Invalid RSDT signature!\n");
		return -1;
	}

	if (acpi_checksum(table, table->base.length) != 0) {
		ARC_DEBUG(ERR, "Invalid RSDT checksum!\n");
		return -1;
	}

	ARC_DEBUG(INFO, "RSDT:\n");
	ARC_DEBUG(INFO, "\tRevision: %d\n", table->base.revision);
	ARC_DEBUG(INFO, "\tLength: %d B\n", table->base.length);
	ARC_DEBUG(INFO, "\tOEMID: %.*s\n", 6, table->base.OEMID);
	ARC_DEBUG(INFO, "\tOEMTID: %.*s\n", 8, table->base.OEMTID);
	ARC_DEBUG(INFO, "\tOEMREV: %d\n", table->base.OEMREV);
	ARC_DEBUG(INFO, "\tCID: %.*s\n", 4, table->base.creator_id);
	ARC_DEBUG(INFO, "\tCREV: %d\n", table->base.creator_rev);

	int entries = (table->base.length - sizeof(struct rsdt)) / 4;
	for (int i = 0; i < entries; i++) {
		struct ARC_RSDTBaseEntry *entry = (void *)ARC_PHYS_TO_HHDM(table->entries[i]);

		int index = -1;

		switch (entry->signature) {
			case ARC_ACPI_TBLSIG_APIC: {
				size_t size = entry->length - sizeof(struct ARC_RSDTBaseEntry) - 8;
				vfs_create("/dev/acpi/apic", ARC_STD_PERM, ARC_VFS_N_BUFF, &size);
				struct ARC_File *file = NULL;
				vfs_open("/dev/acpi/apic", 0, ARC_STD_PERM, (void *)&file);
				vfs_write((uint8_t *)entry + sizeof(struct ARC_RSDTBaseEntry) + 8, 1, size, file);
				vfs_close(file);

				continue;
			}

			case ARC_ACPI_TBLSIG_FACP: {
				index = ARC_DRI_FADT;

				break;
			}

			case ARC_ACPI_TBLSIG_HPET: {
				index = ARC_DRI_HPET;

				break;
			}

			default: {
				ARC_DEBUG(INFO, "Unimplemented RSDT table \"%.*s\", 0x%"PRIX32"\n", 4, (char *)&entry->signature, entry->signature);
				continue;
			}
		}

		if (init_resource_at("/dev/acpi", ARC_DRI_DEV, index, (void *)entry) != 0) {
			ARC_DEBUG(ERR, "Failed to initialize resource\n");
		}
	}

	return 0;
}

int do_xsdt(void *address) {
	struct xsdt *table = (struct xsdt *)address;

	if (table->base.signature != ARC_ACPI_TBLSIG_XSDT) {
		ARC_DEBUG(ERR, "Invalid XSDT signature!\n");
		return -1;
	}

	if (acpi_checksum(table, table->base.length) != 0) {
		ARC_DEBUG(ERR, "Invalid XSDT checksum!\n");
		return -1;
	}

	ARC_DEBUG(INFO, "XSDT:\n");
	ARC_DEBUG(INFO, "\tRevision: %d\n", table->base.revision);
	ARC_DEBUG(INFO, "\tLength: %d B\n", table->base.length);
	ARC_DEBUG(INFO, "\tOEMID: %.*s\n", 6, table->base.OEMID);
	ARC_DEBUG(INFO, "\tOEMTID: %.*s\n", 8, table->base.OEMTID);
	ARC_DEBUG(INFO, "\tOEMREV: %d\n", table->base.OEMREV);
	ARC_DEBUG(INFO, "\tCID: %.*s\n", 4, table->base.creator_id);
	ARC_DEBUG(INFO, "\tCREV: %d\n", table->base.creator_rev);

	int entries = (table->base.length - sizeof(struct xsdt)) / 8;
	for (int i = 0; i < entries; i++) {
		struct ARC_RSDTBaseEntry *entry = (void *)ARC_PHYS_TO_HHDM(table->entries[i]);

		int index = -1;

		switch (entry->signature) {
			case ARC_ACPI_TBLSIG_APIC: {
				size_t size = entry->length - sizeof(struct ARC_RSDTBaseEntry) - 8;
				vfs_create("/dev/acpi/apic", ARC_STD_PERM, ARC_VFS_N_BUFF, &size);
				struct ARC_File *file = NULL;
				vfs_open("/dev/acpi/apic", 0, ARC_STD_PERM, (void *)&file);
				vfs_write((uint8_t *)entry + sizeof(struct ARC_RSDTBaseEntry) + 8, 1, size, file);
				vfs_close(file);

				continue;
			}

			case ARC_ACPI_TBLSIG_FACP: {
				index = ARC_DRI_FADT;

				break;
			}

			case ARC_ACPI_TBLSIG_HPET: {
				index = ARC_DRI_HPET;

				break;
			}

			default: {
				ARC_DEBUG(INFO, "Unimplemented RSDT table \"%.*s\", 0x%"PRIx32"\n", 4, (char *)&entry->signature, entry->signature);
				continue;
			}
		}

		if (init_resource_at("/dev/acpi", ARC_DRI_DEV, index, (void *)entry) != 0) {
			ARC_DEBUG(ERR, "Failed to initialize resource\n");
		}
	}

	return 0;
}

int empty_rsdt() {
	return 0;
}

int init_rsdt(struct ARC_Resource *res, void *arg) {
	(void)res;

	struct rsdp *rsdp = (struct rsdp *)arg;

	if (rsdp->revision == 0 && acpi_checksum(rsdp, 20) == 0) {
		// Revision 0
		return do_rsdt((void *)ARC_PHYS_TO_HHDM(rsdp->rsdt_addr));
	} else if (acpi_checksum(rsdp, sizeof(struct rsdp)) == 0) {
		// Revision N
		return do_xsdt((void *)ARC_PHYS_TO_HHDM(rsdp->xsdt_addr));
	} else {
		// Fail
		ARC_DEBUG(INFO, "Invalid checksum\n");
		return -1;
	}

	// No idea how you got here
	return -2;
}

int uninit_rsdt() {
	return 0;
};

ARC_REGISTER_DRIVER(3, rsdt_driver) = {
        .index = ARC_DRI_RSDT,
	.instance_counter = 0,
	.name_format = "acpi",
        .init = init_rsdt,
	.uninit = uninit_rsdt,
	.read = empty_rsdt,
	.write = empty_rsdt,
	.seek = empty_rsdt,
	.rename = empty_rsdt,
	.open = empty_rsdt,
	.close = empty_rsdt,
};
