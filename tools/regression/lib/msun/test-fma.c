/*-
 * Copyright (c) 2008 David Schultz <das@FreeBSD.org>
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
 * Tests for fma{,f,l}().
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <fenv.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

#define	ALL_STD_EXCEPT	(FE_DIVBYZERO | FE_INEXACT | FE_INVALID | \
			 FE_OVERFLOW | FE_UNDERFLOW)

#pragma STDC FENV_ACCESS ON

/*
 * Test that a function returns the correct value and sets the
 * exception flags correctly. The exceptmask specifies which
 * exceptions we should check. We need to be lenient for several
 * reasons, but mainly because on some architectures it's impossible
 * to raise FE_OVERFLOW without raising FE_INEXACT.
 *
 * These are macros instead of functions so that assert provides more
 * meaningful error messages.
 */
#define	test(func, x, y, z, result, exceptmask, excepts) do {		\
	assert(feclearexcept(FE_ALL_EXCEPT) == 0);			\
	assert(fpequal((func)((x), (y), (z)), (result)));		\
	assert(((func), fetestexcept(exceptmask) == (excepts)));	\
} while (0)

#define	testall(x, y, z, result, exceptmask, excepts)	do {		\
	test(fma, (x), (y), (z), (double)(result), (exceptmask), (excepts)); \
	test(fmaf, (x), (y), (z), (float)(result), (exceptmask), (excepts)); \
	test(fmal, (x), (y), (z), (result), (exceptmask), (excepts));	\
} while (0)

/* Test in all rounding modes. */
#define	testrnd(func, x, y, z, rn, ru, rd, rz, exceptmask, excepts)	do { \
	fesetround(FE_TONEAREST);					\
	test((func), (x), (y), (z), (rn), (exceptmask), (excepts));	\
	fesetround(FE_UPWARD);						\
	test((func), (x), (y), (z), (ru), (exceptmask), (excepts));	\
	fesetround(FE_DOWNWARD);					\
	test((func), (x), (y), (z), (rd), (exceptmask), (excepts));	\
	fesetround(FE_TOWARDZERO);					\
	test((func), (x), (y), (z), (rz), (exceptmask), (excepts));	\
} while (0)

/*
 * Determine whether x and y are equal, with two special rules:
 *	+0.0 != -0.0
 *	 NaN == NaN
 */
int
fpequal(long double x, long double y)
{

	return ((x == y && !signbit(x) == !signbit(y))
		|| (isnan(x) && isnan(y)));
}

static void
test_zeroes(void)
{
	const int rd = (fegetround() == FE_DOWNWARD);

	testall(0.0, 0.0, 0.0, 0.0, ALL_STD_EXCEPT, 0);
	testall(1.0, 0.0, 0.0, 0.0, ALL_STD_EXCEPT, 0);
	testall(0.0, 1.0, 0.0, 0.0, ALL_STD_EXCEPT, 0);
	testall(0.0, 0.0, 1.0, 1.0, ALL_STD_EXCEPT, 0);

	testall(-0.0, 0.0, 0.0, rd ? -0.0 : 0.0, ALL_STD_EXCEPT, 0);
	testall(0.0, -0.0, 0.0, rd ? -0.0 : 0.0, ALL_STD_EXCEPT, 0);
	testall(-0.0, -0.0, 0.0, 0.0, ALL_STD_EXCEPT, 0);
	testall(0.0, 0.0, -0.0, rd ? -0.0 : 0.0, ALL_STD_EXCEPT, 0);
	testall(-0.0, -0.0, -0.0, rd ? -0.0 : 0.0, ALL_STD_EXCEPT, 0);

	testall(-0.0, 0.0, -0.0, -0.0, ALL_STD_EXCEPT, 0);
	testall(0.0, -0.0, -0.0, -0.0, ALL_STD_EXCEPT, 0);

	testall(-1.0, 1.0, 1.0, rd ? -0.0 : 0.0, ALL_STD_EXCEPT, 0);
	testall(1.0, -1.0, 1.0, rd ? -0.0 : 0.0, ALL_STD_EXCEPT, 0);
	testall(-1.0, -1.0, -1.0, rd ? -0.0 : 0.0, ALL_STD_EXCEPT, 0);

	switch (fegetround()) {
	case FE_TONEAREST:
	case FE_TOWARDZERO:
		test(fmaf, -FLT_MIN, FLT_MIN, 0.0, -0.0,
		     ALL_STD_EXCEPT, FE_INEXACT | FE_UNDERFLOW);
		test(fma, -DBL_MIN, DBL_MIN, 0.0, -0.0,
		     ALL_STD_EXCEPT, FE_INEXACT | FE_UNDERFLOW);
		test(fmal, -LDBL_MIN, LDBL_MIN, 0.0, -0.0,
		     ALL_STD_EXCEPT, FE_INEXACT | FE_UNDERFLOW);
	}
}

