/*
 * hostapd / RADIUS message processing
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

#include "includes.h"

#include "common.h"
#include "radius.h"
#include "md5.h"
#include "crypto.h"


struct radius_msg *radius_msg_new(u8 code, u8 identifier)
{
	struct radius_msg *msg;

	msg = os_malloc(sizeof(*msg));
	if (msg == NULL)
		return NULL;

	if (radius_msg_initialize(msg, RADIUS_DEFAULT_MSG_SIZE)) {
		os_free(msg);
		return NULL;
	}

	radius_msg_set_hdr(msg, code, identifier);

	return msg;
}


int radius_msg_initialize(struct radius_msg *msg, size_t init_len)
{
	if (msg == NULL || init_len < sizeof(struct radius_hdr))
		return -1;

	os_memset(msg, 0, sizeof(*msg));
	msg->buf = wpa_zalloc(init_len);
	if (msg->buf == NULL)
		return -1;

	msg->buf_size = init_len;
	msg->hdr = (struct radius_hdr *) msg->buf;
	msg->buf_used = sizeof(*msg->hdr);

	msg->attrs =
		os_malloc(RADIUS_DEFAULT_ATTR_COUNT * sizeof(*msg->attrs));
	if (msg->attrs == NULL) {
		os_free(msg->buf);
		msg->buf = NULL;
		msg->hdr = NULL;
		return -1;
	}

	msg->attr_size = RADIUS_DEFAULT_ATTR_COUNT;
	msg->attr_used = 0;

	return 0;
}


void radius_msg_set_hdr(struct radius_msg *msg, u8 code, u8 identifier)
{
	msg->hdr->code = code;
	msg->hdr->identifier = identifier;
}


void radius_msg_free(struct radius_msg *msg)
{
	if (msg->buf != NULL) {
		os_free(msg->buf);
		msg->buf = NULL;
		msg->hdr = NULL;
	}
	msg->buf_size = msg->buf_used = 0;

	if (msg->attrs != NULL) {
		os_free(msg->attrs);
		msg->attrs = NULL;
	}
	msg->attr_size = msg->attr_used = 0;
}


static const char *radius_code_string(u8 code)
{
	switch (code) {
	case RADIUS_CODE_ACCESS_REQUEST: return "Access-Request";
	case RADIUS_CODE_ACCESS_ACCEPT: return "Access-Accept";
	case RADIUS_CODE_ACCESS_REJECT: return "Access-Reject";
	case RADIUS_CODE_ACCOUNTING_REQUEST: return "Accounting-Request";
	case RADIUS_CODE_ACCOUNTING_RESPONSE: return "Accounting-Response";
	case RADIUS_CODE_ACCESS_CHALLENGE: return "Access-Challenge";
	case RADIUS_CODE_STATUS_SERVER: return "Status-Server";
	case RADIUS_CODE_STATUS_CLIENT: return "Status-Client";
	case RADIUS_CODE_RESERVED: return "Reserved";
	default: return "?Unknown?";
	}
}


struct radius_attr_type {
	u8 type;
	char *name;
	enum {
		RADIUS_ATTR_UNDIST, RADIUS_ATTR_TEXT, RADIUS_ATTR_IP,
		RADIUS_ATTR_HEXDUMP, RADIUS_ATTR_INT32, RADIUS_ATTR_IPV6
	} data_type;
};

static struct radius_attr_type radius_attrs[] =
{
	{ RADIUS_ATTR_USER_NAME, "User-Name", RADIUS_ATTR_TEXT },
	{ RADIUS_ATTR_USER_PASSWORD, "User-Password", RADIUS_ATTR_UNDIST },
	{ RADIUS_ATTR_NAS_IP_ADDRESS, "NAS-IP-Address", RADIUS_ATTR_IP },
	{ RADIUS_ATTR_NAS_PORT, "NAS-Port", RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_FRAMED_MTU, "Framed-MTU", RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_STATE, "State", RADIUS_ATTR_UNDIST },
	{ RADIUS_ATTR_CLASS, "Class", RADIUS_ATTR_UNDIST },
	{ RADIUS_ATTR_VENDOR_SPECIFIC, "Vendor-Specific", RADIUS_ATTR_UNDIST },
	{ RADIUS_ATTR_SESSION_TIMEOUT, "Session-Timeout", RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_IDLE_TIMEOUT, "Idle-Timeout", RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_TERMINATION_ACTION, "Termination-Action",
	  RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_CALLED_STATION_ID, "Called-Station-Id",
	  RADIUS_ATTR_TEXT },
	{ RADIUS_ATTR_CALLING_STATION_ID, "Calling-Station-Id",
	  RADIUS_ATTR_TEXT },
	{ RADIUS_ATTR_NAS_IDENTIFIER, "NAS-Identifier", RADIUS_ATTR_TEXT },
	{ RADIUS_ATTR_ACCT_STATUS_TYPE, "Acct-Status-Type",
	  RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_ACCT_DELAY_TIME, "Acct-Delay-Time", RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_ACCT_INPUT_OCTETS, "Acct-Input-Octets",
	  RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_ACCT_OUTPUT_OCTETS, "Acct-Output-Octets",
	  RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_ACCT_SESSION_ID, "Acct-Session-Id", RADIUS_ATTR_TEXT },
	{ RADIUS_ATTR_ACCT_AUTHENTIC, "Acct-Authentic", RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_ACCT_SESSION_TIME, "Acct-Session-Time",
	  RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_ACCT_INPUT_PACKETS, "Acct-Input-Packets",
	  RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_ACCT_OUTPUT_PACKETS, "Acct-Output-Packets",
	  RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_ACCT_TERMINATE_CAUSE, "Acct-Terminate-Cause",
	  RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_ACCT_MULTI_SESSION_ID, "Acct-Multi-Session-Id",
	  RADIUS_ATTR_TEXT },
	{ RADIUS_ATTR_ACCT_LINK_COUNT, "Acct-Link-Count", RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_ACCT_INPUT_GIGAWORDS, "Acct-Input-Gigawords", 
	  RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_ACCT_OUTPUT_GIGAWORDS, "Acct-Output-Gigawords",
	  RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_EVENT_TIMESTAMP, "Event-Timestamp",
	  RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_NAS_PORT_TYPE, "NAS-Port-Type", RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_TUNNEL_TYPE, "Tunnel-Type", RADIUS_ATTR_HEXDUMP },
	{ RADIUS_ATTR_TUNNEL_MEDIUM_TYPE, "Tunnel-Medium-Type",
	  RADIUS_ATTR_HEXDUMP },
	{ RADIUS_ATTR_CONNECT_INFO, "Connect-Info", RADIUS_ATTR_TEXT },
	{ RADIUS_ATTR_EAP_MESSAGE, "EAP-Message", RADIUS_ATTR_UNDIST },
	{ RADIUS_ATTR_MESSAGE_AUTHENTICATOR, "Message-Authenticator",
	  RADIUS_ATTR_UNDIST },
	{ RADIUS_ATTR_TUNNEL_PRIVATE_GROUP_ID, "Tunnel-Private-Group-Id",
	  RADIUS_ATTR_HEXDUMP },
	{ RADIUS_ATTR_ACCT_INTERIM_INTERVAL, "Acct-Interim-Interval",
	  RADIUS_ATTR_INT32 },
	{ RADIUS_ATTR_NAS_IPV6_ADDRESS, "NAS-IPv6-Address", RADIUS_ATTR_IPV6 },
};
#define RADIUS_ATTRS (sizeof(radius_attrs) / sizeof(radius_attrs[0]))


static struct radius_attr_type *radius_get_attr_type(u8 type)
{
	size_t i;

	for (i = 0; i < RADIUS_ATTRS; i++) {
		if (type == radius_attrs[i].type)
			return &radius_attrs[i];
	}

	return NULL;
}


static void print_char(char c)
{
	if (c >= 32 && c < 127)
		printf("%c", c);
	else
		printf("<%02x>", c);
}


static void radius_msg_dump_attr(struct radius_attr_hdr *hdr)
{
	struct radius_attr_type *attr;
	int i, len;
	unsigned char *pos;

	attr = radius_get_attr_type(hdr->type);

	printf("   Attribute %d (%s) length=%d\n",
	       hdr->type, attr ? attr->name : "?Unknown?", hdr->length);

	if (attr == NULL)
		return;

	len = hdr->length - sizeof(struct radius_attr_hdr);
	pos = (unsigned char *) (hdr + 1);

	switch (attr->data_type) {
	case RADIUS_ATTR_TEXT:
		printf("      Value: '");
		for (i = 0; i < len; i++)
			print_char(pos[i]);
		printf("'\n");
		break;

	case RADIUS_ATTR_IP:
		if (len == 4) {
			struct in_addr *addr = (struct in_addr *) pos;
			printf("      Value: %s\n", inet_ntoa(*addr));
		} else
			printf("      Invalid IP address length %d\n", len);
		break;

#ifdef CONFIG_IPV6
	case RADIUS_ATTR_IPV6:
		if (len == 16) {
			char buf[128];
			const char *atxt;
			struct in6_addr *addr = (struct in6_addr *) pos;
			atxt = inet_ntop(AF_INET6, addr, buf, sizeof(buf));
			printf("      Value: %s\n", atxt ? atxt : "?");
		} else
			printf("      Invalid IPv6 address length %d\n", len);
		break;
#endif /* CONFIG_IPV6 */

	case RADIUS_ATTR_HEXDUMP:
	case RADIUS_ATTR_UNDIST:
		printf("      Value:");
		for (i = 0; i < len; i++)
			printf(" %02x", pos[i]);
		printf("\n");
		break;

	case RADIUS_ATTR_INT32:
		if (len == 4)
			printf("      Value: %u\n", WPA_GET_BE32(pos));
		else
			printf("      Invalid INT32 length %d\n", len);
		break;

	default:
		break;
	}
}


