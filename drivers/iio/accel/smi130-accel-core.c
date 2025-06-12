/*
 * 3-axis accelerometer driver supporting following Bosch-Sensortec chips:
 *  - SMI130
 *
 * Forked from bmc150-accel-core.c
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
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/events.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/regmap.h>

#include "smi130-accel.h"

#define SMI130_ACCEL_IRQ_NAME			"smi130_accel_event"

#define SMI130_ACCEL_REG_CHIP_ID		0x00

#define SMI130_ACCEL_REG_PMU_RANGE		0x0F

#define SMI130_ACCEL_DEF_RANGE_2G		0x03
#define SMI130_ACCEL_DEF_RANGE_4G		0x05
#define SMI130_ACCEL_DEF_RANGE_8G		0x08
#define SMI130_ACCEL_DEF_RANGE_16G		0x0C

/* Default BW: 125Hz */
#define SMI130_ACCEL_REG_PMU_BW		0x10
#define SMI130_ACCEL_DEF_BW			125

#define SMI130_ACCEL_REG_RESET			0x14
#define SMI130_ACCEL_RESET_VAL			0xB6

#define SMI130_ACCEL_REG_INT_MAP_1		0x1A
#define SMI130_ACCEL_INT_MAP_1_BIT_DATA		BIT(7)

#define SMI130_ACCEL_REG_INT_EN_1		0x17
#define SMI130_ACCEL_INT_EN_BIT_DATA_EN		BIT(4)

#define SMI130_ACCEL_REG_XOUT_L		0x02

#define SMI130_ACCEL_MAX_STARTUP_TIME_MS	100

#define SMI130_ACCEL_REG_TEMP			0x08
#define SMI130_ACCEL_TEMP_CENTER_VAL		24

#define SMI130_ACCEL_AXIS_TO_REG(axis)	(SMI130_ACCEL_REG_XOUT_L + (axis * 2))
#define SMI130_AUTO_SUSPEND_DELAY_MS		2000

enum smi130_accel_axis {
	AXIS_X,
	AXIS_Y,
	AXIS_Z,
	AXIS_MAX,
};

struct smi130_scale_info {
	int scale;
	u8 reg_range;
};

struct smi130_accel_chip_info {
	const char *name;
	u8 chip_id;
	const struct iio_chan_spec *channels;
	int num_channels;
	const struct smi130_scale_info scale_table[4];
};

struct smi130_accel_interrupt {
	const struct smi130_accel_interrupt_info *info;
	atomic_t users;
};

struct smi130_accel_trigger {
	struct smi130_accel_data *data;
	struct iio_trigger *indio_trig;
	int (*setup)(struct smi130_accel_trigger *t, bool state);
	int intr;
	bool enabled;
};

enum smi130_accel_interrupt_id {
	SMI130_ACCEL_INT_DATA_READY,
	SMI130_ACCEL_INTERRUPTS,
};

enum smi130_accel_trigger_id {
	SMI130_ACCEL_TRIGGER_DATA_READY,
	SMI130_ACCEL_TRIGGERS,
};

struct smi130_accel_data {
	struct regmap *regmap;
	int irq;
	struct smi130_accel_interrupt interrupts[SMI130_ACCEL_INTERRUPTS];
	struct smi130_accel_trigger triggers[SMI130_ACCEL_TRIGGERS];
	struct mutex mutex;
	s16 buffer[8];
	u8 bw_bits;
	u32 range;
	int64_t timestamp;
	const struct smi130_accel_chip_info *chip_info;
};

static const struct {
	int val;
	int val2;
	u8 bw_bits;
} smi130_accel_samp_freq_table[] = { {15, 620000, 0x08},
				     {31, 260000, 0x09},
				     {62, 500000, 0x0A},
				     {125, 0, 0x0B},
				     {250, 0, 0x0C},
				     {500, 0, 0x0D},
				     {1000, 0, 0x0E},
				     {2000, 0, 0x0F} };

