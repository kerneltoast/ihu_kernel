// SPDX-License-Identifier: GPL
/*
 * ds90u-i2c-adapter.c - i2c adapter for TI (formerly National Semiconductor)
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

/**
 * DOC: ds90u-i2c-adapter
 *
 * This module is intended to expose the I2C interface of TI (formerly
 * National Semiconductor) FPD-Link III ICs.
 *
 * It creates a virtual i2c adapter for providing gated access to additional
 * devices across the LVDS link.
 *
 * The ds90u-core module is the main driver for these ICs and this module
 * piggybacks on top of it.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include "ds90u-core.h"
#include "ds90u-i2c.h"
#include "ds90u-i2c-adapter.h"

static void ds90u_parent_lock_bus(struct i2c_adapter *adapter,
				unsigned int flags)
{
	struct ds90u_cdata *cdata =
		container_of(adapter, struct ds90u_cdata, adap);
	struct i2c_adapter *parent = cdata->client->adapter;

	i2c_lock_bus(parent, flags);
}

static int ds90u_parent_trylock_bus(struct i2c_adapter *adapter,
				  unsigned int flags)
{
	struct ds90u_cdata *cdata =
		container_of(adapter, struct ds90u_cdata, adap);
	struct i2c_adapter *parent = cdata->client->adapter;

	return i2c_trylock_bus(parent, flags);
}

static void ds90u_parent_unlock_bus(struct i2c_adapter *adapter,
				  unsigned int flags)
{
	struct ds90u_cdata *cdata =
		container_of(adapter, struct ds90u_cdata, adap);
	struct i2c_adapter *parent = cdata->client->adapter;

	i2c_unlock_bus(parent, flags);
}

/*
 * these lock operations look up the actual parent bus and attempt to transfer
 * the lock operation to it since we are not able to copy the bus_lock mutex.
 */
static const struct i2c_lock_operations ds90u_parent_lock_ops = {
	.lock_bus =    ds90u_parent_lock_bus,
	.trylock_bus = ds90u_parent_trylock_bus,
	.unlock_bus =  ds90u_parent_unlock_bus,
};


/* ds90u_master_xfer() and ds90u_smbus_xfer() :
 * ds90u i2c proxy calls. These i2c accesses go through the lvds link
 * and are dependent on the link to be stable. However the lvds link
 * can loose lock in case:
 * - graphics input clock changes in which case the internal pll will resynchronize
 * - the link is reconfigured during startup
 * - link is instable during startup
 * In case the lvds link is unstable the i2c access will return error EREMOTEIO
 * in which case the transfer is retried here.
 */
#define DS90U_NUMBER_OF_RETRIES 4
#define DS90U_SLEEP_INBETWEEN_RETRIES 100

int ds90u_master_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
		      int num)
{
	int ret = 0, retries = 0;
	struct ds90u_cdata *cdata =
		container_of(adap, struct ds90u_cdata, adap);

retry:
	ret = cdata->algo_parent.master_xfer(adap, msgs, num);

	if (ret == -EREMOTEIO) {
		retries++;
		if (retries < DS90U_NUMBER_OF_RETRIES) {
			msleep(DS90U_SLEEP_INBETWEEN_RETRIES);
			goto retry;
		} else {
			dev_err(&adap->dev, "%s: i2c transfer failed (%d) after %d retries\n",
			       __func__, ret, retries);
		}
	}

	return ret;
}

int ds90u_smbus_xfer(struct i2c_adapter *adap, u16 addr,
		      unsigned short flags, char read_write,
		      u8 command, int size, union i2c_smbus_data *data)
{
	int ret = 0, retries = 0;
	struct ds90u_cdata *cdata =
		container_of(adap, struct ds90u_cdata, adap);

retry:
	ret = cdata->algo_parent.smbus_xfer(adap, addr,
					     flags, read_write,
					     command, size, data);
	if (ret == -EREMOTEIO) {
		retries++;
		if (retries < DS90U_NUMBER_OF_RETRIES) {
			msleep(DS90U_SLEEP_INBETWEEN_RETRIES);
			goto retry;
		} else {
			dev_err(&adap->dev, "%s: i2c transfer failed (%d) after %d retries\n",
			       __func__, ret, retries);
		}
	}

	return ret;
}

