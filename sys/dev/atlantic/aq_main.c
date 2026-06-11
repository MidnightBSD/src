/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2019 aQuantia Corporation. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   (1) Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer.
 *
 *   (2) Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   (3)The name of the author may not be used to endorse or promote
 *   products derived from this software without specific prior
 *   written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_rss.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitstring.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/priv.h>
#include <sys/rman.h>
#include <sys/sbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_var.h>
#include <net/iflib.h>
#include <net/rss_config.h>

#include "aq_dbg.h"
#include "aq_device.h"
#include "aq_fw.h"
#include "aq_hostboot.h"
#include "aq_hw.h"
#include "aq_hw_llh.h"
#include "aq_ring.h"
#include "ifdi_if.h"

#define AQ_XXX_UNIMPLEMENTED_FUNCTION                                  \
	do {                                                           \
		printf("atlantic: unimplemented function: %s@%s:%d\n", \
		    __func__, __FILE__, __LINE__);                     \
	} while (0)

MALLOC_DEFINE(M_AQ, "aq", "Aquantia");

char aq_driver_version[] = AQ_VER;

#define AQUANTIA_VENDOR_ID 0x1D6A

#define AQ_DEVICE_ID_0001 0x0001
#define AQ_DEVICE_ID_D100 0xD100
#define AQ_DEVICE_ID_D107 0xD107
#define AQ_DEVICE_ID_D108 0xD108
#define AQ_DEVICE_ID_D109 0xD109

#define AQ_DEVICE_ID_AQC100 0x00B1
#define AQ_DEVICE_ID_AQC107 0x07B1
#define AQ_DEVICE_ID_AQC108 0x08B1
#define AQ_DEVICE_ID_AQC109 0x09B1
#define AQ_DEVICE_ID_AQC111 0x11B1
#define AQ_DEVICE_ID_AQC112 0x12B1

#define AQ_DEVICE_ID_AQC100S 0x80B1
#define AQ_DEVICE_ID_AQC107S 0x87B1
#define AQ_DEVICE_ID_AQC108S 0x88B1
#define AQ_DEVICE_ID_AQC109S 0x89B1
#define AQ_DEVICE_ID_AQC111S 0x91B1
#define AQ_DEVICE_ID_AQC112S 0x92B1

#define AQ_DEVICE_ID_AQC113DEV 0x00C0
#define AQ_DEVICE_ID_AQC113CS 0x94C0
#define AQ_DEVICE_ID_AQC113CA 0x34C0
#define AQ_DEVICE_ID_AQC114CS 0x93C0
#define AQ_DEVICE_ID_AQC113 0x04C0
#define AQ_DEVICE_ID_AQC113C 0x14C0
#define AQ_DEVICE_ID_AQC115C 0x12C0
#define AQ_DEVICE_ID_AQC116C 0x11C0

static pci_vendor_info_t aq_vendor_info_array[] = {
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_0001,
	    "Aquantia AQtion 10Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_D107,
	    "Aquantia AQtion 10Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_D108,
	    "Aquantia AQtion 5Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_D109,
	    "Aquantia AQtion 2.5Gbit Network Adapter"),

	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC107,
	    "Aquantia AQtion 10Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC108,
	    "Aquantia AQtion 5Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC109,
	    "Aquantia AQtion 2.5Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC100,
	    "Aquantia AQtion 10Gbit Network Adapter"),

	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC107S,
	    "Aquantia AQtion 10Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC108S,
	    "Aquantia AQtion 5Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC109S,
	    "Aquantia AQtion 2.5Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC100S,
	    "Aquantia AQtion 10Gbit Network Adapter"),

	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC111,
	    "Aquantia AQtion 5Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC112,
	    "Aquantia AQtion 2.5Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC111S,
	    "Aquantia AQtion 5Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC112S,
	    "Aquantia AQtion 2.5Gbit Network Adapter"),

	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC113DEV,
	    "Aquantia AQtion 10Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC113CS,
	    "Aquantia AQtion 10Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC113CA,
	    "Aquantia AQtion 10Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC114CS,
	    "Aquantia AQtion 10Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC113,
	    "Aquantia AQtion 10Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC113C,
	    "Aquantia AQtion 10Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC115C,
	    "Aquantia AQtion 2.5Gbit Network Adapter"),
	PVID(AQUANTIA_VENDOR_ID, AQ_DEVICE_ID_AQC116C,
	    "Aquantia AQtion 1Gbit Network Adapter"),

	PVID_END
};

/* Device setup, teardown, etc */
static void *aq_register(device_t dev);
static int aq_module_event(module_t mod, int what, void *arg);
static int aq_if_attach_pre(if_ctx_t ctx);
static int aq_if_attach_post(if_ctx_t ctx);
static int aq_if_detach(if_ctx_t ctx);
static int aq_if_shutdown(if_ctx_t ctx);
static int aq_if_suspend(if_ctx_t ctx);
static int aq_if_resume(if_ctx_t ctx);

/* Soft queue setup and teardown */
static int aq_if_tx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs,
    uint64_t *paddrs, int ntxqs, int ntxqsets);
static int aq_if_rx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs,
    uint64_t *paddrs, int nrxqs, int nrxqsets);
static void aq_if_queues_free(if_ctx_t ctx);

/* Device configuration */
static void aq_if_init(if_ctx_t ctx);
static void aq_if_stop(if_ctx_t ctx);
static void aq_if_multi_set(if_ctx_t ctx);
static int aq_if_mtu_set(if_ctx_t ctx, uint32_t mtu);
static void aq_if_media_status(if_ctx_t ctx, struct ifmediareq *ifmr);
static int aq_if_media_change(if_ctx_t ctx);
static int aq_if_promisc_set(if_ctx_t ctx, int flags);
static uint64_t aq_if_get_counter(if_ctx_t ctx, ift_counter cnt);
static void aq_if_timer(if_ctx_t ctx, uint16_t qid);
static bool aq_if_needs_restart(if_ctx_t ctx, enum iflib_restart_event event);
static int aq_hw_capabilities(struct aq_dev *softc);
static void aq_add_stats_sysctls(struct aq_dev *softc);
static int aq_sysctl_phy_temp(SYSCTL_HANDLER_ARGS);
static int aq_sysctl_cable_len(SYSCTL_HANDLER_ARGS);
static int aq_sysctl_cable_diag(SYSCTL_HANDLER_ARGS);
static int aq_sysctl_fw_ver(SYSCTL_HANDLER_ARGS);
static int aq_sysctl_fw_iface_ver(SYSCTL_HANDLER_ARGS);
static int aq_sysctl_eee_rate(SYSCTL_HANDLER_ARGS);
static int aq_sysctl_eee_supported(SYSCTL_HANDLER_ARGS);
static int aq_sysctl_eee_lp_rate(SYSCTL_HANDLER_ARGS);
static int aq_sysctl_l2_filter(SYSCTL_HANDLER_ARGS);
static int aq_sysctl_l3l4_filter(SYSCTL_HANDLER_ARGS);
static int aq_sysctl_vlan_filter(SYSCTL_HANDLER_ARGS);
static int aq_sysctl_wol_phy(SYSCTL_HANDLER_ARGS);
static int aq_sysctl_wol_mask(SYSCTL_HANDLER_ARGS);
static int aq_sysctl_downshift(SYSCTL_HANDLER_ARGS);
static int aq_sysctl_media_detect(SYSCTL_HANDLER_ARGS);
static int aq_sysctl_loopback(SYSCTL_HANDLER_ARGS);
static int aq_sysctl_itr_mode(SYSCTL_HANDLER_ARGS);
static int aq_sysctl_itr(SYSCTL_HANDLER_ARGS);
static int aq_sysctl_apply_itr(struct aq_dev *softc, int itr_mode,
    uint16_t itr_tx, uint16_t itr_rx);
static void aq_rss_prepare(struct aq_dev *softc, uint32_t *rss_hash_cfg);
static bool aq_rss_udp_enabled(uint32_t rss_hash_cfg);

/* Interrupt enable / disable */
static void aq_if_enable_intr(if_ctx_t ctx);
static void aq_if_disable_intr(if_ctx_t ctx);

enum aq_itr_sysctl_id {
	AQ_ITR_SYSCTL_TX = 0,
	AQ_ITR_SYSCTL_RX = 1,
};
static int aq_if_rx_queue_intr_enable(if_ctx_t ctx, uint16_t rxqid);
static int aq_if_msix_intr_assign(if_ctx_t ctx, int msix);

/* VLAN support */
static bool aq_is_vlan_promisc_required(struct aq_dev *softc);
static void aq_update_vlan_filters(struct aq_dev *softc);
static void aq_apply_rx_filters(struct aq_dev *softc);
static void aq_if_vlan_register(if_ctx_t ctx, uint16_t vtag);
static void aq_if_vlan_unregister(if_ctx_t ctx, uint16_t vtag);

/* Informational/diagnostic */
static void aq_if_led_func(if_ctx_t ctx, int onoff);

static device_method_t aq_methods[] = { DEVMETHOD(device_register, aq_register),
	DEVMETHOD(device_probe, iflib_device_probe),
	DEVMETHOD(device_attach, iflib_device_attach),
	DEVMETHOD(device_detach, iflib_device_detach),
	DEVMETHOD(device_shutdown, iflib_device_shutdown),
	DEVMETHOD(device_suspend, iflib_device_suspend),
	DEVMETHOD(device_resume, iflib_device_resume),

	DEVMETHOD_END };

static driver_t aq_driver = {
	"aq",
	aq_methods,
	sizeof(struct aq_dev),
};

#if __FreeBSD_version >= 1400058
DRIVER_MODULE(atlantic, pci, aq_driver, aq_module_event, 0);
#else
static devclass_t aq_devclass;
DRIVER_MODULE(atlantic, pci, aq_driver, aq_devclass, aq_module_event, 0);
#endif

MODULE_DEPEND(atlantic, pci, 1, 1, 1);
MODULE_DEPEND(atlantic, ether, 1, 1, 1);
MODULE_DEPEND(atlantic, iflib, 1, 1, 1);

IFLIB_PNP_INFO(pci, atlantic, aq_vendor_info_array);

static device_method_t aq_if_methods[] = {
	/* Device setup, teardown, etc */
	DEVMETHOD(ifdi_attach_pre, aq_if_attach_pre),
	DEVMETHOD(ifdi_attach_post, aq_if_attach_post),
	DEVMETHOD(ifdi_detach, aq_if_detach),

	DEVMETHOD(ifdi_shutdown, aq_if_shutdown),
	DEVMETHOD(ifdi_suspend, aq_if_suspend),
	DEVMETHOD(ifdi_resume, aq_if_resume),

	/* Soft queue setup and teardown */
	DEVMETHOD(ifdi_tx_queues_alloc, aq_if_tx_queues_alloc),
	DEVMETHOD(ifdi_rx_queues_alloc, aq_if_rx_queues_alloc),
	DEVMETHOD(ifdi_queues_free, aq_if_queues_free),

	/* Device configuration */
	DEVMETHOD(ifdi_init, aq_if_init), DEVMETHOD(ifdi_stop, aq_if_stop),
	DEVMETHOD(ifdi_multi_set, aq_if_multi_set),
	DEVMETHOD(ifdi_mtu_set, aq_if_mtu_set),
	DEVMETHOD(ifdi_media_status, aq_if_media_status),
	DEVMETHOD(ifdi_media_change, aq_if_media_change),
	DEVMETHOD(ifdi_promisc_set, aq_if_promisc_set),
	DEVMETHOD(ifdi_get_counter, aq_if_get_counter),
	DEVMETHOD(ifdi_update_admin_status, aq_if_update_admin_status),
	DEVMETHOD(ifdi_timer, aq_if_timer),
	DEVMETHOD(ifdi_needs_restart, aq_if_needs_restart),

	/* Interrupt enable / disable */
	DEVMETHOD(ifdi_intr_enable, aq_if_enable_intr),
	DEVMETHOD(ifdi_intr_disable, aq_if_disable_intr),
	DEVMETHOD(ifdi_rx_queue_intr_enable, aq_if_rx_queue_intr_enable),
	DEVMETHOD(ifdi_tx_queue_intr_enable, aq_if_rx_queue_intr_enable),
	DEVMETHOD(ifdi_msix_intr_assign, aq_if_msix_intr_assign),

	/* VLAN support */
	DEVMETHOD(ifdi_vlan_register, aq_if_vlan_register),
	DEVMETHOD(ifdi_vlan_unregister, aq_if_vlan_unregister),

	/* Informational/diagnostic */
	DEVMETHOD(ifdi_led_func, aq_if_led_func),

	DEVMETHOD_END
};

