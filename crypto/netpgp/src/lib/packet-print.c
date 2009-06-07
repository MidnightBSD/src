/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Alistair Crooks (agc@NetBSD.org)
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 2005-2008 Nominet UK (www.nic.uk)
 * All rights reserved.
 * Contributors: Ben Laurie, Rachel Willmer. The Contributors have asserted
 * their moral rights under the UK Copyright Design and Patents Act 1988 to
 * be recorded as the authors of this copyright work.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * ! \file \brief Standard API print functions
 */
#include "config.h"

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

#if defined(__NetBSD__)
__COPYRIGHT("@(#) Copyright (c) 2009 The NetBSD Foundation, Inc. All rights reserved.");
__RCSID("$NetBSD: packet-print.c,v 1.16 2009/05/31 23:26:20 agc Exp $");
#endif

#include <string.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "crypto.h"
#include "keyring.h"
#include "packet-show.h"
#include "signature.h"
#include "readerwriter.h"
#include "netpgpdefs.h"
#include "packet.h"
#include "netpgpdigest.h"

static int      indent = 0;

/* static functions */

static void 
print_indent(void)
{
	int             i = 0;

	for (i = 0; i < indent; i++) {
		printf("  ");
	}
}

static void 
print_name(const char *name)
{
	print_indent();
	if (name) {
		printf("%s: ", name);
	}
}

static void 
print_hexdump(const char *name, const unsigned char *data, unsigned int len)
{
	print_name(name);

	printf("len=%d, data=0x", len);
	hexdump(stdout, data, len, "");
	printf("\n");
}

static void 
hexdump_data(const char *name, const unsigned char *data, unsigned len)
{
	print_name(name);

	printf("0x");
	hexdump(stdout, data, len, "");
	printf("\n");
}

static void 
print_uint(const char *name, unsigned int val)
{
	print_name(name);
	printf("%d\n", val);
}

static void 
showtime(const char *name, time_t t)
{
	printf("%s=%" PRItime "d (%.24s)", name, (long long) t, ctime(&t));
}

static void 
print_time(const char *name, time_t t)
{
	print_indent();
	printf("%s: ", name);
	showtime("time", t);
	printf("\n");
}

static void 
ptime(FILE *fp, time_t t)
{
	struct tm      *tm;

	tm = gmtime(&t);
	(void) fprintf(fp, "%04d-%02d-%02d",
		tm->tm_year + 1900,
		tm->tm_mon + 1,
		tm->tm_mday);
}

static void 
print_string_and_value(const char *name, const char *str, unsigned char value)
{
	print_name(name);
	printf("%s (0x%x)\n", str, value);
}

static void 
print_tagname(const char *str)
{
	print_indent();
	printf("%s packet\n", str);
}

static void 
print_data(const char *name, const __ops_data_t *data)
{
	print_hexdump(name, data->contents, data->len);
}

static void 
print_bn(const char *name, const BIGNUM *bn)
{
	print_indent();
	printf("%s=", name);
	if (bn) {
		BN_print_fp(stdout, bn);
		putchar('\n');
	} else {
		puts("(unset)");
	}
}


static void 
print_packet_hex(const __ops_subpacket_t *pkt)
{
	unsigned char  *cur;
	unsigned	rem;
	unsigned	blksz = 4;
	int             i;

	printf("\nhexdump of packet contents follows:\n");
	for (i = 1, cur = pkt->raw;
	     cur < (pkt->raw + pkt->length);
	     cur += blksz, i++) {
		rem = pkt->raw + pkt->length - cur;
		hexdump(stdout, cur, (rem <= blksz) ? rem : blksz, "");
		printf(" ");
		if (i % 8 == 0) {
			printf("\n");
		}

	}
	printf("\n");
}

static void 
print_escaped(const unsigned char *data, size_t length)
{
	while (length-- > 0) {
		if ((*data >= 0x20 && *data < 0x7f && *data != '%') ||
		    *data == '\n') {
			putchar(*data);
		} else {
			printf("%%%02x", *data);
		}
		++data;
	}
}

static void 
print_string(const char *name, const char *str)
{
	print_name(name);
	print_escaped((const unsigned char *) str, strlen(str));
	putchar('\n');
}

static void 
print_utf8_string(const char *name, const unsigned char *str)
{
	/* \todo Do this better for non-English character sets */
	print_string(name, (const char *) str);
}

