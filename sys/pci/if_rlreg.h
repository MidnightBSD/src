/*-
 * Copyright (c) 1997, 1998-2003
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: release/7.0.0/sys/pci/if_rlreg.h 174609 2007-12-15 02:57:18Z yongari $
 */

/*
 * RealTek 8129/8139 register offsets
 */
#define	RL_IDR0		0x0000		/* ID register 0 (station addr) */
#define RL_IDR1		0x0001		/* Must use 32-bit accesses (?) */
#define RL_IDR2		0x0002
#define RL_IDR3		0x0003
#define RL_IDR4		0x0004
#define RL_IDR5		0x0005
					/* 0006-0007 reserved */
#define RL_MAR0		0x0008		/* Multicast hash table */
#define RL_MAR1		0x0009
#define RL_MAR2		0x000A
#define RL_MAR3		0x000B
#define RL_MAR4		0x000C
#define RL_MAR5		0x000D
#define RL_MAR6		0x000E
#define RL_MAR7		0x000F

#define RL_TXSTAT0	0x0010		/* status of TX descriptor 0 */
#define RL_TXSTAT1	0x0014		/* status of TX descriptor 1 */
#define RL_TXSTAT2	0x0018		/* status of TX descriptor 2 */
#define RL_TXSTAT3	0x001C		/* status of TX descriptor 3 */

#define RL_TXADDR0	0x0020		/* address of TX descriptor 0 */
#define RL_TXADDR1	0x0024		/* address of TX descriptor 1 */
#define RL_TXADDR2	0x0028		/* address of TX descriptor 2 */
#define RL_TXADDR3	0x002C		/* address of TX descriptor 3 */

#define RL_RXADDR		0x0030	/* RX ring start address */
#define RL_RX_EARLY_BYTES	0x0034	/* RX early byte count */
#define RL_RX_EARLY_STAT	0x0036	/* RX early status */
#define RL_COMMAND	0x0037		/* command register */
#define RL_CURRXADDR	0x0038		/* current address of packet read */
#define RL_CURRXBUF	0x003A		/* current RX buffer address */
#define RL_IMR		0x003C		/* interrupt mask register */
#define RL_ISR		0x003E		/* interrupt status register */
#define RL_TXCFG	0x0040		/* transmit config */
#define RL_RXCFG	0x0044		/* receive config */
#define RL_TIMERCNT	0x0048		/* timer count register */
#define RL_MISSEDPKT	0x004C		/* missed packet counter */
#define RL_EECMD	0x0050		/* EEPROM command register */
#define RL_CFG0		0x0051		/* config register #0 */
#define RL_CFG1		0x0052		/* config register #1 */
                                        /* 0053-0057 reserved */   
#define RL_MEDIASTAT	0x0058		/* media status register (8139) */
					/* 0059-005A reserved */
#define RL_MII		0x005A		/* 8129 chip only */
#define RL_HALTCLK	0x005B
#define RL_MULTIINTR	0x005C		/* multiple interrupt */
#define RL_PCIREV	0x005E		/* PCI revision value */
					/* 005F reserved */
#define RL_TXSTAT_ALL	0x0060		/* TX status of all descriptors */

/* Direct PHY access registers only available on 8139 */
#define RL_BMCR		0x0062		/* PHY basic mode control */
#define RL_BMSR		0x0064		/* PHY basic mode status */
#define RL_ANAR		0x0066		/* PHY autoneg advert */
#define RL_LPAR		0x0068		/* PHY link partner ability */
#define RL_ANER		0x006A		/* PHY autoneg expansion */

#define RL_DISCCNT	0x006C		/* disconnect counter */
#define RL_FALSECAR	0x006E		/* false carrier counter */
#define RL_NWAYTST	0x0070		/* NWAY test register */
#define RL_RX_ER	0x0072		/* RX_ER counter */
#define RL_CSCFG	0x0074		/* CS configuration register */

/*
 * When operating in special C+ mode, some of the registers in an
 * 8139C+ chip have different definitions. These are also used for
 * the 8169 gigE chip.
 */
#define RL_DUMPSTATS_LO		0x0010	/* counter dump command register */
#define RL_DUMPSTATS_HI		0x0014	/* counter dump command register */
#define RL_TXLIST_ADDR_LO	0x0020	/* 64 bits, 256 byte alignment */
#define RL_TXLIST_ADDR_HI	0x0024	/* 64 bits, 256 byte alignment */
#define RL_TXLIST_ADDR_HPRIO_LO	0x0028	/* 64 bits, 256 byte alignment */
#define RL_TXLIST_ADDR_HPRIO_HI	0x002C	/* 64 bits, 256 byte alignment */
#define RL_CFG2			0x0053
#define RL_TIMERINT		0x0054	/* interrupt on timer expire */
#define RL_TXSTART		0x00D9	/* 8 bits */
#define RL_CPLUS_CMD		0x00E0	/* 16 bits */
#define RL_RXLIST_ADDR_LO	0x00E4	/* 64 bits, 256 byte alignment */
#define RL_RXLIST_ADDR_HI	0x00E8	/* 64 bits, 256 byte alignment */
#define RL_EARLY_TX_THRESH	0x00EC	/* 8 bits */

/*
 * Registers specific to the 8169 gigE chip
 */
