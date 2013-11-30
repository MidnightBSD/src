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

/** \file
 */
#include "config.h"

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

#if defined(__NetBSD__)
__COPYRIGHT("@(#) Copyright (c) 2009 The NetBSD Foundation, Inc. All rights reserved.");
__RCSID("$NetBSD: keyring.c,v 1.15 2009/05/31 23:26:20 agc Exp $");
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "types.h"
#include "keyring.h"
#include "packet-parse.h"
#include "signature.h"
#include "netpgpsdk.h"
#include "readerwriter.h"
#include "netpgpdefs.h"
#include "packet.h"
#include "crypto.h"
#include "validate.h"
#include "netpgpdigest.h"



/**
   \ingroup HighLevel_Keyring

   \brief Creates a new __ops_keydata_t struct

   \return A new __ops_keydata_t struct, initialised to zero.

   \note The returned __ops_keydata_t struct must be freed after use with __ops_keydata_free.
*/

__ops_keydata_t  *
__ops_keydata_new(void)
{
	return calloc(1, sizeof(__ops_keydata_t));
}


/**
 \ingroup HighLevel_Keyring

 \brief Frees keydata and its memory

 \param keydata Key to be freed.

 \note This frees the keydata itself, as well as any other memory alloc-ed by it.
*/
void 
__ops_keydata_free(__ops_keydata_t *keydata)
{
	unsigned        n;

	for (n = 0; n < keydata->nuids; ++n) {
		__ops_userid_free(&keydata->uids[n]);
	}
	(void) free(keydata->uids);
	keydata->uids = NULL;
	keydata->nuids = 0;

	for (n = 0; n < keydata->npackets; ++n) {
		__ops_subpacket_free(&keydata->packets[n]);
	}
	(void) free(keydata->packets);
	keydata->packets = NULL;
	keydata->npackets = 0;

	if (keydata->type == OPS_PTAG_CT_PUBLIC_KEY) {
		__ops_pubkey_free(&keydata->key.pubkey);
	} else {
		__ops_seckey_free(&keydata->key.seckey);
	}

	(void) free(keydata);
}

/**
 \ingroup HighLevel_KeyGeneral

 \brief Returns the public key in the given keydata.
 \param keydata

  \return Pointer to public key

  \note This is not a copy, do not free it after use.
*/

const __ops_pubkey_t *
__ops_get_pubkey(const __ops_keydata_t *keydata)
{
	return (keydata->type == OPS_PTAG_CT_PUBLIC_KEY) ?
				&keydata->key.pubkey :
				&keydata->key.seckey.pubkey;
}

/**
\ingroup HighLevel_KeyGeneral

\brief Check whether this is a secret key or not.
*/

unsigned 
__ops_is_key_secret(const __ops_keydata_t *data)
{
	return data->type != OPS_PTAG_CT_PUBLIC_KEY;
}

/**
 \ingroup HighLevel_KeyGeneral

 \brief Returns the secret key in the given keydata.

 \note This is not a copy, do not free it after use.

 \note This returns a const.  If you need to be able to write to this
 pointer, use __ops_get_writable_seckey
*/

const __ops_seckey_t *
__ops_get_seckey(const __ops_keydata_t *data)
{
	return (data->type == OPS_PTAG_CT_SECRET_KEY) ?
				&data->key.seckey : NULL;
}

/**
 \ingroup HighLevel_KeyGeneral

  \brief Returns the secret key in the given keydata.

  \note This is not a copy, do not free it after use.

  \note If you do not need to be able to modify this key, there is an
  equivalent read-only function __ops_get_seckey.
*/

__ops_seckey_t *
__ops_get_writable_seckey(__ops_keydata_t *data)
{
	return (data->type == OPS_PTAG_CT_SECRET_KEY) ?
				&data->key.seckey : NULL;
}

/* utility function to zero out memory */
void
__ops_forget(void *vp, unsigned size)
{
	(void) memset(vp, 0x0, size);
}

typedef struct {
	const __ops_keydata_t	*key;
	char			*passphrase;
	__ops_seckey_t		*seckey;
} decrypt_t;

