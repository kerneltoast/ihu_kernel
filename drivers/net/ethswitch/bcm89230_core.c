#include <linux/module.h>
#include <linux/delay.h>          // required for delay during read back

#include "bcm89230_spi.h"
#include "bcm89230_actions.h"
#include "bcm89230_core.h"

static const struct ACTION_ITEM pre_init_actions[] = {
	{ 0x0A800000, 16, CHECK_FAIL,    0x03F8, 0x00B0, "Checking model revision (only BCM89230 and BCM89231 are supported)" },
};

static const struct ACTION_ITEM rgmii_1G[] = {
	{ 0x0A840026, 16, SET_VERIFY,    0x0007, 0x0006, "port 8: Enabling RGMII interface in 1 G mode" },
};

static const struct ACTION_ITEM rgmii_100M[] = {
	{ 0x0A840026, 16, SET_VERIFY,    0x0007, 0x0005, "port 8: Enabling RGMII interface in 100 M mode" },
};

static const struct ACTION_ITEM init_actions[] = {
	{ 0x0B00000B,  8, SET_VERIFY,      0x03,   0x02, "Enabling frame forwarding in unmanged mode" },

	{ 0x0B000022, 16, SET_VERIFY,    1 << 6, 1 << 6, "port 8: Including port for forwarding in Dumb mode" },

	{ 0x0A840024, 16, CHECK_FAIL,    1 << 1, 1 << 1, "port 8: Checking mode" },

	{ 0x0A0F23E0, 16, SET_VERIFY, MASK_FULL, 0xC600, "port 4: not documented (FAQ)" },
	{ 0x0A0F23C0, 16, SET_VERIFY, MASK_FULL, 0x35A4, "port 4: not documented (FAQ)" },
	{ 0x0A0F23D8, 16, SET_VERIFY, MASK_FULL, 0x818A, "port 4: not documented (FAQ)" },
	{ 0x0A0F23CC, 16, SET_VERIFY, MASK_FULL, 0x00C0, "port 4: not documented (FAQ)" },
	{ 0x0A0F23D6, 16, SET_VERIFY, MASK_FULL, 0xA008, "port 4: not documented (FAQ)" },
	{ 0x0A0F23D4, 16, SET_VERIFY, MASK_FULL, 0x008C, "port 4: not documented (FAQ)" },
	{ 0x0A0F23CE, 16, SET_VERIFY, MASK_FULL, 0x0000, "port 4: not documented (FAQ)" },
	{ 0x0A0F2050, 16, SET_VERIFY, MASK_FULL, 0x0C30, "port 4: Enable DSP clock (FAQ)" },
	{ 0x0A0F22CC, 16, SET_VERIFY, MASK_FULL, 0x0020, "port 4: not documented (FAQ)" },
	{ 0x0A0F2208, 16, SET_VERIFY, MASK_FULL, 0x0037, "port 4: not documented (FAQ)" },
	{ 0x0A0F224A, 16, SET_VERIFY, MASK_FULL, 0x0936, "port 4: not documented (FAQ)" },
	{ 0x0A0F22A0, 16, SET_VERIFY, MASK_FULL, 0x470B, "port 4: not documented (FAQ)" },
	{ 0x0A0F22A8, 16, SET_VERIFY, MASK_FULL, 0x89CF, "port 4: not documented (FAQ)" },
	{ 0x0A0F23CC, 16, SET_VERIFY, MASK_FULL, 0x0004, "port 4: not documented (FAQ)" },
	{ 0x0A0F23C2, 16, SET_VERIFY, MASK_FULL, 0xE025, "port 4: not documented (FAQ)" },
	{ 0x0A0F23CA, 16, SET_VERIFY, MASK_FULL, 0x0C00, "port 4: not documented (FAQ)" },
	{ 0x0A0F2600, 16, SET_VERIFY, MASK_FULL, 0x0000, "port 4: not documented, maybe equivalent to 0x0A022600 (enable Eth-mode on port 4) (FAQ)" },
	{ 0x0A0FFFC8, 16, SET_VERIFY, MASK_FULL, 0x01E1, "port 4: not documented, maybe equivalent to 0x0A02FFC8 (no pause capability) (FAQ)" },
	{ 0x0A0FFFC0, 16, SET_VERIFY, MASK_FULL, 0x2100, "port 4: register partially undocumented, maybe equivalent to 0x0A02FFC0 (100 Mb/s with auto-negotiation off) (FAQ)" },

	{ 0x090F205E, 16,  SET_VALUE, MASK_FULL, 0xF867, "port 0: enable receive counter" },
	{ 0x094F205E, 16,  SET_VALUE, MASK_FULL, 0xF867, "port 1: enable receive counter" },
	{ 0x0A0F205E, 16,  SET_VALUE, MASK_FULL, 0xF867, "port 4: enable receive counter, should be equivalent to 0x0A02205E" },

	{ 0x0A840000, 16, SET_VERIFY,    0x0060, 0x0020, "port 8: set slew rate" },
	{ 0x0A84000A, 16, SET_VERIFY,    1 << 0,      0, "port 8: set low voltage mode" },
	{ 0x0A840020, 16,  SET_VALUE,    0x000F, 0x0007, "port 8: enable delay control (set to 2 ns)" },
	{ 0x0A840020, 16, CHECK_FAIL,    0x0007, 0x0007, "port 8: checking delay control" },

	{ 0x0B000000,  8, CHECK_WARN, MASK_FULL,   0x20, "port 0: Checking state of spanning tree" },
	{ 0x0B000000,  8, SET_VERIFY, MASK_FULL,   0x00, "port 0: Disabling spanning tree" },
	{ 0x0B000001,  8, CHECK_WARN, MASK_FULL,   0x20, "port 1: Checking state of spanning tree" },
	{ 0x0B000001,  8, SET_VERIFY, MASK_FULL,   0x00, "port 1: Disabling spanning tree" },
	{ 0x0B000004,  8, CHECK_WARN, MASK_FULL,   0x20, "port 4: Checking state of spanning tree" },
	{ 0x0B000004,  8, SET_VERIFY, MASK_FULL,   0x00, "port 4: Disabling spanning tree" },

	{ 0x0B000203,  8, SET_VERIFY,      0x01,   0x00, "port 8: Disabling Broadcom tag" },

	{ 0x0B000038,  16, SET_VERIFY,   0x01FF, 0x01FF, "Disable pause pass through for RX Register for all ports" },
	{ 0x0B00003A,  16, SET_VERIFY,   0x01FF, 0x01FF, "Disable pause pass through for TX Register for all ports" },

	{ 0x0A0F2000, 16, SET_VERIFY,    0x4000, 0x4000, "port 4: Disabling Automatic MDI crossover" },
};

