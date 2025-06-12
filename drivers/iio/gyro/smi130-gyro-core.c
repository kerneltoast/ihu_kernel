/*
 * SMI130 Gyro Sensor driver
 *
 * Forked from bmg160_core.c
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

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/events.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include "smi130-gyro.h"

#define SMI130_GYRO_IRQ_NAME		"smi130_gyro_event"

#define SMI130_GYRO_REG_CHIP_ID		0x00
#define SMI130_GYRO_CHIP_ID_VAL		0x0F

#define SMI130_GYRO_REG_RANGE		0x0F

#define SMI130_GYRO_RANGE_2000DPS		0
#define SMI130_GYRO_RANGE_1000DPS		1
#define SMI130_GYRO_RANGE_500DPS		2
#define SMI130_GYRO_RANGE_250DPS		3
#define SMI130_GYRO_RANGE_125DPS		4

#define SMI130_GYRO_REG_PMU_BW		0x10
#define SMI130_GYRO_NO_FILTER		0
#define SMI130_GYRO_DEF_BW			100
#define SMI130_GYRO_REG_PMU_BW_RES		BIT(7)

#define SMI130_GYRO_GYRO_REG_RESET		0x14
#define SMI130_GYRO_GYRO_RESET_VAL		0xb6

#define SMI130_GYRO_REG_INT_MAP_1		0x18
#define SMI130_GYRO_INT_MAP_1_BIT_NEW_DATA	BIT(0)

#define SMI130_GYRO_REG_INT_EN_0		0x15
#define SMI130_GYRO_DATA_ENABLE_INT		BIT(7)

#define SMI130_GYRO_REG_INT_EN_1		0x16
#define SMI130_GYRO_INT1_BIT_OD		BIT(1)

#define SMI130_GYRO_REG_XOUT_L		0x02
#define SMI130_GYRO_AXIS_TO_REG(axis)	(SMI130_GYRO_REG_XOUT_L + (axis * 2))

#define SMI130_GYRO_REG_TEMP		0x08
#define SMI130_GYRO_TEMP_CENTER_VAL		24

#define SMI130_GYRO_MAX_STARTUP_TIME_MS	80

#define SMI130_GYRO_AUTO_SUSPEND_DELAY_MS	2000

struct smi130_gyro_data {
	struct regmap *regmap;
	struct iio_trigger *dready_trig;
	struct mutex mutex;
	s16 buffer[8];
	u32 dps_range;
	bool dready_trigger_on;
	int irq;
};

enum smi130_gyro_axis {
	AXIS_X,
	AXIS_Y,
	AXIS_Z,
	AXIS_MAX,
};

static const struct {
	int odr;
	int filter;
	int bw_bits;
} smi130_gyro_samp_freq_table[] = { {100, 32, 0x07},
			       {200, 64, 0x06},
			       {100, 12, 0x05},
			       {200, 23, 0x04},
			       {400, 47, 0x03},
			       {1000, 116, 0x02},
			       {2000, 230, 0x01} };

static const struct {
	int scale;
	int dps_range;
} smi130_gyro_scale_table[] = { { 1065, SMI130_GYRO_RANGE_2000DPS},
			   { 532, SMI130_GYRO_RANGE_1000DPS},
			   { 266, SMI130_GYRO_RANGE_500DPS},
			   { 133, SMI130_GYRO_RANGE_250DPS},
			   { 66, SMI130_GYRO_RANGE_125DPS} };

static int smi130_gyro_convert_freq_to_bit(int val)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smi130_gyro_samp_freq_table); ++i) {
		if (smi130_gyro_samp_freq_table[i].odr == val)
			return smi130_gyro_samp_freq_table[i].bw_bits;
	}

	return -EINVAL;
}

static int smi130_gyro_set_bw(struct smi130_gyro_data *data, int val)
{
	struct device *dev = regmap_get_device(data->regmap);
	int ret;
	int bw_bits;

	bw_bits = smi130_gyro_convert_freq_to_bit(val);
	if (bw_bits < 0)
		return bw_bits;

	ret = regmap_write(data->regmap, SMI130_GYRO_REG_PMU_BW, bw_bits);
	if (ret < 0) {
		dev_err(dev, "Error writing reg_pmu_bw\n");
		return ret;
	}

	return 0;
}

static int smi130_gyro_get_filter(struct smi130_gyro_data *data, int *val)
{
	struct device *dev = regmap_get_device(data->regmap);
	int ret;
	int i;
	unsigned int bw_bits;

	ret = regmap_read(data->regmap, SMI130_GYRO_REG_PMU_BW, &bw_bits);
	if (ret < 0) {
		dev_err(dev, "Error reading reg_pmu_bw\n");
		return ret;
	}

	/* Ignore the readonly reserved bit. */
	bw_bits &= ~SMI130_GYRO_REG_PMU_BW_RES;

	for (i = 0; i < ARRAY_SIZE(smi130_gyro_samp_freq_table); ++i) {
		if (smi130_gyro_samp_freq_table[i].bw_bits == bw_bits)
			break;
	}

	*val = smi130_gyro_samp_freq_table[i].filter;

	return ret ? ret : IIO_VAL_INT;
}


