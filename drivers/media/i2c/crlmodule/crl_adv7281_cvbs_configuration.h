
/*
 * Copyright (c) 2015--2016 Intel Corporation. All Rights Reserved.
 *
 * Author: Jianxu Zheng <jian.xu.zheng@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __CRLMODULE_ADV7281_CVBS_CONFIGURATION_H_
#define __CRLMODULE_ADV7281_CVBS_CONFIGURATION_H_

#include "crlmodule-sensor-ds.h"

#ifdef ADV7282_I2P_CONVERSION
  #define PICTURE_HEIGHT 576
#else
  #define PICTURE_HEIGHT 288
#endif

#define ADV7281_PWD_GPIO_PIN	446
#define ADV7281_RST_GPIO_PIN	462

/* Power GPIOs, they are written in the order they are listed here */
static struct crl_power_seq_entity adv7281_power_items[] = {
	{
		.type = CRL_POWER_ETY_GPIO_CUSTOM,
		.ent_number = ADV7281_PWD_GPIO_PIN,
		.val = 1,
		.undo_val = 0,
		.delay = 5000,
	},
	{
		.type = CRL_POWER_ETY_GPIO_CUSTOM,
		.ent_number = ADV7281_RST_GPIO_PIN,
		.val = 1,
		.undo_val = 0,
		.delay = 5000,
	},
};

static struct crl_register_write_rep adv7281_onetime_init_regset[] = {
};

static struct crl_register_write_rep adv7281_cvbs_powerup_regset[] = {

	{0x0F, CRL_REG_LEN_08BIT, 0x00, 0x00}, /* Exit Power Down Mode */
#ifdef COLORBAR_TEST_SCREEN
	{0x00, CRL_REG_LEN_08BIT, 0x04, 0x00}, /* ADI required write */
	{0x0c, CRL_REG_LEN_08BIT, 0x37, 0x00}, /* Force free-run mode */
	{0x02, CRL_REG_LEN_08BIT, 0x84, 0x00}, /* Force PAL video mode */
	{0x14, CRL_REG_LEN_08BIT, 0x11, 0x00}, /* Set free-run mode to color bars*/
#else
	{0x02, CRL_REG_LEN_08BIT, 0x04, 0x00}, /* Auto detect video mode */

	{0x52, CRL_REG_LEN_08BIT, 0xC0, 0x00}, /* Diff_CVBS AFE IBIAS */
	{0x00, CRL_REG_LEN_08BIT, 0x10, 0x00}, /* INSEL =unconnected input [INSEL Switch] */

	{0x00, CRL_REG_LEN_08BIT, 0x0e, 0x00}, /* INSEL = CVBS_P in on Ain 1, CVBS_N in on Ain2 */
	{0x59, CRL_REG_LEN_08BIT, 0x11, 0x00}, /* Enable diff input, GPO0 = 1. Needed for SEM HW3.0 compatibility */

	{0x0e, CRL_REG_LEN_08BIT, 0x80, 0x00}, /* ADI required write */
	{0x9c, CRL_REG_LEN_08BIT, 0x00, 0x00}, /* Reset Current Clamp Circuitry [step1] */
	{0x9c, CRL_REG_LEN_08BIT, 0xff, 0x00}, /* Reset Current Clamp Circuitry [step2] */
	{0x0e, CRL_REG_LEN_08BIT, 0x00, 0x00}, /* Enter user sub-map 0 */

	{0x5A, CRL_REG_LEN_08BIT, 0x90, 0x00}, /* ADI Required Write [common mode clamp setup] */
	{0x60, CRL_REG_LEN_08BIT, 0xA0, 0x00}, /* ADI Required Write [common mode clamp setup] */

	{0x00, CRL_REG_LEN_DELAY, 0x19, 0x00}, /* Force common mode clamps on for 25 ms*/

