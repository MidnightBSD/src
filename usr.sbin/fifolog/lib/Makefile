# $FreeBSD: release/10.0.0/usr.sbin/fifolog/lib/Makefile 249657 2013-04-19 19:45:00Z ed $

LIB=	fifolog
INTERNALLIB=	# API not published or supported.

SRCS=	fifolog_int.c fifolog_create.c fifolog_write_poll.c fifolog_reader.c
SRCS+=	getdate.y

CFLAGS+= -I${.CURDIR}

NO_WMISSING_VARIABLE_DECLARATIONS=

.include <bsd.lib.mk>
