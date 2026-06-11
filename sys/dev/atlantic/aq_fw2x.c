/**
 * aQuantia Corporation Network Driver
 * Copyright (C) 2014-2017 aQuantia Corporation. All rights reserved
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
 *   (3) The name of the author may not be used to endorse or promote
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
 *
 * @file aq_fw2x.c
 * Firmware v2.x specific functions.
 * @date 2017.12.11  @author roman.agafonov@aquantia.com
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/bitstring.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/socket.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/iflib.h>

#include "aq_common.h"
#include "aq_dbg.h"
#include "aq_device.h"
#include "aq_fw.h"
#include "aq_hw.h"
#include "aq_hw_llh.h"
#include "aq_hw_llh_internal.h"

typedef enum {
	CAPS_LO_10BASET_HD = 0x00,
	CAPS_LO_10BASET_FD,
	CAPS_LO_100BASETX_HD,
	CAPS_LO_100BASET4_HD,
	CAPS_LO_100BASET2_HD,
	CAPS_LO_100BASETX_FD,
	CAPS_LO_100BASET2_FD,
	CAPS_LO_1000BASET_HD,
	CAPS_LO_1000BASET_FD,
	CAPS_LO_2P5GBASET_FD,
	CAPS_LO_5GBASET_FD,
	CAPS_LO_10GBASET_FD,
} fw2x_caps_lo;

typedef enum {
	CAPS_HI_RESERVED1 = 0x00,
	CAPS_HI_10BASET_EEE,
	CAPS_HI_RESERVED2,
	CAPS_HI_PAUSE,
	CAPS_HI_ASYMMETRIC_PAUSE,
	CAPS_HI_100BASETX_EEE,
	CAPS_HI_RESERVED3,
	CAPS_HI_RESERVED4,
	CAPS_HI_1000BASET_FD_EEE,
	CAPS_HI_2P5GBASET_FD_EEE,
	CAPS_HI_5GBASET_FD_EEE,
	CAPS_HI_10GBASET_FD_EEE,
	CAPS_HI_RESERVED5,
	CAPS_HI_RESERVED6,
	CAPS_HI_RESERVED7,
	CAPS_HI_RESERVED8,
	CAPS_HI_RESERVED9,
	CAPS_HI_CABLE_DIAG,
	CAPS_HI_TEMPERATURE,
	CAPS_HI_DOWNSHIFT,
	CAPS_HI_PTP_AVB_EN,
	CAPS_HI_MEDIA_DETECT,
	CAPS_HI_LINK_DROP,
	CAPS_HI_SLEEP_PROXY,
	CAPS_HI_WOL,
	CAPS_HI_MAC_STOP,
	CAPS_HI_EXT_LOOPBACK,
	CAPS_HI_INT_LOOPBACK,
	CAPS_HI_EFUSE_AGENT,
	CAPS_HI_WOL_TIMER,
	CAPS_HI_STATISTICS,
	CAPS_HI_TRANSACTION_ID,
} fw2x_caps_hi;

typedef enum aq_fw2x_rate {
	FW2X_RATE_100M = 0x20,
	FW2X_RATE_1G = 0x100,
	FW2X_RATE_2G5 = 0x200,
	FW2X_RATE_5G = 0x400,
	FW2X_RATE_10G = 0x800,
} aq_fw2x_rate;

typedef struct fw2x_msm_statistics {
	uint32_t uprc;
	uint32_t mprc;
	uint32_t bprc;
	uint32_t erpt;
	uint32_t uptc;
	uint32_t mptc;
	uint32_t bptc;
	uint32_t erpr;
	uint32_t mbtc;
	uint32_t bbtc;
	uint32_t mbrc;
	uint32_t bbrc;
	uint32_t ubrc;
	uint32_t ubtc;
	uint32_t ptc;
	uint32_t prc;
} fw2x_msm_statistics;

typedef struct fw2x_phy_cable_diag_data {
	uint32_t lane_data[4];
} fw2x_phy_cable_diag_data;

typedef struct fw2x_capabilities {
	uint32_t caps_lo;
	uint32_t caps_hi;
} fw2x_capabilities;

typedef struct fw2x_mailbox // struct fwHostInterface
{
	uint32_t version;
	uint32_t transaction_id;
	int32_t error;
	fw2x_msm_statistics msm; // msmStatistics_t msm;
	uint16_t phy_h_bit;
	uint16_t phy_fault_code;
	int16_t phy_temperature;
	uint8_t cable_len;
	uint8_t reserved1;
	fw2x_phy_cable_diag_data diag_data;
	uint32_t reserved[8];

	fw2x_capabilities caps;

	/* ... */
} fw2x_mailbox;

