/* ============================================================================
 * PROJECT: TC9560
 * Copyright (C) 2018-2020  Toshiba Electronic Devices & Storage Corporation
 *
 * DATE : 24-Mar-2020
 * Driver Version : V_00-08c2
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * ========================================================================= */
/* ============================================================================
 * COPYRIGHT © 2018-2019
 *
 * Toshiba America Electronic Components
 *
 * PROJECT:   TC9560
 *
 * Permission is hereby granted,
 * free of charge, to any person obtaining a copy of this software annotated
 * with this license and the Software, to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *
 * EXAMPLE PROGRAMS ARE PROVIDED AS-IS WITH NO WARRANTY OF ANY KIND, 
 * EITHER EXPRESS OR IMPLIED.
 *
 * TOSHIBA ASSUMES NO LIABILITY FOR CUSTOMERS' PRODUCT DESIGN OR APPLICATIONS.
 * 
 * THIS SOFTWARE IS PROVIDED AS-IS AND HAS NOT BEEN FULLY TESTED.  IT IS
 * INTENDED FOR REFERENCE USE ONLY.
 * 
 * TOSHIBA DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES AND ALL LIABILITY OR
 * ANY DAMAGES ASSOCIATED WITH YOUR USE OF THIS SOFTWARE.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY TOSHIBA SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL TOSHIBA BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * ========================================================================= */
/* =========================================================================
 * The Synopsys DWC ETHER QOS Software Driver and documentation (hereinafter
 * "Software") is an unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto.  Permission is hereby granted,
 * free of charge, to any person obtaining a copy of this software annotated
 * with this license and the Software, to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * ========================================================================= */

/*! History:
 *      2-March-2016 : Initial
 *     21-March-2016 : Modified "pdev->msi_cap + PCI_MSI_MASK_64" register
 *                     read as "pci_write_config_dword"
 */

/*!@file: DWC_ETH_QOS_pci.c
 * @brief: Driver functions.
 */
#include <linux/firmware.h>
#include <../kernel/power/power.h>
#include "DWC_ETH_QOS_yregacc.h"
#include "DWC_ETH_QOS_yheader.h"
#include "DWC_ETH_QOS_pci.h"
#include "DWC_ETH_QOS_sysfs_registers.h"

//#define READ_MAC_FROM_MEM

#ifdef READ_MAC_FROM_MEM
#define MAC_MEM_ADDR
#endif

#ifdef TC9560_LOAD_FW_HEADER
#include "fw_OSLess_HWSeq_emac_v2.5.3.h"
#endif

static char *parent_dev_name;
module_param(parent_dev_name, charp, 0444);
MODULE_PARM_DESC(parent_dev_name, "name of parent device which is needed to be configured after STR before this driver");
static struct device_link *link_to_parent;

static UCHAR dev_addr[6] = { 0xEC, 0x21, 0xE5, 0x0D, 0x4F, 0xEA};
static UCHAR dev_mac_addr0[6] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

typedef struct {
	char mdio_key[32];
	unsigned short mdio_key_len;
	char mac_key[32];
	unsigned short mac_key_len;
	unsigned short mac_str_len;
	char mac_str_def[20];
} config_param_list_t;


const config_param_list_t config_param_list[] = {
{"MDIOBUSID", 9, "MAC_ID", 6, 17, "00:00:00:00:00:00"},
};

USHORT mdio_bus_id;

#define CONFIG_PARAM_NUM (sizeof(config_param_list)/sizeof(config_param_list[0]))
/* Holds virtual address for BAR0 for register access,
   BAR1 for SRAM memory and BAR2 for Flash memory */

#if !defined(TC9560_DECLARE_MEM_FOR_DMAAPI) && !defined(DESC_HOSTMEM_BUF_HOSTMEM)
ULONG cur_virt;
#endif

dma_addr_t dwc_eth_tc9560_hostmem_pci_base_addr_phy;
ULONG dwc_eth_tc9560_hostmem_pci_base_addr_virt;

#ifdef READ_MAC_FROM_MEM
/*!
 * \brief API to read MAC ID from memory location and update the dev_addr
 *
 * \return void
 *
 * \retval 0 on success & -ve number on failure.
 */

