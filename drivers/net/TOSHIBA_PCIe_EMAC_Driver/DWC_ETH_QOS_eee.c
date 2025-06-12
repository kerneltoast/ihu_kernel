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

/*!@file: DWC_ETH_QOS_eee.c
 * @brief: Driver functions.
 */
#include "DWC_ETH_QOS_yheader.h"

/* Clause 22 registers to access clause 45 register set */
#define MMD_CTRL_REG		0x0D	/* MMD Access Control Register */
#define MMD_ADDR_DATA_REG	0x0E	/* MMD Access Address Data Register */

/* MMD Access Control register fields */
#define MMD_CTRL_FUNC_ADDR		0x0000	/* address */
#define MMD_CTRL_FUNC_DATA_NOINCR	0x4000	/* data, no post increment */
#define MMD_CTRL_FUNC_DATA_INCR_ON_RDWT	0x8000	/* data, post increment on
						   reads & writes */
#define MMD_CTRL_FUNC_DATA_INCR_ON_WT	0xC000	/* data, post increment on
						   writes only */
/* Clause 45 expansion register */
#define CL45_PCS_EEE_ABLE 0x14	/* EEE Capability register */
#define CL45_ADV_EEE_REG 0x3C   /* EEE advertisement */
#define CL45_AN_EEE_LPABLE_REG	0x3D	/* EEE Link Partner ability reg */
#define CL45_CLK_STOP_EN_REG 0x0 /* Clock Stop enable reg */

/* Clause 45 expansion registers fields */
#define CL45_LP_ADV_EEE_STATS_1000BASE_T 0x0004	/* LP EEE capabilities
						   status */
#define CL45_CLK_STOP_EN	0x400 /* Enable xMII Clock Stop */


void DWC_ETH_QOS_enable_eee_mode(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct DWC_ETH_QOS_tx_wrapper_descriptor *tx_desc_data = NULL;
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	int tx_idle = 0, chInx;

	DBGPR_EEE("-->DWC_ETH_QOS_enable_eee_mode\n");

	for (chInx = 0; chInx < pdata->tx_dma_ch_cnt; chInx++) {

		if (!pdata->tx_dma_ch_for_host[chInx])
			continue;

		tx_desc_data = GET_TX_WRAPPER_DESC(chInx);

		if ((tx_desc_data->dirty_tx == tx_desc_data->cur_tx) &&
			(pdata->tx_path_in_lpi_mode == false)) {
			tx_idle = 1;
		} else {
			tx_idle = 0;
			break;
		}
	}

	if (tx_idle) {
		hw_if->set_eee_mode(pdata);
		}

	DBGPR_EEE("<--DWC_ETH_QOS_enable_eee_mode\n");
}

//#ifdef  TC9560_UNSUPPORTED_FEATURE
void DWC_ETH_QOS_disable_eee_mode(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct hw_if_struct *hw_if = &(pdata->hw_if);

	DBGPR_EEE("-->DWC_ETH_QOS_disable_eee_mode\n");

	hw_if->reset_eee_mode(pdata);
	del_timer_sync(&pdata->eee_ctrl_timer);
	pdata->tx_path_in_lpi_mode = false;

	DBGPR_EEE("-->DWC_ETH_QOS_disable_eee_mode\n");
}
//#endif


