#define pr_fmt(fmt) "bcm89230: " fmt

#include <linux/module.h>

#include <linux/delay.h>          // required for delay during reset
#include <linux/pm.h>             // required for suspend to RAM
#include <linux/gpio/consumer.h>
#include <linux/moduleparam.h>
#include <linux/sysfs.h>

#include "bcm89230_spi.h"
#include "bcm89230_core.h"
#include "bcm89230_attr.h"

#define SPI_SPEED_HZ 1000000

#define BCM89230_CHIP_ID        (0 << 6)        // chip ID as required in SPI command
#define BCM89230_READ_COMMAND   (0 << 5)        // read
#define BCM89230_WRITE_COMMAND  (1 << 5)        // write
#define BCM89230_AUTO_INCREMENT (1 << 4)        // auto increment
#if SPI_SPEED_HZ <= 25000000
	#define BCM89230_READ_WAIT_BYTES 2
	#define BCM89230_READ_WAIT_STATE (1 << 2)
#elif SPI_SPEED_HZ <= 50000000
	#define BCM89230_READ_WAIT_BYTES 4
	#define BCM89230_READ_WAIT_STATE (2 << 2)
#else
	#define BCM89230_READ_WAIT_BYTES 6
	#define BCM89230_READ_WAIT_STATE (3 << 2)
#endif
#define BCM89230_8_BIT_ACCESS   (0 << 0)        //  8 bit read/write
#define BCM89230_16_BIT_ACCESS  (1 << 0)        // 16 bit read/write
#define BCM89230_32_BIT_ACCESS  (2 << 0)        // 32 bit read/write
#define BCM89230_64_BIT_ACCESS  (3 << 0)        // 64 bit read/write

/**
 * spi_transfer() - Perform a SPI transfer.
 * @spi:            struct describes spi on which transfer will be performed
 * @tx_buffer:      pointer to buffer containing the data to send or NULL if zeros should be shifted out
 * @rx_buffer:      pointer to buffer where received data should be stored or NULL if received data are not required
 * @size:           number of bytes to send/receive
 * @write_and_read: 0: read operation; 1: write and read operation
 * return:
 *  0 on success
 *  non-zero otherwise
 */
static int
spi_transfer(
	struct spi_device *spi,
	unsigned char * const tx_buffer,
	unsigned char * const rx_buffer,
	const u32 size,
	bool write_and_read)
{
	int ret;

	dev_dbg(&spi->dev, "%s size  %d\n", __func__, size);

	if (write_and_read) {
		dev_dbg(&spi->dev, "TX:\n");
		print_hex_dump_debug(pr_fmt(), DUMP_PREFIX_OFFSET, 32, 1, tx_buffer, 5, false);

		ret = spi_write_then_read(spi, tx_buffer, 5, rx_buffer, size);

		dev_dbg(&spi->dev, "RX:\n");
		print_hex_dump_debug(pr_fmt(), DUMP_PREFIX_OFFSET, 32, 1, rx_buffer, size, false);
	} else {
		dev_dbg(&spi->dev, "TX:\n");
		print_hex_dump_debug(pr_fmt(), DUMP_PREFIX_OFFSET, 32, 1, tx_buffer, size, false);

		ret = spi_write(spi, tx_buffer, size);
	}
	if (ret != 0) {
		dev_err(&spi->dev, "SPI transfer failed.\n");
		return -1;
	}
	return 0;
}

/**
 * get_command_byte_spi() - Returns the command required for BCM89230 read/write
 * @access_size:  size of accessed register in bytes
 * @write:        0: read operation; 1: write operation
 * @result:      returned command byte
 * return:
 *  0 if ok, non-zero otherwise
 */
unsigned char
get_command_byte_spi(const unsigned int access_size, const int write, unsigned char *result)
{
	unsigned char command;

	switch (access_size) {
	case 1:
		command = BCM89230_8_BIT_ACCESS;
		break;
	case 2:
		command = BCM89230_16_BIT_ACCESS;
		break;
	case 4:
		command = BCM89230_32_BIT_ACCESS;
		break;
	case 8:
		command = BCM89230_64_BIT_ACCESS;
		break;
	default:
		command = 0;
		return 1;
	}

	if (write) {
		command |= BCM89230_CHIP_ID
			| BCM89230_WRITE_COMMAND
			| BCM89230_AUTO_INCREMENT;
	} else {
		command |= BCM89230_CHIP_ID
			| BCM89230_READ_COMMAND
			| BCM89230_AUTO_INCREMENT
			| BCM89230_READ_WAIT_STATE;
	}
	*result = command;
	return 0;
}

/**
 * reset_bcm89230_chip() - Resets BCM89230 chip.
 * @dev: device in which information of reset pin is stored
 * @release: 1: reset and release, 0: keep reset
 * return:
 *  0 on success
 *  non-zero otherwise
 */
