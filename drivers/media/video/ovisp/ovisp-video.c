
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/bug.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>

#include "ovisp-isp.h"
#include "ovisp-video.h"
#include "ovisp-videobuf.h"
#include "ovisp-debugtool.h"
#include "isp-debug.h"

#include "ovisp-csi.h"
#include "../ov5645.h"
static struct ovisp_camera_format formats[] = {
	{
		.name     = "YUV 4:2:2 packed, YCbYCr",
		.code	  = V4L2_MBUS_FMT_SBGGR8_1X8,
		.fourcc   = V4L2_PIX_FMT_YUYV,
		.depth    = 16,
	},
	{
		.name     = "YUV 4:2:0 semi planar, Y/CbCr",
		.code	  = V4L2_MBUS_FMT_SBGGR8_1X8,
		.fourcc   = V4L2_PIX_FMT_NV12,
		.depth    = 12,
	},
#if 0
	{
		.name     = "YUV 4:2:0 semi planar, Y/CbCr",
		.code	  = V4L2_MBUS_FMT_SBGGR10_1X10,
		.fourcc   = V4L2_PIX_FMT_NV12YUV422,
		.depth    = 12,
	},
#endif
	{
		.name	  ="RAW8",
		.code	  = V4L2_MBUS_FMT_SBGGR8_1X8,
		.fourcc	  = V4L2_PIX_FMT_SBGGR8,
		.depth	  = 8,
	}
};

static struct ovisp_camera_format bypass_formats[] = {
	{
		.name     = "YUV 4:2:2 packed, YCbYCr",
		.code	  = V4L2_MBUS_FMT_YUYV8_2X8,
		.fourcc   = V4L2_PIX_FMT_YUYV,
		.depth    = 16,
	},
	{
		.name	  ="RAW8",
		.code	  = V4L2_MBUS_FMT_SBGGR8_1X8,
		.fourcc	  = V4L2_PIX_FMT_SBGGR8,
		.depth	  = 8,
	}
};

static int ovisp_subdev_mclk_on(struct ovisp_camera_dev *camdev,
			int index)
{
	int ret;

	if (camdev->csd[index].mclk) {
		clk_enable(camdev->csd[index].mclk);
	} else {
		ret = isp_dev_call(camdev->isp, mclk_on, index);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return -EINVAL;
	}

	return 0;
}

static int ovisp_subdev_mclk_off(struct ovisp_camera_dev *camdev,
			int index)
{
	int ret;

	if (camdev->csd[index].mclk) {
		clk_disable(camdev->csd[index].mclk);
	} else {
		ret = isp_dev_call(camdev->isp, mclk_off, index);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return -EINVAL;
	}

	return 0;
}

static int ovisp_subdev_power_on(struct ovisp_camera_dev *camdev,
					struct ovisp_camera_client *client,
					int index)
{
	struct ovisp_camera_subdev *csd = &camdev->csd[index];
	struct v4l2_cropcap caps;
	int ret;

	////ret = ovisp_subdev_mclk_on(camdev, index);
	//if (ret)
	//return ret;

	/* first camera work power on */
	if(!regulator_is_enabled(camdev->camera_power))
	    regulator_enable(camdev->camera_power);

	if (client->power) {
		ret = client->power(1);
		if (ret)
			goto err;
	}

	if (client->reset) {
		ret = client->reset();
		if (ret)
			goto err;
	}

	csd->prop.bypass = csd->bypass;
	csd->prop.index = index;
	ret = isp_dev_call(camdev->isp, open, &csd->prop);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		goto err;


	ISP_PRINT(ISP_INFO,"--%s:%d index=%d\n", __func__, __LINE__, index);
	if (camdev->input >= 0) {
		ISP_PRINT(ISP_INFO,"--%s:%d\n", __func__, __LINE__);
		ret = v4l2_subdev_call(csd->sd, core, s_power, 1);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			goto err;
	ISP_PRINT(ISP_INFO,"--%s:%d\n", __func__, __LINE__);
		ret = v4l2_subdev_call(csd->sd, core, reset, 1);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			goto err;
	ISP_PRINT(ISP_INFO,"--%s:%d\n", __func__, __LINE__);
		ret = v4l2_subdev_call(csd->sd, core, init, 1);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			goto err;
	ISP_PRINT(ISP_INFO,"--%s:%d\n", __func__, __LINE__);
		ret = v4l2_subdev_call(csd->sd, video, cropcap, &caps);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			goto err;
	ISP_PRINT(ISP_INFO,"--%s:%d\n", __func__, __LINE__);
		csd->max_width = caps.bounds.width;
		csd->max_height = caps.bounds.height;
	ISP_PRINT(ISP_INFO,"--%s:%d\n", __func__, __LINE__);
		ret = isp_dev_call(camdev->isp, config, NULL);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			goto err;
	}
	ISP_PRINT(ISP_INFO,"--%s:%d\n", __func__, __LINE__);
	return 0;

err:
	ovisp_subdev_mclk_off(camdev, index);
	return -EINVAL;
}

static int ovisp_subdev_power_off(struct ovisp_camera_dev *camdev,
					struct ovisp_camera_client *client,
					int index)
{
	struct ovisp_camera_subdev *csd = &camdev->csd[index];
	int ret;


	if (camdev->input >= 0) {
		ret = v4l2_subdev_call(csd->sd, core, s_power, 0);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return -EINVAL;
	}

	ret = isp_dev_call(camdev->isp, close, &csd->prop);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		return -EINVAL;

	/*digital power down*/
	if (client->power) {
		ret = client->power(0);
		if (ret)
			return -EINVAL;
	}
	/*cpu_xxx power should not be power on or off*/
	/*analog power off*/
	if(regulator_is_enabled(camdev->camera_power))
		        regulator_disable(camdev->camera_power);

	ovisp_subdev_mclk_off(camdev, index);

	return 0;
}

struct ovisp_camera_format *ovisp_camera_find_format(
					struct ovisp_camera_dev *camdev,
					struct v4l2_format *f)
{
	struct ovisp_camera_subdev *csd = &camdev->csd[camdev->input];
	struct ovisp_camera_format *fmt;
	unsigned int num;
	unsigned int i;

	if (csd->bypass) {
		fmt = bypass_formats;
		num = ARRAY_SIZE(bypass_formats);
	} else {
		fmt = formats;
		num = ARRAY_SIZE(formats);
	}

	for (i = 0; i < num; i++) {
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			break;
		fmt++;
	}

	if (i == num)
		return NULL;

	return fmt;
}

static int ovisp_camera_get_mclk(struct ovisp_camera_dev *camdev,
					struct ovisp_camera_client *client,
					int index)
{
	struct ovisp_camera_subdev *csd = &camdev->csd[index];
	struct clk* mclk_parent;

	if (!(client->flags & CAMERA_CLIENT_CLK_EXT) || !client->mclk_name) {
		csd->mclk = NULL;
		return 0;
	}

	csd->mclk = clk_get(camdev->dev, client->mclk_name);
	if (IS_ERR(csd->mclk)) {
		ISP_PRINT(ISP_ERROR,"Cannot get sensor input clock \"%s\"\n",
			client->mclk_name);
		return PTR_ERR(csd->mclk);
	}

	if (client->mclk_parent_name) {
		mclk_parent = clk_get(camdev->dev, client->mclk_parent_name);
		if (IS_ERR(mclk_parent)) {
			ISP_PRINT(ISP_ERROR,"Cannot get sensor input parent clock \"%s\"\n",
				client->mclk_parent_name);
			clk_put(csd->mclk);
			return PTR_ERR(mclk_parent);
		}

		clk_set_parent(csd->mclk, mclk_parent);
		clk_put(mclk_parent);
	}

	clk_set_rate(csd->mclk, client->mclk_rate);

	return 0;
}

