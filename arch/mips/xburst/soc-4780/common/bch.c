/*
 *  Copyright (C) 2013 Fighter Sun <wanmyqawdr@126.com>
 *  HW ECC-BCH support functions
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/completion.h>

#include <soc/irq.h>
#include <soc/bch.h>

#define DRVNAME "jz4780-bch"

#define BCH_REGS_FILE_BASE	0x134d0000

typedef struct {
	volatile u32 bhcr;
	volatile u32 bhcsr;
	volatile u32 bhccr;
	volatile u32 bhcnt;
	volatile u32 bhdr;
	volatile u32 bhpar[28];
	volatile u32 bherr[64];
	volatile u32 bhint;
	volatile u32 bhintes;
	volatile u32 bhintec;
	volatile u32 bhinte;
	volatile u32 bhto;
} regs_file_t;

/* instance a singleton bchc */
struct {
	struct clk *clk_bch;
	struct clk *clk_bch_gate;

	atomic_t busy;

	regs_file_t __iomem * const regs_file;
	const struct resource regs_file_mem;

	struct list_head req_list;
	spinlock_t lock;

	wait_queue_head_t kbchd_wait;
	struct completion req_done;

	struct device *dev;

	struct task_struct *kbchd_task;
} instance = {
	.regs_file = (regs_file_t *)
					CKSEG1ADDR(BCH_REGS_FILE_BASE),
	.regs_file_mem = {
		.start = BCH_REGS_FILE_BASE,
		.end = BCH_REGS_FILE_BASE + sizeof(regs_file_t) - 1,
	},

}, *bchc = &instance;

static inline u32 div_ceiling(u32 x, u32 y)
{
	return (x + y - 1) / y;
}

static void bch_enable_encode(bch_request_t *req)
{
	bchc->regs_file->bhcr |= 1 << 2;
}

static void bch_enable_decode(bch_request_t *req)
{
	bchc->regs_file->bhcr &= ~(1 << 2);
}

static void bch_wait_for_encode_done(bch_request_t *req)
{
	bchc->regs_file->bhcr |= 1 << 1;
	wait_for_completion(&bchc->req_done);
}

static void bch_wait_for_decode_done(bch_request_t *req)
{
	bchc->regs_file->bhcr |= 1 << 1;
	wait_for_completion(&bchc->req_done);
}

static void bch_encode(bch_request_t *req)
{
	u32 *data = req->raw_data;

	int j = req->blksz >> 2;
	int i;

	bch_enable_encode(req);

	for (i = 0; i < j; i++)
		bchc->regs_file->bhdr = data[i];

	bch_wait_for_encode_done(req);

	j = div_ceiling(req->ecc_level * 14 >> 3, sizeof(u32));
	data = req->ecc_data;
	for (i = 0; i < j; i++)
		data[i] = bchc->regs_file->bhpar[i];

	/* TODO: error report */
}

static void bch_decode(bch_request_t *req)
{
	u32 *data= req->raw_data;

	int j = req->blksz >> 2;
	int i;

	bch_enable_decode(req);

	for (i = 0; i < j; i++)
		bchc->regs_file->bhdr = data[i];

	data = req->ecc_data;
	j = div_ceiling(req->ecc_level * 14 >> 3, sizeof(u32));
	for (i = 0; i < j; i++)
		bchc->regs_file->bhdr = data[i];

	bch_wait_for_decode_done(req);

	/* TODO: copy decoded infromations and error report */
}

static void bch_correct(bch_request_t *req)
{
	bch_decode(req);

	/* TODO: ecc correct codes */
}

static int bch_request_enqueue(bch_request_t *req)
{
	INIT_LIST_HEAD(&req->node);
	spin_lock(&bchc->lock);
	list_add_tail(&req->node, &bchc->req_list);
	spin_unlock(&bchc->lock);

	wake_up(&bchc->kbchd_wait);

	return 0;
}

static int bch_request_dequeue(void)
{
	struct list_head *temp;
	bch_request_t *req;

	while (1) {
		spin_lock(&bchc->lock);
		if (list_empty(&bchc->req_list)) {
			spin_unlock(&bchc->lock);
			break;
		}

		temp = bchc->req_list.next;
		list_del_init(temp);
		req = list_entry(temp, bch_request_t, node);

		switch (req->type) {
		case bch_req_encode:
			bch_encode(req);
			break;

		case bch_req_decode:
			bch_decode(req);
			break;

		case bch_req_correct:
			bch_correct(req);
			break;

		default:
			dev_err(bchc->dev, "unsupported operation.\n");
			break;
		}

		if (req->complete)
			req->complete(req);
	}

	return 0;
}

int bch_request_submit(bch_request_t *req)
{
	if (req->blksz & 0x3 || req->blksz > 1900)
		return -EINVAL;
	else if (req->ecc_level & 0x3 || req->ecc_level > 64)
		return -EINVAL;
	else if (req->dev == NULL)
		return -ENODEV;

	return bch_request_enqueue(req);
}
EXPORT_SYMBOL(bch_request_submit);

static int bch_thread(void *__unused)
{
	set_freezable();

	do {
		bch_request_dequeue();
		wait_event_freezable(bchc->kbchd_wait,
				!list_empty(&bchc->req_list) ||
				kthread_should_stop());

		/* TODO: thread exiting control */
	} while (!kthread_should_stop() || !list_empty(&bchc->req_list));

	dev_err(bchc->dev, "kbchd exiting.\n");

	return 0;
}

static irqreturn_t bch_isr(int irq, void *__unused)
{
	complete(&bchc->req_done);
	return IRQ_HANDLED;
}

static int bch_probe(struct platform_device *pdev)
{
	spin_lock_init(&bchc->lock);
	INIT_LIST_HEAD(&bchc->req_list);
	init_waitqueue_head(&bchc->kbchd_wait);
	init_completion(&bchc->req_done);
	bchc->dev = &pdev->dev;

	/* TODO: bch clk & interrupt control */

	bchc->kbchd_task = kthread_run(bch_thread, NULL, "kbchd");
	if (IS_ERR(bchc->kbchd_task))
		return PTR_ERR(bchc->kbchd_task);

	return request_irq(IRQ_BCH, bch_isr, 0, DRVNAME, NULL);
}

static struct platform_driver bch_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = DRVNAME,
	},
};

static struct platform_device bch_device = {
	.name = DRVNAME,
};

static int __init bch_init(void)
{
	platform_device_register(&bch_device);
	return platform_driver_probe(&bch_driver, bch_probe);
}

postcore_initcall(bch_init);

MODULE_AUTHOR("Fighter Sun <wanmyqawdr@126.com>");
MODULE_DESCRIPTION("SoC-jz4780 HW ECC-BCH support functions");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:"DRVNAME);
