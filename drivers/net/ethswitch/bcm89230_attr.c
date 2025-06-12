#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/spi/spi.h>

#include "bcm89230_attr.h"
#include "bcm89230_core.h"
#include "bcm89230_actions.h"
#include "bcm89230_spi.h"

#define PORT0_ATTR_RW(_name) struct device_attribute port0_##_name##_attr = \
	__ATTR(_name, 0644, \
	sysfs_port0_##_name##_show, sysfs_port0_##_name##_store)

#define PORT0_ATTR_RO(_name) struct device_attribute port0_##_name##_attr = \
	__ATTR(_name, 0444, \
	sysfs_port0_##_name##_show, NULL)

#define PORT1_ATTR_RW(_name) struct device_attribute port1_##_name##_attr = \
	__ATTR(_name, 0644, \
	sysfs_port1_##_name##_show, sysfs_port1_##_name##_store)

#define PORT1_ATTR_RO(_name) struct device_attribute port1_##_name##_attr = \
	__ATTR(_name, 0444, \
	sysfs_port1_##_name##_show, NULL)

#define PORT4_ATTR_RW(_name) struct device_attribute port4_##_name##_attr = \
	__ATTR(_name, 0644, \
	sysfs_port4_##_name##_show, sysfs_port4_##_name##_store)

#define PORT4_ATTR_RO(_name) struct device_attribute port4_##_name##_attr = \
	__ATTR(_name, 0444, \
	sysfs_port4_##_name##_show, NULL)

#define COMMON_ATTR_RW(_name) struct device_attribute common_##_name##_attr = \
	__ATTR(_name, 0644, \
	sysfs_common_##_name##_show, sysfs_common_##_name##_store)

#define COMMON_ATTR_RO(_name) struct device_attribute common_##_name##_attr = \
	__ATTR(_name, 0444, \
	sysfs_common_##_name##_show, NULL)

#define DEBUG_ATTR_RW(_name) struct device_attribute debug_##_name##_attr = \
	__ATTR(_name, 0644, \
	sysfs_debug_##_name##_show, sysfs_debug_##_name##_store)

typedef struct twocomm {
	const char *one;
	const char *two;
} twocomm;

#define TWOCOMM(name, _one, _two) \
static const twocomm name = { \
	.one = _one, \
	.two = _two, \
}

TWOCOMM(onoff, "ON\n", "OFF\n");
TWOCOMM(txrx, "TX\n", "RX\n");
TWOCOMM(speed0, "100\n", "DEF\n");
TWOCOMM(speed4, "100\n", "10\n");
TWOCOMM(ms, "MASTER\n", "SLAVE\n");
TWOCOMM(progress, "COMPLETE\n", "IN_PROGRESS\n");

ssize_t register_read(struct device *dev, ACTION_ITEM action, u64 *result)
{
	int ret_val;
	struct spi_device *spi = container_of(dev, struct spi_device, dev);

	dev_dbg(dev, "diagCommandExecute get_value %s ...\n", action.comment);
	if (perform_action_reg_returns(spi, &action, result)) {
		dev_warn(dev, "diagCommandExecute get_value %s FAILS\n", action.comment);
		return -ENXIO;
	}
	return 0;
}

int register_read_and_clear(struct device *dev, u32 register_address, u64 *reg_content)
{
	ACTION_ITEM get_value_act = { register_address, 32, GET_VALUE, MASK_FULL, 0, "Read" };
	ACTION_ITEM clear_value_act = { register_address, 32, SET_VALUE, MASK_FULL, 0, "Clear" };

	struct spi_device *spi = container_of(dev, struct spi_device, dev);

	dev_dbg(dev, "diagCommandExecute get_value and clear %x ...\n", register_address);
	if (perform_action_reg_returns(spi, &get_value_act, reg_content)) {
		dev_warn(dev, "diagCommandExecute get_value %x FAILS\n", register_address);
		return -ENXIO;
	}

	if (perform_action(spi, &clear_value_act)) {
		dev_warn(dev, "diagCommandExecute set_value %x FAILS\n", register_address);
		return -ENXIO;
	}
	return 0;
}

ssize_t register_set(struct device *dev, ACTION_ITEM action)
{
	struct spi_device *spi = container_of(dev, struct spi_device, dev);

	dev_dbg(&spi->dev, "diagCommandExecute %s ...\n", action.comment);
	if (perform_action(spi, &action)) {
		dev_warn(dev, "diagCommandExecute set_value %s FAILS\n", action.comment);
		return -ENXIO;
	}

	return 0;
}

ssize_t sysfs_get_value(struct device *dev, ACTION_ITEM action, char *buf)
{
	int ret_val;
	u64 result;
	if(-ENXIO != register_read(dev,action,&result)) {
		ret_val = scnprintf(buf, 30, "0x%llx\n", result);
		return ret_val;
	}
	return -ENXIO;
}

ssize_t sysfs_check_value(struct device *dev, ACTION_ITEM action_true, ACTION_ITEM action_false, char *buf, const struct twocomm *tc)
{
	int ret_val = 0;
	struct spi_device *spi = container_of(dev, struct spi_device, dev);

	ret_val = perform_action(spi, &action_true);

	if (ret_val == 0) {
		dev_dbg(dev, "diagCommandExecute check_value %s\n", action_true.comment);
		ret_val = sprintf(buf, tc->one);
		return ret_val;
	}

	ret_val = perform_action(spi, &action_false);

	if (ret_val == 0) {
		dev_dbg(dev, "diagCommandExecute check_value %s\n", action_false.comment);
		ret_val = sprintf(buf, tc->two);
		return ret_val;
	}

	dev_warn(dev, "diagCommandExecute check_value %s/%s = FAILS\n", action_true.comment, action_false.comment);
	return -ENXIO;
}

ssize_t sysfs_set_value(struct device *dev, ACTION_ITEM action_true, ACTION_ITEM action_false, const char *buf, const struct twocomm *tc)
{
	struct spi_device *spi = container_of(dev, struct spi_device, dev);

	if (strcmp(buf, tc->one) == 0) {
		dev_dbg(&spi->dev, "diagCommandExecute %s ...\n", action_true.comment);
		if (perform_action(spi, &action_true)) {
			dev_warn(dev, "diagCommandExecute set_value %s FAILS\n", action_true.comment);
			return -ENXIO;
		}
		return 0;
	}
	if (strcmp(buf, tc->two) == 0) {
		dev_dbg(&spi->dev, "diagCommandExecute %s ...\n", action_false.comment);
		if (perform_action(spi, &action_false)) {
			dev_warn(dev, "diagCommandExecute set_value %s FAILS\n", action_false.comment);
			return -ENXIO;
		}
		return 0;
	}
	dev_warn(&spi->dev, "diagCommandExecute %s WRONG param\n", action_true.comment);
	return -ENXIO;
}

ssize_t sysfs_port0_loopback_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x090FFFC0, 16, CHECK_VALUE, 0x4000, 0x4000, "Check Port 0 loopback mode on" };
	ACTION_ITEM action2 = { 0x090FFFC0, 16, CHECK_VALUE, 0x4000, 0x0000, "Check Port 0 loopback mode off" };

	return sysfs_check_value(dev, action1, action2, buf, &onoff);
}

ssize_t sysfs_port0_loopback_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x090FFFC0, 16, SET_VERIFY, 0x4000, 0x4000, "Set Port 0 to loopback mode on" };
	ACTION_ITEM action2 = { 0x090FFFC0, 16, SET_VERIFY, 0x4000, 0x0000, "Set Port 0 to loopback mode off" };

	ret_val = sysfs_set_value(dev, action1, action2, buf, &onoff);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_port0_power_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x090FFFC0, 16, CHECK_VALUE, 0x0800, 0x0000, "Check Port 0 power mode on" };
	ACTION_ITEM action2 = { 0x090FFFC0, 16, CHECK_VALUE, 0x0800, 0x0800, "Check Port 0 power mode off" };

	return sysfs_check_value(dev, action1, action2, buf, &onoff);
}

ssize_t sysfs_port0_power_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x090FFFC0, 16, SET_VERIFY, 0x0800, 0x0000, "Set Port 0 to power mode on" };
	ACTION_ITEM action2 = { 0x090FFFC0, 16, SET_VERIFY, 0x0800, 0x0800, "Set Port 0 to power mode off" };

	ret_val = sysfs_set_value(dev, action1, action2, buf, &onoff);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_port0_control_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action = { 0x090FFFC0, 16, GET_VALUE, MASK_FULL, 0, "Get Port 0 control register" };

	return sysfs_get_value(dev, action, buf);
}

ssize_t sysfs_port0_rmt_loopback_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x090F2058, 16, CHECK_VALUE, 0x8000, 0x8000, "Check Port 0 remote loopback mode on" };
	ACTION_ITEM action2 = { 0x090F2058, 16, CHECK_VALUE, 0x8000, 0x0000, "Check Port 0 remote loopback mode off" };

	return sysfs_check_value(dev, action1, action2, buf, &onoff);
}