void radius_msg_dump(struct radius_msg *msg)
{
	size_t i;

	printf("RADIUS message: code=%d (%s) identifier=%d length=%d\n",
	       msg->hdr->code, radius_code_string(msg->hdr->code),
	       msg->hdr->identifier, ntohs(msg->hdr->length));

	for (i = 0; i < msg->attr_used; i++) {
		radius_msg_dump_attr(msg->attrs[i]);
	}
}


int radius_msg_finish(struct radius_msg *msg, u8 *secret, size_t secret_len)
{
	if (secret) {
		u8 auth[MD5_MAC_LEN];
		struct radius_attr_hdr *attr;

		os_memset(auth, 0, MD5_MAC_LEN);
		attr = radius_msg_add_attr(msg,
					   RADIUS_ATTR_MESSAGE_AUTHENTICATOR,
					   auth, MD5_MAC_LEN);
		if (attr == NULL) {
			printf("WARNING: Could not add "
			       "Message-Authenticator\n");
			return -1;
		}
		msg->hdr->length = htons(msg->buf_used);
		hmac_md5(secret, secret_len, msg->buf, msg->buf_used,
			 (u8 *) (attr + 1));
	} else
		msg->hdr->length = htons(msg->buf_used);

	if (msg->buf_used > 0xffff) {
		printf("WARNING: too long RADIUS message (%lu)\n",
		       (unsigned long) msg->buf_used);
		return -1;
	}
	return 0;
}


