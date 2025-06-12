/*
 * Copyright 2014 Belkin Inc.
 * Copyright 2015 Andrew Lunn <andrew@lunn.ch>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/platform_data/leds-tlc591xx.h>

#define TLC591XX_MAX_LEDS	16

#define TLC591XX_REG_MODE1	0x00
#define MODE1_RESPON_ADDR_MASK	0xF0
#define MODE1_NORMAL_MODE	(0 << 4)
#define MODE1_SPEED_MODE	(1 << 4)

#define TLC591XX_REG_MODE2	0x01
#define MODE2_DIM		(0 << 5)
#define MODE2_BLINK		(1 << 5)
#define MODE2_OCH_STOP		(0 << 3)
#define MODE2_OCH_ACK		(1 << 3)

#define TLC591XX_REG_PWM(x)	(0x02 + (x))

/* LED Driver Output State, determine the source that drives LED outputs */
#define LEDOUT_OFF		0x0	/* Output LOW */
#define LEDOUT_ON		0x1	/* Output HI-Z */
#define LEDOUT_DIM		0x2	/* Dimming */
#define LEDOUT_BLINK		0x3	/* Blinking */
#define LEDOUT_MASK		0x3

#define CURRENT_MULTIPLIER_MASK 0x80

#define ldev_to_led(c)		container_of(c, struct tlc591xx_led, ldev)

struct tlc591xx_led {
	bool active;
	unsigned int led_no;
	struct led_classdev ldev;
	struct tlc591xx_priv *priv;
};

struct tlc591xx_priv {
	struct tlc591xx_led leds[TLC591XX_MAX_LEDS+1];
	struct regmap *regmap;
	unsigned int max_leds;
	unsigned int reg_ledout_offset;
	bool use_group_brightness_mode;
	bool use_low_current_multiplier;
	enum led_brightness (*brightness_get_saved)(void);
	void (*brightness_save)(enum led_brightness);
};

struct tlc591xx {
	unsigned int max_leds;
	unsigned int reg_ledout_offset;
	unsigned int reg_output_gain_offset;
};

static const struct tlc591xx tlc59116 = {
	.max_leds = 16,
	.reg_ledout_offset = 0x14,
	.reg_output_gain_offset = 0x1c,
};

static const struct tlc591xx tlc59108 = {
	.max_leds = 8,
	.reg_ledout_offset = 0x0c,
	.reg_output_gain_offset = 0x12,
};

static int
tlc591xx_set_mode(struct regmap *regmap, u8 mode)
{
	int err;
	u8 val;

	err = regmap_write(regmap, TLC591XX_REG_MODE1, MODE1_NORMAL_MODE);
	if (err)
		return err;

	val = MODE2_OCH_STOP | mode;

	return regmap_write(regmap, TLC591XX_REG_MODE2, val);
}

static int
tlc591xx_set_ledout(struct tlc591xx_priv *priv, struct tlc591xx_led *led,
		    u8 val)
{
	unsigned int i = (led->led_no % 4) * 2;
	unsigned int mask = LEDOUT_MASK << i;
	unsigned int addr = priv->reg_ledout_offset + (led->led_no >> 2);

	val = val << i;

	return regmap_update_bits(priv->regmap, addr, mask, val);
}

static int
tlc591xx_set_group_ledout(struct tlc591xx_priv *priv, u8 val)
{
	unsigned int addr = priv->reg_ledout_offset;
	int status, i;

	switch (val) {
	case LEDOUT_ON:
		val = 0x55;
		break;

	case LEDOUT_DIM:
		val = 0xAA;
		break;

	case LEDOUT_BLINK:
		val = 0xFF;
		break;

	case LEDOUT_OFF:
	default:
		val = 0x00;
		break;
	}

	for (i = 0; i < (priv->max_leds / 4); i++) {
		status = regmap_write(priv->regmap, addr+i, val);
		if (status)
			break;
	}
	return status;
}

static int
tlc591xx_set_pwm(struct tlc591xx_priv *priv, struct tlc591xx_led *led,
		 u8 brightness)
{
	u8 pwm = TLC591XX_REG_PWM(led->led_no);

	return regmap_write(priv->regmap, pwm, brightness);
}

static int
tlc591xx_brightness_set(struct led_classdev *led_cdev,
			enum led_brightness brightness)
{
	struct tlc591xx_led *led = ldev_to_led(led_cdev);
	struct tlc591xx_priv *priv = led->priv;
	int err=0;

