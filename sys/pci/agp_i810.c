/*-
 * Copyright (c) 2000 Doug Rabson
 * Copyright (c) 2000 Ruslan Ermilov
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

/*
 * Fixes for 830/845G support: David Dawes <dawes@xfree86.org>
 * 852GM/855GM/865G support added by David Dawes <dawes@xfree86.org>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/pci/agp_i810.c,v 1.32.2.1 2005/12/14 00:47:25 anholt Exp $");

#include "opt_bus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <pci/agppriv.h>
#include <pci/agpreg.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

MALLOC_DECLARE(M_AGP);

#define READ1(off)	bus_space_read_1(sc->bst, sc->bsh, off)
#define READ4(off)	bus_space_read_4(sc->bst, sc->bsh, off)
#define WRITE4(off,v)	bus_space_write_4(sc->bst, sc->bsh, off, v)
#define WRITEGTT(off,v)	bus_space_write_4(sc->gtt_bst, sc->gtt_bsh, off, v)

#define CHIP_I810 0	/* i810/i815 */
#define CHIP_I830 1	/* 830M/845G */
#define CHIP_I855 2	/* 852GM/855GM/865G */
#define CHIP_I915 3	/* 915G/915GM */

struct agp_i810_softc {
	struct agp_softc agp;
	u_int32_t initial_aperture;	/* aperture size at startup */
	struct agp_gatt *gatt;
	int chiptype;			/* i810-like or i830 */
	u_int32_t dcache_size;		/* i810 only */
	u_int32_t stolen;		/* number of i830/845 gtt entries for stolen memory */
	device_t bdev;			/* bridge device */

	struct resource *regs;		/* memory mapped GC registers */
	bus_space_tag_t bst;		/* bus_space tag */
	bus_space_handle_t bsh;		/* bus_space handle */

	struct resource *gtt;		/* memory mapped GATT entries */
	bus_space_tag_t gtt_bst;	/* bus_space tag */
	bus_space_handle_t gtt_bsh;	/* bus_space handle */

        struct resource *gm;            /* unmapped (but allocated) aperture */
};

static const char*
agp_i810_match(device_t dev)
{
	if (pci_get_class(dev) != PCIC_DISPLAY
	    || pci_get_subclass(dev) != PCIS_DISPLAY_VGA)
		return NULL;

	switch (pci_get_devid(dev)) {
	case 0x71218086:
		return ("Intel 82810 (i810 GMCH) SVGA controller");

	case 0x71238086:
		return ("Intel 82810-DC100 (i810-DC100 GMCH) SVGA controller");

	case 0x71258086:
		return ("Intel 82810E (i810E GMCH) SVGA controller");

	case 0x11328086:
		return ("Intel 82815 (i815 GMCH) SVGA controller");

	case 0x35778086:
		return ("Intel 82830M (830M GMCH) SVGA controller");

	case 0x25628086:
		return ("Intel 82845G (845G GMCH) SVGA controller");

	case 0x35828086:
		switch (pci_read_config(dev, AGP_I85X_CAPID, 1)) {
		case AGP_I855_GME:
			return ("Intel 82855GME (855GME GMCH) SVGA controller");

		case AGP_I855_GM:
			return ("Intel 82855GM (855GM GMCH) SVGA controller");

		case AGP_I852_GME:
			return ("Intel 82852GME (852GME GMCH) SVGA controller");

		case AGP_I852_GM:
			return ("Intel 82852GM (852GM GMCH) SVGA controller");

		default:
			return ("Intel 8285xM (85xGM GMCH) SVGA controller");
		}

	case 0x25728086:
		return ("Intel 82865G (865G GMCH) SVGA controller");

	case 0x25828086:
		return ("Intel 82915G (915G GMCH) SVGA controller");

	case 0x25928086:
		return ("Intel 82915GM (915GM GMCH) SVGA controller");

	case 0x27728086:
		return ("Intel 82945G (945G GMCH) SVGA controller");

	case 0x27A28086:
		return ("Intel 82945GM (945GM GMCH) SVGA controller");
	};

	return NULL;
}

/*
 * Find bridge device.
 */
