/**
 * @file namespace.c
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
 * Driver implementing functions to manage I/O namespaces in an NVM subsystem.
*/
#include <lib/resource.h>
#include <drivers/sysdev/nvme/namespace.h>
#include <drivers/sysdev/nvme/nvme.h>
#include <mm/pmm.h>
#include <global.h>
#include <mm/allocator.h>
#include <lib/util.h>
#include <lib/perms.h>
#include <drivers/dri_defs.h>
#include <lib/partscan/partscan.h>
#include <fs/vfs.h>

#define NAME_FORMAT "/dev/nvme%dn%d"

struct nvme_namespace_driver_state {
	struct ARC_Resource *res;
	struct controller_state *state;
	size_t nsze;
	size_t ncap;
	size_t lba_size;
	size_t meta_size;
	int namespace;
	int ioqpair;
	uint8_t nvm_set;
	uint8_t meta_follows_lba;
};

int init_nvme_namespace(struct ARC_Resource *res, void *args) {
	if (res == NULL || args == NULL) {
		return -1;
	}

	struct nvme_namespace_dri_args *dri_args = (struct nvme_namespace_dri_args *)args;
	struct nvme_namespace_driver_state *state = (struct nvme_namespace_driver_state *)alloc(sizeof(*state));

	if (state == NULL) {
		return -2;
	}

	memset(state, 0, sizeof(*state));

	state->state = dri_args->state;
	state->namespace = dri_args->namespace;
	state->res = res;
	res->driver_state = state;

	uint8_t *data = (uint8_t *)pmm_alloc_page();
	// NOTE: Assuming NVM Command Set
	struct qs_entry cmd = {
	        .cdw0.opcode = 0x6,
		.prp.entry1 = ARC_HHDM_TO_PHYS(data),
		.cdw10 = 0x0,
		.cdw11 = (dri_args->command_set & 0xFF) << 24,
		.nsid = state->namespace,
        };

	nvme_submit_command(state->state, ADMIN_QUEUE, &cmd);
	nvme_poll_completion(state->state, &cmd, NULL);

	// Get size of LBAs and metas
	uint8_t format_idx = MASKED_READ(data[26], 0, 0xF) | (MASKED_READ(data[26], 5, 0b11) << 4);
	state->meta_follows_lba = MASKED_READ(data[26], 4, 1);

	uint32_t lbaf = *(uint32_t *)&data[128 + format_idx];
	uint8_t lba_exp = MASKED_READ(lbaf, 16, 0xFF);

	state->lba_size = 1;
	while (lba_exp > 0) {
		state->lba_size <<= 1;
		lba_exp--;
	}

	state->meta_size = MASKED_READ(lbaf, 0, 0xFFFF);

	// Set some base information
	state->nvm_set = data[100];
	state->nsze = *(uint64_t *)data;
	state->ncap = *(uint64_t *)&data[8];

	// TODO: The following three commands aren't yet used
	cmd.cdw10 = 0x5;
	nvme_submit_command(state->state, ADMIN_QUEUE, &cmd);
	nvme_poll_completion(state->state, &cmd, NULL);

	cmd.cdw10 = 0x6;
	nvme_submit_command(state->state, ADMIN_QUEUE, &cmd);
	nvme_poll_completion(state->state, &cmd, NULL);

	// TODO: Figure out why this returns 0x4002
	//cmd.cdw10 = 0x8;
	//cmd.cdw11 = 0;
	//nvme_submit_command(state->state, ADMIN_QUEUE, &cmd);
	//nvme_poll_completion(state->state, &cmd, NULL);

	void *qpairs = pmm_alloc(PAGE_SIZE * 2);
	uintptr_t sub = (uintptr_t)qpairs;
	uintptr_t comp = sub + 0x1000;

	struct qpair_list_entry *qpair = nvme_create_qpair(state->state, sub, PAGE_SIZE / sizeof(struct qs_entry), comp, PAGE_SIZE / sizeof(struct qc_entry));

	if (qpair != NULL) {
		nvme_create_io_qpair(state->state, qpair, state->nvm_set, 0);
	} else {
		// Use an existing qpair
		// TODO: Come up with a way to evenly distribute namespaces amongst
		//       IO qpairs if none can be created
	}

	char path[64] = { 0 };
	sprintf(path, NAME_FORMAT, dri_args->state->controller_id, dri_args->namespace);

	struct ARC_VFSNodeInfo info = {
	        .type = ARC_VFS_N_DEV,
		.mode = ARC_STD_PERM,
		.resource_overwrite = res,
        };
	vfs_create(path, &info);

	partscan_enumerate_partitions(path);

	return 0;
}

