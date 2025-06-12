// SPDX-License-Identifier: GPL-2.0

#include <linux/gpio/consumer.h>

#include "csd_ser_max96757.h"
#include "csd_ser_max96757_i.h"

/**
 * csd_max96757_run_od_gpio_test() - Checks signal path from MAX96757
 * @csd:    pointer to csd_data
 * @result: test result
 * @gpio:   GPIO on SOC side
 * @mfp:    GPIO on serializer side
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_max96757_run_od_gpio_test(struct csd_data *csd,
	enum csd_gpio_test_result *result, struct gpio_desc *gpio,
	unsigned int mfp)
{
	bool passed_low = true;
	bool passed_high = true;
	int ret;

	/* Set GPIO low */
	ret = csd_write_ser_8pc(csd, MAX96757_GPIO_A(mfp),
		0,
		(u8) ~MAX96757_GPIO_A_GPIO_IN);
	if (ret)
		return ret;

	if (gpiod_get_value_cansleep(gpio))
		passed_low = false;

	/* Set GPIO high */
	ret = csd_write_ser_8pc(csd, MAX96757_GPIO_A(mfp),
		MAX96757_GPIO_A_GPIO_OUT,
		(u8) ~MAX96757_GPIO_A_GPIO_IN);
	if (ret)
		return ret;

	if (!gpiod_get_value_cansleep(gpio))
		passed_high = false;

	*result = csd_gpio_test_eval(passed_low, passed_high);

	return 0;
}

/**
 * csd_max96757_run_errb_test() - Checks ERRB signal path from MAX96757
 * @csd:    pointer to csd_data
 * @result: test result
 * @errb:   ERRB GPIO
 * @gmsl2:  true when operating in GMSL2 mode
 *
 * Note: Currently the ERRB signal is not used anywhere else. Therefore no
 * further error handling is performed in case writes fail. Once the signal
 * is needed for some other purpose the error handling in this function needs
 * to be improved or this test needs to be removed.
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96757_run_errb_test(struct csd_data *csd,
	enum csd_gpio_test_result *result, struct gpio_desc *errb, bool gmsl2)
{
	return csd_max96757_run_od_gpio_test(csd, result, errb, 1);
}

/**
 * csd_max96757_run_int_test() - Checks INT signal path from MAX96757
 * @csd:    pointer to csd_data
 * @result: test result
 * @irq:    IRQ GPIO
 * @gmsl2:  true when operating in GMSL2 mode
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96757_run_int_test(struct csd_data *csd,
	enum csd_gpio_test_result *result, struct gpio_desc *irq, bool gmsl2)
{
	bool passed_low = true;
	bool passed_high = true;
	int ret;

	/* Set GPIO low */
	if (gmsl2)
		ret = csd_write_ser_8pc(csd, MAX96757_GPIO_A(2),
			MAX96757_GPIO_A_RES_CFG,
			(u8) ~MAX96757_GPIO_A_GPIO_IN);
	else
		ret = csd_write_ser_8c(csd, MAX96757_GMSL1_F,
			0);

	if (ret)
		return ret;

	if (gpiod_get_value_cansleep(irq))
		passed_low = false;

	/* Set GPIO high */
	if (gmsl2)
		ret = csd_write_ser_8pc(csd, MAX96757_GPIO_A(2),
			MAX96757_GPIO_A_RES_CFG |
			MAX96757_GPIO_A_GPIO_OUT,
			(u8) ~MAX96757_GPIO_A_GPIO_IN);
	else
		ret = csd_write_ser_8c(csd, MAX96757_GMSL1_F,
			MAX96757_GMSL1_F_SET_GPO);

	if (ret)
		return ret;

	if (!gpiod_get_value_cansleep(irq))
		passed_high = false;

	/* Re-enable reception */
	if (gmsl2)
		ret = csd_write_ser_8pc(csd, MAX96757_GPIO_A(2),
			MAX96757_GPIO_A_RES_CFG |
			MAX96757_GPIO_A_GPIO_OUT |
			MAX96757_GPIO_A_GPIO_RX_EN,
			(u8) ~MAX96757_GPIO_A_GPIO_IN);
	else
		ret = csd_write_ser_8c(csd, MAX96757_GMSL1_F,
			MAX96757_GMSL1_F_GPO_RX_EN);

	if (ret)
		return ret;

	*result = csd_gpio_test_eval(passed_low, passed_high);

	return 0;
}

