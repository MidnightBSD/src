/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Sam Sheridan
 */

#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/types.h>

#include "fw/api/rs.h"
#include "rs-table.h"

#define	LEGACY_CCK_MASK		(BIT(IWL_LAST_CCK_RATE + 1) - 1)
#define	LEGACY_RATE_MASK	(BIT(IWL_RATE_COUNT_LEGACY) - 1)
#define	LEGACY_ANT_MASK		(RATE_MCS_ANT_AB_MSK >> RATE_MCS_ANT_POS)

/* IWL_RATE_*_INDEX values from the fastest to the slowest rate. */
static const u8 legacy_by_bitrate[IWL_RATE_COUNT_LEGACY] = {
	IWL_RATE_54M_INDEX, IWL_RATE_48M_INDEX, IWL_RATE_36M_INDEX,
	IWL_RATE_24M_INDEX, IWL_RATE_18M_INDEX, IWL_RATE_12M_INDEX,
	IWL_RATE_11M_INDEX, IWL_RATE_9M_INDEX, IWL_RATE_6M_INDEX,
	IWL_RATE_5M_INDEX, IWL_RATE_2M_INDEX, IWL_RATE_1M_INDEX,
};

/* Bitrates in 100 kbit/s by IWL_RATE_*_INDEX value. */
static const u16 legacy_bitrate[IWL_RATE_COUNT_LEGACY] = {
	[IWL_RATE_1M_INDEX] = 10, [IWL_RATE_2M_INDEX] = 20,
	[IWL_RATE_5M_INDEX] = 55, [IWL_RATE_11M_INDEX] = 110,
	[IWL_RATE_6M_INDEX] = 60, [IWL_RATE_9M_INDEX] = 90,
	[IWL_RATE_12M_INDEX] = 120, [IWL_RATE_18M_INDEX] = 180,
	[IWL_RATE_24M_INDEX] = 240, [IWL_RATE_36M_INDEX] = 360,
	[IWL_RATE_48M_INDEX] = 480, [IWL_RATE_54M_INDEX] = 540,
};

/* Single-stream HT bitrates at 20 MHz with a long guard interval, by MCS. */
static const u16 ht20_bitrate[8] = {
	65, 130, 195, 260, 390, 520, 585, 650,
};

int
iwl_mvm_rs_legacy_index(u8 dot11rate)
{
	int i;

	for (i = 0; i < IWL_RATE_COUNT_LEGACY; i++)
		if (legacy_bitrate[i] == (u16)dot11rate * 5)
			return (i);
	return (-1);
}

/* The legacy rates in the mask whose bitrate is at most bitrate. */
static u32
legacy_not_faster_than(u32 rate_idx_mask, u16 bitrate)
{
	int i;

	for (i = 0; i < IWL_RATE_COUNT_LEGACY; i++)
		if (legacy_bitrate[i] > bitrate)
			rate_idx_mask &= ~BIT(i);
	return (rate_idx_mask);
}

static u32
legacy_rate_n_flags(u8 idx, u8 tx_ant)
{
	u32 rate;

	if (idx <= IWL_LAST_CCK_RATE)
		rate = RATE_MCS_MOD_TYPE_CCK | idx;
	else
		rate = RATE_MCS_MOD_TYPE_LEGACY_OFDM |
		    (idx - IWL_FIRST_OFDM_RATE);
	return (rate | ((u32)tx_ant << RATE_MCS_ANT_POS));
}

static u32
ht_rate_n_flags(u8 mcs, u8 tx_ant)
{
	return (RATE_MCS_MOD_TYPE_HT | (mcs & RATE_HT_MCS_CODE_MSK) |
	    ((u32)tx_ant << RATE_MCS_ANT_POS));
}

/* The legacy rates usable on the band, or 0. */
static u32
usable_legacy(bool is_2ghz, u32 rate_idx_mask)
{
	rate_idx_mask &= LEGACY_RATE_MASK;
	if (!is_2ghz)
		rate_idx_mask &= ~LEGACY_CCK_MASK;
	return (rate_idx_mask);
}

static bool
one_antenna(u8 tx_ant)
{
	return ((tx_ant & LEGACY_ANT_MASK) != 0 &&
	    (tx_ant & ~LEGACY_ANT_MASK) == 0 && (tx_ant & (tx_ant - 1)) == 0);
}