static driver_t aq_if_driver = { "aq_if", aq_if_methods,
	sizeof(struct aq_dev) };

static struct if_shared_ctx aq_sctx_init = {
	.isc_magic = IFLIB_MAGIC,
	.isc_q_align = PAGE_SIZE,

	/* Chip MTU-derived fields are updated in aq_if_attach_pre */
	.isc_tx_maxsize = HW_ATL_B0_TSO_SIZE,
	.isc_tx_maxsegsize = HW_ATL_B0_MTU_JUMBO,
#if __FreeBSD__ >= 12
	.isc_tso_maxsize = HW_ATL_B0_TSO_SIZE,
	.isc_tso_maxsegsize = HW_ATL_B0_MTU_JUMBO,
#endif
	.isc_rx_maxsize = HW_ATL_B0_MTU_JUMBO,
	.isc_rx_nsegments = 16,
	.isc_rx_maxsegsize = PAGE_SIZE,
	.isc_nfl = 1,
	.isc_nrxqs = 1,
	.isc_ntxqs = 1,
	.isc_admin_intrcnt = 1,
	.isc_vendor_info = aq_vendor_info_array,
	.isc_driver_version = aq_driver_version,
	.isc_driver = &aq_if_driver,
	.isc_flags = IFLIB_NEED_SCRATCH | IFLIB_TSO_INIT_IP |
	    IFLIB_NEED_ZERO_CSUM,

	.isc_nrxd_min = { HW_ATL_B0_MIN_RXD },
	.isc_ntxd_min = { HW_ATL_B0_MIN_TXD },
	.isc_nrxd_max = { HW_ATL_B0_MAX_RXD },
	.isc_ntxd_max = { HW_ATL_B0_MAX_TXD },
	.isc_nrxd_default = { PAGE_SIZE / sizeof(aq_txc_desc_t) * 4 },
	.isc_ntxd_default = { PAGE_SIZE / sizeof(aq_txc_desc_t) * 4 },
};

/*
 * TUNEABLE PARAMETERS:
 */

static SYSCTL_NODE(_hw, OID_AUTO, aq, CTLFLAG_RD, 0,
    "Atlantic driver parameters");
/* UDP Receive-Side Scaling */
static int aq_enable_rss_udp = 1;
SYSCTL_INT(_hw_aq, OID_AUTO, enable_rss_udp, CTLFLAG_RDTUN, &aq_enable_rss_udp,
    0, "Enable Receive-Side Scaling (RSS) for UDP");

#define AQ_RSS_UDP_HASH_TYPES                                    \
	(RSS_HASHTYPE_RSS_UDP_IPV4 | RSS_HASHTYPE_RSS_UDP_IPV6 | \
	    RSS_HASHTYPE_RSS_UDP_IPV6_EX)

/*
 * Device Methods
 */
static void *
aq_register(device_t dev)
{
	return (&aq_sctx_init);
}

static int
aq_module_event(module_t mod __unused, int what, void *arg __unused)
{
	switch (what) {
	case MOD_LOAD:
		return (aq_hostboot_module_load());
	case MOD_QUIESCE:
		return (0);
	case MOD_UNLOAD:
		return (aq_hostboot_module_unload());
	default:
		return (EOPNOTSUPP);
	}
}

static void
aq_rss_prepare(struct aq_dev *softc, uint32_t *rss_hash_cfg)
{
	bool rss_enabled;
	uint32_t qcnt;
	uint32_t i;

	rss_enabled = aq_rss_enabled(softc);
	qcnt = rss_enabled ? softc->rx_rings_count : 1U;

	if (!rss_enabled) {
		memset(softc->rss_key, 0, sizeof(softc->rss_key));
		memset(softc->rss_table, 0, sizeof(softc->rss_table));
		*rss_hash_cfg = 0;
		return;
	}

	rss_getkey(softc->rss_key);
	*rss_hash_cfg = rss_gethashconfig();

#ifdef RSS
	if (rss_gethashalgo() == RSS_HASH_TOEPLITZ) {
		for (i = 0; i < ARRAY_SIZE(softc->rss_table); i++) {
			softc->rss_table[i] =
			    (uint8_t)(rss_get_indirection_to_bucket(i) % qcnt);
		}
	} else
#endif
	{
		for (i = 0; i < ARRAY_SIZE(softc->rss_table); i++) {
			softc->rss_table[i] = (uint8_t)(i % qcnt);
		}
	}
}

static bool
aq_rss_udp_enabled(uint32_t rss_hash_cfg)
{
	return (
	    aq_enable_rss_udp && ((rss_hash_cfg & AQ_RSS_UDP_HASH_TYPES) != 0));
}

static int
aq_if_attach_pre(if_ctx_t ctx)
{
	struct aq_dev *softc;
	struct aq_hw *hw;
	if_softc_ctx_t scctx;
	uint32_t mtu_jumbo;
	int rc;

	AQ_DBG_ENTER();
	softc = iflib_get_softc(ctx);
	rc = 0;

	softc->ctx = ctx;
	softc->dev = iflib_get_dev(ctx);
	softc->media = iflib_get_media(ctx);
	softc->scctx = iflib_get_softc_ctx(ctx);
	softc->sctx = iflib_get_sctx(ctx);
	scctx = softc->scctx;

	softc->mmio_rid = PCIR_BAR(0);
	softc->mmio_res = bus_alloc_resource_any(softc->dev, SYS_RES_MEMORY,
	    &softc->mmio_rid, RF_ACTIVE | RF_SHAREABLE);
	if (softc->mmio_res == NULL) {
		device_printf(softc->dev,
		    "failed to allocate MMIO resources\n");
		rc = ENXIO;
		goto fail;
	}

	softc->mmio_tag = rman_get_bustag(softc->mmio_res);
	softc->mmio_handle = rman_get_bushandle(softc->mmio_res);
	softc->mmio_size = rman_get_size(softc->mmio_res);
	softc->hw.hw_addr = (uint8_t *)softc->mmio_handle;
	hw = &softc->hw;
	hw->aq_dev = softc;
	hw->device_id = pci_get_device(softc->dev);
	hw->vendor_id = pci_get_vendor(softc->dev);
	hw->subsystem_vendor_id = pci_get_subvendor(softc->dev);
	hw->subsystem_device_id = pci_get_subdevice(softc->dev);
	hw->revision_id = pci_get_revid(softc->dev);
	hw->chip_id = AQ_READ_REG(hw, 0x10);
	hw->chip_rev = AQ_READ_REG(hw, 0x14);
	switch (hw->device_id) {
	case AQ_DEVICE_ID_AQC113DEV:
	case AQ_DEVICE_ID_AQC113CS:
	case AQ_DEVICE_ID_AQC113CA:
	case AQ_DEVICE_ID_AQC114CS:
	case AQ_DEVICE_ID_AQC113:
	case AQ_DEVICE_ID_AQC113C:
	case AQ_DEVICE_ID_AQC115C:
	case AQ_DEVICE_ID_AQC116C:
		hw->chip_features |= AQ_HW_CHIP_ANTIGUA;
		break;
	default:
		hw->chip_features |= AQ_HW_CHIP_ATLANTIC;
		break;
	}
	hw->link_rate = aq_fw_speed_auto;
	if (!AQ_HW_IS_AQ2(hw))
		hw->link_rate &= ~aq_fw_10M;
	hw->itr_mode = AQ_ITR_MODE_AUTO;
	hw->itr_tx = 0;
	hw->itr_rx = 0;
	hw->fc.fc_rx = 1;
	hw->fc.fc_tx = 1;
	hw->eee_rate = 0;
	softc->linkup = 0U;
	softc->last_stats_valid = false;
	memset(&softc->accum_stats, 0, sizeof(softc->accum_stats));
	softc->wol_phy = false;
	softc->wol_mask = 0;
	softc->downshift = 0;
	softc->media_detect = false;
	softc->loopback_mode = 0;
	hw->rpc_addr = 0;
	hw->rpc_tid = 0;
	hw->rpc_len = 0;
	memset(hw->rpc_buf, 0, sizeof(hw->rpc_buf));
	hw->settings_addr = 0;
	mtx_init(&softc->aq2_fw_request_mtx, "aq2 fwreq", NULL, MTX_DEF);

	for (int i = 0; i < AQ_HW_ETYPE_MAX_FILTERS; i++) {
		softc->rx_filters.etype_filters[i].queue = -1;
		softc->rx_filters.etype_filters[i].location = (uint8_t)i;
	}
	for (int i = 0; i < AQ_HW_L3L4_MAX_FILTERS; i++) {
		softc->rx_filters.l3l4_filters[i].location = (uint8_t)i;
	}
	for (int i = 0; i < AQ_HW_VLAN_MAX_FILTERS; i++) {
		softc->rx_filters.vlan_filters[i].queue = 0xFF;
		softc->rx_filters.vlan_filters[i].location = (uint8_t)i;
	}

	aq_hostboot_init(softc);

	/* Look up ops and caps. */
	rc = aq_hw_mpi_create(hw);
	if (rc != 0) {
		AQ_DBG_ERROR(" %s: aq_hw_mpi_create fail err=%d", __func__, rc);
		goto fail;
	}

	if (hw->fast_start_enabled) {
		if (hw->fw_ops && hw->fw_ops->reset)
			hw->fw_ops->reset(hw);
	} else {
		rc = aq_hw_reset(&softc->hw);
		if (rc != 0)
			goto fail;
	}
	aq_hw_capabilities(softc);

	rc = aq_hw_get_mac_permanent(hw, hw->mac_addr);
	if (rc != 0) {
		AQ_DBG_ERROR("Unable to get mac addr from hw");
		goto fail;
	}
	mtu_jumbo = aq_hw_mtu_jumbo(hw);
	aq_sctx_init.isc_tx_maxsegsize = mtu_jumbo;
#if __FreeBSD__ >= 12
	aq_sctx_init.isc_tso_maxsegsize = mtu_jumbo;
#endif
	aq_sctx_init.isc_rx_maxsize = mtu_jumbo;

	softc->admin_ticks = 0;

	iflib_set_mac(ctx, hw->mac_addr);
#if __FreeBSD__ < 13
	/* since FreeBSD13 deadlock due to calling iflib_led_func() under
	 * CTX_LOCK() */
	iflib_led_create(ctx);
#endif
	scctx->isc_tx_csum_flags = CSUM_IP | CSUM_TCP | CSUM_UDP | CSUM_TSO |
	    CSUM_IP6_TCP | CSUM_IP6_UDP | CSUM_IP6_TSO;
#if __FreeBSD__ >= 12
	scctx->isc_capabilities = IFCAP_HWCSUM | IFCAP_HWCSUM_IPV6 | IFCAP_TSO |
	    IFCAP_JUMBO_MTU | IFCAP_VLAN_HWFILTER | IFCAP_VLAN_MTU |
	    IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_HWCSUM;
#ifdef IFCAP_WOL_MAGIC
	scctx->isc_capabilities |= IFCAP_WOL_MAGIC;
#elif defined(IFCAP_WOL)
	scctx->isc_capabilities |= IFCAP_WOL;
#endif
	scctx->isc_capenable = scctx->isc_capabilities;
#else
	if_t ifp;
	int cap;
	ifp = iflib_get_ifp(ctx);
	cap = IFCAP_HWCSUM | IFCAP_HWCSUM_IPV6 | IFCAP_TSO | IFCAP_JUMBO_MTU |
	    IFCAP_VLAN_HWFILTER | IFCAP_VLAN_MTU | IFCAP_VLAN_HWTAGGING |
	    IFCAP_VLAN_HWCSUM;
#ifdef IFCAP_WOL_MAGIC
	cap |= IFCAP_WOL_MAGIC;
#elif defined(IFCAP_WOL)
	cap |= IFCAP_WOL;
#endif
	if_setcapenable(ifp, cap);
#endif
	scctx->isc_tx_nsegments = 31, scctx->isc_tx_tso_segments_max = 31;
	scctx->isc_tx_tso_size_max = HW_ATL_B0_TSO_SIZE -
	    sizeof(struct ether_vlan_header);
	scctx->isc_tx_tso_segsize_max = mtu_jumbo;
	scctx->isc_min_frame_size = 52;
	scctx->isc_txrx = &aq_txrx;

	scctx->isc_txqsizes[0] = sizeof(aq_tx_desc_t) * scctx->isc_ntxd[0];
	scctx->isc_rxqsizes[0] = sizeof(aq_rx_desc_t) * scctx->isc_nrxd[0];

	scctx->isc_ntxqsets_max = AQ_HW_TX_MAX_QUEUES;
	scctx->isc_nrxqsets_max = AQ_HW_RX_MAX_QUEUES;

	/* iflib will map and release this bar */
	scctx->isc_msix_bar = pci_msix_table_bar(softc->dev);

	softc->vlan_tags = bit_alloc(4096, M_AQ, M_NOWAIT);

	AQ_DBG_EXIT(rc);
	return (rc);

fail:
	if (mtx_initialized(&softc->aq2_fw_request_mtx))
		mtx_destroy(&softc->aq2_fw_request_mtx);

	if (softc->mmio_res != NULL)
		bus_release_resource(softc->dev, SYS_RES_MEMORY,
		    softc->mmio_rid, softc->mmio_res);

	AQ_DBG_EXIT(rc);
	return (rc);
}

