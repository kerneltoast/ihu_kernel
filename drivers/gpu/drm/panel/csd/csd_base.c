// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>
#include <linux/serdev.h>
#include <linux/of.h>
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#include <linux/crc32.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/kref.h>
#include <linux/plist.h>
#include <linux/kthread.h>
#include <linux/jiffies.h>
#include <linux/cdev.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <uapi/linux/sched/types.h>
#include <drm/drm_panel.h>
#include <drm/drm_bridge.h>
#include <drm/drm_modes.h>
#include <uapi/linux/media-bus-format.h>

#include "csd_common.h"
#include "csd_base.h"
#include "csd_ser_max96757.h"
#include "csd_ser_max96787.h"
#include "csd_des_max96778.h"

static bool gmsl2;
module_param(gmsl2, bool, 0444);
MODULE_PARM_DESC(gmsl2, "Use GMSL2 mode (default: GMSL1)");

static bool ee_mode;
module_param(ee_mode, bool, 0644);
MODULE_PARM_DESC(ee_mode, "Allow unsafe operations for testing purpose");

static unsigned int conv_timing[11];
module_param_array(conv_timing, uint, NULL, 0444);
MODULE_PARM_DESC(conv_timing, "Display timing for converter boards, implies that a converter board is used");

static unsigned short init_brightness =
	CSD_MSG_DISPLAY_STATUS_MAX_BRIGHTNESS / 2;
module_param_named(brightness, init_brightness, ushort, 0444);
MODULE_PARM_DESC(brightness, "Initial brightness (0 - "
	__stringify(CSD_MSG_DISPLAY_STATUS_MAX_BRIGHTNESS) ")");

static bool no_hdcp;
module_param(no_hdcp, bool, 0444);
MODULE_PARM_DESC(no_hdcp, "Don't use HDCP");

enum csd_receiver_state {
	CSD_RECEIVER_IDLE,
	CSD_RECEIVER_SYNC_LOST,
	CSD_RECEIVER_RECV,
	CSD_RECEIVER_CSD_REQ_DEV,
	CSD_RECEIVER_CSD_DES_RREQ_MSB,
	CSD_RECEIVER_CSD_DES_RREQ_LSB,
	CSD_RECEIVER_CSD_DES_WREQ_MSB,
	CSD_RECEIVER_CSD_DES_WREQ_LSB,
	CSD_RECEIVER_CSD_DES_RREQ_CNT,
	CSD_RECEIVER_CSD_DES_WREQ_CNT,
	CSD_RECEIVER_CSD_DES_RREQ_ACK,
	CSD_RECEIVER_CSD_DES_WREQ_DATA,
	CSD_RECEIVER_CSD_DES_RREQ_DATA,
	CSD_RECEIVER_CSD_DES_WREQ_ACK,
};

enum csd_event_type {
	CSD_EVENT_UNLOAD,
	CSD_EVENT_INT_IRQ,
	CSD_EVENT_SUSPEND,
	CSD_EVENT_DISABLE,
	CSD_EVENT_POST_DISABLE,
	CSD_EVENT_PRE_ENABLE,
	CSD_EVENT_ENABLE,
	CSD_EVENT_READ_EDID,
	CSD_EVENT_READ_HDCP,
	CSD_EVENT_EE_ACCESS,
	CSD_EVENT_DIAG_REPORT,
	CSD_EVENT_LF_READ,
	CSD_EVENT_ERRB_TEST,
	CSD_EVENT_CTRL_TEST,
	CSD_EVENT_GPIO_TEST,
};

enum csd_ee_access_type {
	CSD_EE_ACCESS_SER_READ,
	CSD_EE_ACCESS_SER_WRITE,
	CSD_EE_ACCESS_DES_READ,
	CSD_EE_ACCESS_DES_WRITE,
};

enum csd_state {
	CSD_STATE_PROBED_CRESET,
	CSD_STATE_SOFF_CRESET,
	CSD_STATE_SCONF_CRESET,
	CSD_STATE_SRECV_CRESET,
	CSD_STATE_CSD_POWERING_ON,
	CSD_STATE_CSD_PBL,
	CSD_STATE_CSD_PBL_APP_EARLY,
	CSD_STATE_CSD_PBL_APP_LATE,
	CSD_STATE_CSD_APP,
	CSD_STATE_IDLE,
	CSD_STATE_CSD_PROG,
	CSD_STATE_CSD_REG_PROG,
	CSD_STATE_CSD_PROG_REG_PROG,
	CSD_STATE_RESETTING_CSD,
	CSD_STATE_CONVERTER,
	CSD_STATE_WAIT_CONVERTER,
	CSD_STATE_RESET_SER,
	CSD_STATE_SUSPENDED,
	CSD_STATE_TERMINATE,
};

enum csd_wakeup_substate {
	CSD_WSUBSTATE_CSD_POWERING_ON,
	CSD_WSUBSTATE_CSD_PBL,
	CSD_WSUBSTATE_CSD_PBL_APP_EARLY,
	CSD_WSUBSTATE_CSD_PBL_APP_LATE,
	CSD_WSUBSTATE_CSD_APP,
};

enum csd_periodic_state {
	CSD_PSTATE_IDLE_IHU_REQ,
	CSD_PSTATE_IDLE_STATUS,
	CSD_PSTATE_IDLE_TOUCH,
	CSD_PSTATE_ACTIV_IHU_REQ,
	CSD_PSTATE_ACTIV_TOUCH1,
	CSD_PSTATE_ACTIV_TOUCH3,
	CSD_PSTATE_ACTIV_STATUS,
	CSD_PSTATE_ACTIV_TOUCH5,
	CSD_PSTATE_ACTIV_TOUCH7,
};

/*
 * struct csd_msg_ihu_request - IHU Request
 */
struct csd_msg_ihu_request {
	__be16 brightness;
	u8     lvds_link_parameter;
	u8     mode_control;
	u8     reserve0;
	u8     touch_report_mode;
	u8     touch_report_rate;
	u8     reserve1;
	u8     reserve2;
} __packed;

/*
 * struct csd_msg_shutdown_request - Shutdown Request
 */
struct csd_msg_shutdown_request {
	u8 shutdown_request;
} __packed;

/*
 * struct csd_msg_display_status - Display Status
 */
struct csd_msg_display_status {
	u8     mode;
	u8     display_on;
	__be16 brightness;
	u8     voltage;
	u8     current_consumption;
	u8     temperature;
	u8     lvds_register_error_count;
	u8     error_reg_1;
	u8     error_reg_2;
	u8     error_reg_3;
	u8     sleep_factor;
} __packed;

/*
 * struct csd_msg_touch_report - Touch Report
 */
struct csd_msg_touch_report {
	__be16 x;
	__be16 y;
	u8     state_id;
	u8     width;
	u8     height;
	u8     probability;
} __packed;

/*
 * struct csd_msg_touch_status - Touch Status
 */
struct csd_msg_touch_status {
	__be16                      screen_width;
	__be16                      screen_height;
	__be16                      time_stamp;
	u8                          reserve;
	u8                          number_of_reports;
	struct csd_msg_touch_report report[CSD_MSG_TOUCH_STATUS_MAX_CNT];
} __packed;

/*
 * struct csd_msg_display_edid - Display EDID
 */
struct csd_msg_display_edid {
	u8 data[128];
} __packed;

/*
 * struct csd_msg_interrupt_status - Interrupt Status
 */
struct csd_msg_interrupt_status {
	u8 msg_data_type;
} __packed;

/*
 * struct csd_msg_diag_first - payload of single or first diagnostic frame
 */
struct csd_msg_diag_first {
	u8 byte1;
	u8 byte2;
	u8 data[CSD_MSG_DIAG_FIRST_PAYLOAD_SIZE];
} __packed;

/*
 * struct csd_msg_diag_cons - payload of consecutive diagnostic frame
 */
struct csd_msg_diag_cons {
	u8 byte1;
	u8 data[CSD_MSG_DIAG_CONS_PAYLOAD_SIZE];
} __packed;

/**
 * struct csd_reply - Reply
 * @received: number of bytes received excluding ACK, including CRC
 *            (-1: waiting for ACK)
 * @length:   expected reply length excluding ACK and excluding CRC
 * @data:     buffer to store reply payload in
 * @msg_id:   message ID, iff not 0 a CRC check will be performed
 * @crc:      received CRC
 * @time:     time when the reply was received (end of message)
 * @done:     successful reply reception completion
 */
struct csd_reply {
	int               received;
	u8                length;
	u8                *data;
	u8                msg_id;
	u32               crc;
	ktime_t           time;
	struct completion done;
};

/**
 * struct csd_rx - UART RX related data
 * @lock:      lock to protect this structure
 * @state:     state of receiver
 * @expecting: number of bytes to receive before switching state
 * @reply:     data of a specific expected message (NULL: drop messages)
 */
struct csd_rx {
	struct mutex            lock;
	enum csd_receiver_state state;
	unsigned int            expecting;
	struct csd_reply        *reply;
};

/**
 * struct csd_event - CSD event
 * @node: plist node
 * @type: type of event
 * @data: type specific data (may be NULL)
 * @done: indicates that event processing was completed
 * @ret:  return value
 */
struct csd_event {
	struct plist_node   node;
	enum csd_event_type type;
	void                *data;
	struct completion   done;
	int                 ret;
};

/**
 * struct csd_waiter - Task waiting for events
 * @task: task waiting for events
 * @up:   set to true on wakeup
 */
struct csd_waiter {
	struct task_struct *task;
	bool               up;
};

/**
 * struct csd_event_buffer - Buffer for events
 * @lock:           lock to protect this structure
 * @module_unload:  module unload is ongoing (reject new events)
 * @int_irq_masked: ignore int irqs
 * @queue:          head of event queue
 * @waiter:         task waiting for incoming events or NULL
 */
struct csd_event_buffer {
	struct mutex      lock;
	bool              module_unload;
	bool              int_irq_masked;
	struct plist_head queue;
	struct csd_waiter *waiter;
};

/**
 * struct csd_ser_hdcp_func - HDCP related function pointers
 * @prepare_det:  prepare detection of HDCP requests from SOC
 *                (set to NULL if aux channel is not used for HDCP negotiation)
 * @get_an:       get AN
 * @get_aksv:     get AKSV
 * @set_bksv:     set BKSV
 * @get_ri:       get Ri
 * @wait_vsync:   wait for next VSYNC
 * @enable_enc:   enable encryption (may be NULL, means no HDCP support at all)
 *                If NULL, all other pointers might be / should be NULL as well.
 *                If set, all other pointers have to be set as well unless
 *                documented otherwise.
 * @readback_enc: readback register value after @enable_enc
 * @prepare_enc:  prepare for encryption enabling
 * @check_bksv:   check BKSV
 * @handle_req:   handle HDCP request from SOC
 *                (set to NULL if aux channel is not used for HDCP negotiation)
 * @check_req:    check for HDCP request from SOC
 *                (set to NULL if aux channel is not used for HDCP negotiation)
 * @read_stat:    read HDCP status
 *                (set to NULL if aux channel is not used for HDCP negotiation)
 */
struct csd_ser_hdcp_func {
	int (*prepare_det)(struct csd_data *csd);
	int (*get_an)(struct csd_data *csd, u8 *an, u8 size);
	int (*get_aksv)(struct csd_data *csd, u8 *aksv, u8 size);
	int (*set_bksv)(struct csd_data *csd, u8 *bksv, u8 size);
	int (*get_ri)(struct csd_data *csd, u8 *ri, u8 size);
	int (*wait_vsync)(struct csd_data *csd);
	int (*enable_enc)(struct csd_data *csd);
	int (*readback_enc)(struct csd_data *csd);
	int (*prepare_enc)(struct csd_data *csd);
	int (*check_bksv)(struct csd_data *csd);
	int (*handle_req)(struct csd_data *csd, u8 *bksv, u8 bksv_size);
	int (*check_req)(struct csd_data *csd, bool *request);
	int (*read_stat)(struct csd_data *csd, bool *enabled);
};

/**
 * struct csd_ser_info - serializer chip information
 * @dev_id:      device ID
 * @name:        chip name
 * @power_on:    performs initial configuration on power on
 * @pre_enable:  performs configuration on pre_enable request (may be NULL)
 * @enable:      performs configuration on enable request (may be NULL)
 * @post_wakeup: performs final configuration after wakeup (may be NULL)
 * @errb_test:   checks ERRB signal path
 * @int_test:    checks INT signal path (may be NULL)
 * @lock_test:   checks LOCK signal path (may be NULL)
 * @mode_test:   checks GMSL mode signal path (may be NULL)
 * @line_fault:  reads line fault diagnostics
 * @soft_reset:  perform soft reset of serializer (may be NULL)
 * @check_pclk:  check if pixel clock is present (used in GMSL1 mode only)
 *
 * @hdcp:        HDCP related function pointers (overall support for HDCP is
 *               determined based on @hdcp.enable_enc being set or not)
 */
struct csd_ser_info {
	u8   dev_id;
	char *name;
	int  (*power_on)(struct csd_data *csd, bool gmsl2);
	int  (*pre_enable)(struct csd_data *csd, bool gmsl2,
		const struct drm_display_mode *mode);
	int  (*enable)(struct csd_data *csd, bool gmsl2);
	int  (*post_wakeup)(struct csd_data *csd, bool gmsl2);
	int  (*errb_test)(struct csd_data *csd,
		enum csd_gpio_test_result *result, struct gpio_desc *errb,
		bool gmsl2);
	int  (*int_test)(struct csd_data *csd,
		enum csd_gpio_test_result *result, struct gpio_desc *irq,
		bool gmsl2);
	int  (*lock_test)(struct csd_data *csd,
		enum csd_gpio_test_result *result, struct gpio_desc *lock,
		bool gmsl2);
	int  (*mode_test)(struct csd_data *csd,
		enum csd_gpio_test_result *result, struct gpio_desc *mode,
		bool gmsl2);
	int  (*line_fault)(struct csd_data *csd, u8 *line_status);
	int  (*soft_reset)(struct csd_data *csd, bool gmsl2);
	int  (*check_pclk)(struct csd_data *csd, bool *pclk);

	struct csd_ser_hdcp_func hdcp;
};

/**
 * struct csd_des_info - deserializer chip information
 * @dev_id:       device ID
 * @name:         chip name
 * @hdcp_support: true if chip supports HDCP
 */
struct csd_des_info {
	u8   dev_id;
	char *name;
	bool hdcp_support;
};

/**
 * struct csd_conv_info - converter board information
 * @dev_id: device ID
 * @name:   name
 * @setup:  converter board setup function (may be NULL)
 * @status: indicates if board is already/still configured (may be NULL)
 */
struct csd_conv_info {
	u8   dev_id;
	char *name;
	int  (*setup)(struct csd_data *csd,
		const struct drm_display_mode *mode, u8 chip_rev);
	int  (*status)(struct csd_data *csd, bool *configured);
};

/**
 * struct csd_display_status - display status information for diagnostics
 * @lock:   protects this structure and \*status
 * @status: most recent display status
 */
struct csd_display_status {
	struct mutex                  lock;
	struct csd_msg_display_status *status;
};

/**
 * struct csd_brightness - buffer for passing new brightness values
 * @lock:       protects this structure
 * @brightness: requested display brightness
 */
struct csd_brightness {
	struct mutex lock;
	u16          brightness;
};

/**
 * struct csd_user_hdcp_req - buffer for passing HDCP requests from user
 * @lock:    protects this structure
 * @request: HDCP encryption requested
 */
struct csd_user_hdcp_req {
	struct mutex lock;
	bool         request;
};

/**
 * struct csd_ee_reg_access - information for manually register access
 * @lock:    protects this structure
 * @address: address of register to access
 */
struct csd_ee_reg_access {
	struct mutex lock;
	u16          address;
};

/**
 * struct csd_drvdata - Driver private data exposed driver wide as drvdata
 * @rx:             UART RX state
 * @event_buffer:   buffer to store events
 * @display_status: display status
 * @brightness:     requested brightness
 * @user_hdcp:      HDCP encryption request from user space
 * @ee_ser_access:  information for manual serializer register access
 * @ee_des_access:  information for manual deserializer register access
 * @resume:         indicates to resume from STR
 * @ser_info:       information about used serializer
 * @ser_rev:        serializer revision
 */
struct csd_drvdata {
	struct csd_rx             rx;
	struct csd_event_buffer   event_buffer;
	struct csd_display_status display_status;
	struct csd_brightness     brightness;
	struct csd_user_hdcp_req  user_hdcp;
	struct csd_ee_reg_access  ee_ser_access;
	struct csd_ee_reg_access  ee_des_access;
	struct completion         resume;
	const struct csd_ser_info *ser_info;
	u8                        ser_rev;
};

/**
 * struct csd_event_gpio_test_buffer - buffer passed with CSD_EVENT_GPIO_TEST
 * @buf:     buffer where to store human readable output
 * @written: bytes written to buffer
 */
struct csd_event_gpio_test_buffer {
	char    *buf;
	ssize_t written;
};

/**
 * struct csd_event_ee_access_buffer - buffer passed with CSD_EVENT_EE_ACCESS
 * @type:    access type
 * @address: address of register to access
 * @data:    data to read/write
 */
struct csd_event_ee_access_buffer {
	enum csd_ee_access_type type;
	u16                     address;
	u8                      data;
};

/**
 * struct csd_event_diag_rep_buffer - buffer passed with CSD_EVENT_DIAG_REPORT
 * @req:           pointer to request buffer
 * @req_size:      total size of request
 * @req_send:      number of bytes already transmitted
 * @resp:          pointer to response buffer
 * @resp_size:     size of response buffer
 * @resp_expected: expected size of response
 * @resp_recv:     number of bytes received
 * @expire:        initially set to timeout for response in jiffies,
 *                 current jiffies are added after request was transmitted
 * @retry:         number of consecutive failures
 */
struct csd_event_diag_rep_buffer {
	u8            *req;
	u16           req_size;
	u16           req_send;
	u8            *resp;
	u16           resp_size;
	u16           resp_expected;
	u16           resp_recv;
	unsigned long expire;
	int           retry;
};

/**
 * struct csd_event_read_hdcp_buffer - buffer passed with CSD_EVENT_READ_HDCP
 * @des_support: true if deserializer supports HDCP
 * @show_bksv:   indicates if BKSV should be exposed to user space
 *               (valid only if @des_support is true)
 * @enabled:     indicates if HDCP is enabled
 *               (valid only if @des_support is true)
 * @bksv:        BKSV copied from deserializer
 *               (valid only if @show_bksv and @enabled are true)
 */
struct csd_event_read_hdcp_buffer {
	bool des_support;
	bool show_bksv;
	bool enabled;
	u8   bksv[CSD_HDCP_BKSV_SIZE];
};

/**
 * struct csd_ref_count - reference counter
 * @lock:           protects this structure
 * @devices_in_use: bit map indicating which index values are used
 */
struct csd_ref_count {
	struct mutex lock;
	u32          devices_in_use;
};

/**
 * struct csd_cdev_drvdata - exposure of device structure to cdev
 * @lock:      protects this structure
 * @dev:       pointer to main device structure
 * @ref_count: number of active users + 1
 * @done:      completed when ref_count drops to zero
 */
struct csd_cdev_drvdata {
	struct mutex      lock;
	struct device     *dev;
	struct kref       ref_count;
	struct completion done;
};

/**
 * struct csd_global_data - global data of this driver
 * @refs:    reference counter
 * @major:   major number for dev files (along with the first minor number)
 * @class:   class for dev files
 * @drvdata: device structures exposed to cdevs
 * @cdev:    chardev handling all minor numbers
 */
struct csd_global_data {
	struct csd_ref_count    refs;
	dev_t                   major;
	struct class            *class;
	struct csd_cdev_drvdata drvdata[CSD_MAX_DEVICE_COUNT];
	struct cdev             cdev;
};

/**
 * struct csd_msg_header_u8 - message header with 1 byte reg address
 * @sync:  CSD_SYNC_BYTE
 * @dev:   device address
 * @reg:   register address
 * @count: payload length
 */
struct csd_msg_header_u8 {
	u8 sync;
	u8 dev;
	u8 reg;
	u8 count;
} __packed;

/**
 * struct csd_msg_header_u16 - message header with 2 byte reg address
 * @sync:    CSD_SYNC_BYTE
 * @dev:     device address
 * @reg_msb: register address (MSB)
 * @reg_lsb: register address (LSB)
 * @count:   payload length
 */
struct csd_msg_header_u16 {
	u8 sync;
	u8 dev;
	u8 reg_msb;
	u8 reg_lsb;
	u8 count;
} __packed;

/**
 * struct csd_write_packet_u8h - write packet with 1 byte reg address
 * @header: message header
 * @data:   message payload
 */
struct csd_write_packet_u8h {
	struct csd_msg_header_u8 header;
	u8                       data[CSD_MAX_PAYLOAD];
} __packed;

/**
 * struct csd_write_packet_u16h - write packet with 2 byte reg address
 * @header: message header
 * @data:   message payload
 */
struct csd_write_packet_u16h {
	struct csd_msg_header_u16 header;
	u8                        data[CSD_MAX_PAYLOAD];
} __packed;

/**
 * struct csd_chip_id - ser/des device ID and revision
 * @id:  device ID
 * @rev: device revision
 */
struct csd_chip_id {
	u8 id;
	u8 rev;
} __packed;

/**
 * struct csd_data - Driver private data used by probe, main thread and remove
 * @serdev:          serdev device
 * @write_buffer:    buffer to assemble outgoing write messages
 * @read_buffer:     buffer used for error checking reads
 * @drvdata:         data exposed as drvdata
 * @gpio_power_down: power down GPIO
 * @gpio_gmsl_mode:  GMSL mode GPIO
 * @gpio_ctrl:       control GPIO
 * @gpio_int:        interrupt GPIO
 * @gpio_errb:       ERRB GPIO
 * @gpio_lock:       LOCK GPIO
 * @gpio_int_irq:    interrupt number of interrupt GPIO
 * @last_msg:        time when last response to any message to any of ser,
 *                   des or CSD was received
 * @csd_ctrl_time:   time when gpio_ctrl was toggled, cleared to 0
 *                   when known that CSD is in reset state
 * @index:           index of display (used to name device files in /dev/)
 * @csd_dev:         device created for cdev representing CSD
 * @input_dev:       input device
 * @panel:           drm_panel
 * @bridge:          wrapper around panel for systems missing panel support
 * @main_thread:     thread running event loop
 */
struct csd_data {
	struct serdev_device *serdev;
	u8                   write_buffer[sizeof(struct csd_write_packet_u16h)];
	u8                   read_buffer[2][CSD_MAX_PAYLOAD];

	struct csd_drvdata   drvdata;

	struct gpio_desc     *gpio_power_down;
	struct gpio_desc     *gpio_gmsl_mode;
	struct gpio_desc     *gpio_ctrl;
	struct gpio_desc     *gpio_int;
	struct gpio_desc     *gpio_errb;
	struct gpio_desc     *gpio_lock;

	unsigned int         gpio_int_irq;

	ktime_t              last_msg;

	ktime_t              csd_ctrl_time;

	int                  index;

	struct device        *csd_dev;
	struct input_dev     *input_dev;
	struct drm_panel     panel;
	struct drm_bridge    *bridge;

	struct task_struct   *main_thread;
};

/**
 * struct csd_periodic_buffer - State of periodic events
 * @state:     current state
 * @last_time: timestamp (start of transmission) of last periodic message
 */
struct csd_periodic_buffer {
	enum csd_periodic_state state;
	ktime_t                 last_time;
};

/**
 * struct csd_hdcp_state - HDCP state related information
 * @last_soc:  timestamp (start of transmission) of last periodic check
 *             for HDCP requests from SOC
 * @last_gmsl: timestamp (start of transmission) of last periodic check
 *             of link integrity on GMSL link
 * @enabled:   true if HDCP is enabled on serializer side of GMSL link
 * @bksv:      BKSV copied from deserializer
 */
struct csd_hdcp_state {
	ktime_t last_soc;
	ktime_t last_gmsl;
	bool    enabled;
	u8      bksv[CSD_HDCP_BKSV_SIZE];
};

/**
 * struct csd_state_buffer - driver state maintained by main thread
 * @exposed_status:     index in display_status array indicating the
 *                      element currently exposed to drvdata
 * @display_status:     flip buffers for display status
 * @periodic:           state of periodic events
 * @mode_fails:         number of consecutive reports of CSD to be in
 *                      another mode than expected
 * @transmission_fails: number of consecutive transmission failures
 * @touch_down:         contact status of tracked touch points
 * @prog_signature:     set when switching to CSD_POWERING_ON state,
 *                      true indicates that CSD is expected to switch to
 *                      programming session as requested by diagnostic request
 * @line_status:        cached line fault status or CSD_LINE_STATUS_UNKNOWN
 * @des_info:           static information about detected deserializer
 * @hdcp:               HDCP state information
 * @last_diag:          time stamp (end of reception) of the CSD reply to
 *                      the last diagnostic frame
 */
struct csd_state_buffer {
	int                           exposed_status;
	struct csd_msg_display_status display_status[2];
	struct csd_periodic_buffer    periodic;
	int                           mode_fails;
	int                           transmission_fails;
	bool                          touch_down[CSD_MSG_TOUCH_STATUS_MAX_CNT];
	bool                          prog_signature;
	u8                            line_status;
	const struct csd_des_info     *des_info;
	struct csd_hdcp_state         hdcp;
	ktime_t                       last_diag;
};

static const struct drm_display_mode csd_display_mode_gmsl1 = {
	DRM_MODE("768x1024", DRM_MODE_TYPE_DRIVER, 55000,
		768, 808, 840, 878, 0,
		1024, 1044, 1045, 1049, 0,
		DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC
	)
};

static const struct drm_display_mode csd_display_mode_gmsl2 = {
	DRM_MODE("1152x1536", DRM_MODE_TYPE_DRIVER, 123340,
		1152, 1184, 1224, 1280, 0,
		1536, 1539, 1540, 1606, 0,
		DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC
	)
};

static struct drm_display_mode csd_display_mode_conv_timing = {
	DRM_MODE("conv_timing", DRM_MODE_TYPE_DRIVER, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		0
	)
};

/**
 * csd_get_device() - Returns pointer to struct device
 * @csd: pointer to csd_data
 *
 * Return: pointer to struct device
 */
struct device *csd_get_device(struct csd_data *csd)
{
	return &csd->serdev->dev;
}

/**
 * csd_ser_supports_hdcp() - Indicates if serializer supports HDCP
 * @ser_info: serializer information
 *
 * Return: true if serializer supports HDCP
 */
static bool csd_ser_supports_hdcp(const struct csd_ser_info *ser_info)
{
	return !!ser_info->hdcp.enable_enc;
}

/**
 * csd_using_conv_display_mode() - Indicates if converter board timing is used
 *
 * Return: true if converter board timing is used
 */
static bool csd_using_conv_display_mode(void)
{
	if (csd_display_mode_conv_timing.clock)
		return true;

	return false;
}

/**
 * csd_get_display_mode() - Returns current display mode
 *
 * Return: pointer to current display mode
 */
static const struct drm_display_mode *csd_get_display_mode(void)
{
	if (csd_using_conv_display_mode())
		return &csd_display_mode_conv_timing;

	if (gmsl2)
		return &csd_display_mode_gmsl2;

	return &csd_display_mode_gmsl1;
}

static ssize_t serializer_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", drvdata->ser_info->name);
}
static DEVICE_ATTR_RO(serializer);

static ssize_t serializer_rev_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);

	return sprintf(buf, "0x%02hhX\n", drvdata->ser_rev);
}
static DEVICE_ATTR_RO(serializer_rev);

static ssize_t mode_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_display_status *display_status = &drvdata->display_status;
	u8 mode;

	mutex_lock(&display_status->lock);

	if (!display_status->status) {
		mutex_unlock(&display_status->lock);
		return -ENODEV;
	}

	mode = display_status->status->mode;

	mutex_unlock(&display_status->lock);

	switch (mode) {
	case CSD_MSG_DISPLAY_STATUS_MODE_SLEEP:
		return sprintf(buf, "sleep\n");
	case CSD_MSG_DISPLAY_STATUS_MODE_TOUCH:
		return sprintf(buf, "touch\n");
	case CSD_MSG_DISPLAY_STATUS_MODE_NORMAL:
		return sprintf(buf, "normal\n");
	case CSD_MSG_DISPLAY_STATUS_MODE_PRBS:
		return sprintf(buf, "rpbs\n");
	default:
		return sprintf(buf, "unknown\n");
	}
}
static DEVICE_ATTR_RO(mode);

static ssize_t display_on_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_display_status *display_status = &drvdata->display_status;
	u8 display_on;

	mutex_lock(&display_status->lock);

	if (!display_status->status) {
		mutex_unlock(&display_status->lock);
		return -ENODEV;
	}

	display_on = display_status->status->display_on;

	mutex_unlock(&display_status->lock);

	switch (display_on) {
	case CSD_MSG_DISPLAY_STATUS_DISPLAY_ON_OFF:
		return sprintf(buf, "off\n");
	case CSD_MSG_DISPLAY_STATUS_DISPLAY_ON_ON:
		return sprintf(buf, "on\n");
	default:
		return sprintf(buf, "unknown\n");
	}
}
static DEVICE_ATTR_RO(display_on);

static ssize_t actual_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_display_status *display_status = &drvdata->display_status;
	__be16 brightness;

	mutex_lock(&display_status->lock);

	if (!display_status->status) {
		mutex_unlock(&display_status->lock);
		return -ENODEV;
	}

	brightness = display_status->status->brightness;

	mutex_unlock(&display_status->lock);

	if (be16_to_cpu(brightness) > CSD_MSG_DISPLAY_STATUS_MAX_BRIGHTNESS)
		return sprintf(buf, "unknown\n");

	return sprintf(buf, "%hu\n", be16_to_cpu(brightness));
}
static DEVICE_ATTR_RO(actual_brightness);

static ssize_t voltage_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_display_status *display_status = &drvdata->display_status;
	u8 voltage;

	mutex_lock(&display_status->lock);

	if (!display_status->status) {
		mutex_unlock(&display_status->lock);
		return -ENODEV;
	}

	voltage = display_status->status->voltage;

	mutex_unlock(&display_status->lock);

	if (voltage > CSD_MSG_DISPLAY_STATUS_MAX_VOLTAGE)
		return sprintf(buf, "unknown\n");

	return sprintf(buf, "%i\n", ((int) voltage) * 100);
}
static DEVICE_ATTR_RO(voltage);

static ssize_t consumption_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_display_status *display_status = &drvdata->display_status;
	u8 consumption;

	mutex_lock(&display_status->lock);

	if (!display_status->status) {
		mutex_unlock(&display_status->lock);
		return -ENODEV;
	}

	consumption = display_status->status->current_consumption;

	mutex_unlock(&display_status->lock);

	if (consumption > CSD_MSG_DISPLAY_STATUS_MAX_CONSUMPTION)
		return sprintf(buf, "unknown\n");

	return sprintf(buf, "%i\n", ((int) consumption) * 100);
}
static DEVICE_ATTR_RO(consumption);

static ssize_t temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_display_status *display_status = &drvdata->display_status;
	u8 temperature;

	mutex_lock(&display_status->lock);

	if (!display_status->status) {
		mutex_unlock(&display_status->lock);
		return -ENODEV;
	}

	temperature = display_status->status->temperature;

	mutex_unlock(&display_status->lock);

	if (temperature > CSD_MSG_DISPLAY_STATUS_MAX_TEMPERATURE)
		return sprintf(buf, "unknown\n");

	return sprintf(buf, "%i\n", ((int) temperature) - 100);
}
static DEVICE_ATTR_RO(temperature);

static ssize_t temperature_warn_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_display_status *display_status = &drvdata->display_status;
	u8 error_reg_3;

	mutex_lock(&display_status->lock);

	if (!display_status->status) {
		mutex_unlock(&display_status->lock);
		return -ENODEV;
	}

	error_reg_3 = display_status->status->error_reg_3;

	mutex_unlock(&display_status->lock);

	if (error_reg_3 & CSD_MSG_DISPLAY_STATUS_ERR_3_TEMP_WARN)
		return sprintf(buf, "1\n");

	return sprintf(buf, "0\n");
}
static DEVICE_ATTR_RO(temperature_warn);

static ssize_t lvds_errors_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_display_status *display_status = &drvdata->display_status;
	u8 lvds_errors;

	mutex_lock(&display_status->lock);

	if (!display_status->status) {
		mutex_unlock(&display_status->lock);
		return -ENODEV;
	}

	lvds_errors = display_status->status->lvds_register_error_count;

	mutex_unlock(&display_status->lock);

	return sprintf(buf, "%hhu\n", lvds_errors);
}
static DEVICE_ATTR_RO(lvds_errors);

static ssize_t error_reg_1_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_display_status *display_status = &drvdata->display_status;
	u8 error_reg_1;

	mutex_lock(&display_status->lock);

	if (!display_status->status) {
		mutex_unlock(&display_status->lock);
		return -ENODEV;
	}

	error_reg_1 = display_status->status->error_reg_1;

	mutex_unlock(&display_status->lock);

	return sprintf(buf, "0x%02hhX\n", error_reg_1);
}
static DEVICE_ATTR_RO(error_reg_1);

static ssize_t error_reg_2_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_display_status *display_status = &drvdata->display_status;
	u8 error_reg_2;

	mutex_lock(&display_status->lock);

	if (!display_status->status) {
		mutex_unlock(&display_status->lock);
		return -ENODEV;
	}

	error_reg_2 = display_status->status->error_reg_2;

	mutex_unlock(&display_status->lock);

	return sprintf(buf, "0x%02hhX\n", error_reg_2);
}
static DEVICE_ATTR_RO(error_reg_2);

