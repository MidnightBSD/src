/*
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mount_union.c	8.5 (Berkeley) 3/27/94";
#else
static const char rcsid[] =
  "$FreeBSD: src/sbin/mount_unionfs/mount_unionfs.c,v 1.24 2005/06/10 09:51:43 delphij Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/uio.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "mntopts.h"

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_END
};

static int	subdir(const char *, const char *);
static void	usage (void) __dead2;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct iovec iov[8];
	int ch, mntflags;
	char source[MAXPATHLEN];
	char target[MAXPATHLEN];
	int iovcnt;

	iovcnt = 6;
	mntflags = 0;
	while ((ch = getopt(argc, argv, "bo:r")) != -1)
		switch (ch) {
		case 'b':
			iov[6].iov_base = "below";
			iov[6].iov_len = strlen(iov[6].iov_base) + 1;
			iov[7].iov_base = NULL;
			iov[7].iov_len = 0;
			iovcnt = 8;
			break;
		case 'o':
			getmntopts(optarg, mopts, &mntflags, 0);
			break;
		case 'r':
			iov[6].iov_base = "replace";
			iov[6].iov_len = strlen(iov[6].iov_base) + 1;
			iov[7].iov_base = NULL;
			iov[7].iov_len = 0;
			iovcnt = 8;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	/* resolve both target and source with realpath(3) */
	(void)checkpath(argv[0], target);
	(void)checkpath(argv[1], source);

	if (subdir(target, source) || subdir(source, target))
		errx(EX_USAGE, "%s (%s) and %s (%s) are not distinct paths",
		    argv[0], target, argv[1], source);

	iov[0].iov_base = "fstype";
	iov[0].iov_len = strlen(iov[0].iov_base) + 1;
	iov[1].iov_base = "unionfs";
	iov[1].iov_len = strlen(iov[1].iov_base) + 1;
	iov[2].iov_base = "fspath";
	iov[2].iov_len = strlen(iov[2].iov_base) + 1;
	iov[3].iov_base = source;
	iov[3].iov_len = strlen(source) + 1;
	iov[4].iov_base = "target";
	iov[4].iov_len = strlen(iov[4].iov_base) + 1;
	iov[5].iov_base = target;
	iov[5].iov_len = strlen(target) + 1;
	if (nmount(iov, iovcnt, mntflags))
		err(EX_OSERR, "%s", target);
	exit(0);
}

int
subdir(p, dir)
	const char *p;
	const char *dir;
{
	int l;

	l = strlen(dir);
	if (l <= 1)
		return (1);

	if ((strncmp(p, dir, l) == 0) && (p[l] == '/' || p[l] == '\0'))
		return (1);

	return (0);
}

void
usage()
{
	(void)fprintf(stderr,
		"usage: mount_unionfs [-br] [-o options] directory uniondir\n");
	exit(EX_USAGE);
}
