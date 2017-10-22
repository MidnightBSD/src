/*	$OpenBSD: xform.c,v 1.16 2001/08/28 12:20:43 ben Exp $	*/
/*-
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * This code was written by John Ioannidis for BSD/OS in Athens, Greece,
 * in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Additional features in 1999 by Angelos D. Keromytis.
 *
 * Copyright (C) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 *
 * Copyright (C) 2001, Angelos D. Keromytis.
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <machine/cpu.h>

#include <crypto/blowfish/blowfish.h>
#include <crypto/des/des.h>
#include <crypto/rijndael/rijndael.h>
#include <crypto/camellia/camellia.h>
#include <crypto/sha1.h>

#include <opencrypto/cast.h>
#include <opencrypto/deflate.h>
#include <opencrypto/rmd160.h>
#include <opencrypto/skipjack.h>

#include <sys/md5.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>

static	int null_setkey(u_int8_t **, u_int8_t *, int);
static	int des1_setkey(u_int8_t **, u_int8_t *, int);
static	int des3_setkey(u_int8_t **, u_int8_t *, int);
static	int blf_setkey(u_int8_t **, u_int8_t *, int);
static	int cast5_setkey(u_int8_t **, u_int8_t *, int);
static	int skipjack_setkey(u_int8_t **, u_int8_t *, int);
static	int rijndael128_setkey(u_int8_t **, u_int8_t *, int);
static	int aes_xts_setkey(u_int8_t **, u_int8_t *, int);
static	int cml_setkey(u_int8_t **, u_int8_t *, int);

static	void null_encrypt(caddr_t, u_int8_t *);
static	void des1_encrypt(caddr_t, u_int8_t *);
static	void des3_encrypt(caddr_t, u_int8_t *);
static	void blf_encrypt(caddr_t, u_int8_t *);
static	void cast5_encrypt(caddr_t, u_int8_t *);
static	void skipjack_encrypt(caddr_t, u_int8_t *);
static	void rijndael128_encrypt(caddr_t, u_int8_t *);
static	void aes_xts_encrypt(caddr_t, u_int8_t *);
static	void cml_encrypt(caddr_t, u_int8_t *);

static	void null_decrypt(caddr_t, u_int8_t *);
static	void des1_decrypt(caddr_t, u_int8_t *);
static	void des3_decrypt(caddr_t, u_int8_t *);
static	void blf_decrypt(caddr_t, u_int8_t *);
static	void cast5_decrypt(caddr_t, u_int8_t *);
static	void skipjack_decrypt(caddr_t, u_int8_t *);
static	void rijndael128_decrypt(caddr_t, u_int8_t *);
static	void aes_xts_decrypt(caddr_t, u_int8_t *);
static	void cml_decrypt(caddr_t, u_int8_t *);

static	void null_zerokey(u_int8_t **);
static	void des1_zerokey(u_int8_t **);
static	void des3_zerokey(u_int8_t **);
static	void blf_zerokey(u_int8_t **);
static	void cast5_zerokey(u_int8_t **);
static	void skipjack_zerokey(u_int8_t **);
static	void rijndael128_zerokey(u_int8_t **);
static	void aes_xts_zerokey(u_int8_t **);
static	void cml_zerokey(u_int8_t **);

static	void aes_xts_reinit(caddr_t, u_int8_t *);

static	void null_init(void *);
static	int null_update(void *, u_int8_t *, u_int16_t);
static	void null_final(u_int8_t *, void *);
static	int MD5Update_int(void *, u_int8_t *, u_int16_t);
static	void SHA1Init_int(void *);
static	int SHA1Update_int(void *, u_int8_t *, u_int16_t);
static	void SHA1Final_int(u_int8_t *, void *);
static	int RMD160Update_int(void *, u_int8_t *, u_int16_t);
static	int SHA256Update_int(void *, u_int8_t *, u_int16_t);
static	int SHA384Update_int(void *, u_int8_t *, u_int16_t);
static	int SHA512Update_int(void *, u_int8_t *, u_int16_t);

static	u_int32_t deflate_compress(u_int8_t *, u_int32_t, u_int8_t **);
static	u_int32_t deflate_decompress(u_int8_t *, u_int32_t, u_int8_t **);

MALLOC_DEFINE(M_XDATA, "xform", "xform data buffers");

/* Encryption instances */
struct enc_xform enc_xform_null = {
	CRYPTO_NULL_CBC, "NULL",
	/* NB: blocksize of 4 is to generate a properly aligned ESP header */
	NULL_BLOCK_LEN, 0, 256, /* 2048 bits, max key */
	null_encrypt,
	null_decrypt,
	null_setkey,
	null_zerokey,
	NULL
};

