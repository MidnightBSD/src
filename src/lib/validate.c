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
#include "config.h"

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

#if defined(__NetBSD__)
__COPYRIGHT("@(#) Copyright (c) 2009 The NetBSD Foundation, Inc. All rights reserved.");
__RCSID("$NetBSD: validate.c,v 1.17 2009/05/31 23:26:20 agc Exp $");
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <string.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "packet-parse.h"
#include "packet-show.h"
#include "keyring.h"
#include "signature.h"
#include "netpgpsdk.h"
#include "readerwriter.h"
#include "netpgpdefs.h"
#include "memory.h"
#include "packet.h"
#include "crypto.h"
#include "validate.h"

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif


/* Does the signed hash match the given hash? */
static unsigned
check_binary_sig(const unsigned len,
		const unsigned char *data,
		const __ops_sig_t *sig,
		const __ops_pubkey_t *signer)
{
	unsigned char   hashout[OPS_MAX_HASH_SIZE];
	unsigned char   trailer[6];
	unsigned int    hashedlen;
	__ops_hash_t	hash;
	unsigned	n = 0;

	__OPS_USED(signer);
	__ops_hash_any(&hash, sig->info.hash_alg);
	hash.init(&hash);
	hash.add(&hash, data, len);
	switch (sig->info.version) {
	case OPS_V3:
		trailer[0] = sig->info.type;
		trailer[1] = (unsigned)(sig->info.birthtime) >> 24;
		trailer[2] = (unsigned)(sig->info.birthtime) >> 16;
		trailer[3] = (unsigned)(sig->info.birthtime) >> 8;
		trailer[4] = (unsigned char)(sig->info.birthtime);
		hash.add(&hash, &trailer[0], 5);
		break;

	case OPS_V4:
		hash.add(&hash, sig->info.v4_hashed, sig->info.v4_hashlen);
		trailer[0] = 0x04;	/* version */
		trailer[1] = 0xFF;
		hashedlen = sig->info.v4_hashlen;
		trailer[2] = hashedlen >> 24;
		trailer[3] = hashedlen >> 16;
		trailer[4] = hashedlen >> 8;
		trailer[5] = hashedlen;
		hash.add(&hash, trailer, 6);
		break;

	default:
		(void) fprintf(stderr, "Invalid signature version %d\n",
				sig->info.version);
		return 0;
	}

	n = hash.finish(&hash, hashout);
	if (__ops_get_debug_level(__FILE__)) {
		printf("check_binary_sig: hash length %" PRIsize "u\n",
			hash.size);
	}
	return __ops_check_sig(hashout, n, sig, signer);
}

static int 
keydata_reader(void *dest, size_t length, __ops_error_t **errors,
	       __ops_reader_t *readinfo,
	       __ops_cbdata_t *cbinfo)
{
	validate_reader_t *reader = __ops_reader_get_arg(readinfo);

	__OPS_USED(errors);
	__OPS_USED(cbinfo);
	if (reader->offset == reader->key->packets[reader->packet].length) {
		reader->packet += 1;
		reader->offset = 0;
	}
	if (reader->packet == reader->key->npackets) {
		return 0;
	}

	/*
	 * we should never be asked to cross a packet boundary in a single
	 * read
	 */
	if (reader->key->packets[reader->packet].length <
			reader->offset + length) {
		(void) fprintf(stderr, "keydata_reader: weird length\n");
		return 0;
	}

	(void) memcpy(dest,
		&reader->key->packets[reader->packet].raw[reader->offset],
		length);
	reader->offset += length;

	return length;
}

static void 
free_sig_info(__ops_sig_info_t *sig)
{
	(void) free(sig->v4_hashed);
	(void) free(sig);
}

static void 
copy_sig_info(__ops_sig_info_t *dst, const __ops_sig_info_t *src)
{
	(void) memcpy(dst, src, sizeof(*src));
	dst->v4_hashed = calloc(1, src->v4_hashlen);
	(void) memcpy(dst->v4_hashed, src->v4_hashed, src->v4_hashlen);
}

