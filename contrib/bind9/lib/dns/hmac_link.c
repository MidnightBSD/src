/*
 * Portions Copyright (C) 2004-2006  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 1999-2002  Internet Software Consortium.
 * Portions Copyright (C) 1995-2000 by Network Associates, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Principal Author: Brian Wellington
 * $Id: hmac_link.c,v 1.1.6.5 2006/01/27 23:57:44 marka Exp $
 */

#include <config.h>

#include <isc/buffer.h>
#include <isc/hmacmd5.h>
#include <isc/hmacsha.h>
#include <isc/md5.h>
#include <isc/sha1.h>
#include <isc/mem.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dst/result.h>

#include "dst_internal.h"
#include "dst_parse.h"

#define HMAC_LEN	64
#define HMAC_IPAD	0x36
#define HMAC_OPAD	0x5c

static isc_result_t hmacmd5_fromdns(dst_key_t *key, isc_buffer_t *data);

typedef struct hmackey {
	unsigned char key[HMAC_LEN];
} HMAC_Key;

static isc_result_t
getkeybits(dst_key_t *key, struct dst_private_element *element) {

	if (element->length != 2)
		return (DST_R_INVALIDPRIVATEKEY);

	key->key_bits =	(element->data[0] << 8) + element->data[1];

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacmd5_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_hmacmd5_t *hmacmd5ctx;
	HMAC_Key *hkey = key->opaque;

	hmacmd5ctx = isc_mem_get(dctx->mctx, sizeof(isc_hmacmd5_t));
	if (hmacmd5ctx == NULL)
		return (ISC_R_NOMEMORY);
	isc_hmacmd5_init(hmacmd5ctx, hkey->key, HMAC_LEN);
	dctx->opaque = hmacmd5ctx;
	return (ISC_R_SUCCESS);
}

static void
hmacmd5_destroyctx(dst_context_t *dctx) {
	isc_hmacmd5_t *hmacmd5ctx = dctx->opaque;

	if (hmacmd5ctx != NULL) {
		isc_hmacmd5_invalidate(hmacmd5ctx);
		isc_mem_put(dctx->mctx, hmacmd5ctx, sizeof(isc_hmacmd5_t));
		dctx->opaque = NULL;
	}
}

static isc_result_t
hmacmd5_adddata(dst_context_t *dctx, const isc_region_t *data) {
	isc_hmacmd5_t *hmacmd5ctx = dctx->opaque;

	isc_hmacmd5_update(hmacmd5ctx, data->base, data->length);
	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacmd5_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_hmacmd5_t *hmacmd5ctx = dctx->opaque;
	unsigned char *digest;

	if (isc_buffer_availablelength(sig) < ISC_MD5_DIGESTLENGTH)
		return (ISC_R_NOSPACE);
	digest = isc_buffer_used(sig);
	isc_hmacmd5_sign(hmacmd5ctx, digest);
	isc_buffer_add(sig, ISC_MD5_DIGESTLENGTH);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacmd5_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_hmacmd5_t *hmacmd5ctx = dctx->opaque;

	if (sig->length > ISC_MD5_DIGESTLENGTH)
		return (DST_R_VERIFYFAILURE);

	if (isc_hmacmd5_verify2(hmacmd5ctx, sig->base, sig->length))
		return (ISC_R_SUCCESS);
	else
		return (DST_R_VERIFYFAILURE);
}

static isc_boolean_t
hmacmd5_compare(const dst_key_t *key1, const dst_key_t *key2) {
	HMAC_Key *hkey1, *hkey2;

	hkey1 = (HMAC_Key *)key1->opaque;
	hkey2 = (HMAC_Key *)key2->opaque;

	if (hkey1 == NULL && hkey2 == NULL)
		return (ISC_TRUE);
	else if (hkey1 == NULL || hkey2 == NULL)
		return (ISC_FALSE);

	if (memcmp(hkey1->key, hkey2->key, HMAC_LEN) == 0)
		return (ISC_TRUE);
	else
		return (ISC_FALSE);
}

static isc_result_t
hmacmd5_generate(dst_key_t *key, int pseudorandom_ok) {
	isc_buffer_t b;
	isc_result_t ret;
	int bytes;
	unsigned char data[HMAC_LEN];

	bytes = (key->key_size + 7) / 8;
	if (bytes > HMAC_LEN) {
		bytes = HMAC_LEN;
		key->key_size = HMAC_LEN * 8;
	}

	memset(data, 0, HMAC_LEN);
	ret = dst__entropy_getdata(data, bytes, ISC_TF(pseudorandom_ok != 0));

	if (ret != ISC_R_SUCCESS)
		return (ret);

	isc_buffer_init(&b, data, bytes);
	isc_buffer_add(&b, bytes);
	ret = hmacmd5_fromdns(key, &b);
	memset(data, 0, HMAC_LEN);

	return (ret);
}

static isc_boolean_t
hmacmd5_isprivate(const dst_key_t *key) {
	UNUSED(key);
	return (ISC_TRUE);
}

static void
hmacmd5_destroy(dst_key_t *key) {
	HMAC_Key *hkey = key->opaque;
	memset(hkey, 0, sizeof(HMAC_Key));
	isc_mem_put(key->mctx, hkey, sizeof(HMAC_Key));
	key->opaque = NULL;
}