static int read_mac_addr_from_memory(void)
{
    unsigned char *mac_addr;
    unsigned int itr = 0;
    int retVal = -1;
	
    /* MAC_MEM_ADDR macro to be initialized with requird MAC address value. 
       copy the MAC address value (MAC_MEM_ADDR) to mac_addr pointer (allocate required memory for mac_addr pointer)
       and read MAC address from fixed memory location
       NOTE : In case of dual EMAC, please add the code to access the fixed memory path for each EMAC MAC ID */

    printk("MAC address read from address 0x%x is %x:%x:%x:%x:%x:%x\n", mac_addr, mac_addr[0], mac_addr[1],
                                                    mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    /* Read and update the MAC address */
    for (itr = 0 ; itr < 6; itr++) {
        dev_addr[itr] = mac_addr[itr];
    }
    retVal = 0;
    return retVal;

}
#endif

void DWC_ETH_QOS_init_all_fptrs(struct DWC_ETH_QOS_prv_data *pdata)
{
	DWC_ETH_QOS_init_function_ptrs_dev(&pdata->hw_if);
	DWC_ETH_QOS_init_function_ptrs_desc(&pdata->desc_if);
}



/*!
 * \brief API to kernel read from file
 *
 * \param[in] file   - pointer to file descriptor
 * \param[in] offset - Offset of file to start read
 * \param[in] size   - Size of the buffer to be read
 *
 * \param[out] data   - File data buffer
 *
 *
 * \return integer
 *
 * \retval 0 on success & -ve number on failure.
 */
static int file_read(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size)
{
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
	    ret = vfs_read(file, data, size, &offset);
#else //4.14.0
		ret = kernel_read(file, data, size, &offset);
#endif //4.14.0

    set_fs(oldfs);
    return ret;
}

/*!
 * \brief API to validate MAC ID
 *
 * \param[in] char *s - pointer to MAC ID string
 *
 * \return boolean
 *
 * \retval true on success and false on failure.
 */
bool isMAC(char *s)
{
    int i = 0;
    if (s == NULL)
	return false;

    for (i = 0; i < 17; i++) {
        if (i % 3 != 2 && !isxdigit(s[i]))
            return false;
        if (i % 3 == 2 && s[i] != ':')
            return false;
    }
    return true;
}

/*!
 * \brief API to extract MAC ID from given string
 *
 * \param[in] char *string - pointer to MAC ID string
 *
 * \return None
 */
void extract_macid(char *string)
{
	char *token_m = NULL;
	int j = 0;
        int mac_id = 0;

	/* Extract MAC ID byte by byte */
	token_m = strsep(&string, ":");
	while (token_m != NULL) {
		sscanf(token_m, "%x", &mac_id);
		dev_addr[j++] = mac_id;
		token_m = strsep(&string, ":");
	}
}

/*!
 * \brief API to parse and extract the user configured MAC ID
 *
 * \param[in] file_buf - Pointer to file data buffer
 *
 * \return boolean
 *
 * \return - True on Success and False in failure
 */
static bool lookfor_macid(char *file_buf)
{
	char *string = NULL, *token_n = NULL, *token_s = NULL, *token_m = NULL;
	bool status = false;
	int tc9560_device_no = 0;

	string = file_buf;
	/* Parse Line-0 */
	token_n = strsep(&string, "\n");
	while (token_n != NULL) {

		/* Check if line is enabled */
		if (token_n[0] != '#') {
			/* Extract the token based space character */
			token_s = strsep(&token_n, " ");
			if (token_s != NULL) {
			if (strncmp(token_s, config_param_list[0].mdio_key, 9) == 0) {
					token_s = strsep(&token_n, " ");
					token_m = strsep(&token_s, ":");
					sscanf(token_m, "%d", &tc9560_device_no);
					if (tc9560_device_no != mdio_bus_id) {
						token_n = strsep(&string, "\n");
						if (token_n == NULL)
							break;
						continue;
					}
				}
			}

			/* Extract the token based space character */
			token_s = strsep(&token_n, " ");
			if (token_s != NULL) {
				/* Compare if parsed string matches with key listed in configuration table */
				if (strncmp(token_s, config_param_list[0].mac_key, 6) == 0) {

					NDBGPR_L1("MAC_ID Key is found\n");
					/* Read next word */
					token_s = strsep(&token_n, " \n");
					if (token_s != NULL) {

						/* Check if MAC ID length  and MAC ID is valid */
						if ((isMAC(token_s) == true) && (strlen(token_s) ==  config_param_list[0].mac_str_len)) {
							/* If user configured MAC ID is valid,  assign default MAC ID */
							extract_macid(token_s);
							status = true;
						} else {
							NMSGPR_ALERT("Valid Mac ID not found\n");
						}
					}
				}
			}
		}
		/* Read next lile */
		token_n = strsep(&string, "\n");
        if (token_n == NULL)
			break;

	}
	return status;
}

/*!
 * \brief Parse the user configuration file for various config
 *
 * \param[in] None
 *
 * \return None
 *
 */

#define MAC_ADDRESS_FILE "/mnt/vendor/oem_config/mac_addr.ini"

static void parse_config_file(void)
{
	struct file *filep = NULL;
	char *data = kmalloc(1000, GFP_KERNEL);
	mm_segment_t oldfs;
	int ret, flags = O_RDONLY, i = 0;

	oldfs = get_fs();
	set_fs(get_ds());
	filep = filp_open(MAC_ADDRESS_FILE, flags, 0600);
	set_fs(oldfs);
	if (IS_ERR(filep)) {
		NMSGPR_ALERT("Mac configuration file not found\n");
		NMSGPR_ALERT("Using Default MAC Address\n");
		return;
	} else {
		/* Parse the file */
		ret = file_read(filep, 0, data, 1000);
		for (i = 0; i < CONFIG_PARAM_NUM; i++) {
			if (strstr((const char *)data, config_param_list[i].mdio_key)) {
				NDBGPR_L1("Pattern Match\n");
				if (strncmp(config_param_list[i].mdio_key, "MDIOBUSID", 9) == 0) {
					/* MAC ID Configuration */
					NDBGPR_L1("MAC_ID Configuration\n");
					if (lookfor_macid(data) == false) {
						//extract_macid ((char *)config_param_list[i].str_def);
					}
				}
			}
		}
	}

	kfree(data);
	filp_close(filep, NULL);

	return;
}

/*!
* \brief Load TC9560 firmware from binary file into SRAM using kernel
*  firmware interface.
*
* \details This function performs following steps:
* \ - Assert TC9560 Cortex M3 system reset.
* \ - Copies TC9560 firmware from a binary file into SRAM.
* \ - De -assert TC9560 Cortex M3 cold and system reset.
*
* \param[in] pdev - pointer to pci_dev structure.
*
* \return integer
*
*/
static int load_tc9560_firmware(struct pci_dev *pdev)
{
    ULONG reg_val;
#ifndef TC9560_LOAD_FW_HEADER		
    const struct firmware *pfw = NULL;
#endif
	struct net_device *dev = pci_get_drvdata(pdev);
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
#ifdef TC9560_LOAD_FW_HEADER	
	ULONG adrs = 0, val = 0;
	UINT fw_size = 0;
#endif	

     NMSGPR_INFO("DWC_ETH_QOS: Start firmware load");

#ifdef TC9560_LOAD_FW_HEADER
	fw_size = sizeof(fw_data);
	if(fw_size > 512*1024){
		printk(KERN_ALERT"Error : FW size exceeds the memory size\n");	
		return -EINVAL;
	}
#else
     /* Get TC9560 FW binary through kernel firmware interface request */
     if (request_firmware(&pfw, FIRMWARE_NAME, &pdev->dev) != 0) {
       NMSGPR_ERR("DWC_ETH_QOS: Error in calling request_firmware ");
       return -EINVAL;
     }

     if (pfw == NULL) {
       NMSGPR_ERR("DWC_ETH_QOS: request_firmware: pfw == NULL");
       return -EINVAL;
     }
#endif

     /* Read current value of NRSTCTRL register */
     TC9560_NRSTCTRL_RgRd(reg_val);

     /* Assert TC9560 CM3 system reset (NRSTCTRL.MCU0RST) */
     TC9560_NRSTCTRL_RgWr(reg_val | 1);

#ifdef TC9560_LOAD_FW_HEADER
	adrs = 0; /* SRAM Start Address */
	do
	{
		val =  fw_data[adrs+0] << 0;
		val |= fw_data[adrs+1] << 8;
		val |= fw_data[adrs+2] << 16;
		val |= fw_data[adrs+3] << 24;
		iowrite32(val, (void*)(pdata->dwc_eth_tc9560_SRAM_pci_base_addr_virt + adrs));
		adrs += 4;
	} while(adrs < fw_size);	
#else
     /* Copy TC9560 FW to SRAM */
     memcpy((char *)pdata->dwc_eth_tc9560_SRAM_pci_base_addr_virt, pfw->data, pfw->size);
#endif

     /* De-assert TC9560 CM3 cold and system reset (NRSTCTRL.MCU1RST and NRSTCTRL.MCU0RST) */
     TC9560_NRSTCTRL_RgWr(reg_val & ~0x03);

#ifndef TC9560_LOAD_FW_HEADER
     /* Release kernel firmware interface */
     release_firmware(pfw);
#endif
     NMSGPR_INFO("DWC_ETH_QOS: Firmware loaded\n");

    return 0;
}

/*!
* \brief API to initialize the device.
*
* \details This probing function gets called (during execution of
* pci_register_driver() for already existing devices or later if a
* new device gets inserted) for all PCI devices which match the ID table
* and are not "owned" by the other drivers yet. This function gets passed
* a "struct pci_dev *" for each device whose entry in the ID table matches
* the device. The probe function returns zero when the driver chooses to take
* "ownership" of the device or an error code (negative number) otherwise.
* The probe function always gets called from process context, so it can sleep.
*
* \param[in] pdev - pointer to pci_dev structure.
* \param[in] id   - pointer to table of device ID/ID's the driver is inerested.
*
* \return integer
*
* \retval 0 on success & -ve number on failure.
*/

static int DWC_ETH_QOS_probe(struct pci_dev *pdev,
				const struct pci_device_id *id)
{

	struct DWC_ETH_QOS_prv_data *pdata = NULL;
	struct net_device *dev = NULL;
	int i, ret = 0;
	struct hw_if_struct *hw_if = NULL;
	struct desc_if_struct *desc_if = NULL;
	UCHAR tx_q_count = 0, rx_q_count = 0;
	unsigned int reg_val;
#ifdef TC9560_DECLARE_MEM_FOR_DMAAPI
	ULONG phy_mem_adrs;
#endif

#ifdef TC9560_ENABLE_PCIE_MEM_ACCESS
	ULONG_LONG adrs_to_be_replaced, adrs_for_replacement;
	UINT tmap_no, no_of_bits;
#endif

	ULONG dwc_eth_tc9560_reg_pci_base_addr;
	ULONG dwc_eth_tc9560_SRAM_pci_base_addr_phy;
	ULONG dwc_eth_tc9560_SRAM_pci_base_addr_virt;
	ULONG dwc_eth_tc9560_FLASH_pci_base_addr;
	ULONG dwc_eth_tc9560_reg_len;
	ULONG dwc_eth_tc9560_SRAM_len;
	ULONG dwc_eth_tc9560_FLASH_len;

	NDBGPR_L1("Debug version\n");

	DBGPR("--> DWC_ETH_QOS_probe\n");

	ret = pci_enable_device(pdev);
	if (ret) {
		NMSGPR_ALERT("%s:Unable to enable device\n", DEV_NAME);
		goto err_out_enb_failed;
	}

    /* Query and set the appropriate masks for DMA operations. */
    if ((pci_set_dma_mask(pdev, DMA_BIT_MASK(64))) &&
         (pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64)))) {
        NMSGPR_ALERT(
               "%s: 64 bits DMA Configuration not supported, aborting\n",
               pci_name(pdev));
		goto err_out_dma_mask_failed;
    }

	if (pci_request_regions(pdev, DEV_NAME)) {
		NMSGPR_ALERT("%s:Failed to get PCI regions\n", DEV_NAME);
		ret = -ENODEV;
		goto err_out_req_reg_failed;
	}
	pci_set_master(pdev);

    /* Read BAR0 and map the TC9560 register base address
       Read BAR1 and map the TC9560 SRAM memory address
       Read BAR2 and map the TC9560 Flash memory address */
    dwc_eth_tc9560_reg_len = pci_resource_len(pdev, 0);
    dwc_eth_tc9560_SRAM_len = pci_resource_len(pdev, 2);
    dwc_eth_tc9560_FLASH_len = pci_resource_len(pdev, 4);

    NDBGPR_L1("BAR0 length = %ld kb\n", dwc_eth_tc9560_reg_len);
    NDBGPR_L1("BAR2 length = %ld kb\n", dwc_eth_tc9560_SRAM_len);
    NDBGPR_L1("BAR4 length = %ld kb\n", dwc_eth_tc9560_FLASH_len);

    dwc_eth_tc9560_reg_pci_base_addr = pci_resource_start(pdev, 0);
    dwc_eth_tc9560_SRAM_pci_base_addr_phy = pci_resource_start(pdev, 2);
    dwc_eth_tc9560_FLASH_pci_base_addr = pci_resource_start(pdev, 4);

    NDBGPR_L1("BAR0 iommu address = 0x%lx\n", dwc_eth_tc9560_reg_pci_base_addr);
    NDBGPR_L1("BAR2 iommu address = 0x%lx\n", dwc_eth_tc9560_SRAM_pci_base_addr_phy);
    NDBGPR_L1("BAR4 iommu address = 0x%lx\n", dwc_eth_tc9560_FLASH_pci_base_addr);

    dwc_eth_tc9560_reg_pci_base_addr = (ULONG)ioremap(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));
    dwc_eth_tc9560_SRAM_pci_base_addr_virt = (ULONG)ioremap(pci_resource_start(pdev, 2), pci_resource_len(pdev, 2));