struct enc_xform enc_xform_des = {
	CRYPTO_DES_CBC, "DES",
	DES_BLOCK_LEN, 8, 8,
	des1_encrypt,
	des1_decrypt,
	des1_setkey,
	des1_zerokey,
	NULL
};

struct enc_xform enc_xform_3des = {
	CRYPTO_3DES_CBC, "3DES",
	DES3_BLOCK_LEN, 24, 24,
	des3_encrypt,
	des3_decrypt,
	des3_setkey,
	des3_zerokey,
	NULL
};

struct enc_xform enc_xform_blf = {
	CRYPTO_BLF_CBC, "Blowfish",
	BLOWFISH_BLOCK_LEN, 5, 56 /* 448 bits, max key */,
	blf_encrypt,
	blf_decrypt,
	blf_setkey,
	blf_zerokey,
	NULL
};

struct enc_xform enc_xform_cast5 = {
	CRYPTO_CAST_CBC, "CAST-128",
	CAST128_BLOCK_LEN, 5, 16,
	cast5_encrypt,
	cast5_decrypt,
	cast5_setkey,
	cast5_zerokey,
	NULL
};

struct enc_xform enc_xform_skipjack = {
	CRYPTO_SKIPJACK_CBC, "Skipjack",
	SKIPJACK_BLOCK_LEN, 10, 10,
	skipjack_encrypt,
	skipjack_decrypt,
	skipjack_setkey,
	skipjack_zerokey,
	NULL
};

struct enc_xform enc_xform_rijndael128 = {
	CRYPTO_RIJNDAEL128_CBC, "Rijndael-128/AES",
	RIJNDAEL128_BLOCK_LEN, 8, 32,
	rijndael128_encrypt,
	rijndael128_decrypt,
	rijndael128_setkey,
	rijndael128_zerokey,
	NULL
};

struct enc_xform enc_xform_aes_xts = {
	CRYPTO_AES_XTS, "AES-XTS",
	RIJNDAEL128_BLOCK_LEN, 32, 64,
	aes_xts_encrypt,
	aes_xts_decrypt,
	aes_xts_setkey,
	aes_xts_zerokey,
	aes_xts_reinit
};

