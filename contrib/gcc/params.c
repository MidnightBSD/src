/* params.c - Run-time parameters.
   Copyright (C) 2001, 2003 Free Software Foundation, Inc.
   Written by Mark Mitchell <mark@codesourcery.com>.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.

*/

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "params.h"
#include "toplev.h"

/* An array containing the compiler parameters and their current
   values.  */

param_info *compiler_params;

/* The number of entries in the table.  */

static size_t num_compiler_params;

/* Add the N PARAMS to the current list of compiler parameters.  */

void
add_params (const param_info params[], size_t n)
{
  /* Allocate enough space for the new parameters.  */
  compiler_params = xrealloc (compiler_params,
			      (num_compiler_params + n) * sizeof (param_info));
  /* Copy them into the table.  */
  memcpy (compiler_params + num_compiler_params,
	  params,
	  n * sizeof (param_info));
  /* Keep track of how many parameters we have.  */
  num_compiler_params += n;
}

/* Set the VALUE associated with the parameter given by NAME.  */

void
set_param_value (const char *name, int value)
{
  size_t i;

  /* Make sure nobody tries to set a parameter to an invalid value.  */
  if (value == INVALID_PARAM_VAL)
    abort ();

  /* Scan the parameter table to find a matching entry.  */
  for (i = 0; i < num_compiler_params; ++i)
    if (strcmp (compiler_params[i].option, name) == 0)
      {
	compiler_params[i].value = value;
	return;
      }

  /* If we didn't find this parameter, issue an error message.  */
  error ("invalid parameter `%s'", name);
}
