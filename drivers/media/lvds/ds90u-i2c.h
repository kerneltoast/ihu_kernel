/* SPDX-License-Identifier: GPL */
/*
 * ds90u-i2c.h - i2c interface for TI (formerly National Semiconductor)
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
#ifndef _LINUX_DS90U_I2C_H_
#define _LINUX_DS90U_I2C_H_

#include <linux/i2c.h>

/**
 * enum ds90u_config_names - List of mapped registers configurations
 * These aren't necessarily mapped 1-to-1 with registers. Some config items
 * are in register x on one device and register y on a different device.
 * @CFG_DEVICE_ID        :	I2C Device ID register
 * @CFG_I2C_PASSTHROUGH  :	I2C Pass-Through Mode bit
 * @CFG_I2C_PROMISCUOUS  :	I2C Control Pass All bit
 * @CFG_I2C_SLAVE_ID0    :	I2C Slave ID 0 register
 * @CFG_I2C_SLAVE_ID1    :	I2C Slave ID 1 register
 * @CFG_I2C_SLAVE_ID2    :	I2C Slave ID 2 register
 * @CFG_I2C_SLAVE_ID3    :	I2C Slave ID 3 register
 * @CFG_I2C_SLAVE_ID4    :	I2C Slave ID 4 register
 * @CFG_I2C_SLAVE_ID5    :	I2C Slave ID 5 register
 * @CFG_I2C_SLAVE_ID6    :	I2C Slave ID 6 register
 * @CFG_I2C_SLAVE_ID7    :	I2C Slave ID 7 register
 * @CFG_I2C_SLAVE_ALIAS0 :	I2C Slave Alias 0 register
 * @CFG_I2C_SLAVE_ALIAS1 :	I2C Slave Alias 1 register
 * @CFG_I2C_SLAVE_ALIAS2 :	I2C Slave Alias 2 register
 * @CFG_I2C_SLAVE_ALIAS3 :	I2C Slave Alias 3 register
 * @CFG_I2C_SLAVE_ALIAS4 :	I2C Slave Alias 4 register
 * @CFG_I2C_SLAVE_ALIAS5 :	I2C Slave Alias 5 register
 * @CFG_I2C_SLAVE_ALIAS6 :	I2C Slave Alias 6 register
 * @CFG_I2C_SLAVE_ALIAS7 :	I2C Slave Alias 7 register
 * @CFG_GPIO0_CONFIG     :	GPIO 0 Configuration register
 * @CFG_GPIO1_CONFIG     :	GPIO 1 Configuration register
 * @CFG_GPIO2_CONFIG     :	GPIO 2 Configuration register
 * @CFG_GPIO3_CONFIG     :	GPIO 3 Configuration register
 * @CFG_REG_GPIO4_CONFIG :	GPIO REG_4 Configuration register
 * @CFG_REG_GPIO5_CONFIG :	GPIO REG_5 Configuration register
 * @CFG_REG_GPIO6_CONFIG :	GPIO REG_6 Configuration register
 * @CFG_REG_GPIO7_CONFIG :	GPIO REG_7 Configuration register
 * @CFG_REG_GPIO8_CONFIG :	GPIO REG_8 Configuration register
 * @CFG_REG_GPI0_STATUS  :	GPI REG_0 Status register
 * @CFG_REG_GPI1_STATUS  :	GPI REG_0 Status register
 * @CFG_REG_GPI2_STATUS  :	GPI REG_0 Status register
 * @CFG_REG_GPI3_STATUS  :	GPI REG_0 Status register
 * @CFG_REG_GPI4_STATUS  :	GPI REG_0 Status register
 * @CFG_REG_GPI5_STATUS  :	GPI REG_0 Status register
 * @CFG_REG_GPI6_STATUS  :	GPI REG_0 Status register
 * @CFG_REG_GPI7_STATUS  :	GPI REG_0 Status register
 * @CFG_REG_GPI8_STATUS  :	GPI REG_0 Status register
 * @CFG_PCLK_EDGE        :	Pixel Clock Edge Select bit
 * @CFG_LINK_STATUS      :	FPD-Link lock status
 * @CFG_SCL_HIGH_TIME    :	SCL High Time register
 * @CFG_SCL_LOW_TIME     :	SCL Low Time register
 * @CFG_ICR_ENABLE       :	Interrupt enable bit
 * @CFG_ICR_RX_DETECT    :	RX Detect interrupt config bit
 * @CFG_ISR              :	The Interrupt Status Register
 * @CFG_18BIT_VID        :	Enable 18 bit Video mode
 * @CFG_DE_GATE_RGB      :	Gate RGB during DE blanking
 * @CFG_PASS_RGB         :	Pass RGB independent of DE blanking
 * @CFG_CLOCK_DETECT     :	Disable clock detection
 * @CFG_BCC_WD_VALUE     :	BCC Watchdog Timeout Value
 * @CFG_BCC_WD_DISABLE   :	Disable BCC Watchdog
 * @CFG_MAILBOX0         :	present in DS90U_DESER0 only, scratchpad register
 * @CFG_BLK_I2S_AUTO     :	Block I2S Autoconfig
 * @CFG_I2S_DISABLE      :	Disable I2S
 * @CFG_LVDS_VOD_CONTROL :	Set FPD/OLDI output voltage swing
 * @CFG_DUAL_RX_LOCK_MODE:	Configure lock detection mode on dual-lane dev
 * @CFG_MAX_CONFIG       :	Keeps the count of items in the enum.
 *			Keep this item last.
 */