#define RL_TIMERINT_8169	0x0058	/* different offset than 8139 */
#define RL_PHYAR		0x0060
#define RL_TBICSR		0x0064
#define RL_TBI_ANAR		0x0068
#define RL_TBI_LPAR		0x006A
#define RL_GMEDIASTAT		0x006C	/* 8 bits */
#define RL_MAXRXPKTLEN		0x00DA	/* 16 bits, chip multiplies by 8 */
#define RL_GTXSTART		0x0038	/* 16 bits */

/*
 * TX config register bits
 */
#define RL_TXCFG_CLRABRT	0x00000001	/* retransmit aborted pkt */
#define RL_TXCFG_MAXDMA		0x00000700	/* max DMA burst size */
#define RL_TXCFG_CRCAPPEND	0x00010000	/* CRC append (0 = yes) */
#define RL_TXCFG_LOOPBKTST	0x00060000	/* loopback test */
#define RL_TXCFG_IFG2		0x00080000	/* 8169 only */
#define RL_TXCFG_IFG		0x03000000	/* interframe gap */
#define RL_TXCFG_HWREV		0x7CC00000

#define RL_LOOPTEST_OFF		0x00000000
#define RL_LOOPTEST_ON		0x00020000
#define RL_LOOPTEST_ON_CPLUS	0x00060000

/* Known revision codes. */

#define RL_HWREV_8169		0x00000000
#define RL_HWREV_8110S		0x00800000
#define RL_HWREV_8169S		0x04000000
#define RL_HWREV_8169_8110SB	0x10000000
#define RL_HWREV_8169_8110SC	0x18000000
#define RL_HWREV_8168_SPIN1	0x30000000
#define RL_HWREV_8100E		0x30800000
#define RL_HWREV_8101E		0x34000000
#define RL_HWREV_8168_SPIN2	0x38000000
#define RL_HWREV_8168_SPIN3	0x38400000
#define RL_HWREV_8139		0x60000000
#define RL_HWREV_8139A		0x70000000
#define RL_HWREV_8139AG		0x70800000
#define RL_HWREV_8139B		0x78000000
#define RL_HWREV_8130		0x7C000000
#define RL_HWREV_8139C		0x74000000
#define RL_HWREV_8139D		0x74400000
#define RL_HWREV_8139CPLUS	0x74800000
#define RL_HWREV_8101		0x74c00000
#define RL_HWREV_8100		0x78800000

#define RL_TXDMA_16BYTES	0x00000000
#define RL_TXDMA_32BYTES	0x00000100
#define RL_TXDMA_64BYTES	0x00000200
#define RL_TXDMA_128BYTES	0x00000300
#define RL_TXDMA_256BYTES	0x00000400
#define RL_TXDMA_512BYTES	0x00000500
#define RL_TXDMA_1024BYTES	0x00000600
#define RL_TXDMA_2048BYTES	0x00000700

/*
 * Transmit descriptor status register bits.
 */
#define RL_TXSTAT_LENMASK	0x00001FFF
#define RL_TXSTAT_OWN		0x00002000
#define RL_TXSTAT_TX_UNDERRUN	0x00004000
#define RL_TXSTAT_TX_OK		0x00008000
#define RL_TXSTAT_EARLY_THRESH	0x003F0000
#define RL_TXSTAT_COLLCNT	0x0F000000
#define RL_TXSTAT_CARR_HBEAT	0x10000000
#define RL_TXSTAT_OUTOFWIN	0x20000000
#define RL_TXSTAT_TXABRT	0x40000000
#define RL_TXSTAT_CARRLOSS	0x80000000

/*
 * Interrupt status register bits.
 */
#define RL_ISR_RX_OK		0x0001
#define RL_ISR_RX_ERR		0x0002
#define RL_ISR_TX_OK		0x0004
#define RL_ISR_TX_ERR		0x0008
#define RL_ISR_RX_OVERRUN	0x0010
#define RL_ISR_PKT_UNDERRUN	0x0020
#define RL_ISR_LINKCHG		0x0020	/* 8169 only */
#define RL_ISR_FIFO_OFLOW	0x0040	/* 8139 only */
#define RL_ISR_TX_DESC_UNAVAIL	0x0080	/* C+ only */
#define RL_ISR_SWI		0x0100	/* C+ only */
#define RL_ISR_CABLE_LEN_CHGD	0x2000
#define RL_ISR_PCS_TIMEOUT	0x4000	/* 8129 only */
#define RL_ISR_TIMEOUT_EXPIRED	0x4000
#define RL_ISR_SYSTEM_ERR	0x8000

#define RL_INTRS	\
	(RL_ISR_TX_OK|RL_ISR_RX_OK|RL_ISR_RX_ERR|RL_ISR_TX_ERR|		\
	RL_ISR_RX_OVERRUN|RL_ISR_PKT_UNDERRUN|RL_ISR_FIFO_OFLOW|	\
	RL_ISR_PCS_TIMEOUT|RL_ISR_SYSTEM_ERR)

#ifdef RE_TX_MODERATION
#define RL_INTRS_CPLUS	\
	(RL_ISR_RX_OK|RL_ISR_RX_ERR|RL_ISR_TX_ERR|			\
	RL_ISR_RX_OVERRUN|RL_ISR_PKT_UNDERRUN|RL_ISR_FIFO_OFLOW|	\
	RL_ISR_PCS_TIMEOUT|RL_ISR_SYSTEM_ERR|RL_ISR_TIMEOUT_EXPIRED)
#else
#define RL_INTRS_CPLUS	\
	(RL_ISR_RX_OK|RL_ISR_RX_ERR|RL_ISR_TX_ERR|RL_ISR_TX_OK|		\
	RL_ISR_RX_OVERRUN|RL_ISR_PKT_UNDERRUN|RL_ISR_FIFO_OFLOW|	\
	RL_ISR_PCS_TIMEOUT|RL_ISR_SYSTEM_ERR|RL_ISR_TIMEOUT_EXPIRED)
