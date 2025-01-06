/**
 * @file super.c
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan - Operating System Kernel
 * Copyright (C) 2023-2025 awewsomegamer
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
 * Superblock dirvers for the EXT2 filesystem.
*/
#include <global.h>
#include <drivers/dri_defs.h>
#include <mm/allocator.h>
#include <lib/perms.h>
#include <drivers/sysfs/ext2/super.h>
#include <drivers/sysfs/ext2/util.h>
#include <lib/util.h>

static int init_ext2_super(struct ARC_Resource *res, void *args) {
	if (res == NULL || args == NULL) {
		ARC_DEBUG(ERR, "Failed to initialize superblock driver, improper parameters (%p %p)\n", res, args);
		return -1;
	}

	struct ext2_super_driver_state *state = alloc(sizeof(*state));

	if (state == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate state\n");
		return -2;
	}

	memset(state, 0, sizeof(*state));
	vfs_open(args, 0, ARC_STD_PERM, &state->basic.partition);

	if (state->basic.partition == NULL) {
		ARC_DEBUG(ERR, "Failed to open partition\n");
		vfs_close(state->basic.partition);
		free(state);
		return -3;
	}

	vfs_seek(state->basic.partition, 1024, SEEK_SET);
	if (vfs_read(&state->super, 1, sizeof(state->super), state->basic.partition) != sizeof(state->super)) {
		ARC_DEBUG(ERR, "Failed to read in super block\n");
		vfs_close(state->basic.partition);
		free(state);
		return -4;
	}

	if (state->super.sig != EXT2_SIG) {
		ARC_DEBUG(ERR, "Signature mismatch\n");
		vfs_close(state->basic.partition);
		free(state);
		return -5;
	}

	state->basic.block_size = 1024;
	for (int i = state->super.log2_block_size; i > 0; i--) {
		state->basic.block_size <<= 1;
	}

	size_t block_groups = min((state->super.total_blocks + state->super.blocks_per_group - 1) / state->super.blocks_per_group,
                                  (state->super.total_inodes + state->super.inodes_per_group - 1) / state->super.inodes_per_group);

	struct ext2_block_group_desc *descriptor_table = alloc(block_groups * sizeof(struct ext2_block_group_desc));

	if (descriptor_table == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate descriptor table\n");
		vfs_close(state->basic.partition);
		free(state);
		return -6;
	}

	vfs_seek(state->basic.partition, (1 + state->super.superblock) * state->basic.block_size, SEEK_SET);
	if (vfs_read(descriptor_table, 1, block_groups * sizeof(struct ext2_block_group_desc), state->basic.partition)
	    != block_groups * sizeof(struct ext2_block_group_desc)) {
		ARC_DEBUG(ERR, "Failed to read in descriptor table\n");
		vfs_close(state->basic.partition);
		free(descriptor_table);
		free(state);
		return -6;
	}

	state->descriptor_table = descriptor_table;
	state->parition_path = strdup(args);
	state->basic.node = ext2_read_inode(state, 2);
	state->basic.inode = 2;
	res->driver_state = state;

	return 0;
}

static int uninit_ext2_super() {
	return 0;
};

static size_t read_ext2_super(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
 	}

	return 0;
}

static size_t write_ext2_super(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
 	}

	return 0;
}

static int stat_ext2_super(struct ARC_Resource *res, char *filename, struct stat *stat) {
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

	struct ext2_super_driver_state *state = res->driver_state;

	uint64_t inode_number = ext2_get_inode_in_dir(&state->basic, filename);
	struct ext2_inode *inode = ext2_read_inode(state, inode_number);

	if (inode == NULL) {
		ARC_DEBUG(ERR, "Failed to stat, no inode found\n");
		return -1;
	}

	stat->st_mode = inode->type_perms;

	return 0;
}

static void *locate_ext2_super(struct ARC_Resource *res, char *filename) {
	if (res == NULL || filename == NULL) {
		ARC_DEBUG(ERR, "Failed to locate, improper parameters (%p %p)\n", res, filename);
		return NULL;
	}

	struct ext2_locate_args *args = alloc(sizeof(*args));

	if (args == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate arguments\n");
		return NULL;
	}

	struct ext2_super_driver_state *state = res->driver_state;
	uint64_t inode = ext2_get_inode_in_dir(&state->basic, filename);
	args->node = ext2_read_inode(state, inode);
	args->inode = inode;
	args->super = state;

	return args;
}

struct ext2_inode *ext2_read_inode(struct ext2_super_driver_state *state, uint64_t inode) {
	if (state == NULL || state->descriptor_table == NULL || inode == 0) {
		ARC_DEBUG(ERR, "Failed to read inode, improper parameters (%p %p %lu)\n", state, state->descriptor_table, inode);
		return NULL;
	}

	struct ext2_inode *buffer = alloc(state->super.inode_size);// sizeof(struct ext2_inode));

	if (buffer == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate inode\n");
		return NULL;
	}

	uint64_t block_group = ((inode - 1) / state->super.inodes_per_group);
	uint64_t index_in_table = ((inode - 1) % state->super.inodes_per_group);
	uint64_t inode_table_address = (state->descriptor_table[block_group].inode_table_start) * state->basic.block_size;
	uint64_t inode_offset = state->super.inode_size * index_in_table;

	vfs_seek(state->basic.partition, inode_table_address + inode_offset, SEEK_SET);
	vfs_read(buffer, 1, state->super.inode_size, state->basic.partition);

	return buffer;
}

ARC_REGISTER_DRIVER(0, ext2, super) = {
        .init = init_ext2_super,
	.uninit = uninit_ext2_super,
	.write = dridefs_size_t_func_empty,
	.read = dridefs_size_t_func_empty,
	.seek = dridefs_int_func_empty,
	.rename = dridefs_int_func_empty,
	.stat = stat_ext2_super,
	.control = dridefs_void_func_empty,
	.create = dridefs_int_func_empty,
	.remove = dridefs_int_func_empty,
	.locate = locate_ext2_super,
};
