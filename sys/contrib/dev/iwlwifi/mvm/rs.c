/*-
 * Copyright (c) 2020-2025 The FreeBSD Foundation
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
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
 *
 * $FreeBSD$
 */

/*
 * XXX-BZ:
 * This file is left as a wrapper to make mvm compile and we will only
 * deal with it on a need basis.  Most newer chipsets do this in firmware.
 */
#include <sys/param.h>
#include <net/cfg80211.h>		/* LinuxKPI 802.11 TODO() calls. */

#include "rs.h"
#include "rs-table.h"
#include "mvm.h"

/* Aggregation limits sent with a host-built rate table. */
#define	IWL_MVM_RS_AGG_TIME_LIMIT		4000
#define	IWL_MVM_RS_AGG_DISABLE_START_TH		3

/* Delay before a refused table is sent again. */
#define	IWL_MVM_RS_LQ_RETRY_MS			200
/* Shortest interval between two adaptation tables for one station. */
#define	IWL_MVM_RS_ADAPT_MS			100

#ifdef CONFIG_IWLWIFI_DEBUGFS
/*
 * Fill struct iwl_mvm_frame_stats.
 * Deal with various RATE_MCS_*_MSK. See rx.c, fw/api/rs.h, et al.
 * XXX-BZ consider calling iwl_new_rate_from_v1() in rx.c so we can also
 * use this in rxmq.c.
 */
void
iwl_mvm_update_frame_stats(struct iwl_mvm *mvm, u32 rate, bool agg)
{
	uint8_t nss;

	spin_lock_bh(&mvm->drv_stats_lock);
	mvm->drv_rx_stats.success_frames++;

	if (rate & RATE_MCS_HT_MSK_V1) {
		mvm->drv_rx_stats.ht_frames++;
		nss = 1 + ((rate & RATE_HT_MCS_NSS_MSK_V1) >> RATE_HT_MCS_NSS_POS_V1);
	} else if (rate & RATE_MCS_VHT_MSK_V1) {
		mvm->drv_rx_stats.vht_frames++;
		nss = 1 + FIELD_GET(RATE_MCS_NSS_MSK, rate);
	} else {
		mvm->drv_rx_stats.legacy_frames++;
		nss = 0;
	}

	switch (rate & RATE_MCS_CHAN_WIDTH_MSK_V1) {
	case RATE_MCS_CHAN_WIDTH_20:
		mvm->drv_rx_stats.bw_20_frames++;
		break;
	case RATE_MCS_CHAN_WIDTH_40:
		mvm->drv_rx_stats.bw_40_frames++;
		break;
	case RATE_MCS_CHAN_WIDTH_80:
		mvm->drv_rx_stats.bw_80_frames++;
		break;
	case RATE_MCS_CHAN_WIDTH_160:
		mvm->drv_rx_stats.bw_160_frames++;
		break;
	}

	if ((rate & RATE_MCS_CCK_MSK_V1) == 0 &&
	    (rate & RATE_MCS_SGI_MSK_V1) != 0)
		mvm->drv_rx_stats.sgi_frames++;
	else
		mvm->drv_rx_stats.ngi_frames++;

	switch (nss) {
	case 1:
		mvm->drv_rx_stats.siso_frames++;
		break;
	case 2:
		mvm->drv_rx_stats.mimo2_frames++;
		break;
	}

	if (agg)
		mvm->drv_rx_stats.agg_frames++;

	/* ampdu_count? */
	/* fail_frames? */

	mvm->drv_rx_stats.last_rates[mvm->drv_rx_stats.last_frame_idx] = rate;
	mvm->drv_rx_stats.last_frame_idx++;
	mvm->drv_rx_stats.last_frame_idx %=
	    ARRAY_SIZE(mvm->drv_rx_stats.last_rates);

	spin_unlock_bh(&mvm->drv_stats_lock);
}

void
iwl_mvm_reset_frame_stats(struct iwl_mvm *mvm)
{
	/* Apply same locking rx.c does; debugfs seems to read unloked? */
	spin_lock_bh(&mvm->drv_stats_lock);
	memset(&mvm->drv_rx_stats, 0, sizeof(mvm->drv_rx_stats));
	spin_unlock_bh(&mvm->drv_stats_lock);
}
#endif

int
iwl_mvm_rate_control_register(void)
{
	TODO("This likely has to call into net80211 unless we gain compat code in LinuxKPI");
	return (0);
}

