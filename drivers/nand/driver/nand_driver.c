/**
 * nand_driver.c
 **/
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include <soc/base.h>

#include <nand_api.h>

#define USE_EDGE_IRQ

struct nand_api_osdependent ndd_private;
nand_sharing_params share_parms;

/* write protect */
#define WP_ENABLE	0
#define WP_DISABLE	1
#define wp_enable(gpio)		gpio_direction_output(gpio, WP_ENABLE)
#define wp_disable(gpio)	gpio_direction_output(gpio, WP_DISABLE)

#define __ndd_irq_to_gpio(irq) (irq - IRQ_GPIO_BASE)

#ifndef USE_EDGE_IRQ
#define NDD_GPIO_IOBASE ((GPIO_IOBASE) + 0xa0000000)
/* Port Interrupt Mask Register */
#define NDD_GPIO_PXMASK(n)	(*(volatile unsigned int *)((NDD_GPIO_IOBASE) + (0x20 + (n)*0x100)))
/* Port Interrupt Mask Set Reg */
#define NDD_GPIO_PXMASKS(n)	(*(volatile unsigned int *)((NDD_GPIO_IOBASE) + (0x24 + (n)*0x100)))
/* Port Interrupt Mask Clear Reg */
#define NDD_GPIO_PXMASKC(n)	(*(volatile unsigned int *)((NDD_GPIO_IOBASE) + (0x28 + (n)*0x100)))
/* Port Pattern 0 Set Register */
#define NDD_GPIO_PXPAT0S(n)	(*(volatile unsigned int *)((NDD_GPIO_IOBASE) + (0x44 + (n)*0x100)))
/* Port Pattern 0 Clear Register */
#define NDD_GPIO_PXPAT0C(n)	(*(volatile unsigned int *)((NDD_GPIO_IOBASE) + (0x48 + (n)*0x100)))
#define __ndd_gpio_mask_irq(gpio)				\
	do {							\
		unsigned int p, o;				\
		p = (gpio) / 32;				\
		o = (gpio) % 32;				\
		NDD_GPIO_PXMASKS(p) = (1 << o);			\
	} while (0)
#define __ndd_gpio_unmask_irq(gpio)				\
	do {							\
		unsigned int p, o;				\
		p = (gpio) / 32;				\
		o = (gpio) % 32;				\
		NDD_GPIO_PXMASKC(p) = (1 << o);			\
	} while (0)
#define __ndd_gpio_as_irq_high_level(gpio)		\
	do {						\
		unsigned int p, o;			\
		p = (gpio) / 32;			\
		o = (gpio) % 32;			\
		NDD_GPIO_PXPAT0S(p) = (1 << o);		\
	} while (0)
#define __ndd_gpio_as_irq_low_level(gpio)		\
	do {						\
		unsigned int p, o;			\
		p = (gpio) / 32;			\
		o = (gpio) % 32;			\
		NDD_GPIO_PXPAT0C(p) = (1 << o);		\
	} while (0)

static int ignore_irqs = 0;
#endif

/* ############################################################################################ *\
 * os clib dependent functions
\* ############################################################################################ */
static void ndd_ndelay(unsigned long nsecs)
{
	ndelay(nsecs);
}

static int ndd_div_s64_32(long long dividend, int divisor)
{
	long long result = 0;
	int remainder = 0;

	result = div_s64_rem(dividend, divisor, &remainder);
	//result = remainder ? (result +1) : result;
	return result;
}

static void* ndd_continue_alloc(unsigned int size)
{
	return kzalloc(size, GFP_KERNEL);
}

static void ndd_continue_free(void *addr)
{
	kfree(addr);
}

static unsigned int ndd_get_vaddr(unsigned int paddr)
{
	return (unsigned int)(page_address(pfn_to_page(paddr >> PAGE_SHIFT)) + (paddr & ~PAGE_MASK));
}

static void ndd_dma_cache_wback(unsigned long addr, unsigned long size)
{
	dma_cache_wback(addr, size);
}

