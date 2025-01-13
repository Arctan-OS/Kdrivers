/**
 * @file util.c
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
*/
#include <drivers/sysfs/ext2/util.h>
#include <mm/allocator.h>
#include <lib/util.h>
#include <fs/vfs.h>

// TODO: For the below three functions revise naming
static uint32_t ext2_load_block(uint32_t *block,
				uint32_t (*create_callback)(void *, uint32_t inode), uint32_t inode, void *create_arg) {
	if (*block != 0) {
		return *block;
	}

	if (create_callback == NULL) {
		return 0;
	}

	return create_callback(create_arg, inode);
}

static int ext2_load_read_block(struct ext2_basic_driver_state *state, uint32_t *block, uint32_t **out,
				uint32_t (*create_callback)(void *, uint32_t inode), uint32_t inode, void *create_arg) {
	if (state == NULL) {
		return -1;
	}

	if (*out == NULL && (*out = (uint32_t *)alloc(state->block_size)) == NULL) {
		return -2;
	}

	uint32_t _block = ext2_load_block(block, create_callback, inode, create_arg);

	if (_block != 0) {
		vfs_seek(state->partition, _block * state->block_size, SEEK_SET);
		vfs_read(*out, 1, state->block_size, state->partition);
		return 0;
	}

	return -3;
}

