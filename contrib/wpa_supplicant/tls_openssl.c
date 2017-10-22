/*
 * WPA Supplicant / SSL/TLS interface functions for openssl
 * Copyright (c) 2004-2006, Jouni Malinen <j@w1.fi>
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

#ifndef CONFIG_SMARTCARD
#ifndef OPENSSL_NO_ENGINE
#define OPENSSL_NO_ENGINE
#endif
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pkcs12.h>
#include <openssl/x509v3.h>
#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif /* OPENSSL_NO_ENGINE */

#include "common.h"
#include "tls.h"

#if OPENSSL_VERSION_NUMBER >= 0x0090800fL
#define OPENSSL_d2i_TYPE const unsigned char **
#else
#define OPENSSL_d2i_TYPE unsigned char **
#endif

static int tls_openssl_ref_count = 0;

struct tls_connection {
	SSL *ssl;
	BIO *ssl_in, *ssl_out;
#ifndef OPENSSL_NO_ENGINE
	ENGINE *engine;        /* functional reference to the engine */
	EVP_PKEY *private_key; /* the private key if using engine */
#endif /* OPENSSL_NO_ENGINE */
	char *subject_match, *altsubject_match;
	int read_alerts, write_alerts, failed;

	u8 *pre_shared_secret;
	size_t pre_shared_secret_len;
};


#ifdef CONFIG_NO_STDOUT_DEBUG

static void _tls_show_errors(void)
{
	unsigned long err;

	while ((err = ERR_get_error())) {
		/* Just ignore the errors, since stdout is disabled */
	}
}
#define tls_show_errors(l, f, t) _tls_show_errors()

#else /* CONFIG_NO_STDOUT_DEBUG */

static void tls_show_errors(int level, const char *func, const char *txt)
{
	unsigned long err;

	wpa_printf(level, "OpenSSL: %s - %s %s",
		   func, txt, ERR_error_string(ERR_get_error(), NULL));

	while ((err = ERR_get_error())) {
		wpa_printf(MSG_INFO, "OpenSSL: pending error: %s",
			   ERR_error_string(err, NULL));
	}
}

#endif /* CONFIG_NO_STDOUT_DEBUG */


#ifdef CONFIG_NATIVE_WINDOWS

/* Windows CryptoAPI and access to certificate stores */
#include <wincrypt.h>

#ifdef __MINGW32_VERSION
/*
 * MinGW does not yet include all the needed definitions for CryptoAPI, so
 * define here whatever extra is needed.
 */
#define CALG_SSL3_SHAMD5 (ALG_CLASS_HASH | ALG_TYPE_ANY | ALG_SID_SSL3SHAMD5)
#define CERT_SYSTEM_STORE_CURRENT_USER (1 << 16)
#define CERT_STORE_READONLY_FLAG 0x00008000
#define CERT_STORE_OPEN_EXISTING_FLAG 0x00004000
#define CRYPT_ACQUIRE_COMPARE_KEY_FLAG 0x00000004

static BOOL WINAPI
(*CryptAcquireCertificatePrivateKey)(PCCERT_CONTEXT pCert, DWORD dwFlags,
				     void *pvReserved, HCRYPTPROV *phCryptProv,
				     DWORD *pdwKeySpec, BOOL *pfCallerFreeProv)
= NULL; /* to be loaded from crypt32.dll */

static PCCERT_CONTEXT WINAPI
(*CertEnumCertificatesInStore)(HCERTSTORE hCertStore,
			       PCCERT_CONTEXT pPrevCertContext)
= NULL; /* to be loaded from crypt32.dll */

static int mingw_load_crypto_func(void)
{
	HINSTANCE dll;

	/* MinGW does not yet have full CryptoAPI support, so load the needed
	 * function here. */

	if (CryptAcquireCertificatePrivateKey)
		return 0;

	dll = LoadLibrary("crypt32");
	if (dll == NULL) {
		wpa_printf(MSG_DEBUG, "CryptoAPI: Could not load crypt32 "
			   "library");
		return -1;
	}

	CryptAcquireCertificatePrivateKey = GetProcAddress(
		dll, "CryptAcquireCertificatePrivateKey");
	if (CryptAcquireCertificatePrivateKey == NULL) {
		wpa_printf(MSG_DEBUG, "CryptoAPI: Could not get "
			   "CryptAcquireCertificatePrivateKey() address from "
			   "crypt32 library");
		return -1;
	}

	CertEnumCertificatesInStore = (void *) GetProcAddress(
		dll, "CertEnumCertificatesInStore");
	if (CertEnumCertificatesInStore == NULL) {
		wpa_printf(MSG_DEBUG, "CryptoAPI: Could not get "
			   "CertEnumCertificatesInStore() address from "
			   "crypt32 library");
		return -1;
	}

	return 0;
}

#else /* __MINGW32_VERSION */

static int mingw_load_crypto_func(void)
{
	return 0;
}

#endif /* __MINGW32_VERSION */


struct cryptoapi_rsa_data {
	const CERT_CONTEXT *cert;
	HCRYPTPROV crypt_prov;
	DWORD key_spec;
	BOOL free_crypt_prov;
};


static void cryptoapi_error(const char *msg)
{
	wpa_printf(MSG_INFO, "CryptoAPI: %s; err=%u",
		   msg, (unsigned int) GetLastError());
}


static int cryptoapi_rsa_pub_enc(int flen, const unsigned char *from,
				 unsigned char *to, RSA *rsa, int padding)
{
	wpa_printf(MSG_DEBUG, "%s - not implemented", __func__);
	return 0;
}


static int cryptoapi_rsa_pub_dec(int flen, const unsigned char *from,
				 unsigned char *to, RSA *rsa, int padding)
{
	wpa_printf(MSG_DEBUG, "%s - not implemented", __func__);
	return 0;
}


static int cryptoapi_rsa_priv_enc(int flen, const unsigned char *from,
				  unsigned char *to, RSA *rsa, int padding)
{
	struct cryptoapi_rsa_data *priv =
		(struct cryptoapi_rsa_data *) rsa->meth->app_data;
	HCRYPTHASH hash;
	DWORD hash_size, len, i;
	unsigned char *buf = NULL;
	int ret = 0;

	if (priv == NULL) {
		RSAerr(RSA_F_RSA_EAY_PRIVATE_ENCRYPT,
		       ERR_R_PASSED_NULL_PARAMETER);
		return 0;
	}

	if (padding != RSA_PKCS1_PADDING) {
		RSAerr(RSA_F_RSA_EAY_PRIVATE_ENCRYPT,
		       RSA_R_UNKNOWN_PADDING_TYPE);
		return 0;
	}

	if (flen != 16 /* MD5 */ + 20 /* SHA-1 */) {
		wpa_printf(MSG_INFO, "%s - only MD5-SHA1 hash supported",
			   __func__);
		RSAerr(RSA_F_RSA_EAY_PRIVATE_ENCRYPT,
		       RSA_R_INVALID_MESSAGE_LENGTH);
		return 0;
	}

	if (!CryptCreateHash(priv->crypt_prov, CALG_SSL3_SHAMD5, 0, 0, &hash))
	{
		cryptoapi_error("CryptCreateHash failed");
		return 0;
	}

	len = sizeof(hash_size);
	if (!CryptGetHashParam(hash, HP_HASHSIZE, (BYTE *) &hash_size, &len,
			       0)) {
		cryptoapi_error("CryptGetHashParam failed");
		goto err;
	}

	if ((int) hash_size != flen) {
		wpa_printf(MSG_INFO, "CryptoAPI: Invalid hash size (%u != %d)",
			   (unsigned) hash_size, flen);
		RSAerr(RSA_F_RSA_EAY_PRIVATE_ENCRYPT,
		       RSA_R_INVALID_MESSAGE_LENGTH);
		goto err;
	}
	if (!CryptSetHashParam(hash, HP_HASHVAL, (BYTE * ) from, 0)) {
		cryptoapi_error("CryptSetHashParam failed");
		goto err;
	}

	len = RSA_size(rsa);
	buf = os_malloc(len);
	if (buf == NULL) {
		RSAerr(RSA_F_RSA_EAY_PRIVATE_ENCRYPT, ERR_R_MALLOC_FAILURE);
		goto err;
	}

	if (!CryptSignHash(hash, priv->key_spec, NULL, 0, buf, &len)) {
		cryptoapi_error("CryptSignHash failed");
		goto err;
	}

	for (i = 0; i < len; i++)
		to[i] = buf[len - i - 1];
	ret = len;

err:
	os_free(buf);
	CryptDestroyHash(hash);

	return ret;
}


static int cryptoapi_rsa_priv_dec(int flen, const unsigned char *from,
				  unsigned char *to, RSA *rsa, int padding)
{
	wpa_printf(MSG_DEBUG, "%s - not implemented", __func__);
	return 0;
}


static void cryptoapi_free_data(struct cryptoapi_rsa_data *priv)
{
	if (priv == NULL)
		return;
	if (priv->crypt_prov && priv->free_crypt_prov)
		CryptReleaseContext(priv->crypt_prov, 0);
	if (priv->cert)
		CertFreeCertificateContext(priv->cert);
	os_free(priv);
}


static int cryptoapi_finish(RSA *rsa)
{
	cryptoapi_free_data((struct cryptoapi_rsa_data *) rsa->meth->app_data);
	os_free((void *) rsa->meth);
	rsa->meth = NULL;
	return 1;
}


