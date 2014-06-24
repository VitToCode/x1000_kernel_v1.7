/*
 *  CPU frequency scaling for JZ47XX SOCS
 *
 *  Copyright (C) 2012 Ingenic Corporation
 *  Written by ztyan<ztyan@ingenic.cn>
 *
 *  Based on cpu-sa1110.c, Copyright (C) 2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

 /* #define DEBUG */
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
#include <linux/slab.h>

#define CPUFREQ_NR	21
#define MIN_FREQ	50000
#define MIN_VOLT	1200000
#define MAX_MPLL_DIV	14			/* Max CPU MPLL div */
#ifndef CONFIG_TVOL_L
#define tVOL_L		30			/* ms of voltage latency */
#else
#define tVOL_L		CONFIG_TVOL_L
#endif
#ifndef CONFIG_TVOL_H
#define tVOL_H		500			/* ms of voltage hold time */
#else
#define tVOL_H		CONFIG_TVOL_H
#endif
#ifndef CONFIG_DETE_PERIOD
#define DETE_PERIOD	30000			/* freq detect period */
#else
#define DETE_PERIOD	CONFIG_DETE_PERIOD
#endif
#ifndef CONFIG_HIGH_THRESHOLD
#define HIGH_THRESHOLD	30			/* percent of high load threshold */
#else
#define HIGH_THRESHOLD	CONFIG_HIGH_THRESHOLD
#endif
#ifndef CONFIG_HIFREQ_MINUTE
#define HIFREQ_MINUTE	30			/* high freq last time */
#else
#define HIFREQ_MINUTE	CONFIG_HIFREQ_MINUTE
#endif
#ifndef CONFIG_MAX_APLL_FREQ
#define MAX_APLL_FREQ	1824000			/* max cpufreq from APLL */
#else
#define MAX_APLL_FREQ	CONFIG_MAX_APLL_FREQ
#endif
#ifndef CONFIG_LOW_APLL_FREQ
#define LOW_APLL_FREQ	1300000			/* low cpufreq from APLL */
#else
#define LOW_APLL_FREQ	CONFIG_LOW_APLL_FREQ
#endif
#ifndef CONFIG_APLL_FREQ_STEP
#define APLL_FREQ_STEP	96000			/* step of APLL freq */
#else
#define APLL_FREQ_STEP	CONFIG_APLL_FREQ_STEP
#endif

#define USE_PREPARE 0


struct cpufreq_frequency_table jz_freq_table[CPUFREQ_NR] = {
	{0,6000}, {1,8000},
	{2,12000}, {3,24000},
	{4,30000}, {5,40000},
	{6,48000}, {7,60000},
	{8,72000}, {9,80000},
	{10,96000}, {11,120000},
	{12,200000}, {13,300000},
	{14,400000}, {15,600000},
	{16,792000}, {17,912000},
	{18,1008000}, {19,1200000},
	{20,CPUFREQ_TABLE_END},
};

struct lpj_info {
	unsigned long	ref;
	unsigned int	freq;
};

static DEFINE_PER_CPU(struct lpj_info, lpj_ref);

struct jz_cpufreq {
	unsigned int vol_v;
	unsigned int freq;
	unsigned int freq_gate;
	unsigned int freq_high;
	spinlock_t freq_lock;

	unsigned int rate;

	int radical_cnt;
	unsigned long long timer_start;
	unsigned long long timer_end;
	unsigned long long radical_time;
	struct lpj_info global_lpj_ref;
	struct cpufreq_frequency_table freq_table[CPUFREQ_NR];
	struct clk *cpu_clk;
	struct regulator *cpu_regulator;
	struct delayed_work vol_work;

	struct task_struct *freq_thread;
	struct work_struct work;
	struct cpufreq_policy jz_policy;
};

struct jz_cpufreq *g_jz_cpufreq;

#ifdef CONFIG_JZ_BIG_LITTLE_SWITCH
extern int change_cpu_init(unsigned int freqs_max, unsigned int freqs_min);
extern int is_need_switch(struct clk *cpu_clk, unsigned int freqs_new,
			  unsigned int freqs_old);
extern void jz_switch_cpu(void);
#endif

unsigned long __attribute__((weak)) core_reg_table[12][2] = {
	{ 1584000,1400000 }, // >= 1.548 GHz - 1.40V
	{ 1300000,1350000 }, // >= 1.300 GHz - 1.35V
	{ 1200000,1200000 }, // >= 1.200 GHz - 1.20V
	{MIN_FREQ,MIN_VOLT},
};

