#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/syscore_ops.h>
#include <irq.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
//#include <linux/delay.h>

#include <asm/div64.h>

#include <soc/base.h>
#include <soc/extal.h>
#include <soc/gpio.h>
#include <soc/tcu.h>
#include <soc/irq.h>

#define regr(off) 	inl(TCU_IOBASE + (off))
#define regw(val,off)	outl(val, TCU_IOBASE + (off))

#define TCU_CNT_MAX (65535UL)
#define NR_TCU_CH 8
#define RESERVED_CH 5

static DECLARE_BITMAP(tcu_map, NR_TCU_CH) = { (1<<NR_TCU_CH) - 1, };

enum tcu_mode {
	TCU1_MODE = 1,
	TCU2_MODE = 2,
};

enum tcu_clk_mode {
	PCLK_EN = 0, /*timer input clock is PCLK*/
	RTC_EN = 1,/*timer input clock is RTC*/
	EXT_EN = 2,/*timer input clock is EXT*/
	CLK_MASK = 3,
};
enum tcu_irq_mode {
	FULL_IRQ_MODE = 1,
	HALF_IRQ_MODE = 2,
	FULL_HALF_IRQ_MODE = 3,
	NULL_IRQ_MODE = 4,
};
struct tcu_device {
	int id;
	short using;
	short pwm_flag;

	enum tcu_irq_mode irq_type;
	enum tcu_clk_mode clock;
	enum tcu_mode tcumode;
	int half_num;
	int full_num;
	int count_value;
	unsigned int init_level; /*used in pwm output mode*/
	unsigned int divi_ratio;
	unsigned int pwm_shutdown; /*0-->graceful shutdown   1-->abrupt shutdown only use in TCU1_MODE*/
	struct work_struct work;
} tcu_chs[NR_TCU_CH];

struct mutex lock;
static struct workqueue_struct *workqueue;

struct tcu_irq_list {
	int id;
	struct tcu_irq_list *next;
};
struct tcu_irq_list *list_irq[8];


static inline void dump_tcu_reg(void)
{
	int i = 0;
	printk("TCU_TSTR  : %08x\n", regr(TCU_TSTR )); 
	printk("TCU_TSTSR : %08x\n", regr(TCU_TSTSR)); 
	printk("TCU_TSTCR : %08x\n", regr(TCU_TSTCR)); 
	printk("TCU_TSR	  : %08x\n", regr(TCU_TSR  )); 
	printk("TCU_TSSR  : %08x\n", regr(TCU_TSSR )); 
	printk("TCU_TSCR  : %08x\n", regr(TCU_TSCR )); 
	printk("TCU_TER	  : %08x\n", regr(TCU_TER  )); 
	printk("TCU_TESR  : %08x\n", regr(TCU_TESR )); 
	printk("TCU_TECR  : %08x\n", regr(TCU_TECR )); 
	printk("TCU_TFR	  : %08x\n", regr(TCU_TFR  )); 
	printk("TCU_TFSR  : %08x\n", regr(TCU_TFSR )); 
	printk("TCU_TFCR  : %08x\n", regr(TCU_TFCR )); 
	printk("TCU_TMR	  : %08x\n", regr(TCU_TMR  )); 
	printk("TCU_TMSR  : %08x\n", regr(TCU_TMSR )); 
	printk("TCU_TMCR  : %08x\n", regr(TCU_TMCR )); 
	for(i=0; i<8; i++) {
		printk("CH_TDFR(%d): %08x\n",i, regr(CH_TDFR(i))); 
		printk("CH_TDHR(%d): %08x\n",i, regr(CH_TDHR(i)));	
		printk("CH_TCNT(%d): %08x\n",i, regr(CH_TCNT(i))); 
		printk("CH_TCSR(%d): %08x\n",i, regr(CH_TCSR(i))); 
	}
}


static void tcu_select_division_ratio(int id, unsigned int ratio)
{    /*division ratio 1/4/16/256/1024/mask->no internal input clock*/
	int tmp;
	tmp = regr(CH_TCSR(id));
	switch(ratio) {
	case 0: regw((tmp | CSR_DIV1),CH_TCSR(id)); break;
	case 1: regw((tmp | CSR_DIV4),CH_TCSR(id)); break;
	case 2: regw((tmp | CSR_DIV16),CH_TCSR(id)); break;
	case 3: regw((tmp | CSR_DIV64),CH_TCSR(id)); break;
	case 4: regw((tmp | CSR_DIV256),CH_TCSR(id)); break;
	case 5: regw((tmp | CSR_DIV1024),CH_TCSR(id)); break;
	default: 
		regw((tmp | CSR_DIV_MSK),CH_TCSR(id)); break;

	}
}

