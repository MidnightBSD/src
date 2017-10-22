/*-
 * Copyright (c) 1994, Sean Eric Fagan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Sean Eric Fagan.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/ptrace.h>
#include <sys/sx.h>
#include <sys/malloc.h>
#include <sys/signalvar.h>

#include <machine/reg.h>

#include <security/audit/audit.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_param.h>

#ifdef COMPAT_FREEBSD32
#include <sys/procfs.h>
#include <compat/freebsd32/freebsd32_signal.h>

struct ptrace_io_desc32 {
	int		piod_op;
	uint32_t	piod_offs;
	uint32_t	piod_addr;
	uint32_t	piod_len;
};

struct ptrace_vm_entry32 {
	int		pve_entry;
	int		pve_timestamp;
	uint32_t	pve_start;
	uint32_t	pve_end;
	uint32_t	pve_offset;
	u_int		pve_prot;
	u_int		pve_pathlen;
	int32_t		pve_fileid;
	u_int		pve_fsid;
	uint32_t	pve_path;
};

struct ptrace_lwpinfo32 {
	lwpid_t	pl_lwpid;	/* LWP described. */
	int	pl_event;	/* Event that stopped the LWP. */
	int	pl_flags;	/* LWP flags. */
	sigset_t	pl_sigmask;	/* LWP signal mask */
	sigset_t	pl_siglist;	/* LWP pending signal */
	struct siginfo32 pl_siginfo;	/* siginfo for signal */
	char	pl_tdname[MAXCOMLEN + 1];	/* LWP name. */
	int	pl_child_pid;		/* New child pid */
};

#endif

/*
 * Functions implemented using PROC_ACTION():
 *
 * proc_read_regs(proc, regs)
 *	Get the current user-visible register set from the process
 *	and copy it into the regs structure (<machine/reg.h>).
 *	The process is stopped at the time read_regs is called.
 *
 * proc_write_regs(proc, regs)
 *	Update the current register set from the passed in regs
 *	structure.  Take care to avoid clobbering special CPU
 *	registers or privileged bits in the PSL.
 *	Depending on the architecture this may have fix-up work to do,
 *	especially if the IAR or PCW are modified.
 *	The process is stopped at the time write_regs is called.
 *
 * proc_read_fpregs, proc_write_fpregs
 *	deal with the floating point register set, otherwise as above.
 *
 * proc_read_dbregs, proc_write_dbregs
 *	deal with the processor debug register set, otherwise as above.
 *
 * proc_sstep(proc)
 *	Arrange for the process to trap after executing a single instruction.
 */

#define	PROC_ACTION(action) do {					\
	int error;							\
									\
	PROC_LOCK_ASSERT(td->td_proc, MA_OWNED);			\
	if ((td->td_proc->p_flag & P_INMEM) == 0)			\
		error = EIO;						\
	else								\
		error = (action);					\
	return (error);							\
} while(0)

int
proc_read_regs(struct thread *td, struct reg *regs)
{

	PROC_ACTION(fill_regs(td, regs));
}

int
proc_write_regs(struct thread *td, struct reg *regs)
{

	PROC_ACTION(set_regs(td, regs));
}

int
proc_read_dbregs(struct thread *td, struct dbreg *dbregs)
{

	PROC_ACTION(fill_dbregs(td, dbregs));
}

int
proc_write_dbregs(struct thread *td, struct dbreg *dbregs)
{

	PROC_ACTION(set_dbregs(td, dbregs));
}

/*
 * Ptrace doesn't support fpregs at all, and there are no security holes
 * or translations for fpregs, so we can just copy them.
 */
int
proc_read_fpregs(struct thread *td, struct fpreg *fpregs)
{

	PROC_ACTION(fill_fpregs(td, fpregs));
}

int
proc_write_fpregs(struct thread *td, struct fpreg *fpregs)
{

	PROC_ACTION(set_fpregs(td, fpregs));
}

#ifdef COMPAT_FREEBSD32
/* For 32 bit binaries, we need to expose the 32 bit regs layouts. */
int
proc_read_regs32(struct thread *td, struct reg32 *regs32)
{

	PROC_ACTION(fill_regs32(td, regs32));
}

int
proc_write_regs32(struct thread *td, struct reg32 *regs32)
{

	PROC_ACTION(set_regs32(td, regs32));
}

int
proc_read_dbregs32(struct thread *td, struct dbreg32 *dbregs32)
{

	PROC_ACTION(fill_dbregs32(td, dbregs32));
}

int
proc_write_dbregs32(struct thread *td, struct dbreg32 *dbregs32)
{

	PROC_ACTION(set_dbregs32(td, dbregs32));
}

