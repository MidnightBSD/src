# $MidnightBSD: src/lib/csu/amd64/Makefile,v 1.3 2011/12/18 02:23:42 laffer1 Exp $
# $FreeBSD: src/lib/csu/amd64/Makefile,v 1.18.20.2 2010/01/19 20:19:52 kib Exp $

.PATH: ${.CURDIR}/../common

SRCS=		crt1.c crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
OBJS+=		Scrt1.o gcrt1.o
CFLAGS+=	-I${.CURDIR}/../common \
		-I${.CURDIR}/../../libc/include
CFLAGS+=	-fno-omit-frame-pointer

all: ${OBJS}

CLEANFILES=	${OBJS}
CLEANFILES+=	crt1.s gcrt1.s Scrt1.s

# See the comment in lib/csu/common/crtbrand.c for the reason crt1.c is not
# directly compiled to .o files.

crt1.s: crt1.c
	${CC} ${CFLAGS} -S -o ${.TARGET} ${.CURDIR}/crt1.c
	sed -i "" -e '/\.note\.ABI-tag/s/progbits/note/' ${.TARGET}

crt1.o: crt1.s
	${CC} ${ACFLAGS} -c -o ${.TARGET} crt1.s

gcrt1.s: crt1.c
	${CC} ${CFLAGS} -DGCRT -S -o ${.TARGET} ${.CURDIR}/crt1.c
	sed -i "" -e '/\.note\.ABI-tag/s/progbits/note/' ${.TARGET}

gcrt1.o: gcrt1.s
	${CC} ${ACFLAGS} -c -o ${.TARGET} gcrt1.s

Scrt1.s: crt1.c
	${CC} ${CFLAGS} -fPIC -DPIC -S -o ${.TARGET} ${.CURDIR}/crt1.c
	sed -i "" -e '/\.note\.ABI-tag/s/progbits/note/' ${.TARGET}

Scrt1.o: Scrt1.s
	${CC} ${ACFLAGS} -c -o ${.TARGET} Scrt1.s

realinstall:
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${OBJS} ${DESTDIR}${LIBDIR}

.include <bsd.lib.mk>
