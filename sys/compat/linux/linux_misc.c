/*-
 * Copyright (c) 2002 Doug Rabson
 * Copyright (c) 1994-1995 S�ren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/blist.h>
#include <sys/fcntl.h>
#if defined(__i386__)
#include <sys/imgact_aout.h>
#endif
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/racct.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>
#include <sys/wait.h>
#include <sys/cpuset.h>

#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/swap_pager.h>

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif

#include <compat/linux/linux_file.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_sysproto.h>
#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_misc.h>

int stclohz;				/* Statistics clock frequency */

static unsigned int linux_to_bsd_resource[LINUX_RLIM_NLIMITS] = {
	RLIMIT_CPU, RLIMIT_FSIZE, RLIMIT_DATA, RLIMIT_STACK,
	RLIMIT_CORE, RLIMIT_RSS, RLIMIT_NPROC, RLIMIT_NOFILE,
	RLIMIT_MEMLOCK, RLIMIT_AS 
};

struct l_sysinfo {
	l_long		uptime;		/* Seconds since boot */
	l_ulong		loads[3];	/* 1, 5, and 15 minute load averages */
#define LINUX_SYSINFO_LOADS_SCALE 65536
	l_ulong		totalram;	/* Total usable main memory size */
	l_ulong		freeram;	/* Available memory size */
	l_ulong		sharedram;	/* Amount of shared memory */
	l_ulong		bufferram;	/* Memory used by buffers */
	l_ulong		totalswap;	/* Total swap space size */
	l_ulong		freeswap;	/* swap space still available */
	l_ushort	procs;		/* Number of current processes */
	l_ushort	pads;
	l_ulong		totalbig;
	l_ulong		freebig;
	l_uint		mem_unit;
	char		_f[20-2*sizeof(l_long)-sizeof(l_int)];	/* padding */
};
int
linux_sysinfo(struct thread *td, struct linux_sysinfo_args *args)
{
	struct l_sysinfo sysinfo;
	vm_object_t object;
	int i, j;
	struct timespec ts;

	getnanouptime(&ts);
	if (ts.tv_nsec != 0)
		ts.tv_sec++;
	sysinfo.uptime = ts.tv_sec;

	/* Use the information from the mib to get our load averages */
	for (i = 0; i < 3; i++)
		sysinfo.loads[i] = averunnable.ldavg[i] *
		    LINUX_SYSINFO_LOADS_SCALE / averunnable.fscale;

	sysinfo.totalram = physmem * PAGE_SIZE;
	sysinfo.freeram = sysinfo.totalram - cnt.v_wire_count * PAGE_SIZE;

	sysinfo.sharedram = 0;
	mtx_lock(&vm_object_list_mtx);
	TAILQ_FOREACH(object, &vm_object_list, object_list)
		if (object->shadow_count > 1)
			sysinfo.sharedram += object->resident_page_count;
	mtx_unlock(&vm_object_list_mtx);

	sysinfo.sharedram *= PAGE_SIZE;
	sysinfo.bufferram = 0;

	swap_pager_status(&i, &j);
	sysinfo.totalswap = i * PAGE_SIZE;
	sysinfo.freeswap = (i - j) * PAGE_SIZE;

	sysinfo.procs = nprocs;

	/* The following are only present in newer Linux kernels. */
	sysinfo.totalbig = 0;
	sysinfo.freebig = 0;
	sysinfo.mem_unit = 1;

	return (copyout(&sysinfo, args->info, sizeof(sysinfo)));
}

int
linux_alarm(struct thread *td, struct linux_alarm_args *args)
{
	struct itimerval it, old_it;
	u_int secs;
	int error;

#ifdef DEBUG
	if (ldebug(alarm))
		printf(ARGS(alarm, "%u"), args->secs);
#endif
	
	secs = args->secs;

	if (secs > INT_MAX)
		secs = INT_MAX;

	it.it_value.tv_sec = (long) secs;
	it.it_value.tv_usec = 0;
	it.it_interval.tv_sec = 0;
	it.it_interval.tv_usec = 0;
	error = kern_setitimer(td, ITIMER_REAL, &it, &old_it);
	if (error)
		return (error);
	if (timevalisset(&old_it.it_value)) {
		if (old_it.it_value.tv_usec != 0)
			old_it.it_value.tv_sec++;
		td->td_retval[0] = old_it.it_value.tv_sec;
	}
	return (0);
}

int
linux_brk(struct thread *td, struct linux_brk_args *args)
{
	struct vmspace *vm = td->td_proc->p_vmspace;
	vm_offset_t new, old;
	struct obreak_args /* {
		char * nsize;
	} */ tmp;

#ifdef DEBUG
	if (ldebug(brk))
		printf(ARGS(brk, "%p"), (void *)(uintptr_t)args->dsend);
#endif
	old = (vm_offset_t)vm->vm_daddr + ctob(vm->vm_dsize);
	new = (vm_offset_t)args->dsend;
	tmp.nsize = (char *)new;
	if (((caddr_t)new > vm->vm_daddr) && !sys_obreak(td, &tmp))
		td->td_retval[0] = (long)new;
	else
		td->td_retval[0] = (long)old;

	return (0);
}

#if defined(__i386__)
/* XXX: what about amd64/linux32? */

