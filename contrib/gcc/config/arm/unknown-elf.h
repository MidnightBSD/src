/* Definitions for non-Linux based ARM systems using ELF
   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.
   Contributed by Catherine Moore <clm@cygnus.com>

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 2, or (at your
   option) any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* elfos.h should have already been included.  Now just override
   any conflicting definitions and add any extras.  */

/* Run-time Target Specification.  */
#ifndef TARGET_VERSION
#define TARGET_VERSION	fputs (" (ARM/ELF)", stderr);
#endif

/* Default to using APCS-32 and software floating point.  */
#ifndef TARGET_DEFAULT
#define TARGET_DEFAULT	(ARM_FLAG_SOFT_FLOAT | ARM_FLAG_APCS_32 | ARM_FLAG_APCS_FRAME | ARM_FLAG_MMU_TRAPS)
#endif

/* Now we define the strings used to build the spec file.  */
#undef  STARTFILE_SPEC
#define STARTFILE_SPEC	" crti%O%s crtbegin%O%s crt0%O%s"

#undef  ENDFILE_SPEC
#define ENDFILE_SPEC	"crtend%O%s crtn%O%s"

/* The __USES_INITFINI__ define is tested in newlib/libc/sys/arm/crt0.S
   to see if it needs to invoked _init() and _fini().  */
#undef  SUBTARGET_CPP_SPEC
#define SUBTARGET_CPP_SPEC  "-D__USES_INITFINI__"

#undef  PREFERRED_DEBUGGING_TYPE
#define PREFERRED_DEBUGGING_TYPE DWARF2_DEBUG

/* Return a nonzero value if DECL has a section attribute.  */
#define IN_NAMED_SECTION(DECL)						\
  ((TREE_CODE (DECL) == FUNCTION_DECL || TREE_CODE (DECL) == VAR_DECL)	\
   && DECL_SECTION_NAME (DECL) != NULL_TREE)

#undef  ASM_OUTPUT_ALIGNED_BSS
#define ASM_OUTPUT_ALIGNED_BSS(FILE, DECL, NAME, SIZE, ALIGN)   	\
  do									\
    {									\
      if (IN_NAMED_SECTION (DECL))					\
	named_section (DECL, NULL, 0);					\
      else								\
	bss_section ();							\
      									\
      ASM_OUTPUT_ALIGN (FILE, floor_log2 (ALIGN / BITS_PER_UNIT));	\
									\
      last_assemble_variable_decl = DECL;				\
      ASM_DECLARE_OBJECT_NAME (FILE, NAME, DECL);			\
      ASM_OUTPUT_SKIP (FILE, SIZE ? (int)(SIZE) : 1);			\
    } 									\
  while (0)

#undef  ASM_OUTPUT_ALIGNED_DECL_LOCAL
#define ASM_OUTPUT_ALIGNED_DECL_LOCAL(FILE, DECL, NAME, SIZE, ALIGN)	\
  do									\
    {									\
      if ((DECL) != NULL && IN_NAMED_SECTION (DECL))			\
	named_section (DECL, NULL, 0);					\
      else								\
	bss_section ();							\
									\
      ASM_OUTPUT_ALIGN (FILE, floor_log2 (ALIGN / BITS_PER_UNIT));	\
      ASM_OUTPUT_LABEL (FILE, NAME);					\
      fprintf (FILE, "\t.space\t%d\n", SIZE ? (int)(SIZE) : 1);		\
    }									\
  while (0)

#ifndef CPP_APCS_PC_DEFAULT_SPEC
#define CPP_APCS_PC_DEFAULT_SPEC	"-D__APCS_32__"
#endif
     
#ifndef SUBTARGET_CPU_DEFAULT
#define SUBTARGET_CPU_DEFAULT 		TARGET_CPU_arm7tdmi
#endif

