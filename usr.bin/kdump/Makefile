#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: release/7.0.0/usr.bin/kdump/Makefile 158766 2006-05-20 14:27:22Z netchild $

.PATH: ${.CURDIR}/../ktrace

PROG=		kdump
SRCS=		kdump.c ioctl.c kdump_subr.c subr.c
CFLAGS+=	-I${.CURDIR}/../ktrace -I${.CURDIR} -I${.CURDIR}/../..

CLEANFILES=	ioctl.c kdump_subr.c

ioctl.c: mkioctls
	sh ${.CURDIR}/mkioctls ${DESTDIR}/usr/include > ${.TARGET}

kdump_subr.c: mksubr
	sh ${.CURDIR}/mksubr ${DESTDIR}/usr/include > ${.TARGET}

.include <bsd.prog.mk>
