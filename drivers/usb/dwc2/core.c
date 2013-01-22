#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>

#include <linux/usb/otg.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

#include "core.h"
#include "gadget.h"
#include "debug.h"

#ifdef CONFIG_USB_DWC2_VERBOSE_VERBOSE
int dwc2_core_debug_en = 0;
module_param(dwc2_core_debug_en, int, 0644);

#define DWC2_CORE_DEBUG_MSG(msg...)				\
        do {							\
		if (unlikely(dwc2_core_debug_en)) {		\
			printk("CPU%d: ", smp_processor_id());	\
			printk("DWC2(CORE): " msg);		\
		}						\
	} while(0)

#else
#define DWC2_CORE_DEBUG_MSG(msg...)  do {  } while(0)
#endif


void dwc2_wait_3_phy_clocks(void) {
	/* PHY Cock: 30MHZ or 60MHZ */
	int ns = (1 * 1000 * 1000 * 1000) / (30 * 1000 * 1000);

	/* yes, we wait 5 phy clocks */
	ns *= 5;

	ndelay(ns);
}

void dwc2_enable_global_interrupts(struct dwc2 *dwc)
{
	gahbcfg_data_t ahbcfg;

	ahbcfg.d32 = readl(&dwc->core_global_regs->gahbcfg);
	ahbcfg.b.glblintrmsk = 1;
	writel(ahbcfg.d32, &dwc->core_global_regs->gahbcfg);
}

void dwc2_disable_global_interrupts(struct dwc2 *dwc)
{
	gahbcfg_data_t ahbcfg;

	ahbcfg.d32 = readl(&dwc->core_global_regs->gahbcfg);
	ahbcfg.b.glblintrmsk = 0;
	writel(ahbcfg.d32, &dwc->core_global_regs->gahbcfg);
}

uint8_t dwc2_is_device_mode(struct dwc2 *dwc)
{
	uint32_t curmod = readl(&dwc->core_global_regs->gintsts);

	return (curmod & 0x1) == 0;
}

uint8_t dwc2_is_host_mode(struct dwc2 *dwc)
{
	uint32_t curmod = readl(&dwc->core_global_regs->gintsts);

	return (curmod & 0x1) == 1;
}

/**
 * Do core a soft reset of the core.  Be careful with this because it
 * resets all the internal state machines of the core.
 */
void dwc2_core_reset(struct dwc2 *dwc)
{
	dwc_otg_core_global_regs_t *global_regs = dwc->core_global_regs;
	volatile grstctl_t greset = {.d32 = 0 };
	int count = 0;

	/* Core Soft Reset */
	count = 0;
	greset.b.csftrst = 1;
	writel(greset.d32, &global_regs->grstctl);
	do {
		greset.d32 = readl(&global_regs->grstctl);
		if (++count > 10000) {
			dev_warn(dwc->dev, "%s() HANG! Soft Reset GRSTCTL=0x%08x\n",
				__func__, greset.d32);
			break;
		}
		udelay(1);
	} while (greset.b.csftrst == 1);

	/* Wait for AHB master IDLE state. */
	do {
		udelay(10);
		greset.d32 = readl(&global_regs->grstctl);
		if (++count > 100000) {
			dev_warn(dwc->dev, "%s() HANG! AHB Idle GRSTCTL=0x%08x\n",
				__func__, greset.d32);
			return;
		}
	} while (greset.b.ahbidle == 0);

	dwc2_wait_3_phy_clocks();
}

void dwc2_enable_common_interrupts(struct dwc2 *dwc)
{
	dwc_otg_core_global_regs_t *global_regs = dwc->core_global_regs;
	gintmsk_data_t intr_mask = {.d32 = 0 };

	/* Clear any pending OTG Interrupts */
	writel(0xFFFFFFFF, &global_regs->gotgint);

	/* Clear any pending interrupts */
	writel(0xFFFFFFFF, &global_regs->gintsts);

	/*
	 * Enable the interrupts in the GINTMSK.
	 */
	intr_mask.b.modemismatch = 1;
	intr_mask.b.otgintr = 1;

	if (!dwc->dma_enable) {
		intr_mask.b.rxstsqlvl = 1;
	}

	intr_mask.b.conidstschng = 1;
	intr_mask.b.wkupintr = 1;
	intr_mask.b.disconnect = 0;
	intr_mask.b.usbsuspend = 1;
	intr_mask.b.sessreqintr = 1;

#ifdef CONFIG_USB_DWC_OTG_LPM
	if (dwc->core_params->lpm_enable) {
		intr_mask.b.lpmtranrcvd = 1;
	}
#endif
	writel(intr_mask.d32, &global_regs->gintmsk);
}

