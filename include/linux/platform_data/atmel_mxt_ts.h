/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __LINUX_PLATFORM_DATA_ATMEL_MXT_TS_H
#define __LINUX_PLATFORM_DATA_ATMEL_MXT_TS_H

#include <linux/types.h>

/* The platform data for the Atmel maXTouch touchscreen driver */
struct mxt_platform_data {
	int enable_gpio;
	unsigned long irqflags;
	bool allow_inverted_polarity_reset;
	bool device_properties_optional;
	unsigned int self_test_watchdog_period;
	int (*max_y_alignment)(int max_y);
/**
 * virtual_keys_init() - Perform actions to prepre and introduce virtual keys configuration needed by userspace.
 * @dev: Device
 * Return:
 *  0 on success\n
 *  non-zero otherwise
 */
	int (*virtual_keys_init)(struct device *dev);
/**
 * virtual_keys_close() - Perform actions to clean up virtual keys configuration needed by userspace.
 */
	void (*virtual_keys_close)(void);
};

#endif /* __LINUX_PLATFORM_DATA_ATMEL_MXT_TS_H */