struct __packed fw2x_offload_ip_info {
	uint8_t v4_local_addr_count;
	uint8_t v4_addr_count;
	uint8_t v6_local_addr_count;
	uint8_t v6_addr_count;
	uint32_t v4_addr;
	uint32_t v4_prefix;
	uint32_t v6_addr;
	uint32_t v6_prefix;
};

struct __packed fw2x_offload_port_info {
	uint16_t udp_port_count;
	uint16_t tcp_port_count;
	uint32_t udp_port;
	uint32_t tcp_port;
};

struct __packed fw2x_offload_ka_info {
	uint16_t v4_ka_count;
	uint16_t v6_ka_count;
	uint32_t retry_count;
	uint32_t retry_interval;
	uint32_t v4_ka;
	uint32_t v6_ka;
};

struct __packed fw2x_offload_rr_info {
	uint32_t rr_count;
	uint32_t rr_buf_len;
	uint32_t rr_id_x;
	uint32_t rr_buf;
};

struct __packed fw2x_offload_info {
	uint32_t version;
	uint32_t len;
	uint8_t mac_addr[6];
	uint8_t reserved[2];
	struct fw2x_offload_ip_info ips;
	struct fw2x_offload_port_info ports;
	struct fw2x_offload_ka_info kas;
	struct fw2x_offload_rr_info rrs;
};

struct __packed fw2x_rpc_msg {
	uint32_t msg_id;
	struct fw2x_offload_info offloads;
};

struct fw2x_rpc_tid {
	union {
		uint32_t val;
		struct {
			uint16_t tid;
			uint16_t len;
		};
	};
};

struct __packed fw2x_mbox_header {
	uint32_t version;
	uint32_t transaction_id;
	uint32_t error;
};

struct __packed fw2x_stats {
	uint32_t uprc;
	uint32_t mprc;
	uint32_t bprc;
	uint32_t erpt;
	uint32_t uptc;
	uint32_t mptc;
	uint32_t bptc;
	uint32_t erpr;
	uint32_t mbtc;
	uint32_t bbtc;
	uint32_t mbrc;
	uint32_t bbrc;
	uint32_t ubrc;
	uint32_t ubtc;
	uint32_t dpc;
};

struct __packed fw2x_ptp_offset {
	uint16_t ingress_100;
	uint16_t egress_100;
	uint16_t ingress_1000;
	uint16_t egress_1000;
	uint16_t ingress_2500;
	uint16_t egress_2500;
	uint16_t ingress_5000;
	uint16_t egress_5000;
	uint16_t ingress_10000;
	uint16_t egress_10000;
};

struct __packed fw2x_info {
	uint8_t reserved[6];
	uint16_t phy_fault_code;
	uint16_t phy_temperature;
	uint8_t cable_len;
	uint8_t reserved1;
	uint8_t cable_diag_data[16];
	struct fw2x_ptp_offset ptp_offset;
	uint8_t reserved2[12];
	uint32_t caps_lo;
	uint32_t caps_hi;
	uint32_t reserved_datapath;
	uint32_t reserved3[7];
	uint32_t reserved_simpleresp[3];
	uint32_t reserved_linkstat[7];
	uint32_t reserved_wakes_count;
	uint32_t reserved_eee_stat[12];
	uint32_t tx_stuck_cnt;
	uint32_t setting_address;
	uint32_t setting_length;
	uint32_t caps_ex;
};

struct __packed fw2x_mbox_full {
	struct fw2x_mbox_header header;
	struct fw2x_stats stats;
	struct fw2x_info info;
};

struct __packed fw2x_settings {
	uint32_t mtu;
	uint32_t downshift_retry_count;
	uint32_t link_pause_frame_quanta_100m;
	uint32_t link_pause_frame_threshold_100m;
	uint32_t link_pause_frame_quanta_1g;
	uint32_t link_pause_frame_threshold_1g;
	uint32_t link_pause_frame_quanta_2p5g;
	uint32_t link_pause_frame_threshold_2p5g;
	uint32_t link_pause_frame_quanta_5g;
	uint32_t link_pause_frame_threshold_5g;
	uint32_t link_pause_frame_quanta_10g;
	uint32_t link_pause_frame_threshold_10g;
	uint32_t pfc_quanta_class_0;
	uint32_t pfc_threshold_class_0;
	uint32_t pfc_quanta_class_1;
	uint32_t pfc_threshold_class_1;
	uint32_t pfc_quanta_class_2;
	uint32_t pfc_threshold_class_2;
	uint32_t pfc_quanta_class_3;
	uint32_t pfc_threshold_class_3;
	uint32_t pfc_quanta_class_4;
	uint32_t pfc_threshold_class_4;
	uint32_t pfc_quanta_class_5;
	uint32_t pfc_threshold_class_5;
	uint32_t pfc_quanta_class_6;
	uint32_t pfc_threshold_class_6;
	uint32_t pfc_quanta_class_7;
	uint32_t pfc_threshold_class_7;
	uint32_t eee_link_down_timeout;
	uint32_t eee_link_up_timeout;
	uint32_t eee_max_link_drops;
	uint32_t eee_rates_mask;
	uint32_t wake_timer;
	uint32_t thermal_shutdown_off_temp;
	uint32_t thermal_shutdown_warning_temp;
	uint32_t thermal_shutdown_cold_temp;
	uint32_t msm_options;
	uint32_t dac_cable_serdes_modes;
	uint32_t media_detect;
};