static __ops_cb_ret_t 
decrypt_cb(const __ops_packet_t *pkt, __ops_cbdata_t *cbinfo)
{
	const __ops_contents_t	*content = &pkt->u;
	decrypt_t		*decrypt;

	decrypt = __ops_callback_arg(cbinfo);
	switch (pkt->tag) {
	case OPS_PARSER_PTAG:
	case OPS_PTAG_CT_USER_ID:
	case OPS_PTAG_CT_SIGNATURE:
	case OPS_PTAG_CT_SIGNATURE_HEADER:
	case OPS_PTAG_CT_SIGNATURE_FOOTER:
	case OPS_PTAG_CT_TRUST:
		break;

	case OPS_GET_PASSPHRASE:
#if 1
		*content->skey_passphrase.passphrase = decrypt->passphrase;
		return OPS_KEEP_MEMORY;
#else
		cbinfo->cryptinfo.keydata = decrypt->key;
		return get_passphrase_cb(pkt, cbinfo);
#endif

	case OPS_PARSER_ERRCODE:
		switch (content->errcode.errcode) {
		case OPS_E_P_MPI_FORMAT_ERROR:
			/* Generally this means a bad passphrase */
			fprintf(stderr, "Bad passphrase!\n");
			return OPS_RELEASE_MEMORY;

		case OPS_E_P_PACKET_CONSUMED:
			/* And this is because of an error we've accepted */
			return OPS_RELEASE_MEMORY;
		default:
			break;
		}
		(void) fprintf(stderr, "parse error: %s\n",
				__ops_errcode(content->errcode.errcode));
		return OPS_FINISHED;

	case OPS_PARSER_ERROR:
		fprintf(stderr, "parse error: %s\n", content->error.error);
		return OPS_FINISHED;

	case OPS_PTAG_CT_SECRET_KEY:
		decrypt->seckey = calloc(1, sizeof(*decrypt->seckey));
		decrypt->seckey->checkhash = calloc(1, OPS_CHECKHASH_SIZE);
		*decrypt->seckey = content->seckey;
		return OPS_KEEP_MEMORY;

	case OPS_PARSER_PACKET_END:
		/* nothing to do */
		break;

	default:
		fprintf(stderr, "Unexpected tag %d (0x%x)\n", pkt->tag,
			pkt->tag);
		return OPS_FINISHED;
	}

	return OPS_RELEASE_MEMORY;
}

/**
\ingroup Core_Keys
\brief Decrypts secret key from given keydata with given passphrase
\param key Key from which to get secret key
\param passphrase Passphrase to use to decrypt secret key
\return secret key
*/
__ops_seckey_t *
__ops_decrypt_seckey(const __ops_keydata_t *key, const char *passphrase)
{
	__ops_parseinfo_t	*parse;
	const int		 printerrors = 1;
	decrypt_t		 decrypt;

	(void) memset(&decrypt, 0x0, sizeof(decrypt));
	decrypt.key = key;
	decrypt.passphrase = strdup(passphrase);
	parse = __ops_parseinfo_new();
	__ops_keydata_reader_set(parse, key);
	__ops_set_callback(parse, decrypt_cb, &decrypt);
	parse->readinfo.accumulate = 1;
	__ops_parse(parse, !printerrors);
	__ops_forget(decrypt.passphrase, strlen(decrypt.passphrase));
	(void) free(decrypt.passphrase);
	return decrypt.seckey;
}

/**
\ingroup Core_Keys
\brief Set secret key in content
\param content Content to be set
\param key Keydata to get secret key from
*/
void 
__ops_set_seckey(__ops_contents_t *cont, const __ops_keydata_t *key)
{
	*cont->get_seckey.seckey = &key->key.seckey;
}

/**
\ingroup Core_Keys
\brief Get Key ID from keydata
\param key Keydata to get Key ID from
\return Pointer to Key ID inside keydata
*/
const unsigned char *
__ops_get_key_id(const __ops_keydata_t *key)
{
	return key->key_id;
}

