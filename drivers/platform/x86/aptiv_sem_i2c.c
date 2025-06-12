/*
 * aptiv_sem_i2c.c
 *
 * Platform drivers for I2C devices on Aptiv VGTT SEM hardware.
 * Copyright (C) 2018 Aptiv
 * Authors: Chris Baker <chris.l.baker@aptiv.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/platform_data/ds90u.h>
#include <linux/platform_data/leds-tlc591xx.h>
#include <linux/platform_data/leds-lp8860.h>
#include <linux/platform_data/atmel_mxt_ts.h>
#include <linux/platform_data/ds90u-gpioseq.h>
#include <uapi/linux/input-event-codes.h>

#define LOCAL_BUS   CONFIG_MFD_INTEL_LPSS_PCI_NUMBER_OF_I2C_BUS_2
#define SERIALIZER_BUS CONFIG_DELPHI_LVDS_VIRT_I2C_BUS_NUM
#define DESERIALIZER_BUS  CONFIG_DELPHI_LVDS_VIRT_I2C_REMOTE_BUS_NUM

#define ST_ACCEL_GYRO_BUS	CONFIG_MFD_INTEL_LPSS_PCI_NUMBER_OF_I2C_BUS_3
#define ST_ACCEL_GYRO_ADDRESS	0x6a
#define ST_ACCEL_GYRO_NAME	"asm330lhh"

#define BOSCH_ACCEL_GYRO_BUS	CONFIG_MFD_INTEL_LPSS_PCI_NUMBER_OF_I2C_BUS_3
#define BOSCH_ACCEL_ADDRESS	0x18
#define BOSCH_ACCEL_NAME	"smi130_accel"
#define BOSCH_GYRO_ADDRESS	0x68
#define BOSCH_GYRO_NAME		"smi130_gyro"

/*
 * Saved Stoneridge display backlight brightness.
 * Used for restoring display backlight brightness on SID reconnection.
 */
static enum led_brightness sid_display_backlight_saved_brightness;
static DEFINE_MUTEX(sid_display_backlight_saved_brightness_lock);
enum led_brightness sid_display_backlight_get_saved_brightness(void);
void sid_display_backlight_save_brightness(enum led_brightness);

/*
 * Saved Stoneridge buttons backlight brightness.
 * Used for restoring buttons backlight brightness on SID reconnection.
 */
static enum led_brightness sid_buttons_backlight_saved_brightness;
static DEFINE_MUTEX(sid_buttons_backlight_saved_brightness_lock);
enum led_brightness sid_buttons_backlight_get_saved_brightness(void);
void sid_buttons_backlight_save_brightness(enum led_brightness);
static int sid_mxt_ts_pos_correction(int max_y);
static int sid_mxt_ts_virtual_key_init(struct device * dev);
static void sid_mxt_ts_virtual_key_close(void);

/* configuration for Stoneridge SID */
/* serializer */
static struct ds90u_platform_data serializer_delphi_929_platdata = {
	.pdb_gpio = 444,
	.lock_gpio = 463,
	.scl_pulse_width = -1, /* don't care */
	.gpio_controller = false,
	.i2c_adapter = true,
	.i2c_adapter_num = SERIALIZER_BUS,
	.i2c_alias_init = {
		{0x2c, 0x2c}, /* remote deserializer */
		{0x2d, 0x2d}, /* display backlight */
		{0x40, 0x40}, /* button backlight */
		{0x4a, 0x4a}, /* atmel touchscreen */
		{0x4b, 0x2c}, /* point dummy tftenable device to deserializer */
	},
	.gpio_init = {
		//[1]=DS90U_GPIO_TRANSMIT,  /* Touchscreen Reset */
	},
	.gpio_deinit = {
		[0]=DS90U_GPIO_DONTTOUCH,
		[1]=DS90U_GPIO_DONTTOUCH,
		[2]=DS90U_GPIO_DONTTOUCH,
		[3]=DS90U_GPIO_DONTTOUCH
	},
	.apply_AVMUTE_errata=true,
	.disable_clock_autodetect=true,
	.apply_temp_ramp_errata=true,
	.verify_hdmi_clock=true,
	.backchannel_watchdog_value=1,
	.apply_resume_delay=400,
	.test_deserializer_mailbox=0x2c
};