static int smi130_gyro_set_filter(struct smi130_gyro_data *data, int val)
{
	struct device *dev = regmap_get_device(data->regmap);
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(smi130_gyro_samp_freq_table); ++i) {
		if (smi130_gyro_samp_freq_table[i].filter == val)
			break;
	}

	ret = regmap_write(data->regmap, SMI130_GYRO_REG_PMU_BW,
			   smi130_gyro_samp_freq_table[i].bw_bits);
	if (ret < 0) {
		dev_err(dev, "Error writing reg_pmu_bw\n");
		return ret;
	}

	return 0;
}

static int smi130_gyro_chip_init(struct smi130_gyro_data *data)
{
	struct device *dev = regmap_get_device(data->regmap);
	int ret;
	unsigned int val;

	/*
	 * Reset chip to get it in a known good state. A delay of 30ms after
	 * reset is required according to the datasheet.
	 */
	regmap_write(data->regmap, SMI130_GYRO_GYRO_REG_RESET,
		     SMI130_GYRO_GYRO_RESET_VAL);
	usleep_range(30000, 30700); /* leftover from bmg160, not really
					mentioned in smi130 datasheet */

	ret = regmap_read(data->regmap, SMI130_GYRO_REG_CHIP_ID, &val);
	if (ret < 0) {
		dev_err(dev, "Error reading reg_chip_id\n");
		return ret;
	}

	dev_dbg(dev, "Chip Id %x\n", val);
	if (val != SMI130_GYRO_CHIP_ID_VAL) {
		dev_err(dev, "invalid chip %x\n", val);
		return -ENODEV;
	}

	/* Set Bandwidth */
	ret = smi130_gyro_set_bw(data, SMI130_GYRO_DEF_BW);
	if (ret < 0)
		return ret;

	/* Set Default Range */
	ret = regmap_write(data->regmap, SMI130_GYRO_REG_RANGE,
				SMI130_GYRO_RANGE_500DPS);
	if (ret < 0) {
		dev_err(dev, "Error writing reg_range\n");
		return ret;
	}
	data->dps_range = SMI130_GYRO_RANGE_500DPS;

	/* Set default interrupt mode */
	ret = regmap_update_bits(data->regmap, SMI130_GYRO_REG_INT_EN_1,
				 SMI130_GYRO_INT1_BIT_OD, 0);
	if (ret < 0) {
		dev_err(dev, "Error updating bits in reg_int_en_1\n");
		return ret;
	}

	return 0;
}

static int smi130_gyro_set_power_state(struct smi130_gyro_data *data, bool on)
{
#ifdef CONFIG_PM
	struct device *dev = regmap_get_device(data->regmap);
	int ret;

	if (on)
		ret = pm_runtime_get_sync(dev);
	else {
		pm_runtime_mark_last_busy(dev);
		ret = pm_runtime_put_autosuspend(dev);
	}

	if (ret < 0) {
		dev_err(dev, "Failed: smi130_gyro_set_power_state for %d\n", on);

		if (on)
			pm_runtime_put_noidle(dev);

		return ret;
	}
#endif

	return 0;
}

