/* Operating system specific defines to be used when targeting GCC for
   hosting on Windows32, using a Unix style C library and tools.
   Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.

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

#define DBX_DEBUGGING_INFO 1
#define SDB_DEBUGGING_INFO 1
#undef PREFERRED_DEBUGGING_TYPE
#define PREFERRED_DEBUGGING_TYPE DBX_DEBUG

#define TARGET_EXECUTABLE_SUFFIX ".exe"

#define TARGET_IS_PE_COFF 1

#include <stdio.h>

/* Masks for subtarget switches used by other files.  */
#define MASK_NOP_FUN_DLLIMPORT 0x08000000 /* Ignore dllimport for functions */

/* Used in winnt.c.  */
#define TARGET_NOP_FUN_DLLIMPORT (target_flags & MASK_NOP_FUN_DLLIMPORT)

#undef  SUBTARGET_SWITCHES
#define SUBTARGET_SWITCHES \
{ "cygwin",		  0, N_("Use the Cygwin interface") },	\
{ "no-cygwin",		  0, N_("Use the Mingw32 interface") },	\
{ "windows",		  0, N_("Create GUI application") },	\
{ "no-win32",		  0, N_("Don't set Windows defines") },	\
{ "win32",		  0, N_("Set Windows defines") },	\
{ "console",		  0, N_("Create console application") },\
{ "dll",		  0, N_("Generate code for a DLL") },	\
{ "nop-fun-dllimport",	  MASK_NOP_FUN_DLLIMPORT,		\
  N_("Ignore dllimport for functions") },			\
{ "no-nop-fun-dllimport", -MASK_NOP_FUN_DLLIMPORT, "" },	\
{ "threads",		  0, N_("Use Mingw-specific thread support") },

#define MAYBE_UWIN_CPP_BUILTINS() /* Nothing.  */

/* Support the __declspec keyword by turning them into attributes.
   We currently only support: dllimport and dllexport.
   Note that the current way we do this may result in a collision with
   predefined attributes later on.  This can be solved by using one attribute,
   say __declspec__, and passing args to it.  The problem with that approach
   is that args are not accumulated: each new appearance would clobber any
   existing args.  */

#define TARGET_OS_CPP_BUILTINS()					\
  do									\
    {									\
	builtin_define ("_X86_=1");					\
	builtin_assert ("system=winnt");				\
	builtin_define ("__stdcall=__attribute__((__stdcall__))");	\
	builtin_define ("__fastcall=__attribute__((__fastcall__))");	\
	builtin_define ("__cdecl=__attribute__((__cdecl__))");		\
	builtin_define ("__declspec(x)=__attribute__((x))");		\
	if (!flag_iso)							\
	  {								\
	    builtin_define ("_stdcall=__attribute__((__stdcall__))");	\
	    builtin_define ("_fastcall=__attribute__((__fastcall__))");	\
	    builtin_define ("_cdecl=__attribute__((__cdecl__))");	\
	  }								\
	MAYBE_UWIN_CPP_BUILTINS ();					\
	EXTRA_OS_CPP_BUILTINS ();					\
  }									\
  while (0)

/* Get tree.c to declare a target-specific specialization of
   merge_decl_attributes.  */
#define TARGET_DLLIMPORT_DECL_ATTRIBUTES

/* This macro defines names of additional specifications to put in the specs
   that can be used in various specifications like CC1_SPEC.  Its definition
   is an initializer with a subgrouping for each command option.

   Each subgrouping contains a string constant, that defines the
   specification name, and a string constant that used by the GCC driver
   program.

   Do not define this macro if it does not need to do anything.  */

#undef  SUBTARGET_EXTRA_SPECS
#define SUBTARGET_EXTRA_SPECS						\
  { "mingw_include_path", DEFAULT_TARGET_MACHINE }

#undef MATH_LIBRARY
#define MATH_LIBRARY ""

#define SIZE_TYPE "unsigned int"
#define PTRDIFF_TYPE "int"
#define WCHAR_TYPE_SIZE 16
#define WCHAR_TYPE "short unsigned int"


