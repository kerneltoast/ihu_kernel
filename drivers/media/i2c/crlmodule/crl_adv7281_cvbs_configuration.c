/*
 * Copyright (c) 2016--2017 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/pm_runtime.h>
#include "crlmodule.h"
#include "crlmodule-regs.h"
#include <linux/delay.h>

#define ADV7281_SENSOR_POWER_UP_DELAY        200 /* ms */
#define ADV7281_SENSOR_INTERRUPT_DELAY       100 /* ms */

#define ADV7281_SENSOR_DIAG_PIN_1              0x5D // DIAG1 Control Register
#define ADV7281_SENSOR_DIAG_PIN_2              0x5E // DIAG2 Control Register
#define ADV7281_SENSOR_ADICONTROL              0x0E // ADI Control 1 Register
#define ADV7281_SENSOR_INTRQ_OP_SEL            0x40 // Interrupt Configuration 1 Register
#define ADV7281_SENSOR_DIAG_TRI1_L1_MSK        0x55 // Interrupt Mask 5 Register
#define ADV7281_SENSOR_DIAG_TRI1_L1_CLR        0x54 // Interrupt Clear 5 Register
#define ADV7281_SENSOR_DIAG_TRI1_L1_ST         0x53 // Interrupt Status 5 Register
#define ADV7281_SENSOR_POWER_MANAGEMENT        0x0F // Power management Register
#define ADV7281_SENSOR_STATUS                  0x10 // Status 1 Register
#define ADV7281_IDENTIFICATION                 0x11 // Identification Register
#define ADV7281_SENSOR_DEFAULT_VALUE_Y         0x0C // Default Value Y Register
#define ADV7281_SENSOR_ANALOG_CLAMP_CONTROL    0x14 // Analog clamp control Register

#define ADV7281_SENSOR_SET_SINGLE_COLOR        0x10 // Analog clamp control Single color Value
#define ADV7281_SENSOR_SET_COLOR_BARS          0x11 // Analog clamp control Color Bars Value
#define ADV7281_SENSOR_SET_LUMA_RAMP           0x12 // Analog clamp control Luma Ramp Value
#define ADV7281_SENSOR_SET_BOUNDARY_BOX        0x15 // Analog clamp control Boundary Box Value
#define ADV7281_SENSOR_GET_COLOR_BARS          0x01 // Analog clamp control Color Bars read Value
#define ADV7281_SENSOR_GET_LUMA_RAMP           0x02 // Analog clamp control Luma Ramp read Value
#define ADV7281_SENSOR_GET_SINGLE_COLOR        0x00 // Analog clamp control Single color read Value
#define ADV7281_SENSOR_GET_BOUNDARY_BOX        0x05 // Analog clamp control Boundary Box read Value
#define ADV7281_SENSOR_RUN_MODE_MASK           0x01 // DEF_VAL_EN mask Value
#define ADV7281_SENSOR_VIDEO_CMD_MIN_SIZE      0x01 // MIN count size
#define ADV7281_SENSOR_DIAG_SLICE_LEVEL        0x1C // DIAG1 Control Value
#define ADV7281_SENSOR_ACCESS_INTERRUPT_SPACE  0x20 // ADI Control 1 Value
#define ADV7281_SENSOR_ACCESS_INTERRUPT_CFG    0xD1 // Interrupt Configuration 1 Value
#define ADV7281_SENSOR_ACCESS_INTERRUPT_MSK    0x0A // Interrupt Clear 5 Value
#define ADV7281_SENSOR_READ_I2C_ADDRESS        0x43 // I2C Address for reading
#define ADV7281_SENSOR_LOW_LINE                0x02 // ADI Control 1 Read Low Value
#define ADV7281_SENSOR_HIGH_LINE               0x08 // ADI Control 1 Read High Value
#define ADV7281_SENSOR_DEF_VAL_EN_LOW          0x36 // DEF_VAL_EN Low Value
#define ADV7281_SENSOR_DEF_VAL_EN_HIGH         0x37 // DEF_VAL_EN High Value


/**
 * adv7281_sensor_init_stb_interrupt() -
 * Initialize Short to Battery detection Interrupt
 * @dev:	specific device
 */
