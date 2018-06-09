# $MidnightBSD$
# $FreeBSD: stable/10/lib/libproc/Makefile 276486 2014-12-31 23:25:37Z ngie $

.include <bsd.own.mk>

LIB=	proc

SRCS=	proc_bkpt.c		\
	proc_create.c		\
	proc_regs.c		\
	proc_sym.c		\
	proc_rtld.c		\
	proc_util.c

INCS=	libproc.h

CFLAGS+=	-I${.CURDIR}

.if ${MK_LIBCPLUSPLUS} != "no"
LDADD+=		-lcxxrt
DPADD+=		${LIBCXXRT}
.elif ${MK_GNUCXX} != "no"
LDADD+=		-lsupc++
DPADD+=		${LIBSTDCPLUSPLUS}
.else
CFLAGS+=	-DNO_CXA_DEMANGLE
.endif

SHLIB_MAJOR=	2

MAN=

.include <bsd.lib.mk>
