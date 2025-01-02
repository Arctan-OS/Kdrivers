/**
 * @file directory.c
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
#include <drivers/dri_defs.h>
#include <lib/resource.h>
#include <drivers/sysfs/ext2/super.h>

struct driver_state {
	struct ext2_super_driver_state *super;
	struct ext2_basic_driver_state basic;
};

ARC_REGISTER_DRIVER(0, ext2, directory) = {
        .init = dridefs_int_func_empty,
	.uninit = dridefs_int_func_empty,
	.write = dridefs_size_t_func_empty,
	.read = dridefs_size_t_func_empty,
	.seek = dridefs_int_func_empty,
	.rename = dridefs_int_func_empty,
	.stat = dridefs_int_func_empty,
	.control = dridefs_void_func_empty,
	.create = dridefs_int_func_empty,
	.remove = dridefs_int_func_empty,
	.locate = dridefs_void_func_empty,
};
