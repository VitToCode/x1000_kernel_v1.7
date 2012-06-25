
#ifndef __XB_SND_I2S0_H__
#define __XB_SND_I2S0_H__

#include <asm/io.h>


extern unsigned int DEFAULT_REPLAY_ROUTE;
extern unsigned int DEFAULT_RECORD_ROUTE;

/**
 * global variable
 **/
static void __iomem *i2s0_iomem;


#define NEED_RECONF_DMA         0x00000001
#define NEED_RECONF_TRIGGER     0x00000002
#define NEED_RECONF_FILTER      0x00000004

/**
 * registers
 **/

#define AIC0FR 0x0
#define AIC0CR 0x4
#define AICCR1 0x8
#define AICCR2 0xc
#define I2S0CR 0x10
#define AIC0SR 0x14
#define A0CSR  0x18
#define I2S0SR 0x1c
#define A0CCAR 0x20
#define A0CCDR 0x24
#define A0CSAR 0x28
#define A0CSDR 0x2c
#define I2S0DIV 0x30
#define AIC0DR 0x34
#define CKCFG  0xa0
#define RGADW  0xa4
#define RGDATA 0xa8


/**
 * i2s register control
 **/
static unsigned long read_val;
static unsigned long tmp_val;
#define i2s0_write_reg(addr,val)        \
	writel(val,i2s0_iomem+addr)

#define i2s0_read_reg(addr)             \
	readl(i2s0_iomem+addr)

#define i2s0_set_reg(addr,val,mask,offset)\
	do {										\
		tmp_val = val;							\
		read_val = i2s0_read_reg(addr);         \
		read_val &= (~mask);                    \
		tmp_val = ((tmp_val << offset) & mask); \
		tmp_val |= read_val;                    \
		i2s0_write_reg(addr,tmp_val);           \
	}while(0)

#define i2s0_get_reg(addr,mask,offset)  \
	((i2s0_read_reg(addr) & mask) >> offset)

#define i2s0_clear_reg(addr,mask)       \
	i2s0_write_reg(addr,~mask)

/* AICFR */
#define I2S0_ENB_OFFSET         (0)
#define I2S0_ENB_MASK           (0x1 << I2S0_ENB_OFFSET)
#define I2S0_SYNCD_OFFSET       (1)
#define I2S0_SYNCD_MASK         (0x1 << I2S0_SYNCD_OFFSET)
#define I2S0_BCKD_OFFSET        (2)
#define I2S0_BCKD_MASK          (0x1 << I2S0_BCKD_OFFSET)
#define I2S0_RST_OFFSET         (3)
#define I2S0_RST_MASK           (0X1 << I2S0_RST_OFFSET)
#define I2S0_AUSEL_OFFSET       (4)
#define I2S0_AUSEL_MASK         (0x1 << I2S0_AUSEL_OFFSET)
#define I2S0_ICDC_OFFSET        (5)
#define I2S0_ICDC_MASK          (0x1 << I2S0_ICDC_OFFSET)
#define I2S0_LSMP_OFFSET        (6)
#define I2S0_LSMP_MASK          (0x1 << I2S0_LSMP_OFFSET)
#define I2S0_ICS_OFFSET         (7)
#define I2S0_ICS_MASK           (0x1 << I2S0_ICS_OFFSET)
#define I2S0_DMODE_OFFSET       (8)
#define I2S0_DMODE_MASK         (0x1 << I2S0_DMODE_OFFSET)
#define I2S0_ISYNCD_OFFSET      (9)
#define I2S0_ISYNCD_MASK        (0x1 << I2S0_ISYNCD_OFFSET)
#define I2S0_IBCKD_OFFSET       (10)
#define I2S0_IBCKD_MASK         (0x1 << I2S0_IBCKD_OFFSET)
#define I2S0_TFTH_OFFSET        (16)
#define I2S0_TFTH_MASK          (0x1f << I2S0_TFTH_OFFSET)
#define I2S0_RFTH_OFFSET        (24)
#define I2S0_RFTH_MASK          (0xf << I2S0_RFTH_OFFSET)

#define __aic0_select_i2s()             \
	i2s0_set_reg(AIC0FR,1,I2S0_AUSEL_MASK,I2S0_AUSEL_OFFSET)
