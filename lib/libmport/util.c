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
 * $MidnightBSD: src/lib/libmport/util.c,v 1.9 2008/01/05 22:18:20 ctriv Exp $
 */


#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include "mport.h"

/* Package meta-data creation and destruction */
mportPackageMeta* mport_packagemeta_new() 
{
  /* we use calloc so any pointers that aren't set are NULL.
     (calloc zero's out the memory region. */
  return (mportPackageMeta *)calloc(1, sizeof(mportPackageMeta));
}

void mport_packagemeta_free(mportPackageMeta *pack)
{
  free(pack->pkg_filename);
  free(pack->comment);
  free(pack->sourcedir);
  free(pack->desc);
  free(pack->prefix);
  free(pack->depends);
  free(pack->mtree);
  free(pack->origin);
  free(pack->conflicts);
  free(pack->pkginstall);
  free(pack->pkgdeinstall);
  free(pack->pkgmessage);
  
  if (pack->conflicts != NULL)
    free(*(pack->conflicts));
  
  if (pack->depends != NULL)
    free(*(pack->depends));

  free(pack);
}

/* free a vector of mportPackageMeta pointers */
void mport_packagemeta_vec_free(mportPackageMeta **vec)
{
  int i;
  for (i=0; *(vec + i) != NULL; i++) {
    mport_packagemeta_free(*(vec + i));
  }
  
  free(vec);
}

/* a wrapper around chdir, to work with our error system */
int mport_chdir(mportInstance *mport, const char *dir)
{
  if (mport != NULL) {
    char *finaldir;
  
    asprintf(&finaldir, "%s%s", mport->root, dir);
  
    if (finaldir == NULL)
      RETURN_ERROR(MPORT_ERR_NO_MEM, "Couldn't building root'ed dir");
    
    if (chdir(finaldir) != 0) {
      free(finaldir);
      RETURN_ERRORX(MPORT_ERR_SYSCALL_FAILED, "Couldn't chdir to %s: %s", finaldir, strerror(errno));
    }
  
    free(finaldir);
  } else {
    if (chdir(dir) != 0) 
      RETURN_ERRORX(MPORT_ERR_SYSCALL_FAILED, "Couldn't chdir to %s: %s", dir, strerror(errno));
  }
  
  return MPORT_OK;
}    


/* deletes the entire directory tree at name.
 * think rm -r filename
 */
int mport_rmtree(const char *filename) 
{
  return mport_xsystem(NULL, "/bin/rm -r %s", filename);
}  


/*
 * Copy fromname to toname 
 *
 */
int mport_copy_file(const char *fromname, const char *toname)
{
  return mport_xsystem(NULL, "/bin/cp %s %s", fromname, toname);
}



/* 
 * create a directory with mode 755.  Do not fail if the
 * directory exists already.
 */
int mport_mkdir(const char *dir) 
{
  if (mkdir(dir, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) != 0) {
    if (errno != EEXIST) 
      RETURN_ERRORX(MPORT_ERR_SYSCALL_FAILED, "Couldn't mkdir %s: %s", dir, strerror(errno));
  }
  
  return MPORT_OK;
}


/*
 * mport_rmdir(dir, ignore_nonempty)
 *
 * delete the given directory.  If ignore_nonempty is non-zero, then
 * we return OK even if we couldn't delete the dir because it wasn't empty or
 * didn't exist.
 */
int mport_rmdir(const char *dir, int ignore_nonempty)
{
  if (rmdir(dir) != 0) {
    if (ignore_nonempty && (errno == ENOTEMPTY || errno == ENOENT)) {
      return MPORT_OK;
    } else {
      RETURN_ERRORX(MPORT_ERR_SYSCALL_FAILED, "Couldn't rmdir %s: %s", dir, strerror(errno));
    }
  } 
  
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


/* mport_xsystem(mportInstance *mport, char *fmt, ...)
 * 
 * Our own version on system that takes a format string and a list 
 * of values.  The fmt works exactly like the stdio output formats.
 * 
 * If mport is non-NULL and has a root set, your command will run 
 * chroot'ed into mport->root.
 */
int mport_xsystem(mportInstance *mport, const char *fmt, ...) 
{
  va_list args;
  char *cmnd;
  int ret;
  
  va_start(args, fmt);
  
  if (vasprintf(&cmnd, fmt, args) == -1) {
    /* XXX How will the caller know this is no mem, and not a failed exec? */
    va_end(args);
    RETURN_ERROR(MPORT_ERR_NO_MEM, "Couldn't allocate xsystem cmnd string.");
  }
  va_end(args);
  
  if (mport != NULL && *(mport->root) != '\0') {
    char *chroot_cmd;
    if (asprintf(&chroot_cmd, "%s %s %s", MPORT_CHROOT_BIN, mport->root, cmnd) == -1)
      RETURN_ERROR(MPORT_ERR_NO_MEM, "Couldn't allocate xsystem chroot string.");
  
    free(cmnd);
    cmnd = chroot_cmd;
  }
    
  ret = system(cmnd);
  
  free(cmnd);
  
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

/*
 * mport_run_plist_exec(fmt, cwd, last_file)
 * 
 * handles a @exec or a @unexec directive in a plist.  This function
 * does the substitions and then runs the command.  last_file is 
 * absolute path.
 *
 * Substitutions:
 * %F	The last filename extracted (last_file argument)
 * %D	The current working directory (cwd)
 * %B	Return the directory part ("dirname") of %D/%F
 * %f	Return the filename part of ("basename") %D/%F
 */
int mport_run_plist_exec(mportInstance *mport, const char *fmt, const char *cwd, const char *last_file) 
{
  size_t l;
  size_t max = FILENAME_MAX * 2;
  char cmnd[max];
  char *pos = cmnd;
  char *name;

  while (*fmt && max > 0) {
    if (*fmt == '%') {
      fmt++;
      switch (*fmt) {
        case 'F':
          /* last_file is absolute, so we skip the cwd at the begining */
          (void)strlcpy(pos, last_file + strlen(cwd) + 1, max);
          l = strlen(last_file + strlen(cwd) + 1);
          pos += l;
          max -= l;
          break;
        case 'D':
          (void)strlcpy(pos, cwd, max);
          l = strlen(cwd);
          pos += l;
          max -= l;
          break;
        case 'B':
          name = dirname(last_file);
          (void)strlcpy(pos, name, max);
          l = strlen(name);
          pos += l;
          max -= l;
          break;
        case 'f':
          name = basename(last_file);
          (void)strlcpy(pos, name, max);
          l = strlen(name);
          pos += l;
          max -= l;
          break;
        default:
          *pos = *fmt;
          max--;
          pos++;
      }
      fmt++;
    } else {
      *pos = *fmt;
      pos++;
      fmt++;
      max--;
    }
  }
  
  *pos = '\0';

  /* cmnd now hold the expaded command, now execute it*/
  return mport_xsystem(mport, cmnd);
}          