void
iwl_mvm_rate_control_unregister(void)
{
	TODO("This likely has to call into net80211 unless we gain compat code in LinuxKPI");
}

int
iwl_mvm_tx_protection(struct iwl_mvm *mvm, struct iwl_mvm_sta *mvmsta, bool enable)
{
	if (iwl_mvm_has_tlc_offload(mvm))
		return (rs_fw_tx_protection(mvm, mvmsta, enable));
        else {
		TODO();
		return (0);
	}
}

/*
 * Ask for an immediate worker run while the firmware runs.  Only the worker
 * itself arms deadlines, from every station's state, so a kick can never be
 * moved later by another caller.
 */
static void
iwl_mvm_rs_wk_kick(struct iwl_mvm *mvm)
{
	spin_lock_bh(&mvm->rs_wk_lock);
	mvm->rs_wk_kick = true;
	if (test_bit(IWL_MVM_STATUS_FIRMWARE_RUNNING, &mvm->status))
		mod_delayed_work(system_wq, &mvm->rs_lq_wk, 0);
	spin_unlock_bh(&mvm->rs_wk_lock);
}

/* The station's legacy rate indices on band and the antenna to send with. */
static int
iwl_mvm_rs_sw_inputs(struct iwl_mvm *mvm, struct ieee80211_link_sta *link_sta,
    enum nl80211_band band, u32 *rate_idx_mask, u8 *tx_ant, u8 *ant)
{
	/*
	 * mac80211's band-local index 0 is 6 Mbit/s off 2.4 GHz; the driver
	 * counts that rate as IWL_FIRST_OFDM_RATE.
	 */
	*rate_idx_mask = link_sta->supp_rates[band];
	if (band != NL80211_BAND_2GHZ)
		*rate_idx_mask <<= IWL_FIRST_OFDM_RATE;

	*tx_ant = iwl_mvm_get_valid_tx_ant(mvm);
	if (*tx_ant == 0)
		return (-EINVAL);
	*ant = first_antenna(*tx_ant);
	return (0);
}

/*
 * Fill the station's command from table and queue it, with a fresh retry
 * budget.  Callers hold mvm->mutex, as does every other writer of lq.
 */
static void
iwl_mvm_rs_sw_send_table(struct iwl_mvm *mvm, struct iwl_mvm_sta *mvmsta,
    const u32 table[LQ_MAX_RETRY_NUM], u8 ant, u8 tx_ant)
{
	struct iwl_lq_sta *lq_sta;
	struct iwl_lq_cmd *lq;
	int i, ret;

	lockdep_assert_held(&mvm->mutex);

	lq_sta = &mvmsta->deflink.lq_sta.rs_drv;
	lq = &lq_sta->lq;
	memset(lq, 0, sizeof(*lq));
	lq->sta_id = mvmsta->deflink.sta_id;
	lq->single_stream_ant_msk = ant;
	lq->dual_stream_ant_msk = num_of_ant(tx_ant) > 1 ? tx_ant : ant;
	lq->agg_time_limit = cpu_to_le16(IWL_MVM_RS_AGG_TIME_LIMIT);
	lq->agg_disable_start_th = IWL_MVM_RS_AGG_DISABLE_START_TH;
	lq->agg_frame_cnt_limit = lq_sta->pers.max_agg_bufsize;
	for (i = 0; i < LQ_MAX_RETRY_NUM; i++)
		lq->rs_table[i] = iwl_mvm_v3_rate_to_fw(table[i],
		    mvm->fw_rates_ver);
	lq_sta->pers.lq_valid = true;
	lq_sta->pers.lq_retries = 0;

	ret = iwl_mvm_send_lq_cmd(mvm, lq);
	lq_sta->pers.lq_error = ret;
	if (ret != 0) {
		IWL_ERR(mvm, "sta %d: LQ_CMD not queued: %d\n", lq->sta_id, ret);
		lq_sta->pers.next_retry = jiffies +
		    msecs_to_jiffies(IWL_MVM_RS_LQ_RETRY_MS);
		iwl_mvm_rs_wk_kick(mvm);
	}
}

/*
 * Build the station's legacy rate table from its negotiated rates and queue
 * it.  Starts a new request epoch.  Callers hold mvm->mutex.
 */
