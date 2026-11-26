/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Sam Sheridan
 */

#ifndef	_IWLWIFI_MVM_RS_TABLE_H
#define	_IWLWIFI_MVM_RS_TABLE_H

#include <linux/types.h>

#include "fw/api/rs.h"

/*
 * Fill a link-quality rate table with the legacy rates in rate_idx_mask
 * (bits are IWL_RATE_*_INDEX values) in descending bitrate order, the
 * lowest rate repeated to the end.  Entries are in the driver's v3 rate
 * format with tx_ant as the antenna.  Returns -EINVAL and leaves the
 * table alone when no usable rate or antenna is given.
 */
int	iwl_mvm_rs_legacy_table(bool is_2ghz, u32 rate_idx_mask, u8 tx_ant,
	    u32 table[LQ_MAX_RETRY_NUM]);

/*
 * As iwl_mvm_rs_legacy_table, starting at the fastest rate in the set not
 * faster than head_idx (an IWL_RATE_*_INDEX value).  -EINVAL when no rate
 * in the set is that slow.
 */
int	iwl_mvm_rs_legacy_table_from(bool is_2ghz, u32 rate_idx_mask,
	    u8 head_idx, u8 tx_ant, u32 table[LQ_MAX_RETRY_NUM]);

/*
 * Fill a link-quality rate table with single-stream HT rates at 20 MHz and
 * long guard interval, from the highest MCS in mcs_mask (bits are MCS 0-7)
 * not above mcs down to MCS 0, then the legacy rates not faster than the
 * last HT entry, the slowest repeated to the end.  Returns -EINVAL and
 * leaves the table alone when mcs is above 7, no HT rate at or below mcs
 * is in mcs_mask, no legacy rate is slow enough for the tail, or the
 * antenna is bad.
 */
int	iwl_mvm_rs_ht_table(u8 mcs, u8 mcs_mask, bool is_2ghz,
	    u32 rate_idx_mask, u8 tx_ant, u32 table[LQ_MAX_RETRY_NUM]);

/* The IWL_RATE_*_INDEX value of a legacy rate in 500 kbit/s, or -1. */
int	iwl_mvm_rs_legacy_index(u8 dot11rate);

/* What net80211's rate control last chose for a station. */
enum iwl_mvm_rs_request_type {
	IWL_MVM_RS_REQ_NONE = 0,
	IWL_MVM_RS_REQ_LEGACY,		/* rate is an IWL_RATE_*_INDEX value */
	IWL_MVM_RS_REQ_HT,		/* rate is a single-stream MCS */
};

struct iwl_mvm_rs_request {
	u8	type;
	u8	rate;
};

/*
 * The newest request for a station and whether it still awaits processing.
 * gen orders requests and only grows; consumed_gen is the last request
 * processed, whether or not a table came of it.
 */
struct iwl_mvm_rs_desired {
	struct iwl_mvm_rs_request req;
	u32	gen;
	u32	consumed_gen;
	bool	pending;
};

/* Drop the request; gen stays as the floor for the next ones. */
void	iwl_mvm_rs_desired_reset(struct iwl_mvm_rs_desired *d);
/* Keep req if gen is above every generation seen; true when kept. */
bool	iwl_mvm_rs_desired_offer(struct iwl_mvm_rs_desired *d,
	    const struct iwl_mvm_rs_request *req, u32 gen);
/* Copy the pending request and its generation; false when none is owed. */
bool	iwl_mvm_rs_desired_take(const struct iwl_mvm_rs_desired *d,
	    struct iwl_mvm_rs_request *req, u32 *gen);
/* Generation gen was processed; pending stays if a newer one arrived. */
void	iwl_mvm_rs_desired_consumed(struct iwl_mvm_rs_desired *d, u32 gen);

/*
 * The worker's next run: with a kick, now (returns true, *delay 0); with a
 * deadline, then (*delay from now, 0 if past); otherwise none (false).
 */
bool	iwl_mvm_rs_wk_next(bool kick, bool deadline, unsigned long now,
	    unsigned long at, unsigned long *delay);

/* Resends of a table the command queue refused, after the first attempt. */
#define	IWL_MVM_RS_LQ_RETRIES		3

/*
 * Decide after a send: 0 when it surfaced no error, 1 to try again, -1 when
 * the budget in *retries is used up.  A failed send counts in *retries.
 */
int	iwl_mvm_rs_lq_retry_next(int send_result, u8 *retries);

#endif	/* _IWLWIFI_MVM_RS_TABLE_H */