#if !defined(TC9560_DECLARE_MEM_FOR_DMAAPI) && !defined(DESC_HOSTMEM_BUF_HOSTMEM)
    cur_virt = dwc_eth_tc9560_SRAM_pci_base_addr_virt + TC9560_DMAAPI_MEM_OFFSET;
#endif
    dwc_eth_tc9560_FLASH_pci_base_addr = (ULONG)ioremap(pci_resource_start(pdev, 4), pci_resource_len(pdev, 4));

    NDBGPR_L1("BAR0 virtual address = 0x%lx\n", dwc_eth_tc9560_reg_pci_base_addr);
    NDBGPR_L1("BAR2 virtual address = 0x%lx\n", dwc_eth_tc9560_SRAM_pci_base_addr_virt);
    NDBGPR_L1("BAR4 virtual address = 0x%lx\n", dwc_eth_tc9560_FLASH_pci_base_addr);

    if (((void __iomem *)dwc_eth_tc9560_reg_pci_base_addr == NULL)  ||
        ((void __iomem *)dwc_eth_tc9560_SRAM_pci_base_addr_virt == NULL) ||
        ((void __iomem *)dwc_eth_tc9560_FLASH_pci_base_addr == NULL)) {
        NMSGPR_ALERT(
               "%s: cannot map TC9560 BARs, aborting",
               pci_name(pdev));
        ret = -EIO;
        goto err_out_map_failed;
    }

   {
       unsigned int rd_val;

       rd_val = *(volatile unsigned int *)(dwc_eth_tc9560_reg_pci_base_addr + NCLKCTRL_OFFSET);
       *(volatile unsigned int *)(dwc_eth_tc9560_reg_pci_base_addr + NCLKCTRL_OFFSET) = (rd_val | 0x80); //Enable MAC Clock

       rd_val = *(volatile unsigned int *)(dwc_eth_tc9560_reg_pci_base_addr + NRSTCTRL_OFFSET);
       *(volatile unsigned int *)(dwc_eth_tc9560_reg_pci_base_addr + NRSTCTRL_OFFSET) = (rd_val & ~0x80); //Deassert MAC Reset

       NDBGPR_L1("HFR0 Val = 0x%08x \n", *(volatile unsigned int *)(dwc_eth_tc9560_reg_pci_base_addr + EMAC_HW_FEATURE0_OFFSET));
       NDBGPR_L1("HFR1 Val = 0x%08x \n", *(volatile unsigned int *)(dwc_eth_tc9560_reg_pci_base_addr + EMAC_HW_FEATURE1_OFFSET));
       NDBGPR_L1("HFR2 Val = 0x%08x \n", *(volatile unsigned int *)(dwc_eth_tc9560_reg_pci_base_addr + EMAC_HW_FEATURE2_OFFSET));
    }

	/* queue count */
	tx_q_count = get_tx_queue_count(dwc_eth_tc9560_reg_pci_base_addr);
	rx_q_count = get_rx_queue_count(dwc_eth_tc9560_reg_pci_base_addr);

    NDBGPR_L1("No of TX Queue = %d\n", tx_q_count);
    NDBGPR_L1("No of RX Queue = %d\n", rx_q_count);

	/* TODO: current driver has combine code for TX and RX Q, so considering the maximum (which is rx q count) for now.
     In this case TX may write one extra queue, but as those fields are reserved it will not create any issue. */

	/* Need to add 2 to number of queue as this api corresponds to number of DMA channels */
	dev = alloc_etherdev_mqs(sizeof(struct DWC_ETH_QOS_prv_data),
				(tx_q_count+2), (rx_q_count+2));
	if (dev == NULL) {
		NMSGPR_ALERT("%s:Unable to alloc new net device\n",
		    DEV_NAME);
		ret = -ENOMEM;
		goto err_out_dev_failed;
	}

	++mdio_bus_id;

