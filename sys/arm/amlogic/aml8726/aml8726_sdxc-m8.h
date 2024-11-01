/*-
 * Copyright 2015 John Wehle <john@feith.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef	_ARM_AMLOGIC_AML8726_SDXC_M8_H
#define	_ARM_AMLOGIC_AML8726_SDXC_M8_H

#define	AML_SDXC_ALIGN_DMA				4
#define	AML_SDXC_MAX_DMA				4096

/*
 * Timeouts are in milliseconds
 *
 * Read and write are per section 4.6.2 of the:
 *
 *   SD Specifications Part 1
 *   Physical Layer Simplified Specification
 *   Version 4.10
 */
#define	AML_SDXC_CMD_TIMEOUT				50
#define	AML_SDXC_READ_TIMEOUT				100
#define	AML_SDXC_WRITE_TIMEOUT				500
#define	AML_SDXC_MAX_TIMEOUT				5000

#define	AML_SDXC_BUSY_POLL_INTVL			1
#define	AML_SDXC_BUSY_TIMEOUT				1000

/*
 * There's some disagreements between the S805 documentation
 * and the Amlogic Linux platform code regarding the exact
 * layout of various registers ... when in doubt we follow
 * the platform code.
 */

#define	AML_SDXC_CMD_ARGUMENT_REG			0

#define	AML_SDXC_SEND_REG				4
#define	AML_SDXC_SEND_REP_PKG_CNT_MASK			(0xffffU << 16)
#define	AML_SDXC_SEND_REP_PKG_CNT_SHIFT			16
#define	AML_SDXC_SEND_DATA_STOP				(1 << 11)
#define	AML_SDXC_SEND_DATA_WRITE			(1 << 10)
#define	AML_SDXC_SEND_RESP_NO_CRC7_CHECK		(1 << 9)
#define	AML_SDXC_SEND_RESP_136				(1 << 8)
#define	AML_SDXC_SEND_CMD_HAS_DATA			(1 << 7)
#define	AML_SDXC_SEND_CMD_HAS_RESP			(1 << 6)
#define	AML_SDXC_SEND_INDEX_MASK			0x3f
#define	AML_SDXC_SEND_INDEX_SHIFT			0

#define	AML_SDXC_CNTRL_REG				8
#define	AML_SDXC_CNTRL_TX_ENDIAN_MASK			(7 << 29)
#define	AML_SDXC_CNTRL_TX_ENDIAN_SHIFT			29
#define	AML_SDXC_CNTRL_RX_ENDIAN_MASK			(7 << 24)
#define	AML_SDXC_CNTRL_RX_ENDIAN_SHIFT			24
#define	AML_SDXC_CNTRL_RX_PERIOD_SHIFT			20
#define	AML_SDXC_CNTRL_RX_TIMEOUT_SHIFT			13
#define	AML_SDXC_CNTRL_PKG_LEN_MASK			(0x1ff << 4)
#define	AML_SDXC_CNTRL_PKG_LEN_SHIFT			4
#define	AML_SDXC_CNTRL_BUS_WIDTH_MASK			(3 << 0)
#define	AML_SDXC_CNTRL_BUS_WIDTH_1			(0 << 0)
#define	AML_SDXC_CNTRL_BUS_WIDTH_4			(1 << 0)
#define	AML_SDXC_CNTRL_BUS_WIDTH_8			(2 << 0)

#define	AML_SDXC_STATUS_REG				12
#define	AML_SDXC_STATUS_TX_CNT_MASK			(0x7f << 13)
#define	AML_SDXC_STATUS_TX_CNT_SHIFT			13
#define	AML_SDXC_STATUS_RX_CNT_MASK			(0x7f << 6)
#define	AML_SDXC_STATUS_RX_CNT_SHIFT			6
#define	AML_SDXC_STATUS_CMD				(1 << 5)
#define	AML_SDXC_STATUS_DAT3				(1 << 4)
#define	AML_SDXC_STATUS_DAT2				(1 << 3)
#define	AML_SDXC_STATUS_DAT1				(1 << 2)
#define	AML_SDXC_STATUS_DAT0				(1 << 1)
#define	AML_SDXC_STATUS_BUSY				(1 << 0)

#define	AML_SDXC_CLK_CNTRL_REG				16
#define	AML_SDXC_CLK_CNTRL_MEM_PWR_MASK			(3 << 25)
#define	AML_SDXC_CLK_CNTRL_MEM_PWR_OFF			(3 << 25)
#define	AML_SDXC_CLK_CNTRL_MEM_PWR_ON			(0 << 25)
#define	AML_SDXC_CLK_CNTRL_CLK_SEL_MASK			(3 << 16)
#define	AML_SDXC_CLK_CNTRL_CLK_SEL_SHIFT		16
#define	AML_SDXC_CLK_CNTRL_CLK_MODULE_EN		(1 << 15)
#define	AML_SDXC_CLK_CNTRL_SD_CLK_EN			(1 << 14)
#define	AML_SDXC_CLK_CNTRL_RX_CLK_EN			(1 << 13)
#define	AML_SDXC_CLK_CNTRL_TX_CLK_EN			(1 << 12)
#define	AML_SDXC_CLK_CNTRL_CLK_DIV_MASK			0x0fff
#define	AML_SDXC_CLK_CNTRL_CLK_DIV_SHIFT		0

