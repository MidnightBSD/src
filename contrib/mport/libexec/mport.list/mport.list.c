/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2010, 2011, 2013, 2014 Lucas Holt
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
main(int argc, char *argv[])
{
	int ch;
	mportInstance *mport;

	mportListPrint printOpts;
	bool noIndex = false;
	bool quiet = false;
	bool verbose = false;

	const char *chroot_path = NULL;

	if (argc > 3)
		usage();

	while ((ch = getopt(argc, argv, "c:lopqvuU")) != -1) {
		switch (ch) {
		case 'c':
			chroot_path = optarg;
			break;
		case 'l':
			printOpts.locks = true;
			break;
		case 'o':
			printOpts.origin = true;
			break;
		case 'p':
			printOpts.prime = true;
			break;
		case 'q':
			quiet = true;
			break;
		case 'v':
			verbose = true;
			break;
		case 'u':
			printOpts.update = true;
			break;
		case 'U':
			noIndex = true;
			break;
		case '?':
		default:
			usage();
			break;
		}
	}

	if (chroot_path != NULL) {
		if (chroot(chroot_path) == -1) {
			err(EXIT_FAILURE, "chroot failed");
		}
	}

	mport = mport_instance_new();
	if (mport_instance_init(mport, NULL, NULL, noIndex, mport_verbosity(quiet, verbose)) != MPORT_OK) {
		warnx("%s", mport_err_string());
		exit(EXIT_FAILURE);
	}

	if (!noIndex && printOpts.update && mport_index_load(mport) != MPORT_OK) {
		warnx("Unable to load updates index, %s", mport_err_string());
		mport_instance_free(mport);
		exit(8);
	}

	int ret = mport_list_print(mport, &printOpts);
	mport_instance_free(mport);
	return (ret);
}

static void
usage(void)
{

	fprintf(stderr, "Usage: mport.list [-q | -v | -u | -c <chroot path>]\n");

	exit(2);
}
