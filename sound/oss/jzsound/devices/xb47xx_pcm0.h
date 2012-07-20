
#ifndef __XB_SND_PCM0_H__
#define __XB_SND_PCM0_H__

#include <asm/io.h>
#include "../interface/xb_snd_dsp.h"
/**
 * global variable
 **/
extern void volatile __iomem *volatile pcm0_iomem;


#define NEED_RECONF_DMA         0x00000001
#define NEED_RECONF_TRIGGER     0x00000002
#define NEED_RECONF_FILTER      0x00000004

/**
 * registers
 **/

#define PCMCTL0		0x00
#define PCMCFG0		0x04
#define PCMDP0		0x08
#define PCMINTC0	0x0C
#define PCMINTS0	0x10
#define PCMDIV0		0x14

/**
 * i2s register control
 **/
static unsigned long read_val;
static unsigned long tmp_val;
#define pcm0_write_reg(addr,val)        \
	writel(val,pcm0_iomem+addr)

#define pcm0_read_reg(addr)             \
	readl(pcm0_iomem+addr)

#define pcm0_set_reg(addr,val,mask,offset)\
	do {										\
		tmp_val = val;							\
		read_val = pcm0_read_reg(addr);         \
		read_val &= (~mask);                    \
		tmp_val = ((tmp_val << offset) & mask); \
		tmp_val |= read_val;                    \
		pcm0_write_reg(addr,tmp_val);           \
	}while(0)

#define pcm0_get_reg(addr,mask,offset)  \
	((pcm0_read_reg(addr) & mask) >> offset)

#define pcm0_clear_reg(addr,mask)       \
	pcm0_write_reg(addr,~mask)

/*PCMCTL0*/
#define PCM0_ERDMA_OFFSET       (9)
#define PCM0_ERDMA_MASK         (0x1 << PCM0_ERDMA_OFFSET)
#define PCM0_ETDMA_OFFSET       (8)
#define PCM0_ETDMA_MASK         (0x1 << PCM0_ETDMA_OFFSET)
#define PCM0_LSMP_OFFSET		(7)
#define PCM0_LSMP_MASK			(0x1 << PCM0_LSMP_OFFSET)
#define PCM0_ERPL_OFFSET		(6)
#define PCM0_ERPL_MASK			(0x1 << PCM0_ERPL_OFFSET)
#define PCM0_EREC_OFFSET        (5)
#define PCM0_EREC_MASK          (0x1 << PCM0_EREC_OFFSET)
#define PCM0_FLUSH_OFFSET       (4)
#define PCM0_FLUSH_MASK         (0x1 << PCM0_FLUSH_OFFSET)
#define PCM0_RST_OFFSET			(3)
#define PCM0_RST_MASK			(0x1 << PCM0_RST_OFFSET)
#define PCM0_CLKEN_OFFSET       (1)
#define PCM0_CLKEN_MASK         (0x1 << PCM0_CLKEN_OFFSET)
#define PCM0_PCMEN_OFFSET       (0)
#define PCM0_PCMEN_MASK         (0x1 << PCM0_PCMEN_OFFSET)

#define __pcm0_enable_transmit_dma()    \
	pcm0_set_reg(PCMCTL0,1,PCM0_ETDMA_MASK,PCM0_ETDMA_OFFSET)
#define __pcm0_disable_transmit_dma()   \
	pcm0_set_reg(PCMCTL0,0,PCM0_ETDMA_MASK,PCM0_ETDMA_OFFSET)
#define __pcm0_enable_receive_dma()     \
	pcm0_set_reg(PCMCTL0,1,PCM0_ERDMA_MASK,PCM0_ERDMA_OFFSET)
#define __pcm0_disable_receive_dma()    \
	pcm0_set_reg(PCMCTL0,0,PCM0_ERDMA_MASK,PCM0_ERDMA_OFFSET)