struct enc_xform enc_xform_arc4 = {
	CRYPTO_ARC4, "ARC4",
	1, 1, 32,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

struct enc_xform enc_xform_camellia = {
	CRYPTO_CAMELLIA_CBC, "Camellia",
	CAMELLIA_BLOCK_LEN, 8, 32,
	cml_encrypt,
	cml_decrypt,
	cml_setkey,
	cml_zerokey,
	NULL
};

/* Authentication instances */
struct auth_hash auth_hash_null = {
	CRYPTO_NULL_HMAC, "NULL-HMAC",
	0, NULL_HASH_LEN, NULL_HMAC_BLOCK_LEN, sizeof(int),	/* NB: context isn't used */
	null_init, null_update, null_final
};

struct auth_hash auth_hash_hmac_md5 = {
	CRYPTO_MD5_HMAC, "HMAC-MD5",
	16, MD5_HASH_LEN, MD5_HMAC_BLOCK_LEN, sizeof(MD5_CTX),
	(void (*) (void *)) MD5Init, MD5Update_int,
	(void (*) (u_int8_t *, void *)) MD5Final
};

struct auth_hash auth_hash_hmac_sha1 = {
	CRYPTO_SHA1_HMAC, "HMAC-SHA1",
	20, SHA1_HASH_LEN, SHA1_HMAC_BLOCK_LEN, sizeof(SHA1_CTX),
	SHA1Init_int, SHA1Update_int, SHA1Final_int
};

struct auth_hash auth_hash_hmac_ripemd_160 = {
	CRYPTO_RIPEMD160_HMAC, "HMAC-RIPEMD-160",
	20, RIPEMD160_HASH_LEN, RIPEMD160_HMAC_BLOCK_LEN, sizeof(RMD160_CTX),
	(void (*)(void *)) RMD160Init, RMD160Update_int,
	(void (*)(u_int8_t *, void *)) RMD160Final
};

struct auth_hash auth_hash_key_md5 = {
	CRYPTO_MD5_KPDK, "Keyed MD5",
	0, MD5_KPDK_HASH_LEN, 0, sizeof(MD5_CTX),
	(void (*)(void *)) MD5Init, MD5Update_int,
	(void (*)(u_int8_t *, void *)) MD5Final
};

struct auth_hash auth_hash_key_sha1 = {
	CRYPTO_SHA1_KPDK, "Keyed SHA1",
	0, SHA1_KPDK_HASH_LEN, 0, sizeof(SHA1_CTX),
	SHA1Init_int, SHA1Update_int, SHA1Final_int
};

struct auth_hash auth_hash_hmac_sha2_256 = {
	CRYPTO_SHA2_256_HMAC, "HMAC-SHA2-256",
	32, SHA2_256_HASH_LEN, SHA2_256_HMAC_BLOCK_LEN, sizeof(SHA256_CTX),
	(void (*)(void *)) SHA256_Init, SHA256Update_int,
	(void (*)(u_int8_t *, void *)) SHA256_Final
};

struct auth_hash auth_hash_hmac_sha2_384 = {
	CRYPTO_SHA2_384_HMAC, "HMAC-SHA2-384",
	48, SHA2_384_HASH_LEN, SHA2_384_HMAC_BLOCK_LEN, sizeof(SHA384_CTX),
	(void (*)(void *)) SHA384_Init, SHA384Update_int,
	(void (*)(u_int8_t *, void *)) SHA384_Final
};

struct auth_hash auth_hash_hmac_sha2_512 = {
	CRYPTO_SHA2_512_HMAC, "HMAC-SHA2-512",
	64, SHA2_512_HASH_LEN, SHA2_512_HMAC_BLOCK_LEN, sizeof(SHA512_CTX),
	(void (*)(void *)) SHA512_Init, SHA512Update_int,
	(void (*)(u_int8_t *, void *)) SHA512_Final
};

/* Compression instance */
struct comp_algo comp_algo_deflate = {
	CRYPTO_DEFLATE_COMP, "Deflate",
	90, deflate_compress,
	deflate_decompress
};

/*
 * Encryption wrapper routines.
 */
static void
null_encrypt(caddr_t key, u_int8_t *blk)
{
}
static void
null_decrypt(caddr_t key, u_int8_t *blk)
{
}
static int
null_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	*sched = NULL;
	return 0;
}
static void
null_zerokey(u_int8_t **sched)
{
	*sched = NULL;
}

static void
des1_encrypt(caddr_t key, u_int8_t *blk)
{
	des_cblock *cb = (des_cblock *) blk;
	des_key_schedule *p = (des_key_schedule *) key;

	des_ecb_encrypt(cb, cb, p[0], DES_ENCRYPT);
}

static void
des1_decrypt(caddr_t key, u_int8_t *blk)
{
	des_cblock *cb = (des_cblock *) blk;
	des_key_schedule *p = (des_key_schedule *) key;

	des_ecb_encrypt(cb, cb, p[0], DES_DECRYPT);
}

static int
des1_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	des_key_schedule *p;
	int err;

	p = malloc(sizeof (des_key_schedule),
		M_CRYPTO_DATA, M_NOWAIT|M_ZERO);
	if (p != NULL) {
		des_set_key((des_cblock *) key, p[0]);
		err = 0;
	} else
		err = ENOMEM;
	*sched = (u_int8_t *) p;
	return err;
}

