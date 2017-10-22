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
__FBSDID("$FreeBSD: release/7.0.0/sys/kern/kern_exec.c 175495 2008-01-19 18:15:07Z kib $");

#include "opt_hwpmc_hooks.h"
#include "opt_ktrace.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/filedesc.h>
#include <sys/fcntl.h>
#include <sys/acct.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/wait.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/pioctl.h>
#include <sys/namei.h>
#include <sys/resourcevar.h>
#include <sys/sf_buf.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/shm.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>

#ifdef	HWPMC_HOOKS
#include <sys/pmckern.h>
#endif

#include <machine/reg.h>

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

MALLOC_DEFINE(M_PARGS, "proc-args", "Process arguments");

static int sysctl_kern_ps_strings(SYSCTL_HANDLER_ARGS);
static int sysctl_kern_usrstack(SYSCTL_HANDLER_ARGS);
static int sysctl_kern_stackprot(SYSCTL_HANDLER_ARGS);
static int do_execve(struct thread *td, struct image_args *args,
    struct mac *mac_p);
static void exec_free_args(struct image_args *);

/* XXX This should be vm_size_t. */
SYSCTL_PROC(_kern, KERN_PS_STRINGS, ps_strings, CTLTYPE_ULONG|CTLFLAG_RD,
    NULL, 0, sysctl_kern_ps_strings, "LU", "");

/* XXX This should be vm_size_t. */
SYSCTL_PROC(_kern, KERN_USRSTACK, usrstack, CTLTYPE_ULONG|CTLFLAG_RD,
    NULL, 0, sysctl_kern_usrstack, "LU", "");

SYSCTL_PROC(_kern, OID_AUTO, stackprot, CTLTYPE_INT|CTLFLAG_RD,
    NULL, 0, sysctl_kern_stackprot, "I", "");

u_long ps_arg_cache_limit = PAGE_SIZE / 16;
SYSCTL_ULONG(_kern, OID_AUTO, ps_arg_cache_limit, CTLFLAG_RW, 
    &ps_arg_cache_limit, 0, "");

static int
sysctl_kern_ps_strings(SYSCTL_HANDLER_ARGS)
{
	struct proc *p;
	int error;

	p = curproc;
#ifdef SCTL_MASK32
	if (req->flags & SCTL_MASK32) {
		unsigned int val;
		val = (unsigned int)p->p_sysent->sv_psstrings;
		error = SYSCTL_OUT(req, &val, sizeof(val));
	} else
#endif
		error = SYSCTL_OUT(req, &p->p_sysent->sv_psstrings,
		   sizeof(p->p_sysent->sv_psstrings));
	return error;
}

static int
sysctl_kern_usrstack(SYSCTL_HANDLER_ARGS)
{
	struct proc *p;
	int error;

	p = curproc;
#ifdef SCTL_MASK32
	if (req->flags & SCTL_MASK32) {
		unsigned int val;
		val = (unsigned int)p->p_sysent->sv_usrstack;
		error = SYSCTL_OUT(req, &val, sizeof(val));
	} else
#endif
		error = SYSCTL_OUT(req, &p->p_sysent->sv_usrstack,
		    sizeof(p->p_sysent->sv_usrstack));
	return error;
}

static int
sysctl_kern_stackprot(SYSCTL_HANDLER_ARGS)
{
	struct proc *p;

	p = curproc;
	return (SYSCTL_OUT(req, &p->p_sysent->sv_stackprot,
	    sizeof(p->p_sysent->sv_stackprot)));
}

/*
 * Each of the items is a pointer to a `const struct execsw', hence the
 * double pointer here.
 */
static const struct execsw **execsw;

#ifndef _SYS_SYSPROTO_H_
struct execve_args {
	char    *fname; 
	char    **argv;
	char    **envv; 
};
#endif

