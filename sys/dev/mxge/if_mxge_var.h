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

$FreeBSD: release/7.0.0/sys/dev/mxge/if_mxge_var.h 172162 2007-09-13 21:29:02Z gallatin $

***************************************************************************/

#define MXGE_ETH_STOPPED 0
#define MXGE_ETH_STOPPING 1
#define MXGE_ETH_STARTING 2
#define MXGE_ETH_RUNNING 3
#define MXGE_ETH_OPEN_FAILED 4

#define MXGE_FW_OFFSET 1024*1024
#define MXGE_EEPROM_STRINGS_SIZE 256
#define MXGE_MAX_SEND_DESC 128

typedef struct {
	void *addr;
	bus_addr_t bus_addr;
	bus_dma_tag_t dmat;
	bus_dmamap_t map;
} mxge_dma_t;


typedef struct {
	mcp_slot_t *entry;
	mxge_dma_t dma;
	int cnt;
	int idx;
	int mask;
} mxge_rx_done_t;

typedef struct
{
  uint32_t data0;
  uint32_t data1;
  uint32_t data2;
} mxge_cmd_t;

struct mxge_rx_buffer_state {
	struct mbuf *m;
	bus_dmamap_t map;
};

struct mxge_tx_buffer_state {
	struct mbuf *m;
	bus_dmamap_t map;
	int flag;
};

typedef struct
{
	volatile mcp_kreq_ether_recv_t *lanai;	/* lanai ptr for recv ring */
	mcp_kreq_ether_recv_t *shadow;	/* host shadow of recv ring */
	struct mxge_rx_buffer_state *info;
	bus_dma_tag_t dmat;
	bus_dmamap_t extra_map;
	int cnt;
	int nbufs;
	int cl_size;
	int alloc_fail;
	int mask;			/* number of rx slots -1 */
} mxge_rx_buf_t;

typedef struct
{
	volatile mcp_kreq_ether_send_t *lanai;	/* lanai ptr for sendq	*/
	mcp_kreq_ether_send_t *req_list;	/* host shadow of sendq */
	char *req_bytes;
	bus_dma_segment_t *seg_list;
	struct mxge_tx_buffer_state *info;
	bus_dma_tag_t dmat;
	int req;			/* transmits submitted	*/
	int mask;			/* number of transmit slots -1 */
	int done;			/* transmits completed	*/
	int pkt_done;			/* packets completed */
	int boundary;			/* boundary transmits cannot cross*/
	int max_desc;			/* max descriptors per xmit */
	int stall;			/* #times hw queue exhausted */
	int wake;			/* #times irq re-enabled xmit */
	int watchdog_req;		/* cache of req */
	int watchdog_done;		/* cache of done */
	int watchdog_rx_pause;		/* cache of pause rq recvd */
} mxge_tx_buf_t;

struct lro_entry;
struct lro_entry
{
	SLIST_ENTRY(lro_entry) next;
	struct mbuf  	*m_head;
	struct mbuf	*m_tail;
	int		timestamp;
	struct ip	*ip;
	uint32_t	tsval;
	uint32_t	tsecr;
	uint32_t	source_ip;
	uint32_t	dest_ip;
	uint32_t	next_seq;
	uint32_t	ack_seq;
	uint32_t	len;
	uint32_t	data_csum;
	uint16_t	window;
	uint16_t	source_port;
	uint16_t	dest_port;
	uint16_t	append_cnt;
	uint16_t	mss;
	
};
SLIST_HEAD(lro_head, lro_entry);

