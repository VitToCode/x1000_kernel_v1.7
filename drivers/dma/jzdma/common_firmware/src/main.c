/*
 * main.c
 */
#include <common.h>
#include <pdma.h>
#include <nand.h>
#include <mcu.h>

#include <asm/jzsoc.h>


static noinline void mcu_init(void)
{
	/* clear mailbox irq pending */
	REG32(PDMAC_DMNMB) = 0;
	REG32(PDMAC_DMSMB) = 0;
	REG32(PDMAC_DMINT) = PDMAC_DMINT_S_IMSK;
	/* clear irq mask for channel irq */
	REG32(PDMAC_DCIRQM) = 0;
}

struct nand_mcu_ops mcu_ops;
static TaskNode tasklist[MCU_TASK_NUM];


static void tasklist_init(TaskNode *tasklist)
{
	int i = 0;
	for(i = 0; i < MCU_TASK_NUM; i++){
		tasklist[i].func = NULL;
		tasklist[i].msg.ops.flag = 0;
	}
}

static __bank4 noinline void mcu_common_init(struct mcu_common_msg *common)
{
	TaskMsgPipe *pipe = common->taskmsgpipe;
	common->totaltasknum = 0;
	common->completetasknum = 0;
	common->ret_index = 0;
	common->ret = (unsigned char *)(TCSM_BANK7 - 0x200);
	common->current_cs = -1;
	common->nand_io = 0;
	common->chan_descriptor = 0;
	/* init taskmsgpipe */
	pipe[0].msgdata_start = (struct task_msg *)(TCSM_BANK4 - 0xc0);
	pipe[0].msgdata_end = pipe[0].msgdata_start + MCU_TMPP;
	pipe[0].next = &pipe[1];
	pipe[1].msgdata_start = (struct task_msg *)TCSM_BANK4;
	pipe[1].msgdata_end = pipe[1].msgdata_start + MCU_TMPP;
	pipe[1].next = &pipe[0];
	/* init some pointer of taskmsg */
	common->taskmsg_cpipe = pipe;
	common->taskmsg_index = NULL;
}
static void mcu_sleep(struct nand_mcu_ops *mcu)
{
	__pdma_irq_disable();
	__pdma_mwait();
	__pdma_irq_enable();
}
extern volatile unsigned int mcurunflag;
int main(void)
{
	struct nand_mcu_ops *mcu = &mcu_ops;
	int i = 0;
	unsigned int ret0 = TASK_FINISH, ret1 = TASK_WAIT;
	unsigned int taskresource = 0;
//	union ops_resource opsresource;
	TaskNode *task = NULL;
	mcurunflag = 0;
	mcu_ops.nand_info.rb0 = 0xff;
	mcu_ops.nand_info.rb1 = 0xff;
	mcu->pipe[0].pipe_data = (unsigned char *)TCSM_BANK5;
	mcu->pipe[1].pipe_data = (unsigned char *)TCSM_BANK6;
	mcu_common_init(&(mcu->common));
	tasklist_init(tasklist);
	mcu->wait_task = tasklist;
	mcu_init();
	__pdma_irq_enable();

	task = mcu->wait_task;
	while(1){
		taskresource = 0;
		ret1 = mcu_taskmanager(&mcu_ops);
		for(i = 0; i < MCU_TASK_NUM; i++){
			if(mcu->wait_task[i].func){
				ret0 = task[i].func(&(task[i].msg),i);
				if(ret0 == TASK_FINISH){
					task[i].func = NULL;
					task[i].msg.ops.flag = 0;
				}
			}
		}
#if 0
		if(ret < 0 ){
			mcu_nand_reset(mcu, MB_MCU_ERROR);
			for(i = 0; i < MCU_TASK_NUM; i++)
				task[i].func = NULL;
			mcu_sleep(mcu);
		}else
#else
		{
#endif
			for(i = 0; i < MCU_TASK_NUM; i++){
				if(task[i].func != NULL)
					taskresource |= task[i].msg.ops.bits.resource;
			}
			if(!taskresource && ret1 == TASK_FINISH){
				mcu_nand_reset(mcu, MB_MCU_DONE);
				mcu_sleep(mcu);
			}
//			else{
//				opsresource.flag = get_resource();
//				if(!__resource_satisfy(opsresource.flag, taskresource)){
//					mcu_sleep(mcu);
//				}
//			}
		}
	}
	return 0;
}
