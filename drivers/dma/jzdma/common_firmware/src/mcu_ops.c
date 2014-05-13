/*
 * irq_pdma.c
 */
#include <common.h>
#include <pdma.h>
#include <nand.h>
#include <mcu.h>

#include <asm/jzsoc.h>

/**
 * if random has no effect, we need to
 * use this macro to fix the dma trans.
*/
#define DATA_4BYTES_ALIGN

//#define FILL_UP_WRITE_PAGE

#define MCU_NAND_USE_PN
#define USE_EDGE_IRQ

struct s_mailbox{
	unsigned short msg;
	unsigned short type;
};
union u_mailbox{
	struct s_mailbox mb;
	unsigned int     mbox;
};

#define IRQ_LEVEL_HIGH 1
#define IRQ_LEVEL_LOW 0
#define MCUBADBLOCKSIZE 4

#define MAILBOX_NAND (*(volatile unsigned int *)(0xf4000020))
#define MAILBOX_GPIO (*(volatile unsigned int *)(0xf4000024))
#define MAILBOX_GPIO_PEND_ADDR0 ((volatile unsigned int *)0xf4000028)
#define MAILBOX_GPIO_PEND_ADDR5 ((volatile unsigned int *)0xf4000040))
static void (*mcu_write_port)(void *,int);
volatile unsigned int mcurunflag = 0;
static volatile union u_mailbox mailbox;
volatile OpsResource g_opsresource;
static unsigned short mcu_retvalue;
static unsigned char report[2];
static unsigned int g_writeoffset;
//static unsigned int filldata = 0xaa55aa55;
#ifdef FILL_UP_WRITE_PAGE
static unsigned int badblockdata = 0xffffffff;
#endif //FILL_UP_WRITE_PAGE

extern struct nand_mcu_ops mcu_ops;

void mcu_delay(unsigned int cycle)
{
	__asm__ __volatile__ (
			"       .set    noreorder           \n"
			"       .align  3                   \n"
			"1:     bnez    %0, 1b              \n"
			"       subu    %0, 1               \n"
			"       .set    reorder             \n"
			: "=r" (cycle)
			: "0" (cycle));
}

void trap_entry(void)
{
	unsigned int intctl;
	int i;
	unsigned int pend,mark,intc;
	unsigned int rb0_pend, rb1_pend,is_rb0,is_rb1;
	unsigned int dcirqp;
	intctl = __pdma_read_cp0(12, 1);

	if (intctl & MCU_CHANNEL_IRQ) {
		dcirqp = REG32(PDMAC_DCIRQP);
		for (i = 0; i < 5; i++) {
			/* check MCU channel irq */
			if (dcirqp & ( 1 << i)) {
				switch(i) {
				case PDMA_BCH_CHANNEL:
					g_opsresource.bits.chan0 = AVAILABLE;
					break;
				case PDMA_IO_CHANNEL:
					g_opsresource.bits.chan1 = AVAILABLE;
					break;
				case PDMA_DDR_CHANNEL:
					g_opsresource.bits.chan2 = AVAILABLE;
					break;
				case PDMA_MSG_CHANNEL:
				case PDMA_MOVE_CHANNEL:
					g_opsresource.bits.msgflag += INCREASE;
					break;
				default: break;
				}
				REG32(PDMAC_DCCS(i)) = 0;
			}
		}
	}
	if(intctl & MCU_INTC_IRQ){
		mark = REG_GPIO_PXMASK(0);
		pend = REG_GPIO_PXFLG(0);
		intc = REG_GPIO_PXINT(0);
		pend &= (~mark & intc);
		is_rb0 = (mcu_ops.nand_info.rb0 != 0xff);
		is_rb1 = (mcu_ops.nand_info.rb1 != 0xff);
		rb0_pend = is_rb0 ? pend & (0x1 << mcu_ops.nand_info.rb0) : 0;
		rb1_pend = is_rb1 ? pend & (0x1 << mcu_ops.nand_info.rb1) : 0;
		if (pend) {
			if(mcurunflag && (rb0_pend || rb1_pend)){
				unsigned int gpin1,gpin2,gpin;
				while(rb0_pend || rb1_pend) {
					gpin1 = REG_GPIO_PXPIN(0);
					if(rb0_pend) {
						REG_GPIO_PXFLGC(0) = 0x1 << mcu_ops.nand_info.rb0;
						g_opsresource.bits.rb0 = AVAILABLE;
						g_opsresource.bits.setrb0 = GOTRB;
					}
					if(rb1_pend) {
						REG_GPIO_PXFLGC(0) = 0x1 << mcu_ops.nand_info.rb1;
						g_opsresource.bits.rb1 = AVAILABLE;
						g_opsresource.bits.setrb1 = GOTRB;

					}
					gpin2 = REG_GPIO_PXPIN(0);
					gpin = gpin1 ^ gpin2;
					rb0_pend = is_rb0 ? (gpin2 & gpin) & (1 <<  mcu_ops.nand_info.rb0) : 0;
					rb1_pend = is_rb1 ? (gpin2 & gpin) & (1 <<  mcu_ops.nand_info.rb1) : 0;
				}
			} else {
				REG_GPIO_PXMASKS(0) = pend;
				mailbox.mb.type = MCU_MSG_INTC;
				MAILBOX_GPIO = mailbox.mbox;
				MAILBOX_GPIO_PEND_ADDR0[0] = pend;
				REG32(PDMAC_DMNMB) = mailbox.mbox;
			}
		}
	}
}

