/*-
 * Copyright (c) 1998 Kazutaka YOKOTA and Michael Smith
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/i386/isa/vesa.c 153110 2005-12-05 11:58:35Z ru $");

#include "opt_vga.h"
#include "opt_vesa.h"

#ifndef VGA_NO_MODE_CHANGE

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/fbio.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <machine/md_var.h>
#include <machine/vm86.h>
#include <machine/pc/bios.h>
#include <machine/pc/vesa.h>

#include <dev/fb/fbreg.h>
#include <dev/fb/vgareg.h>

#ifndef __i386__
#include <isa/isareg.h>
#else
#include <i386/isa/isa.h>
#endif

#define	VESA_VIA_CLE266		"VIA CLE266\r\n"

#ifndef VESA_DEBUG
#define VESA_DEBUG	0
#endif

/* VESA video adapter state buffer stub */
struct adp_state {
	int		sig;
#define V_STATE_SIG	0x61736576
	u_char		regs[1];
};
typedef struct adp_state adp_state_t;

/* VESA video adapter */
static video_adapter_t *vesa_adp = NULL;
static int vesa_state_buf_size = 0;
#define VESA_VM86_BUFSIZE	(3 * PAGE_SIZE)
static void *vesa_vm86_buf;

/* VESA functions */
#if 0
static int			vesa_nop(void);
#endif
static int			vesa_error(void);
static vi_probe_t		vesa_probe;
static vi_init_t		vesa_init;
static vi_get_info_t		vesa_get_info;
static vi_query_mode_t		vesa_query_mode;
static vi_set_mode_t		vesa_set_mode;
static vi_save_font_t		vesa_save_font;
static vi_load_font_t		vesa_load_font;
static vi_show_font_t		vesa_show_font;
static vi_save_palette_t	vesa_save_palette;
static vi_load_palette_t	vesa_load_palette;
static vi_set_border_t		vesa_set_border;
static vi_save_state_t		vesa_save_state;
static vi_load_state_t		vesa_load_state;
static vi_set_win_org_t		vesa_set_origin;
static vi_read_hw_cursor_t	vesa_read_hw_cursor;
static vi_set_hw_cursor_t	vesa_set_hw_cursor;
static vi_set_hw_cursor_shape_t	vesa_set_hw_cursor_shape;
static vi_blank_display_t	vesa_blank_display;
static vi_mmap_t		vesa_mmap;
static vi_ioctl_t		vesa_ioctl;
static vi_clear_t		vesa_clear;
static vi_fill_rect_t		vesa_fill_rect;
static vi_bitblt_t		vesa_bitblt;
static vi_diag_t		vesa_diag;
static int			vesa_bios_info(int level);
static struct vm86context	vesa_vmcontext;

static video_switch_t vesavidsw = {
	vesa_probe,
	vesa_init,
	vesa_get_info,
	vesa_query_mode,
	vesa_set_mode,
	vesa_save_font,
	vesa_load_font,
	vesa_show_font,
	vesa_save_palette,
	vesa_load_palette,
	vesa_set_border,
	vesa_save_state,
	vesa_load_state,
	vesa_set_origin,
	vesa_read_hw_cursor,
	vesa_set_hw_cursor,
	vesa_set_hw_cursor_shape,
	vesa_blank_display,
	vesa_mmap,
	vesa_ioctl,
	vesa_clear,
	vesa_fill_rect,
	vesa_bitblt,
	vesa_error,
	vesa_error,
	vesa_diag,
};

static video_switch_t *prevvidsw;

/* VESA BIOS video modes */
#define VESA_MAXMODES	64
#define EOT		(-1)
#define NA		(-2)

#define MODE_TABLE_DELTA 8

static int vesa_vmode_max = 0;
static video_info_t vesa_vmode_empty = { EOT };
static video_info_t *vesa_vmode = &vesa_vmode_empty;

static int vesa_init_done = FALSE;
static int has_vesa_bios = FALSE;
static struct vesa_info *vesa_adp_info = NULL;
static u_int16_t *vesa_vmodetab = NULL;
static char *vesa_oemstr = NULL;
static char *vesa_venderstr = NULL;
static char *vesa_prodstr = NULL;
static char *vesa_revstr = NULL;

/* local macros and functions */
#define BIOS_SADDRTOLADDR(p) ((((p) & 0xffff0000) >> 12) + ((p) & 0x0000ffff))

static int int10_set_mode(int mode);
static int vesa_bios_get_mode(int mode, struct vesa_mode *vmode);
static int vesa_bios_set_mode(int mode);
static int vesa_bios_get_dac(void);
static int vesa_bios_set_dac(int bits);
static int vesa_bios_save_palette(int start, int colors, u_char *palette,
				  int bits);
static int vesa_bios_save_palette2(int start, int colors, u_char *r, u_char *g,
				   u_char *b, int bits);
static int vesa_bios_load_palette(int start, int colors, u_char *palette,
				  int bits);
#ifdef notyet
static int vesa_bios_load_palette2(int start, int colors, u_char *r, u_char *g,
				   u_char *b, int bits);
#endif
#define STATE_SIZE	0
#define STATE_SAVE	1
#define STATE_LOAD	2
#define STATE_HW	(1<<0)
#define STATE_DATA	(1<<1)
#define STATE_DAC	(1<<2)
#define STATE_REG	(1<<3)
#define STATE_MOST	(STATE_HW | STATE_DATA | STATE_REG)
#define STATE_ALL	(STATE_HW | STATE_DATA | STATE_DAC | STATE_REG)
static int vesa_bios_state_buf_size(void);
static int vesa_bios_save_restore(int code, void *p, size_t size);
static int vesa_bios_get_line_length(void);
static int vesa_bios_set_line_length(int pixel, int *bytes, int *lines);
#if 0
static int vesa_bios_get_start(int *x, int *y);
#endif
static int vesa_bios_set_start(int x, int y);
static int vesa_map_gen_mode_num(int type, int color, int mode);
static int vesa_translate_flags(u_int16_t vflags);
static int vesa_translate_mmodel(u_int8_t vmodel);
static void *vesa_fix_ptr(u_int32_t p, u_int16_t seg, u_int16_t off, 
			  u_char *buf);
static int vesa_bios_init(void);
static void vesa_clear_modes(video_info_t *info, int color);
static vm_offset_t vesa_map_buffer(u_int paddr, size_t size);
static void vesa_unmap_buffer(vm_offset_t vaddr, size_t size);

#if 0
static int vesa_get_origin(video_adapter_t *adp, off_t *offset);
#endif

static void
dump_buffer(u_char *buf, size_t len)
{
    int i;

    for(i = 0; i < len;) {
	printf("%02x ", buf[i]);
	if ((++i % 16) == 0)
	    printf("\n");
    }
}

/* INT 10 BIOS calls */
static int
int10_set_mode(int mode)
{
	struct vm86frame vmf;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x0000 | mode;
	vm86_intcall(0x10, &vmf);
	return 0;
}

