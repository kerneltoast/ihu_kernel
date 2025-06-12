// SPDX-License-Identifier: GPL-2.0

#include <linux/device.h>
#include <linux/gpio/consumer.h>

#include "csd_ser_max96787.h"
#include "csd_ser_max96787_i.h"

/**
 * csd_max96787_run_errb_test() - Checks ERRB signal path from MAX96787
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
int csd_max96787_run_errb_test(struct csd_data *csd,
	enum csd_gpio_test_result *result, struct gpio_desc *errb, bool gmsl2)
{
	bool passed_low = true;
	bool passed_high = true;
	int ret;

	/* Force ERRB high */
	ret = csd_write_ser_8pc(csd, MAX96787_IO_CHK2,
		MAX96787_IO_CHK2_PIN_DRV_SEL |
		MAX96787_IO_CHK2_ERRB,
		(u8) ~MAX96787_IO_CHK2_RSVD6_W0);
	if (ret)
		return ret;

	if (!gpiod_get_value_cansleep(errb))
		passed_high = false;

	/* Force ERRB low */
	ret = csd_write_ser_8pc(csd, MAX96787_IO_CHK2,
		MAX96787_IO_CHK2_ERRB,
		(u8) ~MAX96787_IO_CHK2_RSVD6_W0);
	if (ret)
		return ret;

	if (gpiod_get_value_cansleep(errb))
		passed_low = false;

	/* Stop forcing ERRB */
	ret = csd_write_ser_8pc(csd, MAX96787_IO_CHK2,
		0,
		(u8) ~MAX96787_IO_CHK2_RSVD6_W0);
	if (ret)
		return ret;

	*result = csd_gpio_test_eval(passed_low, passed_high);

	return 0;
}

/**
 * csd_max96787_run_int_test() - Checks INT signal path from MAX96787
 * @csd:    pointer to csd_data
 * @result: test result
 * @errb:   INT GPIO
 * @gmsl2:  true when operating in GMSL2 mode
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_run_int_test(struct csd_data *csd,
	enum csd_gpio_test_result *result, struct gpio_desc *irq, bool gmsl2)
{
	bool passed_low = true;
	bool passed_high = true;
	int ret;

	/* Set GPIO low */
	if (gmsl2)
		ret = csd_write_ser_8pc(csd, MAX96787_GPIO7_A,
			MAX96787_GPIOX_A_RES_CFG,
			(u8) ~MAX96787_GPIOX_A_GPIO_IN);
	else
		ret = csd_write_ser_8c(csd, MAX96787_GMSL1_F,
			0);

	if (ret)
		return ret;

	if (gpiod_get_value_cansleep(irq))
		passed_low = false;

	/* Set GPIO high */
	if (gmsl2)
		ret = csd_write_ser_8pc(csd, MAX96787_GPIO7_A,
			MAX96787_GPIOX_A_RES_CFG |
			MAX96787_GPIOX_A_GPIO_OUT,
			(u8) ~MAX96787_GPIOX_A_GPIO_IN);
	else
		ret = csd_write_ser_8c(csd, MAX96787_GMSL1_F,
			MAX96787_GMSL1_F_SET_GPO);

	if (ret)
		return ret;

	if (!gpiod_get_value_cansleep(irq))
		passed_high = false;

	/* Re-enable reception */
	if (gmsl2)
		ret = csd_write_ser_8pc(csd, MAX96787_GPIO7_A,
			MAX96787_GPIOX_A_RES_CFG |
			MAX96787_GPIOX_A_RX_EN,
			(u8) ~MAX96787_GPIOX_A_GPIO_IN);
	else
		ret = csd_write_ser_8c(csd, MAX96787_GMSL1_F,
			MAX96787_GMSL1_F_GPO_RX_EN);

	if (ret)
		return ret;

	*result = csd_gpio_test_eval(passed_low, passed_high);

	return 0;
}

