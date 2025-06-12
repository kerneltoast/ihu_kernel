/*
 * ds90u.h - platform data for TI (formerly National Semiconductor)
 * DS90Ux9xx FPD-Link III serializer/deserializer ICs.
 *
 * Copyright (C) 2017 Delphi Technologies, Inc., All Rights Reserved.
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

#ifndef __LINUX_PLATFORM_DATA_DS90U_H
#define __LINUX_PLATFORM_DATA_DS90U_H

#include <linux/types.h>
#include <linux/i2c/ds90u.h>

/**
 * struct ds90u_platform_data - platform data used to configure the ds90u driver
 * @pdb_gpio       :	GPIO pin for toggling power-down mode
 * @lock_gpio      :	GPIO pin for lock detection (LOCK or INTB)
 * @scl_pulse_width:	Width for high and low SCL pulse timing (see datasheet)
 * @lut_file_name  :	File name for white-balance LUT
 * @gpio_base      :	Base gpio number for the controller to start at
 * @gpio_controller:	Create a GPIO controller for i2c-to-gpio bridge
 * @video_18_bit   :	Use 18-bit video mode
 * @i2c_promiscuous_mode:	Enable I2C Passthrough-All
 * @mapsel_override:	Override mapsel setting - only certain chips have this
 * @mapsel_value   :	Value to use when overriding mapsel setting
 * @oen_override   :	Override OEN/OSS setting
 * @oen_value      :	Value to use when overriding OEN setting
 * @oss_value      :	Value to use when overriding OEN/OSS setting
 * @i2c_adapter    :	Create an I2C adapter for child drivers to use
 * @i2c_adapter_num:	Number to use for I2C adapter ID (-1 for auto)
 * @i2c_alias_init :	Slave address alias mappings
 * @gpio_init      :	Initial values for GPIO pinmux registers
 * @apply_AVMUTE_errata :
 *                      AVMUTE errata, keep deserializer from accidently
 *                      entering AVMUTE mode
 * @disable_clock_autodetect :
 *                      Disable clock detection
 * @apply_temp_ramp_errata :
 *                      Apply errata for display issue when the ambient
 *                      temperature around serializer changes by more than
 *                      60degC. This errata is applicable for DS90U_SER929.
 * @backchannel_watchdog_value: Timeout value in units of 2 milliseconds of
 *                      backchannel watchdog. Set it to:
 *                       - 0x01-0x7F: Valid value for watchdog timeout.
 *                       - 0xFF     : Disable the watchdog.
 *                       - Other    : Use the default setting of watchdog.
 * @apply_resume_delay : to let lvds parameter change settle under resume
 * @test_deserializer_mailbox : if set use test mailbox to detect reconnect
 * @lvds_vod_value :	Value for the FPD/OLDI Output Voltage swing setting
 */
struct ds90u_platform_data {
	int  pdb_gpio;
	int  lock_gpio;
	int  scl_pulse_width;
	const char* lut_file_name;
	int  gpio_base;
	bool gpio_controller;
	bool video_18_bit;
	bool i2c_promiscuous_mode;
	bool mapsel_override;
	bool mapsel_value;
	bool oen_override;
	bool oen_value;
	bool oss_value;
	bool i2c_adapter;
	int i2c_adapter_num;
	struct ds90u_alias_map i2c_alias_init[DS90U_NUM_SLAVE_ALIAS];
	uint8_t gpio_init[DS90U_MAX_NUM_GPIO];
	uint8_t gpio_deinit[DS90U_MAX_NUM_GPIO];
	bool apply_AVMUTE_errata;
	bool disable_clock_autodetect;
	bool apply_temp_ramp_errata;
	bool verify_hdmi_clock;
	uint32_t backchannel_watchdog_value;
	uint32_t apply_resume_delay;
	uint32_t test_deserializer_mailbox;
	int lvds_vod_value;
};

#endif /* __LINUX_PLATFORM_DATA_DS90U_H */