static void
test_infinities(void)
{

	testall(INFINITY, 1.0, -1.0, INFINITY, ALL_STD_EXCEPT, 0);
	testall(-1.0, INFINITY, 0.0, -INFINITY, ALL_STD_EXCEPT, 0);
	testall(0.0, 0.0, INFINITY, INFINITY, ALL_STD_EXCEPT, 0);
	testall(1.0, 1.0, INFINITY, INFINITY, ALL_STD_EXCEPT, 0);
	testall(1.0, 1.0, -INFINITY, -INFINITY, ALL_STD_EXCEPT, 0);

	testall(INFINITY, -INFINITY, 1.0, -INFINITY, ALL_STD_EXCEPT, 0);
	testall(INFINITY, INFINITY, 1.0, INFINITY, ALL_STD_EXCEPT, 0);
	testall(-INFINITY, -INFINITY, INFINITY, INFINITY, ALL_STD_EXCEPT, 0);

	testall(0.0, INFINITY, 1.0, NAN, ALL_STD_EXCEPT, FE_INVALID);
	testall(INFINITY, 0.0, -0.0, NAN, ALL_STD_EXCEPT, FE_INVALID);

	/* The invalid exception is optional in this case. */
	testall(INFINITY, 0.0, NAN, NAN, ALL_STD_EXCEPT & ~FE_INVALID, 0);

	testall(INFINITY, INFINITY, -INFINITY, NAN,
		ALL_STD_EXCEPT, FE_INVALID);
	testall(-INFINITY, INFINITY, INFINITY, NAN,
		ALL_STD_EXCEPT, FE_INVALID);
	testall(INFINITY, -1.0, INFINITY, NAN,
		ALL_STD_EXCEPT, FE_INVALID);

	test(fmaf, FLT_MAX, FLT_MAX, -INFINITY, -INFINITY, ALL_STD_EXCEPT, 0);
	test(fma, DBL_MAX, DBL_MAX, -INFINITY, -INFINITY, ALL_STD_EXCEPT, 0);
	test(fmal, LDBL_MAX, LDBL_MAX, -INFINITY, -INFINITY,
	     ALL_STD_EXCEPT, 0);
	test(fmaf, FLT_MAX, -FLT_MAX, INFINITY, INFINITY, ALL_STD_EXCEPT, 0);
	test(fma, DBL_MAX, -DBL_MAX, INFINITY, INFINITY, ALL_STD_EXCEPT, 0);
	test(fmal, LDBL_MAX, -LDBL_MAX, INFINITY, INFINITY,
	     ALL_STD_EXCEPT, 0);
}

static void
test_nans(void)
{

	testall(NAN, 0.0, 0.0, NAN, ALL_STD_EXCEPT, 0);
	testall(1.0, NAN, 1.0, NAN, ALL_STD_EXCEPT, 0);
	testall(1.0, -1.0, NAN, NAN, ALL_STD_EXCEPT, 0);
	testall(0.0, 0.0, NAN, NAN, ALL_STD_EXCEPT, 0);
	testall(NAN, NAN, NAN, NAN, ALL_STD_EXCEPT, 0);

	/* x*y should not raise an inexact/overflow/underflow if z is NaN. */
	testall(M_PI, M_PI, NAN, NAN, ALL_STD_EXCEPT, 0);
	test(fmaf, FLT_MIN, FLT_MIN, NAN, NAN, ALL_STD_EXCEPT, 0);
	test(fma, DBL_MIN, DBL_MIN, NAN, NAN, ALL_STD_EXCEPT, 0);
	test(fmal, LDBL_MIN, LDBL_MIN, NAN, NAN, ALL_STD_EXCEPT, 0);
	test(fmaf, FLT_MAX, FLT_MAX, NAN, NAN, ALL_STD_EXCEPT, 0);
	test(fma, DBL_MAX, DBL_MAX, NAN, NAN, ALL_STD_EXCEPT, 0);
	test(fmal, LDBL_MAX, LDBL_MAX, NAN, NAN, ALL_STD_EXCEPT, 0);
}