static void dwc2_init_csr(struct dwc2 *dwc) {
	/* change to uint8_t*, for convenient to add an offset */
	uint8_t *reg_base = (uint8_t *)dwc->regs;
	int i = 0;

	/* Global CSR */
	dwc->core_global_regs = (dwc_otg_core_global_regs_t *)reg_base;

	/* Device Mode CSR */
	dwc->dev_if.dev_global_regs =
		(dwc_otg_device_global_regs_t *) (reg_base + DWC_DEV_GLOBAL_REG_OFFSET);

	for (i = 0; i < MAX_EPS_CHANNELS; i++) {
		dwc->dev_if.in_ep_regs[i] = (dwc_otg_dev_in_ep_regs_t *)
			(reg_base + DWC_DEV_IN_EP_REG_OFFSET +
				(i * DWC_EP_REG_OFFSET));

		dwc->dev_if.out_ep_regs[i] = (dwc_otg_dev_out_ep_regs_t *)
			(reg_base + DWC_DEV_OUT_EP_REG_OFFSET +
				(i * DWC_EP_REG_OFFSET));
	}

	/* Host Mode CSR */
	dwc->host_if.host_global_regs = (dwc_otg_host_global_regs_t *)
		(reg_base + DWC_OTG_HOST_GLOBAL_REG_OFFSET);

	dwc->host_if.hprt0 =
		(uint32_t *) (reg_base + DWC_OTG_HOST_PORT_REGS_OFFSET);

	for (i = 0; i < MAX_EPS_CHANNELS; i++) {
		dwc->host_if.hc_regs[i] = (dwc_otg_hc_regs_t *)
			(reg_base + DWC_OTG_HOST_CHAN_REGS_OFFSET +
				(i * DWC_OTG_CHAN_REGS_OFFSET));
	}

	dwc->host_if.num_host_channels = MAX_EPS_CHANNELS;

	/* Power Management */
	dwc->pcgcctl = (uint32_t *) (reg_base + DWC_OTG_PCGCCTL_OFFSET);


	/*
	 * Store the contents of the hardware configuration registers here for
	 * easy access later.
	 */
	dwc->hwcfgs.hwcfg1.d32 = readl(&dwc->core_global_regs->ghwcfg1);
	dwc->hwcfgs.hwcfg2.d32 = readl(&dwc->core_global_regs->ghwcfg2);
	dwc->hwcfgs.hwcfg3.d32 = readl(&dwc->core_global_regs->ghwcfg3);
	dwc->hwcfgs.hwcfg4.d32 = readl(&dwc->core_global_regs->ghwcfg4);

	/* Force host mode to get HPTXFSIZ exact power on value */
	{
		gusbcfg_data_t gusbcfg = {.d32 = 0 };

		gusbcfg.d32 =  readl(&dwc->core_global_regs->gusbcfg);
		gusbcfg.b.force_host_mode = 1;
		writel(gusbcfg.d32, &dwc->core_global_regs->gusbcfg);
		mdelay(100);

		dwc->hwcfgs.hptxfsiz.d32 = readl(&dwc->core_global_regs->hptxfsiz);

		gusbcfg.d32 =  readl(&dwc->core_global_regs->gusbcfg);
		gusbcfg.b.force_host_mode = 0;
		writel(gusbcfg.d32, &dwc->core_global_regs->gusbcfg);
		mdelay(100);
	}

	dwc->hwcfgs.hcfg.d32 = readl(&dwc->host_if.host_global_regs->hcfg);
	dwc->hwcfgs.dcfg.d32 = readl(&dwc->dev_if.dev_global_regs->dcfg);

	/*
	 * Set the SRP sucess bit for FS-I2c
	 */
	//dwc->srp_success = 0;
	//dwc->srp_timer_started = 0;

	dwc->snpsid = readl(&dwc->core_global_regs->gsnpsid);

	dev_info(dwc->dev, "Core Release: %x.%x%x%x\n",
		(dwc->snpsid >> 12 & 0xF),
		(dwc->snpsid >> 8 & 0xF),
		(dwc->snpsid >> 4 & 0xF), (dwc->snpsid & 0xF));
}

/**
 * This function calculates the number of IN EPS
 * using GHWCFG1 and GHWCFG2 registers values
 *
 * @param dwc Programming view of the DWC_otg controller
 */