#define __pcm0_play_zero()              \
	pcm0_set_reg(PCMCTL0,0,PCM0_LSMP_MASK,PCM0_LSMP_OFFSET)
#define __pcm0_play_lastsample()        \
	pcm0_set_reg(PCMCTL0,1,PCM0_LSMP_MASK,PCM0_LSMP_OFFSET)

#define __pcm0_enable_replay()          \
	pcm0_set_reg(PCMCTL0,1,PCM0_ERPL_MASK,PCM0_ERPL_OFFSET)
#define __pcm0_disable_replay()         \
	pcm0_set_reg(PCMCTL0,0,PCM0_ERPL_MASK,PCM0_ERPL_OFFSET)
#define __pcm0_enable_record()          \
	pcm0_set_reg(PCMCTL0,1,PCM0_EREC_MASK,PCM0_EREC_OFFSET)
#define __pcm0_disable_record()         \
	pcm0_set_reg(PCMCTL0,0,PCM0_EREC_MASK,PCM0_EREC_OFFSET)

#define __pcm0_flush_fifo()            \
	pcm0_set_reg(PCMCTL0,1,PCM0_FLUSH_MASK,PCM0_FLUSH_OFFSET)

#define __pcm0_reset()                  \
	pcm0_set_reg(PCMCTL0,1,PCM0_RST_MASK,PCM0_RST_OFFSET)

#define __pcm0_clock_enable()			\
	pcm0_set_reg(PCMCTL0,1,PCM0_CLKEN_MASK,PCM0_CLKEN_OFFSET)
#define __pcm0_clock_disable()			\
	pcm0_set_reg(PCMCTL0,0,PCM0_CLKEN_MASK,PCM0_CLKEN_OFFSET)

#define __pcm0_enable()                 \
	pcm0_set_reg(PCMCTL0,1,PCM0_PCMEN_MASK,PCM0_PCMEN_OFFSET)
#define __pcm0_disable()                \
	pcm0_set_reg(PCMCTL0,0,PCM0_PCMEN_MASK,PCM0_PCMEN_OFFSET)

/*PCMCFG0*/
#define PCM0_SLOT_OFFSET        (13)
#define PCM0_SLOT_MASK          (0x3 << PCM0_SLOT_OFFSET)
#define PCM0_ISS_OFFSET			(12)
#define PCM0_ISS_MASK			(0x1 << PCM0_ISS_OFFSET)
#define PCM0_OSS_OFFSET			(11)
#define PCM0_OSS_MASK			(0x1 << PCM0_OSS_OFFSET)
#define PCM0_IMSBPOS_OFFSET     (10)
#define PCM0_IMSBPOS_MASK       (0x1 << PCM0_IMSBPOS_OFFSET)
#define PCM0_OMSBPOS_OFFSET     (9)
#define PCM0_OMSBPOS_MASK       (0x1 << PCM0_OMSBPOS_OFFSET)
#define PCM0_RFTH_OFFSET        (5)
#define PCM0_RFTH_MASK          (0xf << PCM0_RFTH_OFFSET)
#define PCM0_TFTH_OFFSET        (1)
#define PCM0_TFTH_MASK          (0xf << PCM0_TFTH_OFFSET)
#define PCM0_PCMMOD_OFFSET      (0)
#define PCM0_PCMMOD_MASK        (0x1 << PCM0_PCMMOD_OFFSET)

#define __pcm0_set_slot(n)	\
	pcm0_set_reg(PCMCFG0,n,PCM0_SLOT_MASK,PCM0_SLOT_OFFSET)

#define __pcm0_set_oss_sample_size(n)   \
	pcm0_set_reg(PCMCFG0,n,PCM0_OSS_MASK,PCM0_OSS_OFFSET)
#define __pcm0_set_iss_sample_size(n)   \
	pcm0_set_reg(PCMCFG0,n,PCM0_ISS_MASK,PCM0_ISS_OFFSET)

