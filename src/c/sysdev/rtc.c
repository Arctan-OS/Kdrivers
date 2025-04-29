/**
 * @file rtc.c
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan - Operating System Kernel
 * Copyright (C) 2023-2025 awewsomegamer
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
#include <lib/resource.h>
#include <global.h>
#include <drivers/dri_defs.h>

static int init_rtc(struct ARC_Resource *res, void *args) {
	if (res == NULL || args == NULL) {
		return -1;
	}

	printf("Hello World 2\n");

	return 0;
}

static int uninit_rtc() {
	return 0;
}

static size_t read_rtc(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
 	}

        return 0;
}

static size_t write_rtc(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
        }

        return 0;
}

static uint64_t acpi_codes[] = {
	0x95368E5074F817D9,
	ARC_DRIDEF_ACPI_TERMINATOR
};

ARC_REGISTER_DRIVER(3, rtc,) = {
        .init = init_rtc,
	.uninit = uninit_rtc,
        .read = read_rtc,
        .write = write_rtc,
	.seek = dridefs_int_func_empty,
	.rename = dridefs_int_func_empty,
	.acpi_codes = acpi_codes
};

#undef NAME_FORMAT