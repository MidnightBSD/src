# $FreeBSD: stable/11/lib/libssp/Makefile 359059 2020-03-18 01:55:27Z kevans $

PACKAGE=	clibs
SHLIBDIR?=	/lib
SHLIB=		ssp
SHLIB_MAJOR=	0

VERSION_DEF=	${.CURDIR}/Versions.def
SYMBOL_MAPS=	${.CURDIR}/Symbol.map

.PATH: ${SRCTOP}/lib/libc/secure
CFLAGS+=	-I${SRCTOP}/lib/libc/include
# _elf_aux_info is exported from libc as elf_aux_info(3), so just that for the
# libssp build instead.
CFLAGS+=	-D_elf_aux_info=elf_aux_info
SRCS=		stack_protector.c fortify_stubs.c

CFLAGS.fortify_stubs.c=	-Wno-unused-parameter

# Stack protection on libssp symbols should be considered harmful, as we may
# be talking about, for example, the guard setup constructor.
SSP_CFLAGS:=

.include <bsd.lib.mk>