	{0x60, CRL_REG_LEN_08BIT, 0xB0, 0x00}, /* ADI Required Writes [common mode clamp setup] */
	{0x5F, CRL_REG_LEN_08BIT, 0xA8, 0x00}, /* SHA gain for Div4 */
	{0x0E, CRL_REG_LEN_08BIT, 0x80, 0x00}, /* ADI Required Writes */
	{0xB6, CRL_REG_LEN_08BIT, 0x08, 0x00}, /* ADI Required Writes [differential CVBS required write] */
	{0xC0, CRL_REG_LEN_08BIT, 0xA0, 0x00}, /* ADI Required Writes [differential CVBS required write] */
	{0x0E, CRL_REG_LEN_08BIT, 0x00, 0x00}, /* Enter User Map */
	{0x0E, CRL_REG_LEN_08BIT, 0x80, 0x00}, /* ADI Required Write [Fast Switch] */
	{0xD9, CRL_REG_LEN_08BIT, 0x44, 0x00}, /* ADI Required Write [Fast Switch] */
	{0x0e, CRL_REG_LEN_08BIT, 0x40, 0x00}, /* Enter user sub-map 2 [Fast Switch]*/
	{0xe0, CRL_REG_LEN_08BIT, 0x01, 0x00}, /* Enable Fast Switch Mode [Fast Switch] */
	{0x0e, CRL_REG_LEN_08BIT, 0x00, 0x00}, /* Select User Map [Fast Switch] */
	{0x80, CRL_REG_LEN_08BIT, 0x51, 0x00}, /* ADI Required Write */
	{0x81, CRL_REG_LEN_08BIT, 0x51, 0x00}, /* ADI Required Write */
	{0x82, CRL_REG_LEN_08BIT, 0x68, 0x00}, /* ADI Required Write */

#endif
	{0x17, CRL_REG_LEN_08BIT, 0x41, 0x00}, /* Enable SH1 */
	{0x03, CRL_REG_LEN_08BIT, 0x4E, 0x00}, /* Power down unused pads */
	{0x04, CRL_REG_LEN_08BIT, 0x57, 0x00}, /* Enable INTRQ pin */
	{0x13, CRL_REG_LEN_08BIT, 0x00, 0x00}, /* Enable ADV7281M for 28_63636MHz crystal */


#ifdef ADV7282_I2P_CONVERSION
	{0xFD, CRL_REG_LEN_08BIT, 0x84, 0x00}, /* Set VPP Map Address [I2P] */
	{0xA3, CRL_REG_LEN_08BIT, 0x00, 0x84}, /* ADI Required Write [I2P] */
	{0x5B, CRL_REG_LEN_08BIT, 0x00, 0x84}, /* Advanced Timing Enabled [I2P] */
	{0x55, CRL_REG_LEN_08BIT, 0x80, 0x84}, /* Enable I2P [I2P] */
	{0xFE, CRL_REG_LEN_08BIT, 0x88, 0x00}, /* Set CSI Map Address */
	{0x01, CRL_REG_LEN_08BIT, 0x20, 0x88}, /* ADI Required Write [I2P] */
	{0x02, CRL_REG_LEN_08BIT, 0x28, 0x88}, /* ADI Required Write [I2P] */
	{0x03, CRL_REG_LEN_08BIT, 0x38, 0x88}, /* ADI Required Write [I2P] */
	{0x04, CRL_REG_LEN_08BIT, 0x30, 0x88}, /* ADI Required Write [I2P] */
	{0x05, CRL_REG_LEN_08BIT, 0x30, 0x88}, /* ADI Required Write [I2P] */
	{0x06, CRL_REG_LEN_08BIT, 0x80, 0x88}, /* ADI Required Write [I2P] */
	{0x07, CRL_REG_LEN_08BIT, 0x70, 0x88}, /* ADI Required Write [I2P] */
	{0x08, CRL_REG_LEN_08BIT, 0x50, 0x88}, /* ADI Required Write [I2P] */
#endif
	{0xfe, CRL_REG_LEN_08BIT, 0x88, 0x00}, /* Enable CSI register map, use i2c addr 44 (88>>2) */
	{0xde, CRL_REG_LEN_08BIT, 0x02, 0x88}, /* Powerup MIPI D-Phy */
	{0xd2, CRL_REG_LEN_08BIT, 0xf7, 0x88}, /* ADI required write */
	{0xd8, CRL_REG_LEN_08BIT, 0x65, 0x88}, /* ADI required write */
	{0xe0, CRL_REG_LEN_08BIT, 0x09, 0x88}, /* ADI required write */
	{0x2c, CRL_REG_LEN_08BIT, 0x00, 0x88}, /* ADI required write */
#ifdef ADV7282_I2P_CONVERSION
	{0x1D, CRL_REG_LEN_08BIT, 0x80, 0x88}, /* ADI Required Write [I2P] */
#endif
};


static struct crl_register_write_rep adv7281_cvbs_streamon_regs[] = {
	{0x00, CRL_REG_LEN_08BIT, 0x00, 0x88}, /* Enable CSI-2 tx output */
};

