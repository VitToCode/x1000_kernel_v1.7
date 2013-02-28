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

#define USE_PIO

#define BCH_INT_DECODE_FINISH	(1 << 3)
#define BCH_INT_ENCODE_FINISH	(1 << 2)
#define BCH_INT_UNCORRECT	(1 << 1)

#define BCH_ENABLED_INT	\
	(	\
	BCH_INT_DECODE_FINISH |	\
	BCH_INT_ENCODE_FINISH |	\
	BCH_INT_UNCORRECT	\
	)

#define BCH_CLK_RATE (200 * 1000 * 1000)

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

	regs_file_t __iomem * const regs_file;
	const struct resource regs_file_mem;

	struct list_head req_list;
	struct completion req_done;
	spinlock_t lock;

	struct task_struct *kbchd_task;
	wait_queue_head_t kbchd_wait;

	struct device *dev;
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

static void bch_select_encode(bch_request_t *req)
{
	bchc->regs_file->bhcsr = 1 << 2;
}

static void bch_select_ecc_level(bch_request_t *req)
{
	bchc->regs_file->bhccr = 0x7f << 4;
	bchc->regs_file->bhcsr = req->ecc_level;
}

static void bch_select_calc_size(bch_request_t *req)
{
	bchc->regs_file->bhcnt = 0;
	bchc->regs_file->bhcnt = ((req->ecc_level * 14 >> 8) << 16)
			| req->ecc_level;
}

static void bch_select_decode(bch_request_t *req)
{
	bchc->regs_file->bhccr = 1 << 2;
}

static void bch_wait_for_encode_done(bch_request_t *req)
{
	wait_for_completion(&bchc->req_done);
}

static void bch_wait_for_decode_done(bch_request_t *req)
{
	wait_for_completion(&bchc->req_done);
}

static void bch_start_new_operation(void)
{
	/* start operation */
	bchc->regs_file->bhcsr = 1 << 1;
}

static void bch_encode_by_cpu(bch_request_t *req)
{
	u32 *data = req->raw_data;

	int j = req->blksz >> 2;
	int i;

	bch_select_encode(req);
	bch_select_ecc_level(req);
	bch_select_calc_size(req);

	bch_start_new_operation();

	for (i = 0; i < j; i++)
		bchc->regs_file->bhdr = data[i];

	bch_wait_for_encode_done(req);

	if (bchc->regs_file->bhint & BCH_INT_ENCODE_FINISH) {
		j = div_ceiling(req->ecc_level * 14 >> 3, sizeof(u32));
		data = req->ecc_data;
		for (i = 0; i < j; i++)
			data[i] = bchc->regs_file->bhpar[i];

		req->ret_val = BCH_RET_OK;
	} else {
		req->ret_val = BCH_RET_UNEXPECTED;
	}
}

static void bch_decode_by_cpu(bch_request_t *req)
{
	u32 *data= req->raw_data;

	int j = req->blksz >> 2;
	int i;

	bch_select_ecc_level(req);
	bch_select_decode(req);
	bch_select_calc_size(req);

	bch_start_new_operation();

	for (i = 0; i < j; i++)
		bchc->regs_file->bhdr = data[i];

	data = req->ecc_data;
	j = div_ceiling(req->ecc_level * 14 >> 3, sizeof(u32));
	for (i = 0; i < j; i++)
		bchc->regs_file->bhdr = data[i];

	bch_wait_for_decode_done(req);

	if (bchc->regs_file->bhint & BCH_INT_DECODE_FINISH) {
		req->errrept_word_cnt = bchc->regs_file->bhint & (0x7f << 24);
		req->cnt_ecc_errors = bchc->regs_file->bhint & (0x7f << 16);
		for (i = 0; i < req->errrept_word_cnt; i++)
			req->errrept_data[i] = bchc->regs_file->bherr[i];

		req->ret_val = BCH_RET_OK;

	} else if (bchc->regs_file->bhint & BCH_INT_UNCORRECT){
		req->ret_val = BCH_RET_UNCORRECTABLE;

	} else {
		req->ret_val = BCH_RET_UNEXPECTED;
	}
}

static void bch_correct(bch_request_t *req)
{
	int i;
	int mask;
	int index;
	u16 *raw_data;

	raw_data = (u16 *)req->raw_data;
	for (i = 0; i < req->errrept_word_cnt; i++) {
		index = req->errrept_data[i] & 0xffff;
		mask = req->errrept_data[i] & (0xffff << 16);
		raw_data[(i << 6) + index] ^= mask;
	}

	req->ret_val = BCH_RET_OK;

	if (req->complete)
		req->complete(req);
}

static void bch_decode_correct_by_cpu(bch_request_t *req)
{
	/* start decode process */
	bch_decode_by_cpu(req);

	/* return if req is not correctable */
	if (req->ret_val)
		return;

	/* start correct process */
	bch_correct(req);
}

