#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
# $FreeBSD: stable/9/usr.sbin/rarpd/Makefile 245076 2013-01-05 22:55:08Z markj $

PROG=	rarpd
MAN=	rarpd.8

DPADD=	${LIBUTIL}
LDADD=	-lutil

WARNS?=	3
# This breaks with format strings returned by expand_syslog_m().. argh!
#FORMAT_AUDIT?=	1

.include <bsd.prog.mk>