static struct crl_register_write_rep adv7281_cvbs_streamoff_regs[] = {
	{0x00, CRL_REG_LEN_08BIT, 0x80, 0x88}, /* Disable CSI-2 tx output */
};


static struct crl_pll_configuration adv7281_cvbs_pll_configurations[] = {
	{
		.input_clk = 286363636,
		.op_sys_clk = 216000000,
		.bitsperpixel = 16,
		.pixel_rate_csi = 27000000,
		.pixel_rate_pa = 27000000,
		.csi_lanes = 1,
	 },
	 {
		.input_clk = 24000000,
		.op_sys_clk = 130000000,
		.bitsperpixel = 16,
		.pixel_rate_csi = 130000000,
		.pixel_rate_pa = 130000000,
		.csi_lanes = 1,
	 },
};

static struct crl_subdev_rect_rep adv7281_cvbs_pal_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 720,
		.in_rect.height = PICTURE_HEIGHT,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 720,
		.out_rect.height = PICTURE_HEIGHT,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 720,
		.in_rect.height = PICTURE_HEIGHT,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 720,
		.out_rect.height = PICTURE_HEIGHT,
	},
};

static struct crl_mode_rep adv7281_cvbs_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(adv7281_cvbs_pal_rects),
		.sd_rects = adv7281_cvbs_pal_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 720,
		.height = PICTURE_HEIGHT,
	},
};

static struct crl_sensor_subdev_config adv7281_cvbs_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "adv7281-cvbs binner",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "adv7281-cvbs pixel array",
	},
};

static struct crl_sensor_limits adv7281_cvbs_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 720,
	.y_addr_max = PICTURE_HEIGHT,
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

static struct crl_csi_data_fmt adv7281_cvbs_crl_csi_data_fmt[] = {
	{
		.code = MEDIA_BUS_FMT_UYVY8_1X16,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 16,
	},
};

static struct crl_v4l2_ctrl adv7281_cvbs_v4l2_ctrls[] = {
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
};

int adv7281_sensor_init(struct i2c_client *);
int adv7281_sensor_cleanup(struct i2c_client *);

static struct crl_sensor_configuration adv7281_cvbs_crl_configuration = {

	/* one time initialization is done by HDMI part */
	.sensor_init = adv7281_sensor_init,
	.sensor_cleanup = adv7281_sensor_cleanup,

	.onetime_init_regs_items = ARRAY_SIZE(adv7281_onetime_init_regset),
	.onetime_init_regs = adv7281_onetime_init_regset,

	.power_items = ARRAY_SIZE(adv7281_power_items),
	.power_entities = adv7281_power_items,

	.powerup_regs_items = ARRAY_SIZE(adv7281_cvbs_powerup_regset),
	.powerup_regs = adv7281_cvbs_powerup_regset,
	.poweroff_regs_items = 0,
	.poweroff_regs = 0,

	.subdev_items = ARRAY_SIZE(adv7281_cvbs_sensor_subdevs),
	.subdevs = adv7281_cvbs_sensor_subdevs,

	.sensor_limits = &adv7281_cvbs_sensor_limits,

	.pll_config_items = ARRAY_SIZE(adv7281_cvbs_pll_configurations),
	.pll_configs = adv7281_cvbs_pll_configurations,

	.modes_items = ARRAY_SIZE(adv7281_cvbs_modes),
	.modes = adv7281_cvbs_modes,

	.streamon_regs_items = ARRAY_SIZE(adv7281_cvbs_streamon_regs),
	.streamon_regs = adv7281_cvbs_streamon_regs,

	.streamoff_regs_items = ARRAY_SIZE(adv7281_cvbs_streamoff_regs),
	.streamoff_regs = adv7281_cvbs_streamoff_regs,

	.v4l2_ctrls_items = ARRAY_SIZE(adv7281_cvbs_v4l2_ctrls),
	.v4l2_ctrl_bank = adv7281_cvbs_v4l2_ctrls,

	.csi_fmts_items = ARRAY_SIZE(adv7281_cvbs_crl_csi_data_fmt),
	.csi_fmts = adv7281_cvbs_crl_csi_data_fmt,

	.addr_len = CRL_ADDR_7BIT,
	.i2c_mutex_in_use = true,
};

#endif  /* __CRLMODULE_ADV7281_CVBS_CONFIGURATION_H_ */
