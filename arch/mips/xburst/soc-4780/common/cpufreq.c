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

#include <asm/cpu.h>

#ifdef CONFIG_SMP
struct lpj_info {
	unsigned long	ref;
	unsigned int	freq;
};

static DEFINE_PER_CPU(struct lpj_info, lpj_ref);
static struct lpj_info global_lpj_ref;
#endif

static struct clk *cpu_clk;
static struct regulator *cpu_regulator;
static spinlock_t freq_lock;

#define CPUFREQ_NR 8
static struct cpufreq_frequency_table freq_table[CPUFREQ_NR];

#define MIN_FREQ 150000
#define MIN_VOLT 1200000
static unsigned long regulator_table[12][2] = {
	{ 1750000,1400000 }, // 1.7 GHz - 1.4V
	{ 1400000,1400000 }, // 1.4 GHz - 1.4V
	{ 1200000,1200000 },
	{MIN_FREQ,MIN_VOLT},
};

unsigned long regulator_find_voltage(int freqs)
{
	int i = 1;
	if(freqs >= regulator_table[0][0])
		return regulator_table[0][1];
	while(regulator_table[i][0] && regulator_table[i][1]) {
		if(freqs >= regulator_table[i][0])
			return regulator_table[i-1][1];
		i++;
	}
	return 0;
}

static int freq_table_prepare(void)
{
	struct clk *apll;
	struct clk *mpll;
	struct clk *cparent;
	unsigned int i,max;
	unsigned int apll_rate,mpll_rate;

	apll = clk_get(NULL,"apll");
	if (IS_ERR(apll)) {
		return -EINVAL;
	}

	mpll = clk_get(NULL,"mpll");
	if (IS_ERR(mpll)) {
		clk_put(apll);
		return -EINVAL;
	}

	apll_rate = clk_get_rate(apll) / 1000;
	mpll_rate = clk_get_rate(mpll) / 1000;
	cparent = clk_get_parent(cpu_clk);
	memset(freq_table,0,sizeof(freq_table));
#if 0
	if(apll_rate > mpll_rate) {
		max = apll_rate;
		for(i=0;i<CPUFREQ_NR && max >= (mpll_rate + 200000);i++) {
			freq_table[i].index = i;
			freq_table[i].frequency = max;
			max -= 192000;
		}
	}
#else
	if (cparent == apll) {
		freq_table[0].index = 0;
		freq_table[0].frequency = apll_rate;
		i = 1;
	} else {
		i = 0;
	}
#endif
	max = mpll_rate;
	for(;i<CPUFREQ_NR && max > 100000;i++) {
		freq_table[i].index = i;
		freq_table[i].frequency = max;
		max = max >> 1;
	}

	freq_table[i].index = i;
	freq_table[i].frequency = CPUFREQ_TABLE_END;
	freq_table[CPUFREQ_NR-1].index = CPUFREQ_NR-1;
	freq_table[CPUFREQ_NR-1].frequency = CPUFREQ_TABLE_END;

	clk_put(apll);
	clk_put(mpll);
#if 0
	for(i=0;i<CPUFREQ_NR;i++) {
		printk("%u %u\n",freq_table[i].index,freq_table[i].frequency);
	}
#endif
	return 0;
}

static int jz4780_verify_speed(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, freq_table);
}

static unsigned int jz4780_getspeed(unsigned int cpu)
{
	if (cpu >= NR_CPUS)
		return 0;

	return clk_get_rate(cpu_clk) / 1000;
}

static int jz4780_target(struct cpufreq_policy *policy,
		unsigned int target_freq,
		unsigned int relation)
{
	unsigned int i;
	int r,ret = 0;
	struct cpufreq_freqs freqs;
	unsigned long freq, flags, volt = 0, volt_old = 0;

	ret = cpufreq_frequency_table_target(policy, freq_table, target_freq,
			relation, &i);
	if (ret) {
		printk("%s: cpu%d: no freq match for %d(ret=%d)\n",
				__func__, policy->cpu, target_freq, ret);
		return ret;
	}
	freqs.new = freq_table[i].frequency;
	if (!freqs.new) {
		printk("%s: cpu%d: no match for freq %d\n", __func__,
				policy->cpu, target_freq);
		return -EINVAL;
	}

	freqs.old = jz4780_getspeed(policy->cpu);
	freqs.cpu = policy->cpu;

	if (freqs.old == freqs.new && policy->cur == freqs.new)
		return ret;

	freq = freqs.new * 1000;
	
	if (cpu_regulator) {
		volt = regulator_find_voltage(freqs.new);
		volt_old = regulator_get_voltage(cpu_regulator);
	}

	pr_debug("cpufreq-jz4780: %u MHz, %ld mV --> %u MHz, %ld mV\n", 
			freqs.old / 1000, volt_old ? volt_old / 1000 : -1,
			freqs.new / 1000, volt ? volt / 1000 : -1);

	if (cpu_regulator && (freqs.new > freqs.old)) {
		r = regulator_set_voltage(cpu_regulator, volt, volt);
		if (r < 0) {
			pr_warning("%s: unable to scale voltage up.\n",__func__);
			freqs.new = freqs.old;
			goto done;
		}
	}
	/* notifiers */
	for_each_cpu(i, policy->cpus) {
		freqs.cpu = i;
		cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
	}

	spin_lock_irqsave(&freq_lock, flags);
	ret = clk_set_rate(cpu_clk, freqs.new * 1000);


	freqs.new = jz4780_getspeed(policy->cpu);
#ifdef CONFIG_SMP
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
	if (!global_lpj_ref.freq) {
		global_lpj_ref.ref = loops_per_jiffy;
		global_lpj_ref.freq = freqs.old;
	}
	loops_per_jiffy = cpufreq_scale(global_lpj_ref.ref, global_lpj_ref.freq,
			freqs.new);
#endif
	spin_unlock_irqrestore(&freq_lock, flags);
	if (cpu_regulator && (freqs.new < freqs.old)) {
		r = regulator_set_voltage(cpu_regulator, volt, volt);
		if (r < 0) {
			pr_warning("%s: unable to scale voltage down. old:%d new:%d volt:%lu\n",__func__,
					freqs.old,freqs.new,volt);
			ret = clk_set_rate(cpu_clk, freqs.old * 1000);
			freqs.new = freqs.old;
			goto done;
		}
	}
done:
	/* notifiers */
	for_each_cpu(i, policy->cpus) {
		freqs.cpu = i;
		cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
	}

	return ret;
}