static uint32_t calc_num_in_eps(struct dwc2 *dwc)
{
	uint32_t num_in_eps = 0;
	uint32_t num_eps = dwc->hwcfgs.hwcfg2.b.num_dev_ep;
	uint32_t num_tx_fifos = dwc->hwcfgs.hwcfg4.b.num_in_eps;
	int i;

#define DWC_IS_INEP(_i)							\
	({								\
		unsigned int epdir = dwc->hwcfgs.hwcfg1.d32 >> ((_i) * 2);	\
		epdir &= 0x3;						\
									\
		((epdir == DWC_HWCFG1_DIR_BIDIR) || (epdir == DWC_HWCFG1_DIR_IN)); \
	})

	for (i = 0; i < num_eps; ++i) {
		if (DWC_IS_INEP(i))
			num_in_eps++;
	}

#undef DWC_IS_INEP

	if (dwc->hwcfgs.hwcfg4.b.ded_fifo_en)
		num_in_eps = (num_in_eps > num_tx_fifos) ? num_tx_fifos : num_in_eps;

	return num_in_eps;
}

/**
 * This function calculates the number of OUT EPS
 * using GHWCFG1 and GHWCFG2 registers values
 *
 * @param dwc Programming view of the DWC_otg controller
 */
static uint32_t calc_num_out_eps(struct dwc2 *dwc)
{
	uint32_t num_out_eps = 0;
	uint32_t num_eps = dwc->hwcfgs.hwcfg2.b.num_dev_ep;
	int i;

#define DWC_IS_OUTEP(_i)						\
	({								\
		unsigned int epdir = dwc->hwcfgs.hwcfg1.d32 >> ((_i) * 2);	\
		epdir &= 0x3;						\
									\
		((epdir == DWC_HWCFG1_DIR_BIDIR) || (epdir == DWC_HWCFG1_DIR_OUT)); \
	})

	for (i = 0; i < num_eps; ++i) {
		if (DWC_IS_OUTEP(i))
			num_out_eps++;
	}

#undef DWC_IS_OUTEP

	return num_out_eps;
}

/*
 * dwc2_core_init - Low-level initialization of DWC2 Core
 * @dwc: Pointer to our controller context structure
 *
 * Returns 0 on success otherwise negative errno.
 */
int dwc2_core_init(struct dwc2 *dwc)
{
	int i = 0;
	dwc_otg_core_global_regs_t *global_regs = dwc->core_global_regs;
	struct dwc2_dev_if *dev_if = &dwc->dev_if;
	gahbcfg_data_t ahbcfg = {.d32 = 0 };
	gusbcfg_data_t usbcfg = {.d32 = 0 };
	gotgctl_data_t gotgctl = {.d32 = 0 };

	/* Common Initialization */
	usbcfg.d32 = readl(&global_regs->gusbcfg);
	usbcfg.b.ulpi_ext_vbus_drv = 0;
	usbcfg.b.term_sel_dl_pulse = 0;
	writel(usbcfg.d32, &global_regs->gusbcfg);

	/* Reset the Controller */
	dwc2_core_reset(dwc);

	/* TODO: use configurable value here! */
	dwc->adp_enable = 0;
	dwc->power_down = 0;

	dwc->otg_sts = 0;

	/* Initialize parameters from Hardware configuration registers. */
	dwc->dev_if.num_in_eps = calc_num_in_eps(dwc);
	dwc->dev_if.num_out_eps = calc_num_out_eps(dwc);

	WARN( (dwc->dev_if.num_in_eps != dwc->dev_if.num_out_eps),
		"IN and OUT endpoint number not match(%d=%d)\n",
		dwc->dev_if.num_in_eps, dwc->dev_if.num_out_eps);

	for (i = 0; i < dwc->hwcfgs.hwcfg4.b.num_dev_perio_in_ep; i++) {
		dev_if->perio_tx_fifo_size[i] =
			readl(&global_regs->dtxfsiz[i]) >> 16;
	}

	for (i = 0; i < dwc->hwcfgs.hwcfg4.b.num_in_eps; i++) {
		dev_if->tx_fifo_size[i] =
			readl(&global_regs->dtxfsiz[i]) >> 16;
	}

	dwc->total_fifo_size = dwc->hwcfgs.hwcfg3.b.dfifo_depth;
	dwc->rx_fifo_size = readl(&global_regs->grxfsiz);
	dwc->nperio_tx_fifo_size = readl(&global_regs->gnptxfsiz) >> 16;

	dev_info(dwc->dev, "Total FIFO SZ=%d\n", dwc->total_fifo_size);
	dev_info(dwc->dev, "Rx FIFO SZ=%d\n", dwc->rx_fifo_size);
	dev_info(dwc->dev, "NP Tx FIFO SZ=%d\n", dwc->nperio_tx_fifo_size);

	/* High speed PHY init */
	if (!dwc->phy_inited) {
		dwc->phy_inited = 1;

		/* UTMI+ interface */
		usbcfg.b.ulpi_utmi_sel = 0;
		/* 16-bit */
		usbcfg.b.phyif = 1;
		writel(usbcfg.d32, &global_regs->gusbcfg);

		/* Reset after setting the PHY parameters */
		dwc2_core_reset(dwc);
	}

	/* we use UTMI+ PHY, so set ULPI fields to 0 */
	usbcfg.d32 = readl(&global_regs->gusbcfg);
	usbcfg.b.ulpi_fsls = 0;
	usbcfg.b.ulpi_clk_sus_m = 0;
	writel(usbcfg.d32, &global_regs->gusbcfg);

	/* External DMA Mode burst Settings */
	dev_info(dwc->dev, "Architecture: External DMA\n");
	ahbcfg.d32 = 0;
	ahbcfg.b.hburstlen = DWC_GAHBCFG_EXT_DMA_BURST_16word;
	ahbcfg.b.dmaenable = dwc->dma_enable;
	writel(ahbcfg.d32, &global_regs->gahbcfg);

	usbcfg.d32 = readl(&global_regs->gusbcfg);
	/* TODO: allow user to enable HNP and SRP dynamically */
	usbcfg.b.hnpcap = 1;
	usbcfg.b.srpcap = 1;
	writel(usbcfg.d32, &global_regs->gusbcfg);

	/* TODO: LPM settings here if enable LPM */

	gotgctl.d32 = readl(&dwc->core_global_regs->gotgctl);
	gotgctl.b.otgver = dwc->otg_ver;
	writel(gotgctl.d32, &dwc->core_global_regs->gotgctl);

	/* Do device or host intialization based on mode during PCD
	 * and HCD initialization  */
	if (dwc2_is_host_mode(dwc)) {
		dev_info(dwc->dev, "Host Mode!\n");
		dwc->op_state = DWC2_A_HOST;
		/* TODO: vbus */
		//jz_dwc_set_vbus(dwc, 1);
	} else {
		dev_info(dwc->dev, "Device Mode!\n");
		dwc->op_state = DWC2_B_PERIPHERAL;
		/* TODO: vbus */
		//jz_dwc_set_vbus(dwc, 0);
	}

	/* Enable common interrupts */
	dwc2_enable_common_interrupts(dwc);

	return 0;
}