typedef struct {
	struct ifnet* ifp;
	struct mtx tx_mtx;
	int csum_flag;			/* rx_csums? 		*/
	mxge_tx_buf_t tx;		/* transmit ring 	*/
	mxge_rx_buf_t rx_small;
	mxge_rx_buf_t rx_big;
	mxge_rx_done_t rx_done;
	mcp_irq_data_t *fw_stats;
	bus_dma_tag_t	parent_dmat;
	volatile uint8_t *sram;
	struct lro_head lro_active;
	struct lro_head lro_free;
	int lro_queued;
	int lro_flushed;
	int lro_bad_csum;
	int lro_cnt;
	int sram_size;
	volatile uint32_t *irq_deassert;
	volatile uint32_t *irq_claim;
	mcp_cmd_response_t *cmd;
	mxge_dma_t cmd_dma;
	mxge_dma_t zeropad_dma;
	mxge_dma_t fw_stats_dma;
	struct pci_dev *pdev;
	int msi_enabled;
	int link_state;
	unsigned int rdma_tags_available;
	int intr_coal_delay;
	volatile uint32_t *intr_coal_delay_ptr;
	int wc;
	struct mtx cmd_mtx;
	struct mtx driver_mtx;
	int wake_queue;
	int stop_queue;
	int down_cnt;
	int watchdog_resets;
	int tx_defragged;
	int pause;
	struct resource *mem_res;
	struct resource *irq_res;
	void *ih; 
	char *fw_name;
	char eeprom_strings[MXGE_EEPROM_STRINGS_SIZE];
	char fw_version[128];
	int fw_ver_major;
	int fw_ver_minor;
	int fw_ver_tiny;
	int adopted_rx_filter_bug;
	device_t dev;
	struct ifmedia media;
	int read_dma;
	int write_dma;
	int read_write_dma;
	int fw_multicast_support;
	int link_width;
	int max_mtu;
	int tx_defrag;
	int media_flags;
	int need_media_probe;
	mxge_dma_t dmabench_dma;
	struct callout co_hdl;
	char *mac_addr_string;
	uint8_t	mac_addr[6];		/* eeprom mac address */
	char product_code_string[64];
	char serial_number_string[64];
	char scratch[256];
	char tx_mtx_name[16];
	char cmd_mtx_name[16];
	char driver_mtx_name[16];
} mxge_softc_t;

#define MXGE_PCI_VENDOR_MYRICOM 	0x14c1
#define MXGE_PCI_DEVICE_Z8E 	0x0008
#define MXGE_PCI_DEVICE_Z8E_9 	0x0009
#define MXGE_XFP_COMPLIANCE_BYTE	131

#define MXGE_HIGHPART_TO_U32(X) \
(sizeof (X) == 8) ? ((uint32_t)((uint64_t)(X) >> 32)) : (0)
#define MXGE_LOWPART_TO_U32(X) ((uint32_t)(X))

struct mxge_media_type
{
	int flag;
	uint8_t bitmask;
	char *name;
};

/* implement our own memory barriers, since bus_space_barrier
   cannot handle write-combining regions */

#if defined (__GNUC__)
  #if #cpu(i386) || defined __i386 || defined i386 || defined __i386__ || #cpu(x86_64) || defined __x86_64__
    #define mb()  __asm__ __volatile__ ("sfence;": : :"memory")
  #elif #cpu(sparc64) || defined sparc64 || defined __sparcv9 
    #define mb()  __asm__ __volatile__ ("membar #MemIssue": : :"memory")
  #elif #cpu(sparc) || defined sparc || defined __sparc__
    #define mb()  __asm__ __volatile__ ("stbar;": : :"memory")
  #else
    #define mb() 	/* XXX just to make this compile */
  #endif
#else
  #error "unknown compiler"
#endif

static inline void
mxge_pio_copy(volatile void *to_v, void *from_v, size_t size)
{
  register volatile uintptr_t *to;
  volatile uintptr_t *from;
  size_t i;

  to = (volatile uintptr_t *) to_v;
  from = from_v;
  for (i = (size / sizeof (uintptr_t)); i; i--) {
	  *to = *from;
	  to++;
	  from++;
  }

}

void mxge_lro_flush(mxge_softc_t *mgp, struct lro_entry *lro);
int mxge_lro_rx(mxge_softc_t *mgp, struct mbuf *m_head, uint32_t csum);
		


/*
  This file uses Myri10GE driver indentation.

  Local Variables:
  c-file-style:"linux"
  tab-width:8
  End:
*/
