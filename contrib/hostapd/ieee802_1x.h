/* $FreeBSD: release/7.0.0/contrib/hostapd/ieee802_1x.h 172506 2007-10-10 16:59:15Z cvs2svn $ */

/*
 * hostapd / IEEE 802.1X Authenticator
 * Copyright (c) 2002-2005, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#ifndef IEEE802_1X_H
#define IEEE802_1X_H

/* draft-congdon-radius-8021x-20.txt */

struct ieee802_1x_eapol_key {
	u8 type;
	u16 key_length;
	u8 replay_counter[8]; /* does not repeat within the life of the keying
			       * material used to encrypt the Key field;
			       * 64-bit NTP timestamp MAY be used here */
	u8 key_iv[16]; /* cryptographically random number */
	u8 key_index; /* key flag in the most significant bit:
		       * 0 = broadcast (default key),
		       * 1 = unicast (key mapping key); key index is in the
		       * 7 least significant bits */
	u8 key_signature[16]; /* HMAC-MD5 message integrity check computed with
			       * MS-MPPE-Send-Key as the key */

	/* followed by key: if packet body length = 44 + key length, then the
	 * key field (of key_length bytes) contains the key in encrypted form;
	 * if packet body length = 44, key field is absent and key_length
	 * represents the number of least significant octets from
	 * MS-MPPE-Send-Key attribute to be used as the keying material;
	 * RC4 key used in encryption = Key-IV + MS-MPPE-Recv-Key */
} __attribute__ ((packed));


void ieee802_1x_receive(struct hostapd_data *hapd, const u8 *sa, const u8 *buf,
			size_t len);
void ieee802_1x_new_station(struct hostapd_data *hapd, struct sta_info *sta);
void ieee802_1x_free_station(struct sta_info *sta);

void ieee802_1x_request_identity(struct hostapd_data *hapd,
				 struct sta_info *sta);
void ieee802_1x_tx_canned_eap(struct hostapd_data *hapd, struct sta_info *sta,
			      int success);
void ieee802_1x_tx_req(struct hostapd_data *hapd, struct sta_info *sta);
void ieee802_1x_tx_key(struct hostapd_data *hapd, struct sta_info *sta);
void ieee802_1x_send_resp_to_server(struct hostapd_data *hapd,
				    struct sta_info *sta);
void ieee802_1x_abort_auth(struct hostapd_data *hapd, struct sta_info *sta);
void ieee802_1x_set_sta_authorized(struct hostapd_data *hapd,
				   struct sta_info *sta, int authorized);
void ieee802_1x_dump_state(FILE *f, const char *prefix, struct sta_info *sta);
int ieee802_1x_init(struct hostapd_data *hapd);
void ieee802_1x_deinit(struct hostapd_data *hapd);
int ieee802_1x_reconfig(struct hostapd_data *hapd,
			struct hostapd_config *oldconf,
			struct hostapd_bss_config *oldbss);
int ieee802_1x_tx_status(struct hostapd_data *hapd, struct sta_info *sta,
			 u8 *buf, size_t len, int ack);
u8 * ieee802_1x_get_identity(struct eapol_state_machine *sm, size_t *len);
u8 * ieee802_1x_get_radius_class(struct eapol_state_machine *sm, size_t *len,
				 int idx);
u8 * ieee802_1x_get_key_crypt(struct eapol_state_machine *sm, size_t *len);
void ieee802_1x_notify_port_enabled(struct eapol_state_machine *sm,
				    int enabled);
void ieee802_1x_notify_port_valid(struct eapol_state_machine *sm,
				  int valid);
void ieee802_1x_notify_pre_auth(struct eapol_state_machine *sm, int pre_auth);
int ieee802_1x_get_mib(struct hostapd_data *hapd, char *buf, size_t buflen);
int ieee802_1x_get_mib_sta(struct hostapd_data *hapd, struct sta_info *sta,
			   char *buf, size_t buflen);
void hostapd_get_ntp_timestamp(u8 *buf);
void ieee802_1x_finished(struct hostapd_data *hapd, struct sta_info *sta,
			 int success);
char *eap_type_text(u8 type);

struct radius_class_data;

void ieee802_1x_free_radius_class(struct radius_class_data *class);
int ieee802_1x_copy_radius_class(struct radius_class_data *dst,
				 struct radius_class_data *src);

#endif /* IEEE802_1X_H */
