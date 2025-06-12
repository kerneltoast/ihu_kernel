/*
 * aptiv_sem_gpio.c
 *
 * GPIO definitions on Aptiv VGTT SEM board.
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
#include <linux/gpio/machine.h>

/* Eth/BroadReach switch BCM89230 GPIOs */
static struct gpiod_lookup_table vgtt_bcm89230_gpios = {
	.dev_id = "spi1.0",
	.table = {
		/* reset line for Eth/BroadReach switch BCM89230 */
		GPIO_LOOKUP("INT3452:00", 9, "bcm89230-reset", GPIO_ACTIVE_HIGH),
		{ },
	},
};

static struct gpiod_lookup_table gpio_table_atmel_mxt_ts = {
	.dev_id = "9-004a",
	.table = {
		GPIO_LOOKUP("INT3452:00", 30, "mxt_irq", GPIO_ACTIVE_HIGH),
		/* Use GPIO_ACTIVE_LOW for SID C samples as a default setting.
		   For retrying with inverted polarity for SID B samples
		   allow_inverted_polarity_reset field of
		   mxt_platform_data has to be set to true. */
		GPIO_LOOKUP("INT3452:00", 32, "reset", GPIO_ACTIVE_LOW),
		{ },
	},
};

/* Platform GPIOs (lookup with NULL dev) for SEM 5.0 and later */
static struct gpiod_lookup_table vgtt_5_0_platform_gpios = {
	.dev_id = NULL,
	.table = {
		/* IMU interrupt pins shared between
		   ST ASM330LHH and Bosch SMI130 */
		GPIO_LOOKUP_IDX("INT3452:02", 19, "i2c2-imu-int", 0, GPIO_ACTIVE_HIGH),
		GPIO_LOOKUP_IDX("INT3452:02", 25, "i2c2-imu-int", 1, GPIO_ACTIVE_HIGH),
		{ },
	},
};

static int __init aptiv_platform_gpio_init(void)
{
	gpiod_add_lookup_table(&vgtt_5_0_platform_gpios);
	gpiod_add_lookup_table(&vgtt_bcm89230_gpios);
	gpiod_add_lookup_table(&gpio_table_atmel_mxt_ts);

	return 0;
}
arch_initcall(aptiv_platform_gpio_init);
