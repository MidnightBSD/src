# $MidnightBSD: src/gnu/usr.bin/gdb/kgdb/Makefile,v 1.2 2012/07/04 13:37:32 laffer1 Exp $
# $FreeBSD: src/gnu/usr.bin/gdb/kgdb/Makefile,v 1.6.10.1.2.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	kgdb${GDB_SUFFIX}
SRCS=	main.c kld.c kthr.c trgt.c trgt_${TARGET_CPUARCH}.c
WARNS?=	2

BULIBS= ${OBJ_BU}/libbfd/libbfd.a ${OBJ_BU}/libopcodes/libopcodes.a \
	${OBJ_BU}/libiberty/libiberty.a
GDBLIBS= ${OBJ_GDB}/libgdb/libgdb.a

DPADD=	${GDBLIBS} ${BULIBS} ${LIBKVM} ${LIBM} ${LIBREADLINE} ${LIBTERMCAP} ${LIBGNUREGEX}
LDADD=	${GDBLIBS} ${BULIBS} -lkvm -lm -lreadline -ltermcap -lgnuregex

.include <bsd.prog.mk>