/* static unsigned long regulator_find_voltage(int freqs) */
/* { */
/* 	int i = 0; */

/* 	while(core_reg_table[i][0] && core_reg_table[i][1]) { */
/* 		if(freqs >= core_reg_table[i][0]) */
/* 			return core_reg_table[i][1]; */
/* 		i++; */
/* 	} */
/* 	return 0; */
/* } */
#if USE_PREPARE
static int freq_table_prepare(struct jz_cpufreq *jzcpufreq)
{
	struct clk *sclka, *mpll, *apll, *cparent;
	unsigned int j,i = 0;
	unsigned int sclka_rate, mpll_rate, apll_rate;
	unsigned int max_rate = MAX_APLL_FREQ;

	sclka = clk_get(NULL,"sclka");
	if (IS_ERR(sclka)) {
		return -EINVAL;
	}
	mpll = clk_get(NULL,"mpll");
	if (IS_ERR(mpll)) {
		goto mpll_err;
	}
	apll = clk_get(NULL,"apll");
	if (IS_ERR(apll)) {
		goto apll_err;
	}

	sclka_rate = clk_get_rate(sclka) / 1000;
	apll_rate = clk_get_rate(apll) / 1000;
	mpll_rate = clk_get_rate(mpll) / 1000;
	cparent = clk_get_parent(jzcpufreq->cpu_clk);

	memset(jzcpufreq->freq_table, 0, sizeof(jzcpufreq->freq_table));

#define _FREQ_TAB(in, fr)				\
	jzcpufreq->freq_table[in].index = in;		\
	jzcpufreq->freq_table[in].frequency = fr	\

	if (clk_is_enabled(apll)) {
		jzcpufreq->freq_high = apll_rate;

		while (max_rate > LOW_APLL_FREQ) {
			_FREQ_TAB(i, max_rate);
			i++;
			max_rate -= APLL_FREQ_STEP;
		}
		_FREQ_TAB(i, 1008000);
		i++;
	} else {
		jzcpufreq->freq_high = mpll_rate;
	}

	jzcpufreq->freq_gate = jzcpufreq->freq_high;

	for (j = 1; (i < CPUFREQ_NR) && (jzcpufreq->freq_high / j >= MIN_FREQ)
		     && (j <= MAX_MPLL_DIV); i++) {
		_FREQ_TAB(i, jzcpufreq->freq_high / j++);
	}
	_FREQ_TAB(i, CPUFREQ_TABLE_END);
#undef _FREQ_TAB

	clk_put(apll);
	clk_put(sclka);
	clk_put(mpll);
#ifdef DEBUG
	pr_info("----Freq table:\n");
	for (i = 0; jzcpufreq->freq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
		printk("%u %u\n",jzcpufreq->freq_table[i].index,jzcpufreq->freq_table[i].frequency);
	}
#endif
	return 0;
mpll_err:
	clk_put(sclka);
apll_err:
	clk_put(mpll);
	return -EINVAL;
}
#endif
static int jz4785_verify_speed(struct cpufreq_policy *policy)
{
	struct jz_cpufreq *jz_cpufreq =	g_jz_cpufreq;

	return cpufreq_frequency_table_verify(policy, jz_cpufreq->freq_table);
}

static unsigned int jz4785_getspeed(unsigned int cpu)
{
	if (cpu >= NR_CPUS)
		return 0;
	return clk_get_rate(g_jz_cpufreq->cpu_clk) / 1000;
}