static void dwc2_core_exit(struct dwc2 *dwc)
{
}

/*
 * Caller must take care of lock
 */
static void dwc2_gadget_disconnect(struct dwc2 *dwc)
{
	if (dwc->gadget_driver && dwc->gadget_driver->disconnect) {
		dwc2_spin_unlock(dwc);
		dwc->gadget_driver->disconnect(&dwc->gadget);
		dwc2_spin_lock(dwc);
	}
}

static void dwc2_gadget_resume(struct dwc2 *dwc) {
	if (dwc->gadget_driver && dwc->gadget_driver->resume) {
		dwc2_spin_unlock(dwc);
		dwc->gadget_driver->resume(&dwc->gadget);
		dwc2_spin_lock(dwc);
	}
}

static void dwc2_gadget_suspend(struct dwc2 *dwc)
{
	if (dwc->gadget_driver && dwc->gadget_driver->suspend) {
		dwc2_spin_unlock(dwc);
		dwc->gadget_driver->suspend(&dwc->gadget);
		dwc2_spin_lock(dwc);
	}
}

static void dwc2_handle_mode_mismatch_intr(struct dwc2 *dwc)
{
	gintsts_data_t gintsts;

	/* Just Log a warnning message */
	dev_warn(dwc->dev, "Mode Mismatch Interrupt: currently in %s mode\n",
		dwc2_is_host_mode(dwc) ? "Host" : "Device");

	/* Clear interrupt */
	gintsts.d32 = 0;
	gintsts.b.modemismatch = 1;
	writel(gintsts.d32, &dwc->core_global_regs->gintsts);
}

