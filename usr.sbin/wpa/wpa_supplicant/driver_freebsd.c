/*
 * WPA Supplicant - driver interaction with BSD net80211 layer
 * Copyright (c) 2004, Sam Leffler <sam@errno.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 *
 * $FreeBSD: release/7.0.0/usr.sbin/wpa/wpa_supplicant/driver_freebsd.c 173434 2007-11-08 05:52:24Z thompsa $
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "common.h"
#include "driver.h"
#include "eloop.h"
#include "wpa_supplicant.h"
#include "l2_packet.h"
#include "wpa.h"			/* XXX for RSN_INFO_ELEM */

#include <sys/socket.h>
#include <net/if.h>
#include <net/ethernet.h>

#include <net80211/ieee80211.h>
#include <net80211/ieee80211_crypto.h>
#include <net80211/ieee80211_ioctl.h>

struct wpa_driver_bsd_data {
	int	sock;			/* open socket for 802.11 ioctls */
	int	route;			/* routing socket for events */
	char	ifname[IFNAMSIZ+1];	/* interface name */
	unsigned int ifindex;		/* interface index */
	void	*ctx;
	int	prev_roaming;		/* roaming state to restore on deinit */
	int	prev_privacy;		/* privacy state to restore on deinit */
	int	prev_wpa;		/* wpa state to restore on deinit */
};

static int
set80211var(struct wpa_driver_bsd_data *drv, int op, const void *arg, int arg_len)
{
	struct ieee80211req ireq;

	memset(&ireq, 0, sizeof(ireq));
	strncpy(ireq.i_name, drv->ifname, IFNAMSIZ);
	ireq.i_type = op;
	ireq.i_len = arg_len;
	ireq.i_data = (void *) arg;

	if (ioctl(drv->sock, SIOCS80211, &ireq) < 0) {
		fprintf(stderr, "ioctl[SIOCS80211, op %u, len %u]: %s\n",
			op, arg_len, strerror(errno));
		return -1;
	}
	return 0;
}

static int
get80211var(struct wpa_driver_bsd_data *drv, int op, void *arg, int arg_len)
{
	struct ieee80211req ireq;

	memset(&ireq, 0, sizeof(ireq));
	strncpy(ireq.i_name, drv->ifname, IFNAMSIZ);
	ireq.i_type = op;
	ireq.i_len = arg_len;
	ireq.i_data = arg;

	if (ioctl(drv->sock, SIOCG80211, &ireq) < 0) {
		fprintf(stderr, "ioctl[SIOCG80211, op %u, len %u]: %s\n",
			op, arg_len, strerror(errno));
		return -1;
	}
	return ireq.i_len;
}

static int
set80211param(struct wpa_driver_bsd_data *drv, int op, int arg)
{
	struct ieee80211req ireq;

	memset(&ireq, 0, sizeof(ireq));
	strncpy(ireq.i_name, drv->ifname, IFNAMSIZ);
	ireq.i_type = op;
	ireq.i_val = arg;

	if (ioctl(drv->sock, SIOCS80211, &ireq) < 0) {
		fprintf(stderr, "ioctl[SIOCS80211, op %u, arg 0x%x]: %s\n",
			op, arg, strerror(errno));
		return -1;
	}
	return 0;
}

static int
get80211param(struct wpa_driver_bsd_data *drv, int op)
{
	struct ieee80211req ireq;

	memset(&ireq, 0, sizeof(ireq));
	strncpy(ireq.i_name, drv->ifname, IFNAMSIZ);
	ireq.i_type = op;

	if (ioctl(drv->sock, SIOCG80211, &ireq) < 0) {
		fprintf(stderr, "ioctl[SIOCG80211, op %u]: %s\n",
			op, strerror(errno));
		return -1;
	}
	return ireq.i_val;
}

static int
getifflags(struct wpa_driver_bsd_data *drv, int *flags)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, drv->ifname, sizeof (ifr.ifr_name));
	if (ioctl(drv->sock, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		perror("SIOCGIFFLAGS");
		return errno;
	}
	*flags = ifr.ifr_flags & 0xffff;
	return 0;
}

