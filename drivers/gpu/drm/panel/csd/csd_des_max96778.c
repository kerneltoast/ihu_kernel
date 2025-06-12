// SPDX-License-Identifier: GPL-2.0

#include "csd_des_max96778.h"
#include "csd_des_max96778_i.h"

static int csd_des_max96778_wait_vlock(struct csd_data *csd)
{
	struct device *dev = csd_get_device(csd);
	int retries = 2;

	for (;;) {
		int ret;
		u8 reg;

		ret = csd_read_des_8pc(csd, MAX96778_VPRBS, &reg, 0);
		if (ret) {
			dev_err(dev, "Failed to read video lock\n");
			return ret;
		}

		if (reg & MAX96778_VPRBS_VIDEO_LOCK)
			return 0;

		if (!retries--) {
			dev_err(dev, "No video lock\n");
			return -ETIME;
		}

		csd_msleep(20);
	}
}

static const u16 legacy_vtrg_regs[] = {
	0x4507, 0x7010, 0x7011, 0x7012, 0x7014, 0x7015, 0x7016, 0x7017,
	0x7044, 0x7045, 0x7046, 0x7047, 0x7048, 0x7049, 0x704A, 0x704B,
	0x704C, 0x704D, 0x704E, 0x704F, 0x7050, 0x7051, 0x7052, 0x7053,
	0x7020, 0x702D, 0x702E, 0x702F, 0x7030,
};

struct legacy_vtrg_mode {
	struct drm_display_mode mode;
	u8                      values[ARRAY_SIZE(legacy_vtrg_regs)];
};

static const struct legacy_vtrg_mode legacy_vtrg_modes[] = {
	{
		.mode = {
			DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500,
			1920, 2008, 2052, 2200, 0,
			1080, 1084, 1089, 1125, 0,
			DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC)
		},
		.values = {
			0x80, 0x83, 0x00, 0x00, 0x62, 0x00, 0x2C, 0x01,
			0xB9, 0x22, 0x80, 0x07, 0x08, 0x03, 0xB2, 0x00,
			0x65, 0x04, 0x38, 0x04, 0x29, 0x00, 0x05, 0x00,
			0x07, 0x01, 0x01, 0x01, 0x28,
		},
	},
	{
		.mode = {
			DRM_MODE("1152x1536", DRM_MODE_TYPE_DRIVER, 123340,
			1152, 1184, 1224, 1280, 0,
			1536, 1539, 1540, 1606, 0,
			DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC)
		},
		.values = {
			0x80, 0x83, 0x05, 0x00, 0x04, 0x02, 0xCD, 0x07,
			0x53, 0x18, 0x80, 0x04, 0xD4, 0x01, 0xC3, 0x80,
			0x46, 0x06, 0x00, 0x06, 0x43, 0x00, 0x01, 0x80,
			0x07, 0x01, 0x01, 0x01, 0x28,
		},
	},
	{
		.mode = {
			DRM_MODE("1920x720", DRM_MODE_TYPE_DRIVER, 104000,
			1920, 2004, 2196, 2318, 0,
			720, 723, 733, 748, 0,
			DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC)
		},
		.values = {
			0x80, 0x84, 0x07, 0x00, 0xB8, 0x00, 0x71, 0x03,
			0x3A, 0x34, 0x80, 0x07, 0x14, 0x07, 0x54, 0x84,
			0xEC, 0x02, 0xD0, 0x02, 0x19, 0x00, 0x0A, 0x80,
			0x07, 0x01, 0x01, 0x01, 0x28,
		},
	},
	{
		.mode = {
			DRM_MODE("1024x768", DRM_MODE_TYPE_DRIVER, 65000,
			1024, 1048, 1184, 1344, 0,
			768, 771, 777, 806, 0,
			DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC)
		},
		.values = {
			0x80, 0x88, 0x00, 0x00, 0x0C, 0x00, 0x6A, 0x00,
			0x77, 0x30, 0x00, 0x04, 0xAD, 0x0A, 0xE8, 0x04,
			0x26, 0x03, 0x00, 0x03, 0x23, 0x00, 0x06, 0x00,
			0x07, 0x01, 0x01, 0x01, 0x28,
		},
	},
	{
		.mode = {
			DRM_MODE("640x480", DRM_MODE_TYPE_DRIVER, 25175,
			640, 656, 752, 800, 0,
			480, 490, 492, 525, 0,
			DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC)
		},
		.values = {
			0x80, 0x96, 0x00, 0x00, 0x14, 0x00, 0xDE, 0x01,
			0x68, 0x4A, 0x80, 0x02, 0x65, 0x0D, 0xEE, 0x08,
			0x0D, 0x02, 0xE0, 0x01, 0x23, 0x00, 0x02, 0x00,
			0x07, 0x01, 0x01, 0x01, 0x28,
		},
	},
};

