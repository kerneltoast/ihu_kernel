// SPDX-License-Identifier: GPL
/*
 * ds90u-i2c.c - i2c driver for TI (formerly National Semiconductor)
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

/**
 * DOC: ds90u-i2c
 *
 * This module is intended to handle i2c communication with TI (formerly
 * National Semiconductor) FPD-Link III ICs.
 *
 * This modules abstracts the I2C read/write functions so that other modules
 * can access functionality by register name/field.
 *
 * I believe that all part numbers beginning with ds90u are i2c-compatible,
 * for the most part. But, there may be exceptions that I haven't
 * encountered.
 * Double check the register definitions when adding a compatibility with a new
 * chip revision.
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include "ds90u-core.h" /* TODO: currently only needed for the DS90U_MAX_TYPE and cdata.type field */
#include "ds90u-i2c.h"

/* TODO: I really want to revisit this strategy since it doesn't scale well */

/* Register mapping for ds90u_node_type/ds90u_config_names */
static const uint8_t ds90u_config_reg[CFG_MAX_CONFIG][DS90U_MAX_TYPE] = {
/*                           DS90U_SER0, DS90U_SER925, DS90U_DESER0, DS90U_DESER1, DS90U_DESER926 */
/* CFG_DEVICE_ID         */ {      0x00,         0x00,         0x00,         0x00,           0x00},
/* CFG_I2C_PASSTHROUGH   */ {      0x03,         0x03,         0x03,         0x03,           0x03},
/* CFG_I2C_PROMISCUOUS   */ {      0x17,         0x17,         0x05,         0x05,           0x05},
/* CFG_I2C_SLAVE_ID0     */ {      0x07,         0x07,         0x08,         0x08,           0x08},
/* CFG_I2C_SLAVE_ID1     */ {      0x70,         0xFF,         0x09,         0x09,           0x09},
/* CFG_I2C_SLAVE_ID2     */ {      0x71,         0xFF,         0x0A,         0x0A,           0x0A},
/* CFG_I2C_SLAVE_ID3     */ {      0x72,         0xFF,         0x0B,         0x0B,           0x0B},
/* CFG_I2C_SLAVE_ID4     */ {      0x73,         0xFF,         0x0C,         0x0C,           0x0C},
/* CFG_I2C_SLAVE_ID5     */ {      0x74,         0xFF,         0x0D,         0x0D,           0x0D},
/* CFG_I2C_SLAVE_ID6     */ {      0x75,         0xFF,         0x0E,         0x0E,           0x0E},
/* CFG_I2C_SLAVE_ID7     */ {      0x76,         0xFF,         0x0F,         0x0F,           0x0F},
/* CFG_I2C_SLAVE_ALIAS0  */ {      0x08,         0x08,         0x10,         0x10,           0x10},
/* CFG_I2C_SLAVE_ALIAS1  */ {      0x77,         0xFF,         0x11,         0x11,           0x11},
/* CFG_I2C_SLAVE_ALIAS2  */ {      0x78,         0xFF,         0x12,         0x12,           0x12},
/* CFG_I2C_SLAVE_ALIAS3  */ {      0x79,         0xFF,         0x13,         0x13,           0x13},
/* CFG_I2C_SLAVE_ALIAS4  */ {      0x7A,         0xFF,         0x14,         0x14,           0x14},
/* CFG_I2C_SLAVE_ALIAS5  */ {      0x7B,         0xFF,         0x15,         0x15,           0x15},
/* CFG_I2C_SLAVE_ALIAS6  */ {      0x7C,         0xFF,         0x16,         0x16,           0x16},
/* CFG_I2C_SLAVE_ALIAS7  */ {      0x7D,         0xFF,         0x17,         0x17,           0x17},
/* CFG_GPIO0_CONFIG      */ {      0x0D,         0x0D,         0x1D,         0x1D,           0x1D},
/* CFG_GPIO1_CONFIG      */ {      0x0E,         0x0E,         0x1E,         0x1E,           0x1E},
/* CFG_GPIO2_CONFIG      */ {      0x0E,         0x0E,         0x1E,         0x1E,           0x1E},
/* CFG_GPIO3_CONFIG      */ {      0x0F,         0x0F,         0x1F,         0x1F,           0x1F},
/* CFG_REG_GPIO4_CONFIG  */ {      0xFF,         0x0F,         0xFF,         0xFF,           0x1F},
/* CFG_REG_GPIO5_CONFIG  */ {      0x10,         0x10,         0x20,         0x20,           0x20},
/* CFG_REG_GPIO6_CONFIG  */ {      0x10,         0x10,         0x20,         0x20,           0x20},
/* CFG_REG_GPIO7_CONFIG  */ {      0x11,         0x11,         0x21,         0x21,           0x21},
/* CFG_REG_GPIO8_CONFIG  */ {      0x11,         0x11,         0x21,         0x21,           0x21},
/* CFG_REG_GPI0_STATUS   */ {      0x1C,         0xFF,         0x6E,         0x6E,           0xFF},
/* CFG_REG_GPI1_STATUS   */ {      0x1C,         0xFF,         0x6E,         0x6E,           0xFF},
/* CFG_REG_GPI2_STATUS   */ {      0x1C,         0xFF,         0x6E,         0x6E,           0xFF},
/* CFG_REG_GPI3_STATUS   */ {      0x1C,         0xFF,         0x6E,         0x6E,           0xFF},
/* CFG_REG_GPI4_STATUS   */ {      0xFF,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_REG_GPI5_STATUS   */ {      0x1C,         0xFF,         0x6E,         0x6E,           0xFF},
/* CFG_REG_GPI6_STATUS   */ {      0x1C,         0xFF,         0x6E,         0x6E,           0xFF},
/* CFG_REG_GPI7_STATUS   */ {      0x1C,         0xFF,         0x6E,         0x6E,           0xFF},
/* CFG_REG_GPI8_STATUS   */ {      0x1D,         0xFF,         0x6F,         0x6F,           0xFF},
/* CFG_PCLK_EDGE         */ {      0x03,         0x03,         0x03,         0x03,           0x03},
/* CFG_LINK_STATUS       */ {      0x0C,         0x0C,         0x1C,         0x1C,           0x1C},
/* CFG_SCL_HIGH_TIME     */ {      0x18,         0x18,         0x26,         0x26,           0x26},
/* CFG_SCL_LOW_TIME      */ {      0x19,         0x19,         0x27,         0x27,           0x27},
/* CFG_ICR_ENABLE        */ {      0xC6,         0xC6,         0xFF,         0xFF,           0xFF},
/* CFG_ICR_RX_DETECT     */ {      0xC6,         0xC6,         0xFF,         0xFF,           0xFF},
/* CFG_ISR               */ {      0xC7,         0xC7,         0xFF,         0xFF,           0xFF},
/* CFG_18BIT_VID         */ {      0x12,         0x12,         0xFF,         0xFF,           0xFF},
/* CFG_WBAL_PAGE         */ {      0xFF,         0xFF,         0x2A,         0x2A,           0x2A},
/* CFG_WBAL_EN           */ {      0xFF,         0xFF,         0x2A,         0x2A,           0x2A},
/* CFG_WBAL_RELOAD       */ {      0xFF,         0xFF,         0x2A,         0x2A,           0x2A},
/* CFG_MODE_CONTROL      */ {      0x13,         0x13,         0x23,         0x23,           0x23}, /* TODO: 914 is 0x1f */
/* CFG_MAPSEL_OVR        */ {      0x13,         0xFF,         0x49,         0x49,           0xFF},
/* CFG_OEN_OVR           */ {      0xFF,         0xFF,         0x02,         0x02,           0x02}, /* TODO: 914 is 0x1f */
/* CFG_DE_GATE_RGB       */ {      0x04,         0x04,         0xFF,         0xFF,           0xFF},
/* CFG_PASS_RGB          */ {      0x12,         0x12,         0x22,         0x22,           0x22},
/* CFG_CLOCK_DETECT      */ {      0x5B,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_BCC_WD_VALUE      */ {      0x16,         0x16,         0x04,         0x04,           0x04},
/* CFG_BCC_WD_DISABLE    */ {      0x16,         0x16,         0x04,         0x04,           0x04},
/* CFG_MAILBOX0          */ {      0x00,         0x00,         0x18,         0x18,           0x00},
/* CFG_BLK_I2S_AUTO      */ {      0xFF,         0xFF,         0x28,         0xFF,           0xFF},
/* CFG_I2S_DISABLE       */ {      0xFF,         0xFF,         0x28,         0x28,           0xFF},
/* CFG_LVDS_VOD_CONTROL  */ {      0xFF,         0xFF,         0x4B,         0x4B,           0xFF},
/* CFG_DUAL_RX_LOCK_MODE */ {      0xFF,         0xFF,         0xFF,         0x34,           0xFF},
/* CFG_RESET_CTL_REG     */ {      0x01,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_BRIDGE_CTL_REG    */ {      0x4f,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_IND_ACC_CTL_REG   */ {      0x40,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_IND_ACC_ADDR_REG  */ {      0x41,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_IND_ACC_DATA_REG  */ {      0x42,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_DUAL_CTL1_REG     */ {      0x5b,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_GENERAL_CFG2_REG  */ {      0x04,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_DEVICE_CFG_REG    */ {      0x02,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_BIST_PIN_CONFIG   */ {      0xFF,         0xFF,         0x24,         0x24,           0x24},
};

/* Bit-mask mapping for ds90u_node_type/ds90u_config_names */
static const uint8_t ds90u_config_mask[CFG_MAX_CONFIG][DS90U_MAX_TYPE] = {
/*                           DS90U_SER0, DS90U_SER925, DS90U_DESER0, DS90U_DESER1, DS90U_DESER926 */
/* CFG_DEVICE_ID         */ {      0xFE,         0xFE,         0xFE,         0xFE,           0xFE},
/* CFG_I2C_PASSTHROUGH   */ {      0x08,         0x08,         0x08,         0x08,           0x08},
/* CFG_I2C_PROMISCUOUS   */ {      0x80,         0x80,         0x80,         0x80,           0x80},
/* CFG_I2C_SLAVE_ID0     */ {      0xFE,         0xFE,         0xFE,         0xFE,           0xFE},
/* CFG_I2C_SLAVE_ID1     */ {      0xFE,         0x00,         0xFE,         0xFE,           0xFE},
/* CFG_I2C_SLAVE_ID2     */ {      0xFE,         0x00,         0xFE,         0xFE,           0xFE},
/* CFG_I2C_SLAVE_ID3     */ {      0xFE,         0x00,         0xFE,         0xFE,           0xFE},
/* CFG_I2C_SLAVE_ID4     */ {      0xFE,         0x00,         0xFE,         0xFE,           0xFE},
/* CFG_I2C_SLAVE_ID5     */ {      0xFE,         0x00,         0xFE,         0xFE,           0xFE},
/* CFG_I2C_SLAVE_ID6     */ {      0xFE,         0x00,         0xFE,         0xFE,           0xFE},
/* CFG_I2C_SLAVE_ID7     */ {      0xFE,         0x00,         0xFE,         0xFE,           0xFE},
/* CFG_I2C_SLAVE_ALIAS0  */ {      0xFE,         0xFE,         0xFE,         0xFE,           0xFE},
/* CFG_I2C_SLAVE_ALIAS1  */ {      0xFE,         0x00,         0xFE,         0xFE,           0xFE},
/* CFG_I2C_SLAVE_ALIAS2  */ {      0xFE,         0x00,         0xFE,         0xFE,           0xFE},
/* CFG_I2C_SLAVE_ALIAS3  */ {      0xFE,         0x00,         0xFE,         0xFE,           0xFE},
/* CFG_I2C_SLAVE_ALIAS4  */ {      0xFE,         0x00,         0xFE,         0xFE,           0xFE},
/* CFG_I2C_SLAVE_ALIAS5  */ {      0xFE,         0x00,         0xFE,         0xFE,           0xFE},
/* CFG_I2C_SLAVE_ALIAS6  */ {      0xFE,         0x00,         0xFE,         0xFE,           0xFE},
/* CFG_I2C_SLAVE_ALIAS7  */ {      0xFE,         0x00,         0xFE,         0xFE,           0xFE},
/* CFG_GPIO0_CONFIG      */ {      0x0F,         0x0F,         0x0F,         0x0F,           0x0F},
/* CFG_GPIO1_CONFIG      */ {      0x0F,         0x0F,         0x0F,         0x0F,           0x0F},
/* CFG_GPIO2_CONFIG      */ {      0xF0,         0xF0,         0xF0,         0xF0,           0xF0},
/* CFG_GPIO3_CONFIG      */ {      0x0F,         0x0F,         0x0F,         0x0F,           0x0F},
/* CFG_REG_GPIO4_CONFIG  */ {      0x00,         0xF0,         0x00,         0x00,           0xF0},
/* CFG_REG_GPIO5_CONFIG  */ {      0x0F,         0x0F,         0x0F,         0x0F,           0x0F},
/* CFG_REG_GPIO6_CONFIG  */ {      0xF0,         0xF0,         0xF0,         0xF0,           0xF0},
/* CFG_REG_GPIO7_CONFIG  */ {      0x0F,         0x0F,         0x0F,         0x0F,           0x0F},
/* CFG_REG_GPIO8_CONFIG  */ {      0xF0,         0xF0,         0xF0,         0xF0,           0xF0},
/* CFG_REG_GPI0_STATUS   */ {      0x01,         0x00,         0x01,         0x01,           0x00},
/* CFG_REG_GPI1_STATUS   */ {      0x02,         0x00,         0x02,         0x02,           0x00},
/* CFG_REG_GPI2_STATUS   */ {      0x04,         0x00,         0x04,         0x04,           0x00},
/* CFG_REG_GPI3_STATUS   */ {      0x08,         0x00,         0x08,         0x08,           0x00},
/* CFG_REG_GPI4_STATUS   */ {      0x00,         0x00,         0x00,         0x00,           0x00},
/* CFG_REG_GPI5_STATUS   */ {      0x20,         0x00,         0x20,         0x20,           0x00},
/* CFG_REG_GPI6_STATUS   */ {      0x40,         0x00,         0x40,         0x40,           0x00},
/* CFG_REG_GPI7_STATUS   */ {      0x80,         0x00,         0x80,         0x80,           0x00},
/* CFG_REG_GPI8_STATUS   */ {      0x01,         0x00,         0x01,         0x01,           0x00},
/* CFG_PCLK_EDGE         */ {      0x01,         0x01,         0x01,         0x01,           0x01},
/* CFG_LINK_STATUS       */ {      0x01,         0x01,         0x01,         0x01,           0x01},
/* CFG_SCL_HIGH_TIME     */ {      0xFF,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_SCL_LOW_TIME      */ {      0xFF,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_ICR_ENABLE        */ {      0x01,         0x01,         0x00,         0x00,           0x00},
/* CFG_ICR_RX_DETECT     */ {      0x40,         0x40,         0x00,         0x00,           0x00},
/* CFG_ISR               */ {      0xFF,         0xFF,         0x00,         0x00,           0x00},
/* CFG_18BIT_VID         */ {      0x04,         0x04,         0x00,         0x00,           0x00},
/* CFG_WBAL_PAGE         */ {      0x00,         0x00,         0xC0,         0xC0,           0xC0},
/* CFG_WBAL_EN           */ {      0x00,         0x00,         0x20,         0x20,           0x20},
/* CFG_WBAL_RELOAD       */ {      0x00,         0x00,         0x10,         0x10,           0x10},
/* CFG_MODE_CONTROL      */ {      0xFF,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_MAPSEL_OVR        */ {      0x60,         0x00,         0x60,         0x60,           0x00},
/* CFG_OEN_OVR           */ {      0x00,         0x00,         0xD0,         0xD0,           0xD0}, /* TODO: 914 is 0xE0 */
/* CFG_DE_GATE_RGB       */ {      0x10,         0x10,         0x00,         0x00,           0x00},
/* CFG_PASS_RGB          */ {      0x40,         0x40,         0x40,         0x40,           0x40},
/* CFG_CLOCK_DETECT      */ {      0x20,         0x00,         0x00,         0x00,           0x00},
/* CFG_BCC_WD_VALUE      */ {      0xFE,         0xFE,         0xFE,         0xFE,           0xFE},
/* CFG_BCC_WD_DISABLE    */ {      0x01,         0x01,         0x01,         0x01,           0x01},
/* CFG_MAILBOX0          */ {      0x00,         0x00,         0xff,         0xFF,           0x00},
/* CFG_BLK_I2S_AUTO      */ {      0x00,         0x00,         0x80,         0x00,           0x00},
/* CFG_I2S_DISABLE       */ {      0x00,         0x00,         0x04,         0x04,           0x00},
/* CFG_LVDS_VOD_CONTROL  */ {      0x00,         0x00,         0x03,         0x03,           0x00},
/* CFG_DUAL_RX_LOCK_MODE */ {      0x00,         0x00,         0x00,         0x40,           0x00},
/* CFG_RESET_CTL_REG     */ {      0xFF,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_BRIDGE_CTL_REG    */ {      0xFF,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_IND_ACC_CTL_REG   */ {      0xFF,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_IND_ACC_ADDR_REG  */ {      0xFF,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_IND_ACC_DATA_REG  */ {      0xFF,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_DUAL_CTL1_REG     */ {      0xFF,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_GENERAL_CFG2_REG  */ {      0xFF,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_DEVICE_CFG_REG    */ {      0xFF,         0xFF,         0xFF,         0xFF,           0xFF},
/* CFG_BIST_PIN_CONFIG   */ {      0xFF,         0xFF,         0x08,         0x08,           0x08},
};

/* Value-Shift mapping for ds90u_node_type/ds90u_config_names */
static const uint8_t ds90u_config_shift[CFG_MAX_CONFIG][DS90U_MAX_TYPE] = {
/*                           DS90U_SER0, DS90U_SER925, DS90U_DESER0, DS90U_DESER1, DS90U_DESER926 */
/* CFG_DEVICE_ID         */ {         1,            1,            1,            1,              1},
/* CFG_I2C_PASSTHROUGH   */ {         3,            3,            3,            3,              3},
/* CFG_I2C_PROMISCUOUS   */ {         7,            7,            7,            7,              7},
/* CFG_I2C_SLAVE_ID0     */ {         1,            1,            1,            1,              1},
/* CFG_I2C_SLAVE_ID1     */ {         1,            1,            1,            1,              1},
/* CFG_I2C_SLAVE_ID2     */ {         1,            1,            1,            1,              1},
/* CFG_I2C_SLAVE_ID3     */ {         1,            1,            1,            1,              1},
/* CFG_I2C_SLAVE_ID4     */ {         1,            1,            1,            1,              1},
/* CFG_I2C_SLAVE_ID5     */ {         1,            1,            1,            1,              1},
/* CFG_I2C_SLAVE_ID6     */ {         1,            1,            1,            1,              1},
/* CFG_I2C_SLAVE_ID7     */ {         1,            1,            1,            1,              1},
/* CFG_I2C_SLAVE_ALIAS0  */ {         1,            1,            1,            1,              1},
/* CFG_I2C_SLAVE_ALIAS1  */ {         1,            1,            1,            1,              1},
/* CFG_I2C_SLAVE_ALIAS2  */ {         1,            1,            1,            1,              1},
/* CFG_I2C_SLAVE_ALIAS3  */ {         1,            1,            1,            1,              1},
/* CFG_I2C_SLAVE_ALIAS4  */ {         1,            1,            1,            1,              1},
/* CFG_I2C_SLAVE_ALIAS5  */ {         1,            1,            1,            1,              1},
/* CFG_I2C_SLAVE_ALIAS6  */ {         1,            1,            1,            1,              1},
/* CFG_I2C_SLAVE_ALIAS7  */ {         1,            1,            1,            1,              1},
/* CFG_GPIO0_CONFIG      */ {         0,            0,            0,            0,              0},
/* CFG_GPIO1_CONFIG      */ {         0,            0,            0,            0,              0},
/* CFG_GPIO2_CONFIG      */ {         4,            4,            4,            4,              4},
/* CFG_GPIO3_CONFIG      */ {         0,            0,            0,            0,              0},
/* CFG_REG_GPIO4_CONFIG  */ {         4,            4,            4,            4,              4},
/* CFG_REG_GPIO5_CONFIG  */ {         0,            0,            0,            0,              0},
/* CFG_REG_GPIO6_CONFIG  */ {         4,            4,            4,            4,              4},
/* CFG_REG_GPIO7_CONFIG  */ {         0,            0,            0,            0,              0},
/* CFG_REG_GPIO8_CONFIG  */ {         4,            4,            4,            4,              4},
/* CFG_REG_GPI0_STATUS   */ {         0,            0,            0,            0,              0},
/* CFG_REG_GPI1_STATUS   */ {         1,            1,            1,            1,              1},
/* CFG_REG_GPI2_STATUS   */ {         2,            2,            2,            2,              2},
/* CFG_REG_GPI3_STATUS   */ {         3,            3,            3,            3,              3},
/* CFG_REG_GPI4_STATUS   */ {         4,            4,            4,            4,              4},
/* CFG_REG_GPI5_STATUS   */ {         5,            5,            5,            5,              5},
/* CFG_REG_GPI6_STATUS   */ {         6,            6,            6,            6,              6},
/* CFG_REG_GPI7_STATUS   */ {         7,            7,            7,            7,              7},
/* CFG_REG_GPI8_STATUS   */ {         0,            0,            0,            0,              0},
/* CFG_PCLK_EDGE         */ {         0,            0,            0,            0,              0},
/* CFG_LINK_STATUS       */ {         0,            0,            0,            0,              0},
/* CFG_SCL_HIGH_TIME     */ {         0,            0,            0,            0,              0},
/* CFG_SCL_LOW_TIME      */ {         0,            0,            0,            0,              0},
/* CFG_ICR_ENABLE        */ {         0,            0,            0,            0,              0},
/* CFG_ICR_RX_DETECT     */ {         6,            6,            6,            6,              6},
/* CFG_ISR               */ {         0,            0,            0,            0,              0},
/* CFG_18BIT_VID         */ {         2,            2,            0,            0,              0},
/* CFG_WBAL_PAGE         */ {         0,            0,            6,            6,              6},
/* CFG_WBAL_EN           */ {         0,            0,            5,            5,              5},
/* CFG_WBAL_RELOAD       */ {         0,            0,            4,            4,              4},
/* CFG_MODE_CONTROL      */ {         0,            0,            0,            0,              0},
/* CFG_MAPSEL_OVR        */ {         5,            0,            5,            5,              0},
/* CFG_OEN_OVR           */ {         0,            0,            4,            4,              4}, /* TODO: 914 is 5 */
/* CFG_DE_GATE_RGB       */ {         4,            4,            0,            0,              0},
/* CFG_PASS_RGB          */ {         6,            6,            6,            6,              6},
/* CFG_CLOCK_DETECT      */ {         5,            0,            0,            0,              0},
/* CFG_BCC_WD_VALUE      */ {         1,            1,            1,            1,              1},
/* CFG_BCC_WD_DISABLE    */ {         0,            0,            0,            0,              0},
/* CFG_MAILBOX0          */ {         0,            0,            0,            0,              0},
/* CFG_BLK_I2S_AUTO      */ {         0,            0,            7,            0,              0},
/* CFG_I2S_DISABLE       */ {         0,            0,            2,            2,              0},
/* CFG_LVDS_VOD_CONTROL  */ {         0,            0,            0,            0,              0},
/* CFG_DUAL_RX_LOCK_MODE */ {         0,            0,            0,            6,              0},
/* CFG_RESET_CTL_REG     */ {         0,            0,            0,            0,              0},
/* CFG_BRIDGE_CTL_REG    */ {         0,            0,            0,            0,              0},
/* CFG_IND_ACC_CTL_REG   */ {         0,            0,            0,            0,              0},
/* CFG_IND_ACC_ADDR_REG  */ {         0,            0,            0,            0,              0},
/* CFG_IND_ACC_DATA_REG  */ {         0,            0,            0,            0,              0},
/* CFG_DUAL_CTL1_REG     */ {         0,            0,            0,            0,              0},
/* CFG_GENERAL_CFG2_REG  */ {         0,            0,            0,            0,              0},
/* CFG_DEVICE_CFG_REG    */ {         0,            0,            0,            0,              0},
/* CFG_BIST_PIN_CONFIG   */ {         0,            0,            3,            3,              3},
};

/**
 * ds90u_config_read() - config read helper
 * Uses config enum to read device-specific configuration.
 * @client:	device-specific client
 * @config:	config item to read
 */
int ds90u_config_read(struct i2c_client *client, enum ds90u_config_names config)
{
	int ret;
	enum ds90u_node_type type;
	struct ds90u_cdata *cdata;

	cdata = (struct ds90u_cdata *)i2c_get_clientdata(client);
	WARN_ON(cdata == NULL);
	if (cdata == NULL)
		return -ENOMEM;

	type = cdata->type;
	if (type >= DS90U_MAX_TYPE || config >= CFG_MAX_CONFIG)
		return -EINVAL;

	/* abort the read if the register is invalid for this device */
	if (ds90u_config_mask[config][type] == 0x00)
		return -ENOENT;

	ret = i2c_smbus_read_byte_data(client, ds90u_config_reg[config][type]);
	if (ret < 0) {
		dev_err(&client->dev, "read of 0x%02X failed\n", ds90u_config_reg[config][type]);
		return ret;
	}

	ret = ret & ds90u_config_mask[config][type];
	ret = ret >> ds90u_config_shift[config][type];
	return ret;
}

/**
 * ds90u_config_write() - config write helper
 * Uses config enum to write device-specific configuration bits.
 * @client:	device-specific client
 * @config:	config item to write
 * @value :	desired value to write
 */
int ds90u_config_write(struct i2c_client *client,
		       enum ds90u_config_names config, uint8_t value)
{
	int ret = 0;
	enum ds90u_node_type type;
	struct ds90u_cdata *cdata;

	cdata = (struct ds90u_cdata *)i2c_get_clientdata(client);
	WARN_ON(cdata == NULL);
	if (cdata == NULL)
		return -ENOMEM;

	type = cdata->type;
	if (type >= DS90U_MAX_TYPE || config >= CFG_MAX_CONFIG)
		return -EINVAL;

	/* abort the write if the register is invalid for this device */
	if (ds90u_config_mask[config][type] == 0x00)
		return -ENOENT;

	/*
	 * short-circuit the read and update process if we are updating
	 * the whole register
	 */
	if (ds90u_config_mask[config][type] != 0xFF) {
		ret = i2c_smbus_read_byte_data(client,
					       ds90u_config_reg[config][type]);
		if (ret < 0) {
			dev_err(&client->dev, "read of 0x%02X failed\n", ds90u_config_reg[config][type]);
			return ret;
		}

		ret = ret & ~ds90u_config_mask[config][type];
		value = value << ds90u_config_shift[config][type];
		value = value & ds90u_config_mask[config][type];
		value = value | ret;
	}

	ret = i2c_smbus_write_byte_data(client, ds90u_config_reg[config][type], value);
	if (ret < 0)
		dev_err(&client->dev, "write 0x%02X to 0x%02X failed\n", value, ds90u_config_reg[config][type]);
	return ret;
}

/**
 * ds90u_config_set() - config write helper
 * Uses config enum to set device-specific configuration bits.
 * @client:	device-specific client
 * @config:	config item to set
 */
int ds90u_config_set(struct i2c_client *client, enum ds90u_config_names config)
{
	int ret;
	enum ds90u_node_type type;
	struct ds90u_cdata *cdata;
	uint8_t value;

	cdata = (struct ds90u_cdata *)i2c_get_clientdata(client);
	WARN_ON(cdata == NULL);
	if (cdata == NULL)
		return -ENOMEM;

	type = cdata->type;
	if (type >= DS90U_MAX_TYPE || config >= CFG_MAX_CONFIG)
		return -EINVAL;

	/* abort the write if the register is invalid for this device */
	if (ds90u_config_mask[config][type] == 0x00)
		return -ENOENT;

	ret = i2c_smbus_read_byte_data(client, ds90u_config_reg[config][type]);
	if (ret < 0) {
		dev_err(&client->dev, "read of 0x%02X failed\n", ds90u_config_reg[config][type]);
		return ret;
	}

	value = ret | ds90u_config_mask[config][type];
	ret = i2c_smbus_write_byte_data(client, ds90u_config_reg[config][type], value);
	if (ret < 0)
		dev_err(&client->dev, "write 0x%02X to 0x%02X failed\n", value, ds90u_config_reg[config][type]);
	return ret;
}

/**
 * ds90u_config_clear() - config write helper
 * Uses config enum to clear device-specific configuration bits.
 * @client:	device-specific client
 * @config:	config item to clear
 */
int ds90u_config_clear(struct i2c_client *client,
		       enum ds90u_config_names config)
{
	int ret;
	enum ds90u_node_type type;
	struct ds90u_cdata *cdata;
	uint8_t value;

	cdata = (struct ds90u_cdata *)i2c_get_clientdata(client);
	WARN_ON(cdata == NULL);
	if (cdata == NULL)
		return -ENOMEM;

	type = cdata->type;
	if (type >= DS90U_MAX_TYPE || config >= CFG_MAX_CONFIG)
		return -EINVAL;

	/* abort the write if the register is invalid for this device */
	if (ds90u_config_mask[config][type] == 0x00)
		return -ENOENT;

	ret = i2c_smbus_read_byte_data(client, ds90u_config_reg[config][type]);
	if (ret < 0) {
		dev_err(&client->dev, "read of 0x%02X failed\n", ds90u_config_reg[config][type]);
		return ret;
	}

	value = ret & ~ds90u_config_mask[config][type];
	ret = i2c_smbus_write_byte_data(client, ds90u_config_reg[config][type], value);
	if (ret < 0)
		dev_err(&client->dev, "write 0x%02X to 0x%02X failed\n", value, ds90u_config_reg[config][type]);
	return ret;
}

