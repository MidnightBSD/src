/*	$NetBSD: shmat.S,v 1.1 2000/12/29 20:14:04 bjh21 Exp $	*/

#include <machine/asm.h>
__FBSDID("$FreeBSD: stable/11/lib/libc/arm/sys/shmat.S 288373 2015-09-29 16:09:58Z kib $");
#include "SYS.h"

RSYSCALL(shmat)

	.section .note.GNU-stack,"",%progbits
