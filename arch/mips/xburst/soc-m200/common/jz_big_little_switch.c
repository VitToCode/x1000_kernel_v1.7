#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/opp.h>
#include <linux/cpu.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/syscalls.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>

#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <jz_proc.h>

#include <asm/cpu.h>
#include <smp_cp0.h>

int _regs_stack[64];
int _tlb_entry_regs[32 * 3 + 4];
int _ready_flag;
int _wait_flag;

spinlock_t switch_lock;
struct clk *p0_clk;
struct clk *p1_clk;
static unsigned int little_max, big_min;
static unsigned int cpu_proc;

extern long long save_goto(unsigned int);
extern void switch_cpu(void);
extern void _start_secondary(void);

int change_cpu_init(unsigned int freqs_max, unsigned int freqs_min)
{
	unsigned int scpu_start_addr = 0, i, j;
	unsigned int *tmp;
	unsigned long flags;

#define MAX_REQUEST_ALLOC_SIZE 4096
	tmp = (unsigned int *)kmalloc(MAX_REQUEST_ALLOC_SIZE, GFP_KERNEL);

	local_irq_save(flags);
	for (i = 0; i < MAX_REQUEST_ALLOC_SIZE / 4; i++) {
		tmp[i] = __get_free_page(GFP_ATOMIC | GFP_DMA);
		if (tmp[i] && !(tmp[i] & 0xffff))
			break;
	}
	if (i < MAX_REQUEST_ALLOC_SIZE / 4)
		scpu_start_addr = tmp[i];
	for (j = 0; j < i; j++)
		free_page(tmp[j]);
	local_irq_restore(flags);

	printk("the secondary cpu base addr is %x\n", scpu_start_addr);
	BUG_ON(scpu_start_addr == 0);
	kfree(tmp);

	memcpy((void *)scpu_start_addr, (void *)_start_secondary, 0xC0 * 4);
	/* we need flush L2CACHE */
	dma_cache_sync(NULL, (void *)scpu_start_addr, 0x300, DMA_TO_DEVICE);

	set_smp_reim((scpu_start_addr & 0xffff0000) | 0x1ff);

	spin_lock_init(&switch_lock);

	p0_clk = clk_get(NULL, "p0");
	if (IS_ERR(p0_clk)) {
		printk("get p0clk fail!\n");
		return -1;
	}
	clk_enable(p0_clk);

	p1_clk = clk_get(NULL, "p1");
	if (IS_ERR(p1_clk)) {
		printk("get p0clk fail!\n");
		clk_put(p0_clk);
		return -1;
	}

	little_max = freqs_max;
	big_min = freqs_min;

	return 0;
}

static int power_on_another_cpu(int cpu)
{
	if (0 == cpu)
		clk_enable(p1_clk);
	else
		clk_enable(p0_clk);

	return 0;
}

static int power_down_cpu(int cpu)
{
	if (0 == cpu)
		clk_disable(p0_clk);
	else
		clk_disable(p1_clk);

	return 0;
}

static int start_another_core(int cpu)
{
	volatile unsigned int ctrl;

	/*
	 * first we save the CTRL.RPC and set RPC = 1
	 * set SW_RSTX = 1 keep reset another cpu
	 */
	ctrl = get_smp_ctrl();
	_regs_stack[51] = ctrl & (0x3 << 8);
	ctrl |= (0x3 << 8);

	if (0 == cpu)
		ctrl |= 0x2;
	else
		ctrl |= 0x1;
	set_smp_ctrl(ctrl);

	/*
	 * then start another cpu
	 * hardware request!!!
	 */
	ctrl = get_smp_ctrl();
	if(0 == cpu)
		ctrl &= ~(0x2);
	else
		ctrl &= ~(0x1);
	set_smp_ctrl(ctrl);

	return 0;
}

int is_need_switch(struct clk *cpu_clk, unsigned int freqs_new,
		   unsigned int freqs_old)
{
	volatile int cpu_no;
	int ret = 0, need_switch = 0;
#ifndef CONFIG_FPGA_TEST
	unsigned long flags;
#endif

	if(!cpu_proc)
		return need_switch;

	cpu_no = read_c0_ebase() & 1;
	if ((freqs_old >= big_min) && (freqs_new < big_min) && (cpu_no == 0)) {
		need_switch = 1;
		power_on_another_cpu(cpu_no);
#ifndef CONFIG_FPGA_TEST
		spin_lock_irqsave(&switch_lock, flags);
		ret = clk_set_rate(cpu_clk, big_min * 1000);
		spin_unlock_irqrestore(&switch_lock, flags);
#endif
		start_another_core(cpu_no);
	} else if ((freqs_old <= little_max) && (freqs_new > little_max) && (cpu_no == 1)) {
		need_switch = 1;
		power_on_another_cpu(cpu_no);
#ifndef CONFIG_FPGA_TEST
		spin_lock_irqsave(&switch_lock, flags);
		ret = clk_set_rate(cpu_clk, little_max * 1000);
		spin_unlock_irqrestore(&switch_lock, flags);
#endif
		start_another_core(cpu_no);
	}
	if(ret < 0) {
		printk("clk set rate fail\n");
		return ret;
	}
	return need_switch;
}

