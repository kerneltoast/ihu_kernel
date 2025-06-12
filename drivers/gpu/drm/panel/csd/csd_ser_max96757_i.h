/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __CSD_SER_MAX96757_I_H__
#define __CSD_SER_MAX96757_I_H__

#define MAX96757_REG5                                   0x0005
#define MAX96757_REG5_LOCK_EN                           BIT(7)
#define MAX96757_REG5_ERRB_EN                           BIT(6)
#define MAX96757_REG5_RSVD5_W0                          BIT(5)
#define MAX96757_REG5_RSVD4_W0                          BIT(4)
#define MAX96757_REG5_PU_LF3                            BIT(3)
#define MAX96757_REG5_PU_LF2                            BIT(2)
#define MAX96757_REG5_PU_LF1                            BIT(1)
#define MAX96757_REG5_PU_LF0                            BIT(0)

#define MAX96757_REG6                                   0x0006
#define MAX96757_REG6_GMSL2                             BIT(7)
#define MAX96757_REG6_RSVD6_W0                          BIT(6)
#define MAX96757_REG6_RCLKEN                            BIT(5)
#define MAX96757_REG6_RSVD4_W0                          BIT(4)
#define MAX96757_REG6_RSVD3_W1                          BIT(3)
#define MAX96757_REG6_RSVD2_W0                          BIT(2)
#define MAX96757_REG6_RSVD1_W1                          BIT(1)
#define MAX96757_REG6_RSVD0_W1                          BIT(0)

#define MAX96757_CTRL0                                  0x0010
#define MAX96757_CTRL0_RESET_ALL                        BIT(7)
#define MAX96757_CTRL0_RESET_LINK                       BIT(6)
#define MAX96757_CTRL0_RESET_ONE_SHOT                   BIT(5)
#define MAX96757_CTRL0_AUTO_LINK                        BIT(4)
#define MAX96757_CTRL0_SLEEP                            BIT(3)
#define MAX96757_CTRL0_REG_ENABLE                       BIT(2)
#define MAX96757_CTRL0_LINK_CFG                  GENMASK(1, 0)
#define MAX96757_CTRL0_LINK_CFG_DUAL                  (0 << 0)
#define MAX96757_CTRL0_LINK_CFG_A                     (1 << 0)
#define MAX96757_CTRL0_LINK_CFG_B                     (2 << 0)
#define MAX96757_CTRL0_LINK_CFG_SPLITTER              (3 << 0)

#define MAX96757_CTRL2                                  0x0012
#define MAX96757_CTRL2_RSVD7_W0                         BIT(7)
#define MAX96757_CTRL2_RSVD6_W0                         BIT(6)
#define MAX96757_CTRL2_RSVD5_W0                         BIT(5)
#define MAX96757_CTRL2_REG_MNL                          BIT(4)
#define MAX96757_CTRL2_RSVD3_W0                         BIT(3)
#define MAX96757_CTRL2_RSVD2_W1                         BIT(2)
#define MAX96757_CTRL2_RSVD1_W0                         BIT(1)
#define MAX96757_CTRL2_RSVD0_W0                         BIT(0)

#define MAX96757_REG26                                  0x0026
#define MAX96757_REG26_RSVD7_W0                         BIT(7)
#define MAX96757_REG26_LF_1_MSB                         BIT(6)
#define MAX96757_REG26_LF_1_MASK                 GENMASK(5, 4)
#define MAX96757_REG26_LF_1_BATTERY                   (0 << 4)
#define MAX96757_REG26_LF_1_GND                       (1 << 4)
#define MAX96757_REG26_LF_1_NORMAL                    (2 << 4)
#define MAX96757_REG26_LF_1_OPEN                      (3 << 4)
#define MAX96757_REG26_RSVD3_W0                         BIT(3)
#define MAX96757_REG26_LF_0_MSB                         BIT(2)
#define MAX96757_REG26_LF_0_MASK                 GENMASK(1, 0)
#define MAX96757_REG26_LF_0_BATTERY                   (0 << 0)
#define MAX96757_REG26_LF_0_GND                       (1 << 0)
#define MAX96757_REG26_LF_0_NORMAL                    (2 << 0)
#define MAX96757_REG26_LF_0_OPEN                      (3 << 0)