static void
iwl_mvm_rs_sw_rate_init(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
    struct ieee80211_sta *sta,
    struct ieee80211_bss_conf *link_conf, struct ieee80211_link_sta *link_sta,
    enum nl80211_band band)
{
	struct iwl_mvm_sta *mvmsta;
	struct iwl_lq_sta *lq_sta;
	u32 table[LQ_MAX_RETRY_NUM], rate_idx_mask;
	u8 tx_ant, ant;
	int ret;

	lockdep_assert_held(&mvm->mutex);

	mvmsta = iwl_mvm_sta_from_mac80211(sta);
	lq_sta = &mvmsta->deflink.lq_sta.rs_drv;
	lq_sta->pers.lq_valid = false;
	lq_sta->pers.lq_error = 0;
	lq_sta->pers.build_error = 0;
	lq_sta->pers.lq_retries = 0;
	spin_lock_bh(&lq_sta->pers.lock);
	iwl_mvm_rs_desired_reset(&lq_sta->pers.desired);
	spin_unlock_bh(&lq_sta->pers.lock);
	lq_sta->pers.next_build = jiffies;
	if (!iwlwifi_mod_params.sw_rate_control)
		return;

	rate_idx_mask = 0;
	tx_ant = 0;
	ret = iwl_mvm_rs_sw_inputs(mvm, link_sta, band, &rate_idx_mask, &tx_ant,
	    &ant);
	if (ret == 0)
		ret = iwl_mvm_rs_legacy_table(band == NL80211_BAND_2GHZ,
		    rate_idx_mask, ant, table);
	if (ret != 0) {
		lq_sta->pers.build_error = ret;
		IWL_ERR(mvm, "sta %d: no legacy rate table (rates %#x, ant %#x): %d\n",
		    mvmsta->deflink.sta_id, rate_idx_mask, tx_ant, ret);
		return;
	}
	iwl_mvm_rs_sw_send_table(mvm, mvmsta, table, ant, tx_ant);
}

/* net80211 chose a rate for the station.  Any context; takes no mutex. */
void
iwl_mvm_rs_sw_txrate_update(struct iwl_mvm *mvm, struct ieee80211_sta *sta,
    const struct lkpi_txrate *txrate)
{
	struct iwl_lq_sta *lq_sta;
	struct iwl_mvm_rs_request req;
	bool wanted;
	int idx;

	if (!iwlwifi_mod_params.sw_rate_control || iwl_mvm_has_tlc_offload(mvm))
		return;

	switch (txrate->type) {
	case LKPI_TXRATE_LEGACY:
		idx = iwl_mvm_rs_legacy_index(txrate->rate);
		if (idx < 0)
			return;
		req.type = IWL_MVM_RS_REQ_LEGACY;
		req.rate = idx;
		break;
	case LKPI_TXRATE_HT:
		if (txrate->rate > 7) {
			IWL_DEBUG_RATE(mvm, "MCS %u requested with one stream\n",
			    txrate->rate);
			return;
		}
		req.type = IWL_MVM_RS_REQ_HT;
		req.rate = txrate->rate;
		break;
	default:
		return;
	}

	lq_sta = &iwl_mvm_sta_from_mac80211(sta)->deflink.lq_sta.rs_drv;
	spin_lock_bh(&lq_sta->pers.lock);
	wanted = iwl_mvm_rs_desired_offer(&lq_sta->pers.desired, &req,
	    txrate->gen);
	spin_unlock_bh(&lq_sta->pers.lock);

	if (wanted)
		iwl_mvm_rs_wk_kick(mvm);
}

/*
 * Build and send the table for the station's newest request when one is
 * owed and its interval has passed.  Returns the jiffies at which the
 * station is due again, or 0.  *sent tells whether a command went out.
 */
