# $FreeBSD: release/10.0.0/gnu/usr.bin/gdb/arch/sparc64/Makefile 224687 2011-08-06 17:53:45Z marius $

.if !defined(GDB_CROSS_DEBUGGER)
LIBSRCS+= fbsd-threads.c
LIBSRCS+= solib.c solib-legacy.c solib-svr4.c
LIBSRCS+= sparc-nat.c sparc64-nat.c sparc64fbsd-nat.c
.endif
LIBSRCS+= sparc-tdep.c sparc64-tdep.c sparc64fbsd-tdep.c

nm.h:
	echo '#include "sparc/nm-fbsd.h"' > ${.TARGET}

tm.h:
	echo '#include "sparc/tm-fbsd.h"' > ${.TARGET}
