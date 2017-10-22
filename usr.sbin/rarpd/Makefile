#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
# $FreeBSD: release/10.0.0/usr.sbin/rarpd/Makefile 238282 2012-07-09 08:11:16Z hrs $

PROG=	rarpd
MAN=	rarpd.8

DPADD=	${LIBUTIL}
LDADD=	-lutil

WARNS?=	3
# This breaks with format strings returned by expand_syslog_m().. argh!
#FORMAT_AUDIT?=	1

.include <bsd.prog.mk>