/**
 * csd_max96757_run_lock_test() - Checks LOCK signal path from MAX96757
 * @csd:    pointer to csd_data
 * @result: test result
 * @lock:   LOCK GPIO
 * @gmsl2:  true when operating in GMSL2 mode
 *
 * Note: Currently the LOCK signal is not used anywhere else. Therefore no
 * further error handling is performed in case writes fail. Once the signal
 * is needed for some other purpose the error handling in this function needs
 * to be improved or this test needs to be removed.
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96757_run_lock_test(struct csd_data *csd,
	enum csd_gpio_test_result *result, struct gpio_desc *lock, bool gmsl2)
{
	return csd_max96757_run_od_gpio_test(csd, result, lock, 0);
}

/**
 * set_deskew_timing() - Write video timing to deskew block
 * @csd:  pointer to csd_data
 * @mode: display mode
 *
 * Return: 0 on success, negative error code otherwise
 */
static int set_deskew_timing(struct csd_data *csd,
	const struct drm_display_mode *mode)
{
	const u16 hsync = mode->hsync_end - mode->hsync_start;
	const u16 vsync = mode->vsync_end - mode->vsync_start;
	const u16 vfp = mode->vsync_start - mode->vdisplay;
	const u16 vbp = mode->vtotal - mode->vsync_end;
	const u16 hfp = mode->hsync_start - mode->hdisplay;
	const u16 hbp = mode->htotal - mode->hsync_end;
	u8 set1[] = {
		/* hsync low byte (MAX96757_DSI5) */
		(u8) hsync,

		/* vsync low byte (MAX96757_DSI6) */
		(u8) vsync,

		/* vsync high nibble and hsync high nibble (MAX96757_DSI7) */
		MAX96757_DSI7_VSYNC_H((u8) (vsync >> 8)) |
		MAX96757_DSI7_HSYNC_H((u8) (hsync >> 8)),
	};
	u8 set2[] = {
		/* vfp low byte (MAX96757_DSI37) */
		(u8) vfp,

		/* vbp low nibble and vfp high nibble (MAX96757_DSI38) */
		MAX96757_DSI38_VBP_L((u8) (vbp & 0xF)) |
		MAX96757_DSI38_VFP_H((u8) (vfp >> 8)),

		/* vbp high byte (MAX96757_DSI39) */
		(u8) (vbp >> 4),

		/* vactive low byte (MAX96757_DSI40) */
		(u8) mode->vdisplay,

		/* vactive high nibble (MAX96757_DSI41) */
		MAX96757_DSI41_VACTIVE_H((u8) (mode->vdisplay >> 8)),

		/* hfp low byte (MAX96757_DSI42) */
		(u8) hfp,

		/* hbp low nibble and hfp high nibble (MAX96757_DSI43) */
		MAX96757_DSI43_HBP_L((u8) (hbp & 0xF)) |
		MAX96757_DSI43_HFP_H((u8) (hfp >> 8)),

		/* hbp high byte (MAX96757_DSI44) */
		(u8) (hbp >> 4),

		/* hactive low byte (MAX96757_DSI45) */
		(u8) mode->hdisplay,

		/* hactive high bits (MAX96757_DSI46) */
		MAX96757_DSI46_HACTIVE_H((u8) (mode->hdisplay >> 8)),
	};
	u8 check2[sizeof(set2)] = {
		/* vfp low byte (MAX96757_DSI37) */
		(u8) ~0,

		/* vbp low nibble and vfp high nibble (MAX96757_DSI38) */
		(u8) ~0,

		/* vbp high byte (MAX96757_DSI39) */
		(u8) ~0,

		/* vactive low byte (MAX96757_DSI40) */
		(u8) ~0,

		/* vactive high nibble (MAX96757_DSI41) */
		MAX96757_DSI41_VACTIVE_H_MASK,

		/* hfp low byte (MAX96757_DSI42) */
		(u8) ~0,

		/* hbp low nibble and hfp high nibble (MAX96757_DSI43) */
		(u8) ~0,

		/* hbp high byte (MAX96757_DSI44) */
		(u8) ~0,

		/* hactive low byte (MAX96757_DSI45) */
		(u8) ~0,

		/* hactive high bits (MAX96757_DSI46) */
		MAX96757_DSI46_HACTIVE_H_MASK,
	};
	int ret;

	ret = csd_write_ser_c(csd, MAX96757_DSI5, set1, sizeof(set1));
	if (ret)
		return ret;

	ret = csd_write_ser_pc(csd, MAX96757_DSI37, set2, check2, sizeof(set2));
	if (ret)
		return ret;

	return 0;
}