static void
des1_zerokey(u_int8_t **sched)
{
	bzero(*sched, sizeof (des_key_schedule));
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

static void
des3_encrypt(caddr_t key, u_int8_t *blk)
{
	des_cblock *cb = (des_cblock *) blk;
	des_key_schedule *p = (des_key_schedule *) key;

	des_ecb3_encrypt(cb, cb, p[0], p[1], p[2], DES_ENCRYPT);
}

static void
des3_decrypt(caddr_t key, u_int8_t *blk)
{
	des_cblock *cb = (des_cblock *) blk;
	des_key_schedule *p = (des_key_schedule *) key;

	des_ecb3_encrypt(cb, cb, p[0], p[1], p[2], DES_DECRYPT);
}

static int
des3_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	des_key_schedule *p;
	int err;

	p = malloc(3*sizeof (des_key_schedule),
		M_CRYPTO_DATA, M_NOWAIT|M_ZERO);
	if (p != NULL) {
		des_set_key((des_cblock *)(key +  0), p[0]);
		des_set_key((des_cblock *)(key +  8), p[1]);
		des_set_key((des_cblock *)(key + 16), p[2]);
		err = 0;
	} else
		err = ENOMEM;
	*sched = (u_int8_t *) p;
	return err;
}

static void
des3_zerokey(u_int8_t **sched)
{
	bzero(*sched, 3*sizeof (des_key_schedule));
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

static void
blf_encrypt(caddr_t key, u_int8_t *blk)
{
	BF_LONG t[2];

	memcpy(t, blk, sizeof (t));
	t[0] = ntohl(t[0]);
	t[1] = ntohl(t[1]);
	/* NB: BF_encrypt expects the block in host order! */
	BF_encrypt(t, (BF_KEY *) key);
	t[0] = htonl(t[0]);
	t[1] = htonl(t[1]);
	memcpy(blk, t, sizeof (t));
}

static void
blf_decrypt(caddr_t key, u_int8_t *blk)
{
	BF_LONG t[2];

	memcpy(t, blk, sizeof (t));
	t[0] = ntohl(t[0]);
	t[1] = ntohl(t[1]);
	/* NB: BF_decrypt expects the block in host order! */
	BF_decrypt(t, (BF_KEY *) key);
	t[0] = htonl(t[0]);
	t[1] = htonl(t[1]);
	memcpy(blk, t, sizeof (t));
}

static int
blf_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	int err;

	*sched = malloc(sizeof(BF_KEY),
		M_CRYPTO_DATA, M_NOWAIT|M_ZERO);
	if (*sched != NULL) {
		BF_set_key((BF_KEY *) *sched, len, key);
		err = 0;
	} else
		err = ENOMEM;
	return err;
}

static void
blf_zerokey(u_int8_t **sched)
{
	bzero(*sched, sizeof(BF_KEY));
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

static void
cast5_encrypt(caddr_t key, u_int8_t *blk)
{
	cast_encrypt((cast_key *) key, blk, blk);
}

static void
cast5_decrypt(caddr_t key, u_int8_t *blk)
{
	cast_decrypt((cast_key *) key, blk, blk);
}

static int
cast5_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	int err;

	*sched = malloc(sizeof(cast_key), M_CRYPTO_DATA, M_NOWAIT|M_ZERO);
	if (*sched != NULL) {
		cast_setkey((cast_key *)*sched, key, len);
		err = 0;
	} else
		err = ENOMEM;
	return err;
}