#define __aic0_select_aclink()          \
	i2s0_set_reg(AIC0FR,0,I2S0_AUSEL_MASK,I2S0_AUSEL_OFFSET)
#define __i2s0_set_transmit_trigger(n)  \
	i2s0_set_reg(AIC0FR,n,I2S0_TFTH_MASK,I2S0_TFTH_OFFSET)
#define __i2s0_set_receive_trigger(n)   \
	i2s0_set_reg(AIC0FR,n,I2S0_RFTH_MASK,I2S0_RFTH_OFFSET)
#define __i2s0_as_master()              \
	i2s0_set_reg(AIC0FR,1,I2S0_ICS_MASK,I2S0_ICS_OFFSET)
#define __i2s0_as_slave()               \
	i2s0_set_reg(AIC0FR,0,I2S0_ICS_MASK,I2S0_ICS_OFFSET)
#define __i2s0_internal_codec()         \
	i2s0_set_reg(AIC0FR,1,I2S0_ICDC_MASK,I2S0_ICDC_OFFSET)
#define __i2s0_external_codec()         \
	i2s0_set_reg(AIC0FR,0,I2S0_ICDC_MASK,I2S0_ICDC_OFFSET)
#define __i2s0_bclk_input()             \
	i2s0_set_reg(AIC0FR,0,I2S0_BCKD_MASK,I2S0_BCKD_OFFSET)
#define __i2s0_bclk_output()            \
	i2s0_set_reg(AIC0FR,1,I2S0_BCKD_MASK,I2S0_BCKD_OFFSET)
#define __i2s0_sync_input()             \
	i2s0_set_reg(AIC0FR,0,I2S0_SYNCD_MASK,I2S0_SYNCD_OFFSET)
#define __i2s0_sync_output()            \
	i2s0_set_reg(AIC0FR,1,I2S0_SYNCD_MASK,I2S0_SYNCD_OFFSET)
#define __i2s0_ibclk_input()            \
	i2s0_set_reg(AIC0FR,0,I2S0_IBCKD_MASK,I2S0_IBCKD_OFFSET)
#define __i2s0_ibclk_output()           \
	i2s0_set_reg(AIC0FR,1,I2S0_IBCKD_MASK,I2S0_IBCKD_OFFSET)
#define __i2s0_isync_input()            \
	i2s0_set_reg(AIC0FR,0,I2S0_ISYNCD_MASK,I2S0_ISYNCD_OFFSET)
#define __i2s0_isync_output()           \
	i2s0_set_reg(AIC0FR,0,I2S0_ISYNCD_MASK,I2S0_ISYNCD_OFFSET)

#define __i2s0_internal_clkset()        \
	do {                                            \
		__i2s0_bclk_input();                    \
		__i2s0_sync_input();                    \
		__i2s0_isync_input();                   \
		__i2s0_ibclk_input();                   \
	}while(0)

#define __i2s0_play_zero()              \
	i2s0_set_reg(AIC0FR,0,I2S0_LSMP_MASK,I2S0_LSMP_OFFSET)
#define __i2s0_play_lastsample()        \
	i2s0_set_reg(AIC0FR,1,I2S0_LSMP_MASK,I2S0_LSMP_OFFSET)

#define __i2s0_reset()                  \
	i2s0_set_reg(AIC0FR,1,I2S0_RST_MASK,I2S0_RST_OFFSET)

#define __i2s0_enable()                 \
	i2s0_set_reg(AIC0FR,1,I2S0_ENB_MASK,I2S0_ENB_OFFSET)
#define __i2s0_disable()                \
	i2s0_set_reg(AIC0FR,0,I2S0_ENB_MASK,I2S0_ENB_OFFSET)

