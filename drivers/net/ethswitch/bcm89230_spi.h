#ifndef BCM89230_SPI_H
#define BCM89230_SPI_H
#include <linux/spi/spi.h>

/** @brief Reads one register of the BCM89230.
 * @param[in]   address    address of the register to read
 * @param[in]   size       size of the register in bits (8, 16, 32 or 64)
 * @param[out]  value      pointer to a location where to store the received value
 * @return
 *  0 on success\n
 *  non-zero otherwise
 */
int read_bcm89230_reg_spi(
	struct spi_device *spi,
	const u32 address,
	unsigned int size,
	uint64_t * const value);

/** @brief Writes value to specified register of the BCM89230.
 * @param[in]   address     register address
 * @param[in]   size        size of register
 * @param[in]   value       value to write
 * @return
 *  0 on success\n
 *  non-zero otherwise
 */
int write_bcm89230_reg_spi(
	struct spi_device *spi,
	const u32 address,
	unsigned int size,
	const u64 value);

/**
 * struct bcm_ee_reg_access - information for manually register access
 *
 * @lock:	protects this structure
 * @address:	address of register to access
 * @bits:	reading/writing register size
 *
 */
struct bcm_ee_reg_access {
	struct mutex lock;
	u32          address;
	u8           bits;
};
/**
 * struct bcm_drvdata - Driver private data exposed driver wide as drvdata
 * @reset_pin:		adress of reset GPIO
 */
struct bcm_drvdata {
	struct gpio_desc *reset_pin;
	struct bcm_ee_reg_access ee_access;
};

/**
 * struct bcm_data - Driver private data used by probe, remove
 * @drvdata:		data exposed as drvdata
 */
struct bcm_data {
	struct bcm_drvdata drvdata;
};
#endif