static void dwc2_handle_otg_intr(struct dwc2 *dwc) {
	dwc_otg_core_global_regs_t *global_regs = dwc->core_global_regs;
	gotgint_data_t gotgint;
	gotgctl_data_t gotgctl;
	gintsts_data_t gintsts;
	//gintmsk_data_t gintmsk;
	//gpwrdn_data_t gpwrdn;

	gotgint.d32 = readl(&global_regs->gotgint);
	DWC2_CORE_DEBUG_MSG("%s: otgint = 0x%08x\n",
			__func__, gotgint.d32);

	if (gotgint.b.sesenddet) {
		dev_dbg(dwc->dev, "session end detected!\n");

		gotgctl.d32 = readl(&global_regs->gotgctl);
		DWC2_CORE_DEBUG_MSG("%s:%d gotgctl = 0x%08x op_state = %d\n",
				__func__, __LINE__, gotgctl.d32, dwc->op_state);

		if (dwc->op_state == DWC2_B_HOST) {
			 /* TODO: Initialized the Core for Device mode. */
			dwc->op_state = DWC2_B_PERIPHERAL;
		} else {
			/* If not B_HOST and Device HNP still set. HNP
			 * Did not succeed!*/
			if (gotgctl.b.devhnpen) {
				dev_err(dwc->dev, "Device Not Connected/Responding!\n");
			}

			/*
			 * TODO: If Session End Detected the B-Cable has been disconnected.
			 *       Reset PCD and Gadget driver to a clean state.
			 */
			dwc->lx_state = DWC_OTG_L0;
			dwc2_gadget_handle_session_end(dwc);
			dwc2_gadget_disconnect(dwc);

			/* TODO: if adp enable, handle ADP Sense here */
		}

		gotgctl.d32 = readl(&global_regs->gotgctl);
		gotgctl.b.devhnpen = 0;
		writel(gotgctl.d32, &global_regs->gotgctl);
	}

	if (gotgint.b.sesreqsucstschng) {
		dev_info(dwc->dev, " ++OTG Interrupt: Session Reqeust Success Status Change++\n");
		gotgctl.d32 = readl(&global_regs->gotgctl);
		if (gotgctl.b.sesreqscs) {
			/* TODO: did gadget suspended??? */
			dwc2_gadget_resume(dwc);

			/* Clear Session Request */
			gotgctl.b.sesreq = 0;
			writel(gotgctl.d32, &global_regs->gotgctl);
		}
	}

	if (gotgint.b.hstnegsucstschng) {
		dev_info(dwc->dev, "OTG Interrupt: hstnegsucstschng\n");
		/*
		 * Print statements during the HNP interrupt handling
		 * can cause it to fail.
		 */
		gotgctl.d32 = readl(&global_regs->gotgctl);
		if (gotgctl.b.hstnegscs) {
			if (dwc2_is_host_mode(dwc)) {
				/* TODO: handle Host Negotiation Success here! */
				dev_info(dwc->dev, "%s:%d Host Negotiation Success\n", __func__, __LINE__);
			}
		} else {
			gotgctl.b.hnpreq = 0;
			gotgctl.b.devhnpen = 0;
			writel(gotgctl.d32, &global_regs->gotgctl);
			dev_err(dwc->dev, "Device Not Connected/Responding\n");
		}
	}

	if (gotgint.b.hstnegdet) {
		/* The disconnect interrupt is set at the same time as
		 * Host Negotiation Detected.  During the mode
		 * switch all interrupts are cleared so the disconnect
		 * interrupt handler will not get executed.
		 */
		dev_info(dwc->dev, "++OTG Interrupt: "
			"Host Negotiation Detected++ (%s)\n",
			(dwc2_is_host_mode(dwc) ? "Host" : "Device"));
		if (dwc2_is_device_mode(dwc)) {
			dev_info(dwc->dev, "a_suspend->a_peripheral (%d)\n", dwc->op_state);

			dwc2_spin_unlock(dwc);
			//cil_hcd_disconnect(dwc);
			//cil_pcd_start(dwc);
			dwc2_spin_lock(dwc);
			dwc->op_state = DWC2_A_PERIPHERAL;
		} else {
			/* TODO: */
		}
	}

	if (gotgint.b.adevtoutchng) {
		dev_info(dwc->dev, " ++OTG Interrupt: "
			"A-Device Timeout Change++\n");
	}

	if (gotgint.b.debdone) {
		dev_info(dwc->dev, " ++OTG Interrupt: " "Debounce Done++\n");
	}

	/* Clear GOTGINT */
	writel(gotgint.d32, &dwc->core_global_regs->gotgint);

	/* Clear interrupt */
	gintsts.d32 = 0;
	gintsts.b.otgintr = 1;
	writel(gintsts.d32, &dwc->core_global_regs->gintsts);
}

static void dwc2_handle_conn_id_status_change_intr(struct dwc2 *dwc) {
	gintsts_data_t gintsts;

	/* Just Log a warnning message */
	dev_info(dwc->dev, "ID PIN CHANGED!\n");

	/* Clear interrupt */
	gintsts.d32 = 0;
	gintsts.b.conidstschng = 1;
	writel(gintsts.d32, &dwc->core_global_regs->gintsts);
}