// EEE caps
#define FW2X_FW_CAP_EEE_100M (1ULL << (32 + CAPS_HI_100BASETX_EEE))
#define FW2X_FW_CAP_EEE_1G (1ULL << (32 + CAPS_HI_1000BASET_FD_EEE))
#define FW2X_FW_CAP_EEE_2G5 (1ULL << (32 + CAPS_HI_2P5GBASET_FD_EEE))
#define FW2X_FW_CAP_EEE_5G (1ULL << (32 + CAPS_HI_5GBASET_FD_EEE))
#define FW2X_FW_CAP_EEE_10G (1ULL << (32 + CAPS_HI_10GBASET_FD_EEE))

// Flow Control
#define FW2X_FW_CAP_PAUSE (1ULL << (32 + CAPS_HI_PAUSE))
#define FW2X_FW_CAP_ASYM_PAUSE (1ULL << (32 + CAPS_HI_ASYMMETRIC_PAUSE))

// Link Drop
#define FW2X_CAP_LINK_DROP (1ull << (32 + CAPS_HI_LINK_DROP))
#define FW2X_CAP_SLEEP_PROXY (1ull << (32 + CAPS_HI_SLEEP_PROXY))
#define FW2X_CAP_WOL (1ull << (32 + CAPS_HI_WOL))
#define FW2X_CAP_DOWNSHIFT (1ull << (32 + CAPS_HI_DOWNSHIFT))
#define FW2X_CAP_MEDIA_DETECT (1ull << (32 + CAPS_HI_MEDIA_DETECT))

// MSM Statistics
#define FW2X_CAP_STATISTICS (1ull << (32 + CAPS_HI_STATISTICS))

#define FW2X_RATE_MASK                                                  \
	(FW2X_RATE_100M | FW2X_RATE_1G | FW2X_RATE_2G5 | FW2X_RATE_5G | \
	    FW2X_RATE_10G)
#define FW2X_EEE_MASK                                                      \
	(FW2X_FW_CAP_EEE_100M | FW2X_FW_CAP_EEE_1G | FW2X_FW_CAP_EEE_2G5 | \
	    FW2X_FW_CAP_EEE_5G | FW2X_FW_CAP_EEE_10G)

#define FW2X_CTRL_WAKE_ON_LINK BIT(16)
#define FW2X_CTRL_LINK_DROP BIT(22)
#define FW2X_CTRL_SLEEP_PROXY BIT(23)
#define FW2X_CTRL_WOL BIT(24)
#define FW2X_CTRL_DOWNSHIFT BIT(19)
#define FW2X_CTRL_EXT_LOOPBACK BIT(26)
#define FW2X_CTRL_INT_LOOPBACK BIT(27)

#define FW2X_MPI_LED_ADDR 0x31c
#define FW2X_MPI_RPC_ADDR 0x334
#define FW2X_RPC_CONTROL_ADDR 0x338
#define FW2X_RPC_STATE_ADDR 0x33c
#define FW2X_MPI_CONTROL_ADDR 0x368
#define FW2X_MPI_STATE_ADDR 0x370
#define FW2X_MPI_CONTROL2_ADDR 0x36c
#define FW2X_MPI_STATE2_ADDR 0x374

#define FW2X_FW_MIN_VER_LED 0x03010026U

#define FW2X_LED_BLINK 0x2U
#define FW2X_LED_DEFAULT 0x0U

// Firmware v2-3.x specific functions.
//@{
int fw2x_reset(struct aq_hw *hw);

int fw2x_set_mode(struct aq_hw *hw, enum aq_hw_fw_mpi_state_e mode,
    aq_fw_link_speed_t speed);
int fw2x_get_mode(struct aq_hw *hw, enum aq_hw_fw_mpi_state_e *mode,
    aq_fw_link_speed_t *speed, aq_fw_link_fc_t *fc);

int fw2x_get_mac_addr(struct aq_hw *hw, uint8_t *mac);
int fw2x_get_stats(struct aq_hw *hw, struct aq_stats_s *stats);
int fw2x_get_phy_temp(struct aq_hw *hw, int *temp_c);
int fw2x_get_cable_len(struct aq_hw *hw, uint8_t *len);
int fw2x_get_cable_diag(struct aq_hw *hw, uint32_t lane_data[4]);
int fw2x_set_eee_rate(struct aq_hw *hw, uint32_t rate);
int fw2x_get_eee_rate(struct aq_hw *hw, uint32_t *rate, uint32_t *supported,
    uint32_t *lp_rate);