static void 
add_sig_to_list(const __ops_sig_info_t *sig, __ops_sig_info_t **sigs,
			unsigned *count)
{
	if (*count == 0) {
		*sigs = calloc(*count + 1, sizeof(__ops_sig_info_t));
	} else {
		*sigs = realloc(*sigs,
				(*count + 1) * sizeof(__ops_sig_info_t));
	}
	copy_sig_info(&(*sigs)[*count], sig);
	*count += 1;
}


__ops_cb_ret_t
__ops_validate_key_cb(const __ops_packet_t *pkt, __ops_cbdata_t *cbinfo)
{
	const __ops_contents_t	 *content = &pkt->u;
	const __ops_keydata_t	 *signer;
	validate_key_cb_t	 *key;
	__ops_error_t		**errors;
	__ops_io_t		 *io;
	unsigned		  valid = 0;

	io = cbinfo->io;
	if (__ops_get_debug_level(__FILE__)) {
		(void) fprintf(io->errs, "%s\n",
				__ops_show_packet_tag(pkt->tag));
	}
	key = __ops_callback_arg(cbinfo);
	errors = __ops_callback_errors(cbinfo);
	switch (pkt->tag) {
	case OPS_PTAG_CT_PUBLIC_KEY:
		if (key->pubkey.version != 0) {
			(void) fprintf(io->errs,
				"__ops_validate_key_cb: version bad\n");
			return OPS_FINISHED;
		}
		key->pubkey = content->pubkey;
		return OPS_KEEP_MEMORY;

	case OPS_PTAG_CT_PUBLIC_SUBKEY:
		if (key->subkey.version) {
			__ops_pubkey_free(&key->subkey);
		}
		key->subkey = content->pubkey;
		return OPS_KEEP_MEMORY;

	case OPS_PTAG_CT_SECRET_KEY:
		key->seckey = content->seckey;
		key->pubkey = key->seckey.pubkey;
		return OPS_KEEP_MEMORY;

	case OPS_PTAG_CT_USER_ID:
		if (key->userid.userid) {
			__ops_userid_free(&key->userid);
		}
		key->userid = content->userid;
		key->last_seen = ID;
		return OPS_KEEP_MEMORY;

	case OPS_PTAG_CT_USER_ATTR:
		if (content->userattr.data.len == 0) {
			(void) fprintf(io->errs,
			"__ops_validate_key_cb: user attribute length 0");
			return OPS_FINISHED;
		}
		(void) fprintf(io->outs, "user attribute, length=%d\n",
			(int) content->userattr.data.len);
		if (key->userattr.data.len) {
			__ops_userattr_free(&key->userattr);
		}
		key->userattr = content->userattr;
		key->last_seen = ATTRIBUTE;
		return OPS_KEEP_MEMORY;

	case OPS_PTAG_CT_SIGNATURE:	/* V3 sigs */
	case OPS_PTAG_CT_SIGNATURE_FOOTER:	/* V4 sigs */

		signer = __ops_getkeybyid(io, key->keyring,
					 content->sig.info.signer_id);
		if (!signer) {
			add_sig_to_list(&content->sig.info,
					&key->result->unknown_sigs,
					&key->result->unknownc);
			break;
		}
		switch (content->sig.info.type) {
		case OPS_CERT_GENERIC:
		case OPS_CERT_PERSONA:
		case OPS_CERT_CASUAL:
		case OPS_CERT_POSITIVE:
		case OPS_SIG_REV_CERT:
			valid = (key->last_seen == ID) ?
			    __ops_check_useridcert_sig(&key->pubkey,
					&key->userid,
					&content->sig,
					__ops_get_pubkey(signer),
					key->reader->key->packets[
						key->reader->packet].raw) :
			    __ops_check_userattrcert_sig(&key->pubkey,
					&key->userattr,
					&content->sig,
				       __ops_get_pubkey(signer),
					key->reader->key->packets[
						key->reader->packet].raw);
			break;

		case OPS_SIG_SUBKEY:
			/*
			 * XXX: we should also check that the signer is the
			 * key we are validating, I think.
			 */
			valid = __ops_check_subkey_sig(&key->pubkey,
				&key->subkey,
				&content->sig,
				__ops_get_pubkey(signer),
				key->reader->key->packets[
					key->reader->packet].raw);
			break;

		case OPS_SIG_DIRECT:
			valid = __ops_check_direct_sig(&key->pubkey,
				&content->sig,
				__ops_get_pubkey(signer),
				key->reader->key->packets[
					key->reader->packet].raw);
			break;

		case OPS_SIG_STANDALONE:
		case OPS_SIG_PRIMARY:
		case OPS_SIG_REV_KEY:
		case OPS_SIG_REV_SUBKEY:
		case OPS_SIG_TIMESTAMP:
		case OPS_SIG_3RD_PARTY:
			OPS_ERROR_1(errors, OPS_E_UNIMPLEMENTED,
				"Sig Verification type 0x%02x not done yet\n",
				content->sig.info.type);
			break;

		default:
			OPS_ERROR_1(errors, OPS_E_UNIMPLEMENTED,
				    "Unexpected signature type 0x%02x\n",
				    	content->sig.info.type);
		}

		if (valid) {
			add_sig_to_list(&content->sig.info,
				&key->result->valid_sigs,
				&key->result->validc);
		} else {
			OPS_ERROR(errors, OPS_E_V_BAD_SIGNATURE, "Bad Sig");
			add_sig_to_list(&content->sig.info,
					&key->result->invalid_sigs,
					&key->result->invalidc);
		}
		break;

		/* ignore these */
	case OPS_PARSER_PTAG:
	case OPS_PTAG_CT_SIGNATURE_HEADER:
	case OPS_PARSER_PACKET_END:
		break;

	case OPS_GET_PASSPHRASE:
		if (key->getpassphrase) {
			return key->getpassphrase(pkt, cbinfo);
		}
		break;

	default:
		(void) fprintf(stderr, "unexpected tag=0x%x\n", pkt->tag);
		return OPS_FINISHED;
	}
	return OPS_RELEASE_MEMORY;
}