static void dwc2_handle_disconnect_intr(struct dwc2 *dwc) {
	gintsts_data_t gintsts;

	if (dwc2_is_device_mode(dwc)) {
		gotgctl_data_t gotgctl;

		gotgctl.d32 = readl(&dwc->core_global_regs->gotgctl);
		if (gotgctl.b.hstsethnpen == 1) {
			/* Do nothing, if HNP in process the OTG
			 * interrupt "Host Negotiation Detected"
			 * interrupt will do the mode switch.
			 */
		} else if (gotgctl.b.devhnpen == 0) {
			dev_info(dwc->dev, "====>enter %s:%d\n", __func__, __LINE__);
#if 0
			/* If in device mode Disconnect and stop the HCD, then
			 * start the PCD. */
			DWC_SPINUNLOCK(dwc->lock);
			cil_hcd_disconnect(dwc);
			cil_pcd_start(dwc);
			DWC_SPINLOCK(dwc->lock);
			dwc->op_state = B_PERIPHERAL;
#endif
		} else {
			dev_warn(dwc->dev, "!a_peripheral && !devhnpen\n");
		}
	}

	gintsts.d32 = 0;
	gintsts.b.disconnect = 1;
	writel(gintsts.d32, &dwc->core_global_regs->gintsts);
}

/**
 * This interrupt indicates that a device is initiating the Session
 * Request Protocol to request the host to turn on bus power so a new
 * session can begin. The handler responds by turning on bus power. If
 * the DWC_otg controller is in low power mode, the handler brings the
 * controller out of low power mode before turning on bus power.
 *
 * @param dwc Programming view of DWC_otg controller.
 */
static void dwc2_handle_session_req_intr(struct dwc2 *dwc)
{
	gintsts_data_t gintsts;

	dev_dbg(dwc->dev, "++Session Request Interrupt++\n");

	if (dwc2_is_device_mode(dwc)) {
		dev_info(dwc->dev, "SRP: Device mode\n");
	} else {
		/* TODO: Handle Host Mode here! */
	}

	/* Clear interrupt */
	gintsts.d32 = 0;
	gintsts.b.sessreqintr = 1;
	writel(gintsts.d32, &dwc->core_global_regs->gintsts);
}

/**
 * This interrupt indicates that the DWC_otg controller has detected a
 * resume or remote wakeup sequence. If the DWC_otg controller is in
 * low power mode, the handler must brings the controller out of low
 * power mode. The controller automatically begins resume
 * signaling. The handler schedules a time to stop resume signaling.
 */
static void dwc2_handle_wakeup_detected_intr(struct dwc2 *dwc) {
	gintsts_data_t gintsts;

	if (dwc2_is_device_mode(dwc)) {
		dctl_data_t dctl = {.d32 = 0 };
		if (dwc->lx_state == DWC_OTG_L2) {
			/* Clear the Remote Wakeup Signaling */

			dctl.d32 = readl(&dwc->dev_if.dev_global_regs->dctl);
			dctl.b.rmtwkupsig = 1;
			writel(dctl.d32, &dwc->dev_if.dev_global_regs->dctl);

			dwc2_gadget_resume(dwc);
		} else {
			glpmcfg_data_t lpmcfg;
			lpmcfg.d32 = readl(&dwc->core_global_regs->glpmcfg);
			lpmcfg.b.hird_thres &= (~(1 << 4));
			writel(lpmcfg.d32, &dwc->core_global_regs->glpmcfg);
		}
		/** Change to L0 state*/
		dwc->lx_state = DWC_OTG_L0;
	}

	/* Clear interrupt */
	gintsts.d32 = 0;
	gintsts.b.wkupintr = 1;
	writel(gintsts.d32, &dwc->core_global_regs->gintsts);
}

/**
 * This interrupt indicates that SUSPEND state has been detected on
 * the USB.
 *
 * For HNP the USB Suspend interrupt signals the change from
 * "a_peripheral" to "a_host".
 *
 * When power management is enabled the core will be put in low power
 * mode.
 */
static void dwc2_handle_usb_suspend_intr(struct dwc2 *dwc)
{
	dsts_data_t dsts;
	gintsts_data_t gintsts;

	if (dwc2_is_device_mode(dwc)) {
		/* Check the Device status register to determine if the Suspend
		 * state is active. */
		dsts.d32 = readl(&dwc->dev_if.dev_global_regs->dsts);
		dev_dbg(dwc->dev, "DSTS = 0x%08x\n", dsts.d32);

		dwc2_gadget_suspend(dwc);
	} else {
		dev_info(dwc->dev, "%s:%d: host mode not implemented!\n", __func__, __LINE__);
	}

	/* Change to L2(suspend) state */
	dwc->lx_state = DWC_OTG_L2;

	/* Clear interrupt */
	gintsts.d32 = 0;
	gintsts.b.usbsuspend = 1;
	writel(gintsts.d32, &dwc->core_global_regs->gintsts);
}