int
proc_read_fpregs32(struct thread *td, struct fpreg32 *fpregs32)
{

	PROC_ACTION(fill_fpregs32(td, fpregs32));
}

int
proc_write_fpregs32(struct thread *td, struct fpreg32 *fpregs32)
{

	PROC_ACTION(set_fpregs32(td, fpregs32));
}
#endif

int
proc_sstep(struct thread *td)
{

	PROC_ACTION(ptrace_single_step(td));
}

int
proc_rwmem(struct proc *p, struct uio *uio)
{
	vm_map_t map;
	vm_offset_t pageno;		/* page number */
	vm_prot_t reqprot;
	int error, fault_flags, page_offset, writing;

	/*
	 * Assert that someone has locked this vmspace.  (Should be
	 * curthread but we can't assert that.)  This keeps the process
	 * from exiting out from under us until this operation completes.
	 */
	KASSERT(p->p_lock >= 1, ("%s: process %p (pid %d) not held", __func__,
	    p, p->p_pid));

	/*
	 * The map we want...
	 */
	map = &p->p_vmspace->vm_map;

	/*
	 * If we are writing, then we request vm_fault() to create a private
	 * copy of each page.  Since these copies will not be writeable by the
	 * process, we must explicity request that they be dirtied.
	 */
	writing = uio->uio_rw == UIO_WRITE;
	reqprot = writing ? VM_PROT_COPY | VM_PROT_READ : VM_PROT_READ;
	fault_flags = writing ? VM_FAULT_DIRTY : VM_FAULT_NORMAL;

	/*
	 * Only map in one page at a time.  We don't have to, but it
	 * makes things easier.  This way is trivial - right?
	 */
	do {
		vm_offset_t uva;
		u_int len;
		vm_page_t m;

		uva = (vm_offset_t)uio->uio_offset;

		/*
		 * Get the page number of this segment.
		 */
		pageno = trunc_page(uva);
		page_offset = uva - pageno;

		/*
		 * How many bytes to copy
		 */
		len = min(PAGE_SIZE - page_offset, uio->uio_resid);

		/*
		 * Fault and hold the page on behalf of the process.
		 */
		error = vm_fault_hold(map, pageno, reqprot, fault_flags, &m);
		if (error != KERN_SUCCESS) {
			if (error == KERN_RESOURCE_SHORTAGE)
				error = ENOMEM;
			else
				error = EFAULT;
			break;
		}

		/*
		 * Now do the i/o move.
		 */
		error = uiomove_fromphys(&m, page_offset, len, uio);

		/* Make the I-cache coherent for breakpoints. */
		if (writing && error == 0) {
			vm_map_lock_read(map);
			if (vm_map_check_protection(map, pageno, pageno +
			    PAGE_SIZE, VM_PROT_EXECUTE))
				vm_sync_icache(map, uva, len);
			vm_map_unlock_read(map);
		}

		/*
		 * Release the page.
		 */
		vm_page_lock(m);
		vm_page_unhold(m);
		vm_page_unlock(m);

	} while (error == 0 && uio->uio_resid > 0);

	return (error);
}