#define	AML_SDXC_DMA_ADDR_REG				20

#define	AML_SDXC_PDMA_REG				24
#define	AML_SDXC_PDMA_TX_FILL				(1U << 31)
#define	AML_SDXC_PDMA_RX_FLUSH_NOW			(1 << 30)
#define	AML_SDXC_PDMA_RX_FLUSH_MODE_SW			(1 << 29)
#define	AML_SDXC_PDMA_TX_THOLD_MASK			(0x3f << 22)
#define	AML_SDXC_PDMA_TX_THOLD_SHIFT			22
#define	AML_SDXC_PDMA_RX_THOLD_MASK			(0x3f << 15)
#define	AML_SDXC_PDMA_RX_THOLD_SHIFT			15
#define	AML_SDXC_PDMA_RD_BURST_MASK			(0x1f << 10)
#define	AML_SDXC_PDMA_RD_BURST_SHIFT			10
#define	AML_SDXC_PDMA_WR_BURST_MASK			(0x1f << 5)
#define	AML_SDXC_PDMA_WR_BURST_SHIFT			5
#define	AML_SDXC_PDMA_DMA_URGENT			(1 << 4)
#define	AML_SDXC_PDMA_RESP_INDEX_MASK			(7 << 1)
#define	AML_SDXC_PDMA_RESP_INDEX_SHIFT			1
#define	AML_SDXC_PDMA_DMA_EN				(1 << 0)

#define	AML_SDXC_MISC_REG				28
#define	AML_SDXC_MISC_TXSTART_THOLD_MASK		(7U << 29)
#define	AML_SDXC_MISC_TXSTART_THOLD_SHIFT		29
#define	AML_SDXC_MISC_MANUAL_STOP_MODE			(1 << 28)
#define	AML_SDXC_MISC_WCRC_OK_PAT_MASK			(7 << 7)
#define	AML_SDXC_MISC_WCRC_OK_PAT_SHIFT			7
#define	AML_SDXC_MISC_WCRC_ERR_PAT_MASK			(7 << 4)
#define	AML_SDXC_MISC_WCRC_ERR_PAT_SHIFT		4

#define	AML_SDXC_DATA_REG				32

#define	AML_SDXC_IRQ_ENABLE_REG				36
#define	AML_SDXC_IRQ_ENABLE_TX_FIFO_EMPTY		(1 << 13)
#define	AML_SDXC_IRQ_ENABLE_RX_FIFO_FULL		(1 << 12)
#define	AML_SDXC_IRQ_ENABLE_DMA_DONE			(1 << 11)
#define	AML_SDXC_IRQ_ENABLE_TRANSFER_DONE_OK		(1 << 7)
#define	AML_SDXC_IRQ_ENABLE_A_PKG_CRC_ERR		(1 << 6)
#define	AML_SDXC_IRQ_ENABLE_A_PKG_TIMEOUT_ERR		(1 << 5)
#define	AML_SDXC_IRQ_ENABLE_A_PKG_DONE_OK		(1 << 4)
#define	AML_SDXC_IRQ_ENABLE_RESP_CRC_ERR		(1 << 2)
#define	AML_SDXC_IRQ_ENABLE_RESP_TIMEOUT_ERR		(1 << 1)
#define	AML_SDXC_IRQ_ENABLE_RESP_OK			(1 << 0)

#define	AML_SDXC_IRQ_ENABLE_STANDARD			\
    (AML_SDXC_IRQ_ENABLE_TX_FIFO_EMPTY |		\
    AML_SDXC_IRQ_ENABLE_RX_FIFO_FULL |			\
    AML_SDXC_IRQ_ENABLE_A_PKG_CRC_ERR |			\
    AML_SDXC_IRQ_ENABLE_A_PKG_TIMEOUT_ERR |		\
    AML_SDXC_IRQ_ENABLE_RESP_CRC_ERR |			\
    AML_SDXC_IRQ_ENABLE_RESP_TIMEOUT_ERR |		\
    AML_SDXC_IRQ_ENABLE_RESP_OK)

