======================
CSD user documentation
======================

module parameters
=================

gmsl2
-----

By default the serializer is configured to operate in GMSL1 mode, which is suitable for the smaller versions of the display. In order to operate in GMSL2 mode this parameter needs to be set to true.

ee_mode
-------

This mode is intended for development purpose in particular for tests performed by electrical engineering. By default the driver will prevent the user from doing certain operations, which may potentially lead to unpredictable behavior, which would be difficult to reproduce and to debug. The main example is performing a manual access from user space to serializer or deserializer registers, while the wakeup sequence of CSD is ongoing. By design this may lead to collisions on the UART interface and therefore produce unpredictable behavior. However it might be desirable to perform such operation e.g. in case there is some development converter board attached instead of a CSD or when nothing is attached at all. Please note that a manual access to serializer and deserializer registers is always possible independent of this setting once the wakeup sequence is completed or after a converter board was successfully detected. The driver will ensure proper synchronization to avoid collisions. Disabling this option will not automatically prevent a malicious predictable change of serializer or deserializer registers. It prevents just accesses, which may lead to unpredictable changes by design. Other security mechanisms like file permissions and SELinux policies should be used to prevent unauthorized access to the sysfs files used for manual register access.

For electrical engineering testing purposes it is often preferable to stop any communication with deserializer. Therefore detection and configuration of converter boards will not be done, when ee_mode is enabled. Converter boards, which require configuration, will not work right away, when connected while running in ee_mode. In such case it might be desirable to start without ee_mode enabled and to switch it on later. In general ee_mode can be turned on and off at run-time as desired.

conv_timing
-----------

For usage with converter boards this parameter allows to specify a custom display timing in the form "conv_timing=clock_kHz,hdisplay,hsync_start,hsync_end,htotal,vdisplay,vsync_start,vsync_end,vtotal,hsync_pol,vsync_pol". Providing a custom timing implies that a converter board is used. If an actual CSD is attached, it will be kept turned off, as using a custom timing with an actual CSD would operate the CSD out of spec and could cause (permanent) damage. Using ee_mode to allow manual register access is not required, when conv_timing is used, as a potentially attached CSD would be kept in reset state. Therefore collisions are not possible. Nevertheless ee_mode can be used in combination with conv_timing in case detection and configuration of converter boards is not desired.

brightness
----------

By default the CSD will be turned on with 50 % of the maximum brightness. This parameter can be used to change this default to any value between 0 and 1023.

no_hdcp
-------
Set this to true to disable use of HDCP independent of HDCP support provided by serializer and deserializer.

dyndbg
------

As usual this driver supports also dynamic debugging. When enabling dynamic debug output care must be taken that the console log level is set sufficiently low to avoid printing debug messages to serial console. When certain debug messages are enabled the amount of created logs will be very high during normal operation. In case the system needs to output this logs over some low bandwidth connection like a serial console the driver will not be able to meet the required timing to allow normal operation. Please use a high bandwidth connection like USB to transfer such debug logs.

sysfs interface
===============

The driver provides several sysfs files to expose information about serializer and CSD, to allow brightness control from user space, to provide debug access to serializer and deserializer and to trigger certain manufacturing tests.

