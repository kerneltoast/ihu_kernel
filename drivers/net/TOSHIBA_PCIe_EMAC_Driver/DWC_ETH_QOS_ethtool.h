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

#ifndef __DWC_ETH_QOS_ETHTOOL_H__

#define __DWC_ETH_QOS_ETHTOOL_H__

static void DWC_ETH_QOS_get_pauseparam(struct net_device *dev,
				       struct ethtool_pauseparam *pause);
static int DWC_ETH_QOS_set_pauseparam(struct net_device *dev,
				      struct ethtool_pauseparam *pause);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0))
static int DWC_ETH_QOS_getsettings(struct net_device *dev,
				   struct ethtool_cmd *cmd);
static int DWC_ETH_QOS_setsettings(struct net_device *dev,
				   struct ethtool_cmd *cmd);
#else
static int DWC_ETH_QOS_get_link_ksettings(struct net_device *dev, struct  ethtool_link_ksettings *cmd);
static int DWC_ETH_QOS_set_link_ksettings(struct net_device *dev, const struct  ethtool_link_ksettings *cmd);
#endif

static void DWC_ETH_QOS_get_wol(struct net_device *dev,
				struct ethtool_wolinfo *wol);
static int DWC_ETH_QOS_set_wol(struct net_device *dev,
			       struct ethtool_wolinfo *wol);

static int DWC_ETH_QOS_set_coalesce(struct net_device *dev,
				    struct ethtool_coalesce *ec);
static int DWC_ETH_QOS_get_coalesce(struct net_device *dev,
				    struct ethtool_coalesce *ec);

static int DWC_ETH_QOS_get_sset_count(struct net_device *dev, int sset);

static void DWC_ETH_QOS_get_strings(struct net_device *dev, u32 stringset, u8 *data);

static void DWC_ETH_QOS_get_ethtool_stats(struct net_device *dev,
	struct ethtool_stats *dummy, u64 *data);

static int DWC_ETH_QOS_get_ts_info(struct net_device *dev, struct ethtool_ts_info *info);
static void DWC_ETH_QOS_get_drivinfo(struct net_device *dev, struct ethtool_drvinfo *drv_info);

#endif
