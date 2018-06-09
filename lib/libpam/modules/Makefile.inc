# $MidnightBSD$
# $FreeBSD: stable/10/lib/libpam/modules/Makefile.inc 313579 2017-02-11 05:41:53Z ngie $

PAMDIR=		${SRCTOP}/contrib/openpam

NO_INSTALLLIB=
NO_PROFILE=

CFLAGS+= -I${PAMDIR}/include -I${SRCTOP}/lib/libpam

# This is nasty.
# For the static case, libpam.a depends on the modules.
# For the dynamic case, the modules depend on libpam.so.N
.if defined(_NO_LIBPAM_SO_YET)
NO_PIC=
.else
SHLIB_NAME?=	${LIB}.so.${SHLIB_MAJOR}
DPADD+=		${LIBPAM}
LDADD+=		-lpam
.endif

.include "../Makefile.inc"
