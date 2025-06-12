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
 */

/*!@file: DWC_ETH_QOS_mdio.c
 * @brief: Driver functions.
 */
#include "DWC_ETH_QOS_yheader.h"

/*!
* \brief read MII PHY register, function called by the driver alone
*
* \details Read MII registers through the API read_phy_reg where the
* related MAC registers can be configured.
*
* \param[in] pdata - pointer to driver private data structure.
* \param[in] phyaddr - the phy address to read
* \param[in] phyreg - the phy regiester id to read
* \param[out] phydata - pointer to the value that is read from the phy registers
*
* \return int
*
* \retval  0 - successfully read data from register
* \retval -1 - error occurred
* \retval  1 - if the feature is not defined.
*/

extern USHORT mdio_bus_id;

INT DWC_ETH_QOS_mdio_read_direct(struct DWC_ETH_QOS_prv_data *pdata,
				 int phyaddr, int phyreg, int *phydata)
{
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	int phy_reg_read_status;
	int dev_type = 0;

	DBGPR_MDIO("--> DWC_ETH_QOS_mdio_read_direct: phyaddr = %d, phyreg = %d\n",
	      phyaddr, phyreg);

	if (phyreg & MII_ADDR_C45) {
		dev_type = ((phyreg >> 16) & 0x1f);
		if (hw_if->read_phy_regs_c45) {
			phy_reg_read_status =
				hw_if->read_phy_regs_c45(phyaddr, dev_type, phyreg, phydata, pdata);
		} else {
			phy_reg_read_status = 1;
			NMSGPR_ALERT("%s: hw_if->read_phy_regs_c45 not defined",
				   DEV_NAME);
		}
	} else {
		if (hw_if->read_phy_regs) {
			phy_reg_read_status =
		    		hw_if->read_phy_regs(phyaddr, phyreg, phydata, pdata);
		} else {
			phy_reg_read_status = 1;
			NMSGPR_ALERT("%s: hw_if->read_phy_regs not defined",
		       		DEV_NAME);
		}
	}

	DBGPR_MDIO("<-- DWC_ETH_QOS_mdio_read_direct: phydata = %#x\n", *phydata);

	return phy_reg_read_status;
}

/*!
* \brief write MII PHY register, function called by the driver alone
*
* \details Writes MII registers through the API write_phy_reg where the
* related MAC registers can be configured.
*
* \param[in] pdata - pointer to driver private data structure.
* \param[in] phyaddr - the phy address to write
* \param[in] phyreg - the phy regiester id
*
* to write
* \param[out] phydata - actual data to be written into the phy registers
*
* \return void
*
* \retval  0 - successfully read data from register
* \retval -1 - error occurred
* \retval  1 - if the feature is not defined.
*/

INT DWC_ETH_QOS_mdio_write_direct(struct DWC_ETH_QOS_prv_data *pdata,
				  int phyaddr, int phyreg, int phydata)
{
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	int phy_reg_write_status;
	int dev_type = 0;

	DBGPR_MDIO("--> DWC_ETH_QOS_mdio_write_direct: phyaddr = %d, phyreg = %d, phydata = %#x\n",
	      phyaddr, phyreg, phydata);

	if (phyreg & MII_ADDR_C45) {
		dev_type = ((phyreg >> 16) & 0x1f);
		if (hw_if->write_phy_regs_c45) {
			phy_reg_write_status =
				hw_if->write_phy_regs_c45(phyaddr, dev_type, phyreg, phydata, pdata);
		} else {
			phy_reg_write_status = 1;
			NMSGPR_ALERT("%s: hw_if->write_phy_regs_c45 not defined",
				   DEV_NAME);
		}
	} else {
		if (hw_if->write_phy_regs) {
			phy_reg_write_status =
				hw_if->write_phy_regs(phyaddr, phyreg, phydata, pdata);
		} else {
			phy_reg_write_status = 1;
			NMSGPR_ALERT("%s: hw_if->write_phy_regs not defined",
				   DEV_NAME);
		}
	}

	DBGPR_MDIO("<-- DWC_ETH_QOS_mdio_write_direct\n");

	return phy_reg_write_status;
}

