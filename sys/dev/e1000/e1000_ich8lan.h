/******************************************************************************

  Copyright (c) 2001-2013, Intel Corporation 
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived from 
      this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/
/*$FreeBSD: stable/9/sys/dev/e1000/e1000_ich8lan.h 248292 2013-03-14 22:55:59Z jfv $*/

#ifndef _E1000_ICH8LAN_H_
#define _E1000_ICH8LAN_H_

#define ICH_FLASH_GFPREG		0x0000
#define ICH_FLASH_HSFSTS		0x0004
#define ICH_FLASH_HSFCTL		0x0006
#define ICH_FLASH_FADDR			0x0008
#define ICH_FLASH_FDATA0		0x0010

/* Requires up to 10 seconds when MNG might be accessing part. */
#define ICH_FLASH_READ_COMMAND_TIMEOUT	10000000
#define ICH_FLASH_WRITE_COMMAND_TIMEOUT	10000000
#define ICH_FLASH_ERASE_COMMAND_TIMEOUT	10000000
#define ICH_FLASH_LINEAR_ADDR_MASK	0x00FFFFFF
#define ICH_FLASH_CYCLE_REPEAT_COUNT	10

#define ICH_CYCLE_READ			0
#define ICH_CYCLE_WRITE			2
#define ICH_CYCLE_ERASE			3

#define FLASH_GFPREG_BASE_MASK		0x1FFF
#define FLASH_SECTOR_ADDR_SHIFT		12

#define ICH_FLASH_SEG_SIZE_256		256
#define ICH_FLASH_SEG_SIZE_4K		4096
#define ICH_FLASH_SEG_SIZE_8K		8192
#define ICH_FLASH_SEG_SIZE_64K		65536

#define E1000_ICH_FWSM_RSPCIPHY		0x00000040 /* Reset PHY on PCI Reset */
/* FW established a valid mode */
#define E1000_ICH_FWSM_FW_VALID		0x00008000
#define E1000_ICH_FWSM_PCIM2PCI		0x01000000 /* ME PCIm-to-PCI active */
#define E1000_ICH_FWSM_PCIM2PCI_COUNT	2000

#define E1000_ICH_MNG_IAMT_MODE		0x2

#define E1000_FWSM_WLOCK_MAC_MASK	0x0380
#define E1000_FWSM_WLOCK_MAC_SHIFT	7

/* Shared Receive Address Registers */
#define E1000_SHRAL_PCH_LPT(_i)		(0x05408 + ((_i) * 8))
#define E1000_SHRAH_PCH_LPT(_i)		(0x0540C + ((_i) * 8))

#define ID_LED_DEFAULT_ICH8LAN	((ID_LED_DEF1_DEF2 << 12) | \
				 (ID_LED_OFF1_OFF2 <<  8) | \
				 (ID_LED_OFF1_ON2  <<  4) | \
				 (ID_LED_DEF1_DEF2))

#define E1000_ICH_NVM_SIG_WORD		0x13
#define E1000_ICH_NVM_SIG_MASK		0xC000
#define E1000_ICH_NVM_VALID_SIG_MASK	0xC0
#define E1000_ICH_NVM_SIG_VALUE		0x80

#define E1000_ICH8_LAN_INIT_TIMEOUT	1500

#define E1000_FEXTNVM_SW_CONFIG		1
#define E1000_FEXTNVM_SW_CONFIG_ICH8M	(1 << 27) /* Bit redefined for ICH8M */

#define E1000_FEXTNVM3_PHY_CFG_COUNTER_MASK	0x0C000000
#define E1000_FEXTNVM3_PHY_CFG_COUNTER_50MSEC	0x08000000

#define E1000_FEXTNVM4_BEACON_DURATION_MASK	0x7
#define E1000_FEXTNVM4_BEACON_DURATION_8USEC	0x7
#define E1000_FEXTNVM4_BEACON_DURATION_16USEC	0x3

#define E1000_FEXTNVM6_REQ_PLL_CLK	0x00000100

#define PCIE_ICH8_SNOOP_ALL	PCIE_NO_SNOOP_ALL

