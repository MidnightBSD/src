/* modechange.h -- definitions for file mode manipulation

   Copyright (C) 1989, 1990, 1997, 2003, 2004, 2005, 2006 Free
   Software Foundation, Inc.

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

#if ! defined MODECHANGE_H_
# define MODECHANGE_H_

# include <stdbool.h>
# include <sys/types.h>

struct mode_change *mode_compile (const char *);
struct mode_change *mode_create_from_ref (const char *);
mode_t mode_adjust (mode_t, bool, mode_t, struct mode_change const *,
		    mode_t *);

#endif