/* VESA BIOS calls */
static int
vesa_bios_get_mode(int mode, struct vesa_mode *vmode)
{
	struct vm86frame vmf;
	u_char *buf;
	int err;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f01; 
	vmf.vmf_ecx = mode;
	buf = vesa_vm86_buf;
	vm86_getptr(&vesa_vmcontext, (vm_offset_t)buf, &vmf.vmf_es, &vmf.vmf_di);

	err = vm86_datacall(0x10, &vmf, &vesa_vmcontext);
	if ((err != 0) || (vmf.vmf_ax != 0x4f))
		return 1;
	bcopy(buf, vmode, sizeof(*vmode));
	return 0;
}

static int
vesa_bios_set_mode(int mode)
{
	struct vm86frame vmf;
	int err;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f02;
	vmf.vmf_ebx = mode;
	err = vm86_intcall(0x10, &vmf);
	return ((err != 0) || (vmf.vmf_ax != 0x4f));
}

static int
vesa_bios_get_dac(void)
{
	struct vm86frame vmf;
	int err;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f08;
	vmf.vmf_ebx = 1;	/* get DAC width */
	err = vm86_intcall(0x10, &vmf);
	if ((err != 0) || (vmf.vmf_ax != 0x4f))
		return 6;	/* XXX */
	return ((vmf.vmf_ebx >> 8) & 0x00ff);
}

static int
vesa_bios_set_dac(int bits)
{
	struct vm86frame vmf;
	int err;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f08;
	vmf.vmf_ebx = (bits << 8);
	err = vm86_intcall(0x10, &vmf);
	if ((err != 0) || (vmf.vmf_ax != 0x4f))
		return 6;	/* XXX */
	return ((vmf.vmf_ebx >> 8) & 0x00ff);
}

static int
vesa_bios_save_palette(int start, int colors, u_char *palette, int bits)
{
	struct vm86frame vmf;
	u_char *p;
	int err;
	int i;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f09;
	vmf.vmf_ebx = 1;	/* get primary palette data */
	vmf.vmf_ecx = colors;
	vmf.vmf_edx = start;
	p = vesa_vm86_buf;
	vm86_getptr(&vesa_vmcontext, (vm_offset_t)p, &vmf.vmf_es, &vmf.vmf_di);

	err = vm86_datacall(0x10, &vmf, &vesa_vmcontext);
	if ((err != 0) || (vmf.vmf_ax != 0x4f))
		return 1;

	bits = 8 - bits;
	for (i = 0; i < colors; ++i) {
		palette[i*3]     = p[i*4 + 2] << bits;
		palette[i*3 + 1] = p[i*4 + 1] << bits;
		palette[i*3 + 2] = p[i*4] << bits;
	}
	return 0;
}

static int
vesa_bios_save_palette2(int start, int colors, u_char *r, u_char *g, u_char *b,
			int bits)
{
	struct vm86frame vmf;
	u_char *p;
	int err;
	int i;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f09;
	vmf.vmf_ebx = 1;	/* get primary palette data */
	vmf.vmf_ecx = colors;
	vmf.vmf_edx = start;
	p = vesa_vm86_buf;
	vm86_getptr(&vesa_vmcontext, (vm_offset_t)p, &vmf.vmf_es, &vmf.vmf_di);

	err = vm86_datacall(0x10, &vmf, &vesa_vmcontext);
	if ((err != 0) || (vmf.vmf_ax != 0x4f))
		return 1;

	bits = 8 - bits;
	for (i = 0; i < colors; ++i) {
		r[i] = p[i*4 + 2] << bits;
		g[i] = p[i*4 + 1] << bits;
		b[i] = p[i*4] << bits;
	}
	return 0;
}

static int
vesa_bios_load_palette(int start, int colors, u_char *palette, int bits)
{
	struct vm86frame vmf;
	u_char *p;
	int err;
	int i;

	p = vesa_vm86_buf;
	bits = 8 - bits;
	for (i = 0; i < colors; ++i) {
		p[i*4]	   = palette[i*3 + 2] >> bits;
		p[i*4 + 1] = palette[i*3 + 1] >> bits;
		p[i*4 + 2] = palette[i*3] >> bits;
		p[i*4 + 3] = 0;
	}

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f09;
	vmf.vmf_ebx = 0;	/* set primary palette data */
	vmf.vmf_ecx = colors;
	vmf.vmf_edx = start;
	vm86_getptr(&vesa_vmcontext, (vm_offset_t)p, &vmf.vmf_es, &vmf.vmf_di);

	err = vm86_datacall(0x10, &vmf, &vesa_vmcontext);
	return ((err != 0) || (vmf.vmf_ax != 0x4f));
}

#ifdef notyet
static int
vesa_bios_load_palette2(int start, int colors, u_char *r, u_char *g, u_char *b,
			int bits)
{
	struct vm86frame vmf;
	u_char *p;
	int err;
	int i;

	p = vesa_vm86_buf;
	bits = 8 - bits;
	for (i = 0; i < colors; ++i) {
		p[i*4]	   = b[i] >> bits;
		p[i*4 + 1] = g[i] >> bits;
		p[i*4 + 2] = r[i] >> bits;
		p[i*4 + 3] = 0;
	}

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f09;
	vmf.vmf_ebx = 0;	/* set primary palette data */
	vmf.vmf_ecx = colors;
	vmf.vmf_edx = start;
	vm86_getptr(&vesa_vmcontext, (vm_offset_t)p, &vmf.vmf_es, &vmf.vmf_di);

	err = vm86_datacall(0x10, &vmf, &vesa_vmcontext);
	return ((err != 0) || (vmf.vmf_ax != 0x4f));
}
#endif

static int
vesa_bios_state_buf_size(void)
{
	struct vm86frame vmf;
	int err;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f04; 
	vmf.vmf_ecx = STATE_ALL;
	vmf.vmf_edx = STATE_SIZE;
	err = vm86_intcall(0x10, &vmf);
	if ((err != 0) || (vmf.vmf_ax != 0x4f))
		return 0;
	return vmf.vmf_bx*64;
}

static int
vesa_bios_save_restore(int code, void *p, size_t size)
{
	struct vm86frame vmf;
	u_char *buf;
	int err;

	if (size > VESA_VM86_BUFSIZE)
		return (1);

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f04; 
	vmf.vmf_ecx = STATE_ALL;
	vmf.vmf_edx = code;	/* STATE_SAVE/STATE_LOAD */
	buf = vesa_vm86_buf;
	vm86_getptr(&vesa_vmcontext, (vm_offset_t)buf, &vmf.vmf_es, &vmf.vmf_bx);
	bcopy(p, buf, size);

	err = vm86_datacall(0x10, &vmf, &vesa_vmcontext);
	bcopy(buf, p, size);
	return ((err != 0) || (vmf.vmf_ax != 0x4f));
}

static int
vesa_bios_get_line_length(void)
{
	struct vm86frame vmf;
	int err;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f06; 
	vmf.vmf_ebx = 1;	/* get scan line length */
	err = vm86_intcall(0x10, &vmf);
	if ((err != 0) || (vmf.vmf_ax != 0x4f))
		return -1;
	return vmf.vmf_bx;	/* line length in bytes */
}

