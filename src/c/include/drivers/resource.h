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

#define ARC_SHARE_DRIVER_INDICES(...) ;

enum ARC_DRI_GROUP {
        ARC_DRIGRP_FS_SUPER = 0,
	ARC_DRIGRP_FS_DIR,
	ARC_DRIGRP_FS_FILE,
	ARC_DRIGRP_DEV_ACPI,
	ARC_DRIGRP_DEV_PCI,
        ARC_DRIGRP_DEV,
};

ARC_SHARE_DRIVER_INDICES(ARC_DRIGRP_FS_SUPER, ARC_DRIGRP_FS_DIR, ARC_DRIGRP_FS_FILE)

typedef struct ARC_Resource {
	uint64_t id;
	uint64_t dri_index;
	struct ARC_DriverDef *driver;
	void *driver_state;
        int dri_group;
} ARC_Resource;

typedef struct ARC_File {
	long offset;
	ARC_GraphNode *node;
} ARC_File;

// NOTE: No function pointer in a driver definition
//       should be NULL.
typedef struct ARC_DriverDef {
	int    (*init)   (ARC_Resource *res, void *args);
	int    (*uninit) (ARC_Resource *res);
	size_t (*write)  (void *buffer, size_t size, size_t count, ARC_File *file, ARC_Resource *res);
	size_t (*read)   (void *buffer, size_t size, size_t count, ARC_File *file, ARC_Resource *res);
	int    (*seek)   (ARC_File *file, ARC_Resource *res);
	int    (*rename) (char *from, char *to, ARC_Resource *res);
	int    (*stat)   (ARC_Resource *res, char *path, struct stat *stat);
	void  *(*control)(ARC_Resource *res, void *buffer, size_t size);
	int    (*create) (ARC_Resource *res, char *path, uint32_t mode, int type);
	int    (*remove) (ARC_Resource *res, char *path);
	void  *(*locate) (ARC_Resource *res, char *path);
	uint64_t *codes; // Terminate with ARC_DRIDEF_CODES_TERMINATOR
} ARC_DriverDef;

ARC_Resource *init_resource(int dri_group, int64_t dri_index, void *args);
ARC_Resource *init_pci_resource(ARC_PCIHeaderMeta *meta);
ARC_Resource *init_acpi_resource(uint64_t hid_hash, void *args);
int uninit_resource(struct ARC_Resource *resource);

#endif