// TODO: Could it be possible to break this up into further components
//       such that it will also allow for the implementation of deletions
//       or should it be kept as is and the delete function given its own
//       way to traverse the blocks and tables?
static size_t ext2_traverse_blocks(struct ext2_basic_driver_state *state, uint64_t offset, size_t size,
				   size_t (*do_callback)(struct ext2_basic_driver_state *, uint32_t block, uint64_t traversed, uint64_t jank, void *), void *do_arg,
				   uint32_t (*create_callback)(void *, uint32_t inode), void *create_arg) {
	if (state == NULL || size == 0) {
		ARC_DEBUG(ERR, "Failed to traverse blocks improper parameters (%p %d)\n", state, size);
		return 0;
	}

	uint64_t traversed = 0;
	uint64_t ptr_count = state->block_size / 4;

	uint32_t *sibp = NULL;
	uint64_t last_doubly = 0;
	uint32_t *dibp = NULL;
	uint64_t last_triply = 0;
	uint32_t *tibp = NULL;

	while (traversed < size) {
		uint64_t base_blk_idx = (traversed + offset) / state->block_size;
		uint64_t jank = (traversed + offset) - (base_blk_idx * state->block_size);

		if (base_blk_idx < 12) {
			uint32_t block = ext2_load_block(&state->node->dbp[base_blk_idx], create_callback, state->inode, create_arg);

			if (block == 0) {
				ARC_DEBUG(ERR, "Failed to get next block (index: %lu)\n", base_blk_idx);
				goto exit;
			}

			traversed += do_callback(state, block, traversed, jank, do_arg);
		} else {
			uint64_t singly_idx = base_blk_idx - 12;
			uint64_t doubly_idx = singly_idx / state->block_size;
			uint64_t triply_idx = (doubly_idx / state->block_size);
			singly_idx %= ptr_count;

			if ((doubly_idx | triply_idx) == 0) {
				if (sibp == NULL && ext2_load_read_block(state, &state->node->sibp, &sibp, create_callback, state->inode, create_arg) != 0) {
					ARC_DEBUG(ERR, "Failed to load and read node->sibp (index: %lu)", base_blk_idx);
					goto exit;
				}

				goto skip_dibp;
			} else if (doubly_idx >= ptr_count + 1) {
				if (tibp == NULL && ext2_load_read_block(state, &state->node->tibp, &tibp, create_callback, state->inode, create_arg) != 0) {
					ARC_DEBUG(ERR, "Failed to load and read node->tibp (index: %lu)", base_blk_idx);
					goto exit;
				}

				uint32_t block = ext2_load_block(&tibp[(triply_idx - 1) % ptr_count], create_callback, state->inode, create_arg);

				if (block == 0) {
					ARC_DEBUG(ERR, "Failed to load and read node->tibp[%lu] (index: %lu)", (triply_idx - 1) % ptr_count, base_blk_idx);
					goto exit;
				}

				if (last_triply != 0 && block != last_triply) {
					vfs_seek(state->partition, last_triply * state->block_size, SEEK_SET);
					vfs_write(dibp, 1, state->block_size, state->partition);
				} else if (block == last_triply) {
					goto do_dibp;
				}

				ext2_load_read_block(state, &tibp[(triply_idx - 1) % ptr_count], &dibp, create_callback, state->inode, create_arg);

				last_triply = block;
			} else {
				// Assume 1 <= doubly_idx <= 1024 and triply_idx = 0
				if (dibp == NULL && ext2_load_read_block(state, &state->node->dibp, &dibp, create_callback, state->inode, create_arg) != 0) {
					ARC_DEBUG(ERR, "Failed to load and read node->dibp (index: %lu)", base_blk_idx);
					goto exit;
				}
			}

			do_dibp:;

			uint32_t block = ext2_load_block(&dibp[(doubly_idx - 1) % ptr_count], create_callback, state->inode, create_arg);

			if (block == 0) {
				ARC_DEBUG(ERR, "Failed to load and read dibp[%lu] (index: %lu)", (doubly_idx - 1) % ptr_count, base_blk_idx);
				goto exit;
			}

			if (last_doubly != 0 && block != last_doubly) {
				vfs_seek(state->partition, last_doubly * state->block_size, SEEK_SET);
				vfs_write(sibp, 1, state->block_size, state->partition);
			} else if (block == last_doubly) {
				goto skip_dibp;
			}

			ext2_load_read_block(state, &dibp[(doubly_idx - 1) % ptr_count], &sibp, create_callback, state->inode, create_arg);

			last_doubly = block;

			skip_dibp:;

			uint32_t resolve_block = ext2_load_block(&sibp[singly_idx], create_callback, state->inode, create_arg);

			if (resolve_block == 0) {
				ARC_DEBUG(ERR, "Failed to load and read sibp[%lu] (index: %lu)", singly_idx, base_blk_idx);
				goto exit;
			}

			traversed += do_callback(state, resolve_block, traversed, jank, do_arg);
		}
	}

	exit:;
	if (tibp != NULL) {
		vfs_seek(state->partition, state->node->tibp * state->block_size, SEEK_SET);
		vfs_write(tibp, 1, state->block_size, state->partition);
		free(tibp);
	}

	if (dibp != NULL) {
		if (last_triply == 0) {
			vfs_seek(state->partition, state->node->dibp * state->block_size, SEEK_SET);
		} else {
			vfs_seek(state->partition, last_triply * state->block_size, SEEK_SET);
		}

		vfs_write(dibp, 1, state->block_size, state->partition);

		free(dibp);
	}

	if (sibp != NULL) {
		if (last_doubly == 0) {
			vfs_seek(state->partition, state->node->sibp * state->block_size, SEEK_SET);
		} else {
			vfs_seek(state->partition, last_doubly * state->block_size, SEEK_SET);
		}

		vfs_write(sibp, 1, state->block_size, state->partition);

		free(sibp);
	}

	return traversed;
}

struct internal_callback_args {
	size_t size;
	uint8_t *buffer;
};

static size_t ext2_read_callback(struct ext2_basic_driver_state *state, uint32_t block, uint64_t traversed, uint64_t jank, void *args) {
	if (args == NULL) {
		ARC_DEBUG(ERR, "Read callback failed, improper parameters (%p)", args);
		return 0;
	}

	struct internal_callback_args *cast_args = args;

	vfs_seek(state->partition, block * state->block_size + jank, SEEK_SET);
	size_t copy_size = min(state->block_size - jank, cast_args->size - traversed);

	return vfs_read(cast_args->buffer + traversed, 1, copy_size, state->partition);
}

size_t ext2_read_inode_data(struct ext2_basic_driver_state *state, uint8_t *buffer, uint64_t offset, size_t size) {
	if (state == NULL || buffer == NULL || size == 0) {
		ARC_DEBUG(ERR, "Failed to read inode data, improper parameters (%p %p %lu)\n", state, buffer, size);
		return 0;
	}

	struct internal_callback_args args = {
	        .buffer = buffer,
		.size = size,
        };

	return ext2_traverse_blocks(state, offset, size, ext2_read_callback, &args, NULL, NULL);
}

