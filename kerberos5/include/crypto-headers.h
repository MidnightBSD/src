/* $FreeBSD: release/10.0.0/kerberos5/include/crypto-headers.h 233294 2012-03-22 08:48:42Z stas $ */
#ifndef __crypto_headers_h__
#define __crypto_headers_h__

#include <openssl/evp.h>
#include <openssl/des.h>
#include <openssl/rc4.h>
#include <openssl/rc2.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/ui.h>
#include <openssl/rand.h>
#include <openssl/engine.h>
#include <openssl/pkcs12.h>
#include <openssl/pem.h>
#include <openssl/hmac.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/ecdh.h>
#ifndef BN_is_negative
#define BN_set_negative(bn, flag) ((bn)->neg=(flag)?1:0)
#define BN_is_negative(bn) ((bn)->neg != 0)
#endif

#endif /* __crypto_headers_h__ */
