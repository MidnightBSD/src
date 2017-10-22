# $FreeBSD: stable/9/gnu/usr.bin/gdb/arch/powerpc64/Makefile 241193 2012-10-04 09:53:59Z fjoe $

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
