// SPDX-License-Identifier: GPL-2.0-only
/*
 * Amazon Annapurna Labs Alpine AHCI SATA driver.
 * Copyright 2020 Pierre Bourdon <delroth@gmail.com>
 *
 * Based on AHCI driver modifications in Annapurna Labs' 4.2.8 kernel fork
 * released by QNAP.
 */

#include <linux/bits.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/libata.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/pci.h>
#include <scsi/scsi_host.h>
#include "ahci.h"

#define DRV_NAME "ahci-alpine"
#define DRV_VERSION "1.0"

enum {
	AHCI_PCI_BAR			= 5,

	/* MSI-X entries 0-2 are reserved. */
	AL_INT_MSIX_RX_COMPLETION_START	= 3,

	IOFIC_BASE			= 0x2000,
	IOFIC_GROUP_B_INT_MASK_CLEAR	= 0x58,
	IOFIC_GROUP_B_INT_CTRL		= 0x68,

	/* INT_CTRL bits */
	INT_CTRL_CLEAR_ON_READ		= (1 << 0),
	INT_CTRL_AUTO_MASK		= (1 << 1),
	INT_CTRL_AUTO_CLEAR		= (1 << 2),
	INT_CTRL_SET_ON_POSEDGE		= (1 << 3),
	INT_CTRL_MOD_RES_MASK		= 0x0F000000,
	INT_CTRL_MOD_RES_SHIFT		= 24,

	/* Moderation timer resolution, ~16us. */
	MOD_RES_VAL			= 15,
};

/*
 * Unmasks interrupts for ports [from, to].
 */
static void ahci_alpine_unmask_interrupts(struct ahci_host_priv *hpriv,
					  int from, int to)
{
	void __iomem *iofic_base = hpriv->mmio + IOFIC_BASE;
	writel(~GENMASK(to, from), iofic_base + IOFIC_GROUP_B_INT_MASK_CLEAR);
}

/*
 * Sets up the interrupt controller on the I/O fabric bus master, then
 * allocates interrupts, properly skipping reserved MSI-X entries.
 */
static int ahci_alpine_init_msix(struct pci_dev *pdev, unsigned int nr_ports,
				 struct ahci_host_priv *hpriv)
{
	void __iomem *iofic_base = hpriv->mmio + IOFIC_BASE;
	struct msix_entry msix_entries[AHCI_MAX_PORTS];
	int i, rc, ctrl;

	ctrl = INT_CTRL_CLEAR_ON_READ | INT_CTRL_AUTO_MASK |
	       INT_CTRL_AUTO_CLEAR | INT_CTRL_SET_ON_POSEDGE |
	       (MOD_RES_VAL << INT_CTRL_MOD_RES_SHIFT);
	writel(ctrl, iofic_base + IOFIC_GROUP_B_INT_CTRL);

	for (i = 0; i < nr_ports; ++i) {
		msix_entries[i].entry = AL_INT_MSIX_RX_COMPLETION_START + i;
	}
	rc = pci_enable_msix_exact(pdev, msix_entries, nr_ports);
	if (rc) {
		dev_info(&pdev->dev, "failed to enable MSI-X, err %d\n", rc);
		return rc;
	}

	ahci_alpine_unmask_interrupts(hpriv, 0, nr_ports - 1);
	return 0;
}

static int ahci_alpine_get_irq_vector(struct ata_host *host, int port)
{
	return pci_irq_vector(to_pci_dev(host->dev), port);
}

static irqreturn_t ahci_alpine_irq_handler(int irq, void *dev_instance)
{
	struct ata_port *ap = dev_instance;
	struct ahci_host_priv *hpriv = ap->host->private_data;

	spin_lock(ap->lock);
	ahci_port_intr(ap);
	spin_unlock(ap->lock);

	writel(1 << ap->port_no, hpriv->mmio + HOST_IRQ_STAT);
	ahci_alpine_unmask_interrupts(hpriv, ap->port_no, ap->port_no);

	return IRQ_HANDLED;
}

