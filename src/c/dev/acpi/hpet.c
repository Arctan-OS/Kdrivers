/**
 * @file hpet.c
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
#include <arch/acpi/acpi.h>
#include <lib/resource.h>
#include <drivers/dri_defs.h>

struct hpet {
	struct ARC_RSDTBaseEntry base;
	union {
		uint8_t hardware_rev_id;
		uint8_t first_n_cmps : 5;
		uint8_t count_size_cap : 1;
		uint8_t resv0 : 1;
		uint8_t legacy_irq_routing : 1;
		uint16_t pci_vendor_id;
	} event_timer_blk_id;
	uint32_t base_addr[3];
	uint8_t hpet_number;
	uint16_t main_counter_min;
	uint8_t prot_oem_attr;
};

int empty_hpet() {
	return 0;
}

int init_hpet(struct ARC_Resource *res, void *arg) {
	(void)res;

	struct hpet *hpet = (struct hpet *)arg;
	(void)hpet;

	return 0;
};

int uninit_hpet() {
	return 0;
}

int read_hpet(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	(void)file;
	(void)buffer;
	(void)size;
	(void)count;
	(void)res;
	return 0;
}

int write_hpet(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	(void)file;
	(void)buffer;
	(void)size;
	(void)count;
	(void)res;
	return 0;
}

ARC_REGISTER_DRIVER(3, hpet) = {
        .index = ARC_DRI_HPET,
	.instance_counter = 0,
	.name_format = "hpet%d",
	.init = init_hpet,
	.uninit = uninit_hpet,
	.read = read_hpet,
	.write = write_hpet,
	.open = empty_hpet,
	.close = empty_hpet,
	.seek = empty_hpet,
	.rename = empty_hpet,
};
