# $FreeBSD: stable/9/sbin/camcontrol/Makefile 237740 2012-06-29 03:37:23Z scottl $

PROG=	camcontrol
SRCS=	camcontrol.c util.c
.if !defined(RELEASE_CRUNCH)
SRCS+=	fwdownload.c modeedit.c progress.c
.else
CFLAGS+= -DMINIMALISTIC
.endif
# This is verboten
.if ${MACHINE_CPUARCH} == "arm"
WARNS?= 3
.endif
DPADD=	${LIBCAM} ${LIBSBUF} ${LIBUTIL}
LDADD=	-lcam -lsbuf -lutil
MAN=	camcontrol.8

.include <bsd.prog.mk>