static void adv7281_sensor_init_stb_interrupt(struct device *dev)
{
	struct crl_register_read_rep reg;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);

	reg.len = CRL_REG_LEN_08BIT;
	reg.mask = 0xff;
	reg.dev_i2c_addr = CRL_I2C_ADDRESS_NO_OVERRIDE;

	/* Configure interrupt
	42 5D 1C ; Enable Diagnostic pin 1 - level 1.125V */
	reg.address = ADV7281_SENSOR_DIAG_PIN_1;
	crlmodule_write_reg(sensor, reg.dev_i2c_addr, reg.address,
			reg.len, reg.mask, ADV7281_SENSOR_DIAG_SLICE_LEVEL);
	/* 42 5E 1C ; Enable Diagnostic pin 1 - level 1.125V */
	reg.address = ADV7281_SENSOR_DIAG_PIN_2;
	crlmodule_write_reg(sensor, reg.dev_i2c_addr, reg.address,
			reg.len, reg.mask, ADV7281_SENSOR_DIAG_SLICE_LEVEL);
	/* 42 0E 20 ; Enter Interrupt Map 01 */
	reg.address = ADV7281_SENSOR_ADICONTROL;
	crlmodule_write_reg(sensor, reg.dev_i2c_addr, reg.address,
			reg.len, reg.mask, ADV7281_SENSOR_ACCESS_INTERRUPT_SPACE);
	/* 42 40 D1 ; set INTRQ pin to drive low when active and remain low until cleared */
	reg.address = ADV7281_SENSOR_INTRQ_OP_SEL;
	crlmodule_write_reg(sensor, reg.dev_i2c_addr, reg.address,
			reg.len, reg.mask, ADV7281_SENSOR_ACCESS_INTERRUPT_CFG);
	/* 42 55 0A ; Unmask Diagnostic Interrupts */
	reg.address = ADV7281_SENSOR_DIAG_TRI1_L1_MSK;
	crlmodule_write_reg(sensor, reg.dev_i2c_addr, reg.address,
			reg.len, reg.mask, ADV7281_SENSOR_ACCESS_INTERRUPT_MSK);
	/* 42 54 0A ; Clear Diagnostic Interrupts */
	reg.address = ADV7281_SENSOR_DIAG_TRI1_L1_CLR;
	crlmodule_write_reg(sensor, reg.dev_i2c_addr, reg.address,
			reg.len, reg.mask, ADV7281_SENSOR_ACCESS_INTERRUPT_MSK);
	/* 42 0E 00 ; Enter User Map */
	reg.address = ADV7281_SENSOR_ADICONTROL;
	crlmodule_write_reg(sensor, reg.dev_i2c_addr, reg.address,
			reg.len, reg.mask, 0x00);

}
/**
 * adv7281_sensor_show_inputstatus() - Update input line status
 * @dev:	specific device
 * @attr:	device attributes
 * @buf:	device file buffer
 */
