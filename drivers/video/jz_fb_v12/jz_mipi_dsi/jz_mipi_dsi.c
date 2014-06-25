/* linux/drivers/video/jz_mipi_dsi/jz_mipi_dsi.c
 *
 * Ingenic SoC MIPI-DSI dsim driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/memory.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/notifier.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>

#include <linux/gpio.h>


#include <mach/jzfb.h>
#include "jz_mipi_dsi_lowlevel.h"
#include "jz_mipi_dsih_hal.h"
#include "jz_mipi_dsi_regs.h"

#define DSI_IOBASE      0x13014000

void dump_dsi_reg(struct dsi_device *dsi);
static DEFINE_MUTEX(dsi_lock);
static struct jzdsi_platform_data *to_dsi_plat(struct platform_device
					       *pdev)
{
	return pdev->dev.platform_data;
}

int jz_dsi_video_cfg(struct dsi_device *dsi)
{
	dsih_error_t err_code = OK;
	unsigned short bytes_per_pixel_x100 = 0;	/* bpp x 100 because it can be 2.25 */
	unsigned short video_size = 0;
	unsigned int ratio_clock_xPF = 0;	/* holds dpi clock/byte clock times precision factor */
	unsigned short null_packet_size = 0;
	unsigned char video_size_step = 1;
	unsigned int total_bytes = 0;
	unsigned int bytes_per_chunk = 0;
	unsigned int no_of_chunks = 0;
	unsigned int bytes_left = 0;
	unsigned int chunk_overhead = 0;

	struct video_config *video_config;
	video_config = dsi->video_config;

	/* check DSI controller dsi */
	if ((dsi == NULL) || (video_config == NULL)) {
		return ERR_DSI_INVALID_INSTANCE;
	}
	if (dsi->state != INITIALIZED) {
		return ERR_DSI_INVALID_INSTANCE;
	}

	ratio_clock_xPF =
	    (video_config->byte_clock * PRECISION_FACTOR) /
	    (video_config->pixel_clock);
	video_size = video_config->h_active_pixels;
	/*  disable set up ACKs and error reporting */
	mipi_dsih_hal_dpi_frame_ack_en(dsi, video_config->receive_ack_packets);
	if (video_config->receive_ack_packets) {	/* if ACK is requested, enable BTA, otherwise leave as is */
		mipi_dsih_hal_bta_en(dsi, 1);
	}
	/*0:switch to high speed transfer, 1 low power mode */
	mipi_dsih_write_word(dsi, R_DSI_HOST_CMD_MODE_CFG, 0);
	/*0:enable video mode, 1:enable cmd mode */
	mipi_dsih_hal_gen_set_mode(dsi, 0);

	err_code =
	    jz_dsi_video_coding(dsi, &bytes_per_pixel_x100, &video_size_step,
				&video_size);
	if (err_code) {
		return err_code;
	}

	jz_dsi_dpi_cfg(dsi, &ratio_clock_xPF, &bytes_per_pixel_x100);

	/* TX_ESC_CLOCK_DIV must be less than 20000KHz */
	jz_dsih_hal_tx_escape_division(dsi, TX_ESC_CLK_DIV);

	/* video packetisation   */
	if (video_config->video_mode == VIDEO_BURST_WITH_SYNC_PULSES) {	/* BURST */
		//mipi_dsih_hal_dpi_null_packet_en(dsi, 0);
		mipi_dsih_hal_dpi_null_packet_size(dsi, 0);
		//mipi_dsih_hal_dpi_multi_packet_en(dsi, 0);
		err_code =
		    err_code ? err_code : mipi_dsih_hal_dpi_chunks_no(dsi, 1);
		err_code =
		    err_code ? err_code :
		    mipi_dsih_hal_dpi_video_packet_size(dsi, video_size);
		if (err_code != OK) {
			return err_code;
		}
		/* BURST by default, returns to LP during ALL empty periods - energy saving */
		mipi_dsih_hal_dpi_lp_during_hfp(dsi, 1);
		mipi_dsih_hal_dpi_lp_during_hbp(dsi, 1);
		mipi_dsih_hal_dpi_lp_during_vactive(dsi, 1);
		mipi_dsih_hal_dpi_lp_during_vfp(dsi, 1);
		mipi_dsih_hal_dpi_lp_during_vbp(dsi, 1);
		mipi_dsih_hal_dpi_lp_during_vsync(dsi, 1);
#ifdef CONFIG_DSI_DPI_DEBUG
		/*      D E B U G               */
		{
			printk("burst video");
			printk("h line time %d ,",
			       (unsigned
				short)((video_config->h_total_pixels *
					ratio_clock_xPF) / PRECISION_FACTOR));
			printk("video_size %d ,", video_size);
		}
#endif
	} else {		/* non burst transmission */
		null_packet_size = 0;
		/* bytes to be sent - first as one chunk */
		bytes_per_chunk =
		    (bytes_per_pixel_x100 * video_config->h_active_pixels) /
		    100 + VIDEO_PACKET_OVERHEAD;
		/* bytes being received through the DPI interface per byte clock cycle */
		total_bytes =
		    (ratio_clock_xPF * video_config->no_of_lanes *
		     (video_config->h_total_pixels -
		      video_config->h_back_porch_pixels -
		      video_config->h_sync_pixels)) / PRECISION_FACTOR;
		printk("---->total_bytes:%d, bytes_per_chunk:%d\n", total_bytes,
		       bytes_per_chunk);
		/* check if the in pixels actually fit on the DSI link */
		if (total_bytes >= bytes_per_chunk) {
			chunk_overhead = total_bytes - bytes_per_chunk;
			/* overhead higher than 1 -> enable multi packets */
			if (chunk_overhead > 1) {
				for (video_size = video_size_step; video_size < video_config->h_active_pixels; video_size += video_size_step) {	/* determine no of chunks */
					if ((((video_config->h_active_pixels *
					       PRECISION_FACTOR) / video_size) %
					     PRECISION_FACTOR) == 0) {
						no_of_chunks =
						    video_config->
						    h_active_pixels /
						    video_size;
						bytes_per_chunk =
						    (bytes_per_pixel_x100 *
						     video_size) / 100 +
						    VIDEO_PACKET_OVERHEAD;
						if (total_bytes >=
						    (bytes_per_chunk *
						     no_of_chunks)) {
							bytes_left =
							    total_bytes -
							    (bytes_per_chunk *
							     no_of_chunks);
							break;
						}
					}
				}
				/* prevent overflow (unsigned - unsigned) */
				if (bytes_left >
				    (NULL_PACKET_OVERHEAD * no_of_chunks)) {
					null_packet_size =
					    (bytes_left -
					     (NULL_PACKET_OVERHEAD *
					      no_of_chunks)) / no_of_chunks;
					if (null_packet_size > MAX_NULL_SIZE) {	/* avoid register overflow */
						null_packet_size =
						    MAX_NULL_SIZE;
					}
				}
			} else {	/* no multi packets */
				no_of_chunks = 1;
#ifdef CONFIG_DSI_DPI_DEBUG
				/*      D E B U G               */
				{
					printk("no multi no null video");
					printk("h line time %d",
					       (unsigned
						short)((video_config->
							h_total_pixels *
							ratio_clock_xPF) /
						       PRECISION_FACTOR));
					printk("video_size %d", video_size);
				}
#endif
				/* video size must be a multiple of 4 when not 18 loosely */
				for (video_size = video_config->h_active_pixels;
				     (video_size % video_size_step) != 0;
				     video_size++) {
					;
				}
			}
		} else {
			printk
			    ("resolution cannot be sent to display through current settings");
			err_code = ERR_DSI_OVERFLOW;

		}
	}
	err_code =
	    err_code ? err_code : mipi_dsih_hal_dpi_chunks_no(dsi,
							      no_of_chunks);
	err_code =
	    err_code ? err_code : mipi_dsih_hal_dpi_video_packet_size(dsi,
								      video_size);
	err_code =
	    err_code ? err_code : mipi_dsih_hal_dpi_null_packet_size(dsi,
								     null_packet_size);

	// mipi_dsih_hal_dpi_null_packet_en(dsi, null_packet_size > 0? 1: 0);
	// mipi_dsih_hal_dpi_multi_packet_en(dsi, (no_of_chunks > 1)? 1: 0);
