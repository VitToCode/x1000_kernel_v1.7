#ifndef __DRIVERS_USB_DWC2_CORE_H
#define __DRIVERS_USB_DWC2_CORE_H

#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/ioport.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/debugfs.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

#define DWC_PR(r)							\
	printk("DWC2: REG["#r"]=0x%08x\n", *((volatile unsigned int *)((r) | 0xb3500000)))

#define DWC_RR(_r) 	r##_r = (*((volatile unsigned int *)((_r) | 0xb3500000)))
#define DWC_P(v)	printk("DWC2: REG["#v"] = 0x%08x\n", r##v)

/** Macros defined for DWC OTG HW Release version */

#define OTG_CORE_REV_2_60a	0x4F54260A
#define OTG_CORE_REV_2_71a	0x4F54271A
#define OTG_CORE_REV_2_72a	0x4F54272A
#define OTG_CORE_REV_2_80a	0x4F54280A
#define OTG_CORE_REV_2_81a	0x4F54281A
#define OTG_CORE_REV_2_90a	0x4F54290A
#define OTG_CORE_REV_2_91a	0x4F54291A
#define OTG_CORE_REV_2_92a	0x4F54292A
#define OTG_CORE_REV_2_93a	0x4F54293A
#define OTG_CORE_REV_2_94a	0x4F54294A

/** Maximum number of Periodic FIFOs */
#define MAX_PERIO_FIFOS 15
/** Maximum number of Periodic FIFOs */
#define MAX_TX_FIFOS	15

/** Maximum number of Endpoints/HostChannels */
#define MAX_EPS_CHANNELS	16

#define DWC_MAX_PKT_CNT		1023
#define DWC_MAX_TRANSFER_SIZE	(1024 * 1023)

#include "dwc_otg_regs.h"

/**
 * struct dwc2_ep - device side endpoint representation
 * @usb_ep: usb endpoint
 * @request_list: list of requests for this endpoint
 * @dwc: pointer to DWC controller
 * @flags: endpoint flags (wedged, stalled, ...)
 * @number: endpoint number (1 - 15)
 * @type: set to bmAttributes & USB_ENDPOINT_XFERTYPE_MASK
 * @interval: the intervall on which the ISOC transfer is started
 * @name: a human readable name e.g. ep1out-bulk
 * @is_in: true for Tx(IN), false for Rx(OUT)
 * @tx_fifo_num: GRSTCTL.TxFNum of this ep
 * @desc: endpoint descriptor.  This pointer is set before the endpoint is
 *	enabled and remains valid until the endpoint is disabled.
 */
struct dwc2_ep {
	struct usb_ep		 usb_ep;
	struct list_head	 request_list;
	struct dwc2		*dwc;

	unsigned		 flags;
#define DWC2_EP_ENABLED		(1 << 0)
#define DWC2_EP_STALL		(1 << 1)
#define DWC2_EP_WEDGE		(1 << 2)
#define DWC2_EP_BUSY		(1 << 4)
#define DWC2_EP_PENDING_REQUEST	(1 << 5)
	/* This last one is specific to EP0 */
#define DWC3_EP0_DIR_IN		(1 << 31)

	u8			 number;
	u8			 type;
	u16			 maxp;
	u32			 interval;
	char			 name[20];
	unsigned		 is_in:1;

	unsigned		 tx_fifo_num;

	struct list_head	 garbage_list;
	const struct usb_endpoint_descriptor	*desc;
};

/**
 * States of EP0.
 */
enum dwc2_ep0_state {
	EP0_DISCONNECTED = 0,
	EP0_SETUP_PHASE,
	EP0_DATA_PHASE,
	EP0_STATUS_PHASE,
};

enum dwc2_device_state {
	DWC2_DEFAULT_STATE,
	DWC2_ADDRESS_STATE,
	DWC2_CONFIGURED_STATE,
};

enum dwc2_lx_state {
	/** On state */
	DWC_OTG_L0 = 0,
	/** LPM sleep state*/
	DWC_OTG_L1,
	/** USB suspend state*/
	DWC_OTG_L2,
	/** Off state*/
	DWC_OTG_L3
};

struct dwc2_request {
	struct usb_request	 request;
	struct list_head	 list;
	struct dwc2_ep		*dwc2_ep;

	int			 transfering;
	unsigned		 mapped:1;

	int			 trans_count_left;
	dma_addr_t		 next_dma_addr;
	int			 zlp_transfered;

	int			 xfersize;
	int			 pktcnt;
};

#define DWC_DEV_GLOBAL_REG_OFFSET 0x800
#define DWC_DEV_IN_EP_REG_OFFSET 0x900
#define DWC_EP_REG_OFFSET 0x20
#define DWC_DEV_OUT_EP_REG_OFFSET 0xB00

/**
 * struct dwc2_dev_if - Device-specific information
 * @dev_global_regs: Pointer to device Global registers 800h
 * @in_ep_regs: Device Logical IN Endpoint-Specific Registers 900h-AFCh
 * @out_ep_regs: Device Logical OUT Endpoint-Specific Registers B00h-CFCh
 * @speed: Device Speed 0: Unknown, 1: LS, 2: FS, 3: HS
 * @num_in_eps: number of Tx EPs
 * @num_out_eps: number of Rx EPs
 * @perio_tx_fifo_size: size of periodic FIFOs (Bytes)
 * @tx_fifo_size: size of Tx FIFOs (Bytes)
 */
struct dwc2_dev_if {
	dwc_otg_device_global_regs_t	*dev_global_regs;
	dwc_otg_dev_in_ep_regs_t	*in_ep_regs[MAX_EPS_CHANNELS];
	dwc_otg_dev_out_ep_regs_t	*out_ep_regs[MAX_EPS_CHANNELS];
	uint8_t				 speed;
	uint8_t				 num_in_eps;
	uint8_t				 num_out_eps;
	uint16_t			 perio_tx_fifo_size[MAX_PERIO_FIFOS];
	uint16_t			 tx_fifo_size[MAX_TX_FIFOS];
};

#define DWC_OTG_HOST_GLOBAL_REG_OFFSET 0x400
#define DWC_OTG_HOST_PORT_REGS_OFFSET 0x440
#define DWC_OTG_HOST_CHAN_REGS_OFFSET 0x500
#define DWC_OTG_CHAN_REGS_OFFSET 0x20

/**
 * struct dwc2_host_if - Host-specific information
 * @host_global_regs: Host Global Registers starting at offset 400h
 * @hprt0: Host Port 0 Control and Status Register
 * @hc_regs: Host Channel Specific Registers at offsets 500h-5FCh
 * @num_host_channels: Number of Host Channels (range: 1-16)
 * @perio_ep_supported: Periodic EPs supported (0: no, 1: yes)
 * @perio_tx_fifo_size: Periodic Tx FIFO Size (Only 1 host periodic Tx FIFO)
 */
struct dwc2_host_if {
	dwc_otg_host_global_regs_t	*host_global_regs;
	volatile uint32_t		*hprt0;
	dwc_otg_hc_regs_t		*hc_regs[MAX_EPS_CHANNELS];
	uint8_t				 num_host_channels;
	uint8_t				 perio_eps_supported;
	uint16_t			 perio_tx_fifo_size;
};

#define DWC_OTG_PCGCCTL_OFFSET	0xE00

/**
 * struct dwc2_hwcfgs - Hardware Configurations, stored here for convenience
 * @hwcfg1: HWCFG1
 * @hwcfg2: HWCFG2
 * @hwcfg3: HWCFG3
 * @hwcfg4: HWCFG4
 * @hptxfsiz: HPTXFSIZ
 * @hcfg: HCFG
 * @dcfg: DCFG
 */
struct dwc2_hwcfgs {
	hwcfg1_data_t	hwcfg1;
	hwcfg2_data_t	hwcfg2;
	hwcfg3_data_t	hwcfg3;
	hwcfg4_data_t	hwcfg4;
	fifosize_data_t hptxfsiz;

	hcfg_data_t	hcfg;
	dcfg_data_t	dcfg;
};

#define DWC2_MAX_NUMBER_OF_SETUP_PKT		5
#define DWC_EP0_MAXPACKET	64

/**
 * struct dwc2 - representation of our controller
 * @ctrl_req: usb control request which is used for ep0
 * @ctrl_req_addr: dma address of ctrl_req
 * @status_buf: status buf for GET_STATUS request
 * @status_buf_addr: dma address of status_buf
 * @lock: for synchronizing
 * @pdev: for Communication with the glue layer
 * @dev: pointer to our struct device
 * @dma_enable: enable DMA, 0: Slave Mode, 1: DMA Mode
 * @dma_desc_enable: enable Scatter/Gather DMA, 0: enable, 1: disable
 * @core_global_regs: Core Global registers starting at offset 000h
 * @host_if: Host-specific information
 * @pcgcctl: Power and Clock Gating Control Register
 * @hwcfgs: copy of hwcfgs registers
 * @phy_inited: The PHY need only init once, but the core can re-init many times
 * @op_state: The operational State
 * @total_fifo_size: Total RAM for FIFOs (Bytes)
 * @rx_fifo_size: Size of Rx FIFO (Bytes)
 * @nperio_tx_fifo_size: Size of Non-periodic Tx FIFO (Bytes)
 * @power_down: Power Down Enable, 0: disable, 1: enable
 * @adp_enable: ADP support Enable
 * @regs: ioremap-ed HW register base address
 * @snpsid: Value from SNPSID register
 * @otg_ver: OTG revision supported, 0: OTG 1.3, 1: OTG 2.0
 * @otg_sts: OTG status flag used for HNP polling
 * @dev_if: Device-specific information
 * @eps: endpoints, OUT[num_out_eps] then IN[num_in_eps]
 * @gadget: device side representation of the peripheral controller
 * @gadget_driver: pointer to the gadget driver
 * @three_stage_setup: set if we perform a three phase setup
 * @ep0_expect_in: true when we expect a DATA IN transfer
 * @ep0state: state of endpoint zero
 */
struct dwc2 {
	struct usb_ctrlrequest		 ctrl_req;
	struct usb_ctrlrequest		*ctrl_req_virt;
	dma_addr_t			 ctrl_req_addr;
#define DWC2_CTRL_REQ_ACTUAL_ALLOC_SIZE	 PAGE_SIZE

	u16				 status_buf;
	dma_addr_t			 status_buf_addr;
	struct dwc2_request		 ep0_usb_req;
	/*
	 * because ep0out did not alloc disable operation,
	 * we use a shadow buffer here to avoid memory corruption
	 */
	u8				 ep0out_shadow_buf[DWC_EP0_MAXPACKET];
	void 				*ep0out_shadow_uncached;
	u32				 ep0out_shadow_dma;

	spinlock_t			 lock;
	atomic_t			 in_irq;
	int				 owner_cpu;

	struct device			*dev;
	struct platform_device 		*pdev;
	int				 dma_enable;
	int				 dma_desc_enable;
	dwc_otg_core_global_regs_t	*core_global_regs;
	struct dwc2_host_if		 host_if;
	volatile uint32_t		*pcgcctl;
	struct dwc2_hwcfgs		 hwcfgs;
	int				 phy_inited;
	/** The operational State, during transations
	 * (a_host>>a_peripherial and b_device=>b_host) this may not
	 * match the core but allows the software to determine
	 * transitions.
	 */
	uint8_t				 op_state;
	/** A-Device is a_host */
#define DWC2_A_HOST		(1)
	/** A-Device is a_suspend */
#define DWC2_A_SUSPEND		(2)
	/** A-Device is a_peripherial */
#define DWC2_A_PERIPHERAL	(3)
	/** B-Device is operating as a Peripheral. */
#define DWC2_B_PERIPHERAL	(4)
	/** B-Device is operating as a Host. */
#define DWC2_B_HOST		(5)

	enum dwc2_lx_state		 lx_state;
	uint16_t			 total_fifo_size;
	uint16_t			 rx_fifo_size;
	uint16_t			 nperio_tx_fifo_size;
	uint32_t			 power_down;
	uint32_t			 adp_enable;
	void __iomem			*regs;
	uint32_t			 snpsid;
	uint32_t			 otg_ver;
	uint8_t				 otg_sts;
	struct dwc2_dev_if		 dev_if;
	struct dwc2_ep			*eps[MAX_EPS_CHANNELS * 2];
	struct usb_gadget		 gadget;
	struct usb_gadget_driver	*gadget_driver;

	unsigned			 is_selfpowered:1;
	unsigned			 three_stage_setup:1;
	unsigned			 ep0_expect_in:1;
	unsigned			 remote_wakeup_enable:1;
	unsigned			 delayed_status:1;
	unsigned			 delayed_status_sent:1;
	unsigned			 b_hnp_enable:1;
	unsigned			 a_hnp_support:1;
	unsigned			 a_alt_hnp_support:1;

	enum dwc2_ep0_state		 ep0state;
	enum dwc2_device_state		 dev_state;

	struct timer_list delayed_status_watchdog;

	int setup_prepared;

	struct dentry		*root;
	u8			test_mode;
	u8			test_mode_nr;
};

#define dwc2_spin_lock_irqsave(__dwc, flg)				\
	do {								\
		struct dwc2 *_dwc = (__dwc);				\
	__dwc2_lock_barrior:						\
		spin_lock_irqsave(&_dwc->lock, (flg));			\
		if (atomic_read(&_dwc->in_irq)) {			\
			if (dwc->owner_cpu != smp_processor_id()) {	\
				spin_unlock_irqrestore(&_dwc->lock, (flg)); \
				goto __dwc2_lock_barrior;		\
			}						\
		}							\
	} while (0)

#define dwc2_spin_unlock_irqrestore(__dwc, flg)			\
	do {							\
		struct dwc2 *_dwc = (__dwc);			\
		spin_unlock_irqrestore(&_dwc->lock, (flg));	\
	} while (0)

#define dwc2_spin_lock(__dwc)						\
	do {								\
		struct dwc2 *_dwc = (__dwc);				\
		if (unlikely(!irqs_disabled()))				\
			panic("dwc2_spin_lock must called from interrupt handler!\n"); \
		spin_lock(&_dwc->lock);					\
	} while(0)

#define dwc2_spin_unlock(__dwc)						\
	do {								\
		struct dwc2 *_dwc = (__dwc);				\
		if (unlikely(!irqs_disabled()))				\
			panic("dwc2_spin_unlock must called from interrupt handler!\n"); \
		spin_unlock(&_dwc->lock);				\
	} while(0)

/* prototypes */

int dwc2_core_init(struct dwc2 *dwc);

int dwc2_host_init(struct dwc2 *dwc);
void dwc2_host_exit(struct dwc2 *dwc);

int dwc2_gadget_init(struct dwc2 *dwc);
void dwc2_gadget_exit(struct dwc2 *dwc);

void dwc2_enable_common_interrupts(struct dwc2 *dwc);
uint8_t dwc2_is_device_mode(struct dwc2 *dwc);
uint8_t dwc2_is_host_mode(struct dwc2 *dwc);

void dwc2_wait_3_phy_clocks(void);
void dwc2_core_reset(struct dwc2 *dwc);

#endif /* __DRIVERS_USB_DWC2_CORE_H */
