/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The MidnightBSD Project
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

#include <sys/param.h>

#include "libc_private.h"

/*
 * The kernel reports __MidnightBSD_version as its osreldate, but libc and
 * rtld compare the value returned by __getosreldate() against FreeBSD
 * version thresholds (INO64_FIRST, P_OSREL_*, ...).  Translate a
 * MidnightBSD version to the __FreeBSD_version of the tree that release
 * was built from, so those checks see the features the kernel really has.
 *
 * Each entry is the first MidnightBSD version with that FreeBSD version.
 * Where one MidnightBSD version spanned several FreeBSD versions, the
 * lowest is used, so a feature is never assumed before it existed.  Keep
 * this table in sync with sys/sys/param.h when the FreeBSD base changes.
 */
static const struct {
	int	mnbsd;
	int	fbsd;
} osrel_map[] = {
	{ 400001, 1305500 },	/* 4.0 */
	{ 301006, 1204500 },	/* 3.1 */
	{ 300003, 1203507 },	/* 3.0 */
	{ 200000, 1103507 },	/* 2.0 */
	{ 100001, 1004501 },	/* 1.0 */
};

/*
 * MidnightBSD versions are well below the oldest FreeBSD version userland
 * still checks for.  Anything at or above this is already a FreeBSD
 * version (for example, a jail created with a FreeBSD osreldate) and is
 * returned unchanged.
 */
#define	OSREL_FREEBSD_MIN	700000

int
__osrel_to_freebsd(int osrel)
{
	size_t i;

	if (osrel <= 0 || osrel >= OSREL_FREEBSD_MIN)
		return (osrel);

	/* A kernel at least as new as this libc has everything it knows. */
	if (osrel >= __MidnightBSD_version)
		return (__FreeBSD_version);

	for (i = 0; i < nitems(osrel_map); i++) {
		if (osrel >= osrel_map[i].mnbsd)
			return (osrel_map[i].fbsd);
	}

	/* Older than any release with a FreeBSD 10 base. */
	return (osrel);
}
