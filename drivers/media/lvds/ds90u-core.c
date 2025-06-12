// SPDX-License-Identifier: GPL
/*
 * ds90u-core.c - driver core for TI (formerly National Semiconductor)
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

/**
 * DOC: ds90u-core
 *
 * This module is intended to handle i2c communication with TI (formerly
 * National Semiconductor) FPD-Link III ICs.
 *
 * This module handles the probing and configuration of the device.
 * Since these ICs provide several interfaces to a host SOC, I expect there
 * will eventually be several related modules (i2s, v4l, gpio, etc).
 * The most important module is the i2c module which provides register access
 * and intitialization and the i2c-adapter module which creates an adapter for
 * accessing other i2c devices across the LVDS link.
 *
 * I believe that all part numbers beginning with ds90u are i2c-compatible,
 * for the most part. But, there may be exceptions that I haven't encountered.
 * Double check the probe function and register definitions when adding a new
 * chip to the DS90U_DEVICE_IDS list.
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_data/ds90u.h>
#include <linux/i2c/ds90u.h>
#include "ds90u-core.h"
#include "ds90u-i2c.h"
#include "ds90u-gpio-seq.h"
#include "ds90u-init.h"

static int ds90u_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int ds90u_remove(struct i2c_client *client);
static int ds90u_suspend(struct device *dev);
static int ds90u_resume(struct device *dev);
static void ds90u_set_pdb(struct i2c_client *client, int value);

/**
 * DS90U_LINK_POLL_MS - polling delay for checking serializer link loss
 */
#define DS90U_LINK_POLL_MS (2000U)

/**
 * DS90U_RXID_BASE_REG - base register for the RXID field
 * This field is handled specially because it is used for device
 * type autodetection.
 */
#define DS90U_RXID_BASE_REG ((uint8_t) 0xF0)
#define DS90U_RXID_LEN      (6)

/**
 * DS90U_ANA_IA - DS90U Indirect Access Registers
 */
#define DS90U_ANA_IA_CNTL                   ((uint8_t) 0x40)
#define DS90U_ANA_IA_ADDR                   ((uint8_t) 0x41)
#define DS90U_ANA_IA_DATA                   ((uint8_t) 0x42)

#define DS90U_ANA_IA_READ                   ((uint8_t) 0x01)
#define DS90U_ANA_IA_WRITE                  ((uint8_t) 0x0)

#define DS90U_OLDI_FPD_PLL_SM_CTL           ((uint8_t) 0x49)
#define DS90U_OLDI_PLL_PPM_CNT              ((uint8_t) 0x4A)
#define DS90U_ANA_IA_CNTL_OLDI              ((uint8_t) 0x10)
#define DS90U_ANA_IA_CNTL_FPD               ((uint8_t) 0x14)

#define DS90U_ANA_IA_CNTL_OLDI_READ         (DS90U_ANA_IA_CNTL_OLDI | \
					     DS90U_ANA_IA_READ)
#define DS90U_ANA_IA_CNTL_OLDI_WRITE        (DS90U_ANA_IA_CNTL_OLDI | \
					     DS90U_ANA_IA_WRITE)
#define DS90U_ANA_IA_CNTL_FPD_READ          (DS90U_ANA_IA_CNTL_FPD | \
					     DS90U_ANA_IA_READ)
#define DS90U_ANA_IA_CNTL_FPD_WRITE         (DS90U_ANA_IA_CNTL_FPD | \
					     DS90U_ANA_IA_WRITE)

#define DS90U_OLDI_FPD_PLL_SM_DIS_RESET     ((uint8_t) 0x00)
#define DS90U_OLDI_FPD_PLL_SM_EN_RESET      ((uint8_t) 0x10)

#define DS90U_PWR_UP_STATE_MACHINE_CFG      ((uint8_t) 0x23)
#define DS90U_STATE_MACHINE_CAP             ((uint8_t) 0x24)
#define DS90U_HDMI_VERIFY_MAX_RETRY         (100)

/**
 * struct ds90u_rxid_map - Lookup table for auto-detecting the device type
 * @rxid:	6-byte string that will be checked against the RX ID register
 * @type:	Type that indicates the register map and device type
 */
struct ds90u_rxid_map {
	char rxid[DS90U_RXID_LEN];
	enum ds90u_node_type type;
};


/* Map ds90u_node_type to serializer true/false */
static const int ds90u_type_is_serializer[DS90U_MAX_TYPE] = {
	1, /* DS90U_SER0 */
	1, /* DS90U_SER925 */
	0, /* DS90U_DESER0 */
	0, /* DS90U_DESER926 */
};

/*
 * List of device IDs handled by this driver.
 * Each entry should be an X_DS90U_ID.
 *   X_DS90U_ID(device-table-name, auto-detect-id, ds90u_node_type)
 *
 * The auto-detect-id is a 6-byte string that will be checked against
 * the RX ID register as a method of auto-detecting the chip variant.
 *
 * Note that the auto-detect feature will probably not work for devices
 * across the FPD-Link connection.
 */
#define DS90U_DEVICE_IDS \
	X_DS90U_ID("ds90ub925q", "_UB925", DS90U_SER925) \
	X_DS90U_ID("ds90uh925q", "_UH925", DS90U_SER925) \
	X_DS90U_ID("ds90ub927q", "_UB927", DS90U_SER0) \
	X_DS90U_ID("ds90uh927q", "_UH927", DS90U_SER0) \
	X_DS90U_ID("ds90ub929q", "_UB929", DS90U_SER0) \
	X_DS90U_ID("ds90uh929q", "_UH929", DS90U_SER0) \
	X_DS90U_ID("ds90ub941q", "_UB941", DS90U_SER0) \
	X_DS90U_ID("ds90uh941q", "_UH941", DS90U_SER0) \
	X_DS90U_ID("ds90ub949q", "_UB949", DS90U_SER0) \
	X_DS90U_ID("ds90uh949q", "_UH949", DS90U_SER0) \
	X_DS90U_ID("ds90ub926q", "_UB926", DS90U_DESER926) \
	X_DS90U_ID("ds90uh926q", "_UH926", DS90U_DESER926) \
	X_DS90U_ID("ds90ub928q", "_UB928", DS90U_DESER0) \
	X_DS90U_ID("ds90uh928q", "_UH928", DS90U_DESER0) \
	X_DS90U_ID("ds90ub940q", "_UB940", DS90U_DESER0) \
	X_DS90U_ID("ds90uh940q", "_UH940", DS90U_DESER0) \
	X_DS90U_ID("ds90ub948q", "_UB948", DS90U_DESER1) \
	X_DS90U_ID("ds90uh948q", "_UH948", DS90U_DESER1) \
	/*end*/

