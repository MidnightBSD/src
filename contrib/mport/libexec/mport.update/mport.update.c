/*-
 * Copyright (c) 2021 Lucas Holt
 * Copyright (c) 2009 Chris Reinhardt
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
 */

#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <mport.h>

static void usage(void);

int main(int argc, char *argv[]) {
	int i, ch;
	mportInstance *mport;
	const char *chroot_path = NULL;

	while ((ch = getopt(argc, argv, "c:")) != -1) {
		switch (ch) {
			case 'c':
				chroot_path = optarg;
				break;
			case '?':
			default:
				usage();
				break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 1)
		usage();

	argv++;
	argc--;

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

	for (i = 0; i < argc; i++) {
		if (mport_update_primative(mport, argv[i]) != MPORT_OK) {
			warnx("%s", mport_err_string());
			mport_instance_free(mport);
			exit(EXIT_FAILURE);
		}
	}

	mport_instance_free(mport);

	return (0);
}

static void
usage(void) {

	fprintf(stderr, "Usage: mport.update [-c <chroot directory>] pkgfile1 pkgfile2 ...\n");
	exit(2);
}