#endif

/*
 * Media status register. (8139 only)
 */
#define RL_MEDIASTAT_RXPAUSE	0x01
#define RL_MEDIASTAT_TXPAUSE	0x02
#define RL_MEDIASTAT_LINK	0x04
#define RL_MEDIASTAT_SPEED10	0x08
#define RL_MEDIASTAT_RXFLOWCTL	0x40	/* duplex mode */
#define RL_MEDIASTAT_TXFLOWCTL	0x80	/* duplex mode */

/*
 * Receive config register.
 */
#define RL_RXCFG_RX_ALLPHYS	0x00000001	/* accept all nodes */
#define RL_RXCFG_RX_INDIV	0x00000002	/* match filter */
#define RL_RXCFG_RX_MULTI	0x00000004	/* accept all multicast */
#define RL_RXCFG_RX_BROAD	0x00000008	/* accept all broadcast */
#define RL_RXCFG_RX_RUNT	0x00000010
#define RL_RXCFG_RX_ERRPKT	0x00000020
#define RL_RXCFG_WRAP		0x00000080
#define RL_RXCFG_MAXDMA		0x00000700
#define RL_RXCFG_BUFSZ		0x00001800
#define RL_RXCFG_FIFOTHRESH	0x0000E000
#define RL_RXCFG_EARLYTHRESH	0x07000000

#define RL_RXDMA_16BYTES	0x00000000
#define RL_RXDMA_32BYTES	0x00000100
#define RL_RXDMA_64BYTES	0x00000200
#define RL_RXDMA_128BYTES	0x00000300
#define RL_RXDMA_256BYTES	0x00000400
#define RL_RXDMA_512BYTES	0x00000500
#define RL_RXDMA_1024BYTES	0x00000600
#define RL_RXDMA_UNLIMITED	0x00000700

#define RL_RXBUF_8		0x00000000
#define RL_RXBUF_16		0x00000800
#define RL_RXBUF_32		0x00001000
#define RL_RXBUF_64		0x00001800

#define RL_RXFIFO_16BYTES	0x00000000
#define RL_RXFIFO_32BYTES	0x00002000
#define RL_RXFIFO_64BYTES	0x00004000
#define RL_RXFIFO_128BYTES	0x00006000
#define RL_RXFIFO_256BYTES	0x00008000
#define RL_RXFIFO_512BYTES	0x0000A000
#define RL_RXFIFO_1024BYTES	0x0000C000
#define RL_RXFIFO_NOTHRESH	0x0000E000

/*
 * Bits in RX status header (included with RX'ed packet
 * in ring buffer).
 */
#define RL_RXSTAT_RXOK		0x00000001
#define RL_RXSTAT_ALIGNERR	0x00000002
#define RL_RXSTAT_CRCERR	0x00000004
#define RL_RXSTAT_GIANT		0x00000008
#define RL_RXSTAT_RUNT		0x00000010
#define RL_RXSTAT_BADSYM	0x00000020
#define RL_RXSTAT_BROAD		0x00002000
#define RL_RXSTAT_INDIV		0x00004000
#define RL_RXSTAT_MULTI		0x00008000
#define RL_RXSTAT_LENMASK	0xFFFF0000

#define RL_RXSTAT_UNFINISHED	0xFFF0		/* DMA still in progress */
/*
 * Command register.
 */
#define RL_CMD_EMPTY_RXBUF	0x0001
#define RL_CMD_TX_ENB		0x0004
#define RL_CMD_RX_ENB		0x0008
#define RL_CMD_RESET		0x0010

/*
 * EEPROM control register
 */
#define RL_EE_DATAOUT		0x01	/* Data out */
#define RL_EE_DATAIN		0x02	/* Data in */
#define RL_EE_CLK		0x04	/* clock */
#define RL_EE_SEL		0x08	/* chip select */
#define RL_EE_MODE		(0x40|0x80)

#define RL_EEMODE_OFF		0x00
#define RL_EEMODE_AUTOLOAD	0x40
#define RL_EEMODE_PROGRAM	0x80
#define RL_EEMODE_WRITECFG	(0x80|0x40)

/* 9346 EEPROM commands */
#define RL_9346_ADDR_LEN	6	/* 93C46 1K: 128x16 */
#define RL_9356_ADDR_LEN	8	/* 93C56 2K: 256x16 */

#define RL_9346_WRITE          0x5
#define RL_9346_READ           0x6
#define RL_9346_ERASE          0x7
#define RL_9346_EWEN           0x4
#define RL_9346_EWEN_ADDR      0x30
#define RL_9456_EWDS           0x4
#define RL_9346_EWDS_ADDR      0x00

#define RL_EECMD_WRITE		0x140
#define RL_EECMD_READ_6BIT	0x180
#define RL_EECMD_READ_8BIT	0x600
#define RL_EECMD_ERASE		0x1c0

#define RL_EE_ID		0x00
#define RL_EE_PCI_VID		0x01
#define RL_EE_PCI_DID		0x02
/* Location of station address inside EEPROM */
#define RL_EE_EADDR		0x07

/*
 * MII register (8129 only)
 */
#define RL_MII_CLK		0x01
#define RL_MII_DATAIN		0x02
#define RL_MII_DATAOUT		0x04
#define RL_MII_DIR		0x80	/* 0 == input, 1 == output */