static int
vesa_bios_set_line_length(int pixel, int *bytes, int *lines)
{
	struct vm86frame vmf;
	int err;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f06; 
	vmf.vmf_ebx = 0;	/* set scan line length in pixel */
	vmf.vmf_ecx = pixel;
	err = vm86_intcall(0x10, &vmf);
#if VESA_DEBUG > 1
	printf("bx:%d, cx:%d, dx:%d\n", vmf.vmf_bx, vmf.vmf_cx, vmf.vmf_dx); 
#endif
	if ((err != 0) || (vmf.vmf_ax != 0x4f))
		return 1;
	if (bytes)
		*bytes = vmf.vmf_bx;
	if (lines)
		*lines = vmf.vmf_dx;
	return 0;
}

#if 0
static int
vesa_bios_get_start(int *x, int *y)
{
	struct vm86frame vmf;
	int err;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f07; 
	vmf.vmf_ebx = 1;	/* get display start */
	err = vm86_intcall(0x10, &vmf);
	if ((err != 0) || (vmf.vmf_ax != 0x4f))
		return 1;
	*x = vmf.vmf_cx;
	*y = vmf.vmf_dx;
	return 0;
}
#endif

static int
vesa_bios_set_start(int x, int y)
{
	struct vm86frame vmf;
	int err;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f07; 
	vmf.vmf_ebx = 0x80;	/* set display start */
	vmf.vmf_edx = y;
	vmf.vmf_ecx = x;
	err = vm86_intcall(0x10, &vmf);
	return ((err != 0) || (vmf.vmf_ax != 0x4f));
}

/* map a generic video mode to a known mode */
static int
vesa_map_gen_mode_num(int type, int color, int mode)
{
    static struct {
	int from;
	int to;
    } mode_map[] = {
	{ M_TEXT_132x25, M_VESA_C132x25 },
	{ M_TEXT_132x43, M_VESA_C132x43 },
	{ M_TEXT_132x50, M_VESA_C132x50 },
	{ M_TEXT_132x60, M_VESA_C132x60 },
    };
    int i;

    for (i = 0; i < sizeof(mode_map)/sizeof(mode_map[0]); ++i) {
        if (mode_map[i].from == mode)
            return mode_map[i].to;
    }
    return mode;
}

static int
vesa_translate_flags(u_int16_t vflags)
{
	static struct {
		u_int16_t mask;
		int set;
		int reset;
	} ftable[] = {
		{ V_MODECOLOR, V_INFO_COLOR, 0 },
		{ V_MODEGRAPHICS, V_INFO_GRAPHICS, 0 },
		{ V_MODELFB, V_INFO_LINEAR, 0 },
	};
	int flags;
	int i;

	for (flags = 0, i = 0; i < sizeof(ftable)/sizeof(ftable[0]); ++i) {
		flags |= (vflags & ftable[i].mask) ? 
			 ftable[i].set : ftable[i].reset;
	}
	return flags;
}

static int
vesa_translate_mmodel(u_int8_t vmodel)
{
	static struct {
		u_int8_t vmodel;
		int mmodel;
	} mtable[] = {
		{ V_MMTEXT,	V_INFO_MM_TEXT },
		{ V_MMCGA,	V_INFO_MM_CGA },
		{ V_MMHGC,	V_INFO_MM_HGC },
		{ V_MMEGA,	V_INFO_MM_PLANAR },
		{ V_MMPACKED,	V_INFO_MM_PACKED },
		{ V_MMDIRCOLOR,	V_INFO_MM_DIRECT },
	};
	int i;

	for (i = 0; mtable[i].mmodel >= 0; ++i) {
		if (mtable[i].vmodel == vmodel)
			return mtable[i].mmodel;
	}
	return V_INFO_MM_OTHER;
}

static void
*vesa_fix_ptr(u_int32_t p, u_int16_t seg, u_int16_t off, u_char *buf)
{
	if (p == 0)
		return NULL;
	if (((p >> 16) == seg) && ((p & 0xffff) >= off))
		return (void *)(buf + ((p & 0xffff) - off));
	else {
		p = BIOS_SADDRTOLADDR(p);
		return (void *)BIOS_PADDRTOVADDR(p);
	}
}