int
linux_uselib(struct thread *td, struct linux_uselib_args *args)
{
	struct nameidata ni;
	struct vnode *vp;
	struct exec *a_out;
	struct vattr attr;
	vm_offset_t vmaddr;
	unsigned long file_offset;
	unsigned long bss_size;
	char *library;
	ssize_t aresid;
	int error;
	int locked, vfslocked;

	LCONVPATHEXIST(td, args->library, &library);

#ifdef DEBUG
	if (ldebug(uselib))
		printf(ARGS(uselib, "%s"), library);
#endif

	a_out = NULL;
	vfslocked = 0;
	locked = 0;
	vp = NULL;

	NDINIT(&ni, LOOKUP, ISOPEN | FOLLOW | LOCKLEAF | MPSAFE | AUDITVNODE1,
	    UIO_SYSSPACE, library, td);
	error = namei(&ni);
	LFREEPATH(library);
	if (error)
		goto cleanup;

	vp = ni.ni_vp;
	vfslocked = NDHASGIANT(&ni);
	NDFREE(&ni, NDF_ONLY_PNBUF);

	/*
	 * From here on down, we have a locked vnode that must be unlocked.
	 * XXX: The code below largely duplicates exec_check_permissions().
	 */
	locked = 1;

	/* Writable? */
	if (vp->v_writecount) {
		error = ETXTBSY;
		goto cleanup;
	}

	/* Executable? */
	error = VOP_GETATTR(vp, &attr, td->td_ucred);
	if (error)
		goto cleanup;

	if ((vp->v_mount->mnt_flag & MNT_NOEXEC) ||
	    ((attr.va_mode & 0111) == 0) || (attr.va_type != VREG)) {
		/* EACCESS is what exec(2) returns. */
		error = ENOEXEC;
		goto cleanup;
	}

	/* Sensible size? */
	if (attr.va_size == 0) {
		error = ENOEXEC;
		goto cleanup;
	}

	/* Can we access it? */
	error = VOP_ACCESS(vp, VEXEC, td->td_ucred, td);
	if (error)
		goto cleanup;

	/*
	 * XXX: This should use vn_open() so that it is properly authorized,
	 * and to reduce code redundancy all over the place here.
	 * XXX: Not really, it duplicates far more of exec_check_permissions()
	 * than vn_open().
	 */
#ifdef MAC
	error = mac_vnode_check_open(td->td_ucred, vp, VREAD);
	if (error)
		goto cleanup;
#endif
	error = VOP_OPEN(vp, FREAD, td->td_ucred, td, NULL);
	if (error)
		goto cleanup;

	/* Pull in executable header into exec_map */
	error = vm_mmap(exec_map, (vm_offset_t *)&a_out, PAGE_SIZE,
	    VM_PROT_READ, VM_PROT_READ, 0, OBJT_VNODE, vp, 0);
	if (error)
		goto cleanup;

	/* Is it a Linux binary ? */
	if (((a_out->a_magic >> 16) & 0xff) != 0x64) {
		error = ENOEXEC;
		goto cleanup;
	}

	/*
	 * While we are here, we should REALLY do some more checks
	 */

	/* Set file/virtual offset based on a.out variant. */
	switch ((int)(a_out->a_magic & 0xffff)) {
	case 0413:			/* ZMAGIC */
		file_offset = 1024;
		break;
	case 0314:			/* QMAGIC */
		file_offset = 0;
		break;
	default:
		error = ENOEXEC;
		goto cleanup;
	}

	bss_size = round_page(a_out->a_bss);

	/* Check various fields in header for validity/bounds. */
	if (a_out->a_text & PAGE_MASK || a_out->a_data & PAGE_MASK) {
		error = ENOEXEC;
		goto cleanup;
	}

	/* text + data can't exceed file size */
	if (a_out->a_data + a_out->a_text > attr.va_size) {
		error = EFAULT;
		goto cleanup;
	}

	/*
	 * text/data/bss must not exceed limits
	 * XXX - this is not complete. it should check current usage PLUS
	 * the resources needed by this library.
	 */
	PROC_LOCK(td->td_proc);
	if (a_out->a_text > maxtsiz ||
	    a_out->a_data + bss_size > lim_cur(td->td_proc, RLIMIT_DATA) ||
	    racct_set(td->td_proc, RACCT_DATA, a_out->a_data +
	    bss_size) != 0) {
		PROC_UNLOCK(td->td_proc);
		error = ENOMEM;
		goto cleanup;
	}
	PROC_UNLOCK(td->td_proc);

	/*
	 * Prevent more writers.
	 * XXX: Note that if any of the VM operations fail below we don't
	 * clear this flag.
	 */
	vp->v_vflag |= VV_TEXT;

	/*
	 * Lock no longer needed
	 */
	locked = 0;
	VOP_UNLOCK(vp, 0);
	VFS_UNLOCK_GIANT(vfslocked);

	/*
	 * Check if file_offset page aligned. Currently we cannot handle
	 * misalinged file offsets, and so we read in the entire image
	 * (what a waste).
	 */
	if (file_offset & PAGE_MASK) {
#ifdef DEBUG
		printf("uselib: Non page aligned binary %lu\n", file_offset);
#endif
		/* Map text+data read/write/execute */

		/* a_entry is the load address and is page aligned */
		vmaddr = trunc_page(a_out->a_entry);

		/* get anon user mapping, read+write+execute */
		error = vm_map_find(&td->td_proc->p_vmspace->vm_map, NULL, 0,
		    &vmaddr, a_out->a_text + a_out->a_data, FALSE, VM_PROT_ALL,
		    VM_PROT_ALL, 0);
		if (error)
			goto cleanup;

		error = vn_rdwr(UIO_READ, vp, (void *)vmaddr, file_offset,
		    a_out->a_text + a_out->a_data, UIO_USERSPACE, 0,
		    td->td_ucred, NOCRED, &aresid, td);
		if (error != 0)
			goto cleanup;
		if (aresid != 0) {
			error = ENOEXEC;
			goto cleanup;
		}
	} else {
#ifdef DEBUG
		printf("uselib: Page aligned binary %lu\n", file_offset);
#endif
		/*
		 * for QMAGIC, a_entry is 20 bytes beyond the load address
		 * to skip the executable header
		 */
		vmaddr = trunc_page(a_out->a_entry);

		/*
		 * Map it all into the process's space as a single
		 * copy-on-write "data" segment.
		 */
		error = vm_mmap(&td->td_proc->p_vmspace->vm_map, &vmaddr,
		    a_out->a_text + a_out->a_data, VM_PROT_ALL, VM_PROT_ALL,
		    MAP_PRIVATE | MAP_FIXED, OBJT_VNODE, vp, file_offset);
		if (error)
			goto cleanup;
	}
#ifdef DEBUG
	printf("mem=%08lx = %08lx %08lx\n", (long)vmaddr, ((long *)vmaddr)[0],
	    ((long *)vmaddr)[1]);
#endif
	if (bss_size != 0) {
		/* Calculate BSS start address */
		vmaddr = trunc_page(a_out->a_entry) + a_out->a_text +
		    a_out->a_data;

		/* allocate some 'anon' space */
		error = vm_map_find(&td->td_proc->p_vmspace->vm_map, NULL, 0,
		    &vmaddr, bss_size, FALSE, VM_PROT_ALL, VM_PROT_ALL, 0);
		if (error)
			goto cleanup;
	}

cleanup:
	/* Unlock vnode if needed */
	if (locked) {
		VOP_UNLOCK(vp, 0);
		VFS_UNLOCK_GIANT(vfslocked);
	}

	/* Release the temporary mapping. */
	if (a_out)
		kmem_free_wakeup(exec_map, (vm_offset_t)a_out, PAGE_SIZE);

	return (error);
}

#endif	/* __i386__ */

