# $FreeBSD: stable/9/usr.sbin/mount_smbfs/Makefile 201390 2010-01-02 11:07:44Z ed $

PROG=	mount_smbfs
SRCS=	mount_smbfs.c getmntopts.c
MAN=	mount_smbfs.8

MOUNTDIR=	${.CURDIR}/../../sbin/mount
CONTRIBDIR=	${.CURDIR}/../../contrib/smbfs
CFLAGS+=	-DSMBFS -I${MOUNTDIR} -I${CONTRIBDIR}/include

LDADD=	-lsmb -lkiconv
DPADD=	${LIBSMB} ${LIBKICONV}

# Needs to be dynamically linked for optional dlopen() access to
# userland libiconv (see the -E option).
#
NO_SHARED?=	NO

.PATH:	${CONTRIBDIR}/mount_smbfs
.PATH:  ${MOUNTDIR}

.include <bsd.prog.mk>
