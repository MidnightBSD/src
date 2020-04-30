/*
 * infinity.c
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: stable/11/lib/libc/riscv/gen/infinity.c 294227 2016-01-17 15:21:23Z br $");

#include <math.h>

/* bytes for +Infinity on riscv */
const union __infinity_un __infinity = { { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f } };

/* bytes for NaN */
const union __nan_un __nan = { { 0, 0, 0xc0, 0xff } };