static isc_result_t
hmacmd5_todns(const dst_key_t *key, isc_buffer_t *data) {
	HMAC_Key *hkey;
	unsigned int bytes;

	REQUIRE(key->opaque != NULL);

	hkey = (HMAC_Key *) key->opaque;

	bytes = (key->key_size + 7) / 8;
	if (isc_buffer_availablelength(data) < bytes)
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(data, hkey->key, bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacmd5_fromdns(dst_key_t *key, isc_buffer_t *data) {
	HMAC_Key *hkey;
	int keylen;
	isc_region_t r;
	isc_md5_t md5ctx;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	hkey = (HMAC_Key *) isc_mem_get(key->mctx, sizeof(HMAC_Key));
	if (hkey == NULL)
		return (ISC_R_NOMEMORY);

	memset(hkey->key, 0, sizeof(hkey->key));

	if (r.length > HMAC_LEN) {
		isc_md5_init(&md5ctx);
		isc_md5_update(&md5ctx, r.base, r.length);
		isc_md5_final(&md5ctx, hkey->key);
		keylen = ISC_MD5_DIGESTLENGTH;
	}
	else {
		memcpy(hkey->key, r.base, r.length);
		keylen = r.length;
	}

	key->key_size = keylen * 8;
	key->opaque = hkey;

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacmd5_tofile(const dst_key_t *key, const char *directory) {
	int cnt = 0;
	HMAC_Key *hkey;
	dst_private_t priv;
	int bytes = (key->key_size + 7) / 8;
	unsigned char buf[2];

	if (key->opaque == NULL)
		return (DST_R_NULLKEY);

	hkey = (HMAC_Key *) key->opaque;

	priv.elements[cnt].tag = TAG_HMACMD5_KEY;
	priv.elements[cnt].length = bytes;
	priv.elements[cnt++].data = hkey->key;

	buf[0] = (key->key_bits >> 8) & 0xffU;
	buf[1] = key->key_bits & 0xffU;
	priv.elements[cnt].tag = TAG_HMACMD5_BITS;
	priv.elements[cnt].data = buf;
	priv.elements[cnt++].length = 2;

	priv.nelements = cnt;
	return (dst__privstruct_writefile(key, &priv, directory));
}

static isc_result_t
hmacmd5_parse(dst_key_t *key, isc_lex_t *lexer) {
	dst_private_t priv;
	isc_result_t result, tresult;
	isc_buffer_t b;
	isc_mem_t *mctx = key->mctx;
	unsigned int i;

	/* read private key file */
	result = dst__privstruct_parse(key, DST_ALG_HMACMD5, lexer, mctx, &priv);
	if (result != ISC_R_SUCCESS)
		return (result);

	key->key_bits = 0;
	for (i = 0; i < priv.nelements && result == ISC_R_SUCCESS; i++) {
		switch (priv.elements[i].tag) {
		case TAG_HMACMD5_KEY:
			isc_buffer_init(&b, priv.elements[i].data,
				        priv.elements[i].length);
			isc_buffer_add(&b, priv.elements[i].length);
			tresult = hmacmd5_fromdns(key, &b);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		case TAG_HMACMD5_BITS:
			tresult = getkeybits(key, &priv.elements[i]);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		default:
			result = DST_R_INVALIDPRIVATEKEY;
			break;
		}
	}
	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));
	return (result);
}

static dst_func_t hmacmd5_functions = {
	hmacmd5_createctx,
	hmacmd5_destroyctx,
	hmacmd5_adddata,
	hmacmd5_sign,
	hmacmd5_verify,
	NULL, /*%< computesecret */
	hmacmd5_compare,
	NULL, /*%< paramcompare */
	hmacmd5_generate,
	hmacmd5_isprivate,
	hmacmd5_destroy,
	hmacmd5_todns,
	hmacmd5_fromdns,
	hmacmd5_tofile,
	hmacmd5_parse,
	NULL, /*%< cleanup */
};

isc_result_t
dst__hmacmd5_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &hmacmd5_functions;
	return (ISC_R_SUCCESS);
}

static isc_result_t hmacsha1_fromdns(dst_key_t *key, isc_buffer_t *data);

typedef struct {
	unsigned char key[ISC_SHA1_DIGESTLENGTH];
} HMACSHA1_Key;

static isc_result_t
hmacsha1_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_hmacsha1_t *hmacsha1ctx;
	HMACSHA1_Key *hkey = key->opaque;

	hmacsha1ctx = isc_mem_get(dctx->mctx, sizeof(isc_hmacsha1_t));
	if (hmacsha1ctx == NULL)
		return (ISC_R_NOMEMORY);
	isc_hmacsha1_init(hmacsha1ctx, hkey->key, ISC_SHA1_DIGESTLENGTH);
	dctx->opaque = hmacsha1ctx;
	return (ISC_R_SUCCESS);
}

static void
hmacsha1_destroyctx(dst_context_t *dctx) {
	isc_hmacsha1_t *hmacsha1ctx = dctx->opaque;

	if (hmacsha1ctx != NULL) {
		isc_hmacsha1_invalidate(hmacsha1ctx);
		isc_mem_put(dctx->mctx, hmacsha1ctx, sizeof(isc_hmacsha1_t));
		dctx->opaque = NULL;
	}
}

static isc_result_t
hmacsha1_adddata(dst_context_t *dctx, const isc_region_t *data) {
	isc_hmacsha1_t *hmacsha1ctx = dctx->opaque;

	isc_hmacsha1_update(hmacsha1ctx, data->base, data->length);
	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha1_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_hmacsha1_t *hmacsha1ctx = dctx->opaque;
	unsigned char *digest;

	if (isc_buffer_availablelength(sig) < ISC_SHA1_DIGESTLENGTH)
		return (ISC_R_NOSPACE);
	digest = isc_buffer_used(sig);
	isc_hmacsha1_sign(hmacsha1ctx, digest, ISC_SHA1_DIGESTLENGTH);
	isc_buffer_add(sig, ISC_SHA1_DIGESTLENGTH);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha1_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_hmacsha1_t *hmacsha1ctx = dctx->opaque;

	if (sig->length > ISC_SHA1_DIGESTLENGTH || sig->length == 0)
		return (DST_R_VERIFYFAILURE);

	if (isc_hmacsha1_verify(hmacsha1ctx, sig->base, sig->length))
		return (ISC_R_SUCCESS);
	else
		return (DST_R_VERIFYFAILURE);
}

static isc_boolean_t
hmacsha1_compare(const dst_key_t *key1, const dst_key_t *key2) {
	HMACSHA1_Key *hkey1, *hkey2;

	hkey1 = (HMACSHA1_Key *)key1->opaque;
	hkey2 = (HMACSHA1_Key *)key2->opaque;

	if (hkey1 == NULL && hkey2 == NULL)
		return (ISC_TRUE);
	else if (hkey1 == NULL || hkey2 == NULL)
		return (ISC_FALSE);

	if (memcmp(hkey1->key, hkey2->key, ISC_SHA1_DIGESTLENGTH) == 0)
		return (ISC_TRUE);
	else
		return (ISC_FALSE);
}

static isc_result_t
hmacsha1_generate(dst_key_t *key, int pseudorandom_ok) {
	isc_buffer_t b;
	isc_result_t ret;
	int bytes;
	unsigned char data[HMAC_LEN];

	bytes = (key->key_size + 7) / 8;
	if (bytes > HMAC_LEN) {
		bytes = HMAC_LEN;
		key->key_size = HMAC_LEN * 8;
	}

	memset(data, 0, HMAC_LEN);
	ret = dst__entropy_getdata(data, bytes, ISC_TF(pseudorandom_ok != 0));

	if (ret != ISC_R_SUCCESS)
		return (ret);

	isc_buffer_init(&b, data, bytes);
	isc_buffer_add(&b, bytes);
	ret = hmacsha1_fromdns(key, &b);
	memset(data, 0, ISC_SHA1_DIGESTLENGTH);

	return (ret);
}

