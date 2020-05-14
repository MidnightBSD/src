#
# $FreeBSD: stable/11/usr.sbin/auditreduce/Makefile 319119 2017-05-29 10:15:41Z ngie $
#

OPENBSMDIR=${SRCTOP}/contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/auditreduce

CFLAGS+=	-I${OPENBSMDIR}

PROG=	auditreduce
MAN=	auditreduce.1

LIBADD=	bsm

WARNS?=	3

.include <bsd.prog.mk>