static const struct ACTION_ITEM force_master_actions[] = {
	{ 0x090FFFC0, 16,  SET_VALUE,    0xCFFF, 0x0208, "port 0: forcing port to master" },
	{ 0x090FFFC0, 16, CHECK_FAIL,    0xCFF8, 0x0208, "port 0: checking port mode" },
	{ 0x094FFFC0, 16,  SET_VALUE,    0xCFFF, 0x0208, "port 1: forcing port to master" },
	{ 0x094FFFC0, 16, CHECK_FAIL,    0xCFF8, 0x0208, "port 1: checking port mode" },
};

static const struct ACTION_ITEM readback_actions[] = {
	{ 0x0A800000, 16, PRINT_REG, MASK_FULL,      0, "model revision (should be 0x00B3)" },
	{ 0x0B000100, 16, PRINT_REG, MASK_FULL,      0, "link status" },
	{ 0x090FFFC2, 16, PRINT_REG, MASK_FULL,      0, "port 0: status" },
	{ 0x090F2060, 16, PRINT_REG, MASK_FULL,      0, "port 0: number of received packets" },
	{ 0x094FFFC2, 16, PRINT_REG, MASK_FULL,      0, "port 1: status" },
	{ 0x094F2060, 16, PRINT_REG, MASK_FULL,      0, "port 1: number of received packets" },
	{ 0x0A0FFFC2, 16, PRINT_REG, MASK_FULL,      0, "port 4: status" },
	{ 0x0A0F2060, 16, PRINT_REG, MASK_FULL,      0, "port 4: number of received packets" },
	{ 0x0B000104, 32, PRINT_REG, MASK_FULL,      0, "port speed summary" },
	{ 0x0B000108, 16, PRINT_REG, MASK_FULL,      0, "duplex status summary" },
	{ 0x0B000190, 16, PRINT_REG, MASK_FULL,      0, "reset status of switch core" },
	{ 0x0B002000, 16, PRINT_REG, MASK_FULL,      0, "TX octets counter for port 0 (BR)" },
	{ 0x0B002100, 16, PRINT_REG, MASK_FULL,      0, "TX octets counter for port 1 (BR)" },
	{ 0x0B002400, 16, PRINT_REG, MASK_FULL,      0, "TX octets counter for port 4 (Eth)" },
	{ 0x0B002800, 16, PRINT_REG, MASK_FULL,      0, "TX octets counter for port 8 (RGMII)" },
	{ 0x0B00204C, 16, PRINT_REG, MASK_FULL,      0, "RX octets counter for port 0 (BR)" },
	{ 0x0B00214C, 16, PRINT_REG, MASK_FULL,      0, "RX octets counter for port 1 (BR)" },
	{ 0x0B00244C, 16, PRINT_REG, MASK_FULL,      0, "RX octets counter for port 4 (Eth)" },
	{ 0x0B00284C, 16, PRINT_REG, MASK_FULL,      0, "RX octets counter for port 8 (RGMII)" },
};