static isc_boolean_t
hmacsha1_isprivate(const dst_key_t *key) {
	UNUSED(key);
	return (ISC_TRUE);
}

static void
hmacsha1_destroy(dst_key_t *key) {
	HMACSHA1_Key *hkey = key->opaque;
	memset(hkey, 0, sizeof(HMACSHA1_Key));
	isc_mem_put(key->mctx, hkey, sizeof(HMACSHA1_Key));
	key->opaque = NULL;
}

static isc_result_t
hmacsha1_todns(const dst_key_t *key, isc_buffer_t *data) {
	HMACSHA1_Key *hkey;
	unsigned int bytes;

	REQUIRE(key->opaque != NULL);

	hkey = (HMACSHA1_Key *) key->opaque;

	bytes = (key->key_size + 7) / 8;
	if (isc_buffer_availablelength(data) < bytes)
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(data, hkey->key, bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha1_fromdns(dst_key_t *key, isc_buffer_t *data) {
	HMACSHA1_Key *hkey;
	int keylen;
	isc_region_t r;
	isc_sha1_t sha1ctx;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	hkey = (HMACSHA1_Key *) isc_mem_get(key->mctx, sizeof(HMACSHA1_Key));
	if (hkey == NULL)
		return (ISC_R_NOMEMORY);

	memset(hkey->key, 0, sizeof(hkey->key));

	if (r.length > ISC_SHA1_DIGESTLENGTH) {
		isc_sha1_init(&sha1ctx);
		isc_sha1_update(&sha1ctx, r.base, r.length);
		isc_sha1_final(&sha1ctx, hkey->key);
		keylen = ISC_SHA1_DIGESTLENGTH;
	}
	else {
		memcpy(hkey->key, r.base, r.length);
		keylen = r.length;
	}

	key->key_size = keylen * 8;
	key->opaque = hkey;

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha1_tofile(const dst_key_t *key, const char *directory) {
	int cnt = 0;
	HMACSHA1_Key *hkey;
	dst_private_t priv;
	int bytes = (key->key_size + 7) / 8;
	unsigned char buf[2];

	if (key->opaque == NULL)
		return (DST_R_NULLKEY);

	hkey = (HMACSHA1_Key *) key->opaque;

	priv.elements[cnt].tag = TAG_HMACSHA1_KEY;
	priv.elements[cnt].length = bytes;
	priv.elements[cnt++].data = hkey->key;

	buf[0] = (key->key_bits >> 8) & 0xffU;
	buf[1] = key->key_bits & 0xffU;
	priv.elements[cnt].tag = TAG_HMACSHA1_BITS;
	priv.elements[cnt].data = buf;
	priv.elements[cnt++].length = 2;

	priv.nelements = cnt;
	return (dst__privstruct_writefile(key, &priv, directory));
}

static isc_result_t
hmacsha1_parse(dst_key_t *key, isc_lex_t *lexer) {
	dst_private_t priv;
	isc_result_t result, tresult;
	isc_buffer_t b;
	isc_mem_t *mctx = key->mctx;
	unsigned int i;

	/* read private key file */
	result = dst__privstruct_parse(key, DST_ALG_HMACSHA1, lexer, mctx,
				       &priv);
	if (result != ISC_R_SUCCESS)
		return (result);

	key->key_bits = 0;
	for (i = 0; i < priv.nelements; i++) {
		switch (priv.elements[i].tag) {
		case TAG_HMACSHA1_KEY:
			isc_buffer_init(&b, priv.elements[i].data,
				        priv.elements[i].length);
			isc_buffer_add(&b, priv.elements[i].length);
			tresult = hmacsha1_fromdns(key, &b);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		case TAG_HMACSHA1_BITS:
			tresult = getkeybits(key, &priv.elements[i]);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		default:
			result = DST_R_INVALIDPRIVATEKEY;
			break;
		}
	}
	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));
	return (result);
}

static dst_func_t hmacsha1_functions = {
	hmacsha1_createctx,
	hmacsha1_destroyctx,
	hmacsha1_adddata,
	hmacsha1_sign,
	hmacsha1_verify,
	NULL, /* computesecret */
	hmacsha1_compare,
	NULL, /* paramcompare */
	hmacsha1_generate,
	hmacsha1_isprivate,
	hmacsha1_destroy,
	hmacsha1_todns,
	hmacsha1_fromdns,
	hmacsha1_tofile,
	hmacsha1_parse,
	NULL, /* cleanup */
};

isc_result_t
dst__hmacsha1_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &hmacsha1_functions;
	return (ISC_R_SUCCESS);
}

static isc_result_t hmacsha224_fromdns(dst_key_t *key, isc_buffer_t *data);

typedef struct {
	unsigned char key[ISC_SHA224_DIGESTLENGTH];
} HMACSHA224_Key;

static isc_result_t
hmacsha224_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_hmacsha224_t *hmacsha224ctx;
	HMACSHA224_Key *hkey = key->opaque;

	hmacsha224ctx = isc_mem_get(dctx->mctx, sizeof(isc_hmacsha224_t));
	if (hmacsha224ctx == NULL)
		return (ISC_R_NOMEMORY);
	isc_hmacsha224_init(hmacsha224ctx, hkey->key, ISC_SHA224_DIGESTLENGTH);
	dctx->opaque = hmacsha224ctx;
	return (ISC_R_SUCCESS);
}

static void
hmacsha224_destroyctx(dst_context_t *dctx) {
	isc_hmacsha224_t *hmacsha224ctx = dctx->opaque;

	if (hmacsha224ctx != NULL) {
		isc_hmacsha224_invalidate(hmacsha224ctx);
		isc_mem_put(dctx->mctx, hmacsha224ctx, sizeof(isc_hmacsha224_t));
		dctx->opaque = NULL;
	}
}