static const CERT_CONTEXT * cryptoapi_find_cert(const char *name, DWORD store)
{
	HCERTSTORE cs;
	const CERT_CONTEXT *ret = NULL;

	cs = CertOpenStore((LPCSTR) CERT_STORE_PROV_SYSTEM, 0, 0,
			   store | CERT_STORE_OPEN_EXISTING_FLAG |
			   CERT_STORE_READONLY_FLAG, L"MY");
	if (cs == NULL) {
		cryptoapi_error("Failed to open 'My system store'");
		return NULL;
	}

	if (strncmp(name, "cert://", 7) == 0) {
		unsigned short wbuf[255];
		MultiByteToWideChar(CP_ACP, 0, name + 7, -1, wbuf, 255);
		ret = CertFindCertificateInStore(cs, X509_ASN_ENCODING |
						 PKCS_7_ASN_ENCODING,
						 0, CERT_FIND_SUBJECT_STR,
						 wbuf, NULL);
	} else if (strncmp(name, "hash://", 7) == 0) {
		CRYPT_HASH_BLOB blob;
		int len;
		const char *hash = name + 7;
		unsigned char *buf;

		len = os_strlen(hash) / 2;
		buf = os_malloc(len);
		if (buf && hexstr2bin(hash, buf, len) == 0) {
			blob.cbData = len;
			blob.pbData = buf;
			ret = CertFindCertificateInStore(cs,
							 X509_ASN_ENCODING |
							 PKCS_7_ASN_ENCODING,
							 0, CERT_FIND_HASH,
							 &blob, NULL);
		}
		os_free(buf);
	}

	CertCloseStore(cs, 0);

	return ret;
}


static int tls_cryptoapi_cert(SSL *ssl, const char *name)
{
	X509 *cert = NULL;
	RSA *rsa = NULL, *pub_rsa;
	struct cryptoapi_rsa_data *priv;
	RSA_METHOD *rsa_meth;

	if (name == NULL ||
	    (strncmp(name, "cert://", 7) != 0 &&
	     strncmp(name, "hash://", 7) != 0))
		return -1;

	priv = os_zalloc(sizeof(*priv));
	rsa_meth = os_zalloc(sizeof(*rsa_meth));
	if (priv == NULL || rsa_meth == NULL) {
		wpa_printf(MSG_WARNING, "CryptoAPI: Failed to allocate memory "
			   "for CryptoAPI RSA method");
		os_free(priv);
		os_free(rsa_meth);
		return -1;
	}

	priv->cert = cryptoapi_find_cert(name, CERT_SYSTEM_STORE_CURRENT_USER);
	if (priv->cert == NULL) {
		priv->cert = cryptoapi_find_cert(
			name, CERT_SYSTEM_STORE_LOCAL_MACHINE);
	}
	if (priv->cert == NULL) {
		wpa_printf(MSG_INFO, "CryptoAPI: Could not find certificate "
			   "'%s'", name);
		goto err;
	}

	cert = d2i_X509(NULL, (OPENSSL_d2i_TYPE) &priv->cert->pbCertEncoded,
			priv->cert->cbCertEncoded);
	if (cert == NULL) {
		wpa_printf(MSG_INFO, "CryptoAPI: Could not process X509 DER "
			   "encoding");
		goto err;
	}

	if (mingw_load_crypto_func())
		goto err;

	if (!CryptAcquireCertificatePrivateKey(priv->cert,
					       CRYPT_ACQUIRE_COMPARE_KEY_FLAG,
					       NULL, &priv->crypt_prov,
					       &priv->key_spec,
					       &priv->free_crypt_prov)) {
		cryptoapi_error("Failed to acquire a private key for the "
				"certificate");
		goto err;
	}

	rsa_meth->name = "Microsoft CryptoAPI RSA Method";
	rsa_meth->rsa_pub_enc = cryptoapi_rsa_pub_enc;
	rsa_meth->rsa_pub_dec = cryptoapi_rsa_pub_dec;
	rsa_meth->rsa_priv_enc = cryptoapi_rsa_priv_enc;
	rsa_meth->rsa_priv_dec = cryptoapi_rsa_priv_dec;
	rsa_meth->finish = cryptoapi_finish;
	rsa_meth->flags = RSA_METHOD_FLAG_NO_CHECK;
	rsa_meth->app_data = (char *) priv;

	rsa = RSA_new();
	if (rsa == NULL) {
		SSLerr(SSL_F_SSL_CTX_USE_CERTIFICATE_FILE,
		       ERR_R_MALLOC_FAILURE);
		goto err;
	}

	if (!SSL_use_certificate(ssl, cert)) {
		RSA_free(rsa);
		rsa = NULL;
		goto err;
	}
	pub_rsa = cert->cert_info->key->pkey->pkey.rsa;
	X509_free(cert);
	cert = NULL;

	rsa->n = BN_dup(pub_rsa->n);
	rsa->e = BN_dup(pub_rsa->e);
	if (!RSA_set_method(rsa, rsa_meth))
		goto err;

	if (!SSL_use_RSAPrivateKey(ssl, rsa))
		goto err;
	RSA_free(rsa);

	return 0;

err:
	if (cert)
		X509_free(cert);
	if (rsa)
		RSA_free(rsa);
	else {
		os_free(rsa_meth);
		cryptoapi_free_data(priv);
	}
	return -1;
}


static int tls_cryptoapi_ca_cert(SSL_CTX *ssl_ctx, SSL *ssl, const char *name)
{
	HCERTSTORE cs;
	PCCERT_CONTEXT ctx = NULL;
	X509 *cert;
	char buf[128];
	const char *store;
#ifdef UNICODE
	WCHAR *wstore;
#endif /* UNICODE */

	if (mingw_load_crypto_func())
		return -1;

	if (name == NULL || strncmp(name, "cert_store://", 13) != 0)
		return -1;

	store = name + 13;
#ifdef UNICODE
	wstore = os_malloc((os_strlen(store) + 1) * sizeof(WCHAR));
	if (wstore == NULL)
		return -1;
	wsprintf(wstore, L"%S", store);
	cs = CertOpenSystemStore(0, wstore);
	os_free(wstore);
#else /* UNICODE */
	cs = CertOpenSystemStore(0, store);
#endif /* UNICODE */
	if (cs == NULL) {
		wpa_printf(MSG_DEBUG, "%s: failed to open system cert store "
			   "'%s': error=%d", __func__, store,
			   (int) GetLastError());
		return -1;
	}

	while ((ctx = CertEnumCertificatesInStore(cs, ctx))) {
		cert = d2i_X509(NULL, (OPENSSL_d2i_TYPE) &ctx->pbCertEncoded,
				ctx->cbCertEncoded);
		if (cert == NULL) {
			wpa_printf(MSG_INFO, "CryptoAPI: Could not process "
				   "X509 DER encoding for CA cert");
			continue;
		}

		X509_NAME_oneline(X509_get_subject_name(cert), buf,
				  sizeof(buf));
		wpa_printf(MSG_DEBUG, "OpenSSL: Loaded CA certificate for "
			   "system certificate store: subject='%s'", buf);

		if (!X509_STORE_add_cert(ssl_ctx->cert_store, cert)) {
			tls_show_errors(MSG_WARNING, __func__,
					"Failed to add ca_cert to OpenSSL "
					"certificate store");
		}

		X509_free(cert);
	}

	if (!CertCloseStore(cs, 0)) {
		wpa_printf(MSG_DEBUG, "%s: failed to close system cert store "
			   "'%s': error=%d", __func__, name + 13,
			   (int) GetLastError());
	}

	return 0;
}


#else /* CONFIG_NATIVE_WINDOWS */

static int tls_cryptoapi_cert(SSL *ssl, const char *name)
{
	return -1;
}

#endif /* CONFIG_NATIVE_WINDOWS */


static void ssl_info_cb(const SSL *ssl, int where, int ret)
{
	const char *str;
	int w;

	wpa_printf(MSG_DEBUG, "SSL: (where=0x%x ret=0x%x)", where, ret);
	w = where & ~SSL_ST_MASK;
	if (w & SSL_ST_CONNECT)
		str = "SSL_connect";
	else if (w & SSL_ST_ACCEPT)
		str = "SSL_accept";
	else
		str = "undefined";

	if (where & SSL_CB_LOOP) {
		wpa_printf(MSG_DEBUG, "SSL: %s:%s",
			   str, SSL_state_string_long(ssl));
	} else if (where & SSL_CB_ALERT) {
		wpa_printf(MSG_INFO, "SSL: SSL3 alert: %s:%s:%s",
			   where & SSL_CB_READ ?
			   "read (remote end reported an error)" :
			   "write (local SSL3 detected an error)",
			   SSL_alert_type_string_long(ret),
			   SSL_alert_desc_string_long(ret));
		if ((ret >> 8) == SSL3_AL_FATAL) {
			struct tls_connection *conn =
				SSL_get_app_data((SSL *) ssl);
			if (where & SSL_CB_READ)
				conn->read_alerts++;
			else
				conn->write_alerts++;
		}
	} else if (where & SSL_CB_EXIT && ret <= 0) {
		wpa_printf(MSG_DEBUG, "SSL: %s:%s in %s",
			   str, ret == 0 ? "failed" : "error",
			   SSL_state_string_long(ssl));
	}
}


