/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2010 Oleksandr Tymoshenko <gonzo@freebsd.org>
 * Copyright (c) 2008 Semihalf, Grzegorz Bernacki
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * from: FreeBSD: src/sys/arm/arm/minidump_machdep.c v214223
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/kerneldump.h>
#include <sys/msgbuf.h>
#include <sys/watchdog.h>
#include <sys/vmmeter.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>
#include <vm/vm_dumpset.h>
#include <machine/atomic.h>
#include <machine/elf.h>
#include <machine/md_var.h>
#include <machine/minidump.h>
#include <machine/cache.h>

CTASSERT(sizeof(struct kerneldumpheader) == 512);

static struct kerneldumpheader kdh;

/* Handle chunked writes. */
static uint64_t dumpsize;
/* Just auxiliary bufffer */
static char tmpbuffer[PAGE_SIZE] __aligned(sizeof(uint64_t));

extern pd_entry_t *kernel_segmap;

static int
write_buffer(struct dumperinfo *di, char *ptr, size_t sz)
{
	size_t len;
	int error, c;
	u_int maxdumpsz;

	maxdumpsz = di->maxiosize;

	if (maxdumpsz == 0)	/* seatbelt */
		maxdumpsz = PAGE_SIZE;

	error = 0;

	while (sz) {
		len = min(maxdumpsz, sz);

		dumpsys_pb_progress(len);
		wdog_kern_pat(WD_LASTVAL);

		if (ptr) {
			error = dump_append(di, ptr, len);
			if (error)
				return (error);
			ptr += len;
			sz -= len;
		} else {
			panic("pa is not supported");
		}

		/* Check for user abort. */
		c = cncheckc();
		if (c == 0x03)
			return (ECANCELED);
		if (c != -1)
			printf(" (CTRL-C to abort) ");
	}

	return (0);
}

