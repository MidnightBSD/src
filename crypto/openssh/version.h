/* $OpenBSD: version.h,v 1.83 2018/10/10 16:43:49 deraadt Exp $ */

#define SSH_VERSION	"OpenSSH_7.9"

#define SSH_PORTABLE	"p1"
#define SSH_RELEASE	SSH_VERSION SSH_PORTABLE

#define SSH_VERSION_MIDNIGHTBSD	"MidnightBSD-20211221"

#ifdef WITH_OPENSSL
#define OPENSSL_VERSION_STRING	OpenSSL_version(OPENSSL_VERSION)
#else
#define OPENSSL_VERSION_STRING	"without OpenSSL"
#endif