void jz_switch_cpu(void)
{
	unsigned long flags;
	int cpu_no;

	cpu_no = read_c0_ebase() & 1;

	spin_lock_irqsave(&switch_lock, flags);
	save_goto((unsigned int)switch_cpu);
	spin_unlock_irqrestore(&switch_lock, flags);
	power_down_cpu(cpu_no);

	printk("after:current epc = 0x%x\n", (unsigned int)read_c0_epc());
	printk("after:current lcr1 = 0x%x\n", *(unsigned int*)(0xb0000004));
}

/* ------------------------------cpu switch proc------------------------------- */
static void get_str_from_user(unsigned char *str, int strlen,
			      const char *buffer, unsigned long count)
{
	int len = count > strlen-1 ? strlen-1 : count;
	int i;

	if(len == 0) {
		str[0] = 0;
		return;
	}
	copy_from_user(str,buffer,len);
	str[len] = 0;
	for(i = len;i >= 0;i--) {
		if((str[i] == '\r') || (str[i] == '\n'))
			str[i] = 0;
	}
}

#include <soc/base.h>
#define PART_OFF	0x20
#define IMR_OFF		(0x04)
/* #define DDR_AUTOSR_EN   (0xb34f0304) */
/* #define DDR_DDLP        (0xb34f00bc) */
/* #define REG32_ADDR(addr) *((volatile unsigned int*)(addr)) */
static void cpu_entry_wait(void)
{
	unsigned int imr0_val, imr1_val;
	unsigned int *imr0_addr, *imr1_addr;
	void __iomem *intc_base;

	intc_base = ioremap(INTC_IOBASE, 0xfff);
	if(!intc_base) {
		printk("cpu switch ioremap intc reg addr error\n");
		return;
	}

	imr0_addr = intc_base + IMR_OFF;
	imr1_addr = intc_base + PART_OFF + IMR_OFF;
	/* save interrupt */
	imr0_val = readl(imr0_addr);
	imr1_val = readl(imr1_addr);

	/* mask all interrupt except GPIO */
	writel(0xfffc0fff, imr0_addr);
	writel(0xffffffff, imr1_addr);
	/* REG32_ADDR(DDR_AUTOSR_EN) = 0; */
	/* udelay(100); */
	/* REG32_ADDR(DDR_DDLP) = 0x0000f003; */
	__asm__ __volatile__(
		"wait \n\t"
		);
	/* REG32_ADDR(DDR_DDLP) = 0; */
	/* udelay(100); */
	/* REG32_ADDR(DDR_AUTOSR_EN) = 1; */

	/* restore  interrupt */
	writel(imr0_val, imr0_addr);
	writel(imr1_val, imr1_addr);

	iounmap(intc_base);
}
static int cpu_wait_proc_write(struct file *file, const char __user *buffer,
			       unsigned long count, void *data)
{
	char str[10];

	get_str_from_user(str,10,buffer,count);
	if(strlen(str) > 0) {
		if (strncmp(str, "wait", 5) == 0) {
			cpu_entry_wait();
		}
	}
	return count;
}
static int cpu_switch_read_proc(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
	int cpu_no, len;

	cpu_no = read_c0_ebase() & 1;
	len = sprintf(page,"current cpu status is %s cpu\n", !cpu_no? "big" : "little");

	return len;
}

static int cpu_switch_write_proc(struct file *file, const char __user *buffer,unsigned long count, void *data)
{
	if(count && (buffer[0] == '1'))
		cpu_proc = 1;
	else if(count && (buffer[0] == '0'))
		cpu_proc = 0;
	else
		printk("\"echo 1 > cpu_switch\" or \"echo 0 > cpu_switch \" ");
	return count;
}

static int __init cpu_switch_proc(void)
{
	struct proc_dir_entry *res,*p;

	p = jz_proc_mkdir("cpu_switch");
	if (!p) {
		pr_warning("create_proc_entry for common cpu switch failed.\n");
		return -ENODEV;
	}
	res = create_proc_entry("cpu_switch", 0600, p);
	if (res) {
		res->read_proc = cpu_switch_read_proc;
		res->write_proc = cpu_switch_write_proc;
		res->data =(void *)p;
		cpu_proc = 1;
	}
	res = create_proc_entry("wait", 0600, p);
	if (res) {
		res->read_proc = NULL;
		res->write_proc = cpu_wait_proc_write;
		res->data = (void *)p;
	}

	return 0;
}

module_init(cpu_switch_proc);