static const struct legacy_vtrg_mode *legacy_vtrg_lookup_mode(
	const struct drm_display_mode *mode)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(legacy_vtrg_modes); i++)
		if (drm_mode_equal(&legacy_vtrg_modes[i].mode, mode))
			return &legacy_vtrg_modes[i];

	return NULL;
}

static int csd_des_max96778_legacy_vtrg(struct csd_data *csd,
	const struct drm_display_mode *mode)
{
	struct device *dev = csd_get_device(csd);
	const struct legacy_vtrg_mode *vtrg_mode;
	int ret;
	int i;

	vtrg_mode = legacy_vtrg_lookup_mode(mode);
	if (!vtrg_mode) {
		dev_warn(dev, "The currently used display timing is not fully supported by the used combination of software and converter board. Display output might be unstable.\n");
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(legacy_vtrg_regs); i++) {
		ret = csd_write_des_8pc(csd, legacy_vtrg_regs[i],
			vtrg_mode->values[i], 0);

		if (ret)
			return ret;
	}

	/* Enable VTRG */
	ret = csd_write_des_8pc(csd, MAX96778_VTRG_CTRL_B0,
		MAX96778_VTRG_CTRL_B0_VID_EN_MODE_AUTO |
		MAX96778_VTRG_CTRL_B0_VTRG_EN,
		MAX96778_VTRG_CTRL_B0_VID_EN_MODE_MASK |
		MAX96778_VTRG_CTRL_B0_VTRG_RST |
		MAX96778_VTRG_CTRL_B0_VTRG_EN);
	if (ret)
		return ret;

	return 0;
}

static int csd_des_max96778_auto_vtrg(struct csd_data *csd,
	const struct drm_display_mode *mode)
{
	u8 clock[] = {
		(u8) (mode->clock >> 0),
		(u8) (mode->clock >> 8),
		(u8) (mode->clock >> 16),
	};
	int ret;

	/* Set pixel clock in kHz */
	ret = csd_write_des_c(csd, MAX96778_PIX_RATE_PER_B0, clock,
		sizeof(clock));
	if (ret)
		return ret;

	/* Enable VTRG auto enabling as part of DP link training */
	ret = csd_write_des_8pc(csd, MAX96778_AUTO_VTRG_EN,
		MAX96778_AUTO_VTRG_EN_ENABLE,
		MAX96778_AUTO_VTRG_EN_ENABLE);
	if (ret)
		return ret;

	return 0;
}

static int csd_des_max96778_mandatory_settings(struct csd_data *csd)
{
	u8 err_ch[] = { 0x28, 0x68 };
	u8 err_ch_check[ARRAY_SIZE(err_ch)] = { 0x7F, 0x7F };
	int ret;

	/* Set error channel threshold voltages for PHY A */
	ret = csd_write_des_pc(csd, MAX96778_RLMS58_A, err_ch, err_ch_check,
		sizeof(err_ch));

	if (ret)
		return ret;

	/* Set error channel threshold voltages for PHY B */
	ret = csd_write_des_pc(csd, MAX96778_RLMS58_B, err_ch, err_ch_check,
		sizeof(err_ch));

	if (ret)
		return ret;

	/* Increase robustness of GMSL Lock (undocumented, no check) */
	ret = csd_write_des_8pc(csd, 0x0302, 0x10, 0);
	if (ret)
		return ret;

	return 0;
}

