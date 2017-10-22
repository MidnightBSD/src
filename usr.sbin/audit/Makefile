#
# $FreeBSD: release/10.0.0/usr.sbin/audit/Makefile 201390 2010-01-02 11:07:44Z ed $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/audit

CFLAGS+=	-I${OPENBSMDIR}

PROG=	audit
MAN=	audit.8

DPADD=	${LIBBSM}
LDADD=	-lbsm

WARNS?=	5

.include <bsd.prog.mk>
