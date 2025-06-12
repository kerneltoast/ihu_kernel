/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __CSD_H__
#define __CSD_H__

/**
 * struct csd_get_diag_report_arg - Argument to CSD_GET_DIAG_REPORT ioctl
 *
 * @request_ptr:          request payload
 * @response_ptr:         pointer to a buffer for response (maybe NULL)
 * @payload_len:          length of request payload
 * @timeout:              timeout in ms for response (0: no response)
 * @response_buffer_size: size of response buffer
 */
struct csd_get_diag_report_arg {
	u8  *request_ptr;
	u8  *response_ptr;
	int payload_len;
	int timeout;
	int response_buffer_size;
} __packed;

#define CSD_GET_DIAG_REPORT               _IOWR('q', 1, struct csd_diag_arg *)
#define CSD_GET_DIAG_REPORT_MAX_TIMEOUT                                  20000

#define CSD_DIAG_RETRIES          10
#define CSD_RECOVERY_RETRIES       3
/* minimal time in ms between two diagnostic messages */
#define CSD_DIAG_DELAY            20

/* minimal time in us between any two messages */
#define CSD_MSG_DELAY           2000

/* This value defines the upper limit on displays attached at the same time. */
#define CSD_MAX_DEVICE_COUNT       3
#define CSD_BASE_MINOR             0

#define CSD_EVENT_PRIO_UNLOAD    -64
#define CSD_EVENT_PRIO_VIDEO     -56
#define CSD_EVENT_PRIO_SUSPEND   -40
#define CSD_EVENT_PRIO_INT_IRQ   -32
#define CSD_EVENT_PRIO_DIAG_ACT  -16  /* ongoing diagnostig request */
#define CSD_EVENT_PRIO_PERIODIC    0
#define CSD_EVENT_PRIO_DIAG_REP    8  /* new diagnostic request */
#define CSD_EVENT_PRIO_LF_READ    16
#define CSD_EVENT_PRIO_READ_HDCP  24
#define CSD_EVENT_PRIO_READ_EDID  32
#define CSD_EVENT_PRIO_MFG_TEST   48
#define CSD_EVENT_PRIO_EE_ACCESS  64

#define CSD_SER_DES_MAX_RETRIES    2

#define CSD_DES_ADDR_READ       0x91
#define CSD_DES_ADDR_WRITE      0x90
#define CSD_SER_ADDR_READ       0x81
#define CSD_SER_ADDR_WRITE      0x80
#define CSD_CSD_ADDR_READ       0x6D
#define CSD_CSD_ADDR_WRITE      0x6C

#define CSD_ACK_BYTE            0xC3
#define CSD_SYNC_BYTE           0x79

#define CSD_WRITE_TIMEOUT         20
#define CSD_READ_TIMEOUT          20

#define CSD_MAX_PAYLOAD         0xFF
#define CSD_CRC_SIZE     sizeof(u32)

#define CSD_WAITING_TIME_MS     8000

/*
 * Serializer is kept powered-on after probing as it is expected that the
 * video output will be turned on soon. However if video output is not enabled
 * within the time given here serializer will be powered down to save power.
 * Note: Diagnostic messages may be postponed for this duration.
 */
#define CSD_SER_IDLE_TIMEOUT         5000

#define CSD_CONVERTER_POLL           1000

#define CSD_MAX_TRANSMISSION_FAILS     10
#define CSD_MAX_MODE_FAILS             20
#define CSD_PROG_INT_RETRIES           10

#define CSD_DISPLAY_WIDTH_GMSL1       768
#define CSD_DISPLAY_HEIGHT_GMSL1     1024
#define CSD_DISPLAY_WIDTH_GMSL2      1152
#define CSD_DISPLAY_HEIGHT_GMSL2     1536
/* dots per meter */
#define CSD_DISPLAY_DPM_GMSL1        5565
#define CSD_DISPLAY_DPM_GMSL2        6776

#define CSD_HDCP_SOC_CHK_INTERVALL    400
#define CSD_HDCP_GMSL_CHK_INTERVALL  2000
#define CSD_HDCP_GMSL_CHK_RETRIES       5
#define CSD_HDCP_GMSL_REINIT_RETRIES    5
#define CSD_HDCP_SOC_REQ_RETRIES        3

