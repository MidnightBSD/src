# $FreeBSD: stable/11/lib/libpam/modules/Makefile.inc 313538 2017-02-10 07:32:40Z ngie $

PAMDIR=		${SRCTOP}/contrib/openpam

MK_INSTALLLIB=	no
MK_PROFILE=	no

CFLAGS+= -I${PAMDIR}/include -I${SRCTOP}/lib/libpam

SHLIB_NAME?=	${LIB}.so.${SHLIB_MAJOR}
LIBADD+=	pam

.include "../Makefile.inc"
