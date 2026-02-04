#include "drivers/sysdev/nvme/nvme.h"
#include "drivers/resource.h"
#include "drivers/dri_defs.h"
#include "mm/allocator.h"
#include "mm/pmm.h"
#include <stddef.h>

typedef struct driver_state {
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
        
} driver_state_t;

int nvme_create_qpair() {
        return 0;
}

int nvme_delete_qpair() {
        return 0;
}

int nvme_delete_all_qpairs() {
        return 0;
}

int nvme_create_io_qpair() {
        return 0;
}

int nvme_setup_io_queues() {
        return 0;
}

static int nvme_identify_controller(driver_state_t *state) {
	uint8_t *data = (uint8_t *)pmm_fast_page_alloc();

	qs_entry_t cmd = {
	        .cdw0.opcode = 0x6,
		.prp.entry1 = ARC_HHDM_TO_PHYS(data),
		.cdw10 = 0x1,
        };

	qs_wrap_t wrap = state->submit(state->transport, NULL, &cmd);
	state->poll(state->transport, &wrap, NULL);

	// 77    Maximum Data Transfer Size in 2 << cap.mpsmin
	//       CTRATT mem bit cleared, includes the size of the interleaved metadata
	//       mem bit set then this is size of user data only
	state->ctrl_iden.max_transfer_size = data[77];

	// 79:78 CNTLID
	state->ctrl_iden.id = *(uint16_t *)(&data[78]);

	// 83:80 VERS
	state->ctrl_iden.version = *(uint32_t *)(&data[80]);

	// 99:96 CTRATT bit 16 is MEM for 77
	//              bit 11 is multi-domain subsystem
	//              bit 10 UUID list 1: supported
	state->ctrl_iden.ctratt = *(uint32_t *)(&data[96]);

	// 111   Controller type (0: resv, 1: IO, 2: discovery, 3 ADMIN, all else resv)
	state->ctrl_iden.type = data[111];

	// TODO: CRDTs

	cmd.cdw10 = 0x2;
	wrap = state->submit(state->transport, NULL, &cmd);
	state->poll(state->transport, &wrap, NULL);

	pmm_fast_page_free(data);

	return 0;
}

int init_nvme(ARC_Resource *res, void *arg) {
        if (arg == NULL) {
                ARC_DEBUG(ERR, "NULL pointer given for transport resource\n");
                return -1;
        }
        
        driver_state_t *state = alloc(sizeof(*state));

        if (state == NULL) {
                ARC_DEBUG(INFO, "Failed to allocate driver state\n");
                return -2;
        }

        memset(state, 0, sizeof(*state));

        res->driver_state = state;
        
        ARC_DEBUG(INFO, "Initializing general NVME driver with transport=%p\n", arg);
        
        ARC_Resource *transport = arg;
        state->transport = transport;
        
        nvme_transport_iden_t ident = { 0 };
        ARC_ControlPacketInstruction cmd = { .command = NVME_TRANSPORT_CTRL_IDEN,
                                             .data    = &ident };
        
        ARC_ControlPacketResponse *resp = NULL;
        if (transport->driver->control == NULL ||
            (resp = transport->driver->control(transport, &cmd)) == NULL) {
                return -1;
        }

        state->poll = ident.poll;
        state->submit = ident.submit;
        printf("%p %p\n", state->poll, state->submit);
        
        nvme_identify_controller(state);

        
        // TODO: Take the properties from PCI / fabrics and initialize
        //       it into controller state
        // TODO: Further initialize controller
        // TODO: Enumerate all NVME structures and create files into the
        //       VFS under /dev/

        return 0;
}

int uninit_nvme(ARC_Resource *resource) {
        (void)resource;
        return 0;
}

static size_t read_nvme(void *buffer, size_t size, size_t count, ARC_File *file, ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
	}

        return 0;
}

static size_t write_nvme(void *buffer, size_t size, size_t count, ARC_File *file, ARC_Resource *res) {
	if (buffer == NULL || size == 0 || count == 0 || file == NULL || res == NULL) {
		return 0;
	}
        
        return 0;
}

static int stat_nvme_pci(ARC_Resource *res, char *filename, struct stat *stat) {
	(void)res;
	(void)filename;
	(void)stat;
        return 0;
}

ARC_REGISTER_DRIVER(ARC_DRIGRP_DEV, nvme) = {
        .init = init_nvme,
	.uninit = uninit_nvme,
	.read = read_nvme,
	.write = write_nvme,
	.seek = dridefs_int_func_empty,
	.rename = dridefs_int_func_empty,
	.stat = stat_nvme_pci,
	.codes = NULL
};