enum ds90u_config_names {
	CFG_DEVICE_ID,
	CFG_I2C_PASSTHROUGH,
	CFG_I2C_PROMISCUOUS,
	CFG_I2C_SLAVE_ID0, /* keep all CFG_I2C_SLAVE_IDxs contiguous */
	CFG_I2C_SLAVE_ID1, /* keep all CFG_I2C_SLAVE_IDxs contiguous */
	CFG_I2C_SLAVE_ID2, /* keep all CFG_I2C_SLAVE_IDxs contiguous */
	CFG_I2C_SLAVE_ID3, /* keep all CFG_I2C_SLAVE_IDxs contiguous */
	CFG_I2C_SLAVE_ID4, /* keep all CFG_I2C_SLAVE_IDxs contiguous */
	CFG_I2C_SLAVE_ID5, /* keep all CFG_I2C_SLAVE_IDxs contiguous */
	CFG_I2C_SLAVE_ID6, /* keep all CFG_I2C_SLAVE_IDxs contiguous */
	CFG_I2C_SLAVE_ID7, /* keep all CFG_I2C_SLAVE_IDxs contiguous */
	CFG_I2C_SLAVE_ALIAS0, /* keep all CFG_I2C_SLAVE_ALIASxs contiguous */
	CFG_I2C_SLAVE_ALIAS1, /* keep all CFG_I2C_SLAVE_ALIASxs contiguous */
	CFG_I2C_SLAVE_ALIAS2, /* keep all CFG_I2C_SLAVE_ALIASxs contiguous */
	CFG_I2C_SLAVE_ALIAS3, /* keep all CFG_I2C_SLAVE_ALIASxs contiguous */
	CFG_I2C_SLAVE_ALIAS4, /* keep all CFG_I2C_SLAVE_ALIASxs contiguous */
	CFG_I2C_SLAVE_ALIAS5, /* keep all CFG_I2C_SLAVE_ALIASxs contiguous */
	CFG_I2C_SLAVE_ALIAS6, /* keep all CFG_I2C_SLAVE_ALIASxs contiguous */
	CFG_I2C_SLAVE_ALIAS7, /* keep all CFG_I2C_SLAVE_ALIASxs contiguous */
	CFG_GPIO0_CONFIG,     /* --------------------------- */
	CFG_GPIO1_CONFIG,     /*             ^               */
	CFG_GPIO2_CONFIG,     /*             |               */
	CFG_GPIO3_CONFIG,     /* keep all 9 CFG_GPIOx_CONFIG */
	CFG_REG_GPIO4_CONFIG, /* and CFG_REG_GPIOx_CONFIG    */
	CFG_REG_GPIO5_CONFIG, /* entries contiguous          */
	CFG_REG_GPIO6_CONFIG, /*             |               */
	CFG_REG_GPIO7_CONFIG, /*             v               */
	CFG_REG_GPIO8_CONFIG, /* --------------------------- */
	CFG_REG_GPI0_STATUS, /* keep all CFG_REG_GPIx_STATUSs contiguous */
	CFG_REG_GPI1_STATUS, /* keep all CFG_REG_GPIx_STATUSs contiguous */
	CFG_REG_GPI2_STATUS, /* keep all CFG_REG_GPIx_STATUSs contiguous */
	CFG_REG_GPI3_STATUS, /* keep all CFG_REG_GPIx_STATUSs contiguous */
	CFG_REG_GPI4_STATUS, /* keep all CFG_REG_GPIx_STATUSs contiguous */
	CFG_REG_GPI5_STATUS, /* keep all CFG_REG_GPIx_STATUSs contiguous */
	CFG_REG_GPI6_STATUS, /* keep all CFG_REG_GPIx_STATUSs contiguous */
	CFG_REG_GPI7_STATUS, /* keep all CFG_REG_GPIx_STATUSs contiguous */
	CFG_REG_GPI8_STATUS, /* keep all CFG_REG_GPIx_STATUSs contiguous */
	CFG_REG_PCLK_EDGE,
	CFG_LINK_STATUS,
	CFG_SCL_HIGH_TIME,
	CFG_SCL_LOW_TIME,
	CFG_ICR_ENABLE,
	CFG_ICR_RX_DETECT,
	CFG_ISR,
	CFG_18BIT_VID,
	CFG_WBAL_PAGE,
	CFG_WBAL_EN,
	CFG_WBAL_RELOAD,
	CFG_MODE_CONTROL, /* has different settings depending on model */
	CFG_MAPSEL_OVR, /* mapsel override, sometimes shared with mode */
	CFG_OEN_OVR, /* OEN and OSS override, 914 shares with mode control */
	CFG_DE_GATE_RGB,
	CFG_PASS_RGB,
	CFG_CLOCK_DETECT,
	CFG_BCC_WD_VALUE,
	CFG_BCC_WD_DISABLE,
	CFG_MAILBOX0,
	CFG_BLK_I2S_AUTO,
	CFG_I2S_DISABLE,
	CFG_LVDS_VOD_CONTROL,
	CFG_DUAL_RX_LOCK_MODE,
	CFG_RESET_CTL_REG,
	CFG_BRIDGE_CTL_REG,
	CFG_IND_ACC_CTL_REG,
	CFG_IND_ACC_ADDR_REG,
	CFG_IND_ACC_DATA_REG,
	CFG_DUAL_CTL1_REG,
	CFG_GENERAL_CFG2_REG,
	CFG_DEVICE_CFG_REG,
	CFG_BIST_PIN_CONFIG,
	CFG_MAX_CONFIG /*keep last*/
};

int ds90u_config_read(struct i2c_client *client,
		      enum ds90u_config_names config);
int ds90u_config_write(struct i2c_client *client,
		       enum ds90u_config_names config, uint8_t value);
int ds90u_config_set(struct i2c_client *client, enum ds90u_config_names config);
int ds90u_config_clear(struct i2c_client *client,
		       enum ds90u_config_names config);

extern struct i2c_driver tftenable_driver;

#endif /* _LINUX_DS90U_I2C_H_ */