#define CSD_HDCP_AN_SIZE                8
#define CSD_HDCP_AKSV_SIZE              5
#define CSD_HDCP_BKSV_SIZE              5

/*
 * To reset CSD using the control signal, we need a reset pulse of
 * CSD_RESET_PULSE_LENGTH ms. The control signal is however not connected
 * directly to our GPIO pin. Our GPIO pin goes to VIP and is forwarded to
 * CSD. We need to consider the propagation time inside VIP, as well as
 * the slow falling edge on the external signal, to make sure CSD actually
 * sees a low pulse of at least CSD_RESET_PULSE_LENGTH ms.
 *
 * According to spec a reset pulse length of 10 ms should be sufficient.
 * CSDs however need much longer to enter reset state. This is an agreed
 * deviation from spec. It was not possible to get the new requirement
 * for the reset pulse length. Measurements have shown worst case values of:
 *
 * CSD 1.5: 180 ms
 * CSD 3.0: 220 ms
 * CSD 3.1: 360 ms
 * CSD 3.5: 240 ms
 */
#define CSD_RESET_PULSE_LENGTH                        400
#define CSD_RESET_PROPAGATION_DELAY                    50
#define CSD_RESET_FALLING_TIME                         10

/* Serializer registers */

#define CSD_MAX_REG_DEV_ID                         0x000D

#define CSD_MAX_REG_DEV_REV_MASK                     0x0F

/* GMSL2 deserializer registers */

#define CSD_MAX_DREG_GMSL2_DEV_ID                  0x000D

#define CSD_MAX_DREG_GMSL2_DEV_REV_MASK              0x0F

#define CSD_MAX_DREG_GMSL2_HDCP_BKSV               0x0680

#define CSD_MAX_DREG_GMSL2_HDCP_RI                 0x0685

#define CSD_MAX_DREG_GMSL2_HDCP_AN                 0x0688

#define CSD_MAX_DREG_GMSL2_HDCP_AKSV               0x0690

#define CSD_MAX_DREG_GMSL2_HDCP_15                 0x0695
#define CSD_MAX_DREG_GMSL2_HDCP_15_RSVD7_W0        BIT(7)
#define CSD_MAX_DREG_GMSL2_HDCP_15_RSVD6_W0        BIT(6)
#define CSD_MAX_DREG_GMSL2_HDCP_15_RSVD5_W0        BIT(5)
#define CSD_MAX_DREG_GMSL2_HDCP_15_RSVD4_W0        BIT(4)
#define CSD_MAX_DREG_GMSL2_HDCP_15_RSVD3_W0        BIT(3)
#define CSD_MAX_DREG_GMSL2_HDCP_15_RSVD2_W0        BIT(2)
#define CSD_MAX_DREG_GMSL2_HDCP_15_AUTH_START      BIT(1)
#define CSD_MAX_DREG_GMSL2_HDCP_15_ENC_EN          BIT(0)

#define CSD_MAX_DREG_GMSL2_HDCP_37                 0x06B7
#define CSD_MAX_DREG_GMSL2_HDCP_37_HDCP_PD         BIT(7)
#define CSD_MAX_DREG_GMSL2_HDCP_37_AH_MODE         BIT(6)
#define CSD_MAX_DREG_GMSL2_HDCP_37_RSVD5_W0        BIT(5)
#define CSD_MAX_DREG_GMSL2_HDCP_37_RSVD4_W0        BIT(4)
#define CSD_MAX_DREG_GMSL2_HDCP_37_RSVD3_W0        BIT(3)
#define CSD_MAX_DREG_GMSL2_HDCP_37_RSVD2_W0        BIT(2)
#define CSD_MAX_DREG_GMSL2_HDCP_37_RSVD1_W0        BIT(1)
#define CSD_MAX_DREG_GMSL2_HDCP_37_RSVD0_W0        BIT(0)

/* GMSL1 deserializer registers and mappings */

#define CSD_MAX_DREG_GMSL1_DEV_ID                    0x1E

