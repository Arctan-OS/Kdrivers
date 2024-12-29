/**
 * @file super.c
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
 * Superblock dirvers for the EXT2 filesystem.
*/
#include <global.h>
#include <drivers/dri_defs.h>
#include <mm/allocator.h>
#include <lib/perms.h>
#include <drivers/sysfs/ext2/super.h>
#include <lib/util.h>

struct driver_state {
	struct ARC_File *partition;
	struct ext2_block_group_desc *descriptor_table;
	size_t block_size;
	struct ext2_inode *root_directory;
	struct ext2_super_block super;
};

static struct ext2_inode *ext2_read_inode(struct driver_state *state, uint64_t inode) {
	struct ext2_inode *buffer = alloc(state->super.inode_size);// sizeof(struct ext2_inode));

	if (buffer == NULL) {
		return NULL;
	}

	uint64_t block_group = ((inode - 1) / state->super.inodes_per_group);
	uint64_t index_in_table = ((inode - 1) % state->super.inodes_per_group);
	uint64_t inode_table_address = (state->descriptor_table[block_group].inode_table_start) * state->block_size;
	uint64_t inode_offset = state->super.inode_size * index_in_table;

	vfs_seek(state->partition, inode_table_address + inode_offset, SEEK_SET);
	vfs_read(buffer, 1, state->super.inode_size, state->partition);

	return buffer;
}