#ifndef OPENSSL_NO_ENGINE
/**
 * tls_engine_load_dynamic_generic - load any openssl engine
 * @pre: an array of commands and values that load an engine initialized
 *       in the engine specific function
 * @post: an array of commands and values that initialize an already loaded
 *        engine (or %NULL if not required)
 * @id: the engine id of the engine to load (only required if post is not %NULL
 *
 * This function is a generic function that loads any openssl engine.
 *
 * Returns: 0 on success, -1 on failure
 */
static int tls_engine_load_dynamic_generic(const char *pre[],
					   const char *post[], const char *id)
{
	ENGINE *engine;
	const char *dynamic_id = "dynamic";

	engine = ENGINE_by_id(id);
	if (engine) {
		ENGINE_free(engine);
		wpa_printf(MSG_DEBUG, "ENGINE: engine '%s' is already "
			   "available", id);
		return 0;
	}
	ERR_clear_error();

	engine = ENGINE_by_id(dynamic_id);
	if (engine == NULL) {
		wpa_printf(MSG_INFO, "ENGINE: Can't find engine %s [%s]",
			   dynamic_id,
			   ERR_error_string(ERR_get_error(), NULL));
		return -1;
	}

	/* Perform the pre commands. This will load the engine. */
	while (pre && pre[0]) {
		wpa_printf(MSG_DEBUG, "ENGINE: '%s' '%s'", pre[0], pre[1]);
		if (ENGINE_ctrl_cmd_string(engine, pre[0], pre[1], 0) == 0) {
			wpa_printf(MSG_INFO, "ENGINE: ctrl cmd_string failed: "
				   "%s %s [%s]", pre[0], pre[1],
				   ERR_error_string(ERR_get_error(), NULL));
			ENGINE_free(engine);
			return -1;
		}
		pre += 2;
	}

	/*
	 * Free the reference to the "dynamic" engine. The loaded engine can
	 * now be looked up using ENGINE_by_id().
	 */
	ENGINE_free(engine);

	engine = ENGINE_by_id(id);
	if (engine == NULL) {
		wpa_printf(MSG_INFO, "ENGINE: Can't find engine %s [%s]",
			   id, ERR_error_string(ERR_get_error(), NULL));
		return -1;
	}

	while (post && post[0]) {
		wpa_printf(MSG_DEBUG, "ENGINE: '%s' '%s'", post[0], post[1]);
		if (ENGINE_ctrl_cmd_string(engine, post[0], post[1], 0) == 0) {
			wpa_printf(MSG_DEBUG, "ENGINE: ctrl cmd_string failed:"
				" %s %s [%s]", post[0], post[1],
				   ERR_error_string(ERR_get_error(), NULL));
			ENGINE_remove(engine);
			ENGINE_free(engine);
			return -1;
		}
		post += 2;
	}
	ENGINE_free(engine);

	return 0;
}


/**
 * tls_engine_load_dynamic_pkcs11 - load the pkcs11 engine provided by opensc
 * @pkcs11_so_path: pksc11_so_path from the configuration
 * @pcks11_module_path: pkcs11_module_path from the configuration
 */
static int tls_engine_load_dynamic_pkcs11(const char *pkcs11_so_path,
					  const char *pkcs11_module_path)
{
	char *engine_id = "pkcs11";
	const char *pre_cmd[] = {
		"SO_PATH", NULL /* pkcs11_so_path */,
		"ID", NULL /* engine_id */,
		"LIST_ADD", "1",
		/* "NO_VCHECK", "1", */
		"LOAD", NULL,
		NULL, NULL
	};
	const char *post_cmd[] = {
		"MODULE_PATH", NULL /* pkcs11_module_path */,
		NULL, NULL
	};

	if (!pkcs11_so_path || !pkcs11_module_path)
		return 0;

	pre_cmd[1] = pkcs11_so_path;
	pre_cmd[3] = engine_id;
	post_cmd[1] = pkcs11_module_path;

	wpa_printf(MSG_DEBUG, "ENGINE: Loading pkcs11 Engine from %s",
		   pkcs11_so_path);

	return tls_engine_load_dynamic_generic(pre_cmd, post_cmd, engine_id);
}


/**
 * tls_engine_load_dynamic_opensc - load the opensc engine provided by opensc
 * @opensc_so_path: opensc_so_path from the configuration
 */
static int tls_engine_load_dynamic_opensc(const char *opensc_so_path)
{
	char *engine_id = "opensc";
	const char *pre_cmd[] = {
		"SO_PATH", NULL /* opensc_so_path */,
		"ID", NULL /* engine_id */,
		"LIST_ADD", "1",
		"LOAD", NULL,
		NULL, NULL
	};

	if (!opensc_so_path)
		return 0;

	pre_cmd[1] = opensc_so_path;
	pre_cmd[3] = engine_id;

	wpa_printf(MSG_DEBUG, "ENGINE: Loading OpenSC Engine from %s",
		   opensc_so_path);

	return tls_engine_load_dynamic_generic(pre_cmd, NULL, engine_id);
}
#endif /* OPENSSL_NO_ENGINE */


void * tls_init(const struct tls_config *conf)
{
	SSL_CTX *ssl;

	if (tls_openssl_ref_count == 0) {
		SSL_load_error_strings();
		SSL_library_init();
		/* TODO: if /dev/urandom is available, PRNG is seeded
		 * automatically. If this is not the case, random data should
		 * be added here. */

#ifdef PKCS12_FUNCS
		PKCS12_PBE_add();
#endif  /* PKCS12_FUNCS */
	}
	tls_openssl_ref_count++;

	ssl = SSL_CTX_new(TLSv1_method());
	if (ssl == NULL)
		return NULL;

	SSL_CTX_set_info_callback(ssl, ssl_info_cb);

#ifndef OPENSSL_NO_ENGINE
	if (conf &&
	    (conf->opensc_engine_path || conf->pkcs11_engine_path ||
	     conf->pkcs11_module_path)) {
		wpa_printf(MSG_DEBUG, "ENGINE: Loading dynamic engine");
		ERR_load_ENGINE_strings();
		ENGINE_load_dynamic();

		if (tls_engine_load_dynamic_opensc(conf->opensc_engine_path) ||
		    tls_engine_load_dynamic_pkcs11(conf->pkcs11_engine_path,
						   conf->pkcs11_module_path)) {
			tls_deinit(ssl);
			return NULL;
		}
	}
#endif /* OPENSSL_NO_ENGINE */

	return ssl;
}


void tls_deinit(void *ssl_ctx)
{
	SSL_CTX *ssl = ssl_ctx;
	SSL_CTX_free(ssl);

	tls_openssl_ref_count--;
	if (tls_openssl_ref_count == 0) {
#ifndef OPENSSL_NO_ENGINE
		ENGINE_cleanup();
#endif /* OPENSSL_NO_ENGINE */
		ERR_free_strings();
		EVP_cleanup();
	}
}


static int tls_engine_init(struct tls_connection *conn, const char *engine_id,
			   const char *pin, const char *key_id)
{
#ifndef OPENSSL_NO_ENGINE
	int ret = -1;
	if (engine_id == NULL) {
		wpa_printf(MSG_ERROR, "ENGINE: Engine ID not set");
		return -1;
	}
	if (pin == NULL) {
		wpa_printf(MSG_ERROR, "ENGINE: Smartcard PIN not set");
		return -1;
	}
	if (key_id == NULL) {
		wpa_printf(MSG_ERROR, "ENGINE: Key Id not set");
		return -1;
	}

	ERR_clear_error();
	conn->engine = ENGINE_by_id(engine_id);
	if (!conn->engine) {
		wpa_printf(MSG_ERROR, "ENGINE: engine %s not available [%s]",
			   engine_id, ERR_error_string(ERR_get_error(), NULL));
		goto err;
	}
	if (ENGINE_init(conn->engine) != 1) {
		wpa_printf(MSG_ERROR, "ENGINE: engine init failed "
			   "(engine: %s) [%s]", engine_id,
			   ERR_error_string(ERR_get_error(), NULL));
		goto err;
	}
	wpa_printf(MSG_DEBUG, "ENGINE: engine initialized");

	if (ENGINE_ctrl_cmd_string(conn->engine, "PIN", pin, 0) == 0) {
		wpa_printf(MSG_ERROR, "ENGINE: cannot set pin [%s]",
			   ERR_error_string(ERR_get_error(), NULL));
		goto err;
	}
	conn->private_key = ENGINE_load_private_key(conn->engine,
						    key_id, NULL, NULL);
	if (!conn->private_key) {
		wpa_printf(MSG_ERROR, "ENGINE: cannot load private key with id"
				" '%s' [%s]", key_id,
			   ERR_error_string(ERR_get_error(), NULL));
		ret = TLS_SET_PARAMS_ENGINE_PRV_INIT_FAILED;
		goto err;
	}
	return 0;

err:
	if (conn->engine) {
		ENGINE_free(conn->engine);
		conn->engine = NULL;
	}

	if (conn->private_key) {
		EVP_PKEY_free(conn->private_key);
		conn->private_key = NULL;
	}

	return ret;
#else /* OPENSSL_NO_ENGINE */
	return 0;
#endif /* OPENSSL_NO_ENGINE */
}


static void tls_engine_deinit(struct tls_connection *conn)
{
#ifndef OPENSSL_NO_ENGINE
	wpa_printf(MSG_DEBUG, "ENGINE: engine deinit");
	if (conn->private_key) {
		EVP_PKEY_free(conn->private_key);
		conn->private_key = NULL;
	}
	if (conn->engine) {
		ENGINE_finish(conn->engine);
		conn->engine = NULL;
	}
#endif /* OPENSSL_NO_ENGINE */
}