static int
vesa_bios_init(void)
{
	static u_char buf[512];
	struct vm86frame vmf;
	struct vesa_mode vmode;
	video_info_t *p;
	u_char *vmbuf;
	int is_via_cle266;
	int modes;
	int err;
	int i;

	if (vesa_init_done)
		return 0;

	has_vesa_bios = FALSE;
	vesa_adp_info = NULL;
	vesa_vmode_max = 0;
	vesa_vmode[0].vi_mode = EOT;

	/* Allocate a buffer and add each page to the vm86 context. */
	vesa_vm86_buf = malloc(VESA_VM86_BUFSIZE, M_DEVBUF, M_WAITOK | M_ZERO);
	KASSERT(((vm_offset_t)vesa_vm86_buf & PAGE_MASK) == 0,
	    ("bad vesa_vm86_buf alignment"));
	for (i = 0; i < howmany(VESA_VM86_BUFSIZE, PAGE_SIZE); i++)
		vm86_addpage(&vesa_vmcontext, i + 1,
		    (vm_offset_t)vesa_vm86_buf + PAGE_SIZE * i);

	vmbuf = vesa_vm86_buf;
	bzero(&vmf, sizeof(vmf));	/* paranoia */
	bcopy("VBE2", vmbuf, 4);	/* try for VBE2 data */
	vmf.vmf_eax = 0x4f00;
	vm86_getptr(&vesa_vmcontext, (vm_offset_t)vmbuf, &vmf.vmf_es, &vmf.vmf_di);

	err = vm86_datacall(0x10, &vmf, &vesa_vmcontext);
	if ((err != 0) || (vmf.vmf_ax != 0x4f) || bcmp("VESA", vmbuf, 4))
		return 1;
	bcopy(vmbuf, buf, sizeof(buf));
	vesa_adp_info = (struct vesa_info *)buf;
	if (bootverbose) {
		printf("VESA: information block\n");
		dump_buffer(buf, 64);
	}
	if (vesa_adp_info->v_version < 0x0102) {
		printf("VESA: VBE version %d.%d is not supported; "
		       "version 1.2 or later is required.\n",
		       ((vesa_adp_info->v_version & 0xf000) >> 12) * 10 
			   + ((vesa_adp_info->v_version & 0x0f00) >> 8),
		       ((vesa_adp_info->v_version & 0x00f0) >> 4) * 10 
			   + (vesa_adp_info->v_version & 0x000f));
		return 1;
	}

	/* fix string ptrs */
	vesa_oemstr = (char *)vesa_fix_ptr(vesa_adp_info->v_oemstr,
					   vmf.vmf_es, vmf.vmf_di, buf);
	is_via_cle266 = strcmp(vesa_oemstr, VESA_VIA_CLE266) == 0;

	if (vesa_adp_info->v_version >= 0x0200) {
		vesa_venderstr = 
		    (char *)vesa_fix_ptr(vesa_adp_info->v_venderstr,
					 vmf.vmf_es, vmf.vmf_di, buf);
		vesa_prodstr = 
		    (char *)vesa_fix_ptr(vesa_adp_info->v_prodstr,
					 vmf.vmf_es, vmf.vmf_di, buf);
		vesa_revstr = 
		    (char *)vesa_fix_ptr(vesa_adp_info->v_revstr,
					 vmf.vmf_es, vmf.vmf_di, buf);
	}

	/* obtain video mode information */
	vesa_vmodetab = (u_int16_t *)vesa_fix_ptr(vesa_adp_info->v_modetable,
						  vmf.vmf_es, vmf.vmf_di, buf);
	if (vesa_vmodetab == NULL)
		return 1;
	for (i = 0, modes = 0; 
		(i < (M_VESA_MODE_MAX - M_VESA_BASE + 1))
		&& (vesa_vmodetab[i] != 0xffff); ++i) {
		if (vesa_bios_get_mode(vesa_vmodetab[i], &vmode))
			continue;

		/* reject unsupported modes */
#if 0
		if ((vmode.v_modeattr & (V_MODESUPP | V_MODEOPTINFO 
					| V_MODENONVGA))
		    != (V_MODESUPP | V_MODEOPTINFO))
			continue;
#else
		if ((vmode.v_modeattr & V_MODEOPTINFO) == 0) {
#if VESA_DEBUG > 1
			printf(
		"Rejecting VESA %s mode: %d x %d x %d bpp  attr = %x\n",
			    vmode.v_modeattr & V_MODEGRAPHICS ? "graphics" : "text",
			    vmode.v_width, vmode.v_height, vmode.v_bpp,
			    vmode.v_modeattr);
#endif
			continue;
		}
#endif

		/* expand the array if necessary */
		if (modes >= vesa_vmode_max) {
			vesa_vmode_max += MODE_TABLE_DELTA;
			p = malloc(sizeof(*vesa_vmode)*(vesa_vmode_max + 1),
				   M_DEVBUF, M_WAITOK);
#if VESA_DEBUG > 1
			printf("vesa_bios_init(): modes:%d, vesa_mode_max:%d\n",
			       modes, vesa_vmode_max);
#endif
			if (modes > 0) {
				bcopy(vesa_vmode, p, sizeof(*vesa_vmode)*modes);
				free(vesa_vmode, M_DEVBUF);
			}
			vesa_vmode = p;
		}

#if VESA_DEBUG > 1
		printf("Found VESA %s mode: %d x %d x %d bpp\n",
		    vmode.v_modeattr & V_MODEGRAPHICS ? "graphics" : "text",
		    vmode.v_width, vmode.v_height, vmode.v_bpp);
#endif
		if (is_via_cle266) {
		    if ((vmode.v_width & 0xff00) >> 8 == vmode.v_height - 1) {
			vmode.v_width &= 0xff;
			vmode.v_waseg = 0xb8000 >> 4;
		    }
		}

		/* copy some fields */
		bzero(&vesa_vmode[modes], sizeof(vesa_vmode[modes]));
		vesa_vmode[modes].vi_mode = vesa_vmodetab[i];
		vesa_vmode[modes].vi_width = vmode.v_width;
		vesa_vmode[modes].vi_height = vmode.v_height;
		vesa_vmode[modes].vi_depth = vmode.v_bpp;
		vesa_vmode[modes].vi_planes = vmode.v_planes;
		vesa_vmode[modes].vi_cwidth = vmode.v_cwidth;
		vesa_vmode[modes].vi_cheight = vmode.v_cheight;
		vesa_vmode[modes].vi_window = (u_int)vmode.v_waseg << 4;
		/* XXX window B */
		vesa_vmode[modes].vi_window_size = vmode.v_wsize*1024;
		vesa_vmode[modes].vi_window_gran = vmode.v_wgran*1024;
		if (vmode.v_modeattr & V_MODELFB)
			vesa_vmode[modes].vi_buffer = vmode.v_lfb;
		else
			vesa_vmode[modes].vi_buffer = 0;
		/* XXX */
		vesa_vmode[modes].vi_buffer_size
			= vesa_adp_info->v_memsize*64*1024;
#if 0
		if (vmode.v_offscreen > vmode.v_lfb)
			vesa_vmode[modes].vi_buffer_size
				= vmode.v_offscreen + vmode.v_offscreensize*1024
				      - vmode.v_lfb;
		else
			vesa_vmode[modes].vi_buffer_size
				= vmode.v_offscreen + vmode.v_offscreensize*1024
#endif
		vesa_vmode[modes].vi_mem_model 
			= vesa_translate_mmodel(vmode.v_memmodel);
		vesa_vmode[modes].vi_pixel_fields[0] = 0;
		vesa_vmode[modes].vi_pixel_fields[1] = 0;
		vesa_vmode[modes].vi_pixel_fields[2] = 0;
		vesa_vmode[modes].vi_pixel_fields[3] = 0;
		vesa_vmode[modes].vi_pixel_fsizes[0] = 0;
		vesa_vmode[modes].vi_pixel_fsizes[1] = 0;
		vesa_vmode[modes].vi_pixel_fsizes[2] = 0;
		vesa_vmode[modes].vi_pixel_fsizes[3] = 0;
		if (vesa_vmode[modes].vi_mem_model == V_INFO_MM_PACKED) {
			vesa_vmode[modes].vi_pixel_size = (vmode.v_bpp + 7)/8;
		} else if (vesa_vmode[modes].vi_mem_model == V_INFO_MM_DIRECT) {
			vesa_vmode[modes].vi_pixel_size = (vmode.v_bpp + 7)/8;
			vesa_vmode[modes].vi_pixel_fields[0]
			    = vmode.v_redfieldpos;
			vesa_vmode[modes].vi_pixel_fields[1]
			    = vmode.v_greenfieldpos;
			vesa_vmode[modes].vi_pixel_fields[2]
			    = vmode.v_bluefieldpos;
			vesa_vmode[modes].vi_pixel_fields[3]
			    = vmode.v_resfieldpos;
			vesa_vmode[modes].vi_pixel_fsizes[0]
			    = vmode.v_redmasksize;
			vesa_vmode[modes].vi_pixel_fsizes[1]
			    = vmode.v_greenmasksize;
			vesa_vmode[modes].vi_pixel_fsizes[2]
			    = vmode.v_bluemasksize;
			vesa_vmode[modes].vi_pixel_fsizes[3]
			    = vmode.v_resmasksize;
		} else {
			vesa_vmode[modes].vi_pixel_size = 0;
		}
		
		vesa_vmode[modes].vi_flags 
			= vesa_translate_flags(vmode.v_modeattr) | V_INFO_VESA;
		++modes;
	}
	vesa_vmode[modes].vi_mode = EOT;
	if (bootverbose)
		printf("VESA: %d mode(s) found\n", modes);

	has_vesa_bios = (modes > 0);
	if (!has_vesa_bios)
		return (1);

	return (0);
}

