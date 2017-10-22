#
# $FreeBSD: release/7.0.0/usr.sbin/praudit/Makefile 155213 2006-02-02 10:15:30Z rwatson $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/praudit

PROG=	praudit
MAN=	praudit.1

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
