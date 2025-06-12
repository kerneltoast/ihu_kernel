/*
 * aptiv_ihu_gpio.c
 *
 * GPIO definitions on Aptiv IHU board.
 * Copyright (C) 2018 Aptiv
 * Authors: Oliver Barta <oliver.barta@aptiv.com>
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
#include <linux/gpio/machine.h>

static struct gpiod_lookup_table gpio_table_csd_serial = {
	.dev_id = "serial0-0",
	.table = {
		GPIO_LOOKUP("INT3452:00", 10, "power_down", GPIO_ACTIVE_HIGH),
		GPIO_LOOKUP("INT3452:00", 72, "gmsl_mode", GPIO_ACTIVE_HIGH),
		GPIO_LOOKUP("INT3452:00", 26, "csd_ctrl", GPIO_ACTIVE_HIGH),
		GPIO_LOOKUP("INT3452:01", 34, "csd_int", GPIO_ACTIVE_HIGH),
		GPIO_LOOKUP("INT3452:00",  9, "errb", GPIO_ACTIVE_HIGH),
		{ }
	},
};

static struct gpiod_lookup_table gpio_table_apix = {
	.dev_id = "0-0032",
	.table = {
		/* Reset pin */
		GPIO_LOOKUP_IDX("INT3452:00", 22, "reset", 0, GPIO_ACTIVE_HIGH),
		{ },
	},
};

/* Platform GPIOs (lookup with NULL dev) */
static struct gpiod_lookup_table ihu_platform_gpios = {
	.dev_id = NULL,
	.table = {
		/* IMU interrupt pins shared between
		   ST ASM330LHH and Bosch SMI130 */
		GPIO_LOOKUP_IDX("INT3452:02", 24, "i2c2-imu-int", 0, GPIO_ACTIVE_HIGH),
		GPIO_LOOKUP_IDX("INT3452:00", 55, "i2c2-imu-int", 1, GPIO_ACTIVE_HIGH),
		{ },
	},
};

static int __init aptiv_platform_gpio_init(void)
{
	gpiod_add_lookup_table(&ihu_platform_gpios);

	gpiod_add_lookup_table(&gpio_table_csd_serial);

	gpiod_add_lookup_table(&gpio_table_apix);

	return 0;
}
arch_initcall(aptiv_platform_gpio_init);
