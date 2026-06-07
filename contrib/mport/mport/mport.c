/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2010-2018, 2021, 2026 Lucas Holt
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <err.h>
#include <locale.h>
#include <getopt.h>
#include <mport.h>
#include <mport_private.h>
#include <libutil.h>

#define MPORT_TOOLS_PATH "/usr/libexec/"

static void usage(void);

static void show_version(/*@null@*/ mportInstance *, int);

static int loadIndex(/*@notnull@*/ mportInstance *);

static /*@only@*/ mportIndexEntry **lookupIndex(
    /*@notnull@*/ mportInstance *, /*@notnull@*/ const char *);

static int add(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *filename,
    mportAutomatic automatic);
static int install(/*@notnull@*/ mportInstance *, /*@notnull@*/ const char *, mportAutomatic);
static int reinstall_dependents(/*@notnull@*/ mportInstance *, /*@notnull@*/ const char *);
static int reinstall_dependents_impl(/*@notnull@*/ mportInstance *, /*@notnull@*/ const char *,
    /*@notnull@*/ /*@out@*/ char ***, /*@notnull@*/ /*@out@*/ size_t *,
    /*@notnull@*/ /*@out@*/ size_t *);

static int cpeList(/*@notnull@*/ mportInstance *);
static int cpeGet(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *packageName);
static int purlList(/*@notnull@*/ mportInstance *);
static int purlGet(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *packageName);

static int configGet(/*@notnull@*/ mportInstance *, /*@notnull@*/ const char *);

static int configSet(
    /*@notnull@*/ mportInstance *, /*@notnull@*/ const char *, /*@notnull@*/ const char *);

static int delete(/*@notnull@*/ mportInstance *, /*@notnull@*/ const char *);

static int deleteMany(
    /*@notnull@*/ mportInstance *mport, int argc, /*@notnull@*/ char *argv[], bool skipFirst);

static int deleteAll(/*@notnull@*/ mportInstance *);

static int info(/*@notnull@*/ mportInstance *, /*@null@*/ const char *);

static int search(/*@notnull@*/ mportInstance *, /*@null@*/ char **);

static int stats(/*@notnull@*/ mportInstance *mport);

static int clean(/*@notnull@*/ mportInstance *);

static int verify(/*@notnull@*/ mportInstance *);
static int verify_many(
    /*@notnull@*/ mportInstance *mport, int argc, /*@notnull@*/ char *argv[], bool skipFirst);

static int lock(/*@notnull@*/ mportInstance *, /*@notnull@*/ const char *);

static int unlock(/*@notnull@*/ mportInstance *, /*@notnull@*/ const char *);

static int which(/*@notnull@*/ mportInstance *, /*@null@*/ const char *, bool);

static int audit(/*@notnull@*/ mportInstance *, bool);

static int audit_package(/*@notnull@*/ mportInstance *, /*@notnull@*/ const char *, bool);

static int selectMirror(/*@notnull@*/ mportInstance *mport);

static mportPackageMeta **lookup_package(
    /*@notnull@*/ mportInstance *mport, /*@null@*/ const char *packageName);

static int annotate_show(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *packageName,
    /*@notnull@*/ const char *tagName);
static int annotate_list(/*@notnull@*/ mportInstance *mport, /*@null@*/ const char *tagName);
static int annotate_delete(/*@notnull@*/ mportInstance *mport,
    /*@notnull@*/ const char *packageName, /*@notnull@*/ const char *tagName);
static int annotate_add(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *packageName,
    /*@notnull@*/ const char *tagName, /*@notnull@*/ const char *tagValue);

static mportPackageMeta **
sort_dependencies_topological(
    mportInstance *mport, mportPackageMeta **flat_packs, int package_count, bool reverse_edges)
{
	mportPackageMeta **sorted_packs = calloc((size_t)package_count, sizeof(mportPackageMeta *));
	int *in_degree = calloc((size_t)package_count, sizeof(int));
	bool *queued = calloc((size_t)package_count, sizeof(bool));

	struct edge {
		int to;
		struct edge *next;
	};
	struct edge **adj = calloc((size_t)package_count, sizeof(struct edge *));

	if (sorted_packs == NULL || in_degree == NULL || adj == NULL || queued == NULL) {
		warnx("Out of memory");
		goto error;
	}

	// Build the dependency graph
	for (int i = 0; i < package_count; i++) {
		mportPackageMeta **downdeps = NULL;
		if (mport_pkgmeta_get_downdepends(mport, flat_packs[i], &downdeps) != MPORT_OK) {
			warnx("Error getting dependencies for %s: %s", flat_packs[i]->name,
			    mport_err_string());
			goto error;
		}

		if (downdeps != NULL) {
			for (mportPackageMeta **d = downdeps; *d != NULL; d++) {
				for (int j = 0; j < package_count; j++) {
					if (i == j)
						continue;
					if (strcmp((*d)->name, flat_packs[j]->name) == 0) {
						int from = reverse_edges ? j : i;
						int to = reverse_edges ? i : j;

						bool duplicate = false;
						const struct edge *curr = adj[from];
						while (curr != NULL) {
							if (curr->to == to) {
								duplicate = true;
								break;
							}
							curr = curr->next;
						}

						if (!duplicate) {
							struct edge *new_edge =
							    malloc(sizeof(struct edge));
							if (new_edge == NULL) {
								warnx("Out of memory");
								mport_pkgmeta_vec_free(downdeps);
								goto error;
							}
							new_edge->to = to;
							new_edge->next = adj[from];
							adj[from] = new_edge;
							in_degree[to]++;
						}
						break;
					}
				}
			}
			mport_pkgmeta_vec_free(downdeps);
		}
	}

	int sorted_count = 0;
	while (sorted_count < package_count) {
		int i;
		for (i = 0; i < package_count; i++) {
			if (!queued[i] && in_degree[i] == 0) {
				break;
			}
		}

		if (i == package_count) {
			warnx(
			    "Dependency cycle detected among packages. Ordering may be sub-optimal.");
			for (i = 0; i < package_count; i++) {
				if (!queued[i])
					break;
			}
		}

		queued[i] = true;
		sorted_packs[sorted_count++] = flat_packs[i];

		const struct edge *curr = adj[i];
		while (curr != NULL) {
			in_degree[curr->to]--;
			curr = curr->next;
		}
	}

	for (int i = 0; i < package_count; i++) {
		struct edge *curr = adj[i];
		while (curr != NULL) {
			struct edge *next = curr->next;
			free(curr);
			curr = next;
		}
	}
	free(adj);
	free(in_degree);
	free(queued);
	return sorted_packs;

error:
	if (adj != NULL) {
		for (int i = 0; i < package_count; i++) {
			struct edge *curr = adj[i];
			while (curr != NULL) {
				struct edge *next = curr->next;
				free(curr);
				curr = next;
			}
		}
		free(adj);
	}
	free(sorted_packs);
	free(in_degree);
	free(queued);
	return NULL;
}

