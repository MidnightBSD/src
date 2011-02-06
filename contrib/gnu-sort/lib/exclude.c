/* exclude.c -- exclude file names

   Copyright (C) 1992, 1993, 1994, 1997, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.
   If not, write to the Free Software Foundation,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Written by Paul Eggert <eggert@twinsun.com>  */

#include <config.h>

#include <stdbool.h>

#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "exclude.h"
#include "fnmatch.h"
#include "xalloc.h"
#include "verify.h"

#if USE_UNLOCKED_IO
# include "unlocked-io.h"
#endif

/* Non-GNU systems lack these options, so we don't need to check them.  */
#ifndef FNM_CASEFOLD
# define FNM_CASEFOLD 0
#endif
#ifndef FNM_EXTMATCH
# define FNM_EXTMATCH 0
#endif
#ifndef FNM_LEADING_DIR
# define FNM_LEADING_DIR 0
#endif

verify (((EXCLUDE_ANCHORED | EXCLUDE_INCLUDE | EXCLUDE_WILDCARDS)
	 & (FNM_PATHNAME | FNM_NOESCAPE | FNM_PERIOD | FNM_LEADING_DIR
	    | FNM_CASEFOLD | FNM_EXTMATCH))
	== 0);

/* An exclude pattern-options pair.  The options are fnmatch options
   ORed with EXCLUDE_* options.  */

struct patopts
  {
    char const *pattern;
    int options;
  };

/* An exclude list, of pattern-options pairs.  */

struct exclude
  {
    struct patopts *exclude;
    size_t exclude_alloc;
    size_t exclude_count;
  };

/* Return a newly allocated and empty exclude list.  */

struct exclude *
new_exclude (void)
{
  return xzalloc (sizeof *new_exclude ());
}

/* Free the storage associated with an exclude list.  */

void
free_exclude (struct exclude *ex)
{
  free (ex->exclude);
  free (ex);
}

/* Return zero if PATTERN matches F, obeying OPTIONS, except that
   (unlike fnmatch) wildcards are disabled in PATTERN.  */

static int
fnmatch_no_wildcards (char const *pattern, char const *f, int options)
{
  if (! (options & FNM_LEADING_DIR))
    return ((options & FNM_CASEFOLD)
	    ? mbscasecmp (pattern, f)
	    : strcmp (pattern, f));
  else if (! (options & FNM_CASEFOLD))
    {
      size_t patlen = strlen (pattern);
      int r = strncmp (pattern, f, patlen);
      if (! r)
	{
	  r = f[patlen];
	  if (r == '/')
	    r = 0;
	}
      return r;
    }
  else
    {
      /* Walk through a copy of F, seeing whether P matches any prefix
	 of F.

	 FIXME: This is an O(N**2) algorithm; it should be O(N).
	 Also, the copy should not be necessary.  However, fixing this
	 will probably involve a change to the mbs* API.  */

      char *fcopy = xstrdup (f);
      char *p;
      int r;
      for (p = fcopy; ; *p++ = '/')
	{
	  p = strchr (p, '/');
	  if (p)
	    *p = '\0';
	  r = mbscasecmp (pattern, fcopy);
	  if (!p || r <= 0)
	    break;
	}
      free (fcopy);
      return r;
    }
}

bool
exclude_fnmatch (char const *pattern, char const *f, int options)
{
  int (*matcher) (char const *, char const *, int) =
    (options & EXCLUDE_WILDCARDS
     ? fnmatch
     : fnmatch_no_wildcards);
  bool matched = ((*matcher) (pattern, f, options) == 0);
  char const *p;

  if (! (options & EXCLUDE_ANCHORED))
    for (p = f; *p && ! matched; p++)
      if (*p == '/' && p[1] != '/')
	matched = ((*matcher) (pattern, p + 1, options) == 0);

  return matched;
}

/* Return true if EX excludes F.  */

bool
excluded_file_name (struct exclude const *ex, char const *f)
{
  size_t exclude_count = ex->exclude_count;

  /* If no options are given, the default is to include.  */
  if (exclude_count == 0)
    return false;
  else
    {
      struct patopts const *exclude = ex->exclude;
      size_t i;

      /* Otherwise, the default is the opposite of the first option.  */
      bool excluded = !! (exclude[0].options & EXCLUDE_INCLUDE);

      /* Scan through the options, seeing whether they change F from
	 excluded to included or vice versa.  */
      for (i = 0;  i < exclude_count;  i++)
	{
	  char const *pattern = exclude[i].pattern;
	  int options = exclude[i].options;
	  if (excluded == !! (options & EXCLUDE_INCLUDE))
	    excluded ^= exclude_fnmatch (pattern, f, options);
	}

      return excluded;
    }
}

/* Append to EX the exclusion PATTERN with OPTIONS.  */

void
add_exclude (struct exclude *ex, char const *pattern, int options)
{
  struct patopts *patopts;

  if (ex->exclude_count == ex->exclude_alloc)
    ex->exclude = x2nrealloc (ex->exclude, &ex->exclude_alloc,
			      sizeof *ex->exclude);

  patopts = &ex->exclude[ex->exclude_count++];
  patopts->pattern = pattern;
  patopts->options = options;
}

/* Use ADD_FUNC to append to EX the patterns in FILE_NAME, each with
   OPTIONS.  LINE_END terminates each pattern in the file.  If
   LINE_END is a space character, ignore trailing spaces and empty
   lines in FILE.  Return -1 on failure, 0 on success.  */

int
add_exclude_file (void (*add_func) (struct exclude *, char const *, int),
		  struct exclude *ex, char const *file_name, int options,
		  char line_end)
{
  bool use_stdin = file_name[0] == '-' && !file_name[1];
  FILE *in;
  char *buf = NULL;
  char *p;
  char const *pattern;
  char const *lim;
  size_t buf_alloc = 0;
  size_t buf_count = 0;
  int c;
  int e = 0;

  if (use_stdin)
    in = stdin;
  else if (! (in = fopen (file_name, "r")))
    return -1;

  while ((c = getc (in)) != EOF)
    {
      if (buf_count == buf_alloc)
	buf = x2realloc (buf, &buf_alloc);
      buf[buf_count++] = c;
    }

  if (ferror (in))
    e = errno;

  if (!use_stdin && fclose (in) != 0)
    e = errno;

  buf = xrealloc (buf, buf_count + 1);
  buf[buf_count] = line_end;
  lim = buf + buf_count + ! (buf_count == 0 || buf[buf_count - 1] == line_end);
  pattern = buf;

  for (p = buf; p < lim; p++)
    if (*p == line_end)
      {
	char *pattern_end = p;

	if (isspace ((unsigned char) line_end))
	  {
	    for (; ; pattern_end--)
	      if (pattern_end == pattern)
		goto next_pattern;
	      else if (! isspace ((unsigned char) pattern_end[-1]))
		break;
	  }

	*pattern_end = '\0';
	(*add_func) (ex, pattern, options);

      next_pattern:
	pattern = p + 1;
      }

  errno = e;
  return e ? -1 : 0;
}