int tls_get_errors(void *ssl_ctx)
{
	int count = 0;
	unsigned long err;

	while ((err = ERR_get_error())) {
		wpa_printf(MSG_INFO, "TLS - SSL error: %s",
			   ERR_error_string(err, NULL));
		count++;
	}

	return count;
}

struct tls_connection * tls_connection_init(void *ssl_ctx)
{
	SSL_CTX *ssl = ssl_ctx;
	struct tls_connection *conn;

	conn = os_zalloc(sizeof(*conn));
	if (conn == NULL)
		return NULL;
	conn->ssl = SSL_new(ssl);
	if (conn->ssl == NULL) {
		tls_show_errors(MSG_INFO, __func__,
				"Failed to initialize new SSL connection");
		os_free(conn);
		return NULL;
	}

	SSL_set_app_data(conn->ssl, conn);
	SSL_set_options(conn->ssl,
			SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
			SSL_OP_SINGLE_DH_USE);

	conn->ssl_in = BIO_new(BIO_s_mem());
	if (!conn->ssl_in) {
		tls_show_errors(MSG_INFO, __func__,
				"Failed to create a new BIO for ssl_in");
		SSL_free(conn->ssl);
		os_free(conn);
		return NULL;
	}

	conn->ssl_out = BIO_new(BIO_s_mem());
	if (!conn->ssl_out) {
		tls_show_errors(MSG_INFO, __func__,
				"Failed to create a new BIO for ssl_out");
		SSL_free(conn->ssl);
		BIO_free(conn->ssl_in);
		os_free(conn);
		return NULL;
	}

	SSL_set_bio(conn->ssl, conn->ssl_in, conn->ssl_out);

	return conn;
}


void tls_connection_deinit(void *ssl_ctx, struct tls_connection *conn)
{
	if (conn == NULL)
		return;
	os_free(conn->pre_shared_secret);
	SSL_free(conn->ssl);
	tls_engine_deinit(conn);
	os_free(conn->subject_match);
	os_free(conn->altsubject_match);
	os_free(conn);
}


int tls_connection_established(void *ssl_ctx, struct tls_connection *conn)
{
	return conn ? SSL_is_init_finished(conn->ssl) : 0;
}


int tls_connection_shutdown(void *ssl_ctx, struct tls_connection *conn)
{
	if (conn == NULL)
		return -1;

	/* Shutdown previous TLS connection without notifying the peer
	 * because the connection was already terminated in practice
	 * and "close notify" shutdown alert would confuse AS. */
	SSL_set_quiet_shutdown(conn->ssl, 1);
	SSL_shutdown(conn->ssl);
	return 0;
}


static int tls_match_altsubject_component(X509 *cert, int type,
					  const char *value, size_t len)
{
	GENERAL_NAME *gen;
	void *ext;
	int i, found = 0;

	ext = X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);

	for (i = 0; ext && i < sk_GENERAL_NAME_num(ext); i++) {
		gen = sk_GENERAL_NAME_value(ext, i);
		if (gen->type != type)
			continue;
		if (os_strlen((char *) gen->d.ia5->data) == len &&
		    os_memcmp(value, gen->d.ia5->data, len) == 0)
			found++;
	}

	return found;
}


static int tls_match_altsubject(X509 *cert, const char *match)
{
	int type;
	const char *pos, *end;
	size_t len;

	pos = match;
	do {
		if (os_strncmp(pos, "EMAIL:", 6) == 0) {
			type = GEN_EMAIL;
			pos += 6;
		} else if (os_strncmp(pos, "DNS:", 4) == 0) {
			type = GEN_DNS;
			pos += 4;
		} else if (os_strncmp(pos, "URI:", 4) == 0) {
			type = GEN_URI;
			pos += 4;
		} else {
			wpa_printf(MSG_INFO, "TLS: Invalid altSubjectName "
				   "match '%s'", pos);
			return 0;
		}
		end = os_strchr(pos, ';');
		while (end) {
			if (os_strncmp(end + 1, "EMAIL:", 6) == 0 ||
			    os_strncmp(end + 1, "DNS:", 4) == 0 ||
			    os_strncmp(end + 1, "URI:", 4) == 0)
				break;
			end = os_strchr(end + 1, ';');
		}
		if (end)
			len = end - pos;
		else
			len = os_strlen(pos);
		if (tls_match_altsubject_component(cert, type, pos, len) > 0)
			return 1;
		pos = end + 1;
	} while (end);

	return 0;
}


static int tls_verify_cb(int preverify_ok, X509_STORE_CTX *x509_ctx)
{
	char buf[256];
	X509 *err_cert;
	int err, depth;
	SSL *ssl;
	struct tls_connection *conn;
	char *match, *altmatch;

	err_cert = X509_STORE_CTX_get_current_cert(x509_ctx);
	err = X509_STORE_CTX_get_error(x509_ctx);
	depth = X509_STORE_CTX_get_error_depth(x509_ctx);
	ssl = X509_STORE_CTX_get_ex_data(x509_ctx,
					 SSL_get_ex_data_X509_STORE_CTX_idx());
	X509_NAME_oneline(X509_get_subject_name(err_cert), buf, sizeof(buf));

	conn = SSL_get_app_data(ssl);
	match = conn ? conn->subject_match : NULL;
	altmatch = conn ? conn->altsubject_match : NULL;

	if (!preverify_ok) {
		wpa_printf(MSG_WARNING, "TLS: Certificate verification failed,"
			   " error %d (%s) depth %d for '%s'", err,
			   X509_verify_cert_error_string(err), depth, buf);
	} else {
		wpa_printf(MSG_DEBUG, "TLS: tls_verify_cb - "
			   "preverify_ok=%d err=%d (%s) depth=%d buf='%s'",
			   preverify_ok, err,
			   X509_verify_cert_error_string(err), depth, buf);
		if (depth == 0 && match && os_strstr(buf, match) == NULL) {
			wpa_printf(MSG_WARNING, "TLS: Subject '%s' did not "
				   "match with '%s'", buf, match);
			preverify_ok = 0;
		} else if (depth == 0 && altmatch &&
			   !tls_match_altsubject(err_cert, altmatch)) {
			wpa_printf(MSG_WARNING, "TLS: altSubjectName match "
				   "'%s' not found", altmatch);
			preverify_ok = 0;
		}
	}

	return preverify_ok;
}


#ifndef OPENSSL_NO_STDIO
static int tls_load_ca_der(void *_ssl_ctx, const char *ca_cert)
{
	SSL_CTX *ssl_ctx = _ssl_ctx;
	X509_LOOKUP *lookup;
	int ret = 0;

	lookup = X509_STORE_add_lookup(ssl_ctx->cert_store,
				       X509_LOOKUP_file());
	if (lookup == NULL) {
		tls_show_errors(MSG_WARNING, __func__,
				"Failed add lookup for X509 store");
		return -1;
	}

	if (!X509_LOOKUP_load_file(lookup, ca_cert, X509_FILETYPE_ASN1)) {
		unsigned long err = ERR_peek_error();
		tls_show_errors(MSG_WARNING, __func__,
				"Failed load CA in DER format");
		if (ERR_GET_LIB(err) == ERR_LIB_X509 &&
		    ERR_GET_REASON(err) == X509_R_CERT_ALREADY_IN_HASH_TABLE) {
			wpa_printf(MSG_DEBUG, "OpenSSL: %s - ignoring "
				   "cert already in hash table error",
				   __func__);
		} else
			ret = -1;
	}

	return ret;
}
#endif /* OPENSSL_NO_STDIO */


static int tls_connection_ca_cert(void *_ssl_ctx, struct tls_connection *conn,
				  const char *ca_cert, const u8 *ca_cert_blob,
				  size_t ca_cert_blob_len, const char *ca_path)
{
	SSL_CTX *ssl_ctx = _ssl_ctx;

	if (ca_cert_blob) {
		X509 *cert = d2i_X509(NULL, (OPENSSL_d2i_TYPE) &ca_cert_blob,
				      ca_cert_blob_len);
		if (cert == NULL) {
			tls_show_errors(MSG_WARNING, __func__,
					"Failed to parse ca_cert_blob");
			return -1;
		}

		if (!X509_STORE_add_cert(ssl_ctx->cert_store, cert)) {
			unsigned long err = ERR_peek_error();
			tls_show_errors(MSG_WARNING, __func__,
					"Failed to add ca_cert_blob to "
					"certificate store");
			if (ERR_GET_LIB(err) == ERR_LIB_X509 &&
			    ERR_GET_REASON(err) ==
			    X509_R_CERT_ALREADY_IN_HASH_TABLE) {
				wpa_printf(MSG_DEBUG, "OpenSSL: %s - ignoring "
					   "cert already in hash table error",
					   __func__);
			} else {
				X509_free(cert);
				return -1;
			}
		}
		X509_free(cert);
		wpa_printf(MSG_DEBUG, "OpenSSL: %s - added ca_cert_blob "
			   "to certificate store", __func__);
		SSL_set_verify(conn->ssl, SSL_VERIFY_PEER, tls_verify_cb);
		return 0;
	}

#ifdef CONFIG_NATIVE_WINDOWS
	if (ca_cert && tls_cryptoapi_ca_cert(ssl_ctx, conn->ssl, ca_cert) ==
	    0) {
		wpa_printf(MSG_DEBUG, "OpenSSL: Added CA certificates from "
			   "system certificate store");
		SSL_set_verify(conn->ssl, SSL_VERIFY_PEER, tls_verify_cb);
		return 0;
	}
#endif /* CONFIG_NATIVE_WINDOWS */

	if (ca_cert || ca_path) {
#ifndef OPENSSL_NO_STDIO
		if (SSL_CTX_load_verify_locations(ssl_ctx, ca_cert, ca_path) !=
		    1) {
			tls_show_errors(MSG_WARNING, __func__,
					"Failed to load root certificates");
			if (ca_cert &&
			    tls_load_ca_der(ssl_ctx, ca_cert) == 0) {
				wpa_printf(MSG_DEBUG, "OpenSSL: %s - loaded "
					   "DER format CA certificate",
					   __func__);
			} else
				return -1;
		} else {
			wpa_printf(MSG_DEBUG, "TLS: Trusted root "
				   "certificate(s) loaded");
			tls_get_errors(ssl_ctx);
		}
		SSL_set_verify(conn->ssl, SSL_VERIFY_PEER, tls_verify_cb);
#else /* OPENSSL_NO_STDIO */
		wpa_printf(MSG_DEBUG, "OpenSSL: %s - OPENSSL_NO_STDIO",
			   __func__);
		return -1;
#endif /* OPENSSL_NO_STDIO */
	} else {
		/* No ca_cert configured - do not try to verify server
		 * certificate */
		SSL_set_verify(conn->ssl, SSL_VERIFY_NONE, NULL);
	}

	return 0;
}