static void ndd_dma_cache_inv(unsigned long addr, unsigned long size)
{
	dma_cache_inv(addr, size);
}

static unsigned long long ndd_get_time_nsecs(void)
{
	return sched_clock();
}

/* ############################################################################################ *\
 * callback functions
\* ############################################################################################ */
static void ndd_wp_enable(int gpio)
{
	wp_enable(gpio);
}

static void ndd_wp_disable(int gpio)
{
	wp_disable(gpio);
}

static void ndd_clear_rb_state(rb_item *rbitem)
{
	if (try_wait_for_completion((struct completion *)rbitem->irq_private))
		printk("WARNING: nand driver has an unprocess rb! irq = %d, gpio = %d\n",
		       rbitem->irq, rbitem->gpio);
}

static int ndd_wait_rb_timeout(rb_item *rbitem, int timeout)
{
	int ret = 0;

	if (rbitem->irq != -1) {
		ret = wait_for_completion_timeout((struct completion *)rbitem->irq_private,
						  (msecs_to_jiffies(timeout)));
		if (ret == 0){
			printk("nand_driver.c[%s]: wait rb timeout!\n ",__func__);
			ret = -1;
		}else
			ret = 0;
	} else {
		/* pollimg rbitem->gpio */
	}

	return ret;
}

static int ndd_try_wait_rb(rb_item *rbitem, int delay)
{
	msleep(delay);
	if (rbitem->irq != -1) {
		if (try_wait_for_completion((struct completion *)rbitem->irq_private))
			return 1;
		else
			return 0;
	} else {
		/* pollimg rbitem->gpio */
	}
}

static int nfi_readl(int reg)
{
	void __iomem *iomem = ndd_private.base->nfi.iomem;

	return __raw_readl(iomem + reg);
}

static void nfi_writel(int reg, int val)
{
	void __iomem *iomem = ndd_private.base->nfi.iomem;

	__raw_writel((val), iomem + reg);
}

static int bch_readl(int reg)
{
	void __iomem *iomem = ndd_private.base->bch.iomem;

	return __raw_readl(iomem + reg);
}

static void bch_writel(int reg, int val)
{
	void __iomem *iomem = ndd_private.base->bch.iomem;

	__raw_writel((val), iomem + reg);
}

static int nfi_clk_enable(void)
{
	return clk_enable((struct clk *)ndd_private.base->nfi.gate);
}

static void nfi_clk_disable(void)
{
	clk_disable((struct clk *)ndd_private.base->nfi.gate);
}

static int bch_clk_enable(void)
{
	int ret;
	unsigned int rate;
	struct clk *gate = (struct clk *)ndd_private.base->bch.gate;
	struct clk *clk = (struct clk *)ndd_private.base->bch.clk;

	ret = clk_enable(gate);
	if (ret)
		return ret;

	clk_disable(clk);

#ifdef BCH_USE_NEMC_RATE
	rate = ndd_private.base->nfi.rate;
#else
	rate = clk_get_rate(clk);
#endif
	//printk("nand bch clk rate [%d]\n", rate);

	ret = clk_set_rate(clk, rate);
	if (ret)
		return ret;

	return clk_enable(clk);
}

static void bch_clk_disable(void)
{
	clk_disable((struct clk *)ndd_private.base->bch.gate);
	clk_disable((struct clk *)ndd_private.base->bch.clk);
}

/* ############################################################################################ *\
 * sub functions for driver probe
\* ############################################################################################ */
static irqreturn_t nand_waitrb_handler(int irq, void *dev_id)
{
	int gpio = __ndd_irq_to_gpio(irq);
#ifndef USE_EDGE_IRQ
	if (gpio_get_value(gpio))
		__ndd_gpio_as_irq_low_level(gpio);
	else {
		__ndd_gpio_as_irq_high_level(gpio);
		return IRQ_HANDLED;
	}

	if (ignore_irqs) {
		ignore_irqs--;
		return IRQ_HANDLED;
	}
#endif
	if (gpio_get_value(gpio)) {
		complete((struct completion *)dev_id);
	} else
		printk("WARNING: maybe not a real rb interrupt!\n");

	return IRQ_HANDLED;
}

