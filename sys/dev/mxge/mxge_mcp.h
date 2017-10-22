/*******************************************************************************

Copyright (c) 2006-2007, Myricom Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Myricom Inc, nor the names of its
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

$FreeBSD: release/7.0.0/sys/dev/mxge/mxge_mcp.h 171917 2007-08-22 13:22:12Z gallatin $
***************************************************************************/

#ifndef _myri10ge_mcp_h
#define _myri10ge_mcp_h

#define MXGEFW_VERSION_MAJOR	1
#define MXGEFW_VERSION_MINOR	4

#ifdef MXGEFW
typedef signed char          int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long long    int64_t;
typedef unsigned char       uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
#endif

/* 8 Bytes */
struct mcp_dma_addr {
  uint32_t high;
  uint32_t low;
};
typedef struct mcp_dma_addr mcp_dma_addr_t;

/* 4 Bytes.  8 Bytes for NDIS drivers. */
struct mcp_slot {
#ifdef MXGEFW_NDIS
  /* Place at the top so it gets written before length.
   * The driver polls length.
   */
  uint32_t hash;
#endif
  uint16_t checksum;
  uint16_t length;
};
typedef struct mcp_slot mcp_slot_t;

#ifdef MXGEFW_NDIS
/* Two bits of length in mcp_slot are used to indicate hash type. */
#define MXGEFW_RSS_HASH_NULL (0 << 14) /* bit 15:14 = 00 */
#define MXGEFW_RSS_HASH_IPV4 (1 << 14) /* bit 15:14 = 01 */
#define MXGEFW_RSS_HASH_TCP_IPV4 (2 << 14) /* bit 15:14 = 10 */
#define MXGEFW_RSS_HASH_MASK (3 << 14) /* bit 15:14 = 11 */
#endif

/* 64 Bytes */
struct mcp_cmd {
  uint32_t cmd;
  uint32_t data0;	/* will be low portion if data > 32 bits */
  /* 8 */
  uint32_t data1;	/* will be high portion if data > 32 bits */
  uint32_t data2;	/* currently unused.. */
  /* 16 */
  struct mcp_dma_addr response_addr;
  /* 24 */
  uint8_t pad[40];
};
typedef struct mcp_cmd mcp_cmd_t;

/* 8 Bytes */
struct mcp_cmd_response {
  uint32_t data;
  uint32_t result;
};
typedef struct mcp_cmd_response mcp_cmd_response_t;



/* 
   flags used in mcp_kreq_ether_send_t:

   The SMALL flag is only needed in the first segment. It is raised
   for packets that are total less or equal 512 bytes.

   The CKSUM flag must be set in all segments.

   The PADDED flags is set if the packet needs to be padded, and it
   must be set for all segments.

   The  MXGEFW_FLAGS_ALIGN_ODD must be set if the cumulative
   length of all previous segments was odd.
*/


#define MXGEFW_FLAGS_SMALL      0x1
#define MXGEFW_FLAGS_TSO_HDR    0x1
#define MXGEFW_FLAGS_FIRST      0x2
#define MXGEFW_FLAGS_ALIGN_ODD  0x4
#define MXGEFW_FLAGS_CKSUM      0x8
#define MXGEFW_FLAGS_TSO_LAST   0x8
#define MXGEFW_FLAGS_NO_TSO     0x10
#define MXGEFW_FLAGS_TSO_CHOP   0x10
#define MXGEFW_FLAGS_TSO_PLD    0x20

#define MXGEFW_SEND_SMALL_SIZE  1520
#define MXGEFW_MAX_MTU          9400

union mcp_pso_or_cumlen {
  uint16_t pseudo_hdr_offset;
  uint16_t cum_len;
};
typedef union mcp_pso_or_cumlen mcp_pso_or_cumlen_t;

#define	MXGEFW_MAX_SEND_DESC 12
#define MXGEFW_PAD	    2

/* 16 Bytes */
struct mcp_kreq_ether_send {
  uint32_t addr_high;
  uint32_t addr_low;
  uint16_t pseudo_hdr_offset;
  uint16_t length;
  uint8_t  pad;
  uint8_t  rdma_count;
  uint8_t  cksum_offset; 	/* where to start computing cksum */
  uint8_t  flags;	       	/* as defined above */
};
typedef struct mcp_kreq_ether_send mcp_kreq_ether_send_t;