static unsigned long
iwl_mvm_rs_sw_adapt(struct iwl_mvm *mvm, struct ieee80211_sta *sta,
    unsigned long now, bool *sent)
{
	struct iwl_mvm_sta *mvmsta;
	struct iwl_lq_sta *lq_sta;
	struct ieee80211_chanctx_conf *chanctx;
	struct iwl_mvm_rs_request req;
	enum nl80211_band band;
	u32 table[LQ_MAX_RETRY_NUM], rate_idx_mask, gen;
	u8 tx_ant, ant;
	bool owed;
	int ret;

	lockdep_assert_held(&mvm->mutex);

	*sent = false;
	mvmsta = iwl_mvm_sta_from_mac80211(sta);
	lq_sta = &mvmsta->deflink.lq_sta.rs_drv;
	spin_lock_bh(&lq_sta->pers.lock);
	owed = iwl_mvm_rs_desired_take(&lq_sta->pers.desired, &req, &gen);
	spin_unlock_bh(&lq_sta->pers.lock);
	if (!owed)
		return (0);

	/* Requests from before the station left AUTHORIZED are void. */
	if (!mvmsta->authorized) {
		spin_lock_bh(&lq_sta->pers.lock);
		iwl_mvm_rs_desired_consumed(&lq_sta->pers.desired, gen);
		spin_unlock_bh(&lq_sta->pers.lock);
		return (0);
	}
	if (time_before(now, lq_sta->pers.next_build))
		return (lq_sta->pers.next_build);

	rcu_read_lock();
	chanctx = rcu_dereference(mvmsta->vif->bss_conf.chanctx_conf);
	band = chanctx != NULL ? chanctx->def.chan->band : NUM_NL80211_BANDS;
	rcu_read_unlock();

	rate_idx_mask = 0;
	tx_ant = 0;
	ret = -ENODEV;
	if (band != NUM_NL80211_BANDS)
		ret = iwl_mvm_rs_sw_inputs(mvm, &sta->deflink, band,
		    &rate_idx_mask, &tx_ant, &ant);
	if (ret == 0) {
		if (req.type == IWL_MVM_RS_REQ_HT &&
		    sta->deflink.ht_cap.ht_supported)
			ret = iwl_mvm_rs_ht_table(req.rate,
			    sta->deflink.ht_cap.mcs.rx_mask[0],
			    band == NL80211_BAND_2GHZ, rate_idx_mask, ant, table);
		else if (req.type == IWL_MVM_RS_REQ_LEGACY)
			ret = iwl_mvm_rs_legacy_table_from(
			    band == NL80211_BAND_2GHZ, rate_idx_mask, req.rate,
			    ant, table);
		else
			ret = -EINVAL;
	}
	lq_sta->pers.build_error = ret;
	if (ret == 0) {
		iwl_mvm_rs_sw_send_table(mvm, mvmsta, table, ant, tx_ant);
		*sent = true;
	} else
		IWL_ERR_LIMIT(mvm, "sta %d: no table for request %u/%u: %d\n",
		    mvmsta->deflink.sta_id, req.type, req.rate, ret);

	spin_lock_bh(&lq_sta->pers.lock);
	iwl_mvm_rs_desired_consumed(&lq_sta->pers.desired, gen);
	spin_unlock_bh(&lq_sta->pers.lock);
	lq_sta->pers.next_build = now + msecs_to_jiffies(IWL_MVM_RS_ADAPT_MS);
	return (0);
}

/*
 * Under mvm->mutex, for every station in fw_id_to_mac_id: the table its
 * newest request asks for, then a resend of a table the command queue
 * refused, up to IWL_MVM_RS_LQ_RETRIES times.  Runs again when something is
 * still due.  A station whose budget is used up keeps its table until the
 * next rate init, request or aggregation setup sends one.
 */