static int
aq_if_attach_post(if_ctx_t ctx)
{
	struct aq_dev *softc;
	struct ifmediareq ifmr;
	int err;
	int rc;

	AQ_DBG_ENTER();

	softc = iflib_get_softc(ctx);
	rc = 0;

	aq_update_hw_stats(softc);

	aq_initmedia(softc);

	switch (softc->scctx->isc_intr) {
	case IFLIB_INTR_LEGACY:
		rc = EOPNOTSUPP;
		goto exit;
		goto exit;
		break;
	case IFLIB_INTR_MSI:
		break;
	case IFLIB_INTR_MSIX:
		break;
	default:
		device_printf(softc->dev, "unknown interrupt mode\n");
		rc = EOPNOTSUPP;
		goto exit;
	}

	aq_add_stats_sysctls(softc);

	/*
	 * aq_if_init is only called when brought up by ifconfig up.
	 * Without this, ifconfig carrier state after kldload is misleading.
	 */
	err = aq_hw_set_link_speed(&softc->hw, softc->hw.link_rate);
	if (err != EOK) {
		device_printf(softc->dev, "atlantic: aq_hw_set_link_speed: %d",
		    err);
	}
	aq_if_media_status(ctx, &ifmr);
exit:
	AQ_DBG_EXIT(rc);
	return (rc);
}

static int
aq_if_detach(if_ctx_t ctx)
{
	struct aq_dev *softc;
	int i;

	AQ_DBG_ENTER();
	softc = iflib_get_softc(ctx);

	aq_hw_deinit(&softc->hw);

	for (i = 0; i < softc->scctx->isc_nrxqsets; i++)
		iflib_irq_free(ctx, &softc->rx_rings[i]->irq);
	iflib_irq_free(ctx, &softc->irq);

	if (mtx_initialized(&softc->aq2_fw_request_mtx))
		mtx_destroy(&softc->aq2_fw_request_mtx);

	if (softc->mmio_res != NULL)
		bus_release_resource(softc->dev, SYS_RES_MEMORY,
		    softc->mmio_rid, softc->mmio_res);

	free(softc->vlan_tags, M_AQ);

	AQ_DBG_EXIT(0);
	return (0);
}

static int
aq_if_shutdown(if_ctx_t ctx)
{
	struct aq_dev *softc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);

	AQ_DBG_ENTER();

	aq_if_stop(ctx);
#ifdef IFCAP_WOL_MAGIC
	softc->hw.wol_flags = (if_getcapenable(ifp) & IFCAP_WOL_MAGIC) ?
	    AQ_WOL_MAGIC :
	    0;
#elif defined(IFCAP_WOL)
	softc->hw.wol_flags = (if_getcapenable(ifp) & IFCAP_WOL) ?
	    AQ_WOL_MAGIC :
	    0;
#else
	softc->hw.wol_flags = 0;
#endif
	if (softc->wol_phy)
		softc->hw.wol_flags |= AQ_WOL_PHY;
	aq_hw_set_power(&softc->hw, 0);

	AQ_DBG_EXIT(0);
	return (0);
}

static int
aq_if_suspend(if_ctx_t ctx)
{
	struct aq_dev *softc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);

	AQ_DBG_ENTER();

	aq_if_stop(ctx);
#ifdef IFCAP_WOL_MAGIC
	softc->hw.wol_flags = (if_getcapenable(ifp) & IFCAP_WOL_MAGIC) ?
	    AQ_WOL_MAGIC :
	    0;
#elif defined(IFCAP_WOL)
	softc->hw.wol_flags = (if_getcapenable(ifp) & IFCAP_WOL) ?
	    AQ_WOL_MAGIC :
	    0;
#else
	softc->hw.wol_flags = 0;
#endif
	if (softc->wol_phy)
		softc->hw.wol_flags |= AQ_WOL_PHY;
	aq_hw_set_power(&softc->hw, 0);

	AQ_DBG_EXIT(0);
	return (0);
}

static int
aq_if_resume(if_ctx_t ctx)
{
	AQ_DBG_ENTER();

	aq_if_init(ctx);

	AQ_DBG_EXIT(0);
	return (0);
}

/* Soft queue setup and teardown */
static int
aq_if_tx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs,
    int ntxqs, int ntxqsets)
{
	struct aq_dev *softc;
	struct aq_ring *ring;
	int rc = 0, i;

	AQ_DBG_ENTERA("ntxqs=%d, ntxqsets=%d", ntxqs, ntxqsets);
	softc = iflib_get_softc(ctx);
	AQ_DBG_PRINT("tx descriptors  number %d", softc->scctx->isc_ntxd[0]);

	for (i = 0; i < ntxqsets; i++) {
		ring = softc->tx_rings[i] = malloc(sizeof(struct aq_ring), M_AQ,
		    M_NOWAIT | M_ZERO);
		if (!ring) {
			rc = ENOMEM;
			device_printf(softc->dev,
			    "atlantic: tx_ring malloc fail\n");
			goto fail;
		}
		ring->tx_descs = (aq_tx_desc_t *)vaddrs[i];
		ring->tx_size = softc->scctx->isc_ntxd[0];
		ring->tx_descs_phys = paddrs[i];
		ring->tx_head = ring->tx_tail = 0;
		ring->index = i;
		ring->dev = softc;

		softc->tx_rings_count++;
	}

	AQ_DBG_EXIT(rc);
	return (rc);

fail:
	aq_if_queues_free(ctx);
	AQ_DBG_EXIT(rc);
	return (rc);
}

static int
aq_if_rx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs,
    int nrxqs, int nrxqsets)
{
	struct aq_dev *softc;
	struct aq_ring *ring;
	int rc = 0, i;

	AQ_DBG_ENTERA("nrxqs=%d, nrxqsets=%d", nrxqs, nrxqsets);
	softc = iflib_get_softc(ctx);

	for (i = 0; i < nrxqsets; i++) {
		ring = softc->rx_rings[i] = malloc(sizeof(struct aq_ring), M_AQ,
		    M_NOWAIT | M_ZERO);
		if (!ring) {
			rc = ENOMEM;
			device_printf(softc->dev,
			    "atlantic: rx_ring malloc fail\n");
			goto fail;
		}

		ring->rx_descs = (aq_rx_desc_t *)vaddrs[i];
		ring->rx_descs_phys = paddrs[i];
		ring->rx_size = softc->scctx->isc_nrxd[0];
		ring->index = i;
		ring->dev = softc;

		switch (MCLBYTES) {
		case (4 * 1024):
		case (8 * 1024):
		case (16 * 1024):
			ring->rx_max_frame_size = MCLBYTES;
			break;
		default:
			ring->rx_max_frame_size = 2048;
			break;
		}

		softc->rx_rings_count++;
	}

	AQ_DBG_EXIT(rc);
	return (rc);

fail:
	aq_if_queues_free(ctx);
	AQ_DBG_EXIT(rc);
	return (rc);
}

static void
aq_if_queues_free(if_ctx_t ctx)
{
	struct aq_dev *softc;
	int i;

	AQ_DBG_ENTER();
	softc = iflib_get_softc(ctx);

	for (i = 0; i < softc->tx_rings_count; i++) {
		if (softc->tx_rings[i]) {
			free(softc->tx_rings[i], M_AQ);
			softc->tx_rings[i] = NULL;
		}
	}
	softc->tx_rings_count = 0;
	for (i = 0; i < softc->rx_rings_count; i++) {
		if (softc->rx_rings[i]) {
			free(softc->rx_rings[i], M_AQ);
			softc->rx_rings[i] = NULL;
		}
	}
	softc->rx_rings_count = 0;

	AQ_DBG_EXIT(0);
	return;
}

/* Device configuration */
static void
aq_if_init(if_ctx_t ctx)
{
	struct aq_dev *softc;
	struct aq_hw *hw;
	if_t ifp;
	struct ifmediareq ifmr;
	uint32_t rss_hash_cfg;
	bool udp_rss_enable;
	int i, err;

	AQ_DBG_ENTER();
	softc = iflib_get_softc(ctx);
	hw = &softc->hw;
	ifp = iflib_get_ifp(ctx);

	/* Re-sync HW MAC from current ifnet LLADDR on each init */
	bcopy(if_getlladdr(ifp), hw->mac_addr, ETHER_ADDR_LEN);

	err = aq_hw_init(&softc->hw, softc->msix,
	    softc->scctx->isc_intr == IFLIB_INTR_MSIX, if_getcapenable(ifp));
	if (err != EOK) {
		device_printf(softc->dev, "atlantic: aq_hw_init: %d", err);
	}

	aq_if_media_status(ctx, &ifmr);

	aq_update_vlan_filters(softc);
	aq_apply_rx_filters(softc);
	aq_if_multi_set(ctx);

	for (i = 0; i < softc->tx_rings_count; i++) {
		struct aq_ring *ring = softc->tx_rings[i];
		err = aq_ring_tx_init(&softc->hw, ring);
		if (err) {
			device_printf(softc->dev,
			    "atlantic: aq_ring_tx_init: %d", err);
		}
		err = aq_ring_tx_start(hw, ring);
		if (err != EOK) {
			device_printf(softc->dev,
			    "atlantic: aq_ring_tx_start: %d", err);
		}
	}
	for (i = 0; i < softc->rx_rings_count; i++) {
		struct aq_ring *ring = softc->rx_rings[i];
		err = aq_ring_rx_init(&softc->hw, ring);
		if (err) {
			device_printf(softc->dev,
			    "atlantic: aq_ring_rx_init: %d", err);
		}
		err = aq_ring_rx_start(hw, ring);
		if (err != EOK) {
			device_printf(softc->dev,
			    "atlantic: aq_ring_rx_start: %d", err);
		}
		aq_if_rx_queue_intr_enable(ctx, i);
	}

	aq_rss_prepare(softc, &rss_hash_cfg);
	udp_rss_enable = aq_rss_udp_enabled(rss_hash_cfg);
	if (!udp_rss_enable)
		rss_hash_cfg &= ~AQ_RSS_UDP_HASH_TYPES;

	aq_hw_start(hw);
	aq_hw_interrupt_moderation_set(hw);
	aq_if_enable_intr(ctx);
	aq_hw_rss_hash_set(&softc->hw, softc->rss_key);
	aq_hw_rss_set(&softc->hw, softc->rss_table);
	aq_hw_rss_hash_types_set(&softc->hw, rss_hash_cfg);
	aq_hw_udp_rss_enable(hw, udp_rss_enable);
	aq_hw_set_link_speed(hw, hw->link_rate);
	aq_hw_set_eee_rate(hw, hw->eee_rate);

	err = aq_hw_set_downshift(hw, softc->downshift);
	if (err != 0 && err != ENOTSUP) {
		device_printf(softc->dev, "atlantic: aq_hw_set_downshift: %d\n",
		    err);
	}

	if (hw->fw_ops == &aq_fw2x_ops) {
		err = fw2x_set_media_detect(hw, softc->media_detect);
		if (err != 0 && err != ENOTSUP) {
			device_printf(softc->dev,
			    "atlantic: fw2x_set_media_detect: %d\n", err);
		}

		fw2x_set_loopback(hw, softc->loopback_mode);
	}

	AQ_DBG_EXIT(0);
}

