#include <mach/camera.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#define CAMERA_RST			GPIO_PD(27)
#define CAMERA_PWDN_N		GPIO_PA(13) /* pin conflict with USB_ID */
#define CAMERA_MCLK			GPIO_PE(2) /* no use */



#if defined(CONFIG_VIDEO_OVISP)

int temp = 1;
#if defined(CONFIG_VIDEO_OV5645)
static int ov5645_power(int onoff)
{
	if(temp) {
	gpio_request(CAMERA_PWDN_N, "CAMERA_PWDN_N");
	gpio_request(CAMERA_RST, "CAMERA_RST");
		temp = 0;
	}
	if (onoff) {
		printk("[board camera]:%s, power on\n", __func__);
		//gpio_direction_output(CAMERA_PWDN_N, 0);
		mdelay(10);
		//gpio_direction_output(CAMERA_PWDN_N, 1);
		//gpio_direction_output(CAMERA_RST, 1);   /*PWM0 */
		;
	} else {
		printk("[board camera]:%s, power off\n", __func__);
		//gpio_direction_output(CAMERA_PWDN_N, 0);
		gpio_direction_output(CAMERA_RST, 0);   /*PWM0 */
		;
	}

	return 0;
}

static int ov5645_reset(void)
{
	printk("[board camera]:%s\n", __func__);

#if 1
	/*reset*/
	gpio_direction_output(CAMERA_RST, 1);   /*PWM0 */
	mdelay(10);
	gpio_direction_output(CAMERA_RST, 0);   /*PWM0 */
	mdelay(10);
	gpio_direction_output(CAMERA_RST, 1);   /*PWM0 */
#endif
	return 0;
}

static struct i2c_board_info ov5645_board_info = {
	.type = "ov5645",
	.addr = 0x3c,
};
#endif /* CONFIG_VIDEO_OV5645 */

static struct ovisp_camera_client ovisp_camera_clients[] = {
#if defined(CONFIG_VIDEO_OV5645)
	{
		.board_info = &ov5645_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI,
		.mclk_rate = 24000000,
		.max_video_width = 2592,
		.max_video_height = 1944,
		.power = ov5645_power,
		.reset = ov5645_reset,
	},
#endif /* CONFIG_VIDEO_OV5645 */
};

struct ovisp_camera_platform_data ovisp_camera_info = {
	.i2c_adapter_id = 3,
#ifdef CONFIG_OVISP_I2C
	.flags = CAMERA_USE_ISP_I2C | CAMERA_USE_HIGH_BYTE
			| CAMERA_I2C_PIO_MODE | CAMERA_I2C_STANDARD_SPEED,
#else
	.flags = CAMERA_USE_HIGH_BYTE
			| CAMERA_I2C_PIO_MODE | CAMERA_I2C_STANDARD_SPEED,
#endif
	.client = ovisp_camera_clients,
	.client_num = ARRAY_SIZE(ovisp_camera_clients),
};
#endif /* CONFIG_VIDEO_OVISP */
