/* Definitions of target machine for GNU compiler, for ARM with PE obj format.
   Copyright (C) 1995, 1996, 1999, 2000, 2002, 2003 Free Software Foundation, Inc.
   Contributed by Doug Evans (dje@cygnus.com).
   
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
   along with GCC; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* Enable PE specific code.  */
#define ARM_PE		1

#define ARM_PE_FLAG_CHAR '@'

/* Ensure that @x. will be stripped from the function name.  */
#undef SUBTARGET_NAME_ENCODING_LENGTHS
#define SUBTARGET_NAME_ENCODING_LENGTHS  \
  case ARM_PE_FLAG_CHAR: return 3;

#undef  USER_LABEL_PREFIX
#define USER_LABEL_PREFIX "_"


/* Run-time Target Specification.  */
#undef  TARGET_VERSION
#define TARGET_VERSION fputs (" (ARM/pe)", stderr)

/* Get tree.c to declare a target-specific specialization of
   merge_decl_attributes.  */
#define TARGET_DLLIMPORT_DECL_ATTRIBUTES

/* Support the __declspec keyword by turning them into attributes.
   We currently only support: naked, dllimport, and dllexport.
   Note that the current way we do this may result in a collision with
   predefined attributes later on.  This can be solved by using one attribute,
   say __declspec__, and passing args to it.  The problem with that approach
   is that args are not accumulated: each new appearance would clobber any
   existing args.  */
#undef  SUBTARGET_CPP_SPEC
#define SUBTARGET_CPP_SPEC "-D__pe__ -D__declspec(x)=__attribute__((x))"


/* Experimental addition for pr 7885.
   Ignore dllimport for functions.  */
#define TARGET_FLAG_NOP_FUN	(1 << 24)

#undef  TARGET_NOP_FUN_DLLIMPORT
#define TARGET_NOP_FUN_DLLIMPORT (target_flags & TARGET_FLAG_NOP_FUN)

#undef  SUBTARGET_SWITCHES
#define SUBTARGET_SWITCHES					\
{ "nop-fun-dllimport",		  TARGET_FLAG_NOP_FUN,		\
  N_("Ignore dllimport attribute for functions") },		\
{ "no-nop-fun-dllimport",	- TARGET_FLAG_NOP_FUN, "" },

/* Defaulting to APCS-26 support is a legacy issue.   It has been done
   that way for a long time, so changing it will probably break some
   people's worlds.  Support for APCS-32 is now enabled as a multilib,
   and at some point in the future APCS-32 may become the default.
   Possibly when chips that support APCS-26 are no longer made.  */

#undef  TARGET_DEFAULT
#define TARGET_DEFAULT	(ARM_FLAG_SOFT_FLOAT | TARGET_FLAG_NOP_FUN | ARM_FLAG_MMU_TRAPS)

#undef  MULTILIB_DEFAULTS
#define MULTILIB_DEFAULTS \
  { "marm", "mlittle-endian", "msoft-float", "mapcs-26", "mno-thumb-interwork" }  

#undef  WCHAR_TYPE
#define WCHAR_TYPE 	"short unsigned int"
#undef  WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 16

/* r11 is fixed.  */
#undef  SUBTARGET_CONDITIONAL_REGISTER_USAGE
#define SUBTARGET_CONDITIONAL_REGISTER_USAGE \
  fixed_regs [11] = 1; \
  call_used_regs [11] = 1;


/* Define this macro if in some cases global symbols from one translation
   unit may not be bound to undefined symbols in another translation unit
   without user intervention.  For instance, under Microsoft Windows
   symbols must be explicitly imported from shared libraries (DLLs).  */
#define MULTIPLE_SYMBOL_SPACES

#define TARGET_ASM_UNIQUE_SECTION arm_pe_unique_section

#define SUPPORTS_ONE_ONLY 1

/* Switch into a generic section.  */
#undef  TARGET_ASM_NAMED_SECTION
#define TARGET_ASM_NAMED_SECTION  default_pe_asm_named_section

#define TARGET_ASM_FILE_START_FILE_DIRECTIVE true