int radius_msg_finish_srv(struct radius_msg *msg, const u8 *secret,
			  size_t secret_len, const u8 *req_authenticator)
{
	u8 auth[MD5_MAC_LEN];
	struct radius_attr_hdr *attr;
	const u8 *addr[4];
	size_t len[4];

	os_memset(auth, 0, MD5_MAC_LEN);
	attr = radius_msg_add_attr(msg, RADIUS_ATTR_MESSAGE_AUTHENTICATOR,
				   auth, MD5_MAC_LEN);
	if (attr == NULL) {
		printf("WARNING: Could not add Message-Authenticator\n");
		return -1;
	}
	msg->hdr->length = htons(msg->buf_used);
	os_memcpy(msg->hdr->authenticator, req_authenticator,
		  sizeof(msg->hdr->authenticator));
	hmac_md5(secret, secret_len, msg->buf, msg->buf_used,
		 (u8 *) (attr + 1));

	/* ResponseAuth = MD5(Code+ID+Length+RequestAuth+Attributes+Secret) */
	addr[0] = (u8 *) msg->hdr;
	len[0] = 1 + 1 + 2;
	addr[1] = req_authenticator;
	len[1] = MD5_MAC_LEN;
	addr[2] = (u8 *) (msg->hdr + 1);
	len[2] = msg->buf_used - sizeof(*msg->hdr);
	addr[3] = secret;
	len[3] = secret_len;
	md5_vector(4, addr, len, msg->hdr->authenticator);

	if (msg->buf_used > 0xffff) {
		printf("WARNING: too long RADIUS message (%lu)\n",
		       (unsigned long) msg->buf_used);
		return -1;
	}
	return 0;
}


void radius_msg_finish_acct(struct radius_msg *msg, u8 *secret,
			    size_t secret_len)
{
	const u8 *addr[2];
	size_t len[2];

	msg->hdr->length = htons(msg->buf_used);
	os_memset(msg->hdr->authenticator, 0, MD5_MAC_LEN);
	addr[0] = msg->buf;
	len[0] = msg->buf_used;
	addr[1] = secret;
	len[1] = secret_len;
	md5_vector(2, addr, len, msg->hdr->authenticator);

	if (msg->buf_used > 0xffff) {
		printf("WARNING: too long RADIUS messages (%lu)\n",
		       (unsigned long) msg->buf_used);
	}
}


static int radius_msg_add_attr_to_array(struct radius_msg *msg,
					struct radius_attr_hdr *attr)
{
	if (msg->attr_used >= msg->attr_size) {
		struct radius_attr_hdr **nattrs;
		int nlen = msg->attr_size * 2;

		nattrs = os_realloc(msg->attrs, nlen * sizeof(*msg->attrs));
		if (nattrs == NULL)
			return -1;

		msg->attrs = nattrs;
		msg->attr_size = nlen;
	}

	msg->attrs[msg->attr_used++] = attr;

	return 0;
}