static isc_result_t
hmacsha224_adddata(dst_context_t *dctx, const isc_region_t *data) {
	isc_hmacsha224_t *hmacsha224ctx = dctx->opaque;

	isc_hmacsha224_update(hmacsha224ctx, data->base, data->length);
	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha224_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_hmacsha224_t *hmacsha224ctx = dctx->opaque;
	unsigned char *digest;

	if (isc_buffer_availablelength(sig) < ISC_SHA224_DIGESTLENGTH)
		return (ISC_R_NOSPACE);
	digest = isc_buffer_used(sig);
	isc_hmacsha224_sign(hmacsha224ctx, digest, ISC_SHA224_DIGESTLENGTH);
	isc_buffer_add(sig, ISC_SHA224_DIGESTLENGTH);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha224_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_hmacsha224_t *hmacsha224ctx = dctx->opaque;

	if (sig->length > ISC_SHA224_DIGESTLENGTH || sig->length == 0)
		return (DST_R_VERIFYFAILURE);

	if (isc_hmacsha224_verify(hmacsha224ctx, sig->base, sig->length))
		return (ISC_R_SUCCESS);
	else
		return (DST_R_VERIFYFAILURE);
}

static isc_boolean_t
hmacsha224_compare(const dst_key_t *key1, const dst_key_t *key2) {
	HMACSHA224_Key *hkey1, *hkey2;

	hkey1 = (HMACSHA224_Key *)key1->opaque;
	hkey2 = (HMACSHA224_Key *)key2->opaque;

	if (hkey1 == NULL && hkey2 == NULL)
		return (ISC_TRUE);
	else if (hkey1 == NULL || hkey2 == NULL)
		return (ISC_FALSE);

	if (memcmp(hkey1->key, hkey2->key, ISC_SHA224_DIGESTLENGTH) == 0)
		return (ISC_TRUE);
	else
		return (ISC_FALSE);
}

static isc_result_t
hmacsha224_generate(dst_key_t *key, int pseudorandom_ok) {
	isc_buffer_t b;
	isc_result_t ret;
	int bytes;
	unsigned char data[HMAC_LEN];

	bytes = (key->key_size + 7) / 8;
	if (bytes > HMAC_LEN) {
		bytes = HMAC_LEN;
		key->key_size = HMAC_LEN * 8;
	}

	memset(data, 0, HMAC_LEN);
	ret = dst__entropy_getdata(data, bytes, ISC_TF(pseudorandom_ok != 0));

	if (ret != ISC_R_SUCCESS)
		return (ret);

	isc_buffer_init(&b, data, bytes);
	isc_buffer_add(&b, bytes);
	ret = hmacsha224_fromdns(key, &b);
	memset(data, 0, ISC_SHA224_DIGESTLENGTH);

	return (ret);
}

static isc_boolean_t
hmacsha224_isprivate(const dst_key_t *key) {
	UNUSED(key);
	return (ISC_TRUE);
}

static void
hmacsha224_destroy(dst_key_t *key) {
	HMACSHA224_Key *hkey = key->opaque;
	memset(hkey, 0, sizeof(HMACSHA224_Key));
	isc_mem_put(key->mctx, hkey, sizeof(HMACSHA224_Key));
	key->opaque = NULL;
}

static isc_result_t
hmacsha224_todns(const dst_key_t *key, isc_buffer_t *data) {
	HMACSHA224_Key *hkey;
	unsigned int bytes;

	REQUIRE(key->opaque != NULL);

	hkey = (HMACSHA224_Key *) key->opaque;

	bytes = (key->key_size + 7) / 8;
	if (isc_buffer_availablelength(data) < bytes)
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(data, hkey->key, bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha224_fromdns(dst_key_t *key, isc_buffer_t *data) {
	HMACSHA224_Key *hkey;
	int keylen;
	isc_region_t r;
	isc_sha224_t sha224ctx;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	hkey = (HMACSHA224_Key *) isc_mem_get(key->mctx, sizeof(HMACSHA224_Key));
	if (hkey == NULL)
		return (ISC_R_NOMEMORY);

	memset(hkey->key, 0, sizeof(hkey->key));

	if (r.length > ISC_SHA224_DIGESTLENGTH) {
		isc_sha224_init(&sha224ctx);
		isc_sha224_update(&sha224ctx, r.base, r.length);
		isc_sha224_final(hkey->key, &sha224ctx);
		keylen = ISC_SHA224_DIGESTLENGTH;
	}
	else {
		memcpy(hkey->key, r.base, r.length);
		keylen = r.length;
	}

	key->key_size = keylen * 8;
	key->opaque = hkey;

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha224_tofile(const dst_key_t *key, const char *directory) {
	int cnt = 0;
	HMACSHA224_Key *hkey;
	dst_private_t priv;
	int bytes = (key->key_size + 7) / 8;
	unsigned char buf[2];

	if (key->opaque == NULL)
		return (DST_R_NULLKEY);

	hkey = (HMACSHA224_Key *) key->opaque;

	priv.elements[cnt].tag = TAG_HMACSHA224_KEY;
	priv.elements[cnt].length = bytes;
	priv.elements[cnt++].data = hkey->key;

	buf[0] = (key->key_bits >> 8) & 0xffU;
	buf[1] = key->key_bits & 0xffU;
	priv.elements[cnt].tag = TAG_HMACSHA224_BITS;
	priv.elements[cnt].data = buf;
	priv.elements[cnt++].length = 2;

	priv.nelements = cnt;
	return (dst__privstruct_writefile(key, &priv, directory));
}

static isc_result_t
hmacsha224_parse(dst_key_t *key, isc_lex_t *lexer) {
	dst_private_t priv;
	isc_result_t result, tresult;
	isc_buffer_t b;
	isc_mem_t *mctx = key->mctx;
	unsigned int i;

	/* read private key file */
	result = dst__privstruct_parse(key, DST_ALG_HMACSHA224, lexer, mctx,
				       &priv);
	if (result != ISC_R_SUCCESS)
		return (result);

	key->key_bits = 0;
	for (i = 0; i < priv.nelements; i++) {
		switch (priv.elements[i].tag) {
		case TAG_HMACSHA224_KEY:
			isc_buffer_init(&b, priv.elements[i].data,
				        priv.elements[i].length);
			isc_buffer_add(&b, priv.elements[i].length);
			tresult = hmacsha224_fromdns(key, &b);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		case TAG_HMACSHA224_BITS:
			tresult = getkeybits(key, &priv.elements[i]);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		default:
			result = DST_R_INVALIDPRIVATEKEY;
			break;
		}
	}
	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));
	return (result);
}

static dst_func_t hmacsha224_functions = {
	hmacsha224_createctx,
	hmacsha224_destroyctx,
	hmacsha224_adddata,
	hmacsha224_sign,
	hmacsha224_verify,
	NULL, /* computesecret */
	hmacsha224_compare,
	NULL, /* paramcompare */
	hmacsha224_generate,
	hmacsha224_isprivate,
	hmacsha224_destroy,
	hmacsha224_todns,
	hmacsha224_fromdns,
	hmacsha224_tofile,
	hmacsha224_parse,
	NULL, /* cleanup */
};

isc_result_t
dst__hmacsha224_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &hmacsha224_functions;
	return (ISC_R_SUCCESS);
}

