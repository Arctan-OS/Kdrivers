/**
 * @file rtc.c
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
#include "drivers/dri_defs.h"
#include "drivers/resource.h"
#include "fs/vfs.h"
#include "global.h"

static int init_rtc(struct ARC_Resource *res, void *args) {
	if (res == NULL || args == NULL) {
		return -1;
	}

	char *path = "/dev/rtc0";

        /*
	struct ARC_VFSNodeInfo info = {
	        .type = ARC_VFS_N_DEV,
		.mode = ARC_STD_PERM,
		.resource_overwrite = res,
        };
	vfs_create(path, &info);
        */

	return 0;
}

static int uninit_rtc() {
	return 0;
}

static size_t read_rtc(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
 	}

        return 0;
}

static size_t write_rtc(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
        }

        return 0;
}

static int stat_rtc(struct ARC_Resource *res, char *filename, struct stat *stat) {
	(void)filename;

	if (res == NULL || stat == NULL) {
		return -1;
	}

	return 0;
}

static uint64_t acpi_codes[] = {
	0x95368E5074F817D9,
	ARC_DRIDEF_CODES_TERMINATOR
};

ARC_REGISTER_DRIVER(ARC_DRIGRP_DEV_ACPI, rtc) = {
        .init = init_rtc,
	.uninit = uninit_rtc,
        .read = read_rtc,
        .write = write_rtc,
	.seek = dridefs_int_func_empty,
	.rename = dridefs_int_func_empty,
	.stat = stat_rtc,
	.codes = acpi_codes
};

#undef NAME_FORMAT