static void tcu_select_clk(int id,int clock)
{
	int tmp;
	tmp = regr(CH_TCSR(id));
	switch(clock) {
	case EXT_EN:  regw((tmp | CSR_EXT_EN),CH_TCSR(id));break;
	case RTC_EN:  regw((tmp | CSR_RTC_EN),CH_TCSR(id));break;
	case PCLK_EN: regw((tmp | CSR_PCK_EN),CH_TCSR(id));break;
	case CLK_MASK:
		regw((tmp & (~CSR_CLK_MSK)),CH_TCSR(id));
	}
}

static void set_tcu_counter_value(int id,int count_value)
{
	regw(count_value,CH_TCNT(id));
}

static void set_tcu_full_half_value(int id,unsigned int full_num,unsigned int half_num)
{
	regw(full_num,CH_TDFR(id));
	regw(half_num,CH_TDHR(id));	
}

void tcu_disable_counter(int id)
{
	regw(BIT(id),TCU_TECR);
} 
void tcu_enable_counter(int id)
{
	regw(BIT(id),TCU_TESR);
}

void tcu_stop_clock(int id)
{
	regw(BIT(id),TCU_TSSR);
}
void tcu_start_clock(int id)
{
	regw(BIT(id),TCU_TSCR);
}


void tcu_pwm_output_enable(int id)
{
	int tmp;
	tmp = regr(CH_TCSR(id));
	regw((tmp | TCSR_PWM_EN),CH_TCSR(id));
}
void tcu_pwm_output_disable(int id)
{
	int tmp = regr(CH_TCSR(id));
	regw(tmp & (~TCSR_PWM_EN),CH_TCSR(id));
}

void tcu_pwm_input_enable(int id)
{
	int tmp = regr(CH_TCSR(id));
	regw((tmp | (1 << 6)),CH_TCSR(id));
}

void tcu_pwm_input_disable(int id)
{
	int tmp = regr(CH_TCSR(id));
	regw(tmp & (~(1 << 6)),CH_TCSR(id));
}


void tcu_shutdown_counter(struct tcu_device *tcu)
{
	if(tcu->pwm_flag)
		tcu_pwm_output_disable(tcu->id); /*disable PWM_EN*/
	tcu_disable_counter(tcu->id);
	
	if(tcu->tcumode == TCU2_MODE)
		while(regr(TCU_TSTR) & (1 << tcu->id));
}

static void tcu_set_pwm_output_init_level(int id,int level)
{
	int tmp;
	tmp = regr(CH_TCSR(id));
	if(level){
		regw((tmp | TCSR_PWM_HIGH),CH_TCSR(id));
	}
	else {
		regw((tmp &(~TCSR_PWM_HIGH)),CH_TCSR(id));
	}
}

void tcu_set_pwm_shutdown(int id,unsigned int shutdown)/*only use in TCU1_MODE*/
{
	int tmp;
	tmp = regr(CH_TCSR(id));
	if(shutdown)
		regw((tmp | TCSR_PWM_SD),CH_TCSR(id));
	else {
		regw((tmp & (~TCSR_PWM_SD)),CH_TCSR(id));
	}

}

static void tcu_mask_full_match_irq(int id)
{
	regw(BIT(id),TCU_TMSR);
}
static void tcu_mask_half_match_irq(int id)
{
	regw(BIT((id) + 16),TCU_TMSR);	
}
static void tcu_unmask_full_match_irq(int id)
{
	regw(BIT(id),TCU_TMCR);
}
static void tcu_unmask_half_match_irq(int id)
{
	regw(BIT((id) + 16),TCU_TMCR);	
}

static struct tcu_irq_list *get_interrupt_flag(void)
{
	int tmp1 = 0;
	int tmp2 = 0;
	int i,num;
	struct tcu_irq_list *head;
	struct tcu_irq_list *p;
	head = (struct tcu_irq_list *)kzalloc(sizeof(struct tcu_irq_list),GFP_KERNEL);
	head->id = 8;
	head->next = NULL;
	p = head;
	tmp1 = regr(TCU_TFR);
	tmp2 = regr(TCU_TMR);
//	printk("--------get_interrupt_flag :flag = %08x\n",tmp1);
//	printk("--------get_interrupt_mask :mask = %08x\n",tmp2);
	i = num =0;
	while(i < 24) {
		if(!(tmp2 & (1 << i)) && (tmp1 & (1 << i))) {
			if(i <=7)  num =i;
			else if(i >=16 && i<=23) 
				num = i - 16;
			else {
				i ++;
				continue;
			}
			list_irq[num] = (struct tcu_irq_list *)kzalloc(sizeof(struct tcu_irq_list),GFP_KERNEL);
			p->next = list_irq[num];
			list_irq[num]->id = num;
			list_irq[num]->next = NULL;
			p = list_irq[num];		
		}
		i++;
	}
	return head;
}