static device_t
agp_i810_find_bridge(device_t dev)
{
	device_t *children, child;
	int nchildren, i;
	u_int32_t devid;

	/*
	 * Calculate bridge device's ID.
	 */
	devid = pci_get_devid(dev);
	switch (devid) {
	case 0x71218086:
	case 0x71238086:
	case 0x71258086:
		devid -= 0x10000;
		break;

	case 0x11328086:
	case 0x35778086:
	case 0x25628086:
	case 0x35828086:
	case 0x25728086:
	case 0x25828086:
	case 0x25928086:
		devid -= 0x20000;
		break;
	};
	if (device_get_children(device_get_parent(dev), &children, &nchildren))
		return 0;

	for (i = 0; i < nchildren; i++) {
		child = children[i];

		if (pci_get_devid(child) == devid) {
			free(children, M_TEMP);
			return child;
		}
	}
	free(children, M_TEMP);
	return 0;
}

static int
agp_i810_probe(device_t dev)
{
	const char *desc;

	if (resource_disabled("agp", device_get_unit(dev)))
		return (ENXIO);
	desc = agp_i810_match(dev);
	if (desc) {
		device_t bdev;
		u_int8_t smram;
		unsigned int gcc1;
		int devid = pci_get_devid(dev);

		bdev = agp_i810_find_bridge(dev);
		if (!bdev) {
			if (bootverbose)
				printf("I810: can't find bridge device\n");
			return ENXIO;
		}

		/*
		 * checking whether internal graphics device has been activated.
		 */
		switch (devid) {
			/* i810 */
		case 0x71218086:
		case 0x71238086:
		case 0x71258086:
		case 0x11328086:
			smram = pci_read_config(bdev, AGP_I810_SMRAM, 1);
			if ((smram & AGP_I810_SMRAM_GMS)
			    == AGP_I810_SMRAM_GMS_DISABLED) {
				if (bootverbose)
					printf("I810: disabled, not probing\n");
				return ENXIO;
			}
			break;

			/* i830 */
		case 0x35778086:
		case 0x35828086:
		case 0x25628086:
		case 0x25728086:
			gcc1 = pci_read_config(bdev, AGP_I830_GCC1, 1);
			if ((gcc1 & AGP_I830_GCC1_DEV2) == AGP_I830_GCC1_DEV2_DISABLED) {
				if (bootverbose)
					printf("I830: disabled, not probing\n");
				return ENXIO;
			}
			break;

			/* i915 */
		case 0x25828086:
		case 0x25928086:
		case 0x27728086:	/* 945G GMCH */
		case 0x27A28086:	/* 945GM GMCH */
			gcc1 = pci_read_config(bdev, AGP_I915_DEVEN, 4);
			if ((gcc1 & AGP_I915_DEVEN_D2F0) ==
			    AGP_I915_DEVEN_D2F0_DISABLED) {
				if (bootverbose)
					printf("I915: disabled, not probing\n");
				return ENXIO;
			}
			break;

		default:
			return ENXIO;
		}

		device_verbose(dev);
		device_set_desc(dev, desc);
		return BUS_PROBE_DEFAULT;
	}

	return ENXIO;
}

