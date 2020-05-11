#
# $FreeBSD: stable/11/usr.sbin/flowctl/Makefile 275054 2014-11-25 16:57:27Z bapt $
#

.include <src.opts.mk>
	
PROG=	flowctl
MAN=	flowctl.8

WARNS?= 2
LIBADD=	netgraph

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif
.if ${MK_INET_SUPPORT} != "no"
CFLAGS+= -DINET
.endif

.include <bsd.prog.mk>