static int
setifflags(struct wpa_driver_bsd_data *drv, int flags)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, drv->ifname, sizeof (ifr.ifr_name));
	ifr.ifr_flags = flags & 0xffff;
	if (ioctl(drv->sock, SIOCSIFFLAGS, (caddr_t)&ifr) < 0) {
		perror("SIOCSIFFLAGS");
		return errno;
	}
	return 0;
}

static int
wpa_driver_bsd_get_bssid(void *priv, u8 *bssid)
{
	struct wpa_driver_bsd_data *drv = priv;

	return get80211var(drv, IEEE80211_IOC_BSSID,
		bssid, IEEE80211_ADDR_LEN) < 0 ? -1 : 0;
}

#if 0
static int
wpa_driver_bsd_set_bssid(void *priv, const char *bssid)
{
	struct wpa_driver_bsd_data *drv = priv;

	return set80211var(drv, IEEE80211_IOC_BSSID,
		bssid, IEEE80211_ADDR_LEN);
}
#endif

static int
wpa_driver_bsd_get_ssid(void *priv, u8 *ssid)
{
	struct wpa_driver_bsd_data *drv = priv;

	return get80211var(drv, IEEE80211_IOC_SSID,
		ssid, IEEE80211_NWID_LEN);
}

static int
wpa_driver_bsd_set_ssid(void *priv, const char *ssid,
			     size_t ssid_len)
{
	struct wpa_driver_bsd_data *drv = priv;

	return set80211var(drv, IEEE80211_IOC_SSID, ssid, ssid_len);
}

static int
wpa_driver_bsd_set_wpa_ie(struct wpa_driver_bsd_data *drv,
	const char *wpa_ie, size_t wpa_ie_len)
{
	return set80211var(drv, IEEE80211_IOC_OPTIE, wpa_ie, wpa_ie_len);
}

static int
wpa_driver_bsd_set_wpa_internal(void *priv, int wpa, int privacy)
{
	struct wpa_driver_bsd_data *drv = priv;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "%s: wpa=%d privacy=%d",
		__FUNCTION__, wpa, privacy);

	if (!wpa && wpa_driver_bsd_set_wpa_ie(drv, NULL, 0) < 0)
		ret = -1;
	if (set80211param(drv, IEEE80211_IOC_PRIVACY, privacy) < 0)
		ret = -1;
	if (set80211param(drv, IEEE80211_IOC_WPA, wpa) < 0)
		ret = -1;

	return ret;
}

static int
wpa_driver_bsd_set_wpa(void *priv, int enabled)
{
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);

	return wpa_driver_bsd_set_wpa_internal(priv, enabled ? 3 : 0, enabled);
}

static int
wpa_driver_bsd_del_key(struct wpa_driver_bsd_data *drv, int key_idx,
		       const unsigned char *addr)
{
	struct ieee80211req_del_key wk;

	memset(&wk, 0, sizeof(wk));
	if (addr != NULL &&
	    bcmp(addr, "\xff\xff\xff\xff\xff\xff", IEEE80211_ADDR_LEN) != 0) {
		struct ether_addr ea;

		memcpy(&ea, addr, IEEE80211_ADDR_LEN);
		wpa_printf(MSG_DEBUG, "%s: addr=%s keyidx=%d",
			__func__, ether_ntoa(&ea), key_idx);
		memcpy(wk.idk_macaddr, addr, IEEE80211_ADDR_LEN);
		wk.idk_keyix = (uint8_t) IEEE80211_KEYIX_NONE;
	} else {
		wpa_printf(MSG_DEBUG, "%s: keyidx=%d", __func__, key_idx);
		wk.idk_keyix = key_idx;
	}
	return set80211var(drv, IEEE80211_IOC_DELKEY, &wk, sizeof(wk));
}