static int
agp_i810_attach(device_t dev)
{
	struct agp_i810_softc *sc = device_get_softc(dev);
	struct agp_gatt *gatt;
	int error, rid;

	sc->bdev = agp_i810_find_bridge(dev);
	if (!sc->bdev)
		return ENOENT;

	error = agp_generic_attach(dev);
	if (error)
		return error;

	switch (pci_get_devid(dev)) {
	case 0x71218086:
	case 0x71238086:
	case 0x71258086:
	case 0x11328086:
		sc->chiptype = CHIP_I810;
		break;
	case 0x35778086:
	case 0x25628086:
		sc->chiptype = CHIP_I830;
		break;
	case 0x35828086:
	case 0x25728086:
		sc->chiptype = CHIP_I855;
		break;
	case 0x25828086:
	case 0x25928086:
	case 0x27728086:	/* 945G GMCH */
	case 0x27A28086:	/* 945GM GMCH */
		sc->chiptype = CHIP_I915;
		break;
	};

	/* Same for i810 and i830 */
	if (sc->chiptype == CHIP_I915)
		rid = AGP_I915_MMADR;
	else
		rid = AGP_I810_MMADR;

	sc->regs = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
					  RF_ACTIVE);
	if (!sc->regs) {
		agp_generic_detach(dev);
		return ENODEV;
	}
	sc->bst = rman_get_bustag(sc->regs);
	sc->bsh = rman_get_bushandle(sc->regs);

	if (sc->chiptype == CHIP_I915) {
		rid = AGP_I915_GTTADR;
		sc->gtt = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
						 RF_ACTIVE);
		if (!sc->gtt) {
			bus_release_resource(dev, SYS_RES_MEMORY,
					     AGP_I915_MMADR, sc->regs);
			agp_generic_detach(dev);
			return ENODEV;
		}
		sc->gtt_bst = rman_get_bustag(sc->gtt);
		sc->gtt_bsh = rman_get_bushandle(sc->gtt);

		/* While agp_generic_attach allocates the AGP_APBASE resource
		 * to try to reserve the aperture, on the 915 the aperture
		 * isn't in PCIR_BAR(0), it's in PCIR_BAR(2), so it allocated
		 * the registers that we just mapped anyway.  So, allocate the
		 * aperture here, which also gives us easy access to it for the
		 * agp_i810_get_aperture().
		 */
		rid = AGP_I915_GMADR;
		sc->gm = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, 0);
		if (sc->gm == NULL) {
			bus_release_resource(dev, SYS_RES_MEMORY,
					     AGP_I915_MMADR, sc->regs);
			bus_release_resource(dev, SYS_RES_MEMORY,
					     AGP_I915_GTTADR, sc->regs);
			agp_generic_detach(dev);
			return ENODEV;
		}

	}

	sc->initial_aperture = AGP_GET_APERTURE(dev);

	gatt = malloc( sizeof(struct agp_gatt), M_AGP, M_NOWAIT);
	if (!gatt) {
 		agp_generic_detach(dev);
 		return ENOMEM;
	}
	sc->gatt = gatt;

	gatt->ag_entries = AGP_GET_APERTURE(dev) >> AGP_PAGE_SHIFT;

	if ( sc->chiptype == CHIP_I810 ) {
		/* Some i810s have on-chip memory called dcache */
		if (READ1(AGP_I810_DRT) & AGP_I810_DRT_POPULATED)
			sc->dcache_size = 4 * 1024 * 1024;
		else
			sc->dcache_size = 0;

		/* According to the specs the gatt on the i810 must be 64k */
		gatt->ag_virtual = contigmalloc( 64 * 1024, M_AGP, 0, 
					0, ~0, PAGE_SIZE, 0);
		if (!gatt->ag_virtual) {
			if (bootverbose)
				device_printf(dev, "contiguous allocation failed\n");
			free(gatt, M_AGP);
			agp_generic_detach(dev);
			return ENOMEM;
		}
		bzero(gatt->ag_virtual, gatt->ag_entries * sizeof(u_int32_t));
	
		gatt->ag_physical = vtophys((vm_offset_t) gatt->ag_virtual);
		agp_flush_cache();
		/* Install the GATT. */
		WRITE4(AGP_I810_PGTBL_CTL, gatt->ag_physical | 1);
	} else if ( sc->chiptype == CHIP_I830 ) {
		/* The i830 automatically initializes the 128k gatt on boot. */
		unsigned int gcc1, pgtblctl;
		
		gcc1 = pci_read_config(sc->bdev, AGP_I830_GCC1, 1);
		switch (gcc1 & AGP_I830_GCC1_GMS) {
			case AGP_I830_GCC1_GMS_STOLEN_512:
				sc->stolen = (512 - 132) * 1024 / 4096;
				break;
			case AGP_I830_GCC1_GMS_STOLEN_1024: 
				sc->stolen = (1024 - 132) * 1024 / 4096;
				break;
			case AGP_I830_GCC1_GMS_STOLEN_8192: 
				sc->stolen = (8192 - 132) * 1024 / 4096;
				break;
			default:
				sc->stolen = 0;
				device_printf(dev, "unknown memory configuration, disabling\n");
				agp_generic_detach(dev);
				return EINVAL;
		}
		if (sc->stolen > 0)
			device_printf(dev, "detected %dk stolen memory\n", sc->stolen * 4);
		device_printf(dev, "aperture size is %dM\n", sc->initial_aperture / 1024 / 1024);

		/* GATT address is already in there, make sure it's enabled */
		pgtblctl = READ4(AGP_I810_PGTBL_CTL);
		pgtblctl |= 1;
		WRITE4(AGP_I810_PGTBL_CTL, pgtblctl);

		gatt->ag_physical = pgtblctl & ~1;
	} else if (sc->chiptype == CHIP_I855 || sc->chiptype == CHIP_I915) {	/* CHIP_I855 */
		unsigned int gcc1, pgtblctl, stolen;

		/* Stolen memory is set up at the beginning of the aperture by
		 * the BIOS, consisting of the GATT followed by 4kb for the BIOS
		 * display.
		 */
		if (sc->chiptype == CHIP_I855)
			stolen = 132;
		else
			stolen = 260;

		gcc1 = pci_read_config(sc->bdev, AGP_I855_GCC1, 1);
		switch (gcc1 & AGP_I855_GCC1_GMS) {
			case AGP_I855_GCC1_GMS_STOLEN_1M:
				sc->stolen = (1024 - stolen) * 1024 / 4096;
				break;
			case AGP_I855_GCC1_GMS_STOLEN_4M: 
				sc->stolen = (4096 - stolen) * 1024 / 4096;
				break;
			case AGP_I855_GCC1_GMS_STOLEN_8M: 
				sc->stolen = (8192 - stolen) * 1024 / 4096;
				break;
			case AGP_I855_GCC1_GMS_STOLEN_16M: 
				sc->stolen = (16384 - stolen) * 1024 / 4096;
				break;
			case AGP_I855_GCC1_GMS_STOLEN_32M: 
				sc->stolen = (32768 - stolen) * 1024 / 4096;
				break;
			case AGP_I915_GCC1_GMS_STOLEN_48M: 
				sc->stolen = (49152 - stolen) * 1024 / 4096;
				break;
			case AGP_I915_GCC1_GMS_STOLEN_64M: 
				sc->stolen = (65536 - stolen) * 1024 / 4096;
				break;
			default:
				sc->stolen = 0;
				device_printf(dev, "unknown memory configuration, disabling\n");
				agp_generic_detach(dev);
				return EINVAL;
		}
		if (sc->stolen > 0)
			device_printf(dev, "detected %dk stolen memory\n", sc->stolen * 4);
		device_printf(dev, "aperture size is %dM\n", sc->initial_aperture / 1024 / 1024);

		/* GATT address is already in there, make sure it's enabled */
		pgtblctl = READ4(AGP_I810_PGTBL_CTL);
		pgtblctl |= 1;
		WRITE4(AGP_I810_PGTBL_CTL, pgtblctl);

		gatt->ag_physical = pgtblctl & ~1;
	}

	/* Add a device for the drm to attach to */
	if (!device_add_child( dev, "drmsub", -1 ))
		printf("out of memory...\n");

	return bus_generic_attach(dev);
}

