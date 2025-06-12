/*
 * aptiv_ihu_spi.c
 *
 * Platform drivers for SPI devices on Aptiv IHU board.
 * Copyright (C) 2018 Aptiv
 * Authors: Oliver Barta <oliver.barta@aptiv.com>
 *          Chris Baker <chris.l.baker@aptiv.com>
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
#include <linux/spi/spi.h>

#include <linux/device.h>
#include <linux/platform_device.h>

static struct spi_board_info apl_spi_slaves[] __initdata = {
	/*
	 * For userspace spidev driver, set modalias to "spidev".
	 */
	{
		/* Saturn DAB */
		.modalias = "spidev",
		.platform_data = NULL,
		.controller_data = NULL,
		.irq = 0,
		.max_speed_hz = 5000000, /* heard there were problems with 8 MHz */
		.bus_num = 3,
		.chip_select = 0,
		.mode = SPI_MODE_1,
	}
};

static int __init aptiv_platform_spi_init(void)
{
	int retval;

	retval = spi_register_board_info(apl_spi_slaves, ARRAY_SIZE(apl_spi_slaves));

	return retval;
}
arch_initcall(aptiv_platform_spi_init);