struct radius_attr_hdr *radius_msg_add_attr(struct radius_msg *msg, u8 type,
					    const u8 *data, size_t data_len)
{
	size_t buf_needed;
	struct radius_attr_hdr *attr;

	if (data_len > RADIUS_MAX_ATTR_LEN) {
		printf("radius_msg_add_attr: too long attribute (%lu bytes)\n",
		       (unsigned long) data_len);
		return NULL;
	}

	buf_needed = msg->buf_used + sizeof(*attr) + data_len;

	if (msg->buf_size < buf_needed) {
		/* allocate more space for message buffer */
		unsigned char *nbuf;
		size_t i, nlen = msg->buf_size;
		int diff;

		while (nlen < buf_needed)
			nlen *= 2;
		nbuf = os_realloc(msg->buf, nlen);
		if (nbuf == NULL)
			return NULL;
		diff = nbuf - msg->buf;
		msg->buf = nbuf;
		msg->hdr = (struct radius_hdr *) msg->buf;
		/* adjust attr pointers to match with the new buffer */
		for (i = 0; i < msg->attr_used; i++)
			msg->attrs[i] = (struct radius_attr_hdr *)
				(((u8 *) msg->attrs[i]) + diff);
		os_memset(msg->buf + msg->buf_size, 0, nlen - msg->buf_size);
		msg->buf_size = nlen;
	}

	attr = (struct radius_attr_hdr *) (msg->buf + msg->buf_used);
	attr->type = type;
	attr->length = sizeof(*attr) + data_len;
	if (data_len > 0)
		os_memcpy(attr + 1, data, data_len);

	msg->buf_used += sizeof(*attr) + data_len;

	if (radius_msg_add_attr_to_array(msg, attr))
		return NULL;

	return attr;
}


struct radius_msg *radius_msg_parse(const u8 *data, size_t len)
{
	struct radius_msg *msg;
	struct radius_hdr *hdr;
	struct radius_attr_hdr *attr;
	size_t msg_len;
	unsigned char *pos, *end;

	if (data == NULL || len < sizeof(*hdr))
		return NULL;

	hdr = (struct radius_hdr *) data;

	msg_len = ntohs(hdr->length);
	if (msg_len < sizeof(*hdr) || msg_len > len) {
		printf("Invalid RADIUS message length\n");
		return NULL;
	}

	if (msg_len < len) {
		printf("Ignored %lu extra bytes after RADIUS message\n",
		       (unsigned long) len - msg_len);
	}

	msg = os_malloc(sizeof(*msg));
	if (msg == NULL)
		return NULL;

	if (radius_msg_initialize(msg, msg_len)) {
		os_free(msg);
		return NULL;
	}

	os_memcpy(msg->buf, data, msg_len);
	msg->buf_size = msg->buf_used = msg_len;

	/* parse attributes */
	pos = (unsigned char *) (msg->hdr + 1);
	end = msg->buf + msg->buf_used;
	while (pos < end) {
		if ((size_t) (end - pos) < sizeof(*attr))
			goto fail;

		attr = (struct radius_attr_hdr *) pos;

		if (pos + attr->length > end || attr->length < sizeof(*attr))
			goto fail;

		/* TODO: check that attr->length is suitable for attr->type */

		if (radius_msg_add_attr_to_array(msg, attr))
			goto fail;

		pos += attr->length;
	}

	return msg;

 fail:
	radius_msg_free(msg);
	os_free(msg);
	return NULL;
}


int radius_msg_add_eap(struct radius_msg *msg, const u8 *data, size_t data_len)
{
	const u8 *pos = data;
	size_t left = data_len;

	while (left > 0) {
		int len;
		if (left > RADIUS_MAX_ATTR_LEN)
			len = RADIUS_MAX_ATTR_LEN;
		else
			len = left;

		if (!radius_msg_add_attr(msg, RADIUS_ATTR_EAP_MESSAGE,
					 pos, len))
			return 0;

		pos += len;
		left -= len;
	}

	return 1;
}


u8 *radius_msg_get_eap(struct radius_msg *msg, size_t *eap_len)
{
	u8 *eap, *pos;
	size_t len, i;

	if (msg == NULL)
		return NULL;

	len = 0;
	for (i = 0; i < msg->attr_used; i++) {
		if (msg->attrs[i]->type == RADIUS_ATTR_EAP_MESSAGE)
			len += msg->attrs[i]->length -
				sizeof(struct radius_attr_hdr);
	}

	if (len == 0)
		return NULL;

	eap = os_malloc(len);
	if (eap == NULL)
		return NULL;

	pos = eap;
	for (i = 0; i < msg->attr_used; i++) {
		if (msg->attrs[i]->type == RADIUS_ATTR_EAP_MESSAGE) {
			struct radius_attr_hdr *attr = msg->attrs[i];
			int flen = attr->length - sizeof(*attr);
			os_memcpy(pos, attr + 1, flen);
			pos += flen;
		}
	}

	if (eap_len)
		*eap_len = len;

	return eap;
}


