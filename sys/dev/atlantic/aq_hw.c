/* $OpenBSD: if_aq_pci.c,v 1.34 2026/01/15 06:41:21 dlg Exp $ */
/*	$NetBSD: if_aq.c,v 1.27 2021/06/16 00:21:18 riastradh Exp $	*/

/*
 * Copyright (c) 2021 Jonathan Matthew <jonathan@d14n.org>
 * Copyright (c) 2021 Mike Larkin <mlarkin@openbsd.org>
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

/*
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitstring.h>
#include <sys/endian.h>
#include <sys/socket.h>

#include <machine/cpu.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/iflib.h>
#include <net/rss_config.h>

#include "aq_common.h"
#include "aq_dbg.h"
#include "aq_device.h"
#include "aq_fw.h"
#include "aq_hostboot.h"
#include "aq_hw.h"
#include "aq_hw_llh.h"
#include "aq_hw_llh_internal.h"

#define AQ_HW_FW_SM_RAM 0x2U
#define AQ_CFG_FW_MIN_VER_EXPECTED 0x01050006U

#define AQ2_HW_FPGA_VERSION_REG 0x00f4u
#define AQ2_LAUNCHTIME_CTRL_REG 0x7a1cu
#define AQ2_LAUNCHTIME_CTRL_RATIO_MSK 0x0000ff00u
#define AQ2_LAUNCHTIME_CTRL_RATIO_SHIFT 8
#define AQ2_LAUNCHTIME_CTRL_RATIO_SPEED_QUARTER 4u
#define AQ2_LAUNCHTIME_CTRL_RATIO_SPEED_HALF 2u
#define AQ2_LAUNCHTIME_CTRL_RATIO_SPEED_FULL 1u

#define AQ2_RPF_NEW_CTRL_REG 0x5104u
#define AQ2_RPF_NEW_CTRL_ENABLE BIT(11)

#define AQ2_RPF_REDIR2_REG 0x54c8u
#define AQ2_RPF_REDIR2_INDEX BIT(12)
#define AQ2_RPF_REDIR2_HASHTYPE_MSK 0x000001ffu
#define AQ2_RPF_REDIR2_HASHTYPE_IPV4 BIT(0)
#define AQ2_RPF_REDIR2_HASHTYPE_IPV4_TCP BIT(1)
#define AQ2_RPF_REDIR2_HASHTYPE_IPV4_UDP BIT(2)
#define AQ2_RPF_REDIR2_HASHTYPE_IPV6 BIT(3)
#define AQ2_RPF_REDIR2_HASHTYPE_IPV6_TCP BIT(4)
#define AQ2_RPF_REDIR2_HASHTYPE_IPV6_UDP BIT(5)
#define AQ2_RPF_REDIR2_HASHTYPE_IPV6_EX BIT(6)
#define AQ2_RPF_REDIR2_HASHTYPE_IPV6_TCP_EX BIT(7)
#define AQ2_RPF_REDIR2_HASHTYPE_IPV6_UDP_EX BIT(8)
#define AQ2_RPF_REDIR2_HASHTYPE_ALL                                           \
	(AQ2_RPF_REDIR2_HASHTYPE_IPV4 | AQ2_RPF_REDIR2_HASHTYPE_IPV4_TCP |    \
	    AQ2_RPF_REDIR2_HASHTYPE_IPV4_UDP | AQ2_RPF_REDIR2_HASHTYPE_IPV6 | \
	    AQ2_RPF_REDIR2_HASHTYPE_IPV6_TCP |                                \
	    AQ2_RPF_REDIR2_HASHTYPE_IPV6_UDP |                                \
	    AQ2_RPF_REDIR2_HASHTYPE_IPV6_EX |                                 \
	    AQ2_RPF_REDIR2_HASHTYPE_IPV6_TCP_EX |                             \
	    AQ2_RPF_REDIR2_HASHTYPE_IPV6_UDP_EX)

#define AQ2_RPF_REC_TAB_ENABLE_REG 0x6ff0u
#define AQ2_RPF_REC_TAB_ENABLE_MSK 0x0000ffffu

#define AQ2_RPF_L2BC_TAG_REG 0x50f0u
#define AQ2_RPF_L2BC_TAG_MSK 0x0000003fu
#define AQ2_RPF_L2UC_MSW_REG(i) (0x5114u + (i) * 8u)

#define AQ2_ART_SEM_REG 0x03acu
#define AQ2_RPF_ACT_ART_REQ_TAG_REG(i) (0x14000u + (i) * 0x10u)
#define AQ2_RPF_ACT_ART_REQ_MASK_REG(i) (0x14004u + (i) * 0x10u)
#define AQ2_RPF_ACT_ART_REQ_ACTION_REG(i) (0x14008u + (i) * 0x10u)

#define AQ2_ART_ACTION_ACT_SHIFT 8
#define AQ2_ART_ACTION_RSS 0x0080u
#define AQ2_ART_ACTION_INDEX_SHIFT 2
#define AQ2_ART_ACTION_ENABLE 0x0001u
#define AQ2_ART_ACTION(act, rss, idx, en)           \
	(((act) << AQ2_ART_ACTION_ACT_SHIFT) |      \
	    ((rss) ? AQ2_ART_ACTION_RSS : 0u) |     \
	    ((idx) << AQ2_ART_ACTION_INDEX_SHIFT) | \
	    ((en) ? AQ2_ART_ACTION_ENABLE : 0u))
#define AQ2_ART_ACTION_DROP AQ2_ART_ACTION(0, 0, 0, 1)
#define AQ2_ART_ACTION_DISABLE AQ2_ART_ACTION(0, 0, 0, 0)
#define AQ2_ART_ACTION_ASSIGN_TC(tc) AQ2_ART_ACTION(1, 1, (tc), 1)
#define AQ2_ART_ACTION_ASSIGN_QUEUE(q) AQ2_ART_ACTION(1, 0, (q), 1)

#define AQ2_RPF_TAG_PCP_MASK 0xe0000000u
#define AQ2_RPF_TAG_PCP_SHIFT 29
#define AQ2_RPF_TAG_UNTAG_MASK 0x00004000u
#define AQ2_RPF_TAG_VLAN_MASK 0x00003c00u
#define AQ2_RPF_TAG_VLAN_SHIFT 10
#define AQ2_RPF_TAG_ALLMC_MASK 0x00000040u
#define AQ2_RPF_TAG_UC_MASK 0x0000002fu

#define AQ2_RPF_INDEX_L2_PROMISC_OFF 0
#define AQ2_RPF_INDEX_VLAN_PROMISC_OFF 1
#define AQ2_RPF_INDEX_VLAN_USER 40
#define AQ2_RPF_INDEX_PCP_TO_TC 56

#define AQ2_RX_Q_TC_MAP_REG(i) (0x5900u + (i) * 4u)
#define AQ2_TX_Q_TC_MAP_REG(i) (0x799cu + (i) * 4u)

#define AQ2_HW_NUM_TCS 4u

#define AQ2_RPF_RSS_REDIR_MAX 64u
#define AQ2_RPF_RSS_REDIR_REG(tc, i) \
	(0x6200u + (0x100u * ((tc) >> 2)) + (i) * 4u)
#define AQ2_RPF_RSS_REDIR_TC_MSK(tc) (0x1fu << (5u * ((tc) & 3u)))

#define AQ2_TPS_DATA_TCT_CREDIT_MAX_MSK 0xffff0000u
#define AQ2_TPS_DATA_TCT_WEIGHT_MSK 0x00007fffu

#define AQ2_RPF_L2UC_MSW_TAG_MSK 0x03c00000u

#define AQ2_RPF_VL_TAG_REG(filter) (0x5290u + (filter) * 4u)
#define AQ2_RPF_VL_TAG_MSK 0x0000f000u
#define AQ2_RPF_VL_TAG_SHIFT 12

#define AQ2_TX_INTR_MODERATION_CTL_REG(i) (0x7c28u + (i) * 0x40u)
#define AQ2_TX_INTR_MODERATION_CTL_EN BIT(1)

#define AQ_A0_AUTO_MODERATION_REG 0x00002A00U
#define AQ_A0_AUTO_MODERATION_RESET 0x40000000U
#define AQ_A0_AUTO_MODERATION_RESTORE 0x8D000000U

#define AQ_ITR_REG_ENABLE_PATTERN 2U
#define AQ_ITR_REG_MIN_TIMER_MAX 0xFFU
#define AQ_ITR_REG_MAX_TIMER_MAX 0x1FFU
#define AQ_ITR_A0_TIMER_MAX 0x7FFU

/*
 * Deterministic PCP (0..7) -> TC mapping for AQ2.
 * For now we keep a fixed 4-TC policy:
 *   PCP 0,1 -> TC0
 *   PCP 2,3 -> TC1
 *   PCP 4,5 -> TC2
 *   PCP 6,7 -> TC3
 */
