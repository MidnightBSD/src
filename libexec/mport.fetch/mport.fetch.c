/*-
 * Copyright (c) 2011 Lucas Holt
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
__MBSDID("$MidnightBSD: src/libexec/mport.list/mport.list.c,v 1.10 2011/03/03 20:59:44 laffer1 Exp $");

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <mport.h>
#include "mport_private.h"

static void usage(void);

int 
main(int argc, char *argv[]) 
{
  int ch;
  mportInstance *mport;
  mportIndexEntry **indexEntries;
  bool verbose = false;
  char *bundleFile;

  if (argc < 2)
    usage();
    
  while ((ch = getopt(argc, argv, "v")) != -1) {
    switch (ch) {
      case 'v':
        verbose = true;
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

  if (mport_index_load(mport) != MPORT_OK)
    errx(4, "Unable to load updates index");

  if (mport_index_lookup_pkgname(mport, argv[0], &indexEntries) != MPORT_OK) {
    fprintf(stderr, "Error looking up package name %s: %d %s\n", argv[0],  mport_err_code(), mport_err_string());
    exit(mport_err_code());
  }

  if (indexEntries != NULL) {
    /* TODO: currently only fetches first match */
    if (*indexEntries != NULL) {
      bundleFile = strdup((*indexEntries)->bundlefile);
      mport_index_entry_free_vec(indexEntries);
      indexEntries = NULL;
    }

    if (verbose)
      printf("Fetching %s\n", bundleFile);
    if (mport_fetch_bundle(mport, bundleFile) != MPORT_OK) {
      fprintf(stderr, "%s\n", mport_err_string());
      exit(mport_err_code());
    }

    free(bundleFile);
  }

  mport_instance_free(mport); 
  
  return 0;
}


static void 
usage() 
{
  fprintf(stderr, "Usage: mport.fetch <package name>\n");
  exit(2);
}