static int tls_global_ca_cert(SSL_CTX *ssl_ctx, const char *ca_cert)
{
	if (ca_cert) {
		if (SSL_CTX_load_verify_locations(ssl_ctx, ca_cert, NULL) != 1)
		{
			tls_show_errors(MSG_WARNING, __func__,
					"Failed to load root certificates");
			return -1;
		}

		wpa_printf(MSG_DEBUG, "TLS: Trusted root "
			   "certificate(s) loaded");

#ifndef OPENSSL_NO_STDIO
		/* Add the same CAs to the client certificate requests */
		SSL_CTX_set_client_CA_list(ssl_ctx,
					   SSL_load_client_CA_file(ca_cert));
#endif /* OPENSSL_NO_STDIO */
	}

	return 0;
}


int tls_global_set_verify(void *ssl_ctx, int check_crl)
{
	int flags;

	if (check_crl) {
		X509_STORE *cs = SSL_CTX_get_cert_store(ssl_ctx);
		if (cs == NULL) {
			tls_show_errors(MSG_INFO, __func__, "Failed to get "
					"certificate store when enabling "
					"check_crl");
			return -1;
		}
		flags = X509_V_FLAG_CRL_CHECK;
		if (check_crl == 2)
			flags |= X509_V_FLAG_CRL_CHECK_ALL;
		X509_STORE_set_flags(cs, flags);
	}
	return 0;
}


static int tls_connection_set_subject_match(struct tls_connection *conn,
					    const char *subject_match,
					    const char *altsubject_match)
{
	os_free(conn->subject_match);
	conn->subject_match = NULL;
	if (subject_match) {
		conn->subject_match = os_strdup(subject_match);
		if (conn->subject_match == NULL)
			return -1;
	}

	os_free(conn->altsubject_match);
	conn->altsubject_match = NULL;
	if (altsubject_match) {
		conn->altsubject_match = os_strdup(altsubject_match);
		if (conn->altsubject_match == NULL)
			return -1;
	}

	return 0;
}


int tls_connection_set_verify(void *ssl_ctx, struct tls_connection *conn,
			      int verify_peer)
{
	if (conn == NULL)
		return -1;

	if (verify_peer) {
		SSL_set_verify(conn->ssl, SSL_VERIFY_PEER |
			       SSL_VERIFY_FAIL_IF_NO_PEER_CERT |
			       SSL_VERIFY_CLIENT_ONCE, tls_verify_cb);
	} else {
		SSL_set_verify(conn->ssl, SSL_VERIFY_NONE, NULL);
	}

	SSL_set_accept_state(conn->ssl);

	return 0;
}


static int tls_connection_client_cert(struct tls_connection *conn,
				      const char *client_cert,
				      const u8 *client_cert_blob,
				      size_t client_cert_blob_len)
{
	if (client_cert == NULL && client_cert_blob == NULL)
		return 0;

	if (client_cert_blob &&
	    SSL_use_certificate_ASN1(conn->ssl, (u8 *) client_cert_blob,
				     client_cert_blob_len) == 1) {
		wpa_printf(MSG_DEBUG, "OpenSSL: SSL_use_certificate_ASN1 --> "
			   "OK");
		return 0;
	} else if (client_cert_blob) {
		tls_show_errors(MSG_DEBUG, __func__,
				"SSL_use_certificate_ASN1 failed");
	}

	if (client_cert == NULL)
		return -1;

#ifndef OPENSSL_NO_STDIO
	if (SSL_use_certificate_file(conn->ssl, client_cert,
				     SSL_FILETYPE_ASN1) == 1) {
		wpa_printf(MSG_DEBUG, "OpenSSL: SSL_use_certificate_file (DER)"
			   " --> OK");
		return 0;
	} else {
		tls_show_errors(MSG_DEBUG, __func__,
				"SSL_use_certificate_file (DER) failed");
	}

	if (SSL_use_certificate_file(conn->ssl, client_cert,
				     SSL_FILETYPE_PEM) == 1) {
		wpa_printf(MSG_DEBUG, "OpenSSL: SSL_use_certificate_file (PEM)"
			   " --> OK");
		return 0;
	} else {
		tls_show_errors(MSG_DEBUG, __func__,
				"SSL_use_certificate_file (PEM) failed");
	}
#else /* OPENSSL_NO_STDIO */
	wpa_printf(MSG_DEBUG, "OpenSSL: %s - OPENSSL_NO_STDIO", __func__);
#endif /* OPENSSL_NO_STDIO */

	return -1;
}


static int tls_global_client_cert(SSL_CTX *ssl_ctx, const char *client_cert)
{
#ifndef OPENSSL_NO_STDIO
	if (client_cert == NULL)
		return 0;

	if (SSL_CTX_use_certificate_file(ssl_ctx, client_cert,
					 SSL_FILETYPE_ASN1) != 1 &&
	    SSL_CTX_use_certificate_file(ssl_ctx, client_cert,
					 SSL_FILETYPE_PEM) != 1) {
		tls_show_errors(MSG_INFO, __func__,
				"Failed to load client certificate");
		return -1;
	}
	return 0;
#else /* OPENSSL_NO_STDIO */
	if (client_cert == NULL)
		return 0;
	wpa_printf(MSG_DEBUG, "OpenSSL: %s - OPENSSL_NO_STDIO", __func__);
	return -1;
#endif /* OPENSSL_NO_STDIO */
}


static int tls_passwd_cb(char *buf, int size, int rwflag, void *password)
{
	if (password == NULL) {
		return 0;
	}
	os_strncpy(buf, (char *) password, size);
	buf[size - 1] = '\0';
	return os_strlen(buf);
}


#ifdef PKCS12_FUNCS
static int tls_parse_pkcs12(SSL_CTX *ssl_ctx, SSL *ssl, PKCS12 *p12,
			    const char *passwd)
{
	EVP_PKEY *pkey;
	X509 *cert;
	STACK_OF(X509) *certs;
	int res = 0;
	char buf[256];

	pkey = NULL;
	cert = NULL;
	certs = NULL;
	if (!PKCS12_parse(p12, passwd, &pkey, &cert, &certs)) {
		tls_show_errors(MSG_DEBUG, __func__,
				"Failed to parse PKCS12 file");
		PKCS12_free(p12);
		return -1;
	}
	wpa_printf(MSG_DEBUG, "TLS: Successfully parsed PKCS12 data");

	if (cert) {
		X509_NAME_oneline(X509_get_subject_name(cert), buf,
				  sizeof(buf));
		wpa_printf(MSG_DEBUG, "TLS: Got certificate from PKCS12: "
			   "subject='%s'", buf);
		if (ssl) {
			if (SSL_use_certificate(ssl, cert) != 1)
				res = -1;
		} else {
			if (SSL_CTX_use_certificate(ssl_ctx, cert) != 1)
				res = -1;
		}
		X509_free(cert);
	}

	if (pkey) {
		wpa_printf(MSG_DEBUG, "TLS: Got private key from PKCS12");
		if (ssl) {
			if (SSL_use_PrivateKey(ssl, pkey) != 1)
				res = -1;
		} else {
			if (SSL_CTX_use_PrivateKey(ssl_ctx, pkey) != 1)
				res = -1;
		}
		EVP_PKEY_free(pkey);
	}

	if (certs) {
		while ((cert = sk_X509_pop(certs)) != NULL) {
			X509_NAME_oneline(X509_get_subject_name(cert), buf,
					  sizeof(buf));
			wpa_printf(MSG_DEBUG, "TLS: additional certificate"
				   " from PKCS12: subject='%s'", buf);
			/*
			 * There is no SSL equivalent for the chain cert - so
			 * always add it to the context...
			 */
			if (SSL_CTX_add_extra_chain_cert(ssl_ctx, cert) != 1) {
				res = -1;
				break;
			}
		}
		sk_X509_free(certs);
	}

	PKCS12_free(p12);

	if (res < 0)
		tls_get_errors(ssl_ctx);

	return res;
}
#endif  /* PKCS12_FUNCS */


