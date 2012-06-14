#ifndef __TEST_H__
#define __TEST_H__

#define GPIO_SD2_VCC_EN_N	(32 * 2 + 29) /* GPC29 */
#define GPIO_SD2_CD_N		(32 * 1 + 24) /* GPB24 */

extern struct jzmmc_platform_data test_inand_pdata;
extern struct jzmmc_platform_data test_tf_pdata;
extern struct jzmmc_platform_data test_sdio_pdata;

extern struct jzfb_platform_data jzfb_pdata;
/**
 * sound platform data
 **/
extern struct snd_codec_data codec_data;

extern struct platform_device test_backlight_device;
extern struct platform_device test_lcd_device;

#endif /* __TEST_H__ */