#define MAX96757_VTX1_X                                 0x01C9
#define MAX96757_VTX1_X_UD7_W0                          BIT(7)
#define MAX96757_VTX1_X_UD6_W0                          BIT(6)
#define MAX96757_VTX1_X_PCLKDET_VTX                     BIT(5)
#define MAX96757_VTX1_X_UD4_W0                          BIT(4)
#define MAX96757_VTX1_X_UD3_W0                          BIT(3)
#define MAX96757_VTX1_X_UD2_W0                          BIT(2)
#define MAX96757_VTX1_X_UD1_W0                          BIT(1)
#define MAX96757_VTX1_X_VS_TRIG                         BIT(0)

#define MAX96757_GPIO_A(gpio)            (0x02BE + 3 * (gpio))
#define MAX96757_GPIO_A_RES_CFG                         BIT(7)
#define MAX96757_GPIO_A_TX_PRIO                         BIT(6)
#define MAX96757_GPIO_A_TX_COMP_EN                      BIT(5)
#define MAX96757_GPIO_A_GPIO_OUT                        BIT(4)
#define MAX96757_GPIO_A_GPIO_IN                         BIT(3)
#define MAX96757_GPIO_A_GPIO_RX_EN                      BIT(2)
#define MAX96757_GPIO_A_GPIO_TX_EN                      BIT(1)
#define MAX96757_GPIO_A_GPIO_OUT_DIS                    BIT(0)

#define MAX96757_GPIO_B(gpio)            (0x02BF + 3 * (gpio))
#define MAX96757_GPIO_B_PULL_UPDN_SEL_MASK       GENMASK(7, 6)
#define MAX96757_GPIO_B_PULL_UPDN_SEL_NONE            (0 << 6)
#define MAX96757_GPIO_B_PULL_UPDN_SEL_UP              (1 << 6)
#define MAX96757_GPIO_B_PULL_UPDN_SEL_DOWN            (2 << 6)
#define MAX96757_GPIO_B_OUT_TYPE                        BIT(5)
#define MAX96757_GPIO_B_GPIO_TX_ID_MASK          GENMASK(4, 0)
#define MAX96757_GPIO_B_GPIO_TX_ID(id)             ((id) << 0)

#define MAX96757_GPIO_C(gpio)            (0x02C0 + 3 * (gpio))
#define MAX96757_GPIO_C_OVR_RES_CFG                     BIT(7)
#define MAX96757_GPIO_C_RSVD6_W1                        BIT(6)
#define MAX96757_GPIO_C_RSVD5_W0                        BIT(5)
#define MAX96757_GPIO_C_GPIO_RX_ID_MASK          GENMASK(4, 0)
#define MAX96757_GPIO_C_GPIO_RX_ID(id)             ((id) << 0)

#define MAX96757_PFDDIV                                 0x0302

#define MAX96757_DSI0                                   0x0380
#define MAX96757_DSI0_CTRL0_VIDEO_MODE_MASK      GENMASK(7, 6)
#define MAX96757_DSI0_CTRL0_VIDEO_MODE_NBSP           (0 << 6)
#define MAX96757_DSI0_CTRL0_VIDEO_MODE_NBSE           (1 << 6)
#define MAX96757_DSI0_CTRL0_HSYNC_POL_POSITIVE          BIT(5)
#define MAX96757_DSI0_CTRL0_VSYNC_POL_POSITIVE          BIT(4)
#define MAX96757_DSI0_CTRL0_AUTODETECT_LENGTH           BIT(3)
#define MAX96757_DSI0_UD2_W1                            BIT(2)
#define MAX96757_DSI0_UD1_W0                            BIT(1)
#define MAX96757_DSI0_UD0_W1                            BIT(0)

#define MAX96757_DSI5                                   0x0385

#define MAX96757_DSI6                                   0x0386