int radius_msg_verify_msg_auth(struct radius_msg *msg, const u8 *secret,
			       size_t secret_len, const u8 *req_auth)
{
	u8 auth[MD5_MAC_LEN], orig[MD5_MAC_LEN];
	u8 orig_authenticator[16];
	struct radius_attr_hdr *attr = NULL;
	size_t i;

	for (i = 0; i < msg->attr_used; i++) {
		if (msg->attrs[i]->type == RADIUS_ATTR_MESSAGE_AUTHENTICATOR) {
			if (attr != NULL) {
				printf("Multiple Message-Authenticator "
				       "attributes in RADIUS message\n");
				return 1;
			}
			attr = msg->attrs[i];
		}
	}

	if (attr == NULL) {
		printf("No Message-Authenticator attribute found\n");
		return 1;
	}

	os_memcpy(orig, attr + 1, MD5_MAC_LEN);
	os_memset(attr + 1, 0, MD5_MAC_LEN);
	if (req_auth) {
		os_memcpy(orig_authenticator, msg->hdr->authenticator,
			  sizeof(orig_authenticator));
		os_memcpy(msg->hdr->authenticator, req_auth,
			  sizeof(msg->hdr->authenticator));
	}
	hmac_md5(secret, secret_len, msg->buf, msg->buf_used, auth);
	os_memcpy(attr + 1, orig, MD5_MAC_LEN);
	if (req_auth) {
		os_memcpy(msg->hdr->authenticator, orig_authenticator,
			  sizeof(orig_authenticator));
	}

	if (os_memcmp(orig, auth, MD5_MAC_LEN) != 0) {
		printf("Invalid Message-Authenticator!\n");
		return 1;
	}

	return 0;
}


int radius_msg_verify(struct radius_msg *msg, const u8 *secret,
		      size_t secret_len, struct radius_msg *sent_msg, int auth)
{
	const u8 *addr[4];
	size_t len[4];
	u8 hash[MD5_MAC_LEN];

	if (sent_msg == NULL) {
		printf("No matching Access-Request message found\n");
		return 1;
	}

	if (auth &&
	    radius_msg_verify_msg_auth(msg, secret, secret_len,
				       sent_msg->hdr->authenticator)) {
		return 1;
	}

	/* ResponseAuth = MD5(Code+ID+Length+RequestAuth+Attributes+Secret) */
	addr[0] = (u8 *) msg->hdr;
	len[0] = 1 + 1 + 2;
	addr[1] = sent_msg->hdr->authenticator;
	len[1] = MD5_MAC_LEN;
	addr[2] = (u8 *) (msg->hdr + 1);
	len[2] = msg->buf_used - sizeof(*msg->hdr);
	addr[3] = secret;
	len[3] = secret_len;
	md5_vector(4, addr, len, hash);
	if (os_memcmp(hash, msg->hdr->authenticator, MD5_MAC_LEN) != 0) {
		printf("Response Authenticator invalid!\n");
		return 1;
	}

	return 0;
}


int radius_msg_copy_attr(struct radius_msg *dst, struct radius_msg *src,
			 u8 type)
{
	struct radius_attr_hdr *attr = NULL;
	size_t i;

	for (i = 0; i < src->attr_used; i++) {
		if (src->attrs[i]->type == type) {
			attr = src->attrs[i];
			break;
		}
	}

	if (attr == NULL)
		return 0;

	if (!radius_msg_add_attr(dst, type, (u8 *) (attr + 1),
				 attr->length - sizeof(*attr)))
		return -1;

	return 1;
}


/* Create Request Authenticator. The value should be unique over the lifetime
 * of the shared secret between authenticator and authentication server.
 * Use one-way MD5 hash calculated from current timestamp and some data given
 * by the caller. */
void radius_msg_make_authenticator(struct radius_msg *msg,
				   const u8 *data, size_t len)
{
	struct os_time tv;
	long int l;
	const u8 *addr[3];
	size_t elen[3];

	os_get_time(&tv);
	l = os_random();
	addr[0] = (u8 *) &tv;
	elen[0] = sizeof(tv);
	addr[1] = data;
	elen[1] = len;
	addr[2] = (u8 *) &l;
	elen[2] = sizeof(l);
	md5_vector(3, addr, elen, msg->hdr->authenticator);
}


/* Get Vendor-specific RADIUS Attribute from a parsed RADIUS message.
 * Returns the Attribute payload and sets alen to indicate the length of the
 * payload if a vendor attribute with subtype is found, otherwise returns NULL.
 * The returned payload is allocated with os_malloc() and caller must free it
 * by calling os_free().
 */
static u8 *radius_msg_get_vendor_attr(struct radius_msg *msg, u32 vendor,
				      u8 subtype, size_t *alen)
{
	u8 *data, *pos;
	size_t i, len;

	if (msg == NULL)
		return NULL;

	for (i = 0; i < msg->attr_used; i++) {
		struct radius_attr_hdr *attr = msg->attrs[i];
		size_t left;
		u32 vendor_id;
		struct radius_attr_vendor *vhdr;

		if (attr->type != RADIUS_ATTR_VENDOR_SPECIFIC)
			continue;

		left = attr->length - sizeof(*attr);
		if (left < 4)
			continue;

		pos = (u8 *) (attr + 1);

		os_memcpy(&vendor_id, pos, 4);
		pos += 4;
		left -= 4;

		if (ntohl(vendor_id) != vendor)
			continue;

		while (left >= sizeof(*vhdr)) {
			vhdr = (struct radius_attr_vendor *) pos;
			if (vhdr->vendor_length > left ||
			    vhdr->vendor_length < sizeof(*vhdr)) {
				left = 0;
				break;
			}
			if (vhdr->vendor_type != subtype) {
				pos += vhdr->vendor_length;
				left -= vhdr->vendor_length;
				continue;
			}

			len = vhdr->vendor_length - sizeof(*vhdr);
			data = os_malloc(len);
			if (data == NULL)
				return NULL;
			os_memcpy(data, pos + sizeof(*vhdr), len);
			if (alen)
				*alen = len;
			return data;
		}
	}

	return NULL;
}


