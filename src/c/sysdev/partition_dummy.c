/**
 * @file partition_dummy.c
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
#include <drivers/sysdev/partition_dummy.h>
#include <lib/resource.h>
#include <global.h>
#include <mm/allocator.h>
#include <lib/util.h>
#include <lib/perms.h>
#include <drivers/dri_defs.h>
#include <abi-bits/seek-whence.h>

#define NAME_FORMAT "%sp%d"

struct driver_state {
	struct ARC_File *drive;
	uint64_t attrs;
	uint64_t start_lba;
	size_t size_in_lbas;
	size_t lba_size;
	uint32_t partition_number;
};

static int init_partition_dummy(struct ARC_Resource *res, void *args) {
	if (res == NULL || args == NULL) {
		return -1;
	}

	struct driver_state *state = (struct driver_state *)alloc(sizeof(*state));

	if (state == NULL) {
		return -2;
	}

	memset(state, 0, sizeof(*state));

	struct ARC_DriArgs_ParitionDummy *dri_args = (struct ARC_DriArgs_ParitionDummy *)args;

	state->attrs = dri_args->attrs;
	state->start_lba = dri_args->lba_start;
	state->lba_size = dri_args->lba_size;
	state->size_in_lbas = dri_args->size_in_lbas;
	state->partition_number = dri_args->partition_number;

	vfs_open(dri_args->drive_path, 0, ARC_STD_PERM, &state->drive);
	res->driver_state = state;

	char *path = (char *)alloc(strlen(dri_args->drive_path) + 32);
	sprintf(path, NAME_FORMAT, dri_args->drive_path, dri_args->partition_number);

	struct ARC_VFSNodeInfo info = {
	        .type = ARC_VFS_N_DEV,
		.mode = ARC_STD_PERM,
		.resource_overwrite = res,
        };
	vfs_create(path, &info);

	free(path);

	return 0;
}

static int uninit_partition_dummy() {
	return 0;
};

static size_t read_partition_dummy(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
 	}

	struct driver_state *state = (struct driver_state *)res->driver_state;

	vfs_seek(state->drive, file->offset + (state->start_lba * state->lba_size), SEEK_SET);

	return vfs_read(buffer, size, count, state->drive);
}

static size_t write_partition_dummy(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
 	}

	struct driver_state *state = (struct driver_state *)res->driver_state;

	vfs_seek(state->drive, file->offset + (state->start_lba * state->lba_size), SEEK_SET);

	return vfs_write(buffer, size, count, state->drive);
}

static int stat_partition_dummy(struct ARC_Resource *res, char *filename, struct stat *stat) {
	(void)filename;

	if (res == NULL || stat == NULL) {
		return -1;
	}

	struct driver_state *state = (struct driver_state *)res->driver_state;

	stat->st_blksize = state->lba_size;
	stat->st_blocks = state->size_in_lbas;
	stat->st_size = (state->lba_size * state->size_in_lbas);

	return 0;
}

ARC_REGISTER_DRIVER(3, partition_dummy,) = {
        .init = init_partition_dummy,
	.uninit = uninit_partition_dummy,
	.read = read_partition_dummy,
	.write = write_partition_dummy,
	.seek = dridefs_int_func_empty,
	.rename = dridefs_int_func_empty,
	.stat = stat_partition_dummy,
};

#undef NAME_FORMAT