static void
vesa_clear_modes(video_info_t *info, int color)
{
	while (info->vi_mode != EOT) {
		if ((info->vi_flags & V_INFO_COLOR) != color)
			info->vi_mode = NA;
		++info;
	}
}

static vm_offset_t
vesa_map_buffer(u_int paddr, size_t size)
{
	vm_offset_t vaddr;
	u_int off;

	off = paddr - trunc_page(paddr);
	vaddr = (vm_offset_t)pmap_mapdev(paddr - off, size + off);
#if VESA_DEBUG > 1
	printf("vesa_map_buffer: paddr:%x vaddr:%x size:%x off:%x\n",
	       paddr, vaddr, size, off);
#endif
	return (vaddr + off);
}

static void
vesa_unmap_buffer(vm_offset_t vaddr, size_t size)
{
#if VESA_DEBUG > 1
	printf("vesa_unmap_buffer: vaddr:%x size:%x\n", vaddr, size);
#endif
	kmem_free(kernel_map, vaddr, size);
}

/* entry points */

static int
vesa_configure(int flags)
{
	video_adapter_t *adp;
	int adapters;
	int error;
	int i;

	if (vesa_init_done)
		return 0;
	if (flags & VIO_PROBE_ONLY)
		return 0;		/* XXX */

	/*
	 * If the VESA module has already been loaded, abort loading 
	 * the module this time.
	 */
	for (i = 0; (adp = vid_get_adapter(i)) != NULL; ++i) {
		if (adp->va_flags & V_ADP_VESA)
			return ENXIO;
		if (adp->va_type == KD_VGA)
			break;
	}
	/*
	 * The VGA adapter is not found.  This is because either 
	 * 1) the VGA driver has not been initialized, or 2) the VGA card
	 * is not present.  If 1) is the case, we shall defer
	 * initialization for now and try again later.
	 */
	if (adp == NULL) {
		vga_sub_configure = vesa_configure;
		return ENODEV;
	}

	/* count number of registered adapters */
	for (++i; vid_get_adapter(i) != NULL; ++i)
		;
	adapters = i;

	/* call VESA BIOS */
	vesa_adp = adp;
	if (vesa_bios_init()) {
		vesa_adp = NULL;
		return ENXIO;
	}
	vesa_adp->va_flags |= V_ADP_VESA;

	/* remove conflicting modes if we have more than one adapter */
	if (adapters > 1) {
		vesa_clear_modes(vesa_vmode,
				 (vesa_adp->va_flags & V_ADP_COLOR) ? 
				     V_INFO_COLOR : 0);
	}

	if ((error = vesa_load_ioctl()) == 0) {
		prevvidsw = vidsw[vesa_adp->va_index];
		vidsw[vesa_adp->va_index] = &vesavidsw;
		vesa_init_done = TRUE;
	} else {
		vesa_adp = NULL;
		return error;
	}

	return 0;
}

#if 0
static int
vesa_nop(void)
{
	return 0;
}
#endif

static int
vesa_error(void)
{
	return 1;
}

static int
vesa_probe(int unit, video_adapter_t **adpp, void *arg, int flags)
{
	return (*prevvidsw->probe)(unit, adpp, arg, flags);
}

static int
vesa_init(int unit, video_adapter_t *adp, int flags)
{
	return (*prevvidsw->init)(unit, adp, flags);
}

static int
vesa_get_info(video_adapter_t *adp, int mode, video_info_t *info)
{
	int i;

	if ((*prevvidsw->get_info)(adp, mode, info) == 0)
		return 0;

	if (adp != vesa_adp)
		return 1;

	mode = vesa_map_gen_mode_num(vesa_adp->va_type, 
				     vesa_adp->va_flags & V_ADP_COLOR, mode);
	for (i = 0; vesa_vmode[i].vi_mode != EOT; ++i) {
		if (vesa_vmode[i].vi_mode == NA)
			continue;
		if (vesa_vmode[i].vi_mode == mode) {
			*info = vesa_vmode[i];
			return 0;
		}
	}
	return 1;
}

static int
vesa_query_mode(video_adapter_t *adp, video_info_t *info)
{
	int i;

	if ((*prevvidsw->query_mode)(adp, info) == 0)
		return 0;
	if (adp != vesa_adp)
		return ENODEV;

	for (i = 0; vesa_vmode[i].vi_mode != EOT; ++i) {
		if ((info->vi_width != 0)
		    && (info->vi_width != vesa_vmode[i].vi_width))
			continue;
		if ((info->vi_height != 0)
		    && (info->vi_height != vesa_vmode[i].vi_height))
			continue;
		if ((info->vi_cwidth != 0)
		    && (info->vi_cwidth != vesa_vmode[i].vi_cwidth))
			continue;
		if ((info->vi_cheight != 0)
		    && (info->vi_cheight != vesa_vmode[i].vi_cheight))
			continue;
		if ((info->vi_depth != 0)
		    && (info->vi_depth != vesa_vmode[i].vi_depth))
			continue;
		if ((info->vi_planes != 0)
		    && (info->vi_planes != vesa_vmode[i].vi_planes))
			continue;
		/* pixel format, memory model */
		if ((info->vi_flags != 0)
		    && (info->vi_flags != vesa_vmode[i].vi_flags))
			continue;
		*info = vesa_vmode[i];
		return 0;
	}
	return ENODEV;
}