/**
 * csd_max96787_run_mode_test() - Checks GMLS mode signal path from MAX96787
 * @csd:    pointer to csd_data
 * @result: test result
 * @mode:   GMSL mode GPIO
 * @gmsl2:  true when operating in GMSL2 mode
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_run_mode_test(struct csd_data *csd,
	 enum csd_gpio_test_result *result, struct gpio_desc *mode, bool gmsl2)
{
	bool passed_low = true;
	bool passed_high = true;
	u8 reg;
	int ret;

	/* Set mode line high */
	gpiod_set_value_cansleep(mode, 1);

	csd_msleep(1);

	ret = csd_read_ser_8pc(csd, MAX96787_IO_CHK5, &reg,
		MAX96787_IO_CHK5_GMSL2);
	if (ret)
		goto mode_test_end;

	if (!(reg & MAX96787_IO_CHK5_GMSL2))
		passed_high = false;

	/* Set mode line low */
	gpiod_set_value_cansleep(mode, 0);

	csd_msleep(1);

	ret = csd_read_ser_8pc(csd, MAX96787_IO_CHK5, &reg,
		MAX96787_IO_CHK5_GMSL2);
	if (ret)
		goto mode_test_end;

	if (reg & MAX96787_IO_CHK5_GMSL2)
		passed_low = false;

mode_test_end:
	/* Restore mode setting */
	gpiod_set_value_cansleep(mode, gmsl2 ? 0 : 1);

	if (ret)
		return ret;

	*result = csd_gpio_test_eval(passed_low, passed_high);

	return 0;
}

