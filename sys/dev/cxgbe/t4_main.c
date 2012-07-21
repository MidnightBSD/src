/*-
 * Copyright (c) 2011 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: Navdeep Parhar <np@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/priv.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/pciio.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>
#include <sys/firmware.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/if_vlan_var.h>

#include "common/common.h"
#include "common/t4_msg.h"
#include "common/t4_regs.h"
#include "common/t4_regs_values.h"
#include "t4_ioctl.h"
#include "t4_l2t.h"

/* T4 bus driver interface */
static int t4_probe(device_t);
static int t4_attach(device_t);
static int t4_detach(device_t);
static device_method_t t4_methods[] = {
	DEVMETHOD(device_probe,		t4_probe),
	DEVMETHOD(device_attach,	t4_attach),
	DEVMETHOD(device_detach,	t4_detach),

	DEVMETHOD_END
};
static driver_t t4_driver = {
	"t4nex",
	t4_methods,
	sizeof(struct adapter)
};


/* T4 port (cxgbe) interface */
static int cxgbe_probe(device_t);
static int cxgbe_attach(device_t);
static int cxgbe_detach(device_t);
static device_method_t cxgbe_methods[] = {
	DEVMETHOD(device_probe,		cxgbe_probe),
	DEVMETHOD(device_attach,	cxgbe_attach),
	DEVMETHOD(device_detach,	cxgbe_detach),
	{ 0, 0 }
};
static driver_t cxgbe_driver = {
	"cxgbe",
	cxgbe_methods,
	sizeof(struct port_info)
};

static d_ioctl_t t4_ioctl;
static d_open_t t4_open;
static d_close_t t4_close;

static struct cdevsw t4_cdevsw = {
       .d_version = D_VERSION,
       .d_flags = 0,
       .d_open = t4_open,
       .d_close = t4_close,
       .d_ioctl = t4_ioctl,
       .d_name = "t4nex",
};

/* ifnet + media interface */
static void cxgbe_init(void *);
static int cxgbe_ioctl(struct ifnet *, unsigned long, caddr_t);
static int cxgbe_transmit(struct ifnet *, struct mbuf *);
static void cxgbe_qflush(struct ifnet *);
static int cxgbe_media_change(struct ifnet *);
static void cxgbe_media_status(struct ifnet *, struct ifmediareq *);

MALLOC_DEFINE(M_CXGBE, "cxgbe", "Chelsio T4 Ethernet driver and services");

/*
 * Correct lock order when you need to acquire multiple locks is t4_list_lock,
 * then ADAPTER_LOCK, then t4_uld_list_lock.
 */
static struct mtx t4_list_lock;
static SLIST_HEAD(, adapter) t4_list;
#ifdef TCP_OFFLOAD
static struct mtx t4_uld_list_lock;
static SLIST_HEAD(, uld_info) t4_uld_list;
#endif

/*
 * Tunables.  See tweak_tunables() too.
 */

/*
 * Number of queues for tx and rx, 10G and 1G, NIC and offload.
 */
#define NTXQ_10G 16
static int t4_ntxq10g = -1;
TUNABLE_INT("hw.cxgbe.ntxq10g", &t4_ntxq10g);

#define NRXQ_10G 8
static int t4_nrxq10g = -1;
TUNABLE_INT("hw.cxgbe.nrxq10g", &t4_nrxq10g);

#define NTXQ_1G 4
static int t4_ntxq1g = -1;
TUNABLE_INT("hw.cxgbe.ntxq1g", &t4_ntxq1g);

#define NRXQ_1G 2
static int t4_nrxq1g = -1;
TUNABLE_INT("hw.cxgbe.nrxq1g", &t4_nrxq1g);

#ifdef TCP_OFFLOAD
#define NOFLDTXQ_10G 8
static int t4_nofldtxq10g = -1;
TUNABLE_INT("hw.cxgbe.nofldtxq10g", &t4_nofldtxq10g);

#define NOFLDRXQ_10G 2
static int t4_nofldrxq10g = -1;
TUNABLE_INT("hw.cxgbe.nofldrxq10g", &t4_nofldrxq10g);

#define NOFLDTXQ_1G 2
static int t4_nofldtxq1g = -1;
TUNABLE_INT("hw.cxgbe.nofldtxq1g", &t4_nofldtxq1g);

#define NOFLDRXQ_1G 1
static int t4_nofldrxq1g = -1;
TUNABLE_INT("hw.cxgbe.nofldrxq1g", &t4_nofldrxq1g);
#endif

/*
 * Holdoff parameters for 10G and 1G ports.
 */
#define TMR_IDX_10G 1
static int t4_tmr_idx_10g = TMR_IDX_10G;
TUNABLE_INT("hw.cxgbe.holdoff_timer_idx_10G", &t4_tmr_idx_10g);

#define PKTC_IDX_10G (-1)
static int t4_pktc_idx_10g = PKTC_IDX_10G;
TUNABLE_INT("hw.cxgbe.holdoff_pktc_idx_10G", &t4_pktc_idx_10g);

#define TMR_IDX_1G 1
static int t4_tmr_idx_1g = TMR_IDX_1G;
TUNABLE_INT("hw.cxgbe.holdoff_timer_idx_1G", &t4_tmr_idx_1g);

#define PKTC_IDX_1G (-1)
static int t4_pktc_idx_1g = PKTC_IDX_1G;
TUNABLE_INT("hw.cxgbe.holdoff_pktc_idx_1G", &t4_pktc_idx_1g);

/*
 * Size (# of entries) of each tx and rx queue.
 */
static unsigned int t4_qsize_txq = TX_EQ_QSIZE;
TUNABLE_INT("hw.cxgbe.qsize_txq", &t4_qsize_txq);

static unsigned int t4_qsize_rxq = RX_IQ_QSIZE;
TUNABLE_INT("hw.cxgbe.qsize_rxq", &t4_qsize_rxq);

/*
 * Interrupt types allowed (bits 0, 1, 2 = INTx, MSI, MSI-X respectively).
 */
static int t4_intr_types = INTR_MSIX | INTR_MSI | INTR_INTX;
TUNABLE_INT("hw.cxgbe.interrupt_types", &t4_intr_types);

/*
 * Configuration file.
 */
static char t4_cfg_file[32] = "default";
TUNABLE_STR("hw.cxgbe.config_file", t4_cfg_file, sizeof(t4_cfg_file));

/*
 * ASIC features that will be used.  Disable the ones you don't want so that the
 * chip resources aren't wasted on features that will not be used.
 */
static int t4_linkcaps_allowed = 0;	/* No DCBX, PPP, etc. by default */
TUNABLE_INT("hw.cxgbe.linkcaps_allowed", &t4_linkcaps_allowed);

static int t4_niccaps_allowed = FW_CAPS_CONFIG_NIC;
TUNABLE_INT("hw.cxgbe.niccaps_allowed", &t4_niccaps_allowed);

static int t4_toecaps_allowed = -1;
TUNABLE_INT("hw.cxgbe.toecaps_allowed", &t4_toecaps_allowed);

static int t4_rdmacaps_allowed = 0;
TUNABLE_INT("hw.cxgbe.rdmacaps_allowed", &t4_rdmacaps_allowed);

static int t4_iscsicaps_allowed = 0;
TUNABLE_INT("hw.cxgbe.iscsicaps_allowed", &t4_iscsicaps_allowed);

static int t4_fcoecaps_allowed = 0;
TUNABLE_INT("hw.cxgbe.fcoecaps_allowed", &t4_fcoecaps_allowed);

struct intrs_and_queues {
	int intr_type;		/* INTx, MSI, or MSI-X */
	int nirq;		/* Number of vectors */
	int intr_flags;
	int ntxq10g;		/* # of NIC txq's for each 10G port */
	int nrxq10g;		/* # of NIC rxq's for each 10G port */
	int ntxq1g;		/* # of NIC txq's for each 1G port */
	int nrxq1g;		/* # of NIC rxq's for each 1G port */
#ifdef TCP_OFFLOAD
	int nofldtxq10g;	/* # of TOE txq's for each 10G port */
	int nofldrxq10g;	/* # of TOE rxq's for each 10G port */
	int nofldtxq1g;		/* # of TOE txq's for each 1G port */
	int nofldrxq1g;		/* # of TOE rxq's for each 1G port */
#endif
};

struct filter_entry {
        uint32_t valid:1;	/* filter allocated and valid */
        uint32_t locked:1;	/* filter is administratively locked */
        uint32_t pending:1;	/* filter action is pending firmware reply */
	uint32_t smtidx:8;	/* Source MAC Table index for smac */
	struct l2t_entry *l2t;	/* Layer Two Table entry for dmac */

        struct t4_filter_specification fs;
};

enum {
	XGMAC_MTU	= (1 << 0),
	XGMAC_PROMISC	= (1 << 1),
	XGMAC_ALLMULTI	= (1 << 2),
	XGMAC_VLANEX	= (1 << 3),
	XGMAC_UCADDR	= (1 << 4),
	XGMAC_MCADDRS	= (1 << 5),

	XGMAC_ALL	= 0xffff
};

static int map_bars(struct adapter *);
static void setup_memwin(struct adapter *);
static int cfg_itype_and_nqueues(struct adapter *, int, int,
    struct intrs_and_queues *);
static int prep_firmware(struct adapter *);
static int upload_config_file(struct adapter *, const struct firmware *,
    uint32_t *, uint32_t *);
static int partition_resources(struct adapter *, const struct firmware *);
static int get_params__pre_init(struct adapter *);
static int get_params__post_init(struct adapter *);
static void t4_set_desc(struct adapter *);
static void build_medialist(struct port_info *);
static int update_mac_settings(struct port_info *, int);
static int cxgbe_init_locked(struct port_info *);
static int cxgbe_init_synchronized(struct port_info *);
static int cxgbe_uninit_locked(struct port_info *);
static int cxgbe_uninit_synchronized(struct port_info *);
static int adapter_full_init(struct adapter *);
static int adapter_full_uninit(struct adapter *);
static int port_full_init(struct port_info *);
static int port_full_uninit(struct port_info *);
static void quiesce_eq(struct adapter *, struct sge_eq *);
static void quiesce_iq(struct adapter *, struct sge_iq *);
static void quiesce_fl(struct adapter *, struct sge_fl *);
static int t4_alloc_irq(struct adapter *, struct irq *, int rid,
    driver_intr_t *, void *, char *);
static int t4_free_irq(struct adapter *, struct irq *);
static void reg_block_dump(struct adapter *, uint8_t *, unsigned int,
    unsigned int);
static void t4_get_regs(struct adapter *, struct t4_regdump *, uint8_t *);
static void cxgbe_tick(void *);
static void cxgbe_vlan_config(void *, struct ifnet *, uint16_t);
static int cpl_not_handled(struct sge_iq *, const struct rss_header *,
    struct mbuf *);
static int an_not_handled(struct sge_iq *, const struct rsp_ctrl *);
static int t4_sysctls(struct adapter *);
static int cxgbe_sysctls(struct port_info *);
static int sysctl_int_array(SYSCTL_HANDLER_ARGS);
static int sysctl_bitfield(SYSCTL_HANDLER_ARGS);
static int sysctl_holdoff_tmr_idx(SYSCTL_HANDLER_ARGS);
static int sysctl_holdoff_pktc_idx(SYSCTL_HANDLER_ARGS);
static int sysctl_qsize_rxq(SYSCTL_HANDLER_ARGS);
static int sysctl_qsize_txq(SYSCTL_HANDLER_ARGS);
static int sysctl_handle_t4_reg64(SYSCTL_HANDLER_ARGS);
#ifdef SBUF_DRAIN
static int sysctl_cctrl(SYSCTL_HANDLER_ARGS);
static int sysctl_cpl_stats(SYSCTL_HANDLER_ARGS);
static int sysctl_ddp_stats(SYSCTL_HANDLER_ARGS);
static int sysctl_devlog(SYSCTL_HANDLER_ARGS);
static int sysctl_fcoe_stats(SYSCTL_HANDLER_ARGS);
static int sysctl_hw_sched(SYSCTL_HANDLER_ARGS);
static int sysctl_lb_stats(SYSCTL_HANDLER_ARGS);
static int sysctl_meminfo(SYSCTL_HANDLER_ARGS);
static int sysctl_path_mtus(SYSCTL_HANDLER_ARGS);
static int sysctl_pm_stats(SYSCTL_HANDLER_ARGS);
static int sysctl_rdma_stats(SYSCTL_HANDLER_ARGS);
static int sysctl_tcp_stats(SYSCTL_HANDLER_ARGS);
static int sysctl_tids(SYSCTL_HANDLER_ARGS);
static int sysctl_tp_err_stats(SYSCTL_HANDLER_ARGS);
static int sysctl_tx_rate(SYSCTL_HANDLER_ARGS);
#endif
static inline void txq_start(struct ifnet *, struct sge_txq *);
static uint32_t fconf_to_mode(uint32_t);
static uint32_t mode_to_fconf(uint32_t);
static uint32_t fspec_to_fconf(struct t4_filter_specification *);
static int get_filter_mode(struct adapter *, uint32_t *);
static int set_filter_mode(struct adapter *, uint32_t);
static inline uint64_t get_filter_hits(struct adapter *, uint32_t);
static int get_filter(struct adapter *, struct t4_filter *);
static int set_filter(struct adapter *, struct t4_filter *);
static int del_filter(struct adapter *, struct t4_filter *);
static void clear_filter(struct filter_entry *);
static int set_filter_wr(struct adapter *, int);
static int del_filter_wr(struct adapter *, int);
static int filter_rpl(struct sge_iq *, const struct rss_header *,
    struct mbuf *);
static int get_sge_context(struct adapter *, struct t4_sge_context *);
static int read_card_mem(struct adapter *, struct t4_mem_range *);
#ifdef TCP_OFFLOAD
static int toe_capability(struct port_info *, int);
#endif
static int t4_mod_event(module_t, int, void *);

struct t4_pciids {
	uint16_t device;
	char *desc;
} t4_pciids[] = {
	{0xa000, "Chelsio Terminator 4 FPGA"},
	{0x4400, "Chelsio T440-dbg"},
	{0x4401, "Chelsio T420-CR"},
	{0x4402, "Chelsio T422-CR"},
	{0x4403, "Chelsio T440-CR"},
	{0x4404, "Chelsio T420-BCH"},
	{0x4405, "Chelsio T440-BCH"},
	{0x4406, "Chelsio T440-CH"},
	{0x4407, "Chelsio T420-SO"},
	{0x4408, "Chelsio T420-CX"},
	{0x4409, "Chelsio T420-BT"},
	{0x440a, "Chelsio T404-BT"},
};

#ifdef TCP_OFFLOAD
/*
 * service_iq() has an iq and needs the fl.  Offset of fl from the iq should be
 * exactly the same for both rxq and ofld_rxq.
 */
CTASSERT(offsetof(struct sge_ofld_rxq, iq) == offsetof(struct sge_rxq, iq));
CTASSERT(offsetof(struct sge_ofld_rxq, fl) == offsetof(struct sge_rxq, fl));
#endif

static int
t4_probe(device_t dev)
{
	int i;
	uint16_t v = pci_get_vendor(dev);
	uint16_t d = pci_get_device(dev);
	uint8_t f = pci_get_function(dev);

	if (v != PCI_VENDOR_ID_CHELSIO)
		return (ENXIO);

	/* Attach only to PF0 of the FPGA */
	if (d == 0xa000 && f != 0)
		return (ENXIO);

	for (i = 0; i < ARRAY_SIZE(t4_pciids); i++) {
		if (d == t4_pciids[i].device) {
			device_set_desc(dev, t4_pciids[i].desc);
			return (BUS_PROBE_DEFAULT);
		}
	}

	return (ENXIO);
}

static int
t4_attach(device_t dev)
{
	struct adapter *sc;
	int rc = 0, i, n10g, n1g, rqidx, tqidx;
	struct intrs_and_queues iaq;
	struct sge *s;
#ifdef TCP_OFFLOAD
	int ofld_rqidx, ofld_tqidx;
#endif

	sc = device_get_softc(dev);
	sc->dev = dev;

	pci_enable_busmaster(dev);
	if (pci_find_cap(dev, PCIY_EXPRESS, &i) == 0) {
		uint32_t v;

		pci_set_max_read_req(dev, 4096);
		v = pci_read_config(dev, i + PCIR_EXPRESS_DEVICE_CTL, 2);
		v |= PCIM_EXP_CTL_RELAXED_ORD_ENABLE;
		pci_write_config(dev, i + PCIR_EXPRESS_DEVICE_CTL, v, 2);
	}

	snprintf(sc->lockname, sizeof(sc->lockname), "%s",
	    device_get_nameunit(dev));
	mtx_init(&sc->sc_lock, sc->lockname, 0, MTX_DEF);
	mtx_lock(&t4_list_lock);
	SLIST_INSERT_HEAD(&t4_list, sc, link);
	mtx_unlock(&t4_list_lock);

	mtx_init(&sc->sfl_lock, "starving freelists", 0, MTX_DEF);
	TAILQ_INIT(&sc->sfl);
	callout_init(&sc->sfl_callout, CALLOUT_MPSAFE);

	rc = map_bars(sc);
	if (rc != 0)
		goto done; /* error message displayed already */

	/*
	 * This is the real PF# to which we're attaching.  Works from within PCI
	 * passthrough environments too, where pci_get_function() could return a
	 * different PF# depending on the passthrough configuration.  We need to
	 * use the real PF# in all our communication with the firmware.
	 */
	sc->pf = G_SOURCEPF(t4_read_reg(sc, A_PL_WHOAMI));
	sc->mbox = sc->pf;

	memset(sc->chan_map, 0xff, sizeof(sc->chan_map));
	sc->an_handler = an_not_handled;
	for (i = 0; i < ARRAY_SIZE(sc->cpl_handler); i++)
		sc->cpl_handler[i] = cpl_not_handled;
	t4_register_cpl_handler(sc, CPL_SET_TCB_RPL, filter_rpl);

	/* Prepare the adapter for operation */
	rc = -t4_prep_adapter(sc);
	if (rc != 0) {
		device_printf(dev, "failed to prepare adapter: %d.\n", rc);
		goto done;
	}

	/*
	 * Do this really early, with the memory windows set up even before the
	 * character device.  The userland tool's register i/o and mem read
	 * will work even in "recovery mode".
	 */
	setup_memwin(sc);
	sc->cdev = make_dev(&t4_cdevsw, device_get_unit(dev), UID_ROOT,
	    GID_WHEEL, 0600, "%s", device_get_nameunit(dev));
	sc->cdev->si_drv1 = sc;

	/* Go no further if recovery mode has been requested. */
	if (TUNABLE_INT_FETCH("hw.cxgbe.sos", &i) && i != 0) {
		device_printf(dev, "recovery mode.\n");
		goto done;
	}

	/* Prepare the firmware for operation */
	rc = prep_firmware(sc);
	if (rc != 0)
		goto done; /* error message displayed already */

	rc = get_params__pre_init(sc);
	if (rc != 0)
		goto done; /* error message displayed already */

	rc = t4_sge_init(sc);
	if (rc != 0)
		goto done; /* error message displayed already */

	if (sc->flags & MASTER_PF) {
		/* get basic stuff going */
		rc = -t4_fw_initialize(sc, sc->mbox);
		if (rc != 0) {
			device_printf(dev, "early init failed: %d.\n", rc);
			goto done;
		}
	}

	rc = get_params__post_init(sc);
	if (rc != 0)
		goto done; /* error message displayed already */

	if (sc->flags & MASTER_PF) {

		/* final tweaks to some settings */

		t4_load_mtus(sc, sc->params.mtus, sc->params.a_wnd,
		    sc->params.b_wnd);
		t4_write_reg(sc, A_ULP_RX_TDDP_PSZ, V_HPZ0(PAGE_SHIFT - 12));
		t4_set_reg_field(sc, A_TP_PARA_REG3, F_TUNNELCNGDROP0 |
		    F_TUNNELCNGDROP1 | F_TUNNELCNGDROP2 | F_TUNNELCNGDROP3, 0);
		t4_set_reg_field(sc, A_TP_PARA_REG5,
		    V_INDICATESIZE(M_INDICATESIZE) |
		    F_REARMDDPOFFSET | F_RESETDDPOFFSET,
		    V_INDICATESIZE(M_INDICATESIZE) |
		    F_REARMDDPOFFSET | F_RESETDDPOFFSET);
	} else {
		/*
		 * XXX: Verify that we can live with whatever the master driver
		 * has done so far, and hope that it doesn't change any global
		 * setting from underneath us in the future.
		 */
	}

	t4_read_indirect(sc, A_TP_PIO_ADDR, A_TP_PIO_DATA, &sc->filter_mode, 1,
	    A_TP_VLAN_PRI_MAP);

	for (i = 0; i < NCHAN; i++)
		sc->params.tp.tx_modq[i] = i;

	rc = t4_create_dma_tag(sc);
	if (rc != 0)
		goto done; /* error message displayed already */

	/*
	 * First pass over all the ports - allocate VIs and initialize some
	 * basic parameters like mac address, port type, etc.  We also figure
	 * out whether a port is 10G or 1G and use that information when
	 * calculating how many interrupts to attempt to allocate.
	 */
	n10g = n1g = 0;
	for_each_port(sc, i) {
		struct port_info *pi;

		pi = malloc(sizeof(*pi), M_CXGBE, M_ZERO | M_WAITOK);
		sc->port[i] = pi;

		/* These must be set before t4_port_init */
		pi->adapter = sc;
		pi->port_id = i;

		/* Allocate the vi and initialize parameters like mac addr */
		rc = -t4_port_init(pi, sc->mbox, sc->pf, 0);
		if (rc != 0) {
			device_printf(dev, "unable to initialize port %d: %d\n",
			    i, rc);
			free(pi, M_CXGBE);
			sc->port[i] = NULL;
			goto done;
		}

		snprintf(pi->lockname, sizeof(pi->lockname), "%sp%d",
		    device_get_nameunit(dev), i);
		mtx_init(&pi->pi_lock, pi->lockname, 0, MTX_DEF);

		if (is_10G_port(pi)) {
			n10g++;
			pi->tmr_idx = t4_tmr_idx_10g;
			pi->pktc_idx = t4_pktc_idx_10g;
		} else {
			n1g++;
			pi->tmr_idx = t4_tmr_idx_1g;
			pi->pktc_idx = t4_pktc_idx_1g;
		}

		pi->xact_addr_filt = -1;

		pi->qsize_rxq = t4_qsize_rxq;
		pi->qsize_txq = t4_qsize_txq;

		pi->dev = device_add_child(dev, "cxgbe", -1);
		if (pi->dev == NULL) {
			device_printf(dev,
			    "failed to add device for port %d.\n", i);
			rc = ENXIO;
			goto done;
		}
		device_set_softc(pi->dev, pi);
	}

	/*
	 * Interrupt type, # of interrupts, # of rx/tx queues, etc.
	 */
	rc = cfg_itype_and_nqueues(sc, n10g, n1g, &iaq);
	if (rc != 0)
		goto done; /* error message displayed already */

	sc->intr_type = iaq.intr_type;
	sc->intr_count = iaq.nirq;
	sc->flags |= iaq.intr_flags;

	s = &sc->sge;
	s->nrxq = n10g * iaq.nrxq10g + n1g * iaq.nrxq1g;
	s->ntxq = n10g * iaq.ntxq10g + n1g * iaq.ntxq1g;
	s->neq = s->ntxq + s->nrxq;	/* the free list in an rxq is an eq */
	s->neq += sc->params.nports + 1;/* ctrl queues: 1 per port + 1 mgmt */
	s->niq = s->nrxq + 1;		/* 1 extra for firmware event queue */

#ifdef TCP_OFFLOAD
	if (is_offload(sc)) {

		s->nofldrxq = n10g * iaq.nofldrxq10g + n1g * iaq.nofldrxq1g;
		s->nofldtxq = n10g * iaq.nofldtxq10g + n1g * iaq.nofldtxq1g;
		s->neq += s->nofldtxq + s->nofldrxq;
		s->niq += s->nofldrxq;

		s->ofld_rxq = malloc(s->nofldrxq * sizeof(struct sge_ofld_rxq),
		    M_CXGBE, M_ZERO | M_WAITOK);
		s->ofld_txq = malloc(s->nofldtxq * sizeof(struct sge_wrq),
		    M_CXGBE, M_ZERO | M_WAITOK);
	}
#endif

	s->ctrlq = malloc(sc->params.nports * sizeof(struct sge_wrq), M_CXGBE,
	    M_ZERO | M_WAITOK);
	s->rxq = malloc(s->nrxq * sizeof(struct sge_rxq), M_CXGBE,
	    M_ZERO | M_WAITOK);
	s->txq = malloc(s->ntxq * sizeof(struct sge_txq), M_CXGBE,
	    M_ZERO | M_WAITOK);
	s->iqmap = malloc(s->niq * sizeof(struct sge_iq *), M_CXGBE,
	    M_ZERO | M_WAITOK);
	s->eqmap = malloc(s->neq * sizeof(struct sge_eq *), M_CXGBE,
	    M_ZERO | M_WAITOK);

	sc->irq = malloc(sc->intr_count * sizeof(struct irq), M_CXGBE,
	    M_ZERO | M_WAITOK);

	t4_init_l2t(sc, M_WAITOK);

	/*
	 * Second pass over the ports.  This time we know the number of rx and
	 * tx queues that each port should get.
	 */
	rqidx = tqidx = 0;
#ifdef TCP_OFFLOAD
	ofld_rqidx = ofld_tqidx = 0;
#endif
	for_each_port(sc, i) {
		struct port_info *pi = sc->port[i];

		if (pi == NULL)
			continue;

		pi->first_rxq = rqidx;
		pi->first_txq = tqidx;
		if (is_10G_port(pi)) {
			pi->nrxq = iaq.nrxq10g;
			pi->ntxq = iaq.ntxq10g;
		} else {
			pi->nrxq = iaq.nrxq1g;
			pi->ntxq = iaq.ntxq1g;
		}

		rqidx += pi->nrxq;
		tqidx += pi->ntxq;

#ifdef TCP_OFFLOAD
		if (is_offload(sc)) {
			pi->first_ofld_rxq = ofld_rqidx;
			pi->first_ofld_txq = ofld_tqidx;
			if (is_10G_port(pi)) {
				pi->nofldrxq = iaq.nofldrxq10g;
				pi->nofldtxq = iaq.nofldtxq10g;
			} else {
				pi->nofldrxq = iaq.nofldrxq1g;
				pi->nofldtxq = iaq.nofldtxq1g;
			}
			ofld_rqidx += pi->nofldrxq;
			ofld_tqidx += pi->nofldtxq;
		}
#endif
	}

	rc = bus_generic_attach(dev);
	if (rc != 0) {
		device_printf(dev,
		    "failed to attach all child ports: %d\n", rc);
		goto done;
	}

	device_printf(dev,
	    "PCIe x%d, %d ports, %d %s interrupt%s, %d eq, %d iq\n",
	    sc->params.pci.width, sc->params.nports, sc->intr_count,
	    sc->intr_type == INTR_MSIX ? "MSI-X" :
	    (sc->intr_type == INTR_MSI ? "MSI" : "INTx"),
	    sc->intr_count > 1 ? "s" : "", sc->sge.neq, sc->sge.niq);

	t4_set_desc(sc);

done:
	if (rc != 0 && sc->cdev) {
		/* cdev was created and so cxgbetool works; recover that way. */
		device_printf(dev,
		    "error during attach, adapter is now in recovery mode.\n");
		rc = 0;
	}

	if (rc != 0)
		t4_detach(dev);
	else
		t4_sysctls(sc);

	return (rc);
}

