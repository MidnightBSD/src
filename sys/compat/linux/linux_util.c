/*-
 * Copyright (c) 1994 Christos Zoulas
 * Copyright (c) 1995 Frank van der Linden
 * Copyright (c) 1995 Scott Bartram
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	from: svr4_util.c,v 1.5 1995/01/22 23:44:50 christos Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/compat/linux/linux_util.c,v 1.29 2005/03/01 17:57:45 jhb Exp $");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/syscallsubr.h>
#include <sys/systm.h>
#include <sys/vnode.h>

#include <machine/stdarg.h>

#include <compat/linux/linux_util.h>

const char      linux_emul_path[] = "/compat/linux";

/*
 * Search an alternate path before passing pathname arguments on
 * to system calls. Useful for keeping a separate 'emulation tree'.
 *
 * If cflag is set, we check if an attempt can be made to create
 * the named file, i.e. we check if the directory it should
 * be in exists.
 */
int
linux_emul_convpath(td, path, pathseg, pbuf, cflag)
	struct thread	 *td;
	char		 *path;
	enum uio_seg	  pathseg;
	char		**pbuf;
	int		  cflag;
{

	return (kern_alternate_path(td, linux_emul_path, path, pathseg, pbuf,
		cflag));
}

void
linux_msg(const struct thread *td, const char *fmt, ...)
{
	va_list ap;
	struct proc *p;

	p = td->td_proc;
	printf("linux: pid %d (%s): ", (int)p->p_pid, p->p_comm);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}
