// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2020 Pierre Bourdon <delroth@gmail.com> */

#include <linux/device.h>
#include <linux/pci.h>
#include <linux/phy.h>

#include "al_eth.h"

static int al_eth_mdio_read(struct mii_bus *mdio, int mii_id, int reg)
{
	struct al_eth_adapter *adapter = mdio->priv;
	u16 val;

	val = ioread32(&adapter->mac_regs + 0x200 + 4 * reg);
	dev_info(&adapter->pdev->dev, "mdio_read: id=%d reg=%d val=%hd\n", mii_id, reg, val);
	return val;
}

static int al_eth_mdio_write(struct mii_bus *mdio, int mii_id, int reg,
			     u16 value)
{
	struct al_eth_adapter *adapter = mdio->priv;
	dev_info(&adapter->pdev->dev, "mdio_write: id=%d reg=%d val=%hd\n", mii_id, reg, value);
	return 0;
}

int al_eth_mdio_init(struct al_eth_adapter *adapter)
{
	struct device *dev = &adapter->pdev->dev;
	int phy_mdio_addr = adapter->board_info.phy_mdio_addr;
	int i, rc;

	dev_err(dev, "mdio freq: %d phy type: %d\n", adapter->board_info.mdio_freq, adapter->board_info.phy_if_type);

	adapter->mdio_bus = mdiobus_alloc();
	if (!adapter->mdio_bus)
		return -ENOMEM;

	adapter->mdio_bus->name = "al_eth_mdio";
	snprintf(adapter->mdio_bus->id, MII_BUS_ID_SIZE, "al_eth-%s",
		 pci_name(adapter->pdev));
	adapter->mdio_bus->parent = dev;
	adapter->mdio_bus->priv = adapter;

	adapter->mdio_bus->read = al_eth_mdio_read;
	adapter->mdio_bus->write = al_eth_mdio_write;

	adapter->mdio_bus->phy_mask = ~(1 << phy_mdio_addr);
	for (i = 0; i < PHY_MAX_ADDR; ++i)
		adapter->mdio_bus->irq[i] = PHY_POLL;

	rc = mdiobus_register(adapter->mdio_bus);
	if (rc) {
		dev_err(dev, "could not register MDIO bus: rc=%d\n", rc);
		goto err_register;
	}

	adapter->phydev = phy_find_first(adapter->mdio_bus);
	if (!adapter->phydev) {
		dev_err(dev, "could not find phy at addr %d\n", phy_mdio_addr);
		rc = -ENOENT;
		goto err_no_phy;
	}

	return 0;

err_no_phy:
	mdiobus_unregister(adapter->mdio_bus);
err_register:
	mdiobus_free(adapter->mdio_bus);
	return rc;
}

void al_eth_mdio_destroy(struct al_eth_adapter *adapter)
{
	mdiobus_unregister(adapter->mdio_bus);
	mdiobus_free(adapter->mdio_bus);
}