static int jz4785_target(struct cpufreq_policy *policy,
			 unsigned int target_freq,
			 unsigned int relation)
{
	unsigned int i;
	int ret = 0;

	struct cpufreq_freqs freqs;
	volatile unsigned long freq, flags;//, volt = 0;
	int this_cpu = smp_processor_id();
#ifdef CONFIG_JZ_BIG_LITTLE_SWITCH
	int need_switch = 0;
#endif
	struct jz_cpufreq *jz_cpufreq =	g_jz_cpufreq;

	ret = cpufreq_frequency_table_target(policy, jz_cpufreq->freq_table, target_freq, relation, &i);
	if (ret) {
		printk("%s: cpu%d: no freq match for %d(ret=%d)\n",
		       __func__, policy->cpu, target_freq, ret);
		return ret;
	}
	freqs.new = jz_cpufreq->freq_table[i].frequency;
	if (!freqs.new) {
		printk("%s: cpu%d: no match for freq %d\n", __func__,
		       policy->cpu, target_freq);
		return -EINVAL;
	}

	freqs.old = jz4785_getspeed(policy->cpu);
	freqs.cpu = policy->cpu;

	if (freqs.old == freqs.new && policy->cur == freqs.new)
		return ret;

#ifdef CONFIG_JZ_BIG_LITTLE_SWITCH
	need_switch = is_need_switch(jz_cpufreq->cpu_clk, freqs.new, freqs.old);
	if(need_switch < 0) {
		printk("%s:clk set rate fail\n", __func__);
		return need_switch;
	}
#endif
	freq = freqs.new * 1000;
	if ((freqs.old < jz_cpufreq->freq_gate) && (freqs.new > jz_cpufreq->freq_gate))
		freqs.new = jz_cpufreq->freq_gate;
#if 0
	if (jz_cpufreq->cpu_regulator && (freqs.new > freqs.old)) {
		int vol_t;

		volt = regulator_find_voltage(freqs.new);
		vol_t = jz_cpufreq->vol_v;

		if (volt > jz_cpufreq->vol_v) {
			cancel_delayed_work_sync(&jz_cpufreq->vol_work);
			vol_t = regulator_get_voltage(jz_cpufreq->cpu_regulator);
		}
		jz_cpufreq->vol_v = volt;
		while (vol_t < jz_cpufreq->vol_v) {
			vol_t += 50000;
			r = regulator_set_voltage(jz_cpufreq->cpu_regulator, vol_t, vol_t);
			if (r < 0) {
				pr_err("unable to scale voltage up. volt:%d\n", vol_t);
				freqs.new = freqs.old;
				goto done;
			}
			msleep(tVOL_L);
			if (vol_t >= jz_cpufreq->vol_v)
				break;
		}
	}
#endif
	/* notifiers */
	for_each_cpu(i, policy->cpus) {
		freqs.cpu = i;
		cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
	}

#ifdef CONFIG_JZ_BIG_LITTLE_SWITCH
	if(need_switch)
		jz_switch_cpu();
#endif
	spin_lock_irqsave(&jz_cpufreq->freq_lock, flags);
	ret = clk_set_rate(jz_cpufreq->cpu_clk, freqs.new * 1000);
	freqs.new = jz4785_getspeed(policy->cpu);
	if ((freqs.new > jz_cpufreq->freq_gate) && (freqs.old <= jz_cpufreq->freq_gate)) {
		jz_cpufreq->timer_start = cpu_clock(this_cpu);
	} else if ((freqs.new <= jz_cpufreq->freq_gate) && (freqs.old > jz_cpufreq->freq_gate)) {
		jz_cpufreq->timer_end = cpu_clock(this_cpu);
		jz_cpufreq->radical_time += jz_cpufreq->timer_end - jz_cpufreq->timer_start;
	}

	/*
	 * Note that loops_per_jiffy is not updated on SMP systems in
	 * cpufreq driver. So, update the per-CPU loops_per_jiffy value
	 * on frequency transition. We need to update all dependent CPUs.
	 */
	for_each_cpu(i, policy->cpus) {
		struct lpj_info *lpj = &per_cpu(lpj_ref, i);
		if (!lpj->freq) {
			lpj->ref = cpu_data[i].udelay_val;
			lpj->freq = freqs.old;
		}

		cpu_data[i].udelay_val = cpufreq_scale(lpj->ref, lpj->freq, freqs.new);
	}

	/* And don't forget to adjust the global one */
	if (!jz_cpufreq->global_lpj_ref.freq) {
		jz_cpufreq->global_lpj_ref.ref = loops_per_jiffy;
		jz_cpufreq->global_lpj_ref.freq = freqs.old;
	}
	loops_per_jiffy = cpufreq_scale(jz_cpufreq->global_lpj_ref.ref,
					jz_cpufreq->global_lpj_ref.freq, freqs.new);

	spin_unlock_irqrestore(&jz_cpufreq->freq_lock, flags);
#if 0
	if (jz_cpufreq->cpu_regulator && (freqs.new < freqs.old)) {
		volt = regulator_find_voltage(freqs.new);
		if (volt < jz_cpufreq->vol_v) {
			jz_cpufreq->vol_v = volt;
			schedule_delayed_work(&jz_cpufreq->vol_work,
					      msecs_to_jiffies(tVOL_H));
		}
	}
done:
#endif
	/* notifiers */
	for_each_cpu(i, policy->cpus) {
		freqs.cpu = i;
		cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
	}
	return ret;
}

static int freq_write(char *filename, char *data)
{
	int fd = sys_open(filename, O_RDWR, 0);
	unsigned count = strlen(data);

	if (fd < 0) {
		printk(KERN_WARNING "%s: Can not open %s\n",
		       __func__, filename);
		return -ENOENT;
	}
	if ((unsigned)sys_write(fd, (char *)data, count) != count) {
		printk(KERN_WARNING "%s: Can not write %s\n",
		       __func__, filename);
		return -EIO;
	}
	return 0;
}