/*
 * Idempotent
 */
static int
t4_detach(device_t dev)
{
	struct adapter *sc;
	struct port_info *pi;
	int i, rc;

	sc = device_get_softc(dev);

	if (sc->flags & FULL_INIT_DONE)
		t4_intr_disable(sc);

	if (sc->cdev) {
		destroy_dev(sc->cdev);
		sc->cdev = NULL;
	}

	rc = bus_generic_detach(dev);
	if (rc) {
		device_printf(dev,
		    "failed to detach child devices: %d\n", rc);
		return (rc);
	}

	for (i = 0; i < MAX_NPORTS; i++) {
		pi = sc->port[i];
		if (pi) {
			t4_free_vi(pi->adapter, sc->mbox, sc->pf, 0, pi->viid);
			if (pi->dev)
				device_delete_child(dev, pi->dev);

			mtx_destroy(&pi->pi_lock);
			free(pi, M_CXGBE);
		}
	}

	if (sc->flags & FULL_INIT_DONE)
		adapter_full_uninit(sc);

	if (sc->flags & FW_OK)
		t4_fw_bye(sc, sc->mbox);

	if (sc->intr_type == INTR_MSI || sc->intr_type == INTR_MSIX)
		pci_release_msi(dev);

	if (sc->regs_res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->regs_rid,
		    sc->regs_res);

	if (sc->msix_res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->msix_rid,
		    sc->msix_res);

	if (sc->l2t)
		t4_free_l2t(sc->l2t);

#ifdef TCP_OFFLOAD
	free(sc->sge.ofld_rxq, M_CXGBE);
	free(sc->sge.ofld_txq, M_CXGBE);
#endif
	free(sc->irq, M_CXGBE);
	free(sc->sge.rxq, M_CXGBE);
	free(sc->sge.txq, M_CXGBE);
	free(sc->sge.ctrlq, M_CXGBE);
	free(sc->sge.iqmap, M_CXGBE);
	free(sc->sge.eqmap, M_CXGBE);
	free(sc->tids.ftid_tab, M_CXGBE);
	t4_destroy_dma_tag(sc);
	if (mtx_initialized(&sc->sc_lock)) {
		mtx_lock(&t4_list_lock);
		SLIST_REMOVE(&t4_list, sc, adapter, link);
		mtx_unlock(&t4_list_lock);
		mtx_destroy(&sc->sc_lock);
	}

	if (mtx_initialized(&sc->sfl_lock))
		mtx_destroy(&sc->sfl_lock);

	bzero(sc, sizeof(*sc));

	return (0);
}


static int
cxgbe_probe(device_t dev)
{
	char buf[128];
	struct port_info *pi = device_get_softc(dev);

	snprintf(buf, sizeof(buf), "port %d", pi->port_id);
	device_set_desc_copy(dev, buf);

	return (BUS_PROBE_DEFAULT);
}

#define T4_CAP (IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU | IFCAP_HWCSUM | \
    IFCAP_VLAN_HWCSUM | IFCAP_TSO | IFCAP_JUMBO_MTU | IFCAP_LRO | \
    IFCAP_VLAN_HWTSO | IFCAP_LINKSTATE | IFCAP_HWCSUM_IPV6)
#define T4_CAP_ENABLE (T4_CAP)

static int
cxgbe_attach(device_t dev)
{
	struct port_info *pi = device_get_softc(dev);
	struct ifnet *ifp;

	/* Allocate an ifnet and set it up */
	ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "Cannot allocate ifnet\n");
		return (ENOMEM);
	}
	pi->ifp = ifp;
	ifp->if_softc = pi;

	callout_init(&pi->tick, CALLOUT_MPSAFE);

	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;

	ifp->if_init = cxgbe_init;
	ifp->if_ioctl = cxgbe_ioctl;
	ifp->if_transmit = cxgbe_transmit;
	ifp->if_qflush = cxgbe_qflush;

	ifp->if_capabilities = T4_CAP;
#ifdef TCP_OFFLOAD
	if (is_offload(pi->adapter))
		ifp->if_capabilities |= IFCAP_TOE4;
#endif
	ifp->if_capenable = T4_CAP_ENABLE;
	ifp->if_hwassist = CSUM_TCP | CSUM_UDP | CSUM_IP | CSUM_TSO |
	    CSUM_UDP_IPV6 | CSUM_TCP_IPV6;

	/* Initialize ifmedia for this port */
	ifmedia_init(&pi->media, IFM_IMASK, cxgbe_media_change,
	    cxgbe_media_status);
	build_medialist(pi);

	pi->vlan_c = EVENTHANDLER_REGISTER(vlan_config, cxgbe_vlan_config, ifp,
	    EVENTHANDLER_PRI_ANY);

	ether_ifattach(ifp, pi->hw_addr);

#ifdef TCP_OFFLOAD
	if (is_offload(pi->adapter)) {
		device_printf(dev,
		    "%d txq, %d rxq (NIC); %d txq, %d rxq (TOE)\n",
		    pi->ntxq, pi->nrxq, pi->nofldtxq, pi->nofldrxq);
	} else
#endif
		device_printf(dev, "%d txq, %d rxq\n", pi->ntxq, pi->nrxq);

	cxgbe_sysctls(pi);

	return (0);
}

static int
cxgbe_detach(device_t dev)
{
	struct port_info *pi = device_get_softc(dev);
	struct adapter *sc = pi->adapter;
	struct ifnet *ifp = pi->ifp;

	/* Tell if_ioctl and if_init that the port is going away */
	ADAPTER_LOCK(sc);
	SET_DOOMED(pi);
	wakeup(&sc->flags);
	while (IS_BUSY(sc))
		mtx_sleep(&sc->flags, &sc->sc_lock, 0, "t4detach", 0);
	SET_BUSY(sc);
	ADAPTER_UNLOCK(sc);

	if (pi->vlan_c)
		EVENTHANDLER_DEREGISTER(vlan_config, pi->vlan_c);

	PORT_LOCK(pi);
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	callout_stop(&pi->tick);
	PORT_UNLOCK(pi);
	callout_drain(&pi->tick);

	/* Let detach proceed even if these fail. */
	cxgbe_uninit_synchronized(pi);
	port_full_uninit(pi);

	ifmedia_removeall(&pi->media);
	ether_ifdetach(pi->ifp);
	if_free(pi->ifp);

	ADAPTER_LOCK(sc);
	CLR_BUSY(sc);
	wakeup_one(&sc->flags);
	ADAPTER_UNLOCK(sc);

	return (0);
}

static void
cxgbe_init(void *arg)
{
	struct port_info *pi = arg;
	struct adapter *sc = pi->adapter;

	ADAPTER_LOCK(sc);
	cxgbe_init_locked(pi); /* releases adapter lock */
	ADAPTER_LOCK_ASSERT_NOTOWNED(sc);
}

static int
cxgbe_ioctl(struct ifnet *ifp, unsigned long cmd, caddr_t data)
{
	int rc = 0, mtu, flags;
	struct port_info *pi = ifp->if_softc;
	struct adapter *sc = pi->adapter;
	struct ifreq *ifr = (struct ifreq *)data;
	uint32_t mask;

	switch (cmd) {
	case SIOCSIFMTU:
		ADAPTER_LOCK(sc);
		rc = IS_DOOMED(pi) ? ENXIO : (IS_BUSY(sc) ? EBUSY : 0);
		if (rc) {
fail:
			ADAPTER_UNLOCK(sc);
			return (rc);
		}

		mtu = ifr->ifr_mtu;
		if ((mtu < ETHERMIN) || (mtu > ETHERMTU_JUMBO)) {
			rc = EINVAL;
		} else {
			ifp->if_mtu = mtu;
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				t4_update_fl_bufsize(ifp);
				PORT_LOCK(pi);
				rc = update_mac_settings(pi, XGMAC_MTU);
				PORT_UNLOCK(pi);
			}
		}
		ADAPTER_UNLOCK(sc);
		break;

	case SIOCSIFFLAGS:
		ADAPTER_LOCK(sc);
		if (IS_DOOMED(pi)) {
			rc = ENXIO;
			goto fail;
		}
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				flags = pi->if_flags;
				if ((ifp->if_flags ^ flags) &
				    (IFF_PROMISC | IFF_ALLMULTI)) {
					if (IS_BUSY(sc)) {
						rc = EBUSY;
						goto fail;
					}
					PORT_LOCK(pi);
					rc = update_mac_settings(pi,
					    XGMAC_PROMISC | XGMAC_ALLMULTI);
					PORT_UNLOCK(pi);
				}
				ADAPTER_UNLOCK(sc);
			} else
				rc = cxgbe_init_locked(pi);
			pi->if_flags = ifp->if_flags;
		} else if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			rc = cxgbe_uninit_locked(pi);
		else
			ADAPTER_UNLOCK(sc);

		ADAPTER_LOCK_ASSERT_NOTOWNED(sc);
		break;

	case SIOCADDMULTI:	
	case SIOCDELMULTI: /* these two can be called with a mutex held :-( */
		ADAPTER_LOCK(sc);
		rc = IS_DOOMED(pi) ? ENXIO : (IS_BUSY(sc) ? EBUSY : 0);
		if (rc)
			goto fail;

		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			PORT_LOCK(pi);
			rc = update_mac_settings(pi, XGMAC_MCADDRS);
			PORT_UNLOCK(pi);
		}
		ADAPTER_UNLOCK(sc);
		break;

	case SIOCSIFCAP:
		ADAPTER_LOCK(sc);
		rc = IS_DOOMED(pi) ? ENXIO : (IS_BUSY(sc) ? EBUSY : 0);
		if (rc)
			goto fail;

		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		if (mask & IFCAP_TXCSUM) {
			ifp->if_capenable ^= IFCAP_TXCSUM;
			ifp->if_hwassist ^= (CSUM_TCP | CSUM_UDP | CSUM_IP);

			if (IFCAP_TSO4 & ifp->if_capenable &&
			    !(IFCAP_TXCSUM & ifp->if_capenable)) {
				ifp->if_capenable &= ~IFCAP_TSO4;
				if_printf(ifp,
				    "tso4 disabled due to -txcsum.\n");
			}
		}
		if (mask & IFCAP_TXCSUM_IPV6) {
			ifp->if_capenable ^= IFCAP_TXCSUM_IPV6;
			ifp->if_hwassist ^= (CSUM_UDP_IPV6 | CSUM_TCP_IPV6);

			if (IFCAP_TSO6 & ifp->if_capenable &&
			    !(IFCAP_TXCSUM_IPV6 & ifp->if_capenable)) {
				ifp->if_capenable &= ~IFCAP_TSO6;
				if_printf(ifp,
				    "tso6 disabled due to -txcsum6.\n");
			}
		}
		if (mask & IFCAP_RXCSUM)
			ifp->if_capenable ^= IFCAP_RXCSUM;
		if (mask & IFCAP_RXCSUM_IPV6)
			ifp->if_capenable ^= IFCAP_RXCSUM_IPV6;

		/*
		 * Note that we leave CSUM_TSO alone (it is always set).  The
		 * kernel takes both IFCAP_TSOx and CSUM_TSO into account before
		 * sending a TSO request our way, so it's sufficient to toggle
		 * IFCAP_TSOx only.
		 */
		if (mask & IFCAP_TSO4) {
			if (!(IFCAP_TSO4 & ifp->if_capenable) &&
			    !(IFCAP_TXCSUM & ifp->if_capenable)) {
				if_printf(ifp, "enable txcsum first.\n");
				rc = EAGAIN;
				goto fail;
			}
			ifp->if_capenable ^= IFCAP_TSO4;
		}
		if (mask & IFCAP_TSO6) {
			if (!(IFCAP_TSO6 & ifp->if_capenable) &&
			    !(IFCAP_TXCSUM_IPV6 & ifp->if_capenable)) {
				if_printf(ifp, "enable txcsum6 first.\n");
				rc = EAGAIN;
				goto fail;
			}
			ifp->if_capenable ^= IFCAP_TSO6;
		}
		if (mask & IFCAP_LRO) {
#if defined(INET) || defined(INET6)
			int i;
			struct sge_rxq *rxq;

			ifp->if_capenable ^= IFCAP_LRO;
			for_each_rxq(pi, i, rxq) {
				if (ifp->if_capenable & IFCAP_LRO)
					rxq->iq.flags |= IQ_LRO_ENABLED;
				else
					rxq->iq.flags &= ~IQ_LRO_ENABLED;
			}
#endif
		}
#ifdef TCP_OFFLOAD
		if (mask & IFCAP_TOE) {
			int enable = (ifp->if_capenable ^ mask) & IFCAP_TOE;

			rc = toe_capability(pi, enable);
			if (rc != 0)
				goto fail;

			ifp->if_capenable ^= mask;
		}
#endif
		if (mask & IFCAP_VLAN_HWTAGGING) {
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				PORT_LOCK(pi);
				rc = update_mac_settings(pi, XGMAC_VLANEX);
				PORT_UNLOCK(pi);
			}
		}
		if (mask & IFCAP_VLAN_MTU) {
			ifp->if_capenable ^= IFCAP_VLAN_MTU;

			/* Need to find out how to disable auto-mtu-inflation */
		}
		if (mask & IFCAP_VLAN_HWTSO)
			ifp->if_capenable ^= IFCAP_VLAN_HWTSO;
		if (mask & IFCAP_VLAN_HWCSUM)
			ifp->if_capenable ^= IFCAP_VLAN_HWCSUM;

#ifdef VLAN_CAPABILITIES
		VLAN_CAPABILITIES(ifp);
#endif
		ADAPTER_UNLOCK(sc);
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		ifmedia_ioctl(ifp, ifr, &pi->media, cmd);
		break;

	default:
		rc = ether_ioctl(ifp, cmd, data);
	}

	return (rc);
}

static int
cxgbe_transmit(struct ifnet *ifp, struct mbuf *m)
{
	struct port_info *pi = ifp->if_softc;
	struct adapter *sc = pi->adapter;
	struct sge_txq *txq = &sc->sge.txq[pi->first_txq];
	struct buf_ring *br;
	int rc;

	M_ASSERTPKTHDR(m);

	if (__predict_false(pi->link_cfg.link_ok == 0)) {
		m_freem(m);
		return (ENETDOWN);
	}

	if (m->m_flags & M_FLOWID)
		txq += (m->m_pkthdr.flowid % pi->ntxq);
	br = txq->br;

	if (TXQ_TRYLOCK(txq) == 0) {
		struct sge_eq *eq = &txq->eq;

		/*
		 * It is possible that t4_eth_tx finishes up and releases the
		 * lock between the TRYLOCK above and the drbr_enqueue here.  We
		 * need to make sure that this mbuf doesn't just sit there in
		 * the drbr.
		 */

		rc = drbr_enqueue(ifp, br, m);
		if (rc == 0 && callout_pending(&eq->tx_callout) == 0 &&
		    !(eq->flags & EQ_DOOMED))
			callout_reset(&eq->tx_callout, 1, t4_tx_callout, eq);
		return (rc);
	}

	/*
	 * txq->m is the mbuf that is held up due to a temporary shortage of
	 * resources and it should be put on the wire first.  Then what's in
	 * drbr and finally the mbuf that was just passed in to us.
	 *
	 * Return code should indicate the fate of the mbuf that was passed in
	 * this time.
	 */

	TXQ_LOCK_ASSERT_OWNED(txq);
	if (drbr_needs_enqueue(ifp, br) || txq->m) {

		/* Queued for transmission. */

		rc = drbr_enqueue(ifp, br, m);
		m = txq->m ? txq->m : drbr_dequeue(ifp, br);
		(void) t4_eth_tx(ifp, txq, m);
		TXQ_UNLOCK(txq);
		return (rc);
	}

	/* Direct transmission. */
	rc = t4_eth_tx(ifp, txq, m);
	if (rc != 0 && txq->m)
		rc = 0;	/* held, will be transmitted soon (hopefully) */

	TXQ_UNLOCK(txq);
	return (rc);
}

static void
cxgbe_qflush(struct ifnet *ifp)
{
	struct port_info *pi = ifp->if_softc;
	struct sge_txq *txq;
	int i;
	struct mbuf *m;

	/* queues do not exist if !PORT_INIT_DONE. */
	if (pi->flags & PORT_INIT_DONE) {
		for_each_txq(pi, i, txq) {
			TXQ_LOCK(txq);
			m_freem(txq->m);
			txq->m = NULL;
			while ((m = buf_ring_dequeue_sc(txq->br)) != NULL)
				m_freem(m);
			TXQ_UNLOCK(txq);
		}
	}
	if_qflush(ifp);
}

static int
cxgbe_media_change(struct ifnet *ifp)
{
	struct port_info *pi = ifp->if_softc;

	device_printf(pi->dev, "%s unimplemented.\n", __func__);

	return (EOPNOTSUPP);
}

static void
cxgbe_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct port_info *pi = ifp->if_softc;
	struct ifmedia_entry *cur = pi->media.ifm_cur;
	int speed = pi->link_cfg.speed;
	int data = (pi->port_type << 8) | pi->mod_type;

	if (cur->ifm_data != data) {
		build_medialist(pi);
		cur = pi->media.ifm_cur;
	}

	ifmr->ifm_status = IFM_AVALID;
	if (!pi->link_cfg.link_ok)
		return;

	ifmr->ifm_status |= IFM_ACTIVE;

	/* active and current will differ iff current media is autoselect. */
	if (IFM_SUBTYPE(cur->ifm_media) != IFM_AUTO)
		return;

	ifmr->ifm_active = IFM_ETHER | IFM_FDX;
	if (speed == SPEED_10000)
		ifmr->ifm_active |= IFM_10G_T;
	else if (speed == SPEED_1000)
		ifmr->ifm_active |= IFM_1000_T;
	else if (speed == SPEED_100)
		ifmr->ifm_active |= IFM_100_TX;
	else if (speed == SPEED_10)
		ifmr->ifm_active |= IFM_10_T;
	else
		KASSERT(0, ("%s: link up but speed unknown (%u)", __func__,
			    speed));
}

void
t4_fatal_err(struct adapter *sc)
{
	t4_set_reg_field(sc, A_SGE_CONTROL, F_GLOBALENABLE, 0);
	t4_intr_disable(sc);
	log(LOG_EMERG, "%s: encountered fatal error, adapter stopped.\n",
	    device_get_nameunit(sc->dev));
}

static int
map_bars(struct adapter *sc)
{
	sc->regs_rid = PCIR_BAR(0);
	sc->regs_res = bus_alloc_resource_any(sc->dev, SYS_RES_MEMORY,
	    &sc->regs_rid, RF_ACTIVE);
	if (sc->regs_res == NULL) {
		device_printf(sc->dev, "cannot map registers.\n");
		return (ENXIO);
	}
	sc->bt = rman_get_bustag(sc->regs_res);
	sc->bh = rman_get_bushandle(sc->regs_res);
	sc->mmio_len = rman_get_size(sc->regs_res);

	sc->msix_rid = PCIR_BAR(4);
	sc->msix_res = bus_alloc_resource_any(sc->dev, SYS_RES_MEMORY,
	    &sc->msix_rid, RF_ACTIVE);
	if (sc->msix_res == NULL) {
		device_printf(sc->dev, "cannot map MSI-X BAR.\n");
		return (ENXIO);
	}

	return (0);
}

static void
setup_memwin(struct adapter *sc)
{
	uint32_t bar0;

	/*
	 * Read low 32b of bar0 indirectly via the hardware backdoor mechanism.
	 * Works from within PCI passthrough environments too, where
	 * rman_get_start() can return a different value.  We need to program
	 * the memory window decoders with the actual addresses that will be
	 * coming across the PCIe link.
	 */
	bar0 = t4_hw_pci_read_cfg4(sc, PCIR_BAR(0));
	bar0 &= (uint32_t) PCIM_BAR_MEM_BASE;

	t4_write_reg(sc, PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_BASE_WIN, 0),
	    	     (bar0 + MEMWIN0_BASE) | V_BIR(0) |
		     V_WINDOW(ilog2(MEMWIN0_APERTURE) - 10));

	t4_write_reg(sc, PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_BASE_WIN, 1),
		     (bar0 + MEMWIN1_BASE) | V_BIR(0) |
		     V_WINDOW(ilog2(MEMWIN1_APERTURE) - 10));

	t4_write_reg(sc, PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_BASE_WIN, 2),
		     (bar0 + MEMWIN2_BASE) | V_BIR(0) |
		     V_WINDOW(ilog2(MEMWIN2_APERTURE) - 10));

	/* flush */
	t4_read_reg(sc, PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_BASE_WIN, 2));
}