int
cpu_minidumpsys(struct dumperinfo *di, const struct minidumpstate *state)
{
	struct minidumphdr mdhdr;
	struct msgbuf *mbp;
	uint64_t *dump_avail_buf;
	uint32_t ptesize;
	vm_paddr_t pa;
	vm_offset_t prev_pte = 0;
	uint32_t count = 0;
	vm_offset_t va;
	pt_entry_t *pte;
	int i, error;
	void *dump_va;

	/* Live dumps are untested. */
	if (!dumping)
		return (EOPNOTSUPP);

	/* Flush cache */
	mips_dcache_wbinv_all();

	/* Walk page table pages, set bits in vm_page_dump */
	ptesize = 0;

	for (va = VM_MIN_KERNEL_ADDRESS; va < kernel_vm_end; va += NBPDR) {
		ptesize += PAGE_SIZE;
		pte = pmap_pte(kernel_pmap, va);
		KASSERT(pte != NULL, ("pte for %jx is NULL", (uintmax_t)va));
		for (i = 0; i < NPTEPG; i++) {
			if (pte_test(&pte[i], PTE_V)) {
				pa = TLBLO_PTE_TO_PA(pte[i]);
				if (vm_phys_is_dumpable(pa))
					vm_page_dump_add(state->dump_bitset,
					    pa);
			}
		}
	}

	/*
	 * Now mark pages from 0 to phys_avail[0], that's where kernel 
	 * and pages allocated by pmap_steal reside
	 */
	for (pa = 0; pa < phys_avail[0]; pa += PAGE_SIZE) {
		if (vm_phys_is_dumpable(pa))
			vm_page_dump_add(state->dump_bitset, pa);
	}

	/* Calculate dump size. */
	mbp = state->msgbufp;
	dumpsize = ptesize;
	dumpsize += round_page(mbp->msg_size);
	dumpsize += round_page(nitems(dump_avail) * sizeof(uint64_t));
	dumpsize += round_page(BITSET_SIZE(vm_page_dump_pages));
	VM_PAGE_DUMP_FOREACH(state->dump_bitset, pa) {
		/* Clear out undumpable pages now if needed */
		if (vm_phys_is_dumpable(pa))
			dumpsize += PAGE_SIZE;
		else
			vm_page_dump_drop(state->dump_bitset, pa);
	}
	dumpsize += PAGE_SIZE;

	dumpsys_pb_init(dumpsize);

	/* Initialize mdhdr */
	bzero(&mdhdr, sizeof(mdhdr));
	strcpy(mdhdr.magic, MINIDUMP_MAGIC);
	mdhdr.version = MINIDUMP_VERSION;
	mdhdr.msgbufsize = mbp->msg_size;
	mdhdr.bitmapsize = round_page(BITSET_SIZE(vm_page_dump_pages));
	mdhdr.ptesize = ptesize;
	mdhdr.kernbase = VM_MIN_KERNEL_ADDRESS;
	mdhdr.dumpavailsize = round_page(nitems(dump_avail) * sizeof(uint64_t));

	dump_init_header(di, &kdh, KERNELDUMPMAGIC, KERNELDUMP_MIPS_VERSION,
	    dumpsize);

	error = dump_start(di, &kdh);
	if (error != 0)
		goto fail;

	printf("Dumping %llu out of %ju MB:", (long long)dumpsize >> 20,
	    ptoa((uintmax_t)physmem) / 1048576);

	/* Dump my header */
	bzero(tmpbuffer, sizeof(tmpbuffer));
	bcopy(&mdhdr, tmpbuffer, sizeof(mdhdr));
	error = write_buffer(di, tmpbuffer, PAGE_SIZE);
	if (error)
		goto fail;

	/* Dump msgbuf up front */
	error = write_buffer(di, mbp->msg_ptr, round_page(mbp->msg_size));
	if (error)
		goto fail;

	/* Dump dump_avail.  Make a copy using 64-bit physical addresses. */
	_Static_assert(nitems(dump_avail) * sizeof(uint64_t) <=
	    sizeof(tmpbuffer), "Large dump_avail not handled");
	bzero(tmpbuffer, sizeof(tmpbuffer));
	if (sizeof(dump_avail[0]) != sizeof(uint64_t)) {
		dump_avail_buf = (uint64_t *)tmpbuffer;
		for (i = 0; dump_avail[i] != 0 || dump_avail[i + 1] != 0; i++) {
			dump_avail_buf[i] = dump_avail[i];
			dump_avail_buf[i + 1] = dump_avail[i + 1];
		}
	} else {
		memcpy(tmpbuffer, dump_avail, sizeof(dump_avail));
	}
	error = write_buffer(di, tmpbuffer, PAGE_SIZE);
	if (error)
		goto fail;

	/* Dump bitmap */
	error = write_buffer(di, (char *)vm_page_dump,
	    round_page(BITSET_SIZE(vm_page_dump_pages)));
	if (error)
		goto fail;

	/* Dump kernel page table pages */
	for (va = VM_MIN_KERNEL_ADDRESS; va < kernel_vm_end; va += NBPDR) {
		pte = pmap_pte(kernel_pmap, va);
		KASSERT(pte != NULL, ("pte for %jx is NULL", (uintmax_t)va));
		if (!count) {
			prev_pte = (vm_offset_t)pte;
			count++;
		} else {
			if ((vm_offset_t)pte == (prev_pte + count * PAGE_SIZE))
				count++;
			else {
				error = write_buffer(di, (char*)prev_pte,
				    count * PAGE_SIZE);
				if (error)
					goto fail;
				count = 1;
				prev_pte = (vm_offset_t)pte;
			}
		}
	}

	if (count) {
		error = write_buffer(di, (char*)prev_pte, count * PAGE_SIZE);
		if (error)
			goto fail;
		count = 0;
		prev_pte = 0;
	}

	/* Dump memory chunks page by page */
	VM_PAGE_DUMP_FOREACH(state->dump_bitset, pa) {
		dump_va = pmap_kenter_temporary(pa, 0);
		error = write_buffer(di, dump_va, PAGE_SIZE);
		if (error)
			goto fail;
		pmap_kenter_temporary_free(pa);
	}

	error = dump_finish(di, &kdh);
	if (error != 0)
		goto fail;

	printf("\nDump complete\n");
	return (0);

fail:
	if (error < 0)
		error = -error;

	if (error == ECANCELED)
		printf("\nDump aborted\n");
	else if (error == E2BIG || error == ENOSPC) {
		printf("\nDump failed. Partition too small (about %lluMB were "
		    "needed this time).\n", (long long)dumpsize >> 20);
	} else
		printf("\n** DUMP FAILED (ERROR %d) **\n", error);
	return (error);
}