/* Enable parsing of #pragma pack(push,<n>) and #pragma pack(pop).  */
#define HANDLE_PRAGMA_PACK_PUSH_POP 1

union tree_node;
#define TREE union tree_node *

#undef EXTRA_SECTIONS
#define EXTRA_SECTIONS in_drectve

#undef EXTRA_SECTION_FUNCTIONS
#define EXTRA_SECTION_FUNCTIONS					\
  DRECTVE_SECTION_FUNCTION					\
  SWITCH_TO_SECTION_FUNCTION

#define DRECTVE_SECTION_FUNCTION \
void									\
drectve_section (void)							\
{									\
  if (in_section != in_drectve)						\
    {									\
      fprintf (asm_out_file, "%s\n", "\t.section .drectve\n");		\
      in_section = in_drectve;						\
    }									\
}
void drectve_section (void);

/* Older versions of gas don't handle 'r' as data.
   Explicitly set data flag with 'd'.  */  
#define READONLY_DATA_SECTION_ASM_OP "\t.section .rdata,\"dr\""

/* Switch to SECTION (an `enum in_section').

   ??? This facility should be provided by GCC proper.
   The problem is that we want to temporarily switch sections in
   ASM_DECLARE_OBJECT_NAME and then switch back to the original section
   afterwards.  */
#define SWITCH_TO_SECTION_FUNCTION				\
void switch_to_section (enum in_section, tree);			\
void								\
switch_to_section (enum in_section section, tree decl)		\
{								\
  switch (section)						\
    {								\
      case in_text: text_section (); break;			\
      case in_data: data_section (); break;			\
      case in_readonly_data: readonly_data_section (); break;	\
      case in_named: named_section (decl, NULL, 0); break;	\
      case in_drectve: drectve_section (); break;		\
      default: abort (); break;				\
    }								\
}

/* Don't allow flag_pic to propagate since gas may produce invalid code
   otherwise.  */

#undef  SUBTARGET_OVERRIDE_OPTIONS
#define SUBTARGET_OVERRIDE_OPTIONS					\
do {									\
  if (flag_pic)								\
    {									\
      warning ("-f%s ignored for target (all code is position independent)",\
	       (flag_pic > 1) ? "PIC" : "pic");				\
      flag_pic = 0;							\
    }									\
} while (0)								\

/* Define this macro if references to a symbol must be treated
   differently depending on something about the variable or
   function named by the symbol (such as what section it is in).

   On i386 running Windows NT, modify the assembler name with a suffix
   consisting of an atsign (@) followed by string of digits that represents
   the number of bytes of arguments passed to the function, if it has the
   attribute STDCALL.

   In addition, we must mark dll symbols specially. Definitions of
   dllexport'd objects install some info in the .drectve section.
   References to dllimport'd objects are fetched indirectly via
   _imp__.  If both are declared, dllexport overrides.  This is also
   needed to implement one-only vtables: they go into their own
   section and we need to set DECL_SECTION_NAME so we do that here.
   Note that we can be called twice on the same decl.  */

#undef TARGET_ENCODE_SECTION_INFO
#define TARGET_ENCODE_SECTION_INFO  i386_pe_encode_section_info
#undef  TARGET_STRIP_NAME_ENCODING
#define TARGET_STRIP_NAME_ENCODING  i386_pe_strip_name_encoding_full

/* Output a reference to a label.  */
#undef ASM_OUTPUT_LABELREF
#define ASM_OUTPUT_LABELREF  i386_pe_output_labelref

/* Output a common block.  */
#undef ASM_OUTPUT_COMMON
#define ASM_OUTPUT_COMMON(STREAM, NAME, SIZE, ROUNDED)	\
do {							\
  if (i386_pe_dllexport_name_p (NAME))			\
    i386_pe_record_exported_symbol (NAME, 1);		\
  if (! i386_pe_dllimport_name_p (NAME))		\
    {							\
      fprintf ((STREAM), "\t.comm\t");			\
      assemble_name ((STREAM), (NAME));			\
      fprintf ((STREAM), ", %d\t%s %d\n",		\
	       (int)(ROUNDED), ASM_COMMENT_START, (int)(SIZE));	\
    }							\
} while (0)