/*
 * Tests for cases where z is very small compared to x*y.
 */
static void
test_small_z(void)
{

	/* x*y positive, z positive */
	if (fegetround() == FE_UPWARD) {
		test(fmaf, 1.0, 1.0, 0x1.0p-100, 1.0 + FLT_EPSILON,
		     ALL_STD_EXCEPT, FE_INEXACT);
		test(fma, 1.0, 1.0, 0x1.0p-200, 1.0 + DBL_EPSILON,
		     ALL_STD_EXCEPT, FE_INEXACT);
		test(fmal, 1.0, 1.0, 0x1.0p-200, 1.0 + LDBL_EPSILON,
		     ALL_STD_EXCEPT, FE_INEXACT);
	} else {
		testall(0x1.0p100, 1.0, 0x1.0p-100, 0x1.0p100,
			ALL_STD_EXCEPT, FE_INEXACT);
	}

	/* x*y negative, z negative */
	if (fegetround() == FE_DOWNWARD) {
		test(fmaf, -1.0, 1.0, -0x1.0p-100, -(1.0 + FLT_EPSILON),
		     ALL_STD_EXCEPT, FE_INEXACT);
		test(fma, -1.0, 1.0, -0x1.0p-200, -(1.0 + DBL_EPSILON),
		     ALL_STD_EXCEPT, FE_INEXACT);
		test(fmal, -1.0, 1.0, -0x1.0p-200, -(1.0 + LDBL_EPSILON),
		     ALL_STD_EXCEPT, FE_INEXACT);
	} else {
		testall(0x1.0p100, -1.0, -0x1.0p-100, -0x1.0p100,
			ALL_STD_EXCEPT, FE_INEXACT);
	}

	/* x*y positive, z negative */
	if (fegetround() == FE_DOWNWARD || fegetround() == FE_TOWARDZERO) {
		test(fmaf, 1.0, 1.0, -0x1.0p-100, 1.0 - FLT_EPSILON / 2,
		     ALL_STD_EXCEPT, FE_INEXACT);
		test(fma, 1.0, 1.0, -0x1.0p-200, 1.0 - DBL_EPSILON / 2,
		     ALL_STD_EXCEPT, FE_INEXACT);
		test(fmal, 1.0, 1.0, -0x1.0p-200, 1.0 - LDBL_EPSILON / 2,
		     ALL_STD_EXCEPT, FE_INEXACT);
	} else {
		testall(0x1.0p100, 1.0, -0x1.0p-100, 0x1.0p100,
			ALL_STD_EXCEPT, FE_INEXACT);
	}

	/* x*y negative, z positive */
	if (fegetround() == FE_UPWARD || fegetround() == FE_TOWARDZERO) {
		test(fmaf, -1.0, 1.0, 0x1.0p-100, -1.0 + FLT_EPSILON / 2,
		     ALL_STD_EXCEPT, FE_INEXACT);
		test(fma, -1.0, 1.0, 0x1.0p-200, -1.0 + DBL_EPSILON / 2,
		     ALL_STD_EXCEPT, FE_INEXACT);
		test(fmal, -1.0, 1.0, 0x1.0p-200, -1.0 + LDBL_EPSILON / 2,
		     ALL_STD_EXCEPT, FE_INEXACT);
	} else {
		testall(-0x1.0p100, 1.0, 0x1.0p-100, -0x1.0p100,
			ALL_STD_EXCEPT, FE_INEXACT);
	}
}

/*
 * Tests for cases where z is very large compared to x*y.
 */
