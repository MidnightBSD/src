# $FreeBSD: stable/11/usr.bin/nm/Makefile 298401 2016-04-21 12:58:29Z emaste $

.include <src.opts.mk>

ELFTCDIR=	${SRCTOP}/contrib/elftoolchain
NMDIR=		${ELFTCDIR}/nm

.PATH: ${NMDIR}

PROG=	nm

LIBADD=	dwarf elftc elf

CFLAGS+=-I${ELFTCDIR}/libelftc -I${ELFTCDIR}/common

.include <bsd.prog.mk>