#define E1000_ICH_RAR_ENTRIES	7
#define E1000_PCH2_RAR_ENTRIES	5 /* RAR[0], SHRA[0-3] */
#define E1000_PCH_LPT_RAR_ENTRIES	12 /* RAR[0], SHRA[0-10] */

#define PHY_PAGE_SHIFT		5
#define PHY_REG(page, reg)	(((page) << PHY_PAGE_SHIFT) | \
				 ((reg) & MAX_PHY_REG_ADDRESS))
#define IGP3_KMRN_DIAG		PHY_REG(770, 19) /* KMRN Diagnostic */
#define IGP3_VR_CTRL		PHY_REG(776, 18) /* Voltage Regulator Control */

#define IGP3_KMRN_DIAG_PCS_LOCK_LOSS		0x0002
#define IGP3_VR_CTRL_DEV_POWERDOWN_MODE_MASK	0x0300
#define IGP3_VR_CTRL_MODE_SHUTDOWN		0x0200

/* PHY Wakeup Registers and defines */
#define BM_PORT_GEN_CFG		PHY_REG(BM_PORT_CTRL_PAGE, 17)
#define BM_RCTL			PHY_REG(BM_WUC_PAGE, 0)
#define BM_WUC			PHY_REG(BM_WUC_PAGE, 1)
#define BM_WUFC			PHY_REG(BM_WUC_PAGE, 2)
#define BM_WUS			PHY_REG(BM_WUC_PAGE, 3)
#define BM_RAR_L(_i)		(BM_PHY_REG(BM_WUC_PAGE, 16 + ((_i) << 2)))
#define BM_RAR_M(_i)		(BM_PHY_REG(BM_WUC_PAGE, 17 + ((_i) << 2)))
#define BM_RAR_H(_i)		(BM_PHY_REG(BM_WUC_PAGE, 18 + ((_i) << 2)))
#define BM_RAR_CTRL(_i)		(BM_PHY_REG(BM_WUC_PAGE, 19 + ((_i) << 2)))
#define BM_MTA(_i)		(BM_PHY_REG(BM_WUC_PAGE, 128 + ((_i) << 1)))

#define BM_RCTL_UPE		0x0001 /* Unicast Promiscuous Mode */
#define BM_RCTL_MPE		0x0002 /* Multicast Promiscuous Mode */
#define BM_RCTL_MO_SHIFT	3      /* Multicast Offset Shift */
#define BM_RCTL_MO_MASK		(3 << 3) /* Multicast Offset Mask */
#define BM_RCTL_BAM		0x0020 /* Broadcast Accept Mode */
#define BM_RCTL_PMCF		0x0040 /* Pass MAC Control Frames */
#define BM_RCTL_RFCE		0x0080 /* Rx Flow Control Enable */

#define HV_LED_CONFIG		PHY_REG(768, 30) /* LED Configuration */
#define HV_MUX_DATA_CTRL	PHY_REG(776, 16)
#define HV_MUX_DATA_CTRL_GEN_TO_MAC	0x0400
#define HV_MUX_DATA_CTRL_FORCE_SPEED	0x0004
#define HV_STATS_PAGE	778
#define HV_SCC_UPPER	PHY_REG(HV_STATS_PAGE, 16) /* Single Collision Count */
#define HV_SCC_LOWER	PHY_REG(HV_STATS_PAGE, 17)
#define HV_ECOL_UPPER	PHY_REG(HV_STATS_PAGE, 18) /* Excessive Coll. Count */
#define HV_ECOL_LOWER	PHY_REG(HV_STATS_PAGE, 19)
#define HV_MCC_UPPER	PHY_REG(HV_STATS_PAGE, 20) /* Multiple Coll. Count */
#define HV_MCC_LOWER	PHY_REG(HV_STATS_PAGE, 21)
#define HV_LATECOL_UPPER PHY_REG(HV_STATS_PAGE, 23) /* Late Collision Count */
#define HV_LATECOL_LOWER PHY_REG(HV_STATS_PAGE, 24)
#define HV_COLC_UPPER	PHY_REG(HV_STATS_PAGE, 25) /* Collision Count */
#define HV_COLC_LOWER	PHY_REG(HV_STATS_PAGE, 26)
#define HV_DC_UPPER	PHY_REG(HV_STATS_PAGE, 27) /* Defer Count */
#define HV_DC_LOWER	PHY_REG(HV_STATS_PAGE, 28)
#define HV_TNCRS_UPPER	PHY_REG(HV_STATS_PAGE, 29) /* Transmit with no CRS */
#define HV_TNCRS_LOWER	PHY_REG(HV_STATS_PAGE, 30)

