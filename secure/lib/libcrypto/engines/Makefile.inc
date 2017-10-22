# $FreeBSD: release/10.0.0/secure/lib/libcrypto/engines/Makefile.inc 238405 2012-07-12 19:30:53Z jkim $

LCRYPTO_SRC=	${.CURDIR}/../../../../../crypto/openssl
.PATH: ${LCRYPTO_SRC}/engines ${LCRYPTO_SRC}/engines/ccgost

SHLIBDIR?= /usr/lib/engines