static const struct {
	int bw_bits;
	int msec;
} smi130_accel_sample_upd_time[] = { {0x08, 64},
				     {0x09, 32},
				     {0x0A, 16},
				     {0x0B, 8},
				     {0x0C, 4},
				     {0x0D, 2},
				     {0x0E, 1},
				     {0x0F, 1} };

const struct regmap_config smi130_regmap_conf = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x3f,
};
EXPORT_SYMBOL_GPL(smi130_regmap_conf);

static int smi130_accel_set_bw(struct smi130_accel_data *data, int val,
			       int val2)
{
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(smi130_accel_samp_freq_table); ++i) {
		if (smi130_accel_samp_freq_table[i].val == val &&
		    smi130_accel_samp_freq_table[i].val2 == val2) {
			ret = regmap_write(data->regmap,
				SMI130_ACCEL_REG_PMU_BW,
				smi130_accel_samp_freq_table[i].bw_bits);
			if (ret < 0)
				return ret;

			data->bw_bits =
				smi130_accel_samp_freq_table[i].bw_bits;
			return 0;
		}
	}

	return -EINVAL;
}

static int smi130_accel_get_bw(struct smi130_accel_data *data, int *val,
			       int *val2)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smi130_accel_samp_freq_table); ++i) {
		if (smi130_accel_samp_freq_table[i].bw_bits == data->bw_bits) {
			*val = smi130_accel_samp_freq_table[i].val;
			*val2 = smi130_accel_samp_freq_table[i].val2;
			return IIO_VAL_INT_PLUS_MICRO;
		}
	}

	return -EINVAL;
}

#ifdef CONFIG_PM
static int smi130_accel_get_startup_times(struct smi130_accel_data *data)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smi130_accel_sample_upd_time); ++i) {
		if (smi130_accel_sample_upd_time[i].bw_bits == data->bw_bits)
			return smi130_accel_sample_upd_time[i].msec;
	}

	return SMI130_ACCEL_MAX_STARTUP_TIME_MS;
}

static int smi130_accel_set_power_state(struct smi130_accel_data *data, bool on)
{
	struct device *dev = regmap_get_device(data->regmap);
	int ret;

	if (on) {
		ret = pm_runtime_get_sync(dev);
	} else {
		pm_runtime_mark_last_busy(dev);
		ret = pm_runtime_put_autosuspend(dev);
	}

	if (ret < 0) {
		dev_err(dev,
			"Failed: smi130_accel_set_power_state for %d\n", on);
		if (on)
			pm_runtime_put_noidle(dev);

		return ret;
	}

	return 0;
}
#else
static int smi130_accel_set_power_state(struct smi130_accel_data *data, bool on)
{
	return 0;
}
#endif

static const struct smi130_accel_interrupt_info {
	u8 map_reg;
	u8 map_bitmask;
	u8 en_reg;
	u8 en_bitmask;
} smi130_accel_interrupts[SMI130_ACCEL_INTERRUPTS] = {
	{ /* data ready interrupt */
		.map_reg = SMI130_ACCEL_REG_INT_MAP_1,
		.map_bitmask = SMI130_ACCEL_INT_MAP_1_BIT_DATA,
		.en_reg = SMI130_ACCEL_REG_INT_EN_1,
		.en_bitmask = SMI130_ACCEL_INT_EN_BIT_DATA_EN,
	},
};

static void smi130_accel_interrupts_setup(struct iio_dev *indio_dev,
					  struct smi130_accel_data *data)
{
	int i;

	for (i = 0; i < SMI130_ACCEL_INTERRUPTS; i++)
		data->interrupts[i].info = &smi130_accel_interrupts[i];
}

