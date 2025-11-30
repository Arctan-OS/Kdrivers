/**
 * @file resource.h
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
#ifndef ARC_RESOURCE_H
#define ARC_RESOURCE_H

#include "arch/pci.h"
#include "lib/graph/base.h"
#include "sys/stat.h"

#include <stddef.h>
#include <stdint.h>

#define ARC_REGISTER_DRIVER(group, name) \
	ARC_DriverDef _driver_##name##_##group

typedef struct ARC_Resource {
	/// ID
	uint64_t id;
	/// Specific driver function set (supplied on init by caller).
	uint64_t dri_index;
	/// Driver functions.
	struct ARC_DriverDef *driver;
	/// State managed by driver, owned by resource.
	void *driver_state;
} ARC_Resource;

typedef struct ARC_File {
	/// Current offset into the file.
	long offset;
	/// Pointer to the VFS node.
	ARC_GraphNode *node;
} ARC_File;

// NOTE: No function pointer in a driver definition
//       should be NULL.
typedef struct ARC_DriverDef {
	int (*init)(ARC_Resource *res, void *args);
	int (*uninit)(ARC_Resource *res);
	size_t (*write)(void *buffer, size_t size, size_t count, ARC_File *file, ARC_Resource *res);
	size_t (*read)(void *buffer, size_t size, size_t count, ARC_File *file, ARC_Resource *res);
	int (*seek)(ARC_File *file, ARC_Resource *res);
	int (*rename)(char *a, char *b, ARC_Resource *res);
	int (*stat)(ARC_Resource *res, char *path, struct stat *stat);
	void *(*control)(ARC_Resource *res, void *buffer, size_t size);
	int (*create)(ARC_Resource *res, char *path, uint32_t mode, int type);
	int (*remove)(ARC_Resource *res, char *path);
	void *(*locate)(ARC_Resource *res, char *path);
 	uint32_t *pci_codes; // Terminates with ARC_DRI_PCI_TERMINATOR if non-NULL
	uint64_t *acpi_codes; // Terminates with ARC_DRI_ACPI_TERMINATOR if non-NULL
} ARC_DriverDef;

ARC_Resource *init_resource(ARC_DriverDef *dri_list, int64_t dri_index, void *args);
ARC_Resource *init_pci_resource(ARC_PCIHeaderMeta *meta);
ARC_Resource *init_acpi_resource(uint64_t hid_hash, void *args);
int uninit_resource(struct ARC_Resource *resource);

#endif
