/*-
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
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
 *
 * $NetBSD: ofw_machdep.c,v 1.5 2000/05/23 13:25:43 tsubai Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/powerpc/powerpc/ofw_machdep.c,v 1.13.2.1 2005/11/11 05:21:08 grehan Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/stat.h>

#include <net/ethernet.h>

#include <dev/ofw/openfirm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>

#include <machine/powerpc.h>
#include <machine/ofw_machdep.h>
#include <powerpc/ofw/ofw_pci.h>

#define	OFMEM_REGIONS	32
static struct mem_region OFmem[OFMEM_REGIONS + 1], OFavail[OFMEM_REGIONS + 3];
static struct mem_region OFfree[OFMEM_REGIONS + 3];

extern register_t ofmsr[5];
extern struct   pcpu __pcpu[MAXCPU];
extern struct	pmap ofw_pmap;
extern int	pmap_bootstrapped;
static int	(*ofwcall)(void *);

/*
 * Saved SPRG0-3 from OpenFirmware. Will be restored prior to the callback.
 */
register_t	ofw_sprg0_save;

static __inline void
ofw_sprg_prepare(void)
{
	/*
	 * Assume that interrupt are disabled at this point, or
	 * SPRG1-3 could be trashed
	 */
	__asm __volatile("mfsprg0 %0\n\t"
			 "mtsprg0 %1\n\t"
	    		 "mtsprg1 %2\n\t"
	    		 "mtsprg2 %3\n\t"
			 "mtsprg3 %4\n\t"
			 : "=&r"(ofw_sprg0_save)
			 : "r"(ofmsr[1]),
			 "r"(ofmsr[2]),
			 "r"(ofmsr[3]),
			 "r"(ofmsr[4]));
}

static __inline void
ofw_sprg_restore(void)
{
	/*
	 * Note that SPRG1-3 contents are irrelevant. They are scratch
	 * registers used in the early portion of trap handling when
	 * interrupts are disabled.
	 *
	 * PCPU data cannot be used until this routine is called !
	 */
	__asm __volatile("mtsprg0 %0" :: "r"(ofw_sprg0_save));
}

/*
 * Memory region utilities: determine if two regions overlap,
 * and merge two overlapping regions into one
 */
static int
memr_overlap(struct mem_region *r1, struct mem_region *r2)
{
	if ((r1->mr_start + r1->mr_size) < r2->mr_start ||
	    (r2->mr_start + r2->mr_size) < r1->mr_start)
		return (FALSE);
	
	return (TRUE);	
}

static void
memr_merge(struct mem_region *from, struct mem_region *to)
{
	int end;
	end = imax(to->mr_start + to->mr_size, from->mr_start + from->mr_size);
	to->mr_start = imin(from->mr_start, to->mr_start);
	to->mr_size = end - to->mr_start;
}

/*
 * This is called during powerpc_init, before the system is really initialized.
 * It shall provide the total and the available regions of RAM.
 * Both lists must have a zero-size entry as terminator.
 * The available regions need not take the kernel into account, but needs
 * to provide space for two additional entry beyond the terminating one.
 */
void
mem_regions(struct mem_region **memp, int *memsz,
		struct mem_region **availp, int *availsz)
{
	int phandle;
	int asz, msz, fsz;
	int i, j;
	int still_merging;
	
	/*
	 * Get memory.
	 */
	if ((phandle = OF_finddevice("/memory")) == -1
	    || (msz = OF_getprop(phandle, "reg",
			  OFmem, sizeof OFmem[0] * OFMEM_REGIONS))
	       <= 0
	    || (asz = OF_getprop(phandle, "available",
			  OFavail, sizeof OFavail[0] * OFMEM_REGIONS))
	       <= 0)
		panic("no memory?");
	*memp = OFmem;
	*memsz = msz / sizeof(struct mem_region);

	/*
	 * OFavail may have overlapping regions - collapse these
	 * and copy out remaining regions to OFfree
	 */
	asz /= sizeof(struct mem_region);
	do {
		still_merging = FALSE;
		for (i = 0; i < asz; i++) {
			if (OFavail[i].mr_size == 0)
				continue;
			for (j = i+1; j < asz; j++) {
				if (OFavail[j].mr_size == 0)
					continue;
				if (memr_overlap(&OFavail[j], &OFavail[i])) {
					memr_merge(&OFavail[j], &OFavail[i]);
					/* mark inactive */
					OFavail[j].mr_size = 0;
					still_merging = TRUE;
				}
			}
		}
	} while (still_merging == TRUE);

	/* evict inactive ranges */
	for (i = 0, fsz = 0; i < asz; i++) {
		if (OFavail[i].mr_size != 0) {
			OFfree[fsz] = OFavail[i];
			fsz++;
		}
	}

	*availp = OFfree;
	*availsz = fsz;
}

void
set_openfirm_callback(int (*openfirm)(void *))
{

	ofwcall = openfirm;
}

int
openfirmware(void *args)
{
	long	oldmsr;
	int	result;
	u_int	srsave[16];
	u_int   i;

	__asm __volatile(	"\t"
		"sync\n\t"
		"mfmsr  %0\n\t"
		"mtmsr  %1\n\t"
		"isync\n"
		: "=r" (oldmsr)
		: "r" (ofmsr[0])
	);

	ofw_sprg_prepare();

	if (pmap_bootstrapped) {
		/*
		 * Swap the kernel's address space with Open Firmware's
		 */
		for (i = 0; i < 16; i++) {
			srsave[i] = mfsrin(i << ADDR_SR_SHFT);
			mtsrin(i << ADDR_SR_SHFT, ofw_pmap.pm_sr[i]);
		}

		/*
		 * Clear battable[] translations
		 */
		__asm __volatile("mtdbatu 2, %0\n"
				 "mtdbatu 3, %0" : : "r" (0));
		isync();
	}

       	result = ofwcall(args);

	if (pmap_bootstrapped) {
		/*
		 * Restore the kernel's addr space. The isync() doesn;t
		 * work outside the loop unless mtsrin() is open-coded
		 * in an asm statement :(
		 */
		for (i = 0; i < 16; i++) {
			mtsrin(i << ADDR_SR_SHFT, srsave[i]);
			isync();
		}
	}

	ofw_sprg_restore();

	__asm(	"\t"
		"mtmsr  %0\n\t"
		"isync\n"
		: : "r" (oldmsr)
	);

	return (result);
}

void
OF_halt()
{
	int retval;	/* dummy, this may not be needed */

	OF_interpret("shut-down", 1, &retval);
	for (;;);	/* just in case */
}

void
OF_reboot()
{
	int retval;	/* dummy, this may not be needed */

	OF_interpret("reset-all", 1, &retval);
	for (;;);	/* just in case */
}

void
OF_getetheraddr(device_t dev, u_char *addr)
{
	phandle_t	node;

	node = ofw_pci_find_node(dev);
	OF_getprop(node, "local-mac-address", addr, ETHER_ADDR_LEN);
}

int
mem_valid(vm_offset_t addr, int len)
{
	int i;

	for (i = 0; i < OFMEM_REGIONS; i++)
		if ((addr >= OFmem[i].mr_start) 
		    && (addr + len < OFmem[i].mr_start + OFmem[i].mr_size))
			return (0);

	return (EFAULT);
}
