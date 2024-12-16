/**
 * @file partition_dummy.h
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
#ifndef ARC_DRIVERS_DEV_PARTITION_DUMMY_H
#define ARC_DRIVERS_DEV_PARTITION_DUMMY_H

#include <stdint.h>
#include <stddef.h>

struct ARC_DriArgs_ParitionDummy {
	char *drive_path;
	uint64_t lba_start;
	uint64_t attrs;
	size_t size_in_lbas;
	size_t lba_size;
	uint32_t partition_number;
};

#endif