static int
updateMany(mportInstance *mport, int argc, char **argv)
{
	mportPackageMeta **packs = NULL;
	int package_count = 0;
	int resultCode = MPORT_OK;

	mportPackageMeta ***results = calloc((size_t)argc, sizeof(mportPackageMeta **));
	if (results == NULL) {
		warnx("Out of memory");
		return MPORT_ERR_FATAL;
	}

	if (argc > 1 && strchr(argv[1], '*') != NULL) {
		char *pkg = mport_string_replace(argv[1], "*", "%");
		if (mport_pkgmeta_search_master(mport, &results[0], "pkg like %Q", pkg) !=
		    MPORT_OK) {
			warnx("%s", mport_err_string());
			free(pkg);
			free(results);
			return (MPORT_ERR_FATAL);
		}
		free(pkg);
		if (results[0] == NULL) {
			warnx("No packages installed matching '%s'", argv[1]);
			free(results);
			return (MPORT_ERR_FATAL);
		}
		packs = results[0];
		while (*packs != NULL) {
			package_count++;
			packs++;
		}
	} else {
		for (int i = 1; i < argc; i++) {
			results[i - 1] = lookup_package(mport, argv[i]);
			if (results[i - 1] != NULL) {
				packs = results[i - 1];
				while (*packs != NULL) {
					package_count++;
					packs++;
				}
			} else {
				warnx("Package %s is not installed. Skipping update.", argv[i]);
			}
		}
	}

	if (package_count == 0) {
		for (int i = 0; i < argc; i++)
			if (results[i] != NULL)
				mport_pkgmeta_vec_free(results[i]);
		free(results);
		return MPORT_OK;
	}

	mportPackageMeta **flat_packs = calloc((size_t)package_count, sizeof(mportPackageMeta *));
	if (flat_packs == NULL) {
		warnx("Out of memory");
		resultCode = MPORT_ERR_FATAL;
		goto cleanup;
	}

	int flat_idx = 0;
	for (int i = 0; i < argc; i++) {
		if (results[i] == NULL)
			continue;
		packs = results[i];
		while (*packs != NULL) {
			if (flat_idx < package_count) {
				flat_packs[flat_idx++] = *packs;
			}
			packs++;
		}
	}

	if (flat_idx != package_count) {
		warnx("Warning: package count mismatch during update (%d != %d)", flat_idx,
		    package_count);
		package_count = flat_idx;
	}

	mportPackageMeta **sorted_packs =
	    sort_dependencies_topological(mport, flat_packs, package_count, true);
	if (sorted_packs == NULL) {
		resultCode = MPORT_ERR_FATAL;
		free(flat_packs);
		goto cleanup;
	}

	for (int i = 0; i < package_count; i++) {
		int tempResultCode = mport_update(mport, sorted_packs[i]->name);
		if (tempResultCode != MPORT_OK) {
			resultCode = tempResultCode;
			mport_call_msg_cb(mport, "Error updating package %s: %s",
			    sorted_packs[i]->name, mport_err_string());
		}
	}

	free(flat_packs);
	free(sorted_packs);

cleanup:
	for (int i = 0; i < argc; i++) {
		if (results[i] != NULL)
			mport_pkgmeta_vec_free(results[i]);
	}

	free(results);
	return (resultCode);
}
int
main(int argc, char *argv[])
{
	char *flag = NULL, *buf = NULL;
	mportInstance *mport;
	int resultCode = MPORT_ERR_FATAL;
	int tempResultCode;
	int i;
	char **searchQuery;
	signed char ch;
	const char *chroot_path = NULL;
	const char *outputPath = NULL;
	int version = 0;
	int noIndex = 0;
	bool quiet = false;
	bool verbose = false;
	bool force = false;
	bool brief = false;

	struct option longopts[] = {
		{ "no-index", no_argument, NULL, 'U' },
		{ "verbose", no_argument, NULL, 'V' },
		{ "brief", no_argument, NULL, 'b' },
		{ "chroot", required_argument, NULL, 'c' },
		{ "force", no_argument, NULL, 'f' },
		{ "output", required_argument, NULL, 'o' },
		{ "quiet", no_argument, NULL, 'q' },
		{ "version", no_argument, NULL, 'v' },
		{ NULL, 0, NULL, 0 },
	};

	if (argc < 2)
		usage();

	if (setenv("POSIXLY_CORRECT", "1", 1) == -1)
		err(EXIT_FAILURE, "setenv() failed");

	setlocale(LC_ALL, "");

	while ((ch = getopt_long(argc, argv, "+c:o:bfqUVv", longopts, NULL)) != -1) {
		switch (ch) {
		case 'U':
			noIndex++;
			break;
		case 'V':
			verbose = true;
			break;
		case 'b':
			brief = true;
			break;
		case 'c':
			chroot_path = optarg;
			break;
		case 'f':
			force = true;
			break;
		case 'o':
			outputPath = optarg;
			break;
		case 'q':
			quiet = true;
			break;
		case 'v':
			version++;
			break;
		default:
			errx(EXIT_FAILURE, "Invalid argument provided");
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (chroot_path != NULL) {
		if (chroot(chroot_path) == -1) {
			err(EXIT_FAILURE, "chroot failed");
		}
		if (chdir("/") == -1) {
			err(EXIT_FAILURE, "chdir failed");
		}
	}

	mport = mport_instance_new();

	if (mport_instance_init(mport, NULL, outputPath, noIndex != 0,
		mport_verbosity(quiet, verbose, brief)) != MPORT_OK) {
		errx(1, "%s", mport_err_string());
	}
	mport->force = force;

	if (version == 1) {
		show_version(mport, version);
		mport_instance_free(mport);
		exit(EXIT_SUCCESS);
	}

	const char *cmd = argv[0];

	if (cmd == NULL)
		usage();

	if (!strcmp(cmd, "add")) {
		if (argc == 1) {
			mport_instance_free(mport);
			usage();
		}

		int local_argc = argc;
		char *const *local_argv = argv;
		int aflag = 0;

		if (local_argc > 1) {
			int ch2;
			while ((ch2 = getopt(local_argc, local_argv, "A")) != -1) {
				switch (ch2) {
				case 'A':
					aflag = 1;
					break;
				}
			}
			local_argc -= optind;
			local_argv += optind;
		}

		mport->noIndex = true;
		mport->offline = true;

		for (i = 0; i < local_argc; i++) {
			tempResultCode = add(
			    mport, local_argv[i], aflag == 1 ? MPORT_AUTOMATIC : MPORT_EXPLICIT);
			if (tempResultCode != 0)
				resultCode = tempResultCode;
		}
	} else if (!strcmp(cmd, "install")) {
		if (argc == 1) {
			mport_instance_free(mport);
			usage();
		}

		int local_argc = argc;
		char *const *local_argv = argv;
		int aflag = 0;
		int rflag = 0;

		if (local_argc > 1) {
			int ch2;
#if defined(__MidnightBSD__)
			optreset = 1;
#endif
			optind = 1;
			while ((ch2 = getopt(local_argc, local_argv, "AMry")) != -1) {
				switch (ch2) {
				case 'A':
					aflag = 1;
					break;
				case 'M':
					mport->ignoreMissing = true;
					break;
				case 'r':
					rflag = 1;
					break;
				case 'y':
					setenv("ASSUME_ALWAYS_YES", "1", 1);
					break;
				}
			}
			local_argc -= optind;
			local_argv += optind;
		}

		loadIndex(mport);
		resultCode = MPORT_OK;
		for (i = 0; i < local_argc; i++) {
			tempResultCode = install(
			    mport, local_argv[i], aflag == 1 ? MPORT_AUTOMATIC : MPORT_EXPLICIT);
			if (tempResultCode != 0)
				resultCode = tempResultCode;
			if (tempResultCode == 0 && rflag && mport->force) {
				tempResultCode = reinstall_dependents(mport, local_argv[i]);
				if (tempResultCode != 0)
					resultCode = tempResultCode;
			}
		}
	} else if (!strcmp(cmd, "delete")) {
		if (argc == 1) {
			mport_instance_free(mport);
			usage();
		}
		int local_argc = argc;
		char **local_argv = argv;

		if (local_argc > 1) {
			int ch2;
#if defined(__MidnightBSD__)
			optreset = 1;
#endif
			optind = 1;
			while ((ch2 = getopt(local_argc, local_argv, "y")) != -1) {
				switch (ch2) {
				case 'y':
					setenv("ASSUME_ALWAYS_YES", "1", 1);
					break;
				}
			}
			local_argc -= optind;
			local_argv += optind;
		}
		resultCode = deleteMany(mport, local_argc, local_argv, false);
	} else if (!strcmp(cmd, "update")) {
		if (argc == 1) {
			mport_instance_free(mport);
			usage();
		}
		loadIndex(mport);
		resultCode = updateMany(mport, argc, argv);
	} else if (!strcmp(cmd, "download")) {
		loadIndex(mport);
		char *path;
		int aflag = 0;
		int dflag = 0;
		int ch2;

#if defined(__MidnightBSD__)
		optreset = 1;
#endif
		optind = 1;
		while ((ch2 = getopt(argc, argv, "ad")) != -1) {
			switch (ch2) {
			case 'a':
				aflag = 1;
				break;
			case 'd':
				dflag = 1;
				break;
			}
		}
		int local_argc = argc - optind;
		char *const *local_argv = argv + optind;

		if (aflag) {
			resultCode = mport_download(mport, NULL, true, false, &path);
		} else {
			for (i = 0; i < local_argc; i++) {
				tempResultCode =
				    mport_download(mport, local_argv[i], false, dflag == 1, &path);
				if (tempResultCode != 0) {
					resultCode = tempResultCode;
				} else if (path != NULL) {
					free(path);
				}
			}
		}
	} else if (!strcmp(cmd, "upgrade")) {
		loadIndex(mport);
		resultCode = mport_upgrade(mport);
	} else if (!strcmp(cmd, "annotate")) {
		loadIndex(mport);

		int local_argc = argc;
		char *const *local_argv = argv;
		int Sflag = 0;
		int aflag = 0;
		int Aflag = 0;
		int Dflag = 0;

		if (local_argc > 1) {
			int ch2;
			while ((ch2 = getopt(local_argc, local_argv, "aqADS")) != -1) {
				switch (ch2) {
				case 'S':
					Sflag = 1;
					break;
				case 'q':
					mport->verbosity = MPORT_VQUIET;
					break;
				case 'a':
					aflag = 1;
					break;
				case 'A':
					Aflag = 1;
					break;
				case 'D':
					Dflag = 1;
					break;
				}
			}
			local_argc -= optind;
			local_argv += optind;
		}

		if (local_argc > 1 && Sflag) {
			int index = quiet == true ? 2 : 0;
			if (aflag == 0) {
				resultCode =
				    annotate_show(mport, local_argv[index], local_argv[index + 1]);
			} else {
				resultCode = annotate_list(mport, local_argv[1]);
			}
		} else if (local_argc > 1 && Dflag) {
			resultCode = annotate_delete(mport, local_argv[1], local_argv[2]);
		} else if (local_argc > 2 && Aflag) {
			resultCode =
			    annotate_add(mport, local_argv[1], local_argv[2], local_argv[3]);
		} else {
			mport_instance_free(mport);
			usage();
		}
	} else if (!strcmp(cmd, "audit")) {
		loadIndex(mport);
		int rflag = 0;
		int ch2;

#if defined(__MidnightBSD__)
		optreset = 1;
#endif
		optind = 1;
		while ((ch2 = getopt(argc, argv, "r")) != -1) {
			switch (ch2) {
			case 'r':
				rflag = 1;
				break;
			}
		}
		int local_argc = argc - optind;
		char *const *local_argv = argv + optind;

		if (local_argc > 0) {
			resultCode = audit_package(mport, local_argv[0], rflag > 0);
		} else {
			resultCode = audit(mport, rflag > 0);
		}
	} else if (!strcmp(cmd, "locks")) {
		asprintf(&buf, "%s%s", MPORT_TOOLS_PATH, "mport.list");
		flag = strdup("-l");
		resultCode = execl(buf, "mport.list", flag, (char *)0);
		free(flag);
		free(buf);
	} else if (!strcmp(cmd, "import")) {
		loadIndex(mport);
		resultCode = mport_import(mport, argv[2]);
	} else if (!strcmp(cmd, "export")) {
		resultCode = mport_export(mport, argv[2]);
	} else if (!strcmp(cmd, "lock")) {
		if (argc > 1) {
			lock(mport, argv[1]);
		} else {
			usage();
		}
	} else if (!strcmp(cmd, "unlock")) {
		if (argc > 1) {
			unlock(mport, argv[1]);
		} else {
			usage();
		}
	} else if (!strcmp(cmd, "list")) {
		mportListPrint opts;
		opts.verbose = false;
		opts.locks = false;
		opts.prime = false;
		opts.update = false;
		opts.origin = false;

		if (argc > 1) {
			if (!strcmp(argv[1], "updates") || !strcmp(argv[1], "up")) {
				opts.update = true;
				loadIndex(mport);
			} else if (!strcmp(argv[1], "prime")) {
				opts.prime = true;
			} else {
				mport_instance_free(mport);
				usage();
			}
		} else {
			opts.verbose = true;
		}
		resultCode = mport_list_print(mport, &opts);
	} else if (!strcmp(cmd, "info")) {
		loadIndex(mport);
		resultCode = info(mport, argv[1]);
	} else if (!strcmp(cmd, "index")) {
		resultCode = mport_index_get(mport);
		if (resultCode != MPORT_OK) {
			fprintf(stderr, "Unable to fetch index: %s\n", mport_err_string());
		}
	} else if (!strcmp(cmd, "search")) {
		loadIndex(mport);
		searchQuery = calloc((size_t)argc, sizeof(char *));
		if (searchQuery == NULL) {
			fprintf(stderr, "calloc failed\n");
			exit(EXIT_FAILURE);
		}
		for (i = 1; i < argc; i++) {
			searchQuery[i - 1] = strdup(argv[i]);
			if (searchQuery[i - 1] == NULL) {
				fprintf(stderr, "strdup failed\n");
				exit(EXIT_FAILURE);
			}
		}
		resultCode = search(mport, searchQuery);
		for (i = 1; i < argc; i++) {
			free(searchQuery[i - 1]);
		}
		free(searchQuery);
	} else if (!strcmp(cmd, "shell")) {
		asprintf(&buf, "%s/%s", "/usr/bin", "sqlite3");
		flag = strdup("/var/db/mport/master.db");
		resultCode = execl(buf, "sqlite3", flag, (char *)0);
		free(flag);
		free(buf);
	} else if (!strcmp(cmd, "stats")) {
		loadIndex(mport);
		resultCode = stats(mport);
	} else if (!strcmp(cmd, "clean")) {
		loadIndex(mport);
		resultCode = clean(mport);
	} else if (!strcmp(cmd, "config")) {
		if (argc < 2) {
			mport_instance_free(mport);
			usage();
		}

		if (!strcmp(argv[1], "list")) {
			char **result = mport_setting_list(mport);
			char **ptr = result;
			if (result != NULL) {
				int c = 0;
				while (*result != NULL) {
					printf("%s\n", *result);
					result++;
					c++;
				}
				for (int j = 0; j < c; j++)
					free(ptr[j]);
				free(ptr);
			}
			resultCode = MPORT_OK;
		} else if (!strcmp(argv[1], "get")) {
			resultCode = configGet(mport, argv[2]);
		} else if (!strcmp(argv[1], "set")) {
			resultCode = configSet(mport, argv[2], argv[3]);
		}
	} else if (!strcmp(cmd, "mirror")) {
		if (argc < 2) {
			mport_instance_free(mport);
			usage();
		}
		if (!strcmp(argv[1], "list")) {
			loadIndex(mport);
			printf("To set a mirror, use the following command:\n");
			printf("mport set config mirror_region <country>\n\n");
			resultCode = mport_index_print_mirror_list(mport);
		} else if (!strcmp(argv[1], "select")) {
			loadIndex(mport);
			resultCode = selectMirror(mport);
		}
	} else if (!strcmp(cmd, "cpe")) {
		if (argc == 1) {
			resultCode = cpeList(mport);
		} else {
			for (i = 1; i < argc; i++) {
				tempResultCode = cpeGet(mport, argv[i]);
				if (tempResultCode != 0)
					resultCode = tempResultCode;
			}
		}
	} else if (!strcmp(cmd, "purl")) {
		if (argc == 1) {
			resultCode = purlList(mport);
		} else {
			for (i = 1; i < argc; i++) {
				tempResultCode = purlGet(mport, argv[i]);
				if (tempResultCode != 0)
					resultCode = tempResultCode;
			}
		}
	} else if (!strcmp(cmd, "deleteall")) {
		resultCode = deleteAll(mport);
	} else if (!strcmp(cmd, "autoremove")) {
		resultCode = mport_autoremove(mport);
	} else if (!strcmp(cmd, "verify")) {
		int local_argc = argc;
		char *const *local_argv = argv;
		int rflag = 0;
		int dflag = 0;

		if (local_argc > 1) {
			int ch2;
			optind = 1;
			while ((ch2 = getopt(local_argc, local_argv, "dr")) != -1) {
				switch (ch2) {
				case 'd':
					dflag = 1;
					break;
				case 'r':
					rflag = 1;
					break;
				}
			}
			local_argc -= optind;
			local_argv += optind;
		}

		if (dflag) {
			int nmissing = mport_check_missing_depends(mport);
			if (nmissing < 0) {
				warnx("%s", mport_err_string());
				resultCode = mport_err_code();
			} else if (nmissing == 0) {
				printf("All dependencies are satisfied.\n");
			} else {
				printf("%d missing dependenc%s found.\n", nmissing,
				    nmissing == 1 ? "y" : "ies");
				resultCode = MPORT_ERR_WARN;
			}
		}

		if (rflag) {
			for (int x = 0; x < local_argc; x++) {
				mportPackageMeta **packs = lookup_package(mport, local_argv[x]);
				if (packs == NULL) {
					continue;
				}
				tempResultCode = mport_recompute_checksums(mport, *packs);
				if (tempResultCode != 0)
					resultCode = tempResultCode;
				mport_pkgmeta_vec_free(packs);
			}
		}

		if (!dflag && !rflag) {
			if (argc > 1) {
				resultCode = verify_many(mport, argc, argv, true);
			} else {
				resultCode = verify(mport);
			}
		}
	} else if (!strcmp(cmd, "version")) {
		int local_argc = argc;
		char *const *local_argv = argv;
		if (local_argc > 1) {
			int ch2, tflag;
			tflag = 0;
			while ((ch2 = getopt(local_argc, local_argv, "t")) != -1) {
				switch (ch2) {
				case 't':
					tflag = 1;
					break;
				}
			}
			local_argc -= optind;
			local_argv += optind;

			if (tflag) {
				if (local_argv[0] == NULL) {
					fprintf(stderr, "Usage: mport version -t <v1> <v2>\n");
					return -2;
				}
				if (local_argv[1] == NULL) {
					fprintf(stderr, "Usage: mport version -t <v1> <v2>\n");
					return -2;
				}
				resultCode = mport_version_cmp(local_argv[0], local_argv[1]);
				printf("%c\n",
				    resultCode == 0	 ? '=' :
					resultCode == -1 ? '<' :
							   '>');
			}
		}
	} else if (!strcmp(cmd, "which")) {
		int local_argc = argc;
		char *const *local_argv = argv;
		if (local_argc > 1) {
			int ch2, oflag;
			oflag = 0;
			while ((ch2 = getopt(local_argc, local_argv, "qo")) != -1) {
				switch (ch2) {
				case 'q':
					mport->verbosity = MPORT_VQUIET;
					break;
				case 'o':
					oflag = 1;
					break;
				}
			}
			local_argc -= optind;
			local_argv += optind;
			resultCode = which(mport, *local_argv, oflag);
		} else {
			usage();
		}
	} else {
		mport_instance_free(mport);
		usage();
	}

	mport_instance_free(mport);
	exit(resultCode);
}

static void
usage(void)
{
	show_version(NULL, 2);

	fprintf(stderr,
	    "usage: mport [-c chroot dir] [-o output] [-b|q|V] [-fUV] <command> [args...]\n\n"
	    "Global options:\n"
	    "  -c <dir>    Set chroot directory\n"
	    "  -o <file>   Set output file\n"
	    "  -f          Force operation\n"
	    "  -q          Quiet mode\n"
	    "  -b          Brief output\n\n"
	    "  -V          Verbose mode\n"
	    "  -U          No index update\n"
	    "  -v          Show version\n"
	    "Commands:\n"
	    "  Package Management:\n"
	    "    add [-A] <package file>     Install package from file\n"
	    "    install [-AMry] <package>     Install package from repository\n"
	    "    delete <package>            Remove installed package\n"
	    "    update [package]            Update installed package(s)\n"
	    "    upgrade                     Upgrade all outdated packages\n"
	    "    autoremove                  Remove automatically installed packages\n"
	    "    clean                       Clean package cache\n"
	    "    verify [-d] [-r] [package]       Verify installed packages\n"
	    "      -d                            Check for missing dependencies\n"
	    "    deleteall                   Remove all installed packages\n\n"
	    "  Information:\n"
	    "    search <query>              Search for packages\n"
	    "    info <package>              Display package information\n"
	    "    list [updates|prime]        List installed packages\n"
	    "    which [-qo] <file>          Find which package provides a file\n"
	    "    stats                       Show package statistics\n\n"
	    "  Index and Repository:\n"
	    "    index                       Update package index\n"
	    "    mirror list                 List available mirrors\n"
	    "    mirror select               Select fastest mirror\n"
	    "    download [-ad] <package>    Download package without installing\n\n"
	    "  Configuration:\n"
	    "    config list                 List all settings\n"
	    "    config get <setting>        Get value of a setting\n"
	    "    config set <setting> <val>  Set value of a setting\n\n"
	    "  Security:\n"
	    "    audit [package]             Check for security vulnerabilities\n"
	    "    lock <package>              Lock package against modifications\n"
	    "    unlock <package>            Unlock package\n"
	    "    locks                       List locked packages\n"
	    "    cpe [package]               List Common Platform Enumeration info\n"
	    "    purl [package]              List Package URL info\n"
	    "  Miscellaneous:\n"
	    "    annotate [-aqADS]           Manage annotations\n"
	    "    import <file>               Import package list\n"
	    "    export <file>               Export package list\n"
	    "    shell                       Open SQLite shell for package database\n"
	    "    version -t <v1> <v2>        Compare two version strings\n");
	exit(EXIT_FAILURE);
}

static void
show_version(/*@null@*/ mportInstance *mport, int count)
{
	char *version = NULL;
	if (count == 1)
		version = mport_version_short(mport);
	else
		version = mport_version(mport);
	fprintf(stderr, "%s", version);
	if (mport == NULL)
		fprintf(stderr, "(Host OS version, not configured)\n\n");
	free(version);
}

static int
loadIndex(/*@notnull@*/ mportInstance *mport)
{
	int result = mport_index_load(mport);
	if (result == MPORT_ERR_WARN)
		warnx("%s", mport_err_string());
	else if (result != MPORT_OK)
		errx(4, "Unable to load index %s", mport_err_string());
	return result;
}

static mportIndexEntry **
lookupIndex(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *packageName)
{
	mportIndexEntry **indexEntries = NULL;

	if (mport_index_lookup_pkgname(mport, packageName, &indexEntries) != MPORT_OK) {
		fprintf(stderr, "Error looking up package name %s: %d %s\n", packageName,
		    mport_err_code(), mport_err_string());
		errx(mport_err_code(), "%s", mport_err_string());
	}

	return (indexEntries);
}

static int
selectMirror(/*@notnull@*/ mportInstance *mport)
{
	mportMirrorEntry **mirrorEntry = NULL;
	mportMirrorEntry **mirrorEntry_orig = NULL;
	char hostname[256];

	mport_index_mirror_list(mport, &mirrorEntry);
	mirrorEntry_orig = mirrorEntry;

	long fastest = 1000;
	const char *country = "us";

	while (mirrorEntry != NULL && *mirrorEntry != NULL) {
		char *p = strchr((*mirrorEntry)->url, '/');
		if (p != NULL) {
			*p = '\0';
			p++;
			p++;
		}
		char *end = strchr(p, '/');
		if (end != NULL) {
			*end = '\0';
		}
		strlcpy(hostname, p, sizeof(hostname));
		mport_call_msg_cb(mport, "Trying mirror %s %s", (*mirrorEntry)->country, hostname);
		long rtt = ping(hostname);

		if (rtt != -1 && rtt < fastest) {
			fastest = rtt;
			country = (*mirrorEntry)->country;
		}

		mirrorEntry++;
	}

	mport_call_msg_cb(mport, "Using mirror %s with rtt %ld ms\n", country, fastest);
	int result = mport_setting_set(mport, MPORT_SETTING_MIRROR_REGION, country);

	if (result != MPORT_OK) {
		warnx("%s", mport_err_string());
		mport_index_mirror_entry_free_vec(mirrorEntry_orig);
		return mport_err_code();
	}

	mport_index_mirror_entry_free_vec(mirrorEntry_orig);

	return MPORT_OK;
}

static int
search(/*@notnull@*/ mportInstance *mport, /*@null@*/ char **query)
{
	mportIndexEntry **indexEntry = NULL;

	if (query == NULL || *query == NULL) {
		fprintf(stderr, "Search terms required\n");
		return (1);
	}

	while (query != NULL && *query != NULL) {
		/*
		 * On failure mport_index_search_term() may leave a partially
		 * built vector (a non-NULL entry with NULL fields), so only
		 * print when it succeeds -- but still free whatever it
		 * allocated.
		 */
		if (mport_index_search_term(mport, &indexEntry, *query) == MPORT_OK &&
		    indexEntry != NULL) {
			for (mportIndexEntry **e = indexEntry; *e != NULL; e++) {
				fprintf(stdout, "%s\t%s\t%s\n", (*e)->pkgname, (*e)->version,
				    (*e)->comment);
			}
		}
		if (indexEntry != NULL) {
			mport_index_entry_free_vec(indexEntry);
			indexEntry = NULL;
		}
		query++;
	}

	return (0);
}

static int
lock(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *packageName)
{
	mportPackageMeta **packs = lookup_package(mport, packageName);

	if (packs != NULL) {
		mport_lock_lock(mport, (*packs));
		mport_pkgmeta_vec_free(packs);
		return (MPORT_OK);
	}

	return (MPORT_ERR_FATAL);
}

static int
unlock(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *packageName)
{
	mportPackageMeta **packs = lookup_package(mport, packageName);

	if (packs != NULL) {
		mport_lock_unlock(mport, (*packs));
		mport_pkgmeta_vec_free(packs);
		return (MPORT_OK);
	}

	return (MPORT_ERR_FATAL);
}

static int
stats(/*@notnull@*/ mportInstance *mport)
{
	char flatsize_str[8];
	mportStats *s = NULL;
	if (mport_stats(mport, &s) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return (1);
	}

	humanize_number(flatsize_str, sizeof(flatsize_str), s->pkg_installed_size, "B",
	    HN_AUTOSCALE, HN_DECIMAL | HN_IEC_PREFIXES);

	printf("Local package database:\n");
	printf("\tInstalled packages: %u\n", s->pkg_installed);
	printf("\tDisk space occupied: %s\n", flatsize_str);
	printf("\nRemote package database:\n");
	printf("\tPackages available: %u\n", s->pkg_available);

	free(s);

	return (0);
}

static int
info(/*@notnull@*/ mportInstance *mport, /*@null@*/ const char *packageName)
{
	if (packageName == NULL) {
		warnx("%s", "Specify package name");
		return (1);
	}

	char *out = mport_info(mport, packageName);
	if (out == NULL) {
		warnx("%s", mport_err_string());
		return (1);
	}

	printf("%s", out);
	free(out);

	return (0);
}

static int
which(/*@notnull@*/ mportInstance *mport, /*@null@*/ const char *filePath, bool origin)
{
	mportPackageMeta *pack = NULL;

	if (filePath == NULL) {
		warnx("%s", "Specify file path");
		return (1);
	}

	if (mport_asset_get_package_from_file_path(mport, filePath, &pack) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return (1);
	}

	mport_drop_privileges();

	if (pack != NULL && pack->origin != NULL) {
		if (mport->verbosity == MPORT_VQUIET && origin) {
			printf("%s\n", pack->origin);
		} else if (mport->verbosity == MPORT_VQUIET) {
			printf("%s-%s\n", pack->name, pack->version);
		} else if (origin) {
			printf("%s was installed by package %s\n", filePath, pack->origin);
		} else {
			printf("%s was installed by package %s-%s\n", filePath, pack->name,
			    pack->version);
		}
	}

	return (0);
}

static int
add(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *filename,
    mportAutomatic automatic)
{
	return mport_install_primative(mport, filename, NULL, automatic);
}

static int
install(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *packageName,
    mportAutomatic automatic)
{
	mportIndexEntry **indexEntry = NULL;
	mportIndexEntry **ie = NULL;
	mportIndexEntry **i2 = NULL;
	int resultCode = MPORT_OK;
	int item;
	int choice;

	indexEntry = lookupIndex(mport, packageName);
	if (indexEntry == NULL || *indexEntry == NULL) {
		int loc = -1;
		size_t len = strlen(packageName);
		for (int i = len - 1; i >= 0; i--) {
			if (packageName[i] == '-') {
				loc = i;
				break;
			}
		}

		if (loc > 0) {
			char *d = strdup(packageName);
			if (d == NULL) {
				fprintf(stderr, "strdup failed\n");
				exit(EXIT_FAILURE);
			}
			const char *v = &d[loc + 1];
			d[loc] = '\0'; /* hack off the version number */
			mport_index_entry_free_vec(
			    indexEntry); /* free the empty result from the full-name lookup */
			indexEntry = lookupIndex(mport, d);
			if (indexEntry == NULL || (*indexEntry) == NULL ||
			    strcmp(v, (*indexEntry)->version) != 0) {
				fprintf(
				    stderr, "Package %s not found in the index.\n", packageName);
				exit(4);
			}
			free(d);
		}
	}

	if (indexEntry == NULL || *indexEntry == NULL) {
		fprintf(stderr, "Package %s not found in the index.\n", packageName);
		exit(4);
	}

	ie = indexEntry;
	if (indexEntry[1] != NULL) {
		printf("Multiple packages found. Please select one:\n");
		i2 = indexEntry;
		item = 0;
		while (*i2 != NULL) {
			printf("%d. %s-%s\n", item, (*i2)->pkgname, (*i2)->version);
			item++;
			i2++;
		}
		while (scanf("%d", &choice) < 1 || choice > item || choice < 0) {
			fprintf(stderr, "Please select an entry 0 - %d\n", item - 1);
		}
		item = 0;
		while (*indexEntry != NULL) {
			if (item == choice)
				break;
			item++;
			indexEntry++;
		}
	}

	if (indexEntry != NULL && *indexEntry != NULL) {
		resultCode = mport_install(
		    mport, (*indexEntry)->pkgname, (*indexEntry)->version, NULL, automatic);
	}

	mport_index_entry_free_vec(ie);

	return (resultCode);
}

/*
 * Inner recursive worker for reinstall_dependents.
 * visited/visit_count/visit_max guard against reinstalling the same package
 * twice when it appears as a dependent via multiple paths in the tree.
 * The visited array grows dynamically; visit_count and visit_cap are updated
 * in place so callers can free every entry on return.
 */
static int
reinstall_dependents_impl(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *baseName,
    /*@notnull@*/ /*@out@*/ char ***visited_p, /*@notnull@*/ /*@out@*/ size_t *visit_count,
    /*@notnull@*/ /*@out@*/ size_t *visit_cap)
{
	mportPackageMeta **packs = NULL;
	mportPackageMeta **updepends = NULL;
	int resultCode = MPORT_OK;

	if (mport_pkgmeta_search_master(mport, &packs, "LOWER(pkg)=LOWER(%Q)", baseName) !=
		MPORT_OK ||
	    packs == NULL || packs[0] == NULL) {
		mport_pkgmeta_vec_free(packs);
		return MPORT_OK;
	}

	if (mport_pkgmeta_get_updepends(mport, packs[0], &updepends) != MPORT_OK) {
		mport_pkgmeta_vec_free(packs);
		warnx("%s", mport_err_string());
		return MPORT_ERR_FATAL;
	}
	mport_pkgmeta_vec_free(packs);

	if (updepends == NULL)
		return MPORT_OK;

	for (mportPackageMeta **dep = updepends; *dep != NULL; dep++) {
		bool already_visited = false;
		for (size_t v = 0; v < *visit_count; v++) {
			if (strcmp((*visited_p)[v], (*dep)->name) == 0) {
				already_visited = true;
				break;
			}
		}
		if (already_visited)
			continue;

		/* grow visited array if needed */
		if (*visit_count >= *visit_cap) {
			size_t new_cap = *visit_cap * 2;
			char **grown = reallocarray(*visited_p, new_cap, sizeof(char *));
			if (grown == NULL) {
				warnx("reinstall_dependents: out of memory growing visited set");
				resultCode = MPORT_ERR_FATAL;
				break;
			}
			*visited_p = grown;
			*visit_cap = new_cap;
		}

		char *n = strdup((*dep)->name);
		if (n != NULL)
			(*visited_p)[(*visit_count)++] = n;

		if (install(mport, (*dep)->name, (*dep)->automatic) == MPORT_OK) {
			if (reinstall_dependents_impl(
				mport, (*dep)->name, visited_p, visit_count, visit_cap) != MPORT_OK)
				resultCode = MPORT_ERR_FATAL;
		} else {
			resultCode = MPORT_ERR_FATAL;
		}
	}

	mport_pkgmeta_vec_free(updepends);
	return resultCode;
}

/*
 * Recursively reinstall all installed packages that depend on packageName.
 * Resolves versioned inputs (e.g. "openssl-3.1.0") to the base package name
 * via the index before walking the reverse-dependency tree.
 */
static int
reinstall_dependents(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *packageName)
{
	mportIndexEntry **ie = NULL;
	const char *baseName = packageName;
	char *stripped = NULL;

	/* resolve actual pkgname from index to handle "name-version" inputs */
	if (mport_index_lookup_pkgname(mport, packageName, &ie) == MPORT_OK && ie != NULL &&
	    *ie != NULL) {
		baseName = (*ie)->pkgname;
	} else {
		mport_index_entry_free_vec(ie);
		ie = NULL;
		const char *dash = strrchr(packageName, '-');
		if (dash != NULL && dash != packageName) {
			stripped = strndup(packageName, (size_t)(dash - packageName));
			if (stripped != NULL &&
			    mport_index_lookup_pkgname(mport, stripped, &ie) == MPORT_OK &&
			    ie != NULL && *ie != NULL) {
				baseName = (*ie)->pkgname;
			}
		}
	}

	size_t visit_cap = 32;
	size_t visit_count = 0;
	char **visited = calloc(visit_cap, sizeof(char *));
	int ret = MPORT_ERR_FATAL;

	if (visited != NULL)
		ret =
		    reinstall_dependents_impl(mport, baseName, &visited, &visit_count, &visit_cap);

	for (size_t i = 0; i < visit_count; i++)
		free(visited[i]);
	free(visited);
	mport_index_entry_free_vec(ie);
	free(stripped);

	return ret;
}

static int
deleteMany(/*@notnull@*/ mportInstance *mport, int argc, /*@notnull@*/ char *argv[], bool skipFirst)
{
	mportPackageMeta **packs = NULL;
	int start = skipFirst ? 1 : 0;
	size_t count = (size_t)(argc - start);
	int package_count = 0;
	int missing = 0;
	int locked = 0;
	long long total_flatsize = 0;
	char flatsize_str[8];
	int resultCode = MPORT_OK;

	mportPackageMeta ***results = calloc(count, sizeof(mportPackageMeta **));
	if (results == NULL) {
		warnx("Out of memory");
		return MPORT_ERR_FATAL;
	}

	printf("Installed packages to be REMOVED:\n\n");

	for (size_t i = 0; i < count; i++) {
		mportPackageMeta **packs_orig = NULL;
		if (mport_pkgmeta_search_master(
			mport, &packs_orig, "LOWER(pkg)=LOWER(%Q)", argv[start + i]) != MPORT_OK) {
			warnx("%s", mport_err_string());
			missing++;
			continue;
		}

		if (packs_orig == NULL) {
			warnx("No packages installed matching '%s'", argv[start + i]);
			missing++;
			continue;
		}

		results[i] = packs_orig;
		packs = packs_orig;
		while (*packs != NULL) {
			printf("\t%s: %s", (*packs)->name, (*packs)->version);
			if ((*packs)->flavor != NULL) {
				printf(",%s", (*packs)->flavor);
			}
			printf("\n");

			if (mport_lock_islocked((*packs)) == MPORT_LOCKED) {
				locked++;
			}

			package_count++;
			total_flatsize += (*packs)->flatsize;
			packs++;
		}
		printf("\n");
	}

	if (package_count == 0 || locked > 0 || missing > 0) {
		printf("%zu packages requested for removal: %d locked, %d missing\n", count, locked,
		    missing);
	}

	if (package_count == 0) {
		for (size_t i = 0; i < count; i++)
			if (results[i] != NULL)
				mport_pkgmeta_vec_free(results[i]);
		free(results);
		return (MPORT_ERR_WARN);
	}

	humanize_number(flatsize_str, sizeof(flatsize_str), total_flatsize, "B", HN_AUTOSCALE,
	    HN_DECIMAL | HN_IEC_PREFIXES);

	printf("Packages to be deleted: %d\n", package_count);
	printf("Total disk space to be freed: %s\n", flatsize_str);

	if ((mport->confirm_cb)(
		"Proceed with deinstalling packages?", "Delete", "Don't delete", 0) != MPORT_OK) {
		for (size_t i = 0; i < count; i++)
			if (results[i] != NULL)
				mport_pkgmeta_vec_free(results[i]);
		free(results);
		return (MPORT_ERR_WARN);
	}

	mportPackageMeta **flat_packs = calloc((size_t)package_count, sizeof(mportPackageMeta *));
	if (flat_packs == NULL) {
		warnx("Out of memory");
		resultCode = MPORT_ERR_FATAL;
		goto cleanup;
	}

	int flat_idx = 0;
	for (size_t i = 0; i < count; i++) {
		if (results[i] == NULL)
			continue;
		packs = results[i];
		while (*packs != NULL) {
			if (flat_idx < package_count) {
				flat_packs[flat_idx++] = *packs;
			}
			packs++;
		}
	}

	if (flat_idx != package_count) {
		warnx("Warning: package count mismatch during deletion (%d != %d)", flat_idx,
		    package_count);
		package_count = flat_idx;
	}

	mportPackageMeta **sorted_packs =
	    sort_dependencies_topological(mport, flat_packs, package_count, false);
	if (sorted_packs == NULL) {
		resultCode = MPORT_ERR_FATAL;
		free(flat_packs);
		goto cleanup;
	}

	for (int i = 0; i < package_count; i++) {
		mportPackageMeta *pack = sorted_packs[i];
		if (mport_lock_islocked(pack) == MPORT_LOCKED) {
			warnx("Package '%s' is locked. skipping", pack->name);
			continue;
		}

		pack->action = MPORT_ACTION_DELETE;
		if (mport_delete_primative(mport, pack, mport->force) != MPORT_OK) {
			warnx("%s", mport_err_string());
			resultCode = MPORT_ERR_FATAL;
		}
	}

	free(flat_packs);
	free(sorted_packs);

cleanup:
	for (size_t i = 0; i < count; i++) {
		if (results[i] != NULL)
			mport_pkgmeta_vec_free(results[i]);
	}

	free(results);
	return (resultCode);
}

static mportPackageMeta **
lookup_package(/*@notnull@*/ mportInstance *mport, /*@null@*/ const char *packageName)
{
	mportPackageMeta **packs = NULL;

	if (packageName == NULL) {
		warnx("%s", "Specify package name");
		return (NULL);
	}

	if (mport_pkgmeta_search_master(mport, &packs, "LOWER(pkg)=LOWER(%Q)", packageName) !=
	    MPORT_OK) {
		warnx("%s", mport_err_string());
		mport_pkgmeta_vec_free(packs);
		return NULL;
	}

	if (packs == NULL) {
		warnx("No packages installed matching '%s'", packageName);
		return NULL;
	}

	return packs;
}

static int delete(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *packageName)
{
	mportPackageMeta **packs = lookup_package(mport, packageName);
	if (packs == NULL) {
		return (MPORT_ERR_FATAL);
	}

	mportPackageMeta **packs_orig = packs;
	while (*packs != NULL) {
		(*packs)->action = MPORT_ACTION_DELETE;
		if (mport_delete_primative(mport, *packs, mport->force) != MPORT_OK) {
			warnx("%s", mport_err_string());
			mport_pkgmeta_vec_free(packs_orig);
			return (MPORT_ERR_FATAL);
		}
		packs++;
	}

	mport_pkgmeta_vec_free(packs_orig);

	return (MPORT_OK);
}

static int
configGet(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *settingName)
{
	char *val = NULL;

	if (settingName == NULL) {
		warnx("%s", "Specify setting name");
		return (1);
	}

	val = mport_setting_get(mport, settingName);

	mport_drop_privileges();

	if (val != NULL) {
		printf("Setting %s value is %s\n", settingName, val);
		free(val);
	} else {
		printf("Setting %s is undefined.\n", settingName);
	}

	return 0;
}

static int
configSet(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *settingName,
    /*@notnull@*/ const char *val)
{
	if (settingName == NULL || val == NULL) {
		warnx("%s", "Specify setting name and value");
		return (1);
	}

	int result = mport_setting_set(mport, settingName, val);

	mport_drop_privileges();

	if (result != MPORT_OK) {
		warnx("%s", mport_err_string());
		return mport_err_code();
	}

	return 0;
}

static int
purlGet(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *packageName)
{
	int purl_total = 0;

	mportPackageMeta **packs = lookup_package(mport, packageName);

	mport_drop_privileges();

	if (packs == NULL) {
		return (MPORT_ERR_FATAL);
	}

	mportPackageMeta **packs_orig = packs;
	while (*packs != NULL) {
		char *purl = mport_purl_uri(*packs);
		if (purl != NULL) {
			printf("%s\n", purl);
			free(purl);
		}

		purl_total++;
		packs++;
	}

	mport_pkgmeta_vec_free(packs_orig);

	if (purl_total == 0) {
		return (MPORT_ERR_WARN);
	}

	return (MPORT_OK);
}

static int
purlList(/*@notnull@*/ mportInstance *mport)
{
	mportPackageMeta **packs = NULL;
	mportPackageMeta **packs_orig = NULL;
	int purl_total = 0;

	if (mport_pkgmeta_list(mport, &packs_orig) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return mport_err_code();
	}

	mport_drop_privileges();

	if (packs_orig == NULL) {
		warnx("No packages installed.");
		return (1);
	}

	packs = packs_orig;
	while (*packs != NULL) {
		char *purl = mport_purl_uri(*packs);
		if (purl != NULL) {
			printf("%s", purl);
			free(purl);
		}

		purl_total++;
		packs++;
	}
	mport_pkgmeta_vec_free(packs_orig);

	if (purl_total == 0) {
		errx(EX_SOFTWARE, "No packages contained PURL information.");
	}

	return (0);
}

static int
cpeGet(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *packageName)
{
	int cpe_total = 0;

	mportPackageMeta **packs = lookup_package(mport, packageName);

	mport_drop_privileges();

	if (packs == NULL) {
		return (MPORT_ERR_WARN);
	}

	mportPackageMeta **packs_orig = packs;
	while (*packs != NULL) {
		if ((*packs)->cpe != NULL && (*packs)->cpe[0] != '\0') {
			printf("%s\n", (*packs)->cpe);
			cpe_total++;
		}
		packs++;
	}

	mport_pkgmeta_vec_free(packs_orig);

	if (cpe_total == 0) {
		return (MPORT_ERR_WARN);
	}

	return (MPORT_OK);
}

static int
cpeList(/*@notnull@*/ mportInstance *mport)
{
	mportPackageMeta **packs = NULL;
	mportPackageMeta **packs_orig = NULL;
	int cpe_total = 0;

	if (mport_pkgmeta_list(mport, &packs_orig) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return mport_err_code();
	}

	mport_drop_privileges();

	if (packs_orig == NULL) {
		warnx("No packages installed.");
		return (1);
	}

	packs = packs_orig;
	while (*packs != NULL) {
		if ((*packs)->cpe != NULL && (*packs)->cpe[0] != '\0') {
			printf("%s\n", (*packs)->cpe);
			cpe_total++;
		}
		packs++;
	}
	mport_pkgmeta_vec_free(packs_orig);

	if (cpe_total == 0) {
		errx(EX_SOFTWARE, "No packages contained CPE information.");
	}

	return (0);
}

static int
verify_many(
    /*@notnull@*/ mportInstance *mport, int argc, /*@notnull@*/ char *argv[], bool skipfirst)
{
	int total = 0;
	int start = skipfirst ? 1 : 0;

	if (argc < 2) {
		fprintf(stderr, "Usage: mport verify <package>\n");
		return (MPORT_ERR_WARN);
	}

	for (int i = start; i < argc; i++) {
		mportPackageMeta **pkg_matches = lookup_package(mport, argv[i]);

		if (pkg_matches == NULL) {
			continue;
		}

		for (mportPackageMeta **pkg = pkg_matches; *pkg != NULL; pkg++) {
			mport_verify_package(mport, *pkg);
			total++;
		}

		mport_pkgmeta_vec_free(pkg_matches);
	}

	printf("Packages verified: %d\n", total);

	return (MPORT_OK);
}

static int
verify(/*@notnull@*/ mportInstance *mport)
{
	mportPackageMeta **packs = NULL;
	int total = 0;

	if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return mport_err_code();
	}

	if (packs == NULL) {
		warnx("No packages installed.");
		return (MPORT_ERR_WARN);
	}

	for (mportPackageMeta **pack = packs; *pack != NULL; pack++) {
		mport_verify_package(mport, *pack);
		total++;
	}

	mport_pkgmeta_vec_free(packs);
	printf("Packages verified: %d\n", total);

	return (MPORT_OK);
}

static int
deleteAll(/*@notnull@*/ mportInstance *mport)
{
	mportPackageMeta **packs = NULL;
	mportPackageMeta **depends = NULL;
	int total = 0;
	int errors = 0;
	int skip;

	if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return (1);
	}

	if (packs == NULL) {
		fprintf(stderr, "No packages installed.\n");
		return (1);
	}

	if ((mport->confirm_cb)("Proceed with removing all packages on the system?", "Delete",
		"Don't delete", 0) != MPORT_OK) {
		mport_pkgmeta_vec_free(packs);
		return (MPORT_ERR_WARN); // User chose not to proceed
	}

	while (1) {
		skip = 0;
		for (mportPackageMeta **p = packs; *p != NULL; p++) {
			if (mport_pkgmeta_get_updepends(mport, *p, &depends) == MPORT_OK) {
				if (depends == NULL) {
					if (delete (mport, (*p)->name) != MPORT_OK) {
						fprintf(stderr, "Error deleting %s\n", (*p)->name);
						errors++;
					}
					total++;
				} else {
					skip++;
					mport_pkgmeta_vec_free(depends);
				}
			}
		}
		if (skip == 0)
			break;
		mport_pkgmeta_vec_free(packs);
		if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
			warnx("%s", mport_err_string());
			return (1);
		}
	}

	mport_pkgmeta_vec_free(packs);

	printf("Packages deleted: %d\nErrors: %d\nTotal: %d\n", total - errors, errors, total);
	return (0);
}