static uint64_t
read64_(struct aq_hw *hw, uint32_t addr)
{
	uint64_t lo = AQ_READ_REG(hw, addr);
	uint64_t hi = AQ_READ_REG(hw, addr + 4);
	return (lo | (hi << 32));
}

static uint64_t
get_mpi_ctrl_(struct aq_hw *hw)
{
	return (read64_(hw, FW2X_MPI_CONTROL_ADDR));
}

static uint64_t
get_mpi_state_(struct aq_hw *hw)
{
	return (read64_(hw, FW2X_MPI_STATE_ADDR));
}

static void
set_mpi_ctrl_(struct aq_hw *hw, uint64_t value)
{
	AQ_WRITE_REG(hw, FW2X_MPI_CONTROL_ADDR, (uint32_t)value);
	AQ_WRITE_REG(hw, FW2X_MPI_CONTROL_ADDR + 4, (uint32_t)(value >> 32));
}

static uint32_t
fw2x_caps_to_eee_mask_(uint64_t caps)
{
	uint32_t rate = 0;

	if (caps & FW2X_FW_CAP_EEE_10G)
		rate |= AQ_EEE_10G;
	if (caps & FW2X_FW_CAP_EEE_5G)
		rate |= AQ_EEE_5G;
	if (caps & FW2X_FW_CAP_EEE_2G5)
		rate |= AQ_EEE_2G5;
	if (caps & FW2X_FW_CAP_EEE_1G)
		rate |= AQ_EEE_1G;
	if (caps & FW2X_FW_CAP_EEE_100M)
		rate |= AQ_EEE_100M;

	return (rate);
}

static uint64_t
fw2x_eee_mask_to_caps_(uint32_t rate)
{
	uint64_t caps = 0;

	if (rate & AQ_EEE_10G)
		caps |= FW2X_FW_CAP_EEE_10G;
	if (rate & AQ_EEE_5G)
		caps |= FW2X_FW_CAP_EEE_5G;
	if (rate & AQ_EEE_2G5)
		caps |= FW2X_FW_CAP_EEE_2G5;
	if (rate & AQ_EEE_1G)
		caps |= FW2X_FW_CAP_EEE_1G;
	if (rate & AQ_EEE_100M)
		caps |= FW2X_FW_CAP_EEE_100M;

	return (caps);
}

int
fw2x_read_settings_addr(struct aq_hw *hw)
{
	uint32_t addr = 0;
	uint32_t len = 0;
	uint32_t offset;
	int err;

	if (hw->mbox_addr == 0)
		return (ENOTSUP);
	offset = offsetof(struct fw2x_mbox_full, info.setting_address);
	err = aq_hw_fw_downld_dwords(hw, hw->mbox_addr + offset, &addr, 1);
	if (err != 0)
		return (err);
	offset = offsetof(struct fw2x_mbox_full, info.setting_length);
	err = aq_hw_fw_downld_dwords(hw, hw->mbox_addr + offset, &len, 1);
	if (err != 0)
		return (err);

	if (addr == 0 || addr == 0xffffffffU || (addr & 0x3U) != 0 ||
	    len < sizeof(struct fw2x_settings)) {
		hw->settings_addr = 0;
		return (ENOTSUP);
	}

	hw->settings_addr = addr;
	return (0);
}

static int
fw2x_write_settings_dwords(struct aq_hw *hw, uint32_t offset, const uint32_t *p,
    uint32_t cnt)
{
	if (hw->settings_addr == 0)
		return (ENOTSUP);
	return (aq_hw_fw_upload_dwords(hw, hw->settings_addr + offset, p, cnt));
}

static int
fw2x_rpc_call(struct aq_hw *hw, const void *buf, uint32_t len)
{
	struct fw2x_rpc_tid sw;
	uint32_t dword_cnt;
	int err = 0;

	if (len > AQ_FW_RPC_MAX)
		return (EINVAL);

	if (buf && len != 0) {
		memcpy(hw->rpc_buf, buf, len);
		hw->rpc_len = (uint16_t)len;
	}

	if (hw->rpc_addr == 0)
		return (ENOTSUP);

	dword_cnt = (len + sizeof(uint32_t) - 1U) / sizeof(uint32_t);
	if (dword_cnt != 0) {
		err = aq_hw_fw_upload_dwords(hw, hw->rpc_addr,
		    (const uint32_t *)(const void *)hw->rpc_buf, dword_cnt);
		if (err != 0)
			return (err);
	}

	hw->rpc_tid++;
	sw.tid = hw->rpc_tid;
	sw.len = (uint16_t)len;
	AQ_WRITE_REG(hw, FW2X_RPC_CONTROL_ADDR, sw.val);
	return (0);
}

