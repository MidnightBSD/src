/* $FreeBSD: src/gnu/usr.bin/binutils/gdb/fbsd-kgdb-i386.h,v 1.5 2004/01/26 19:40:47 obrien Exp $ */

#ifndef FBSD_KGDB_I386_H
#define FBSD_KGDB_I386_H

#include "i386/tm-fbsd.h"
#include "fbsd-kgdb.h"

/* Override FRAME_SAVED_PC to enable the recognition of signal handlers.  */
#undef  FRAME_SAVED_PC
#define FRAME_SAVED_PC(FRAME) \
  (kernel_debugging \
    ? fbsd_kern_frame_saved_pc (FRAME) \
    : i386bsd_frame_saved_pc (FRAME))
extern CORE_ADDR fbsd_kern_frame_saved_pc(struct frame_info *fr);

/* Offset to saved PC in sigcontext, from <sys/signal.h>.  */
/* DEO:XXX where is this really from??? */
#define SIGCONTEXT_PC_OFFSET 20

#endif /* FBSD_KGDB_I386_H */
