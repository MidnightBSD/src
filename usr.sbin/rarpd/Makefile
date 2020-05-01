#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
# $FreeBSD: stable/11/usr.sbin/rarpd/Makefile 275054 2014-11-25 16:57:27Z bapt $

PROG=	rarpd
MAN=	rarpd.8

LIBADD=	util

WARNS?=	3
# This breaks with format strings returned by expand_syslog_m().. argh!
#FORMAT_AUDIT?=	1

.include <bsd.prog.mk>
