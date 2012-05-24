#ifndef __TEST_H__
#define __TEST_H__

#define GPIO_SD2_VCC_EN_N	(32 * 2 + 29) /* GPC29 */
#define GPIO_SD2_CD_N		(32 * 1 + 24) /* GPB24 */

extern struct jzmmc_platform_data test_inand_pdata;
extern struct jzmmc_platform_data test_tf_pdata;
extern struct jzmmc_platform_data test_sdio_pdata;
#endif /* __TEST_H__ */