/* 8 Bytes */
struct mcp_kreq_ether_recv {
  uint32_t addr_high;
  uint32_t addr_low;
};
typedef struct mcp_kreq_ether_recv mcp_kreq_ether_recv_t;


/* Commands */

#define	MXGEFW_BOOT_HANDOFF	0xfc0000
#define	MXGEFW_BOOT_DUMMY_RDMA	0xfc01c0

#define	MXGEFW_ETH_CMD		0xf80000
#define	MXGEFW_ETH_SEND_4	0x200000
#define	MXGEFW_ETH_SEND_1	0x240000
#define	MXGEFW_ETH_SEND_2	0x280000
#define	MXGEFW_ETH_SEND_3	0x2c0000
#define	MXGEFW_ETH_RECV_SMALL	0x300000
#define	MXGEFW_ETH_RECV_BIG	0x340000

#define	MXGEFW_ETH_SEND(n)		(0x200000 + (((n) & 0x03) * 0x40000))
#define	MXGEFW_ETH_SEND_OFFSET(n)	(MXGEFW_ETH_SEND(n) - MXGEFW_ETH_SEND_4)

enum myri10ge_mcp_cmd_type {
  MXGEFW_CMD_NONE = 0,
  /* Reset the mcp, it is left in a safe state, waiting
     for the driver to set all its parameters */
  MXGEFW_CMD_RESET,

  /* get the version number of the current firmware..
     (may be available in the eeprom strings..? */
  MXGEFW_GET_MCP_VERSION,


  /* Parameters which must be set by the driver before it can
     issue MXGEFW_CMD_ETHERNET_UP. They persist until the next
     MXGEFW_CMD_RESET is issued */

  MXGEFW_CMD_SET_INTRQ_DMA,
  MXGEFW_CMD_SET_BIG_BUFFER_SIZE,	/* in bytes, power of 2 */
  MXGEFW_CMD_SET_SMALL_BUFFER_SIZE,	/* in bytes */
  

  /* Parameters which refer to lanai SRAM addresses where the 
     driver must issue PIO writes for various things */

  MXGEFW_CMD_GET_SEND_OFFSET,
  MXGEFW_CMD_GET_SMALL_RX_OFFSET,
  MXGEFW_CMD_GET_BIG_RX_OFFSET,
  MXGEFW_CMD_GET_IRQ_ACK_OFFSET,
  MXGEFW_CMD_GET_IRQ_DEASSERT_OFFSET,

  /* Parameters which refer to rings stored on the MCP,
     and whose size is controlled by the mcp */

  MXGEFW_CMD_GET_SEND_RING_SIZE,	/* in bytes */
  MXGEFW_CMD_GET_RX_RING_SIZE,		/* in bytes */

  /* Parameters which refer to rings stored in the host,
     and whose size is controlled by the host.  Note that
     all must be physically contiguous and must contain 
     a power of 2 number of entries.  */

  MXGEFW_CMD_SET_INTRQ_SIZE, 	/* in bytes */

  /* command to bring ethernet interface up.  Above parameters
     (plus mtu & mac address) must have been exchanged prior
     to issuing this command  */
  MXGEFW_CMD_ETHERNET_UP,

  /* command to bring ethernet interface down.  No further sends
     or receives may be processed until an MXGEFW_CMD_ETHERNET_UP
     is issued, and all interrupt queues must be flushed prior
     to ack'ing this command */

  MXGEFW_CMD_ETHERNET_DOWN,

  /* commands the driver may issue live, without resetting
     the nic.  Note that increasing the mtu "live" should
     only be done if the driver has already supplied buffers
     sufficiently large to handle the new mtu.  Decreasing
     the mtu live is safe */

  MXGEFW_CMD_SET_MTU,
  MXGEFW_CMD_GET_INTR_COAL_DELAY_OFFSET,  /* in microseconds */
  MXGEFW_CMD_SET_STATS_INTERVAL,   /* in microseconds */
  MXGEFW_CMD_SET_STATS_DMA_OBSOLETE, /* replaced by SET_STATS_DMA_V2 */

