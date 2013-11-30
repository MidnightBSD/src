/* Copyright (C) 1996 Free Software Foundation, Inc.
This file is part of GNU Fortran libU77 library.

This library is free software; you can redistribute it and/or modify it
under the terms of the GNU Library General Public License as published
by the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GNU Fortran is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with GNU Fortran; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include "f2c.h"
#include "fio.h"

integer
G77_fgetc_0 (const integer * lunit, char *c, ftnlen Lc)
{
  int err;
  FILE *f = f__units[*lunit].ufd;

  if (*lunit >= MXUNIT || *lunit < 0)
    return 101;			/* bad unit error */
  err = getc (f);
  if (err == EOF)
    {
      if (feof (f))
	return -1;
      else
	return ferror (f);
    }
  else
    {
      if (Lc == 0)
	return 0;

      c[0] = err;
      while (--Lc)
	*++c = ' ';
      return 0;
    }
}

integer
G77_fget_0 (char *c, const ftnlen Lc)
{
  integer five = 5;

  return G77_fgetc_0 (&five, c, Lc);
}
