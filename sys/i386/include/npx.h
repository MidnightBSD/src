/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	from: @(#)npx.h	5.3 (Berkeley) 1/18/91
 * $FreeBSD$
 */

/*
 * 287/387 NPX Coprocessor Data Structures and Constants
 * W. Jolitz 1/90
 */

#ifndef _MACHINE_NPX_H_
#define	_MACHINE_NPX_H_

/* Environment information of floating point unit */
struct env87 {
	long	en_cw;		/* control word (16bits) */
	long	en_sw;		/* status word (16bits) */
	long	en_tw;		/* tag word (16bits) */
	long	en_fip;		/* floating point instruction pointer */
	u_short	en_fcs;		/* floating code segment selector */
	u_short	en_opcode;	/* opcode last executed (11 bits ) */
	long	en_foo;		/* floating operand offset */
	long	en_fos;		/* floating operand segment selector */
};

/* Contents of each floating point accumulator */
struct fpacc87 {
#ifdef dontdef /* too unportable */
	u_long	fp_mantlo;	/* mantissa low (31:0) */
	u_long	fp_manthi;	/* mantissa high (63:32) */
	int	fp_exp:15;	/* exponent */
	int	fp_sgn:1;	/* mantissa sign */
#else
	u_char	fp_bytes[10];
#endif
};

/* Floating point context */
struct save87 {
	struct	env87 sv_env;	/* floating point control/status */
	struct	fpacc87	sv_ac[8];	/* accumulator contents, 0-7 */
	u_char	sv_pad0[4];	/* padding for (now unused) saved status word */
	/*
	 * Bogus padding for emulators.  Emulators should use their own
	 * struct and arrange to store into this struct (ending here)
	 * before it is inspected for ptracing or for core dumps.  Some
	 * emulators overwrite the whole struct.  We have no good way of
	 * knowing how much padding to leave.  Leave just enough for the
	 * GPL emulator's i387_union (176 bytes total).
	 */
	u_char	sv_pad[64];	/* padding; used by emulators */
};

struct  envxmm {
	u_int16_t	en_cw;		/* control word (16bits) */
	u_int16_t	en_sw;		/* status word (16bits) */
	u_int16_t	en_tw;		/* tag word (16bits) */
	u_int16_t	en_opcode;	/* opcode last executed (11 bits ) */
	u_int32_t	en_fip;		/* floating point instruction pointer */
	u_int16_t	en_fcs;		/* floating code segment selector */
	u_int16_t	en_pad0;	/* padding */
	u_int32_t	en_foo;		/* floating operand offset */
	u_int16_t	en_fos;		/* floating operand segment selector */
	u_int16_t	en_pad1;	/* padding */
	u_int32_t	en_mxcsr;	/* SSE control/status register */
	u_int32_t	en_mxcsr_mask;	/* valid bits in mxcsr */
};

/* Contents of each SSE extended accumulator */
struct  xmmacc {
	u_char	xmm_bytes[16];
};

/* Contents of the upper 16 bytes of each AVX extended accumulator */
struct  ymmacc {
	uint8_t  ymm_bytes[16];
};

struct  savexmm {
	struct	envxmm	sv_env;
	struct {
		struct fpacc87	fp_acc;
		u_char		fp_pad[6];      /* padding */
	} sv_fp[8];
	struct xmmacc	sv_xmm[8];
	u_char sv_pad[224];
} __aligned(16);

union	savefpu {
	struct	save87	sv_87;
	struct	savexmm	sv_xmm;
};

struct xstate_hdr {
	uint64_t xstate_bv;
	uint8_t xstate_rsrv0[16];
	uint8_t	xstate_rsrv[40];
};

struct savexmm_xstate {
	struct xstate_hdr sx_hd;
	struct ymmacc	sx_ymm[16];
};

struct savexmm_ymm {
	struct	envxmm	sv_env;
	struct {
		struct fpacc87	fp_acc;
		int8_t		fp_pad[6];      /* padding */
	} sv_fp[8];
	struct xmmacc	sv_xmm[16];
	uint8_t sv_pad[96];
	struct savexmm_xstate sv_xstate;
} __aligned(64);

/*
 * The hardware default control word for i387's and later coprocessors is
 * 0x37F, giving:
 *
 *	round to nearest
 *	64-bit precision
 *	all exceptions masked.
 *
 * We modify the affine mode bit and precision bits in this to give:
 *
 *	affine mode for 287's (if they work at all) (1 in bitfield 1<<12)
 *	53-bit precision (2 in bitfield 3<<8)
 *
 * 64-bit precision often gives bad results with high level languages
 * because it makes the results of calculations depend on whether
 * intermediate values are stored in memory or in FPU registers.
 */
#define	__INITIAL_NPXCW__	0x127F
#define	__INITIAL_MXCSR__	0x1F80

#ifdef _KERNEL

#define	PCB_USER_FPU(pcb) (((pcb)->pcb_flags & PCB_KERNNPX) == 0)

int	npxdna(void);
void	npxdrop(void);
void	npxexit(struct thread *td);
int	npxformat(void);
int	npxgetregs(struct thread *td);
void	npxinit(void);
void	npxsave(union savefpu *addr);
void	npxsetregs(struct thread *td, union savefpu *addr);
int	npxtrap(void);
void	npxuserinited(struct thread *);
struct fpu_kern_ctx *fpu_kern_alloc_ctx(u_int flags);
void	fpu_kern_free_ctx(struct fpu_kern_ctx *ctx);
int	fpu_kern_enter(struct thread *td, struct fpu_kern_ctx *ctx,
	    u_int flags);
int	fpu_kern_leave(struct thread *td, struct fpu_kern_ctx *ctx);
int	fpu_kern_thread(u_int flags);
int	is_fpu_kern_thread(u_int flags);

/*
 * Flags for fpu_kern_enter() and fpu_kern_thread().
 */
#define	FPU_KERN_NORMAL	0x0000
#define	FPU_KERN_NOWAIT	0x0001

#endif

#endif /* !_MACHINE_NPX_H_ */
