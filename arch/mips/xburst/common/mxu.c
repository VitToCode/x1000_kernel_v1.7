#include <asm/processor.h>
#include <linux/sched.h>
#include <asm/mxu.h>

void __save_mxu(void *tsk_void)
{
	struct task_struct *tsk = tsk_void;
	register unsigned int reg_val asm("v0");
	asm volatile(".word	0x7002042e \n\t");
	tsk->thread.mxu.regs[0] = reg_val;
	asm volatile(".word	0x7002006e \n\t");
	tsk->thread.mxu.regs[1] = reg_val;
	asm volatile(".word	0x700200ae \n\t");
	tsk->thread.mxu.regs[2] = reg_val;
	asm volatile(".word	0x700200ee \n\t");
	tsk->thread.mxu.regs[3] = reg_val;
	asm volatile(".word	0x7002012e \n\t"); 
	tsk->thread.mxu.regs[4] = reg_val;
	asm volatile(".word	0x7002016e \n\t");		
	tsk->thread.mxu.regs[5] = reg_val;
	asm volatile(".word	0x700201ae \n\t");
	tsk->thread.mxu.regs[6] = reg_val;
	asm volatile(".word	0x700201ee \n\t");
	tsk->thread.mxu.regs[7] = reg_val;
	asm volatile(".word	0x7002022e \n\t");
	tsk->thread.mxu.regs[8] = reg_val;
	asm volatile(".word	0x7002026e \n\t");
	tsk->thread.mxu.regs[9] = reg_val;
	asm volatile(".word	0x700202ae \n\t");
	tsk->thread.mxu.regs[10] = reg_val;
	asm volatile(".word	0x700202ee \n\t");
	tsk->thread.mxu.regs[11] = reg_val;
	asm volatile(".word	0x7002032e \n\t");
	tsk->thread.mxu.regs[12] = reg_val;
	asm volatile(".word	0x7002036e \n\t");
	tsk->thread.mxu.regs[13] = reg_val;
	asm volatile(".word	0x700203ae \n\t");
	tsk->thread.mxu.regs[14] = reg_val;
	asm volatile(".word	0x700203ee \n\t");
	tsk->thread.mxu.regs[15] = reg_val;
}

void __restore_mxu(void * tsk_void)
{
	struct task_struct *tsk = tsk_void;
	volatile register unsigned int reg_val asm("v0");

	reg_val = tsk->thread.mxu.regs[0];
	asm volatile(".word	0x7002042f \n\t"::"r"(reg_val));
	reg_val = tsk->thread.mxu.regs[1];
	asm volatile(".word	0x7002006f \n\t"::"r"(reg_val));
	reg_val = tsk->thread.mxu.regs[2];
	asm volatile(".word	0x700200af \n\t"::"r"(reg_val));
	reg_val = tsk->thread.mxu.regs[3];
	asm volatile(".word	0x700200ef \n\t"::"r"(reg_val));
	reg_val = tsk->thread.mxu.regs[4];
	asm volatile(".word	0x7002012f \n\t"::"r"(reg_val));
	reg_val = tsk->thread.mxu.regs[5];
	asm volatile(".word	0x7002016f \n\t"::"r"(reg_val));
	reg_val = tsk->thread.mxu.regs[6];
	asm volatile(".word	0x700201af \n\t"::"r"(reg_val));
	reg_val = tsk->thread.mxu.regs[7];
	asm volatile(".word	0x700201ef \n\t"::"r"(reg_val));
	reg_val = tsk->thread.mxu.regs[8];
	asm volatile(".word	0x7002022f \n\t"::"r"(reg_val));
	reg_val = tsk->thread.mxu.regs[9];
	asm volatile(".word	0x7002026f \n\t"::"r"(reg_val));
	reg_val = tsk->thread.mxu.regs[10];
	asm volatile(".word	0x700202af \n\t"::"r"(reg_val));
	reg_val = tsk->thread.mxu.regs[11];
	asm volatile(".word	0x700202ef \n\t"::"r"(reg_val));
	reg_val = tsk->thread.mxu.regs[12];
	asm volatile(".word	0x7002032f \n\t"::"r"(reg_val));
	reg_val = tsk->thread.mxu.regs[13];
	asm volatile(".word	0x7002036f \n\t"::"r"(reg_val));
	reg_val = tsk->thread.mxu.regs[14];
	asm volatile(".word	0x700203af \n\t"::"r"(reg_val));
	reg_val = tsk->thread.mxu.regs[15];
	asm volatile(".word	0x700203ef \n\t"::"r"(reg_val));
}

