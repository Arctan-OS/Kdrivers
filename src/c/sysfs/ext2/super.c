/**
 * @file super.c
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
 * Superblock dirvers for the EXT2 filesystem.
*/
#include "abi-bits/seek-whence.h"
#include "drivers/cntrl_defs.h"
#include "drivers/dri_defs.h"
#include "drivers/sysfs/ext2/super.h"
#include "drivers/sysfs/ext2/util.h"
#include "fs/vfs.h"
#include "global.h"
#include "lib/util.h"
#include "mm/allocator.h"

static int ext2_check_super(struct ext2_super_driver_state *state) {
	if (state == NULL) {
		ARC_DEBUG(ERR, "Cannot check NULL state\n");
		return -1;
	}

	if (state->super.state != 1) {
		ARC_DEBUG(ERR, "Filesystem has errors, %s\n", (state->super.err_handle == 2 ? "mounting as read only" : "ignoring"));
		MASKED_WRITE(state->basic.attributes, !(state->super.err_handle == 2), 1, 1);

		if (state->super.err_handle == 3) {
			ARC_DEBUG(ERR, "Filesystem has errors and error handle mechanism is to fail\n");
			return -2;
		}
	}

	char *system_names[] = {
	        "Linux",
		"GNU HURD",
		"MASIX",
		"FreeBSD",
		"Other"
        };

	ARC_DEBUG(INFO, "Filesystem was created by a %s system\n", system_names[state->super.os_id]);

	// Check required features
	if (MASKED_READ(state->super.required_features, 0, 1) == 1) {
		ARC_DEBUG(ERR, "This implementation does not support compressed filesystems\n");
		return -3;
	}

	if (MASKED_READ(state->super.required_features, 1, 1) == 1) {
		ARC_DEBUG(INFO, "Directory entries have a type field\n");
	}

	if (MASKED_READ(state->super.required_features, 2, 1) == 1) {
		ARC_DEBUG(ERR, "This implementation does not support replaying journals\n");
		return -4;
	}

	if (MASKED_READ(state->super.required_features, 3, 1) == 1) {
		ARC_DEBUG(ERR, "This implementation does not support the use of a journal\n");
		return -5;
	}

	// Check features required for writes
	if (MASKED_READ(state->super.write_features, 0, 1) == 1) {
		ARC_DEBUG(ERR, "(TODO) This implementation does not support the use of sparse superblocks and block descriptors, disabling write\n");
		MASKED_WRITE(state->basic.attributes, 0, 1, 1);
	}

	if (MASKED_READ(state->super.write_features, 1, 1) == 1) {
		ARC_DEBUG(INFO, "Filesystem uses 64-bit file sizes\n");
	}

	if (MASKED_READ(state->super.write_features, 2, 1) == 1) {
		ARC_DEBUG(ERR, "This implementation does not support directories with binary trees, disabling write\n");
		MASKED_WRITE(state->basic.attributes, 0, 1, 1);
	}

	return 0;
}

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

	// TODO: In this case check for sparse superblocks
	if (state->super.sig != EXT2_SIG) {
		ARC_DEBUG(ERR, "Signature mismatch\n");

		vfs_close(state->basic.partition);
		free(state);
		return -5;
	}

	// Enable writing, ext2_check_super or post-mount call is able
	// to change this
	MASKED_WRITE(state->basic.attributes, 1, 1, 1);

	if (ext2_check_super(state) != 0) {
		ARC_DEBUG(ERR, "Superblock check failed\n");
		vfs_close(state->basic.partition);
		free(state);
		return -6;
	}

	state->basic.block_size = 1024;
	for (int i = state->super.log2_block_size; i > 0; i--) {
		state->basic.block_size <<= 1;
	}

	size_t block_groups = min((state->super.total_blocks + state->super.blocks_per_group - 1) / state->super.blocks_per_group,
                                  (state->super.total_inodes + state->super.inodes_per_group - 1) / state->super.inodes_per_group);

	state->descriptor_count = block_groups;

	struct ext2_block_group_desc *descriptor_table = alloc(block_groups * sizeof(struct ext2_block_group_desc));

	if (descriptor_table == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate descriptor table\n");
		vfs_close(state->basic.partition);
		free(state);
		return -7;
	}

	vfs_seek(state->basic.partition, (1 + state->super.superblock) * state->basic.block_size, SEEK_SET);
	if (vfs_read(descriptor_table, 1, block_groups * sizeof(struct ext2_block_group_desc), state->basic.partition)
	    != block_groups * sizeof(struct ext2_block_group_desc)) {
		ARC_DEBUG(ERR, "Failed to read in descriptor table\n");
		vfs_close(state->basic.partition);
		free(descriptor_table);
		free(state);
		return -8;
	}

	state->descriptor_table = descriptor_table;
	state->parition_path = strdup(args);
	state->basic.node = ext2_read_inode(state, 2);
	state->basic.inode = 2;
	res->driver_state = state;

	return 0;
}