#define __pcm0_set_msb_normal_in()   \
	pcm0_set_reg(PCMCFG0,0,PCM0_IMSBPOS_MASK,PCM0_IMSBPOS_OFFSET)
#define __pcm0_set_msb_one_shift_in()   \
	pcm0_set_reg(PCMCFG0,1,PCM0_IMSBPOS_MASK,PCM0_IMSBPOS_OFFSET)

#define __pcm0_set_msb_normal_out()   \
	pcm0_set_reg(PCMCFG0,0,PCM0_OMSBPOS_MASK,PCM0_OMSBPOS_OFFSET)
#define __pcm0_set_msb_one_shift_out()   \
	pcm0_set_reg(PCMCFG0,1,PCM0_OMSBPOS_MASK,PCM0_OMSBPOS_OFFSET)

#define __pcm0_set_transmit_trigger(n)  \
	pcm0_set_reg(PCMCFG0,n,PCM0_TFTH_MASK,PCM0_TFTH_OFFSET)
#define __pcm0_set_receive_trigger(n)   \
	pcm0_set_reg(PCMCFG0,n,PCM0_RFTH_MASK,PCM0_RFTH_OFFSET)

#define __pcm0_as_master()             \
	pcm0_set_reg(PCMCFG0,0,PCM0_PCMMOD_MASK,PCM0_PCMMOD_OFFSET)
#define __pcm0_as_slaver()             \
	pcm0_set_reg(PCMCFG0,1,PCM0_PCMMOD_MASK,PCM0_PCMMOD_OFFSET)

/*PCMDP0*/
#define PCM0_PCMDP_OFFSET		(0)
#define PCM0_PCMDP_MASK			(~0)

#define __pcm0_read_fifo()		\
	pcm0_get_reg(PCMDP0,PCM0_PCMDP_MASK,PCM0_PCMDP_OFFSET);

/*PCMINTC0*/
#define PCM0_ETFS_OFFSET        (3)
#define PCM0_ETFS_MASK          (0X1 << PCM0_ETFS_OFFSET)
#define PCM0_ETUR_OFFSET        (2)
#define PCM0_ETUR_MASK          (0x1 << PCM0_ETUR_OFFSET)
#define PCM0_ERFS_OFFSET        (1)
#define PCM0_ERFS_MASK          (0x1 << PCM0_ERFS_OFFSET)
#define PCM0_EROR_OFFSET        (0)
#define PCM0_EROR_MASK          (0x1 << PCM0_EROR_OFFSET)

#define __pcm0_enable_receive_intr()    \
	pcm0_set_reg(PCMINTC0,1,PCM0_ERFS_MASK,PCM0_ERFS_OFFSET)
#define __pcm0_disable_receive_intr()   \
	pcm0_set_reg(PCMINTC0,0,PCM0_ERFS_MASK,PCM0_ERFS_OFFSET)

#define __pcm0_enable_underrun_intr()   \
	pcm0_set_reg(PCMINTC0,1,PCM0_ETUR_MASK,PCM0_ETUR_OFFSET)
#define __pcm0_disable_underrun_intr()  \
	pcm0_set_reg(PCMINTC0,0,PCM0_ETUR_MASK,PCM0_ETUR_OFFSET)

#define __pcm0_enable_transmit_intr()   \
	pcm0_set_reg(PCMINTC0,1,PCM0_ETFS_MASK,PCM0_ETFS_OFFSET)
#define __pcm0_disable_transmit_intr()  \
	pcm0_set_reg(PCMINTC0,0,PCM0_ETFS_MASK,PCM0_ETFS_OFFSET)

#define __pcm0_enable_overrun_intr()    \
	pcm0_set_reg(PCMINTC0,1,PCM0_EROR_MASK,PCM0_EROR_OFFSET)
#define __pcm0_disable_overrun_intr()   \
	pcm0_set_reg(PCMINTC0,0,PCM0_EROR_MASK,PCM0_EROR_OFFSET)

