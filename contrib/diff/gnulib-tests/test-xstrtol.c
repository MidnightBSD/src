/* -*- buffer-read-only: t -*- vi: set ro: */
/* DO NOT EDIT! GENERATED AUTOMATICALLY! */
#line 1
/* Test of xstrtol module.
   Copyright (C) 1995, 1996, 1998, 1999, 2000, 2001, 2003, 2004, 2005, 2006,
   2007, 2008, 2009, 2010 Free Software Foundation, Inc.

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

#include <config.h>

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#include "xstrtol.h"
#include "error.h"

#ifndef __xstrtol
# define __xstrtol xstrtol
# define __strtol_t long int
# define __spec "ld"
#endif

char *program_name;

/* Don't show the program name in error messages.  */
static void
print_no_progname (void)
{
}

int
main (int argc, char **argv)
{
  strtol_error s_err;
  int i;

  program_name = argv[0];
  error_print_progname = print_no_progname;

  for (i = 1; i < argc; i++)
    {
      char *p;
      __strtol_t val;

      s_err = __xstrtol (argv[i], &p, 0, &val, "bckMw0");
      if (s_err == LONGINT_OK)
        {
          printf ("%s->%" __spec " (%s)\n", argv[i], val, p);
        }
      else
        {
          xstrtol_fatal (s_err, -2, 'X', NULL, argv[i]);
        }
    }
  exit (0);
}