static int
cfg_itype_and_nqueues(struct adapter *sc, int n10g, int n1g,
    struct intrs_and_queues *iaq)
{
	int rc, itype, navail, nrxq10g, nrxq1g, n;
	int nofldrxq10g = 0, nofldrxq1g = 0;

	bzero(iaq, sizeof(*iaq));

	iaq->ntxq10g = t4_ntxq10g;
	iaq->ntxq1g = t4_ntxq1g;
	iaq->nrxq10g = nrxq10g = t4_nrxq10g;
	iaq->nrxq1g = nrxq1g = t4_nrxq1g;
#ifdef TCP_OFFLOAD
	if (is_offload(sc)) {
		iaq->nofldtxq10g = t4_nofldtxq10g;
		iaq->nofldtxq1g = t4_nofldtxq1g;
		iaq->nofldrxq10g = nofldrxq10g = t4_nofldrxq10g;
		iaq->nofldrxq1g = nofldrxq1g = t4_nofldrxq1g;
	}
#endif

	for (itype = INTR_MSIX; itype; itype >>= 1) {

		if ((itype & t4_intr_types) == 0)
			continue;	/* not allowed */

		if (itype == INTR_MSIX)
			navail = pci_msix_count(sc->dev);
		else if (itype == INTR_MSI)
			navail = pci_msi_count(sc->dev);
		else
			navail = 1;
restart:
		if (navail == 0)
			continue;

		iaq->intr_type = itype;
		iaq->intr_flags = 0;

		/*
		 * Best option: an interrupt vector for errors, one for the
		 * firmware event queue, and one each for each rxq (NIC as well
		 * as offload).
		 */
		iaq->nirq = T4_EXTRA_INTR;
		iaq->nirq += n10g * (nrxq10g + nofldrxq10g);
		iaq->nirq += n1g * (nrxq1g + nofldrxq1g);
		if (iaq->nirq <= navail &&
		    (itype != INTR_MSI || powerof2(iaq->nirq))) {
			iaq->intr_flags |= INTR_DIRECT;
			goto allocate;
		}

		/*
		 * Second best option: an interrupt vector for errors, one for
		 * the firmware event queue, and one each for either NIC or
		 * offload rxq's.
		 */
		iaq->nirq = T4_EXTRA_INTR;
		iaq->nirq += n10g * max(nrxq10g, nofldrxq10g);
		iaq->nirq += n1g * max(nrxq1g, nofldrxq1g);
		if (iaq->nirq <= navail &&
		    (itype != INTR_MSI || powerof2(iaq->nirq)))
			goto allocate;

		/*
		 * Next best option: an interrupt vector for errors, one for the
		 * firmware event queue, and at least one per port.  At this
		 * point we know we'll have to downsize nrxq or nofldrxq to fit
		 * what's available to us.
		 */
		iaq->nirq = T4_EXTRA_INTR;
		iaq->nirq += n10g + n1g;
		if (iaq->nirq <= navail) {
			int leftover = navail - iaq->nirq;

			if (n10g > 0) {
				int target = max(nrxq10g, nofldrxq10g);

				n = 1;
				while (n < target && leftover >= n10g) {
					leftover -= n10g;
					iaq->nirq += n10g;
					n++;
				}
				iaq->nrxq10g = min(n, nrxq10g);
#ifdef TCP_OFFLOAD
				if (is_offload(sc))
					iaq->nofldrxq10g = min(n, nofldrxq10g);
#endif
			}

			if (n1g > 0) {
				int target = max(nrxq1g, nofldrxq1g);

				n = 1;
				while (n < target && leftover >= n1g) {
					leftover -= n1g;
					iaq->nirq += n1g;
					n++;
				}
				iaq->nrxq1g = min(n, nrxq1g);
#ifdef TCP_OFFLOAD
				if (is_offload(sc))
					iaq->nofldrxq1g = min(n, nofldrxq1g);
#endif
			}

			if (itype != INTR_MSI || powerof2(iaq->nirq))
				goto allocate;
		}

		/*
		 * Least desirable option: one interrupt vector for everything.
		 */
		iaq->nirq = iaq->nrxq10g = iaq->nrxq1g = 1;
#ifdef TCP_OFFLOAD
		if (is_offload(sc))
			iaq->nofldrxq10g = iaq->nofldrxq1g = 1;
#endif

allocate:
		navail = iaq->nirq;
		rc = 0;
		if (itype == INTR_MSIX)
			rc = pci_alloc_msix(sc->dev, &navail);
		else if (itype == INTR_MSI)
			rc = pci_alloc_msi(sc->dev, &navail);

		if (rc == 0) {
			if (navail == iaq->nirq)
				return (0);

			/*
			 * Didn't get the number requested.  Use whatever number
			 * the kernel is willing to allocate (it's in navail).
			 */
			device_printf(sc->dev, "fewer vectors than requested, "
			    "type=%d, req=%d, rcvd=%d; will downshift req.\n",
			    itype, iaq->nirq, navail);
			pci_release_msi(sc->dev);
			goto restart;
		}

		device_printf(sc->dev,
		    "failed to allocate vectors:%d, type=%d, req=%d, rcvd=%d\n",
		    itype, rc, iaq->nirq, navail);
	}

	device_printf(sc->dev,
	    "failed to find a usable interrupt type.  "
	    "allowed=%d, msi-x=%d, msi=%d, intx=1", t4_intr_types,
	    pci_msix_count(sc->dev), pci_msi_count(sc->dev));

	return (ENXIO);
}

/*
 * Install a compatible firmware (if required), establish contact with it (by
 * saying hello), and reset the device.  If we end up as the master driver,
 * partition adapter resources by providing a configuration file to the
 * firmware.
 */
static int
prep_firmware(struct adapter *sc)
{
	const struct firmware *fw = NULL, *cfg = NULL, *default_cfg;
	int rc;
	enum dev_state state;

	default_cfg = firmware_get(T4_CFGNAME);

	/* Check firmware version and install a different one if necessary */
	rc = t4_check_fw_version(sc);
	snprintf(sc->fw_version, sizeof(sc->fw_version), "%u.%u.%u.%u",
	    G_FW_HDR_FW_VER_MAJOR(sc->params.fw_vers),
	    G_FW_HDR_FW_VER_MINOR(sc->params.fw_vers),
	    G_FW_HDR_FW_VER_MICRO(sc->params.fw_vers),
	    G_FW_HDR_FW_VER_BUILD(sc->params.fw_vers));
	if (rc != 0) {
		uint32_t v = 0;

		fw = firmware_get(T4_FWNAME);
		if (fw != NULL) {
			const struct fw_hdr *hdr = (const void *)fw->data;

			v = ntohl(hdr->fw_ver);

			/*
			 * The firmware module will not be used if it isn't the
			 * same major version as what the driver was compiled
			 * with.
			 */
			if (G_FW_HDR_FW_VER_MAJOR(v) != FW_VERSION_MAJOR) {
				device_printf(sc->dev,
				    "Found firmware image but version %d "
				    "can not be used with this driver (%d)\n",
				    G_FW_HDR_FW_VER_MAJOR(v), FW_VERSION_MAJOR);

				firmware_put(fw, FIRMWARE_UNLOAD);
				fw = NULL;
			}
		}

		if (fw == NULL && rc < 0) {
			device_printf(sc->dev, "No usable firmware. "
			    "card has %d.%d.%d, driver compiled with %d.%d.%d",
			    G_FW_HDR_FW_VER_MAJOR(sc->params.fw_vers),
			    G_FW_HDR_FW_VER_MINOR(sc->params.fw_vers),
			    G_FW_HDR_FW_VER_MICRO(sc->params.fw_vers),
			    FW_VERSION_MAJOR, FW_VERSION_MINOR,
			    FW_VERSION_MICRO);
			rc = EAGAIN;
			goto done;
		}

		/*
		 * Always upgrade, even for minor/micro/build mismatches.
		 * Downgrade only for a major version mismatch or if
		 * force_firmware_install was specified.
		 */
		if (fw != NULL && (rc < 0 || v > sc->params.fw_vers)) {
			device_printf(sc->dev,
			    "installing firmware %d.%d.%d.%d on card.\n",
			    G_FW_HDR_FW_VER_MAJOR(v), G_FW_HDR_FW_VER_MINOR(v),
			    G_FW_HDR_FW_VER_MICRO(v), G_FW_HDR_FW_VER_BUILD(v));

			rc = -t4_load_fw(sc, fw->data, fw->datasize);
			if (rc != 0) {
				device_printf(sc->dev,
				    "failed to install firmware: %d\n", rc);
				goto done;
			} else {
				/* refresh */
				(void) t4_check_fw_version(sc);
				snprintf(sc->fw_version,
				    sizeof(sc->fw_version), "%u.%u.%u.%u",
				    G_FW_HDR_FW_VER_MAJOR(sc->params.fw_vers),
				    G_FW_HDR_FW_VER_MINOR(sc->params.fw_vers),
				    G_FW_HDR_FW_VER_MICRO(sc->params.fw_vers),
				    G_FW_HDR_FW_VER_BUILD(sc->params.fw_vers));
			}
		}
	}

	/* Contact firmware.  */
	rc = t4_fw_hello(sc, sc->mbox, sc->mbox, MASTER_MAY, &state);
	if (rc < 0) {
		rc = -rc;
		device_printf(sc->dev,
		    "failed to connect to the firmware: %d.\n", rc);
		goto done;
	}
	if (rc == sc->mbox)
		sc->flags |= MASTER_PF;

	/* Reset device */
	rc = -t4_fw_reset(sc, sc->mbox, F_PIORSTMODE | F_PIORST);
	if (rc != 0) {
		device_printf(sc->dev, "firmware reset failed: %d.\n", rc);
		if (rc != ETIMEDOUT && rc != EIO)
			t4_fw_bye(sc, sc->mbox);
		goto done;
	}

	/* Partition adapter resources as specified in the config file. */
	if (sc->flags & MASTER_PF) {
		if (strncmp(t4_cfg_file, "default", sizeof(t4_cfg_file))) {
			char s[32];

			snprintf(s, sizeof(s), "t4fw_cfg_%s", t4_cfg_file);
			cfg = firmware_get(s);
			if (cfg == NULL) {
				device_printf(sc->dev,
				    "unable to locate %s module, "
				    "will use default config file.\n", s);
			}
		}

		rc = partition_resources(sc, cfg ? cfg : default_cfg);
		if (rc != 0)
			goto done;	/* error message displayed already */
	}

	sc->flags |= FW_OK;

done:
	if (fw != NULL)
		firmware_put(fw, FIRMWARE_UNLOAD);
	if (cfg != NULL)
		firmware_put(cfg, FIRMWARE_UNLOAD);
	if (default_cfg != NULL)
		firmware_put(default_cfg, FIRMWARE_UNLOAD);

	return (rc);
}

#define FW_PARAM_DEV(param) \
	(V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) | \
	 V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_##param))
#define FW_PARAM_PFVF(param) \
	(V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_PFVF) | \
	 V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_PFVF_##param))

/*
 * Upload configuration file to card's memory.
 */
static int
upload_config_file(struct adapter *sc, const struct firmware *fw, uint32_t *mt,
    uint32_t *ma)
{
	int rc, i;
	uint32_t param, val, mtype, maddr, bar, off, win, remaining;
	const uint32_t *b;

	/* Figure out where the firmware wants us to upload it. */
	param = FW_PARAM_DEV(CF);
	rc = -t4_query_params(sc, sc->mbox, sc->pf, 0, 1, &param, &val);
	if (rc != 0) {
		/* Firmwares without config file support will fail this way */
		device_printf(sc->dev,
		    "failed to query config file location: %d.\n", rc);
		return (rc);
	}
	*mt = mtype = G_FW_PARAMS_PARAM_Y(val);
	*ma = maddr = G_FW_PARAMS_PARAM_Z(val) << 16;

	if (maddr & 3) {
		device_printf(sc->dev,
		    "cannot upload config file (type %u, addr %x).\n",
		    mtype, maddr);
		return (EFAULT);
	}

	/* Translate mtype/maddr to an address suitable for the PCIe window */
	val = t4_read_reg(sc, A_MA_TARGET_MEM_ENABLE);
	val &= F_EDRAM0_ENABLE | F_EDRAM1_ENABLE | F_EXT_MEM_ENABLE;
	switch (mtype) {
	case FW_MEMTYPE_CF_EDC0:
		if (!(val & F_EDRAM0_ENABLE))
			goto err;
		bar = t4_read_reg(sc, A_MA_EDRAM0_BAR);
		maddr += G_EDRAM0_BASE(bar) << 20;
		break;

	case FW_MEMTYPE_CF_EDC1:
		if (!(val & F_EDRAM1_ENABLE))
			goto err;
		bar = t4_read_reg(sc, A_MA_EDRAM1_BAR);
		maddr += G_EDRAM1_BASE(bar) << 20;
		break;

	case FW_MEMTYPE_CF_EXTMEM:
		if (!(val & F_EXT_MEM_ENABLE))
			goto err;
		bar = t4_read_reg(sc, A_MA_EXT_MEMORY_BAR);
		maddr += G_EXT_MEM_BASE(bar) << 20;
		break;

	default:
err:
		device_printf(sc->dev,
		    "cannot upload config file (type %u, enabled %u).\n",
		    mtype, val);
		return (EFAULT);
	}

	/*
	 * Position the PCIe window (we use memwin2) to the 16B aligned area
	 * just at/before the upload location.
	 */
	win = maddr & ~0xf;
	off = maddr - win;  /* offset from the start of the window. */
	t4_write_reg(sc, PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_OFFSET, 2), win);
	t4_read_reg(sc, PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_OFFSET, 2));

	remaining = fw->datasize;
	if (remaining > FLASH_CFG_MAX_SIZE ||
	    remaining > MEMWIN2_APERTURE - off) {
		device_printf(sc->dev, "cannot upload config file all at once "
		    "(size %u, max %u, room %u).\n",
		    remaining, FLASH_CFG_MAX_SIZE, MEMWIN2_APERTURE - off);
		return (EFBIG);
	}

	/*
	 * XXX: sheer laziness.  We deliberately added 4 bytes of useless
	 * stuffing/comments at the end of the config file so it's ok to simply
	 * throw away the last remaining bytes when the config file is not an
	 * exact multiple of 4.
	 */
	b = fw->data;
	for (i = 0; remaining >= 4; i += 4, remaining -= 4)
		t4_write_reg(sc, MEMWIN2_BASE + off + i, *b++);

	return (rc);
}

/*
 * Partition chip resources for use between various PFs, VFs, etc.  This is done
 * by uploading the firmware configuration file to the adapter and instructing
 * the firmware to process it.
 */
static int
partition_resources(struct adapter *sc, const struct firmware *cfg)
{
	int rc;
	struct fw_caps_config_cmd caps;
	uint32_t mtype, maddr, finicsum, cfcsum;

	rc = cfg ? upload_config_file(sc, cfg, &mtype, &maddr) : ENOENT;
	if (rc != 0) {
		mtype = FW_MEMTYPE_CF_FLASH;
		maddr = t4_flash_cfg_addr(sc);
	}

	bzero(&caps, sizeof(caps));
	caps.op_to_write = htobe32(V_FW_CMD_OP(FW_CAPS_CONFIG_CMD) |
	    F_FW_CMD_REQUEST | F_FW_CMD_READ);
	caps.cfvalid_to_len16 = htobe32(F_FW_CAPS_CONFIG_CMD_CFVALID |
	    V_FW_CAPS_CONFIG_CMD_MEMTYPE_CF(mtype) |
	    V_FW_CAPS_CONFIG_CMD_MEMADDR64K_CF(maddr >> 16) | FW_LEN16(caps));
	rc = -t4_wr_mbox(sc, sc->mbox, &caps, sizeof(caps), &caps);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to pre-process config file: %d.\n", rc);
		return (rc);
	}

	finicsum = be32toh(caps.finicsum);
	cfcsum = be32toh(caps.cfcsum);
	if (finicsum != cfcsum) {
		device_printf(sc->dev,
		    "WARNING: config file checksum mismatch: %08x %08x\n",
		    finicsum, cfcsum);
	}
	sc->cfcsum = cfcsum;

#define LIMIT_CAPS(x) do { \
	caps.x &= htobe16(t4_##x##_allowed); \
	sc->x = htobe16(caps.x); \
} while (0)

	/*
	 * Let the firmware know what features will (not) be used so it can tune
	 * things accordingly.
	 */
	LIMIT_CAPS(linkcaps);
	LIMIT_CAPS(niccaps);
	LIMIT_CAPS(toecaps);
	LIMIT_CAPS(rdmacaps);
	LIMIT_CAPS(iscsicaps);
	LIMIT_CAPS(fcoecaps);
#undef LIMIT_CAPS

	caps.op_to_write = htobe32(V_FW_CMD_OP(FW_CAPS_CONFIG_CMD) |
	    F_FW_CMD_REQUEST | F_FW_CMD_WRITE);
	caps.cfvalid_to_len16 = htobe32(FW_LEN16(caps));
	rc = -t4_wr_mbox(sc, sc->mbox, &caps, sizeof(caps), NULL);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to process config file: %d.\n", rc);
		return (rc);
	}

	return (0);
}

/*
 * Retrieve parameters that are needed (or nice to have) prior to calling
 * t4_sge_init and t4_fw_initialize.
 */
static int
get_params__pre_init(struct adapter *sc)
{
	int rc;
	uint32_t param[2], val[2];
	struct fw_devlog_cmd cmd;
	struct devlog_params *dlog = &sc->params.devlog;

	param[0] = FW_PARAM_DEV(PORTVEC);
	param[1] = FW_PARAM_DEV(CCLK);
	rc = -t4_query_params(sc, sc->mbox, sc->pf, 0, 2, param, val);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to query parameters (pre_init): %d.\n", rc);
		return (rc);
	}

	sc->params.portvec = val[0];
	sc->params.nports = 0;
	while (val[0]) {
		sc->params.nports++;
		val[0] &= val[0] - 1;
	}

	sc->params.vpd.cclk = val[1];

	/* Read device log parameters. */
	bzero(&cmd, sizeof(cmd));
	cmd.op_to_write = htobe32(V_FW_CMD_OP(FW_DEVLOG_CMD) |
	    F_FW_CMD_REQUEST | F_FW_CMD_READ);
	cmd.retval_len16 = htobe32(FW_LEN16(cmd));
	rc = -t4_wr_mbox(sc, sc->mbox, &cmd, sizeof(cmd), &cmd);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to get devlog parameters: %d.\n", rc);
		bzero(dlog, sizeof (*dlog));
		rc = 0;	/* devlog isn't critical for device operation */
	} else {
		val[0] = be32toh(cmd.memtype_devlog_memaddr16_devlog);
		dlog->memtype = G_FW_DEVLOG_CMD_MEMTYPE_DEVLOG(val[0]);
		dlog->start = G_FW_DEVLOG_CMD_MEMADDR16_DEVLOG(val[0]) << 4;
		dlog->size = be32toh(cmd.memsize_devlog);
	}

	return (rc);
}

/*
 * Retrieve various parameters that are of interest to the driver.  The device
 * has been initialized by the firmware at this point.
 */
static int
get_params__post_init(struct adapter *sc)
{
	int rc;
	uint32_t param[7], val[7];
	struct fw_caps_config_cmd caps;

	param[0] = FW_PARAM_PFVF(IQFLINT_START);
	param[1] = FW_PARAM_PFVF(EQ_START);
	param[2] = FW_PARAM_PFVF(FILTER_START);
	param[3] = FW_PARAM_PFVF(FILTER_END);
	rc = -t4_query_params(sc, sc->mbox, sc->pf, 0, 4, param, val);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to query parameters (post_init): %d.\n", rc);
		return (rc);
	}

	sc->sge.iq_start = val[0];
	sc->sge.eq_start = val[1];
	sc->tids.ftid_base = val[2];
	sc->tids.nftids = val[3] - val[2] + 1;

	/* get capabilites */
	bzero(&caps, sizeof(caps));
	caps.op_to_write = htobe32(V_FW_CMD_OP(FW_CAPS_CONFIG_CMD) |
	    F_FW_CMD_REQUEST | F_FW_CMD_READ);
	caps.cfvalid_to_len16 = htobe32(FW_LEN16(caps));
	rc = -t4_wr_mbox(sc, sc->mbox, &caps, sizeof(caps), &caps);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to get card capabilities: %d.\n", rc);
		return (rc);
	}

	if (caps.toecaps) {
		/* query offload-related parameters */
		param[0] = FW_PARAM_DEV(NTID);
		param[1] = FW_PARAM_PFVF(SERVER_START);
		param[2] = FW_PARAM_PFVF(SERVER_END);
		param[3] = FW_PARAM_PFVF(TDDP_START);
		param[4] = FW_PARAM_PFVF(TDDP_END);
		param[5] = FW_PARAM_DEV(FLOWC_BUFFIFO_SZ);
		rc = -t4_query_params(sc, sc->mbox, sc->pf, 0, 6, param, val);
		if (rc != 0) {
			device_printf(sc->dev,
			    "failed to query TOE parameters: %d.\n", rc);
			return (rc);
		}
		sc->tids.ntids = val[0];
		sc->tids.natids = min(sc->tids.ntids / 2, MAX_ATIDS);
		sc->tids.stid_base = val[1];
		sc->tids.nstids = val[2] - val[1] + 1;
		sc->vres.ddp.start = val[3];
		sc->vres.ddp.size = val[4] - val[3] + 1;
		sc->params.ofldq_wr_cred = val[5];
		sc->params.offload = 1;
	}
	if (caps.rdmacaps) {
		param[0] = FW_PARAM_PFVF(STAG_START);
		param[1] = FW_PARAM_PFVF(STAG_END);
		param[2] = FW_PARAM_PFVF(RQ_START);
		param[3] = FW_PARAM_PFVF(RQ_END);
		param[4] = FW_PARAM_PFVF(PBL_START);
		param[5] = FW_PARAM_PFVF(PBL_END);
		rc = -t4_query_params(sc, sc->mbox, sc->pf, 0, 6, param, val);
		if (rc != 0) {
			device_printf(sc->dev,
			    "failed to query RDMA parameters(1): %d.\n", rc);
			return (rc);
		}
		sc->vres.stag.start = val[0];
		sc->vres.stag.size = val[1] - val[0] + 1;
		sc->vres.rq.start = val[2];
		sc->vres.rq.size = val[3] - val[2] + 1;
		sc->vres.pbl.start = val[4];
		sc->vres.pbl.size = val[5] - val[4] + 1;

		param[0] = FW_PARAM_PFVF(SQRQ_START);
		param[1] = FW_PARAM_PFVF(SQRQ_END);
		param[2] = FW_PARAM_PFVF(CQ_START);
		param[3] = FW_PARAM_PFVF(CQ_END);
		param[4] = FW_PARAM_PFVF(OCQ_START);
		param[5] = FW_PARAM_PFVF(OCQ_END);
		rc = -t4_query_params(sc, 0, 0, 0, 6, param, val);
		if (rc != 0) {
			device_printf(sc->dev,
			    "failed to query RDMA parameters(2): %d.\n", rc);
			return (rc);
		}
		sc->vres.qp.start = val[0];
		sc->vres.qp.size = val[1] - val[0] + 1;
		sc->vres.cq.start = val[2];
		sc->vres.cq.size = val[3] - val[2] + 1;
		sc->vres.ocq.start = val[4];
		sc->vres.ocq.size = val[5] - val[4] + 1;
	}
	if (caps.iscsicaps) {
		param[0] = FW_PARAM_PFVF(ISCSI_START);
		param[1] = FW_PARAM_PFVF(ISCSI_END);
		rc = -t4_query_params(sc, sc->mbox, sc->pf, 0, 2, param, val);
		if (rc != 0) {
			device_printf(sc->dev,
			    "failed to query iSCSI parameters: %d.\n", rc);
			return (rc);
		}
		sc->vres.iscsi.start = val[0];
		sc->vres.iscsi.size = val[1] - val[0] + 1;
	}

	/* These are finalized by FW initialization, load their values now */
	val[0] = t4_read_reg(sc, A_TP_TIMER_RESOLUTION);
	sc->params.tp.tre = G_TIMERRESOLUTION(val[0]);
	sc->params.tp.dack_re = G_DELAYEDACKRESOLUTION(val[0]);
	t4_read_mtu_tbl(sc, sc->params.mtus, NULL);

	return (rc);
}