static void 
print_duration(const char *name, time_t t)
{
	int             mins, hours, days, years;

	print_indent();
	printf("%s: ", name);
	printf("duration %" PRItime "d seconds", (long long) t);

	mins = (int)(t / 60);
	hours = mins / 60;
	days = hours / 24;
	years = days / 365;

	printf(" (approx. ");
	if (years) {
		printf("%d %s", years, years == 1 ? "year" : "years");
	} else if (days) {
		printf("%d %s", days, days == 1 ? "day" : "days");
	} else if (hours) {
		printf("%d %s", hours, hours == 1 ? "hour" : "hours");
	}
	printf(")\n");
}

static void 
print_boolean(const char *name, unsigned char boolval)
{
	print_name(name);
	printf("%s\n", (boolval) ? "Yes" : "No");
}

static void 
print_text_breakdown(__ops_text_t *text)
{
	const char     *prefix = ".. ";
	unsigned        i;

	/* these were recognised */
	for (i = 0; i < text->known.used; i++) {
		print_indent();
		printf("%s", prefix);
		printf("%s\n", text->known.strings[i]);
	}
	/*
	 * these were not recognised. the strings will contain the hex value
	 * of the unrecognised value in string format - see
	 * process_octet_str()
	 */
	if (text->unknown.used) {
		printf("\n");
		print_indent();
		printf("Not Recognised: ");
	}
	for (i = 0; i < text->unknown.used; i++) {
		print_indent();
		printf("%s", prefix);
		printf("%s\n", text->unknown.strings[i]);
	}
}

static void 
print_headers(const __ops_headers_t *h)
{
	unsigned        i;

	for (i = 0; i < h->headerc; ++i) {
		printf("%s=%s\n", h->headers[i].key, h->headers[i].value);
	}
}

static void 
print_block(const char *name, const unsigned char *str, size_t length)
{
	int             o = length;

	print_indent();
	printf(">>>>> %s >>>>>\n", name);

	print_indent();
	for (; length > 0; --length) {
		if (*str >= 0x20 && *str < 0x7f && *str != '%') {
			putchar(*str);
		} else if (*str == '\n') {
			putchar(*str);
			print_indent();
		} else {
			printf("%%%02x", *str);
		}
		++str;
	}
	if (o && str[-1] != '\n') {
		putchar('\n');
		print_indent();
		fputs("[no newline]", stdout);
	} else {
		print_indent();
	}
	printf("<<<<< %s <<<<<\n", name);
}

/* return the number of bits in the public key */
static int
numkeybits(const __ops_pubkey_t *pubkey)
{
	switch(pubkey->alg) {
	case OPS_PKA_RSA:
	case OPS_PKA_RSA_ENCRYPT_ONLY:
	case OPS_PKA_RSA_SIGN_ONLY:
		return BN_num_bytes(pubkey->key.rsa.n) * 8;
	case OPS_PKA_DSA:
		switch(BN_num_bytes(pubkey->key.dsa.q)) {
		case 20:
			return 1024;
		case 28:
			return 2048;
		case 32:
			return 3072;
		default:
			return 0;
		}
	case OPS_PKA_ELGAMAL:
		return BN_num_bytes(pubkey->key.elgamal.y) * 8;
	default:
		return -1;
	}
}

/**
   \ingroup Core_Print

   Prints a public key in succinct detail

   \param key Ptr to public key
*/
void
__ops_print_pubkeydata(__ops_io_t *io, const __ops_keydata_t *key)
{
	unsigned int    i;

	(void) fprintf(io->errs, "pub %d/%s ",
		numkeybits(&key->key.pubkey),
		__ops_show_pka(key->key.pubkey.alg));
	hexdump(io->errs, key->key_id, OPS_KEY_ID_SIZE, "");
	(void) fprintf(io->errs, " ");
	ptime(io->errs, key->key.pubkey.birthtime);
	(void) fprintf(io->errs, "\nKey fingerprint: ");
	hexdump(io->errs, key->fingerprint.fingerprint, OPS_FINGERPRINT_SIZE,
		" ");
	(void) fprintf(io->errs, "\n");
	for (i = 0; i < key->nuids; i++) {
		(void) fprintf(io->errs, "uid              %s\n",
			key->uids[i].userid);
	}
}