static int smi130_accel_set_interrupt(struct smi130_accel_data *data, int i,
				      bool state)
{
	struct device *dev = regmap_get_device(data->regmap);
	struct smi130_accel_interrupt *intr = &data->interrupts[i];
	const struct smi130_accel_interrupt_info *info = intr->info;
	int ret;

	dev_dbg(dev, "%s\n", __func__);

	if (state) {
		if (atomic_inc_return(&intr->users) > 1)
			return 0;
	} else {
		if (atomic_dec_return(&intr->users) > 0)
			return 0;
	}

	/*
	 * We will expect the enable and disable to do operation in reverse
	 * order. This will happen here anyway, as our resume operation uses
	 * sync mode runtime pm calls. The suspend operation will be delayed
	 * by autosuspend delay.
	 * So the disable operation will still happen in reverse order of
	 * enable operation. When runtime pm is disabled the mode is always on,
	 * so sequence doesn't matter.
	 */
	ret = smi130_accel_set_power_state(data, state);
	if (ret < 0)
		return ret;

	/* map the interrupt to the appropriate pins */
	ret = regmap_update_bits(data->regmap, info->map_reg, info->map_bitmask,
				 (state ? info->map_bitmask : 0));
	if (ret < 0) {
		dev_err(dev, "Error updating reg_int_map\n");
		goto out_fix_power_state;
	}

	/* enable/disable the interrupt */
	ret = regmap_update_bits(data->regmap, info->en_reg, info->en_bitmask,
				 (state ? info->en_bitmask : 0));
	if (ret < 0) {
		dev_err(dev, "Error updating reg_int_en\n");
		goto out_fix_power_state;
	}

	return 0;

out_fix_power_state:
	smi130_accel_set_power_state(data, false);
	return ret;
}

static int smi130_accel_set_scale(struct smi130_accel_data *data, int val)
{
	struct device *dev = regmap_get_device(data->regmap);
	int ret, i;

	for (i = 0; i < ARRAY_SIZE(data->chip_info->scale_table); ++i) {
		if (data->chip_info->scale_table[i].scale == val) {
			ret = regmap_write(data->regmap,
				     SMI130_ACCEL_REG_PMU_RANGE,
				     data->chip_info->scale_table[i].reg_range);
			if (ret < 0) {
				dev_err(dev, "Error writing pmu_range\n");
				return ret;
			}

			data->range = data->chip_info->scale_table[i].reg_range;
			return 0;
		}
	}

	return -EINVAL;
}

static int smi130_accel_get_temp(struct smi130_accel_data *data, int *val)
{
	struct device *dev = regmap_get_device(data->regmap);
	int ret;
	unsigned int value;

	mutex_lock(&data->mutex);

	ret = regmap_read(data->regmap, SMI130_ACCEL_REG_TEMP, &value);
	if (ret < 0) {
		dev_err(dev, "Error reading reg_temp\n");
		mutex_unlock(&data->mutex);
		return ret;
	}
	*val = sign_extend32(value, 7);

	mutex_unlock(&data->mutex);

	return IIO_VAL_INT;
}

static int smi130_accel_get_axis(struct smi130_accel_data *data,
				 struct iio_chan_spec const *chan,
				 int *val)
{
	struct device *dev = regmap_get_device(data->regmap);
	int ret;
	int axis = chan->scan_index;
	__le16 raw_val;

	mutex_lock(&data->mutex);
	ret = smi130_accel_set_power_state(data, true);
	if (ret < 0) {
		mutex_unlock(&data->mutex);
		return ret;
	}

	ret = regmap_bulk_read(data->regmap, SMI130_ACCEL_AXIS_TO_REG(axis),
			       &raw_val, sizeof(raw_val));
	if (ret < 0) {
		dev_err(dev, "Error reading axis %d\n", axis);
		smi130_accel_set_power_state(data, false);
		mutex_unlock(&data->mutex);
		return ret;
	}
	*val = sign_extend32(le16_to_cpu(raw_val) >> chan->scan_type.shift,
			     chan->scan_type.realbits - 1);
	ret = smi130_accel_set_power_state(data, false);
	mutex_unlock(&data->mutex);
	if (ret < 0)
		return ret;

	return IIO_VAL_INT;
}

