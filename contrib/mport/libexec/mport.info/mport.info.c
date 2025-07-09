/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2010, 2022 Lucas Holt
 * Copyright (c) 2008 Chris Reinhardt
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

#include <sys/cdefs.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <mport.h>

static void usage(void);

int
main(int argc, char *argv[]) {
	int ch;
	mportInstance *mport;
	mportPackageMeta **packs;
	bool quiet = false;
	bool verbose = false;
	bool origin = false;
	bool xFlag = false;
	const char *chroot_path = NULL;

	while ((ch = getopt(argc, argv, "c:oqvx")) != -1) {
		switch (ch) {
			case 'c':
				chroot_path = optarg;
				break;
			case 'o':
				origin = true;
				break;
			case 'q':
				quiet = true;
				break;
			case 'v':
				verbose = true;
				break;
			case 'x':
			    xFlag = true;
				quiet = true;
				break;
			case '?':
			default:
				usage();
				break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argv[0] == NULL) {
		warnx("Missing origin");
		exit(2);
	}

	if (chroot_path != NULL) {
		if (chroot(chroot_path) == -1) {
			err(EXIT_FAILURE, "chroot failed");
		}
	}

	mport = mport_instance_new();

	if (mport_instance_init(mport, NULL, NULL, false, mport_verbosity(quiet, verbose, false)) != MPORT_OK) {
		warnx("%s", mport_err_string());
		mport_instance_free(mport);
		exit(EXIT_FAILURE);
	}

	if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
		warnx("%s", mport_err_string());
		mport_instance_free(mport);
		exit(EXIT_FAILURE);
	}

	if (packs == NULL) {
		if (mport->verbosity != MPORT_VQUIET)
			warnx("No packages installed matching.");
		mport_instance_free(mport);
		exit(3);
	}

	if (mport->verbosity != MPORT_VQUIET && origin)
		printf("The following installed package(s) has %s origin:\n", argv[0]);

	while (*packs != NULL) {
		if (origin && strcmp(argv[0], (*packs)->origin) == 0) {
			if (xFlag) {
				printf("%s-%s\n", (*packs)->name, (*packs)->version);
			} else {
				printf("%s-%s\t\t%s\n", (*packs)->name, (*packs)->version,
				       (*packs)->origin);
			}
		} else if (strcmp(argv[0], (*packs)->name) == 0) {
			if (xFlag) {
				printf("%s-%s\n", (*packs)->name, (*packs)->version);
			} else {
                printf("%s-%s\t\t%s\n", (*packs)->name, (*packs)->version,
                       (*packs)->origin);
            }
		}

		packs++;
	}

	mport_instance_free(mport);

	return 0;
}


static void
usage(void) {
	fprintf(stderr, "Usage: mport.info [-o | -q | -v | -x] [-c <chroot directory>] <origin>\n");
	exit(2);
}