ssize_t sysfs_port0_rmt_loopback_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x090F2058, 16, SET_VERIFY, 0x8000, 0x8000, "Set Port 0 to remote loopback mode on" };
	ACTION_ITEM action2 = { 0x090F2058, 16, SET_VERIFY, 0x8000, 0x0000, "Set Port 0 to remote loopback mode off" };

	ret_val = sysfs_set_value(dev, action1, action2, buf, &onoff);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_port0_speed_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x090FFFC0, 16, CHECK_VALUE, 0x03C0, 0x0200, "Check Port 0 Speed 100 Mbs/s" };
	ACTION_ITEM action2 = { 0x090FFFC0, 16, CHECK_VALUE, 0x03C0, 0x0000, "Check Port 0 Speed default setting" };

	return sysfs_check_value(dev, action1, action2, buf, &speed0);
}

ssize_t sysfs_port0_speed_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x090FFFC0, 16, SET_VERIFY, 0x03C0, 0x0200, "Set Port 0 Speed to 100 Mbs/s" };
	ACTION_ITEM action2 = { 0x090FFFC0, 16, SET_VERIFY, 0x03C0, 0x0000, "Set Port 0 Speed to default setting" };

	ret_val = sysfs_set_value(dev, action1, action2, buf, &speed0);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_port0_error_cnt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u64 result = 0;
	u64 reg_content;

	//Rx Aligment error packet counter
	if (register_read_and_clear(dev, 0x0B002080, &reg_content))
		return -ENXIO;
	result = result + reg_content >= result ? result + reg_content : MASK_FULL;

	//Rx FCS error packet counter
	if (register_read_and_clear(dev, 0x0B002084, &reg_content))
		return -ENXIO;
	result = result + reg_content >= result ? result + reg_content : MASK_FULL;

	//Rx Symbol error counter
	if (register_read_and_clear(dev, 0x0B0020AC, &reg_content))
		return -ENXIO;
	result = result + reg_content >= result ? result + reg_content : MASK_FULL;

	//Rx In Range error counter
	if (register_read_and_clear(dev, 0x0B0020B0, &reg_content))
		return -ENXIO;
	result = result + reg_content >= result ? result + reg_content : MASK_FULL;

	//Rx Out Range error counter
	if (register_read_and_clear(dev, 0x0B0020B4, &reg_content))
		return -ENXIO;
	result = result + reg_content >= result ? result + reg_content : MASK_FULL;

	return scnprintf(buf, 30, "%llx\n", result);
}