static ssize_t error_reg_3_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_display_status *display_status = &drvdata->display_status;
	u8 error_reg_3;

	mutex_lock(&display_status->lock);

	if (!display_status->status) {
		mutex_unlock(&display_status->lock);
		return -ENODEV;
	}

	error_reg_3 = display_status->status->error_reg_3;

	mutex_unlock(&display_status->lock);

	return sprintf(buf, "0x%02hhX\n", error_reg_3);
}
static DEVICE_ATTR_RO(error_reg_3);

static ssize_t sleep_factor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_display_status *display_status = &drvdata->display_status;
	u8 sleep_factor;

	mutex_lock(&display_status->lock);

	if (!display_status->status) {
		mutex_unlock(&display_status->lock);
		return -ENODEV;
	}

	sleep_factor = display_status->status->sleep_factor;

	mutex_unlock(&display_status->lock);

	switch (sleep_factor) {
	case CSD_MSG_DISPLAY_STATUS_SLEEP_NORMAL:
		return sprintf(buf, "normal\n");
	case CSD_MSG_DISPLAY_STATUS_SLEEP_LOW_VOLTAGE:
		return sprintf(buf, "low voltage shutdown\n");
	case CSD_MSG_DISPLAY_STATUS_SLEEP_LVDS_ERR:
		return sprintf(buf, "communication failure (LVDS_ERR)\n");
	case CSD_MSG_DISPLAY_STATUS_SLEEP_LVDS_UNLOCK:
		return sprintf(buf, "communication failure (LVDS_UNLOCK)\n");
	case CSD_MSG_DISPLAY_STATUS_SLEEP_LVDS_ERR_UNLOCK:
		return sprintf(buf,
			"communication failure (LVDS_ERR and LVDS_UNLOCK)\n");
	case CSD_MSG_DISPLAY_STATUS_SLEEP_TIMEOUT:
		return sprintf(buf, "communication failure (timeout)\n");
	case CSD_MSG_DISPLAY_STATUS_SLEEP_WATCHDOG:
		return sprintf(buf, "watchdog reset\n");
	default:
		return sprintf(buf, "unknown\n");
	}
}
static DEVICE_ATTR_RO(sleep_factor);

static const struct device_attribute * const diag_sysfs_entries[] = {
	&dev_attr_mode,
	&dev_attr_display_on,
	&dev_attr_actual_brightness,
	&dev_attr_voltage,
	&dev_attr_consumption,
	&dev_attr_temperature,
	&dev_attr_temperature_warn,
	&dev_attr_lvds_errors,
	&dev_attr_error_reg_1,
	&dev_attr_error_reg_2,
	&dev_attr_error_reg_3,
	&dev_attr_sleep_factor,
};

static ssize_t brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	u16 brightness;

	mutex_lock(&drvdata->brightness.lock);

	brightness = drvdata->brightness.brightness;

	mutex_unlock(&drvdata->brightness.lock);

	return sprintf(buf, "%hu\n", brightness);
}

static ssize_t brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	u16 brightness;
	int ret;

	ret = kstrtou16(buf, 0, &brightness);
	if (ret)
		return ret;

	if (brightness > CSD_MSG_DISPLAY_STATUS_MAX_BRIGHTNESS)
		return -ERANGE;

	mutex_lock(&drvdata->brightness.lock);

	drvdata->brightness.brightness = brightness;

	mutex_unlock(&drvdata->brightness.lock);

	return count;
}
static DEVICE_ATTR_RW(brightness);

static ssize_t max_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", CSD_MSG_DISPLAY_STATUS_MAX_BRIGHTNESS);
}
static DEVICE_ATTR_RO(max_brightness);

static struct csd_global_data csd_global;

/**
 * csd_in_ee_mode() - Checks if ee_mode is enabled
 *
 * Return: true when in ee_mode
 */
static bool csd_in_ee_mode(void)
{
	bool is_ee;

	kernel_param_lock(THIS_MODULE);
	is_ee = ee_mode;
	kernel_param_unlock(THIS_MODULE);

	return is_ee;
}

/**
 * csd_crc() - Computes CRC of communication message
 * @msg_id: message ID
 * @data:   pointer to buffer containing message payload
 * @length: length of payload
 *
 * This function computes a CRC according to CSD specification. The CRC is
 * computed over the message ID and the payload of the packet.
 *
 * Return: computed CRC
 */
static u32 csd_crc(u8 msg_id, const u8 *data, u8 length)
{
	return ~bitrev32(crc32_be(crc32_be(~0, &msg_id, 1), data, length));
}

/**
 * csd_check_crc() - Checks CRC of message and tries re-sync if required
 * @dev: pointer to device structure (used for printing to kernel log)
 * @rx:  receiver related state information
 *
 * This function checks the CRC of the message which is currently in the reply
 * buffer. In case the CRC doesn't match all bytes in the reply buffer are
 * dropped up to and including the next ACK byte. In case there is no ACK byte
 * in the reply buffer (including the CRC buffer) all bytes are dropped. The
 * receiver state and reply buffer are updated accordingly.
 *
 * Return: 0 when CRC matches, -EAGAIN otherwise
 */
static int csd_check_crc(struct device *dev, struct csd_rx *rx)
{
	struct csd_reply *reply = rx->reply;
	u32 crc_comp;
	int i;

	crc_comp = csd_crc(reply->msg_id, reply->data, reply->length);

	if (crc_comp == reply->crc)
		return 0;

	dev_info(dev, "Checksum mismatch, computed 0x%08X, received 0x%08X\n",
		crc_comp, reply->crc);

	/*
	 * A CRC error means that there was either a transmission error or our
	 * parser is just out of sync. Try to resync by moving to the next ACK
	 * which may already be in some buffer or which may be received later.
	 */

	/* Try to find another ACK in data buffer */
	for (i = 0; i < reply->length; i++) {
		int j;

		if (reply->data[i] != CSD_ACK_BYTE)
			continue;

		if (i != reply->length - 1) {
			memmove(&reply->data[0], &reply->data[i + 1],
				reply->length - (i + 1));
		}
		reply->received -= i + 1;
		/* CSD_CRC_SIZE <= received < length + CSD_CRC_SIZE */

		/* Move bytes buffered in crc buffer to data buffer */
		for (j = 0; j < i + 1 && j < CSD_CRC_SIZE; j++) {
			reply->data[reply->received - CSD_CRC_SIZE + j] =
				(u8) (reply->crc >> (24 - j * 8));
		}

		rx->state = CSD_RECEIVER_RECV;
		return -EAGAIN;
	}

	/* Try to find another ACK in CRC buffer */
	for (i = 0; i < CSD_CRC_SIZE; i++) {
		int j;

		if ((u8) (reply->crc >> (24 - i * 8)) != CSD_ACK_BYTE)
			continue;

		reply->received = CSD_CRC_SIZE - (i + 1);
		/* 0 <= received < CSD_CRC_SIZE */

		/* Move bytes buffered in crc buffer to data buffer */
		for (j = 0; j < reply->length && j < reply->received; j++) {
			reply->data[j] =
				(u8) (reply->crc >> (24 - (i + 1 + j) * 8));
		}

		rx->state = CSD_RECEIVER_RECV;
		return -EAGAIN;
	}

	reply->received = -1;
	rx->state = CSD_RECEIVER_SYNC_LOST;
	return -EAGAIN;
}

/**
 * csd_receive_buf() - Callback invoked when data are received on UART
 * @serdev: serdev device
 * @buf:    buffer containing received data
 * @size:   number of bytes in buffer
 *
 * Received data are processed based on the current state of the receiver.
 *
 * Return: size
 */
static int csd_receive_buf(struct serdev_device *serdev,
	const unsigned char *buf, size_t size)
{
	struct csd_drvdata *drvdata = serdev_device_get_drvdata(serdev);
	struct device *dev = &serdev->dev;
	struct csd_rx *rx = &drvdata->rx;
	struct csd_reply *reply;
	size_t i;

	print_hex_dump_debug("csd UART RX: ", DUMP_PREFIX_NONE,
		16, 1, buf, size, false);

	mutex_lock(&rx->lock);

	reply = rx->reply;

	if (reply)
		reply->time = ktime_get();

	for (i = 0; i < size; i++) {
		switch (rx->state) {

		case CSD_RECEIVER_IDLE:

			if (buf[i] == CSD_SYNC_BYTE) {
				/* SYNC */

				rx->state = CSD_RECEIVER_CSD_REQ_DEV;
				break;
			}
			/* fall through */

		case CSD_RECEIVER_SYNC_LOST:

			if (buf[i] == CSD_ACK_BYTE) {
				/* ACK */

				if (reply) {
					if (reply->length != 0)
						rx->state = CSD_RECEIVER_RECV;
					reply->received = 0;
				}
				/* ignore unexpected ACK */
			}

			/* ignore any other bytes */
			break;

		case CSD_RECEIVER_RECV:
			if (reply) {
				if (reply->received < 0) {
					dev_warn(dev, "Restarted read process\n");
					rx->state = CSD_RECEIVER_IDLE;
					break;
				}

				if (reply->msg_id &&
					reply->received >= reply->length) {

					reply->crc <<= 8;
					reply->crc |= (u32) buf[i];
				} else {
					reply->data[reply->received] = buf[i];
				}
				reply->received++;

			} else {
				dev_warn(dev, "Stopped receiving process\n");
				rx->state = CSD_RECEIVER_IDLE;
			}
			break;

		case CSD_RECEIVER_CSD_REQ_DEV:

			switch (buf[i]) {

			case CSD_DES_ADDR_READ:

				if (gmsl2)
					rx->state =
						CSD_RECEIVER_CSD_DES_RREQ_MSB;
				else
					rx->state =
						CSD_RECEIVER_CSD_DES_RREQ_LSB;
				break;

			case CSD_DES_ADDR_WRITE:

				if (gmsl2)
					rx->state =
						CSD_RECEIVER_CSD_DES_WREQ_MSB;
				else
					rx->state =
						CSD_RECEIVER_CSD_DES_WREQ_LSB;
				break;

			case CSD_SER_ADDR_READ:
				dev_warn(dev,
					"Detected a read access to serializer from CSD controller\n");

				rx->state = CSD_RECEIVER_SYNC_LOST;
				break;

			case CSD_SER_ADDR_WRITE:
				dev_warn(dev,
					"Detected a write access to serializer from CSD controller\n");

				rx->state = CSD_RECEIVER_SYNC_LOST;
				break;

			default:
				if (buf[i] & 0x01) {
					dev_warn(dev,
						"Detected a read access to 0x%02hhX from CSD controller\n",
						buf[i]);
				} else {
					dev_warn(dev,
						"Detected a write access to 0x%02hhX from CSD controller\n",
						buf[i]);
				}

				rx->state = CSD_RECEIVER_SYNC_LOST;
			}
			break;

		case CSD_RECEIVER_CSD_DES_RREQ_MSB:

			rx->state = CSD_RECEIVER_CSD_DES_RREQ_LSB;
			break;

		case CSD_RECEIVER_CSD_DES_RREQ_LSB:

			rx->state = CSD_RECEIVER_CSD_DES_RREQ_CNT;
			break;

		case CSD_RECEIVER_CSD_DES_WREQ_MSB:

			rx->state = CSD_RECEIVER_CSD_DES_WREQ_LSB;
			break;

		case CSD_RECEIVER_CSD_DES_WREQ_LSB:

			rx->state = CSD_RECEIVER_CSD_DES_WREQ_CNT;
			break;

		case CSD_RECEIVER_CSD_DES_RREQ_CNT:

			rx->state = CSD_RECEIVER_CSD_DES_RREQ_ACK;
			if (buf[i])
				rx->expecting = (unsigned int) buf[i];
			else
				rx->expecting = 256;
			break;

		case CSD_RECEIVER_CSD_DES_WREQ_CNT:

			rx->state = CSD_RECEIVER_CSD_DES_WREQ_DATA;
			if (buf[i])
				rx->expecting = (unsigned int) buf[i];
			else
				rx->expecting = 256;
			break;

		case CSD_RECEIVER_CSD_DES_RREQ_ACK:

			if (buf[i] == CSD_ACK_BYTE) {
				rx->state = CSD_RECEIVER_CSD_DES_RREQ_DATA;
			} else {
				dev_warn(dev, "Missing read ACK\n");
				rx->state = CSD_RECEIVER_SYNC_LOST;
			}
			break;

		case CSD_RECEIVER_CSD_DES_WREQ_DATA:

			if (!--rx->expecting)
				rx->state = CSD_RECEIVER_CSD_DES_WREQ_ACK;
			break;

		case CSD_RECEIVER_CSD_DES_RREQ_DATA:

			if (!--rx->expecting) {
				rx->state = CSD_RECEIVER_IDLE;
				dev_dbg(dev, "Filtered read access\n");
			}
			break;

		case CSD_RECEIVER_CSD_DES_WREQ_ACK:

			if (buf[i] == CSD_ACK_BYTE) {
				dev_dbg(dev, "Filtered write access\n");
				rx->state = CSD_RECEIVER_IDLE;
			} else {
				dev_warn(dev, "Missing write ACK\n");
				rx->state = CSD_RECEIVER_SYNC_LOST;
			}
		}

		if (reply && reply->received >= 0 &&
			(u8) reply->received ==
			reply->length +
			(reply->msg_id ? CSD_CRC_SIZE : 0)) {

			if (reply->msg_id) {
				if (csd_check_crc(dev, rx))
					continue;
			}

			rx->state = CSD_RECEIVER_IDLE;

			rx->reply = NULL;

			complete(&reply->done);

			reply = NULL;
		}
	}

	mutex_unlock(&rx->lock);

	return size;
}

/**
 * csd_sleep_until() - Sleeps until given time
 * @expire: absolute time
 */
static void csd_sleep_until(ktime_t expire)
{
	if (ktime_after(ktime_get(), expire))
		return;

	for (;;) {
		__set_current_state(TASK_UNINTERRUPTIBLE);

		if (!schedule_hrtimeout_range(&expire, CSD_HRTIME_DELTA,
			HRTIMER_MODE_ABS))

			break;
	}
}

/**
 * csd_wait_until_csd_in_reset() - Sleeps until CSD is actually in reset state
 * @csd: pointer to csd_data
 */
static void csd_wait_until_csd_in_reset(struct csd_data *csd)
{
	if (!ktime_to_ns(csd->csd_ctrl_time))
		return;

	csd_sleep_until(ktime_add_ms(csd->csd_ctrl_time,
		CSD_RESET_PULSE_LENGTH +
		CSD_RESET_PROPAGATION_DELAY +
		CSD_RESET_FALLING_TIME));

	csd->csd_ctrl_time = ns_to_ktime(0);

	/*
	 * We may have interrupted an ongoing register access by resetting
	 * the CSD. Our receiver might still be tracking this access.
	 * Reset receiver state as we know the access will not continue
	 * and it would prevent the next message to be received properly.
	 */
	mutex_lock(&csd->drvdata.rx.lock);
	csd->drvdata.rx.state = CSD_RECEIVER_IDLE;
	mutex_unlock(&csd->drvdata.rx.lock);
}

/**
 * csd_read() - Sends a read request over UART and receives response
 * @csd:      pointer to csd_data
 * @req:      raw request message to send
 * @req_size: size of request message
 * @msg_id:   for read requests to CSD this is a copy of the message ID
 *            encoded in the request message, for all other messages this
 *            needs to be zero, a non-zero value implies a CRC check on
 *            the response
 * @res:      buffer for response (may be NULL)
 * @res_size: size of response buffer
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_read(struct csd_data *csd, const u8 *req, u16 req_size,
	u8 msg_id, u8 *res, u8 res_size)
{
	struct device *dev = &csd->serdev->dev;
	struct csd_reply reply = {
		.received = -1,
		.length   = res_size,
		.data     = res,
		.msg_id   = msg_id,
		.crc      = 0,
		.done     = COMPLETION_INITIALIZER_ONSTACK(reply.done),
	};
	int ret;

	/*
	 * In deviation from the specification, CSD requires at least 2 ms
	 * between any two messages, independent if the message was directed
	 * to CSD, serializer or deserializer. When messages are sent faster
	 * CSD frequently fails to reply to the next message directed to CSD.
	 */
	if (ktime_to_ns(csd->csd_ctrl_time))
		csd_sleep_until(ktime_add_us(csd->last_msg, CSD_MSG_DELAY));

	mutex_lock(&csd->drvdata.rx.lock);
	csd->drvdata.rx.reply = &reply;
	mutex_unlock(&csd->drvdata.rx.lock);

	print_hex_dump_debug("csd UART TX: ", DUMP_PREFIX_NONE,
		16, 1, req, req_size, false);

	ret = serdev_device_write(csd->serdev, req, req_size,
		msecs_to_jiffies(CSD_WRITE_TIMEOUT) + 1);
	if (ret) {
		dev_warn(dev, "Failed to send data over UART\n");

		mutex_lock(&csd->drvdata.rx.lock);
		csd->drvdata.rx.reply = NULL;
		mutex_unlock(&csd->drvdata.rx.lock);

		return ret;
	}

	if (!wait_for_completion_timeout(&reply.done,
		msecs_to_jiffies(CSD_READ_TIMEOUT) + 1)) {

		dev_dbg(dev, "Timeout while waiting for response\n");

		mutex_lock(&csd->drvdata.rx.lock);
		/* reset receiver state due to timeout */
		csd->drvdata.rx.state = CSD_RECEIVER_IDLE;
		csd->drvdata.rx.reply = NULL;
		mutex_unlock(&csd->drvdata.rx.lock);

		return -ETIMEDOUT;
	}

	csd->last_msg = reply.time;

	return 0;
}

/**
 * csd_write() - Sends a write request over UART and receives ACK
 * @csd:      pointer to csd_data
 * @req:      raw request message to send
 * @req_size: size of request message
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_write(struct csd_data *csd, const u8 *req, u16 req_size)
{
	return csd_read(csd, req, req_size, 0, NULL, 0);
}

/**
 * csd_read_ser() - Reads continuous register range from serializer
 * @csd:  pointer to csd_data
 * @reg:  address of first register to read
 * @data: pointer to location where to store received values
 * @size: number of bytes (registers) to read
 *
 * Due to a hardware errata the most significant 8 bits of the addresses of
 * the first and last register accessed need to be identical otherwise the
 * access will fail.
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_read_ser(struct csd_data *csd, u16 reg, u8 *data, u8 size)
{
	struct csd_msg_header_u16 msg = {
		.sync = CSD_SYNC_BYTE,
		.dev = CSD_SER_ADDR_READ,
		.reg_msb = (u8) (reg >> 8),
		.reg_lsb = (u8) (reg >> 0),
		.count = size,
	};

	return csd_read(csd, (u8 *) &msg, sizeof(msg), 0, data, size);
}

/**
 * csd_write_ser() - Writes continuous register range of serializer
 * @csd:  pointer to csd_data
 * @reg:  address of first register to write
 * @data: pointer to array of values to write
 * @size: number of bytes (registers) to write
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_write_ser(struct csd_data *csd, u16 reg, const u8 *data, u8 size)
{
	struct csd_write_packet_u16h *msg =
		(struct csd_write_packet_u16h *) csd->write_buffer;

	msg->header.sync = CSD_SYNC_BYTE;
	msg->header.dev = CSD_SER_ADDR_WRITE;
	msg->header.reg_msb = (u8) (reg >> 8);
	msg->header.reg_lsb = (u8) (reg >> 0);
	msg->header.count = size;

	memcpy(msg->data, data, size);

	return csd_write(csd, (u8 *) msg, sizeof(msg->header) + size);
}

/**
 * csd_read_des() - Reads continuous register range from deserializer
 * @csd:  pointer to csd_data
 * @reg:  address of first register to read
 * @data: pointer to location where to store received values
 * @size: number of bytes (registers) to read
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_read_des(struct csd_data *csd, u16 reg, u8 *data, u8 size)
{
	int ret;

	if (gmsl2) {
		struct csd_msg_header_u16 msg = {
			.sync = CSD_SYNC_BYTE,
			.dev = CSD_DES_ADDR_READ,
			.reg_msb = (u8) (reg >> 8),
			.reg_lsb = (u8) (reg >> 0),
			.count = size,
		};

		ret = csd_read(csd, (u8 *) &msg, sizeof(msg), 0, data, size);
	} else {
		struct csd_msg_header_u8 msg = {
			.sync = CSD_SYNC_BYTE,
			.dev = CSD_DES_ADDR_READ,
			.reg = (u8) reg,
			.count = size,
		};

		ret = csd_read(csd, (u8 *) &msg, sizeof(msg), 0, data, size);
	}

	return ret;
}

/**
 * csd_write_des() - Writes a continuous register range of deserializer
 * @csd:  pointer to csd_data
 * @reg:  address of first register to write
 * @data: pointer to array of values to write
 * @size: number of bytes (registers) to write
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_write_des(struct csd_data *csd, u16 reg, const u8 *data, u8 size)
{
	int ret;

	if (gmsl2) {
		struct csd_write_packet_u16h *msg =
			(struct csd_write_packet_u16h *) csd->write_buffer;

		msg->header.sync = CSD_SYNC_BYTE;
		msg->header.dev = CSD_DES_ADDR_WRITE;
		msg->header.reg_msb = (u8) (reg >> 8);
		msg->header.reg_lsb = (u8) (reg >> 0);
		msg->header.count = size;

		memcpy(msg->data, data, size);

		ret = csd_write(csd, (u8 *) msg, sizeof(msg->header) + size);
	} else {
		struct csd_write_packet_u8h *msg =
			(struct csd_write_packet_u8h *) csd->write_buffer;

		msg->header.sync = CSD_SYNC_BYTE,
		msg->header.dev = CSD_DES_ADDR_WRITE,
		msg->header.reg = (u8) reg,
		msg->header.count = size,

		memcpy(msg->data, data, size);

		ret = csd_write(csd, (u8 *) msg, sizeof(msg->header) + size);
	}

	return ret;
}

/**
 * csd_buffers_equal() - Compares two buffers
 * @a:     first buffer
 * @b:     second buffer
 * @check: bit mask defining which bits to check or CSD_RW_CHECK_ALL
 * @size:  number of bytes
 *
 * Return: true when buffers are equal
 */
static bool csd_buffers_equal(const u8 *a, const u8 *b, const u8 *check,
	u8 size)
{
	u8 i;

	if (check == CSD_RW_CHECK_ALL)
		return !bcmp(a, b, size);

	for (i = 0; i < size; i++)
		if ((a[i] & check[i]) != (b[i] & check[i]))
			return false;

	return true;
}

/**
 * csd_read_serdes_pc() - Reads continuous register range with error checking
 * @csd:        pointer to csd_data
 * @reg:        address of first register to read
 * @data:       pointer to location where to store received values
 * @check:      bit mask for error checking (special values see below)
 * @size:       number of registers to read
 * @serializer: true for serializer read, false for deserializer read
 *
 * This function tries to compensate for transmission errors by reading
 * the specified registers multiple times and checking if the readout is
 * identical. Only bits set in @check are considered during check.
 * The values obtained by the most recent read are returned for ignored bits.
 *
 * Set @check to NULL to skip checking and use only retry feature. Avoid
 * providing a check buffer with all bits cleared. That would result in an
 * unnecessary read and useless comparison.
 *
 * Set @check to CSD_RW_CHECK_ALL to check all bits or provide a check buffer
 * with all bits set.
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_read_serdes_pc(struct csd_data *csd, u16 reg, u8 *data, const u8 *check,
	u8 size, bool serializer)
{
	struct device *dev = &csd->serdev->dev;
	int received = 0;
	int reads = 0;
	int first_error = 0;

	while (reads++ < 3 + CSD_SER_DES_MAX_RETRIES - (received ? 0 : 1)) {
		u8 *target_buffer;
		int ret;

		if (check && !received)
			target_buffer = csd->read_buffer[0];
		else
			target_buffer = data;

		if (serializer)
			ret = csd_read_ser(csd, reg, target_buffer, size);
		else
			ret = csd_read_des(csd, reg, target_buffer, size);

		if (ret) {
			if (!first_error)
				first_error = ret;
			continue;
		}

		received++;

		if (received == 1) {
			if (check)
				continue;

			return 0;
		}

		if (received == 2) {
			if (csd_buffers_equal(csd->read_buffer[0], data, check,
				size))

				return 0;

			dev_dbg(dev, "%serializer readout unstable for register(s) 0x%04hX - 0x%04hX\n",
				serializer ? "S" : "Des", reg, reg + size - 1);

			memcpy(csd->read_buffer[1], data, size);

			continue;
		}

		if (csd_buffers_equal(csd->read_buffer[0], data, check, size))
			return 0;

		if (csd_buffers_equal(csd->read_buffer[1], data, check, size))
			return 0;

		dev_err(dev, "Readout from %sserializer unstable for register(s) 0x%04hX - 0x%04hX\n",
			serializer ? "" : "de", reg, reg + size - 1);

		return -EIO;
	}

	return first_error;
}

/**
 * csd_write_serdes_pc() - Writes continuous register range with error checking
 * @csd:        pointer to csd_data
 * @reg:        address of first register to write
 * @data:       array of values to write
 * @check:      bit mask for error checking (special values see below)
 * @size:       number of bytes to write
 * @serializer: true for serializer write, false for deserializer write
 *
 * This function tries to compensate for transmission problems.
 * It performs retries in case the write fails and reads back the values
 * of the written registers checking if all bits defined in @check are set
 * as given by @data.
 *
 * Set @check to NULL to skip checking and use only retry feature. Avoid
 * providing a check buffer with all bits cleared. That would result in two
 * unnecessary reads and useless comparision.
 *
 * Set @check to CSD_RW_CHECK_ALL to check all bits or provide a check buffer
 * with all bits set.
 *
 * Return: 0 on success, negative error code otherwise
 */
int csd_write_serdes_pc(struct csd_data *csd, u16 reg, const u8 *data,
	const u8 *check, u8 size, bool serializer)
{
	struct device *dev = &csd->serdev->dev;
	int first_error = 0;
	int retry = 0;
	int ret;

	for (;;) {
		if (serializer)
			ret = csd_write_ser(csd, reg, data, size);
		else
			ret = csd_write_des(csd, reg, data, size);

		if (!ret)
			break;

		if (!first_error)
			first_error = ret;

		if (retry++ >= CSD_SER_DES_MAX_RETRIES)
			return first_error;
	}

	if (!check)
		return 0;

	ret = csd_read_serdes_pc(csd, reg, csd->write_buffer, check, size,
		serializer);

	if (ret)
		return ret;

	if (!csd_buffers_equal(csd->write_buffer, data, check, size)) {
		dev_err(dev, "Readback from %sserializer returned wrong value for register(s) 0x%04hX - 0x%04hX\n",
			serializer ? "" : "de", reg, reg + size - 1);

		return -EIO;
	}

	return 0;
}

/**
 * csd_read_csd() - Sends a read request to CSD and receives response
 * @csd:    pointer to csd_data
 * @msg_id: message ID
 * @data:   pointer to buffer where to store the response payload
 * @size:   expected size of response
 *
 * A read request with the provided message ID will be send to CSD and the
 * response will be received. ACK and CRC will be stripped so that only
 * the actual payload will be placed in the response buffer. The length
 * of the actual response has to match the expected size otherwise an
 * error will be returned.
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_read_csd(struct csd_data *csd, u8 msg_id, u8 *data, u8 size)
{
	struct csd_msg_header_u8 msg = {
		.sync = CSD_SYNC_BYTE,
		.dev = CSD_CSD_ADDR_READ,
		.reg = msg_id,
		.count = size + CSD_CRC_SIZE,
	};

	if (size > CSD_MAX_PAYLOAD - CSD_CRC_SIZE)
		return -EINVAL;

	return csd_read(csd, (u8 *) &msg, sizeof(msg), msg_id, data, size);
}

/**
 * csd_write_csd() - Sends a write request with given payload to CSD
 * @csd:    pointer to csd_data
 * @msg_id: message ID
 * @data:   pointer to payload
 * @size:   size of payload
 *
 * This function sends a write request packet including header and checksum
 * with the given payload. An ACK is received.
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_write_csd(struct csd_data *csd, u8 msg_id, const u8 *data,
	u8 size)
{
	u32 crc;
	struct csd_write_packet_u8h *msg =
		(struct csd_write_packet_u8h *) csd->write_buffer;

	msg->header.sync = CSD_SYNC_BYTE;
	msg->header.dev = CSD_CSD_ADDR_WRITE;
	msg->header.reg = msg_id;
	msg->header.count = size + CSD_CRC_SIZE;

	if (size > CSD_MAX_PAYLOAD - CSD_CRC_SIZE)
		return -EINVAL;

	memcpy(msg->data, data, size);

	crc = csd_crc(msg_id, data, size);

	msg->data[size + 0] = (u8) (crc >> 24);
	msg->data[size + 1] = (u8) (crc >> 16);
	msg->data[size + 2] = (u8) (crc >>  8);
	msg->data[size + 3] = (u8) (crc >>  0);

	return csd_write(csd, (u8 *) msg,
		sizeof(msg->header) + (u16) size + CSD_CRC_SIZE);
}

static const struct serdev_device_ops csd_serdev_device_ops = {
	.receive_buf  = csd_receive_buf,
	.write_wakeup = serdev_device_write_wakeup,
};

/**
 * csd_get_event_name() - Returns human readable name of event
 * @type: event type
 *
 * Return: human readable name of event
 */
static const char *csd_get_event_name(enum csd_event_type type)
{
	switch (type) {
	case CSD_EVENT_UNLOAD:
		return "unload";
	case CSD_EVENT_INT_IRQ:
		return "int_irq";
	case CSD_EVENT_SUSPEND:
		return "suspend";
	case CSD_EVENT_DISABLE:
		return "disable";
	case CSD_EVENT_POST_DISABLE:
		return "post_disable";
	case CSD_EVENT_PRE_ENABLE:
		return "pre_enable";
	case CSD_EVENT_ENABLE:
		return "enable";
	case CSD_EVENT_READ_EDID:
		return "read EDID";
	case CSD_EVENT_READ_HDCP:
		return "read HDCP";
	case CSD_EVENT_EE_ACCESS:
		return "manual register access";
	case CSD_EVENT_DIAG_REPORT:
		return "diag report";
	case CSD_EVENT_LF_READ:
		return "line fault read";
	case CSD_EVENT_ERRB_TEST:
		return "ERRB signal path test";
	case CSD_EVENT_CTRL_TEST:
		return "CTRL signal path test";
	case CSD_EVENT_GPIO_TEST:
		return "GPIO signal path test";
	}

	return "unknown";
}

/**
 * csd_complete_event() - Completes an event
 * @dev:   device structure used for printing to kernel log
 * @event: event to complete
 * @ret:   return value to store in event
 *
 * An event is completed by storing the return value in the event buffer and
 * calling complete on the completion structure. For events which don't have
 * a completion this function doesn't do anything.
 */
static void csd_complete_event(struct device *dev, struct csd_event *event,
	int ret)
{
	dev_dbg(dev, "Completing %s event with return value %i\n",
		csd_get_event_name(event->type), ret);

	event->ret = ret;
	complete(&event->done);
}

/**
 * csd_reject_event() - Completes an event with -EBUSY return value
 * @dev:   device structure used for printing to kernel log
 * @event: event to reject
 */
static void csd_reject_event(struct device *dev, struct csd_event *event)
{
	csd_complete_event(dev, event, -EBUSY);
}

/**
 * csd_reject_mfg_event() - Rejects an MFG event
 * @dev:   device structure used for printing to kernel log
 * @event: event to reject
 */
static void csd_reject_mfg_event(struct device *dev, struct csd_event *event)
{
	dev_err(dev, "Manufacturing tests are not allowed during normal operation. Please turn off video output\n");

	csd_reject_event(dev, event);
}

/**
 * csd_reject_events() - Rejects events in given non-empty queue
 * @dev:   device structure used for printing to kernel log
 * @queue: queue with events to reject
 */
static void csd_reject_events(struct device *dev, struct plist_head *queue)
{
	struct csd_event *event;
	struct csd_event *event_tmp;

	plist_for_each_entry_safe(event, event_tmp, queue, node) {
		plist_del(&event->node, queue);
		csd_reject_event(dev, event);
	}
}

/**
 * csd_event_buffer_unload() - Rejects queued events and blocks future events
 * @csd: pointer to csd_data
 *
 * This function is used when the driver is unloaded or unbound to ensure
 * further events are not enqueued and waiting threads are unblocked.
 */
static void csd_event_buffer_unload(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;
	struct csd_event_buffer *buffer = &csd->drvdata.event_buffer;

	mutex_lock(&buffer->lock);
	buffer->module_unload = true;
	mutex_unlock(&buffer->lock);

	if (plist_head_empty(&buffer->queue))
		return;

	csd_reject_events(dev, &buffer->queue);
}

/**
 * csd_event_wait() - Wait until an event is available
 * @buffer:  event buffer
 * @timeout: absolute timeout as ktime_t
 *
 * buffer->lock has to be acquired before calling this function.
 *
 * Return: 0 on success, -ETIME on timeout
 */