static int
fw2x_set_downshift(struct aq_hw *hw, uint32_t counter)
{
	uint32_t mpi_opts;
	uint32_t offset;
	int err;

	if (counter > AQ_DOWNSHIFT_MAX)
		return (EINVAL);
	if ((hw->fw_caps & FW2X_CAP_DOWNSHIFT) == 0)
		return (ENOTSUP);
	offset = offsetof(struct fw2x_settings, downshift_retry_count);
	err = fw2x_write_settings_dwords(hw, offset, &counter, 1);
	if (err != 0)
		return (err);

	mpi_opts = AQ_READ_REG(hw, FW2X_MPI_CONTROL2_ADDR);
	if (counter)
		mpi_opts |= FW2X_CTRL_DOWNSHIFT;
	else
		mpi_opts &= ~FW2X_CTRL_DOWNSHIFT;
	AQ_WRITE_REG(hw, FW2X_MPI_CONTROL2_ADDR, mpi_opts);
	return (0);
}

int
fw2x_set_media_detect(struct aq_hw *hw, bool enable)
{
	uint32_t val = enable ? 1U : 0U;
	uint32_t offset;

	if ((hw->fw_caps & FW2X_CAP_MEDIA_DETECT) == 0)
		return (ENOTSUP);
	offset = offsetof(struct fw2x_settings, media_detect);
	return (fw2x_write_settings_dwords(hw, offset, &val, 1));
}

int
fw2x_set_loopback(struct aq_hw *hw, int mode)
{
	uint32_t mpi_opts;

	mpi_opts = AQ_READ_REG(hw, FW2X_MPI_CONTROL2_ADDR);
	switch (mode) {
	case 0:
		mpi_opts &= ~(FW2X_CTRL_INT_LOOPBACK | FW2X_CTRL_EXT_LOOPBACK);
		break;
	case 1:
		mpi_opts |= FW2X_CTRL_INT_LOOPBACK;
		mpi_opts &= ~FW2X_CTRL_EXT_LOOPBACK;
		break;
	case 2:
		mpi_opts |= FW2X_CTRL_EXT_LOOPBACK;
		mpi_opts &= ~FW2X_CTRL_INT_LOOPBACK;
		break;
	default:
		return (EINVAL);
	}
	AQ_WRITE_REG(hw, FW2X_MPI_CONTROL2_ADDR, mpi_opts);
	return (0);
}

static uint32_t
fw2x_rpc_state_get(struct aq_hw *hw)
{
	return (AQ_READ_REG(hw, FW2X_RPC_STATE_ADDR));
}

static int
fw2x_rpc_wait(struct aq_hw *hw, uint32_t *fw_len)
{
	struct fw2x_rpc_tid sw;
	struct fw2x_rpc_tid fw;
	int err = 0;
	uint32_t dword_cnt;

	do {
		sw.val = AQ_READ_REG(hw, FW2X_RPC_CONTROL_ADDR);
		hw->rpc_tid = sw.tid;

		AQ_HW_WAIT_FOR(((fw.val = fw2x_rpc_state_get(hw)),
				   sw.tid == fw.tid),
		    1000U, 100000U);
		if (err != EOK)
			return (EIO);

		if (fw.len == 0xFFFFU) {
			err = fw2x_rpc_call(hw, NULL, sw.len);
			if (err != 0)
				return (err);
		}
	} while (sw.tid != fw.tid || fw.len == 0xFFFFU);

	if (fw.len > AQ_FW_RPC_MAX)
		return (EINVAL);
	if (fw.len != 0) {
		dword_cnt = (fw.len + sizeof(uint32_t) - 1U) / sizeof(uint32_t);
		err = aq_hw_fw_downld_dwords(hw, hw->rpc_addr,
		    (uint32_t *)(void *)hw->rpc_buf, dword_cnt);
		if (err != 0)
			return (err);
	}
	if (fw_len)
		*fw_len = fw.len;
	return (0);
}

int
fw2x_reset(struct aq_hw *hw)
{
	fw2x_capabilities caps = { 0 };
	AQ_DBG_ENTER();
	int err = aq_hw_fw_downld_dwords(hw,
	    hw->mbox_addr + offsetof(fw2x_mailbox, caps), (uint32_t *)&caps,
	    sizeof(caps) / sizeof(uint32_t));
	if (err == EOK) {
		hw->fw_caps = caps.caps_lo | ((uint64_t)caps.caps_hi << 32);
		trace(dbg_init, "fw2x> F/W capabilities mask = %llx",
		    (unsigned long long)hw->fw_caps);
	} else {
		trace_error(dbg_init,
		    "fw2x> can't get F/W capabilities mask, error %d", err);
	}

	AQ_DBG_EXIT(EOK);
	return (EOK);
}

