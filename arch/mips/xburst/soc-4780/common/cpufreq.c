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

#define CPUFREQ_NR 8
static struct cpufreq_frequency_table freq_table[CPUFREQ_NR];

static unsigned long regulator_table[12][2] = {
	{1700000,1350000}, // 1.7 GHz - 1.25V
	{1500000,1300000},
	{1300000,1250000},
	{1200000,1200000},
	{1100000,1150000},
	{ 900000,1100000},
	{ 700000,1100000},
	{ 500000,1100000},
	{ 300000,1100000},
	{ 150000,1025000},
	{      0,1025000},
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

static void freq_table_prepare(void)
{
	unsigned int i,max = clk_get_rate(cpu_clk) / 1000;
	memset(freq_table,0,sizeof(freq_table));
	for(i=0;i<CPUFREQ_NR && max >= 100000;i++) {
		freq_table[i].index = i;
		freq_table[i].frequency = max;
		max = max >> 1;
	}
	freq_table[i].index = i;
	freq_table[i].frequency = CPUFREQ_TABLE_END;
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
	unsigned long freq, volt = 0, volt_old = 0;

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

	/* notifiers */
	for_each_cpu(i, policy->cpus) {
		freqs.cpu = i;
		cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
	}

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

	ret = clk_set_rate(cpu_clk, freqs.new * 1000);

	if (cpu_regulator && (freqs.new < freqs.old)) {
		r = regulator_set_voltage(cpu_regulator, volt, volt);
		if (r < 0) {
			pr_warning("%s: unable to scale voltage down.\n",__func__);
			ret = clk_set_rate(cpu_clk, freqs.old * 1000);
			freqs.new = freqs.old;
			goto done;
		}
	}

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
	policy->cpuinfo.transition_latency = 300 * 1000;

	return 0;
}

static int jz4780_cpu_exit(struct cpufreq_policy *policy)
{
	clk_put(cpu_clk);
	return 0;
}

static struct freq_attr *jz4780_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver jz4780_driver = {
	.flags		= CPUFREQ_STICKY,
	.verify		= jz4780_verify_speed,
	.target		= jz4780_target,
	.get		= jz4780_getspeed,
	.init		= jz4780_cpu_init,
	.exit		= jz4780_cpu_exit,
	.name		= "jz4780",
	.attr		= jz4780_cpufreq_attr,
};

static int __init jz4780_cpufreq_init(void)
{
	cpu_clk = clk_get(NULL, "cclk");

	if (IS_ERR(cpu_clk))
		return PTR_ERR(cpu_clk);

	cpu_regulator = regulator_get(NULL, "vcore");
	if (IS_ERR(cpu_regulator)) {
		pr_warning("%s: unable to get MPU regulator\n", __func__);
		cpu_regulator = NULL;
	} else {
		/* 
		 * Ensure physical regulator is present.
		 * (e.g. could be dummy regulator.)
		 */
		if (regulator_get_voltage(cpu_regulator) < 0) {
			pr_warn("%s: physical regulator not present for MPU\n",
					__func__);
			regulator_put(cpu_regulator);
			cpu_regulator = NULL;
		}
	}

	freq_table_prepare();

	return cpufreq_register_driver(&jz4780_driver);
}

static void __exit jz4780_cpufreq_exit(void)
{
	cpufreq_unregister_driver(&jz4780_driver);
}

MODULE_AUTHOR("ztyan<ztyan@ingenic.cn>");
MODULE_DESCRIPTION("cpufreq driver for JZ47XX SoCs");
MODULE_LICENSE("GPL");
module_init(jz4780_cpufreq_init);
module_exit(jz4780_cpufreq_exit);