/* TODO This diagnostic feature should be controlled by a sysfs interface */
static bool read_back;
module_param(read_back, bool, 0444);
MODULE_PARM_DESC(read_back, "Enable read back");

static bool downgrade_RGMII;
module_param(downgrade_RGMII, bool, 0444);
MODULE_PARM_DESC(downgrade_RGMII, "Downgrade RGMII interface from 1 GBit/s to 100 MBit/s");

static bool force_master;
module_param(force_master, bool, 0444);
MODULE_PARM_DESC(force_master, "Force BroadR-Reach ports to master");

/**
 * map_action_to_string() - Returns a human-readable representation of action.
 * @action:      action
 * Return:
 *  zero-terminated string representing the action
 */
static char
*map_action_to_string(enum ACTION action)
{
	switch (action) {
	case SET_VALUE:
		return "set value";
	case SET_VERIFY:
		return "set value and verify write";
	case CHECK_WARN:
		return "check (warn)";
	case CHECK_FAIL:
		return "check (fail)";
	case PRINT_REG:
		return "print";
	case GET_VALUE:
		return "get value";
	case CHECK_VALUE:
		return "check silent";
	default:
		return "unknown";
	}
}

/**
 * print_action_info() - Prints information about an action.
 * @spi: device description
 * @action:   action
 */
static void
print_action_info(struct spi_device *spi, const struct ACTION_ITEM * const action)
{
	dev_dbg(&spi->dev, "  register: 0x%08X\n", action->register_address);
	dev_dbg(&spi->dev, "  size:     %u\n", action->register_size);
	dev_dbg(&spi->dev, "  action:   %s\n", map_action_to_string(action->action));
	dev_dbg(&spi->dev, "  mask:     0x%016llX\n", action->mask);
	dev_dbg(&spi->dev, "  value:    0x%016llX\n", action->value);
	dev_dbg(&spi->dev, "  comment:  %s\n", action->comment);
}

/**
 * set_value() - Writes register specified by action.
 * @spi:      to which SPI device register value will be send
 * @action:   action containing information about register
 * return:
 *  0 on success
 *  non-zero otherwise
 */