/* deserializer */
static struct ds90u_platform_data deserializer_disp_928_platdata = {
	.pdb_gpio = -1,
	.lock_gpio = -1,
	.scl_pulse_width = 25,
	.gpio_controller = true,
	.gpio_base = 200, /* -1 corresponds to 259 */
	.i2c_adapter = true,
	.i2c_adapter_num = DESERIALIZER_BUS,
	.i2c_alias_init = {
	},
	.gpio_init = {
		[0]=DS90U_GPIO_OUTPUT_LOW,  /* GPIO-0: use gpio-chip ds90ub928q@2C index 0 : TFT-enable: start LOW and switch to HIGH via tftenable i2c dummy and gpio-200 */
		[1]=DS90U_GPIO_OUTPUT_LOW,  /* GPIO-1: use gpio-chip ds90ub928q@2C index 1 : Touch Controller Reset (atmel_mxt_ts)  */
		[2]=DS90U_GPIO_OUTPUT_LOW,  /* GPIO-2: use gpio-chip ds90ub928q@2C index 2 : TFT-backlight-enable (lp8860): switch to high via lp8860 and gpio-202 */
		[3]=DS90U_GPIO_INPUT,       /* GPIO-3: Backlight Fault */
	},
	.gpio_deinit = {
		[0]=DS90U_GPIO_OUTPUT_LOW,
		[1]=DS90U_GPIO_DONTTOUCH, /* let the atmel_mxt_ts code do the reset */
		[2]=DS90U_GPIO_OUTPUT_LOW,
		[3]=DS90U_GPIO_DONTTOUCH
	},
	.mapsel_override = false,
	.oen_override=true,
	.oen_value=1,
	.oss_value=1,
	.apply_resume_delay=400
};

static struct mxt_platform_data sid_mxt_ts_platdata = {
	.enable_gpio = 201, /* ds90ub928q@2C gpio 1 */
	.irqflags = IRQF_TRIGGER_LOW,
	.allow_inverted_polarity_reset = true,
	.device_properties_optional = true,
	.self_test_watchdog_period = 30000,
	.max_y_alignment = &sid_mxt_ts_pos_correction,
	.virtual_keys_init = &sid_mxt_ts_virtual_key_init,
	.virtual_keys_close = &sid_mxt_ts_virtual_key_close,
};

static struct lp8860_platform_data sid_display_backlight_platdata = {
	.label = "sid_backlight",
	.enable_gpio = 202, /* ds90ub928q@2C gpio 2 */
	.immutable_eeprom = true,
	.brightness_get_saved = &sid_display_backlight_get_saved_brightness,
	.brightness_save = &sid_display_backlight_save_brightness,
};

static struct tlc591xx_platform_data sid_button_backlight_platdata = {
	.use_group_brightness_mode = true,
	.use_low_current_multiplier = true,
	.brightness_get_saved = &sid_buttons_backlight_get_saved_brightness,
	.brightness_save = &sid_buttons_backlight_save_brightness,
};

static struct ds90_gpioseq_platform_data sid_tftenable_platdata = {
	.label = "ds90ugpioseq",
	.enable_gpio = 200, /* ds90ub928q@2C gpio 0 */
	.delay = 150
};

/* devices that will be added to  adapter busnum LVDS_REMOTE_BUS */
static struct i2c_board_info deserializer_delphi_remote_lvds_devices [] __initdata = {
	{
		I2C_BOARD_INFO("atmel_mxt_ts", 0x4A), /* first touch then backlight (see stoneridge verification document) */
		.platform_data = &sid_mxt_ts_platdata,
	},
	{
		I2C_BOARD_INFO("tlc59108", 0x40),
		.platform_data = &sid_button_backlight_platdata,
	},
	{ /* to be able to sequence tft-enable before tft-backlight enable */
		I2C_BOARD_INFO("ds90ugpioseq", 0x4b), /* add a dummy 0x4b address, mapped to i2c-alias 0x2c to be able to discover */
		.platform_data = &sid_tftenable_platdata,
	},
	{
		I2C_BOARD_INFO("lp8860", 0x2d),
		.platform_data = &sid_display_backlight_platdata,
	},
};

