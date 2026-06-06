/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2007-2009 Chris Reinhardt
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "mport.h"
#include "mport_private.h"

#define CMND_MAGIC_COOKIE '@'
#define STRING_EQ(r, l) (strcmp((r), (l)) == 0)

static mportAssetListEntryType parse_command(const char *);
static int parse_file_owner_mode(mportAssetListEntry *, char *);

struct command_map {
	const char *name;
	mportAssetListEntryType type;
};

static const struct command_map command_map_table[] = {
	{ "comment", ASSET_COMMENT },
	{ "preexec", ASSET_PREEXEC },
	{ "preunexec", ASSET_PREUNEXEC },
	{ "postexec", ASSET_POSTEXEC },
	{ "postunexec", ASSET_POSTUNEXEC },
	{ "exec", ASSET_EXEC },
	{ "unexec", ASSET_UNEXEC },
	{ "dir", ASSET_DIR },
	{ "dirrm", ASSET_DIRRM },
	{ "dirrmtry", ASSET_DIRRMTRY },
	{ "cwd", ASSET_CWD },
	{ "cd", ASSET_CWD },
	{ "srcdir", ASSET_SRC },
	{ "mode", ASSET_CHMOD },
	{ "owner", ASSET_CHOWN },
	{ "group", ASSET_CHGRP },
	{ "noinst", ASSET_NOINST },
	{ "ignore", ASSET_IGNORE },
	{ "ignore_inst", ASSET_IGNORE_INST },
	{ "info", ASSET_INFO },
	{ "name", ASSET_NAME },
	{ "display", ASSET_DISPLAY },
	{ "pkgdep", ASSET_PKGDEP },
	{ "conflicts", ASSET_CONFLICTS },
	{ "mtree", ASSET_MTREE },
	{ "option", ASSET_OPTION },
	{ "sample", ASSET_SAMPLE },
	{ "shell", ASSET_SHELL },
	{ "ldconfig-linux", ASSET_LDCONFIG_LINUX },
	{ "ldconfig", ASSET_LDCONFIG },
	{ "rmempty", ASSET_RMEMPTY },
	{ "glib-schemas", ASSET_GLIB_SCHEMAS },
	{ "kld", ASSET_KLD },
	{ "desktop-file-utils", ASSET_DESKTOP_FILE_UTILS },
	{ "touch", ASSET_TOUCH },
};

/* Do everything needed to set up a new plist.  Always use this to create a plist,
 * don't go off and do it yourself.
 */
MPORT_PUBLIC_API mportAssetList *
mport_assetlist_new(void)
{

	mportAssetList *list = (mportAssetList *)calloc(1, sizeof(mportAssetList));
	assert(list != NULL);
	STAILQ_INIT(list);
	return list;
}

/* free all the entries in the list, and then the list itself. */
MPORT_PUBLIC_API void
mport_assetlist_free(mportAssetList *list)
{
	mportAssetListEntry *n = NULL;
	mportAssetList *list_orig;
	list_orig = list;

	if (list == NULL)
		return;

	while (!STAILQ_EMPTY(list)) {
		n = STAILQ_FIRST(list);
		if (n == NULL)
			continue;

		free(n->data);
		n->data = NULL;

		STAILQ_REMOVE_HEAD(list, next);
		free(n);
		n = NULL;
	}

	free(list_orig);
	list_orig = NULL;
	list = NULL;
}

/**
 * Parses the contents of the given plistfile pointer.
 * Returns MPORT_OK on success,
 * an error code on failure.
 */
