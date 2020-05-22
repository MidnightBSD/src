#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/11/usr.bin/xinstall/Makefile 319186 2017-05-30 04:27:05Z ngie $

.include <src.opts.mk>

PROG=		xinstall
PROGNAME=	install
SRCS=		xinstall.c getid.c
MAN=		install.1

.PATH:		${SRCTOP}/contrib/mtree
CFLAGS+=	-I${SRCTOP}/contrib/mtree
CFLAGS+=	-I${SRCTOP}/lib/libnetbsd

LIBADD=	md

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
