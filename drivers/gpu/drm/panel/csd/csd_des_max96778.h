/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __CSD_DES_MAX96778_H__
#define __CSD_DES_MAX96778_H__

#include "csd_common.h"
#include <drm/drm_modes.h>

int csd_des_max96778_setup(struct csd_data *csd,
	const struct drm_display_mode *mode, u8 chip_rev);
int csd_des_max96778_status(struct csd_data *csd, bool *configured);

#endif /* __CSD_DES_MAX96778_H__ */