#undef FW_PARAM_PFVF
#undef FW_PARAM_DEV

static void
t4_set_desc(struct adapter *sc)
{
	char buf[128];
	struct adapter_params *p = &sc->params;

	snprintf(buf, sizeof(buf), "Chelsio %s %sNIC (rev %d), S/N:%s, E/C:%s",
	    p->vpd.id, is_offload(sc) ? "R" : "", p->rev, p->vpd.sn, p->vpd.ec);

	device_set_desc_copy(sc->dev, buf);
}

static void
build_medialist(struct port_info *pi)
{
	struct ifmedia *media = &pi->media;
	int data, m;

	PORT_LOCK(pi);

	ifmedia_removeall(media);

	m = IFM_ETHER | IFM_FDX;
	data = (pi->port_type << 8) | pi->mod_type;

	switch(pi->port_type) {
	case FW_PORT_TYPE_BT_XFI:
		ifmedia_add(media, m | IFM_10G_T, data, NULL);
		break;

	case FW_PORT_TYPE_BT_XAUI:
		ifmedia_add(media, m | IFM_10G_T, data, NULL);
		/* fall through */

	case FW_PORT_TYPE_BT_SGMII:
		ifmedia_add(media, m | IFM_1000_T, data, NULL);
		ifmedia_add(media, m | IFM_100_TX, data, NULL);
		ifmedia_add(media, IFM_ETHER | IFM_AUTO, data, NULL);
		ifmedia_set(media, IFM_ETHER | IFM_AUTO);
		break;

	case FW_PORT_TYPE_CX4:
		ifmedia_add(media, m | IFM_10G_CX4, data, NULL);
		ifmedia_set(media, m | IFM_10G_CX4);
		break;

	case FW_PORT_TYPE_SFP:
	case FW_PORT_TYPE_FIBER_XFI:
	case FW_PORT_TYPE_FIBER_XAUI:
		switch (pi->mod_type) {

		case FW_PORT_MOD_TYPE_LR:
			ifmedia_add(media, m | IFM_10G_LR, data, NULL);
			ifmedia_set(media, m | IFM_10G_LR);
			break;

		case FW_PORT_MOD_TYPE_SR:
			ifmedia_add(media, m | IFM_10G_SR, data, NULL);
			ifmedia_set(media, m | IFM_10G_SR);
			break;

		case FW_PORT_MOD_TYPE_LRM:
			ifmedia_add(media, m | IFM_10G_LRM, data, NULL);
			ifmedia_set(media, m | IFM_10G_LRM);
			break;

		case FW_PORT_MOD_TYPE_TWINAX_PASSIVE:
		case FW_PORT_MOD_TYPE_TWINAX_ACTIVE:
			ifmedia_add(media, m | IFM_10G_TWINAX, data, NULL);
			ifmedia_set(media, m | IFM_10G_TWINAX);
			break;

		case FW_PORT_MOD_TYPE_NONE:
			m &= ~IFM_FDX;
			ifmedia_add(media, m | IFM_NONE, data, NULL);
			ifmedia_set(media, m | IFM_NONE);
			break;

		case FW_PORT_MOD_TYPE_NA:
		case FW_PORT_MOD_TYPE_ER:
		default:
			ifmedia_add(media, m | IFM_UNKNOWN, data, NULL);
			ifmedia_set(media, m | IFM_UNKNOWN);
			break;
		}
		break;

	case FW_PORT_TYPE_KX4:
	case FW_PORT_TYPE_KX:
	case FW_PORT_TYPE_KR:
	default:
		ifmedia_add(media, m | IFM_UNKNOWN, data, NULL);
		ifmedia_set(media, m | IFM_UNKNOWN);
		break;
	}

	PORT_UNLOCK(pi);
}

#define FW_MAC_EXACT_CHUNK	7

/*
 * Program the port's XGMAC based on parameters in ifnet.  The caller also
 * indicates which parameters should be programmed (the rest are left alone).
 */
static int
update_mac_settings(struct port_info *pi, int flags)
{
	int rc;
	struct ifnet *ifp = pi->ifp;
	struct adapter *sc = pi->adapter;
	int mtu = -1, promisc = -1, allmulti = -1, vlanex = -1;

	PORT_LOCK_ASSERT_OWNED(pi);
	KASSERT(flags, ("%s: not told what to update.", __func__));

	if (flags & XGMAC_MTU)
		mtu = ifp->if_mtu;

	if (flags & XGMAC_PROMISC)
		promisc = ifp->if_flags & IFF_PROMISC ? 1 : 0;

	if (flags & XGMAC_ALLMULTI)
		allmulti = ifp->if_flags & IFF_ALLMULTI ? 1 : 0;

	if (flags & XGMAC_VLANEX)
		vlanex = ifp->if_capenable & IFCAP_VLAN_HWTAGGING ? 1 : 0;

	rc = -t4_set_rxmode(sc, sc->mbox, pi->viid, mtu, promisc, allmulti, 1,
	    vlanex, false);
	if (rc) {
		if_printf(ifp, "set_rxmode (%x) failed: %d\n", flags, rc);
		return (rc);
	}

	if (flags & XGMAC_UCADDR) {
		uint8_t ucaddr[ETHER_ADDR_LEN];

		bcopy(IF_LLADDR(ifp), ucaddr, sizeof(ucaddr));
		rc = t4_change_mac(sc, sc->mbox, pi->viid, pi->xact_addr_filt,
		    ucaddr, true, true);
		if (rc < 0) {
			rc = -rc;
			if_printf(ifp, "change_mac failed: %d\n", rc);
			return (rc);
		} else {
			pi->xact_addr_filt = rc;
			rc = 0;
		}
	}

	if (flags & XGMAC_MCADDRS) {
		const uint8_t *mcaddr[FW_MAC_EXACT_CHUNK];
		int del = 1;
		uint64_t hash = 0;
		struct ifmultiaddr *ifma;
		int i = 0, j;

		if_maddr_rlock(ifp);
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
			mcaddr[i++] =
			    LLADDR((struct sockaddr_dl *)ifma->ifma_addr);

			if (i == FW_MAC_EXACT_CHUNK) {
				rc = t4_alloc_mac_filt(sc, sc->mbox, pi->viid,
				    del, i, mcaddr, NULL, &hash, 0);
				if (rc < 0) {
					rc = -rc;
					for (j = 0; j < i; j++) {
						if_printf(ifp,
						    "failed to add mc address"
						    " %02x:%02x:%02x:"
						    "%02x:%02x:%02x rc=%d\n",
						    mcaddr[j][0], mcaddr[j][1],
						    mcaddr[j][2], mcaddr[j][3],
						    mcaddr[j][4], mcaddr[j][5],
						    rc);
					}
					goto mcfail;
				}
				del = 0;
				i = 0;
			}
		}
		if (i > 0) {
			rc = t4_alloc_mac_filt(sc, sc->mbox, pi->viid,
			    del, i, mcaddr, NULL, &hash, 0);
			if (rc < 0) {
				rc = -rc;
				for (j = 0; j < i; j++) {
					if_printf(ifp,
					    "failed to add mc address"
					    " %02x:%02x:%02x:"
					    "%02x:%02x:%02x rc=%d\n",
					    mcaddr[j][0], mcaddr[j][1],
					    mcaddr[j][2], mcaddr[j][3],
					    mcaddr[j][4], mcaddr[j][5],
					    rc);
				}
				goto mcfail;
			}
		}

		rc = -t4_set_addr_hash(sc, sc->mbox, pi->viid, 0, hash, 0);
		if (rc != 0)
			if_printf(ifp, "failed to set mc address hash: %d", rc);
mcfail:
		if_maddr_runlock(ifp);
	}

	return (rc);
}

static int
cxgbe_init_locked(struct port_info *pi)
{
	struct adapter *sc = pi->adapter;
	int rc = 0;

	ADAPTER_LOCK_ASSERT_OWNED(sc);

	while (!IS_DOOMED(pi) && IS_BUSY(sc)) {
		if (mtx_sleep(&sc->flags, &sc->sc_lock, PCATCH, "t4init", 0)) {
			rc = EINTR;
			goto done;
		}
	}
	if (IS_DOOMED(pi)) {
		rc = ENXIO;
		goto done;
	}
	KASSERT(!IS_BUSY(sc), ("%s: controller busy.", __func__));

	/* Give up the adapter lock, port init code can sleep. */
	SET_BUSY(sc);
	ADAPTER_UNLOCK(sc);

	rc = cxgbe_init_synchronized(pi);

done:
	ADAPTER_LOCK(sc);
	KASSERT(IS_BUSY(sc), ("%s: controller not busy.", __func__));
	CLR_BUSY(sc);
	wakeup_one(&sc->flags);
	ADAPTER_UNLOCK(sc);
	return (rc);
}

static int
cxgbe_init_synchronized(struct port_info *pi)
{
	struct adapter *sc = pi->adapter;
	struct ifnet *ifp = pi->ifp;
	int rc = 0;

	ADAPTER_LOCK_ASSERT_NOTOWNED(sc);

	if (isset(&sc->open_device_map, pi->port_id)) {
		KASSERT(ifp->if_drv_flags & IFF_DRV_RUNNING,
		    ("mismatch between open_device_map and if_drv_flags"));
		return (0);	/* already running */
	}

	if (!(sc->flags & FULL_INIT_DONE) &&
	    ((rc = adapter_full_init(sc)) != 0))
		return (rc);	/* error message displayed already */

	if (!(pi->flags & PORT_INIT_DONE) &&
	    ((rc = port_full_init(pi)) != 0))
		return (rc); /* error message displayed already */

	PORT_LOCK(pi);
	rc = update_mac_settings(pi, XGMAC_ALL);
	PORT_UNLOCK(pi);
	if (rc)
		goto done;	/* error message displayed already */

	rc = -t4_link_start(sc, sc->mbox, pi->tx_chan, &pi->link_cfg);
	if (rc != 0) {
		if_printf(ifp, "start_link failed: %d\n", rc);
		goto done;
	}

	rc = -t4_enable_vi(sc, sc->mbox, pi->viid, true, true);
	if (rc != 0) {
		if_printf(ifp, "enable_vi failed: %d\n", rc);
		goto done;
	}

	/* all ok */
	setbit(&sc->open_device_map, pi->port_id);
	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	callout_reset(&pi->tick, hz, cxgbe_tick, pi);
done:
	if (rc != 0)
		cxgbe_uninit_synchronized(pi);

	return (rc);
}

static int
cxgbe_uninit_locked(struct port_info *pi)
{
	struct adapter *sc = pi->adapter;
	int rc;

	ADAPTER_LOCK_ASSERT_OWNED(sc);

	while (!IS_DOOMED(pi) && IS_BUSY(sc)) {
		if (mtx_sleep(&sc->flags, &sc->sc_lock, PCATCH, "t4uninit", 0)) {
			rc = EINTR;
			goto done;
		}
	}
	if (IS_DOOMED(pi)) {
		rc = ENXIO;
		goto done;
	}
	KASSERT(!IS_BUSY(sc), ("%s: controller busy.", __func__));
	SET_BUSY(sc);
	ADAPTER_UNLOCK(sc);

	rc = cxgbe_uninit_synchronized(pi);

	ADAPTER_LOCK(sc);
	KASSERT(IS_BUSY(sc), ("%s: controller not busy.", __func__));
	CLR_BUSY(sc);
	wakeup_one(&sc->flags);
done:
	ADAPTER_UNLOCK(sc);
	return (rc);
}

/*
 * Idempotent.
 */
static int
cxgbe_uninit_synchronized(struct port_info *pi)
{
	struct adapter *sc = pi->adapter;
	struct ifnet *ifp = pi->ifp;
	int rc;

	ADAPTER_LOCK_ASSERT_NOTOWNED(sc);

	/*
	 * Disable the VI so that all its data in either direction is discarded
	 * by the MPS.  Leave everything else (the queues, interrupts, and 1Hz
	 * tick) intact as the TP can deliver negative advice or data that it's
	 * holding in its RAM (for an offloaded connection) even after the VI is
	 * disabled.
	 */
	rc = -t4_enable_vi(sc, sc->mbox, pi->viid, false, false);
	if (rc) {
		if_printf(ifp, "disable_vi failed: %d\n", rc);
		return (rc);
	}

	clrbit(&sc->open_device_map, pi->port_id);
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;

	pi->link_cfg.link_ok = 0;
	pi->link_cfg.speed = 0;
	t4_os_link_changed(sc, pi->port_id, 0);

	return (0);
}

#define T4_ALLOC_IRQ(sc, irq, rid, handler, arg, name) do { \
	rc = t4_alloc_irq(sc, irq, rid, handler, arg, name); \
	if (rc != 0) \
		goto done; \
} while (0)

static int
adapter_full_init(struct adapter *sc)
{
	int rc, i, rid, p, q;
	char s[8];
	struct irq *irq;
	struct port_info *pi;
	struct sge_rxq *rxq;
#ifdef TCP_OFFLOAD
	struct sge_ofld_rxq *ofld_rxq;
#endif

	ADAPTER_LOCK_ASSERT_NOTOWNED(sc);
	KASSERT((sc->flags & FULL_INIT_DONE) == 0,
	    ("%s: FULL_INIT_DONE already", __func__));

	/*
	 * queues that belong to the adapter (not any particular port).
	 */
	rc = t4_setup_adapter_queues(sc);
	if (rc != 0)
		goto done;

	for (i = 0; i < ARRAY_SIZE(sc->tq); i++) {
		sc->tq[i] = taskqueue_create("t4 taskq", M_NOWAIT,
		    taskqueue_thread_enqueue, &sc->tq[i]);
		if (sc->tq[i] == NULL) {
			device_printf(sc->dev,
			    "failed to allocate task queue %d\n", i);
			rc = ENOMEM;
			goto done;
		}
		taskqueue_start_threads(&sc->tq[i], 1, PI_NET, "%s tq%d",
		    device_get_nameunit(sc->dev), i);
	}

	/*
	 * Setup interrupts.
	 */
	irq = &sc->irq[0];
	rid = sc->intr_type == INTR_INTX ? 0 : 1;
	if (sc->intr_count == 1) {
		KASSERT(!(sc->flags & INTR_DIRECT),
		    ("%s: single interrupt && INTR_DIRECT?", __func__));

		T4_ALLOC_IRQ(sc, irq, rid, t4_intr_all, sc, "all");
	} else {
		/* Multiple interrupts. */
		KASSERT(sc->intr_count >= T4_EXTRA_INTR + sc->params.nports,
		    ("%s: too few intr.", __func__));

		/* The first one is always error intr */
		T4_ALLOC_IRQ(sc, irq, rid, t4_intr_err, sc, "err");
		irq++;
		rid++;

		/* The second one is always the firmware event queue */
		T4_ALLOC_IRQ(sc, irq, rid, t4_intr_evt, &sc->sge.fwq, "evt");
		irq++;
		rid++;

		/*
		 * Note that if INTR_DIRECT is not set then either the NIC rx
		 * queues or (exclusive or) the TOE rx queueus will be taking
		 * direct interrupts.
		 *
		 * There is no need to check for is_offload(sc) as nofldrxq
		 * will be 0 if offload is disabled.
		 */
		for_each_port(sc, p) {
			pi = sc->port[p];

#ifdef TCP_OFFLOAD
			/*
			 * Skip over the NIC queues if they aren't taking direct
			 * interrupts.
			 */
			if (!(sc->flags & INTR_DIRECT) &&
			    pi->nofldrxq > pi->nrxq)
				goto ofld_queues;
#endif
			rxq = &sc->sge.rxq[pi->first_rxq];
			for (q = 0; q < pi->nrxq; q++, rxq++) {
				snprintf(s, sizeof(s), "%d.%d", p, q);
				T4_ALLOC_IRQ(sc, irq, rid, t4_intr, rxq, s);
				irq++;
				rid++;
			}

#ifdef TCP_OFFLOAD
			/*
			 * Skip over the offload queues if they aren't taking
			 * direct interrupts.
			 */
			if (!(sc->flags & INTR_DIRECT))
				continue;
ofld_queues:
			ofld_rxq = &sc->sge.ofld_rxq[pi->first_ofld_rxq];
			for (q = 0; q < pi->nofldrxq; q++, ofld_rxq++) {
				snprintf(s, sizeof(s), "%d,%d", p, q);
				T4_ALLOC_IRQ(sc, irq, rid, t4_intr, ofld_rxq, s);
				irq++;
				rid++;
			}
#endif
		}
	}

	t4_intr_enable(sc);
	sc->flags |= FULL_INIT_DONE;
done:
	if (rc != 0)
		adapter_full_uninit(sc);

	return (rc);
}
#undef T4_ALLOC_IRQ

static int
adapter_full_uninit(struct adapter *sc)
{
	int i;

	ADAPTER_LOCK_ASSERT_NOTOWNED(sc);

	t4_teardown_adapter_queues(sc);

	for (i = 0; i < sc->intr_count; i++)
		t4_free_irq(sc, &sc->irq[i]);

	for (i = 0; i < ARRAY_SIZE(sc->tq) && sc->tq[i]; i++) {
		taskqueue_free(sc->tq[i]);
		sc->tq[i] = NULL;
	}

	sc->flags &= ~FULL_INIT_DONE;

	return (0);
}

static int
port_full_init(struct port_info *pi)
{
	struct adapter *sc = pi->adapter;
	struct ifnet *ifp = pi->ifp;
	uint16_t *rss;
	struct sge_rxq *rxq;
	int rc, i;

	ADAPTER_LOCK_ASSERT_NOTOWNED(sc);
	KASSERT((pi->flags & PORT_INIT_DONE) == 0,
	    ("%s: PORT_INIT_DONE already", __func__));

	sysctl_ctx_init(&pi->ctx);
	pi->flags |= PORT_SYSCTL_CTX;

	/*
	 * Allocate tx/rx/fl queues for this port.
	 */
	rc = t4_setup_port_queues(pi);
	if (rc != 0)
		goto done;	/* error message displayed already */

	/*
	 * Setup RSS for this port.
	 */
	rss = malloc(pi->nrxq * sizeof (*rss), M_CXGBE,
	    M_ZERO | M_WAITOK);
	for_each_rxq(pi, i, rxq) {
		rss[i] = rxq->iq.abs_id;
	}
	rc = -t4_config_rss_range(sc, sc->mbox, pi->viid, 0,
	    pi->rss_size, rss, pi->nrxq);
	free(rss, M_CXGBE);
	if (rc != 0) {
		if_printf(ifp, "rss_config failed: %d\n", rc);
		goto done;
	}

	pi->flags |= PORT_INIT_DONE;
done:
	if (rc != 0)
		port_full_uninit(pi);

	return (rc);
}

/*
 * Idempotent.
 */
static int
port_full_uninit(struct port_info *pi)
{
	struct adapter *sc = pi->adapter;
	int i;
	struct sge_rxq *rxq;
	struct sge_txq *txq;
#ifdef TCP_OFFLOAD
	struct sge_ofld_rxq *ofld_rxq;
	struct sge_wrq *ofld_txq;
#endif

	if (pi->flags & PORT_INIT_DONE) {

		/* Need to quiesce queues.  XXX: ctrl queues? */

		for_each_txq(pi, i, txq) {
			quiesce_eq(sc, &txq->eq);
		}

#ifdef TCP_OFFLOAD
		for_each_ofld_txq(pi, i, ofld_txq) {
			quiesce_eq(sc, &ofld_txq->eq);
		}
#endif

		for_each_rxq(pi, i, rxq) {
			quiesce_iq(sc, &rxq->iq);
			quiesce_fl(sc, &rxq->fl);
		}

#ifdef TCP_OFFLOAD
		for_each_ofld_rxq(pi, i, ofld_rxq) {
			quiesce_iq(sc, &ofld_rxq->iq);
			quiesce_fl(sc, &ofld_rxq->fl);
		}
#endif
	}

	t4_teardown_port_queues(pi);
	pi->flags &= ~PORT_INIT_DONE;

	return (0);
}

static void
quiesce_eq(struct adapter *sc, struct sge_eq *eq)
{
	EQ_LOCK(eq);
	eq->flags |= EQ_DOOMED;

	/*
	 * Wait for the response to a credit flush if one's
	 * pending.
	 */
	while (eq->flags & EQ_CRFLUSHED)
		mtx_sleep(eq, &eq->eq_lock, 0, "crflush", 0);
	EQ_UNLOCK(eq);

	callout_drain(&eq->tx_callout);	/* XXX: iffy */
	pause("callout", 10);		/* Still iffy */

	taskqueue_drain(sc->tq[eq->tx_chan], &eq->tx_task);
}

static void
quiesce_iq(struct adapter *sc, struct sge_iq *iq)
{
	(void) sc;	/* unused */

	/* Synchronize with the interrupt handler */
	while (!atomic_cmpset_int(&iq->state, IQS_IDLE, IQS_DISABLED))
		pause("iqfree", 1);
}

static void
quiesce_fl(struct adapter *sc, struct sge_fl *fl)
{
	mtx_lock(&sc->sfl_lock);
	FL_LOCK(fl);
	fl->flags |= FL_DOOMED;
	FL_UNLOCK(fl);
	mtx_unlock(&sc->sfl_lock);

	callout_drain(&sc->sfl_callout);
	KASSERT((fl->flags & FL_STARVING) == 0,
	    ("%s: still starving", __func__));
}

static int
t4_alloc_irq(struct adapter *sc, struct irq *irq, int rid,
    driver_intr_t *handler, void *arg, char *name)
{
	int rc;

	irq->rid = rid;
	irq->res = bus_alloc_resource_any(sc->dev, SYS_RES_IRQ, &irq->rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (irq->res == NULL) {
		device_printf(sc->dev,
		    "failed to allocate IRQ for rid %d, name %s.\n", rid, name);
		return (ENOMEM);
	}

	rc = bus_setup_intr(sc->dev, irq->res, INTR_MPSAFE | INTR_TYPE_NET,
	    NULL, handler, arg, &irq->tag);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to setup interrupt for rid %d, name %s: %d\n",
		    rid, name, rc);
	} else if (name)
		bus_describe_intr(sc->dev, irq->res, irq->tag, name);

	return (rc);
}

static int
t4_free_irq(struct adapter *sc, struct irq *irq)
{
	if (irq->tag)
		bus_teardown_intr(sc->dev, irq->res, irq->tag);
	if (irq->res)
		bus_release_resource(sc->dev, SYS_RES_IRQ, irq->rid, irq->res);

	bzero(irq, sizeof(*irq));

	return (0);
}

static void
reg_block_dump(struct adapter *sc, uint8_t *buf, unsigned int start,
    unsigned int end)
{
	uint32_t *p = (uint32_t *)(buf + start);

	for ( ; start <= end; start += sizeof(uint32_t))
		*p++ = t4_read_reg(sc, start);
}

