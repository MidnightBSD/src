/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014, 2025 Lucas Holt
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
#include <locale.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <mport.h>
#include <ucl.h>

#define MPORT_LUA_PRE_INSTALL_FILE "pkg-pre-install.lua"
#define MPORT_LUA_POST_INSTALL_FILE "pkg-post-install.lua"
#define MPORT_LUA_PRE_DEINSTALL_FILE "pkg-pre-deinstall.lua"
#define MPORT_LUA_POST_DEINSTALL_FILE "pkg-post-deinstall.lua"

static void usage(void);

static void check_for_required_args(
    /*@notnull@*/ const mportPackageMeta *, /*@notnull@*/ const mportCreateExtras *);
static void manifest_alloc_failure(
    /*@null@*/ ucl_object_t *, /*@notnull@*/ mportPackageMeta *, /*@notnull@*/ mportCreateExtras *);
static void parse_manifest_ucl(/*@notnull@*/ const char *, /*@notnull@*/ mportPackageMeta *,
    /*@notnull@*/ mportCreateExtras *);
static void checked_strlcpy(/*@notnull@*/ char *, /*@notnull@*/ const char *, size_t,
    /*@notnull@*/ const char *);
static void manifest_strdup_field(/*@null@*/ ucl_object_t *, /*@notnull@*/ mportPackageMeta *,
    /*@notnull@*/ mportCreateExtras *, /*@notnull@*/ char **, /*@notnull@*/ const char *);