static int smi130_accel_read_raw(struct iio_dev *indio_dev,
				 struct iio_chan_spec const *chan,
				 int *val, int *val2, long mask)
{
	struct smi130_accel_data *data = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		switch (chan->type) {
		case IIO_TEMP:
			return smi130_accel_get_temp(data, val);
		case IIO_ACCEL:
			if (iio_buffer_enabled(indio_dev))
				return -EBUSY;
			else
				return smi130_accel_get_axis(data, chan, val);
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_OFFSET:
		if (chan->type == IIO_TEMP) {
			*val = SMI130_ACCEL_TEMP_CENTER_VAL;
			return IIO_VAL_INT;
		} else {
			return -EINVAL;
		}
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		switch (chan->type) {
		case IIO_TEMP:
			*val2 = 500000;
			return IIO_VAL_INT_PLUS_MICRO;
		case IIO_ACCEL:
		{
			int i;
			const struct smi130_scale_info *si;
			int st_size = ARRAY_SIZE(data->chip_info->scale_table);

			for (i = 0; i < st_size; ++i) {
				si = &data->chip_info->scale_table[i];
				if (si->reg_range == data->range) {
					*val2 = si->scale;
					return IIO_VAL_INT_PLUS_MICRO;
				}
			}
			return -EINVAL;
		}
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_SAMP_FREQ:
		mutex_lock(&data->mutex);
		ret = smi130_accel_get_bw(data, val, val2);
		mutex_unlock(&data->mutex);
		return ret;
	default:
		return -EINVAL;
	}
}

static int smi130_accel_write_raw(struct iio_dev *indio_dev,
				  struct iio_chan_spec const *chan,
				  int val, int val2, long mask)
{
	struct smi130_accel_data *data = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		mutex_lock(&data->mutex);
		ret = smi130_accel_set_bw(data, val, val2);
		mutex_unlock(&data->mutex);
		break;
	case IIO_CHAN_INFO_SCALE:
		if (val)
			return -EINVAL;

		mutex_lock(&data->mutex);
		ret = smi130_accel_set_scale(data, val2);
		mutex_unlock(&data->mutex);
		return ret;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static IIO_CONST_ATTR_SAMP_FREQ_AVAIL(
		"15.620000 31.260000 62.50000 125 250 500 1000 2000");

static struct attribute *smi130_accel_attributes[] = {
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
	NULL,
};

static const struct attribute_group smi130_accel_attrs_group = {
	.attrs = smi130_accel_attributes,
};

#define SMI130_ACCEL_CHANNEL(_axis, bits) {				\
	.type = IIO_ACCEL,						\
	.modified = 1,							\
	.channel2 = IIO_MOD_##_axis,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),			\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) |		\
				BIT(IIO_CHAN_INFO_SAMP_FREQ),		\
	.scan_index = AXIS_##_axis,					\
	.scan_type = {							\
		.sign = 's',						\
		.realbits = (bits),					\
		.storagebits = 16,					\
		.shift = 16 - (bits),					\
		.endianness = IIO_LE,					\
	},								\
}

#define SMI130_ACCEL_CHANNELS(bits) {					\
	{								\
		.type = IIO_TEMP,					\
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |		\
				      BIT(IIO_CHAN_INFO_SCALE) |	\
				      BIT(IIO_CHAN_INFO_OFFSET),	\
		.scan_index = -1,					\
	},								\
	SMI130_ACCEL_CHANNEL(X, bits),					\
	SMI130_ACCEL_CHANNEL(Y, bits),					\
	SMI130_ACCEL_CHANNEL(Z, bits),					\
	IIO_CHAN_SOFT_TIMESTAMP(3),					\
}

static const struct iio_chan_spec smi130_accel_channels[] =
	SMI130_ACCEL_CHANNELS(12);

