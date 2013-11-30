/* exclude.h -- declarations for excluding file names

   Copyright (C) 1992-1994, 1997, 1999, 2001-2003, 2005-2006, 2009-2011 Free
   Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef _GL_EXCLUDE_H
#define _GL_EXCLUDE_H 1

#include <stdbool.h>

/* Written by Paul Eggert <eggert@twinsun.com>
   and Sergey Poznyakoff <gray@gnu.org> */

/* Exclude options, which can be ORed with fnmatch options.  */

/* Patterns must match the start of file names, instead of matching
   anywhere after a '/'.  */
#define EXCLUDE_ANCHORED (1 << 30)

/* Include instead of exclude.  */
#define EXCLUDE_INCLUDE (1 << 29)

/* '?', '*', '[', and '\\' are special in patterns.  Without this
   option, these characters are ordinary and fnmatch is not used.  */
#define EXCLUDE_WILDCARDS (1 << 28)

struct exclude;

bool fnmatch_pattern_has_wildcards (const char *, int);

struct exclude *new_exclude (void);
void free_exclude (struct exclude *);
void add_exclude (struct exclude *, char const *, int);
int add_exclude_file (void (*) (struct exclude *, char const *, int),
                      struct exclude *, char const *, int, char);
bool excluded_file_name (struct exclude const *, char const *);
bool exclude_fnmatch (char const *pattern, char const *f, int options);

#endif /* _GL_EXCLUDE_H */