/*
 * Config 0 register
 */
#define RL_CFG0_ROM0		0x01
#define RL_CFG0_ROM1		0x02
#define RL_CFG0_ROM2		0x04
#define RL_CFG0_PL0		0x08
#define RL_CFG0_PL1		0x10
#define RL_CFG0_10MBPS		0x20	/* 10 Mbps internal mode */
#define RL_CFG0_PCS		0x40
#define RL_CFG0_SCR		0x80

/*
 * Config 1 register
 */
#define RL_CFG1_PWRDWN		0x01
#define RL_CFG1_SLEEP		0x02
#define RL_CFG1_IOMAP		0x04
#define RL_CFG1_MEMMAP		0x08
#define RL_CFG1_RSVD		0x10
#define RL_CFG1_DRVLOAD		0x20
#define RL_CFG1_LED0		0x40
#define RL_CFG1_FULLDUPLEX	0x40	/* 8129 only */
#define RL_CFG1_LED1		0x80

/*
 * 8139C+ register definitions
 */

/* RL_DUMPSTATS_LO register */

#define RL_DUMPSTATS_START	0x00000008

/* Transmit start register */

#define RL_TXSTART_SWI		0x01	/* generate TX interrupt */
#define RL_TXSTART_START	0x40	/* start normal queue transmit */
#define RL_TXSTART_HPRIO_START	0x80	/* start hi prio queue transmit */

/*
 * Config 2 register, 8139C+/8169/8169S/8110S only
 */
#define RL_CFG2_BUSFREQ		0x07
#define RL_CFG2_BUSWIDTH	0x08
#define RL_CFG2_AUXPWRSTS	0x10

#define RL_BUSFREQ_33MHZ	0x00
#define RL_BUSFREQ_66MHZ	0x01
                                        
#define RL_BUSWIDTH_32BITS	0x00
#define RL_BUSWIDTH_64BITS	0x08

/* C+ mode command register */

#define RL_CPLUSCMD_TXENB	0x0001	/* enable C+ transmit mode */
#define RL_CPLUSCMD_RXENB	0x0002	/* enable C+ receive mode */
#define RL_CPLUSCMD_PCI_MRW	0x0008	/* enable PCI multi-read/write */
#define RL_CPLUSCMD_PCI_DAC	0x0010	/* PCI dual-address cycle only */
#define RL_CPLUSCMD_RXCSUM_ENB	0x0020	/* enable RX checksum offload */
#define RL_CPLUSCMD_VLANSTRIP	0x0040	/* enable VLAN tag stripping */

/* C+ early transmit threshold */

#define RL_EARLYTXTHRESH_CNT	0x003F	/* byte count times 8 */ 

/*
 * Gigabit PHY access register (8169 only)
 */

#define RL_PHYAR_PHYDATA	0x0000FFFF
#define RL_PHYAR_PHYREG		0x001F0000
#define RL_PHYAR_BUSY		0x80000000

/*
 * Gigabit media status (8169 only)
 */
#define RL_GMEDIASTAT_FDX	0x01	/* full duplex */
#define RL_GMEDIASTAT_LINK	0x02	/* link up */
#define RL_GMEDIASTAT_10MBPS	0x04	/* 10mps link */
#define RL_GMEDIASTAT_100MBPS	0x08	/* 100mbps link */
#define RL_GMEDIASTAT_1000MBPS	0x10	/* gigE link */
#define RL_GMEDIASTAT_RXFLOW	0x20	/* RX flow control on */
#define RL_GMEDIASTAT_TXFLOW	0x40	/* TX flow control on */
#define RL_GMEDIASTAT_TBI	0x80	/* TBI enabled */

/*
 * The RealTek doesn't use a fragment-based descriptor mechanism.
 * Instead, there are only four register sets, each or which represents
 * one 'descriptor.' Basically, each TX descriptor is just a contiguous
 * packet buffer (32-bit aligned!) and we place the buffer addresses in
 * the registers so the chip knows where they are.
 *
 * We can sort of kludge together the same kind of buffer management
 * used in previous drivers, but we have to do buffer copies almost all
 * the time, so it doesn't really buy us much.
 *
 * For reception, there's just one large buffer where the chip stores
 * all received packets.
 */

#define RL_RX_BUF_SZ		RL_RXBUF_64
#define RL_RXBUFLEN		(1 << ((RL_RX_BUF_SZ >> 11) + 13))
#define RL_TX_LIST_CNT		4
#define RL_MIN_FRAMELEN		60
#define RL_TXTHRESH(x)		((x) << 11)
#define RL_TX_THRESH_INIT	96
#define RL_RX_FIFOTHRESH	RL_RXFIFO_NOTHRESH
#define RL_RX_MAXDMA		RL_RXDMA_UNLIMITED
#define RL_TX_MAXDMA		RL_TXDMA_2048BYTES

#define RL_RXCFG_CONFIG (RL_RX_FIFOTHRESH|RL_RX_MAXDMA|RL_RX_BUF_SZ)
#define RL_TXCFG_CONFIG	(RL_TXCFG_IFG|RL_TX_MAXDMA)

#define RL_ETHER_ALIGN	2

struct rl_chain_data {
	uint16_t		cur_rx;
	uint8_t			*rl_rx_buf;
	uint8_t			*rl_rx_buf_ptr;
	bus_dmamap_t		rl_rx_dmamap;