/* X-macro splits out the device-table-name and ds90u_node_type*/
#undef X_DS90U_ID
#define X_DS90U_ID(name, rxid, type) {name, type},
static const struct i2c_device_id ds90u_id[] = {
	{"ds90u-auto", DS90U_MAX_TYPE},
	DS90U_DEVICE_IDS
	{},
};
MODULE_DEVICE_TABLE(i2c, ds90u_id);

/* X-macro splits out the auto-detect-id and ds90u_node_type*/
#undef X_DS90U_ID
#define X_DS90U_ID(name, rxid, type) {rxid, type},
static const struct ds90u_rxid_map auto_detect_id[] = {
	DS90U_DEVICE_IDS
};

static SIMPLE_DEV_PM_OPS(ds90u_pm_ops, ds90u_suspend, ds90u_resume);

/*
 * This lists compatible entries in addition to the entries in the
 * i2c_device_id table. Those entries are preferred as these entries
 * will use autodetection.
 * We might add support for the .data field here later.
 */
static const struct of_device_id ds90u_dt_ids[] = {
	{ .compatible = "ti,ds90u", },
	{ .compatible = "national,ds90u", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ds90u_dt_ids);

static struct i2c_driver ds90u_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "ds90u",
		.of_match_table = ds90u_dt_ids,
		.pm = &ds90u_pm_ops,
	},

	.id_table = ds90u_id,
	.probe    = ds90u_probe,
	.remove   = ds90u_remove,
};

/**
 * link_status_show() - update input status
 * @dev:	specific device
 * @attr:	device attributes
 * @buf:	device file buffer
 */
static ssize_t link_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int link_status;
	struct ds90u_cdata *cdata = dev_get_drvdata(dev);

	link_status = ds90u_config_read(cdata->client, CFG_LINK_STATUS);
	if (link_status == 1)
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "0\n");
}

static DEVICE_ATTR_RO(link_status);

/**
 * ds90u_sysfs_register() - register sysfs status nodes
 * @dev:	device structure
 */
static void ds90u_sysfs_register(struct device *dev)
{
	if (device_create_file(dev, &dev_attr_link_status))
		dev_err(dev, "unable to create sysfs link status file\n");
}

/**
 * ds90u_sysfs_unregister() - unregister sysfs status nodes
 * @dev:	device structure
 */
static void ds90u_sysfs_unregister(struct device *dev)
{
	device_remove_file(dev, &dev_attr_link_status);
}

/**
 * ds90u_set_pdb() - PDB gpio helper
 * @client:	device-specific client
 * @value :	gpio state
 */
static void ds90u_set_pdb(struct i2c_client *client, int value)
{
	struct ds90u_cdata *cdata;

	cdata = (struct ds90u_cdata *)i2c_get_clientdata(client);
	WARN_ON(cdata == NULL);
	if (cdata == NULL)
		return;

	if (cdata->pdb_gpio >= 0)
		gpio_set_value(cdata->pdb_gpio, value);
}

/**
 * ds90u_auto_detect() - detect device type by reading RX ID register
 * @client:	device-specific client
 *
 * Reads the (HDCP) RX ID register (0xF0-0xF5) and attempts to match it
 * with a known device.
 *
 * This process can fail if the device being probed is on the remote side
 * of the FPD-Link. (Remote device will tend to respond to HDCP register
 * queries with 0x00 response.)
 */
static enum ds90u_node_type ds90u_auto_detect(struct i2c_client *client)
{
	enum ds90u_node_type type = DS90U_MAX_TYPE;
	int value, i;
	char rxid[DS90U_RXID_LEN];

	dev_dbg(&client->dev, "%s\n", __func__);

	/* read the RX ID registers (0xF0-0xF5) */
	for (i = 0; i < sizeof(rxid); i++)	{
		value = i2c_smbus_read_byte_data(client, DS90U_RXID_BASE_REG+i);
		if (value < 0)
			return DS90U_MAX_TYPE;
		rxid[i] = value;
	}
	dev_dbg(&client->dev, "  rxid=%.*s\n", (int)sizeof(rxid), rxid);

	/* compare to known IDs */
	for (i = 0; i < ARRAY_SIZE(auto_detect_id); i++) {
		if (memcmp(auto_detect_id[i].rxid, rxid, sizeof(rxid)) == 0) {
			type = auto_detect_id[i].type;
			break;
		}
	}

	return type;
}

/**
 * ds90u_config_reset_pll() - Reset the PLL for oLDI and FPD
 * @client             : device-specific client
 * @analog_control_page: Analog register selection
 */
static void ds90u_config_reset_pll(struct i2c_client *client,
				       uint8_t analog_control_page)
{
	/* select target for register access */
	i2c_smbus_write_byte_data(client, DS90U_ANA_IA_CNTL, analog_control_page);

	/* reset PLL */
	i2c_smbus_write_byte_data(client, DS90U_ANA_IA_ADDR,
				  DS90U_OLDI_FPD_PLL_SM_CTL);
	i2c_smbus_write_byte_data(client, DS90U_ANA_IA_DATA,
				  DS90U_OLDI_FPD_PLL_SM_EN_RESET);
	i2c_smbus_write_byte_data(client, DS90U_ANA_IA_DATA,
				  DS90U_OLDI_FPD_PLL_SM_DIS_RESET);
}

/**
 * ds90u_serializer_thread_handler() - threaded IRQ handler for serializer
 * @irq   :	irq number
 * @dev_id:	interrupt data
 */
static irqreturn_t ds90u_serializer_thread_handler(int irq, void *dev_id)
{
	struct ds90u_cdata *cdata;
	struct i2c_client *client = dev_id;
	int isr;

	cdata = (struct ds90u_cdata *)i2c_get_clientdata(client);
	WARN_ON(cdata == NULL);
	if (cdata == NULL)
		return IRQ_HANDLED;

	dev_dbg(&client->dev, "IRQ: %d\n", irq);
	dev_dbg(&client->dev, "  client=0x%02X\n", client->addr);

	/* check the serializer ISR to clear the interrupt */
	isr = ds90u_config_read(client, CFG_ISR);
	dev_dbg(&client->dev, "  ISR = 0x%02X\n", isr);

	/* Check for IS_RXDET_INT */
	if (0x41 == (isr & 0x41)) {
		int link_status;

		/*
		 * Cancel the delayed work
		 * 3 cases need to be handled:
		 *   1. delayed work is running
		 *   2. delayed work is waiting on timer
		 *   3. delayed work is not scheduled
		 * For case 1, this will wait for it to end and then cancel any
		 * re-scheduled work.
		 * For case 2, this will remove it from the queue (and
		 * cdata->link_status is still 1).
		 * For case 3, this will do nothing (and cdata->link_status is
		 * already 0).
		 */
		cancel_delayed_work_sync(&cdata->poll_task);

		/*
		 * For case 1 above, this is probably redundant but should
		 * return the same value.
		 */
		link_status = ds90u_config_read(client, CFG_LINK_STATUS);
		dev_dbg(&client->dev, "  link status = %d\n", link_status);

		/*
		 * If transitioning from cdata->link_status=1 to
		 * link_status=1, we must assume that the link did go
		 * down and the poll task simply missed it.
		 */
		if (cdata->link_status)
			ds90u_deinitialize_link(cdata);

		if (link_status) {
			ds90u_initialize_link(client, cdata);
			schedule_delayed_work(&cdata->poll_task, round_jiffies_relative(msecs_to_jiffies(DS90U_LINK_POLL_MS)));
		}

		cdata->link_status = link_status;
	}

	/* TODO Check for IS_RX_INT
	if (0x21 == (isr & 0x21)) {
		handle RX interrupt (pass to child drivers?)
	}*/

	return IRQ_HANDLED;
}