static int
agp_i810_detach(device_t dev)
{
	struct agp_i810_softc *sc = device_get_softc(dev);
	int error;
	device_t child;

	error = agp_generic_detach(dev);
	if (error)
		return error;

	/* Clear the GATT base. */
	if ( sc->chiptype == CHIP_I810 ) {
		WRITE4(AGP_I810_PGTBL_CTL, 0);
	} else {
		unsigned int pgtblctl;
		pgtblctl = READ4(AGP_I810_PGTBL_CTL);
		pgtblctl &= ~1;
		WRITE4(AGP_I810_PGTBL_CTL, pgtblctl);
	}

	/* Put the aperture back the way it started. */
	AGP_SET_APERTURE(dev, sc->initial_aperture);

	if ( sc->chiptype == CHIP_I810 ) {
		contigfree(sc->gatt->ag_virtual, 64 * 1024, M_AGP);
	}
	free(sc->gatt, M_AGP);

	if (sc->chiptype == CHIP_I915) {
		bus_release_resource(dev, SYS_RES_MEMORY, AGP_I915_GMADR,
				     sc->gm);
		bus_release_resource(dev, SYS_RES_MEMORY, AGP_I915_GTTADR,
				     sc->gtt);
		bus_release_resource(dev, SYS_RES_MEMORY, AGP_I915_MMADR,
				     sc->regs);
	} else {
		bus_release_resource(dev, SYS_RES_MEMORY, AGP_I810_MMADR,
				     sc->regs);
	}

	child = device_find_child( dev, "drmsub", 0 );
	if (child)
	   device_delete_child( dev, child );

	return 0;
}

