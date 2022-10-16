/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
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
	int ch, i;
	mportInstance *mport;
	mportPackageMeta **packs;
	mportPackageMeta **depends;
	const char *arg = NULL, *where = NULL;
	const char *chroot_path = NULL;

	if (argc == 1)
		usage();

	while ((ch = getopt(argc, argv, "c:o:n:")) != -1) {
		switch (ch) {
			case 'c':
				chroot_path = optarg;
				break;
			case 'o':
				where = "origin=%Q";
				arg = optarg;
				break;
			case 'n':
				where = "pkg=%Q";
				arg = optarg;
				break;
			case '?':
			default:
				usage();
				break;
		}
	}

	if (arg == NULL || where == NULL)
		usage();

	if (chroot_path != NULL) {
		if (chroot(chroot_path) == -1) {
			err(EXIT_FAILURE, "chroot failed");
		}
	}

	if ((mport = mport_instance_new()) == NULL) {
		warnx("Out of memory");
		exit(1);
	}

	if (mport_instance_init(mport, NULL) != MPORT_OK) {
		warnx("%s", mport_err_string());
		exit(1);
	}

	if (mport_pkgmeta_search_master(mport, &packs, where, arg) != MPORT_OK) {
		warnx("%s", mport_err_string());
		mport_instance_free(mport);
		exit(EXIT_FAILURE);
	}

	if (packs == NULL) {
		warnx("No packages installed matching '%s'", arg);
		mport_instance_free(mport);
		exit(3);
	}

	if (packs[1] != NULL) {
		warnx("Ambiguous package identifier: %s", arg);
		mport_instance_free(mport);
		exit(3);
	}

	if (mport_pkgmeta_get_updepends(mport, packs[0], &depends) != MPORT_OK) {
		warnx("%s", mport_err_string());
		mport_instance_free(mport);
		exit(EXIT_FAILURE);
	}

	if (depends == NULL) {
		/* no depends, nothing to print. */
		mport_instance_free(mport);
		exit(EXIT_SUCCESS);
	}

	i = 0;
	while (depends[i] != NULL) {
		(void) printf("%s\n", depends[i]->origin);
		i++;
	}

	mport_instance_free(mport);

	return (0);
}


static void
usage(void) {
	fprintf(stderr, "Usage: mport.updepends -n pkgname\n"
	                "       mport.updepends -o origin -c <chroot path>\n");
	exit(2);
}
