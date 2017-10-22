# $FreeBSD: stable/9/usr.sbin/fifolog/lib/Makefile 178068 2008-04-10 14:02:00Z ru $

LIB=	fifolog
INTERNALLIB=	# API not published or supported.

SRCS=	fifolog_int.c fifolog_create.c fifolog_write_poll.c fifolog_reader.c
SRCS+=	getdate.y

CFLAGS+= -I${.CURDIR}

.include <bsd.lib.mk>