/* AICCR */
#define I2S0_EREC_OFFSET        (0)
#define I2S0_EREC_MASK          (0x1 << I2S0_EREC_OFFSET)
#define I2S0_ERPL_OFFSET        (1)
#define I2S0_ERPL_MASK          (0x1 << I2S0_ERPL_OFFSET)
#define I2S0_ENLBF_OFFSET       (2)
#define I2S0_ENLBF_MASK         (0x1 << I2S0_ENLBF_OFFSET)
#define I2S0_ETFS_OFFSET        (3)
#define I2S0_ETFS_MASK          (0X1 << I2S0_ETFS_OFFSET)
#define I2S0_ERFS_OFFSET        (4)
#define I2S0_ERFS_MASK          (0x1 << I2S0_ERFS_OFFSET)
#define I2S0_ETUR_OFFSET        (5)
#define I2S0_ETUR_MASK          (0x1 << I2S0_ETUR_OFFSET)
#define I2S0_EROR_OFFSET        (6)
#define I2S0_EROR_MASK          (0x1 << I2S0_EROR_OFFSET)
#define I2S0_RFLUSH_OFFSET      (7)
#define I2S0_RFLUSH_MASK        (0x1 << I2S0_RFLUSH_OFFSET)
#define I2S0_TFLUSH_OFFSET      (8)
#define I2S0_TFLUSH_MASK        (0x1 << I2S0_TFLUSH_OFFSET)
#define I2S0_ASVTSU_OFFSET      (9)
#define I2S0_ASVTSU_MASK        (0x1 << I2S0_ASVTSU_OFFSET)
#define I2S0_ENDSW_OFFSET       (10)
#define I2S0_ENDSW_MASK         (0x1 << I2S0_ENDSW_OFFSET)
#define I2S0_M2S_OFFSET         (11)
#define I2S0_M2S_MASK           (0x1 << I2S0_M2S_OFFSET)
#define I2S0_TDMS_OFFSET        (14)
#define I2S0_TDMS_MASK          (0x1 << I2S0_TDMS_OFFSET)
#define I2S0_RDMS_OFFSET        (15)
#define I2S0_RDMS_MASK          (0x1 << I2S0_RDMS_OFFSET)
#define I2S0_ISS_OFFSET         (16)
#define I2S0_ISS_MASK           (0x7 << I2S0_ISS_OFFSET)
#define I2S0_OSS_OFFSET         (19)
#define I2S0_OSS_MASK           (0x7 << I2S0_OSS_OFFSET)
#define I2S0_CHANNEL_OFFSET     (24)
#define I2S0_CHANNEL_MASK       (0x7 << I2S0_CHANNEL_OFFSET)
#define I2S0_PACK16_OFFSET      (28)
#define I2S0_PACK16_MASK        (0x1 << I2S0_PACK16_OFFSET)


#define __i2s0_enable_pack16()          \
	i2s0_set_reg(AIC0CR,1,I2S0_PACK16_MASK,I2S0_PACK16_OFFSET)
#define __i2s0_disable_pack16()         \
	i2s0_set_reg(AIC0CR,0,I2S0_PACK16_MASK,I2S0_PACK16_OFFSET)
#define __i2s0_out_channel_select(n)    \
	i2s0_set_reg(AIC0CR,n,I2S0_CHANNEL_MASK,I2S0_CHANNEL_OFFSET)
#define __i2s0_set_oss_sample_size(n)   \
	i2s0_set_reg(AIC0CR,n,I2S0_OSS_MASK,I2S0_OSS_OFFSET)
#define __i2s0_set_iss_sample_size(n)   \
	i2s0_set_reg(AIC0CR,n,I2S0_ISS_MASK,I2S0_ISS_OFFSET)

#define __i2s0_enable_transmit_dma()    \
	i2s0_set_reg(AIC0CR,1,I2S0_TDMS_MASK,I2S0_TDMS_OFFSET)
#define __i2s0_disable_transmit_dma()   \
	i2s0_set_reg(AIC0CR,0,I2S0_TDMS_MASK,I2S0_TDMS_OFFSET)
#define __i2s0_enable_receive_dma()     \
	i2s0_set_reg(AIC0CR,1,I2S0_RDMS_MASK,I2S0_RDMS_OFFSET)
#define __i2s0_disable_receive_dma()    \
	i2s0_set_reg(AIC0CR,0,I2S0_RDMS_MASK,I2S0_RDMS_OFFSET)

#define __i2s0_enable_mono2stereo()     \
	i2s0_set_reg(AIC0CR,1,I2S0_M2S_MASK,I2S0_M2S_OFFSET)