static void
aq_if_stop(if_ctx_t ctx)
{
	struct aq_dev *softc;
	struct aq_hw *hw;
	int i;

	AQ_DBG_ENTER();

	softc = iflib_get_softc(ctx);
	hw = &softc->hw;

	/* disable interrupt */
	aq_if_disable_intr(ctx);

	for (i = 0; i < softc->tx_rings_count; i++) {
		aq_ring_tx_stop(hw, softc->tx_rings[i]);
		softc->tx_rings[i]->tx_head = 0;
		softc->tx_rings[i]->tx_tail = 0;
	}
	for (i = 0; i < softc->rx_rings_count; i++) {
		aq_ring_rx_stop(hw, softc->rx_rings[i]);
	}

	aq_hw_reset(&softc->hw);
	softc->last_stats_valid = false;
	memset(&softc->last_stats, 0, sizeof(softc->last_stats));
	softc->linkup = false;
	aq_if_update_admin_status(ctx);
	AQ_DBG_EXIT(0);
}

static uint64_t
aq_if_get_counter(if_ctx_t ctx, ift_counter cnt)
{
	struct aq_dev *softc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);

	switch (cnt) {
	case IFCOUNTER_IERRORS:
		return (softc->accum_stats.err_pkts_rcvd);
	case IFCOUNTER_IQDROPS:
		return (softc->accum_stats.drop_pkts_dma);
	case IFCOUNTER_OERRORS:
		return (softc->accum_stats.err_pkts_txd);
	default:
		return (if_get_counter_default(ifp, cnt));
	}
}

static int
aq_mc_slots(struct aq_hw *hw)
{
	return ((int)(aq_hw_mac_max(hw) - AQ_HW_MAC_MIN));
}

#if __FreeBSD_version >= 1300054
static u_int
aq_mc_filter_apply(void *arg, struct sockaddr_dl *dl, u_int count)
{
	struct aq_dev *softc = arg;
	struct aq_hw *hw = &softc->hw;
	uint8_t *mac_addr = NULL;
	uint32_t index;

	if ((int)count >= aq_mc_slots(hw))
		return (0);

	index = AQ_HW_MAC_MIN + count;
	mac_addr = LLADDR(dl);
	aq_hw_mac_addr_set(hw, mac_addr, index);

	/* cppcheck-suppress wrongPrintfScanfArgNum; kernel printf supports %D.
	 */
	aq_log_detail("set %u mc address %6D", index, mac_addr, ":");
	return (1);
}
#else
static int
aq_mc_filter_apply(void *arg, struct ifmultiaddr *ifma, int count)
{
	struct aq_dev *softc = arg;
	struct aq_hw *hw = &softc->hw;
	uint8_t *mac_addr = NULL;
	uint32_t index;
	if (ifma->ifma_addr->sa_family != AF_LINK)
		return (0);
	if (count >= aq_mc_slots(hw))
		return (0);

	index = AQ_HW_MAC_MIN + (uint32_t)count;
	mac_addr = LLADDR((struct sockaddr_dl *)ifma->ifma_addr);
	aq_hw_mac_addr_set(hw, mac_addr, index);

	/* cppcheck-suppress wrongPrintfScanfArgNum; kernel printf supports %D.
	 */
	aq_log_detail("set %u mc address %6D", index, mac_addr, ":");
	return (1);
}
#endif

static bool
aq_is_mc_promisc_required(struct aq_dev *softc)
{
	return (softc->mcnt > aq_mc_slots(&softc->hw));
}

static void
aq_if_multi_set(if_ctx_t ctx)
{
	struct aq_dev *softc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);
	struct aq_hw *hw = &softc->hw;
	bool mc_promisc;
	uint32_t mac_max = aq_hw_mac_max(hw);
	uint32_t i;
	AQ_DBG_ENTER();
#if __FreeBSD_version >= 1300054
	softc->mcnt = if_llmaddr_count(iflib_get_ifp(ctx));
#else
	softc->mcnt = if_multiaddr_count(iflib_get_ifp(ctx),
	    aq_mc_slots(hw) + 1);
#endif
	if (aq_is_mc_promisc_required(softc)) {
		mc_promisc = true;
	} else {
		mc_promisc = !!(if_getflags(ifp) & IFF_ALLMULTI);
#if __FreeBSD_version >= 1300054
		if_foreach_llmaddr(iflib_get_ifp(ctx), &aq_mc_filter_apply,
		    softc);
#else
		if_multi_apply(iflib_get_ifp(ctx), aq_mc_filter_apply, softc);
#endif
		for (i = AQ_HW_MAC_MIN + softc->mcnt; i < mac_max; i++)
			aq_hw_mac_addr_set(hw, NULL, (uint8_t)i);
	}
	aq_hw_set_promisc(hw, !!(if_getflags(ifp) & IFF_PROMISC),
	    aq_is_vlan_promisc_required(softc), mc_promisc);
	AQ_DBG_EXIT(0);
}

static int
aq_if_mtu_set(if_ctx_t ctx, uint32_t mtu)
{
	int err = 0;
	struct aq_dev *softc = iflib_get_softc(ctx);

	AQ_DBG_ENTER();

	if (mtu < ETHERMIN ||
	    mtu > (aq_hw_mtu_jumbo(&softc->hw) - ETHER_HDR_LEN - ETHER_CRC_LEN))
		err = EINVAL;

	AQ_DBG_EXIT(err);
	return (err);
}

static void
aq_if_media_status(if_ctx_t ctx, struct ifmediareq *ifmr)
{
	if_t ifp;

	AQ_DBG_ENTER();

	ifp = iflib_get_ifp(ctx);

	aq_mediastatus(ifp, ifmr);

	AQ_DBG_EXIT(0);
}

static int
aq_if_media_change(if_ctx_t ctx)
{
	struct aq_dev *softc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);
	int rc = 0;

	AQ_DBG_ENTER();

	/* Not allowd in UP state, since causes unsync of rings */
	if ((if_getflags(ifp) & IFF_UP)) {
		rc = EPERM;
		goto exit;
	}

	ifp = iflib_get_ifp(softc->ctx);

	rc = aq_mediachange(ifp);

exit:
	AQ_DBG_EXIT(rc);
	return (rc);
}

static int
aq_if_promisc_set(if_ctx_t ctx, int flags)
{
	struct aq_dev *softc;

	AQ_DBG_ENTER();

	softc = iflib_get_softc(ctx);

	aq_hw_set_promisc(&softc->hw, !!(flags & IFF_PROMISC),
	    aq_is_vlan_promisc_required(softc),
	    !!(flags & IFF_ALLMULTI) || aq_is_mc_promisc_required(softc));

	AQ_DBG_EXIT(0);
	return (0);
}

static bool
aq_if_needs_restart(if_ctx_t ctx __unused, enum iflib_restart_event event)
{
	switch (event) {
	case IFLIB_RESTART_VLAN_CONFIG:
	default:
		return (false);
	}
}

static void
aq_if_timer(if_ctx_t ctx, uint16_t qid)
{
	struct aq_dev *softc;
	uint64_t ticks_now;

	//	AQ_DBG_ENTER();

	softc = iflib_get_softc(ctx);
	ticks_now = ticks;

	/* Schedule aqc_if_update_admin_status() once per sec */
	if (ticks_now - softc->admin_ticks >= hz) {
		softc->admin_ticks = ticks_now;
		iflib_admin_intr_deferred(ctx);
	}

	//	AQ_DBG_EXIT(0);
	return;
}

/* Interrupt enable / disable */
static void
aq_if_enable_intr(if_ctx_t ctx)
{
	struct aq_dev *softc = iflib_get_softc(ctx);
	struct aq_hw *hw = &softc->hw;

	AQ_DBG_ENTER();

	/* Enable interrupts */
	itr_irq_msk_setlsw_set(hw, BIT(softc->msix + 1) - 1);

	AQ_DBG_EXIT(0);
}

static void
aq_if_disable_intr(if_ctx_t ctx)
{
	struct aq_dev *softc = iflib_get_softc(ctx);
	struct aq_hw *hw = &softc->hw;

	AQ_DBG_ENTER();

	/* Disable interrupts */
	itr_irq_msk_clearlsw_set(hw, BIT(softc->msix + 1) - 1);

	AQ_DBG_EXIT(0);
}

static int
aq_if_rx_queue_intr_enable(if_ctx_t ctx, uint16_t rxqid)
{
	struct aq_dev *softc = iflib_get_softc(ctx);
	struct aq_hw *hw = &softc->hw;

	AQ_DBG_ENTER();

	itr_irq_msk_setlsw_set(hw, BIT(softc->rx_rings[rxqid]->msix));

	AQ_DBG_EXIT(0);
	return (0);
}

static int
aq_if_msix_intr_assign(if_ctx_t ctx, int msix)
{
	struct aq_dev *softc;
	int i, vector = 0, rc;
	char irq_name[16];
	int rx_vectors;
	int last_rx_irq = -1;

	AQ_DBG_ENTER();
	softc = iflib_get_softc(ctx);

	for (i = 0; i < softc->rx_rings_count; i++, vector++) {
		snprintf(irq_name, sizeof(irq_name), "rxq%d", i);
		rc = iflib_irq_alloc_generic(ctx, &softc->rx_rings[i]->irq,
		    vector + 1, IFLIB_INTR_RX, aq_isr_rx, softc->rx_rings[i],
		    softc->rx_rings[i]->index, irq_name);
		device_printf(softc->dev, "Assign IRQ %u to rx ring %u\n",
		    vector, softc->rx_rings[i]->index);

		if (rc) {
			device_printf(softc->dev,
			    "failed to set up RX handler\n");
			goto fail;
		}

		softc->rx_rings[i]->msix = vector;
		last_rx_irq = i;
	}

	rx_vectors = vector;

	for (i = 0; i < softc->tx_rings_count; i++, vector++) {
		snprintf(irq_name, sizeof(irq_name), "txq%d", i);
		iflib_softirq_alloc_generic(ctx, &softc->rx_rings[i]->irq,
		    IFLIB_INTR_TX, softc->tx_rings[i], i, irq_name);

		softc->tx_rings[i]->msix = (vector % softc->rx_rings_count);
		device_printf(softc->dev, "Assign IRQ %u to tx ring %u\n",
		    softc->tx_rings[i]->msix, softc->tx_rings[i]->index);
	}

	rc = iflib_irq_alloc_generic(ctx, &softc->irq, rx_vectors + 1,
	    IFLIB_INTR_ADMIN, aq_linkstat_isr, softc, 0, "aq");
	softc->msix = rx_vectors;
	device_printf(softc->dev, "Assign IRQ %u to admin proc \n", rx_vectors);
	if (rc) {
		device_printf(iflib_get_dev(ctx),
		    "Failed to register admin handler");
		goto fail;
	}
	AQ_DBG_EXIT(0);
	return (0);

fail:
	for (i = last_rx_irq; i >= 0; i--)
		iflib_irq_free(ctx, &softc->rx_rings[i]->irq);
	AQ_DBG_EXIT(rc);
	return (rc);
}

static bool
aq_is_vlan_promisc_required(struct aq_dev *softc)
{
	if_t ifp = iflib_get_ifp(softc->ctx);
	int vlan_tag_count;

	/* If hardware VLAN filtering is disabled, stay in VLAN promisc. */
	if (!(if_getcapenable(ifp) & IFCAP_VLAN_HWFILTER))
		return (true);

	bit_count(softc->vlan_tags, 0, 4096, &vlan_tag_count);

	if (vlan_tag_count <= AQ_HW_VLAN_MAX_FILTERS)
		return (false);
	else
		return (true);
}

