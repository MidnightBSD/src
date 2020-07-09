# $FreeBSD: stable/11/tests/sys/pjdfstest/pjdfstest/Makefile 299094 2016-05-04 23:20:53Z ngie $

PACKAGE=	tests

.PATH: ${SRCTOP}/contrib/pjdfstest

BINDIR=		${TESTSBASE}/sys/pjdfstest
PROG=		pjdfstest
MAN=

CFLAGS+=	-D__OS_FreeBSD__ -DHAS_LCHMOD -DHAS_CHFLAGS -DHAS_FCHFLAGS
CFLAGS+=	-DHAS_CHFLAGSAT -DHAS_LCHFLAGS -DHAS_FREEBSD_ACL -DHAS_BINDAT
CFLAGS+=	-DHAS_CONNECTAT

# XXX: the upstream sources need some massaging with how it assigns ngroups in
# pjdfstest.c
NO_WERROR=
WARNS?=		6

.include <bsd.prog.mk>