#define MAX96757_DSI7                                   0x0387
#define MAX96757_DSI7_VSYNC_H_MASK               GENMASK(7, 4)
#define MAX96757_DSI7_VSYNC_H(x)                    ((x) << 4)
#define MAX96757_DSI7_HSYNC_H_MASK               GENMASK(3, 0)
#define MAX96757_DSI7_HSYNC_H(x)                    ((x) << 0)

#define MAX96757_DSI36                                  0x03A4
#define MAX96757_DSI36_FIFO_SIZE_MASK            GENMASK(7, 6)
#define MAX96757_DSI36_FIFO_SIZE(x)                 ((x) << 6)
#define MAX96757_DSI36_RSVD5_W0                         BIT(5)
#define MAX96757_DSI36_RSVD4_W0                         BIT(4)
#define MAX96757_DSI36_RSVD3_W0                         BIT(3)
#define MAX96757_DSI36_RSVD2_W0                         BIT(2)
#define MAX96757_DSI36_DESKEW_SEL                       BIT(1)
#define MAX96757_DSI36_DESKEW_EN                        BIT(0)

#define MAX96757_DSI37                                  0x03A5

#define MAX96757_DSI38                                  0x03A6
#define MAX96757_DSI38_VBP_L_MASK                GENMASK(7, 4)
#define MAX96757_DSI38_VBP_L(x)                     ((x) << 4)
#define MAX96757_DSI38_VFP_H_MASK                GENMASK(3, 0)
#define MAX96757_DSI38_VFP_H(x)                     ((x) << 0)

#define MAX96757_DSI39                                  0x03A7

#define MAX96757_DSI40                                  0x03A8

#define MAX96757_DSI41                                  0x03A9
#define MAX96757_DSI41_RSVD7_W0                         BIT(7)
#define MAX96757_DSI41_RSVD6_W0                         BIT(6)
#define MAX96757_DSI41_RSVD5_W0                         BIT(5)
#define MAX96757_DSI41_RSVD4_W0                         BIT(4)
#define MAX96757_DSI41_VACTIVE_H_MASK            GENMASK(3, 0)
#define MAX96757_DSI41_VACTIVE_H(x)                 ((x) << 0)

#define MAX96757_DSI42                                  0x03AA

#define MAX96757_DSI43                                  0x03AB
#define MAX96757_DSI43_HBP_L_MASK                GENMASK(7, 4)
#define MAX96757_DSI43_HBP_L(x)                     ((x) << 4)
#define MAX96757_DSI43_HFP_H_MASK                GENMASK(3, 0)
#define MAX96757_DSI43_HFP_H(x)                     ((x) << 0)

#define MAX96757_DSI44                                  0x03AC

#define MAX96757_DSI45                                  0x03AD

#define MAX96757_DSI46                                  0x03AE
#define MAX96757_DSI46_RSVD7_W0                         BIT(7)
#define MAX96757_DSI46_RSVD6_W0                         BIT(6)
#define MAX96757_DSI46_RSVD5_W0                         BIT(5)
#define MAX96757_DSI46_HACTIVE_H_MASK            GENMASK(4, 0)
#define MAX96757_DSI46_HACTIVE_H(x)                 ((x) << 0)

#define MAX96757_GMSL1_2                                0x0402
#define MAX96757_GMSL1_2_RSVD7_W0                       BIT(7)
#define MAX96757_GMSL1_2_RSVD6_W0                       BIT(6)
#define MAX96757_GMSL1_2_SSEN                           BIT(5)
#define MAX96757_GMSL1_2_RSVD4_W0                       BIT(4)
#define MAX96757_GMSL1_2_RSVD3_W0                       BIT(3)
#define MAX96757_GMSL1_2_RSVD2_W0                       BIT(2)
#define MAX96757_GMSL1_2_RSVD1_W0                       BIT(1)
#define MAX96757_GMSL1_2_RSVD0_W0                       BIT(0)

#define MAX96757_GMSL1_7                                0x0407
#define MAX96757_GMSL1_7_DBL                            BIT(7)
#define MAX96757_GMSL1_7_HIBW                           BIT(6)
#define MAX96757_GMSL1_7_BWS                            BIT(5)
#define MAX96757_GMSL1_7_RSVD4_W0                       BIT(4)
#define MAX96757_GMSL1_7_DRS                            BIT(3)
#define MAX96757_GMSL1_7_HVEN                           BIT(2)
#define MAX96757_GMSL1_7_RSVD1_W0                       BIT(1)
#define MAX96757_GMSL1_7_PXL_CRC                        BIT(0)