static u_int32_t
agp_i810_get_aperture(device_t dev)
{
	struct agp_i810_softc *sc = device_get_softc(dev);
	uint32_t temp;
	u_int16_t miscc;

	switch (sc->chiptype) {
	case CHIP_I810:
		miscc = pci_read_config(sc->bdev, AGP_I810_MISCC, 2);
		if ((miscc & AGP_I810_MISCC_WINSIZE) == AGP_I810_MISCC_WINSIZE_32)
			return 32 * 1024 * 1024;
		else
			return 64 * 1024 * 1024;
	case CHIP_I830:
		temp = pci_read_config(sc->bdev, AGP_I830_GCC1, 2);
		if ((temp & AGP_I830_GCC1_GMASIZE) == AGP_I830_GCC1_GMASIZE_64)
			return 64 * 1024 * 1024;
		else
			return 128 * 1024 * 1024;
	case CHIP_I855:
		return 128 * 1024 * 1024;
	case CHIP_I915:
		/* The documentation states that AGP_I915_MSAC should have bit
		 * 1 set if the aperture is 128MB instead of 256.  However,
		 * that bit appears to not get set, so we instead use the
		 * aperture resource size, which should always be correct.
		 */
		return rman_get_size(sc->gm);
	}

	return 0;
}

static int
agp_i810_set_aperture(device_t dev, u_int32_t aperture)
{
	struct agp_i810_softc *sc = device_get_softc(dev);
	u_int16_t miscc, gcc1;
	u_int32_t temp;

	switch (sc->chiptype) {
	case CHIP_I810:
		/*
		 * Double check for sanity.
		 */
		if (aperture != 32 * 1024 * 1024 && aperture != 64 * 1024 * 1024) {
			device_printf(dev, "bad aperture size %d\n", aperture);
			return EINVAL;
		}

		miscc = pci_read_config(sc->bdev, AGP_I810_MISCC, 2);
		miscc &= ~AGP_I810_MISCC_WINSIZE;
		if (aperture == 32 * 1024 * 1024)
			miscc |= AGP_I810_MISCC_WINSIZE_32;
		else
			miscc |= AGP_I810_MISCC_WINSIZE_64;
	
		pci_write_config(sc->bdev, AGP_I810_MISCC, miscc, 2);
		break;
	case CHIP_I830:
		if (aperture != 64 * 1024 * 1024 &&
		    aperture != 128 * 1024 * 1024) {
			device_printf(dev, "bad aperture size %d\n", aperture);
			return EINVAL;
		}
		gcc1 = pci_read_config(sc->bdev, AGP_I830_GCC1, 2);
		gcc1 &= ~AGP_I830_GCC1_GMASIZE;
		if (aperture == 64 * 1024 * 1024)
			gcc1 |= AGP_I830_GCC1_GMASIZE_64;
		else
			gcc1 |= AGP_I830_GCC1_GMASIZE_128;

		pci_write_config(sc->bdev, AGP_I830_GCC1, gcc1, 2);
		break;
	case CHIP_I855:
		if (aperture != 128 * 1024 * 1024) {
			device_printf(dev, "bad aperture size %d\n", aperture);
			return EINVAL;
		}
		break;
	case CHIP_I915:
		temp = pci_read_config(dev, AGP_I915_MSAC, 1);
		temp &= ~AGP_I915_MSAC_GMASIZE;

		switch (aperture) {
		case 128 * 1024 * 1024:
			temp |= AGP_I915_MSAC_GMASIZE_128;
			break;
		case 256 * 1024 * 1024:
			temp |= AGP_I915_MSAC_GMASIZE_256;
			break;
		default:
			device_printf(dev, "bad aperture size %d\n", aperture);
			return EINVAL;
		}

		pci_write_config(dev, AGP_I915_MSAC, temp, 1);
		break;
	}

	return 0;
}

