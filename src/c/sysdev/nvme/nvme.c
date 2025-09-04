/**
 * @file nvme.c
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
*/
#include "drivers/dri_defs.h"
#include "drivers/sysdev/nvme/pci.h"
#include "drivers/sysdev/nvme/namespace.h"
#include "drivers/sysdev/nvme/nvme.h"
#include "fs/vfs.h"
#include "lib/mutex.h"
#include "lib/ringbuffer.h"
#include "lib/util.h"
#include "mm/allocator.h"
#include "mm/pmm.h"

#define NAME_FORMAT "/dev/nvme%d"

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

	size_t idx = __builtin_ffs(state->id_bmp);

	if (idx >= state->max_ioqpair_count) {
		mutex_unlock(&state->qpair_lock);
		free(entry);

		return NULL;
	}

	entry->id = idx - 1;
	state->id_bmp &= ~(1 << entry->id);
	entry->submission_queue = init_ringbuffer((void *)sub, sub_len, sizeof(struct qs_entry));
	entry->completion_queue = init_ringbuffer((void *)comp, comp_len, sizeof(struct qc_entry));
	entry->phase = 1;
	entry->next = state->list;
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

int nvme_create_io_qpair(struct controller_state *state, struct qpair_list_entry *qpair, uint8_t nvm_set, int irq) {
	if (state == NULL || qpair == NULL) {
		return -1;
	}

	uint16_t real_id = (uint16_t)qpair->id + 1;

	struct qs_entry cmd = {
		.cdw0.opcode = 0x5,
                .prp.entry1 = ARC_HHDM_TO_PHYS(qpair->completion_queue->base),
		.cdw10 = real_id | ((qpair->completion_queue->objs - 1) << 16),
		.cdw11 = 1 | ((irq > 31) << 1) | ((irq & 0xFFFF) << 16),
		.cdw12 = nvm_set
        };

	nvme_submit_command(state, ADMIN_QUEUE, &cmd);
	nvme_poll_completion(state, &cmd, NULL);

	cmd.cdw0.opcode = 0x1;
	cmd.prp.entry1 = ARC_HHDM_TO_PHYS(qpair->submission_queue->base);
	cmd.cdw10 = real_id | ((qpair->submission_queue->objs - 1) << 16);
	cmd.cdw11 = 1 | (real_id << 16);

	nvme_submit_command(state, ADMIN_QUEUE, &cmd);
	nvme_poll_completion(state, &cmd, NULL);

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
		uint64_t *iocs_struct = (uint64_t *)pmm_fast_page_alloc();

		struct qs_entry iocs_struct_cmd = {
	                .cdw0.opcode = 0x6,
			.prp.entry1 = ARC_HHDM_TO_PHYS(iocs_struct),
	                .cdw10 = 0x1C | (state->controller_id << 16),
                };

		nvme_submit_command(state, ADMIN_QUEUE, &iocs_struct_cmd);
		nvme_poll_completion(state, &iocs_struct_cmd, NULL);

		uint32_t i = 0;
		uint64_t enabled_cmd_sets = 0;

		for (; i < 512; i++) {
			if (iocs_struct[i] != 0) {
				enabled_cmd_sets = iocs_struct[i];
				break;
			}
		}

		pmm_fast_page_free(iocs_struct);

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

int nvme_enumerate_enabled_command_sets(struct controller_state *state, uint64_t command_sets) {
	while (command_sets != 0) {
		int idx = __builtin_ffs(command_sets) - 1;

		uint64_t *namespaces = (uint64_t *)pmm_fast_page_alloc();
		memset(namespaces, 0, PAGE_SIZE);

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
			if (namespaces[i] == 0) {
				continue;
			}

			// TODO: Make this dynamic
			// TODO: Create a configure function for drivers, so that the
			//       namespace driver can be initialized earlier (using admin cmd 0x2)
			//       stored in a list and then just configured here

			struct nvme_namespace_dri_args args = {
				.state = state,
			        .namespace = namespaces[i],
				.command_set = idx
		        };

			init_resource(ARC_DRIDEF_NVME_NAMESPACE, &args);
		}

		pmm_fast_page_free(namespaces);

		command_sets &= ~(1 << (idx));
	}
	return 0;
}

