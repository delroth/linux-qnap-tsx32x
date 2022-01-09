// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2020 Pierre Bourdon <delroth@gmail.com> */

#ifndef _AL_ETH_H_
#define _AL_ETH_H_

#include <linux/cache.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/phy.h>
#include <linux/skbuff.h>
#include <linux/types.h>

/* TODO: Remove */
#include "al_eth_ec_regs.h"
#include "al_eth_mac_regs.h"

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define AL_ETH_NUM_QUEUES	1
#define AL_ETH_BUFF_PER_RING	1024

#define AL_ETH_UDMA_BAR		0
#define AL_ETH_MAC_BAR		2
#define AL_ETH_EC_BAR		4

#define AL_ETH_BARS_MASK	((1 << AL_ETH_UDMA_BAR) | \
				 (1 << AL_ETH_MAC_BAR) | \
				 (1 << AL_ETH_EC_BAR))

struct al_eth_board_info {
	bool has_external_phy;
	u8 phy_mdio_addr;
	u8 phy_if_type;
	u8 mdio_freq;
};

struct al_eth_ring {
	struct netdev_queue *tx_queue;
	void __iomem *udma_q_regs;

	u16 next_desc;
	u16 next_info_to_use;
	u16 next_info_to_clean;

	union {
		struct al_eth_rx_dma_desc *rx_descs;
		struct al_eth_tx_dma_desc *tx_descs;
	};
	size_t descs_size;
	dma_addr_t descs_dma_addr;

	union {
		struct al_eth_rx_info *rx_info;
		struct al_eth_tx_info *tx_info;
	};
} ____cacheline_aligned_in_smp;

struct al_eth_adapter {
	struct pci_dev *pdev;
	struct net_device *netdev;
	struct mii_bus *mdio_bus;
	struct phy_device *phydev;

	bool registered;

	void __iomem *udma_regs;
	void __iomem *mac_regs;
	struct al_ec_regs __iomem *ec_regs;

	struct al_eth_board_info board_info;

	struct al_eth_ring *rx_ring[AL_ETH_NUM_QUEUES];
	struct al_eth_ring *tx_ring[AL_ETH_NUM_QUEUES];
};

/* al_eth_ec.c */
void al_eth_ec_get_mac_addr(struct al_eth_adapter *adapter, u8* addr);
void al_eth_ec_set_mac_addr(struct al_eth_adapter *adapter, const u8* addr);

/* al_eth_mac.c */
void al_eth_mac_read_board_info(struct al_eth_adapter *adapter,
				struct al_eth_board_info *info);

/* al_eth_mdio.c */
int al_eth_mdio_init(struct al_eth_adapter *adapter);
void al_eth_mdio_destroy(struct al_eth_adapter *adapter);

/* al_eth_netdev.c */
int al_eth_netdev_init(struct al_eth_adapter *adapter);
void al_eth_netdev_destroy(struct al_eth_adapter *adapter);

/* al_eth_tx.c */
int al_eth_tx_ring_create(struct al_eth_adapter *adapter,
			  struct al_eth_ring **ring_ptr, int idx);
void al_eth_tx_ring_destroy(struct al_eth_adapter *adapter,
			    struct al_eth_ring *ring);
void al_eth_tx_enqueue(struct al_eth_adapter *adapter,
		       struct al_eth_ring *ring, struct sk_buff *skb);

#endif /* _IXGBE_H_ */