/* devices that will be added to  adapter busnum SERIALIZER_BUS which is the
 * the native i2c-2 */
static struct i2c_board_info local_delphi_i2c_2_devices [] __initdata = {
	{
		I2C_BOARD_INFO("ds90ub929q", 0x1a), /* the serializer */
		.platform_data = &serializer_delphi_929_platdata,
	},
};

/* devices that will be added to  adapter busnum DESERIALIZER_BUS which is the
 * the native i2c-2 */
static struct i2c_board_info serializer_delphi_devices [] __initdata = {
	{
		I2C_BOARD_INFO("ds90ub928q", 0x2C), /* the de-serializer */
		.platform_data = &deserializer_disp_928_platdata,
	},
};

static struct i2c_board_info st_accel_gyro_board_info __initdata = {
	I2C_BOARD_INFO(ST_ACCEL_GYRO_NAME, ST_ACCEL_GYRO_ADDRESS),
	.irq = 0, /* value set up in imu_get_irq_pins */
	.platform_data = NULL, /* defaults to INT1 for drdy interrupt */
};

struct sid_mxt_virtual_key_map_data {
	uint16_t key_code;
	uint16_t center_x;
	uint16_t center_y;
	uint16_t width;
	uint16_t height;
};

static struct kobject *sid_mxt_ts_virtual_keys_map;

/* set up the ST ASM330LLH platform device */
static int __init st_asm330llh_init(void)
{
	int retval = 0;
	struct i2c_adapter *i2c_adap = NULL;

	/* register the device */
	i2c_adap = i2c_get_adapter(ST_ACCEL_GYRO_BUS);
	if (!i2c_adap) {
		pr_err("%s: i2c_get_adapter(%d) failed\n", __func__, ST_ACCEL_GYRO_BUS);
	} else {
		if (!i2c_new_device(i2c_adap, &st_accel_gyro_board_info))
			retval=-ENODEV;

		i2c_put_adapter(i2c_adap);
	}

	return retval;
}

static struct i2c_board_info bosch_smi130_accel_board_info __initdata = {
	I2C_BOARD_INFO(BOSCH_ACCEL_NAME, BOSCH_ACCEL_ADDRESS),
	.irq = 0, /* value set up in imu_get_irq_pins */
};

static struct i2c_board_info bosch_smi130_gyro_board_info __initdata = {
	I2C_BOARD_INFO(BOSCH_GYRO_NAME, BOSCH_GYRO_ADDRESS),
	.irq = 0, /* value set up in imu_get_irq_pins */
};

/* set up the Bosch SMI130 platform devices */
static int __init bosch_smi130_init(void)
{
	int retval = 0;
	struct i2c_adapter *i2c_adap = NULL;

	/* register the devices */
	i2c_adap = i2c_get_adapter(BOSCH_ACCEL_GYRO_BUS);
	if (!i2c_adap) {
		pr_err("%s: i2c_get_adapter(%d) failed\n", __func__, BOSCH_ACCEL_GYRO_BUS);
	} else {
		if (!i2c_new_device(i2c_adap, &bosch_smi130_accel_board_info))
			retval = -ENODEV;

		if (!i2c_new_device(i2c_adap, &bosch_smi130_gyro_board_info))
			retval = -ENODEV;

		i2c_put_adapter(i2c_adap);
	}

	return retval;
}

/*
 * There are two IMU configurations, one with Bosch SMI130 and one with
 * STM ASM330LHH. The IRQ pins are identical for both configurations.
 * This function will request the GPIO pins at a platform level and get the
 * IRQ number for use with either driver.
 */