  MXGEFW_ENABLE_PROMISC,
  MXGEFW_DISABLE_PROMISC,
  MXGEFW_SET_MAC_ADDRESS,

  MXGEFW_ENABLE_FLOW_CONTROL,
  MXGEFW_DISABLE_FLOW_CONTROL,

  /* do a DMA test
     data0,data1 = DMA address
     data2       = RDMA length (MSH), WDMA length (LSH)
     command return data = repetitions (MSH), 0.5-ms ticks (LSH)
  */
  MXGEFW_DMA_TEST,

  MXGEFW_ENABLE_ALLMULTI,
  MXGEFW_DISABLE_ALLMULTI,

  /* returns MXGEFW_CMD_ERROR_MULTICAST
     if there is no room in the cache
     data0,MSH(data1) = multicast group address */
  MXGEFW_JOIN_MULTICAST_GROUP,
  /* returns MXGEFW_CMD_ERROR_MULTICAST
     if the address is not in the cache,
     or is equal to FF-FF-FF-FF-FF-FF
     data0,MSH(data1) = multicast group address */
  MXGEFW_LEAVE_MULTICAST_GROUP,
  MXGEFW_LEAVE_ALL_MULTICAST_GROUPS,

  MXGEFW_CMD_SET_STATS_DMA_V2,
  /* data0, data1 = bus addr,
     data2 = sizeof(struct mcp_irq_data) from driver point of view, allows
     adding new stuff to mcp_irq_data without changing the ABI */

  MXGEFW_CMD_UNALIGNED_TEST,
  /* same than DMA_TEST (same args) but abort with UNALIGNED on unaligned
     chipset */

  MXGEFW_CMD_UNALIGNED_STATUS,
  /* return data = boolean, true if the chipset is known to be unaligned */

  MXGEFW_CMD_ALWAYS_USE_N_BIG_BUFFERS,
  /* data0 = number of big buffers to use.  It must be 0 or a power of 2.
   * 0 indicates that the NIC consumes as many buffers as they are required
   * for packet. This is the default behavior.
   * A power of 2 number indicates that the NIC always uses the specified
   * number of buffers for each big receive packet.
   * It is up to the driver to ensure that this value is big enough for
   * the NIC to be able to receive maximum-sized packets.
   */

  MXGEFW_CMD_GET_MAX_RSS_QUEUES,
  MXGEFW_CMD_ENABLE_RSS_QUEUES,
  /* data0 = number of slices n (0, 1, ..., n-1) to enable
   * data1 = interrupt mode. 0=share one INTx/MSI, 1=use one MSI-X per queue.
   * If all queues share one interrupt, the driver must have set
   * RSS_SHARED_INTERRUPT_DMA before enabling queues.
   */
  MXGEFW_CMD_GET_RSS_SHARED_INTERRUPT_MASK_OFFSET,
  MXGEFW_CMD_SET_RSS_SHARED_INTERRUPT_DMA,
  /* data0, data1 = bus address lsw, msw */
  MXGEFW_CMD_GET_RSS_TABLE_OFFSET,
  /* get the offset of the indirection table */
  MXGEFW_CMD_SET_RSS_TABLE_SIZE,
  /* set the size of the indirection table */
  MXGEFW_CMD_GET_RSS_KEY_OFFSET,
  /* get the offset of the secret key */
  MXGEFW_CMD_RSS_KEY_UPDATED,
  /* tell nic that the secret key's been updated */
  MXGEFW_CMD_SET_RSS_ENABLE,
  /* data0 = enable/disable rss
   * 0: disable rss.  nic does not distribute receive packets.
   * 1: enable rss.  nic distributes receive packets among queues.
   * data1 = hash type
   * 1: IPV4
   * 2: TCP_IPV4
   * 3: IPV4 | TCP_IPV4
   */

  MXGEFW_CMD_GET_MAX_TSO6_HDR_SIZE,
  /* Return data = the max. size of the entire headers of a IPv6 TSO packet.
   * If the header size of a IPv6 TSO packet is larger than the specified
   * value, then the driver must not use TSO.
   * This size restriction only applies to IPv6 TSO.
   * For IPv4 TSO, the maximum size of the headers is fixed, and the NIC
   * always has enough header buffer to store maximum-sized headers.
   */
  
