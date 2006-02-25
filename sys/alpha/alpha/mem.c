/*-
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, and code derived from software contributed to
 * Berkeley by William Jolitz.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
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
 *	from: Utah $Hdr: mem.c 1.13 89/10/08$
 *	from: @(#)mem.c	7.2 (Berkeley) 5/9/91
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/alpha/alpha/mem.c,v 1.51 2004/08/01 11:40:50 markm Exp $");

/*
 * Memory special file
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/msgbuf.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/uio.h>

#include <machine/md_var.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <machine/memdev.h>

struct mem_range_softc mem_range_softc;

/* ARGSUSED */
int
memrw(struct cdev *dev, struct uio *uio, int flags)
{
	vm_offset_t o, v;
	int c = 0;
	struct iovec *iov;
	int error = 0, rw;
	vm_offset_t addr, eaddr;

	GIANT_REQUIRED;

	while (uio->uio_resid > 0 && !error) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("memrw");
			continue;
		}
		if (minor(dev) == CDEV_MINOR_MEM) {
			v = uio->uio_offset;
kmemphys:
			/* Allow reads only in RAM. */
			rw = (uio->uio_rw == UIO_READ) ? VM_PROT_READ : VM_PROT_WRITE;
			if ((alpha_pa_access(v) & rw) != rw) {
				error = EFAULT;
				c = 0;
				break;
			}

			o = uio->uio_offset & PAGE_MASK;
			c = min(uio->uio_resid, (int)(PAGE_SIZE - o));
			error =
			    uiomove((caddr_t)ALPHA_PHYS_TO_K0SEG(v), c, uio);
			continue;
		}
		else if (minor(dev) == CDEV_MINOR_KMEM) {
			v = uio->uio_offset;

			if (v >= ALPHA_K0SEG_BASE && v <= ALPHA_K0SEG_END) {
				v = ALPHA_K0SEG_TO_PHYS(v);
				goto kmemphys;
			}

			c = min(iov->iov_len, MAXPHYS);
			/*
			 * Make sure that all of the pages are currently
			 * resident so that we don't create any zero-fill
			 * pages.
			 */
			addr = trunc_page(v);
			eaddr = round_page(v + c);
			for (; addr < eaddr; addr += PAGE_SIZE) 
				if (pmap_extract(kernel_pmap, addr) == 0) {
					return EFAULT;
				}
			if (!kernacc((caddr_t)v, c,
			    uio->uio_rw == UIO_READ ? 
			    VM_PROT_READ : VM_PROT_WRITE)) {
				return (EFAULT);
			}
			error = uiomove((caddr_t)v, c, uio);
			continue;
		}
		/* else panic! */
	}
	return (error);
}

/*
 * allow user processes to MMAP some memory sections
 * instead of going through read/write
 */
int
memmmap(struct cdev *dev, vm_offset_t offset, vm_paddr_t *paddr, int prot)
{
	/*
	 * /dev/mem is the only one that makes sense through this
	 * interface.  For /dev/kmem any physaddr we return here
	 * could be transient and hence incorrect or invalid at
	 * a later time.
	 */
	if (minor(dev) != CDEV_MINOR_MEM)
		return (-1);

	/*
	 * Allow access only in RAM.
	 */
	if ((prot & alpha_pa_access(atop((vm_offset_t)offset))) != prot)
		return (-1);
	*paddr = ALPHA_PHYS_TO_K0SEG(offset);
	return (0);
}

void
dev_mem_md_init(void)
{
}
