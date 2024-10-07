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
 * CPIO superblock driver for the initramfs image.
*/
#include <lib/atomics.h>
#include <lib/perms.h>
#include <fs/vfs.h>
#include <mm/allocator.h>
#include <global.h>
#include <time.h>
#include <lib/util.h>
#include <sys/stat.h>

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
	void *initramfs_base;
};

static void *initramfs_find_file(void *fs, char *filename) {
	if (fs == NULL || filename == NULL) {
		ARC_DEBUG(ERR, "Either fs %p or filename %p is NULL\n", fs, filename);
		return NULL;
	}

	struct ARC_HeaderCPIO *header = (struct ARC_HeaderCPIO *)fs;
	uint64_t offset = 0;

	while (header->magic == 0070707) {
		char *name = ((char *)header) + ARC_NAME_OFFSET;

		if (strncmp(name, filename, strlen(filename)) != 0) {
			goto next;
		}

		ARC_DEBUG(INFO, "Found file \"%s\"\n", name);

		return (void *)header;

		next:;
		offset += ARC_DATA_OFFSET(header) + ARC_DATA_SIZE(header);
		header = (struct ARC_HeaderCPIO *)(fs + offset);
	}

	ARC_DEBUG(ERR, "Could not find file \"%s\"\n", filename);

	return NULL;
}

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

	state->initramfs_base = args;
	state->resource = res;
	res->driver_state = state;

	return 0;
}

static int initramfs_uninit(struct ARC_Resource *res) {
	mutex_lock(&res->dri_state_mutex);
	free(res->driver_state);

	return 0;
}

static int initramfs_stat(struct ARC_Resource *res, char *filename, struct stat *stat) {
	if (res == NULL || filename == NULL || stat == NULL) {
		return 1;
	}

	mutex_lock(&res->dri_state_mutex);

	struct internal_driver_state *state = (struct internal_driver_state *)res->driver_state;
	struct ARC_HeaderCPIO *header = initramfs_find_file(state->initramfs_base, filename);

	mutex_unlock(&res->dri_state_mutex);

	if (header == NULL) {
		return 1;
	}

	return initramfs_internal_stat(header, stat);
}

static void *initramfs_locate(struct ARC_Resource *res, char *filename) {
	if (res == NULL || filename == NULL) {
		return NULL;
	}

	mutex_lock(&res->dri_state_mutex);

	struct internal_driver_state *state = (struct internal_driver_state *)res->driver_state;
	void *ret = initramfs_find_file(state->initramfs_base, filename);

	mutex_unlock(&res->dri_state_mutex);

	return ret;
}

struct ARC_SuperDriverDef initramfs_super_spec = {
	.create = initramfs_empty,
	.remove = initramfs_empty,
	.link = initramfs_empty,
	.rename = initramfs_empty,
	.stat = initramfs_stat,
	.locate = initramfs_locate,
};

ARC_REGISTER_DRIVER(0, initramfs_super) = {
	.index = 0,
	.instance_counter = 0,
	.name_format = "cpiofs%d",
	.init = initramfs_init,
	.uninit = initramfs_uninit,
	.open = initramfs_empty,
	.close = initramfs_empty,
	.read = initramfs_empty,
	.write = initramfs_empty,
	.seek = initramfs_empty,
	.rename = initramfs_empty,
	.identifer = ARC_DRIVER_IDEN_SUPER,
	.driver = (void *)&initramfs_super_spec,
};

