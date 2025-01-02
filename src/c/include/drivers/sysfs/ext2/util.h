/**
 * @file super.h
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
*/
#ifndef ARC_DRIVERS_SYSFS_EXT2_UTIL_H
#define ARC_DRIVERS_SYSFS_EXT2_UTIL_H

#include <stdint.h>
#include <global.h>

struct ext2_inode {
	uint16_t type_perms;
	uint16_t uid;
	uint32_t size_low;
	uint32_t last_access;
	uint32_t creation;
	uint32_t last_mod;
	uint32_t deletion;
	uint16_t gid;
	uint16_t hard_link_count;
	uint32_t sectors_used;
	uint32_t flags;
	uint32_t os_specific0;
	uint32_t dbp[12];
	uint32_t sibp;
	uint32_t dibp;
	uint32_t tibp;
	uint32_t gen_number;
	uint32_t ext_acl; // Reserved in ext2 version 0
	uint32_t ext_dynamic; // File: upper 32-bits of file size; Directoy: ACL
	uint32_t frag_block_addr;
	uint8_t os_specific1[12];
}__attribute__((packed));

struct ext2_dir_ent {
	uint32_t inode;
	uint16_t total_size;
	uint8_t lower_name_len;
	union {
		uint8_t type;
		uint8_t upper_name_len;
	} u1;
	char name[];
}__attribute__((packed));

struct ext2_basic_driver_state {
	struct ARC_File *partition;
	struct ext2_inode *node;
	uint64_t attributes; // Bit | Description
			     // 0   | 1: Enable caching
			     // 1   | 1: Write enabled
			     // 2   | 1: 64-bit inode sizes
	size_t block_size;
};

size_t ext2_read_inode_data(struct ext2_basic_driver_state *inode, uint8_t *buffer, uint64_t offset, size_t size);
size_t ext2_write_inode_data(struct ext2_basic_driver_state *inode, uint8_t *buffer, uint64_t offset, size_t size);
uint64_t ext2_get_inode_in_dir(struct ext2_basic_driver_state *dir, char *filename);
void ext2_list_directory(struct ext2_basic_driver_state *dir, int (*callback)(struct ext2_dir_ent *, void *arg), void *arg);

#endif
