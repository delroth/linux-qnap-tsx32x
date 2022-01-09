// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2020 Pierre Bourdon <delroth@gmail.com> */

#include <asm/io.h>
#include <linux/types.h>

#include "al_eth.h"

void al_eth_ec_get_mac_addr(struct al_eth_adapter *adapter, u8* addr)
{
	u32 mac_hi = ioread32(&adapter->ec_regs->fwd_mac[0].data_h);
	u32 mac_lo = ioread32(&adapter->ec_regs->fwd_mac[0].data_l);

	addr[0] = mac_hi >> 8;
	addr[1] = mac_hi;
	addr[2] = mac_lo >> 24;
	addr[3] = mac_lo >> 16;
	addr[4] = mac_lo >> 8;
	addr[5] = mac_lo;
}

void al_eth_ec_set_mac_addr(struct al_eth_adapter *adapter, const u8* addr)
{
	u32 mac_hi, mac_lo;

	mac_hi = (addr[0] << 8) | addr[1];
	mac_lo = (addr[2] << 24) | (addr[3] << 16) | (addr[4] << 8) | addr[5];

	iowrite32(mac_hi, &adapter->ec_regs->fwd_mac[0].data_h);
	iowrite32(mac_lo, &adapter->ec_regs->fwd_mac[0].data_l);
}