static int gpio_irq_request(unsigned short gpio, unsigned short *irq, int rbcomp)
{
	int ret;
	char *name_gpio, *name_irq;
	struct completion *rb_comp = (struct completion *)rbcomp;

	/* request gpio */
	name_gpio = vmalloc(32);
	sprintf(name_gpio, "nand_rb%d", gpio);
	ret = gpio_request_one(gpio, GPIOF_DIR_IN, name_gpio);
	if (ret) {
		printk("ERROR: [%s] request gpio error", name_gpio);
		vfree(name_gpio);
		return ret;
	}

	/* request irq */
	*irq = gpio_to_irq(gpio);
	name_irq = vmalloc(32);
	sprintf(name_irq, "nand_rb%d", *irq);
#ifndef USE_EDGE_IRQ
	ret = request_irq(*irq, nand_waitrb_handler,
			  IRQF_DISABLED | IRQF_TRIGGER_HIGH,
			  name_irq, (void *)rb_comp);
#else
	ret = request_irq(*irq, nand_waitrb_handler,
			  IRQF_DISABLED | IRQF_TRIGGER_RISING,
			  name_irq, (void *)rb_comp);
#endif
	if (ret) {
		printk("ERROR: [%s] request irq error", name_irq);
		vfree(name_irq);
		vfree(name_gpio);
		return ret;
	}

	/* init rb complition */
	init_completion(rb_comp);
	//printk("\t request rb irq: gpio = [%d], irq = [%d], comp = [%p]\n", gpio, *irq, rb_comp);

#ifndef USE_EDGE_IRQ
	while (ignore_irqs) {
		ndd_print(NDD_WARNING, "wait ingore unused level irq\n");
		msleep(10);
	}
#endif

	return 0;
}

static void* get_iomem(struct platform_device *pdev, int index)
{
	void __iomem *iomem;
	struct resource *mem_res;

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, index);
	if (!mem_res) {
		printk("ERROR: get memory resource error!\n");
		return NULL;
	}

	iomem = ioremap(mem_res->start, resource_size(mem_res));
	if (!iomem) {
		printk("ERROR: remap io memory error!\n");
		return NULL;
	}

	return iomem;
}

static int get_irq(struct platform_device *pdev, int index)
{
	int irq;

	irq = platform_get_irq(pdev, index);
	if (irq < 0) {
		printk("ERROR: get irq resource error!\n");
		return -1;
	}

	return irq;
}

static int get_pdma_channel(struct platform_device *pdev, int index)
{
	struct resource *dma_res;

	dma_res = platform_get_resource(pdev, IORESOURCE_DMA, index);
	if (!dma_res) {
		printk("ERROR: get dma resource error!\n");
		return -1;
	}

	return (unsigned int)dma_res->start;
}

static inline int get_devices_resources(struct platform_device *pdev, io_base *base)
{
	int i;
	unsigned int res_cnt = pdev->num_resources;
	unsigned int index = 0;
	nfi_base *nfi = &(base->nfi);
	bch_base *bch = &(base->bch);
	pdma_base *pdma = &(base->pdma);

	/* get resource for nfi */
	nfi->iomem = get_iomem(pdev, index);
	nfi->irq = get_irq(pdev, index);
	index++;
	res_cnt -= 2;
	//printk("\t get nfi resources, iomem = [%p], irq = [%d]!\n", nfi->iomem, nfi->irq);

	/* get resource for bch */
	bch->iomem = get_iomem(pdev, index);
	bch->irq = get_irq(pdev, index);
	index++;
	res_cnt -= 2;
	//printk("\t get bch resources, iomem = [%p], irq = [%d]!\n", bch->iomem, bch->irq);

	/* get resource for pdma */
	pdma->iomem = get_iomem(pdev, index);
	pdma->dma_channel = get_pdma_channel(pdev, 0);
	index++;
	res_cnt -= 2;
	//printk("\t get pdma resources, iomem = [%p], dma_channel = [%d]!\n", pdma->iomem, pdma->dma_channel);

	for (i = 0; i < res_cnt; i++) {
		nfi->cs_iomem[i] = get_iomem(pdev, index);
		index++;
		//printk("\t get cs[%d] resources, iomem = [%p]!\n", i, nfi->cs_iomem[i]);
	}

	return 0;
}