int uninit_nvme_namespace() {
	return 0;
};

static size_t read_nvme_namespace(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
 	}

	struct nvme_namespace_driver_state *state = res->driver_state;

	// TODO: Extend the size of this data buffer, possibly change
	//       it such that there is a cache allocated in the initialization
	//       so that a buffer does not have to be allocated on the fly
	uint8_t *data = pmm_alloc_page();
	void *meta = pmm_alloc_page();
	size_t read = 0;

	while (read < size * count) {
		uint32_t lba = (file->offset + read) / state->lba_size;
		size_t jank = (file->offset + read) - (lba * state->lba_size);

		struct qs_entry cmd = {
	                .cdw0.opcode = 0x2,
			.prp.entry1 = ARC_HHDM_TO_PHYS(data),
			.mptr = ARC_HHDM_TO_PHYS(meta),
			.cdw12 = (PAGE_SIZE / state->lba_size) - 1,
			.cdw10 = lba,
			.nsid = state->namespace
                };

		nvme_submit_command(state->state, state->ioqpair, &cmd);
		nvme_poll_completion(state->state, &cmd, NULL);

		size_t copy_size = min(PAGE_SIZE - jank, size * count - read);

		memcpy(buffer + read, data + jank, copy_size);
		read += copy_size;
	}

	pmm_free_page(meta);
	pmm_free_page(data);

	return (size * count);
}

static size_t write_nvme_namespace(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
 	}

	struct nvme_namespace_driver_state *state = res->driver_state;

	// TODO: Extend the size of this data buffer, possibly change
	//       it such that there is a cache allocated in the initialization
	//       so that a buffer does not have to be allocated on the fly
	uint8_t *data = pmm_alloc_page();
	void *meta = pmm_alloc_page();
	size_t written = 0;

	while (written < size * count) {
		size_t copy_size = min(PAGE_SIZE, size * count - written);
		uint32_t lba = (file->offset + written) / state->lba_size;
		size_t jank = (file->offset + written) - (lba * state->lba_size);

		struct qs_entry cmd = {
	                .cdw0.opcode = 0x2,
			.prp.entry1 = ARC_HHDM_TO_PHYS(data),
			.mptr = ARC_HHDM_TO_PHYS(meta),
			.cdw12 = (PAGE_SIZE / state->lba_size) - 1,
			.cdw10 = lba,
			.nsid = state->namespace
                };

		// TODO: Could there be a way to cache the "off-cuts" of the read function
		//       to use here, such that the drive does not have to be re-read in the
		//       event that an lba is not fully filled or begins writing at an offset
		//       not aligned to state->lba_size?
		if (copy_size < state->lba_size || jank > 0) {
			nvme_submit_command(state->state, state->ioqpair, &cmd);
			nvme_poll_completion(state->state, &cmd, NULL);
		}

		if (copy_size + jank > PAGE_SIZE) {
			copy_size -= jank;
		}

		memcpy(data + jank, buffer + written, copy_size);

		cmd.cdw0.opcode = 0x1;
		nvme_submit_command(state->state, state->ioqpair, &cmd);
		nvme_poll_completion(state->state, &cmd, NULL);

		written += copy_size;
	}

	pmm_free_page(meta);
	pmm_free_page(data);

	return (size * count);
}

static int stat_nvme_namespace(struct ARC_Resource *res, char *filename, struct stat *stat) {
	(void)filename;

	if (res == NULL || stat == NULL) {
		return -1;
	}

	struct nvme_namespace_driver_state *state = (struct nvme_namespace_driver_state *)res->driver_state;

	stat->st_blksize = state->lba_size;
	stat->st_blocks = state->nsze;
	stat->st_size = state->lba_size * state->nsze;

	return 0;
}

ARC_REGISTER_DRIVER(3, nvme_namespace,) = {
        .init = init_nvme_namespace,
	.uninit = uninit_nvme_namespace,
	.read = read_nvme_namespace,
	.write = write_nvme_namespace,
	.seek = dridefs_int_func_empty,
	.rename = dridefs_int_func_empty,
	.stat = stat_nvme_namespace,
};

#undef NAME_FORMAT
