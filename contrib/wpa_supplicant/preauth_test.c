/*
 * WPA Supplicant - test code for pre-authentication
 * Copyright (c) 2003-2006, Jouni Malinen <jkmaline@cc.hut.fi>
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
 * IEEE 802.1X Supplicant test code (to be used in place of wpa_supplicant.c.
 * Not used in production version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <netinet/in.h>
#include <assert.h>
#include <arpa/inet.h>

#include "common.h"
#include "config.h"
#include "eapol_sm.h"
#include "eloop.h"
#include "wpa.h"
#include "eap.h"
#include "wpa_supplicant.h"
#include "wpa_supplicant_i.h"
#include "l2_packet.h"
#include "ctrl_iface.h"
#include "pcsc_funcs.h"
#include "preauth.h"


extern int wpa_debug_level;
extern int wpa_debug_show_keys;

struct wpa_driver_ops *wpa_supplicant_drivers[] = { };


struct preauth_test_data {
	int auth_timed_out;
};


static void _wpa_supplicant_req_scan(void *wpa_s, int sec, int usec)
{
	wpa_supplicant_req_scan(wpa_s, sec, usec);
}


static void _wpa_supplicant_disassociate(void *wpa_s, int reason_code)
{
	wpa_supplicant_disassociate(wpa_s, reason_code);
}


static void _wpa_supplicant_deauthenticate(void *wpa_s, int reason_code)
{
	wpa_supplicant_deauthenticate(wpa_s, reason_code);
}


static u8 * wpa_alloc_eapol(const struct wpa_supplicant *wpa_s, u8 type,
			    const void *data, u16 data_len,
			    size_t *msg_len, void **data_pos)
{
	struct ieee802_1x_hdr *hdr;

	*msg_len = sizeof(*hdr) + data_len;
	hdr = malloc(*msg_len);
	if (hdr == NULL)
		return NULL;

	hdr->version = wpa_s->conf->eapol_version;
	hdr->type = type;
	hdr->length = htons(data_len);

	if (data)
		memcpy(hdr + 1, data, data_len);
	else
		memset(hdr + 1, 0, data_len);

	if (data_pos)
		*data_pos = hdr + 1;

	return (u8 *) hdr;
}


static u8 * _wpa_alloc_eapol(void *wpa_s, u8 type,
			     const void *data, u16 data_len,
			     size_t *msg_len, void **data_pos)
{
	return wpa_alloc_eapol(wpa_s, type, data, data_len, msg_len, data_pos);
}


static void _wpa_supplicant_set_state(void *ctx, wpa_states state)
{
	struct wpa_supplicant *wpa_s = ctx;
	wpa_s->wpa_state = state;
}


static wpa_states _wpa_supplicant_get_state(void *ctx)
{
	struct wpa_supplicant *wpa_s = ctx;
	return wpa_s->wpa_state;
}


static int wpa_ether_send(void *wpa_s, const u8 *dest, u16 proto,
			  const u8 *buf, size_t len)
{
	printf("%s - not implemented\n", __func__);
	return -1;
}


static struct wpa_ssid * _wpa_supplicant_get_ssid(void *wpa_s)
{
	return wpa_supplicant_get_ssid(wpa_s);
}


static void _wpa_supplicant_cancel_auth_timeout(void *wpa_s)
{
	wpa_supplicant_cancel_auth_timeout(wpa_s);
}


static int wpa_supplicant_get_beacon_ie(void *wpa_s)
{
	printf("%s - not implemented\n", __func__);
	return -1;
}


void wpa_supplicant_scan(void *eloop_ctx, void *timeout_ctx)
{
	printf("%s - not implemented\n", __func__);
}


static int wpa_supplicant_get_bssid(void *wpa_s, u8 *bssid)
{
	printf("%s - not implemented\n", __func__);
	return -1;
}


static int wpa_supplicant_set_key(void *wpa_s, wpa_alg alg,
				  const u8 *addr, int key_idx, int set_tx,
				  const u8 *seq, size_t seq_len,
				  const u8 *key, size_t key_len)
{
	printf("%s - not implemented\n", __func__);
	return -1;
}


static int wpa_supplicant_add_pmkid(void *wpa_s,
				    const u8 *bssid, const u8 *pmkid)
{
	printf("%s - not implemented\n", __func__);
	return -1;
}


static int wpa_supplicant_remove_pmkid(void *wpa_s,
				       const u8 *bssid, const u8 *pmkid)
{
	printf("%s - not implemented\n", __func__);
	return -1;
}


static void wpa_supplicant_set_config_blob(void *ctx,
					   struct wpa_config_blob *blob)
{
	struct wpa_supplicant *wpa_s = ctx;
	wpa_config_set_blob(wpa_s->conf, blob);
}


static const struct wpa_config_blob *
wpa_supplicant_get_config_blob(void *ctx, const char *name)
{
	struct wpa_supplicant *wpa_s = ctx;
	return wpa_config_get_blob(wpa_s->conf, name);
}


static void test_eapol_clean(struct wpa_supplicant *wpa_s)
{
	rsn_preauth_deinit(wpa_s->wpa);
	pmksa_candidate_free(wpa_s->wpa);
	pmksa_cache_free(wpa_s->wpa);
	wpa_sm_deinit(wpa_s->wpa);
	scard_deinit(wpa_s->scard);
	wpa_supplicant_ctrl_iface_deinit(wpa_s);
	wpa_config_free(wpa_s->conf);
}


static void eapol_test_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct preauth_test_data *p = eloop_ctx;
	printf("EAPOL test timed out\n");
	p->auth_timed_out = 1;
	eloop_terminate();
}


static void eapol_test_poll(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	if (!rsn_preauth_in_progress(wpa_s->wpa))
		eloop_terminate();
	else {
		eloop_register_timeout(0, 100000, eapol_test_poll, eloop_ctx,
				       timeout_ctx);
	}
}


static struct wpa_driver_ops dummy_driver;


static void wpa_init_conf(struct wpa_supplicant *wpa_s, const char *ifname)
{
	struct l2_packet_data *l2;
	struct wpa_sm_ctx *ctx;

	memset(&dummy_driver, 0, sizeof(dummy_driver));
	wpa_s->driver = &dummy_driver;

	ctx = malloc(sizeof(*ctx));
	assert(ctx != NULL);

	memset(ctx, 0, sizeof(*ctx));
	ctx->ctx = wpa_s;
	ctx->set_state = _wpa_supplicant_set_state;
	ctx->get_state = _wpa_supplicant_get_state;
	ctx->req_scan = _wpa_supplicant_req_scan;
	ctx->deauthenticate = _wpa_supplicant_deauthenticate;
	ctx->disassociate = _wpa_supplicant_disassociate;
	ctx->set_key = wpa_supplicant_set_key;
	ctx->scan = wpa_supplicant_scan;
	ctx->get_ssid = _wpa_supplicant_get_ssid;
	ctx->get_bssid = wpa_supplicant_get_bssid;
	ctx->ether_send = wpa_ether_send;
	ctx->get_beacon_ie = wpa_supplicant_get_beacon_ie;
	ctx->alloc_eapol = _wpa_alloc_eapol;
	ctx->cancel_auth_timeout = _wpa_supplicant_cancel_auth_timeout;
	ctx->add_pmkid = wpa_supplicant_add_pmkid;
	ctx->remove_pmkid = wpa_supplicant_remove_pmkid;
	ctx->set_config_blob = wpa_supplicant_set_config_blob;
	ctx->get_config_blob = wpa_supplicant_get_config_blob;

	wpa_s->wpa = wpa_sm_init(ctx);
	assert(wpa_s->wpa != NULL);
	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_PROTO, WPA_PROTO_RSN);

	strncpy(wpa_s->ifname, ifname, sizeof(wpa_s->ifname));
	wpa_sm_set_ifname(wpa_s->wpa, wpa_s->ifname);

	l2 = l2_packet_init(wpa_s->ifname, NULL, ETH_P_RSN_PREAUTH, NULL,
			    NULL, 0);
	assert(l2 != NULL);
	if (l2_packet_get_own_addr(l2, wpa_s->own_addr)) {
		wpa_printf(MSG_WARNING, "Failed to get own L2 address\n");
		exit(-1);
	}
	l2_packet_deinit(l2);
	wpa_sm_set_own_addr(wpa_s->wpa, wpa_s->own_addr);
}


static void eapol_test_terminate(int sig, void *eloop_ctx,
				 void *signal_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	wpa_msg(wpa_s, MSG_INFO, "Signal %d received - terminating", sig);
	eloop_terminate();
}


int main(int argc, char *argv[])
{
	struct wpa_supplicant wpa_s;
	int ret = 1;
	u8 bssid[ETH_ALEN];
	struct preauth_test_data preauth_test;

	memset(&preauth_test, 0, sizeof(preauth_test));

	wpa_debug_level = 0;
	wpa_debug_show_keys = 1;

	if (argc != 4) {
		printf("usage: preauth_test <conf> <target MAC address> "
		       "<ifname>\n");
		return -1;
	}

	if (hwaddr_aton(argv[2], bssid)) {
		printf("Failed to parse target address '%s'.\n", argv[2]);
		return -1;
	}

	eloop_init(&wpa_s);

	memset(&wpa_s, 0, sizeof(wpa_s));
	wpa_s.conf = wpa_config_read(argv[1]);
	if (wpa_s.conf == NULL) {
		printf("Failed to parse configuration file '%s'.\n", argv[1]);
		return -1;
	}
	if (wpa_s.conf->ssid == NULL) {
		printf("No networks defined.\n");
		return -1;
	}

	wpa_init_conf(&wpa_s, argv[3]);
	if (wpa_supplicant_ctrl_iface_init(&wpa_s)) {
		printf("Failed to initialize control interface '%s'.\n"
		       "You may have another preauth_test process already "
		       "running or the file was\n"
		       "left by an unclean termination of preauth_test in "
		       "which case you will need\n"
		       "to manually remove this file before starting "
		       "preauth_test again.\n",
		       wpa_s.conf->ctrl_interface);
		return -1;
	}
	if (wpa_supplicant_scard_init(&wpa_s, wpa_s.conf->ssid))
		return -1;

	if (rsn_preauth_init(wpa_s.wpa, bssid, wpa_s.conf->ssid))
		return -1;

	eloop_register_timeout(30, 0, eapol_test_timeout, &preauth_test, NULL);
	eloop_register_timeout(0, 100000, eapol_test_poll, &wpa_s, NULL);
	eloop_register_signal(SIGINT, eapol_test_terminate, NULL);
	eloop_register_signal(SIGTERM, eapol_test_terminate, NULL);
	eloop_register_signal(SIGHUP, eapol_test_terminate, NULL);
	eloop_run();

	if (preauth_test.auth_timed_out)
		ret = -2;
	else {
		ret = pmksa_cache_get(wpa_s.wpa, bssid, NULL) ? 0 : -3;
	}

	test_eapol_clean(&wpa_s);

	eloop_destroy();

	return ret;
}
