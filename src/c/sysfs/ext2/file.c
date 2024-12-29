/**
 * @file file.c
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
 * File driver for the EXT2 filesystem.
*/
#include <lib/resource.h>
#include <drivers/dri_defs.h>

struct driver_state {
	struct ARC_File *partition;
	size_t block_size;
	struct ext2_inode *inode;
};

static int init_ext2_file(struct ARC_Resource *res, void *args) {
	if (res == NULL || args == NULL) {
		return -1;
	}

	return 0;
}

static int uninit_ext2_file() {
	return 0;
};

static size_t read_ext2_file(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
 	}

	return 0;
}

static size_t write_ext2_file(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
 	}

	return 0;
}

static int stat_ext2_file(struct ARC_Resource *res, char *filename, struct stat *stat) {
	(void)filename;
	if (res == NULL || stat == NULL) {
		return -1;
	}

	struct driver_state *state = res->driver_state;

//	stat->st_atim.tv_sec = inode->last_access;
//	stat->st_mtim.tv_sec = inode->last_mod;
//	stat->st_ctim.tv_sec = inode->creation;
//	stat->st_size = inode->size_low;
//	stat->st_blocks = inode->sectors_used;
//	stat->st_mode = inode->type_perms;

	return 0;
}

ARC_REGISTER_DRIVER(0, ext2, super) = {
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