static int init_ext2_super(struct ARC_Resource *res, void *args) {
	if (res == NULL || args == NULL) {
		return -1;
	}

	struct driver_state *state = (struct driver_state *)alloc(sizeof(*state));

	if (state == NULL) {
		return -2;
	}

	vfs_open(args, 0, ARC_STD_PERM, &state->partition);

	if (state->partition == NULL) {
		free(state);
		return -3;
	}

	// Read in super block
	vfs_seek(state->partition, 1024, SEEK_SET);
	vfs_read(&state->super, 1, sizeof(struct ext2_super_block), state->partition);

	if (state->super.sig != EXT2_SIG) {
		vfs_close(state->partition);
		free(state);
		return -4;
	}

	state->block_size = 1024;
	for (uint32_t i = 0; i < state->super.log2_block_size; i++) {
		state->block_size <<= 1;
	}

	// Read in group descriptor table
	size_t a = (state->super.total_blocks + state->super.blocks_per_group - 1) / state->super.blocks_per_group;
	size_t b = (state->super.total_inodes + state->super.inodes_per_group - 1) / state->super.inodes_per_group;
	size_t c = min(a, b);

	state->descriptor_table = (struct ext2_block_group_desc *)alloc(c * sizeof(struct ext2_block_group_desc));

	if (state->descriptor_table == NULL) {
		vfs_close(state->partition);
		free(state);
		return -5;
	}

	vfs_seek(state->partition, state->block_size * (state->super.superblock + 1), SEEK_SET);
	vfs_read(state->descriptor_table, sizeof(struct ext2_block_group_desc), c, state->partition);

	state->root_directory = ext2_read_inode(state, 2);

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



static void ext2_enumerate_singly_linked(struct driver_state *state, uint32_t *list,
					 void (*callback)(struct driver_state *, uint8_t *block, void *), void *callback_arg) {
	if (state == NULL || list == NULL || callback == NULL) {
		return;
	}

	uint8_t *block = alloc(state->block_size);

	if (block == NULL) {
		return;
	}

	for (size_t i = 0; i < state->block_size / 4; i++) {
		vfs_seek(state->partition, list[i] * state->block_size, SEEK_SET);
		vfs_read(block, state->block_size, 1, state->partition);
		callback(state, block, callback_arg);
	}

	free(block);
}

static void ext2_enumerate_doubly_linked(struct driver_state *state, uint32_t *list,
					 void (*callback)(struct driver_state *, uint8_t *block, void *), void *callback_arg) {
	if (state == NULL || list == NULL || callback == NULL) {
		return;
	}

	uint8_t *block = alloc(state->block_size);

	if (block == NULL) {
		return;
	}

	for (size_t i = 0; i < state->block_size / 4; i++) {
		vfs_seek(state->partition, list[i] * state->block_size, SEEK_SET);
		vfs_read(block, state->block_size, 1, state->partition);
		ext2_enumerate_doubly_linked(state, (uint32_t *)block, callback, callback_arg);
	}

	free(block);
}

static void ext2_enumerate_triply_linked(struct driver_state *state, uint32_t *list,
					 void (*callback)(struct driver_state *, uint8_t *block, void *), void *callback_arg) {
	if (state == NULL || list == NULL || callback == NULL) {
		return;
	}

	uint8_t *block = alloc(state->block_size);

	if (block == NULL) {
		return;
	}

	for (size_t i = 0; i < state->block_size / 4; i++) {
		vfs_seek(state->partition, list[i] * state->block_size, SEEK_SET);
		vfs_read(block, state->block_size, 1, state->partition);
		ext2_enumerate_doubly_linked(state, (uint32_t *)block, callback, callback_arg);
	}

	free(block);
}

struct directory_enumerate_args {
	char *filename;
	uint64_t ret_inode;
};

static void ext2_enumerate_directory_entries(struct driver_state *state, uint8_t *block, void *arg) {
	if (state == NULL || block == NULL || arg == NULL) {
		ARC_DEBUG(ERR, "Failed to enumerate directory entries, imporper parameters (%p %p %p)\n", state, block, arg);
		return;
	}

	struct directory_enumerate_args *dargs = (struct directory_enumerate_args *)arg;

	for (size_t i = 0; i < state->block_size;) {
		struct ext2_dir_ent *ent = (struct ext2_dir_ent *)&block[i];

		if (strncmp(dargs->filename, ent->name, ent->lower_name_len) == 0) {
			dargs->ret_inode = ent->inode;
			return;
		}

		i += ent->total_size;
		if (ent->total_size == 0) {
			i++;
		}
	}

	return;
}

uint32_t ext2_lookup_inode_in_directory(struct driver_state *state, struct ext2_inode *dir, char *filename) {
	if (state == NULL || dir == NULL || filename == NULL) {
		ARC_DEBUG(ERR, "Failed to lookup inode in directory, imporper parameters (%p %p %s)\n", state, dir, filename);
		return 0;
	}

	// Direct
	uint8_t *block = alloc(state->block_size);

	struct directory_enumerate_args args = {
	        .filename = filename,
        };

	for (int i = 0; i < 12; i++) {
		vfs_seek(state->partition, dir->dbp[i] * state->block_size, SEEK_SET);
		vfs_read(block, state->block_size, 1, state->partition);
		ext2_enumerate_directory_entries(state, block, &args);

		if (args.ret_inode != 0) {
			goto ret_case;
		}
	}

	// Singly
	vfs_seek(state->partition, dir->sibp * state->block_size, SEEK_SET);
	vfs_read(block, state->block_size, 1, state->partition);
	ext2_enumerate_singly_linked(state, (uint32_t *)block, ext2_enumerate_directory_entries, &args);

	if (args.ret_inode != 0) {
		goto ret_case;
	}

	// Doubly
	vfs_seek(state->partition, dir->dibp * state->block_size, SEEK_SET);
	vfs_read(block, state->block_size, 1, state->partition);
	ext2_enumerate_doubly_linked(state, (uint32_t *)block, ext2_enumerate_directory_entries, &args);

	if (args.ret_inode != 0) {
		goto ret_case;
	}

	// Triply
	vfs_seek(state->partition, dir->tibp * state->block_size, SEEK_SET);
	vfs_read(block, state->block_size, 1, state->partition);
	ext2_enumerate_triply_linked(state, (uint32_t *)block, ext2_enumerate_directory_entries, &args);

	if (args.ret_inode != 0) {
		goto ret_case;
	}

	ret_case:;
	free(block);

	return args.ret_inode;
}

static int stat_ext2_super(struct ARC_Resource *res, char *filename, struct stat *stat) {
	if (res == NULL || stat == NULL) {
		return -1;
	}

	struct driver_state *state = res->driver_state;

	if (filename == NULL) {
		stat->st_atim.tv_sec = state->root_directory->last_access;
		stat->st_mtim.tv_sec = state->root_directory->last_mod;
		stat->st_ctim.tv_sec = state->root_directory->creation;
		stat->st_mode = state->root_directory->type_perms;

		return 0;
	}

	uint32_t i_inode = ext2_lookup_inode_in_directory(res->driver_state, state->root_directory, filename);
	struct ext2_inode *inode = ext2_read_inode(state, i_inode);

	stat->st_atim.tv_sec = inode->last_access;
	stat->st_mtim.tv_sec = inode->last_mod;
	stat->st_ctim.tv_sec = inode->creation;
	stat->st_size = inode->size_low;
	stat->st_blocks = inode->sectors_used;
	stat->st_mode = inode->type_perms;

	return 0;
}

static void *locate_ext2_super(struct ARC_Resource *res, char *filename) {
	if (res == NULL || filename == NULL) {
		return NULL;
	}

	struct driver_state *state = res->driver_state;
	uint32_t inode = ext2_lookup_inode_in_directory(res->driver_state, state->root_directory, filename);

	return ext2_read_inode(state, inode);
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
