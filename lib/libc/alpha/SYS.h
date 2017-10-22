/*	From: NetBSD: SYS.h,v 1.5 1997/05/02 18:15:15 kleink Exp */

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 * $FreeBSD: src/lib/libc/alpha/SYS.h,v 1.16 2003/10/29 10:45:00 tjr Exp $
 */

#include <machine/asm.h>
#include <sys/syscall.h>

#define	CALLSYS_ERROR(name)					\
	CALLSYS_NOERROR(name);					\
	br	gp, LLABEL(name,0);				\
LLABEL(name,0):							\
	LDGP(gp);						\
	beq	a3, LLABEL(name,1);				\
	jmp	zero, .cerror;					\
LLABEL(name,1):


#define	SYSCALL(name)						\
LEAF(__CONCAT(__sys_,name),0);		/* XXX # of args? */	\
	WEAK_ALIAS(name, __CONCAT(__sys_,name));		\
	WEAK_ALIAS(__CONCAT(_,name), __CONCAT(__sys_,name));	\
	CALLSYS_ERROR(name)

#define	SYSCALL_NOERROR(name)					\
LEAF(__CONCAT(__sys_,name),0);		/* XXX # of args? */	\
	WEAK_ALIAS(name, __CONCAT(__sys_,name));		\
	WEAK_ALIAS(__CONCAT(_,name), __CONCAT(__sys_,name));	\
	CALLSYS_NOERROR(name)


#define RSYSCALL(name)						\
	SYSCALL(name);						\
	RET;							\
END(__CONCAT(__sys_,name))

#define RSYSCALL_NOERROR(name)					\
	SYSCALL_NOERROR(name);					\
	RET;							\
END(__CONCAT(__sys_,name))


#define	PSEUDO(name)						\
LEAF(__CONCAT(__sys_,name),0);		/* XXX # of args? */	\
	WEAK_ALIAS(__CONCAT(_,name), __CONCAT(__sys_, name));	\
	CALLSYS_ERROR(name);					\
	RET;							\
END(__CONCAT(__sys_,name))