static int
ptrace_vm_entry(struct thread *td, struct proc *p, struct ptrace_vm_entry *pve)
{
	struct vattr vattr;
	vm_map_t map;
	vm_map_entry_t entry;
	vm_object_t obj, tobj, lobj;
	struct vmspace *vm;
	struct vnode *vp;
	char *freepath, *fullpath;
	u_int pathlen;
	int error, index, vfslocked;

	error = 0;
	obj = NULL;

	vm = vmspace_acquire_ref(p);
	map = &vm->vm_map;
	vm_map_lock_read(map);

	do {
		entry = map->header.next;
		index = 0;
		while (index < pve->pve_entry && entry != &map->header) {
			entry = entry->next;
			index++;
		}
		if (index != pve->pve_entry) {
			error = EINVAL;
			break;
		}
		while (entry != &map->header &&
		    (entry->eflags & MAP_ENTRY_IS_SUB_MAP) != 0) {
			entry = entry->next;
			index++;
		}
		if (entry == &map->header) {
			error = ENOENT;
			break;
		}

		/* We got an entry. */
		pve->pve_entry = index + 1;
		pve->pve_timestamp = map->timestamp;
		pve->pve_start = entry->start;
		pve->pve_end = entry->end - 1;
		pve->pve_offset = entry->offset;
		pve->pve_prot = entry->protection;

		/* Backing object's path needed? */
		if (pve->pve_pathlen == 0)
			break;

		pathlen = pve->pve_pathlen;
		pve->pve_pathlen = 0;

		obj = entry->object.vm_object;
		if (obj != NULL)
			VM_OBJECT_LOCK(obj);
	} while (0);

	vm_map_unlock_read(map);
	vmspace_free(vm);

	pve->pve_fsid = VNOVAL;
	pve->pve_fileid = VNOVAL;

	if (error == 0 && obj != NULL) {
		lobj = obj;
		for (tobj = obj; tobj != NULL; tobj = tobj->backing_object) {
			if (tobj != obj)
				VM_OBJECT_LOCK(tobj);
			if (lobj != obj)
				VM_OBJECT_UNLOCK(lobj);
			lobj = tobj;
			pve->pve_offset += tobj->backing_object_offset;
		}
		vp = (lobj->type == OBJT_VNODE) ? lobj->handle : NULL;
		if (vp != NULL)
			vref(vp);
		if (lobj != obj)
			VM_OBJECT_UNLOCK(lobj);
		VM_OBJECT_UNLOCK(obj);

		if (vp != NULL) {
			freepath = NULL;
			fullpath = NULL;
			vn_fullpath(td, vp, &fullpath, &freepath);
			vfslocked = VFS_LOCK_GIANT(vp->v_mount);
			vn_lock(vp, LK_SHARED | LK_RETRY);
			if (VOP_GETATTR(vp, &vattr, td->td_ucred) == 0) {
				pve->pve_fileid = vattr.va_fileid;
				pve->pve_fsid = vattr.va_fsid;
			}
			vput(vp);
			VFS_UNLOCK_GIANT(vfslocked);

			if (fullpath != NULL) {
				pve->pve_pathlen = strlen(fullpath) + 1;
				if (pve->pve_pathlen <= pathlen) {
					error = copyout(fullpath, pve->pve_path,
					    pve->pve_pathlen);
				} else
					error = ENAMETOOLONG;
			}
			if (freepath != NULL)
				free(freepath, M_TEMP);
		}
	}

	return (error);
}

#ifdef COMPAT_FREEBSD32
static int      
ptrace_vm_entry32(struct thread *td, struct proc *p,
    struct ptrace_vm_entry32 *pve32)
{
	struct ptrace_vm_entry pve;
	int error;

	pve.pve_entry = pve32->pve_entry;
	pve.pve_pathlen = pve32->pve_pathlen;
	pve.pve_path = (void *)(uintptr_t)pve32->pve_path;

	error = ptrace_vm_entry(td, p, &pve);
	if (error == 0) {
		pve32->pve_entry = pve.pve_entry;
		pve32->pve_timestamp = pve.pve_timestamp;
		pve32->pve_start = pve.pve_start;
		pve32->pve_end = pve.pve_end;
		pve32->pve_offset = pve.pve_offset;
		pve32->pve_prot = pve.pve_prot;
		pve32->pve_fileid = pve.pve_fileid;
		pve32->pve_fsid = pve.pve_fsid;
	}

	pve32->pve_pathlen = pve.pve_pathlen;
	return (error);
}

static void
ptrace_lwpinfo_to32(const struct ptrace_lwpinfo *pl,
    struct ptrace_lwpinfo32 *pl32)
{

	pl32->pl_lwpid = pl->pl_lwpid;
	pl32->pl_event = pl->pl_event;
	pl32->pl_flags = pl->pl_flags;
	pl32->pl_sigmask = pl->pl_sigmask;
	pl32->pl_siglist = pl->pl_siglist;
	siginfo_to_siginfo32(&pl->pl_siginfo, &pl32->pl_siginfo);
	strcpy(pl32->pl_tdname, pl->pl_tdname);
	pl32->pl_child_pid = pl->pl_child_pid;
}
#endif /* COMPAT_FREEBSD32 */

/*
 * Process debugging system call.
 */
#ifndef _SYS_SYSPROTO_H_
struct ptrace_args {
	int	req;
	pid_t	pid;
	caddr_t	addr;
	int	data;
};
#endif

#ifdef COMPAT_FREEBSD32
/*
 * This CPP subterfuge is to try and reduce the number of ifdefs in
 * the body of the code.
 *   COPYIN(uap->addr, &r.reg, sizeof r.reg);
 * becomes either:
 *   copyin(uap->addr, &r.reg, sizeof r.reg);
 * or
 *   copyin(uap->addr, &r.reg32, sizeof r.reg32);
 * .. except this is done at runtime.
 */
