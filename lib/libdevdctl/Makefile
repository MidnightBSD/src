# $FreeBSD: stable/11/lib/libdevdctl/Makefile 300906 2016-05-28 17:43:40Z asomers $

LIB_CXX=	devdctl
INCS=	consumer.h		\
	event.h			\
	event_factory.h		\
	exception.h		\
	guid.h
SRCS=	consumer.cc		\
	event.cc		\
	event_factory.cc	\
	exception.cc		\
	guid.cc

INCSDIR= ${INCLUDEDIR}/devdctl

WARNS?=	3
PRIVATELIB=	true
SHLIB_MAJOR=	0

.include <bsd.lib.mk>
