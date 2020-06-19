#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/11/usr.sbin/inetd/Makefile 356388 2020-01-05 21:35:02Z kevans $

.include <src.opts.mk>

PROG=	inetd
MAN=	inetd.8
MLINKS=	inetd.8 inetd.conf.5
SRCS=	inetd.c builtins.c

CFLAGS+= -DLOGIN_CAP
#CFLAGS+= -DSANITY_CHECK

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

LIBADD=	util

.if ${MK_TCP_WRAPPERS} != "no"
CFLAGS+=	-DLIBWRAP
LIBADD+=	wrap
.endif

# XXX for src/release/picobsd
.if !defined(RELEASE_CRUNCH)
CFLAGS+= -DIPSEC
LIBADD+=	ipsec
.endif

.include <bsd.prog.mk>