#define __i2s0_disable_mono2stereo()    \
	i2s0_set_reg(AIC0CR,0,I2S0_M2S_MASK,I2S0_M2S_OFFSET)

#define __i2s0_enable_byteswap()        \
	i2s0_set_reg(AIC0CR,1,I2S0_ENDSW_MASK,I2S0_ENDSW_OFFSET)
#define __i2s0_disable_byteswap()       \
	i2s0_set_reg(AIC0CR,0,I2S0_ENDSW_MASK,I2S0_ENDSW_OFFSET)

#define __i2s0_enable_signadj()       \
	i2s0_set_reg(AIC0CR,1,I2S0_ASVTSU_MASK,I2S0_ASVTSU_OFFSET)
#define __i2s0_disable_signadj()      \
	i2s0_set_reg(AIC0CR,0,I2S0_ASVTSU_MASK,I2S0_ASVTSU_OFFSET)

#define __i2s0_flush_tfifo()            \
	i2s0_set_reg(AIC0CR,1,I2S0_TFLUSH_MASK,I2S0_TFLUSH_OFFSET)
#define __i2s0_flush_rfifo()            \
	i2s0_set_reg(AIC0CR,1,I2S0_RFLUSH_MASK,I2S0_RFLUSH_OFFSET)

#define __i2s0_enable_overrun_intr()    \
	i2s0_set_reg(AIC0CR,1,I2S0_EROR_MASK,I2S0_EROR_OFFSET)
#define __i2s0_disable_overrun_intr()   \
	i2s0_set_reg(AIC0CR,0,I2S0_EROR_MASK,I2S0_EROR_OFFSET)

#define __i2s0_enable_underrun_intr()   \
	i2s0_set_reg(AIC0CR,1,I2S0_ETUR_MASK,I2S0_ETUR_OFFSET)
#define __i2s0_disable_underrun_intr()  \
	i2s0_set_reg(AIC0CR,0,I2S0_ETUR_MASK,I2S0_ETUR_OFFSET)

#define __i2s0_enable_transmit_intr()   \
	i2s0_set_reg(AIC0CR,1,I2S0_ETFS_MASK,I2S0_ETFS_OFFSET)
#define __i2s0_disable_transmit_intr()  \
	i2s0_set_reg(AIC0CR,0,I2S0_ETFS_MASK,I2S0_ETFS_OFFSET)

#define __i2s0_enable_receive_intr()    \
	i2s0_set_reg(AIC0CR,1,I2S0_ERFS_MASK,I2S0_ERFS_OFFSET)
#define __i2s0_disable_receive_intr()   \
	i2s0_set_reg(AIC0CR,0,I2S0_ERFS_MASK,I2S0_ERFS_OFFSET)

#define __i2s0_enable_loopback()        \
	i2s0_set_reg(AIC0CR,1,I2S0_ENLBF_MASK,I2S0_ENLBF_OFFSET)
#define __i2s0_disable_loopback()       \
	i2s0_set_reg(AIC0CR,0,I2S0_ENLBF_MASK,I2S0_ENLBF_OFFSET)

#define __i2s0_enable_replay()          \
	i2s0_set_reg(AIC0CR,1,I2S0_ERPL_MASK,I2S0_ERPL_OFFSET)
#define __i2s0_disable_replay()         \
	i2s0_set_reg(AIC0CR,0,I2S0_ERPL_MASK,I2S0_ERPL_OFFSET)

#define __i2s0_enable_record()          \
	i2s0_set_reg(AIC0CR,1,I2S0_EREC_MASK,I2S0_EREC_OFFSET)
#define __i2s0_disable_record()         \
	i2s0_set_reg(AIC0CR,0,I2S0_EREC_MASK,I2S0_EREC_OFFSET)

/* I2SCR */
#define I2S0_AMSL_OFFSET        (0)
#define I2S0_AMSL_MASK          (0x1 << I2S0_AMSL_OFFSET)
#define I2S0_ESCLK_OFFSET       (4)
#define I2S0_ESCLK_MASK         (0x1 << I2S0_ESCLK_OFFSET)
#define I2S0_STPBK_OFFSET       (12)
#define I2S0_STPBK_MASK         (0x1 << I2S0_STPBK_OFFSET)
#define I2S0_ISTPBK_OFFSET      (13)
#define I2S0_ISTPBK_MASK        (0X1 << I2S0_ISTPBK_OFFSET)
#define I2S0_SWLH_OFFSET        (16)
#define I2S0_SWLH_MASK          (0x1 << I2S0_SWLH_OFFSET)
#define I2S0_RFIRST_OFFSET      (17)
#define I2S0_RFIRST_MASK        (0x1 << I2S0_ETUR_OFFSET)

