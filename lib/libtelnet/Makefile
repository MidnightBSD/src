#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
# $FreeBSD: stable/11/lib/libtelnet/Makefile 323030 2017-08-30 19:38:43Z cy $

.include <src.opts.mk>

PACKAGE=lib${LIB}
TELNETDIR=	${SRCTOP}/contrib/telnet
.PATH:		${TELNETDIR}/libtelnet

LIB=		telnet

INTERNALLIB=

SRCS=		genget.c getent.c misc.c
CFLAGS+=	-I${TELNETDIR}

WARNS?=		2

.if ${MK_OPENSSL} != "no"
SRCS+=		encrypt.c auth.c enc_des.c sra.c pk.c
CFLAGS+=	-DENCRYPTION -DAUTHENTICATION -DSRA
.endif

.if ${MK_KERBEROS_SUPPORT} != "no"
SRCS+=		kerberos5.c
CFLAGS+=	-DKRB5 -DFORWARD -Dnet_write=telnet_net_write
.endif

.include <bsd.lib.mk>