#define E1000_FCRTV_PCH	0x05F40 /* PCH Flow Control Refresh Timer Value */

#define E1000_NVM_K1_CONFIG	0x1B /* NVM K1 Config Word */
#define E1000_NVM_K1_ENABLE	0x1  /* NVM Enable K1 bit */

/* SMBus Control Phy Register */
#define CV_SMB_CTRL		PHY_REG(769, 23)
#define CV_SMB_CTRL_FORCE_SMBUS	0x0001

/* SMBus Address Phy Register */
#define HV_SMB_ADDR		PHY_REG(768, 26)
#define HV_SMB_ADDR_MASK	0x007F
#define HV_SMB_ADDR_PEC_EN	0x0200
#define HV_SMB_ADDR_VALID	0x0080
#define HV_SMB_ADDR_FREQ_MASK		0x1100
#define HV_SMB_ADDR_FREQ_LOW_SHIFT	8
#define HV_SMB_ADDR_FREQ_HIGH_SHIFT	12

/* Strapping Option Register - RO */
#define E1000_STRAP			0x0000C
#define E1000_STRAP_SMBUS_ADDRESS_MASK	0x00FE0000
#define E1000_STRAP_SMBUS_ADDRESS_SHIFT	17
#define E1000_STRAP_SMT_FREQ_MASK	0x00003000
#define E1000_STRAP_SMT_FREQ_SHIFT	12

/* OEM Bits Phy Register */
#define HV_OEM_BITS		PHY_REG(768, 25)
#define HV_OEM_BITS_LPLU	0x0004 /* Low Power Link Up */
#define HV_OEM_BITS_GBE_DIS	0x0040 /* Gigabit Disable */
#define HV_OEM_BITS_RESTART_AN	0x0400 /* Restart Auto-negotiation */

/* KMRN Mode Control */
#define HV_KMRN_MODE_CTRL	PHY_REG(769, 16)
#define HV_KMRN_MDIO_SLOW	0x0400

/* KMRN FIFO Control and Status */
#define HV_KMRN_FIFO_CTRLSTA			PHY_REG(770, 16)
#define HV_KMRN_FIFO_CTRLSTA_PREAMBLE_MASK	0x7000
#define HV_KMRN_FIFO_CTRLSTA_PREAMBLE_SHIFT	12

/* PHY Power Management Control */
#define HV_PM_CTRL		PHY_REG(770, 17)
#define HV_PM_CTRL_PLL_STOP_IN_K1_GIGA	0x100

#define SW_FLAG_TIMEOUT		1000 /* SW Semaphore flag timeout in ms */

/* PHY Low Power Idle Control */
#define I82579_LPI_CTRL				PHY_REG(772, 20)
#define I82579_LPI_CTRL_100_ENABLE		0x2000
#define I82579_LPI_CTRL_1000_ENABLE		0x4000
#define I82579_LPI_CTRL_ENABLE_MASK		0x6000
#define I82579_LPI_CTRL_FORCE_PLL_LOCK_COUNT	0x80

