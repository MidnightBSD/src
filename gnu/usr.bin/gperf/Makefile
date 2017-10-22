# $FreeBSD: stable/9/gnu/usr.bin/gperf/Makefile 230237 2012-01-16 22:17:12Z bapt $

.include <bsd.own.mk>
GPERFDIR=	${.CURDIR}/../../../contrib/gperf

.PATH: ${GPERFDIR}/src ${GPERFDIR}/lib ${GPERFDIR}/doc

SUBDIR=	doc

PROG_CXX=	gperf
SRCS=	bool-array.cc hash-table.cc input.cc keyword-list.cc keyword.cc \
	main.cc options.cc output.cc positions.cc search.cc version.cc \
	getline.cc hash.cc
WARNS=	1

CXXFLAGS+=	-I${GPERFDIR}/lib -I${.CURDIR}

.include <bsd.prog.mk>
