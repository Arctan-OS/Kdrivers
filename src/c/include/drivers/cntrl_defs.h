/**
 * @file cntrl_defs.h
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
#ifndef ARC_DRIVERS_CNTRL_DEFS_H
#define ARC_DRIVERS_CNTRL_DEFS_H

#define CNTRL_CMDSET_DRIVER 0
#define CNTRL_CMDSET_STANDARD 1

#define CNTRL_OPCODE_ASSOCIATE 0
#define CNTRL_OPCODE_DISASSOCATE 1

// Bit offsets
#define CNTRL_CMDATTRS_OPSIZE 0 // 2 bits (log2(size))
#define CNTRL_CMDATTRS_RESV0 2 // Rest of attributes

#endif
