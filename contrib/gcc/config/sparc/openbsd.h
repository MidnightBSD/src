/* Configuration file for sparc OpenBSD target.
   Copyright (C) 1999, 2002, 2003 Free Software Foundation, Inc.

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

/* Target OS builtins.  */
#define TARGET_OS_CPP_BUILTINS()		\
  do						\
    {						\
	builtin_define ("__unix__");		\
	builtin_define ("__OpenBSD__");		\
	builtin_assert ("system=unix");		\
	builtin_assert ("system=OpenBSD");	\
    }						\
  while (0)

/* Layout of source language data types */

/* This must agree with <machine/ansi.h> */
#undef SIZE_TYPE
#define SIZE_TYPE "unsigned int"

#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "int"

#undef WCHAR_TYPE
#define WCHAR_TYPE "int"

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32

/* Specific options for DBX Output.  */

/* This is BSD, so it wants DBX format.  */
#define DBX_DEBUGGING_INFO 1

/* This is the char to use for continuation */
#define DBX_CONTIN_CHAR '?'

/* Stack & calling: aggregate returns.  */

/* Don't default to pcc-struct-return, because gcc is the only compiler, and
   we want to retain compatibility with older gcc versions.  */
#undef DEFAULT_PCC_STRUCT_RETURN
#define DEFAULT_PCC_STRUCT_RETURN 0

/* Assembler format: exception region output.  */

/* All configurations that don't use elf must be explicit about not using
   dwarf unwind information. */
#define DWARF2_UNWIND_INFO 0

#undef ASM_PREFERRED_EH_DATA_FORMAT
