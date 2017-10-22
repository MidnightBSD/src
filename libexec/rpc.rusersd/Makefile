# $FreeBSD: stable/9/libexec/rpc.rusersd/Makefile 202210 2010-01-13 18:28:58Z ed $

PROG =	rpc.rusersd
SRCS =	rusersd.c rusers_proc.c
MAN =	rpc.rusersd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

#.if exists(/usr/X11R6/include/X11/extensions/xidle.h)
#CFLAGS+= -DXIDLE
#LDADD+= -L/usr/X11R6/lib -lXext -lX11
#.endif

.include <bsd.prog.mk>
