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
#include <arctan.h>
#include <lib/resource.h>
#include <stdint.h>
#include <global.h>
#include <lib/util.h>
#include <drivers/dri_defs.h>
#include <lib/perms.h>

int empty_nvme() {
	return 0;
}

int init_nvme(struct ARC_Resource *res, void *arg) {
	ARC_DEBUG(INFO, "Creating new NVME with resource %p and argument %p\n", res, arg);

	return 0;
}

int uninit_nvme() {
	return 0;
};

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
	.read = empty_nvme,
	.write = empty_nvme,
	.seek = empty_nvme,
	.rename = empty_nvme,
	.open = empty_nvme,
	.close = empty_nvme,
	.pci_codes = pci_codes
};