int
linux_select(struct thread *td, struct linux_select_args *args)
{
	l_timeval ltv;
	struct timeval tv0, tv1, utv, *tvp;
	int error;

#ifdef DEBUG
	if (ldebug(select))
		printf(ARGS(select, "%d, %p, %p, %p, %p"), args->nfds,
		    (void *)args->readfds, (void *)args->writefds,
		    (void *)args->exceptfds, (void *)args->timeout);
#endif

	/*
	 * Store current time for computation of the amount of
	 * time left.
	 */
	if (args->timeout) {
		if ((error = copyin(args->timeout, &ltv, sizeof(ltv))))
			goto select_out;
		utv.tv_sec = ltv.tv_sec;
		utv.tv_usec = ltv.tv_usec;
#ifdef DEBUG
		if (ldebug(select))
			printf(LMSG("incoming timeout (%jd/%ld)"),
			    (intmax_t)utv.tv_sec, utv.tv_usec);
#endif

		if (itimerfix(&utv)) {
			/*
			 * The timeval was invalid.  Convert it to something
			 * valid that will act as it does under Linux.
			 */
			utv.tv_sec += utv.tv_usec / 1000000;
			utv.tv_usec %= 1000000;
			if (utv.tv_usec < 0) {
				utv.tv_sec -= 1;
				utv.tv_usec += 1000000;
			}
			if (utv.tv_sec < 0)
				timevalclear(&utv);
		}
		microtime(&tv0);
		tvp = &utv;
	} else
		tvp = NULL;

	error = kern_select(td, args->nfds, args->readfds, args->writefds,
	    args->exceptfds, tvp, sizeof(l_int) * 8);

#ifdef DEBUG
	if (ldebug(select))
		printf(LMSG("real select returns %d"), error);
#endif
	if (error)
		goto select_out;

	if (args->timeout) {
		if (td->td_retval[0]) {
			/*
			 * Compute how much time was left of the timeout,
			 * by subtracting the current time and the time
			 * before we started the call, and subtracting
			 * that result from the user-supplied value.
			 */
			microtime(&tv1);
			timevalsub(&tv1, &tv0);
			timevalsub(&utv, &tv1);
			if (utv.tv_sec < 0)
				timevalclear(&utv);
		} else
			timevalclear(&utv);
#ifdef DEBUG
		if (ldebug(select))
			printf(LMSG("outgoing timeout (%jd/%ld)"),
			    (intmax_t)utv.tv_sec, utv.tv_usec);
#endif
		ltv.tv_sec = utv.tv_sec;
		ltv.tv_usec = utv.tv_usec;
		if ((error = copyout(&ltv, args->timeout, sizeof(ltv))))
			goto select_out;
	}

select_out:
#ifdef DEBUG
	if (ldebug(select))
		printf(LMSG("select_out -> %d"), error);
#endif
	return (error);
}

int
linux_mremap(struct thread *td, struct linux_mremap_args *args)
{
	struct munmap_args /* {
		void *addr;
		size_t len;
	} */ bsd_args;
	int error = 0;

#ifdef DEBUG
	if (ldebug(mremap))
		printf(ARGS(mremap, "%p, %08lx, %08lx, %08lx"),
		    (void *)(uintptr_t)args->addr,
		    (unsigned long)args->old_len,
		    (unsigned long)args->new_len,
		    (unsigned long)args->flags);
#endif

	if (args->flags & ~(LINUX_MREMAP_FIXED | LINUX_MREMAP_MAYMOVE)) {
		td->td_retval[0] = 0;
		return (EINVAL);
	}

	/*
	 * Check for the page alignment.
	 * Linux defines PAGE_MASK to be FreeBSD ~PAGE_MASK.
	 */
	if (args->addr & PAGE_MASK) {
		td->td_retval[0] = 0;
		return (EINVAL);
	}

	args->new_len = round_page(args->new_len);
	args->old_len = round_page(args->old_len);

	if (args->new_len > args->old_len) {
		td->td_retval[0] = 0;
		return (ENOMEM);
	}

	if (args->new_len < args->old_len) {
		bsd_args.addr =
		    (caddr_t)((uintptr_t)args->addr + args->new_len);
		bsd_args.len = args->old_len - args->new_len;
		error = sys_munmap(td, &bsd_args);
	}

	td->td_retval[0] = error ? 0 : (uintptr_t)args->addr;
	return (error);
}

#define LINUX_MS_ASYNC       0x0001
#define LINUX_MS_INVALIDATE  0x0002
#define LINUX_MS_SYNC        0x0004

int
linux_msync(struct thread *td, struct linux_msync_args *args)
{
	struct msync_args bsd_args;

	bsd_args.addr = (caddr_t)(uintptr_t)args->addr;
	bsd_args.len = (uintptr_t)args->len;
	bsd_args.flags = args->fl & ~LINUX_MS_SYNC;

	return (sys_msync(td, &bsd_args));
}

int
linux_time(struct thread *td, struct linux_time_args *args)
{
	struct timeval tv;
	l_time_t tm;
	int error;

#ifdef DEBUG
	if (ldebug(time))
		printf(ARGS(time, "*"));
#endif

	microtime(&tv);
	tm = tv.tv_sec;
	if (args->tm && (error = copyout(&tm, args->tm, sizeof(tm))))
		return (error);
	td->td_retval[0] = tm;
	return (0);
}

struct l_times_argv {
	l_clock_t	tms_utime;
	l_clock_t	tms_stime;
	l_clock_t	tms_cutime;
	l_clock_t	tms_cstime;
};


/*
 * Glibc versions prior to 2.2.1 always use hard-coded CLK_TCK value.
 * Since 2.2.1 Glibc uses value exported from kernel via AT_CLKTCK
 * auxiliary vector entry.
 */
#define	CLK_TCK		100

#define	CONVOTCK(r)	(r.tv_sec * CLK_TCK + r.tv_usec / (1000000 / CLK_TCK))
#define	CONVNTCK(r)	(r.tv_sec * stclohz + r.tv_usec / (1000000 / stclohz))

#define	CONVTCK(r)	(linux_kernver(td) >= LINUX_KERNVER_2004000 ?		\
			    CONVNTCK(r) : CONVOTCK(r))

int
linux_times(struct thread *td, struct linux_times_args *args)
{
	struct timeval tv, utime, stime, cutime, cstime;
	struct l_times_argv tms;
	struct proc *p;
	int error;

#ifdef DEBUG
	if (ldebug(times))
		printf(ARGS(times, "*"));
#endif

	if (args->buf != NULL) {
		p = td->td_proc;
		PROC_LOCK(p);
		PROC_SLOCK(p);
		calcru(p, &utime, &stime);
		PROC_SUNLOCK(p);
		calccru(p, &cutime, &cstime);
		PROC_UNLOCK(p);

		tms.tms_utime = CONVTCK(utime);
		tms.tms_stime = CONVTCK(stime);

		tms.tms_cutime = CONVTCK(cutime);
		tms.tms_cstime = CONVTCK(cstime);

		if ((error = copyout(&tms, args->buf, sizeof(tms))))
			return (error);
	}

	microuptime(&tv);
	td->td_retval[0] = (int)CONVTCK(tv);
	return (0);
}