void
iwl_mvm_rs_lq_wk(struct work_struct *wk)
{
	struct iwl_mvm *mvm;
	struct ieee80211_sta *sta;
	struct iwl_lq_sta *lq_sta;
	unsigned long now, due, next;
	bool pending, sent;
	int i;

	mvm = container_of(wk, struct iwl_mvm, rs_lq_wk.work);
	pending = false;
	next = 0;

	spin_lock_bh(&mvm->rs_wk_lock);
	mvm->rs_wk_kick = false;
	spin_unlock_bh(&mvm->rs_wk_lock);

	mutex_lock(&mvm->mutex);
	/* MAC stop clears the running bit before it cancels this work. */
	if (!test_bit(IWL_MVM_STATUS_FIRMWARE_RUNNING, &mvm->status)) {
		mutex_unlock(&mvm->mutex);
		return;
	}
	now = jiffies;
	for (i = 0; i < mvm->fw->ucode_capa.num_stations; i++) {
		sta = rcu_dereference_protected(mvm->fw_id_to_mac_id[i],
		    lockdep_is_held(&mvm->mutex));
		if (IS_ERR_OR_NULL(sta))
			continue;
		lq_sta = &iwl_mvm_sta_from_mac80211(sta)->deflink.lq_sta.rs_drv;

		due = iwl_mvm_rs_sw_adapt(mvm, sta, now, &sent);
		if (sent && lq_sta->pers.lq_error != 0)
			due = lq_sta->pers.next_retry;
		if (due != 0 && (!pending || time_before(due, next))) {
			next = due;
			pending = true;
		}
		if (sent || !lq_sta->pers.lq_valid || lq_sta->pers.lq_error == 0 ||
		    lq_sta->pers.lq_retries >= IWL_MVM_RS_LQ_RETRIES)
			continue;
		if (time_before(now, lq_sta->pers.next_retry)) {
			due = lq_sta->pers.next_retry;
			if (!pending || time_before(due, next)) {
				next = due;
				pending = true;
			}
			continue;
		}

		lq_sta->pers.lq_error = iwl_mvm_send_lq_cmd(mvm, &lq_sta->lq);
		switch (iwl_mvm_rs_lq_retry_next(lq_sta->pers.lq_error,
		    &lq_sta->pers.lq_retries)) {
		case 1:
			lq_sta->pers.next_retry = now +
			    msecs_to_jiffies(IWL_MVM_RS_LQ_RETRY_MS);
			due = lq_sta->pers.next_retry;
			if (!pending || time_before(due, next)) {
				next = due;
				pending = true;
			}
			break;
		case -1:
			IWL_ERR(mvm, "sta %d: LQ_CMD given up after %d resends: %d\n",
			    i, lq_sta->pers.lq_retries, lq_sta->pers.lq_error);
			break;
		}
	}
	mutex_unlock(&mvm->mutex);

	/* A kick that arrived during this run wins over the deadline. */
	spin_lock_bh(&mvm->rs_wk_lock);
	if (test_bit(IWL_MVM_STATUS_FIRMWARE_RUNNING, &mvm->status) &&
	    iwl_mvm_rs_wk_next(mvm->rs_wk_kick, pending, jiffies, next, &due))
		mod_delayed_work(system_wq, &mvm->rs_lq_wk, due);
	spin_unlock_bh(&mvm->rs_wk_lock);
}

void
iwl_mvm_rs_rate_init(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
    struct ieee80211_sta *sta,
    struct ieee80211_bss_conf *link_conf, struct ieee80211_link_sta *link_sta,
    enum nl80211_band band)
{
	if (iwl_mvm_has_tlc_offload(mvm))
		iwl_mvm_rs_fw_rate_init(mvm, vif, sta, link_conf, link_sta, band);
	else
		iwl_mvm_rs_sw_rate_init(mvm, vif, sta, link_conf, link_sta, band);
}

void
iwl_mvm_rs_tx_status(struct iwl_mvm *mvm, struct ieee80211_sta *sta, int tid,
    struct ieee80211_tx_info *ba_info, bool t)
{
	TODO();
}

void
rs_update_last_rssi(struct iwl_mvm *mvm __unused, struct iwl_mvm_sta *mvmsta,
    struct ieee80211_rx_status *rx_status)
{
	struct iwl_lq_sta *lq_sta;
	int i;

	if (mvmsta == NULL || rx_status == NULL)
		return;

	/*
	 * Assumption based on mvm/sta.h is that this should update
	 * mvmsta->lq_sta.rs_drv but so far we only saw a iwl_lq_cmd (lq)
	 * access in that struct so nowhere to put rssi information.
	 * So the only thing would be if this is required internally
	 * to functions in this file.
	 * The "FW" version accesses more fields.  We assume they
	 * are the same for now.
	 */

	lq_sta = &mvmsta->deflink.lq_sta.rs_drv;

	lq_sta->pers.last_rssi = S8_MIN;
	lq_sta->pers.chains = rx_status->chains;

	for (i = 0; i < nitems(lq_sta->pers.chain_signal); i++) {
		if ((rx_status->chains & BIT(i)) == 0)
			continue;

		lq_sta->pers.chain_signal[i] = rx_status->chain_signal[i];
		if (rx_status->chain_signal[i] > lq_sta->pers.last_rssi)
			lq_sta->pers.last_rssi = rx_status->chain_signal[i];
	}
}

int
rs_pretty_print_rate_v1(char *buf, int bufsz, const u32 rate)
{
	TODO();
	return (0);
}