#define	COPYIN(u, k, s)		wrap32 ? \
	copyin(u, k ## 32, s ## 32) : \
	copyin(u, k, s)
#define	COPYOUT(k, u, s)	wrap32 ? \
	copyout(k ## 32, u, s ## 32) : \
	copyout(k, u, s)
#else
#define	COPYIN(u, k, s)		copyin(u, k, s)
#define	COPYOUT(k, u, s)	copyout(k, u, s)
#endif
int
sys_ptrace(struct thread *td, struct ptrace_args *uap)
{
	/*
	 * XXX this obfuscation is to reduce stack usage, but the register
	 * structs may be too large to put on the stack anyway.
	 */
	union {
		struct ptrace_io_desc piod;
		struct ptrace_lwpinfo pl;
		struct ptrace_vm_entry pve;
		struct dbreg dbreg;
		struct fpreg fpreg;
		struct reg reg;
#ifdef COMPAT_FREEBSD32
		struct dbreg32 dbreg32;
		struct fpreg32 fpreg32;
		struct reg32 reg32;
		struct ptrace_io_desc32 piod32;
		struct ptrace_lwpinfo32 pl32;
		struct ptrace_vm_entry32 pve32;
#endif
	} r;
	void *addr;
	int error = 0;
#ifdef COMPAT_FREEBSD32
	int wrap32 = 0;

	if (SV_CURPROC_FLAG(SV_ILP32))
		wrap32 = 1;
#endif
	AUDIT_ARG_PID(uap->pid);
	AUDIT_ARG_CMD(uap->req);
	AUDIT_ARG_VALUE(uap->data);
	addr = &r;
	switch (uap->req) {
	case PT_GETREGS:
	case PT_GETFPREGS:
	case PT_GETDBREGS:
	case PT_LWPINFO:
		break;
	case PT_SETREGS:
		error = COPYIN(uap->addr, &r.reg, sizeof r.reg);
		break;
	case PT_SETFPREGS:
		error = COPYIN(uap->addr, &r.fpreg, sizeof r.fpreg);
		break;
	case PT_SETDBREGS:
		error = COPYIN(uap->addr, &r.dbreg, sizeof r.dbreg);
		break;
	case PT_IO:
		error = COPYIN(uap->addr, &r.piod, sizeof r.piod);
		break;
	case PT_VM_ENTRY:
		error = COPYIN(uap->addr, &r.pve, sizeof r.pve);
		break;
	default:
		addr = uap->addr;
		break;
	}
	if (error)
		return (error);

	error = kern_ptrace(td, uap->req, uap->pid, addr, uap->data);
	if (error)
		return (error);

	switch (uap->req) {
	case PT_VM_ENTRY:
		error = COPYOUT(&r.pve, uap->addr, sizeof r.pve);
		break;
	case PT_IO:
		error = COPYOUT(&r.piod, uap->addr, sizeof r.piod);
		break;
	case PT_GETREGS:
		error = COPYOUT(&r.reg, uap->addr, sizeof r.reg);
		break;
	case PT_GETFPREGS:
		error = COPYOUT(&r.fpreg, uap->addr, sizeof r.fpreg);
		break;
	case PT_GETDBREGS:
		error = COPYOUT(&r.dbreg, uap->addr, sizeof r.dbreg);
		break;
	case PT_LWPINFO:
		error = copyout(&r.pl, uap->addr, uap->data);
		break;
	}

	return (error);
}
#undef COPYIN
#undef COPYOUT

#ifdef COMPAT_FREEBSD32
/*
 *   PROC_READ(regs, td2, addr);
 * becomes either:
 *   proc_read_regs(td2, addr);
 * or
 *   proc_read_regs32(td2, addr);
 * .. except this is done at runtime.  There is an additional
 * complication in that PROC_WRITE disallows 32 bit consumers
 * from writing to 64 bit address space targets.
 */
#define	PROC_READ(w, t, a)	wrap32 ? \
	proc_read_ ## w ## 32(t, a) : \
	proc_read_ ## w (t, a)
#define	PROC_WRITE(w, t, a)	wrap32 ? \
	(safe ? proc_write_ ## w ## 32(t, a) : EINVAL ) : \
	proc_write_ ## w (t, a)
#else
#define	PROC_READ(w, t, a)	proc_read_ ## w (t, a)
#define	PROC_WRITE(w, t, a)	proc_write_ ## w (t, a)
#endif

int
kern_ptrace(struct thread *td, int req, pid_t pid, void *addr, int data)
{
	struct iovec iov;
	struct uio uio;
	struct proc *curp, *p, *pp;
	struct thread *td2 = NULL;
	struct ptrace_io_desc *piod = NULL;
	struct ptrace_lwpinfo *pl;
	int error, write, tmp, num;
	int proctree_locked = 0;
	lwpid_t tid = 0, *buf;
#ifdef COMPAT_FREEBSD32
	int wrap32 = 0, safe = 0;
	struct ptrace_io_desc32 *piod32 = NULL;
	struct ptrace_lwpinfo32 *pl32 = NULL;
	struct ptrace_lwpinfo plr;
#endif

	curp = td->td_proc;

	/* Lock proctree before locking the process. */
	switch (req) {
	case PT_TRACE_ME:
	case PT_ATTACH:
	case PT_STEP:
	case PT_CONTINUE:
	case PT_TO_SCE:
	case PT_TO_SCX:
	case PT_SYSCALL:
	case PT_FOLLOW_FORK:
	case PT_DETACH:
		sx_xlock(&proctree_lock);
		proctree_locked = 1;
		break;
	default:
		break;
	}

	write = 0;
	if (req == PT_TRACE_ME) {
		p = td->td_proc;
		PROC_LOCK(p);
	} else {
		if (pid <= PID_MAX) {
			if ((p = pfind(pid)) == NULL) {
				if (proctree_locked)
					sx_xunlock(&proctree_lock);
				return (ESRCH);
			}
		} else {
			td2 = tdfind(pid, -1);
			if (td2 == NULL) {
				if (proctree_locked)
					sx_xunlock(&proctree_lock);
				return (ESRCH);
			}
			p = td2->td_proc;
			tid = pid;
			pid = p->p_pid;
		}
	}
	AUDIT_ARG_PROCESS(p);

	if ((p->p_flag & P_WEXIT) != 0) {
		error = ESRCH;
		goto fail;
	}
	if ((error = p_cansee(td, p)) != 0)
		goto fail;

	if ((error = p_candebug(td, p)) != 0)
		goto fail;

	/*
	 * System processes can't be debugged.
	 */
	if ((p->p_flag & P_SYSTEM) != 0) {
		error = EINVAL;
		goto fail;
	}

	if (tid == 0) {
		if ((p->p_flag & P_STOPPED_TRACE) != 0) {
			KASSERT(p->p_xthread != NULL, ("NULL p_xthread"));
			td2 = p->p_xthread;
		} else {
			td2 = FIRST_THREAD_IN_PROC(p);
		}
		tid = td2->td_tid;
	}

#ifdef COMPAT_FREEBSD32
	/*
	 * Test if we're a 32 bit client and what the target is.
	 * Set the wrap controls accordingly.
	 */
	if (SV_CURPROC_FLAG(SV_ILP32)) {
		if (SV_PROC_FLAG(td2->td_proc, SV_ILP32))
			safe = 1;
		wrap32 = 1;
	}
#endif
	/*
	 * Permissions check
	 */
	switch (req) {
	case PT_TRACE_ME:
		/* Always legal. */
		break;

	case PT_ATTACH:
		/* Self */
		if (p->p_pid == td->td_proc->p_pid) {
			error = EINVAL;
			goto fail;
		}

		/* Already traced */
		if (p->p_flag & P_TRACED) {
			error = EBUSY;
			goto fail;
		}

		/* Can't trace an ancestor if you're being traced. */
		if (curp->p_flag & P_TRACED) {
			for (pp = curp->p_pptr; pp != NULL; pp = pp->p_pptr) {
				if (pp == p) {
					error = EINVAL;
					goto fail;
				}
			}
		}


		/* OK */
		break;

	case PT_CLEARSTEP:
		/* Allow thread to clear single step for itself */
		if (td->td_tid == tid)
			break;

		/* FALLTHROUGH */
	default:
		/* not being traced... */
		if ((p->p_flag & P_TRACED) == 0) {
			error = EPERM;
			goto fail;
		}

		/* not being traced by YOU */
		if (p->p_pptr != td->td_proc) {
			error = EBUSY;
			goto fail;
		}

		/* not currently stopped */
		if ((p->p_flag & (P_STOPPED_SIG | P_STOPPED_TRACE)) == 0 ||
		    p->p_suspcount != p->p_numthreads  ||
		    (p->p_flag & P_WAITED) == 0) {
			error = EBUSY;
			goto fail;
		}

		if ((p->p_flag & P_STOPPED_TRACE) == 0) {
			static int count = 0;
			if (count++ == 0)
				printf("P_STOPPED_TRACE not set.\n");
		}

		/* OK */
		break;
	}

	/* Keep this process around until we finish this request. */
	_PHOLD(p);

#ifdef FIX_SSTEP
	/*
	 * Single step fixup ala procfs
	 */
	FIX_SSTEP(td2);
#endif

	/*
	 * Actually do the requests
	 */

	td->td_retval[0] = 0;

	switch (req) {
	case PT_TRACE_ME:
		/* set my trace flag and "owner" so it can read/write me */
		p->p_flag |= P_TRACED;
		p->p_oppid = p->p_pptr->p_pid;
		break;

	case PT_ATTACH:
		/* security check done above */
		/*
		 * It would be nice if the tracing relationship was separate
		 * from the parent relationship but that would require
		 * another set of links in the proc struct or for "wait"
		 * to scan the entire proc table.  To make life easier,
		 * we just re-parent the process we're trying to trace.
		 * The old parent is remembered so we can put things back
		 * on a "detach".
		 */
		p->p_flag |= P_TRACED;
		p->p_oppid = p->p_pptr->p_pid;
		if (p->p_pptr != td->td_proc) {
			proc_reparent(p, td->td_proc);
		}
		data = SIGSTOP;
		goto sendsig;	/* in PT_CONTINUE below */

	case PT_CLEARSTEP:
		error = ptrace_clear_single_step(td2);
		break;

	case PT_SETSTEP:
		error = ptrace_single_step(td2);
		break;

	case PT_SUSPEND:
		td2->td_dbgflags |= TDB_SUSPEND;
		thread_lock(td2);
		td2->td_flags |= TDF_NEEDSUSPCHK;
		thread_unlock(td2);
		break;

	case PT_RESUME:
		td2->td_dbgflags &= ~TDB_SUSPEND;
		break;

	case PT_FOLLOW_FORK:
		if (data)
			p->p_flag |= P_FOLLOWFORK;
		else
			p->p_flag &= ~P_FOLLOWFORK;
		break;

	case PT_STEP:
	case PT_CONTINUE:
	case PT_TO_SCE:
	case PT_TO_SCX:
	case PT_SYSCALL:
	case PT_DETACH:
		/* Zero means do not send any signal */
		if (data < 0 || data > _SIG_MAXSIG) {
			error = EINVAL;
			break;
		}

		switch (req) {
		case PT_STEP:
			error = ptrace_single_step(td2);
			if (error)
				goto out;
			break;
		case PT_CONTINUE:
		case PT_TO_SCE:
		case PT_TO_SCX:
		case PT_SYSCALL:
			if (addr != (void *)1) {
				error = ptrace_set_pc(td2,
				    (u_long)(uintfptr_t)addr);
				if (error)
					goto out;
			}
			switch (req) {
			case PT_TO_SCE:
				p->p_stops |= S_PT_SCE;
				break;
			case PT_TO_SCX:
				p->p_stops |= S_PT_SCX;
				break;
			case PT_SYSCALL:
				p->p_stops |= S_PT_SCE | S_PT_SCX;
				break;
			}
			break;
		case PT_DETACH:
			/* reset process parent */
			if (p->p_oppid != p->p_pptr->p_pid) {
				struct proc *pp;

				PROC_LOCK(p->p_pptr);
				sigqueue_take(p->p_ksi);
				PROC_UNLOCK(p->p_pptr);

				PROC_UNLOCK(p);
				pp = pfind(p->p_oppid);
				if (pp == NULL)
					pp = initproc;
				else
					PROC_UNLOCK(pp);
				PROC_LOCK(p);
				proc_reparent(p, pp);
				if (pp == initproc)
					p->p_sigparent = SIGCHLD;
			}
			p->p_oppid = 0;
			p->p_flag &= ~(P_TRACED | P_WAITED | P_FOLLOWFORK);

			/* should we send SIGCHLD? */
			/* childproc_continued(p); */
			break;
		}

	sendsig:
		if (proctree_locked) {
			sx_xunlock(&proctree_lock);
			proctree_locked = 0;
		}
		p->p_xstat = data;
		p->p_xthread = NULL;
		if ((p->p_flag & (P_STOPPED_SIG | P_STOPPED_TRACE)) != 0) {
			/* deliver or queue signal */
			td2->td_dbgflags &= ~TDB_XSIG;
			td2->td_xsig = data;

			if (req == PT_DETACH) {
				struct thread *td3;
				FOREACH_THREAD_IN_PROC(p, td3) {
					td3->td_dbgflags &= ~TDB_SUSPEND; 
				}
			}
			/*
			 * unsuspend all threads, to not let a thread run,
			 * you should use PT_SUSPEND to suspend it before
			 * continuing process.
			 */
			PROC_SLOCK(p);
			p->p_flag &= ~(P_STOPPED_TRACE|P_STOPPED_SIG|P_WAITED);
			thread_unsuspend(p);
			PROC_SUNLOCK(p);
		} else {
			if (data)
				kern_psignal(p, data);
		}
		break;

	case PT_WRITE_I:
	case PT_WRITE_D:
		td2->td_dbgflags |= TDB_USERWR;
		write = 1;
		/* FALLTHROUGH */
	case PT_READ_I:
	case PT_READ_D:
		PROC_UNLOCK(p);
		tmp = 0;
		/* write = 0 set above */
		iov.iov_base = write ? (caddr_t)&data : (caddr_t)&tmp;
		iov.iov_len = sizeof(int);
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = (off_t)(uintptr_t)addr;
		uio.uio_resid = sizeof(int);
		uio.uio_segflg = UIO_SYSSPACE;	/* i.e.: the uap */
		uio.uio_rw = write ? UIO_WRITE : UIO_READ;
		uio.uio_td = td;
		error = proc_rwmem(p, &uio);
		if (uio.uio_resid != 0) {
			/*
			 * XXX proc_rwmem() doesn't currently return ENOSPC,
			 * so I think write() can bogusly return 0.
			 * XXX what happens for short writes?  We don't want
			 * to write partial data.
			 * XXX proc_rwmem() returns EPERM for other invalid
			 * addresses.  Convert this to EINVAL.  Does this
			 * clobber returns of EPERM for other reasons?
			 */
			if (error == 0 || error == ENOSPC || error == EPERM)
				error = EINVAL;	/* EOF */
		}
		if (!write)
			td->td_retval[0] = tmp;
		PROC_LOCK(p);
		break;

	case PT_IO:
#ifdef COMPAT_FREEBSD32
		if (wrap32) {
			piod32 = addr;
			iov.iov_base = (void *)(uintptr_t)piod32->piod_addr;
			iov.iov_len = piod32->piod_len;
			uio.uio_offset = (off_t)(uintptr_t)piod32->piod_offs;
			uio.uio_resid = piod32->piod_len;
		} else
#endif
		{
			piod = addr;
			iov.iov_base = piod->piod_addr;
			iov.iov_len = piod->piod_len;
			uio.uio_offset = (off_t)(uintptr_t)piod->piod_offs;
			uio.uio_resid = piod->piod_len;
		}
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_segflg = UIO_USERSPACE;
		uio.uio_td = td;
#ifdef COMPAT_FREEBSD32
		tmp = wrap32 ? piod32->piod_op : piod->piod_op;
#else
		tmp = piod->piod_op;
#endif
		switch (tmp) {
		case PIOD_READ_D:
		case PIOD_READ_I:
			uio.uio_rw = UIO_READ;
			break;
		case PIOD_WRITE_D:
		case PIOD_WRITE_I:
			td2->td_dbgflags |= TDB_USERWR;
			uio.uio_rw = UIO_WRITE;
			break;
		default:
			error = EINVAL;
			goto out;
		}
		PROC_UNLOCK(p);
		error = proc_rwmem(p, &uio);
#ifdef COMPAT_FREEBSD32
		if (wrap32)
			piod32->piod_len -= uio.uio_resid;
		else
#endif
			piod->piod_len -= uio.uio_resid;
		PROC_LOCK(p);
		break;

	case PT_KILL:
		data = SIGKILL;
		goto sendsig;	/* in PT_CONTINUE above */

	case PT_SETREGS:
		td2->td_dbgflags |= TDB_USERWR;
		error = PROC_WRITE(regs, td2, addr);
		break;

	case PT_GETREGS:
		error = PROC_READ(regs, td2, addr);
		break;

	case PT_SETFPREGS:
		td2->td_dbgflags |= TDB_USERWR;
		error = PROC_WRITE(fpregs, td2, addr);
		break;

	case PT_GETFPREGS:
		error = PROC_READ(fpregs, td2, addr);
		break;

	case PT_SETDBREGS:
		td2->td_dbgflags |= TDB_USERWR;
		error = PROC_WRITE(dbregs, td2, addr);
		break;

	case PT_GETDBREGS:
		error = PROC_READ(dbregs, td2, addr);
		break;

	case PT_LWPINFO:
		if (data <= 0 ||
#ifdef COMPAT_FREEBSD32
		    (!wrap32 && data > sizeof(*pl)) ||
		    (wrap32 && data > sizeof(*pl32))) {
#else
		    data > sizeof(*pl)) {
#endif
			error = EINVAL;
			break;
		}
#ifdef COMPAT_FREEBSD32
		if (wrap32) {
			pl = &plr;
			pl32 = addr;
		} else
#endif
		pl = addr;
		pl->pl_lwpid = td2->td_tid;
		pl->pl_flags = 0;
		if (td2->td_dbgflags & TDB_XSIG) {
			pl->pl_event = PL_EVENT_SIGNAL;
			if (td2->td_dbgksi.ksi_signo != 0 &&
#ifdef COMPAT_FREEBSD32
			    ((!wrap32 && data >= offsetof(struct ptrace_lwpinfo,
			    pl_siginfo) + sizeof(pl->pl_siginfo)) ||
			    (wrap32 && data >= offsetof(struct ptrace_lwpinfo32,
			    pl_siginfo) + sizeof(struct siginfo32)))
#else
			    data >= offsetof(struct ptrace_lwpinfo, pl_siginfo)
			    + sizeof(pl->pl_siginfo)
#endif
			){
				pl->pl_flags |= PL_FLAG_SI;
				pl->pl_siginfo = td2->td_dbgksi.ksi_info;
			}
		}
		if ((pl->pl_flags & PL_FLAG_SI) == 0)
			bzero(&pl->pl_siginfo, sizeof(pl->pl_siginfo));
		if (td2->td_dbgflags & TDB_SCE)
			pl->pl_flags |= PL_FLAG_SCE;
		else if (td2->td_dbgflags & TDB_SCX)
			pl->pl_flags |= PL_FLAG_SCX;
		if (td2->td_dbgflags & TDB_EXEC)
			pl->pl_flags |= PL_FLAG_EXEC;
		if (td2->td_dbgflags & TDB_FORK) {
			pl->pl_flags |= PL_FLAG_FORKED;
			pl->pl_child_pid = td2->td_dbg_forked;
		}
		if (td2->td_dbgflags & TDB_CHILD)
			pl->pl_flags |= PL_FLAG_CHILD;
		pl->pl_sigmask = td2->td_sigmask;
		pl->pl_siglist = td2->td_siglist;
		strcpy(pl->pl_tdname, td2->td_name);
#ifdef COMPAT_FREEBSD32
		if (wrap32)
			ptrace_lwpinfo_to32(pl, pl32);
#endif
		break;

	case PT_GETNUMLWPS:
		td->td_retval[0] = p->p_numthreads;
		break;

	case PT_GETLWPLIST:
		if (data <= 0) {
			error = EINVAL;
			break;
		}
		num = imin(p->p_numthreads, data);
		PROC_UNLOCK(p);
		buf = malloc(num * sizeof(lwpid_t), M_TEMP, M_WAITOK);
		tmp = 0;
		PROC_LOCK(p);
		FOREACH_THREAD_IN_PROC(p, td2) {
			if (tmp >= num)
				break;
			buf[tmp++] = td2->td_tid;
		}
		PROC_UNLOCK(p);
		error = copyout(buf, addr, tmp * sizeof(lwpid_t));
		free(buf, M_TEMP);
		if (!error)
			td->td_retval[0] = tmp;
		PROC_LOCK(p);
		break;

	case PT_VM_TIMESTAMP:
		td->td_retval[0] = p->p_vmspace->vm_map.timestamp;
		break;

	case PT_VM_ENTRY:
		PROC_UNLOCK(p);
#ifdef COMPAT_FREEBSD32
		if (wrap32)
			error = ptrace_vm_entry32(td, p, addr);
		else
#endif
		error = ptrace_vm_entry(td, p, addr);
		PROC_LOCK(p);
		break;

	default:
#ifdef __HAVE_PTRACE_MACHDEP
		if (req >= PT_FIRSTMACH) {
			PROC_UNLOCK(p);
			error = cpu_ptrace(td2, req, addr, data);
			PROC_LOCK(p);
		} else
#endif
			/* Unknown request. */
			error = EINVAL;
		break;
	}

out:
	/* Drop our hold on this process now that the request has completed. */
	_PRELE(p);
fail:
	PROC_UNLOCK(p);
	if (proctree_locked)
		sx_xunlock(&proctree_lock);
	return (error);
}
#undef PROC_READ
#undef PROC_WRITE

/*
 * Stop a process because of a debugging event;
 * stay stopped until p->p_step is cleared
 * (cleared by PIOCCONT in procfs).
 */
void
stopevent(struct proc *p, unsigned int event, unsigned int val)
{

	PROC_LOCK_ASSERT(p, MA_OWNED);
	p->p_step = 1;
	do {
		p->p_xstat = val;
		p->p_xthread = NULL;
		p->p_stype = event;	/* Which event caused the stop? */
		wakeup(&p->p_stype);	/* Wake up any PIOCWAIT'ing procs */
		msleep(&p->p_step, &p->p_mtx, PWAIT, "stopevent", 0);
	} while (p->p_step);
}