static const struct smi130_accel_chip_info smi130_accel_chip_info_tbl[] = {
	[smi130] = {
		.name = "SMI130A",
		.chip_id = 0xFA,
		.channels = smi130_accel_channels,
		.num_channels = ARRAY_SIZE(smi130_accel_channels),
		.scale_table = { {9610, SMI130_ACCEL_DEF_RANGE_2G},
				 {19122, SMI130_ACCEL_DEF_RANGE_4G},
				 {38344, SMI130_ACCEL_DEF_RANGE_8G},
				 {76590, SMI130_ACCEL_DEF_RANGE_16G} },
	},
};

static const struct iio_info smi130_accel_info = {
	.attrs			= &smi130_accel_attrs_group,
	.read_raw		= smi130_accel_read_raw,
	.write_raw		= smi130_accel_write_raw,
};

static const unsigned long smi130_accel_scan_masks[] = {
					BIT(AXIS_X) | BIT(AXIS_Y) | BIT(AXIS_Z),
					0};

static irqreturn_t smi130_accel_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct smi130_accel_data *data = iio_priv(indio_dev);
	int ret;

	mutex_lock(&data->mutex);
	ret = regmap_bulk_read(data->regmap, SMI130_ACCEL_REG_XOUT_L,
			       data->buffer, AXIS_MAX * 2);
	mutex_unlock(&data->mutex);
	if (ret < 0)
		goto err_read;

	iio_push_to_buffers_with_timestamp(indio_dev, data->buffer,
					   pf->timestamp);
err_read:
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static int smi130_accel_trigger_set_state(struct iio_trigger *trig,
					  bool state)
{
	struct smi130_accel_trigger *t = iio_trigger_get_drvdata(trig);
	struct smi130_accel_data *data = t->data;
	int ret;

	mutex_lock(&data->mutex);

	if (t->enabled == state) {
		mutex_unlock(&data->mutex);
		return 0;
	}

	if (t->setup) {
		ret = t->setup(t, state);
		if (ret < 0) {
			mutex_unlock(&data->mutex);
			return ret;
		}
	}

	ret = smi130_accel_set_interrupt(data, t->intr, state);
	if (ret < 0) {
		mutex_unlock(&data->mutex);
		return ret;
	}

	t->enabled = state;

	mutex_unlock(&data->mutex);

	return ret;
}

static const struct iio_trigger_ops smi130_accel_trigger_ops = {
	.set_trigger_state = smi130_accel_trigger_set_state,
	.try_reenable = NULL,
};

static irqreturn_t smi130_accel_irq_handler(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct smi130_accel_data *data = iio_priv(indio_dev);
	bool ack = false;
	int i;

	data->timestamp = iio_get_time_ns(indio_dev);

	for (i = 0; i < SMI130_ACCEL_TRIGGERS; i++) {
		if (data->triggers[i].enabled) {
			iio_trigger_poll(data->triggers[i].indio_trig);
			ack = true;
			break;
		}
	}

	if (ack)
		return IRQ_HANDLED;

	return IRQ_NONE;
}

static const struct {
	int intr;
	const char *name;
	int (*setup)(struct smi130_accel_trigger *t, bool state);
} smi130_accel_triggers[SMI130_ACCEL_TRIGGERS] = {
	{
		.intr = 0,
		.name = "%s-dev%d",
	},
};

static void smi130_accel_unregister_triggers(struct smi130_accel_data *data,
					     int from)
{
	int i;

	for (i = from; i >= 0; i--) {
		if (data->triggers[i].indio_trig) {
			iio_trigger_unregister(data->triggers[i].indio_trig);
			data->triggers[i].indio_trig = NULL;
		}
	}
}

static int smi130_accel_triggers_setup(struct iio_dev *indio_dev,
				       struct smi130_accel_data *data)
{
	struct device *dev = regmap_get_device(data->regmap);
	int i, ret;