/*!
* \brief API to control EEE mode.
*
* \details This function will move the MAC transmitter in LPI mode
* if there is no data transfer and MAC is not already in LPI state.
*
* \param[in] data - data hook
*
* \return void
*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0))
static void DWC_ETH_QOS_eee_ctrl_timer(struct timer_list* time)
{
	struct DWC_ETH_QOS_prv_data *pdata = from_timer(pdata, time, eee_ctrl_timer);

	DBGPR_EEE("-->DWC_ETH_QOS_eee_ctrl_timer\n");

	DWC_ETH_QOS_enable_eee_mode(pdata);

	DBGPR_EEE("<--DWC_ETH_QOS_eee_ctrl_timer\n");
}
#else
static void DWC_ETH_QOS_eee_ctrl_timer(unsigned long data)
{
	struct DWC_ETH_QOS_prv_data *pdata =
		(struct DWC_ETH_QOS_prv_data *)data;

	DBGPR_EEE("-->DWC_ETH_QOS_eee_ctrl_timer\n");

	DWC_ETH_QOS_enable_eee_mode(pdata);

	DBGPR_EEE("<--DWC_ETH_QOS_eee_ctrl_timer\n");
}
#endif

#if 0

#define MDIO_EEE_100TX		0x0002	/* EEE is supported for 100BASE-TX */
#define MDIO_EEE_1000T		0x0004	/* EEE is supported for 1000BASE-T */
#define MDIO_EEE_10GT		0x0008	/* EEE is supported for 10GBASE-T */
#define MDIO_EEE_1000KX		0x0010	/* EEE is supported for 1000BASE-KX */
#define MDIO_EEE_10GKX4		0x0020	/* EEE is supported for 10GBASE-KX4 */
#define MDIO_EEE_10GKR		0x0040	/* EEE is supported for 10GBASE KR */

 /* A small helper function that translates MMD EEE Capability (3.20) bits
 * to ethtool supported settings.
 * */
static u32 DWC_ETH_QOS_mmd_eee_cap_to_ethtool_sup_t(u16 eee_cap)
{
	u32 supported = 0;

	if (eee_cap & MDIO_EEE_100TX)
		supported |= SUPPORTED_100baseT_Full;
	if (eee_cap & MDIO_EEE_1000T)
		supported |= SUPPORTED_1000baseT_Full;
	if (eee_cap & MDIO_EEE_10GT)
		supported |= SUPPORTED_10000baseT_Full;
	if (eee_cap & MDIO_EEE_1000KX)
		supported |= SUPPORTED_1000baseKX_Full;
	if (eee_cap & MDIO_EEE_10GKX4)
		supported |= SUPPORTED_10000baseKX4_Full;
	if (eee_cap & MDIO_EEE_10GKR)
		supported |= SUPPORTED_10000baseKR_Full;

	return supported;
}

 /* A small helper function that translates the MMD EEE Advertisment (7.60)
  * and MMD EEE Link Partner Ability (7.61) bits to ethtool advertisement
  * settings.
  * */
static inline u32 DWC_ETH_QOS_mmd_eee_adv_to_ethtool_adv_t(u16 eee_adv)
{
	u32 adv = 0;

	if (eee_adv & MDIO_EEE_100TX)
		adv |= ADVERTISED_100baseT_Full;
	if (eee_adv & MDIO_EEE_1000T)
		adv |= ADVERTISED_1000baseT_Full;
	if (eee_adv & MDIO_EEE_10GT)
		adv |= ADVERTISED_10000baseT_Full;
	if (eee_adv & MDIO_EEE_1000KX)
		adv |= ADVERTISED_1000baseKX_Full;
	if (eee_adv & MDIO_EEE_10GKX4)
		adv |= ADVERTISED_10000baseKX4_Full;
	if (eee_adv & MDIO_EEE_10GKR)
		adv |= ADVERTISED_10000baseKR_Full;

	return adv;
}
#endif