static int smi130_gyro_setup_new_data_interrupt(struct smi130_gyro_data *data,
					   bool status)
{
	struct device *dev = regmap_get_device(data->regmap);
	int ret;

	/* Enable/Disable INT_MAP1 mapping */
	ret = regmap_update_bits(data->regmap, SMI130_GYRO_REG_INT_MAP_1,
				 SMI130_GYRO_INT_MAP_1_BIT_NEW_DATA,
				 (status ? SMI130_GYRO_INT_MAP_1_BIT_NEW_DATA : 0));
	if (ret < 0) {
		dev_err(dev, "Error updating bits in reg_int_map1\n");
		return ret;
	}

	if (status)
		ret = regmap_write(data->regmap, SMI130_GYRO_REG_INT_EN_0,
				   SMI130_GYRO_DATA_ENABLE_INT);
	else
		ret = regmap_write(data->regmap, SMI130_GYRO_REG_INT_EN_0, 0);

	if (ret < 0) {
		dev_err(dev, "Error writing reg_int_en0\n");
		return ret;
	}

	return 0;
}

static int smi130_gyro_get_bw(struct smi130_gyro_data *data, int *val)
{
	struct device *dev = regmap_get_device(data->regmap);
	int i;
	unsigned int bw_bits;
	int ret;

	ret = regmap_read(data->regmap, SMI130_GYRO_REG_PMU_BW, &bw_bits);
	if (ret < 0) {
		dev_err(dev, "Error reading reg_pmu_bw\n");
		return ret;
	}

	/* Ignore the readonly reserved bit. */
	bw_bits &= ~SMI130_GYRO_REG_PMU_BW_RES;

	for (i = 0; i < ARRAY_SIZE(smi130_gyro_samp_freq_table); ++i) {
		if (smi130_gyro_samp_freq_table[i].bw_bits == bw_bits) {
			*val = smi130_gyro_samp_freq_table[i].odr;
			return IIO_VAL_INT;
		}
	}

	return -EINVAL;
}

static int smi130_gyro_set_scale(struct smi130_gyro_data *data, int val)
{
	struct device *dev = regmap_get_device(data->regmap);
	int ret, i;

	for (i = 0; i < ARRAY_SIZE(smi130_gyro_scale_table); ++i) {
		if (smi130_gyro_scale_table[i].scale == val) {
			ret = regmap_write(data->regmap, SMI130_GYRO_REG_RANGE,
					   smi130_gyro_scale_table[i].dps_range);
			if (ret < 0) {
				dev_err(dev, "Error writing reg_range\n");
				return ret;
			}
			data->dps_range = smi130_gyro_scale_table[i].dps_range;
			return 0;
		}
	}

	return -EINVAL;
}

static int smi130_gyro_get_temp(struct smi130_gyro_data *data, int *val)
{
	struct device *dev = regmap_get_device(data->regmap);
	int ret;
	unsigned int raw_val;

	mutex_lock(&data->mutex);
	ret = smi130_gyro_set_power_state(data, true);
	if (ret < 0) {
		mutex_unlock(&data->mutex);
		return ret;
	}

	ret = regmap_read(data->regmap, SMI130_GYRO_REG_TEMP, &raw_val);
	if (ret < 0) {
		dev_err(dev, "Error reading reg_temp\n");
		smi130_gyro_set_power_state(data, false);
		mutex_unlock(&data->mutex);
		return ret;
	}

	*val = sign_extend32(raw_val, 7);
	ret = smi130_gyro_set_power_state(data, false);
	mutex_unlock(&data->mutex);
	if (ret < 0)
		return ret;

	return IIO_VAL_INT;
}

static int smi130_gyro_get_axis(struct smi130_gyro_data *data, int axis, int *val)
{
	struct device *dev = regmap_get_device(data->regmap);
	int ret;
	__le16 raw_val;

	mutex_lock(&data->mutex);
	ret = smi130_gyro_set_power_state(data, true);
	if (ret < 0) {
		mutex_unlock(&data->mutex);
		return ret;
	}

	ret = regmap_bulk_read(data->regmap, SMI130_GYRO_AXIS_TO_REG(axis), &raw_val,
			       sizeof(raw_val));
	if (ret < 0) {
		dev_err(dev, "Error reading axis %d\n", axis);
		smi130_gyro_set_power_state(data, false);
		mutex_unlock(&data->mutex);
		return ret;
	}

	*val = sign_extend32(le16_to_cpu(raw_val), 15);
	ret = smi130_gyro_set_power_state(data, false);
	mutex_unlock(&data->mutex);
	if (ret < 0)
		return ret;

	return IIO_VAL_INT;
}