#ifdef READ_MAC_FROM_MEM
    /* Read mac address from fixed memory location */
    read_mac_addr_from_memory();
#else
	/* Read mac address from mac.ini file */
	parse_config_file();
#endif

	if (!is_valid_ether_addr(dev_addr)) {
		NMSGPR_ERR("Not found valid mac address\n");
		NMSGPR_INFO("Using Default MAC Address\n");
	}
	NMSGPR_INFO("MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
		dev_addr[0], dev_addr[1], dev_addr[2], dev_addr[3], dev_addr[4], dev_addr[5]);
	dev->dev_addr[0] = dev_addr[0];
	dev->dev_addr[1] = dev_addr[1];
	dev->dev_addr[2] = dev_addr[2];
	dev->dev_addr[3] = dev_addr[3];
	dev->dev_addr[4] = dev_addr[4];
	dev->dev_addr[5] = dev_addr[5];

	/* Incrementing the LSB byte of Default MAC address by 2 for every new TC9560 device found on pcie bus if validation of config.ini fails */
    dev_addr[5] = (0xFF & (dev_addr[5] + ((mdio_bus_id)*2)));

	dev->base_addr = dwc_eth_tc9560_reg_pci_base_addr;
	SET_NETDEV_DEV(dev, &pdev->dev);
	pdata = netdev_priv(dev);
	DWC_ETH_QOS_init_all_fptrs(pdata);
	hw_if = &(pdata->hw_if);
	desc_if = &(pdata->desc_if);

	pci_set_drvdata(pdev, dev);
	pdata->pdev = pdev;

	pdata->dev = dev;
	pdata->tx_dma_ch_cnt = tx_q_count + 2;
	pdata->rx_dma_ch_cnt = rx_q_count + 2;
	pdata->dwc_eth_tc9560_FLASH_pci_base_addr = dwc_eth_tc9560_FLASH_pci_base_addr;
	pdata->dwc_eth_tc9560_SRAM_pci_base_addr_virt = dwc_eth_tc9560_SRAM_pci_base_addr_virt;

	if ((pdata->rx_dma_ch_cnt > TC9560_DMA_RX_CH_CNT) || (pdata->tx_dma_ch_cnt > TC9560_DMA_TX_CH_CNT)) {
		NMSGPR_ALERT("Bug: Allocated memory is not enough for dma ch indicator \n");
	}
	if ((tx_q_count > TC9560_TX_Q_CNT) || (rx_q_count > TC9560_RX_Q_CNT)) {
		NMSGPR_ALERT("Bug: Allocated memory is not enough for Q indicator \n");
	}

	/* 	Host: TX DMA CH : 0,2,3,4,
		Host: RX DMA CH : 0,2,3,4,5 */
	for (i = 0; i < pdata->rx_dma_ch_cnt && i < TC9560_DMA_RX_CH_CNT; i++)
		pdata->rx_dma_ch_for_host[i] = 1;
	for (i = 0; i < pdata->tx_dma_ch_cnt && i < TC9560_DMA_TX_CH_CNT; i++)
		pdata->tx_dma_ch_for_host[i] = 1;

	/* 	Host: TX Q : 0,1,2
		Host: RX Q : 0,1,2,3 */
	for (i = 0; i < rx_q_count && i < TC9560_RX_Q_CNT; i++)
		pdata->rx_q_for_host[i] = 1;
	for (i = 0; i < tx_q_count && i < TC9560_TX_Q_CNT; i++)
		pdata->tx_q_for_host[i] = 1;


	/* 	M3: TX DMA CH : 1
		M3: RX DMA CH : 1 */
	pdata->tx_dma_ch_for_host[1] = 0;
	pdata->rx_dma_ch_for_host[1] = 0;

#ifdef TC9560_DECLARE_MEM_FOR_DMAAPI
	phy_mem_adrs = dwc_eth_tc9560_SRAM_pci_base_addr_phy;
	ret = dma_declare_coherent_memory(&pdata->pdev->dev, phy_mem_adrs + TC9560_DMAAPI_MEM_OFFSET,
              TC9560_DMAAPI_MEM_BASE + TC9560_DMAAPI_MEM_OFFSET,
              TC9560_DMAAPI_MEM_LENGTH,
              DMA_MEMORY_MAP | DMA_MEMORY_EXCLUSIVE);
        if (ret == 0) {
                NMSGPR_ALERT("Coherent memory declaration error!!\n");
                ret = -ENXIO;
                goto err_coherent_mem_declaration;
	}
#endif

#ifdef TC9560_TX_DATA_BUF_IN_SRAM
	pdata->tx_mem_pool = dma_pool_create("TX_MEMORY_POOL", &pdata->pdev->dev,
			TC9560_TX_MEM_POOL_SIZE, TC9560_TX_MEM_POOL_ALIGN, TC9560_TX_MEM_POOL_ALLOC);
	if (pdata->tx_mem_pool == NULL) {
                NMSGPR_ALERT("Cann't create TX Memory Pool\n");
                ret = -ENOMEM;
		goto err_tx_mem_pool_creation;
	}
#endif
#ifdef TC9560_RX_DATA_BUF_IN_SRAM
	pdata->rx_mem_pool = dma_pool_create("RX_MEMORY_POOL", &pdata->pdev->dev,
			TC9560_RX_MEM_POOL_SIZE, TC9560_RX_MEM_POOL_ALIGN, TC9560_RX_MEM_POOL_ALLOC);
	if (pdata->rx_mem_pool == NULL) {
                NMSGPR_ALERT("Cann't create RX Memory Pool\n");
                ret = -ENOMEM;
		goto err_rx_mem_pool_creation;
	}
#endif

    pdata->tc9560_timestamp_valid_window = TC9560_TS_WINDOW;
    if ((int)pdata->tc9560_timestamp_valid_window < 0 || pdata->tc9560_timestamp_valid_window > 255) {
        NMSGPR_INFO("Setting default timestamp window\n");
        pdata->tc9560_timestamp_valid_window = 4;
    }

#ifdef TC9560_ENABLE_PCIE_MEM_ACCESS
	/* Configure TMAP 0 to access full range of host memory */
	tmap_no = 0;
	adrs_to_be_replaced = ((unsigned long long)0x00000010 << 32);
	adrs_for_replacement = ((unsigned long long)0x00000000 << 32);
	no_of_bits = 28;
	hw_if->tc9560_config_tamap(tmap_no, adrs_to_be_replaced, adrs_for_replacement, no_of_bits, pdata);
#endif