static int csd_des_max96778_reset(struct csd_data *csd)
{
	struct device *dev = csd_get_device(csd);
	u8 ctrl0 = MAX96778_CTRL0_RESET_ALL;
	u8 status[2];
	int ret;

	/*
	 * We don't need to check the return value, as this write will return
	 * an error code in any case, either because it has really failed or
	 * because it has succeeded and deserializer has reset.
	 */
	csd_write_des(csd, MAX96778_CTRL0, &ctrl0, sizeof(ctrl0));

	/*
	 * Wait for reset to complete and link to lock again.
	 * Spec says 45 ms typ. and 60 ms max.
	 */
	csd_msleep(100);

	ret = csd_read_des_c(csd, MAX96778_DP_TRAIN_STATUS, status,
		sizeof(status));

	if (ret)
		return ret;

	if (status[0] || status[1]) {
		dev_err(dev, "Converter board reset has failed\n");
		return -EIO;
	}

	return 0;
}

int csd_des_max96778_setup(struct csd_data *csd,
	const struct drm_display_mode *mode, u8 chip_rev)
{
	u16 h_front_porch = (u16) (mode->hsync_start - mode->hdisplay);
	u16 h_sync_width = (u16) (mode->hsync_end - mode->hsync_start);
	u16 h_back_porch = (u16) (mode->htotal - mode->hsync_end);
	u16 v_front_porch = (u16) (mode->vsync_start - mode->vdisplay);
	u16 v_sync_width = (u16) (mode->vsync_end - mode->vsync_start);
	u16 v_back_porch = (u16) (mode->vtotal - mode->vsync_end);
	/* depends on MAX96778_LANE_COUNT_COUNT_TWO */
	u16 hwords = (u16) (mode->hdisplay * 3 / 2 - 2);
	/* depends on MAX96778_LINK_RATE_LINK_RATE_2_70 */
	u16 nvid = 0x8000;
	u16 mvid = (u16) (
		(((long long) mode->clock) * ((long long) nvid)) / 270000L);
	u16 tuc = 0x0040;
	u8 sync_pol = 0;
	u8 settings1[] = {
		/* horizontal active pixels LSB */
		/* MAX96778_HRES_B0*/       (u8) mode->hdisplay,
		/* horizontal active pixels MSB */
		/* MAX96778_HRES_B1 */      (u8) (mode->hdisplay >> 8),
		/* horizontal front porch LSB */
		/* MAX96778_HFP_B0 */       (u8) h_front_porch,
		/* horizontal front porch MSB */
		/* MAX96778_HFP_B1 */       (u8) (h_front_porch >> 8),
		/* horizontal sync width LSB */
		/* MAX96778_HSW_B0 */       (u8) h_sync_width,
		/* horizontal sync width MSB */
		/* MAX96778_HSW_B1 */       (u8) (h_sync_width >> 8),
		/* horizontal back porch LSB */
		/* MAX96778_HBP_B0 */       (u8) h_back_porch,
		/* horizontal back porch MSB */
		/* MAX96778_HBP_B1 */       (u8) (h_back_porch >> 8),
		/* vertical active pixels LSB */
		/* MAX96778_VRES_B0 */      (u8) mode->vdisplay,
		/* vertical active pixels MSB */
		/* MAX96778_VRES_B1 */      (u8) (mode->vdisplay >> 8),
		/* vertical front porch LSB */
		/* MAX96778_VFP_B0 */       (u8) v_front_porch,
		/* vertical front porch MSB */
		/* MAX96778_VFP_B1 */       (u8) (v_front_porch >> 8),
		/* vertical sync width LSB */
		/* MAX96778_VSW_B0 */       (u8) v_sync_width,
		/* vertical sync width MSB */
		/* MAX96778_VSW_B1 */       (u8) (v_sync_width >> 8),
		/* vertical back porch LSB */
		/* MAX96778_VBP_B0 */       (u8) v_back_porch,
		/* vertical back porch MSB */
		/* MAX96778_VBP_B1 */       (u8) (v_back_porch >> 8),
		/* 16-bit words of active data per line LSB */
		/* MAX96778_HWORDS_B0 */    (u8) hwords,
		/* 16-bit words of active data per line MSB */
		/* MAX96778_HWORDS_B1 */    (u8) (hwords >> 8),
		/* MVID MSA parameter for asynchronous clocking LSB */
		/* MAX96778_MVID_B0 */      (u8) mvid,
		/* MVID MSA parameter for asynchronous clocking MSB */
		/* MAX96778_MVID_B1 */      (u8) (mvid >> 8),
		/* NVID MSA parameter for asynchronous clocking LSB */
		/* MAX96778_NVID_B0 */      (u8) nvid,
		/* NVID MSA parameter for asynchronous clocking MSB */
		/* MAX96778_NVID_B1 */      (u8) (nvid >> 8),
		/* TUC value LSB */
		/* MAX96778_TUC_VALUE_B0 */ (u8) tuc,
		/* TUC value MSB */
		/* MAX96778_TUC_VALUE_B1 */ (u8) (tuc >> 8),
	};
	u8 settings2[] = {
		/* Spread Spectrum config */
		/* 0xE7B0 */ 0x01,
		/* 0xE7B1 */ 0x04,
		/* EDP config (depends on MAX96778_LINK_RATE_LINK_RATE_2_70) */
		/* 0xE7B2 */ 0x50,
		/* 0xE7B3 */ 0x00,
		/* 0xE7B4 */ 0x00,
		/* 0xE7B5 */ 0x40,
		/* 0xE7B6 */ 0x6C,
		/* 0xE7B7 */ 0x20,
		/* 0xE7B8 */ 0x07,
		/* 0xE7B9 */ 0x00,
		/* 0xE7BA */ 0x01,
		/* 0xE7BB */ 0x00,
		/* 0xE7BC */ 0x00,
		/* 0xE7BD */ 0x00,
		/* 0xE7BE */ 0x52,
		/* 0xE7BF */ 0x00,
	};
	int ret;

	if (mode->flags & DRM_MODE_FLAG_NVSYNC)
		sync_pol |= MAX96778_HVPOL_VSYNC_POL;

	if (mode->flags & DRM_MODE_FLAG_NHSYNC)
		sync_pol |= MAX96778_HVPOL_HSYNC_POL;

	/* Reset deserializer to well known state */
	ret = csd_des_max96778_reset(csd);
	if (ret)
		return ret;

	/* mandatory register programming */
	ret = csd_des_max96778_mandatory_settings(csd);
	if (ret)
		return ret;

	/* Set DP link rate */
	ret = csd_write_des_8pc(csd, MAX96778_LINK_RATE,
		MAX96778_LINK_RATE_LINK_RATE_2_70,
		MAX96778_LINK_RATE_LINK_RATE_MASK);
	if (ret)
		return ret;

	/* Set DP lane count */
	ret = csd_write_des_8pc(csd, MAX96778_LANE_COUNT,
		MAX96778_LANE_COUNT_COUNT_TWO,
		MAX96778_LANE_COUNT_COUNT_MASK);
	if (ret)
		return ret;

	/* apply settings arrays */
	ret = csd_write_des_c(csd, MAX96778_HRES_B0, settings1,
		sizeof(settings1));
	if (ret)
		return ret;

	ret = csd_write_des_c(csd, 0xE7B0, settings2, sizeof(settings2));
	if (ret)
		return ret;

	/* Set sync polarity */
	ret = csd_write_des_8pc(csd, MAX96778_HVPOL,
		sync_pol,
		MAX96778_HVPOL_VSYNC_POL |
		MAX96778_HVPOL_HSYNC_POL);
	if (ret)
		return ret;

	/*
	 * Setup VTRG
	 *
	 * Rev. 4 and before: support only legacy VTRG
	 * Rev. 5: Improved VTRG, but still not full feature set
	 * Rev. 6 and newer: full featured automatic VTRG with best accuracy
	 */
	if (chip_rev < 6)
		ret = csd_des_max96778_legacy_vtrg(csd, mode);
	else
		ret = csd_des_max96778_auto_vtrg(csd, mode);

	if (ret)
		return ret;

	/* Select DP link training command */
	ret = csd_write_des_8c(csd, MAX96778_DP_COMMAND,
		MAX96778_DP_COMMAND_TRAIN);
	if (ret)
		return ret;

	/* typically we already have video lock at this point right away */
	ret = csd_des_max96778_wait_vlock(csd);
	if (ret)
		return ret;

	/* Run DP training (register undocumented, no error checking) */
	ret = csd_write_des_8pc(csd, MAX96778_DP_EXECUTE,
		MAX96778_DP_EXECUTE_RUN,
		0);
	if (ret)
		return ret;

	return 0;
}

