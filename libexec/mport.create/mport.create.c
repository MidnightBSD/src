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
 * $MidnightBSD: src/libexec/mport.create/mport.create.c,v 1.1 2007/09/23 22:32:13 ctriv Exp $
 */



#include <sys/cdefs.h>
__MBSDID("$MidnightBSD: src/libexec/mport.create/mport.create.c,v 1.1 2007/09/23 22:32:13 ctriv Exp $");


#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <mport.h>


#define STRING_EQ(s1, s2) (strcmp((s1), (s2)) == 0)

static void usage(void);

int main(int argc, char *argv[]) 
{
  int ch;
  mportPackageMeta *pack = mport_new_packagemeta();
  mportPlist *plist = mport_new_plist();
  FILE *fp;
    
  while ((ch = getopt(argc, argv, "o:n:v:c:l:s:d:p:P:D:M:O:C:i:j:m:r:")) != -1) {
    switch (ch) {
      case 'o':
        pack->pkg_filename = optarg;
        break;
      case 'n':
        pack->name = optarg;
        break;
      case 'v':
        pack->version = optarg;
        break;
      case 'c':
        pack->comment = optarg;
        break;
      case 'l':
        pack->lang = optarg;
        break;
      case 's':
        pack->sourcedir = optarg;
        break;
      case 'd':
        pack->desc = optarg;
        break;
      case 'p':
        if ((fp = fopen(optarg, "r")) == NULL) {
          err(1, "%s", optarg);
        }
        if (mport_parse_plist_file(fp, plist) != 0) {
          fprintf(stderr, "Could not parse plist file '%s'.\n", optarg);
          exit(1);
        }
        
        break;
      case 'P':
        pack->prefix = optarg;
        break;
      case 'D':
        mport_parselist(optarg, &(pack->depends));
        break;
      case 'M':
        pack->mtree = optarg;
        break;
      case 'O':
        pack->origin = optarg;
        break;
      case 'C':
        mport_parselist(optarg, &(pack->conflicts));
        break;
      case 'i':
        pack->pkginstall = optarg;
        break;
      case 'j':
        pack->pkgdeinstall = optarg;
        break;
      case 'm':
        pack->pkgmessage = optarg;
        break;
      case 'r':
        pack->req_script = optarg;
        break;
      default:
        usage();
        break; 
    }
  } 
  
  
  // XXX Check that we have all the required args.
  
  if (mport_create_pkg(plist, pack) != MPORT_OK) {
    fprintf(stderr, "mport.create: error: %s\n", mport_err_string());
    return 1;
  }
  
  return 0;
}
  

static void usage() 
{
  fprintf(stderr, "Coming soon: usage!\n");
  exit(1);
}
