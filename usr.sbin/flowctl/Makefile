#
# $FreeBSD: release/10.0.0/usr.sbin/flowctl/Makefile 223824 2011-07-06 15:18:32Z pluknet $
#

.include <bsd.own.mk>
	
PROG=	flowctl
MAN=	flowctl.8

WARNS?= 2
DPADD=  ${LIBNETGRAPH}
LDADD=  -lnetgraph

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif
.if ${MK_INET_SUPPORT} != "no"
CFLAGS+= -DINET
.endif

.include <bsd.prog.mk>
