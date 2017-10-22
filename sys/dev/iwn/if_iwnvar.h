/*	$FreeBSD$	*/
/*	$OpenBSD: if_iwnvar.h,v 1.18 2010/04/30 16:06:46 damien Exp $	*/

/*-
 * Copyright (c) 2007, 2008
 *	Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2008 Sam Leffler, Errno Consulting
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

struct iwn_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint64_t	wr_tsft;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	int8_t		wr_dbm_antsignal;
	int8_t		wr_dbm_antnoise;
} __packed;

#define IWN_RX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_TSFT) |				\
	 (1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL) |			\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTNOISE))

struct iwn_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint8_t		wt_rate;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
} __packed;

#define IWN_TX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL))

struct iwn_dma_info {
	bus_dma_tag_t		tag;
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	bus_addr_t		paddr;
	caddr_t			vaddr;
	bus_size_t		size;
};

struct iwn_tx_data {
	bus_dmamap_t		map;
	bus_addr_t		cmd_paddr;
	bus_addr_t		scratch_paddr;
	struct mbuf		*m;
	struct ieee80211_node	*ni;
};

struct iwn_tx_ring {
	struct iwn_dma_info	desc_dma;
	struct iwn_dma_info	cmd_dma;
	struct iwn_tx_desc	*desc;
	struct iwn_tx_cmd	*cmd;
	struct iwn_tx_data	data[IWN_TX_RING_COUNT];
	bus_dma_tag_t		data_dmat;
	int			qid;
	int			queued;
	int			cur;
	int			read;
};

struct iwn_softc;

struct iwn_rx_data {
	struct mbuf	*m;
	bus_dmamap_t	map;
};

struct iwn_rx_ring {
	struct iwn_dma_info	desc_dma;
	struct iwn_dma_info	stat_dma;
	uint32_t		*desc;
	struct iwn_rx_status	*stat;
	struct iwn_rx_data	data[IWN_RX_RING_COUNT];
	bus_dma_tag_t		data_dmat;
	int			cur;
};

struct iwn_node {
	struct	ieee80211_node		ni;	/* must be the first */
	uint16_t			disable_tid;
	uint8_t				id;
	uint32_t			ridx[256];
	struct {
		uint64_t		bitmap;
		int			startidx;
		int			nframes;
	} agg[IEEE80211_TID_SIZE];
};

struct iwn_calib_state {
	uint8_t		state;
#define IWN_CALIB_STATE_INIT	0
#define IWN_CALIB_STATE_ASSOC	1
#define IWN_CALIB_STATE_RUN	2

	u_int		nbeacons;
	uint32_t	noise[3];
	uint32_t	rssi[3];
	uint32_t	ofdm_x1;
	uint32_t	ofdm_mrc_x1;
	uint32_t	ofdm_x4;
	uint32_t	ofdm_mrc_x4;
	uint32_t	cck_x4;
	uint32_t	cck_mrc_x4;
	uint32_t	bad_plcp_ofdm;
	uint32_t	fa_ofdm;
	uint32_t	bad_plcp_cck;
	uint32_t	fa_cck;
	uint32_t	low_fa;
	uint8_t		cck_state;
#define IWN_CCK_STATE_INIT	0
#define IWN_CCK_STATE_LOFA	1
#define IWN_CCK_STATE_HIFA	2

	uint8_t		noise_samples[20];
	u_int		cur_noise_sample;
	uint8_t		noise_ref;
	uint32_t	energy_samples[10];
	u_int		cur_energy_sample;
	uint32_t	energy_cck;
};

struct iwn_calib_info {
	uint8_t		*buf;
	u_int		len;
};

struct iwn_fw_part {
	const uint8_t	*text;
	uint32_t	textsz;
	const uint8_t	*data;
	uint32_t	datasz;
};

struct iwn_fw_info {
	const uint8_t		*data;
	size_t			size;
	struct iwn_fw_part	init;
	struct iwn_fw_part	main;
	struct iwn_fw_part	boot;
};

struct iwn_ops {
	int		(*load_firmware)(struct iwn_softc *);
	void		(*read_eeprom)(struct iwn_softc *);
	int		(*post_alive)(struct iwn_softc *);
	int		(*nic_config)(struct iwn_softc *);
	void		(*update_sched)(struct iwn_softc *, int, int, uint8_t,
			    uint16_t);
	int		(*get_temperature)(struct iwn_softc *);
	int		(*get_rssi)(struct iwn_softc *, struct iwn_rx_stat *);
	int		(*set_txpower)(struct iwn_softc *,
			    struct ieee80211_channel *, int);
	int		(*init_gains)(struct iwn_softc *);
	int		(*set_gains)(struct iwn_softc *);
	int		(*add_node)(struct iwn_softc *, struct iwn_node_info *,
			    int);
	void		(*tx_done)(struct iwn_softc *, struct iwn_rx_desc *,
			    struct iwn_rx_data *);
	void		(*ampdu_tx_start)(struct iwn_softc *,
			    struct ieee80211_node *, int, uint8_t, uint16_t);
	void		(*ampdu_tx_stop)(struct iwn_softc *, int, uint8_t,
			    uint16_t);
};

struct iwn_vap {
	struct ieee80211vap	iv_vap;
	uint8_t			iv_ridx;

	int			(*iv_newstate)(struct ieee80211vap *,
				    enum ieee80211_state, int);
};
#define	IWN_VAP(_vap)	((struct iwn_vap *)(_vap))

