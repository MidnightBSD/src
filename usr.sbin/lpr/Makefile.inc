# $FreeBSD: release/10.0.0/usr.sbin/lpr/Makefile.inc 160753 2006-07-27 14:52:12Z yar $

.include <bsd.own.mk>

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

LIBLPR=	${.OBJDIR}/../common_source/liblpr.a

.include "../Makefile.inc"
