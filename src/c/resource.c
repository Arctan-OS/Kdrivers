/**
 * @file resource.c
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
#include "abi-bits/errno.h"
#include "arch/pci.h"
#include "drivers/dri_defs.h"
#include "drivers/resource.h"
#include "global.h"
#include "lib/util.h"
#include "mm/allocator.h"

/*
** Driver Groups:
**  Group Number | Description
**  0            | Base filesystem drivers.
**  1            | User filesystem drivers.
**  2            | User device drivers.
**  3            | Base device drivers.
*/

uint64_t current_id = 0;

ARC_Resource *init_resource(int64_t dri_index, void *args) {
	if (dri_index >= ARC_DRIDEF_COUNT || dri_index == -1) {
		ARC_DEBUG(ERR, "Invalid driver index (0x%"PRIx64")\n", dri_index);
		return NULL;
	}

	ARC_Resource *resource = (struct ARC_Resource *)alloc(sizeof(struct ARC_Resource));

	if (resource == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate memory for resource\n");
		return NULL;
	}

	memset(resource, 0, sizeof(struct ARC_Resource));

	ARC_DEBUG(INFO, "Initializing resource %lu (Index: %lu)\n", current_id, dri_index);

	// Initialize resource properties
	resource->id = ARC_ATOMIC_LOAD(current_id);
	ARC_ATOMIC_INC(current_id);

	resource->dri_index = dri_index;

	// Fetch and set the appropriate definition
	struct ARC_DriverDef *def = _DRIVER_LOOKUP_TABLE[dri_index];
	resource->driver = def;

	if (def == NULL) {
		free(resource);
		ARC_DEBUG(ERR, "No driver definition found\n");

		return NULL;
	}

	if (def->init == NULL) {
		free(resource);
		ARC_DEBUG(ERR, "Driver does not define an initialization function\n");

		return NULL;
	}

	int ret = def->init(resource, args);

	if (ret != 0) {
		free(resource);
		ARC_DEBUG(ERR, "Driver init function returned %d\n", ret);

		return NULL;
	}

	return resource;
}

static uint64_t get_dri_def_pci(uint16_t vendor, uint16_t device) {
	uint32_t target = (vendor << 16) | device;

	for (uint64_t i = 0; i < ARC_DRIDEF_COUNT; i++) {
		struct ARC_DriverDef *def = _DRIVER_LOOKUP_TABLE[i];

		if (def->pci_codes == NULL) {
			continue;
		}

		for (uint32_t code = 0;; code++) {
			if (def->pci_codes[code] == ARC_DRIDEF_PCI_TERMINATOR) {
				break;
			} else if (def->pci_codes[code] == target) {
				return i;
			}
		}
	}

	return (uint64_t)-1;
}

ARC_Resource *init_pci_resource(ARC_PCIHeaderMeta *meta) {
	uint16_t vendor = meta->header->common.vendor_id;
	uint16_t device = meta->header->common.device_id;

	if (vendor == 0xFFFF && device == 0xFFFF) {
		ARC_DEBUG(WARN, "Skipping PCI resource initialization\n");
		return NULL;
	}

	ARC_DEBUG(INFO, "Initializing PCI resource 0x%04x:0x%04x\n", vendor, device);

	return init_resource(get_dri_def_pci(vendor, device), meta);
}

static uint64_t get_dri_def_acpi(uint64_t hid_hash) {
	for (uint64_t i = 0; i < ARC_DRIDEF_COUNT; i++) {
		struct ARC_DriverDef *def = _DRIVER_LOOKUP_TABLE[i];

		if (def->acpi_codes == NULL) {
			continue;
		}

		for (uint32_t code = 0;; code++) {
			if (def->acpi_codes[code] == ARC_DRIDEF_ACPI_TERMINATOR) {
				break;
			} else if (def->acpi_codes[code] == hid_hash) {
				return i;
			}
		}
	}

	return (uint64_t)-1;
}

ARC_Resource *init_acpi_resource(uint64_t hid_hash, void *args) {
	if (hid_hash == 0) {
		ARC_DEBUG(WARN, "Skipping ACPI resource initialization\n");
		return NULL;
	}

	ARC_DEBUG(INFO, "Initializing ACPI resource (%"PRIX64")\n", hid_hash);

	return init_resource(get_dri_def_acpi(hid_hash), args);
}

int uninit_resource(struct ARC_Resource *resource) {
	if (resource == NULL) {
		ARC_DEBUG(ERR, "Resource is NULL, cannot uninitialize\n");
		return 1;
	}

	ARC_DEBUG(INFO, "Uninitializing resource %lu\n", resource->id);

	resource->driver->uninit(resource);

	free(resource);

	return 0;
}