/**
\ingroup Core_Print
\param pubkey
*/
void
__ops_print_pubkey(const __ops_pubkey_t *pubkey)
{
	printf("------- PUBLIC KEY ------\n");
	print_uint("Version", (unsigned)pubkey->version);
	print_time("Creation Time", pubkey->birthtime);
	if (pubkey->version == OPS_V3) {
		print_uint("Days Valid", pubkey->days_valid);
	}
	print_string_and_value("Algorithm", __ops_show_pka(pubkey->alg),
			       pubkey->alg);
	switch (pubkey->alg) {
	case OPS_PKA_DSA:
		print_bn("p", pubkey->key.dsa.p);
		print_bn("q", pubkey->key.dsa.q);
		print_bn("g", pubkey->key.dsa.g);
		print_bn("y", pubkey->key.dsa.y);
		break;

	case OPS_PKA_RSA:
	case OPS_PKA_RSA_ENCRYPT_ONLY:
	case OPS_PKA_RSA_SIGN_ONLY:
		print_bn("n", pubkey->key.rsa.n);
		print_bn("e", pubkey->key.rsa.e);
		break;

	case OPS_PKA_ELGAMAL:
	case OPS_PKA_ELGAMAL_ENCRYPT_OR_SIGN:
		print_bn("p", pubkey->key.elgamal.p);
		print_bn("g", pubkey->key.elgamal.g);
		print_bn("y", pubkey->key.elgamal.y);
		break;

	default:
		(void) fprintf(stderr,
			"__ops_print_pubkey: Unusual algorithm\n");
	}

	printf("------- end of PUBLIC KEY ------\n");
}

/**
   \ingroup Core_Print

   Prints a secret key

   \param key Ptr to public key
*/

void
__ops_print_seckeydata(const __ops_keydata_t *key)
{
	printf("sec ");
	__ops_show_pka(key->key.pubkey.alg);
	printf(" ");

	hexdump(stdout, key->key_id, OPS_KEY_ID_SIZE, "");
	printf(" ");

	ptime(stdout, key->key.pubkey.birthtime);
	printf(" ");

	if (key->nuids == 1) {
		/* print on same line as other info */
		printf("%s\n", key->uids[0].userid);
	} else {
		/* print all uids on separate line  */
		unsigned int    i;
		printf("\n");
		for (i = 0; i < key->nuids; i++) {
			printf("uid              %s\n", key->uids[i].userid);
		}
	}
}

/**
\ingroup Core_Print
\param type
\param seckey
*/
static void
__ops_print_seckey_verbose(const __ops_content_tag_t type,
				const __ops_seckey_t *seckey)
{
	printf("------- SECRET KEY or ENCRYPTED SECRET KEY ------\n");
	print_tagname((type == OPS_PTAG_CT_SECRET_KEY) ?
			"SECRET_KEY" :
			"ENCRYPTED_SECRET_KEY");
	/* __ops_print_pubkey(key); */
	printf("S2K Usage: %d\n", seckey->s2k_usage);
	if (seckey->s2k_usage != OPS_S2KU_NONE) {
		printf("S2K Specifier: %d\n", seckey->s2k_specifier);
		printf("Symmetric algorithm: %d (%s)\n", seckey->alg,
		       __ops_show_symm_alg(seckey->alg));
		printf("Hash algorithm: %d (%s)\n", seckey->hash_alg,
		       __ops_show_hash_alg(seckey->hash_alg));
		if (seckey->s2k_specifier != OPS_S2KS_SIMPLE) {
			print_hexdump("Salt", seckey->salt,
					sizeof(seckey->salt));
		}
		if (seckey->s2k_specifier == OPS_S2KS_ITERATED_AND_SALTED) {
			printf("Octet count: %d\n", seckey->octetc);
		}
		print_hexdump("IV", seckey->iv, __ops_block_size(seckey->alg));
	}
	/* no more set if encrypted */
	if (type == OPS_PTAG_CT_ENCRYPTED_SECRET_KEY) {
		return;
	}
	switch (seckey->pubkey.alg) {
	case OPS_PKA_RSA:
		print_bn("d", seckey->key.rsa.d);
		print_bn("p", seckey->key.rsa.p);
		print_bn("q", seckey->key.rsa.q);
		print_bn("u", seckey->key.rsa.u);
		break;

	case OPS_PKA_DSA:
		print_bn("x", seckey->key.dsa.x);
		break;

	default:
		(void) fprintf(stderr,
			"__ops_print_seckey_verbose: unusual algorithm\n");
	}
	if (seckey->s2k_usage == OPS_S2KU_ENCRYPTED_AND_HASHED) {
		print_hexdump("Checkhash", seckey->checkhash,
				OPS_CHECKHASH_SIZE);
	} else {
		printf("Checksum: %04x\n", seckey->checksum);
	}
	printf("------- end of SECRET KEY or ENCRYPTED SECRET KEY ------\n");
}


