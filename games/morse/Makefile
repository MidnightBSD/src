#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: release/7.0.0/games/morse/Makefile 152306 2005-11-11 09:57:32Z ru $

PROG=	morse
MAN=	morse.6

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
CFLAGS += -DSPEAKER=\"/dev/speaker\"
.endif

.include <bsd.prog.mk>