static isc_result_t hmacsha256_fromdns(dst_key_t *key, isc_buffer_t *data);

typedef struct {
	unsigned char key[ISC_SHA256_DIGESTLENGTH];
} HMACSHA256_Key;

static isc_result_t
hmacsha256_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_hmacsha256_t *hmacsha256ctx;
	HMACSHA256_Key *hkey = key->opaque;

	hmacsha256ctx = isc_mem_get(dctx->mctx, sizeof(isc_hmacsha256_t));
	if (hmacsha256ctx == NULL)
		return (ISC_R_NOMEMORY);
	isc_hmacsha256_init(hmacsha256ctx, hkey->key, ISC_SHA256_DIGESTLENGTH);
	dctx->opaque = hmacsha256ctx;
	return (ISC_R_SUCCESS);
}

static void
hmacsha256_destroyctx(dst_context_t *dctx) {
	isc_hmacsha256_t *hmacsha256ctx = dctx->opaque;

	if (hmacsha256ctx != NULL) {
		isc_hmacsha256_invalidate(hmacsha256ctx);
		isc_mem_put(dctx->mctx, hmacsha256ctx, sizeof(isc_hmacsha256_t));
		dctx->opaque = NULL;
	}
}

static isc_result_t
hmacsha256_adddata(dst_context_t *dctx, const isc_region_t *data) {
	isc_hmacsha256_t *hmacsha256ctx = dctx->opaque;

	isc_hmacsha256_update(hmacsha256ctx, data->base, data->length);
	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha256_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_hmacsha256_t *hmacsha256ctx = dctx->opaque;
	unsigned char *digest;

	if (isc_buffer_availablelength(sig) < ISC_SHA256_DIGESTLENGTH)
		return (ISC_R_NOSPACE);
	digest = isc_buffer_used(sig);
	isc_hmacsha256_sign(hmacsha256ctx, digest, ISC_SHA256_DIGESTLENGTH);
	isc_buffer_add(sig, ISC_SHA256_DIGESTLENGTH);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha256_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_hmacsha256_t *hmacsha256ctx = dctx->opaque;

	if (sig->length > ISC_SHA256_DIGESTLENGTH || sig->length == 0)
		return (DST_R_VERIFYFAILURE);

	if (isc_hmacsha256_verify(hmacsha256ctx, sig->base, sig->length))
		return (ISC_R_SUCCESS);
	else
		return (DST_R_VERIFYFAILURE);
}

static isc_boolean_t
hmacsha256_compare(const dst_key_t *key1, const dst_key_t *key2) {
	HMACSHA256_Key *hkey1, *hkey2;

	hkey1 = (HMACSHA256_Key *)key1->opaque;
	hkey2 = (HMACSHA256_Key *)key2->opaque;

	if (hkey1 == NULL && hkey2 == NULL)
		return (ISC_TRUE);
	else if (hkey1 == NULL || hkey2 == NULL)
		return (ISC_FALSE);

	if (memcmp(hkey1->key, hkey2->key, ISC_SHA256_DIGESTLENGTH) == 0)
		return (ISC_TRUE);
	else
		return (ISC_FALSE);
}

static isc_result_t
hmacsha256_generate(dst_key_t *key, int pseudorandom_ok) {
	isc_buffer_t b;
	isc_result_t ret;
	int bytes;
	unsigned char data[HMAC_LEN];

	bytes = (key->key_size + 7) / 8;
	if (bytes > HMAC_LEN) {
		bytes = HMAC_LEN;
		key->key_size = HMAC_LEN * 8;
	}

	memset(data, 0, HMAC_LEN);
	ret = dst__entropy_getdata(data, bytes, ISC_TF(pseudorandom_ok != 0));

	if (ret != ISC_R_SUCCESS)
		return (ret);

	isc_buffer_init(&b, data, bytes);
	isc_buffer_add(&b, bytes);
	ret = hmacsha256_fromdns(key, &b);
	memset(data, 0, ISC_SHA256_DIGESTLENGTH);

	return (ret);
}

static isc_boolean_t
hmacsha256_isprivate(const dst_key_t *key) {
	UNUSED(key);
	return (ISC_TRUE);
}

static void
hmacsha256_destroy(dst_key_t *key) {
	HMACSHA256_Key *hkey = key->opaque;
	memset(hkey, 0, sizeof(HMACSHA256_Key));
	isc_mem_put(key->mctx, hkey, sizeof(HMACSHA256_Key));
	key->opaque = NULL;
}