/**
 * config_deskew() - Configure deskew block
 * @csd:  pointer to csd_data
 * @mode: display mode
 *
 * Return: 0 on success, negative error code otherwise
 */
static int config_deskew(struct csd_data *csd,
	const struct drm_display_mode *mode)
{
	int ret;

	ret = set_deskew_timing(csd, mode);
	if (ret)
		return ret;

	ret = csd_write_ser_8pc(csd, MAX96757_DSI36,
		MAX96757_DSI36_FIFO_SIZE(3) |
		MAX96757_DSI36_DESKEW_EN,
		MAX96757_DSI36_FIFO_SIZE_MASK |
		MAX96757_DSI36_DESKEW_SEL |
		MAX96757_DSI36_DESKEW_EN);
	if (ret)
		return ret;

	return 0;
}

/**
 * set_input_sync_pol() - Set HS/VS polarity configuration for DSI input
 * @csd:  pointer to csd_data
 * @mode: display mode
 *
 * Return: 0 on success, negative error code otherwise
 */
static int set_input_sync_pol(struct csd_data *csd,
	const struct drm_display_mode *mode)
{
	u8 pol = 0;
	int ret;

	if (mode->flags & DRM_MODE_FLAG_PHSYNC)
		pol |= MAX96757_DSI0_CTRL0_HSYNC_POL_POSITIVE;

	if (mode->flags & DRM_MODE_FLAG_PVSYNC)
		pol |= MAX96757_DSI0_CTRL0_VSYNC_POL_POSITIVE;

	ret = csd_write_ser_8pc(csd, MAX96757_DSI0,
		MAX96757_DSI0_CTRL0_VIDEO_MODE_NBSP |
		pol |
		MAX96757_DSI0_CTRL0_AUTODETECT_LENGTH |
		MAX96757_DSI0_UD2_W1 |
		MAX96757_DSI0_UD0_W1,
		MAX96757_DSI0_CTRL0_VIDEO_MODE_MASK |
		MAX96757_DSI0_CTRL0_HSYNC_POL_POSITIVE |
		MAX96757_DSI0_CTRL0_VSYNC_POL_POSITIVE |
		MAX96757_DSI0_CTRL0_AUTODETECT_LENGTH);
	if (ret)
		return ret;

	return 0;
}