static int
set_value(struct spi_device *spi, const struct ACTION_ITEM * const action)
{
	u64 value_to_write;
	u64 full_mask = 0xFFFFFFFFFFFFFFFF;

	// perform read if this is a read-modify-write operation
	full_mask >>= 64 - action->register_size;
	if ((action->mask & full_mask) != full_mask) {
		if (read_bcm89230_reg_spi(spi, action->register_address, action->register_size, &value_to_write)) {
			dev_err(&spi->dev, "Read part of read-modify-write operation failed.\n");
			return 1;
		}

#ifdef DEBUG
		if ((action->value & action->mask) != action->value) {
			dev_warn(&spi->dev, "Some bits that are set in value are masked.\n");
			print_action_info(spi, action);
		}
#endif
		value_to_write = (value_to_write & ~action->mask) | (action->value & action->mask);
	} else {
		value_to_write = action->value;
	}

	return write_bcm89230_reg_spi(spi, action->register_address, action->register_size, value_to_write);
}

/**
 * check_value() - Checks one register of the BCM89230.
 * @spi:      from which SPI device register will be read
 * @action:   action containing information about register
 * @fail:     0: print a warning if the value doesn't match; 1: print an error message and return an error if the value doesn't match; 2:silently
 * @silent:   perorm action without printing msg on console
 * Return:
 *  0 on success\n
 *  non-zero otherwise
 */
static int
check_value(struct spi_device *spi, const struct ACTION_ITEM * const action, const int fail, const int silent)
{
	u64 value;

	if (read_bcm89230_reg_spi(spi, action->register_address, action->register_size, &value)) {
		dev_err(&spi->dev, "Read failed.\n");
		return 1;
	}

#ifdef DEBUG
	if ((action->value & action->mask) != action->value) {
		dev_warn(&spi->dev, "Some bits that are set in value are masked.\n");
		print_action_info(spi, action);
	}
#endif
	if ((value & action->mask) != (action->value & action->mask)) {
		if (fail) {
			if (!silent)
				dev_err(&spi->dev, "Check failed. Value was 0x%016llX.\n", value);
			print_action_info(spi, action);
			return 1;
		}

		if (!silent)
			dev_warn(&spi->dev, "Check failed. Value was 0x%016llX.\n", value);
		print_action_info(spi, action);
		return 0;
	}

	return 0;
}

/**
 * print_value() - Reads register specified by action and prints its value.
 * @spi: from which SPI device register will be read
 * @action: containing information about register
 * Return:
 *  0 on success
 *  non-zero otherwise
 */
static int
print_value(struct spi_device *spi, const struct ACTION_ITEM * const action)
{
	u64 value;

	if (read_bcm89230_reg_spi(spi, action->register_address, action->register_size, &value)) {
		dev_err(&spi->dev, "Read failed.\n");
		return 1;
	}

	dev_info(&spi->dev, "Register 0x%08X contains 0x%016llX (%s).\n", action->register_address, value, action->comment);

	return 0;
}

/**
 * get_value() - Returns register value specified by action.
 * @spi:     structure describes SPI device from which register will be read.
 * @action:  action containing information about register
 * @value:   value of register
 * Return:
 *  0 on success\n
 *  non-zero otherwise
 */
static int
get_value(struct spi_device *spi, const struct ACTION_ITEM * const action, uint64_t * const value)
{
	u64 tmp_value;

	if (read_bcm89230_reg_spi(spi, action->register_address, action->register_size, &tmp_value)) {
		dev_err(&spi->dev, "Read failed.\n");
		return 1;
	}

	*value = tmp_value & action->mask;
	return 0;
}

/**
 * perform_action_reg_returns() - Performs action requested by action and return result
 * @spi:     structure describes SPI device from which register will be read.
 * @action:  action to perform
 * @result:  value of read register
 * Return:
 *  0 on success\n
 *  non-zero otherwise
 */