	for (i = 0; i < SMI130_ACCEL_TRIGGERS; i++) {
		struct smi130_accel_trigger *t = &data->triggers[i];

		t->indio_trig = devm_iio_trigger_alloc(dev,
					smi130_accel_triggers[i].name,
						       indio_dev->name,
						       indio_dev->id);
		if (!t->indio_trig) {
			ret = -ENOMEM;
			break;
		}

		t->indio_trig->dev.parent = dev;
		t->indio_trig->ops = &smi130_accel_trigger_ops;
		t->intr = smi130_accel_triggers[i].intr;
		t->data = data;
		t->setup = smi130_accel_triggers[i].setup;
		iio_trigger_set_drvdata(t->indio_trig, t);

		ret = iio_trigger_register(t->indio_trig);
		if (ret)
			break;

		if (0==i)
			indio_dev->trig = iio_trigger_get(t->indio_trig);
	}

	if (ret)
		smi130_accel_unregister_triggers(data, i - 1);

	return ret;
}

static int smi130_accel_buffer_preenable(struct iio_dev *indio_dev)
{
	struct smi130_accel_data *data = iio_priv(indio_dev);

	return smi130_accel_set_power_state(data, true);
}

static int smi130_accel_buffer_postenable(struct iio_dev *indio_dev)
{
	if (indio_dev->currentmode == INDIO_BUFFER_TRIGGERED)
		return iio_triggered_buffer_postenable(indio_dev);

	return 0;
}

static int smi130_accel_buffer_predisable(struct iio_dev *indio_dev)
{
	if (indio_dev->currentmode == INDIO_BUFFER_TRIGGERED)
		return iio_triggered_buffer_predisable(indio_dev);

	return 0;
}

static int smi130_accel_buffer_postdisable(struct iio_dev *indio_dev)
{
	struct smi130_accel_data *data = iio_priv(indio_dev);

	return smi130_accel_set_power_state(data, false);
}

static const struct iio_buffer_setup_ops smi130_accel_buffer_ops = {
	.preenable = smi130_accel_buffer_preenable,
	.postenable = smi130_accel_buffer_postenable,
	.predisable = smi130_accel_buffer_predisable,
	.postdisable = smi130_accel_buffer_postdisable,
};

static int smi130_accel_chip_init(struct smi130_accel_data *data)
{
	struct device *dev = regmap_get_device(data->regmap);
	int ret, i;
	unsigned int val;

	/*
	 * Reset chip to get it in a known good state. A delay of 1.8ms after
	 * reset is required according to the data sheets of supported chips.
	 */
	regmap_write(data->regmap, SMI130_ACCEL_REG_RESET,
		     SMI130_ACCEL_RESET_VAL);
	usleep_range(1800, 2500);

	ret = regmap_read(data->regmap, SMI130_ACCEL_REG_CHIP_ID, &val);
	if (ret < 0) {
		dev_err(dev, "Error: Reading chip id\n");
		return ret;
	}

	dev_dbg(dev, "Chip Id %x\n", val);
	for (i = 0; i < ARRAY_SIZE(smi130_accel_chip_info_tbl); i++) {
		if (smi130_accel_chip_info_tbl[i].chip_id == val) {
			data->chip_info = &smi130_accel_chip_info_tbl[i];
			break;
		}
	}

	if (!data->chip_info) {
		dev_err(dev, "Invalid chip %x\n", val);
		return -ENODEV;
	}

	/* Set Bandwidth */
	ret = smi130_accel_set_bw(data, SMI130_ACCEL_DEF_BW, 0);
	if (ret < 0)
		return ret;

	/* Set Default Range */
	ret = regmap_write(data->regmap, SMI130_ACCEL_REG_PMU_RANGE,
			   SMI130_ACCEL_DEF_RANGE_4G);
	if (ret < 0) {
		dev_err(dev, "Error writing reg_pmu_range\n");
		return ret;
	}

	data->range = SMI130_ACCEL_DEF_RANGE_4G;

	return 0;
}

