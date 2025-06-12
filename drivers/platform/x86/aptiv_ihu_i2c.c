/*
 * aptiv_ihu_i2c.c
 *
 * Platform drivers for I2C devices on Aptiv VCC IHU hardware.
 * Copyright (C) 2018 Aptiv
 * Authors: Christopher N. Hesse <christopher.hesse@aptiv.com>
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
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <media/i2c/adv7604.h>

#define ADV7680_BUS		CONFIG_MFD_INTEL_LPSS_PCI_NUMBER_OF_I2C_BUS_1
#define ADV7680_ADDRESS 	0x32
#define ADV7680_NAME 		"adv7680"

#define ST_ACCEL_GYRO_BUS	CONFIG_MFD_INTEL_LPSS_PCI_NUMBER_OF_I2C_BUS_3
#define ST_ACCEL_GYRO_ADDRESS	0x6a
#define ST_ACCEL_GYRO_NAME	"asm330lhh"

#define BOSCH_ACCEL_GYRO_BUS	CONFIG_MFD_INTEL_LPSS_PCI_NUMBER_OF_I2C_BUS_3
#define BOSCH_ACCEL_ADDRESS	0x18
#define BOSCH_ACCEL_NAME	"smi130_accel"
#define BOSCH_GYRO_ADDRESS	0x68
#define BOSCH_GYRO_NAME		"smi130_gyro"

/* configuration for ADV7604/7680 video decoder */
static struct adv76xx_platform_data adv7680_pdata = {
	.inv_vs_pol = 1,
	.inv_hs_pol = 1,
	//.inv_llc_pol = 1,
	//.insert_av_codes = 1,
	.op_656_range = 0,					/* OK */
	.int1_config = ADV76XX_INT1_CONFIG_OPEN_DRAIN,
	.i2c_addresses[ADV7680_PAGE_VFE] = 0x52,
	.i2c_addresses[ADV7680_PAGE_APIX_TX] = 0x0a,
	.i2c_addresses[ADV7680_PAGE_APIX_HDCP_TX] = 0x08,
	.i2c_addresses[ADV7604_PAGE_AVLINK] = 0x42,
	.i2c_addresses[ADV76XX_PAGE_CEC] = 0x40,
	.i2c_addresses[ADV76XX_PAGE_INFOFRAME] = 0x3e,
	.i2c_addresses[ADV7604_PAGE_ESDP] = 0x38,
	.i2c_addresses[ADV7604_PAGE_DPP] = 0x3c,
	.i2c_addresses[ADV76XX_PAGE_AFE] = 0x26,	/* DPLL */
	.i2c_addresses[ADV76XX_PAGE_REP] = 0x3A,	/* KSV */
	.i2c_addresses[ADV76XX_PAGE_EDID] = 0x36,
	.i2c_addresses[ADV76XX_PAGE_HDMI] = 0x34,
	.i2c_addresses[ADV76XX_PAGE_TEST] = 0x30,
	.i2c_addresses[ADV76XX_PAGE_CP] = 0x22,
	.i2c_addresses[ADV7604_PAGE_VDP] = 0x24,
	.disable_pwrdnb = 0,
	.disable_cable_det_rst = 0,
	.default_input = 0,
	.blank_data = 1,
	.alt_data_sat = 0,					/* OK */
	//.op_format_mode_sel = ADV7604_OP_FORMAT_MODE0,	/* OK */
	.bus_order = ADV7604_BUS_ORDER_RGB,
	//.dr_str_data = ADV76XX_DR_STR_HIGH,
	//.dr_str_clk = ADV76XX_DR_STR_HIGH,
	//.dr_str_sync = ADV76XX_DR_STR_HIGH,
	//.hdmi_free_run_mode = 1,
};

static struct i2c_board_info apix_adv7680_board_info __initdata = {
	I2C_BOARD_INFO(ADV7680_NAME, ADV7680_ADDRESS),
	.platform_data = &adv7680_pdata,
};

static struct i2c_board_info st_accel_gyro_board_info __initdata = {
	I2C_BOARD_INFO(ST_ACCEL_GYRO_NAME, ST_ACCEL_GYRO_ADDRESS),
	.irq = 0, /* value set up in imu_get_irq_pins */
	.platform_data = NULL, /* defaults to INT1 for drdy interrupt */
};

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
	int retval = 0;
#if IS_ENABLED(CONFIG_VIDEO_ADV7604)
	i2c_register_board_info(ADV7680_BUS, &apix_adv7680_board_info, 1);
#endif

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