void tcu_set_full_match_flags(int id)
{
	regw(BIT(id),TCU_TFSR);
}
void tcu_set_half_match_flags(int id)
{
	regw(BIT((id) + 16),TCU_TFSR);
}
static void tcu_clear_full_match_flags(int id)
{
	regw(BIT(id),TCU_TFCR);
}
static void tcu_clear_half_match_flags(int id)
{
	regw(BIT((id) + 16),TCU_TFCR);
}

static void tcu_clear_counter_to_zero(int id,enum tcu_mode mode)
{
	int tmp;
	tmp = regr(CH_TCSR(id));
	if(mode == TCU2_MODE) {
		regw((tmp | TCSR_CNT_CLRZ),CH_TCSR(id));
	}else {
		regw(0,CH_TCNT(id));	
	}
}


static void set_tcu_irq_mode(int id,enum tcu_irq_mode mode)
{
	switch(mode){
	case FULL_IRQ_MODE: 
		tcu_mask_half_match_irq(id);
		tcu_unmask_full_match_irq(id);
		break;
	case HALF_IRQ_MODE:
		tcu_mask_full_match_irq(id);
		tcu_unmask_half_match_irq(id);
		break;
	case FULL_HALF_IRQ_MODE:
		tcu_unmask_half_match_irq(id);
		tcu_unmask_full_match_irq(id);
		break;
	case NULL_IRQ_MODE:         /*PWM*/
		tcu_mask_full_match_irq(id);
		tcu_mask_half_match_irq(id);
	}
}

static void tcu_set_start_state(struct tcu_device *tcu)
{
	tcu_disable_counter(tcu->id);
	tcu_mask_full_match_irq(tcu->id);
	tcu_mask_half_match_irq(tcu->id);
	tcu_clear_full_match_flags(tcu->id);
	tcu_clear_half_match_flags(tcu->id);
	tcu_start_clock(tcu->id);
	regw(0,CH_TCSR(tcu->id));
	if(tcu->tcumode == TCU1_MODE)
		tcu_set_pwm_shutdown(tcu->id,tcu->pwm_shutdown);
	
}

static void tcu_set_close_state(struct tcu_device *tcu)
{
	tcu_mask_full_match_irq(tcu->id);
	tcu_mask_half_match_irq(tcu->id);	
	
	tcu_clear_full_match_flags(tcu->id);
	tcu_clear_half_match_flags(tcu->id);
        tcu_stop_clock(tcu->id);
	tcu_clear_counter_to_zero(tcu->id,tcu->tcumode);	

	tcu_chs[tcu->id].irq_type = NULL_IRQ_MODE;
	tcu_chs[tcu->id].clock = CLK_MASK;
	tcu_chs[tcu->id].pwm_flag = 0;
}

static irqreturn_t tcu_irqhandler(int irq,void *data)
{
	int i;
	struct tcu_irq_list *p,*q;

	p = get_interrupt_flag();
	q = p;
	p = p->next;
	q->next = NULL;
	kfree(q);
	while(p != NULL) {
		q = p;
		i = p->id;
		p = p->next;

		tcu_mask_full_match_irq(i);
		tcu_mask_half_match_irq(i);
		tcu_clear_full_match_flags(i);
		tcu_clear_half_match_flags(i);

		queue_work(workqueue,&tcu_chs[i].work);
		q->next = NULL;
		kfree(q);
	}
	return IRQ_HANDLED;
}

int __init tcu_init(void)
{
	int ret,i;
	unsigned int mask_bit = ~(1 << RESERVED_CH);

	workqueue = create_singlethread_workqueue("tcu_workqueue");
	if (!workqueue){
		printk(" ------tcu create workqueue fail!!!-------\n");
		ret = -ENOMEM;
		return ret;
	}
	mutex_init(&lock);
#ifndef RESERVED_CH
	if (0 != (ret =request_irq(IRQ_TCU1, tcu_irqhandler,IRQF_DISABLED, "tcu1",NULL))) {
		printk(KERN_INFO "tcu_timer :%s request irq error !\n ","tcu1");
		return ret;
	}
#endif
	if (0 != (ret = request_irq(IRQ_TCU2, tcu_irqhandler,IRQF_DISABLED, "tcu2", NULL))) {
		printk(KERN_INFO "tcu_timer :%s request irq error !\n ","tcu2");
		return ret;
	}

	/*mask all interrupts*/

	regw(mask_bit,TCU_TMSR);
	/*clear match flags*/
	regw(mask_bit,TCU_TFCR);
	/*stop all clocks*/
	regw(mask_bit,TCU_TSSR);
        /*clear half full counter to 0*/
	for(i = 0;i < 8;i++){
		if (i == RESERVED_CH)
			continue;
		set_tcu_full_half_value(i, 0, 0);
		set_tcu_counter_value(i,0);
	}
	return 0;
}

