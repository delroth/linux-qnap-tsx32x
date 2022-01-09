// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright(c) 2020 Pierre Bourdon <delroth@gmail.com>
 *
 * Inspired by the Corundum NIC out of tree driver structure.
 * Copyright(c) 2019 The Regents of the University of California
 *
 * Based on Annapurna Labs' Alpine NIC out of tree driver.
 * Copyright(c) 2012 Annapurna Labs
 */

#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>

#include "al_eth.h"

#define DRV_VERSION "1.0"

static int al_eth_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct al_eth_adapter *adapter;
	struct device *dev = &pdev->dev;
	void __iomem * const *iomap_table;
	int rc;

	if (!(adapter = devm_kzalloc(dev, sizeof(*adapter), GFP_KERNEL)))
		return -ENOMEM;

	adapter->pdev = pdev;

	rc = pcim_enable_device(pdev);
	if (rc)
		return rc;

	rc = pcim_iomap_regions(pdev, AL_ETH_BARS_MASK, KBUILD_MODNAME);
	if (rc) {
		dev_err(dev, "pcim_iomap_regions failed: err=%d\n", rc);
		goto err_iomap_regions;
	}

	iomap_table = pcim_iomap_table(pdev);
	if (!iomap_table) {
		dev_err(dev, "pcim_iomap_table failed\n");
		rc = -ENOMEM;
		goto err_iomap_table;
	}

	adapter->udma_regs = iomap_table[AL_ETH_UDMA_BAR];
	adapter->mac_regs = iomap_table[AL_ETH_MAC_BAR];
	adapter->ec_regs = iomap_table[AL_ETH_EC_BAR];

	pci_set_master(pdev);

	al_eth_mac_read_board_info(adapter, &adapter->board_info);
	if (adapter->board_info.has_external_phy) {
		rc = al_eth_mdio_init(adapter);
		if (rc)
			goto err_phy;
	} else {
		dev_err(dev, "no external PHY, probably SFP+. Unsupported.\n");
		rc = -EINVAL;
		goto err_phy;
	}

	rc = al_eth_netdev_init(adapter);
	if (rc)
		goto err_init_netdev;

	iowrite32(0x9, adapter->mac_regs + 0x8);

	pci_set_drvdata(pdev, adapter);
	return 0;

err_init_netdev:
	al_eth_mdio_destroy(adapter);
err_phy:
err_iomap_table:
	pcim_iounmap_regions(pdev, AL_ETH_BARS_MASK);
err_iomap_regions:
	pci_disable_device(pdev);
	return rc;
}

static void al_eth_remove(struct pci_dev *pdev)
{
	struct al_eth_adapter *adapter = pci_get_drvdata(pdev);

	/* drvdata is set only after successful probing */
	if (!adapter)
		return;

	al_eth_mdio_destroy(adapter);
	al_eth_netdev_destroy(adapter);
	pci_disable_device(pdev);
}

static const struct pci_device_id al_eth_pci_tbl[] = {
	/* Standard integrated NIC */
	{ PCI_DEVICE(PCI_VENDOR_ID_AMAZON_ANNAPURNA_LABS, 0x0001) },
	/* Advanced integrated NIC */
	{ PCI_DEVICE(PCI_VENDOR_ID_AMAZON_ANNAPURNA_LABS, 0x0002) },
	{},
};

static struct pci_driver al_eth_pci_driver = {
	.name = KBUILD_MODNAME,
	.id_table = al_eth_pci_tbl,
	.probe = al_eth_probe,
	.remove = al_eth_remove,
};

module_pci_driver(al_eth_pci_driver);

MODULE_AUTHOR("Pierre Bourdon <delroth@gmail.com>");
MODULE_DESCRIPTION("Amazon Annapurna Labs Alpine SoC Internal NIC Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRV_VERSION);
MODULE_DEVICE_TABLE(pci, al_eth_pci_tbl);