/**
\ingroup Core_Keys
\brief How many User IDs in this key?
\param key Keydata to check
\return Num of user ids
*/
unsigned 
__ops_get_userid_count(const __ops_keydata_t *key)
{
	return key->nuids;
}

/**
\ingroup Core_Keys
\brief Get indexed user id from key
\param key Key to get user id from
\param index Which key to get
\return Pointer to requested user id
*/
const unsigned char *
__ops_get_userid(const __ops_keydata_t *key, unsigned subscript)
{
	return key->uids[subscript].userid;
}

/**
   \ingroup HighLevel_Supported
   \brief Checks whether key's algorithm and type are supported by OpenPGP::SDK
   \param keydata Key to be checked
   \return 1 if key algorithm and type are supported by OpenPGP::SDK; 0 if not
*/

unsigned 
__ops_is_key_supported(const __ops_keydata_t *keydata)
{
	if (keydata->type == OPS_PTAG_CT_PUBLIC_KEY) {
		if (keydata->key.pubkey.alg == OPS_PKA_RSA) {
			return 1;
		}
	} else if (keydata->type == OPS_PTAG_CT_PUBLIC_KEY) {
		if (keydata->key.pubkey.alg == OPS_PKA_DSA) {
			return 1;
		}
	}
	return 0;
}

/* \todo check where userid pointers are copied */
/**
\ingroup Core_Keys
\brief Copy user id, including contents
\param dst Destination User ID
\param src Source User ID
\note If dst already has a userid, it will be freed.
*/
void 
__ops_copy_userid(__ops_userid_t *dst, const __ops_userid_t *src)
{
	size_t          len = strlen((char *) src->userid);
	if (dst->userid)
		free(dst->userid);
	dst->userid = calloc(1, len + 1);

	(void) memcpy(dst->userid, src->userid, len);
}

/* \todo check where pkt pointers are copied */
/**
\ingroup Core_Keys
\brief Copy packet, including contents
\param dst Destination packet
\param src Source packet
\note If dst already has a packet, it will be freed.
*/
void 
__ops_copy_packet(__ops_subpacket_t *dst, const __ops_subpacket_t *src)
{
	if (dst->raw) {
		(void) free(dst->raw);
	}
	dst->raw = calloc(1, src->length);
	dst->length = src->length;
	(void) memcpy(dst->raw, src->raw, src->length);
}

/**
\ingroup Core_Keys
\brief Add User ID to keydata
\param keydata Key to which to add User ID
\param userid User ID to add
\return Pointer to new User ID
*/
__ops_userid_t  *
__ops_add_userid(__ops_keydata_t *keydata, const __ops_userid_t *userid)
{
	__ops_userid_t  *new_uid = NULL;

	EXPAND_ARRAY(keydata, uids);

	/* initialise new entry in array */
	new_uid = &keydata->uids[keydata->nuids];

	new_uid->userid = NULL;

	/* now copy it */
	__ops_copy_userid(new_uid, userid);
	keydata->nuids++;

	return new_uid;
}

/**
\ingroup Core_Keys
\brief Add packet to key
\param keydata Key to which to add packet
\param packet Packet to add
\return Pointer to new packet
*/
__ops_subpacket_t   *
__ops_add_subpacket(__ops_keydata_t *keydata, const __ops_subpacket_t *packet)
{
	__ops_subpacket_t   *new_pkt = NULL;

	EXPAND_ARRAY(keydata, packets);

	/* initialise new entry in array */
	new_pkt = &keydata->packets[keydata->npackets];
	new_pkt->length = 0;
	new_pkt->raw = NULL;

	/* now copy it */
	__ops_copy_packet(new_pkt, packet);
	keydata->npackets++;

	return new_pkt;
}