static const struct ata_port_info ahci_alpine_ata_port_info = {
	AHCI_HFLAGS(AHCI_HFLAG_NO_PMP | AHCI_HFLAG_MULTI_MSI),
	.flags = AHCI_FLAG_COMMON,
	.pio_mask = ATA_PIO4,
	.udma_mask = ATA_UDMA6,
	.port_ops = &ahci_ops,
};

static struct scsi_host_template ahci_alpine_sht = {
	AHCI_SHT("ahci-alpine"),
};

static int ahci_alpine_init_one(struct pci_dev *pdev,
				const struct pci_device_id *ent)
{
	struct device *dev = &pdev->dev;
	struct ata_port_info pi = ahci_alpine_ata_port_info;
	const struct ata_port_info *ppi[] = { &pi, NULL };
	struct ahci_host_priv *hpriv;
	struct ata_host *host;
	int rc, n_ports;

	ata_print_version_once(dev, DRV_VERSION);

	/* acquire resources */
	rc = pcim_enable_device(pdev);
	if (rc)
		return rc;

	/* AHCI controllers often implement SFF compatible interface.
	 * Grab all PCI BARs just in case.
	 */
	rc = pcim_iomap_regions_request_all(pdev, 1 << AHCI_PCI_BAR, DRV_NAME);
	if (rc == -EBUSY)
		pcim_pin_device(pdev);
	if (rc)
		return rc;

	hpriv = devm_kzalloc(dev, sizeof(*hpriv), GFP_KERNEL);
	if (!hpriv)
		return -ENOMEM;
	hpriv->flags = (unsigned long)pi.private_data;

	hpriv->mmio = pcim_iomap_table(pdev)[AHCI_PCI_BAR];

	/* save initial config */
	ahci_save_initial_config(dev, hpriv);

	/* prepare host */
	if (hpriv->cap & HOST_CAP_NCQ)
		pi.flags |= ATA_FLAG_NCQ;

	ahci_set_em_messages(hpriv, &pi);

	n_ports = ahci_nr_ports(hpriv->cap);
	host = ata_host_alloc_pinfo(dev, ppi, n_ports);
	if (!host)
		return -ENOMEM;
	host->private_data = hpriv;

	rc = ahci_alpine_init_msix(pdev, n_ports, hpriv);
	if (rc)
		return rc;
	hpriv->get_irq_vector = ahci_alpine_get_irq_vector;
	hpriv->multi_irq_handler = ahci_alpine_irq_handler;

	/* initialize adapter */
	rc = dma_set_mask_and_coherent(
		dev, DMA_BIT_MASK((hpriv->cap & HOST_CAP_64) ? 64 : 32));
	if (rc) {
		dev_err(&pdev->dev, "DMA enable failed\n");
		return rc;
	}

	rc = ahci_reset_controller(host);
	if (rc)
		return rc;

	ahci_init_controller(host);
	ahci_print_info(host, "SATA");

	pci_set_master(pdev);

	return ahci_host_activate(host, &ahci_alpine_sht);
}

static const struct pci_device_id ahci_alpine_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_AMAZON_ANNAPURNA_LABS, 0x0031),
	  .class = PCI_CLASS_STORAGE_SATA_AHCI, .class_mask = 0xffffff },

	{},
};

static struct pci_driver ahci_alpine_pci_driver = {
	.name = DRV_NAME,
	.id_table = ahci_alpine_pci_tbl,
	.probe = ahci_alpine_init_one,
	.remove = ata_pci_remove_one,
};

module_pci_driver(ahci_alpine_pci_driver);

MODULE_DESCRIPTION("Amazon Annapurna Labs Alpine AHCI SATA driver");
MODULE_AUTHOR("Pierre Bourdon <delroth@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, ahci_alpine_pci_tbl);
MODULE_ALIAS("ahci:alpine");