int smi130_accel_core_probe(struct device *dev, struct regmap *regmap, int irq,
			    const char *name)
{
	struct smi130_accel_data *data;
	struct iio_dev *indio_dev;
	int ret;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	dev_set_drvdata(dev, indio_dev);
	data->irq = irq;

	data->regmap = regmap;

	ret = smi130_accel_chip_init(data);
	if (ret < 0)
		return ret;

	mutex_init(&data->mutex);

	indio_dev->dev.parent = dev;
	indio_dev->channels = data->chip_info->channels;
	indio_dev->num_channels = data->chip_info->num_channels;
	indio_dev->name = name ? name : data->chip_info->name;
	indio_dev->available_scan_masks = smi130_accel_scan_masks;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &smi130_accel_info;

	ret = iio_triggered_buffer_setup(indio_dev,
					 &iio_pollfunc_store_time,
					 smi130_accel_trigger_handler,
					 &smi130_accel_buffer_ops);
	if (ret < 0) {
		dev_err(dev, "Failed: iio triggered buffer setup\n");
		return ret;
	}

	if (data->irq > 0) {
		ret = devm_request_irq(
					dev, data->irq,
					smi130_accel_irq_handler,
					IRQF_TRIGGER_RISING,
					SMI130_ACCEL_IRQ_NAME,
					indio_dev);
		if (ret)
			goto err_buffer_cleanup;

		smi130_accel_interrupts_setup(indio_dev, data);

		ret = smi130_accel_triggers_setup(indio_dev, data);
		if (ret)
			goto err_buffer_cleanup;
	}

	ret = pm_runtime_set_active(dev);
	if (ret)
		goto err_trigger_unregister;

	pm_runtime_enable(dev);
	pm_runtime_set_autosuspend_delay(dev, SMI130_AUTO_SUSPEND_DELAY_MS);
	pm_runtime_use_autosuspend(dev);

	ret = iio_device_register(indio_dev);
	if (ret < 0) {
		dev_err(dev, "Unable to register iio device\n");
		goto err_trigger_unregister;
	}

	return 0;

err_trigger_unregister:
	smi130_accel_unregister_triggers(data, SMI130_ACCEL_TRIGGERS - 1);
err_buffer_cleanup:
	iio_triggered_buffer_cleanup(indio_dev);

	return ret;
}
EXPORT_SYMBOL_GPL(smi130_accel_core_probe);

int smi130_accel_core_remove(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct smi130_accel_data *data = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);

	pm_runtime_disable(dev);
	pm_runtime_set_suspended(dev);
	pm_runtime_put_noidle(dev);

	smi130_accel_unregister_triggers(data, SMI130_ACCEL_TRIGGERS - 1);

	iio_triggered_buffer_cleanup(indio_dev);

	return 0;
}
EXPORT_SYMBOL_GPL(smi130_accel_core_remove);

#ifdef CONFIG_PM_SLEEP
static int smi130_accel_suspend(struct device *dev)
{
	return 0;
}

static int smi130_accel_resume(struct device *dev)
{
	return 0;
}
#endif

#ifdef CONFIG_PM
static int smi130_accel_runtime_suspend(struct device *dev)
{
	dev_dbg(dev,  __func__);

	return 0;
}

static int smi130_accel_runtime_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct smi130_accel_data *data = iio_priv(indio_dev);
	int sleep_val;

	dev_dbg(dev,  __func__);

	sleep_val = smi130_accel_get_startup_times(data);
	if (sleep_val < 20)
		usleep_range(sleep_val * 1000, 20000);
	else
		msleep_interruptible(sleep_val);

	return 0;
}
#endif

const struct dev_pm_ops smi130_accel_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(smi130_accel_suspend, smi130_accel_resume)
	SET_RUNTIME_PM_OPS(smi130_accel_runtime_suspend,
			   smi130_accel_runtime_resume, NULL)
};
EXPORT_SYMBOL_GPL(smi130_accel_pm_ops);

MODULE_AUTHOR("Chris Baker <chris.l.baker@aptiv.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SMI130 accelerometer driver");
