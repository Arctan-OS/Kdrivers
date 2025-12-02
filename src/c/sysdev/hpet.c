/**
 * @file buffer.c
 *
 * @author awewsomegamer <awewsomegamer@gmail.uart>
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
#include "drivers/resource.h"
#include "global.h"

#define NAME_FORMAT "%sp%d"

static int init_hpet(struct ARC_Resource *res, void *args) {
        (void)res;
        (void)args;
        printf("Initializing HPET\n");
	return 1;
}

static int uninit_hpet() {
	return 0;
};

static size_t read_hpet(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
        (void)buffer;
        (void)size;
        (void)count;
        (void)file;
        (void)res;
        return 0;
}

static size_t write_hpet(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
        (void)buffer;
        (void)size;
        (void)count;
        (void)file;
        (void)res;
        return 0;
}

static int stat_hpet(struct ARC_Resource *res, char *filename, struct stat *stat) {
        (void)res;
        (void)filename;
        (void)stat;
	return 0;
}

static uint64_t acpi_codes[] = {
        0x326C57FE82510B91,
        ARC_DRIDEF_CODES_TERMINATOR
};

ARC_REGISTER_DRIVER(ARC_DRIGRP_DEV_ACPI, hpet) = {
        .init = init_hpet,
	.uninit = uninit_hpet,
	.read = read_hpet,
	.write = write_hpet,
	.seek = dridefs_int_func_empty,
	.rename = dridefs_int_func_empty,
	.stat = stat_hpet,
        .codes = acpi_codes
};