#if defined(PHY_MII)
	/* PHY Mode (MII/RMII or default RGMII)
	* should be configured prior to GMAC clock enable/reset. */
	hw_if->tc9560_set_tx_clk_25MHz(pdata);
	NMSGPR_INFO( "%s:\t Fixed MII speed-100,tx_clk-25MHz MII\n", __func__);
	hw_if->tc9560_set_phy_intf_MII(pdata); 
	NMSGPR_INFO( "%s:\t Fixed MII mode.\n", __func__);
	hw_if->set_full_duplex(pdata);
	NMSGPR_INFO( "%s:\t Fixed Full Duplex mode.\n", __func__);
#endif

#if defined(PHY_RMII)
	/* PHY Mode (MII/RMII or default RGMII)
	* should be configured prior to GMAC clock enable/reset. */
	hw_if->tc9560_set_tx_clk_25MHz_RMII(pdata);
	NMSGPR_INFO( "%s:\t Fixed RMII speed-100,tx_clk-25MHz RMII\n", __func__);
	hw_if->tc9560_set_phy_intf_RMII(pdata); 
	NMSGPR_INFO( "%s:\t Fixed RMII mode.\n", __func__);
	hw_if->set_full_duplex(pdata);
	NMSGPR_INFO( "%s:\t Fixed Full Duplex mode.\n", __func__);
#endif

    /* issue clock enable to GMAC device */
    hw_if->tc9560_mac_clock_config(0x1, pdata);
    /* issue software reset to GMAC device */
    hw_if->exit(pdata);

	//Assert TDM reset, in case if it is on.
	reg_val = hw_if->tc9560_reg_rd(NRSTCTRL_OFFSET, 0, pdata);
	reg_val |= (0x1<<6);
	hw_if->tc9560_reg_wr(NRSTCTRL_OFFSET, reg_val, 0, pdata);

	/* Enable MSI support: following commneted code just enables the one MSI interrupt. This is only kept for debugging purpose.*/
#if 1
    ret = pci_enable_msi(pdev);
    if (ret) {
	NMSGPR_ALERT("%s:Enable MSI error\n", DEV_NAME);
	goto err_out_msi_failed;
    }
    pci_write_config_dword(pdev, pdev->msi_cap + PCI_MSI_MASK_64, 0);
#else
    pdata->max_irq = TC9560_MAX_MSI;
        ret = pci_enable_msi_range(pdev, pdata->max_irq, pdata->max_irq);
        if (ret < 0) { //Failed to allocate
            NMSGPR_ALERT("%s:Enable MSI error\n", DEV_NAME);
            goto err_out_msi_failed;
        } else if (ret > 0) {   //This is the possible number that device can allocate
            pdata->max_irq = ret;
    	    NMSGPR_ALERT("Changing max no of MSI to = %d\n", pdata->max_irq);
        }
    NMSGPR_ALERT("Allocated MSI = %d\n", pdata->max_irq);
#endif
    //  Only one MSI supported for now.
	dev->irq = pdev->irq;
        NDBGPR_L1("Allocated IRQ Number = %d\n", dev->irq);

	NMSGPR_INFO("%s:\t Host Driver Version %s", __func__, DRIVER_VERSION);

	reg_val = hw_if->tc9560_reg_rd(TC9560_MODE_STATUS_OFFSET, 0, pdata);

	/* Check for Host initiated boot mode Bit[6] = 1 */
    if ((reg_val & TC9560_NMODESTS_HOST_BOOT_MASK) == TC9560_NMODESTS_HOST_BOOT_MASK) {
        /* Load firmware */
       ret = load_tc9560_firmware(pdev);
       if (ret < 0) {
             NMSGPR_ALERT("%s:\t Firmware load failed \n",__func__);
             goto err_firmware_load_failed;
       }
		NMSGPR_INFO("%s:\t Firmware Version  %s", __func__, FW_VERSION);
    } else {    	
		unsigned char major_num;
		unsigned char minor_num;
		unsigned char patch_num;

		patch_num = ioread8((void *)(pdata->dwc_eth_tc9560_SRAM_pci_base_addr_virt + M3_SRAM_DEBUG_OFFSET + M3STAT_FW_VER_OFFSET));
		minor_num = ioread8((void *)(pdata->dwc_eth_tc9560_SRAM_pci_base_addr_virt + M3_SRAM_DEBUG_OFFSET + M3STAT_FW_VER_OFFSET + 0x01));
		major_num = ioread8((void *)(pdata->dwc_eth_tc9560_SRAM_pci_base_addr_virt + M3_SRAM_DEBUG_OFFSET + M3STAT_FW_VER_OFFSET + 0x02));

		NMSGPR_INFO("Firmware Version  V%d.%d.%d", major_num,minor_num,patch_num);
    }

	DWC_ETH_QOS_get_all_hw_features(pdata);
	DWC_ETH_QOS_print_all_hw_features(pdata);

	ret = desc_if->alloc_queue_struct(pdata);
	if (ret < 0) {
		NMSGPR_ALERT("ERROR: Unable to alloc Tx/Rx queue\n");
		goto err_out_q_alloc_failed;
	}

	dev->netdev_ops = DWC_ETH_QOS_get_netdev_ops();

	pdata->interface = DWC_ETH_QOS_get_phy_interface(pdata);
	/* Bypass PHYLIB for TBI, RTBI and SGMII interface */
	if (1 == pdata->hw_feat.sma_sel) {
		ret = DWC_ETH_QOS_mdio_register(dev);
		if (ret < 0) {
			NMSGPR_ALERT("MDIO bus (id %d) registration failed\n",
			       pdata->bus_id);
			goto err_out_mdio_reg;
		}
	} else {
		NMSGPR_ALERT("%s: MDIO is not present\n\n", DEV_NAME);
	}

	/* enabling and registration of irq with magic wakeup */
	if (1 == pdata->hw_feat.mgk_sel) {
		device_set_wakeup_capable(&pdev->dev, 1);
		device_set_wakeup_enable(&pdata->pdev->dev, 1);
		pdata->wolopts = WAKE_MAGIC;
		enable_irq_wake(dev->irq);
	}

	for (i = 0; i < TC9560_RX_DMA_CH_CNT; i++) {
		struct DWC_ETH_QOS_rx_dma_ch *rx_dma_ch = GET_RX_DMA_CH_PTR(i);
		if (!pdata->rx_dma_ch_for_host[i])
			continue;
        rx_dma_ch->chInx = i;
		netif_napi_add(dev, &rx_dma_ch->napi, DWC_ETH_QOS_poll_mq,
				(64));
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 16, 0))
	SET_ETHTOOL_OPS(dev, DWC_ETH_QOS_get_ethtool_ops());
#else	//3.16.0
	netdev_set_default_ethtool_ops(dev, DWC_ETH_QOS_get_ethtool_ops());