#ifdef  CONFIG_DSI_DPI_DEBUG
	/*      D E B U G               */
	{
		printk("total_bytes %d ,", total_bytes);
		printk("bytes_per_chunk %d ,", bytes_per_chunk);
		printk("bytes left %d ,", bytes_left);
		printk("null packets %d ,", null_packet_size);
		printk("chunks %d ,", no_of_chunks);
		printk("video_size %d ", video_size);
	}
#endif
	mipi_dsih_hal_dpi_video_vc(dsi, video_config->virtual_channel);
	jz_dsih_dphy_no_of_lanes(dsi, video_config->no_of_lanes);
	/* enable high speed clock */
	mipi_dsih_dphy_enable_hs_clk(dsi, 1);
	printk("video configure is ok!\n");
	return err_code;

}

/* set all register settings to MIPI DSI controller. */
dsih_error_t jz_dsi_phy_cfg(struct dsi_device * dsi)
{
	dsih_error_t err = OK;
	err = jz_dsi_set_clock(dsi);
	if (err) {
		return err;
	}
	err = jz_init_dsi(dsi);
	if (err) {
		return err;
	}
	return OK;
}

/* define MIPI-DSI Master operations. */
static struct dsi_master_ops jz_master_ops = {
	.video_cfg = jz_dsi_video_cfg,
	.cmd_read = NULL,	/*jz_dsi_rd_data, */
	.cmd_write = NULL,	/*jz_dsi_wr_data, */
};

