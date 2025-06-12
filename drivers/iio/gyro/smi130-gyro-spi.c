#include <linux/spi/spi.h>
#include <linux/regmap.h>
#include <linux/iio/iio.h>
#include <linux/module.h>
#include <linux/acpi.h>

#include "smi130-gyro.h"

static const struct regmap_config smi130_gyro_regmap_spi_conf = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x3f,
};

static int smi130_gyro_spi_probe(struct spi_device *spi)
{
	struct regmap *regmap;
	const struct spi_device_id *id = spi_get_device_id(spi);

	regmap = devm_regmap_init_spi(spi, &smi130_gyro_regmap_spi_conf);
	if (IS_ERR(regmap)) {
		dev_err(&spi->dev, "Failed to register spi regmap %d\n",
			(int)PTR_ERR(regmap));
		return PTR_ERR(regmap);
	}

	return smi130_gyro_core_probe(&spi->dev, regmap, spi->irq, id->name);
}

static int smi130_gyro_spi_remove(struct spi_device *spi)
{
	smi130_gyro_core_remove(&spi->dev);

	return 0;
}

static const struct acpi_device_id smi130_gyro_acpi_match[] = {
	{"SMI130G",	0},
	{ },
};
MODULE_DEVICE_TABLE(acpi, smi130_gyro_acpi_match);

static const struct spi_device_id smi130_gyro_spi_id[] = {
	{"smi130_gyro", 0},
	{}
};
MODULE_DEVICE_TABLE(spi, smi130_gyro_spi_id);

static struct spi_driver smi130_gyro_spi_driver = {
	.driver = {
		.name	= "smi130_gyro_spi",
		.acpi_match_table = ACPI_PTR(smi130_gyro_acpi_match),
		.pm	= &smi130_gyro_pm_ops,
	},
	.probe		= smi130_gyro_spi_probe,
	.remove		= smi130_gyro_spi_remove,
	.id_table	= smi130_gyro_spi_id,
};
module_spi_driver(smi130_gyro_spi_driver);

MODULE_AUTHOR("Chris Baker <chris.l.baker@aptiv.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SMI130 SPI Gyro driver");
