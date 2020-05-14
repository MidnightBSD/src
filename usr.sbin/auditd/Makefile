#
# $FreeBSD: stable/11/usr.sbin/auditd/Makefile 319119 2017-05-29 10:15:41Z ngie $
#

OPENBSMDIR=${SRCTOP}/contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/auditd

CFLAGS+=	-I${OPENBSMDIR}

PROG=	auditd
SRCS=	auditd.c audit_warn.c auditd_fbsd.c
MAN=	auditd.8

LIBADD=	auditd bsm

WARNS?=	3

.include <bsd.prog.mk>