static int ovisp_camera_init_client(struct ovisp_camera_dev *camdev,
					struct ovisp_camera_client *client,
					int index)
{
	struct ovisp_camera_subdev *csd = &camdev->csd[index];
	int i2c_adapter_id;
	int err = 0;
	int ret;

	if (!client->board_info) {
		ISP_PRINT(ISP_ERROR,"Invalid client info\n");
		return -EINVAL;
	}

	if (client->flags & CAMERA_CLIENT_INDEP_I2C) {
		i2c_adapter_id = client->i2c_adapter_id;
		ISP_PRINT(ISP_INFO,"111111111111\n");
	}
	else {
		i2c_adapter_id = camdev->pdata->i2c_adapter_id;
		ISP_PRINT(ISP_INFO,"2222222222222222222\n");
	}
	if (client->flags & CAMERA_CLIENT_ISP_BYPASS)
		csd->bypass = 1;
	else
		csd->bypass = 0;

	//ret = ovisp_camera_get_mclk(camdev, client, index);
	//if (ret)
	//return ret;

	ret = isp_dev_call(camdev->isp, init, NULL);
	if (ret) {
		ISP_PRINT(ISP_ERROR,"Failed to init isp\n");
		clk_put(csd->mclk);
		return -EINVAL;
	}

	ret = ovisp_subdev_power_on(camdev, client, index);
	if (ret) {
		ISP_PRINT(ISP_ERROR,"Failed to power on subdev(%s)\n",
			client->board_info->type);
		clk_put(csd->mclk);
		err = -ENODEV;
		goto isp_dev_release;
	}

	csd->i2c_adap = i2c_get_adapter(i2c_adapter_id);
	if (!csd->i2c_adap) {
		ISP_PRINT(ISP_ERROR,"Cannot get I2C adapter(%d)\n",
			i2c_adapter_id);
		clk_put(csd->mclk);
		err = -ENODEV;
		goto subdev_power_off;
	}
	ISP_PRINT(ISP_INFO,"i2c...........................1111111\n");
	csd->sd = v4l2_i2c_new_subdev_board(&camdev->v4l2_dev,
		csd->i2c_adap,
		client->board_info,
		NULL);
	ISP_PRINT(ISP_INFO,"i2c...........................222222222\n");
	if (!csd->sd) {
		ISP_PRINT(ISP_ERROR,"Cannot get subdev(%s)\n",
			client->board_info->type);
		clk_put(csd->mclk);
		i2c_put_adapter(csd->i2c_adap);
		err = -EIO;
		goto subdev_power_off;
	}

subdev_power_off:
	ret = ovisp_subdev_power_off(camdev, client, index);
	if (ret) {
		ISP_PRINT(ISP_ERROR,"Failed to power off subdev(%s)\n",
			client->board_info->type);
		err = -EINVAL;
	}

isp_dev_release:
	ret = isp_dev_call(camdev->isp, release, NULL);
	if (ret) {
		ISP_PRINT(ISP_ERROR,"Failed to init isp\n");
		err = -EINVAL;
	}
	ISP_PRINT(ISP_INFO,"--%s:%d\n", __func__, __LINE__);
	return err;
}

static void ovisp_camera_free_client(struct ovisp_camera_dev *camdev,
			int index)
{
	struct ovisp_camera_subdev *csd = &camdev->csd[index];

	v4l2_device_unregister_subdev(csd->sd);
	i2c_unregister_device(v4l2_get_subdevdata(csd->sd));
	i2c_put_adapter(csd->i2c_adap);
	clk_put(csd->mclk);
}

static int ovisp_camera_init_subdev(struct ovisp_camera_dev *camdev)
{
	struct ovisp_camera_platform_data *pdata = camdev->pdata;
	unsigned int i;
	int ret;

	if (!pdata->client) {
		ISP_PRINT(ISP_ERROR,"Invalid client data\n");
		return -EINVAL;
	}

	camdev->clients = 0;
	for (i = 0; i < pdata->client_num; i++) {
		ret = ovisp_camera_init_client(camdev,
					&pdata->client[i], camdev->clients);
		if (ret)
			continue;

		camdev->csd[camdev->clients++].client = &pdata->client[i];
		if (camdev->clients > OVISP_CAMERA_CLIENT_NUM) {
			ISP_PRINT(ISP_ERROR,"Too many clients\n");
			return -EINVAL;
		}
	}

	ISP_PRINT(ISP_INFO,"Detect %d clients\n", camdev->clients);

	if (camdev->clients == 0)
		return -ENODEV;

	return 0;
}

static void ovisp_camera_free_subdev(struct ovisp_camera_dev *camdev)
{
	unsigned int i;

	for (i = 0; i < camdev->clients; i++)
		ovisp_camera_free_client(camdev, i);
}

static int ovisp_camera_active(struct ovisp_camera_dev *camdev)
{
	struct ovisp_camera_capture *capture = &camdev->capture;

	return capture->running;
}

static int ovisp_camera_update_buffer(struct ovisp_camera_dev *camdev, int index)
{
	struct ovisp_camera_capture *capture = &camdev->capture;
	struct isp_buffer buf;
	int ret;

	if(camdev->vbq.memory == V4L2_MEMORY_MMAP){
		buf.addr = ovisp_vb2_plane_paddr(&capture->active[index]->vb, 0);
	}
	else if(camdev->vbq.memory == V4L2_MEMORY_USERPTR){
		buf.addr = ovisp_vb2_plane_vaddr(&capture->active[index]->vb, 0);
	}else{
		ISP_PRINT(ISP_ERROR,"the type of memory isn't supported!\n");
		return -EINVAL;
	}
	ISP_PRINT(ISP_INFO,"%s:buf.addr:0x%lx  index = %d\n", __func__, buf.addr, index);
	ret = isp_dev_call(camdev->isp, update_buffer, &buf, index);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		return -EINVAL;

	return 0;
}
#if 0
static int ovisp_camera_enable_capture(struct ovisp_camera_dev *camdev)
{
	struct ovisp_camera_capture *capture = &camdev->capture;
	struct isp_buffer buf;
	int ret;

	if(camdev->vbq.memory == V4L2_MEMORY_MMAP){
		buf.addr = ovisp_vb2_plane_paddr(&capture->active->vb, 0);
	}
	else if(camdev->vbq.memory == V4L2_MEMORY_USERPTR){
		buf.addr = ovisp_vb2_plane_vaddr(&capture->active->vb, 0);
	}else{
		ISP_PRINT(ISP_ERROR,"the type of memory isn't supported!\n");
		return -EINVAL;
	}
	ISP_PRINT(ISP_INFO,"%s:%d buf.addr:0x%lx\n", __func__,__LINE__, buf.addr);
	ret = isp_dev_call(camdev->isp, enable_capture, &buf);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		return -EINVAL;

	return 0;
}

