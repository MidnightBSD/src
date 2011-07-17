/*-
 * Copyright (c) 2010, 2011 Lucas Holt
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
__MBSDID("$MidnightBSD: src/usr.sbin/mport/mport.c,v 1.31 2011/07/10 15:42:48 laffer1 Exp $");

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mport.h>

#define MPORT_TOOLS_PATH "/usr/libexec/"
#define MPORT_LOCAL_PKG_PATH "/var/db/mport/downloads"

static void usage(void);
static void loadIndex(mportInstance *);
static mportIndexEntry ** lookupIndex(mportInstance *, const char *);
static int install(mportInstance *, const char *);
static int delete(mportInstance *, const char *);
static int deleteAll(mportInstance *);
static int update(mportInstance *, const char *);
static int upgrade(mportInstance *);
static int info(mportInstance *, const char *);
static int search(mportInstance *, char **);
static int clean(mportInstance *);
static int indexCheck(mportInstance *, mportPackageMeta *);
static int updateDown(mportInstance *, mportPackageMeta *);

int 
main(int argc, char *argv[]) {
	char *flag, *buf = NULL;
	mportInstance *mport;
	int resultCode = MPORT_ERR_FATAL;
	int i;
	char **searchQuery;

	if (argc < 2)
		usage();

	mport = mport_instance_new();

	if (mport_instance_init(mport, NULL) != MPORT_OK) {
		warnx("%s", mport_err_string());
		exit(1);
	}

	loadIndex(mport);

	if (!strcmp(argv[1], "install")) {
		resultCode = install(mport, argv[2]);
	} else if (!strcmp(argv[1], "delete")) {
		for (i = 2; i < argc; i++) {
			resultCode = delete(mport, argv[i]);
		}
	} else if (!strcmp(argv[1], "update")) {
		resultCode = update(mport, argv[2]);
	} else if (!strcmp(argv[1], "upgrade")) {
		resultCode = upgrade(mport);
        } else if (!strcmp(argv[1], "list")) {
		asprintf(&buf, "%s%s", MPORT_TOOLS_PATH, "mport.list");
		if (argc > 2 && !strcmp(argv[2], "updates")) {
			flag = "-u";
		} else {
			flag = "-v";
		}
		resultCode = execl(buf, "mport.list", flag, (char *)0);
		free(buf);
	} else if (!strcmp(argv[1], "info")) {
		resultCode = info(mport, argv[2]);
	} else if (!strcmp(argv[1], "search")) {
		searchQuery = calloc(argc - 1, sizeof(char*));
		for (i = 2; i < argc; i++) {
			searchQuery[i-2] = strdup(argv[i]);
		}
		resultCode = search(mport, searchQuery);
		for (i = 2; i < argc; i++) {
			free(searchQuery[i-2]);
		}
		free(searchQuery);
	} else if (!strcmp(argv[1], "clean")) {
		resultCode = clean(mport);
	} else if (!strcmp(argv[1], "deleteall")) {
		resultCode = deleteAll(mport);
	} else {
		mport_instance_free(mport);
		usage();
	}

	mport_instance_free(mport);
	return resultCode;
}

void
usage(void) {
	fprintf( stderr, 
		"usage: mport <command> args:\n"
		"       mport clean\n"
		"       mport delete [package name]\n"
		"       mport deleteall\n"
		"       mport info [package name]\n"
		"       mport install [package name]\n"
		"       mport list [updates]\n"
		"       mport search [query ...]\n"
		"       mport update [package name]\n"
		"       mport upgrade\n"
	);
	exit(1);
}

void
loadIndex(mportInstance *mport) {
	int result = mport_index_load(mport);
	if (result == MPORT_ERR_WARN)
		warnx(mport_err_string());
	else if (result != MPORT_OK)
                errx(4, "Unable to load index %s", mport_err_string());
}

mportIndexEntry **
lookupIndex(mportInstance *mport, const char *packageName) {
	mportIndexEntry **indexEntries;

	if (mport_index_lookup_pkgname(mport, packageName, &indexEntries) != MPORT_OK) {
		fprintf(stderr, "Error looking up package name %s: %d %s\n", packageName,  mport_err_code(), mport_err_string());
		exit(mport_err_code());
	}

	return indexEntries;
}

int
search(mportInstance *mport, char **query) {
	mportIndexEntry **indexEntry;
	mportPackageMeta **packs;

	if (query == NULL || *query == NULL) {
		fprintf(stderr, "Search terms required\n");
		return 1;
	}

	while (query != NULL && *query != NULL) {
		mport_index_search(mport, &indexEntry, "pkg glob %Q or comment glob %Q", *query, *query);
		if (indexEntry == NULL || *indexEntry == NULL) {
			query++;
			continue;
		}

		while (indexEntry != NULL && *indexEntry != NULL) {
			fprintf(stdout, "%s\t%s\t%s\n", (*indexEntry)->pkgname,
					  		(*indexEntry)->version,
							(*indexEntry)->comment);
			indexEntry++;
		}

		mport_index_entry_free_vec(indexEntry);
		query++;	
	}

	return 0;
}

int
info(mportInstance *mport, const char *packageName) {
	mportIndexEntry **indexEntry;
	mportPackageMeta **packs;
	char *status, *origin;

	if (packageName == NULL) {
		fprintf(stderr, "Specify package name\n");
		return 1;
	}

	indexEntry = lookupIndex(mport, packageName);
	if (indexEntry == NULL || *indexEntry == NULL) {
		fprintf(stderr, "%s not found in index.\n", packageName);
		return 1;
	}

	if (mport_pkgmeta_search_master(mport, &packs, "pkg=%Q", packageName) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return 1;
	}

	if (packs == NULL) {
		status = "N/A";
		origin = "";
	} else {
		status = (*packs)->version;
		origin = (*packs)->origin;
	}

	printf("%s\nlatest: %s\ninstalled: %s\nlicense: %s\norigin: %s\n\n%s\n",
		(*indexEntry)->pkgname,
		(*indexEntry)->version,
		status,
		(*indexEntry)->license,
		origin,
		(*indexEntry)->comment);

	mport_index_entry_free_vec(indexEntry);
	return 0;
}

int
install(mportInstance *mport, const char *packageName) {
	char *buf, *packagePath;
	mportIndexEntry **indexEntry;
	int resultCode;

	indexEntry = lookupIndex(mport, packageName);
	if (indexEntry == NULL || *indexEntry == NULL)
		return 1;

	asprintf(&packagePath, "%s/%s", MPORT_LOCAL_PKG_PATH, (*indexEntry)->bundlefile);

	if (!mport_file_exists(packagePath)) {
		if (mport_fetch_bundle(mport, (*indexEntry)->bundlefile) != MPORT_OK) {
			fprintf(stderr, "%s\n", mport_err_string());
			free(packagePath);
			return mport_err_code();
		}
	}

	if (!mport_verify_hash(packagePath, (*indexEntry)->hash)) {
		fprintf(stderr, "Package fails hash verification.\n");
		free(packagePath);
		return 1;
        }

	asprintf(&buf, "%s%s", MPORT_TOOLS_PATH, "mport.install");

	resultCode = execl(buf, "mport.install", packagePath, (char *)0);
	free(buf);
	free(packagePath);
	mport_index_entry_free_vec(indexEntry);

	return resultCode;
}

int
delete(mportInstance *mport, const char *packageName) {
	char *buf;
	int resultCode;

	asprintf(&buf, "%s%s %s %s", MPORT_TOOLS_PATH, "mport.delete", "-n", packageName);
	resultCode = system(buf);
 	//resultCode = execl(buf, "mport.delete", "-n", packageName, (char *)0);	
	free(buf);

	return resultCode;
}

int
update(mportInstance *mport, const char *packageName) {
	mportIndexEntry **indexEntry;
	char *path;

	indexEntry = lookupIndex(mport, packageName);
        if (indexEntry == NULL || *indexEntry == NULL)
                return 1;

	asprintf(&path, "%s/%s", MPORT_LOCAL_PKG_PATH, (*indexEntry)->bundlefile);

	if (!mport_file_exists(path)) {
        	if (mport_fetch_bundle(mport, (*indexEntry)->bundlefile) != MPORT_OK) {
                	fprintf(stderr, "%s\n", mport_err_string());
			free(path);
                	return mport_err_code();
        	}
	}

	if (!mport_verify_hash(path, (*indexEntry)->hash)) {
		fprintf(stderr, "Package fails hash verification.\n");
		free(path);
		return 1;
	}

	if (mport_update_primative(mport, path) != MPORT_OK) {
		fprintf(stderr, "%s\n", mport_err_string());
		free(path);
		return mport_err_code();
	}
	free(path);
	mport_index_entry_free_vec(indexEntry);

	return 0;
}

int
upgrade(mportInstance *mport) {
	mportPackageMeta **packs;
	mportIndexEntry **indexEntries;
	int total = 0;
	int updated = 0;

	if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return(1);
	}

	if (packs == NULL) {
		fprintf(stderr, "No packages installed.\n");
		return(1);
	}

	while (*packs != NULL) {
		if (indexCheck(mport, *packs)) {
			updated += updateDown(mport, *packs);
		}
		packs++;
		total++;
	}
	mport_pkgmeta_vec_free(packs);
	printf("Packages updated: %d\nTotal: %d\n", updated, total);
	return (0);
}

int
indexCheck(mportInstance *mport, mportPackageMeta *pack) {
	mportIndexEntry **indexEntries;
	int ret = 0;

	if (mport_index_lookup_pkgname(mport, pack->name, &indexEntries) != MPORT_OK) {
		fprintf(stderr, "Error Looking up package name %s: %d %s\n", pack->name,  mport_err_code(), mport_err_string());
		return 0;
	}

	if (indexEntries != NULL) {
		while (*indexEntries != NULL) {
			if ((*indexEntries)->version != NULL && mport_version_cmp(pack->version, (*indexEntries)->version) < 0) {
				ret = 1;
				break;
			}
			indexEntries++;
		}
		mport_index_entry_free_vec(indexEntries);
	}

	return ret;
}

int
updateDown(mportInstance *mport, mportPackageMeta *pack) {
	mportPackageMeta **depends;
	int ret = 0;

	fprintf(stderr, "Entering %s\n", pack->name);

	if (mport_pkgmeta_get_downdepends(mport, pack, &depends) == MPORT_OK) {
		if (depends == NULL) {
			if (indexCheck(mport, pack)) {
				fprintf(stderr, "Updating %s\n", pack->name); 
				if (update(mport, pack->name) !=0) {
					fprintf(stderr, "Error updating %s\n", pack->name);
					ret = 0;
				} else
					ret = 1;
			} else
				ret = 0;
		} else {
			while (*depends != NULL) {
				ret += updateDown(mport, (*depends));
				if (indexCheck(mport, *depends)) {
					fprintf(stderr, "Updating depends %s\n", (*depends)->name);
					if (update(mport, (*depends)->name) != 0) {
						fprintf(stderr, "Error updating %s\n", (*depends)->name);
					} else
						ret++;
				}
				depends++;
			}
			if (indexCheck(mport, pack)) {
				fprintf(stderr, "Updating port called %s\n", pack->name);
				if (update(mport, pack->name) != 0) {
					fprintf(stderr, "Error updating %s\n", pack->name);
				} else
					ret++;
			}
		}
		mport_pkgmeta_vec_free(depends);
	}

	return ret;
}

int
deleteAll(mportInstance *mport) {
	mportPackageMeta **packs;
	mportPackageMeta **depends;
	int total = 0;
	int errors = 0;
	int skip;

	if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return(1);
	}

	if (packs == NULL) {
		fprintf(stderr, "No packages installed.\n");
		return(1);
	}

	while (1) {
		skip = 0;
		while (*packs != NULL) {
			if (mport_pkgmeta_get_updepends(mport, *packs, &depends) == MPORT_OK) {
				if (depends == NULL) {
					if (delete(mport, (*packs)->name) != 0) {
						fprintf(stderr, "Error deleting %s\n", (*packs)->name);
						errors++;
					}
					total++;
				} else {
					skip++;
					mport_pkgmeta_vec_free(depends);
				}
			}
			packs++;
		}
		if (skip == 0)
			break;
		mport_pkgmeta_vec_free(packs);
		if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
			warnx("%s", mport_err_string());
			return(1);
		}
	}

	mport_pkgmeta_vec_free(packs);

	printf("Packages deleted: %d\nErrors: %d\nTotal: %d\n", total - errors, errors, total);
	return (0);
}

int
clean(mportInstance *mport) {
	int ret;

	ret = mport_clean_database(mport);
	if (ret == MPORT_OK)
		ret = mport_clean_oldpackages(mport);
	return ret;
}