static int __init imu_get_irq_pins(void)
{
	int imu_int1_num = -1;
	int imu_int2_num = -1;
	struct gpio_desc *imu_int1_desc, *imu_int2_desc;

	/*
	 * Request INT1 IRQ Pin
	 * Bosch SMI130 - Accel IRQ
	 * STM ASM330LHH - DRDY IRQ
	 * Optional: both drivers can run without this but will have limited
	 * functionality.
	 */
	imu_int1_desc = gpiod_get_index_optional(NULL, "i2c2-imu-int", 0,
						GPIOD_IN);
	if (IS_ERR(imu_int1_desc)) {
		pr_warn("%s: gpiod_get(%d) failed (%ld)\n", __func__, 0, PTR_ERR(imu_int1_desc));
	} else {
		imu_int1_num = gpiod_to_irq(imu_int1_desc);
		if (imu_int1_num < 0) {
			pr_warn("%s: gpiod_to_irq(%d) failed (%d)\n", __func__, 0, imu_int1_num);
			gpiod_put(imu_int1_desc);
		} else {
			/* store the irq number in both drivers
			   whichever one actually probes will make use of it */
			bosch_smi130_accel_board_info.irq = imu_int1_num;
			st_accel_gyro_board_info.irq = imu_int1_num;
		}
	}

	/*
	 * Request INT2 IRQ Pin
	 * Bosch SMI130 - Gyro IRQ
	 * STM ASM330LHH - Unused
	 * Optional: the driver can run without this but will have limited
	 * functionality.
	 */
	imu_int2_desc = gpiod_get_index_optional(NULL, "i2c2-imu-int", 1,
						GPIOD_IN);
	if (IS_ERR(imu_int2_desc)) {
		pr_warn("%s: gpiod_get(%d) failed (%ld)\n", __func__, 1, PTR_ERR(imu_int2_desc));
	} else {
		imu_int2_num = gpiod_to_irq(imu_int2_desc);
		if (imu_int2_num < 0) {
			pr_warn("%s: gpiod_to_irq(%d) failed (%d)\n", __func__, 1, imu_int2_num);
			gpiod_put(imu_int2_desc);
		} else {
			/* store the irq number */
			bosch_smi130_gyro_board_info.irq = imu_int2_num;
		}
	}

	return 0; /* always return 0, interrupts are currently optional */
}

/* platform devices initilized at arch_initcall using i2c_register_board_info */
static int __init aptiv_platform_i2c_init(void)
{
	int retval;

	retval = i2c_register_board_info(LOCAL_BUS,
		local_delphi_i2c_2_devices,
		ARRAY_SIZE(local_delphi_i2c_2_devices));
	if (retval)
		printk(KERN_ERR "delphi_vgtt_i2c: i2c_register_board_info(%d)=%d\n", LOCAL_BUS, retval);

	retval = i2c_register_board_info(SERIALIZER_BUS,
		serializer_delphi_devices,
		ARRAY_SIZE(serializer_delphi_devices));
	if (retval)
		printk(KERN_ERR "delphi_vgtt_i2c: i2c_register_board_info(%d)=%d\n", SERIALIZER_BUS, retval);

	retval = i2c_register_board_info(DESERIALIZER_BUS,
		deserializer_delphi_remote_lvds_devices,
		ARRAY_SIZE(deserializer_delphi_remote_lvds_devices));
	if (retval)
		printk(KERN_ERR "delphi_vgtt_i2c: i2c_register_board_info(%d)=%d\n", DESERIALIZER_BUS, retval);

	return retval;
}
arch_initcall(aptiv_platform_i2c_init);

/* platform devices initilized at device_initcall using i2c_new_device */
static int __init aptiv_platform_i2c_device_init(void)
{
	int retval = 0;
	int test = 0;

	test = imu_get_irq_pins();
	if (test) {
		printk(KERN_ERR "%s: imu_get_irq_pins failed (%d)\n", __func__, test);
		retval=test;
	} else {
		test = st_asm330llh_init();
		if (test) {
			printk(KERN_ERR "%s: st_asm330llh_init failed (%d)\n", __func__, test);
			retval=test;
		}

		test = bosch_smi130_init();
		if (test) {
			printk(KERN_ERR "%s: bosch_smi130_init failed (%d)\n", __func__, test);
			retval=test;
		}
	}

	return retval;
}
device_initcall(aptiv_platform_i2c_device_init);

