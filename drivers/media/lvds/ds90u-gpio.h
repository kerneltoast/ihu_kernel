/* SPDX-License-Identifier: GPL */
/*
 * ds90u-gpio.h - gpio interface for TI (formerly National Semiconductor)
 * DS90Ux9xx FPD-Link III serializer/deserializer ICs.
 *
 * Copyright (C) 2014-2017 Delphi Technologies, Inc., All Rights Reserved.
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
#ifndef _LINUX_DS90U_GPIO_H_
#define _LINUX_DS90U_GPIO_H_

#include "ds90u-core.h"

void ds90u_register_gpio_controller(struct ds90u_cdata *cdata);
void ds90u_unregister_gpio_controller(struct ds90u_cdata *cdata);

#endif /* _LINUX_DS90U_GPIO_H_ */
