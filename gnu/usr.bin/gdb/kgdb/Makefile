# $FreeBSD: stable/11/gnu/usr.bin/gdb/kgdb/Makefile 291738 2015-12-04 03:17:47Z bdrewery $

PROG=	kgdb${GDB_SUFFIX}
SRCS=	main.c kld.c kthr.c trgt.c trgt_${TARGET_CPUARCH}.c
WARNS?=	2

BULIBS= ${OBJ_BU}/libbfd/libbfd.a ${OBJ_BU}/libopcodes/libopcodes.a \
	${OBJ_BU}/libiberty/libiberty.a
GDBLIBS= ${OBJ_GDB}/libgdb/libgdb.a

DPADD=	${GDBLIBS} ${BULIBS}
LDADD=	${GDBLIBS} ${BULIBS}
LIBADD+=	m readline ncursesw gnuregex kvm

.include <bsd.prog.mk>
