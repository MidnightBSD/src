#
# $FreeBSD: stable/11/usr.sbin/praudit/Makefile 336212 2018-07-11 20:11:06Z asomers $
#

OPENBSMDIR=${SRCTOP}/contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/praudit

PROG=	praudit
MAN=	praudit.1

WARNS?=	3

LIBADD=	bsm

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
