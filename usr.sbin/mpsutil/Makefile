# $FreeBSD: stable/11/usr.sbin/mpsutil/Makefile 319119 2017-05-29 10:15:41Z ngie $

PROG=	mpsutil
SRCS=	mps_cmd.c mps_flash.c mps_show.c mpsutil.c
MAN=	mpsutil.8

WARNS?= 3

#LIBADD=	cam util
LINKS=	${BINDIR}/mpsutil ${BINDIR}/mprutil
MLINKS=	mpsutil.8 mprutil.8

CFLAGS+= -I${SRCTOP}/sys -I. -DUSE_MPT_IOCTLS
# Avoid dirdep dependency on libutil
CFLAGS+= -I${SRCTOP}/lib/libutil

# Here be dragons
.ifdef DEBUG
CFLAGS+= -DDEBUG
.endif

.include <bsd.prog.mk>
