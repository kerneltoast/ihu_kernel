/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __CSD_SER_MAX96787_I_H__
#define __CSD_SER_MAX96787_I_H__

#define MAX96787_REG1                           0x0001
#define MAX96787_REG1_HDMI_AUTOS                BIT(7)
/* HDMI_TERM_TRIM_EN must be set to 1 to meet HDMI compliance */
#define MAX96787_REG1_HDMI_TERM_TRIM_EN         BIT(6)
#define MAX96787_REG1_IIC_2_EN                  BIT(5)
#define MAX96787_REG1_IIC_1_EN                  BIT(4)
#define MAX96787_REG1_TX_RATE_MASK       GENMASK(3, 2)
#define MAX96787_REG1_TX_RATE_3GBPS           (1 << 2)
#define MAX96787_REG1_TX_RATE_6GBPS           (2 << 2)
#define MAX96787_REG1_RX_RATE_MASK       GENMASK(1, 0)
#define MAX96787_REG1_RX_RATE_187MBPS         (0 << 0)
#define MAX96787_REG1_RX_RATE_375MBPS         (1 << 0)
#define MAX96787_REG1_RX_RATE_750MBPS         (2 << 0)
#define MAX96787_REG1_RX_RATE_1_5GBPS         (3 << 0)

#define MAX96787_REG4                           0x0004
#define MAX96787_REG4_LF_1_M                    BIT(7)
#define MAX96787_REG4_LF_0_M                    BIT(6)
#define MAX96787_REG4_PU_LF1                    BIT(5)
#define MAX96787_REG4_PU_LF0                    BIT(4)
#define MAX96787_REG4_LF_1_MASK          GENMASK(3, 2)
#define MAX96787_REG4_LF_1_BATTERY            (0 << 2)
#define MAX96787_REG4_LF_1_GND                (1 << 2)
#define MAX96787_REG4_LF_1_NORMAL             (2 << 2)
#define MAX96787_REG4_LF_1_OPEN               (3 << 2)
#define MAX96787_REG4_LF_0_MASK          GENMASK(1, 0)
#define MAX96787_REG4_LF_0_BATTERY            (0 << 0)
#define MAX96787_REG4_LF_0_GND                (1 << 0)
#define MAX96787_REG4_LF_0_NORMAL             (2 << 0)
#define MAX96787_REG4_LF_0_OPEN               (3 << 0)

#define MAX96787_CTRL0                          0x0010
#define MAX96787_CTRL0_RESET_ALL                BIT(7)
#define MAX96787_CTRL0_RESET_LINK               BIT(6)
#define MAX96787_CTRL0_RESET_ONESHOT            BIT(5)
#define MAX96787_CTRL0_AUTO_LINK                BIT(4)
#define MAX96787_CTRL0_SLEEP                    BIT(3)
#define MAX96787_CTRL0_RSVD2_W0                 BIT(2)
#define MAX96787_CTRL0_LINK_CFG_MASK     GENMASK(1, 0)
#define MAX96787_CTRL0_LINK_CFG_DUAL          (0 << 0)
#define MAX96787_CTRL0_LINK_CFG_LINK_A        (1 << 0)
#define MAX96787_CTRL0_LINK_CFG_LINK_B        (2 << 0)
#define MAX96787_CTRL0_LINK_CFG_SPLITTER      (3 << 0)

#define MAX96787_INTR2                          0x001A
#define MAX96787_INTR2_PHY_INT_OEN_B            BIT(7)
#define MAX96787_INTR2_PHY_INT_OEN_A            BIT(6)
#define MAX96787_INTR2_REM_ERR_OEN              BIT(5)
#define MAX96787_INTR2_HDMI_INT_OEN             BIT(4)
#define MAX96787_INTR2_LFLT_INT_OEN             BIT(3)
#define MAX96787_INTR2_IDLE_ERR_OEN             BIT(2)
#define MAX96787_INTR2_DEC_ERR_OEN_B            BIT(1)
#define MAX96787_INTR2_DEC_ERR_OEN_A            BIT(0)

#define MAX96787_INTR4                          0x001C
#define MAX96787_INTR4_EOM_ERR_OEN_B            BIT(7)
#define MAX96787_INTR4_EOM_ERR_OEN_A            BIT(6)
#define MAX96787_INTR4_HDCP22_INT_OEN           BIT(5)
#define MAX96787_INTR4_HDCP_INT_OEN             BIT(4)
#define MAX96787_INTR4_MAX_RT_OEN               BIT(3)
#define MAX96787_INTR4_RT_CNT_OE                BIT(2)
#define MAX96787_INTR4_PKT_CNT_OEN              BIT(1)
#define MAX96787_INTR4_WM_ERR_OEN               BIT(0)