static ssize_t adv7281_sensor_show_inputstatus(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int link_status = 1;
	u32 read_val = 0;
	u32 read_int = 0;
	char *normal = "2";
	char *disconnected = "3";
	char *stb= "0";
	int ret = 0;

	struct crl_register_read_rep reg;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);

	reg.address = ADV7281_SENSOR_POWER_MANAGEMENT;
	reg.len = CRL_REG_LEN_08BIT;
	reg.mask = 0xff;
	reg.dev_i2c_addr = CRL_I2C_ADDRESS_NO_OVERRIDE;
	ret = crlmodule_read_reg(sensor, reg, &read_val);
	if (ret < 0) {
		dev_err(&client->dev, "Reading ADV7281_SENSOR_POWER_MANAGEMENT Error: %d\n", ret);
		return ret;
	}
	if (read_val != 0) {
		pm_runtime_put(&client->dev);
		ret = crlmodule_write_regs(sensor, sensor->sensor_ds->powerup_regs,
						    sensor->sensor_ds->powerup_regs_items);
		if (ret < 0) {
			dev_err(&client->dev, "Writing powerup regs Error: %d\n", ret);
			return ret;
		}
		msleep(ADV7281_SENSOR_POWER_UP_DELAY);
	}

	reg.address = ADV7281_SENSOR_STATUS;
	ret = crlmodule_read_reg(sensor, reg, &read_val);
	if (ret < 0) {
		dev_err(&client->dev, "Reading ADV7281_SENSOR_STATUS Error: %d\n", ret);
		return ret;
	}
	dev_dbg(&client->dev, "adv7281_sensor_show_inputstatus: %d\n", read_val);
	link_status= read_val & 0x01;

	reg.address = ADV7281_SENSOR_ADICONTROL;
	ret = crlmodule_write_reg(sensor, reg.dev_i2c_addr, reg.address,
			reg.len, reg.mask, ADV7281_SENSOR_ACCESS_INTERRUPT_SPACE);
	if (ret < 0) {
		dev_err(&client->dev, "Writing ADV7281_SENSOR_ADICONTROL Error: %d\n", ret);
		return ret;
	}

	reg.address = ADV7281_SENSOR_DIAG_TRI1_L1_ST;
	reg.dev_i2c_addr = ADV7281_SENSOR_READ_I2C_ADDRESS;
	ret = crlmodule_read_reg(sensor, reg, &read_int);
	if (ret < 0) {
		dev_err(&client->dev, "Reading ADV7281_SENSOR_DIAG_TRI1_L1_ST Error: %d\n", ret);
		return ret;
	}
	if (read_int != 0){/* interrupt detected */
		reg.dev_i2c_addr = CRL_I2C_ADDRESS_NO_OVERRIDE;
		reg.address = ADV7281_SENSOR_DIAG_TRI1_L1_CLR;
		ret = crlmodule_write_reg(sensor, reg.dev_i2c_addr, reg.address,
				reg.len, reg.mask, ADV7281_SENSOR_ACCESS_INTERRUPT_MSK);
		if (ret < 0) {
			dev_err(&client->dev, "Writing ADV7281_SENSOR_DIAG_TRI1_L1_CLR Error: %d\n", ret);
			return ret;
		}
		msleep(ADV7281_SENSOR_INTERRUPT_DELAY);
	}
	reg.address = ADV7281_SENSOR_DIAG_TRI1_L1_ST;
	reg.dev_i2c_addr = ADV7281_SENSOR_READ_I2C_ADDRESS;
	ret = crlmodule_read_reg(sensor, reg, &read_int);
	if (ret < 0) {
		dev_err(&client->dev, "Reading ADV7281_SENSOR_DIAG_TRI1_L1_ST Error: %d\n", ret);
		return ret;
	}
	dev_dbg(&client->dev, "interrupt status: %d\n", read_int);
	if (read_int != 0){/* confirm interrupt detected */
		reg.dev_i2c_addr = CRL_I2C_ADDRESS_NO_OVERRIDE;
		reg.address = ADV7281_SENSOR_DIAG_TRI1_L1_CLR;
		ret = crlmodule_write_reg(sensor, reg.dev_i2c_addr, reg.address,
				reg.len, reg.mask, ADV7281_SENSOR_ACCESS_INTERRUPT_MSK);
		if (ret < 0) {
			dev_err(&client->dev, "Writing ADV7281_SENSOR_DIAG_TRI1_L1_CLR Error: %d\n", ret);
			return ret;
		}

		reg.address = ADV7281_SENSOR_ADICONTROL;
		ret = crlmodule_write_reg(sensor, reg.dev_i2c_addr, reg.address,
				reg.len, reg.mask, 0x00);
		if (ret < 0) {
			dev_err(&client->dev, "Writing ADV7281_SENSOR_ADICONTROL Error: %d\n", ret);
			return ret;
		}

		dev_dbg(&client->dev, "STB interrupt detected: %d\n", read_int);

		if (read_int == ADV7281_SENSOR_LOW_LINE)
			return sprintf(buf, "%s%s\n", stb,disconnected);
		else if (read_int == ADV7281_SENSOR_HIGH_LINE)
			return sprintf(buf, "%s%s\n", disconnected,stb);
		else
			return sprintf(buf, "%s%s\n", stb,stb);
	} else {
		reg.dev_i2c_addr = CRL_I2C_ADDRESS_NO_OVERRIDE;
		reg.address = ADV7281_SENSOR_ADICONTROL;
		ret = crlmodule_write_reg(sensor, reg.dev_i2c_addr, reg.address,
				reg.len, reg.mask, 0x00);
		if (ret < 0) {
			dev_err(&client->dev, "Writing ADV7281_SENSOR_ADICONTROL Error: %d\n", ret);
			return ret;
		}
		if (link_status == 1)
			return sprintf(buf, "%s%s\n", normal,normal);
		else
			return sprintf(buf, "%s%s\n", disconnected,disconnected);
	}
}

