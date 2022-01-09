// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2020 Pierre Bourdon <delroth@gmail.com> */

#include <linux/compiler.h>
#include <linux/device.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/types.h>

#include "al_eth.h"

struct al_eth_netdev_priv {
	struct al_eth_adapter *adapter;
};

static int al_eth_ndo_open(struct net_device *netdev)
{
	return 0;
}

static int al_eth_ndo_stop(struct net_device *netdev)
{
	return 0;
}

static netdev_tx_t al_eth_ndo_start_xmit(struct sk_buff *skb,
					 struct net_device *netdev)
{
	struct al_eth_netdev_priv *priv = netdev_priv(netdev);
	struct al_eth_adapter *adapter = priv->adapter;
	int ring_idx;

	ring_idx = skb_get_queue_mapping(skb);
	if (unlikely(ring_idx >= AL_ETH_NUM_QUEUES)) {
		pr_err("skb received on queue %d >= max %d\n",
		       ring_idx, AL_ETH_NUM_QUEUES);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	al_eth_tx_enqueue(adapter, adapter->tx_ring[ring_idx], skb);
	return NETDEV_TX_OK;

}

static const struct net_device_ops al_eth_netdev_ops = {
	.ndo_open = al_eth_ndo_open,
	.ndo_stop = al_eth_ndo_stop,
	.ndo_start_xmit = al_eth_ndo_start_xmit,
	.ndo_validate_addr = eth_validate_addr,
};

int al_eth_netdev_init(struct al_eth_adapter *adapter)
{
	struct al_eth_netdev_priv *priv;
	struct net_device *netdev;
	struct device *dev = &adapter->pdev->dev;
	int rc, i;

	netdev = alloc_etherdev_mq(sizeof(struct al_eth_netdev_priv),
				   AL_ETH_NUM_QUEUES);

	if (!netdev) {
		dev_err(dev, "alloc_etherdev_mq failed\n");
		return -ENOMEM;
	}

	adapter->netdev = netdev;

	SET_NETDEV_DEV(netdev, dev);
	netdev->netdev_ops = &al_eth_netdev_ops;

	priv = netdev_priv(netdev);
	priv->adapter = adapter;

	strcpy(netdev->name, "eth%d");

	netdev->addr_len = ETH_ALEN;
	al_eth_ec_get_mac_addr(adapter, netdev->dev_addr);

	for (i = 0; i < AL_ETH_NUM_QUEUES; ++i) {
		rc = al_eth_tx_ring_create(adapter, &adapter->tx_ring[i], i);
		if (rc)
			goto err;
	}

	rc = register_netdev(netdev);
	if (rc)
		goto err;

	adapter->registered = true;
	return 0;

err:
	al_eth_netdev_destroy(adapter);
	return rc;
}

void al_eth_netdev_destroy(struct al_eth_adapter *adapter)
{
	int i;

	if (adapter->registered)
		unregister_netdev(adapter->netdev);

	for (i = 0; i < AL_ETH_NUM_QUEUES; ++i) {
		if (adapter->rx_ring[i]) {
		}
		if (adapter->tx_ring[i]) {
			al_eth_tx_ring_destroy(adapter, adapter->tx_ring[i]);
		}
	}

	free_netdev(adapter->netdev);
}
