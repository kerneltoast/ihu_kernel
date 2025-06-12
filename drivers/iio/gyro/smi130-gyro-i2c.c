#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/iio/iio.h>
#include <linux/module.h>
#include <linux/acpi.h>

#include "smi130-gyro.h"

static const struct regmap_config smi130_gyro_regmap_i2c_conf = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x3f
};

static int smi130_gyro_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct regmap *regmap;
	const char *name = NULL;

	regmap = devm_regmap_init_i2c(client, &smi130_gyro_regmap_i2c_conf);
	if (IS_ERR(regmap)) {
		dev_err(&client->dev, "Failed to register i2c regmap %d\n",
			(int)PTR_ERR(regmap));
		return PTR_ERR(regmap);
	}

	if (id)
		name = id->name;

	return smi130_gyro_core_probe(&client->dev, regmap, client->irq, name);
}

static int smi130_gyro_i2c_remove(struct i2c_client *client)
{
	smi130_gyro_core_remove(&client->dev);

	return 0;
}

static const struct acpi_device_id smi130_gyro_acpi_match[] = {
	{"SMI130G", 0},
	{},
};

MODULE_DEVICE_TABLE(acpi, smi130_gyro_acpi_match);

static const struct i2c_device_id smi130_gyro_i2c_id[] = {
	{"smi130_gyro", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, smi130_gyro_i2c_id);

static struct i2c_driver smi130_gyro_i2c_driver = {
	.driver = {
		.name	= "smi130_gyro_i2c",
		.acpi_match_table = ACPI_PTR(smi130_gyro_acpi_match),
		.pm	= &smi130_gyro_pm_ops,
	},
	.probe		= smi130_gyro_i2c_probe,
	.remove		= smi130_gyro_i2c_remove,
	.id_table	= smi130_gyro_i2c_id,
};
module_i2c_driver(smi130_gyro_i2c_driver);

MODULE_AUTHOR("Chris Baker <chris.l.baker@aptiv.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SMI130 I2C Gyro driver");
