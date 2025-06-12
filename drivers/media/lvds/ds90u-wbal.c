// SPDX-License-Identifier: GPL
/*
 * ds90u-wbal.c - White balance for TI (formerly National Semiconductor)
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

/**
 * DOC: ds90u-wbal
 *
 * This module is intended to expose the white balance functions of TI (formerly
 * National Semiconductor) FPD-Link III ICs.
 *
 * The ds90u-core module is the main driver for these ICs and this module
 * piggybacks on top of it.
 *
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/string.h>
#include <linux/firmware.h>
#include "ds90u-core.h"
#include "ds90u-i2c.h"
#include "ds90u-wbal.h"

static void firmware_load(const struct firmware *fw, void *context)
{
	struct i2c_client *client = (struct i2c_client *)context;
	int index = 0;
	int lut_index = 0;
	int ret = 0;

	dev_dbg(&client->dev, "White Balance: firmware_load\n");

	if (fw == NULL) {
		dev_err(&client->dev, "White balance: Lut file returned NULL.\n");
		return;
	}

	if (fw->size == 0) {
		dev_err(&client->dev, "White balance: Lut file returned empty.\n");
		return;
	}


	if (fw->size != LUT_FILE_SIZE) {
		dev_err(&client->dev, "White balance: file size incorrect. expected = %u, loaded = %zu\n", LUT_FILE_SIZE, fw->size);
		return;
	}

	ret = ds90u_config_set(client, CFG_WBAL_EN);
	if (ret < 0) {
		dev_err(&client->dev, "White balance: Unable to enable feature (%d)\n", ret);
		return;
	}

	for (lut_index = 0; lut_index < NUMBER_OF_LUTS; lut_index++) {
		ret = ds90u_config_write(client, CFG_WBAL_PAGE, lut_index+1);
		if (ret < 0) {
			dev_err(&client->dev, "White balance: Unable to set LUT page %d (%d)\n", lut_index+1, ret);
			continue;
		}

		for (index = 0; index < COLOR_VALUES; index++) {
			ret = i2c_smbus_write_byte_data(client, index,
				fw->data[index + COLOR_VALUES * lut_index]);
			if (ret < 0)
				dev_err(&client->dev, "write 0x%02X to 0x%02X failed\n", index, fw->data[index + COLOR_VALUES * lut_index]);
			dev_dbg(&client->dev, "  Register (%#04x) = %#04x\n", index, fw->data[index + COLOR_VALUES * lut_index]);
		}
	}

	ds90u_config_write(client, CFG_WBAL_PAGE, 0);
	dev_info(&client->dev, "White Balance: Load Complete\n");

	release_firmware(fw);
}


/**
 * ds90u_init_white_bal() - initialize device white balance lut
 * Setup the white balance registers in the device
 * Assumes a valid link exists.
 * @client:	device-specific client struct
 */
void ds90u_init_white_bal(struct i2c_client *client)
{
	const char *filename;
	int ret;
	struct ds90u_cdata *cdata;

	dev_dbg(&client->dev, "%s\n", __func__);

	cdata = (struct ds90u_cdata *)i2c_get_clientdata(client);
	WARN_ON(cdata == NULL);
	if (cdata == NULL)
		return;

	filename = cdata->lut_file_name;
	if ((filename == NULL) || (filename[0] == '\0')) {
		dev_err(&client->dev, "White balance: filename invalid\n");
		return;
	}

	dev_dbg(&client->dev, "  Request_firmware. Filename = %s\n", filename);
	ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG, filename,
		&(client->dev), GFP_KERNEL, client, firmware_load);

	if (ret != 0) {
		dev_err(&client->dev, "White balance request failed. ret = %d\n", ret);
		return;
	}
}