static int
vesa_set_mode(video_adapter_t *adp, int mode)
{
	video_info_t info;
	int len;

	if (adp != vesa_adp)
		return (*prevvidsw->set_mode)(adp, mode);

	mode = vesa_map_gen_mode_num(adp->va_type, 
				     adp->va_flags & V_ADP_COLOR, mode);
#if VESA_DEBUG > 0
	printf("VESA: set_mode(): %d(%x) -> %d(%x)\n",
		adp->va_mode, adp->va_mode, mode, mode);
#endif
	/* 
	 * If the current mode is a VESA mode and the new mode is not,
	 * restore the state of the adapter first by setting one of the
	 * standard VGA mode, so that non-standard, extended SVGA registers 
	 * are set to the state compatible with the standard VGA modes. 
	 * Otherwise (*prevvidsw->set_mode)() may not be able to set up 
	 * the new mode correctly.
	 */
	if (VESA_MODE(adp->va_mode)) {
		if ((*prevvidsw->get_info)(adp, mode, &info) == 0) {
			int10_set_mode(adp->va_initial_bios_mode);
			if (adp->va_info.vi_flags & V_INFO_LINEAR)
				vesa_unmap_buffer(adp->va_buffer,
						  vesa_adp_info->v_memsize*64*1024);
			/* 
			 * Once (*prevvidsw->get_info)() succeeded, 
			 * (*prevvidsw->set_mode)() below won't fail...
			 */
		}
	}

	/* we may not need to handle this mode after all... */
	if ((*prevvidsw->set_mode)(adp, mode) == 0)
		return 0;

	/* is the new mode supported? */
	if (vesa_get_info(adp, mode, &info))
		return 1;
	/* assert(VESA_MODE(mode)); */

#if VESA_DEBUG > 0
	printf("VESA: about to set a VESA mode...\n");
#endif
	/* don't use the linear frame buffer for text modes. XXX */
	if (!(info.vi_flags & V_INFO_GRAPHICS))
		info.vi_flags &= ~V_INFO_LINEAR;

	if (vesa_bios_set_mode(mode | ((info.vi_flags & V_INFO_LINEAR) ? 0x4000 : 0)))
		return 1;

	if (adp->va_info.vi_flags & V_INFO_LINEAR)
		vesa_unmap_buffer(adp->va_buffer,
				  vesa_adp_info->v_memsize*64*1024);

#if VESA_DEBUG > 0
	printf("VESA: mode set!\n");
#endif
	vesa_adp->va_mode = mode;
	vesa_adp->va_flags &= ~V_ADP_COLOR;
	vesa_adp->va_flags |= 
		(info.vi_flags & V_INFO_COLOR) ? V_ADP_COLOR : 0;
	vesa_adp->va_crtc_addr =
		(vesa_adp->va_flags & V_ADP_COLOR) ? COLOR_CRTC : MONO_CRTC;
	if (info.vi_flags & V_INFO_LINEAR) {
#if VESA_DEBUG > 1
		printf("VESA: setting up LFB\n");
#endif
		vesa_adp->va_buffer =
			vesa_map_buffer(info.vi_buffer,
					vesa_adp_info->v_memsize*64*1024);
		vesa_adp->va_buffer_size = info.vi_buffer_size;
		vesa_adp->va_window = vesa_adp->va_buffer;
		vesa_adp->va_window_size = info.vi_buffer_size/info.vi_planes;
		vesa_adp->va_window_gran = info.vi_buffer_size/info.vi_planes;
	} else {
		vesa_adp->va_buffer = 0;
		vesa_adp->va_buffer_size = info.vi_buffer_size;
		vesa_adp->va_window = BIOS_PADDRTOVADDR(info.vi_window);
		vesa_adp->va_window_size = info.vi_window_size;
		vesa_adp->va_window_gran = info.vi_window_gran;
	}
	vesa_adp->va_window_orig = 0;
	len = vesa_bios_get_line_length();
	if (len > 0) {
		vesa_adp->va_line_width = len;
	} else if (info.vi_flags & V_INFO_GRAPHICS) {
		switch (info.vi_depth/info.vi_planes) {
		case 1:
			vesa_adp->va_line_width = info.vi_width/8;
			break;
		case 2:
			vesa_adp->va_line_width = info.vi_width/4;
			break;
		case 4:
			vesa_adp->va_line_width = info.vi_width/2;
			break;
		case 8:
		default: /* shouldn't happen */
			vesa_adp->va_line_width = info.vi_width;
			break;
		}
	} else {
		vesa_adp->va_line_width = info.vi_width;
	}
	vesa_adp->va_disp_start.x = 0;
	vesa_adp->va_disp_start.y = 0;
#if VESA_DEBUG > 0
	printf("vesa_set_mode(): vi_width:%d, len:%d, line_width:%d\n",
	       info.vi_width, len, vesa_adp->va_line_width);
#endif
	bcopy(&info, &vesa_adp->va_info, sizeof(vesa_adp->va_info));

	/* move hardware cursor out of the way */
	(*vidsw[vesa_adp->va_index]->set_hw_cursor)(vesa_adp, -1, -1);

	return 0;
}

static int
vesa_save_font(video_adapter_t *adp, int page, int fontsize, int fontwidth,
	       u_char *data, int ch, int count)
{
	return (*prevvidsw->save_font)(adp, page, fontsize, fontwidth, data,
		ch, count);
}

static int
vesa_load_font(video_adapter_t *adp, int page, int fontsize, int fontwidth,
	       u_char *data, int ch, int count)
{
	return (*prevvidsw->load_font)(adp, page, fontsize, fontwidth, data,
		ch, count);
}

static int
vesa_show_font(video_adapter_t *adp, int page)
{
	return (*prevvidsw->show_font)(adp, page);
}

static int
vesa_save_palette(video_adapter_t *adp, u_char *palette)
{
	int bits;
	int error;

	if ((adp == vesa_adp) && (vesa_adp_info->v_flags & V_DAC8)
	    && VESA_MODE(adp->va_mode)) {
		bits = vesa_bios_get_dac();
		error = vesa_bios_save_palette(0, 256, palette, bits);
		if (error == 0)
			return 0;
		if (bits != 6)
			return error;
	}

	return (*prevvidsw->save_palette)(adp, palette);
}

static int
vesa_load_palette(video_adapter_t *adp, u_char *palette)
{
#ifdef notyet
	int bits;
	int error;

	if ((adp == vesa_adp) && (vesa_adp_info->v_flags & V_DAC8) 
	    && VESA_MODE(adp->va_mode) && ((bits = vesa_bios_set_dac(8)) > 6)) {
		error = vesa_bios_load_palette(0, 256, palette, bits);
		if (error == 0)
			return 0;
		if (vesa_bios_set_dac(6) != 6)
			return 1;
	}
#endif /* notyet */

	return (*prevvidsw->load_palette)(adp, palette);
}

static int
vesa_set_border(video_adapter_t *adp, int color)
{
	return (*prevvidsw->set_border)(adp, color);
}

static int
vesa_save_state(video_adapter_t *adp, void *p, size_t size)
{
	if (adp != vesa_adp)
		return (*prevvidsw->save_state)(adp, p, size);

	if (vesa_state_buf_size == 0)
		vesa_state_buf_size = vesa_bios_state_buf_size();
	if (size == 0)
		return (sizeof(int) + vesa_state_buf_size);
	else if (size < (sizeof(int) + vesa_state_buf_size))
		return 1;

	((adp_state_t *)p)->sig = V_STATE_SIG;
	bzero(((adp_state_t *)p)->regs, vesa_state_buf_size);
	return vesa_bios_save_restore(STATE_SAVE, ((adp_state_t *)p)->regs, 
				      vesa_state_buf_size);
}

static int
vesa_load_state(video_adapter_t *adp, void *p)
{
	if ((adp != vesa_adp) || (((adp_state_t *)p)->sig != V_STATE_SIG))
		return (*prevvidsw->load_state)(adp, p);

	return vesa_bios_save_restore(STATE_LOAD, ((adp_state_t *)p)->regs, 
				      vesa_state_buf_size);
}

#if 0
static int
vesa_get_origin(video_adapter_t *adp, off_t *offset)
{
	struct vm86frame vmf;
	int err;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f05; 
	vmf.vmf_ebx = 0x10;		/* WINDOW_A, XXX */
	err = vm86_intcall(0x10, &vmf); 
	if ((err != 0) || (vmf.vmf_ax != 0x4f))
		return 1;
	*offset = vmf.vmf_dx*adp->va_window_gran;
	return 0;
}
#endif

