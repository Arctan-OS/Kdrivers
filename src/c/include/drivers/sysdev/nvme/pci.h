/**
 * @file pci.h
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
#ifndef ARC_DRIVERS_DEV_SYSNVME_PCI_H
#define ARC_DRIVERS_DEV_SYSNVME_PCI_H

#include <arch/pci/pci.h>
#include <drivers/sysdev/nvme/nvme.h>

int nvme_pci_submit_command(struct controller_state *state, int queue, struct qs_entry *cmd);
int nvme_pci_poll_completion(struct controller_state *state, struct qs_entry *cmd, struct qc_entry *ret);

int init_nvme_pci(struct controller_state *state, struct ARC_PCIHeader *header);

#endif