static void
cast5_zerokey(u_int8_t **sched)
{
	bzero(*sched, sizeof(cast_key));
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

static void
skipjack_encrypt(caddr_t key, u_int8_t *blk)
{
	skipjack_forwards(blk, blk, (u_int8_t **) key);
}

static void
skipjack_decrypt(caddr_t key, u_int8_t *blk)
{
	skipjack_backwards(blk, blk, (u_int8_t **) key);
}

static int
skipjack_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	int err;

	/* NB: allocate all the memory that's needed at once */
	*sched = malloc(10 * (sizeof(u_int8_t *) + 0x100),
		M_CRYPTO_DATA, M_NOWAIT|M_ZERO);
	if (*sched != NULL) {
		u_int8_t** key_tables = (u_int8_t**) *sched;
		u_int8_t* table = (u_int8_t*) &key_tables[10];
		int k;

		for (k = 0; k < 10; k++) {
			key_tables[k] = table;
			table += 0x100;
		}
		subkey_table_gen(key, (u_int8_t **) *sched);
		err = 0;
	} else
		err = ENOMEM;
	return err;
}

static void
skipjack_zerokey(u_int8_t **sched)
{
	bzero(*sched, 10 * (sizeof(u_int8_t *) + 0x100));
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

static void
rijndael128_encrypt(caddr_t key, u_int8_t *blk)
{
	rijndael_encrypt((rijndael_ctx *) key, (u_char *) blk, (u_char *) blk);
}

static void
rijndael128_decrypt(caddr_t key, u_int8_t *blk)
{
	rijndael_decrypt(((rijndael_ctx *) key), (u_char *) blk,
	    (u_char *) blk);
}

static int
rijndael128_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	int err;

	if (len != 16 && len != 24 && len != 32)
		return (EINVAL);
	*sched = malloc(sizeof(rijndael_ctx), M_CRYPTO_DATA,
	    M_NOWAIT|M_ZERO);
	if (*sched != NULL) {
		rijndael_set_key((rijndael_ctx *) *sched, (u_char *) key,
		    len * 8);
		err = 0;
	} else
		err = ENOMEM;
	return err;
}

static void
rijndael128_zerokey(u_int8_t **sched)
{
	bzero(*sched, sizeof(rijndael_ctx));
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

#define	AES_XTS_BLOCKSIZE	16
#define	AES_XTS_IVSIZE		8
#define	AES_XTS_ALPHA		0x87	/* GF(2^128) generator polynomial */

struct aes_xts_ctx {
	rijndael_ctx key1;
	rijndael_ctx key2;
	u_int8_t tweak[AES_XTS_BLOCKSIZE];
};

void
aes_xts_reinit(caddr_t key, u_int8_t *iv)
{
	struct aes_xts_ctx *ctx = (struct aes_xts_ctx *)key;
	u_int64_t blocknum;
	u_int i;

	/*
	 * Prepare tweak as E_k2(IV). IV is specified as LE representation
	 * of a 64-bit block number which we allow to be passed in directly.
	 */
	bcopy(iv, &blocknum, AES_XTS_IVSIZE);
	for (i = 0; i < AES_XTS_IVSIZE; i++) {
		ctx->tweak[i] = blocknum & 0xff;
		blocknum >>= 8;
	}
	/* Last 64 bits of IV are always zero */
	bzero(ctx->tweak + AES_XTS_IVSIZE, AES_XTS_IVSIZE);

	rijndael_encrypt(&ctx->key2, ctx->tweak, ctx->tweak);
}

static void
aes_xts_crypt(struct aes_xts_ctx *ctx, u_int8_t *data, u_int do_encrypt)
{
	u_int8_t block[AES_XTS_BLOCKSIZE];
	u_int i, carry_in, carry_out;

	for (i = 0; i < AES_XTS_BLOCKSIZE; i++)
		block[i] = data[i] ^ ctx->tweak[i];

	if (do_encrypt)
		rijndael_encrypt(&ctx->key1, block, data);
	else
		rijndael_decrypt(&ctx->key1, block, data);

	for (i = 0; i < AES_XTS_BLOCKSIZE; i++)
		data[i] ^= ctx->tweak[i];

	/* Exponentiate tweak */
	carry_in = 0;
	for (i = 0; i < AES_XTS_BLOCKSIZE; i++) {
		carry_out = ctx->tweak[i] & 0x80;
		ctx->tweak[i] = (ctx->tweak[i] << 1) | (carry_in ? 1 : 0);
		carry_in = carry_out;
	}
	if (carry_in)
		ctx->tweak[0] ^= AES_XTS_ALPHA;
	bzero(block, sizeof(block));
}

void
aes_xts_encrypt(caddr_t key, u_int8_t *data)
{
	aes_xts_crypt((struct aes_xts_ctx *)key, data, 1);
}

void
aes_xts_decrypt(caddr_t key, u_int8_t *data)
{
	aes_xts_crypt((struct aes_xts_ctx *)key, data, 0);
}

int
aes_xts_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	struct aes_xts_ctx *ctx;

	if (len != 32 && len != 64)
		return EINVAL;

	*sched = malloc(sizeof(struct aes_xts_ctx), M_CRYPTO_DATA,
	    M_NOWAIT | M_ZERO);
	if (*sched == NULL)
		return ENOMEM;
	ctx = (struct aes_xts_ctx *)*sched;

	rijndael_set_key(&ctx->key1, key, len * 4);
	rijndael_set_key(&ctx->key2, key + (len / 2), len * 4);

	return 0;
}