static void
test_big_z(void)
{

	/* z positive, x*y positive */
	if (fegetround() == FE_UPWARD) {
		test(fmaf, 0x1.0p-50, 0x1.0p-50, 1.0, 1.0 + FLT_EPSILON,
		     ALL_STD_EXCEPT, FE_INEXACT);
		test(fma, 0x1.0p-100, 0x1.0p-100, 1.0, 1.0 + DBL_EPSILON,
		     ALL_STD_EXCEPT, FE_INEXACT);
		test(fmal, 0x1.0p-100, 0x1.0p-100, 1.0, 1.0 + LDBL_EPSILON,
		     ALL_STD_EXCEPT, FE_INEXACT);
	} else {
		testall(-0x1.0p-50, -0x1.0p-50, 0x1.0p100, 0x1.0p100,
			ALL_STD_EXCEPT, FE_INEXACT);
	}

	/* z negative, x*y negative */
	if (fegetround() == FE_DOWNWARD) {
		test(fmaf, -0x1.0p-50, 0x1.0p-50, -1.0, -(1.0 + FLT_EPSILON),
		     ALL_STD_EXCEPT, FE_INEXACT);
		test(fma, -0x1.0p-100, 0x1.0p-100, -1.0, -(1.0 + DBL_EPSILON),
		     ALL_STD_EXCEPT, FE_INEXACT);
		test(fmal, -0x1.0p-100, 0x1.0p-100, -1.0, -(1.0 + LDBL_EPSILON),
		     ALL_STD_EXCEPT, FE_INEXACT);
	} else {
		testall(0x1.0p-50, -0x1.0p-50, -0x1.0p100, -0x1.0p100,
			ALL_STD_EXCEPT, FE_INEXACT);
	}

	/* z negative, x*y positive */
	if (fegetround() == FE_UPWARD || fegetround() == FE_TOWARDZERO) {
		test(fmaf, -0x1.0p-50, -0x1.0p-50, -1.0,
		     -1.0 + FLT_EPSILON / 2, ALL_STD_EXCEPT, FE_INEXACT);
		test(fma, -0x1.0p-100, -0x1.0p-100, -1.0,
		     -1.0 + DBL_EPSILON / 2, ALL_STD_EXCEPT, FE_INEXACT);
		test(fmal, -0x1.0p-100, -0x1.0p-100, -1.0,
		     -1.0 + LDBL_EPSILON / 2, ALL_STD_EXCEPT, FE_INEXACT);
	} else {
		testall(0x1.0p-50, 0x1.0p-50, -0x1.0p100, -0x1.0p100,
			ALL_STD_EXCEPT, FE_INEXACT);
	}

	/* z positive, x*y negative */
	if (fegetround() == FE_DOWNWARD || fegetround() == FE_TOWARDZERO) {
		test(fmaf, 0x1.0p-50, -0x1.0p-50, 1.0, 1.0 - FLT_EPSILON / 2,
		     ALL_STD_EXCEPT, FE_INEXACT);
		test(fma, 0x1.0p-100, -0x1.0p-100, 1.0, 1.0 - DBL_EPSILON / 2,
		     ALL_STD_EXCEPT, FE_INEXACT);
		test(fmal, 0x1.0p-100, -0x1.0p-100, 1.0, 1.0 - LDBL_EPSILON / 2,
		     ALL_STD_EXCEPT, FE_INEXACT);
	} else {
		testall(-0x1.0p-50, 0x1.0p-50, 0x1.0p100, 0x1.0p100,
			ALL_STD_EXCEPT, FE_INEXACT);
	}
}

