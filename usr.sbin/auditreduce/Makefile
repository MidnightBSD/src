#
# $FreeBSD: release/10.0.0/usr.sbin/auditreduce/Makefile 201390 2010-01-02 11:07:44Z ed $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/auditreduce

CFLAGS+=	-I${OPENBSMDIR}

PROG=	auditreduce
MAN=	auditreduce.1

DPADD=	${LIBBSM}
LDADD=	-lbsm

WARNS?=	3

.include <bsd.prog.mk>