/*PCMINTS0*/
#define PCM0_RSTS_OFFSET        (14)
#define PCM0_RSTS_MASK          (0x1 << PCM0_RSTS_OFFSET)
#define PCM0_TFL_OFFSET         (9)
#define PCM0_TFL_MASK           (0x1f << PCM0_TFL_OFFSET)
#define PCM0_TFS_OFFSET         (8)
#define PCM0_TFS_MASK           (0x1 << PCM0_TFS_OFFSET)
#define PCM0_TUR_OFFSET         (7)
#define PCM0_TUR_MASK           (0x1 << PCM0_TUR_OFFSET)
#define PCM0_RFL_OFFSET         (2)
#define PCM0_RFL_MASK           (0x1f << PCM0_RFL_OFFSET)
#define PCM0_RFS_OFFSET         (1)
#define PCM0_RFS_MASK           (0x1 << PCM0_RFS_OFFSET)
#define PCM0_ROR_OFFSET         (0)
#define PCM0_ROR_MASK           (0X1 << PCM0_ROR_OFFSET)

#define __pcm0_test_rst_complie()	\
	!pcm0_get_reg(PCMINTS0,PCM0_RSTS_MASK,PCM0_RSTS_OFFSET)
#define __pcm0_test_tfl()               \
	pcm0_get_reg(PCMINTS0,PCM0_TFL_MASK,PCM0_TFL_OFFSET)
#define __pcm0_test_tfs()               \
	pcm0_get_reg(PCMINTS0,PCM0_TFS_MASK,PCM0_TFS_OFFSET)
#define __pcm0_test_tur()               \
	pcm0_get_reg(PCMINTS0,PCM0_TUR_MASK,PCM0_TUR_OFFSET)
#define __pcm0_clear_tur()	\
	pcm0_set_reg(PCMINTS0,0,PCM0_TUR_MASK,PCM0_TUR_OFFSET)
#define __pcm0_test_rfl()               \
	pcm0_get_reg(PCMINTS0,PCM0_RFL_MASK,PCM0_RFL_OFFSET)
#define __pcm0_test_rfs()               \
	pcm0_get_reg(PCMINTS0,PCM0_RFS_MASK,PCM0_RFS_OFFSET)
#define __pcm0_clear_ror()	\
	pcm0_set_reg(PCMINTS0,0,PCM0_ROR_MASK,PCM0_ROR_OFFSET)
#define __pcm0_test_ror()               \
	pcm0_get_reg(PCMINTS0,PCM0_ROR_MASK,PCM0_ROR_OFFSET)
/* PCMDIV0 */
#define PCM0_SYNC_OFFSET		(11)
#define PCM0_SYNC_MASK			(0x3f << PCM0_SYNC_OFFSET)
#define PCM0_SYNDIV_OFFSET		(6)
#define PCM0_SYNDIV_MASK		(0x1f << PCM0_SYNDIV_OFFSET)
#define PCM0_CLKDIV_OFFSET		(0)
#define PCM0_CLKDIV_MASK		(0x3f << PCM0_CLKDIV_OFFSET)

#define __pcm0_set_sync(n)	\
	pcm0_set_reg(PCMDIV0,n,PCM0_SYNC_MASK,PCM0_SYNC_OFFSET)
#define __pcm0_set_syndiv(n)	\
	pcm0_set_reg(PCMDIV0,n,PCM0_SYNDIV_MASK,PCM0_SYNDIV_OFFSET)
#define __pcm0_set_clkdiv(n)	\
pcm0_set_reg(PCMDIV0,n,PCM0_CLKDIV_MASK,PCM0_CLKDIV_OFFSET)


#define CODEC_RMODE                     0x1
#define CODEC_WMODE                     0x2
#define CODEC_RWMODE                    0x3

#endif /* _XB_SND_I2S_H_ */