/**
 * adv7281_sensor_show_videopattern() - Update video pattern status
 * @dev:	specific device
 * @attr:	device attributes
 * @buf:	device file buffer
 */
static ssize_t adv7281_sensor_show_videopattern(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u32 read_val = 0;
	int ret = 0;
	const char *normal = "0";
	const char *color_bars = "1";
	const char *luma_ramp = "2";
	const char *single_color = "3";
	const char *boundary_box = "4";
	const char *pattern;
	struct crl_register_read_rep reg;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);

	reg.address = ADV7281_SENSOR_DEFAULT_VALUE_Y;
	reg.len = CRL_REG_LEN_08BIT;
	reg.mask = 0xff;
	reg.dev_i2c_addr = CRL_I2C_ADDRESS_NO_OVERRIDE;
	ret = crlmodule_read_reg(sensor, reg, &read_val);
	if (ret < 0) {
		dev_err(&client->dev, "Reading ADV7281_SENSOR_DEFAULT_VALUE_Y Error: %d\n", ret);
		return ret;
	}
	read_val = read_val & ADV7281_SENSOR_RUN_MODE_MASK;
	if (read_val == 0){
		pattern = normal;
	} else {
		reg.address = ADV7281_SENSOR_ANALOG_CLAMP_CONTROL;
		ret = crlmodule_read_reg(sensor, reg, &read_val);
		if (ret < 0) {
			dev_err(&client->dev, "Reading ADV7281_SENSOR_ANALOG_CLAMP_CONTROL Error: %d\n", ret);
			return ret;
		}
		read_val = read_val & 0x0f;
		pattern = normal;
		if(read_val == ADV7281_SENSOR_GET_COLOR_BARS)
			pattern = color_bars;
		if(read_val == ADV7281_SENSOR_GET_LUMA_RAMP)
			pattern = luma_ramp;
		if(read_val == ADV7281_SENSOR_GET_SINGLE_COLOR)
			pattern = single_color;
		if(read_val == ADV7281_SENSOR_GET_BOUNDARY_BOX)
			pattern = boundary_box;
	}
	return sprintf(buf, "%s\n", pattern);
}

/**
 * adv7281_sensor_store_videopattern() - Set video pattern
 * @dev:	specific device
 * @attr:	device attributes
 * @buf:	device file buffer
 */
static ssize_t adv7281_sensor_store_videopattern(struct device *dev,
			struct device_attribute *attr,
			const char *buf,
			size_t count)
{
	char val;
	int pattern;
	int ret = 0;
	const char normal = '0';
	const char color_bars = '1';
	const char luma_ramp = '2';
	const char single_color = '3';
	const char boundary_box = '4';
	struct crl_register_read_rep reg;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);

	if (count >= ADV7281_SENSOR_VIDEO_CMD_MIN_SIZE) {
		val  =  buf[0];
		reg.len = CRL_REG_LEN_08BIT;
		reg.mask = 0xff;
		reg.dev_i2c_addr = CRL_I2C_ADDRESS_NO_OVERRIDE;

		if(val == normal){
			reg.address = ADV7281_SENSOR_DEFAULT_VALUE_Y;
			ret = crlmodule_write_reg(sensor, reg.dev_i2c_addr, reg.address,
					reg.len, reg.mask, ADV7281_SENSOR_DEF_VAL_EN_LOW);
			if (ret < 0) {
				dev_err(&client->dev, "Writing ADV7281_SENSOR_DEFAULT_VALUE_Y Error: %d\n", ret);
			}

			pattern = ADV7281_SENSOR_SET_SINGLE_COLOR;
			reg.address = ADV7281_SENSOR_ANALOG_CLAMP_CONTROL;
			ret = crlmodule_write_reg(sensor, reg.dev_i2c_addr, reg.address,
					reg.len, reg.mask, pattern);
			if (ret < 0) {
				dev_err(&client->dev, "Writing ADV7281_SENSOR_ANALOG_CLAMP_CONTROL Error: %d\n", ret);
			}
		} else {
			if(val == color_bars)
				pattern = ADV7281_SENSOR_SET_COLOR_BARS;
			if(val == luma_ramp)
				pattern = ADV7281_SENSOR_SET_LUMA_RAMP;
			if(val == single_color)
				pattern = ADV7281_SENSOR_SET_SINGLE_COLOR;
			if(val == boundary_box)
				pattern = ADV7281_SENSOR_SET_BOUNDARY_BOX;

			reg.address = ADV7281_SENSOR_DEFAULT_VALUE_Y;
			ret = crlmodule_write_reg(sensor, reg.dev_i2c_addr, reg.address,
					reg.len, reg.mask, ADV7281_SENSOR_DEF_VAL_EN_HIGH);
			if (ret < 0) {
				dev_err(&client->dev, "Writing ADV7281_SENSOR_DEFAULT_VALUE_Y Error: %d\n", ret);
			}
			reg.address = ADV7281_SENSOR_ANALOG_CLAMP_CONTROL;
			ret = crlmodule_write_reg(sensor, reg.dev_i2c_addr, reg.address,
					reg.len, reg.mask, pattern);
			if (ret < 0) {
				dev_err(&client->dev, "Writing ADV7281_SENSOR_ANALOG_CLAMP_CONTROL Error: %d\n", ret);
			}
		}
	} else {
		dev_err(&client->dev,
				"Error input chars less than supported count: %ld \n", count);
	}

	return count;
}
/**
 * adv7281_sensor_show_hardwareid() - Update Hardware id
 * @dev:	specific device
 * @attr:	device attributes
 * @buf:	device file buffer
 */
