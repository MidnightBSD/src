/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Lucas Holt
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
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <mport.h>

#define MAX_WHERE_LEN 1024

static char *build_where(int, char **);

static void usage(void);

int main(int argc, char *argv[]) {
	int ch;
	mportInstance *mport;
	mportPackageMeta **packs;
	char *where;
	int quiet = 0;
	int origin = 0;
	const char *chroot_path = NULL;

	if (argc == 1)
		usage();

	while ((ch = getopt(argc, argv, "c:oq")) != -1) {
		switch (ch) {
			case 'c':
				chroot_path = optarg;
				break;
			case 'o':
				origin = 1;
				break;
			case 'q':
				quiet = 1;
				break;
			case '?':
			default:
				usage();
				break;
		}
	}

	argc -= optind;
	argv += optind;

	if (chroot_path != NULL) {
		if (chroot(chroot_path) == -1) {
			err(EXIT_FAILURE, "chroot failed");
		}
	}

	mport = mport_instance_new();

	if (mport_instance_init(mport, NULL, NULL, false) != MPORT_OK) {
		warnx("%s", mport_err_string());
		mport_instance_free(mport);
		exit(EXIT_FAILURE);
	}

	where = build_where(argc, argv);

	if (mport_pkgmeta_search_master(mport, &packs, where) != MPORT_OK) {
		warnx("(where: %s): %s", where, mport_err_string());
		mport_instance_free(mport);
		free(where);
		exit(EXIT_FAILURE);
	}

	if (packs == NULL) {
		if (!quiet)
			warnx("No packages installed matching.");
		mport_instance_free(mport);
		free(where);
		exit(3);
	}

	while (*packs != NULL) {
		if (quiet && origin) {
			printf("%s\n", (*packs)->origin);
		} else if (!quiet) {
			if (origin) {
				printf("%s-%s\t\t%s\n", (*packs)->name, (*packs)->version,
				       (*packs)->origin);
			} else {
				printf("%s-%s\n", (*packs)->name, (*packs)->version);
			}
		}
		packs++;
	}

	mport_instance_free(mport);
	free(where);

	return 0;
}


static void
usage(void) {
	fprintf(stderr, "Usage: mport.query [-oq] [-c <chroot directory>] <query statement>\n");
	exit(2);
}


static char *
build_where(int argc, char **argv) {
	char *arg, *column, *clause = NULL;
	char *where = (char *) malloc(sizeof(char) * (MAX_WHERE_LEN));
	char op[3];
	int started = 0, i;
	size_t cur;

	for (i = 0; i < argc; i++) {
		column = arg = argv[i];

		if ((cur = strcspn(arg, "=><")) == strlen(arg)) {
			warnx("No op for arg: %s", arg);
			usage();
		}

		/* we're being a little circumspect here because we do not want to inject anything
		   from the outside into sqlite */
		if (arg[cur] == '>') {
			if (arg[cur + 1] == '=') {
				strlcpy(op, ">=", sizeof(op));
				arg += cur + 2;
			} else {
				strlcpy(op, ">", sizeof(op));
				arg += cur + 1;
			}
		} else if (arg[cur] == '<') {
			if (arg[cur + 1] == '=') {
				strlcpy(op, "<=", sizeof(op));
				arg += cur + 2;
			} else {
				strlcpy(op, "<", sizeof(op));
				arg += cur + 1;
			}
		} else {
			strlcpy(op, "=", sizeof(op));
			arg += cur + 1;
		}

		column[cur] = '\0';

		if (strcmp(column, "name") == 0) {
			clause = sqlite3_mprintf("pkg GLOB %Q", arg);
		} else if (strcmp(column, "origin") == 0) {
			clause = sqlite3_mprintf("origin=%Q", arg);
		} else if (strcmp(column, "version") == 0) {
			clause = sqlite3_mprintf("mport_version_cmp(version, %Q)%s0", arg, op);
		} else {
			usage();
		}

		if (started == 0) {
			strlcpy(where, clause, MAX_WHERE_LEN);
			started++;
		} else {
			strlcat(where, " AND ", MAX_WHERE_LEN);
			strlcat(where, clause, MAX_WHERE_LEN);
		}

		sqlite3_free(clause);
	}

	return where;
}

