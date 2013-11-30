/* Copyright (C) 1998, 1999, 2001, 2005, 2006, 2007 Free Software Foundation, Inc.
   This file is derived from the one in the GNU C Library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#if !_LIBC
# include <config.h>
#endif

#include <stdlib.h>

#if !_LIBC
# include "tempname.h"
# define __gen_tempname gen_tempname
# define __GT_FILE GT_FILE
#endif

#include <stdio.h>

#ifndef __GT_FILE
# define __GT_FILE 0
#endif

/* Generate a unique temporary file name from TEMPLATE.
   The last six characters of TEMPLATE must be "XXXXXX";
   they are replaced with a string that makes the file name unique.
   Then open the file and return a fd. */
int
mkstemp (template)
     char *template;
{
  return __gen_tempname (template, __GT_FILE);
}