	struct mbuf		*rl_tx_chain[RL_TX_LIST_CNT];
	bus_dmamap_t		rl_tx_dmamap[RL_TX_LIST_CNT];
	uint8_t			last_tx;
	uint8_t			cur_tx;
};

#define RL_INC(x)		(x = (x + 1) % RL_TX_LIST_CNT)
#define RL_CUR_TXADDR(x)	((x->rl_cdata.cur_tx * 4) + RL_TXADDR0)
#define RL_CUR_TXSTAT(x)	((x->rl_cdata.cur_tx * 4) + RL_TXSTAT0)
#define RL_CUR_TXMBUF(x)	(x->rl_cdata.rl_tx_chain[x->rl_cdata.cur_tx])
#define RL_CUR_DMAMAP(x)	(x->rl_cdata.rl_tx_dmamap[x->rl_cdata.cur_tx])
#define RL_LAST_TXADDR(x)	((x->rl_cdata.last_tx * 4) + RL_TXADDR0)
#define RL_LAST_TXSTAT(x)	((x->rl_cdata.last_tx * 4) + RL_TXSTAT0)
#define RL_LAST_TXMBUF(x)	(x->rl_cdata.rl_tx_chain[x->rl_cdata.last_tx])
#define RL_LAST_DMAMAP(x)	(x->rl_cdata.rl_tx_dmamap[x->rl_cdata.last_tx])

struct rl_type {
	uint16_t		rl_vid;
	uint16_t		rl_did;
	int			rl_basetype;
	char			*rl_name;
};

struct rl_hwrev {
	uint32_t		rl_rev;
	int			rl_type;
	char			*rl_desc;
};

struct rl_mii_frame {
	uint8_t		mii_stdelim;
	uint8_t		mii_opcode;
	uint8_t		mii_phyaddr;
	uint8_t		mii_regaddr;
	uint8_t		mii_turnaround;
	uint16_t	mii_data;
};

/*
 * MII constants
 */
#define RL_MII_STARTDELIM	0x01
#define RL_MII_READOP		0x02
#define RL_MII_WRITEOP		0x01
#define RL_MII_TURNAROUND	0x02

#define RL_8129			1
#define RL_8139			2
#define RL_8139CPLUS		3
#define RL_8169			4

#define RL_ISCPLUS(x)		((x)->rl_type == RL_8139CPLUS ||	\
				 (x)->rl_type == RL_8169)

/*
 * The 8139C+ and 8160 gigE chips support descriptor-based TX
 * and RX. In fact, they even support TCP large send. Descriptors
 * must be allocated in contiguous blocks that are aligned on a
 * 256-byte boundary. The rings can hold a maximum of 64 descriptors.
 */

/*
 * RX/TX descriptor definition. When large send mode is enabled, the
 * lower 11 bits of the TX rl_cmd word are used to hold the MSS, and
 * the checksum offload bits are disabled. The structure layout is
 * the same for RX and TX descriptors
 */

struct rl_desc {
	uint32_t		rl_cmdstat;
	uint32_t		rl_vlanctl;
	uint32_t		rl_bufaddr_lo;
	uint32_t		rl_bufaddr_hi;
};

#define RL_TDESC_CMD_FRAGLEN	0x0000FFFF
#define RL_TDESC_CMD_TCPCSUM	0x00010000	/* TCP checksum enable */
#define RL_TDESC_CMD_UDPCSUM	0x00020000	/* UDP checksum enable */
#define RL_TDESC_CMD_IPCSUM	0x00040000	/* IP header checksum enable */
#define RL_TDESC_CMD_MSSVAL	0x07FF0000	/* Large send MSS value */
#define RL_TDESC_CMD_MSSVAL_SHIFT	16	/* Large send MSS value shift */
#define RL_TDESC_CMD_LGSEND	0x08000000	/* TCP large send enb */
#define RL_TDESC_CMD_EOF	0x10000000	/* end of frame marker */
#define RL_TDESC_CMD_SOF	0x20000000	/* start of frame marker */
#define RL_TDESC_CMD_EOR	0x40000000	/* end of ring marker */
#define RL_TDESC_CMD_OWN	0x80000000	/* chip owns descriptor */

#define RL_TDESC_VLANCTL_TAG	0x00020000	/* Insert VLAN tag */
#define RL_TDESC_VLANCTL_DATA	0x0000FFFF	/* TAG data */

/*
 * Error bits are valid only on the last descriptor of a frame
 * (i.e. RL_TDESC_CMD_EOF == 1)
 */

#define RL_TDESC_STAT_COLCNT	0x000F0000	/* collision count */
#define RL_TDESC_STAT_EXCESSCOL	0x00100000	/* excessive collisions */
#define RL_TDESC_STAT_LINKFAIL	0x00200000	/* link faulure */
#define RL_TDESC_STAT_OWINCOL	0x00400000	/* out-of-window collision */
#define RL_TDESC_STAT_TXERRSUM	0x00800000	/* transmit error summary */
#define RL_TDESC_STAT_UNDERRUN	0x02000000	/* TX underrun occured */
#define RL_TDESC_STAT_OWN	0x80000000

/*
 * RX descriptor cmd/vlan definitions
 */

#define RL_RDESC_CMD_EOR	0x40000000
#define RL_RDESC_CMD_OWN	0x80000000
#define RL_RDESC_CMD_BUFLEN	0x00001FFF