/* Output a reference to a label.  */
#undef  ASM_OUTPUT_LABELREF
#define ASM_OUTPUT_LABELREF(STREAM, NAME)  \
  asm_fprintf (STREAM, "%U%s", arm_strip_name_encoding (NAME))

/* Output a function definition label.  */
#undef  ASM_DECLARE_FUNCTION_NAME
#define ASM_DECLARE_FUNCTION_NAME(STREAM, NAME, DECL)   	\
  do								\
    {								\
      if (arm_dllexport_name_p (NAME))				\
	{							\
	  drectve_section ();					\
	  fprintf (STREAM, "\t.ascii \" -export:%s\"\n",	\
		   arm_strip_name_encoding (NAME));		\
	  function_section (DECL);				\
	}							\
      ARM_DECLARE_FUNCTION_NAME (STREAM, NAME, DECL);		\
      if (TARGET_THUMB)						\
	fprintf (STREAM, "\t.code 16\n");			\
      ASM_OUTPUT_LABEL (STREAM, NAME);				\
    }								\
  while (0)

/* Output a common block.  */
#undef  ASM_OUTPUT_COMMON
#define ASM_OUTPUT_COMMON(STREAM, NAME, SIZE, ROUNDED)	\
  do							\
    {							\
      if (arm_dllexport_name_p (NAME))			\
	{						\
	  drectve_section ();				\
	  fprintf ((STREAM), "\t.ascii \" -export:%s\"\n",\
		   arm_strip_name_encoding (NAME));	\
	}						\
      if (! arm_dllimport_name_p (NAME))		\
	{						\
	  fprintf ((STREAM), "\t.comm\t"); 		\
	  assemble_name ((STREAM), (NAME));		\
	  asm_fprintf ((STREAM), ", %d\t%@ %d\n",	\
 		   (int)(ROUNDED), (int)(SIZE));	\
	}						\
    }							\
  while (0)

/* Output the label for an initialized variable.  */
#undef  ASM_DECLARE_OBJECT_NAME
#define ASM_DECLARE_OBJECT_NAME(STREAM, NAME, DECL) 	\
  do							\
    {							\
      if (arm_dllexport_name_p (NAME))			\
	{						\
	  enum in_section save_section = in_section;	\
	  drectve_section ();				\
	  fprintf (STREAM, "\t.ascii \" -export:%s\"\n",\
		   arm_strip_name_encoding (NAME));	\
	  switch_to_section (save_section, (DECL));	\
	}						\
      ASM_OUTPUT_LABEL ((STREAM), (NAME));		\
    }							\
  while (0)

/* Support the ctors/dtors and other sections.  */

#define DRECTVE_SECTION_ASM_OP	"\t.section .drectve"

/* A list of other sections which the compiler might be "in" at any
   given time.  */

#undef  EXTRA_SECTIONS
#define EXTRA_SECTIONS in_drectve

/* A list of extra section function definitions.  */

#undef  EXTRA_SECTION_FUNCTIONS
#define EXTRA_SECTION_FUNCTIONS \
  DRECTVE_SECTION_FUNCTION	\
  SWITCH_TO_SECTION_FUNCTION

#define DRECTVE_SECTION_FUNCTION \
void									\
drectve_section (void)							\
{									\
  if (in_section != in_drectve)						\
    {									\
      fprintf (asm_out_file, "%s\n", DRECTVE_SECTION_ASM_OP);		\
      in_section = in_drectve;						\
    }									\
}

/* Switch to SECTION (an `enum in_section').

   ??? This facility should be provided by GCC proper.
   The problem is that we want to temporarily switch sections in
   ASM_DECLARE_OBJECT_NAME and then switch back to the original section
   afterwards.  */
#define SWITCH_TO_SECTION_FUNCTION				\
static void							\
switch_to_section (enum in_section section, tree decl)		\
{								\
  switch (section)						\
    {								\
      case in_text: text_section (); break;			\
      case in_data: data_section (); break;			\
      case in_named: named_section (decl, NULL, 0); break;	\
      case in_readonly_data: readonly_data_section (); break;	\
      case in_ctors: ctors_section (); break;			\
      case in_dtors: dtors_section (); break;			\
      case in_drectve: drectve_section (); break;		\
      default: abort (); break;					\
    }								\
}

