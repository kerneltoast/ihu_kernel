/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __IDT_P91E0A_H__
#define __IDT_P91E0A_H__

#include <linux/regmap.h>

struct idt_p91e0a {
	struct device *dev;
	struct regmap *regmap;
	u8 i2c_addr;
};

#endif	/* __IDT_P91E0A_H__ */