static int
wpa_driver_bsd_set_key(void *priv, wpa_alg alg,
		       const unsigned char *addr, int key_idx, int set_tx,
		       const u8 *seq, size_t seq_len,
		       const u8 *key, size_t key_len)
{
	struct wpa_driver_bsd_data *drv = priv;
	struct ieee80211req_key wk;
	struct ether_addr ea;
	char *alg_name;
	u_int8_t cipher;

	if (alg == WPA_ALG_NONE)
		return wpa_driver_bsd_del_key(drv, key_idx, addr);

	switch (alg) {
	case WPA_ALG_WEP:
		alg_name = "WEP";
		cipher = IEEE80211_CIPHER_WEP;
		break;
	case WPA_ALG_TKIP:
		alg_name = "TKIP";
		cipher = IEEE80211_CIPHER_TKIP;
		break;
	case WPA_ALG_CCMP:
		alg_name = "CCMP";
		cipher = IEEE80211_CIPHER_AES_CCM;
		break;
	default:
		wpa_printf(MSG_DEBUG, "%s: unknown/unsupported algorithm %d",
			__func__, alg);
		return -1;
	}

	memcpy(&ea, addr, IEEE80211_ADDR_LEN);
	wpa_printf(MSG_DEBUG,
		"%s: alg=%s addr=%s key_idx=%d set_tx=%d seq_len=%zu key_len=%zu",
		__func__, alg_name, ether_ntoa(&ea), key_idx, set_tx,
		seq_len, key_len);

	if (seq_len > sizeof(u_int64_t)) {
		wpa_printf(MSG_DEBUG, "%s: seq_len %zu too big",
			__func__, seq_len);
		return -2;
	}
	if (key_len > sizeof(wk.ik_keydata)) {
		wpa_printf(MSG_DEBUG, "%s: key length %zu too big",
			__func__, key_len);
		return -3;
	}

	memset(&wk, 0, sizeof(wk));
	wk.ik_type = cipher;
	wk.ik_flags = IEEE80211_KEY_RECV;
	if (set_tx)
		wk.ik_flags |= IEEE80211_KEY_XMIT;
	memcpy(wk.ik_macaddr, addr, IEEE80211_ADDR_LEN);
	/*
	 * Deduce whether group/global or unicast key by checking
	 * the address (yech).  Note also that we can only mark global
	 * keys default; doing this for a unicast key is an error.
	 */
	if (bcmp(addr, "\xff\xff\xff\xff\xff\xff", IEEE80211_ADDR_LEN) == 0) {
		wk.ik_flags |= IEEE80211_KEY_GROUP;
		wk.ik_keyix = key_idx;
	} else {
		wk.ik_keyix = (key_idx == 0 ? IEEE80211_KEYIX_NONE : key_idx);
	}
	if (wk.ik_keyix != IEEE80211_KEYIX_NONE && set_tx)
		wk.ik_flags |= IEEE80211_KEY_DEFAULT;
	wk.ik_keylen = key_len;
	memcpy(&wk.ik_keyrsc, seq, seq_len);
	wk.ik_keyrsc = le64toh(wk.ik_keyrsc);
	memcpy(wk.ik_keydata, key, key_len);

	return set80211var(drv, IEEE80211_IOC_WPAKEY, &wk, sizeof(wk));
}

static int
wpa_driver_bsd_set_countermeasures(void *priv, int enabled)
{
	struct wpa_driver_bsd_data *drv = priv;

	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, enabled);
	return set80211param(drv, IEEE80211_IOC_COUNTERMEASURES, enabled);
}


static int
wpa_driver_bsd_set_drop_unencrypted(void *priv, int enabled)
{
	struct wpa_driver_bsd_data *drv = priv;

	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, enabled);
	return set80211param(drv, IEEE80211_IOC_DROPUNENCRYPTED, enabled);
}

static int
wpa_driver_bsd_deauthenticate(void *priv, const u8 *addr, int reason_code)
{
	struct wpa_driver_bsd_data *drv = priv;
	struct ieee80211req_mlme mlme;

	wpa_printf(MSG_DEBUG, "%s", __func__);
	memset(&mlme, 0, sizeof(mlme));
	mlme.im_op = IEEE80211_MLME_DEAUTH;
	mlme.im_reason = reason_code;
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	return set80211var(drv, IEEE80211_IOC_MLME, &mlme, sizeof(mlme));
}

static int
wpa_driver_bsd_disassociate(void *priv, const u8 *addr, int reason_code)
{
	struct wpa_driver_bsd_data *drv = priv;
	struct ieee80211req_mlme mlme;

	wpa_printf(MSG_DEBUG, "%s", __func__);
	memset(&mlme, 0, sizeof(mlme));
	mlme.im_op = IEEE80211_MLME_DISASSOC;
	mlme.im_reason = reason_code;
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	return set80211var(drv, IEEE80211_IOC_MLME, &mlme, sizeof(mlme));
}