static inline int get_devices_clk(struct device	*dev, io_base *base)
{
	nfi_base *nfi = &(base->nfi);
	bch_base *bch = &(base->bch);

	/* get nfi clk */
	nfi->gate = clk_get(dev, "nemc");
	if (IS_ERR(nfi->gate)) {
		printk("ERROR: get nfi gate clk error!\n");
		return -1;
	}
	nfi->rate = clk_get_rate(nfi->gate);
	//printk("\t get nfi clk gate [%p]!\n", nfi->gate);

	if (nfi->rate > NFI_MAX_RATE_LIMIT) {
		printk("ERROR: nand controller clk rate [%ld] is fast than max support [%d]\n",
		       nfi->rate, NFI_MAX_RATE_LIMIT);
		return -1;
	} else {
		//printk("nand controller clk rate [%ld]\n", nfi->rate);
	}

	/* get bch clk */
	bch->gate = clk_get(dev, "bch");
	if (IS_ERR(bch->gate)) {
		printk("ERROR: get bch gate clk error!\n");
		return -1;
	}
	bch->clk = clk_get(dev, "cgu_bch");
	if (IS_ERR(bch->clk)) {
		printk("ERROR: get bch clk error!\n");
		return -1;
	}
	//printk("\t get bch clk gate [%p], clk [%p]!\n", bch->gate, bch->clk);

	return 0;
}

static inline int get_device_callbak(io_base *base)
{
	nfi_base *nfi = &(base->nfi);
	bch_base *bch = &(base->bch);

	/* nfi callbacks */
	nfi->readl = nfi_readl;
	nfi->writel = nfi_writel;
	nfi->clk_enable = nfi_clk_enable;
	nfi->clk_disable = nfi_clk_disable;

	/* bch callbacks */
	bch->readl = bch_readl;
	bch->writel = bch_writel;
	bch->clk_enable = bch_clk_enable;
	bch->clk_disable = bch_clk_disable;

	return 0;
}

static int fill_io_base(struct platform_device *pdev, io_base *base)
{
	int ret;

	//printk("\nget devices resources:\n");
	ret = get_devices_resources(pdev, base);
	if (ret) {
		printk("ERROR: get devices resources error!\n");
		return ret;
	}

	//printk("\nget devices clk:\n");
	ret = get_devices_clk(&pdev->dev, base);
	if (ret) {
		printk("ERROR: get devices clk error");
		return ret;
	}

	//printk("\nget devices callback:\n");
	get_device_callbak(base);
	return 0;
}