static int
clean(/*@notnull@*/ mportInstance *mport)
{
	int ret;
	int result = MPORT_OK;

	ret = mport_clean_database(mport);
	if (ret != MPORT_OK) {
		result = ret;
	}

	ret = mport_clean_oldpackages(mport);
	if (ret != MPORT_OK) {
		result = ret;
	}

	ret = mport_clean_oldmtree(mport);
	if (ret != MPORT_OK) {
		result = ret;
	}

	ret = mport_clean_tempfiles(mport);
	if (ret != MPORT_OK) {
		result = ret;
	}

	return (result);
}

static int
audit(/*@notnull@*/ mportInstance *mport, bool dependsOn)
{
	mportPackageMeta **packs = NULL;

	if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return (1);
	}

	if (packs == NULL) {
		fprintf(stderr, "No packages installed.\n");
		return (1);
	}

	for (mportPackageMeta **pack = packs; *pack != NULL; pack++) {
		char *output = mport_audit(mport, (*pack)->name, dependsOn);
		if (output != NULL && output[0] != '\0') {
			if (mport->verbosity == MPORT_VQUIET)
				printf("%s", output);
			else
				printf("%s\n", output);
			free(output);
		}
	}

	mport_pkgmeta_vec_free(packs);

	return (0);
}

static int
audit_package(
    /*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *packageName, bool dependsOn)
{
	mportPackageMeta **packs = lookup_package(mport, packageName);
	if (packs == NULL) {
		return (MPORT_ERR_FATAL);
	}

	mportPackageMeta **packs_orig = packs;
	while (*packs != NULL) {
		char *output = mport_audit(mport, (*packs)->name, dependsOn);
		if (output != NULL && output[0] != '\0') {
			if (mport->verbosity == MPORT_VQUIET)
				printf("%s", output);
			else
				printf("%s\n", output);
			free(output);
		}
		packs++;
	}

	mport_pkgmeta_vec_free(packs_orig);

	return (0);
}

static int
annotate_list(/*@notnull@*/ mportInstance *mport, /*@null@*/ const char *tagName)
{
	mportPackageMeta **packs = NULL;
	mportPackageMeta **packs_orig = NULL;

	if (tagName == NULL) {
		warnx("%s", "Specify tag name");
		return (1);
	}

	if (mport_pkgmeta_list(mport, &packs_orig) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return mport_err_code();
	}

	if (packs_orig == NULL) {
		warnx("No packages installed.");
		return (1);
	}

	packs = packs_orig;
	while (*packs != NULL) {
		char *annotationValue = NULL;
		int result = mport_annotation_get(mport, (*packs)->name, tagName, &annotationValue);
		if (result == MPORT_OK && annotationValue != NULL) {
			if (mport->verbosity == MPORT_VQUIET)
				printf("%s\n", annotationValue);
			else
				printf("%s-%s: Tag: %s Value: %s\n", (*packs)->name,
				    (*packs)->version, tagName, annotationValue);
			free(annotationValue);
			annotationValue = NULL;
		}
		packs++;
	}

	mport_pkgmeta_vec_free(packs_orig);

	return (0);
}