#define RL_RDESC_STAT_OWN	0x80000000
#define RL_RDESC_STAT_EOR	0x40000000
#define RL_RDESC_STAT_SOF	0x20000000
#define RL_RDESC_STAT_EOF	0x10000000
#define RL_RDESC_STAT_FRALIGN	0x08000000	/* frame alignment error */
#define RL_RDESC_STAT_MCAST	0x04000000	/* multicast pkt received */
#define RL_RDESC_STAT_UCAST	0x02000000	/* unicast pkt received */
#define RL_RDESC_STAT_BCAST	0x01000000	/* broadcast pkt received */
#define RL_RDESC_STAT_BUFOFLOW	0x00800000	/* out of buffer space */
#define RL_RDESC_STAT_FIFOOFLOW	0x00400000	/* FIFO overrun */
#define RL_RDESC_STAT_GIANT	0x00200000	/* pkt > 4096 bytes */
#define RL_RDESC_STAT_RXERRSUM	0x00100000	/* RX error summary */
#define RL_RDESC_STAT_RUNT	0x00080000	/* runt packet received */
#define RL_RDESC_STAT_CRCERR	0x00040000	/* CRC error */
#define RL_RDESC_STAT_PROTOID	0x00030000	/* Protocol type */
#define RL_RDESC_STAT_IPSUMBAD	0x00008000	/* IP header checksum bad */
#define RL_RDESC_STAT_UDPSUMBAD	0x00004000	/* UDP checksum bad */
#define RL_RDESC_STAT_TCPSUMBAD	0x00002000	/* TCP checksum bad */
#define RL_RDESC_STAT_FRAGLEN	0x00001FFF	/* RX'ed frame/frag len */
#define RL_RDESC_STAT_GFRAGLEN	0x00003FFF	/* RX'ed frame/frag len */
#define RL_RDESC_STAT_ERRS	(RL_RDESC_STAT_GIANT|RL_RDESC_STAT_RUNT| \
				 RL_RDESC_STAT_CRCERR)

#define RL_RDESC_VLANCTL_TAG	0x00010000	/* VLAN tag available
						   (rl_vlandata valid)*/
#define RL_RDESC_VLANCTL_DATA	0x0000FFFF	/* TAG data */

#define RL_PROTOID_NONIP	0x00000000
#define RL_PROTOID_TCPIP	0x00010000
#define RL_PROTOID_UDPIP	0x00020000
#define RL_PROTOID_IP		0x00030000
#define RL_TCPPKT(x)		(((x) & RL_RDESC_STAT_PROTOID) == \
				 RL_PROTOID_TCPIP)
#define RL_UDPPKT(x)		(((x) & RL_RDESC_STAT_PROTOID) == \
				 RL_PROTOID_UDPIP)

/*
 * Statistics counter structure (8139C+ and 8169 only)
 */
struct rl_stats {
	uint32_t		rl_tx_pkts_lo;
	uint32_t		rl_tx_pkts_hi;
	uint32_t		rl_tx_errs_lo;
	uint32_t		rl_tx_errs_hi;
	uint32_t		rl_tx_errs;
	uint16_t		rl_missed_pkts;
	uint16_t		rl_rx_framealign_errs;
	uint32_t		rl_tx_onecoll;
	uint32_t		rl_tx_multicolls;
	uint32_t		rl_rx_ucasts_hi;
	uint32_t		rl_rx_ucasts_lo;
	uint32_t		rl_rx_bcasts_lo;
	uint32_t		rl_rx_bcasts_hi;
	uint32_t		rl_rx_mcasts;
	uint16_t		rl_tx_aborts;
	uint16_t		rl_rx_underruns;
};

/*
 * Rx/Tx descriptor parameters (8139C+ and 8169 only)
 *
 * Tx/Rx count must be equal.  Shared code like re_dma_map_desc assumes this.
 * Buffers must be a multiple of 8 bytes.  Currently limit to 64 descriptors
 * due to the 8139C+.  We need to put the number of descriptors in the ring
 * structure and use that value instead.
 */
#ifndef	__NO_STRICT_ALIGNMENT
#define RE_FIXUP_RX	1
#endif

#define RL_TX_DESC_CNT		64
#define RL_TX_DESC_THLD		4
#define RL_RX_DESC_CNT		RL_TX_DESC_CNT

#define RL_RX_LIST_SZ		(RL_RX_DESC_CNT * sizeof(struct rl_desc))
#define RL_TX_LIST_SZ		(RL_TX_DESC_CNT * sizeof(struct rl_desc))
#define RL_RING_ALIGN		256
#define RL_IFQ_MAXLEN		512
#define RL_DESC_INC(x)		(x = (x + 1) % RL_TX_DESC_CNT)
#define RL_OWN(x)		(le32toh((x)->rl_cmdstat) & RL_RDESC_STAT_OWN)
#define RL_RXBYTES(x)		(le32toh((x)->rl_cmdstat) & sc->rl_rxlenmask)
#define RL_PKTSZ(x)		((x)/* >> 3*/)
#ifdef RE_FIXUP_RX
#define RE_ETHER_ALIGN	sizeof(uint64_t)
#define RE_RX_DESC_BUFLEN	(MCLBYTES - RE_ETHER_ALIGN)
#else
#define RE_ETHER_ALIGN	0
#define RE_RX_DESC_BUFLEN	MCLBYTES
#endif

#define	RL_MSI_MESSAGES	2

#define RL_ADDR_LO(y)		((uint64_t) (y) & 0xFFFFFFFF)
#define RL_ADDR_HI(y)		((uint64_t) (y) >> 32)

/* see comment in dev/re/if_re.c */
#define RL_JUMBO_FRAMELEN	7440
#define RL_JUMBO_MTU		(RL_JUMBO_FRAMELEN-ETHER_HDR_LEN-ETHER_CRC_LEN)