	if (priv->use_group_brightness_mode) {
		switch (brightness) {
		case 0:
			err = tlc591xx_set_group_ledout(priv, LEDOUT_OFF);
			break;
		case LED_FULL:
			err = tlc591xx_set_group_ledout(priv, LEDOUT_ON);
			break;
		default:
			err = tlc591xx_set_group_ledout(priv, LEDOUT_BLINK);
			break;
		}
	} else {
		switch (brightness) {
		case 0:
			err = tlc591xx_set_ledout(priv, led, LEDOUT_OFF);
			break;
		case LED_FULL:
			err = tlc591xx_set_ledout(priv, led, LEDOUT_ON);
			break;
		default:
			err = tlc591xx_set_ledout(priv, led, LEDOUT_DIM);
			break;
		}
	}

	if (!err)
		err = tlc591xx_set_pwm(priv, led, brightness);

	if (!err && priv->brightness_save)
		priv->brightness_save(brightness);

	return err;
}

static void
tlc591xx_destroy_devices(struct tlc591xx_priv *priv, unsigned int j)
{
	int i = j;

	while (--i >= 0) {
		if (priv->leds[i].active) {
			/* prevent potential call to brightness_set to avoid
			   fatal exception during short period after remove()
			   when it is still possible to process brightness
			   change while resources are being unloaded */
			priv->leds[i].ldev.brightness_set_blocking = NULL;

			/* set LED_OFF here as led_classdev_unregister will not
			   be able to do this anymore due to cleared pointer */
			tlc591xx_brightness_set(&priv->leds[i].ldev, LED_OFF);

			led_classdev_unregister(&priv->leds[i].ldev);
		}
	}
}

static int tlc591xx_brightness_restore(struct tlc591xx_led *led)
{
	enum led_brightness brightness_to_restore;
	int err = 0;

	if (led->priv->brightness_get_saved) {
		brightness_to_restore = led->priv->brightness_get_saved();
		if (brightness_to_restore) {
			err = tlc591xx_brightness_set(&led->ldev,
						      brightness_to_restore);
			if (err)
				dev_warn(led->ldev.dev,
					 "couldn't restore buttons brightness %d\n",
					 brightness_to_restore);
		}
	}

	return err;
}

static int
tlc591xx_configure(struct device *dev,
		   struct tlc591xx_priv *priv,
		   const struct tlc591xx *tlc591xx)
{
	unsigned int i;
	int err = 0;

	if (priv->use_low_current_multiplier) {
		err = regmap_update_bits(
			priv->regmap,
			tlc591xx->reg_output_gain_offset,
			CURRENT_MULTIPLIER_MASK,
			0);
		if (err) {
			/* this could be a critical parameter,
			   so return any error */
			dev_err(dev, "unable to set low current multiplier\n");
			return err;
		}
	}

	tlc591xx_set_mode(priv->regmap, MODE2_DIM);
	if (priv->use_group_brightness_mode) {
		/* group brightness control register is conveniently at the
		   end of the individual brightness registers */
		struct tlc591xx_led *led = &priv->leds[tlc591xx->max_leds];

		for (i = 0; i < tlc591xx->max_leds; i++ ) {
			u8 pwm;
			/* set individual channels to 0xFF control will then
			   be done through group brightness register */
			pwm = TLC591XX_REG_PWM(i);
			regmap_write(priv->regmap, pwm, 0xFF);
		}
		led->priv = priv;
		led->led_no = tlc591xx->max_leds; /* group brightness reg */
		led->ldev.brightness_set_blocking = tlc591xx_brightness_set;
		led->ldev.max_brightness = LED_FULL;
		err = tlc591xx_brightness_restore(led);
		if (err)
			return err;
		err = led_classdev_register(dev, &led->ldev);
		if (err < 0) {
			dev_err(dev, "couldn't register LED %s\n",
				led->ldev.name);
			return err;
		}
	} else {
		for (i = 0; i < TLC591XX_MAX_LEDS; i++) {
			struct tlc591xx_led *led = &priv->leds[i];

			if (!led->active)
				continue;

			led->priv = priv;
			led->led_no = i;
			led->ldev.brightness_set_blocking =
				tlc591xx_brightness_set;
			led->ldev.max_brightness = LED_FULL;
			err = tlc591xx_brightness_restore(led);
			if (err)
				goto exit;
			err = led_classdev_register(dev, &led->ldev);
			if (err < 0) {
				dev_err(dev, "couldn't register LED %s\n",
					led->ldev.name);
				goto exit;
			}
		}
	}

	return 0;

exit:
	tlc591xx_destroy_devices(priv, i);
	return err;
}

static const struct regmap_config tlc591xx_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x1e,
};

static const struct of_device_id of_tlc591xx_leds_match[] = {
	{ .compatible = "ti,tlc59116",
	  .data = &tlc59116 },
	{ .compatible = "ti,tlc59108",
	  .data = &tlc59108 },
	{},
};
MODULE_DEVICE_TABLE(of, of_tlc591xx_leds_match);