/**
 * csd_max9678x_power_on() - Performs MAX9678X power_on config
 * @csd:          pointer to csd_data
 * @gmsl2:        true when operating in GMSL2 mode
 * @hdcp_support: true for MAX96787 and false for MAX96785
 *
 * This performs initial serializer configuration. The configuration is
 * intended to match the LVDS-to-HDMI/DP converter boards and to allow basic
 * communication with CSD. Final configuration required for actual display
 * output on CSD is done by csd_configure_max96787_post_wakeup().
 * Configuration which requires video input to be present is done by
 * csd_configure_max96787_enable().
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_max9678x_power_on(struct csd_data *csd, bool gmsl2,
	bool hdcp_support)
{
	int ret;

	if (gmsl2) {
		/* Enable link reset (required to apply bit rate setting) */
		ret = csd_write_ser_8pc(csd, MAX96787_CTRL0,
			MAX96787_CTRL0_RESET_LINK |
			MAX96787_CTRL0_AUTO_LINK |
			MAX96787_CTRL0_LINK_CFG_LINK_A,
			MAX96787_CTRL0_RESET_ALL |
			MAX96787_CTRL0_RESET_LINK |
			MAX96787_CTRL0_AUTO_LINK |
			MAX96787_CTRL0_SLEEP |
			MAX96787_CTRL0_LINK_CFG_MASK);
		if (ret)
			return ret;

		/* Disable transmission of GPIO1 state (NC) */
		ret = csd_write_ser_8pc(csd, MAX96787_GPIO1_A,
			MAX96787_GPIOX_A_RES_CFG |
			MAX96787_GPIOX_A_GPIO_OUT_DIS,
			(u8) ~MAX96787_GPIOX_A_GPIO_IN);
		if (ret)
			return ret;

		/* Map GPIO 10 from deserializer to GPIO 7 on serializer */
		ret = csd_write_ser_8pc(csd, MAX96787_GPIO7_C,
			MAX96787_GPIOX_C_GPIO_RX_ID(0x0A),
			(u8) ~(MAX96787_GPIOX_C_GPIO_RECVED |
			MAX96787_GPIOX_C_GPIO_STATE));
		if (ret)
			return ret;

		/* Configure GPIO 7 as output */
		ret = csd_write_ser_8pc(csd, MAX96787_GPIO7_A,
			MAX96787_GPIOX_A_RES_CFG |
			MAX96787_GPIOX_A_RX_EN,
			(u8) ~MAX96787_GPIOX_A_GPIO_IN);
		if (ret)
			return ret;

		/* Set transmitter bit rate to 6 Gbps (requires reset) */
		ret = csd_write_ser_8c(csd, MAX96787_REG1,
			MAX96787_REG1_HDMI_TERM_TRIM_EN |
			MAX96787_REG1_TX_RATE_6GBPS);
		if (ret)
			return ret;

		/* Release link reset (using new bit rate setting) */
		ret = csd_write_ser_8pc(csd, MAX96787_CTRL0,
			MAX96787_CTRL0_AUTO_LINK |
			MAX96787_CTRL0_LINK_CFG_LINK_A,
			MAX96787_CTRL0_RESET_ALL |
			MAX96787_CTRL0_RESET_LINK |
			MAX96787_CTRL0_AUTO_LINK |
			MAX96787_CTRL0_SLEEP |
			MAX96787_CTRL0_LINK_CFG_MASK);
		if (ret)
			return ret;

		if (hdcp_support) {
			/*
			 * Power down HDCP2.2 block
			 * Note: This block is accessible only in GMSL2 mode.
			 */
			ret = csd_write_ser_8pc(csd, MAX96787_SYS_CTRL_0,
				MAX96787_SYS_CTRL_0_HDCP_2_2_OFF |
				MAX96787_SYS_CTRL_0_CLK_EN,
				(u8) ~MAX96787_SYS_CTRL_0_RSVD4_W0);
			if (ret)
				return ret;
		}

	} else {
		/* Switch CNTL pins to GPIO mode */
		ret = csd_write_ser_8c(csd, MAX96787_GMSL1_F,
			MAX96787_GMSL1_F_GPO_RX_EN);
		if (ret)
			return ret;

		/* Set bus width for 30-bit bus */
		ret = csd_write_ser_8pc(csd, MAX96787_GMSL1_7,
			MAX96787_GMSL1_7_BWS,
			(u8) ~(MAX96787_GMSL1_7_RSVD4_W0 |
			MAX96787_GMSL1_7_RSVD1_W0));
		if (ret)
			return ret;

		/* Enable spread spectrum */
		ret = csd_write_ser_8pc(csd, MAX96787_GMSL1_2,
			MAX96787_GMSL1_2_SSEN,
			MAX96787_GMSL1_2_SSEN);
		if (ret)
			return ret;
	}

	/* Disable reporting of events at ERRB interrupt pin */
	ret = csd_write_ser_8c(csd, MAX96787_INTR2, 0x00);
	if (ret)
		return ret;

	ret = csd_write_ser_8c(csd, MAX96787_INTR4, 0x00);
	if (ret)
		return ret;

	ret = csd_write_ser_8pc(csd, MAX96787_INTR6,
		0,
		(u8) ~MAX96787_INTR6_RSVD7_W0);
	if (ret)
		return ret;

	if (hdcp_support) {
		/* Enable HDCP repeater mode */
		ret = csd_write_ser_8pc(csd, MAX96787_RX_BCAMS_SET,
			MAX96787_RX_BCAMS_SET_HDMI_CAPABLE |
			MAX96787_RX_BCAMS_SET_REPEATER,
			MAX96787_RX_BCAMS_SET_HDMI_CAPABLE |
			MAX96787_RX_BCAMS_SET_REPEATER);
		if (ret)
			return ret;
	}

	/* Enable line fault monitoring */
	ret = csd_write_ser_8pc(csd, MAX96787_REG4,
		MAX96787_REG4_PU_LF1 | MAX96787_REG4_PU_LF0,
		MAX96787_REG4_PU_LF1 | MAX96787_REG4_PU_LF0);
	if (ret)
		return ret;

	return 0;
}

