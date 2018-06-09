# $MidnightBSD$
# $FreeBSD: stable/10/lib/libthr/tests/dlopen/dso/Makefile 313488 2017-02-09 22:49:48Z ngie $

TESTSRC=	${SRCTOP}/contrib/netbsd-tests/lib/libpthread/dlopen/dso

SHLIB=		h_pthread_dlopen
SHLIB_MAJOR=    1
SHLIB_NAME=	h_pthread_dlopen.so.${SHLIB_MAJOR}
SRCS=		h_pthread_dlopen.c

DPADD+=		${LIBPTHREAD}
LDADD+=		-lpthread

LIBDIR=         ${TESTSBASE}/lib/libthr/dlopen

.include <netbsd-tests.test.mk>

.include <bsd.lib.mk>
