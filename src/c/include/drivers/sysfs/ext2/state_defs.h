/**
 * @file state_defs.h
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
#ifndef ARC_DRIVERS_SYSFS_EXT2_STATE_DEFS_H
#define ARC_DRIVERS_SYSFS_EXT2_STATE_DEFS_H

#include "drivers/sysfs/ext2/ext2.h"

#include <stdint.h>
#include <global.h>

struct ext2_basic_driver_state {
	struct ARC_File *partition;
	struct ext2_inode *node;
	uint64_t attributes; // Bit | Description
			     // 0   | 1: Enable caching
			     // 1   | 1: Write enabled
			     // 2   | 1: 64-bit inode sizes
	size_t block_size;
	uint32_t inode;
};

struct ext2_super_driver_state {
	char *parition_path;
	struct ext2_block_group_desc *descriptor_table;
	uint64_t descriptor_count;
	struct ext2_basic_driver_state basic;
	struct ext2_super_block super;
};

struct ext2_node_driver_state {
	struct ext2_super_driver_state *super;
	struct ext2_basic_driver_state basic;
};

#endif
