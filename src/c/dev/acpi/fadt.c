/**
 * @file fadt.c
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
#include <global.h>
#include <fs/vfs.h>
#include <arch/acpi/acpi.h>
#include <drivers/dri_defs.h>
#include <lib/perms.h>

struct fadt {
        struct ARC_RSDTBaseEntry base;
        uint32_t firmware_ctrl;
        uint32_t dsdt;
        uint8_t resv0;
        uint8_t pref_pm_profile;
        uint16_t sci_int;
        uint32_t smi_cmd;
        uint8_t acpi_enable;
        uint8_t acpi_disable;
        uint8_t s4bios_req;
        uint8_t pstate_cnt;
        uint32_t pm1a_evt_blk;
        uint32_t pm1b_evt_blk;
        uint32_t pm1a_cnt_blk;
        uint32_t pm1b_cnt_blk;
        uint32_t pm2_cnt_blk;
        uint32_t pm_tmr_blk;
        uint32_t gpe0_blk;
        uint32_t gpe1_blk;
        uint8_t pm1_evt_len;
        uint8_t pm1_cnt_len;
        uint8_t pm2_cnt_len;
        uint8_t pm_tmr_len;
        uint8_t gpe0_blk_len;
        uint8_t gpe1_blk_len;
        uint8_t gpe1_base;
        uint8_t cst_cnt;
        uint16_t p_lvl2_lat;
        uint16_t p_lvl3_lat;
        uint16_t flush_size;
        uint16_t flush_stride;
        uint8_t duty_offset;
        uint8_t duty_width;
        uint8_t day_alrm;
        uint8_t mon_alrm;
        uint8_t century;
        uint16_t iapc_boot_arch;
        uint8_t resv1;
        uint32_t flags;
        uint32_t reset_reg[3];
        uint8_t reset_value;
        uint16_t arm_boot_arch;
        uint8_t fadt_minor_ver;
        uint64_t x_firmware_ctrl;
        uint64_t x_dsdt;
        uint32_t x_pm1a_evt_blk[3];
        uint32_t x_pm1b_evt_blk[3];
        uint32_t x_pm1a_cnt_blk[3];
        uint32_t x_pm1b_cnt_blk[3];
        uint32_t x_pm2_cnt_blk[3];
        uint32_t x_pm_tmr_blk[3];
        uint32_t x_gpe0_blk[3];
        uint32_t x_gpe1_blk[3];
        uint32_t sleep_ctrl_reg[3];
        uint32_t sleep_stat_reg[3];
        uint64_t hyper_visor_iden;
}__attribute__((packed));

int empty_fadt() {
	return 0;
}

int init_fadt(struct ARC_Resource *res, void *arg) {
	(void)res;

	struct fadt *fadt = (struct fadt *)arg;

	void *dsdt = (void *)ARC_PHYS_TO_HHDM(fadt->x_dsdt == 0 ? fadt->dsdt : fadt->x_dsdt);
	init_resource_at("/dev/acpi", ARC_DRI_DEV, ARC_DRI_DSDT, dsdt);

	return 0;
}

int uninit_fadt() {
	return 0;
}

int read_fadt(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	(void)file;
	(void)buffer;
	(void)size;
	(void)count;
	(void)res;
	return 0;
}

int write_fadt(void *buffer, size_t size, size_t count, struct ARC_File *file, struct ARC_Resource *res) {
	(void)file;
	(void)buffer;
	(void)size;
	(void)count;
	(void)res;
	return 0;
}

ARC_REGISTER_DRIVER(3, fadt) = {
        .index = ARC_DRI_FADT,
	.instance_counter = 0,
	.name_format = "fadt",
        .init = init_fadt,
	.uninit = uninit_fadt,
	.read = read_fadt,
	.write = write_fadt,
	.open = empty_fadt,
	.close = empty_fadt,
	.rename = empty_fadt,
	.seek = empty_fadt,
};
