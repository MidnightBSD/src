/*
 * The contents of this file are in the public domain.
 * Written by Garrett A. Wollman, 2000-10-07.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/lib/libc/gen/pmadvise.c 118684 2003-08-09 03:23:24Z bms $");

#include <sys/mman.h>

int
posix_madvise(void *address, size_t size, int how)
{
	return madvise(address, size, how);
}
