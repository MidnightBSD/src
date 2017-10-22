# $FreeBSD: release/10.0.0/gnu/usr.bin/gdb/gdb/Makefile 251512 2013-06-07 21:40:02Z emaste $

PROG=	gdb${GDB_SUFFIX}
SRCS=	gdb.c

BULIBS= ${OBJ_BU}/libbfd/libbfd.a ${OBJ_BU}/libopcodes/libopcodes.a \
	${OBJ_BU}/libiberty/libiberty.a
GDBLIBS= ${OBJ_GDB}/libgdb/libgdb.a

# libthread_db.so calls back into gdb for the proc services.  Make all the
# global symbols visible.
LDFLAGS+= -Wl,-E

DPADD=	${GDBLIBS} ${BULIBS} ${LIBM} ${LIBREADLINE} ${LIBTERMCAP} ${LIBGNUREGEX}
LDADD=	${GDBLIBS} ${BULIBS} -lm -lreadline -ltermcap -lgnuregex

.include <bsd.prog.mk>
CFLAGS+=	-DDEBUGDIR=\"${DEBUGDIR}\"