#define MAX96787_INTR6                          0x001E
#define MAX96787_INTR6_RSVD7_W0                 BIT(7)
#define MAX96787_INTR6_MEM_INT_ERR_OEN          BIT(6)
#define MAX96787_INTR6_LOCK_B_OEN               BIT(5)
#define MAX96787_INTR6_LOCK_A_OEN               BIT(4)
#define MAX96787_INTR6_VDDCMP_INT_OEN           BIT(3)
#define MAX96787_INTR6_PORZ_INT_OEN             BIT(2)
#define MAX96787_INTR6_APRBS_ERR_OEN            BIT(1)
#define MAX96787_INTR6_VDDBAD_INT_OEN           BIT(0)

#define MAX96787_IO_CHK2                        0x003A
#define MAX96787_IO_CHK2_PIN_DRV_SEL            BIT(7)
#define MAX96787_IO_CHK2_RSVD6_W0               BIT(6)
#define MAX96787_IO_CHK2_DDC_SDA                BIT(5)
#define MAX96787_IO_CHK2_DDC_SCL                BIT(4)
#define MAX96787_IO_CHK2_HPD                    BIT(3)
#define MAX96787_IO_CHK2_ERRB                   BIT(2)
#define MAX96787_IO_CHK2_LOCK                   BIT(1)
#define MAX96787_IO_CHK2_GMSL2                  BIT(0)

#define MAX96787_IO_CHK5                        0x003D
#define MAX96787_IO_CHK5_RSVD7_W0               BIT(7)
#define MAX96787_IO_CHK5_RSVD6_W0               BIT(6)
#define MAX96787_IO_CHK5_DDC_SDA                BIT(5)
#define MAX96787_IO_CHK5_DDC_SCL                BIT(4)
#define MAX96787_IO_CHK5_HPD                    BIT(3)
#define MAX96787_IO_CHK5_ERRB                   BIT(2)
#define MAX96787_IO_CHK5_LOCK                   BIT(1)
#define MAX96787_IO_CHK5_GMSL2                  BIT(0)

#define MAX96787_VTX1                           0x01C9
#define MAX96787_VTX1_HDMI_SCDT                 BIT(7)
#define MAX96787_VTX1_HDMI_CKDT                 BIT(6)
#define MAX96787_VTX1_PCLKDET                   BIT(5)
#define MAX96787_VTX1_VS_OUT_EN                 BIT(2)
#define MAX96787_VTX1_HS_OUT_EN                 BIT(1)
#define MAX96787_VTX1_VS_TRIG                   BIT(0)

#define MAX96787_GPIO1_A                        0x0203
#define MAX96787_GPIO7_A                        0x0215

#define MAX96787_GPIOX_A_RES_CFG                BIT(7)
#define MAX96787_GPIOX_A_TX_PRIO                BIT(6)
#define MAX96787_GPIOX_A_TX_COMP_EN             BIT(5)
#define MAX96787_GPIOX_A_GPIO_OUT               BIT(4)
#define MAX96787_GPIOX_A_GPIO_IN                BIT(3)
#define MAX96787_GPIOX_A_RX_EN                  BIT(2)
#define MAX96787_GPIOX_A_TX_EN                  BIT(1)
#define MAX96787_GPIOX_A_GPIO_OUT_DIS           BIT(0)

#define MAX96787_GPIO7_C                        0x0217

#define MAX96787_GPIOX_C_OVR_RES_CFG            BIT(7)
#define MAX96787_GPIOX_C_GPIO_RECVED            BIT(6)
#define MAX96787_GPIOX_C_GPIO_STATE             BIT(5)
#define MAX96787_GPIOX_C_GPIO_RX_ID_MASK GENMASK(4, 0)
#define MAX96787_GPIOX_C_GPIO_RX_ID(id)    ((id) << 0)

#define MAX96787_GMSL1_2                        0x0302
#define MAX96787_GMSL1_2_RSVD7_W0               BIT(7)
#define MAX96787_GMSL1_2_RSVD6_W0               BIT(6)
#define MAX96787_GMSL1_2_SSEN                   BIT(5)
#define MAX96787_GMSL1_2_RSVD4_W0               BIT(4)
#define MAX96787_GMSL1_2_RSVD3_W0               BIT(3)
#define MAX96787_GMSL1_2_RSVD2_W0               BIT(2)
#define MAX96787_GMSL1_2_RSVD1_W0               BIT(1)
#define MAX96787_GMSL1_2_RSVD0_W0               BIT(0)