static int
wpa_driver_bsd_associate(void *priv, struct wpa_driver_associate_params *params)
{
	struct wpa_driver_bsd_data *drv = priv;
	struct ieee80211req_mlme mlme;
	int privacy;

	wpa_printf(MSG_DEBUG,
		"%s: ssid '%.*s' wpa ie len %u pairwise %u group %u key mgmt %u"
		, __func__
		, params->ssid_len, params->ssid
		, params->wpa_ie_len
		, params->pairwise_suite
		, params->group_suite
		, params->key_mgmt_suite
	);

	/* XXX error handling is wrong but unclear what to do... */
	if (wpa_driver_bsd_set_wpa_ie(drv, params->wpa_ie, params->wpa_ie_len) < 0)
		return -1;

	privacy = !(params->pairwise_suite == CIPHER_NONE &&
	    params->group_suite == CIPHER_NONE &&
	    params->key_mgmt_suite == KEY_MGMT_NONE &&
	    params->wpa_ie_len == 0);
	wpa_printf(MSG_DEBUG, "%s: set PRIVACY %u", __func__, privacy);

	if (set80211param(drv, IEEE80211_IOC_PRIVACY, privacy) < 0)
		return -1;

	if (params->wpa_ie_len &&
	    set80211param(drv, IEEE80211_IOC_WPA,
			  params->wpa_ie[0] == RSN_INFO_ELEM ? 2 : 1) < 0)
		return -1;

	memset(&mlme, 0, sizeof(mlme));
	mlme.im_op = IEEE80211_MLME_ASSOC;
	if (params->ssid != NULL)
		memcpy(mlme.im_ssid, params->ssid, params->ssid_len);
	mlme.im_ssid_len = params->ssid_len;
	if (params->bssid != NULL)
		memcpy(mlme.im_macaddr, params->bssid, IEEE80211_ADDR_LEN);
	if (set80211var(drv, IEEE80211_IOC_MLME, &mlme, sizeof(mlme)) < 0)
		return -1;
	return 0;
}

static int
wpa_driver_bsd_set_auth_alg(void *priv, int auth_alg)
{
	struct wpa_driver_bsd_data *drv = priv;
	int authmode;

	if ((auth_alg & AUTH_ALG_OPEN_SYSTEM) &&
	    (auth_alg & AUTH_ALG_SHARED_KEY))
		authmode = IEEE80211_AUTH_AUTO;
	else if (auth_alg & AUTH_ALG_SHARED_KEY)
		authmode = IEEE80211_AUTH_SHARED;
	else
		authmode = IEEE80211_AUTH_OPEN;

	wpa_printf(MSG_DEBUG, "%s alg 0x%x authmode %u",
		__func__, auth_alg, authmode);

	return set80211param(drv, IEEE80211_IOC_AUTHMODE, authmode);
}

static int
wpa_driver_bsd_scan(void *priv, const u8 *ssid, size_t ssid_len)
{
	struct wpa_driver_bsd_data *drv = priv;
	int flags;

	/* NB: interface must be marked UP to do a scan */
	if (getifflags(drv, &flags) != 0 || setifflags(drv, flags | IFF_UP) != 0)
		return -1;

	/* set desired ssid before scan */
	if (wpa_driver_bsd_set_ssid(drv, ssid, ssid_len) < 0)
		return -1;

	/* NB: net80211 delivers a scan complete event so no need to poll */
	return set80211param(drv, IEEE80211_IOC_SCAN_REQ, 0);
}

#include <net/route.h>
#include <net80211/ieee80211_freebsd.h>