#define CSD_MAX_DREG_DEV_ID(gmsl2) \
	((gmsl2) ? CSD_MAX_DREG_GMSL2_DEV_ID : CSD_MAX_DREG_GMSL1_DEV_ID)

#define CSD_MAX_DREG_GMSL1_DEV_REV_MASK              0x0F

#define CSD_MAX_DREG_DEV_REV_MASK(gmsl2) \
	((gmsl2) ? CSD_MAX_DREG_GMSL2_DEV_REV_MASK : \
	CSD_MAX_DREG_GMSL1_DEV_REV_MASK)

#define CSD_MAX_DREG_GMSL1_HDCP_BKSV                 0x80

#define CSD_MAX_DREG_HDCP_BKSV(gmsl2) \
	((gmsl2) ? CSD_MAX_DREG_GMSL2_HDCP_BKSV : CSD_MAX_DREG_GMSL1_HDCP_BKSV)

#define CSD_MAX_DREG_GMSL1_HDCP_RI                   0x85

#define CSD_MAX_DREG_HDCP_RI(gmsl2) \
	((gmsl2) ? CSD_MAX_DREG_GMSL2_HDCP_RI : CSD_MAX_DREG_GMSL1_HDCP_RI)

#define CSD_MAX_DREG_GMSL1_HDCP_AN                   0x88

#define CSD_MAX_DREG_HDCP_AN(gmsl2) \
	((gmsl2) ? CSD_MAX_DREG_GMSL2_HDCP_AN : CSD_MAX_DREG_GMSL1_HDCP_AN)

#define CSD_MAX_DREG_GMSL1_HDCP_AKSV                 0x90

#define CSD_MAX_DREG_HDCP_AKSV(gmsl2) \
	((gmsl2) ? CSD_MAX_DREG_GMSL2_HDCP_AKSV : CSD_MAX_DREG_GMSL1_HDCP_AKSV)

#define CSD_MAX_DREG_GMSL1_HDCP_15                   0x95
#define CSD_MAX_DREG_GMSL1_HDCP_15_PD_HDCP         BIT(7)
#define CSD_MAX_DREG_GMSL1_HDCP_15_RSVD6_W0        BIT(6)
#define CSD_MAX_DREG_GMSL1_HDCP_15_RSVD5_W0        BIT(5)
#define CSD_MAX_DREG_GMSL1_HDCP_15_RSVD4_W0        BIT(4)
#define CSD_MAX_DREG_GMSL1_HDCP_15_GPIO1_FUNC      BIT(3)
#define CSD_MAX_DREG_GMSL1_HDCP_15_GPIO0_FUNC      BIT(2)
#define CSD_MAX_DREG_GMSL1_HDCP_15_AUTH_START      BIT(1)
#define CSD_MAX_DREG_GMSL1_HDCP_15_ENC_EN          BIT(0)

/* CSD messages */

#define CSD_MSG_IHU_REQUEST_ID                          0x01
#define CSD_MSG_IHU_REQUEST_MODE_TOUCH                  0x01
#define CSD_MSG_IHU_REQUEST_MODE_NORMAL                 0x02
#define CSD_MSG_IHU_REQUEST_MODE_PRBS                   0x03
#define CSD_MSG_IHU_REQUEST_REPORT_MODE_SINGLE          0x00
#define CSD_MSG_IHU_REQUEST_REPORT_MODE_CYCLIC          0x01
#define CSD_MSG_IHU_REQUEST_REPORT_RATE_100HZ           0x64
#define CSD_MSG_IHU_REQUEST_REPORT_RATE_150HZ           0x96
#define CSD_MSG_IHU_REQUEST_REPORT_RATE_200HZ           0xC8
#define CSD_MSG_IHU_REQUEST_REPORT_RATE_250HZ           0xFA

#define CSD_MSG_SHUTDOWN_REQUEST_ID                     0x02
#define CSD_MSG_SHUTDOWN_REQUEST_NO_SHUTDOWN            0x00
#define CSD_MSG_SHUTDOWN_REQUEST_SHUTDOWN               0x01