ssize_t sysfs_port0_packet_cnt_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x090F205E, 16, CHECK_VALUE, 0x0800, 0x0000, "Check Port 0 Packet Counter Mode TX" };
	ACTION_ITEM action2 = { 0x090F205E, 16, CHECK_VALUE, 0x0800, 0x0800, "Check Port 0 Packet Counter Mode RX" };

	return sysfs_check_value(dev, action1, action2, buf, &txrx);
}

ssize_t sysfs_port0_packet_cnt_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x090F205E, 16, SET_VERIFY, 0x0800, 0x0000, "Set Port 0 - Packet Counter Mode to TX" };
	ACTION_ITEM action2 = { 0x090F205E, 16, SET_VERIFY, 0x0800, 0x0800, "Set Port 0 - Packet Counter Mode to RX" };

	ret_val = sysfs_set_value(dev, action1, action2, buf, &txrx);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_port0_packet_cnt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action = { 0x090F2060, 16, GET_VALUE, MASK_FULL, 0, "Get Port 0 Packet Counter" };

	return sysfs_get_value(dev, action, buf);
}

ssize_t sysfs_port0_link_state_pass_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x090F201C, 16, CHECK_VALUE, 0x1000, 0x1000, "Check Port 0 - Force Link Pass State" };
	ACTION_ITEM action2 = { 0x090F201C, 16, CHECK_VALUE, 0x1000, 0x0000, "Check Port 0 - Normal Link Operation" };

	return sysfs_check_value(dev, action1, action2, buf, &onoff);
}

ssize_t sysfs_port0_link_state_pass_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x090F201C, 16, SET_VERIFY, 0x1000, 0x1000, "Set Port 0 - Force Link Pass State" };
	ACTION_ITEM action2 = { 0x090F201C, 16, SET_VERIFY, 0x1000, 0x0000, "Set Port 0 - Set Link to Normal Operation" };

	ret_val = sysfs_set_value(dev, action1, action2, buf, &onoff);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_port0_auto_mdix_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x0A0F205E, 16, CHECK_VALUE, 0x0200, 0x0200, "Check Port 0 - 100BASE-TX Forced Mode Auto-MDIX Enable" };
	ACTION_ITEM action2 = { 0x0A0F205E, 16, CHECK_VALUE, 0x0200, 0x0000, "Check Port 0 - 100BASE-TX Forced Mode Auto-MDIX Disable" };

	return sysfs_check_value(dev, action1, action2, buf, &onoff);
}

ssize_t sysfs_port0_auto_mdix_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x0A0F205E, 16, SET_VERIFY, 0x0200, 0x0200, "Set Port 0 - Set 100BASE-TX Forced Mode Auto-MDIX to Enable" };
	ACTION_ITEM action2 = { 0x0A0F205E, 16, SET_VERIFY, 0x0200, 0x0000, "Set Port 0 - Set 100BASE-TX Forced Mode Auto-MDIX to Disable" };

	ret_val = sysfs_set_value(dev, action1, action2, buf, &onoff);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_port0_cable_diag_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x090F2540, 16, CHECK_VALUE, 0X0800, 0X0000, "Check Port 0 Cable Diagnostics Status Complete" };
	ACTION_ITEM action2 = { 0x090F2540, 16, CHECK_VALUE, 0X0800, 0X0800, "Check Port 0 Cable Diagnostics Status In progress" };

	return sysfs_check_value(dev, action1, action2, buf, &progress);
}

ssize_t sysfs_port0_cable_diag_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct spi_device *spi = container_of(dev, struct spi_device, dev);
	ACTION_ITEM action = { 0x090F2540, 16, SET_VALUE, 0x8000, 0x8000, "Set Port 0 - Start Automotive Cable Diagnostics" };

	if (strcmp(buf, "START\n") == 0) {
		dev_dbg(dev, "diagCommandExecute %s ...\n", action.comment);

		if (perform_action(spi, &action)) {
			dev_warn(dev, "diagCommandExecute set_value %s FAILS\n", action.comment);
			return -ENXIO;
		}
		return count;
	}

	dev_warn(dev, "diagCommandExecute %s WRONG param\n", action.comment);
	return -ENXIO;
}

ssize_t sysfs_port0_cable_diag_fault_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action = { 0x090F2542, 16, GET_VALUE, MASK_FULL, 0x0000, "Get Port 0 Cable Diagnostics Fault Type" };

	return sysfs_get_value(dev, action, buf);
}

ssize_t sysfs_port0_cable_diag_len_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action = { 0x090F2544, 16, GET_VALUE, MASK_FULL, 0x0000, "Get Port 0 Cable Length or fault location" };

	return sysfs_get_value(dev, action, buf);
}