/**
 * csd_max96785_power_on() - Performs MAX96785 power_on config
 * @csd:   pointer to csd_data
 * @gmsl2: true when operating in GMSL2 mode
 *
 * This performs initial serializer configuration. The configuration is
 * intended to match the LVDS-to-HDMI/DP converter boards and to allow basic
 * communication with CSD. Final configuration required for actual display
 * output on CSD is done by csd_configure_max96787_post_wakeup().
 * Configuration which requires video input to be present is done by
 * csd_configure_max96787_enable().
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96785_power_on(struct csd_data *csd, bool gmsl2)
{
	return csd_max9678x_power_on(csd, gmsl2, false);
}

/**
 * csd_max96787_power_on() - Performs MAX96787 power_on config
 * @csd:   pointer to csd_data
 * @gmsl2: true when operating in GMSL2 mode
 *
 * This performs initial serializer configuration. The configuration is
 * intended to match the LVDS-to-HDMI/DP converter boards and to allow basic
 * communication with CSD. Final configuration required for actual display
 * output on CSD is done by csd_configure_max96787_post_wakeup().
 * Configuration which requires video input to be present is done by
 * csd_configure_max96787_enable().
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_power_on(struct csd_data *csd, bool gmsl2)
{
	return csd_max9678x_power_on(csd, gmsl2, true);
}

/**
 * csd_max96787_enable() - Performs MAX96787 enable config
 * @csd:   pointer to csd_data
 * @gmsl2: true when operating in GMSL2 mode
 *
 * This performs serializer configuration which requires video input to be
 * present. The configuration is intended to match the LVDS-to-HDMI/DP
 * converter boards and to allow basic communication with CSD. Final
 * configuration required for actual display output on CSD is done by
 * csd_configure_max96787_post_wakeup().
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_enable(struct csd_data *csd, bool gmsl2)
{
	int ret;

	if (!gmsl2) {
		/* Disable serialization */
		ret = csd_write_ser_8pc(csd, MAX96787_GMSL1_4,
			MAX96787_GMSL1_4_REVCCEN |
			MAX96787_GMSL1_4_FWDCCEN,
			MAX96787_GMSL1_4_SEREN |
			MAX96787_GMSL1_4_CLINKEN |
			MAX96787_GMSL1_4_PRBSEN |
			MAX96787_GMSL1_4_REVCCEN |
			MAX96787_GMSL1_4_FWDCCEN);
		if (ret)
			return ret;
	}

	/* Enable HDMI input */
	ret = csd_write_ser_8c(csd, MAX96787_REG1,
		MAX96787_REG1_HDMI_TERM_TRIM_EN |
		MAX96787_REG1_HDMI_AUTOS |
		(gmsl2 ?
		MAX96787_REG1_TX_RATE_6GBPS :
		MAX96787_REG1_TX_RATE_3GBPS));
	if (ret)
		return ret;

	if (!gmsl2) {
		/*
		 * Maxim recommends to wait at least the time equivalent to
		 * 6 video frames after the HDMI input was enabled before
		 * enabling serialization to make sure the clock signal
		 * is stable inside serializer. Otherwise the GMSL1 link may
		 * fail to lock due to wrong PLL calibration.
		 */
		csd_msleep(100);

		/* Enable serialization */
		ret = csd_write_ser_8pc(csd, MAX96787_GMSL1_4,
			MAX96787_GMSL1_4_SEREN |
			MAX96787_GMSL1_4_REVCCEN |
			MAX96787_GMSL1_4_FWDCCEN,
			MAX96787_GMSL1_4_SEREN |
			MAX96787_GMSL1_4_CLINKEN |
			MAX96787_GMSL1_4_PRBSEN |
			MAX96787_GMSL1_4_REVCCEN |
			MAX96787_GMSL1_4_FWDCCEN);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 * csd_max96787_post_wakeup() - Performs final MAX96787 configuration
 * @csd:   pointer to csd_data
 * @gmsl2: true when operating in GMSL2 mode
 *
 * This function performs final configuration changes required for display
 * output on CSD. Initial configuration is done by csd_max96785_power_on()
 * or csd_max96787_power_on() and csd_max96787_enable().
 * Differences between converter board and actual CSD are considered here.
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_post_wakeup(struct csd_data *csd, bool gmsl2)
{
	int ret;

	if (!gmsl2) {
		/* Set OLDI mapping */
		ret = csd_write_ser_8pc(csd, MAX96787_GMSL1_15,
			MAX96787_GMSL1_15_SEL_RGB888,
			MAX96787_GMSL1_15_SEL_VESA |
			MAX96787_GMSL1_15_SEL_RGB888);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 * csd_max96787_get_line_fault() - Gets GMSL line status for MAX96787
 * @csd:         pointer to csd_data
 * @line_status: location where to store line status
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_get_line_fault(struct csd_data *csd, u8 *line_status)
{
	u8 reg4;
	u8 lf_p;
	u8 lf_n;
	int ret;

	ret = csd_read_ser_8c(csd, MAX96787_REG4, &reg4);

	if (ret)
		return ret;

	if (reg4 & MAX96787_REG4_LF_1_M) {
		lf_p = CSD_LINE_STATUS_LINE_TO_LINE;
	} else {
		switch (reg4 & MAX96787_REG4_LF_1_MASK) {

		case MAX96787_REG4_LF_1_BATTERY:
			lf_p = CSD_LINE_STATUS_SHORT_BATTERY;
			break;

		case MAX96787_REG4_LF_1_GND:
			lf_p = CSD_LINE_STATUS_SHORT_GROUND;
			break;

		case MAX96787_REG4_LF_1_NORMAL:
			lf_p = CSD_LINE_STATUS_NORMAL;
			break;

		default: /* MAX96787_REG4_LF_1_OPEN */
			lf_p = CSD_LINE_STATUS_OPEN;
		}
	}

	if (reg4 & MAX96787_REG4_LF_0_M) {
		lf_n = CSD_LINE_STATUS_LINE_TO_LINE;
	} else {
		switch (reg4 & MAX96787_REG4_LF_0_MASK) {

		case MAX96787_REG4_LF_0_BATTERY:
			lf_n = CSD_LINE_STATUS_SHORT_BATTERY;
			break;

		case MAX96787_REG4_LF_0_GND:
			lf_n = CSD_LINE_STATUS_SHORT_GROUND;
			break;

		case MAX96787_REG4_LF_0_NORMAL:
			lf_n = CSD_LINE_STATUS_NORMAL;
			break;

		default: /* MAX96787_REG4_LF_0_OPEN */
			lf_n = CSD_LINE_STATUS_OPEN;
		}
	}

	*line_status = (lf_p << 4) | lf_n;

	return 0;
}

/**
 * csd_max96787_check_pclk() - Checks if pixel clock is present
 * @csd:  pointer to csd_data
 * @pclk: location where to store result
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_check_pclk(struct csd_data *csd, bool *pclk)
{
	u8 vtx1;
	int ret;

	ret = csd_read_ser_8pc(csd, MAX96787_VTX1, &vtx1,
		MAX96787_VTX1_PCLKDET);

	if (ret)
		return ret;

	if (vtx1 & MAX96787_VTX1_PCLKDET)
		*pclk = true;
	else
		*pclk = false;

	return 0;
}

/**
 * csd_max96787_hdcp_prepare_det() - Prepare for detection of requests on DDC
 * @csd: pointer to csd_data
 *
 * When HDMI source tries to enable HDCP protection the ready bit will
 * be cleared.
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_hdcp_prepare_det(struct csd_data *csd)
{
	int ret;

	/*
	 * Invalidate KSV list size.
	 * At this point we are setting the READY bit just in order to be
	 * able to detect future HDCP requests on DDC interface. The content
	 * of KSV list (if any) needs to be considered invalid at this point.
	 */
	ret = csd_write_ser_8c(csd, MAX96787_RX_SHA_LENGTH1, 0);
	if (ret)
		return ret;

	ret = csd_write_ser_8c(csd, MAX96787_RX_SHA_LENGTH2, 0);
	if (ret)
		return ret;

	/*
	 * Set ready bit
	 * Don't check ready bit during readback. It might have been cleared
	 * already. In any case we will catch the fact that it is clear
	 * during our next periodic read.
	 */
	ret = csd_write_ser_8pc(csd, MAX96787_RX_BCAMS_SET,
		MAX96787_RX_BCAMS_SET_HDMI_CAPABLE |
		MAX96787_RX_BCAMS_SET_REPEATER |
		MAX96787_RX_BCAMS_SET_KSV_READY,
		MAX96787_RX_BCAMS_SET_HDMI_CAPABLE |
		MAX96787_RX_BCAMS_SET_REPEATER);
	if (ret)
		return ret;

	return 0;
}

/**
 * csd_max96787_hdcp_get_an() - Read AN
 * @csd:  pointer to csd_data
 * @an:   buffer to store AN
 * @size: size of AN
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_hdcp_get_an(struct csd_data *csd, u8 *an, u8 size)
{
	return csd_read_ser_c(csd, MAX96787_HDCP_AN, an, size);
}

/**
 * csd_max96787_hdcp_get_aksv() - Read AKSV
 * @csd:  pointer to csd_data
 * @aksv: buffer to store AKSV
 * @size: size of AKSV
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_hdcp_get_aksv(struct csd_data *csd, u8 *aksv, u8 size)
{
	return csd_read_ser_c(csd, MAX96787_HDCP_AKSV, aksv, size);
}

/**
 * csd_max96787_hdcp_set_bksv() - Write BKSV
 * @csd:  pointer to csd_data
 * @bksv: buffer containing BKSV
 * @size: size of BKSV
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_hdcp_set_bksv(struct csd_data *csd, u8 *bksv, u8 size)
{
	return csd_write_ser_c(csd, MAX96787_HDCP_BKSV, bksv, size);
}

/**
 * csd_max96787_hdcp_get_ri() - Read Ri
 * @csd:  pointer to csd_data
 * @ri:   buffer to store Ri
 * @size: size of Ri
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_hdcp_get_ri(struct csd_data *csd, u8 *ri, u8 size)
{
	return csd_read_ser_pc(csd, MAX96787_HDCP_RI, ri, NULL, size);
}

/**
 * csd_max96787_hdcp_wait_vsync() - Waits until next VSYNC on HDMI interface
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_hdcp_wait_vsync(struct csd_data *csd)
{
	struct device *dev = csd_get_device(csd);
	unsigned long expire;
	int ret;

	/*
	 * Clear VSYNC detection bit
	 * Don't read back after write. This is time critical.
	 * Potential errors will be caught later.
	 */
	ret = csd_write_ser_8pc(csd, MAX96787_HDCP_TX15,
		0,
		0);

	if (ret)
		return ret;

	expire = jiffies + msecs_to_jiffies(CSD_HDCP_VSYNC_WAIT_TIME) + 1;

	do {
		u8 value;

		ret = csd_read_ser_8pc(csd, MAX96787_HDCP_TX15, &value, 0);
		if (ret)
			continue;

		if (value & MAX96787_HDCP_TX15_VSYNC_DET)
			return 0;

	} while (time_is_after_jiffies(expire));

	dev_err(dev, "Failed to detect VSYNC\n");
	return -EINVAL;
}

/**
 * csd_max96787_hdcp_enable_enc() - Enable HDCP encryption
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_hdcp_enable_enc(struct csd_data *csd)
{
	return csd_write_ser_8pc(csd, MAX96787_HDCP_TX15,
		MAX96787_HDCP_TX15_ENC_EN,
		0);
}

/**
 * csd_max96787_hdcp_readback_enc() - Readback register after enable_enc()
 * @csd: pointer to csd_data
 *
 * This just double-checks that the write in enable_enc() was successful,
 * as this can't be done right away. As enabling HDCP needs to happen on
 * ser and des side within one frame, we need to avoid any additional
 * read/write operations at that point.
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_hdcp_readback_enc(struct csd_data *csd)
{
	struct device *dev = csd_get_device(csd);
	u8 value;
	int ret;

	ret = csd_read_ser_8pc(csd, MAX96787_HDCP_TX15, &value,
		MAX96787_HDCP_TX15_EN_INT_COMP |
		MAX96787_HDCP_TX15_HDCP_RESET |
		MAX96787_HDCP_TX15_START_AUTH |
		MAX96787_HDCP_TX15_ENC_EN);

	if (ret)
		return ret;

	if ((value & (MAX96787_HDCP_TX15_EN_INT_COMP |
		MAX96787_HDCP_TX15_HDCP_RESET |
		MAX96787_HDCP_TX15_START_AUTH |
		MAX96787_HDCP_TX15_ENC_EN)) !=
		MAX96787_HDCP_TX15_ENC_EN) {

		dev_err(dev, "Readback of serializer HDCP_TX15 register returned wrong value 0x%02hhX\n",
			value);
		return -EIO;
	}

	return 0;
}

/**
 * csd_max96787_hdcp_prepare_enc() - Prepare enabling of encryption
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_hdcp_prepare_enc(struct csd_data *csd)
{
	int ret;

	/*
	 * power up HDCP block in serializer
	 * and disable Fast-Pj mode
	 */
	ret = csd_write_ser_8c(csd, MAX96787_HDCP_TX37, 0);
	if (ret)
		return ret;

	/* disable encryption in serializer (in case of reinit) */
	ret = csd_write_ser_8pc(csd, MAX96787_HDCP_TX15,
		0,
		MAX96787_HDCP_TX15_EN_INT_COMP |
		MAX96787_HDCP_TX15_HDCP_RESET |
		MAX96787_HDCP_TX15_START_AUTH |
		MAX96787_HDCP_TX15_ENC_EN);

	if (ret)
		return ret;

	/* reset HDCP block in serializer (in case of reinit) */
	ret = csd_write_ser_8pc(csd, MAX96787_HDCP_TX15,
		MAX96787_HDCP_TX15_HDCP_RESET,
		MAX96787_HDCP_TX15_EN_INT_COMP |
		MAX96787_HDCP_TX15_START_AUTH |
		MAX96787_HDCP_TX15_ENC_EN);

	if (ret)
		return ret;

	/*
	 * The reset bit is supposed to be clear automatically after 1 us.
	 * However that is not happening. It needs to be cleared manually.
	 */
	ret = csd_write_ser_8pc(csd, MAX96787_HDCP_TX15,
		0,
		MAX96787_HDCP_TX15_EN_INT_COMP |
		MAX96787_HDCP_TX15_HDCP_RESET |
		MAX96787_HDCP_TX15_START_AUTH |
		MAX96787_HDCP_TX15_ENC_EN);

	if (ret)
		return ret;

	/* start authentication */
	ret = csd_write_ser_8pc(csd, MAX96787_HDCP_TX15,
		MAX96787_HDCP_TX15_START_AUTH,
		MAX96787_HDCP_TX15_EN_INT_COMP |
		MAX96787_HDCP_TX15_HDCP_RESET |
		MAX96787_HDCP_TX15_ENC_EN);

	if (ret)
		return ret;

	/*
	 * At this point we should wait until the auth bit is cleared which
	 * is supposed to happen once the session random number is ready.
	 * However that doesn't happen. It needs to be cleared manually.
	 */
	csd_msleep(1);

	ret = csd_write_ser_8pc(csd, MAX96787_HDCP_TX15,
		0,
		MAX96787_HDCP_TX15_EN_INT_COMP |
		MAX96787_HDCP_TX15_HDCP_RESET |
		MAX96787_HDCP_TX15_START_AUTH |
		MAX96787_HDCP_TX15_ENC_EN);

	if (ret)
		return ret;

	return 0;
}

