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
 *
 * $MidnightBSD: src/lib/libmport/plist.c,v 1.4 2008/01/05 22:18:20 ctriv Exp $
 */



#include <sys/cdefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "mport.h"
#include "mport_private.h"


#define CMND_MAGIC_COOKIE '@'
#define STRING_EQ(r,l) (strcmp((r),(l)) == 0)

static mportAssetListEntryType parse_command(const char*);

/* Do everything needed to set up a new plist.  Always use this to create a plist,
 * don't go off and do it yourself.
 */
MPORT_PUBLIC_API mportAssetList* mport_assetlist_new() 
{
  mportAssetList *list = (mportAssetList*)malloc(sizeof(mportAssetList));
  STAILQ_INIT(list);
  return list;
}


/* free all the entries in the list, and then the list itself. */
MPORT_PUBLIC_API void mport_assetlist_free(mportAssetList *list) 
{
  mportAssetListEntry *n;

  while (!STAILQ_EMPTY(list)) {
     n = STAILQ_FIRST(list);
     STAILQ_REMOVE_HEAD(list, next);
     free(n->data);
     free(n);
  }
  
  free(list);
}


/* Parses the contents of the given plistfile pointer.  Returns MPORT_OK on success, 
 * an error code on failure.
 *
 */
MPORT_PUBLIC_API int mport_parse_plistfile(FILE *fp, mportAssetList *list)
{
  size_t length;
  char *line;
  
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
    *(line + length - 1) = 0;
    
    mportAssetListEntry *entry = (mportAssetListEntry *)malloc(sizeof(mportAssetListEntry));
    
    if (entry == NULL) {
      RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
    }
       
    if (*line == CMND_MAGIC_COOKIE) {
      line++;
      char *cmnd = strsep(&line, " \t");
      
      if (cmnd == NULL) 
        RETURN_ERROR(MPORT_ERR_FATAL, "Malformed plist file.");

      entry->type = parse_command(cmnd);      
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
      
      entry->data = (char  *)malloc(strlen(line) + 1);
      if (entry->data == NULL) {
        RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
      }

      char *pos = line + strlen(line) - 1;
      
      while (isspace(*pos)) {
        *pos = 0;
        pos--;
      }
      
      strlcpy(entry->data, line, (strlen(line) + 1));
    }
    
    STAILQ_INSERT_TAIL(list, entry, next);
  }

  return MPORT_OK;  
}


    
static mportAssetListEntryType parse_command(const char *s) 
{
  /* This is in a rough frequency order */
  if (STRING_EQ(s, "comment"))
    return ASSET_COMMENT;
  if (STRING_EQ(s, "exec"))
    return ASSET_EXEC;
  if (STRING_EQ(s, "unexec"))
    return ASSET_UNEXEC;
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
  
  return ASSET_INVALID;
}



