/* $MidnightBSD$ */
/*-
 * Copyright (c) 2011-2014 Nathan Whitehorn
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
__FBSDID("$FreeBSD: stable/10/sys/powerpc/ps3/ps3_syscons.c 271769 2014-09-18 14:38:18Z dumbbell $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/limits.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/fbio.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/platform.h>
#include <machine/pmap.h>

#include <dev/vt/vt.h>
#include <dev/vt/hw/fb/vt_fb.h>
#include <dev/vt/colors/vt_termcolors.h>

#include "ps3-hvcall.h"

#define PS3FB_SIZE (4*1024*1024)

#define L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_MODE_SET	0x0100
#define L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_SYNC		0x0101
#define  L1GPU_DISPLAY_SYNC_HSYNC			1
#define  L1GPU_DISPLAY_SYNC_VSYNC			2
#define L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_FLIP		0x0102

static vd_init_t ps3fb_init;
static vd_probe_t ps3fb_probe;
void ps3fb_remap(void);

struct ps3fb_softc {
	struct fb_info	fb_info;

	uint64_t	sc_fbhandle;
	uint64_t	sc_fbcontext;
	uint64_t	sc_dma_control;
	uint64_t	sc_driver_info;
	uint64_t	sc_reports;
	uint64_t	sc_reports_size;
};

static struct vt_driver vt_ps3fb_driver = {
	.vd_name = "ps3fb",
	.vd_probe = ps3fb_probe,
	.vd_init = ps3fb_init,
	.vd_blank = vt_fb_blank,
	.vd_bitblt_text = vt_fb_bitblt_text,
	.vd_bitblt_bmp = vt_fb_bitblt_bitmap,
	.vd_drawrect = vt_fb_drawrect,
	.vd_setpixel = vt_fb_setpixel,
	.vd_fb_ioctl = vt_fb_ioctl,
	.vd_fb_mmap = vt_fb_mmap,
	/* Better than VGA, but still generic driver. */
	.vd_priority = VD_PRIORITY_GENERIC + 1,
};

VT_DRIVER_DECLARE(vt_ps3fb, vt_ps3fb_driver);
static struct ps3fb_softc ps3fb_softc;

static int
ps3fb_probe(struct vt_device *vd)
{
	struct ps3fb_softc *sc;
	int disable;
	char compatible[64];
#if 0
	phandle_t root;
#endif

	disable = 0;
	TUNABLE_INT_FETCH("hw.syscons.disable", &disable);
	if (disable != 0)
		return (0);

	sc = &ps3fb_softc;

#if 0
	root = OF_finddevice("/");
	if (OF_getprop(root, "compatible", compatible, sizeof(compatible)) <= 0)
                return (0);
	
	if (strncmp(compatible, "sony,ps3", sizeof(compatible)) != 0)
		return (0);
#else
	TUNABLE_STR_FETCH("hw.platform", compatible, sizeof(compatible));
	if (strcmp(compatible, "ps3") != 0)
		return (CN_DEAD);
#endif

	return (CN_INTERNAL);
}

void
ps3fb_remap(void)
{
	struct ps3fb_softc *sc;
	vm_offset_t va, fb_paddr;

	sc = &ps3fb_softc;

	lv1_gpu_close();
	lv1_gpu_open(0);

	lv1_gpu_context_attribute(0, L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_MODE_SET,
	    0,0,0,0);
	lv1_gpu_context_attribute(0, L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_MODE_SET,
	    0,0,1,0);
	lv1_gpu_context_attribute(0, L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_SYNC,
	    0,L1GPU_DISPLAY_SYNC_VSYNC,0,0);
	lv1_gpu_context_attribute(0, L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_SYNC,
	    1,L1GPU_DISPLAY_SYNC_VSYNC,0,0);
	lv1_gpu_memory_allocate(PS3FB_SIZE, 0, 0, 0, 0, &sc->sc_fbhandle,
	    &fb_paddr);
	lv1_gpu_context_allocate(sc->sc_fbhandle, 0, &sc->sc_fbcontext,
	    &sc->sc_dma_control, &sc->sc_driver_info, &sc->sc_reports,
	    &sc->sc_reports_size);

	lv1_gpu_context_attribute(sc->sc_fbcontext,
	    L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_FLIP, 0, 0, 0, 0);
	lv1_gpu_context_attribute(sc->sc_fbcontext,
	    L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_FLIP, 1, 0, 0, 0);

	sc->fb_info.fb_pbase = fb_paddr;
	for (va = 0; va < PS3FB_SIZE; va += PAGE_SIZE)
		pmap_kenter_attr(0x10000000 + va, fb_paddr + va,
		    VM_MEMATTR_WRITE_COMBINING); 
}

static int
ps3fb_init(struct vt_device *vd)
{
	struct ps3fb_softc *sc;

	/* Init softc */
	vd->vd_softc = sc = &ps3fb_softc;

	/* XXX: get from HV repository */
	sc->fb_info.fb_depth = 32;
	sc->fb_info.fb_height = 480;
	sc->fb_info.fb_width = 720;
	sc->fb_info.fb_stride = sc->fb_info.fb_width*4;
	sc->fb_info.fb_size = sc->fb_info.fb_height * sc->fb_info.fb_stride;
	sc->fb_info.fb_bpp = sc->fb_info.fb_stride / sc->fb_info.fb_width * 8;

	/*
	 * The loader puts the FB at 0x10000000, so use that for now.
	 */

	sc->fb_info.fb_vbase = 0x10000000;

	/* 32-bit VGA palette */
	vt_generate_cons_palette(sc->fb_info.fb_cmap, COLOR_FORMAT_RGB,
	    255, 0, 255, 8, 255, 16);

	/* Set correct graphics context */
	lv1_gpu_context_attribute(sc->sc_fbcontext,
	    L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_FLIP, 0, 0, 0, 0);
	lv1_gpu_context_attribute(sc->sc_fbcontext,
	    L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_FLIP, 1, 0, 0, 0);

	vt_fb_init(vd);
	sc->fb_info.fb_flags &= ~FB_FLAG_NOMMAP; /* Set wrongly by vt_fb_init */

	return (CN_INTERNAL);
}

