# $FreeBSD: stable/9/lib/libpam/modules/Makefile.inc 236115 2012-05-26 17:56:54Z des $

PAMDIR=		${.CURDIR}/../../../../contrib/openpam

NO_INSTALLLIB=
NO_PROFILE=

CFLAGS+= -I${PAMDIR}/include -I${.CURDIR}/../../libpam

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