#define CSD_MSG_DISPLAY_STATUS_ID                       0x10
#define CSD_MSG_DISPLAY_STATUS_MODE_SLEEP               0x00
#define CSD_MSG_DISPLAY_STATUS_MODE_TOUCH               0x01
#define CSD_MSG_DISPLAY_STATUS_MODE_NORMAL              0x02
#define CSD_MSG_DISPLAY_STATUS_MODE_PRBS                0x03
#define CSD_MSG_DISPLAY_STATUS_DISPLAY_ON_OFF           0x00
#define CSD_MSG_DISPLAY_STATUS_DISPLAY_ON_ON            0x01
#define CSD_MSG_DISPLAY_STATUS_MAX_BRIGHTNESS           1023
#define CSD_MSG_DISPLAY_STATUS_MAX_VOLTAGE               254
#define CSD_MSG_DISPLAY_STATUS_MAX_CONSUMPTION            23
#define CSD_MSG_DISPLAY_STATUS_MAX_TEMPERATURE           250
#define CSD_MSG_DISPLAY_STATUS_ERR_1_LVDS_LOCK        BIT(0)
#define CSD_MSG_DISPLAY_STATUS_ERR_1_TOUCH_COM_ERR    BIT(1)
#define CSD_MSG_DISPLAY_STATUS_ERR_1_TOUCH_INIT_ERR   BIT(2)
#define CSD_MSG_DISPLAY_STATUS_ERR_1_VIDEO_LOSS       BIT(3)
#define CSD_MSG_DISPLAY_STATUS_ERR_1_LVDS_COM_ERR     BIT(4)
#define CSD_MSG_DISPLAY_STATUS_ERR_1_EPROM_COM_ERR    BIT(5)
#define CSD_MSG_DISPLAY_STATUS_ERR_1_EPROM_CON_ERR    BIT(6)
#define CSD_MSG_DISPLAY_STATUS_ERR_1_EPROM_CHK_ERR    BIT(7)
#define CSD_MSG_DISPLAY_STATUS_ERR_2_THERM_1_ERR      BIT(0)
#define CSD_MSG_DISPLAY_STATUS_ERR_2_THERM_2_ERR      BIT(1)
#define CSD_MSG_DISPLAY_STATUS_ERR_2_THERM_3_ERR      BIT(2)
#define CSD_MSG_DISPLAY_STATUS_ERR_2_THERM_4_ERR      BIT(3)
#define CSD_MSG_DISPLAY_STATUS_ERR_2_HIGH_TEMP        BIT(4)
#define CSD_MSG_DISPLAY_STATUS_ERR_2_BACK_1_ERR       BIT(5)
#define CSD_MSG_DISPLAY_STATUS_ERR_2_BACK_2_ERR       BIT(6)
#define CSD_MSG_DISPLAY_STATUS_ERR_2_V_MON_ERR        BIT(7)
#define CSD_MSG_DISPLAY_STATUS_ERR_3_LOW_VOLTAGE      BIT(0)
#define CSD_MSG_DISPLAY_STATUS_ERR_3_HIGH_VOLTAGE     BIT(1)
#define CSD_MSG_DISPLAY_STATUS_ERR_3_TOUCH_OPEN       BIT(2)
#define CSD_MSG_DISPLAY_STATUS_ERR_3_TOUCH_LOW        BIT(3)
#define CSD_MSG_DISPLAY_STATUS_ERR_3_TOUCH_REBOOT     BIT(4)
#define CSD_MSG_DISPLAY_STATUS_ERR_3_MODEL_ERR        BIT(5)
#define CSD_MSG_DISPLAY_STATUS_ERR_3_INT_ERR          BIT(6)
#define CSD_MSG_DISPLAY_STATUS_ERR_3_TEMP_WARN        BIT(7)
#define CSD_MSG_DISPLAY_STATUS_SLEEP_NORMAL             0x00
#define CSD_MSG_DISPLAY_STATUS_SLEEP_LOW_VOLTAGE        0x01
#define CSD_MSG_DISPLAY_STATUS_SLEEP_LVDS_ERR           0x02
#define CSD_MSG_DISPLAY_STATUS_SLEEP_LVDS_UNLOCK        0x03
#define CSD_MSG_DISPLAY_STATUS_SLEEP_LVDS_ERR_UNLOCK    0x04
#define CSD_MSG_DISPLAY_STATUS_SLEEP_TIMEOUT            0x05
#define CSD_MSG_DISPLAY_STATUS_SLEEP_WATCHDOG           0x06
#define CSD_MSG_DISPLAY_STATUS_SLEEP_UNKNOWN            0xFF