static void
aq_update_vlan_filters(struct aq_dev *softc)
{
	struct aq_rx_filter_vlan aq_vlans[AQ_HW_VLAN_MAX_FILTERS];
	struct aq_hw *hw = &softc->hw;
	if_t ifp = iflib_get_ifp(softc->ctx);
	bool used[4096];
	int bit_pos = 0;
	int vlan_tag = -1;
	int i;

	memset(used, 0, sizeof(used));
	memset(aq_vlans, 0, sizeof(aq_vlans));

	/*
	 * If VLAN hardware filtering is disabled, force VLAN promisc and skip
	 * programming the table so all VLAN tags are accepted.
	 */
	if (!(if_getcapenable(ifp) & IFCAP_VLAN_HWFILTER)) {
		if (AQ_HW_IS_AQ2(hw))
			aq2_hw_vlan_promisc_set(hw, true);
		else
			hw_atl_b0_hw_vlan_promisc_set(hw, true);
		return;
	}

	if (AQ_HW_IS_AQ2(hw))
		aq2_hw_vlan_promisc_set(hw, true);
	else
		hw_atl_b0_hw_vlan_promisc_set(hw, true);

	for (i = 0; i < AQ_HW_VLAN_MAX_FILTERS; i++) {
		struct aq_rx_filter_vlan *flt =
		    &softc->rx_filters.vlan_filters[i];

		if (!flt->enable)
			continue;
		if (!bit_test(softc->vlan_tags, flt->vlan_id)) {
			memset(flt, 0, sizeof(*flt));
			flt->location = (uint8_t)i;
			flt->queue = 0xFF;
			continue;
		}
		if (flt->queue != 0xFF && flt->queue >= softc->rx_rings_count) {
			memset(flt, 0, sizeof(*flt));
			flt->location = (uint8_t)i;
			flt->queue = 0xFF;
			continue;
		}
		aq_vlans[i] = *flt;
		aq_vlans[i].location = (uint8_t)i;
		used[flt->vlan_id] = true;
	}

	for (i = 0; i < AQ_HW_VLAN_MAX_FILTERS; i++) {
		if (aq_vlans[i].enable)
			continue;

		while (1) {
			bit_ffs_at(softc->vlan_tags, bit_pos, 4096, &vlan_tag);
			if (vlan_tag == -1)
				break;
			bit_pos = vlan_tag + 1;
			if (!used[vlan_tag])
				break;
		}
		if (vlan_tag != -1) {
			aq_vlans[i].enable = true;
			aq_vlans[i].location = (uint8_t)i;
			aq_vlans[i].queue = 0xFF;
			aq_vlans[i].vlan_id = (uint16_t)vlan_tag;
			used[vlan_tag] = true;
		}
	}

	if (AQ_HW_IS_AQ2(hw)) {
		aq2_hw_vlan_set(hw, aq_vlans);
		aq2_hw_vlan_promisc_set(hw, aq_is_vlan_promisc_required(softc));
	} else {
		hw_atl_b0_hw_vlan_set(hw, aq_vlans);
		hw_atl_b0_hw_vlan_promisc_set(hw,
		    aq_is_vlan_promisc_required(softc));
	}
}

static void
aq_apply_rx_filters(struct aq_dev *softc)
{
	struct aq_hw *hw = &softc->hw;
	int i;
	int err;

	for (i = 0; i < AQ_HW_ETYPE_MAX_FILTERS; i++) {
		struct aq_rx_filter_l2 *flt =
		    &softc->rx_filters.etype_filters[i];

		if (!flt->enable)
			continue;
		flt->location = (uint8_t)i;
		err = aq_hw_filter_l2_set(hw, flt);
		if (err != 0)
			device_printf(softc->dev,
			    "atlantic: l2 filter %d apply failed: %d\n", i,
			    err);
	}

	for (i = 0; i < AQ_HW_L3L4_MAX_FILTERS; i++) {
		struct aq_rx_filter_l3l4 *flt =
		    &softc->rx_filters.l3l4_filters[i];

		if ((flt->cmd & HW_ATL_RX_ENABLE_FLTR_L3L4) == 0)
			continue;
		flt->location = (uint8_t)i;
		err = aq_hw_filter_l3l4_set(hw, flt);
		if (err != 0)
			device_printf(softc->dev,
			    "atlantic: l3l4 filter %d apply failed: %d\n", i,
			    err);
	}
}

/* VLAN support */
static void
aq_if_vlan_register(if_ctx_t ctx, uint16_t vtag)
{
	struct aq_dev *softc = iflib_get_softc(ctx);

	AQ_DBG_ENTERA("%d", vtag);

	bit_set(softc->vlan_tags, vtag);

	aq_update_vlan_filters(softc);

	AQ_DBG_EXIT(0);
}

static void
aq_if_vlan_unregister(if_ctx_t ctx, uint16_t vtag)
{
	struct aq_dev *softc = iflib_get_softc(ctx);

	AQ_DBG_ENTERA("%d", vtag);

	bit_clear(softc->vlan_tags, vtag);

	aq_update_vlan_filters(softc);

	AQ_DBG_EXIT(0);
}

static void
aq_if_led_func(if_ctx_t ctx, int onoff)
{
	struct aq_dev *softc = iflib_get_softc(ctx);
	struct aq_hw *hw = &softc->hw;

	AQ_DBG_ENTERA("%d", onoff);
	if (hw->fw_ops && hw->fw_ops->led_control)
		hw->fw_ops->led_control(hw, onoff);

	AQ_DBG_EXIT(0);
}

static int
aq_hw_capabilities(struct aq_dev *softc)
{
	struct aq_hw *hw = &softc->hw;

	if (hw->vendor_id != AQUANTIA_VENDOR_ID)
		return (ENXIO);

	switch (hw->device_id) {
	case AQ_DEVICE_ID_D100:
	case AQ_DEVICE_ID_AQC100:
	case AQ_DEVICE_ID_AQC100S:
		softc->media_type = AQ_MEDIA_TYPE_FIBRE;
		softc->link_speeds = AQ_LINK_ALL & ~AQ_LINK_10G;
		break;

	case AQ_DEVICE_ID_0001:
	case AQ_DEVICE_ID_D107:
	case AQ_DEVICE_ID_AQC107:
	case AQ_DEVICE_ID_AQC107S:
	case AQ_DEVICE_ID_AQC113DEV:
	case AQ_DEVICE_ID_AQC113CS:
	case AQ_DEVICE_ID_AQC113CA:
	case AQ_DEVICE_ID_AQC113:
	case AQ_DEVICE_ID_AQC113C:
		softc->media_type = AQ_MEDIA_TYPE_TP;
		softc->link_speeds = AQ_LINK_ALL | AQ_LINK_10M;
		break;

	case AQ_DEVICE_ID_D108:
	case AQ_DEVICE_ID_AQC108:
	case AQ_DEVICE_ID_AQC108S:
	case AQ_DEVICE_ID_AQC111:
	case AQ_DEVICE_ID_AQC111S:
		softc->media_type = AQ_MEDIA_TYPE_TP;
		softc->link_speeds = AQ_LINK_ALL & ~AQ_LINK_10G;
		break;

	case AQ_DEVICE_ID_D109:
	case AQ_DEVICE_ID_AQC109:
	case AQ_DEVICE_ID_AQC109S:
	case AQ_DEVICE_ID_AQC112:
	case AQ_DEVICE_ID_AQC112S:
	case AQ_DEVICE_ID_AQC115C:
		softc->media_type = AQ_MEDIA_TYPE_TP;
		softc->link_speeds = (AQ_LINK_ALL &
					 ~(AQ_LINK_10G | AQ_LINK_5G)) |
		    AQ_LINK_10M;
		break;

	case AQ_DEVICE_ID_AQC114CS:
		softc->media_type = AQ_MEDIA_TYPE_TP;
		softc->link_speeds = (AQ_LINK_ALL & ~AQ_LINK_10G) | AQ_LINK_10M;
		break;

	case AQ_DEVICE_ID_AQC116C:
		softc->media_type = AQ_MEDIA_TYPE_TP;
		softc->link_speeds =
		    (AQ_LINK_ALL & ~(AQ_LINK_10G | AQ_LINK_5G | AQ_LINK_2G5)) |
		    AQ_LINK_10M;
		break;

	default:
		return (ENXIO);
	}

	return (0);
}

static int
aq_sysctl_print_rss_config(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	device_t dev = softc->dev;
	struct sbuf *buf;
	int error = 0;

	buf = sbuf_new_for_sysctl(NULL, NULL, 256, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for output.\n");
		return (ENOMEM);
	}

	/* Print out the redirection table */
	sbuf_cat(buf, "\nRSS Indirection table:\n");
	for (int i = 0; i < HW_ATL_RSS_INDIRECTION_TABLE_MAX; i++) {
		sbuf_printf(buf, "%d ", softc->rss_table[i]);
		if ((i + 1) % 10 == 0)
			sbuf_printf(buf, "\n");
	}

	sbuf_cat(buf, "\nRSS Key:\n");
	for (int i = 0; i < HW_ATL_RSS_HASHKEY_SIZE; i++) {
		sbuf_printf(buf, "0x%02x ", softc->rss_key[i]);
	}
	sbuf_printf(buf, "\n");

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);

	sbuf_delete(buf);

	return (0);
}

static int
aq_sysctl_phy_temp(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	int temp;
	int err;

	err = aq_hw_get_phy_temp(&softc->hw, &temp);
	if (err != 0)
		return (err);

	return (sysctl_handle_int(oidp, &temp, 0, req));
}

static int
aq_sysctl_cable_len(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	uint8_t len = 0;
	int val;
	int err;

	err = aq_hw_get_cable_len(&softc->hw, &len);
	if (err != 0)
		return (err);

	val = len;
	return (sysctl_handle_int(oidp, &val, 0, req));
}

static int
aq_sysctl_cable_diag(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	uint32_t lane_data[4];
	char buf[64];
	int err;

	err = aq_hw_get_cable_diag(&softc->hw, lane_data);
	if (err != 0)
		return (err);

	snprintf(buf, sizeof(buf), "0x%08x 0x%08x 0x%08x 0x%08x", lane_data[0],
	    lane_data[1], lane_data[2], lane_data[3]);
	return (sysctl_handle_string(oidp, buf, 0, req));
}

static int
aq_sysctl_fw_ver(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	char buf[16];

	if (softc->hw.fw_version.raw == 0) {
		buf[0] = '\0';
	} else {
		snprintf(buf, sizeof(buf), "%u.%u.%u",
		    softc->hw.fw_version.major_version,
		    softc->hw.fw_version.minor_version,
		    softc->hw.fw_version.build_number);
	}

	return (sysctl_handle_string(oidp, buf, 0, req));
}

static int
aq_sysctl_fw_iface_ver(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	char buf[16];

	if (!AQ_HW_IS_AQ2(&softc->hw))
		buf[0] = '\0';
	else if (softc->hw.aq2_iface_ver ==
	    AQ2_FW_INTERFACE_OUT_VERSION_IFACE_VER_B0)
		snprintf(buf, sizeof(buf), "B0");
	else if (softc->hw.aq2_iface_ver ==
	    AQ2_FW_INTERFACE_OUT_VERSION_IFACE_VER_A0)
		snprintf(buf, sizeof(buf), "A0");
	else
		buf[0] = '\0';

	return (sysctl_handle_string(oidp, buf, 0, req));
}

static int
aq_parse_u32(const char *val, uint32_t *out)
{
	char *endp;
	unsigned long v;

	if (val == NULL || val[0] == '\0')
		return (EINVAL);
	v = strtoul(val, &endp, 0);
	if (endp == val || *endp != '\0')
		return (EINVAL);
	if (v > UINT32_MAX)
		return (ERANGE);
	*out = (uint32_t)v;
	return (0);
}

static int
aq_parse_s32(const char *val, int *out)
{
	char *endp;
	long v;

	if (val == NULL || val[0] == '\0')
		return (EINVAL);
	v = strtol(val, &endp, 0);
	if (endp == val || *endp != '\0')
		return (EINVAL);
	if (v > INT_MAX || v < INT_MIN)
		return (ERANGE);
	*out = (int)v;
	return (0);
}

static int
aq_parse_ipv6_hex(const char *val, uint32_t out[4])
{
	char buf[9];
	const char *p = val;
	size_t len;
	char *endp;
	int i;

	if (p == NULL || p[0] == '\0')
		return (EINVAL);
	if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
		p += 2;
	len = strlen(p);
	if (len != 32)
		return (EINVAL);
	for (i = 0; i < 4; i++) {
		unsigned long v;
		memcpy(buf, p + (i * 8), 8);
		buf[8] = '\0';
		if (strspn(buf, "0123456789abcdefABCDEF") != 8)
			return (EINVAL);
		v = strtoul(buf, &endp, 16);
		if (endp == buf || *endp != '\0' || v > UINT32_MAX)
			return (EINVAL);
		out[i] = (uint32_t)v;
	}
	return (0);
}

