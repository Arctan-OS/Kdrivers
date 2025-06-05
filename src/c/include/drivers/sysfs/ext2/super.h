/**
 * @file super.h
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
#ifndef ARC_DRIVERS_SYSFS_EXT2_SUPER_H
#define ARC_DRIVERS_SYSFS_EXT2_SUPER_H

#include <stdint.h>
#include <global.h>
#include <drivers/sysfs/ext2/state_defs.h>

struct ext2_locate_args {
	struct ext2_inode *node;
	struct ext2_super_driver_state *super;
	uint32_t inode;
};

struct ext2_inode *ext2_read_inode(struct ext2_super_driver_state *state, uint64_t inode);

#endif
