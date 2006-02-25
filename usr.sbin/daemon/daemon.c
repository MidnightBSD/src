/*-
 * Copyright (c) 1999 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	From BSDI: daemon.c,v 1.2 1996/08/15 01:11:09 jch Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.sbin/daemon/daemon.c,v 1.3 2004/06/02 06:48:13 phk Exp $");

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void usage(void);

int
main(int argc, char *argv[])
{
	int ch, nochdir, noclose, errcode;
	FILE *pidf;
	const char *pidfile;

	nochdir = noclose = 1;
	pidfile = NULL;
	while ((ch = getopt(argc, argv, "-cfp:")) != -1) {
		switch (ch) {
		case 'c':
			nochdir = 0;
			break;
		case 'f':
			noclose = 0;
			break;
		case 'p':
			pidfile = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();
	/*
	 * Try to open the pidfile before calling daemon(3),
	 * to be able to report the error intelligently
	 */
	if (pidfile) {
		pidf = fopen(pidfile, "w");
		if (pidf == NULL)
			err(2, "pidfile ``%s''", pidfile);
	}

	if (daemon(nochdir, noclose) == -1)
		err(1, NULL);

	/* Now that we are the child, write out the pid */
	if (pidfile) {
		fprintf(pidf, "%lu\n", (unsigned long)getpid());
		fclose(pidf);
	}

	execvp(argv[0], argv);

	/*
	 * execvp() failed -- unlink pidfile if any, and
	 * report the error
	 */
	errcode = errno; /* Preserve errcode -- unlink may reset it */
	if (pidfile)
		unlink(pidfile);

	/* The child is now running, so the exit status doesn't matter. */
	errc(1, errcode, "%s", argv[0]);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: daemon [-cf] [-p pidfile] command arguments ...\n");
	exit(1);
}
