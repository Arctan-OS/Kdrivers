/**
 * @file ext2.c
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
#include <drivers/sysfs/super/ext2.h>
#include <lib/util.h>

#define EXT2_INODE2_BLOCK_GROUP(__inode, __state) \
	((inode - 1) / __state->super.inodes_per_group)

#define EXT2_INODE2_ITABLE(__inode, __state) \
	((inode - 1) % __state->super.inodes_per_group)

#define EXT2_INODE2_BLOCK(__inode, __state) \
	(EXT2_INODE2_ITABLE(__inode, __state) / __state->block_size)

struct driver_state {
	struct ARC_Resource *res;
	struct ARC_File *partition;
	struct ext2_block_group_desc *descriptor_table;
	size_t block_size;
	uint64_t root_directory_block;
	struct ext2_super_block super;
};

struct directory_hints {
	uint64_t inode_block;
};

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

	vfs_seek(state->partition, (state->super.superblock + 1) * state->block_size, SEEK_SET);
	vfs_read(state->descriptor_table, sizeof(struct ext2_block_group_desc), c, state->partition);

	state->res = res;
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
	(void)filename;

	if (res == NULL || stat == NULL) {
		return -1;
	}

	return 0;
}

static uint64_t get_node_from_dir(uint64_t start_block, char *filepath) {
	size_t last_sep = 0;
	size_t max = strlen(filepath);

	for (size_t i = 0; i < max; i++) {
		if (filepath[i] != '/' && i != max - 1) {
			continue;
		}

		size_t comp_len = i - last_sep - 1;

		if (comp_len == 0) {
			continue;
		}

		char *comp_name = strndup(&filepath[last_sep + 1], comp_len);

		if (filepath[i] == '/') {
			last_sep = i;
		}
	}

	return 0;
}

static void *locate_ext2_super(struct ARC_Resource *res, char *filename) {
	if (res == NULL || filename == NULL) {
		return NULL;
	}

	return NULL;
}


ARC_REGISTER_DRIVER(0, ext2, super) = {
        .init = init_ext2_super,
	.uninit = uninit_ext2_super,
	.write = write_ext2_super,
	.read = read_ext2_super,
	.seek = dridefs_int_func_empty,
	.rename = dridefs_int_func_empty,
	.stat = stat_ext2_super,
	.control = dridefs_void_func_empty,
	.create = dridefs_int_func_empty,
	.remove = dridefs_int_func_empty,
	.locate = locate_ext2_super,
};
