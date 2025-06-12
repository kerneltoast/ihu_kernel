// SPDX-License-Identifier: GPL
/*
 * ds90u-init.c - link initialization for TI (formerly National Semiconductor)
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
 * DOC: ds90u-init
 *
 * This module is intended to handle i2c communication with TI (formerly
 * National Semiconductor) FPD-Link III ICs.
 *
 * This modules initializes the registers in the chip when a new link is
 * established.
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/i2c/ds90u.h>
#include "ds90u-core.h"
#include "ds90u-i2c.h"
#include "ds90u-init.h"
#include "ds90u-i2c-adapter.h"
#include "ds90u-gpio.h"
#include "ds90u-wbal.h"

/**
 * ds90u_init_i2c_map() - initialize device i2c translation mapping
 * Initializes the client i2c translation registers based on device tree
 * bindings. Assumes a valid link exists.
 * @client:	device-specific client struct
 * @cdata:	device-specific client data struct
 */
static void ds90u_init_i2c_map(struct i2c_client *client,
	struct ds90u_cdata *cdata)
{
	size_t i;

	dev_dbg(&client->dev, "%s\n", __func__);

	for (i = 0; i < ARRAY_SIZE(cdata->i2c_alias_init); i++) {
		int retval;
		uint8_t slave_alias;
		uint8_t slave_id;

		slave_id = cdata->i2c_alias_init[i].remote_address;
		slave_alias = cdata->i2c_alias_init[i].alias_address;

		if ((slave_id == 0) || (slave_alias == 0))
			continue;

		/*
		 * work-around for the 925-model -- data sheet indicates that it
		 * only supports 1 slave alias
		 */
		if ((i > 0) && (cdata->type == DS90U_SER925)) {
			dev_warn(&client->dev, "device only supports 1 i2c alias mapping -- ignoring extra entries\n");
			break;
		}

		retval = ds90u_config_write(client, CFG_I2C_SLAVE_ID0 + i,
			slave_id);
		if (retval)
			continue;
		retval = ds90u_config_write(client, CFG_I2C_SLAVE_ALIAS0 + i,
			slave_alias);
		if (retval == 0)
			dev_dbg(&client->dev, "  mapping address 0x%02X -> 0x%02X\n", slave_alias, slave_id);
	}
}

/**
 * ds90u_init_gpios() - initialize device gpio ports
 * Initializes the client gpio pinmux registers to desired initial values.
 * Assumes a valid link exists.
 * @client:	device-specific client struct
 * @cdata:	device-specific client data struct
 */
static void ds90u_init_gpios(struct i2c_client *client,
	struct ds90u_cdata *cdata)
{
	int i;

	dev_dbg(&client->dev, "%s\n", __func__);

	for (i = 0; i < ARRAY_SIZE(cdata->gpio_init); i++) {
		int result;

		if (cdata->gpio_init[i] > 0x0F)
			continue;

		result = ds90u_config_write(client, CFG_GPIO0_CONFIG + i,
			cdata->gpio_init[i]);

		if (result == 0)
			dev_dbg(&client->dev, "  GPIO %d = 0x%1X : %d\n", i, cdata->gpio_init[i], cdata->serializer);
	}
}

/**
 * ds90u_deinit_gpios() - set gpio signals to defined state
 * @client:	device-specific client struct
 * @cdata:	device-specific client data struct
 */
static void ds90u_deinit_gpios(struct i2c_client *client,
	struct ds90u_cdata *cdata)
{
	int i;

	dev_dbg(&client->dev, "%s\n", __func__);

	for (i = 0; i < ARRAY_SIZE(cdata->gpio_deinit); i++) {
		int result;

		if (cdata->gpio_deinit[i] == DS90U_GPIO_DONTTOUCH)
			continue;

		result = ds90u_config_write(client, CFG_GPIO0_CONFIG + i,
			cdata->gpio_deinit[i]);

		if (result == 0)
			dev_dbg(&client->dev, "  GPIO %d = 0x%1X\n", i, cdata->gpio_deinit[i]);
	}
}