__ops_cb_ret_t
validate_data_cb(const __ops_packet_t *pkt, __ops_cbdata_t *cbinfo)
{
	const __ops_contents_t	 *content = &pkt->u;
	const __ops_keydata_t	 *signer;
	validate_data_cb_t	 *data;
	__ops_error_t		**errors;
	__ops_io_t		 *io;
	unsigned		  valid = 0;

	io = cbinfo->io;
	if (__ops_get_debug_level(__FILE__)) {
		(void) fprintf(io->errs, "validate_data_cb: %s\n",
				__ops_show_packet_tag(pkt->tag));
	}
	data = __ops_callback_arg(cbinfo);
	errors = __ops_callback_errors(cbinfo);
	switch (pkt->tag) {
	case OPS_PTAG_CT_SIGNED_CLEARTEXT_HEADER:
		/*
		 * ignore - this gives us the "Armor Header" line "Hash:
		 * SHA1" or similar
		 */
		break;

	case OPS_PTAG_CT_LITERAL_DATA_HEADER:
		/* ignore */
		break;

	case OPS_PTAG_CT_LITERAL_DATA_BODY:
		data->data.litdata_body = content->litdata_body;
		data->type = LITERAL_DATA;
		__ops_memory_add(data->mem, data->data.litdata_body.data,
				       data->data.litdata_body.length);
		return OPS_KEEP_MEMORY;

	case OPS_PTAG_CT_SIGNED_CLEARTEXT_BODY:
		data->data.cleartext_body = content->cleartext_body;
		data->type = SIGNED_CLEARTEXT;
		__ops_memory_add(data->mem, data->data.litdata_body.data,
			       data->data.litdata_body.length);
		return OPS_KEEP_MEMORY;

	case OPS_PTAG_CT_SIGNED_CLEARTEXT_TRAILER:
		/* this gives us an __ops_hash_t struct */
		break;

	case OPS_PTAG_CT_SIGNATURE:	/* V3 sigs */
	case OPS_PTAG_CT_SIGNATURE_FOOTER:	/* V4 sigs */
		if (__ops_get_debug_level(__FILE__)) {
			(void) fprintf(io->outs, "\n*** hashed data:\n");
			hexdump(io->outs, content->sig.info.v4_hashed,
					content->sig.info.v4_hashlen, " ");
			(void) fprintf(io->outs, "\n");
			(void) fprintf(io->outs, "type=%02x signer_id=",
					content->sig.info.type);
			hexdump(io->outs, content->sig.info.signer_id,
				sizeof(content->sig.info.signer_id), "");
			(void) fprintf(io->outs, "\n");
		}
		signer = __ops_getkeybyid(io, data->keyring,
					 content->sig.info.signer_id);
		if (!signer) {
			OPS_ERROR(errors, OPS_E_V_UNKNOWN_SIGNER,
					"Unknown Signer");
			add_sig_to_list(&content->sig.info,
					&data->result->unknown_sigs,
					&data->result->unknownc);
			break;
		}
		switch (content->sig.info.type) {
		case OPS_SIG_BINARY:
		case OPS_SIG_TEXT:
			if (__ops_mem_len(data->mem) == 0 &&
			    data->detachname) {
				/* check we have seen some data */
				/* if not, need to read from detached name */
				(void) fprintf(io->outs,
				"netpgp: assuming signed data in \"%s\"\n",
					data->detachname);
				data->mem = __ops_memory_new();
				__ops_mem_readfile(data->mem, data->detachname);
			}
			valid = check_binary_sig(__ops_mem_len(data->mem),
					__ops_mem_data(data->mem),
					&content->sig,
					__ops_get_pubkey(signer));
			break;

		default:
			OPS_ERROR_1(errors, OPS_E_UNIMPLEMENTED,
				    "No Sig Verification type 0x%02x yet\n",
				    content->sig.info.type);
			break;

		}

		if (valid) {
			add_sig_to_list(&content->sig.info,
					&data->result->valid_sigs,
					&data->result->validc);
		} else {
			OPS_ERROR(errors, OPS_E_V_BAD_SIGNATURE,
					"Bad Signature");
			add_sig_to_list(&content->sig.info,
					&data->result->invalid_sigs,
					&data->result->invalidc);
		}
		break;

		/* ignore these */
	case OPS_PARSER_PTAG:
	case OPS_PTAG_CT_SIGNATURE_HEADER:
	case OPS_PTAG_CT_ARMOUR_HEADER:
	case OPS_PTAG_CT_ARMOUR_TRAILER:
	case OPS_PTAG_CT_1_PASS_SIG:
		break;

	case OPS_PARSER_PACKET_END:
		break;

	default:
		OPS_ERROR(errors, OPS_E_V_NO_SIGNATURE, "No signature");
		break;
	}
	return OPS_RELEASE_MEMORY;
}