static int ovisp_camera_disable_capture(struct ovisp_camera_dev *camdev)
{
	int ret;

	ret = isp_dev_call(camdev->isp, disable_capture, NULL);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		return -EINVAL;

	return 0;
}
#endif
static int ovisp_camera_start_capture(struct ovisp_camera_dev *camdev)
{
	struct ovisp_camera_capture *capture = &camdev->capture;
	struct isp_capture cap;
	int ret;
	if (capture->running)
		return 0;

	ISP_PRINT(ISP_INFO,"*********************start_capture begin*************************\n");
	ISP_PRINT(ISP_INFO,"Start capture(%s)\n",
			camdev->snapshot ? "snapshot" : "preview");
#if 0
	/*get physical addr*/
	if(camdev->vbq.memory == V4L2_MEMORY_MMAP){
		cap.buf.addr = ovisp_vb2_plane_paddr(&capture->active->vb, 0);
	}
	else if(camdev->vbq.memory == V4L2_MEMORY_USERPTR){
		cap.buf.addr = ovisp_vb2_plane_vaddr(&capture->active->vb, 0);
	}else{
		ISP_PRINT(ISP_ERROR,"the type of memory isn't supported!\n");
		return -EINVAL;
	}
#endif
	cap.snapshot = camdev->snapshot;
	cap.client = camdev->csd[camdev->input].client;
	capture->running = 1;
	ret = isp_dev_call(camdev->isp, start_capture, &cap);
	if (ret < 0 && ret != -ENOIOCTLCMD) {
		ISP_PRINT(ISP_ERROR,"Start capture failed %d\n", ret);
		return -EINVAL;
	}

	/*ret = v4l2_subdev_call(sd, core, g_register, &frame->vmfmt);*/
	ISP_PRINT(ISP_INFO,"*********************start_capture end*************************\n");

	return 0;
}

static int ovisp_camera_stop_capture(struct ovisp_camera_dev *camdev)
{
	struct ovisp_camera_capture *capture = &camdev->capture;
	int ret;

	ISP_PRINT(ISP_INFO,"Stop capture(%s)\n",
			camdev->snapshot ? "snapshot" : "preview");

	ret = isp_dev_call(camdev->isp, stop_capture, NULL);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		return -EINVAL;

	capture->active[0] = NULL;
	capture->active[1] = NULL;
	capture->running = 0;

	return 0;
}

static int ovisp_camera_start_streaming(struct ovisp_camera_dev *camdev)
{
	struct ovisp_camera_subdev *csd = &camdev->csd[camdev->input];
	struct ovisp_camera_capture *capture = &camdev->capture;
	struct ovisp_camera_frame *frame = &camdev->frame;
	int ret;

	capture->running = 0;
	capture->active[0] = NULL;
	capture->active[1] = NULL;

	if (!camdev->snapshot && !csd->bypass) {
		if (csd->client->max_video_width
				&& csd->client->max_video_height) {
			frame->ifmt.dev_width = csd->client->max_video_width;
			frame->ifmt.dev_height = csd->client->max_video_height;
			frame->vmfmt.width = frame->ifmt.dev_width;
			frame->vmfmt.height = frame->ifmt.dev_height;
		}
	}

	ISP_PRINT(ISP_WARNING,"Set format(%s). ISP %dx%d,%x/%x. device %dx%d,%x\n",
			camdev->snapshot ? "snapshot" : "preview",
			frame->ifmt.width, frame->ifmt.height,
			frame->ifmt.code, frame->ifmt.fourcc,
			frame->vmfmt.width, frame->vmfmt.height,
			frame->ifmt.code);

	ret = v4l2_subdev_call(csd->sd, video, s_mbus_fmt, &frame->vmfmt);
	if (ret && ret != -ENOIOCTLCMD) {
		ISP_PRINT(ISP_ERROR,"Failed to set device format\n");
		return -EINVAL;
	}

	/*1. csi phy start here*/
	ret = isp_dev_call(camdev->isp, pre_fmt, &frame->ifmt);
	if (ret < 0 && ret != -ENOIOCTLCMD) {
		ISP_PRINT(ISP_ERROR,"Failed to set isp format\n");
		return -EINVAL;
	}

	if(camdev->first_init) {
		ret = v4l2_subdev_call(csd->sd, video, s_stream, 1);
		camdev->first_init = 0;
	}

	/*now csi got data!!!!!*/
	if (ret && ret != -ENOIOCTLCMD)
		if (ret && ret != -ENOIOCTLCMD) {
			ISP_PRINT(ISP_ERROR,"Failed to set device stream\n");
			return -EINVAL;
		}

	/*set isp format */
	ret = isp_dev_call(camdev->isp, s_fmt, &frame->ifmt);
	if (ret < 0 && ret != -ENOIOCTLCMD) {
		ISP_PRINT(ISP_ERROR,"Failed to set isp format\n");
		return -EINVAL;
	}
	/*now start capture,
	 * in this procedure, we will use firmware to set set format
	 *
	 * */
	ISP_PRINT(ISP_INFO,"the main procedure ended ........., now start capture ......\n");
//	return ovisp_camera_start_capture(camdev);
	return 0;
}

static int ovisp_camera_stop_streaming(struct ovisp_camera_dev *camdev)
{
	//	int ret;
	int status;

	if (!ovisp_camera_active(camdev))
		return 0;

	status = ovisp_camera_stop_capture(camdev);

	//	if (ret && ret != -ENOIOCTLCMD)
	//		return ret;

	return status;
}

static int ovisp_camera_try_format(struct ovisp_camera_dev *camdev)
{
	struct ovisp_camera_frame *frame = &camdev->frame;
	struct ovisp_camera_subdev *csd = &camdev->csd[camdev->input];
	int ret;

	ISP_PRINT(ISP_INFO,"--%s:%d  %d\n", __func__, __LINE__, camdev->input);

	frame->ifmt.width = frame->width;
	frame->ifmt.height = frame->height;
	if (csd->bypass) {
		ISP_PRINT(ISP_INFO,"%s:%d %d\n", __func__, __LINE__, frame->width);
		frame->ifmt.dev_width = frame->width;
		frame->ifmt.dev_height = frame->height;
	} else {
		ISP_PRINT(ISP_INFO,"%s:%d %d\n", __func__, __LINE__, csd->max_width);
		frame->ifmt.dev_width = csd->max_width;
		frame->ifmt.dev_height = csd->max_height;
	}
	frame->ifmt.code = frame->fmt->code;
	frame->ifmt.fourcc = frame->fmt->fourcc;
	frame->ifmt.fmt_data = v4l2_get_fmt_data(&frame->vmfmt);
	memset(frame->ifmt.fmt_data, 0, sizeof(*frame->ifmt.fmt_data));

	/*to match the format we supported*/
	ret = isp_dev_call(camdev->isp, try_fmt, &frame->ifmt);

	ISP_PRINT(ISP_INFO,"%s:%d\n", __func__, __LINE__);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		return -EINVAL;

	ISP_PRINT(ISP_INFO,"%s:%d\n", __func__, __LINE__);
	frame->vmfmt.width = frame->ifmt.dev_width;
	frame->vmfmt.height = frame->ifmt.dev_height;
	frame->vmfmt.code = frame->ifmt.code;
	/*to match the sensor supported format*/
	ret = v4l2_subdev_call(csd->sd, video, try_mbus_fmt, &frame->vmfmt);
	if (ret && ret != -ENOIOCTLCMD)
		return -EINVAL;

	ISP_PRINT(ISP_INFO,"%s:%d %d %d\n", __func__, __LINE__, frame->vmfmt.width, frame->ifmt.dev_width);
	ISP_PRINT(ISP_INFO,"%s:%d %d %d\n", __func__, __LINE__, frame->vmfmt.height, frame->ifmt.dev_height);
	ISP_PRINT(ISP_INFO,"%s:%d %08X %08X\n", __func__, __LINE__, frame->vmfmt.code, frame->ifmt.code);

#if 1
	if ((frame->vmfmt.width != frame->ifmt.dev_width)
			|| (frame->vmfmt.height != frame->ifmt.dev_height)
			|| (frame->vmfmt.code != frame->ifmt.code))
		return -EINVAL;
#endif
	ISP_PRINT(ISP_INFO,"%s:%d\n", __func__, __LINE__);
	return 0;
}

