/*
 * JZSOC TCU Unit, support timer and PWM application.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2006 Ingenic Semiconductor Inc.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>

#include <asm/div64.h>

#include <soc/base.h>
#include <soc/extal.h>
#include <soc/gpio.h>
#include <soc/tcu.h>


#define regr(off) 	inl(TCU_IOBASE + (off))
#define regw(val,off)	outl(val, TCU_IOBASE + (off))


#define TCU_CNT_MAX (65535UL)
#define NR_TCU_CH 8

static DECLARE_BITMAP(tcu_map, NR_TCU_CH) = { (1<<NR_TCU_CH) - 1, };

struct pwm_device {
	short id,running;
	const char *label;
} pwm_chs[NR_TCU_CH];

struct pwm_device *pwm_request(int id, const char *label)
{
	if (id < 0 || id > NR_TCU_CH)
		return ERR_PTR(-ENODEV);
	if (!test_bit(id, tcu_map))
		return ERR_PTR(-EBUSY);

	pwm_chs[id].id = id;
	pwm_chs[id].label = label;

	//jzgpio_ctrl_pull(GPIO_PORT_E,0,BIT(id));
	clear_bit(id, tcu_map);
	return &pwm_chs[id];
}

void pwm_free(struct pwm_device *pwm)
{
	set_bit(pwm->id, tcu_map);
}

int pwm_config(struct pwm_device *pwm, int duty_ns, int period_ns)
{
	unsigned long long tmp;
	unsigned long period, duty, csr;
	int prescaler = 0;

	if (!pwm || test_bit(pwm->id, tcu_map))
		return -EINVAL;
	if (duty_ns < 0 || duty_ns > period_ns)
		return -EINVAL;

	/* period < 10us || period > 1s */
	if (period_ns < 10000 || period_ns > 1000000000)
		return -EINVAL;


	tmp = JZ_EXTAL;
	tmp = tmp * period_ns;
	do_div(tmp, 1000000000);
	period = tmp;

	while (period > 0xffff && prescaler < 6) {
		period >>= 2;
		++prescaler;
	}

	if (prescaler == 6)
		return -EINVAL;

	tmp = (unsigned long long)period * duty_ns;
	do_div(tmp, period_ns);
	duty = tmp;

	if (duty >= period)
		duty = period - 1;

	csr = (prescaler) << 3;
	csr |= CSR_EXT_EN | TCSR_PWM_EN | TCSR_PWM_HIGH;

	if (pwm->running == 1)
		regw(BIT(pwm->id), TCU_TECR); /* disable */

	regw(csr,  CH_TCSR(pwm->id));
	regw(period, CH_TDFR(pwm->id));
	regw(duty, CH_TDHR(pwm->id));
	regw(0,    CH_TCNT(pwm->id));

	if (pwm->running == 1)
		regw(BIT(pwm->id), TCU_TESR); /* enable */

	return 0;
}

int pwm_enable(struct pwm_device *pwm)
{
	if (!pwm || test_bit(pwm->id, tcu_map))
		return -EINVAL;

	if (!pwm->running) {
		pwm->running = 1;
		regw(BIT(pwm->id), TCU_TESR); /* enable */
	}
	return 0;
}

void pwm_disable(struct pwm_device *pwm)
{
	if (!pwm || test_bit(pwm->id, tcu_map))
		return;

	if (pwm->running) {
		pwm->running = 0;
		regw(BIT(pwm->id), TCU_TECR); /* disable */
	}
}