static isc_result_t
hmacsha256_todns(const dst_key_t *key, isc_buffer_t *data) {
	HMACSHA256_Key *hkey;
	unsigned int bytes;

	REQUIRE(key->opaque != NULL);

	hkey = (HMACSHA256_Key *) key->opaque;

	bytes = (key->key_size + 7) / 8;
	if (isc_buffer_availablelength(data) < bytes)
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(data, hkey->key, bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha256_fromdns(dst_key_t *key, isc_buffer_t *data) {
	HMACSHA256_Key *hkey;
	int keylen;
	isc_region_t r;
	isc_sha256_t sha256ctx;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	hkey = (HMACSHA256_Key *) isc_mem_get(key->mctx, sizeof(HMACSHA256_Key));
	if (hkey == NULL)
		return (ISC_R_NOMEMORY);

	memset(hkey->key, 0, sizeof(hkey->key));

	if (r.length > ISC_SHA256_DIGESTLENGTH) {
		isc_sha256_init(&sha256ctx);
		isc_sha256_update(&sha256ctx, r.base, r.length);
		isc_sha256_final(hkey->key, &sha256ctx);
		keylen = ISC_SHA256_DIGESTLENGTH;
	}
	else {
		memcpy(hkey->key, r.base, r.length);
		keylen = r.length;
	}

	key->key_size = keylen * 8;
	key->opaque = hkey;

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha256_tofile(const dst_key_t *key, const char *directory) {
	int cnt = 0;
	HMACSHA256_Key *hkey;
	dst_private_t priv;
	int bytes = (key->key_size + 7) / 8;
	unsigned char buf[2];

	if (key->opaque == NULL)
		return (DST_R_NULLKEY);

	hkey = (HMACSHA256_Key *) key->opaque;

	priv.elements[cnt].tag = TAG_HMACSHA256_KEY;
	priv.elements[cnt].length = bytes;
	priv.elements[cnt++].data = hkey->key;

	buf[0] = (key->key_bits >> 8) & 0xffU;
	buf[1] = key->key_bits & 0xffU;
	priv.elements[cnt].tag = TAG_HMACSHA256_BITS;
	priv.elements[cnt].data = buf;
	priv.elements[cnt++].length = 2;

	priv.nelements = cnt;
	return (dst__privstruct_writefile(key, &priv, directory));
}

static isc_result_t
hmacsha256_parse(dst_key_t *key, isc_lex_t *lexer) {
	dst_private_t priv;
	isc_result_t result, tresult;
	isc_buffer_t b;
	isc_mem_t *mctx = key->mctx;
	unsigned int i;

	/* read private key file */
	result = dst__privstruct_parse(key, DST_ALG_HMACSHA256, lexer, mctx,
				       &priv);
	if (result != ISC_R_SUCCESS)
		return (result);

	key->key_bits = 0;
	for (i = 0; i < priv.nelements; i++) {
		switch (priv.elements[i].tag) {
		case TAG_HMACSHA256_KEY:
			isc_buffer_init(&b, priv.elements[i].data,
				        priv.elements[i].length);
			isc_buffer_add(&b, priv.elements[i].length);
			tresult = hmacsha256_fromdns(key, &b);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		case TAG_HMACSHA256_BITS:
			tresult = getkeybits(key, &priv.elements[i]);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		default:
			result = DST_R_INVALIDPRIVATEKEY;
			break;
		}
	}
	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));
	return (result);
}

static dst_func_t hmacsha256_functions = {
	hmacsha256_createctx,
	hmacsha256_destroyctx,
	hmacsha256_adddata,
	hmacsha256_sign,
	hmacsha256_verify,
	NULL, /* computesecret */
	hmacsha256_compare,
	NULL, /* paramcompare */
	hmacsha256_generate,
	hmacsha256_isprivate,
	hmacsha256_destroy,
	hmacsha256_todns,
	hmacsha256_fromdns,
	hmacsha256_tofile,
	hmacsha256_parse,
	NULL, /* cleanup */
};

isc_result_t
dst__hmacsha256_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &hmacsha256_functions;
	return (ISC_R_SUCCESS);
}

static isc_result_t hmacsha384_fromdns(dst_key_t *key, isc_buffer_t *data);

typedef struct {
	unsigned char key[ISC_SHA384_DIGESTLENGTH];
} HMACSHA384_Key;

static isc_result_t
hmacsha384_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_hmacsha384_t *hmacsha384ctx;
	HMACSHA384_Key *hkey = key->opaque;

	hmacsha384ctx = isc_mem_get(dctx->mctx, sizeof(isc_hmacsha384_t));
	if (hmacsha384ctx == NULL)
		return (ISC_R_NOMEMORY);
	isc_hmacsha384_init(hmacsha384ctx, hkey->key, ISC_SHA384_DIGESTLENGTH);
	dctx->opaque = hmacsha384ctx;
	return (ISC_R_SUCCESS);
}

static void
hmacsha384_destroyctx(dst_context_t *dctx) {
	isc_hmacsha384_t *hmacsha384ctx = dctx->opaque;

	if (hmacsha384ctx != NULL) {
		isc_hmacsha384_invalidate(hmacsha384ctx);
		isc_mem_put(dctx->mctx, hmacsha384ctx, sizeof(isc_hmacsha384_t));
		dctx->opaque = NULL;
	}
}

static isc_result_t
hmacsha384_adddata(dst_context_t *dctx, const isc_region_t *data) {
	isc_hmacsha384_t *hmacsha384ctx = dctx->opaque;

	isc_hmacsha384_update(hmacsha384ctx, data->base, data->length);
	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha384_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_hmacsha384_t *hmacsha384ctx = dctx->opaque;
	unsigned char *digest;

	if (isc_buffer_availablelength(sig) < ISC_SHA384_DIGESTLENGTH)
		return (ISC_R_NOSPACE);
	digest = isc_buffer_used(sig);
	isc_hmacsha384_sign(hmacsha384ctx, digest, ISC_SHA384_DIGESTLENGTH);
	isc_buffer_add(sig, ISC_SHA384_DIGESTLENGTH);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha384_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_hmacsha384_t *hmacsha384ctx = dctx->opaque;

	if (sig->length > ISC_SHA384_DIGESTLENGTH || sig->length == 0)
		return (DST_R_VERIFYFAILURE);

	if (isc_hmacsha384_verify(hmacsha384ctx, sig->base, sig->length))
		return (ISC_R_SUCCESS);
	else
		return (DST_R_VERIFYFAILURE);
}

static isc_boolean_t
hmacsha384_compare(const dst_key_t *key1, const dst_key_t *key2) {
	HMACSHA384_Key *hkey1, *hkey2;

	hkey1 = (HMACSHA384_Key *)key1->opaque;
	hkey2 = (HMACSHA384_Key *)key2->opaque;

	if (hkey1 == NULL && hkey2 == NULL)
		return (ISC_TRUE);
	else if (hkey1 == NULL || hkey2 == NULL)
		return (ISC_FALSE);

	if (memcmp(hkey1->key, hkey2->key, ISC_SHA384_DIGESTLENGTH) == 0)
		return (ISC_TRUE);
	else
		return (ISC_FALSE);
}

static isc_result_t
hmacsha384_generate(dst_key_t *key, int pseudorandom_ok) {
	isc_buffer_t b;
	isc_result_t ret;
	int bytes;
	unsigned char data[HMAC_LEN];

	bytes = (key->key_size + 7) / 8;
	if (bytes > HMAC_LEN) {
		bytes = HMAC_LEN;
		key->key_size = HMAC_LEN * 8;
	}

	memset(data, 0, HMAC_LEN);
	ret = dst__entropy_getdata(data, bytes, ISC_TF(pseudorandom_ok != 0));

	if (ret != ISC_R_SUCCESS)
		return (ret);

	isc_buffer_init(&b, data, bytes);
	isc_buffer_add(&b, bytes);
	ret = hmacsha384_fromdns(key, &b);
	memset(data, 0, ISC_SHA384_DIGESTLENGTH);

	return (ret);
}

static isc_boolean_t
hmacsha384_isprivate(const dst_key_t *key) {
	UNUSED(key);
	return (ISC_TRUE);
}

