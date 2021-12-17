/*-
 * Copyright (c) 2007 Chris Reinhardt
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
 *
 * $MidnightBSD: src/libexec/mport.merge/mport.merge.c,v 1.3 2011/10/23 00:23:38 laffer1 Exp $
 */

#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <sysexits.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <mport.h>


static void usage(void);

int main(int argc, char *argv[]) {
	int ch, i;
	const char *outfile = NULL;
	const char **inputfiles;
	mportInstance *mport;
	const char *chroot_path = NULL;

	if (argc == 1)
		usage();

	while ((ch = getopt(argc, argv, "c:o:")) != -1) {
		switch (ch) {
			case 'c':
				chroot_path = optarg;
				break;
			case 'o':
				outfile = optarg;
				break;
			case '?':
			default:
				usage();
				break;
		}
	}

	argc -= optind;
	argv += optind;

	if (outfile == NULL)
		usage();

	if (chroot_path != NULL) {
		if (chroot(chroot_path) == -1) {
			err(EXIT_FAILURE, "chroot failed");
		}
	}

	mport = mport_instance_new();
	if (mport_instance_init(mport, NULL) != MPORT_OK) {
		warnx("%s", mport_err_string());
		exit(EXIT_FAILURE);
	}

	if ((inputfiles = (const char **) malloc((argc + 1) * sizeof(char **))) == NULL)
		err(EX_OSERR, "Couldn't allocate input array");

	for (i = 0; i < argc; i++) {
		if ((inputfiles[i] = strdup(argv[i])) == NULL)
			err(EX_OSERR, "Couldn't allocate input filename");
	}

	inputfiles[i] = NULL;

	if (mport_merge_primative(mport, (const char **) inputfiles, outfile) != MPORT_OK)
		errx(EX_SOFTWARE, "Could not merge package files: %s", mport_err_string());

	free(inputfiles);

	return 0;
}


static void
usage(void) {
	fprintf(stderr, "Usage: mport.merge -c <chroot path> -o <outputfilename> <pkgfile1> <pkgfile2> ...\n");
	exit(2);
}
