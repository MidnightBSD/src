/*-
 * Copyright (c) 2021 Lucas Holt
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
 */

#include <sys/cdefs.h>

#include <stdlib.h>
#include <stdio.h>
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
	char *prefix = NULL;
	mportInstance *mport;
	int error_code = 0;
	const char *chroot_path = NULL;

	while ((ch = getopt(argc, argv, "c:p:")) != -1) {
		switch (ch) {
			case 'c':
				chroot_path = optarg;
				break;
			case 'p':
				prefix = optarg;
				break;
			case '?':
			default:
				usage();
				break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	if (chroot_path != NULL) {
		if (chroot(chroot_path) == -1) {
			err(EXIT_FAILURE, "chroot failed");
		}
	}

	mport = mport_instance_new();

	if (mport_instance_init(mport, NULL) != MPORT_OK) {
		warnx("Init failed: %s", mport_err_string());
		return EXIT_FAILURE;
	}

	for (int i = 0; i < argc; i++) {

		if (mport_install_primative(mport, argv[i], prefix, MPORT_EXPLICIT) != MPORT_OK) {
			warnx("install failed: %s", mport_err_string());
			mport_instance_free(mport);
			exit(EXIT_FAILURE);
		}
	}


	mport_instance_free(mport);
	exit(error_code);
}

static
void usage(void)
{

	fprintf(stderr, "Usage: mport.install [-p prefix] [-c <chroot path>] pkgfile1 pkgfile2 ...\n");
	exit(1);
}