static u8 * decrypt_ms_key(const u8 *key, size_t len,
			   const u8 *req_authenticator,
			   const u8 *secret, size_t secret_len, size_t *reslen)
{
	u8 *plain, *ppos, *res;
	const u8 *pos;
	size_t left, plen;
	u8 hash[MD5_MAC_LEN];
	int i, first = 1;
	const u8 *addr[3];
	size_t elen[3];

	/* key: 16-bit salt followed by encrypted key info */

	if (len < 2 + 16)
		return NULL;

	pos = key + 2;
	left = len - 2;
	if (left % 16) {
		printf("Invalid ms key len %lu\n", (unsigned long) left);
		return NULL;
	}

	plen = left;
	ppos = plain = os_malloc(plen);
	if (plain == NULL)
		return NULL;

	while (left > 0) {
		/* b(1) = MD5(Secret + Request-Authenticator + Salt)
		 * b(i) = MD5(Secret + c(i - 1)) for i > 1 */

		addr[0] = secret;
		elen[0] = secret_len;
		if (first) {
			addr[1] = req_authenticator;
			elen[1] = MD5_MAC_LEN;
			addr[2] = key;
			elen[2] = 2; /* Salt */
		} else {
			addr[1] = pos - MD5_MAC_LEN;
			elen[1] = MD5_MAC_LEN;
		}
		md5_vector(first ? 3 : 2, addr, elen, hash);
		first = 0;

		for (i = 0; i < MD5_MAC_LEN; i++)
			*ppos++ = *pos++ ^ hash[i];
		left -= MD5_MAC_LEN;
	}

	if (plain[0] > plen - 1) {
		printf("Failed to decrypt MPPE key\n");
		os_free(plain);
		return NULL;
	}

	res = os_malloc(plain[0]);
	if (res == NULL) {
		os_free(plain);
		return NULL;
	}
	os_memcpy(res, plain + 1, plain[0]);
	if (reslen)
		*reslen = plain[0];
	os_free(plain);
	return res;
}


static void encrypt_ms_key(const u8 *key, size_t key_len, u16 salt,
			   const u8 *req_authenticator,
			   const u8 *secret, size_t secret_len,
			   u8 *ebuf, size_t *elen)
{
	int i, len, first = 1;
	u8 hash[MD5_MAC_LEN], saltbuf[2], *pos;
	const u8 *addr[3];
	size_t _len[3];

	saltbuf[0] = salt >> 8;
	saltbuf[1] = salt;

	len = 1 + key_len;
	if (len & 0x0f) {
		len = (len & 0xf0) + 16;
	}
	os_memset(ebuf, 0, len);
	ebuf[0] = key_len;
	os_memcpy(ebuf + 1, key, key_len);

	*elen = len;

	pos = ebuf;
	while (len > 0) {
		/* b(1) = MD5(Secret + Request-Authenticator + Salt)
		 * b(i) = MD5(Secret + c(i - 1)) for i > 1 */
		addr[0] = secret;
		_len[0] = secret_len;
		if (first) {
			addr[1] = req_authenticator;
			_len[1] = MD5_MAC_LEN;
			addr[2] = saltbuf;
			_len[2] = sizeof(saltbuf);
		} else {
			addr[1] = pos - MD5_MAC_LEN;
			_len[1] = MD5_MAC_LEN;
		}
		md5_vector(first ? 3 : 2, addr, _len, hash);
		first = 0;

		for (i = 0; i < MD5_MAC_LEN; i++)
			*pos++ ^= hash[i];

		len -= MD5_MAC_LEN;
	}
}


struct radius_ms_mppe_keys *
radius_msg_get_ms_keys(struct radius_msg *msg, struct radius_msg *sent_msg,
		       u8 *secret, size_t secret_len)
{
	u8 *key;
	size_t keylen;
	struct radius_ms_mppe_keys *keys;

	if (msg == NULL || sent_msg == NULL)
		return NULL;

	keys = wpa_zalloc(sizeof(*keys));
	if (keys == NULL)
		return NULL;

	key = radius_msg_get_vendor_attr(msg, RADIUS_VENDOR_ID_MICROSOFT,
					 RADIUS_VENDOR_ATTR_MS_MPPE_SEND_KEY,
					 &keylen);
	if (key) {
		keys->send = decrypt_ms_key(key, keylen,
					    sent_msg->hdr->authenticator,
					    secret, secret_len,
					    &keys->send_len);
		os_free(key);
	}

	key = radius_msg_get_vendor_attr(msg, RADIUS_VENDOR_ID_MICROSOFT,
					 RADIUS_VENDOR_ATTR_MS_MPPE_RECV_KEY,
					 &keylen);
	if (key) {
		keys->recv = decrypt_ms_key(key, keylen,
					    sent_msg->hdr->authenticator,
					    secret, secret_len,
					    &keys->recv_len);
		os_free(key);
	}

	return keys;
}