/**
 * csd_max96757_power_on() - Performs MAX96757 power_on config
 * @csd:   pointer to csd_data
 * @gmsl2: true when operating in GMSL2 mode
 *
 * This performs initial serializer configuration. The configuration is
 * intended to match the LVDS-to-HDMI/DP converter boards and to allow basic
 * communication with CSD. Final configuration required for actual display
 * output on CSD is done by csd_configure_max96757_post_wakeup().
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96757_power_on(struct csd_data *csd, bool gmsl2)
{
	u8 nc_pd_pins[] = {3, 9, 10, 11, 12, 15, 16, 17, 18};
	int ret;
	int i;

	/* Enable internal LDO regulator and for GMSL1 enable link reset */
	ret = csd_write_ser_8c(csd, MAX96757_CTRL0,
		(gmsl2 ? 0 : MAX96757_CTRL0_RESET_LINK) |
		MAX96757_CTRL0_AUTO_LINK |
		MAX96757_CTRL0_REG_ENABLE |
		MAX96757_CTRL0_LINK_CFG_A);
	if (ret)
		return ret;

	/* Disable LDO range sensing */
	ret = csd_write_ser_8pc(csd, MAX96757_CTRL2,
		MAX96757_CTRL2_REG_MNL |
		MAX96757_CTRL2_RSVD2_W1,
		MAX96757_CTRL2_REG_MNL);
	if (ret)
		return ret;

	if (!gmsl2) {
		/* Switch to GMSL1 mode (requires link reset) */
		ret = csd_write_ser_8pc(csd, MAX96757_REG6,
			MAX96757_REG6_RSVD3_W1 |
			MAX96757_REG6_RSVD1_W1 |
			MAX96757_REG6_RSVD0_W1,
			MAX96757_REG6_GMSL2 |
			MAX96757_REG6_RCLKEN);
		if (ret)
			return ret;

		/* Set bus width for 30-bit bus */
		ret = csd_write_ser_8pc(csd, MAX96757_GMSL1_7,
			MAX96757_GMSL1_7_BWS,
			(u8) ~(MAX96757_GMSL1_7_RSVD4_W0 |
			MAX96757_GMSL1_7_RSVD1_W0));
		if (ret)
			return ret;

		/* Enable spread spectrum */
		ret = csd_write_ser_8pc(csd, MAX96757_GMSL1_2,
			MAX96757_GMSL1_2_SSEN,
			MAX96757_GMSL1_2_SSEN);
		if (ret)
			return ret;

		/* Switch CNTL pins to GPIO mode */
		ret = csd_write_ser_8c(csd, MAX96757_GMSL1_F,
			MAX96757_GMSL1_F_GPO_RX_EN);
		if (ret)
			return ret;

		/* release link from reset */
		ret = csd_write_ser_8c(csd, MAX96757_CTRL0,
			MAX96757_CTRL0_AUTO_LINK |
			MAX96757_CTRL0_REG_ENABLE |
			MAX96757_CTRL0_LINK_CFG_A);
		if (ret)
			return ret;
	} else {
		/* Map GPIO 10 from deserializer to GPIO 2 on serializer */
		ret = csd_write_ser_8pc(csd, MAX96757_GPIO_C(2),
			MAX96757_GPIO_C_RSVD6_W1 |
			MAX96757_GPIO_C_GPIO_RX_ID(10),
			MAX96757_GPIO_C_OVR_RES_CFG |
			MAX96757_GPIO_C_GPIO_RX_ID_MASK);
		if (ret)
			return ret;

		/* GPIO 2 Enable reception and output */
		ret = csd_write_ser_8pc(csd, MAX96757_GPIO_A(2),
			MAX96757_GPIO_A_RES_CFG |
			MAX96757_GPIO_A_GPIO_OUT |
			MAX96757_GPIO_A_GPIO_RX_EN,
			(u8) ~MAX96757_GPIO_A_GPIO_IN);
		if (ret)
			return ret;
	}

	/* GPIO 0 Disable reception */
	ret = csd_write_ser_8pc(csd, MAX96757_GPIO_A(0),
		MAX96757_GPIO_A_GPIO_OUT,
		(u8) ~MAX96757_GPIO_A_GPIO_IN);
	if (ret)
		return ret;

	/* GPIO 1 Switch to open drain type with pull-up */
	ret = csd_write_ser_8c(csd, MAX96757_GPIO_B(1),
		MAX96757_GPIO_B_PULL_UPDN_SEL_UP |
		MAX96757_GPIO_B_GPIO_TX_ID(1));
	if (ret)
		return ret;

	/* GPIO 1 Set 40k pull and output high */
	ret = csd_write_ser_8pc(csd, MAX96757_GPIO_A(1),
		MAX96757_GPIO_A_GPIO_OUT,
		(u8) ~MAX96757_GPIO_A_GPIO_IN);
	if (ret)
		return ret;

	/* GPIO 7 Disable transmission */
	ret = csd_write_ser_8pc(csd, MAX96757_GPIO_A(7),
		MAX96757_GPIO_A_RES_CFG |
		MAX96757_GPIO_A_GPIO_OUT_DIS,
		(u8) ~MAX96757_GPIO_A_GPIO_IN);
	if (ret)
		return ret;

	/* GPIO 8 Disable reception and output */
	ret = csd_write_ser_8pc(csd, MAX96757_GPIO_A(8),
		MAX96757_GPIO_A_RES_CFG |
		MAX96757_GPIO_A_GPIO_OUT |
		MAX96757_GPIO_A_GPIO_OUT_DIS,
		(u8) ~MAX96757_GPIO_A_GPIO_IN);
	if (ret)
		return ret;

	/* Enable pull-downs on NC pins */
	for (i = 0; i < ARRAY_SIZE(nc_pd_pins); i++) {
		ret = csd_write_ser_8c(csd, MAX96757_GPIO_B(nc_pd_pins[i]),
			MAX96757_GPIO_B_PULL_UPDN_SEL_DOWN |
			MAX96757_GPIO_B_OUT_TYPE |
			MAX96757_GPIO_B_GPIO_TX_ID(nc_pd_pins[i]));
		if (ret)
			return ret;
	}

	/*
	 * Increase output voltage of internal voltage regulator
	 * This is a workaround for a chip errata intended to avoid excessive
	 * jitter on the GMSL link which can lead to loss of GMSL lock.
	 */
	ret = csd_write_ser_8c(csd, MAX96757_PFDDIV,
		0x10);
	if (ret)
		return ret;

	/*
	 * Switch LOCK and ERRB in GPIO mode and enable line fault monitors
	 * Neither of both signals is used during normal operation.
	 * Note: LOCK is anyway available only in GMSL2 mode.
	 * For manufacturing test, we use them in GPIO mode.
	 */
	ret = csd_write_ser_8pc(csd, MAX96757_REG5,
		MAX96757_REG5_PU_LF1 |
		MAX96757_REG5_PU_LF0,
		MAX96757_REG5_LOCK_EN |
		MAX96757_REG5_ERRB_EN |
		MAX96757_REG5_PU_LF3 |
		MAX96757_REG5_PU_LF2 |
		MAX96757_REG5_PU_LF1 |
		MAX96757_REG5_PU_LF0);
	if (ret)
		return ret;

	return 0;
}

