/* SPDX-License-Identifier: GPL */
/*
 * ds90u-core.h - core i2c driver for TI (formerly National Semiconductor)
 * DS90Ux9xx FPD-Link III serializer/deserializer ICs.
 *
 * Copyright (C) 2014-2017 Delphi Technologies, Inc., All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef _LINUX_DS90U_CORE_H_
#define _LINUX_DS90U_CORE_H_

#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/i2c/ds90u.h>

#ifdef CONFIG_SERDES_TIDS90U_DRM_BRIDGE
#include <drm/drm_bridge.h>
#endif
/**
 * enum ds90u_node_type - Indicates the register map and device type
 * @DS90U_SER0    :	Indicates a generic FPD-Link III serializer
 * @DS90U_SER925  :	Register Set for the 925 serializer
 * @DS90U_DESER0  :	Indicates a generic FPD-Link III deserializer
 * @DS90U_DESER1  :	Used for the 948 deserializer (probably for all 94x/98x, but I am still reviewing)
 * @DS90U_DESER926:	Register Set for the 926 deserializer
 * @DS90U_MAX_TYPE:	Keeps the count of items in the enum. Also used for
 *			the auto-detect feature.
 *			Keep this item last.
 */
enum ds90u_node_type {
	DS90U_SER0,
	DS90U_SER925,
	DS90U_DESER0,
	DS90U_DESER1,
	DS90U_DESER926,

	DS90U_MAX_TYPE /*keep last*/
};

/**
 * struct ds90u_cdata - per-client data needed by the driver
 * @client     :	Link back to the i2c_client struct
 * @type       :	Type that indicates the register map and device type
 * @serializer :	Bool indicating whether this is a serializer or not
 * @link_status:	Bool indicating whther the link is currently established
 * @poll_task  :	workqueue task to periodically check the serializer status
 * @pdb_gpio   :	GPIO pin for toggling power-down mode
 * @lock_gpio  :	GPIO pin for lock detection (LOCK or INTB)
 * @scl_pulse_width:	Width for high and low SCL pulse timing (see datasheet)
 * @lut_file_name  :	File name for white-balance LUT
 * @gc             :	gpio controller interface
 * @gpio_base      :	Base gpio number for the controller to start at
 * @gpio_controller:	Create a GPIO controller for i2c-to-gpio bridge
 * @video_18_bit   :	Use 18-bit video mode
 * @i2c_promiscuous_mode:	Enable I2C Passthrough-All
 * @i2c_adapter    :	Create an I2C adapter for child drivers to use
 * @adap           :	I2C adapter interface
 * @i2c_alias_init :	Slave address alias mappings
 * @gpio_init      :	Initial values for GPIO pinmux registers
 * @mapsel_override:	Override mapsel setting - only certain chips have this
 * @mapsel_value   :	Value to use when overriding mapsel setting
 * @oen_override   :	Override OEN setting
 * @oen_value      :	Value to use when overriding OEN setting
 * @oss_value      :	Value to use when overriding OEN/OSS setting
 * @apply_AVMUTE_errata :
 *                      AVMUTE errata, keep deserializer from accidently
 *                      entering AVMUTE mode
 * @disable_clock_autodetect :
 *                      Disable clock detection
 * @apply_temp_ramp_errata :
 *                      Apply errata for display issue when the ambient
 *                      temperature around serializer changes by more than
 *                      60degC. This errata is applicable for DS90U_SER929.
 * @verify_hdmi_clock : verify HDMI clock stability before init B sequence
 *                      for DS90U_SER929.
 * @backchannel_watchdog_value: Timeout value in units of 2 milliseconds of
 *                      backchannel watchdog. Set it to:
 *                       - 0x01-0x7F: Valid value for watchdog timeout.
 *                       - 0xFF     : Disable the watchdog.
 *                       - Other    : Use the default setting of watchdog.
 * @apply_resume_delay : to let lvds parameter change settle under resume
 * @test_deserializer_mailbox : if set compare to deserializer mailbox0
 * @lvds_vod_value :	Value to set FPD/OLDI output voltage swing
 */
struct ds90u_cdata {
	struct i2c_client *client;
	enum ds90u_node_type type;
	int serializer;
	int link_status;
	struct delayed_work poll_task;
	struct delayed_work resume_delayed_worker;
	int pdb_gpio;
	int lock_gpio;
	int scl_pulse_width;
	const char *lut_file_name;
	struct gpio_chip gc;
	int  gpio_base;
	bool gpio_controller;
	bool video_18_bit;
	bool i2c_promiscuous_mode;
	bool i2c_adapter;
	struct i2c_algorithm algo_parent;
	struct i2c_algorithm algo;
	struct i2c_adapter adap;
	struct ds90u_alias_map i2c_alias_init[DS90U_NUM_SLAVE_ALIAS];
	uint8_t gpio_init[DS90U_MAX_NUM_GPIO];
	uint8_t gpio_deinit[DS90U_MAX_NUM_GPIO];
	bool mapsel_override;
	bool mapsel_value;
	bool oen_override;
	bool oen_value;
	bool oss_value;
	bool apply_AVMUTE_errata;
	bool disable_clock_autodetect;
	bool apply_temp_ramp_errata;
	bool verify_hdmi_clock;
	uint32_t backchannel_watchdog_value;
	uint32_t apply_resume_delay;
	int hdmi_clock_retry;
	uint32_t test_deserializer_mailbox;
	int lvds_vod_value;
#ifdef CONFIG_SERDES_TIDS90U_DRM_BRIDGE
	struct drm_bridge drm_bridge;
#endif
};

#endif /* _LINUX_DS90U_CORE_H_ */
