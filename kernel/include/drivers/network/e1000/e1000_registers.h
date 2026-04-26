#pragma once

#define E1000_REG_CTRL              0x0000
#define E1000_REG_STATUS            0x0008
#define E1000_REG_EECD              0x0010
#define E1000_REG_EERD              0x0014
#define E1000_REG_ICR               0x00C0
#define E1000_REG_IMS               0x00D0
#define E1000_REG_RCTL              0x0100
#define E1000_REG_RDBAL             0x2800
#define E1000_REG_RDBAH             0x2804
#define E1000_REG_RDLEN             0x2808
#define E1000_REG_RDH               0x2810
#define E1000_REG_RDT               0x2818
#define E1000_REG_TCTL              0x0400
#define E1000_REG_TDBAL             0x3800
#define E1000_REG_TDBAH             0x3804
#define E1000_REG_TDLEN             0x3808
#define E1000_REG_TDH               0x3810
#define E1000_REG_TDT               0x3818
#define E1000_REG_RAL(n)            0x5400 + (8 * n)
#define E1000_REG_RAH(n)            0x5404 + (8 * n)
#define E1000_REG_TIPG              0x0410

#define E1000_BIT_CTRL_FD           (1 << 0)
#define E1000_BIT_CTRL_LRST         (1 << 3)
#define E1000_BIT_CTRL_ASDE         (1 << 5)
#define E1000_BIT_CTRL_SLU          (1 << 6)
#define E1000_BIT_CTRL_ILOS         (1 << 7)
#define E1000_BIT_CTRL_FRCSPD       (1 << 11)
#define E1000_BIT_CTRL_FRCDPLX      (1 << 12)
#define E1000_BIT_CTRL_SDP0_DATA    (1 << 18)
#define E1000_BIT_CTRL_SDP1_DATA    (1 << 19)
#define E1000_BIT_CTRL_ADVD3WUC     (1 << 20)
#define E1000_BIT_CTRL_RST          (1 << 26)
#define E1000_BIT_CTRL_RFCE         (1 << 27)
#define E1000_BIT_CTRL_TFCE         (1 << 28)
#define E1000_BIT_CTRL_VME          (1 << 30)
#define E1000_BIT_CTRL_PHY_RST      (1 << 31)

#define E1000_BIT_STATUS_FD         (1 << 0)
#define E1000_BIT_STATUS_LU         (1 << 1)

#define E1000_BIT_EECD_EE_PRES      (1 << 8)

#define E1000_BIT_EERD_START        (1 << 0)
#define E1000_BIT_EERD_DONE         (1 << 4)

#define E1000_BIT_TCTL_EN           (1 << 1)
#define E1000_BIT_TCTL_PSP          (1 << 3)

#define E1000_BIT_RCTL_EN           (1 << 1)
#define E1000_BIT_RCTL_SBP          (1 << 2)
#define E1000_BIT_RCTL_UPE          (1 << 3)
#define E1000_BIT_RCTL_LPE          (1 << 5)
#define E1000_BIT_RCTL_BAM          (1 << 15)
#define E1000_BIT_RCTL_BSEX         (1 << 25)
#define E1000_BIT_RCTL_BSIZE(x)     (x << 16)

#define E1000_BIT_IM_TDW            (1 << 0)
#define E1000_BIT_IM_TXQE           (1 << 1)
#define E1000_BIT_IM_LSC            (1 << 2)
#define E1000_BIT_IM_RXSEQ          (1 << 3)
#define E1000_BIT_IM_RXDMT0         (1 << 4)
#define E1000_BIT_IM_RXO            (1 << 6)
#define E1000_BIT_IM_RXT            (1 << 7)
#define E1000_BIT_IM_MDAC           (1 << 9)
#define E1000_BIT_IM_RXCFG          (1 << 10)
#define E1000_BIT_IM_PHYINT         (1 << 12)
#define E1000_BIT_IM_TXD_LOW        (1 << 15)
#define E1000_BIT_IM_SRPD           (1 << 16)