int
linux_newuname(struct thread *td, struct linux_newuname_args *args)
{
	struct l_new_utsname utsname;
	char osname[LINUX_MAX_UTSNAME];
	char osrelease[LINUX_MAX_UTSNAME];
	char *p;

#ifdef DEBUG
	if (ldebug(newuname))
		printf(ARGS(newuname, "*"));
#endif

	linux_get_osname(td, osname);
	linux_get_osrelease(td, osrelease);

	bzero(&utsname, sizeof(utsname));
	strlcpy(utsname.sysname, osname, LINUX_MAX_UTSNAME);
	getcredhostname(td->td_ucred, utsname.nodename, LINUX_MAX_UTSNAME);
	getcreddomainname(td->td_ucred, utsname.domainname, LINUX_MAX_UTSNAME);
	strlcpy(utsname.release, osrelease, LINUX_MAX_UTSNAME);
	strlcpy(utsname.version, version, LINUX_MAX_UTSNAME);
	for (p = utsname.version; *p != '\0'; ++p)
		if (*p == '\n') {
			*p = '\0';
			break;
		}
	strlcpy(utsname.machine, linux_platform, LINUX_MAX_UTSNAME);

	return (copyout(&utsname, args->buf, sizeof(utsname)));
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
struct l_utimbuf {
	l_time_t l_actime;
	l_time_t l_modtime;
};

int
linux_utime(struct thread *td, struct linux_utime_args *args)
{
	struct timeval tv[2], *tvp;
	struct l_utimbuf lut;
	char *fname;
	int error;

	LCONVPATHEXIST(td, args->fname, &fname);

#ifdef DEBUG
	if (ldebug(utime))
		printf(ARGS(utime, "%s, *"), fname);
#endif

	if (args->times) {
		if ((error = copyin(args->times, &lut, sizeof lut))) {
			LFREEPATH(fname);
			return (error);
		}
		tv[0].tv_sec = lut.l_actime;
		tv[0].tv_usec = 0;
		tv[1].tv_sec = lut.l_modtime;
		tv[1].tv_usec = 0;
		tvp = tv;
	} else
		tvp = NULL;

	error = kern_utimes(td, fname, UIO_SYSSPACE, tvp, UIO_SYSSPACE);
	LFREEPATH(fname);
	return (error);
}

int
linux_utimes(struct thread *td, struct linux_utimes_args *args)
{
	l_timeval ltv[2];
	struct timeval tv[2], *tvp = NULL;
	char *fname;
	int error;

	LCONVPATHEXIST(td, args->fname, &fname);

#ifdef DEBUG
	if (ldebug(utimes))
		printf(ARGS(utimes, "%s, *"), fname);
#endif

	if (args->tptr != NULL) {
		if ((error = copyin(args->tptr, ltv, sizeof ltv))) {
			LFREEPATH(fname);
			return (error);
		}
		tv[0].tv_sec = ltv[0].tv_sec;
		tv[0].tv_usec = ltv[0].tv_usec;
		tv[1].tv_sec = ltv[1].tv_sec;
		tv[1].tv_usec = ltv[1].tv_usec;
		tvp = tv;
	}

	error = kern_utimes(td, fname, UIO_SYSSPACE, tvp, UIO_SYSSPACE);
	LFREEPATH(fname);
	return (error);
}

int
linux_futimesat(struct thread *td, struct linux_futimesat_args *args)
{
	l_timeval ltv[2];
	struct timeval tv[2], *tvp = NULL;
	char *fname;
	int error, dfd;

	dfd = (args->dfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->dfd;
	LCONVPATHEXIST_AT(td, args->filename, &fname, dfd);

#ifdef DEBUG
	if (ldebug(futimesat))
		printf(ARGS(futimesat, "%s, *"), fname);
#endif

	if (args->utimes != NULL) {
		if ((error = copyin(args->utimes, ltv, sizeof ltv))) {
			LFREEPATH(fname);
			return (error);
		}
		tv[0].tv_sec = ltv[0].tv_sec;
		tv[0].tv_usec = ltv[0].tv_usec;
		tv[1].tv_sec = ltv[1].tv_sec;
		tv[1].tv_usec = ltv[1].tv_usec;
		tvp = tv;
	}

	error = kern_utimesat(td, dfd, fname, UIO_SYSSPACE, tvp, UIO_SYSSPACE);
	LFREEPATH(fname);
	return (error);
}
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

int
linux_common_wait(struct thread *td, int pid, int *status,
    int options, struct rusage *ru)
{
	int error, tmpstat;

	error = kern_wait(td, pid, &tmpstat, options, ru);
	if (error)
		return (error);

	if (status) {
		tmpstat &= 0xffff;
		if (WIFSIGNALED(tmpstat))
			tmpstat = (tmpstat & 0xffffff80) |
			    BSD_TO_LINUX_SIGNAL(WTERMSIG(tmpstat));
		else if (WIFSTOPPED(tmpstat))
			tmpstat = (tmpstat & 0xffff00ff) |
			    (BSD_TO_LINUX_SIGNAL(WSTOPSIG(tmpstat)) << 8);
		error = copyout(&tmpstat, status, sizeof(int));
	}

	return (error);
}

int
linux_waitpid(struct thread *td, struct linux_waitpid_args *args)
{
	int options;
 
#ifdef DEBUG
	if (ldebug(waitpid))
		printf(ARGS(waitpid, "%d, %p, %d"),
		    args->pid, (void *)args->status, args->options);
#endif
	/*
	 * this is necessary because the test in kern_wait doesn't work
	 * because we mess with the options here
	 */
	if (args->options & ~(WUNTRACED | WNOHANG | WCONTINUED | __WCLONE))
		return (EINVAL);
   
	options = (args->options & (WNOHANG | WUNTRACED));
	/* WLINUXCLONE should be equal to __WCLONE, but we make sure */
	if (args->options & __WCLONE)
		options |= WLINUXCLONE;

	return (linux_common_wait(td, args->pid, args->status, options, NULL));
}


int
linux_mknod(struct thread *td, struct linux_mknod_args *args)
{
	char *path;
	int error;

	LCONVPATHCREAT(td, args->path, &path);

#ifdef DEBUG
	if (ldebug(mknod))
		printf(ARGS(mknod, "%s, %d, %d"), path, args->mode, args->dev);
#endif

	switch (args->mode & S_IFMT) {
	case S_IFIFO:
	case S_IFSOCK:
		error = kern_mkfifo(td, path, UIO_SYSSPACE, args->mode);
		break;

	case S_IFCHR:
	case S_IFBLK:
		error = kern_mknod(td, path, UIO_SYSSPACE, args->mode,
		    args->dev);
		break;

	case S_IFDIR:
		error = EPERM;
		break;

	case 0:
		args->mode |= S_IFREG;
		/* FALLTHROUGH */
	case S_IFREG:
		error = kern_open(td, path, UIO_SYSSPACE,
		    O_WRONLY | O_CREAT | O_TRUNC, args->mode);
		if (error == 0)
			kern_close(td, td->td_retval[0]);
		break;

	default:
		error = EINVAL;
		break;
	}
	LFREEPATH(path);
	return (error);
}

int
linux_mknodat(struct thread *td, struct linux_mknodat_args *args)
{
	char *path;
	int error, dfd;

	dfd = (args->dfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->dfd;
	LCONVPATHCREAT_AT(td, args->filename, &path, dfd);

#ifdef DEBUG
	if (ldebug(mknodat))
		printf(ARGS(mknodat, "%s, %d, %d"), path, args->mode, args->dev);
#endif

	switch (args->mode & S_IFMT) {
	case S_IFIFO:
	case S_IFSOCK:
		error = kern_mkfifoat(td, dfd, path, UIO_SYSSPACE, args->mode);
		break;

	case S_IFCHR:
	case S_IFBLK:
		error = kern_mknodat(td, dfd, path, UIO_SYSSPACE, args->mode,
		    args->dev);
		break;

	case S_IFDIR:
		error = EPERM;
		break;

	case 0:
		args->mode |= S_IFREG;
		/* FALLTHROUGH */
	case S_IFREG:
		error = kern_openat(td, dfd, path, UIO_SYSSPACE,
		    O_WRONLY | O_CREAT | O_TRUNC, args->mode);
		if (error == 0)
			kern_close(td, td->td_retval[0]);
		break;

	default:
		error = EINVAL;
		break;
	}
	LFREEPATH(path);
	return (error);
}

/*
 * UGH! This is just about the dumbest idea I've ever heard!!
 */
int
linux_personality(struct thread *td, struct linux_personality_args *args)
{
#ifdef DEBUG
	if (ldebug(personality))
		printf(ARGS(personality, "%lu"), (unsigned long)args->per);
#endif
	if (args->per != 0)
		return (EINVAL);

	/* Yes Jim, it's still a Linux... */
	td->td_retval[0] = 0;
	return (0);
}

struct l_itimerval {
	l_timeval it_interval;
	l_timeval it_value;
};

#define	B2L_ITIMERVAL(bip, lip) 					\
	(bip)->it_interval.tv_sec = (lip)->it_interval.tv_sec;		\
	(bip)->it_interval.tv_usec = (lip)->it_interval.tv_usec;	\
	(bip)->it_value.tv_sec = (lip)->it_value.tv_sec;		\
	(bip)->it_value.tv_usec = (lip)->it_value.tv_usec;

int
linux_setitimer(struct thread *td, struct linux_setitimer_args *uap)
{
	int error;
	struct l_itimerval ls;
	struct itimerval aitv, oitv;

#ifdef DEBUG
	if (ldebug(setitimer))
		printf(ARGS(setitimer, "%p, %p"),
		    (void *)uap->itv, (void *)uap->oitv);
#endif

	if (uap->itv == NULL) {
		uap->itv = uap->oitv;
		return (linux_getitimer(td, (struct linux_getitimer_args *)uap));
	}

	error = copyin(uap->itv, &ls, sizeof(ls));
	if (error != 0)
		return (error);
	B2L_ITIMERVAL(&aitv, &ls);
#ifdef DEBUG
	if (ldebug(setitimer)) {
		printf("setitimer: value: sec: %jd, usec: %ld\n",
		    (intmax_t)aitv.it_value.tv_sec, aitv.it_value.tv_usec);
		printf("setitimer: interval: sec: %jd, usec: %ld\n",
		    (intmax_t)aitv.it_interval.tv_sec, aitv.it_interval.tv_usec);
	}
#endif
	error = kern_setitimer(td, uap->which, &aitv, &oitv);
	if (error != 0 || uap->oitv == NULL)
		return (error);
	B2L_ITIMERVAL(&ls, &oitv);

	return (copyout(&ls, uap->oitv, sizeof(ls)));
}

int
linux_getitimer(struct thread *td, struct linux_getitimer_args *uap)
{
	int error;
	struct l_itimerval ls;
	struct itimerval aitv;

#ifdef DEBUG
	if (ldebug(getitimer))
		printf(ARGS(getitimer, "%p"), (void *)uap->itv);
#endif
	error = kern_getitimer(td, uap->which, &aitv);
	if (error != 0)
		return (error);
	B2L_ITIMERVAL(&ls, &aitv);
	return (copyout(&ls, uap->itv, sizeof(ls)));
}

int
linux_nice(struct thread *td, struct linux_nice_args *args)
{
	struct setpriority_args bsd_args;

	bsd_args.which = PRIO_PROCESS;
	bsd_args.who = 0;		/* current process */
	bsd_args.prio = args->inc;
	return (sys_setpriority(td, &bsd_args));
}

int
linux_setgroups(struct thread *td, struct linux_setgroups_args *args)
{
	struct ucred *newcred, *oldcred;
	l_gid_t *linux_gidset;
	gid_t *bsd_gidset;
	int ngrp, error;
	struct proc *p;

	ngrp = args->gidsetsize;
	if (ngrp < 0 || ngrp >= ngroups_max + 1)
		return (EINVAL);
	linux_gidset = malloc(ngrp * sizeof(*linux_gidset), M_TEMP, M_WAITOK);
	error = copyin(args->grouplist, linux_gidset, ngrp * sizeof(l_gid_t));
	if (error)
		goto out;
	newcred = crget();
	p = td->td_proc;
	PROC_LOCK(p);
	oldcred = crcopysafe(p, newcred);

	/*
	 * cr_groups[0] holds egid. Setting the whole set from
	 * the supplied set will cause egid to be changed too.
	 * Keep cr_groups[0] unchanged to prevent that.
	 */

	if ((error = priv_check_cred(oldcred, PRIV_CRED_SETGROUPS, 0)) != 0) {
		PROC_UNLOCK(p);
		crfree(newcred);
		goto out;
	}

	if (ngrp > 0) {
		newcred->cr_ngroups = ngrp + 1;

		bsd_gidset = newcred->cr_groups;
		ngrp--;
		while (ngrp >= 0) {
			bsd_gidset[ngrp + 1] = linux_gidset[ngrp];
			ngrp--;
		}
	} else
		newcred->cr_ngroups = 1;

	setsugid(p);
	p->p_ucred = newcred;
	PROC_UNLOCK(p);
	crfree(oldcred);
	error = 0;
out:
	free(linux_gidset, M_TEMP);
	return (error);
}

int
linux_getgroups(struct thread *td, struct linux_getgroups_args *args)
{
	struct ucred *cred;
	l_gid_t *linux_gidset;
	gid_t *bsd_gidset;
	int bsd_gidsetsz, ngrp, error;

	cred = td->td_ucred;
	bsd_gidset = cred->cr_groups;
	bsd_gidsetsz = cred->cr_ngroups - 1;

	/*
	 * cr_groups[0] holds egid. Returning the whole set
	 * here will cause a duplicate. Exclude cr_groups[0]
	 * to prevent that.
	 */

	if ((ngrp = args->gidsetsize) == 0) {
		td->td_retval[0] = bsd_gidsetsz;
		return (0);
	}

	if (ngrp < bsd_gidsetsz)
		return (EINVAL);

	ngrp = 0;
	linux_gidset = malloc(bsd_gidsetsz * sizeof(*linux_gidset),
	    M_TEMP, M_WAITOK);
	while (ngrp < bsd_gidsetsz) {
		linux_gidset[ngrp] = bsd_gidset[ngrp + 1];
		ngrp++;
	}

	error = copyout(linux_gidset, args->grouplist, ngrp * sizeof(l_gid_t));
	free(linux_gidset, M_TEMP);
	if (error)
		return (error);

	td->td_retval[0] = ngrp;
	return (0);
}

int
linux_setrlimit(struct thread *td, struct linux_setrlimit_args *args)
{
	struct rlimit bsd_rlim;
	struct l_rlimit rlim;
	u_int which;
	int error;

#ifdef DEBUG
	if (ldebug(setrlimit))
		printf(ARGS(setrlimit, "%d, %p"),
		    args->resource, (void *)args->rlim);
#endif

	if (args->resource >= LINUX_RLIM_NLIMITS)
		return (EINVAL);

	which = linux_to_bsd_resource[args->resource];
	if (which == -1)
		return (EINVAL);

	error = copyin(args->rlim, &rlim, sizeof(rlim));
	if (error)
		return (error);

	bsd_rlim.rlim_cur = (rlim_t)rlim.rlim_cur;
	bsd_rlim.rlim_max = (rlim_t)rlim.rlim_max;
	return (kern_setrlimit(td, which, &bsd_rlim));
}

int
linux_old_getrlimit(struct thread *td, struct linux_old_getrlimit_args *args)
{
	struct l_rlimit rlim;
	struct proc *p = td->td_proc;
	struct rlimit bsd_rlim;
	u_int which;

#ifdef DEBUG
	if (ldebug(old_getrlimit))
		printf(ARGS(old_getrlimit, "%d, %p"),
		    args->resource, (void *)args->rlim);
#endif

	if (args->resource >= LINUX_RLIM_NLIMITS)
		return (EINVAL);

	which = linux_to_bsd_resource[args->resource];
	if (which == -1)
		return (EINVAL);

	PROC_LOCK(p);
	lim_rlimit(p, which, &bsd_rlim);
	PROC_UNLOCK(p);

#ifdef COMPAT_LINUX32
	rlim.rlim_cur = (unsigned int)bsd_rlim.rlim_cur;
	if (rlim.rlim_cur == UINT_MAX)
		rlim.rlim_cur = INT_MAX;
	rlim.rlim_max = (unsigned int)bsd_rlim.rlim_max;
	if (rlim.rlim_max == UINT_MAX)
		rlim.rlim_max = INT_MAX;
#else
	rlim.rlim_cur = (unsigned long)bsd_rlim.rlim_cur;
	if (rlim.rlim_cur == ULONG_MAX)
		rlim.rlim_cur = LONG_MAX;
	rlim.rlim_max = (unsigned long)bsd_rlim.rlim_max;
	if (rlim.rlim_max == ULONG_MAX)
		rlim.rlim_max = LONG_MAX;
#endif
	return (copyout(&rlim, args->rlim, sizeof(rlim)));
}

int
linux_getrlimit(struct thread *td, struct linux_getrlimit_args *args)
{
	struct l_rlimit rlim;
	struct proc *p = td->td_proc;
	struct rlimit bsd_rlim;
	u_int which;

#ifdef DEBUG
	if (ldebug(getrlimit))
		printf(ARGS(getrlimit, "%d, %p"),
		    args->resource, (void *)args->rlim);
#endif

	if (args->resource >= LINUX_RLIM_NLIMITS)
		return (EINVAL);

	which = linux_to_bsd_resource[args->resource];
	if (which == -1)
		return (EINVAL);

	PROC_LOCK(p);
	lim_rlimit(p, which, &bsd_rlim);
	PROC_UNLOCK(p);

	rlim.rlim_cur = (l_ulong)bsd_rlim.rlim_cur;
	rlim.rlim_max = (l_ulong)bsd_rlim.rlim_max;
	return (copyout(&rlim, args->rlim, sizeof(rlim)));
}

int
linux_sched_setscheduler(struct thread *td,
    struct linux_sched_setscheduler_args *args)
{
	struct sched_setscheduler_args bsd;

#ifdef DEBUG
	if (ldebug(sched_setscheduler))
		printf(ARGS(sched_setscheduler, "%d, %d, %p"),
		    args->pid, args->policy, (const void *)args->param);
#endif

	switch (args->policy) {
	case LINUX_SCHED_OTHER:
		bsd.policy = SCHED_OTHER;
		break;
	case LINUX_SCHED_FIFO:
		bsd.policy = SCHED_FIFO;
		break;
	case LINUX_SCHED_RR:
		bsd.policy = SCHED_RR;
		break;
	default:
		return (EINVAL);
	}

	bsd.pid = args->pid;
	bsd.param = (struct sched_param *)args->param;
	return (sys_sched_setscheduler(td, &bsd));
}

int
linux_sched_getscheduler(struct thread *td,
    struct linux_sched_getscheduler_args *args)
{
	struct sched_getscheduler_args bsd;
	int error;

#ifdef DEBUG
	if (ldebug(sched_getscheduler))
		printf(ARGS(sched_getscheduler, "%d"), args->pid);
#endif

	bsd.pid = args->pid;
	error = sys_sched_getscheduler(td, &bsd);

	switch (td->td_retval[0]) {
	case SCHED_OTHER:
		td->td_retval[0] = LINUX_SCHED_OTHER;
		break;
	case SCHED_FIFO:
		td->td_retval[0] = LINUX_SCHED_FIFO;
		break;
	case SCHED_RR:
		td->td_retval[0] = LINUX_SCHED_RR;
		break;
	}

	return (error);
}

int
linux_sched_get_priority_max(struct thread *td,
    struct linux_sched_get_priority_max_args *args)
{
	struct sched_get_priority_max_args bsd;

#ifdef DEBUG
	if (ldebug(sched_get_priority_max))
		printf(ARGS(sched_get_priority_max, "%d"), args->policy);
#endif

	switch (args->policy) {
	case LINUX_SCHED_OTHER:
		bsd.policy = SCHED_OTHER;
		break;
	case LINUX_SCHED_FIFO:
		bsd.policy = SCHED_FIFO;
		break;
	case LINUX_SCHED_RR:
		bsd.policy = SCHED_RR;
		break;
	default:
		return (EINVAL);
	}
	return (sys_sched_get_priority_max(td, &bsd));
}

int
linux_sched_get_priority_min(struct thread *td,
    struct linux_sched_get_priority_min_args *args)
{
	struct sched_get_priority_min_args bsd;

#ifdef DEBUG
	if (ldebug(sched_get_priority_min))
		printf(ARGS(sched_get_priority_min, "%d"), args->policy);
#endif

	switch (args->policy) {
	case LINUX_SCHED_OTHER:
		bsd.policy = SCHED_OTHER;
		break;
	case LINUX_SCHED_FIFO:
		bsd.policy = SCHED_FIFO;
		break;
	case LINUX_SCHED_RR:
		bsd.policy = SCHED_RR;
		break;
	default:
		return (EINVAL);
	}
	return (sys_sched_get_priority_min(td, &bsd));
}

#define REBOOT_CAD_ON	0x89abcdef
#define REBOOT_CAD_OFF	0
#define REBOOT_HALT	0xcdef0123
#define REBOOT_RESTART	0x01234567
#define REBOOT_RESTART2	0xA1B2C3D4
#define REBOOT_POWEROFF	0x4321FEDC
#define REBOOT_MAGIC1	0xfee1dead
#define REBOOT_MAGIC2	0x28121969
#define REBOOT_MAGIC2A	0x05121996
#define REBOOT_MAGIC2B	0x16041998

int
linux_reboot(struct thread *td, struct linux_reboot_args *args)
{
	struct reboot_args bsd_args;

#ifdef DEBUG
	if (ldebug(reboot))
		printf(ARGS(reboot, "0x%x"), args->cmd);
#endif

	if (args->magic1 != REBOOT_MAGIC1)
		return (EINVAL);

	switch (args->magic2) {
	case REBOOT_MAGIC2:
	case REBOOT_MAGIC2A:
	case REBOOT_MAGIC2B:
		break;
	default:
		return (EINVAL);
	}

	switch (args->cmd) {
	case REBOOT_CAD_ON:
	case REBOOT_CAD_OFF:
		return (priv_check(td, PRIV_REBOOT));
	case REBOOT_HALT:
		bsd_args.opt = RB_HALT;
		break;
	case REBOOT_RESTART:
	case REBOOT_RESTART2:
		bsd_args.opt = 0;
		break;
	case REBOOT_POWEROFF:
		bsd_args.opt = RB_POWEROFF;
		break;
	default:
		return (EINVAL);
	}
	return (sys_reboot(td, &bsd_args));
}


/*
 * The FreeBSD native getpid(2), getgid(2) and getuid(2) also modify
 * td->td_retval[1] when COMPAT_43 is defined. This clobbers registers that
 * are assumed to be preserved. The following lightweight syscalls fixes
 * this. See also linux_getgid16() and linux_getuid16() in linux_uid16.c
 *
 * linux_getpid() - MP SAFE
 * linux_getgid() - MP SAFE
 * linux_getuid() - MP SAFE
 */

int
linux_getpid(struct thread *td, struct linux_getpid_args *args)
{
	struct linux_emuldata *em;

#ifdef DEBUG
	if (ldebug(getpid))
		printf(ARGS(getpid, ""));
#endif

	if (linux_use26(td)) {
		em = em_find(td->td_proc, EMUL_DONTLOCK);
		KASSERT(em != NULL, ("getpid: emuldata not found.\n"));
		td->td_retval[0] = em->shared->group_pid;
	} else {
		td->td_retval[0] = td->td_proc->p_pid;
	}

	return (0);
}

int
linux_gettid(struct thread *td, struct linux_gettid_args *args)
{

#ifdef DEBUG
	if (ldebug(gettid))
		printf(ARGS(gettid, ""));
#endif

	td->td_retval[0] = td->td_proc->p_pid;
	return (0);
}


int
linux_getppid(struct thread *td, struct linux_getppid_args *args)
{
	struct linux_emuldata *em;
	struct proc *p, *pp;

#ifdef DEBUG
	if (ldebug(getppid))
		printf(ARGS(getppid, ""));
#endif

	if (!linux_use26(td)) {
		PROC_LOCK(td->td_proc);
		td->td_retval[0] = td->td_proc->p_pptr->p_pid;
		PROC_UNLOCK(td->td_proc);
		return (0);
	}

	em = em_find(td->td_proc, EMUL_DONTLOCK);

	KASSERT(em != NULL, ("getppid: process emuldata not found.\n"));

	/* find the group leader */
	p = pfind(em->shared->group_pid);

	if (p == NULL) {
#ifdef DEBUG
	   	printf(LMSG("parent process not found.\n"));
#endif
		return (0);
	}

	pp = p->p_pptr;		/* switch to parent */
	PROC_LOCK(pp);
	PROC_UNLOCK(p);

	/* if its also linux process */
	if (pp->p_sysent == &elf_linux_sysvec) {
		em = em_find(pp, EMUL_DONTLOCK);
		KASSERT(em != NULL, ("getppid: parent emuldata not found.\n"));

		td->td_retval[0] = em->shared->group_pid;
	} else
		td->td_retval[0] = pp->p_pid;

	PROC_UNLOCK(pp);

	return (0);
}

int
linux_getgid(struct thread *td, struct linux_getgid_args *args)
{

#ifdef DEBUG
	if (ldebug(getgid))
		printf(ARGS(getgid, ""));
#endif

	td->td_retval[0] = td->td_ucred->cr_rgid;
	return (0);
}

int
linux_getuid(struct thread *td, struct linux_getuid_args *args)
{

#ifdef DEBUG
	if (ldebug(getuid))
		printf(ARGS(getuid, ""));
#endif

	td->td_retval[0] = td->td_ucred->cr_ruid;
	return (0);
}


int
linux_getsid(struct thread *td, struct linux_getsid_args *args)
{
	struct getsid_args bsd;

#ifdef DEBUG
	if (ldebug(getsid))
		printf(ARGS(getsid, "%i"), args->pid);
#endif

	bsd.pid = args->pid;
	return (sys_getsid(td, &bsd));
}

int
linux_nosys(struct thread *td, struct nosys_args *ignore)
{

	return (ENOSYS);
}

int
linux_getpriority(struct thread *td, struct linux_getpriority_args *args)
{
	struct getpriority_args bsd_args;
	int error;

#ifdef DEBUG
	if (ldebug(getpriority))
		printf(ARGS(getpriority, "%i, %i"), args->which, args->who);
#endif

	bsd_args.which = args->which;
	bsd_args.who = args->who;
	error = sys_getpriority(td, &bsd_args);
	td->td_retval[0] = 20 - td->td_retval[0];
	return (error);
}

int
linux_sethostname(struct thread *td, struct linux_sethostname_args *args)
{
	int name[2];

#ifdef DEBUG
	if (ldebug(sethostname))
		printf(ARGS(sethostname, "*, %i"), args->len);
#endif

	name[0] = CTL_KERN;
	name[1] = KERN_HOSTNAME;
	return (userland_sysctl(td, name, 2, 0, 0, 0, args->hostname,
	    args->len, 0, 0));
}

int
linux_setdomainname(struct thread *td, struct linux_setdomainname_args *args)
{
	int name[2];

#ifdef DEBUG
	if (ldebug(setdomainname))
		printf(ARGS(setdomainname, "*, %i"), args->len);
#endif

	name[0] = CTL_KERN;
	name[1] = KERN_NISDOMAINNAME;
	return (userland_sysctl(td, name, 2, 0, 0, 0, args->name,
	    args->len, 0, 0));
}

int
linux_exit_group(struct thread *td, struct linux_exit_group_args *args)
{
	struct linux_emuldata *em;

#ifdef DEBUG
	if (ldebug(exit_group))
		printf(ARGS(exit_group, "%i"), args->error_code);
#endif

	em = em_find(td->td_proc, EMUL_DONTLOCK);
	if (em->shared->refs > 1) {
		EMUL_SHARED_WLOCK(&emul_shared_lock);
		em->shared->flags |= EMUL_SHARED_HASXSTAT;
		em->shared->xstat = W_EXITCODE(args->error_code, 0);
		EMUL_SHARED_WUNLOCK(&emul_shared_lock);
		if (linux_use26(td))
			linux_kill_threads(td, SIGKILL);
	}

	/*
	 * XXX: we should send a signal to the parent if
	 * SIGNAL_EXIT_GROUP is set. We ignore that (temporarily?)
	 * as it doesnt occur often.
	 */
	exit1(td, W_EXITCODE(args->error_code, 0));

	return (0);
}

#define _LINUX_CAPABILITY_VERSION  0x19980330

struct l_user_cap_header {
	l_int	version;
	l_int	pid;
};

struct l_user_cap_data {
	l_int	effective;
	l_int	permitted;
	l_int	inheritable;
};

int
linux_capget(struct thread *td, struct linux_capget_args *args)
{
	struct l_user_cap_header luch;
	struct l_user_cap_data lucd;
	int error;

	if (args->hdrp == NULL)
		return (EFAULT);

	error = copyin(args->hdrp, &luch, sizeof(luch));
	if (error != 0)
		return (error);

	if (luch.version != _LINUX_CAPABILITY_VERSION) {
		luch.version = _LINUX_CAPABILITY_VERSION;
		error = copyout(&luch, args->hdrp, sizeof(luch));
		if (error)
			return (error);
		return (EINVAL);
	}

	if (luch.pid)
		return (EPERM);

	if (args->datap) {
		/*
		 * The current implementation doesn't support setting
		 * a capability (it's essentially a stub) so indicate
		 * that no capabilities are currently set or available
		 * to request.
		 */
		bzero (&lucd, sizeof(lucd));
		error = copyout(&lucd, args->datap, sizeof(lucd));
	}

	return (error);
}

int
linux_capset(struct thread *td, struct linux_capset_args *args)
{
	struct l_user_cap_header luch;
	struct l_user_cap_data lucd;
	int error;

	if (args->hdrp == NULL || args->datap == NULL)
		return (EFAULT);

	error = copyin(args->hdrp, &luch, sizeof(luch));
	if (error != 0)
		return (error);

	if (luch.version != _LINUX_CAPABILITY_VERSION) {
		luch.version = _LINUX_CAPABILITY_VERSION;
		error = copyout(&luch, args->hdrp, sizeof(luch));
		if (error)
			return (error);
		return (EINVAL);
	}

	if (luch.pid)
		return (EPERM);

	error = copyin(args->datap, &lucd, sizeof(lucd));
	if (error != 0)
		return (error);

	/* We currently don't support setting any capabilities. */
	if (lucd.effective || lucd.permitted || lucd.inheritable) {
		linux_msg(td,
			  "capset effective=0x%x, permitted=0x%x, "
			  "inheritable=0x%x is not implemented",
			  (int)lucd.effective, (int)lucd.permitted,
			  (int)lucd.inheritable);
		return (EPERM);
	}

	return (0);
}

int
linux_prctl(struct thread *td, struct linux_prctl_args *args)
{
	int error = 0, max_size;
	struct proc *p = td->td_proc;
	char comm[LINUX_MAX_COMM_LEN];
	struct linux_emuldata *em;
	int pdeath_signal;

#ifdef DEBUG
	if (ldebug(prctl))
		printf(ARGS(prctl, "%d, %d, %d, %d, %d"), args->option,
		    args->arg2, args->arg3, args->arg4, args->arg5);
#endif

	switch (args->option) {
	case LINUX_PR_SET_PDEATHSIG:
		if (!LINUX_SIG_VALID(args->arg2))
			return (EINVAL);
		em = em_find(p, EMUL_DOLOCK);
		KASSERT(em != NULL, ("prctl: emuldata not found.\n"));
		em->pdeath_signal = args->arg2;
		EMUL_UNLOCK(&emul_lock);
		break;
	case LINUX_PR_GET_PDEATHSIG:
		em = em_find(p, EMUL_DOLOCK);
		KASSERT(em != NULL, ("prctl: emuldata not found.\n"));
		pdeath_signal = em->pdeath_signal;
		EMUL_UNLOCK(&emul_lock);
		error = copyout(&pdeath_signal,
		    (void *)(register_t)args->arg2,
		    sizeof(pdeath_signal));
		break;
	case LINUX_PR_GET_KEEPCAPS:
		/*
		 * Indicate that we always clear the effective and
		 * permitted capability sets when the user id becomes
		 * non-zero (actually the capability sets are simply
		 * always zero in the current implementation).
		 */
		td->td_retval[0] = 0;
		break;
	case LINUX_PR_SET_KEEPCAPS:
		/*
		 * Ignore requests to keep the effective and permitted
		 * capability sets when the user id becomes non-zero.
		 */
		break;
	case LINUX_PR_SET_NAME:
		/*
		 * To be on the safe side we need to make sure to not
		 * overflow the size a linux program expects. We already
		 * do this here in the copyin, so that we don't need to
		 * check on copyout.
		 */
		max_size = MIN(sizeof(comm), sizeof(p->p_comm));
		error = copyinstr((void *)(register_t)args->arg2, comm,
		    max_size, NULL);

		/* Linux silently truncates the name if it is too long. */
		if (error == ENAMETOOLONG) {
			/*
			 * XXX: copyinstr() isn't documented to populate the
			 * array completely, so do a copyin() to be on the
			 * safe side. This should be changed in case
			 * copyinstr() is changed to guarantee this.
			 */
			error = copyin((void *)(register_t)args->arg2, comm,
			    max_size - 1);
			comm[max_size - 1] = '\0';
		}
		if (error)
			return (error);

		PROC_LOCK(p);
		strlcpy(p->p_comm, comm, sizeof(p->p_comm));
		PROC_UNLOCK(p);
		break;
	case LINUX_PR_GET_NAME:
		PROC_LOCK(p);
		strlcpy(comm, p->p_comm, sizeof(comm));
		PROC_UNLOCK(p);
		error = copyout(comm, (void *)(register_t)args->arg2,
		    strlen(comm) + 1);
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error);
}

/*
 * Get affinity of a process.
 */
int
linux_sched_getaffinity(struct thread *td,
    struct linux_sched_getaffinity_args *args)
{
	int error;
	struct cpuset_getaffinity_args cga;

#ifdef DEBUG
	if (ldebug(sched_getaffinity))
		printf(ARGS(sched_getaffinity, "%d, %d, *"), args->pid,
		    args->len);
#endif
	if (args->len < sizeof(cpuset_t))
		return (EINVAL);

	cga.level = CPU_LEVEL_WHICH;
	cga.which = CPU_WHICH_PID;
	cga.id = args->pid;
	cga.cpusetsize = sizeof(cpuset_t);
	cga.mask = (cpuset_t *) args->user_mask_ptr;

	if ((error = sys_cpuset_getaffinity(td, &cga)) == 0)
		td->td_retval[0] = sizeof(cpuset_t);

	return (error);
}

/*
 *  Set affinity of a process.
 */
int
linux_sched_setaffinity(struct thread *td,
    struct linux_sched_setaffinity_args *args)
{
	struct cpuset_setaffinity_args csa;

#ifdef DEBUG
	if (ldebug(sched_setaffinity))
		printf(ARGS(sched_setaffinity, "%d, %d, *"), args->pid,
		    args->len);
#endif
	if (args->len < sizeof(cpuset_t))
		return (EINVAL);

	csa.level = CPU_LEVEL_WHICH;
	csa.which = CPU_WHICH_PID;
	csa.id = args->pid;
	csa.cpusetsize = sizeof(cpuset_t);
	csa.mask = (cpuset_t *) args->user_mask_ptr;

	return (sys_cpuset_setaffinity(td, &csa));
}