static int smi130_gyro_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val, int *val2, long mask)
{
	struct smi130_gyro_data *data = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		switch (chan->type) {
		case IIO_TEMP:
			return smi130_gyro_get_temp(data, val);
		case IIO_ANGL_VEL:
			if (iio_buffer_enabled(indio_dev))
				return -EBUSY;
			else
				return smi130_gyro_get_axis(data, chan->scan_index,
						       val);
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_OFFSET:
		if (chan->type == IIO_TEMP) {
			*val = SMI130_GYRO_TEMP_CENTER_VAL;
			return IIO_VAL_INT;
		} else
			return -EINVAL;
	case IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY:
		return smi130_gyro_get_filter(data, val);
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		switch (chan->type) {
		case IIO_TEMP:
			*val2 = 500000;
			return IIO_VAL_INT_PLUS_MICRO;
		case IIO_ANGL_VEL:
		{
			int i;

			for (i = 0; i < ARRAY_SIZE(smi130_gyro_scale_table); ++i) {
				if (smi130_gyro_scale_table[i].dps_range ==
							data->dps_range) {
					*val2 = smi130_gyro_scale_table[i].scale;
					return IIO_VAL_INT_PLUS_MICRO;
				}
			}
			return -EINVAL;
		}
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val2 = 0;
		mutex_lock(&data->mutex);
		ret = smi130_gyro_get_bw(data, val);
		mutex_unlock(&data->mutex);
		return ret;
	default:
		return -EINVAL;
	}
}

static int smi130_gyro_write_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int val, int val2, long mask)
{
	struct smi130_gyro_data *data = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		mutex_lock(&data->mutex);
		ret = smi130_gyro_set_power_state(data, true);
		if (ret < 0) {
			mutex_unlock(&data->mutex);
			return ret;
		}
		ret = smi130_gyro_set_bw(data, val);
		if (ret < 0) {
			smi130_gyro_set_power_state(data, false);
			mutex_unlock(&data->mutex);
			return ret;
		}
		ret = smi130_gyro_set_power_state(data, false);
		mutex_unlock(&data->mutex);
		return ret;
	case IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY:
		if (val2)
			return -EINVAL;

		mutex_lock(&data->mutex);
		ret = smi130_gyro_set_power_state(data, true);
		if (ret < 0) {
			smi130_gyro_set_power_state(data, false);
			mutex_unlock(&data->mutex);
			return ret;
		}
		ret = smi130_gyro_set_filter(data, val);
		if (ret < 0) {
			smi130_gyro_set_power_state(data, false);
			mutex_unlock(&data->mutex);
			return ret;
		}
		ret = smi130_gyro_set_power_state(data, false);
		mutex_unlock(&data->mutex);
		return ret;
	case IIO_CHAN_INFO_SCALE:
		if (val)
			return -EINVAL;

		mutex_lock(&data->mutex);
		ret = smi130_gyro_set_power_state(data, true);
		if (ret < 0) {
			mutex_unlock(&data->mutex);
			return ret;
		}
		ret = smi130_gyro_set_scale(data, val2);
		if (ret < 0) {
			smi130_gyro_set_power_state(data, false);
			mutex_unlock(&data->mutex);
			return ret;
		}
		ret = smi130_gyro_set_power_state(data, false);
		mutex_unlock(&data->mutex);
		return ret;
	default:
		return -EINVAL;
	}

	return -EINVAL;
}

static IIO_CONST_ATTR_SAMP_FREQ_AVAIL("100 200 400 1000 2000");

static IIO_CONST_ATTR(in_anglvel_scale_available,
		      "0.001065 0.000532 0.000266 0.000133 0.000066");

static struct attribute *smi130_gyro_attributes[] = {
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
	&iio_const_attr_in_anglvel_scale_available.dev_attr.attr,
	NULL,
};

static const struct attribute_group smi130_gyro_attrs_group = {
	.attrs = smi130_gyro_attributes,
};

