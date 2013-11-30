/* provide a replacement openat function
   Copyright (C) 2004, 2005, 2006 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* written by Jim Meyering */

#include <config.h>

#include "openat.h"

#include <stdarg.h>
#include <stddef.h>

#include "dirname.h" /* solely for definition of IS_ABSOLUTE_FILE_NAME */
#include "fcntl--.h"
#include "lchown.h"
#include "lstat.h"
#include "openat-priv.h"
#include "save-cwd.h"

/* Replacement for Solaris' openat function.
   <http://www.google.com/search?q=openat+site:docs.sun.com>
   First, try to simulate it via open ("/proc/self/fd/FD/FILE").
   Failing that, simulate it by doing save_cwd/fchdir/open/restore_cwd.
   If either the save_cwd or the restore_cwd fails (relatively unlikely),
   then give a diagnostic and exit nonzero.
   Otherwise, upon failure, set errno and return -1, as openat does.
   Upon successful completion, return a file descriptor.  */
int
openat (int fd, char const *file, int flags, ...)
{
  mode_t mode = 0;

  if (flags & O_CREAT)
    {
      va_list arg;
      va_start (arg, flags);

      /* If mode_t is narrower than int, use the promoted type (int),
         not mode_t.  Use sizeof to guess whether mode_t is narrower;
         we don't know of any practical counterexamples.  */
      mode = (sizeof (mode_t) < sizeof (int)
	      ? va_arg (arg, int)
	      : va_arg (arg, mode_t));

      va_end (arg);
    }

  return openat_permissive (fd, file, flags, mode, NULL);
}

/* Like openat (FD, FILE, FLAGS, MODE), but if CWD_ERRNO is
   nonnull, set *CWD_ERRNO to an errno value if unable to save
   or restore the initial working directory.  This is needed only
   the first time remove.c's remove_dir opens a command-line
   directory argument.

   If a previous attempt to restore the current working directory
   failed, then we must not even try to access a `.'-relative name.
   It is the caller's responsibility not to call this function
   in that case.  */

int
openat_permissive (int fd, char const *file, int flags, mode_t mode,
		   int *cwd_errno)
{
  struct saved_cwd saved_cwd;
  int saved_errno;
  int err;
  bool save_ok;

  if (fd == AT_FDCWD || IS_ABSOLUTE_FILE_NAME (file))
    return open (file, flags, mode);

  {
    char buf[OPENAT_BUFFER_SIZE];
    char *proc_file = openat_proc_name (buf, fd, file);
    if (proc_file)
      {
	int open_result = open (proc_file, flags, mode);
	int open_errno = errno;
	if (proc_file != buf)
	  free (proc_file);
	/* If the syscall succeeds, or if it fails with an unexpected
	   errno value, then return right away.  Otherwise, fall through
	   and resort to using save_cwd/restore_cwd.  */
	if (0 <= open_result || ! EXPECTED_ERRNO (open_errno))
	  {
	    errno = open_errno;
	    return open_result;
	  }
      }
  }

  save_ok = (save_cwd (&saved_cwd) == 0);
  if (! save_ok)
    {
      if (! cwd_errno)
	openat_save_fail (errno);
      *cwd_errno = errno;
    }

  err = fchdir (fd);
  saved_errno = errno;

  if (! err)
    {
      err = open (file, flags, mode);
      saved_errno = errno;
      if (save_ok && restore_cwd (&saved_cwd) != 0)
	{
	  if (! cwd_errno)
	    openat_restore_fail (errno);
	  *cwd_errno = errno;
	}
    }

  free_cwd (&saved_cwd);
  errno = saved_errno;
  return err;
}

/* Return true if our openat implementation must resort to
   using save_cwd and restore_cwd.  */
bool
openat_needs_fchdir (void)
{
  bool needs_fchdir = true;
  int fd = open ("/", O_RDONLY);

  if (0 <= fd)
    {
      char buf[OPENAT_BUFFER_SIZE];
      char *proc_file = openat_proc_name (buf, fd, ".");
      if (proc_file)
	{
	  needs_fchdir = false;
	  if (proc_file != buf)
	    free (proc_file);
	}
      close (fd);
    }

  return needs_fchdir;
}

#if !HAVE_FDOPENDIR