#define __i2s0_send_rfirst()            \
	i2s0_set_reg(I2S0CR,1,I2S0_RFIRST_MASK,I2S0_RFIRST_OFFSET)
#define __i2s0_send_lfirst()            \
	i2s0_set_reg(I2S0CR,0,I2S0_RFIRST_MASK,I2S0_RFIRST_OFFSET)

#define __i2s0_switch_lr()              \
	i2s0_set_reg(I2S0CR,1,I2S0_SWLH_MASK,I2S0_SWLH_OFFSET)
#define __i2s0_unswitch_lr()            \
	i2s0_set_reg(I2S0CR,0,I2S0_SWLH_MASK,I2S0_SWLH_OFFSET)

#define __i2s0_stop_bitclk()            \
	i2s0_set_reg(I2S0CR,1,I2S0_STPBK_MASK,I2S0_STPBK_OFFSET)
#define __i2s0_start_bitclk()           \
	i2s0_set_reg(I2S0CR,0,I2S0_STPBK_MASK,I2S0_STPBK_OFFSET)

#define __i2s0_stop_ibitclk()           \
	i2s0_set_reg(I2S0CR,1,I2S0_ISTPBK_MASK,I2S0_ISTPBK_OFFSET)
#define __i2s0_start_ibitclk()          \
	i2s0_set_reg(I2S0CR,0,I2S0_ISTPBK_MASK,I2S0_ISTPBK_OFFSET)

#define __i2s0_enable_sysclk_output()   \
	i2s0_set_reg(I2S0CR,1,I2S0_ESCLK_MASK,I2S0_ESCLK_OFFSET)
#define __i2s0_disable_sysclk_output()  \
	i2s0_set_reg(I2S0CR,0,I2S0_ESCLK_MASK,I2S0_ESCLK_OFFSET)
#define __i2s0_select_i2s()             \
	i2s0_set_reg(I2S0CR,0,I2S0_AMSL_MASK,I2S0_AMSL_OFFSET)
#define __i2s0_select_msbjustified()    \
	i2s0_set_reg(I2S0CR,1,I2S0_AMSL_MASK,I2S0_AMSL_OFFSET)


/* AICSR*/
#define I2S0_TFS_OFFSET         (3)
#define I2S0_TFS_MASK           (0x1 << I2S0_TFS_OFFSET)
#define I2S0_RFS_OFFSET         (4)
#define I2S0_RFS_MASK           (0x1 << I2S0_RFS_OFFSET)
#define I2S0_TUR_OFFSET         (5)
#define I2S0_TUR_MASK           (0x1 << I2S0_TUR_OFFSET)
#define I2S0_ROR_OFFSET         (6)
#define I2S0_ROR_MASK           (0X1 << I2S0_ROR_OFFSET)
#define I2S0_TFL_OFFSET         (8)
#define I2S0_TFL_MASK           (0x3f << I2S0_TFL_OFFSET)
#define I2S0_RFL_OFFSET         (24)
#define I2S0_RFL_MASK           (0x3f << I2S0_RFL_OFFSET)

#define __i2s0_clear_tur()	\
	i2s0_set_reg(AIC0SR,0,I2S0_TUR_MASK,I2S0_TUR_OFFSET)
#define __i2s0_test_tur()               \
	i2s0_get_reg(AIC0SR,I2S0_TUR_MASK,I2S0_TUR_OFFSET)
#define __i2s0_clear_ror()	\
	i2s0_set_reg(AIC0SR,0,I2S0_ROR_MASK,I2S0_ROR_OFFSET)
#define __i2s0_test_ror()               \
	i2s0_get_reg(AIC0SR,I2S0_ROR_MASK,I2S0_ROR_OFFSET)