ssize_t sysfs_port0_SQI_read_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x090F2050, 16, CHECK_VALUE, 0x0800, 0x0800, "Check Port 0 - Mode ON for SQI data reading from registry" };
	ACTION_ITEM action2 = { 0x090F2050, 16, CHECK_VALUE, 0x0800, 0x0000, "Check Port 0 - Mode OFF for SQI data reading from registry" };

	return sysfs_check_value(dev, action1, action2, buf, &onoff);
}

ssize_t sysfs_port0_SQI_read_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x090F2050, 16, SET_VERIFY, 0x0800, 0x0800, "Set Port 0 - Mode ON for SQI data reading from registry" };
	ACTION_ITEM action2 = { 0x090F2050, 16, SET_VERIFY, 0x0800, 0x0000, "Set Port 0 - Mode OFF for SQI data reading from registry" };

	ret_val = sysfs_set_value(dev, action1, action2, buf, &onoff);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_port0_SQI_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action = { 0x090F2210, 16, GET_VALUE, MASK_FULL, 0, "Get Port 0 Signal Quality Indicator data for calculation" };

	return sysfs_get_value(dev, action, buf);
}

ssize_t sysfs_port0_RX_good_packet_cnt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u64 result = 0;
	u64 reg_content;

	//Rx Unicast packet counter
	if (register_read_and_clear(dev, 0x0B002094, &reg_content))
		return -ENXIO;
	result = result + reg_content >= result ? result + reg_content :MASK_FULL;

	//Rx Multicast packet counter
	if (register_read_and_clear(dev, 0x0B002098, &reg_content))
		return -ENXIO;
	result = result + reg_content >= result ? result + reg_content : MASK_FULL;

	//Rx Broadcast packet counter
	if (register_read_and_clear(dev, 0x0B00209C, &reg_content))
		return -ENXIO;
	result = result + reg_content >= result ? result + reg_content : MASK_FULL;

	return scnprintf(buf, 30, "%llx\n", result);
}

ssize_t sysfs_port1_loopback_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x094FFFC0, 16, CHECK_VALUE, 0x4000, 0x4000, "Check Port 1 loopback mode on" };
	ACTION_ITEM action2 = { 0x094FFFC0, 16, CHECK_VALUE, 0x4000, 0x0000, "Check Port 1 loopback mode off" };

	return sysfs_check_value(dev, action1, action2, buf, &onoff);
}

ssize_t sysfs_port1_loopback_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x094FFFC0, 16, SET_VERIFY, 0x4000, 0x4000, "Set Port 1 to loopback mode on" };
	ACTION_ITEM action2 = { 0x094FFFC0, 16, SET_VERIFY, 0x4000, 0x0000, "Set Port 1 to loopback mode off" };

	ret_val = sysfs_set_value(dev, action1, action2, buf, &onoff);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_port1_power_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x094FFFC0, 16, CHECK_VALUE, 0x0800, 0x0000, "Check Port 1 power mode on" };
	ACTION_ITEM action2 = { 0x094FFFC0, 16, CHECK_VALUE, 0x0800, 0x0800, "Check Port 1 power mode off" };

	return sysfs_check_value(dev, action1, action2, buf, &onoff);
}

ssize_t sysfs_port1_power_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x094FFFC0, 16, SET_VERIFY, 0x0800, 0x0000, "Set Port 1 to power mode on" };
	ACTION_ITEM action2 = { 0x094FFFC0, 16, SET_VERIFY, 0x0800, 0x0800, "Set Port 1 to power mode off"};

	ret_val = sysfs_set_value(dev, action1, action2, buf, &onoff);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_port1_control_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action = { 0x094FFFC0, 16, GET_VALUE, MASK_FULL, 0, "Get Port 1 control register" };

	return sysfs_get_value(dev, action, buf);
}

ssize_t sysfs_port4_loopback_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x0A0FFFC0, 16, CHECK_VALUE, 0x4000, 0x4000, "Check Port 4 loopback mode on" };
	ACTION_ITEM action2 = { 0x0A0FFFC0, 16, CHECK_VALUE, 0x4000, 0x0000, "Check Port 4 loopback mode off" };

	return sysfs_check_value(dev, action1, action2, buf, &onoff);
}

ssize_t sysfs_port4_loopback_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x0A0FFFC0, 16, SET_VERIFY, 0x4000, 0x4000, "Set Port 4 to loopback mode on" };
	ACTION_ITEM action2 = { 0x0A0FFFC0, 16, SET_VERIFY, 0x4000, 0x0000, "Set Port 4 to loopback mode off" };

	ret_val = sysfs_set_value(dev, action1, action2, buf, &onoff);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_port4_power_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x0A0FFFC0, 16, CHECK_VALUE, 0x0800, 0x0000, "Check Port 4 power mode on" };
	ACTION_ITEM action2 = { 0x0A0FFFC0, 16, CHECK_VALUE, 0x0800, 0x0800, "Check Port 4 power mode off" };

	return sysfs_check_value(dev, action1, action2, buf, &onoff);
}

ssize_t sysfs_port4_power_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x0A0FFFC0, 16, SET_VERIFY, 0x0800, 0x0000, "Set Port 4 to power mode on" };
	ACTION_ITEM action2 = { 0x0A0FFFC0, 16, SET_VERIFY, 0x0800, 0x0800, "Set Port 4 to power mode off" };

	ret_val = sysfs_set_value(dev, action1, action2, buf, &onoff);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_port4_control_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action = { 0x0A0FFFC0, 16, GET_VALUE, MASK_FULL, 0, "Get Port 4 control register" };

	return sysfs_get_value(dev, action, buf);
}

