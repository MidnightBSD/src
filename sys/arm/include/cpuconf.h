/* $MidnightBSD$ */
/*	$NetBSD: cpuconf.h,v 1.8 2003/09/06 08:55:42 rearnsha Exp $	*/

/*-
 * Copyright (c) 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: stable/10/sys/arm/include/cpuconf.h 276312 2014-12-27 17:36:49Z ian $
 *
 */

#ifndef _MACHINE_CPUCONF_H_
#define	_MACHINE_CPUCONF_H_

/*
 * IF YOU CHANGE THIS FILE, MAKE SURE TO UPDATE THE DEFINITION OF
 * "PMAP_NEEDS_PTE_SYNC" IN <arm/arm32/pmap.h> FOR THE CPU TYPE
 * YOU ARE ADDING SUPPORT FOR.
 */

/*
 * Step 1: Count the number of CPU types configured into the kernel.
 */
#define	CPU_NTYPES	(defined(CPU_ARM9) +				\
			 defined(CPU_ARM9E) +				\
			 defined(CPU_ARM10) +				\
			 defined(CPU_ARM1136) +				\
			 defined(CPU_ARM1176) +				\
			 defined(CPU_XSCALE_80200) +			\
			 defined(CPU_XSCALE_80321) +			\
			 defined(CPU_XSCALE_PXA2X0) +			\
			 defined(CPU_FA526) +				\
			 defined(CPU_FA626TE) +				\
			 defined(CPU_XSCALE_IXP425)) +			\
			 defined(CPU_CORTEXA) +				\
			 defined(CPU_KRAIT) +				\
			 defined(CPU_MV_PJ4B)

/*
 * Step 2: Determine which ARM architecture versions are configured.
 */
#if defined(CPU_ARM9) || defined(CPU_FA526)
#define	ARM_ARCH_4	1
#else
#define	ARM_ARCH_4	0
#endif

#if (defined(CPU_ARM9E) || defined(CPU_ARM10) ||			\
     defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321) ||		\
     defined(CPU_XSCALE_80219) || defined(CPU_XSCALE_81342) ||		\
     defined(CPU_XSCALE_PXA2X0) || defined(CPU_XSCALE_IXP425) ||	\
     defined(CPU_FA626TE))
#define	ARM_ARCH_5	1
#else
#define	ARM_ARCH_5	0
#endif

#if !defined(ARM_ARCH_6)
#if defined(CPU_ARM1136) || defined(CPU_ARM1176)
#define ARM_ARCH_6	1
#else
#define ARM_ARCH_6	0
#endif
#endif

#if defined(CPU_CORTEXA) || defined(CPU_KRAIT) || defined(CPU_MV_PJ4B)
#define ARM_ARCH_7A	1
#else
#define ARM_ARCH_7A	0
#endif

#define	ARM_NARCH	(ARM_ARCH_4 + ARM_ARCH_5 + ARM_ARCH_6 | ARM_ARCH_7A)

/*
 * Compatibility for userland builds that have no CPUTYPE defined.  Use the ARCH
 * constants predefined by the compiler to define our old-school arch constants.
 * This is a stopgap measure to tide us over until the conversion of all code
 * to the newer ACLE constants defined by ARM (see acle-compat.h).
 */
#if ARM_NARCH == 0
#if defined(__ARM_ARCH_4T__)
#undef  ARM_ARCH_4
#undef  ARM_NARCH
#define ARM_ARCH_4 1
#define ARM_NARCH  1
#define CPU_ARM9 1
#elif defined(__ARM_ARCH_6ZK__)
#undef  ARM_ARCH_6
#undef  ARM_NARCH
#define ARM_ARCH_6 1
#define ARM_NARCH  1
#define CPU_ARM1176 1
#endif
#endif

#if ARM_NARCH == 0 && !defined(KLD_MODULE) && defined(_KERNEL)
#error ARM_NARCH is 0
#endif

#if ARM_ARCH_5 || ARM_ARCH_6 || ARM_ARCH_7A
/*
 * We could support Thumb code on v4T, but the lack of clean interworking
 * makes that hard.
 */
#define THUMB_CODE
#endif

/*
 * Step 3: Define which MMU classes are configured:
 *
 *	ARM_MMU_MEMC		Prehistoric, external memory controller
 *				and MMU for ARMv2 CPUs.
 *
 *      ARM_MMU_GENERIC		Generic ARM MMU, compatible with ARMv4 and v5.
 *
 *	ARM_MMU_V6		ARMv6 MMU.
 *
 *	ARM_MMU_V7		ARMv7 MMU.
 *
 *	ARM_MMU_XSCALE		XScale MMU.  Compatible with generic ARM
 *				MMU, but also has several extensions which
 *				require different PTE layout to use.
 */
#if (defined(CPU_ARM9) || defined(CPU_ARM9E) ||	\
     defined(CPU_ARM10) || defined(CPU_FA526) ||	\
     defined(CPU_FA626TE))
#define	ARM_MMU_GENERIC		1
#else
#define	ARM_MMU_GENERIC		0
#endif

#if defined(CPU_ARM1136) || defined(CPU_ARM1176)
#define ARM_MMU_V6		1
#else
#define ARM_MMU_V6		0
#endif

#if defined(CPU_CORTEXA) || defined(CPU_KRAIT) || defined(CPU_MV_PJ4B)
#define ARM_MMU_V7		1
#else
#define ARM_MMU_V7		0
#endif

#if (defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321) ||		\
     defined(CPU_XSCALE_PXA2X0) || defined(CPU_XSCALE_IXP425) ||	\
     defined(CPU_XSCALE_80219) || defined(CPU_XSCALE_81342))
#define	ARM_MMU_XSCALE		1
#else
#define	ARM_MMU_XSCALE		0
#endif

#define	ARM_NMMUS		(ARM_MMU_GENERIC + ARM_MMU_V6 + \
				 ARM_MMU_V7 + ARM_MMU_XSCALE)
#if ARM_NMMUS == 0 && !defined(KLD_MODULE) && defined(_KERNEL)
#error ARM_NMMUS is 0
#endif

/*
 * Step 4: Define features that may be present on a subset of CPUs
 *
 *	ARM_XSCALE_PMU		Performance Monitoring Unit on 80200 and 80321
 */

#if (defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321) || \
     defined(CPU_XSCALE_80219) || defined(CPU_XSCALE_81342))
#define ARM_XSCALE_PMU	1
#else
#define ARM_XSCALE_PMU	0
#endif

#if defined(CPU_XSCALE_81342)
#define CPU_XSCALE_CORE3
#endif
#endif /* _MACHINE_CPUCONF_H_ */
