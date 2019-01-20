# $MidnightBSD$
# $FreeBSD: stable/11/secure/lib/libcrypto/engines/Makefile.inc 319189 2017-05-30 04:42:36Z ngie $

LCRYPTO_SRC=	${SRCTOP}/crypto/openssl
.PATH: ${LCRYPTO_SRC}/engines ${LCRYPTO_SRC}/engines/ccgost

SHLIBDIR?= /usr/lib/engines
