/**
 * @file dsdt.c
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
#include <lib/resource.h>
#include <arch/acpi/acpi.h>
#include <global.h>
#include <drivers/dri_defs.h>

int empty_dsdt() {
	return 0;
}

// This is the structure for tables of signature
// DSDT, SSDT, PSDT (obsolete post ACPI 1.0, treat as
// SSDT if one does appear)
struct xsdt {
	struct ARC_RSDTBaseEntry base;
	uint8_t bytes[];
}__attribute__((packed));

int init_dsdt(struct ARC_Resource *res, void *arg) {
	(void)res;

	struct xsdt *table = (struct xsdt *)arg;

	if (acpi_checksum(table, table->base.length) != 0) {
		return -1;
	}

	// TODO: Possibly check for functional differences depending
	//       on if the table is a DSDT, SSDT, or PSDT. Most likely
	//       non exist, except for loading order, but still
	//       check
	switch (table->base.signature) {
		case ARC_ACPI_TBLSIG_DSDT: {
			ARC_DEBUG(INFO, "Found DSDT at %p, loading\n", table);
			break;
		}

		case ARC_ACPI_TBLSIG_SSDT: {
			ARC_DEBUG(INFO, "Found SSDT at %p, loading\n", table);
			break;
		}

		case ARC_ACPI_TBLSIG_PSDT: {
			ARC_DEBUG(INFO, "Found PSDT at %p, loading\n", table);
			break;
		}
	}

	ARC_DEBUG(WARN, "Definitely loading AML\n");

	return 0;
}

int uninit_dsdt() {
	return 0;
}

ARC_REGISTER_DRIVER(3, dsdt) = {
        .index = ARC_DRI_DSDT,
	.instance_counter = 0,
	.name_format = "dsdt%d",
	.init = init_dsdt,
	.uninit = uninit_dsdt,
	.read = empty_dsdt,
	.write = empty_dsdt,
	.open = empty_dsdt,
	.close = empty_dsdt,
	.rename = empty_dsdt,
	.seek = empty_dsdt,
};