/**
 * csd_max96757_pre_enable() - Performs MAX96757 pre_enable config
 * @csd:   pointer to csd_data
 * @gmsl2: true when operating in GMSL2 mode
 * @mode:  display mode
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96757_pre_enable(struct csd_data *csd, bool gmsl2,
	const struct drm_display_mode *mode)
{
	int ret;

	/* Configure HS/VS polarity on DSI input */
	ret = set_input_sync_pol(csd, mode);
	if (ret)
		return ret;

	/* Configure deskew block */
	ret = config_deskew(csd, mode);
	if (ret)
		return ret;

	return 0;
}

/**
 * csd_max96757_post_wakeup() - Performs final MAX96757 configuration
 * @csd:   pointer to csd_data
 * @gmsl2: true when operating in GMSL2 mode
 *
 * This function performs final configuration changes required for display
 * output on CSD. Initial configuration is done by csd_max96757_power_on().
 * Differences between converter board and actual CSD are considered here.
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96757_post_wakeup(struct csd_data *csd, bool gmsl2)
{
	int ret;

	if (!gmsl2) {
		/* Set OLDI mapping */
		ret = csd_write_ser_8pc(csd, MAX96757_GMSL1_15,
			MAX96757_GMSL1_15_SEL_RGB888,
			MAX96757_GMSL1_15_SEL_VESA |
			MAX96757_GMSL1_15_SEL_RGB888);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 * csd_max96757_get_line_fault() - Gets GMSL line status for MAX96757
 * @csd:         pointer to csd_data
 * @line_status: location where to store line status
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96757_get_line_fault(struct csd_data *csd, u8 *line_status)
{
	u8 reg26;
	u8 lf_p;
	u8 lf_n;
	int ret;

	ret = csd_read_ser_8c(csd, MAX96757_REG26, &reg26);

	if (ret)
		return ret;

	if (reg26 & MAX96757_REG26_LF_1_MSB) {
		lf_p = CSD_LINE_STATUS_LINE_TO_LINE;
	} else {
		switch (reg26 & MAX96757_REG26_LF_1_MASK) {

		case MAX96757_REG26_LF_1_BATTERY:
			lf_p = CSD_LINE_STATUS_SHORT_BATTERY;
			break;

		case MAX96757_REG26_LF_1_GND:
			lf_p = CSD_LINE_STATUS_SHORT_GROUND;
			break;

		case MAX96757_REG26_LF_1_NORMAL:
			lf_p = CSD_LINE_STATUS_NORMAL;
			break;

		default: /* MAX96757_REG26_LF_1_OPEN */
			lf_p = CSD_LINE_STATUS_OPEN;
		}
	}

	if (reg26 & MAX96757_REG26_LF_0_MSB) {
		lf_n = CSD_LINE_STATUS_LINE_TO_LINE;
	} else {
		switch (reg26 & MAX96757_REG26_LF_0_MASK) {

		case MAX96757_REG26_LF_0_BATTERY:
			lf_n = CSD_LINE_STATUS_SHORT_BATTERY;
			break;

		case MAX96757_REG26_LF_0_GND:
			lf_n = CSD_LINE_STATUS_SHORT_GROUND;
			break;

		case MAX96757_REG26_LF_0_NORMAL:
			lf_n = CSD_LINE_STATUS_NORMAL;
			break;

		default: /* MAX96757_REG26_LF_0_OPEN */
			lf_n = CSD_LINE_STATUS_OPEN;
		}
	}

	*line_status = (lf_p << 4) | lf_n;

	return 0;
}