int
main(int argc, char *argv[])
{
	int ch;
	int plist_seen = 0;
	/*@only@*/ mportInstance *mport = mport_instance_new();
	/*@only@*/ mportPackageMeta *pack = mport_pkgmeta_new();
	/*@only@*/ mportCreateExtras *extra = mport_createextras_new();
	/*@only@*/ mportAssetList *assetlist = mport_assetlist_new();
	FILE *fp;
	struct tm expDate;
	int result = EXIT_SUCCESS;

	(void)setlocale(LC_ALL, "");

	if (mport == NULL || pack == NULL || extra == NULL || assetlist == NULL) {
		errx(EXIT_FAILURE, "Failed to allocate memory");
	}

	// we need this to know if the user customized the "target_os" configuration.
	// the caveat is that the userland it was built against could be wrong.
	if (mport_instance_init(mport, NULL, NULL, false, false) != MPORT_OK) {
		mport_instance_free(mport);
		mport_pkgmeta_free(pack);
		mport_createextras_free(extra);
		mport_assetlist_free(assetlist);
		errx(EXIT_FAILURE, "%s", mport_err_string());
	}

	while ((ch = getopt(argc, argv, "A:C:D:E:L:M:O:P:S:X:c:d:e:f:i:j:l:m:n:o:p:r:s:t:v:x:")) !=
	    -1) {
		switch (ch) {
		case 'o':
			checked_strlcpy(extra->pkg_filename, optarg, sizeof(extra->pkg_filename),
			    "package filename");
			break;
		case 'n':
			if (optarg != NULL) {
				pack->name = strdup(optarg);
				if (pack->name == NULL)
					errx(EXIT_FAILURE, "Out of memory");
			}
			break;
		case 'v':
			if (optarg != NULL) {
				pack->version = strdup(optarg);
				if (pack->version == NULL)
					errx(EXIT_FAILURE, "Out of memory");
			}
			break;
		case 'c':
			if (optarg != NULL) {
				pack->comment = strdup(optarg);
				if (pack->comment == NULL)
					errx(EXIT_FAILURE, "Out of memory");
			}
			break;
		case 'f':
			if (optarg != NULL) {
				pack->flavor = strdup(optarg);
				if (pack->flavor == NULL)
					errx(EXIT_FAILURE, "Out of memory");
			}
			break;
		case 'e':
			if (optarg != NULL) {
				pack->cpe = strdup(optarg);
				if (pack->cpe == NULL)
					errx(EXIT_FAILURE, "Out of memory");
			}
			break;
		case 'l':
			if (optarg != NULL) {
				pack->lang = strdup(optarg);
				if (pack->lang == NULL)
					errx(EXIT_FAILURE, "Out of memory");
			}
			break;
		case 's':
			checked_strlcpy(
			    extra->sourcedir, optarg, sizeof(extra->sourcedir), "source dir");
			break;
		case 'd':
			if (optarg != NULL) {
				pack->desc = strdup(optarg);
				if (pack->desc == NULL)
					errx(EXIT_FAILURE, "Out of memory");
			}
			break;
		case 'p':
			if ((fp = fopen(optarg, "r")) == NULL) {
				err(1, "%s", optarg);
			}
			if (mport_parse_plistfile(fp, assetlist) != 0) {
				warnx("Could not parse plist file '%s'.\n", optarg);
				fclose(fp);
				result = EXIT_FAILURE;
				goto cleanup;
			}
			fclose(fp);

			plist_seen++;

			break;
		case 'P':
			if (optarg != NULL) {
				pack->prefix = strdup(optarg);
				if (pack->prefix == NULL)
					errx(EXIT_FAILURE, "Out of memory");
			}
			break;
		case 'D':
			mport_parselist(optarg, &(extra->depends), &(extra->depends_count));
			break;
		case 'M':
			extra->mtree = strdup(optarg);
			if (extra->mtree == NULL)
				errx(EXIT_FAILURE, "Out of memory");
			break;
		case 'O':
			if (optarg != NULL) {
				pack->origin = strdup(optarg);
				if (pack->origin == NULL)
					errx(EXIT_FAILURE, "Out of memory");
			}
			break;
		case 'C':
			mport_parselist_tll(optarg, &(extra->conflicts));
			break;
		case 'A':
			mport_parselist_tll(optarg, &(extra->annotations));
			break;
		case 'E':
			memset(&expDate, 0, sizeof(expDate));
			if (strptime(optarg, "%Y-%m-%d", &expDate) == NULL)
				errx(EXIT_FAILURE, "Invalid expiration date '%s'", optarg);
			pack->expiration_date = mktime(&expDate);
			if (pack->expiration_date == (time_t)-1)
				errx(EXIT_FAILURE, "Invalid expiration date '%s'", optarg);
			break;
		case 'S':
			if (optarg[0] == '1' || optarg[0] == 'Y' || optarg[0] == 'y' ||
			    optarg[0] == 'T' || optarg[0] == 't')
				pack->no_provide_shlib = 1;
			else
				pack->no_provide_shlib = 0;
			break;
		case 'L':
			if (optarg != NULL) {
				asprintf(&extra->luapkgpostinstall, "%s/%s", optarg,
				    MPORT_LUA_POST_INSTALL_FILE);
				asprintf(&extra->luapkgpreinstall, "%s/%s", optarg,
				    MPORT_LUA_PRE_INSTALL_FILE);
				asprintf(&extra->luapkgpostdeinstall, "%s/%s", optarg,
				    MPORT_LUA_POST_DEINSTALL_FILE);
				asprintf(&extra->luapkgpredeinstall, "%s/%s", optarg,
				    MPORT_LUA_PRE_DEINSTALL_FILE);
				if (extra->luapkgpostinstall == NULL ||
				    extra->luapkgpreinstall == NULL ||
				    extra->luapkgpostdeinstall == NULL ||
				    extra->luapkgpredeinstall == NULL)
					errx(EXIT_FAILURE, "Out of memory");
			}
			break;
		case 'i':
			extra->pkginstall = strdup(optarg);
			if (extra->pkginstall == NULL)
				errx(EXIT_FAILURE, "Out of memory");
			break;
		case 'j':
			extra->pkgdeinstall = strdup(optarg);
			if (extra->pkgdeinstall == NULL)
				errx(EXIT_FAILURE, "Out of memory");
			break;
		case 'm':
			extra->pkgmessage = strdup(optarg);
			if (extra->pkgmessage == NULL)
				errx(EXIT_FAILURE, "Out of memory");
			break;
		case 't':
			mport_parselist(optarg, &(pack->categories), &(pack->categories_count));
			break;
		case 'x':
			if (optarg != NULL) {
				pack->deprecated = strdup(optarg);
				if (pack->deprecated == NULL)
					errx(EXIT_FAILURE, "Out of memory");
			}
			break;
		case 'X':
			parse_manifest_ucl(optarg, pack, extra);
			break;
		case '?':
		default:
			usage();
			break;
		}
	}

	check_for_required_args(pack, extra);
	if (plist_seen == 0) {
		warnx("Required arg missing: plist");
		usage();
	}

	pack->type = MPORT_TYPE_APP; /* Todo: This should be configurable */

	if (mport_create_primative(mport, assetlist, pack, extra) != MPORT_OK) {
		warnx("%s", mport_err_string());
		result = EXIT_FAILURE;
		goto cleanup;
	}

cleanup:
	mport_pkgmeta_free(pack);
	mport_createextras_free(extra);
	/* TODO: fix asset free mport_assetlist_free(assetlist); */
	mport_instance_free(mport);

	return result;
}