static int __cpuinit jz4780_cpu_init(struct cpufreq_policy *policy)
{
	if (policy->cpu >= NR_CPUS) {
		return -EINVAL;
	}

	if(cpufreq_frequency_table_cpuinfo(policy, freq_table))
		return -ENODATA;

	cpufreq_frequency_table_get_attr(freq_table, policy->cpu);

	policy->min = policy->cpuinfo.min_freq;
	policy->max = policy->cpuinfo.max_freq;
	policy->cur = jz4780_getspeed(policy->cpu);

	/*
	 * On JZ47XX SMP configuartion, both processors share the voltage
	 * and clock. So both CPUs needs to be scaled together and hence
	 * needs software co-ordination. Use cpufreq affected_cpus
	 * interface to handle this scenario.
	 */
#ifdef CONFIG_SMP
	policy->shared_type = CPUFREQ_SHARED_TYPE_ANY;
	cpumask_setall(policy->cpus);
#endif

	/* 300us for latency. FIXME: what's the actual transition time? */
	policy->cpuinfo.transition_latency = 500 * 1000;

	return 0;
}

static unsigned int rate,volt;
int jz4780_cpu_suspend(struct cpufreq_policy *policy)
{
	if(cpu_clk) {
		rate = clk_get_rate(cpu_clk);
		clk_set_rate(cpu_clk,MIN_FREQ * 1000);
	}
	
	if (cpu_regulator) {
		volt = regulator_get_voltage(cpu_regulator);
		if(regulator_set_voltage(cpu_regulator,MIN_VOLT,MIN_VOLT)) {
			printk("cpufreq suspend failed.\n");
			return -1;
		}
	}

	return 0;
}

int jz4780_cpu_resume(struct cpufreq_policy *policy)
{
	if(cpu_clk) {
		clk_set_rate(cpu_clk,rate);
	}

	if (cpu_regulator) {
		if(regulator_set_voltage(cpu_regulator,volt,volt)) {
			printk("cpufreq resume failed.\n");
			return -1;
		}
	}

	return 0;
}

static struct freq_attr *jz4780_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver jz4780_driver = {
	.name		= "jz4780",
	.flags		= CPUFREQ_STICKY,
	.verify		= jz4780_verify_speed,
	.target		= jz4780_target,
	.get		= jz4780_getspeed,
	.init		= jz4780_cpu_init,
	.suspend	= jz4780_cpu_suspend,
	.resume		= jz4780_cpu_resume,
	.attr		= jz4780_cpufreq_attr,
};

static int __init jz4780_cpufreq_init(void)
{
	cpu_clk = clk_get(NULL, "cclk");

	if (IS_ERR(cpu_clk))
		return PTR_ERR(cpu_clk);
#ifdef CONFIG_CPUFREQ_CHANGE_VCORE
	cpu_regulator = regulator_get(NULL, "vcore");
	if (IS_ERR(cpu_regulator)) {
		pr_warning("%s: unable to get CPU regulator\n", __func__);
		cpu_regulator = NULL;
	} else {
		/* 
		 * Ensure physical regulator is present.
		 * (e.g. could be dummy regulator.)
		 */
		if (regulator_get_voltage(cpu_regulator) < 0) {
			pr_warn("%s: physical regulator not present for CPU\n",
					__func__);
			regulator_put(cpu_regulator);
			cpu_regulator = NULL;
		}
	}
#else
	cpu_regulator = NULL;
#endif
	spin_lock_init(&freq_lock);

	if(freq_table_prepare())
		return -EINVAL;

	return cpufreq_register_driver(&jz4780_driver);
}

static void __exit jz4780_cpufreq_exit(void)
{
	if(cpu_clk) 
		clk_put(cpu_clk);
	if(cpu_regulator)
		regulator_put(cpu_regulator);
	cpufreq_unregister_driver(&jz4780_driver);
}

MODULE_AUTHOR("ztyan<ztyan@ingenic.cn>");
MODULE_DESCRIPTION("cpufreq driver for JZ47XX SoCs");
MODULE_LICENSE("GPL");
module_init(jz4780_cpufreq_init);
module_exit(jz4780_cpufreq_exit);