/* Replacement for Solaris' function by the same name.
   <http://www.google.com/search?q=fdopendir+site:docs.sun.com>
   First, try to simulate it via opendir ("/proc/self/fd/FD").  Failing
   that, simulate it by doing save_cwd/fchdir/opendir(".")/restore_cwd.
   If either the save_cwd or the restore_cwd fails (relatively unlikely),
   then give a diagnostic and exit nonzero.
   Otherwise, this function works just like Solaris' fdopendir.

   W A R N I N G:
   Unlike the other fd-related functions here, this one
   effectively consumes its FD parameter.  The caller should not
   close or otherwise manipulate FD if this function returns successfully.  */
DIR *
fdopendir (int fd)
{
  struct saved_cwd saved_cwd;
  int saved_errno;
  DIR *dir;

  char buf[OPENAT_BUFFER_SIZE];
  char *proc_file = openat_proc_name (buf, fd, ".");
  if (proc_file)
    {
      dir = opendir (proc_file);
      saved_errno = errno;
    }
  else
    {
      dir = NULL;
      saved_errno = EOPNOTSUPP;
    }

  /* If the syscall fails with an expected errno value, resort to
     save_cwd/restore_cwd.  */
  if (! dir && EXPECTED_ERRNO (saved_errno))
    {
      if (save_cwd (&saved_cwd) != 0)
	openat_save_fail (errno);

      if (fchdir (fd) != 0)
	{
	  dir = NULL;
	  saved_errno = errno;
	}
      else
	{
	  dir = opendir (".");
	  saved_errno = errno;

	  if (restore_cwd (&saved_cwd) != 0)
	    openat_restore_fail (errno);
	}

      free_cwd (&saved_cwd);
    }

  if (dir)
    close (fd);
  if (proc_file != buf)
    free (proc_file);
  errno = saved_errno;
  return dir;
}

#endif

/* Replacement for Solaris' function by the same name.
   <http://www.google.com/search?q=fstatat+site:docs.sun.com>
   First, try to simulate it via l?stat ("/proc/self/fd/FD/FILE").
   Failing that, simulate it via save_cwd/fchdir/(stat|lstat)/restore_cwd.
   If either the save_cwd or the restore_cwd fails (relatively unlikely),
   then give a diagnostic and exit nonzero.
   Otherwise, this function works just like Solaris' fstatat.  */

#define AT_FUNC_NAME fstatat
#define AT_FUNC_F1 lstat
#define AT_FUNC_F2 stat
#define AT_FUNC_USE_F1_COND flag == AT_SYMLINK_NOFOLLOW
#define AT_FUNC_POST_FILE_PARAM_DECLS , struct stat *st, int flag
#define AT_FUNC_POST_FILE_ARGS        , st
#include "at-func.c"
#undef AT_FUNC_NAME
#undef AT_FUNC_F1
#undef AT_FUNC_F2
#undef AT_FUNC_USE_F1_COND
#undef AT_FUNC_POST_FILE_PARAM_DECLS
#undef AT_FUNC_POST_FILE_ARGS

/* Replacement for Solaris' function by the same name.
   <http://www.google.com/search?q=unlinkat+site:docs.sun.com>
   First, try to simulate it via (unlink|rmdir) ("/proc/self/fd/FD/FILE").
   Failing that, simulate it via save_cwd/fchdir/(unlink|rmdir)/restore_cwd.
   If either the save_cwd or the restore_cwd fails (relatively unlikely),
   then give a diagnostic and exit nonzero.
   Otherwise, this function works just like Solaris' unlinkat.  */

#define AT_FUNC_NAME unlinkat
#define AT_FUNC_F1 rmdir
#define AT_FUNC_F2 unlink
#define AT_FUNC_USE_F1_COND flag == AT_REMOVEDIR
#define AT_FUNC_POST_FILE_PARAM_DECLS , int flag
#define AT_FUNC_POST_FILE_ARGS        /* empty */
#include "at-func.c"
#undef AT_FUNC_NAME
#undef AT_FUNC_F1
#undef AT_FUNC_F2
#undef AT_FUNC_USE_F1_COND
#undef AT_FUNC_POST_FILE_PARAM_DECLS
#undef AT_FUNC_POST_FILE_ARGS