/*!
* \brief read MII PHY register.
*
* \details Read MII registers through the API read_phy_reg where the
* related MAC registers can be configured.
*
* \param[in] bus - points to the mii_bus structure
* \param[in] phyaddr - the phy address to write
* \param[in] phyreg - the phy register offset to write
*
* \return int
*
* \retval  - value read from given phy register
*/

static INT DWC_ETH_QOS_mdio_read(struct mii_bus *bus, int phyaddr, int phyreg)
{
	struct net_device *dev = bus->priv;
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	int dev_type = 0;
	int phydata;

	DBGPR_MDIO("--> DWC_ETH_QOS_mdio_read: phyaddr = %d, phyreg = %d\n",
	      phyaddr, phyreg);

	if (phyreg & MII_ADDR_C45) {
		dev_type = ((phyreg >> 16) & 0x1f);
		if (hw_if->read_phy_regs_c45) {
		    hw_if->read_phy_regs_c45(phyaddr, dev_type, phyreg, &phydata, pdata);
		} else {
			NMSGPR_ALERT("%s: hw_if->read_phy_regs_c45 not defined",
				   DEV_NAME);
		}
	} else {
		if (hw_if->read_phy_regs) {
			hw_if->read_phy_regs(phyaddr, phyreg, &phydata, pdata);
		} else {
			NMSGPR_ALERT("%s: hw_if->read_phy_regs not defined",
				   DEV_NAME);
		}
	}

	DBGPR_MDIO("<-- DWC_ETH_QOS_mdio_read: phydata = %#x\n", phydata);

	return phydata;
}

/*!
* \brief API to write MII PHY register
*
* \details This API is expected to write MII registers with the value being
* passed as the last argument which is done in write_phy_regs API
* called by this function.
*
* \param[in] bus - points to the mii_bus structure
* \param[in] phyaddr - the phy address to write
* \param[in] phyreg - the phy register offset to write
* \param[in] phydata - the register value to write with
*
* \return 0 on success and -ve number on failure.
*/

static INT DWC_ETH_QOS_mdio_write(struct mii_bus *bus, int phyaddr, int phyreg,
				  u16 phydata)
{
	struct net_device *dev = bus->priv;
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	int dev_type = 0;
	INT ret = Y_SUCCESS;

	DBGPR_MDIO("--> DWC_ETH_QOS_mdio_write: phyaddr = %d, phyreg = %d, phydata = %#x\n",
	      phyaddr, phyreg, phydata);

	if (phyreg & MII_ADDR_C45) {
		dev_type = ((phyreg >> 16) & 0x1f);
		if (hw_if->write_phy_regs_c45) {
			hw_if->write_phy_regs_c45(phyaddr, dev_type, phyreg, phydata, pdata);
		} else {
			ret = -1;
			NMSGPR_ALERT("%s: hw_if->write_phy_regs_c45 not defined",
				DEV_NAME);
		}
	} else {
		if (hw_if->write_phy_regs) {
			hw_if->write_phy_regs(phyaddr, phyreg, phydata, pdata);
		} else {
			ret = -1;
			NMSGPR_ALERT("%s: hw_if->write_phy_regs not defined",
		       DEV_NAME);
		}
	}

	DBGPR_MDIO("<-- DWC_ETH_QOS_mdio_write\n");

	return ret;
}




/*!
 * \details This function is invoked by other functions to get the PHY register
 * dump. This function is used during development phase for debug purpose.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return 0
 */

