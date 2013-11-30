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

#ifndef CRYPTO_H_
#define CRYPTO_H_

#include "keyring.h"
#include "packet.h"
#include "packet-parse.h"

#include <openssl/dsa.h>

#define OPS_MIN_HASH_SIZE	16

typedef void __ops_hash_init_t(__ops_hash_t *);
typedef void __ops_hash_add_t(__ops_hash_t *, const unsigned char *, unsigned);
typedef unsigned __ops_hash_finish_t(__ops_hash_t *, unsigned char *);

/** _ops_hash_t */
struct _ops_hash_t {
	__ops_hash_alg_t	 alg;		/* algorithm */
	size_t			 size;		/* size */
	const char		*name;		/* what it's known as */
	__ops_hash_init_t	*init;		/* initialisation func */
	__ops_hash_add_t	*add;		/* add text func */
	__ops_hash_finish_t	*finish;	/* finalise func */
	void		 	*data;		/* blob for data */
};

typedef void __ops_setiv_func_t(__ops_crypt_t *, const unsigned char *);
typedef void __ops_setkey_func_t(__ops_crypt_t *, const unsigned char *);
typedef void __ops_crypt_init_t(__ops_crypt_t *);
typedef void __ops_crypt_resync_t(__ops_crypt_t *);
typedef void __ops_blkenc_t(__ops_crypt_t *, void *, const void *);
typedef void __ops_blkdec_t(__ops_crypt_t *, void *, const void *);
typedef void __ops_crypt_cfb_encrypt_t(__ops_crypt_t *, void *, const void *,
					size_t);
typedef void __ops_crypt_cfb_decrypt_t(__ops_crypt_t *, void *, const void *,
					size_t);
typedef void __ops_crypt_finish_t(__ops_crypt_t *);

/** _ops_crypt_t */
struct _ops_crypt_t {
	__ops_symm_alg_t		alg;
	size_t				blocksize;
	size_t				keysize;
	__ops_setiv_func_t		*set_iv;/* Call before decrypt init! */
	__ops_setkey_func_t		*set_key;/* Call this before init! */
	__ops_crypt_init_t		*base_init;
	__ops_crypt_resync_t		*decrypt_resync;
	/* encrypt/decrypt one block  */
	__ops_blkenc_t			*block_encrypt;
	__ops_blkdec_t			*block_decrypt;
	/* Standard CFB encrypt/decrypt (as used by Sym Enc Int Prot packets) */
	__ops_crypt_cfb_encrypt_t	*cfb_encrypt;
	__ops_crypt_cfb_decrypt_t	*cfb_decrypt;
	__ops_crypt_finish_t		*decrypt_finish;
	unsigned char			iv[OPS_MAX_BLOCK_SIZE];
	unsigned char			civ[OPS_MAX_BLOCK_SIZE];
	unsigned char			siv[OPS_MAX_BLOCK_SIZE];
		/* siv is needed for weird v3 resync */
	unsigned char			key[OPS_MAX_KEY_SIZE];
	int				num;
		/* num is offset - see openssl _encrypt doco */
	void				*encrypt_key;
	void				*decrypt_key;
};

void __ops_crypto_init(void);
void __ops_crypto_finish(void);
void __ops_hash_md5(__ops_hash_t *);
void __ops_hash_sha1(__ops_hash_t *);
void __ops_hash_sha256(__ops_hash_t *);
void __ops_hash_sha512(__ops_hash_t *);
void __ops_hash_sha384(__ops_hash_t *);
void __ops_hash_sha224(__ops_hash_t *);
void __ops_hash_any(__ops_hash_t *, __ops_hash_alg_t);
__ops_hash_alg_t __ops_str_to_hash_alg(const char *);
const char *__ops_text_from_hash(__ops_hash_t *);
unsigned __ops_hash_size(__ops_hash_alg_t);
unsigned __ops_hash(unsigned char *, __ops_hash_alg_t, const void *, size_t);

void __ops_hash_add_int(__ops_hash_t *, unsigned, unsigned);

unsigned __ops_dsa_verify(const unsigned char *, size_t,
			const __ops_dsa_sig_t *,
			const __ops_dsa_pubkey_t *);

int __ops_rsa_public_decrypt(unsigned char *, const unsigned char *, size_t,
			const __ops_rsa_pubkey_t *);
int __ops_rsa_public_encrypt(unsigned char *, const unsigned char *, size_t,
			const __ops_rsa_pubkey_t *);

int __ops_rsa_private_encrypt(unsigned char *, const unsigned char *, size_t,
			const __ops_rsa_seckey_t *, const __ops_rsa_pubkey_t *);
int __ops_rsa_private_decrypt(unsigned char *, const unsigned char *, size_t,
			const __ops_rsa_seckey_t *, const __ops_rsa_pubkey_t *);

unsigned __ops_block_size(__ops_symm_alg_t);
unsigned __ops_key_size(__ops_symm_alg_t);

int __ops_decrypt_data(__ops_content_tag_t, __ops_region_t *,
			__ops_parseinfo_t *);