static int ovisp_camera_irq_notify(unsigned int status, void *data)
{
	struct ovisp_camera_dev *camdev = (struct ovisp_camera_dev *)data;
	struct ovisp_camera_capture *capture = &camdev->capture;
	struct ovisp_camera_frame *frame = &camdev->frame;
	struct ovisp_camera_buffer *buf = NULL;
	unsigned long flags;

	if (!capture->running)
		return 0;

	//ISP_PRINT(ISP_INFO,"%s:%d,<<<irq_notify>>> status:0x%x\n", __func__, __LINE__, status);
	if (status & ISP_NOTIFY_DATA_DONE) {
		buf = NULL;
		spin_lock_irqsave(&camdev->slock, flags);
		if((status & ISP_NOTIFY_DATA_DONE0) == ISP_NOTIFY_DATA_DONE0){
			buf = capture->active[0];
			capture->active[0] = NULL;
		}else{
			buf = capture->active[1];
			capture->active[1] = NULL;
		}
		spin_unlock_irqrestore(&camdev->slock, flags);
		if(buf && buf->vb.state == VB2_BUF_STATE_ACTIVE){
			if(camdev->snapshot)
				buf->vb.v4l2_buf.field = frame->field;
			buf->vb.v4l2_buf.sequence = capture->out_frames++;
			do_gettimeofday(&buf->vb.v4l2_buf.timestamp);

			vb2_buffer_done(&buf->vb, VB2_BUF_STATE_DONE);
		}else
			capture->lose_frames++;

	}
	if (status & ISP_NOTIFY_DATA_START){
		buf = NULL;
		capture->in_frames++;
		spin_lock_irqsave(&camdev->slock, flags);
		if (!list_empty(&capture->list)){
			buf = list_entry(capture->list.next,
				struct ovisp_camera_buffer, list);
			list_del(&buf->list);
		}
		spin_unlock_irqrestore(&camdev->slock, flags);
		if(buf){
			if((status & ISP_NOTIFY_DATA_START0)== ISP_NOTIFY_DATA_START0){
				capture->active[1] = buf;
				ovisp_camera_update_buffer(camdev, 1);
			}else{
				capture->active[0] = buf;
				ovisp_camera_update_buffer(camdev, 0);
			}
		}
	}

	if (status & ISP_NOTIFY_OVERFLOW)
		capture->error_frames++;

	if (status & ISP_NOTIFY_DROP_FRAME){
		buf = NULL;
		capture->drop_frames++;
		spin_lock_irqsave(&camdev->slock, flags);
		if (!list_empty(&capture->list)){
			buf = list_entry(capture->list.next,
				struct ovisp_camera_buffer, list);
			list_del(&buf->list);
		}
		spin_unlock_irqrestore(&camdev->slock, flags);
		if(buf){
			if((status & ISP_NOTIFY_DROP_FRAME0) == ISP_NOTIFY_DROP_FRAME0){
				capture->active[0] = buf;
				ovisp_camera_update_buffer(camdev, 0);
			}else{
				capture->active[1] = buf;
				ovisp_camera_update_buffer(camdev, 1);
			}
		}
	}

	return 0;
}

static int ovisp_vb2_queue_setup(struct vb2_queue *vq, unsigned int *nbuffers,
		unsigned int *nplanes, unsigned long sizes[],
		void *alloc_ctxs[])
{
	struct ovisp_camera_dev *camdev = vb2_get_drv_priv(vq);
	struct ovisp_camera_capture *capture = &camdev->capture;
	struct ovisp_camera_frame *frame = &camdev->frame;
	unsigned long size;

	size = (frame->width * frame->height * frame->fmt->depth) >> 3;
	if (0 == *nbuffers)
		*nbuffers = 32;

	while (size * *nbuffers > OVISP_CAMERA_BUFFER_MAX)
		(*nbuffers)--;

	*nplanes = 1;
	sizes[0] = size;
#if 0
	/*sizes[0] = 640*480*6;*/
	if(frame->ifmt.fourcc == V4L2_PIX_FMT_NV12YUV422)
		sizes[0] = size + 640*360*2;
	else
		sizes[0] = 1280*960*2;
#endif
	alloc_ctxs[0] = camdev->alloc_ctx;
	ISP_PRINT(ISP_INFO,"*nbuffers = %d\n",*nbuffers);
	if (*nbuffers == 1)
		camdev->snapshot = 1;
	else
		camdev->snapshot = 0;

	INIT_LIST_HEAD(&capture->list);
	capture->active[0] = NULL;
	capture->active[1] = NULL;
	capture->running = 0;
	capture->drop_frames = 0;
	capture->lose_frames = 0;
	capture->error_frames = 0;
	capture->in_frames = 0;
	capture->out_frames = 0;

	return 0;
}

static int ovisp_vb2_buffer_init(struct vb2_buffer *vb)
{
	vb->v4l2_buf.reserved = ovisp_vb2_plane_paddr(vb, 0);

	return 0;
}

