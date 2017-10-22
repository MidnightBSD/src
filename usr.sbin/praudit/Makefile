#
# $FreeBSD: release/10.0.0/usr.sbin/praudit/Makefile 201390 2010-01-02 11:07:44Z ed $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/praudit

PROG=	praudit
MAN=	praudit.1

WARNS?=	3

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