int __ops_crypt_any(__ops_crypt_t *, __ops_symm_alg_t);
void __ops_decrypt_init(__ops_crypt_t *);
void __ops_encrypt_init(__ops_crypt_t *);
size_t __ops_decrypt_se(__ops_crypt_t *, void *, const void *, size_t);
size_t __ops_encrypt_se(__ops_crypt_t *, void *, const void *, size_t);
size_t __ops_decrypt_se_ip(__ops_crypt_t *, void *, const void *, size_t);
size_t __ops_encrypt_se_ip(__ops_crypt_t *, void *, const void *, size_t);
unsigned __ops_is_sa_supported(__ops_symm_alg_t);

void __ops_reader_push_decrypt(__ops_parseinfo_t *, __ops_crypt_t *,
			__ops_region_t *);
void __ops_reader_pop_decrypt(__ops_parseinfo_t *);

/* Hash everything that's read */
void __ops_reader_push_hash(__ops_parseinfo_t *, __ops_hash_t *);
void __ops_reader_pop_hash(__ops_parseinfo_t *);

int __ops_decrypt_decode_mpi(unsigned char *, unsigned, const BIGNUM *,
			const __ops_seckey_t *);
unsigned __ops_rsa_encrypt_mpi(const unsigned char *, const size_t,
			const __ops_pubkey_t *,
			__ops_pk_sesskey_parameters_t *);

/* Encrypt everything that's written */
struct __ops_key_data;
void __ops_writer_push_encrypt(__ops_output_t *,
			const struct __ops_key_data *);

unsigned   __ops_encrypt_file(__ops_io_t *, const char *, const char *,
			const __ops_keydata_t *,
			const unsigned, const unsigned);
unsigned   __ops_decrypt_file(__ops_io_t *,
			const char *,
			const char *,
			__ops_keyring_t *,
			const unsigned,
			const unsigned,
			__ops_cbfunc_t *);

/* Keys */
__ops_keydata_t  *__ops_rsa_new_selfsign_key(const int,
			const unsigned long, __ops_userid_t *);

int __ops_dsa_size(const __ops_dsa_pubkey_t *);
DSA_SIG *__ops_dsa_sign(unsigned char *, unsigned,
				const __ops_dsa_seckey_t *,
				const __ops_dsa_pubkey_t *);

/** __ops_reader_t */
struct __ops_reader_t {
	__ops_reader_func_t	*reader; /* reader func to get parse data */
	__ops_reader_destroyer_t *destroyer;
	void			*arg;	/* args to pass to reader function */
	unsigned		 accumulate:1;	/* set to gather packet data */
	unsigned char		*accumulated;	/* the accumulated data */
	unsigned		 asize;	/* size of the buffer */
	unsigned		 alength;/* used buffer */
	unsigned		 position;	/* reader-specific offset */
	__ops_reader_t		*next;
	__ops_parseinfo_t	*parent;/* parent parse_info structure */
};


/** __ops_cryptinfo_t
 Encrypt/decrypt settings
*/
struct __ops_cryptinfo_t {
	char			*passphrase;
	__ops_keyring_t		*keyring;
	const __ops_keydata_t	*keydata;
	__ops_cbfunc_t		*getpassphrase;
};

/** __ops_cbdata_t */
struct __ops_cbdata_t {
	__ops_cbfunc_t		*cbfunc;	/* callback function */
	void			*arg;	/* args to pass to callback func */
	__ops_error_t		**errors; /* address of error stack */
	__ops_cbdata_t		*next;
	__ops_output_t		*output;/* used if writing out parsed info */
	__ops_io_t		*io;		/* error/output messages */
	__ops_cryptinfo_t	 cryptinfo;	/* used when decrypting */
};

/** __ops_hashtype_t */
typedef struct {
	__ops_hash_t	hash;	/* hashes we should hash data with */
	unsigned char	keyid[OPS_KEY_ID_SIZE];
} __ops_hashtype_t;

#define NTAGS	0x100	/* == 256 */

/** \brief Structure to hold information about a packet parse.
 *
 *  This information includes options about the parse:
 *  - whether the packet contents should be accumulated or not
 *  - whether signature subpackets should be parsed or left raw
 *
 *  It contains options specific to the parsing of armoured data:
 *  - whether headers are allowed in armoured data without a gap
 *  - whether a blank line is allowed at the start of the armoured data
 *
 *  It also specifies :
 *  - the callback function to use and its arguments
 *  - the reader function to use and its arguments
 *
 *  It also contains information about the current state of the parse:
 *  - offset from the beginning
 *  - the accumulated data, if any
 *  - the size of the buffer, and how much has been used
 *
 *  It has a linked list of errors.
 */

struct __ops_parseinfo_t {
	unsigned char		 ss_raw[NTAGS / 8];
		/* 1 bit / sig-subpkt type; set to get raw data */
	unsigned char		 ss_parsed[NTAGS / 8];
		/* 1 bit / sig-subpkt type; set to get parsed data */
	__ops_reader_t	 	 readinfo;
	__ops_cbdata_t		 cbinfo;
	__ops_error_t		*errors;
	void			*io;		/* io streams */
	__ops_crypt_t		 decrypt;
	__ops_cryptinfo_t	 cryptinfo;
	size_t			 nhashes;
	__ops_hashtype_t        *hashes;
	unsigned		 reading_v3_secret:1;
	unsigned		 reading_mpi_len:1;
	unsigned		 exact_read:1;
};

#endif /* CRYPTO_H_ */