static void set_resource(Rbit bit, int state)
{
	__pdma_irq_disable();
	switch(bit){
		case R_RB0:
			g_opsresource.bits.rb0 = state;
			g_opsresource.bits.setrb0 = WAITRB;
			break;
		case R_RB1:
			g_opsresource.bits.rb1 = state;
			g_opsresource.bits.setrb1 = WAITRB;
			break;
		case R_NEMC:
			g_opsresource.bits.nemc = state;
			break;
		case R_CHAN0:
			g_opsresource.bits.chan0 = state;
			break;
		case R_CHAN1:
			g_opsresource.bits.chan1 = state;
			break;
		case R_CHAN2:
			g_opsresource.bits.chan2 = state;
			break;
		case R_PIPE0:
			g_opsresource.bits.pipe0 = state;
			break;
		case R_PIPE1:
			g_opsresource.bits.pipe1 = state;
			break;
		case R_MSGFLAG:
			g_opsresource.bits.msgflag += state;
			break;
		case R_ALLBITS:
			g_opsresource.flag = state;
			break;
		default:
			break;
	}
	__pdma_irq_enable();
}
static void mcu_write_port_8bit(void *src, int len)
{
	int i;
	for(i = 0; i < len; i++)
		REG8(mcu_ops.common.nand_io) = ((unsigned char *)src)[i];
}
static void mcu_write_port_16bit(void *src, int len)
{
	int i;
	for(i = 0; i < (len >> 1); i++)
		REG16(mcu_ops.common.nand_io) = ((unsigned short *)src)[i];
}
unsigned __bank4 int get_resource(void)
{
	unsigned int ret;
	__pdma_irq_disable();
	ret = g_opsresource.flag;
	__pdma_irq_enable();
	return ret;
}

static __bank4 char set_ret_value(unsigned char *array, unsigned int index, unsigned char value)
{
	char ret = -1;
	unsigned int row,col0,col1;
	if(array){
		row = index / 2;
		col0 = 4 * (index % 2 ? 1 : 0);
		col1 = col0 ? 0 : 4;
		array[row] = (array[row] & (0xf << col1)) | (value << col0);
	}
	mcu_retvalue |= value;
	return ret;
}
static inline void send_mailbox_to_cpu(unsigned short msg)
{
	__pdma_irq_disable();
	mcurunflag = 0;
	mailbox.mb.type = MCU_MSG_NORMAL;
	mailbox.mb.msg = msg;
	while(REG32(PDMAC_DMINT) & PDMAC_DMINT_N_IP);
//	(*(((unsigned int *)(MCU_TEST_DATA))+6))++;
	MAILBOX_NAND = mailbox.mbox;
	REG32(PDMAC_DMNMB) = mailbox.mbox;
	__pdma_irq_enable();
}

static void mcu_nand_init(struct nand_mcu_ops *mcu, int context)
{
	struct taskmsg_init *msg = (struct taskmsg_init *)context;
	NandChip *nandinfo = &(mcu->nand_info);
	unsigned char *dest = (unsigned char *)nandinfo;
	unsigned char *src = (unsigned char *)(&msg->info);
	int i;
	for(i = 0; i < sizeof(NandChip); i++){
		dest[i] = src[i];
	}

	mcu->pipe[0].pipe_par = mcu->pipe[0].pipe_data + nandinfo->eccsize;
	mcu->pipe[1].pipe_par = mcu->pipe[1].pipe_data + nandinfo->eccsize;
	if(nandinfo->buswidth == 16)
		mcu_write_port = mcu_write_port_16bit;
	else
		mcu_write_port = mcu_write_port_8bit;
	/* some resources of mcu inits */
	set_resource(R_ALLBITS,0xff);
}

