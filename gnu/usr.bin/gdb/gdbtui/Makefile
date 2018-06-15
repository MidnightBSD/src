# $MidnightBSD$
# $FreeBSD: stable/10/gnu/usr.bin/gdb/gdbtui/Makefile 276486 2014-12-31 23:25:37Z ngie $

PROG=	gdbtui${GDB_SUFFIX}
SRCS=	tui-main.c
MAN=

BULIBS= ${OBJ_BU}/libbfd/libbfd.a ${OBJ_BU}/libopcodes/libopcodes.a \
	${OBJ_BU}/libiberty/libiberty.a
GDBLIBS= ${OBJ_GDB}/libgdb/libgdb.a

# libthread_db.so calls back into gdb for the proc services.  Make all the
# global symbols visible.
LDFLAGS+= -Wl,-E

DPADD=	${GDBLIBS} ${BULIBS} ${LIBM} ${LIBREADLINE} ${LIBTERMCAP} ${LIBGNUREGEX}
LDADD=	${GDBLIBS} ${BULIBS} -lm -lreadline -ltermcap -lgnuregex

.include <bsd.prog.mk>
