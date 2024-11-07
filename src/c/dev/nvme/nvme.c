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
#include <mm/pmm.h>

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
	if (qpair == NULL) {
		return -1;
	}

	ARC_DEBUG(WARN, "Implement\n");

	return 0;
}

int nvme_delete_all_qpairs(struct controller_state *state) {
	if (state == NULL) {
		return -1;
	}

	state->id_bmp = -1;
	ARC_DEBUG(WARN, "Implement\n");

	return 0;
}

uint64_t nvme_set_command_set(struct controller_state *state) {
	if (state == NULL || (state->flags & 1) == 0) {
		return -1;
	}

	uint64_t cap = state->properties->cap;
	uint64_t cc = state->properties->cc;

	if (MASKED_READ(cap, 43, 1) == 1) {
		// CAP.CSS.IOCSS not set
		uint64_t *iocs_struct = (uint64_t *)pmm_alloc();

		struct qs_entry iocs_struct_cmd = {
	                .cdw0.opcode = 0x6,
			.prp.entry1 = ARC_HHDM_TO_PHYS(iocs_struct),
	                .cdw10 = 0x1C | (state->controller_id << 16),
                };

		nvme_submit_command(state, ADMIN_QUEUE, &iocs_struct_cmd);
		nvme_poll_completion(state, &iocs_struct_cmd, NULL);

		int i = 0;
		uint64_t enabled_cmd_sets = 0;

		for (; i < 512; i++) {
			if (iocs_struct[i] != 0) {
				enabled_cmd_sets = iocs_struct[i];
				break;
			}
		}

		pmm_free(iocs_struct);

		struct qs_entry set_cmd = {
		        .cdw0.opcode = 0x9,
			.cdw10 = 0x19,
			.cdw11 = i & 0xFF,
	        };

		nvme_submit_command(state, ADMIN_QUEUE, &set_cmd);
		struct qc_entry set_ret = { 0 };
		nvme_poll_completion(state, &set_cmd, &set_ret);

		if ((set_ret.dw0 & 0xFF) != i) {
			ARC_DEBUG(ERR, "Command set not set to desired command set (TODO)\n");
		}

		return enabled_cmd_sets;
	} else if (MASKED_READ(cc, 1, 0b111) == 0) {
		// NVM Command Set is enabled
		return 0x1;
	}

	return 0;
}

int nvme_enumerate_enabled_command_sets(struct controller_state *state, uint64_t command_sets, uint64_t dri_instance) {
	while (command_sets != 0) {
		int idx = __builtin_ffs(command_sets) - 1;

		uint64_t *namespaces = (uint64_t *)pmm_alloc();

		struct qs_entry get_ns_cmd = {
	                .cdw0.opcode = 0x6,
			.prp.entry1 = ARC_HHDM_TO_PHYS(namespaces),
	                .cdw10 = 0x7 | (state->controller_id << 16),
			.cdw11 = (idx & 0xFF) << 24,
			.nsid = 0x0,
                };

		nvme_submit_command(state, ADMIN_QUEUE, &get_ns_cmd);
		nvme_poll_completion(state, &get_ns_cmd, NULL);

		for (int i = 0; i < 512; i++) {
			// TODO: Initialize namespace drivers
		}

		pmm_free(namespaces);

		command_sets &= ~(1 << (idx));
	}
	return 0;
}

int nvme_identify_controller(struct controller_state *state) {
	if (state == NULL || (state->flags & 1) == 0) {
		return -1;
	}

	uint8_t *data = (uint8_t *)pmm_alloc();

	struct qs_entry cmd = {
	        .cdw0.opcode = 0x6,
		.prp.entry1 = ARC_HHDM_TO_PHYS(data),
		.cdw10 = 0x1,
        };

	nvme_submit_command(state, ADMIN_QUEUE, &cmd);
	struct qc_entry ret = { 0 };
	nvme_poll_completion(state, &cmd, &ret);

	// 77    Maximum Data Transfer Size in 2 << cap.mpsmin
	//       CTRATT mem bit cleared, includes the size of the interleaved metadata
	//       mem bit set then this is size of user data only
	state->max_transfer_size = data[77];

	// 79:78 CNTLID
	state->controller_id = *(uint16_t *)(&data[78]);

	// 83:80 VERS
	state->controller_version = *(uint32_t *)(&data[80]);

	// 99:96 CTRATT bit 16 is MEM for 77
	//              bit 11 is multi-domain subsystem
	//              bit 10 UUID list 1: supported
	state->ctratt = *(uint32_t *)(&data[96]);

	// 111   Controller type (0: resv, 1: IO, 2: discovery, 3 ADMIN, all else resv)
	state->controller_type = data[111];

	// TODO: CRDTs

	pmm_free(data);

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

	init_nvme_pci(cntrl, arg);
	nvme_identify_controller(cntrl);
	uint64_t enabled_command_sets = nvme_set_command_set(cntrl);
	nvme_enumerate_enabled_command_sets(cntrl, enabled_command_sets, res->instance);

	return 0;
}

int uninit_nvme() {
	return 0;
};

int read_nvme(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
	}

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

int empty_nvme() {
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