static void
test_accuracy(void)
{

	/* ilogb(x*y) - ilogb(z) = 20 */
	testrnd(fmaf, -0x1.c139d8p-51, -0x1.600e7ap32, 0x1.26558cp-38,
		0x1.34e48ap-18, 0x1.34e48cp-18, 0x1.34e48ap-18, 0x1.34e48ap-18,
		ALL_STD_EXCEPT, FE_INEXACT);
	testrnd(fma, -0x1.c139d7b84f1a3p-51, -0x1.600e7a2a16484p32,
		0x1.26558cac31580p-38, 0x1.34e48a78aae97p-18,
		0x1.34e48a78aae97p-18, 0x1.34e48a78aae96p-18,
		0x1.34e48a78aae96p-18, ALL_STD_EXCEPT, FE_INEXACT);
#if LDBL_MANT_DIG == 113
	testrnd(fmal, -0x1.c139d7b84f1a3079263afcc5bae3p-51L,
		-0x1.600e7a2a164840edbe2e7d301a72p32L,
		0x1.26558cac315807eb07e448042101p-38L,
		0x1.34e48a78aae96c76ed36077dd387p-18L,
		0x1.34e48a78aae96c76ed36077dd388p-18L,
		0x1.34e48a78aae96c76ed36077dd387p-18L,
		0x1.34e48a78aae96c76ed36077dd387p-18L,
		ALL_STD_EXCEPT, FE_INEXACT);
#elif LDBL_MANT_DIG == 64
	testrnd(fmal, -0x1.c139d7b84f1a307ap-51L, -0x1.600e7a2a164840eep32L,
		0x1.26558cac315807ecp-38L, 0x1.34e48a78aae96c78p-18L,
		0x1.34e48a78aae96c78p-18L, 0x1.34e48a78aae96c76p-18L,
		0x1.34e48a78aae96c76p-18L, ALL_STD_EXCEPT, FE_INEXACT);
#elif LDBL_MANT_DIG == 53
	testrnd(fmal, -0x1.c139d7b84f1a3p-51L, -0x1.600e7a2a16484p32L,
		0x1.26558cac31580p-38L, 0x1.34e48a78aae97p-18L,
		0x1.34e48a78aae97p-18L, 0x1.34e48a78aae96p-18L,
		0x1.34e48a78aae96p-18L, ALL_STD_EXCEPT, FE_INEXACT);
#endif

	/* ilogb(x*y) - ilogb(z) = -40 */
	testrnd(fmaf, 0x1.98210ap53, 0x1.9556acp-24, 0x1.d87da4p70,
		0x1.d87da4p70, 0x1.d87da6p70, 0x1.d87da4p70, 0x1.d87da4p70,
		ALL_STD_EXCEPT, FE_INEXACT);
	testrnd(fma, 0x1.98210ac83fe2bp53, 0x1.9556ac1475f0fp-24,
		0x1.d87da3aafc60ep70, 0x1.d87da3aafda40p70,
		0x1.d87da3aafda40p70, 0x1.d87da3aafda3fp70,
		0x1.d87da3aafda3fp70, ALL_STD_EXCEPT, FE_INEXACT);
#if LDBL_MANT_DIG == 113
	testrnd(fmal, 0x1.98210ac83fe2a8f65b6278b74cebp53L,
		0x1.9556ac1475f0f28968b61d0de65ap-24L,
		0x1.d87da3aafc60d830aa4c6d73b749p70L,
		0x1.d87da3aafda3f36a69eb86488224p70L,
		0x1.d87da3aafda3f36a69eb86488225p70L,
		0x1.d87da3aafda3f36a69eb86488224p70L,
		0x1.d87da3aafda3f36a69eb86488224p70L,
		ALL_STD_EXCEPT, FE_INEXACT);
#elif LDBL_MANT_DIG == 64
	testrnd(fmal, 0x1.98210ac83fe2a8f6p53L, 0x1.9556ac1475f0f28ap-24L,
		0x1.d87da3aafc60d83p70L, 0x1.d87da3aafda3f36ap70L,
		0x1.d87da3aafda3f36ap70L, 0x1.d87da3aafda3f368p70L,
		0x1.d87da3aafda3f368p70L, ALL_STD_EXCEPT, FE_INEXACT);
#elif LDBL_MANT_DIG == 53
	testrnd(fmal, 0x1.98210ac83fe2bp53L, 0x1.9556ac1475f0fp-24L,
		0x1.d87da3aafc60ep70L, 0x1.d87da3aafda40p70L,
		0x1.d87da3aafda40p70L, 0x1.d87da3aafda3fp70L,
		0x1.d87da3aafda3fp70L, ALL_STD_EXCEPT, FE_INEXACT);
#endif
}

int
main(int argc, char *argv[])
{
	int rmodes[] = { FE_TONEAREST, FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO };
	int i;

	printf("1..18\n");

	for (i = 0; i < 4; i++) {
		fesetround(rmodes[i]);
		test_zeroes();
		printf("ok %d - fma zeroes\n", i + 1);
	}

	for (i = 0; i < 4; i++) {
		fesetround(rmodes[i]);
		test_infinities();
		printf("ok %d - fma infinities\n", i + 5);
	}

	fesetround(FE_TONEAREST);
	test_nans();
	printf("ok 9 - fma NaNs\n");

	for (i = 0; i < 4; i++) {
		fesetround(rmodes[i]);
		test_small_z();
		printf("ok %d - fma small z\n", i + 10);
	}

	for (i = 0; i < 4; i++) {
		fesetround(rmodes[i]);
		test_big_z();
		printf("ok %d - fma big z\n", i + 14);
	}

	fesetround(FE_TONEAREST);
	test_accuracy();
	printf("ok 18 - fma accuracy\n");

	/*
	 * TODO:
	 * - Tests for subnormals
	 * - Cancellation tests (e.g., z = (double)x*y, but x*y is inexact)
	 */

	return (0);
}
