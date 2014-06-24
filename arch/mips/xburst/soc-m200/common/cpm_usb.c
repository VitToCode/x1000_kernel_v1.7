#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <soc/base.h>
#include <soc/extal.h>
#include <soc/cpm.h>

#define USBRDT_VBFIL_LD_EN		25
#define USBPCR_TXPREEMPHTUNE		6
#define USBPCR_POR			22
#define USBPCR_USB_MODE			31
#define USBPCR_COMMONONN		25
#define USBPCR_VBUSVLDEXT		24
#define USBPCR_VBUSVLDEXTSEL		23
#define USBPCR_OTG_DISABLE		20
#define USBPCR_IDPULLUP_MASK		28
#define OPCR_SPENDN0			7
#define USBPCR1_USB_SEL			28
#define USBPCR1_WORD_IF0		19
#define USBPCR1_WORD_IF1		18

#define __cpm_inl(off)		inl(CPM_IOBASE + (off))
#define __cpm_outl(val,off)	outl(val,CPM_IOBASE + (off))
#define __cpm_clear_bit(val,off)	do{__cpm_outl((__cpm_inl(off) & ~(1<<(val))),off);}while(0)
#define __cpm_set_bit(val,off)	do{__cpm_outl((__cpm_inl(off) |  (1<<val)),off);}while(0)
#define __cpm_test_bit(val,off)	(__cpm_inl(off) & (0x1<<val))

int cpm_start_ohci(void)
{
	int tmp;
	static int has_reset = 0;

	/* The PLL uses CLKCORE as reference */
	tmp = __cpm_inl(CPM_USBPCR1);
	tmp &= ~(0x1<<31);
	__cpm_outl(tmp,CPM_USBPCR1);

	tmp = __cpm_inl(CPM_USBPCR1);
	tmp |= (0x3<<26);
	__cpm_outl(tmp,CPM_USBPCR1);

	/* selects the reference clock frequency 48M */
	tmp = __cpm_inl(CPM_USBPCR1);
	tmp &= ~(0x3<<24);

	tmp |=(2<<24);
	__cpm_outl(tmp,CPM_USBPCR1);

	__cpm_set_bit(23,CPM_USBPCR1);
	__cpm_set_bit(22,CPM_USBPCR1);

	__cpm_set_bit(18,CPM_USBPCR1);
	__cpm_set_bit(19,CPM_USBPCR1);

	__cpm_set_bit(7, CPM_OPCR);
	__cpm_set_bit(6, CPM_OPCR);

	__cpm_set_bit(24, CPM_USBPCR);

	/* OTG PHY reset */
	__cpm_set_bit(22, CPM_USBPCR);
	udelay(30);
	__cpm_clear_bit(22, CPM_USBPCR);
	udelay(300);

	/* UHC soft reset */
	if(!has_reset) {
		__cpm_set_bit(20, CPM_USBPCR1);
		__cpm_set_bit(21, CPM_USBPCR1);
		__cpm_set_bit(13, CPM_SRBC);
		__cpm_set_bit(12, CPM_SRBC);
		__cpm_set_bit(11, CPM_SRBC);
		udelay(300);
		__cpm_clear_bit(20, CPM_USBPCR1);
		__cpm_clear_bit(21, CPM_USBPCR1);
		__cpm_clear_bit(13, CPM_SRBC);
		__cpm_clear_bit(12, CPM_SRBC);
		__cpm_clear_bit(11, CPM_SRBC);
		udelay(300);
		has_reset = 1;
	}

	return 0;
}
EXPORT_SYMBOL(cpm_start_ohci);

int cpm_stop_ohci(void)
{
	/* disable ohci phy power */
	__cpm_clear_bit(18,CPM_USBPCR1);
	__cpm_clear_bit(19,CPM_USBPCR1);

	__cpm_clear_bit(6, CPM_OPCR);
	__cpm_clear_bit(7, CPM_OPCR);

	return 0;
}
EXPORT_SYMBOL(cpm_stop_ohci);