static void dwc2_handle_restore_done_intr(struct dwc2 *dwc) {
	gintsts_data_t gintsts;

	gintsts.d32 = 0;
	gintsts.b.restoredone = 1;
	writel(gintsts.d32, &dwc->core_global_regs->gintsts);
}

static void dwc2_handle_port_intr(struct dwc2 *dwc) {
	gintsts_data_t gintsts;

	gintsts.d32 = 0;
	gintsts.b.portintr = 1;
	writel(gintsts.d32, &dwc->core_global_regs->gintsts);
}

static void dwc2_handle_common_interrupts(struct dwc2 *dwc, gintsts_data_t *gintr_status) {
	//gpwrdn_data_t gpwrdn = {.d32 = 0 };

	if (gintr_status->b.modemismatch) {
		dwc2_handle_mode_mismatch_intr(dwc);
		gintr_status->b.modemismatch =  0;
	}
	if (gintr_status->b.otgintr) {
		dwc2_handle_otg_intr(dwc);
		gintr_status->b.otgintr = 0;
	}
	if (gintr_status->b.conidstschng) {
		dwc2_handle_conn_id_status_change_intr(dwc);
		gintr_status->b.conidstschng = 0;
	}
	if (gintr_status->b.disconnect) {
		dwc2_handle_disconnect_intr(dwc);
		gintr_status->b.disconnect = 0;
	}
	if (gintr_status->b.sessreqintr) {
		dwc2_handle_session_req_intr(dwc);
		gintr_status->b.sessreqintr = 0;
	}
	if (gintr_status->b.wkupintr) {
		dwc2_handle_wakeup_detected_intr(dwc);
		gintr_status->b.wkupintr = 0;
	}
	if (gintr_status->b.usbsuspend) {
		dwc2_handle_usb_suspend_intr(dwc);
		gintr_status->b.usbsuspend = 0;
	}
#ifdef CONFIG_USB_DWC2_LPM
	if (gintr_status->b.lpmtranrcvd) {
		dwc2_handle_lpm_intr(dwc);
	}
#endif
	if (gintr_status->b.restoredone) {
		dwc2_handle_restore_done_intr(dwc);
		gintr_status->b.restoredone = 0;
	}
	if (gintr_status->b.portintr && dwc2_is_device_mode(dwc)) {
		/*
		 * The port interrupt occurs while in device mode with HPRT0
		 * Port Enable/Disable.
		 */
		dwc2_handle_port_intr(dwc);
		gintr_status->b.portintr = 0;
	}

	/* TODO: Handle ADP interrupt here */

	/* TODO: handle gpwrdn interrupts here */
	// gpwrdn.d32 = readl(&dwc->core_global_regs->gpwrdn);
}

static inline int dwc_lock(struct dwc2 *dwc)
{
	if (atomic_inc_return(&dwc->in_irq) == 1) {
		dwc->owner_cpu = smp_processor_id();
		return 0;
	} else {
		atomic_dec(&dwc->in_irq);
		return -1;
	}
}

static inline void dwc_unlock(struct dwc2 *dwc)
{
	atomic_dec(&dwc->in_irq);
}

static irqreturn_t dwc2_interrupt(int irq, void *_dwc) {
	struct dwc2			*dwc	     = _dwc;
	dwc_otg_core_global_regs_t	*global_regs = dwc->core_global_regs;
	gintsts_data_t			 gintr_status;
	gintsts_data_t			 gintsts;
	gintsts_data_t			 gintmsk;

	/*
	 * TODO: is it better to call spin_try_lock() here?
	 *       aka, if we can not get the lock, we have two selection:
	 *       1. spin until we get the lock or
	 *       2. abort this interrupt, let the DWC core interrupt us later
	 */

	// spin_lock(&dwc->lock);
	if (!spin_trylock(&dwc->lock)) {
		return IRQ_HANDLED;
	}

	if (dwc_lock(dwc) < 0) {
		spin_unlock(&dwc->lock);
		return IRQ_HANDLED;
	}

	gintsts.d32 = readl(&global_regs->gintsts);
	gintmsk.d32 = readl(&global_regs->gintmsk);
	gintr_status.d32 = gintsts.d32 & gintmsk.d32;

	DWC2_CORE_DEBUG_MSG("%s:%d gintsts=0x%08x & gintmsk=0x%08x = 0x%08x\n",
		__func__, __LINE__, gintsts.d32, gintmsk.d32, gintr_status.d32);

	dwc2_handle_common_interrupts(dwc, &gintr_status);

	if (dwc2_is_device_mode(dwc)) {
		dwc2_handle_device_mode_interrupt(dwc, &gintr_status);
	} else {
		dev_err(dwc->dev, "HOST MODE NOT IMPLEMENTED!\n");
	}

	if (gintr_status.d32) {
		printk("===>unhandled interrupt! gintr_status = 0x%08x\n",
			gintr_status.d32);
	}

	dwc_unlock(dwc);
	spin_unlock(&dwc->lock);

	return IRQ_HANDLED;
}

