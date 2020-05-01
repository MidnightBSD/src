# $FreeBSD: stable/11/usr.sbin/uathload/Makefile 319191 2017-05-30 05:15:09Z ngie $

PROG=	uathload
MAN=	uathload.8

SRCS=	uathload.c
FILES=	ar5523.bin
FILESDIR=	${SHAREDIR}/firmware
FILESMODE=	444

CLEANFILES=	ar5523.bin

ar5523.bin: ${SRCTOP}/sys/contrib/dev/uath/ar5523.bin.uu
	uudecode -p ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
