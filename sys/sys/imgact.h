/*-
 * Copyright (c) 1993, David Greenman
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
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: release/7.0.0/sys/sys/imgact.h 174854 2007-12-22 06:32:46Z cvs2svn $
 */

#ifndef _SYS_IMGACT_H_
#define	_SYS_IMGACT_H_

#include <sys/uio.h>

#define MAXSHELLCMDLEN	PAGE_SIZE

struct image_args {
	char *buf;		/* pointer to string buffer */
	char *begin_argv;	/* beginning of argv in buf */
	char *begin_envv;	/* beginning of envv in buf */
	char *endp;		/* current `end' pointer of arg & env strings */
	char *fname;            /* pointer to filename of executable (system space) */
	int stringspace;	/* space left in arg & env buffer */
	int argc;		/* count of argument strings */
	int envc;		/* count of environment strings */
};

struct image_params {
	struct proc *proc;	/* our process struct */
	struct label *execlabel;	/* optional exec label */
	struct vnode *vp;	/* pointer to vnode of file to exec */
	struct vm_object *object;	/* The vm object for this vp */
	struct vattr *attr;	/* attributes of file */
	const char *image_header; /* head of file to exec */
	unsigned long entry_addr; /* entry address of target executable */
	char vmspace_destroyed;	/* flag - we've blown away original vm space */
	char interpreted;	/* flag - this executable is interpreted */
	char *interpreter_name;	/* name of the interpreter */
	void *auxargs;		/* ELF Auxinfo structure pointer */
	struct sf_buf *firstpage;	/* first page that we mapped */
	unsigned long ps_strings; /* PS_STRINGS for BSD/OS binaries */
	size_t auxarg_size;
	struct image_args *args;	/* system call arguments */
	struct sysentvec *sysent;	/* system entry vector */
};

#ifdef _KERNEL
struct sysentvec;
struct thread;

int	exec_check_permissions(struct image_params *);
register_t *exec_copyout_strings(struct image_params *);
int	exec_new_vmspace(struct image_params *, struct sysentvec *);
void	exec_setregs(struct thread *, u_long, u_long, u_long);
int	exec_shell_imgact(struct image_params *);
int	exec_copyin_args(struct image_args *, char *, enum uio_seg,
	char **, char **);
#endif

#endif /* !_SYS_IMGACT_H_ */
