/*
 * Copyright (c) 1997 - 2004 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "krb5_locl.h"
RCSID("$Id: crypto.c,v 1.73.2.4 2004/03/06 16:38:00 lha Exp $");
/* RCSID("$FreeBSD: release/7.0.0/crypto/heimdal/lib/krb5/crypto.c 172506 2007-10-10 16:59:15Z cvs2svn $"); */

#undef CRYPTO_DEBUG
#ifdef CRYPTO_DEBUG
static void krb5_crypto_debug(krb5_context, int, size_t, krb5_keyblock*);
#endif


struct key_data {
    krb5_keyblock *key;
    krb5_data *schedule;
};

struct key_usage {
    unsigned usage;
    struct key_data key;
};

struct krb5_crypto_data {
    struct encryption_type *et;
    struct key_data key;
    int num_key_usage;
    struct key_usage *key_usage;
};

#define CRYPTO_ETYPE(C) ((C)->et->type)

/* bits for `flags' below */
#define F_KEYED		 1	/* checksum is keyed */
#define F_CPROOF	 2	/* checksum is collision proof */
#define F_DERIVED	 4	/* uses derived keys */
#define F_VARIANT	 8	/* uses `variant' keys (6.4.3) */
#define F_PSEUDO	16	/* not a real protocol type */
#define F_SPECIAL	32	/* backwards */

struct salt_type {
    krb5_salttype type;
    const char *name;
    krb5_error_code (*string_to_key)(krb5_context, krb5_enctype, krb5_data, 
				     krb5_salt, krb5_data, krb5_keyblock*);
};

struct key_type {
    krb5_keytype type; /* XXX */
    const char *name;
    size_t bits;
    size_t size;
    size_t schedule_size;
#if 0
    krb5_enctype best_etype;
#endif
    void (*random_key)(krb5_context, krb5_keyblock*);
    void (*schedule)(krb5_context, struct key_data *);
    struct salt_type *string_to_key;
};

struct checksum_type {
    krb5_cksumtype type;
    const char *name;
    size_t blocksize;
    size_t checksumsize;
    unsigned flags;
    void (*checksum)(krb5_context context,
		     struct key_data *key,
		     const void *buf, size_t len,
		     unsigned usage,
		     Checksum *csum);
    krb5_error_code (*verify)(krb5_context context,
			      struct key_data *key,
			      const void *buf, size_t len,
			      unsigned usage,
			      Checksum *csum);
};

struct encryption_type {
    krb5_enctype type;
    const char *name;
    size_t blocksize;
    size_t padsize;
    size_t confoundersize;
    struct key_type *keytype;
    struct checksum_type *checksum;
    struct checksum_type *keyed_checksum;
    unsigned flags;
    krb5_error_code (*encrypt)(krb5_context context,
			       struct key_data *key,
			       void *data, size_t len,
			       krb5_boolean encrypt,
			       int usage,
			       void *ivec);
};

#define ENCRYPTION_USAGE(U) (((U) << 8) | 0xAA)
#define INTEGRITY_USAGE(U) (((U) << 8) | 0x55)
#define CHECKSUM_USAGE(U) (((U) << 8) | 0x99)

static struct checksum_type *_find_checksum(krb5_cksumtype type);
static struct encryption_type *_find_enctype(krb5_enctype type);
static struct key_type *_find_keytype(krb5_keytype type);
static krb5_error_code _get_derived_key(krb5_context, krb5_crypto, 
					unsigned, struct key_data**);
static struct key_data *_new_derived_key(krb5_crypto crypto, unsigned usage);
static krb5_error_code derive_key(krb5_context context,
				  struct encryption_type *et,
				  struct key_data *key,
				  const void *constant,
				  size_t len);
static krb5_error_code hmac(krb5_context context,
			    struct checksum_type *cm, 
			    const void *data, 
			    size_t len, 
			    unsigned usage,
			    struct key_data *keyblock,
			    Checksum *result);
static void free_key_data(krb5_context context, struct key_data *key);
static krb5_error_code usage2arcfour (krb5_context, int *);

/************************************************************
 *                                                          *
 ************************************************************/

static void
krb5_DES_random_key(krb5_context context,
	       krb5_keyblock *key)
{
    des_cblock *k = key->keyvalue.data;
    do {
	krb5_generate_random_block(k, sizeof(des_cblock));
	des_set_odd_parity(k);
    } while(des_is_weak_key(k));
}

static void
krb5_DES_schedule(krb5_context context,
	     struct key_data *key)
{
    des_set_key(key->key->keyvalue.data, key->schedule->data);
}

static void
DES_string_to_key_int(unsigned char *data, size_t length, des_cblock *key)
{
    des_key_schedule schedule;
    int i;
    int reverse = 0;
    unsigned char *p;

    unsigned char swap[] = { 0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe, 
			     0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf };
    memset(key, 0, 8);
    
    p = (unsigned char*)key;
    for (i = 0; i < length; i++) {
	unsigned char tmp = data[i];
	if (!reverse)
	    *p++ ^= (tmp << 1);
	else
	    *--p ^= (swap[tmp & 0xf] << 4) | swap[(tmp & 0xf0) >> 4];
	if((i % 8) == 7)
	    reverse = !reverse;
    }
    des_set_odd_parity(key);
    if(des_is_weak_key(key))
	(*key)[7] ^= 0xF0;
    des_set_key(key, schedule);
    des_cbc_cksum((void*)data, key, length, schedule, key);
    memset(schedule, 0, sizeof(schedule));
    des_set_odd_parity(key);
}