void dump_phy_registers(struct DWC_ETH_QOS_prv_data *pdata)
{
	int phydata = 0;
	int phyreg = 0;
	int phy_cl45 = 0;

	if (pdata->phydev == NULL)
		return;

	NMSGPR_ALERT(
	       "\n************* PHY Reg dump *************************\n");
	for (phyreg = 0; phyreg < 32 ; phyreg++) {
		DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, phyreg, &phydata);
		if (phydata == 0xFFFF) {
		phy_cl45 = 1;
		break;
		}
		NMSGPR_ALERT("Phy Register (%#x) = %#x\n", phyreg, phydata);
    }

	if (phy_cl45) {
        phyreg = 0;

        DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, (phyreg | (1 << 16) | MII_ADDR_C45), &phydata);
        NMSGPR_ALERT("Phy  Device 1(PMA/PMD) Register (%#x)(Control Register) = %#x\n", phyreg, phydata);
        DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, ((phyreg + 1) | (1 << 16) | MII_ADDR_C45), &phydata);
        NMSGPR_ALERT("Phy  Device 1(PMA/PMD) Register (%#x)(Status Register) = %#x\n", phyreg, phydata);

        phyreg = 0;
        DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, (phyreg | (3 << 16) | MII_ADDR_C45), &phydata);
        NMSGPR_ALERT("Phy  Device 3(PCS) Register (%#x)(Control Register) = %#x\n", phyreg, phydata);
        DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, ((phyreg + 1) | (3 << 16) | MII_ADDR_C45), &phydata);
        NMSGPR_ALERT("Phy  Device 3(PCS) Register (%#x)(Status Register) = %#x\n", phyreg, phydata);

        phyreg = 0;
        DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, (phyreg | (7 << 16) | MII_ADDR_C45), &phydata);
        NMSGPR_ALERT("Phy  Device 7(Auto-Negotation) Register (%#x)(Control Register) = %#x\n", phyreg, phydata);
        DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr, ((phyreg + 1) | (7 << 16) | MII_ADDR_C45), &phydata);
        NMSGPR_ALERT("Phy  Device 7(Auto-Negotation) Register (%#x)(Status Register) = %#x\n", phyreg, phydata);
	}

	NMSGPR_ALERT(
	       "\n****************************************************\n");
}

/*!
* \brief API to adjust link parameters.
*
* \details This function will be called by PAL to inform the driver
* about various link parameters like duplex and speed. This function
* will configure the MAC based on link parameters.
*
* \param[in] dev - pointer to net_device structure
*
* \return void
*/

static void DWC_ETH_QOS_adjust_link(struct net_device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	struct phy_device *phydev = pdata->phydev;
	//unsigned long flags;
	int new_state = 0;

	if (phydev == NULL)
		return;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
	DBGPR_MDIO("-->DWC_ETH_QOS_adjust_link. address %d link %d\n", phydev->addr,
	      phydev->link);
#else
	DBGPR_MDIO("-->DWC_ETH_QOS_adjust_link. link %d\n", phydev->link);
#endif

	//spin_lock_irqsave(&pdata->lock, flags);

	if (phydev->link) {
		/* Now we make sure that we can be in full duplex mode.
		 * If not, we operate in half-duplex mode */
		if (phydev->duplex != pdata->oldduplex) {
			new_state = 1;
			if (phydev->duplex) {
				hw_if->set_full_duplex(pdata);
			} else {
				hw_if->set_half_duplex(pdata);
#ifdef DWC_ETH_QOS_CERTIFICATION_PKTBURSTCNT_HALFDUPLEX
				/* For Synopsys testing and debugging only */
				{
					UINT phydata;

					/* setting 'Assert CRS on transmit' */
					phydata = 0;
					DWC_ETH_QOS_mdio_read_direct(pdata, pdata->phyaddr,
						DWC_ETH_QOS_PHY_CTL, &phydata);
					phydata |= (1 << 11);
					DWC_ETH_QOS_mdio_write_direct(pdata, pdata->phyaddr,
						DWC_ETH_QOS_PHY_CTL, phydata);
				}
#endif
			}
			pdata->oldduplex = phydev->duplex;
		}

		/* FLOW ctrl operation */
		if (phydev->pause || phydev->asym_pause) {
			if (pdata->flow_ctrl != pdata->oldflow_ctrl)
				DWC_ETH_QOS_configure_flow_ctrl(pdata);
		}

		if (phydev->speed != pdata->speed) {
			new_state = 1;
#if defined(PHY_MII)
			hw_if->set_mii_speed_100(pdata);
			hw_if->set_full_duplex(pdata);
			hw_if->tc9560_set_tx_clk_25MHz(pdata);
			pdata->speed = phydev->speed = SPEED_100;
			NMSGPR_INFO( "%s:\t Fixed MII speed-100,tx_clk-25MHz\n", __func__);
			hw_if->tc9560_set_phy_intf_MII(pdata);   /* check whether it is required */
			NMSGPR_INFO( "%s:\t Fixed MII mode.\n", __func__);
#else
#if defined(PHY_RMII)
			hw_if->set_mii_speed_100(pdata);
			hw_if->set_full_duplex(pdata);
			hw_if->tc9560_set_tx_clk_25MHz_RMII(pdata);
			pdata->speed = phydev->speed = SPEED_100;
			NMSGPR_INFO( "%s:\t Fixed RMII speed-100,tx_clk-25MHz\n", __func__);
			hw_if->tc9560_set_phy_intf_RMII(pdata); 
			NMSGPR_INFO( "%s:\t Fixed RMII mode.\n", __func__);
#else
			switch (phydev->speed) {
			case SPEED_1000:
				hw_if->set_gmii_speed(pdata);
				hw_if->tc9560_set_tx_clk_125MHz(pdata);
				break;
			case SPEED_100:
				hw_if->set_mii_speed_100(pdata);
				hw_if->tc9560_set_tx_clk_25MHz(pdata);
				break;
			case SPEED_10:
				hw_if->set_mii_speed_10(pdata);
				hw_if->tc9560_set_tx_clk_2_5MHz(pdata);
				break;
			}
			pdata->speed = phydev->speed;
#endif /* End of #if defined(PHY_RMII) */
#endif /* End of #if defined(PHY_MII) */

		}

		if (!pdata->oldlink) {
			new_state = 1;
			pdata->oldlink = 1;
		}
	} else if (pdata->oldlink) {
		new_state = 1;
		pdata->oldlink = 0;
		pdata->speed = 0;
		pdata->oldduplex = -1;
	}

	if (new_state) {
		phy_print_status(phydev);
		if (CONFIGURED == pdata->AVB_config_status)
			DWC_ETH_QOS_reload_fqtss_cfg(pdata);
	}
	/* At this stage, it could be need to setup the EEE or adjust some
	 * MAC related HW registers.
	 * */
	pdata->eee_enabled = DWC_ETH_QOS_eee_init(pdata);

	//spin_unlock_irqrestore(&pdata->lock, flags);

	DBGPR_MDIO("<--DWC_ETH_QOS_adjust_link\n");
}

