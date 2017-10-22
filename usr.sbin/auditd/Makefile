#
# $FreeBSD: release/10.0.0/usr.sbin/auditd/Makefile 204352 2010-02-26 09:41:16Z ru $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/auditd

CFLAGS+=	-I${OPENBSMDIR}

PROG=	auditd
SRCS=	auditd.c audit_warn.c auditd_fbsd.c
MAN=	auditd.8

DPADD=	${LIBAUDITD} ${LIBBSM}
LDADD=	-lauditd -lbsm

WARNS?=	3

.include <bsd.prog.mk>
