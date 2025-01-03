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

size_t ext2_read_inode_data(struct ext2_basic_driver_state *state, uint8_t *buffer, uint64_t offset, size_t size) {
	if (state == NULL || buffer == NULL || size == 0) {
		return 0;
	}

	size_t read = 0;
	int ptr_count = state->block_size / 4;

	uint32_t *singly_ind = NULL;
	if (state->node->sibp != 0 && (singly_ind = alloc(state->block_size)) == NULL) {
		goto exit;
	}

	uint32_t *doubly_ind = NULL;
	uint64_t last_doubly_idx = 0;
	if (state->node->dibp != 0 && (doubly_ind = alloc(state->block_size)) == NULL) {
		goto exit;
	}

	uint32_t *triply_ind = NULL;
	uint64_t last_triply_idx = 0;
	if (state->node->tibp != 0 && (triply_ind = alloc(state->block_size)) == NULL) {
		goto exit;
	}

	if (singly_ind != NULL) {
		vfs_seek(state->partition, state->node->sibp * state->block_size, SEEK_SET);
		vfs_read(singly_ind, state->block_size, 1, state->partition);
	}

	if (doubly_ind != NULL) {
		vfs_seek(state->partition, state->node->dibp * state->block_size, SEEK_SET);
		vfs_read(doubly_ind, state->block_size, 1, state->partition);
	}

	// NOTE: For some reason if this table were laoded on a read on dpb[11], it would
	//       cause an error. Reads on dpb[10:0] would be fine. May become an issue in
	//       the future if the triply indirect table is also present
	if (triply_ind != NULL) {
		vfs_seek(state->partition, state->node->tibp * state->block_size, SEEK_SET);
		vfs_read(triply_ind, state->block_size, 1, state->partition);
	}

	while (read < size) {
		uint64_t block_idx = (offset + read) / state->block_size;
		uint64_t jank = (offset + read) - (block_idx * state->block_size);
		size_t copy_size = min(state->block_size - jank, size - read);


		if (block_idx < 12) {
			vfs_seek(state->partition, state->node->dbp[block_idx] * state->block_size + jank, SEEK_SET);
			vfs_read(buffer + read, 1, copy_size, state->partition);
		} else {
			uint64_t singly_idx = (block_idx - 12) % ptr_count;
			uint64_t doubly_idx = (block_idx - 12) / ptr_count;
			uint64_t triply_idx = (block_idx - 12) / (ptr_count*ptr_count);

			if (triply_ind != NULL && triply_idx >= 1 && triply_idx != last_triply_idx) {
				vfs_seek(state->partition, triply_ind[(triply_idx - 1) % ptr_count] * state->block_size, SEEK_SET);
				if (vfs_read(doubly_ind, state->block_size, 1, state->partition) != state->block_size) {
					break;
				}
			}

			if (doubly_ind != NULL && doubly_idx >= 1 && doubly_idx != last_doubly_idx) {
				vfs_seek(state->partition, doubly_ind[(doubly_idx - 1) % ptr_count] * state->block_size, SEEK_SET);
				if (vfs_read(singly_ind, state->block_size, 1, state->partition) != state->block_size) {
					break;
				}
			}

			vfs_seek(state->partition, singly_ind[singly_idx] * state->block_size + jank, SEEK_SET);
			if (vfs_read(buffer + read, 1, copy_size, state->partition) != copy_size) {
				break;
			}

			last_doubly_idx = doubly_idx;
			last_triply_idx = triply_idx;
		}

		read += copy_size;
	}

	exit:;
	if (singly_ind != NULL) {
		free(singly_ind);
	}

	if (doubly_ind != NULL) {
		free(doubly_ind);
	}

	if (triply_ind != NULL) {
		free(triply_ind);
	}

	return read;
}

size_t ext2_write_inode_data(struct ext2_basic_driver_state *state, uint8_t *buffer, uint64_t offset, size_t size) {
	if (state == NULL || buffer == NULL || size == 0 || MASKED_READ(state->attributes, 1, 1) == 0) {
		return 0;

	}

	(void)offset;
//	uint64_t starting_block = offset / state->block_size;
//	uint64_t block_count = ALIGN(size, state->block_size) / state->block_size;

	return 0;
}

struct internal_get_inode_in_dir_arg {
	char *target;
	uint64_t inode_number;
};

static int callback_get_inode_in_dir(struct ext2_dir_ent *ent, void *arg) {
	if (ent == NULL || arg == NULL) {
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
