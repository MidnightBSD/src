# $FreeBSD: stable/11/lib/librt/Makefile 313538 2017-02-10 07:32:40Z ngie $

.include <src.opts.mk>

PACKAGE=lib${LIB}
LIB=rt
SHLIB_MAJOR= 1
CFLAGS+=-I${SRCTOP}/lib/libc/include -I${.CURDIR}
.ifndef NO_THREAD_STACK_UNWIND
CFLAGS+=-fexceptions
.endif
CFLAGS+=-Winline -Wall
LIBADD=	pthread

WARNS?=	2

SRCS+= aio.c mq.c sigev_thread.c timer.c

PRECIOUSLIB=

VERSION_DEF=${SRCTOP}/lib/libc/Versions.def
SYMBOL_MAPS=${.CURDIR}/Symbol.map

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.lib.mk>
