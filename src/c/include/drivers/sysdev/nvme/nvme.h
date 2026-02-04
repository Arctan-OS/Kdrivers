/**
 * @file nvme.h
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
#ifndef ARC_DRIVERS_SYSDEV_NVME_NVME_H
#define ARC_DRIVERS_SYSDEV_NVME_NVME_H

#include "lib/ringbuffer.h"
#define NVME_ADMIN_QUEUE -1
#define NVME_ADMIN_QUEUE_SUB_LEN 64
#define NVME_ADMIN_QUEUE_COMP_LEN 256

#include "drivers/resource.h"

enum {
        NVME_TRANSPORT_CTRL_IDEN, // Identify the transport layer (write nvme_transport_iden_t structure)
        NVME_TRANSPORT_CTRL_TO_PROPS, // Destination of reads/writes becomes controller properties
};

enum {
        NVME_TRANSPORT_TYPE_PCI,
};

typedef struct ctrl_props {
	uint64_t cap;
	uint32_t vs;
	uint32_t intms;
	uint32_t intmc;
	uint32_t cc;
	uint32_t resv0;
	uint32_t csts;
	uint32_t nssr;
	uint32_t aqa;
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
} __attribute__((packed)) ctrl_props_t;
STATIC_ASSERT(sizeof(ctrl_props_t) == 0x1000, "Controller properties size mismatch");

typedef struct qs_entry {
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
}__attribute__((packed)) qs_entry_t;
STATIC_ASSERT(sizeof(struct qs_entry) == 64, "Submission Queue Entry size mismatch");

typedef struct qc_entry {
	uint32_t dw0;
	uint32_t dw1;
	uint16_t sq_head_ptr;
	uint16_t sq_ident;
	uint16_t cid;
	uint8_t phase : 1;
	uint16_t status : 15;
}__attribute__((packed)) qc_entry_t;
STATIC_ASSERT(sizeof(struct qc_entry) == 16, "Completeion Queue Entry Size mismatch");

typedef struct nvme_qpair {
        ARC_Ringbuffer *subq;
        ARC_Ringbuffer *cmpq;
        int id;
        int phase;
} nvme_qpair_t;

typedef struct qs_wrap {
        nvme_qpair_t *qpair;
        qs_entry_t *cmd;
} qs_wrap_t;

typedef qs_wrap_t (*nvme_submit_t)(ARC_Resource *, nvme_qpair_t *, qs_entry_t *);
typedef int (*nvme_poll_t)(ARC_Resource *, qs_wrap_t *, qc_entry_t *);

// Shared between nvme.c and namespace.c
typedef struct nvme_driver_state {
        ARC_Resource *transport;
        nvme_submit_t submit;
        nvme_poll_t poll;

        struct {
                size_t max_transfer_size;
                uint16_t id;
                uint32_t version;
                uint32_t type;
                int ctratt;
        } ctrl_iden;

        struct {
                size_t requested; // Number of queue pairs requested
                size_t granted;   // Number of queue pairs granted
                                  // The number of queue pairs per namespace = granted / Arc_ProcessorCounter
                size_t count;     // Number of qpairs initialized
                nvme_qpair_t *qs;
        } qpairs;
} nvme_driver_state_t;

typedef struct nvme_transport_iden {
        uint8_t type;
        nvme_submit_t submit;
        nvme_poll_t poll;
} nvme_transport_iden_t;

#endif
