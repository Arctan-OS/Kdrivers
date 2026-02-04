#include "drivers/resource.h"
#include "drivers/sysdev/nvme/nvme.h"

static int nvme_register_io_qpair(nvme_driver_state_t *state, nvme_qpair_t *qpair, uint8_t nvm_set, int irq) {
        if (state == NULL || qpair == NULL) {
		return -1;
	}

	struct qs_entry cmd = {
		.cdw0.opcode = 0x5,
                .prp.entry1 = ARC_HHDM_TO_PHYS(qpair->cmpq->base),
		.cdw10 = qpair->id | ((qpair->cmpq->objs - 1) << 16),
		.cdw11 = 1 | ((irq > 31) << 1) | ((irq & 0xFFFF) << 16),
		.cdw12 = nvm_set
        };

        qs_wrap_t wrap = state->submit(state->transport, NULL, &cmd);
	state->poll(state->transport, &wrap, NULL);

	cmd.cdw0.opcode = 0x1;
	cmd.prp.entry1 = ARC_HHDM_TO_PHYS(qpair->subq->base);
	cmd.cdw10 = qpair->id | ((qpair->subq->objs - 1) << 16);
	cmd.cdw11 = 1 | (qpair->id << 16);

        wrap = state->submit(state->transport, NULL, &cmd);
	state->poll(state->transport, &wrap, NULL);
        
        return 0;
}