static const uint8_t aq2_pcp_to_tc_map[8] = { 0, 0, 1, 1, 2, 2, 3, 3 };

struct aq_itr_timer_pair {
	uint16_t min;
	uint16_t max;
};

/*
 * Interrupt moderation tables program the per-speed minimum and maximum
 * timer fields. Firmware reports both 5G and 5GSR links as 5000 Mbps, so
 * a single 5G bucket is sufficient here.
 */
static const struct aq_itr_timer_pair aq_itr_tx_table[] = {
	{ 0x0FU, 0x0FFU }, /* 10Gbit */
	{ 0x0FU, 0x1FFU }, /* 5Gbit */
	{ 0x0FU, 0x1FFU }, /* 2.5Gbit */
	{ 0x0FU, 0x1FFU }, /* 1Gbit */
	{ 0x0FU, 0x1FFU }, /* 100Mbit */
};

static const struct aq_itr_timer_pair aq_itr_rx_table[] = {
	{ 0x06U, 0x038U }, /* 10Gbit */
	{ 0x0CU, 0x070U }, /* 5Gbit */
	{ 0x18U, 0x0E0U }, /* 2.5Gbit */
	{ 0x30U, 0x080U }, /* 1Gbit */
	{ 0x04U, 0x050U }, /* 100Mbit */
};

static const uint16_t aq_itr_a0_table[] = {
	0x01CU, /* 10Gbit */
	0x039U, /* 5Gbit */
	0x073U, /* 2.5Gbit */
	0x120U, /* 1Gbit */
	0x1FFU, /* 100Mbit */
};

static unsigned int
aq_hw_link_speed_to_itr_index(uint32_t mbps)
{
	switch (mbps) {
	case 5000U:
		return (1U);
	case 2500U:
		return (2U);
	case 1000U:
		return (3U);
	case 100U:
		return (4U);
	case 10000U:
	default:
		return (0U);
	}
}

static uint32_t
aq_hw_get_link_speed(struct aq_hw *hw)
{
	struct aq_hw_fc_info fc_neg;
	uint32_t link_speed = 0U;

	if (aq_hw_get_link_state(hw, &link_speed, &fc_neg) != 0)
		link_speed = 0U;

	return (link_speed);
}

static int
aq_hw_interrupt_moderation_set_a0(struct aq_hw *hw)
{
	uint32_t link_speed;
	uint32_t itr;
	unsigned int i;

	switch (hw->itr_mode) {
	case AQ_ITR_MODE_OFF:
		itr = 0U;
		break;
	case AQ_ITR_MODE_ON:
		itr = min((uint32_t)(hw->itr_tx / 2U), AQ_ITR_A0_TIMER_MAX);
		itr = 0x80000000U | (itr << 16);
		break;
	case AQ_ITR_MODE_AUTO: {
		uint32_t n;
		unsigned int speed_index;

		link_speed = aq_hw_get_link_speed(hw);
		n = AQ_READ_REG(hw, AQ_A0_AUTO_MODERATION_REG) & 0xFFFFU;
		if (n < link_speed) {
			itr = 0U;
		} else {
			speed_index = aq_hw_link_speed_to_itr_index(link_speed);
			itr = 0x80000000U |
			    ((uint32_t)aq_itr_a0_table[speed_index] << 16);
		}

		AQ_WRITE_REG(hw, AQ_A0_AUTO_MODERATION_REG,
		    AQ_A0_AUTO_MODERATION_RESET);
		AQ_WRITE_REG(hw, AQ_A0_AUTO_MODERATION_REG,
		    AQ_A0_AUTO_MODERATION_RESTORE);
		break;
	}
	default:
		return (EINVAL);
	}

	for (i = HW_ATL_B0_RINGS_MAX; i--;) {
		reg_irq_thr_set(hw, itr, i);
	}

	return (aq_hw_err_from_flags(hw));
}

int
aq_hw_interrupt_moderation_set(struct aq_hw *hw)
{
	uint32_t itr_rx = AQ_ITR_REG_ENABLE_PATTERN;
	uint32_t itr_tx = AQ_ITR_REG_ENABLE_PATTERN;
	unsigned int speed_index;
	unsigned int i;
	int err;

	AQ_DBG_ENTER();

	if (AQ_HW_IS_AQ1_A0(hw)) {
		err = aq_hw_interrupt_moderation_set_a0(hw);
		AQ_DBG_EXIT(err);
		return (err);
	}

	switch (hw->itr_mode) {
	case AQ_ITR_MODE_OFF:
		tdm_tx_desc_wr_wb_irq_en_set(hw, 1U);
		tdm_tdm_intr_moder_en_set(hw, 0U);
		rdm_rx_desc_wr_wb_irq_en_set(hw, 1U);
		rdm_rdm_intr_moder_en_set(hw, 0U);
		itr_tx = 0U;
		itr_rx = 0U;
		break;
	case AQ_ITR_MODE_ON: {
		uint32_t tx_max_timer;
		uint32_t tx_min_timer;
		uint32_t rx_max_timer;
		uint32_t rx_min_timer;

		tdm_tx_desc_wr_wb_irq_en_set(hw, 0U);
		tdm_tdm_intr_moder_en_set(hw, 1U);
		rdm_rx_desc_wr_wb_irq_en_set(hw, 0U);
		rdm_rdm_intr_moder_en_set(hw, 1U);

		tx_max_timer = min((uint32_t)(hw->itr_tx / 2U),
		    AQ_ITR_REG_MAX_TIMER_MAX);
		tx_min_timer = min(tx_max_timer / 2U, AQ_ITR_REG_MIN_TIMER_MAX);
		rx_max_timer = min((uint32_t)(hw->itr_rx / 2U),
		    AQ_ITR_REG_MAX_TIMER_MAX);
		rx_min_timer = min(rx_max_timer / 2U, AQ_ITR_REG_MIN_TIMER_MAX);

		itr_tx |= tx_min_timer << 8;
		itr_tx |= tx_max_timer << 16;
		itr_rx |= rx_min_timer << 8;
		itr_rx |= rx_max_timer << 16;
		break;
	}
	case AQ_ITR_MODE_AUTO:
		tdm_tx_desc_wr_wb_irq_en_set(hw, 0U);
		tdm_tdm_intr_moder_en_set(hw, 1U);
		rdm_rx_desc_wr_wb_irq_en_set(hw, 0U);
		rdm_rdm_intr_moder_en_set(hw, 1U);

		speed_index = aq_hw_link_speed_to_itr_index(
		    aq_hw_get_link_speed(hw));

		itr_tx |= (uint32_t)aq_itr_tx_table[speed_index].min << 8;
		itr_tx |= (uint32_t)aq_itr_tx_table[speed_index].max << 16;
		itr_rx |= (uint32_t)aq_itr_rx_table[speed_index].min << 8;
		itr_rx |= (uint32_t)aq_itr_rx_table[speed_index].max << 16;
		break;
	default:
		err = EINVAL;
		AQ_DBG_EXIT(err);
		return (err);
	}

	for (i = HW_ATL_B0_RINGS_MAX; i--;) {
		if (AQ_HW_IS_AQ2(hw)) {
			AQ_WRITE_REG(hw, AQ2_TX_INTR_MODERATION_CTL_REG(i),
			    itr_tx == 0U ?
				0U :
				itr_tx | AQ2_TX_INTR_MODERATION_CTL_EN);
		} else {
			reg_tx_intr_moder_ctrl_set(hw, itr_tx, i);
		}
		reg_rx_intr_moder_ctrl_set(hw, itr_rx, i);
	}

	err = aq_hw_err_from_flags(hw);
	AQ_DBG_EXIT(err);
	return (err);
}

int
aq_hw_err_from_flags(struct aq_hw *hw)
{
	return (0);
}

static int
aq2_filter_art_set(struct aq_hw *hw, uint32_t idx, uint32_t tag, uint32_t mask,
    uint32_t action)
{
	int timo;
	uint32_t sem;

	if (!AQ_HW_IS_AQ2(hw))
		return (0);

	for (timo = 1000; timo > 0; --timo) {
		sem = AQ_READ_REG(hw, AQ2_ART_SEM_REG);
		if (sem == 1U)
			break;
		usec_delay(10);
	}

	if (timo == 0)
		return (ETIMEDOUT);

	idx += hw->art_base_index;
	AQ_WRITE_REG(hw, AQ2_RPF_ACT_ART_REQ_TAG_REG(idx), tag);
	AQ_WRITE_REG(hw, AQ2_RPF_ACT_ART_REQ_MASK_REG(idx), mask);
	AQ_WRITE_REG(hw, AQ2_RPF_ACT_ART_REQ_ACTION_REG(idx), action);
	AQ_WRITE_REG(hw, AQ2_ART_SEM_REG, 1U);

	return (0);
}