static int ovisp_vb2_buffer_prepare(struct vb2_buffer *vb)
{
	struct ovisp_camera_buffer *buf =
		container_of(vb, struct ovisp_camera_buffer, vb);
	struct ovisp_camera_dev *camdev = vb2_get_drv_priv(vb->vb2_queue);
	struct ovisp_camera_frame *frame = &camdev->frame;
	unsigned long size;

	if (NULL == frame->fmt) {
		ISP_PRINT(ISP_ERROR,"Format not set\n");
		return -EINVAL;
	}

	if (frame->width  < 48 || frame->width  > OVISP_CAMERA_WIDTH_MAX ||
			frame->height < 32 || frame->height > OVISP_CAMERA_HEIGHT_MAX) {
		ISP_PRINT(ISP_ERROR,"Invalid format (%dx%d)\n",
				frame->width, frame->height);
		return -EINVAL;
	}

	size = (frame->width * frame->height * frame->fmt->depth) >> 3;
	if (vb2_plane_size(vb, 0) < size) {
		ISP_PRINT(ISP_ERROR,"Data will not fit into plane (%lu < %lu)\n",
				vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}

	vb2_set_plane_payload(&buf->vb, 0, size);

	vb->v4l2_buf.reserved = ovisp_vb2_plane_paddr(vb, 0);
	return 0;
}

static void ovisp_vb2_buffer_queue(struct vb2_buffer *vb)
{
	struct ovisp_camera_buffer *buf =
		container_of(vb, struct ovisp_camera_buffer, vb);
	struct ovisp_camera_dev *camdev = vb2_get_drv_priv(vb->vb2_queue);
	struct ovisp_camera_capture *capture = &camdev->capture;
	unsigned long flags;

	ISP_PRINT(ISP_INFO,"%s=========1=======%d\n",__func__, __LINE__);
	spin_lock_irqsave(&camdev->slock, flags);
	list_add_tail(&buf->list, &capture->list);
#if 0
	ISP_PRINT(ISP_INFO,"%s:%d ---add_list---\n",__func__,__LINE__);
	if (!capture->active) {
		ISP_PRINT(ISP_INFO,"%s:%d ---active = NULL---\n",__func__,__LINE__);
		capture->active = buf;
		if (capture->running)
			ovisp_camera_enable_capture(camdev);
	}
#endif
	spin_unlock_irqrestore(&camdev->slock, flags);

//	ISP_PRINT(ISP_INFO,"capture->running:%d, capture->active:%p\n",
//			capture->running, capture->active);
	if (!capture->running) {
		ISP_PRINT(ISP_INFO,"%s:%d start_capture!\n",__func__,__LINE__);
		ovisp_camera_start_capture(camdev);
	}
//	ISP_PRINT(ISP_INFO,"%s=========1=======%d\n",__func__, __LINE__);
}

static int ovisp_vb2_start_streaming(struct vb2_queue *vq)
{
	struct ovisp_camera_dev *camdev = vb2_get_drv_priv(vq);

	if (ovisp_camera_active(camdev))
		return -EBUSY;

	return ovisp_camera_start_streaming(camdev);
}

static int ovisp_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct ovisp_camera_dev *camdev = vb2_get_drv_priv(vq);

	if (!ovisp_camera_active(camdev))
		return -EINVAL;

	return ovisp_camera_stop_streaming(camdev);
}

static void ovisp_vb2_lock(struct vb2_queue *vq)
{
	struct ovisp_camera_dev *camdev = vb2_get_drv_priv(vq);
	mutex_lock(&camdev->mlock);
}

static void ovisp_vb2_unlock(struct vb2_queue *vq)
{
	struct ovisp_camera_dev *camdev = vb2_get_drv_priv(vq);
	mutex_unlock(&camdev->mlock);
}

static struct vb2_ops ovisp_vb2_qops = {
	.queue_setup		= ovisp_vb2_queue_setup,
	.buf_init		= ovisp_vb2_buffer_init,
	.buf_prepare		= ovisp_vb2_buffer_prepare,
	.buf_queue		= ovisp_vb2_buffer_queue,
	.wait_prepare		= ovisp_vb2_unlock,
	.wait_finish		= ovisp_vb2_lock,
	.start_streaming	= ovisp_vb2_start_streaming,
	.stop_streaming		= ovisp_vb2_stop_streaming,
};

static int ovisp_vidioc_querycap(struct file *file, void  *priv,
		struct v4l2_capability *cap)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);

	strcpy(cap->driver, "ovisp");
	strcpy(cap->card, "ovisp");
	strlcpy(cap->bus_info, camdev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = OVISP_CAMERA_VERSION;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	return 0;
}
/**
* ovisp_vidioc_g_priority() - get priority handler
* @file: file ptr
* @priv: file handle
* @prio: ptr to v4l2_priority structure
*/
static int ovisp_vidioc_g_priority(struct file *file, void *priv,
                          enum v4l2_priority *prio)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	*prio = v4l2_prio_max(&camdev->prio);
	return 0;
}
/**
* ovisp_vidioc_s_priority() - set priority handler
* @file: file ptr
* @priv: file handle
* @prio: ptr to v4l2_priority structure
*/
static int ovisp_vidioc_s_priority(struct file *file, void *priv, enum v4l2_priority p)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	struct ovisp_fh *fh = priv;
	return v4l2_prio_change(&camdev->prio, &fh->prio, p);
}

static int ovisp_vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
		struct v4l2_fmtdesc *f)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	struct ovisp_camera_subdev *csd = &camdev->csd[camdev->input];
	struct ovisp_camera_frame *frame = &camdev->frame;
	struct ovisp_camera_format *fmt;
	int ret;

	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);
	if (csd->bypass) {
		if (f->index >= ARRAY_SIZE(bypass_formats))
			return -EINVAL;

		fmt = &bypass_formats[f->index];
	} else {
		if (f->index >= ARRAY_SIZE(formats))
			return -EINVAL;

		fmt = &formats[f->index];

		memset(&frame->ifmt, 0, sizeof(frame->ifmt));
		frame->ifmt.fourcc = fmt->fourcc;
		ret = isp_dev_call(camdev->isp, check_fmt, &frame->ifmt);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return -EINVAL;
	}

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}

static int ovisp_vidioc_g_fmt_vid_cap(struct file *file, void *priv,
		struct v4l2_format *f)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	struct ovisp_camera_frame *frame = &camdev->frame;
	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);

	f->fmt.pix.width        = frame->width;
	f->fmt.pix.height       = frame->height;
	f->fmt.pix.field        = frame->field;
	f->fmt.pix.pixelformat  = frame->fmt->fourcc;
	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * frame->fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;
	return 0;
}

#define FUNC_LINE  ISP_PRINT(ISP_INFO,"%s,%d\n", __func__, __LINE__)
static int ovisp_vidioc_try_fmt_vid_cap(struct file *file, void *priv,
		struct v4l2_format *f)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	struct ovisp_camera_frame *frame = &camdev->frame;
	int ret;

	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);

	if (f->fmt.pix.field == V4L2_FIELD_ANY)
		f->fmt.pix.field = V4L2_FIELD_INTERLACED;
	else if (V4L2_FIELD_INTERLACED != f->fmt.pix.field)
		return -EINVAL;


	frame->fmt = ovisp_camera_find_format(camdev, f);
	if (!frame->fmt) {
		ISP_PRINT(ISP_ERROR,"Fourcc format (0x%08x) invalid\n",
				f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	v4l_bound_align_image(&f->fmt.pix.width,
			48, OVISP_CAMERA_WIDTH_MAX, 2,
			&f->fmt.pix.height,
			32, OVISP_CAMERA_HEIGHT_MAX, 0, 0);
	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * frame->fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;

	frame->width = f->fmt.pix.width;
	frame->height = f->fmt.pix.height;
	frame->field = f->fmt.pix.field;

	ret = ovisp_camera_try_format(camdev);
	if (ret) {
		ISP_PRINT(ISP_ERROR,"Format(%dx%d,%x/%x) is unsupported\n",
				f->fmt.pix.width,
				f->fmt.pix.height,
				frame->fmt->code,
				frame->fmt->fourcc);
		return ret;
	}

	return 0;
}

static int ovisp_vidioc_s_fmt_vid_cap(struct file *file, void *priv,
		struct v4l2_format *f)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	struct vb2_queue *q = &camdev->vbq;
	struct ovisp_fh *fh = priv;
	int ret;

	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);
	/* check priority */
	ret = v4l2_prio_check(&camdev->prio, fh->prio);
	if(0 != ret)
		return ret;


	ret = ovisp_vidioc_try_fmt_vid_cap(file, priv, f);
	if (ret < 0)
		return ret;

	if (vb2_is_streaming(q))
		return -EBUSY;

	return 0;
}