static int tls_read_pkcs12(SSL_CTX *ssl_ctx, SSL *ssl, const char *private_key,
			   const char *passwd)
{
#ifdef PKCS12_FUNCS
	FILE *f;
	PKCS12 *p12;

	f = fopen(private_key, "rb");
	if (f == NULL)
		return -1;

	p12 = d2i_PKCS12_fp(f, NULL);
	fclose(f);

	if (p12 == NULL) {
		tls_show_errors(MSG_INFO, __func__,
				"Failed to use PKCS#12 file");
		return -1;
	}

	return tls_parse_pkcs12(ssl_ctx, ssl, p12, passwd);

#else /* PKCS12_FUNCS */
	wpa_printf(MSG_INFO, "TLS: PKCS12 support disabled - cannot read "
		   "p12/pfx files");
	return -1;
#endif  /* PKCS12_FUNCS */
}


static int tls_read_pkcs12_blob(SSL_CTX *ssl_ctx, SSL *ssl,
				const u8 *blob, size_t len, const char *passwd)
{
#ifdef PKCS12_FUNCS
	PKCS12 *p12;

	p12 = d2i_PKCS12(NULL, (OPENSSL_d2i_TYPE) &blob, len);
	if (p12 == NULL) {
		tls_show_errors(MSG_INFO, __func__,
				"Failed to use PKCS#12 blob");
		return -1;
	}

	return tls_parse_pkcs12(ssl_ctx, ssl, p12, passwd);

#else /* PKCS12_FUNCS */
	wpa_printf(MSG_INFO, "TLS: PKCS12 support disabled - cannot parse "
		   "p12/pfx blobs");
	return -1;
#endif  /* PKCS12_FUNCS */
}


static int tls_connection_engine_private_key(struct tls_connection *conn)
{
#ifndef OPENSSL_NO_ENGINE
	if (SSL_use_PrivateKey(conn->ssl, conn->private_key) != 1) {
		tls_show_errors(MSG_ERROR, __func__,
				"ENGINE: cannot use private key for TLS");
		return -1;
	}
	if (!SSL_check_private_key(conn->ssl)) {
		tls_show_errors(MSG_INFO, __func__,
				"Private key failed verification");
		return -1;
	}
	return 0;
#else /* OPENSSL_NO_ENGINE */
	wpa_printf(MSG_ERROR, "SSL: Configuration uses engine, but "
		   "engine support was not compiled in");
	return -1;
#endif /* OPENSSL_NO_ENGINE */
}


static int tls_connection_private_key(void *_ssl_ctx,
				      struct tls_connection *conn,
				      const char *private_key,
				      const char *private_key_passwd,
				      const u8 *private_key_blob,
				      size_t private_key_blob_len)
{
	SSL_CTX *ssl_ctx = _ssl_ctx;
	char *passwd;
	int ok;

	if (private_key == NULL && private_key_blob == NULL)
		return 0;

	if (private_key_passwd) {
		passwd = os_strdup(private_key_passwd);
		if (passwd == NULL)
			return -1;
	} else
		passwd = NULL;

	SSL_CTX_set_default_passwd_cb(ssl_ctx, tls_passwd_cb);
	SSL_CTX_set_default_passwd_cb_userdata(ssl_ctx, passwd);

	ok = 0;
	while (private_key_blob) {
		if (SSL_use_PrivateKey_ASN1(EVP_PKEY_RSA, conn->ssl,
					    (u8 *) private_key_blob,
					    private_key_blob_len) == 1) {
			wpa_printf(MSG_DEBUG, "OpenSSL: SSL_use_PrivateKey_"
				   "ASN1(EVP_PKEY_RSA) --> OK");
			ok = 1;
			break;
		} else {
			tls_show_errors(MSG_DEBUG, __func__,
					"SSL_use_PrivateKey_ASN1(EVP_PKEY_RSA)"
					" failed");
		}

		if (SSL_use_PrivateKey_ASN1(EVP_PKEY_DSA, conn->ssl,
					    (u8 *) private_key_blob,
					    private_key_blob_len) == 1) {
			wpa_printf(MSG_DEBUG, "OpenSSL: SSL_use_PrivateKey_"
				   "ASN1(EVP_PKEY_DSA) --> OK");
			ok = 1;
			break;
		} else {
			tls_show_errors(MSG_DEBUG, __func__,
					"SSL_use_PrivateKey_ASN1(EVP_PKEY_DSA)"
					" failed");
		}

		if (SSL_use_RSAPrivateKey_ASN1(conn->ssl,
					       (u8 *) private_key_blob,
					       private_key_blob_len) == 1) {
			wpa_printf(MSG_DEBUG, "OpenSSL: "
				   "SSL_use_RSAPrivateKey_ASN1 --> OK");
			ok = 1;
			break;
		} else {
			tls_show_errors(MSG_DEBUG, __func__,
					"SSL_use_RSAPrivateKey_ASN1 failed");
		}

		if (tls_read_pkcs12_blob(ssl_ctx, conn->ssl, private_key_blob,
					 private_key_blob_len, passwd) == 0) {
			wpa_printf(MSG_DEBUG, "OpenSSL: PKCS#12 as blob --> "
				   "OK");
			ok = 1;
			break;
		}

		break;
	}

	while (!ok && private_key) {
#ifndef OPENSSL_NO_STDIO
		if (SSL_use_PrivateKey_file(conn->ssl, private_key,
					    SSL_FILETYPE_ASN1) == 1) {
			wpa_printf(MSG_DEBUG, "OpenSSL: "
				   "SSL_use_PrivateKey_File (DER) --> OK");
			ok = 1;
			break;
		} else {
			tls_show_errors(MSG_DEBUG, __func__,
					"SSL_use_PrivateKey_File (DER) "
					"failed");
		}

		if (SSL_use_PrivateKey_file(conn->ssl, private_key,
					    SSL_FILETYPE_PEM) == 1) {
			wpa_printf(MSG_DEBUG, "OpenSSL: "
				   "SSL_use_PrivateKey_File (PEM) --> OK");
			ok = 1;
			break;
		} else {
			tls_show_errors(MSG_DEBUG, __func__,
					"SSL_use_PrivateKey_File (PEM) "
					"failed");
		}
#else /* OPENSSL_NO_STDIO */
		wpa_printf(MSG_DEBUG, "OpenSSL: %s - OPENSSL_NO_STDIO",
			   __func__);
#endif /* OPENSSL_NO_STDIO */

		if (tls_read_pkcs12(ssl_ctx, conn->ssl, private_key, passwd)
		    == 0) {
			wpa_printf(MSG_DEBUG, "OpenSSL: Reading PKCS#12 file "
				   "--> OK");
			ok = 1;
			break;
		}

		if (tls_cryptoapi_cert(conn->ssl, private_key) == 0) {
			wpa_printf(MSG_DEBUG, "OpenSSL: Using CryptoAPI to "
				   "access certificate store --> OK");
			ok = 1;
			break;
		}

		break;
	}

	if (!ok) {
		wpa_printf(MSG_INFO, "OpenSSL: Failed to load private key");
		os_free(passwd);
		ERR_clear_error();
		return -1;
	}
	ERR_clear_error();
	SSL_CTX_set_default_passwd_cb(ssl_ctx, NULL);
	os_free(passwd);
	
	if (!SSL_check_private_key(conn->ssl)) {
		tls_show_errors(MSG_INFO, __func__, "Private key failed "
				"verification");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "SSL: Private key loaded successfully");
	return 0;
}


static int tls_global_private_key(SSL_CTX *ssl_ctx, const char *private_key,
				  const char *private_key_passwd)
{
	char *passwd;

	if (private_key == NULL)
		return 0;

	if (private_key_passwd) {
		passwd = os_strdup(private_key_passwd);
		if (passwd == NULL)
			return -1;
	} else
		passwd = NULL;

	SSL_CTX_set_default_passwd_cb(ssl_ctx, tls_passwd_cb);
	SSL_CTX_set_default_passwd_cb_userdata(ssl_ctx, passwd);
	if (
#ifndef OPENSSL_NO_STDIO
	    SSL_CTX_use_PrivateKey_file(ssl_ctx, private_key,
					SSL_FILETYPE_ASN1) != 1 &&
	    SSL_CTX_use_PrivateKey_file(ssl_ctx, private_key,
					SSL_FILETYPE_PEM) != 1 &&
#endif /* OPENSSL_NO_STDIO */
	    tls_read_pkcs12(ssl_ctx, NULL, private_key, passwd)) {
		tls_show_errors(MSG_INFO, __func__,
				"Failed to load private key");
		os_free(passwd);
		ERR_clear_error();
		return -1;
	}
	os_free(passwd);
	ERR_clear_error();
	SSL_CTX_set_default_passwd_cb(ssl_ctx, NULL);
	
	if (!SSL_CTX_check_private_key(ssl_ctx)) {
		tls_show_errors(MSG_INFO, __func__,
				"Private key failed verification");
		return -1;
	}

	return 0;
}


