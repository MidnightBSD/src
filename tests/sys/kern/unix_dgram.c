/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Gleb Smirnoff <glebius@FreeBSD.org>
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
 * Subset of FreeBSD's tests/sys/kern/unix_dgram.c: only the fchmod(2) test
 * is carried here, as the remaining tests depend on newer socket buffer
 * infrastructure that this kernel does not yet have.
 */

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(fchmod);
ATF_TC_BODY(fchmod, tc)
{
	struct stat sb;
	struct sockaddr_un sun;
	int error, sd;

	memset(&sun, 0, sizeof(sun));
	sun.sun_len = sizeof(sun);
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, "sock", sizeof(sun.sun_path));

	sd = socket(PF_UNIX, SOCK_DGRAM, 0);
	ATF_REQUIRE(sd != -1);

	error = fchmod(sd, 0600 | S_ISUID);
	ATF_REQUIRE_ERRNO(EINVAL, error == -1);

	umask(0022);
	error = fchmod(sd, 0766);
	ATF_REQUIRE(error == 0);

	error = bind(sd, (struct sockaddr *)&sun, sizeof(sun));
	ATF_REQUIRE(error == 0);

	error = stat(sun.sun_path, &sb);
	ATF_REQUIRE(error == 0);
	ATF_REQUIRE_MSG((sb.st_mode & 0777) == 0744,
	    "sb.st_mode = %o", sb.st_mode);

	error = fchmod(sd, 0666);
	ATF_REQUIRE_ERRNO(EINVAL, error == -1);

	ATF_REQUIRE(close(sd) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, fchmod);

	return (atf_no_error());
}