static void
aq_hw_chip_features_init(struct aq_hw *hw, uint32_t *p)
{
	uint32_t chip_features = hw->chip_features &
	    (AQ_HW_CHIP_ATLANTIC | AQ_HW_CHIP_ANTIGUA);
	uint32_t val = reg_glb_mif_id_get(hw);
	uint32_t mif_rev = val & 0xFFU;

	if ((0xFU & mif_rev) == 1U) {
		chip_features |= AQ_HW_CHIP_REVISION_A0 | AQ_HW_CHIP_MPI_AQ |
		    AQ_HW_CHIP_MIPS;
	} else if ((0xFU & mif_rev) == 2U) {
		chip_features |= AQ_HW_CHIP_REVISION_B0 | AQ_HW_CHIP_MPI_AQ |
		    AQ_HW_CHIP_MIPS | AQ_HW_CHIP_TPO2 | AQ_HW_CHIP_RPF2;
	} else if ((0xFU & mif_rev) == 0xAU) {
		chip_features |= AQ_HW_CHIP_REVISION_B1 | AQ_HW_CHIP_MPI_AQ |
		    AQ_HW_CHIP_MIPS | AQ_HW_CHIP_TPO2 | AQ_HW_CHIP_RPF2;
	}

	*p = chip_features;
}

int
aq_hw_fw_downld_dwords(struct aq_hw *hw, uint32_t a, uint32_t *p, uint32_t cnt)
{
	int err = 0;

	//    AQ_DBG_ENTER();
	AQ_HW_WAIT_FOR(reg_glb_cpu_sem_get(hw, AQ_HW_FW_SM_RAM) == 1U, 1U,
	    10000U);

	if (err != EOK) {
		bool is_locked;

		reg_glb_cpu_sem_set(hw, 1U, AQ_HW_FW_SM_RAM);
		is_locked = reg_glb_cpu_sem_get(hw, AQ_HW_FW_SM_RAM);
		if (!is_locked) {
			err = ETIME;
			goto err_exit;
		}
	}

	mif_mcp_up_mailbox_addr_set(hw, a);

	for (++cnt; --cnt && !err;) {
		mif_mcp_up_mailbox_execute_operation_set(hw, 1);

		if (IS_CHIP_FEATURE(hw, REVISION_B1))
			AQ_HW_WAIT_FOR(a != mif_mcp_up_mailbox_addr_get(hw), 1U,
			    1000U);
		else
			AQ_HW_WAIT_FOR(!mif_mcp_up_mailbox_busy_get(hw), 1,
			    1000U);

		*(p++) = mif_mcp_up_mailbox_data_get(hw);
	}

	reg_glb_cpu_sem_set(hw, 1U, AQ_HW_FW_SM_RAM);

err_exit:
	//    AQ_DBG_EXIT(err);
	return (err);
}

int
aq_hw_fw_upload_dwords(struct aq_hw *hw, uint32_t a, const uint32_t *p,
    uint32_t cnt)
{
	int err = 0;

	AQ_HW_WAIT_FOR(reg_glb_cpu_sem_get(hw, AQ_HW_FW_SM_RAM) == 1U, 1U,
	    10000U);

	if (err != EOK) {
		bool is_locked;

		reg_glb_cpu_sem_set(hw, 1U, AQ_HW_FW_SM_RAM);
		is_locked = reg_glb_cpu_sem_get(hw, AQ_HW_FW_SM_RAM);
		if (!is_locked) {
			err = ETIME;
			goto err_exit;
		}
	}

	mif_mcp_up_mailbox_addr_set(hw, a);

	for (++cnt; --cnt && !err;) {
		mif_mcp_up_mailbox_data_set(hw, *(p++));
		mif_mcp_up_mailbox_execute_operation_set(hw, 1);

		if (IS_CHIP_FEATURE(hw, REVISION_B1))
			AQ_HW_WAIT_FOR(a != mif_mcp_up_mailbox_addr_get(hw), 1U,
			    1000U);
		else
			AQ_HW_WAIT_FOR(!mif_mcp_up_mailbox_busy_get(hw), 1,
			    1000U);
	}

	reg_glb_cpu_sem_set(hw, 1U, AQ_HW_FW_SM_RAM);

err_exit:
	return (err);
}

int
aq_hw_ver_match(const aq_hw_fw_version *ver_expected,
    const aq_hw_fw_version *ver_actual)
{
	AQ_DBG_ENTER();

	if (ver_actual->major_version >= ver_expected->major_version)
		return (true);
	if (ver_actual->minor_version >= ver_expected->minor_version)
		return (true);
	if (ver_actual->build_number >= ver_expected->build_number)
		return (true);

	return (false);
}

static int
aq_hw_boot_pass(struct aq_hw *hw)
{
	if (AQ_HW_IS_AQ2(hw)) {
		return (aq2_fw_reboot(hw));
	}

	hw->fw_version.raw = 0;
	return (aq_fw_reset(hw));
}

static int
aq_hw_boot(struct aq_hw *hw)
{
	int err;

	aq_hostboot_refresh_status(hw);

	if (aq_hostboot_force(hw)) {
		err = aq_hostboot_request_fw(hw);
		if (err != 0)
			goto out;
	}

	err = aq_hw_boot_pass(hw);
	if (err == AQ_HOSTBOOT_IMAGE_REQUIRED && !aq_hostboot_force(hw)) {
		err = aq_hostboot_request_fw(hw);
		if (err != 0)
			goto out;

		err = aq_hw_boot_pass(hw);
	}
	if (err == AQ_HOSTBOOT_IMAGE_REQUIRED) {
		aq_log_error(
		    "host boot firmware image is required but not found");
		err = ENOENT;
	}

out:
	aq_hostboot_release_fw(hw);
	return (err);
}

static int
aq_hw_init_ucp(struct aq_hw *hw)
{
	int err = 0;

	AQ_DBG_ENTER();

	err = aq_hw_boot(hw);
	if (err != EOK) {
		if (AQ_HW_IS_AQ2(hw))
			aq_log_error(
			    "aq_hw_init_ucp(): A2 F/W reboot failed, err %d",
			    err);
		else
			aq_log_error(
			    "aq_hw_init_ucp(): F/W reset failed, err %d", err);
		return (err);
	}
	if (AQ_HW_IS_AQ2(hw)) {
		AQ_DBG_EXIT(err);
		return (EOK);
	}

	aq_hw_chip_features_init(hw, &hw->chip_features);
	err = aq_fw_ops_init(hw);
	if (err != EOK) {
		aq_log_error("could not initialize F/W ops, err %d", err);
		return (err);
	}

	if (hw->fw_version.major_version == 1) {
		if (!AQ_READ_REG(hw, 0x370)) {
			unsigned int rnd = 0;
			unsigned int ucp_0x370 = 0;

			rnd = arc4random();

			ucp_0x370 = 0x02020202 | (0xFEFEFEFE & rnd);
			AQ_WRITE_REG(hw, AQ_HW_UCP_0X370_REG, ucp_0x370);
		}

		reg_glb_cpu_scratch_scp_set(hw, 0, 25);
	}

	/* check 10 times by 1ms */
	AQ_HW_WAIT_FOR((hw->mbox_addr = AQ_READ_REG(hw, 0x360)) != 0, 400U, 20);
	if (hw->fw_version.major_version >= 2)
		AQ_HW_WAIT_FOR((hw->rpc_addr = AQ_READ_REG(hw,
				    AQ_HW_MPI_RPC_ADDR)) != 0,
		    400U, 20);
	else
		hw->rpc_addr = 0;
	if (hw->fw_version.major_version >= 2)
		fw2x_read_settings_addr(hw);
	else
		hw->settings_addr = 0;
	aq_hw_fw_version ver_expected = { .raw = AQ_CFG_FW_MIN_VER_EXPECTED };
	if (!aq_hw_ver_match(&ver_expected, &hw->fw_version))
		aq_log_error(
		    "atlantic: aq_hw_init_ucp(), wrong FW version: expected:%x actual:%x",
		    AQ_CFG_FW_MIN_VER_EXPECTED, hw->fw_version.raw);

	AQ_DBG_EXIT(err);
	return (err);
}

int
aq_hw_mpi_create(struct aq_hw *hw)
{
	int err = 0;

	AQ_DBG_ENTER();
	err = aq_hw_init_ucp(hw);
	if (err != EOK)
		goto err_exit;

err_exit:
	AQ_DBG_EXIT(err);
	return (err);
}

int
aq_hw_mpi_read_stats(struct aq_hw *hw, struct aq_stats_s *stats)
{
	int err = 0;
	//    AQ_DBG_ENTER();

	memset(stats, 0, sizeof(*stats));

	if (hw->fw_ops && hw->fw_ops->get_stats) {
		err = hw->fw_ops->get_stats(hw, stats);
	} else {
		err = ENOTSUP;
		aq_log_error("get_stats() not supported by F/W");
	}

	if (err == EOK) {
		stats->drop_pkts_dma = reg_rx_dma_stat_counter7get(hw);
		stats->rsc_pkts_rcvd = stats_rx_lro_coalesced_pkt_count0_get(
		    hw);
	}

	//    AQ_DBG_EXIT(err);
	return (err);
}

