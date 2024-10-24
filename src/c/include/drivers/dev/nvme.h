/**
 * @file nvme.h
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
#ifndef ARC_DRIVERS_DEV_NVME_H
#define ARC_DRIVERS_DEV_NVME_H

#include <stdint.h>
#include <stddef.h>
#include <global.h>

#define SQnTDBL(__properties, __n) ((uintptr_t)__properties->data + ((2 * (__n)) * (4 << __properties->cap.dstrd)))
#define CQnHDBL(__properties, __n) ((uintptr_t)__properties->data + ((2 * (__n) + 1) * (4 << __properties->cap.dstrd)))
#define ADMIN_QUEUE -1

struct controller_properties {
	struct {
		uint16_t mqes : 16;
		uint8_t cqr : 1;
		uint8_t ams : 2;
		uint8_t resv0 : 5;
		uint8_t to : 8;
		uint8_t dstrd : 4;
		uint8_t nssrs : 1;
		uint8_t css : 8;
		uint8_t bps : 1;
		uint8_t cps : 2;
		uint8_t mpsmin : 4;
		uint8_t mpsmax : 4;
		uint8_t pmrs : 1;
		uint8_t cmbs : 1;
		uint8_t nsss : 1;
		uint8_t crms : 2;
		uint8_t nsses : 1;
		uint8_t resv1 : 2;
	}__attribute__((packed)) cap;

	struct {
		uint8_t tmp;
		uint8_t min;
		uint16_t maj;
	}__attribute__((packed)) vs;

	uint32_t intms;
	uint32_t intmc;

	struct  {
		uint8_t en : 1;
		uint8_t resv0 : 3;
		uint8_t css : 3;
		uint8_t mps : 4;
		uint8_t ams : 3;
		uint8_t shn : 2;
		uint8_t iosqes : 4;
		uint8_t iocqes : 4;
		uint8_t crime : 1;
		uint16_t resv1 : 7;
	}__attribute__((packed)) cc;

	uint32_t resv0;

	struct {
		uint8_t rdy : 1;
		uint8_t cfs : 1;
		uint8_t shst : 2;
		uint8_t nssro :  1;
		uint8_t pp : 1;
		uint8_t st : 1;
		uint32_t resv0 : 25;
	}__attribute__((packed)) csts;

	uint32_t nssr;

	struct {
		uint16_t asqs : 12;
		uint8_t resv0 : 4;
		uint16_t acqs : 12;
		uint8_t resv1 : 4;
	}__attribute__((packed)) aqa;

	uint64_t asq;
	uint64_t acq;
	uint32_t cmbloc;
	uint32_t cmbsz;
	uint32_t bpinfo;
	uint32_t bprsel;
	uint64_t bpmbl;
	uint64_t cmbmsc;
	uint32_t cmbsts;
	uint32_t cmbebs;
	uint32_t cmbswtp;
	uint32_t nssd;
	uint32_t crto;
	uint8_t resv1[0xD94];
	uint32_t pmrcap;
	uint32_t pmrctl;
	uint32_t pmrsts;
	uint32_t pmrebs;
	uint32_t pmrswtp;
	uint32_t pmrmscl;
	uint32_t pmrmscu;
	uint8_t resv2[0x1E4];
	uint8_t data[];
}__attribute__((packed));
STATIC_ASSERT(sizeof(struct controller_properties) == 0x1000, "Controller properties size mismatch");

struct qs_entry {
	struct {
		uint8_t opcode;
		uint8_t fuse : 2;
		uint8_t resv0 : 4;
		uint8_t psdt : 2;
		uint16_t cid;
	}__attribute__((packed)) cdw0;

	uint32_t nsid;
	uint32_t cdw2;
	uint32_t cdw3;
	uint64_t mptr;
	// If CDW0.PSDT is set to 01 or 10, the following
	// two are SGL1, otherwise they are PRP1 and PRP2
	// respectively
	struct {
		uint64_t entry1;
		uint64_t entry2;
	}__attribute__((packed)) prp;
	uint32_t cdw10;
	uint32_t cdw11;
	uint32_t cdw12;
	uint32_t cdw13;
	uint32_t cdw14;
	uint32_t cdw15;
}__attribute__((packed));
STATIC_ASSERT(sizeof(struct qs_entry) == 64, "Submission Queue Entry size mismatch");

struct qc_entry {
	uint32_t dw0;
	uint32_t dw1;
	uint16_t sq_head_ptr;
	uint16_t sq_ident;
	uint16_t cid;
	uint8_t phase : 1;
	uint16_t status : 15;
}__attribute__((packed));
STATIC_ASSERT(sizeof(struct qc_entry) == 16, "Completeion Queue Entry Size mismatch");

#endif
