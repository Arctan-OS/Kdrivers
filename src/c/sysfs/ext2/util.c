#include <drivers/sysfs/ext2/util.h>
#include <mm/allocator.h>
#include <lib/util.h>
#include <fs/vfs.h>

size_t ext2_read_inode_data(struct ext2_basic_driver_state *state, uint8_t *buffer, uint64_t offset, size_t size) {
	if (state == NULL || buffer == NULL || size == 0) {
		return 0;

	}

	uint64_t starting_block_ptr = offset / state->block_size;
	uint64_t block_ptr_count = ALIGN(size, state->block_size) / state->block_size;
	uint64_t read = 0;

	if (starting_block_ptr < 12) {
		uint64_t ending_block = min(starting_block_ptr + block_ptr_count, 12);

		for (uint64_t i = starting_block_ptr; i < ending_block; i++) {
			size_t copy_size = min(state->block_size, size - read);

			vfs_seek(state->partition, offset + read + (state->node->dbp[i] * state->block_size), SEEK_SET);
			if (vfs_read(buffer + read, copy_size, 1, state->partition) != copy_size) {
				break;
			}

			read += copy_size;
		}

		if (read == size) {
			return read;
		}

		block_ptr_count -= 12 - starting_block_ptr;
		starting_block_ptr = 12;
	}

	starting_block_ptr -= 12;

	uint64_t ptr_count = (state->block_size / 4);

	uint8_t *triply_ind = alloc(state->block_size);

	if (triply_ind == NULL) {
		return 0;
	}

	vfs_seek(state->partition, state->node->tibp * state->block_size, SEEK_SET);
	vfs_read(triply_ind, state->block_size, 1, state->partition);

	uint8_t *doubly_ind = alloc(state->block_size);

	if (doubly_ind == NULL) {
		free(triply_ind);
		return 0;
	}

	vfs_seek(state->partition, state->node->sibp * state->block_size, SEEK_SET);
	vfs_read(doubly_ind, state->block_size, 1, state->partition);

	uint8_t *singly_ind = alloc(state->block_size);

	if (singly_ind == NULL) {
		free(triply_ind);
		free(doubly_ind);
		return 0;
	}

	vfs_seek(state->partition, state->node->dibp * state->block_size, SEEK_SET);
	vfs_read(singly_ind, state->block_size, 1, state->partition);

	while (read < size) {
		size_t copy_size = min(state->block_size, size - read);
		uint64_t singly_idx = starting_block_ptr % ptr_count;
		uint64_t doubly_idx = (starting_block_ptr / ptr_count) % ptr_count;
		uint64_t triply_idx = (starting_block_ptr / (ptr_count*ptr_count)) % ptr_count;

		if (doubly_idx > 1) {
			vfs_seek(state->partition, triply_ind[triply_idx] * state->block_size, SEEK_SET);
			if (vfs_read(doubly_ind, state->block_size, 1, state->partition) != state->block_size) {
				break;
			}
		}

		if (doubly_idx >= 1) {
			vfs_seek(state->partition, doubly_ind[doubly_idx] * state->block_size, SEEK_SET);
			if (vfs_read(singly_ind, state->block_size, 1, state->partition) != state->block_size) {
				break;
			}
		}

		vfs_seek(state->partition, singly_ind[singly_idx] * state->block_size, SEEK_SET);
		if (vfs_read(buffer, copy_size, 1, state->partition) != copy_size) {
			break;
		}

		read += copy_size;
	}

	if (triply_ind != NULL) {
		free(triply_ind);
	}

	if (doubly_ind != NULL) {
		free(doubly_ind);
	}

	if (singly_ind != NULL) {
		free(singly_ind);
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