int jz_dsi_phy_open(struct dsi_device *dsi)
{
	struct video_config *video_config;
	video_config = dsi->video_config;

	pr_info("entry %s()\n", __func__);
	if (dsi->dsi_phy->status == INITIALIZED) {
		return ERR_DSI_PHY_INVALID;
	}

	jz_dsih_dphy_reset(dsi, 0);
	jz_dsih_dphy_stop_wait_time(dsi, 0x1c);	/* 0x1c: */

	if (video_config->no_of_lanes == 2 || video_config->no_of_lanes == 1) {
		jz_dsih_dphy_no_of_lanes(dsi, video_config->no_of_lanes);
	} else {
		return ERR_DSI_OUT_OF_BOUND;
	}

	jz_dsih_dphy_clock_en(dsi, 1);
	jz_dsih_dphy_shutdown(dsi, 1);
	jz_dsih_dphy_reset(dsi, 1);

	dsi->dsi_phy->status = INITIALIZED;
	return OK;
}

void dump_dsi_reg(struct dsi_device *dsi)
{
	dev_info(dsi->dev, "===========>dump dsi reg\n");
	dev_info(dsi->dev, "VERSION------------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VERSION));
	dev_info(dsi->dev, "PWR_UP:------------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_PWR_UP));
	dev_info(dsi->dev, "CLKMGR_CFG---------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_CLKMGR_CFG));
	dev_info(dsi->dev, "DPI_VCID-----------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_DPI_VCID));
	dev_info(dsi->dev, "DPI_COLOR_CODING---:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_DPI_COLOR_CODING));
	dev_info(dsi->dev, "DPI_CFG_POL--------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_DPI_CFG_POL));
	dev_info(dsi->dev, "DPI_LP_CMD_TIM-----:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_DPI_LP_CMD_TIM));
	dev_info(dsi->dev, "DBI_VCID-----------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_DBI_VCID));
	dev_info(dsi->dev, "DBI_CFG------------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_DBI_CFG));
	dev_info(dsi->dev, "DBI_PARTITIONING_EN:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_DBI_PARTITIONING_EN));
	dev_info(dsi->dev, "DBI_CMDSIZE--------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_DBI_CMDSIZE));
	dev_info(dsi->dev, "PCKHDL_CFG---------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_PCKHDL_CFG));
	dev_info(dsi->dev, "GEN_VCID-----------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_GEN_VCID));
	dev_info(dsi->dev, "MODE_CFG-----------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_MODE_CFG));
	dev_info(dsi->dev, "VID_MODE_CFG-------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_MODE_CFG));
	dev_info(dsi->dev, "VID_PKT_SIZE-------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_PKT_SIZE));
	dev_info(dsi->dev, "VID_NUM_CHUNKS-----:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_NUM_CHUNKS));
	dev_info(dsi->dev, "VID_NULL_SIZE------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_NULL_SIZE));
	dev_info(dsi->dev, "VID_HSA_TIME-------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_HSA_TIME));
	dev_info(dsi->dev, "VID_HBP_TIME-------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_HBP_TIME));
	dev_info(dsi->dev, "VID_HLINE_TIME-----:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_HLINE_TIME));
	dev_info(dsi->dev, "VID_VSA_LINES------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_VSA_LINES));
	dev_info(dsi->dev, "VID_VBP_LINES------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_VBP_LINES));
	dev_info(dsi->dev, "VID_VFP_LINES------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_VFP_LINES));
	dev_info(dsi->dev, "VID_VACTIVE_LINES--:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_VACTIVE_LINES));
	dev_info(dsi->dev, "EDPI_CMD_SIZE------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_EDPI_CMD_SIZE));
	dev_info(dsi->dev, "CMD_MODE_CFG-------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_CMD_MODE_CFG));
	dev_info(dsi->dev, "GEN_HDR------------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_GEN_HDR));
	dev_info(dsi->dev, "GEN_PLD_DATA-------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_GEN_PLD_DATA));
	dev_info(dsi->dev, "CMD_PKT_STATUS-----:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_CMD_PKT_STATUS));
	dev_info(dsi->dev, "TO_CNT_CFG---------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_TO_CNT_CFG));
	dev_info(dsi->dev, "HS_RD_TO_CNT-------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_HS_RD_TO_CNT));
	dev_info(dsi->dev, "LP_RD_TO_CNT-------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_LP_RD_TO_CNT));
	dev_info(dsi->dev, "HS_WR_TO_CNT-------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_HS_WR_TO_CNT));
	dev_info(dsi->dev, "LP_WR_TO_CNT_CFG---:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_LP_WR_TO_CNT));
	dev_info(dsi->dev, "BTA_TO_CNT---------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_BTA_TO_CNT));
	dev_info(dsi->dev, "SDF_3D-------------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_SDF_3D));
	dev_info(dsi->dev, "LPCLK_CTRL---------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_LPCLK_CTRL));
	dev_info(dsi->dev, "PHY_TMR_LPCLK_CFG--:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_PHY_TMR_LPCLK_CFG));
	dev_info(dsi->dev, "PHY_TMR_CFG--------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_PHY_TMR_CFG));
	dev_info(dsi->dev, "PHY_RSTZ-----------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_PHY_RSTZ));
	dev_info(dsi->dev, "PHY_IF_CFG---------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_PHY_IF_CFG));
	dev_info(dsi->dev, "PHY_ULPS_CTRL------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_PHY_ULPS_CTRL));
	dev_info(dsi->dev, "PHY_TX_TRIGGERS----:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_PHY_TX_TRIGGERS));
	dev_info(dsi->dev, "PHY_STATUS---------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_PHY_STATUS));
	dev_info(dsi->dev, "PHY_TST_CTRL0------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_PHY_TST_CTRL0));
	dev_info(dsi->dev, "PHY_TST_CTRL1------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_PHY_TST_CTRL1));
	dev_info(dsi->dev, "INT_ST0------------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_INT_ST0));
	dev_info(dsi->dev, "INT_ST1------------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_INT_ST1));
	dev_info(dsi->dev, "INT_MSK0-----------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_INT_MSK0));
	dev_info(dsi->dev, "INT_MSK1-----------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_INT_MSK1));
	dev_info(dsi->dev, "INT_FORCE0---------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_INT_FORCE0));
	dev_info(dsi->dev, "INT_FORCE1---------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_INT_FORCE1));
	dev_info(dsi->dev, "VID_SHADOW_CTRL----:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_SHADOW_CTRL));
	dev_info(dsi->dev, "DPI_VCID_ACT-------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_DPI_VCID_ACT));
	dev_info(dsi->dev, "DPI_COLOR_CODING_AC:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_DPI_COLOR_CODING_ACT));
	dev_info(dsi->dev, "DPI_LP_CMD_TIM_ACT-:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_DPI_LP_CMD_TIM_ACT));
	dev_info(dsi->dev, "VID_MODE_CFG_ACT---:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_MODE_CFG_ACT));
	dev_info(dsi->dev, "VID_PKT_SIZE_ACT---:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_PKT_SIZE_ACT));
	dev_info(dsi->dev, "VID_NUM_CHUNKS_ACT-:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_NUM_CHUNKS_ACT));
	dev_info(dsi->dev, "VID_HSA_TIME_ACT---:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_HSA_TIME_ACT));
	dev_info(dsi->dev, "VID_HBP_TIME_ACT---:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_HBP_TIME_ACT));
	dev_info(dsi->dev, "VID_HLINE_TIME_ACT-:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_HLINE_TIME_ACT));
	dev_info(dsi->dev, "VID_VSA_LINES_ACT--:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_VSA_LINES_ACT));
	dev_info(dsi->dev, "VID_VBP_LINES_ACT--:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_VBP_LINES_ACT));
	dev_info(dsi->dev, "VID_VFP_LINES_ACT--:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_VFP_LINES_ACT));
	dev_info(dsi->dev, "VID_VACTIVE_LINES_ACT:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_VID_VACTIVE_LINES_ACT));
	dev_info(dsi->dev, "SDF_3D_ACT---------:%08x\n",
		 mipi_dsih_read_word(dsi, R_DSI_HOST_SDF_3D_ACT));

}

void set_base_dir_tx(struct dsi_device *dsi, void *param)
{
	int i = 0;
	register_config_t phy_direction[] = {
		{0xb4, 0x02},
		{0xb8, 0xb0},
		{0xb8, 0x100b0},
		{0xb4, 0x00},
		{0xb8, 0x000b0},
		{0xb8, 0x0000},
		{0xb4, 0x02},
		{0xb4, 0x00}
	};
	i = mipi_dsih_write_register_configuration(dsi, phy_direction,
						   (sizeof(phy_direction) /
						    sizeof(register_config_t)));
	if (i != (sizeof(phy_direction) / sizeof(register_config_t))) {
		printk("ERROR setting up testchip %d", i);
	}
}

static int jz_dsi_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct dsi_device *dsi;
	struct dsi_phy *dsi_phy;
	struct jzdsi_platform_data *pdata = to_dsi_plat(pdev);
	int retry = 5;
	int st_mask = 0;
	int ret = -EINVAL;
	pr_info("entry %s()\n", __func__);

	dsi =
	    (struct dsi_device *)kzalloc(sizeof(struct dsi_device), GFP_KERNEL);
	if (!dsi) {
		dev_err(&pdev->dev, "failed to allocate dsi object.\n");
		ret = -ENOMEM;
		goto err_dsi;
	}

	dsi_phy = (struct dsi_phy *)kzalloc(sizeof(struct dsi_phy), GFP_KERNEL);
	if (!dsi_phy) {
		dev_err(&pdev->dev, "failed to allocate dsi phy  object.\n");
		ret = -ENOMEM;
		goto err_dsi_phy;
	}

	dsi->state = NOT_INITIALIZED;
	dsi->dsi_config = &(pdata->dsi_config);
	dsi->dev = &pdev->dev;
	dsi->id = pdev->id;

	dsi_phy->status = NOT_INITIALIZED;
	dsi_phy->reference_freq = REFERENCE_FREQ;	/*default 24MHz ? */
	dsi_phy->bsp_pre_config = set_base_dir_tx;
	dsi->dsi_phy = dsi_phy;
	dsi->video_config = &(pdata->video_config);
	dsi->video_config->pixel_clock = PICOS2KHZ(pdata->modes->pixclock);	// dpi_clock
	dsi->video_config->h_polarity = pdata->modes->sync & FB_SYNC_HOR_HIGH_ACT;
	dsi->video_config->h_active_pixels = pdata->modes->xres;
	dsi->video_config->h_sync_pixels = pdata->modes->hsync_len;	// min 4 pixels
	dsi->video_config->h_back_porch_pixels = pdata->modes->right_margin;
	dsi->video_config->h_total_pixels = pdata->modes->xres + pdata->modes->hsync_len + pdata->modes->left_margin + pdata->modes->right_margin;
	dsi->video_config->v_active_lines = pdata->modes->yres;
	dsi->video_config->v_polarity =  pdata->modes->sync & FB_SYNC_VERT_HIGH_ACT;	//1:active high, 0: active low
	dsi->video_config->v_sync_lines = pdata->modes->vsync_len;
	dsi->video_config->v_back_porch_lines = pdata->modes->lower_margin;
	dsi->video_config->v_total_lines = pdata->modes->yres + pdata->modes->upper_margin + pdata->modes->lower_margin + pdata->modes->vsync_len;
	dsi->master_ops = &jz_master_ops;
	dsi->cmd_list = pdata->cmd_list;
	dsi->cmd_packet_len = pdata->cmd_packet_len;

	dsi->clock = clk_get(&pdev->dev, "dsi");
	if (IS_ERR(dsi->clock)) {
		dev_err(&pdev->dev, "failed to get dsi clock source\n");
		ret = -ENODEV;
		goto err_clock_get;
	}

	clk_enable(dsi->clock);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get io memory region\n");
		ret = -ENODEV;
		goto err_platform_get;
	}
	dev_dbg(dsi->dev,
		"res->start:0x%x, resource_size(res):%d,dev_name(&pdev->dev):%s\n",
		res->start, resource_size(res), dev_name(&pdev->dev));
	dsi->res =
	    request_mem_region(res->start, resource_size(res),
			       dev_name(&pdev->dev));
	if (!dsi->res) {
		dev_err(&pdev->dev, "failed to request io memory region\n");
		ret = -ENOMEM;
		goto err_mem_region;
	}

	dsi->address = (unsigned int)ioremap(res->start, resource_size(res));
	if (!dsi->address) {
		dev_err(&pdev->dev, "failed to remap io region\n");
		ret = -ENOMEM;
		goto err_ioremap;
	}
	dsi->dsi_phy->address = dsi->address;

	/*select mipi dsi */
	*((volatile unsigned int *)0xb30500a4) = 1 << 7;	//MCTRL

	ret = jz_dsi_phy_open(dsi);
	if (ret) {
		goto err_phy_open;
	}

	/*set command mode */
	mipi_dsih_write_word(dsi, R_DSI_HOST_MODE_CFG, 0x1);
	/*set this register for cmd size, default 0x6 */
	mipi_dsih_write_word(dsi, R_DSI_HOST_EDPI_CMD_SIZE, 0x6);

	/*
	 * jz_dsi_phy_cfg:
	 * PLL programming, config the output freq to DEFAULT_DATALANE_BPS.
	 * */
	ret = jz_dsi_phy_cfg(dsi);
	if (ret) {
		goto err_phy_cfg;
	}

	pr_info("wait for phy config ready\n");
	if (dsi->video_config->no_of_lanes == 2)
		st_mask = 0x95;
	else {
		st_mask = 0x15;
	}

	/*checkout phy clk lock and  clklane, datalane stopstate  */
	while ((mipi_dsih_read_word(dsi, R_DSI_HOST_PHY_STATUS) & st_mask) !=
	       st_mask && retry--) {
			printk("phy status = %08x\n", mipi_dsih_read_word(dsi, R_DSI_HOST_PHY_STATUS));
	}


	if (!retry)
		goto err_phy_state;

	dsi->state = INITIALIZED;
	pdata->dsi_state = &dsi->state;

#ifdef CONFIG_DSI_DPI_DEBUG	/*test pattern */
	unsigned int tmp = 0;
	/*low power */
	mipi_dsih_write_word(dsi, R_DSI_HOST_CMD_MODE_CFG, 0xffffff0);

	lcd_panel_init(dsi, dsi->cmd_list, dsi->cmd_packet_len);

	mipi_dsih_dphy_enable_hs_clk(dsi, 1);
	jz_dsi_video_cfg(dsi);

	tmp = mipi_dsih_read_word(dsi, R_DSI_HOST_VID_MODE_CFG);
	tmp |= 1 << 16 | 0 << 20 | 1 << 24;
	mipi_dsih_write_word(dsi, R_DSI_HOST_VID_MODE_CFG, tmp);
#endif

	return 0;
err_phy_state:
	dev_err(dsi->dev, "jz dsi phy state error\n");
err_phy_cfg:
	dev_err(dsi->dev, "jz dsi phy cfg error\n");
err_phy_open:
	dev_err(dsi->dev, "jz dsi phy open error\n");
err_ioremap:
	release_mem_region(dsi->res->start, resource_size(dsi->res));
err_mem_region:
	release_resource(dsi->res);
err_platform_get:
	clk_put(dsi->clock);
err_clock_get:
	kfree(dsi_phy);
err_dsi_phy:
	kfree(dsi);
err_dsi:
	return ret;
}

static int jz_dsi_remove(struct platform_device *pdev)
{
	struct dsi_device *dsi = platform_get_drvdata(pdev);
	struct dsi_phy *dsi_phy;
	dsi_phy = dsi->dsi_phy;

	iounmap((unsigned int *)dsi->address);
	clk_disable(dsi->clock);
	clk_put(dsi->clock);


	release_resource(dsi->res);
	release_mem_region(dsi->res->start, resource_size(dsi->res));

	kfree(dsi_phy);
	kfree(dsi);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int jz_dsi_suspend(struct device *dev)
{
	 /*FIXED*/ return 0;
}

static int jz_dsi_resume(struct device *dev)
{
	 /*FIXED*/ return 0;
}
#endif

static const struct dev_pm_ops jz_dsi_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(jz_dsi_suspend, jz_dsi_resume)
};

struct platform_driver jz_dsi_driver = {
	.probe = jz_dsi_probe,
	.remove = jz_dsi_remove,
	.driver = {
		   .name = "jz-dsi",
		   .owner = THIS_MODULE,
		   .pm = &jz_dsi_pm_ops,
		   },
};

static struct resource jz_dsi_resources[] = {
	[0] = {
	       .start = DSI_IOBASE,
	       .end = DSI_IOBASE + 0x190,
	       .flags = IORESOURCE_MEM,
	       },
};

struct platform_device jz_dsi_device = {
	.name = "jz-dsi",
	.id = -1,
	.dev = {
		.platform_data = &jzdsi_pdata,
	},
	.num_resources = ARRAY_SIZE(jz_dsi_resources),
	.resource = jz_dsi_resources,
};



/*static int __init jz_dsi_init(void)
{
	return platform_driver_register(&jz_dsi_driver);
}

static void __exit jz_dsi_exit(void)
{
	platform_driver_unregister(&jz_dsi_driver);
}

arch_initcall(jz_dsi_init);
module_exit(jz_dsi_exit);
*/
MODULE_AUTHOR("ykliu <ykliu@ingenic.cn>");
MODULE_DESCRIPTION("Ingenic SoC MIPI-DSI driver");
MODULE_LICENSE("GPL");
