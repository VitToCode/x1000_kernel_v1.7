/*
 * irq_pdma.c
 */
#include <common.h>
#include <pdma.h>
#include <nand.h>
#include <message.h>
#include <irq_mcu.h>

#include <asm/jzsoc.h>

extern unsigned int channel_irq;

void trap_entry(void)
{
	unsigned int intctl;
	int i;

	intctl = __pdma_read_cp0(12, 1);

	if (intctl & MCU_CHANNEL_IRQ) {
		for (i = 0; i < 32; i++) {
			/* check MCU channel irq */
			if (__pdmac_channel_mirq_check(i)) {
				switch(i) {
				case PDMA_BCH_CHANNEL:
				case PDMA_NEMC_CHANNEL:
				case PDMA_DDR_CHANNEL:
				case PDMA_MSG_CHANNEL:
					if (__pdmac_channel_enabled(i))
						channel_irq |= (1 << i);
				}

				__pdmac_channel_mirq_clear(i);
			}
		}
	}
}

void pdma_nand_irq_handle(struct nand_chip *nand, struct nand_pipe_buf *pipe_buf,
				unsigned char *oob_buf)
{
	if (channel_irq & (1 << PDMA_BCH_CHANNEL)) {
		/* Clear channel irq info */
		channel_irq &= ~(1 << PDMA_BCH_CHANNEL);

		if (nand->mode & PNAND_BCH_ENC) {
			unsigned char *par_buf = oob_buf + nand->eccpos;

			irq_bch_calculate_handle(nand, pipe_buf + (nand->pipe_cnt & 0x1),
						par_buf + nand->pipe_cnt * nand->eccbytes);
			nand->mode &= ~PNAND_BCH_ENC;
		} else if (nand->mode & PNAND_BCH_DEC) {
			irq_bch_correct_handle(nand, pipe_buf + ((nand->pipe_cnt - 1) & 0x1));
			nand->mode &= ~PNAND_BCH_DEC;
			ddr_channel_cfg((pipe_buf + ((nand->pipe_cnt - 1) & 0x1))->pipe_data,
					nand->ddr_addr + (nand->pipe_cnt -1) * nand->eccsize,
					nand->eccsize, TCSM_TO_DDR);
			__pdmac_channel_launch(PDMA_DDR_CHANNEL);
/*
			if (nand->report & ALL_FF) { // Read Ecc all 0xff
				nand->mode = PNAND_HALT;
				while (REG_PDMAC_DCCS(PDMA_NEMC_CHANNEL) & PDMAC_DCCS_CTE);
				__nand_disable();
				channel_irq &= ~(1 << PDMA_NEMC_CHANNEL);
				__pdmac_mnmb_send(MB_NAND_ALL_FF);
                        } else if (nand->report & UNCOR_ECC) { // Uncorrectable Error
				nand->mode = PNAND_HALT;
				while (REG_PDMAC_DCCS(PDMA_NEMC_CHANNEL) & PDMAC_DCCS_CTE);
				__nand_disable();
				channel_irq &= ~(1 << PDMA_NEMC_CHANNEL);
				__pdmac_mnmb_send(MB_NAND_UNCOR_ECC);
			} else {
				ddr_channel_cfg((pipe_buf + ((nand->pipe_cnt - 1) & 0x1))->pipe_data,
						nand->ddr_addr + (nand->pipe_cnt -1) * nand->eccsize,
						nand->eccsize, TCSM_TO_DDR);
				__pdmac_channel_launch(PDMA_DDR_CHANNEL);
			}
*/
		}
	}

	if (channel_irq & (1 << PDMA_NEMC_CHANNEL)) {
		/* Clear channel irq info */
		channel_irq &= ~(1 << PDMA_NEMC_CHANNEL);
		nand->mode &= ~PNAND_NEMC;

		if (nand->ctrl == CTRL_READ_OOB) {
			nand->ctrl = CTRL_READ_DATA;
			pdma_nand_read_ctrl(nand, 0, CTRL_READ_DATA);

			nand->pipe_cnt = 0;
			nand->mode = PNAND_NEMC;
			pdma_nand_read_data(nand, pipe_buf, NULL);
		} else if (nand->ctrl == CTRL_WRITE_OOB) {
			int ret;
			ret = pdma_nand_write_ctrl(nand, 0, CTRL_WRITE_CONFIRM);
			nand->mode = PNAND_HALT;
			__nand_disable();

			if (ret < 0)
				__pdmac_mnmb_send(MB_NAND_WRITE_FAIL);
			else
				__pdmac_mnmb_send(MB_NAND_WRITE_DONE);
		}
	}

	if (channel_irq & (1 << PDMA_DDR_CHANNEL)) {
		/* Clear channel irq info */
		channel_irq &= ~(1 << PDMA_DDR_CHANNEL);
		nand->mode &= ~PNAND_DDR;

		if (nand->mode & PNAND_BCH_ENC)
			pdma_bch_calculate(nand, pipe_buf + (nand->pipe_cnt & 0x1));

	}

	if (!nand->mode) {
		unsigned char *par_buf = oob_buf + nand->eccpos + nand->pipe_cnt * nand->eccbytes;
		nand->pipe_cnt++;

		switch (nand->ctrl) {
		case CTRL_READ_DATA :
			if (nand->pipe_cnt < nand->eccsteps)
				nand->mode |= PNAND_NEMC;
			if (nand->pipe_cnt < nand->eccsteps + 1) {
				nand->mode |= PNAND_BCH_DEC | PNAND_DDR;

				pdma_nand_read_data(nand, pipe_buf, par_buf);
				//channel_irq |= 1 << PDMA_BCH_CHANNEL;
			} else {
				nand->mode = PNAND_HALT;
				__pn_disable();
				__nand_disable();
                                if(nand->report & ALL_FF) {
                                        __pdmac_mnmb_send(MB_NAND_ALL_FF);
                                } else if(nand->report & UNCOR_ECC) {
                                        __pdmac_mnmb_send(MB_NAND_UNCOR_ECC);
                                } else if(nand->report & MOVE_BLOCK) {
        				__pdmac_mnmb_send(MB_MOVE_BLOCK);
                                } else {
        				__pdmac_mnmb_send(MB_NAND_READ_DONE);
                                }
                        }
			break;
		case CTRL_WRITE_DATA :
			if (nand->pipe_cnt < nand->eccsteps)
				nand->mode |= PNAND_DDR | PNAND_BCH_ENC;
			if (nand->pipe_cnt < nand->eccsteps + 1) {
				nand->mode |= PNAND_NEMC;

				pdma_nand_write_data(nand, pipe_buf,
					nand->ddr_addr + nand->pipe_cnt * nand->eccsize);
			} else {
				nand->ctrl = CTRL_WRITE_OOB;
				pdma_nand_write_ctrl(nand, 0, CTRL_WRITE_OOB);
				pdma_nand_write_oob(nand, oob_buf);
			}
			break;
		default :
			nand->mode = PNAND_HALT;
		}
	}
}