#define __i2s0_test_tfs()               \
	i2s0_get_reg(AIC0SR,I2S0_TFS_MASK,I2S0_TFS_OFFSET)
#define __i2s0_test_rfs()               \
	i2s0_get_reg(AIC0SR,I2S0_RFS_MASK,I2S0_RFS_OFFSET)
#define __i2s0_test_tfl()               \
	i2s0_get_reg(AIC0SR,I2S0_TFL_MASK,I2S0_TFL_OFFSET)
#define __i2s0_test_rfl()               \
	i2s0_get_reg(AIC0SR,I2S0_RFL_MASK,I2S0_RFL_OFFSET)
/* I2SSR */
#define I2S0_BSY_OFFSET         (2)
#define I2S0_BSY_MASK           (0x1 << I2S0_BSY_OFFSET)
#define I2S0_RBSY_OFFSET        (3)
#define I2S0_RBSY_MASK          (0x1 << I2S0_RBSY_OFFSET)
#define I2S0_TBSY_OFFSET        (4)
#define I2S0_TBSY_MASK          (0x1 << I2S0_TBSY_OFFSET)
#define I2S0_CHBSY_OFFSET       (5)
#define I2S0_CHBSY_MASK         (0X1 << I2S0_CHBSY_OFFSET)

#define __i2s0_is_busy()                \
	i2s0_get_reg(I2S0SR,I2S0_BSY_MASK,I2S0_BSY_OFFSET)
#define __i2s0_rx_is_busy()             \
	i2s0_get_reg(I2S0SR,I2S0_RBSY_MASK,I2S0_RBSY_OFFSET)
#define __i2s0_tx_is_busy()             \
	i2s0_get_reg(I2S0SR,I2S0_TBSY_MASK,I2S0_TBSY_OFFSET)
#define __i2s0_channel_is_busy()        \
	i2s0_get_reg(I2S0SR,I2S0_CHBSY_MASK,I2S0_CHBSY_OFFSET)
/* AICDR */
#define I2S0_DATA_OFFSET        (0)
#define I2S0_DATA_MASK          (0xffffff << I2S0_DATA_OFFSET)

#define __i2s0_write_tfifo(v)           \
	i2s0_set_reg(AIC0DR,v,I2S0_DATA_MASK,I2S0_DATA_OFFSET)
#define __i2s0_read_rfifo()             \
	i2s0_get_reg(AIC0DR,I2S0_DATA_MASK,I2S0_DATA_OFFSET)

/* I2SDIV */
#define I2S0_DV_OFFSET          (0)
#define I2S0_DV_MASK            (0xf << I2S0_DV_OFFSET)
#define I2S0_IDV_OFFSET         (8)
#define I2S0_IDV_MASK           (0xf << I2S0_IDV_OFFSET)

static inline int  __i2s0_set_sample_rate(unsigned long i2sclk, unsigned long sync)
{
	int dlv = i2sclk/(64*sync);
	unsigned long val = 0;
	switch (dlv) {
		case 2: val = 0x1;
			break;
		case 3: val = 0x2;
			break;
		case 4: val = 0x3;
			break;
		case 6: val = 0x5;
			break;
		case 8: val = 0x7;
			break;
		case 12:val = 0xb;
			break;
	}
	i2s0_set_reg(I2S0DIV,val,I2S0_DV_MASK,I2S0_DV_OFFSET);
	i2s0_set_reg(I2S0DIV,val,I2S0_IDV_MASK,I2S0_IDV_OFFSET);

	return val;
}

/*
 * CKCFG
 */
/*useless*/

/*
 * RGADW
 */
#define I2S0_RGDIN_OFFSET       (0)
#define I2S0_RGDIN_MASK         (0xff << I2S0_RGDIN_OFFSET)
#define I2S0_RGADDR_OFFSET      (8)
#define I2S0_RGADDR_MASK        (0x7f << I2S0_RGADDR_OFFSET)
#define I2S0_RGWR_OFFSET        (16)
#define I2S0_RGWR_MASK          (0x1  << I2S0_RGWR_OFFSET)

#define test_rw_inval()         \
	i2s0_get_reg(RGADW,I2S0_RGWR_MASK,I2S0_RGWR_OFFSET)
/*
 * RGDATA
 */