/**
 * ds90u_deserializer_thread_handler() - threaded IRQ handler for deserializer
 * @irq   :	irq number
 * @dev_id:	interrupt data
 */
static irqreturn_t ds90u_deserializer_thread_handler(int irq, void *dev_id)
{
	struct ds90u_cdata *cdata;
	struct i2c_client *client = dev_id;
	int link_status;

	cdata = (struct ds90u_cdata *)i2c_get_clientdata(client);
	WARN_ON(cdata == NULL);
	if (cdata == NULL)
		return IRQ_HANDLED;

	dev_dbg(&client->dev, "IRQ (deser): %d\n", irq);
	dev_dbg(&client->dev, "  client=0x%02X\n", client->addr);

	/*
	 * For non deserializer (level) interrupt, we often get spurious
	 * triggers, so only re-initialize on state change.
	 */
	link_status = ds90u_config_read(client, CFG_LINK_STATUS);
	dev_dbg(&client->dev, "  link status = %d\n", link_status);

	if (cdata->link_status && !link_status) {
		dev_dbg(&client->dev, "  Lost Link.");
		cdata->link_status = false;
		ds90u_deinitialize_link(cdata);
	} else if (!cdata->link_status && link_status) {
		dev_dbg(&client->dev, "  Gained Link.");
		ds90u_initialize_link(client, cdata);
	}

	cdata->link_status = link_status;

	return IRQ_HANDLED;
}

/**
 * ds90u_match_dev() - match i2c client under adapter with test_deserializer_mailbox i2c address
 */
static int ds90u_match_dev(struct device *dev, void *data)
{
	struct ds90u_cdata *cdata = (struct ds90u_cdata *) data;
	struct i2c_client *client = i2c_verify_client(dev);

	return (client && &cdata->adap == client->adapter && client->addr == cdata->test_deserializer_mailbox);
}

/**
 * ds90u_check_deserializer_reset() - search for deserializer behind adapter, if found check mailbox0 conect to deserializer address
 */
static int ds90u_check_deserializer_reset(struct ds90u_cdata *cdata)
{
	struct i2c_client *deserializer = 0;
	struct device *dev;
	uint32_t mailbox0;

	if (!cdata->serializer)
		return 0;

	dev = bus_find_device(&i2c_bus_type, NULL, cdata, ds90u_match_dev);
	if (dev)
		deserializer = i2c_verify_client(dev);

	if (!deserializer)
		return 0;

	mailbox0 = ds90u_config_read(deserializer, CFG_MAILBOX0);
	if (mailbox0 >= 0) {
		/*
		 * deserializer (only DS90U_DESER0) stores i2c address in mailbox0
		 * if unchanged, no reset has happened
		 */
		if (mailbox0 != (unsigned short) deserializer->addr) {
			dev_warn(&deserializer->dev, "detected deserializer reset 0x%x\n", cdata->test_deserializer_mailbox);
			return 1;
		}
	}

	return  0;
}

/**
 * ds90u_link_poll_task() - workqueue task to periodically check link status
 * Only runs for serializers that have a link active.
 * drivers behind the i2c gateway.
 * @work:	workqueue data
 */
static void ds90u_link_poll_task(struct work_struct *work)
{
	int link_status;
	struct ds90u_cdata *cdata = container_of(work, struct ds90u_cdata, poll_task.work);

	link_status = ds90u_config_read(cdata->client, CFG_LINK_STATUS);

	if (link_status) {

		/* For serializer check deserializer mailbox0 */
		if (ds90u_check_deserializer_reset(cdata))
			cdata->link_status = 0;

		if (!cdata->link_status)
			ds90u_initialize_link(cdata->client, cdata);
		schedule_delayed_work(&cdata->poll_task, round_jiffies_relative(msecs_to_jiffies(DS90U_LINK_POLL_MS)));
	} else {
		dev_dbg(&cdata->client->dev, "POLL: link status=0\n");
		if (cdata->link_status)
			ds90u_deinitialize_link(cdata);
	}

	cdata->link_status = link_status;
}

/**
 * ds90u_set_serializer_config() - set serializer configuration bits
 * Serializer uses intb pin, which must be configured and armed.
 * @client:	device-specific client struct
 * @cdata :	device-specific clientdata struct
 */
static void ds90u_set_serializer_config(struct i2c_client *client,
					struct ds90u_cdata *cdata)
{
	ds90u_config_set(client, CFG_ICR_RX_DETECT);
	/* TODO: ds90u_config_set(client, CFG_ICR_RX_INTB); */
	ds90u_config_set(client, CFG_ICR_ENABLE);

	/* read the ISR to clear the initial state */
	if (cdata->link_status)
		ds90u_config_read(client, CFG_ISR);
}

/**
 * ds90u_verify_hdmi_clock_stability() - verify HDMI clock stability
 *
 * This routine verifies if HDMI clock is stable before applying Init B.
 *
 * @client:	device-specific client struct
 * @cdata :	device-specific clientdata struct
 */
static void ds90u_verify_hdmi_clock_stability(struct i2c_client *client,
					       struct ds90u_cdata *cdata)
{
	int retry = 0;

	/* Enabel read for the power-up state machine */
	i2c_smbus_write_byte_data(client, DS90U_PWR_UP_STATE_MACHINE_CFG, 0x80);

	/* Enabel register-read capability of state-machine */
	i2c_smbus_write_byte_data(client, DS90U_STATE_MACHINE_CAP, 0x80);

	while ((++retry < DS90U_HDMI_VERIFY_MAX_RETRY) &&
	  ((i2c_smbus_read_byte_data(client, DS90U_STATE_MACHINE_CAP)&0x1F) != 0x1B)) {
		msleep(10);
	}

	/* Disable register-read capability of state-machine */
	i2c_smbus_write_byte_data(client, DS90U_STATE_MACHINE_CAP, 0x0);