#endif	//3.16.0

	DWC_ETH_QOS_reset_ethtool_stats(pdata);


	if (pdata->hw_feat.tso_en) {
		dev->hw_features = NETIF_F_TSO;
		dev->hw_features |= NETIF_F_SG;
		dev->hw_features |= NETIF_F_IP_CSUM;
		dev->hw_features |= NETIF_F_IPV6_CSUM;
		NDBGPR_L2("Supports TSO, SG and TX COE\n");
	} else if (pdata->hw_feat.tx_coe_sel) {
		dev->hw_features = NETIF_F_IP_CSUM;
		dev->hw_features |= NETIF_F_IPV6_CSUM;
		NDBGPR_L2("Supports TX COE\n");
	}

	if (pdata->hw_feat.rx_coe_sel) {
		dev->hw_features |= NETIF_F_RXCSUM;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 5, 7))
		dev->hw_features |= NETIF_F_LRO;
		NDBGPR_L2("Supports RX COE and LRO\n");
#else
		dev->hw_features |= NETIF_F_GRO;
		NDBGPR_L2("Supports RX COE and GRO\n");
#endif
	}
#ifdef DWC_ETH_QOS_ENABLE_VLAN_TAG
	dev->vlan_features |= dev->hw_features;
	dev->hw_features |= NETIF_F_HW_VLAN_CTAG_RX;
	if (pdata->hw_feat.sa_vlan_ins) {
		dev->hw_features |= NETIF_F_HW_VLAN_CTAG_TX;
		NDBGPR_L2("VLAN Feature enabled\n");
	}
	if (pdata->hw_feat.vlan_hash_en) {
		dev->hw_features |= NETIF_F_HW_VLAN_CTAG_FILTER;
		NDBGPR_L2("VLAN HASH Filtering enabled\n");
	}
#endif /* end of DWC_ETH_QOS_ENABLE_VLAN_TAG */
	dev->features |= dev->hw_features;
	pdata->dev_state |= dev->features;

	DWC_ETH_QOS_init_rx_coalesce(pdata);

#ifdef DWC_ETH_QOS_CONFIG_PTP
	DWC_ETH_QOS_ptp_init(pdata);
#endif	/* end of DWC_ETH_QOS_CONFIG_PTP */

	spin_lock_init(&pdata->lock);
	spin_lock_init(&pdata->tx_lock);
	mutex_init(&pdata->pmt_lock);

	INIT_WORK(&pdata->powerup_work, DWC_ETH_QOS_powerup_handler);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
	dev->min_mtu = DWC_ETH_QOS_MIN_SUPPORTED_MTU;
	dev->max_mtu = DWC_ETH_QOS_MAX_SUPPORTED_MTU;
#endif

	ret = register_netdev(dev);
	if (ret) {
		NMSGPR_ALERT("%s: Net device registration failed\n",
		    DEV_NAME);
		goto err_out_netdev_failed;
	}

	DBGPR("<-- DWC_ETH_QOS_probe\n");

	if (pdata->hw_feat.pcs_sel) {
		netif_carrier_off(dev);
		NMSGPR_ALERT("carrier off till LINK is up\n");
	}

	dwc_create_debugfs(dev);

	if (parent_dev_name) {
		struct kobject *parent_kobj = kset_find_obj(dev->dev.kobj.kset, parent_dev_name);

		if (parent_kobj) {
			struct device *parent_dev = container_of(parent_kobj, struct device, kobj);

			// Set driver as consumer to parent device specified in module param.
			// It is needed to guarantee correct resuming after STR: first parent_dev, next Toshiba.
			// DL_FLAG_STATELESS to avoid automatic link delete when parent driver is removed but device not.
			link_to_parent = device_link_add(&pdev->dev, parent_dev, DL_FLAG_STATELESS);
			if (!link_to_parent)
				NMSGPR_ALERT("%s: link to parent dev=%s - FAILED\n", __func__,  parent_dev_name);
		} else {
			NMSGPR_ALERT("%s: Parent device %s - NOT found\n", __func__, parent_dev_name);
		}
	}
	return 0;

 err_out_netdev_failed:
#ifdef DWC_ETH_QOS_CONFIG_PTP
	DWC_ETH_QOS_ptp_remove(pdata);
#endif	/* end of DWC_ETH_QOS_CONFIG_PTP */

	if (1 == pdata->hw_feat.sma_sel)
		DWC_ETH_QOS_mdio_unregister(dev);

 err_out_mdio_reg:
	desc_if->free_queue_struct(pdata);

 err_firmware_load_failed:
 err_out_q_alloc_failed:
    pci_disable_msi(pdev);
 err_out_msi_failed:

#ifdef TC9560_RX_DATA_BUF_IN_SRAM
    dma_pool_destroy(pdata->rx_mem_pool);
 err_rx_mem_pool_creation:
#endif
#ifdef TC9560_TX_DATA_BUF_IN_SRAM
    dma_pool_destroy(pdata->tx_mem_pool);
 err_tx_mem_pool_creation:
#endif
#ifdef TC9560_DECLARE_MEM_FOR_DMAAPI
    dma_release_declared_memory(&pdata->pdev->dev);

 err_coherent_mem_declaration:
#endif

	/* issue clock disable to GMAC device */
    hw_if->tc9560_mac_clock_config(0x0, pdata);

	free_netdev(dev);
	pci_set_drvdata(pdev, NULL);

 err_out_dev_failed:
    if ((void __iomem *)dwc_eth_tc9560_reg_pci_base_addr != NULL)
	    pci_iounmap(pdev, (void __iomem *)dwc_eth_tc9560_reg_pci_base_addr);

 err_out_map_failed:
	pci_release_regions(pdev);

 err_out_dma_mask_failed:
 err_out_req_reg_failed:
	pci_disable_device(pdev);

 err_out_enb_failed:

	return ret;
}

/*!
* \brief API to release all the resources from the driver.
*
* \details The remove function gets called whenever a device being handled
* by this driver is removed (either during deregistration of the driver or
* when it is manually pulled out of a hot-pluggable slot). This function
* should reverse operations performed at probe time. The remove function
* always gets called from process context, so it can sleep.
*
* \param[in] pdev - pointer to pci_dev structure.
*
* \return void
*/

