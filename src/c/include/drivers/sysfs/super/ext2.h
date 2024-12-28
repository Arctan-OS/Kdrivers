/**
 * @file partition_dummy.h
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
#ifndef ARC_DRIVERS_SYSFS_SUPER_EXT2_H
#define ARC_DRIVERS_SYSFS_SUPER_EXT2_H

#define EXT2_SIG 0xEF53

#include <stdint.h>

struct ext2_super_block {
	uint32_t total_inodes;
	uint32_t total_blocks;
	uint32_t total_resv_blocks; // for super user
	uint32_t total_unallocated_blocks;
	uint32_t total_unallocated_inodes;
	uint32_t superblock;
	uint32_t log2_block_size;
	uint32_t log2_frag_size;
	uint32_t blocks_per_group;
	uint32_t frags_per_group;
	uint32_t inodes_per_group;
	uint32_t last_mount;
	uint32_t last_written;
	uint16_t mount_count;
	uint16_t mounts_per_check;
	uint16_t sig;
	uint16_t state;
	uint16_t err_handle;
	uint16_t ver_min;
	uint32_t last_check;
	uint32_t interval_forced_check;
	uint32_t os_id;
	uint32_t ver_maj;
	uint16_t uid_superuser;
	uint16_t gid_superuser;
	uint32_t first_non_resv_inode;
	uint16_t inode_size;
	uint16_t superblock_group;
	uint32_t opt_features;
	uint32_t required_features;
	uint32_t unsupported_features;
	uint8_t fs_id[16];
	uint8_t vol_name[16];
	uint8_t last_path[64];
	uint32_t compression_algo;
	uint8_t file_pre_alloc_blocks;
	uint8_t dir_pre_alloc_blocks;
	uint16_t resv0;
	uint8_t journal_id[16];
	uint32_t journal_inode;
	uint32_t journal_dev;
	uint32_t orphan_inode_list_head;
}__attribute__((packed));

struct ext2_block_group_desc {
	uint32_t usage_bmp_block; // block address
	uint32_t usage_bmp_inode; // block address
	uint32_t inode_table_start; // block address
	uint32_t unallocated_blocks;
	uint32_t unallocated_inodes;
	uint32_t directory_count;
}__attribute__((packed));


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
	uint32_t dbp0;
	uint32_t dpb1;
	uint32_t dpb2;
	uint32_t dpb3;
	uint32_t dbp4;
	uint32_t dpb5;
	uint32_t dpb6;
	uint32_t dpb7;
	uint32_t dbp8;
	uint32_t dpb9;
	uint32_t dpb10;
	uint32_t dpb11;
	uint32_t sibp;
	uint32_t dibp;
	uint32_t gen_number;
	uint32_t ext_acl; // Reserved in ext2 version 0
	uint32_t ext_dynamic; // File: upper 32-bits of file size; Directoy: ACL
	uint32_t frag_block_addr;
	uint8_t os_specific1[12];
}__attribute__((packed));

#endif