static void
hmacsha384_destroy(dst_key_t *key) {
	HMACSHA384_Key *hkey = key->opaque;
	memset(hkey, 0, sizeof(HMACSHA384_Key));
	isc_mem_put(key->mctx, hkey, sizeof(HMACSHA384_Key));
	key->opaque = NULL;
}

static isc_result_t
hmacsha384_todns(const dst_key_t *key, isc_buffer_t *data) {
	HMACSHA384_Key *hkey;
	unsigned int bytes;

	REQUIRE(key->opaque != NULL);

	hkey = (HMACSHA384_Key *) key->opaque;

	bytes = (key->key_size + 7) / 8;
	if (isc_buffer_availablelength(data) < bytes)
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(data, hkey->key, bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha384_fromdns(dst_key_t *key, isc_buffer_t *data) {
	HMACSHA384_Key *hkey;
	int keylen;
	isc_region_t r;
	isc_sha384_t sha384ctx;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	hkey = (HMACSHA384_Key *) isc_mem_get(key->mctx, sizeof(HMACSHA384_Key));
	if (hkey == NULL)
		return (ISC_R_NOMEMORY);

	memset(hkey->key, 0, sizeof(hkey->key));

	if (r.length > ISC_SHA384_DIGESTLENGTH) {
		isc_sha384_init(&sha384ctx);
		isc_sha384_update(&sha384ctx, r.base, r.length);
		isc_sha384_final(hkey->key, &sha384ctx);
		keylen = ISC_SHA384_DIGESTLENGTH;
	}
	else {
		memcpy(hkey->key, r.base, r.length);
		keylen = r.length;
	}

	key->key_size = keylen * 8;
	key->opaque = hkey;

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha384_tofile(const dst_key_t *key, const char *directory) {
	int cnt = 0;
	HMACSHA384_Key *hkey;
	dst_private_t priv;
	int bytes = (key->key_size + 7) / 8;
	unsigned char buf[2];

	if (key->opaque == NULL)
		return (DST_R_NULLKEY);

	hkey = (HMACSHA384_Key *) key->opaque;

	priv.elements[cnt].tag = TAG_HMACSHA384_KEY;
	priv.elements[cnt].length = bytes;
	priv.elements[cnt++].data = hkey->key;

	buf[0] = (key->key_bits >> 8) & 0xffU;
	buf[1] = key->key_bits & 0xffU;
	priv.elements[cnt].tag = TAG_HMACSHA384_BITS;
	priv.elements[cnt].data = buf;
	priv.elements[cnt++].length = 2;

	priv.nelements = cnt;
	return (dst__privstruct_writefile(key, &priv, directory));
}

static isc_result_t
hmacsha384_parse(dst_key_t *key, isc_lex_t *lexer) {
	dst_private_t priv;
	isc_result_t result, tresult;
	isc_buffer_t b;
	isc_mem_t *mctx = key->mctx;
	unsigned int i;

	/* read private key file */
	result = dst__privstruct_parse(key, DST_ALG_HMACSHA384, lexer, mctx,
				       &priv);
	if (result != ISC_R_SUCCESS)
		return (result);

	key->key_bits = 0;
	for (i = 0; i < priv.nelements; i++) {
		switch (priv.elements[i].tag) {
		case TAG_HMACSHA384_KEY:
			isc_buffer_init(&b, priv.elements[i].data,
				        priv.elements[i].length);
			isc_buffer_add(&b, priv.elements[i].length);
			tresult = hmacsha384_fromdns(key, &b);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		case TAG_HMACSHA384_BITS:
			tresult = getkeybits(key, &priv.elements[i]);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		default:
			result = DST_R_INVALIDPRIVATEKEY;
			break;
		}
	}
	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));
	return (result);
}

static dst_func_t hmacsha384_functions = {
	hmacsha384_createctx,
	hmacsha384_destroyctx,
	hmacsha384_adddata,
	hmacsha384_sign,
	hmacsha384_verify,
	NULL, /* computesecret */
	hmacsha384_compare,
	NULL, /* paramcompare */
	hmacsha384_generate,
	hmacsha384_isprivate,
	hmacsha384_destroy,
	hmacsha384_todns,
	hmacsha384_fromdns,
	hmacsha384_tofile,
	hmacsha384_parse,
	NULL, /* cleanup */
};

isc_result_t
dst__hmacsha384_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &hmacsha384_functions;
	return (ISC_R_SUCCESS);
}

static isc_result_t hmacsha512_fromdns(dst_key_t *key, isc_buffer_t *data);

typedef struct {
	unsigned char key[ISC_SHA512_DIGESTLENGTH];
} HMACSHA512_Key;

static isc_result_t
hmacsha512_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_hmacsha512_t *hmacsha512ctx;
	HMACSHA512_Key *hkey = key->opaque;

	hmacsha512ctx = isc_mem_get(dctx->mctx, sizeof(isc_hmacsha512_t));
	if (hmacsha512ctx == NULL)
		return (ISC_R_NOMEMORY);
	isc_hmacsha512_init(hmacsha512ctx, hkey->key, ISC_SHA512_DIGESTLENGTH);
	dctx->opaque = hmacsha512ctx;
	return (ISC_R_SUCCESS);
}

static void
hmacsha512_destroyctx(dst_context_t *dctx) {
	isc_hmacsha512_t *hmacsha512ctx = dctx->opaque;

	if (hmacsha512ctx != NULL) {
		isc_hmacsha512_invalidate(hmacsha512ctx);
		isc_mem_put(dctx->mctx, hmacsha512ctx, sizeof(isc_hmacsha512_t));
		dctx->opaque = NULL;
	}
}

static isc_result_t
hmacsha512_adddata(dst_context_t *dctx, const isc_region_t *data) {
	isc_hmacsha512_t *hmacsha512ctx = dctx->opaque;

	isc_hmacsha512_update(hmacsha512ctx, data->base, data->length);
	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha512_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_hmacsha512_t *hmacsha512ctx = dctx->opaque;
	unsigned char *digest;

	if (isc_buffer_availablelength(sig) < ISC_SHA512_DIGESTLENGTH)
		return (ISC_R_NOSPACE);
	digest = isc_buffer_used(sig);
	isc_hmacsha512_sign(hmacsha512ctx, digest, ISC_SHA512_DIGESTLENGTH);
	isc_buffer_add(sig, ISC_SHA512_DIGESTLENGTH);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha512_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_hmacsha512_t *hmacsha512ctx = dctx->opaque;

	if (sig->length > ISC_SHA512_DIGESTLENGTH || sig->length == 0)
		return (DST_R_VERIFYFAILURE);

	if (isc_hmacsha512_verify(hmacsha512ctx, sig->base, sig->length))
		return (ISC_R_SUCCESS);
	else
		return (DST_R_VERIFYFAILURE);
}

