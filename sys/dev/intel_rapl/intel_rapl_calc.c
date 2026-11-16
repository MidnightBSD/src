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
 * Unit conversions for intel_rapl(4).  All arithmetic is done in 64 bits
 * and every conversion into a register field is range checked, so that an
 * out of range request is refused rather than silently truncated.
 */

#include <sys/types.h>
#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/errno.h>
#else
#include <errno.h>
#include <stdint.h>
#endif

#include "intel_rapl_calc.h"

/* Power units to milliwatts, rounded down. */
uint64_t
rapl_units_to_mw(uint32_t units, u_int power_shift)
{

	return (((uint64_t)units * 1000) >> power_shift);
}

/*
 * Milliwatts to power units, rounded to nearest.  Fails rather than wraps
 * when the result does not fit the 15-bit limit field, and refuses a result
 * of zero, which would program a limit of 0 W.
 */
int
rapl_mw_to_units(uint32_t mw, u_int power_shift, uint32_t *units)
{
	uint64_t u;

	u = (((uint64_t)mw << power_shift) + 500) / 1000;
	if (u == 0 || u > RAPL_PL_POWER_MASK)
		return (EINVAL);
	*units = (uint32_t)u;
	return (0);
}

/* Time window field to microseconds, rounded down. */
uint64_t
rapl_window_to_us(uint32_t field, u_int time_shift)
{
	uint64_t y, z;

	y = RAPL_WINDOW_Y(field);
	z = RAPL_WINDOW_Z(field);

	/* 2^31 * 7 * 10^6 is below 2^64, so this cannot overflow. */
	return ((((uint64_t)1 << y) * (4 + z) * 1000000) >>
	    (time_shift + 2));
}

/*
 * Microseconds to the nearest representable time window field.  The
 * representable values are sparse and unevenly spaced, so search them all
 * rather than invert the formula.  Ties go to the shorter window.
 */
int
rapl_us_to_window(uint64_t us, u_int time_shift, uint32_t *field)
{
	uint64_t best_err, cand, err;
	uint32_t best, y, z;

	if (us == 0)
		return (EINVAL);

	best = 0;
	best_err = UINT64_MAX;
	for (y = 0; y <= 0x1f; y++) {
		for (z = 0; z <= 0x03; z++) {
			cand = rapl_window_to_us(RAPL_WINDOW_FIELD(y, z),
			    time_shift);
			err = cand > us ? cand - us : us - cand;
			if (err < best_err) {
				best_err = err;
				best = RAPL_WINDOW_FIELD(y, z);
			}
		}
	}
	*field = best;
	return (0);
}

/*
 * Energy counter ticks to microjoules.  One tick is 1/2^energy_shift J.
 * The whole and fractional parts are scaled separately, so the result
 * only overflows past 1.8e13 J, thousands of years at any package power.
 */
uint64_t
rapl_ticks_to_uj(uint64_t ticks, u_int energy_shift)
{
	uint64_t frac;

	frac = ticks & (((uint64_t)1 << energy_shift) - 1);
	return ((ticks >> energy_shift) * 1000000 +
	    ((frac * 1000000) >> energy_shift));
}

/* Average power in milliwatts for an energy delta over an interval. */
uint32_t
rapl_power_mw(uint64_t uj, uint64_t us)
{
	uint64_t mw;

	if (us == 0)
		return (0);
	/* uj covers at most one 32-bit counter span, so uj * 1000 fits. */
	mw = uj * 1000 / us;
	return (mw > UINT32_MAX ? UINT32_MAX : (uint32_t)mw);
}
