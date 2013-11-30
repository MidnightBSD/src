#serial 10
# Check for several getcwd bugs with long file names.
# If so, arrange to compile the wrapper function.

# This is necessary for at least GNU libc on linux-2.4.19 and 2.4.20.
# I've heard that this is due to a Linux kernel bug, and that it has
# been fixed between 2.4.21-pre3 and 2.4.21-pre4.  */

# Copyright (C) 2003, 2004, 2005 Free Software Foundation, Inc.
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# From Jim Meyering

AC_DEFUN([gl_FUNC_GETCWD_PATH_MAX],
[
  AC_CHECK_DECLS_ONCE(getcwd)
  AC_REQUIRE([gl_USE_SYSTEM_EXTENSIONS])
  AC_CACHE_CHECK([whether getcwd handles long file names properly],
    gl_cv_func_getcwd_path_max,
    [# Arrange for deletion of the temporary directory this test creates.
     ac_clean_files="$ac_clean_files confdir3"
     AC_RUN_IFELSE(
       [AC_LANG_SOURCE(
	  [[
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#ifndef AT_FDCWD
# define AT_FDCWD 0
#endif
#ifdef ENAMETOOLONG
# define is_ENAMETOOLONG(x) ((x) == ENAMETOOLONG)
#else
# define is_ENAMETOOLONG(x) 0
#endif

/* Don't get link errors because mkdir is redefined to rpl_mkdir.  */
#undef mkdir

#ifndef S_IRWXU
# define S_IRWXU 0700
#endif

/* The length of this name must be 8.  */
#define DIR_NAME "confdir3"
#define DIR_NAME_LEN 8
#define DIR_NAME_SIZE (DIR_NAME_LEN + 1)

/* The length of "../".  */
#define DOTDOTSLASH_LEN 3

/* Leftover bytes in the buffer, to work around library or OS bugs.  */
#define BUF_SLOP 20

int
main (void)
{
#ifndef PATH_MAX
  /* The Hurd doesn't define this, so getcwd can't exhibit the bug --
     at least not on a local file system.  And if we were to start worrying
     about remote file systems, we'd have to enable the wrapper function
     all of the time, just to be safe.  That's not worth the cost.  */
  exit (0);
#elif ((INT_MAX / (DIR_NAME_SIZE / DOTDOTSLASH_LEN + 1) \
        - DIR_NAME_SIZE - BUF_SLOP) \
       <= PATH_MAX)
  /* FIXME: Assuming there's a system for which this is true,
     this should be done in a compile test.  */
  exit (0);
#else
  char buf[PATH_MAX * (DIR_NAME_SIZE / DOTDOTSLASH_LEN + 1)
	   + DIR_NAME_SIZE + BUF_SLOP];
  char *cwd = getcwd (buf, PATH_MAX);
  size_t initial_cwd_len;
  size_t cwd_len;
  int fail = 0;
  size_t n_chdirs = 0;

  if (cwd == NULL)
    exit (1);

  cwd_len = initial_cwd_len = strlen (cwd);

  while (1)
    {
      size_t dotdot_max = PATH_MAX * (DIR_NAME_SIZE / DOTDOTSLASH_LEN);
      char *c = NULL;

      cwd_len += DIR_NAME_SIZE;
      /* If mkdir or chdir fails, it could be that this system cannot create
	 any file with an absolute name longer than PATH_MAX, such as cygwin.
	 If so, leave fail as 0, because the current working directory can't
	 be too long for getcwd if it can't even be created.  For other
	 errors, be pessimistic and consider that as a failure, too.  */
      if (mkdir (DIR_NAME, S_IRWXU) < 0 || chdir (DIR_NAME) < 0)
	{
	  if (! (errno == ERANGE || is_ENAMETOOLONG (errno)))
	    fail = 2;
	  break;
	}

      if (PATH_MAX <= cwd_len && cwd_len < PATH_MAX + DIR_NAME_SIZE)
	{
	  c = getcwd (buf, PATH_MAX);
	  if (!c && errno == ENOENT)
	    {
	      fail = 1;
	      break;
	    }
	  if (c || ! (errno == ERANGE || is_ENAMETOOLONG (errno)))
	    {
	      fail = 2;
	      break;
	    }
	}

      if (dotdot_max <= cwd_len - initial_cwd_len)
	{
	  if (dotdot_max + DIR_NAME_SIZE < cwd_len - initial_cwd_len)
	    break;
	  c = getcwd (buf, cwd_len + 1);
	  if (!c)
	    {
	      if (! (errno == ERANGE || errno == ENOENT
		     || is_ENAMETOOLONG (errno)))
		{
		  fail = 2;
		  break;
		}
	      if (AT_FDCWD || errno == ERANGE || errno == ENOENT)
		{
		  fail = 1;
		  break;
		}
	    }
	}

      if (c && strlen (c) != cwd_len)
	{
	  fail = 2;
	  break;
	}
      ++n_chdirs;
    }

  /* Leaving behind such a deep directory is not polite.
     So clean up here, right away, even though the driving
     shell script would also clean up.  */
  {
    size_t i;

    /* Unlink first, in case the chdir failed.  */
    unlink (DIR_NAME);
    for (i = 0; i <= n_chdirs; i++)
      {
	if (chdir ("..") < 0)
	  break;
	rmdir (DIR_NAME);
      }
  }

  exit (fail);
#endif
}
          ]])],
    [gl_cv_func_getcwd_path_max=yes],
    [case $? in
     1) gl_cv_func_getcwd_path_max='no, but it is partly working';;
     *) gl_cv_func_getcwd_path_max=no;;
     esac],
    [gl_cv_func_getcwd_path_max=no])
  ])
  case $gl_cv_func_getcwd_path_max in
  no,*)
    AC_DEFINE([HAVE_PARTLY_WORKING_GETCWD], 1,
      [Define to 1 if getcwd works, except it sometimes fails when it shouldn't,
       setting errno to ERANGE, ENAMETOOLONG, or ENOENT.  If __GETCWD_PREFIX
       is not defined, it doesn't matter whether HAVE_PARTLY_WORKING_GETCWD
       is defined.]);;
  esac
])
