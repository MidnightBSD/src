# $MidnightBSD$

LIBJANSSON=	${.CURDIR}/../../contrib/jansson

LIB=		jansson
SHLIB_MAJOR=	4
INCS=		jansson.h jansson_config.h
SRCS=		dump.c error.c hashtable.c hashtable_seed.c load.c memory.c \
		pack_unpack.c strbuffer.c strconv.c utf.c value.c

.PATH:		${LIBJANSSON}/src

CFLAGS+=	-I${.CURDIR} -DHAVE_STDINT_H -DHAVE_SYS_PARAM_H

WARNS?=		1

.if !defined(COMPAT_32BIT)
afterinstall:
	${INSTALL} ${SHLINSTALLFLAGS} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
		${_INSTALLFLAGS} ${.CURDIR}/jansson.pc ${DESTDIR}/usr/libdata/pkgconfig/jansson.pc
.endif


.include <bsd.lib.mk>