static int ovisp_vidioc_reqbufs(struct file *file, void *priv,
		struct v4l2_requestbuffers *p)
{
	int ret = 0;
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	struct vb2_queue *q = &camdev->vbq;
	ISP_PRINT(ISP_INFO,"%s[%d]\n", __func__, __LINE__);
	if(p->type != q->type){
		ISP_PRINT(ISP_ERROR,"%s[%d] req->type = %d, queue->type = %d\n", __func__, __LINE__,
					p->type, q->type);
		return -EINVAL;
	}
	if(p->memory == V4L2_MEMORY_USERPTR){
		if(p->count == 0)
			ret = isp_dev_call(camdev->isp, tlb_unmap_all_vaddr);
		else
			ret = isp_dev_call(camdev->isp, tlb_init);
		if(ret < 0){
			ISP_PRINT(ISP_ERROR,"%s[%d] tlb operator failed!\n", __func__, __LINE__);
			return -EINVAL;
		}
	}
	return vb2_reqbufs(&camdev->vbq, p);
}

static int ovisp_vidioc_querybuf(struct file *file, void *priv,
		struct v4l2_buffer *p)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);
	return vb2_querybuf(&camdev->vbq, p);
}

static int ovisp_vidioc_qbuf(struct file *file, void *priv,
		struct v4l2_buffer *p)
{
	int ret = 0;
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);
	if(p->memory == V4L2_MEMORY_USERPTR){
//		dma_cache_wback_inv(0x80000000,4*1024*1024);
		dma_cache_sync(NULL,(void *)(p->m.userptr),p->length, DMA_FROM_DEVICE);
		ret = isp_dev_call(camdev->isp, tlb_map_one_vaddr,p->m.userptr,p->length);
		if(ret < 0){
			ISP_PRINT(ISP_ERROR,"%s[%d] tlb operator failed!\n", __func__, __LINE__);
			return -EINVAL;
		}
	}
	return vb2_qbuf(&camdev->vbq, p);
}

static int ovisp_vidioc_dqbuf(struct file *file, void *priv,
		struct v4l2_buffer *p)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	int status;
	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);
	status = vb2_dqbuf(&camdev->vbq, p, file->f_flags & O_NONBLOCK);
	return status;
}

static int ovisp_vidioc_streamon(struct file *file, void *priv,
		enum v4l2_buf_type i)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);

	if (ovisp_camera_active(camdev))
		return -EBUSY;

	return vb2_streamon(&camdev->vbq, i);
}

static int ovisp_vidioc_streamoff(struct file *file, void *priv,
		enum v4l2_buf_type i)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);
	return vb2_streamoff(&camdev->vbq, i);
}

static int ovisp_vidioc_enum_input(struct file *file, void *priv,
		struct v4l2_input *inp)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	struct ovisp_camera_client *client;

	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);
	if (inp->index >= camdev->clients)
		return -EINVAL;

	client = camdev->csd[inp->index].client;

	inp->type = V4L2_INPUT_TYPE_CAMERA;
	inp->std = V4L2_STD_525_60;
	strncpy(inp->name, client->board_info->type, I2C_NAME_SIZE);

	return 0;
}

static int ovisp_vidioc_g_input(struct file *file, void *priv,
		unsigned int *index)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);
	*index = camdev->input;
	return 0;
}

static int ovisp_vidioc_s_input(struct file *file, void *priv,
		unsigned int index)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	struct ovisp_fh *fh = priv;
	int ret;
	ISP_PRINT(ISP_INFO,"--%s:%d\n", __func__, __LINE__);
	if (ovisp_camera_active(camdev))
		return -EBUSY;

	if (index == camdev->input)
		return 0;

	if (index >= camdev->clients) {
		ISP_PRINT(ISP_ERROR,"Input(%d) exceeds chip maximum(%d)\n",
				index, camdev->clients);
		return -EINVAL;
	}
	/* check priority */
	ret = v4l2_prio_check(&camdev->prio, fh->prio);
	if(0 != ret)
		return ret;


	if (camdev->input >= 0) {
		/* Old client. */
		ret = ovisp_subdev_power_off(camdev,
				camdev->csd[camdev->input].client,
				camdev->input);
		if (ret) {
			ISP_PRINT(ISP_ERROR,"Failed to power off subdev\n");
			return ret;
		}
	}

	/* New client. */
	camdev->input = index;
	ret = isp_dev_call(camdev->isp, init, NULL);
	if (ret) {
		ISP_PRINT(ISP_ERROR,"Failed to init isp\n");
		ret = -EINVAL;
		goto err;
	}

	ret = ovisp_subdev_power_on(camdev,
			camdev->csd[index].client, index);
	if (ret) {
		ISP_PRINT(ISP_ERROR,"Failed to power on subdev\n");
		ret = -EIO;
		goto release_isp;
	}

	ISP_PRINT(ISP_INFO,"Select client %d\n", index);

	return 0;

release_isp:
	isp_dev_call(camdev->isp, release, NULL);
err:
	camdev->input = -1;
	return ret;
}

static int ovisp_vidioc_s_ctrl(struct file *file, void *priv,
		struct v4l2_control *ctrl)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	struct ovisp_camera_subdev *csd = &camdev->csd[camdev->input];
	int ret = 0;
	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);

	if (csd->bypass)
		ret = v4l2_subdev_call(csd->sd, core, s_ctrl, ctrl);
	else
		ret = isp_dev_call(camdev->isp, s_ctrl, ctrl);

	return ret;
}

static int ovisp_vidioc_g_ctrl(struct file *file, void *priv,
		struct v4l2_control *ctrl)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	struct ovisp_camera_subdev *csd = &camdev->csd[camdev->input];
	int ret = 0;

	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);
	if (csd->bypass)
		ret = v4l2_subdev_call(csd->sd, core, g_ctrl, ctrl);
	else
		ret = isp_dev_call(camdev->isp, g_ctrl, ctrl);

	return ret;
}

static int ovisp_vidioc_cropcap(struct file *file, void *priv,
		struct v4l2_cropcap *a)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	struct ovisp_camera_subdev *csd = &camdev->csd[camdev->input];
	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);

	a->bounds.left			= 0;
	a->bounds.top			= 0;
	a->bounds.width			= csd->max_width;
	a->bounds.height		= csd->max_height;
	a->defrect			= a->bounds;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	ISP_PRINT(ISP_INFO,"CropCap %dx%d\n", csd->max_width, csd->max_height);

	return 0;
}