static ssize_t adv7281_sensor_show_hardwareid(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u32 read_val = 0;
	int ret = 0;

	struct crl_register_read_rep reg;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);

	reg.address = ADV7281_IDENTIFICATION;
	reg.len = CRL_REG_LEN_08BIT;
	reg.mask = 0xff;
	reg.dev_i2c_addr = CRL_I2C_ADDRESS_NO_OVERRIDE;

	ret = crlmodule_read_reg(sensor, reg, &read_val);
	if (ret < 0) {
		dev_err(&client->dev, "Reading ADV7281_IDENTIFICATION Error: %d\n", ret);
		return ret;
	}

	dev_dbg(&client->dev, "adv7281_IDENTIFICATION: %d\n", read_val);

	return sprintf(buf, "%ld\n", read_val);
}
/*---- Define Device-Attribute for input line status----------*/
static DEVICE_ATTR(link_status, S_IRUGO, adv7281_sensor_show_inputstatus, NULL);
static DEVICE_ATTR(video_pattern,(S_IRUGO | S_IWUSR),
		adv7281_sensor_show_videopattern, adv7281_sensor_store_videopattern);
static DEVICE_ATTR(hardware_id, S_IRUGO, adv7281_sensor_show_hardwareid, NULL);

/**
 * adv7281_sensor_sysfs_register() - driver register sysfs status device
 * @dev:	device-specific
 */
static int adv7281_sensor_sysfs_register(struct device *dev)
{
	int err;
	err = device_create_file(dev, &dev_attr_link_status);
	device_create_file(dev, &dev_attr_video_pattern);
	device_create_file(dev, &dev_attr_hardware_id);
	return err;
}

/**
 * adv7281_sensor_sysfs_unregister() - driver unregister sysfs status device
 * @dev:	device-specific
 */
static void adv7281_sensor_sysfs_unregister(struct device *dev)
{
	device_remove_file(dev, &dev_attr_link_status);
	device_remove_file(dev, &dev_attr_video_pattern);
	device_remove_file(dev, &dev_attr_hardware_id);
}

/**
 * adv7281_sensor_init() - Initialize sensor
 * @dev:	client-specific
 */
int adv7281_sensor_init(struct i2c_client *client)
{
	int res;
	res = adv7281_sensor_sysfs_register(&client->dev);
	adv7281_sensor_init_stb_interrupt(&client->dev);
	return res;
}

/**
 * adv7281_sensor_init() - Sensor cleanup
 * @dev:	client-specific
 */
int adv7281_sensor_cleanup(struct i2c_client *client)
{
	adv7281_sensor_sysfs_unregister(&client->dev);
	return 0;
}
