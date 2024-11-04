/**
 * @file nvme.c
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
#include <drivers/dev/nvme/nvme.h>
#include <drivers/dev/nvme/pci.h>
#include <mm/allocator.h>
#include <lib/ringbuffer.h>
#include <lib/util.h>
#include <drivers/dri_defs.h>

struct driver_state {
	struct controller_state *controller_state;
	struct ARC_Resource *res;
};

int nvme_submit_command(struct controller_state *state, int queue, struct qs_entry *cmd) {
	return nvme_pci_submit_command(state, queue, cmd);
}

int nvme_poll_completion(struct controller_state *state, struct qs_entry *cmd, struct qc_entry *ret) {
	return nvme_pci_poll_completion(state, cmd, ret);
}

int empty_nvme() {
	return 0;
}

struct qpair_list_entry *nvme_create_qpair(struct controller_state *state, uintptr_t sub, size_t sub_len, uintptr_t comp, size_t comp_len) {
	if (state == NULL || state->id_bmp == 0 || comp_len == 0 || sub_len == 0) {
		return NULL;
	}

	struct qpair_list_entry *entry = (struct qpair_list_entry *)alloc(sizeof(*entry));

	if (entry == NULL) {
		return NULL;
	}

	mutex_lock(&state->qpair_lock);

	entry->id = __builtin_ffs(state->id_bmp) - 1;
	state->id_bmp &= ~(1 << entry->id);
	entry->next = state->list;
	entry->submission_queue = init_ringbuffer((void *)sub, sub_len, sizeof(struct qs_entry));
	entry->completion_queue = init_ringbuffer((void *)comp, comp_len, sizeof(struct qc_entry));
	entry->phase = 1;
	state->list = entry;

	mutex_unlock(&state->qpair_lock);

	memset((void *)sub, 0, sub_len);
	memset((void *)comp, 0, comp_len);

	return entry;
}

int nvme_delete_qpair(struct qpair_list_entry *qpair) {
	(void)qpair;
	ARC_DEBUG(WARN, "Implement\n");

	return 0;
}

int nvme_delete_all_qpairs(struct controller_state *state) {
	state->id_bmp = -1;
	ARC_DEBUG(WARN, "Implement\n");

	return 0;
}


int init_nvme(struct ARC_Resource *res, void *arg) {
	if (res == NULL || arg == NULL) {
		return -1;
	}
	// NOTE: Assuming that arg is a ARC_PCIHeader

	struct driver_state *dri = (struct driver_state *)alloc(sizeof(*dri));

	if (dri == NULL) {
		return -2;
	}

	struct controller_state *cntrl = (struct controller_state *)alloc(sizeof(*cntrl));

	if (cntrl == NULL) {
		free(dri);
		return -3;
	}

	dri->controller_state = cntrl;
	dri->res = res;
	res->driver_state = dri;

	return init_nvme_pci(cntrl, arg);
}

int uninit_nvme() {
	return 0;
};

int read_nvme(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
	}

	printf("Definitely reading\n");

	return 1;
}

int write_nvme(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	(void)buffer;
	(void)size;
	(void)count;
	(void)file;
	(void)res;

	return 0;
}

static uint32_t pci_codes[] = {
        0x1b360010,
        ARC_DRI_PCI_TERMINATOR
};

ARC_REGISTER_DRIVER(3, nvme_driver) = {
        .index = 0,
	.instance_counter = 0,
	.name_format = "nvme%d",
        .init = init_nvme,
	.uninit = uninit_nvme,
	.read = read_nvme,
	.write = write_nvme,
	.seek = empty_nvme,
	.rename = empty_nvme,
	.open = empty_nvme,
	.close = empty_nvme,
	.pci_codes = pci_codes
};
