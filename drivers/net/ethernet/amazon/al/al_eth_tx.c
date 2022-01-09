// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2020 Pierre Bourdon <delroth@gmail.com> */

#include <linux/compiler.h>
#include <linux/gfp.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "al_eth.h"

struct al_eth_tx_dma_desc {
	u32 len_and_flags;
	u32 flags;
	u64 addr;
} __packed __aligned(16);

struct al_eth_tx_info {
	struct sk_buff *skb;
	DEFINE_DMA_UNMAP_ADDR(dma_addr);
	DEFINE_DMA_UNMAP_LEN(len);
};

int al_eth_tx_ring_create(struct al_eth_adapter *adapter,
			  struct al_eth_ring **ring_ptr, int idx)
{
	struct device *dev = &adapter->pdev->dev;
	struct al_eth_ring *ring;
	int rc;

	ring = kzalloc(sizeof(*ring), GFP_KERNEL);
	if (!ring) {
		dev_err(dev, "failed to allocate tx ring %d\n", idx);
		return -ENOMEM;
	}

	ring->udma_q_regs = adapter->udma_regs + 0x1000 + 0x1000 * idx;

	ring->tx_info = kvzalloc(sizeof(*ring->tx_info) * AL_ETH_BUFF_PER_RING,
				 GFP_KERNEL);
	if (!ring->tx_info) {
		dev_err(dev, "failed to allocate tx ring %d info\n", idx);
		rc = -ENOMEM;
		goto err_info;
	}

	ring->descs_size = AL_ETH_BUFF_PER_RING * sizeof(*ring->tx_descs);
	ring->tx_descs = dma_alloc_coherent(dev, ring->descs_size,
					    &ring->descs_dma_addr, GFP_KERNEL);
	if (!ring->tx_descs) {
		dev_err(dev, "failed to allocate tx ring %d descs\n", idx);
		rc = -ENOMEM;
		goto err_descs;
	}

	iowrite32(1 << 8, ring->udma_q_regs + 0xb0);
	iowrite32((u64)ring->tx_descs, ring->udma_q_regs + 0x28);
	iowrite32((u64)ring->tx_descs >> 32, ring->udma_q_regs + 0x2c);
	iowrite32(AL_ETH_BUFF_PER_RING, ring->udma_q_regs + 0x30);
	iowrite32(0x30000, ring->udma_q_regs + 0x20);

	*ring_ptr = ring;
	return 0;

err_descs:
	kvfree(ring->tx_info);
err_info:
	kfree(ring);
	return rc;
}

void al_eth_tx_ring_destroy(struct al_eth_adapter *adapter,
			    struct al_eth_ring *ring)
{
	struct device *dev = &adapter->pdev->dev;

	/* TODO: Wait for ring to be consumed */

	dma_free_coherent(dev, ring->descs_size, ring->tx_descs,
			  ring->descs_dma_addr);
	kvfree(ring->tx_info);
	kfree(ring);
}

void al_eth_tx_enqueue(struct al_eth_adapter *adapter,
		       struct al_eth_ring *ring, struct sk_buff *skb)
{
	struct device *dev = &adapter->pdev->dev;
	struct al_eth_tx_dma_desc *desc;
	struct al_eth_tx_info *info;
	dma_addr_t dma_addr;

	/* TODO */
	BUG_ON(skb_shinfo(skb)->nr_frags);

	dma_addr = dma_map_single(dev, skb->data, skb->len, PCI_DMA_TODEVICE);
	if (unlikely(dma_mapping_error(dev, dma_addr))) {
		dev_err(dev, "could not map tx skb (len = %d)\n", skb->len);
		dev_kfree_skb_any(skb);
		return;
	}

	info = &ring->tx_info[ring->next_info_to_use++];
	info->skb = skb;
	dma_unmap_addr_set(info, dma_addr, dma_addr);
	dma_unmap_len_set(info, len, skb->len);

	desc = &ring->tx_descs[ring->next_desc++];
	/*init_tx_desc(desc, dma_addr, skb->len);*/

	dma_wmb();

	/* Trigger DMA */
	dev_err(dev, "triggering dma for 1 pkt\n");
	iowrite32(1, ring->udma_q_regs + 0x38);

	return;
}