/* Output the label for an initialized variable.  */
#undef ASM_DECLARE_OBJECT_NAME
#define ASM_DECLARE_OBJECT_NAME(STREAM, NAME, DECL)	\
do {							\
  if (i386_pe_dllexport_name_p (NAME))			\
    i386_pe_record_exported_symbol (NAME, 1);		\
  ASM_OUTPUT_LABEL ((STREAM), (NAME));			\
} while (0)


/* Emit code to check the stack when allocating more that 4000
   bytes in one go.  */

#define CHECK_STACK_LIMIT 4000

/* By default, target has a 80387, uses IEEE compatible arithmetic,
   returns float values in the 387 and needs stack probes.
   We also align doubles to 64-bits for MSVC default compatibility.  */

#undef TARGET_SUBTARGET_DEFAULT
#define TARGET_SUBTARGET_DEFAULT \
   (MASK_80387 | MASK_IEEE_FP | MASK_FLOAT_RETURNS | MASK_STACK_PROBE \
    | MASK_ALIGN_DOUBLE)

/* This is how to output an assembler line
   that says to advance the location counter
   to a multiple of 2**LOG bytes.  */

#undef ASM_OUTPUT_ALIGN
#define ASM_OUTPUT_ALIGN(FILE,LOG)	\
    if ((LOG)!=0) fprintf ((FILE), "\t.align %d\n", 1<<(LOG))

/* Define this macro if in some cases global symbols from one translation
   unit may not be bound to undefined symbols in another translation unit
   without user intervention.  For instance, under Microsoft Windows
   symbols must be explicitly imported from shared libraries (DLLs).  */
#define MULTIPLE_SYMBOL_SPACES

extern void i386_pe_unique_section (TREE, int);
#define TARGET_ASM_UNIQUE_SECTION i386_pe_unique_section

#define SUPPORTS_ONE_ONLY 1

/* Switch into a generic section.  */
#define TARGET_ASM_NAMED_SECTION  i386_pe_asm_named_section

/* Select attributes for named sections.  */
#define TARGET_SECTION_TYPE_FLAGS  i386_pe_section_type_flags

/* Write the extra assembler code needed to declare a function
   properly.  If we are generating SDB debugging information, this
   will happen automatically, so we only need to handle other cases.  */
#undef ASM_DECLARE_FUNCTION_NAME
#define ASM_DECLARE_FUNCTION_NAME(FILE, NAME, DECL)			\
  do									\
    {									\
      if (i386_pe_dllexport_name_p (NAME))				\
	i386_pe_record_exported_symbol (NAME, 0);			\
      if (write_symbols != SDB_DEBUG)					\
	i386_pe_declare_function_type (FILE, NAME, TREE_PUBLIC (DECL));	\
      ASM_OUTPUT_LABEL (FILE, NAME);					\
    }									\
  while (0)

/* Add an external function to the list of functions to be declared at
   the end of the file.  */
#define ASM_OUTPUT_EXTERNAL(FILE, DECL, NAME)				\
  do									\
    {									\
      if (TREE_CODE (DECL) == FUNCTION_DECL)				\
	i386_pe_record_external_function (NAME);			\
    }									\
  while (0)

/* Declare the type properly for any external libcall.  */
#define ASM_OUTPUT_EXTERNAL_LIBCALL(FILE, FUN) \
  i386_pe_declare_function_type (FILE, XSTR (FUN, 0), 1)

/* This says out to put a global symbol in the BSS section.  */
#undef ASM_OUTPUT_ALIGNED_BSS
#define ASM_OUTPUT_ALIGNED_BSS(FILE, DECL, NAME, SIZE, ALIGN) \
  asm_output_aligned_bss ((FILE), (DECL), (NAME), (SIZE), (ALIGN))