/**
 * csd_max96787_hdcp_check_bksv() - Check validity of BKVS
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_hdcp_check_bksv(struct csd_data *csd)
{
	struct device *dev = csd_get_device(csd);
	u8 value;
	int ret;

	/* check if BKSV is considered valid */
	ret = csd_read_ser_8pc(csd, MAX96787_HDCP_TX16, &value,
		MAX96787_HDCP_TX16_INVALID_BK);

	if (ret)
		return ret;

	if (value & MAX96787_HDCP_TX16_INVALID_BK) {
		dev_err(dev, "Invalid BKSV detected\n");
		return -EINVAL;
	}

	return 0;
}

/**
 * csd_max96787_hdcp_handle_req() - Handles HDCP request on HDMI link
 * @csd:       pointer to csd_data
 * @bksv:      buffer containing BKSV
 * @bksv_size: size of BKSV
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_hdcp_handle_req(struct csd_data *csd, u8 *bksv, u8 bksv_size)
{
	int ret;
	u8 i;

	/* Configure count */
	ret = csd_write_ser_8c(csd, MAX96787_RX_SHD_BSTATUS1, 1);
	if (ret)
		return ret;

	/* Configure depth */
	ret = csd_write_ser_8c(csd, MAX96787_RX_SHD_BSTATUS2, 1);
	if (ret)
		return ret;

	/* Set KSV pointer to zero */
	ret = csd_write_ser_8pc(csd, MAX96787_RX_KSV_SHA_START1, 0, 0);
	if (ret)
		return ret;

	ret = csd_write_ser_8pc(csd, MAX96787_RX_KSV_SHA_START2, 0, 0);
	if (ret)
		return ret;

	/*
	 * Write BKSV value into KSV FIFO
	 * From User Guide: "KSVs should be written 1 byte at a time
	 *                   with single byte writes (No multibyte)"
	 * Please note that the register address is not incremented
	 * for each byte and that a readback is not possible.
	 */
	for (i = 0; i < bksv_size; i++) {
		ret = csd_write_ser_8pc(csd, MAX96787_RX_KSV_FIFO, bksv[i], 0);
		if (ret)
			return ret;
	}

	/* Set KSV pointer to zero */
	ret = csd_write_ser_8pc(csd, MAX96787_RX_KSV_SHA_START1, 0, 0);
	if (ret)
		return ret;

	ret = csd_write_ser_8pc(csd, MAX96787_RX_KSV_SHA_START2, 0, 0);
	if (ret)
		return ret;

	/* Set KSV list size */
	ret = csd_write_ser_8c(csd, MAX96787_RX_SHA_LENGTH1, bksv_size);
	if (ret)
		return ret;

	ret = csd_write_ser_8c(csd, MAX96787_RX_SHA_LENGTH2, 0);
	if (ret)
		return ret;

	/* Start SHA calculation */
	ret = csd_write_ser_8pc(csd, MAX96787_RX_SHA_CTRL,
		MAX96787_RX_SHA_CTRL_SHA_GO,
		0);
	if (ret)
		return ret;

	/* Wait for SHA */
	csd_msleep(10);

	/*
	 * Set ready bit
	 * We can't read back the ready bit. It might be cleared right away.
	 * That is not an issue as it will be checked periodically later.
	 */
	ret = csd_write_ser_8pc(csd, MAX96787_RX_BCAMS_SET,
		MAX96787_RX_BCAMS_SET_HDMI_CAPABLE |
		MAX96787_RX_BCAMS_SET_REPEATER |
		MAX96787_RX_BCAMS_SET_KSV_READY,
		MAX96787_RX_BCAMS_SET_HDMI_CAPABLE |
		MAX96787_RX_BCAMS_SET_REPEATER);
	if (ret)
		return ret;

	return 0;
}