static void
t4_get_regs(struct adapter *sc, struct t4_regdump *regs, uint8_t *buf)
{
	int i;
	static const unsigned int reg_ranges[] = {
		0x1008, 0x1108,
		0x1180, 0x11b4,
		0x11fc, 0x123c,
		0x1300, 0x173c,
		0x1800, 0x18fc,
		0x3000, 0x30d8,
		0x30e0, 0x5924,
		0x5960, 0x59d4,
		0x5a00, 0x5af8,
		0x6000, 0x6098,
		0x6100, 0x6150,
		0x6200, 0x6208,
		0x6240, 0x6248,
		0x6280, 0x6338,
		0x6370, 0x638c,
		0x6400, 0x643c,
		0x6500, 0x6524,
		0x6a00, 0x6a38,
		0x6a60, 0x6a78,
		0x6b00, 0x6b84,
		0x6bf0, 0x6c84,
		0x6cf0, 0x6d84,
		0x6df0, 0x6e84,
		0x6ef0, 0x6f84,
		0x6ff0, 0x7084,
		0x70f0, 0x7184,
		0x71f0, 0x7284,
		0x72f0, 0x7384,
		0x73f0, 0x7450,
		0x7500, 0x7530,
		0x7600, 0x761c,
		0x7680, 0x76cc,
		0x7700, 0x7798,
		0x77c0, 0x77fc,
		0x7900, 0x79fc,
		0x7b00, 0x7c38,
		0x7d00, 0x7efc,
		0x8dc0, 0x8e1c,
		0x8e30, 0x8e78,
		0x8ea0, 0x8f6c,
		0x8fc0, 0x9074,
		0x90fc, 0x90fc,
		0x9400, 0x9458,
		0x9600, 0x96bc,
		0x9800, 0x9808,
		0x9820, 0x983c,
		0x9850, 0x9864,
		0x9c00, 0x9c6c,
		0x9c80, 0x9cec,
		0x9d00, 0x9d6c,
		0x9d80, 0x9dec,
		0x9e00, 0x9e6c,
		0x9e80, 0x9eec,
		0x9f00, 0x9f6c,
		0x9f80, 0x9fec,
		0xd004, 0xd03c,
		0xdfc0, 0xdfe0,
		0xe000, 0xea7c,
		0xf000, 0x11190,
		0x19040, 0x1906c,
		0x19078, 0x19080,
		0x1908c, 0x19124,
		0x19150, 0x191b0,
		0x191d0, 0x191e8,
		0x19238, 0x1924c,
		0x193f8, 0x19474,
		0x19490, 0x194f8,
		0x19800, 0x19f30,
		0x1a000, 0x1a06c,
		0x1a0b0, 0x1a120,
		0x1a128, 0x1a138,
		0x1a190, 0x1a1c4,
		0x1a1fc, 0x1a1fc,
		0x1e040, 0x1e04c,
		0x1e284, 0x1e28c,
		0x1e2c0, 0x1e2c0,
		0x1e2e0, 0x1e2e0,
		0x1e300, 0x1e384,
		0x1e3c0, 0x1e3c8,
		0x1e440, 0x1e44c,
		0x1e684, 0x1e68c,
		0x1e6c0, 0x1e6c0,
		0x1e6e0, 0x1e6e0,
		0x1e700, 0x1e784,
		0x1e7c0, 0x1e7c8,
		0x1e840, 0x1e84c,
		0x1ea84, 0x1ea8c,
		0x1eac0, 0x1eac0,
		0x1eae0, 0x1eae0,
		0x1eb00, 0x1eb84,
		0x1ebc0, 0x1ebc8,
		0x1ec40, 0x1ec4c,
		0x1ee84, 0x1ee8c,
		0x1eec0, 0x1eec0,
		0x1eee0, 0x1eee0,
		0x1ef00, 0x1ef84,
		0x1efc0, 0x1efc8,
		0x1f040, 0x1f04c,
		0x1f284, 0x1f28c,
		0x1f2c0, 0x1f2c0,
		0x1f2e0, 0x1f2e0,
		0x1f300, 0x1f384,
		0x1f3c0, 0x1f3c8,
		0x1f440, 0x1f44c,
		0x1f684, 0x1f68c,
		0x1f6c0, 0x1f6c0,
		0x1f6e0, 0x1f6e0,
		0x1f700, 0x1f784,
		0x1f7c0, 0x1f7c8,
		0x1f840, 0x1f84c,
		0x1fa84, 0x1fa8c,
		0x1fac0, 0x1fac0,
		0x1fae0, 0x1fae0,
		0x1fb00, 0x1fb84,
		0x1fbc0, 0x1fbc8,
		0x1fc40, 0x1fc4c,
		0x1fe84, 0x1fe8c,
		0x1fec0, 0x1fec0,
		0x1fee0, 0x1fee0,
		0x1ff00, 0x1ff84,
		0x1ffc0, 0x1ffc8,
		0x20000, 0x2002c,
		0x20100, 0x2013c,
		0x20190, 0x201c8,
		0x20200, 0x20318,
		0x20400, 0x20528,
		0x20540, 0x20614,
		0x21000, 0x21040,
		0x2104c, 0x21060,
		0x210c0, 0x210ec,
		0x21200, 0x21268,
		0x21270, 0x21284,
		0x212fc, 0x21388,
		0x21400, 0x21404,
		0x21500, 0x21518,
		0x2152c, 0x2153c,
		0x21550, 0x21554,
		0x21600, 0x21600,
		0x21608, 0x21628,
		0x21630, 0x2163c,
		0x21700, 0x2171c,
		0x21780, 0x2178c,
		0x21800, 0x21c38,
		0x21c80, 0x21d7c,
		0x21e00, 0x21e04,
		0x22000, 0x2202c,
		0x22100, 0x2213c,
		0x22190, 0x221c8,
		0x22200, 0x22318,
		0x22400, 0x22528,
		0x22540, 0x22614,
		0x23000, 0x23040,
		0x2304c, 0x23060,
		0x230c0, 0x230ec,
		0x23200, 0x23268,
		0x23270, 0x23284,
		0x232fc, 0x23388,
		0x23400, 0x23404,
		0x23500, 0x23518,
		0x2352c, 0x2353c,
		0x23550, 0x23554,
		0x23600, 0x23600,
		0x23608, 0x23628,
		0x23630, 0x2363c,
		0x23700, 0x2371c,
		0x23780, 0x2378c,
		0x23800, 0x23c38,
		0x23c80, 0x23d7c,
		0x23e00, 0x23e04,
		0x24000, 0x2402c,
		0x24100, 0x2413c,
		0x24190, 0x241c8,
		0x24200, 0x24318,
		0x24400, 0x24528,
		0x24540, 0x24614,
		0x25000, 0x25040,
		0x2504c, 0x25060,
		0x250c0, 0x250ec,
		0x25200, 0x25268,
		0x25270, 0x25284,
		0x252fc, 0x25388,
		0x25400, 0x25404,
		0x25500, 0x25518,
		0x2552c, 0x2553c,
		0x25550, 0x25554,
		0x25600, 0x25600,
		0x25608, 0x25628,
		0x25630, 0x2563c,
		0x25700, 0x2571c,
		0x25780, 0x2578c,
		0x25800, 0x25c38,
		0x25c80, 0x25d7c,
		0x25e00, 0x25e04,
		0x26000, 0x2602c,
		0x26100, 0x2613c,
		0x26190, 0x261c8,
		0x26200, 0x26318,
		0x26400, 0x26528,
		0x26540, 0x26614,
		0x27000, 0x27040,
		0x2704c, 0x27060,
		0x270c0, 0x270ec,
		0x27200, 0x27268,
		0x27270, 0x27284,
		0x272fc, 0x27388,
		0x27400, 0x27404,
		0x27500, 0x27518,
		0x2752c, 0x2753c,
		0x27550, 0x27554,
		0x27600, 0x27600,
		0x27608, 0x27628,
		0x27630, 0x2763c,
		0x27700, 0x2771c,
		0x27780, 0x2778c,
		0x27800, 0x27c38,
		0x27c80, 0x27d7c,
		0x27e00, 0x27e04
	};

	regs->version = 4 | (sc->params.rev << 10);
	for (i = 0; i < ARRAY_SIZE(reg_ranges); i += 2)
		reg_block_dump(sc, buf, reg_ranges[i], reg_ranges[i + 1]);
}

static void
cxgbe_tick(void *arg)
{
	struct port_info *pi = arg;
	struct ifnet *ifp = pi->ifp;
	struct sge_txq *txq;
	int i, drops;
	struct port_stats *s = &pi->stats;

	PORT_LOCK(pi);
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		PORT_UNLOCK(pi);
		return;	/* without scheduling another callout */
	}

	t4_get_port_stats(pi->adapter, pi->tx_chan, s);

	ifp->if_opackets = s->tx_frames - s->tx_pause;
	ifp->if_ipackets = s->rx_frames - s->rx_pause;
	ifp->if_obytes = s->tx_octets - s->tx_pause * 64;
	ifp->if_ibytes = s->rx_octets - s->rx_pause * 64;
	ifp->if_omcasts = s->tx_mcast_frames - s->tx_pause;
	ifp->if_imcasts = s->rx_mcast_frames - s->rx_pause;
	ifp->if_iqdrops = s->rx_ovflow0 + s->rx_ovflow1 + s->rx_ovflow2 +
	    s->rx_ovflow3;

	drops = s->tx_drop;
	for_each_txq(pi, i, txq)
		drops += txq->br->br_drops;
	ifp->if_snd.ifq_drops = drops;

	ifp->if_oerrors = s->tx_error_frames;
	ifp->if_ierrors = s->rx_jabber + s->rx_runt + s->rx_too_long +
	    s->rx_fcs_err + s->rx_len_err;

	callout_schedule(&pi->tick, hz);
	PORT_UNLOCK(pi);
}

static void
cxgbe_vlan_config(void *arg, struct ifnet *ifp, uint16_t vid)
{
	struct ifnet *vlan;

	if (arg != ifp)
		return;

	vlan = VLAN_DEVAT(ifp, vid);
	VLAN_SETCOOKIE(vlan, ifp);
}

static int
cpl_not_handled(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{

#ifdef INVARIANTS
	panic("%s: opcode 0x%02x on iq %p with payload %p",
	    __func__, rss->opcode, iq, m);
#else
	log(LOG_ERR, "%s: opcode 0x%02x on iq %p with payload %p",
	    __func__, rss->opcode, iq, m);
	m_freem(m);
#endif
	return (EDOOFUS);
}

int
t4_register_cpl_handler(struct adapter *sc, int opcode, cpl_handler_t h)
{
	uintptr_t *loc, new;

	if (opcode >= ARRAY_SIZE(sc->cpl_handler))
		return (EINVAL);

	new = h ? (uintptr_t)h : (uintptr_t)cpl_not_handled;
	loc = (uintptr_t *) &sc->cpl_handler[opcode];
	atomic_store_rel_ptr(loc, new);

	return (0);
}

static int
an_not_handled(struct sge_iq *iq, const struct rsp_ctrl *ctrl)
{

#ifdef INVARIANTS
	panic("%s: async notification on iq %p (ctrl %p)", __func__, iq, ctrl);
#else
	log(LOG_ERR, "%s: async notification on iq %p (ctrl %p)",
	    __func__, iq, ctrl);
#endif
	return (EDOOFUS);
}

int
t4_register_an_handler(struct adapter *sc, an_handler_t h)
{
	uintptr_t *loc, new;

	new = h ? (uintptr_t)h : (uintptr_t)an_not_handled;
	loc = (uintptr_t *) &sc->an_handler;
	atomic_store_rel_ptr(loc, new);

	return (0);
}

static int
t4_sysctls(struct adapter *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *oid;
	struct sysctl_oid_list *children, *c0;
	static char *caps[] = {
		"\20\1PPP\2QFC\3DCBX",			/* caps[0] linkcaps */
		"\20\1NIC\2VM\3IDS\4UM\5UM_ISGL",	/* caps[1] niccaps */
		"\20\1TOE",				/* caps[2] toecaps */
		"\20\1RDDP\2RDMAC",			/* caps[3] rdmacaps */
		"\20\1INITIATOR_PDU\2TARGET_PDU"	/* caps[4] iscsicaps */
		    "\3INITIATOR_CNXOFLD\4TARGET_CNXOFLD"
		    "\5INITIATOR_SSNOFLD\6TARGET_SSNOFLD",
		"\20\1INITIATOR\2TARGET\3CTRL_OFLD"	/* caps[5] fcoecaps */
	};

	ctx = device_get_sysctl_ctx(sc->dev);

	/*
	 * dev.t4nex.X.
	 */
	oid = device_get_sysctl_tree(sc->dev);
	c0 = children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "nports", CTLFLAG_RD,
	    &sc->params.nports, 0, "# of ports");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "hw_revision", CTLFLAG_RD,
	    &sc->params.rev, 0, "chip hardware revision");

	SYSCTL_ADD_STRING(ctx, children, OID_AUTO, "firmware_version",
	    CTLFLAG_RD, &sc->fw_version, 0, "firmware version");

	SYSCTL_ADD_STRING(ctx, children, OID_AUTO, "cf",
	    CTLFLAG_RD, &t4_cfg_file, 0, "configuration file");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "cfcsum", CTLFLAG_RD,
	    &sc->cfcsum, 0, "config file checksum");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "linkcaps",
	    CTLTYPE_STRING | CTLFLAG_RD, caps[0], sc->linkcaps,
	    sysctl_bitfield, "A", "available link capabilities");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "niccaps",
	    CTLTYPE_STRING | CTLFLAG_RD, caps[1], sc->niccaps,
	    sysctl_bitfield, "A", "available NIC capabilities");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "toecaps",
	    CTLTYPE_STRING | CTLFLAG_RD, caps[2], sc->toecaps,
	    sysctl_bitfield, "A", "available TCP offload capabilities");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "rdmacaps",
	    CTLTYPE_STRING | CTLFLAG_RD, caps[3], sc->rdmacaps,
	    sysctl_bitfield, "A", "available RDMA capabilities");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "iscsicaps",
	    CTLTYPE_STRING | CTLFLAG_RD, caps[4], sc->iscsicaps,
	    sysctl_bitfield, "A", "available iSCSI capabilities");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "fcoecaps",
	    CTLTYPE_STRING | CTLFLAG_RD, caps[5], sc->fcoecaps,
	    sysctl_bitfield, "A", "available FCoE capabilities");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "core_clock", CTLFLAG_RD,
	    &sc->params.vpd.cclk, 0, "core clock frequency (in KHz)");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "holdoff_timers",
	    CTLTYPE_STRING | CTLFLAG_RD, sc->sge.timer_val,
	    sizeof(sc->sge.timer_val), sysctl_int_array, "A",
	    "interrupt holdoff timer values (us)");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "holdoff_pkt_counts",
	    CTLTYPE_STRING | CTLFLAG_RD, sc->sge.counter_val,
	    sizeof(sc->sge.counter_val), sysctl_int_array, "A",
	    "interrupt holdoff packet counter values");

#ifdef SBUF_DRAIN
	/*
	 * dev.t4nex.X.misc.  Marked CTLFLAG_SKIP to avoid information overload.
	 */
	oid = SYSCTL_ADD_NODE(ctx, c0, OID_AUTO, "misc",
	    CTLFLAG_RD | CTLFLAG_SKIP, NULL,
	    "logs and miscellaneous information");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "cctrl",
	    CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
	    sysctl_cctrl, "A", "congestion control");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "cpl_stats",
	    CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
	    sysctl_cpl_stats, "A", "CPL statistics");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "ddp_stats",
	    CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
	    sysctl_ddp_stats, "A", "DDP statistics");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "devlog",
	    CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
	    sysctl_devlog, "A", "firmware's device log");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "fcoe_stats",
	    CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
	    sysctl_fcoe_stats, "A", "FCoE statistics");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "hw_sched",
	    CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
	    sysctl_hw_sched, "A", "hardware scheduler ");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "l2t",
	    CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
	    sysctl_l2t, "A", "hardware L2 table");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "lb_stats",
	    CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
	    sysctl_lb_stats, "A", "loopback statistics");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "meminfo",
	    CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
	    sysctl_meminfo, "A", "memory regions");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "path_mtus",
	    CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
	    sysctl_path_mtus, "A", "path MTUs");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "pm_stats",
	    CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
	    sysctl_pm_stats, "A", "PM statistics");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "rdma_stats",
	    CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
	    sysctl_rdma_stats, "A", "RDMA statistics");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "tcp_stats",
	    CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
	    sysctl_tcp_stats, "A", "TCP statistics");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "tids",
	    CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
	    sysctl_tids, "A", "TID information");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "tp_err_stats",
	    CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
	    sysctl_tp_err_stats, "A", "TP error statistics");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "tx_rate",
	    CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
	    sysctl_tx_rate, "A", "Tx rate");
#endif

#ifdef TCP_OFFLOAD
	if (is_offload(sc)) {
		/*
		 * dev.t4nex.X.toe.
		 */
		oid = SYSCTL_ADD_NODE(ctx, c0, OID_AUTO, "toe", CTLFLAG_RD,
		    NULL, "TOE parameters");
		children = SYSCTL_CHILDREN(oid);

		sc->tt.sndbuf = 256 * 1024;
		SYSCTL_ADD_INT(ctx, children, OID_AUTO, "sndbuf", CTLFLAG_RW,
		    &sc->tt.sndbuf, 0, "max hardware send buffer size");

		sc->tt.ddp = 0;
		SYSCTL_ADD_INT(ctx, children, OID_AUTO, "ddp", CTLFLAG_RW,
		    &sc->tt.ddp, 0, "DDP allowed");
		sc->tt.indsz = M_INDICATESIZE;
		SYSCTL_ADD_INT(ctx, children, OID_AUTO, "indsz", CTLFLAG_RW,
		    &sc->tt.indsz, 0, "DDP max indicate size allowed");
		sc->tt.ddp_thres = 3*4096;
		SYSCTL_ADD_INT(ctx, children, OID_AUTO, "ddp_thres", CTLFLAG_RW,
		    &sc->tt.ddp_thres, 0, "DDP threshold");
	}
#endif


	return (0);
}

static int
cxgbe_sysctls(struct port_info *pi)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *oid;
	struct sysctl_oid_list *children;

	ctx = device_get_sysctl_ctx(pi->dev);

	/*
	 * dev.cxgbe.X.
	 */
	oid = device_get_sysctl_tree(pi->dev);
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "nrxq", CTLFLAG_RD,
	    &pi->nrxq, 0, "# of rx queues");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "ntxq", CTLFLAG_RD,
	    &pi->ntxq, 0, "# of tx queues");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "first_rxq", CTLFLAG_RD,
	    &pi->first_rxq, 0, "index of first rx queue");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "first_txq", CTLFLAG_RD,
	    &pi->first_txq, 0, "index of first tx queue");

#ifdef TCP_OFFLOAD
	if (is_offload(pi->adapter)) {
		SYSCTL_ADD_INT(ctx, children, OID_AUTO, "nofldrxq", CTLFLAG_RD,
		    &pi->nofldrxq, 0,
		    "# of rx queues for offloaded TCP connections");
		SYSCTL_ADD_INT(ctx, children, OID_AUTO, "nofldtxq", CTLFLAG_RD,
		    &pi->nofldtxq, 0,
		    "# of tx queues for offloaded TCP connections");
		SYSCTL_ADD_INT(ctx, children, OID_AUTO, "first_ofld_rxq",
		    CTLFLAG_RD, &pi->first_ofld_rxq, 0,
		    "index of first TOE rx queue");
		SYSCTL_ADD_INT(ctx, children, OID_AUTO, "first_ofld_txq",
		    CTLFLAG_RD, &pi->first_ofld_txq, 0,
		    "index of first TOE tx queue");
	}
#endif

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "holdoff_tmr_idx",
	    CTLTYPE_INT | CTLFLAG_RW, pi, 0, sysctl_holdoff_tmr_idx, "I",
	    "holdoff timer index");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "holdoff_pktc_idx",
	    CTLTYPE_INT | CTLFLAG_RW, pi, 0, sysctl_holdoff_pktc_idx, "I",
	    "holdoff packet counter index");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "qsize_rxq",
	    CTLTYPE_INT | CTLFLAG_RW, pi, 0, sysctl_qsize_rxq, "I",
	    "rx queue size");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "qsize_txq",
	    CTLTYPE_INT | CTLFLAG_RW, pi, 0, sysctl_qsize_txq, "I",
	    "tx queue size");

	/*
	 * dev.cxgbe.X.stats.
	 */
	oid = SYSCTL_ADD_NODE(ctx, children, OID_AUTO, "stats", CTLFLAG_RD,
	    NULL, "port statistics");
	children = SYSCTL_CHILDREN(oid);

