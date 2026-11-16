/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Rafal Rabczuk
 * Copyright (c) 2026 Lucas Holt
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Register layouts and unit conversions for Intel RAPL (Running Average
 * Power Limit).  Nothing in here touches an MSR or any kernel facility, so
 * the same code is compiled into the driver and into its userland tests.
 */

#ifndef _DEV_INTEL_RAPL_CALC_H_
#define	_DEV_INTEL_RAPL_CALC_H_

/*
 * MSR_RAPL_POWER_UNIT.  Each field is a power-of-two divisor: a power unit
 * of 3 means one step is 1/2^3 W.
 */
#define	RAPL_UNIT_POWER(u)	((u_int)((u) & 0x0f))
#define	RAPL_UNIT_ENERGY(u)	((u_int)(((u) >> 8) & 0x1f))
#define	RAPL_UNIT_TIME(u)	((u_int)(((u) >> 16) & 0x0f))

/*
 * MSR_PKG_POWER_LIMIT.  PL1 is in the low 32 bits and PL2 in the high 32
 * bits, with the same layout.  Bit 63 locks the whole register until reset.
 */
#define	RAPL_PL2_SHIFT		32
#define	RAPL_PL_POWER_MASK	0x7fffULL	/* bits 14:0 */
#define	RAPL_PL_ENABLE		(1ULL << 15)
#define	RAPL_PL_CLAMP		(1ULL << 16)
#define	RAPL_PL_WINDOW_SHIFT	17		/* bits 23:17 */
#define	RAPL_PL_WINDOW_MASK	0x7fULL
#define	RAPL_PL_LOCKED		(1ULL << 63)

/*
 * MSR_PKG_POWER_INFO.  The power fields are in power units; the maximum
 * time window uses the same Y/Z encoding as the limit register, with only
 * one bit of Z.
 */
#define	RAPL_INFO_TDP(i)	((uint32_t)((i) & 0x7fff))
#define	RAPL_INFO_MIN(i)	((uint32_t)(((i) >> 16) & 0x7fff))
#define	RAPL_INFO_MAX(i)	((uint32_t)(((i) >> 32) & 0x7fff))
#define	RAPL_INFO_WINDOW(i)	((uint32_t)(((i) >> 48) & 0x3f))

/*
 * A time window field encodes 2^Y * (1 + Z/4) time units, with Y in bits
 * 4:0 and Z in bits 6:5 of the field.
 */
#define	RAPL_WINDOW_Y(f)	((f) & 0x1f)
#define	RAPL_WINDOW_Z(f)	(((f) >> 5) & 0x03)
#define	RAPL_WINDOW_FIELD(y, z)	(((y) & 0x1f) | (((z) & 0x03) << 5))

uint64_t rapl_units_to_mw(uint32_t units, u_int power_shift);
int	 rapl_mw_to_units(uint32_t mw, u_int power_shift, uint32_t *units);
uint64_t rapl_window_to_us(uint32_t field, u_int time_shift);
int	 rapl_us_to_window(uint64_t us, u_int time_shift, uint32_t *field);
uint64_t rapl_ticks_to_uj(uint64_t ticks, u_int energy_shift);
uint32_t rapl_power_mw(uint64_t uj, uint64_t us);

#endif /* !_DEV_INTEL_RAPL_CALC_H_ */
