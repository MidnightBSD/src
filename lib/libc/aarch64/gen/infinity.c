/*
 * infinity.c
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: stable/11/lib/libc/aarch64/gen/infinity.c 286959 2015-08-20 13:11:52Z andrew $");

#include <math.h>

/* bytes for +Infinity on aarch64 */
const union __infinity_un __infinity = { { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f } };

/* bytes for NaN */
const union __nan_un __nan = { { 0, 0, 0xc0, 0xff } };