ssize_t sysfs_port4_auto_neg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x0A0FFFC0, 16, CHECK_VALUE, 0x1000, 0x1000, "Check Port 4 auto neg on" };
	ACTION_ITEM action2 = { 0x0A0FFFC0, 16, CHECK_VALUE, 0x1000, 0x0000, "Check Port 4 auto neg off" };

	return sysfs_check_value(dev, action1, action2, buf, &onoff);
}

ssize_t sysfs_port4_auto_neg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x0A0FFFC0, 16, SET_VERIFY, 0x1000, 0x1000, "Set Port 4 to auto neg on" };
	ACTION_ITEM action2 = { 0x0A0FFFC0, 16, SET_VERIFY, 0x1000, 0x0000, "Set Port 4 to auto neg off" };

	ret_val = sysfs_set_value(dev, action1, action2, buf, &onoff);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_port4_rmt_loopback_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x0A0F2058, 16, CHECK_VALUE, 0x8000, 0x8000, "Check Port 4 remote loopback mode on" };
	ACTION_ITEM action2 = { 0x0A0F2058, 16, CHECK_VALUE, 0x8000, 0x0000, "Check Port 4 remote loopback mode off" };

	return sysfs_check_value(dev, action1, action2, buf, &onoff);
}

ssize_t sysfs_port4_rmt_loopback_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x0A0F2058, 16, SET_VERIFY, 0x8000, 0x8000, "Set Port 4 to remote loopback mode on" };
	ACTION_ITEM action2 = { 0x0A0F2058, 16, SET_VERIFY, 0x8000, 0x0000, "Set Port 4 to remote loopback mode off" };

	ret_val = sysfs_set_value(dev, action1, action2, buf, &onoff);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_port4_speed_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x0A0FFFC0, 16, CHECK_VALUE, 0x2040, 0x2000, "Test Set Port 4 Speed to 100 Mbs/s" };
	ACTION_ITEM action2 = { 0x0A0FFFC0, 16, CHECK_VALUE, 0x2040, 0x0000, "Test Set Port 4 Speed to 10 Mbs/s " };

	return sysfs_check_value(dev, action1, action2, buf, &speed4);
}

ssize_t sysfs_port4_speed_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x0A0FFFC0, 16, SET_VERIFY, 0x2040, 0x2000, "Test Set Port 4 Speed to 100 Mbs/s" };
	ACTION_ITEM action2 = { 0x0A0FFFC0, 16, SET_VERIFY, 0x2040, 0x0000, "Test Set Port 4 Speed to 10 Mbs/s " };

	ret_val = sysfs_set_value(dev, action1, action2, buf, &speed4);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_port4_error_cnt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u64 result = 0;
	u64 reg_content;

	//Rx Aligment error packet counter
	if (register_read_and_clear(dev, 0x0B002480, &reg_content))
		return -ENXIO;
	result = result + reg_content >= result ? result + reg_content : MASK_FULL;

	//Rx FCS error packet counter
	if (register_read_and_clear(dev, 0x0B002484, &reg_content))
		return -ENXIO;
	result = result + reg_content >= result ? result + reg_content : MASK_FULL;

	//Rx Symbol error counter
	if (register_read_and_clear(dev, 0x0B0024AC, &reg_content))
		return -ENXIO;
	result = result + reg_content >= result ? result + reg_content : MASK_FULL;

	//Rx In Range error counter
	if (register_read_and_clear(dev, 0x0B0024B0, &reg_content))
		return -ENXIO;
	result = result + reg_content >= result ? result + reg_content : MASK_FULL;

	//Rx Out Range error counter
	if (register_read_and_clear(dev, 0x0B0024B4, &reg_content))
		return -ENXIO;
	result = result + reg_content >= result ? result + reg_content : MASK_FULL;

	return scnprintf(buf, 30, "%llx\n", result);
}

ssize_t sysfs_port4_packet_cnt_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x0A0F205E, 16, CHECK_VALUE, 0x0800, 0x0000, "Check Port 4 - Packet Counter Mode TX" };
	ACTION_ITEM action2 = { 0x0A0F205E, 16, CHECK_VALUE, 0x0800, 0x0800, "Check Port 4 - Packet Counter Mode RX" };

	return sysfs_check_value(dev, action1, action2, buf, &txrx);
}

ssize_t sysfs_port4_packet_cnt_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x0A0F205E, 16, SET_VERIFY, 0x0800, 0x0000, "Set Port 4 - Packet Counter Mode to TX" };
	ACTION_ITEM action2 = { 0x0A0F205E, 16, SET_VERIFY, 0x0800, 0x0800, "Set Port 4 - Packet Counter Mode to RX" };

	ret_val = sysfs_set_value(dev, action1, action2, buf, &txrx);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_port4_packet_cnt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action = { 0x0A0F2060, 16, GET_VALUE, MASK_FULL, 0, "Get Port 4 Packet Counter" };

	return sysfs_get_value(dev, action, buf);
}

