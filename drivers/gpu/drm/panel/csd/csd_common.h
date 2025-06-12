/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __CSD_COMMON_H__
#define __CSD_COMMON_H__

#include <linux/delay.h>

#define CSD_LINE_STATUS_SHORT_BATTERY  0x0
#define CSD_LINE_STATUS_SHORT_GROUND   0x1
#define CSD_LINE_STATUS_NORMAL         0x2
#define CSD_LINE_STATUS_OPEN           0x3
#define CSD_LINE_STATUS_LINE_TO_LINE   0x4
#define CSD_LINE_STATUS_UNKNOWN       0xFF

/* slack used for hrtimers in ns */
#define CSD_HRTIME_DELTA            10000

#define CSD_HDCP_VSYNC_WAIT_TIME       50

enum csd_gpio_test_result {
	CSD_GPIO_TEST_OK,
	CSD_GPIO_TEST_LOW,
	CSD_GPIO_TEST_HIGH,
	CSD_GPIO_TEST_INV,
};

struct csd_data;

struct device *csd_get_device(struct csd_data *csd);

/**
 * csd_usleep() - Sleep for number of us
 * @usecs: number of us to sleep
 *
 * Wrapper around usleep_range() for convenience.
 */
static inline void csd_usleep(unsigned long usecs)
{
	usleep_range(usecs, usecs + CSD_HRTIME_DELTA / 1000);
}

/**
 * csd_msleep() - Sleep for number of ms
 * @msecs: number of ms to sleep
 *
 * Compared to msleep() this version is backed by hrtimers, which makes
 * the timing more stable compared to the jiffies backed msleep().
 */
static inline void csd_msleep(unsigned long msecs)
{
	csd_usleep(msecs * 1000);
}

/**
 * csd_gpio_test_eval() - Helper to evaluate gpio test result
 * @passed_low:  low test passed
 * @passed_high: high test passed
 *
 * Return: Test result summary
 */
static inline enum csd_gpio_test_result csd_gpio_test_eval(bool passed_low,
	bool passed_high)
{
	/* GPIO working */
	if (passed_low && passed_high)
		return CSD_GPIO_TEST_OK;

	/* GPIO line stuck low */
	if (passed_low)
		return CSD_GPIO_TEST_LOW;

	/* GPIO line stuck high */
	if (passed_high)
		return CSD_GPIO_TEST_HIGH;

	/* GPIO line is inverting */
	return CSD_GPIO_TEST_INV;
}

#define CSD_RW_CHECK_ALL     ((const u8 *) -1)

/* Read */

int csd_read_serdes_pc(struct csd_data *csd, u16 reg, u8 *data, const u8 *check,
	u8 size, bool serializer);

/* Read serializer */

static inline int csd_read_ser_pc(struct csd_data *csd, u16 reg, u8 *data,
	const u8 *check, u8 size)
{
	return csd_read_serdes_pc(csd, reg, data, check, size, true);
}

static inline int csd_read_ser_c(struct csd_data *csd, u16 reg, u8 *data,
	u8 size)
{
	return csd_read_ser_pc(csd, reg, data, CSD_RW_CHECK_ALL, size);
}

static inline int csd_read_ser_8pc(struct csd_data *csd, u16 reg, u8 *data,
	u8 check)
{
	return csd_read_ser_pc(csd, reg, data, check ? &check : NULL, 1);
}

static inline int csd_read_ser_8c(struct csd_data *csd, u16 reg, u8 *data)
{
	return csd_read_ser_c(csd, reg, data, 1);
}

/* Read deserializer */

static inline int csd_read_des_pc(struct csd_data *csd, u16 reg, u8 *data,
	const u8 *check, u8 size)
{
	return csd_read_serdes_pc(csd, reg, data, check, size, false);
}

static inline int csd_read_des_c(struct csd_data *csd, u16 reg, u8 *data,
	u8 size)
{
	return csd_read_des_pc(csd, reg, data, CSD_RW_CHECK_ALL, size);
}

static inline int csd_read_des_8pc(struct csd_data *csd, u16 reg, u8 *data,
	u8 check)
{
	return csd_read_des_pc(csd, reg, data, check ? &check : NULL, 1);
}

static inline int csd_read_des_8c(struct csd_data *csd, u16 reg, u8 *data)
{
	return csd_read_des_c(csd, reg, data, 1);
}

/* Write */

int csd_write_serdes_pc(struct csd_data *csd, u16 reg, const u8 *data,
	const u8 *check, u8 size, bool serializer);

/* Write serializer */

static inline int csd_write_ser_pc(struct csd_data *csd, u16 reg,
	const u8 *data, const u8 *check, u8 size)
{
	return csd_write_serdes_pc(csd, reg, data, check, size, true);
}

static inline int csd_write_ser_c(struct csd_data *csd, u16 reg, const u8 *data,
	u8 size)
{
	return csd_write_ser_pc(csd, reg, data, CSD_RW_CHECK_ALL, size);
}

static inline int csd_write_ser_8pc(struct csd_data *csd, u16 reg, u8 data,
	u8 check)
{
	return csd_write_ser_pc(csd, reg, &data, check ? &check : NULL, 1);
}

static inline int csd_write_ser_8c(struct csd_data *csd, u16 reg, u8 data)
{
	return csd_write_ser_c(csd, reg, &data, 1);
}

/* Write deserializer */

int csd_write_des(struct csd_data *csd, u16 reg, const u8 *data, u8 size);

static inline int csd_write_des_pc(struct csd_data *csd, u16 reg,
	const u8 *data, const u8 *check, u8 size)
{
	return csd_write_serdes_pc(csd, reg, data, check, size, false);
}

static inline int csd_write_des_c(struct csd_data *csd, u16 reg, const u8 *data,
	u8 size)
{
	return csd_write_des_pc(csd, reg, data, CSD_RW_CHECK_ALL, size);
}

static inline int csd_write_des_8pc(struct csd_data *csd, u16 reg, u8 data,
	u8 check)
{
	return csd_write_des_pc(csd, reg, &data, check ? &check : NULL, 1);
}

static inline int csd_write_des_8c(struct csd_data *csd, u16 reg, u8 data)
{
	return csd_write_des_c(csd, reg, &data, 1);
}

#endif /* __CSD_COMMON_H__ */
