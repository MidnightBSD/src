/*-
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
 * $MidnightBSD: src/libexec/mport.check-fake/mport.check-fake.c,v 1.1 2008/04/26 18:00:39 ctriv Exp $
 */



#include <sys/cdefs.h>
__MBSDID("$MidnightBSD: src/libexec/mport.check-fake/mport.check-fake.c,v 1.1 2008/04/26 18:00:39 ctriv Exp $");


#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <mport.h>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <regex.h>

#ifdef PRINT_DIAG
#define DIAG(fmt, ...) warnx(fmt, ##__VA_ARGS__);
#else
#define DIAG(fmt, ...) 
#endif

static void usage(void);
static int check_fake(mportAssetList *, const char *, const char *, const char *);
static int grep_file(const char *, const char *);


int main(int argc, char *argv[]) 
{
  int ch, ret;
  const char *skip = NULL, *prefix = NULL, *destdir = NULL, *assetlistfile = NULL;
  mportAssetList *assetlist;
  FILE *fp;

  while ((ch = getopt(argc, argv, "f:d:s:p:")) != -1) {
    switch (ch) {
      case 's':
        skip = optarg;
        break;
      case 'p':
        prefix = optarg;
        break;
      case 'd':
        destdir = optarg;
        break;
      case 'f':
        assetlistfile = optarg;
        break;
      case '?':
      default:
        usage();
        break; 
    }
  } 

  argc -= optind;
  argv += optind;

  DIAG("assetlist = %s; destdir = %s; prefix = %s; skip = %s", assetlistfile, destdir, prefix, skip)

  if (!prefix || !destdir || !assetlistfile) 
    usage();
  
  if ((fp = fopen(assetlistfile, "r")) == NULL)
    err(EX_NOINPUT, "Could not open assetlist file %s", assetlistfile);
      
  if ((assetlist = mport_assetlist_new()) == NULL) 
    err(EX_OSERR, "Could not not allocate assetlist");
  
  if (mport_parse_plistfile(fp, assetlist) != MPORT_OK)
    err(EX_DATAERR, "Invalid assetlist");

  DIAG("running check_fake")
  
  printf("Checking %s\n", destdir);
  ret = check_fake(assetlist, destdir, prefix, skip);
  
  if (ret == 0) {
    printf("Fake succeeded.\n");
  } else {
    printf("Fake failed.\n");
  }
  
  mport_assetlist_free(assetlist);
  
  return ret;
}

static int check_fake(mportAssetList *assetlist, const char *destdir, const char *prefix, const char *skip)
{
  mportAssetListEntry *e;
  char cwd[FILENAME_MAX], file[FILENAME_MAX];
  char *anchored_skip;
  struct stat st;
  regex_t skipre;
  int ret = 0;

  DIAG("checking skip: %s", skip)
    
  if (skip != NULL) {
    DIAG("Compiling skip: %s", skip)
    
    if (asprintf(&anchored_skip, "^%s$", skip) == -1)
      err(EX_OSERR, "Could not build skip regex");
    
    if (regcomp(&skipre, anchored_skip, REG_EXTENDED|REG_NOSUB) != 0)
      errx(EX_DATAERR, "Could not compile skip regex");
      
     
    free(anchored_skip);  
  }
  
  DIAG("Coping prefix (%s) to cwd", prefix)
  
  (void)strlcpy(cwd, prefix, FILENAME_MAX);

  DIAG("Starting loop, cwd: %s", cwd)
  
  STAILQ_FOREACH(e, assetlist, next) {
    if (e->type == ASSET_CWD) {
        if (e->data == NULL) {
          DIAG("Setting cwd to '%s'", prefix)
          (void)strlcpy(cwd, prefix, FILENAME_MAX);
        } else {
          DIAG("Setting cwd to '%s'", e->data)
          (void)strlcpy(cwd, e->data, FILENAME_MAX);
        }
        
        break;
    }
    
    if (e->type != ASSET_FILE)
      continue;
    
    (void)snprintf(file, FILENAME_MAX, "%s%s/%s", destdir, cwd, e->data);

    DIAG("checking %s", file)
      
    if (lstat(file, &st) != 0) {
      (void)snprintf(file, FILENAME_MAX, "%s/%s", cwd, e->data);
      
      if (lstat(file, &st) == 0) {
        (void)printf("    %s installed in %s\n", e->data, cwd);
      } else {
        (void)printf("    %s not installed.\n", e->data);
      }
      
      ret = 1;
      continue;
    }
    
    if (S_ISLNK(st.st_mode))
      continue;  /* skip symlinks */


    /* if file matches skip continue */
    if (skip != NULL && (regexec(&skipre, e->data, 0, NULL, 0) == 0))
      continue;      
    
    
    DIAG("==> Grepping %s", file)
    /* grep file for fake destdir */
    if (grep_file(file, destdir)) {
      (void)printf("    %s contains the fake destdir\n", e->data);
      ret = 1;
    }
  }
  
  if (skip != NULL)
    regfree(&skipre);
  
  return ret;
} 
      

static int grep_file(const char *filename, const char *destdir)
{
  FILE *file;
  char *line, *nline;
  static regex_t regex;
  static int compiled = 0;
  size_t len;
  int ret = 0;
  
  DIAG("===> Compiling destdir: %s", destdir)
  
  /* Should we cache the compiled regex? */
  if (!compiled) {
    if (regcomp(&regex, destdir, REG_EXTENDED|REG_NOSUB) != 0)
      errx(EX_DATAERR, "Could not compile destdir regex");
    compiled++;
  }
  
  if ((file = fopen(filename, "r")) == NULL)
    err(EX_SOFTWARE, "Couldn't open %s", filename);
    
  while ((line = fgetln(file, &len)) != NULL) {
    /* if we end in \n just switch it to \0, otherwise we need more mem */
    if (line[len - 1] == '\n') {
      line[len - 1] = '\0';
    } else {
      nline = (char *)malloc((len + 1) * sizeof(char));
      if (nline == NULL)
        err(EX_OSERR, "Couldn't allocate nline buf");
      
      memcpy(nline, line, len);
      nline[len] = '\0';
      nline = line;
    }
    
    if (regexec(&regex, line, 0, NULL, 0) == 0) {
      DIAG("===> Match line: %s", line);
      ret = 1;      
      break;
    }
  }
  
  if (ferror(file) != 0)
    err(EX_IOERR, "Error reading %s", filename);
  
  fclose(file);
  return ret;
}
      
static void usage() 
{
  errx(EX_USAGE, "Usage: mport.delete [-s skip] <-f plistfile> <-d destdir> <-p prefix>");
  exit(EX_USAGE);
}