ssize_t sysfs_port4_link_state_pass_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x0A0F201C, 16, CHECK_VALUE, 0x1000, 0x1000, "Check Port 4 - Force Link Pass State" };
	ACTION_ITEM action2 = { 0x0A0F201C, 16, CHECK_VALUE, 0x1000, 0x0000, "Set Port 4 - Normal Link to Operation" };

	return sysfs_check_value(dev, action1, action2, buf, &onoff);
}

ssize_t sysfs_port4_link_state_pass_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x0A0F201C, 16, SET_VERIFY, 0x1000, 0x1000, "Set Port 4 - Force Link Pass State" };
	ACTION_ITEM action2 = { 0x0A0F201C, 16, SET_VERIFY, 0x1000, 0x0000, "Set Port 4 - Set Link to Normal Operation" };

	ret_val = sysfs_set_value(dev, action1, action2, buf, &onoff);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_port4_SQI_read_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x0A0F2050, 16, CHECK_VALUE, 0x0800, 0x0800, "Check Port 4 - Mode ON for SQI data reading from registry" };
	ACTION_ITEM action2 = { 0x0A0F2050, 16, CHECK_VALUE, 0x0800, 0x0000, "Check Port 4 - Mode OFF for SQI data reading from registry" };

	return sysfs_check_value(dev, action1, action2, buf, &onoff);
}

ssize_t sysfs_port4_SQI_read_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x0A0F2050, 16, SET_VERIFY, 0x0800, 0x0800, "Set Port 4 - Mode ON for SQI data reading from registry" };
	ACTION_ITEM action2 = { 0x0A0F2050, 16, SET_VERIFY, 0x0800, 0x0000, "Set Port 4 - Mode OFF for SQI data reading from registry" };

	ret_val = sysfs_set_value(dev, action1, action2, buf, &onoff);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_port4_SQI_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action = { 0x0A0F2210, 16, GET_VALUE, MASK_FULL, 0, "Get Port 4 Signal Quality Indicator data for calculation" };

	return sysfs_get_value(dev, action, buf);
}

ssize_t sysfs_port4_RX_good_packet_cnt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u64 result = 0;
	u64 reg_content;

	//Rx Unicast packet counter
	if (register_read_and_clear(dev, 0x0B002494, &reg_content))
		return -ENXIO;
	result = result + reg_content >= result ? result + reg_content : MASK_FULL;

	//Rx Multicast packet counter
	if (register_read_and_clear(dev, 0x0B002498, &reg_content))
		return -ENXIO;
	result = result + reg_content >= result ? result + reg_content : MASK_FULL;

	//Rx Broadcast packet counter
	if (register_read_and_clear(dev, 0x0B00249C, &reg_content))
		return -ENXIO;
	result = result + reg_content >= result ? result + reg_content : MASK_FULL;

	return scnprintf(buf, 30, "%llx\n", result);
}

ssize_t sysfs_common_link_stat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action = { 0x0B000100, 64, GET_VALUE, MASK_FULL, 0, "Get link status for all ports" };

	return sysfs_get_value(dev, action, buf);
}

ssize_t sysfs_common_model_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action = { 0x0A800000, 16, GET_VALUE, MASK_FULL, 0, "Get Model Number and revision" };

	return sysfs_get_value(dev, action, buf);
}

ssize_t sysfs_common_rev_ID_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action = { 0x0B000240, 8, GET_VALUE, MASK_FULL, 0, "Get Revision Number Reg" };

	return sysfs_get_value(dev, action, buf);
}

static const char *command_TM0 = "NORMAL\n";
static const int command_TM_size = 7;
static const char *command_TM1 = "TEST_1\n";
static const char *command_TM2 = "TEST_2\n";
static const char *command_TM3 = "TEST_3\n";
static const char *command_TM4 = "TEST_4\n";
static const char *command_TM5 = "TEST_5\n";

ssize_t sysfs_common_test_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret_val = 0;
	s64 result;
	ACTION_ITEM action = { 0x090FFFCA, 16, GET_VALUE, 0xE000, 0x0000, "Check test modes" };
	struct spi_device *spi = container_of(dev, struct spi_device, dev);

	if (perform_action_reg_returns(spi, &action, &result)) {
		dev_warn(dev, "diagCommandExecute set_value %s FAILS\n", action.comment);
		return -ENXIO;
	}

	switch (result) {
	case 0x0000:
		dev_info(dev, "Test Mode 0\n");
		ret_val = sprintf(buf, command_TM0);
		return ret_val;
	case 0x2000:
		dev_info(dev, "Test Mode 1\n");
		ret_val = sprintf(buf, command_TM1);
		return ret_val;
	case 0x4000:
		dev_info(dev, "Test Mode 2\n");
		ret_val = sprintf(buf, command_TM2);
		return ret_val;
	case 0x6000:
		dev_info(dev, "Test Mode 3\n");
		ret_val = sprintf(buf, command_TM3);
		return ret_val;
	case 0x8000:
		dev_info(dev, "Test Mode 4\n");
		ret_val = sprintf(buf, command_TM4);
		return ret_val;
	case 0xA000:
		dev_info(dev, "Test Mode 5\n");
		ret_val = sprintf(buf, command_TM5);
		return ret_val;
	default:
		ret_val = sprintf(buf, "ERR\n");
		dev_info(dev, "Invalid Test Mode\n");
		return ret_val;
	}
}

