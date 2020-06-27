# $FreeBSD: stable/11/usr.sbin/wpa/wpa_passphrase/Makefile 319190 2017-05-30 04:45:11Z ngie $

.include "../Makefile.inc"

.PATH.c:${WPA_SUPPLICANT_DISTDIR}

PROG=	wpa_passphrase
SRCS=	common.c md5-internal.c md5.c os_unix.c sha1-internal.c sha1-pbkdf2.c \
	sha1.c wpa_passphrase.c

CFLAGS+= -DCONFIG_CRYPTO_INTERNAL -DINTERNAL_SHA1 -DINTERNAL_MD5

LIBADD+=	util

MAN=	wpa_passphrase.8

.include <bsd.prog.mk>