  MXGEFW_CMD_SET_TSO_MODE,
  /* data0 = TSO mode.
   * 0: Linux/FreeBSD style (NIC default)
   * 1: NDIS/NetBSD style
   */

  MXGEFW_CMD_MDIO_READ,
  /* data0 = dev_addr (PMA/PMD or PCS ...), data1 = register/addr */
  MXGEFW_CMD_MDIO_WRITE,
  /* data0 = dev_addr,  data1 = register/addr, data2 = value  */

  MXGEFW_CMD_XFP_I2C_READ,
  /* Starts to get a fresh copy of one byte or of the whole xfp i2c table, the
   * obtained data is cached inside the xaui-xfi chip :
   *   data0 : "all" flag : 0 => get one byte, 1=> get 256 bytes, 
   *   data1 : if (data0 == 0): index of byte to refresh [ not used otherwise ]
   * The operation might take ~1ms for a single byte or ~65ms when refreshing all 256 bytes
   * During the i2c operation,  MXGEFW_CMD_XFP_I2C_READ or MXGEFW_CMD_XFP_BYTE attempts
   *  will return MXGEFW_CMD_ERROR_BUSY
   */
  MXGEFW_CMD_XFP_BYTE
  /* Return the last obtained copy of a given byte in the xfp i2c table
   * (copy cached during the last relevant MXGEFW_CMD_XFP_I2C_READ)
   *   data0 : index of the desired table entry
   *  Return data = the byte stored at the requested index in the table
   */
};
typedef enum myri10ge_mcp_cmd_type myri10ge_mcp_cmd_type_t;


enum myri10ge_mcp_cmd_status {
  MXGEFW_CMD_OK = 0,
  MXGEFW_CMD_UNKNOWN,
  MXGEFW_CMD_ERROR_RANGE,
  MXGEFW_CMD_ERROR_BUSY,
  MXGEFW_CMD_ERROR_EMPTY,
  MXGEFW_CMD_ERROR_CLOSED,
  MXGEFW_CMD_ERROR_HASH_ERROR,
  MXGEFW_CMD_ERROR_BAD_PORT,
  MXGEFW_CMD_ERROR_RESOURCES,
  MXGEFW_CMD_ERROR_MULTICAST,
  MXGEFW_CMD_ERROR_UNALIGNED,
  MXGEFW_CMD_ERROR_NO_MDIO,
  MXGEFW_CMD_ERROR_XFP_FAILURE,
  MXGEFW_CMD_ERROR_XFP_ABSENT
};
typedef enum myri10ge_mcp_cmd_status myri10ge_mcp_cmd_status_t;


#define MXGEFW_OLD_IRQ_DATA_LEN 40

struct mcp_irq_data {
  /* add new counters at the beginning */
  uint32_t future_use[1];
  uint32_t dropped_pause;
  uint32_t dropped_unicast_filtered;
  uint32_t dropped_bad_crc32;
  uint32_t dropped_bad_phy;
  uint32_t dropped_multicast_filtered;
/* 40 Bytes */
  uint32_t send_done_count;

#define MXGEFW_LINK_DOWN 0
#define MXGEFW_LINK_UP 1
#define MXGEFW_LINK_MYRINET 2
#define MXGEFW_LINK_UNKNOWN 3
  uint32_t link_up;
  uint32_t dropped_link_overflow;
  uint32_t dropped_link_error_or_filtered;
  uint32_t dropped_runt;
  uint32_t dropped_overrun;
  uint32_t dropped_no_small_buffer;
  uint32_t dropped_no_big_buffer;
  uint32_t rdma_tags_available;

  uint8_t tx_stopped;
  uint8_t link_down;
  uint8_t stats_updated;
  uint8_t valid;
};
typedef struct mcp_irq_data mcp_irq_data_t;

#ifdef MXGEFW_NDIS
struct mcp_rss_shared_interrupt {
  uint8_t pad[2];
  uint8_t queue;
  uint8_t valid;
};
#endif

#endif /* _myri10ge_mcp_h */