/* Output function declarations at the end of the file.  */
#undef TARGET_ASM_FILE_END
#define TARGET_ASM_FILE_END i386_pe_file_end

#undef ASM_COMMENT_START
#define ASM_COMMENT_START " #"

/* DWARF2 Unwinding doesn't work with exception handling yet.  To make
   it work, we need to build a libgcc_s.dll, and dcrt0.o should be
   changed to call __register_frame_info/__deregister_frame_info.  */
#define DWARF2_UNWIND_INFO 0

/* Don't assume anything about the header files.  */
#define NO_IMPLICIT_EXTERN_C

#undef PROFILE_HOOK
#define PROFILE_HOOK(LABEL)						\
  if (MAIN_NAME_P (DECL_NAME (current_function_decl)))			\
    {									\
      emit_call_insn (gen_rtx (CALL, VOIDmode,				\
	gen_rtx_MEM (FUNCTION_MODE,					\
		     gen_rtx_SYMBOL_REF (Pmode, "_monstartup")),	\
	const0_rtx));							\
    }

/* Java Native Interface (JNI) methods on Win32 are invoked using the
   stdcall calling convention.  */
#undef MODIFY_JNI_METHOD_CALL
#define MODIFY_JNI_METHOD_CALL(MDECL)					      \
  build_type_attribute_variant ((MDECL),				      \
			       build_tree_list (get_identifier ("stdcall"),   \
						NULL))

/* External function declarations.  */

extern void i386_pe_record_external_function (const char *);
extern void i386_pe_declare_function_type (FILE *, const char *, int);
extern void i386_pe_record_exported_symbol (const char *, int);
extern void i386_pe_file_end (void);
extern int i386_pe_dllexport_name_p (const char *);
extern int i386_pe_dllimport_name_p (const char *);

/* For Win32 ABI compatibility */
#undef DEFAULT_PCC_STRUCT_RETURN
#define DEFAULT_PCC_STRUCT_RETURN 0

/* MSVC returns aggregate types of up to 8 bytes via registers.
   See i386.c:ix86_return_in_memory.  */
#undef MS_AGGREGATE_RETURN
#define MS_AGGREGATE_RETURN 1

/* No data type wants to be aligned rounder than this.  */
#undef	BIGGEST_ALIGNMENT
#define BIGGEST_ALIGNMENT 128

/* Native complier aligns internal doubles in structures on dword boundaries.  */
#undef	BIGGEST_FIELD_ALIGNMENT
#define BIGGEST_FIELD_ALIGNMENT 64

/* A bit-field declared as `int' forces `int' alignment for the struct.  */
#undef PCC_BITFIELD_TYPE_MATTERS
#define PCC_BITFIELD_TYPE_MATTERS 1
#define GROUP_BITFIELDS_BY_ALIGN TYPE_NATIVE(rec)

/* Enable alias attribute support.  */
#ifndef SET_ASM_OP
#define SET_ASM_OP "\t.set\t"
#endif
/* This implements the `alias' attribute, keeping any stdcall or
   fastcall decoration.  */
#undef	ASM_OUTPUT_DEF_FROM_DECLS
#define	ASM_OUTPUT_DEF_FROM_DECLS(STREAM, DECL, TARGET) 		\
  do									\
    {									\
      const char *alias;						\
      rtx rtlname = XEXP (DECL_RTL (DECL), 0);				\
      if (GET_CODE (rtlname) == SYMBOL_REF)				\
	alias = XSTR (rtlname, 0);					\
      else								\
	abort ();							\
      if (TREE_CODE (DECL) == FUNCTION_DECL)				\
	i386_pe_declare_function_type (STREAM, alias,			\
				       TREE_PUBLIC (DECL));		\
      ASM_OUTPUT_DEF (STREAM, alias, IDENTIFIER_POINTER (TARGET));	\
    } while (0)

#undef TREE

#ifndef BUFSIZ
# undef FILE
#endif
