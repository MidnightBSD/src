/*-
 * Copyright (c) 2009 Sam Leffler, Errno Consulting
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifdef __FreeBSD__
__FBSDID("$FreeBSD$");
#endif

/*
 * IEEE 802.11 send/recv action frame support.
 */

#include "opt_inet.h"
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h> 
 
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_action.h>
#include <net80211/ieee80211_mesh.h>

static int
send_inval(struct ieee80211_node *ni, int cat, int act, void *sa)
{
	return EINVAL;
}

static ieee80211_send_action_func *ba_send_action[8] = {
	send_inval, send_inval, send_inval, send_inval,
	send_inval, send_inval, send_inval, send_inval,
};
static ieee80211_send_action_func *ht_send_action[8] = {
	send_inval, send_inval, send_inval, send_inval,
	send_inval, send_inval, send_inval, send_inval,
};
static ieee80211_send_action_func *meshpl_send_action[8] = {
	send_inval, send_inval, send_inval, send_inval,
	send_inval, send_inval, send_inval, send_inval,
};
static ieee80211_send_action_func *meshlm_send_action[4] = {
	send_inval, send_inval, send_inval, send_inval,
};
static ieee80211_send_action_func *hwmp_send_action[8] = {
	send_inval, send_inval, send_inval, send_inval,
	send_inval, send_inval, send_inval, send_inval,
};
static ieee80211_send_action_func *vendor_send_action[8] = {
	send_inval, send_inval, send_inval, send_inval,
	send_inval, send_inval, send_inval, send_inval,
};

int
ieee80211_send_action_register(int cat, int act, ieee80211_send_action_func *f)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	switch (cat) {
	case IEEE80211_ACTION_CAT_BA:
		if (act >= N(ba_send_action))
			break;
		ba_send_action[act] = f;
		return 0;
	case IEEE80211_ACTION_CAT_HT:
		if (act >= N(ht_send_action))
			break;
		ht_send_action[act] = f;
		return 0;
	case IEEE80211_ACTION_CAT_MESHPEERING:
		if (act >= N(meshpl_send_action))
			break;
		meshpl_send_action[act] = f;
		return 0;
	case IEEE80211_ACTION_CAT_MESHLMETRIC:
		if (act >= N(meshlm_send_action))
			break;
		meshlm_send_action[act] = f;
		return 0;
	case IEEE80211_ACTION_CAT_MESHPATH:
		if (act >= N(hwmp_send_action))
			break;
		hwmp_send_action[act] = f;
		return 0;
	case IEEE80211_ACTION_CAT_VENDOR:
		if (act >= N(vendor_send_action))
			break;
		vendor_send_action[act] = f;
		return 0;
	}
	return EINVAL;
#undef N
}

void
ieee80211_send_action_unregister(int cat, int act)
{
	ieee80211_send_action_register(cat, act, send_inval);
}

int
ieee80211_send_action(struct ieee80211_node *ni, int cat, int act, void *sa)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	ieee80211_send_action_func *f = send_inval;

	switch (cat) {
	case IEEE80211_ACTION_CAT_BA:
		if (act < N(ba_send_action))
			f = ba_send_action[act];
		break;
	case IEEE80211_ACTION_CAT_HT:
		if (act < N(ht_send_action))
			f = ht_send_action[act];
		break;
	case IEEE80211_ACTION_CAT_MESHPEERING:
		if (act < N(meshpl_send_action))
			f = meshpl_send_action[act];
		break;
	case IEEE80211_ACTION_CAT_MESHLMETRIC:
		if (act < N(meshlm_send_action))
			f = meshlm_send_action[act];
		break;
	case IEEE80211_ACTION_CAT_MESHPATH:
		if (act < N(hwmp_send_action))
			f = hwmp_send_action[act];
		break;
	case IEEE80211_ACTION_CAT_VENDOR:
		if (act < N(vendor_send_action))
			f = vendor_send_action[act];
		break;
	}
	return f(ni, cat, act, sa);
#undef N
}

static int
recv_inval(struct ieee80211_node *ni, const struct ieee80211_frame *wh,
	const uint8_t *frm, const uint8_t *efrm)
{
	return EINVAL;
}

