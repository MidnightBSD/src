# $FreeBSD: stable/11/usr.bin/xo/Makefile 322172 2017-08-07 17:23:44Z phil $

.include <src.opts.mk>

LIBXOSRC=	${SRCTOP}/contrib/libxo

.PATH:
.PATH:	${LIBXOSRC}/xo

PROG=	xo
MAN=	xo.1

# XXX For xoversion.h
CFLAGS+=-I${LIBXOSRC}/libxo

# XXX For xo_config.h
CFLAGS+=-I${SRCTOP}/lib/libxo

LIBADD=	xo util

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