static void DWC_ETH_QOS_remove(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct desc_if_struct *desc_if = &(pdata->desc_if);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	struct DWC_ETH_QOS_rx_dma_ch *rx_dma_ch = NULL;
	int i;
	DBGPR("--> DWC_ETH_QOS_remove\n");

	dwc_remove_debugfs(dev);

	if (pdata->irq_number != 0) {
		disable_irq_wake(dev->irq);
		free_irq(pdata->irq_number, pdata);
		pdata->irq_number = 0;
	}

	if (1 == pdata->hw_feat.sma_sel)
		DWC_ETH_QOS_mdio_unregister(dev);

#ifdef DWC_ETH_QOS_CONFIG_PTP
	DWC_ETH_QOS_ptp_remove(pdata);
#endif /* end of DWC_ETH_QOS_CONFIG_PTP */

	unregister_netdev(dev);

#ifdef TC9560_RX_DATA_BUF_IN_SRAM
    dma_pool_destroy(pdata->rx_mem_pool);
#endif
#ifdef TC9560_TX_DATA_BUF_IN_SRAM
    dma_pool_destroy(pdata->tx_mem_pool);
#endif
#ifdef TC9560_DECLARE_MEM_FOR_DMAAPI
    dma_release_declared_memory(&pdata->pdev->dev);
#endif

	/* issue clock disable to GMAC device */
    hw_if->tc9560_mac_clock_config(0x0, pdata);

	/* If NAPI is enabled, delete any references to the NAPI struct. */
	for (i = 0; i < TC9560_RX_DMA_CH_CNT; i++) {
		rx_dma_ch = GET_RX_DMA_CH_PTR(i);
		if (!pdata->rx_dma_ch_for_host[i])
			continue;
		netif_napi_del(&rx_dma_ch->napi);
	}

	desc_if->free_queue_struct(pdata);

	free_netdev(dev);
    pci_disable_msi(pdev);
	pci_set_drvdata(pdev, NULL);

	if ((void __iomem *)pdata->dev->base_addr != NULL)
        pci_iounmap(pdev, (void __iomem *)pdata->dev->base_addr);

	if ((void __iomem *)pdata->dwc_eth_tc9560_SRAM_pci_base_addr_virt != NULL)
		pci_iounmap(pdev, (void __iomem *)pdata->dwc_eth_tc9560_SRAM_pci_base_addr_virt);
	if ((void __iomem *)pdata->dwc_eth_tc9560_FLASH_pci_base_addr != NULL)
		pci_iounmap(pdev, (void __iomem *)pdata->dwc_eth_tc9560_FLASH_pci_base_addr);

	pci_release_regions(pdev);
	pci_disable_device(pdev);

	if (link_to_parent)
		device_link_del(link_to_parent);

	DBGPR("<-- DWC_ETH_QOS_remove\n");

	return;
}

static struct pci_device_id DWC_ETH_QOS_id[2] = {
	{PCI_DEVICE(VENDOR_ID, DEVICE_ID), PCI_DEVICE_CLASS(PCI_NVMC_CLASS_CODE, 0xffffff)},
	{PCI_DEVICE(VENDOR_ID, DEVICE_ID), PCI_DEVICE_CLASS(PCI_ETHC_CLASS_CODE, 0xffffff)} 
};

struct pci_dev *DWC_ETH_QOS_pcidev;

static struct pci_driver DWC_ETH_QOS_pci_driver = {
	.name = "DWC_ETH_QOS",
	.id_table = DWC_ETH_QOS_id,
	.probe = DWC_ETH_QOS_probe,
	.remove = DWC_ETH_QOS_remove,
	.shutdown = DWC_ETH_QOS_shutdown,
	.suspend_late = DWC_ETH_QOS_suspend_late,
	.resume_early = DWC_ETH_QOS_resume_early,
#ifdef CONFIG_PM
	.suspend = DWC_ETH_QOS_suspend,
	.resume = DWC_ETH_QOS_resume,
#endif
	.driver = {
		   .name = DEV_NAME,
		   .owner = THIS_MODULE,
	},
};

static void DWC_ETH_QOS_shutdown(struct pci_dev *pdev)
{
	return;
}

static INT DWC_ETH_QOS_suspend_late(struct pci_dev *pdev, pm_message_t state)
{
	return 0;
}

static INT DWC_ETH_QOS_resume_early(struct pci_dev *pdev)
{
	return 0;
}

#ifdef CONFIG_PM

/*!
 * \brief Routine to put the device in suspend mode
 *
 * \details This function gets called by PCI core when the device is being
 * suspended. The suspended state is passed as input argument to it.
 * Following operations are performed in this function,
 * - stop the phy.
 * - detach the device from stack.
 * - stop the queue.
 * - Disable napi.
 * - Stop DMA TX and RX process.
 * - Enable power down mode using PMT module or disable MAC TX and RX process.
 * - Save the pci state.
 *
 * \param[in] pdev – pointer to pci device structure.
 * \param[in] state – suspend state of device.
 *
 * \return int
 *
 * \retval 0
 */

static INT DWC_ETH_QOS_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	INT ret, pmt_flags = 0;
	UINT mac_addr;
#ifdef  TC9560_UNSUPPORTED_FEATURE	
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	unsigned int rwk_filter_values[] = {
		/* for filter 0 CRC is computed on 0 - 7 bytes from offset */
		0x000000ff,

		/* for filter 1 CRC is computed on 0 - 7 bytes from offset */
		0x000000ff,

		/* for filter 2 CRC is computed on 0 - 7 bytes from offset */
		0x000000ff,

		/* for filter 3 CRC is computed on 0 - 31 bytes from offset */
		0x000000ff,

		/* filter 0, 1 independently enabled and would apply for
		 * unicast packet only filter 3, 2 combined as,
		 * "Filter-3 pattern AND NOT Filter-2 pattern" */
		0x03050101,

		/* filter 3, 2, 1 and 0 offset is 50, 58, 66, 74 bytes
		 * from start */
		0x4a423a32,

		/* pattern for filter 1 and 0, "0x55", "11", repeated 8 times */
		0xe7b77eed,

		/* pattern for filter 3 and 4, "0x44", "33", repeated 8 times */
		0x9b8a5506,
	};
#endif

	DBGPR("-->DWC_ETH_QOS_suspend\n");

	if (!dev || (!pdata->hw_feat.mgk_sel &&
			!pdata->hw_feat.rwk_sel)) {
		DBGPR("<--DWC_ETH_QOS_dev_suspend\n");
		return -EINVAL;
	}

    if(pdata->eee_enabled) {
        DWC_ETH_QOS_disable_eee_mode(pdata);
    }

	if (pdata->hw_feat.rwk_sel && (pdata->wolopts & WAKE_UCAST)) {
#ifdef  TC9560_UNSUPPORTED_FEATURE
		pmt_flags |= DWC_ETH_QOS_REMOTE_WAKEUP;
		hw_if->configure_rwk_filter(rwk_filter_values, 8, pdata);
#endif
	}

	if (pdata->hw_feat.mgk_sel && (pdata->wolopts & WAKE_MAGIC))
		pmt_flags |= DWC_ETH_QOS_MAGIC_WAKEUP;

		MAC_MA0HR_RgRd(mac_addr);
		mac_addr &= 0x0000FFFF;
		dev_mac_addr0[5] = (mac_addr >> 8) & 0xFF;
		dev_mac_addr0[4] = (mac_addr & 0xFF);

		MAC_MA0LR_RgRd(mac_addr);
		dev_mac_addr0[3] = (mac_addr & 0xFF000000) >> 24;
		dev_mac_addr0[2] = (mac_addr & 0x00FF0000) >> 16;
		dev_mac_addr0[1] = (mac_addr & 0x0000FF00) >> 8;
		dev_mac_addr0[0] = (mac_addr & 0x000000FF);

		MAC_MA0HR_RgWr((((pdata->dev->dev_addr[5]) << 8) |
						(pdata->dev->dev_addr[4])));
		MAC_MA0HR_AE_UdfWr(0x1);
		MAC_MA0LR_RgWr(((pdata->dev->dev_addr[3] << 24) |
						(pdata->dev->dev_addr[2] << 16) |
						(pdata->dev->dev_addr[1] << 8) |
						(pdata->dev->dev_addr[0])));
	ret = DWC_ETH_QOS_powerdown(dev, pmt_flags, DWC_ETH_QOS_DRIVER_CONTEXT);
	pci_save_state(pdev);
	if (pm_test_level == TEST_DEVICES)
		NMSGPR_INFO("%s: Skipping power state transition\n", dev->name);
	else
		pci_set_power_state(pdev, pci_choose_state(pdev, state));

	DBGPR("<--DWC_ETH_QOS_suspend\n");

	return ret;
}