int csd_des_max96778_status(struct csd_data *csd, bool *configured)
{
	struct device *dev = csd_get_device(csd);
	int retry;

	/* Some errors seem to be sticky, we need to repeat to clear old ones */
	for (retry = 0; retry < 3; retry++) {
		bool reconfigure = false;
		bool recheck = true;
		u8 status[2];
		int ret;

		ret = csd_read_des_pc(csd, MAX96778_DP_TRAIN_STATUS, status,
			NULL, sizeof(status));

		if (ret)
			return ret;

		if (status[0] == MAX96778_DP_TRAIN_STATUS_OK) {
			*configured = true;
			return 0;
		}

		if (status[0] == MAX96778_DP_TRAIN_STATUS_HPD) {
			dev_info(dev, "converter board: HPD event or IRQ occurred\n");
			continue;
		}

		if (status[0] != MAX96778_DP_TRAIN_STATUS_ERR) {
			dev_warn(dev, "converter board: Unknown DP link training status (0x%02hhX 0x%02hhX)\n",
				status[0], status[1]);
			continue;
		}

		/* Ignore global flag bit, unless it is the only one set */
		if (status[1] == MAX96778_DP_TRAIN_ERROR_GFLAG) {
			dev_warn(dev, "converter board: Unknown DP link training error\n");
			continue;
		}

		if (status[1] & MAX96778_DP_TRAIN_ERROR_TP1) {
			dev_err(dev, "converter board: DP link training phase 1 failed\n");
			reconfigure = true;
		}

		if (status[1] & MAX96778_DP_TRAIN_ERROR_TP2) {
			dev_err(dev, "converter board: DP link training phase 2 failed\n");
			reconfigure = true;
		}

		if (status[1] & MAX96778_DP_TRAIN_ERROR_PARMS) {
			dev_err(dev, "converter board: DP link training failed due to missing parameters\n");
			reconfigure = true;
		}

		if (status[1] & MAX96778_DP_TRAIN_ERROR_NOHPD) {
			dev_info(dev, "converter board: DP link training failed due to missing HPD\n");
			/*
			 * There is nothing connected to converter board,
			 * we can consider configuration complete.
			 */
			recheck = false;
		}

		if (status[1] & MAX96778_DP_TRAIN_ERROR_NOVL) {
			dev_err(dev, "converter board: DP link training failed due to missing video lock\n");
			reconfigure = true;
		}

		if (status[1] & MAX96778_DP_TRAIN_ERROR_WDG)
			dev_warn(dev, "converter board: DP link training failed due to watchdog timer bite\n");

		if (status[1] & MAX96778_DP_TRAIN_ERROR_VLOST) {
			dev_err(dev, "converter board: Video lock lost after DP link training\n");
			reconfigure = true;
		}

		if (reconfigure || !recheck) {
			*configured = !reconfigure;
			return 0;
		}
	}

	*configured = false;
	return 0;
}
