# $MidnightBSD$
# $FreeBSD: release/9.2.0/cddl/lib/drti/Makefile 250534 2013-05-12 00:35:57Z brooks $

.PATH:		${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libdtrace/common

SRCS=		drti.c
FILES=		${SRCS:R:S/$/.o/g}
FILESOWN=	${LIBOWN}
FILESGRP=	${LIBGRP}
FILESMODE=	${LIBMODE}
FILESDIR=	${LIBDIR}/dtrace
CLEANFILES=	${FILES}
LIB=		drti-dummy
INTERNALLIB=

CFLAGS+=	-I${.CURDIR}/../../../sys/cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/compat/opensolaris/include \
		-I${OPENSOLARIS_USR_DISTDIR}/head \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libctf/common \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libdtrace/common \
		-I${OPENSOLARIS_SYS_DISTDIR}/uts/common \
		-DPIC ${PICFLAG}

.if make(install) && defined(LIBRARIES_ONLY)
install:
	${INSTALL} -o ${FILESOWN} -g ${FILESGRP} -m ${FILESMODE} \
	    ${FILES} ${DESTDIR}${FILESDIR}
.endif

.include <bsd.lib.mk>