/**
 * csd_max96787_hdcp_check_req() - Checks for HDCP request on HDMI link
 * @csd:     pointer to csd_data
 * @request: indicates if HDCP request from SOC is pending
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_hdcp_check_req(struct csd_data *csd, bool *request)
{
	u8 reg_rx_bcams;
	int ret;

	ret = csd_read_ser_8pc(csd, MAX96787_RX_BCAMS_SET, &reg_rx_bcams,
		MAX96787_RX_BCAMS_SET_KSV_READY);

	if (ret)
		return ret;

	*request = !(reg_rx_bcams & MAX96787_RX_BCAMS_SET_KSV_READY);

	return 0;
}

/**
 * csd_max96787_hdcp_read_stat() - Gets HDCP status
 * @csd:     pointer to csd_data
 * @enabled: set to indicate whether HDCP is enabled
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_max96787_hdcp_read_stat(struct csd_data *csd, bool *enabled)
{
	u8 reg_rx_hdcp_stat;
	int ret;

	ret = csd_read_ser_8pc(csd, MAX96787_RX_HDCP_STAT, &reg_rx_hdcp_stat,
		MAX96787_RX_HDCP_STAT_DECRYPTING);

	if (ret)
		return ret;

	*enabled = !!(reg_rx_hdcp_stat & MAX96787_RX_HDCP_STAT_DECRYPTING);

	return 0;
}
