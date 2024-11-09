/**
 * @file namespace.c
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
 * Driver implementing functions to manage I/O namespaces in an NVM subsystem.
*/
#include <lib/resource.h>
#include <drivers/dev/nvme/namespace.h>
#include <drivers/dev/nvme/nvme.h>
#include <mm/pmm.h>
#include <global.h>
#include <mm/allocator.h>

struct nvme_namespace_driver_state {
	struct ARC_Resource *res;
	struct controller_state *state;
	size_t nsze;
	size_t ncap;
	int namespace;
	int ioqpair;
	uint8_t nvm_set;
};

int empty_nvme_namespace() {
	return 0;
}

int init_nvme_namespace(struct ARC_Resource *res, void *args) {
	if (res == NULL || args == NULL) {
		return -1;
	}

	struct nvme_namespace_dri_args *dri_args = (struct nvme_namespace_dri_args *)args;
	struct nvme_namespace_driver_state *state = (struct nvme_namespace_driver_state *)alloc(sizeof(*state));

	state->state = dri_args->state;
	state->namespace = dri_args->namespace;
	state->res = res;
	res->driver_state = state;

	uint8_t *data = (uint8_t *)pmm_alloc();
	// NOTE: Assuming NVM Command Set
	struct qs_entry cmd = {
	        .cdw0.opcode = 0x6,
		.prp.entry1 = ARC_HHDM_TO_PHYS(data),
		.cdw10 = 0x0,
		.cdw11 = (dri_args->command_set & 0xFF) << 24,
		.nsid = state->namespace,
        };

	nvme_submit_command(state->state, ADMIN_QUEUE, &cmd);
	printf("Status: %x\n", nvme_poll_completion(state->state, &cmd, NULL));

	state->nvm_set = data[100];
	state->nsze = *(uint64_t *)data;
	state->ncap = *(uint64_t *)&data[8];

	cmd.cdw10 = 0x5;
	nvme_submit_command(state->state, ADMIN_QUEUE, &cmd);
	printf("Status: %x\n", nvme_poll_completion(state->state, &cmd, NULL));

	cmd.cdw10 = 0x6;
	nvme_submit_command(state->state, ADMIN_QUEUE, &cmd);
	printf("Status: %x\n", nvme_poll_completion(state->state, &cmd, NULL));

	cmd.cdw10 = 0x8;
	nvme_submit_command(state->state, ADMIN_QUEUE, &cmd);
	printf("Status: %x\n", nvme_poll_completion(state->state, &cmd, NULL));

	void *qpairs = pmm_contig_alloc(2);
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

	return 0;
}

int uninit_nvme_namespace() {
	return 0;
};

int read_nvme_namespace(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
	}

	struct nvme_namespace_driver_state *state = res->driver_state;

	void *meta = pmm_alloc();

	struct qs_entry cmd = {
	        .cdw0.opcode = 0x2,
		.prp.entry1 = ARC_HHDM_TO_PHYS(buffer),
		.mptr = ARC_HHDM_TO_PHYS(meta),
		.cdw12 = 0,
		.cdw10 = 2,
		.nsid = state->namespace
        };

	nvme_submit_command(state->state, state->ioqpair, &cmd);
	printf("Read status: %x\n", nvme_poll_completion(state->state, &cmd, NULL));

	return 1;
}

int write_nvme_namespace(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	(void)buffer;
	(void)size;
	(void)count;
	(void)file;
	(void)res;

	return 0;
}

ARC_REGISTER_DRIVER(3, nvme_namespace_driver) = {
        .index = 1,
	.instance_counter = 0,
	.name_format = "%sn%d",
        .init = init_nvme_namespace,
	.uninit = uninit_nvme_namespace,
	.read = read_nvme_namespace,
	.write = write_nvme_namespace,
	.seek = empty_nvme_namespace,
	.rename = empty_nvme_namespace,
	.open = empty_nvme_namespace,
	.close = empty_nvme_namespace,
};
