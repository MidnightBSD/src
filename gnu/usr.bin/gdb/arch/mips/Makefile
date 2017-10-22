# $FreeBSD: stable/9/gnu/usr.bin/gdb/arch/mips/Makefile 241193 2012-10-04 09:53:59Z fjoe $

.if !defined(GDB_CROSS_DEBUGGER)
LIBSRCS+= mipsfbsd-nat.c fbsd-threads.c
.endif
LIBSRCS+= solib.c solib-svr4.c
LIBSRCS+= mips-tdep.c mipsfbsd-tdep.c fbsd-proc.c

nm.h:
	echo '#include "mips/nm-fbsd.h"' > ${.TARGET}

tm.h:
	echo '#include "mips/tm-fbsd.h"' > ${.TARGET}