static int
aq_parse_bool(const char *val, bool *out)
{
	uint32_t v;
	int err;

	err = aq_parse_u32(val, &v);
	if (err != 0)
		return (err);
	if (v != 0 && v != 1)
		return (EINVAL);
	*out = (v != 0);
	return (0);
}

static const char *
aq_proto_name(uint32_t cmd)
{
	uint32_t proto = cmd & 0x7U;

	if ((cmd & HW_ATL_RX_ENABLE_CMP_PROT_L4) == 0)
		return ("any");

	switch (proto) {
	case HW_ATL_RX_UDP:
		return ("udp");
	case HW_ATL_RX_SCTP:
		return ("sctp");
	case HW_ATL_RX_ICMP:
		return ("icmp");
	case HW_ATL_RX_TCP:
	default:
		return ("tcp");
	}
}

static int
aq_parse_proto(const char *val, int *proto, bool *cmp)
{
	if (val == NULL || val[0] == '\0')
		return (EINVAL);
	if (strcasecmp(val, "any") == 0) {
		*cmp = false;
		*proto = 0;
		return (0);
	}
	if (strcasecmp(val, "tcp") == 0) {
		*cmp = true;
		*proto = HW_ATL_RX_TCP;
		return (0);
	}
	if (strcasecmp(val, "udp") == 0) {
		*cmp = true;
		*proto = HW_ATL_RX_UDP;
		return (0);
	}
	if (strcasecmp(val, "sctp") == 0) {
		*cmp = true;
		*proto = HW_ATL_RX_SCTP;
		return (0);
	}
	if (strcasecmp(val, "icmp") == 0) {
		*cmp = true;
		*proto = HW_ATL_RX_ICMP;
		return (0);
	}
	return (EINVAL);
}

static const char *
aq_action_name(uint32_t cmd)
{
	uint32_t action = (cmd >> HW_ATL_RX_BOFFSET_ACTION_FL3F4) & 0x7U;

	switch (action) {
	case HW_ATL_RX_DISCARD:
		return ("drop");
	case HW_ATL_RX_HOST:
	default:
		return ("host");
	}
}

static int
aq_parse_action(const char *val, int *action)
{
	if (val == NULL || val[0] == '\0')
		return (EINVAL);
	if (strcasecmp(val, "drop") == 0) {
		*action = HW_ATL_RX_DISCARD;
		return (0);
	}
	if (strcasecmp(val, "host") == 0) {
		*action = HW_ATL_RX_HOST;
		return (0);
	}
	return (EINVAL);
}

static int
aq_sysctl_l2_filter(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	int location = (int)arg2;
	struct aq_rx_filter_l2 cfg;
	char buf[128];
	int err;

	if (location < 0 || location >= AQ_HW_ETYPE_MAX_FILTERS)
		return (EINVAL);

	cfg = softc->rx_filters.etype_filters[location];
	snprintf(buf, sizeof(buf),
	    "enable=%u,ethertype=0x%04x,queue=%d,prio_en=%u,prio=%u",
	    cfg.enable ? 1U : 0U, cfg.ethertype, cfg.queue,
	    cfg.user_priority_en ? 1U : 0U, cfg.user_priority);

	err = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (err || !req->newptr)
		return (err);
	if (buf[0] == '\0')
		return (EINVAL);

	cfg.location = (uint8_t)location;
	{
		char *p = buf;
		char *tok;

		while ((tok = strsep(&p, ",")) != NULL) {
			char *eq;

			while (*tok == ' ' || *tok == '\t')
				tok++;
			if (*tok == '\0')
				continue;
			eq = strchr(tok, '=');
			if (eq == NULL)
				return (EINVAL);
			*eq = '\0';
			eq++;
			if (strcmp(tok, "enable") == 0) {
				bool v;
				err = aq_parse_bool(eq, &v);
				if (err != 0)
					return (err);
				cfg.enable = v;
			} else if (strcmp(tok, "ethertype") == 0) {
				uint32_t v;
				err = aq_parse_u32(eq, &v);
				if (err != 0 || v > 0xFFFFU)
					return (EINVAL);
				cfg.ethertype = (uint16_t)v;
			} else if (strcmp(tok, "queue") == 0) {
				int v;
				err = aq_parse_s32(eq, &v);
				if (err != 0)
					return (err);
				cfg.queue = (int8_t)v;
			} else if (strcmp(tok, "prio_en") == 0) {
				bool v;
				err = aq_parse_bool(eq, &v);
				if (err != 0)
					return (err);
				cfg.user_priority_en = v;
			} else if (strcmp(tok, "prio") == 0) {
				uint32_t v;
				err = aq_parse_u32(eq, &v);
				if (err != 0 || v > 7U)
					return (EINVAL);
				cfg.user_priority = (uint8_t)v;
			} else {
				return (EINVAL);
			}
		}
	}

	if (cfg.queue < -1)
		return (EINVAL);
	if (cfg.queue >= 0 && (uint32_t)cfg.queue >= softc->rx_rings_count)
		return (EINVAL);

	if (!cfg.enable) {
		cfg.queue = -1;
		err = aq_hw_filter_l2_clear(&softc->hw, &cfg);
		if (err != 0)
			return (err);
		memset(&cfg, 0, sizeof(cfg));
		cfg.location = (uint8_t)location;
		cfg.queue = -1;
	} else {
		err = aq_hw_filter_l2_set(&softc->hw, &cfg);
		if (err != 0)
			return (err);
	}

	softc->rx_filters.etype_filters[location] = cfg;
	return (0);
}

static int
aq_sysctl_vlan_filter(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	int location = (int)arg2;
	struct aq_rx_filter_vlan cfg;
	char buf[96];
	int err;
	int queue = -1;

	if (location < 0 || location >= AQ_HW_VLAN_MAX_FILTERS)
		return (EINVAL);

	cfg = softc->rx_filters.vlan_filters[location];
	if (cfg.queue != 0xFF)
		queue = (int)cfg.queue;
	snprintf(buf, sizeof(buf), "enable=%u,vlan=%u,queue=%d",
	    cfg.enable ? 1U : 0U, cfg.vlan_id, queue);

	err = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (err || !req->newptr)
		return (err);
	if (buf[0] == '\0')
		return (EINVAL);

	cfg.location = (uint8_t)location;
	{
		char *p = buf;
		char *tok;

		while ((tok = strsep(&p, ",")) != NULL) {
			char *eq;

			while (*tok == ' ' || *tok == '\t')
				tok++;
			if (*tok == '\0')
				continue;
			eq = strchr(tok, '=');
			if (eq == NULL)
				return (EINVAL);
			*eq = '\0';
			eq++;
			if (strcmp(tok, "enable") == 0) {
				bool v;
				err = aq_parse_bool(eq, &v);
				if (err != 0)
					return (err);
				cfg.enable = v;
			} else if (strcmp(tok, "vlan") == 0) {
				uint32_t v;
				err = aq_parse_u32(eq, &v);
				if (err != 0 || v > 4095U)
					return (EINVAL);
				cfg.vlan_id = (uint16_t)v;
			} else if (strcmp(tok, "queue") == 0) {
				int v;
				err = aq_parse_s32(eq, &v);
				if (err != 0)
					return (err);
				queue = v;
			} else {
				return (EINVAL);
			}
		}
	}

	if (queue < -1)
		return (EINVAL);
	if (queue >= 0 && (uint32_t)queue >= softc->rx_rings_count)
		return (EINVAL);

	if (!cfg.enable) {
		memset(&cfg, 0, sizeof(cfg));
		cfg.location = (uint8_t)location;
		cfg.queue = 0xFF;
		softc->rx_filters.vlan_filters[location] = cfg;
		aq_update_vlan_filters(softc);
		return (0);
	}

	if (!bit_test(softc->vlan_tags, cfg.vlan_id))
		return (EINVAL);

	for (int i = 0; i < AQ_HW_VLAN_MAX_FILTERS; i++) {
		if (i == location)
			continue;
		if (softc->rx_filters.vlan_filters[i].enable &&
		    softc->rx_filters.vlan_filters[i].vlan_id == cfg.vlan_id)
			return (EEXIST);
	}

	cfg.queue = (queue < 0) ? 0xFF : (uint8_t)queue;
	softc->rx_filters.vlan_filters[location] = cfg;
	aq_update_vlan_filters(softc);
	return (0);
}

static int
aq_sysctl_wol_phy(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	int val;
	int err;

	val = softc->wol_phy ? 1 : 0;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err || !req->newptr)
		return (err);

	if (val != 0 && val != 1)
		return (EINVAL);

	softc->wol_phy = (val != 0);
	return (0);
}

static int
aq_sysctl_wol_mask(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	if_t ifp = iflib_get_ifp(softc->ctx);
	uint32_t mask;
	uint32_t newmask;
	int err;

	mask = softc->wol_phy ? AQ_WOL_PHY : 0;
#ifdef IFCAP_WOL_MAGIC
	if (if_getcapenable(ifp) & IFCAP_WOL_MAGIC)
		mask |= AQ_WOL_MAGIC;
#elif defined(IFCAP_WOL)
	if (if_getcapenable(ifp) & IFCAP_WOL)
		mask |= AQ_WOL_MAGIC;
#endif
	newmask = mask;

	err = sysctl_handle_int(oidp, (int *)&newmask, 0, req);
	if (err || !req->newptr)
		return (err);

	if (newmask & ~(AQ_WOL_MAGIC | AQ_WOL_PHY))
		return (EINVAL);

#ifdef IFCAP_WOL_MAGIC
	if (newmask & AQ_WOL_MAGIC)
		if_setcapenable(ifp, if_getcapenable(ifp) | IFCAP_WOL_MAGIC);
	else
		if_setcapenable(ifp, if_getcapenable(ifp) & ~IFCAP_WOL_MAGIC);
#elif defined(IFCAP_WOL)
	if (newmask & AQ_WOL_MAGIC)
		if_setcapenable(ifp, if_getcapenable(ifp) | IFCAP_WOL);
	else
		if_setcapenable(ifp, if_getcapenable(ifp) & ~IFCAP_WOL);
#endif

	softc->wol_phy = ((newmask & AQ_WOL_PHY) != 0);
	softc->wol_mask = newmask;
	return (0);
}

static int
aq_sysctl_downshift(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	int val;
	int err;

	val = (int)softc->downshift;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err || !req->newptr)
		return (err);
	if (val < 0 || val > (int)AQ_DOWNSHIFT_MAX)
		return (EINVAL);

	err = aq_hw_set_downshift(&softc->hw, (uint32_t)val);
	if (err != 0)
		return (err);

	softc->downshift = (uint32_t)val;
	return (0);
}

static int
aq_sysctl_media_detect(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	int val;
	int err;

	val = softc->media_detect ? 1 : 0;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err || !req->newptr)
		return (err);
	if (val != 0 && val != 1)
		return (EINVAL);

	if (softc->hw.fw_ops != &aq_fw2x_ops)
		return (ENOTSUP);
	err = fw2x_set_media_detect(&softc->hw, (val != 0));
	if (err != 0)
		return (err);

	softc->media_detect = (val != 0);
	return (0);
}

static int
aq_sysctl_loopback(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	int val;
	int err;

	val = softc->loopback_mode;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err || !req->newptr)
		return (err);
	if (val < 0 || val > 2)
		return (EINVAL);

	if (softc->hw.fw_ops != &aq_fw2x_ops)
		return (ENOTSUP);
	err = fw2x_set_loopback(&softc->hw, val);
	if (err != 0)
		return (err);

	softc->loopback_mode = val;
	return (0);
}

static int
aq_sysctl_apply_itr(struct aq_dev *softc, int itr_mode, uint16_t itr_tx,
    uint16_t itr_rx)
{
	struct aq_hw *hw = &softc->hw;
	if_t ifp = iflib_get_ifp(softc->ctx);
	int saved_mode = hw->itr_mode;
	uint16_t saved_tx = hw->itr_tx;
	uint16_t saved_rx = hw->itr_rx;
	int err;

	hw->itr_mode = itr_mode;
	hw->itr_tx = itr_tx;
	hw->itr_rx = itr_rx;

	if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0)
		return (0);

	err = aq_hw_interrupt_moderation_set(hw);
	if (err == 0)
		return (0);

	hw->itr_mode = saved_mode;
	hw->itr_tx = saved_tx;
	hw->itr_rx = saved_rx;
	(void)aq_hw_interrupt_moderation_set(hw);

	return (err);
}