void
aes_xts_zerokey(u_int8_t **sched)
{
	bzero(*sched, sizeof(struct aes_xts_ctx));
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

static void
cml_encrypt(caddr_t key, u_int8_t *blk)
{
	camellia_encrypt((camellia_ctx *) key, (u_char *) blk, (u_char *) blk);
}

static void
cml_decrypt(caddr_t key, u_int8_t *blk)
{
	camellia_decrypt(((camellia_ctx *) key), (u_char *) blk,
	    (u_char *) blk);
}

static int
cml_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	int err;

	if (len != 16 && len != 24 && len != 32)
		return (EINVAL);
	*sched = malloc(sizeof(camellia_ctx), M_CRYPTO_DATA,
	    M_NOWAIT|M_ZERO);
	if (*sched != NULL) {
		camellia_set_key((camellia_ctx *) *sched, (u_char *) key,
		    len * 8);
		err = 0;
	} else
		err = ENOMEM;
	return err;
}

static void
cml_zerokey(u_int8_t **sched)
{
	bzero(*sched, sizeof(camellia_ctx));
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

/*
 * And now for auth.
 */

static void
null_init(void *ctx)
{
}

static int
null_update(void *ctx, u_int8_t *buf, u_int16_t len)
{
	return 0;
}

static void
null_final(u_int8_t *buf, void *ctx)
{
	if (buf != (u_int8_t *) 0)
		bzero(buf, 12);
}

static int
RMD160Update_int(void *ctx, u_int8_t *buf, u_int16_t len)
{
	RMD160Update(ctx, buf, len);
	return 0;
}

static int
MD5Update_int(void *ctx, u_int8_t *buf, u_int16_t len)
{
	MD5Update(ctx, buf, len);
	return 0;
}

static void
SHA1Init_int(void *ctx)
{
	SHA1Init(ctx);
}

static int
SHA1Update_int(void *ctx, u_int8_t *buf, u_int16_t len)
{
	SHA1Update(ctx, buf, len);
	return 0;
}

static void
SHA1Final_int(u_int8_t *blk, void *ctx)
{
	SHA1Final(blk, ctx);
}

static int
SHA256Update_int(void *ctx, u_int8_t *buf, u_int16_t len)
{
	SHA256_Update(ctx, buf, len);
	return 0;
}

static int
SHA384Update_int(void *ctx, u_int8_t *buf, u_int16_t len)
{
	SHA384_Update(ctx, buf, len);
	return 0;
}

static int
SHA512Update_int(void *ctx, u_int8_t *buf, u_int16_t len)
{
	SHA512_Update(ctx, buf, len);
	return 0;
}

/*
 * And compression
 */

static u_int32_t
deflate_compress(data, size, out)
	u_int8_t *data;
	u_int32_t size;
	u_int8_t **out;
{
	return deflate_global(data, size, 0, out);
}

static u_int32_t
deflate_decompress(data, size, out)
	u_int8_t *data;
	u_int32_t size;
	u_int8_t **out;
{
	return deflate_global(data, size, 1, out);
}