MPORT_PUBLIC_API int
mport_parse_plistfile(FILE *fp, mportAssetList *list)
{
	size_t linecap = 0;
	char *line = NULL;
	ssize_t read = 0;
	int ret = MPORT_OK;
	assert(fp != NULL);

	while ((read = getline(&line, &linecap, fp)) != -1) {
		if (read > 0 && line[read - 1] == '\n') {
			line[--read] = '\0';
		}

		char *parse_line = line;

		/* Skip blank/whitespace lines early to avoid allocation */
		while (*parse_line != '\0' && isspace((unsigned char)*parse_line)) {
			parse_line++;
		}
		if (*parse_line == '\0')
			continue;

		mportAssetListEntry *entry =
		    (mportAssetListEntry *)calloc(1, sizeof(mportAssetListEntry));
		if (entry == NULL) {
			SET_ERROR(MPORT_ERR_FATAL, "Out of memory.");
			ret = MPORT_ERR_FATAL;
			goto cleanup;
		}

		/* reset parse_line because leading whitespace might be significant for
		 * non-directives */
		parse_line = line;

		if (*parse_line == CMND_MAGIC_COOKIE) {
			parse_line++;
			/* directive lines skip leading spaces after @ */
			while (*parse_line != '\0' && isspace((unsigned char)*parse_line)) {
				parse_line++;
			}

			char *cmnd = strsep(&parse_line, " \t");
			if (cmnd == NULL || *cmnd == '\0') {
				free(entry);
				SET_ERROR(MPORT_ERR_FATAL, "Malformed plist file.");
				ret = MPORT_ERR_FATAL;
				goto cleanup;
			}

			entry->checksum[0] =
			    '\0'; /* checksum is only used by bundle read install */
			entry->type = parse_command(cmnd);
			if (entry->type == ASSET_FILE_OWNER_MODE) {
				if (parse_file_owner_mode(entry, cmnd) != MPORT_OK) {
					free(entry);
					ret = MPORT_ERR_FATAL;
					goto cleanup;
				}
			} else if (entry->type == ASSET_DIR_OWNER_MODE) {
				if (parse_file_owner_mode(entry, &cmnd[3]) != MPORT_OK) {
					free(entry);
					ret = MPORT_ERR_FATAL;
					goto cleanup;
				}
			} else if (entry->type == ASSET_SAMPLE_OWNER_MODE) {
				if (parse_file_owner_mode(entry, &cmnd[6]) != MPORT_OK) {
					free(entry);
					ret = MPORT_ERR_FATAL;
					goto cleanup;
				}
			}

			/* Skip leading whitespace for directive data */
			while (parse_line != NULL && *parse_line != '\0' &&
			    isspace((unsigned char)*parse_line)) {
				parse_line++;
			}
		} else {
			/* command is backed by a file */
			entry->type = ASSET_FILE;
		}

		if (parse_line == NULL || *parse_line == '\0') {
			/* line was just a directive, no data */
			entry->data = NULL;
		} else {
			if (entry->type == ASSET_COMMENT) {
				if (!strncmp(parse_line, "ORIGIN:", 7)) {
					parse_line += 7;
					entry->type = ASSET_ORIGIN;
				} else if (!strncmp(parse_line, "DEPORIGIN:", 10)) {
					parse_line += 10;
					entry->type = ASSET_DEPORIGIN;
				}
			}

			size_t buflen = strlen(parse_line);
			char *pos = parse_line + buflen - 1;
			while (pos >= parse_line && isspace((unsigned char)*pos)) {
				*pos = '\0';
				pos--;
			}

			entry->data = strdup(parse_line);
			if (entry->data == NULL) {
				free(entry);
				SET_ERROR(MPORT_ERR_FATAL, "Out of memory.");
				ret = MPORT_ERR_FATAL;
				goto cleanup;
			}
		}

		STAILQ_INSERT_TAIL(list, entry, next);
	}

cleanup:
	free(line);
	return ret;
}

/**
 * Parse the file owner, group and mode.
 */
static int
parse_file_owner_mode(mportAssetListEntry *entry, char *cmnd)
{
	char *op = cmnd;
	char *permissions[3] = { NULL, NULL, NULL };
	char *tok = NULL;
	int i = 0;

	if (entry == NULL || cmnd == NULL) {
		RETURN_ERROR(MPORT_ERR_FATAL, "Entry or command is NULL");
	}

	while ((tok = strsep(&op, "(,)")) != NULL && i < 3) {
		if (*tok == '\0')
			continue;
		permissions[i++] = tok;
	}

	if (permissions[0] != NULL) {
		strlcpy(entry->owner, permissions[0], MAXLOGNAME);
	}
	if (permissions[1] != NULL) {
		strlcpy(entry->group, permissions[1], MAXLOGNAME * 2);
	}
	if (permissions[2] != NULL) {
		strlcpy(entry->mode, permissions[2], 5);
	}

	return MPORT_OK;
}

static mportAssetListEntryType
parse_command(const char *s)
{
	if (s == NULL || *s == '\0')
		return ASSET_INVALID;

	/* special/Prefix cases first */
	if (s[0] == '(')
		return ASSET_FILE_OWNER_MODE;
	if (strncmp(s, "dir(", 4) == 0)
		return ASSET_DIR_OWNER_MODE;
	if (strncmp(s, "sample(", 7) == 0)
		return ASSET_SAMPLE_OWNER_MODE;

	for (size_t i = 0; i < sizeof(command_map_table) / sizeof(command_map_table[0]); i++) {
		if (STRING_EQ(s, command_map_table[i].name))
			return command_map_table[i].type;
	}

	return ASSET_INVALID;
}
