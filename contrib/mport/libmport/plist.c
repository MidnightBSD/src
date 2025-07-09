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
#define STRING_EQ(r, l) (strcmp((r),(l)) == 0)

static mportAssetListEntryType parse_command(const char *);
static int parse_file_owner_mode(mportAssetListEntry *, char *);

/* Do everything needed to set up a new plist.  Always use this to create a plist,
 * don't go off and do it yourself.
 */
MPORT_PUBLIC_API mportAssetList *
mport_assetlist_new(void) {

    mportAssetList *list = (mportAssetList *) calloc(1, sizeof(mportAssetList));
    assert(list != NULL);
    STAILQ_INIT(list);
    return list;
}


/* free all the entries in the list, and then the list itself. */
MPORT_PUBLIC_API void
mport_assetlist_free(mportAssetList *list) {
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
mport_parse_plistfile(FILE *fp, mportAssetList *list) {
    size_t linecap = 0;
    char *line = NULL;
    ssize_t read = 0;
    assert(fp != NULL);

    while ((read = getline(&line, &linecap, fp)) != -1) {
        if (line[read - 1] == '\n')
            line[read - 1] = '\0';

        /* skip blank lines */
        if (line[0] == '\0' || read == 1)
            continue;

        mportAssetListEntry *entry = (mportAssetListEntry *) calloc(1, sizeof(mportAssetListEntry));

        if (entry == NULL) {
            RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
        }

        char *parse_line = line;

        /* clear out any leading whitespace */
        while (*parse_line != '\0' && isspace(*parse_line)) {
            parse_line++;
        }

        /* line is effectively empty. skip it. */
        if (*parse_line == '\0') {
            continue;
        }

        if (*parse_line == CMND_MAGIC_COOKIE) {
            parse_line++;
            char *cmnd = strsep(&parse_line, " \t");

            if (cmnd == NULL) {
                free(entry);
                RETURN_ERROR(MPORT_ERR_FATAL, "Malformed plist file.");
            }

		entry->checksum[0] = '\0'; /* checksum is only used by bundle read install */
		entry->type = parse_command(cmnd);
		if (entry->type == ASSET_FILE_OWNER_MODE)
            if (parse_file_owner_mode(entry, cmnd) != MPORT_OK) {
                free(entry);
                RETURN_ERROR(MPORT_ERR_FATAL, "Failed to parse file owner mode");
            }
		if (entry->type == ASSET_DIR_OWNER_MODE) {
            if (parse_file_owner_mode(entry, &cmnd[3]) != MPORT_OK) {
                free(entry);
                RETURN_ERROR(MPORT_ERR_FATAL, "Failed to parse dir owner mode");
            }
		}
		if (entry->type == ASSET_SAMPLE_OWNER_MODE)
            if (parse_file_owner_mode(entry, &cmnd[6]) != MPORT_OK) {
                free(entry);
                RETURN_ERROR(MPORT_ERR_FATAL, "Failed to parse sample owner mode");
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
            if (buflen > SIZE_MAX - 1) {
                // Handle overflow error
                free(entry);
                RETURN_ERROR(MPORT_ERR_FATAL, "Buffer too large, potential overflow.");
            }

            char *pos = parse_line + buflen - 1;
            while (pos >= parse_line && isspace(*pos)) {
                *pos = '\0';
                pos--;
            }

            entry->data = strdup(parse_line);
            if (entry->data == NULL) {
                free(entry);
                RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
            }
        }

        STAILQ_INSERT_TAIL(list, entry, next);
    }
    free(line);

    return MPORT_OK;
}

/**
 * Parse the file owner, group and mode.
 */
static int 
parse_file_owner_mode(mportAssetListEntry *entry, char *cmnd) {
	char *start = NULL;
	char *op;
	char *permissions[3] = {NULL, NULL, NULL};
	char *tok = NULL;
	int i = 0;

    if (entry == NULL) {
        RETURN_ERROR(MPORT_ERR_FATAL, "Entry is NULL");
    }

    if (cmnd == NULL)
        RETURN_ERROR(MPORT_ERR_FATAL, "command is missing");
    op = start = strdup(cmnd);
    if (op == NULL) {
        RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
    }

	while((tok = strsep(&op, "(,)")) != NULL) {
		if (i == 3)
			break;
		permissions[i] = op;
		i++;
	}

	if (permissions[0] != NULL) {
#ifdef DEBUG
		fprintf(stderr, "owner %s -", permissions[0]);
#endif
		strlcpy(entry->owner, permissions[0], MAXLOGNAME);
	}
	if (permissions[1] != NULL) {
#ifdef DEBUG
		fprintf(stderr, "; group %s -", permissions[1]);
#endif
		strlcpy(entry->group, permissions[1], MAXLOGNAME * 2);
	}
	if (permissions[2] != NULL) {
#ifdef DEBUG
		fprintf(stderr, "; mode %s -", permissions[2]);
#endif
		strlcpy(entry->mode, permissions[2], 5);
	}

	free(start);

	return MPORT_OK;
}



static mportAssetListEntryType
parse_command(const char *s) {

    /* This is in a rough frequency order */

    if (STRING_EQ(s, "comment"))
        return ASSET_COMMENT;

    if (STRING_EQ(s, "preexec"))
        return ASSET_PREEXEC;
    if (STRING_EQ(s, "preunexec"))
        return ASSET_PREUNEXEC;
    if (STRING_EQ(s, "postexec"))
        return ASSET_POSTEXEC;
    if (STRING_EQ(s, "postunexec"))
        return ASSET_POSTUNEXEC;

    /* EXEC and UNEXEC are deprecated in favor of their pre/post variants */
    if (STRING_EQ(s, "exec"))
        return ASSET_EXEC;
    if (STRING_EQ(s, "unexec"))
        return ASSET_UNEXEC;

    /* dir is preferred to dirrm and dirrmtry */
    if (STRING_EQ(s, "dir"))
        return ASSET_DIR;
    if (strncmp(s, "dir(", 4) == 0)
        return ASSET_DIR_OWNER_MODE;
    if (STRING_EQ(s, "dirrm"))
        return ASSET_DIRRM;
    if (STRING_EQ(s, "dirrmtry"))
        return ASSET_DIRRMTRY;
    if (STRING_EQ(s, "cwd") || STRING_EQ(s, "cd"))
        return ASSET_CWD;

    if (STRING_EQ(s, "srcdir"))
        return ASSET_SRC;
    if (STRING_EQ(s, "mode"))
        return ASSET_CHMOD;
    if (STRING_EQ(s, "owner"))
        return ASSET_CHOWN;
    if (STRING_EQ(s, "group"))
        return ASSET_CHGRP;
    if (STRING_EQ(s, "noinst"))
        return ASSET_NOINST;
    if (STRING_EQ(s, "ignore"))
        return ASSET_IGNORE;
    if (STRING_EQ(s, "ignore_inst"))
        return ASSET_IGNORE_INST;
    if (STRING_EQ(s, "info"))
        return ASSET_INFO;
    if (STRING_EQ(s, "name"))
        return ASSET_NAME;
    if (STRING_EQ(s, "display"))
        return ASSET_DISPLAY;
    if (STRING_EQ(s, "pkgdep"))
        return ASSET_PKGDEP;
    if (STRING_EQ(s, "conflicts"))
        return ASSET_CONFLICTS;
    if (STRING_EQ(s, "mtree"))
        return ASSET_MTREE;
    if (STRING_EQ(s, "option"))
        return ASSET_OPTION;
    if (STRING_EQ(s, "sample"))
        return ASSET_SAMPLE;
    if (strncmp(s, "sample(", 7) == 0)
		return ASSET_SAMPLE_OWNER_MODE;
    if (STRING_EQ(s, "shell"))
        return ASSET_SHELL;
    if (STRING_EQ(s, "ldconfig-linux")) {
    	return ASSET_LDCONFIG_LINUX;
    }
    if (STRING_EQ(s, "ldconfig")) {
    	return ASSET_LDCONFIG;
    }
    if (STRING_EQ(s, "rmempty")) {
    	return ASSET_RMEMPTY;
    }
    if (STRING_EQ(s, "glib-schemas")) {
        return ASSET_GLIB_SCHEMAS;
    }
    if (STRING_EQ(s, "kld")) {
        return ASSET_KLD;
    }
    if (STRING_EQ(s, "desktop-file-utils")) {
        return ASSET_DESKTOP_FILE_UTILS;
    }
    if (STRING_EQ(s, "touch")) {
        return ASSET_TOUCH;
    }

    /* special case, starts with ( as in @(root,wheel,0755) */
    if (s[0] == '(') {
		return ASSET_FILE_OWNER_MODE;
    }

    return ASSET_INVALID;
}



