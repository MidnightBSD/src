# $FreeBSD: stable/11/usr.sbin/fifolog/lib/Makefile 291735 2015-12-04 03:17:17Z bdrewery $

LIB=	fifolog
INTERNALLIB=	# API not published or supported.

SRCS=	fifolog_int.c fifolog_create.c fifolog_write_poll.c fifolog_reader.c
SRCS+=	getdate.y

CFLAGS+= -I${.CURDIR}
LIBADD=	z

NO_WMISSING_VARIABLE_DECLARATIONS=

.include <bsd.lib.mk>
