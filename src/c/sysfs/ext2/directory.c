/**
 * @file directory.c
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
#include "abi-bits/seek-whence.h"
#include "drivers/dri_defs.h"
#include "drivers/resource.h"
#include "drivers/sysfs/ext2/super.h"
#include "drivers/sysfs/ext2/util.h"
#include "fs/vfs.h"
#include "mm/allocator.h"

static int init_ext2_directory(struct ARC_Resource *res, void *args) {
	if (res == NULL || args == NULL) {
		ARC_DEBUG(ERR, "Failed to initialize directory driver, improper parameters (%p %p)\n", res, args);
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

static int uninit_ext2_directory(struct ARC_Resource *res) {
	if (res == NULL) {
		return -1;
	}

	// TODO: Sync
	free(res->driver_state);

	return 0;
};

static size_t read_ext2_directory(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
 	}

	return 0;
}

static size_t write_ext2_directory(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
 	}

	return 0;
}

static int stat_ext2_directory(struct ARC_Resource *res, char *filename, struct stat *stat) {
	if (res == NULL || stat == NULL) {
		ARC_DEBUG(ERR, "Failed to stat, improper parameters (%p %p)\n", res, stat);
		return -1;
	}

	if (filename == NULL) {
		ARC_DEBUG(ERR, "Failed to stat, no file name given\n");
		return 0;
	}

	// TODO: Since the VFS calls stat prior to locate, could this part of the stat
	//       and the locate function work together in a cache?

	struct ext2_node_driver_state *state = res->driver_state;

	uint64_t inode_number = ext2_get_inode_in_dir(&state->basic, filename);
	struct ext2_inode *inode = ext2_read_inode(state->super, inode_number);

	if (inode == NULL) {
		ARC_DEBUG(ERR, "Failed to stat, no inode found\n");
		return -1;
	}

	stat->st_mode = inode->type_perms;

	return 0;
}

static void *locate_ext2_directory(struct ARC_Resource *res, char *filename) {
	if (res == NULL || filename == NULL) {
		ARC_DEBUG(ERR, "Failed to locate, improper parameters (%p %p)\n", res, filename);
		return NULL;
	}

	struct ext2_locate_args *args = alloc(sizeof(*args));

	if (args == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate arguments\n");
		return NULL;
	}

	struct ext2_node_driver_state *state = res->driver_state;
	uint64_t inode = ext2_get_inode_in_dir(&state->basic, filename);
	args->node = ext2_read_inode(state->super, inode);
	args->inode = inode;
	args->super = state->super;

	return args;
}

ARC_REGISTER_DRIVER(ARC_DRIGRP_FS_DIR, ext2) = {
        .init = init_ext2_directory,
	.uninit = uninit_ext2_directory,
	.write = write_ext2_directory,
	.read = read_ext2_directory,
	.seek = dridefs_int_func_empty,
	.rename = dridefs_int_func_empty,
	.stat = stat_ext2_directory,
	.control = dridefs_void_func_empty,
	.create = dridefs_int_func_empty,
	.remove = dridefs_int_func_empty,
	.locate = locate_ext2_directory,
};