static int tls_connection_dh(struct tls_connection *conn, const char *dh_file)
{
#ifdef OPENSSL_NO_DH
	if (dh_file == NULL)
		return 0;
	wpa_printf(MSG_ERROR, "TLS: openssl does not include DH support, but "
		   "dh_file specified");
	return -1;
#else /* OPENSSL_NO_DH */
	DH *dh;
	BIO *bio;

	/* TODO: add support for dh_blob */
	if (dh_file == NULL)
		return 0;
	if (conn == NULL)
		return -1;

	bio = BIO_new_file(dh_file, "r");
	if (bio == NULL) {
		wpa_printf(MSG_INFO, "TLS: Failed to open DH file '%s': %s",
			   dh_file, ERR_error_string(ERR_get_error(), NULL));
		return -1;
	}
	dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
	BIO_free(bio);
#ifndef OPENSSL_NO_DSA
	while (dh == NULL) {
		DSA *dsa;
		wpa_printf(MSG_DEBUG, "TLS: Failed to parse DH file '%s': %s -"
			   " trying to parse as DSA params", dh_file,
			   ERR_error_string(ERR_get_error(), NULL));
		bio = BIO_new_file(dh_file, "r");
		if (bio == NULL)
			break;
		dsa = PEM_read_bio_DSAparams(bio, NULL, NULL, NULL);
		BIO_free(bio);
		if (!dsa) {
			wpa_printf(MSG_DEBUG, "TLS: Failed to parse DSA file "
				   "'%s': %s", dh_file,
				   ERR_error_string(ERR_get_error(), NULL));
			break;
		}

		wpa_printf(MSG_DEBUG, "TLS: DH file in DSA param format");
		dh = DSA_dup_DH(dsa);
		DSA_free(dsa);
		if (dh == NULL) {
			wpa_printf(MSG_INFO, "TLS: Failed to convert DSA "
				   "params into DH params");
			break;
		}
		break;
	}
#endif /* !OPENSSL_NO_DSA */
	if (dh == NULL) {
		wpa_printf(MSG_INFO, "TLS: Failed to read/parse DH/DSA file "
			   "'%s'", dh_file);
		return -1;
	}

	if (SSL_set_tmp_dh(conn->ssl, dh) != 1) {
		wpa_printf(MSG_INFO, "TLS: Failed to set DH params from '%s': "
			   "%s", dh_file,
			   ERR_error_string(ERR_get_error(), NULL));
		DH_free(dh);
		return -1;
	}
	DH_free(dh);
	return 0;
#endif /* OPENSSL_NO_DH */
}


int tls_connection_get_keys(void *ssl_ctx, struct tls_connection *conn,
			    struct tls_keys *keys)
{
	SSL *ssl;

	if (conn == NULL || keys == NULL)
		return -1;
	ssl = conn->ssl;
	if (ssl == NULL || ssl->s3 == NULL || ssl->session == NULL)
		return -1;

	os_memset(keys, 0, sizeof(*keys));
	keys->master_key = ssl->session->master_key;
	keys->master_key_len = ssl->session->master_key_length;
	keys->client_random = ssl->s3->client_random;
	keys->client_random_len = SSL3_RANDOM_SIZE;
	keys->server_random = ssl->s3->server_random;
	keys->server_random_len = SSL3_RANDOM_SIZE;

	return 0;
}


int tls_connection_prf(void *tls_ctx, struct tls_connection *conn,
		       const char *label, int server_random_first,
		       u8 *out, size_t out_len)
{
	return -1;
}


u8 * tls_connection_handshake(void *ssl_ctx, struct tls_connection *conn,
			      const u8 *in_data, size_t in_len,
			      size_t *out_len, u8 **appl_data,
			      size_t *appl_data_len)
{
	int res;
	u8 *out_data;

	if (appl_data)
		*appl_data = NULL;

	/*
	 * Give TLS handshake data from the server (if available) to OpenSSL
	 * for processing.
	 */
	if (in_data &&
	    BIO_write(conn->ssl_in, in_data, in_len) < 0) {
		tls_show_errors(MSG_INFO, __func__,
				"Handshake failed - BIO_write");
		return NULL;
	}

	/* Initiate TLS handshake or continue the existing handshake */
	res = SSL_connect(conn->ssl);
	if (res != 1) {
		int err = SSL_get_error(conn->ssl, res);
		if (err == SSL_ERROR_WANT_READ)
			wpa_printf(MSG_DEBUG, "SSL: SSL_connect - want "
				   "more data");
		else if (err == SSL_ERROR_WANT_WRITE)
			wpa_printf(MSG_DEBUG, "SSL: SSL_connect - want to "
				   "write");
		else {
			tls_show_errors(MSG_INFO, __func__, "SSL_connect");
			conn->failed++;
		}
	}

	/* Get the TLS handshake data to be sent to the server */
	res = BIO_ctrl_pending(conn->ssl_out);
	wpa_printf(MSG_DEBUG, "SSL: %d bytes pending from ssl_out", res);
	out_data = os_malloc(res == 0 ? 1 : res);
	if (out_data == NULL) {
		wpa_printf(MSG_DEBUG, "SSL: Failed to allocate memory for "
			   "handshake output (%d bytes)", res);
		if (BIO_reset(conn->ssl_out) < 0) {
			tls_show_errors(MSG_INFO, __func__,
					"BIO_reset failed");
		}
		*out_len = 0;
		return NULL;
	}
	res = res == 0 ? 0 : BIO_read(conn->ssl_out, out_data, res);
	if (res < 0) {
		tls_show_errors(MSG_INFO, __func__,
				"Handshake failed - BIO_read");
		if (BIO_reset(conn->ssl_out) < 0) {
			tls_show_errors(MSG_INFO, __func__,
					"BIO_reset failed");
		}
		*out_len = 0;
		return NULL;
	}
	*out_len = res;

	if (SSL_is_init_finished(conn->ssl) && appl_data) {
		*appl_data = os_malloc(in_len);
		if (*appl_data) {
			res = SSL_read(conn->ssl, *appl_data, in_len);
			if (res < 0) {
				tls_show_errors(MSG_INFO, __func__,
						"Failed to read possible "
						"Application Data");
				os_free(*appl_data);
				*appl_data = NULL;
			} else {
				*appl_data_len = res;
				wpa_hexdump_key(MSG_MSGDUMP, "SSL: Application"
						" Data in Finish message",
						*appl_data, *appl_data_len);
			}
		}
	}

	return out_data;
}


u8 * tls_connection_server_handshake(void *ssl_ctx,
				     struct tls_connection *conn,
				     const u8 *in_data, size_t in_len,
				     size_t *out_len)
{
	int res;
	u8 *out_data;
	char buf[10];

	if (in_data &&
	    BIO_write(conn->ssl_in, in_data, in_len) < 0) {
		tls_show_errors(MSG_INFO, __func__,
				"Handshake failed - BIO_write");
		return NULL;
	}

	res = SSL_read(conn->ssl, buf, sizeof(buf));
	if (res >= 0) {
		wpa_printf(MSG_DEBUG, "SSL: Unexpected data from SSL_read "
			   "(res=%d)", res);
	}

	res = BIO_ctrl_pending(conn->ssl_out);
	wpa_printf(MSG_DEBUG, "SSL: %d bytes pending from ssl_out", res);
	out_data = os_malloc(res == 0 ? 1 : res);
	if (out_data == NULL) {
		wpa_printf(MSG_DEBUG, "SSL: Failed to allocate memory for "
			   "handshake output (%d bytes)", res);
		if (BIO_reset(conn->ssl_out) < 0) {
			tls_show_errors(MSG_INFO, __func__,
					"BIO_reset failed");
		}
		*out_len = 0;
		return NULL;
	}
	res = res == 0 ? 0 : BIO_read(conn->ssl_out, out_data, res);
	if (res < 0) {
		tls_show_errors(MSG_INFO, __func__,
				"Handshake failed - BIO_read");
		if (BIO_reset(conn->ssl_out) < 0) {
			tls_show_errors(MSG_INFO, __func__,
					"BIO_reset failed");
		}
		*out_len = 0;
		return NULL;
	}
	*out_len = res;
	return out_data;
}


int tls_connection_encrypt(void *ssl_ctx, struct tls_connection *conn,
			   const u8 *in_data, size_t in_len,
			   u8 *out_data, size_t out_len)
{
	int res;

	if (conn == NULL)
		return -1;

	/* Give plaintext data for OpenSSL to encrypt into the TLS tunnel. */
	if ((res = BIO_reset(conn->ssl_in)) < 0 ||
	    (res = BIO_reset(conn->ssl_out)) < 0) {
		tls_show_errors(MSG_INFO, __func__, "BIO_reset failed");
		return res;
	}
	res = SSL_write(conn->ssl, in_data, in_len);
	if (res < 0) {
		tls_show_errors(MSG_INFO, __func__,
				"Encryption failed - SSL_write");
		return res;
	}

	/* Read encrypted data to be sent to the server */
	res = BIO_read(conn->ssl_out, out_data, out_len);
	if (res < 0) {
		tls_show_errors(MSG_INFO, __func__,
				"Encryption failed - BIO_read");
		return res;
	}

	return res;
}


int tls_connection_decrypt(void *ssl_ctx, struct tls_connection *conn,
			   const u8 *in_data, size_t in_len,
			   u8 *out_data, size_t out_len)
{
	int res;

	/* Give encrypted data from TLS tunnel for OpenSSL to decrypt. */
	res = BIO_write(conn->ssl_in, in_data, in_len);
	if (res < 0) {
		tls_show_errors(MSG_INFO, __func__,
				"Decryption failed - BIO_write");
		return res;
	}
	if (BIO_reset(conn->ssl_out) < 0) {
		tls_show_errors(MSG_INFO, __func__, "BIO_reset failed");
		return res;
	}

	/* Read decrypted data for further processing */
	res = SSL_read(conn->ssl, out_data, out_len);
	if (res < 0) {
		tls_show_errors(MSG_INFO, __func__,
				"Decryption failed - SSL_read");
		return res;
	}

	return res;
}


