/**
 * @file nsio.c
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

int empty_nvme_ns_io() {
	return 0;
}

int init_nvme_ns_io(struct ARC_Resource *res, void *arg) {
	return 0;
}

int uninit_nvme_ns_io() {
	return 0;
};

int read_nvme_ns_io(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
	}

	return 1;
}

int write_nvme_ns_io(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	(void)buffer;
	(void)size;
	(void)count;
	(void)file;
	(void)res;

	return 0;
}

ARC_REGISTER_DRIVER(3, nvme_ns_io_driver) = {
        .index = 1,
	.instance_counter = 0,
	.name_format = "%sn%d",
        .init = init_nvme_ns_io,
	.uninit = uninit_nvme_ns_io,
	.read = read_nvme_ns_io,
	.write = write_nvme_ns_io,
	.seek = empty_nvme_ns_io,
	.rename = empty_nvme_ns_io,
	.open = empty_nvme_ns_io,
	.close = empty_nvme_ns_io,
};
