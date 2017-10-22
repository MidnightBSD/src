# $FreeBSD: stable/9/secure/lib/libcrypto/engines/Makefile.inc 160819 2006-07-29 19:41:41Z simon $

LCRYPTO_SRC=	${.CURDIR}/../../../../../crypto/openssl
.PATH: ${LCRYPTO_SRC}/engines

SHLIBDIR?= /usr/lib/engines