static int
vesa_set_origin(video_adapter_t *adp, off_t offset)
{
	struct vm86frame vmf;
	int err;

	/*
	 * This function should return as quickly as possible to 
	 * maintain good performance of the system. For this reason,
	 * error checking is kept minimal and let the VESA BIOS to 
	 * detect error.
	 */
	if (adp != vesa_adp) 
		return (*prevvidsw->set_win_org)(adp, offset);

	/* if this is a linear frame buffer, do nothing */
	if (adp->va_info.vi_flags & V_INFO_LINEAR)
		return 0;
	/* XXX */
	if (adp->va_window_gran == 0)
		return 1;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f05; 
	vmf.vmf_ebx = 0;		/* WINDOW_A, XXX */
	vmf.vmf_edx = offset/adp->va_window_gran;
	err = vm86_intcall(0x10, &vmf); 
	if ((err != 0) || (vmf.vmf_ax != 0x4f))
		return 1;
	bzero(&vmf, sizeof(vmf));
	vmf.vmf_eax = 0x4f05; 
	vmf.vmf_ebx = 1;		/* WINDOW_B, XXX */
	vmf.vmf_edx = offset/adp->va_window_gran;
	err = vm86_intcall(0x10, &vmf); 
	adp->va_window_orig = (offset/adp->va_window_gran)*adp->va_window_gran;
	return 0;			/* XXX */
}

static int
vesa_read_hw_cursor(video_adapter_t *adp, int *col, int *row)
{
	return (*prevvidsw->read_hw_cursor)(adp, col, row);
}

static int
vesa_set_hw_cursor(video_adapter_t *adp, int col, int row)
{
	return (*prevvidsw->set_hw_cursor)(adp, col, row);
}

static int
vesa_set_hw_cursor_shape(video_adapter_t *adp, int base, int height,
			 int celsize, int blink)
{
	return (*prevvidsw->set_hw_cursor_shape)(adp, base, height, celsize,
						 blink);
}

static int
vesa_blank_display(video_adapter_t *adp, int mode) 
{
	/* XXX: use VESA DPMS */
	return (*prevvidsw->blank_display)(adp, mode);
}

static int
vesa_mmap(video_adapter_t *adp, vm_offset_t offset, vm_paddr_t *paddr,
	  int prot)
{
#if VESA_DEBUG > 0
	printf("vesa_mmap(): window:0x%x, buffer:0x%x, offset:0x%x\n", 
	       adp->va_info.vi_window, adp->va_info.vi_buffer, offset);
#endif

	if ((adp == vesa_adp) && (adp->va_info.vi_flags & V_INFO_LINEAR)) {
		/* va_window_size == va_buffer_size/vi_planes */
		/* XXX: is this correct? */
		if (offset > adp->va_window_size - PAGE_SIZE)
			return -1;
		*paddr = adp->va_info.vi_buffer + offset;
		return 0;
	} else {
		return (*prevvidsw->mmap)(adp, offset, paddr, prot);
	}
}

static int
vesa_clear(video_adapter_t *adp)
{
	return (*prevvidsw->clear)(adp);
}

static int
vesa_fill_rect(video_adapter_t *adp, int val, int x, int y, int cx, int cy)
{
	return (*prevvidsw->fill_rect)(adp, val, x, y, cx, cy);
}

static int
vesa_bitblt(video_adapter_t *adp,...)
{
	/* FIXME */
	return 1;
}

static int
get_palette(video_adapter_t *adp, int base, int count,
	    u_char *red, u_char *green, u_char *blue, u_char *trans)
{
	u_char *r;
	u_char *g;
	u_char *b;
	int bits;
	int error;

	if ((base < 0) || (base >= 256) || (count < 0) || (count > 256))
		return 1;
	if ((base + count) > 256)
		return 1;
	if (!(vesa_adp_info->v_flags & V_DAC8) || !VESA_MODE(adp->va_mode))
		return 1;

	bits = vesa_bios_get_dac();
	if (bits <= 6)
		return 1;

	r = malloc(count*3, M_DEVBUF, M_WAITOK);
	g = r + count;
	b = g + count;
	error = vesa_bios_save_palette2(base, count, r, g, b, bits);
	if (error == 0) {
		copyout(r, red, count);
		copyout(g, green, count);
		copyout(b, blue, count);
		if (trans != NULL) {
			bzero(r, count);
			copyout(r, trans, count);
		}
	}
	free(r, M_DEVBUF);

	/* if error && bits != 6 at this point, we are in in trouble... XXX */
	return error;
}

static int
set_palette(video_adapter_t *adp, int base, int count,
	    u_char *red, u_char *green, u_char *blue, u_char *trans)
{
	return 1;
#ifdef notyet
	u_char *r;
	u_char *g;
	u_char *b;
	int bits;
	int error;

	if ((base < 0) || (base >= 256) || (base + count > 256))
		return 1;
	if (!(vesa_adp_info->v_flags & V_DAC8) || !VESA_MODE(adp->va_mode)
		|| ((bits = vesa_bios_set_dac(8)) <= 6))
		return 1;

	r = malloc(count*3, M_DEVBUF, M_WAITOK);
	g = r + count;
	b = g + count;
	copyin(red, r, count);
	copyin(green, g, count);
	copyin(blue, b, count);

	error = vesa_bios_load_palette2(base, count, r, g, b, bits);
	free(r, M_DEVBUF);
	if (error == 0)
		return 0;

	/* if the following call fails, we are in trouble... XXX */
	vesa_bios_set_dac(6);
	return 1;
#endif /* notyet */
}

static int
vesa_ioctl(video_adapter_t *adp, u_long cmd, caddr_t arg)
{
	int bytes;

	if (adp != vesa_adp)
		return (*prevvidsw->ioctl)(adp, cmd, arg);

	switch (cmd) {
	case FBIO_SETWINORG:	/* set frame buffer window origin */
		if (!VESA_MODE(adp->va_mode))
			return (*prevvidsw->ioctl)(adp, cmd, arg);
		return (vesa_set_origin(adp, *(off_t *)arg) ? ENODEV : 0);

	case FBIO_SETDISPSTART:	/* set display start address */
		if (!VESA_MODE(adp->va_mode))
			return (*prevvidsw->ioctl)(adp, cmd, arg);
		if (vesa_bios_set_start(((video_display_start_t *)arg)->x,
					((video_display_start_t *)arg)->y))
			return ENODEV;
		adp->va_disp_start.x = ((video_display_start_t *)arg)->x;
		adp->va_disp_start.y = ((video_display_start_t *)arg)->y;
		return 0;

	case FBIO_SETLINEWIDTH:	/* set line length in pixel */
		if (!VESA_MODE(adp->va_mode))
			return (*prevvidsw->ioctl)(adp, cmd, arg);
		if (vesa_bios_set_line_length(*(u_int *)arg, &bytes, NULL))
			return ENODEV;
		adp->va_line_width = bytes;
#if VESA_DEBUG > 1
		printf("new line width:%d\n", adp->va_line_width);
#endif
		return 0;

	case FBIO_GETPALETTE:	/* get color palette */
		if (get_palette(adp, ((video_color_palette_t *)arg)->index,
				((video_color_palette_t *)arg)->count,
				((video_color_palette_t *)arg)->red,
				((video_color_palette_t *)arg)->green,
				((video_color_palette_t *)arg)->blue,
				((video_color_palette_t *)arg)->transparent))
			return (*prevvidsw->ioctl)(adp, cmd, arg);
		return 0;


	case FBIO_SETPALETTE:	/* set color palette */
		if (set_palette(adp, ((video_color_palette_t *)arg)->index,
				((video_color_palette_t *)arg)->count,
				((video_color_palette_t *)arg)->red,
				((video_color_palette_t *)arg)->green,
				((video_color_palette_t *)arg)->blue,
				((video_color_palette_t *)arg)->transparent))
			return (*prevvidsw->ioctl)(adp, cmd, arg);
		return 0;

	case FBIOGETCMAP:	/* get color palette */
		if (get_palette(adp, ((struct fbcmap *)arg)->index,
				((struct fbcmap *)arg)->count,
				((struct fbcmap *)arg)->red,
				((struct fbcmap *)arg)->green,
				((struct fbcmap *)arg)->blue, NULL))
			return (*prevvidsw->ioctl)(adp, cmd, arg);
		return 0;

	case FBIOPUTCMAP:	/* set color palette */
		if (set_palette(adp, ((struct fbcmap *)arg)->index,
				((struct fbcmap *)arg)->count,
				((struct fbcmap *)arg)->red,
				((struct fbcmap *)arg)->green,
				((struct fbcmap *)arg)->blue, NULL))
			return (*prevvidsw->ioctl)(adp, cmd, arg);
		return 0;

	default:
		return (*prevvidsw->ioctl)(adp, cmd, arg);
	}
}