static int
reset_bcm89230_chip(struct device *dev, int release)
{
	struct gpio_desc *reset_pin;
	struct bcm_drvdata *drvdata = dev_get_drvdata(dev);

	if (!drvdata->reset_pin) {
		dev_info(dev, "%s\n", __func__);
		reset_pin = devm_gpiod_get(dev, "bcm89230-reset", GPIOD_OUT_LOW);
		drvdata->reset_pin = reset_pin;
	} else {
		dev_info(dev, "%s after suspend\n", __func__);
		reset_pin = drvdata->reset_pin;
		gpiod_set_value(reset_pin, 0);
	}
	if (IS_ERR(reset_pin)) {
		dev_err(dev, "%s: devm_gpiod_get(%d) failed (%ld)\n", __func__, 0, PTR_ERR(reset_pin));
		return -1;
	}

	if (release) {
		msleep(10);
		gpiod_set_value(reset_pin, 1);
		msleep(10);
	}
	dev_info(dev, "reset switch done%s\n", release ? "" : ", keep reset");
	return 0;
}

/**
 * probe_bcm89230_spi() - probe function.
 * @spi: device which is probed
 * return:
 *  0 on success
 *  non-zero otherwise
 */
static int
probe_bcm89230_spi(struct spi_device *spi)
{
	int ret_value;
	struct device *dev = &spi->dev;
	struct bcm_data *bcm;

	dev_info(dev, "bcm89230spi_probe\n");

	bcm = devm_kzalloc(dev, sizeof(*bcm), GFP_KERNEL);
	if (!bcm)
		return -ENOMEM;

	dev_set_drvdata(dev, &bcm->drvdata);

	ret_value = reset_bcm89230_chip(dev, 1);
	if (ret_value != 0) {
		dev_err(dev, "reset failed\n");
		return -2;
	}

	ret_value = configure_bcm89230_chip(spi);
	if (ret_value != 0) {
		dev_err(dev, "configure chip failed\n");
		return -3;
	}

	ret_value = sysfs_create_groups(&dev->kobj, groups_attr);
	if (ret_value != 0) {
		dev_err(dev, "configure sysfs failed\n");
		return -4;
	}

	return 0;
}

/**
 * check_size() - Checks if number is 8, 16, 32 or 64 and prints error if not.
 * @size:  any number
 * return:
 *  0: if size is 8, 16, 32 or 64; 1: otherwise
 */
int
check_size(const unsigned int size)
{
	switch (size) {
	case 8:
	case 16:
	case 32:
	case 64:
		return 0;
	default:
		return 1;
	}
}

/**
 * remove_bcm89230_spi() - Exeutes when driver is removed.
 * @spi:  description of device
 * return:
 *  0: if ok, non-zero otherwise
 */
static int
remove_bcm89230_spi(struct spi_device *spi)
{
	int res = 0;
	struct device *dev = &spi->dev;
	struct bcm_drvdata *drvdata = dev_get_drvdata(dev);
	struct bcm_data *bcm = container_of(drvdata, struct bcm_data, drvdata);

	sysfs_remove_groups(&dev->kobj, groups_attr);

	dev_info(dev, "bcm89230spi_remove\n");

	return res;
}

/**
 * read_bcm89230_reg_spi() - read bcm89230 register.
 * @spi:  description of device
 * @address: address of register to be read
 * @size: size of register
 * @value: content of register
 * return:
 *  0: if ok, non-zero otherwise
 */
int
read_bcm89230_reg_spi(
	struct spi_device *spi,
	const u32 address,
	unsigned int size,
	u64 * const value)
{
	unsigned char tx_buffer[1 + 4 + BCM89230_READ_WAIT_BYTES + 8] = {0};
	unsigned char rx_buffer[sizeof(tx_buffer)];
	unsigned int access_size;   // physical size of access (usually equal to size (in bytes))

	// check pointer
	if (!value) {
		dev_err(&spi->dev, "Internal error: Null pointer passed.\n");
		return 1;
	}

	// check size
	if (check_size(size)) {
		dev_err(&spi->dev, "Internal error: Invalid write size.\n");
		return 1;
	}
	dev_dbg(&spi->dev, "read bites %d\n", size);
	// bits -> bytes
	size /= 8;

	// physical access size
	if ((address & 0xFFFF0000) == 0x0B000000)
		access_size = 8;
	else
		access_size = size;

	// set command
	if (get_command_byte_spi(access_size, 0, tx_buffer) != 0) {
		dev_err(&spi->dev, "Internal error: Invalid access size.\n");
		return 1;
	}

	// set address
	tx_buffer[1] = (unsigned char)(address >> 24);
	tx_buffer[2] = (unsigned char)(address >> 16);
	tx_buffer[3] = (unsigned char)(address >> 8);
	tx_buffer[4] = (unsigned char)(address >> 0);

	// perform transfer
	if (spi_transfer(spi, tx_buffer, rx_buffer,  BCM89230_READ_WAIT_BYTES + access_size, true)) {
		dev_err(&spi->dev, "Register read failed.\n");
		return 1;
	}
	unsigned int offset;     // offset of the requested value in rx_buffer
	unsigned int index;      // index used to access the value field in rx_buffer
	u64 received_value = 0;

	// parse result
	offset =  BCM89230_READ_WAIT_BYTES + access_size - size;

	for (index = 0; index < size; index++)
		received_value = (received_value << 8) | rx_buffer[offset + index];

	dev_dbg(&spi->dev, "received value %llx\n", received_value);
	*value = received_value;
	return 0;
}