static int
aq_sysctl_itr_mode(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	int val;
	int err;

	val = softc->hw.itr_mode;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err || !req->newptr)
		return (err);
	if (val != AQ_ITR_MODE_OFF && val != AQ_ITR_MODE_ON &&
	    val != AQ_ITR_MODE_AUTO)
		return (EINVAL);

	return (aq_sysctl_apply_itr(softc, val, softc->hw.itr_tx,
	    softc->hw.itr_rx));
}

static int
aq_sysctl_itr(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	struct aq_hw *hw = &softc->hw;
	uint16_t itr_tx = hw->itr_tx;
	uint16_t itr_rx = hw->itr_rx;
	int val;
	int err;

	val = ((int)arg2 == AQ_ITR_SYSCTL_RX) ? hw->itr_rx : hw->itr_tx;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err || !req->newptr)
		return (err);
	if (val < 0 || (unsigned int)val > AQ_ITR_USEC_MAX)
		return (EINVAL);

	if (AQ_HW_IS_AQ1_A0(hw)) {
		itr_tx = (uint16_t)val;
		itr_rx = (uint16_t)val;
	} else if ((int)arg2 == AQ_ITR_SYSCTL_RX) {
		itr_rx = (uint16_t)val;
	} else {
		itr_tx = (uint16_t)val;
	}

	return (aq_sysctl_apply_itr(softc, hw->itr_mode, itr_tx, itr_rx));
}

static int
aq_sysctl_l3l4_filter(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	int location = (int)arg2;
	struct aq_rx_filter_l3l4 cfg;
	char buf[256];
	uint32_t cmd = 0;
	int err;
	int action = HW_ATL_RX_HOST;
	int queue = -1;
	bool enable = false;
	bool ipv6 = false;
	bool ipv6_set = false;
	bool cmp_proto = false;
	int proto = 0;
	bool proto_set = false;
	bool src_set = false;
	bool dst_set = false;
	bool src6_set = false;
	bool dst6_set = false;

	if (location < 0 || location >= AQ_HW_L3L4_MAX_FILTERS)
		return (EINVAL);

	cfg = softc->rx_filters.l3l4_filters[location];
	if (cfg.cmd & HW_ATL_RX_ENABLE_QUEUE_L3L4)
		queue = (cfg.cmd >> HW_ATL_RX_BOFFSET_QUEUE_FL3L4) & 0xFF;
	action = (cfg.cmd >> HW_ATL_RX_BOFFSET_ACTION_FL3F4) & 0x7U;

	snprintf(buf, sizeof(buf),
	    "enable=%u,ipv6=%u,proto=%s,src=0x%08x,dst=0x%08x,"
	    "src6=%08x%08x%08x%08x,dst6=%08x%08x%08x%08x,"
	    "sport=%u,dport=%u,action=%s,queue=%d",
	    (cfg.cmd & HW_ATL_RX_ENABLE_FLTR_L3L4) ? 1U : 0U,
	    cfg.is_ipv6 ? 1U : 0U, aq_proto_name(cfg.cmd), cfg.ip_src[0],
	    cfg.ip_dst[0], cfg.ip_src[0], cfg.ip_src[1], cfg.ip_src[2],
	    cfg.ip_src[3], cfg.ip_dst[0], cfg.ip_dst[1], cfg.ip_dst[2],
	    cfg.ip_dst[3], cfg.p_src, cfg.p_dst, aq_action_name(cfg.cmd),
	    queue);

	err = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (err || !req->newptr)
		return (err);
	if (buf[0] == '\0')
		return (EINVAL);

	cfg.location = (uint8_t)location;
	enable = (cfg.cmd & HW_ATL_RX_ENABLE_FLTR_L3L4) != 0;
	ipv6 = cfg.is_ipv6;
	if (cfg.cmd & HW_ATL_RX_ENABLE_CMP_PROT_L4) {
		cmp_proto = true;
		proto = cfg.cmd & 0x7U;
	}

	{
		char *p = buf;
		char *tok;

		while ((tok = strsep(&p, ",")) != NULL) {
			char *eq;

			while (*tok == ' ' || *tok == '\t')
				tok++;
			if (*tok == '\0')
				continue;
			eq = strchr(tok, '=');
			if (eq == NULL)
				return (EINVAL);
			*eq = '\0';
			eq++;
			if (strcmp(tok, "enable") == 0) {
				bool v;
				err = aq_parse_bool(eq, &v);
				if (err != 0)
					return (err);
				enable = v;
			} else if (strcmp(tok, "ipv6") == 0) {
				bool v;
				err = aq_parse_bool(eq, &v);
				if (err != 0)
					return (err);
				ipv6 = v;
				ipv6_set = true;
			} else if (strcmp(tok, "proto") == 0) {
				err = aq_parse_proto(eq, &proto, &cmp_proto);
				if (err != 0)
					return (err);
				proto_set = true;
			} else if (strcmp(tok, "src") == 0) {
				uint32_t v;
				err = aq_parse_u32(eq, &v);
				if (err != 0)
					return (err);
				cfg.ip_src[0] = v;
				src_set = true;
			} else if (strcmp(tok, "dst") == 0) {
				uint32_t v;
				err = aq_parse_u32(eq, &v);
				if (err != 0)
					return (err);
				cfg.ip_dst[0] = v;
				dst_set = true;
			} else if (strcmp(tok, "src6") == 0) {
				err = aq_parse_ipv6_hex(eq, cfg.ip_src);
				if (err != 0)
					return (err);
				src6_set = true;
			} else if (strcmp(tok, "dst6") == 0) {
				err = aq_parse_ipv6_hex(eq, cfg.ip_dst);
				if (err != 0)
					return (err);
				dst6_set = true;
			} else if (strcmp(tok, "sport") == 0) {
				uint32_t v;
				err = aq_parse_u32(eq, &v);
				if (err != 0 || v > 0xFFFFU)
					return (EINVAL);
				cfg.p_src = (uint16_t)v;
			} else if (strcmp(tok, "dport") == 0) {
				uint32_t v;
				err = aq_parse_u32(eq, &v);
				if (err != 0 || v > 0xFFFFU)
					return (EINVAL);
				cfg.p_dst = (uint16_t)v;
			} else if (strcmp(tok, "action") == 0) {
				err = aq_parse_action(eq, &action);
				if (err != 0)
					return (err);
			} else if (strcmp(tok, "queue") == 0) {
				err = aq_parse_s32(eq, &queue);
				if (err != 0)
					return (err);
			} else {
				return (EINVAL);
			}
		}
	}

	if (queue < -1)
		return (EINVAL);
	if (queue >= 0 && (uint32_t)queue >= softc->rx_rings_count)
		return (EINVAL);
	if (ipv6 &&
	    ((location & 3) != 0 || location + 3 >= AQ_HW_L3L4_MAX_FILTERS))
		return (EINVAL);

	cfg.is_ipv6 = ipv6;
	if (ipv6_set) {
		if (ipv6) {
			if (!src6_set)
				memset(cfg.ip_src, 0, sizeof(cfg.ip_src));
			if (!dst6_set)
				memset(cfg.ip_dst, 0, sizeof(cfg.ip_dst));
		} else {
			if (!src_set)
				cfg.ip_src[0] = 0;
			if (!dst_set)
				cfg.ip_dst[0] = 0;
		}
	}
	if (!enable) {
		err = aq_hw_filter_l3l4_clear(&softc->hw, &cfg);
		if (err != 0)
			return (err);
		memset(&cfg, 0, sizeof(cfg));
		cfg.location = (uint8_t)location;
		cfg.p_src = 0;
		cfg.p_dst = 0;
		softc->rx_filters.l3l4_filters[location] = cfg;
		return (0);
	}

	cmd |= HW_ATL_RX_ENABLE_FLTR_L3L4;
	if (ipv6)
		cmd |= HW_ATL_RX_ENABLE_L3_IPv6;
	if (cmp_proto || proto_set) {
		if (cmp_proto) {
			cmd |= HW_ATL_RX_ENABLE_CMP_PROT_L4;
			cmd |= (uint32_t)proto;
		}
	}
	if (!ipv6) {
		if (cfg.ip_src[0])
			cmd |= HW_ATL_RX_ENABLE_CMP_SRC_ADDR_L3;
		if (cfg.ip_dst[0])
			cmd |= HW_ATL_RX_ENABLE_CMP_DEST_ADDR_L3;
	} else {
		if (cfg.ip_src[0] || cfg.ip_src[1] || cfg.ip_src[2] ||
		    cfg.ip_src[3])
			cmd |= HW_ATL_RX_ENABLE_CMP_SRC_ADDR_L3;
		if (cfg.ip_dst[0] || cfg.ip_dst[1] || cfg.ip_dst[2] ||
		    cfg.ip_dst[3])
			cmd |= HW_ATL_RX_ENABLE_CMP_DEST_ADDR_L3;
	}
	if (cfg.p_dst)
		cmd |= HW_ATL_RX_ENABLE_CMP_DEST_PORT_L4;
	if (cfg.p_src)
		cmd |= HW_ATL_RX_ENABLE_CMP_SRC_PORT_L4;

	if (action == HW_ATL_RX_DISCARD) {
		cmd |= ((uint32_t)HW_ATL_RX_DISCARD
		    << HW_ATL_RX_BOFFSET_ACTION_FL3F4);
	} else if (queue >= 0) {
		cmd |= ((uint32_t)HW_ATL_RX_HOST
		    << HW_ATL_RX_BOFFSET_ACTION_FL3F4);
		cmd |= ((uint32_t)queue << HW_ATL_RX_BOFFSET_QUEUE_FL3L4);
		cmd |= HW_ATL_RX_ENABLE_QUEUE_L3L4;
	}

	cfg.cmd = cmd;
	err = aq_hw_filter_l3l4_set(&softc->hw, &cfg);
	if (err != 0)
		return (err);

	softc->rx_filters.l3l4_filters[location] = cfg;
	return (0);
}

static int
aq_sysctl_eee_rate(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	uint32_t rate = 0;
	uint32_t supported = 0;
	uint32_t lp = 0;
	int val;
	int err;

	err = aq_hw_get_eee_rate(&softc->hw, &rate, &supported, &lp);
	if (err != 0)
		return (err);

	val = (int)rate;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err || !req->newptr)
		return (err);

	if ((uint32_t)val & ~supported)
		return (EINVAL);

	err = aq_hw_set_eee_rate(&softc->hw, (uint32_t)val);
	if (err != 0)
		return (err);

	softc->hw.eee_rate = (uint32_t)val;
	return (0);
}

static int
aq_sysctl_eee_supported(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	uint32_t rate = 0;
	uint32_t supported = 0;
	uint32_t lp = 0;
	int val;
	int err;

	err = aq_hw_get_eee_rate(&softc->hw, &rate, &supported, &lp);
	if (err != 0)
		return (err);

	val = (int)supported;
	return (sysctl_handle_int(oidp, &val, 0, req));
}

static int
aq_sysctl_eee_lp_rate(SYSCTL_HANDLER_ARGS)
{
	struct aq_dev *softc = (struct aq_dev *)arg1;
	uint32_t rate = 0;
	uint32_t supported = 0;
	uint32_t lp = 0;
	int val;
	int err;

	err = aq_hw_get_eee_rate(&softc->hw, &rate, &supported, &lp);
	if (err != 0)
		return (err);

	val = (int)lp;
	return (sysctl_handle_int(oidp, &val, 0, req));
}

static int
aq_sysctl_print_tx_head(SYSCTL_HANDLER_ARGS)
{
	struct aq_ring *ring = arg1;
	int error = 0;
	unsigned int val;

	if (!ring)
		return (0);

	val = tdm_tx_desc_head_ptr_get(&ring->dev->hw, ring->index);

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return (error);

	return (0);
}

static int
aq_sysctl_print_tx_tail(SYSCTL_HANDLER_ARGS)
{
	struct aq_ring *ring = arg1;
	int error = 0;
	unsigned int val;

	if (!ring)
		return (0);

	val = reg_tx_dma_desc_tail_ptr_get(&ring->dev->hw, ring->index);

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return (error);

	return (0);
}