static int freq_monitor(void *data)
{
	int this_cpu;
	unsigned int radical_t;
	struct jz_cpufreq *jz_cpufreq = (struct jz_cpufreq *)data;

	while (1) {
		spin_lock(&jz_cpufreq->freq_lock);
		this_cpu = smp_processor_id();

		if (jz_cpufreq->timer_end < jz_cpufreq->timer_start) {
			jz_cpufreq->timer_end = cpu_clock(this_cpu);
			jz_cpufreq->radical_time += jz_cpufreq->timer_end - jz_cpufreq->timer_start;
			jz_cpufreq->timer_start = cpu_clock(this_cpu);
		}
		do_div(jz_cpufreq->radical_time, 1000000);
		radical_t = (unsigned int)jz_cpufreq->radical_time;
		if (radical_t > (DETE_PERIOD * HIGH_THRESHOLD / 100)) {
			jz_cpufreq->radical_cnt++;
		} else {
			if (jz_cpufreq->radical_cnt > 0)
				jz_cpufreq->radical_cnt--;
		}
		pr_debug("radical_t=%ums radical_cnt=%d\n", radical_t, jz_cpufreq->radical_cnt);
		jz_cpufreq->radical_time = 0;
		spin_unlock(&jz_cpufreq->freq_lock);

		if (jz_cpufreq->radical_cnt > HIFREQ_MINUTE * 60000 / DETE_PERIOD) {
			char max_freq[10];
			sprintf(max_freq, "%u", jz_cpufreq->freq_gate);
			pr_info("set maxfreq to normal mode\n");
			freq_write("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", max_freq);
			jz_cpufreq->radical_cnt = 0;
		}
		msleep(DETE_PERIOD);
	}
	return 0;
}

static void vol_down_work(struct work_struct *work)
{
	int vol_t, ret;

	struct jz_cpufreq *jz_cpufreq = g_jz_cpufreq;

	vol_t = regulator_get_voltage(jz_cpufreq->cpu_regulator);
	if (vol_t > jz_cpufreq->vol_v) {
		vol_t -= 50000;
		ret = regulator_set_voltage(jz_cpufreq->cpu_regulator, vol_t, vol_t);
		if (ret < 0) {
			pr_err("unable to scale voltage down. volt:%d\n", jz_cpufreq->vol_v);
			return;
		}
		msleep(tVOL_L);
		if (vol_t > jz_cpufreq->vol_v)
			schedule_delayed_work(&jz_cpufreq->vol_work, msecs_to_jiffies(tVOL_L));
	}
}