/**
\ingroup Core_Print
\param tag
\param key
*/
static void 
__ops_print_pk_sesskey(__ops_content_tag_t tag,
			 const __ops_pk_sesskey_t * key)
{
	print_tagname((tag == OPS_PTAG_CT_PK_SESSION_KEY) ?
		"PUBLIC KEY SESSION KEY" :
		"ENCRYPTED PUBLIC KEY SESSION KEY");
	printf("Version: %d\n", key->version);
	print_hexdump("Key ID", key->key_id, sizeof(key->key_id));
	printf("Algorithm: %d (%s)\n", key->alg,
	       __ops_show_pka(key->alg));
	switch (key->alg) {
	case OPS_PKA_RSA:
		print_bn("encrypted_m", key->parameters.rsa.encrypted_m);
		break;

	case OPS_PKA_ELGAMAL:
		print_bn("g_to_k", key->parameters.elgamal.g_to_k);
		print_bn("encrypted_m", key->parameters.elgamal.encrypted_m);
		break;

	default:
		(void) fprintf(stderr,
			"__ops_print_pk_sesskey: unusual algorithm\n");
	}
	if (tag == OPS_PTAG_CT_PK_SESSION_KEY) {
		printf("Symmetric algorithm: %d (%s)\n", key->symm_alg,
		       __ops_show_symm_alg(key->symm_alg));
		print_hexdump("Key", key->key, __ops_key_size(key->symm_alg));
		printf("Checksum: %04x\n", key->checksum);
	}
}

static void 
start_subpacket(int type)
{
	indent++;
	print_indent();
	printf("-- %s (type 0x%02x)\n",
	       __ops_show_ss_type(type),
	       type - OPS_PTAG_SIG_SUBPKT_BASE);
}

static void 
end_subpacket(void)
{
	indent--;
}

