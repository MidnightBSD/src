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
 * $MidnightBSD: src/lib/libmport/util.c,v 1.5 2007/09/27 03:24:38 ctriv Exp $
 */


#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "mport.h"

__MBSDID("$MidnightBSD: src/lib/libmport/util.c,v 1.5 2007/09/27 03:24:38 ctriv Exp $");

/* Package meta-data creation and destruction */
mportPackageMeta* mport_new_packagemeta() 
{
  /* we use calloc so any pointers that aren't set are NULL.
     (calloc zero's out the memory region. */
  return (mportPackageMeta *)calloc(1, sizeof(mportPackageMeta));
}

void mport_free_packagemeta(mportPackageMeta *pack)
{
  free(pack->pkg_filename);
  free(pack->comment);
  free(pack->sourcedir);
  free(pack->desc);
  free(pack->prefix);
  free(*(pack->depends));
  free(pack->depends);
  free(pack->mtree);
  free(pack->origin);
  free(*(pack->conflicts));
  free(pack->conflicts);
  free(pack->pkginstall);
  free(pack->pkgdeinstall);
  free(pack->pkgmessage);
  free(pack);
}


/* deletes the entire directory tree at name.
 * think rm -r filename
 */
int mport_rmtree(const char *filename) 
{
  return mport_xsystem("/bin/rm -r %s", filename);
}  




/*
 * Copy fromname to toname 
 *
 */
int mport_copy_file(const char *fromname, const char *toname)
{
  return mport_xsystem("/bin/cp %s %s", fromname, toname);
}


/*
 * Quick test to see if a file exists.
 */
int mport_file_exists(const char *file) 
{
  struct stat st;
  
  return (lstat(file, &st) == 0);
}
  

/* mport_xsystem(char *fmt, ...)
 * 
 * Our own version on system that takes a format string and a list 
 * of values.  The fmt works exactly like the stdio output formats.
 */
int mport_xsystem(const char *fmt, ...) 
{
  va_list args;
  char *cmnd;
  int ret;
  
  va_start(args, fmt);
  if (vasprintf(&cmnd, fmt, args) == -1) {
    /* XXX How will the caller know this is no mem, and not a failed exec? */
    return MPORT_ERR_NO_MEM;
  }
  
  ret = system(cmnd);
  
  free(cmnd);
  va_end(args);
  
  return ret;
}
  



/*
 * mport_parselist(input, string_array_pointer)
 *
 * hacks input into sub strings by whitespace.  Allocates a chunk or memory
 * for a array of those strings, and sets the pointer you pass to refernce
 * that memory
 *
 * char input[] = "foo bar baz"
 * char **list;
 * 
 * mport_parselist(input, &list);
 * list = {"foo", "bar", "baz"};
 */
void mport_parselist(char *opt, char ***list) 
{
  int len;
  char *input;
  char *field;

  input = (char *)malloc(strlen(opt) + 1);
  strlcpy(input, opt, strlen(opt) + 1);
  
  /* first we need to get the length of the depends list */
  for (len = 0; (field = strsep(&opt, " \t\n")) != NULL;) {
    if (*field != '\0')
      len++;
  }    

  *list = (char **)malloc((len + 1) * sizeof(char *));

  if (len == 0) {
    **list = NULL;
    return;
  }

  /* dereference once so we don't loose our minds. */
  char **vec = *list;
  
  while ((field = strsep(&input, " \t\n")) != NULL) {
    if (*field == '\0')
      continue;

    *vec = field;
    vec++;
  }

  *vec = NULL;
}