struct rl_softc;

struct rl_dmaload_arg {
	int			rl_idx;
	int			rl_maxsegs;
	uint32_t		rl_flags;
	struct rl_desc		*rl_ring;
};

struct rl_list_data {
	struct mbuf		*rl_tx_mbuf[RL_TX_DESC_CNT];
	struct mbuf		*rl_rx_mbuf[RL_RX_DESC_CNT];
	int			rl_tx_prodidx;
	int			rl_rx_prodidx;
	int			rl_tx_considx;
	int			rl_tx_free;
	bus_dmamap_t		rl_tx_dmamap[RL_TX_DESC_CNT];
	bus_dmamap_t		rl_rx_dmamap[RL_RX_DESC_CNT];
	bus_dma_tag_t		rl_mtag;	/* mbuf mapping tag */
	bus_dma_tag_t		rl_stag;	/* stats mapping tag */
	bus_dmamap_t		rl_smap;	/* stats map */
	struct rl_stats		*rl_stats;
	bus_addr_t		rl_stats_addr;
	bus_dma_tag_t		rl_rx_list_tag;
	bus_dmamap_t		rl_rx_list_map;
	struct rl_desc		*rl_rx_list;
	bus_addr_t		rl_rx_list_addr;
	bus_dma_tag_t		rl_tx_list_tag;
	bus_dmamap_t		rl_tx_list_map;
	struct rl_desc		*rl_tx_list;
	bus_addr_t		rl_tx_list_addr;
};

struct rl_softc {
	struct ifnet		*rl_ifp;	/* interface info */
	bus_space_handle_t	rl_bhandle;	/* bus space handle */
	bus_space_tag_t		rl_btag;	/* bus space tag */
	device_t		rl_dev;
	struct resource		*rl_res;
	struct resource		*rl_irq[RL_MSI_MESSAGES];
	void			*rl_intrhand[RL_MSI_MESSAGES];
	device_t		rl_miibus;
	bus_dma_tag_t		rl_parent_tag;
	bus_dma_tag_t		rl_tag;
	uint8_t			rl_type;
	int			rl_eecmd_read;
	int			rl_eewidth;
	uint8_t			rl_stats_no_timeout;
	int			rl_txthresh;
	struct rl_chain_data	rl_cdata;
	struct rl_list_data	rl_ldata;
	struct callout		rl_stat_callout;
	int			rl_watchdog_timer;
	struct mtx		rl_mtx;
	struct mbuf		*rl_head;
	struct mbuf		*rl_tail;
	uint32_t		rl_hwrev;
	uint32_t		rl_rxlenmask;
	int			rl_testmode;
	int			rl_if_flags;
	int			suspended;	/* 0 = normal  1 = suspended */
#ifdef DEVICE_POLLING
	int			rxcycles;
#endif

	struct task		rl_txtask;
	struct task		rl_inttask;

	struct mtx		rl_intlock;
	int			rl_txstart;
	int			rl_link;
	int			rl_msi;
};

#define	RL_LOCK(_sc)		mtx_lock(&(_sc)->rl_mtx)
#define	RL_UNLOCK(_sc)		mtx_unlock(&(_sc)->rl_mtx)
#define	RL_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->rl_mtx, MA_OWNED)

/*
 * register space access macros
 */
#define CSR_WRITE_STREAM_4(sc, reg, val)	\
	bus_space_write_stream_4(sc->rl_btag, sc->rl_bhandle, reg, val)
#define CSR_WRITE_4(sc, reg, val)	\
	bus_space_write_4(sc->rl_btag, sc->rl_bhandle, reg, val)
#define CSR_WRITE_2(sc, reg, val)	\
	bus_space_write_2(sc->rl_btag, sc->rl_bhandle, reg, val)
#define CSR_WRITE_1(sc, reg, val)	\
	bus_space_write_1(sc->rl_btag, sc->rl_bhandle, reg, val)

#define CSR_READ_4(sc, reg)		\
	bus_space_read_4(sc->rl_btag, sc->rl_bhandle, reg)
#define CSR_READ_2(sc, reg)		\
	bus_space_read_2(sc->rl_btag, sc->rl_bhandle, reg)
#define CSR_READ_1(sc, reg)		\
	bus_space_read_1(sc->rl_btag, sc->rl_bhandle, reg)

#define CSR_SETBIT_1(sc, offset, val)		\
	CSR_WRITE_1(sc, offset, CSR_READ_1(sc, offset) | (val))

#define CSR_CLRBIT_1(sc, offset, val)		\
	CSR_WRITE_1(sc, offset, CSR_READ_1(sc, offset) & ~(val))

#define CSR_SETBIT_2(sc, offset, val)		\
	CSR_WRITE_2(sc, offset, CSR_READ_2(sc, offset) | (val))

#define CSR_CLRBIT_2(sc, offset, val)		\
	CSR_WRITE_2(sc, offset, CSR_READ_2(sc, offset) & ~(val))

#define CSR_SETBIT_4(sc, offset, val)		\
	CSR_WRITE_4(sc, offset, CSR_READ_4(sc, offset) | (val))

#define CSR_CLRBIT_4(sc, offset, val)		\
	CSR_WRITE_4(sc, offset, CSR_READ_4(sc, offset) & ~(val))

#define RL_TIMEOUT		1000

/*
 * General constants that are fun to know.
 *
 * RealTek PCI vendor ID
 */