ssize_t sysfs_common_test_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct spi_device *spi = container_of(dev, struct spi_device, dev);
	int ret_val;

	if (strcmp(buf, command_TM0) == 0) {
		ACTION_ITEM action = { 0x090FFFCA, 16, SET_VERIFY, 0xE000, 0x0000, "Normal operation" };

		ret_val = perform_action(spi, &action);
	} else if (strcmp(buf, command_TM1) == 0) {
		ACTION_ITEM action = { 0x090FFFCA, 16, SET_VERIFY, 0xE000, 0x2000, "Test mode 1: Transmit droop test mode" };

		ret_val = perform_action(spi, &action);
	} else if (strcmp(buf, command_TM2) == 0) {
		ACTION_ITEM action = { 0x090FFFCA, 16, SET_VERIFY, 0xE000, 0x4000, "Test mode 2: Transmit jitter test in MASTER mode" };

		ret_val = perform_action(spi, &action);
	} else if (strcmp(buf, command_TM3) == 0) {
		ACTION_ITEM action = { 0x090FFFCA, 16, SET_VERIFY, 0xE000, 0x6000, "Test mode 3: Transmit jitter test in SLAVE mode" };

		ret_val = perform_action(spi, &action);
	} else if (strcmp(buf, command_TM4) == 0) {
		ACTION_ITEM action = { 0x090FFFCA, 16, SET_VERIFY, 0xE000, 0x8000, "Test mode 4: Transmitter distortion test" };

		ret_val = perform_action(spi, &action);
	} else if (strcmp(buf, command_TM5) == 0) {
		ACTION_ITEM action = { 0x090FFFCA, 16, SET_VERIFY, 0xE000, 0xA000, "Test mode 5: Normal operation at full power" };

		ret_val = perform_action(spi, &action);
	} else {
		dev_warn(dev, "diagCommandExecute test_mode WRONG param\n");
		return -ENXIO;
	}

	if (ret_val) {
		dev_warn(dev, "diagCommandExecute set_value test_mode FAILS\n");
		return -ENXIO;
	}

	return count;
}

ssize_t sysfs_common_br_phy_strap_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ACTION_ITEM action1 = { 0x090FFFC0, 16, CHECK_VALUE, 0x0208, 0x0208, "Check port 0:  Master" };
	ACTION_ITEM action2 = { 0x090FFFC0, 16, CHECK_VALUE, 0x0208, 0x0200, "Check port 0:  Slave" };

	return sysfs_check_value(dev, action1, action2, buf, &ms);
}

ssize_t sysfs_common_br_phy_strap_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret_val;
	ACTION_ITEM action1 = { 0x090FFFC0, 16, SET_VERIFY, 0x0208, 0x0208, "port 0: forcing port to Master" };
	ACTION_ITEM action2 = { 0x090FFFC0, 16, SET_VERIFY, 0x0208, 0x0200, "port 0: forcing port to Slave" };

	ret_val = sysfs_set_value(dev, action1, action2, buf, &ms);

	if (!ret_val)
		return count;
	return ret_val;
}

ssize_t sysfs_debug_bits_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct bcm_drvdata *drvdata = dev_get_drvdata(dev);
	u8 bits;
	int ret;

	ret = kstrtou8(buf, 0, &bits);
	if (ret)
		return ret;

	if ( (bits != 8) && (bits != 16) && (bits != 32) && (bits != 64) ) {
		dev_warn(dev, "DBG_ONLY wrong bits value = %d should be one of (8,16,32,64) \n", bits);
		return -EINVAL;
	}

	mutex_lock(&drvdata->ee_access.lock);

	drvdata->ee_access.bits = bits;

	mutex_unlock(&drvdata->ee_access.lock);

	return count;
}

ssize_t sysfs_debug_bits_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bcm_drvdata *drvdata = dev_get_drvdata(dev);
	u8 bits;

	mutex_lock(&drvdata->ee_access.lock);

	bits = drvdata->ee_access.bits;

	mutex_unlock(&drvdata->ee_access.lock);

	return sprintf(buf, "%d\n", bits);
}

ssize_t sysfs_debug_address_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct bcm_drvdata *drvdata = dev_get_drvdata(dev);
	u32 address;
	int ret;

	ret = kstrtou32(buf, 0, &address);
	if (ret)
		return ret;

	mutex_lock(&drvdata->ee_access.lock);

	drvdata->ee_access.address = address;

	mutex_unlock(&drvdata->ee_access.lock);

	return count;
}

ssize_t sysfs_debug_address_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bcm_drvdata *drvdata = dev_get_drvdata(dev);
	u32 address;

	mutex_lock(&drvdata->ee_access.lock);

	address = drvdata->ee_access.address;

	mutex_unlock(&drvdata->ee_access.lock);

	return sprintf(buf, "0x%08x\n", address);
}

ssize_t sysfs_debug_reg_value_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct bcm_drvdata *drvdata = dev_get_drvdata(dev);
	u32 address;
	u8 bits;
	u64 reg_value;
	int ret;

	ret = kstrtou64(buf, 0, &reg_value);
	if (ret)
		return ret;

	mutex_lock(&drvdata->ee_access.lock);

	address = drvdata->ee_access.address;
	bits = drvdata->ee_access.bits;

	mutex_unlock(&drvdata->ee_access.lock);

	ACTION_ITEM action = { address, bits, SET_VALUE, MASK_FULL, reg_value, "EE set register" };

	ret = register_set(dev, action);
	if (ret)
		return ret;

	return count;
}