/**
\ingroup Core_Print
\param contents
*/
int 
__ops_print_packet(const __ops_packet_t *pkt)
{
	const __ops_contents_t	*content = &pkt->u;
	static unsigned		 unarmoured;
	__ops_text_t		*text;
	const char		*str;

	if (unarmoured && pkt->tag != OPS_PTAG_CT_UNARMOURED_TEXT) {
		unarmoured = 0;
		puts("UNARMOURED TEXT ends");
	}
	if (pkt->tag == OPS_PARSER_PTAG) {
		printf("=> OPS_PARSER_PTAG: %s\n",
			__ops_show_packet_tag(content->ptag.type));
	} else {
		printf("=> %s\n", __ops_show_packet_tag(pkt->tag));
	}

	switch (pkt->tag) {
	case OPS_PARSER_ERROR:
		printf("parse error: %s\n", content->error.error);
		break;

	case OPS_PARSER_ERRCODE:
		printf("parse error: %s\n",
		       __ops_errcode(content->errcode.errcode));
		break;

	case OPS_PARSER_PACKET_END:
		print_packet_hex(&content->packet);
		break;

	case OPS_PARSER_PTAG:
		if (content->ptag.type == OPS_PTAG_CT_PUBLIC_KEY) {
			indent = 0;
			printf("\n*** NEXT KEY ***\n");
		}
		printf("\n");
		print_indent();
		printf("==== ptag new_format=%d type=%d length_type=%d"
		       " length=0x%x (%d) position=0x%x (%d)\n",
		       content->ptag.new_format,
		       content->ptag.type, content->ptag.length_type,
		       content->ptag.length, content->ptag.length,
		       content->ptag.position, content->ptag.position);
		print_tagname(__ops_show_packet_tag(content->ptag.type));
		break;

	case OPS_PTAG_CT_SE_DATA_HEADER:
		print_tagname("SYMMETRIC ENCRYPTED DATA");
		break;

	case OPS_PTAG_CT_SE_IP_DATA_HEADER:
		print_tagname(
			"SYMMETRIC ENCRYPTED INTEGRITY PROTECTED DATA HEADER");
		printf("Version: %d\n", content->se_ip_data_header.version);
		break;

	case OPS_PTAG_CT_SE_IP_DATA_BODY:
		print_tagname(
			"SYMMETRIC ENCRYPTED INTEGRITY PROTECTED DATA BODY");
		printf("  data body length=%d\n",
		       content->se_data_body.length);
		printf("    data=");
		hexdump(stdout, content->se_data_body.data,
			content->se_data_body.length, "");
		printf("\n");
		break;

	case OPS_PTAG_CT_PUBLIC_KEY:
	case OPS_PTAG_CT_PUBLIC_SUBKEY:
		print_tagname((pkt->tag == OPS_PTAG_CT_PUBLIC_KEY) ?
			"PUBLIC KEY" :
			"PUBLIC SUBKEY");
		__ops_print_pubkey(&content->pubkey);
		break;

	case OPS_PTAG_CT_TRUST:
		print_tagname("TRUST");
		print_data("Trust", &content->trust.data);
		break;

	case OPS_PTAG_CT_USER_ID:
		/* XXX: how do we print UTF-8? */
		print_tagname("USER ID");
		print_utf8_string("userid", content->userid.userid);
		break;

	case OPS_PTAG_CT_SIGNATURE:
		print_tagname("SIGNATURE");
		print_indent();
		print_uint("Signature Version",
				   (unsigned)content->sig.info.version);
		if (content->sig.info.birthtime_set) {
			print_time("Signature Creation Time",
				   content->sig.info.birthtime);
		}

		print_string_and_value("Signature Type",
			    __ops_show_sig_type(content->sig.info.type),
				       content->sig.info.type);

		if (content->sig.info.signer_id_set) {
			hexdump_data("Signer ID",
					   content->sig.info.signer_id,
				  sizeof(content->sig.info.signer_id));
		}

		print_string_and_value("Public Key Algorithm",
			__ops_show_pka(content->sig.info.key_alg),
				     content->sig.info.key_alg);
		print_string_and_value("Hash Algorithm",
			__ops_show_hash_alg(content->sig.info.hash_alg),
			content->sig.info.hash_alg);
		print_uint("Hashed data len",
			content->sig.info.v4_hashlen);
		print_indent();
		hexdump_data("hash2", &content->sig.hash2[0], 2);
		switch (content->sig.info.key_alg) {
		case OPS_PKA_RSA:
		case OPS_PKA_RSA_SIGN_ONLY:
			print_bn("sig", content->sig.info.sig.rsa.sig);
			break;

		case OPS_PKA_DSA:
			print_bn("r", content->sig.info.sig.dsa.r);
			print_bn("s", content->sig.info.sig.dsa.s);
			break;

		case OPS_PKA_ELGAMAL_ENCRYPT_OR_SIGN:
			print_bn("r", content->sig.info.sig.elgamal.r);
			print_bn("s", content->sig.info.sig.elgamal.s);
			break;

		default:
			(void) fprintf(stderr,
				"__ops_print_packet: Unusual algorithm\n");
			return 0;
		}

		if (content->sig.hash)
			printf("data hash is set\n");

		break;

	case OPS_PTAG_CT_COMPRESSED:
		print_tagname("COMPRESSED");
		print_uint("Compressed Data Type",
			(unsigned)content->compressed.type);
		break;

	case OPS_PTAG_CT_1_PASS_SIG:
		print_tagname("ONE PASS SIGNATURE");

		print_uint("Version", (unsigned)content->one_pass_sig.version);
		print_string_and_value("Signature Type",
		    __ops_show_sig_type(content->one_pass_sig.sig_type),
				       content->one_pass_sig.sig_type);
		print_string_and_value("Hash Algorithm",
			__ops_show_hash_alg(content->one_pass_sig.hash_alg),
			content->one_pass_sig.hash_alg);
		print_string_and_value("Public Key Algorithm",
			__ops_show_pka(content->one_pass_sig.key_alg),
			content->one_pass_sig.key_alg);
		hexdump_data("Signer ID",
				   content->one_pass_sig.keyid,
				   sizeof(content->one_pass_sig.keyid));
		print_uint("Nested", content->one_pass_sig.nested);
		break;

	case OPS_PTAG_CT_USER_ATTR:
		print_tagname("USER ATTRIBUTE");
		print_hexdump("User Attribute",
			      content->userattr.data.contents,
			      content->userattr.data.len);
		break;

	case OPS_PTAG_RAW_SS:
		if (pkt->critical) {
			(void) fprintf(stderr, "contents are critical\n");
			return 0;
		}
		start_subpacket(pkt->tag);
		print_uint("Raw Signature Subpacket: tag",
			(unsigned)(content->ss_raw.tag -
		   	OPS_PTAG_SIG_SUBPKT_BASE));
		print_hexdump("Raw Data",
			      content->ss_raw.raw,
			      content->ss_raw.length);
		break;

	case OPS_PTAG_SS_CREATION_TIME:
		start_subpacket(pkt->tag);
		print_time("Signature Creation Time", content->ss_time.time);
		end_subpacket();
		break;

	case OPS_PTAG_SS_EXPIRATION_TIME:
		start_subpacket(pkt->tag);
		print_duration("Signature Expiration Time",
			content->ss_time.time);
		end_subpacket();
		break;

	case OPS_PTAG_SS_KEY_EXPIRY:
		start_subpacket(pkt->tag);
		print_duration("Key Expiration Time", content->ss_time.time);
		end_subpacket();
		break;

	case OPS_PTAG_SS_TRUST:
		start_subpacket(pkt->tag);
		print_string("Trust Signature", "");
		print_uint("Level", (unsigned)content->ss_trust.level);
		print_uint("Amount", (unsigned)content->ss_trust.amount);
		end_subpacket();
		break;

	case OPS_PTAG_SS_REVOCABLE:
		start_subpacket(pkt->tag);
		print_boolean("Revocable", content->ss_revocable.revocable);
		end_subpacket();
		break;

	case OPS_PTAG_SS_REVOCATION_KEY:
		start_subpacket(pkt->tag);
		/* not yet tested */
		printf("  revocation key: class=0x%x",
		       content->ss_revocation_key.class);
		if (content->ss_revocation_key.class & 0x40) {
			printf(" (sensitive)");
		}
		printf(", algid=0x%x", content->ss_revocation_key.algid);
		printf(", fingerprint=");
		hexdump(stdout, content->ss_revocation_key.fingerprint,
				OPS_FINGERPRINT_SIZE, "");
		printf("\n");
		end_subpacket();
		break;

	case OPS_PTAG_SS_ISSUER_KEY_ID:
		start_subpacket(pkt->tag);
		print_hexdump("Issuer Key Id",
			      &content->ss_issuer.key_id[0],
			      sizeof(content->ss_issuer.key_id));
		end_subpacket();
		break;

	case OPS_PTAG_SS_PREFERRED_SKA:
		start_subpacket(pkt->tag);
		print_data("Preferred Symmetric Algorithms",
			   &content->ss_skapref.data);

		text = __ops_showall_ss_skapref(content->ss_skapref);
		print_text_breakdown(text);
		__ops_text_free(text);

		end_subpacket();
		break;

	case OPS_PTAG_SS_PRIMARY_USER_ID:
		start_subpacket(pkt->tag);
		print_boolean("Primary User ID",
			      content->ss_primary_userid.primary_userid);
		end_subpacket();
		break;

	case OPS_PTAG_SS_PREFERRED_HASH:
		start_subpacket(pkt->tag);
		print_data("Preferred Hash Algorithms",
			   &content->ss_hashpref.data);

		text = __ops_showall_ss_hashpref(content->ss_hashpref);
		print_text_breakdown(text);
		__ops_text_free(text);
		end_subpacket();
		break;

	case OPS_PTAG_SS_PREF_COMPRESS:
		start_subpacket(pkt->tag);
		print_data("Preferred Compression Algorithms",
			   &content->ss_zpref.data);

		text = __ops_showall_ss_zpref(content->ss_zpref);
		print_text_breakdown(text);
		__ops_text_free(text);
		end_subpacket();
		break;

	case OPS_PTAG_SS_KEY_FLAGS:
		start_subpacket(pkt->tag);
		print_data("Key Flags", &content->ss_key_flags.data);

		text = __ops_showall_ss_key_flags(content->ss_key_flags);
		print_text_breakdown(text);
		__ops_text_free(text);

		end_subpacket();
		break;

	case OPS_PTAG_SS_KEYSERV_PREFS:
		start_subpacket(pkt->tag);
		print_data("Key Server Preferences",
			   &content->ss_key_server_prefs.data);

		text = __ops_show_keyserv_prefs(content->ss_key_server_prefs);
		print_text_breakdown(text);
		__ops_text_free(text);

		end_subpacket();
		break;

	case OPS_PTAG_SS_FEATURES:
		start_subpacket(pkt->tag);
		print_data("Features",
			   &content->ss_features.data);

		text = __ops_showall_ss_features(content->ss_features);
		print_text_breakdown(text);
		__ops_text_free(text);

		end_subpacket();
		break;

	case OPS_PTAG_SS_NOTATION_DATA:
		start_subpacket(pkt->tag);
		print_indent();
		printf("Notation Data:\n");

		indent++;
		print_data("Flags", &content->ss_notation.flags);
		text = __ops_showall_notation(content->ss_notation);
		print_text_breakdown(text);
		__ops_text_free(text);

		/* xxx - TODO: print out UTF - rachel */

		print_data("Name", &content->ss_notation.name);

		print_data("Value", &content->ss_notation.value);

		indent--;
		end_subpacket();
		break;

	case OPS_PTAG_SS_REGEXP:
		start_subpacket(pkt->tag);
		print_hexdump("Regular Expression",
			      (unsigned char *) content->ss_regexp.regexp,
			      strlen(content->ss_regexp.regexp));
		print_string(NULL, content->ss_regexp.regexp);
		end_subpacket();
		break;

	case OPS_PTAG_SS_POLICY_URI:
		start_subpacket(pkt->tag);
		print_string("Policy URL", content->ss_policy.url);
		end_subpacket();
		break;

	case OPS_PTAG_SS_SIGNERS_USER_ID:
		start_subpacket(pkt->tag);
		print_utf8_string("Signer's User ID",
			content->ss_signer.userid);
		end_subpacket();
		break;

	case OPS_PTAG_SS_PREF_KEYSERV:
		start_subpacket(pkt->tag);
		print_string("Preferred Key Server", content->ss_keyserv.name);
		end_subpacket();
		break;

	case OPS_PTAG_SS_EMBEDDED_SIGNATURE:
		start_subpacket(pkt->tag);
		end_subpacket();/* \todo print out contents? */
		break;

	case OPS_PTAG_SS_USERDEFINED00:
	case OPS_PTAG_SS_USERDEFINED01:
	case OPS_PTAG_SS_USERDEFINED02:
	case OPS_PTAG_SS_USERDEFINED03:
	case OPS_PTAG_SS_USERDEFINED04:
	case OPS_PTAG_SS_USERDEFINED05:
	case OPS_PTAG_SS_USERDEFINED06:
	case OPS_PTAG_SS_USERDEFINED07:
	case OPS_PTAG_SS_USERDEFINED08:
	case OPS_PTAG_SS_USERDEFINED09:
	case OPS_PTAG_SS_USERDEFINED10:
		start_subpacket(pkt->tag);
		print_hexdump("Internal or user-defined",
			      content->ss_userdef.data.contents,
			      content->ss_userdef.data.len);
		end_subpacket();
		break;

	case OPS_PTAG_SS_RESERVED:
		start_subpacket(pkt->tag);
		print_hexdump("Reserved",
			      content->ss_userdef.data.contents,
			      content->ss_userdef.data.len);
		end_subpacket();
		break;

	case OPS_PTAG_SS_REVOCATION_REASON:
		start_subpacket(pkt->tag);
		print_hexdump("Revocation Reason",
			      &content->ss_revocation.code,
			      1);
		str = __ops_show_ss_rr_code(content->ss_revocation.code);
		print_string(NULL, str);
		/* xxx - todo : output text as UTF-8 string */
		end_subpacket();
		break;

	case OPS_PTAG_CT_LITERAL_DATA_HEADER:
		print_tagname("LITERAL DATA HEADER");
		printf("  literal data header format=%c filename='%s'\n",
		       content->litdata_header.format,
		       content->litdata_header.filename);
		showtime("    modification time",
			 content->litdata_header.mtime);
		printf("\n");
		break;

	case OPS_PTAG_CT_LITERAL_DATA_BODY:
		print_tagname("LITERAL DATA BODY");
		printf("  literal data body length=%d\n",
		       content->litdata_body.length);
		printf("    data=");
		print_escaped(content->litdata_body.data,
			      content->litdata_body.length);
		printf("\n");
		break;

	case OPS_PTAG_CT_SIGNATURE_HEADER:
		print_tagname("SIGNATURE");
		print_indent();
		print_uint("Signature Version",
				   (unsigned)content->sig.info.version);
		if (content->sig.info.birthtime_set) {
			print_time("Signature Creation Time",
				content->sig.info.birthtime);
		}
		print_string_and_value("Signature Type",
			    __ops_show_sig_type(content->sig.info.type),
				       content->sig.info.type);
		if (content->sig.info.signer_id_set) {
			hexdump_data("Signer ID",
				content->sig.info.signer_id,
				sizeof(content->sig.info.signer_id));
		}
		print_string_and_value("Public Key Algorithm",
			__ops_show_pka(content->sig.info.key_alg),
				     content->sig.info.key_alg);
		print_string_and_value("Hash Algorithm",
			__ops_show_hash_alg(content->sig.info.hash_alg),
			content->sig.info.hash_alg);

		break;

	case OPS_PTAG_CT_SIGNATURE_FOOTER:
		print_indent();
		hexdump_data("hash2", &content->sig.hash2[0], 2);

		switch (content->sig.info.key_alg) {
		case OPS_PKA_RSA:
			print_bn("sig", content->sig.info.sig.rsa.sig);
			break;

		case OPS_PKA_DSA:
			print_bn("r", content->sig.info.sig.dsa.r);
			print_bn("s", content->sig.info.sig.dsa.s);
			break;

		case OPS_PKA_ELGAMAL_ENCRYPT_OR_SIGN:
			print_bn("r", content->sig.info.sig.elgamal.r);
			print_bn("s", content->sig.info.sig.elgamal.s);
			break;

		case OPS_PKA_PRIVATE00:
		case OPS_PKA_PRIVATE01:
		case OPS_PKA_PRIVATE02:
		case OPS_PKA_PRIVATE03:
		case OPS_PKA_PRIVATE04:
		case OPS_PKA_PRIVATE05:
		case OPS_PKA_PRIVATE06:
		case OPS_PKA_PRIVATE07:
		case OPS_PKA_PRIVATE08:
		case OPS_PKA_PRIVATE09:
		case OPS_PKA_PRIVATE10:
			print_data("Private/Experimental",
			   &content->sig.info.sig.unknown.data);
			break;

		default:
			(void) fprintf(stderr,
				"__ops_print_packet: Unusual key algorithm\n");
			return 0;
		}
		break;

	case OPS_GET_PASSPHRASE:
		print_tagname("OPS_GET_PASSPHRASE");
		break;

	case OPS_PTAG_CT_SECRET_KEY:
		print_tagname("OPS_PTAG_CT_SECRET_KEY");
		__ops_print_seckey_verbose(pkt->tag, &content->seckey);
		break;

	case OPS_PTAG_CT_ENCRYPTED_SECRET_KEY:
		print_tagname("OPS_PTAG_CT_ENCRYPTED_SECRET_KEY");
		__ops_print_seckey_verbose(pkt->tag, &content->seckey);
		break;

	case OPS_PTAG_CT_ARMOUR_HEADER:
		print_tagname("ARMOUR HEADER");
		print_string("type", content->armour_header.type);
		break;

	case OPS_PTAG_CT_SIGNED_CLEARTEXT_HEADER:
		print_tagname("SIGNED CLEARTEXT HEADER");
		print_headers(&content->cleartext_head.headers);
		break;

	case OPS_PTAG_CT_SIGNED_CLEARTEXT_BODY:
		print_tagname("SIGNED CLEARTEXT BODY");
		print_block("signed cleartext", content->cleartext_body.data,
			    content->cleartext_body.length);
		break;

	case OPS_PTAG_CT_SIGNED_CLEARTEXT_TRAILER:
		print_tagname("SIGNED CLEARTEXT TRAILER");
		printf("hash algorithm: %d\n",
		       content->cleartext_trailer.hash->alg);
		printf("\n");
		break;

	case OPS_PTAG_CT_UNARMOURED_TEXT:
		if (!unarmoured) {
			print_tagname("UNARMOURED TEXT");
			unarmoured = 1;
		}
		putchar('[');
		print_escaped(content->unarmoured_text.data,
			      content->unarmoured_text.length);
		putchar(']');
		break;

	case OPS_PTAG_CT_ARMOUR_TRAILER:
		print_tagname("ARMOUR TRAILER");
		print_string("type", content->armour_header.type);
		break;

	case OPS_PTAG_CT_PK_SESSION_KEY:
	case OPS_PTAG_CT_ENCRYPTED_PK_SESSION_KEY:
		__ops_print_pk_sesskey(pkt->tag, &content->pk_sesskey);
		break;

	case OPS_GET_SECKEY:
		__ops_print_pk_sesskey(OPS_PTAG_CT_ENCRYPTED_PK_SESSION_KEY,
				    content->get_seckey.pk_sesskey);
		break;

	default:
		print_tagname("UNKNOWN PACKET TYPE");
		fprintf(stderr, "__ops_print_packet: unknown tag=%d (0x%x)\n",
			pkt->tag, pkt->tag);
		exit(EXIT_FAILURE);
	}
	return 1;
}

