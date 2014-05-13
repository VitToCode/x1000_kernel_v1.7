/*
 * nand_mcu.c
 */
#include <common.h>
#include <nand.h>
#include <mcu.h>

#include <asm/jzsoc.h>
void nand_enable(NandChip *nandinfo, int cs)
{
	__nand_chip_select(cs);
}
void nand_disable(NandChip *nandinfo)
{
	__nand_chip_disselect();
}
static void __send_cmd_to_nand(unsigned char cmd, unsigned int dly_f,unsigned int dly_b, unsigned char busy, u32 port)
{
	unsigned int reg = NAND_NFCM_DLYF(dly_f) | NAND_NFCM_DLYB(dly_b) | NAND_NFCM_CMD(cmd);
	if(busy)
		reg |= NAND_NFCM_BUSY;
	__nand_cmd(reg, port);
}
static void __send_addr_to_nand(unsigned char addr, unsigned int dly_f,unsigned int dly_b, unsigned char busy, u32 port)
{
	unsigned int reg = NAND_NFAD_DLYF(dly_f) | NAND_NFAD_DLYB(dly_b) | NAND_NFAD_ADDR(addr);
	if(busy)
		reg |= NAND_NFAD_BUSY;
	__nand_addr(reg, port);
}
static inline void nand_status(int *status,u8 buswidth, u32 port)
{
	if(buswidth == 8){
		*status = REG8(port);
	}else{
		*status = REG16(port);
	}
}
static void nand_send_addr(struct nand_mcu_ops *mcu, int col_addr, int row_addr, unsigned int delay)
{
	u32 nandport = mcu->common.nand_io;
	int rowcycle = mcu->nand_info.rowcycle;
	int i;

	if(mcu->nand_info.buswidth != 8)
		col_addr = col_addr >> 1;
	if (col_addr >= 0) {
		if (mcu->nand_info.pagesize != 512) {
			__send_addr_to_nand(col_addr & 0xFF, delay, delay, 0, nandport);
			col_addr >>= 8;
		}
		__send_addr_to_nand(col_addr & 0xFF, delay, delay, 0, nandport);
	}

	if (row_addr >= 0) {
		for (i = 0; i < rowcycle - 1; i++) {
			__send_addr_to_nand(row_addr & 0xFF, delay, delay, 0, nandport);
			row_addr >>= 8;
		}
		//__send_addr_to_nand(row_addr & 0xFF, delay, delay, 0, nandport);
		__send_addr_to_nand(row_addr & 0xFF, delay, delay, 0, nandport);
	}
}
unsigned int send_nand_command(struct nand_mcu_ops *mcu, struct msgdata_cmd *cmd)
{
	u32 nandport = mcu->common.nand_io;
	unsigned char command = cmd->command;
	unsigned int cmd_delay = cmd->cmddelay;
	unsigned int addr_delay = cmd->addrdelay;
	struct msgdata_cmd cmd0;
	int ret = 0;
	cmd0.offset = -1;

	__send_cmd_to_nand(command, cmd_delay, cmd_delay, 0, nandport);
	if(cmd->offset == cmd0.offset){
		nand_send_addr(mcu,-1,cmd->pageid, addr_delay);
	}else
		nand_send_addr(mcu,(int)(cmd->offset) * 512,cmd->pageid, addr_delay);
	if(command == NAND_CMD_STATUS){
		nand_status(&ret, mcu->nand_info.buswidth, nandport);
		if(ret & NAND_STATUS_FAIL){
			ret = MSG_RET_FAIL;
		}
		else if(!(ret & NAND_STATUS_WP)){
			ret = MSG_RET_WP;
		}else{
			ret = MSG_RET_SUCCESS;
		}
	}
	return ret;
}
void send_read_random(struct nand_mcu_ops *mcu, int offset)
{
	u32 nandport = mcu->common.nand_io;
	__send_cmd_to_nand(NAND_CMD_RNDOUT, 0, 0, 0, nandport);
	nand_send_addr(mcu, offset, -1, 0);
	__send_cmd_to_nand(NAND_CMD_RNDOUTSTART, mcu->nand_info.twhr2, 0, 0, nandport);
}

void send_prog_random(struct nand_mcu_ops *mcu, int offset)
{
	u32 nandport = mcu->common.nand_io;
	__send_cmd_to_nand(NAND_CMD_RNDIN, mcu->nand_info.tcwaw, 0, 0, nandport);
	nand_send_addr(mcu, offset, -1, mcu->nand_info.tadl);
}