static void
wpa_driver_bsd_event_receive(int sock, void *ctx, void *sock_ctx)
{
	struct wpa_driver_bsd_data *drv = sock_ctx;
	char buf[2048];
	struct if_announcemsghdr *ifan;
	struct if_msghdr *ifm;
	struct rt_msghdr *rtm;
	union wpa_event_data event;
	struct ieee80211_michael_event *mic;
	int n;

	n = read(sock, buf, sizeof(buf));
	if (n < 0) {
		if (errno != EINTR && errno != EAGAIN)
			perror("read(PF_ROUTE)");
		return;
	}

	rtm = (struct rt_msghdr *) buf;
	if (rtm->rtm_version != RTM_VERSION) {
		wpa_printf(MSG_DEBUG, "Routing message version %d not "
			"understood\n", rtm->rtm_version);
		return;
	}
	memset(&event, 0, sizeof(event));
	switch (rtm->rtm_type) {
	case RTM_IFANNOUNCE:
		ifan = (struct if_announcemsghdr *) rtm;
		if (ifan->ifan_index != drv->ifindex)
			break;
		strlcpy(event.interface_status.ifname, drv->ifname,
			sizeof(event.interface_status.ifname));
		switch (ifan->ifan_what) {
		case IFAN_DEPARTURE:
			event.interface_status.ievent = EVENT_INTERFACE_REMOVED;
		default:
			return;
		}
		wpa_printf(MSG_DEBUG, "RTM_IFANNOUNCE: Interface '%s' %s",
			   event.interface_status.ifname,
			   ifan->ifan_what == IFAN_DEPARTURE ?
				"removed" : "added");
		wpa_supplicant_event(ctx, EVENT_INTERFACE_STATUS, &event);
		break;
	case RTM_IEEE80211:
		ifan = (struct if_announcemsghdr *) rtm;
		if (ifan->ifan_index != drv->ifindex)
			break;
		switch (ifan->ifan_what) {
		case RTM_IEEE80211_ASSOC:
		case RTM_IEEE80211_REASSOC:
			wpa_supplicant_event(ctx, EVENT_ASSOC, NULL);
			break;
		case RTM_IEEE80211_DISASSOC:
			wpa_supplicant_event(ctx, EVENT_DISASSOC, NULL);
			break;
		case RTM_IEEE80211_SCAN:
			wpa_supplicant_event(ctx, EVENT_SCAN_RESULTS, NULL);
			break;
		case RTM_IEEE80211_REPLAY:
			/* ignore */
			break;
		case RTM_IEEE80211_MICHAEL:
			mic = (struct ieee80211_michael_event *) &ifan[1];
			wpa_printf(MSG_DEBUG,
				"Michael MIC failure wireless event: "
				"keyix=%u src_addr=" MACSTR, mic->iev_keyix,
				MAC2STR(mic->iev_src));

			memset(&event, 0, sizeof(event));
			event.michael_mic_failure.unicast =
				!IEEE80211_IS_MULTICAST(mic->iev_dst);
			wpa_supplicant_event(ctx, EVENT_MICHAEL_MIC_FAILURE,
				&event);
			break;
		}
		break;
	case RTM_IFINFO:
		ifm = (struct if_msghdr *) rtm;
		if (ifm->ifm_index != drv->ifindex)
			break;
		if ((rtm->rtm_flags & RTF_UP) == 0) {
			strlcpy(event.interface_status.ifname, drv->ifname,
				sizeof(event.interface_status.ifname));
			event.interface_status.ievent = EVENT_INTERFACE_REMOVED;
			wpa_printf(MSG_DEBUG, "RTM_IFINFO: Interface '%s' DOWN",
				   event.interface_status.ifname);
			wpa_supplicant_event(ctx, EVENT_INTERFACE_STATUS, &event);
		}
		break;
	}
}

/* Compare function for sorting scan results. Return >0 if @b is consider
 * better. */
static int
wpa_scan_result_compar(const void *a, const void *b)
{
	const struct wpa_scan_result *wa = a;
	const struct wpa_scan_result *wb = b;

	/* WPA/WPA2 support preferred */
	if ((wb->wpa_ie_len || wb->rsn_ie_len) &&
	    !(wa->wpa_ie_len || wa->rsn_ie_len))
		return 1;
	if (!(wb->wpa_ie_len || wb->rsn_ie_len) &&
	    (wa->wpa_ie_len || wa->rsn_ie_len))
		return -1;

	/* privacy support preferred */
	if ((wa->caps & IEEE80211_CAPINFO_PRIVACY) &&
	    (wb->caps & IEEE80211_CAPINFO_PRIVACY) == 0)
		return 1;
	if ((wa->caps & IEEE80211_CAPINFO_PRIVACY) == 0 &&
	    (wb->caps & IEEE80211_CAPINFO_PRIVACY))
		return -1;

	/* best/max rate preferred if signal level close enough XXX */
	if (wa->maxrate != wb->maxrate && abs(wb->level - wa->level) < 5)
		return wb->maxrate - wa->maxrate;

	/* use freq for channel preference */

	/* all things being equal, use signal level */
	return wb->level - wa->level;
}

