/*-
 * Copyright (c) 2010 Lucas Holt
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
 *
 * $MidnightBSD: src/libexec/mport.list/mport.list.c,v 1.2 2010/03/04 01:12:27 laffer1 Exp $
 */



#include <sys/cdefs.h>
__MBSDID("$MidnightBSD: src/libexec/mport.list/mport.list.c,v 1.2 2010/03/04 01:12:27 laffer1 Exp $");


#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <mport.h>

static void usage(void);
static char * str_remove( const char *str, const char ch );

int main(int argc, char *argv[]) 
{
  int ch;
  mportInstance *mport;
  mportPackageMeta **packs;
  bool quiet = false;
  bool verbose = false;
  char *comment;

  if (argc > 2)
    usage();
    
  while ((ch = getopt(argc, argv, "qv")) != -1) {
    switch (ch) {
      case 'q':
        quiet = true;
        break;
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

  if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
    warnx("%s", mport_err_string());
    exit(1);
  }
  
  if (packs == NULL) {
    if (!quiet)
      warnx("No packages installed matching.");
    exit(3);
  }
  
  while (*packs != NULL) {
    if (verbose) {
      comment = str_remove((*packs)->comment, '\\');
      (void) printf("%s-%s\t%s\n", (*packs)->name, (*packs)->version, comment);
      free(comment);
    }
    else if (quiet)
      (void) printf("%s\n", (*packs)->name);
    else
      (void) printf("%s-%s\n", (*packs)->name, (*packs)->version);
    packs++;
  }

  mport_instance_free(mport); 
  
  return 0;
}


static char * str_remove( const char *str, const char ch )
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


static void usage() 
{
  fprintf(stderr, "Usage: mport.list [-q | -v]\n");
  exit(2);
}