#define MAX96787_GMSL1_4                        0x0304
#define MAX96787_GMSL1_4_SEREN                  BIT(7)
#define MAX96787_GMSL1_4_CLINKEN                BIT(6)
#define MAX96787_GMSL1_4_PRBSEN                 BIT(5)
#define MAX96787_GMSL1_4_RSVD4_W0               BIT(4)
#define MAX96787_GMSL1_4_RSVD3_W0               BIT(3)
#define MAX96787_GMSL1_4_RSVD2_W0               BIT(2)
#define MAX96787_GMSL1_4_REVCCEN                BIT(1)
#define MAX96787_GMSL1_4_FWDCCEN                BIT(0)

#define MAX96787_GMSL1_7                        0x0307
#define MAX96787_GMSL1_7_DBL                    BIT(7)
#define MAX96787_GMSL1_7_HIBW                   BIT(6)
#define MAX96787_GMSL1_7_BWS                    BIT(5)
#define MAX96787_GMSL1_7_RSVD4_W0               BIT(4)
#define MAX96787_GMSL1_7_DRS                    BIT(3)
#define MAX96787_GMSL1_7_HVEN                   BIT(2)
#define MAX96787_GMSL1_7_RSVD1_W0               BIT(1)
#define MAX96787_GMSL1_7_PXL_CRC                BIT(0)

#define MAX96787_GMSL1_F                        0x030F
#define MAX96787_GMSL1_F_CNTL4_IN_EN            BIT(7)
#define MAX96787_GMSL1_F_CNTL3_IN_EN            BIT(6)
#define MAX96787_GMSL1_F_CNTL2_IN_EN            BIT(5)
#define MAX96787_GMSL1_F_CNTL1_IN_EN            BIT(4)
#define MAX96787_GMSL1_F_CNTL0_IN_EN            BIT(3)
#define MAX96787_GMSL1_F_GPO_RX_EN              BIT(2)
#define MAX96787_GMSL1_F_GPO_OUT_SEL            BIT(1)
#define MAX96787_GMSL1_F_SET_GPO                BIT(0)

#define MAX96787_GMSL1_15                       0x0315
#define MAX96787_GMSL1_15_RSVD7_W0              BIT(7)
#define MAX96787_GMSL1_15_RSVD6_W0              BIT(6)
#define MAX96787_GMSL1_15_RSVD5_W0              BIT(5)
#define MAX96787_GMSL1_15_RSVD4_W0              BIT(4)
#define MAX96787_GMSL1_15_RSVD3_W0              BIT(3)
#define MAX96787_GMSL1_15_RSVD2_W0              BIT(2)
#define MAX96787_GMSL1_15_SEL_VESA              BIT(1)
#define MAX96787_GMSL1_15_SEL_RGB888            BIT(0)

#define MAX96787_HDCP_BKSV                      0x0680

#define MAX96787_HDCP_RI                        0x0685

#define MAX96787_HDCP_AN                        0x0688

#define MAX96787_HDCP_AKSV                      0x0690

#define MAX96787_HDCP_TX15                      0x0695
#define MAX96787_HDCP_TX15_RSVD7_W0             BIT(7)
#define MAX96787_HDCP_TX15_EN_INT_COMP          BIT(6)
#define MAX96787_HDCP_TX15_RSVD5_W0             BIT(5)
#define MAX96787_HDCP_TX15_RSVD4_W0             BIT(4)
#define MAX96787_HDCP_TX15_HDCP_RESET           BIT(3)
#define MAX96787_HDCP_TX15_START_AUTH           BIT(2)
#define MAX96787_HDCP_TX15_VSYNC_DET            BIT(1)
#define MAX96787_HDCP_TX15_ENC_EN               BIT(0)

#define MAX96787_HDCP_TX16                      0x0696
#define MAX96787_HDCP_TX16_PJF_MATCH            BIT(4)
#define MAX96787_HDCP_TX16_V_MATCHED            BIT(3)
#define MAX96787_HDCP_TX16_PJ_MATCHED           BIT(2)
#define MAX96787_HDCP_TX16_RI_MATCHED           BIT(1)
#define MAX96787_HDCP_TX16_INVALID_BK           BIT(0)