static int csd_event_wait(struct csd_event_buffer *buffer, ktime_t *timeout)
{
	struct csd_waiter waiter;

	if (!plist_head_empty(&buffer->queue))
		return 0;

	buffer->waiter = &waiter;
	waiter.task = current;
	waiter.up = false;

	for (;;) {
		int timeout_ret;

		__set_current_state(TASK_UNINTERRUPTIBLE);

		mutex_unlock(&buffer->lock);

		timeout_ret = schedule_hrtimeout_range(timeout,
			CSD_HRTIME_DELTA, HRTIMER_MODE_ABS);

		mutex_lock(&buffer->lock);

		if (waiter.up)
			return 0;

		if (!timeout_ret) {
			buffer->waiter = NULL;
			return -ETIME;
		}
	}
}

/**
 * csd_event_wakeup() - Wake up waiting task if any
 * @buffer: event buffer
 *
 * buffer->lock has to be acquired before calling this function.
 */
static void csd_event_wakeup(struct csd_event_buffer *buffer)
{
	struct csd_waiter *waiter = buffer->waiter;

	if (!waiter)
		return;

	buffer->waiter = NULL;
	waiter->up = true;
	wake_up_process(waiter->task);
}

/**
 * csd_event_restore_postponed() - Restore postponed events
 * @dev:       device structure used for printing to kernel log
 * @buffer:    event buffer
 * @postponed: queue with postponed events (might be empty)
 *
 * This function will add all events from @postponed back to the event buffer
 * preserving the order. @postponed might be uninitialized on return.
 */
static void csd_event_restore_postponed(struct device *dev,
	struct csd_event_buffer *buffer, struct plist_head *postponed)
{
	if (plist_head_empty(postponed))
		return;

	mutex_lock(&buffer->lock);

	if (buffer->module_unload) {
		mutex_unlock(&buffer->lock);
		csd_reject_events(dev, postponed);
		return;
	}

	if (!plist_head_empty(&buffer->queue)) {
		struct csd_event *event;
		struct csd_event *event_tmp;

		/* Move all events to postponed queue */
		plist_for_each_entry_safe(event, event_tmp, &buffer->queue,
			node) {

			plist_del(&event->node, &buffer->queue);
			plist_add(&event->node, postponed);
		}
	}

	/* Replace buffer queue with postponed queue */
	list_splice(&postponed->node_list, &buffer->queue.node_list);

	mutex_unlock(&buffer->lock);
}

/**
 * csd_wakeup_diag_is_valid_prog() - Checks for programming session request
 * @dev:   device structure used for printing to kernel log
 * @event: CSD_EVENT_DIAG_REPORT event
 *
 * This function returns true iff the given request is a valid programming
 * session request, which was not successfully transmitted so far. If processing
 * of the request was already started, i.e. the request was already successfully
 * sent, false is returned. False is also returned, if the response buffer
 * is not large enough to hold a positive response message.
 *
 * Return: true if event is a valid programming session request
 */
static bool csd_wakeup_diag_is_valid_prog(struct device *dev,
	struct csd_event *event)
{
	struct csd_event_diag_rep_buffer *diag_rep = event->data;

	if (diag_rep->req_size != 2)
		return false;

	if (diag_rep->req[0] != CSD_MSG_DIAG_REQ_SESSION)
		return false;

	if (diag_rep->req_send)
		return false;

	if (diag_rep->req[1] == CSD_MSG_DIAG_REQ_SESSION_PROG_RESP) {
		if (!diag_rep->resp) {
			dev_warn_ratelimited(dev,
				"Invalid diagnostic request. Timeout for a session change request needs to be non-zero if suppressPosRspMsgIndicationBit is not set\n");

			return false;
		}

		if (diag_rep->resp_size < CSD_MSG_DIAG_RESP_SESSION_SIZE) {
			dev_warn_ratelimited(dev,
				"Invalid diagnostic request. Response buffer not large enough for a session change request\n");

			return false;
		}
	} else if (diag_rep->req[1] == CSD_MSG_DIAG_REQ_SESSION_PROG_NO_RESP) {
		if (diag_rep->resp) {
			dev_warn_ratelimited(dev,
				"Invalid diagnostic request. Timeout for a session change request needs to be zero if suppressPosRspMsgIndicationBit is set\n");

			return false;
		}
	} else {
		return false;
	}

	return true;
}

/**
 * csd_reject_diag_events() - Rejects most queued CSD_EVENT_DIAG_REPORT events
 * @csd: pointer to csd_data
 *
 * All queued CSD_EVENT_DIAG_REPORT events are rejected except for valid
 * programming session requests. This exception is intended to assist
 * recovery of CSDs with firmware broken due to aborted software download.
 * Valid programming session requests are rejected only if the retry count
 * is too high. This is required to ensure a request is not blocking forever.
 */
static void csd_reject_diag_events(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;
	struct csd_event_buffer *buffer = &csd->drvdata.event_buffer;
	struct csd_event *event;
	struct csd_event *event_tmp;

	mutex_lock(&buffer->lock);

	if (plist_head_empty(&buffer->queue)) {
		mutex_unlock(&buffer->lock);
		return;
	}

	plist_for_each_entry_safe(event, event_tmp, &buffer->queue, node) {
		if (event->type == CSD_EVENT_DIAG_REPORT) {
			struct csd_event_diag_rep_buffer *diag_rep =
				event->data;

			if (!csd_wakeup_diag_is_valid_prog(dev, event) ||
				++diag_rep->retry > CSD_RECOVERY_RETRIES) {

				plist_del(&event->node, &buffer->queue);
				csd_reject_event(dev, event);
			}
		}
	}

	mutex_unlock(&buffer->lock);
}

/**
 * csd_event_enqueue() - Enqueues an event
 * @dev:    device structure used for printing to kernel log
 * @buffer: event buffer
 * @event:  event to enqueue
 *
 * This function itself can't fail but it may happen that the event is
 * rejected immediately.
 */
static void csd_event_enqueue(struct device *dev,
	struct csd_event_buffer *buffer, struct csd_event *event)
{
	bool enqueued = true;

	dev_dbg(dev, "Trying to enqueue %s event\n",
		csd_get_event_name(event->type));

	mutex_lock(&buffer->lock);

	if (buffer->module_unload) {

		enqueued = false;

	} else if (event->type == CSD_EVENT_INT_IRQ && buffer->int_irq_masked) {

		dev_dbg(dev, "Ignoring int interrupt\n");
		enqueued = false;

	} else {
		plist_add(&event->node, &buffer->queue);
		csd_event_wakeup(buffer);
	}

	mutex_unlock(&buffer->lock);

	if (!enqueued)
		csd_reject_event(dev, event);
}

/**
 * csd_event_diag_requeue() - Reinserts an ongoing diagnostic event
 * @dev:    device structure used for printing to kernel log
 * @buffer: event buffer
 * @event:  event to enqueue
 *
 * The event priority of the diagnostic event is increased while reinserting
 * for the first time. This is done for two reasons. First of all events are
 * usually enqueued and processed in FIFO fashion. Removing and reinserting
 * an event with the normal enqueue function would result in a changed order
 * in case at least one additional event with the same priority as the current
 * one is in the queue. The second reason is that the priority needs to be
 * increased above the priority of periodic messages. This avoids sending
 * of periodic messages between frames of one diagnostic message. Starting
 * with an lower priority on the other hand allows touch input to work even
 * when the driver gets spammed with diagnostic messages.
 */
static void csd_event_diag_requeue(struct device *dev,
	struct csd_event_buffer *buffer, struct csd_event *event)
{
	event->node.prio = CSD_EVENT_PRIO_DIAG_ACT;

	csd_event_enqueue(dev, buffer, event);
}

/**
 * csd_event_postpone() - Insert an event to postponed queue
 * @dev:       device structure used for printing to kernel log
 * @postponed: event queue for postponed events
 * @event:     event to enqueue
 *
 * Note: An interrupt event must never be postponed because an attempt to
 * mask/disable the interrupt line while the interrupt event is postponed
 * would immediately deadlock as the postponed event wouldn't be rejected.
 */
static void csd_event_postpone(struct device *dev, struct plist_head *postponed,
	struct csd_event *event)
{
	dev_dbg(dev, "Postponing %s event\n",
		csd_get_event_name(event->type));

	plist_add(&event->node, postponed);
}

/**
 * csd_get_event_prio() - Returns the priority associated with an event type
 * @type: event type
 *
 * Return: priority
 */
static int csd_get_event_prio(enum csd_event_type type)
{
	switch (type) {
	case CSD_EVENT_UNLOAD:
		return CSD_EVENT_PRIO_UNLOAD;
	case CSD_EVENT_INT_IRQ:
		return CSD_EVENT_PRIO_INT_IRQ;
	case CSD_EVENT_SUSPEND:
		return CSD_EVENT_PRIO_SUSPEND;
	case CSD_EVENT_DISABLE:
	case CSD_EVENT_POST_DISABLE:
	case CSD_EVENT_PRE_ENABLE:
	case CSD_EVENT_ENABLE:
		return CSD_EVENT_PRIO_VIDEO;
	case CSD_EVENT_READ_EDID:
		return CSD_EVENT_PRIO_READ_EDID;
	case CSD_EVENT_READ_HDCP:
		return CSD_EVENT_PRIO_READ_HDCP;
	case CSD_EVENT_EE_ACCESS:
		return CSD_EVENT_PRIO_EE_ACCESS;
	case CSD_EVENT_DIAG_REPORT:
		return CSD_EVENT_PRIO_DIAG_REP;
	case CSD_EVENT_LF_READ:
		return CSD_EVENT_PRIO_LF_READ;
	case CSD_EVENT_ERRB_TEST:
	case CSD_EVENT_CTRL_TEST:
	case CSD_EVENT_GPIO_TEST:
		return CSD_EVENT_PRIO_MFG_TEST;
	}

	return 0;
}

/**
 * csd_event_enqueue_and_wait() - Enqueues an event and waits for completion
 * @dev:    device structure used for printing to kernel log
 * @buffer: event buffer
 * @type:   event type
 * @data:   pointer to event specific data
 *
 * This function enqueues an event, blocks until event processing was
 * completed and returns the result of event processing. The result is
 * typically set by calling csd_complete_event() or csd_reject_event().
 *
 * Return: result of event processing
 */
static int csd_event_enqueue_and_wait(struct device *dev,
	struct csd_event_buffer *buffer, enum csd_event_type type, void *data)
{
	struct csd_event event = {
		.type = type,
		.ret  = -EFAULT,
		.done = COMPLETION_INITIALIZER_ONSTACK(event.done),
		.data = data,
	};

	plist_node_init(&event.node, csd_get_event_prio(type));

	csd_event_enqueue(dev, buffer, &event);

	wait_for_completion(&event.done);

	return event.ret;
}

/**
 * csd_ee_access() - Handle manual register access
 * @csd:   pointer to csd_data
 * @event: CSD_EVENT_EE_ACCESS event
 */
static void csd_ee_access(struct csd_data *csd, struct csd_event *event)
{
	struct device *dev = &csd->serdev->dev;
	struct csd_event_ee_access_buffer *buffer = event->data;
	int ret;

	switch (buffer->type) {

	case CSD_EE_ACCESS_SER_READ:
		ret = csd_read_ser(csd, buffer->address, &buffer->data, 1);
		break;

	case CSD_EE_ACCESS_SER_WRITE:
		ret = csd_write_ser(csd, buffer->address, &buffer->data, 1);
		break;

	case CSD_EE_ACCESS_DES_READ:
		ret = csd_read_des(csd, buffer->address, &buffer->data, 1);
		break;

	default: /* CSD_EE_ACCESS_DES_WRITE */
		ret = csd_write_des(csd, buffer->address, &buffer->data, 1);

	}

	csd_complete_event(dev, event, ret);
}

/**
 * csd_power_up_ser() - Powers up the serializer chip
 * @csd: pointer to csd_data
 *
 * This function powers up the serializer by raising the power down pin.
 * When GMSL 1 mode is configured the GMSL mode pin is raised before.
 */
static void csd_power_up_ser(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;

	dev_dbg(dev, "Powering up serializer\n");

	if (!gmsl2)
		gpiod_set_value_cansleep(csd->gpio_gmsl_mode, 1);

	gpiod_set_value_cansleep(csd->gpio_power_down, 1);

	/*
	 * Wait for serializer to power-up (no max defined in datasheet)
	 * MAX96747: 2.25 ms typical
	 * MAX96755: 2.25 ms typical
	 * MAX96787: 1.1  ms typical
	 */
	csd_msleep(5);
}

/**
 * csd_power_down_ser() - Powers down the serializer chip
 * @csd: pointer to csd_data
 *
 * Serializer is guaranteed to be powered down when this call returns.
 */
static void csd_power_down_ser(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;

	dev_dbg(dev, "Powering down serializer\n");

	gpiod_set_value_cansleep(csd->gpio_power_down, 0);

	if (!gmsl2)
		gpiod_set_value_cansleep(csd->gpio_gmsl_mode, 0);

	csd_usleep(100);
}

/**
 * csd_mask_int_irq() - Masks or unmasks interrupts generated by interrupt line
 * @csd:  pointer to csd_data
 * @mask: set to true to mask and to false to unmask
 *
 * Pending interrupt events will be discarded. This function must never
 * be called while processing an interrupt event otherwise disabling the
 * interrupt line will deadlock.
 */
static void csd_mask_int_irq(struct csd_data *csd, bool mask)
{
	struct device *dev = &csd->serdev->dev;
	struct csd_event_buffer *buffer = &csd->drvdata.event_buffer;

	dev_dbg(dev, "%sasking int interrupt\n", mask ? "M" : "Unm");

	mutex_lock(&buffer->lock);

	if (buffer->int_irq_masked == mask) {
		if (mask)
			dev_err(dev, "Tried to disable an already disabled interrupt\n");
		else
			dev_err(dev, "Tried to enable an already enabled interrupt\n");

		mutex_unlock(&buffer->lock);
		return;
	}

	/* Block / unblock future interrupts */
	buffer->int_irq_masked = mask;

	/* Discard pending interrupts */
	if (mask && !plist_head_empty(&buffer->queue)) {
		struct csd_event *event;
		struct csd_event *event_tmp;

		plist_for_each_entry_safe(event, event_tmp, &buffer->queue,
			node) {

			if (event->type == CSD_EVENT_INT_IRQ) {
				plist_del(&event->node, &buffer->queue);
				csd_reject_event(dev, event);
			}
		}
	}

	mutex_unlock(&buffer->lock);

	if (mask)
		disable_irq(csd->gpio_int_irq);
	else
		enable_irq(csd->gpio_int_irq);
}

/**
 * csd_get_event() - Returns event with highest priority from queue
 * @csd:     pointer to csd_data
 * @timeout: absolute timeout in ktime or NULL for infinite timeout
 *
 * This function returns the event with the highest priority from the
 * event queue. The event is removed from the queue. In case no event is
 * available NULL is returned after timeout has expired.
 *
 * When timeout has already elapsed only high priority events are returned.
 * I.e. NULL is returned even if there are events in the queue as long as
 * all pending events have a lower priority than a timeout. This is used
 * to implement periodic operations and makes sure that periodic operations
 * are not delayed infinitely by low priority requests.
 *
 * Return: highest priority event or NULL in case of timeout
 */
static struct csd_event *csd_get_event(struct csd_data *csd, ktime_t *timeout)
{
	struct device *dev = &csd->serdev->dev;
	struct csd_event_buffer *event_buffer = &csd->drvdata.event_buffer;
	struct csd_event *event;

	mutex_lock(&event_buffer->lock);

	if (csd_event_wait(event_buffer, timeout)) {

		mutex_unlock(&event_buffer->lock);

		dev_dbg(dev, "No events available, timeout\n");

		return NULL;
	}

	event = plist_first_entry(&event_buffer->queue, struct csd_event, node);

	if (timeout && event->node.prio > CSD_EVENT_PRIO_PERIODIC &&
		ktime_after(ktime_get(), *timeout)) {

		mutex_unlock(&event_buffer->lock);

		dev_dbg(dev, "Only low priority events available, timeout\n");

		return NULL;
	}

	plist_del(&event->node, &event_buffer->queue);

	mutex_unlock(&event_buffer->lock);

	dev_dbg(dev, "Processing %s event\n", csd_get_event_name(event->type));

	return event;
}

/**
 * csd_get_event_hard_timeout() - Returns event with highest priority from queue
 * @csd:     pointer to csd_data
 * @timeout: absolute timeout in ktime
 *
 * Same as csd_get_event(), but always returns timeout if timeout has already
 * elapsed, instead of checking for high priority events.
 *
 * Return: highest priority event or NULL in case of timeout
 */
static struct csd_event *csd_get_event_hard_timeout(struct csd_data *csd,
	ktime_t timeout)
{
	if (ktime_after(ktime_get(), timeout))
		return NULL;

	return csd_get_event(csd, &timeout);
}

/**
 * csd_prepare_suspend() - Prepares suspend
 * @csd: pointer to csd_data
 *
 * This function closes the serdev device and reinitializes the completion
 * structure used to wait for resume.
 *
 * Return: 0
 */
static int csd_prepare_suspend(struct csd_data *csd)
{
	serdev_device_close(csd->serdev);

	reinit_completion(&csd->drvdata.resume);

	return 0;
}

static const struct csd_ser_info ser_info[] = {
	{
		.dev_id      = 0x9C,
		.name        = "MAX96757",
		.power_on    = csd_max96757_power_on,
		.pre_enable  = csd_max96757_pre_enable,
		.post_wakeup = csd_max96757_post_wakeup,
		.errb_test   = csd_max96757_run_errb_test,
		.int_test    = csd_max96757_run_int_test,
		.lock_test   = csd_max96757_run_lock_test,
		.line_fault  = csd_max96757_get_line_fault,
		.soft_reset  = csd_max96757_soft_reset,
		.check_pclk  = csd_max96757_check_pclk,

		.hdcp = {
			.get_an       = csd_max96757_hdcp_get_an,
			.get_aksv     = csd_max96757_hdcp_get_aksv,
			.set_bksv     = csd_max96757_hdcp_set_bksv,
			.get_ri       = csd_max96757_hdcp_get_ri,
			.wait_vsync   = csd_max96757_hdcp_wait_vsync,
			.enable_enc   = csd_max96757_hdcp_enable_enc,
			.readback_enc = csd_max96757_hdcp_readback_enc,
			.prepare_enc  = csd_max96757_hdcp_prepare_enc,
			.check_bksv   = csd_max96757_hdcp_check_bksv,
		},
	},
	{
		.dev_id      = 0xB4,
		.name        = "MAX96785",
		.power_on    = csd_max96785_power_on,
		.enable      = csd_max96787_enable,
		.post_wakeup = csd_max96787_post_wakeup,
		.errb_test   = csd_max96787_run_errb_test,
		.int_test    = csd_max96787_run_int_test,
		.mode_test   = csd_max96787_run_mode_test,
		.line_fault  = csd_max96787_get_line_fault,
		.check_pclk  = csd_max96787_check_pclk,
	},
	{
		.dev_id      = 0xB5,
		.name        = "MAX96787",
		.power_on    = csd_max96787_power_on,
		.enable      = csd_max96787_enable,
		.post_wakeup = csd_max96787_post_wakeup,
		.errb_test   = csd_max96787_run_errb_test,
		.int_test    = csd_max96787_run_int_test,
		.mode_test   = csd_max96787_run_mode_test,
		.line_fault  = csd_max96787_get_line_fault,
		.check_pclk  = csd_max96787_check_pclk,

		.hdcp = {
			.prepare_det  = csd_max96787_hdcp_prepare_det,
			.get_an       = csd_max96787_hdcp_get_an,
			.get_aksv     = csd_max96787_hdcp_get_aksv,
			.set_bksv     = csd_max96787_hdcp_set_bksv,
			.get_ri       = csd_max96787_hdcp_get_ri,
			.wait_vsync   = csd_max96787_hdcp_wait_vsync,
			.enable_enc   = csd_max96787_hdcp_enable_enc,
			.readback_enc = csd_max96787_hdcp_readback_enc,
			.prepare_enc  = csd_max96787_hdcp_prepare_enc,
			.check_bksv   = csd_max96787_hdcp_check_bksv,
			.handle_req   = csd_max96787_hdcp_handle_req,
			.check_req    = csd_max96787_hdcp_check_req,
			.read_stat    = csd_max96787_hdcp_read_stat,
		},
	},
};

static const struct csd_des_info des_info_gmsl1[] = {
	{
		.dev_id       = 0x08,
		.name         = "MAX9266",
		.hdcp_support = true,
	},
	{
		.dev_id       = 0x24,
		.name         = "MAX9278",
		.hdcp_support = false,
	},
	{
		.dev_id       = 0x28,
		.name         = "MAX9282",
		.hdcp_support = true,
	},
};

static const struct csd_des_info des_info_gmsl2[] = {
	{
		.dev_id       = 0x82,
		.name         = "MAX96752",
		.hdcp_support = false,
	},
	{
		.dev_id       = 0x84,
		.name         = "MAX96754",
		.hdcp_support = true,
	},
};

static const struct csd_des_info des_info_fallback = {
	.dev_id       = 0,
	.name         = "unknown",
	.hdcp_support = false,
};

/**
 * csd_lookup_ser_info() - Looks up serializer info
 * @dev_id: device ID of serializer
 *
 * Return: pointer to matching csd_ser_info struct or NULL
 */
static const struct csd_ser_info *csd_lookup_ser_info(u8 dev_id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ser_info); i++) {
		if (ser_info[i].dev_id == dev_id)
			return &ser_info[i];
	}

	return NULL;
}

/**
 * csd_lookup_des_info() - Looks up deserializer info
 * @dev_id: device ID of deserializer
 *
 * Return: pointer to matching csd_des_info struct or NULL
 */
static const struct csd_des_info *csd_lookup_des_info(u8 dev_id)
{
	int i;

	if (gmsl2)
		for (i = 0; i < ARRAY_SIZE(des_info_gmsl2); i++) {
			if (des_info_gmsl2[i].dev_id == dev_id)
				return &des_info_gmsl2[i];
		}
	else
		for (i = 0; i < ARRAY_SIZE(des_info_gmsl1); i++) {
			if (des_info_gmsl1[i].dev_id == dev_id)
				return &des_info_gmsl1[i];
		}

	return NULL;
}

/**
 * csd_detect_serializer() - Tries to detect serializer
 * @csd:     pointer to csd_data
 * @ser_rev: pointer to location where to store serializer revision
 *
 * Tries to read the serializer ID and to find matching serializer info.
 *
 * Return: pointer to matching csd_ser_info struct or NULL
 */
static const struct csd_ser_info *csd_detect_serializer(struct csd_data *csd,
	u8 *ser_rev)
{
	struct device *dev = &csd->serdev->dev;
	int retry;

	for (retry = 0; retry < 10; retry++) {
		const struct csd_ser_info *ser;
		struct csd_chip_id chip_id;
		struct csd_chip_id chip_id_check = {
			.id = 0xFF,
			.rev = CSD_MAX_REG_DEV_REV_MASK,
		};
		int ret;

		ret = csd_read_ser_pc(csd, CSD_MAX_REG_DEV_ID, (u8 *) &chip_id,
			(u8 *) &chip_id_check, sizeof(chip_id));

		if (ret)
			continue;

		ser = csd_lookup_ser_info(chip_id.id);
		if (!ser) {
			dev_err(dev, "Serializer ID 0x%02hhX unknown\n",
				chip_id.id);
			continue;
		}

		chip_id.rev &= CSD_MAX_REG_DEV_REV_MASK;

		dev_info(dev, "Detected %s (rev. 0x%02hhX) serializer\n",
			ser->name, chip_id.rev);

		*ser_rev = chip_id.rev;
		return ser;
	}

	dev_err(dev, "Failed to detect serializer\n");
	return NULL;
}

/**
 * csd_detect_deserializer() - Tries to detect deserializer
 * @csd:      pointer to csd_data
 * @des_info: location where to store pointer to deserializer info
 *
 * Tries to read the deserializer ID and to find matching deserializer info.
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_detect_deserializer(struct csd_data *csd,
	const struct csd_des_info **des_info)
{
	struct device *dev = &csd->serdev->dev;
	const struct csd_des_info *des;
	struct csd_chip_id chip_id;
	struct csd_chip_id chip_id_check = {
		.id = 0xFF,
		.rev = CSD_MAX_DREG_DEV_REV_MASK(gmsl2),
	};
	int ret;

	ret = csd_read_des_pc(csd, CSD_MAX_DREG_DEV_ID(gmsl2), (u8 *) &chip_id,
		(u8 *) &chip_id_check, sizeof(chip_id));

	if (ret) {
		dev_err(dev, "Failed to detect deserializer\n");
		return ret;
	}

	des = csd_lookup_des_info(chip_id.id);
	if (!des) {
		dev_warn(dev,
			"Deserializer ID 0x%02hhX unknown. Display functionality might be limited\n",
			chip_id.id);

		*des_info = &des_info_fallback;
		return 0;
	}

	chip_id.rev &= CSD_MAX_DREG_DEV_REV_MASK(gmsl2);

	dev_info(dev, "Detected %s (rev. 0x%02hhX) deserializer\n",
		des->name, chip_id.rev);

	*des_info = des;
	return 0;
}

/**
 * csd_serializer_power_on() - Performs ser power on config
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_serializer_power_on(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;
	int first_error = 0;
	int retries = 10;

	for (;;) {
		int ret;

		ret = csd->drvdata.ser_info->power_on(csd, gmsl2);
		if (!ret)
			return 0;

		if (!first_error)
			first_error = ret;

		if (!retries--) {
			dev_err(dev, "Constantly failing to configure serializer\n");
			return first_error;
		}

		dev_warn(dev, "Failed to configure serializer\n");
		csd_power_down_ser(csd);
		csd_power_up_ser(csd);
	}
}

/**
 * csd_ser_do_power_on_pre_enable() - Ser power on and pre_enable config
 * @csd:         pointer to csd_data
 * @pwr_on_done: assume power on configuration was already done
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_ser_do_power_on_pre_enable(struct csd_data *csd,
	bool pwr_on_done)
{
	struct device *dev = &csd->serdev->dev;
	int first_error = 0;
	int retries = 10;

	for (;;) {
		int ret = 0;

		if (!pwr_on_done)
			ret = csd->drvdata.ser_info->power_on(csd, gmsl2);

		if (!ret) {
			if (!csd->drvdata.ser_info->pre_enable)
				return 0;

			ret = csd->drvdata.ser_info->pre_enable(csd, gmsl2,
				csd_get_display_mode());

			if (!ret)
				return 0;
		}

		if (!first_error)
			first_error = ret;

		if (!retries--) {
			dev_err(dev, "Constantly failing to configure serializer\n");
			return first_error;
		}

		dev_warn(dev, "Failed to configure serializer\n");
		csd_power_down_ser(csd);
		csd_power_up_ser(csd);

		pwr_on_done = false;
	}
}

/**
 * csd_serializer_power_on_pre_enable() - Ser power on and pre_enable config
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_serializer_power_on_pre_enable(struct csd_data *csd)
{
	return csd_ser_do_power_on_pre_enable(csd, false);
}

/**
 * csd_serializer_pre_enable() - Performs ser pre_enable config
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_serializer_pre_enable(struct csd_data *csd)
{
	return csd_ser_do_power_on_pre_enable(csd, true);
}

/**
 * csd_serializer_enable() - Performs serializer enable configuration
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_serializer_enable(struct csd_data *csd)
{
	if (!csd->drvdata.ser_info->enable)
		return 0;

	return csd->drvdata.ser_info->enable(csd, gmsl2);
}

/**
 * csd_serializer_post_wakeup() - Performs ser post-wakeup config
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_serializer_post_wakeup(struct csd_data *csd)
{
	if (!csd->drvdata.ser_info->post_wakeup)
		return 0;

	return csd->drvdata.ser_info->post_wakeup(csd, gmsl2);
}

/**
 * csd_hdcp_prepare() - Prepare for detection of HDCP requests
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_hdcp_prepare(struct csd_data *csd)
{
	if (csd->drvdata.ser_info->hdcp.prepare_det)
		return csd->drvdata.ser_info->hdcp.prepare_det(csd);

	mutex_lock(&csd->drvdata.user_hdcp.lock);

	/*
	 * prepare_det() prepares the serializer to detect subsequent
	 * HDCP requests received over auxiliary channel of video link.
	 * For video links / serializers that do not have this feature,
	 * we rely on user space to request HDCP encryption. We do the
	 * same as done in serializers with aux channel. We toggle a
	 * flag, which will get toggled back when a request comes in.
	 */
	csd->drvdata.user_hdcp.request = false;

	mutex_unlock(&csd->drvdata.user_hdcp.lock);

	return 0;
}

/**
 * csd_hdcp_copy_an() - Copies session random number from ser to des
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_hdcp_copy_an(struct csd_data *csd)
{
	u8 an[CSD_HDCP_AN_SIZE];
	int ret;

	ret = csd->drvdata.ser_info->hdcp.get_an(csd, an, sizeof(an));
	if (ret)
		return ret;

	ret = csd_write_des_c(csd, CSD_MAX_DREG_HDCP_AN(gmsl2), an, sizeof(an));
	if (ret)
		return ret;

	return 0;
}

/**
 * csd_hdcp_copy_aksv() - Copies AKSV from ser to des
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_hdcp_copy_aksv(struct csd_data *csd)
{
	u8 aksv[CSD_HDCP_AKSV_SIZE];
	int ret;

	ret = csd->drvdata.ser_info->hdcp.get_aksv(csd, aksv, sizeof(aksv));
	if (ret)
		return ret;

	ret = csd_write_des_c(csd, CSD_MAX_DREG_HDCP_AKSV(gmsl2), aksv,
		sizeof(aksv));

	if (ret)
		return ret;

	return 0;
}

/**
 * csd_hdcp_copy_bksv() - Copies BKSV from des to ser
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_hdcp_copy_bksv(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	int ret;

	ret = csd_read_des_c(csd, CSD_MAX_DREG_HDCP_BKSV(gmsl2),
		state->hdcp.bksv, sizeof(state->hdcp.bksv));

	if (ret)
		return ret;

	ret = csd->drvdata.ser_info->hdcp.set_bksv(csd, state->hdcp.bksv,
		sizeof(state->hdcp.bksv));

	if (ret)
		return ret;

	return 0;
}

/**
 * csd_hdcp_check_ri_once() - Checks HDCP link integrity without retries
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_hdcp_check_ri_once(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;
	u16 ri_ser;
	u16 ri_des;
	int ret;

	/*
	 * Documentation from Maxim recommends to perform following steps:
	 *
	 * 1. Read Ri
	 * 2. Read Ri'
	 * 3. Read Ri again
	 *    if new Ri is not equal to Ri from step 1 then go back to step 1
	 *
	 * As we are going to perform a retry in case of a mismatch anyway
	 * it seams reasonable to omit step 3.
	 */

	/* Read Ri */
	ret = csd->drvdata.ser_info->hdcp.get_ri(csd, (u8 *) &ri_ser,
		sizeof(ri_ser));

	if (ret)
		return ret;

	/* Read Ri' */
	ret = csd_read_des_pc(csd, CSD_MAX_DREG_HDCP_RI(gmsl2), (u8 *) &ri_des,
		NULL, sizeof(ri_des));

	if (ret)
		return ret;

	if (ri_ser != ri_des) {
		dev_dbg(dev, "Ri mismatch\n");
		return -EINVAL;
	}

	return 0;
}