struct iwn_softc {
	device_t		sc_dev;

	struct ifnet		*sc_ifp;
	int			sc_debug;

	struct mtx		sc_mtx;

	u_int			sc_flags;
#define IWN_FLAG_HAS_OTPROM	(1 << 1)
#define IWN_FLAG_CALIB_DONE	(1 << 2)
#define IWN_FLAG_USE_ICT	(1 << 3)
#define IWN_FLAG_INTERNAL_PA	(1 << 4)
#define IWN_FLAG_HAS_11N	(1 << 6)
#define IWN_FLAG_ENH_SENS	(1 << 7)
#define IWN_FLAG_ADV_BTCOEX	(1 << 8)

	uint8_t 		hw_type;

	struct iwn_ops		ops;
	const char		*fwname;
	const struct iwn_sensitivity_limits
				*limits;
	int			ntxqs;
	int			firstaggqueue;
	int			ndmachnls;
	uint8_t			broadcast_id;
	int			rxonsz;
	int			schedsz;
	uint32_t		fw_text_maxsz;
	uint32_t		fw_data_maxsz;
	uint32_t		fwsz;
	bus_size_t		sched_txfact_addr;
	uint32_t		reset_noise_gain;
	uint32_t		noise_gain;

	/* TX scheduler rings. */
	struct iwn_dma_info	sched_dma;
	uint16_t		*sched;
	uint32_t		sched_base;

	/* "Keep Warm" page. */
	struct iwn_dma_info	kw_dma;

	/* Firmware image. */
	const struct firmware	*fw_fp;

	/* Firmware DMA transfer. */
	struct iwn_dma_info	fw_dma;

	/* ICT table. */
	struct iwn_dma_info	ict_dma;
	uint32_t		*ict;
	int			ict_cur;

	/* TX/RX rings. */
	struct iwn_tx_ring	txq[IWN5000_NTXQUEUES];
	struct iwn_rx_ring	rxq;

	int			mem_rid;
	struct resource		*mem;
	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;
	int			irq_rid;
	struct resource		*irq;
	void 			*sc_ih;
	bus_size_t		sc_sz;
	int			sc_cap_off;	/* PCIe Capabilities. */

	/* Tasks used by the driver */
	struct task		sc_reinit_task;
	struct task		sc_radioon_task;
	struct task		sc_radiooff_task;

	struct callout		calib_to;
	int			calib_cnt;
	struct iwn_calib_state	calib;
	struct callout		watchdog_to;

	struct iwn_fw_info	fw;
	struct iwn_calib_info	calibcmd[5];
	uint32_t		errptr;

	struct iwn_rx_stat	last_rx_stat;
	int			last_rx_valid;
	struct iwn_ucode_info	ucode_info;
	struct iwn_rxon		rxon;
	uint32_t		rawtemp;
	int			temp;
	int			noise;
	uint32_t		qfullmsk;

	uint32_t		prom_base;
	struct iwn4965_eeprom_band
				bands[IWN_NBANDS];
	struct iwn_eeprom_chan	eeprom_channels[IWN_NBANDS][IWN_MAX_CHAN_PER_BAND];
	uint16_t		rfcfg;
	uint8_t			calib_ver;
	char			eeprom_domain[4];
	uint32_t		eeprom_crystal;
	int16_t			eeprom_temp;
	int16_t			eeprom_voltage;
	int8_t			maxpwr2GHz;
	int8_t			maxpwr5GHz;
	int8_t			maxpwr[IEEE80211_CHAN_MAX];

	int32_t			temp_off;
	uint32_t		int_mask;
	uint8_t			ntxchains;
	uint8_t			nrxchains;
	uint8_t			txchainmask;
	uint8_t			rxchainmask;
	uint8_t			chainmask;

	int			sc_tx_timer;

	struct ieee80211_tx_ampdu *qid2tap[IWN5000_NTXQUEUES];

	int			(*sc_ampdu_rx_start)(struct ieee80211_node *,
				    struct ieee80211_rx_ampdu *, int, int, int);
	void			(*sc_ampdu_rx_stop)(struct ieee80211_node *,
				    struct ieee80211_rx_ampdu *);
	int			(*sc_addba_request)(struct ieee80211_node *,
				    struct ieee80211_tx_ampdu *, int, int, int);
	int			(*sc_addba_response)(struct ieee80211_node *,
				    struct ieee80211_tx_ampdu *, int, int, int);
	void			(*sc_addba_stop)(struct ieee80211_node *,
				    struct ieee80211_tx_ampdu *);


	struct iwn_rx_radiotap_header sc_rxtap;
	struct iwn_tx_radiotap_header sc_txtap;
};

#define IWN_LOCK_INIT(_sc) \
	mtx_init(&(_sc)->sc_mtx, device_get_nameunit((_sc)->sc_dev), \
	    MTX_NETWORK_LOCK, MTX_DEF)
#define IWN_LOCK(_sc)			mtx_lock(&(_sc)->sc_mtx)
#define IWN_LOCK_ASSERT(_sc)		mtx_assert(&(_sc)->sc_mtx, MA_OWNED)
#define IWN_UNLOCK(_sc)			mtx_unlock(&(_sc)->sc_mtx)
#define IWN_LOCK_DESTROY(_sc)		mtx_destroy(&(_sc)->sc_mtx)
