/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __CSD_DES_MAX96778_I_H__
#define __CSD_DES_MAX96778_I_H__

#define MAX96778_CTRL0                                  0x0010
#define MAX96778_CTRL0_RESET_ALL                        BIT(7)
#define MAX96778_CTRL0_RESET_LINK                       BIT(6)
#define MAX96778_CTRL0_RESET_ONESHOT                    BIT(5)
#define MAX96778_CTRL0_AUTO_LINK                        BIT(4)
#define MAX96778_CTRL0_SLEEP                            BIT(3)
#define MAX96778_CTRL0_UD2_W0                           BIT(2)
#define MAX96778_CTRL0_LINK_CFG_MASK             GENMASK(1, 0)
#define MAX96778_CTRL0_LINK_CFG_DUAL                  (0 << 0)
#define MAX96778_CTRL0_LINK_CFG_A                     (1 << 0)
#define MAX96778_CTRL0_LINK_CFG_B                     (2 << 0)

#define MAX96778_VPRBS                                  0x01DC
#define MAX96778_VPRBS_UD7_W0                           BIT(7)
#define MAX96778_VPRBS_UD6_W0                           BIT(6)
#define MAX96778_VPRBS_VPRBS_FAIL                       BIT(5)
#define MAX96778_VPRBS_VPRBS_CHK_EN                     BIT(4)
#define MAX96778_VPRBS_UD3_W0                           BIT(3)
#define MAX96778_VPRBS_UD2_W0                           BIT(2)
#define MAX96778_VPRBS_UD1_W0                           BIT(1)
#define MAX96778_VPRBS_VIDEO_LOCK                       BIT(0)

#define MAX96778_DP_TRAIN_STATUS                        0x07F0
#define MAX96778_DP_TRAIN_STATUS_ERR                      0x00
#define MAX96778_DP_TRAIN_STATUS_OK                       0x01
#define MAX96778_DP_TRAIN_STATUS_HPD                      0x80

#define MAX96778_DP_TRAIN_ERROR                         0x07F1
#define MAX96778_DP_TRAIN_ERROR_GFLAG                   BIT(7)
#define MAX96778_DP_TRAIN_ERROR_TP1                     BIT(6)
#define MAX96778_DP_TRAIN_ERROR_TP2                     BIT(5)
#define MAX96778_DP_TRAIN_ERROR_PARMS                   BIT(4)
#define MAX96778_DP_TRAIN_ERROR_NOHPD                   BIT(3)
#define MAX96778_DP_TRAIN_ERROR_NOVL                    BIT(2)
#define MAX96778_DP_TRAIN_ERROR_WDG                     BIT(1)
#define MAX96778_DP_TRAIN_ERROR_VLOST                   BIT(0)

#define MAX96778_RLMS58_A                               0x1458

#define MAX96778_RLMS58_B                               0x1558

#define MAX96778_VTRG_CTRL_B0                           0x7000
#define MAX96778_VTRG_CTRL_B0_UD7_W0                    BIT(7)
#define MAX96778_VTRG_CTRL_B0_UD6_W0                    BIT(6)
#define MAX96778_VTRG_CTRL_B0_UD5_W0                    BIT(5)
#define MAX96778_VTRG_CTRL_B0_UD4_W0                    BIT(4)
#define MAX96778_VTRG_CTRL_B0_VID_EN_MODE_MASK   GENMASK(3, 2)
#define MAX96778_VTRG_CTRL_B0_VID_EN_MODE_AUTO        (0 << 2)
#define MAX96778_VTRG_CTRL_B0_VID_EN_MODE_OFF         (2 << 2)
#define MAX96778_VTRG_CTRL_B0_VID_EN_MODE_ON          (3 << 2)
#define MAX96778_VTRG_CTRL_B0_VTRG_RST                  BIT(1)
#define MAX96778_VTRG_CTRL_B0_VTRG_EN                   BIT(0)

#define MAX96778_PIX_RATE_PER_B0                        0x7014

#define MAX96778_DP_COMMAND                             0xE776
#define MAX96778_DP_COMMAND_REBOOT                        0x01
#define MAX96778_DP_COMMAND_TRAIN                         0x02
#define MAX96778_DP_COMMAND_READ                          0x10
#define MAX96778_DP_COMMAND_WRITE                         0x20

#define MAX96778_DP_EXECUTE                             0xE777
#define MAX96778_DP_EXECUTE_RUN                           0x80

