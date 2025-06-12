/*
 * Backlight driver for Allegro Microsystems a8522 Backlight Devices
 *
 * Copyright 2016 Delphi Inc.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/leds.h>
#include <linux/of_gpio.h>
#include <linux/firmware.h>
#include <linux/delay.h>

#define BRIGHTNESS_CURVE_LENGTH 16
#define A8522_MAX_BRIGHTNESS 	15
#define A8522_DEFAULT_BRIGHTNESS 15
#define A8522_FAULT_TIMER	15000

#define A8522_GENERAL_FAULTS1 	0x38
#define A8522_GENERAL_FAULTS2 	0x39

#define A8522_POLYPHASE_GROUP 	0x09
#define A8522_LED_SHORT_1 	0x0A
#define A8522_LED_SHORT_3 	0x0B
#define A8522_LED_SHORT_5 	0x0C
#define A8522_LED_SHORT_7 	0x0D

#define A8522_LED_ENABLE1 	0x00
#define A8522_LED_ENABLE2 	0x01
#define A8522_LED_PWM_PERIOD1 	0x02
#define A8522_LED_PWM_PERIOD2 	0x03
#define A8522_LED_OVP   	0x04
#define A8522_BOOST_DERATE 	0x05
#define A8522_FAULT_MODE1 	0x06
#define A8522_FAULT_MODE2 	0x07

#define A8522_REGULATION_HYSTERESIS 0x25
#define A8522_LED_CURRENT1 	0x26
#define A8522_LED_CURRENT2 	0x27
#define A8522_LED_CURRENT3 	0x28
#define A8522_LED_CURRENT4 	0x29
#define A8522_LED_CURRENT5 	0x2A
#define A8522_LED_CURRENT6 	0x2B
#define A8522_LED_CURRENT7 	0x2C
#define A8522_LED_CURRENT8 	0x2D

#define A8522_GPIO_CONTROL 	0x0f

#define A8522_PWM_OUTPUT1_1 	0x10
#define A8522_PWM_OUTPUT1_2 	0x11
#define A8522_PWM_OUTPUT2_1 	0x12
#define A8522_PWM_OUTPUT2_2 	0x13
#define A8522_PWM_OUTPUT3_1 	0x14
#define A8522_PWM_OUTPUT3_2 	0x15
#define A8522_PWM_OUTPUT4_1 	0x16
#define A8522_PWM_OUTPUT4_2 	0x17
#define A8522_PWM_OUTPUT5_1 	0x18
#define A8522_PWM_OUTPUT5_2 	0x19
#define A8522_PWM_OUTPUT6_1 	0x1A
#define A8522_PWM_OUTPUT6_2 	0x1B
#define A8522_PWM_OUTPUT7_1 	0x1C
#define A8522_PWM_OUTPUT7_2 	0x1D
#define A8522_PWM_OUTPUT8_1 	0x1E
#define A8522_PWM_OUTPUT8_2 	0x1F

#define A8522_PWM_UPDATE 	0x24

#define A8522_GENERAL_FAULT_STATUS1 0x38
#define A8522_GENERAL_FAULT_STATUS2 0x39

static int a8522_bl_get_brightness(struct backlight_device *bl);
static int a8522_bl_setup(struct backlight_device *bl);
static int a8522_bl_set(struct backlight_device *bl, int brightness);

/* endiannes is backwards here so that it can be normal in the firmware curve */
const uint16_t defaultDayBrightnessCurve[BRIGHTNESS_CURVE_LENGTH] =
{
	0x0000, /* 0x0000 */
	0x300A, /* 0x0A30 */
	0xA010, /* 0x10A0 */
	0x3017, /* 0x1730 */
	0xC01D, /* 0x1DC0 */
	0x6024, /* 0x2460 */
	0x202B, /* 0x2B20 */
	0xE031, /* 0x31E0 */
	0xA038, /* 0x38A0 */
	0x603F, /* 0x3F60 */
	0x4046, /* 0x4640 */
	0x004D, /* 0x4D00 */
	0xF053, /* 0x53F0 */
	0xD05A, /* 0x5AD0 */
	0xC061, /* 0x61C0 */
	0xE068, /* 0x68E0 */
};

