#
# $FreeBSD: stable/11/usr.sbin/audit/Makefile 319119 2017-05-29 10:15:41Z ngie $
#

OPENBSMDIR=${SRCTOP}/contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/audit

CFLAGS+=	-I${OPENBSMDIR}

PROG=	audit
MAN=	audit.8

LIBADD=	bsm

WARNS?=	5

.include <bsd.prog.mk>