static int
annotate_show(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *packageName,
    /*@notnull@*/ const char *tagName)
{
	char *annotationValue = NULL;
	mportPackageMeta **packs = lookup_package(mport, packageName);
	if (packs == NULL) {
		return (MPORT_ERR_FATAL);
	}

	mportPackageMeta **packs_orig = packs;
	int tag_flavor = strcmp(tagName, "flavor") == 0;
	int tag_cpe = !tag_flavor && strcmp(tagName, "cpe") == 0;
	while (*packs != NULL) {
		if (tag_flavor) {
			if ((*packs)->flavor != NULL && (*packs)->flavor[0] != '\0') {
				if (mport->verbosity == MPORT_VQUIET)
					printf("%s\n", (*packs)->flavor);
				else
					printf("%s-%s: Tag: %s Value: %s\n", (*packs)->name,
					    (*packs)->version, tagName, (*packs)->flavor);
			}
		} else if (tag_cpe) {
			if ((*packs)->cpe != NULL && (*packs)->cpe[0] != '\0') {
				if (mport->verbosity == MPORT_VQUIET)
					printf("%s\n", (*packs)->cpe);
				else
					printf("%s-%s: Tag: %s Value: %s\n", (*packs)->name,
					    (*packs)->version, tagName, (*packs)->cpe);
			}
		} else {
			int result =
			    mport_annotation_get(mport, (*packs)->name, tagName, &annotationValue);
			if (result == MPORT_OK && annotationValue != NULL) {
				if (mport->verbosity == MPORT_VQUIET)
					printf("%s\n", annotationValue);
				else
					printf("%s-%s: Tag: %s Value: %s\n", (*packs)->name,
					    (*packs)->version, tagName, annotationValue);
				free(annotationValue);
				annotationValue = NULL;
			}
		}
		packs++;
	}

	mport_pkgmeta_vec_free(packs_orig);

	return (0);
}

static int
annotate_add(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *packageName,
    /*@notnull@*/ const char *tagName, /*@notnull@*/ const char *tagValue)
{
	mportPackageMeta **packs = lookup_package(mport, packageName);
	if (packs == NULL) {
		return (MPORT_ERR_FATAL);
	}

	mportPackageMeta **packs_orig = packs;
	while (*packs != NULL) {
		int result = mport_annotation_set(mport, (*packs)->name, tagName, tagValue);
		if (result != MPORT_OK) {
			warnx("%s", mport_err_string());
		}
		packs++;
	}

	mport_pkgmeta_vec_free(packs_orig);

	return (0);
}

static int
annotate_delete(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *packageName,
    /*@notnull@*/ const char *tagName)
{
	mportPackageMeta **packs = lookup_package(mport, packageName);
	if (packs == NULL) {
		return (MPORT_ERR_FATAL);
	}

	mportPackageMeta **packs_orig = packs;
	while (*packs != NULL) {
		int result = mport_annotation_delete(mport, (*packs)->name, tagName);
		if (result != MPORT_OK) {
			warnx("%s", mport_err_string());
		}
		packs++;
	}

	mport_pkgmeta_vec_free(packs_orig);

	return (0);
}
