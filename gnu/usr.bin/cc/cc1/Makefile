# $FreeBSD: stable/9/gnu/usr.bin/cc/cc1/Makefile 235398 2012-05-13 13:57:53Z kib $

.include "../Makefile.inc"
NO_MAN=
.include <bsd.own.mk>

.PATH: ${GCCDIR}

PROG=	cc1
SRCS=	main.c c-parser.c c-lang.c
BINDIR=	/usr/libexec
.if ${MK_SHARED_TOOLCHAIN} == "no"
NO_SHARED?=yes
.endif

OBJS+=  ${PROG}-checksum.o
DPADD=	${LIBBACKEND} ${LIBCPP} ${LIBDECNUMBER} ${LIBIBERTY}
LDADD=	${LIBBACKEND} ${LIBCPP} ${LIBDECNUMBER} ${LIBIBERTY}

DOBJS+=	${SRCS:N*.h:R:S/$/.o/g}
${PROG}-dummy:	${DOBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${DOBJS} ${LDADD}
CLEANFILES+=	${PROG}-dummy

${PROG}-checksum.c:	${PROG}-dummy
	../cc_tools/genchecksum ${PROG}-dummy > ${.TARGET}
CLEANFILES+=	${PROG}-checksum.c

.include <bsd.prog.mk>