/**
\ingroup Core_Keys
\brief Add signed User ID to key
\param keydata Key to which to add signed User ID
\param userid User ID to add
\param sigpacket Packet to add
*/
void 
__ops_add_signed_userid(__ops_keydata_t *keydata,
		const __ops_userid_t *userid,
		const __ops_subpacket_t *sigpacket)
{
	__ops_subpacket_t	*pkt = NULL;
	__ops_userid_t		*uid = NULL;

	uid = __ops_add_userid(keydata, userid);
	pkt = __ops_add_subpacket(keydata, sigpacket);

	/*
         * add entry in sigs array to link the userid and sigpacket
	 * and add ptr to it from the sigs array */
	EXPAND_ARRAY(keydata, sigs);

	/**setup new entry in array */
	keydata->sigs[keydata->nsigs].userid = uid;
	keydata->sigs[keydata->nsigs].packet = pkt;

	keydata->nsigs++;
}

/**
\ingroup Core_Keys
\brief Add selfsigned User ID to key
\param keydata Key to which to add user ID
\param userid Self-signed User ID to add
\return 1 if OK; else 0
*/
unsigned 
__ops_add_selfsigned_userid(__ops_keydata_t *keydata, __ops_userid_t *userid)
{
	__ops_create_sig_t	*sig = NULL;
	__ops_subpacket_t	 sigpacket;
	__ops_memory_t		*mem_userid = NULL;
	__ops_output_t		*useridoutput = NULL;
	__ops_memory_t		*mem_sig = NULL;
	__ops_output_t		*sigoutput = NULL;

	/*
         * create signature packet for this userid
         */

	/* create userid pkt */
	__ops_setup_memory_write(&useridoutput, &mem_userid, 128);
	__ops_write_struct_userid(useridoutput, userid);

	/* create sig for this pkt */
	sig = __ops_create_sig_new();
	__ops_sig_start_key_sig(sig, &keydata->key.seckey.pubkey, userid,
					OPS_CERT_POSITIVE);
	__ops_add_birthtime(sig, time(NULL));
	__ops_add_issuer_keyid(sig, keydata->key_id);
	__ops_add_primary_userid(sig, 1);
	__ops_end_hashed_subpkts(sig);

	__ops_setup_memory_write(&sigoutput, &mem_sig, 128);
	__ops_write_sig(sigoutput, sig, &keydata->key.seckey.pubkey,
				&keydata->key.seckey);

	/* add this packet to keydata */
	sigpacket.length = __ops_mem_len(mem_sig);
	sigpacket.raw = __ops_mem_data(mem_sig);

	/* add userid to keydata */
	__ops_add_signed_userid(keydata, userid, &sigpacket);

	/* cleanup */
	__ops_create_sig_delete(sig);
	__ops_output_delete(useridoutput);
	__ops_output_delete(sigoutput);
	__ops_memory_free(mem_userid);
	__ops_memory_free(mem_sig);

	return 1;
}

/**
\ingroup Core_Keys
\brief Initialise __ops_keydata_t
\param keydata Keydata to initialise
\param type OPS_PTAG_CT_PUBLIC_KEY or OPS_PTAG_CT_SECRET_KEY
*/
void 
__ops_keydata_init(__ops_keydata_t *keydata, const __ops_content_tag_t type)
{
	if (keydata->type != OPS_PTAG_CT_RESERVED) {
		(void) fprintf(stderr,
			"__ops_keydata_init: wrong keydata type\n");
	} else if (type != OPS_PTAG_CT_PUBLIC_KEY &&
		   type != OPS_PTAG_CT_SECRET_KEY) {
		(void) fprintf(stderr, "__ops_keydata_init: wrong type\n");
	} else {
		keydata->type = type;
	}
}


static __ops_cb_ret_t
cb_keyring_read(const __ops_packet_t *pkt, __ops_cbdata_t *cbinfo)
{
	__OPS_USED(cbinfo);

	switch (pkt->tag) {
	case OPS_PARSER_PTAG:
	case OPS_PTAG_CT_ENCRYPTED_SECRET_KEY:	/* we get these because we
						 * didn't prompt */
	case OPS_PTAG_CT_SIGNATURE_HEADER:
	case OPS_PTAG_CT_SIGNATURE_FOOTER:
	case OPS_PTAG_CT_SIGNATURE:
	case OPS_PTAG_CT_TRUST:
	case OPS_PARSER_ERRCODE:
		break;

	default:
		;
	}

	return OPS_RELEASE_MEMORY;
}