enum led_brightness sid_display_backlight_get_saved_brightness(void)
{
	enum led_brightness saved_brightness;
	mutex_lock(&sid_display_backlight_saved_brightness_lock);
	saved_brightness = sid_display_backlight_saved_brightness;
	mutex_unlock(&sid_display_backlight_saved_brightness_lock);
	return saved_brightness;
}

void sid_display_backlight_save_brightness(enum led_brightness brightness)
{
	mutex_lock(&sid_display_backlight_saved_brightness_lock);
	sid_display_backlight_saved_brightness = brightness;
	mutex_unlock(&sid_display_backlight_saved_brightness_lock);
}

enum led_brightness sid_buttons_backlight_get_saved_brightness(void)
{
	enum led_brightness saved_brightness;
	mutex_lock(&sid_buttons_backlight_saved_brightness_lock);
	saved_brightness = sid_buttons_backlight_saved_brightness;
	mutex_unlock(&sid_buttons_backlight_saved_brightness_lock);
	return saved_brightness;
}

void sid_buttons_backlight_save_brightness(enum led_brightness brightness)
{
	mutex_lock(&sid_buttons_backlight_saved_brightness_lock);
	sid_buttons_backlight_saved_brightness = brightness;
	mutex_unlock(&sid_buttons_backlight_saved_brightness_lock);
}

static int sid_mxt_ts_pos_correction(int max_y)
{
	/* SEM SID touches needs to be realigned
	 * 720 is the value reported by SID B2, C0 and C1.1 samples
	 * 719 is the value reported since SID C2 variant
	 */
	if ((max_y == 720) || (max_y == 719))
		return 660;
	return max_y;
}

static int
sid_mxt_virtual_key_map_fill(const struct sid_mxt_virtual_key_map_data *vkm_data,
			     size_t vkm_data_size, char *buf)
{
	int chars_written = 0;
	int i;

	for (i = 0; i < vkm_data_size; ++i)
		chars_written += scnprintf(buf + chars_written,
					   PAGE_SIZE - chars_written,
					   "0x01:%hu:%hu:%hu:%hu:%hu\n",
					   vkm_data[i].key_code,
					   vkm_data[i].center_x,
					   vkm_data[i].center_y,
					   vkm_data[i].width,
					   vkm_data[i].height);

	return chars_written;
}

static ssize_t
sid_mxt_virtual_key_map_show(__attribute__((unused)) struct device *dev,
			     __attribute__((unused)) struct device_attribute *attr,
			     char *buf)
{
	const struct sid_mxt_virtual_key_map_data vkm_data[] = {
		{ KEY_BACK,     495, 766, 70, 56 },
		{ KEY_HOMEPAGE, 640, 780, 50, 68 },
		{ KEY_MENU,     785, 768, 60, 56 },
	};

	return sid_mxt_virtual_key_map_fill(vkm_data,
					    sizeof(vkm_data)/sizeof(vkm_data[0]),
					    buf);
}

static struct device_attribute sid_mxt_dev_attr_virtualkeys = {
	.attr = {
		.name = "virtualkeys.Atmel_maXTouch_Touchscreen",
		.mode = S_IRUGO,
	},
	.show = sid_mxt_virtual_key_map_show,
	.store = NULL,
};

static int sid_mxt_ts_virtual_key_init(struct device * dev)
{
	int error;

	sid_mxt_ts_virtual_keys_map = kobject_create_and_add("board_properties",
							     NULL);
	if (!sid_mxt_ts_virtual_keys_map) {
		dev_err(dev, "Failure creating board_properties kobject\n");
		return -1;
	}

	error = sysfs_create_file(sid_mxt_ts_virtual_keys_map,
				  &sid_mxt_dev_attr_virtualkeys.attr);
	if (error) {
		dev_err(dev, "Failure %d creating sysfs entry %s\n",
			error, sid_mxt_dev_attr_virtualkeys.attr.name);
		kobject_put(sid_mxt_ts_virtual_keys_map);
		return error;
	}

	return 0;
}

static void sid_mxt_ts_virtual_key_close(void)
{
	sysfs_remove_file(sid_mxt_ts_virtual_keys_map, &sid_mxt_dev_attr_virtualkeys.attr);
	kobject_put(sid_mxt_ts_virtual_keys_map);
}
