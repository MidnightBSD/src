# $FreeBSD: stable/11/usr.sbin/bsnmpd/tools/bsnmptools/Makefile 313242 2017-02-04 17:21:17Z ngie $
# Author: Shteryana Shopova <syrinx@FreeBSD.org>

.include <src.opts.mk>

.PATH: ${.CURDIR}

PROG=	bsnmpget

LIBADD=	bsnmp bsnmptools

CFLAGS+=	-I${.CURDIR:H}/libbsnmptools
LDFLAGS+=	-L${.OBJDIR:H}/libbsnmptools

LINKS=  ${BINDIR}/bsnmpget ${BINDIR}/bsnmpwalk
LINKS+=  ${BINDIR}/bsnmpget ${BINDIR}/bsnmpset

MAN=	bsnmpget.1

MLINKS=		bsnmpget.1 bsnmpwalk.1
MLINKS+=	bsnmpget.1 bsnmpset.1

.include <bsd.prog.mk>
