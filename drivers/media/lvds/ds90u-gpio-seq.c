// SPDX-License-Identifier: GPL
/*
 * ds90u-gpio-seq.c - driver for ds90u gpio sequencing
 * DS90Ux9xx FPD-Link III serializer/deserializer ICs.
 *
 * Copyright (C) 2014-2017 Delphi Technologies, Inc., All Rights Reserved.
 * Copyright (C) 2018-2019 Aptiv, All Rights Reserved.
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

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_data/ds90u.h>
#include <linux/i2c/ds90u.h>
#include <linux/platform_data/ds90u-gpioseq.h>
#include "ds90u-core.h"
#include "ds90u-i2c.h"
#include "ds90u-init.h"

/*
 * To be able to sequence the tft-enable signal before the tft-backlight-enable signal
 * a dummy i2c device is used. It will use the ds90u gpiochip
 */

struct ds90_gpioseq {
	struct gpio_desc *enable_gpio;
};

static int ds90_gpioseq_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct ds90_gpioseq_platform_data *pdata = client->dev.platform_data;
	struct ds90_gpioseq *data;
	int ret;

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	if (pdata) {
		if (gpio_is_valid(pdata->enable_gpio)) {
			ret = devm_gpio_request(&client->dev,
						pdata->enable_gpio, "tftenable");
			if (ret) {
				dev_err(&client->dev, "Failed to get enable gpio: %d\n", ret);
				return ret;
			}
			data->enable_gpio = gpio_to_desc(pdata->enable_gpio);
		}
	}
	if (data->enable_gpio)
		gpiod_direction_output(data->enable_gpio, 1);

	i2c_set_clientdata(client, data);

	/*
	 * at this point tft-backlight is still off, introduce delay to
	 * avoid flicker
	 */
	if (pdata->delay)
		msleep(pdata->delay);

	return 0;
}

static int ds90_gpioseq_remove(struct i2c_client *client)
{
	struct ds90_gpioseq *data = i2c_get_clientdata(client);

	if (data->enable_gpio) {
		msleep(100);
		gpiod_direction_output(data->enable_gpio, 0);
	}
	return 0;
}

static const struct i2c_device_id ds90_gpioseq_id[] = {
	{ "ds90ugpioseq", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ds90_gpioseq_id);

#ifdef CONFIG_OF
static const struct of_device_id of_ds90_gpioseq_match[] = {
	{ .compatible = "ti,ds90ugpioseq", },
	{},
};
MODULE_DEVICE_TABLE(of, of_ds90_gpioseq_match);
#endif

struct i2c_driver ds90_gpioseq_driver = {
	.driver = {
		.name	= "ds90ugpioseq",
		.of_match_table = of_match_ptr(of_ds90_gpioseq_match),
	},
	.probe		= ds90_gpioseq_probe,
	.remove		= ds90_gpioseq_remove,
	.id_table	= ds90_gpioseq_id,
};

module_i2c_driver(ds90_gpioseq_driver);

MODULE_DESCRIPTION("DS90u Gpio sequencing helper");
MODULE_AUTHOR("Aptiv");
MODULE_LICENSE("GPL");
