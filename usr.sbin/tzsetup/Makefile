# $FreeBSD: stable/11/usr.sbin/tzsetup/Makefile 319193 2017-05-30 07:29:18Z ngie $

.include <src.opts.mk>

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I.

.if ${MK_DIALOG} != no
WARNS?=	3
CFLAGS+=	-I${SRCTOP}/contrib/dialog -DHAVE_DIALOG
LIBADD=	dialog ncursesw
.endif

.include <bsd.prog.mk>