static int
aq_hw_mpi_set(struct aq_hw *hw, enum aq_hw_fw_mpi_state_e state, uint32_t speed)
{
	int err = ENOTSUP;
	AQ_DBG_ENTERA("speed %d", speed);

	if (hw->fw_ops && hw->fw_ops->set_mode) {
		err = hw->fw_ops->set_mode(hw, state, speed);
	} else {
		aq_log_error("set_mode() not supported by F/W");
	}

	AQ_DBG_EXIT(err);
	return (err);
}

int
aq_hw_set_link_speed(struct aq_hw *hw, uint32_t speed)
{
	return (aq_hw_mpi_set(hw, MPI_INIT, speed));
}

int
aq_hw_get_link_state(struct aq_hw *hw, uint32_t *link_speed,
    struct aq_hw_fc_info *fc_neg)
{
	int err = EOK;

	//   AQ_DBG_ENTER();

	enum aq_hw_fw_mpi_state_e mode;
	aq_fw_link_speed_t speed = aq_fw_none;
	aq_fw_link_fc_t fc;

	if (hw->fw_ops && hw->fw_ops->get_mode) {
		err = hw->fw_ops->get_mode(hw, &mode, &speed, &fc);
	} else {
		aq_log_error("get_mode() not supported by F/W");
		AQ_DBG_EXIT(ENOTSUP);
		return (ENOTSUP);
	}

	if (err != EOK) {
		aq_log_error("get_mode() failed, err %d", err);
		AQ_DBG_EXIT(err);
		return (err);
	}
	*link_speed = 0;
	if (mode != MPI_INIT)
		return (0);

	switch (speed) {
	case aq_fw_10G:
		*link_speed = 10000U;
		break;
	case aq_fw_5G:
		*link_speed = 5000U;
		break;
	case aq_fw_2G5:
		*link_speed = 2500U;
		break;
	case aq_fw_1G:
		*link_speed = 1000U;
		break;
	case aq_fw_100M:
		*link_speed = 100U;
		break;
	case aq_fw_10M:
		*link_speed = 10U;
		break;
	default:
		*link_speed = 0U;
		break;
	}

	fc_neg->fc_rx = !!(fc & aq_fw_fc_ENABLE_RX);
	fc_neg->fc_tx = !!(fc & aq_fw_fc_ENABLE_TX);
	//   AQ_DBG_EXIT(0);
	return (0);
}