	/* Disable read for the power-up state machine */
	i2c_smbus_write_byte_data(client, DS90U_PWR_UP_STATE_MACHINE_CFG, 0x0);

	if (retry == DS90U_HDMI_VERIFY_MAX_RETRY) {
		dev_err(&client->dev, "%s: Failed to verify HDMI clock\n", __func__);
	} else {
		dev_info(&client->dev, "%s: Succeed to verify HDMI clock at retry %d\n",
			__func__, retry);
	}
}

/**
 * ds90u_reset_oldi_fpd_pll() - reset oLDI and FPD PLL
 * This is an errata to fix the display issue when temperature ramp
 * @client:	device-specific client struct
 */
static void ds90u_reset_oldi_fpd_pll(struct i2c_client *client)
{
	int val = 0;

	i2c_smbus_write_byte_data(client, DS90U_ANA_IA_CNTL,
				  DS90U_ANA_IA_CNTL_OLDI_READ);
	i2c_smbus_write_byte_data(client, DS90U_ANA_IA_ADDR,
				  DS90U_OLDI_PLL_PPM_CNT);
	val = i2c_smbus_read_byte_data(client, DS90U_ANA_IA_DATA);

	if (val == 0x07) {
		i2c_smbus_write_byte_data(client, DS90U_ANA_IA_CNTL,
					  DS90U_ANA_IA_CNTL_OLDI_WRITE);
		i2c_smbus_write_byte_data(client, DS90U_ANA_IA_ADDR,
					      DS90U_OLDI_PLL_PPM_CNT);
		i2c_smbus_write_byte_data(client, DS90U_ANA_IA_DATA, 0x3F);
	} else if (val != 0x3F) {
		dev_err(&client->dev, "%s: PLL_PPM_CNT value 0x%x\n", __func__, val);
		return;
	}

	/* Reset the oLDI and FPD PLL settings */
	ds90u_config_reset_pll(client, DS90U_ANA_IA_CNTL_OLDI_WRITE);
	msleep(10);
	ds90u_config_reset_pll(client, DS90U_ANA_IA_CNTL_FPD_WRITE);
	msleep(10);
}

/**
 * ds90u_apply_init_errata() - apply errata during initialization
 *
 * There are two init sequence in this routine. The Init A sequence consists of
 * following erratas:
 *  - #5: backchannel_watchdog_value
 *  - #6: apply_AVMUTE_errata
 *  - #2: disable_clock_autodetect
 *
 * The Init B sequence consists of following errata:
 *  - #1: apply_temp_ramp_errata
 *
 * Between Init A and Init B programming sequence, following errata is applied
 * to check the HDMI clock:
 *  - #4: verify_hdmi_clock
 *
 * More details refer to spec "FPD-Link_DS90Ux929-Q1_Errata_<version>.pdf".
 *
 * @client:	device-specific client struct
 * @cdata :	device-specific clientdata struct
 */
static void ds90u_apply_init_errata(struct i2c_client *client,
			      struct ds90u_cdata *cdata)
{
	/*
	 * Set the backchannel watchdog timeout value.
	 */
	if ((cdata->backchannel_watchdog_value > 0) &&
	    (cdata->backchannel_watchdog_value <= 0x7F)) {
		ds90u_config_write(client, CFG_BCC_WD_VALUE,
				   cdata->backchannel_watchdog_value);
	} else if (cdata->backchannel_watchdog_value == 0xFF) {
		ds90u_config_set(client, CFG_BCC_WD_DISABLE);
	}

	/*
	 * Workaround for AVMUTE issue: When using FPD-Link III for UB Serializers,
	 * or UH Serializer working in UB mode, it's possible to trigger the
	 * companion Deserializer to enter AVMUTE mode. Keep that from happening.
	 */
	if (cdata->apply_AVMUTE_errata)
		ds90u_config_set(client, CFG_DE_GATE_RGB);

	/*
	 * Workaround for Display Blanking During Temperature Ramp issue: Display
	 * would flicker for a short time when the ambient temperature around 929,
	 * 947 or 949 changes by more than 60degC.
	 */
	if (cdata->disable_clock_autodetect)
		ds90u_config_clear(client, CFG_CLOCK_DETECT);

	dev_info(&client->dev, "%s: Init A finished\n", __func__);

	/* Verify the HDMI clock */
	if (cdata->verify_hdmi_clock)
		ds90u_verify_hdmi_clock_stability(client, cdata);

	/* reset oLDI and FPD PLLs */
	if (cdata->apply_temp_ramp_errata)
		ds90u_reset_oldi_fpd_pll(client);

	dev_info(&client->dev, "%s: Init B finished\n", __func__);
}

/**
 * ds90u_register_lock_interrupt() - final registration of interrupts
 * Gets IRQ GPIO from device tree and registers it via devm interface.
 * @client:	device-specific client
 */
static void ds90u_register_lock_interrupt(struct i2c_client *client)
{
	int lock_gpio;
	int lock_irq;
	int check;
	struct ds90u_cdata *cdata;

	dev_dbg(&client->dev, "%s\n", __func__);

	cdata = (struct ds90u_cdata *)i2c_get_clientdata(client);
	WARN_ON(cdata == NULL);
	if (cdata == NULL)
		return;

	lock_gpio = cdata->lock_gpio;
	if (lock_gpio < 0)
		return;

	dev_dbg(&client->dev, "  lock-gpio=%d\n", lock_gpio);

	lock_irq = gpio_to_irq(lock_gpio);
	dev_dbg(&client->dev, "  irq=%d\n", lock_irq);
	if (lock_irq < 0) {
		dev_err(&client->dev, "%s: IRQ Invalid for GPIO %d\n", __func__, lock_gpio);
		return;
	}

	check = devm_gpio_request_one(&client->dev, lock_gpio,
				      GPIOF_DIR_IN|GPIOF_EXPORT_DIR_CHANGEABLE,
				      "ds90u-lock-detect");
	if (check < 0) {
		dev_err(&client->dev, "%s: GPIO %d invalid %d\n", __func__, lock_gpio, check);
		return;
	}

	if (cdata->serializer)
		ds90u_set_serializer_config(client, cdata);

	/*
	 * request the irq -
	 * serializer uses intb falling edge, deserializer uses gpio level
	 */
	if (cdata->serializer)
		check = devm_request_threaded_irq(&client->dev, lock_irq, NULL,
			&ds90u_serializer_thread_handler,
			IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
			"ds90u-ser-lock-detect", client);
	else
		check = devm_request_threaded_irq(&client->dev, lock_irq, NULL,
			&ds90u_deserializer_thread_handler,
			IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
			"ds90u-deser-lock-detect", client);

	if (check < 0) {
		dev_err(&client->dev, "%s: IRQ request failed %d\n", __func__, check);
		devm_gpio_free(&client->dev, lock_gpio);
		return;
	}
}