#define MAX96778_LINK_RATE                              0xE790
#define MAX96778_LINK_RATE_UD7_W0                       BIT(7)
#define MAX96778_LINK_RATE_UD6_W0                       BIT(6)
#define MAX96778_LINK_RATE_UD5_W0                       BIT(5)
#define MAX96778_LINK_RATE_LINK_RATE_MASK        GENMASK(4, 0)
#define MAX96778_LINK_RATE_LINK_RATE_1_62          (0x06 << 0)
#define MAX96778_LINK_RATE_LINK_RATE_2_16          (0x08 << 0)
#define MAX96778_LINK_RATE_LINK_RATE_2_43          (0x09 << 0)
#define MAX96778_LINK_RATE_LINK_RATE_2_70          (0x0A << 0)
#define MAX96778_LINK_RATE_LINK_RATE_3_24          (0x0C << 0)
#define MAX96778_LINK_RATE_LINK_RATE_4_32          (0x10 << 0)
#define MAX96778_LINK_RATE_LINK_RATE_5_40          (0x1A << 0)
#define MAX96778_LINK_RATE_LINK_RATE_8_10          (0x1E << 0)

#define MAX96778_LANE_COUNT                             0xE792
#define MAX96778_LANE_COUNT_UD7_W0                      BIT(7)
#define MAX96778_LANE_COUNT_UD6_W0                      BIT(6)
#define MAX96778_LANE_COUNT_UD5_W0                      BIT(5)
#define MAX96778_LANE_COUNT_UD4_W0                      BIT(4)
#define MAX96778_LANE_COUNT_UD3_W0                      BIT(3)
#define MAX96778_LANE_COUNT_COUNT_MASK           GENMASK(2, 0)
#define MAX96778_LANE_COUNT_COUNT_ONE                 (1 << 0)
#define MAX96778_LANE_COUNT_COUNT_TWO                 (2 << 0)
#define MAX96778_LANE_COUNT_COUNT_FOUR                (4 << 0)

#define MAX96778_HRES_B0                                0xE794
#define MAX96778_HRES_B1                                0xE795
#define MAX96778_HFP_B0                                 0xE796
#define MAX96778_HFP_B1                                 0xE797
#define MAX96778_HSW_B0                                 0xE798
#define MAX96778_HSW_B1                                 0xE799
#define MAX96778_HBP_B0                                 0xE79A
#define MAX96778_HBP_B1                                 0xE79B
#define MAX96778_VRES_B0                                0xE79C
#define MAX96778_VRES_B1                                0xE79D
#define MAX96778_VFP_B0                                 0xE79E
#define MAX96778_VFP_B1                                 0xE79F
#define MAX96778_VSW_B0                                 0xE7A0
#define MAX96778_VSW_B1                                 0xE7A1
#define MAX96778_VBP_B0                                 0xE7A2
#define MAX96778_VBP_B1                                 0xE7A3
#define MAX96778_HWORDS_B0                              0xE7A4
#define MAX96778_HWORDS_B1                              0xE7A5
#define MAX96778_MVID_B0                                0xE7A6
#define MAX96778_MVID_B1                                0xE7A7
#define MAX96778_NVID_B0                                0xE7A8
#define MAX96778_NVID_B1                                0xE7A9
#define MAX96778_TUC_VALUE_B0                           0xE7AA
#define MAX96778_TUC_VALUE_B1                           0xE7AB

#define MAX96778_HVPOL                                  0xE7AC
#define MAX96778_HVPOL_UD7_W0                           BIT(7)
#define MAX96778_HVPOL_UD6_W0                           BIT(6)
#define MAX96778_HVPOL_UD5_W0                           BIT(5)
#define MAX96778_HVPOL_UD4_W0                           BIT(4)
#define MAX96778_HVPOL_UD3_W0                           BIT(3)
#define MAX96778_HVPOL_UD2_W0                           BIT(2)
#define MAX96778_HVPOL_VSYNC_POL                        BIT(1)
#define MAX96778_HVPOL_HSYNC_POL                        BIT(0)

#define MAX96778_AUTO_VTRG_EN                           0xE7C6
#define MAX96778_AUTO_VTRG_EN_UD7_W0                    BIT(7)
#define MAX96778_AUTO_VTRG_EN_UD6_W0                    BIT(6)
#define MAX96778_AUTO_VTRG_EN_UD5_W0                    BIT(5)
#define MAX96778_AUTO_VTRG_EN_UD4_W0                    BIT(4)
#define MAX96778_AUTO_VTRG_EN_UD3_W0                    BIT(3)
#define MAX96778_AUTO_VTRG_EN_UD2_W0                    BIT(2)
#define MAX96778_AUTO_VTRG_EN_UD1_W0                    BIT(1)
#define MAX96778_AUTO_VTRG_EN_ENABLE                    BIT(0)

#endif /* __CSD_DES_MAX96778_I_H__ */
