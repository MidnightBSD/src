# $MidnightBSD$
#
# $FreeBSD: stable/10/lib/libauditd/Makefile 313584 2017-02-11 05:53:13Z ngie $
#

OPENBSMDIR=		${SRCTOP}/contrib/openbsm
LIBAUDITDDIR=		${OPENBSMDIR}/libauditd
LIBBSMDIR=		${OPENBSMDIR}/libbsm

LIB=		auditd

.PATH:		${LIBAUDITDDIR}

SRCS=	auditd_lib.c

#
# Must use BSM include files from within the contrib area, not the system.
#
CFLAGS+=	-I${OPENBSMDIR} -I${LIBBSMDIR}

WARNS?=		3

MAN=

.include <bsd.lib.mk>
