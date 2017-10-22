# $FreeBSD: stable/9/usr.bin/lex/lib/Makefile 201386 2010-01-02 10:27:05Z ed $

.include <bsd.own.mk>

LIB=    ln
SRCS=   libmain.c libyywrap.c
NO_PIC=

WARNS?=	2

.if ${MK_INSTALLLIB} != "no"
LINKS=  ${LIBDIR}/libln.a ${LIBDIR}/libl.a
LINKS+=	${LIBDIR}/libln.a ${LIBDIR}/libfl.a
.endif

.if ${MK_PROFILE} != "no"
LINKS+= ${LIBDIR}/libln_p.a ${LIBDIR}/libl_p.a
LINKS+=	${LIBDIR}/libln_p.a ${LIBDIR}/libfl_p.a
.endif

.include <bsd.lib.mk>