static int uninit_ext2_super(struct ARC_Resource *res) {
	if (res == NULL) {
		return -1;
	}

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

static uint64_t *ext2_allocate_blocks(struct ext2_super_driver_state *state, uint32_t inode, uint32_t count) {
	if (state == NULL || inode == 0 || count == 0) {
		ARC_DEBUG(ERR, "Improper parameters (%p %u %u)\n", state, inode, count);
		return NULL;
	}

	uint64_t block_group = ((inode - 1) / state->super.inodes_per_group);

	// NOTE: This code will not account for the case where count is greater than any
	//       number of unallocated blocks in any block group. It may be worthwhile to
	//       account for such a case, but it is unlikely given that this function will
	//       most likely be called with count=1, and only called with count>1 when
	//       first creating a file.
	int use_group = -1;
	for (uint64_t i = 0; i < state->descriptor_count; i++) {
		if (state->descriptor_table[(block_group + i) % state->descriptor_count].unallocated_blocks >= count) {
			use_group = (block_group + i) % state->descriptor_count;
			break;
		}
	}

	if (use_group == -1) {
		return NULL;
	}

	uint64_t *ret = (uint64_t *)alloc(count * 8);

	if (ret == NULL) {
		return NULL;
	}

	uint64_t *block_bmp = (uint64_t *)alloc(state->basic.block_size);

	if (block_bmp == NULL) {
		free(ret);
		return NULL;
	}

	vfs_seek(state->basic.partition, state->descriptor_table[use_group].usage_bmp_block * state->basic.block_size, SEEK_SET);
	vfs_read(block_bmp, 1, state->basic.block_size, state->basic.partition);

	int next_ret_idx = 0;
	uint64_t offset;
	uint64_t base_block = ALIGN_UP(state->descriptor_table[use_group].inode_table_start + (((state->basic.block_size * 8) * state->super.inode_size)), state->basic.block_size) / state->basic.block_size;
	for (offset = 0; offset < state->basic.block_size / 8; offset++) {
		uint64_t range_start_block = (base_block + ((offset << 3)));

		if (block_bmp[offset] == 0) {
			uint32_t max = min(count, (uint32_t)64);
			block_bmp[offset] = ~block_bmp[offset];
			block_bmp[offset] >>= max;
			block_bmp[offset] <<= max;
			block_bmp[offset] = ~block_bmp[offset];
			ret[next_ret_idx++] = range_start_block | (uint64_t)(max - 1) << 32;
		} else {
			for (int i = 0; i < 64 && count > 0; i++) {
				int free_idx = __builtin_ffs(~block_bmp[offset]) - 1;
				MASKED_WRITE(block_bmp[offset], 1, free_idx, 1);
				count--;
				ret[next_ret_idx++] = (range_start_block + free_idx) | (uint64_t)1 << 32;
			}
		}
	}

	return ret;
}

static int ext2_delete_inode(struct ext2_super_driver_state *state, uint32_t inode) {
	if (state == NULL || inode == 0) {
		ARC_DEBUG(ERR, "Improper parameters (%p %u)\n", state, inode);
	}

	ARC_DEBUG(ERR, "Unimplemented\n");

	return -1;
}

static int create_ext2_super(struct ARC_Resource *res, char *name, uint32_t mode, int type) {
	if (res == NULL || name == NULL || mode == 0 || type == 0) {
		return -1;
	}

	return 0;
}

static int remove_ext2_super(struct ARC_Resource *res, char *name) {
	if (res == NULL || name == NULL) {
		return -1;
	}

	return 0;
}

static void *control_ext2_super(struct ARC_Resource *res, void *command, size_t len) {
	if (res == NULL || command == NULL || len == 0) {
		return NULL;
	}

	struct ext2_super_driver_state *state = res->driver_state;

	uint8_t cmd_set = *(uint8_t *)(command + len - 1);
	uint8_t cmd_attrs = *(uint8_t *)(command + len - 2);

	switch (cmd_set) {
		case 0x0: {
			// CMD SET 0 - Specific to driver
			// CMD_ATRRS BIT | DESCRIPTION
			//           0   | 1:Allocate block for given inode 0: Delete given inode
			//           7:1 | [0]=1: Number of blocks to allocate - 1, [0]=0: Reserved
			// INODE CMD_ATTRS CMD_SET
			uint32_t inode = *(uint32_t *)command;
			int count = (cmd_attrs >> 1) + 1;

			if (MASKED_READ(cmd_attrs, 0, 1) == 0) {
				return (void *)ext2_allocate_blocks(state, inode, count);
			} else {
				return (void *)(uint64_t)ext2_delete_inode(state, inode);
			}
		}

		case 0x1: {
			// CMD SET 1 - Standard commands
			// CMD_ATTRS BIT | DESCRIPTION
			//           1:0 | (log2(opcode_size)) in bytes
			//
			// DATA OPCODE CMD_ATTRS CMD_SET
			uint8_t opcode_size = 1;
			int log2_size = MASKED_READ(cmd_attrs, 0, 2);

			while (log2_size > 0) {
				opcode_size <<= 1;
				log2_size--;
			}

			uint8_t opcode = *(uint8_t *)(command + len - opcode_size - 3);

			switch (opcode) {
				case CNTRL_OPCODE_ASSOCIATE: {
					// Associate
					break;
				}

				case CNTRL_OPCODE_DISASSOCATE: {
					// Disassociate
					break;
				}
			}

			// OPCODES: 0x0 - Associate
			//            DATA: NODE_NAME INODE
			//          0x1 - Disassociate
			//            DATA: INODE
			//          0x2 - Set property
			//            DATA: NEW_PROPERTIES
		}
	}

	return NULL;
}

struct ext2_inode *ext2_read_inode(struct ext2_super_driver_state *state, uint64_t inode) {
	if (state == NULL || state->descriptor_table == NULL || inode == 0) {
		ARC_DEBUG(ERR, "Failed to read inode, improper parameters (%p %p %lu)\n", state, state->descriptor_table, inode);
		return NULL;
	}

	struct ext2_inode *buffer = alloc(state->super.inode_size);

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

ARC_REGISTER_DRIVER(ARC_DRI_GROUP_FS_SUPER, ext2) = {
        .init = init_ext2_super,
	.uninit = uninit_ext2_super,
	.write = write_ext2_super,
	.read = read_ext2_super,
	.seek = dridefs_int_func_empty,
	.rename = dridefs_int_func_empty,
	.stat = stat_ext2_super,
	.control = control_ext2_super,
	.create = create_ext2_super,
	.remove = remove_ext2_super,
	.locate = locate_ext2_super,
};