#define MAX96757_GMSL1_F                                0x040F
#define MAX96757_GMSL1_F_CNTL4_IN_EN                    BIT(7)
#define MAX96757_GMSL1_F_CNTL3_IN_EN                    BIT(6)
#define MAX96757_GMSL1_F_CNTL2_IN_EN                    BIT(5)
#define MAX96757_GMSL1_F_CNTL1_IN_EN                    BIT(4)
#define MAX96757_GMSL1_F_CNTL0_IN_EN                    BIT(3)
#define MAX96757_GMSL1_F_GPO_RX_EN                      BIT(2)
#define MAX96757_GMSL1_F_GPO_OUT_SEL                    BIT(1)
#define MAX96757_GMSL1_F_SET_GPO                        BIT(0)

#define MAX96757_GMSL1_15                               0x0415
#define MAX96757_GMSL1_15_RSVD7_W0                      BIT(7)
#define MAX96757_GMSL1_15_RSVD6_W0                      BIT(6)
#define MAX96757_GMSL1_15_RSVD5_W0                      BIT(5)
#define MAX96757_GMSL1_15_RSVD4_W0                      BIT(4)
#define MAX96757_GMSL1_15_RSVD3_W0                      BIT(3)
#define MAX96757_GMSL1_15_RSVD2_W0                      BIT(2)
#define MAX96757_GMSL1_15_SEL_VESA                      BIT(1)
#define MAX96757_GMSL1_15_SEL_RGB888                    BIT(0)

#define MAX96757_HDCP_BKSV                              0x1680

#define MAX96757_HDCP_RI                                0x1685

#define MAX96757_HDCP_AN                                0x1688

#define MAX96757_HDCP_AKSV                              0x1690

#define MAX96757_HDCP_TX15                              0x1695
#define MAX96757_HDCP_TX15_RSVD7_W0                     BIT(7)
#define MAX96757_HDCP_TX15_EN_INT_COMP                  BIT(6)
#define MAX96757_HDCP_TX15_RSVD5_W0                     BIT(5)
#define MAX96757_HDCP_TX15_RSVD4_W0                     BIT(4)
#define MAX96757_HDCP_TX15_HDCP_RESET                   BIT(3)
#define MAX96757_HDCP_TX15_START_AUTH                   BIT(2)
#define MAX96757_HDCP_TX15_VSYNC_DET                    BIT(1)
#define MAX96757_HDCP_TX15_ENC_EN                       BIT(0)

#define MAX96757_HDCP_TX16                              0x1696
#define MAX96757_HDCP_TX16_PJF_MATCH                    BIT(4)
#define MAX96757_HDCP_TX16_V_MATCHED                    BIT(3)
#define MAX96757_HDCP_TX16_PJ_MATCHED                   BIT(2)
#define MAX96757_HDCP_TX16_RI_MATCHED                   BIT(1)
#define MAX96757_HDCP_TX16_INVALID_BK                   BIT(0)

#define MAX96757_HDCP_TX37                              0x16B7
#define MAX96757_HDCP_TX37_HDCP_PD                      BIT(7)
#define MAX96757_HDCP_TX37_FAST_PJ_EN                   BIT(6)
#define MAX96757_HDCP_TX37_FORCE_VIDEO                  BIT(5)
#define MAX96757_HDCP_TX37_FORCE_AUDIO                  BIT(4)
#define MAX96757_HDCP_TX37_AH_MASK_ENCOFF               BIT(3)
#define MAX96757_HDCP_TX37_AH_MASK_NEWDEV               BIT(2)
#define MAX96757_HDCP_TX37_AH_MASK_KSVRDY               BIT(1)
#define MAX96757_HDCP_TX37_AH_MASK_FAIL                 BIT(0)

#endif /* __CSD_SER_MAX96757_I_H__ */
