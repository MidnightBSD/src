/*-
 * Copyright (c) 2010, 2011 Lucas Holt
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
 */

#include <sys/cdefs.h>
__MBSDID("$MidnightBSD: src/libexec/mport.list/mport.list.c,v 1.7 2011/02/26 21:24:18 laffer1 Exp $");

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <mport.h>

static void usage(void);
static char * str_remove( const char *str, const char ch );

int 
main(int argc, char *argv[]) 
{
  int ch;
  mportInstance *mport;
  mportPackageMeta **packs;
  mportIndexEntry **indexEntries;
  bool quiet = false;
  bool verbose = false;
  bool origin = false;
  bool update = false;
  char *comment;

  if (argc > 2)
    usage();
    
  while ((ch = getopt(argc, argv, "oqvu")) != -1) {
    switch (ch) {
      case 'o':
        origin = true;
        break;
      case 'q':
        quiet = true;
        break;
      case 'v':
        verbose = true;
        break;
      case 'u':
        update = true;
        break; 
      case '?':
      default:
        usage();
        break; 
    }
  } 

  argc -= optind;
  argv += optind;

  mport = mport_instance_new();
  
  if (mport_instance_init(mport, NULL) != MPORT_OK) {
    warnx("%s", mport_err_string());
    exit(1);
  }

  if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
    warnx("%s", mport_err_string());
    mport_instance_free(mport);
    exit(1);
  }
  
  if (packs == NULL) {
    if (!quiet)
      warnx("No packages installed matching.");
    mport_instance_free(mport);
    exit(3);
  }

  if (update) {
    if (mport_index_load(mport) != MPORT_OK)
        errx(4, "Unable to load updates index");
  }
  
  while (*packs != NULL) {
    if (update) {
      mport_index_lookup_pkgname(mport, (*packs)->name, &indexEntries);

      if (indexEntries != NULL) {
        while (*indexEntries != NULL) {
          if (mport_version_cmp((*packs)->version, (*indexEntries)->version) == 1)
            (void) printf("%s: %s < %s\n", (*packs)->name, (*packs)->version, (*indexEntries)->version);
          indexEntries++;
        }

        mport_index_entry_free_vec(indexEntries);
      }
    } else if (verbose) {
      comment = str_remove((*packs)->comment, '\\');
      (void) printf("%s-%s\t%s\n", (*packs)->name, (*packs)->version, comment);
      free(comment);
    }
    else if (quiet && !origin)
      (void) printf("%s\n", (*packs)->name);
    else if (quiet && origin)
      (void) printf("%s\n", (*packs)->origin);
    else if (origin)
      (void) printf("Information for %s-%s:\n\nOrigin:\n%s\n\n",
	(*packs)->name, (*packs)->version, (*packs)->origin);
    else
      (void) printf("%s-%s\n", (*packs)->name, (*packs)->version);
    packs++;
  }

  mport_instance_free(mport); 
  
  return 0;
}


static char * 
str_remove( const char *str, const char ch )
{
   size_t i;
   size_t x;
   size_t len;
   char *output;

   if (str == NULL)
       return NULL;

  len = strlen(str);

   output = calloc(len, sizeof(char));

   for (i = 0, x = 0; i < len; i++) {
      if (str[i] != ch) {
          output[x] = str[i];
          x++;
      }
    }
    output[len -1] = '\0';

    return output;
} 


static void 
usage() 
{
  fprintf(stderr, "Usage: mport.list [-q | -v | -u]\n");
  exit(2);
}