static inline int mcu_nand_prepare(struct nand_mcu_ops *mcu, int context)
{
	struct task_msg *msg = (struct task_msg *)context;
	int ret = TASK_WAIT;
	unsigned int resource = get_resource();
	if(__resource_satisfy(resource,msg->ops.bits.resource)){
		mcu->common.totaltasknum = msg->msgdata.prepare.totaltasknum;
		mcu->nand_info.ecclevel = msg->msgdata.prepare.eccbit;
		mcu->nand_info.eccbytes = msg->msgdata.prepare.eccbit * 14 / 8;
		mcu->common.completetasknum = 0;
		mcu->common.ret_index = 0;
		__pdma_irq_disable();
		mcurunflag = 1;
		__pdma_irq_enable();
		mcu_retvalue = 0;
		ret = TASK_FINISH;
	}
	return ret;
}

static void change_nand_cs(NandChip *nandinfo, struct mcu_common_msg *common, int id)
{
	if(common->current_cs != nandinfo->cs[id]){
		common->current_cs = nandinfo->cs[id];
		nand_enable(nandinfo,common->current_cs);
		common->nand_io = NEMC_CS(common->current_cs);
	}
}
static int mcu_nand_readdata(struct task_msg *msg, int pipe)
{
	struct msgdata_data *data = &(msg->msgdata.data);
	struct mcu_common_msg *common = &(mcu_ops.common);
	NandChip *nandinfo = &(mcu_ops.nand_info);
	int ret = TASK_WAIT;
	unsigned int cnt = 0;
	union ops_resource  resource;
	resource.flag = get_resource();
	while(__resource_satisfy(resource.flag, msg->ops.bits.resource)){
		switch(msg->ops.bits.state){
			case CHAN1_DATA:
				if(common->chan_descriptor & (0x1 << PDMA_IO_CHANNEL))
					goto readdata_tail;
				change_nand_cs(nandinfo, common, msg->ops.bits.chipsel);
				set_resource(R_CHAN1,UNAVAILABLE);
				set_resource(R_NEMC,UNAVAILABLE);
				if (nandinfo->pagesize != 512) {
#ifdef DATA_4BYTES_ALIGN
					send_read_random(&mcu_ops, (nandinfo->eccsize + (nandinfo->eccbytes+3)/4*4)*(data->offset / nandinfo->eccsize));
#else
					send_read_random(&mcu_ops, (nandinfo->eccsize + nandinfo->eccbytes)*(data->offset / nandinfo->eccsize));
#endif
				}

#ifdef MCU_NAND_USE_PN
				__pn_enable();
				__nand_counter0_enable();
#endif
#ifdef DATA_4BYTES_ALIGN
				/* if pagesize == 512, read all data of page and wait rb */
				if (nandinfo->pagesize == 512)
					pdma_nand_io_data(common->nand_io, (u32)(mcu_ops.pipe[pipe].pipe_data),
							    nandinfo->eccsize + (nandinfo->oobsize + 3) / 4 * 4, NEMC_TO_TCSM);
				else
					pdma_nand_io_data(common->nand_io, (u32)(mcu_ops.pipe[pipe].pipe_data),
							    nandinfo->eccsize + (nandinfo->eccbytes + 3) / 4 * 4, NEMC_TO_TCSM);
#else
				pdma_nand_io_data(common->nand_io, (u32)(mcu_ops.pipe[pipe].pipe_data),
						    nandinfo->eccsize + nandinfo->eccbytes, NEMC_TO_TCSM);
				/* if pagesize == 512, read all data of page and wait rb */
				if (nandinfo->pagesize == 512)
					pdma_nand_io_data(common->nand_io, (u32)(mcu_ops.pipe[pipe].pipe_data),
							    nandinfo->eccsize + nandinfo->oobsize, NEMC_TO_TCSM);
				else
					pdma_nand_io_data(common->nand_io, (u32)(mcu_ops.pipe[pipe].pipe_data),
							    nandinfo->eccsize + nandinfo->eccbytes, NEMC_TO_TCSM);

#endif
				SET_CHAN_DESCRIPTOR(common->chan_descriptor, PDMA_IO_CHANNEL);
				/* the preparation of next step */
				msg->ops.bits.resource &= ~(0x01 << R_NEMC);
				msg->ops.bits.state = CHAN1_PARITY;
				/* if pagesize == 512, read all data of page and wait rb */
				if (nandinfo->pagesize == 512) {
					if(msg->ops.bits.resource & (0x01 << R_RB0))
						set_resource(R_RB0,UNAVAILABLE);
					else
						set_resource(R_RB1,UNAVAILABLE);
				}
				break;
			case CHAN1_PARITY:
				CLEAR_CHAN_DESCRIPTOR(common->chan_descriptor, PDMA_IO_CHANNEL);
				set_resource(R_NEMC,AVAILABLE);
				common->current_cs = -1;
				nand_disable(nandinfo);
#ifdef MCU_NAND_USE_PN
				__nand_read_counter(cnt);
				__nand_counter_disable();
				__pn_disable();
				if(cnt < nandinfo->ecclevel){
					set_ret_value(common->ret,common->ret_index,MSG_RET_EMPTY);
					common->ret_index++;
					ret = TASK_FINISH;
					goto readdata_tail;
				}
#endif
				/* the preparation of next step */
				msg->ops.bits.state = BCH_PREPARE;
				resource.flag = 0;
				resource.bits.chan0 = AVAILABLE;
				msg->ops.bits.resource = resource.flag;
				break;
			case BCH_PREPARE:
				if(common->chan_descriptor & (0x1 << PDMA_BCH_CHANNEL))
					goto readdata_tail;
				set_resource(R_CHAN0,UNAVAILABLE);
				pdma_bch_decode_prepare(nandinfo, &(mcu_ops.pipe[pipe]));
				SET_CHAN_DESCRIPTOR(common->chan_descriptor, PDMA_BCH_CHANNEL);
				/* the preparation of next step */
				msg->ops.bits.state = BCH_FINISH;
				break;
			case BCH_FINISH:
				bch_decode_complete(nandinfo, mcu_ops.pipe[pipe].pipe_data, mcu_ops.pipe[pipe].pipe_par,&report[pipe]);
				CLEAR_CHAN_DESCRIPTOR(common->chan_descriptor, PDMA_BCH_CHANNEL);
				/* the preparation of next step */
				msg->ops.bits.state = CHAN2_DATA;
				resource.flag = 0;
				resource.bits.chan2 = AVAILABLE;
				msg->ops.bits.resource = resource.flag;
				break;
			case CHAN2_DATA:
				if(common->chan_descriptor & (0x1 << PDMA_DDR_CHANNEL))
					goto readdata_tail;
				set_resource(R_CHAN2,UNAVAILABLE);
				pdma_nand_ddr_data((u32)(mcu_ops.pipe[pipe].pipe_data + (data->offset % nandinfo->eccsize)),
						data->pdata, data->bytes, TCSM_TO_DDR);
				SET_CHAN_DESCRIPTOR(common->chan_descriptor, PDMA_DDR_CHANNEL);
				/* the preparation of next step */
				msg->ops.bits.state = CHAN2_FINISH;
				break;
			case CHAN2_FINISH:
				set_ret_value(common->ret,common->ret_index,report[pipe]);
				common->ret_index++;
				CLEAR_CHAN_DESCRIPTOR(common->chan_descriptor, PDMA_DDR_CHANNEL);
				ret = TASK_FINISH;
				goto readdata_tail;
			default:
				goto readdata_tail;
		}
		resource.flag = get_resource();
	}
readdata_tail:
	return ret;
}
static __bank4 int mcu_nand_writedata(struct task_msg *msg, int pipe)
{
	struct msgdata_data *data = &(msg->msgdata.data);
	struct mcu_common_msg *common = &(mcu_ops.common);
	NandChip *nandinfo = &(mcu_ops.nand_info);
	int ret = TASK_WAIT;
	union ops_resource resource;
	resource.flag = get_resource();
	while(__resource_satisfy(resource.flag, msg->ops.bits.resource)){
		switch(msg->ops.bits.state){
			case CHAN2_DATA:
				if(common->chan_descriptor & (0x1 << PDMA_DDR_CHANNEL))
					goto writedata_tail;
				set_resource(R_CHAN2,UNAVAILABLE);
				pdma_nand_ddr_data(data->pdata,
						(u32)(mcu_ops.pipe[pipe].pipe_data + data->offset % nandinfo->eccsize),
						data->bytes, DDR_TO_TCSM);
				SET_CHAN_DESCRIPTOR(common->chan_descriptor, PDMA_DDR_CHANNEL);
				/* the preparation of next step */
				msg->ops.bits.state = CHAN2_FINISH;
				break;
			case CHAN2_FINISH:
				CLEAR_CHAN_DESCRIPTOR(common->chan_descriptor, PDMA_DDR_CHANNEL);
				set_ret_value(common->ret,common->ret_index,MSG_RET_SUCCESS);
				common->ret_index++;
				if(msg->ops.bits.model == MCU_WRITE_DATA_WAIT){
					ret = TASK_FINISH;
					goto writedata_tail;
				}
				/* the preparation of next step */
				msg->ops.bits.state = BCH_PREPARE;
				resource.flag = 0;
				resource.bits.chan0 = AVAILABLE;
				msg->ops.bits.resource = resource.flag;
				break;
			case BCH_PREPARE:
				if(common->chan_descriptor & (0x1 << PDMA_BCH_CHANNEL))
					goto writedata_tail;
				set_resource(R_CHAN0,UNAVAILABLE);
				pdma_bch_encode_prepare(nandinfo, &(mcu_ops.pipe[pipe]));
				SET_CHAN_DESCRIPTOR(common->chan_descriptor, PDMA_BCH_CHANNEL);
				/* the preparation of next step */
				msg->ops.bits.state = BCH_FINISH;
				break;
			case BCH_FINISH:
				bch_encode_complete(nandinfo, &(mcu_ops.pipe[pipe]));
				CLEAR_CHAN_DESCRIPTOR(common->chan_descriptor, PDMA_BCH_CHANNEL);
				/* the preparation of next step */
				msg->ops.bits.state = CHAN1_DATA;
				resource.flag = 0;
				resource.bits.chan1 = AVAILABLE;
				resource.bits.nemc = AVAILABLE;
				msg->ops.bits.resource = resource.flag;
				break;
			case CHAN1_DATA:
				if(common->chan_descriptor & (0x1 << PDMA_IO_CHANNEL))
					goto writedata_tail;
				change_nand_cs(nandinfo, common, msg->ops.bits.chipsel);
				set_resource(R_CHAN1,UNAVAILABLE);
				set_resource(R_NEMC,UNAVAILABLE);
#ifdef DATA_4BYTES_ALIGN

				g_writeoffset = (nandinfo->eccsize + (nandinfo->eccbytes + 3)/4*4)*(data->offset / nandinfo->eccsize);
#else
				g_writeoffset = (nandinfo->eccsize + nandinfo->eccbytes)*(data->offset / nandinfo->eccsize);
#endif
				if (nandinfo->pagesize != 512)
					send_prog_random(&mcu_ops, g_writeoffset);
#ifdef MCU_NAND_USE_PN
				__pn_enable();
#endif
#ifdef DATA_4BYTES_ALIGN
				pdma_nand_io_data((u32)(mcu_ops.pipe[pipe].pipe_data), common->nand_io,
						    nandinfo->eccsize + (nandinfo->eccbytes + 3) / 4 * 4, TCSM_TO_NEMC);
#else
				pdma_nand_io_data((u32)(mcu_ops.pipe[pipe].pipe_data), common->nand_io,
						    nandinfo->eccsize + nandinfo->eccbytes, TCSM_TO_NEMC);

#endif
				SET_CHAN_DESCRIPTOR(common->chan_descriptor, PDMA_IO_CHANNEL);
				/* the preparation of next step */
				msg->ops.bits.resource &= ~(0x01 << R_NEMC);
				msg->ops.bits.state = CHAN1_PARITY;
				break;
			case CHAN1_PARITY:
				mcu_delay(nandinfo->tsync);
#ifdef MCU_NAND_USE_PN
				__pn_disable();
#endif
				CLEAR_CHAN_DESCRIPTOR(common->chan_descriptor, PDMA_IO_CHANNEL);
				set_resource(R_NEMC,AVAILABLE);
				ret = TASK_FINISH;
				goto writedata_tail;
			default:
				goto writedata_tail;
		}
		resource.flag = get_resource();
	}
writedata_tail:
	return ret;
}
static inline void mcu_copy(char *dest, char *src, int len)
{
	int i = 0;
	while(i<len){
		dest[i] = src[i];
		i++;
	}
}
static int mcu_nand_movedata(struct nand_mcu_ops *mcu,int context)
{
	struct task_msg *msg = (struct task_msg *)context;
	int ret = TASK_WAIT;
	TASKFUNC func;
	union ops_resource resource;
	int pipe = -1;
	resource.flag = msg->ops.bits.resource;
	if(resource.bits.pipe0)
		pipe = 0;
	else if(resource.bits.pipe1)
		pipe = 1;
	else
		pipe = -1;
	if(pipe == -1){
		ret = MSG_RET_FAIL;
		set_ret_value(mcu->common.ret,mcu->common.ret_index,ret);
		mcu->common.ret_index++;
		goto MOVEDATA_ERROR;
	}
	if(mcu->wait_task[pipe].func == NULL){
		if(msg->ops.bits.model == MCU_READ_DATA){
			func = mcu_nand_readdata;
		}else{
			func = mcu_nand_writedata;
		}
		mcu->wait_task[pipe].func = func;
		mcu_copy((char *)&(mcu->wait_task[pipe].msg),(char *)msg, sizeof(struct task_msg));
		ret = TASK_HANDLE;
	}
MOVEDATA_ERROR:
	return ret;
}

