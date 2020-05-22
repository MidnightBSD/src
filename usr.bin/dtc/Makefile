# $FreeBSD: stable/11/usr.bin/dtc/Makefile 328825 2018-02-03 01:20:01Z kevans $

PROG_CXX=dtc
SRCS=	dtc.cc input_buffer.cc string.cc dtb.cc fdt.cc checking.cc
MAN=	dtc.1

WARNS?=	3

CXXFLAGS+=	-std=c++11 -fno-rtti -fno-exceptions

NO_SHARED?=NO

.include <bsd.prog.mk>