static int
agp_i810_bind_page(device_t dev, int offset, vm_offset_t physical)
{
	struct agp_i810_softc *sc = device_get_softc(dev);

	if (offset < 0 || offset >= (sc->gatt->ag_entries << AGP_PAGE_SHIFT)) {
		device_printf(dev, "failed: offset is 0x%08x, shift is %d, entries is %d\n", offset, AGP_PAGE_SHIFT, sc->gatt->ag_entries);
		return EINVAL;
	}

	if ( sc->chiptype != CHIP_I810 ) {
		if ( (offset >> AGP_PAGE_SHIFT) < sc->stolen ) {
			device_printf(dev, "trying to bind into stolen memory");
			return EINVAL;
		}
	}

	if (sc->chiptype == CHIP_I915) {
		WRITEGTT((offset >> AGP_PAGE_SHIFT) * 4, physical | 1);
	} else {
		WRITE4(AGP_I810_GTT + (offset >> AGP_PAGE_SHIFT) * 4, physical | 1);
	}

	return 0;
}

static int
agp_i810_unbind_page(device_t dev, int offset)
{
	struct agp_i810_softc *sc = device_get_softc(dev);

	if (offset < 0 || offset >= (sc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return EINVAL;

	if ( sc->chiptype != CHIP_I810 ) {
		if ( (offset >> AGP_PAGE_SHIFT) < sc->stolen ) {
			device_printf(dev, "trying to unbind from stolen memory");
			return EINVAL;
		}
	}

	if (sc->chiptype == CHIP_I915) {
		WRITEGTT((offset >> AGP_PAGE_SHIFT) * 4, 0);
	} else {
		WRITE4(AGP_I810_GTT + (offset >> AGP_PAGE_SHIFT) * 4, 0);
	}
	
	return 0;
}

/*
 * Writing via memory mapped registers already flushes all TLBs.
 */
static void
agp_i810_flush_tlb(device_t dev)
{
}

static int
agp_i810_enable(device_t dev, u_int32_t mode)
{

	return 0;
}

static struct agp_memory *
agp_i810_alloc_memory(device_t dev, int type, vm_size_t size)
{
	struct agp_i810_softc *sc = device_get_softc(dev);
	struct agp_memory *mem;

	if ((size & (AGP_PAGE_SIZE - 1)) != 0)
		return 0;

	if (sc->agp.as_allocated + size > sc->agp.as_maxmem)
		return 0;

	if (type == 1) {
		/*
		 * Mapping local DRAM into GATT.
		 */
		if ( sc->chiptype != CHIP_I810 )
			return 0;
		if (size != sc->dcache_size)
			return 0;
	} else if (type == 2) {
		/*
		 * Bogus mapping of a single page for the hardware cursor.
		 */
		if (size != AGP_PAGE_SIZE)
			return 0;
	}

	mem = malloc(sizeof *mem, M_AGP, M_WAITOK);
	mem->am_id = sc->agp.as_nextid++;
	mem->am_size = size;
	mem->am_type = type;
	if (type != 1)
		mem->am_obj = vm_object_allocate(OBJT_DEFAULT,
						 atop(round_page(size)));
	else
		mem->am_obj = 0;

	if (type == 2) {
		/*
		 * Allocate and wire down the page now so that we can
		 * get its physical address.
		 */
		vm_page_t m;

		VM_OBJECT_LOCK(mem->am_obj);
		m = vm_page_grab(mem->am_obj, 0, VM_ALLOC_NOBUSY |
		    VM_ALLOC_WIRED | VM_ALLOC_ZERO | VM_ALLOC_RETRY);
		VM_OBJECT_UNLOCK(mem->am_obj);
		mem->am_physical = VM_PAGE_TO_PHYS(m);
	} else {
		mem->am_physical = 0;
	}

	mem->am_offset = 0;
	mem->am_is_bound = 0;
	TAILQ_INSERT_TAIL(&sc->agp.as_memory, mem, am_link);
	sc->agp.as_allocated += size;

	return mem;
}

static int
agp_i810_free_memory(device_t dev, struct agp_memory *mem)
{
	struct agp_i810_softc *sc = device_get_softc(dev);

	if (mem->am_is_bound)
		return EBUSY;

	if (mem->am_type == 2) {
		/*
		 * Unwire the page which we wired in alloc_memory.
		 */
		vm_page_t m;

		VM_OBJECT_LOCK(mem->am_obj);
		m = vm_page_lookup(mem->am_obj, 0);
		VM_OBJECT_UNLOCK(mem->am_obj);
		vm_page_lock_queues();
		vm_page_unwire(m, 0);
		vm_page_unlock_queues();
	}

	sc->agp.as_allocated -= mem->am_size;
	TAILQ_REMOVE(&sc->agp.as_memory, mem, am_link);
	if (mem->am_obj)
		vm_object_deallocate(mem->am_obj);
	free(mem, M_AGP);
	return 0;
}

static int
agp_i810_bind_memory(device_t dev, struct agp_memory *mem,
		     vm_offset_t offset)
{
	struct agp_i810_softc *sc = device_get_softc(dev);
	vm_offset_t i;

	if (mem->am_type != 1)
		return agp_generic_bind_memory(dev, mem, offset);

	if ( sc->chiptype != CHIP_I810 )
		return EINVAL;

	for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE) {
		WRITE4(AGP_I810_GTT + (offset >> AGP_PAGE_SHIFT) * 4,
		       i | 3);
	}

	return 0;
}

static int
agp_i810_unbind_memory(device_t dev, struct agp_memory *mem)
{
	struct agp_i810_softc *sc = device_get_softc(dev);
	vm_offset_t i;

	if (mem->am_type != 1)
		return agp_generic_unbind_memory(dev, mem);

	if ( sc->chiptype != CHIP_I810 )
		return EINVAL;

	for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE)
		WRITE4(AGP_I810_GTT + (i >> AGP_PAGE_SHIFT) * 4, 0);

	return 0;
}