int
aq_hw_get_mac_permanent(struct aq_hw *hw, uint8_t *mac)
{
	int err = ENOTSUP;
	AQ_DBG_ENTER();

	if (hw->fw_ops && hw->fw_ops->get_mac_addr)
		err = hw->fw_ops->get_mac_addr(hw, mac);

	/* Couldn't get MAC address from HW. Use auto-generated one. */
	if ((mac[0] & 1) || ((mac[0] | mac[1] | mac[2]) == 0)) {
		uint16_t rnd;
		uint32_t h = 0;
		uint32_t l = 0;

		printf(
		    "atlantic: HW MAC address %x:%x:%x:%x:%x:%x is multicast or empty MAC",
		    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		printf("atlantic: Use random MAC address");

		rnd = arc4random();

		/* chip revision */
		l = 0xE3000000U | (0xFFFFU & rnd) | (0x00 << 16);
		h = 0x8001300EU;

		mac[5] = (uint8_t)(0xFFU & l);
		l >>= 8;
		mac[4] = (uint8_t)(0xFFU & l);
		l >>= 8;
		mac[3] = (uint8_t)(0xFFU & l);
		l >>= 8;
		mac[2] = (uint8_t)(0xFFU & l);
		mac[1] = (uint8_t)(0xFFU & h);
		h >>= 8;
		mac[0] = (uint8_t)(0xFFU & h);

		err = EOK;
	}

	AQ_DBG_EXIT(err);
	return (err);
}

int
aq_hw_deinit(struct aq_hw *hw)
{
	AQ_DBG_ENTER();
	aq_hw_mpi_set(hw, MPI_DEINIT, 0);
	AQ_DBG_EXIT(0);
	return (0);
}

int
aq_hw_set_power(struct aq_hw *hw, unsigned int power_state)
{
	AQ_DBG_ENTER();
	if (AQ_HW_IS_AQ2(hw)) {
		if (hw->wol_flags != 0)
			aq2_fw_set_wol(hw, hw->wol_flags, hw->mac_addr);
		else
			aq_hw_mpi_set(hw, MPI_DEINIT, 0);
	} else {
		if (hw->wol_flags != 0 && hw->fw_ops == &aq_fw2x_ops)
			fw2x_set_wol(hw, hw->wol_flags, hw->mac_addr);
		else
			aq_hw_mpi_set(hw, MPI_POWER, 0);
	}
	AQ_DBG_EXIT(0);
	return (0);
}

int
aq_hw_get_phy_temp(struct aq_hw *hw, int *temp_c)
{
	if (hw->fw_ops && hw->fw_ops->get_phy_temp)
		return (hw->fw_ops->get_phy_temp(hw, temp_c));
	return (ENOTSUP);
}

int
aq_hw_get_cable_len(struct aq_hw *hw, uint8_t *len)
{
	if (hw->fw_ops && hw->fw_ops->get_cable_len)
		return (hw->fw_ops->get_cable_len(hw, len));
	return (ENOTSUP);
}

int
aq_hw_get_cable_diag(struct aq_hw *hw, uint32_t lane_data[4])
{
	if (hw->fw_ops && hw->fw_ops->get_cable_diag)
		return (hw->fw_ops->get_cable_diag(hw, lane_data));
	return (ENOTSUP);
}

int
aq_hw_set_downshift(struct aq_hw *hw, uint32_t counter)
{
	if (hw->fw_ops && hw->fw_ops->set_downshift)
		return (hw->fw_ops->set_downshift(hw, counter));
	return (ENOTSUP);
}

int
aq_hw_set_eee_rate(struct aq_hw *hw, uint32_t rate)
{
	if (hw->fw_ops && hw->fw_ops->set_eee_rate)
		return (hw->fw_ops->set_eee_rate(hw, rate));
	return (ENOTSUP);
}

int
aq_hw_get_eee_rate(struct aq_hw *hw, uint32_t *rate, uint32_t *supported,
    uint32_t *lp_rate)
{
	if (hw->fw_ops && hw->fw_ops->get_eee_rate)
		return (hw->fw_ops->get_eee_rate(hw, rate, supported, lp_rate));
	return (ENOTSUP);
}

/* HW NIC functions */

int
aq_hw_reset(struct aq_hw *hw)
{
	int err = 0;

	AQ_DBG_ENTER();

	err = aq_hw_boot(hw);
	if (err != EOK)
		goto err_exit;
	itr_irq_reg_res_dis_set(hw, 0);
	itr_res_irq_set(hw, 1);

	/* check 10 times by 1ms */
	AQ_HW_WAIT_FOR(itr_res_irq_get(hw) == 0, 1000, 10);
	if (err != EOK) {
		printf("atlantic: IRQ reset failed: %d", err);
		goto err_exit;
	}

	if (hw->fw_ops && hw->fw_ops->reset)
		hw->fw_ops->reset(hw);

	err = aq_hw_err_from_flags(hw);

err_exit:
	AQ_DBG_EXIT(err);
	return (err);
}

static int
aq_hw_qos_set(struct aq_hw *hw)
{
	uint32_t tc = 0U;
	uint32_t buff_size = 0U;
	unsigned int i_priority = 0U;
	int err = 0;

	AQ_DBG_ENTER();
	/* TPS Descriptor rate init */
	tps_tx_pkt_shed_desc_rate_curr_time_res_set(hw, 0x0U);
	tps_tx_pkt_shed_desc_rate_lim_set(hw, 0xA);

	/* TPS VM init */
	tps_tx_pkt_shed_desc_vm_arb_mode_set(hw, 0U);

	/* TPS TC credits init */
	tps_tx_pkt_shed_desc_tc_arb_mode_set(hw, 0U);
	tps_tx_pkt_shed_data_arb_mode_set(hw, 0U);

	if (AQ_HW_IS_AQ2(hw)) {
		AQ_WRITE_REG_BIT(hw, tps_data_tctcredit_max_adr(0U),
		    AQ2_TPS_DATA_TCT_CREDIT_MAX_MSK, 16, 0xfff0U);
		AQ_WRITE_REG_BIT(hw, tps_data_tctweight_adr(0U),
		    AQ2_TPS_DATA_TCT_WEIGHT_MSK, 0, 0x640U);
	} else {
		tps_tx_pkt_shed_tc_data_max_credit_set(hw, 0xFFF, 0U);
		tps_tx_pkt_shed_tc_data_weight_set(hw, 0x64, 0U);
	}
	tps_tx_pkt_shed_desc_tc_max_credit_set(hw, 0x50, 0U);
	tps_tx_pkt_shed_desc_tc_weight_set(hw, 0x1E, 0U);

	/* Tx buf size */
	buff_size = AQ_HW_IS_AQ2(hw) ? AQ2_HW_TXBUF_MAX : AQ_HW_TXBUF_MAX;
	tpb_tx_pkt_buff_size_per_tc_set(hw, buff_size, tc);
	tpb_tx_buff_hi_threshold_per_tc_set(hw,
	    (buff_size * (1024 / 32U) * 66U) / 100U, tc);
	tpb_tx_buff_lo_threshold_per_tc_set(hw,
	    (buff_size * (1024 / 32U) * 50U) / 100U, tc);

	/* QoS Rx buf size per TC */
	tc = 0;
	buff_size = AQ_HW_IS_AQ2(hw) ? AQ2_HW_RXBUF_MAX : AQ_HW_RXBUF_MAX;
	rpb_rx_pkt_buff_size_per_tc_set(hw, buff_size, tc);
	rpb_rx_buff_hi_threshold_per_tc_set(hw,
	    (buff_size * (1024U / 32U) * 66U) / 100U, tc);
	rpb_rx_buff_lo_threshold_per_tc_set(hw,
	    (buff_size * (1024U / 32U) * 50U) / 100U, tc);

	/* QoS 802.1p priority -> TC mapping */
	for (i_priority = 8U; i_priority--;)
		rpf_rpb_user_priority_tc_map_set(hw, i_priority, 0U);

	if (AQ_HW_IS_AQ2(hw)) {
		AQ_WRITE_REG_BIT(hw, 0x00007900U, 0x00000200U, 9, 1U);

		AQ_WRITE_REG(hw, AQ2_TX_Q_TC_MAP_REG(0), 0x00000000U);
		AQ_WRITE_REG(hw, AQ2_TX_Q_TC_MAP_REG(1), 0x00000000U);
		AQ_WRITE_REG(hw, AQ2_TX_Q_TC_MAP_REG(2), 0x01010101U);
		AQ_WRITE_REG(hw, AQ2_TX_Q_TC_MAP_REG(3), 0x01010101U);
		AQ_WRITE_REG(hw, AQ2_TX_Q_TC_MAP_REG(4), 0x02020202U);
		AQ_WRITE_REG(hw, AQ2_TX_Q_TC_MAP_REG(5), 0x02020202U);
		AQ_WRITE_REG(hw, AQ2_TX_Q_TC_MAP_REG(6), 0x03030303U);
		AQ_WRITE_REG(hw, AQ2_TX_Q_TC_MAP_REG(7), 0x03030303U);

		AQ_WRITE_REG(hw, AQ2_RX_Q_TC_MAP_REG(0), 0x00000000U);
		AQ_WRITE_REG(hw, AQ2_RX_Q_TC_MAP_REG(1), 0x11111111U);
		AQ_WRITE_REG(hw, AQ2_RX_Q_TC_MAP_REG(2), 0x22222222U);
		AQ_WRITE_REG(hw, AQ2_RX_Q_TC_MAP_REG(3), 0x33333333U);
	}

	err = aq_hw_err_from_flags(hw);
	AQ_DBG_EXIT(err);
	return (err);
}

static int
aq_hw_offload_set(struct aq_hw *hw, bool rx_ip_csum_enable,
    bool rx_l4_csum_enable)
{
	int err = 0;

	AQ_DBG_ENTER();
	/* TX checksums offloads*/
	tpo_ipv4header_crc_offload_en_set(hw, 1);
	tpo_tcp_udp_crc_offload_en_set(hw, 1);
	if (err != EOK)
		goto err_exit;

	/* RX checksums offloads*/
	rpo_ipv4header_crc_offload_en_set(hw, rx_ip_csum_enable);
	rpo_tcp_udp_crc_offload_en_set(hw, rx_l4_csum_enable);
	if (err != EOK)
		goto err_exit;

	/* LSO offloads*/
	tdm_large_send_offload_en_set(hw, 0xFFFFFFFFU);
	if (err != EOK)
		goto err_exit;

	/* LRO offloads */
	{
		uint32_t i = 0;
		uint32_t val = (8U < HW_ATL_B0_LRO_RXD_MAX) ?
		    0x3U :
		    ((4U < HW_ATL_B0_LRO_RXD_MAX) ?
			    0x2U :
			    ((2U < HW_ATL_B0_LRO_RXD_MAX) ? 0x1U : 0x0));

		for (i = 0; i < HW_ATL_B0_RINGS_MAX; i++)
			rpo_lro_max_num_of_descriptors_set(hw, val, i);

		rpo_lro_time_base_divider_set(hw, 0x61AU);
		rpo_lro_inactive_interval_set(hw, 0);
		/* the LRO timebase divider is 5 uS (0x61a),
		 * to get a maximum coalescing interval of 250 uS,
		 * we need to multiply by 50(0x32) to get
		 * the default value 250 uS
		 */
		rpo_lro_max_coalescing_interval_set(hw, 50);

		rpo_lro_qsessions_lim_set(hw, 1U);

		rpo_lro_total_desc_lim_set(hw, 2U);

		rpo_lro_patch_optimization_en_set(hw, 0U);

		rpo_lro_min_pay_of_first_pkt_set(hw, 10U);

		rpo_lro_pkt_lim_set(hw, 1U);

		rpo_lro_en_set(hw, (hw->lro_enabled ? 0xFFFFFFFFU : 0U));
	}

	err = aq_hw_err_from_flags(hw);

err_exit:
	AQ_DBG_EXIT(err);
	return (err);
}

static int
aq_hw_init_tx_path(struct aq_hw *hw)
{
	int err = 0;

	AQ_DBG_ENTER();

	/* Tx TC/RSS number config */
	tpb_tx_tc_mode_set(hw, 1U);

	thm_lso_tcp_flag_of_first_pkt_set(hw, 0x0FF6U);
	thm_lso_tcp_flag_of_middle_pkt_set(hw, 0x0FF6U);
	thm_lso_tcp_flag_of_last_pkt_set(hw, 0x0F7FU);

	/* Tx interrupts */
	tdm_tx_desc_wr_wb_irq_en_set(hw, 1U);

	/* misc */
	AQ_WRITE_REG(hw, 0x00007040U,
	    0x00010000U); // IS_CHIP_FEATURE(TPO2) ? 0x00010000U : 0x00000000U);
	tdm_tx_dca_en_set(hw, 0U);
	tdm_tx_dca_mode_set(hw, 0U);

	tpb_tx_path_scp_ins_en_set(hw, 1U);
	if (AQ_HW_IS_AQ2(hw))
		AQ_WRITE_REG_BIT(hw, 0x00007900U, 0x00000020U, 5, 0U);
	err = aq_hw_err_from_flags(hw);
	AQ_DBG_EXIT(err);
	return (err);
}

static int
aq_hw_init_rx_path(struct aq_hw *hw)
{
	// struct aq_nic_cfg_s *cfg = hw->aq_nic_cfg;
	unsigned int control_reg_val = 0U;
	int i;
	int err;

	AQ_DBG_ENTER();
	/* Rx TC/RSS number config */
	rpb_rpf_rx_traf_class_mode_set(hw, 1U);

	/* Rx flow control */
	rpb_rx_flow_ctl_mode_set(hw, 1U);

	/* RSS Ring selection */
	struct aq_dev *softc = (struct aq_dev *)hw->aq_dev;

	reg_rx_flr_rss_control1set(hw,
	    aq_rss_enabled(softc) ? 0xB3333333U : 0U);

	/* Multicast filters */
	{
		uint32_t mac_max = aq_hw_mac_max(hw);
		for (i = mac_max; i--;) {
			rpfl2_uc_flr_en_set(hw, (i == 0U) ? 1U : 0U, i);
			rpfl2unicast_flr_act_set(hw, 1U, i);
		}
	}
	reg_rx_flr_mcst_flr_msk_set(hw, 0x00000000U);
	reg_rx_flr_mcst_flr_set(hw, 0x00010FFFU, 0U);

	/* Vlan filters */
	rpf_vlan_outer_etht_set(hw, 0x88A8U);
	rpf_vlan_inner_etht_set(hw, 0x8100U);
	rpf_vlan_accept_untagged_packets_set(hw, true);
	rpf_vlan_untagged_act_set(hw, HW_ATL_RX_HOST);

	rpf_vlan_prom_mode_en_set(hw, 1);

	/* misc */
	if (!AQ_HW_IS_AQ2(hw)) {
		control_reg_val = 0x000F0000U; /* RPF2 */

		/* RSS hash type set for IP/TCP */
		control_reg_val |= 0x1EU;

		AQ_WRITE_REG(hw, 0x00005040U, control_reg_val);
	}

	rpfl2broadcast_en_set(hw, 1U);
	rpfl2broadcast_flr_act_set(hw, 1U);
	rpfl2broadcast_count_threshold_set(hw, 0xFFFFU & (~0U / 256U));

	if (AQ_HW_IS_AQ2(hw)) {
		AQ_WRITE_REG_BIT(hw, AQ2_RPF_REC_TAB_ENABLE_REG,
		    AQ2_RPF_REC_TAB_ENABLE_MSK, 0, 0xffffU);
		AQ_WRITE_REG_BIT(hw, AQ2_RPF_L2UC_MSW_REG(0),
		    AQ2_RPF_L2UC_MSW_TAG_MSK, 22, 1U);
		AQ_WRITE_REG_BIT(hw, AQ2_RPF_L2BC_TAG_REG, AQ2_RPF_L2BC_TAG_MSK,
		    0, 1U);

		aq2_filter_art_set(hw, AQ2_RPF_INDEX_L2_PROMISC_OFF, 0,
		    AQ2_RPF_TAG_UC_MASK | AQ2_RPF_TAG_ALLMC_MASK,
		    AQ2_ART_ACTION_DROP);
		aq2_filter_art_set(hw, AQ2_RPF_INDEX_VLAN_PROMISC_OFF, 0,
		    AQ2_RPF_TAG_VLAN_MASK | AQ2_RPF_TAG_UNTAG_MASK,
		    AQ2_ART_ACTION_DROP);

		for (i = 0; i < 8; i++) {
			aq2_filter_art_set(hw, AQ2_RPF_INDEX_PCP_TO_TC + i,
			    (i << AQ2_RPF_TAG_PCP_SHIFT), AQ2_RPF_TAG_PCP_MASK,
			    AQ2_ART_ACTION_ASSIGN_TC(aq2_pcp_to_tc_map[i]));
		}
	}

	err = aq_hw_err_from_flags(hw);
	AQ_DBG_EXIT(err);
	return (err);
}

int
aq_hw_mac_addr_set(struct aq_hw *hw, uint8_t *mac_addr, uint8_t index)
{
	int err = 0;
	unsigned int h = 0U;
	unsigned int l = 0U;

	AQ_DBG_ENTER();
	if (!mac_addr) {
		rpfl2_uc_flr_en_set(hw, 0U, index);
		err = aq_hw_err_from_flags(hw);
		goto err_exit;
	}
	h = (mac_addr[0] << 8) | (mac_addr[1]);
	l = (mac_addr[2] << 24) | (mac_addr[3] << 16) | (mac_addr[4] << 8) |
	    mac_addr[5];

	rpfl2_uc_flr_en_set(hw, 0U, index);
	rpfl2unicast_dest_addresslsw_set(hw, l, index);
	rpfl2unicast_dest_addressmsw_set(hw, h, index);
	if (AQ_HW_IS_AQ2(hw))
		AQ_WRITE_REG_BIT(hw, AQ2_RPF_L2UC_MSW_REG(index),
		    AQ2_RPF_L2UC_MSW_TAG_MSK, 22, 1U);
	rpfl2_uc_flr_en_set(hw, 1U, index);
	err = aq_hw_err_from_flags(hw);

err_exit:
	AQ_DBG_EXIT(err);
	return (err);
}

int
aq_hw_init(struct aq_hw *hw, uint8_t adm_irq, bool msix, int capenable)
{
	int err = 0;
	bool rx_ip_csum_enable;
	bool rx_l4_csum_enable;
	uint32_t val = 0;

	AQ_DBG_ENTER();

	if (!AQ_HW_IS_AQ2(hw)) {
		/* Force limit MRRS on RDM/TDM to 2K */
		val = AQ_READ_REG(hw, AQ_HW_PCI_REG_CONTROL_6_ADR);
		AQ_WRITE_REG(hw, AQ_HW_PCI_REG_CONTROL_6_ADR,
		    (val & ~0x707) | 0x404);

		/* TX DMA total request limit. B0 hardware is not capable to
		 * handle more than (8K-MRRS) incoming DMA data.
		 * Value 24 in 256byte units
		 */
		AQ_WRITE_REG(hw, AQ_HW_TX_DMA_TOTAL_REQ_LIMIT_ADR, 24);
	} else {
		uint32_t fpgaver = AQ_READ_REG(hw, AQ2_HW_FPGA_VERSION_REG);
		uint32_t ratio;

		if (fpgaver < 0x01000000U)
			ratio = AQ2_LAUNCHTIME_CTRL_RATIO_SPEED_FULL;
		else if (fpgaver >= 0x01008502U)
			ratio = AQ2_LAUNCHTIME_CTRL_RATIO_SPEED_HALF;
		else
			ratio = AQ2_LAUNCHTIME_CTRL_RATIO_SPEED_QUARTER;

		AQ_WRITE_REG_BIT(hw, AQ2_LAUNCHTIME_CTRL_REG,
		    AQ2_LAUNCHTIME_CTRL_RATIO_MSK,
		    AQ2_LAUNCHTIME_CTRL_RATIO_SHIFT, ratio);
	}
	aq_hw_init_tx_path(hw);
	aq_hw_init_rx_path(hw);

	aq_hw_mac_addr_set(hw, hw->mac_addr, AQ_HW_MAC);

	aq_hw_mpi_set(hw, MPI_INIT, hw->link_rate);

	aq_hw_qos_set(hw);

	if (AQ_HW_IS_AQ2(hw)) {
		AQ_WRITE_REG_BIT(hw, AQ2_RPF_NEW_CTRL_REG,
		    AQ2_RPF_NEW_CTRL_ENABLE, 11, 1U);
	}

	err = aq_hw_err_from_flags(hw);
	if (err != EOK)
		goto err_exit;

	/* Interrupts */
	// Enable interrupt
	itr_irq_status_cor_en_set(hw, 0);    // Disable clear-on-read for status
	itr_irq_auto_mask_clr_en_set(hw, 1); // Enable auto-mask clear.
	if (AQ_HW_IS_AQ2(hw)) {
		/* see aq_hw_atl2_igcr_table_ in linux driver */
		reg_irq_glb_ctl_set(hw, msix ? 0x20000026U : 0x20000025U);
		itr_irq_auto_masklsw_set(hw, 0xFFFFFFFFU);

		reg_gen_irq_map_set(hw,
		    ((8U << 0x18) | (1U << 0x1F)) |
			((8U << 0x10) | (1U << 0x17)),
		    0);
	} else {
		if (msix)
			itr_irq_mode_set(hw, 0x6); // MSIX + multi vector
		else
			itr_irq_mode_set(hw, 0x5); // MSI + multi vector

		reg_gen_irq_map_set(hw, 0x80 | adm_irq, 3);
	}

	rx_ip_csum_enable = !!(capenable & IFCAP_RXCSUM);
	rx_l4_csum_enable = !!(capenable & (IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6));
	aq_hw_offload_set(hw, rx_ip_csum_enable, rx_l4_csum_enable);

err_exit:
	AQ_DBG_EXIT(err);
	return (err);
}

int
aq_hw_start(struct aq_hw *hw)
{
	int err;

	AQ_DBG_ENTER();
	tpb_tx_buff_en_set(hw, 1U);
	rpb_rx_buff_en_set(hw, 1U);
	err = aq_hw_err_from_flags(hw);
	AQ_DBG_EXIT(err);
	return (err);
}

/**
 * @brief Set VLAN filter table
 * @details Configure VLAN filter table to accept (and assign the queue) traffic
 *  for the particular vlan ids.
 * Note: use this function under vlan promisc mode not to lost the traffic
 *
 * @param aq_hw_s
 * @param aq_rx_filter_vlan VLAN filter configuration
 * @return 0 - OK, errno on error
 */
int
hw_atl_b0_hw_vlan_set(struct aq_hw_s *self, struct aq_rx_filter_vlan *aq_vlans)
{
	int i;

	for (i = 0; i < AQ_HW_VLAN_MAX_FILTERS; i++) {
		hw_atl_rpf_vlan_flr_en_set(self, 0U, i);
		hw_atl_rpf_vlan_rxq_en_flr_set(self, 0U, i);
		if (aq_vlans[i].enable) {
			hw_atl_rpf_vlan_id_flr_set(self, aq_vlans[i].vlan_id,
			    i);
			hw_atl_rpf_vlan_flr_act_set(self, 1U, i);
			hw_atl_rpf_vlan_flr_en_set(self, 1U, i);
			if (aq_vlans[i].queue != 0xFF) {
				hw_atl_rpf_vlan_rxq_flr_set(self,
				    aq_vlans[i].queue, i);
				hw_atl_rpf_vlan_rxq_en_flr_set(self, 1U, i);
			}
		}
	}

	return (aq_hw_err_from_flags(self));
}

int
hw_atl_b0_hw_vlan_promisc_set(struct aq_hw_s *self, bool promisc)
{
	hw_atl_rpf_vlan_prom_mode_en_set(self, promisc);
	return (aq_hw_err_from_flags(self));
}

static void
aq2_rpf_vlan_tag_set(struct aq_hw_s *self, uint32_t tag, uint32_t filter)
{
	AQ_WRITE_REG_BIT(self, AQ2_RPF_VL_TAG_REG(filter), AQ2_RPF_VL_TAG_MSK,
	    AQ2_RPF_VL_TAG_SHIFT, tag);
}

int
aq2_hw_vlan_promisc_set(struct aq_hw_s *self, bool vlan_promisc)
{
	bool l2_promisc = AQ_READ_REG_BIT(self, rpfl2promis_mode_adr,
			      rpfl2promis_mode_msk,
			      rpfl2promis_mode_shift) != 0U;
	uint32_t action;

	hw_atl_rpf_vlan_prom_mode_en_set(self, vlan_promisc);

	/* ART filter cannot be enabled for promisc modes */
	action = (vlan_promisc || l2_promisc) ? AQ2_ART_ACTION_DISABLE :
						AQ2_ART_ACTION_DROP;

	aq2_filter_art_set(self, AQ2_RPF_INDEX_VLAN_PROMISC_OFF, 0,
	    AQ2_RPF_TAG_VLAN_MASK | AQ2_RPF_TAG_UNTAG_MASK, action);

	return (aq_hw_err_from_flags(self));
}

int
aq2_hw_vlan_set(struct aq_hw_s *self, struct aq_rx_filter_vlan *aq_vlans)
{
	uint32_t action;
	uint32_t index;
	uint32_t i;

	hw_atl_rpf_vlan_prom_mode_en_set(self, 1U);

	for (i = 0; i < AQ_HW_VLAN_MAX_FILTERS; i++) {
		hw_atl_rpf_vlan_flr_en_set(self, 0U, i);
		hw_atl_rpf_vlan_rxq_en_flr_set(self, 0U, i);
		index = AQ2_RPF_INDEX_VLAN_USER + i;
		aq2_filter_art_set(self, index, 0, 0, AQ2_ART_ACTION_DISABLE);

		if (!aq_vlans[i].enable)
			continue;

		hw_atl_rpf_vlan_id_flr_set(self, aq_vlans[i].vlan_id, i);
		hw_atl_rpf_vlan_flr_act_set(self, 1U, i);
		hw_atl_rpf_vlan_flr_en_set(self, 1U, i);

		if (aq_vlans[i].queue != 0xFF) {
			hw_atl_rpf_vlan_rxq_flr_set(self, aq_vlans[i].queue, i);
			hw_atl_rpf_vlan_rxq_en_flr_set(self, 1U, i);

			aq2_rpf_vlan_tag_set(self, i + 2, i);

			action = AQ2_ART_ACTION_ASSIGN_QUEUE(aq_vlans[i].queue);
			index = AQ2_RPF_INDEX_VLAN_USER + i;
			aq2_filter_art_set(self, index,
			    (i + 2) << AQ2_RPF_TAG_VLAN_SHIFT,
			    AQ2_RPF_TAG_VLAN_MASK, action);
		} else {
			aq2_rpf_vlan_tag_set(self, 1, i);
		}
	}

	return (aq_hw_err_from_flags(self));
}

static void
aq_hw_l2_route_set(struct aq_hw *hw, const struct aq_rx_filter_l2 *data,
    bool enabled)
{
	uint32_t route_enable = 0U;

	if (enabled && data->queue >= 0)
		route_enable = 1U;

	hw_atl_rpf_etht_flr_act_set(hw, route_enable, data->location);
	hw_atl_rpf_etht_rx_queue_en_set(hw, route_enable, data->location);
	if (route_enable != 0U)
		hw_atl_rpf_etht_rx_queue_set(hw, data->queue, data->location);
}

static void
aq_hw_l2_filter_write(struct aq_hw *hw, const struct aq_rx_filter_l2 *data,
    bool enabled)
{
	uint32_t filter_enable = enabled ? 1U : 0U;
	uint32_t ethertype = enabled ? data->ethertype : 0U;
	uint32_t prio_enable = (enabled && data->user_priority_en) ? 1U : 0U;

	hw_atl_rpf_etht_flr_en_set(hw, filter_enable, data->location);
	hw_atl_rpf_etht_flr_set(hw, ethertype, data->location);
	hw_atl_rpf_etht_user_priority_en_set(hw, prio_enable, data->location);
	if (prio_enable != 0U)
		hw_atl_rpf_etht_user_priority_set(hw, data->user_priority,
		    data->location);
	aq_hw_l2_route_set(hw, data, enabled);
}

int
aq_hw_filter_l2_set(struct aq_hw *hw, struct aq_rx_filter_l2 *data)
{
	if (AQ_HW_IS_AQ2(hw))
		return (ENOTSUP);

	aq_hw_l2_filter_write(hw, data, true);
	return (aq_hw_err_from_flags(hw));
}

int
aq_hw_filter_l2_clear(struct aq_hw *hw, struct aq_rx_filter_l2 *data)
{
	if (AQ_HW_IS_AQ2(hw))
		return (ENOTSUP);

	aq_hw_l2_filter_write(hw, data, false);
	return (aq_hw_err_from_flags(hw));
}

static void
aq_hw_l3l4_slot_clear(struct aq_hw *hw, uint8_t location)
{
	hw_atl_rpfl3l4_cmd_clear(hw, location);
	hw_atl_rpf_l4_spd_set(hw, 0U, location);
	hw_atl_rpf_l4_dpd_set(hw, 0U, location);
}

static void
aq_hw_l3l4_rule_clear(struct aq_hw *hw, const struct aq_rx_filter_l3l4 *data)
{
	uint8_t location = data->location;
	int i;

	if (!data->is_ipv6) {
		aq_hw_l3l4_slot_clear(hw, location);
		hw_atl_rpfl3l4_ipv4_src_addr_clear(hw, location);
		hw_atl_rpfl3l4_ipv4_dest_addr_clear(hw, location);
		return;
	}

	for (i = 0; i < HW_ATL_RX_CNT_REG_ADDR_IPV6; ++i)
		aq_hw_l3l4_slot_clear(hw, (uint8_t)(location + i));
	hw_atl_rpfl3l4_ipv6_src_addr_clear(hw, location);
	hw_atl_rpfl3l4_ipv6_dest_addr_clear(hw, location);
}

static void
aq_hw_l3l4_addr_set(struct aq_hw *hw, const struct aq_rx_filter_l3l4 *data)
{
	uint8_t location = data->location;
	uint32_t addr_cmp = HW_ATL_RX_ENABLE_CMP_DEST_ADDR_L3 |
	    HW_ATL_RX_ENABLE_CMP_SRC_ADDR_L3;

	if ((data->cmd & addr_cmp) == 0U)
		return;

	if (data->is_ipv6) {
		hw_atl_rpfl3l4_ipv6_dest_addr_set(hw, location, data->ip_dst);
		hw_atl_rpfl3l4_ipv6_src_addr_set(hw, location, data->ip_src);
		return;
	}

	hw_atl_rpfl3l4_ipv4_dest_addr_set(hw, location, data->ip_dst[0]);
	hw_atl_rpfl3l4_ipv4_src_addr_set(hw, location, data->ip_src[0]);
}

static void
aq_hw_l3l4_ports_set(struct aq_hw *hw, const struct aq_rx_filter_l3l4 *data)
{
	uint8_t location = data->location;
	uint32_t port_cmp = HW_ATL_RX_ENABLE_CMP_DEST_PORT_L4 |
	    HW_ATL_RX_ENABLE_CMP_SRC_PORT_L4;

	if ((data->cmd & port_cmp) == 0U)
		return;

	hw_atl_rpf_l4_dpd_set(hw, data->p_dst, location);
	hw_atl_rpf_l4_spd_set(hw, data->p_src, location);
}

int
aq_hw_filter_l3l4_clear(struct aq_hw *hw, struct aq_rx_filter_l3l4 *data)
{
	if (AQ_HW_IS_AQ2(hw))
		return (ENOTSUP);

	aq_hw_l3l4_rule_clear(hw, data);
	return (aq_hw_err_from_flags(hw));
}

int
aq_hw_filter_l3l4_set(struct aq_hw *hw, struct aq_rx_filter_l3l4 *data)
{
	int err;

	if (AQ_HW_IS_AQ2(hw))
		return (ENOTSUP);

	aq_hw_l3l4_rule_clear(hw, data);
	err = aq_hw_err_from_flags(hw);
	if (err != 0)
		return (err);

	aq_hw_l3l4_addr_set(hw, data);
	aq_hw_l3l4_ports_set(hw, data);
	hw_atl_rpfl3l4_cmd_set(hw, data->location, data->cmd);

	return (aq_hw_err_from_flags(hw));
}

void
aq_hw_set_promisc(struct aq_hw_s *self, bool l2_promisc, bool vlan_promisc,
    bool mc_promisc)
{
	AQ_DBG_ENTERA("promisc %d, vlan_promisc %d, allmulti %d", l2_promisc,
	    vlan_promisc, mc_promisc);

	rpfl2promiscuous_mode_en_set(self, l2_promisc);

	if (AQ_HW_IS_AQ2(self)) {
		uint32_t action = l2_promisc ? AQ2_ART_ACTION_DISABLE :
					       AQ2_ART_ACTION_DROP;

		/* ALLMC is gated by l2_mc_accept_all, conditional mask not
		 * needed */
		aq2_filter_art_set(self, AQ2_RPF_INDEX_L2_PROMISC_OFF, 0,
		    AQ2_RPF_TAG_UC_MASK | AQ2_RPF_TAG_ALLMC_MASK, action);

		aq2_hw_vlan_promisc_set(self, vlan_promisc);
	} else {
		hw_atl_b0_hw_vlan_promisc_set(self, l2_promisc | vlan_promisc);
	}

	rpfl2_accept_all_mc_packets_set(self, mc_promisc);
	rpfl2multicast_flr_en_set(self, mc_promisc, 0);

	AQ_DBG_EXIT(0);
}

int
aq_hw_rss_hash_set(struct aq_hw_s *self,
    uint8_t rss_key[HW_ATL_RSS_HASHKEY_SIZE])
{
	uint32_t rss_key_dw[HW_ATL_RSS_HASHKEY_SIZE / 4];
	uint32_t addr = 0U;
	uint32_t i = 0U;
	int err = 0;

	AQ_DBG_ENTER();

	memcpy(rss_key_dw, rss_key, HW_ATL_RSS_HASHKEY_SIZE);

	for (i = 10, addr = 0U; i--; ++addr) {
		uint32_t key_data = bswap32(rss_key_dw[i]);
		rpf_rss_key_wr_data_set(self, key_data);
		rpf_rss_key_addr_set(self, addr);
		rpf_rss_key_wr_en_set(self, 1U);
		AQ_HW_WAIT_FOR(rpf_rss_key_wr_en_get(self) == 0, 1000U, 10U);
		if (err != EOK)
			goto err_exit;
	}

	err = aq_hw_err_from_flags(self);

err_exit:
	AQ_DBG_EXIT(err);
	return (err);
}

int
aq_hw_rss_hash_get(struct aq_hw_s *self,
    uint8_t rss_key[HW_ATL_RSS_HASHKEY_SIZE])
{
	uint32_t rss_key_dw[HW_ATL_RSS_HASHKEY_SIZE / 4];
	uint32_t addr = 0U;
	uint32_t i = 0U;
	int err = 0;

	AQ_DBG_ENTER();

	for (i = 10, addr = 0U; i--; ++addr) {
		rpf_rss_key_addr_set(self, addr);
		rss_key_dw[i] = bswap32(rpf_rss_key_rd_data_get(self));
	}
	memcpy(rss_key, rss_key_dw, HW_ATL_RSS_HASHKEY_SIZE);

	err = aq_hw_err_from_flags(self);

	AQ_DBG_EXIT(err);
	return (err);
}

int
aq_hw_rss_hash_types_set(struct aq_hw_s *self, uint32_t rss_hash_cfg)
{
	uint32_t aq2_hash_types = 0U;

	if (!AQ_HW_IS_AQ2(self))
		return (0);

	if (rss_hash_cfg & RSS_HASHTYPE_RSS_IPV4)
		aq2_hash_types |= AQ2_RPF_REDIR2_HASHTYPE_IPV4;
	if (rss_hash_cfg & RSS_HASHTYPE_RSS_TCP_IPV4)
		aq2_hash_types |= AQ2_RPF_REDIR2_HASHTYPE_IPV4_TCP;
	if (rss_hash_cfg & RSS_HASHTYPE_RSS_UDP_IPV4)
		aq2_hash_types |= AQ2_RPF_REDIR2_HASHTYPE_IPV4_UDP;
	if (rss_hash_cfg & RSS_HASHTYPE_RSS_IPV6)
		aq2_hash_types |= AQ2_RPF_REDIR2_HASHTYPE_IPV6;
	if (rss_hash_cfg & RSS_HASHTYPE_RSS_TCP_IPV6)
		aq2_hash_types |= AQ2_RPF_REDIR2_HASHTYPE_IPV6_TCP;
	if (rss_hash_cfg & RSS_HASHTYPE_RSS_UDP_IPV6)
		aq2_hash_types |= AQ2_RPF_REDIR2_HASHTYPE_IPV6_UDP;
	if (rss_hash_cfg & RSS_HASHTYPE_RSS_IPV6_EX)
		aq2_hash_types |= AQ2_RPF_REDIR2_HASHTYPE_IPV6_EX;
	if (rss_hash_cfg & RSS_HASHTYPE_RSS_TCP_IPV6_EX)
		aq2_hash_types |= AQ2_RPF_REDIR2_HASHTYPE_IPV6_TCP_EX;
	if (rss_hash_cfg & RSS_HASHTYPE_RSS_UDP_IPV6_EX)
		aq2_hash_types |= AQ2_RPF_REDIR2_HASHTYPE_IPV6_UDP_EX;

	AQ_WRITE_REG_BIT(self, AQ2_RPF_REDIR2_REG, AQ2_RPF_REDIR2_HASHTYPE_MSK,
	    0, aq2_hash_types);

	return (aq_hw_err_from_flags(self));
}

int
aq_hw_rss_set(struct aq_hw_s *self,
    uint8_t rss_table[HW_ATL_RSS_INDIRECTION_TABLE_MAX])
{
	uint16_t bitary[(HW_ATL_RSS_INDIRECTION_TABLE_MAX * 3 / 16U)];
	int err = 0;
	uint32_t i = 0U;

	if (AQ_HW_IS_AQ2(self)) {
		struct aq_dev *softc = (struct aq_dev *)self->aq_dev;
		uint32_t qcnt = 1U;
		uint32_t tc;

		if (softc && softc->rx_rings_count > 0)
			qcnt = softc->rx_rings_count;

		AQ_WRITE_REG_BIT(self, AQ2_RPF_REDIR2_REG, AQ2_RPF_REDIR2_INDEX,
		    12, 0U);
		for (i = 0; i < AQ2_RPF_RSS_REDIR_MAX; i++) {
			for (tc = 0; tc < AQ2_HW_NUM_TCS; tc++) {
				uint32_t q = rss_table[i] % qcnt;
				uint32_t shift = 5U * (tc & 3U);

				AQ_WRITE_REG_BIT(self,
				    AQ2_RPF_RSS_REDIR_REG(tc, i),
				    AQ2_RPF_RSS_REDIR_TC_MSK(tc), shift, q);
			}
		}

		return (0);
	}

	memset(bitary, 0, sizeof(bitary));

	for (i = HW_ATL_RSS_INDIRECTION_TABLE_MAX; i--;) {
		(*(uint32_t *)(bitary + ((i * 3U) / 16U))) |= ((rss_table[i])
		    << ((i * 3U) & 0xFU));
	}

	for (i = ARRAY_SIZE(bitary); i--;) {
		rpf_rss_redir_tbl_wr_data_set(self, bitary[i]);
		rpf_rss_redir_tbl_addr_set(self, i);
		rpf_rss_redir_wr_en_set(self, 1U);
		AQ_HW_WAIT_FOR(rpf_rss_redir_wr_en_get(self) == 0, 1000U, 10U);
		if (err != EOK)
			goto err_exit;
	}

	err = aq_hw_err_from_flags(self);

err_exit:
	return (err);
}

int
aq_hw_udp_rss_enable(struct aq_hw_s *self, bool enable)
{
	int err = 0;

	if (AQ_HW_IS_AQ2(self))
		return (0);

	if (!enable) { /* HW bug workaround:
			* Disable RSS for UDP using rx flow filter 0.
			* HW does not track RSS stream for fragmenged UDP,
			* 0x5040 control reg does not work.
			*/
		hw_atl_rpf_l3_l4_enf_set(self, true, 0);
		hw_atl_rpf_l4_protf_en_set(self, true, 0);
		hw_atl_rpf_l3_l4_rxqf_en_set(self, true, 0);
		hw_atl_rpf_l3_l4_actf_set(self, L2_FILTER_ACTION_HOST, 0);
		hw_atl_rpf_l3_l4_rxqf_set(self, 0, 0);
		hw_atl_rpf_l4_protf_set(self, HW_ATL_RX_UDP, 0);
	} else {
		hw_atl_rpf_l3_l4_enf_set(self, false, 0);
	}

	err = aq_hw_err_from_flags(self);
	return (err);
}
