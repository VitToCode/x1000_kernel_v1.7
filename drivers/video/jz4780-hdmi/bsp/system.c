/*
 * system.c
 *
 *  Created on: Jun 25, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 *
 *
 * 	@note: this file should be re-written to match the environment the
 * 	API is running on
 */

#include <linux/delay.h>
#include "../util/types.h"
#include "system.h"
#include <linux/interrupt.h>
#include <soc/irq.h>

struct hdmi_irq_handler {
	handler_t handler;
	void *param;
};

static struct hdmi_irq_handler m_irq_handler[3];

void system_SleepMs(unsigned ms)
{
	msleep(ms);
}

int system_ThreadCreate(void* handle, thread_t pFunc, void* param)
{
	/* TODO */
	return FALSE;
}

int system_ThreadDestruct(void* handle)
{
	/* TODO */
	return FALSE;
}

int system_Start(thread_t thread)
{
	thread(NULL);
	return FALSE;	      /* note: false is 0, but here we means success */
}

static unsigned system_InterruptMap(interrupt_id_t id)
{
	/* TODO */
	switch (id) {
	case TX_WAKEUP_INT:
		//return IRQ_HDMI_WAKEUP;
		return ~0;

	case TX_INT:
		return IRQ_HDMI;
	};
	return ~0;
}

int system_InterruptDisable(interrupt_id_t id)
{
	unsigned int irq = system_InterruptMap(id);
	disable_irq(irq);
	return TRUE;
}

int system_InterruptEnable(interrupt_id_t id)
{
	unsigned int irq = system_InterruptMap(id);
	enable_irq(irq);
	return TRUE;
}

int system_InterruptAcknowledge(interrupt_id_t id)
{
	/* do nothing */
	return TRUE;
}

static irqreturn_t jzhdmi_irq_handler(int irq, void *devid)
{
	struct hdmi_irq_handler *handler = (struct hdmi_irq_handler *)devid;

	//printk("-----=======irq irq=%d---\n",irq);
	handler->handler(handler->param);

	return IRQ_HANDLED;
}

static char *m_irq_name[3] = {
	"HDMI RX_INT",
	"HDMI TX_WAKEUP_INT",
	"HDMI TX_INT"
};

// typedef void (*handler_t)(void *)
int system_InterruptHandlerRegister(interrupt_id_t id, handler_t handler,
				    void * param) {
	int ret;
	unsigned int irq = system_InterruptMap(id);
	struct hdmi_irq_handler *irq_handler = &m_irq_handler[id - 1];
	irq_handler->handler = handler;
	irq_handler->param = param;
	printk("-----id=%d irq=%d---\n",id,irq);
	ret = request_irq(irq,
			  jzhdmi_irq_handler,
			  IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING
			  | IRQF_DISABLED,
			  m_irq_name[id - 1],
			  (void *)irq_handler);
	printk("ret=%d\n",ret);
	return (ret ? FALSE : TRUE);
}

int system_InterruptHandlerUnregister(interrupt_id_t id)
{
	unsigned int irq = system_InterruptMap(id);
	struct hdmi_irq_handler *irq_handler = &m_irq_handler[id - 1];

	free_irq(irq, irq_handler);
	return TRUE;
}
