# $FreeBSD: release/10.0.0/gnu/usr.bin/gdb/arch/powerpc/Makefile 169188 2007-05-01 18:29:34Z marcel $

.if !defined(GDB_CROSS_DEBUGGER)
LIBSRCS+= fbsd-proc.c fbsd-threads.c gcore.c
LIBSRCS+= ppcfbsd-nat.c
.endif
LIBSRCS+= solib.c solib-svr4.c
LIBSRCS+= ppc-sysv-tdep.c ppcfbsd-tdep.c rs6000-tdep.c

nm.h:
	echo '#include "powerpc/nm-fbsd.h"' > ${.TARGET}

tm.h:
	echo '#include "powerpc/tm-ppc-eabi.h"' > ${.TARGET}
