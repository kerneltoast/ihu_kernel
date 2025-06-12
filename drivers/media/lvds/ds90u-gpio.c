// SPDX-License-Identifier: GPL
/*
 * ds90u-gpio.c - gpio controller for TI (formerly National Semiconductor)
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
 * DOC: ds90u-gpio
 *
 * This module is intended to expose the GPIO interface of TI (formerly
 * National Semiconductor) FPD-Link III ICs.
 *
 * The ds90u-core module is the main driver for these ICs and this module
 * piggybacks on top of it.
 *
 * All the ds90u parts appear to support 4 physical GPIOs, but in some cases
 * these are multiplexed with other pin functions. Additionally, some chips
 * have additional registered GPIOs and high speed GPIOs.
 */

#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c/ds90u.h>
#include "ds90u-core.h"
#include "ds90u-gpio.h"
#include "ds90u-i2c.h"

static int ds90u_reg_gpio_dir_in(struct gpio_chip *chip, unsigned int offset)
{
	struct ds90u_cdata *cdata = container_of(chip, struct ds90u_cdata, gc);

	if (offset < 4) {
		return ds90u_config_write(cdata->client,
			CFG_GPIO0_CONFIG + offset, DS90U_GPIO_INPUT);
	} else {
		if (chip->ngpio == 8)
			offset += 1; /* skip reg gpio 4 */
		offset -= 4;
		return ds90u_config_write(cdata->client,
			CFG_REG_GPIO4_CONFIG + offset, DS90U_GPIO_INPUT);
	}

	return -EINVAL;
}

static int ds90u_reg_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	struct ds90u_cdata *cdata = container_of(chip, struct ds90u_cdata, gc);
	int val = 0;

	if (offset < 4) {
		val = ds90u_config_read(cdata->client,
					CFG_GPIO0_CONFIG + offset);
		if (val == DS90U_GPIO_OUTPUT_HIGH ||
		    val == DS90U_GPIO_OUTPUT_LOW) {
			val = (val == DS90U_GPIO_OUTPUT_HIGH) ? 1 : 0;
		} else {
			val = ds90u_config_read(cdata->client,
						CFG_REG_GPI0_STATUS + offset);
		}
	} else {
		if (chip->ngpio == 8)
			offset += 1; /* skip reg gpio 4 */

		val = ds90u_config_read(cdata->client,
					CFG_REG_GPIO4_CONFIG + offset - 4);

		if (val == DS90U_GPIO_OUTPUT_HIGH ||
		    val == DS90U_GPIO_OUTPUT_LOW) {
			val = (val == DS90U_GPIO_OUTPUT_HIGH) ? 1 : 0;
		} else {

			val = ds90u_config_read(cdata->client,
						CFG_REG_GPI0_STATUS + offset);
		}
	}

	return val;
}

static int ds90u_reg_gpio_dir_out(struct gpio_chip *chip, unsigned int offset, int value)
{
	struct ds90u_cdata *cdata = container_of(chip, struct ds90u_cdata, gc);
	uint8_t val = value ? DS90U_GPIO_OUTPUT_HIGH : DS90U_GPIO_OUTPUT_LOW;

	if (offset < 4) {
		return ds90u_config_write(cdata->client,
			CFG_GPIO0_CONFIG + offset, val);
	} else {
		if (chip->ngpio == 8)
			offset += 1; /* skip reg gpio 4 */
		offset -= 4;
		return ds90u_config_write(cdata->client,
			CFG_REG_GPIO4_CONFIG + offset, val);
	}

	return -EINVAL;
}

static void
ds90u_reg_gpio_set(struct gpio_chip *chip, unsigned int offset, int value)
{
	struct ds90u_cdata *cdata = container_of(chip, struct ds90u_cdata, gc);
	uint8_t val = value ? DS90U_GPIO_OUTPUT_HIGH : DS90U_GPIO_OUTPUT_LOW;

	if (offset < 4) {
		WARN_ON(ds90u_config_write(cdata->client,
			CFG_GPIO0_CONFIG + offset, val));
		return;
	}

	if (chip->ngpio == 8)
		offset += 1; /* skip reg gpio 4 */
	offset -= 4;
	WARN_ON(ds90u_config_write(cdata->client,
		CFG_REG_GPIO4_CONFIG + offset, val));
}

/**
 * ds90u_register_controller() - initialize gpio-controller interface
 * Configures the driver with a gpiochip interface that can be used by other
 * device in the device-tree.
 * @cdata:	device-specific client data struct
 */
void ds90u_register_gpio_controller(struct ds90u_cdata *cdata)
{
	int ret;

	dev_dbg(&cdata->client->dev, "%s\n", __func__);

	if (!cdata->gpio_controller)
		return;

	cdata->gc.can_sleep = 1;

	/* only 925/926 has GPIO REG 4, but 925/926 has no input capability */
	if ((cdata->type == DS90U_SER925) || (cdata->type == DS90U_DESER926)) {
		cdata->gc.ngpio = 9;
		cdata->gc.direction_input = NULL;
		cdata->gc.get = NULL;
	} else {
		cdata->gc.ngpio = 8;
		cdata->gc.direction_input = ds90u_reg_gpio_dir_in;
		cdata->gc.get = ds90u_reg_gpio_get;
	}

	cdata->gc.direction_output = ds90u_reg_gpio_dir_out;
	cdata->gc.set = ds90u_reg_gpio_set;

	if (cdata->gc.label)
		dev_dbg(&cdata->client->dev, "  Creating gpio controller for %s\n", cdata->gc.label);
	ret = gpiochip_add(&cdata->gc);
	if (ret) {
		cdata->gc.ngpio = 0;
		dev_err(&cdata->client->dev, "gpiochip_add() failed -- %d\n", ret);
	}
}

/**
 * ds90u_unregister_controller() - remove the gpio-controller interface
 * Called during link-loss or driver shutdown.
 * @cdata:	device-specific client data struct
 */
void ds90u_unregister_gpio_controller(struct ds90u_cdata *cdata)
{
	dev_dbg(&cdata->client->dev, "%s\n", __func__);

	if ((cdata->gpio_controller) && (cdata->gc.ngpio != 0))
		gpiochip_remove(&cdata->gc);

	cdata->gc.ngpio = 0;
}