static int ovisp_vidioc_g_crop(struct file *file, void *priv,
		struct v4l2_crop *a)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	struct ovisp_camera_subdev *csd = &camdev->csd[camdev->input];
	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);

	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= csd->max_width;
	a->c.height	= csd->max_height;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int ovisp_vidioc_s_crop(struct file *file, void *priv,
		struct v4l2_crop *a)
{
	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);
	return 0;
}

static int ovisp_vidioc_s_parm(struct file *file, void *priv,
		struct v4l2_streamparm *parm)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	struct ovisp_camera_subdev *csd = &camdev->csd[camdev->input];
	int ret = 0;
	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);

	if (csd->bypass)
		ret = v4l2_subdev_call(csd->sd, video, s_parm, parm);
	else
		ret = isp_dev_call(camdev->isp, s_parm, parm);

	return ret;

}

static int ovisp_vidioc_g_parm(struct file *file, void *priv,
		struct v4l2_streamparm *parm)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	struct ovisp_camera_subdev *csd = &camdev->csd[camdev->input];
	int ret = 0;
	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);

	if (csd->bypass)
		ret = v4l2_subdev_call(csd->sd, video, g_parm, parm);
	else
		ret = isp_dev_call(camdev->isp, g_parm, parm);

	return ret;
}

static int ovisp_v4l2_open(struct file *file)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	struct ovisp_fh *fh;

	ISP_PRINT(ISP_INFO,"Open camera. refcnt %d\n", camdev->refcnt);
	camdev->first_init = 1;

	if (++camdev->refcnt == 1) {
		camdev->input = -1;
	}

	fh = kzalloc(sizeof(*fh), GFP_KERNEL);
	if(NULL == fh)
		goto fh_alloc_fail;
	file->private_data = fh;
	/* Initialize priority of this instance to default priority */
	fh->prio = V4L2_PRIORITY_DEFAULT;
	v4l2_prio_open(&camdev->prio, &fh->prio);
	ISP_PRINT(ISP_INFO,"%s==========%d\n", __func__, __LINE__);
	//	camdev->input = 0;
	return 0;
fh_alloc_fail:
	return -ENOMEM;
}

static int ovisp_v4l2_close(struct file *file)
{
	int ret = 0;
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	struct ovisp_fh *fh = file->private_data;
	struct v4l2_subdev *sd = camdev->csd->sd;
	ISP_PRINT(ISP_INFO,"Close camera. refcnt %d\n", camdev->refcnt);
	dump_sensor_exposure(sd);

	if (--camdev->refcnt == 0) {
		ovisp_camera_stop_streaming(camdev);
		vb2_queue_release(&camdev->vbq);
		ovisp_subdev_power_off(camdev,
				camdev->csd[camdev->input].client, camdev->input);
		isp_dev_call(camdev->isp, release, NULL);
		camdev->input = -1;
	}
	/* Close the priority */
	v4l2_prio_close(&camdev->prio, fh->prio);

	if(camdev->isp->tlb_flag)
		ret = isp_dev_call(camdev->isp, tlb_deinit);
	return ret;
}

static unsigned int ovisp_v4l2_poll(struct file *file,
		struct poll_table_struct *wait)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	return vb2_poll(&camdev->vbq, file, wait);
}

static int ovisp_v4l2_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct ovisp_camera_dev *camdev = video_drvdata(file);
	return vb2_mmap(&camdev->vbq, vma);
}

static const struct v4l2_ioctl_ops ovisp_v4l2_ioctl_ops = {

	/* VIDIOC_QUERYCAP handler */
	.vidioc_querycap		= ovisp_vidioc_querycap,
	/* Priority handling */
	.vidioc_s_priority		= ovisp_vidioc_s_priority,
	.vidioc_g_priority		= ovisp_vidioc_g_priority,

	.vidioc_enum_fmt_vid_cap	= ovisp_vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap		= ovisp_vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap		= ovisp_vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap		= ovisp_vidioc_s_fmt_vid_cap,

	/*frame management*/
	.vidioc_reqbufs             = ovisp_vidioc_reqbufs,
	.vidioc_querybuf            = ovisp_vidioc_querybuf,
	.vidioc_qbuf                = ovisp_vidioc_qbuf,
	.vidioc_dqbuf               = ovisp_vidioc_dqbuf,

	/**/
	.vidioc_enum_input          = ovisp_vidioc_enum_input,
	.vidioc_g_input             = ovisp_vidioc_g_input,
	.vidioc_s_input             = ovisp_vidioc_s_input,

	/*isp function, modified according to spec*/
	.vidioc_g_ctrl	            = ovisp_vidioc_g_ctrl,
	.vidioc_s_ctrl              = ovisp_vidioc_s_ctrl,
	.vidioc_cropcap             = ovisp_vidioc_cropcap,
	.vidioc_g_crop              = ovisp_vidioc_g_crop,
	.vidioc_s_crop              = ovisp_vidioc_s_crop,
	.vidioc_s_parm              = ovisp_vidioc_s_parm,
	.vidioc_g_parm              = ovisp_vidioc_g_parm,

	.vidioc_streamon            = ovisp_vidioc_streamon,
	.vidioc_streamoff           = ovisp_vidioc_streamoff,
};

static struct v4l2_file_operations ovisp_v4l2_fops = {
	.owner 		= THIS_MODULE,
	.open 		= ovisp_v4l2_open,
	.release 	= ovisp_v4l2_close,
	.poll		= ovisp_v4l2_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap 		= ovisp_v4l2_mmap,
};

static struct video_device ovisp_camera = {
	.name = "ovisp-camera",
	.minor = -1,
	.release = video_device_release,
	.fops = &ovisp_v4l2_fops,
	.ioctl_ops = &ovisp_v4l2_ioctl_ops,
};

