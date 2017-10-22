/*-
 * Copyright (C) 2002 Benno Rice
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*-
 * Copyright (C) 1993 Wolfgang Solfrank.
 * Copyright (C) 1993 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <machine/pcb.h>
#include <machine/sr.h>
#include <machine/slb.h>

int	setfault(faultbuf);	/* defined in locore.S */

/*
 * Makes sure that the right segment of userspace is mapped in.
 */

#ifdef __powerpc64__
static __inline void
set_user_sr(pmap_t pm, const void *addr)
{
	struct slb *slb;
	register_t slbv;

	/* Try lockless look-up first */
	slb = user_va_to_slb_entry(pm, (vm_offset_t)addr);

	if (slb == NULL) {
		/* If it isn't there, we need to pre-fault the VSID */
		PMAP_LOCK(pm);
		slbv = va_to_vsid(pm, (vm_offset_t)addr) << SLBV_VSID_SHIFT;
		PMAP_UNLOCK(pm);
	} else {
		slbv = slb->slbv;
	}

	/* Mark segment no-execute */
	slbv |= SLBV_N;

	/* If we have already set this VSID, we can just return */
	if (curthread->td_pcb->pcb_cpu.aim.usr_vsid == slbv) 
		return;

	__asm __volatile("isync");
	curthread->td_pcb->pcb_cpu.aim.usr_segm =
	    (uintptr_t)addr >> ADDR_SR_SHFT;
	curthread->td_pcb->pcb_cpu.aim.usr_vsid = slbv;
	__asm __volatile ("slbie %0; slbmte %1, %2; isync" ::
	    "r"(USER_ADDR), "r"(slbv), "r"(USER_SLB_SLBE));
}
#else
static __inline void
set_user_sr(pmap_t pm, const void *addr)
{
	register_t vsid;

	vsid = va_to_vsid(pm, (vm_offset_t)addr);

	/* Mark segment no-execute */
	vsid |= SR_N;

	/* If we have already set this VSID, we can just return */
	if (curthread->td_pcb->pcb_cpu.aim.usr_vsid == vsid)
		return;

	__asm __volatile("isync");
	curthread->td_pcb->pcb_cpu.aim.usr_segm =
	    (uintptr_t)addr >> ADDR_SR_SHFT;
	curthread->td_pcb->pcb_cpu.aim.usr_vsid = vsid;
	__asm __volatile("mtsr %0,%1; isync" :: "n"(USER_SR), "r"(vsid));
}
#endif

int
copyout(const void *kaddr, void *udaddr, size_t len)
{
	struct		thread *td;
	pmap_t		pm;
	faultbuf	env;
	const char	*kp;
	char		*up, *p;
	size_t		l;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (EFAULT);
	}

	kp = kaddr;
	up = udaddr;

	while (len > 0) {
		p = (char *)USER_ADDR + ((uintptr_t)up & ~SEGMENT_MASK);

		l = ((char *)USER_ADDR + SEGMENT_LENGTH) - p;
		if (l > len)
			l = len;

		set_user_sr(pm,up);

		bcopy(kp, p, l);

		up += l;
		kp += l;
		len -= l;
	}

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

int
copyin(const void *udaddr, void *kaddr, size_t len)
{
	struct		thread *td;
	pmap_t		pm;
	faultbuf	env;
	const char	*up;
	char		*kp, *p;
	size_t		l;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (EFAULT);
	}

	kp = kaddr;
	up = udaddr;

	while (len > 0) {
		p = (char *)USER_ADDR + ((uintptr_t)up & ~SEGMENT_MASK);

		l = ((char *)USER_ADDR + SEGMENT_LENGTH) - p;
		if (l > len)
			l = len;

		set_user_sr(pm,up);

		bcopy(p, kp, l);

		up += l;
		kp += l;
		len -= l;
	}

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

int
copyinstr(const void *udaddr, void *kaddr, size_t len, size_t *done)
{
	struct		thread *td;
	pmap_t		pm;
	faultbuf	env;
	const char	*up;
	char		*kp;
	size_t		l;
	int		rv, c;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (EFAULT);
	}

	kp = kaddr;
	up = udaddr;

	rv = ENAMETOOLONG;

	for (l = 0; len-- > 0; l++) {
		if ((c = fubyte(up++)) < 0) {
			rv = EFAULT;
			break;
		}

		if (!(*kp++ = c)) {
			l++;
			rv = 0;
			break;
		}
	}

	if (done != NULL) {
		*done = l;
	}

	td->td_pcb->pcb_onfault = NULL;
	return (rv);
}