#define CHECK_ARG(exp, errmsg)                              \
	if (exp == NULL) {                                  \
		warnx("Required arg missing: %s", #errmsg); \
		usage();                                    \
	}

static void
check_for_required_args(const mportPackageMeta *pkg, const mportCreateExtras *extra)
{
	CHECK_ARG(pkg->name, "package name")
	CHECK_ARG(pkg->version, "package version");
	CHECK_ARG(extra->pkg_filename, "package filename");
	CHECK_ARG(extra->sourcedir, "source dir");
	CHECK_ARG(pkg->prefix, "prefix");
	CHECK_ARG(pkg->origin, "origin");
	CHECK_ARG(pkg->categories, "categories");
}

static void
checked_strlcpy(char *dst, const char *src, size_t dstlen, const char *field_name)
{
	if (strlcpy(dst, src, dstlen) >= dstlen)
		errx(EXIT_FAILURE, "%s is too long", field_name);
}

static void
usage(void)
{
	fprintf(stderr, "\nmport.create <arguments>\n");
	fprintf(stderr, "Arguments:\n");
	fprintf(stderr, "\t-n <package name>\n");
	fprintf(stderr, "\t-v <package version>\n");
	fprintf(stderr, "\t-o <package filename>\n");
	fprintf(stderr, "\t-s <source dir (usually the fake destdir)>\n");
	fprintf(stderr, "\t-p <plist filename>\n");
	fprintf(stderr, "\t-P <prefix>\n");
	fprintf(stderr, "\t-O <origin>\n");
	fprintf(stderr, "\t-c <comment (short description)>\n");
	fprintf(stderr, "\t-e <cpe string>\n");
	fprintf(stderr, "\t-l <package lang>\n");
	fprintf(stderr, "\t-D <package depends>\n");
	fprintf(stderr, "\t-C <package conflicts>\n");
	fprintf(stderr, "\t-A <package annotations>\n");
	fprintf(stderr, "\t-d <pkg-descr file>\n");
	fprintf(stderr, "\t-i <pkg-install script>\n");
	fprintf(stderr, "\t-j <pkg-deinstall script>\n");
	fprintf(stderr, "\t-m <pkg-message file>\n");
	fprintf(stderr, "\t-M <mtree file>\n");
	fprintf(stderr, "\t-t <categories>\n");
	fprintf(stderr, "\t-X <UCL manifest file>\n");
	exit(1);
}

static void
manifest_alloc_failure(ucl_object_t *root, mportPackageMeta *pack, mportCreateExtras *extra)
{
	if (root != NULL)
		ucl_object_unref(root);
	mport_pkgmeta_free(pack);
	mport_createextras_free(extra);
	errx(EXIT_FAILURE, "Out of memory while parsing manifest");
}

static void
manifest_strdup_field(ucl_object_t *root, mportPackageMeta *pack, mportCreateExtras *extra,
    char **field, const char *value)
{
	*field = strdup(value);
	if (*field == NULL)
		manifest_alloc_failure(root, pack, extra);
}

static void
parse_manifest_ucl(const char *manifest_file, mportPackageMeta *pack, mportCreateExtras *extra)
{
	struct ucl_parser *parser;
	ucl_object_t *root;
	const ucl_object_t *obj;

	parser = ucl_parser_new(0);
	if (!ucl_parser_add_file(parser, manifest_file)) {
		warnx(
		    "Failed to parse manifest %s: %s", manifest_file, ucl_parser_get_error(parser));
		ucl_parser_free(parser);
		exit(1);
	}

	root = ucl_parser_get_object(parser);
	ucl_parser_free(parser);

	if (root == NULL) {
		warnx("Manifest %s is empty or invalid", manifest_file);
		exit(1);
	}

	if ((obj = ucl_object_lookup(root, "name")) != NULL && ucl_object_type(obj) == UCL_STRING)
		manifest_strdup_field(root, pack, extra, &pack->name, ucl_object_tostring(obj));

	if ((obj = ucl_object_lookup(root, "version")) != NULL &&
	    ucl_object_type(obj) == UCL_STRING)
		manifest_strdup_field(root, pack, extra, &pack->version, ucl_object_tostring(obj));

	if ((obj = ucl_object_lookup(root, "comment")) != NULL &&
	    ucl_object_type(obj) == UCL_STRING)
		manifest_strdup_field(root, pack, extra, &pack->comment, ucl_object_tostring(obj));

	if ((obj = ucl_object_lookup(root, "desc")) != NULL && ucl_object_type(obj) == UCL_STRING)
		manifest_strdup_field(root, pack, extra, &pack->desc, ucl_object_tostring(obj));

	if ((obj = ucl_object_lookup(root, "prefix")) != NULL && ucl_object_type(obj) == UCL_STRING)
		manifest_strdup_field(root, pack, extra, &pack->prefix, ucl_object_tostring(obj));

	if ((obj = ucl_object_lookup(root, "origin")) != NULL && ucl_object_type(obj) == UCL_STRING)
		manifest_strdup_field(root, pack, extra, &pack->origin, ucl_object_tostring(obj));

	if ((obj = ucl_object_lookup(root, "cpe")) != NULL && ucl_object_type(obj) == UCL_STRING)
		manifest_strdup_field(root, pack, extra, &pack->cpe, ucl_object_tostring(obj));

	if ((obj = ucl_object_lookup(root, "categories")) != NULL &&
	    ucl_object_type(obj) == UCL_ARRAY) {
		ucl_object_iter_t it = NULL;
		const ucl_object_t *cat;
		size_t count = 0;
		while ((cat = ucl_object_iterate(obj, &it, true)) != NULL) {
			if (ucl_object_type(cat) == UCL_STRING) {
				count++;
			}
		}
		if (count > 0) {
			pack->categories = calloc(count + 1, sizeof(char *));
			if (pack->categories == NULL)
				manifest_alloc_failure(root, pack, extra);
			pack->categories_count = count;
			it = NULL;
			size_t i = 0;
			while ((cat = ucl_object_iterate(obj, &it, true)) != NULL) {
				if (ucl_object_type(cat) == UCL_STRING) {
					manifest_strdup_field(root, pack, extra,
					    &pack->categories[i++], ucl_object_tostring(cat));
				}
			}
			pack->categories[i] = NULL;
		}
	}

	if ((obj = ucl_object_lookup(root, "deps")) != NULL && ucl_object_type(obj) == UCL_OBJECT) {
		ucl_object_iter_t it = NULL;
		const ucl_object_t *dep;
		size_t count = 0;

		while ((dep = ucl_object_iterate(obj, &it, true)) != NULL) {
			count++;
		}

		if (count > 0) {
			extra->depends = calloc(count + 1, sizeof(char *));
			if (extra->depends == NULL)
				manifest_alloc_failure(root, pack, extra);
			extra->depends_count = count;
			it = NULL;
			size_t i = 0;
			while ((dep = ucl_object_iterate(obj, &it, true)) != NULL) {
				const char *pkgname = ucl_object_key(dep);
				const ucl_object_t *origin_obj = ucl_object_lookup(dep, "origin");
				const ucl_object_t *version_obj = ucl_object_lookup(dep, "version");
				if (pkgname && origin_obj && version_obj &&
				    ucl_object_type(origin_obj) == UCL_STRING &&
				    ucl_object_type(version_obj) == UCL_STRING) {
					if (asprintf(&extra->depends[i], "%s:%s:%s", pkgname,
						ucl_object_tostring(origin_obj),
						ucl_object_tostring(version_obj)) == -1)
						manifest_alloc_failure(root, pack, extra);
					i++;
				}
			}
			extra->depends[i] = NULL;
		}
	}

	if ((obj = ucl_object_lookup(root, "annotations")) != NULL &&
	    ucl_object_type(obj) == UCL_OBJECT) {
		ucl_object_iter_t it = NULL;
		const ucl_object_t *ann;

		while ((ann = ucl_object_iterate(obj, &it, true)) != NULL) {
			const char *tag = ucl_object_key(ann);
			if (tag && ucl_object_type(ann) == UCL_STRING) {
				char *ann_str;
				if (asprintf(&ann_str, "%s:%s", tag, ucl_object_tostring(ann)) ==
				    -1)
					manifest_alloc_failure(root, pack, extra);
				tll_push_back(extra->annotations, ann_str);
			}
		}
	}

	if ((obj = ucl_object_lookup(root, "shlibs_provided")) != NULL &&
	    ucl_object_type(obj) == UCL_ARRAY) {
		if (ucl_array_size(obj) > 0) {
			pack->no_provide_shlib = 0;
		} else {
			pack->no_provide_shlib = 1;
		}
	} else {
		pack->no_provide_shlib = 1;
	}

	ucl_object_unref(root);
}