static int ovisp_camera_probe(struct platform_device *pdev)
{
	struct ovisp_camera_dev *camdev;
	struct ovisp_camera_platform_data *pdata;
	struct video_device *vfd;
	struct isp_device *isp;
	struct vb2_queue *q;
	struct resource *res;
	int irq;
	int ret;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		ISP_PRINT(ISP_ERROR,"Platform data not set\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (!res || !irq) {
		ISP_PRINT(ISP_ERROR,"Not enough platform resources");
		return -ENODEV;
	}

	res = request_mem_region(res->start,
			res->end - res->start + 1, dev_name(&pdev->dev));
	if (!res) {
		ISP_PRINT(ISP_ERROR,"Not enough memory for resources\n");
		return -EBUSY;
	}

	camdev = kzalloc(sizeof(*camdev), GFP_KERNEL);
	if (!camdev) {
		ISP_PRINT(ISP_ERROR,"Failed to allocate camera device\n");
		ret = -ENOMEM;
		goto exit;
	}

	isp = kzalloc(sizeof(*isp), GFP_KERNEL);
	if (!isp) {
		ISP_PRINT(ISP_ERROR,"Failed to allocate isp device\n");
		ret = -ENOMEM;
		goto free_camera_dev;;
	}

	isp->irq = irq;
	isp->res = res;
	isp->dev = &pdev->dev;
	isp->pdata = pdata;
	isp->irq_notify = ovisp_camera_irq_notify;
	isp->data = camdev;
#if 0
	isp->csi_power = regulator_get(isp->dev, "csi_avdd");
	if(IS_ERR(isp->csi_power)) {
		    dev_warn(isp->dev, "csi regulator missing\n");
	}
#endif
	ret = isp_device_init(isp);
	if (ret) {
		ISP_PRINT(ISP_ERROR,"Unable to init isp device.n");
		goto free_isp_dev;
	}

	snprintf(camdev->v4l2_dev.name, sizeof(camdev->v4l2_dev.name),
			"%s", dev_name(&pdev->dev));
	ret = v4l2_device_register(NULL, &camdev->v4l2_dev);
	if (ret) {
		ISP_PRINT(ISP_ERROR,"Failed to register v4l2 device\n");
		ret = -ENOMEM;
		goto release_isp_dev;
	}

	spin_lock_init(&camdev->slock);

	camdev->isp = isp;
	camdev->dev = &pdev->dev;
	camdev->pdata = pdata;
	camdev->input = -1;
	camdev->refcnt = 0;
	camdev->frame.fmt = &formats[0];
	camdev->frame.width = 0;
	camdev->frame.height = 0;
	camdev->frame.field = V4L2_FIELD_INTERLACED;
	camdev->first_init = 1;

	camdev->camera_power = regulator_get(camdev->dev, "cpu_avdd");
	if(IS_ERR(camdev->camera_power)) {
		dev_warn(camdev->dev, "camera regulator missing\n");
	}


	if (sizeof(camdev->frame.vmfmt.reserved)
			< sizeof(*camdev->frame.ifmt.fmt_data)) {
		ISP_PRINT(ISP_ERROR,"V4l2 format info struct is too large\n");
		ret = -EINVAL;
		goto unreg_v4l2_dev;
	}

#if 1
	/* Initialize contiguous memory allocator */
	camdev->alloc_ctx = ovisp_vb2_init_ctx(camdev->dev);
	if (IS_ERR(camdev->alloc_ctx)) {
		ret = PTR_ERR(camdev->alloc_ctx);
		goto unreg_v4l2_dev;
	}
#endif


#ifdef OVISP_DEBUGTOOL_ENABLE
	camdev->offline.size = 10*1024*1024;
	camdev->offline.vaddr = dma_alloc_coherent(camdev->dev,
			camdev->offline.size,
			&(camdev->offline.paddr),
			GFP_KERNEL);

	ISP_PRINT(ISP_WARNING,"camdev->offline.vaddr:0x%08lx,camdev->offline.paddr:0x%08lx",
			(unsigned long)camdev->offline.vaddr,
			(unsigned long)camdev->offline.paddr);

	camdev->bracket.size = 10*1024*1024;
	camdev->bracket.vaddr = dma_alloc_coherent(camdev->dev,
			camdev->bracket.size,
			&(camdev->bracket.paddr),
			GFP_KERNEL);
	ISP_PRINT(ISP_WARNING, "camdev->bracket.vaddr:0x%08lx,camdev->bracket.paddr:0x%08lx",
			(unsigned long)camdev->bracket.vaddr,
			(unsigned long)camdev->bracket.paddr);
#endif

	ret = ovisp_camera_init_subdev(camdev);
	if (ret) {
		ISP_PRINT(ISP_ERROR,"Failed to register v4l2 device\n");
		ret = -ENOMEM;
		goto cleanup_ctx;
	}
	/* Initialize queue. */
	q = &camdev->vbq;
	memset(q, 0, sizeof(camdev->vbq));
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP | VB2_USERPTR;
	q->drv_priv = camdev;
	q->buf_struct_size = sizeof(struct ovisp_camera_buffer);
	q->ops = &ovisp_vb2_qops;
	q->mem_ops = &ovisp_vb2_memops;

	vb2_queue_init(q);

	mutex_init(&camdev->mlock);

	vfd = video_device_alloc();
	if (!vfd) {
		ISP_PRINT(ISP_ERROR,"Failed to allocate video device\n");
		ret = -ENOMEM;
		goto free_i2c;
	}

	memcpy(vfd, &ovisp_camera, sizeof(ovisp_camera));
	vfd->lock = &camdev->mlock;
	vfd->v4l2_dev = &camdev->v4l2_dev;
	vfd->debug = V4L2_DEBUG_IOCTL | V4L2_DEBUG_IOCTL_ARG;
	//vfd->debug = V4L2_DEBUG_IOCTL;
	camdev->vfd = vfd;

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, -1);
	if (ret < 0) {
		ISP_PRINT(ISP_ERROR,"Failed to register video device\n");
		goto free_video_device;
	}

	video_set_drvdata(vfd, camdev);
	platform_set_drvdata(pdev, camdev);

	/* init v4l2_priority */
	v4l2_prio_init(&camdev->prio);

	return 0;

free_video_device:
	video_device_release(vfd);
free_i2c:
	ovisp_camera_free_subdev(camdev);
cleanup_ctx:
	ovisp_vb2_cleanup_ctx(camdev->alloc_ctx);
unreg_v4l2_dev:
	v4l2_device_unregister(&camdev->v4l2_dev);
release_isp_dev:
	isp_device_release(camdev->isp);
free_isp_dev:
	kfree(camdev->isp);
free_camera_dev:
	kfree(camdev);
exit:
	return ret;

}

static int __exit ovisp_camera_remove(struct platform_device *pdev)
{
	struct ovisp_camera_dev *camdev = platform_get_drvdata(pdev);

	regulator_put(camdev->camera_power);
	video_device_release(camdev->vfd);
	v4l2_device_unregister(&camdev->v4l2_dev);
	platform_set_drvdata(pdev, NULL);
#ifdef OVISP_DEBUGTOOL_ENABLE
	dma_free_coherent(camdev->dev, camdev->offline.size, camdev->offline.vaddr, (dma_addr_t)camdev->offline.paddr);
#endif
	ovisp_vb2_cleanup_ctx(camdev->alloc_ctx);
	ovisp_camera_free_subdev(camdev);
	isp_device_release(camdev->isp);
	kfree(camdev->isp);
	kfree(camdev);

	return 0;
}

#ifdef CONFIG_PM
static int ovisp_camera_suspend(struct device *dev)
{
	struct ovisp_camera_dev *camdev = dev_get_drvdata(dev);

	isp_dev_call(camdev->isp, suspend, NULL);

	return 0;
}

static int ovisp_camera_resume(struct device *dev)
{
	struct ovisp_camera_dev *camdev = dev_get_drvdata(dev);

	isp_dev_call(camdev->isp, resume, NULL);

	return 0;
}

static struct dev_pm_ops ovisp_camera_pm_ops = {
	.suspend = ovisp_camera_suspend,
	.resume = ovisp_camera_resume,
};
#endif

static struct platform_driver ovisp_camera_driver = {
	.probe = ovisp_camera_probe,
	.remove = __exit_p(ovisp_camera_remove),
	.driver = {
		.name = "ovisp-camera",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &ovisp_camera_pm_ops,
#endif
	},
};

static int __init ovisp_camera_init(void)
{
	return platform_driver_register(&ovisp_camera_driver);
}

static void __exit ovisp_camera_exit(void)
{
	platform_driver_unregister(&ovisp_camera_driver);
}

module_init(ovisp_camera_init);
module_exit(ovisp_camera_exit);

MODULE_DESCRIPTION("OVISP camera driver");
MODULE_LICENSE("GPL");