static int
agp_i810_print_child(device_t dev, device_t child)
{
	int retval = 0;

	retval += bus_print_child_header(dev, child);
	retval += printf(": (child of agp_i810.c)");
	retval += bus_print_child_footer(dev, child);

	return retval;
}

static device_method_t agp_i810_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		agp_i810_probe),
	DEVMETHOD(device_attach,	agp_i810_attach),
	DEVMETHOD(device_detach,	agp_i810_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* AGP interface */
	DEVMETHOD(agp_get_aperture,	agp_i810_get_aperture),
	DEVMETHOD(agp_set_aperture,	agp_i810_set_aperture),
	DEVMETHOD(agp_bind_page,	agp_i810_bind_page),
	DEVMETHOD(agp_unbind_page,	agp_i810_unbind_page),
	DEVMETHOD(agp_flush_tlb,	agp_i810_flush_tlb),
	DEVMETHOD(agp_enable,		agp_i810_enable),
	DEVMETHOD(agp_alloc_memory,	agp_i810_alloc_memory),
	DEVMETHOD(agp_free_memory,	agp_i810_free_memory),
	DEVMETHOD(agp_bind_memory,	agp_i810_bind_memory),
	DEVMETHOD(agp_unbind_memory,	agp_i810_unbind_memory),

	/* bus methods */
	DEVMETHOD(bus_print_child,	agp_i810_print_child),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	{ 0, 0 }
};

static driver_t agp_i810_driver = {
	"agp",
	agp_i810_methods,
	sizeof(struct agp_i810_softc),
};

static devclass_t agp_devclass;

DRIVER_MODULE(agp_i810, pci, agp_i810_driver, agp_devclass, 0, 0);
MODULE_DEPEND(agp_i810, agp, 1, 1, 1);
MODULE_DEPEND(agp_i810, pci, 1, 1, 1);
