/*
 *
 *  Bluetooth HCI UART driver for marvell devices
 *
 *  Copyright (C) 2016  Marvell International Ltd.
 *  Copyright (C) 2016  Intel Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <linux/tty.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include "hci_uart.h"

struct mrvl_data {
	struct sk_buff *rx_skb;
	struct sk_buff_head txq;
};

#define HCI_OP_VENDOR_SET_BAUD_RATE   0xFC09

struct mrvl_update_uart_baud_rate {
	__le32 baud_rate;
} __packed;

static int mrvl_set_baudrate(struct hci_uart *hu, unsigned int speed)
{
	struct hci_dev *hdev = hu->hdev;
	struct sk_buff *skb;
	struct mrvl_update_uart_baud_rate param;

	bt_dev_dbg(hdev, "Set Controller UART speed to %d bit/s", speed);

	param.baud_rate = cpu_to_le32(speed);

	/* This Marvell specific command changes the UART's controller baud
	 * rate.
	 */
	skb = __hci_cmd_sync(hdev, HCI_OP_VENDOR_SET_BAUD_RATE,
		sizeof(param), &param, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		int err = PTR_ERR(skb);
		bt_dev_err(hdev,
			   "Marvell: failed to write update baudrate (%d)",
			   err);
		return err;
	}

	kfree_skb(skb);

	return 0;
}

static int mrvl_open(struct hci_uart *hu)
{
	struct mrvl_data *mrvl;

	BT_DBG("hu %p", hu);

	if (!hci_uart_has_flow_control(hu))
		return -EOPNOTSUPP;

	mrvl = kzalloc(sizeof(*mrvl), GFP_KERNEL);
	if (!mrvl)
		return -ENOMEM;

	skb_queue_head_init(&mrvl->txq);

	hu->priv = mrvl;
	return 0;
}

static int mrvl_close(struct hci_uart *hu)
{
	struct mrvl_data *mrvl = hu->priv;

	BT_DBG("hu %p", hu);

	skb_queue_purge(&mrvl->txq);
	kfree_skb(mrvl->rx_skb);
	kfree(mrvl);

	hu->priv = NULL;
	return 0;
}

static int mrvl_flush(struct hci_uart *hu)
{
	struct mrvl_data *mrvl = hu->priv;

	BT_DBG("hu %p", hu);

	skb_queue_purge(&mrvl->txq);

	return 0;
}

static struct sk_buff *mrvl_dequeue(struct hci_uart *hu)
{
	struct mrvl_data *mrvl = hu->priv;
	struct sk_buff *skb;

	skb = skb_dequeue(&mrvl->txq);
	if (skb) {
		/* Prepend skb with frame type */
		memcpy(skb_push(skb, 1), &bt_cb(skb)->pkt_type, 1);
	}

	return skb;
}

static int mrvl_enqueue(struct hci_uart *hu, struct sk_buff *skb)
{
	struct mrvl_data *mrvl = hu->priv;

	skb_queue_tail(&mrvl->txq, skb);
	return 0;
}

static const struct h4_recv_pkt mrvl_recv_pkts[] = {
	{ H4_RECV_ACL,       .recv = hci_recv_frame     },
	{ H4_RECV_SCO,       .recv = hci_recv_frame     },
	{ H4_RECV_EVENT,     .recv = hci_recv_frame     },
};

static int mrvl_recv(struct hci_uart *hu, const void *data, int count)
{
	struct mrvl_data *mrvl = hu->priv;

	if (!test_bit(HCI_UART_REGISTERED, &hu->flags))
		return -EUNATCH;

	mrvl->rx_skb = h4_recv_buf(hu->hdev, mrvl->rx_skb, data, count,
				    mrvl_recv_pkts,
				    ARRAY_SIZE(mrvl_recv_pkts));
	if (IS_ERR(mrvl->rx_skb)) {
		int err = PTR_ERR(mrvl->rx_skb);
		bt_dev_err(hu->hdev, "Frame reassembly failed (%d)", err);
		mrvl->rx_skb = NULL;
		return err;
	}

	return count;
}

static int mrvl_setup(struct hci_uart *hu)
{
	return 0;
}

static const struct hci_uart_proto mrvl_proto = {
	.id		= HCI_UART_MRVL,
	.name		= "Marvell",
	.init_speed	= 115200,
	.oper_speed	= 3000000,
	.open		= mrvl_open,
	.close		= mrvl_close,
	.flush		= mrvl_flush,
	.setup		= mrvl_setup,
	.set_baudrate	= mrvl_set_baudrate,
	.recv		= mrvl_recv,
	.enqueue	= mrvl_enqueue,
	.dequeue	= mrvl_dequeue,
};

int __init mrvl_init(void)
{
	return hci_uart_register_proto(&mrvl_proto);
}

int __exit mrvl_deinit(void)
{
	return hci_uart_unregister_proto(&mrvl_proto);
}