int
subyte(void *addr, int byte)
{
	struct		thread *td;
	pmap_t		pm;
	faultbuf	env;
	char		*p;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;
	p = (char *)(USER_ADDR + ((uintptr_t)addr & ~SEGMENT_MASK));

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	set_user_sr(pm,addr);

	*p = (char)byte;

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

#ifdef __powerpc64__
int
suword32(void *addr, int word)
{
	struct		thread *td;
	pmap_t		pm;
	faultbuf	env;
	int		*p;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;
	p = (int *)(USER_ADDR + ((uintptr_t)addr & ~SEGMENT_MASK));

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	set_user_sr(pm,addr);

	*p = word;

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}
#endif

int
suword(void *addr, long word)
{
	struct		thread *td;
	pmap_t		pm;
	faultbuf	env;
	long		*p;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;
	p = (long *)(USER_ADDR + ((uintptr_t)addr & ~SEGMENT_MASK));

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	set_user_sr(pm,addr);

	*p = word;

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

#ifdef __powerpc64__
int
suword64(void *addr, int64_t word)
{
	return (suword(addr, (long)word));
}
#else
int
suword32(void *addr, int32_t word)
{
	return (suword(addr, (long)word));
}
#endif

int
fubyte(const void *addr)
{
	struct		thread *td;
	pmap_t		pm;
	faultbuf	env;
	u_char		*p;
	int		val;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;
	p = (u_char *)(USER_ADDR + ((uintptr_t)addr & ~SEGMENT_MASK));

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	set_user_sr(pm,addr);

	val = *p;

	td->td_pcb->pcb_onfault = NULL;
	return (val);
}

#ifdef __powerpc64__
int32_t
fuword32(const void *addr)
{
	struct		thread *td;
	pmap_t		pm;
	faultbuf	env;
	int32_t		*p, val;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;
	p = (int32_t *)(USER_ADDR + ((uintptr_t)addr & ~SEGMENT_MASK));

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	set_user_sr(pm,addr);

	val = *p;

	td->td_pcb->pcb_onfault = NULL;
	return (val);
}
#endif

long
fuword(const void *addr)
{
	struct		thread *td;
	pmap_t		pm;
	faultbuf	env;
	long		*p, val;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;
	p = (long *)(USER_ADDR + ((uintptr_t)addr & ~SEGMENT_MASK));

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	set_user_sr(pm,addr);

	val = *p;

	td->td_pcb->pcb_onfault = NULL;
	return (val);
}

#ifndef __powerpc64__
int32_t
fuword32(const void *addr)
{
	return ((int32_t)fuword(addr));
}
#endif

uint32_t
casuword32(volatile uint32_t *addr, uint32_t old, uint32_t new)
{
	struct thread *td;
	pmap_t pm;
	faultbuf env;
	uint32_t *p, val;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;
	p = (uint32_t *)(USER_ADDR + ((uintptr_t)addr & ~SEGMENT_MASK));

	set_user_sr(pm,(const void *)(vm_offset_t)addr);

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	__asm __volatile (
		"1:\tlwarx %0, 0, %2\n\t"	/* load old value */
		"cmplw %3, %0\n\t"		/* compare */
		"bne 2f\n\t"			/* exit if not equal */
		"stwcx. %4, 0, %2\n\t"      	/* attempt to store */
		"bne- 1b\n\t"			/* spin if failed */
		"b 3f\n\t"			/* we've succeeded */
		"2:\n\t"
		"stwcx. %0, 0, %2\n\t"       	/* clear reservation (74xx) */
		"3:\n\t"
		: "=&r" (val), "=m" (*p)
		: "r" (p), "r" (old), "r" (new), "m" (*p)
		: "cc", "memory");

	td->td_pcb->pcb_onfault = NULL;

	return (val);
}

#ifndef __powerpc64__
u_long
casuword(volatile u_long *addr, u_long old, u_long new)
{
	return (casuword32((volatile uint32_t *)addr, old, new));
}
#else
u_long
casuword(volatile u_long *addr, u_long old, u_long new)
{
	struct thread *td;
	pmap_t pm;
	faultbuf env;
	u_long *p, val;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;
	p = (u_long *)(USER_ADDR + ((uintptr_t)addr & ~SEGMENT_MASK));

	set_user_sr(pm,(const void *)(vm_offset_t)addr);

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	__asm __volatile (
		"1:\tldarx %0, 0, %2\n\t"	/* load old value */
		"cmpld %3, %0\n\t"		/* compare */
		"bne 2f\n\t"			/* exit if not equal */
		"stdcx. %4, 0, %2\n\t"      	/* attempt to store */
		"bne- 1b\n\t"			/* spin if failed */
		"b 3f\n\t"			/* we've succeeded */
		"2:\n\t"
		"stdcx. %0, 0, %2\n\t"       	/* clear reservation (74xx) */
		"3:\n\t"
		: "=&r" (val), "=m" (*p)
		: "r" (p), "r" (old), "r" (new), "m" (*p)
		: "cc", "memory");

	td->td_pcb->pcb_onfault = NULL;

	return (val);
}
#endif

