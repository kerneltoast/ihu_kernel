/*
 * ds90u.h - interface definitions for TI (formerly National Semiconductor)
 * DS90Ux9xx FPD-Link III serializer/deserializer ICs.
 *
 * Copyright (C) 2017 Delphi Technologies, Inc., All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef __LINUX_I2C_DS90U_H_
#define __LINUX_I2C_DS90U_H_

#include <linux/types.h>

/**
 * DS90U_NUM_SLAVE_ALIAS - the number of i2c slave ID -> alias mappings
 */
#define DS90U_NUM_SLAVE_ALIAS	((size_t) 8)

/**
 * DS90U_MAX_NUM_GPIO - number of GPIOs - 925/926 have 9 total, others have 8
 */
#define DS90U_MAX_NUM_GPIO	((size_t) 9)

/**
 * enum ds90u_gpio_states - Possible configuration values
 * @DS90U_GPIO_DISABLED   :	0 - Disabled (normal pin function)
 * @DS90U_GPIO_TRANSMIT   :	3 - Transmit (local input, passthrough to remote output)
 * @DS90U_GPIO_INPUT      :	3 - Local Input (same as Transmit)
 * @DS90U_GPIO_RECIEVE    :	5 - Receive (local output, passthrough from remote input)
 * @DS90U_GPIO_OUTPUT_LOW :	1 - Local Output (defaults low)
 * @DS90U_GPIO_OUTPUT_HIGH:	9 - Local Output (defaults high)
 * @DS90U_GPIO_TRISTATE   :	2 - Tristate (929/940-model only)
 * @DS90U_GPIO_RX_DEFAULT_LOW : 7 - Receive from remote but output low if no link (929-model only)
 * @DS90U_GPIO_RX_DEFAULT_HIGH: F - Receive from remote but output high if no link (929-model only)
 * special deinit cases:
 * @DS90U_GPIO_DONTTOUCH     : dont touch on power-down deinit
 */
enum ds90u_gpio_states {
	DS90U_GPIO_DISABLED    = 0x00,
	DS90U_GPIO_TRANSMIT    = 0x03,
	DS90U_GPIO_INPUT       = 0x03,
	DS90U_GPIO_RECEIVE     = 0x05,
	DS90U_GPIO_OUTPUT_LOW  = 0x01,
	DS90U_GPIO_OUTPUT_HIGH = 0x09,
	DS90U_GPIO_TRISTATE    = 0x02, /* 929/940-model only */
	DS90U_GPIO_RX_DEFAULT_LOW  = 0x07, /* 929-model only */
	DS90U_GPIO_RX_DEFAULT_HIGH = 0x0F, /* 929-model only */

	/* special deinit cases */
	DS90U_GPIO_DONTTOUCH     = 0xFF,
};

/**
 * struct ds90u_alias_map - map between slave i2c address and alias address
 * @alias_address :	Alias address that you want to use locally
 * @remote_address:	Native address for device on remote end
 */
struct ds90u_alias_map {
	uint8_t alias_address;
	uint8_t remote_address;
};
#endif /* __LINUX_I2C_DS90U_H_ */
