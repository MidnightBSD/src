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
 * $MidnightBSD: src/libexec/mport.create/mport.create.c,v 1.3 2007/09/27 23:09:12 ctriv Exp $
 */



#include <sys/cdefs.h>
__MBSDID("$MidnightBSD: src/libexec/mport.create/mport.create.c,v 1.3 2007/09/27 23:09:12 ctriv Exp $");


#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <mport.h>


static void usage(void);

int main(int argc, char *argv[]) 
{
  int ch, force;
  mportInstance *mport;
  mportPackageMeta **packs;
  const char *arg, *where = NULL;
  force = 0;
    
  while ((ch = getopt(argc, argv, "fo:n:")) != -1) {
    switch (ch) {
      case 'f':
        force = 1;
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

  if (arg == NULL)
    usage();

  mport = mport_instance_new();
  
  if (mport_instance_init(mport, NULL) != MPORT_OK) {
    warnx("%s", mport_err_string());
    exit(1);
  }

  if (mport_get_meta_from_master(mport, &packs, where, arg) != MPORT_OK) {
    warnx("%s", mport_err_string());
    exit(1);
  }
  
  while (*packs != NULL) {
    if (mport_delete_primative(mport, *packs, force) != MPORT_OK) {
      warnx("%s", mport_err_string());
      exit(1);
    }
    packs++;
  }

  mport_instance_free(mport); 
  
  return 0;
}


static void usage() 
{
  fprintf(stderr, "Usage: mport.delete [-f] -n pkgname\n");
  fprintf(stderr, "Usage: mport.delete [-f] -o origin\n");
  exit(2);
}