static int
vesa_diag(video_adapter_t *adp, int level)
{
	int error;

	/* call the previous handler first */
	error = (*prevvidsw->diag)(adp, level);
	if (error)
		return error;

	if (adp != vesa_adp)
		return 1;

	if (level <= 0)
		return 0;

	return 0;
}

static int
vesa_bios_info(int level)
{
#if VESA_DEBUG > 1
	struct vesa_mode vmode;
	int i;
#endif

	if (bootverbose) {
		/* general adapter information */
		printf(
	"VESA: v%d.%d, %dk memory, flags:0x%x, mode table:%p (%x)\n", 
		    ((vesa_adp_info->v_version & 0xf000) >> 12) * 10 +
		    ((vesa_adp_info->v_version & 0x0f00) >> 8),
		    ((vesa_adp_info->v_version & 0x00f0) >> 4) * 10 +
		    (vesa_adp_info->v_version & 0x000f),
		    vesa_adp_info->v_memsize * 64, vesa_adp_info->v_flags,
		    vesa_vmodetab, vesa_adp_info->v_modetable);

		/* OEM string */
		if (vesa_oemstr != NULL)
			printf("VESA: %s\n", vesa_oemstr);
	}

	if (level <= 0)
		return 0;

	if (vesa_adp_info->v_version >= 0x0200 && bootverbose) {
		/* vender name, product name, product revision */
		printf("VESA: %s %s %s\n",
			(vesa_venderstr != NULL) ? vesa_venderstr : "unknown",
			(vesa_prodstr != NULL) ? vesa_prodstr : "unknown",
			(vesa_revstr != NULL) ? vesa_revstr : "?");
	}

#if VESA_DEBUG > 1
	/* mode information */
	for (i = 0;
		(i < (M_VESA_MODE_MAX - M_VESA_BASE + 1))
		&& (vesa_vmodetab[i] != 0xffff); ++i) {
		if (vesa_bios_get_mode(vesa_vmodetab[i], &vmode))
			continue;

		/* print something for diagnostic purpose */
		printf("VESA: mode:0x%03x, flags:0x%04x", 
		       vesa_vmodetab[i], vmode.v_modeattr);
		if (vmode.v_modeattr & V_MODEOPTINFO) {
			if (vmode.v_modeattr & V_MODEGRAPHICS) {
				printf(", G %dx%dx%d %d, ", 
				       vmode.v_width, vmode.v_height,
				       vmode.v_bpp, vmode.v_planes);
			} else {
				printf(", T %dx%d, ", 
				       vmode.v_width, vmode.v_height);
			}
			printf("font:%dx%d, ", 
			       vmode.v_cwidth, vmode.v_cheight);
			printf("pages:%d, mem:%d",
			       vmode.v_ipages + 1, vmode.v_memmodel);
		}
		if (vmode.v_modeattr & V_MODELFB) {
			printf("\nVESA: LFB:0x%x, off:0x%x, off_size:0x%x", 
			       vmode.v_lfb, vmode.v_offscreen,
			       vmode.v_offscreensize*1024);
		}
		printf("\n");
		printf("VESA: window A:0x%x (%x), window B:0x%x (%x), ",
		       vmode.v_waseg, vmode.v_waattr,
		       vmode.v_wbseg, vmode.v_wbattr);
		printf("size:%dk, gran:%dk\n",
		       vmode.v_wsize, vmode.v_wgran);
	}
#endif /* VESA_DEBUG > 1 */

	return 0;
}

/* module loading */

static int
vesa_load(void)
{
	int error;
	int s;

	if (vesa_init_done)
		return 0;

	/* locate a VGA adapter */
	s = spltty();
	vesa_adp = NULL;
	error = vesa_configure(0);
	splx(s);

	if (error == 0)
		vesa_bios_info(bootverbose);

	return error;
}

static int
vesa_unload(void)
{
	u_char palette[256*3];
	int error;
	int bits;
	int s;

	/* if the adapter is currently in a VESA mode, don't unload */
	if ((vesa_adp != NULL) && VESA_MODE(vesa_adp->va_mode))
		return EBUSY;
	/* 
	 * FIXME: if there is at least one vty which is in a VESA mode,
	 * we shouldn't be unloading! XXX
	 */

	s = spltty();
	if ((error = vesa_unload_ioctl()) == 0) {
		if (vesa_adp != NULL) {
			if (vesa_adp_info->v_flags & V_DAC8)  {
				bits = vesa_bios_get_dac();
				if (bits > 6) {
					vesa_bios_save_palette(0, 256,
							       palette, bits);
					vesa_bios_set_dac(6);
					vesa_bios_load_palette(0, 256,
							       palette, 6);
				}
			}
			vesa_adp->va_flags &= ~V_ADP_VESA;
			vidsw[vesa_adp->va_index] = prevvidsw;
		}
	}
	splx(s);

	if (vesa_vm86_buf != NULL)
		free(vesa_vm86_buf, M_DEVBUF);

	return error;
}

static int
vesa_mod_event(module_t mod, int type, void *data)
{
	switch (type) {
	case MOD_LOAD:
		return vesa_load();
	case MOD_UNLOAD:
		return vesa_unload();
	default:
		return EOPNOTSUPP;
	}
	return 0;
}

static moduledata_t vesa_mod = {
	"vesa",
	vesa_mod_event,
	NULL,
};

DECLARE_MODULE(vesa, vesa_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);

#endif	/* VGA_NO_MODE_CHANGE */