static aq_fw2x_rate
link_speed_mask_to_fw2x_(uint32_t speed)
{
	uint32_t rate = 0;

	AQ_DBG_ENTER();
	if (speed & aq_fw_10G)
		rate |= FW2X_RATE_10G;

	if (speed & aq_fw_5G)
		rate |= FW2X_RATE_5G;

	if (speed & aq_fw_2G5)
		rate |= FW2X_RATE_2G5;

	if (speed & aq_fw_1G)
		rate |= FW2X_RATE_1G;

	if (speed & aq_fw_100M)
		rate |= FW2X_RATE_100M;

	AQ_DBG_EXIT(rate);
	return ((aq_fw2x_rate)rate);
}

int
fw2x_set_mode(struct aq_hw *hw, enum aq_hw_fw_mpi_state_e mode,
    aq_fw_link_speed_t speed)
{
	uint64_t mpi_ctrl = get_mpi_ctrl_(hw);

	AQ_DBG_ENTERA("speed=%d", speed);
	switch (mode) {
	case MPI_INIT:
		mpi_ctrl &= ~FW2X_RATE_MASK;
		mpi_ctrl |= link_speed_mask_to_fw2x_(speed);
		mpi_ctrl &= ~FW2X_CAP_LINK_DROP;
		mpi_ctrl &= ~FW2X_EEE_MASK;
		mpi_ctrl |= fw2x_eee_mask_to_caps_(hw->eee_rate);
		if (hw->fc.fc_rx)
			mpi_ctrl |= FW2X_FW_CAP_PAUSE;
		if (hw->fc.fc_tx)
			mpi_ctrl |= FW2X_FW_CAP_ASYM_PAUSE;
		break;

	case MPI_DEINIT:
		mpi_ctrl &= ~FW2X_RATE_MASK;
		mpi_ctrl &= ~(FW2X_FW_CAP_PAUSE | FW2X_FW_CAP_ASYM_PAUSE);
		break;

	default:
		trace_error(dbg_init, "fw2x> unknown MPI state %d", mode);
		return (EINVAL);
	}

	set_mpi_ctrl_(hw, mpi_ctrl);
	AQ_DBG_EXIT(EOK);
	return (EOK);
}

int
fw2x_get_mode(struct aq_hw *hw, enum aq_hw_fw_mpi_state_e *mode,
    aq_fw_link_speed_t *link_speed, aq_fw_link_fc_t *fc)
{
	uint64_t mpi_state = get_mpi_state_(hw);
	uint32_t rates = mpi_state & FW2X_RATE_MASK;

	//   AQ_DBG_ENTER();

	if (mode) {
		uint64_t mpi_ctrl = get_mpi_ctrl_(hw);
		if (mpi_ctrl & FW2X_RATE_MASK)
			*mode = MPI_INIT;
		else
			*mode = MPI_DEINIT;
	}

	aq_fw_link_speed_t speed = aq_fw_none;

	if (rates & FW2X_RATE_10G)
		speed = aq_fw_10G;
	else if (rates & FW2X_RATE_5G)
		speed = aq_fw_5G;
	else if (rates & FW2X_RATE_2G5)
		speed = aq_fw_2G5;
	else if (rates & FW2X_RATE_1G)
		speed = aq_fw_1G;
	else if (rates & FW2X_RATE_100M)
		speed = aq_fw_100M;

	if (link_speed)
		*link_speed = speed;

	*fc = (mpi_state & (FW2X_FW_CAP_PAUSE | FW2X_FW_CAP_ASYM_PAUSE)) >>
	    (32 + CAPS_HI_PAUSE);

	//    AQ_DBG_EXIT(0);
	return (EOK);
}

int
fw2x_get_mac_addr(struct aq_hw *hw, uint8_t *mac)
{
	int err = EFAULT;
	uint32_t mac_addr[2];

	AQ_DBG_ENTER();

	uint32_t efuse_shadow_addr = AQ_READ_REG(hw, 0x364);
	if (efuse_shadow_addr == 0) {
		trace_error(dbg_init, "couldn't read eFUSE Shadow Address");
		AQ_DBG_EXIT(EFAULT);
		return (EFAULT);
	}

	err = aq_hw_fw_downld_dwords(hw, efuse_shadow_addr + (40 * 4), mac_addr,
	    ARRAY_SIZE(mac_addr));
	if (err != EOK) {
		mac_addr[0] = 0;
		mac_addr[1] = 0;
		AQ_DBG_EXIT(err);
		return (err);
	}

	mac_addr[0] = bswap32(mac_addr[0]);
	mac_addr[1] = bswap32(mac_addr[1]);

	memcpy(mac, (uint8_t *)mac_addr, ETHER_ADDR_LEN);

	AQ_DBG_EXIT(EOK);
	return (EOK);
}