#define CSD_MSG_TOUCH_STATUS_ID                         0x11
#define CSD_MSG_TOUCH_STATUS_MAX_CNT                       4
#define CSD_MSG_TOUCH_REPORT_CONTACT_STATUS(x)  ((x) & 0x03)
#define CSD_MSG_TOUCH_REPORT_CONTACT_STATUS_DOWN        0x00
#define CSD_MSG_TOUCH_REPORT_CONTACT_STATUS_MOVE        0x01
#define CSD_MSG_TOUCH_REPORT_CONTACT_STATUS_UP          0x02
#define CSD_MSG_TOUCH_REPORT_CONTACT_ID(x)        ((x) >> 2)

#define CSD_MSG_DISPLAY_EDID_ID                         0x12

#define CSD_MSG_INTERRUPT_STATUS_ID                     0x20
#define CSD_MSG_INTERRUPT_STATUS_DEFAULT                0x00
#define CSD_MSG_INTERRUPT_STATUS_TOUCH_PANEL            0x01
#define CSD_MSG_INTERRUPT_STATUS_REG_APP                0x02
#define CSD_MSG_INTERRUPT_STATUS_REG_PBL                0x03

#define CSD_MSG_DIAG_REQUEST_ID                                         0x3C
#define CSD_MSG_DIAG_REQ_SESSION                                        0x10
#define CSD_MSG_DIAG_REQ_SESSION_DEF_RESP                               0x01
#define CSD_MSG_DIAG_REQ_SESSION_PROG_RESP                              0x02
#define CSD_MSG_DIAG_REQ_SESSION_EXT_RESP                               0x03
#define CSD_MSG_DIAG_REQ_SESSION_DEF_NO_RESP                            0x81
#define CSD_MSG_DIAG_REQ_SESSION_PROG_NO_RESP                           0x82
#define CSD_MSG_DIAG_REQ_SESSION_EXT_NO_RESP                            0x83
#define CSD_MSG_DIAG_REQ_RESET                                          0x11
#define CSD_MSG_DIAG_REQ_RESET_RESP                                     0x01
#define CSD_MSG_DIAG_REQ_RESET_NO_RESP                                  0x81
#define CSD_MSG_DIAG_REQ_TESTER                                         0x3E
#define CSD_MSG_DIAG_REQ_TESTER_RESP                                    0x00
#define CSD_MSG_DIAG_REQ_TESTER_NO_RESP                                 0x80

#define CSD_MSG_DIAG_RESPONSE_ID                                        0x3D
#define CSD_MSG_DIAG_RESP_SESSION                                       0x50
#define CSD_MSG_DIAG_RESP_SESSION_SIZE                                     6
#define CSD_MSG_DIAG_RESP_SESSION_DEF                                   0x01
#define CSD_MSG_DIAG_RESP_SESSION_PROG                                  0x02
#define CSD_MSG_DIAG_RESP_SESSION_EXT                                   0x03
#define CSD_MSG_DIAG_RESP_NEGATIVE                                      0x7F
#define CSD_MSG_DIAG_RESP_ERR_PENDING                                   0x78

#define CSD_MSG_DIAG_FIRST_PAYLOAD_SIZE (CSD_MAX_PAYLOAD - 2 - CSD_CRC_SIZE)
#define CSD_MSG_DIAG_CONS_PAYLOAD_SIZE  (CSD_MAX_PAYLOAD - 1 - CSD_CRC_SIZE)
#define CSD_MSG_DIAG_MAX_PAYLOAD_SIZE                                   4095
#define CSD_MSG_DIAG_FRAME_TYPE_SINGLE                                     0
#define CSD_MSG_DIAG_FRAME_TYPE_FIRST                                      1
#define CSD_MSG_DIAG_FRAME_TYPE_CONS                                       2
#define CSD_MSG_DIAG_PADDING_BYTE                                       0xAA

#endif /* __CSD_H__ */