/*!
* \brief API to initialize PHY.
*
* \details This function will initializes the driver's PHY state and attaches
* the PHY to the MAC driver.
*
* \param[in] dev - pointer to net_device structure
*
* \return integer
*
* \retval 0 on success & negative number on failure.
*/

static int DWC_ETH_QOS_init_phy(struct net_device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct phy_device *phydev = NULL;
	char phy_id_fmt[MII_BUS_ID_SIZE + 3];
	char bus_id[MII_BUS_ID_SIZE];

	DBGPR_MDIO("-->DWC_ETH_QOS_init_phy\n");

	pdata->oldlink = 0;
	pdata->speed = 0;
	pdata->oldduplex = -1;

	snprintf(bus_id, MII_BUS_ID_SIZE, "dwc_phy-%x", pdata->bus_id);

	snprintf(phy_id_fmt, MII_BUS_ID_SIZE + 3, PHY_ID_FMT, bus_id,
		 pdata->phyaddr);

	DBGPR_MDIO("trying to attach to %s\n", phy_id_fmt);

	phydev = phy_connect(dev, phy_id_fmt, &DWC_ETH_QOS_adjust_link,
			     pdata->interface);

	if (IS_ERR(phydev)) {
		NMSGPR_ALERT("%s: Could not attach to PHY\n", dev->name);
		return PTR_ERR(phydev);
	}

	if (phydev->phy_id == 0) {
		phy_disconnect(phydev);
		return -ENODEV;
	}


	if ((pdata->interface == PHY_INTERFACE_MODE_GMII) ||
	    (pdata->interface == PHY_INTERFACE_MODE_RGMII)) {
		//phydev->supported = PHY_GBIT_FEATURES;
#ifdef DWC_ETH_QOS_CERTIFICATION_PKTBURSTCNT_HALFDUPLEX
		phydev->supported &= ~SUPPORTED_1000baseT_Full;
#endif
	} else if ((pdata->interface == PHY_INTERFACE_MODE_MII) ||
		(pdata->interface == PHY_INTERFACE_MODE_RMII)) {
		//phydev->supported = PHY_BASIC_FEATURES;
	} else {
        NMSGPR_ALERT("%s : %d: Check me\n", __FUNCTION__, __LINE__);
	}

	phydev->supported |= (SUPPORTED_Pause | SUPPORTED_Asym_Pause);

	phydev->advertising = phydev->supported;

	DBGPR_MDIO("%s: attached to PHY (UID 0x%x) Link = %d\n", dev->name,
	      phydev->phy_id, phydev->link);

	pdata->phydev = phydev;

	DBGPR_MDIO("<--DWC_ETH_QOS_init_phy\n");

	return 0;
}