#define	AML_SDXC_IRQ_STATUS_REG				40
#define	AML_SDXC_IRQ_STATUS_TX_FIFO_EMPTY		(1 << 13)
#define	AML_SDXC_IRQ_STATUS_RX_FIFO_FULL		(1 << 12)
#define	AML_SDXC_IRQ_STATUS_DMA_DONE			(1 << 11)
#define	AML_SDXC_IRQ_STATUS_TRANSFER_DONE_OK		(1 << 7)
#define	AML_SDXC_IRQ_STATUS_A_PKG_CRC_ERR		(1 << 6)
#define	AML_SDXC_IRQ_STATUS_A_PKG_TIMEOUT_ERR		(1 << 5)
#define	AML_SDXC_IRQ_STATUS_A_PKG_DONE_OK		(1 << 4)
#define	AML_SDXC_IRQ_STATUS_RESP_CRC_ERR		(1 << 2)
#define	AML_SDXC_IRQ_STATUS_RESP_TIMEOUT_ERR		(1 << 1)
#define	AML_SDXC_IRQ_STATUS_RESP_OK			(1 << 0)

#define	AML_SDXC_IRQ_STATUS_CLEAR			\
    (AML_SDXC_IRQ_STATUS_TX_FIFO_EMPTY |		\
    AML_SDXC_IRQ_STATUS_RX_FIFO_FULL |			\
    AML_SDXC_IRQ_STATUS_DMA_DONE |			\
    AML_SDXC_IRQ_STATUS_TRANSFER_DONE_OK |		\
    AML_SDXC_IRQ_STATUS_A_PKG_CRC_ERR |			\
    AML_SDXC_IRQ_STATUS_A_PKG_TIMEOUT_ERR |		\
    AML_SDXC_IRQ_STATUS_RESP_CRC_ERR |			\
    AML_SDXC_IRQ_STATUS_RESP_TIMEOUT_ERR |		\
    AML_SDXC_IRQ_STATUS_RESP_OK)

#define	AML_SDXC_SOFT_RESET_REG				44
#define	AML_SDXC_SOFT_RESET_DMA				(1 << 5)
#define	AML_SDXC_SOFT_RESET_TX_PHY			(1 << 4)
#define	AML_SDXC_SOFT_RESET_RX_PHY			(1 << 3)
#define	AML_SDXC_SOFT_RESET_TX_FIFO			(1 << 2)
#define	AML_SDXC_SOFT_RESET_RX_FIFO			(1 << 1)
#define	AML_SDXC_SOFT_RESET_MAIN			(1 << 0)

#define	AML_SDXC_SOFT_RESET				\
    (AML_SDXC_SOFT_RESET_DMA |				\
    AML_SDXC_SOFT_RESET_TX_FIFO |			\
    AML_SDXC_SOFT_RESET_RX_FIFO |			\
    AML_SDXC_SOFT_RESET_MAIN)

#define	AML_SDXC_ENH_CNTRL_REG				52
#define	AML_SDXC_ENH_CNTRL_TX_EMPTY_THOLD_MASK		(0x7f << 25)
#define	AML_SDXC_ENH_CNTRL_TX_EMPTY_THOLD_SHIFT		25
#define	AML_SDXC_ENH_CNTRL_RX_FULL_THOLD_MASK		(0x7f << 18)
#define	AML_SDXC_ENH_CNTRL_RX_FULL_THOLD_SHIFT		18
#define	AML_SDXC_ENH_CNTRL_SDIO_IRQ_PERIOD_MASK		(0xff << 8)
#define	AML_SDXC_ENH_CNTRL_SDIO_IRQ_PERIOD_SHIFT	8

#define	AML_SDXC_ENH_CNTRL_DMA_NO_WR_RESP_CHECK_M8	(1 << 17)
#define	AML_SDXC_ENH_CNTRL_DMA_NO_RD_RESP_CHECK_M8	(1 << 16)
#define	AML_SDXC_ENH_CNTRL_RX_TIMEOUT_MASK_M8		(0xff << 0)
#define	AML_SDXC_ENH_CNTRL_RX_TIMEOUT_SHIFT_M8		0

#define	AML_SDXC_ENH_CNTRL_NO_DMA_CHECK_M8M2		(1 << 2)
#define	AML_SDXC_ENH_CNTRL_NO_WR_RESP_CHECK_M8M2	(1 << 1)
#define	AML_SDXC_ENH_CNTRL_WR_RESP_MODE_SKIP_M8M2	(1 << 0)

#define	AML_SDXC_CLK2_REG				56
#define	AML_SDXC_CLK2_SD_PHASE_MASK			(0x3ff << 12)
#define	AML_SDXC_CLK2_SD_PHASE_SHIFT			12
#define	AML_SDXC_CLK2_RX_PHASE_MASK			(0x3ff << 0)
#define	AML_SDXC_CLK2_RX_PHASE_SHIFT			0

#endif /* _ARM_AMLOGIC_AML8726_SDXC_M8_H */
