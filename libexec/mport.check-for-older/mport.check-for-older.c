/*-
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
 *
 * $MidnightBSD: src/libexec/mport.delete/mport.delete.c,v 1.2 2008/01/05 22:29:14 ctriv Exp $
 */



#include <sys/cdefs.h>
__MBSDID("$MidnightBSD: src/libexec/mport.delete/mport.delete.c,v 1.2 2008/01/05 22:29:14 ctriv Exp $");


#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <mport.h>


static void usage(void);

int main(int argc, char *argv[]) 
{
  int ch;
  mportInstance *mport;
  mportPackageMeta **packs;
  const char *arg, *where, *version = NULL;

  if (argc == 1)
    usage();
    
  while ((ch = getopt(argc, argv, "v:o:n:")) != -1) {
    switch (ch) {
      case 'v':
        version = optarg;
        break;
      case 'o':
        where = "origin=%Q";
        arg   = optarg;
        break;
      case 'n':
        where = "pkg=%Q";
        arg   = optarg;
        break;
      case '?':
      default:
        usage();
        break; 
    }
  } 

  argc -= optind;
  argv += optind;

  if (arg == NULL || version == NULL)
    usage();

  mport = mport_instance_new();
  
  if (mport_instance_init(mport, NULL) != MPORT_OK) {
    warnx("%s", mport_err_string());
    exit(1);
  }

  if (mport_pkgmeta_search_master(mport, &packs, where, arg) != MPORT_OK) {
    warnx("%s", mport_err_string());
    exit(1);
  }
  
  if (packs == NULL) {
    (void)printf("No packages installed matching '%s'\n", arg);
    exit(1);
  }
  

  if (packs[1] != NULL) {
    warnx("Ambiguous package identifier: %s", arg);
    exit(3);
  }
  
  if (mport_version_cmp(packs[0]->version, version) >= 0) {
    (void)printf("%s is installed, but installed version (%s) is not older than port (%s).\n", packs[0]->name, packs[0]->version, version);
    exit(1);
  }
  
  mport_instance_free(mport); 
  
  return 0;
}


static void usage() 
{
  fprintf(stderr, "Usage: mport.check-for-older -n pkgname -v newversion \n");
  fprintf(stderr, "Usage: mport.check-for-older -o origin -v newversion\n");
  exit(2);
}