/**
 * csd_max96757_soft_reset() - Performs soft reset
 * @csd:   pointer to csd_data
 * @gmsl2: true when operating in GMSL2 mode
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96757_soft_reset(struct csd_data *csd, bool gmsl2)
{
	int ret;

	/* Disable HDCP encryption in serializer */
	ret = csd_write_ser_8pc(csd, MAX96757_HDCP_TX15,
		0,
		MAX96757_HDCP_TX15_EN_INT_COMP |
		MAX96757_HDCP_TX15_HDCP_RESET |
		MAX96757_HDCP_TX15_START_AUTH |
		MAX96757_HDCP_TX15_ENC_EN);

	if (ret)
		return ret;

	/*
	 * Power down HDCP block in serializer
	 * and enable Fast-Pj mode
	 */
	ret = csd_write_ser_8c(csd, MAX96757_HDCP_TX37,
		MAX96757_HDCP_TX37_HDCP_PD |
		MAX96757_HDCP_TX37_FAST_PJ_EN);

	if (ret)
		return ret;

	/* For GMSL1 we will do a link reset in csd_max96757_power_on() */
	if (gmsl2) {
		/* Enable link reset */
		ret = csd_write_ser_8c(csd, MAX96757_CTRL0,
			MAX96757_CTRL0_RESET_LINK |
			MAX96757_CTRL0_AUTO_LINK |
			MAX96757_CTRL0_REG_ENABLE |
			MAX96757_CTRL0_LINK_CFG_A);
		if (ret)
			return ret;

		/* Release link from reset */
		ret = csd_write_ser_8c(csd, MAX96757_CTRL0,
			MAX96757_CTRL0_AUTO_LINK |
			MAX96757_CTRL0_REG_ENABLE |
			MAX96757_CTRL0_LINK_CFG_A);
		if (ret)
			return ret;
	} else {
		/* Set VESA mapping */
		ret = csd_write_ser_8pc(csd, MAX96757_GMSL1_15,
			MAX96757_GMSL1_15_SEL_VESA |
			MAX96757_GMSL1_15_SEL_RGB888,
			MAX96757_GMSL1_15_SEL_VESA |
			MAX96757_GMSL1_15_SEL_RGB888);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 * csd_max96757_check_pclk() - Checks if pixel clock is present
 * @csd:  pointer to csd_data
 * @pclk: location where to store result
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96757_check_pclk(struct csd_data *csd, bool *pclk)
{
	u8 vtx1;
	int ret;

	ret = csd_read_ser_8pc(csd, MAX96757_VTX1_X, &vtx1,
		MAX96757_VTX1_X_PCLKDET_VTX);

	if (ret)
		return ret;

	if (vtx1 & MAX96757_VTX1_X_PCLKDET_VTX)
		*pclk = true;
	else
		*pclk = false;

	return 0;
}

/**
 * csd_max96757_hdcp_get_an() - Read AN
 * @csd:  pointer to csd_data
 * @an:   buffer to store AN
 * @size: size of AN
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96757_hdcp_get_an(struct csd_data *csd, u8 *an, u8 size)
{
	return csd_read_ser_c(csd, MAX96757_HDCP_AN, an, size);
}

/**
 * csd_max96757_hdcp_get_aksv() - Read AKSV
 * @csd:  pointer to csd_data
 * @aksv: buffer to store AKSV
 * @size: size of AKSV
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96757_hdcp_get_aksv(struct csd_data *csd, u8 *aksv, u8 size)
{
	return csd_read_ser_c(csd, MAX96757_HDCP_AKSV, aksv, size);
}

/**
 * csd_max96757_hdcp_set_bksv() - Write BKSV
 * @csd:  pointer to csd_data
 * @bksv: buffer containing BKSV
 * @size: size of BKSV
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96757_hdcp_set_bksv(struct csd_data *csd, u8 *bksv, u8 size)
{
	return csd_write_ser_c(csd, MAX96757_HDCP_BKSV, bksv, size);
}

/**
 * csd_max96757_hdcp_get_ri() - Read Ri
 * @csd:  pointer to csd_data
 * @ri:   buffer to store Ri
 * @size: size of Ri
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96757_hdcp_get_ri(struct csd_data *csd, u8 *ri, u8 size)
{
	return csd_read_ser_pc(csd, MAX96757_HDCP_RI, ri, NULL, size);
}

/**
 * csd_max96757_hdcp_wait_vsync() - Waits until next VSYNC on DSI interface
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96757_hdcp_wait_vsync(struct csd_data *csd)
{
	struct device *dev = csd_get_device(csd);
	unsigned long expire;
	int ret;

	/*
	 * Clear VSYNC detection bit
	 * Don't read back after write. This is time critical.
	 * Potential errors will be caught later.
	 */
	ret = csd_write_ser_8pc(csd, MAX96757_HDCP_TX15,
		0,
		0);

	if (ret)
		return ret;

	expire = jiffies + msecs_to_jiffies(CSD_HDCP_VSYNC_WAIT_TIME) + 1;

	do {
		u8 value;

		ret = csd_read_ser_8pc(csd, MAX96757_HDCP_TX15, &value, 0);
		if (ret)
			continue;

		if (value & MAX96757_HDCP_TX15_VSYNC_DET)
			return 0;

	} while (time_is_after_jiffies(expire));

	dev_err(dev, "Failed to detect VSYNC\n");
	return -EINVAL;
}

