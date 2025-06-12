#ifndef DWC_QOS_SYSFS_REGISTERS_H
#define DWC_QOS_SYSFS_REGISTERS_H

#include <linux/debugfs.h>
#include <linux/seq_file.h>

void dwc_create_debugfs(struct net_device *dev);
void dwc_remove_debugfs(struct net_device *dev);

#endif
