#	@(#)Makefile	8.2 (Berkeley) 3/17/94
# $FreeBSD: stable/11/sbin/bsdlabel/Makefile 319185 2017-05-30 04:17:22Z ngie $

.PATH: ${SRCTOP}/sys/geom

PACKAGE=runtime
PROG=	bsdlabel
SRCS=	bsdlabel.c geom_bsd_enc.c
MAN+=	bsdlabel.8

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
LINKS=	${BINDIR}/bsdlabel ${BINDIR}/disklabel
MLINKS=	bsdlabel.8 disklabel.8
.endif

LIBADD=	geom

.include <bsd.prog.mk>
