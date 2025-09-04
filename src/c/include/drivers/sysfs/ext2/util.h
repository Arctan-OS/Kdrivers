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
#ifndef ARC_DRIVERS_SYSFS_EXT2_UTIL_H
#define ARC_DRIVERS_SYSFS_EXT2_UTIL_H

#include "drivers/sysfs/ext2/state_defs.h"

size_t ext2_read_inode_data(struct ext2_basic_driver_state *state, uint8_t *buffer, uint64_t offset, size_t size);
size_t ext2_write_inode_data(struct ext2_node_driver_state *state, uint8_t *buffer, uint64_t offset, size_t size); 
uint64_t ext2_get_inode_in_dir(struct ext2_basic_driver_state *dir, char *filename);
void ext2_list_directory(struct ext2_basic_driver_state *dir, int (*callback)(struct ext2_dir_ent *, void *arg), void *arg);

#endif