static int ndd_gpio_request(unsigned int gpio, const char *lable)
{
	int ret = 0;
	ret = gpio_request(gpio, lable);
	if (ret) {
		printk("ERROR: [%s] request gpio error", lable);
		ret = -1;
	}
	return ret;
}
static rb_info *get_rbinfo_memory(void)
{
	rb_info *rbinfo = NULL;
	unsigned char *rb_comp_base;
	int i = 0;
	rbinfo = kzalloc(sizeof(rb_info) +
				     sizeof(rb_item) * CS_PER_NFI +
				     sizeof(struct completion) * CS_PER_NFI,
				     GFP_KERNEL);
	if(rbinfo){
		rbinfo->rbinfo_table = (rb_item *)((unsigned char *)rbinfo + sizeof(rb_info));
		rb_comp_base = (unsigned char *)rbinfo->rbinfo_table + sizeof(rb_item) * CS_PER_NFI;
		for (i = 0; i < CS_PER_NFI; i++)
			rbinfo->rbinfo_table[i].irq_private = rb_comp_base + sizeof(struct completion) * i;
	}
	return rbinfo;
}
static void abandon_rbinfo_memory(rb_info *rbinfo)
{
	kfree(rbinfo);
}
/* ############################################################################################ *\
 * nand driver main functions
\* ############################################################################################ */
static int nand_probe(struct platform_device *pdev)
{
	int ret;

	ndd_private.rbinfo = NULL;

	/* alloc memory for io_base and fill nand base */
	ndd_private.base = kzalloc(sizeof(io_base), GFP_KERNEL);
	if (!ndd_private.base)
		goto err_alloc_base;

	ret = fill_io_base(pdev, ndd_private.base);
	if (ret)
		goto err_fill_base;

	/* fill fun points */
	ndd_private.wp_enable = ndd_wp_enable;
	ndd_private.wp_disable = ndd_wp_disable;
	ndd_private.clear_rb_state = ndd_clear_rb_state;
	ndd_private.wait_rb_timeout = ndd_wait_rb_timeout;
	ndd_private.try_wait_rb = ndd_try_wait_rb;
	ndd_private.gpio_irq_request = gpio_irq_request;
	ndd_private.ndd_gpio_request = ndd_gpio_request;
	ndd_private.get_rbinfo_memory = get_rbinfo_memory;
	ndd_private.abandon_rbinfo_memory = abandon_rbinfo_memory;
	ndd_private.get_nand_flash = NULL;
	/* clib */
	ndd_private.clib.ndelay = ndd_ndelay;
	ndd_private.clib.div_s64_32 = ndd_div_s64_32;
	ndd_private.clib.continue_alloc = ndd_continue_alloc;
	ndd_private.clib.continue_free = ndd_continue_free;
	ndd_private.clib.printf = printk;
	ndd_private.clib.memcpy = memcpy;
	ndd_private.clib.memset = memset;
	ndd_private.clib.strcmp = strcmp;
	ndd_private.clib.get_vaddr = ndd_get_vaddr;
	ndd_private.clib.dma_cache_wback = ndd_dma_cache_wback;
	ndd_private.clib.dma_cache_inv = ndd_dma_cache_inv;
	ndd_private.clib.get_time_nsecs = ndd_get_time_nsecs;
	ndd_private.erasemode = 0;

	/* nand api init */
	ret = nand_api_init(&ndd_private);
	if (ret)
		goto err_api_init;

	return 0;

err_api_init:
err_fill_base:
	kfree(ndd_private.base);
err_alloc_base:

	return -1;
}

static int nand_remove(struct platform_device *pdev)
{
	return 0;//nand_api_remove();
}

static void nand_shutdown(struct platform_device *pdev)
{
	return ;//nand_api_shutdown();
}

static int nand_suspend(struct platform_device *pdev, pm_message_t state)
{
	return nand_api_suspend();
}

static int nand_resume(struct platform_device *pdev)
{
	return nand_api_resume();
}

static struct platform_driver __jz_nand_driver = {
	.probe		= nand_probe,
	.remove		= nand_remove,
	.shutdown	= nand_shutdown,
	.suspend	= nand_suspend,
	.resume		= nand_resume,
	.driver		= {
		.name	= "jz_nand",
		.owner	= THIS_MODULE,
	},
};

static int __init nand_init(void)
{
	return platform_driver_register(&__jz_nand_driver);
}

static void __exit nand_exit(void)
{
	platform_driver_unregister(&__jz_nand_driver);
}

int __init get_nand_param(void)
{
	memcpy(&share_parms, (unsigned char *)NAND_SHARING_PARMS_ADDR, sizeof(nand_sharing_params));
	return 0;
}

module_init(nand_init);
module_exit(nand_exit);
early_initcall(get_nand_param);