static void 
keydata_destroyer(__ops_reader_t *readinfo)
{
	(void) free(__ops_reader_get_arg(readinfo));
}

void 
__ops_keydata_reader_set(__ops_parseinfo_t *pinfo, const __ops_keydata_t *key)
{
	validate_reader_t *data = calloc(1, sizeof(*data));

	data->key = key;
	data->packet = 0;
	data->offset = 0;
	__ops_reader_set(pinfo, keydata_reader, keydata_destroyer, data);
}

/**
 * \ingroup HighLevel_Verify
 * \brief Indicicates whether any errors were found
 * \param result Validation result to check
 * \return 0 if any invalid signatures or unknown signers
 	or no valid signatures; else 1
 */
static unsigned 
validate_result_status(__ops_validation_t *val)
{
	return val->validc && !val->invalidc && !val->unknownc;
}

/**
 * \ingroup HighLevel_Verify
 * \brief Validate all signatures on a single key against the given keyring
 * \param result Where to put the result
 * \param key Key to validate
 * \param keyring Keyring to use for validation
 * \param cb_get_passphrase Callback to use to get passphrase
 * \return 1 if all signatures OK; else 0
 * \note It is the caller's responsiblity to free result after use.
 * \sa __ops_validate_result_free()
 */
unsigned 
__ops_validate_key_sigs(__ops_validation_t *result,
	const __ops_keydata_t *key,
	const __ops_keyring_t *keyring,
	__ops_cb_ret_t cb_get_passphrase(const __ops_packet_t *,
						__ops_cbdata_t *))
{
	__ops_parseinfo_t	*pinfo;
	validate_key_cb_t	 keysigs;
	const int		 printerrors = 1;

	(void) memset(&keysigs, 0x0, sizeof(keysigs));
	keysigs.result = result;
	keysigs.getpassphrase = cb_get_passphrase;

	pinfo = __ops_parseinfo_new();
	/* __ops_parse_options(&opt,OPS_PTAG_CT_SIGNATURE,OPS_PARSE_PARSED); */

	keysigs.keyring = keyring;

	__ops_set_callback(pinfo, __ops_validate_key_cb, &keysigs);
	pinfo->readinfo.accumulate = 1;
	__ops_keydata_reader_set(pinfo, key);

	/* Note: Coverity incorrectly reports an error that keysigs.reader */
	/* is never used. */
	keysigs.reader = pinfo->readinfo.arg;

	__ops_parse(pinfo, !printerrors);

	__ops_pubkey_free(&keysigs.pubkey);
	if (keysigs.subkey.version) {
		__ops_pubkey_free(&keysigs.subkey);
	}
	__ops_userid_free(&keysigs.userid);
	__ops_userattr_free(&keysigs.userattr);

	__ops_parseinfo_delete(pinfo);

	return (!result->invalidc && !result->unknownc && result->validc);
}

