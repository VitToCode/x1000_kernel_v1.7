/*
 * nand_mcu.c
 */
#include <common.h>
#include <nand.h>
#include <mcu.h>

#include <asm/jzsoc.h>
/*
   static void nand_wait_rb(void)
   {
   volatile unsigned int timeout = 2000;
   while ((REG_GPIO_PXPIN(0) & 0x00100000) && timeout--);
   while (!(REG_GPIO_PXPIN(0) & 0x00100000));
   }
 */
extern void mcu_delay(unsigned int cycle);

void nand_enable(NandChip *nandinfo, int cs)
{
	mcu_delay(nandinfo->tclh);
	__nand_enable(cs);
	mcu_delay(nandinfo->tcs);
}
void nand_disable(NandChip *nandinfo)
{
	mcu_delay(nandinfo->tclh);
	__nand_disable();
}
static void nand_send_addr(struct nand_mcu_ops *mcu, int col_addr, int row_addr)
{
	u32 nandport = mcu->common.nand_io;
	int rowcycle = mcu->nand_info.rowcycle;
	int i;
	if (col_addr >= 0) {
		col_addr = col_addr / (mcu->nand_info.buswidth / 8);

		if (mcu->nand_info.pagesize != 512) {
			__nand_addr(col_addr & 0xFF, nandport);
			col_addr >>= 8;
		}
		__nand_addr(col_addr & 0xFF, nandport);
	}

	if (row_addr >= 0) {
		for (i = 0; i < rowcycle; i++) {
			__nand_addr(row_addr & 0xFF, nandport);
			row_addr >>= 8;
		}
	}
}
__bank4 unsigned int send_nand_command(struct nand_mcu_ops *mcu, struct msgdata_cmd *cmd)
{
	unsigned char command = cmd->command;
	unsigned int cmd_delay = cmd->cmddelay;
	unsigned int addr_delay = cmd->addrdelay;
	struct msgdata_cmd cmd0;
	int ret = 0;
	cmd0.offset = -1;
	__nand_cmd(command,mcu->common.nand_io);
	mcu_delay(cmd_delay);
	if(cmd->offset == cmd0.offset){
		nand_send_addr(mcu,-1,cmd->pageid);
	}else
		nand_send_addr(mcu,(int)(cmd->offset) * 512,cmd->pageid);
	mcu_delay(addr_delay);
	if(command == NAND_CMD_STATUS){
		__nand_status(ret, mcu->common.nand_io);
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
	__nand_cmd(NAND_CMD_RNDOUT, mcu->common.nand_io);
	nand_send_addr(mcu, offset, -1);
	__nand_cmd(NAND_CMD_RNDOUTSTART, mcu->common.nand_io);
	mcu_delay(mcu->nand_info.twhr2);
}

__bank4 void send_prog_random(struct nand_mcu_ops *mcu, int offset)
{
	__nand_cmd(NAND_CMD_RNDIN,mcu->common.nand_io);
	mcu_delay(mcu->nand_info.tcwaw);
	nand_send_addr(mcu, offset, -1);
	mcu_delay(mcu->nand_info.tadl);
}