int tls_connection_resumed(void *ssl_ctx, struct tls_connection *conn)
{
	return conn ? conn->ssl->hit : 0;
}


#if defined(EAP_FAST) || defined(EAP_FAST_DYNAMIC)
/* Pre-shared secred requires a patch to openssl, so this function is
 * commented out unless explicitly needed for EAP-FAST in order to be able to
 * build this file with unmodified openssl. */

static int tls_sess_sec_cb(SSL *s, void *secret, int *secret_len,
			   STACK_OF(SSL_CIPHER) *peer_ciphers,
			   SSL_CIPHER **cipher, void *arg)
{
	struct tls_connection *conn = arg;

	if (conn == NULL || conn->pre_shared_secret == 0)
		return 0;

	os_memcpy(secret, conn->pre_shared_secret,
		  conn->pre_shared_secret_len);
	*secret_len = conn->pre_shared_secret_len;

	return 1;
}


int tls_connection_set_master_key(void *ssl_ctx, struct tls_connection *conn,
				  const u8 *key, size_t key_len)
{
	if (conn == NULL || key_len > SSL_MAX_MASTER_KEY_LENGTH)
		return -1;

	os_free(conn->pre_shared_secret);
	conn->pre_shared_secret = NULL;
	conn->pre_shared_secret_len = 0;

	if (key) {
		conn->pre_shared_secret = os_malloc(key_len);
		if (conn->pre_shared_secret) {
			os_memcpy(conn->pre_shared_secret, key, key_len);
			conn->pre_shared_secret_len = key_len;
		}
		if (SSL_set_session_secret_cb(conn->ssl, tls_sess_sec_cb,
					      conn) != 1)
			return -1;
	} else {
		if (SSL_set_session_secret_cb(conn->ssl, NULL, NULL) != 1)
			return -1;
	}

	return 0;
}
#endif /* EAP_FAST || EAP_FAST_DYNAMIC */


int tls_connection_set_cipher_list(void *tls_ctx, struct tls_connection *conn,
				   u8 *ciphers)
{
	char buf[100], *pos, *end;
	u8 *c;
	int ret;

	if (conn == NULL || conn->ssl == NULL || ciphers == NULL)
		return -1;

	buf[0] = '\0';
	pos = buf;
	end = pos + sizeof(buf);

	c = ciphers;
	while (*c != TLS_CIPHER_NONE) {
		const char *suite;

		switch (*c) {
		case TLS_CIPHER_RC4_SHA:
			suite = "RC4-SHA";
			break;
		case TLS_CIPHER_AES128_SHA:
			suite = "AES128-SHA";
			break;
		case TLS_CIPHER_RSA_DHE_AES128_SHA:
			suite = "DHE-RSA-AES128-SHA";
			break;
		case TLS_CIPHER_ANON_DH_AES128_SHA:
			suite = "ADH-AES128-SHA";
			break;
		default:
			wpa_printf(MSG_DEBUG, "TLS: Unsupported "
				   "cipher selection: %d", *c);
			return -1;
		}
		ret = os_snprintf(pos, end - pos, ":%s", suite);
		if (ret < 0 || ret >= end - pos)
			break;
		pos += ret;

		c++;
	}

	wpa_printf(MSG_DEBUG, "OpenSSL: cipher suites: %s", buf + 1);

	if (SSL_set_cipher_list(conn->ssl, buf + 1) != 1) {
		tls_show_errors(MSG_INFO, __func__,
				"Cipher suite configuration failed");
		return -1;
	}

	return 0;
}


int tls_get_cipher(void *ssl_ctx, struct tls_connection *conn,
		   char *buf, size_t buflen)
{
	const char *name;
	if (conn == NULL || conn->ssl == NULL)
		return -1;

	name = SSL_get_cipher(conn->ssl);
	if (name == NULL)
		return -1;

	os_snprintf(buf, buflen, "%s", name);
	buf[buflen - 1] = '\0';
	return 0;
}


int tls_connection_enable_workaround(void *ssl_ctx,
				     struct tls_connection *conn)
{
	SSL_set_options(conn->ssl, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);

	return 0;
}


#if defined(EAP_FAST) || defined(EAP_FAST_DYNAMIC)
/* ClientHello TLS extensions require a patch to openssl, so this function is
 * commented out unless explicitly needed for EAP-FAST in order to be able to
 * build this file with unmodified openssl. */
int tls_connection_client_hello_ext(void *ssl_ctx, struct tls_connection *conn,
				    int ext_type, const u8 *data,
				    size_t data_len)
{
	if (conn == NULL || conn->ssl == NULL)
		return -1;

	if (SSL_set_hello_extension(conn->ssl, ext_type, (void *) data,
				    data_len) != 1)
		return -1;

	return 0;
}
#endif /* EAP_FAST || EAP_FAST_DYNAMIC */


int tls_connection_get_failed(void *ssl_ctx, struct tls_connection *conn)
{
	if (conn == NULL)
		return -1;
	return conn->failed;
}


int tls_connection_get_read_alerts(void *ssl_ctx, struct tls_connection *conn)
{
	if (conn == NULL)
		return -1;
	return conn->read_alerts;
}


int tls_connection_get_write_alerts(void *ssl_ctx, struct tls_connection *conn)
{
	if (conn == NULL)
		return -1;
	return conn->write_alerts;
}


int tls_connection_set_params(void *tls_ctx, struct tls_connection *conn,
			      const struct tls_connection_params *params)
{
	int ret;
	unsigned long err;

	if (conn == NULL)
		return -1;

	while ((err = ERR_get_error())) {
		wpa_printf(MSG_INFO, "%s: Clearing pending SSL error: %s",
			   __func__, ERR_error_string(err, NULL));
	}

	if (tls_connection_set_subject_match(conn,
					     params->subject_match,
					     params->altsubject_match))
		return -1;
	if (tls_connection_ca_cert(tls_ctx, conn, params->ca_cert,
				   params->ca_cert_blob,
				   params->ca_cert_blob_len,
				   params->ca_path))
		return -1;
	if (tls_connection_client_cert(conn, params->client_cert,
				       params->client_cert_blob,
				       params->client_cert_blob_len))
		return -1;

	if (params->engine) {
		wpa_printf(MSG_DEBUG, "SSL: Initializing TLS engine");
		ret = tls_engine_init(conn, params->engine_id, params->pin,
				      params->key_id);
		if (ret)
			return ret;
		if (tls_connection_engine_private_key(conn))
			return TLS_SET_PARAMS_ENGINE_PRV_VERIFY_FAILED;
	} else if (tls_connection_private_key(tls_ctx, conn,
					      params->private_key,
					      params->private_key_passwd,
					      params->private_key_blob,
					      params->private_key_blob_len)) {
		wpa_printf(MSG_INFO, "TLS: Failed to load private key '%s'",
			   params->private_key);
		return -1;
	}

	if (tls_connection_dh(conn, params->dh_file)) {
		wpa_printf(MSG_INFO, "TLS: Failed to load DH file '%s'",
			   params->dh_file);
		return -1;
	}

	tls_get_errors(tls_ctx);

	return 0;
}


int tls_global_set_params(void *tls_ctx,
			  const struct tls_connection_params *params)
{
	SSL_CTX *ssl_ctx = tls_ctx;
	unsigned long err;

	while ((err = ERR_get_error())) {
		wpa_printf(MSG_INFO, "%s: Clearing pending SSL error: %s",
			   __func__, ERR_error_string(err, NULL));
	}

	if (tls_global_ca_cert(ssl_ctx, params->ca_cert))
		return -1;

	if (tls_global_client_cert(ssl_ctx, params->client_cert))
		return -1;

	if (tls_global_private_key(ssl_ctx, params->private_key,
				   params->private_key_passwd))
		return -1;

	return 0;
}


int tls_connection_get_keyblock_size(void *tls_ctx,
				     struct tls_connection *conn)
{
	const EVP_CIPHER *c;
	const EVP_MD *h;

	if (conn == NULL || conn->ssl == NULL ||
	    conn->ssl->enc_read_ctx == NULL ||
	    conn->ssl->enc_read_ctx->cipher == NULL ||
	    conn->ssl->read_hash == NULL)
		return -1;

	c = conn->ssl->enc_read_ctx->cipher;
	h = conn->ssl->read_hash;

	return 2 * (EVP_CIPHER_key_length(c) +
		    EVP_MD_size(h) +
		    EVP_CIPHER_iv_length(c));
}


unsigned int tls_capabilities(void *tls_ctx)
{
	return 0;
}


int tls_connection_set_ia(void *tls_ctx, struct tls_connection *conn,
			  int tls_ia)
{
	return -1;
}


int tls_connection_ia_send_phase_finished(void *tls_ctx,
					  struct tls_connection *conn,
					  int final,
					  u8 *out_data, size_t out_len)
{
	return -1;
}


int tls_connection_ia_final_phase_finished(void *tls_ctx,
					   struct tls_connection *conn)
{
	return -1;
}


int tls_connection_ia_permute_inner_secret(void *tls_ctx,
					   struct tls_connection *conn,
					   const u8 *key, size_t key_len)
{
	return -1;
}