#define MAX96787_HDCP_TX37                      0x06B7
#define MAX96787_HDCP_TX37_HDCP_PD              BIT(7)
#define MAX96787_HDCP_TX37_FAST_PJ_EN           BIT(6)
#define MAX96787_HDCP_TX37_FORCE_VIDEO          BIT(5)
#define MAX96787_HDCP_TX37_FORCE_AUDIO          BIT(4)
#define MAX96787_HDCP_TX37_AH_MASK_ENCOFF       BIT(3)
#define MAX96787_HDCP_TX37_AH_MASK_NEWDEV       BIT(2)
#define MAX96787_HDCP_TX37_AH_MASK_KSVRDY       BIT(1)
#define MAX96787_HDCP_TX37_AH_MASK_FAIL         BIT(0)

#define MAX96787_RX_BCAMS_SET                   0x239E
/* Shadowed on the DDC side in BCAPS register */
#define MAX96787_RX_BCAMS_SET_HDMI_CAPABLE      BIT(7)
#define MAX96787_RX_BCAMS_SET_REPEATER          BIT(6)
#define MAX96787_RX_BCAMS_SET_KSV_READY         BIT(5)
#define MAX96787_RX_BCAMS_SET_RSVD4_W0          BIT(4)
#define MAX96787_RX_BCAMS_SET_RSVD3_W0          BIT(3)
#define MAX96787_RX_BCAMS_SET_RSVD2_W0          BIT(2)
#define MAX96787_RX_BCAMS_SET_RSVD1_W0          BIT(1)
#define MAX96787_RX_BCAMS_SET_RSVD0_W0          BIT(0)

#define MAX96787_RX_SHD_BSTATUS1                0x239F

#define MAX96787_RX_SHD_BSTATUS2                0x23A0

#define MAX96787_RX_HDCP_STAT                   0x23A2
#define MAX96787_RX_HDCP_STAT_AUTHENTICATED     BIT(4)
#define MAX96787_RX_HDCP_STAT_DECRYPTING        BIT(5)

#define MAX96787_RX_KSV_SHA_START1              0x23A3

#define MAX96787_RX_KSV_SHA_START2              0x23A4

#define MAX96787_RX_SHA_LENGTH1                 0x23A5

#define MAX96787_RX_SHA_LENGTH2                 0x23A6

#define MAX96787_RX_SHA_CTRL                    0x23A7
#define MAX96787_RX_SHA_CTRL_RSVD7_W0           BIT(7)
#define MAX96787_RX_SHA_CTRL_RSVD6_W0           BIT(6)
#define MAX96787_RX_SHA_CTRL_RSVD5_W0           BIT(5)
#define MAX96787_RX_SHA_CTRL_RSVD4_W0           BIT(4)
#define MAX96787_RX_SHA_CTRL_RSVD3_W0           BIT(3)
#define MAX96787_RX_SHA_CTRL_SHA_MODE           BIT(2)
#define MAX96787_RX_SHA_CTRL_RSVD1_W0           BIT(1)
#define MAX96787_RX_SHA_CTRL_SHA_GO             BIT(0)

#define MAX96787_RX_KSV_FIFO                    0x23A8

#define MAX96787_SYS_CTRL_0                     0x4100
#define MAX96787_SYS_CTRL_0_HDCP_2_2_OFF        BIT(7)
/*
 * Clock gating control for function when configured
 * as a Source Device, otherwise ignored
 */
#define MAX96787_SYS_CTRL_0_CLK_EN              BIT(6)
/*
 * Enables HDCP2.2 Video Encryption when configured
 * as a Source Device, otherwise ignored
 */
#define MAX96787_SYS_CTRL_0_HDCP22_EN           BIT(5)
#define MAX96787_SYS_CTRL_0_RSVD4_W0            BIT(4)
/*
 * Interrupt Enable for ERRB interrupt generation
 * Bit 3: Encryption Disabled
 * Bit 2: Topology Changed
 * Bit 1: Topology Available
 * Bit 0: Authentication Failed
 */
#define MAX96787_SYS_CTRL_0_INT_EN_ENC_DIS      BIT(3)
#define MAX96787_SYS_CTRL_0_INT_EN_TOP_CHANGE   BIT(2)
#define MAX96787_SYS_CTRL_0_INT_EN_TOP_AVAIL    BIT(1)
#define MAX96787_SYS_CTRL_0_INT_EN_AUTH_FAIL    BIT(0)

#endif /* __CSD_SER_MAX96787_I_H__ */
