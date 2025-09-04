/**
 * @file file.c
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
 * File driver for the EXT2 filesystem.
*/
#include "abi-bits/seek-whence.h"
#include "drivers/dri_defs.h"
#include "drivers/resource.h"
#include "drivers/sysfs/ext2/ext2.h"
#include "drivers/sysfs/ext2/super.h"
#include "drivers/sysfs/ext2/util.h"
#include "fs/vfs.h"
#include "lib/util.h"
#include "mm/allocator.h"

static int init_ext2_file(struct ARC_Resource *res, void *args) {
	if (res == NULL || args == NULL) {
		ARC_DEBUG(ERR, "Failed to initialize file driver, improper parameters (%p %p)\n", res, args);
		return -1;
	}

	struct ext2_node_driver_state *state = alloc(sizeof(*state));

	if (state == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate state\n");
		return -2;
	}

	struct ext2_locate_args *cast_args = args;

	vfs_open(cast_args->super->parition_path, 0, ARC_STD_PERM, &state->basic.partition);

	if (state->basic.partition == NULL) {
		ARC_DEBUG(ERR, "Failed to open partition\n");
		free(state);
		return -3;
	}

	state->super = cast_args->super;
	// NOTE: This was allocated by the super driver, but we own it now, we must free it
	state->basic.node = cast_args->node;
	state->basic.inode = cast_args->inode;
	state->basic.block_size = cast_args->super->basic.block_size;

	res->driver_state = state;

	free(cast_args);

	return 0;
}

static int uninit_ext2_file(struct ARC_Resource *res) {
	if (res == NULL) {
		return -1;
	}

	// TODO: Sync
	free(res->driver_state);

	return 0;

};

static size_t read_ext2_file(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
 	}

	struct ext2_node_driver_state *state = res->driver_state;

	return ext2_read_inode_data(&state->basic, buffer, file->offset, size * count);
}

static size_t write_ext2_file(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
 	}

	struct ext2_node_driver_state *state = res->driver_state;

	return ext2_write_inode_data(state, buffer, file->offset, size * count);
}

static int stat_ext2_file(struct ARC_Resource *res, char *filename, struct stat *stat) {
	(void)filename;
	if (res == NULL || stat == NULL) {
		ARC_DEBUG(ERR, "Failed to stat file, improper parameters (%p %p)\n", res, stat);
		return -1;
	}

	struct ext2_node_driver_state *state = res->driver_state;

	stat->st_mode = state->basic.node->type_perms;
	stat->st_size = state->basic.node->size_low;

	return 0;
}

ARC_REGISTER_DRIVER(0, ext2, file) = {
        .init = init_ext2_file,
	.uninit = uninit_ext2_file,
	.write = write_ext2_file,
	.read = read_ext2_file,
	.seek = dridefs_int_func_empty,
	.rename = dridefs_int_func_empty,
	.stat = stat_ext2_file,
	.control = dridefs_void_func_empty,
	.create = dridefs_int_func_empty,
	.remove = dridefs_int_func_empty,
	.locate = dridefs_void_func_empty,
};