/*!
* \brief API to register mdio.
*
* \details This function will allocate mdio bus and register it
* phy layer.
*
* \param[in] dev - pointer to net_device structure
*
* \return 0 on success and -ve on failure.
*/

int DWC_ETH_QOS_mdio_register(struct net_device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);
	struct mii_bus *new_bus;
	struct phy_device *new_phy;
	int ret = Y_SUCCESS;
	int phy_reg_read_status;
	int mii_status;

	DBGPR_MDIO("-->DWC_ETH_QOS_mdio_register\n");

	pdata->bus_id = mdio_bus_id;

	new_bus = mdiobus_alloc();
	if (new_bus == NULL) {
		NMSGPR_ALERT("Unable to allocate mdio bus\n");
		return -ENOMEM;
	}

	new_bus->name = "dwc_phy";
	new_bus->read = DWC_ETH_QOS_mdio_read;
	new_bus->write = DWC_ETH_QOS_mdio_write;
	snprintf(new_bus->id, MII_BUS_ID_SIZE, "%s-%x", new_bus->name,
		 pdata->bus_id);
	new_bus->priv = dev;
	new_bus->phy_mask = 1;
	new_bus->parent = &pdata->pdev->dev;
	new_bus->reset_delay_us = 25;
	ret = mdiobus_register(new_bus);
	if (ret != 0) {
		NMSGPR_ALERT("%s: Cannot register as MDIO bus\n",
		    new_bus->name);
		new_bus->priv = NULL;
		mdiobus_free(new_bus);
		return ret;
	}
	pdata->mii = new_bus;

	/* get PHY address */
	new_phy = phy_find_first(new_bus);
	if (!new_phy) {
		NMSGPR_ALERT("%s: No phy could be detected\n", DEV_NAME);
		pdata->mii = NULL;
		mdiobus_unregister(new_bus);
		new_bus->priv = NULL;
		mdiobus_free(new_bus);
		return -ENOLINK;
	}
	pdata->phyaddr = new_phy->mdio.addr;
	NMSGPR_INFO("%s: Phy detected at"\
		" ID/ADDR %d\n", DEV_NAME, pdata->phyaddr);

	DBGPHY_REGS(pdata);

	ret = DWC_ETH_QOS_init_phy(dev);
	if (unlikely(ret)) {
		NMSGPR_ALERT("Cannot attach to PHY (error: %d)\n", ret);
		goto err_out_phy_connect;
	}

	DBGPR_MDIO("<--DWC_ETH_QOS_mdio_register\n");

	return ret;

 err_out_phy_connect:
	DWC_ETH_QOS_mdio_unregister(dev);
	return ret;
}

/*!
* \brief API to unregister mdio.
*
* \details This function will unregister mdio bus and free's the memory
* allocated to it.
*
* \param[in] dev - pointer to net_device structure
*
* \return void
*/

void DWC_ETH_QOS_mdio_unregister(struct net_device *dev)
{
	struct DWC_ETH_QOS_prv_data *pdata = netdev_priv(dev);

	DBGPR_MDIO("-->DWC_ETH_QOS_mdio_unregister\n");

	if (pdata->phydev) {
		phy_stop(pdata->phydev);
		if (pdata->phydev->attached_dev)
			phy_disconnect(pdata->phydev);
		pdata->phydev = NULL;
	}

	mdiobus_unregister(pdata->mii);
	pdata->mii->priv = NULL;
	mdiobus_free(pdata->mii);
	pdata->mii = NULL;

	DBGPR_MDIO("<--DWC_ETH_QOS_mdio_unregister\n");
}