static int
getmaxrate(const uint8_t rates[15], uint8_t nrates)
{
	int i, maxrate = -1;

	for (i = 0; i < nrates; i++) {
		int rate = rates[i] & IEEE80211_RATE_VAL;
		if (rate > maxrate)
			rate = maxrate;
	}
	return maxrate;
}

/* unalligned little endian access */     
#define LE_READ_4(p)					\
	((u_int32_t)					\
	 ((((const u_int8_t *)(p))[0]      ) |		\
	  (((const u_int8_t *)(p))[1] <<  8) |		\
	  (((const u_int8_t *)(p))[2] << 16) |		\
	  (((const u_int8_t *)(p))[3] << 24)))

static int __inline
iswpaoui(const u_int8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((WPA_OUI_TYPE<<24)|WPA_OUI);
}

static int
wpa_driver_bsd_get_scan_results(void *priv,
				     struct wpa_scan_result *results,
				     size_t max_size)
{
#define	min(a,b)	((a)>(b)?(b):(a))
	struct wpa_driver_bsd_data *drv = priv;
	uint8_t buf[24*1024];
	const uint8_t *cp, *vp;
	const struct ieee80211req_scan_result *sr;
	struct wpa_scan_result *wsr;
	int len, ielen;

	memset(results, 0, max_size * sizeof(struct wpa_scan_result));

	len = get80211var(drv, IEEE80211_IOC_SCAN_RESULTS, buf, sizeof(buf));
	if (len < 0)
		return -1;
	cp = buf;
	wsr = results;
	while (len >= sizeof(struct ieee80211req_scan_result)) {
		sr = (const struct ieee80211req_scan_result *) cp;
		memcpy(wsr->bssid, sr->isr_bssid, IEEE80211_ADDR_LEN);
		wsr->ssid_len = sr->isr_ssid_len;
		wsr->freq = sr->isr_freq;
		wsr->noise = sr->isr_noise;
		wsr->qual = sr->isr_rssi;
		wsr->level = 0;		/* XXX? */
		wsr->caps = sr->isr_capinfo;
		wsr->maxrate = getmaxrate(sr->isr_rates, sr->isr_nrates);
		vp = ((u_int8_t *)sr) + sr->isr_ie_off;
		memcpy(wsr->ssid, vp, sr->isr_ssid_len);
		if (sr->isr_ie_len > 0) {
			vp += sr->isr_ssid_len;
			ielen = sr->isr_ie_len;
			while (ielen > 0) {
				switch (vp[0]) {
				case IEEE80211_ELEMID_VENDOR:
					if (!iswpaoui(vp))
						break;
					wsr->wpa_ie_len =
					    min(2+vp[1], SSID_MAX_WPA_IE_LEN);
					memcpy(wsr->wpa_ie, vp, wsr->wpa_ie_len);
					break;
				case IEEE80211_ELEMID_RSN:
					wsr->rsn_ie_len =
					    min(2+vp[1], SSID_MAX_WPA_IE_LEN);
					memcpy(wsr->rsn_ie, vp, wsr->rsn_ie_len);
					break;
				}
				ielen -= 2+vp[1];
				vp += 2+vp[1];
			}
		}

		cp += sr->isr_len, len -= sr->isr_len;
		wsr++;
	}
	qsort(results, wsr - results, sizeof(struct wpa_scan_result),
	      wpa_scan_result_compar);

	wpa_printf(MSG_DEBUG, "Received %d bytes of scan results (%d BSSes)",
		   len, wsr - results);

	return wsr - results;
#undef min
}