/**
 * ds90u_register_i2c_adapter() - initialize i2c-adapter interface
 * Configures the driver with an i2c adapter interface that can be used to gate
 * access to other devices across the LVDS link.
 * @cdata:	device-specific client data struct
 */
void ds90u_register_i2c_adapter(struct ds90u_cdata *cdata)
{
	int ret;
	struct i2c_adapter *parent = cdata->client->adapter;
	const struct i2c_algorithm *i2c_x_algo = 0;

	dev_dbg(&cdata->client->dev, "%s\n", __func__);

	if (!cdata->i2c_adapter)
		return;

	/*
	 * Fill out the adapter structure with parent values.
	 * This is based on code in i2c-mux.c: i2c_add_mux_adapter().
	 * If we have issues with it we can also refer back to fpd3_i2c_adap.c
	 * in TI's kernel repository.
	 */
	cdata->adap.owner = THIS_MODULE;

	/* search for the local i2c-x adapter algo, eather parent is i2c-x or
	 * it is a ds90u proxy adapter in which case the i2c-x algo is stored in algo_parent.
	 */
	i2c_x_algo = parent->algo;
	if (i2c_x_algo->master_xfer == ds90u_master_xfer ||
	     i2c_x_algo->smbus_xfer == ds90u_smbus_xfer) {
		struct ds90u_cdata *cdata =
			container_of(parent, struct ds90u_cdata, adap);
		i2c_x_algo = &cdata->algo_parent;
	}

	cdata->algo = cdata->algo_parent = *i2c_x_algo;
	cdata->adap.algo = &cdata->algo;

	/* overwrite the callbacks with ds90u proxy calls */
	if (cdata->algo_parent.master_xfer)
		cdata->algo.master_xfer = ds90u_master_xfer;
	if (cdata->algo_parent.smbus_xfer)
		cdata->algo.smbus_xfer = ds90u_smbus_xfer;

	/* local_i2c_parent and parent adapter settings are copies, we could also take local_i2c_parent */
	cdata->adap.algo_data = parent->algo_data;

	/* The discovery logical adapter tree is (local-i2c -> serializer -> deseriaizer -> [touch,backlight,buton-bl] )
	 * however the parents is set to local-i2c's parent:
	 * local-i2c-parent -> [local-i2c , serializer , (deseriaizer -> [ touch, backlight,buton-bl]) ]
	 */
	cdata->adap.dev.parent = parent->dev.parent;
	cdata->adap.dev.of_node = cdata->client->dev.of_node;
	cdata->adap.retries = parent->retries;
	cdata->adap.timeout = parent->timeout;
	cdata->adap.quirks = parent->quirks;
	/* locking will recurse to i2c-x adapters's buslock */
	cdata->adap.lock_ops = &ds90u_parent_lock_ops;

	/*
	 * adap.nr and adap.name already initialized during probe
	 * adap.class also already set, but needs to be copied from parent
	 */
	cdata->adap.class = parent->class;

	/* Add driver-data reference since we are reusing parent's algo */
	cdata->adap.dev.driver_data = parent->dev.driver_data;

	if (cdata->adap.name[0])
		dev_dbg(&cdata->client->dev, "  Creating I2C adapter for %s\n", cdata->adap.name);

	ret = i2c_add_numbered_adapter(&cdata->adap);
	if (ret)
		dev_err(&cdata->client->dev, "i2c_add_numbered_adapter() failed -- %d\n", ret);
}

/**
 * ds90u_unregister_i2c_adapter() - remove the i2c-adapter interface
 * Called during link-loss or driver shutdown.
 * @cdata:	device-specific client data struct
 */
void ds90u_unregister_i2c_adapter(struct ds90u_cdata *cdata)
{
	dev_dbg(&cdata->client->dev, "%s\n", __func__);

	if (cdata->i2c_adapter)
		i2c_del_adapter(&cdata->adap);
}
