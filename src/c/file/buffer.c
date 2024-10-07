/**
 * @file buffer.c
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
 * Driver for RAM files or buffers which are accesible by the VFS.
*/
#include <lib/resource.h>
#include <global.h>
#include <mm/allocator.h>
#include <lib/util.h>
#include <abi-bits/seek-whence.h>
#include <arctan.h>

struct buffer_dri_state {
	size_t size;
	void *buffer;
};

int buffer_empty() {
	return 0;
}

static int buffer_init(struct ARC_Resource *res, void *arg) {
	size_t size = ARC_STD_BUFF_SIZE;

	if (arg != NULL) {
		size = *(size_t *)arg;
	}

	struct buffer_dri_state *state = (struct buffer_dri_state *)alloc(sizeof(*state));

	if (state == NULL) {
		return -1;
	}

	state->buffer = (void *)alloc(size);
	state->size = size;

	res->driver_state = state;

	return 0;
}

static int buffer_uninit(struct ARC_Resource *res) {
	struct buffer_dri_state *state = (struct buffer_dri_state *)res->driver_state;

	if (state == NULL) {
		return 1;
	}

	free(state->buffer);
	free(state);

	return 0;
}

static int buffer_open(struct ARC_File *file, struct ARC_Resource *res, int flags, uint32_t mode) {
	(void)mode;
	(void)flags;

	struct buffer_dri_state *state = (struct buffer_dri_state *)res->driver_state;

	if (state == NULL) {
		return -1;
	}

	file->node->stat.st_size = state->size;

	return 0;
}

static int buffer_read(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL || res->driver_state == NULL) {
		return -1;
	}

	mutex_lock(&res->dri_state_mutex);

	struct buffer_dri_state *state = (struct buffer_dri_state *)res->driver_state;

	size_t wanted = size * count;
	size_t accessible = state->size - file->offset;

	if (accessible == 0) {
		return 0;
		mutex_unlock(&res->dri_state_mutex);
	}

	int64_t delta = wanted - accessible;
	size_t given = delta > 0 ? wanted - delta : wanted;
	given = given > 0 ? given : 0;

	// Do the actual giving
	memcpy(buffer, state->buffer + file->offset, given);

	mutex_unlock(&res->dri_state_mutex);

	return given;
}

static int buffer_write(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL || res->driver_state == NULL) {
		return -1;
	}


	mutex_lock(&res->dri_state_mutex);

	struct buffer_dri_state *state = (struct buffer_dri_state *)res->driver_state;

	size_t wanted = size * count;
	size_t accessible = state->size - file->offset;

	if (accessible == 0) {
		return 0;
		mutex_unlock(&res->dri_state_mutex);
	}

	int64_t delta = wanted - accessible;
	size_t given = delta > 0 ? wanted - delta : wanted;
	given = given > 0 ? given : 0;

	// Do the actual receiving
	memcpy(state->buffer + file->offset, buffer, given);

	mutex_unlock(&res->dri_state_mutex);

	return given;
}

static int buffer_seek(struct ARC_File *file, struct ARC_Resource *res) {
	(void)file;
	(void)res;

	return 0;
}

ARC_REGISTER_DRIVER(0, buffer) = {
        .index = 5,
	.instance_counter = 0,
	.name_format = "buff%d",
	.init = buffer_init,
	.uninit = buffer_uninit,
	.open = buffer_open,
	.read = buffer_read,
	.write = buffer_write,
	.seek = buffer_seek,
	.rename = buffer_empty,
	.close = buffer_empty,
};
