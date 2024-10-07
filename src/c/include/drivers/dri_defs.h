/**
 * @file dri_defs.h
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
 * Definitions for driver indices and groups.
*/
#ifndef ARC_DRIVERS_DRI_DEFS_H
#define ARC_DRIVERS_DRI_DEFS_H

// Driver group 0x0 (Filesystem)
#define ARC_DRI_FS 0x0
// Indices within filesystem group
#define ARC_SDRI_INITRAMFS 0x00
#define ARC_FDRI_INITRAMFS 0x01
#define ARC_SDRI_EXT2      0x02
#define ARC_FDRI_EXT2      0x03
#define ARC_SDRI_BUFFER    0x04
#define ARC_FDRI_BUFFER    0x05
#define ARC_SDRI_FIFO      0x06
#define ARC_FDRI_FIFO      0x07

// Undefined driver group 0x1
// Undefined driver group 0x2

// Driver group 0x3 (Devices)
#define ARC_DRI_DEV 0x3
// Indices within devices group
#define ARC_DRI_RSDT 0x00
#define ARC_DRI_APIC 0x01
#define ARC_DRI_FADT 0x02
#define ARC_DRI_DSDT 0x03
#define ARC_DRI_HPET 0x04

#endif