static unsigned int mcu_nand_sendcmd(struct nand_mcu_ops *mcu, int context)
{
	struct task_msg *msg = (struct task_msg *)context;
	unsigned int ret = TASK_WAIT;
	unsigned int msgret = 0 , i;
	union ops_resource resource;
	unsigned char command = msg->msgdata.cmd.command;
	NandChip *nandinfo = &(mcu_ops.nand_info);

	for(i = 0; i < MCU_TASK_NUM; i++){
		if(mcu->wait_task[i].func){
			msgret = 1;
			break;
		}
	}
	resource.flag = get_resource();
	if(!msgret && __resource_satisfy(resource.flag,msg->ops.bits.resource)){
		change_nand_cs(&(mcu->nand_info), &(mcu->common), msg->ops.bits.chipsel);

		/* if pagesize == 512, write 0x00 to reset pointer to 0 before program */
		if ((nandinfo->pagesize == 512) && (command == NAND_CMD_WRITE))
			send_nand_command(mcu, NAND_CMD_READ0);

#ifdef FILL_UP_WRITE_PAGE
		if(command == 0x10 || command == 0x11 || command == 0x15){
#ifdef DATA_4BYTES_ALIGN
			i = nandinfo->pagesize + nandinfo->oobsize - g_writeoffset - nandinfo->eccsize - (nandinfo->eccbytes+3) / 4 * 4;
#else
			i = nandinfo->pagesize + nandinfo->oobsize - g_writeoffset - nandinfo->eccsize - nandinfo->eccbytes;
#endif
			if(i){
#ifdef DATA_4BYTES_ALIGN
				send_prog_random(&mcu_ops, g_writeoffset + nandinfo->eccsize + (nandinfo->eccbytes + 3) / 4 * 4);
#else
				send_prog_random(&mcu_ops, g_writeoffset + nandinfo->eccsize + nandinfo->eccbytes);
#endif
				pdma_nand_io_parity((u32)(&badblockdata), mcu->common.nand_io, i, TCSM_TO_NEMC_FILL);
			}
		}
#endif //FILL_UP_WRITE_PAGE
		if(msg->ops.bits.model == MCU_WITH_RB){
			if(msg->ops.bits.resource & (0x01 << R_RB0))
				set_resource(R_RB0,UNAVAILABLE);
			else
				set_resource(R_RB1,UNAVAILABLE);
		}

		/* if pagesize == 512, do not send 0x30 when read */
		if (!((nandinfo->pagesize == 512) && (command == NAND_CMD_READSTART)))
			msgret = send_nand_command(mcu, &(msg->msgdata.cmd));
		set_ret_value(mcu->common.ret,mcu->common.ret_index,msgret);
		mcu->common.ret_index++;
		ret = TASK_FINISH;
	}
	return ret;
}