int
execve(td, uap)
	struct thread *td;
	struct execve_args /* {
		char *fname;
		char **argv;
		char **envv;
	} */ *uap;
{
	int error;
	struct image_args args;

	error = exec_copyin_args(&args, uap->fname, UIO_USERSPACE,
	    uap->argv, uap->envv);
	if (error == 0)
		error = kern_execve(td, &args, NULL);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct __mac_execve_args {
	char	*fname;
	char	**argv;
	char	**envv;
	struct mac	*mac_p;
};
#endif

int
__mac_execve(td, uap)
	struct thread *td;
	struct __mac_execve_args /* {
		char *fname;
		char **argv;
		char **envv;
		struct mac *mac_p;
	} */ *uap;
{
#ifdef MAC
	int error;
	struct image_args args;

	error = exec_copyin_args(&args, uap->fname, UIO_USERSPACE,
	    uap->argv, uap->envv);
	if (error == 0)
		error = kern_execve(td, &args, uap->mac_p);
	return (error);
#else
	return (ENOSYS);
#endif
}

/*
 * XXX: kern_execve has the astonishing property of not always returning to
 * the caller.  If sufficiently bad things happen during the call to
 * do_execve(), it can end up calling exit1(); as a result, callers must
 * avoid doing anything which they might need to undo (e.g., allocating
 * memory).
 */
int
kern_execve(td, args, mac_p)
	struct thread *td;
	struct image_args *args;
	struct mac *mac_p;
{
	struct proc *p = td->td_proc;
	int error;

	AUDIT_ARG(argv, args->begin_argv, args->argc,
	    args->begin_envv - args->begin_argv);
	AUDIT_ARG(envv, args->begin_envv, args->envc,
	    args->endp - args->begin_envv);
	if (p->p_flag & P_HADTHREADS) {
		PROC_LOCK(p);
		if (thread_single(SINGLE_BOUNDARY)) {
			PROC_UNLOCK(p);
	       		exec_free_args(args);
			return (ERESTART);	/* Try again later. */
		}
		PROC_UNLOCK(p);
	}

	error = do_execve(td, args, mac_p);

	if (p->p_flag & P_HADTHREADS) {
		PROC_LOCK(p);
		/*
		 * If success, we upgrade to SINGLE_EXIT state to
		 * force other threads to suicide.
		 */
		if (error == 0)
			thread_single(SINGLE_EXIT);
		else
			thread_single_end();
		PROC_UNLOCK(p);
	}

	return (error);
}

/*
 * In-kernel implementation of execve().  All arguments are assumed to be
 * userspace pointers from the passed thread.
 */
static int
do_execve(td, args, mac_p)
	struct thread *td;
	struct image_args *args;
	struct mac *mac_p;
{
	struct proc *p = td->td_proc;
	struct nameidata nd, *ndp;
	struct ucred *newcred = NULL, *oldcred;
	struct uidinfo *euip;
	register_t *stack_base;
	int error, len, i;
	struct image_params image_params, *imgp;
	struct vattr attr;
	int (*img_first)(struct image_params *);
	struct pargs *oldargs = NULL, *newargs = NULL;
	struct sigacts *oldsigacts, *newsigacts;
#ifdef KTRACE
	struct vnode *tracevp = NULL;
	struct ucred *tracecred = NULL;
#endif
	struct vnode *textvp = NULL;
	int credential_changing;
	int vfslocked;
	int textset;
#ifdef MAC
	struct label *interplabel = NULL;
	int will_transition;
#endif
#ifdef HWPMC_HOOKS
	struct pmckern_procexec pe;
#endif

	vfslocked = 0;
	imgp = &image_params;

	/*
	 * Lock the process and set the P_INEXEC flag to indicate that
	 * it should be left alone until we're done here.  This is
	 * necessary to avoid race conditions - e.g. in ptrace() -
	 * that might allow a local user to illicitly obtain elevated
	 * privileges.
	 */
	PROC_LOCK(p);
	KASSERT((p->p_flag & P_INEXEC) == 0,
	    ("%s(): process already has P_INEXEC flag", __func__));
	p->p_flag |= P_INEXEC;
	PROC_UNLOCK(p);

	/*
	 * Initialize part of the common data
	 */
	imgp->proc = p;
	imgp->execlabel = NULL;
	imgp->attr = &attr;
	imgp->entry_addr = 0;
	imgp->vmspace_destroyed = 0;
	imgp->interpreted = 0;
	imgp->interpreter_name = args->buf + PATH_MAX + ARG_MAX;
	imgp->auxargs = NULL;
	imgp->vp = NULL;
	imgp->object = NULL;
	imgp->firstpage = NULL;
	imgp->ps_strings = 0;
	imgp->auxarg_size = 0;
	imgp->args = args;

#ifdef MAC
	error = mac_execve_enter(imgp, mac_p);
	if (error)
		goto exec_fail;
#endif

	imgp->image_header = NULL;

	/*
	 * Translate the file name. namei() returns a vnode pointer
	 *	in ni_vp amoung other things.
	 *
	 * XXXAUDIT: It would be desirable to also audit the name of the
	 * interpreter if this is an interpreted binary.
	 */
	ndp = &nd;
	NDINIT(ndp, LOOKUP, ISOPEN | LOCKLEAF | FOLLOW | SAVENAME | MPSAFE |
	    AUDITVNODE1, UIO_SYSSPACE, args->fname, td);

interpret:
	error = namei(ndp);
	if (error)
		goto exec_fail;

	vfslocked = NDHASGIANT(ndp);
	imgp->vp = ndp->ni_vp;

	/*
	 * Check file permissions (also 'opens' file)
	 */
	error = exec_check_permissions(imgp);
	if (error)
		goto exec_fail_dealloc;

	imgp->object = imgp->vp->v_object;
	if (imgp->object != NULL)
		vm_object_reference(imgp->object);

	/*
	 * Set VV_TEXT now so no one can write to the executable while we're
	 * activating it.
	 *
	 * Remember if this was set before and unset it in case this is not
	 * actually an executable image.
	 */
	textset = imgp->vp->v_vflag & VV_TEXT;
	imgp->vp->v_vflag |= VV_TEXT;

	error = exec_map_first_page(imgp);
	if (error)
		goto exec_fail_dealloc;

	imgp->proc->p_osrel = 0;
	/*
	 *	If the current process has a special image activator it
	 *	wants to try first, call it.   For example, emulating shell
	 *	scripts differently.
	 */
	error = -1;
	if ((img_first = imgp->proc->p_sysent->sv_imgact_try) != NULL)
		error = img_first(imgp);

	/*
	 *	Loop through the list of image activators, calling each one.
	 *	An activator returns -1 if there is no match, 0 on success,
	 *	and an error otherwise.
	 */
	for (i = 0; error == -1 && execsw[i]; ++i) {
		if (execsw[i]->ex_imgact == NULL ||
		    execsw[i]->ex_imgact == img_first) {
			continue;
		}
		error = (*execsw[i]->ex_imgact)(imgp);
	}

	if (error) {
		if (error == -1) {
			if (textset == 0)
				imgp->vp->v_vflag &= ~VV_TEXT;
			error = ENOEXEC;
		}
		goto exec_fail_dealloc;
	}

	/*
	 * Special interpreter operation, cleanup and loop up to try to
	 * activate the interpreter.
	 */
	if (imgp->interpreted) {
		exec_unmap_first_page(imgp);
		/*
		 * VV_TEXT needs to be unset for scripts.  There is a short
		 * period before we determine that something is a script where
		 * VV_TEXT will be set. The vnode lock is held over this
		 * entire period so nothing should illegitimately be blocked.
		 */
		imgp->vp->v_vflag &= ~VV_TEXT;
		/* free name buffer and old vnode */
		NDFREE(ndp, NDF_ONLY_PNBUF);
#ifdef MAC
		interplabel = mac_vnode_label_alloc();
		mac_copy_vnode_label(ndp->ni_vp->v_label, interplabel);
#endif
		vput(ndp->ni_vp);
		vm_object_deallocate(imgp->object);
		imgp->object = NULL;
		VFS_UNLOCK_GIANT(vfslocked);
		vfslocked = 0;
		/* set new name to that of the interpreter */
		NDINIT(ndp, LOOKUP, LOCKLEAF | FOLLOW | SAVENAME | MPSAFE,
		    UIO_SYSSPACE, imgp->interpreter_name, td);
		goto interpret;
	}

	/*
	 * Copy out strings (args and env) and initialize stack base
	 */
	if (p->p_sysent->sv_copyout_strings)
		stack_base = (*p->p_sysent->sv_copyout_strings)(imgp);
	else
		stack_base = exec_copyout_strings(imgp);

	/*
	 * If custom stack fixup routine present for this process
	 * let it do the stack setup.
	 * Else stuff argument count as first item on stack
	 */
	if (p->p_sysent->sv_fixup != NULL)
		(*p->p_sysent->sv_fixup)(&stack_base, imgp);
	else
		suword(--stack_base, imgp->args->argc);

	/*
	 * For security and other reasons, the file descriptor table cannot
	 * be shared after an exec.
	 */
	fdunshare(p, td);

	/*
	 * Malloc things before we need locks.
	 */
	newcred = crget();
	euip = uifind(attr.va_uid);
	i = imgp->args->begin_envv - imgp->args->begin_argv;
	/* Cache arguments if they fit inside our allowance */
	if (ps_arg_cache_limit >= i + sizeof(struct pargs)) {
		newargs = pargs_alloc(i);
		bcopy(imgp->args->begin_argv, newargs->ar_args, i);
	}

	/* close files on exec */
	VOP_UNLOCK(imgp->vp, 0, td);
	fdcloseexec(td);
	vn_lock(imgp->vp, LK_EXCLUSIVE | LK_RETRY, td);

	/* Get a reference to the vnode prior to locking the proc */
	VREF(ndp->ni_vp);

	/*
	 * For security and other reasons, signal handlers cannot
	 * be shared after an exec. The new process gets a copy of the old
	 * handlers. In execsigs(), the new process will have its signals
	 * reset.
	 */
	PROC_LOCK(p);
	if (sigacts_shared(p->p_sigacts)) {
		oldsigacts = p->p_sigacts;
		PROC_UNLOCK(p);
		newsigacts = sigacts_alloc();
		sigacts_copy(newsigacts, oldsigacts);
		PROC_LOCK(p);
		p->p_sigacts = newsigacts;
	} else
		oldsigacts = NULL;

	/* Stop profiling */
	stopprofclock(p);

	/* reset caught signals */
	execsigs(p);

	/* name this process - nameiexec(p, ndp) */
	len = min(ndp->ni_cnd.cn_namelen,MAXCOMLEN);
	bcopy(ndp->ni_cnd.cn_nameptr, p->p_comm, len);
	p->p_comm[len] = 0;

	/*
	 * mark as execed, wakeup the process that vforked (if any) and tell
	 * it that it now has its own resources back
	 */
	p->p_flag |= P_EXEC;
	if (p->p_pptr && (p->p_flag & P_PPWAIT)) {
		p->p_flag &= ~P_PPWAIT;
		wakeup(p->p_pptr);
	}

	/*
	 * Implement image setuid/setgid.
	 *
	 * Don't honor setuid/setgid if the filesystem prohibits it or if
	 * the process is being traced.
	 *
	 * XXXMAC: For the time being, use NOSUID to also prohibit
	 * transitions on the file system.
	 */
	oldcred = p->p_ucred;
	credential_changing = 0;
	credential_changing |= (attr.va_mode & VSUID) && oldcred->cr_uid !=
	    attr.va_uid;
	credential_changing |= (attr.va_mode & VSGID) && oldcred->cr_gid !=
	    attr.va_gid;
#ifdef MAC
	will_transition = mac_execve_will_transition(oldcred, imgp->vp,
	    interplabel, imgp);
	credential_changing |= will_transition;
#endif

	if (credential_changing &&
	    (imgp->vp->v_mount->mnt_flag & MNT_NOSUID) == 0 &&
	    (p->p_flag & P_TRACED) == 0) {
		/*
		 * Turn off syscall tracing for set-id programs, except for
		 * root.  Record any set-id flags first to make sure that
		 * we do not regain any tracing during a possible block.
		 */
		setsugid(p);

#ifdef KTRACE
		if (p->p_tracevp != NULL &&
		    priv_check_cred(oldcred, PRIV_DEBUG_DIFFCRED, 0)) {
			mtx_lock(&ktrace_mtx);
			p->p_traceflag = 0;
			tracevp = p->p_tracevp;
			p->p_tracevp = NULL;
			tracecred = p->p_tracecred;
			p->p_tracecred = NULL;
			mtx_unlock(&ktrace_mtx);
		}
#endif
		/*
		 * Close any file descriptors 0..2 that reference procfs,
		 * then make sure file descriptors 0..2 are in use.
		 *
		 * setugidsafety() may call closef() and then pfind()
		 * which may grab the process lock.
		 * fdcheckstd() may call falloc() which may block to
		 * allocate memory, so temporarily drop the process lock.
		 */
		PROC_UNLOCK(p);
		setugidsafety(td);
		VOP_UNLOCK(imgp->vp, 0, td);
		error = fdcheckstd(td);
		vn_lock(imgp->vp, LK_EXCLUSIVE | LK_RETRY, td);
		if (error != 0)
			goto done1;
		PROC_LOCK(p);
		/*
		 * Set the new credentials.
		 */
		crcopy(newcred, oldcred);
		if (attr.va_mode & VSUID)
			change_euid(newcred, euip);
		if (attr.va_mode & VSGID)
			change_egid(newcred, attr.va_gid);
#ifdef MAC
		if (will_transition) {
			mac_execve_transition(oldcred, newcred, imgp->vp,
			    interplabel, imgp);
		}
#endif
		/*
		 * Implement correct POSIX saved-id behavior.
		 *
		 * XXXMAC: Note that the current logic will save the
		 * uid and gid if a MAC domain transition occurs, even
		 * though maybe it shouldn't.
		 */
		change_svuid(newcred, newcred->cr_uid);
		change_svgid(newcred, newcred->cr_gid);
		p->p_ucred = newcred;
		newcred = NULL;
	} else {
		if (oldcred->cr_uid == oldcred->cr_ruid &&
		    oldcred->cr_gid == oldcred->cr_rgid)
			p->p_flag &= ~P_SUGID;
		/*
		 * Implement correct POSIX saved-id behavior.
		 *
		 * XXX: It's not clear that the existing behavior is
		 * POSIX-compliant.  A number of sources indicate that the
		 * saved uid/gid should only be updated if the new ruid is
		 * not equal to the old ruid, or the new euid is not equal
		 * to the old euid and the new euid is not equal to the old
		 * ruid.  The FreeBSD code always updates the saved uid/gid.
		 * Also, this code uses the new (replaced) euid and egid as
		 * the source, which may or may not be the right ones to use.
		 */
		if (oldcred->cr_svuid != oldcred->cr_uid ||
		    oldcred->cr_svgid != oldcred->cr_gid) {
			crcopy(newcred, oldcred);
			change_svuid(newcred, newcred->cr_uid);
			change_svgid(newcred, newcred->cr_gid);
			p->p_ucred = newcred;
			newcred = NULL;
		}
	}

	/*
	 * Store the vp for use in procfs.  This vnode was referenced prior
	 * to locking the proc lock.
	 */
	textvp = p->p_textvp;
	p->p_textvp = ndp->ni_vp;

	/*
	 * Notify others that we exec'd, and clear the P_INEXEC flag
	 * as we're now a bona fide freshly-execed process.
	 */
	KNOTE_LOCKED(&p->p_klist, NOTE_EXEC);
	p->p_flag &= ~P_INEXEC;

	/*
	 * If tracing the process, trap to debugger so breakpoints
	 * can be set before the program executes.
	 * Use tdsignal to deliver signal to current thread, use
	 * psignal may cause the signal to be delivered to wrong thread
	 * because that thread will exit, remember we are going to enter
	 * single thread mode.
	 */
	if (p->p_flag & P_TRACED)
		tdsignal(p, td, SIGTRAP, NULL);

	/* clear "fork but no exec" flag, as we _are_ execing */
	p->p_acflag &= ~AFORK;

	/*
	 * Free any previous argument cache and replace it with
	 * the new argument cache, if any.
	 */
	oldargs = p->p_args;
	p->p_args = newargs;
	newargs = NULL;

#ifdef	HWPMC_HOOKS
	/*
	 * Check if system-wide sampling is in effect or if the
	 * current process is using PMCs.  If so, do exec() time
	 * processing.  This processing needs to happen AFTER the
	 * P_INEXEC flag is cleared.
	 *
	 * The proc lock needs to be released before taking the PMC
	 * SX.
	 */
	if (PMC_SYSTEM_SAMPLING_ACTIVE() || PMC_PROC_IS_USING_PMCS(p)) {
		PROC_UNLOCK(p);
		pe.pm_credentialschanged = credential_changing;
		pe.pm_entryaddr = imgp->entry_addr;

		PMC_CALL_HOOK_X(td, PMC_FN_PROCESS_EXEC, (void *) &pe);
	} else
		PROC_UNLOCK(p);
#else  /* !HWPMC_HOOKS */
	PROC_UNLOCK(p);
#endif

	/* Set values passed into the program in registers. */
	if (p->p_sysent->sv_setregs)
		(*p->p_sysent->sv_setregs)(td, imgp->entry_addr,
		    (u_long)(uintptr_t)stack_base, imgp->ps_strings);
	else
		exec_setregs(td, imgp->entry_addr,
		    (u_long)(uintptr_t)stack_base, imgp->ps_strings);

	vfs_mark_atime(imgp->vp, td);

done1:
	/*
	 * Free any resources malloc'd earlier that we didn't use.
	 */
	uifree(euip);
	if (newcred == NULL)
		crfree(oldcred);
	else
		crfree(newcred);
	VOP_UNLOCK(imgp->vp, 0, td);
	/*
	 * Handle deferred decrement of ref counts.
	 */
	if (textvp != NULL) {
		int tvfslocked;

		tvfslocked = VFS_LOCK_GIANT(textvp->v_mount);
		vrele(textvp);
		VFS_UNLOCK_GIANT(tvfslocked);
	}
	if (ndp->ni_vp && error != 0)
		vrele(ndp->ni_vp);
#ifdef KTRACE
	if (tracevp != NULL) {
		int tvfslocked;

		tvfslocked = VFS_LOCK_GIANT(tracevp->v_mount);
		vrele(tracevp);
		VFS_UNLOCK_GIANT(tvfslocked);
	}
	if (tracecred != NULL)
		crfree(tracecred);
#endif
	vn_lock(imgp->vp, LK_EXCLUSIVE | LK_RETRY, td);
	if (oldargs != NULL)
		pargs_drop(oldargs);
	if (newargs != NULL)
		pargs_drop(newargs);
	if (oldsigacts != NULL)
		sigacts_free(oldsigacts);

exec_fail_dealloc:

	/*
	 * free various allocated resources
	 */
	if (imgp->firstpage != NULL)
		exec_unmap_first_page(imgp);

	if (imgp->vp != NULL) {
		NDFREE(ndp, NDF_ONLY_PNBUF);
		vput(imgp->vp);
	}

	if (imgp->object != NULL)
		vm_object_deallocate(imgp->object);

	if (error == 0) {
		/*
		 * Stop the process here if its stop event mask has
		 * the S_EXEC bit set.
		 */
		STOPEVENT(p, S_EXEC, 0);
		goto done2;
	}

exec_fail:
	/* we're done here, clear P_INEXEC */
	PROC_LOCK(p);
	p->p_flag &= ~P_INEXEC;
	PROC_UNLOCK(p);

done2:
#ifdef MAC
	mac_execve_exit(imgp);
	if (interplabel != NULL)
		mac_vnode_label_free(interplabel);
#endif
	VFS_UNLOCK_GIANT(vfslocked);
	exec_free_args(args);

	if (error && imgp->vmspace_destroyed) {
		/* sorry, no more process anymore. exit gracefully */
		exit1(td, W_EXITCODE(0, SIGABRT));
		/* NOT REACHED */
	}
	return (error);
}

int
exec_map_first_page(imgp)
	struct image_params *imgp;
{
	int rv, i;
	int initial_pagein;
	vm_page_t ma[VM_INITIAL_PAGEIN];
	vm_object_t object;

	if (imgp->firstpage != NULL)
		exec_unmap_first_page(imgp);

	object = imgp->vp->v_object;
	if (object == NULL)
		return (EACCES);
	VM_OBJECT_LOCK(object);
	ma[0] = vm_page_grab(object, 0, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);
	if ((ma[0]->valid & VM_PAGE_BITS_ALL) != VM_PAGE_BITS_ALL) {
		initial_pagein = VM_INITIAL_PAGEIN;
		if (initial_pagein > object->size)
			initial_pagein = object->size;
		for (i = 1; i < initial_pagein; i++) {
			if ((ma[i] = vm_page_lookup(object, i)) != NULL) {
				if (ma[i]->valid)
					break;
				if ((ma[i]->oflags & VPO_BUSY) || ma[i]->busy)
					break;
				vm_page_busy(ma[i]);
			} else {
				ma[i] = vm_page_alloc(object, i,
				    VM_ALLOC_NORMAL | VM_ALLOC_IFNOTCACHED);
				if (ma[i] == NULL)
					break;
			}
		}
		initial_pagein = i;
		rv = vm_pager_get_pages(object, ma, initial_pagein, 0);
		ma[0] = vm_page_lookup(object, 0);
		if ((rv != VM_PAGER_OK) || (ma[0] == NULL) ||
		    (ma[0]->valid == 0)) {
			if (ma[0]) {
				vm_page_lock_queues();
				vm_page_free(ma[0]);
				vm_page_unlock_queues();
			}
			VM_OBJECT_UNLOCK(object);
			return (EIO);
		}
	}
	vm_page_lock_queues();
	vm_page_hold(ma[0]);
	vm_page_unlock_queues();
	vm_page_wakeup(ma[0]);
	VM_OBJECT_UNLOCK(object);

	imgp->firstpage = sf_buf_alloc(ma[0], 0);
	imgp->image_header = (char *)sf_buf_kva(imgp->firstpage);

	return (0);
}

void
exec_unmap_first_page(imgp)
	struct image_params *imgp;
{
	vm_page_t m;

	if (imgp->firstpage != NULL) {
		m = sf_buf_page(imgp->firstpage);
		sf_buf_free(imgp->firstpage);
		imgp->firstpage = NULL;
		vm_page_lock_queues();
		vm_page_unhold(m);
		vm_page_unlock_queues();
	}
}

/*
 * Destroy old address space, and allocate a new stack
 *	The new stack is only SGROWSIZ large because it is grown
 *	automatically in trap.c.
 */
int
exec_new_vmspace(imgp, sv)
	struct image_params *imgp;
	struct sysentvec *sv;
{
	int error;
	struct proc *p = imgp->proc;
	struct vmspace *vmspace = p->p_vmspace;
	vm_offset_t stack_addr;
	vm_map_t map;
	u_long ssiz;

	imgp->vmspace_destroyed = 1;
	imgp->sysent = sv;

	/* May be called with Giant held */
	EVENTHANDLER_INVOKE(process_exec, p, imgp);

	/*
	 * Blow away entire process VM, if address space not shared,
	 * otherwise, create a new VM space so that other threads are
	 * not disrupted
	 */
	map = &vmspace->vm_map;
	if (vmspace->vm_refcnt == 1 && vm_map_min(map) == sv->sv_minuser &&
	    vm_map_max(map) == sv->sv_maxuser) {
		shmexit(vmspace);
		pmap_remove_pages(vmspace_pmap(vmspace));
		vm_map_remove(map, vm_map_min(map), vm_map_max(map));
	} else {
		error = vmspace_exec(p, sv->sv_minuser, sv->sv_maxuser);
		if (error)
			return (error);
		vmspace = p->p_vmspace;
		map = &vmspace->vm_map;
	}

	/* Allocate a new stack */
	if (sv->sv_maxssiz != NULL)
		ssiz = *sv->sv_maxssiz;
	else
		ssiz = maxssiz;
	stack_addr = sv->sv_usrstack - ssiz;
	error = vm_map_stack(map, stack_addr, (vm_size_t)ssiz,
	    sv->sv_stackprot, VM_PROT_ALL, MAP_STACK_GROWS_DOWN);
	if (error)
		return (error);

#ifdef __ia64__
	/* Allocate a new register stack */
	stack_addr = IA64_BACKINGSTORE;
	error = vm_map_stack(map, stack_addr, (vm_size_t)ssiz,
	    sv->sv_stackprot, VM_PROT_ALL, MAP_STACK_GROWS_UP);
	if (error)
		return (error);
#endif

	/* vm_ssize and vm_maxsaddr are somewhat antiquated concepts in the
	 * VM_STACK case, but they are still used to monitor the size of the
	 * process stack so we can check the stack rlimit.
	 */
	vmspace->vm_ssize = sgrowsiz >> PAGE_SHIFT;
	vmspace->vm_maxsaddr = (char *)sv->sv_usrstack - ssiz;

	return (0);
}

/*
 * Copy out argument and environment strings from the old process address
 * space into the temporary string buffer.
 */
int
exec_copyin_args(struct image_args *args, char *fname,
    enum uio_seg segflg, char **argv, char **envv)
{
	char *argp, *envp;
	int error;
	size_t length;

	error = 0;

	bzero(args, sizeof(*args));
	if (argv == NULL)
		return (EFAULT);
	/*
	 * Allocate temporary demand zeroed space for argument and
	 *	environment strings:
	 *
	 * o ARG_MAX for argument and environment;
	 * o MAXSHELLCMDLEN for the name of interpreters.
	 */
	args->buf = (char *) kmem_alloc_wait(exec_map,
	    PATH_MAX + ARG_MAX + MAXSHELLCMDLEN);
	if (args->buf == NULL)
		return (ENOMEM);
	args->begin_argv = args->buf;
	args->endp = args->begin_argv;
	args->stringspace = ARG_MAX;

	args->fname = args->buf + ARG_MAX;

	/*
	 * Copy the file name.
	 */
	error = (segflg == UIO_SYSSPACE) ?
	    copystr(fname, args->fname, PATH_MAX, &length) :
	    copyinstr(fname, args->fname, PATH_MAX, &length);
	if (error != 0)
		goto err_exit;

	/*
	 * extract arguments first
	 */
	while ((argp = (caddr_t) (intptr_t) fuword(argv++))) {
		if (argp == (caddr_t) -1) {
			error = EFAULT;
			goto err_exit;
		}
		if ((error = copyinstr(argp, args->endp,
		    args->stringspace, &length))) {
			if (error == ENAMETOOLONG) 
				error = E2BIG;
			goto err_exit;
		}
		args->stringspace -= length;
		args->endp += length;
		args->argc++;
	}

	args->begin_envv = args->endp;

	/*
	 * extract environment strings
	 */
	if (envv) {
		while ((envp = (caddr_t)(intptr_t)fuword(envv++))) {
			if (envp == (caddr_t)-1) {
				error = EFAULT;
				goto err_exit;
			}
			if ((error = copyinstr(envp, args->endp,
			    args->stringspace, &length))) {
				if (error == ENAMETOOLONG)
					error = E2BIG;
				goto err_exit;
			}
			args->stringspace -= length;
			args->endp += length;
			args->envc++;
		}
	}

	return (0);

err_exit:
	exec_free_args(args);
	return (error);
}

static void
exec_free_args(struct image_args *args)
{

	if (args->buf) {
		kmem_free_wakeup(exec_map, (vm_offset_t)args->buf,
		    PATH_MAX + ARG_MAX + MAXSHELLCMDLEN);
		args->buf = NULL;
	}
}

/*
 * Copy strings out to the new process address space, constructing new arg
 * and env vector tables. Return a pointer to the base so that it can be used
 * as the initial stack pointer.
 */
register_t *
exec_copyout_strings(imgp)
	struct image_params *imgp;
{
	int argc, envc;
	char **vectp;
	char *stringp, *destp;
	register_t *stack_base;
	struct ps_strings *arginfo;
	struct proc *p;
	int szsigcode;

	/*
	 * Calculate string base and vector table pointers.
	 * Also deal with signal trampoline code for this exec type.
	 */
	p = imgp->proc;
	szsigcode = 0;
	arginfo = (struct ps_strings *)p->p_sysent->sv_psstrings;
	if (p->p_sysent->sv_szsigcode != NULL)
		szsigcode = *(p->p_sysent->sv_szsigcode);
	destp =	(caddr_t)arginfo - szsigcode - SPARE_USRSPACE -
	    roundup((ARG_MAX - imgp->args->stringspace), sizeof(char *));

	/*
	 * install sigcode
	 */
	if (szsigcode)
		copyout(p->p_sysent->sv_sigcode, ((caddr_t)arginfo -
		    szsigcode), szsigcode);

	/*
	 * If we have a valid auxargs ptr, prepare some room
	 * on the stack.
	 */
	if (imgp->auxargs) {
		/*
		 * 'AT_COUNT*2' is size for the ELF Auxargs data. This is for
		 * lower compatibility.
		 */
		imgp->auxarg_size = (imgp->auxarg_size) ? imgp->auxarg_size :
		    (AT_COUNT * 2);
		/*
		 * The '+ 2' is for the null pointers at the end of each of
		 * the arg and env vector sets,and imgp->auxarg_size is room
		 * for argument of Runtime loader.
		 */
		vectp = (char **)(destp - (imgp->args->argc +
		    imgp->args->envc + 2 + imgp->auxarg_size) *
		    sizeof(char *));

	} else {
		/*
		 * The '+ 2' is for the null pointers at the end of each of
		 * the arg and env vector sets
		 */
		vectp = (char **)(destp - (imgp->args->argc + imgp->args->envc + 2) *
		    sizeof(char *));
	}

	/*
	 * vectp also becomes our initial stack base
	 */
	stack_base = (register_t *)vectp;

	stringp = imgp->args->begin_argv;
	argc = imgp->args->argc;
	envc = imgp->args->envc;

	/*
	 * Copy out strings - arguments and environment.
	 */
	copyout(stringp, destp, ARG_MAX - imgp->args->stringspace);

	/*
	 * Fill in "ps_strings" struct for ps, w, etc.
	 */
	suword(&arginfo->ps_argvstr, (long)(intptr_t)vectp);
	suword(&arginfo->ps_nargvstr, argc);

	/*
	 * Fill in argument portion of vector table.
	 */
	for (; argc > 0; --argc) {
		suword(vectp++, (long)(intptr_t)destp);
		while (*stringp++ != 0)
			destp++;
		destp++;
	}

	/* a null vector table pointer separates the argp's from the envp's */
	suword(vectp++, 0);

	suword(&arginfo->ps_envstr, (long)(intptr_t)vectp);
	suword(&arginfo->ps_nenvstr, envc);

	/*
	 * Fill in environment portion of vector table.
	 */
	for (; envc > 0; --envc) {
		suword(vectp++, (long)(intptr_t)destp);
		while (*stringp++ != 0)
			destp++;
		destp++;
	}

	/* end of vector table is a null pointer */
	suword(vectp, 0);

	return (stack_base);
}

/*
 * Check permissions of file to execute.
 *	Called with imgp->vp locked.
 *	Return 0 for success or error code on failure.
 */
int
exec_check_permissions(imgp)
	struct image_params *imgp;
{
	struct vnode *vp = imgp->vp;
	struct vattr *attr = imgp->attr;
	struct thread *td;
	int error;

	td = curthread;			/* XXXKSE */

	/* Get file attributes */
	error = VOP_GETATTR(vp, attr, td->td_ucred, td);
	if (error)
		return (error);

#ifdef MAC
	error = mac_check_vnode_exec(td->td_ucred, imgp->vp, imgp);
	if (error)
		return (error);
#endif
	
	/*
	 * 1) Check if file execution is disabled for the filesystem that this
	 *	file resides on.
	 * 2) Insure that at least one execute bit is on - otherwise root
	 *	will always succeed, and we don't want to happen unless the
	 *	file really is executable.
	 * 3) Insure that the file is a regular file.
	 */
	if ((vp->v_mount->mnt_flag & MNT_NOEXEC) ||
	    ((attr->va_mode & 0111) == 0) ||
	    (attr->va_type != VREG))
		return (EACCES);

	/*
	 * Zero length files can't be exec'd
	 */
	if (attr->va_size == 0)
		return (ENOEXEC);

	/*
	 *  Check for execute permission to file based on current credentials.
	 */
	error = VOP_ACCESS(vp, VEXEC, td->td_ucred, td);
	if (error)
		return (error);

	/*
	 * Check number of open-for-writes on the file and deny execution
	 * if there are any.
	 */
	if (vp->v_writecount)
		return (ETXTBSY);

	/*
	 * Call filesystem specific open routine (which does nothing in the
	 * general case).
	 */
	error = VOP_OPEN(vp, FREAD, td->td_ucred, td, NULL);
	return (error);
}

/*
 * Exec handler registration
 */
int
exec_register(execsw_arg)
	const struct execsw *execsw_arg;
{
	const struct execsw **es, **xs, **newexecsw;
	int count = 2;	/* New slot and trailing NULL */

	if (execsw)
		for (es = execsw; *es; es++)
			count++;
	newexecsw = malloc(count * sizeof(*es), M_TEMP, M_WAITOK);
	if (newexecsw == NULL)
		return (ENOMEM);
	xs = newexecsw;
	if (execsw)
		for (es = execsw; *es; es++)
			*xs++ = *es;
	*xs++ = execsw_arg;
	*xs = NULL;
	if (execsw)
		free(execsw, M_TEMP);
	execsw = newexecsw;
	return (0);
}

int
exec_unregister(execsw_arg)
	const struct execsw *execsw_arg;
{
	const struct execsw **es, **xs, **newexecsw;
	int count = 1;

	if (execsw == NULL)
		panic("unregister with no handlers left?\n");

	for (es = execsw; *es; es++) {
		if (*es == execsw_arg)
			break;
	}
	if (*es == NULL)
		return (ENOENT);
	for (es = execsw; *es; es++)
		if (*es != execsw_arg)
			count++;
	newexecsw = malloc(count * sizeof(*es), M_TEMP, M_WAITOK);
	if (newexecsw == NULL)
		return (ENOMEM);
	xs = newexecsw;
	for (es = execsw; *es; es++)
		if (*es != execsw_arg)
			*xs++ = *es;
	*xs = NULL;
	if (execsw)
		free(execsw, M_TEMP);
	execsw = newexecsw;
	return (0);
}