#define SYSCTL_ADD_T4_REG64(pi, name, desc, reg) \
	SYSCTL_ADD_OID(ctx, children, OID_AUTO, name, \
	    CTLTYPE_U64 | CTLFLAG_RD, pi->adapter, reg, \
	    sysctl_handle_t4_reg64, "QU", desc)

	SYSCTL_ADD_T4_REG64(pi, "tx_octets", "# of octets in good frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_BYTES_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_frames", "total # of good frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_FRAMES_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_bcast_frames", "# of broadcast frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_BCAST_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_mcast_frames", "# of multicast frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_MCAST_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_ucast_frames", "# of unicast frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_UCAST_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_error_frames", "# of error frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_ERROR_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_frames_64",
	    "# of tx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_64B_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_frames_65_127",
	    "# of tx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_65B_127B_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_frames_128_255",
	    "# of tx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_128B_255B_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_frames_256_511",
	    "# of tx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_256B_511B_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_frames_512_1023",
	    "# of tx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_512B_1023B_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_frames_1024_1518",
	    "# of tx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_1024B_1518B_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_frames_1519_max",
	    "# of tx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_1519B_MAX_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_drop", "# of dropped tx frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_DROP_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_pause", "# of pause frames transmitted",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_PAUSE_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_ppp0", "# of PPP prio 0 frames transmitted",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_PPP0_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_ppp1", "# of PPP prio 1 frames transmitted",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_PPP1_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_ppp2", "# of PPP prio 2 frames transmitted",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_PPP2_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_ppp3", "# of PPP prio 3 frames transmitted",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_PPP3_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_ppp4", "# of PPP prio 4 frames transmitted",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_PPP4_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_ppp5", "# of PPP prio 5 frames transmitted",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_PPP5_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_ppp6", "# of PPP prio 6 frames transmitted",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_PPP6_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_ppp7", "# of PPP prio 7 frames transmitted",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_PPP7_L));

	SYSCTL_ADD_T4_REG64(pi, "rx_octets", "# of octets in good frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_BYTES_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_frames", "total # of good frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_FRAMES_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_bcast_frames", "# of broadcast frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_BCAST_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_mcast_frames", "# of multicast frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_MCAST_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_ucast_frames", "# of unicast frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_UCAST_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_too_long", "# of frames exceeding MTU",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_MTU_ERROR_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_jabber", "# of jabber frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_MTU_CRC_ERROR_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_fcs_err",
	    "# of frames received with bad FCS",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_CRC_ERROR_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_len_err",
	    "# of frames received with length error",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_LEN_ERROR_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_symbol_err", "symbol errors",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_SYM_ERROR_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_runt", "# of short frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_LESS_64B_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_frames_64",
	    "# of rx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_64B_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_frames_65_127",
	    "# of rx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_65B_127B_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_frames_128_255",
	    "# of rx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_128B_255B_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_frames_256_511",
	    "# of rx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_256B_511B_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_frames_512_1023",
	    "# of rx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_512B_1023B_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_frames_1024_1518",
	    "# of rx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_1024B_1518B_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_frames_1519_max",
	    "# of rx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_1519B_MAX_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_pause", "# of pause frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_PAUSE_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_ppp0", "# of PPP prio 0 frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_PPP0_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_ppp1", "# of PPP prio 1 frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_PPP1_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_ppp2", "# of PPP prio 2 frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_PPP2_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_ppp3", "# of PPP prio 3 frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_PPP3_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_ppp4", "# of PPP prio 4 frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_PPP4_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_ppp5", "# of PPP prio 5 frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_PPP5_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_ppp6", "# of PPP prio 6 frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_PPP6_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_ppp7", "# of PPP prio 7 frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_PPP7_L));

#undef SYSCTL_ADD_T4_REG64

#define SYSCTL_ADD_T4_PORTSTAT(name, desc) \
	SYSCTL_ADD_UQUAD(ctx, children, OID_AUTO, #name, CTLFLAG_RD, \
	    &pi->stats.name, desc)

	/* We get these from port_stats and they may be stale by upto 1s */
	SYSCTL_ADD_T4_PORTSTAT(rx_ovflow0,
	    "# drops due to buffer-group 0 overflows");
	SYSCTL_ADD_T4_PORTSTAT(rx_ovflow1,
	    "# drops due to buffer-group 1 overflows");
	SYSCTL_ADD_T4_PORTSTAT(rx_ovflow2,
	    "# drops due to buffer-group 2 overflows");
	SYSCTL_ADD_T4_PORTSTAT(rx_ovflow3,
	    "# drops due to buffer-group 3 overflows");
	SYSCTL_ADD_T4_PORTSTAT(rx_trunc0,
	    "# of buffer-group 0 truncated packets");
	SYSCTL_ADD_T4_PORTSTAT(rx_trunc1,
	    "# of buffer-group 1 truncated packets");
	SYSCTL_ADD_T4_PORTSTAT(rx_trunc2,
	    "# of buffer-group 2 truncated packets");
	SYSCTL_ADD_T4_PORTSTAT(rx_trunc3,
	    "# of buffer-group 3 truncated packets");

#undef SYSCTL_ADD_T4_PORTSTAT

	return (0);
}

static int
sysctl_int_array(SYSCTL_HANDLER_ARGS)
{
	int rc, *i;
	struct sbuf sb;

	sbuf_new(&sb, NULL, 32, SBUF_AUTOEXTEND);
	for (i = arg1; arg2; arg2 -= sizeof(int), i++)
		sbuf_printf(&sb, "%d ", *i);
	sbuf_trim(&sb);
	sbuf_finish(&sb);
	rc = sysctl_handle_string(oidp, sbuf_data(&sb), sbuf_len(&sb), req);
	sbuf_delete(&sb);
	return (rc);
}

static int
sysctl_bitfield(SYSCTL_HANDLER_ARGS)
{
	int rc;
	struct sbuf *sb;

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return(rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (sb == NULL)
		return (ENOMEM);

	sbuf_printf(sb, "%b", (int)arg2, (char *)arg1);
	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

static int
sysctl_holdoff_tmr_idx(SYSCTL_HANDLER_ARGS)
{
	struct port_info *pi = arg1;
	struct adapter *sc = pi->adapter;
	int idx, rc, i;

	idx = pi->tmr_idx;

	rc = sysctl_handle_int(oidp, &idx, 0, req);
	if (rc != 0 || req->newptr == NULL)
		return (rc);

	if (idx < 0 || idx >= SGE_NTIMERS)
		return (EINVAL);

	ADAPTER_LOCK(sc);
	rc = IS_DOOMED(pi) ? ENXIO : (IS_BUSY(sc) ? EBUSY : 0);
	if (rc == 0) {
		struct sge_rxq *rxq;
		uint8_t v;

		v = V_QINTR_TIMER_IDX(idx) | V_QINTR_CNT_EN(pi->pktc_idx != -1);
		for_each_rxq(pi, i, rxq) {
#ifdef atomic_store_rel_8
			atomic_store_rel_8(&rxq->iq.intr_params, v);
#else
			rxq->iq.intr_params = v;
#endif
		}
		pi->tmr_idx = idx;
	}

	ADAPTER_UNLOCK(sc);
	return (rc);
}

static int
sysctl_holdoff_pktc_idx(SYSCTL_HANDLER_ARGS)
{
	struct port_info *pi = arg1;
	struct adapter *sc = pi->adapter;
	int idx, rc;

	idx = pi->pktc_idx;

	rc = sysctl_handle_int(oidp, &idx, 0, req);
	if (rc != 0 || req->newptr == NULL)
		return (rc);

	if (idx < -1 || idx >= SGE_NCOUNTERS)
		return (EINVAL);

	ADAPTER_LOCK(sc);
	rc = IS_DOOMED(pi) ? ENXIO : (IS_BUSY(sc) ? EBUSY : 0);
	if (rc == 0 && pi->flags & PORT_INIT_DONE)
		rc = EBUSY; /* cannot be changed once the queues are created */

	if (rc == 0)
		pi->pktc_idx = idx;

	ADAPTER_UNLOCK(sc);
	return (rc);
}

static int
sysctl_qsize_rxq(SYSCTL_HANDLER_ARGS)
{
	struct port_info *pi = arg1;
	struct adapter *sc = pi->adapter;
	int qsize, rc;

	qsize = pi->qsize_rxq;

	rc = sysctl_handle_int(oidp, &qsize, 0, req);
	if (rc != 0 || req->newptr == NULL)
		return (rc);

	if (qsize < 128 || (qsize & 7))
		return (EINVAL);

	ADAPTER_LOCK(sc);
	rc = IS_DOOMED(pi) ? ENXIO : (IS_BUSY(sc) ? EBUSY : 0);
	if (rc == 0 && pi->flags & PORT_INIT_DONE)
		rc = EBUSY; /* cannot be changed once the queues are created */

	if (rc == 0)
		pi->qsize_rxq = qsize;

	ADAPTER_UNLOCK(sc);
	return (rc);
}

static int
sysctl_qsize_txq(SYSCTL_HANDLER_ARGS)
{
	struct port_info *pi = arg1;
	struct adapter *sc = pi->adapter;
	int qsize, rc;

	qsize = pi->qsize_txq;

	rc = sysctl_handle_int(oidp, &qsize, 0, req);
	if (rc != 0 || req->newptr == NULL)
		return (rc);

	if (qsize < 128)
		return (EINVAL);

	ADAPTER_LOCK(sc);
	rc = IS_DOOMED(pi) ? ENXIO : (IS_BUSY(sc) ? EBUSY : 0);
	if (rc == 0 && pi->flags & PORT_INIT_DONE)
		rc = EBUSY; /* cannot be changed once the queues are created */

	if (rc == 0)
		pi->qsize_txq = qsize;

	ADAPTER_UNLOCK(sc);
	return (rc);
}

static int
sysctl_handle_t4_reg64(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	int reg = arg2;
	uint64_t val;

	val = t4_read_reg64(sc, reg);

	return (sysctl_handle_64(oidp, &val, 0, req));
}

#ifdef SBUF_DRAIN
static int
sysctl_cctrl(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	struct sbuf *sb;
	int rc, i;
	uint16_t incr[NMTUS][NCCTRL_WIN];
	static const char *dec_fac[] = {
		"0.5", "0.5625", "0.625", "0.6875", "0.75", "0.8125", "0.875",
		"0.9375"
	};

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return (rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 4096, req);
	if (sb == NULL)
		return (ENOMEM);

	t4_read_cong_tbl(sc, incr);

	for (i = 0; i < NCCTRL_WIN; ++i) {
		sbuf_printf(sb, "%2d: %4u %4u %4u %4u %4u %4u %4u %4u\n", i,
		    incr[0][i], incr[1][i], incr[2][i], incr[3][i], incr[4][i],
		    incr[5][i], incr[6][i], incr[7][i]);
		sbuf_printf(sb, "%8u %4u %4u %4u %4u %4u %4u %4u %5u %s\n",
		    incr[8][i], incr[9][i], incr[10][i], incr[11][i],
		    incr[12][i], incr[13][i], incr[14][i], incr[15][i],
		    sc->params.a_wnd[i], dec_fac[sc->params.b_wnd[i]]);
	}

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

static int
sysctl_cpl_stats(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	struct sbuf *sb;
	int rc;
	struct tp_cpl_stats stats;

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return (rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 256, req);
	if (sb == NULL)
		return (ENOMEM);

	t4_tp_get_cpl_stats(sc, &stats);

	sbuf_printf(sb, "                 channel 0  channel 1  channel 2  "
	    "channel 3\n");
	sbuf_printf(sb, "CPL requests:   %10u %10u %10u %10u\n",
		   stats.req[0], stats.req[1], stats.req[2], stats.req[3]);
	sbuf_printf(sb, "CPL responses:  %10u %10u %10u %10u",
		   stats.rsp[0], stats.rsp[1], stats.rsp[2], stats.rsp[3]);

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

static int
sysctl_ddp_stats(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	struct sbuf *sb;
	int rc;
	struct tp_usm_stats stats;

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return(rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 256, req);
	if (sb == NULL)
		return (ENOMEM);

	t4_get_usm_stats(sc, &stats);

	sbuf_printf(sb, "Frames: %u\n", stats.frames);
	sbuf_printf(sb, "Octets: %ju\n", stats.octets);
	sbuf_printf(sb, "Drops:  %u", stats.drops);

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

const char *devlog_level_strings[] = {
	[FW_DEVLOG_LEVEL_EMERG]		= "EMERG",
	[FW_DEVLOG_LEVEL_CRIT]		= "CRIT",
	[FW_DEVLOG_LEVEL_ERR]		= "ERR",
	[FW_DEVLOG_LEVEL_NOTICE]	= "NOTICE",
	[FW_DEVLOG_LEVEL_INFO]		= "INFO",
	[FW_DEVLOG_LEVEL_DEBUG]		= "DEBUG"
};

const char *devlog_facility_strings[] = {
	[FW_DEVLOG_FACILITY_CORE]	= "CORE",
	[FW_DEVLOG_FACILITY_SCHED]	= "SCHED",
	[FW_DEVLOG_FACILITY_TIMER]	= "TIMER",
	[FW_DEVLOG_FACILITY_RES]	= "RES",
	[FW_DEVLOG_FACILITY_HW]		= "HW",
	[FW_DEVLOG_FACILITY_FLR]	= "FLR",
	[FW_DEVLOG_FACILITY_DMAQ]	= "DMAQ",
	[FW_DEVLOG_FACILITY_PHY]	= "PHY",
	[FW_DEVLOG_FACILITY_MAC]	= "MAC",
	[FW_DEVLOG_FACILITY_PORT]	= "PORT",
	[FW_DEVLOG_FACILITY_VI]		= "VI",
	[FW_DEVLOG_FACILITY_FILTER]	= "FILTER",
	[FW_DEVLOG_FACILITY_ACL]	= "ACL",
	[FW_DEVLOG_FACILITY_TM]		= "TM",
	[FW_DEVLOG_FACILITY_QFC]	= "QFC",
	[FW_DEVLOG_FACILITY_DCB]	= "DCB",
	[FW_DEVLOG_FACILITY_ETH]	= "ETH",
	[FW_DEVLOG_FACILITY_OFLD]	= "OFLD",
	[FW_DEVLOG_FACILITY_RI]		= "RI",
	[FW_DEVLOG_FACILITY_ISCSI]	= "ISCSI",
	[FW_DEVLOG_FACILITY_FCOE]	= "FCOE",
	[FW_DEVLOG_FACILITY_FOISCSI]	= "FOISCSI",
	[FW_DEVLOG_FACILITY_FOFCOE]	= "FOFCOE"
};

static int
sysctl_devlog(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	struct devlog_params *dparams = &sc->params.devlog;
	struct fw_devlog_e *buf, *e;
	int i, j, rc, nentries, first = 0;
	struct sbuf *sb;
	uint64_t ftstamp = UINT64_MAX;

	if (dparams->start == 0)
		return (ENXIO);

	nentries = dparams->size / sizeof(struct fw_devlog_e);

	buf = malloc(dparams->size, M_CXGBE, M_NOWAIT);
	if (buf == NULL)
		return (ENOMEM);

	rc = -t4_mem_read(sc, dparams->memtype, dparams->start, dparams->size,
	    (void *)buf);
	if (rc != 0)
		goto done;

	for (i = 0; i < nentries; i++) {
		e = &buf[i];

		if (e->timestamp == 0)
			break;	/* end */

		e->timestamp = be64toh(e->timestamp);
		e->seqno = be32toh(e->seqno);
		for (j = 0; j < 8; j++)
			e->params[j] = be32toh(e->params[j]);

		if (e->timestamp < ftstamp) {
			ftstamp = e->timestamp;
			first = i;
		}
	}

	if (buf[first].timestamp == 0)
		goto done;	/* nothing in the log */

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		goto done;

	sb = sbuf_new_for_sysctl(NULL, NULL, 4096, req);
	if (sb == NULL) {
		rc = ENOMEM;
		goto done;
	}
	sbuf_printf(sb, "%10s  %15s  %8s  %8s  %s\n",
	    "Seq#", "Tstamp", "Level", "Facility", "Message");

	i = first;
	do {
		e = &buf[i];
		if (e->timestamp == 0)
			break;	/* end */

		sbuf_printf(sb, "%10d  %15ju  %8s  %8s  ",
		    e->seqno, e->timestamp,
		    (e->level < ARRAY_SIZE(devlog_level_strings) ?
			devlog_level_strings[e->level] : "UNKNOWN"),
		    (e->facility < ARRAY_SIZE(devlog_facility_strings) ?
			devlog_facility_strings[e->facility] : "UNKNOWN"));
		sbuf_printf(sb, e->fmt, e->params[0], e->params[1],
		    e->params[2], e->params[3], e->params[4],
		    e->params[5], e->params[6], e->params[7]);

		if (++i == nentries)
			i = 0;
	} while (i != first);

	rc = sbuf_finish(sb);
	sbuf_delete(sb);
done:
	free(buf, M_CXGBE);
	return (rc);
}

static int
sysctl_fcoe_stats(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	struct sbuf *sb;
	int rc;
	struct tp_fcoe_stats stats[4];

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return (rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 256, req);
	if (sb == NULL)
		return (ENOMEM);

	t4_get_fcoe_stats(sc, 0, &stats[0]);
	t4_get_fcoe_stats(sc, 1, &stats[1]);
	t4_get_fcoe_stats(sc, 2, &stats[2]);
	t4_get_fcoe_stats(sc, 3, &stats[3]);

	sbuf_printf(sb, "                   channel 0        channel 1        "
	    "channel 2        channel 3\n");
	sbuf_printf(sb, "octetsDDP:  %16ju %16ju %16ju %16ju\n",
	    stats[0].octetsDDP, stats[1].octetsDDP, stats[2].octetsDDP,
	    stats[3].octetsDDP);
	sbuf_printf(sb, "framesDDP:  %16u %16u %16u %16u\n", stats[0].framesDDP,
	    stats[1].framesDDP, stats[2].framesDDP, stats[3].framesDDP);
	sbuf_printf(sb, "framesDrop: %16u %16u %16u %16u",
	    stats[0].framesDrop, stats[1].framesDrop, stats[2].framesDrop,
	    stats[3].framesDrop);

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

static int
sysctl_hw_sched(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	struct sbuf *sb;
	int rc, i;
	unsigned int map, kbps, ipg, mode;
	unsigned int pace_tab[NTX_SCHED];

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return (rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 256, req);
	if (sb == NULL)
		return (ENOMEM);

	map = t4_read_reg(sc, A_TP_TX_MOD_QUEUE_REQ_MAP);
	mode = G_TIMERMODE(t4_read_reg(sc, A_TP_MOD_CONFIG));
	t4_read_pace_tbl(sc, pace_tab);

	sbuf_printf(sb, "Scheduler  Mode   Channel  Rate (Kbps)   "
	    "Class IPG (0.1 ns)   Flow IPG (us)");

	for (i = 0; i < NTX_SCHED; ++i, map >>= 2) {
		t4_get_tx_sched(sc, i, &kbps, &ipg);
		sbuf_printf(sb, "\n    %u      %-5s     %u     ", i,
		    (mode & (1 << i)) ? "flow" : "class", map & 3);
		if (kbps)
			sbuf_printf(sb, "%9u     ", kbps);
		else
			sbuf_printf(sb, " disabled     ");

		if (ipg)
			sbuf_printf(sb, "%13u        ", ipg);
		else
			sbuf_printf(sb, "     disabled        ");

		if (pace_tab[i])
			sbuf_printf(sb, "%10u", pace_tab[i]);
		else
			sbuf_printf(sb, "  disabled");
	}

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

static int
sysctl_lb_stats(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	struct sbuf *sb;
	int rc, i, j;
	uint64_t *p0, *p1;
	struct lb_port_stats s[2];
	static const char *stat_name[] = {
		"OctetsOK:", "FramesOK:", "BcastFrames:", "McastFrames:",
		"UcastFrames:", "ErrorFrames:", "Frames64:", "Frames65To127:",
		"Frames128To255:", "Frames256To511:", "Frames512To1023:",
		"Frames1024To1518:", "Frames1519ToMax:", "FramesDropped:",
		"BG0FramesDropped:", "BG1FramesDropped:", "BG2FramesDropped:",
		"BG3FramesDropped:", "BG0FramesTrunc:", "BG1FramesTrunc:",
		"BG2FramesTrunc:", "BG3FramesTrunc:"
	};

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return (rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 4096, req);
	if (sb == NULL)
		return (ENOMEM);

	memset(s, 0, sizeof(s));

	for (i = 0; i < 4; i += 2) {
		t4_get_lb_stats(sc, i, &s[0]);
		t4_get_lb_stats(sc, i + 1, &s[1]);

		p0 = &s[0].octets;
		p1 = &s[1].octets;
		sbuf_printf(sb, "%s                       Loopback %u"
		    "           Loopback %u", i == 0 ? "" : "\n", i, i + 1);

		for (j = 0; j < ARRAY_SIZE(stat_name); j++)
			sbuf_printf(sb, "\n%-17s %20ju %20ju", stat_name[j],
				   *p0++, *p1++);
	}

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

struct mem_desc {
	unsigned int base;
	unsigned int limit;
	unsigned int idx;
};

static int
mem_desc_cmp(const void *a, const void *b)
{
	return ((const struct mem_desc *)a)->base -
	       ((const struct mem_desc *)b)->base;
}

static void
mem_region_show(struct sbuf *sb, const char *name, unsigned int from,
    unsigned int to)
{
	unsigned int size;

	size = to - from + 1;
	if (size == 0)
		return;

	/* XXX: need humanize_number(3) in libkern for a more readable 'size' */
	sbuf_printf(sb, "%-15s %#x-%#x [%u]\n", name, from, to, size);
}

static int
sysctl_meminfo(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	struct sbuf *sb;
	int rc, i, n;
	uint32_t lo, hi;
	static const char *memory[] = { "EDC0:", "EDC1:", "MC:" };
	static const char *region[] = {
		"DBQ contexts:", "IMSG contexts:", "FLM cache:", "TCBs:",
		"Pstructs:", "Timers:", "Rx FL:", "Tx FL:", "Pstruct FL:",
		"Tx payload:", "Rx payload:", "LE hash:", "iSCSI region:",
		"TDDP region:", "TPT region:", "STAG region:", "RQ region:",
		"RQUDP region:", "PBL region:", "TXPBL region:", "ULPRX state:",
		"ULPTX state:", "On-chip queues:"
	};
	struct mem_desc avail[3];
	struct mem_desc mem[ARRAY_SIZE(region) + 3];	/* up to 3 holes */
	struct mem_desc *md = mem;

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return (rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 4096, req);
	if (sb == NULL)
		return (ENOMEM);

	for (i = 0; i < ARRAY_SIZE(mem); i++) {
		mem[i].limit = 0;
		mem[i].idx = i;
	}

	/* Find and sort the populated memory ranges */
	i = 0;
	lo = t4_read_reg(sc, A_MA_TARGET_MEM_ENABLE);
	if (lo & F_EDRAM0_ENABLE) {
		hi = t4_read_reg(sc, A_MA_EDRAM0_BAR);
		avail[i].base = G_EDRAM0_BASE(hi) << 20;
		avail[i].limit = avail[i].base + (G_EDRAM0_SIZE(hi) << 20);
		avail[i].idx = 0;
		i++;
	}
	if (lo & F_EDRAM1_ENABLE) {
		hi = t4_read_reg(sc, A_MA_EDRAM1_BAR);
		avail[i].base = G_EDRAM1_BASE(hi) << 20;
		avail[i].limit = avail[i].base + (G_EDRAM1_SIZE(hi) << 20);
		avail[i].idx = 1;
		i++;
	}
	if (lo & F_EXT_MEM_ENABLE) {
		hi = t4_read_reg(sc, A_MA_EXT_MEMORY_BAR);
		avail[i].base = G_EXT_MEM_BASE(hi) << 20;
		avail[i].limit = avail[i].base + (G_EXT_MEM_SIZE(hi) << 20);
		avail[i].idx = 2;
		i++;
	}
	if (!i)                                    /* no memory available */
		return 0;
	qsort(avail, i, sizeof(struct mem_desc), mem_desc_cmp);

	(md++)->base = t4_read_reg(sc, A_SGE_DBQ_CTXT_BADDR);
	(md++)->base = t4_read_reg(sc, A_SGE_IMSG_CTXT_BADDR);
	(md++)->base = t4_read_reg(sc, A_SGE_FLM_CACHE_BADDR);
	(md++)->base = t4_read_reg(sc, A_TP_CMM_TCB_BASE);
	(md++)->base = t4_read_reg(sc, A_TP_CMM_MM_BASE);
	(md++)->base = t4_read_reg(sc, A_TP_CMM_TIMER_BASE);
	(md++)->base = t4_read_reg(sc, A_TP_CMM_MM_RX_FLST_BASE);
	(md++)->base = t4_read_reg(sc, A_TP_CMM_MM_TX_FLST_BASE);
	(md++)->base = t4_read_reg(sc, A_TP_CMM_MM_PS_FLST_BASE);

	/* the next few have explicit upper bounds */
	md->base = t4_read_reg(sc, A_TP_PMM_TX_BASE);
	md->limit = md->base - 1 +
		    t4_read_reg(sc, A_TP_PMM_TX_PAGE_SIZE) *
		    G_PMTXMAXPAGE(t4_read_reg(sc, A_TP_PMM_TX_MAX_PAGE));
	md++;

	md->base = t4_read_reg(sc, A_TP_PMM_RX_BASE);
	md->limit = md->base - 1 +
		    t4_read_reg(sc, A_TP_PMM_RX_PAGE_SIZE) *
		    G_PMRXMAXPAGE(t4_read_reg(sc, A_TP_PMM_RX_MAX_PAGE));
	md++;

	if (t4_read_reg(sc, A_LE_DB_CONFIG) & F_HASHEN) {
		hi = t4_read_reg(sc, A_LE_DB_TID_HASHBASE) / 4;
		md->base = t4_read_reg(sc, A_LE_DB_HASH_TID_BASE);
		md->limit = (sc->tids.ntids - hi) * 16 + md->base - 1;
	} else {
		md->base = 0;
		md->idx = ARRAY_SIZE(region);  /* hide it */
	}
	md++;

#define ulp_region(reg) \
	md->base = t4_read_reg(sc, A_ULP_ ## reg ## _LLIMIT);\
	(md++)->limit = t4_read_reg(sc, A_ULP_ ## reg ## _ULIMIT)

	ulp_region(RX_ISCSI);
	ulp_region(RX_TDDP);
	ulp_region(TX_TPT);
	ulp_region(RX_STAG);
	ulp_region(RX_RQ);
	ulp_region(RX_RQUDP);
	ulp_region(RX_PBL);
	ulp_region(TX_PBL);
#undef ulp_region

	md->base = t4_read_reg(sc, A_ULP_RX_CTX_BASE);
	md->limit = md->base + sc->tids.ntids - 1;
	md++;
	md->base = t4_read_reg(sc, A_ULP_TX_ERR_TABLE_BASE);
	md->limit = md->base + sc->tids.ntids - 1;
	md++;

	md->base = sc->vres.ocq.start;
	if (sc->vres.ocq.size)
		md->limit = md->base + sc->vres.ocq.size - 1;
	else
		md->idx = ARRAY_SIZE(region);  /* hide it */
	md++;

	/* add any address-space holes, there can be up to 3 */
	for (n = 0; n < i - 1; n++)
		if (avail[n].limit < avail[n + 1].base)
			(md++)->base = avail[n].limit;
	if (avail[n].limit)
		(md++)->base = avail[n].limit;

	n = md - mem;
	qsort(mem, n, sizeof(struct mem_desc), mem_desc_cmp);

	for (lo = 0; lo < i; lo++)
		mem_region_show(sb, memory[avail[lo].idx], avail[lo].base,
				avail[lo].limit - 1);

	sbuf_printf(sb, "\n");
	for (i = 0; i < n; i++) {
		if (mem[i].idx >= ARRAY_SIZE(region))
			continue;                        /* skip holes */
		if (!mem[i].limit)
			mem[i].limit = i < n - 1 ? mem[i + 1].base - 1 : ~0;
		mem_region_show(sb, region[mem[i].idx], mem[i].base,
				mem[i].limit);
	}

	sbuf_printf(sb, "\n");
	lo = t4_read_reg(sc, A_CIM_SDRAM_BASE_ADDR);
	hi = t4_read_reg(sc, A_CIM_SDRAM_ADDR_SIZE) + lo - 1;
	mem_region_show(sb, "uP RAM:", lo, hi);

	lo = t4_read_reg(sc, A_CIM_EXTMEM2_BASE_ADDR);
	hi = t4_read_reg(sc, A_CIM_EXTMEM2_ADDR_SIZE) + lo - 1;
	mem_region_show(sb, "uP Extmem2:", lo, hi);

	lo = t4_read_reg(sc, A_TP_PMM_RX_MAX_PAGE);
	sbuf_printf(sb, "\n%u Rx pages of size %uKiB for %u channels\n",
		   G_PMRXMAXPAGE(lo),
		   t4_read_reg(sc, A_TP_PMM_RX_PAGE_SIZE) >> 10,
		   (lo & F_PMRXNUMCHN) ? 2 : 1);

	lo = t4_read_reg(sc, A_TP_PMM_TX_MAX_PAGE);
	hi = t4_read_reg(sc, A_TP_PMM_TX_PAGE_SIZE);
	sbuf_printf(sb, "%u Tx pages of size %u%ciB for %u channels\n",
		   G_PMTXMAXPAGE(lo),
		   hi >= (1 << 20) ? (hi >> 20) : (hi >> 10),
		   hi >= (1 << 20) ? 'M' : 'K', 1 << G_PMTXNUMCHN(lo));
	sbuf_printf(sb, "%u p-structs\n",
		   t4_read_reg(sc, A_TP_CMM_MM_MAX_PSTRUCT));

	for (i = 0; i < 4; i++) {
		lo = t4_read_reg(sc, A_MPS_RX_PG_RSV0 + i * 4);
		sbuf_printf(sb, "\nPort %d using %u pages out of %u allocated",
			   i, G_USED(lo), G_ALLOC(lo));
	}
	for (i = 0; i < 4; i++) {
		lo = t4_read_reg(sc, A_MPS_RX_PG_RSV4 + i * 4);
		sbuf_printf(sb,
			   "\nLoopback %d using %u pages out of %u allocated",
			   i, G_USED(lo), G_ALLOC(lo));
	}

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

static int
sysctl_path_mtus(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	struct sbuf *sb;
	int rc;
	uint16_t mtus[NMTUS];

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return (rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 256, req);
	if (sb == NULL)
		return (ENOMEM);

	t4_read_mtu_tbl(sc, mtus, NULL);

	sbuf_printf(sb, "%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
	    mtus[0], mtus[1], mtus[2], mtus[3], mtus[4], mtus[5], mtus[6],
	    mtus[7], mtus[8], mtus[9], mtus[10], mtus[11], mtus[12], mtus[13],
	    mtus[14], mtus[15]);

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

static int
sysctl_pm_stats(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	struct sbuf *sb;
	int rc, i;
	uint32_t tx_cnt[PM_NSTATS], rx_cnt[PM_NSTATS];
	uint64_t tx_cyc[PM_NSTATS], rx_cyc[PM_NSTATS];
	static const char *pm_stats[] = {
		"Read:", "Write bypass:", "Write mem:", "Flush:", "FIFO wait:"
	};

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return (rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 256, req);
	if (sb == NULL)
		return (ENOMEM);

	t4_pmtx_get_stats(sc, tx_cnt, tx_cyc);
	t4_pmrx_get_stats(sc, rx_cnt, rx_cyc);

	sbuf_printf(sb, "                Tx count            Tx cycles    "
	    "Rx count            Rx cycles");
	for (i = 0; i < PM_NSTATS; i++)
		sbuf_printf(sb, "\n%-13s %10u %20ju  %10u %20ju",
		    pm_stats[i], tx_cnt[i], tx_cyc[i], rx_cnt[i], rx_cyc[i]);

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

static int
sysctl_rdma_stats(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	struct sbuf *sb;
	int rc;
	struct tp_rdma_stats stats;

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return (rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 256, req);
	if (sb == NULL)
		return (ENOMEM);

	t4_tp_get_rdma_stats(sc, &stats);
	sbuf_printf(sb, "NoRQEModDefferals: %u\n", stats.rqe_dfr_mod);
	sbuf_printf(sb, "NoRQEPktDefferals: %u", stats.rqe_dfr_pkt);

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

static int
sysctl_tcp_stats(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	struct sbuf *sb;
	int rc;
	struct tp_tcp_stats v4, v6;

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return (rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 256, req);
	if (sb == NULL)
		return (ENOMEM);

	t4_tp_get_tcp_stats(sc, &v4, &v6);
	sbuf_printf(sb,
	    "                                IP                 IPv6\n");
	sbuf_printf(sb, "OutRsts:      %20u %20u\n",
	    v4.tcpOutRsts, v6.tcpOutRsts);
	sbuf_printf(sb, "InSegs:       %20ju %20ju\n",
	    v4.tcpInSegs, v6.tcpInSegs);
	sbuf_printf(sb, "OutSegs:      %20ju %20ju\n",
	    v4.tcpOutSegs, v6.tcpOutSegs);
	sbuf_printf(sb, "RetransSegs:  %20ju %20ju",
	    v4.tcpRetransSegs, v6.tcpRetransSegs);

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

static int
sysctl_tids(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	struct sbuf *sb;
	int rc;
	struct tid_info *t = &sc->tids;

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return (rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 256, req);
	if (sb == NULL)
		return (ENOMEM);

	if (t->natids) {
		sbuf_printf(sb, "ATID range: 0-%u, in use: %u\n", t->natids - 1,
		    t->atids_in_use);
	}

	if (t->ntids) {
		if (t4_read_reg(sc, A_LE_DB_CONFIG) & F_HASHEN) {
			uint32_t b = t4_read_reg(sc, A_LE_DB_SERVER_INDEX) / 4;

			if (b) {
				sbuf_printf(sb, "TID range: 0-%u, %u-%u", b - 1,
				    t4_read_reg(sc, A_LE_DB_TID_HASHBASE) / 4,
				    t->ntids - 1);
			} else {
				sbuf_printf(sb, "TID range: %u-%u",
				    t4_read_reg(sc, A_LE_DB_TID_HASHBASE) / 4,
				    t->ntids - 1);
			}
		} else
			sbuf_printf(sb, "TID range: 0-%u", t->ntids - 1);
		sbuf_printf(sb, ", in use: %u\n",
		    atomic_load_acq_int(&t->tids_in_use));
	}

	if (t->nstids) {
		sbuf_printf(sb, "STID range: %u-%u, in use: %u\n", t->stid_base,
		    t->stid_base + t->nstids - 1, t->stids_in_use);
	}

	if (t->nftids) {
		sbuf_printf(sb, "FTID range: %u-%u\n", t->ftid_base,
		    t->ftid_base + t->nftids - 1);
	}

	sbuf_printf(sb, "HW TID usage: %u IP users, %u IPv6 users",
	    t4_read_reg(sc, A_LE_DB_ACT_CNT_IPV4),
	    t4_read_reg(sc, A_LE_DB_ACT_CNT_IPV6));

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

static int
sysctl_tp_err_stats(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	struct sbuf *sb;
	int rc;
	struct tp_err_stats stats;

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return (rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 256, req);
	if (sb == NULL)
		return (ENOMEM);

	t4_tp_get_err_stats(sc, &stats);

	sbuf_printf(sb, "                 channel 0  channel 1  channel 2  "
		      "channel 3\n");
	sbuf_printf(sb, "macInErrs:      %10u %10u %10u %10u\n",
	    stats.macInErrs[0], stats.macInErrs[1], stats.macInErrs[2],
	    stats.macInErrs[3]);
	sbuf_printf(sb, "hdrInErrs:      %10u %10u %10u %10u\n",
	    stats.hdrInErrs[0], stats.hdrInErrs[1], stats.hdrInErrs[2],
	    stats.hdrInErrs[3]);
	sbuf_printf(sb, "tcpInErrs:      %10u %10u %10u %10u\n",
	    stats.tcpInErrs[0], stats.tcpInErrs[1], stats.tcpInErrs[2],
	    stats.tcpInErrs[3]);
	sbuf_printf(sb, "tcp6InErrs:     %10u %10u %10u %10u\n",
	    stats.tcp6InErrs[0], stats.tcp6InErrs[1], stats.tcp6InErrs[2],
	    stats.tcp6InErrs[3]);
	sbuf_printf(sb, "tnlCongDrops:   %10u %10u %10u %10u\n",
	    stats.tnlCongDrops[0], stats.tnlCongDrops[1], stats.tnlCongDrops[2],
	    stats.tnlCongDrops[3]);
	sbuf_printf(sb, "tnlTxDrops:     %10u %10u %10u %10u\n",
	    stats.tnlTxDrops[0], stats.tnlTxDrops[1], stats.tnlTxDrops[2],
	    stats.tnlTxDrops[3]);
	sbuf_printf(sb, "ofldVlanDrops:  %10u %10u %10u %10u\n",
	    stats.ofldVlanDrops[0], stats.ofldVlanDrops[1],
	    stats.ofldVlanDrops[2], stats.ofldVlanDrops[3]);
	sbuf_printf(sb, "ofldChanDrops:  %10u %10u %10u %10u\n\n",
	    stats.ofldChanDrops[0], stats.ofldChanDrops[1],
	    stats.ofldChanDrops[2], stats.ofldChanDrops[3]);
	sbuf_printf(sb, "ofldNoNeigh:    %u\nofldCongDefer:  %u",
	    stats.ofldNoNeigh, stats.ofldCongDefer);

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

static int
sysctl_tx_rate(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	struct sbuf *sb;
	int rc;
	u64 nrate[NCHAN], orate[NCHAN];

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return (rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 256, req);
	if (sb == NULL)
		return (ENOMEM);

	t4_get_chan_txrate(sc, nrate, orate);
	sbuf_printf(sb, "              channel 0   channel 1   channel 2   "
		 "channel 3\n");
	sbuf_printf(sb, "NIC B/s:     %10ju  %10ju  %10ju  %10ju\n",
	    nrate[0], nrate[1], nrate[2], nrate[3]);
	sbuf_printf(sb, "Offload B/s: %10ju  %10ju  %10ju  %10ju",
	    orate[0], orate[1], orate[2], orate[3]);

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}
#endif

static inline void
txq_start(struct ifnet *ifp, struct sge_txq *txq)
{
	struct buf_ring *br;
	struct mbuf *m;

	TXQ_LOCK_ASSERT_OWNED(txq);

	br = txq->br;
	m = txq->m ? txq->m : drbr_dequeue(ifp, br);
	if (m)
		t4_eth_tx(ifp, txq, m);
}

void
t4_tx_callout(void *arg)
{
	struct sge_eq *eq = arg;
	struct adapter *sc;

	if (EQ_TRYLOCK(eq) == 0)
		goto reschedule;

	if (eq->flags & EQ_STALLED && !can_resume_tx(eq)) {
		EQ_UNLOCK(eq);
reschedule:
		if (__predict_true(!(eq->flags && EQ_DOOMED)))
			callout_schedule(&eq->tx_callout, 1);
		return;
	}

	EQ_LOCK_ASSERT_OWNED(eq);

	if (__predict_true((eq->flags & EQ_DOOMED) == 0)) {

		if ((eq->flags & EQ_TYPEMASK) == EQ_ETH) {
			struct sge_txq *txq = arg;
			struct port_info *pi = txq->ifp->if_softc;

			sc = pi->adapter;
		} else {
			struct sge_wrq *wrq = arg;

			sc = wrq->adapter;
		}

		taskqueue_enqueue(sc->tq[eq->tx_chan], &eq->tx_task);
	}

	EQ_UNLOCK(eq);
}

void
t4_tx_task(void *arg, int count)
{
	struct sge_eq *eq = arg;

	EQ_LOCK(eq);
	if ((eq->flags & EQ_TYPEMASK) == EQ_ETH) {
		struct sge_txq *txq = arg;
		txq_start(txq->ifp, txq);
	} else {
		struct sge_wrq *wrq = arg;
		t4_wrq_tx_locked(wrq->adapter, wrq, NULL);
	}
	EQ_UNLOCK(eq);
}

static uint32_t
fconf_to_mode(uint32_t fconf)
{
	uint32_t mode;

	mode = T4_FILTER_IPv4 | T4_FILTER_IPv6 | T4_FILTER_IP_SADDR |
	    T4_FILTER_IP_DADDR | T4_FILTER_IP_SPORT | T4_FILTER_IP_DPORT;

	if (fconf & F_FRAGMENTATION)
		mode |= T4_FILTER_IP_FRAGMENT;

	if (fconf & F_MPSHITTYPE)
		mode |= T4_FILTER_MPS_HIT_TYPE;

	if (fconf & F_MACMATCH)
		mode |= T4_FILTER_MAC_IDX;

	if (fconf & F_ETHERTYPE)
		mode |= T4_FILTER_ETH_TYPE;

	if (fconf & F_PROTOCOL)
		mode |= T4_FILTER_IP_PROTO;

	if (fconf & F_TOS)
		mode |= T4_FILTER_IP_TOS;

	if (fconf & F_VLAN)
		mode |= T4_FILTER_VLAN;

	if (fconf & F_VNIC_ID)
		mode |= T4_FILTER_VNIC;

	if (fconf & F_PORT)
		mode |= T4_FILTER_PORT;

	if (fconf & F_FCOE)
		mode |= T4_FILTER_FCoE;

	return (mode);
}

static uint32_t
mode_to_fconf(uint32_t mode)
{
	uint32_t fconf = 0;

	if (mode & T4_FILTER_IP_FRAGMENT)
		fconf |= F_FRAGMENTATION;

	if (mode & T4_FILTER_MPS_HIT_TYPE)
		fconf |= F_MPSHITTYPE;

	if (mode & T4_FILTER_MAC_IDX)
		fconf |= F_MACMATCH;

	if (mode & T4_FILTER_ETH_TYPE)
		fconf |= F_ETHERTYPE;

	if (mode & T4_FILTER_IP_PROTO)
		fconf |= F_PROTOCOL;

	if (mode & T4_FILTER_IP_TOS)
		fconf |= F_TOS;

	if (mode & T4_FILTER_VLAN)
		fconf |= F_VLAN;

	if (mode & T4_FILTER_VNIC)
		fconf |= F_VNIC_ID;

	if (mode & T4_FILTER_PORT)
		fconf |= F_PORT;

	if (mode & T4_FILTER_FCoE)
		fconf |= F_FCOE;

	return (fconf);
}

static uint32_t
fspec_to_fconf(struct t4_filter_specification *fs)
{
	uint32_t fconf = 0;

	if (fs->val.frag || fs->mask.frag)
		fconf |= F_FRAGMENTATION;

	if (fs->val.matchtype || fs->mask.matchtype)
		fconf |= F_MPSHITTYPE;

	if (fs->val.macidx || fs->mask.macidx)
		fconf |= F_MACMATCH;

	if (fs->val.ethtype || fs->mask.ethtype)
		fconf |= F_ETHERTYPE;

	if (fs->val.proto || fs->mask.proto)
		fconf |= F_PROTOCOL;

	if (fs->val.tos || fs->mask.tos)
		fconf |= F_TOS;

	if (fs->val.vlan_vld || fs->mask.vlan_vld)
		fconf |= F_VLAN;

	if (fs->val.vnic_vld || fs->mask.vnic_vld)
		fconf |= F_VNIC_ID;

	if (fs->val.iport || fs->mask.iport)
		fconf |= F_PORT;

	if (fs->val.fcoe || fs->mask.fcoe)
		fconf |= F_FCOE;

	return (fconf);
}

static int
get_filter_mode(struct adapter *sc, uint32_t *mode)
{
	uint32_t fconf;

	t4_read_indirect(sc, A_TP_PIO_ADDR, A_TP_PIO_DATA, &fconf, 1,
	    A_TP_VLAN_PRI_MAP);

	if (sc->filter_mode != fconf) {
		log(LOG_WARNING, "%s: cached filter mode out of sync %x %x.\n",
		    device_get_nameunit(sc->dev), sc->filter_mode, fconf);
		sc->filter_mode = fconf;
	}

	*mode = fconf_to_mode(sc->filter_mode);

	return (0);
}

static int
set_filter_mode(struct adapter *sc, uint32_t mode)
{
	uint32_t fconf;
	int rc;

	fconf = mode_to_fconf(mode);

	ADAPTER_LOCK(sc);
	if (IS_BUSY(sc)) {
		rc = EAGAIN;
		goto done;
	}

	if (sc->tids.ftids_in_use > 0) {
		rc = EBUSY;
		goto done;
	}

#ifdef TCP_OFFLOAD
	if (sc->offload_map) {
		rc = EBUSY;
		goto done;
	}
#endif

#ifdef notyet
	rc = -t4_set_filter_mode(sc, fconf);
	if (rc == 0)
		sc->filter_mode = fconf;
#else
	rc = ENOTSUP;
#endif

done:
	ADAPTER_UNLOCK(sc);
	return (rc);
}

static inline uint64_t
get_filter_hits(struct adapter *sc, uint32_t fid)
{
	uint32_t tcb_base = t4_read_reg(sc, A_TP_CMM_TCB_BASE);
	uint64_t hits;

	t4_write_reg(sc, PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_OFFSET, 0),
	    tcb_base + (fid + sc->tids.ftid_base) * TCB_SIZE);
	t4_read_reg(sc, PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_OFFSET, 0));
	hits = t4_read_reg64(sc, MEMWIN0_BASE + 16);

	return (be64toh(hits));
}

static int
get_filter(struct adapter *sc, struct t4_filter *t)
{
	int i, nfilters = sc->tids.nftids;
	struct filter_entry *f;

	ADAPTER_LOCK_ASSERT_OWNED(sc);

	if (IS_BUSY(sc))
		return (EAGAIN);

	if (sc->tids.ftids_in_use == 0 || sc->tids.ftid_tab == NULL ||
	    t->idx >= nfilters) {
		t->idx = 0xffffffff;
		return (0);
	}

	f = &sc->tids.ftid_tab[t->idx];
	for (i = t->idx; i < nfilters; i++, f++) {
		if (f->valid) {
			t->idx = i;
			t->l2tidx = f->l2t ? f->l2t->idx : 0;
			t->smtidx = f->smtidx;
			if (f->fs.hitcnts)
				t->hits = get_filter_hits(sc, t->idx);
			else
				t->hits = UINT64_MAX;
			t->fs = f->fs;

			return (0);
		}
	}

	t->idx = 0xffffffff;
	return (0);
}

static int
set_filter(struct adapter *sc, struct t4_filter *t)
{
	unsigned int nfilters, nports;
	struct filter_entry *f;
	int i;

	ADAPTER_LOCK_ASSERT_OWNED(sc);

	nfilters = sc->tids.nftids;
	nports = sc->params.nports;

	if (nfilters == 0)
		return (ENOTSUP);

	if (!(sc->flags & FULL_INIT_DONE))
		return (EAGAIN);

	if (t->idx >= nfilters)
		return (EINVAL);

	/* Validate against the global filter mode */
	if ((sc->filter_mode | fspec_to_fconf(&t->fs)) != sc->filter_mode)
		return (E2BIG);

	if (t->fs.action == FILTER_SWITCH && t->fs.eport >= nports)
		return (EINVAL);

	if (t->fs.val.iport >= nports)
		return (EINVAL);

	/* Can't specify an iq if not steering to it */
	if (!t->fs.dirsteer && t->fs.iq)
		return (EINVAL);

	/* IPv6 filter idx must be 4 aligned */
	if (t->fs.type == 1 &&
	    ((t->idx & 0x3) || t->idx + 4 >= nfilters))
		return (EINVAL);

	if (sc->tids.ftid_tab == NULL) {
		KASSERT(sc->tids.ftids_in_use == 0,
		    ("%s: no memory allocated but filters_in_use > 0",
		    __func__));

		sc->tids.ftid_tab = malloc(sizeof (struct filter_entry) *
		    nfilters, M_CXGBE, M_NOWAIT | M_ZERO);
		if (sc->tids.ftid_tab == NULL)
			return (ENOMEM);
	}

	for (i = 0; i < 4; i++) {
		f = &sc->tids.ftid_tab[t->idx + i];

		if (f->pending || f->valid)
			return (EBUSY);
		if (f->locked)
			return (EPERM);

		if (t->fs.type == 0)
			break;
	}

	f = &sc->tids.ftid_tab[t->idx];
	f->fs = t->fs;

	return set_filter_wr(sc, t->idx);
}

static int
del_filter(struct adapter *sc, struct t4_filter *t)
{
	unsigned int nfilters;
	struct filter_entry *f;

	ADAPTER_LOCK_ASSERT_OWNED(sc);

	if (IS_BUSY(sc))
		return (EAGAIN);

	nfilters = sc->tids.nftids;

	if (nfilters == 0)
		return (ENOTSUP);

	if (sc->tids.ftid_tab == NULL || sc->tids.ftids_in_use == 0 ||
	    t->idx >= nfilters)
		return (EINVAL);

	if (!(sc->flags & FULL_INIT_DONE))
		return (EAGAIN);

	f = &sc->tids.ftid_tab[t->idx];

	if (f->pending)
		return (EBUSY);
	if (f->locked)
		return (EPERM);

	if (f->valid) {
		t->fs = f->fs;	/* extra info for the caller */
		return del_filter_wr(sc, t->idx);
	}

	return (0);
}

static void
clear_filter(struct filter_entry *f)
{
	if (f->l2t)
		t4_l2t_release(f->l2t);

	bzero(f, sizeof (*f));
}

static int
set_filter_wr(struct adapter *sc, int fidx)
{
	struct filter_entry *f = &sc->tids.ftid_tab[fidx];
	struct wrqe *wr;
	struct fw_filter_wr *fwr;
	unsigned int ftid;

	ADAPTER_LOCK_ASSERT_OWNED(sc);

	if (f->fs.newdmac || f->fs.newvlan) {
		/* This filter needs an L2T entry; allocate one. */
		f->l2t = t4_l2t_alloc_switching(sc->l2t);
		if (f->l2t == NULL)
			return (EAGAIN);
		if (t4_l2t_set_switching(sc, f->l2t, f->fs.vlan, f->fs.eport,
		    f->fs.dmac)) {
			t4_l2t_release(f->l2t);
			f->l2t = NULL;
			return (ENOMEM);
		}
	}

	ftid = sc->tids.ftid_base + fidx;

	wr = alloc_wrqe(sizeof(*fwr), &sc->sge.mgmtq);
	if (wr == NULL)
		return (ENOMEM);

	fwr = wrtod(wr);
	bzero(fwr, sizeof (*fwr));

	fwr->op_pkd = htobe32(V_FW_WR_OP(FW_FILTER_WR));
	fwr->len16_pkd = htobe32(FW_LEN16(*fwr));
	fwr->tid_to_iq =
	    htobe32(V_FW_FILTER_WR_TID(ftid) |
		V_FW_FILTER_WR_RQTYPE(f->fs.type) |
		V_FW_FILTER_WR_NOREPLY(0) |
		V_FW_FILTER_WR_IQ(f->fs.iq));
	fwr->del_filter_to_l2tix =
	    htobe32(V_FW_FILTER_WR_RPTTID(f->fs.rpttid) |
		V_FW_FILTER_WR_DROP(f->fs.action == FILTER_DROP) |
		V_FW_FILTER_WR_DIRSTEER(f->fs.dirsteer) |
		V_FW_FILTER_WR_MASKHASH(f->fs.maskhash) |
		V_FW_FILTER_WR_DIRSTEERHASH(f->fs.dirsteerhash) |
		V_FW_FILTER_WR_LPBK(f->fs.action == FILTER_SWITCH) |
		V_FW_FILTER_WR_DMAC(f->fs.newdmac) |
		V_FW_FILTER_WR_SMAC(f->fs.newsmac) |
		V_FW_FILTER_WR_INSVLAN(f->fs.newvlan == VLAN_INSERT ||
		    f->fs.newvlan == VLAN_REWRITE) |
		V_FW_FILTER_WR_RMVLAN(f->fs.newvlan == VLAN_REMOVE ||
		    f->fs.newvlan == VLAN_REWRITE) |
		V_FW_FILTER_WR_HITCNTS(f->fs.hitcnts) |
		V_FW_FILTER_WR_TXCHAN(f->fs.eport) |
		V_FW_FILTER_WR_PRIO(f->fs.prio) |
		V_FW_FILTER_WR_L2TIX(f->l2t ? f->l2t->idx : 0));
	fwr->ethtype = htobe16(f->fs.val.ethtype);
	fwr->ethtypem = htobe16(f->fs.mask.ethtype);
	fwr->frag_to_ovlan_vldm =
	    (V_FW_FILTER_WR_FRAG(f->fs.val.frag) |
		V_FW_FILTER_WR_FRAGM(f->fs.mask.frag) |
		V_FW_FILTER_WR_IVLAN_VLD(f->fs.val.vlan_vld) |
		V_FW_FILTER_WR_OVLAN_VLD(f->fs.val.vnic_vld) |
		V_FW_FILTER_WR_IVLAN_VLDM(f->fs.mask.vlan_vld) |
		V_FW_FILTER_WR_OVLAN_VLDM(f->fs.mask.vnic_vld));
	fwr->smac_sel = 0;
	fwr->rx_chan_rx_rpl_iq = htobe16(V_FW_FILTER_WR_RX_CHAN(0) |
	    V_FW_FILTER_WR_RX_RPL_IQ(sc->sge.fwq.abs_id));
	fwr->maci_to_matchtypem =
	    htobe32(V_FW_FILTER_WR_MACI(f->fs.val.macidx) |
		V_FW_FILTER_WR_MACIM(f->fs.mask.macidx) |
		V_FW_FILTER_WR_FCOE(f->fs.val.fcoe) |
		V_FW_FILTER_WR_FCOEM(f->fs.mask.fcoe) |
		V_FW_FILTER_WR_PORT(f->fs.val.iport) |
		V_FW_FILTER_WR_PORTM(f->fs.mask.iport) |
		V_FW_FILTER_WR_MATCHTYPE(f->fs.val.matchtype) |
		V_FW_FILTER_WR_MATCHTYPEM(f->fs.mask.matchtype));
	fwr->ptcl = f->fs.val.proto;
	fwr->ptclm = f->fs.mask.proto;
	fwr->ttyp = f->fs.val.tos;
	fwr->ttypm = f->fs.mask.tos;
	fwr->ivlan = htobe16(f->fs.val.vlan);
	fwr->ivlanm = htobe16(f->fs.mask.vlan);
	fwr->ovlan = htobe16(f->fs.val.vnic);
	fwr->ovlanm = htobe16(f->fs.mask.vnic);
	bcopy(f->fs.val.dip, fwr->lip, sizeof (fwr->lip));
	bcopy(f->fs.mask.dip, fwr->lipm, sizeof (fwr->lipm));
	bcopy(f->fs.val.sip, fwr->fip, sizeof (fwr->fip));
	bcopy(f->fs.mask.sip, fwr->fipm, sizeof (fwr->fipm));
	fwr->lp = htobe16(f->fs.val.dport);
	fwr->lpm = htobe16(f->fs.mask.dport);
	fwr->fp = htobe16(f->fs.val.sport);
	fwr->fpm = htobe16(f->fs.mask.sport);
	if (f->fs.newsmac)
		bcopy(f->fs.smac, fwr->sma, sizeof (fwr->sma));

	f->pending = 1;
	sc->tids.ftids_in_use++;

	t4_wrq_tx(sc, wr);
	return (0);
}

static int
del_filter_wr(struct adapter *sc, int fidx)
{
	struct filter_entry *f = &sc->tids.ftid_tab[fidx];
	struct wrqe *wr;
	struct fw_filter_wr *fwr;
	unsigned int ftid;

	ADAPTER_LOCK_ASSERT_OWNED(sc);

	ftid = sc->tids.ftid_base + fidx;

	wr = alloc_wrqe(sizeof(*fwr), &sc->sge.mgmtq);
	if (wr == NULL)
		return (ENOMEM);
	fwr = wrtod(wr);
	bzero(fwr, sizeof (*fwr));

	t4_mk_filtdelwr(ftid, fwr, sc->sge.fwq.abs_id);

	f->pending = 1;
	t4_wrq_tx(sc, wr);
	return (0);
}

static int
filter_rpl(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_set_tcb_rpl *rpl = (const void *)(rss + 1);
	unsigned int idx = GET_TID(rpl);

	KASSERT(m == NULL, ("%s: payload with opcode %02x", __func__,
	    rss->opcode));

	if (idx >= sc->tids.ftid_base &&
	    (idx -= sc->tids.ftid_base) < sc->tids.nftids) {
		unsigned int rc = G_COOKIE(rpl->cookie);
		struct filter_entry *f = &sc->tids.ftid_tab[idx];

		ADAPTER_LOCK(sc);
		if (rc == FW_FILTER_WR_FLT_ADDED) {
			f->smtidx = (be64toh(rpl->oldval) >> 24) & 0xff;
			f->pending = 0;  /* asynchronous setup completed */
			f->valid = 1;
		} else {
			if (rc != FW_FILTER_WR_FLT_DELETED) {
				/* Add or delete failed, display an error */
				log(LOG_ERR,
				    "filter %u setup failed with error %u\n",
				    idx, rc);
			}

			clear_filter(f);
			sc->tids.ftids_in_use--;
		}
		ADAPTER_UNLOCK(sc);
	}

	return (0);
}

static int
get_sge_context(struct adapter *sc, struct t4_sge_context *cntxt)
{
	int rc = EINVAL;

	if (cntxt->cid > M_CTXTQID)
		return (rc);

	if (cntxt->mem_id != CTXT_EGRESS && cntxt->mem_id != CTXT_INGRESS &&
	    cntxt->mem_id != CTXT_FLM && cntxt->mem_id != CTXT_CNM)
		return (rc);

	if (sc->flags & FW_OK) {
		ADAPTER_LOCK(sc);	/* Avoid parallel t4_wr_mbox */
		rc = -t4_sge_ctxt_rd(sc, sc->mbox, cntxt->cid, cntxt->mem_id,
		    &cntxt->data[0]);
		ADAPTER_UNLOCK(sc);
	}

	if (rc != 0) {
		/* Read via firmware failed or wasn't even attempted */

		rc = -t4_sge_ctxt_rd_bd(sc, cntxt->cid, cntxt->mem_id,
		    &cntxt->data[0]);
	}

	return (rc);
}

static int
read_card_mem(struct adapter *sc, struct t4_mem_range *mr)
{
	uint32_t base, size, lo, hi, win, off, remaining, i, n;
	uint32_t *buf, *b;
	int rc;

	/* reads are in multiples of 32 bits */
	if (mr->addr & 3 || mr->len & 3 || mr->len == 0)
		return (EINVAL);

	/*
	 * We don't want to deal with potential holes so we mandate that the
	 * requested region must lie entirely within one of the 3 memories.
	 */
	lo = t4_read_reg(sc, A_MA_TARGET_MEM_ENABLE);
	if (lo & F_EDRAM0_ENABLE) {
		hi = t4_read_reg(sc, A_MA_EDRAM0_BAR);
		base = G_EDRAM0_BASE(hi) << 20;
		size = G_EDRAM0_SIZE(hi) << 20;
		if (size > 0 &&
		    mr->addr >= base && mr->addr < base + size &&
		    mr->addr + mr->len <= base + size)
			goto proceed;
	}
	if (lo & F_EDRAM1_ENABLE) {
		hi = t4_read_reg(sc, A_MA_EDRAM1_BAR);
		base = G_EDRAM1_BASE(hi) << 20;
		size = G_EDRAM1_SIZE(hi) << 20;
		if (size > 0 &&
		    mr->addr >= base && mr->addr < base + size &&
		    mr->addr + mr->len <= base + size)
			goto proceed;
	}
	if (lo & F_EXT_MEM_ENABLE) {
		hi = t4_read_reg(sc, A_MA_EXT_MEMORY_BAR);
		base = G_EXT_MEM_BASE(hi) << 20;
		size = G_EXT_MEM_SIZE(hi) << 20;
		if (size > 0 &&
		    mr->addr >= base && mr->addr < base + size &&
		    mr->addr + mr->len <= base + size)
			goto proceed;
	}
	return (ENXIO);

proceed:
	buf = b = malloc(mr->len, M_CXGBE, M_WAITOK);

	/*
	 * Position the PCIe window (we use memwin2) to the 16B aligned area
	 * just at/before the requested region.
	 */
	win = mr->addr & ~0xf;
	off = mr->addr - win;  /* offset of the requested region in the win */
	remaining = mr->len;

	while (remaining) {
		t4_write_reg(sc,
		    PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_OFFSET, 2), win);
		t4_read_reg(sc,
		    PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_OFFSET, 2));

		/* number of bytes that we'll copy in the inner loop */
		n = min(remaining, MEMWIN2_APERTURE - off);

		for (i = 0; i < n; i += 4, remaining -= 4)
			*b++ = t4_read_reg(sc, MEMWIN2_BASE + off + i);

		win += MEMWIN2_APERTURE;
		off = 0;
	}

	rc = copyout(buf, mr->data, mr->len);
	free(buf, M_CXGBE);

	return (rc);
}

int
t4_os_find_pci_capability(struct adapter *sc, int cap)
{
	int i;

	return (pci_find_cap(sc->dev, cap, &i) == 0 ? i : 0);
}

int
t4_os_pci_save_state(struct adapter *sc)
{
	device_t dev;
	struct pci_devinfo *dinfo;

	dev = sc->dev;
	dinfo = device_get_ivars(dev);

	pci_cfg_save(dev, dinfo, 0);
	return (0);
}

int
t4_os_pci_restore_state(struct adapter *sc)
{
	device_t dev;
	struct pci_devinfo *dinfo;

	dev = sc->dev;
	dinfo = device_get_ivars(dev);

	pci_cfg_restore(dev, dinfo);
	return (0);
}

void
t4_os_portmod_changed(const struct adapter *sc, int idx)
{
	struct port_info *pi = sc->port[idx];
	static const char *mod_str[] = {
		NULL, "LR", "SR", "ER", "TWINAX", "active TWINAX", "LRM"
	};

	if (pi->mod_type == FW_PORT_MOD_TYPE_NONE)
		if_printf(pi->ifp, "transceiver unplugged.\n");
	else if (pi->mod_type == FW_PORT_MOD_TYPE_UNKNOWN)
		if_printf(pi->ifp, "unknown transceiver inserted.\n");
	else if (pi->mod_type == FW_PORT_MOD_TYPE_NOTSUPPORTED)
		if_printf(pi->ifp, "unsupported transceiver inserted.\n");
	else if (pi->mod_type > 0 && pi->mod_type < ARRAY_SIZE(mod_str)) {
		if_printf(pi->ifp, "%s transceiver inserted.\n",
		    mod_str[pi->mod_type]);
	} else {
		if_printf(pi->ifp, "transceiver (type %d) inserted.\n",
		    pi->mod_type);
	}
}

void
t4_os_link_changed(struct adapter *sc, int idx, int link_stat)
{
	struct port_info *pi = sc->port[idx];
	struct ifnet *ifp = pi->ifp;

	if (link_stat) {
		ifp->if_baudrate = IF_Mbps(pi->link_cfg.speed);
		if_link_state_change(ifp, LINK_STATE_UP);
	} else
		if_link_state_change(ifp, LINK_STATE_DOWN);
}

void
t4_iterate(void (*func)(struct adapter *, void *), void *arg)
{
	struct adapter *sc;

	mtx_lock(&t4_list_lock);
	SLIST_FOREACH(sc, &t4_list, link) {
		/*
		 * func should not make any assumptions about what state sc is
		 * in - the only guarantee is that sc->sc_lock is a valid lock.
		 */
		func(sc, arg);
	}
	mtx_unlock(&t4_list_lock);
}

static int
t4_open(struct cdev *dev, int flags, int type, struct thread *td)
{
       return (0);
}

static int
t4_close(struct cdev *dev, int flags, int type, struct thread *td)
{
       return (0);
}

static int
t4_ioctl(struct cdev *dev, unsigned long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	int rc;
	struct adapter *sc = dev->si_drv1;

	rc = priv_check(td, PRIV_DRIVER);
	if (rc != 0)
		return (rc);

	switch (cmd) {
	case CHELSIO_T4_GETREG: {
		struct t4_reg *edata = (struct t4_reg *)data;

		if ((edata->addr & 0x3) != 0 || edata->addr >= sc->mmio_len)
			return (EFAULT);

		if (edata->size == 4)
			edata->val = t4_read_reg(sc, edata->addr);
		else if (edata->size == 8)
			edata->val = t4_read_reg64(sc, edata->addr);
		else
			return (EINVAL);

		break;
	}
	case CHELSIO_T4_SETREG: {
		struct t4_reg *edata = (struct t4_reg *)data;

		if ((edata->addr & 0x3) != 0 || edata->addr >= sc->mmio_len)
			return (EFAULT);

		if (edata->size == 4) {
			if (edata->val & 0xffffffff00000000)
				return (EINVAL);
			t4_write_reg(sc, edata->addr, (uint32_t) edata->val);
		} else if (edata->size == 8)
			t4_write_reg64(sc, edata->addr, edata->val);
		else
			return (EINVAL);
		break;
	}
	case CHELSIO_T4_REGDUMP: {
		struct t4_regdump *regs = (struct t4_regdump *)data;
		int reglen = T4_REGDUMP_SIZE;
		uint8_t *buf;

		if (regs->len < reglen) {
			regs->len = reglen; /* hint to the caller */
			return (ENOBUFS);
		}

		regs->len = reglen;
		buf = malloc(reglen, M_CXGBE, M_WAITOK | M_ZERO);
		t4_get_regs(sc, regs, buf);
		rc = copyout(buf, regs->data, reglen);
		free(buf, M_CXGBE);
		break;
	}
	case CHELSIO_T4_GET_FILTER_MODE:
		rc = get_filter_mode(sc, (uint32_t *)data);
		break;
	case CHELSIO_T4_SET_FILTER_MODE:
		rc = set_filter_mode(sc, *(uint32_t *)data);
		break;
	case CHELSIO_T4_GET_FILTER:
		ADAPTER_LOCK(sc);
		rc = get_filter(sc, (struct t4_filter *)data);
		ADAPTER_UNLOCK(sc);
		break;
	case CHELSIO_T4_SET_FILTER:
		ADAPTER_LOCK(sc);
		rc = set_filter(sc, (struct t4_filter *)data);
		ADAPTER_UNLOCK(sc);
		break;
	case CHELSIO_T4_DEL_FILTER:
		ADAPTER_LOCK(sc);
		rc = del_filter(sc, (struct t4_filter *)data);
		ADAPTER_UNLOCK(sc);
		break;
	case CHELSIO_T4_GET_SGE_CONTEXT:
		rc = get_sge_context(sc, (struct t4_sge_context *)data);
		break;
	case CHELSIO_T4_LOAD_FW: {
		struct t4_data *fw = (struct t4_data *)data;
		uint8_t *fw_data;

		if (sc->flags & FULL_INIT_DONE)
			return (EBUSY);

		fw_data = malloc(fw->len, M_CXGBE, M_NOWAIT);
		if (fw_data == NULL)
			return (ENOMEM);

		rc = copyin(fw->data, fw_data, fw->len);
		if (rc == 0)
			rc = -t4_load_fw(sc, fw_data, fw->len);

		free(fw_data, M_CXGBE);
		break;
	}
	case CHELSIO_T4_GET_MEM:
		rc = read_card_mem(sc, (struct t4_mem_range *)data);
		break;
	default:
		rc = EINVAL;
	}

	return (rc);
}

#ifdef TCP_OFFLOAD
static int
toe_capability(struct port_info *pi, int enable)
{
	int rc;
	struct adapter *sc = pi->adapter;

	ADAPTER_LOCK_ASSERT_OWNED(sc);

	if (!is_offload(sc))
		return (ENODEV);

	if (enable) {
		if (!(sc->flags & FULL_INIT_DONE)) {
			log(LOG_WARNING,
			    "You must enable a cxgbe interface first\n");
			return (EAGAIN);
		}

		if (isset(&sc->offload_map, pi->port_id))
			return (0);

		if (!(sc->flags & TOM_INIT_DONE)) {
			rc = t4_activate_uld(sc, ULD_TOM);
			if (rc == EAGAIN) {
				log(LOG_WARNING,
				    "You must kldload t4_tom.ko before trying "
				    "to enable TOE on a cxgbe interface.\n");
			}
			if (rc != 0)
				return (rc);
			KASSERT(sc->tom_softc != NULL,
			    ("%s: TOM activated but softc NULL", __func__));
			KASSERT(sc->flags & TOM_INIT_DONE,
			    ("%s: TOM activated but flag not set", __func__));
		}

		setbit(&sc->offload_map, pi->port_id);
	} else {
		if (!isset(&sc->offload_map, pi->port_id))
			return (0);

		KASSERT(sc->flags & TOM_INIT_DONE,
		    ("%s: TOM never initialized?", __func__));
		clrbit(&sc->offload_map, pi->port_id);
	}

	return (0);
}

/*
 * Add an upper layer driver to the global list.
 */
int
t4_register_uld(struct uld_info *ui)
{
	int rc = 0;
	struct uld_info *u;

	mtx_lock(&t4_uld_list_lock);
	SLIST_FOREACH(u, &t4_uld_list, link) {
	    if (u->uld_id == ui->uld_id) {
		    rc = EEXIST;
		    goto done;
	    }
	}

	SLIST_INSERT_HEAD(&t4_uld_list, ui, link);
	ui->refcount = 0;
done:
	mtx_unlock(&t4_uld_list_lock);
	return (rc);
}

int
t4_unregister_uld(struct uld_info *ui)
{
	int rc = EINVAL;
	struct uld_info *u;

	mtx_lock(&t4_uld_list_lock);

	SLIST_FOREACH(u, &t4_uld_list, link) {
	    if (u == ui) {
		    if (ui->refcount > 0) {
			    rc = EBUSY;
			    goto done;
		    }

		    SLIST_REMOVE(&t4_uld_list, ui, uld_info, link);
		    rc = 0;
		    goto done;
	    }
	}
done:
	mtx_unlock(&t4_uld_list_lock);
	return (rc);
}

int
t4_activate_uld(struct adapter *sc, int id)
{
	int rc = EAGAIN;
	struct uld_info *ui;

	mtx_lock(&t4_uld_list_lock);

	SLIST_FOREACH(ui, &t4_uld_list, link) {
		if (ui->uld_id == id) {
			rc = ui->activate(sc);
			if (rc == 0)
				ui->refcount++;
			goto done;
		}
	}
done:
	mtx_unlock(&t4_uld_list_lock);

	return (rc);
}

int
t4_deactivate_uld(struct adapter *sc, int id)
{
	int rc = EINVAL;
	struct uld_info *ui;

	mtx_lock(&t4_uld_list_lock);

	SLIST_FOREACH(ui, &t4_uld_list, link) {
		if (ui->uld_id == id) {
			rc = ui->deactivate(sc);
			if (rc == 0)
				ui->refcount--;
			goto done;
		}
	}
done:
	mtx_unlock(&t4_uld_list_lock);

	return (rc);
}
#endif

/*
 * Come up with reasonable defaults for some of the tunables, provided they're
 * not set by the user (in which case we'll use the values as is).
 */
static void
tweak_tunables(void)
{
	int nc = mp_ncpus;	/* our snapshot of the number of CPUs */

	if (t4_ntxq10g < 1)
		t4_ntxq10g = min(nc, NTXQ_10G);

	if (t4_ntxq1g < 1)
		t4_ntxq1g = min(nc, NTXQ_1G);

	if (t4_nrxq10g < 1)
		t4_nrxq10g = min(nc, NRXQ_10G);

	if (t4_nrxq1g < 1)
		t4_nrxq1g = min(nc, NRXQ_1G);

#ifdef TCP_OFFLOAD
	if (t4_nofldtxq10g < 1)
		t4_nofldtxq10g = min(nc, NOFLDTXQ_10G);

	if (t4_nofldtxq1g < 1)
		t4_nofldtxq1g = min(nc, NOFLDTXQ_1G);

	if (t4_nofldrxq10g < 1)
		t4_nofldrxq10g = min(nc, NOFLDRXQ_10G);

	if (t4_nofldrxq1g < 1)
		t4_nofldrxq1g = min(nc, NOFLDRXQ_1G);

	if (t4_toecaps_allowed == -1)
		t4_toecaps_allowed = FW_CAPS_CONFIG_TOE;
#else
	if (t4_toecaps_allowed == -1)
		t4_toecaps_allowed = 0;
#endif

	if (t4_tmr_idx_10g < 0 || t4_tmr_idx_10g >= SGE_NTIMERS)
		t4_tmr_idx_10g = TMR_IDX_10G;

	if (t4_pktc_idx_10g < -1 || t4_pktc_idx_10g >= SGE_NCOUNTERS)
		t4_pktc_idx_10g = PKTC_IDX_10G;

	if (t4_tmr_idx_1g < 0 || t4_tmr_idx_1g >= SGE_NTIMERS)
		t4_tmr_idx_1g = TMR_IDX_1G;

	if (t4_pktc_idx_1g < -1 || t4_pktc_idx_1g >= SGE_NCOUNTERS)
		t4_pktc_idx_1g = PKTC_IDX_1G;

	if (t4_qsize_txq < 128)
		t4_qsize_txq = 128;

	if (t4_qsize_rxq < 128)
		t4_qsize_rxq = 128;
	while (t4_qsize_rxq & 7)
		t4_qsize_rxq++;

	t4_intr_types &= INTR_MSIX | INTR_MSI | INTR_INTX;
}

static int
t4_mod_event(module_t mod, int cmd, void *arg)
{
	int rc = 0;

	switch (cmd) {
	case MOD_LOAD:
		t4_sge_modload();
		mtx_init(&t4_list_lock, "T4 adapters", 0, MTX_DEF);
		SLIST_INIT(&t4_list);
#ifdef TCP_OFFLOAD
		mtx_init(&t4_uld_list_lock, "T4 ULDs", 0, MTX_DEF);
		SLIST_INIT(&t4_uld_list);
#endif
		tweak_tunables();
		break;

	case MOD_UNLOAD:
#ifdef TCP_OFFLOAD
		mtx_lock(&t4_uld_list_lock);
		if (!SLIST_EMPTY(&t4_uld_list)) {
			rc = EBUSY;
			mtx_unlock(&t4_uld_list_lock);
			break;
		}
		mtx_unlock(&t4_uld_list_lock);
		mtx_destroy(&t4_uld_list_lock);
#endif
		mtx_lock(&t4_list_lock);
		if (!SLIST_EMPTY(&t4_list)) {
			rc = EBUSY;
			mtx_unlock(&t4_list_lock);
			break;
		}
		mtx_unlock(&t4_list_lock);
		mtx_destroy(&t4_list_lock);
		break;
	}

	return (rc);
}

static devclass_t t4_devclass;
static devclass_t cxgbe_devclass;

DRIVER_MODULE(t4nex, pci, t4_driver, t4_devclass, t4_mod_event, 0);
MODULE_VERSION(t4nex, 1);

DRIVER_MODULE(cxgbe, t4nex, cxgbe_driver, cxgbe_devclass, 0, 0);
MODULE_VERSION(cxgbe, 1);
