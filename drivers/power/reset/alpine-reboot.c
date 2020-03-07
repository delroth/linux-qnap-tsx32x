// SPDX-License-Identifier: GPL-2.0-only
/*
 * Alpine SoC reboot driver.
 *
 * Copyright (c) 2020 Hani Ayoub <hani@annapurnaLabs.com>
 * Copyright (c) 2020 Pierre Bourdon <delroth@gmail.com>
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>

#define WDTLOAD		0x000
#define LOAD_MIN	0x00000001
#define LOAD_MAX	0xFFFFFFFF
#define WDTVALUE	0x004
#define WDTCONTROL	0x008

/* control register masks */
#define	INT_ENABLE	(1 << 0)
#define	RESET_ENABLE	(1 << 1)
#define WDTLOCK		0xC00
#define	UNLOCK		0x1ACCE551
#define	LOCK		0x00000001

static void __iomem *base;

static int alpine_restart_handler(struct notifier_block *this,
				  unsigned long mode, void *cmd)
{
	writel(UNLOCK, base + WDTLOCK);
	writel(LOAD_MIN, base + WDTLOAD);
	writel(INT_ENABLE | RESET_ENABLE, base + WDTCONTROL);

	while (1)
		cpu_do_idle();

	return NOTIFY_DONE;
}

static struct notifier_block alpine_restart_nb = {
	.notifier_call = alpine_restart_handler,
	.priority = 128,
};

static int alpine_reboot_probe(struct platform_device *pdev)
{
	struct device_node *wdt_node;
	int err;

	wdt_node = of_parse_phandle(pdev->dev.of_node, "wdt-parent", 0);
	if (!wdt_node) {
		WARN(1, "failed to get wdt-parent entry");
		return -ENODEV;
	}

	base = of_iomap(wdt_node, 0);
	if (!base) {
		WARN(1, "failed to map watchdog base address");
		return -ENODEV;
	}

	err = register_restart_handler(&alpine_restart_nb);
	if (err) {
		dev_err(&pdev->dev,
		        "cannot register restart handler (err=%d)\n", err);
		iounmap(base);
	}

	return err;
}

static const struct of_device_id alpine_reboot_of_match[] = {
	{ .compatible = "al,alpine-reboot" },
	{}
};

static struct platform_driver alpine_reboot_driver = {
	.probe = alpine_reboot_probe,
	.driver = {
		.name = "alpine-reboot",
		.of_match_table = alpine_reboot_of_match,
	},
};
module_platform_driver(alpine_reboot_driver);