/**
 * csd_hdcp_check_ri() - Checks HDCP link integrity with retries
 * @csd: pointer to csd_data
 *
 * This check is critical as we need to reinitialize HDCP if this check fails.
 * We really need to avoid false negatives. Therefore this function performs
 * several retries before failing eventually.
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_hdcp_check_ri(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;
	int retry;
	int ret;

	for (retry = 0; retry < CSD_HDCP_GMSL_CHK_RETRIES; retry++) {
		ret = csd_hdcp_check_ri_once(csd);
		if (!ret)
			return 0;
	}

	dev_err(dev, "GMSL link integrity check failed\n");
	return ret;
}

/**
 * csd_hdcp_enable_encryption() - Enables HDCP encryption on HDCP link
 * @csd: pointer to csd_data
 *
 * This function performs only the final enabling step.
 * Encryption needs to be enabled simultanously on serializer and
 * deserializer side between two VSYNC pulses.
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_hdcp_enable_encryption(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;
	u8 des_reg;
	u8 value;
	int ret;

	if (!gmsl2) {
		/*
		 * We need to do a real read-modify-write here as we can't
		 * make any assumption about the GPIO usage inside CSD.
		 */
		ret = csd_read_des_8pc(csd, CSD_MAX_DREG_GMSL1_HDCP_15, &value,
			CSD_MAX_DREG_GMSL1_HDCP_15_GPIO1_FUNC |
			CSD_MAX_DREG_GMSL1_HDCP_15_GPIO0_FUNC);

		if (ret)
			return ret;

		des_reg = value & (CSD_MAX_DREG_GMSL1_HDCP_15_GPIO1_FUNC |
			CSD_MAX_DREG_GMSL1_HDCP_15_GPIO0_FUNC);

		des_reg |= CSD_MAX_DREG_GMSL1_HDCP_15_AUTH_START |
			CSD_MAX_DREG_GMSL1_HDCP_15_ENC_EN;
	}

	/* Wait for next VSYNC */
	ret = csd->drvdata.ser_info->hdcp.wait_vsync(csd);
	if (ret)
		return ret;

	/* Enable encryption in serializer */
	ret = csd->drvdata.ser_info->hdcp.enable_enc(csd);
	if (ret)
		return ret;

	/* Enable encryption in deserializer */
	if (gmsl2)
		ret = csd_write_des_8pc(csd, CSD_MAX_DREG_GMSL2_HDCP_15,
			CSD_MAX_DREG_GMSL2_HDCP_15_AUTH_START |
			CSD_MAX_DREG_GMSL2_HDCP_15_ENC_EN,
			0);
	else
		ret = csd_write_des_8pc(csd, CSD_MAX_DREG_GMSL1_HDCP_15,
			des_reg,
			0);

	if (ret)
		return ret;

	/*
	 * Ideally we would read the VSYNC_DET bit here to see if we are still
	 * within one frame. Unfortunately the VSYNC_DET bit is in the same
	 * register as the ENC_EN bit. Therefore we might have cleared the
	 * VSYNC_DET bit while setting the ENC_EN bit. We can't do an easy
	 * check here. We simply assume that we were fast enough. If not we
	 * will notice later as the link integrity check will fail.
	 */

	/* Read back (we are now outside of time critical section) */
	ret = csd->drvdata.ser_info->hdcp.readback_enc(csd);
	if (ret)
		return ret;

	if (gmsl2) {
		ret = csd_read_des_8pc(csd, CSD_MAX_DREG_GMSL2_HDCP_15, &value,
			CSD_MAX_DREG_GMSL2_HDCP_15_ENC_EN);

		if (ret)
			return ret;

		if (!(value & CSD_MAX_DREG_GMSL2_HDCP_15_ENC_EN)) {

			dev_err(dev, "Readback of deserializer GMSL2_HDCP_15 register returned wrong value 0x%02hhX\n",
				value);
			return -EIO;
		}
	} else {
		ret = csd_read_des_8pc(csd, CSD_MAX_DREG_GMSL1_HDCP_15, &value,
			CSD_MAX_DREG_GMSL1_HDCP_15_PD_HDCP |
			CSD_MAX_DREG_GMSL1_HDCP_15_GPIO1_FUNC |
			CSD_MAX_DREG_GMSL1_HDCP_15_GPIO0_FUNC |
			CSD_MAX_DREG_GMSL1_HDCP_15_ENC_EN);

		if (ret)
			return ret;

		if ((value & (CSD_MAX_DREG_GMSL1_HDCP_15_PD_HDCP |
			CSD_MAX_DREG_GMSL1_HDCP_15_GPIO1_FUNC |
			CSD_MAX_DREG_GMSL1_HDCP_15_GPIO0_FUNC |
			CSD_MAX_DREG_GMSL1_HDCP_15_ENC_EN)) !=
			(des_reg & (CSD_MAX_DREG_GMSL1_HDCP_15_PD_HDCP |
			CSD_MAX_DREG_GMSL1_HDCP_15_GPIO1_FUNC |
			CSD_MAX_DREG_GMSL1_HDCP_15_GPIO0_FUNC |
			CSD_MAX_DREG_GMSL1_HDCP_15_ENC_EN))) {

			dev_err(dev, "Readback of deserializer GMSL1_HDCP_15 register returned wrong value 0x%02hhX\n",
				value);
			return -EIO;
		}
	}

	return 0;
}

/**
 * csd_hdcp_prepare_encryption() - Prepares HDCP encryption on GMSL link
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * This function performs the configuration required to enable HDCP encryption
 * on GMSL link but doesn't attempt to enable it. If this function fails
 * non-HDCP operation will typically still work fine.
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_hdcp_prepare_encryption(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	u8 value;
	int ret;

	ret = csd->drvdata.ser_info->hdcp.prepare_enc(csd);
	if (ret)
		return ret;

	if (!gmsl2) {
		/*
		 * We need to do a real read-modify-write here as we can't
		 * make any assumption about the GPIO usage inside CSD.
		 */
		ret = csd_read_des_8pc(csd, CSD_MAX_DREG_GMSL1_HDCP_15, &value,
			CSD_MAX_DREG_GMSL1_HDCP_15_GPIO1_FUNC |
			CSD_MAX_DREG_GMSL1_HDCP_15_GPIO0_FUNC);

		if (ret)
			return ret;

		value &= CSD_MAX_DREG_GMSL1_HDCP_15_GPIO1_FUNC |
			CSD_MAX_DREG_GMSL1_HDCP_15_GPIO0_FUNC;
	}

	/* disable encryption in deserializer (in case of reinit) */
	if (gmsl2)
		ret = csd_write_des_8pc(csd, CSD_MAX_DREG_GMSL2_HDCP_15,
			0,
			CSD_MAX_DREG_GMSL2_HDCP_15_ENC_EN);
	else
		ret = csd_write_des_8pc(csd, CSD_MAX_DREG_GMSL1_HDCP_15,
			value,
			CSD_MAX_DREG_GMSL1_HDCP_15_PD_HDCP |
			CSD_MAX_DREG_GMSL1_HDCP_15_GPIO1_FUNC |
			CSD_MAX_DREG_GMSL1_HDCP_15_GPIO0_FUNC |
			CSD_MAX_DREG_GMSL1_HDCP_15_ENC_EN);

	if (ret)
		return ret;

	/*
	 * Power up HDCP block in manual mode in GMSL2 deserializer.
	 * The block is powered on by default in GMSL1 deserializers.
	 */
	if (gmsl2) {
		ret = csd_write_des_8pc(csd, CSD_MAX_DREG_GMSL2_HDCP_37,
			0,
			CSD_MAX_DREG_GMSL2_HDCP_37_HDCP_PD |
			CSD_MAX_DREG_GMSL2_HDCP_37_AH_MODE);

		if (ret)
			return ret;
	}

	ret = csd_hdcp_copy_an(csd);
	if (ret)
		return ret;

	ret = csd_hdcp_copy_aksv(csd);
	if (ret)
		return ret;

	ret = csd_hdcp_copy_bksv(csd, state);
	if (ret)
		return ret;

	ret = csd->drvdata.ser_info->hdcp.check_bksv(csd);
	if (ret)
		return ret;

	ret = csd_hdcp_check_ri(csd);
	if (ret)
		return ret;

	return 0;
}

/**
 * csd_hdcp_reinit() - Performs HDCP (re-)initialization
 * @csd:           pointer to csd_data
 * @state:         state buffer
 * @critical_fail: set to true in case of critical failure (may be NULL)
 *
 * Most failures are not critical in the sense that they will not affect
 * non-HDCP operation. However failures at certain steps in the sequence
 * may break even non-HDCP operation. When such a failure occurs @critical_fail
 * is set to true, otherwise it is kept untouched.
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_hdcp_reinit(struct csd_data *csd, struct csd_state_buffer *state,
	bool *critical_fail)
{
	struct device *dev = &csd->serdev->dev;
	int ret;

	ret = csd_hdcp_prepare_encryption(csd, state);
	if (ret) {
		dev_dbg(dev, "HDCP configuration failed\n");
		return ret;
	}

	ret = csd_hdcp_enable_encryption(csd);
	if (ret) {
		dev_dbg(dev, "Enabling HDCP failed\n");

		if (critical_fail)
			*critical_fail = true;

		return ret;
	}

	return 0;
}

/**
 * csd_hdcp_handle_soc_req() - Handles HDCP request from SOC
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_hdcp_handle_soc_req(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	if (csd->drvdata.ser_info->hdcp.handle_req)
		return csd->drvdata.ser_info->hdcp.handle_req(csd,
			state->hdcp.bksv, sizeof(state->hdcp.bksv));

	mutex_lock(&csd->drvdata.user_hdcp.lock);

	/*
	 * For video links supporting HDCP negotiation over auxiliary channel
	 * we prepare information inside handle_req() to be read over DDC/AUX.
	 * Once done we reset the flag indicating HDCP requests. For user
	 * requested HDCP encryption we are done at this point, we just need
	 * to toggle our detection flag for subsequent requests to work.
	 */
	csd->drvdata.user_hdcp.request = false;

	mutex_unlock(&csd->drvdata.user_hdcp.lock);

	return 0;
}

/**
 * csd_post_wakeup_config() - Performs ser/des configuration after CSD wakeup
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * In order to support actual CSDs as well as converter boards a configuration
 * matching the converter boards is applied initially. After successful CSD
 * wakeup it is known for sure that an actual CSD is attached. At this point
 * it can be switched to the configuration required for the actual CSD. This
 * can happen after display wakeup but still before turning on the CSD i.e.
 * while CSD is still in sleep mode which is the mode entered after wakeup.
 * Therefore operating with slightly wrong settings initially will not have
 * any user visible effect.
 *
 * The configuration (if any) of the deserializer always needs to happen after
 * successful CSD wakeup to make sure that the deserialzer is actually powered
 * on and that the configuration sequence doesn't interfere with the wakeup
 * sequence. During wakeup sequence the CSD controller will perform register
 * access to deserializer.
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_post_wakeup_config(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	struct device *dev = &csd->serdev->dev;
	int ret;

	ret = csd_serializer_post_wakeup(csd);
	if (ret) {
		dev_err(dev, "Failed to reconfigure serializer\n");
		return ret;
	}

	ret = csd_detect_deserializer(csd, &state->des_info);
	if (ret)
		return ret;

	if (no_hdcp || !csd_ser_supports_hdcp(csd->drvdata.ser_info) ||
		!state->des_info->hdcp_support)

		return 0;

	if (state->hdcp.enabled) {
		int retry;

		for (retry = 0; retry < CSD_HDCP_GMSL_REINIT_RETRIES; retry++) {
			ret = csd_hdcp_reinit(csd, state, NULL);
			if (!ret)
				break;
		}

		if (ret) {
			dev_err(dev, "HDCP reinitialization on GMSL link failed after CSD reset\n");
			return ret;
		}

		state->hdcp.last_gmsl = ktime_get();

		for (retry = 0; retry < CSD_HDCP_SOC_REQ_RETRIES; retry++) {
			ret = csd_hdcp_handle_soc_req(csd, state);
			if (!ret)
				break;
		}

		if (ret) {
			dev_err(dev, "HDCP reinitialization failed after CSD reset\n");
			return ret;
		}

		dev_info(dev, "HDCP re-enabled on GMSL link level\n");
	} else {
		ret = csd_hdcp_prepare(csd);
		if (ret) {
			dev_err(dev, "Failed to prepare for HDCP\n");
			return ret;
		}
	}

	return 0;
}

/**
 * csd_power_up_csd() - Raises control line to CSD
 * @csd: pointer to csd_data
 */
static void csd_power_up_csd(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;

	dev_dbg(dev, "Powering up CSD\n");

	gpiod_set_value_cansleep(csd->gpio_ctrl, 1);

	csd->csd_ctrl_time = ktime_get();
}

/**
 * csd_power_down_csd() - Lowers control line to CSD
 * @csd: pointer to csd_data
 */
static void csd_power_down_csd(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;

	dev_dbg(dev, "Powering down CSD\n");

	gpiod_set_value_cansleep(csd->gpio_ctrl, 0);

	/*
	 * We assume that the time between disabling of display output and
	 * enabling of display output during suspend and resume excluding
	 * the time the system is actually in suspend is longer than our
	 * reset pulse length. Therefore we can safely use CLOCK_MONOTONIC
	 * without introducing an additional delay after resume.
	 */
	csd->csd_ctrl_time = ktime_get();
}

/**
 * csd_run_errb_test() - Run ERRB test (DEPRECATED)
 * @csd:    pointer to csd_data
 * @passed: location where to store result
 *
 * This is provided for backward compatibility support. To be removed.
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_run_errb_test(struct csd_data *csd, bool *passed)
{
	enum csd_gpio_test_result errb_result;
	int ret;

	/* ERRB signal test */
	ret = csd->drvdata.ser_info->errb_test(csd, &errb_result,
		csd->gpio_errb, gmsl2);

	if (ret)
		return ret;

	if (errb_result == CSD_GPIO_TEST_OK)
		*passed = true;
	else
		*passed = false;

	return 0;
}

/**
 * max96ser_gpio_failure_type() - Human readable failure type string
 * @result: test result
 *
 * Return: string describing failure type
 */
static const char *csd_gpio_failure_type(enum csd_gpio_test_result result)
{
	switch (result) {

	case CSD_GPIO_TEST_OK:
		return "";

	case CSD_GPIO_TEST_LOW:
		return " stuck low";

	case CSD_GPIO_TEST_HIGH:
		return " stuck high";

	default: /* CSD_GPIO_TEST_INV */
		return " inverting";

	}
}

/**
 * csd_run_gpio_test() - Run GPIO test
 * @csd:    pointer to csd_data
 * @buffer: buffer passed along with CSD_EVENT_GPIO_TEST event
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_run_gpio_test(struct csd_data *csd,
	struct csd_event_gpio_test_buffer *buffer)
{
	const struct {
		struct gpio_desc *gpio;
		int (*test)(struct csd_data *csd,
			enum csd_gpio_test_result *result,
			struct gpio_desc *gpio, bool gmsl2);
		char *name;
		bool to_ser;
	} gpio_tests[] = {
		{
		.gpio = csd->gpio_errb,
		.test = csd->drvdata.ser_info->errb_test,
		.name = "ERRB",
		},
		{
		.gpio = csd->gpio_int,
		.test = csd->drvdata.ser_info->int_test,
		.name = "INT",
		},
		{
		.gpio = csd->gpio_lock,
		.test = csd->drvdata.ser_info->lock_test,
		.name = "LOCK",
		},
		{
		.gpio = csd->gpio_gmsl_mode,
		.test = csd->drvdata.ser_info->mode_test,
		.name = "MODE",
		.to_ser = true,
		},
	};
	enum csd_gpio_test_result result[ARRAY_SIZE(gpio_tests)];
	int i;

	/* Run tests */
	for (i = 0; i < ARRAY_SIZE(gpio_tests); i++) {
		int ret;

		if (!gpio_tests[i].gpio || !gpio_tests[i].test)
			continue;

		ret = gpio_tests[i].test(csd, &result[i], gpio_tests[i].gpio,
			gmsl2);

		if (ret)
			return ret;
	}

	/* Print header */
	buffer->written = snprintf(buffer->buf, PAGE_SIZE,
		"test signal     (direction)  [failure type]\n"
		"-------------------------------------------\n");
	if (buffer->written >= PAGE_SIZE)
		return -EOVERFLOW;

	/* Print results */
	for (i = 0; i < ARRAY_SIZE(gpio_tests); i++) {
		int added;

		if (!gpio_tests[i].gpio || !gpio_tests[i].test)
			continue;

		added = snprintf(buffer->buf + buffer->written,
			PAGE_SIZE - buffer->written,
			"%s %-10s %s%s\n",
			(result[i] == CSD_GPIO_TEST_OK) ? "pass" : "fail",
			gpio_tests[i].name,
			gpio_tests[i].to_ser ? "(SOC -> ser)" : "(ser -> SOC)",
			csd_gpio_failure_type(result[i]));

		if (added >= PAGE_SIZE - buffer->written)
			return -EOVERFLOW;

		buffer->written += added;
	}

	return 0;
}

/**
 * csd_state_soff_creset() - Implements CSD state SOFF_CRESET
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_soff_creset(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	struct device *dev = &csd->serdev->dev;
	bool ctrl_test = false;

	for (;;) {
		struct csd_event *event;

		event = csd_get_event(csd, NULL);

		if (ctrl_test && event->type != CSD_EVENT_CTRL_TEST) {

			dev_warn(dev, "Aborting CTRL signal path test\n");

			csd_power_down_csd(csd);

			ctrl_test = false;
		}

		switch (event->type) {
		int ret;

		case CSD_EVENT_UNLOAD:

			csd_event_buffer_unload(csd);

			csd_complete_event(dev, event, 0);

			return CSD_STATE_TERMINATE;

		case CSD_EVENT_INT_IRQ:

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_SUSPEND:

			csd_prepare_suspend(csd);

			csd_complete_event(dev, event, 0);

			return CSD_STATE_SUSPENDED;

		case CSD_EVENT_DISABLE:

			dev_err(dev, "Display disable request received but display is already disabled\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_POST_DISABLE:

			dev_err(dev, "Display post-disable request received but display is already disabled\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_PRE_ENABLE:

			csd_wait_until_csd_in_reset(csd);

			csd_power_up_ser(csd);

			state->hdcp.enabled = false;

			ret = csd_serializer_power_on_pre_enable(csd);

			if (!ret) {
				csd_complete_event(dev, event, 0);

				return CSD_STATE_SCONF_CRESET;
			}

			csd_event_buffer_unload(csd);

			csd_power_down_ser(csd);

			csd_complete_event(dev, event, ret);

			return CSD_STATE_TERMINATE;

		case CSD_EVENT_ENABLE:

			dev_err(dev, "Display enable request received but display is not in pre-enabled state, pre-enable request expected\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_READ_EDID:
		case CSD_EVENT_READ_HDCP:

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_EE_ACCESS:

			dev_err(dev, "Register access is not possible because the serializer is powered down. Enable video output on SOC to power-on serializer\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_DIAG_REPORT:

			dev_err(dev, "Diagnostic reports can't be processed because the CSD is powered down. Enable video output on SOC to turn on CSD\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_LF_READ:

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_ERRB_TEST:

			dev_err(dev, "ERRB signal path test can't be performed because the serializer is currently powered down. Enable video output on SOC to power-on serializer\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_CTRL_TEST:

			if (*((bool *) event->data)) {

				if (!ctrl_test) {
					csd_power_up_csd(csd);
					ctrl_test = true;
				}

			} else {

				if (ctrl_test) {
					csd_power_down_csd(csd);
					ctrl_test = false;
				}
			}

			csd_complete_event(dev, event, 0);

			break;

		case CSD_EVENT_GPIO_TEST:

			csd_wait_until_csd_in_reset(csd);

			csd_power_up_ser(csd);

			state->hdcp.enabled = false;

			ret = csd->drvdata.ser_info->power_on(csd, gmsl2);

			if (!ret)
				ret = csd_run_gpio_test(csd, event->data);

			csd_power_down_ser(csd);

			csd_complete_event(dev, event, ret);

		}

	}
}

/**
 * csd_state_probed_sconf_common_pp() - Common implementation PROBED and SCONF
 * @csd:       pointer to csd_data
 * @state:     state buffer
 * @probed:    true if invoked for PROBED_CRESET state, false otherwise
 * @postponed: queue for postponed events
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_probed_sconf_common_pp(struct csd_data *csd,
	struct csd_state_buffer *state, bool probed,
	struct plist_head *postponed)
{
	struct device *dev = &csd->serdev->dev;
	ktime_t expire = ktime_add_ms(ktime_get(), CSD_SER_IDLE_TIMEOUT);

	for (;;) {
		struct csd_event *event;

		event = csd_get_event_hard_timeout(csd, expire);

		if (!event) {
			/* timeout */

			if (probed) {
				csd_power_down_ser(csd);
				return CSD_STATE_SOFF_CRESET;
			}

			/* reject postponed events */
			if (!plist_head_empty(postponed))
				csd_reject_events(dev, postponed);

			return CSD_STATE_SCONF_CRESET;
		}

		switch (event->type) {
		enum csd_state next_state;
		int ret;

		case CSD_EVENT_UNLOAD:

			csd_event_buffer_unload(csd);

			csd_power_down_ser(csd);

			csd_complete_event(dev, event, 0);

			return CSD_STATE_TERMINATE;

		case CSD_EVENT_INT_IRQ:

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_SUSPEND:

			if (!probed) {
				dev_err(dev, "Received suspend request while video output is pre-enabled\n");

				csd_reject_event(dev, event);

				break;
			}

			csd_power_down_ser(csd);

			csd_prepare_suspend(csd);

			csd_complete_event(dev, event, 0);

			return CSD_STATE_SUSPENDED;

		case CSD_EVENT_DISABLE:

			dev_err(dev, "Display disable request received but display is not enabled\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_POST_DISABLE:

			if (probed) {
				dev_err(dev, "Display post-disable request received but display is already disabled\n");

				csd_reject_event(dev, event);

				break;
			}

			csd_power_down_ser(csd);

			csd_complete_event(dev, event, 0);

			return CSD_STATE_SOFF_CRESET;

		case CSD_EVENT_PRE_ENABLE:

			if (!probed) {
				dev_err(dev, "Display pre-enable request received but display is already pre-enabled\n");

				csd_reject_event(dev, event);

				break;
			}

			ret = csd_serializer_pre_enable(csd);
			if (ret) {
				csd_event_buffer_unload(csd);

				csd_power_down_ser(csd);

				next_state = CSD_STATE_TERMINATE;
			} else {
				next_state = CSD_STATE_SCONF_CRESET;
			}

			csd_complete_event(dev, event, ret);

			return next_state;

		case CSD_EVENT_ENABLE:

			if (probed) {
				dev_err(dev, "Display enable request received but display is not in pre-enabled state, pre-enable request expected\n");

				csd_reject_event(dev, event);

				break;
			}

			csd_wait_until_csd_in_reset(csd);

			if (!csd_serializer_enable(csd)) {
				next_state = CSD_STATE_SRECV_CRESET;
			} else {
				dev_err(dev, "Post video serializer configuration failed\n");
				next_state = CSD_STATE_RESET_SER;
			}

			/*
			 * ACK always, as we run the same recovery sequence
			 * independent if we fail at this point or later.
			 */
			csd_complete_event(dev, event, 0);

			return next_state;

		case CSD_EVENT_READ_EDID:
		case CSD_EVENT_READ_HDCP:

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_EE_ACCESS:

			csd_wait_until_csd_in_reset(csd);

			csd_ee_access(csd, event);

			break;

		case CSD_EVENT_DIAG_REPORT:

			csd_event_postpone(dev, postponed, event);

			break;

		case CSD_EVENT_LF_READ:

			csd_wait_until_csd_in_reset(csd);

			csd_complete_event(dev, event,
				csd->drvdata.ser_info->line_fault(csd,
					event->data));

			break;

		case CSD_EVENT_ERRB_TEST:

			csd_wait_until_csd_in_reset(csd);

			csd_complete_event(dev, event,
				csd_run_errb_test(csd, event->data));

			break;

		case CSD_EVENT_CTRL_TEST:
		case CSD_EVENT_GPIO_TEST:

			if (probed) {
				/*
				 * We don't want to deal with MFG tests
				 * all over the driver. Do it in SOFF_CRESET.
				 */
				csd_event_postpone(dev, postponed, event);

				csd_power_down_ser(csd);

				return CSD_STATE_SOFF_CRESET;
			}

			csd_reject_mfg_event(dev, event);

		}
	}
}

/**
 * csd_state_probed_sconf_common() - Common implementation PROBED and SCONF
 * @csd:    pointer to csd_data
 * @state:  state buffer
 * @probed: true if invoked for PROBED_CRESET state, false otherwise
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_probed_sconf_common(struct csd_data *csd,
	struct csd_state_buffer *state, bool probed)
{
	struct device *dev = &csd->serdev->dev;
	struct plist_head postponed;
	enum csd_state ret;

	plist_head_init(&postponed);

	ret = csd_state_probed_sconf_common_pp(csd, state, probed, &postponed);

	csd_event_restore_postponed(dev, &csd->drvdata.event_buffer,
		&postponed);

	return ret;
}

/**
 * csd_state_probed_creset() - Implements CSD state PROBED_CRESET
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_probed_creset(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	return csd_state_probed_sconf_common(csd, state, true);
}

/**
 * csd_state_sconf_creset() - Implements CSD state SCONF_CRESET
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_sconf_creset(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	return csd_state_probed_sconf_common(csd, state, false);
}

static const struct csd_conv_info conv_info_gmsl1[] = {
	{
		.dev_id = 0x22,
		.name   = "LDVS to HDMI converter board with MAX9276",
	},
	{
		.dev_id = 0x26,
		.name   = "LDVS to HDMI converter board with MAX9280",
	},
};

static const struct csd_conv_info conv_info_gmsl2[] = {
	{
		.dev_id = 0x8C,
		.name   = "LVDS to eDP converter board with MAX96776",
		.setup  = csd_des_max96778_setup,
		.status = csd_des_max96778_status,
	},
	{
		.dev_id = 0x8D,
		.name   = "LVDS to eDP converter board with MAX96778",
		.setup  = csd_des_max96778_setup,
		.status = csd_des_max96778_status,
	},
	{
		.dev_id = 0xAF,
		.name   = "LVDS to eDP converter board with MAX96774",
		.setup  = csd_des_max96778_setup,
		.status = csd_des_max96778_status,
	},
};

/**
 * csd_lookup_conv_info() - Looks up converter board info
 * @dev_id: device ID of deserializer on converter board
 *
 * Return: pointer to matching csd_conv_info struct or NULL
 */
static const struct csd_conv_info *csd_lookup_conv_info(u8 dev_id)
{
	int i;

	if (gmsl2)
		for (i = 0; i < ARRAY_SIZE(conv_info_gmsl2); i++) {
			if (conv_info_gmsl2[i].dev_id == dev_id)
				return &conv_info_gmsl2[i];
		}
	else
		for (i = 0; i < ARRAY_SIZE(conv_info_gmsl1); i++) {
			if (conv_info_gmsl1[i].dev_id == dev_id)
				return &conv_info_gmsl1[i];
		}

	return NULL;
}

/**
 * csd_setup_converter() - Performs converter board setup if required
 * @csd:       pointer to csd_data
 * @conv_info: information about converter board
 * @force:     force reconfiguration independent of status
 * @chip_rev:  chip revision
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_setup_converter(struct csd_data *csd,
	const struct csd_conv_info *conv_info, bool force, u8 chip_rev)
{
	struct device *dev = &csd->serdev->dev;
	int ret;

	if (!conv_info->setup)
		return 0;

	if (!force && conv_info->status) {
		bool configured;

		ret = conv_info->status(csd, &configured);
		if (ret) {
			dev_err(dev, "Converter board status check failed\n");
			return ret;
		}

		if (configured)
			return 0;

		dev_warn(dev, "Repeating converter board configuration sequence\n");
	}

	ret = conv_info->setup(csd, csd_get_display_mode(), chip_rev);
	if (ret)
		dev_err(dev, "Converter board setup failed\n");

	return ret;
}

/**
 * csd_converter_present() - Checks if converter board is attached
 * @csd:   pointer to csd_data
 * @fresh: true if converter was not detected before
 *
 * Return: true if a converter board was detected
 */
static bool csd_converter_present(struct csd_data *csd, bool fresh)
{
	struct device *dev = &csd->serdev->dev;
	const struct csd_conv_info *conv_info;
	struct csd_chip_id chip_id;
	struct csd_chip_id chip_id_check = {
		.id = 0xFF,
		.rev = CSD_MAX_DREG_DEV_REV_MASK(gmsl2),
	};
	int ret;

	ret = csd_read_des_pc(csd, CSD_MAX_DREG_DEV_ID(gmsl2), (u8 *) &chip_id,
		(u8 *) &chip_id_check, sizeof(chip_id));

	if (ret)
		return false;

	conv_info = csd_lookup_conv_info(chip_id.id);
	if (!conv_info) {
		dev_warn(dev,
			"Detected unknown converter board or malfunctioning CSD with deserializer ID 0x%02X\n",
			chip_id.id);

		return false;
	}

	chip_id.rev &= CSD_MAX_DREG_DEV_REV_MASK(gmsl2);

	if (fresh)
		dev_info(dev, "Detected %s (rev. 0x%02hhX)\n", conv_info->name,
			chip_id.rev);

	ret = csd_setup_converter(csd, conv_info, fresh, chip_id.rev);
	if (ret)
		return false;

	return true;
}

/**
 * csd_state_srecv_creset() - Implements CSD state SRECV_CRESET
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_srecv_creset(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	/*
	 * We never want to power up an actual CSD with some, likely
	 * unsupported, custom timing. This may lead to damage on CSD side.
	 */
	if (csd_using_conv_display_mode()) {
		if (!csd_in_ee_mode() && csd_converter_present(csd, true))
			return CSD_STATE_CONVERTER;

		return CSD_STATE_WAIT_CONVERTER;
	}

	csd_mask_int_irq(csd, false);

	csd_power_up_csd(csd);

	state->prog_signature = false;

	return CSD_STATE_CSD_POWERING_ON;
}

/**
 * csd_diag_is_tester() - Checks if a diag event is a tester present message
 * @event: CSD_EVENT_DIAG_REPORT event
 *
 * Only the request part is checked. Presence and size of response buffer
 * as well as timeout value (expire) are not checked.
 *
 * Return: true if event is a tester present message
 */
static bool csd_diag_is_tester(struct csd_event *event)
{
	struct csd_event_diag_rep_buffer *diag_rep = event->data;

	if (diag_rep->req_size != 2)
		return false;

	if (diag_rep->req[0] != CSD_MSG_DIAG_REQ_TESTER)
		return false;

	if (diag_rep->req[1] != CSD_MSG_DIAG_REQ_TESTER_RESP &&
		diag_rep->req[1] != CSD_MSG_DIAG_REQ_TESTER_NO_RESP)

		return false;

	return true;
}

/**
 * csd_wakeup_diag_emulate_prog() - Emulates processing of programming request
 * @dev:   device structure used for printing to kernel log
 * @event: CSD_EVENT_DIAG_REPORT event
 *
 * This function emulates the processing of a programming session request
 * including a positive response from CSD.
 */
static void csd_wakeup_diag_emulate_prog(struct device *dev,
	struct csd_event *event)
{
	struct csd_event_diag_rep_buffer *diag_rep = event->data;

	dev_info(dev, "Emulating programming session request processing\n");

	diag_rep->req_send = diag_rep->req_size;
	diag_rep->expire += jiffies;

	if (diag_rep->resp) {
		diag_rep->resp[0] = CSD_MSG_DIAG_RESP_SESSION;
		diag_rep->resp[1] = CSD_MSG_DIAG_RESP_SESSION_PROG;
		diag_rep->resp[2] = 0x00;
		diag_rep->resp[3] = 0x19;
		diag_rep->resp[4] = 0x01;
		diag_rep->resp[5] = 0xF4;

		diag_rep->resp_expected = CSD_MSG_DIAG_RESP_SESSION_SIZE;
		diag_rep->resp_recv = CSD_MSG_DIAG_RESP_SESSION_SIZE;

	}
}

/**
 * csd_pwr_on_handle_int_irq() - Handles an int irq event during CSD_POWERING_ON
 * @csd:         pointer to csd_data
 * @state:       state buffer
 * @event:       interrupt event
 * @int_dropped: pointer to variable used to track if an interrupt was
 *               already dropped since CSD_POWERING_ON state was entered
 *
 * Return: next state (CSD_STATE_CSD_POWERING_ON to stay in CSD_POWERING_ON)
 */
static enum csd_state csd_pwr_on_handle_int_irq(struct csd_data *csd,
	struct csd_state_buffer *state, struct csd_event *event,
	bool *int_dropped)
{
	struct device *dev = &csd->serdev->dev;
	struct csd_msg_interrupt_status int_status;
	int read_fail;

	read_fail = csd_read_csd(csd, CSD_MSG_INTERRUPT_STATUS_ID,
		(u8 *) &int_status, sizeof(int_status));

	if (!read_fail && (int_status.msg_data_type ==
		CSD_MSG_INTERRUPT_STATUS_REG_PBL)) {

		csd_complete_event(dev, event, 0);

		return CSD_STATE_CSD_PBL;
	}

	/*
	 * In case an interrupt is issued while a diagnostic message (reply
	 * part if present) is processed which is requesting a switch to
	 * programming session the CSD may still report APP or not respond
	 * at all when the interrupt request is finally handled at this point
	 * after completing the processing of the diagnostic message. This
	 * issue may occur for other reset types as well but is critical only
	 * in case of a programming session switch. For other reset types
	 * simply another reset will be done to recover from this situation.
	 */
	if (state->prog_signature && !(*int_dropped)) {
		dev_warn(dev, "Dropping faulty or obsolete interrupt due to ongoing switch to programming session\n");

		*int_dropped = true;

		csd_reject_event(dev, event);

		return CSD_STATE_CSD_POWERING_ON;
	}

	csd_power_down_csd(csd);

	csd_reject_event(dev, event);

	if (read_fail) {
		bool pclk;

		/*
		 * All below error cases can potentially be caused by a
		 * misconfiguration or other malfunction on serializer side.
		 * Therefore we want to always perform a full reset.
		 */

		csd_mask_int_irq(csd, true);

		if (gmsl2) {
			dev_err(dev, "Failed to read interrupt status from CSD in early wakeup phase, resetting CSD and serializer\n");
			return CSD_STATE_RESET_SER;
		}

		/*
		 * In case of GMSL1 this failure might be caused by missing
		 * pixel clock, which is required for GMSL1 link operation.
		 * We want to check for this common problem at this point.
		 */

		csd_wait_until_csd_in_reset(csd);

		if (csd->drvdata.ser_info->check_pclk(csd, &pclk)) {
			dev_err(dev, "Failed to read interrupt status from CSD in early wakeup phase and failed to check for pixel clock on video interface, resetting CSD and serializer\n");
			return CSD_STATE_RESET_SER;
		}

		if (!pclk) {
			dev_err(dev, "Failed to read interrupt status from CSD in early wakeup phase due to missing pixel clock on video interface, resetting CSD and serializer\n");
			return CSD_STATE_RESET_SER;
		}

		dev_err(dev, "Failed to read interrupt status from CSD in early wakeup phase despite pixel clock being active on video interface, resetting CSD and serializer\n");
		return CSD_STATE_RESET_SER;
	}

	switch (int_status.msg_data_type) {
	case CSD_MSG_INTERRUPT_STATUS_DEFAULT:
		dev_err(dev, "CSD reported default interrupt in early wakeup phase, resetting CSD\n");
		break;
	case CSD_MSG_INTERRUPT_STATUS_TOUCH_PANEL:
		dev_err(dev, "CSD reported touch panel interrupt in early wakeup phase, resetting CSD\n");
		break;
	case CSD_MSG_INTERRUPT_STATUS_REG_APP:
		dev_err(dev, "CSD reported APP mode in early wakeup phase but it is expected to be in PBL mode, resetting CSD\n");
		break;
	default:
		dev_err(dev, "CSD reported unknown interrupt in early wakeup phase, resetting CSD\n");
	}

	return CSD_STATE_RESETTING_CSD;

}

