/**
 * @file initramfs.c
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
 * CPIO file driver for the initramfs image.
*/
#include <abi-bits/errno.h>
#include <lib/atomics.h>
#include <mm/allocator.h>
#include <global.h>
#include <lib/util.h>
#include <sys/stat.h>
#include <abi-bits/seek-whence.h>

#define ARC_NAME_OFFSET (sizeof(struct ARC_HeaderCPIO))
#define ARC_NAME_SIZE(header) (header->namesize + (header->namesize & 1))
#define ARC_DATA_OFFSET(header) (ARC_NAME_OFFSET + ARC_NAME_SIZE(header))
#define ARC_DATA_SIZE(header) (((header->filesize[0] << 16) | header->filesize[1]) + (((header->filesize[0] << 16) | header->filesize[1]) & 1))

struct ARC_HeaderCPIO {
	uint16_t magic;
	uint16_t device;
	uint16_t inode;
	uint16_t mode;
	uint16_t uid;
	uint16_t gid;
	uint16_t nlink;
	uint16_t rdev;
	uint16_t mod_time[2];
	uint16_t namesize;
	uint16_t filesize[2];
}__attribute__((packed));

struct internal_driver_state {
	struct ARC_Resource *resource;
	void *base;
};

static int initramfs_internal_stat(struct ARC_HeaderCPIO *header, struct stat *stat) {
	stat->st_uid = header->uid;
	stat->st_gid = header->gid;
	stat->st_mode = header->mode;
	stat->st_dev = header->device;
	stat->st_ino = header->inode;
	stat->st_nlink = header->nlink;
	stat->st_rdev = header->rdev;
	stat->st_size = ARC_DATA_SIZE(header);
	stat->st_mtim.tv_nsec = 0;
	stat->st_mtim.tv_sec = (header->mod_time[0] << 16) | header->mod_time[1];

	return 0;
}

static int initramfs_empty() {
	return 0;
}

static int initramfs_init(struct ARC_Resource *res, void *args) {
	struct internal_driver_state *state = (struct internal_driver_state *)alloc(sizeof(*state));

	if (state == NULL) {
		return -1;
	}

	state->base = args;
	state->resource = res;
	res->driver_state = state;

	return 0;
}

static int initramfs_uninit(struct ARC_Resource *res) {
	free(res->driver_state);

	return 0;
}

static int initramfs_open(struct ARC_File *file, struct ARC_Resource *res, int flags, uint32_t mode) {
	(void)flags;
	(void)mode;

	if (file == NULL) {
		return EINVAL;
	}

	struct internal_driver_state *state = (struct internal_driver_state *)res->driver_state;

	if (state == NULL) {
		return -1;
	}

	initramfs_internal_stat(state->base, &file->node->stat);

	return 0;
}

static int initramfs_read(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (file == NULL || res->driver_state == NULL) {
		return 0;
	}

	struct internal_driver_state *state = (struct internal_driver_state *)res->driver_state;
	void *addfiles = state->base;

	if (addfiles == NULL) {
		return 0;
	}

	struct ARC_HeaderCPIO *header = (struct ARC_HeaderCPIO *)addfiles;

	uint8_t *data = (uint8_t *)(addfiles + ARC_DATA_OFFSET(header));

	// Copy file data to buffer
	for (size_t i = 0; i < size * count; i++) {
		uint8_t value = 0;

		if (i + file->offset < (size_t)file->node->stat.st_size) {
			value = *((uint8_t *)(data + file->offset + i));
		}

		*((uint8_t *)(buffer + i)) = value;
	}

	return count;
}

static int initramfs_write(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	ARC_DEBUG(ERR, "Read only file system, tried to write %s\n", buffer);

	return 0;
}

static int initramfs_seek(struct ARC_File *file, struct ARC_Resource *res) {
	(void)file;
	(void)res;

	// NOTE: This driver function means to serve as a way to notify
	//       the driver that the user is moving the RW head to another
	//       place in the file. For an initramfs this doesn't do much,
	//       but for other drivers it may be useful to update caches here.

	return 0;
}

ARC_REGISTER_DRIVER(0, initramfs_file) = {
	.index = 1,
	.instance_counter = 0,
	.name_format = "cpiof%d",
	.init = initramfs_init,
	.uninit = initramfs_uninit,
	.open = initramfs_open,
	.close = initramfs_empty,
	.read = initramfs_read,
	.write = initramfs_write,
	.seek = initramfs_seek,
	.rename = initramfs_empty,
};