/**
 * ds90u_resume_delayed_worker() - delayed worker to wake up the device
 *
 * Before applying init B errata, it's required to verify the HDMI clock
 * availability. However, Android service 'DisplayPowerController' cut off the
 * HDMI clock during the whole STR process, it's not possible to get it during
 * resume process. Therefore, we use a delayed worker to wait 8 seconds until
 * the HDMI clock is reenabled by 'DisplayPowerController'.
 *
 * @work:	workqueue data
 */
static void ds90u_resume_delayed_worker(struct work_struct *work)
{
	int i2c_read = 0;
	int max_retry = 800;

	struct ds90u_cdata *cdata = container_of(work, struct ds90u_cdata,
						 resume_delayed_worker.work);
	if (cdata->verify_hdmi_clock) {
		while ((++(cdata->hdmi_clock_retry) < max_retry) &&
		((i2c_smbus_read_byte_data(cdata->client, DS90U_STATE_MACHINE_CAP)&0x1F) != 0x1B)) {
			schedule_delayed_work(&cdata->resume_delayed_worker,
								round_jiffies_relative(msecs_to_jiffies(10)));
			return;
		}

		/* Disable register-read capability of state-machine */
		i2c_smbus_write_byte_data(cdata->client, DS90U_STATE_MACHINE_CAP, 0x0);

		/* Disable read for the power-up state machine */
		i2c_smbus_write_byte_data(cdata->client, DS90U_PWR_UP_STATE_MACHINE_CFG, 0x0);

		if (cdata->hdmi_clock_retry == max_retry) {
			dev_err(&cdata->client->dev, "%s: Failed to verify HDMI clock\n", __func__);
		} else {
			dev_info(&cdata->client->dev, "%s: Succeed to verify HDMI clock at retry %d\n",
					__func__, cdata->hdmi_clock_retry);
			cdata->hdmi_clock_retry = 0;
		}
	}

	/* reset oLDI and FPD PLLs */
	if (cdata->apply_temp_ramp_errata)
		ds90u_reset_oldi_fpd_pll(cdata->client);

	dev_info(&cdata->client->dev, "%s: Init B finished\n", __func__);

	if (cdata != NULL) {
		if (cdata->apply_resume_delay)
			msleep(cdata->apply_resume_delay);

		i2c_read = ds90u_config_read(cdata->client, CFG_LINK_STATUS);
		if (i2c_read > 0) {
			cdata->link_status = i2c_read;
			ds90u_initialize_link(cdata->client, cdata);
		}

		if (cdata->serializer) {
			ds90u_set_serializer_config(cdata->client, cdata);
			schedule_delayed_work(&cdata->poll_task,
			    round_jiffies_relative(msecs_to_jiffies(DS90U_LINK_POLL_MS)));
		}
	}

	ds90u_sysfs_register(&cdata->client->dev);

	ds90u_register_lock_interrupt(cdata->client);
}

/**
 * ds90u_platform_probe() - read platform data into the cdata struct
 * @client:	device-specific client struct
 * @id    :	device type information
 * @cdata :	device-specific clientdata struct
 */
static void ds90u_platform_probe(struct i2c_client *client,
	const struct i2c_device_id *id, struct ds90u_cdata *cdata)
{
	const struct ds90u_platform_data *pdata = client->dev.platform_data;

	if (pdata == NULL)
		return;

	dev_dbg(&client->dev, "%s\n", __func__);

	cdata->pdb_gpio = pdata->pdb_gpio;
	cdata->lock_gpio = pdata->lock_gpio;
	cdata->scl_pulse_width = pdata->scl_pulse_width;
	cdata->i2c_promiscuous_mode = pdata->i2c_promiscuous_mode;
	cdata->video_18_bit = pdata->video_18_bit;
	cdata->verify_hdmi_clock = pdata->verify_hdmi_clock;
	cdata->apply_temp_ramp_errata = pdata->apply_temp_ramp_errata;
	cdata->backchannel_watchdog_value = pdata->backchannel_watchdog_value;
	cdata->test_deserializer_mailbox = pdata->test_deserializer_mailbox;

	if (IS_ENABLED(CONFIG_SERDES_TIDS90U_GPIO)) {
		cdata->gpio_controller = pdata->gpio_controller;
		cdata->gc.base = -1;
		if (cdata->gpio_controller) {
			/**
			 * Setting gc.base to an actual value may be useful
			 * when accessing via sysfs.
			 */
			cdata->gc.base = pdata->gpio_base;

			if (id)
				cdata->gc.label = id->name;
		}
	} else {
		cdata->gpio_controller = false;
	}

	if (IS_ENABLED(CONFIG_SERDES_TIDS90U_LUT))
		cdata->lut_file_name = pdata->lut_file_name;
	else
		cdata->lut_file_name = NULL;

	if (IS_ENABLED(CONFIG_SERDES_TIDS90U_I2C)) {
		cdata->i2c_adapter = pdata->i2c_adapter;
		cdata->adap.nr = pdata->i2c_adapter_num;
		strncpy(cdata->adap.name, id->name, sizeof(cdata->adap.name)-1);
	} else {
		cdata->i2c_adapter = false;
	}

	memcpy(cdata->gpio_init, pdata->gpio_init, sizeof(cdata->gpio_init));
	memcpy(cdata->gpio_deinit, pdata->gpio_deinit, sizeof(cdata->gpio_deinit));
	memcpy(cdata->i2c_alias_init, pdata->i2c_alias_init,
		sizeof(cdata->i2c_alias_init));

	cdata->mapsel_override = pdata->mapsel_override;
	cdata->mapsel_value = pdata->mapsel_value;

	cdata->oen_override = pdata->oen_override;
	cdata->oen_value = pdata->oen_value;
	cdata->oss_value = pdata->oss_value;

	cdata->apply_resume_delay = pdata->apply_resume_delay;
	cdata->apply_AVMUTE_errata = pdata->apply_AVMUTE_errata;
	cdata->disable_clock_autodetect = pdata->disable_clock_autodetect;
	cdata->lvds_vod_value = pdata->lvds_vod_value;
}

/**
 * ds90u_of_get_gpios() - read gpio pinmux initialization settings from OF
 * Initializes the client gpio pinmux defaults based on device tree bindings.
 * @np :	OF device node
 * @cdata :	device-specific clientdata struct
 */
static void ds90u_of_get_gpios(struct device_node *np,
	struct ds90u_cdata *cdata)
{
	char gpio_prop_name[] = "gpio-config-0";
	char gpio_reg_prop_name[] = "gpio-config-reg-4";
	int i;
	u32 val;

	dev_dbg(&cdata->client->dev, "%s\n", __func__);