/**
 * csd_pbl_app_handle_int_irq() - Handles an int irq event during CSD_PBL_APP_*
 * @csd:   pointer to csd_data
 * @event: interrupt event
 *
 * Return: next state
 */
static enum csd_state csd_pbl_app_handle_int_irq(struct csd_data *csd,
	struct csd_event *event)
{
	struct device *dev = &csd->serdev->dev;
	struct csd_msg_interrupt_status int_status;
	int read_fail;

	read_fail = csd_read_csd(csd, CSD_MSG_INTERRUPT_STATUS_ID,
		(u8 *) &int_status, sizeof(int_status));

	if (!read_fail && (int_status.msg_data_type ==
		CSD_MSG_INTERRUPT_STATUS_REG_APP)) {

		csd_complete_event(dev, event, 0);

		return CSD_STATE_CSD_APP;
	}

	if (read_fail) {
		dev_err(dev, "Failed to read interrupt status from CSD at the end of wakeup sequence, resetting CSD\n");
	} else {
		switch (int_status.msg_data_type) {
		case CSD_MSG_INTERRUPT_STATUS_DEFAULT:
			dev_err(dev, "CSD reported default interrupt at the end of wakeup sequence, expected APP mode register access interrupt, resetting CSD\n");
			break;
		case CSD_MSG_INTERRUPT_STATUS_TOUCH_PANEL:
			dev_err(dev, "CSD reported touch panel interrupt at the end of wakeup sequence, expected APP mode register access interrupt, resetting CSD\n");
			break;
		case CSD_MSG_INTERRUPT_STATUS_REG_PBL:
			dev_err(dev, "CSD reported PBL mode register access interrupt at the end of wakeup sequence, expected APP mode register access interrupt, resetting CSD\n");
			break;
		default:
			dev_err(dev, "CSD reported unknown interrupt at the end of wakeup sequence, expected APP mode register access interrupt, resetting CSD\n");
		}
	}

	csd_power_down_csd(csd);

	csd_reject_event(dev, event);

	return CSD_STATE_RESETTING_CSD;
}

/**
 * csd_cache_line_fault() - Tries to read and cache line fault status
 * @csd:   pointer to csd_data
 * @state: state buffer
 */
static void csd_cache_line_fault(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	struct device *dev = &csd->serdev->dev;
	u8 line_status;
	int ret;

	if (csd_in_ee_mode())
		return;

	ret = csd->drvdata.ser_info->line_fault(csd, &line_status);
	if (ret) {
		dev_err(dev, "Reading line fault status failed\n");
		return;
	}

	state->line_status = line_status;
}

/**
 * csd_get_cached_line_fault() - Provides cached value for line status
 * @state:       state buffer
 * @line_status: location where to store cached line status
 *
 * Return: 0 on success, -EBUSY in case cached status is not available
 */
static int csd_get_cached_line_fault(struct csd_state_buffer *state,
	u8 *line_status)
{
	if (state->line_status == CSD_LINE_STATUS_UNKNOWN)
		return -EBUSY;

	*line_status = state->line_status;

	return 0;
}

/**
 * csd_state_csd_wakeup_pp() - Common state implementation for wakeup substates
 * @csd:       pointer to csd_data
 * @state:     state buffer
 * @substate:  specific substate of wakeup sequence
 * @postponed: queue for postponed events
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_csd_wakeup_pp(struct csd_data *csd,
	struct csd_state_buffer *state, enum csd_wakeup_substate substate,
	struct plist_head *postponed)
{
	struct device *dev = &csd->serdev->dev;
	ktime_t expire = ktime_get();
	bool postponed_diag = false;
	bool int_dropped = false;

	switch (substate) {

	case CSD_WSUBSTATE_CSD_POWERING_ON:

		/*
		 * Add propagation time to be on the safe side,
		 * just in case we have just raised CTRL line.
		 */
		expire = ktime_add_ms(expire, CSD_WAITING_TIME_MS +
			CSD_RESET_PROPAGATION_DELAY);
		break;

	case CSD_WSUBSTATE_CSD_PBL:
	case CSD_WSUBSTATE_CSD_APP:

		expire = ktime_add_ms(expire, CSD_WAITING_TIME_MS);
		break;

	case CSD_WSUBSTATE_CSD_PBL_APP_EARLY:
	case CSD_WSUBSTATE_CSD_PBL_APP_LATE:

		expire = ktime_add_ms(expire, CSD_WAITING_TIME_MS / 2);

	}

	while (true) {
		struct csd_event *event;

		event = csd_get_event_hard_timeout(csd, expire);

		if (!event) {
			/* timeout */

			switch (substate) {

			case CSD_WSUBSTATE_CSD_POWERING_ON:

				csd_power_down_csd(csd);

				if (csd_in_ee_mode()) {
					dev_dbg(dev,
						"Wakeup: Timeout while waiting for rising edge on interrupt line after power on. Resetting only CSD due to ee_mode\n");

					return CSD_STATE_RESETTING_CSD;
				}

				csd_wait_until_csd_in_reset(csd);

				if (csd_converter_present(csd, true)) {
					csd_mask_int_irq(csd, true);

					return CSD_STATE_CONVERTER;
				}

				dev_err(dev,
					"Wakeup: Timeout while waiting for rising edge on interrupt line after power on\n");

				csd_mask_int_irq(csd, true);

				return CSD_STATE_RESET_SER;

			case CSD_WSUBSTATE_CSD_PBL:

				dev_err(dev,
					"Wakeup: Timeout while waiting for falling edge on interrupt line (PBL mode)\n");
				break;

			case CSD_WSUBSTATE_CSD_PBL_APP_EARLY:

				return CSD_STATE_CSD_PBL_APP_LATE;

			case CSD_WSUBSTATE_CSD_PBL_APP_LATE:

				dev_err(dev,
					"Wakeup: Timeout while waiting for rising edge on interrupt line (PBL -> APP transition)\n");
				break;

			case CSD_WSUBSTATE_CSD_APP:

				dev_err(dev,
					"Wakeup: Timeout while waiting for falling edge on interrupt line (APP mode)\n");

			}

			csd_power_down_csd(csd);

			return CSD_STATE_RESETTING_CSD;
		}

		switch (event->type) {

		case CSD_EVENT_UNLOAD:

			csd_event_buffer_unload(csd);

			csd_power_down_csd(csd);

			csd_power_down_ser(csd);

			csd_complete_event(dev, event, 0);

			return CSD_STATE_TERMINATE;

		case CSD_EVENT_INT_IRQ:

			switch (substate) {
			enum csd_state next_state;

			case CSD_WSUBSTATE_CSD_POWERING_ON:

				if (!gpiod_get_value_cansleep(csd->gpio_int)) {
					csd_reject_event(dev, event);
					break;
				}

				next_state = csd_pwr_on_handle_int_irq(csd,
					state, event, &int_dropped);

				if (next_state == CSD_STATE_CSD_POWERING_ON)
					break;

				return next_state;

			case CSD_WSUBSTATE_CSD_PBL:

				csd_complete_event(dev, event, 0);

				if (state->prog_signature)
					return CSD_STATE_CSD_PROG;

				return CSD_STATE_CSD_PBL_APP_EARLY;

			case CSD_WSUBSTATE_CSD_PBL_APP_EARLY:
			case CSD_WSUBSTATE_CSD_PBL_APP_LATE:

				if (!gpiod_get_value_cansleep(csd->gpio_int)) {
					csd_reject_event(dev, event);
					break;
				}

				return csd_pbl_app_handle_int_irq(csd, event);

			case CSD_WSUBSTATE_CSD_APP:

				csd_complete_event(dev, event, 0);

				return CSD_STATE_IDLE;

			}

			break;

		case CSD_EVENT_SUSPEND:

			dev_err(dev, "Received suspend request while video output is enabled\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_DISABLE:

			csd_power_down_csd(csd);

			csd_mask_int_irq(csd, true);

			csd_complete_event(dev, event, 0);

			return CSD_STATE_SCONF_CRESET;

		case CSD_EVENT_POST_DISABLE:

			dev_err(dev, "Received post-disable request while video output is enabled, expected disable request first\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_PRE_ENABLE:

			dev_err(dev, "Received pre-enable request while video output is enabled\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_ENABLE:

			dev_err(dev, "Received enable request while video output is already enabled\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_READ_EDID:
		case CSD_EVENT_READ_HDCP:

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_EE_ACCESS:

			if (!csd_in_ee_mode()) {
				dev_err(dev,
					"Accessing registers while display wakeup sequence is ongoing is allowed only in ee_mode\n");
				csd_reject_event(dev, event);
				break;
			}

			csd_ee_access(csd, event);

			break;

		case CSD_EVENT_DIAG_REPORT:

			if (csd_diag_is_tester(event)) {
				csd_reject_event(dev, event);
				break;
			}

			/*
			 * When a firmware update of CSD is interrupted the
			 * firmware may be broken. In that case CSD will stay
			 * in PBL instead of transitioning automatically to
			 * APP mode. To recover from this situation another
			 * firmware download needs to be performed. User space
			 * sends a programming session request to do this. In
			 * general we do not process such a request at this
			 * point as we have a time window of only 10 ms to do
			 * it at this point. Instead we postpone the request
			 * until wakeup is complete. However in case CSD
			 * firmware is broken it will never complete wakeup
			 * and will hang in CSD_WSUBSTATE_CSD_PBL_APP_* until
			 * the internal CSD timeout triggers. To be able to
			 * recover we detect that we are hanging in this
			 * state for long time and transition to programming
			 * state directly if there is a corresponding request
			 * from user space pending. Communcition with CSD is
			 * not required as CSD is expected to be already in
			 * programming session.
			 */
			if (substate == CSD_WSUBSTATE_CSD_PBL_APP_LATE &&
				!postponed_diag &&
				csd_wakeup_diag_is_valid_prog(dev, event)) {

				csd_wakeup_diag_emulate_prog(dev, event);

				csd_complete_event(dev, event, 0);

				return CSD_STATE_CSD_PROG;
			}

			/*
			 * A diagnostic request can't be processed during
			 * wakeup sequence. We could simply reject the request
			 * but that wouldn't be very user friendly. In
			 * particular when switching to programming session
			 * user space doesn't have any way to determine
			 * when the switch is complete. When we reject the
			 * event at this point user space needs to retry
			 * until we finally reach programming session.
			 * By postponing the event user space can simply
			 * block on any diagnostic command in order to
			 * wait for CSD to enter programming session.
			 */

			csd_event_postpone(dev, postponed, event);

			/*
			 * Once we postpone a diag request we need to postpone
			 * also all subsequent ones to maintain order.
			 */
			postponed_diag = true;

			break;

		case CSD_EVENT_LF_READ:

			if (csd_in_ee_mode()) {
				csd_complete_event(dev, event,
					csd->drvdata.ser_info->line_fault(csd,
						event->data));

				break;
			}

			csd_complete_event(dev, event,
				csd_get_cached_line_fault(state, event->data));

			break;

		case CSD_EVENT_ERRB_TEST:

			if (!csd_in_ee_mode()) {
				dev_err(dev,
					"Performing a test of the ERRB signal path while display wakeup sequence is ongoing is allowed only in ee_mode\n");
				csd_reject_event(dev, event);
				break;
			}

			csd_complete_event(dev, event,
				csd_run_errb_test(csd, event->data));

			break;

		case CSD_EVENT_CTRL_TEST:
		case CSD_EVENT_GPIO_TEST:

			csd_reject_mfg_event(dev, event);

		}

	}
}

/**
 * csd_is_wakeup_substate() - Checks if state is part of wakeup sequence
 * @state: state
 *
 * Return: true iff state is part of the CSD wakeup sequence
 */
static bool csd_is_wakeup_substate(enum csd_state state)
{
	switch (state) {

	case CSD_STATE_CSD_POWERING_ON:
	case CSD_STATE_CSD_PBL:
	case CSD_STATE_CSD_PBL_APP_EARLY:
	case CSD_STATE_CSD_PBL_APP_LATE:
	case CSD_STATE_CSD_APP:
		return true;

	default:
		return false;

	}
}

/**
 * csd_state_csd_wakeup() - Common state implementation for wakeup substates
 * @csd:      pointer to csd_data
 * @state:    state buffer
 * @substate: specific substate of wakeup sequence
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_csd_wakeup(struct csd_data *csd,
	struct csd_state_buffer *state, enum csd_wakeup_substate substate)
{
	struct device *dev = &csd->serdev->dev;
	struct plist_head postponed;
	enum csd_state ret;

	plist_head_init(&postponed);

	ret = csd_state_csd_wakeup_pp(csd, state, substate, &postponed);

	csd_event_restore_postponed(dev, &csd->drvdata.event_buffer,
		&postponed);

	/*
	 * While wakeup sequence is ongoing, we are not allowed to read
	 * any registers from serializer. This might lead to collisions
	 * on the UART, as CSD is accessing deserializer most of the time.
	 * Therefore we can't get line fault status during wakeup sequence.
	 * This is not an issue if everything is fine. In that case the
	 * wakeup sequence doesn't take long and the line fault status is
	 * not very interessting anyway, as everything is working.
	 * It gets interessting, when there are failures. In that case the
	 * wakeup sequence may take many seconds to complete or fail, as it
	 * contains long timeouts. To make it worse, when wakeup fails, we
	 * will retry. We may loop through the wakeup sequence infinitely.
	 * In that case we need to be able to get line fault information,
	 * as a line fault might be the root cause for our failure.
	 * We use an optimistic approach. We don't read the line fault status
	 * initially. This is important to avoid unnecessary delays. The line
	 * fault diagnostics are not working well right after power on and
	 * need a delay. When wakeup sequence fails, we read the status and
	 * cache it to be able to provide it to user during next retry.
	 * Once we leave wakeup sequence, we need to clear our cache,
	 * otherwise we might provide very old information during some
	 * subsequent wakeup sequence, not related to any retries.
	 */
	if (!csd_is_wakeup_substate(ret))
		state->line_status = CSD_LINE_STATUS_UNKNOWN;

	return ret;
}

/**
 * csd_state_csd_powering_on() - Implements CSD state CSD_POWERING_ON
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_csd_powering_on(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	return csd_state_csd_wakeup(csd, state, CSD_WSUBSTATE_CSD_POWERING_ON);
}

/**
 * csd_state_csd_pbl() - Implements CSD state CSD_PBL
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_csd_pbl(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	return csd_state_csd_wakeup(csd, state, CSD_WSUBSTATE_CSD_PBL);
}

/**
 * csd_state_csd_pbl_app_early() - Implements CSD state CSD_PBL_APP_EARLY
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_csd_pbl_app_early(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	return csd_state_csd_wakeup(csd, state,
		CSD_WSUBSTATE_CSD_PBL_APP_EARLY);
}

/**
 * csd_state_csd_pbl_app_late() - Implements CSD state CSD_PBL_APP_LATE
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_csd_pbl_app_late(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	return csd_state_csd_wakeup(csd, state, CSD_WSUBSTATE_CSD_PBL_APP_LATE);
}

/**
 * csd_get_and_expose_display_status() - Updates display status information
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * This function reads display status information from CSD. In case the read
 * was successful information are exposed to drvdata to be available
 * for sysfs callbacks.
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_get_and_expose_display_status(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	struct device *dev = &csd->serdev->dev;
	struct csd_msg_display_status *status;
	int unexposed;
	int ret;

	unexposed = (state->exposed_status + 1) % 2;
	status = &state->display_status[unexposed];

	ret = csd_read_csd(csd, CSD_MSG_DISPLAY_STATUS_ID, (u8 *) status,
		sizeof(state->display_status[0]));
	if (ret) {
		state->transmission_fails++;
		return ret;
	}

	state->transmission_fails = 0;

	if (status->mode != CSD_MSG_DISPLAY_STATUS_MODE_NORMAL ||
		status->display_on != CSD_MSG_DISPLAY_STATUS_DISPLAY_ON_ON) {

		/*
		 * It is expected to get a few mode fails during start-up
		 * as we query display status before turning on CSD and
		 * even after turning it on a few additional fails are
		 * expected as the CSD needs some time to actually turn on.
		 */
		state->mode_fails++;

	} else {
		if (state->mode_fails)
			dev_dbg(dev, "Have seen %d CSD mode fails\n",
				state->mode_fails);

		state->mode_fails = 0;
	}

	/* flip status buffers */
	mutex_lock(&csd->drvdata.display_status.lock);

	state->exposed_status = unexposed;
	csd->drvdata.display_status.status = status;

	mutex_unlock(&csd->drvdata.display_status.lock);

	return 0;
}

/**
 * csd_unexpose_display_status() - Clear display status information in drvdata
 * @csd: pointer to csd_data
 */
static void csd_unexpose_display_status(struct csd_data *csd)
{
	mutex_lock(&csd->drvdata.display_status.lock);

	csd->drvdata.display_status.status = NULL;

	mutex_unlock(&csd->drvdata.display_status.lock);
}

/**
 * csd_register_diag_sysfs() - Creates diagnostic sysfs entries
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_register_diag_sysfs(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(diag_sysfs_entries); i++) {
		int ret;
		int j;

		ret = device_create_file(csd->csd_dev, diag_sysfs_entries[i]);
		if (ret) {
			dev_err(dev, "Failed to register diag sysfs entries\n");

			for (j = i - 1; j >= 0; j--)
				device_remove_file(csd->csd_dev,
					diag_sysfs_entries[j]);
			return ret;
		}
	}

	return 0;
}

/**
 * csd_unregister_diag_sysfs() - Removes diagnostic sysfs entries
 * @csd: pointer to csd_data
 */
static void csd_unregister_diag_sysfs(struct csd_data *csd)
{
	int i;

	for (i = ARRAY_SIZE(diag_sysfs_entries) - 1; i >= 0; i--)
		device_remove_file(csd->csd_dev, diag_sysfs_entries[i]);
}

/**
 * csd_send_ihu_request() - Sends an IHU request to CSD
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * This requests the CSD to go to normal mode (in contrast to sleep mode)
 * and sets the currently requested brightness value.
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_send_ihu_request(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	struct csd_msg_ihu_request request = {
		.lvds_link_parameter = 0,
		.mode_control = CSD_MSG_IHU_REQUEST_MODE_NORMAL,
		.reserve0 = 0,
		.touch_report_mode = CSD_MSG_IHU_REQUEST_REPORT_MODE_CYCLIC,
		.touch_report_rate = CSD_MSG_IHU_REQUEST_REPORT_RATE_100HZ,
		.reserve1 = 0,
		.reserve2 = 0,
	};
	int ret;

	mutex_lock(&csd->drvdata.brightness.lock);
	request.brightness = cpu_to_be16(csd->drvdata.brightness.brightness);
	mutex_unlock(&csd->drvdata.brightness.lock);

	ret = csd_write_csd(csd, CSD_MSG_IHU_REQUEST_ID, (u8 *) &request,
		sizeof(request));
	if (ret) {
		state->transmission_fails++;
		return ret;
	}

	state->transmission_fails = 0;
	return 0;
}

/**
 * csd_state_csd_app() - Implements CSD state CSD_APP
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_csd_app(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	struct device *dev = &csd->serdev->dev;
	enum csd_state ret_state;
	int ret;

	ret_state = csd_state_csd_wakeup(csd, state, CSD_WSUBSTATE_CSD_APP);
	if (ret_state != CSD_STATE_IDLE)
		return ret_state;

	state->transmission_fails = 0;

	/*
	 * Read Display Status Information
	 * 1. To force a resynchronization of our receiver state, just in
	 *    case there were some transmission errors and we lost sync.
	 * 2. To get initial display status values.
	 */
	while (true) {
		ret = csd_get_and_expose_display_status(csd, state);
		if (!ret)
			break;

		if (state->transmission_fails > CSD_MAX_TRANSMISSION_FAILS) {
			dev_err(dev, "Failed to read initial display status\n");
			goto cleanup_wakeup;
		}
	}

	/* Perform post wakeup configuration of serializer and deserializer */
	if (csd_post_wakeup_config(csd, state))
		goto cleanup_expose;

	/* Send first IHU Request to tell the CSD to turn on. */
	state->periodic.last_time = ktime_get();

	csd_send_ihu_request(csd, state);

	state->periodic.state = CSD_PSTATE_IDLE_IHU_REQ;

	state->mode_fails = 0;

	state->hdcp.last_soc = ktime_get();

	return CSD_STATE_IDLE;

cleanup_expose:
	csd_unexpose_display_status(csd);

cleanup_wakeup:
	csd_power_down_csd(csd);

	csd_mask_int_irq(csd, true);

	return CSD_STATE_RESET_SER;
}

/**
 * csd_get_time_of_next_periodic() - Expected time of next periodic event
 * @periodic: periodic state buffer
 *
 * This function determines based on the state stored in the periodic state
 * buffer the absolute time when the next periodic message should be (or
 * potentially should have been) send to CSD.
 *
 * Return: time of next periodic event
 */
static ktime_t csd_get_time_of_next_periodic(
	struct csd_periodic_buffer *periodic)
{
	ktime_t expire = periodic->last_time;

	switch (periodic->state) {
	case CSD_PSTATE_IDLE_IHU_REQ:
	case CSD_PSTATE_IDLE_STATUS:
	case CSD_PSTATE_IDLE_TOUCH:
		expire = ktime_add_ms(expire, 20);
		break;
	case CSD_PSTATE_ACTIV_IHU_REQ:
	case CSD_PSTATE_ACTIV_STATUS:
		expire = ktime_add_us(expire, 4500);
		break;
	case CSD_PSTATE_ACTIV_TOUCH1:
	case CSD_PSTATE_ACTIV_TOUCH5:
		expire = ktime_add_ms(expire, 10);
		break;
	case CSD_PSTATE_ACTIV_TOUCH3:
	case CSD_PSTATE_ACTIV_TOUCH7:
		expire = ktime_add_us(expire, 5500);
	}

	return expire;
}

/**
 * csd_send_shutdown_request() - Sends a shutdown request to CSD
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_send_shutdown_request(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;
	struct csd_msg_shutdown_request request = {
		.shutdown_request = CSD_MSG_SHUTDOWN_REQUEST_SHUTDOWN,
	};
	int ret;

	ret = csd_write_csd(csd, CSD_MSG_SHUTDOWN_REQUEST_ID, (u8 *) &request,
		sizeof(request));

	if (ret)
		dev_warn(dev, "CSD shutdown request failed\n");

	return ret;
}

/**
 * csd_verify_touch_report() - Verifies integrity of received touch report
 * @dev:   device structure used for printing to kernel log
 * @touch: touch report to verify
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_verify_touch_report(struct device *dev,
	struct csd_msg_touch_status *touch)
{
	u16 max_x, max_y;
	unsigned int dpm;
	u8 i;

	if (gmsl2) {
		max_x = CSD_DISPLAY_WIDTH_GMSL2 - 1;
		max_y = CSD_DISPLAY_HEIGHT_GMSL2 - 1;
		dpm = CSD_DISPLAY_DPM_GMSL2;
	} else {
		max_x = CSD_DISPLAY_WIDTH_GMSL1 - 1;
		max_y = CSD_DISPLAY_HEIGHT_GMSL1 - 1;
		dpm = CSD_DISPLAY_DPM_GMSL1;
	}

	if (be16_to_cpu(touch->screen_width) != max_x + 1) {
		dev_dbg(dev, "Touch: Invalid screen width\n");
		return -EINVAL;
	}

	if (be16_to_cpu(touch->screen_height) != max_y + 1) {
		dev_dbg(dev, "Touch: Invalid screen height\n");
		return -EINVAL;
	}

	if (touch->number_of_reports > CSD_MSG_TOUCH_STATUS_MAX_CNT) {
		dev_dbg(dev, "Touch: Invalid number of reports\n");
		return -EINVAL;
	}

	for (i = 0; i < touch->number_of_reports; i++) {
		if (be16_to_cpu(touch->report[i].x) > max_x) {
			dev_dbg(dev, "Touch: Invalid x coordinate\n");
			return -EINVAL;
		}

		if (be16_to_cpu(touch->report[i].y) > max_y) {
			dev_dbg(dev, "Touch: Invalid y coordinate\n");
			return -EINVAL;
		}

		if (CSD_MSG_TOUCH_REPORT_CONTACT_STATUS(
			touch->report[i].state_id) == 0x03) {
			dev_dbg(dev, "Touch: Invalid contact status\n");
			return -EINVAL;
		}

		if (((unsigned int) touch->report[i].width) * dpm / 1000 >
			max_x + 1) {
			dev_dbg(dev, "Touch: Width larger than display\n");
			return -EINVAL;
		}

		if (((unsigned int) touch->report[i].height) * dpm / 1000 >
			max_y + 1) {
			dev_dbg(dev, "Touch: Height larger than display\n");
			return -EINVAL;
		}

		if (touch->report[i].probability > 100) {
			dev_dbg(dev, "Touch: Invalid probability\n");
			return -EINVAL;
		}
	}

	return 0;
}

/**
 * csd_propagate_touch_input() - Propagates touch event to input subsystem
 * @csd:   pointer to csd_data
 * @state: state buffer
 * @touch: touch event
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_propagate_touch_input(struct csd_data *csd,
	struct csd_state_buffer *state, struct csd_msg_touch_status *touch)
{
	struct device *dev = &csd->serdev->dev;
	struct input_dev *input_dev = csd->input_dev;
	bool sync_required = false;
	int dpm;
	u8 i;

	/* Print touch reports */
	for (i = 0; i < touch->number_of_reports; i++) {
		struct csd_msg_touch_report *report = &touch->report[i];
		char *status;

		switch (CSD_MSG_TOUCH_REPORT_CONTACT_STATUS(report->state_id)) {
		case CSD_MSG_TOUCH_REPORT_CONTACT_STATUS_DOWN:
			status = "DOWN";
			break;
		case CSD_MSG_TOUCH_REPORT_CONTACT_STATUS_MOVE:
			status = "MOVE";
			break;
		case CSD_MSG_TOUCH_REPORT_CONTACT_STATUS_UP:
			status = "UP";
			break;
		default:
			status = "FAIL";
		}

		dev_dbg(dev, "Touch %hhu: %4s ID: %2hhu, x: %4hu, y: %4hu, width: %3hhu mm, height: %3hhu mm, %3hhu %%\n",
			i,
			status,
			CSD_MSG_TOUCH_REPORT_CONTACT_ID(report->state_id),
			be16_to_cpu(report->x),
			be16_to_cpu(report->y),
			report->width,
			report->height,
			report->probability);
	}

	/*
	 * This is not really mandatory but guaranteed by specification.
	 * Using it makes our life easier.
	 */
	if (touch->number_of_reports != CSD_MSG_TOUCH_STATUS_MAX_CNT) {
		dev_err_once(dev,
			"Touch: Unexpected number of touch reports\n");
		return -EINVAL;
	}

	if (gmsl2)
		dpm = CSD_DISPLAY_DPM_GMSL2;
	else
		dpm = CSD_DISPLAY_DPM_GMSL1;

	for (i = 0; i < touch->number_of_reports; i++) {
		struct csd_msg_touch_report *report = &touch->report[i];

		if (CSD_MSG_TOUCH_REPORT_CONTACT_ID(report->state_id) != i + 1)
			dev_warn_once(dev, "Touch: Unexpected tracking ID\n");

		if (CSD_MSG_TOUCH_REPORT_CONTACT_STATUS(report->state_id) ==
			CSD_MSG_TOUCH_REPORT_CONTACT_STATUS_UP) {

			if (!state->touch_down[i])
				continue;

			input_mt_slot(input_dev, i);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER,
				false);
			sync_required = true;

			state->touch_down[i] = false;
			continue;
		}

		input_mt_slot(input_dev, i);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);

		input_report_abs(input_dev, ABS_MT_POSITION_X,
			be16_to_cpu(report->x));

		input_report_abs(input_dev, ABS_MT_POSITION_Y,
			be16_to_cpu(report->y));

		/*
		 * CSD 3.5 (GMSL2) doesn't report the width and height of touch
		 * events along x and y axis as defined in specification.
		 * Instead it reports the length along the major and minor axis
		 * of the touch event. That would be great, if it would report
		 * also information about the axis, but that is not forseen in
		 * the protocol. Without information about the axis the
		 * information are of little use.
		 * To get at least a rough idea of the size of the touch event
		 * we drop the length along major axis or in general the larger
		 * of both reported values and provide the smaller value as
		 * width and height.
		 */

		if (report->width > report->height) {
			/* horizontal touch */
			if (gmsl2)
				input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,
					((int) report->height) * dpm / 1000);
			else
				input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,
					((int) report->width) * dpm / 1000);

			input_report_abs(input_dev, ABS_MT_TOUCH_MINOR,
				((int) report->height) * dpm / 1000);

			input_report_abs(input_dev, ABS_MT_ORIENTATION, 1);
		} else {
			/* vertical touch */
			if (gmsl2)
				input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,
					((int) report->width) * dpm / 1000);
			else
				input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,
					((int) report->height) * dpm / 1000);

			input_report_abs(input_dev, ABS_MT_TOUCH_MINOR,
				((int) report->width) * dpm / 1000);

			input_report_abs(input_dev, ABS_MT_ORIENTATION, 0);
		}

		sync_required = true;

		state->touch_down[i] = true;
	}

	if (sync_required) {
		input_mt_report_pointer_emulation(input_dev, false);
		input_sync(input_dev);
	}

	return 0;
}

/**
 * csd_release_touch() - Releases all touch points
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Informs the input subsystem that all touch points were released.
 */
static void csd_release_touch(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	struct device *dev = &csd->serdev->dev;
	struct input_dev *input_dev = csd->input_dev;
	bool sync_required = false;
	u8 i;

	for (i = 0; i < CSD_MSG_TOUCH_STATUS_MAX_CNT; i++) {

		if (state->touch_down[i]) {
			input_mt_slot(input_dev, i);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER,
				false);
			sync_required = true;

			state->touch_down[i] = false;
		}

	}

	if (sync_required) {
		dev_dbg(dev, "Releasing touch press\n");
		input_mt_report_pointer_emulation(input_dev, false);
		input_sync(input_dev);
	}
}

/**
 * csd_get_touch_input() - Reads touch input from CSD
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * This function reads touch input from CSD and propagates it to input
 * subsystem.
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_get_touch_input(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	struct device *dev = &csd->serdev->dev;
	struct csd_msg_touch_status touch;
	int ret;

	ret = csd_read_csd(csd, CSD_MSG_TOUCH_STATUS_ID, (u8 *) &touch,
		sizeof(touch));
	if (ret) {
		state->transmission_fails++;
		return ret;
	}

	state->transmission_fails = 0;

	ret = csd_verify_touch_report(dev, &touch);
	if (ret) {
		dev_warn_ratelimited(dev, "Invalid touch report received\n");
		return ret;
	}

	return csd_propagate_touch_input(csd, state, &touch);
}

/**
 * csd_send_periodic_message() - Sends a periodic message to CSD
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Depending on the state one of the periodic messages IHU Request, Display
 * Status Information or Touch Status Information is send to CSD. The response
 * (if any) is processed accordingly.
 *
 * Return: 0
 */
static int csd_send_periodic_message(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	state->periodic.last_time = ktime_get();

	switch (state->periodic.state) {
	case CSD_PSTATE_IDLE_IHU_REQ:
	case CSD_PSTATE_ACTIV_TOUCH3:

		csd_get_and_expose_display_status(csd, state);

		break;

	case CSD_PSTATE_IDLE_STATUS:
	case CSD_PSTATE_ACTIV_IHU_REQ:
	case CSD_PSTATE_ACTIV_TOUCH1:
	case CSD_PSTATE_ACTIV_STATUS:
	case CSD_PSTATE_ACTIV_TOUCH5:

		csd_get_touch_input(csd, state);

		break;

	case CSD_PSTATE_IDLE_TOUCH:
	case CSD_PSTATE_ACTIV_TOUCH7:

		csd_send_ihu_request(csd, state);

	}

	switch (state->periodic.state) {
	case CSD_PSTATE_IDLE_IHU_REQ:
		state->periodic.state = CSD_PSTATE_IDLE_STATUS;
		break;
	case CSD_PSTATE_IDLE_STATUS:
		state->periodic.state = CSD_PSTATE_IDLE_TOUCH;
		break;
	case CSD_PSTATE_IDLE_TOUCH:
		state->periodic.state = CSD_PSTATE_IDLE_IHU_REQ;
		break;
	case CSD_PSTATE_ACTIV_IHU_REQ:
		state->periodic.state = CSD_PSTATE_ACTIV_TOUCH1;
		break;
	case CSD_PSTATE_ACTIV_TOUCH1:
		state->periodic.state = CSD_PSTATE_ACTIV_TOUCH3;
		break;
	case CSD_PSTATE_ACTIV_TOUCH3:
		state->periodic.state = CSD_PSTATE_ACTIV_STATUS;
		break;
	case CSD_PSTATE_ACTIV_STATUS:
		state->periodic.state = CSD_PSTATE_ACTIV_TOUCH5;
		break;
	case CSD_PSTATE_ACTIV_TOUCH5:
		state->periodic.state = CSD_PSTATE_ACTIV_TOUCH7;
		break;
	case CSD_PSTATE_ACTIV_TOUCH7:
		state->periodic.state = CSD_PSTATE_ACTIV_IHU_REQ;
	}

	return 0;
}