#define SMI130_GYRO_CHANNEL(_axis) {					\
	.type = IIO_ANGL_VEL,						\
	.modified = 1,							\
	.channel2 = IIO_MOD_##_axis,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),			\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) |		\
		BIT(IIO_CHAN_INFO_SAMP_FREQ) |				\
		BIT(IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY),	\
	.scan_index = AXIS_##_axis,					\
	.scan_type = {							\
		.sign = 's',						\
		.realbits = 16,					\
		.storagebits = 16,					\
		.endianness = IIO_LE,					\
	},								\
}

static const struct iio_chan_spec smi130_gyro_channels[] = {
	{
		.type = IIO_TEMP,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
				      BIT(IIO_CHAN_INFO_SCALE) |
				      BIT(IIO_CHAN_INFO_OFFSET),
		.scan_index = -1,
	},
	SMI130_GYRO_CHANNEL(X),
	SMI130_GYRO_CHANNEL(Y),
	SMI130_GYRO_CHANNEL(Z),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static const struct iio_info smi130_gyro_info = {
	.attrs			= &smi130_gyro_attrs_group,
	.read_raw		= smi130_gyro_read_raw,
	.write_raw		= smi130_gyro_write_raw,
};

static const unsigned long smi130_gyro_accel_scan_masks[] = {
					BIT(AXIS_X) | BIT(AXIS_Y) | BIT(AXIS_Z),
					0};

static irqreturn_t smi130_gyro_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct smi130_gyro_data *data = iio_priv(indio_dev);
	int ret;

	mutex_lock(&data->mutex);
	ret = regmap_bulk_read(data->regmap, SMI130_GYRO_REG_XOUT_L,
			       data->buffer, AXIS_MAX * 2);
	mutex_unlock(&data->mutex);
	if (ret < 0)
		goto err;

	iio_push_to_buffers_with_timestamp(indio_dev, data->buffer,
					   pf->timestamp);
err:
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static int smi130_gyro_data_rdy_trigger_set_state(struct iio_trigger *trig,
					     bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	struct smi130_gyro_data *data = iio_priv(indio_dev);
	int ret;

	mutex_lock(&data->mutex);

	ret = smi130_gyro_set_power_state(data, state);
	if (ret < 0) {
		mutex_unlock(&data->mutex);
		return ret;
	}
	if (data->dready_trig == trig)
		ret = smi130_gyro_setup_new_data_interrupt(data, state);
	if (ret < 0) {
		smi130_gyro_set_power_state(data, false);
		mutex_unlock(&data->mutex);
		return ret;
	}
	if (data->dready_trig == trig)
		data->dready_trigger_on = state;

	mutex_unlock(&data->mutex);

	return 0;
}

static const struct iio_trigger_ops smi130_gyro_trigger_ops = {
	.set_trigger_state = smi130_gyro_data_rdy_trigger_set_state,
	.try_reenable = NULL,
};

static irqreturn_t smi130_gyro_data_rdy_trig_poll(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct smi130_gyro_data *data = iio_priv(indio_dev);

	if (data->dready_trigger_on)
		iio_trigger_poll(data->dready_trig);

	return IRQ_HANDLED;
}

static int smi130_gyro_buffer_preenable(struct iio_dev *indio_dev)
{
	struct smi130_gyro_data *data = iio_priv(indio_dev);

	return smi130_gyro_set_power_state(data, true);
}

static int smi130_gyro_buffer_postdisable(struct iio_dev *indio_dev)
{
	struct smi130_gyro_data *data = iio_priv(indio_dev);

	return smi130_gyro_set_power_state(data, false);
}

static const struct iio_buffer_setup_ops smi130_gyro_buffer_setup_ops = {
	.preenable = smi130_gyro_buffer_preenable,
	.postenable = iio_triggered_buffer_postenable,
	.predisable = iio_triggered_buffer_predisable,
	.postdisable = smi130_gyro_buffer_postdisable,
};

static const char *smi130_gyro_match_acpi_device(struct device *dev)
{
	const struct acpi_device_id *id;

	id = acpi_match_device(dev->driver->acpi_match_table, dev);
	if (!id)
		return NULL;

	return dev_name(dev);
}

int smi130_gyro_core_probe(struct device *dev, struct regmap *regmap, int irq,
		      const char *name)
{
	struct smi130_gyro_data *data;
	struct iio_dev *indio_dev;
	int ret;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	dev_set_drvdata(dev, indio_dev);
	data->irq = irq;
	data->regmap = regmap;

	ret = smi130_gyro_chip_init(data);
	if (ret < 0)
		return ret;

	mutex_init(&data->mutex);

	if (ACPI_HANDLE(dev))
		name = smi130_gyro_match_acpi_device(dev);

	indio_dev->dev.parent = dev;
	indio_dev->channels = smi130_gyro_channels;
	indio_dev->num_channels = ARRAY_SIZE(smi130_gyro_channels);
	indio_dev->name = name;
	indio_dev->available_scan_masks = smi130_gyro_accel_scan_masks;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &smi130_gyro_info;

	if (data->irq > 0) {
		ret = devm_request_irq(dev,
					data->irq,
					smi130_gyro_data_rdy_trig_poll,
					IRQF_TRIGGER_RISING,
					SMI130_GYRO_IRQ_NAME,
					indio_dev);
		if (ret)
			return ret;

		data->dready_trig = devm_iio_trigger_alloc(dev,
							   "%s-dev%d",
							   indio_dev->name,
							   indio_dev->id);
		if (!data->dready_trig)
			return -ENOMEM;

		data->dready_trig->dev.parent = dev;
		data->dready_trig->ops = &smi130_gyro_trigger_ops;
		iio_trigger_set_drvdata(data->dready_trig, indio_dev);
		ret = iio_trigger_register(data->dready_trig);
		if (ret)
			return ret;

		indio_dev->trig = iio_trigger_get(data->dready_trig);
	}

	ret = iio_triggered_buffer_setup(indio_dev,
					 iio_pollfunc_store_time,
					 smi130_gyro_trigger_handler,
					 &smi130_gyro_buffer_setup_ops);
	if (ret < 0) {
		dev_err(dev,
			"iio triggered buffer setup failed\n");
		goto err_trigger_unregister;
	}

	ret = pm_runtime_set_active(dev);
	if (ret)
		goto err_buffer_cleanup;

	pm_runtime_enable(dev);
	pm_runtime_set_autosuspend_delay(dev,
					 SMI130_GYRO_AUTO_SUSPEND_DELAY_MS);
	pm_runtime_use_autosuspend(dev);

	ret = iio_device_register(indio_dev);
	if (ret < 0) {
		dev_err(dev, "unable to register iio device\n");
		goto err_buffer_cleanup;
	}

	return 0;

err_buffer_cleanup:
	iio_triggered_buffer_cleanup(indio_dev);
err_trigger_unregister:
	if (data->dready_trig)
		iio_trigger_unregister(data->dready_trig);

	return ret;
}
EXPORT_SYMBOL_GPL(smi130_gyro_core_probe);

void smi130_gyro_core_remove(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct smi130_gyro_data *data = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);

	pm_runtime_disable(dev);
	pm_runtime_set_suspended(dev);
	pm_runtime_put_noidle(dev);

	iio_triggered_buffer_cleanup(indio_dev);

	if (data->dready_trig)
		iio_trigger_unregister(data->dready_trig);
}
EXPORT_SYMBOL_GPL(smi130_gyro_core_remove);

#ifdef CONFIG_PM_SLEEP
static int smi130_gyro_suspend(struct device *dev)
{
	return 0;
}

static int smi130_gyro_resume(struct device *dev)
{
	return 0;
}
#endif

#ifdef CONFIG_PM
static int smi130_gyro_runtime_suspend(struct device *dev)
{
	return 0;
}

static int smi130_gyro_runtime_resume(struct device *dev)
{
	msleep_interruptible(SMI130_GYRO_MAX_STARTUP_TIME_MS);

	return 0;
}
#endif

const struct dev_pm_ops smi130_gyro_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(smi130_gyro_suspend, smi130_gyro_resume)
	SET_RUNTIME_PM_OPS(smi130_gyro_runtime_suspend,
			   smi130_gyro_runtime_resume, NULL)
};
EXPORT_SYMBOL_GPL(smi130_gyro_pm_ops);

MODULE_AUTHOR("Chris Baker <chris.l.baker@aptiv.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SMI130 Gyro driver");
