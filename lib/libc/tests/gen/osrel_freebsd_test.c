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

/*
 * Tests for __osrel_to_freebsd(), which libc and rtld use to turn the
 * kernel's MidnightBSD osreldate into a FreeBSD version for ABI checks.
 * The function is private to libc, so its source is built into this test.
 */

#include <sys/param.h>
#include <sys/sysctl.h>

#include <atf-c.h>

int __osrel_to_freebsd(int);

/* From lib/libc/sys/compat-ino64.h. */
#define	INO64_FIRST	1200031

ATF_TC_WITHOUT_HEAD(passthrough);
ATF_TC_BODY(passthrough, tc)
{
	/* Unset, invalid, and values that are already FreeBSD versions. */
	ATF_CHECK_EQ(__osrel_to_freebsd(0), 0);
	ATF_CHECK_EQ(__osrel_to_freebsd(-1), -1);
	ATF_CHECK_EQ(__osrel_to_freebsd(700000), 700000);
	ATF_CHECK_EQ(__osrel_to_freebsd(1305500), 1305500);
	ATF_CHECK_EQ(__osrel_to_freebsd(1400000), 1400000);

	/* Older than any release with a FreeBSD 10 base. */
	ATF_CHECK_EQ(__osrel_to_freebsd(9005), 9005);
	ATF_CHECK_EQ(__osrel_to_freebsd(100000), 100000);
}

ATF_TC_WITHOUT_HEAD(table_boundaries);
ATF_TC_BODY(table_boundaries, tc)
{
	ATF_CHECK_EQ(__osrel_to_freebsd(100001), 1004501);
	ATF_CHECK_EQ(__osrel_to_freebsd(199999), 1004501);
	ATF_CHECK_EQ(__osrel_to_freebsd(200000), 1103507);
	ATF_CHECK_EQ(__osrel_to_freebsd(300002), 1103507);
	ATF_CHECK_EQ(__osrel_to_freebsd(300003), 1203507);
	ATF_CHECK_EQ(__osrel_to_freebsd(301005), 1203507);
	ATF_CHECK_EQ(__osrel_to_freebsd(301006), 1204500);
	ATF_CHECK_EQ(__osrel_to_freebsd(400000), 1204500);
	ATF_CHECK_EQ(__osrel_to_freebsd(400001), 1305500);
}

ATF_TC_WITHOUT_HEAD(current_release);
ATF_TC_BODY(current_release, tc)
{
	/* A kernel at least as new as this libc maps to its FreeBSD base. */
	ATF_CHECK_EQ(__osrel_to_freebsd(__MidnightBSD_version),
	    __FreeBSD_version);
	ATF_CHECK_EQ(__osrel_to_freebsd(__MidnightBSD_version + 1),
	    __FreeBSD_version);
	ATF_CHECK_EQ(__osrel_to_freebsd(699999), __FreeBSD_version);
}

ATF_TC_WITHOUT_HEAD(running_kernel);
ATF_TC_BODY(running_kernel, tc)
{
	size_t len;
	int osrel;

	len = sizeof(osrel);
	ATF_REQUIRE(sysctlbyname("kern.osreldate", &osrel, &len, NULL, 0) ==
	    0);

	/*
	 * The running kernel has the ino64 syscalls, so libc must not fall
	 * back to the freebsd11 compat versions.
	 */
	ATF_CHECK_MSG(__osrel_to_freebsd(osrel) >= INO64_FIRST,
	    "kern.osreldate %d translates to %d, below INO64_FIRST", osrel,
	    __osrel_to_freebsd(osrel));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, passthrough);
	ATF_TP_ADD_TC(tp, table_boundaries);
	ATF_TP_ADD_TC(tp, current_release);
	ATF_TP_ADD_TC(tp, running_kernel);

	return (atf_no_error());
}
