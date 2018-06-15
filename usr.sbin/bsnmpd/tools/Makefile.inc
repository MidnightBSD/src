# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/bsnmpd/tools/Makefile.inc 301669 2016-06-08 18:05:52Z ngie $
# Author: Shteryana Shopova <syrinx@FreeBSD.org>

BINDIR?= /usr/bin

CFLAGS+= -I. -I${.CURDIR}

.if exists(${.OBJDIR}/../libbsnmptools)
LIBBSNMPTOOLSDIR=      ${.OBJDIR}/../libbsnmptools
.else
LIBBSNMPTOOLSDIR=      ${.CURDIR}/../libbsnmptools
.endif
LIBBSNMPTOOLS=         ${LIBBSNMPTOOLSDIR}/libbsnmptools.a

WARNS?=		6
