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
 * $MidnightBSD: src/lib/libmport/util.c,v 1.2 2007/09/24 06:01:46 ctriv Exp $
 */


#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "mport.h"

__MBSDID("$MidnightBSD: src/lib/libmport/util.c,v 1.2 2007/09/24 06:01:46 ctriv Exp $");

/* Package meta-data creation and destruction */
mportPackageMeta* mport_new_packagemeta() 
{
  return (mportPackageMeta *)malloc(sizeof(mportPackageMeta));
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
  free(pack->req_script);
  free(pack);
}

/*
 *Copy fromname to toname 
 *
 * XXX: This is needs some work.  perms?
 */
int mport_copy_file(const char *fromname, const char *toname)
{
  int to;
  int from;
  int len;
  char buf[8192];
  
  
  if ((from = open(fromname, O_RDONLY)) == -1) {
    RETURN_ERROR(MPORT_ERR_SYSCALL_FAILED, strerror(errno));
  }
  
  if ((to = open(toname, O_WRONLY | O_CREAT)) == -1) {
    RETURN_ERROR(MPORT_ERR_SYSCALL_FAILED, strerror(errno));
  }
  
  len = read(from, buf, sizeof(buf));
  
  while (len > 0) {
    write(to, buf, len);
    len = read(from, buf, sizeof(buf));
  }
  
  close(from);
  close(to);
  
  return MPORT_OK;
}

/*
 * Quick test to see if a file exists.
 */
int mport_file_exists(const char *file) 
{
  struct stat st;
  
  return (lstat(file, &st) == 0);
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

  input = (char *)malloc(strlen(opt));
  strlcpy(input, opt, strlen(opt));
  
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