/**
 * ds90u_initialize_link() - device link initialization
 * Initializes the client i2c registers and registers additional client
 * drivers behind the i2c gateway. Assumes a valid link exists.
 * @client:	device-specific client struct
 * @cdata:	device-specific client data struct
 */
void ds90u_initialize_link(struct i2c_client *client, struct ds90u_cdata *cdata)
{
	dev_dbg(&client->dev, "%s\n", __func__);

	if (cdata->video_18_bit)
		ds90u_config_set(client, CFG_18BIT_VID);

	if (cdata->mapsel_override) {
		if (cdata->mapsel_value)
			ds90u_config_write(client, CFG_MAPSEL_OVR, 0x3);
		else
			ds90u_config_write(client, CFG_MAPSEL_OVR, 0x2);
	}
	/* for DS90U_DESER0 save i2c address to mailbox0 and poll and compare to detect reset */
	ds90u_config_write(client, CFG_MAILBOX0, (uint8_t)client->addr);

	if (ds90u_config_set(client, CFG_I2C_PASSTHROUGH) == 0) {
		int scl_pulse_width = cdata->scl_pulse_width;

		if (scl_pulse_width >= 0 && scl_pulse_width <= 0xFF) {
			ds90u_config_write(client, CFG_SCL_HIGH_TIME,
				scl_pulse_width);
			ds90u_config_write(client, CFG_SCL_LOW_TIME,
				scl_pulse_width);
		}

		ds90u_init_i2c_map(client, cdata);
	}

	/* Initialize GPIO controller */
	if (IS_ENABLED(CONFIG_SERDES_TIDS90U_GPIO))
		ds90u_register_gpio_controller(cdata);

	/* Initialize GPIO defaults */
	ds90u_init_gpios(client, cdata);

	/* Initialize white balance (deserializer only) */
	if (IS_ENABLED(CONFIG_SERDES_TIDS90U_LUT))
		if (!cdata->serializer)
			ds90u_init_white_bal(client);

	if (cdata->i2c_promiscuous_mode)
		ds90u_config_set(client, CFG_I2C_PROMISCUOUS);

	/*
	 * Override Output Sleep State
	 * This should probably be done after other configurations, but
	 * probably needs to happen before registering child I2C devices.
	 */
	if (cdata->oen_override) {
		uint8_t value = 0x4; /*value will be shifted into upper nibble*/

		if (cdata->oen_value)
			value |= 0x8;
		if (cdata->oss_value)
			value |= 0x1;
		ds90u_config_write(client, CFG_OEN_OVR, value);
	}

	/* deserializer: BIST enable from register */
	if (!cdata->serializer) {
		ds90u_config_clear(client, CFG_BIST_PIN_CONFIG);
	}

	/* Disable Block I2S auto config
	 * Disable I2S
	 */
	ds90u_config_set(client, CFG_BLK_I2S_AUTO);
	ds90u_config_set(client, CFG_I2S_DISABLE);

	/* Initialize I2C adapter and register child devices */
	if (IS_ENABLED(CONFIG_SERDES_TIDS90U_I2C))
		ds90u_register_i2c_adapter(cdata);

	/* Apply selected voltage swing for the FPD/OLDI output */
	if ((cdata->lvds_vod_value > 0x0) && (cdata->lvds_vod_value <= 0x3))
		ds90u_config_write(client, CFG_LVDS_VOD_CONTROL, cdata->lvds_vod_value);
}

/**
 * ds90u_deinitialize_link() - device link removal
 * Removes any registered gpio controller, i2c adapter, and nested clients.
 * Assumes a valid link exists.
 * @cdata:	device-specific client data struct
 */
void ds90u_deinitialize_link(struct ds90u_cdata *cdata)
{
	dev_dbg(&cdata->client->dev, "%s\n", __func__);

	if (IS_ENABLED(CONFIG_SERDES_TIDS90U_I2C))
		ds90u_unregister_i2c_adapter(cdata);

	/* Deinitialize GPIO  */
	ds90u_deinit_gpios(cdata->client, cdata);

	if (IS_ENABLED(CONFIG_SERDES_TIDS90U_GPIO))
		ds90u_unregister_gpio_controller(cdata);
}
