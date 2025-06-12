/* SPDX-License-Identifier: GPL */
/*
 * ds90u-gpio-seq.h - driver for ds90u gpio sequencing
 * DS90Ux9xx FPD-Link III serializer/deserializer ICs.
 *
 * Copyright (C) 2014-2017 Delphi Technologies, Inc., All Rights Reserved.
 * Copyright (C) 2018-2019 Aptiv, All Rights Reserved.
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
#ifndef _LINUX_DS90U_GPIO_SEQ_H_
#define _LINUX_DS90U_GPIO_SEQ_H_

#include <linux/i2c.h>

extern struct i2c_driver ds90_gpioseq_driver;

#endif