/**
 * write_bcm89230_reg_spi() - write bcm89230 register .
 * @spi:  description of device
 * @address: address of register to be write
 * @size: size of register
 * @value: value which is write to register
 * return:
 *  0: if ok, non-zero otherwise
 */
int
write_bcm89230_reg_spi(
	struct spi_device *spi,
	const u32 address,
	unsigned int size,
	const u64 value)
{
	unsigned char tx_buffer[1 + 4 + 8];
	unsigned char rx_buffer[15];
	unsigned int access_size;   // physical size of access (usually equal to size (in bytes))
	unsigned int offset;        // offset of value in tx_buffer
	unsigned int index;         // index used to access tx_buffer

	// check size
	if (check_size(size)) {
		dev_err(&spi->dev, "Internal error: Invalid write size.\n");
		return 1;
	}

	// bits -> bytes
	size /= 8;
	dev_dbg(&spi->dev, "spiWriteBcm89230Reg\n");
	// physical access size
	if ((address & 0xFFFF0000) == 0x0B000000)
		access_size = 8;
	else
		access_size = size;

	// set command
	if (get_command_byte_spi(access_size, 1, &tx_buffer[0]) != 0) {
		dev_err(&spi->dev, "Internal error: Invalid access size.\n");
		return 1;
	}

	// set address
	tx_buffer[1] = (unsigned char)(address >> 24);
	tx_buffer[2] = (unsigned char)(address >> 16);
	tx_buffer[3] = (unsigned char)(address >> 8);
	tx_buffer[4] = (unsigned char)(address >> 0);

	// pad tx_buffer with zeroes if required
	offset = 1 + 4 + access_size - size;
	for (index = 1 + 4; index < offset; index++)
		tx_buffer[index] = 0;

	// set value
	for (index = 0; index < size; index++)
		tx_buffer[offset + index] = (unsigned char)(value >> ((size - 1 - index) * 8));

	// perform transfer
	if (spi_transfer(spi, tx_buffer, rx_buffer, 1 + 4 + access_size, false)) {
		dev_err(&spi->dev, "Register write failed.\n");
		return 1;
	}

	return 0;
}

static const struct spi_device_id bcm89230spi_ids[] = {
	{"bcm89230spi", 0},
	{}
};

MODULE_DEVICE_TABLE(spi, bcm89230spi_ids);

/**
 * resume_bcm89230_spi() - release reset and configure bcm89230 after resume from STR.
 * @dev:  description of device
 * return:
 *  0: if ok, non-zero otherwise
 */
static int resume_bcm89230_spi(struct device *dev)
{
	int res;
	struct spi_device *spi = container_of(dev, struct spi_device, dev);

	dev_info(dev, "Resume bcm89230\n");
	res = reset_bcm89230_chip(dev, 1);
	if (res != 0) {
		dev_err(dev, "Resume bcm89230 reset chip failed\n");
		return -2;
	}

	res = configure_bcm89230_chip(spi);
	if (res != 0) {
		dev_err(dev, "Resume bcm89230 configure chip failed\n");
		return -3;
	}
	return res;
}

/**
 * suspend_bcm89230_spi() - resets bcm89230 before going to STR.
 * @dev:  description of device
 * return:
 *  0: if ok, non-zero otherwise
 */
static int suspend_bcm89230_spi(struct device *dev)
{
	int res;
	struct spi_device *spi = container_of(dev, struct spi_device, dev);

	dev_info(dev, "Suspend bcm89230\n");
	res = reset_bcm89230_chip(dev, 0);
	if (res != 0) {
		dev_err(dev, "Suspend bcm89230 reset chip failed\n");
		return -2;
	}
	return res;
}

static SIMPLE_DEV_PM_OPS(bcm89230_pm, suspend_bcm89230_spi, resume_bcm89230_spi);

static struct spi_driver bcm89230spi_driver = {
	.driver = {
		.name = "bcm89230spi", .owner = THIS_MODULE, .pm = &bcm89230_pm,
	},
	.id_table = bcm89230spi_ids,
	.probe = probe_bcm89230_spi,
	.remove = remove_bcm89230_spi,
};

module_spi_driver(bcm89230spi_driver);

MODULE_AUTHOR("Jakub Kowalski, jakub.kowalski@aptiv.com");
MODULE_AUTHOR("Oliver Barta, oliver.barta@aptiv.com");
MODULE_DESCRIPTION("Basic driver for BCM89230 ethernet switch");
MODULE_LICENSE("GPL");