struct radius_ms_mppe_keys *
radius_msg_get_cisco_keys(struct radius_msg *msg, struct radius_msg *sent_msg,
			  u8 *secret, size_t secret_len)
{
	u8 *key;
	size_t keylen;
	struct radius_ms_mppe_keys *keys;

	if (msg == NULL || sent_msg == NULL)
		return NULL;

	keys = wpa_zalloc(sizeof(*keys));
	if (keys == NULL)
		return NULL;

	key = radius_msg_get_vendor_attr(msg, RADIUS_VENDOR_ID_CISCO,
					 RADIUS_CISCO_AV_PAIR, &keylen);
	if (key && keylen == 51 &&
	    os_memcmp(key, "leap:session-key=", 17) == 0) {
		keys->recv = decrypt_ms_key(key + 17, keylen - 17,
					    sent_msg->hdr->authenticator,
					    secret, secret_len,
					    &keys->recv_len);
	}
	os_free(key);

	return keys;
}


int radius_msg_add_mppe_keys(struct radius_msg *msg,
			     const u8 *req_authenticator,
			     const u8 *secret, size_t secret_len,
			     const u8 *send_key, size_t send_key_len,
			     const u8 *recv_key, size_t recv_key_len)
{
	struct radius_attr_hdr *attr;
	u32 vendor_id = htonl(RADIUS_VENDOR_ID_MICROSOFT);
	u8 *buf;
	struct radius_attr_vendor *vhdr;
	u8 *pos;
	size_t elen;
	int hlen;
	u16 salt;

	hlen = sizeof(vendor_id) + sizeof(*vhdr) + 2;

	/* MS-MPPE-Send-Key */
	buf = os_malloc(hlen + send_key_len + 16);
	if (buf == NULL) {
		return 0;
	}
	pos = buf;
	os_memcpy(pos, &vendor_id, sizeof(vendor_id));
	pos += sizeof(vendor_id);
	vhdr = (struct radius_attr_vendor *) pos;
	vhdr->vendor_type = RADIUS_VENDOR_ATTR_MS_MPPE_SEND_KEY;
	pos = (u8 *) (vhdr + 1);
	salt = os_random() | 0x8000;
	*pos++ = salt >> 8;
	*pos++ = salt;
	encrypt_ms_key(send_key, send_key_len, salt, req_authenticator, secret,
		       secret_len, pos, &elen);
	vhdr->vendor_length = hlen + elen - sizeof(vendor_id);

	attr = radius_msg_add_attr(msg, RADIUS_ATTR_VENDOR_SPECIFIC,
				   buf, hlen + elen);
	os_free(buf);
	if (attr == NULL) {
		return 0;
	}

	/* MS-MPPE-Recv-Key */
	buf = os_malloc(hlen + send_key_len + 16);
	if (buf == NULL) {
		return 0;
	}
	pos = buf;
	os_memcpy(pos, &vendor_id, sizeof(vendor_id));
	pos += sizeof(vendor_id);
	vhdr = (struct radius_attr_vendor *) pos;
	vhdr->vendor_type = RADIUS_VENDOR_ATTR_MS_MPPE_RECV_KEY;
	pos = (u8 *) (vhdr + 1);
	salt ^= 1;
	*pos++ = salt >> 8;
	*pos++ = salt;
	encrypt_ms_key(recv_key, recv_key_len, salt, req_authenticator, secret,
		       secret_len, pos, &elen);
	vhdr->vendor_length = hlen + elen - sizeof(vendor_id);

	attr = radius_msg_add_attr(msg, RADIUS_ATTR_VENDOR_SPECIFIC,
				   buf, hlen + elen);
	os_free(buf);
	if (attr == NULL) {
		return 0;
	}

	return 1;
}


/* Add User-Password attribute to a RADIUS message and encrypt it as specified
 * in RFC 2865, Chap. 5.2 */