static void *
wpa_driver_bsd_init(void *ctx, const char *ifname)
{
#define	GETPARAM(drv, param, v) \
	(((v) = get80211param(drv, param)) != -1)
	struct wpa_driver_bsd_data *drv;
	int flags;

	drv = malloc(sizeof(*drv));
	if (drv == NULL)
		return NULL;
	memset(drv, 0, sizeof(*drv));
	/*
	 * NB: We require the interface name be mappable to an index.
	 *     This implies we do not support having wpa_supplicant
	 *     wait for an interface to appear.  This seems ok; that
	 *     doesn't belong here; it's really the job of devd.
	 */
	drv->ifindex = if_nametoindex(ifname);
	if (drv->ifindex == 0) {
		wpa_printf(MSG_DEBUG, "%s: interface %s does not exist",
			   __func__, ifname);
		goto fail1;
	}
	drv->sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->sock < 0)
		goto fail1;
	drv->ctx = ctx;
	strncpy(drv->ifname, ifname, sizeof(drv->ifname));

	/*
	 * Mark the interface as down to ensure wpa_supplicant has exclusive
	 * access to the net80211 state machine, do this before opening the
	 * route socket to avoid a false event that the interface disappeared.
	 */
	if (getifflags(drv, &flags) == 0)
		(void) setifflags(drv, flags &~ IFF_UP);

	drv->route = socket(PF_ROUTE, SOCK_RAW, 0);
	if (drv->route < 0)
		goto fail;
	eloop_register_read_sock(drv->route,
		wpa_driver_bsd_event_receive, ctx, drv);

	if (!GETPARAM(drv, IEEE80211_IOC_ROAMING, drv->prev_roaming)) {
		wpa_printf(MSG_DEBUG, "%s: failed to get roaming state: %s",
			__func__, strerror(errno));
		goto fail;
	}
	if (!GETPARAM(drv, IEEE80211_IOC_PRIVACY, drv->prev_privacy)) {
		wpa_printf(MSG_DEBUG, "%s: failed to get privacy state: %s",
			__func__, strerror(errno));
		goto fail;
	}
	if (!GETPARAM(drv, IEEE80211_IOC_WPA, drv->prev_wpa)) {
		wpa_printf(MSG_DEBUG, "%s: failed to get wpa state: %s",
			__func__, strerror(errno));
		goto fail;
	}
	if (set80211param(drv, IEEE80211_IOC_ROAMING, IEEE80211_ROAMING_MANUAL) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to set wpa_supplicant-based "
			   "roaming: %s", __func__, strerror(errno));
		goto fail;
	}

	if (set80211param(drv, IEEE80211_IOC_WPA, 1+2) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to enable WPA support %s",
			   __func__, strerror(errno));
		goto fail;
	}

	return drv;
fail:
	close(drv->sock);
fail1:
	free(drv);
	return NULL;
#undef GETPARAM
}

static void
wpa_driver_bsd_deinit(void *priv)
{
	struct wpa_driver_bsd_data *drv = priv;
	int flags;

	/* NB: mark interface down */
	if (getifflags(drv, &flags) == 0)
		(void) setifflags(drv, flags &~ IFF_UP);

	wpa_driver_bsd_set_wpa_internal(drv, drv->prev_wpa, drv->prev_privacy);
	if (set80211param(drv, IEEE80211_IOC_ROAMING, drv->prev_roaming) < 0)
		wpa_printf(MSG_DEBUG, "%s: failed to restore roaming state",
			__func__);

	(void) close(drv->route);		/* ioctl socket */
	(void) close(drv->sock);		/* event socket */
	free(drv);
}


struct wpa_driver_ops wpa_driver_bsd_ops = {
	.name			= "bsd",
	.desc			= "BSD 802.11 support (Atheros, etc.)",
	.init			= wpa_driver_bsd_init,
	.deinit			= wpa_driver_bsd_deinit,
	.get_bssid		= wpa_driver_bsd_get_bssid,
	.get_ssid		= wpa_driver_bsd_get_ssid,
	.set_wpa		= wpa_driver_bsd_set_wpa,
	.set_key		= wpa_driver_bsd_set_key,
	.set_countermeasures	= wpa_driver_bsd_set_countermeasures,
	.set_drop_unencrypted	= wpa_driver_bsd_set_drop_unencrypted,
	.scan			= wpa_driver_bsd_scan,
	.get_scan_results	= wpa_driver_bsd_get_scan_results,
	.deauthenticate		= wpa_driver_bsd_deauthenticate,
	.disassociate		= wpa_driver_bsd_disassociate,
	.associate		= wpa_driver_bsd_associate,
	.set_auth_alg		= wpa_driver_bsd_set_auth_alg,
};