static __ops_cb_ret_t 
cb_list_packets(const __ops_packet_t *pkt, __ops_cbdata_t *cbinfo)
{
	__OPS_USED(cbinfo);

	__ops_print_packet(pkt);
	return OPS_RELEASE_MEMORY;
}

/**
\ingroup Core_Print
\param filename
\param armour
\param keyring
\param cb_get_passphrase
*/
int 
__ops_list_packets(__ops_io_t *io,
			char *filename,
			unsigned armour,
			__ops_keyring_t *keyring,
			__ops_cbfunc_t *cb_get_passphrase)
{
	__ops_parseinfo_t	*pinfo = NULL;
	const unsigned		 accumulate = 1;
	const int		 printerrors = 1;
	int			 fd = 0;

	fd = __ops_setup_file_read(io, &pinfo, filename, NULL, cb_list_packets,
				accumulate);
	__ops_parse_options(pinfo, OPS_PTAG_SS_ALL, OPS_PARSE_PARSED);
	pinfo->cryptinfo.keyring = keyring;
	pinfo->cryptinfo.getpassphrase = cb_get_passphrase;
	if (armour) {
		__ops_reader_push_dearmour(pinfo);
	}
	__ops_parse(pinfo, printerrors);
	__ops_teardown_file_read(pinfo, fd);
	return 1;
}
