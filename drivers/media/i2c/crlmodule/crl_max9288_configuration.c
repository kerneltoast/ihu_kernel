/*
 * crl_max9288_configuration.c
 *
 * V4L-i2c platform driver for video input device on Delphi IHU board.
 * Copyright (C) 2017 Delphi Technologies, Inc.
 * Authors: Hakan Johansson <hakan.johansson@delphi.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */


/*
 * On Delphi IHU HW, export video_stream_active flag to user space via sysfs
 * '/sys/bus/i2c/drivers/crlmodule/xx/video_stream_active'
 *
 * This flag can be used by user-space application trigger external video source
 */

#include <linux/device.h>
#include <linux/i2c.h>

#include "crlmodule.h"
#include "crlmodule-regs.h"

struct crl_max9288 {
	int video_stream_active;
	struct i2c_client *client;
};

static ssize_t max9288_video_stream_active_show(struct device *dev,
				struct device_attribute *attr,
				char *buf) {
	struct crl_max9288 *max9288;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct crl_subdev *ssd = to_crlmodule_subdev(sd);
	struct crl_sensor *sensor = ssd->sensor;

	max9288 = sensor->sensor_specific_data;

	return snprintf(buf, PAGE_SIZE, "%d", max9288->video_stream_active);
}

static DEVICE_ATTR(video_stream_active, S_IRUGO, max9288_video_stream_active_show, NULL);


static struct attribute *max9288_attributes[] = {
	&dev_attr_video_stream_active.attr,
	NULL
};

static const struct attribute_group max9288_attr_group = {
	.attrs = max9288_attributes,
};

int max9288_sensor_init(struct i2c_client *client) {
	struct crl_max9288 *max9288;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct crl_subdev *ssd = to_crlmodule_subdev(sd);
	struct crl_sensor *sensor = ssd->sensor;

	dev_dbg(&client->dev, "%s\n", __func__);

	max9288 = devm_kzalloc(&client->dev, sizeof(*max9288), GFP_KERNEL);
	if (!max9288)
		return -ENOMEM;

	sensor->sensor_specific_data = max9288;
	max9288->client = client;
	max9288->video_stream_active = 0;

	/* create sysfs endpoint */
	return sysfs_create_group(&client->dev.kobj, &max9288_attr_group);
}

int max9288_sensor_cleanup(struct i2c_client *client) {
	struct crl_max9288 *max9288;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct crl_subdev *ssd = to_crlmodule_subdev(sd);
	struct crl_sensor *sensor = ssd->sensor;

	dev_dbg(&client->dev, "%s\n", __func__);

	max9288 = sensor->sensor_specific_data;
	if (!max9288)
		return -ENODEV;

	/* remove sysfs endpoint */
	sysfs_remove_group(&client->dev.kobj, &max9288_attr_group);
	return 0;
}

int max9288_sensor_stream_start(struct i2c_client *client) {
	struct crl_max9288 *max9288;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct crl_subdev *ssd = to_crlmodule_subdev(sd);
	struct crl_sensor *sensor = ssd->sensor;

	dev_dbg(&client->dev, "%s\n", __func__);

	max9288 = sensor->sensor_specific_data;
	if (!max9288)
		return -ENODEV;

	max9288->video_stream_active = 1;

	/* trigger sysfs notify on stream status change */
	sysfs_notify(&client->dev.kobj, NULL, "video_stream_active");
	return 0;
}

int max9288_sensor_stream_stop(struct i2c_client *client) {
	struct crl_max9288 *max9288;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct crl_subdev *ssd = to_crlmodule_subdev(sd);
	struct crl_sensor *sensor = ssd->sensor;

	dev_dbg(&client->dev, "%s\n", __func__);

	max9288 = sensor->sensor_specific_data;
	if (!max9288)
		return -ENODEV;

	max9288->video_stream_active = 0;

	/* trigger sysfs notify on stream status change */
	sysfs_notify(&client->dev.kobj, NULL, "video_stream_active");
	return 0;
}