	/*
	 * we have some hardcoded values here, check whether the define used for
	 * the struct size matches our expectations
	 */
	WARN_ON(9 != DS90U_MAX_NUM_GPIO);

	for (i = 0; i <= 3; i++) {
		gpio_prop_name[12] = ('0' + i);
		if (of_property_read_u32(np, gpio_prop_name, &val) == 0) {
			if (val > 0x0F) {
				dev_warn(&cdata->client->dev, "invalid init value for %s\n", gpio_prop_name);
				cdata->gpio_init[i] = 0;
			} else {
				cdata->gpio_init[i] = val;
			}
		}
	}

	/* only 925/926 has GPIO REG 4 */
	if ((cdata->type == DS90U_SER925) || (cdata->type == DS90U_DESER926)) {
		if (of_property_read_u32(np, gpio_reg_prop_name, &val) == 0) {
			if (val > 0x0F) {
				dev_warn(&cdata->client->dev, "invalid init value for %s\n", gpio_reg_prop_name);
				cdata->gpio_init[4] = 0;
			} else {
				cdata->gpio_init[4] = val;
			}
		}
	} else {
		cdata->gpio_init[4] = 0;
	}

	for (i = 1; i <= 4; i++) {
		gpio_reg_prop_name[16] = ('4' + i);
		if (of_property_read_u32(np, gpio_reg_prop_name, &val) == 0) {
			if (val > 0x0F) {
				dev_warn(&cdata->client->dev, "invalid init value for %s\n", gpio_reg_prop_name);
				cdata->gpio_init[4+i] = 0;
			} else {
				cdata->gpio_init[4+i] = val;
			}
		}
	}
}

/**
 * ds90u_of_get_i2c_map() - read i2c alias translation mapping from OF
 * Initializes the client i2c alias translation defaults based on the device
 * tree bindings.
 * @np :	OF device node
 * @cdata :	device-specific clientdata struct
 */
static void ds90u_of_get_i2c_map(struct device_node *np,
	struct ds90u_cdata *cdata)
{
	struct property *prop;
	const __be32 *val;
	size_t prop_count;
	size_t i;

	dev_dbg(&cdata->client->dev, "%s\n", __func__);

	prop = of_find_property(np, "i2c-address-map", NULL);
	if (!prop)
		return;

	prop_count = prop->length / sizeof(*val) / 2;

	dev_dbg(&cdata->client->dev, "  i2c-address-map len   = %d\n", prop->length);
	dev_dbg(&cdata->client->dev, "  i2c-address-map count = %zu\n", prop_count);

	/* sanity check */
	if (!prop->value || (prop_count == 0)) {
		dev_err(&cdata->client->dev, "empty i2c-address-map\n");
		return;
	}
	if (prop->length != (prop_count * sizeof(*val) * 2)) {
		dev_err(&cdata->client->dev, "invalid i2c-address-map format\n");
		return;
	}

	if (prop_count > DS90U_NUM_SLAVE_ALIAS)
		dev_warn(&cdata->client->dev, "too many entries in i2c-address-map -- ignoring extra\n");

	/* read the values from DT and write them to cdata->i2c_alias_init */
	val = prop->value;
	for (i = 0; i < prop_count && i < DS90U_NUM_SLAVE_ALIAS; i++) {
		int slave_alias;
		int slave_id;

		slave_alias = be32_to_cpup(val++);
		slave_id = be32_to_cpup(val++);
		if ((slave_alias > 0x7f) || (slave_id > 0x7f)) {
			dev_warn(&cdata->client->dev, "invalid i2c alias map (0x%02X -> 0x%02X)\n", slave_alias, slave_id);
		} else {
			cdata->i2c_alias_init[i].alias_address = slave_alias;
			cdata->i2c_alias_init[i].remote_address = slave_id;
		}
	}
}

/**
 * ds90u_of_probe() - read OF device tree data into the cdata struct
 * @client:	device-specific client struct
 * @cdata :	device-specific clientdata struct
 */
static void ds90u_of_probe(struct i2c_client *client, struct ds90u_cdata *cdata)
{
	struct device_node *np = client->dev.of_node;

	if (np == NULL)
		return;

	dev_dbg(&client->dev, "%s\n", __func__);

	cdata->pdb_gpio = of_get_named_gpio(np, "pdb-gpio", 0);
	cdata->lock_gpio = of_get_named_gpio(np, "lock-gpio", 0);

	cdata->video_18_bit = of_property_read_bool(np, "18-bit-video-mode");
	if (cdata->video_18_bit)
		dev_dbg(&client->dev, "18-bit-video-mode enabled\n");

	cdata->scl_pulse_width = -1;
	of_property_read_u32(np, "scl-pulse-width", &cdata->scl_pulse_width);

	cdata->i2c_promiscuous_mode = of_property_read_bool(np,
		"i2c-promiscuous-mode");

	if (IS_ENABLED(CONFIG_SERDES_TIDS90U_GPIO)) {
		cdata->gpio_controller = of_property_read_bool(np,
			"gpio-controller");

		cdata->gc.base = -1;
		if (cdata->gpio_controller) {
			/**
			 * Setting gc.base to an actual value may be useful if
			 * we ever need to access gpios by number rather than
			 * only via the device-tree. (For example, via sysfs.)
			 */
			of_property_read_u32(np, "gpio-base", &cdata->gc.base);

#if defined(CONFIG_OF_GPIO)
			cdata->gc.of_node = np;
#endif

			cdata->gc.label = np->full_name;
		}
	} else {
		cdata->gpio_controller = false;
	}

	if (IS_ENABLED(CONFIG_SERDES_TIDS90U_LUT)) {
		of_property_read_string(client->dev.of_node, "lut_file",
			&cdata->lut_file_name);
	} else {
		cdata->lut_file_name = NULL;
	}

	if (IS_ENABLED(CONFIG_SERDES_TIDS90U_I2C)) {
		u32 tmp = 0;

		of_property_read_u32(np, "#address-cells", &tmp);
		cdata->i2c_adapter = (tmp == 1);
		cdata->adap.nr = -1;
		cdata->adap.dev.of_node = np;
		strncpy(cdata->adap.name, np->full_name,
			sizeof(cdata->adap.name) - 1);
	} else {
		cdata->i2c_adapter = false;
	}

	ds90u_of_get_gpios(np, cdata);
	ds90u_of_get_i2c_map(np, cdata);

	cdata->mapsel_override = of_property_read_bool(np, "mapsel-override");
	if (cdata->mapsel_override)
		cdata->mapsel_value = of_property_read_bool(np, "mapsel-value");

	cdata->oen_override = of_property_read_bool(np, "oen-override");
	if (cdata->oen_override) {
		cdata->oen_value = of_property_read_bool(np, "oen-value");
		cdata->oss_value = of_property_read_bool(np, "oss-value");
	}

	cdata->apply_AVMUTE_errata = of_property_read_bool(np, "apply-AVMUTE-errata");
	cdata->disable_clock_autodetect = of_property_read_bool(np, "disable-clock-autodetect");
	cdata->verify_hdmi_clock = of_property_read_bool(np, "verify-hdmi-clock");
	cdata->apply_temp_ramp_errata = of_property_read_bool(np, "apply-temp-ramp-errata");
	of_property_read_u32(np, "backchannel-watchdog-value", &cdata->backchannel_watchdog_value);
	cdata->lvds_vod_value = -1;
	of_property_read_u32(np, "lvds-vod-value", &cdata->lvds_vod_value);
}