static inline void
fw2x_stats_from_msm(struct aq_stats_s *dst, const fw2x_msm_statistics *src)
{
	dst->ucast_pkts_rcvd = src->uprc;
	dst->mcast_pkts_rcvd = src->mprc;
	dst->bcast_pkts_rcvd = src->bprc;
	dst->err_pkts_txd = src->erpt;
	dst->ucast_pkts_txd = src->uptc;
	dst->mcast_pkts_txd = src->mptc;
	dst->bcast_pkts_txd = src->bptc;
	dst->err_pkts_rcvd = src->erpr;
	dst->mcast_octets_txd = src->mbtc;
	dst->bcast_octets_txd = src->bbtc;
	dst->mcast_octets_rcvd = src->mbrc;
	dst->bcast_octets_rcvd = src->bbrc;
	dst->ucast_octets_rcvd = src->ubrc;
	dst->ucast_octets_txd = src->ubtc;
	dst->good_pkts_txd = src->ptc;
	dst->good_pkts_rcvd = src->prc;
}

static bool
toggle_mpi_ctrl_and_wait_(struct aq_hw *hw, uint64_t mask, uint32_t timeout_ms,
    uint32_t try_count)
{
	uint64_t ctrl = get_mpi_ctrl_(hw);
	uint64_t state = get_mpi_state_(hw);

	//   AQ_DBG_ENTER();

	// First, check that control and state values are consistent
	if ((ctrl & mask) != (state & mask)) {
		trace_warn(dbg_fw,
		    "fw2x> MPI control (%#llx) and state (%#llx) are not consistent for mask %#llx!",
		    (unsigned long long)ctrl, (unsigned long long)state,
		    (unsigned long long)mask);
		AQ_DBG_EXIT(false);
		return (false);
	}

	// Invert bits (toggle) in control register
	ctrl ^= mask;
	set_mpi_ctrl_(hw, ctrl);

	// Clear all bits except masked
	ctrl &= mask;

	// Wait for FW reflecting change in state register
	while (try_count-- != 0) {
		if ((get_mpi_state_(hw) & mask) == ctrl) {
			//			AQ_DBG_EXIT(true);
			return (true);
		}
		msec_delay(timeout_ms);
	}

	trace_detail(dbg_fw,
	    "f/w2x> timeout while waiting for response in state register for bit %#llx!",
	    (unsigned long long)mask);
	//   AQ_DBG_EXIT(false);
	return (false);
}

int
fw2x_get_stats(struct aq_hw *hw, struct aq_stats_s *stats)
{
	int err = 0;
	fw2x_msm_statistics fw2x_stats = { 0 };

	//    AQ_DBG_ENTER();

	if ((hw->fw_caps & FW2X_CAP_STATISTICS) == 0) {
		trace_warn(dbg_fw, "fw2x> statistics not supported by F/W");
		return (ENOTSUP);
	}

	// Say to F/W to update the statistics
	if (!toggle_mpi_ctrl_and_wait_(hw, FW2X_CAP_STATISTICS, 1, 25)) {
		trace_error(dbg_fw, "fw2x> statistics update timeout");
		AQ_DBG_EXIT(ETIME);
		return (ETIME);
	}

	err = aq_hw_fw_downld_dwords(hw,
	    hw->mbox_addr + offsetof(fw2x_mailbox, msm),
	    (uint32_t *)&fw2x_stats, sizeof(fw2x_stats) / sizeof(uint32_t));

	fw2x_stats_from_msm(stats, &fw2x_stats);

	if (err != EOK)
		trace_error(dbg_fw,
		    "fw2x> download statistics data FAILED, error %d", err);

	//    AQ_DBG_EXIT(err);
	return (err);
}

int
fw2x_set_eee_rate(struct aq_hw *hw, uint32_t rate)
{
	uint64_t mpi_ctrl = get_mpi_ctrl_(hw);

	mpi_ctrl &= ~FW2X_EEE_MASK;
	mpi_ctrl |= fw2x_eee_mask_to_caps_(rate);
	set_mpi_ctrl_(hw, mpi_ctrl);
	return (EOK);
}

int
fw2x_get_eee_rate(struct aq_hw *hw, uint32_t *rate, uint32_t *supported,
    uint32_t *lp_rate)
{
	uint64_t mpi_state = get_mpi_state_(hw);

	if (supported)
		*supported = fw2x_caps_to_eee_mask_(hw->fw_caps);
	if (rate)
		*rate = fw2x_caps_to_eee_mask_(mpi_state);
	if (lp_rate)
		*lp_rate = 0;

	return (EOK);
}

