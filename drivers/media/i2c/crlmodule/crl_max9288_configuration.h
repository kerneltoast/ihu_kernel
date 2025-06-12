/*
 * crl_max9288_configuration.h
 *
 * V4L-i2c platform driver for video input device on Delphi IHU board.
 * Copyright (C) 2017 Delphi Technologies, Inc.
 * Authors: Hakan Johansson <hakan.johansson@delphi.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __CRLMODULE_MAX9288_CONFIGURATION_H_
#define __CRLMODULE_MAX9288_CONFIGURATION_H_

#include "crlmodule-sensor-ds.h"

struct crl_ctrl_data_pair max9288_ctrl_data_lanes[] = {
	{
		.ctrl_id = V4L2_CID_MIPI_LANES,
		.data = 4,
	},

};

static struct crl_pll_configuration max9288_pll_configs[] = {
	{
		.input_clk =   24000000,
		.op_sys_clk = 445500000,
		.bitsperpixel = 24,
		.pixel_rate_csi = 891000000,
		.pixel_rate_pa = 891000000,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = 0,
		.pll_regs = NULL,
		.csi_lanes = 4,
	},
};


static struct crl_subdev_rect_rep max9288_1080p_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 1080,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 1080,
	},
};

static struct crl_subdev_rect_rep max9288_768p_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 1080,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 768,
		.out_rect.height = 1024,
	},
};

static struct crl_subdev_rect_rep max9288_720p_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 1080,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1280,
		.out_rect.height = 720,
	},
};

static struct crl_subdev_rect_rep max9288_VGA_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 1080,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 640,
		.out_rect.height = 480,
	},
};

static struct crl_mode_rep max9288_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(max9288_1080p_rects),
		.sd_rects = max9288_1080p_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1920,
		.height = 1080,
		.comp_items = 1,
		.ctrl_data = &max9288_ctrl_data_lanes[0],
		.mode_regs_items = 0,
		.mode_regs = NULL,
	},
	{
		.sd_rects_items = ARRAY_SIZE(max9288_768p_rects),
		.sd_rects = max9288_768p_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 768,
		.height = 1024,
		.comp_items = 1,
		.ctrl_data = &max9288_ctrl_data_lanes[0],
		.mode_regs_items = 0,
		.mode_regs = NULL,
	},
	{
		.sd_rects_items = ARRAY_SIZE(max9288_720p_rects),
		.sd_rects = max9288_720p_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1280,
		.height = 720,
		.comp_items = 1,
		.ctrl_data = &max9288_ctrl_data_lanes[0],
		.mode_regs_items = 0,
		.mode_regs = NULL,
	},
	{
		.sd_rects_items = ARRAY_SIZE(max9288_VGA_rects),
		.sd_rects = max9288_VGA_rects,
		.binn_hor = 3,
		.binn_vert = 2,
		.scale_m = 1,
		.width = 640,
		.height = 480,
		.comp_items = 1,
		.ctrl_data = &max9288_ctrl_data_lanes[0],
		.mode_regs_items = 0,
		.mode_regs = NULL,
	},

};

static struct crl_sensor_subdev_config max9288_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "MAX9288 binner",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "MAX9288 pixel array",
	},
};

static struct crl_sensor_limits max9288_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 1920,
	.y_addr_max = 1080,
	.min_frame_length_lines = 160,
	.max_frame_length_lines = 65535,
	.min_line_length_pixels = 6024,
	.max_line_length_pixels = 32752,
	.scaler_m_min = 1,
	.scaler_m_max = 1,
	.scaler_n_min = 1,
	.scaler_n_max = 1,
	.min_even_inc = 1,
	.max_even_inc = 1,
	.min_odd_inc = 1,
	.max_odd_inc = 1,
};

static struct crl_csi_data_fmt max9288_crl_csi_data_fmt[] = {
	{
		.code = MEDIA_BUS_FMT_RGB888_1X24,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 24,
	},
};

static struct crl_v4l2_ctrl max9288_v4l2_ctrls[] = {
	{
		.sd_type = CRL_SUBDEV_TYPE_BINNER,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_IDLE,
		.ctrl_id = V4L2_CID_LINK_FREQ,
		.name = "V4L2_CID_LINK_FREQ",
		.type = CRL_V4L2_CTRL_TYPE_MENU_INT,
		.impact = CRL_IMPACTS_NO_IMPACT,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_GET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_PIXEL_RATE,
		.name = "V4L2_CID_PIXEL_RATE_PA",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 0,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_BINNER,
		.op_type = CRL_V4L2_CTRL_GET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_PIXEL_RATE,
		.name = "V4L2_CID_PIXEL_RATE_CSI",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 0,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_BINNER,
		.op_type = CRL_V4L2_CTRL_GET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_MIPI_LANES,
		.name = "V4L2_CID_MIPI_LANES",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 4,
		.data.std_data.max = 4,
		.data.std_data.step = 2,
		.data.std_data.def = 4,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
};


int max9288_sensor_init(struct i2c_client *);
int max9288_sensor_cleanup(struct i2c_client *);
int max9288_sensor_stream_start(struct i2c_client *);
int max9288_sensor_stream_stop(struct i2c_client *);

static struct crl_sensor_configuration max9288_crl_configuration = {

	.sensor_init = max9288_sensor_init,
	.sensor_cleanup = max9288_sensor_cleanup,

	.sensor_stream_start = max9288_sensor_stream_start,
	.sensor_stream_stop = max9288_sensor_stream_stop,

	.powerup_regs_items = 0,
	.powerup_regs = NULL,

	.poweroff_regs_items = 0,
	.poweroff_regs = NULL,

	.subdev_items = ARRAY_SIZE(max9288_sensor_subdevs),
	.subdevs = max9288_sensor_subdevs,

	.sensor_limits = &max9288_sensor_limits,

	.pll_config_items = ARRAY_SIZE(max9288_pll_configs),
	.pll_configs = max9288_pll_configs,

	.modes_items = ARRAY_SIZE(max9288_modes),
	.modes = max9288_modes,

	.streamon_regs_items = 0,
	.streamon_regs = NULL,

	.streamoff_regs_items = 0,
	.streamoff_regs = NULL,

	.v4l2_ctrls_items = ARRAY_SIZE(max9288_v4l2_ctrls),
	.v4l2_ctrl_bank = max9288_v4l2_ctrls,

	.csi_fmts_items = ARRAY_SIZE(max9288_crl_csi_data_fmt),
	.csi_fmts = max9288_crl_csi_data_fmt,
};

#endif  /* __CRLMODULE_MAX9288_CONFIGURATION_H_ */

