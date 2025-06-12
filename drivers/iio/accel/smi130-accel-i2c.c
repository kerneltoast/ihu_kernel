/*
 * 3-axis accelerometer driver supporting following I2C Bosch-Sensortec chips:
 *  - SMI130
 *
 * Forked from bmc150-accel-i2c.c
 * Copyright (c) 2014, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/regmap.h>

#include "smi130-accel.h"

static int smi130_accel_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	struct regmap *regmap;
	const char *name = NULL;

	regmap = devm_regmap_init_i2c(client, &smi130_regmap_conf);
	if (IS_ERR(regmap)) {
		dev_err(&client->dev, "Failed to initialize i2c regmap\n");
		return PTR_ERR(regmap);
	}

	if (id)
		name = id->name;

	return smi130_accel_core_probe(&client->dev, regmap, client->irq, name);
}

static int smi130_accel_remove(struct i2c_client *client)
{
	return smi130_accel_core_remove(&client->dev);
}

static const struct acpi_device_id smi130_accel_acpi_match[] = {
	{"SMI130A",	smi130},
	{ },
};
MODULE_DEVICE_TABLE(acpi, smi130_accel_acpi_match);

static const struct i2c_device_id smi130_accel_id[] = {
	{"smi130_accel",	smi130},
	{}
};

MODULE_DEVICE_TABLE(i2c, smi130_accel_id);

static struct i2c_driver smi130_accel_driver = {
	.driver = {
		.name	= "smi130_accel_i2c",
		.acpi_match_table = ACPI_PTR(smi130_accel_acpi_match),
		.pm	= &smi130_accel_pm_ops,
	},
	.probe		= smi130_accel_probe,
	.remove		= smi130_accel_remove,
	.id_table	= smi130_accel_id,
};
module_i2c_driver(smi130_accel_driver);

MODULE_AUTHOR("Chris Baker <chris.l.baker@aptiv.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SMI130 I2C accelerometer driver");