static int mcu_nand_moveret(struct nand_mcu_ops *mcu, int context)
{
	struct task_msg *msg = (struct task_msg *)context;
	int ret = TASK_WAIT;
	union ops_resource resource;
	int flag = 0,i;
	struct msgdata_ret *retdata = &(msg->msgdata.ret);
	for(i = 0; i < MCU_TASK_NUM; i++){
		if(mcu->wait_task[i].func){
			flag = 1;
			break;
		}
	}
	resource.flag = get_resource();
	if(!flag & __resource_satisfy(resource.flag,msg->ops.bits.resource)){
		set_resource(R_CHAN2,UNAVAILABLE);
		pdma_nand_ddr_data((u32)(mcu->common.ret), retdata->retaddr, retdata->bytes + 8, TCSM_TO_DDR);
		ret = TASK_FINISH;
	}
	return ret;
}
void mcu_nand_reset(struct nand_mcu_ops *mcu, unsigned short msg)
{
	union ops_resource resource;
	if(mcurunflag){
		/* wait for channel's finishing */
		resource.flag = get_resource();
		while(!__resource_satisfy(resource.flag,0xff)){
			resource.flag = get_resource();
		}

		mcu->common.taskmsgpipe[0].msgdata_start->ops.flag = -1;
		mcu->common.taskmsgpipe[1].msgdata_start->ops.flag = -1;
		mcu->common.taskmsg_cpipe = mcu->common.taskmsgpipe;
		mcu->common.taskmsg_index = NULL;
		mcu->common.chan_descriptor = 0;
		mcu->common.current_cs = -1;
		nand_disable(&(mcu->nand_info));
		/* some resources of mcu inits */
		set_resource(R_ALLBITS,0xff);
		send_mailbox_to_cpu(msg | mcu_retvalue);
	}
}
unsigned int mcu_taskmanager(struct nand_mcu_ops *mcu)
{
	struct mcu_common_msg *common = &(mcu->common);
	struct task_msg *msg = NULL;
	union ops_resource resource;
	unsigned int msgsrc_addr = 0;
	unsigned int ret = TASK_FINISH;
	static char flag = 0;
again:
	if(!common->taskmsg_index){
		resource.flag = get_resource();
		if(resource.bits.msgflag){
			set_resource(R_MSGFLAG,REDUCE);
			common->taskmsg_index = common->taskmsg_cpipe->msgdata_start;
			flag = 1;
		}
	}
	if(common->taskmsg_index){
		msg = common->taskmsg_index;
		switch(msg->ops.bits.type){
			case MSG_MCU_INIT:
				mcu_nand_init(mcu, (int)msg);
				pdma_channel_init();
				common->taskmsg_index = NULL;
				__pdma_irq_disable();
				mcurunflag = 1;
				__pdma_irq_enable();
				goto finish;
			case MSG_MCU_PREPARE:
				ret = mcu_nand_prepare(mcu,(int)msg);
				break;
			case MSG_MCU_CMD:
				ret = mcu_nand_sendcmd(mcu,(int)msg);
				break;
			case MSG_MCU_DATA:
				ret = mcu_nand_movedata(mcu,(int)msg);
				break;
			case MSG_MCU_RET:
				ret = mcu_nand_moveret(mcu,(int)msg);
				break;
			case MSG_MCU_PARITY:
				ret = mcu_nand_movedata(mcu,(int)msg);
				break;
			default: break;
		}
		if(flag && common->taskmsg_cpipe->next->msgdata_start->ops.flag == -1 &&
				common->totaltasknum - common->completetasknum > MCU_TMPP){
			flag = 0;
			msgsrc_addr = mcu->nand_info.taskmsgaddr + sizeof(struct task_msg)*(common->completetasknum + MCU_TMPP);
			pdma_nand_movetaskmsg(msgsrc_addr,(unsigned int)common->taskmsg_cpipe->next->msgdata_start,
					sizeof(struct task_msg) * MCU_TMPP);
		}
		if(ret != TASK_WAIT)
		{
			common->taskmsg_index++;
			common->completetasknum++;
			if(common->completetasknum == common->totaltasknum){
				common->taskmsg_index = NULL;
			}else if(common->taskmsg_index == common->taskmsg_cpipe->msgdata_end){
				common->taskmsg_index = NULL;
				common->taskmsg_cpipe->msgdata_start->ops.flag = -1;
				common->taskmsg_cpipe = common->taskmsg_cpipe->next;
				if(ret == TASK_FINISH)
					goto again;
			}else if(ret == TASK_FINISH)
				goto again;
		}
	}
finish:
	return ret;
}