/**
   \ingroup HighLevel_Verify
   \param result Where to put the result
   \param ring Keyring to use
   \param cb_get_passphrase Callback to use to get passphrase
   \note It is the caller's responsibility to free result after use.
   \sa __ops_validate_result_free()
*/
unsigned 
__ops_validate_all_sigs(__ops_validation_t *result,
	    const __ops_keyring_t *ring,
	    __ops_cb_ret_t cb_get_passphrase(const __ops_packet_t *,
	    					__ops_cbdata_t *))
{
	int	n;

	(void) memset(result, 0x0, sizeof(*result));
	for (n = 0; n < ring->nkeys; ++n) {
		__ops_validate_key_sigs(result, &ring->keys[n], ring,
				cb_get_passphrase);
	}
	return validate_result_status(result);
}

/**
   \ingroup HighLevel_Verify
   \brief Frees validation result and associated memory
   \param result Struct to be freed
   \note Must be called after validation functions
*/
void 
__ops_validate_result_free(__ops_validation_t *result)
{
	if (result != NULL) {
		if (result->valid_sigs) {
			free_sig_info(result->valid_sigs);
		}
		if (result->invalid_sigs) {
			free_sig_info(result->invalid_sigs);
		}
		if (result->unknown_sigs) {
			free_sig_info(result->unknown_sigs);
		}
		(void) free(result);
		result = NULL;
	}
}