/**
   \ingroup HighLevel_KeyringRead

   \brief Reads a keyring from a file

   \param keyring Pointer to an existing __ops_keyring_t struct
   \param armour 1 if file is armoured; else 0
   \param filename Filename of keyring to be read

   \return __ops 1 if OK; 0 on error

   \note Keyring struct must already exist.

   \note Can be used with either a public or secret keyring.

   \note You must call __ops_keyring_free() after usage to free alloc-ed memory.

   \note If you call this twice on the same keyring struct, without calling
   __ops_keyring_free() between these calls, you will introduce a memory leak.

   \sa __ops_keyring_read_from_mem()
   \sa __ops_keyring_free()

*/

unsigned 
__ops_keyring_fileread(__ops_keyring_t *keyring,
			const unsigned armour,
			const char *filename)
{
	__ops_parseinfo_t	*parse;
	unsigned		 res = 1;
	int			 fd;

	parse = __ops_parseinfo_new();

	/* add this for the moment, */
	/*
	 * \todo need to fix the problems with reading signature subpackets
	 * later
	 */

	/* __ops_parse_options(parse,OPS_PTAG_SS_ALL,OPS_PARSE_RAW); */
	__ops_parse_options(parse, OPS_PTAG_SS_ALL, OPS_PARSE_PARSED);

#ifdef O_BINARY
	fd = open(filename, O_RDONLY | O_BINARY);
#else
	fd = open(filename, O_RDONLY);
#endif
	if (fd < 0) {
		__ops_parseinfo_delete(parse);
		perror(filename);
		return 0;
	}
#ifdef USE_MMAP_FOR_FILES
	__ops_reader_set_mmap(parse, fd);
#else
	__ops_reader_set_fd(parse, fd);
#endif

	__ops_set_callback(parse, cb_keyring_read, NULL);

	if (armour) {
		__ops_reader_push_dearmour(parse);
	}
	if (__ops_parse_and_accumulate(keyring, parse) == 0) {
		res = 0;
	} else {
		res = 1;
	}
	__ops_print_errors(__ops_parseinfo_get_errors(parse));

	if (armour)
		__ops_reader_pop_dearmour(parse);

	close(fd);

	__ops_parseinfo_delete(parse);

	return res;
}

/**
   \ingroup HighLevel_KeyringRead

   \brief Reads a keyring from memory

   \param keyring Pointer to existing __ops_keyring_t struct
   \param armour 1 if file is armoured; else 0
   \param mem Pointer to a __ops_memory_t struct containing keyring to be read

   \return __ops 1 if OK; 0 on error

   \note Keyring struct must already exist.

   \note Can be used with either a public or secret keyring.

   \note You must call __ops_keyring_free() after usage to free alloc-ed memory.

   \note If you call this twice on the same keyring struct, without calling
   __ops_keyring_free() between these calls, you will introduce a memory leak.

   \sa __ops_keyring_fileread
   \sa __ops_keyring_free
*/
unsigned 
__ops_keyring_read_from_mem(__ops_io_t *io,
				__ops_keyring_t *keyring,
				const unsigned armour,
				__ops_memory_t *mem)
{
	__ops_parseinfo_t	*parse = NULL;
	const unsigned		 noaccum = 0;
	unsigned		 res = 1;

	parse = __ops_parseinfo_new();
	__ops_parse_options(parse, OPS_PTAG_SS_ALL, OPS_PARSE_PARSED);
	__ops_setup_memory_read(io, &parse, mem, NULL, cb_keyring_read,
					noaccum);
	if (armour) {
		__ops_reader_push_dearmour(parse);
	}
	res = __ops_parse_and_accumulate(keyring, parse);
	__ops_print_errors(__ops_parseinfo_get_errors(parse));
	if (armour) {
		__ops_reader_pop_dearmour(parse);
	}
	/* don't call teardown_memory_read because memory was passed in */
	__ops_parseinfo_delete(parse);
	return res;
}

/**
   \ingroup HighLevel_KeyringRead

   \brief Frees keyring's contents (but not keyring itself)

   \param keyring Keyring whose data is to be freed

   \note This does not free keyring itself, just the memory alloc-ed in it.
 */
