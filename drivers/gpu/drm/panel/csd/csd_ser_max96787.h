/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __CSD_SER_MAX96787_H__
#define __CSD_SER_MAX96787_H__

#include "csd_common.h"

int csd_max96787_run_errb_test(struct csd_data *csd,
	enum csd_gpio_test_result *result, struct gpio_desc *errb, bool gmsl2);
int csd_max96787_run_int_test(struct csd_data *csd,
	enum csd_gpio_test_result *result, struct gpio_desc *irq, bool gmsl2);
int csd_max96787_run_mode_test(struct csd_data *csd,
	enum csd_gpio_test_result *result, struct gpio_desc *mode, bool gmsl2);

int csd_max96785_power_on(struct csd_data *csd, bool gmsl2);
int csd_max96787_power_on(struct csd_data *csd, bool gmsl2);
int csd_max96787_enable(struct csd_data *csd, bool gmsl2);
int csd_max96787_post_wakeup(struct csd_data *csd, bool gmsl2);
int csd_max96787_get_line_fault(struct csd_data *csd, u8 *line_status);
int csd_max96787_check_pclk(struct csd_data *csd, bool *pclk);

int csd_max96787_hdcp_prepare_det(struct csd_data *csd);
int csd_max96787_hdcp_get_an(struct csd_data *csd, u8 *an, u8 size);
int csd_max96787_hdcp_get_aksv(struct csd_data *csd, u8 *aksv, u8 size);
int csd_max96787_hdcp_set_bksv(struct csd_data *csd, u8 *bksv, u8 size);
int csd_max96787_hdcp_get_ri(struct csd_data *csd, u8 *ri, u8 size);
int csd_max96787_hdcp_wait_vsync(struct csd_data *csd);
int csd_max96787_hdcp_enable_enc(struct csd_data *csd);
int csd_max96787_hdcp_readback_enc(struct csd_data *csd);
int csd_max96787_hdcp_prepare_enc(struct csd_data *csd);
int csd_max96787_hdcp_check_bksv(struct csd_data *csd);
int csd_max96787_hdcp_handle_req(struct csd_data *csd, u8 *bksv, u8 bksv_size);
int csd_max96787_hdcp_check_req(struct csd_data *csd, bool *request);
int csd_max96787_hdcp_read_stat(struct csd_data *csd, bool *enabled);

#endif /* __CSD_SER_MAX96787_H__ */
