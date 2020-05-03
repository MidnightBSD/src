# $FreeBSD: stable/11/lib/libc++experimental/Makefile 344213 2019-02-16 15:57:29Z dim $

.include <src.opts.mk>

PACKAGE=	clibs
SRCDIR=		${SRCTOP}/contrib/libc++

LIB=		c++experimental
NO_PIC=
MK_PROFILE=	no

.PATH:		${SRCDIR}/src/experimental

SRCS+=		memory_resource.cpp

WARNS?=		0
CXXFLAGS+=	${PICFLAG}
CXXFLAGS+=	-isystem ${SRCDIR}/include
CXXFLAGS+=	-nostdinc++
CXXFLAGS+=	-nostdlib
CXXFLAGS+=	-D_LIBCPP_BUILDING_LIBRARY
CXXFLAGS+=	-DLIBCXXRT
.if empty(CXXFLAGS:M-std=*)
CXXFLAGS+=	-std=c++14
.endif

.include <bsd.lib.mk>