static isc_boolean_t
hmacsha512_compare(const dst_key_t *key1, const dst_key_t *key2) {
	HMACSHA512_Key *hkey1, *hkey2;

	hkey1 = (HMACSHA512_Key *)key1->opaque;
	hkey2 = (HMACSHA512_Key *)key2->opaque;

	if (hkey1 == NULL && hkey2 == NULL)
		return (ISC_TRUE);
	else if (hkey1 == NULL || hkey2 == NULL)
		return (ISC_FALSE);

	if (memcmp(hkey1->key, hkey2->key, ISC_SHA512_DIGESTLENGTH) == 0)
		return (ISC_TRUE);
	else
		return (ISC_FALSE);
}

static isc_result_t
hmacsha512_generate(dst_key_t *key, int pseudorandom_ok) {
	isc_buffer_t b;
	isc_result_t ret;
	int bytes;
	unsigned char data[HMAC_LEN];

	bytes = (key->key_size + 7) / 8;
	if (bytes > HMAC_LEN) {
		bytes = HMAC_LEN;
		key->key_size = HMAC_LEN * 8;
	}

	memset(data, 0, HMAC_LEN);
	ret = dst__entropy_getdata(data, bytes, ISC_TF(pseudorandom_ok != 0));

	if (ret != ISC_R_SUCCESS)
		return (ret);

	isc_buffer_init(&b, data, bytes);
	isc_buffer_add(&b, bytes);
	ret = hmacsha512_fromdns(key, &b);
	memset(data, 0, ISC_SHA512_DIGESTLENGTH);

	return (ret);
}

static isc_boolean_t
hmacsha512_isprivate(const dst_key_t *key) {
	UNUSED(key);
	return (ISC_TRUE);
}

static void
hmacsha512_destroy(dst_key_t *key) {
	HMACSHA512_Key *hkey = key->opaque;
	memset(hkey, 0, sizeof(HMACSHA512_Key));
	isc_mem_put(key->mctx, hkey, sizeof(HMACSHA512_Key));
	key->opaque = NULL;
}

static isc_result_t
hmacsha512_todns(const dst_key_t *key, isc_buffer_t *data) {
	HMACSHA512_Key *hkey;
	unsigned int bytes;

	REQUIRE(key->opaque != NULL);

	hkey = (HMACSHA512_Key *) key->opaque;

	bytes = (key->key_size + 7) / 8;
	if (isc_buffer_availablelength(data) < bytes)
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(data, hkey->key, bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha512_fromdns(dst_key_t *key, isc_buffer_t *data) {
	HMACSHA512_Key *hkey;
	int keylen;
	isc_region_t r;
	isc_sha512_t sha512ctx;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	hkey = (HMACSHA512_Key *) isc_mem_get(key->mctx, sizeof(HMACSHA512_Key));
	if (hkey == NULL)
		return (ISC_R_NOMEMORY);

	memset(hkey->key, 0, sizeof(hkey->key));

	if (r.length > ISC_SHA512_DIGESTLENGTH) {
		isc_sha512_init(&sha512ctx);
		isc_sha512_update(&sha512ctx, r.base, r.length);
		isc_sha512_final(hkey->key, &sha512ctx);
		keylen = ISC_SHA512_DIGESTLENGTH;
	}
	else {
		memcpy(hkey->key, r.base, r.length);
		keylen = r.length;
	}

	key->key_size = keylen * 8;
	key->opaque = hkey;

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha512_tofile(const dst_key_t *key, const char *directory) {
	int cnt = 0;
	HMACSHA512_Key *hkey;
	dst_private_t priv;
	int bytes = (key->key_size + 7) / 8;
	unsigned char buf[2];

	if (key->opaque == NULL)
		return (DST_R_NULLKEY);

	hkey = (HMACSHA512_Key *) key->opaque;

	priv.elements[cnt].tag = TAG_HMACSHA512_KEY;
	priv.elements[cnt].length = bytes;
	priv.elements[cnt++].data = hkey->key;

	buf[0] = (key->key_bits >> 8) & 0xffU;
	buf[1] = key->key_bits & 0xffU;
	priv.elements[cnt].tag = TAG_HMACSHA512_BITS;
	priv.elements[cnt].data = buf;
	priv.elements[cnt++].length = 2;

	priv.nelements = cnt;
	return (dst__privstruct_writefile(key, &priv, directory));
}

static isc_result_t
hmacsha512_parse(dst_key_t *key, isc_lex_t *lexer) {
	dst_private_t priv;
	isc_result_t result, tresult;
	isc_buffer_t b;
	isc_mem_t *mctx = key->mctx;
	unsigned int i;

	/* read private key file */
	result = dst__privstruct_parse(key, DST_ALG_HMACSHA512, lexer, mctx,
				       &priv);
	if (result != ISC_R_SUCCESS)
		return (result);

	key->key_bits = 0;
	for (i = 0; i < priv.nelements; i++) {
		switch (priv.elements[i].tag) {
		case TAG_HMACSHA512_KEY:
			isc_buffer_init(&b, priv.elements[i].data,
				        priv.elements[i].length);
			isc_buffer_add(&b, priv.elements[i].length);
			tresult = hmacsha512_fromdns(key, &b);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		case TAG_HMACSHA512_BITS:
			tresult = getkeybits(key, &priv.elements[i]);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		default:
			result = DST_R_INVALIDPRIVATEKEY;
			break;
		}
	}
	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));
	return (result);
}

static dst_func_t hmacsha512_functions = {
	hmacsha512_createctx,
	hmacsha512_destroyctx,
	hmacsha512_adddata,
	hmacsha512_sign,
	hmacsha512_verify,
	NULL, /* computesecret */
	hmacsha512_compare,
	NULL, /* paramcompare */
	hmacsha512_generate,
	hmacsha512_isprivate,
	hmacsha512_destroy,
	hmacsha512_todns,
	hmacsha512_fromdns,
	hmacsha512_tofile,
	hmacsha512_parse,
	NULL, /* cleanup */
};

isc_result_t
dst__hmacsha512_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &hmacsha512_functions;
	return (ISC_R_SUCCESS);
}

/*! \file */