static ieee80211_recv_action_func *ba_recv_action[8] = {
	recv_inval, recv_inval, recv_inval, recv_inval,
	recv_inval, recv_inval, recv_inval, recv_inval,
};
static ieee80211_recv_action_func *ht_recv_action[8] = {
	recv_inval, recv_inval, recv_inval, recv_inval,
	recv_inval, recv_inval, recv_inval, recv_inval,
};
static ieee80211_recv_action_func *meshpl_recv_action[8] = {
	recv_inval, recv_inval, recv_inval, recv_inval,
	recv_inval, recv_inval, recv_inval, recv_inval,
};
static ieee80211_recv_action_func *meshlm_recv_action[4] = {
	recv_inval, recv_inval, recv_inval, recv_inval,
};
static ieee80211_recv_action_func *hwmp_recv_action[8] = {
	recv_inval, recv_inval, recv_inval, recv_inval,
	recv_inval, recv_inval, recv_inval, recv_inval,
};
static ieee80211_recv_action_func *vendor_recv_action[8] = {
	recv_inval, recv_inval, recv_inval, recv_inval,
	recv_inval, recv_inval, recv_inval, recv_inval,
};

int
ieee80211_recv_action_register(int cat, int act, ieee80211_recv_action_func *f)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	switch (cat) {
	case IEEE80211_ACTION_CAT_BA:
		if (act >= N(ba_recv_action))
			break;
		ba_recv_action[act] = f;
		return 0;
	case IEEE80211_ACTION_CAT_HT:
		if (act >= N(ht_recv_action))
			break;
		ht_recv_action[act] = f;
		return 0;
	case IEEE80211_ACTION_CAT_MESHPEERING:
		if (act >= N(meshpl_recv_action))
			break;
		meshpl_recv_action[act] = f;
		return 0;
	case IEEE80211_ACTION_CAT_MESHLMETRIC:
		if (act >= N(meshlm_recv_action))
			break;
		meshlm_recv_action[act] = f;
		return 0;
	case IEEE80211_ACTION_CAT_MESHPATH:
		if (act >= N(hwmp_recv_action))
			break;
		hwmp_recv_action[act] = f;
		return 0;
	case IEEE80211_ACTION_CAT_VENDOR:
		if (act >= N(vendor_recv_action))
			break;
		vendor_recv_action[act] = f;
		return 0;
	}
	return EINVAL;
#undef N
}

void
ieee80211_recv_action_unregister(int cat, int act)
{
	ieee80211_recv_action_register(cat, act, recv_inval);
}

int
ieee80211_recv_action(struct ieee80211_node *ni,
	const struct ieee80211_frame *wh,
	const uint8_t *frm, const uint8_t *efrm)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	ieee80211_recv_action_func *f = recv_inval;
	const struct ieee80211_action *ia =
	    (const struct ieee80211_action *) frm;

	switch (ia->ia_category) {
	case IEEE80211_ACTION_CAT_BA:
		if (ia->ia_action < N(ba_recv_action))
			f = ba_recv_action[ia->ia_action];
		break;
	case IEEE80211_ACTION_CAT_HT:
		if (ia->ia_action < N(ht_recv_action))
			f = ht_recv_action[ia->ia_action];
		break;
	case IEEE80211_ACTION_CAT_MESHPEERING:
		if (ia->ia_action < N(meshpl_recv_action))
			f = meshpl_recv_action[ia->ia_action];
		break;
	case IEEE80211_ACTION_CAT_MESHLMETRIC:
		if (ia->ia_action < N(meshlm_recv_action))
			f = meshlm_recv_action[ia->ia_action];
		break;
	case IEEE80211_ACTION_CAT_MESHPATH:
		if (ia->ia_action < N(hwmp_recv_action))
			f = hwmp_recv_action[ia->ia_action];
		break;
	case IEEE80211_ACTION_CAT_VENDOR:
		if (ia->ia_action < N(vendor_recv_action))
			f = vendor_recv_action[ia->ia_action];
		break;
	}
	return f(ni, wh, frm, efrm);
#undef N
}
