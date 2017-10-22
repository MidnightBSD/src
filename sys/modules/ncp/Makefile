# $FreeBSD: stable/9/sys/modules/ncp/Makefile 124717 2004-01-19 14:00:26Z ru $

.PATH: ${.CURDIR}/../../netncp

KMOD=	ncp

SRCS=	ncp_conn.c ncp_sock.c ncp_ncp.c ncp_subr.c ncp_crypt.c ncp_mod.c \
	ncp_rq.c ncp_login.c ncp_nls.c opt_ncp.h

.if defined(NCPBURST)
SRCS+=	ncp_burst.c
CFLAGS+= -DNCPBURST
.endif

.include <bsd.kmod.mk>