/* endiannes is backwards here so that it can be normal in the firmware curve */
const uint16_t defaultNightBrightnessCurve[BRIGHTNESS_CURVE_LENGTH] =
{
	0x0000, /* 0x0000 */
	0xA300,	/* 0x00A3 */
	0x0A01,	/* 0x010A */
	0x7301,	/* 0x0173 */
	0xDC01,	/* 0x01DC */
	0x4602,	/* 0x0246 */
	0xB202,	/* 0x02B2 */
	0x1E03,	/* 0x031E */
	0x8A03,	/* 0x038A */
	0xF603,	/* 0x03F6 */
	0x6404,	/* 0x0464 */
	0xD004,	/* 0x04D0 */
	0x3F05,	/* 0x053F */
	0xAD05,	/* 0x05AD */
	0x1C06,	/* 0x061C */
	0x8E06,	/* 0x068E */
};

struct a8522_bl {
	struct i2c_client *client;
	struct backlight_device *bl;
	struct mutex lock;
	struct delayed_work fault_timer;
	uint16_t brightness_curve_day[BRIGHTNESS_CURVE_LENGTH];
	uint16_t brightness_curve_night[BRIGHTNESS_CURVE_LENGTH];
	int id;
	int enable_gpio;
	int lcd_gpio;
	int current_brightness;
	int use_day_curve;
};

static int a8522_read(struct i2c_client *client, int reg, uint8_t *val)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev, "failed reading at 0x%02x\n", reg);
		return ret;
	}

	*val = (uint8_t)ret;
	return 0;
}

static int a8522_write(struct i2c_client *client, u8 reg, u8 val)
{
	dev_dbg(&client->dev, "%s - reg=0x%x, val=0x%x\n", __func__,reg,val);
	return i2c_smbus_write_byte_data(client, reg, val);
}

static void fault_work(struct work_struct *work)
{
	struct a8522_bl *data = container_of(work, struct a8522_bl, fault_timer.work);
	struct i2c_client *client = data->client;
	int ret = 0;
	uint8_t val = 0;
	bool faultDetect = false;

	dev_dbg(&client->dev, "%s\n", __func__);

	ret = a8522_read(data->client, A8522_GENERAL_FAULT_STATUS1, &val);
	if (!ret)
	{
		if((val & 0x01) > 0)
		{
			/*fault 1 detected*/
			faultDetect = true;
			pr_info("Fault 1 detected");
		}
		else if((val & 0x40) > 0)
		{
			/*fault 7 detected*/
			faultDetect = true;
			pr_info("Fault 7 detected");
		}
	}
	else
	{
		pr_info("I2C read failed, A8522_GENERAL_FAULT_STATUS1 ret= %d\n", ret);
	}

	ret = a8522_read(data->client, A8522_GENERAL_FAULT_STATUS2, &val);
	if (!ret)
	{
		if((val & 1) > 0x00)
		{
			/*fault 9 detected*/
			faultDetect = true;
			pr_info("Fault 9 detected");
		}
		else if((val & 0x04) > 0)
		{
			/*fault 11 detected*/
			faultDetect = true;
			pr_info("Fault 11 detected");
		}
	}
	else
	{
		pr_info("I2C read failed, A8522_GENERAL_FAULT_STATUS2 ret= %d\n", ret);
	}

	ret = a8522_read(data->client, A8522_LED_OVP, &val);
	if (!ret)
	{
		if(val != 0x18)
		{
			faultDetect = true;
			pr_info("Low Voltage Fault Detected");
		}
	}
	else
	{
		pr_info("I2C read failed, A8522_LED_OVP ret= %d\n", ret);
	}

	if (faultDetect)
	{
		ret = a8522_bl_setup(data->bl);
		a8522_bl_set(data->bl, data->current_brightness);
	}

	schedule_delayed_work(&(data->fault_timer), msecs_to_jiffies(A8522_FAULT_TIMER));
}