/**
 * csd_touch_active() - Indicates if touch input is currently active
 * @state: state buffer
 *
 * The CSD indicates a start of touch input by raising the interrupt line
 * and by providing corresponding interrupt status information. An end of
 * touch input is signaled by deasserting the interrupt line. During the
 * active time touch input needs to be polled with a higher frequency
 * to improve user experience while saving resources during inactive time.
 * This function indicates if touch input is currently active based on
 * the state tracked in the periodic state buffer.
 *
 * Return: true if touch is active
 */
static bool csd_touch_active(struct csd_state_buffer *state)
{
	switch (state->periodic.state) {

	case CSD_PSTATE_IDLE_IHU_REQ:
	case CSD_PSTATE_IDLE_STATUS:
	case CSD_PSTATE_IDLE_TOUCH:
		return false;

	default:
		return true;

	}
}

/**
 * csd_handle_start_of_touch() - Handles start of touch input
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * See also csd_touch_active().
 *
 * Return: 0
 */
static int csd_handle_start_of_touch(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	struct device *dev = &csd->serdev->dev;

	dev_dbg(dev, "Start of touch input\n");

	csd_get_touch_input(csd, state);

	state->periodic.last_time = ktime_get();

	switch (state->periodic.state) {

	case CSD_PSTATE_ACTIV_IHU_REQ:
	case CSD_PSTATE_ACTIV_TOUCH1:
	case CSD_PSTATE_ACTIV_TOUCH3:
	case CSD_PSTATE_ACTIV_STATUS:
	case CSD_PSTATE_ACTIV_TOUCH5:
	case CSD_PSTATE_ACTIV_TOUCH7:
		dev_dbg(dev, "Start of touch during active state detected\n");
		/* fall through */
	case CSD_PSTATE_IDLE_IHU_REQ:

		csd_get_and_expose_display_status(csd, state);

		state->periodic.state = CSD_PSTATE_ACTIV_STATUS;

		break;

	case CSD_PSTATE_IDLE_STATUS:
	case CSD_PSTATE_IDLE_TOUCH:

		csd_send_ihu_request(csd, state);

		state->periodic.state = CSD_PSTATE_ACTIV_IHU_REQ;

		break;

	}

	return 0;
}

/**
 * csd_handle_end_of_touch() - Handles end of touch input
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * See also csd_touch_active().
 *
 * Return: 0
 */
static int csd_handle_end_of_touch(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	struct device *dev = &csd->serdev->dev;

	dev_dbg(dev, "End of touch input\n");

	state->periodic.last_time = ktime_get();

	csd_get_touch_input(csd, state);

	switch (state->periodic.state) {
	case CSD_PSTATE_IDLE_IHU_REQ:
	case CSD_PSTATE_IDLE_STATUS:
	case CSD_PSTATE_IDLE_TOUCH:
		dev_dbg(dev, "End of touch during touch idle state detected\n");
		/* fall through */
	case CSD_PSTATE_ACTIV_IHU_REQ:
	case CSD_PSTATE_ACTIV_TOUCH1:
	case CSD_PSTATE_ACTIV_TOUCH3:
		state->periodic.state = CSD_PSTATE_IDLE_IHU_REQ;
		break;
	case CSD_PSTATE_ACTIV_STATUS:
	case CSD_PSTATE_ACTIV_TOUCH5:
	case CSD_PSTATE_ACTIV_TOUCH7:
		state->periodic.state = CSD_PSTATE_IDLE_STATUS;
		break;
	}

	return 0;
}

/**
 * csd_handle_int_irq() - Handles an interrupt event during IDLE state
 * @csd:   pointer to csd_data
 * @state: state buffer
 * @event: interrupt event
 *
 * Return: next state
 */
static enum csd_state csd_handle_int_irq(struct csd_data *csd,
	struct csd_state_buffer *state, struct csd_event *event)
{
	struct device *dev = &csd->serdev->dev;
	struct csd_msg_interrupt_status status;
	int ret;

	if (csd_touch_active(state)) {
		/*
		 * CSD indicated touch input before, this interrupt event is
		 * supposed to be a falling edge indicating the end of input.
		 */
		csd_handle_end_of_touch(csd, state);

		csd_complete_event(dev, event, 0);

		return CSD_STATE_IDLE;
	}

	while (true) {
		/* We expect interrupt line to be high */
		if (!gpiod_get_value_cansleep(csd->gpio_int)) {

			dev_dbg(dev, "Interrupt line is low\n");

			csd_reject_event(dev, event);

			return CSD_STATE_IDLE;
		}

		ret = csd_read_csd(csd, CSD_MSG_INTERRUPT_STATUS_ID,
			(u8 *) &status, sizeof(status));

		if (!ret) {
			state->transmission_fails = 0;
			break;
		}

		/*
		 * If the read failed we are in trouble.
		 * We don't know at which point the transmission failed and
		 * we don't know what triggered the interrupt. If we are
		 * unlucky it was a CSD_MSG_INTERRUPT_STATUS_REG_APP and we
		 * lost just the reply from CSD. In that case the CSD will
		 * assume that it can freely access the registers of the
		 * deserializer. Therefore we can not perform a retry as this
		 * might cause collisions on the bus. We can also not just wait
		 * for the interrupt line to go low as it might have been a
		 * CSD_MSG_INTERRUPT_STATUS_TOUCH_PANEL in which case the
		 * interrupt line might be high for long time. The safest
		 * recovery method would be to perform a restart of CSD which
		 * is however not acceptable during normal operation as this
		 * would be very disturbing for the user. The probability
		 * that it was actually a CSD_MSG_INTERRUPT_STATUS_REG_APP
		 * is very low as this event typically occurs only once
		 * during start-up. It may occur at any point in time during
		 * normal operation but it is unusual. Therefore the
		 * probability is very high that we actually lost a
		 * CSD_MSG_INTERRUPT_STATUS_TOUCH_PANEL. Let's give the CSD
		 * some time just in case it was actually a STATUS_REG_APP
		 * and the CSD is performing register access and check the
		 * interrupt line again. In case it is still high retry.
		 * Otherwise forget about it. It was either a register access
		 * which was completed meanwhile or it was a short touch input
		 * which is over now, so we can not do anything anymore.
		 */
		dev_warn(dev, "Reading interrupt status failed\n");

		if (++state->transmission_fails > CSD_MAX_TRANSMISSION_FAILS) {
			/* Reset will be performed */
			csd_reject_event(dev, event);
			return CSD_STATE_IDLE;
		}

		csd_msleep(20);
	}

	switch (status.msg_data_type) {

	case CSD_MSG_INTERRUPT_STATUS_DEFAULT:

		dev_warn(dev, "CSD reported default interrupt\n");
		csd_reject_event(dev, event);
		break;

	case CSD_MSG_INTERRUPT_STATUS_TOUCH_PANEL:

		/*
		 * CSD is allowed to take up to 4 ms to complete interrupt
		 * handling internally. It is not allowed to send a
		 * Touch Status Information request earlier than 4 ms
		 * after the start of the Interrupt Status Information request.
		 * As sending the Interrupt Status Information request and
		 * receiving the response takes below 1 ms we sleep for 4 ms
		 * to be on the safe side. If a Touch Status Information
		 * request is send too early CSD may internally fail to
		 * complete the interrupt handling and may start a retry.
		 */
		csd_msleep(4);

		csd_handle_start_of_touch(csd, state);
		csd_complete_event(dev, event, 0);
		break;

	case CSD_MSG_INTERRUPT_STATUS_REG_APP:

		dev_dbg(dev, "CSD is going to perform register access\n");
		csd_complete_event(dev, event, 0);
		return CSD_STATE_CSD_REG_PROG;

	case CSD_MSG_INTERRUPT_STATUS_REG_PBL:

		dev_err(dev, "CSD reported PBL mode during normal operation\n");

		csd_release_touch(csd, state);

		csd_unexpose_display_status(csd);

		csd_power_down_csd(csd);

		csd_reject_event(dev, event);

		return CSD_STATE_RESETTING_CSD;

	default:

		dev_warn(dev, "CSD reported unknown interrupt type\n");
		csd_reject_event(dev, event);

	}

	return CSD_STATE_IDLE;
}

/**
 * csd_prog_handle_int_irq() - Handles an interrupt event during PROG session
 * @csd:   pointer to csd_data
 * @state: state buffer
 * @event: interrupt event
 *
 * Return: next state
 */
static enum csd_state csd_prog_handle_int_irq(struct csd_data *csd,
	struct csd_state_buffer *state, struct csd_event *event)
{
	struct device *dev = &csd->serdev->dev;
	struct csd_msg_interrupt_status status;
	int retry = 0;
	int ret;

	while (true) {
		/* We expect interrupt line to be high */
		if (!gpiod_get_value_cansleep(csd->gpio_int)) {

			dev_dbg(dev, "Interrupt line is low\n");

			csd_reject_event(dev, event);

			return CSD_STATE_CSD_PROG;
		}

		ret = csd_read_csd(csd, CSD_MSG_INTERRUPT_STATUS_ID,
			(u8 *) &status, sizeof(status));

		if (!ret)
			break;

		dev_warn(dev, "Reading interrupt status failed\n");

		if (++retry > CSD_PROG_INT_RETRIES) {

			csd_power_down_csd(csd);

			csd_reject_event(dev, event);

			return CSD_STATE_RESETTING_CSD;
		}

		csd_msleep(20);
	}

	switch (status.msg_data_type) {

	case CSD_MSG_INTERRUPT_STATUS_DEFAULT:

		dev_warn(dev, "CSD reported default interrupt\n");
		csd_reject_event(dev, event);
		break;

	case CSD_MSG_INTERRUPT_STATUS_TOUCH_PANEL:

		dev_warn(dev, "CSD reported touch in programming session\n");
		csd_reject_event(dev, event);
		break;

	case CSD_MSG_INTERRUPT_STATUS_REG_APP:

		dev_err(dev, "CSD reported APP mode in programming session\n");

		csd_power_down_csd(csd);

		csd_reject_event(dev, event);

		return CSD_STATE_RESETTING_CSD;

	case CSD_MSG_INTERRUPT_STATUS_REG_PBL:

		dev_dbg(dev, "CSD is going to perform register access\n");
		csd_complete_event(dev, event, 0);
		return CSD_STATE_CSD_PROG_REG_PROG;

	default:

		dev_warn(dev, "CSD reported unknown interrupt type\n");
		csd_reject_event(dev, event);

	}

	return CSD_STATE_CSD_PROG;
}

/**
 * csd_send_diag_request() - Sends a diagnostic request packet to CSD
 * @csd:      pointer to csd_data
 * @diag_rep: diagnostic report buffer
 *
 * A diagnostic request packet is send. Depending on the state tracked in the
 * diagnostic report buffer this can be either a first frame, a single frame
 * or a consecutive frame of a multi-frame diagnostic message. ACK received
 * from CSD is checked.
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_send_diag_request(struct csd_data *csd,
	struct csd_event_diag_rep_buffer *diag_rep)
{
	if (!diag_rep->req_send) {
		struct csd_msg_diag_first msg;
		u16 current_size;
		int ret;

		if (diag_rep->req_size <= CSD_MSG_DIAG_FIRST_PAYLOAD_SIZE) {

			msg.byte1 = CSD_MSG_DIAG_FRAME_TYPE_SINGLE << 4;

			current_size = diag_rep->req_size;

			if (current_size != CSD_MSG_DIAG_FIRST_PAYLOAD_SIZE)
				memset(msg.data + current_size,
					CSD_MSG_DIAG_PADDING_BYTE,
					CSD_MSG_DIAG_FIRST_PAYLOAD_SIZE -
					current_size);

		} else {

			msg.byte1 = CSD_MSG_DIAG_FRAME_TYPE_FIRST << 4;
			msg.byte1 |= (u8) (diag_rep->req_size >> 8);

			current_size = CSD_MSG_DIAG_FIRST_PAYLOAD_SIZE;

		}

		msg.byte2 = (u8) diag_rep->req_size;

		memcpy(msg.data, diag_rep->req, current_size);

		ret = csd_write_csd(csd, CSD_MSG_DIAG_REQUEST_ID, (u8 *) &msg,
			sizeof(msg));

		if (ret)
			return ret;

		diag_rep->req_send = current_size;
	} else {
		struct csd_msg_diag_cons msg;
		u16 current_size;
		u8 sn;
		int ret;

		sn = ((diag_rep->req_send - CSD_MSG_DIAG_FIRST_PAYLOAD_SIZE) /
			CSD_MSG_DIAG_CONS_PAYLOAD_SIZE + 1) & 0x0F;

		msg.byte1 = CSD_MSG_DIAG_FRAME_TYPE_CONS << 4;
		msg.byte1 |= sn;

		current_size = diag_rep->req_size - diag_rep->req_send;
		if (current_size > CSD_MSG_DIAG_CONS_PAYLOAD_SIZE)
			current_size = CSD_MSG_DIAG_CONS_PAYLOAD_SIZE;

		if (current_size != CSD_MSG_DIAG_CONS_PAYLOAD_SIZE)
			memset(msg.data + current_size,
				CSD_MSG_DIAG_PADDING_BYTE,
				CSD_MSG_DIAG_CONS_PAYLOAD_SIZE -
				current_size);

		memcpy(msg.data, diag_rep->req + diag_rep->req_send,
			current_size);

		ret = csd_write_csd(csd, CSD_MSG_DIAG_REQUEST_ID, (u8 *) &msg,
			sizeof(msg));

		if (ret)
			return ret;

		diag_rep->req_send += current_size;
	}

	return 0;
}

/**
 * csd_get_diag_response() - Reads a diagnostic response from CSD
 * @csd:      pointer to csd_data
 * @diag_rep: diagnostic report buffer
 *
 * Sends a response request to CSD and receives the response packet storing
 * the content to the diagnostic report buffer. Depending on the state tracked
 * in the diagnostic report buffer either a first or single frame or a
 * consecutive frame is accepted.
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_get_diag_response(struct csd_data *csd,
	struct csd_event_diag_rep_buffer *diag_rep)
{
	struct device *dev = &csd->serdev->dev;

	if (!diag_rep->resp_recv) {
		struct csd_msg_diag_first msg;
		u16 expected;
		int ret;

		ret = csd_read_csd(csd, CSD_MSG_DIAG_RESPONSE_ID, (u8 *) &msg,
			sizeof(msg));

		if (ret)
			return ret;

		switch (msg.byte1 >> 4) {

		case CSD_MSG_DIAG_FRAME_TYPE_SINGLE:

			if (msg.byte1 & 0x0F) {
				dev_err_ratelimited(dev,
					"Invalid size for single frame message\n");
				return -EPROTO;
			}

			expected = msg.byte2;

			if (!expected) {
				dev_err_ratelimited(dev,
					"Single frame message with zero size\n");
				return -EPROTO;
			}

			if (expected > CSD_MSG_DIAG_FIRST_PAYLOAD_SIZE) {
				dev_err_ratelimited(dev,
					"Size too large for single frame message\n");
				return -EPROTO;
			}

			if (expected == 3 &&
				msg.data[0] == CSD_MSG_DIAG_RESP_NEGATIVE &&
				msg.data[2] == CSD_MSG_DIAG_RESP_ERR_PENDING) {

				dev_dbg(dev, "Response pending\n");
				break;
			}

			if (expected > diag_rep->resp_size) {
				dev_err_ratelimited(dev,
					"Single frame response too large for our buffer\n");
				return -ENOBUFS;
			}

			memcpy(diag_rep->resp, msg.data, expected);

			diag_rep->resp_expected = expected;
			diag_rep->resp_recv = expected;

			break;

		case CSD_MSG_DIAG_FRAME_TYPE_FIRST:

			expected = (((u16) msg.byte1 & 0x0F) << 8);
			expected |= msg.byte2;

			if (!expected) {
				dev_err_ratelimited(dev,
					"Multi frame message with zero size\n");
				return -EPROTO;
			}

			/*
			 * Using "<=" instead of "<" would be more logical
			 * but the spec allows it to send a multi frame
			 * message consisting of only the first frame.
			 */
			if (expected < CSD_MSG_DIAG_FIRST_PAYLOAD_SIZE) {
				dev_err_ratelimited(dev,
					"Multi frame message used for single frame payload\n");
				return -EPROTO;
			}

			if (expected > diag_rep->resp_size) {
				dev_err_ratelimited(dev,
					"Multi frame response too large for our buffer\n");
				return -ENOBUFS;
			}

			memcpy(diag_rep->resp, msg.data,
				CSD_MSG_DIAG_FIRST_PAYLOAD_SIZE);

			diag_rep->resp_expected = expected;
			diag_rep->resp_recv = CSD_MSG_DIAG_FIRST_PAYLOAD_SIZE;

			break;

		case CSD_MSG_DIAG_FRAME_TYPE_CONS:

			dev_err_ratelimited(dev,
				"Unexpected consecutive frame\n");
			return -EPROTO;

		default:

			dev_err_ratelimited(dev, "Unknown frame type\n");
			return -EPROTO;

		}

	} else {
		struct csd_msg_diag_cons msg;
		u8 sn;
		int ret;

		sn = ((diag_rep->resp_recv - CSD_MSG_DIAG_FIRST_PAYLOAD_SIZE) /
			CSD_MSG_DIAG_CONS_PAYLOAD_SIZE + 1) & 0x0F;

		ret = csd_read_csd(csd, CSD_MSG_DIAG_RESPONSE_ID, (u8 *) &msg,
			sizeof(msg));

		if (ret)
			return ret;

		switch (msg.byte1 >> 4) {
		u16 current_size;

		case CSD_MSG_DIAG_FRAME_TYPE_SINGLE:

			dev_err_ratelimited(dev,
				"Unexpected single frame message\n");
			return -EPROTO;

		case CSD_MSG_DIAG_FRAME_TYPE_FIRST:

			dev_err_ratelimited(dev,
				"Unexpected first frame\n");
			return -EPROTO;

		case CSD_MSG_DIAG_FRAME_TYPE_CONS:

			if ((msg.byte1 & 0x0F) != sn) {
				dev_err_ratelimited(dev,
					"Unexpected sequence number\n");
				return -EPROTO;
			}

			current_size = diag_rep->resp_expected -
				diag_rep->resp_recv;

			if (current_size > CSD_MSG_DIAG_CONS_PAYLOAD_SIZE)
				current_size = CSD_MSG_DIAG_CONS_PAYLOAD_SIZE;

			memcpy(diag_rep->resp + diag_rep->resp_recv, msg.data,
				current_size);

			diag_rep->resp_recv += current_size;
			break;

		default:

			dev_err_ratelimited(dev,
				"Unknown frame type after first frame\n");
			return -EPROTO;

		}
	}

	return 0;
}

/**
 * csd_handle_diag() - Handles a diagnostic report request
 * @csd:      pointer to csd_data
 * @state:    state buffer
 * @diag_rep: diagnostic report buffer
 *
 * Depending on the state tracked in the diagnostic report buffer either a
 * diagnostic request frame is send or a diagnostic response is requested
 * and received. Delays required by the CSD spec between diagnostic frames
 * are considered.
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_handle_diag(struct csd_data *csd, struct csd_state_buffer *state,
	struct csd_event_diag_rep_buffer *diag_rep)
{
	struct device *dev = &csd->serdev->dev;
	int ret;

	/*
	 * CSD specification defines that at most one frame of a single
	 * diagnostic message including request and response must be send
	 * every 20 ms. A delay between a response and a subsequent
	 * request is not defined, nevertheless we add a delay of 20 ms also
	 * between two unrelated diagnostic frames as CSD is known to be
	 * sensitive to message timing.
	 */
	csd_sleep_until(ktime_add_ms(state->last_diag, CSD_DIAG_DELAY));

	if (diag_rep->req_send < diag_rep->req_size) {

		ret = csd_send_diag_request(csd, diag_rep);

		state->last_diag = ktime_get();

		if (ret) {
			if (++diag_rep->retry > CSD_DIAG_RETRIES)
				return ret;

			/* We failed but we are still within our retry limit */
			return 0;
		}

		if (diag_rep->req_send == diag_rep->req_size)
			diag_rep->expire += jiffies;

		diag_rep->retry = 0;
		return 0;
	}

	if (time_is_before_jiffies(diag_rep->expire)) {
		dev_warn(dev, "Diagnostic report timed out\n");
		return -ETIME;
	}

	ret = csd_get_diag_response(csd, diag_rep);

	state->last_diag = ktime_get();

	if (ret) {
		/*
		 * A retry in case of protocol error will most
		 * likely not succeed either so don't try it.
		 */
		if (ret == -EPROTO)
			return ret;

		/* We won't succeed either if our buffer is full */
		if (ret == -ENOBUFS)
			return ret;

		if (++diag_rep->retry > CSD_DIAG_RETRIES)
			return ret;

		/* We failed but we are still within our retry limit */
		return 0;
	}

	diag_rep->retry = 0;
	return 0;
}

/**
 * csd_diag_is_complete() - Checks if processing of a diagnostic report is done
 * @diag_rep: diagnostic report buffer
 *
 * Diagnostic report requests typically need to be handled in multiple
 * iterations to be able to process higher priority events in particular
 * interrupt requests between individual frames and/or retries. When
 * csd_handle_diag() succeeds this function can be used to check if another
 * iteration is required.
 *
 * Return: true if diagnostic report is full processed
 */
static bool csd_diag_is_complete(struct csd_event_diag_rep_buffer *diag_rep)
{
	if (diag_rep->req_send < diag_rep->req_size)
		return false;

	if (diag_rep->resp) {
		if (!diag_rep->resp_recv)
			return false;

		if (diag_rep->resp_recv < diag_rep->resp_expected)
			return false;
	}

	return true;
}

/**
 * csd_idle_diag_next_state() - Determines next state after diagnostic request
 * @csd:      pointer to csd_data
 * @state:    state buffer
 * @diag_rep: diagnostic report buffer
 *
 * Some diagnostic requests may change the state of the system e.g. by
 * resetting the CSD or by switching the CSD to another session. This function
 * determines in IDLE state the next state based on the diagnostic request.
 *
 * Return: next state
 */
static enum csd_state csd_idle_diag_next_state(struct csd_data *csd,
	struct csd_state_buffer *state,
	struct csd_event_diag_rep_buffer *diag_rep)
{
	struct device *dev = &csd->serdev->dev;

	if (diag_rep->resp_recv &&
		diag_rep->resp[0] == CSD_MSG_DIAG_RESP_NEGATIVE) {

		dev_dbg(dev, "Negative diag response\n");
		return CSD_STATE_IDLE;
	}

	if (diag_rep->req_size == 2 &&
		diag_rep->req[0] == CSD_MSG_DIAG_REQ_RESET &&
		(diag_rep->req[1] == CSD_MSG_DIAG_REQ_RESET_RESP ||
		diag_rep->req[1] == CSD_MSG_DIAG_REQ_RESET_NO_RESP)) {

		dev_dbg(dev, "Diagnostic reset request detected\n");

		csd_release_touch(csd, state);

		csd_unexpose_display_status(csd);

		state->prog_signature = false;

		return CSD_STATE_CSD_POWERING_ON;
	}

	if (diag_rep->req_size == 2 &&
		diag_rep->req[0] == CSD_MSG_DIAG_REQ_SESSION &&
		(diag_rep->req[1] == CSD_MSG_DIAG_REQ_SESSION_PROG_RESP ||
		diag_rep->req[1] == CSD_MSG_DIAG_REQ_SESSION_PROG_NO_RESP)) {

		dev_dbg(dev, "Programming session request detected\n");

		csd_release_touch(csd, state);

		csd_unexpose_display_status(csd);

		state->prog_signature = true;

		return CSD_STATE_CSD_POWERING_ON;
	}

	return CSD_STATE_IDLE;
}

/**
 * csd_prog_diag_next_state() - Determines next state after diagnostic request
 * @csd:      pointer to csd_data
 * @state:    state buffer
 * @diag_rep: diagnostic report buffer
 *
 * Some diagnostic requests may change the state of the system e.g. by
 * resetting the CSD or by switching the CSD to another session. This function
 * determines the next state based on the diagnostic request while CSD is
 * in programming session.
 *
 * Return: next state
 */
static enum csd_state csd_prog_diag_next_state(struct csd_data *csd,
	struct csd_state_buffer *state,
	struct csd_event_diag_rep_buffer *diag_rep)
{
	struct device *dev = &csd->serdev->dev;

	if (diag_rep->resp_recv &&
		diag_rep->resp[0] == CSD_MSG_DIAG_RESP_NEGATIVE) {

		if (diag_rep->req_size == 2 &&
			diag_rep->req[0] == CSD_MSG_DIAG_REQ_RESET &&
			(diag_rep->req[1] == CSD_MSG_DIAG_REQ_RESET_RESP ||
			diag_rep->req[1] == CSD_MSG_DIAG_REQ_RESET_NO_RESP)) {

			dev_info(dev, "Reset request refused, forcing reset\n");

			csd_power_down_csd(csd);

			return CSD_STATE_RESETTING_CSD;
		}

		dev_dbg(dev, "Negative diag response\n");
		return CSD_STATE_CSD_PROG;
	}

	if (diag_rep->req_size == 2 &&
		diag_rep->req[0] == CSD_MSG_DIAG_REQ_RESET &&
		(diag_rep->req[1] == CSD_MSG_DIAG_REQ_RESET_RESP ||
		diag_rep->req[1] == CSD_MSG_DIAG_REQ_RESET_NO_RESP)) {

		dev_dbg(dev, "Diagnostic reset request detected\n");

		state->prog_signature = false;

		return CSD_STATE_CSD_POWERING_ON;
	}

	if (diag_rep->req_size == 2 &&
		diag_rep->req[0] == CSD_MSG_DIAG_REQ_SESSION &&
		(diag_rep->req[1] == CSD_MSG_DIAG_REQ_SESSION_DEF_RESP ||
		diag_rep->req[1] == CSD_MSG_DIAG_REQ_SESSION_DEF_NO_RESP ||
		diag_rep->req[1] == CSD_MSG_DIAG_REQ_SESSION_EXT_RESP ||
		diag_rep->req[1] == CSD_MSG_DIAG_REQ_SESSION_EXT_NO_RESP)) {

		dev_dbg(dev, "Default or extended session request detected\n");

		state->prog_signature = false;

		return CSD_STATE_CSD_POWERING_ON;
	}

	return CSD_STATE_CSD_PROG;
}

/**
 * csd_idle_handle_diag() - Handles a diagnostic request in IDLE state
 * @csd:   pointer to csd_data
 * @state: state buffer
 * @event: event representing diagnostic request
 *
 * Return: next state
 */
static enum csd_state csd_idle_handle_diag(struct csd_data *csd,
	struct csd_state_buffer *state, struct csd_event *event)
{
	struct device *dev = &csd->serdev->dev;
	struct csd_event_diag_rep_buffer *diag_rep = event->data;
	enum csd_state next_state;
	int ret;

	ret = csd_handle_diag(csd, state, diag_rep);
	if (ret) {
		csd_complete_event(dev, event, ret);
		return CSD_STATE_IDLE;
	}

	if (!csd_diag_is_complete(diag_rep)) {
		/*
		 * Larger diagnostic messages or messages requiring a
		 * response need to be processed in multiple steps
		 * in order to process higher priority events like
		 * interrupts in between. Just enqueue the event again.
		 */
		csd_event_diag_requeue(dev, &csd->drvdata.event_buffer, event);
		return CSD_STATE_IDLE;
	}

	next_state = csd_idle_diag_next_state(csd, state, diag_rep);

	csd_complete_event(dev, event, 0);

	return next_state;
}

/**
 * csd_prog_handle_diag() - Handles a diagnostic request in CSD_PROG state
 * @csd:   pointer to csd_data
 * @state: state buffer
 * @event: event representing diagnostic request
 *
 * Return: next state
 */
static enum csd_state csd_prog_handle_diag(struct csd_data *csd,
	struct csd_state_buffer *state, struct csd_event *event)
{
	struct device *dev = &csd->serdev->dev;
	struct csd_event_diag_rep_buffer *diag_rep = event->data;
	enum csd_state next_state;
	int ret;

	ret = csd_handle_diag(csd, state, diag_rep);
	if (ret) {
		if (diag_rep->req_size == 2 &&
			diag_rep->req[0] == CSD_MSG_DIAG_REQ_RESET &&
			(diag_rep->req[1] == CSD_MSG_DIAG_REQ_RESET_RESP ||
			diag_rep->req[1] == CSD_MSG_DIAG_REQ_RESET_NO_RESP)) {

			dev_info(dev, "Reset request failed, forcing reset\n");

			csd_power_down_csd(csd);

			csd_complete_event(dev, event, ret);
			return CSD_STATE_RESETTING_CSD;
		}

		csd_complete_event(dev, event, ret);
		return CSD_STATE_CSD_PROG;
	}

	if (!csd_diag_is_complete(diag_rep)) {
		csd_event_diag_requeue(dev, &csd->drvdata.event_buffer, event);
		return CSD_STATE_CSD_PROG;
	}

	next_state = csd_prog_diag_next_state(csd, state, diag_rep);

	csd_complete_event(dev, event, 0);

	return next_state;
}

/**
 * csd_trans_idle_to_full_reset() - Performs transition from idle to full reset
 * @csd:   pointer to csd_data
 * @state: state buffer
 */
static void csd_trans_idle_to_full_reset(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	csd_release_touch(csd, state);

	csd_unexpose_display_status(csd);

	csd_power_down_csd(csd);

	csd_mask_int_irq(csd, true);
}

/**
 * csd_hdcp_check_soc() - Checks for HDCP requests from SOC
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * It is expected to gracefully fall back to non-HDCP operation if HDCP fails.
 * Therefore only error messages are printed for non-critical errors, while a
 * full reset is performed for errors, which may break non-HDCP operation.
 * In case of gracefull fallback we need to set the READY bit. This will
 * tell the video source to proceed with HDCP auth procedure, which will of
 * course fail as we are obviously not ready in this case. A retry will be
 * triggered in video source. After a few attempts video source can also
 * gracefully fall back to non-HDCP operation, but can still perform another
 * attempt to establish HDCP at any later point in time. Setting the READY
 * bit makes sure that we can detect any later attempt, while we don't need
 * to retry initialization on GMSL link periodically in the mean time.
 *
 * Return: next state (CSD_STATE_IDLE on success or non-critical fail)
 */
static enum csd_state csd_hdcp_check_soc(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	struct device *dev = &csd->serdev->dev;
	bool request;
	int retry;
	int ret;

	if (csd->drvdata.ser_info->hdcp.check_req) {
		ret = csd->drvdata.ser_info->hdcp.check_req(csd, &request);
		if (ret) {
			dev_warn_ratelimited(dev, "Check for HDCP requests from SOC failed\n");
			return CSD_STATE_IDLE;
		}
	} else {
		mutex_lock(&csd->drvdata.user_hdcp.lock);

		request = csd->drvdata.user_hdcp.request;

		mutex_unlock(&csd->drvdata.user_hdcp.lock);
	}

	if (!request)
		return CSD_STATE_IDLE;

	dev_dbg(dev, "Detected HDCP request from SOC\n");

	if (!state->hdcp.enabled) {
		bool critical_fail = false;

		for (retry = 0; retry < CSD_HDCP_GMSL_REINIT_RETRIES; retry++) {
			ret = csd_hdcp_reinit(csd, state, &critical_fail);
			if (!ret)
				break;
		}

		if (ret) {
			dev_warn(dev, "HDCP initialization on GMSL link failed\n");

			if (!critical_fail) {
				csd_hdcp_prepare(csd);
				return CSD_STATE_IDLE;
			}

			dev_err(dev, "Performing reset due to HDCP initialization failure\n");

			csd_trans_idle_to_full_reset(csd, state);

			return CSD_STATE_RESET_SER;
		}

		state->hdcp.last_gmsl = ktime_get();
		state->hdcp.enabled = true;

		dev_info(dev, "HDCP enabled on GMSL link level\n");
	}

	for (retry = 0; retry < CSD_HDCP_SOC_REQ_RETRIES; retry++) {
		ret = csd_hdcp_handle_soc_req(csd, state);
		if (!ret)
			return CSD_STATE_IDLE;
	}

	csd_hdcp_prepare(csd);
	dev_warn(dev, "Handling of HDCP request from SOC failed\n");
	return CSD_STATE_IDLE;
}

/**
 * csd_hdcp_check_gmsl() - Checks link integrity and performs reinit if required
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_hdcp_check_gmsl(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	struct device *dev = &csd->serdev->dev;
	int retry;
	int ret;

	ret = csd_hdcp_check_ri(csd);
	if (!ret)
		return 0;

	for (retry = 0; retry < CSD_HDCP_GMSL_REINIT_RETRIES; retry++) {
		ret = csd_hdcp_reinit(csd, state, NULL);
		if (!ret)
			return 0;
	}

	dev_err(dev, "HDCP reinitialization failed\n");
	return ret;
}

/**
 * csd_hdcp_preferred_time() - Checks if it is preferred time for HDCP check
 * @state: state buffer
 *
 * In order not to delay other periodic messages unnecessarily it is preferable
 * to perform HDCP related checks and potentially HDCP configuration between
 * periodic messages when the gap between two messages is large. This function
 * determines based on the state if we are currently inside such a large gap.
 *
 * Return: true if current gap is large
 */