static size_t ext2_write_callback(struct ext2_basic_driver_state *state, uint32_t block, uint64_t traversed, uint64_t jank, void *args) {
	if (args == NULL) {
		ARC_DEBUG(ERR, "Write callback failed, improper parameters (%p)\n", args);
		return 0;
	}

	struct internal_callback_args *cast_args = args;

	vfs_seek(state->partition, block * state->block_size + jank, SEEK_SET);
	size_t copy_size = min(state->block_size - jank, cast_args->size - traversed);
	return vfs_write(cast_args->buffer + traversed, 1, copy_size, state->partition);
}

static uint32_t ext2_create_callback(void *args, uint32_t inode) {
	if (args == NULL || inode == 0) {
		ARC_DEBUG(ERR, "Create callback failed, improper parameters (%p %lu)", args, inode);
		return 0;
	}

	struct ext2_super_driver_state *state = args;
	(void)state;

	ARC_DEBUG(ERR, "EXT2 Block creation is unimplemented\n");

	// TODO: Call control function in super driver to allocate new block

	return 0;
}

size_t ext2_write_inode_data(struct ext2_node_driver_state *state, uint8_t *buffer, uint64_t offset, size_t size) {
	if (state == NULL || buffer == NULL || size == 0 || MASKED_READ(state->basic.attributes, 1, 1) == 0) {
		ARC_DEBUG(ERR, "Failed to write inode data, improper parameters (%p %p %lu %s)\n", state, buffer, size, MASKED_READ(state->basic.attributes, 1, 1) ? "Write Enabled" : "Write Disabled");
		return 0;
	}

	struct internal_callback_args args = {
	        .buffer = buffer,
		.size = size,
        };

	return ext2_traverse_blocks(&state->basic, offset, size, ext2_write_callback, &args, ext2_create_callback, state->super);
}

struct internal_get_inode_in_dir_arg {
	char *target;
	uint64_t inode_number;
};

static int callback_get_inode_in_dir(struct ext2_dir_ent *ent, void *arg) {
	if (ent == NULL || arg == NULL) {
		ARC_DEBUG(ERR, "Get inode in dir callback failed, improper parameters (%p %p)\n", ent, arg);
		return 1;
	}

	struct internal_get_inode_in_dir_arg *cast_arg = arg;

	// TODO: What if upper name len is also used?
	if (strncmp(ent->name, cast_arg->target, ent->lower_name_len) != 0) {
		return 1;
	}

	cast_arg->inode_number = ent->inode;
	return 0;
}

uint64_t ext2_get_inode_in_dir(struct ext2_basic_driver_state *dir, char *filename) {
	if (dir == NULL || filename == NULL) {
		ARC_DEBUG(ERR, "Get inode in dir failed, improper parameters (%p %p)\n", dir, filename);
		return 0;
	}

	struct internal_get_inode_in_dir_arg arg = {
	        .target = filename
        };

	ext2_list_directory(dir, callback_get_inode_in_dir, &arg);

	return arg.inode_number;
}

void ext2_list_directory(struct ext2_basic_driver_state *dir, int (*callback)(struct ext2_dir_ent *, void *arg), void *arg) {
	if (dir == NULL || callback == NULL) {
		ARC_DEBUG(ERR, "Failed to list directory, improper parameters (%p %p)\n", dir, callback);
		return;
	}

	uint8_t *block = alloc(dir->block_size);
	uint64_t offset = 0;
	size_t delta = 0;

	while ((delta = ext2_read_inode_data(dir, block, offset, dir->block_size)) != 0) {
		for (size_t i = 0; i < dir->block_size;) {
			struct ext2_dir_ent *ent = (struct ext2_dir_ent *)(block + i);

			i += ent->total_size;
			if (ent->total_size == 0) {
				break;
			}

			if (callback(ent, arg) == 0) {
				free(block);
				return;
			}
		}

		offset += delta;
	}

	free(block);
}