static int dwc2_probe(struct platform_device *pdev)
{
	struct resource		*res;
	struct dwc2		*dwc;
	struct device		*dev = &pdev->dev;
	int			irq;
	int			ret = -ENOMEM;
	void __iomem		*regs;

	dwc = devm_kzalloc(dev, sizeof(*dwc), GFP_KERNEL);
	if (!dwc) {
		dev_err(dev, "not enough memory\n");
		return -ENOMEM;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq == -ENXIO) {
		dev_err(dev, "missing IRQ\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "missing memory resource\n");
		return -ENODEV;
	}

	res = devm_request_mem_region(dev, res->start, resource_size(res), dev_name(dev));
	if (!res) {
		dev_err(dev, "can't request mem region\n");
		return -ENOMEM;
	}

	regs = devm_ioremap_nocache(dev, res->start, resource_size(res));
	if (!regs) {
		dev_err(dev, "ioremap failed\n");
		return -ENOMEM;
	}

	spin_lock_init(&dwc->lock);
	atomic_set(&dwc->in_irq, 0);
	platform_set_drvdata(pdev, dwc);

	dwc->regs = regs;
	dwc->pdev = pdev;
	dwc->dev  = dev;

	dwc->dma_enable	     = 1;
	dwc->dma_desc_enable = 0;

	if (dwc->dma_enable) {
		if (dwc->dma_desc_enable) {
			dev_info(dwc->dev, "Using Descriptor DMA mode\n");
		} else {
			dev_info(dwc->dev, "Using Buffer DMA mode\n");
		}
	} else {
		dev_info(dwc->dev, "Using Slave mode\n");
		dwc->dma_desc_enable = 0;
	}

	/*
	 * Set OTG version supported
	 * 0: OTG 1.3
	 * 1: OTG 2.0
	 */
	dwc->otg_ver = 0;

	ret = request_irq(irq, dwc2_interrupt, 0, "dwc2", dwc);
	if (ret) {
		dev_err(dwc->dev, "failed to request irq #%d --> %d\n",
			irq, ret);
		goto err0;
	}

	dwc2_init_csr(dwc);

	dwc2_disable_global_interrupts(dwc);
	ret = dwc2_core_init(dwc);
	if (ret) {
		dev_err(dev, "failed to initialize core\n");
		goto err1;
	}


	//dwc2_set_mode(dwc, DWC3_GCTL_PRTCAP_OTG);
#if 0
	ret = dwc2_host_init(dwc);
	if (ret) {
		dev_err(dev, "failed to initialize host\n");
		goto err2;
	}
#endif

	ret = dwc2_gadget_init(dwc);
	if (ret) {
		dev_err(dev, "failed to initialize gadget\n");
		goto err3;
	}

	/* TODO: if enable ADP support, start ADP here instead of enable global interrupts */
	dwc2_enable_global_interrupts(dwc);

	ret = dwc2_debugfs_init(dwc);
	if (ret) {
		dev_err(dev, "failed to initialize debugfs\n");
		goto err4;
	}

	return 0;

err4:
	dwc2_gadget_exit(dwc);

err3:
	//dwc2_host_exit(dwc);

err2:
	dwc2_core_exit(dwc);

err1:
	free_irq(irq, dwc);

err0:
	return ret;
}

static int dwc2_remove(struct platform_device *pdev)
{
	struct dwc2	*dwc = platform_get_drvdata(pdev);
	int		 irq;

//	dwc2_host_exit(dwc);
	dwc2_gadget_exit(dwc);
	dwc2_core_exit(dwc);

	irq = platform_get_irq(to_platform_device(dwc->dev), 0);
	free_irq(irq, dwc);

	return 0;
}

static struct platform_driver dwc2_driver = {
	.probe		= dwc2_probe,
	.remove		= dwc2_remove,
	.driver		= {
		.name	= "dwc2",
	},
};

static int __init dwc2_init(void)
{
	return platform_driver_register(&dwc2_driver);
}

static void __exit dwc2_exit(void)
{
	platform_driver_unregister(&dwc2_driver);
}

module_init(dwc2_init);
module_exit(dwc2_exit);

MODULE_ALIAS("platform:dwc2");
MODULE_AUTHOR("Lutts Cao <slcao@ingenic.cn>");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("DesignWare USB2.0 OTG Controller Driver");
