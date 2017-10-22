# $FreeBSD: release/10.0.0/lib/libcxxrt/Makefile 236442 2012-06-02 11:00:48Z dim $

SRCDIR=		${.CURDIR}/../../contrib/libcxxrt

SHLIB_MAJOR=	1
SHLIBDIR?=	/lib

.PATH: ${SRCDIR}

LIB=		cxxrt

SRCS+=		libelftc_dem_gnu3.c\
		terminate.cc\
		dynamic_cast.cc\
		memory.cc\
		auxhelper.cc\
		exception.cc\
		stdexcept.cc\
		typeinfo.cc\
		guard.cc

WARNS=		0
CFLAGS+=	-I${SRCDIR}
VERSION_MAP=	${.CURDIR}/Version.map

.include <bsd.lib.mk>