#define I2S0_RGDOUT_OFFSET      (0)
#define I2S0_RGDOUT_MASK        (0xff << I2S0_RGDOUT_OFFSET)
#define I2S0_IRQ_OFFSET         (8)
#define I2S0_IRQ_MASK           (0x1  << I2S0_IRQ_OFFSET)
#define I2S0_RINVAL_OFFSET      (31)
#define I2S0_RINVAL_MASK        (0x1  << I2S0_RINVAL_OFFSET)

static int inline read_inter_codec_reg(int addr)
{
	int reval;
	while(test_rw_inval());
	i2s0_write_reg(RGADW,((addr << I2S0_RGADDR_OFFSET) & I2S0_RGADDR_MASK ));

	while((reval = i2s0_read_reg(RGDATA)) & I2S0_RINVAL_MASK);

	return reval & I2S0_RGDOUT_MASK;
}

static int inline write_inter_codec_reg(int addr,int data)
{
	while(!test_rw_inval());
	i2s0_write_reg(RGADW,(((addr << I2S0_RGADDR_OFFSET) & I2S0_RGDIN_MASK) |
				(((data)<< I2S0_RGDIN_OFFSET)& I2S0_RGDIN_MASK)));
	i2s0_write_reg( RGADW,(((addr << I2S0_RGADDR_OFFSET) & I2S0_RGDIN_MASK) |
			(((data)<< I2S0_RGDIN_OFFSET)& I2S0_RGDIN_MASK) |
			(1 << I2S0_RGWR_OFFSET)));
	if (data != read_inter_codec_reg(addr))
		return -1;
	return 0;
}

static int inline read_inter_codec_irq(void)
{
	return (i2s0_read_reg(RGDATA) & I2S0_IRQ_MASK);
}


static void inline write_inter_codec_reg_bit(int addr,int bitval,int offset)
{
	int val_tmp;
	val_tmp = read_inter_codec_reg(addr);

	if (bitval)
		val_tmp |= (1 << offset);
	else
		val_tmp &= ~(1 << offset);

	write_inter_codec_reg(addr,val_tmp);
}

static void inline write_inter_codec_reg_mask(int addr,int val, int mask,int offset)
{
	write_inter_codec_reg(addr,((read_inter_codec_reg(addr)&(~mask)) | ((val << offset) & mask)));
}

/**
 * default parameter
 **/
#define DEF_REPLAY_FMT			16
#define DEF_REPLAY_CHANNELS		2
#define DEF_REPLAY_RATE			44100

#define DEF_RECORD_FMT			16
#define DEF_RECORD_CHANNELS		2
#define DEF_RECORD_RATE			44100

#define CODEC_RMODE                     0x1
#define CODEC_WMODE                     0x2
#define CODEC_RWMODE                    0x3


/**
 * i2s0 codec control cmd
 **/
enum codec_ioctl_cmd_t {
	CODEC_INIT,
	CODEC_TURN_OFF,
	CODEC_SHUTDOWN,
	CODEC_RESET,
	CODEC_SUSPEND,
	CODEC_RESUME,
	CODEC_ANTI_POP,
	CODEC_SET_ROUTE,
	CODEC_SET_DEVICE,
	CODEC_SET_RECORD_RATE,
	CODEC_SET_RECORD_DATA_WIDTH,
	CODEC_SET_MIC_VOLUME,
	CODEC_SET_RECORD_CHANNEL,
	CODEC_SET_REPLAY_RATE,
	CODEC_SET_REPLAY_DATA_WIDTH,
	CODEC_SET_REPLAY_VOLUME,
	CODEC_SET_REPLAY_CHANNEL,
	CODEC_DAC_MUTE,
	CODEC_DEBUG_ROUTINE,
	CODEC_SET_STANDBY,
	CODEC_GET_RECORD_FMT_CAP,
	CODEC_GET_RECORD_FMT,
	CODEC_GET_REPLAY_FMT_CAP,
	CODEC_GET_REPLAY_FMT,
	CODEC_IRQ_DETECT,
	CODEC_IRQ_HANDLE,
};
/**
 * i2s0 switch state
 **/

void set_switch_state(int state);

#endif /* _XB_SND_I2S_H_ */
