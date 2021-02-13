/*-
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

#include <sys/cdefs.h>

#include <sys/cdefs.h>
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
static int parse_file_owner_mode(mportAssetListEntry **, char *);

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

    while (!STAILQ_EMPTY(list)) {
        n = STAILQ_FIRST(list);
        STAILQ_REMOVE_HEAD(list, next);
        free(n->data);
		free(n->checksum);
		free(n->owner);
		free(n->group);
		free(n->mode);
		/* type is not a pointer */
        free(n);
    }

    free(list);
}


/**
 * Parses the contents of the given plistfile pointer.  
 * Returns MPORT_OK on success, 
 * an error code on failure.
 */
MPORT_PUBLIC_API int
mport_parse_plistfile(FILE *fp, mportAssetList *list) {
    size_t length;
    size_t entrylen;
    char *line;

    assert(fp != NULL);

    while ((line = fgetln(fp, &length)) != NULL) {
        if (feof(fp)) {
            /* File didn't end in \n, get an exta byte so that the next step doesn't
               wack the last char in the string. */
            length++;
            if ((line = realloc(line, length)) == NULL) {
                RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
            }
        }

        if (length == 1)
            /* This is almost certainly a blank line. skip it */
            continue;


        /* change the last \n to \0 */
        *(line + length - 1) = '\0';

        mportAssetListEntry *entry = (mportAssetListEntry *) calloc(1, sizeof(mportAssetListEntry));

        if (entry == NULL) {
            RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
        }

        if (*line == CMND_MAGIC_COOKIE) {
            line++;
            char *cmnd = strsep(&line, " \t");

            if (cmnd == NULL)
                RETURN_ERROR(MPORT_ERR_FATAL, "Malformed plist file.");

		entry->checksum = NULL; /* checksum is only used by bundle read install */
			entry->type = parse_command(cmnd);
		if (entry->type == ASSET_FILE_OWNER_MODE)
			parse_file_owner_mode(&entry, cmnd);
		if (entry->type == ASSET_DIR_OWNER_MODE) {
			parse_file_owner_mode(&entry, &cmnd[3]);
		}
		if (entry->type == ASSET_SAMPLE_OWNER_MODE)
			parse_file_owner_mode(&entry, &cmnd[6]);
        } else {
            entry->type = ASSET_FILE;
        }


        if (line == NULL) {
            /* line was just a directive, no data */
            entry->data = NULL;
        } else {
            if (entry->type == ASSET_COMMENT) {
                if (!strncmp(line, "ORIGIN:", 7)) {
                    line += 7;
                    entry->type = ASSET_ORIGIN;
                } else if (!strncmp(line, "DEPORIGIN:", 10)) {
                    line += 10;
                    entry->type = ASSET_DEPORIGIN;
                }
            }

            entrylen = strlen(line) + 1;
            entry->data = (char *) calloc(entrylen, sizeof(char));
            if (entry->data == NULL) {
                RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
            }

            char *pos = line + strlen(line) - 1;

            while (isspace(*pos)) {
                *pos = 0;
                pos--;
            }

            strlcpy(entry->data, line, entrylen);
        }

        STAILQ_INSERT_TAIL(list, entry, next);
    }

    return MPORT_OK;
}

/**
 * Parse the file owner, group and mode.
 */
static int 
parse_file_owner_mode(mportAssetListEntry **entry, char *cmdLine) {
	char *start;
	char *op = start = strdup(cmdLine);
	char *permissions[3] = {NULL, NULL, NULL};
	char *tok;
	int i = 0;

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
		(*entry)->owner = strdup(permissions[0]);
	}
	if (permissions[1] != NULL) {
#ifdef DEBUG
		fprintf(stderr, "; group %s -", permissions[1]);
#endif
		(*entry)->group = strdup(permissions[1]);
	}
	if (permissions[2] != NULL) {
#ifdef DEBUG
		fprintf(stderr, "; mode %s -", permissions[2]);
#endif
		(*entry)->mode = strdup(permissions[2]);
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
    if (STRING_EQ(s, "ldconfig-linux"))
    	return ASSET_LDCONFIG_LINUX;
    if (STRING_EQ(s, "ldconfig"))
    	return ASSET_LDCONFIG;

    /* special case, starts with ( as in @(root,wheel,0755) */
    if (s[0] == '(')
		return ASSET_FILE_OWNER_MODE;

    return ASSET_INVALID;
}



