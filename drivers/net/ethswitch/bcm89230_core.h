#ifndef BCM89230_CORE_H
#define BCM89230_CORE_H
#include <linux/spi/spi.h>
#include "bcm89230_actions.h"

int
configure_bcm89230_chip(struct spi_device *spi);
int
perform_action_reg_returns(struct spi_device *spi, const struct ACTION_ITEM * const action, uint64_t * const result);
int
perform_action(struct spi_device *spi, const struct ACTION_ITEM * const action);

#endif
