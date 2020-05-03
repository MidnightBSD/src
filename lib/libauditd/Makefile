#
# $FreeBSD: stable/11/lib/libauditd/Makefile 313538 2017-02-10 07:32:40Z ngie $
#

PACKAGE=lib${LIB}
OPENBSMDIR=		${SRCTOP}/contrib/openbsm
_LIBAUDITDDIR=		${OPENBSMDIR}/libauditd
_LIBBSMDIR=		${OPENBSMDIR}/libbsm

LIB=		auditd

.PATH:		${_LIBAUDITDDIR}

SRCS=	auditd_lib.c

#
# Must use BSM include files from within the contrib area, not the system.
#
CFLAGS+=	-I${OPENBSMDIR} -I${_LIBBSMDIR}

WARNS?=		3

MAN=

.include <bsd.lib.mk>