void pdma_msg_irq_handle(struct nand_chip *nand, struct nand_pipe_buf *pipe_buf,
			struct pdma_msg *msg, unsigned char *oob_buf)
{
	unsigned char state;

	if (channel_irq & (1 << PDMA_MSG_CHANNEL)) {
		/* Clear channel irq info */
		channel_irq &= ~(1 << PDMA_MSG_CHANNEL);
		nand->report = 0;

		switch (msg->cmd) {
		case MSG_NAND_INIT :
			pdma_nand_init(nand, pipe_buf, msg->info, oob_buf);
			__pdmac_mnmb_send(MB_NAND_INIT_DONE);
			break;
		case MSG_NAND_READ :
			pdma_channel_init();
			nand->chipsel = msg->info[MSG_NAND_BANK];
			nand->ddr_addr = (void *)msg->info[MSG_DDR_ADDR];
			nand->nand_io = (void *)NEMC_CS(nand->chipsel);

			nand->ctrl = CTRL_READ_OOB;
			__nand_enable(nand->chipsel);
			pdma_nand_read_ctrl(nand, msg->info[MSG_PAGEOFF], CTRL_READ_OOB);
			pdma_nand_read_oob(nand, oob_buf);
			break;
		case MSG_NAND_WRITE :
			pdma_channel_init();
			nand->chipsel = msg->info[MSG_NAND_BANK];
			nand->ddr_addr = (void *)msg->info[MSG_DDR_ADDR];
			nand->nand_io = (void *)NEMC_CS(nand->chipsel);

			__nand_enable(nand->chipsel);
			pdma_nand_status(nand, &state);
			if (!(state & NAND_STATUS_WP)) {
				__pdmac_mnmb_send(MB_NAND_WRITE_PROTECT);
				break;
			}

			nand->ctrl = CTRL_WRITE_DATA;
			pdma_nand_write_ctrl(nand, msg->info[MSG_PAGEOFF], CTRL_WRITE_DATA);

			nand->pipe_cnt = 0;
			nand->mode = PNAND_DDR | PNAND_BCH_ENC;
			pdma_nand_write_data(nand, pipe_buf, nand->ddr_addr);
			break;
		case MSG_NAND_ERASE :
			nand->chipsel = msg->info[MSG_NAND_BANK];
			nand->nand_io = (void *)NEMC_CS(nand->chipsel);
			pdma_nand_erase(nand, msg->info[MSG_PAGEOFF]);
			break;
		default :
			;
		}
	}
}
