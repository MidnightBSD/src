/* Definitions of target machine for GCC, for SPARC64, a.out.
   Copyright (C) 1994, 1996, 1997, 1998  Free Software Foundation, Inc.
   Contributed by Doug Evans, dje@cygnus.com.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */


#undef TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (sparc64-aout)")

#undef TARGET_DEFAULT
#define TARGET_DEFAULT \
  (MASK_V9 + MASK_PTR64 + MASK_64BIT + MASK_HARD_QUAD \
   + MASK_APP_REGS + MASK_FPU + MASK_STACK_BIAS)

/* The only code model supported is Medium/Low.  */
#undef SPARC_DEFAULT_CMODEL
#define SPARC_DEFAULT_CMODEL CM_MEDLOW
