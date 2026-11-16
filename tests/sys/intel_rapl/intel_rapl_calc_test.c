/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
 * Unit conversion tests for intel_rapl(4).  These exercise the arithmetic
 * shared with the driver and touch no hardware.
 */

#include <sys/types.h>

#include <errno.h>
#include <stdint.h>

#include <atf-c.h>

#include "intel_rapl_calc.h"

/* The power, energy and time units almost every Intel part reports. */
#define	PU	3	/* 1/8 W */
#define	ESU	14	/* 1/16384 J */
#define	TU	10	/* 1/1024 s */

ATF_TC_WITHOUT_HEAD(power_roundtrip);
ATF_TC_BODY(power_roundtrip, tc)
{
	uint32_t got, mw, u;
	u_int shift;

	/*
	 * Writing back a value that was read must not change the register.
	 * Units finer than about 4 mW (shift 9 and up) cannot be expressed
	 * in whole milliwatts, so only the shifts below that are exact.
	 */
	for (shift = 0; shift <= 8; shift++) {
		for (u = 1; u <= RAPL_PL_POWER_MASK; u++) {
			mw = (uint32_t)rapl_units_to_mw(u, shift);
			ATF_REQUIRE_EQ_MSG(rapl_mw_to_units(mw, shift, &got),
			    0, "shift %u units %u mw %u", shift, u, mw);
			ATF_REQUIRE_EQ_MSG(got, u, "shift %u units %u mw %u",
			    shift, u, mw);
		}
	}
}

ATF_TC_WITHOUT_HEAD(power_range);
ATF_TC_BODY(power_range, tc)
{
	uint32_t u;

	ATF_CHECK_EQ(rapl_units_to_mw(120, PU), 15000);
	ATF_CHECK_EQ(rapl_units_to_mw(121, PU), 15125);

	ATF_CHECK_EQ(rapl_mw_to_units(15000, PU, &u), 0);
	ATF_CHECK_EQ(u, 120);
	ATF_CHECK_EQ(rapl_mw_to_units(15060, PU, &u), 0);
	ATF_CHECK_EQ(u, 120);
	ATF_CHECK_EQ(rapl_mw_to_units(15070, PU, &u), 0);
	ATF_CHECK_EQ(u, 121);

	/* Zero, or anything that rounds to zero, would program 0 W. */
	ATF_CHECK_EQ(rapl_mw_to_units(0, PU, &u), EINVAL);
	ATF_CHECK_EQ(rapl_mw_to_units(62, PU, &u), EINVAL);
	ATF_CHECK_EQ(rapl_mw_to_units(63, PU, &u), 0);
	ATF_CHECK_EQ(u, 1);

	/* The largest value the 15-bit field holds, and one step past it. */
	ATF_CHECK_EQ(rapl_mw_to_units(4095875, PU, &u), 0);
	ATF_CHECK_EQ(u, RAPL_PL_POWER_MASK);
	ATF_CHECK_EQ(rapl_mw_to_units(4096000, PU, &u), EINVAL);
	ATF_CHECK_EQ(rapl_mw_to_units(UINT32_MAX, PU, &u), EINVAL);
	ATF_CHECK_EQ(rapl_mw_to_units(UINT32_MAX, 15, &u), EINVAL);
}

ATF_TC_WITHOUT_HEAD(window_known);
ATF_TC_BODY(window_known, tc)
{
	uint32_t f;

	/* A typical PL2 window: Y=1, Z=1 is 2.5 units, about 2.44 ms. */
	ATF_CHECK_EQ(rapl_window_to_us(RAPL_WINDOW_FIELD(1, 1), TU), 2441);

	/* A typical PL1 window: 28 s is exactly 2^14 * 1.75 units. */
	ATF_CHECK_EQ(rapl_us_to_window(28000000, TU, &f), 0);
	ATF_CHECK_EQ(f, RAPL_WINDOW_FIELD(14, 3));
	ATF_CHECK_EQ(rapl_window_to_us(f, TU), 28000000);

	/* 1 s is 1024 units, Y=10 and Z=0. */
	ATF_CHECK_EQ(rapl_us_to_window(1000000, TU, &f), 0);
	ATF_CHECK_EQ(f, RAPL_WINDOW_FIELD(10, 0));

	/* The largest field must not overflow. */
	ATF_CHECK_EQ(rapl_window_to_us(RAPL_WINDOW_FIELD(31, 3), 0),
	    ((uint64_t)1 << 29) * 7 * 1000000);

	ATF_CHECK_EQ(rapl_us_to_window(0, TU, &f), EINVAL);
}

ATF_TC_WITHOUT_HEAD(window_roundtrip);
ATF_TC_BODY(window_roundtrip, tc)
{
	uint64_t us;
	uint32_t f, got, y, z;
	u_int shift;

	for (shift = 0; shift <= 15; shift++) {
		for (y = 0; y <= 0x1f; y++) {
			for (z = 0; z <= 3; z++) {
				f = RAPL_WINDOW_FIELD(y, z);
				us = rapl_window_to_us(f, shift);
				if (us == 0)
					continue;
				ATF_REQUIRE_EQ(rapl_us_to_window(us, shift,
				    &got), 0);
				ATF_REQUIRE_EQ_MSG(got, f,
				    "shift %u y %u z %u us %ju", shift, y, z,
				    (uintmax_t)us);
			}
		}
	}
}

ATF_TC_WITHOUT_HEAD(energy);
ATF_TC_BODY(energy, tc)
{

	ATF_CHECK_EQ(rapl_ticks_to_uj(0, ESU), 0);
	ATF_CHECK_EQ(rapl_ticks_to_uj(1, ESU), 61);
	ATF_CHECK_EQ(rapl_ticks_to_uj(16384, ESU), 1000000);
	ATF_CHECK_EQ(rapl_ticks_to_uj(16384 + 8192, ESU), 1500000);
	/* A long accumulated total must not overflow. */
	ATF_CHECK_EQ(rapl_ticks_to_uj((uint64_t)1 << 40, ESU),
	    ((uint64_t)1 << 26) * 1000000);
	/* A full 32-bit counter span at the finest unit. */
	ATF_CHECK_EQ(rapl_ticks_to_uj(UINT32_MAX, 31), 1999999);
}

ATF_TC_WITHOUT_HEAD(power);
ATF_TC_BODY(power, tc)
{

	ATF_CHECK_EQ(rapl_power_mw(15000000, 1000000), 15000);
	ATF_CHECK_EQ(rapl_power_mw(15000000, 2000000), 7500);
	ATF_CHECK_EQ(rapl_power_mw(1, 0), 0);
	/* The largest energy delta one sample can see does not overflow. */
	ATF_CHECK_EQ(rapl_power_mw(rapl_ticks_to_uj(UINT32_MAX, 0), 1000000),
	    UINT32_MAX);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, power_roundtrip);
	ATF_TP_ADD_TC(tp, power_range);
	ATF_TP_ADD_TC(tp, window_known);
	ATF_TP_ADD_TC(tp, window_roundtrip);
	ATF_TP_ADD_TC(tp, energy);
	ATF_TP_ADD_TC(tp, power);
	return (atf_no_error());
}