static bool csd_hdcp_preferred_time(struct csd_state_buffer *state)
{
	struct csd_periodic_buffer *periodic = &state->periodic;

	switch (periodic->state) {
	case CSD_PSTATE_IDLE_IHU_REQ:
	case CSD_PSTATE_IDLE_STATUS:
	case CSD_PSTATE_IDLE_TOUCH:
	case CSD_PSTATE_ACTIV_TOUCH1:
	case CSD_PSTATE_ACTIV_TOUCH5:
		return true;
	default:
		return false;
	}
}

/**
 * csd_hdcp_handle_periodic() - Performs periodic HDCP operations
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * This functions performs periodic HDCP operations in CSD_STATE_IDLE.
 *
 * Return: next state (CSD_STATE_IDLE on success)
 */
static enum csd_state csd_hdcp_handle_periodic(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	struct device *dev = &csd->serdev->dev;
	u64 delta;

	if (no_hdcp || !csd_ser_supports_hdcp(csd->drvdata.ser_info) ||
		!state->des_info->hdcp_support)

		return CSD_STATE_IDLE;

	delta = csd_hdcp_preferred_time(state) ? 40 : 0;

	if (state->hdcp.enabled &&
		ktime_after(ktime_get(), ktime_add_ms(state->hdcp.last_gmsl,
		CSD_HDCP_GMSL_CHK_INTERVALL - delta))) {

		int ret;

		dev_dbg(dev, "Checking link integrity on GMSL\n");

		state->hdcp.last_gmsl = ktime_get();

		ret = csd_hdcp_check_gmsl(csd, state);
		if (ret) {
			dev_err(dev, "Performing reset due to HDCP link integrity check failure\n");

			csd_trans_idle_to_full_reset(csd, state);

			return CSD_STATE_RESET_SER;
		}

		return CSD_STATE_IDLE;
	}

	if (ktime_after(ktime_get(), ktime_add_ms(state->hdcp.last_soc,
		CSD_HDCP_SOC_CHK_INTERVALL - delta))) {

		dev_dbg(dev, "Checking for HDCP requests from SOC\n");

		state->hdcp.last_soc = ktime_get();

		return csd_hdcp_check_soc(csd, state);
	}

	return CSD_STATE_IDLE;
}

/**
 * csd_read_hdcp() - Gets HDCP status
 * @csd:         pointer to csd_data
 * @state:       state buffer
 * @hdcp_status: buffer passed with CSD_EVENT_READ_HDCP
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_read_hdcp(struct csd_data *csd, struct csd_state_buffer *state,
	struct csd_event_read_hdcp_buffer *hdcp_status)
{
	hdcp_status->des_support = state->des_info->hdcp_support;

	if (!hdcp_status->des_support)
		return 0;

	if (csd->drvdata.ser_info->hdcp.read_stat) {
		hdcp_status->show_bksv = false;
		return csd->drvdata.ser_info->hdcp.read_stat(csd,
			&hdcp_status->enabled);
	}

	hdcp_status->show_bksv = true;

	hdcp_status->enabled = state->hdcp.enabled;

	if (hdcp_status->enabled)
		memcpy(hdcp_status->bksv, state->hdcp.bksv, CSD_HDCP_BKSV_SIZE);

	return 0;
}

/**
 * csd_state_idle_prog_common() - Common implementation for IDLE and CSD_PROG
 * @csd:          pointer to csd_data
 * @state:        state buffer
 * @prog_session: true when in CSD_PROG state
 *
 * Return: next state
 */
static enum csd_state csd_state_idle_prog_common(struct csd_data *csd,
	struct csd_state_buffer *state, bool prog_session)
{
	struct device *dev = &csd->serdev->dev;
	struct csd_event *event;

	if (prog_session) {

		event = csd_get_event(csd, NULL);

	} else {
		ktime_t timeout;

		if (state->transmission_fails > CSD_MAX_TRANSMISSION_FAILS) {

			dev_err(dev,
				"Performing reset due to transmission fails\n");

			csd_trans_idle_to_full_reset(csd, state);

			return CSD_STATE_RESET_SER;
		}

		if (state->mode_fails > CSD_MAX_MODE_FAILS) {

			dev_err(dev,
				"Performing CSD reset due to mode mismatch\n");

			csd_release_touch(csd, state);

			csd_unexpose_display_status(csd);

			csd_power_down_csd(csd);

			return CSD_STATE_RESETTING_CSD;
		}

		timeout = csd_get_time_of_next_periodic(&state->periodic);

		event = csd_get_event(csd, &timeout);
	}

	if (!event) {
		/* timeout */

		csd_send_periodic_message(csd, state);

		return csd_hdcp_handle_periodic(csd, state);
	}

	switch (event->type) {
	enum csd_state next_state;

	case CSD_EVENT_UNLOAD:

		csd_event_buffer_unload(csd);

		if (!prog_session) {

			csd_release_touch(csd, state);

			csd_send_shutdown_request(csd);

			csd_unexpose_display_status(csd);

		}

		csd_power_down_csd(csd);

		csd_power_down_ser(csd);

		csd_complete_event(dev, event, 0);

		return CSD_STATE_TERMINATE;

	case CSD_EVENT_INT_IRQ:

		if (prog_session)
			return csd_prog_handle_int_irq(csd, state, event);

		next_state = csd_handle_int_irq(csd, state, event);

		if (next_state != CSD_STATE_IDLE)
			return next_state;

		return csd_hdcp_handle_periodic(csd, state);

	case CSD_EVENT_SUSPEND:

		dev_err(dev, "Received suspend request while video output is enabled and display is on\n");

		csd_reject_event(dev, event);

		break;

	case CSD_EVENT_DISABLE:

		if (!prog_session) {

			csd_release_touch(csd, state);

			csd_send_shutdown_request(csd);

			csd_unexpose_display_status(csd);

		}

		csd_power_down_csd(csd);

		csd_mask_int_irq(csd, true);

		csd_complete_event(dev, event, 0);

		return CSD_STATE_SCONF_CRESET;

	case CSD_EVENT_POST_DISABLE:

		dev_err(dev, "Received post-disable request while video output is enabled, expected disable request first\n");

		csd_reject_event(dev, event);

		break;

	case CSD_EVENT_PRE_ENABLE:

		dev_err(dev, "Received pre-enable request while video output is enabled\n");

		csd_reject_event(dev, event);

		break;

	case CSD_EVENT_ENABLE:

		dev_err(dev, "Received enable request while video output is already enabled\n");

		csd_reject_event(dev, event);

		break;

	case CSD_EVENT_READ_EDID:

		if (prog_session)
			csd_reject_event(dev, event);
		else
			csd_complete_event(dev, event, csd_read_csd(csd,
				CSD_MSG_DISPLAY_EDID_ID, (u8 *) event->data,
				sizeof(struct csd_msg_display_edid)));

		break;

	case CSD_EVENT_READ_HDCP:

		if (prog_session) {
			csd_reject_event(dev, event);
			break;
		}

		csd_complete_event(dev, event, csd_read_hdcp(csd, state,
			event->data));

		break;

	case CSD_EVENT_EE_ACCESS:

		csd_ee_access(csd, event);

		break;

	case CSD_EVENT_DIAG_REPORT:

		if (prog_session)
			return csd_prog_handle_diag(csd, state, event);

		next_state = csd_idle_handle_diag(csd, state, event);

		if (next_state != CSD_STATE_IDLE)
			return next_state;

		return csd_hdcp_handle_periodic(csd, state);

	case CSD_EVENT_LF_READ:

		csd_complete_event(dev, event,
			csd->drvdata.ser_info->line_fault(csd,
				event->data));

		break;

	case CSD_EVENT_ERRB_TEST:

		csd_complete_event(dev, event,
			csd_run_errb_test(csd, event->data));

		break;

	case CSD_EVENT_CTRL_TEST:
	case CSD_EVENT_GPIO_TEST:

		csd_reject_mfg_event(dev, event);

	}

	if (prog_session)
		return CSD_STATE_CSD_PROG;

	return CSD_STATE_IDLE;
}

/**
 * csd_state_idle() - Implements CSD state IDLE
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_idle(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	return csd_state_idle_prog_common(csd, state, false);
}

/**
 * csd_state_csd_prog() - Implements CSD state CSD_PROG
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_csd_prog(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	return csd_state_idle_prog_common(csd, state, true);
}

/**
 * csd_state_csd_reg_prog_common_pp() - Implements CSD\*_REG_PROG states
 * @csd:          pointer to csd_data
 * @state:        state buffer
 * @prog_session: true when in CSD_PROG_REG_PROG state
 * @postponed:    queue for postponed events
 *
 * Return: next state
 */
static enum csd_state csd_state_csd_reg_prog_common_pp(struct csd_data *csd,
	struct csd_state_buffer *state, bool prog_session,
	struct plist_head *postponed)
{
	struct device *dev = &csd->serdev->dev;
	ktime_t expire = ktime_add_ms(ktime_get(), CSD_WAITING_TIME_MS);

	while (true) {
		struct csd_event *event;

		event = csd_get_event_hard_timeout(csd, expire);

		if (!event) {
			/* timeout */

			dev_err(dev, "Missing falling edge on int line\n");

			if (!prog_session) {

				csd_release_touch(csd, state);

				csd_unexpose_display_status(csd);

			}

			csd_power_down_csd(csd);

			csd_mask_int_irq(csd, true);

			return CSD_STATE_RESET_SER;
		}

		switch (event->type) {

		case CSD_EVENT_UNLOAD:

			csd_event_buffer_unload(csd);

			if (!prog_session) {

				csd_release_touch(csd, state);

				csd_unexpose_display_status(csd);

			}

			csd_power_down_csd(csd);

			csd_power_down_ser(csd);

			csd_complete_event(dev, event, 0);

			return CSD_STATE_TERMINATE;

		case CSD_EVENT_INT_IRQ:

			csd_complete_event(dev, event, 0);

			if (prog_session)
				return CSD_STATE_CSD_PROG;

			return CSD_STATE_IDLE;

		case CSD_EVENT_SUSPEND:

			dev_err(dev, "Received suspend request while video output is enabled and CSD is performing register access\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_DISABLE:

			if (!prog_session) {

				csd_release_touch(csd, state);

				csd_unexpose_display_status(csd);

			}

			csd_power_down_csd(csd);

			csd_mask_int_irq(csd, true);

			csd_complete_event(dev, event, 0);

			return CSD_STATE_SCONF_CRESET;

		case CSD_EVENT_POST_DISABLE:

			dev_err(dev, "Received post-disable request while video output is enabled, expected disable request first\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_PRE_ENABLE:

			dev_err(dev, "Received pre-enable request while video output is enabled\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_ENABLE:

			dev_err(dev, "Received enable request while video output is already enabled\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_CTRL_TEST:
		case CSD_EVENT_GPIO_TEST:

			csd_reject_mfg_event(dev, event);

			break;

		case CSD_EVENT_READ_EDID:
		case CSD_EVENT_READ_HDCP:

			if (prog_session) {

				csd_reject_event(dev, event);

				break;

			}

			/* fall through */

		case CSD_EVENT_EE_ACCESS:
		case CSD_EVENT_DIAG_REPORT:
		case CSD_EVENT_LF_READ:
		case CSD_EVENT_ERRB_TEST:

			/*
			 * We can not process any of these events in this
			 * state but we can process them in idle state. The
			 * probability is very high that we will transition
			 * back in idle state very soon. Postpone the event.
			 */

			csd_event_postpone(dev, postponed, event);

		}
	}
}

/**
 * csd_state_csd_reg_prog_common() - Implements CSD\*_REG_PROG states
 * @csd:          pointer to csd_data
 * @state:        state buffer
 * @prog_session: true when in CSD_PROG_REG_PROG state
 *
 * Return: next state
 */
static enum csd_state csd_state_csd_reg_prog_common(struct csd_data *csd,
	struct csd_state_buffer *state, bool prog_session)
{
	struct device *dev = &csd->serdev->dev;
	struct plist_head postponed;
	enum csd_state ret;

	plist_head_init(&postponed);

	ret = csd_state_csd_reg_prog_common_pp(csd, state, prog_session,
		&postponed);

	csd_event_restore_postponed(dev, &csd->drvdata.event_buffer,
		&postponed);

	return ret;
}

/**
 * csd_state_csd_reg_prog() - Implements CSD state CSD_REG_PROG
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_csd_reg_prog(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	return csd_state_csd_reg_prog_common(csd, state, false);
}

/**
 * csd_state_csd_prog_reg_prog() - Implements CSD state CSD_PROG_REG_PROG
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_csd_prog_reg_prog(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	return csd_state_csd_reg_prog_common(csd, state, true);
}

/**
 * csd_state_resetting_csd() - Implements CSD state RESETTING_CSD
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_resetting_csd(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	csd_reject_diag_events(csd);

	csd_mask_int_irq(csd, true);

	csd_wait_until_csd_in_reset(csd);

	csd_cache_line_fault(csd, state);

	return CSD_STATE_SRECV_CRESET;
}

/**
 * csd_state_converter_common() - Implements CSD states \*CONVERTER
 * @csd:      pointer to csd_data
 * @state:    state buffer
 * @detected: true if in state CSD_STATE_CONVERTER, false otherwise
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_converter_common(struct csd_data *csd,
	struct csd_state_buffer *state, bool detected)
{
	struct device *dev = &csd->serdev->dev;
	ktime_t expire = ktime_add_ms(ktime_get(), CSD_CONVERTER_POLL);

	for (;;) {
		struct csd_event *event;

		event = csd_get_event_hard_timeout(csd, expire);

		if (!event) {
			/* timeout */

			if (!csd_in_ee_mode() &&
				csd_converter_present(csd, !detected))

				return CSD_STATE_CONVERTER;

			if (csd_using_conv_display_mode())
				return CSD_STATE_WAIT_CONVERTER;

			return CSD_STATE_SRECV_CRESET;
		}

		switch (event->type) {

		case CSD_EVENT_UNLOAD:

			csd_event_buffer_unload(csd);

			csd_power_down_ser(csd);

			csd_complete_event(dev, event, 0);

			return CSD_STATE_TERMINATE;

		case CSD_EVENT_INT_IRQ:

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_SUSPEND:

			dev_err(dev, "Received suspend request while video output is enabled\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_DISABLE:

			csd_complete_event(dev, event, 0);

			return CSD_STATE_SCONF_CRESET;

		case CSD_EVENT_POST_DISABLE:

			dev_err(dev, "Display post-disable request received, but display is not in disabled state, disable request expected\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_PRE_ENABLE:

			dev_err(dev, "Display pre-enable request received, but display is already enabled\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_ENABLE:

			dev_err(dev, "Display enable request received, but display is already enabled\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_READ_EDID:
		case CSD_EVENT_READ_HDCP:

			if (detected)
				dev_err(dev, "EDID reading and HDCP status reading is supported only with actual CSD displays, but currently a converter board is attached\n");
			else
				dev_err(dev, "EDID reading and HDCP status reading is supported only with actual CSD displays, but currently a converter board is supposed to be attached\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_EE_ACCESS:

			csd_ee_access(csd, event);

			break;

		case CSD_EVENT_DIAG_REPORT:

			if (detected)
				dev_err(dev, "Diagnostic reports are supported only by actual CSD displays, but currently a converter board is attached\n");
			else
				dev_err(dev, "Diagnostic reports are supported only by actual CSD displays, but currently a converter board is supposed to be attached\n");

			csd_reject_event(dev, event);

			break;

		case CSD_EVENT_LF_READ:

			csd_complete_event(dev, event,
				csd->drvdata.ser_info->line_fault(csd,
					event->data));

			break;

		case CSD_EVENT_ERRB_TEST:

			csd_complete_event(dev, event,
				csd_run_errb_test(csd, event->data));

			break;

		case CSD_EVENT_CTRL_TEST:
		case CSD_EVENT_GPIO_TEST:

			csd_reject_mfg_event(dev, event);

		}
	}
}

/**
 * csd_state_converter() - Implements CSD state CONVERTER
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_converter(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	return csd_state_converter_common(csd, state, true);
}

/**
 * csd_state_wait_converter() - Implements CSD state WAIT_CONVERTER
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_wait_converter(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	return csd_state_converter_common(csd, state, false);
}


/**
 * csd_runtime_reset_serializer() - Perform run-time reset of serializer
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Depending on the serializer either the power down line is toggled
 * or a serializer specific soft reset is performed.
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_runtime_reset_serializer(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	int ret = 0;

	if (!csd->drvdata.ser_info->soft_reset) {
		csd_power_down_ser(csd);
		csd_power_up_ser(csd);
	} else {
		ret = csd->drvdata.ser_info->soft_reset(csd, gmsl2);
	}

	state->hdcp.enabled = false;

	return ret;
}

/**
 * csd_state_reset_ser() - Implements CSD state RESET_SER
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_reset_ser(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	struct device *dev = &csd->serdev->dev;
	int retries = 10;

	csd_reject_diag_events(csd);

	csd_wait_until_csd_in_reset(csd);

	csd_cache_line_fault(csd, state);

	while (retries--) {
		if (csd_runtime_reset_serializer(csd, state)) {
			dev_warn(dev, "Runtime reset of serializer has failed\n");
			continue;
		}

		if (csd->drvdata.ser_info->power_on(csd, gmsl2)) {
			dev_warn(dev, "Serializer power on configuration failed\n");
			continue;
		}

		if (csd->drvdata.ser_info->pre_enable &&
			csd->drvdata.ser_info->pre_enable(csd, gmsl2,
			csd_get_display_mode())) {

			dev_warn(dev, "Serializer pre_enable configuration failed\n");
			continue;
		}

		if (csd_serializer_enable(csd)) {
			dev_warn(dev, "Post video serializer configuration failed\n");
			continue;
		}

		return CSD_STATE_SRECV_CRESET;

	}

	dev_err(dev, "Serializer reset and reconfiguration has failed\n");

	csd_event_buffer_unload(csd);

	csd_power_down_ser(csd);

	return CSD_STATE_TERMINATE;
}

/**
 * csd_configure_serdev() - Configures serial device
 * @serdev: serial device
 *
 * Sets communication parameters like baud rate, flow control and parity
 * setting of serial interface to match CSD requirements.
 *
 * Return: 0
 */
static int csd_configure_serdev(struct serdev_device *serdev)
{
	serdev_device_set_baudrate(serdev, 416000);
	serdev_device_set_flow_control(serdev, false);
	serdev_device_set_parity(serdev, SERDEV_PARITY_EVEN);
	return 0;
}

/**
 * csd_state_suspended() - Implements CSD state SUSPENDED
 * @csd:   pointer to csd_data
 * @state: state buffer
 *
 * Return: next CSD state
 */
static enum csd_state csd_state_suspended(struct csd_data *csd,
	struct csd_state_buffer *state)
{
	struct device *dev = &csd->serdev->dev;
	int ret;

	wait_for_completion(&csd->drvdata.resume);

	ret = serdev_device_open(csd->serdev);
	if (ret) {
		dev_err(dev, "Failed to reopen UART interface\n");
		csd->serdev = NULL;
		csd_event_buffer_unload(csd);
		return CSD_STATE_TERMINATE;
	}

	csd_configure_serdev(csd->serdev);

	return CSD_STATE_SOFF_CRESET;
}

/**
 * csd_get_state_name() - Returns human readable name of a state
 * @state: state
 *
 * Return: human readable name
 */
static const char *csd_get_state_name(enum csd_state state)
{
	switch (state) {
	case CSD_STATE_PROBED_CRESET:
		return "PROBED_CRESET";
	case CSD_STATE_SOFF_CRESET:
		return "SOFF_CRESET";
	case CSD_STATE_SCONF_CRESET:
		return "SCONF_CRESET";
	case CSD_STATE_SRECV_CRESET:
		return "SRECV_CRESET";
	case CSD_STATE_CSD_POWERING_ON:
		return "CSD_POWERING_ON";
	case CSD_STATE_CSD_PBL:
		return "CSD_PBL";
	case CSD_STATE_CSD_PBL_APP_EARLY:
		return "CSD_PBL_APP_EARLY";
	case CSD_STATE_CSD_PBL_APP_LATE:
		return "CSD_PBL_APP_LATE";
	case CSD_STATE_CSD_APP:
		return "CSD_APP";
	case CSD_STATE_IDLE:
		return "IDLE";
	case CSD_STATE_CSD_PROG:
		return "CSD_PROG";
	case CSD_STATE_CSD_REG_PROG:
		return "CSD_REG_PROG";
	case CSD_STATE_CSD_PROG_REG_PROG:
		return "CSD_PROG_REG_PROG";
	case CSD_STATE_RESETTING_CSD:
		return "RESETTING_CSD";
	case CSD_STATE_CONVERTER:
		return "CONVERTER";
	case CSD_STATE_WAIT_CONVERTER:
		return "WAIT_CONVERTER";
	case CSD_STATE_RESET_SER:
		return "RESET_SER";
	case CSD_STATE_SUSPENDED:
		return "SUSPENDED";
	case CSD_STATE_TERMINATE:
		return "TERMINATE";
	}

	return "UNKNOWN";
}

/**
 * csd_cdev_ref_release() - Signal last user of cdev has stopped using our dev
 * @kref: pointer to ref_count in struct csd_cdev_drvdata
 */
static void csd_cdev_ref_release(struct kref *kref)
{
	struct csd_cdev_drvdata *cdev_drvdata =
		container_of(kref, struct csd_cdev_drvdata, ref_count);

	complete(&cdev_drvdata->done);
}

/**
 * csd_expose_dev_to_cdev() - Exposes device structure to global variable
 * @index: index of display
 * @dev:   display specific device structure to expose
 *
 * Due to the nature of the kernel interface provided to manage character
 * devices the implementation of the character devices provided to issue
 * ioctl commands involves global variables. In order to access display
 * specific data a corresponding device needs to be exposed as global variable.
 */
static void csd_expose_dev_to_cdev(int index, struct device *dev)
{
	mutex_lock(&csd_global.drvdata[index].lock);

	csd_global.drvdata[index].dev = dev;

	kref_init(&csd_global.drvdata[index].ref_count);

	reinit_completion(&csd_global.drvdata[index].done);

	mutex_unlock(&csd_global.drvdata[index].lock);
}

/**
 * csd_unexpose_dev_from_cdev() - Clear global variable holding device structure
 * @index: index of display
 *
 * See also description for csd_expose_dev_to_cdev().
 * This function blocks as long as the exposed dev is used by user space
 * i.e. the corresponding cdev is kept open by some process.
 */
static void csd_unexpose_dev_from_cdev(int index)
{
	mutex_lock(&csd_global.drvdata[index].lock);

	csd_global.drvdata[index].dev = NULL;

	kref_put(&csd_global.drvdata[index].ref_count, csd_cdev_ref_release);

	mutex_unlock(&csd_global.drvdata[index].lock);

	wait_for_completion(&csd_global.drvdata[index].done);
}

/**
 * csd_register_input_device() - Registers an input device with input subsystem
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_register_input_device(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;
	struct input_dev *input_dev;
	int max_x, max_y;
	int ret;

	input_dev = input_allocate_device();
	if (!input_dev)
		return -ENOMEM;

	input_dev->name = "CSD Touchscreen";
	input_dev->id.bustype = BUS_RS232;
	input_dev->dev.parent = csd->csd_dev;

	input_set_capability(input_dev, EV_KEY, BTN_TOUCH);

	if (gmsl2) {
		max_x = CSD_DISPLAY_WIDTH_GMSL2 - 1;
		max_y = CSD_DISPLAY_HEIGHT_GMSL2 - 1;
	} else {
		max_x = CSD_DISPLAY_WIDTH_GMSL1 - 1;
		max_y = CSD_DISPLAY_HEIGHT_GMSL1 - 1;
	}

	ret = input_mt_init_slots(input_dev, CSD_MSG_TOUCH_STATUS_MAX_CNT,
		INPUT_MT_DIRECT);
	if (ret)
		goto cleanup_input_alloc;

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, max_y, 0, 0);

	/* CSD reports only rectangular shapes */
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0,
		max(max_x + 1, max_y + 1), 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, 0,
		min(max_x + 1, max_y + 1), 0, 0);
	input_set_abs_params(input_dev, ABS_MT_ORIENTATION, 0, 1, 0, 0);

	ret = input_register_device(input_dev);
	if (ret)
		goto cleanup_input_alloc;

	csd->input_dev = input_dev;

	return 0;

cleanup_input_alloc:

	input_free_device(input_dev);

	dev_err(dev, "Failed to register input device\n");

	return ret;
}

/**
 * csd_unregister_input_device() - Removes input device
 * @csd: pointer to csd_data
 */
static void csd_unregister_input_device(struct csd_data *csd)
{
	input_unregister_device(csd->input_dev);
}

/**
 * csd_register_brightness_control() - Registers brightness control
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_register_brightness_control(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;
	int ret;

	ret = device_create_file(csd->csd_dev, &dev_attr_brightness);
	if (ret)
		goto cleanup_pre_max;

	ret = device_create_file(csd->csd_dev, &dev_attr_max_brightness);
	if (ret)
		goto cleanup_max;

	return 0;

cleanup_max:

	device_remove_file(csd->csd_dev, &dev_attr_brightness);

cleanup_pre_max:

	dev_err(dev, "Failed to register brightness control\n");

	return ret;
}

/**
 * csd_unregister_brightness_control() - Unregisters brightness control
 * @csd: pointer to csd_data
 */
static void csd_unregister_brightness_control(struct csd_data *csd)
{
	device_remove_file(csd->csd_dev, &dev_attr_max_brightness);
	device_remove_file(csd->csd_dev, &dev_attr_brightness);
}

static ssize_t ser_reg_addr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	u16 address;

	mutex_lock(&drvdata->ee_ser_access.lock);

	address = drvdata->ee_ser_access.address;

	mutex_unlock(&drvdata->ee_ser_access.lock);

	return sprintf(buf, "0x%04hX\n", address);
}

static ssize_t ser_reg_addr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	u16 address;
	int ret;

	ret = kstrtou16(buf, 0, &address);
	if (ret)
		return ret;

	mutex_lock(&drvdata->ee_ser_access.lock);

	drvdata->ee_ser_access.address = address;

	mutex_unlock(&drvdata->ee_ser_access.lock);

	return count;
}
static DEVICE_ATTR_RW(ser_reg_addr);

static ssize_t des_reg_addr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	u16 address;

	mutex_lock(&drvdata->ee_des_access.lock);

	address = drvdata->ee_des_access.address;

	mutex_unlock(&drvdata->ee_des_access.lock);

	if (gmsl2)
		return sprintf(buf, "0x%04hX\n", address);

	return sprintf(buf, "0x%02hX\n", address);
}

static ssize_t des_reg_addr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	u16 address;
	int ret;

	ret = kstrtou16(buf, 0, &address);
	if (ret)
		return ret;

	if (!gmsl2 && address > U8_MAX) {
		dev_err(dev, "Address out of range for GMSL1\n");
		return -ERANGE;
	}

	mutex_lock(&drvdata->ee_des_access.lock);

	drvdata->ee_des_access.address = address;

	mutex_unlock(&drvdata->ee_des_access.lock);

	return count;
}
static DEVICE_ATTR_RW(des_reg_addr);

static ssize_t ser_reg_value_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_event_ee_access_buffer buffer;
	int ret;

	buffer.type = CSD_EE_ACCESS_SER_READ;

	mutex_lock(&drvdata->ee_ser_access.lock);

	buffer.address = drvdata->ee_ser_access.address;

	/* Keeping the mutex locked is optional from this point on */

	ret = csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_EE_ACCESS, &buffer);

	mutex_unlock(&drvdata->ee_ser_access.lock);

	if (ret)
		return ret;

	return sprintf(buf, "0x%02hhX\n", buffer.data);
}

static ssize_t ser_reg_value_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_event_ee_access_buffer buffer;
	int ret;

	ret = kstrtou8(buf, 0, &buffer.data);
	if (ret)
		return ret;

	buffer.type = CSD_EE_ACCESS_SER_WRITE;

	mutex_lock(&drvdata->ee_ser_access.lock);

	buffer.address = drvdata->ee_ser_access.address;

	/* Keeping the mutex locked is optional from this point on */

	ret = csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_EE_ACCESS, &buffer);

	mutex_unlock(&drvdata->ee_ser_access.lock);

	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(ser_reg_value);

static ssize_t des_reg_value_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_event_ee_access_buffer buffer;
	int ret;

	buffer.type = CSD_EE_ACCESS_DES_READ;

	mutex_lock(&drvdata->ee_des_access.lock);

	buffer.address = drvdata->ee_des_access.address;

	/* Keeping the mutex locked is optional from this point on */

	ret = csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_EE_ACCESS, &buffer);

	mutex_unlock(&drvdata->ee_des_access.lock);

	if (ret)
		return ret;

	return sprintf(buf, "0x%02hhX\n", buffer.data);
}

static ssize_t des_reg_value_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_event_ee_access_buffer buffer;
	int ret;

	ret = kstrtou8(buf, 0, &buffer.data);
	if (ret)
		return ret;

	buffer.type = CSD_EE_ACCESS_DES_WRITE;

	mutex_lock(&drvdata->ee_des_access.lock);

	buffer.address = drvdata->ee_des_access.address;

	/* Keeping the mutex locked is optional from this point on */

	ret = csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_EE_ACCESS, &buffer);

	mutex_unlock(&drvdata->ee_des_access.lock);

	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(des_reg_value);

static ssize_t errb_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	bool passed;
	int ret;

	dev_warn(dev, "The errb_test interface is deprecated, please use gpio_test instead\n");

	ret = csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_ERRB_TEST, &passed);

	if (ret)
		return ret;

	if (passed)
		return sprintf(buf, "ok\n");

	return sprintf(buf, "fail\n");
}
static DEVICE_ATTR_RO(errb_test);

static ssize_t ctrl_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	bool assert;
	int ret;

	ret = strtobool(buf, &assert);
	if (ret)
		return ret;

	ret = csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_CTRL_TEST, &assert);

	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_WO(ctrl_test);

static ssize_t gpio_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_event_gpio_test_buffer buffer = { .buf = buf };
	int ret;

	ret = csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_GPIO_TEST, &buffer);

	if (ret)
		return ret;

	return buffer.written;
}
static DEVICE_ATTR_RO(gpio_test);

/**
 * csd_register_ee_access() - Create sysfs interface for register access
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_register_ee_access(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;
	int ret;

	ret = device_create_file(dev, &dev_attr_ser_reg_addr);
	if (ret) {
		csd_event_buffer_unload(csd);
		goto cleanup_pre_ser_reg_addr;
	}

	ret = device_create_file(dev, &dev_attr_ser_reg_value);
	if (ret) {
		csd_event_buffer_unload(csd);
		goto cleanup_ser_reg_addr;
	}

	ret = device_create_file(csd->csd_dev, &dev_attr_des_reg_addr);
	if (ret) {
		csd_event_buffer_unload(csd);
		goto cleanup_ser_reg_value;
	}

	ret = device_create_file(csd->csd_dev, &dev_attr_des_reg_value);
	if (ret) {
		csd_event_buffer_unload(csd);
		goto cleanup_des_reg_addr;
	}

	return 0;

cleanup_des_reg_addr:

	device_remove_file(csd->csd_dev, &dev_attr_des_reg_addr);

cleanup_ser_reg_value:

	device_remove_file(dev, &dev_attr_ser_reg_value);

cleanup_ser_reg_addr:

	device_remove_file(dev, &dev_attr_ser_reg_addr);

cleanup_pre_ser_reg_addr:

	dev_err(dev, "Failed to register EE debugging interface\n");

	return ret;
}

/**
 * csd_unregister_ee_access() - Remove sysfs interface for register access
 * @csd: pointer to csd_data
 */
static void csd_unregister_ee_access(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;

	device_remove_file(csd->csd_dev, &dev_attr_des_reg_value);
	device_remove_file(csd->csd_dev, &dev_attr_des_reg_addr);
	device_remove_file(dev, &dev_attr_ser_reg_value);
	device_remove_file(dev, &dev_attr_ser_reg_addr);
}

/**
 * csd_register_mfg_tests() - Create sysfs interface for mfg tests
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_register_mfg_tests(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;
	int ret;

	ret = device_create_file(dev, &dev_attr_ctrl_test);
	if (ret) {
		csd_event_buffer_unload(csd);
		goto cleanup_pre_ctrl_test;
	}

	ret = device_create_file(dev, &dev_attr_gpio_test);
	if (ret) {
		csd_event_buffer_unload(csd);
		goto cleanup_ctrl_test;
	}

	ret = device_create_file(dev, &dev_attr_errb_test);
	if (ret) {
		csd_event_buffer_unload(csd);
		goto cleanup_gpio_test;
	}

	return 0;

cleanup_gpio_test:

	device_remove_file(dev, &dev_attr_gpio_test);

cleanup_ctrl_test:

	device_remove_file(dev, &dev_attr_ctrl_test);

cleanup_pre_ctrl_test:

	dev_err(dev, "Failed to register MFG testing interface\n");

	return ret;
}

/**
 * csd_unregister_mfg_tests() - Remove sysfs interface for mfg tests
 * @csd: pointer to csd_data
 */
static void csd_unregister_mfg_tests(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;

	device_remove_file(dev, &dev_attr_errb_test);
	device_remove_file(dev, &dev_attr_gpio_test);
	device_remove_file(dev, &dev_attr_ctrl_test);
}

static ssize_t line_fault_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	u8 line_status;
	int ret;

	ret = csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_LF_READ, &line_status);

	if (ret)
		return ret;

	return sprintf(buf, "0x%02hhX\n", line_status);
}
static DEVICE_ATTR_RO(line_fault);