static krb5_error_code
krb5_DES_string_to_key(krb5_context context,
		  krb5_enctype enctype,
		  krb5_data password,
		  krb5_salt salt,
		  krb5_data opaque,
		  krb5_keyblock *key)
{
    unsigned char *s;
    size_t len;
    des_cblock tmp;

    len = password.length + salt.saltvalue.length;
    s = malloc(len);
    if(len > 0 && s == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    memcpy(s, password.data, password.length);
    memcpy(s + password.length, salt.saltvalue.data, salt.saltvalue.length);
    DES_string_to_key_int(s, len, &tmp);
    key->keytype = enctype;
    krb5_data_copy(&key->keyvalue, tmp, sizeof(tmp));
    memset(&tmp, 0, sizeof(tmp));
    memset(s, 0, len);
    free(s);
    return 0;
}

/* This defines the Andrew string_to_key function.  It accepts a password
 * string as input and converts its via a one-way encryption algorithm to a DES
 * encryption key.  It is compatible with the original Andrew authentication
 * service password database.
 */

/*
 * Short passwords, i.e 8 characters or less.
 */
static void
krb5_DES_AFS3_CMU_string_to_key (krb5_data pw,
			    krb5_data cell,
			    des_cblock *key)
{
    char  password[8+1];	/* crypt is limited to 8 chars anyway */
    int   i;
    
    for(i = 0; i < 8; i++) {
	char c = ((i < pw.length) ? ((char*)pw.data)[i] : 0) ^
		 ((i < cell.length) ?
		  tolower(((unsigned char*)cell.data)[i]) : 0);
	password[i] = c ? c : 'X';
    }
    password[8] = '\0';

    memcpy(key, crypt(password, "p1") + 2, sizeof(des_cblock));

    /* parity is inserted into the LSB so left shift each byte up one
       bit. This allows ascii characters with a zero MSB to retain as
       much significance as possible. */
    for (i = 0; i < sizeof(des_cblock); i++)
	((unsigned char*)key)[i] <<= 1;
    des_set_odd_parity (key);
}

/*
 * Long passwords, i.e 9 characters or more.
 */
static void
krb5_DES_AFS3_Transarc_string_to_key (krb5_data pw,
				 krb5_data cell,
				 des_cblock *key)
{
    des_key_schedule schedule;
    des_cblock temp_key;
    des_cblock ivec;
    char password[512];
    size_t passlen;

    memcpy(password, pw.data, min(pw.length, sizeof(password)));
    if(pw.length < sizeof(password)) {
	int len = min(cell.length, sizeof(password) - pw.length);
	int i;

	memcpy(password + pw.length, cell.data, len);
	for (i = pw.length; i < pw.length + len; ++i)
	    password[i] = tolower((unsigned char)password[i]);
    }
    passlen = min(sizeof(password), pw.length + cell.length);
    memcpy(&ivec, "kerberos", 8);
    memcpy(&temp_key, "kerberos", 8);
    des_set_odd_parity (&temp_key);
    des_set_key (&temp_key, schedule);
    des_cbc_cksum (password, &ivec, passlen, schedule, &ivec);

    memcpy(&temp_key, &ivec, 8);
    des_set_odd_parity (&temp_key);
    des_set_key (&temp_key, schedule);
    des_cbc_cksum (password, key, passlen, schedule, &ivec);
    memset(&schedule, 0, sizeof(schedule));
    memset(&temp_key, 0, sizeof(temp_key));
    memset(&ivec, 0, sizeof(ivec));
    memset(password, 0, sizeof(password));

    des_set_odd_parity (key);
}

static krb5_error_code
DES_AFS3_string_to_key(krb5_context context,
		       krb5_enctype enctype,
		       krb5_data password,
		       krb5_salt salt,
		       krb5_data opaque,
		       krb5_keyblock *key)
{
    des_cblock tmp;
    if(password.length > 8)
	krb5_DES_AFS3_Transarc_string_to_key(password, salt.saltvalue, &tmp);
    else
	krb5_DES_AFS3_CMU_string_to_key(password, salt.saltvalue, &tmp);
    key->keytype = enctype;
    krb5_data_copy(&key->keyvalue, tmp, sizeof(tmp));
    memset(&key, 0, sizeof(key));
    return 0;
}

static void
DES3_random_key(krb5_context context,
		krb5_keyblock *key)
{
    des_cblock *k = key->keyvalue.data;
    do {
	krb5_generate_random_block(k, 3 * sizeof(des_cblock));
	des_set_odd_parity(&k[0]);
	des_set_odd_parity(&k[1]);
	des_set_odd_parity(&k[2]);
    } while(des_is_weak_key(&k[0]) ||
	    des_is_weak_key(&k[1]) ||
	    des_is_weak_key(&k[2]));
}

static void
DES3_schedule(krb5_context context,
	      struct key_data *key)
{
    des_cblock *k = key->key->keyvalue.data;
    des_key_schedule *s = key->schedule->data;
    des_set_key(&k[0], s[0]);
    des_set_key(&k[1], s[1]);
    des_set_key(&k[2], s[2]);
}

/*
 * A = A xor B. A & B are 8 bytes.
 */

static void
xor (des_cblock *key, const unsigned char *b)
{
    unsigned char *a = (unsigned char*)key;
    a[0] ^= b[0];
    a[1] ^= b[1];
    a[2] ^= b[2];
    a[3] ^= b[3];
    a[4] ^= b[4];
    a[5] ^= b[5];
    a[6] ^= b[6];
    a[7] ^= b[7];
}

static krb5_error_code
DES3_string_to_key(krb5_context context,
		   krb5_enctype enctype,
		   krb5_data password,
		   krb5_salt salt,
		   krb5_data opaque,
		   krb5_keyblock *key)
{
    char *str;
    size_t len;
    unsigned char tmp[24];
    des_cblock keys[3];
    
    len = password.length + salt.saltvalue.length;
    str = malloc(len);
    if(len != 0 && str == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    memcpy(str, password.data, password.length);
    memcpy(str + password.length, salt.saltvalue.data, salt.saltvalue.length);
    {
	des_cblock ivec;
	des_key_schedule s[3];
	int i;
	
	_krb5_n_fold(str, len, tmp, 24);
	
	for(i = 0; i < 3; i++){
	    memcpy(keys + i, tmp + i * 8, sizeof(keys[i]));
	    des_set_odd_parity(keys + i);
	    if(des_is_weak_key(keys + i))
		xor(keys + i, (const unsigned char*)"\0\0\0\0\0\0\0\xf0");
	    des_set_key(keys + i, s[i]);
	}
	memset(&ivec, 0, sizeof(ivec));
	des_ede3_cbc_encrypt(tmp,
			     tmp, sizeof(tmp), 
			     s[0], s[1], s[2], &ivec, DES_ENCRYPT);
	memset(s, 0, sizeof(s));
	memset(&ivec, 0, sizeof(ivec));
	for(i = 0; i < 3; i++){
	    memcpy(keys + i, tmp + i * 8, sizeof(keys[i]));
	    des_set_odd_parity(keys + i);
	    if(des_is_weak_key(keys + i))
		xor(keys + i, (const unsigned char*)"\0\0\0\0\0\0\0\xf0");
	}
	memset(tmp, 0, sizeof(tmp));
    }
    key->keytype = enctype;
    krb5_data_copy(&key->keyvalue, keys, sizeof(keys));
    memset(keys, 0, sizeof(keys));
    memset(str, 0, len);
    free(str);
    return 0;
}

static krb5_error_code
DES3_string_to_key_derived(krb5_context context,
			   krb5_enctype enctype,
			   krb5_data password,
			   krb5_salt salt,
			   krb5_data opaque,
			   krb5_keyblock *key)
{
    krb5_error_code ret;
    size_t len = password.length + salt.saltvalue.length;
    char *s;

    s = malloc(len);
    if(len != 0 && s == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    memcpy(s, password.data, password.length);
    memcpy(s + password.length, salt.saltvalue.data, salt.saltvalue.length);
    ret = krb5_string_to_key_derived(context,
				     s,
				     len,
				     enctype,
				     key);
    memset(s, 0, len);
    free(s);
    return ret;
}

/*
 * ARCFOUR
 */

static void
ARCFOUR_random_key(krb5_context context, krb5_keyblock *key)
{
    krb5_generate_random_block (key->keyvalue.data,
				key->keyvalue.length);
}

static void
ARCFOUR_schedule(krb5_context context, struct key_data *kd)
{
    RC4_set_key (kd->schedule->data,
		 kd->key->keyvalue.length, kd->key->keyvalue.data);
}

static krb5_error_code
ARCFOUR_string_to_key(krb5_context context,
		  krb5_enctype enctype,
		  krb5_data password,
		  krb5_salt salt,
		  krb5_data opaque,
		  krb5_keyblock *key)
{
    char *s, *p;
    size_t len;
    int i;
    MD4_CTX m;

    len = 2 * password.length;
    s = malloc (len);
    if (len != 0 && s == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    for (p = s, i = 0; i < password.length; ++i) {
	*p++ = ((char *)password.data)[i];
	*p++ = 0;
    }
    MD4_Init (&m);
    MD4_Update (&m, s, len);
    key->keytype = enctype;
    krb5_data_alloc (&key->keyvalue, 16);
    MD4_Final (key->keyvalue.data, &m);
    memset (s, 0, len);
    free (s);
    return 0;
}

#ifdef ENABLE_AES
/*
 * AES
 */

/* iter is really 1 based, so iter == 0 will be 1 iteration */

krb5_error_code
krb5_PKCS5_PBKDF2(krb5_context context, krb5_cksumtype cktype,
		  krb5_data password, krb5_salt salt, u_int32_t iter,
		  krb5_keytype type, krb5_keyblock *key)
{
    struct checksum_type *c = _find_checksum(cktype);
    struct key_type *kt;
    size_t datalen, leftofkey;
    krb5_error_code ret;
    u_int32_t keypart;
    struct key_data ksign;
    krb5_keyblock kb;
    Checksum result;
    char *data, *tmpcksum;
    int i, j;
    char *p;
    
    if (c == NULL) {
	krb5_set_error_string(context, "checksum %d not supported", cktype);
	return KRB5_PROG_KEYTYPE_NOSUPP;
    }

    kt = _find_keytype(type);
    if (kt == NULL) {
	krb5_set_error_string(context, "key type %d not supported", type);
	return KRB5_PROG_KEYTYPE_NOSUPP;
    }
    
    key->keytype = type;
    ret = krb5_data_alloc (&key->keyvalue, kt->bits / 8);
    if (ret) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ret;
    }
	
    ret = krb5_data_alloc (&result.checksum, c->checksumsize);
    if (ret) {
	krb5_set_error_string(context, "malloc: out of memory");
	krb5_data_free (&key->keyvalue);
	return ret;
    }

    tmpcksum = malloc(c->checksumsize);
    if (tmpcksum == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	krb5_data_free (&key->keyvalue);
	krb5_data_free (&result.checksum);
	return ENOMEM;
    }

    datalen = salt.saltvalue.length + 4;
    data = malloc(datalen);
    if (data == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	free(tmpcksum);
	krb5_data_free (&key->keyvalue);
	krb5_data_free (&result.checksum);
	return ENOMEM;
    }

    kb.keyvalue = password;
    ksign.key = &kb;

    memcpy(data, salt.saltvalue.data, salt.saltvalue.length);

    keypart = 1;
    leftofkey = key->keyvalue.length;
    p = key->keyvalue.data;

    while (leftofkey) {
	int len;

	if (leftofkey > c->checksumsize)
	    len = c->checksumsize;
	else
	    len = leftofkey;

	_krb5_put_int(data + datalen - 4, keypart, 4);

	ret = hmac(context, c, data, datalen, 0, &ksign, &result);
	if (ret)
	    krb5_abortx(context, "hmac failed");
	memcpy(p, result.checksum.data, len);
	memcpy(tmpcksum, result.checksum.data, result.checksum.length);
	for (i = 0; i < iter; i++) {
	    ret = hmac(context, c, tmpcksum, result.checksum.length,
		       0, &ksign, &result);
	    if (ret)
		krb5_abortx(context, "hmac failed");
	    memcpy(tmpcksum, result.checksum.data, result.checksum.length);
	    for (j = 0; j < len; j++)
		p[j] ^= tmpcksum[j];
	}

	p += len;
	leftofkey -= len;
	keypart++;
    }

    free(data);
    free(tmpcksum);
    krb5_data_free (&result.checksum);

    return 0;
}

static krb5_error_code
AES_string_to_key(krb5_context context,
		  krb5_enctype enctype,
		  krb5_data password,
		  krb5_salt salt,
		  krb5_data opaque,
		  krb5_keyblock *key)
{
    krb5_error_code ret;
    u_int32_t iter;
    struct encryption_type *et;
    struct key_data kd;

    if (opaque.length == 0)
	iter = 45056 - 1;
    else if (opaque.length == 4) {
	unsigned long v;
	_krb5_get_int(opaque.data, &v, 4);
	iter = ((u_int32_t)v) - 1;
    } else
	return KRB5_PROG_KEYTYPE_NOSUPP; /* XXX */
	

    et = _find_enctype(enctype);
    if (et == NULL)
	return KRB5_PROG_KEYTYPE_NOSUPP;

    ret = krb5_PKCS5_PBKDF2(context, CKSUMTYPE_SHA1, password, salt, 
			    iter, enctype, key);
    if (ret)
	return ret;

    ret = krb5_copy_keyblock(context, key, &kd.key);
    kd.schedule = NULL;

    ret = derive_key(context, et, &kd, "kerberos", strlen("kerberos"));

    if (ret) {
	krb5_data_free(&key->keyvalue);
    } else {
	ret = krb5_copy_keyblock_contents(context, kd.key, key);
	free_key_data(context, &kd);
    }

    return ret;
}

static void
AES_schedule(krb5_context context, struct key_data *kd)
{
    AES_KEY *key = kd->schedule->data;
    int bits = kd->key->keyvalue.length * 8;
    
    AES_set_encrypt_key(kd->key->keyvalue.data, bits, &key[0]);
    AES_set_decrypt_key(kd->key->keyvalue.data, bits, &key[1]);
}

/*
 *
 */

extern struct salt_type AES_salt[];

#endif /* ENABLE_AES */

extern struct salt_type des_salt[], 
    des3_salt[], des3_salt_derived[], arcfour_salt[];

struct key_type keytype_null = {
    KEYTYPE_NULL,
    "null",
    0,
    0,
    0,
    NULL,
    NULL,
    NULL
};

struct key_type keytype_des = {
    KEYTYPE_DES,
    "des",
    56,
    sizeof(des_cblock),
    sizeof(des_key_schedule),
    krb5_DES_random_key,
    krb5_DES_schedule,
    des_salt
};

struct key_type keytype_des3 = {
    KEYTYPE_DES3,
    "des3",
    168,
    3 * sizeof(des_cblock), 
    3 * sizeof(des_key_schedule), 
    DES3_random_key,
    DES3_schedule,
    des3_salt
};

struct key_type keytype_des3_derived = {
    KEYTYPE_DES3,
    "des3",
    168,
    3 * sizeof(des_cblock),
    3 * sizeof(des_key_schedule), 
    DES3_random_key,
    DES3_schedule,
    des3_salt_derived
};

#ifdef ENABLE_AES
struct key_type keytype_aes128 = {
    KEYTYPE_AES128,
    "aes-128",
    128,
    16,
    sizeof(AES_KEY) * 2,
    NULL,
    AES_schedule,
    AES_salt
};

struct key_type keytype_aes256 = {
    KEYTYPE_AES256,
    "aes-256",
    256,
    16,
    sizeof(AES_KEY) * 2,
    NULL,
    AES_schedule,
    AES_salt
};
#endif /* ENABLE_AES */

struct key_type keytype_arcfour = {
    KEYTYPE_ARCFOUR,
    "arcfour",
    128,
    16,
    sizeof(RC4_KEY),
    ARCFOUR_random_key,
    ARCFOUR_schedule,
    arcfour_salt
};

struct key_type *keytypes[] = {
    &keytype_null,
    &keytype_des,
    &keytype_des3_derived,
    &keytype_des3,
#ifdef ENABLE_AES
    &keytype_aes128,
    &keytype_aes256,
#endif /* ENABLE_AES */
    &keytype_arcfour
};

static int num_keytypes = sizeof(keytypes) / sizeof(keytypes[0]);

static struct key_type *
_find_keytype(krb5_keytype type)
{
    int i;
    for(i = 0; i < num_keytypes; i++)
	if(keytypes[i]->type == type)
	    return keytypes[i];
    return NULL;
}


struct salt_type des_salt[] = {
    {
	KRB5_PW_SALT,
	"pw-salt",
	krb5_DES_string_to_key
    },
    {
	KRB5_AFS3_SALT,
	"afs3-salt",
	DES_AFS3_string_to_key
    },
    { 0 }
};

struct salt_type des3_salt[] = {
    {
	KRB5_PW_SALT,
	"pw-salt",
	DES3_string_to_key
    },
    { 0 }
};

struct salt_type des3_salt_derived[] = {
    {
	KRB5_PW_SALT,
	"pw-salt",
	DES3_string_to_key_derived
    },
    { 0 }
};

#ifdef ENABLE_AES
struct salt_type AES_salt[] = {
    {
	KRB5_PW_SALT,
	"pw-salt",
	AES_string_to_key
    },
    { 0 }
};
#endif /* ENABLE_AES */

struct salt_type arcfour_salt[] = {
    {
	KRB5_PW_SALT,
	"pw-salt",
	ARCFOUR_string_to_key
    },
    { 0 }
};

krb5_error_code
krb5_salttype_to_string (krb5_context context,
			 krb5_enctype etype,
			 krb5_salttype stype,
			 char **string)
{
    struct encryption_type *e;
    struct salt_type *st;

    e = _find_enctype (etype);
    if (e == NULL) {
	krb5_set_error_string(context, "encryption type %d not supported",
			      etype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    for (st = e->keytype->string_to_key; st && st->type; st++) {
	if (st->type == stype) {
	    *string = strdup (st->name);
	    if (*string == NULL) {
		krb5_set_error_string(context, "malloc: out of memory");
		return ENOMEM;
	    }
	    return 0;
	}
    }
    krb5_set_error_string(context, "salttype %d not supported", stype);
    return HEIM_ERR_SALTTYPE_NOSUPP;
}

krb5_error_code
krb5_string_to_salttype (krb5_context context,
			 krb5_enctype etype,
			 const char *string,
			 krb5_salttype *salttype)
{
    struct encryption_type *e;
    struct salt_type *st;

    e = _find_enctype (etype);
    if (e == NULL) {
	krb5_set_error_string(context, "encryption type %d not supported",
			      etype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    for (st = e->keytype->string_to_key; st && st->type; st++) {
	if (strcasecmp (st->name, string) == 0) {
	    *salttype = st->type;
	    return 0;
	}
    }
    krb5_set_error_string(context, "salttype %s not supported", string);
    return HEIM_ERR_SALTTYPE_NOSUPP;
}

krb5_error_code
krb5_get_pw_salt(krb5_context context,
		 krb5_const_principal principal,
		 krb5_salt *salt)
{
    size_t len;
    int i;
    krb5_error_code ret;
    char *p;
     
    salt->salttype = KRB5_PW_SALT;
    len = strlen(principal->realm);
    for (i = 0; i < principal->name.name_string.len; ++i)
	len += strlen(principal->name.name_string.val[i]);
    ret = krb5_data_alloc (&salt->saltvalue, len);
    if (ret)
	return ret;
    p = salt->saltvalue.data;
    memcpy (p, principal->realm, strlen(principal->realm));
    p += strlen(principal->realm);
    for (i = 0; i < principal->name.name_string.len; ++i) {
	memcpy (p,
		principal->name.name_string.val[i],
		strlen(principal->name.name_string.val[i]));
	p += strlen(principal->name.name_string.val[i]);
    }
    return 0;
}

krb5_error_code
krb5_free_salt(krb5_context context, 
	       krb5_salt salt)
{
    krb5_data_free(&salt.saltvalue);
    return 0;
}

krb5_error_code
krb5_string_to_key_data (krb5_context context,
			 krb5_enctype enctype,
			 krb5_data password,
			 krb5_principal principal,
			 krb5_keyblock *key)
{
    krb5_error_code ret;
    krb5_salt salt;

    ret = krb5_get_pw_salt(context, principal, &salt);
    if(ret)
	return ret;
    ret = krb5_string_to_key_data_salt(context, enctype, password, salt, key);
    krb5_free_salt(context, salt);
    return ret;
}

krb5_error_code
krb5_string_to_key (krb5_context context,
		    krb5_enctype enctype,
		    const char *password,
		    krb5_principal principal,
		    krb5_keyblock *key)
{
    krb5_data pw;
    pw.data = (void*)password;
    pw.length = strlen(password);
    return krb5_string_to_key_data(context, enctype, pw, principal, key);
}

krb5_error_code
krb5_string_to_key_data_salt (krb5_context context,
			      krb5_enctype enctype,
			      krb5_data password,
			      krb5_salt salt,
			      krb5_keyblock *key)
{
    krb5_data opaque;
    krb5_data_zero(&opaque);
    return krb5_string_to_key_data_salt_opaque(context, enctype, password, 
					       salt, opaque, key);
}

/*
 * Do a string -> key for encryption type `enctype' operation on
 * `password' (with salt `salt' and the enctype specific data string
 * `opaque'), returning the resulting key in `key'
 */

krb5_error_code
krb5_string_to_key_data_salt_opaque (krb5_context context,
				     krb5_enctype enctype,
				     krb5_data password,
				     krb5_salt salt,
				     krb5_data opaque,
				     krb5_keyblock *key)
{
    struct encryption_type *et =_find_enctype(enctype);
    struct salt_type *st;
    if(et == NULL) {
	krb5_set_error_string(context, "encryption type %d not supported",
			      enctype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    for(st = et->keytype->string_to_key; st && st->type; st++) 
	if(st->type == salt.salttype)
	    return (*st->string_to_key)(context, enctype, password, 
					salt, opaque, key);
    krb5_set_error_string(context, "salt type %d not supported",
			  salt.salttype);
    return HEIM_ERR_SALTTYPE_NOSUPP;
}

/*
 * Do a string -> key for encryption type `enctype' operation on the
 * string `password' (with salt `salt'), returning the resulting key
 * in `key'
 */

krb5_error_code
krb5_string_to_key_salt (krb5_context context,
			 krb5_enctype enctype,
			 const char *password,
			 krb5_salt salt,
			 krb5_keyblock *key)
{
    krb5_data pw;
    pw.data = (void*)password;
    pw.length = strlen(password);
    return krb5_string_to_key_data_salt(context, enctype, pw, salt, key);
}

krb5_error_code
krb5_keytype_to_string(krb5_context context,
		       krb5_keytype keytype,
		       char **string)
{
    struct key_type *kt = _find_keytype(keytype);
    if(kt == NULL) {
	krb5_set_error_string(context, "key type %d not supported", keytype);
	return KRB5_PROG_KEYTYPE_NOSUPP;
    }
    *string = strdup(kt->name);
    if(*string == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    return 0;
}

krb5_error_code
krb5_string_to_keytype(krb5_context context,
		       const char *string,
		       krb5_keytype *keytype)
{
    int i;
    for(i = 0; i < num_keytypes; i++)
	if(strcasecmp(keytypes[i]->name, string) == 0){
	    *keytype = keytypes[i]->type;
	    return 0;
	}
    krb5_set_error_string(context, "key type %s not supported", string);
    return KRB5_PROG_KEYTYPE_NOSUPP;
}

krb5_error_code
krb5_enctype_keysize(krb5_context context,
		     krb5_enctype type,
		     size_t *keysize)
{
    struct encryption_type *et = _find_enctype(type);
    if(et == NULL) {
	krb5_set_error_string(context, "encryption type %d not supported",
			      type);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    *keysize = et->keytype->size;
    return 0;
}

krb5_error_code
krb5_generate_random_keyblock(krb5_context context,
			      krb5_enctype type,
			      krb5_keyblock *key)
{
    krb5_error_code ret;
    struct encryption_type *et = _find_enctype(type);
    if(et == NULL) {
	krb5_set_error_string(context, "encryption type %d not supported",
			      type);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    ret = krb5_data_alloc(&key->keyvalue, et->keytype->size);
    if(ret) 
	return ret;
    key->keytype = type;
    if(et->keytype->random_key)
	(*et->keytype->random_key)(context, key);
    else
	krb5_generate_random_block(key->keyvalue.data, 
				   key->keyvalue.length);
    return 0;
}

static krb5_error_code
_key_schedule(krb5_context context,
	      struct key_data *key)
{
    krb5_error_code ret;
    struct encryption_type *et = _find_enctype(key->key->keytype);
    struct key_type *kt = et->keytype;

    if(kt->schedule == NULL)
	return 0;
    if (key->schedule != NULL)
	return 0;
    ALLOC(key->schedule, 1);
    if(key->schedule == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    ret = krb5_data_alloc(key->schedule, kt->schedule_size);
    if(ret) {
	free(key->schedule);
	key->schedule = NULL;
	return ret;
    }
    (*kt->schedule)(context, key);
    return 0;
}

/************************************************************
 *                                                          *
 ************************************************************/

static void
NONE_checksum(krb5_context context,
	      struct key_data *key,
	      const void *data,
	      size_t len,
	      unsigned usage,
	      Checksum *C)
{
}

static void
CRC32_checksum(krb5_context context,
	       struct key_data *key,
	       const void *data,
	       size_t len,
	       unsigned usage,
	       Checksum *C)
{
    u_int32_t crc;
    unsigned char *r = C->checksum.data;
    _krb5_crc_init_table ();
    crc = _krb5_crc_update (data, len, 0);
    r[0] = crc & 0xff;
    r[1] = (crc >> 8)  & 0xff;
    r[2] = (crc >> 16) & 0xff;
    r[3] = (crc >> 24) & 0xff;
}

static void
RSA_MD4_checksum(krb5_context context,
		 struct key_data *key,
		 const void *data,
		 size_t len,
		 unsigned usage,
		 Checksum *C)
{
    MD4_CTX m;

    MD4_Init (&m);
    MD4_Update (&m, data, len);
    MD4_Final (C->checksum.data, &m);
}

static void
RSA_MD4_DES_checksum(krb5_context context, 
		     struct key_data *key,
		     const void *data, 
		     size_t len, 
		     unsigned usage,
		     Checksum *cksum)
{
    MD4_CTX md4;
    des_cblock ivec;
    unsigned char *p = cksum->checksum.data;
    
    krb5_generate_random_block(p, 8);
    MD4_Init (&md4);
    MD4_Update (&md4, p, 8);
    MD4_Update (&md4, data, len);
    MD4_Final (p + 8, &md4);
    memset (&ivec, 0, sizeof(ivec));
    des_cbc_encrypt(p, 
		    p, 
		    24, 
		    key->schedule->data, 
		    &ivec, 
		    DES_ENCRYPT);
}

static krb5_error_code
RSA_MD4_DES_verify(krb5_context context,
		   struct key_data *key,
		   const void *data,
		   size_t len,
		   unsigned usage,
		   Checksum *C)
{
    MD4_CTX md4;
    unsigned char tmp[24];
    unsigned char res[16];
    des_cblock ivec;
    krb5_error_code ret = 0;

    memset(&ivec, 0, sizeof(ivec));
    des_cbc_encrypt(C->checksum.data,
		    (void*)tmp, 
		    C->checksum.length, 
		    key->schedule->data,
		    &ivec,
		    DES_DECRYPT);
    MD4_Init (&md4);
    MD4_Update (&md4, tmp, 8); /* confounder */
    MD4_Update (&md4, data, len);
    MD4_Final (res, &md4);
    if(memcmp(res, tmp + 8, sizeof(res)) != 0) {
	krb5_clear_error_string (context);
	ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
    }
    memset(tmp, 0, sizeof(tmp));
    memset(res, 0, sizeof(res));
    return ret;
}

static void
RSA_MD5_checksum(krb5_context context,
		 struct key_data *key,
		 const void *data,
		 size_t len,
		 unsigned usage,
		 Checksum *C)
{
    MD5_CTX m;

    MD5_Init  (&m);
    MD5_Update(&m, data, len);
    MD5_Final (C->checksum.data, &m);
}

static void
RSA_MD5_DES_checksum(krb5_context context,
		     struct key_data *key,
		     const void *data,
		     size_t len,
		     unsigned usage,
		     Checksum *C)
{
    MD5_CTX md5;
    des_cblock ivec;
    unsigned char *p = C->checksum.data;
    
    krb5_generate_random_block(p, 8);
    MD5_Init (&md5);
    MD5_Update (&md5, p, 8);
    MD5_Update (&md5, data, len);
    MD5_Final (p + 8, &md5);
    memset (&ivec, 0, sizeof(ivec));
    des_cbc_encrypt(p, 
		    p, 
		    24, 
		    key->schedule->data, 
		    &ivec, 
		    DES_ENCRYPT);
}

static krb5_error_code
RSA_MD5_DES_verify(krb5_context context,
		   struct key_data *key,
		   const void *data,
		   size_t len,
		   unsigned usage,
		   Checksum *C)
{
    MD5_CTX md5;
    unsigned char tmp[24];
    unsigned char res[16];
    des_cblock ivec;
    des_key_schedule *sched = key->schedule->data;
    krb5_error_code ret = 0;

    memset(&ivec, 0, sizeof(ivec));
    des_cbc_encrypt(C->checksum.data, 
		    (void*)tmp, 
		    C->checksum.length, 
		    sched[0],
		    &ivec,
		    DES_DECRYPT);
    MD5_Init (&md5);
    MD5_Update (&md5, tmp, 8); /* confounder */
    MD5_Update (&md5, data, len);
    MD5_Final (res, &md5);
    if(memcmp(res, tmp + 8, sizeof(res)) != 0) {
	krb5_clear_error_string (context);
	ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
    }
    memset(tmp, 0, sizeof(tmp));
    memset(res, 0, sizeof(res));
    return ret;
}

static void
RSA_MD5_DES3_checksum(krb5_context context,
		      struct key_data *key,
		      const void *data,
		      size_t len,
		      unsigned usage,
		      Checksum *C)
{
    MD5_CTX md5;
    des_cblock ivec;
    unsigned char *p = C->checksum.data;
    des_key_schedule *sched = key->schedule->data;
    
    krb5_generate_random_block(p, 8);
    MD5_Init (&md5);
    MD5_Update (&md5, p, 8);
    MD5_Update (&md5, data, len);
    MD5_Final (p + 8, &md5);
    memset (&ivec, 0, sizeof(ivec));
    des_ede3_cbc_encrypt(p, 
			 p, 
			 24, 
			 sched[0], sched[1], sched[2],
			 &ivec, 
			 DES_ENCRYPT);
}

static krb5_error_code
RSA_MD5_DES3_verify(krb5_context context,
		    struct key_data *key,
		    const void *data,
		    size_t len,
		    unsigned usage,
		    Checksum *C)
{
    MD5_CTX md5;
    unsigned char tmp[24];
    unsigned char res[16];
    des_cblock ivec;
    des_key_schedule *sched = key->schedule->data;
    krb5_error_code ret = 0;

    memset(&ivec, 0, sizeof(ivec));
    des_ede3_cbc_encrypt(C->checksum.data, 
			 (void*)tmp, 
			 C->checksum.length, 
			 sched[0], sched[1], sched[2],
			 &ivec,
			 DES_DECRYPT);
    MD5_Init (&md5);
    MD5_Update (&md5, tmp, 8); /* confounder */
    MD5_Update (&md5, data, len);
    MD5_Final (res, &md5);
    if(memcmp(res, tmp + 8, sizeof(res)) != 0) {
	krb5_clear_error_string (context);
	ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
    }
    memset(tmp, 0, sizeof(tmp));
    memset(res, 0, sizeof(res));
    return ret;
}

static void
SHA1_checksum(krb5_context context,
	      struct key_data *key,
	      const void *data,
	      size_t len,
	      unsigned usage,
	      Checksum *C)
{
    SHA_CTX m;

    SHA1_Init(&m);
    SHA1_Update(&m, data, len);
    SHA1_Final(C->checksum.data, &m);
}

/* HMAC according to RFC2104 */
static krb5_error_code
hmac(krb5_context context,
     struct checksum_type *cm, 
     const void *data, 
     size_t len, 
     unsigned usage,
     struct key_data *keyblock,
     Checksum *result)
{
    unsigned char *ipad, *opad;
    unsigned char *key;
    size_t key_len;
    int i;
    
    ipad = malloc(cm->blocksize + len);
    if (ipad == NULL)
	return ENOMEM;
    opad = malloc(cm->blocksize + cm->checksumsize);
    if (opad == NULL) {
	free(ipad);
	return ENOMEM;
    }
    memset(ipad, 0x36, cm->blocksize);
    memset(opad, 0x5c, cm->blocksize);

    if(keyblock->key->keyvalue.length > cm->blocksize){
	(*cm->checksum)(context, 
			keyblock, 
			keyblock->key->keyvalue.data, 
			keyblock->key->keyvalue.length, 
			usage,
			result);
	key = result->checksum.data;
	key_len = result->checksum.length;
    } else {
	key = keyblock->key->keyvalue.data;
	key_len = keyblock->key->keyvalue.length;
    }
    for(i = 0; i < key_len; i++){
	ipad[i] ^= key[i];
	opad[i] ^= key[i];
    }
    memcpy(ipad + cm->blocksize, data, len);
    (*cm->checksum)(context, keyblock, ipad, cm->blocksize + len,
		    usage, result);
    memcpy(opad + cm->blocksize, result->checksum.data, 
	   result->checksum.length);
    (*cm->checksum)(context, keyblock, opad, 
		    cm->blocksize + cm->checksumsize, usage, result);
    memset(ipad, 0, cm->blocksize + len);
    free(ipad);
    memset(opad, 0, cm->blocksize + cm->checksumsize);
    free(opad);

    return 0;
}

krb5_error_code
krb5_hmac(krb5_context context,
	  krb5_cksumtype cktype,
	  const void *data,
	  size_t len,
	  unsigned usage, 
	  krb5_keyblock *key,
	  Checksum *result)
{
    struct checksum_type *c = _find_checksum(cktype);
    struct key_data kd;
    krb5_error_code ret;

    if (c == NULL) {
	krb5_set_error_string (context, "checksum type %d not supported",
			       cktype);
	return KRB5_PROG_SUMTYPE_NOSUPP;
    }

    kd.key = key;
    kd.schedule = NULL;

    ret = hmac(context, c, data, len, usage, &kd, result);

    if (kd.schedule)
	krb5_free_data(context, kd.schedule);

    return ret;
 }

static void
SP_HMAC_SHA1_checksum(krb5_context context,
		      struct key_data *key, 
		      const void *data, 
		      size_t len, 
		      unsigned usage,
		      Checksum *result)
{
    struct checksum_type *c = _find_checksum(CKSUMTYPE_SHA1);
    Checksum res;
    char sha1_data[20];
    krb5_error_code ret;

    res.checksum.data = sha1_data;
    res.checksum.length = sizeof(sha1_data);

    ret = hmac(context, c, data, len, usage, key, &res);
    if (ret)
	krb5_abortx(context, "hmac failed");
    memcpy(result->checksum.data, res.checksum.data, result->checksum.length);
}

/*
 * checksum according to section 5. of draft-brezak-win2k-krb-rc4-hmac-03.txt
 */

static void
HMAC_MD5_checksum(krb5_context context,
		  struct key_data *key,
		  const void *data,
		  size_t len,
		  unsigned usage,
		  Checksum *result)
{
    MD5_CTX md5;
    struct checksum_type *c = _find_checksum (CKSUMTYPE_RSA_MD5);
    const char signature[] = "signaturekey";
    Checksum ksign_c;
    struct key_data ksign;
    krb5_keyblock kb;
    unsigned char t[4];
    unsigned char tmp[16];
    unsigned char ksign_c_data[16];
    krb5_error_code ret;

    ksign_c.checksum.length = sizeof(ksign_c_data);
    ksign_c.checksum.data   = ksign_c_data;
    ret = hmac(context, c, signature, sizeof(signature), 0, key, &ksign_c);
    if (ret)
	krb5_abortx(context, "hmac failed");
    ksign.key = &kb;
    kb.keyvalue = ksign_c.checksum;
    MD5_Init (&md5);
    t[0] = (usage >>  0) & 0xFF;
    t[1] = (usage >>  8) & 0xFF;
    t[2] = (usage >> 16) & 0xFF;
    t[3] = (usage >> 24) & 0xFF;
    MD5_Update (&md5, t, 4);
    MD5_Update (&md5, data, len);
    MD5_Final (tmp, &md5);
    ret = hmac(context, c, tmp, sizeof(tmp), 0, &ksign, result);
    if (ret)
	krb5_abortx(context, "hmac failed");
}

/*
 * same as previous but being used while encrypting.
 */

static void
HMAC_MD5_checksum_enc(krb5_context context,
		      struct key_data *key,
		      const void *data,
		      size_t len,
		      unsigned usage,
		      Checksum *result)
{
    struct checksum_type *c = _find_checksum (CKSUMTYPE_RSA_MD5);
    Checksum ksign_c;
    struct key_data ksign;
    krb5_keyblock kb;
    unsigned char t[4];
    unsigned char ksign_c_data[16];
    krb5_error_code ret;

    t[0] = (usage >>  0) & 0xFF;
    t[1] = (usage >>  8) & 0xFF;
    t[2] = (usage >> 16) & 0xFF;
    t[3] = (usage >> 24) & 0xFF;

    ksign_c.checksum.length = sizeof(ksign_c_data);
    ksign_c.checksum.data   = ksign_c_data;
    ret = hmac(context, c, t, sizeof(t), 0, key, &ksign_c);
    if (ret)
	krb5_abortx(context, "hmac failed");
    ksign.key = &kb;
    kb.keyvalue = ksign_c.checksum;
    ret = hmac(context, c, data, len, 0, &ksign, result);
    if (ret)
	krb5_abortx(context, "hmac failed");
}

struct checksum_type checksum_none = {
    CKSUMTYPE_NONE, 
    "none", 
    1, 
    0, 
    0, 
    NONE_checksum, 
    NULL
};
struct checksum_type checksum_crc32 = {
    CKSUMTYPE_CRC32,
    "crc32",
    1,
    4,
    0,
    CRC32_checksum,
    NULL
};
struct checksum_type checksum_rsa_md4 = {
    CKSUMTYPE_RSA_MD4,
    "rsa-md4",
    64,
    16,
    F_CPROOF,
    RSA_MD4_checksum,
    NULL
};
struct checksum_type checksum_rsa_md4_des = {
    CKSUMTYPE_RSA_MD4_DES,
    "rsa-md4-des",
    64,
    24,
    F_KEYED | F_CPROOF | F_VARIANT,
    RSA_MD4_DES_checksum,
    RSA_MD4_DES_verify
};
#if 0
struct checksum_type checksum_des_mac = { 
    CKSUMTYPE_DES_MAC,
    "des-mac",
    0,
    0,
    0,
    DES_MAC_checksum
};
struct checksum_type checksum_des_mac_k = {
    CKSUMTYPE_DES_MAC_K,
    "des-mac-k",
    0,
    0,
    0,
    DES_MAC_K_checksum
};
struct checksum_type checksum_rsa_md4_des_k = {
    CKSUMTYPE_RSA_MD4_DES_K, 
    "rsa-md4-des-k", 
    0, 
    0, 
    0, 
    RSA_MD4_DES_K_checksum,
    RSA_MD4_DES_K_verify
};
#endif
struct checksum_type checksum_rsa_md5 = {
    CKSUMTYPE_RSA_MD5,
    "rsa-md5",
    64,
    16,
    F_CPROOF,
    RSA_MD5_checksum,
    NULL
};
struct checksum_type checksum_rsa_md5_des = {
    CKSUMTYPE_RSA_MD5_DES,
    "rsa-md5-des",
    64,
    24,
    F_KEYED | F_CPROOF | F_VARIANT,
    RSA_MD5_DES_checksum,
    RSA_MD5_DES_verify
};
struct checksum_type checksum_rsa_md5_des3 = {
    CKSUMTYPE_RSA_MD5_DES3,
    "rsa-md5-des3",
    64,
    24,
    F_KEYED | F_CPROOF | F_VARIANT,
    RSA_MD5_DES3_checksum,
    RSA_MD5_DES3_verify
};
struct checksum_type checksum_sha1 = {
    CKSUMTYPE_SHA1,
    "sha1",
    64,
    20,
    F_CPROOF,
    SHA1_checksum,
    NULL
};
struct checksum_type checksum_hmac_sha1_des3 = {
    CKSUMTYPE_HMAC_SHA1_DES3,
    "hmac-sha1-des3",
    64,
    20,
    F_KEYED | F_CPROOF | F_DERIVED,
    SP_HMAC_SHA1_checksum,
    NULL
};

#ifdef ENABLE_AES
struct checksum_type checksum_hmac_sha1_aes128 = {
    CKSUMTYPE_HMAC_SHA1_96_AES_128,
    "hmac-sha1-96-aes128",
    64,
    12,
    F_KEYED | F_CPROOF | F_DERIVED,
    SP_HMAC_SHA1_checksum,
    NULL
};

struct checksum_type checksum_hmac_sha1_aes256 = {
    CKSUMTYPE_HMAC_SHA1_96_AES_256,
    "hmac-sha1-96-aes256",
    64,
    12,
    F_KEYED | F_CPROOF | F_DERIVED,
    SP_HMAC_SHA1_checksum,
    NULL
};
#endif /* ENABLE_AES */

struct checksum_type checksum_hmac_md5 = {
    CKSUMTYPE_HMAC_MD5,
    "hmac-md5",
    64,
    16,
    F_KEYED | F_CPROOF,
    HMAC_MD5_checksum,
    NULL
};

struct checksum_type checksum_hmac_md5_enc = {
    CKSUMTYPE_HMAC_MD5_ENC,
    "hmac-md5-enc",
    64,
    16,
    F_KEYED | F_CPROOF | F_PSEUDO,
    HMAC_MD5_checksum_enc,
    NULL
};

struct checksum_type *checksum_types[] = {
    &checksum_none,
    &checksum_crc32,
    &checksum_rsa_md4,
    &checksum_rsa_md4_des,
#if 0
    &checksum_des_mac, 
    &checksum_des_mac_k,
    &checksum_rsa_md4_des_k,
#endif
    &checksum_rsa_md5,
    &checksum_rsa_md5_des,
    &checksum_rsa_md5_des3,
    &checksum_sha1,
    &checksum_hmac_sha1_des3,
#ifdef ENABLE_AES
    &checksum_hmac_sha1_aes128,
    &checksum_hmac_sha1_aes256,
#endif
    &checksum_hmac_md5,
    &checksum_hmac_md5_enc
};

static int num_checksums = sizeof(checksum_types) / sizeof(checksum_types[0]);

static struct checksum_type *
_find_checksum(krb5_cksumtype type)
{
    int i;
    for(i = 0; i < num_checksums; i++)
	if(checksum_types[i]->type == type)
	    return checksum_types[i];
    return NULL;
}

static krb5_error_code
get_checksum_key(krb5_context context, 
		 krb5_crypto crypto,
		 unsigned usage,  /* not krb5_key_usage */
		 struct checksum_type *ct, 
		 struct key_data **key)
{
    krb5_error_code ret = 0;

    if(ct->flags & F_DERIVED)
	ret = _get_derived_key(context, crypto, usage, key);
    else if(ct->flags & F_VARIANT) {
	int i;

	*key = _new_derived_key(crypto, 0xff/* KRB5_KU_RFC1510_VARIANT */);
	if(*key == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    return ENOMEM;
	}
	ret = krb5_copy_keyblock(context, crypto->key.key, &(*key)->key);
	if(ret) 
	    return ret;
	for(i = 0; i < (*key)->key->keyvalue.length; i++)
	    ((unsigned char*)(*key)->key->keyvalue.data)[i] ^= 0xF0;
    } else {
	*key = &crypto->key; 
    }
    if(ret == 0)
	ret = _key_schedule(context, *key);
    return ret;
}

static krb5_error_code
create_checksum (krb5_context context,
		 struct checksum_type *ct,
		 krb5_crypto crypto,
		 unsigned usage,
		 void *data,
		 size_t len,
		 Checksum *result)
{
    krb5_error_code ret;
    struct key_data *dkey;
    int keyed_checksum;
    
    keyed_checksum = (ct->flags & F_KEYED) != 0;
    if(keyed_checksum && crypto == NULL) {
	krb5_clear_error_string (context);
	return KRB5_PROG_SUMTYPE_NOSUPP; /* XXX */
    }
    if(keyed_checksum) {
	ret = get_checksum_key(context, crypto, usage, ct, &dkey);
	if (ret)
	    return ret;
    } else
	dkey = NULL;
    result->cksumtype = ct->type;
    krb5_data_alloc(&result->checksum, ct->checksumsize);
    (*ct->checksum)(context, dkey, data, len, usage, result);
    return 0;
}

static int
arcfour_checksum_p(struct checksum_type *ct, krb5_crypto crypto)
{
    return (ct->type == CKSUMTYPE_HMAC_MD5) &&
	(crypto->key.key->keytype == KEYTYPE_ARCFOUR);
}

krb5_error_code
krb5_create_checksum(krb5_context context,
		     krb5_crypto crypto,
		     krb5_key_usage usage,
		     int type,
		     void *data,
		     size_t len,
		     Checksum *result)
{
    struct checksum_type *ct = NULL;
    unsigned keyusage;

    /* type 0 -> pick from crypto */
    if (type) {
	ct = _find_checksum(type);
    } else if (crypto) {
	ct = crypto->et->keyed_checksum;
	if (ct == NULL)
	    ct = crypto->et->checksum;
    }

    if(ct == NULL) {
	krb5_set_error_string (context, "checksum type %d not supported",
			       type);
	return KRB5_PROG_SUMTYPE_NOSUPP;
    }

    if (arcfour_checksum_p(ct, crypto)) {
	keyusage = usage;
	usage2arcfour(context, &keyusage);
    } else
	keyusage = CHECKSUM_USAGE(usage);

    return create_checksum(context, ct, crypto, keyusage,
			   data, len, result);
}

static krb5_error_code
verify_checksum(krb5_context context,
		krb5_crypto crypto,
		unsigned usage, /* not krb5_key_usage */
		void *data,
		size_t len,
		Checksum *cksum)
{
    krb5_error_code ret;
    struct key_data *dkey;
    int keyed_checksum;
    Checksum c;
    struct checksum_type *ct;

    ct = _find_checksum(cksum->cksumtype);
    if (ct == NULL) {
	krb5_set_error_string (context, "checksum type %d not supported",
			       cksum->cksumtype);
	return KRB5_PROG_SUMTYPE_NOSUPP;
    }
    if(ct->checksumsize != cksum->checksum.length) {
	krb5_clear_error_string (context);
	return KRB5KRB_AP_ERR_BAD_INTEGRITY; /* XXX */
    }
    keyed_checksum = (ct->flags & F_KEYED) != 0;
    if(keyed_checksum && crypto == NULL) {
	krb5_clear_error_string (context);
	return KRB5_PROG_SUMTYPE_NOSUPP; /* XXX */
    }
    if(keyed_checksum)
	ret = get_checksum_key(context, crypto, usage, ct, &dkey);
    else
	dkey = NULL;
    if(ct->verify)
	return (*ct->verify)(context, dkey, data, len, usage, cksum);

    ret = krb5_data_alloc (&c.checksum, ct->checksumsize);
    if (ret)
	return ret;

    (*ct->checksum)(context, dkey, data, len, usage, &c);

    if(c.checksum.length != cksum->checksum.length || 
       memcmp(c.checksum.data, cksum->checksum.data, c.checksum.length)) {
	krb5_clear_error_string (context);
	ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
    } else {
	ret = 0;
    }
    krb5_data_free (&c.checksum);
    return ret;
}

krb5_error_code
krb5_verify_checksum(krb5_context context,
		     krb5_crypto crypto,
		     krb5_key_usage usage, 
		     void *data,
		     size_t len,
		     Checksum *cksum)
{
    struct checksum_type *ct;
    unsigned keyusage;

    ct = _find_checksum(cksum->cksumtype);
    if(ct == NULL) {
	krb5_set_error_string (context, "checksum type %d not supported",
			       cksum->cksumtype);
	return KRB5_PROG_SUMTYPE_NOSUPP;
    }

    if (arcfour_checksum_p(ct, crypto)) {
	keyusage = usage;
	usage2arcfour(context, &keyusage);
    } else
	keyusage = CHECKSUM_USAGE(usage);

    return verify_checksum(context, crypto, keyusage,
			   data, len, cksum);
}

krb5_error_code
krb5_checksumsize(krb5_context context,
		  krb5_cksumtype type,
		  size_t *size)
{
    struct checksum_type *ct = _find_checksum(type);
    if(ct == NULL) {
	krb5_set_error_string (context, "checksum type %d not supported",
			       type);
	return KRB5_PROG_SUMTYPE_NOSUPP;
    }
    *size = ct->checksumsize;
    return 0;
}

krb5_boolean
krb5_checksum_is_keyed(krb5_context context,
		       krb5_cksumtype type)
{
    struct checksum_type *ct = _find_checksum(type);
    if(ct == NULL) {
	krb5_set_error_string (context, "checksum type %d not supported",
			       type);
	return KRB5_PROG_SUMTYPE_NOSUPP;
    }
    return ct->flags & F_KEYED;
}

krb5_boolean
krb5_checksum_is_collision_proof(krb5_context context,
				 krb5_cksumtype type)
{
    struct checksum_type *ct = _find_checksum(type);
    if(ct == NULL) {
	krb5_set_error_string (context, "checksum type %d not supported",
			       type);
	return KRB5_PROG_SUMTYPE_NOSUPP;
    }
    return ct->flags & F_CPROOF;
}

/************************************************************
 *                                                          *
 ************************************************************/

static krb5_error_code
NULL_encrypt(krb5_context context,
	     struct key_data *key, 
	     void *data, 
	     size_t len, 
	     krb5_boolean encrypt,
	     int usage,
	     void *ivec)
{
    return 0;
}

static krb5_error_code
DES_CBC_encrypt_null_ivec(krb5_context context,
			  struct key_data *key, 
			  void *data, 
			  size_t len, 
			  krb5_boolean encrypt,
			  int usage,
			  void *ignore_ivec)
{
    des_cblock ivec;
    des_key_schedule *s = key->schedule->data;
    memset(&ivec, 0, sizeof(ivec));
    des_cbc_encrypt(data, data, len, *s, &ivec, encrypt);
    return 0;
}

static krb5_error_code
DES_CBC_encrypt_key_ivec(krb5_context context,
			 struct key_data *key, 
			 void *data, 
			 size_t len, 
			 krb5_boolean encrypt,
			 int usage,
			 void *ignore_ivec)
{
    des_cblock ivec;
    des_key_schedule *s = key->schedule->data;
    memcpy(&ivec, key->key->keyvalue.data, sizeof(ivec));
    des_cbc_encrypt(data, data, len, *s, &ivec, encrypt);
    return 0;
}

static krb5_error_code
DES3_CBC_encrypt(krb5_context context,
		 struct key_data *key, 
		 void *data, 
		 size_t len, 
		 krb5_boolean encrypt,
		 int usage,
		 void *ivec)
{
    des_cblock local_ivec;
    des_key_schedule *s = key->schedule->data;
    if(ivec == NULL) {
	ivec = &local_ivec;
	memset(local_ivec, 0, sizeof(local_ivec));
    }
    des_ede3_cbc_encrypt(data, data, len, s[0], s[1], s[2], ivec, encrypt);
    return 0;
}

static krb5_error_code
DES_CFB64_encrypt_null_ivec(krb5_context context,
			    struct key_data *key, 
			    void *data, 
			    size_t len, 
			    krb5_boolean encrypt,
			    int usage,
			    void *ignore_ivec)
{
    des_cblock ivec;
    int num = 0;
    des_key_schedule *s = key->schedule->data;
    memset(&ivec, 0, sizeof(ivec));

    des_cfb64_encrypt(data, data, len, *s, &ivec, &num, encrypt);
    return 0;
}

static krb5_error_code
DES_PCBC_encrypt_key_ivec(krb5_context context,
			  struct key_data *key, 
			  void *data, 
			  size_t len, 
			  krb5_boolean encrypt,
			  int usage,
			  void *ignore_ivec)
{
    des_cblock ivec;
    des_key_schedule *s = key->schedule->data;
    memcpy(&ivec, key->key->keyvalue.data, sizeof(ivec));

    des_pcbc_encrypt(data, data, len, *s, &ivec, encrypt);
    return 0;
}

#ifdef ENABLE_AES

/*
 * AES draft-raeburn-krb-rijndael-krb-02
 */

void
_krb5_aes_cts_encrypt(const unsigned char *in, unsigned char *out,
		      size_t len, const void *aes_key,
		      unsigned char *ivec, const int enc)
{
    unsigned char tmp[AES_BLOCK_SIZE];
    const AES_KEY *key = aes_key; /* XXX remove this when we always have AES */
    int i;

    /*
     * In the framework of kerberos, the length can never be shorter
     * then at least one blocksize.
     */

    if (enc == AES_ENCRYPT) {

	while(len > AES_BLOCK_SIZE) {
	    for (i = 0; i < AES_BLOCK_SIZE; i++)
		tmp[i] = in[i] ^ ivec[i];
	    AES_encrypt(tmp, out, key);
	    memcpy(ivec, out, AES_BLOCK_SIZE);
	    len -= AES_BLOCK_SIZE;
	    in += AES_BLOCK_SIZE;
	    out += AES_BLOCK_SIZE;
	}

	for (i = 0; i < len; i++)
	    tmp[i] = in[i] ^ ivec[i];
	for (; i < AES_BLOCK_SIZE; i++)
	    tmp[i] = 0 ^ ivec[i];

	AES_encrypt(tmp, out - AES_BLOCK_SIZE, key);

	memcpy(out, ivec, len);

    } else {
	char tmp2[AES_BLOCK_SIZE];
	char tmp3[AES_BLOCK_SIZE];

	while(len > AES_BLOCK_SIZE * 2) {
	    memcpy(tmp, in, AES_BLOCK_SIZE);
	    AES_decrypt(in, out, key);
	    for (i = 0; i < AES_BLOCK_SIZE; i++)
		out[i] ^= ivec[i];
	    memcpy(ivec, tmp, AES_BLOCK_SIZE);
	    len -= AES_BLOCK_SIZE;
	    in += AES_BLOCK_SIZE;
	    out += AES_BLOCK_SIZE;
	}

	len -= AES_BLOCK_SIZE;

	AES_decrypt(in, tmp2, key);

	memcpy(tmp3, in + AES_BLOCK_SIZE, len);
	memcpy(tmp3 + len, tmp2 + len, AES_BLOCK_SIZE - len); /* xor 0 */

	for (i = 0; i < len; i++)
	    out[i + AES_BLOCK_SIZE] = tmp2[i] ^ tmp3[i];

	AES_decrypt(tmp3, out, key);
	for (i = 0; i < AES_BLOCK_SIZE; i++)
	    out[i] ^= ivec[i];
    }
}

static krb5_error_code
AES_CTS_encrypt(krb5_context context,
		struct key_data *key,
		void *data,
		size_t len,
		krb5_boolean encrypt,
		int usage,
		void *ivec)
{
    AES_KEY *k = key->schedule->data;
    char local_ivec[AES_BLOCK_SIZE];

    if (encrypt)
	k = &k[0];
    else
	k = &k[1];
    
    if (len < AES_BLOCK_SIZE)
	krb5_abortx(context, "invalid use of AES_CTS_encrypt");
    if (len == AES_BLOCK_SIZE) {
	if (encrypt)
	    AES_encrypt(data, data, k);
	else
	    AES_decrypt(data, data, k);
    } else {
	if(ivec == NULL) {
	    memset(local_ivec, 0, sizeof(local_ivec));
	    ivec = local_ivec;
	}
	_krb5_aes_cts_encrypt(data, data, len, k, ivec, encrypt);
    }

    return 0;
}
#endif /* ENABLE_AES */

/*
 * section 6 of draft-brezak-win2k-krb-rc4-hmac-03
 *
 * warning: not for small children
 */

static krb5_error_code
ARCFOUR_subencrypt(krb5_context context,
		   struct key_data *key,
		   void *data,
		   size_t len,
		   int usage,
		   void *ivec)
{
    struct checksum_type *c = _find_checksum (CKSUMTYPE_RSA_MD5);
    Checksum k1_c, k2_c, k3_c, cksum;
    struct key_data ke;
    krb5_keyblock kb;
    unsigned char t[4];
    RC4_KEY rc4_key;
    unsigned char *cdata = data;
    unsigned char k1_c_data[16], k2_c_data[16], k3_c_data[16];
    krb5_error_code ret;

    t[0] = (usage >>  0) & 0xFF;
    t[1] = (usage >>  8) & 0xFF;
    t[2] = (usage >> 16) & 0xFF;
    t[3] = (usage >> 24) & 0xFF;

    k1_c.checksum.length = sizeof(k1_c_data);
    k1_c.checksum.data   = k1_c_data;

    ret = hmac(NULL, c, t, sizeof(t), 0, key, &k1_c);
    if (ret)
	krb5_abortx(context, "hmac failed");

    memcpy (k2_c_data, k1_c_data, sizeof(k1_c_data));

    k2_c.checksum.length = sizeof(k2_c_data);
    k2_c.checksum.data   = k2_c_data;

    ke.key = &kb;
    kb.keyvalue = k2_c.checksum;

    cksum.checksum.length = 16;
    cksum.checksum.data   = data;

    ret = hmac(NULL, c, cdata + 16, len - 16, 0, &ke, &cksum);
    if (ret)
	krb5_abortx(context, "hmac failed");

    ke.key = &kb;
    kb.keyvalue = k1_c.checksum;

    k3_c.checksum.length = sizeof(k3_c_data);
    k3_c.checksum.data   = k3_c_data;

    ret = hmac(NULL, c, data, 16, 0, &ke, &k3_c);
    if (ret)
	krb5_abortx(context, "hmac failed");

    RC4_set_key (&rc4_key, k3_c.checksum.length, k3_c.checksum.data);
    RC4 (&rc4_key, len - 16, cdata + 16, cdata + 16);
    memset (k1_c_data, 0, sizeof(k1_c_data));
    memset (k2_c_data, 0, sizeof(k2_c_data));
    memset (k3_c_data, 0, sizeof(k3_c_data));
    return 0;
}

static krb5_error_code
ARCFOUR_subdecrypt(krb5_context context,
		   struct key_data *key,
		   void *data,
		   size_t len,
		   int usage,
		   void *ivec)
{
    struct checksum_type *c = _find_checksum (CKSUMTYPE_RSA_MD5);
    Checksum k1_c, k2_c, k3_c, cksum;
    struct key_data ke;
    krb5_keyblock kb;
    unsigned char t[4];
    RC4_KEY rc4_key;
    unsigned char *cdata = data;
    unsigned char k1_c_data[16], k2_c_data[16], k3_c_data[16];
    unsigned char cksum_data[16];
    krb5_error_code ret;

    t[0] = (usage >>  0) & 0xFF;
    t[1] = (usage >>  8) & 0xFF;
    t[2] = (usage >> 16) & 0xFF;
    t[3] = (usage >> 24) & 0xFF;

    k1_c.checksum.length = sizeof(k1_c_data);
    k1_c.checksum.data   = k1_c_data;

    ret = hmac(NULL, c, t, sizeof(t), 0, key, &k1_c);
    if (ret)
	krb5_abortx(context, "hmac failed");

    memcpy (k2_c_data, k1_c_data, sizeof(k1_c_data));

    k2_c.checksum.length = sizeof(k2_c_data);
    k2_c.checksum.data   = k2_c_data;

    ke.key = &kb;
    kb.keyvalue = k1_c.checksum;

    k3_c.checksum.length = sizeof(k3_c_data);
    k3_c.checksum.data   = k3_c_data;

    ret = hmac(NULL, c, cdata, 16, 0, &ke, &k3_c);
    if (ret)
	krb5_abortx(context, "hmac failed");

    RC4_set_key (&rc4_key, k3_c.checksum.length, k3_c.checksum.data);
    RC4 (&rc4_key, len - 16, cdata + 16, cdata + 16);

    ke.key = &kb;
    kb.keyvalue = k2_c.checksum;

    cksum.checksum.length = 16;
    cksum.checksum.data   = cksum_data;

    ret = hmac(NULL, c, cdata + 16, len - 16, 0, &ke, &cksum);
    if (ret)
	krb5_abortx(context, "hmac failed");

    memset (k1_c_data, 0, sizeof(k1_c_data));
    memset (k2_c_data, 0, sizeof(k2_c_data));
    memset (k3_c_data, 0, sizeof(k3_c_data));

    if (memcmp (cksum.checksum.data, data, 16) != 0) {
	krb5_clear_error_string (context);
	return KRB5KRB_AP_ERR_BAD_INTEGRITY;
    } else {
	return 0;
    }
}

/*
 * convert the usage numbers used in
 * draft-ietf-cat-kerb-key-derivation-00.txt to the ones in
 * draft-brezak-win2k-krb-rc4-hmac-04.txt
 */

static krb5_error_code
usage2arcfour (krb5_context context, int *usage)
{
    switch (*usage) {
    case KRB5_KU_AS_REP_ENC_PART : /* 3 */
    case KRB5_KU_TGS_REP_ENC_PART_SUB_KEY : /* 9 */
	*usage = 8;
	return 0;
    case KRB5_KU_USAGE_SEAL :  /* 22 */
	*usage = 13;
	return 0;
    case KRB5_KU_USAGE_SIGN : /* 23 */
        *usage = 15;
        return 0;
    case KRB5_KU_USAGE_SEQ: /* 24 */
	*usage = 0;
	return 0;
    default :
	return 0;
    }
}

static krb5_error_code
ARCFOUR_encrypt(krb5_context context,
		struct key_data *key,
		void *data,
		size_t len,
		krb5_boolean encrypt,
		int usage,
		void *ivec)
{
    krb5_error_code ret;
    if((ret = usage2arcfour (context, &usage)) != 0)
	return ret;

    if (encrypt)
	return ARCFOUR_subencrypt (context, key, data, len, usage, ivec);
    else
	return ARCFOUR_subdecrypt (context, key, data, len, usage, ivec);
}


/*
 * these should currently be in reverse preference order.
 * (only relevant for !F_PSEUDO) */

static struct encryption_type enctype_null = {
    ETYPE_NULL,
    "null",
    1,
    1,
    0,
    &keytype_null,
    &checksum_none,
    NULL,
    0,
    NULL_encrypt,
};
static struct encryption_type enctype_des_cbc_crc = {
    ETYPE_DES_CBC_CRC,
    "des-cbc-crc",
    8,
    8,
    8,
    &keytype_des,
    &checksum_crc32,
    NULL,
    0,
    DES_CBC_encrypt_key_ivec,
};
static struct encryption_type enctype_des_cbc_md4 = {
    ETYPE_DES_CBC_MD4,
    "des-cbc-md4",
    8,
    8,
    8,
    &keytype_des,
    &checksum_rsa_md4,
    &checksum_rsa_md4_des,
    0,
    DES_CBC_encrypt_null_ivec,
};
static struct encryption_type enctype_des_cbc_md5 = {
    ETYPE_DES_CBC_MD5,
    "des-cbc-md5",
    8,
    8,
    8,
    &keytype_des,
    &checksum_rsa_md5,
    &checksum_rsa_md5_des,
    0,
    DES_CBC_encrypt_null_ivec,
};
static struct encryption_type enctype_arcfour_hmac_md5 = {
    ETYPE_ARCFOUR_HMAC_MD5,
    "arcfour-hmac-md5",
    1,
    1,
    8,
    &keytype_arcfour,
    &checksum_hmac_md5,
    /* &checksum_hmac_md5_enc */ NULL,
    F_SPECIAL,
    ARCFOUR_encrypt
};
static struct encryption_type enctype_des3_cbc_md5 = { 
    ETYPE_DES3_CBC_MD5,
    "des3-cbc-md5",
    8,
    8,
    8,
    &keytype_des3,
    &checksum_rsa_md5,
    &checksum_rsa_md5_des3,
    0,
    DES3_CBC_encrypt,
};
static struct encryption_type enctype_des3_cbc_sha1 = {
    ETYPE_DES3_CBC_SHA1,
    "des3-cbc-sha1",
    8,
    8,
    8,
    &keytype_des3_derived,
    &checksum_sha1,
    &checksum_hmac_sha1_des3,
    F_DERIVED,
    DES3_CBC_encrypt,
};
static struct encryption_type enctype_old_des3_cbc_sha1 = {
    ETYPE_OLD_DES3_CBC_SHA1,
    "old-des3-cbc-sha1",
    8,
    8,
    8,
    &keytype_des3,
    &checksum_sha1,
    &checksum_hmac_sha1_des3,
    0,
    DES3_CBC_encrypt,
};
#ifdef ENABLE_AES
static struct encryption_type enctype_aes128_cts_hmac_sha1 = {
    ETYPE_AES128_CTS_HMAC_SHA1_96,
    "aes128-cts-hmac-sha1-96",
    16,
    1,
    16,
    &keytype_aes128,
    &checksum_sha1,
    &checksum_hmac_sha1_aes128,
    0,
    AES_CTS_encrypt,
};
static struct encryption_type enctype_aes256_cts_hmac_sha1 = {
    ETYPE_AES256_CTS_HMAC_SHA1_96,
    "aes256-cts-hmac-sha1-96",
    16,
    1,
    16,
    &keytype_aes256,
    &checksum_sha1,
    &checksum_hmac_sha1_aes256,
    0,
    AES_CTS_encrypt,
};
#endif /* ENABLE_AES */
static struct encryption_type enctype_des_cbc_none = {
    ETYPE_DES_CBC_NONE,
    "des-cbc-none",
    8,
    8,
    0,
    &keytype_des,
    &checksum_none,
    NULL,
    F_PSEUDO,
    DES_CBC_encrypt_null_ivec,
};
static struct encryption_type enctype_des_cfb64_none = {
    ETYPE_DES_CFB64_NONE,
    "des-cfb64-none",
    1,
    1,
    0,
    &keytype_des,
    &checksum_none,
    NULL,
    F_PSEUDO,
    DES_CFB64_encrypt_null_ivec,
};
static struct encryption_type enctype_des_pcbc_none = {
    ETYPE_DES_PCBC_NONE,
    "des-pcbc-none",
    8,
    8,
    0,
    &keytype_des,
    &checksum_none,
    NULL,
    F_PSEUDO,
    DES_PCBC_encrypt_key_ivec,
};
static struct encryption_type enctype_des3_cbc_none = {
    ETYPE_DES3_CBC_NONE,
    "des3-cbc-none",
    8,
    8,
    0,
    &keytype_des3_derived,
    &checksum_none,
    NULL,
    F_PSEUDO,
    DES3_CBC_encrypt,
};

static struct encryption_type *etypes[] = {
    &enctype_null,
    &enctype_des_cbc_crc,
    &enctype_des_cbc_md4,
    &enctype_des_cbc_md5,
    &enctype_arcfour_hmac_md5,
    &enctype_des3_cbc_md5, 
    &enctype_des3_cbc_sha1,
    &enctype_old_des3_cbc_sha1,
#ifdef ENABLE_AES
    &enctype_aes128_cts_hmac_sha1,
    &enctype_aes256_cts_hmac_sha1,
#endif
    &enctype_des_cbc_none,
    &enctype_des_cfb64_none,
    &enctype_des_pcbc_none,
    &enctype_des3_cbc_none
};

static unsigned num_etypes = sizeof(etypes) / sizeof(etypes[0]);


static struct encryption_type *
_find_enctype(krb5_enctype type)
{
    int i;
    for(i = 0; i < num_etypes; i++)
	if(etypes[i]->type == type)
	    return etypes[i];
    return NULL;
}


krb5_error_code
krb5_enctype_to_string(krb5_context context,
		       krb5_enctype etype,
		       char **string)
{
    struct encryption_type *e;
    e = _find_enctype(etype);
    if(e == NULL) {
	krb5_set_error_string (context, "encryption type %d not supported",
			       etype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    *string = strdup(e->name);
    if(*string == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    return 0;
}

krb5_error_code
krb5_string_to_enctype(krb5_context context,
		       const char *string,
		       krb5_enctype *etype)
{
    int i;
    for(i = 0; i < num_etypes; i++)
	if(strcasecmp(etypes[i]->name, string) == 0){
	    *etype = etypes[i]->type;
	    return 0;
	}
    krb5_set_error_string (context, "encryption type %s not supported",
			   string);
    return KRB5_PROG_ETYPE_NOSUPP;
}

krb5_error_code
krb5_enctype_to_keytype(krb5_context context,
			krb5_enctype etype,
			krb5_keytype *keytype)
{
    struct encryption_type *e = _find_enctype(etype);
    if(e == NULL) {
	krb5_set_error_string (context, "encryption type %d not supported",
			       etype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    *keytype = e->keytype->type; /* XXX */
    return 0;
}

#if 0
krb5_error_code
krb5_keytype_to_enctype(krb5_context context,
			krb5_keytype keytype,
			krb5_enctype *etype)
{
    struct key_type *kt = _find_keytype(keytype);
    krb5_warnx(context, "krb5_keytype_to_enctype(%u)", keytype);
    if(kt == NULL)
	return KRB5_PROG_KEYTYPE_NOSUPP;
    *etype = kt->best_etype;
    return 0;
}
#endif
    
krb5_error_code
krb5_keytype_to_enctypes (krb5_context context,
			  krb5_keytype keytype,
			  unsigned *len,
			  krb5_enctype **val)
{
    int i;
    unsigned n = 0;
    krb5_enctype *ret;

    for (i = num_etypes - 1; i >= 0; --i) {
	if (etypes[i]->keytype->type == keytype
	    && !(etypes[i]->flags & F_PSEUDO))
	    ++n;
    }
    ret = malloc(n * sizeof(*ret));
    if (ret == NULL && n != 0) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    n = 0;
    for (i = num_etypes - 1; i >= 0; --i) {
	if (etypes[i]->keytype->type == keytype
	    && !(etypes[i]->flags & F_PSEUDO))
	    ret[n++] = etypes[i]->type;
    }
    *len = n;
    *val = ret;
    return 0;
}

/*
 * First take the configured list of etypes for `keytype' if available,
 * else, do `krb5_keytype_to_enctypes'.
 */

krb5_error_code
krb5_keytype_to_enctypes_default (krb5_context context,
				  krb5_keytype keytype,
				  unsigned *len,
				  krb5_enctype **val)
{
    int i, n;
    krb5_enctype *ret;

    if (keytype != KEYTYPE_DES || context->etypes_des == NULL)
	return krb5_keytype_to_enctypes (context, keytype, len, val);

    for (n = 0; context->etypes_des[n]; ++n)
	;
    ret = malloc (n * sizeof(*ret));
    if (ret == NULL && n != 0) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    for (i = 0; i < n; ++i)
	ret[i] = context->etypes_des[i];
    *len = n;
    *val = ret;
    return 0;
}

krb5_error_code
krb5_enctype_valid(krb5_context context, 
		 krb5_enctype etype)
{
    return _find_enctype(etype) != NULL;
}

/* if two enctypes have compatible keys */
krb5_boolean
krb5_enctypes_compatible_keys(krb5_context context,
			      krb5_enctype etype1,
			      krb5_enctype etype2)
{
    struct encryption_type *e1 = _find_enctype(etype1);
    struct encryption_type *e2 = _find_enctype(etype2);
    return e1 != NULL && e2 != NULL && e1->keytype == e2->keytype;
}

static krb5_boolean
derived_crypto(krb5_context context,
	       krb5_crypto crypto)
{
    return (crypto->et->flags & F_DERIVED) != 0;
}

static krb5_boolean
special_crypto(krb5_context context,
	       krb5_crypto crypto)
{
    return (crypto->et->flags & F_SPECIAL) != 0;
}

#define CHECKSUMSIZE(C) ((C)->checksumsize)
#define CHECKSUMTYPE(C) ((C)->type)

static krb5_error_code
encrypt_internal_derived(krb5_context context,
			 krb5_crypto crypto,
			 unsigned usage,
			 void *data,
			 size_t len,
			 krb5_data *result,
			 void *ivec)
{
    size_t sz, block_sz, checksum_sz, total_sz;
    Checksum cksum;
    unsigned char *p, *q;
    krb5_error_code ret;
    struct key_data *dkey;
    const struct encryption_type *et = crypto->et;
    
    checksum_sz = CHECKSUMSIZE(et->keyed_checksum);

    sz = et->confoundersize + len;
    block_sz = (sz + et->padsize - 1) &~ (et->padsize - 1); /* pad */
    total_sz = block_sz + checksum_sz;
    p = calloc(1, total_sz);
    if(p == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    
    q = p;
    krb5_generate_random_block(q, et->confoundersize); /* XXX */
    q += et->confoundersize;
    memcpy(q, data, len);
    
    ret = create_checksum(context, 
			  et->keyed_checksum,
			  crypto, 
			  INTEGRITY_USAGE(usage),
			  p, 
			  block_sz,
			  &cksum);
    if(ret == 0 && cksum.checksum.length != checksum_sz) {
	free_Checksum (&cksum);
	krb5_clear_error_string (context);
	ret = KRB5_CRYPTO_INTERNAL;
    }
    if(ret)
	goto fail;
    memcpy(p + block_sz, cksum.checksum.data, cksum.checksum.length);
    free_Checksum (&cksum);
    ret = _get_derived_key(context, crypto, ENCRYPTION_USAGE(usage), &dkey);
    if(ret)
	goto fail;
    ret = _key_schedule(context, dkey);
    if(ret)
	goto fail;
#ifdef CRYPTO_DEBUG
    krb5_crypto_debug(context, 1, block_sz, dkey->key);
#endif
    ret = (*et->encrypt)(context, dkey, p, block_sz, 1, usage, ivec);
    if (ret)
	goto fail;
    result->data = p;
    result->length = total_sz;
    return 0;
 fail:
    memset(p, 0, total_sz);
    free(p);
    return ret;
}


static krb5_error_code
encrypt_internal(krb5_context context,
		 krb5_crypto crypto,
		 void *data,
		 size_t len,
		 krb5_data *result,
		 void *ivec)
{
    size_t sz, block_sz, checksum_sz;
    Checksum cksum;
    unsigned char *p, *q;
    krb5_error_code ret;
    const struct encryption_type *et = crypto->et;
    
    checksum_sz = CHECKSUMSIZE(et->checksum);
    
    sz = et->confoundersize + checksum_sz + len;
    block_sz = (sz + et->padsize - 1) &~ (et->padsize - 1); /* pad */
    p = calloc(1, block_sz);
    if(p == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    
    q = p;
    krb5_generate_random_block(q, et->confoundersize); /* XXX */
    q += et->confoundersize;
    memset(q, 0, checksum_sz);
    q += checksum_sz;
    memcpy(q, data, len);

    ret = create_checksum(context, 
			  et->checksum,
			  crypto,
			  0,
			  p, 
			  block_sz,
			  &cksum);
    if(ret == 0 && cksum.checksum.length != checksum_sz) {
	krb5_clear_error_string (context);
	free_Checksum(&cksum);
	ret = KRB5_CRYPTO_INTERNAL;
    }
    if(ret)
	goto fail;
    memcpy(p + et->confoundersize, cksum.checksum.data, cksum.checksum.length);
    free_Checksum(&cksum);
    ret = _key_schedule(context, &crypto->key);
    if(ret)
	goto fail;
#ifdef CRYPTO_DEBUG
    krb5_crypto_debug(context, 1, block_sz, crypto->key.key);
#endif
    ret = (*et->encrypt)(context, &crypto->key, p, block_sz, 1, 0, ivec);
    if (ret) {
	memset(p, 0, block_sz);
	free(p);
	return ret;
    }
    result->data = p;
    result->length = block_sz;
    return 0;
 fail:
    memset(p, 0, block_sz);
    free(p);
    return ret;
}

static krb5_error_code
encrypt_internal_special(krb5_context context,
			 krb5_crypto crypto,
			 int usage,
			 void *data,
			 size_t len,
			 krb5_data *result,
			 void *ivec)
{
    struct encryption_type *et = crypto->et;
    size_t cksum_sz = CHECKSUMSIZE(et->checksum);
    size_t sz = len + cksum_sz + et->confoundersize;
    char *tmp, *p;
    krb5_error_code ret;

    tmp = malloc (sz);
    if (tmp == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    p = tmp;
    memset (p, 0, cksum_sz);
    p += cksum_sz;
    krb5_generate_random_block(p, et->confoundersize);
    p += et->confoundersize;
    memcpy (p, data, len);
    ret = (*et->encrypt)(context, &crypto->key, tmp, sz, TRUE, usage, ivec);
    if (ret) {
	memset(tmp, 0, sz);
	free(tmp);
	return ret;
    }
    result->data   = tmp;
    result->length = sz;
    return 0;
}

static krb5_error_code
decrypt_internal_derived(krb5_context context,
			 krb5_crypto crypto,
			 unsigned usage,
			 void *data,
			 size_t len,
			 krb5_data *result,
			 void *ivec)
{
    size_t checksum_sz;
    Checksum cksum;
    unsigned char *p;
    krb5_error_code ret;
    struct key_data *dkey;
    struct encryption_type *et = crypto->et;
    unsigned long l;
    
    checksum_sz = CHECKSUMSIZE(et->keyed_checksum);
    if (len < checksum_sz) {
	krb5_clear_error_string (context);
	return EINVAL;		/* XXX - better error code? */
    }

    if (((len - checksum_sz) % et->padsize) != 0) {
	krb5_clear_error_string(context);
	return KRB5_BAD_MSIZE;
    }

    p = malloc(len);
    if(len != 0 && p == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    memcpy(p, data, len);

    len -= checksum_sz;

    ret = _get_derived_key(context, crypto, ENCRYPTION_USAGE(usage), &dkey);
    if(ret) {
	free(p);
	return ret;
    }
    ret = _key_schedule(context, dkey);
    if(ret) {
	free(p);
	return ret;
    }
#ifdef CRYPTO_DEBUG
    krb5_crypto_debug(context, 0, len, dkey->key);
#endif
    ret = (*et->encrypt)(context, dkey, p, len, 0, usage, ivec);
    if (ret) {
	free(p);
	return ret;
    }

    cksum.checksum.data   = p + len;
    cksum.checksum.length = checksum_sz;
    cksum.cksumtype       = CHECKSUMTYPE(et->keyed_checksum);

    ret = verify_checksum(context,
			  crypto,
			  INTEGRITY_USAGE(usage),
			  p,
			  len,
			  &cksum);
    if(ret) {
	free(p);
	return ret;
    }
    l = len - et->confoundersize;
    memmove(p, p + et->confoundersize, l);
    result->data = realloc(p, l);
    if(result->data == NULL) {
	free(p);
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    result->length = l;
    return 0;
}

static krb5_error_code
decrypt_internal(krb5_context context,
		 krb5_crypto crypto,
		 void *data,
		 size_t len,
		 krb5_data *result,
		 void *ivec)
{
    krb5_error_code ret;
    unsigned char *p;
    Checksum cksum;
    size_t checksum_sz, l;
    struct encryption_type *et = crypto->et;
    
    if ((len % et->padsize) != 0) {
	krb5_clear_error_string(context);
	return KRB5_BAD_MSIZE;
    }

    checksum_sz = CHECKSUMSIZE(et->checksum);
    p = malloc(len);
    if(len != 0 && p == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    memcpy(p, data, len);
    
    ret = _key_schedule(context, &crypto->key);
    if(ret) {
	free(p);
	return ret;
    }
#ifdef CRYPTO_DEBUG
    krb5_crypto_debug(context, 0, len, crypto->key.key);
#endif
    ret = (*et->encrypt)(context, &crypto->key, p, len, 0, 0, ivec);
    if (ret) {
	free(p);
	return ret;
    }
    ret = krb5_data_copy(&cksum.checksum, p + et->confoundersize, checksum_sz);
    if(ret) {
 	free(p);
 	return ret;
    }
    memset(p + et->confoundersize, 0, checksum_sz);
    cksum.cksumtype = CHECKSUMTYPE(et->checksum);
    ret = verify_checksum(context, NULL, 0, p, len, &cksum);
    free_Checksum(&cksum);
    if(ret) {
	free(p);
	return ret;
    }
    l = len - et->confoundersize - checksum_sz;
    memmove(p, p + et->confoundersize + checksum_sz, l);
    result->data = realloc(p, l);
    if(result->data == NULL) {
	free(p);
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    result->length = l;
    return 0;
}

static krb5_error_code
decrypt_internal_special(krb5_context context,
			 krb5_crypto crypto,
			 int usage,
			 void *data,
			 size_t len,
			 krb5_data *result,
			 void *ivec)
{
    struct encryption_type *et = crypto->et;
    size_t cksum_sz = CHECKSUMSIZE(et->checksum);
    size_t sz = len - cksum_sz - et->confoundersize;
    unsigned char *p;
    krb5_error_code ret;

    if ((len % et->padsize) != 0) {
	krb5_clear_error_string(context);
	return KRB5_BAD_MSIZE;
    }

    p = malloc (len);
    if (p == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    memcpy(p, data, len);
    
    ret = (*et->encrypt)(context, &crypto->key, p, len, FALSE, usage, ivec);
    if (ret) {
	free(p);
	return ret;
    }

    memmove (p, p + cksum_sz + et->confoundersize, sz);
    result->data = realloc(p, sz);
    if(result->data == NULL) {
	free(p);
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    result->length = sz;
    return 0;
}


krb5_error_code
krb5_encrypt_ivec(krb5_context context,
		  krb5_crypto crypto,
		  unsigned usage,
		  void *data,
		  size_t len,
		  krb5_data *result,
		  void *ivec)
{
    if(derived_crypto(context, crypto))
	return encrypt_internal_derived(context, crypto, usage, 
					data, len, result, ivec);
    else if (special_crypto(context, crypto))
	return encrypt_internal_special (context, crypto, usage,
					 data, len, result, ivec);
    else
	return encrypt_internal(context, crypto, data, len, result, ivec);
}

krb5_error_code
krb5_encrypt(krb5_context context,
	     krb5_crypto crypto,
	     unsigned usage,
	     void *data,
	     size_t len,
	     krb5_data *result)
{
    return krb5_encrypt_ivec(context, crypto, usage, data, len, result, NULL);
}

krb5_error_code
krb5_encrypt_EncryptedData(krb5_context context,
			   krb5_crypto crypto,
			   unsigned usage,
			   void *data,
			   size_t len,
			   int kvno,
			   EncryptedData *result)
{
    result->etype = CRYPTO_ETYPE(crypto);
    if(kvno){
	ALLOC(result->kvno, 1);
	*result->kvno = kvno;
    }else
	result->kvno = NULL;
    return krb5_encrypt(context, crypto, usage, data, len, &result->cipher);
}

krb5_error_code
krb5_decrypt_ivec(krb5_context context,
		  krb5_crypto crypto,
		  unsigned usage,
		  void *data,
		  size_t len,
		  krb5_data *result,
		  void *ivec)
{
    if(derived_crypto(context, crypto))
	return decrypt_internal_derived(context, crypto, usage, 
					data, len, result, ivec);
    else if (special_crypto (context, crypto))
	return decrypt_internal_special(context, crypto, usage,
					data, len, result, ivec);
    else
	return decrypt_internal(context, crypto, data, len, result, ivec);
}

krb5_error_code
krb5_decrypt(krb5_context context,
	     krb5_crypto crypto,
	     unsigned usage,
	     void *data,
	     size_t len,
	     krb5_data *result)
{
    return krb5_decrypt_ivec (context, crypto, usage, data, len, result,
			      NULL);
}

krb5_error_code
krb5_decrypt_EncryptedData(krb5_context context,
			   krb5_crypto crypto,
			   unsigned usage,
			   const EncryptedData *e,
			   krb5_data *result)
{
    return krb5_decrypt(context, crypto, usage, 
			e->cipher.data, e->cipher.length, result);
}

/************************************************************
 *                                                          *
 ************************************************************/

#ifdef HAVE_OPENSSL
#include <openssl/rand.h>

/* From openssl/crypto/rand/rand_lcl.h */
#define ENTROPY_NEEDED 20
static int
seed_something(void)
{
    int fd = -1;
    char buf[1024], seedfile[256];

    /* If there is a seed file, load it. But such a file cannot be trusted,
       so use 0 for the entropy estimate */
    if (RAND_file_name(seedfile, sizeof(seedfile))) {
	fd = open(seedfile, O_RDONLY);
	if (fd >= 0) {
	    read(fd, buf, sizeof(buf));
	    /* Use the full buffer anyway */
	    RAND_add(buf, sizeof(buf), 0.0);
	} else
	    seedfile[0] = '\0';
    } else
	seedfile[0] = '\0';

    /* Calling RAND_status() will try to use /dev/urandom if it exists so
       we do not have to deal with it. */
    if (RAND_status() != 1) {
	krb5_context context;
	const char *p;

	/* Try using egd */
	if (!krb5_init_context(&context)) {
	    p = krb5_config_get_string(context, NULL, "libdefaults",
		"egd_socket", NULL);
	    if (p != NULL)
		RAND_egd_bytes(p, ENTROPY_NEEDED);
	    krb5_free_context(context);
	}
    }
    
    if (RAND_status() == 1)	{
	/* Update the seed file */
	if (seedfile[0])
	    RAND_write_file(seedfile);

	return 0;
    } else
	return -1;
}

void
krb5_generate_random_block(void *buf, size_t len)
{
    static int rng_initialized = 0;
    
    if (!rng_initialized) {
	if (seed_something())
	    krb5_abortx(NULL, "Fatal: could not seed the random number generator");
	
	rng_initialized = 1;
    }
    RAND_bytes(buf, len);
}

#else

void
krb5_generate_random_block(void *buf, size_t len)
{
    des_cblock key, out;
    static des_cblock counter;
    static des_key_schedule schedule;
    int i;
    static int initialized = 0;

    if(!initialized) {
	des_new_random_key(&key);
	des_set_key(&key, schedule);
	memset(&key, 0, sizeof(key));
	des_new_random_key(&counter);
    }
    while(len > 0) {
	des_ecb_encrypt(&counter, &out, schedule, DES_ENCRYPT);
	for(i = 7; i >=0; i--)
	    if(counter[i]++)
		break;
	memcpy(buf, out, min(len, sizeof(out)));
	len -= min(len, sizeof(out));
	buf = (char*)buf + sizeof(out);
    }
}
#endif

static void
DES3_postproc(krb5_context context,
	      unsigned char *k, size_t len, struct key_data *key)
{
    unsigned char x[24];
    int i, j;

    memset(x, 0, sizeof(x));
    for (i = 0; i < 3; ++i) {
	unsigned char foo;

	for (j = 0; j < 7; ++j) {
	    unsigned char b = k[7 * i + j];

	    x[8 * i + j] = b;
	}
	foo = 0;
	for (j = 6; j >= 0; --j) {
	    foo |= k[7 * i + j] & 1;
	    foo <<= 1;
	}
	x[8 * i + 7] = foo;
    }
    k = key->key->keyvalue.data;
    memcpy(k, x, 24);
    memset(x, 0, sizeof(x));
    if (key->schedule) {
	krb5_free_data(context, key->schedule);
	key->schedule = NULL;
    }
    des_set_odd_parity((des_cblock*)k);
    des_set_odd_parity((des_cblock*)(k + 8));
    des_set_odd_parity((des_cblock*)(k + 16));
}

static krb5_error_code
derive_key(krb5_context context,
	   struct encryption_type *et,
	   struct key_data *key,
	   const void *constant,
	   size_t len)
{
    unsigned char *k;
    unsigned int nblocks = 0, i;
    krb5_error_code ret = 0;
    
    struct key_type *kt = et->keytype;
    ret = _key_schedule(context, key);
    if(ret)
	return ret;
    if(et->blocksize * 8 < kt->bits || 
       len != et->blocksize) {
	nblocks = (kt->bits + et->blocksize * 8 - 1) / (et->blocksize * 8);
	k = malloc(nblocks * et->blocksize);
	if(k == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    return ENOMEM;
	}
	_krb5_n_fold(constant, len, k, et->blocksize);
	for(i = 0; i < nblocks; i++) {
	    if(i > 0)
		memcpy(k + i * et->blocksize, 
		       k + (i - 1) * et->blocksize,
		       et->blocksize);
	    (*et->encrypt)(context, key, k + i * et->blocksize, et->blocksize,
			   1, 0, NULL);
	}
    } else {
	/* this case is probably broken, but won't be run anyway */
	void *c = malloc(len);
	size_t res_len = (kt->bits + 7) / 8;

	if(len != 0 && c == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    return ENOMEM;
	}
	memcpy(c, constant, len);
	(*et->encrypt)(context, key, c, len, 1, 0, NULL);
	k = malloc(res_len);
	if(res_len != 0 && k == NULL) {
	    free(c);
	    krb5_set_error_string(context, "malloc: out of memory");
	    return ENOMEM;
	}
	_krb5_n_fold(c, len, k, res_len);
	free(c);
    }
    
    /* XXX keytype dependent post-processing */
    switch(kt->type) {
    case KEYTYPE_DES3:
	DES3_postproc(context, k, nblocks * et->blocksize, key);
	break;
#ifdef ENABLE_AES
    case KEYTYPE_AES128:
    case KEYTYPE_AES256:
	memcpy(key->key->keyvalue.data, k, key->key->keyvalue.length);
	break;
#endif /* ENABLE_AES */
    default:
	krb5_set_error_string(context,
			      "derive_key() called with unknown keytype (%u)", 
			      kt->type);
	ret = KRB5_CRYPTO_INTERNAL;
	break;
    }
    memset(k, 0, nblocks * et->blocksize);
    free(k);
    return ret;
}

static struct key_data *
_new_derived_key(krb5_crypto crypto, unsigned usage)
{
    struct key_usage *d = crypto->key_usage;
    d = realloc(d, (crypto->num_key_usage + 1) * sizeof(*d));
    if(d == NULL)
	return NULL;
    crypto->key_usage = d;
    d += crypto->num_key_usage++;
    memset(d, 0, sizeof(*d));
    d->usage = usage;
    return &d->key;
}

krb5_error_code
krb5_derive_key(krb5_context context,
		const krb5_keyblock *key,
		krb5_enctype etype,
		const void *constant,
		size_t constant_len,
		krb5_keyblock **derived_key)
{
    krb5_error_code ret;
    struct encryption_type *et;
    struct key_data d;

    et = _find_enctype (etype);
    if (et == NULL) {
	krb5_set_error_string(context, "encryption type %d not supported",
			      etype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }

    ret = krb5_copy_keyblock(context, key, derived_key);
    if (ret)
	return ret;

    d.key = *derived_key;
    d.schedule = NULL;
    ret = derive_key(context, et, &d, constant, constant_len);
    if (ret)
	return ret;
    ret = krb5_copy_keyblock(context, d.key, derived_key);
    return ret;
}

static krb5_error_code
_get_derived_key(krb5_context context, 
		 krb5_crypto crypto, 
		 unsigned usage, 
		 struct key_data **key)
{
    int i;
    struct key_data *d;
    unsigned char constant[5];

    for(i = 0; i < crypto->num_key_usage; i++)
	if(crypto->key_usage[i].usage == usage) {
	    *key = &crypto->key_usage[i].key;
	    return 0;
	}
    d = _new_derived_key(crypto, usage);
    if(d == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    krb5_copy_keyblock(context, crypto->key.key, &d->key);
    _krb5_put_int(constant, usage, 5);
    derive_key(context, crypto->et, d, constant, sizeof(constant));
    *key = d;
    return 0;
}


krb5_error_code
krb5_crypto_init(krb5_context context,
		 const krb5_keyblock *key,
		 krb5_enctype etype,
		 krb5_crypto *crypto)
{
    krb5_error_code ret;
    ALLOC(*crypto, 1);
    if(*crypto == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    if(etype == ETYPE_NULL)
	etype = key->keytype;
    (*crypto)->et = _find_enctype(etype);
    if((*crypto)->et == NULL) {
	free(*crypto);
	krb5_set_error_string (context, "encryption type %d not supported",
			       etype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    if((*crypto)->et->keytype->size != key->keyvalue.length) {
	free(*crypto);
	krb5_set_error_string (context, "encryption key has bad length");
	return KRB5_BAD_KEYSIZE;
    }
    ret = krb5_copy_keyblock(context, key, &(*crypto)->key.key);
    if(ret) {
	free(*crypto);
	return ret;
    }
    (*crypto)->key.schedule = NULL;
    (*crypto)->num_key_usage = 0;
    (*crypto)->key_usage = NULL;
    return 0;
}

static void
free_key_data(krb5_context context, struct key_data *key)
{
    krb5_free_keyblock(context, key->key);
    if(key->schedule) {
	memset(key->schedule->data, 0, key->schedule->length);
	krb5_free_data(context, key->schedule);
    }
}

static void
free_key_usage(krb5_context context, struct key_usage *ku)
{
    free_key_data(context, &ku->key);
}

krb5_error_code
krb5_crypto_destroy(krb5_context context,
		    krb5_crypto crypto)
{
    int i;
    
    for(i = 0; i < crypto->num_key_usage; i++)
	free_key_usage(context, &crypto->key_usage[i]);
    free(crypto->key_usage);
    free_key_data(context, &crypto->key);
    free (crypto);
    return 0;
}

krb5_error_code
krb5_crypto_getblocksize(krb5_context context,
			 krb5_crypto crypto,
			 size_t *blocksize)
{
    *blocksize = crypto->et->blocksize;
    return 0;
}

krb5_error_code
krb5_string_to_key_derived(krb5_context context,
			   const void *str,
			   size_t len,
			   krb5_enctype etype,
			   krb5_keyblock *key)
{
    struct encryption_type *et = _find_enctype(etype);
    krb5_error_code ret;
    struct key_data kd;
    size_t keylen = et->keytype->bits / 8;
    u_char *tmp;

    if(et == NULL) {
	krb5_set_error_string (context, "encryption type %d not supported",
			       etype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    ALLOC(kd.key, 1);
    if(kd.key == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }
    ret = krb5_data_alloc(&kd.key->keyvalue, et->keytype->size);
    if(ret) {
	free(kd.key);
	return ret;
    }
    kd.key->keytype = etype;
    tmp = malloc (keylen);
    if(tmp == NULL) {
	krb5_free_keyblock(context, kd.key);
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }
    _krb5_n_fold(str, len, tmp, keylen);
    kd.schedule = NULL;
    DES3_postproc (context, tmp, keylen, &kd); /* XXX */
    memset(tmp, 0, keylen);
    free(tmp);
    ret = derive_key(context, 
		     et,
		     &kd,
		     "kerberos", /* XXX well known constant */
		     strlen("kerberos"));
    ret = krb5_copy_keyblock_contents(context, kd.key, key);
    free_key_data(context, &kd);
    return ret;
}

static size_t
wrapped_length (krb5_context context,
		krb5_crypto  crypto,
		size_t       data_len)
{
    struct encryption_type *et = crypto->et;
    size_t padsize = et->padsize;
    size_t res;

    res =  et->confoundersize + et->checksum->checksumsize + data_len;
    res =  (res + padsize - 1) / padsize * padsize;
    return res;
}

static size_t
wrapped_length_dervied (krb5_context context,
			krb5_crypto  crypto,
			size_t       data_len)
{
    struct encryption_type *et = crypto->et;
    size_t padsize = et->padsize;
    size_t res;

    res =  et->confoundersize + data_len;
    res =  (res + padsize - 1) / padsize * padsize;
    res += et->checksum->checksumsize;
    return res;
}

/*
 * Return the size of an encrypted packet of length `data_len'
 */

size_t
krb5_get_wrapped_length (krb5_context context,
			 krb5_crypto  crypto,
			 size_t       data_len)
{
    if (derived_crypto (context, crypto))
	return wrapped_length_dervied (context, crypto, data_len);
    else
	return wrapped_length (context, crypto, data_len);
}

#ifdef CRYPTO_DEBUG

static krb5_error_code
krb5_get_keyid(krb5_context context,
	       krb5_keyblock *key,
	       u_int32_t *keyid)
{
    MD5_CTX md5;
    unsigned char tmp[16];

    MD5_Init (&md5);
    MD5_Update (&md5, key->keyvalue.data, key->keyvalue.length);
    MD5_Final (tmp, &md5);
    *keyid = (tmp[12] << 24) | (tmp[13] << 16) | (tmp[14] << 8) | tmp[15];
    return 0;
}

static void
krb5_crypto_debug(krb5_context context,
		  int encrypt,
		  size_t len,
		  krb5_keyblock *key)
{
    u_int32_t keyid;
    char *kt;
    krb5_get_keyid(context, key, &keyid);
    krb5_enctype_to_string(context, key->keytype, &kt);
    krb5_warnx(context, "%s %lu bytes with key-id %#x (%s)", 
	       encrypt ? "encrypting" : "decrypting",
	       (unsigned long)len,
	       keyid,
	       kt);
    free(kt);
}

#endif /* CRYPTO_DEBUG */

#if 0
int
main()
{
#if 0
    int i;
    krb5_context context;
    krb5_crypto crypto;
    struct key_data *d;
    krb5_keyblock key;
    char constant[4];
    unsigned usage = ENCRYPTION_USAGE(3);
    krb5_error_code ret;

    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    key.keytype = ETYPE_NEW_DES3_CBC_SHA1;
    key.keyvalue.data = "\xb3\x85\x58\x94\xd9\xdc\x7c\xc8"
	"\x25\xe9\x85\xab\x3e\xb5\xfb\x0e"
	"\xc8\xdf\xab\x26\x86\x64\x15\x25";
    key.keyvalue.length = 24;

    krb5_crypto_init(context, &key, 0, &crypto);

    d = _new_derived_key(crypto, usage);
    if(d == NULL)
	return ENOMEM;
    krb5_copy_keyblock(context, crypto->key.key, &d->key);
    _krb5_put_int(constant, usage, 4);
    derive_key(context, crypto->et, d, constant, sizeof(constant));
    return 0;
#else
    int i;
    krb5_context context;
    krb5_crypto crypto;
    struct key_data *d;
    krb5_keyblock key;
    krb5_error_code ret;
    Checksum res;

    char *data = "what do ya want for nothing?";

    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    key.keytype = ETYPE_NEW_DES3_CBC_SHA1;
    key.keyvalue.data = "Jefe";
    /* "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
       "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"; */
    key.keyvalue.length = 4;

    d = calloc(1, sizeof(*d));

    d->key = &key;
    res.checksum.length = 20;
    res.checksum.data = malloc(res.checksum.length);
    SP_HMAC_SHA1_checksum(context, d, data, 28, &res);

    return 0;
#endif
}
#endif