ssize_t sysfs_debug_reg_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bcm_drvdata *drvdata = dev_get_drvdata(dev);
	u32 address;
	u8 bits;

	mutex_lock(&drvdata->ee_access.lock);

	address = drvdata->ee_access.address;
	bits = drvdata->ee_access.bits;

	mutex_unlock(&drvdata->ee_access.lock);

	ACTION_ITEM action = { address, bits, GET_VALUE, MASK_FULL, 0, "EE read register" };

	return sysfs_get_value(dev, action, buf);
}

static PORT0_ATTR_RW(loopback);
static PORT0_ATTR_RW(power);
static PORT0_ATTR_RO(control_reg);
static PORT0_ATTR_RW(rmt_loopback);
static PORT0_ATTR_RW(speed);
static PORT0_ATTR_RO(error_cnt);
static PORT0_ATTR_RW(packet_cnt_mode);
static PORT0_ATTR_RO(packet_cnt);
static PORT0_ATTR_RW(link_state_pass);
static PORT0_ATTR_RW(auto_mdix);
static PORT0_ATTR_RW(cable_diag);
static PORT0_ATTR_RO(cable_diag_fault);
static PORT0_ATTR_RO(cable_diag_len);
static PORT0_ATTR_RW(SQI_read_mode);
static PORT0_ATTR_RO(SQI);
static PORT0_ATTR_RO(RX_good_packet_cnt);

static struct attribute *port0_entries[] = {
	&port0_loopback_attr.attr,
	&port0_power_attr.attr,
	&port0_control_reg_attr.attr,
	&port0_rmt_loopback_attr.attr,
	&port0_speed_attr.attr,
	&port0_error_cnt_attr.attr,
	&port0_packet_cnt_mode_attr.attr,
	&port0_packet_cnt_attr.attr,
	&port0_link_state_pass_attr.attr,
	&port0_auto_mdix_attr.attr,
	&port0_cable_diag_attr.attr,
	&port0_cable_diag_fault_attr.attr,
	&port0_cable_diag_len_attr.attr,
	&port0_SQI_read_mode_attr.attr,
	&port0_SQI_attr.attr,
	&port0_RX_good_packet_cnt_attr.attr,
	NULL
};

static struct attribute_group port0_attribute_group = {
	.name = "port0",
	.attrs = port0_entries,
};

static PORT1_ATTR_RW(loopback);
static PORT1_ATTR_RW(power);
static PORT1_ATTR_RO(control_reg);

static struct attribute *port1_entries[] = {
	&port1_loopback_attr.attr,
	&port1_power_attr.attr,
	&port1_control_reg_attr.attr,
	NULL
};

static struct attribute_group port1_attribute_group = {
	.name = "port1",
	.attrs = port1_entries,
};

static PORT4_ATTR_RW(loopback);
static PORT4_ATTR_RW(power);
static PORT4_ATTR_RO(control_reg);
static PORT4_ATTR_RW(auto_neg);
static PORT4_ATTR_RW(rmt_loopback);
static PORT4_ATTR_RW(speed);
static PORT4_ATTR_RO(error_cnt);
static PORT4_ATTR_RW(packet_cnt_mode);
static PORT4_ATTR_RO(packet_cnt);
static PORT4_ATTR_RW(link_state_pass);
static PORT4_ATTR_RW(SQI_read_mode);
static PORT4_ATTR_RO(SQI);
static PORT4_ATTR_RO(RX_good_packet_cnt);

static struct attribute *port4_entries[] = {
	&port4_loopback_attr.attr,
	&port4_power_attr.attr,
	&port4_control_reg_attr.attr,
	&port4_auto_neg_attr.attr,
	&port4_rmt_loopback_attr.attr,
	&port4_speed_attr.attr,
	&port4_error_cnt_attr.attr,
	&port4_packet_cnt_mode_attr.attr,
	&port4_packet_cnt_attr.attr,
	&port4_link_state_pass_attr.attr,
	&port4_SQI_read_mode_attr.attr,
	&port4_SQI_attr.attr,
	&port4_RX_good_packet_cnt_attr.attr,
	NULL
};

static struct attribute_group port4_attribute_group = {
	.name = "port4",
	.attrs = port4_entries,
};

static COMMON_ATTR_RO(link_stat);
static COMMON_ATTR_RO(model);
static COMMON_ATTR_RW(test_mode);
static COMMON_ATTR_RW(br_phy_strap);
static COMMON_ATTR_RO(rev_ID);

static struct attribute *common_entries[] = {
	&common_link_stat_attr.attr,
	&common_model_attr.attr,
	&common_test_mode_attr.attr,
	&common_br_phy_strap_attr.attr,
	&common_rev_ID_attr.attr,
	NULL
};

static struct attribute_group common_attribute_group = {
	.name = "common",
	.attrs = common_entries,
};


static DEBUG_ATTR_RW(bits);
static DEBUG_ATTR_RW(address);
static DEBUG_ATTR_RW(reg_value);

static struct attribute *debug_entries[] = {
	&debug_bits_attr.attr,
	&debug_address_attr.attr,
	&debug_reg_value_attr.attr,
	NULL
};

static struct attribute_group debug_attribute_group = {
	.name = "ee_debug",
	.attrs = debug_entries,
};

const struct attribute_group *groups_attr[] = {
	&port0_attribute_group,
	&port1_attribute_group,
	&port4_attribute_group,
	&common_attribute_group,
	&debug_attribute_group,
	NULL,
};