/**
   \ingroup HighLevel_Verify
   \brief Verifies the signatures in a signed file
   \param result Where to put the result
   \param filename Name of file to be validated
   \param armoured Treat file as armoured, if set
   \param keyring Keyring to use
   \return 1 if signatures validate successfully;
   	0 if signatures fail or there are no signatures
   \note After verification, result holds the details of all keys which
   have passed, failed and not been recognised.
   \note It is the caller's responsiblity to call
   	__ops_validate_result_free(result) after use.
*/
unsigned 
__ops_validate_file(__ops_io_t *io,
			__ops_validation_t *result,
			const char *infile,
			const char *outfile,
			const int armoured,
			const __ops_keyring_t *keyring)
{
	validate_data_cb_t	 validation;
	__ops_parseinfo_t	*parse = NULL;
	struct stat		 st;
	const int		 printerrors = 1;
	unsigned		 ret;
	int64_t		 	 sigsize;
	char			 origfile[MAXPATHLEN];
	char			*detachname;
	int			 outfd = 0;
	int			 infd;
	int			 cc;

#define SIG_OVERHEAD	284 /* XXX - depends on sig size? */

	if (stat(infile, &st) < 0) {
		(void) fprintf(io->errs, "can't validate \"%s\"\n", infile);
		return 0;
	}
	sigsize = st.st_size;
	detachname = NULL;
	cc = snprintf(origfile, sizeof(origfile), "%s", infile);
	if (strcmp(&origfile[cc - 4], ".sig") == 0) {
		origfile[cc - 4] = 0x0;
		if (stat(origfile, &st) == 0 &&
		    st.st_size > sigsize - SIG_OVERHEAD) {
			detachname = strdup(origfile);
		}
	}

	(void) memset(&validation, 0x0, sizeof(validation));

	infd = __ops_setup_file_read(io, &parse, infile, &validation,
				validate_data_cb, 1);
	if (infd < 0) {
		return 0;
	}

	validation.detachname = detachname;

	/* Set verification reader and handling options */
	validation.result = result;
	validation.keyring = keyring;
	validation.mem = __ops_memory_new();
	__ops_memory_init(validation.mem, 128);
	/* Note: Coverity incorrectly reports an error that validation.reader */
	/* is never used. */
	validation.reader = parse->readinfo.arg;

	if (armoured) {
		__ops_reader_push_dearmour(parse);
	}

	/* Do the verification */
	__ops_parse(parse, !printerrors);

	/* Tidy up */
	if (armoured) {
		__ops_reader_pop_dearmour(parse);
	}
	__ops_teardown_file_read(parse, infd);

	ret = validate_result_status(result);

	/* this is triggered only for --cat output */
	if (outfile) {
		/* need to send validated output somewhere */
		if (strcmp(outfile, "-") == 0) {
			outfd = STDOUT_FILENO;
		} else {
			outfd = open(outfile, O_WRONLY | O_CREAT, 0666);
		}
		if (outfd < 0) {
			/* even if the signature was good, we can't
			* write the file, so send back a bad return
			* code */
			ret = 0;
		} else if (validate_result_status(result)) {
			unsigned	 len;
			char		*cp;
			int		 i;

			len = __ops_mem_len(validation.mem);
			cp = __ops_mem_data(validation.mem);
			for (i = 0 ; i < (int)len ; i += cc) {
				cc = write(outfd, &cp[i], len - i);
				if (cc < 0) {
					(void) fprintf(io->errs,
						"netpgp: short write\n");
					ret = 0;
					break;
				}
			}
			if (strcmp(outfile, "-") != 0) {
				(void) close(outfd);
			}
		}
	}
	__ops_memory_free(validation.mem);
	return ret;
}

/**
   \ingroup HighLevel_Verify
   \brief Verifies the signatures in a __ops_memory_t struct
   \param result Where to put the result
   \param mem Memory to be validated
   \param armoured Treat data as armoured, if set
   \param keyring Keyring to use
   \return 1 if signature validates successfully; 0 if not
   \note After verification, result holds the details of all keys which
   have passed, failed and not been recognised.
   \note It is the caller's responsiblity to call
   	__ops_validate_result_free(result) after use.
*/

unsigned 
__ops_validate_mem(__ops_io_t *io,
			__ops_validation_t *result,
			__ops_memory_t *mem,
			const int armoured,
			const __ops_keyring_t *keyring)
{
	validate_data_cb_t	 validation;
	__ops_parseinfo_t	*pinfo = NULL;
	const int		 printerrors = 1;

	__ops_setup_memory_read(io, &pinfo, mem, &validation, validate_data_cb,				1);
	/* Set verification reader and handling options */
	(void) memset(&validation, 0x0, sizeof(validation));
	validation.result = result;
	validation.keyring = keyring;
	validation.mem = __ops_memory_new();
	__ops_memory_init(validation.mem, 128);
	/* Note: Coverity incorrectly reports an error that validation.reader */
	/* is never used. */
	validation.reader = pinfo->readinfo.arg;

	if (armoured) {
		__ops_reader_push_dearmour(pinfo);
	}

	/* Do the verification */
	__ops_parse(pinfo, !printerrors);

	/* Tidy up */
	if (armoured) {
		__ops_reader_pop_dearmour(pinfo);
	}
	__ops_teardown_memory_read(pinfo, mem);
	__ops_memory_free(validation.mem);

	return validate_result_status(result);
}