struct radius_attr_hdr *
radius_msg_add_attr_user_password(struct radius_msg *msg,
				  u8 *data, size_t data_len,
				  u8 *secret, size_t secret_len)
{
	u8 buf[128];
	int padlen, i;
	size_t buf_len, pos;
	const u8 *addr[2];
	size_t len[2];
	u8 hash[16];

	if (data_len > 128)
		return NULL;

	os_memcpy(buf, data, data_len);
	buf_len = data_len;

	padlen = data_len % 16;
	if (padlen) {
		padlen = 16 - padlen;
		os_memset(buf + data_len, 0, padlen);
		buf_len += padlen;
	}

	addr[0] = secret;
	len[0] = secret_len;
	addr[1] = msg->hdr->authenticator;
	len[1] = 16;
	md5_vector(2, addr, len, hash);

	for (i = 0; i < 16; i++)
		buf[i] ^= hash[i];
	pos = 16;

	while (pos < buf_len) {
		addr[0] = secret;
		len[0] = secret_len;
		addr[1] = &buf[pos - 16];
		len[1] = 16;
		md5_vector(2, addr, len, hash);

		for (i = 0; i < 16; i++)
			buf[pos + i] ^= hash[i];

		pos += 16;
	}

	return radius_msg_add_attr(msg, RADIUS_ATTR_USER_PASSWORD,
				   buf, buf_len);
}


int radius_msg_get_attr(struct radius_msg *msg, u8 type, u8 *buf, size_t len)
{
	struct radius_attr_hdr *attr = NULL;
	size_t i, dlen;

	for (i = 0; i < msg->attr_used; i++) {
		if (msg->attrs[i]->type == type) {
			attr = msg->attrs[i];
			break;
		}
	}

	if (!attr)
		return -1;

	dlen = attr->length - sizeof(*attr);
	if (buf)
		os_memcpy(buf, (attr + 1), dlen > len ? len : dlen);
	return dlen;
}


int radius_msg_get_attr_ptr(struct radius_msg *msg, u8 type, u8 **buf,
			    size_t *len, const u8 *start)
{
	size_t i;
	struct radius_attr_hdr *attr = NULL;

	for (i = 0; i < msg->attr_used; i++) {
		if (msg->attrs[i]->type == type &&
		    (start == NULL || (u8 *) msg->attrs[i] > start)) {
			attr = msg->attrs[i];
			break;
		}
	}

	if (!attr)
		return -1;

	*buf = (u8 *) (attr + 1);
	*len = attr->length - sizeof(*attr);
	return 0;
}


int radius_msg_count_attr(struct radius_msg *msg, u8 type, int min_len)
{
	size_t i;
	int count;

	for (count = 0, i = 0; i < msg->attr_used; i++) {
		if (msg->attrs[i]->type == type &&
		    msg->attrs[i]->length >=
		    sizeof(struct radius_attr_hdr) + min_len)
			count++;
	}

	return count;
}


struct radius_tunnel_attrs {
	int tag_used;
	int type; /* Tunnel-Type */
	int medium_type; /* Tunnel-Medium-Type */
	int vlanid;
};


/**
 * radius_msg_get_vlanid - Parse RADIUS attributes for VLAN tunnel information
 * @msg: RADIUS message
 * Returns: VLAN ID for the first tunnel configuration of -1 if none is found
 */
int radius_msg_get_vlanid(struct radius_msg *msg)
{
	struct radius_tunnel_attrs tunnel[RADIUS_TUNNEL_TAGS], *tun;
	size_t i;
	struct radius_attr_hdr *attr = NULL;
	const u8 *data;
	char buf[10];
	size_t dlen;

	os_memset(&tunnel, 0, sizeof(tunnel));

	for (i = 0; i < msg->attr_used; i++) {
		attr = msg->attrs[i];
		data = (const u8 *) (attr + 1);
		dlen = attr->length - sizeof(*attr);
		if (attr->length < 3)
			continue;
		if (data[0] >= RADIUS_TUNNEL_TAGS)
			tun = &tunnel[0];
		else
			tun = &tunnel[data[0]];

		switch (attr->type) {
		case RADIUS_ATTR_TUNNEL_TYPE:
			if (attr->length != 6)
				break;
			tun->tag_used++;
			tun->type = (data[1] << 16) | (data[2] << 8) | data[3];
			break;
		case RADIUS_ATTR_TUNNEL_MEDIUM_TYPE:
			if (attr->length != 6)
				break;
			tun->tag_used++;
			tun->medium_type =
				(data[1] << 16) | (data[2] << 8) | data[3];
			break;
		case RADIUS_ATTR_TUNNEL_PRIVATE_GROUP_ID:
			if (data[0] < RADIUS_TUNNEL_TAGS) {
				data++;
				dlen--;
			}
			if (dlen >= sizeof(buf))
				break;
			os_memcpy(buf, data, dlen);
			buf[dlen] = '\0';
			tun->tag_used++;
			tun->vlanid = atoi(buf);
			break;
		}
	}

	for (i = 0; i < RADIUS_TUNNEL_TAGS; i++) {
		tun = &tunnel[i];
		if (tun->tag_used &&
		    tun->type == RADIUS_TUNNEL_TYPE_VLAN &&
		    tun->medium_type == RADIUS_TUNNEL_MEDIUM_TYPE_802 &&
		    tun->vlanid > 0)
			return tun->vlanid;
	}

	return -1;
}