int
perform_action_reg_returns(struct spi_device *spi, const struct ACTION_ITEM * const action, uint64_t * const result)
{
	dev_dbg(&spi->dev, "Performing action:\n");
	print_action_info(spi, action);

	dev_dbg(&spi->dev, "%s ...\n", action->comment ? action->comment : "No description available");

	switch (action->action) {
	case SET_VALUE:
	case SET_VERIFY:
		if (set_value(spi, action))
			return 1;

		if (action->action == SET_VERIFY) {
			if (check_value(spi, action, 1, 0)) {
				dev_err(&spi->dev, "Write verification failed.\n");
				return 1;
			}
		}
		return 0;
	case CHECK_WARN:
		return check_value(spi, action, 0, 0);
	case CHECK_FAIL:
		return check_value(spi, action, 1, 0);
	case PRINT_REG:
		return print_value(spi, action);
	case GET_VALUE:
		return get_value(spi, action, result);
	case CHECK_VALUE:
		return check_value(spi, action, 1, 1);
	default:
		dev_err(&spi->dev, "Internal error: Invalid action.\n");
		return 1;
	}
}

/**
 * perform_action() - Performs action requested by action.
 * @spi:         on which SPI action will be performed
 * @action:      action to perform
 * Return:
 *  0 on success\n
 *  non-zero otherwise
 */
int
perform_action(struct spi_device *spi, const struct ACTION_ITEM * const action)
{
	u64 dont_care;

	return perform_action_reg_returns(spi, action, &dont_care);
}

/**
 * configure_bcm89230_chip() - configures the BCM89230 chip.
 * @spi:    Structure describes spi to be configured
 * Return:
 *  0 on success\n
 *  non-zero otherwise
 */
int
configure_bcm89230_chip(struct spi_device *spi)
{
	int index;

	dev_info(&spi->dev, "Performing pre-init actions ...\n");

	for (index = 0; index < sizeof(pre_init_actions) / sizeof(struct ACTION_ITEM); index++) {
		if (perform_action(spi, &pre_init_actions[index])) {
			dev_err(&spi->dev, "Pre-init actions failed at step %i.\n", index + 1);
			print_action_info(spi, &pre_init_actions[index]);
			return 1;
		}
	}

	dev_info(&spi->dev, "Configuring RMGII interface ...\n");

	if (downgrade_RGMII) {
		for (index = 0; index < sizeof(rgmii_100M) / sizeof(struct ACTION_ITEM); index++) {
			if (perform_action(spi, &rgmii_100M[index])) {
				dev_err(&spi->dev, "Configuration of RGMII interface failed at step %i.\n", index + 1);
				print_action_info(spi, &rgmii_100M[index]);
				return 1;
			}
		}
	} else {
		for (index = 0; index < sizeof(rgmii_1G) / sizeof(struct ACTION_ITEM); index++) {
			if (perform_action(spi, &rgmii_1G[index])) {
				dev_err(&spi->dev, "Configuration of RGMII interface failed at step %i.\n", index + 1);
				print_action_info(spi, &rgmii_1G[index]);
				return 1;
			}
		}
	}

	dev_info(&spi->dev, "Configuring BCM89230 ...\n");

	for (index = 0; index < sizeof(init_actions) / sizeof(struct ACTION_ITEM); index++) {
		if (perform_action(spi, &init_actions[index])) {
			dev_err(&spi->dev, "Configuration failed at step %i.\n", index + 1);
			print_action_info(spi, &init_actions[index]);
			return 1;
		}
	}

	if (force_master) {
		dev_info(&spi->dev, "Forcing BroadR-Reach ports to master ...\n");

		for (index = 0; index < sizeof(force_master_actions) / sizeof(struct ACTION_ITEM); index++) {
			if (perform_action(spi, &force_master_actions[index])) {
				dev_err(&spi->dev, "Configuration failed at step %i.\n", index + 1);
				print_action_info(spi, &force_master_actions[index]);
				return 1;
			}
		}
	}

	if (read_back) {
		dev_info(&spi->dev, "Read back ...\n");

		msleep(1000);

		for (index = 0; index < sizeof(readback_actions) / sizeof(struct ACTION_ITEM); index++) {
			if (perform_action(spi, &readback_actions[index])) {
				dev_err(&spi->dev, "Read back failed at step %i.\n", index + 1);
				print_action_info(spi, &readback_actions[index]);
				return 1;
			}
		}
	}

	return 0;
}