static void bch_pio_config(void)
{
	bchc->regs_file->bhccr = 1 << 11;
}

static void bch_enable(void)
{
	/* enable bchc */
	bchc->regs_file->bhcsr = 1;

	/* do not bypass decoder */
	bchc->regs_file->bhccr = 1 << 12;
}

static void bch_clear_pending_interrupts(void)
{
	/* clear enabled interrupts */
	bchc->regs_file->bhint = BCH_ENABLED_INT;
}

#ifndef USE_PIO

/* TODO: fill them */

static void bch_dma_config(void)
{
	dev_err(bchc->dev, "unsupported operation.\n");
	req->ret_val = BCH_RET_UNSUPPORTED;
}

static void bch_encode_by_dma(bch_request_t *req)
{
	dev_err(bchc->dev, "unsupported operation.\n");
	req->ret_val = BCH_RET_UNSUPPORTED;
}

static void bch_decode_by_dma(bch_request_t *req)
{
	dev_err(bchc->dev, "unsupported operation.\n");
	req->ret_val = BCH_RET_UNSUPPORTED;
}

static void bch_decode_correct_by_dma(bch_request_t *req)
{
	dev_err(bchc->dev, "unsupported operation.\n");
	req->ret_val = BCH_RET_UNSUPPORTED;
}

#endif

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

		/*
		 * no lock !? @$%^%&...
		 * don't worry you are going to lock free area
		 */
		temp = bchc->req_list.next;
		list_del_init(temp);
		req = list_entry(temp, bch_request_t, node);

		switch (req->type) {
		case BCH_REQ_ENCODE:
#ifdef USE_PIO
			bch_encode_by_cpu(req);
#else
			bch_encode_by_dma(req);
#endif
			break;

		case BCH_REQ_DECODE:
#ifdef USE_PIO
			bch_decode_by_cpu(req);
#else
			bch_decode_by_dma(req);

#endif
			break;

		case BCH_REQ_DECODE_CORRECT:
#ifdef USE_PIO
			bch_decode_correct_by_cpu(req);
#else
			bch_decode_correct_by_dma(req);
#endif
			break;

		default:
			dev_err(bchc->dev, "unsupported operation.\n");
			req->ret_val = BCH_RET_UNSUPPORTED;
			break;
		}

		if (req->complete)
			req->complete(req);

		bch_clear_pending_interrupts();
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

	if (req->type == BCH_REQ_CORRECT) {
		bch_correct(req);
		return 0;
	}

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

static void bch_clk_config(void)
{
	bchc->clk_bch = clk_get(bchc->dev, "cgu_bch");
	bchc->clk_bch_gate = clk_get(bchc->dev, "bch");

	clk_enable(bchc->clk_bch_gate);

	/* TODO: consider variable clk rate */
	clk_set_rate(bchc->clk_bch, BCH_CLK_RATE);
	clk_enable(bchc->clk_bch);
}

static void bch_irq_config(void)
{
	bchc->regs_file->bhintec = 0;

	/*
	 * enable de/encodeing finish
	 * and uncorrectable interrupt
	 */

	bchc->regs_file->bhintes = BCH_ENABLED_INT;
}

static irqreturn_t bch_isr(int irq, void *__unused)
{
	/* process only enabled interrupts */
	if (bchc->regs_file->bhint &
			(BCH_INT_DECODE_FINISH | BCH_INT_ENCODE_FINISH))
		complete(&bchc->req_done);

	return IRQ_HANDLED;
}

static int bch_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	spin_lock_init(&bchc->lock);
	INIT_LIST_HEAD(&bchc->req_list);
	init_waitqueue_head(&bchc->kbchd_wait);
	init_completion(&bchc->req_done);
	bchc->dev = &pdev->dev;

	res = request_mem_region(bchc->regs_file_mem.start,
			resource_size(&bchc->regs_file_mem), DRVNAME);
	if (!res) {
		dev_err(bchc->dev, "failed to grab regs file.\n");
		return -EBUSY;
	}

	bch_clk_config();

	bch_enable();

#ifdef USE_PIO
	bch_pio_config();
#else
	bch_dma_config();
#endif

	bch_irq_config();

	bchc->kbchd_task = kthread_run(bch_thread, NULL, "kbchd");
	if (IS_ERR(bchc->kbchd_task)) {
		ret = PTR_ERR(bchc->kbchd_task);
		dev_err(bchc->dev, "failed to start kbchd: %d\n", ret);
		goto err_release_mem;
	}

	ret = request_irq(IRQ_BCH, bch_isr, 0, DRVNAME, NULL);
	if (ret) {
		dev_err(bchc->dev, "failed to request interrupt.\n");
		goto err_release_mem;
	}

	dev_info(bchc->dev, "SoC-jz4780 HW ECC-BCH support "
			"functions initilized.\n");

err_release_mem:
	release_mem_region(bchc->regs_file_mem.start,
			resource_size(&bchc->regs_file_mem));

	return ret;
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