void 
__ops_keyring_free(__ops_keyring_t *keyring)
{
	(void)free(keyring->keys);
	keyring->keys = NULL;
	keyring->nkeys = 0;
	keyring->nkeys_allocated = 0;
}

/**
   \ingroup HighLevel_KeyringFind

   \brief Finds key in keyring from its Key ID

   \param keyring Keyring to be searched
   \param keyid ID of required key

   \return Pointer to key, if found; NULL, if not found

   \note This returns a pointer to the key inside the given keyring,
   not a copy.  Do not free it after use.

*/
const __ops_keydata_t *
__ops_getkeybyid(__ops_io_t *io, const __ops_keyring_t *keyring,
			   const unsigned char keyid[OPS_KEY_ID_SIZE])
{
	int	n;

	for (n = 0; keyring && n < keyring->nkeys; n++) {
		if (__ops_get_debug_level(__FILE__)) {
			int	i;

			(void) fprintf(io->errs,
				"__ops_getkeybyid: keyring keyid ");
			for (i = 0 ; i < OPS_KEY_ID_SIZE ; i++) {
				(void) fprintf(io->errs, "%02x",
					keyring->keys[n].key_id[i]);
			}
			(void) fprintf(io->errs, ", keyid ");
			for (i = 0 ; i < OPS_KEY_ID_SIZE ; i++) {
				(void) fprintf(io->errs, "%02x", keyid[i]);
			}
			(void) fprintf(io->errs, "\n");
		}
		if (memcmp(keyring->keys[n].key_id, keyid, OPS_KEY_ID_SIZE) == 0) {
			return &keyring->keys[n];
		}
		if (memcmp(&keyring->keys[n].key_id[OPS_KEY_ID_SIZE / 2],
				keyid, OPS_KEY_ID_SIZE / 2) == 0) {
			return &keyring->keys[n];
		}
	}
	return NULL;
}

/* convert a string keyid into a binary keyid */
static void
str2keyid(const char *userid, unsigned char *keyid, size_t len)
{
	static const char	*uppers = "0123456789ABCDEF";
	static const char	*lowers = "0123456789abcdef";
	unsigned char		 hichar;
	unsigned char		 lochar;
	size_t			 j;
	const char		*hi;
	const char		*lo;
	int			 i;

	for (i = j = 0 ; j < len && userid[i] && userid[i + 1] ; i += 2, j++) {
		if ((hi = strchr(uppers, userid[i])) == NULL) {
			if ((hi = strchr(lowers, userid[i])) == NULL) {
				break;
			}
			hichar = (hi - lowers);
		} else {
			hichar = (hi - uppers);
		}
		if ((lo = strchr(uppers, userid[i + 1])) == NULL) {
			if ((lo = strchr(lowers, userid[i + 1])) == NULL) {
				break;
			}
			lochar = (lo - lowers);
		} else {
			lochar = (lo - uppers);
		}
		keyid[j] = (hichar << 4) | (lochar);
	}
	keyid[j] = 0x0;
}