int nvme_identify_controller(struct controller_state *state) {
	if (state == NULL || (state->flags & 1) == 0) {
		return -1;
	}

	uint8_t *data = (uint8_t *)pmm_fast_page_alloc();

	struct qs_entry cmd = {
	        .cdw0.opcode = 0x6,
		.prp.entry1 = ARC_HHDM_TO_PHYS(data),
		.cdw10 = 0x1,
        };

	nvme_submit_command(state, ADMIN_QUEUE, &cmd);
	nvme_poll_completion(state, &cmd, NULL);

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

	cmd.cdw10 = 0x2;
	nvme_submit_command(state, ADMIN_QUEUE, &cmd);
	nvme_poll_completion(state, &cmd, NULL);

	pmm_fast_page_free(data);

	return 0;
}

int nvme_setup_io_queues(struct controller_state *state) {
	// Configure IO sub / comp queue sizes
	MASKED_WRITE(state->properties->cc, 6, 16, 0xF);
	MASKED_WRITE(state->properties->cc, 4, 20, 0xF);

	// Request 64 IO qpairs
	struct qs_entry cmd = {
	        .cdw0.opcode = 0x9,
		.cdw10 = 0x7,
		.cdw11 = (63) | (63 << 16)
        };

	nvme_submit_command(state, ADMIN_QUEUE, &cmd);
	struct qc_entry ret = { 0 };
	nvme_poll_completion(state, &cmd, &ret);

	state->max_ioqpair_count = min(MASKED_READ(ret.dw0, 0, 0xFFFF), MASKED_READ(ret.dw0, 16, 0xFFFF));

	return 0;
}

int init_nvme(struct ARC_Resource *res, void *arg) {
	return -1;

	if (res == NULL || arg == NULL) {
		return -1;
	}
	// NOTE: Assuming that arg is a ARC_PCIHeader

	struct driver_state *dri = (struct driver_state *)alloc(sizeof(*dri));

	if (dri == NULL) {
		return -2;
	}

	memset(dri, 0, sizeof(*dri));

	struct controller_state *cntrl = (struct controller_state *)alloc(sizeof(*cntrl));

	if (cntrl == NULL) {
		free(dri);
		return -3;
	}

	memset(cntrl, 0, sizeof(*cntrl));

	dri->controller_state = cntrl;
	dri->res = res;
	res->driver_state = dri;

	cntrl->max_ioqpair_count = 2;

	// XXX: This function calls to reset the controller that reults in a triple fault,
	//      added a return -1; to error out to resource handler
	init_nvme_pci(cntrl, arg);
	nvme_identify_controller(cntrl);
	nvme_setup_io_queues(cntrl);

	uint64_t enabled_command_sets = nvme_set_command_set(cntrl);
	nvme_enumerate_enabled_command_sets(cntrl, enabled_command_sets);

	char path[64] = { 0 };
	sprintf(path, NAME_FORMAT, cntrl->controller_id);

	struct ARC_VFSNodeInfo info = {
	        .type = ARC_VFS_N_DEV,
		.mode = ARC_STD_PERM,
		.resource_overwrite = res,
        };
	vfs_create(path, &info);

	return 0;
}

int uninit_nvme() {
	return 0;
}

static size_t read_nvme(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
	}

	return 1;
}

static size_t write_nvme(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	(void)buffer;
	(void)size;
	(void)count;
	(void)file;
	(void)res;

	return 0;
}

static int stat_nvme(struct ARC_Resource *res, char *filename, struct stat *stat) {
	(void)res;
	(void)filename;
	(void)stat;

	return 0;
}

static uint32_t pci_codes[] = {
        0x1b360010,
        ARC_DRIDEF_PCI_TERMINATOR
};

ARC_REGISTER_DRIVER(3, nvme,) = {
        .init = init_nvme,
	.uninit = uninit_nvme,
	.read = read_nvme,
	.write = write_nvme,
	.seek = dridefs_int_func_empty,
	.rename = dridefs_int_func_empty,
	.stat = stat_nvme,
	.pci_codes = pci_codes
};

#undef NAME_FORMAT