/*!
 * \brief Routine to resume device operation
 *
 * \details This function gets called by PCI core when the device is being
 * resumed. It is always called after suspend has been called. These function
 * reverse operations performed at suspend time. Following operations are
 * performed in this function,
 * - restores the saved pci power state.
 * - Wakeup the device using PMT module if supported.
 * - Starts the phy.
 * - Enable MAC and DMA TX and RX process.
 * - Attach the device to stack.
 * - Enable napi.
 * - Starts the queue.
 *
 * \param[in] pdev – pointer to pci device structure.
 *
 * \return int
 *
 * \retval 0
 */

static INT DWC_ETH_QOS_resume(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	INT ret;
	unsigned int reg_val;

#ifdef TC9560_ENABLE_PCIE_MEM_ACCESS
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	ULONG_LONG adrs_to_be_replaced, adrs_for_replacement;
	UINT tmap_no, no_of_bits;
#endif
	DBGPR("-->DWC_ETH_QOS_resume\n");

	if (!dev) {
		DBGPR("<--DWC_ETH_QOS_dev_resume\n");
		return -EINVAL;
	}

	if (pm_test_level == TEST_DEVICES)
		NMSGPR_INFO("%s: Skipping power state transition\n", dev->name);
	else
		pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);

#if defined(PHY_RMII)
    /* Reconfigure PHY on resume */
    hw_if->tc9560_set_tx_clk_25MHz_RMII(pdata);
    hw_if->tc9560_set_phy_intf_RMII(pdata);
    hw_if->set_full_duplex(pdata);
    NMSGPR_INFO( "%s:\t RMII mode.\n", __func__);
#endif
#if defined(PHY_MII) 
    hw_if->tc9560_set_tx_clk_25MHz(pdata); 
    hw_if->tc9560_set_phy_intf_MII(pdata); 
    hw_if->set_full_duplex(pdata);
    NMSGPR_INFO( "%s:\t MII mode.\n", __func__);
#endif

    /* issue clock enable to GMAC device */
    hw_if->tc9560_mac_clock_config(0x1, pdata);

    /* issue software reset to GMAC device */ 
    hw_if->exit(pdata);

	/* Enables selected interface and does configuration */
	TC9560_NEMACCTL_RgRd(reg_val);
	reg_val &= TC9560_NEMACCTL_TX_INTERFACE_MASK;
	if (ENABLE_RMII_INTERFACE == INTERFACE_SELECTED) {
		reg_val |= TC9560_NEMACCTL_TX_RMII_INTERFACE;
	} else if (ENABLE_RGMII_INTERFACE == INTERFACE_SELECTED) {
		reg_val |= TC9560_NEMACCTL_TX_RGMII_INTERFACE;
	}
	TC9560_NEMACCTL_RgWr(reg_val);

	reg_val = hw_if->tc9560_reg_rd(TC9560_MODE_STATUS_OFFSET, 0, pdata);

	/* Check for Host initiated boot mode Bit[6] = 1 */
	if ((reg_val & TC9560_NMODESTS_HOST_BOOT_MASK) == TC9560_NMODESTS_HOST_BOOT_MASK) {
	/* Load firmware */
	ret = load_tc9560_firmware(pdev);
	if (ret < 0) {
	     NMSGPR_ERR(" Firmware load failed \n");
	     return -EINVAL;
	}
}

#ifdef TC9560_ENABLE_PCIE_MEM_ACCESS
	hw_if = &(pdata->hw_if);
	/* Configure TMAP 0 to access full range of host memory */
	tmap_no = 0;
	adrs_to_be_replaced = ((unsigned long long)0x00000010 << 32);
	adrs_for_replacement = ((unsigned long long)0x00000000 << 32);
	no_of_bits = 28;
	hw_if->tc9560_config_tamap(tmap_no, adrs_to_be_replaced, adrs_for_replacement, no_of_bits, pdata);
#endif

	/* Restore MAC ADDR0 */
	MAC_MA0HR_RgWr((((dev_mac_addr0[5]) << 8) |
	                (dev_mac_addr0[4])));
	MAC_MA0HR_AE_UdfWr(0x1);
	MAC_MA0LR_RgWr(((dev_mac_addr0[3] << 24) |
	                (dev_mac_addr0[2] << 16) |
	                (dev_mac_addr0[1] << 8) |
	                (dev_mac_addr0[0])));

	ret = DWC_ETH_QOS_powerup(dev, DWC_ETH_QOS_DRIVER_CONTEXT);
   	pm_wakeup_event(&pdev->dev, 5000);

	DBGPR("<--DWC_ETH_QOS_resume\n");

	return ret;
}

#endif	/* CONFIG_PM */

/*!
* \brief API to register the driver.
*
* \details This is the first function called when the driver is loaded.
* It register the driver with PCI sub-system
*
* \return void.
*/

static int __init DWC_ETH_QOS_init_module(void)
{
	INT ret = 0;

	DBGPR("-->DWC_ETH_QOS_init_module\n");

	ret = pci_register_driver(&DWC_ETH_QOS_pci_driver);
	if (ret < 0) {
		NMSGPR_ALERT("DWC_ETH_QOS:driver registration failed");
		return ret;
	}

	DBGPR("<--DWC_ETH_QOS_init_module\n");

	return ret;
}

/*!
* \brief API to unregister the driver.
*
* \details This is the first function called when the driver is removed.
* It unregister the driver from PCI sub-system
*
* \return void.
*/

static void __exit DWC_ETH_QOS_exit_module(void)
{
	DBGPR("-->DWC_ETH_QOS_exit_module\n");

	pci_unregister_driver(&DWC_ETH_QOS_pci_driver);

	DBGPR("<--DWC_ETH_QOS_exit_module\n");
}

/*!
* \brief Macro to register the driver registration function.
*
* \details A module always begin with either the init_module or the function
* you specify with module_init call. This is the entry function for modules;
* it tells the kernel what functionality the module provides and sets up the
* kernel to run the module's functions when they're needed. Once it does this,
* entry function returns and the module does nothing until the kernel wants
* to do something with the code that the module provides.
*/
module_init(DWC_ETH_QOS_init_module);

/*!
* \brief Macro to register the driver un-registration function.
*
* \details All modules end by calling either cleanup_module or the function
* you specify with the module_exit call. This is the exit function for modules;
* it undoes whatever entry function did. It unregisters the functionality
* that the entry function registered.
*/
module_exit(DWC_ETH_QOS_exit_module);

/*!
* \brief Macro to declare the module author.
*
* \details This macro is used to declare the module's authore.
*/
MODULE_AUTHOR("TAEC/TDSC");

/*!
* \brief Macro to describe what the module does.
*
* \details This macro is used to describe what the module does.
*/
MODULE_DESCRIPTION("TC9560 Driver");

/*!
* \brief Macro to describe the module license.
*
* \details This macro is used to describe the module license.
*/
MODULE_LICENSE("GPL");
