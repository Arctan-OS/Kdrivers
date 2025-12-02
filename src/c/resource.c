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

static uint64_t current_id = 0;

ARC_Resource *init_resource(int dri_group, int64_t dri_index, void *args) {
        int entry_count = dridefs_get_entry_count(dri_group);
	if (dri_group < 0 || dri_index < 0 || dri_group >= ARC_DRIDEF_DRIVER_GROUPS
            || dri_index >= entry_count) {
		ARC_DEBUG(ERR, "Invalid parameters group: !(0<=%d<%d) or index: !(0<=%d<%d) are true\n", dri_group, ARC_DRIDEF_DRIVER_GROUPS, dri_index, entry_count);
		return NULL;
	}

	ARC_DriverDef *def = arc_dris_table[dri_group][dri_index];
        
        if (def == NULL || def->init == NULL) {
		ARC_DEBUG(ERR, "No driver definition found\n");
		return NULL;
	}
        
	ARC_Resource *resource = (struct ARC_Resource *)alloc(sizeof(struct ARC_Resource));

	if (resource == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate memory for resource\n");
		return NULL;
	}

	memset(resource, 0, sizeof(struct ARC_Resource));

	ARC_DEBUG(INFO, "Initializing resource %lu (Index: %lu)\n", current_id, dri_index);

	resource->id = ARC_ATOMIC_INC(current_id) - 1;
        resource->dri_group = dri_group;
	resource->dri_index = dri_index;
	resource->driver = def;

	int ret = def->init(resource, args);

	if (ret != 0) {
		free(resource);
		ARC_DEBUG(ERR, "Driver init function returned %d\n", ret);

		return NULL;
	}

	return resource;
}

static int internal_find_code(uint64_t target, int group) {
        int count = dridefs_get_entry_count(group);
        
	for (uint64_t i = 0; i < count; i++) {
		ARC_DriverDef *def = arc_dris_table[group][i];

		if (def == NULL || def->codes == NULL) {
			continue;
		}

		for (uint32_t code = 0; def->codes[code] != ARC_DRIDEF_CODES_TERMINATOR; code++) {
			if (def->codes[code] == target) {
                                return i;
			}
		}
	}
}

ARC_Resource *init_pci_resource(ARC_PCIHeaderMeta *meta) {
	uint16_t vendor = meta->header->common.vendor_id;
	uint16_t device = meta->header->common.device_id;

	if (vendor == 0xFFFF && device == 0xFFFF) {
		ARC_DEBUG(WARN, "Skipping PCI resource initialization\n");
		return NULL;
	}

        uint32_t target = (vendor << 16) | device;
        int group = ARC_DRIGRP_DEV_PCI;
        int index = internal_find_code(target, group);

        ARC_DEBUG(INFO, "Initializing PCI resource %04X:%04X (%d, %d)\n", vendor, device, group, index);
        
        return init_resource(group, index, (void *)meta);
}

ARC_Resource *init_acpi_resource(uint64_t hid_hash, void *args) {
	if (hid_hash == 0) {
		ARC_DEBUG(WARN, "Skipping ACPI resource initialization\n");
		return NULL;
	}
        
        int group = ARC_DRIGRP_DEV_ACPI;
        int index = internal_find_code(hid_hash, group);       

        ARC_DEBUG(INFO, "Initializing ACPI resource 0x%"PRIX64" (%d, %d)\n", hid_hash, group, index);
        
        return init_resource(group, index, args);
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
