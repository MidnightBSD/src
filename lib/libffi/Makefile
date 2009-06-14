# $MidnightBSD: src/lib/libffi/Makefile,v 1.2 2009/06/09 00:48:55 laffer1 Exp $

FFI_DIST?=	${.CURDIR}/../../contrib/libffi
SHLIB_MAJOR=    1

.PATH: ${FFI_DIST}/src ${FFI_DIST}/src/x86

LIB=	ffi
SRCS=	closures.c debug.c dlmalloc.c java_raw_api.c prep_cif.c \
	raw_api.c types.c freebsd.S ffi.c
INCS=	${.CURDIR}/ffi.h ${FFI_DIST}/src/x86/ffitarget.h

CFLAGS+=-I${.CURDIR} -I${FFI_DIST}/src -I${FFI_DIST}/src/x86 -I${FFI_DIST}/include

MAN=	${FFI_DIST}/man/ffi.3 ${FFI_DIST}/man/ffi_call.3 \
	${FFI_DIST}/man/ffi_prep_cif.3

.include <bsd.lib.mk>