/* Extended Management Interface (EMI) Registers */
#define I82579_EMI_ADDR		0x10
#define I82579_EMI_DATA		0x11
#define I82579_LPI_UPDATE_TIMER	0x4805 /* in 40ns units + 40 ns base value */
#define I82579_MSE_THRESHOLD	0x084F /* 82579 Mean Square Error Threshold */
#define I82577_MSE_THRESHOLD	0x0887 /* 82577 Mean Square Error Threshold */
#define I82579_MSE_LINK_DOWN	0x2411 /* MSE count before dropping link */
#define I82579_RX_CONFIG		0x3412 /* Receive configuration */
#define I82579_EEE_PCS_STATUS		0x182D	/* IEEE MMD Register 3.1 >> 8 */
#define I82579_EEE_CAPABILITY		0x0410 /* IEEE MMD Register 3.20 */
#define I82579_EEE_ADVERTISEMENT	0x040E /* IEEE MMD Register 7.60 */
#define I82579_EEE_LP_ABILITY		0x040F /* IEEE MMD Register 7.61 */
#define I82579_EEE_100_SUPPORTED	(1 << 1) /* 100BaseTx EEE supported */
#define I82579_EEE_1000_SUPPORTED	(1 << 2) /* 1000BaseTx EEE supported */
#define I217_EEE_PCS_STATUS	0x9401   /* IEEE MMD Register 3.1 */
#define I217_EEE_CAPABILITY	0x8000   /* IEEE MMD Register 3.20 */
#define I217_EEE_ADVERTISEMENT	0x8001   /* IEEE MMD Register 7.60 */
#define I217_EEE_LP_ABILITY	0x8002   /* IEEE MMD Register 7.61 */

#define E1000_EEE_RX_LPI_RCVD	0x0400	/* Tx LP idle received */
#define E1000_EEE_TX_LPI_RCVD	0x0800	/* Rx LP idle received */

/* Intel Rapid Start Technology Support */
#define I217_PROXY_CTRL		BM_PHY_REG(BM_WUC_PAGE, 70)
#define I217_PROXY_CTRL_AUTO_DISABLE	0x0080
#define I217_SxCTRL			PHY_REG(BM_PORT_CTRL_PAGE, 28)
#define I217_SxCTRL_ENABLE_LPI_RESET	0x1000
#define I217_CGFREG			PHY_REG(772, 29)
#define I217_CGFREG_ENABLE_MTA_RESET	0x0002
#define I217_MEMPWR			PHY_REG(772, 26)
#define I217_MEMPWR_DISABLE_SMB_RELEASE	0x0010

/* Receive Address Initial CRC Calculation */
#define E1000_PCH_RAICC(_n)	(0x05F50 + ((_n) * 4))

/* Latency Tolerance Reporting */
#define E1000_LTRV			0x000F8
#define E1000_LTRV_VALUE_MASK		0x000003FF
#define E1000_LTRV_SCALE_MAX		5
#define E1000_LTRV_SCALE_FACTOR		5
#define E1000_LTRV_SCALE_SHIFT		10
#define E1000_LTRV_SCALE_MASK		0x00001C00
#define E1000_LTRV_REQ_SHIFT		15
#define E1000_LTRV_NOSNOOP_SHIFT	16
#define E1000_LTRV_SEND			(1 << 30)

/* Proprietary Latency Tolerance Reporting PCI Capability */
#define E1000_PCI_LTR_CAP_LPT		0xA8

/* OBFF Control & Threshold Defines */
#define E1000_SVCR_OFF_EN		0x00000001
#define E1000_SVCR_OFF_MASKINT		0x00001000
#define E1000_SVCR_OFF_TIMER_MASK	0xFFFF0000
#define E1000_SVCR_OFF_TIMER_SHIFT	16
#define E1000_SVT_OFF_HWM_MASK		0x0000001F

void e1000_set_kmrn_lock_loss_workaround_ich8lan(struct e1000_hw *hw,
						 bool state);
void e1000_igp3_phy_powerdown_workaround_ich8lan(struct e1000_hw *hw);
void e1000_gig_downshift_workaround_ich8lan(struct e1000_hw *hw);
void e1000_suspend_workarounds_ich8lan(struct e1000_hw *hw);
void e1000_resume_workarounds_pchlan(struct e1000_hw *hw);
s32 e1000_configure_k1_ich8lan(struct e1000_hw *hw, bool k1_enable);
void e1000_copy_rx_addrs_to_phy_ich8lan(struct e1000_hw *hw);
s32 e1000_lv_jumbo_workaround_ich8lan(struct e1000_hw *hw, bool enable);
s32 e1000_read_emi_reg_locked(struct e1000_hw *hw, u16 addr, u16 *data);
#endif /* _E1000_ICH8LAN_H_ */