static ssize_t hdcp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_event_read_hdcp_buffer hdcp_status;
	int ret;

	if (no_hdcp || !csd_ser_supports_hdcp(drvdata->ser_info))
		return sprintf(buf, "unsupported\n");

	ret = csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_READ_HDCP, &hdcp_status);

	if (ret)
		return ret;

	if (!hdcp_status.des_support)
		return sprintf(buf, "unsupported\n");

	if (hdcp_status.enabled) {
		if (hdcp_status.show_bksv)
			return sprintf(buf, "on (BKSV: %02hhX%02hhX%02hhX%02hhX%02hhX)\n",
				hdcp_status.bksv[4],
				hdcp_status.bksv[3],
				hdcp_status.bksv[2],
				hdcp_status.bksv[1],
				hdcp_status.bksv[0]);
		else
			return sprintf(buf, "on\n");
	}

	return sprintf(buf, "off\n");
}

static ssize_t hdcp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);

	if (no_hdcp || !csd_ser_supports_hdcp(drvdata->ser_info))
		return -ENOTSUPP;

	if (drvdata->ser_info->hdcp.check_req) {
		dev_err_once(dev, "HDCP encryption needs to be triggered over auxiliary interface of video link\n");
		return -ENOTSUPP;
	}

	if (count < 2 || count > 3 || bcmp(buf, "on\n", count))
		dev_warn_once(dev, "Prefer writing \"on\" to trigger HDCP encryption\n");

	mutex_lock(&drvdata->user_hdcp.lock);

	drvdata->user_hdcp.request = true;

	mutex_unlock(&drvdata->user_hdcp.lock);

	return count;
}
static DEVICE_ATTR_RW(hdcp);

static ssize_t edid_read(struct file *file, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t pos, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	int ret;

	if (pos || count != sizeof(struct csd_msg_display_edid)) {
		dev_err(dev, "Partial reads are not supported\n");
		return -ESPIPE;
	}

	ret = csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_READ_EDID, buf);
	if (ret)
		return ret;

	return sizeof(struct csd_msg_display_edid);
}
static BIN_ATTR_RO(edid, sizeof(struct csd_msg_display_edid));

static irqreturn_t csd_int_irq_handler(int irq, void *data)
{
	struct device *dev = data;
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);

	csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_INT_IRQ, NULL);

	return IRQ_HANDLED;
}

static int csd_panel_disable(struct drm_panel *panel)
{
	struct device *dev = panel->dev;
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	int ret;

	dev_dbg(dev, "Display disable request received\n");

	ret = csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_DISABLE, NULL);

	dev_dbg(dev, "Display disable request completed\n");

	return ret;
}

static int csd_panel_unprepare(struct drm_panel *panel)
{
	struct device *dev = panel->dev;
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	int ret;

	dev_dbg(dev, "Display post-disable request received\n");

	ret = csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_POST_DISABLE, NULL);

	dev_dbg(dev, "Display post-disable request completed\n");

	return ret;
}

static int csd_panel_prepare(struct drm_panel *panel)
{
	struct device *dev = panel->dev;
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	int ret;

	dev_dbg(dev, "Display pre-enable request received\n");

	ret = csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_PRE_ENABLE, NULL);

	dev_dbg(dev, "Display pre-enable request completed\n");

	return ret;
}

static int csd_panel_enable(struct drm_panel *panel)
{
	struct device *dev = panel->dev;
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	int ret;

	dev_dbg(dev, "Display enable request received\n");

	ret = csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_ENABLE, NULL);

	dev_dbg(dev, "Display enable request completed\n");

	return ret;
}

static int csd_panel_get_modes(struct drm_panel *panel)
{
	struct device *dev = panel->dev;
	struct drm_connector *connector = panel->connector;
	u32 bus_format = MEDIA_BUS_FMT_RGB444_1X12;
	struct drm_display_mode *mode;

	dev_dbg(dev, "Display modes requested\n");

	mode = drm_mode_duplicate(connector->dev, csd_get_display_mode());

	if (!mode) {
		dev_err(dev, "Failed to add display mode\n");
		return 0;
	}

	mode->type |= DRM_MODE_TYPE_PREFERRED;

	drm_mode_set_name(mode);

	drm_mode_probed_add(connector, mode);

	if (gmsl2) {
		connector->display_info.width_mm = 170;
		connector->display_info.height_mm = 226;
	} else {
		connector->display_info.width_mm = 138;
		connector->display_info.height_mm = 184;
	}
	connector->display_info.bpc = 4;
	drm_display_info_set_bus_formats(&connector->display_info, &bus_format,
		1);

	return 1;
}

static const struct drm_panel_funcs csd_panel_funcs = {
	.disable = csd_panel_disable,
	.unprepare = csd_panel_unprepare,
	.prepare = csd_panel_prepare,
	.enable = csd_panel_enable,
	.get_modes = csd_panel_get_modes,
};

/**
 * csd_register_panel() - Add panel to global panel registry
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
static int csd_register_panel(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;

	drm_panel_init(&csd->panel);
	csd->panel.dev = dev;
	csd->panel.funcs = &csd_panel_funcs;

	csd->bridge = drm_panel_bridge_add(&csd->panel,
		DRM_MODE_CONNECTOR_VIRTUAL);

	if (IS_ERR(csd->bridge))
		return PTR_ERR(csd->bridge);

	drm_panel_add(&csd->panel);

	return 0;
}

/**
 * csd_unregister_panel() - Remove panel from the global registry
 * @csd: pointer to csd_data
 */
static void csd_unregister_panel(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;

	dev_info(dev, "Waiting for graphics driver to release drm_panel\n");

	drm_panel_remove(&csd->panel);

	drm_panel_bridge_remove(csd->bridge);

	dev_info(dev, "drm_panel removed\n");
}

/**
 * csd_setup() - Perform setup tasks
 * @csd: pointer to csd_data
 *
 * Return: 0 on success, negative error code otherwise
 */
static enum csd_state csd_setup(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;
	int ret;

	csd->gpio_int_irq = gpiod_to_irq(csd->gpio_int);
	if (csd->gpio_int_irq < 0) {
		dev_err(dev, "Failed to translate int GPIO to irq\n");
		return csd->gpio_int_irq;
	}

	/*
	 * In GMSL2 mode our interrupt line goes high as soon as the GPIO
	 * on serializer side is configured during _power_on configuration.
	 * This happens even if there is no deserializer connected at all.
	 * There doesn't seem to be any way to set low as default for the pin.
	 *
	 * When this happens interrupts were disabled by calling disable_irq()
	 * before. In order to actually drop the rising edge as expected
	 * we need to disable lazy interrupt disabling. Depending on the system
	 * it may otherwise happen that the interrupt is marked as pending
	 * during the disabled state and we will receive it as soon as we call
	 * enable_irq() later.
	 */
	irq_set_status_flags(csd->gpio_int_irq, IRQ_DISABLE_UNLAZY);

	ret = devm_request_threaded_irq(dev, csd->gpio_int_irq, NULL,
		csd_int_irq_handler, IRQF_TRIGGER_RISING |
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT, NULL, dev);
	if (ret) {
		dev_err(dev, "Failed to request int interrupt\n");
		csd_event_buffer_unload(csd);
		return ret;
	}

	/*
	 * Interrupt is marked as masked in our event buffer. We can safely
	 * disable it as our handler will never block at this point.
	 */
	disable_irq(csd->gpio_int_irq);

	csd_expose_dev_to_cdev(csd->index, dev);

	csd->csd_dev = device_create(csd_global.class, dev,
		MKDEV(MAJOR(csd_global.major),
		CSD_BASE_MINOR + csd->index),
		&csd->drvdata, "csd%i", csd->index);

	if (IS_ERR(csd->csd_dev)) {
		dev_err(dev, "Failed to create new device\n");
		ret = PTR_ERR(csd->csd_dev);
		csd_event_buffer_unload(csd);
		goto cleanup_expose_to_cdev;
	}

	ret = csd_register_input_device(csd);
	if (ret) {
		csd_event_buffer_unload(csd);
		goto cleanup_device;
	}

	ret = csd_register_brightness_control(csd);
	if (ret) {
		csd_event_buffer_unload(csd);
		goto cleanup_input_dev;
	}

	ret = csd_register_ee_access(csd);
	if (ret)
		goto cleanup_brightness;

	ret = csd_register_mfg_tests(csd);
	if (ret)
		goto cleanup_ee_access;

	ret = device_create_file(dev, &dev_attr_line_fault);
	if (ret) {
		csd_event_buffer_unload(csd);
		goto cleanup_mfg_tests;
	}

	ret = device_create_bin_file(csd->csd_dev, &bin_attr_edid);
	if (ret) {
		csd_event_buffer_unload(csd);
		goto cleanup_line_fault;
	}

	ret = device_create_file(csd->csd_dev, &dev_attr_hdcp);
	if (ret) {
		csd_event_buffer_unload(csd);
		goto cleanup_edid;
	}

	/*
	 * Register diagnostic sysfs entries
	 * sysfs show callbacks succeed iff display status is exposed
	 */
	ret = csd_register_diag_sysfs(csd);
	if (ret) {
		csd_event_buffer_unload(csd);
		goto cleanup_hdcp;
	}

	return 0;

cleanup_hdcp:
	device_remove_file(csd->csd_dev, &dev_attr_hdcp);

cleanup_edid:

	device_remove_bin_file(csd->csd_dev, &bin_attr_edid);

cleanup_line_fault:

	device_remove_file(dev, &dev_attr_line_fault);

cleanup_mfg_tests:

	csd_unregister_mfg_tests(csd);

cleanup_ee_access:

	csd_unregister_ee_access(csd);

cleanup_brightness:

	csd_unregister_brightness_control(csd);

cleanup_input_dev:

	csd_unregister_input_device(csd);

cleanup_device:

	device_destroy(csd_global.class, MKDEV(MAJOR(csd_global.major),
		CSD_BASE_MINOR + csd->index));

cleanup_expose_to_cdev:

	csd_unexpose_dev_from_cdev(csd->index);

	return ret;
}

/**
 * csd_tear_down() - Performs a tear down procedure
 * @csd: pointer to csd_data
 *
 * This cleans up everything done in csd_setup().
 */
static void csd_tear_down(struct csd_data *csd)
{
	struct device *dev = &csd->serdev->dev;

	csd_unregister_diag_sysfs(csd);

	device_remove_file(csd->csd_dev, &dev_attr_hdcp);

	device_remove_bin_file(csd->csd_dev, &bin_attr_edid);

	device_remove_file(dev, &dev_attr_line_fault);

	csd_unregister_mfg_tests(csd);

	csd_unregister_ee_access(csd);

	csd_unregister_brightness_control(csd);

	csd_unregister_input_device(csd);

	device_destroy(csd_global.class, MKDEV(MAJOR(csd_global.major),
		CSD_BASE_MINOR + csd->index));

	csd_unexpose_dev_from_cdev(csd->index);
}

/**
 * csd_main_thread() - entry point of main event processing thread
 * @data: pointer to &struct csd_data
 *
 * Return: 0
 */
static int csd_main_thread(void *data)
{
	struct csd_data *csd = data;
	struct device *dev = &csd->serdev->dev;
	enum csd_state state = CSD_STATE_PROBED_CRESET;
	struct csd_state_buffer state_buffer;
	struct sched_param sched = {
		.sched_priority = MAX_USER_RT_PRIO / 4,
	};

	memset(&state_buffer, 0, sizeof(state_buffer));
	state_buffer.line_status = CSD_LINE_STATUS_UNKNOWN;
	state_buffer.last_diag = ns_to_ktime(0);

	/* Set real time priority for our main event processing thread */
	if (sched_setscheduler(current, SCHED_FIFO, &sched))
		dev_warn(dev, "Failed to set thread priority\n");

	while (true) {

		dev_dbg(dev, "Entering state %s\n", csd_get_state_name(state));

		switch (state) {
		case CSD_STATE_PROBED_CRESET:
			state = csd_state_probed_creset(csd, &state_buffer);
			break;
		case CSD_STATE_SOFF_CRESET:
			state = csd_state_soff_creset(csd, &state_buffer);
			break;
		case CSD_STATE_SCONF_CRESET:
			state = csd_state_sconf_creset(csd, &state_buffer);
			break;
		case CSD_STATE_SRECV_CRESET:
			state = csd_state_srecv_creset(csd, &state_buffer);
			break;
		case CSD_STATE_CSD_POWERING_ON:
			state = csd_state_csd_powering_on(csd, &state_buffer);
			break;
		case CSD_STATE_CSD_PBL:
			state = csd_state_csd_pbl(csd, &state_buffer);
			break;
		case CSD_STATE_CSD_PBL_APP_EARLY:
			state = csd_state_csd_pbl_app_early(csd, &state_buffer);
			break;
		case CSD_STATE_CSD_PBL_APP_LATE:
			state = csd_state_csd_pbl_app_late(csd, &state_buffer);
			break;
		case CSD_STATE_CSD_APP:
			state = csd_state_csd_app(csd, &state_buffer);
			break;
		case CSD_STATE_IDLE:
			state = csd_state_idle(csd, &state_buffer);
			break;
		case CSD_STATE_CSD_PROG:
			state = csd_state_csd_prog(csd, &state_buffer);
			break;
		case CSD_STATE_CSD_REG_PROG:
			state = csd_state_csd_reg_prog(csd, &state_buffer);
			break;
		case CSD_STATE_CSD_PROG_REG_PROG:
			state = csd_state_csd_prog_reg_prog(csd, &state_buffer);
			break;
		case CSD_STATE_RESETTING_CSD:
			state = csd_state_resetting_csd(csd, &state_buffer);
			break;
		case CSD_STATE_CONVERTER:
			state = csd_state_converter(csd, &state_buffer);
			break;
		case CSD_STATE_WAIT_CONVERTER:
			state = csd_state_wait_converter(csd, &state_buffer);
			break;
		case CSD_STATE_RESET_SER:
			state = csd_state_reset_ser(csd, &state_buffer);
			break;
		case CSD_STATE_SUSPENDED:
			state = csd_state_suspended(csd, &state_buffer);
			break;
		case CSD_STATE_TERMINATE:
			dev_info(dev, "Terminate %s\n", __func__);
			return 0;
		}
	}
}

/**
 * csd_global_get_index() - Returns an available index and marks it as used
 *
 * To be able to handle multiple displays each display gets an index assigned.
 * This function returns an available index number and marks it as used.
 *
 * Return: index or negative error code
 */
static int csd_global_get_index(void)
{
	int ret = -EMFILE;
	int i;

	mutex_lock(&csd_global.refs.lock);

	for (i = 0; i < CSD_MAX_DEVICE_COUNT; i++)
		if (!(csd_global.refs.devices_in_use & BIT(i))) {
			csd_global.refs.devices_in_use |= BIT(i);
			ret = i;
			break;
		}

	mutex_unlock(&csd_global.refs.lock);

	return ret;
}

/**
 * csd_global_put_index() - Marks an index as unused
 * @index: index no longer in use
 */
static void csd_global_put_index(int index)
{
	mutex_lock(&csd_global.refs.lock);
	csd_global.refs.devices_in_use &= ~BIT(index);
	mutex_unlock(&csd_global.refs.lock);
}

static int csd_probe(struct serdev_device *serdev)
{
	struct device *dev = &serdev->dev;
	struct csd_data *csd;
	int ret;

	dev_info(dev, "Probing CSD in %s mode\n", gmsl2 ? "GMSL2" : "GMSL1");

	csd = devm_kzalloc(dev, sizeof(*csd), GFP_KERNEL);
	if (!csd)
		return -ENOMEM;

	serdev_device_set_drvdata(serdev, &csd->drvdata);

	csd->serdev = serdev;

	mutex_init(&csd->drvdata.rx.lock);
	csd->drvdata.rx.state = CSD_RECEIVER_IDLE;

	mutex_init(&csd->drvdata.event_buffer.lock);
	csd->drvdata.event_buffer.int_irq_masked = true;
	plist_head_init(&csd->drvdata.event_buffer.queue);

	mutex_init(&csd->drvdata.display_status.lock);

	mutex_init(&csd->drvdata.brightness.lock);
	csd->drvdata.brightness.brightness = init_brightness;

	mutex_init(&csd->drvdata.user_hdcp.lock);

	mutex_init(&csd->drvdata.ee_ser_access.lock);
	mutex_init(&csd->drvdata.ee_des_access.lock);

	init_completion(&csd->drvdata.resume);

	csd->gpio_power_down = devm_gpiod_get(dev, "power_down",
		GPIOD_OUT_LOW);
	if (IS_ERR(csd->gpio_power_down)) {
		dev_err(dev, "Failed to get power down GPIO\n");
		return PTR_ERR(csd->gpio_power_down);
	}

	csd->gpio_gmsl_mode = devm_gpiod_get_optional(dev, "gmsl_mode",
		GPIOD_OUT_LOW);
	if (IS_ERR(csd->gpio_gmsl_mode)) {
		dev_err(dev, "Failed to get GMSL mode GPIO\n");
		return PTR_ERR(csd->gpio_gmsl_mode);
	}
	if (!csd->gpio_gmsl_mode)
		dev_dbg(dev, "GMSL mode GPIO not assigned\n");

	csd->gpio_ctrl = devm_gpiod_get(dev, "csd_ctrl", GPIOD_OUT_LOW);
	if (IS_ERR(csd->gpio_ctrl)) {
		dev_err(dev, "Failed to get control GPIO\n");
		return PTR_ERR(csd->gpio_ctrl);
	}

	csd->gpio_int = devm_gpiod_get(dev, "csd_int", GPIOD_IN);
	if (IS_ERR(csd->gpio_int)) {
		dev_err(dev, "Failed to get interrupt GPIO\n");
		return PTR_ERR(csd->gpio_int);
	}

	csd->gpio_errb = devm_gpiod_get(dev, "errb", GPIOD_IN);
	if (IS_ERR(csd->gpio_errb)) {
		dev_err(dev, "Failed to get ERRB GPIO\n");
		return PTR_ERR(csd->gpio_errb);
	}

	csd->gpio_lock = devm_gpiod_get_optional(dev, "lock", GPIOD_IN);
	if (IS_ERR(csd->gpio_lock)) {
		dev_err(dev, "Failed to get LOCK GPIO\n");
		return PTR_ERR(csd->gpio_lock);
	}
	if (!csd->gpio_lock)
		dev_dbg(dev, "LOCK GPIO not assigned\n");

	csd->last_msg = ns_to_ktime(0);

	csd->csd_ctrl_time = ns_to_ktime(1);

	csd->index = csd_global_get_index();
	if (csd->index < 0) {
		dev_err(dev, "Too many displays attached\n");
		return csd->index;
	}

	/*
	 * The length of the reset pulse required by CSD is so extremely
	 * large, that we realistically need to start worry, if CSD had
	 * enough time to enter reset state, since the system started up.
	 * On IHU 4.0 the wakeup line is accidentally raised by VIP before
	 * pin muxing is applied on SOC side, as the line is driven high by
	 * a pull-up initially, which is required as boot strap. Make sure
	 * CSD had enough time to reenter reset state since that happened.
	 */
	csd_wait_until_csd_in_reset(csd);

	/*
	 * The serializer needs to be released from reset before
	 * serdev_device_open() is called to avoid having a break condition
	 * present on the UART RX line while initializing the UART device.
	 */

	csd_power_up_ser(csd);

	serdev_device_set_client_ops(serdev, &csd_serdev_device_ops);

	ret = serdev_device_open(serdev);
	if (ret)
		goto cleanup_power_up;

	csd_configure_serdev(serdev);

	csd->drvdata.ser_info = csd_detect_serializer(csd,
		&csd->drvdata.ser_rev);

	if (!csd->drvdata.ser_info) {
		ret = -ENODEV;
		goto cleanup_serdev_open;
	}

	ret = csd_serializer_power_on(csd);
	if (ret)
		goto cleanup_serdev_open;

	ret = device_create_file(dev, &dev_attr_serializer);
	if (ret)
		goto cleanup_serdev_open;

	ret = device_create_file(dev, &dev_attr_serializer_rev);
	if (ret)
		goto cleanup_expose_serializer;

	/*
	 * Once this succeeds there might be pending events. In case we fail
	 * to create the main thread we need to discard the pending events
	 * explicitly by calling csd_event_buffer_unload() to unblock any
	 * threads which might be waiting for pending events to be processed.
	 */
	ret = csd_setup(csd);
	if (ret)
		goto cleanup_expose_serializer_rev;

	ret = csd_register_panel(csd);
	if (ret) {
		csd_event_buffer_unload(csd);
		goto cleanup_csd_setup;
	}

	if (kobject_uevent(&dev->kobj, KOBJ_CHANGE))
		dev_warn(dev, "Sending uevent KOBJ_CHANGE failed\n");

	if (kobject_uevent(&csd->csd_dev->kobj, KOBJ_CHANGE))
		dev_warn(csd->csd_dev,
			"Sending uevent KOBJ_CHANGE failed\n");

	csd->main_thread = kthread_run(csd_main_thread, csd, "CSD event loop");
	if (IS_ERR(csd->main_thread)) {
		ret = PTR_ERR(csd->main_thread);
		csd_event_buffer_unload(csd);
		goto cleanup_register_panel;
	}

	return 0;

cleanup_register_panel:
	csd_unregister_panel(csd);

cleanup_csd_setup:
	csd_tear_down(csd);

cleanup_expose_serializer_rev:

	device_remove_file(dev, &dev_attr_serializer_rev);

cleanup_expose_serializer:

	device_remove_file(dev, &dev_attr_serializer);

cleanup_serdev_open:

	serdev_device_close(serdev);

cleanup_power_up:

	csd_power_down_ser(csd);

	csd_global_put_index(csd->index);

	return ret;
}

static void csd_remove(struct serdev_device *serdev)
{
	struct device *dev = &serdev->dev;
	struct csd_drvdata *drvdata = serdev_device_get_drvdata(serdev);
	struct csd_data *csd = container_of(drvdata, struct csd_data, drvdata);

	csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_UNLOAD, NULL);

	kthread_stop(csd->main_thread);

	csd_unregister_panel(csd);

	csd_tear_down(csd);

	device_remove_file(dev, &dev_attr_serializer_rev);

	device_remove_file(dev, &dev_attr_serializer);

	if (csd->serdev)
		serdev_device_close(csd->serdev);

	csd_global_put_index(csd->index);

	/* Just in case we get probed again right away */
	csd_wait_until_csd_in_reset(csd);
}

/**
 * csd_cdev_ioctl_get_diag_report() - Handles a CSD_GET_DIAG_REPORT ioctl
 * @dev: device structure associated with display
 * @arg: argument provided by user space
 *
 * This function generates a CSD_EVENT_DIAG_REPORT event from a corresponding
 * diagnostic request received from user space, waits for the event to be
 * processed and copies the response (if any) back to user space buffer.
 *
 * Return: length of response (may be 0) or negative error code
 */
static long csd_cdev_ioctl_get_diag_report(struct device *dev, void __user *arg)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	struct csd_get_diag_report_arg ioctl_arg;
	struct csd_event_diag_rep_buffer rep_buffer;
	long ret;

	if (copy_from_user(&ioctl_arg, arg, sizeof(ioctl_arg))) {
		dev_err(dev, "Copy from user space failed\n");
		return -EFAULT;
	}

	if (ioctl_arg.timeout < 0 ||
		ioctl_arg.timeout > CSD_GET_DIAG_REPORT_MAX_TIMEOUT) {

		dev_err(dev, "Invalid timeout value\n");
		return -EINVAL;
	}

	if (ioctl_arg.payload_len <= 0 ||
		ioctl_arg.payload_len > CSD_MSG_DIAG_MAX_PAYLOAD_SIZE) {

		dev_err(dev, "Invalid payload size\n");
		return -EINVAL;
	}

	if (!ioctl_arg.request_ptr) {
		dev_err(dev, "Request pointer must not be NULL\n");
		return -EFAULT;
	}

	if (ioctl_arg.timeout) {
		if (!ioctl_arg.response_ptr) {
			dev_err(dev,
				"Response pointer must not be NULL if timeout is not zero\n");
			return -EFAULT;
		}

		if (ioctl_arg.response_buffer_size <= 0) {
			dev_err(dev,
				"Response buffer size must be larger than zero if timeout is not zero\n");
			return -EFAULT;
		}
	}

	rep_buffer.req = kmalloc((size_t) ioctl_arg.payload_len, GFP_KERNEL);
	if (!rep_buffer.req)
		return -ENOMEM;

	if (copy_from_user(rep_buffer.req,
		(void __user *) ioctl_arg.request_ptr,
		(unsigned long) ioctl_arg.payload_len)) {

		dev_err(dev, "Copy of request from user space failed\n");
		ret = -EFAULT;
		goto cleanup_req_buffer;
	}

	print_hex_dump_debug("csd ioctl req: ", DUMP_PREFIX_NONE,
		16, 1, rep_buffer.req, ioctl_arg.payload_len, false);

	rep_buffer.req_size = (u16) ioctl_arg.payload_len;
	rep_buffer.req_send = 0;

	if (ioctl_arg.timeout) {
		rep_buffer.resp_size = min(ioctl_arg.response_buffer_size,
			CSD_MSG_DIAG_MAX_PAYLOAD_SIZE);

		rep_buffer.resp = kmalloc(rep_buffer.resp_size, GFP_KERNEL);

		if (!rep_buffer.resp) {
			ret = -ENOMEM;
			goto cleanup_req_buffer;
		}
	} else {
		rep_buffer.resp_size = 0;
		rep_buffer.resp = NULL;
	}
	rep_buffer.resp_expected = 0;
	rep_buffer.resp_recv = 0;

	rep_buffer.expire = msecs_to_jiffies(ioctl_arg.timeout) + 1;
	rep_buffer.retry = 0;

	ret = (long) csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_DIAG_REPORT, &rep_buffer);

	if (!ret) {
		if (rep_buffer.resp_recv) {
			print_hex_dump_debug("csd ioctl res: ",
				DUMP_PREFIX_NONE, 16, 1, rep_buffer.resp,
				rep_buffer.resp_recv, false);

			if (copy_to_user((void __user *) ioctl_arg.response_ptr,
				rep_buffer.resp, rep_buffer.resp_recv)) {

				dev_err(dev,
					"Failed to copy response to user\n");
				ret = -EFAULT;
			} else {
				ret = (long) rep_buffer.resp_recv;
			}
		}
	}

	kfree(rep_buffer.resp);

cleanup_req_buffer:
	kfree(rep_buffer.req);

	return ret;
}

static long csd_cdev_unlocked_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	struct device *dev = filp->private_data;

	switch (cmd) {

	case CSD_GET_DIAG_REPORT:

		if (!arg) {
			dev_err(dev, "Argument is required for this ioctl\n");
			return -EINVAL;
		}

		return csd_cdev_ioctl_get_diag_report(dev, (void __user *) arg);

	}

	dev_err(dev, "Used ioctl command not supported\n");

	return -EINVAL;
}

static int csd_cdev_open(struct inode *inode, struct file *filp)
{
	int index;

	if (MINOR(inode->i_rdev) < CSD_BASE_MINOR)
		return -ENXIO;

	index = MINOR(inode->i_rdev) - CSD_BASE_MINOR;

	if (index > CSD_MAX_DEVICE_COUNT)
		return -ENXIO;

	mutex_lock(&csd_global.drvdata[index].lock);

	if (!csd_global.drvdata[index].dev) {
		mutex_unlock(&csd_global.drvdata[index].lock);
		return -ENXIO;
	}

	filp->private_data = csd_global.drvdata[index].dev;

	kref_get(&csd_global.drvdata[index].ref_count);

	mutex_unlock(&csd_global.drvdata[index].lock);

	return 0;
}

static int csd_cdev_release(struct inode *inode, struct file *filp)
{
	int index = MINOR(inode->i_rdev) - CSD_BASE_MINOR;

	mutex_lock(&csd_global.drvdata[index].lock);

	kref_put(&csd_global.drvdata[index].ref_count, csd_cdev_ref_release);

	mutex_unlock(&csd_global.drvdata[index].lock);

	return 0;
}

static const struct file_operations csd_cdev_fops = {
	.unlocked_ioctl = csd_cdev_unlocked_ioctl,
	.open = csd_cdev_open,
	.release = csd_cdev_release,
};

static const struct of_device_id csd_of_match[] = {
	{ .compatible = "alpine,csd15" },
	{ .compatible = "alpine,csd30" },
	{ .compatible = "alpine,csd35" },
	{ .compatible = "lg,csd31" },
	{ }
};
MODULE_DEVICE_TABLE(of, csd_of_match);

static const struct acpi_device_id csd_acpi_match[] = {
	{ "MXIM6785", 0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, csd_acpi_match);

static void csd_shutdown(struct device *dev)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);

	csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_UNLOAD, NULL);

	dev_info(dev, "Shutdown complete\n");
}

static int csd_suspend(struct device *dev)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);
	int ret;

	ret = csd_event_enqueue_and_wait(dev, &drvdata->event_buffer,
		CSD_EVENT_SUSPEND, NULL);
	if (ret)
		dev_warn(dev, "Suspend request rejected / suspend failed\n");

	return ret;
}

static int csd_resume(struct device *dev)
{
	struct csd_drvdata *drvdata = dev_get_drvdata(dev);

	dev_dbg(dev, "Resume requested\n");

	complete(&drvdata->resume);

	return 0;
}

static const struct dev_pm_ops csd_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(csd_suspend, csd_resume)
};

static struct serdev_device_driver csd_drv = {
	.driver = {
		.name = "csd",
		.of_match_table = csd_of_match,
		.acpi_match_table = ACPI_PTR(csd_acpi_match),
		.shutdown = csd_shutdown,
		.pm = &csd_pm_ops,
	},
	.probe = csd_probe,
	.remove = csd_remove,
};

static int __init csd_drv_init(void)
{
	int ret;
	int i;

	if (conv_timing[0] && conv_timing[4] && conv_timing[8]) {
		u32 flags;

		csd_display_mode_conv_timing.clock = conv_timing[0];

		csd_display_mode_conv_timing.hdisplay = conv_timing[1];
		csd_display_mode_conv_timing.hsync_start = conv_timing[2];
		csd_display_mode_conv_timing.hsync_end = conv_timing[3];
		csd_display_mode_conv_timing.htotal = conv_timing[4];

		csd_display_mode_conv_timing.vdisplay = conv_timing[5];
		csd_display_mode_conv_timing.vsync_start = conv_timing[6];
		csd_display_mode_conv_timing.vsync_end = conv_timing[7];
		csd_display_mode_conv_timing.vtotal = conv_timing[8];

		if (conv_timing[9])
			flags = DRM_MODE_FLAG_PHSYNC;
		else
			flags = DRM_MODE_FLAG_NHSYNC;

		if (conv_timing[10])
			flags |= DRM_MODE_FLAG_PVSYNC;
		else
			flags |= DRM_MODE_FLAG_NVSYNC;

		csd_display_mode_conv_timing.flags = flags;

		pr_warn("csd: Using conv_timing " DRM_MODE_FMT "\n",
			DRM_MODE_ARG(&csd_display_mode_conv_timing));
	}

	if (init_brightness > CSD_MSG_DISPLAY_STATUS_MAX_BRIGHTNESS)
		init_brightness = CSD_MSG_DISPLAY_STATUS_MAX_BRIGHTNESS;

	mutex_init(&csd_global.refs.lock);

	ret = alloc_chrdev_region(&csd_global.major, CSD_BASE_MINOR,
		CSD_MAX_DEVICE_COUNT, "csd");
	if (ret) {
		pr_err("CSD: Failed to allocate chrdev region\n");
		return ret;
	}

	csd_global.class = class_create(THIS_MODULE, "csd");
	if (IS_ERR(csd_global.class)) {
		pr_err("CSD: Failed to create class\n");
		ret = PTR_ERR(csd_global.class);
		goto cleanup_region;
	}

	for (i = 0; i < CSD_MAX_DEVICE_COUNT; i++) {
		mutex_init(&csd_global.drvdata[i].lock);
		init_completion(&csd_global.drvdata[i].done);
	}

	cdev_init(&csd_global.cdev, &csd_cdev_fops);
	csd_global.cdev.owner = THIS_MODULE;

	/* User space access to cdev is possible from this point on */
	ret = cdev_add(&csd_global.cdev, csd_global.major,
		CSD_MAX_DEVICE_COUNT);
	if (ret) {
		pr_err("CSD: Failed to add character device\n");

		/* Undo kobject_init() from cdev_init() */
		kobject_put(&csd_global.cdev.kobj);

		goto cleanup_class;
	}

	ret = serdev_device_driver_register(&csd_drv);
	if (ret)
		goto cleanup_cdev;

	return 0;

cleanup_cdev:
	cdev_del(&csd_global.cdev);

cleanup_class:
	class_destroy(csd_global.class);

cleanup_region:
	unregister_chrdev_region(csd_global.major, CSD_MAX_DEVICE_COUNT);

	return ret;
}
module_init(csd_drv_init);

static void __exit csd_drv_exit(void)
{
	serdev_device_driver_unregister(&csd_drv);

	cdev_del(&csd_global.cdev);

	class_destroy(csd_global.class);

	unregister_chrdev_region(csd_global.major, CSD_MAX_DEVICE_COUNT);
}
module_exit(csd_drv_exit);

MODULE_AUTHOR("Oliver Barta <oliver.barta@aptiv.com>");
MODULE_DESCRIPTION("Volvo Cars CSD driver");
MODULE_LICENSE("GPL");