/**
 * csd_max96757_hdcp_enable_enc() - Enable HDCP encryption
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96757_hdcp_enable_enc(struct csd_data *csd)
{
	return csd_write_ser_8pc(csd, MAX96757_HDCP_TX15,
		MAX96757_HDCP_TX15_ENC_EN,
		0);
}

/**
 * csd_max96757_hdcp_readback_enc() - Readback register after enable_enc()
 * @csd: pointer to csd_data
 *
 * This just double-checks that the write in enable_enc() was successful,
 * as this can't be done right away. As enabling HDCP needs to happen on
 * ser and des side within one frame, we need to avoid any additional
 * read/write operations at that point.
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96757_hdcp_readback_enc(struct csd_data *csd)
{
	struct device *dev = csd_get_device(csd);
	u8 value;
	int ret;

	ret = csd_read_ser_8pc(csd, MAX96757_HDCP_TX15, &value,
		MAX96757_HDCP_TX15_EN_INT_COMP |
		MAX96757_HDCP_TX15_HDCP_RESET |
		MAX96757_HDCP_TX15_START_AUTH |
		MAX96757_HDCP_TX15_ENC_EN);

	if (ret)
		return ret;

	if ((value & (MAX96757_HDCP_TX15_EN_INT_COMP |
		MAX96757_HDCP_TX15_HDCP_RESET |
		MAX96757_HDCP_TX15_START_AUTH |
		MAX96757_HDCP_TX15_ENC_EN)) !=
		MAX96757_HDCP_TX15_ENC_EN) {

		dev_err(dev, "Readback of serializer HDCP_TX15 register returned wrong value 0x%02hhX\n",
			value);
		return -EIO;
	}

	return 0;
}

/**
 * csd_max96757_hdcp_prepare_enc() - Prepare enabling of encryption
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96757_hdcp_prepare_enc(struct csd_data *csd)
{
	int ret;

	/*
	 * power up HDCP block in serializer
	 * and disable Fast-Pj mode
	 */
	ret = csd_write_ser_8c(csd, MAX96757_HDCP_TX37, 0);
	if (ret)
		return ret;

	/* disable encryption in serializer (in case of reinit) */
	ret = csd_write_ser_8pc(csd, MAX96757_HDCP_TX15,
		0,
		MAX96757_HDCP_TX15_EN_INT_COMP |
		MAX96757_HDCP_TX15_HDCP_RESET |
		MAX96757_HDCP_TX15_START_AUTH |
		MAX96757_HDCP_TX15_ENC_EN);

	if (ret)
		return ret;

	/* reset HDCP block in serializer (in case of reinit) */
	ret = csd_write_ser_8pc(csd, MAX96757_HDCP_TX15,
		MAX96757_HDCP_TX15_HDCP_RESET,
		MAX96757_HDCP_TX15_EN_INT_COMP |
		MAX96757_HDCP_TX15_START_AUTH |
		MAX96757_HDCP_TX15_ENC_EN);

	if (ret)
		return ret;

	/*
	 * The reset bit is supposed to be cleared automatically after 1 us.
	 * However that is not happening. It needs to be cleared manually.
	 */
	ret = csd_write_ser_8pc(csd, MAX96757_HDCP_TX15,
		0,
		MAX96757_HDCP_TX15_EN_INT_COMP |
		MAX96757_HDCP_TX15_HDCP_RESET |
		MAX96757_HDCP_TX15_START_AUTH |
		MAX96757_HDCP_TX15_ENC_EN);

	if (ret)
		return ret;

	/* start authentication */
	ret = csd_write_ser_8pc(csd, MAX96757_HDCP_TX15,
		MAX96757_HDCP_TX15_START_AUTH,
		MAX96757_HDCP_TX15_EN_INT_COMP |
		MAX96757_HDCP_TX15_HDCP_RESET |
		MAX96757_HDCP_TX15_ENC_EN);

	if (ret)
		return ret;

	/*
	 * At this point we should wait until the auth bit is cleared which
	 * is supposed to happen once the session random number is ready.
	 * However that doesn't happen. It needs to be cleared manually.
	 */
	csd_msleep(1);

	ret = csd_write_ser_8pc(csd, MAX96757_HDCP_TX15,
		0,
		MAX96757_HDCP_TX15_EN_INT_COMP |
		MAX96757_HDCP_TX15_HDCP_RESET |
		MAX96757_HDCP_TX15_START_AUTH |
		MAX96757_HDCP_TX15_ENC_EN);

	if (ret)
		return ret;

	return 0;
}

/**
 * csd_max96757_hdcp_check_bksv() - Check validity of BKVS
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96757_hdcp_check_bksv(struct csd_data *csd)
{
	struct device *dev = csd_get_device(csd);
	u8 value;
	int ret;

	/* check if BKSV is considered valid */
	ret = csd_read_ser_8pc(csd, MAX96757_HDCP_TX16, &value,
		MAX96757_HDCP_TX16_INVALID_BK);

	if (ret)
		return ret;

	if (value & MAX96757_HDCP_TX16_INVALID_BK) {
		dev_err(dev, "Invalid BKSV detected\n");
		return -EINVAL;
	}

	return 0;
}