static int a8522_bl_setup(struct backlight_device *bl)
{
	struct a8522_bl *data = bl_get_data(bl);
	struct i2c_client *client = data->client;
	int ret = 0;

	dev_dbg(&client->dev, "%s\n", __func__);

	/*
	enable_registers
	I2C General Fault Registers
	*/
	ret |= a8522_write(client, A8522_GENERAL_FAULTS1, 0xff);

	/*
	second_set_of_registers
	Polyphase Grouping, LED Short Detect Threshold
	*/
	ret |= a8522_write(client, A8522_POLYPHASE_GROUP, 0x49);
	ret |= a8522_write(client, A8522_LED_SHORT_1, 0x00);
	ret |= a8522_write(client, A8522_LED_SHORT_3, 0x00);
	ret |= a8522_write(client, A8522_LED_SHORT_5, 0x00);
	ret |= a8522_write(client, A8522_LED_SHORT_7, 0x00);

	/*
	initial_registers
	Led Enable, LED PWM Period, OVP Threshold, Boost Dithering/Thermal Derating, Fault Mode
	*/
	ret |= a8522_write(client, A8522_LED_ENABLE1, 0x00);
	ret |= a8522_write(client, A8522_LED_ENABLE2, 0xFF);
	ret |= a8522_write(client, A8522_LED_PWM_PERIOD1, 0x0D);
	ret |= a8522_write(client, A8522_LED_PWM_PERIOD2, 0x05);
	ret |= a8522_write(client, A8522_LED_OVP, 0x18);
	ret |= a8522_write(client, A8522_BOOST_DERATE, 0x02);
	ret |= a8522_write(client, A8522_FAULT_MODE1, 0x0A);
	ret |= a8522_write(client, A8522_FAULT_MODE2, 0xBE);


	/*
	execution_registers_1
	LED RegulationVoltage and Output Hysteresis, LEDx DC current[1..8]
	*/
	ret |= a8522_write(client, A8522_REGULATION_HYSTERESIS, 0x01);
	ret |= a8522_write(client, A8522_LED_CURRENT1, 0x1D);
	ret |= a8522_write(client, A8522_LED_CURRENT2, 0x1D);
	ret |= a8522_write(client, A8522_LED_CURRENT3, 0x3B);
	ret |= a8522_write(client, A8522_LED_CURRENT4, 0x1D);
	ret |= a8522_write(client, A8522_LED_CURRENT5, 0x1D);
	ret |= a8522_write(client, A8522_LED_CURRENT6, 0x3B);
	ret |= a8522_write(client, A8522_LED_CURRENT7, 0x1D);
	ret |= a8522_write(client, A8522_LED_CURRENT8, 0x1D);

	return ret;
}

void set_default_brightness_curves(struct a8522_bl *data)
{
	memcpy(data->brightness_curve_day, defaultDayBrightnessCurve,
		sizeof(data->brightness_curve_day));
	memcpy(data->brightness_curve_night, defaultNightBrightnessCurve,
		sizeof(data->brightness_curve_night));
}

static void load_brightness_curve(struct a8522_bl * data, const char * filename,
	bool use_day_curve)
{
	struct i2c_client *client = data->client;
	const struct firmware *fw = NULL;
	int ret;

	dev_dbg(&client->dev, "%s\n", __func__);

	dev_info(&client->dev, "Request_firmware, '%s'\n", filename);
	ret = request_firmware(&fw, filename, &(client->dev));
	if (ret ==-0)
	{
		if (fw->size == (BRIGHTNESS_CURVE_LENGTH)*sizeof(uint16_t))
		{
			if (use_day_curve)
				memcpy(data->brightness_curve_day, fw->data,
				  (BRIGHTNESS_CURVE_LENGTH)*sizeof(uint16_t));
			else
				memcpy(data->brightness_curve_night, fw->data,
				  (BRIGHTNESS_CURVE_LENGTH)*sizeof(uint16_t));
		} else {
			dev_info(&client->dev, "Request_firmware size missmatch, size= %zu",fw->size);
		}
		release_firmware(fw);
	}
	else
	{
		dev_info(&client->dev, "Request_firmware Failed, %d",ret);
	}
}

/* Sysfs entries */
static ssize_t a8522_bl_mode_get(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct a8522_bl *data = dev_get_drvdata(dev);
	dev_dbg(dev, "%s\n", __func__);

	if (data->use_day_curve)
		return sprintf(buf, "%s\n", "day");
	else
		return sprintf(buf, "%s\n", "night");
}