.. flat-table:: main sysfs files

   * - file
     - access
     - description

   * - /sys/class/csd/csd0/brightness
     - RW
     - Display brightness can be controlled by writing a value between 0 and the value returned by max_brightness to this file. Reading this file returns the last value written or the default brightness if no write was performed before.

   * - /sys/class/csd/csd0/device/ctrl_test
     - WO
     - Writing '1' to this file asserts the control line (wakeup signal). Writing '0' deasserts the control line. This is intended to be done as part of manufacturing testing. To avoid any interference with normal operation, this test is allowed only while video output is turned off. Any write to this file, while video output is enabled, is rejected. After a '1' was written to this file, the control line is kept asserted until '0' is written or any other operation is triggered. E.g. running any other manufacturing test, turning on video or reading registers, so any other operation, will immediately abort this test and return the control line to deasserted state.

   * - /sys/class/csd/csd0/des_reg_addr
     - RW
     - This file is used for manual access to deserializer registers. It contains the address of the register, which should be accessed by reads or writes to des_reg_value.

   * - /sys/class/csd/csd0/des_reg_value
     - RW
     - This file is used for manual access to deserializer registers. Reading this file will trigger a read access to the register set with des_reg_addr. Writing this file will trigger a write with the given value to the register defined by des_reg_addr.

   * - /sys/class/csd/csd0/edid
     - RO
     - Reading this file returns the EDID blob provided by the display. The blob is not cached. Every read of this file directly leads to an EDID request being sent to CSD. This blob is provided for informational purposes only. It is not used and by design can't be used internally in any functional way.

   * - /sys/class/csd/csd0/device/errb_test
     - RO
     - DEPRECATED This interface is deprecated and will be removed in future. Please use gpio_test instead. Reading this file triggers a test to verify that the ERRB signal path is working. This test is intended to be run as part of the manufacturing process. Reading the file returns either "ok" or "fail" depending on the test result. In case running the test itself fails, the read of this file will fail. A retry should be performed in that case. Please note that by default this test can not be performed, when the display wakeup sequence is ongoing or if no display is connected. In order to run this test without a display please enable ee_mode or specify a conv_timing.

   * - /sys/class/csd/csd0/device/gpio_test
     - RO
     - Reading this file triggers a self test of all GPIO lines between SOC and serializer and returns the results in human readable format. This test is intended to be run as part of the manufacturing process. In order not to interfer with normal operation this test can be done only while video output is disabled. Reading this file while video output is enabled will fail. In case running the test itself fails, the read of this file will also fail. A retry should be performed in that case.

   * - /sys/class/csd/csd0/hdcp
     - RW
     - Reading this file returns the HDCP status. "unsupported" is returned if no_hdcp option was used or if HDCP is not supported by serializer or deserializer. "off" is returned if HDCP is supported by all components but currently disabled, which can be either intentional or due to an error. "on" is returned if HDCP is currently enabled on all parts of the chain from SOC to display, which support HDCP by design. When a video link like HDMI is used, which supports negotiation of HDCP encryption over an auxiliary channel (DDC for HDMI), HDCP encryption is and needs to be requested over the auxiliary channel. Writing to this file is not supported in this case. When a video link without auxiliary channel, like DSI, is used, HDCP encryption needs to be requested by writing "on" to this file and monitored by reading back this file periodically. After a write, it may take a short while until encryption is actually enabled and "on" is reported, when reading this file. Same timeout, retry and fallback strategies should be used as would be used, when using auxiliary channel. In particular a graceful fallback to non-HDCP operation is recommended in case of repeated failures to enable HDCP.

   * - /sys/class/csd/csd0/device/line_fault
     - RO
     - Reading this file returns the line fault status as 8 bit hex value. The upper four bits contain the status of the positive line and the lower 4 bits contain the status of the negative line. The meaning of each 4 bit value is 0x0: short to battery; 0x1: short to ground; 0x2: normal; 0x3: line open; 0x4: line to line short. Other values are not returned. Each access to this file triggers a read access to serializer. The line fault status is not buffered during normal operation. Reading this file will be typically fast during normal operation, but it may take up to several seconds, as the corresponding read operation can not be performed at any arbitrary point in time. It needs to be ensured that it doesn't conflict with a register access performed by CSD controller. A read performed while wakeup sequence is ongoing, will either fail or provide a cached value obtained shortly before wakeup sequence started, as reading line fault information is not possible during CSD wakeup. As wakeup sequence can potentially take several seconds, the value may obviously be outdated by several seconds during wakeup.

   * - /sys/class/csd/csd0/max_brightness
     - RO
     - Reading this file returns the maximum value for brightness, which is 1023.

   * - /sys/class/csd/csd0/device/ser_reg_addr
     - RW
     - This file is used for manual access to serializer registers. It contains the address of the register, which should be accessed by reads or writes to ser_reg_value.

   * - /sys/class/csd/csd0/device/ser_reg_value
     - RW
     - This file is used for manual access to serializer registers. Reading this file will trigger a read access to the register set with ser_reg_addr. Writing this file will trigger a write with the given value to the register defined by ser_reg_addr.

   * - /sys/class/csd/csd0/device/serializer
     - RO
     - Reading this file returns the human readable name of the detected serializer chip.

   * - /sys/class/csd/csd0/device/serializer_rev
     - RO
     - Reading this file returns the revision of the detected serializer chip.