static int
aq_sysctl_print_rx_head(SYSCTL_HANDLER_ARGS)
{
	struct aq_ring *ring = arg1;
	int error = 0;
	unsigned int val;

	if (!ring)
		return (0);

	val = rdm_rx_desc_head_ptr_get(&ring->dev->hw, ring->index);

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return (error);

	return (0);
}

static int
aq_sysctl_print_rx_tail(SYSCTL_HANDLER_ARGS)
{
	struct aq_ring *ring = arg1;
	int error = 0;
	unsigned int val;

	if (!ring)
		return (0);

	val = reg_rx_dma_desc_tail_ptr_get(&ring->dev->hw, ring->index);

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return (error);

	return (0);
}

static void
aq_add_stats_sysctls(struct aq_dev *softc)
{
	device_t dev = softc->dev;
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);
	struct aq_stats_s *stats = &softc->accum_stats;
	struct sysctl_oid *stat_node, *queue_node;
	struct sysctl_oid_list *stat_list, *queue_list;

#define QUEUE_NAME_LEN 32
	char namebuf[QUEUE_NAME_LEN];

	aq_hostboot_add_sysctls(softc, ctx, child);

	/* RSS configuration */
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "print_rss_config",
	    CTLTYPE_STRING | CTLFLAG_RD, softc, 0, aq_sysctl_print_rss_config,
	    "A", "Prints RSS Configuration");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "phy_temperature",
	    CTLTYPE_INT | CTLFLAG_RD, softc, 0, aq_sysctl_phy_temp, "I",
	    "PHY temperature (C)");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "cable_length",
	    CTLTYPE_INT | CTLFLAG_RD, softc, 0, aq_sysctl_cable_len, "I",
	    "Cable length");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "cable_diag",
	    CTLTYPE_STRING | CTLFLAG_RD, softc, 0, aq_sysctl_cable_diag, "A",
	    "Cable diagnostics");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "fw_ver",
	    CTLTYPE_STRING | CTLFLAG_RD, softc, 0, aq_sysctl_fw_ver, "A",
	    "Live firmware version");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "fw_iface_ver",
	    CTLTYPE_STRING | CTLFLAG_RD, softc, 0, aq_sysctl_fw_iface_ver, "A",
	    "Firmware interface version (AQ2-only)");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "eee_rate",
	    CTLTYPE_INT | CTLFLAG_RW, softc, 0, aq_sysctl_eee_rate, "I",
	    "EEE rate mask");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "eee_supported",
	    CTLTYPE_INT | CTLFLAG_RD, softc, 0, aq_sysctl_eee_supported, "I",
	    "EEE supported mask");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "eee_lp_rate",
	    CTLTYPE_INT | CTLFLAG_RD, softc, 0, aq_sysctl_eee_lp_rate, "I",
	    "EEE link partner mask");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "wol_phy",
	    CTLTYPE_INT | CTLFLAG_RW, softc, 0, aq_sysctl_wol_phy, "I",
	    "Wake on link state change");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "wol_mask",
	    CTLTYPE_INT | CTLFLAG_RW, softc, 0, aq_sysctl_wol_mask, "I",
	    "WOL mask (magic|phy)");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "downshift",
	    CTLTYPE_INT | CTLFLAG_RW, softc, 0, aq_sysctl_downshift, "I",
	    "Downshift retry count");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "media_detect",
	    CTLTYPE_INT | CTLFLAG_RW, softc, 0, aq_sysctl_media_detect, "I",
	    "Enable media detect");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "loopback",
	    CTLTYPE_INT | CTLFLAG_RW, softc, 0, aq_sysctl_loopback, "I",
	    "Loopback mode (0=off,1=int,2=ext)");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "itr_mode",
	    CTLTYPE_INT | CTLFLAG_RW, softc, 0, aq_sysctl_itr_mode, "I",
	    "Interrupt moderation mode (0=off,1=on,-1=auto)");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "itr_tx",
	    CTLTYPE_INT | CTLFLAG_RW, softc, AQ_ITR_SYSCTL_TX, aq_sysctl_itr,
	    "I", "Manual TX interrupt moderation delay in microseconds");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "itr_rx",
	    CTLTYPE_INT | CTLFLAG_RW, softc, AQ_ITR_SYSCTL_RX, aq_sysctl_itr,
	    "I", "Manual RX interrupt moderation delay in microseconds");

	{
		struct sysctl_oid *filter_node;
		struct sysctl_oid *l2_node;
		struct sysctl_oid *vlan_node;
		struct sysctl_oid *l3_node;
		struct sysctl_oid_list *filter_list;
		struct sysctl_oid_list *l2_list;
		struct sysctl_oid_list *vlan_list;
		struct sysctl_oid_list *l3_list;

		filter_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "rx_filter",
		    CTLFLAG_RD, NULL, "RX filter configuration");
		filter_list = SYSCTL_CHILDREN(filter_node);

		l2_node = SYSCTL_ADD_NODE(ctx, filter_list, OID_AUTO, "l2",
		    CTLFLAG_RD, NULL, "L2 ethertype filters");
		l2_list = SYSCTL_CHILDREN(l2_node);
		for (int i = 0; i < AQ_HW_ETYPE_MAX_FILTERS; i++) {
			snprintf(namebuf, QUEUE_NAME_LEN, "f%d", i);
			SYSCTL_ADD_PROC(ctx, l2_list, OID_AUTO, namebuf,
			    CTLTYPE_STRING | CTLFLAG_RW, softc, i,
			    aq_sysctl_l2_filter, "A",
			    "L2 filter: enable,ethertype,queue,prio_en,prio");
		}

		vlan_node = SYSCTL_ADD_NODE(ctx, filter_list, OID_AUTO, "vlan",
		    CTLFLAG_RD, NULL, "VLAN filters");
		vlan_list = SYSCTL_CHILDREN(vlan_node);
		for (int i = 0; i < AQ_HW_VLAN_MAX_FILTERS; i++) {
			snprintf(namebuf, QUEUE_NAME_LEN, "f%d", i);
			SYSCTL_ADD_PROC(ctx, vlan_list, OID_AUTO, namebuf,
			    CTLTYPE_STRING | CTLFLAG_RW, softc, i,
			    aq_sysctl_vlan_filter, "A",
			    "VLAN filter: enable,vlan,queue");
		}

		l3_node = SYSCTL_ADD_NODE(ctx, filter_list, OID_AUTO, "l3l4",
		    CTLFLAG_RD, NULL, "L3/L4 filters");
		l3_list = SYSCTL_CHILDREN(l3_node);
		for (int i = 0; i < AQ_HW_L3L4_MAX_FILTERS; i++) {
			snprintf(namebuf, QUEUE_NAME_LEN, "f%d", i);
			SYSCTL_ADD_PROC(ctx, l3_list, OID_AUTO, namebuf,
			    CTLTYPE_STRING | CTLFLAG_RW, softc, i,
			    aq_sysctl_l3l4_filter, "A",
			    "L3/L4 filter: enable,ipv6,proto,src,dst,src6,dst6,sport,dport,action,queue");
		}
	}

	/* Driver Statistics */
	for (int i = 0; i < softc->tx_rings_count; i++) {
		struct aq_ring *ring = softc->tx_rings[i];
		snprintf(namebuf, QUEUE_NAME_LEN, "tx_queue%d", i);
		queue_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
		    CTLFLAG_RD, NULL, "Queue Name");
		queue_list = SYSCTL_CHILDREN(queue_node);

		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tx_pkts",
		    CTLFLAG_RD, &(ring->stats.tx_pkts), "TX Packets");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tx_bytes",
		    CTLFLAG_RD, &(ring->stats.tx_bytes), "TX Octets");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tx_drops",
		    CTLFLAG_RD, &(ring->stats.tx_drops), "TX Drops");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tx_queue_full",
		    CTLFLAG_RD, &(ring->stats.tx_queue_full), "TX Queue Full");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "tx_head",
		    CTLTYPE_UINT | CTLFLAG_RD, ring, 0, aq_sysctl_print_tx_head,
		    "IU", "ring head pointer");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "tx_tail",
		    CTLTYPE_UINT | CTLFLAG_RD, ring, 0, aq_sysctl_print_tx_tail,
		    "IU", "ring tail pointer");
	}

	for (int i = 0; i < softc->rx_rings_count; i++) {
		struct aq_ring *ring = softc->rx_rings[i];
		snprintf(namebuf, QUEUE_NAME_LEN, "rx_queue%d", i);
		queue_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
		    CTLFLAG_RD, NULL, "Queue Name");
		queue_list = SYSCTL_CHILDREN(queue_node);

		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_pkts",
		    CTLFLAG_RD, &(ring->stats.rx_pkts), "RX Packets");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_bytes",
		    CTLFLAG_RD, &(ring->stats.rx_bytes), "TX Octets");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "jumbo_pkts",
		    CTLFLAG_RD, &(ring->stats.jumbo_pkts), "Jumbo Packets");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_err",
		    CTLFLAG_RD, &(ring->stats.rx_err), "RX Errors");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "irq", CTLFLAG_RD,
		    &(ring->stats.irq), "RX interrupts");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "rx_head",
		    CTLTYPE_UINT | CTLFLAG_RD, ring, 0, aq_sysctl_print_rx_head,
		    "IU", "ring head pointer");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "rx_tail",
		    CTLTYPE_UINT | CTLFLAG_RD, ring, 0, aq_sysctl_print_rx_tail,
		    "IU", " ring tail pointer");
	}

	stat_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "mac", CTLFLAG_RD,
	    NULL, "Statistics (read from HW registers)");
	stat_list = SYSCTL_CHILDREN(stat_node);

	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_pkts_rcvd", CTLFLAG_RD,
	    &stats->good_pkts_rcvd, "Good Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "ucast_pkts_rcvd",
	    CTLFLAG_RD, &stats->ucast_pkts_rcvd, "Unicast Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mcast_pkts_rcvd",
	    CTLFLAG_RD, &stats->mcast_pkts_rcvd, "Multicast Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "bcast_pkts_rcvd",
	    CTLFLAG_RD, &stats->bcast_pkts_rcvd, "Broadcast Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "pause_frames_rcvd",
	    CTLFLAG_RD, &stats->pause_frames_rcvd, "Pause Frames Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rsc_pkts_rcvd", CTLFLAG_RD,
	    &stats->rsc_pkts_rcvd, "Coalesced Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "err_pkts_rcvd", CTLFLAG_RD,
	    &stats->err_pkts_rcvd, "Errors of Packet Receive");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "drop_pkts_dma", CTLFLAG_RD,
	    &stats->drop_pkts_dma, "Dropped Packets in DMA");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_octets_rcvd",
	    CTLFLAG_RD, &stats->good_octets_rcvd, "Good Octets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "ucast_octets_rcvd",
	    CTLFLAG_RD, &stats->ucast_octets_rcvd, "Unicast Octets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mcast_octets_rcvd",
	    CTLFLAG_RD, &stats->mcast_octets_rcvd, "Multicast Octets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "bcast_octets_rcvd",
	    CTLFLAG_RD, &stats->bcast_octets_rcvd, "Broadcast Octets Received");

	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_pkts_txd", CTLFLAG_RD,
	    &stats->good_pkts_txd, "Good Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "ucast_pkts_txd", CTLFLAG_RD,
	    &stats->ucast_pkts_txd, "Unicast Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mcast_pkts_txd", CTLFLAG_RD,
	    &stats->mcast_pkts_txd, "Multicast Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "bcast_pkts_txd", CTLFLAG_RD,
	    &stats->bcast_pkts_txd, "Broadcast Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "pause_frames_txd",
	    CTLFLAG_RD, &stats->pause_frames_txd, "Pause Frames Transmitted");

	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "err_pkts_txd", CTLFLAG_RD,
	    &stats->err_pkts_txd, "Errors of Packet Transmit");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_octets_txd",
	    CTLFLAG_RD, &stats->good_octets_txd, "Good Octets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "ucast_octets_txd",
	    CTLFLAG_RD, &stats->ucast_octets_txd, "Unicast Octets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mcast_octets_txd",
	    CTLFLAG_RD, &stats->mcast_octets_txd,
	    "Multicast Octets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "bcast_octets_txd",
	    CTLFLAG_RD, &stats->bcast_octets_txd,
	    "Broadcast Octets Transmitted");
}