static ssize_t a8522_bl_mode_set(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct a8522_bl *data = dev_get_drvdata(dev);
	dev_dbg(dev, "%s\n", __func__);

	if ((sysfs_streq(buf, "day")) || (buf[0] == '1'))
	{
		data->use_day_curve = 1;
	}
	else if ((sysfs_streq(buf, "night")) || (buf[0] == '0'))
	{
		data->use_day_curve = 0;
	}
	else
	{
		dev_info(dev, "invalid option\n");
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR(brightness_mode, 0664, a8522_bl_mode_get,
			a8522_bl_mode_set);

static struct attribute *a8522_bl_attributes[] = {
	&dev_attr_brightness_mode.attr,
	NULL,
};

static const struct attribute_group a8522_bl_attr_group = {
	.attrs = a8522_bl_attributes,
};

static int a8522_bl_set(struct backlight_device *bl, int brightness)
{
	struct a8522_bl *data = bl_get_data(bl);
	struct i2c_client *client = data->client;
	int ret = 0;
	int x = 0;
	uint8_t brl;
	uint8_t brh;

	dev_dbg(&client->dev, "%s\n", __func__);

	/* Brightness Registers */
	if (data->use_day_curve) {
		brh = (data->brightness_curve_day[brightness] & 0x00FF);
		brl = (data->brightness_curve_day[brightness] & 0xFF00) >> 8;
	} else {
		brh = (data->brightness_curve_night[brightness] & 0x00FF);
		brl = (data->brightness_curve_night[brightness] & 0xFF00) >> 8;
	}

	dev_dbg(&client->dev,"Writing brightness, brightness=%d, reg=0x%02x%02x\n", brightness, brh, brl);

	ret |= a8522_write(client, A8522_GPIO_CONTROL, 0x18);

	for (x = 0; x < 16; x+=2)
	{
		ret |= a8522_write(client, A8522_PWM_OUTPUT1_1 + x, brh);
		ret |= a8522_write(client, A8522_PWM_OUTPUT1_2 + x, brl);
	}

	/*
	execution_registers_2
	PWM On-Time Update
	*/
	ret |= a8522_write(client, A8522_PWM_UPDATE, 0x01);

	if (!ret)
		data->current_brightness = brightness;

	return ret;
}

static int a8522_bl_update_status(struct backlight_device *bl)
{
	int brightness = bl->props.brightness;

	if (bl->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	return a8522_bl_set(bl, brightness);
}

static int a8522_bl_get_brightness(struct backlight_device *bl)
{
	struct a8522_bl *data = bl_get_data(bl);
	return data->current_brightness;
}

static const struct backlight_ops a8522_bl_ops = {
	.update_status	= a8522_bl_update_status,
	.get_brightness	= a8522_bl_get_brightness,
};

static int a8522_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct backlight_device *bl;
	struct a8522_bl *data;
	struct backlight_properties props;
	const char * filename = NULL;
	int ret;
	int check;

	dev_dbg(&client->dev, "%s\n", __func__);

	if (!i2c_check_functionality(client->adapter,
					I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "SMBUS Byte Data not Supported\n");
		return -EIO;
	}

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	/* release the chip from reset */
	data->enable_gpio = of_get_named_gpio(client->dev.of_node, "enable-gpio", 0);
	if (data->enable_gpio >= 0) {
		check = devm_gpio_request_one(&client->dev, data->enable_gpio,
				GPIOF_OUT_INIT_LOW|GPIOF_EXPORT_DIR_CHANGEABLE,
				"Enable_backlight");
		if (check < 0) {
			dev_err(&client->dev, "Unable to register enable GPIO err=%d\n", check);
			return check;
		} else {
			dev_dbg(&client->dev, "  enable=%d\n", data->enable_gpio);
		}
	}

	/* enable the lcd*/
	data->lcd_gpio = of_get_named_gpio(client->dev.of_node, "lcd-gpio", 0);
	if (data->lcd_gpio >= 0) {
		check = devm_gpio_request_one(&client->dev, data->lcd_gpio,
				GPIOF_OUT_INIT_LOW|GPIOF_EXPORT_DIR_CHANGEABLE,
				"lcd_on");
		if (check < 0) {
			dev_err(&client->dev, "Unable to register lcd GPIO err=%d\n", check);
			return check;
		} else {
			dev_dbg(&client->dev, "  lcd=%d\n", data->lcd_gpio);
		}

		mdelay(100);
		gpio_set_value_cansleep(data->lcd_gpio, 1);
	}

	if (data->enable_gpio >= 0) {
		mdelay(100);
		gpio_set_value_cansleep(data->enable_gpio, 1);
	}

	data->client = client;
	data->id = id->driver_data;
	data->current_brightness = A8522_DEFAULT_BRIGHTNESS;
	data->use_day_curve = 1;
	i2c_set_clientdata(client, data);

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = A8522_MAX_BRIGHTNESS;

	mutex_init(&data->lock);

	bl = backlight_device_register(dev_name(&client->dev),
			&client->dev, data, &a8522_bl_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&client->dev, "failed to register backlight\n");
		return PTR_ERR(bl);
	}

	bl->props.brightness = A8522_DEFAULT_BRIGHTNESS;

	data->bl = bl;

	set_default_brightness_curves(data);

	ret = of_property_read_string(client->dev.of_node, "day_curve_file",
		&filename);
	if (!ret) {
		load_brightness_curve(data, filename, true);
	} else {
		load_brightness_curve(data, "a8522_day_curve", true);
	}

	ret = of_property_read_string(client->dev.of_node, "night_curve_file",
		&filename);
	if (!ret) {
		load_brightness_curve(data, filename, false);
	} else {
		load_brightness_curve(data, "a8522_night_curve", false);
	}
	msleep(200);

	ret = a8522_bl_setup(bl);
	if (ret) {
		ret = -EIO;
		goto out;
	}

	/* set default brightness */
	/*
	a8522_bl_set(bl,bl->props.brightness);
	*/

	ret = sysfs_create_group(&bl->dev.kobj,
			&a8522_bl_attr_group);
	if (ret) {
		dev_err(&client->dev, "failed to register sysfs\n");
	}

	backlight_update_status(bl);

	/* Schedule the fault timer */
	INIT_DELAYED_WORK(&(data->fault_timer), &fault_work);
	schedule_delayed_work(&(data->fault_timer), msecs_to_jiffies(A8522_FAULT_TIMER));

	dev_info(&client->dev, "%s Backlight\n",
		client->name);

	return 0;

out:
	backlight_device_unregister(bl);
	return ret;
}

static int a8522_remove(struct i2c_client *client)
{
	struct a8522_bl *data = i2c_get_clientdata(client);
	dev_dbg(&client->dev, "%s\n", __func__);

	if (NULL == data) {
		dev_err(&client->dev, "%s: Client Data is NULL\n", __func__);
		return -EPERM;
	}
	cancel_delayed_work_sync(&(data->fault_timer));
	
	backlight_device_unregister(data->bl);

	if (data->enable_gpio >= 0)
	{
		gpio_set_value_cansleep(data->enable_gpio, 0);
		gpio_free(data->enable_gpio);
	}

	if (data->lcd_gpio >= 0)
	{
		gpio_set_value_cansleep(data->lcd_gpio, 0);
		gpio_free(data->lcd_gpio);
	}
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int a8522_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct a8522_bl *data = i2c_get_clientdata(client);

	if (NULL == data) {
		dev_err(&client->dev, "%s: Client Data is NULL\n", __func__);
		return-EPERM;
	}

	if (data->enable_gpio >= 0)
	{
		gpio_set_value_cansleep(data->enable_gpio, 0);
	}

	return 0;
}

static int a8522_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct a8522_bl *data = i2c_get_clientdata(client);

	if (NULL == data) {
		dev_err(&client->dev, "%s: Client Data is NULL\n", __func__);
		return-EPERM;
	}

	if (data->enable_gpio >= 0)
	{
		gpio_set_value_cansleep(data->enable_gpio, 1);
	}

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(a8522_i2c_pm_ops, a8522_i2c_suspend,
			a8522_i2c_resume);

static const struct i2c_device_id a8522_id[] = {
	{ "a8522", 1 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, a8522_id);

static struct i2c_driver a8522_driver = {
	.driver = {
		.name	= KBUILD_MODNAME,
		.pm	= &a8522_i2c_pm_ops,
	},
	.probe    = a8522_probe,
	.remove   = a8522_remove,
	.id_table = a8522_id,
};

module_i2c_driver(a8522_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Patrick Nice <Patrick.d.nice@delphi.com>");
MODULE_DESCRIPTION("a8522 Backlight driver");
MODULE_ALIAS("i2c:a8522-backlight");