int cpm_start_ehci(void)
{
	return cpm_start_ohci();
}
EXPORT_SYMBOL(cpm_start_ehci);

int cpm_stop_ehci(void)
{
	return cpm_stop_ohci();
}
EXPORT_SYMBOL(cpm_stop_ehci);

void otg_cpm_init(void)
{
	unsigned int ref_clk_div = CONFIG_EXTAL_CLOCK / 24;
	unsigned int usbpcr1;

	/* select dwc otg */
	__cpm_set_bit(USBPCR1_USB_SEL, CPM_USBPCR1);

	/* select utmi data bus width of port0 to 16bit/30M */
	__cpm_set_bit(USBPCR1_WORD_IF0, CPM_USBPCR1);

	usbpcr1 = __cpm_inl(CPM_USBPCR1);
	usbpcr1 &= ~(0x3 << 24);
	usbpcr1 |= (ref_clk_div << 24);
	__cpm_outl(usbpcr1, CPM_USBPCR1);

	/* fil */
	__cpm_outl(0, CPM_USBVBFIL);

	/* rdt */
	__cpm_outl(0x96, CPM_USBRDT);

	/* rdt - filload_en */
	__cpm_set_bit(USBRDT_VBFIL_LD_EN, CPM_USBRDT);

	/* TXRISETUNE & TXVREFTUNE. */
	//__cpm_outl(0x3f, CPM_USBPCR);
	//__cpm_outl(0x35, CPM_USBPCR);

	/* enable tx pre-emphasis */
	//__cpm_set_bit(USBPCR_TXPREEMPHTUNE, CPM_USBPCR);

	/* OTGTUNE adjust */
	//__cpm_outl(7 << 14, CPM_USBPCR);

	__cpm_outl(0x8380385F, CPM_USBPCR);
}
EXPORT_SYMBOL(otg_cpm_init);

void jz_otg_phy_init(void)
{
	pr_debug("init PHY\n");

	__cpm_set_bit(USBPCR_POR, CPM_USBPCR);
	msleep(1);
	__cpm_clear_bit(USBPCR_POR, CPM_USBPCR);
	msleep(1);
}
EXPORT_SYMBOL(jz_otg_phy_init);

void jz_otg_set_device_only_mode(void)
{
	pr_info("DWC IN DEVICE ONLY MODE\n");
	__cpm_clear_bit(USBPCR_USB_MODE, CPM_USBPCR);
	__cpm_clear_bit(USBPCR_OTG_DISABLE, CPM_USBPCR);
}
EXPORT_SYMBOL(jz_otg_set_device_only_mode);

void jz_otg_set_dual_mode(void)
{
	unsigned int tmp;

	pr_info("DWC IN OTG MODE\n");

	tmp = __cpm_inl(CPM_USBPCR);
	tmp |= 1 << USBPCR_USB_MODE;
	tmp |= 1 << USBPCR_VBUSVLDEXT;
	tmp |= 1 << USBPCR_VBUSVLDEXTSEL;
	tmp |= 1 << USBPCR_COMMONONN;

	tmp &= ~(1 << USBPCR_OTG_DISABLE);

	__cpm_outl(tmp & ~(0x03 << USBPCR_IDPULLUP_MASK), CPM_USBPCR);
}
EXPORT_SYMBOL(jz_otg_set_dual_mode);

void jz_otg_phy_suspend(int suspend)
{
	if (!suspend)
		__cpm_set_bit(7, CPM_OPCR);
	else
		__cpm_clear_bit(7, CPM_OPCR);
}
EXPORT_SYMBOL(jz_otg_phy_suspend);

int jz_otg_phy_is_suspend(void)
{
	return (!(__cpm_test_bit(7, CPM_OPCR)));
}
EXPORT_SYMBOL(jz_otg_phy_is_suspend);
