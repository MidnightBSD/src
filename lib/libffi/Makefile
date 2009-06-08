# $MidnightBSD$

FFI_DIST?=	${.CURDIR}/../../contrib/libffi
SHLIB_MAJOR=    1

.PATH: ${FFI_DIST}/src ${FFI_DIST}/src/x86

LIB=	ffi
SRCS=	closures.c debug.c dlmalloc.c java_raw_api.c prep_cif.c \
	raw_api.c types.c freebsd.S ffi.c

CFLAGS+=-I${.CURDIR} -I${FFI_DIST}/src -I${FFI_DIST}/src/x86 -I${FFI_DIST}/include

.include <bsd.lib.mk>