static int
tlc591xx_probe(struct i2c_client *client,
	       const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node, *child;
	struct device *dev = &client->dev;
	const struct of_device_id *match;
	const struct tlc591xx *tlc591xx = NULL;
	struct tlc591xx_priv *priv;
	struct tlc591xx_platform_data *pdata = client->dev.platform_data;
	int err, count;

	dev_dbg(dev, "%s\n", __func__);

	match = of_match_device(of_tlc591xx_leds_match, dev);
	if (match)
		tlc591xx = match->data;

	if ((id) && (59108 == id->driver_data)) {
		dev_dbg(dev, "id==59108\n");
		tlc591xx = &tlc59108;
	} else if ((id) && (59116 == id->driver_data)) {
		dev_dbg(dev, "id==59116\n");
		tlc591xx = &tlc59116;
	}

	if (!tlc591xx)
		return -ENODEV;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->max_leds = tlc591xx->max_leds;
	priv->regmap = devm_regmap_init_i2c(client, &tlc591xx_regmap);
	if (IS_ERR(priv->regmap)) {
		err = PTR_ERR(priv->regmap);
		dev_err(dev, "Failed to allocate register map: %d\n", err);
		return err;
	}
	priv->reg_ledout_offset = tlc591xx->reg_ledout_offset;

	if (NULL == pdata) {
		priv->use_group_brightness_mode = of_property_read_bool(np,
			"use-group-brightness-mode");
		priv->use_low_current_multiplier = of_property_read_bool(np,
			"use-low-current-multiplier");
	} else {
		priv->use_group_brightness_mode =
			pdata->use_group_brightness_mode;
		priv->use_low_current_multiplier =
			pdata->use_low_current_multiplier;
	}

	i2c_set_clientdata(client, priv);

	if (priv->use_group_brightness_mode) {
		priv->leds[tlc591xx->max_leds].active = true;
		if (np)
			priv->leds[tlc591xx->max_leds].ldev.name =
				of_get_property(np, "label", NULL) ? : np->name;
		else
			priv->leds[tlc591xx->max_leds].ldev.name = id->name;
		priv->leds[tlc591xx->max_leds].ldev.default_trigger =
			of_get_property(np, "linux,default-trigger", NULL);
	} else {
		count = of_get_child_count(np);
		if (!count || count > tlc591xx->max_leds)
			return -EINVAL;

		for_each_child_of_node(np, child) {
			int reg;
			err = of_property_read_u32(child, "reg", &reg);
			if (err){
				of_node_put(child);
				return err;
			}
			if (reg < 0 || reg >= tlc591xx->max_leds ||
				priv->leds[reg].active) {
				of_node_put(child);
				return -EINVAL;
			}
			if (priv->leds[reg].active) {
				of_node_put(child);
				return -EINVAL;
			}
			priv->leds[reg].active = true;
			priv->leds[reg].ldev.name =
				of_get_property(child, "label", NULL) ? :
					child->name;
			priv->leds[reg].ldev.default_trigger =
				of_get_property(child, "linux,default-trigger",
					NULL);
		}
	}

	if (pdata) {
		priv->brightness_get_saved = pdata->brightness_get_saved;
		priv->brightness_save = pdata->brightness_save;
	}

	err = tlc591xx_configure(dev, priv, tlc591xx);
	if (-EREMOTEIO == err) {
		msleep(20);
		dev_notice(&client->dev, "Retry configuration: %d\n", err);
		err = tlc591xx_configure(dev, priv, tlc591xx);
		if (err) {
			dev_err(&client->dev, "Failed to configure: %d\n", err);
			return err;
		}
	}

	return 0;
}

static int
tlc591xx_remove(struct i2c_client *client)
{
	struct tlc591xx_priv *priv = i2c_get_clientdata(client);

	tlc591xx_destroy_devices(priv, ARRAY_SIZE(priv->leds));

	return 0;
}

static const struct i2c_device_id tlc591xx_id[] = {
	{ "tlc59116", 59116 },
	{ "tlc59108", 59108 },
	{},
};
MODULE_DEVICE_TABLE(i2c, tlc591xx_id);

static struct i2c_driver tlc591xx_driver = {
	.driver = {
		.name = "tlc591xx",
		.of_match_table = of_match_ptr(of_tlc591xx_leds_match),
	},
	.probe = tlc591xx_probe,
	.remove = tlc591xx_remove,
	.id_table = tlc591xx_id,
};

module_i2c_driver(tlc591xx_driver);

MODULE_AUTHOR("Andrew Lunn <andrew@lunn.ch>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TLC591XX LED driver");
