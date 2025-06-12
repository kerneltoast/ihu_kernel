/* SPDX-License-Identifier: GPL */
/*
 * ds90u-wbal.h - gpio interface for TI (formerly National Semiconductor)
 * DS90Ux9xx FPD-Link III serializer/deserializer ICs.
 *
 * Copyright (C) 2014 Delphi Technologies, Inc., All Rights Reserved.
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
#ifndef _LINUX_DS90U_WBAL_H_
#define _LINUX_DS90U_WBAL_H_

#include <linux/i2c.h>

#define NUMBER_OF_LUTS 3
#define COLOR_VALUES 256

#define LUT_FILE_SIZE  (COLOR_VALUES * NUMBER_OF_LUTS)

void ds90u_init_white_bal(struct i2c_client *client);

#endif /* _LINUX_DS90U_WBAL_H_ */
