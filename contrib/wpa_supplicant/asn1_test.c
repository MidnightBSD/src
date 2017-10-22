/*
 * Testing tool for ASN.1/X.509v3 routines
 * Copyright (c) 2006, Jouni Malinen <j@w1.fi>
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
#include "asn1.h"
#include "x509v3.h"

extern int wpa_debug_level;


static const char * asn1_class_str(int class)
{
	switch (class) {
	case ASN1_CLASS_UNIVERSAL:
		return "Universal";
	case ASN1_CLASS_APPLICATION:
		return "Application";
	case ASN1_CLASS_CONTEXT_SPECIFIC:
		return "Context-specific";
	case ASN1_CLASS_PRIVATE:
		return "Private";
	default:
		return "?";
	}
}


int asn1_parse(const u8 *buf, size_t len, int level)
{
	const u8 *pos, *prev, *end;
	char prefix[10], str[100];
	int _level;
	struct asn1_hdr hdr;
	struct asn1_oid oid;
	u8 tmp;

	_level = level;
	if ((size_t) _level > sizeof(prefix) - 1)
		_level = sizeof(prefix) - 1;
	memset(prefix, ' ', _level);
	prefix[_level] = '\0';

	pos = buf;
	end = buf + len;

	while (pos < end) {
		if (asn1_get_next(pos, end - pos, &hdr) < 0)
			return -1;

		prev = pos;
		pos = hdr.payload;

		wpa_printf(MSG_MSGDUMP, "ASN.1:%s Class %d(%s) P/C %d(%s) "
			   "Tag %u Length %u",
			   prefix, hdr.class, asn1_class_str(hdr.class),
			   hdr.constructed,
			   hdr.constructed ? "Constructed" : "Primitive",
			   hdr.tag, hdr.length);

		if (hdr.class == ASN1_CLASS_CONTEXT_SPECIFIC &&
		    hdr.constructed) {
			if (asn1_parse(pos, hdr.length, level + 1) < 0)
				return -1;
			pos += hdr.length;
		}

		if (hdr.class != ASN1_CLASS_UNIVERSAL)
			continue;

		switch (hdr.tag) {
		case ASN1_TAG_EOC:
			if (hdr.length) {
				wpa_printf(MSG_DEBUG, "ASN.1: Non-zero "
					   "end-of-contents length (%u)",
					   hdr.length);
				return -1;
			}
			wpa_printf(MSG_MSGDUMP, "ASN.1:%s EOC", prefix);
			break;
		case ASN1_TAG_BOOLEAN:
			if (hdr.length != 1) {
				wpa_printf(MSG_DEBUG, "ASN.1: Unexpected "
					   "Boolean length (%u)", hdr.length);
				return -1;
			}
			tmp = *pos++;
			wpa_printf(MSG_MSGDUMP, "ASN.1:%s Boolean %s",
				   prefix, tmp ? "TRUE" : "FALSE");
			break;
		case ASN1_TAG_INTEGER:
			wpa_hexdump(MSG_MSGDUMP, "ASN.1: INTEGER",
				    pos, hdr.length);
			pos += hdr.length;
			break;
		case ASN1_TAG_BITSTRING:
			wpa_hexdump(MSG_MSGDUMP, "ASN.1: BitString",
				    pos, hdr.length);
			pos += hdr.length;
			break;
		case ASN1_TAG_OCTETSTRING:
			wpa_hexdump(MSG_MSGDUMP, "ASN.1: OctetString",
				    pos, hdr.length);
			pos += hdr.length;
			break;
		case ASN1_TAG_NULL:
			if (hdr.length) {
				wpa_printf(MSG_DEBUG, "ASN.1: Non-zero Null "
					   "length (%u)", hdr.length);
				return -1;
			}
			wpa_printf(MSG_MSGDUMP, "ASN.1:%s Null", prefix);
			break;
		case ASN1_TAG_OID:
			if (asn1_get_oid(prev, end - prev, &oid, &prev) < 0) {
				wpa_printf(MSG_DEBUG, "ASN.1: Invalid OID");
				return -1;
			}
			asn1_oid_to_str(&oid, str, sizeof(str));
			wpa_printf(MSG_DEBUG, "ASN.1:%s OID %s", prefix, str);
			pos += hdr.length;
			break;
		case ANS1_TAG_RELATIVE_OID:
			wpa_hexdump(MSG_MSGDUMP, "ASN.1: Relative OID",
				    pos, hdr.length);
			pos += hdr.length;
			break;
		case ASN1_TAG_SEQUENCE:
			wpa_printf(MSG_MSGDUMP, "ASN.1:%s SEQUENCE", prefix);
			if (asn1_parse(pos, hdr.length, level + 1) < 0)
				return -1;
			pos += hdr.length;
			break;
		case ASN1_TAG_SET:
			wpa_printf(MSG_MSGDUMP, "ASN.1:%s SET", prefix);
			if (asn1_parse(pos, hdr.length, level + 1) < 0)
				return -1;
			pos += hdr.length;
			break;
		case ASN1_TAG_PRINTABLESTRING:
			wpa_hexdump_ascii(MSG_MSGDUMP,
					  "ASN.1: PrintableString",
					  pos, hdr.length);
			pos += hdr.length;
			break;
		case ASN1_TAG_IA5STRING:
			wpa_hexdump_ascii(MSG_MSGDUMP, "ASN.1: IA5String",
					  pos, hdr.length);
			pos += hdr.length;
			break;
		case ASN1_TAG_UTCTIME:
			wpa_hexdump_ascii(MSG_MSGDUMP, "ASN.1: UTCTIME",
					  pos, hdr.length);
			pos += hdr.length;
			break;
		case ASN1_TAG_VISIBLESTRING:
			wpa_hexdump_ascii(MSG_MSGDUMP, "ASN.1: VisibleString",
					  pos, hdr.length);
			pos += hdr.length;
			break;
		default:
			wpa_printf(MSG_DEBUG, "ASN.1: Unknown tag %d",
				   hdr.tag);
			return -1;
		}
	}

	return 0;
}


int main(int argc, char *argv[])
{
	FILE *f;
	u8 buf[3000];
	size_t len;
	struct x509_certificate *cert;

	wpa_debug_level = 0;

	f = fopen(argv[1], "rb");
	if (f == NULL)
		return -1;
	len = fread(buf, 1, sizeof(buf), f);
	fclose(f);

	if (asn1_parse(buf, len, 0) < 0)
		printf("Failed to parse DER ASN.1\n");

	printf("\n\n");

	cert = x509_certificate_parse(buf, len);
	if (cert == NULL)
		printf("Failed to parse X.509 certificate\n");
	x509_certificate_free(cert);

	return 0;
}