/* Legacy rates from entry n on, the slowest repeated to the end. */
static void
fill_legacy(u32 rate_idx_mask, u8 tx_ant, u32 table[LQ_MAX_RETRY_NUM], int n)
{
	int i;

	for (i = 0; i < IWL_RATE_COUNT_LEGACY && n < LQ_MAX_RETRY_NUM; i++) {
		if ((rate_idx_mask & BIT(legacy_by_bitrate[i])) == 0)
			continue;
		table[n++] = legacy_rate_n_flags(legacy_by_bitrate[i], tx_ant);
	}
	for (; n < LQ_MAX_RETRY_NUM; n++)
		table[n] = table[n - 1];
}

int
iwl_mvm_rs_legacy_table(bool is_2ghz, u32 rate_idx_mask, u8 tx_ant,
    u32 table[LQ_MAX_RETRY_NUM])
{
	return (iwl_mvm_rs_legacy_table_from(is_2ghz, rate_idx_mask,
	    IWL_RATE_54M_INDEX, tx_ant, table));
}

int
iwl_mvm_rs_legacy_table_from(bool is_2ghz, u32 rate_idx_mask, u8 head_idx,
    u8 tx_ant, u32 table[LQ_MAX_RETRY_NUM])
{
	if (head_idx >= IWL_RATE_COUNT_LEGACY || !one_antenna(tx_ant))
		return (-EINVAL);
	rate_idx_mask = legacy_not_faster_than(
	    usable_legacy(is_2ghz, rate_idx_mask), legacy_bitrate[head_idx]);
	if (rate_idx_mask == 0)
		return (-EINVAL);
	fill_legacy(rate_idx_mask, tx_ant, table, 0);
	return (0);
}

int
iwl_mvm_rs_ht_table(u8 mcs, u8 mcs_mask, bool is_2ghz, u32 rate_idx_mask,
    u8 tx_ant, u32 table[LQ_MAX_RETRY_NUM])
{
	int m, n, lowest;

	if (mcs > 7 || !one_antenna(tx_ant))
		return (-EINVAL);
	mcs_mask &= BIT(mcs + 1) - 1;
	if (mcs_mask == 0)
		return (-EINVAL);
	for (lowest = 0; (mcs_mask & BIT(lowest)) == 0; lowest++)
		;
	rate_idx_mask = legacy_not_faster_than(
	    usable_legacy(is_2ghz, rate_idx_mask), ht20_bitrate[lowest]);
	if (rate_idx_mask == 0)
		return (-EINVAL);

	n = 0;
	for (m = 7; m >= 0; m--)
		if ((mcs_mask & BIT(m)) != 0)
			table[n++] = ht_rate_n_flags(m, tx_ant);
	fill_legacy(rate_idx_mask, tx_ant, table, n);
	return (0);
}

void
iwl_mvm_rs_desired_reset(struct iwl_mvm_rs_desired *d)
{
	d->req.type = IWL_MVM_RS_REQ_NONE;
	d->req.rate = 0;
	d->consumed_gen = d->gen;
	d->pending = false;
}

bool
iwl_mvm_rs_desired_offer(struct iwl_mvm_rs_desired *d,
    const struct iwl_mvm_rs_request *req, u32 gen)
{
	if ((s32)(gen - d->gen) <= 0)
		return (false);
	d->req = *req;
	d->gen = gen;
	d->pending = true;
	return (true);
}

bool
iwl_mvm_rs_wk_next(bool kick, bool deadline, unsigned long now,
    unsigned long at, unsigned long *delay)
{
	if (kick) {
		*delay = 0;
		return (true);
	}
	if (!deadline)
		return (false);
	*delay = (long)(at - now) > 0 ? at - now : 0;
	return (true);
}

bool
iwl_mvm_rs_desired_take(const struct iwl_mvm_rs_desired *d,
    struct iwl_mvm_rs_request *req, u32 *gen)
{
	if (!d->pending)
		return (false);
	*req = d->req;
	*gen = d->gen;
	return (true);
}

void
iwl_mvm_rs_desired_consumed(struct iwl_mvm_rs_desired *d, u32 gen)
{
	d->consumed_gen = gen;
	if (gen == d->gen)
		d->pending = false;
}

int
iwl_mvm_rs_lq_retry_next(int send_result, u8 *retries)
{
	if (send_result == 0)
		return (0);
	if (*retries < IWL_MVM_RS_LQ_RETRIES) {
		(*retries)++;
		return (*retries < IWL_MVM_RS_LQ_RETRIES ? 1 : -1);
	}
	return (-1);
}