#define	RT_VENDORID				0x10EC

/*
 * RealTek chip device IDs.
 */
#define	RT_DEVICEID_8129			0x8129
#define RT_DEVICEID_8101E			0x8136
#define	RT_DEVICEID_8138			0x8138
#define	RT_DEVICEID_8139			0x8139
#define RT_DEVICEID_8169SC			0x8167
#define RT_DEVICEID_8168			0x8168
#define RT_DEVICEID_8169			0x8169
#define RT_DEVICEID_8100			0x8100

#define RT_REVID_8139CPLUS			0x20

/*
 * Accton PCI vendor ID
 */
#define ACCTON_VENDORID				0x1113

/*
 * Accton MPX 5030/5038 device ID.
 */
#define ACCTON_DEVICEID_5030			0x1211

/*
 * Nortel PCI vendor ID
 */
#define NORTEL_VENDORID				0x126C

/*
 * Delta Electronics Vendor ID.
 */
#define DELTA_VENDORID				0x1500

/*
 * Delta device IDs.
 */
#define DELTA_DEVICEID_8139			0x1360

/*
 * Addtron vendor ID.
 */
#define ADDTRON_VENDORID			0x4033

/*
 * Addtron device IDs.
 */
#define ADDTRON_DEVICEID_8139			0x1360

/*
 * D-Link vendor ID.
 */
#define DLINK_VENDORID				0x1186

/*
 * D-Link DFE-530TX+ device ID
 */
#define DLINK_DEVICEID_530TXPLUS		0x1300

/*
 * D-Link DFE-5280T device ID
 */
#define DLINK_DEVICEID_528T			0x4300

/*
 * D-Link DFE-690TXD device ID
 */
#define DLINK_DEVICEID_690TXD			0x1340

/*
 * Corega K.K vendor ID
 */
#define COREGA_VENDORID				0x1259

/*
 * Corega FEther CB-TXD device ID
 */
#define COREGA_DEVICEID_FETHERCBTXD		0xa117

/*
 * Corega FEtherII CB-TXD device ID
 */
#define COREGA_DEVICEID_FETHERIICBTXD		0xa11e

/*
 * Corega CG-LAPCIGT device ID
 */
#define COREGA_DEVICEID_CGLAPCIGT		0xc107

/*
 * Linksys vendor ID
 */
#define LINKSYS_VENDORID			0x1737

/*
 * Linksys EG1032 device ID
 */
#define LINKSYS_DEVICEID_EG1032			0x1032

/*
 * Linksys EG1032 rev 3 sub-device ID
 */
#define LINKSYS_SUBDEVICE_EG1032_REV3		0x0024

/*
 * Peppercon vendor ID
 */
#define PEPPERCON_VENDORID			0x1743

/*
 * Peppercon ROL-F device ID
 */
#define PEPPERCON_DEVICEID_ROLF			0x8139

/*
 * Planex Communications, Inc. vendor ID
 */
#define PLANEX_VENDORID				0x14ea

/*
 * Planex FNW-3603-TX device ID
 */
#define PLANEX_DEVICEID_FNW3603TX		0xab06

/*
 * Planex FNW-3800-TX device ID
 */
#define PLANEX_DEVICEID_FNW3800TX		0xab07

/*
 * LevelOne vendor ID
 */
#define LEVEL1_VENDORID				0x018A

/*
 * LevelOne FPC-0106TX devide ID
 */
#define LEVEL1_DEVICEID_FPC0106TX		0x0106

/*
 * Compaq vendor ID
 */
#define CP_VENDORID				0x021B

/*
 * Edimax vendor ID
 */
#define EDIMAX_VENDORID				0x13D1

/*
 * Edimax EP-4103DL cardbus device ID
 */
#define EDIMAX_DEVICEID_EP4103DL		0xAB06

/* US Robotics vendor ID */

#define USR_VENDORID		0x16EC

/* US Robotics 997902 device ID */

#define USR_DEVICEID_997902	0x0116

/*
 * PCI low memory base and low I/O base register, and
 * other PCI registers.
 */

#define RL_PCI_VENDOR_ID	0x00
#define RL_PCI_DEVICE_ID	0x02
#define RL_PCI_COMMAND		0x04
#define RL_PCI_STATUS		0x06
#define RL_PCI_CLASSCODE	0x09
#define RL_PCI_LATENCY_TIMER	0x0D
#define RL_PCI_HEADER_TYPE	0x0E
#define RL_PCI_LOIO		0x10
#define RL_PCI_LOMEM		0x14
#define RL_PCI_BIOSROM		0x30
#define RL_PCI_INTLINE		0x3C
#define RL_PCI_INTPIN		0x3D
#define RL_PCI_MINGNT		0x3E
#define RL_PCI_MINLAT		0x0F
#define RL_PCI_RESETOPT		0x48
#define RL_PCI_EEPROM_DATA	0x4C

#define RL_PCI_CAPID		0x50 /* 8 bits */
#define RL_PCI_NEXTPTR		0x51 /* 8 bits */
#define RL_PCI_PWRMGMTCAP	0x52 /* 16 bits */
#define RL_PCI_PWRMGMTCTRL	0x54 /* 16 bits */

#define RL_PSTATE_MASK		0x0003
#define RL_PSTATE_D0		0x0000
#define RL_PSTATE_D1		0x0002
#define RL_PSTATE_D2		0x0002
#define RL_PSTATE_D3		0x0003
#define RL_PME_EN		0x0010
#define RL_PME_STATUS		0x8000