int
fw2x_set_wol(struct aq_hw *hw, uint32_t wol_flags, const uint8_t *mac)
{
	uint32_t mpi_ctrl2;
	struct fw2x_rpc_msg msg;
	uint32_t rpc_size;
	int err = 0;

	if (hw->rpc_addr == 0)
		return (ENOTSUP);

	mpi_ctrl2 = AQ_READ_REG(hw, FW2X_MPI_CONTROL2_ADDR);

	if (wol_flags & AQ_WOL_PHY) {
		AQ_WRITE_REG(hw, FW2X_MPI_CONTROL2_ADDR,
		    mpi_ctrl2 | FW2X_CTRL_LINK_DROP);
		AQ_HW_WAIT_FOR((AQ_READ_REG(hw, FW2X_MPI_STATE2_ADDR) &
				   FW2X_CTRL_LINK_DROP) != 0,
		    1000, 100000);
		if (err != EOK)
			return (EIO);
		mpi_ctrl2 &= ~FW2X_CTRL_LINK_DROP;
		mpi_ctrl2 |= FW2X_CTRL_WAKE_ON_LINK;
	} else {
		mpi_ctrl2 &= ~FW2X_CTRL_WAKE_ON_LINK;
	}

	if (wol_flags & AQ_WOL_MAGIC) {
		mpi_ctrl2 |= FW2X_CTRL_SLEEP_PROXY | FW2X_CTRL_WOL;
		err = fw2x_rpc_wait(hw, NULL);
		if (err != 0)
			return (err);
		memset(&msg, 0, sizeof(msg));
		msg.offloads.len = sizeof(msg.offloads);
		memcpy(msg.offloads.mac_addr, mac, 6);
		rpc_size = offsetof(struct fw2x_rpc_msg, offloads) +
		    sizeof(msg.offloads);
		err = fw2x_rpc_call(hw, &msg, rpc_size);
		if (err != 0)
			return (err);
	} else {
		mpi_ctrl2 &= ~(FW2X_CTRL_SLEEP_PROXY | FW2X_CTRL_WOL);
	}

	AQ_WRITE_REG(hw, FW2X_MPI_CONTROL2_ADDR, mpi_ctrl2);
	return (EOK);
}

int
fw2x_get_phy_temp(struct aq_hw *hw, int *temp_c)
{
	uint32_t word = 0;
	int err;

	if (temp_c == NULL)
		return (EINVAL);

	err = aq_hw_fw_downld_dwords(hw,
	    hw->mbox_addr + offsetof(fw2x_mailbox, phy_temperature), &word, 1);
	if (err != EOK)
		return (err);

	word = le32toh(word);
	*temp_c = (int)(int16_t)(word & 0xffffu);
	return (EOK);
}

int
fw2x_get_cable_len(struct aq_hw *hw, uint8_t *len)
{
	uint32_t word = 0;
	int err;

	if (len == NULL)
		return (EINVAL);

	err = aq_hw_fw_downld_dwords(hw,
	    hw->mbox_addr + offsetof(fw2x_mailbox, phy_temperature), &word, 1);
	if (err != EOK)
		return (err);

	word = le32toh(word);
	*len = (uint8_t)((word >> 16) & 0xffu);
	return (EOK);
}

int
fw2x_get_cable_diag(struct aq_hw *hw, uint32_t lane_data[4])
{
	int err;

	if (lane_data == NULL)
		return (EINVAL);

	err = aq_hw_fw_downld_dwords(hw,
	    hw->mbox_addr + offsetof(fw2x_mailbox, diag_data), lane_data, 4);
	if (err != EOK)
		return (err);

	lane_data[0] = le32toh(lane_data[0]);
	lane_data[1] = le32toh(lane_data[1]);
	lane_data[2] = le32toh(lane_data[2]);
	lane_data[3] = le32toh(lane_data[3]);
	return (EOK);
}

static int
fw2x_led_control(struct aq_hw *hw, uint32_t onoff)
{
	int err = 0;

	AQ_DBG_ENTER();

	aq_hw_fw_version ver_expected = { .raw = FW2X_FW_MIN_VER_LED };
	if (aq_hw_ver_match(&ver_expected, &hw->fw_version))
		AQ_WRITE_REG(hw, FW2X_MPI_LED_ADDR,
		    onoff ? (FW2X_LED_BLINK | (FW2X_LED_BLINK << 2) |
				(FW2X_LED_BLINK << 4)) :
			    FW2X_LED_DEFAULT);

	AQ_DBG_EXIT(err);
	return (err);
}

struct aq_firmware_ops aq_fw2x_ops = {
	.reset = fw2x_reset,

	.set_mode = fw2x_set_mode,
	.get_mode = fw2x_get_mode,

	.get_mac_addr = fw2x_get_mac_addr,
	.get_stats = fw2x_get_stats,

	.led_control = fw2x_led_control,
	.get_phy_temp = fw2x_get_phy_temp,
	.get_cable_len = fw2x_get_cable_len,
	.get_cable_diag = fw2x_get_cable_diag,
	.set_downshift = fw2x_set_downshift,
	.set_eee_rate = fw2x_set_eee_rate,
	.get_eee_rate = fw2x_get_eee_rate,
};
