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

#ifdef CONFIG_LCD_AUO_A043FL01V2
extern struct platform_device auo_a043fl01v2_device;
#endif
#ifdef CONFIG_LCD_AT070TN93
extern struct platform_device at070tn93_device;
#endif

#endif /* __TEST_H__ */