/*2 interrupts  chanel 0~4 & 6~7->2 , chanel 5->1  */
struct tcu_device *tcu_request(int channel_num,void (*channel_handler)(struct work_struct *))
{
	mutex_lock(&lock);
	if (channel_num < 0 || channel_num > NR_TCU_CH - 1) {
		mutex_unlock(&lock);
		return ERR_PTR(-ENODEV);
	}
	if (!test_bit(channel_num, tcu_map)) {
		mutex_unlock(&lock);
		return ERR_PTR(-EBUSY);
	}
	
	tcu_chs[channel_num].id = channel_num;

	if(NULL != channel_handler)
		INIT_WORK(&tcu_chs[channel_num].work,channel_handler);
	
	if(tcu_chs[channel_num].id == 1 || tcu_chs[channel_num].id == 2) 
		tcu_chs[channel_num].tcumode = TCU2_MODE;
	else 
		tcu_chs[channel_num].tcumode = TCU1_MODE;
	printk("request channel number:%d ------\n",channel_num);

	clear_bit(channel_num, tcu_map);
	mutex_unlock(&lock);
	return &tcu_chs[channel_num];

}

void tcu_free(struct tcu_device *tcu)
{
	tcu_set_close_state(tcu);
	set_bit(tcu->id, tcu_map);
}


int tcu_as_timer_config(struct tcu_device *tcu)
{
	if (tcu->id < 0 || tcu->id > NR_TCU_CH - 1){
		printk("numbuer isn't in 0~7\n");
		return -EINVAL;
	}
	if (test_bit(tcu->id, tcu_map)){
		printk("already been used!\n");
		return -EINVAL;
	}

	tcu_set_start_state(tcu);

	set_tcu_counter_value(tcu->id,tcu->count_value);
	set_tcu_full_half_value(tcu->id,tcu->full_num,tcu->half_num);

	set_tcu_irq_mode(tcu->id,tcu->irq_type);

	tcu_select_division_ratio(tcu->id,tcu->divi_ratio);
	if(tcu->pwm_flag)
		tcu_pwm_output_enable(tcu->id);
	tcu_select_clk(tcu->id,tcu->clock);

	if(tcu->init_level)
		tcu_set_pwm_output_init_level(tcu->id,1);

//	printk("tcu_control register = 0x%08x\n",regr(CH_TCSR(tcu->id)));
	return 0;
       
}

int tcu_as_counter_config(struct tcu_device *tcu)
{
	if (tcu->id < 0 || tcu->id > NR_TCU_CH - 1)
		return -EINVAL;
	if (test_bit(tcu->id, tcu_map))
		return -EINVAL;
	printk("-------tcu_config channel: %d--------\n", tcu->id);
	tcu_set_start_state(tcu);

	set_tcu_counter_value(tcu->id,tcu->count_value);
	set_tcu_full_half_value(tcu->id,tcu->full_num,tcu->half_num);

	tcu_select_division_ratio(tcu->id,0);
	set_tcu_irq_mode(tcu->id,tcu->irq_type);

	tcu_pwm_output_disable(tcu->id);
	tcu_select_clk(tcu->id,CLK_MASK);
	tcu_pwm_input_enable(tcu->id);

	return 0;
}

int tcu_enable(struct tcu_device *tcu)
{
	if (!tcu->using) {
		tcu->using = 1;
		tcu_enable_counter(tcu->id);
	}
	return 0;
}

void tcu_disable(struct tcu_device *tcu)
{
	if (!tcu || test_bit(tcu->id, tcu_map))
		return;

	if (tcu->using) {
		tcu->using = 0;
		tcu_shutdown_counter(tcu);
	}
}


int tcu_counter_read(struct tcu_device *tcu)
{
	int i = 0;
	int tmp = 0;
 	if (tcu->id < 0 || tcu->id > NR_TCU_CH - 1)
		return -EINVAL;
    
	if (tcu->tcumode == TCU2_MODE){
		while(tmp == 0 && i < 5) {
			tmp = regr(TCU_TSTR) & (1 << (tcu->id + 16));
			i++;
		}
		if(tmp == 0)	return -EINVAL; /*TCU MODE 2 may not read success*/
	}
    
	return (regr(CH_TCNT(tcu->id)));

}


arch_initcall(tcu_init);
