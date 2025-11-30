/**
 * @file uart.c
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
#include "arch/acpi/acpi.h"
#include "arch/io/port.h"
#include "drivers/dri_defs.h"
#include "drivers/resource.h"
#include "fs/vfs.h"
#include "global.h"
#include "lib/util.h"
#include "mm/allocator.h"

#define NAME_FORMAT "/dev/uart%d"

// https://osdev.wiki/wiki/Serial_Ports
// Base Port + X
// X = { 
//      READ:
//	 +0 : { DLAB = 0 -> RX Buffer, DLAB = 1 -> Low byte of divisor }
//	 +1 : { DLAB = 0 -> Interrupt Enable, DLAB = 1 -> High byte of divisor }
//	 +2 : Interrupt identification
//       +5 : Line status register
//	 +6 : Modem status register
//      WRITE:
//       +0 : { DLAB = 0 -> TX Buffer, DLAB = 1 -> Low byte of divisor }
//	 +1 : { DLAB = 0 -> Interrupt Enable, DLAB = 1 -> High byte of divisor }
//	 +2 : FIFO control register
//	R/W:
//	 +3 : Line control register
//	 +4 : Modem control register
//       +7 : Scratch register
// }
//
// Line Control Register: 0bDBPPPSXX                                                    
//     D: Divisor latch bit (DLAB)
//     B: Break enable bit
//     P: Parity { --0 -> None, 001 -> Odd, 
//                 011 -> Even, 101 -> Mark, 
//                 111 -> Space }
//     S: Stop (0: 1 stop bit, 1: 1.5 or 2
//     X: No. data bits - 5
//
// Interrupt Enable Register: 0b----MRTA
//     -: Reserved
//     M: Modem status
//     R: Receiver line status
//     T: Transmitter holding register empty
//     0: Received data available
//
// FIFO Control Register: 0bII--DCRE
//     I: Interrupt trigger level { 00 -> 1 byte, 01 -> 4 bytes, 
//                                  10 -> 8 bytes, 11 -> 14 bytes }
//     -: Reserved
//     D: DMA mode select
//     C: Clear transmit FIFO (will auto clear self)
//     R: Clear receive FIFO (will auto clear self)
//     E: Enable FIFOs
// 
// Interrupt Identification Register: 0bFF--TSSP
//     F: FIFO buffer state { 00 -> No FIFO, 01 -> FIFO unusable,
//                             10 -> FIFO enabled }
//     -: Reserved
//     T: { UART 16550 -> Timeout interrupt pending, -> reserved }
//     S: Interrupt state (priority) 
//                           { 00 -> Modem Status, 01 -> Transmitter holding register empty,
//                           10 -> Received data available, 11 -> Receiver line status }
//                           ( 00 -> Lowest, 11 -> Highest )
//     P: Interrupt pending
//
// Modem Control Register: 0b---LOURD
//     -: Reserved
//     L: Loopback
//     O: Hardware OUT2 pin (used to enable IRQ in PC implemntations)
//     U: Hardware OUT1 pin (unused in PC implementations)
//     R: Hardware RTS pin
//     D: Data terminal ready
//
// Line Status Register: 0bETHBFPOD
//     E: Set if there is an error with a word in input buffer
//     T: Set if the transmitter is not doing anything
//     H: Set if the trasmission buffer is empty
//     B: Set if there is a break in data input
//     F: Set if a stop bit was missing
//     P: Set if there was an error in transmission detected by parity
//     O: Set if data has been lost
//     D: Set if there is data can be read
//
// Modem Status Register: 0bCRSXDTYZ
//     C: Inverted data carrier detect signal (!DCD)
//     R: Inverted ring indicator signal (!RI)
//     S: Inverted data set ready signal (!DSR)
//     X: Inverted clear to send signal (!RTS)
//     D: Indicates DCD has changed
//     T: Ring indicator went high
//     Y: Indicates DSR has changed
//     Z: Indicates CTS has changed


struct driver_state {
	uint32_t port_base;
	uint32_t align;
	int data_len;
};

// TODO: Replace this with some sort of information from the caller
static uint64_t instance_counter = 0;

static void set_buad_rate_divisor(const struct driver_state *state, uint16_t divisor) {
	uint8_t dlab = inb(state->port_base + 3);
	MASKED_WRITE(dlab, 1, 7, 1);
	outb(state->port_base + 3, dlab);

	outb(state->port_base, divisor & 0xFF);
	outb(state->port_base + 1, (divisor >> 8) & 0xFF);

	MASKED_WRITE(dlab, 0, 7, 1);
	outb(state->port_base + 3, dlab);
}

static int get_data_bits(const struct driver_state *state) {
	uint8_t data = inb(state->port_base + 3);
	return MASKED_READ(data, 0, 0b11) + 5;
}

static void set_data_bits(const struct driver_state *state, int bits) {
	uint8_t data = inb(state->port_base + 3);
	MASKED_WRITE(data, bits - 5, 0, 0b11);
	outb(state->port_base + 3, data);
}

enum {
	NO_PARITY = -1,
	ODD_PARITY,
	EVEN_PARITY,
	MARK_PARITY,
	SPACE_PARITY
};

static int get_parity(const struct driver_state *state) {
	uint8_t data = inb(state->port_base + 3);

	if (MASKED_READ(data, 3, 0b001) == 0) {
		return NO_PARITY;
	}

	return MASKED_READ(data, 4, 0b11);
}

static void set_parity(const struct driver_state *state, int parity) {
	uint8_t data = inb(state->port_base + 3);

	if (parity == NO_PARITY) {
		MASKED_WRITE(data, 0, 3, 0b111);
	} else {
		MASKED_WRITE(data, (parity << 1) | 1, 3, 0b111);
	}

	outb(state->port_base + 3, data);
}

enum {
	ONE_STOP_BIT = 1,
	ONE_ONE_HALF_STOP_BITS = 2,
	TWO_STOP_BITS = 3
};

static int get_stop_bits(const struct driver_state *state) {
	uint8_t data = inb(state->port_base + 3);

	if (MASKED_READ(data, 2, 0b1) == 0) {
		return ONE_STOP_BIT;
	} else {
		// Determine 1 1/2 or 2 stop bits
		return TWO_STOP_BITS;
	}
}

static void set_stop_bits(const struct driver_state *state, int count) {
	uint8_t data = inb(state->port_base + 3);

	switch (count) {
		case ONE_STOP_BIT: {
			MASKED_WRITE(data, 0, 2, 0b1);
			break;
		}

		case ONE_ONE_HALF_STOP_BITS:
		case TWO_STOP_BITS: {
			MASKED_WRITE(data, 1, 2, 0b1);
			break;
		}
	}

	outb(state->port_base + 3, data);
}

static void clear_tx_fifo(const struct driver_state *state) {
	outb(state->port_base + 2, 1 << 2);
}

static void clear_rx_fifo(const struct driver_state *state) {
	outb(state->port_base + 2, 1 << 1);
}

static int enable_fifos(const struct driver_state *state) {
	outb(state->port_base + 2, 1);
	
	uint8_t data = inb(state->port_base + 2);

	if (MASKED_READ(data, 6, 0b11) >= 0b10) {
		// NOTE: On the wiki page it says a value of 0b10 will signal
		//       that the FIFOs have been enabled. In QEMU, I receive
		//       a value of 0b11 instead
		return 0;
	} else {
		return -1;
	}
}

static int init_uart(struct ARC_Resource *res, void *args) {
	if (res == NULL || args == NULL) {
		return -1;
	}

	struct driver_state *state = alloc(sizeof(*state));

	if (state == NULL) {
		return -2;
	}

	memset(state, 0, sizeof(*state));

	struct ARC_ACPIDevInfo *dev_info = (struct ARC_ACPIDevInfo *)args;
	state->data_len = dev_info->io->length;
	state->port_base = dev_info->io->base;
	state->align = dev_info->io->align;
	res->driver_state = state;
	
	char path[64] = { 0 };

	sprintf(path, NAME_FORMAT, instance_counter++);

	/*
	struct ARC_VFSNodeInfo info = {
		.type = ARC_VFS_N_DEV,
		.mode = ARC_STD_PERM,
		.resource_overwrite = res,
        };
	
	vfs_create(path, &info);
	*/

	set_data_bits(state, 8);
	set_parity(state, NO_PARITY);
	set_stop_bits(state, ONE_STOP_BIT);

	return 0;
}

static int uninit_uart() {
	return 0;
}

static size_t read_uart(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
 	}

        return 0;
}

static size_t write_uart(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
        }

        return 0;
}

static int stat_uart(struct ARC_Resource *res, char *filename, struct stat *stat) {
	(void)filename;

	if (res == NULL || stat == NULL) {
		return -1;
	}

	struct driver_state *state = (struct driver_state *)res->driver_state;

	return 0;
}


static uint64_t acpi_codes[] = {
	0x9D2E741F3E2DEEC7, 
	ARC_DRIDEF_ACPI_TERMINATOR
};

ARC_REGISTER_DRIVER(ARC_DRI_GROUP_DEV_CHAR, uart) = {
        .init = init_uart,
	.uninit = uninit_uart,
        .read = read_uart,
        .write = write_uart,
	.seek = dridefs_int_func_empty,
	.rename = dridefs_int_func_empty,
	.stat = stat_uart,
	.acpi_codes = acpi_codes
};

#undef NAME_FORMAT