/*!
* \brief API to initialize EEE mode.
*
* \details This function enables the LPI state and start the timer
* to verify whether the tx path can enter in LPI state if
* a. GMAC supports EEE mode &
* b. phy can also manage EEE.
*
* \param[in] pdata - pointer to private data structure
*
* \return bool
*
* \retval true on success & false on failure.
*/
bool DWC_ETH_QOS_eee_init(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	bool ret = false;

	DBGPR_EEE("-->DWC_ETH_QOS_eee_init\n");

	/* HW supports the EEE feature */
	if (pdata->hw_feat.eee_sel) {
#ifndef DWC_ETH_QOS_CUSTOMIZED_EEE_TEST
		if (pdata->phydev) {
			/* check if the PHY supports EEE */
			if (phy_init_eee(pdata->phydev, 1))
				goto phy_eee_failed;
		}
#endif /* DWC_ETH_QOS_CUSTOMIZED_EEE_TEST */

		if (!pdata->eee_active) {
			pdata->eee_active = 1;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0))
			timer_setup(&pdata->eee_ctrl_timer,
					DWC_ETH_QOS_eee_ctrl_timer,
					0);
			pdata->eee_ctrl_timer.function = DWC_ETH_QOS_eee_ctrl_timer;
			pdata->eee_ctrl_timer.expires =
				DWC_ETH_QOS_LPI_TIMER(DWC_ETH_QOS_DEFAULT_LPI_TIMER);
			add_timer(&pdata->eee_ctrl_timer);
#else
			init_timer(&pdata->eee_ctrl_timer);
			pdata->eee_ctrl_timer.function = DWC_ETH_QOS_eee_ctrl_timer;
			pdata->eee_ctrl_timer.data = (unsigned long)pdata;
			pdata->eee_ctrl_timer.expires =
				DWC_ETH_QOS_LPI_TIMER(DWC_ETH_QOS_DEFAULT_LPI_TIMER);
			add_timer(&pdata->eee_ctrl_timer);
#endif

			hw_if->set_eee_timer(DWC_ETH_QOS_DEFAULT_LPI_LS_TIMER,
				DWC_ETH_QOS_DEFAULT_LPI_TWT_TIMER, pdata);
			if (pdata->use_lpi_tx_automate) {
				hw_if->set_lpi_tx_automate(pdata);
			}
		} else {
			/* When EEE has been already initialized we have to modify
			 * the PLS bit in MAC_LPI_Control_Status reg according to
			 * PHY link status.
			 * */
			if (pdata->phydev) { 
				hw_if->set_eee_pls(pdata->phydev->link, pdata);
			}
		}

		DBGPR_EEE("EEE initialized\n");

		ret = true;
	}

	DBGPR_EEE("<--DWC_ETH_QOS_eee_init\n");

#ifndef DWC_ETH_QOS_CUSTOMIZED_EEE_TEST
 phy_eee_failed:
#endif

	return ret;
}

#define MAC_LPS_TLPIEN 0x00000001
#define MAC_LPS_TLPIEX 0x00000002
#define MAC_LPS_RLPIEN 0x00000004
#define MAC_LPS_RLPIEX 0x00000008

void DWC_ETH_QOS_handle_eee_interrupt(struct DWC_ETH_QOS_prv_data *pdata)
{
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	u32 lpi_status;

	DBGPR_EEE("-->DWC_ETH_QOS_handle_eee_interrupt\n");

	lpi_status = hw_if->get_lpi_status(pdata);
	DBGPR_EEE("MAC_LPI_Control_Status = %#x\n", lpi_status);

	if (lpi_status & MAC_LPS_TLPIEN) {
		pdata->tx_path_in_lpi_mode = 1;
		pdata->xstats.tx_path_in_lpi_mode_irq_n++;
		DBGPR_EEE("MAC Transmitter has entered the LPI state\n");
	}

	if (lpi_status & MAC_LPS_TLPIEX) {
		pdata->tx_path_in_lpi_mode = 0;
		pdata->xstats.tx_path_exit_lpi_mode_irq_n++;
		DBGPR_EEE("MAC Transmitter has exited the LPI state\n");
	}

	if (lpi_status & MAC_LPS_RLPIEN) {
		pdata->xstats.rx_path_in_lpi_mode_irq_n++;
		DBGPR_EEE("MAC Receiver has entered the LPI state\n");
	}

	if (lpi_status & MAC_LPS_RLPIEX) {
		pdata->xstats.rx_path_exit_lpi_mode_irq_n++;
		DBGPR_EEE("MAC Receiver has exited the LPI state\n");
	}

	DBGPR_EEE("<--DWC_ETH_QOS_handle_eee_interrupt\n");
}