static int __cpuinit jz4785_cpu_init(struct cpufreq_policy *policy)
{
	struct jz_cpufreq *jz_cpufreq;
#ifdef CONFIG_JZ_BIG_LITTLE_SWITCH
	int i;
	unsigned int freqs_max, max_flag;
	unsigned int freqs_min, freqs_tmp, min_flag;
#endif

	jz_cpufreq = (struct jz_cpufreq *)kzalloc(sizeof(struct jz_cpufreq), GFP_KERNEL);
	if(!jz_cpufreq) {
		pr_err("kzalloc fail!!!\n");
		return -1;
	}
	g_jz_cpufreq = jz_cpufreq;

	jz_cpufreq->cpu_clk = clk_get(NULL, "cclk");
	if (IS_ERR(jz_cpufreq->cpu_clk))
		goto cpu_clk_err;

#ifdef CONFIG_CPUFREQ_CHANGE_VCORE
	jz_cpufreq->cpu_regulator = regulator_get(NULL, "vcore");
	if (IS_ERR(jz_cpufreq->cpu_regulator)) {
		pr_warning("%s: unable to get CPU regulator\n", __func__);
		jz_cpufreq->cpu_regulator = NULL;
	} else {
		/*
		 * Ensure physical regulator is present.
		 * (e.g. could be dummy regulator.)
		 */
		jz_cpufreq->vol_v = regulator_get_voltage(jz_cpufreq->cpu_regulator);
		if (jz_cpufreq->vol_v < 0) {
			pr_warn("%s: physical regulator not present for CPU\n",
				__func__);
			regulator_put(jz_cpufreq->cpu_regulator);
			jz_cpufreq->cpu_regulator = NULL;
		}
	}
#else
	jz_cpufreq->cpu_regulator = NULL;
#endif

	spin_lock_init(&jz_cpufreq->freq_lock);

	memset(jz_cpufreq->freq_table,0,sizeof(jz_cpufreq->freq_table));
	memcpy(jz_cpufreq->freq_table,jz_freq_table,sizeof(jz_freq_table));

	if (policy->cpu >= NR_CPUS)
		goto freq_table_err;

	if(cpufreq_frequency_table_cpuinfo(policy, jz_cpufreq->freq_table))
		goto freq_table_err;

	cpufreq_frequency_table_get_attr(jz_cpufreq->freq_table, policy->cpu);

	policy->min = policy->cpuinfo.min_freq;
	policy->max = policy->cpuinfo.max_freq;
	policy->cur = jz4785_getspeed(policy->cpu);
	/*
	 * On JZ47XX SMP configuartion, both processors share the voltage
	 * and clock. So both CPUs needs to be scaled together and hence
	 * needs software co-ordination. Use cpufreq affected_cpus
	 * interface to handle this scenario.
	 */
	policy->shared_type = CPUFREQ_SHARED_TYPE_ANY;
	cpumask_setall(policy->cpus);

	/* 300us for latency. FIXME: what's the actual transition time? */
	policy->cpuinfo.transition_latency = 500 * 1000;
	INIT_DELAYED_WORK(&jz_cpufreq->vol_work, vol_down_work);
	jz_cpufreq->freq_thread = kthread_run(freq_monitor, jz_cpufreq, "freq_monitor");

#ifdef CONFIG_JZ_BIG_LITTLE_SWITCH
#define BIG_MIN     150000
#define LITTLE_MAX  300000

	freqs_max = LITTLE_MAX;
	freqs_min = BIG_MIN;
	freqs_tmp = freqs_min;
	max_flag = min_flag = 0;
	for(i = 0; i < CPUFREQ_NR; i++) {
 		if(!max_flag && freqs_max >= jz_cpufreq->freq_table[i].frequency && freqs_max < jz_cpufreq->freq_table[i + 1].frequency) {
			freqs_max = jz_cpufreq->freq_table[i].frequency;
			max_flag ++;
		}
		if(!min_flag && freqs_min >= jz_cpufreq->freq_table[i].frequency && freqs_min <= jz_cpufreq->freq_table[i + 1].frequency) {
			freqs_min = jz_cpufreq->freq_table[i + 1].frequency;
			freqs_tmp = jz_cpufreq->freq_table[i].frequency;
			min_flag ++;
		}
		if(max_flag && min_flag)
			break;
	}
	if(freqs_max == freqs_min)
		freqs_min = freqs_tmp;

	/*
	 * setup secondary cpu reset PC and code
	 * set change cpu freq max/min value
	 * init big/little cpu clk
	 */
	change_cpu_init(freqs_max, freqs_min);
#endif
	return 0;

freq_table_err:
	clk_put(jz_cpufreq->cpu_clk);
cpu_clk_err:
	kfree(jz_cpufreq);
	return -1;
}

static int jz4785_cpu_suspend(struct cpufreq_policy *policy)
{
	struct jz_cpufreq *jz_cpufreq = g_jz_cpufreq;

	if(jz_cpufreq->cpu_clk) {
		jz_cpufreq->rate = clk_get_rate(jz_cpufreq->cpu_clk);
		clk_set_rate(jz_cpufreq->cpu_clk,MIN_FREQ * 1000);
	}

	return 0;
}

int jz4785_cpu_resume(struct cpufreq_policy *policy)
{
	struct jz_cpufreq *jz_cpufreq = g_jz_cpufreq;

	if(jz_cpufreq->cpu_clk) {
		clk_set_rate(jz_cpufreq->cpu_clk, jz_cpufreq->rate);
	}

	return 0;
}

static struct freq_attr *jz4785_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver jz4785_driver = {
	.name		= "jz4785",
	.flags		= CPUFREQ_STICKY,
	.verify		= jz4785_verify_speed,
	.target		= jz4785_target,
	.get		= jz4785_getspeed,
	.init		= jz4785_cpu_init,
	.suspend	= jz4785_cpu_suspend,
	.resume		= jz4785_cpu_resume,
	.attr		= jz4785_cpufreq_attr,
};

static int __init jz4785_cpufreq_init(void)
{
	return cpufreq_register_driver(&jz4785_driver);
}

MODULE_AUTHOR("ztyan<ztyan@ingenic.cn>");
MODULE_DESCRIPTION("cpufreq driver for JZ47XX SoCs");
MODULE_LICENSE("GPL");
module_init(jz4785_cpufreq_init);