#ifdef CONFIG_SERDES_TIDS90U_DRM_BRIDGE

static inline struct ds90u_cdata *bridge_to_cdata(struct drm_bridge *bridge)
{
	return container_of(bridge, struct ds90u_cdata, drm_bridge);
}

static int ds90u_bridge_attach(struct drm_bridge *bridge)
{
	struct ds90u_cdata *cdata = bridge_to_cdata(bridge);

	dev_dbg(&cdata->client->dev, "ds90u_bridge_attach\n");

	ds90u_config_write(cdata->client, CFG_RESET_CTL_REG, 0x08);  // Disable DSI input
	ds90u_config_write(cdata->client, CFG_BRIDGE_CTL_REG, 0x8c); // Set 4 DSI lanes
	ds90u_config_write(cdata->client, CFG_DEVICE_CFG_REG, 0x01); // Set DSI0 reverse lane order

	ds90u_config_write(cdata->client, CFG_IND_ACC_CTL_REG, 0x04);  // Set indirect regerister page
	ds90u_config_write(cdata->client, CFG_IND_ACC_ADDR_REG, 0x05); // Set indirect regerister
	ds90u_config_write(cdata->client, CFG_IND_ACC_DATA_REG, 0x30); // Set TSKIP_CNT
	ds90u_config_write(cdata->client, CFG_DUAL_CTL1_REG, 0x0c);    //  Set auto dtect FPD.link III mode

	ds90u_config_write(cdata->client, CFG_GENERAL_CFG2_REG, 0x20); // Clear CRC_ERROR_SET bit in GENERAL_CFG2 register

	ds90u_config_write(cdata->client, CFG_RESET_CTL_REG, 0x00);  // Enable DSI input

	/* Special reset handling needed fo UH481A (should not be needed for UH481AS) */
	msleep(50);
	ds90u_config_write(cdata->client, CFG_IND_ACC_CTL_REG, 0x10);
	ds90u_config_write(cdata->client, CFG_IND_ACC_ADDR_REG, 0x49);
	ds90u_config_write(cdata->client, CFG_IND_ACC_DATA_REG, 0x10);
	ds90u_config_write(cdata->client, CFG_IND_ACC_DATA_REG, 0x00);
	msleep(10);
	ds90u_config_write(cdata->client, CFG_IND_ACC_CTL_REG, 0x14);
	ds90u_config_write(cdata->client, CFG_IND_ACC_ADDR_REG, 0x49);
	ds90u_config_write(cdata->client, CFG_IND_ACC_DATA_REG, 0x10);
	ds90u_config_write(cdata->client, CFG_IND_ACC_DATA_REG, 0x00);

	return 0;
}

static enum drm_mode_status ds90u_bridge_mode_valid(struct drm_bridge *bridge,
			  const struct drm_display_mode *mode)
{
	struct ds90u_cdata *cdata = bridge_to_cdata(bridge);

	dev_dbg(&cdata->client->dev, "ds90u_bridge_mode_valid %s\n", mode->name);
/*
	dev_err(&cdata->client->dev, " %d x %d, clk %d\n", mode->hdisplay, mode->vdisplay, mode->clock );
	dev_err(&cdata->client->dev, " horz start %d, end %d, total %d\n", mode->hsync_start, mode->hsync_end, mode->htotal );
	dev_err(&cdata->client->dev, " vert start %d, end %d, total %d\n", mode->vsync_start, mode->vsync_end, mode->vtotal );
*/
	return MODE_OK;
}

static void ds90u_bridge_disable(struct drm_bridge *bridge)
{
	struct ds90u_cdata *cdata = bridge_to_cdata(bridge);

	dev_dbg(&cdata->client->dev, "ds90u_bridge_disable\n");
}

static void ds90u_bridge_mode_set(struct drm_bridge *bridge,
				    struct drm_display_mode *mode,
				    struct drm_display_mode *adjusted_mode)
{
	struct ds90u_cdata *cdata = bridge_to_cdata(bridge);

	dev_dbg(&cdata->client->dev, "ds90u_bridge_mode_set %s\n", mode->name);
}

static void ds90u_bridge_enable(struct drm_bridge *bridge)
{
	struct ds90u_cdata *cdata = bridge_to_cdata(bridge);

	dev_dbg(&cdata->client->dev, "ds90u_bridge_enable\n");
}


static const struct drm_bridge_funcs ds90u_drm_bridge_funcs = {
	.attach = ds90u_bridge_attach,
	.mode_valid = ds90u_bridge_mode_valid,
	.disable = ds90u_bridge_disable,
	.mode_set = ds90u_bridge_mode_set,
	.enable = ds90u_bridge_enable,
};
#endif

/**
 * ds90u_probe() - driver probe function
 * @client:	device-specific client struct
 * @id    :	device type information
 */
