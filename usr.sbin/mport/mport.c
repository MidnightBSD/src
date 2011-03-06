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
__MBSDID("$MidnightBSD: src/usr.sbin/mport/mport.c,v 1.8 2011/03/06 00:40:02 laffer1 Exp $");

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mport.h>

#define MPORT_TOOLS_PATH "/usr/libexec/"
#define MPORT_LOCAL_PKG_PATH "/var/db/mport/downloads"

static void usage(void);
static mportIndexEntry ** lookupIndex(mportInstance *mport, const char *packageName);
static int install(mportInstance *mport, const char *packageName);
static int delete(mportInstance *mport, const char *packageName);
static int update(mportInstance *mport, const char *packageName);

int 
main(int argc, char *argv[]) {
	char *buf = NULL;
	mportInstance *mport;
	int resultCode;

	if (argc < 2)
		usage();

	mport = mport_instance_new();

	if (mport_instance_init(mport, NULL) != MPORT_OK) {
		warnx("%s", mport_err_string());
		exit(1);
	}

	if (!strcmp(argv[1], "install")) {
		resultCode = install(mport, argv[2]);
	} else if (!strcmp(argv[1], "delete")) {
		resultCode = delete(mport, argv[2]);
	} else if (!strcmp(argv[1], "update")) {
		resultCode = update(mport, argv[2]);
        } else if (!strcmp(argv[1], "list")) {
		if (argc > 2 && !strcmp(argv[2], "updates"))
			asprintf(&buf, "%s%s",
				MPORT_TOOLS_PATH,
				"mport.list -u");
		else
			asprintf(&buf, "%s%s",
                        	MPORT_TOOLS_PATH,
                        	"mport.list -v");
		system(buf);
		free(buf);
		resultCode = 0;
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
		"       mport delete [package name]\n"
		"       mport install [package name]\n"
		"	mport update [package name]\n"
		"       mport list [updates]\n"
	);
	exit(1);
}

mportIndexEntry **
lookupIndex(mportInstance *mport, const char *packageName)
{
	mportIndexEntry **indexEntries;

	if (mport_index_load(mport) != MPORT_OK)
		errx(4, "Unable to load updates index");
	
	if (mport_index_lookup_pkgname(mport, packageName, &indexEntries) != MPORT_OK) {
		fprintf(stderr, "Error looking up package name %s: %d %s\n", packageName,  mport_err_code(), mport_err_string());
		exit(mport_err_code());
	}

	return indexEntries;
}

int
install(mportInstance *mport, const char *packageName) {
	char *buf;
	mportIndexEntry **indexEntry;

	indexEntry = lookupIndex(mport, packageName);
	if (indexEntry == NULL || *indexEntry == NULL)
		return 1;

	/* TODO: verify package isn't already downloaded */
	if (mport_fetch_bundle(mport, (*indexEntry)->bundlefile) != MPORT_OK) {
		fprintf(stderr, "%s\n", mport_err_string());
		exit(mport_err_code());
	}

	asprintf(&buf, "%s%s %s/%s",
			MPORT_TOOLS_PATH,
			"mport.install",
			MPORT_LOCAL_PKG_PATH,
			(*indexEntry)->bundlefile);
	system(buf);
	free(buf);
	mport_index_entry_free_vec(indexEntry);

	return 0;
}

int
delete(mportInstance *mport, const char *packageName) {
	char *buf;

	asprintf(&buf, "%s%s %s",
			MPORT_TOOLS_PATH,
			"mport.delete -n",
			packageName);
	system(buf);
	free(buf);

	return 0;
}

int
update(mportInstance *mport, const char *packageName) {
	/* TODO: verify installed, do updepends */
	delete(mport, packageName);
	return install(mport, packageName);
} 
