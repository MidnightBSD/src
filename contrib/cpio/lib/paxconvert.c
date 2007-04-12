/* Data conversion functions for GNU paxutils

   Copyright (C) 2005 Free Software Foundation, Inc.

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
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <system.h>

/* Format O as a null-terminated decimal string into BUF _backwards_;
   return pointer to start of result.  */
char *
stringify_uintmax_t_backwards (uintmax_t o, char *buf)
{
  *--buf = '\0';
  do
    *--buf = '0' + (int) (o % 10);
  while ((o /= 10) != 0);
  return buf;
}