/**
   \ingroup HighLevel_KeyringFind

   \brief Finds key from its User ID

   \param keyring Keyring to be searched
   \param userid User ID of required key

   \return Pointer to Key, if found; NULL, if not found

   \note This returns a pointer to the key inside the keyring, not a
   copy.  Do not free it.

*/
const __ops_keydata_t *
__ops_getkeybyname(__ops_io_t *io,
			const __ops_keyring_t *keyring,
			const char *name)
{
	const __ops_keydata_t	*kp;
	__ops_keydata_t		*keyp;
	__ops_userid_t		*uidp;
	unsigned char		 keyid[OPS_KEY_ID_SIZE + 1];
	unsigned int    	 i = 0;
	size_t          	 len;
	char	                *cp;
	int             	 n = 0;

	if (!keyring) {
		return NULL;
	}
	len = strlen(name);
	for (n = 0, keyp = keyring->keys; n < keyring->nkeys; ++n, keyp++) {
		for (i = 0, uidp = keyp->uids; i < keyp->nuids; i++, uidp++) {
			if (__ops_get_debug_level(__FILE__)) {
				(void) fprintf(io->outs,
					"[%d][%d] name %s, last '%d'\n",
					n, i, uidp->userid, uidp->userid[len]);
			}
			if (strncmp((char *) uidp->userid, name, len) == 0 &&
			    uidp->userid[len] == ' ') {
				return keyp;
			}
		}
	}

	if (strchr(name, '@') == NULL) {
		/* no '@' sign */
		/* first try name as a keyid */
		(void) memset(keyid, 0x0, sizeof(keyid));
		str2keyid(name, keyid, sizeof(keyid));
		if (__ops_get_debug_level(__FILE__)) {
			(void) fprintf(io->outs,
				"name \"%s\", keyid %02x%02x%02x%02x\n",
				name,
				keyid[0], keyid[1], keyid[2], keyid[3]);
		}
		if ((kp = __ops_getkeybyid(io, keyring, keyid)) != NULL) {
			return kp;
		}
		/* match on full name */
		keyp = keyring->keys;
		for (n = 0; n < keyring->nkeys; ++n, keyp++) {
			uidp = keyp->uids;
			for (i = 0 ; i < keyp->nuids; i++, uidp++) {
				if (__ops_get_debug_level(__FILE__)) {
					(void) fprintf(io->outs,
						"keyid \"%s\" len %"
						PRIsize "u, keyid[len] '%c'\n",
					       (char *) uidp->userid,
					       len, uidp->userid[len]);
				}
				if (strncasecmp((char *) uidp->userid, name,
					len) == 0 && uidp->userid[len] == ' ') {
					return keyp;
				}
			}
		}
	}
	/* match on <email@address> */
	for (n = 0, keyp = keyring->keys; n < keyring->nkeys; ++n, keyp++) {
		for (i = 0, uidp = keyp->uids; i < keyp->nuids; i++, uidp++) {
			/*
			 * look for the rightmost '<', in case there is one
			 * in the comment field
			 */
			cp = strrchr((char *) uidp->userid, '<');
			if (cp != NULL) {
				if (__ops_get_debug_level(__FILE__)) {
					(void) fprintf(io->errs,
						"cp ,%s, name ,%s, len %"
						PRIsize "u ,%c,\n",
						cp + 1,
						name,
						len,
						*(cp + len + 1));
				}
				if (strncasecmp(cp + 1, name, len) == 0 &&
				    *(cp + len + 1) == '>') {
					return keyp;
				}
			}
		}
	}
	return NULL;
}

/**
   \ingroup HighLevel_KeyringList

   \brief Prints all keys in keyring to stdout.

   \param keyring Keyring to use

   \return none
*/
int
__ops_keyring_list(__ops_io_t *io, const __ops_keyring_t *keyring)
{
	__ops_keydata_t		*key;
	int			 n;

	(void) fprintf(io->outs, "%d keys\n", keyring->nkeys);
	for (n = 0, key = &keyring->keys[n]; n < keyring->nkeys; ++n, ++key) {
		if (__ops_is_key_secret(key)) {
			__ops_print_seckeydata(key);
		} else {
			__ops_print_pubkeydata(io, key);
		}
		(void) fputc('\n', io->outs);
	}
	return 1;
}

static unsigned
get_contents_type(const __ops_keydata_t *keydata)
{
	return keydata->type;
}

/* this interface isn't right - hook into callback for getting passphrase */
int
__ops_export_key(const __ops_keydata_t *keydata, unsigned char *passphrase)
{
	__ops_output_t	*output;
	__ops_memory_t		*mem;

	__ops_setup_memory_write(&output, &mem, 128);
	if (get_contents_type(keydata) == OPS_PTAG_CT_PUBLIC_KEY) {
		__ops_write_xfer_pubkey(output, keydata, 1);
	} else {
		__ops_write_xfer_seckey(output, keydata, passphrase,
					strlen((char *)passphrase), 1);
	}
	printf("%s", (char *) __ops_mem_data(mem));
	__ops_teardown_memory_write(output, mem);
	return 1;
}
