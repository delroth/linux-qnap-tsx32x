// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2020 Pierre Bourdon <delroth@gmail.com> */

#include "al_eth.h"

enum {
	REG_MAC_BOARD_INFO_0	= 0x004,
	REG_MAC_BOARD_INFO_1	= 0x404,
	REG_MAC_BOARD_INFO_2	= 0x00c,
};

void al_eth_mac_read_board_info(struct al_eth_adapter *adapter,
				struct al_eth_board_info *info)
{
	u32 reg_0, reg_1, reg_2;

	reg_0 = ioread32(adapter->mac_regs + REG_MAC_BOARD_INFO_0);
	reg_1 = ioread32(adapter->mac_regs + REG_MAC_BOARD_INFO_1);
	reg_2 = ioread32(adapter->mac_regs + REG_MAC_BOARD_INFO_2);

	info->has_external_phy = !!(reg_0 & 0x10);
	info->phy_mdio_addr = (reg_0 >> 5) & 0x1f;
	info->phy_if_type = (reg_0 >> 20) & 0x3;
	info->mdio_freq = (reg_0 >> 14) & 0x3;
}