.. flat-table:: additional diagnostic sysfs files showing cached state while CSD is operating

   * - file
     - access
     - description

   * - /sys/class/csd/csd0/actual_brightness
     - RO
     - Reading this file returns the actual brightness reported by CSD. The value is cached and reflects always the value reported in the last periodic status report from CSD.

   * - /sys/class/csd/csd0/consumption
     - RO
     - Reading this file returns the current consumption in mA reported by CSD. The value is cached and reflects always the value reported in the last periodic status report from CSD.

   * - /sys/class/csd/csd0/display_on
     - RO
     - Reading this file indicates whether the display is currently on or off. The value is cached and reflects always the value reported in the last periodic status report from CSD.

   * - /sys/class/csd/csd0/error_reg_1
     - RO
     - Reading this file returns the error flags reported by CSD in error byte 1. The value is cached and reflects always the value reported in the last periodic status report from CSD.

   * - /sys/class/csd/csd0/error_reg_2
     - RO
     - Reading this file returns the error flags reported by CSD in error byte 2. The value is cached and reflects always the value reported in the last periodic status report from CSD.

   * - /sys/class/csd/csd0/error_reg_3
     - RO
     - Reading this file returns the error flags reported by CSD in error byte 3. The value is cached and reflects always the value reported in the last periodic status report from CSD.

   * - /sys/class/csd/csd0/lvds_errors
     - RO
     - Reading this file returns the number of lvds errors reported by CSD. The value is cached and reflects always the value reported in the last periodic status report from CSD.

   * - /sys/class/csd/csd0/mode
     - RO
     - Reading this file returns the current mode reported by CSD. The value is cached and reflects always the value reported in the last periodic status report from CSD.

   * - /sys/class/csd/csd0/sleep_factor
     - RO
     - In case the CSD is in sleep mode reading this file returns the reason for entering sleep mode reported by CSD. The value is cached and reflects always the value reported in the last periodic status report from CSD.

   * - /sys/class/csd/csd0/temperature
     - RO
     - Reading this file returns the current temperature in °C reported by CSD. The value is cached and reflects always the value reported in the last periodic status report from CSD.

   * - /sys/class/csd/csd0/temperature_warn
     - RO
     - Reads as 1 when the CSD is reporting a temperature warning and 0 otherwise. The value is cached and reflects always the value reported in the last periodic status report from CSD.

   * - /sys/class/csd/csd0/voltage
     - RO
     - Reading this file returns the supply voltage reported by CSD in mV. The value is cached and reflects always the value reported in the last periodic status report from CSD.

diagnostic interface
====================

The CSD driver provides a character device named /dev/csd0 which can be used to send diagnostic commands to CSD. Commands are send using an ioctl call with CSD_GET_DIAG_REPORT command and a pointer to a :c:type:`struct csd_get_diag_report_arg <csd_get_diag_report_arg>` as argument.

.. code-block:: c

	#define CSD_GET_DIAG_REPORT               _IOWR('q', 1, struct csd_diag_arg *)

When a response is expected timeout needs to be set to non-zero value in ms and should be large enough to make sure that a response from CSD can be received within the given time but must not exceed 20 s. Time measurement is started once the request message was transmitted completely. When no response is expected, timeout must be set to zero. In case timeout is non-zero a response buffer with valid size needs to be provided, otherwise response buffer and its size are not used and not checked. A diagnostic ioctl call is always blocking. Response Pending messages received from CSD are silently dropped and user space is not informed. User space is responsible for generating Response Pending messages as required by Test equipment during the blocking time.

CSD can be switched to programming session by sending a corresponding diagnostic session control command. While CSD is in programming session, CSD driver doesn't send any periodic messages to CSD on its own. Control is handed over completely to user space until programming session is left. Programming session can be left by sending another diagnostic session control command or by sending a diagnostic reset command. User space is responsible to ensure that CSD doesn't run into a timeout while being in programming session due to missing communication.

.. kernel-doc:: drivers/gpu/drm/panel/csd/csd_base.h