static int ds90u_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int i2c_read = 0;
	int check;
	enum ds90u_node_type device_type = DS90U_MAX_TYPE;
	struct ds90u_cdata *cdata = NULL;

	WARN_ON(client == NULL);
	if (client == NULL)
		return -EINVAL;

	/* validate the parameters */
	dev_dbg(&client->dev, "%s\n", __func__);
	if (id == NULL) {
		dev_dbg(&client->dev, "  id=null\n");
		device_type = DS90U_MAX_TYPE;
	} else {
		dev_dbg(&client->dev, "  id=%.*s\n", I2C_NAME_SIZE, id->name);
		device_type = id->driver_data;
	}

	/* setup per-client data */
	cdata = devm_kzalloc(&client->dev, sizeof(struct ds90u_cdata),
		GFP_KERNEL);
	if (!cdata)
		return -ENOMEM;

	cdata->client = client;
	cdata->lock_gpio = -1;
	cdata->pdb_gpio = -1;
	i2c_set_clientdata(client, cdata);

	ds90u_platform_probe(client, id, cdata);
	ds90u_of_probe(client, cdata);

	/* release the chip from reset */
	if (cdata->pdb_gpio >= 0) {
		check = devm_gpio_request_one(&client->dev, cdata->pdb_gpio,
				GPIOF_OUT_INIT_LOW|GPIOF_EXPORT_DIR_CHANGEABLE,
				"ds90u-pdb");
		if (check < 0) {
			dev_err(&client->dev, "Unable to register PDB GPIO err=%d\n", check);
			return check;
		}

		dev_dbg(&client->dev, "  pdb=%d\n", cdata->pdb_gpio);

		msleep(10);
		gpio_set_value(cdata->pdb_gpio, 1);
		dev_info(&client->dev, "ds90u-pdb set to HIGH\n");
		msleep(4);
	}

	/* auto-detect the device type */
	if (device_type == DS90U_MAX_TYPE)
		device_type = ds90u_auto_detect(client);

	if (device_type >= DS90U_MAX_TYPE) {
		dev_err(&client->dev, "%s failed - unrecognized device\n", __func__);
		return -ENODEV;
	}

	cdata->type = device_type;
	if (ds90u_type_is_serializer[device_type]) {
		cdata->serializer = 1;
		INIT_DELAYED_WORK(&cdata->poll_task, &ds90u_link_poll_task);

#ifdef CONFIG_SERDES_TIDS90U_DRM_BRIDGE
		cdata->drm_bridge.funcs = &ds90u_drm_bridge_funcs;
		drm_bridge_add(&cdata->drm_bridge);
#endif
	}

	INIT_DELAYED_WORK(&cdata->resume_delayed_worker, &ds90u_resume_delayed_worker);

	/* validate the i2c addr */
	i2c_read = ds90u_config_read(client, CFG_DEVICE_ID);
	if (i2c_read < 0)
		return i2c_read;

	dev_dbg(&client->dev, "  probe returns 0x%02X\n", i2c_read);

	/* this may be normal if the address was re-mapped */
	if (i2c_read != client->addr)
		dev_warn(&client->dev, "%s client address 0x%02X does not match query result 0x%02X\n", __func__, client->addr, i2c_read);

	/* apply initialization errata */
	ds90u_apply_init_errata(client, cdata);

	/*
	 * Configure the deserializer to use the same lock conditions as the
	 * serializer. This avoids a problem where the serializer thinks there
	 * is a lock but the deserialzer doesn't. In this case we would never
	 * get any lock interrupt since we already believe we are locked but
	 * deserializer will skip its init code.
	 * This will probably need to be revisited at some point if this causes
	 * multiple lock/unlock sequences during device probing.
	 */
	if (!cdata->serializer)
		ds90u_config_set(client, CFG_DUAL_RX_LOCK_MODE);

	/* initialize the device */
	i2c_read = ds90u_config_read(client, CFG_LINK_STATUS);
	if (i2c_read < 0)
		return i2c_read;

	cdata->link_status = i2c_read;
	dev_dbg(&client->dev, "  link status = %d\n", i2c_read);
	if (i2c_read)
		ds90u_initialize_link(client, cdata);

	if (cdata->serializer)
		schedule_delayed_work(&cdata->poll_task, round_jiffies_relative(msecs_to_jiffies(DS90U_LINK_POLL_MS)));

	ds90u_sysfs_register(&client->dev);

	/*
	 * set up the lock detection interrupt
	 * this should be done last so that  the IRQ is the first
	 * dev-managed resource that gets released
	 */
	ds90u_register_lock_interrupt(client);

	return 0;
}

/**
 * ds90u_deinitialize() - driver deinitialization function
 * Performs operations common for driver removal and suspending.
 * @client:	device-specific client
 */
static int ds90u_deinitialize(struct i2c_client *client)
{
	struct ds90u_cdata *cdata;
	int lock_gpio;
	int lock_irq;

	cdata = (struct ds90u_cdata *)i2c_get_clientdata(client);
	WARN_ON(cdata == NULL);

	/* release threaded irq */
	lock_gpio = cdata->lock_gpio;
	lock_irq = gpio_to_irq(lock_gpio);

	devm_free_irq(&client->dev, lock_irq, client);
	devm_gpio_free(&client->dev, lock_gpio);

	ds90u_sysfs_unregister(&client->dev);

	if (cdata != NULL) {
		if (cdata->serializer)
			cancel_delayed_work_sync(&cdata->poll_task);

		cancel_delayed_work_sync(&cdata->resume_delayed_worker);

		ds90u_deinitialize_link(cdata);
		cdata->link_status = 0;
	}

	ds90u_set_pdb(client, 0);

	return 0;
}

/**
 * ds90u_remove() - driver remove function
 * @client:	device-specific client
 */
static int ds90u_remove(struct i2c_client *client)
{
	return ds90u_deinitialize(client);
}

/**
 * ds90u_suspend() - driver suspend function
 * @dev:	device structure
 */
static int ds90u_suspend(struct device *dev)
{
	return ds90u_deinitialize(to_i2c_client(dev));
}

/**
 * ds90u_resume() - driver resume function
 * @dev:	device structure
 */

static int ds90u_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ds90u_cdata *cdata;

	cdata = (struct ds90u_cdata *)i2c_get_clientdata(client);
	WARN_ON(cdata == NULL);

	msleep(10);

	ds90u_set_pdb(client, 1);

	/* sleep 4 ms, wait for I2C access available */
	msleep(4);

	/*
	 * init A errata
	 */
	if ((cdata->backchannel_watchdog_value > 0) &&
	    (cdata->backchannel_watchdog_value <= 0x7F)) {
		ds90u_config_write(client, CFG_BCC_WD_VALUE,
				   cdata->backchannel_watchdog_value);
	} else if (cdata->backchannel_watchdog_value == 0xFF) {
		ds90u_config_set(client, CFG_BCC_WD_DISABLE);
	}

	if (cdata->apply_AVMUTE_errata)
		ds90u_config_set(client, CFG_DE_GATE_RGB);

	if (cdata->disable_clock_autodetect)
		ds90u_config_clear(client, CFG_CLOCK_DETECT);

	dev_info(&client->dev, "%s: Init A finished\n", __func__);

	if (cdata->verify_hdmi_clock) {
		/* Enabel read for the power-up state machine */
		i2c_smbus_write_byte_data(client, DS90U_PWR_UP_STATE_MACHINE_CFG, 0x80);

		/* Enabel register-read capability of state-machine */
		i2c_smbus_write_byte_data(client, DS90U_STATE_MACHINE_CAP, 0x80);
	}

	/* use delayed worker to detect HDMI clock, apply init B errata, etc.*/
	schedule_delayed_work(&cdata->resume_delayed_worker, 0);

	return 0;
}

module_i2c_driver(ds90u_i2c_driver);

MODULE_AUTHOR("Chris Baker, Delphi Electronics");
MODULE_DESCRIPTION("TI ds90ux9xx FPD-Link III i2c driver");
MODULE_LICENSE("GPL");
