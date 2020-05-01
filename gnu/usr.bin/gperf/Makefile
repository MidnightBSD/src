# $FreeBSD: stable/11/gnu/usr.bin/gperf/Makefile 279520 2015-03-02 11:49:01Z bapt $

.include <bsd.own.mk>
GPERFDIR=	${.CURDIR}/../../../contrib/gperf

.PATH: ${GPERFDIR}/src ${GPERFDIR}/lib ${GPERFDIR}/doc

PROG_CXX=	gperf
SRCS=	bool-array.cc hash-table.cc input.cc keyword-list.cc keyword.cc \
	main.cc options.cc output.cc positions.cc search.cc version.cc \
	getline.cc hash.cc
WARNS=	1
MAN=	gperf.1 gperf.7

CXXFLAGS+=	-I${GPERFDIR}/lib -I${.CURDIR}

.include <bsd.prog.mk>
