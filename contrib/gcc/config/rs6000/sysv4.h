/* Target definitions for GNU compiler for PowerPC running System V.4
   Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

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
   along with GCC; see the file COPYING.  If not, write to the
   Free Software Foundation, 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.  */

/* Header files should be C++ aware in general.  */
#define NO_IMPLICIT_EXTERN_C

/* Yes!  We are ELF.  */
#define	TARGET_OBJECT_FORMAT OBJECT_ELF

/* Default ABI to compile code for.  */
#define DEFAULT_ABI rs6000_current_abi

/* Default ABI to use.  */
#define RS6000_ABI_NAME "sysv"

/* Override rs6000.h definition.  */
#undef	ASM_DEFAULT_SPEC
#define	ASM_DEFAULT_SPEC "-mppc"

/* Small data support types.  */
enum rs6000_sdata_type {
  SDATA_NONE,			/* No small data support.  */
  SDATA_DATA,			/* Just put data in .sbss/.sdata, don't use relocs.  */
  SDATA_SYSV,			/* Use r13 to point to .sdata/.sbss.  */
  SDATA_EABI			/* Use r13 like above, r2 points to .sdata2/.sbss2.  */
};

extern enum rs6000_sdata_type rs6000_sdata;

/* V.4/eabi switches.  */
#define	MASK_NO_BITFIELD_TYPE	0x40000000	/* Set PCC_BITFIELD_TYPE_MATTERS to 0.  */
#define	MASK_STRICT_ALIGN	0x20000000	/* Set STRICT_ALIGNMENT to 1.  */
#define	MASK_RELOCATABLE	0x10000000	/* GOT pointers are PC relative.  */
#define	MASK_EABI		0x08000000	/* Adhere to eabi, not System V spec.  */
#define	MASK_LITTLE_ENDIAN	0x04000000	/* Target is little endian.  */
#define	MASK_REGNAMES		0x02000000	/* Use alternate register names.  */
#define	MASK_PROTOTYPE		0x01000000	/* Only prototyped fcns pass variable args.  */
#define MASK_NO_BITFIELD_WORD	0x00800000	/* Bitfields cannot cross word boundaries */

#define	TARGET_NO_BITFIELD_TYPE	(target_flags & MASK_NO_BITFIELD_TYPE)
#define	TARGET_STRICT_ALIGN	(target_flags & MASK_STRICT_ALIGN)
#define	TARGET_RELOCATABLE	(target_flags & MASK_RELOCATABLE)
#define	TARGET_EABI		(target_flags & MASK_EABI)
#define	TARGET_LITTLE_ENDIAN	(target_flags & MASK_LITTLE_ENDIAN)
#define	TARGET_REGNAMES		(target_flags & MASK_REGNAMES)
#define	TARGET_PROTOTYPE	(target_flags & MASK_PROTOTYPE)
#define TARGET_NO_BITFIELD_WORD	(target_flags & MASK_NO_BITFIELD_WORD)
#define	TARGET_TOC		((target_flags & MASK_64BIT)		\
				 || ((target_flags & (MASK_RELOCATABLE	\
						      | MASK_MINIMAL_TOC)) \
				     && flag_pic > 1)			\
				 || DEFAULT_ABI == ABI_AIX)

#define	TARGET_BITFIELD_TYPE	(! TARGET_NO_BITFIELD_TYPE)
#define	TARGET_BIG_ENDIAN	(! TARGET_LITTLE_ENDIAN)
#define	TARGET_NO_PROTOTYPE	(! TARGET_PROTOTYPE)
#define	TARGET_NO_TOC		(! TARGET_TOC)
#define	TARGET_NO_EABI		(! TARGET_EABI)

/* Strings provided by SUBTARGET_OPTIONS */
extern const char *rs6000_abi_name;
extern const char *rs6000_sdata_name;
extern const char *rs6000_tls_size_string; /* For -mtls-size= */

/* Override rs6000.h definition.  */
#undef	SUBTARGET_OPTIONS
#define	SUBTARGET_OPTIONS							\
  { "call-",  &rs6000_abi_name, N_("Select ABI calling convention"), 0},	\
  { "sdata=", &rs6000_sdata_name, N_("Select method for sdata handling"), 0},	\
  { "tls-size=", &rs6000_tls_size_string,					\
   N_("Specify bit size of immediate TLS offsets"), 0 }

#define SDATA_DEFAULT_SIZE 8

/* Note, V.4 no longer uses a normal TOC, so make -mfull-toc, be just
   the same as -mminimal-toc.  */
/* Override rs6000.h definition.  */
#undef	SUBTARGET_SWITCHES
#define SUBTARGET_SWITCHES						\
  { "bit-align",	-MASK_NO_BITFIELD_TYPE,				\
    N_("Align to the base type of the bit-field") },			\
  { "no-bit-align",	 MASK_NO_BITFIELD_TYPE,				\
    N_("Don't align to the base type of the bit-field") },		\
  { "strict-align",	 MASK_STRICT_ALIGN,				\
    N_("Don't assume that unaligned accesses are handled by the system") }, \
  { "no-strict-align",	-MASK_STRICT_ALIGN,				\
    N_("Assume that unaligned accesses are handled by the system") },	\
  { "relocatable",	 MASK_RELOCATABLE | MASK_MINIMAL_TOC | MASK_NO_FP_IN_TOC, \
    N_("Produce code relocatable at runtime") },			\
  { "no-relocatable",	-MASK_RELOCATABLE,				\
    N_("Don't produce code relocatable at runtime") },			\
  { "relocatable-lib",	 MASK_RELOCATABLE | MASK_MINIMAL_TOC | MASK_NO_FP_IN_TOC, \
    N_("Produce code relocatable at runtime") },			\
  { "no-relocatable-lib", -MASK_RELOCATABLE,				\
    N_("Don't produce code relocatable at runtime") },			\
  { "little-endian",	 MASK_LITTLE_ENDIAN,				\
    N_("Produce little endian code") },					\
  { "little",		 MASK_LITTLE_ENDIAN,				\
    N_("Produce little endian code") },					\
  { "big-endian",	-MASK_LITTLE_ENDIAN,				\
    N_("Produce big endian code") },					\
  { "big",		-MASK_LITTLE_ENDIAN,				\
    N_("Produce big endian code") },					\
  { "no-toc",		 0, N_("no description yet") },			\
  { "toc",		 MASK_MINIMAL_TOC, N_("no description yet") },	\
  { "full-toc",		 MASK_MINIMAL_TOC, N_("no description yet") },	\
  { "prototype",	 MASK_PROTOTYPE,				\
    N_("Assume all variable arg functions are prototyped") },		\
  { "no-prototype",	-MASK_PROTOTYPE,				\
    N_("Non-prototyped functions might take a variable number of args") }, \
  { "no-traceback",	 0, N_("no description yet") },			\
  { "eabi",		 MASK_EABI, N_("Use EABI") },			\
  { "no-eabi",		-MASK_EABI, N_("Don't use EABI") },		\
  { "bit-word",		-MASK_NO_BITFIELD_WORD, "" },			\
  { "no-bit-word",	 MASK_NO_BITFIELD_WORD,				\
    N_("Do not allow bit-fields to cross word boundaries") },		\
  { "regnames",		  MASK_REGNAMES,				\
    N_("Use alternate register names") },				\
  { "no-regnames",	 -MASK_REGNAMES,				\
    N_("Don't use alternate register names") },				\
  { "sdata",		 0, N_("no description yet") },			\
  { "no-sdata",		 0, N_("no description yet") },			\
  { "sim",		 0,						\
    N_("Link with libsim.a, libc.a and sim-crt0.o") },			\
  { "ads",		 0,						\
    N_("Link with libads.a, libc.a and crt0.o") },			\
  { "yellowknife",	 0,						\
    N_("Link with libyk.a, libc.a and crt0.o") },			\
  { "mvme",		 0,						\
    N_("Link with libmvme.a, libc.a and crt0.o") },			\
  { "emb",		 0,						\
    N_("Set the PPC_EMB bit in the ELF flags header") },		\
  { "windiss",           0, N_("Use the WindISS simulator") },          \
  { "shlib",		 0, N_("no description yet") },			\
  { "64",		 MASK_64BIT | MASK_POWERPC64 | MASK_POWERPC,	\
			 N_("Generate 64-bit code") },			\
  { "32",		 - (MASK_64BIT | MASK_POWERPC64),		\
			 N_("Generate 32-bit code") },			\
  EXTRA_SUBTARGET_SWITCHES						\
  { "newlib",		 0, N_("no description yet") },

/* This is meant to be redefined in the host dependent files.  */
#define EXTRA_SUBTARGET_SWITCHES

/* Sometimes certain combinations of command options do not make sense
   on a particular target machine.  You can define a macro
   `OVERRIDE_OPTIONS' to take account of this.  This macro, if
   defined, is executed once just after all the command options have
   been parsed.

   The macro SUBTARGET_OVERRIDE_OPTIONS is provided for subtargets, to
   get control.  */

#define SUBTARGET_OVERRIDE_OPTIONS					\
do {									\
  if (!g_switch_set)							\
    g_switch_value = SDATA_DEFAULT_SIZE;				\
									\
  if (rs6000_abi_name == NULL)						\
    rs6000_abi_name = RS6000_ABI_NAME;					\
									\
  if (!strcmp (rs6000_abi_name, "sysv"))				\
    rs6000_current_abi = ABI_V4;					\
  else if (!strcmp (rs6000_abi_name, "sysv-noeabi"))			\
    {									\
      rs6000_current_abi = ABI_V4;					\
      target_flags &= ~ MASK_EABI;					\
    }									\
  else if (!strcmp (rs6000_abi_name, "sysv-eabi")			\
	   || !strcmp (rs6000_abi_name, "eabi"))			\
    {									\
      rs6000_current_abi = ABI_V4;					\
      target_flags |= MASK_EABI;					\
    }									\
  else if (!strcmp (rs6000_abi_name, "aixdesc"))			\
    rs6000_current_abi = ABI_AIX;					\
  else if (!strcmp (rs6000_abi_name, "freebsd"))			\
    rs6000_current_abi = ABI_V4;					\
  else if (!strcmp (rs6000_abi_name, "linux"))				\
    rs6000_current_abi = ABI_V4;					\
  else if (!strcmp (rs6000_abi_name, "gnu"))				\
    rs6000_current_abi = ABI_V4;					\
  else if (!strcmp (rs6000_abi_name, "netbsd"))				\
    rs6000_current_abi = ABI_V4;					\
  else if (!strcmp (rs6000_abi_name, "openbsd"))			\
    rs6000_current_abi = ABI_V4;					\
  else if (!strcmp (rs6000_abi_name, "i960-old"))			\
    {									\
      rs6000_current_abi = ABI_V4;					\
      target_flags |= (MASK_LITTLE_ENDIAN | MASK_EABI			\
		       | MASK_NO_BITFIELD_WORD);			\
      target_flags &= ~MASK_STRICT_ALIGN;				\
    }									\
  else									\
    {									\
      rs6000_current_abi = ABI_V4;					\
      error ("bad value for -mcall-%s", rs6000_abi_name);		\
    }									\
									\
  if (rs6000_sdata_name)						\
    {									\
      if (!strcmp (rs6000_sdata_name, "none"))				\
	rs6000_sdata = SDATA_NONE;					\
      else if (!strcmp (rs6000_sdata_name, "data"))			\
	rs6000_sdata = SDATA_DATA;					\
      else if (!strcmp (rs6000_sdata_name, "default"))			\
	rs6000_sdata = (TARGET_EABI) ? SDATA_EABI : SDATA_SYSV;		\
      else if (!strcmp (rs6000_sdata_name, "sysv"))			\
	rs6000_sdata = SDATA_SYSV;					\
      else if (!strcmp (rs6000_sdata_name, "eabi"))			\
	rs6000_sdata = SDATA_EABI;					\
      else								\
	error ("bad value for -msdata=%s", rs6000_sdata_name);		\
    }									\
  else if (DEFAULT_ABI == ABI_V4)					\
    {									\
      rs6000_sdata = SDATA_DATA;					\
      rs6000_sdata_name = "data";					\
    }									\
  else									\
    {									\
      rs6000_sdata = SDATA_NONE;					\
      rs6000_sdata_name = "none";					\
    }									\
									\
  if (TARGET_RELOCATABLE &&						\
      (rs6000_sdata == SDATA_EABI || rs6000_sdata == SDATA_SYSV))	\
    {									\
      rs6000_sdata = SDATA_DATA;					\
      error ("-mrelocatable and -msdata=%s are incompatible",		\
	     rs6000_sdata_name);					\
    }									\
									\
  else if (flag_pic && DEFAULT_ABI != ABI_AIX				\
	   && (rs6000_sdata == SDATA_EABI				\
	       || rs6000_sdata == SDATA_SYSV))				\
    {									\
      rs6000_sdata = SDATA_DATA;					\
      error ("-f%s and -msdata=%s are incompatible",			\
	     (flag_pic > 1) ? "PIC" : "pic",				\
	     rs6000_sdata_name);					\
    }									\
									\
  if ((rs6000_sdata != SDATA_NONE && DEFAULT_ABI != ABI_V4)		\
      || (rs6000_sdata == SDATA_EABI && !TARGET_EABI))			\
    {									\
      rs6000_sdata = SDATA_NONE;					\
      error ("-msdata=%s and -mcall-%s are incompatible",		\
	     rs6000_sdata_name, rs6000_abi_name);			\
    }									\
									\
  targetm.have_srodata_section = rs6000_sdata == SDATA_EABI;		\
									\
  if (TARGET_RELOCATABLE && !TARGET_MINIMAL_TOC)			\
    {									\
      target_flags |= MASK_MINIMAL_TOC;					\
      error ("-mrelocatable and -mno-minimal-toc are incompatible");	\
    }									\
									\
  if (TARGET_RELOCATABLE && rs6000_current_abi == ABI_AIX)		\
    {									\
      target_flags &= ~MASK_RELOCATABLE;				\
      error ("-mrelocatable and -mcall-%s are incompatible",		\
	     rs6000_abi_name);						\
    }									\
									\
  if (!TARGET_64BIT && flag_pic > 1 && rs6000_current_abi == ABI_AIX)	\
    {									\
      flag_pic = 0;							\
      error ("-fPIC and -mcall-%s are incompatible",			\
	     rs6000_abi_name);						\
    }									\
									\
  if (rs6000_current_abi == ABI_AIX && TARGET_LITTLE_ENDIAN)		\
    {									\
      target_flags &= ~MASK_LITTLE_ENDIAN;				\
      error ("-mcall-aixdesc must be big endian");			\
    }									\
									\
  /* Treat -fPIC the same as -mrelocatable.  */				\
  if (flag_pic > 1 && DEFAULT_ABI != ABI_AIX)				\
    target_flags |= MASK_RELOCATABLE | MASK_MINIMAL_TOC | MASK_NO_FP_IN_TOC; \
									\
  else if (TARGET_RELOCATABLE)						\
    flag_pic = 2;							\
} while (0)

#ifndef RS6000_BI_ARCH
# define SUBSUBTARGET_OVERRIDE_OPTIONS					\
do {									\
  if ((TARGET_DEFAULT ^ target_flags) & MASK_64BIT)			\
    error ("-m%s not supported in this configuration",			\
	   (target_flags & MASK_64BIT) ? "64" : "32");			\
} while (0)
#endif

/* Override rs6000.h definition.  */
#undef	TARGET_DEFAULT
#define	TARGET_DEFAULT (MASK_POWERPC | MASK_NEW_MNEMONICS)

/* Override rs6000.h definition.  */
#undef	PROCESSOR_DEFAULT
#define	PROCESSOR_DEFAULT PROCESSOR_PPC750

#define FIXED_R2 1
/* System V.4 uses register 13 as a pointer to the small data area,
   so it is not available to the normal user.  */
#define FIXED_R13 1

/* Size of the V.4 varargs area if needed.  */
/* Override rs6000.h definition.  */
#undef	RS6000_VARARGS_AREA
#define RS6000_VARARGS_AREA ((cfun->machine->sysv_varargs_p) ? RS6000_VARARGS_SIZE : 0)

/* Override default big endianism definitions in rs6000.h.  */
#undef	BYTES_BIG_ENDIAN
#undef	WORDS_BIG_ENDIAN
#define	BYTES_BIG_ENDIAN (TARGET_BIG_ENDIAN)
#define	WORDS_BIG_ENDIAN (TARGET_BIG_ENDIAN)

/* Define this to set the endianness to use in libgcc2.c, which can
   not depend on target_flags.  */
#if !defined(__LITTLE_ENDIAN__) && !defined(__sun__)
#define LIBGCC2_WORDS_BIG_ENDIAN 1
#else
#define LIBGCC2_WORDS_BIG_ENDIAN 0
#endif

/* Define cutoff for using external functions to save floating point.
   Currently on V.4, always use inline stores.  */
#define FP_SAVE_INLINE(FIRST_REG) ((FIRST_REG) < 64)

/* Put jump tables in read-only memory, rather than in .text.  */
#define JUMP_TABLES_IN_TEXT_SECTION 0

/* Prefix and suffix to use to saving floating point.  */
#define	SAVE_FP_PREFIX "_savefpr_"
#define SAVE_FP_SUFFIX "_l"

/* Prefix and suffix to use to restoring floating point.  */
#define	RESTORE_FP_PREFIX "_restfpr_"
#define RESTORE_FP_SUFFIX "_l"

/* Type used for ptrdiff_t, as a string used in a declaration.  */
#define PTRDIFF_TYPE "int"

/* Type used for wchar_t, as a string used in a declaration.  */
/* Override svr4.h definition.  */
#undef	WCHAR_TYPE
#define WCHAR_TYPE "long int"

/* Width of wchar_t in bits.  */
/* Override svr4.h definition.  */
#undef	WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32

/* Make int foo : 8 not cause structures to be aligned to an int boundary.  */
/* Override elfos.h definition.  */
#undef	PCC_BITFIELD_TYPE_MATTERS
#define	PCC_BITFIELD_TYPE_MATTERS (TARGET_BITFIELD_TYPE)

#undef	BITFIELD_NBYTES_LIMITED
#define	BITFIELD_NBYTES_LIMITED (TARGET_NO_BITFIELD_WORD)

/* Define this macro to be the value 1 if instructions will fail to
   work if given data not on the nominal alignment.  If instructions
   will merely go slower in that case, define this macro as 0.  */
#undef	STRICT_ALIGNMENT
#define	STRICT_ALIGNMENT (TARGET_STRICT_ALIGN)

/* Define this macro if you wish to preserve a certain alignment for
   the stack pointer, greater than what the hardware enforces.  The
   definition is a C expression for the desired alignment (measured
   in bits).  This macro must evaluate to a value equal to or larger
   than STACK_BOUNDARY.
   For the SYSV ABI and variants the alignment of the stack pointer
   is usually controlled manually in rs6000.c. However, to maintain
   alignment across alloca () in all circumstances,
   PREFERRED_STACK_BOUNDARY needs to be set as well.
   This has the additional advantage of allowing a bigger maximum
   alignment of user objects on the stack.  */

#undef PREFERRED_STACK_BOUNDARY
#define PREFERRED_STACK_BOUNDARY 128

/* Real stack boundary as mandated by the appropriate ABI.  */
#define ABI_STACK_BOUNDARY \
  ((TARGET_EABI && !TARGET_ALTIVEC && !TARGET_ALTIVEC_ABI) ? 64 : 128)

/* An expression for the alignment of a structure field FIELD if the
   alignment computed in the usual way is COMPUTED.  */
#define ADJUST_FIELD_ALIGN(FIELD, COMPUTED)				      \
	((TARGET_ALTIVEC && TREE_CODE (TREE_TYPE (FIELD)) == VECTOR_TYPE)     \
	 ? 128 : COMPUTED)

/* Define this macro as an expression for the alignment of a type
   (given by TYPE as a tree node) if the alignment computed in the
   usual way is COMPUTED and the alignment explicitly specified was
   SPECIFIED.  */
#define ROUND_TYPE_ALIGN(TYPE, COMPUTED, SPECIFIED)			\
	((TARGET_ALTIVEC  && TREE_CODE (TYPE) == VECTOR_TYPE)	        \
	 ? MAX (MAX ((COMPUTED), (SPECIFIED)), 128)                     \
         : MAX (COMPUTED, SPECIFIED))

#undef  BIGGEST_FIELD_ALIGNMENT

/* Use ELF style section commands.  */

#define	TEXT_SECTION_ASM_OP	"\t.section\t\".text\""

#define	DATA_SECTION_ASM_OP	"\t.section\t\".data\""

#define	BSS_SECTION_ASM_OP	"\t.section\t\".bss\""

/* Override elfos.h definition.  */
#undef	INIT_SECTION_ASM_OP
#define	INIT_SECTION_ASM_OP "\t.section\t\".init\",\"ax\""

/* Override elfos.h definition.  */
#undef	FINI_SECTION_ASM_OP
#define	FINI_SECTION_ASM_OP "\t.section\t\".fini\",\"ax\""

#define	TOC_SECTION_ASM_OP "\t.section\t\".got\",\"aw\""

/* Put PC relative got entries in .got2.  */
#define	MINIMAL_TOC_SECTION_ASM_OP \
  (TARGET_RELOCATABLE || (flag_pic && DEFAULT_ABI != ABI_AIX)		\
   ? "\t.section\t\".got2\",\"aw\"" : "\t.section\t\".got1\",\"aw\"")

#define	SDATA_SECTION_ASM_OP "\t.section\t\".sdata\",\"aw\""
#define	SDATA2_SECTION_ASM_OP "\t.section\t\".sdata2\",\"a\""
#define	SBSS_SECTION_ASM_OP "\t.section\t\".sbss\",\"aw\",@nobits"

/* Besides the usual ELF sections, we need a toc section.  */
/* Override elfos.h definition.  */
#undef	EXTRA_SECTIONS
#define	EXTRA_SECTIONS in_toc, in_sdata, in_sdata2, in_sbss, in_init, in_fini

/* Override elfos.h definition.  */
#undef	EXTRA_SECTION_FUNCTIONS
#define	EXTRA_SECTION_FUNCTIONS						\
  TOC_SECTION_FUNCTION							\
  SDATA_SECTION_FUNCTION						\
  SDATA2_SECTION_FUNCTION						\
  SBSS_SECTION_FUNCTION							\
  INIT_SECTION_FUNCTION							\
  FINI_SECTION_FUNCTION

#define	TOC_SECTION_FUNCTION						\
void									\
toc_section (void)							\
{									\
  if (in_section != in_toc)						\
    {									\
      in_section = in_toc;						\
      if (DEFAULT_ABI == ABI_AIX					\
	  && TARGET_MINIMAL_TOC						\
	  && !TARGET_RELOCATABLE)					\
	{								\
	  if (! toc_initialized)					\
	    {								\
	      toc_initialized = 1;					\
	      fprintf (asm_out_file, "%s\n", TOC_SECTION_ASM_OP);	\
	      (*targetm.asm_out.internal_label) (asm_out_file, "LCTOC", 0); \
	      fprintf (asm_out_file, "\t.tc ");				\
	      ASM_OUTPUT_INTERNAL_LABEL_PREFIX (asm_out_file, "LCTOC1[TC],"); \
	      ASM_OUTPUT_INTERNAL_LABEL_PREFIX (asm_out_file, "LCTOC1"); \
	      fprintf (asm_out_file, "\n");				\
									\
	      fprintf (asm_out_file, "%s\n", MINIMAL_TOC_SECTION_ASM_OP); \
	      ASM_OUTPUT_INTERNAL_LABEL_PREFIX (asm_out_file, "LCTOC1"); \
	      fprintf (asm_out_file, " = .+32768\n");			\
	    }								\
	  else								\
	    fprintf (asm_out_file, "%s\n", MINIMAL_TOC_SECTION_ASM_OP);	\
	}								\
      else if (DEFAULT_ABI == ABI_AIX && !TARGET_RELOCATABLE)		\
	fprintf (asm_out_file, "%s\n", TOC_SECTION_ASM_OP);		\
      else								\
	{								\
	  fprintf (asm_out_file, "%s\n", MINIMAL_TOC_SECTION_ASM_OP);	\
	  if (! toc_initialized)					\
	    {								\
	      ASM_OUTPUT_INTERNAL_LABEL_PREFIX (asm_out_file, "LCTOC1"); \
	      fprintf (asm_out_file, " = .+32768\n");			\
	      toc_initialized = 1;					\
	    }								\
	}								\
    }									\
}									\
									\
extern int in_toc_section (void);					\
int in_toc_section (void)						\
{									\
  return in_section == in_toc;						\
}

#define	SDATA_SECTION_FUNCTION						\
void									\
sdata_section (void)							\
{									\
  if (in_section != in_sdata)						\
    {									\
      in_section = in_sdata;						\
      fprintf (asm_out_file, "%s\n", SDATA_SECTION_ASM_OP);		\
    }									\
}

#define	SDATA2_SECTION_FUNCTION						\
void									\
sdata2_section (void)							\
{									\
  if (in_section != in_sdata2)						\
    {									\
      in_section = in_sdata2;						\
      fprintf (asm_out_file, "%s\n", SDATA2_SECTION_ASM_OP);		\
    }									\
}

#define	SBSS_SECTION_FUNCTION						\
void									\
sbss_section (void)							\
{									\
  if (in_section != in_sbss)						\
    {									\
      in_section = in_sbss;						\
      fprintf (asm_out_file, "%s\n", SBSS_SECTION_ASM_OP);		\
    }									\
}

#define	INIT_SECTION_FUNCTION						\
void									\
init_section (void)							\
{									\
  if (in_section != in_init)						\
    {									\
      in_section = in_init;						\
      fprintf (asm_out_file, "%s\n", INIT_SECTION_ASM_OP);		\
    }									\
}

#define	FINI_SECTION_FUNCTION						\
void									\
fini_section (void)							\
{									\
  if (in_section != in_fini)						\
    {									\
      in_section = in_fini;						\
      fprintf (asm_out_file, "%s\n", FINI_SECTION_ASM_OP);		\
    }									\
}

/* Override default elf definitions.  */
#undef	TARGET_ASM_SELECT_RTX_SECTION
#define	TARGET_ASM_SELECT_RTX_SECTION rs6000_elf_select_rtx_section
#undef	TARGET_ASM_SELECT_SECTION
#define	TARGET_ASM_SELECT_SECTION  rs6000_elf_select_section
#define TARGET_ASM_UNIQUE_SECTION  rs6000_elf_unique_section

/* Return nonzero if this entry is to be written into the constant pool
   in a special way.  We do so if this is a SYMBOL_REF, LABEL_REF or a CONST
   containing one of them.  If -mfp-in-toc (the default), we also do
   this for floating-point constants.  We actually can only do this
   if the FP formats of the target and host machines are the same, but
   we can't check that since not every file that uses
   GO_IF_LEGITIMATE_ADDRESS_P includes real.h.

   Unlike AIX, we don't key off of -mminimal-toc, but instead do not
   allow floating point constants in the TOC if -mrelocatable.  */

#undef	ASM_OUTPUT_SPECIAL_POOL_ENTRY_P
#define	ASM_OUTPUT_SPECIAL_POOL_ENTRY_P(X, MODE)			\
  (TARGET_TOC								\
   && (GET_CODE (X) == SYMBOL_REF					\
       || (GET_CODE (X) == CONST && GET_CODE (XEXP (X, 0)) == PLUS	\
	   && GET_CODE (XEXP (XEXP (X, 0), 0)) == SYMBOL_REF)		\
       || GET_CODE (X) == LABEL_REF					\
       || (GET_CODE (X) == CONST_INT 					\
	   && GET_MODE_BITSIZE (MODE) <= GET_MODE_BITSIZE (Pmode))	\
       || (!TARGET_NO_FP_IN_TOC						\
	   && !TARGET_RELOCATABLE					\
	   && GET_CODE (X) == CONST_DOUBLE				\
	   && GET_MODE_CLASS (GET_MODE (X)) == MODE_FLOAT		\
	   && BITS_PER_WORD == HOST_BITS_PER_INT)))

/* These macros generate the special .type and .size directives which
   are used to set the corresponding fields of the linker symbol table
   entries in an ELF object file under SVR4.  These macros also output
   the starting labels for the relevant functions/objects.  */

/* Write the extra assembler code needed to declare a function properly.
   Some svr4 assemblers need to also have something extra said about the
   function's return value.  We allow for that here.  */

extern int rs6000_pic_labelno;

/* Override elfos.h definition.  */
#undef	ASM_DECLARE_FUNCTION_NAME
#define ASM_DECLARE_FUNCTION_NAME(FILE, NAME, DECL)			\
  rs6000_elf_declare_function_name ((FILE), (NAME), (DECL))

/* The USER_LABEL_PREFIX stuff is affected by the -fleading-underscore
   flag.  The LOCAL_LABEL_PREFIX variable is used by dbxelf.h.  */

#define	LOCAL_LABEL_PREFIX "."
#define	USER_LABEL_PREFIX ""

/* svr4.h overrides (*targetm.asm_out.internal_label).  */

#define	ASM_OUTPUT_INTERNAL_LABEL_PREFIX(FILE,PREFIX)	\
  asm_fprintf (FILE, "%L%s", PREFIX)

/* Globalizing directive for a label.  */
#define GLOBAL_ASM_OP "\t.globl "

/* This says how to output assembler code to declare an
   uninitialized internal linkage data object.  Under SVR4,
   the linker seems to want the alignment of data objects
   to depend on their types.  We do exactly that here.  */

#define	LOCAL_ASM_OP	"\t.local\t"

#define	LCOMM_ASM_OP	"\t.lcomm\t"

/* Override elfos.h definition.  */
#undef	ASM_OUTPUT_ALIGNED_LOCAL
#define	ASM_OUTPUT_ALIGNED_LOCAL(FILE, NAME, SIZE, ALIGN)		\
do {									\
  if (rs6000_sdata != SDATA_NONE && (SIZE) > 0				\
      && (SIZE) <= g_switch_value)					\
    {									\
      sbss_section ();							\
      ASM_OUTPUT_ALIGN (FILE, exact_log2 (ALIGN / BITS_PER_UNIT));	\
      ASM_OUTPUT_LABEL (FILE, NAME);					\
      ASM_OUTPUT_SKIP (FILE, SIZE);					\
      if (!flag_inhibit_size_directive && (SIZE) > 0)			\
	ASM_OUTPUT_SIZE_DIRECTIVE (FILE, NAME, SIZE);			\
    }									\
  else									\
    {									\
      fprintf (FILE, "%s", LCOMM_ASM_OP);				\
      assemble_name ((FILE), (NAME));					\
      fprintf ((FILE), ","HOST_WIDE_INT_PRINT_UNSIGNED",%u\n",		\
	       (SIZE), (ALIGN) / BITS_PER_UNIT);			\
    }									\
  ASM_OUTPUT_TYPE_DIRECTIVE (FILE, NAME, "object");			\
} while (0)

/* Describe how to emit uninitialized external linkage items.  */
#define	ASM_OUTPUT_ALIGNED_BSS(FILE, DECL, NAME, SIZE, ALIGN)		\
do {									\
  ASM_OUTPUT_ALIGNED_LOCAL (FILE, NAME, SIZE, ALIGN);			\
} while (0)

#ifdef HAVE_GAS_MAX_SKIP_P2ALIGN
/* To support -falign-* switches we need to use .p2align so
   that alignment directives in code sections will be padded
   with no-op instructions, rather than zeroes.  */
#define ASM_OUTPUT_MAX_SKIP_ALIGN(FILE,LOG,MAX_SKIP)			\
  if ((LOG) != 0)							\
    {									\
      if ((MAX_SKIP) == 0)						\
	fprintf ((FILE), "\t.p2align %d\n", (LOG));			\
      else								\
	fprintf ((FILE), "\t.p2align %d,,%d\n",	(LOG), (MAX_SKIP));	\
    }
#endif

/* This is how to output code to push a register on the stack.
   It need not be very fast code.

   On the rs6000, we must keep the backchain up to date.  In order
   to simplify things, always allocate 16 bytes for a push (System V
   wants to keep stack aligned to a 16 byte boundary).  */

#define	ASM_OUTPUT_REG_PUSH(FILE, REGNO)				\
do {									\
  if (DEFAULT_ABI == ABI_V4)						\
    asm_fprintf (FILE,							\
		 "\t{stu|stwu} %s,-16(%s)\n\t{st|stw} %s,12(%s)\n",	\
		 reg_names[1], reg_names[1], reg_names[REGNO],		\
		 reg_names[1]);						\
} while (0)

/* This is how to output an insn to pop a register from the stack.
   It need not be very fast code.  */

#define	ASM_OUTPUT_REG_POP(FILE, REGNO)					\
do {									\
  if (DEFAULT_ABI == ABI_V4)						\
    asm_fprintf (FILE,							\
		 "\t{l|lwz} %s,12(%s)\n\t{ai|addic} %s,%s,16\n",	\
		 reg_names[REGNO], reg_names[1], reg_names[1],		\
		 reg_names[1]);						\
} while (0)

/* Switch  Recognition by gcc.c.  Add -G xx support.  */

/* Override svr4.h definition.  */
#undef	SWITCH_TAKES_ARG
#define	SWITCH_TAKES_ARG(CHAR)						\
  ((CHAR) == 'D' || (CHAR) == 'U' || (CHAR) == 'o'			\
   || (CHAR) == 'e' || (CHAR) == 'T' || (CHAR) == 'u'			\
   || (CHAR) == 'I' || (CHAR) == 'm' || (CHAR) == 'x'			\
   || (CHAR) == 'L' || (CHAR) == 'A' || (CHAR) == 'V'			\
   || (CHAR) == 'B' || (CHAR) == 'b' || (CHAR) == 'G')

extern int fixuplabelno;

/* Handle constructors specially for -mrelocatable.  */
#define TARGET_ASM_CONSTRUCTOR  rs6000_elf_asm_out_constructor
#define TARGET_ASM_DESTRUCTOR   rs6000_elf_asm_out_destructor

/* This is the end of what might become sysv4.h.  */

/* Use DWARF 2 debugging information by default.  */
#undef  PREFERRED_DEBUGGING_TYPE
#define PREFERRED_DEBUGGING_TYPE DWARF2_DEBUG

/* Historically we have also supported stabs debugging.  */
#define DBX_DEBUGGING_INFO 1

#define DBX_REGISTER_NUMBER(REGNO) rs6000_dbx_register_number (REGNO)

/* Map register numbers held in the call frame info that gcc has
   collected using DWARF_FRAME_REGNUM to those that should be output in
   .debug_frame and .eh_frame.  We continue to use gcc hard reg numbers
   for .eh_frame, but use the numbers mandated by the various ABIs for
   .debug_frame.  rs6000_emit_prologue has translated any combination of
   CR2, CR3, CR4 saves to a save of CR2.  The actual code emitted saves
   the whole of CR, so we map CR2_REGNO to the DWARF reg for CR.  */
#define DWARF2_FRAME_REG_OUT(REGNO, FOR_EH)	\
  ((FOR_EH) ? (REGNO)				\
   : (REGNO) == CR2_REGNO ? 64			\
   : DBX_REGISTER_NUMBER (REGNO))

#define TARGET_ENCODE_SECTION_INFO  rs6000_elf_encode_section_info
#define TARGET_IN_SMALL_DATA_P  rs6000_elf_in_small_data_p
#define TARGET_SECTION_TYPE_FLAGS  rs6000_elf_section_type_flags

/* The ELF version doesn't encode [DS] or whatever at the end of symbols.  */

#define	RS6000_OUTPUT_BASENAME(FILE, NAME)	\
    assemble_name (FILE, NAME)

/* We have to output the stabs for the function name *first*, before
   outputting its label.  */

#define	DBX_FUNCTION_FIRST

/* This is the end of what might become sysv4dbx.h.  */

#ifndef	TARGET_VERSION
#define	TARGET_VERSION fprintf (stderr, " (PowerPC System V.4)");
#endif

#define TARGET_OS_SYSV_CPP_BUILTINS()	  \
  do                                      \
    {                                     \
      if (flag_pic == 1)		  \
        {				  \
	  builtin_define ("__pic__=1");	  \
	  builtin_define ("__PIC__=1");	  \
        }				  \
      else if (flag_pic == 2)		  \
        {				  \
	  builtin_define ("__pic__=2");	  \
	  builtin_define ("__PIC__=2");	  \
        }				  \
      if (target_flags_explicit		  \
	  & MASK_RELOCATABLE)		  \
	builtin_define ("_RELOCATABLE");  \
    }                                     \
  while (0)

#ifndef	TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS()          \
  do                                      \
    {                                     \
      builtin_define_std ("PPC");         \
      builtin_define_std ("unix");        \
      builtin_define ("__svr4__");        \
      builtin_assert ("system=unix");     \
      builtin_assert ("system=svr4");     \
      builtin_assert ("cpu=powerpc");     \
      builtin_assert ("machine=powerpc"); \
      TARGET_OS_SYSV_CPP_BUILTINS ();	  \
    }                                     \
  while (0)
#endif

/* Pass various options to the assembler.  */
/* Override svr4.h definition.  */
#undef	ASM_SPEC
#define	ASM_SPEC "%(asm_cpu) \
%{.s: %{mregnames} %{mno-regnames}} %{.S: %{mregnames} %{mno-regnames}} \
%{v:-V} %{Qy:} %{!Qn:-Qy} %{n} %{T} %{Ym,*} %{Yd,*} %{Wa,*:%*} \
%{mrelocatable} %{mrelocatable-lib} %{fpic|fpie|fPIC|fPIE:-K PIC} \
%{memb|msdata|msdata=eabi: -memb} \
%{mlittle|mlittle-endian:-mlittle; \
  mbig|mbig-endian      :-mbig;    \
  mcall-aixdesc |		   \
  mcall-freebsd |		   \
  mcall-netbsd  |		   \
  mcall-openbsd |		   \
  mcall-linux   |		   \
  mcall-gnu             :-mbig;    \
  mcall-i960-old        :-mlittle}"

#define	CC1_ENDIAN_BIG_SPEC ""

#define	CC1_ENDIAN_LITTLE_SPEC "\
%{!mstrict-align: %{!mno-strict-align: \
    %{!mcall-i960-old: \
	-mstrict-align \
    } \
}}"

#define	CC1_ENDIAN_DEFAULT_SPEC "%(cc1_endian_big)"

/* Pass -G xxx to the compiler and set correct endian mode.  */
#define	CC1_SPEC "%{G*} \
%{mlittle|mlittle-endian: %(cc1_endian_little);           \
  mbig   |mbig-endian   : %(cc1_endian_big);              \
  mcall-aixdesc |					  \
  mcall-freebsd |					  \
  mcall-netbsd  |					  \
  mcall-openbsd |					  \
  mcall-linux   |					  \
  mcall-gnu             : -mbig %(cc1_endian_big);        \
  mcall-i960-old        : -mlittle %(cc1_endian_little);  \
                        : %(cc1_endian_default)}          \
%{mno-sdata: -msdata=none } \
%{meabi: %{!mcall-*: -mcall-sysv }} \
%{!meabi: %{!mno-eabi: \
    %{mrelocatable: -meabi } \
    %{mcall-freebsd: -mno-eabi } \
    %{mcall-i960-old: -meabi } \
    %{mcall-linux: -mno-eabi } \
    %{mcall-gnu: -mno-eabi } \
    %{mcall-netbsd: -mno-eabi } \
    %{mcall-openbsd: -mno-eabi }}} \
%{msdata: -msdata=default} \
%{mno-sdata: -msdata=none} \
%{profile: -p}"

/* Don't put -Y P,<path> for cross compilers.  */
#ifndef CROSS_COMPILE
#define LINK_PATH_SPEC "\
%{!R*:%{L*:-R %*}} \
%{!nostdlib: %{!YP,*: \
    %{compat-bsd: \
	%{p:-Y P,/usr/ucblib:/usr/ccs/lib/libp:/usr/lib/libp:/usr/ccs/lib:/usr/lib} \
	%{!p:-Y P,/usr/ucblib:/usr/ccs/lib:/usr/lib}} \
	%{!R*: %{!L*: -R /usr/ucblib}} \
    %{!compat-bsd: \
	%{p:-Y P,/usr/ccs/lib/libp:/usr/lib/libp:/usr/ccs/lib:/usr/lib} \
	%{!p:-Y P,/usr/ccs/lib:/usr/lib}}}}"

#else
#define LINK_PATH_SPEC ""
#endif

/* Default starting address if specified.  */
#define LINK_START_SPEC "\
%{mads         : %(link_start_ads)         ; \
  myellowknife : %(link_start_yellowknife) ; \
  mmvme        : %(link_start_mvme)        ; \
  msim         : %(link_start_sim)         ; \
  mwindiss     : %(link_start_windiss)     ; \
  mcall-freebsd: %(link_start_freebsd)     ; \
  mcall-linux  : %(link_start_linux)       ; \
  mcall-gnu    : %(link_start_gnu)         ; \
  mcall-netbsd : %(link_start_netbsd)      ; \
  mcall-openbsd: %(link_start_openbsd)     ; \
               : %(link_start_default)     }"

#define LINK_START_DEFAULT_SPEC ""

/* Override svr4.h definition.  */
#undef	LINK_SPEC
#define	LINK_SPEC "\
%{h*} %{v:-V} %{!msdata=none:%{G*}} %{msdata=none:-G0} \
%{YP,*} %{R*} \
%{Qy:} %{!Qn:-Qy} \
%(link_shlib) \
%{!Wl,-T*: %{!T*: %(link_start) }} \
%(link_target) \
%(link_os)"

/* For now, turn off shared libraries by default.  */
#ifndef SHARED_LIB_SUPPORT
#define NO_SHARED_LIB_SUPPORT
#endif

#ifndef NO_SHARED_LIB_SUPPORT
/* Shared libraries are default.  */
#define LINK_SHLIB_SPEC "\
%{!static: %(link_path) %{!R*:%{L*:-R %*}}} \
%{mshlib: } \
%{static:-dn -Bstatic} \
%{shared:-G -dy -z text} \
%{symbolic:-Bsymbolic -G -dy -z text}"

#else
/* Shared libraries are not default.  */
#define LINK_SHLIB_SPEC "\
%{mshlib: %(link_path) } \
%{!mshlib: %{!shared: %{!symbolic: -dn -Bstatic}}} \
%{static: } \
%{shared:-G -dy -z text %(link_path) } \
%{symbolic:-Bsymbolic -G -dy -z text %(link_path) }"
#endif

/* Override the default target of the linker.  */
#define	LINK_TARGET_SPEC "\
%{mlittle: --oformat elf32-powerpcle } %{mlittle-endian: --oformat elf32-powerpcle } \
%{!mlittle: %{!mlittle-endian: %{!mbig: %{!mbig-endian: \
    %{mcall-i960-old: --oformat elf32-powerpcle} \
  }}}}"

/* Any specific OS flags.  */
#define LINK_OS_SPEC "\
%{mads         : %(link_os_ads)         ; \
  myellowknife : %(link_os_yellowknife) ; \
  mmvme        : %(link_os_mvme)        ; \
  msim         : %(link_os_sim)         ; \
  mwindiss     : %(link_os_windiss)     ; \
  mcall-freebsd: %(link_os_freebsd)     ; \
  mcall-linux  : %(link_os_linux)       ; \
  mcall-gnu    : %(link_os_gnu)         ; \
  mcall-netbsd : %(link_os_netbsd)      ; \
  mcall-openbsd: %(link_os_openbsd)     ; \
               : %(link_os_default)     }"

#define LINK_OS_DEFAULT_SPEC ""

/* Override rs6000.h definition.  */
#undef	CPP_SPEC
#define	CPP_SPEC "%{posix: -D_POSIX_SOURCE} \
%{mads         : %(cpp_os_ads)         ; \
  myellowknife : %(cpp_os_yellowknife) ; \
  mmvme        : %(cpp_os_mvme)        ; \
  msim         : %(cpp_os_sim)         ; \
  mwindiss     : %(cpp_os_windiss)     ; \
  mcall-freebsd: %(cpp_os_freebsd)     ; \
  mcall-linux  : %(cpp_os_linux)       ; \
  mcall-gnu    : %(cpp_os_gnu)         ; \
  mcall-netbsd : %(cpp_os_netbsd)      ; \
  mcall-openbsd: %(cpp_os_openbsd)     ; \
               : %(cpp_os_default)     }"

#define	CPP_OS_DEFAULT_SPEC ""

/* Override svr4.h definition.  */
#undef	STARTFILE_SPEC
#define	STARTFILE_SPEC "\
%{mads         : %(startfile_ads)         ; \
  myellowknife : %(startfile_yellowknife) ; \
  mmvme        : %(startfile_mvme)        ; \
  msim         : %(startfile_sim)         ; \
  mwindiss     : %(startfile_windiss)     ; \
  mcall-freebsd: %(startfile_freebsd)     ; \
  mcall-linux  : %(startfile_linux)       ; \
  mcall-gnu    : %(startfile_gnu)         ; \
  mcall-netbsd : %(startfile_netbsd)      ; \
  mcall-openbsd: %(startfile_openbsd)     ; \
               : %(startfile_default)     }"

#define	STARTFILE_DEFAULT_SPEC ""

/* Override svr4.h definition.  */
#undef	LIB_SPEC
#define	LIB_SPEC "\
%{mads         : %(lib_ads)         ; \
  myellowknife : %(lib_yellowknife) ; \
  mmvme        : %(lib_mvme)        ; \
  msim         : %(lib_sim)         ; \
  mwindiss     : %(lib_windiss)     ; \
  mcall-freebsd: %(lib_freebsd)     ; \
  mcall-linux  : %(lib_linux)       ; \
  mcall-gnu    : %(lib_gnu)         ; \
  mcall-netbsd : %(lib_netbsd)      ; \
  mcall-openbsd: %(lib_openbsd)     ; \
               : %(lib_default)     }"

#define LIB_DEFAULT_SPEC ""

/* Override svr4.h definition.  */
#undef	ENDFILE_SPEC
#define	ENDFILE_SPEC "\
%{mads         : crtsavres.o%s        %(endfile_ads)         ; \
  myellowknife : crtsavres.o%s        %(endfile_yellowknife) ; \
  mmvme        : crtsavres.o%s        %(endfile_mvme)        ; \
  msim         : crtsavres.o%s        %(endfile_sim)         ; \
  mwindiss     :                      %(endfile_windiss)     ; \
  mcall-freebsd: crtsavres.o%s        %(endfile_freebsd)     ; \
  mcall-linux  : crtsavres.o%s        %(endfile_linux)       ; \
  mcall-gnu    : crtsavres.o%s        %(endfile_gnu)         ; \
  mcall-netbsd : crtsavres.o%s        %(endfile_netbsd)      ; \
  mcall-openbsd: crtsavres.o%s        %(endfile_openbsd)     ; \
               : %(crtsavres_default) %(endfile_default)     }"

#define CRTSAVRES_DEFAULT_SPEC "crtsavres.o%s"

#define	ENDFILE_DEFAULT_SPEC ""

/* Motorola ADS support.  */
#define LIB_ADS_SPEC "--start-group -lads -lc --end-group"

#define	STARTFILE_ADS_SPEC "ecrti.o%s crt0.o%s crtbegin.o%s"

#define	ENDFILE_ADS_SPEC "crtend.o%s ecrtn.o%s"

#define LINK_START_ADS_SPEC "-T ads.ld%s"

#define LINK_OS_ADS_SPEC ""

#define CPP_OS_ADS_SPEC ""

/* Motorola Yellowknife support.  */
#define LIB_YELLOWKNIFE_SPEC "--start-group -lyk -lc --end-group"

#define	STARTFILE_YELLOWKNIFE_SPEC "ecrti.o%s crt0.o%s crtbegin.o%s"

#define	ENDFILE_YELLOWKNIFE_SPEC "crtend.o%s ecrtn.o%s"

#define LINK_START_YELLOWKNIFE_SPEC "-T yellowknife.ld%s"

#define LINK_OS_YELLOWKNIFE_SPEC ""

#define CPP_OS_YELLOWKNIFE_SPEC ""

/* Motorola MVME support.  */
#define LIB_MVME_SPEC "--start-group -lmvme -lc --end-group"

#define	STARTFILE_MVME_SPEC "ecrti.o%s crt0.o%s crtbegin.o%s"

#define	ENDFILE_MVME_SPEC "crtend.o%s ecrtn.o%s"

#define LINK_START_MVME_SPEC "-Ttext 0x40000"

#define LINK_OS_MVME_SPEC ""

#define CPP_OS_MVME_SPEC ""

/* PowerPC simulator based on netbsd system calls support.  */
#define LIB_SIM_SPEC "--start-group -lsim -lc --end-group"

#define	STARTFILE_SIM_SPEC "ecrti.o%s sim-crt0.o%s crtbegin.o%s"

#define	ENDFILE_SIM_SPEC "crtend.o%s ecrtn.o%s"

#define LINK_START_SIM_SPEC ""

#define LINK_OS_SIM_SPEC "-m elf32ppcsim"

#define CPP_OS_SIM_SPEC ""

/* FreeBSD support.  */

#define CPP_OS_FREEBSD_SPEC	"\
  -D__PPC__ -D__ppc__ -D__PowerPC__ -D__powerpc__ \
  -Acpu=powerpc -Amachine=powerpc"

#define	STARTFILE_FREEBSD_SPEC	FBSD_STARTFILE_SPEC
#define ENDFILE_FREEBSD_SPEC	FBSD_ENDFILE_SPEC
#define LIB_FREEBSD_SPEC	FBSD_LIB_SPEC
#define LINK_START_FREEBSD_SPEC	""

#define LINK_OS_FREEBSD_SPEC "\
  %{p:%nconsider using `-pg' instead of `-p' with gprof(1)} \
  %{Wl,*:%*} \
  %{v:-V} \
  %{assert*} %{R*} %{rpath*} %{defsym*} \
  %{shared:-Bshareable %{h*} %{soname*}} \
  %{!shared: \
    %{!static: \
      %{rdynamic: -export-dynamic} \
      %{!dynamic-linker:-dynamic-linker %(fbsd_dynamic_linker) }} \
    %{static:-Bstatic}} \
  %{symbolic:-Bsymbolic}"

/* GNU/Linux support.  */
#define LIB_LINUX_SPEC "%{mnewlib: --start-group -llinux -lc --end-group } \
%{!mnewlib: %{pthread:-lpthread} %{shared:-lc} \
%{!shared: %{profile:-lc_p} %{!profile:-lc}}}"

#ifdef HAVE_LD_PIE
#define	STARTFILE_LINUX_SPEC "\
%{!shared: %{pg|p:gcrt1.o%s;pie:Scrt1.o%s;:crt1.o%s}} \
%{mnewlib:ecrti.o%s;:crti.o%s} \
%{static:crtbeginT.o%s;shared|pie:crtbeginS.o%s;:crtbegin.o%s}"
#else
#define	STARTFILE_LINUX_SPEC "\
%{!shared: %{pg|p:gcrt1.o%s;:crt1.o%s}} \
%{mnewlib:ecrti.o%s;:crti.o%s} \
%{static:crtbeginT.o%s;shared|pie:crtbeginS.o%s;:crtbegin.o%s}"
#endif

#define	ENDFILE_LINUX_SPEC "\
%{shared|pie:crtendS.o%s;:crtend.o%s} \
%{mnewlib:ecrtn.o%s;:crtn.o%s}"

#define LINK_START_LINUX_SPEC ""

#define LINK_OS_LINUX_SPEC "-m elf32ppclinux %{!shared: %{!static: \
  %{rdynamic:-export-dynamic} \
  %{!dynamic-linker:-dynamic-linker /lib/ld.so.1}}}"

#if defined(HAVE_LD_EH_FRAME_HDR)
# define LINK_EH_SPEC "%{!static:--eh-frame-hdr} "
#endif

#define CPP_OS_LINUX_SPEC "-D__unix__ -D__gnu_linux__ -D__linux__ \
%{!undef:							  \
  %{!ansi:							  \
    %{!std=*:-Dunix -D__unix -Dlinux -D__linux}			  \
    %{std=gnu*:-Dunix -D__unix -Dlinux -D__linux}}}		  \
-Asystem=linux -Asystem=unix -Asystem=posix %{pthread:-D_REENTRANT}"

/* GNU/Hurd support.  */
#define LIB_GNU_SPEC "%{mnewlib: --start-group -lgnu -lc --end-group } \
%{!mnewlib: %{shared:-lc} %{!shared: %{pthread:-lpthread } \
%{profile:-lc_p} %{!profile:-lc}}}"

#define	STARTFILE_GNU_SPEC "\
%{!shared: %{!static: %{pg:gcrt1.o%s} %{!pg:%{p:gcrt1.o%s} %{!p:crt1.o%s}}}} \
%{static: %{pg:gcrt0.o%s} %{!pg:%{p:gcrt0.o%s} %{!p:crt0.o%s}}} \
%{mnewlib: ecrti.o%s} %{!mnewlib: crti.o%s} \
%{!shared:crtbegin.o%s} %{shared:crtbeginS.o%s}"

#define	ENDFILE_GNU_SPEC "%{!shared:crtend.o%s} %{shared:crtendS.o%s} \
%{mnewlib: ecrtn.o%s} %{!mnewlib: crtn.o%s}"

#define LINK_START_GNU_SPEC ""

#define LINK_OS_GNU_SPEC "-m elf32ppclinux %{!shared: %{!static: \
  %{rdynamic:-export-dynamic} \
  %{!dynamic-linker:-dynamic-linker /lib/ld.so.1}}}"

#define CPP_OS_GNU_SPEC "-D__unix__ -D__gnu_hurd__ -D__GNU__	\
%{!undef:					                \
  %{!ansi: -Dunix -D__unix}}			                \
-Asystem=gnu -Asystem=unix -Asystem=posix %{pthread:-D_REENTRANT}"

/* NetBSD support.  */
#define LIB_NETBSD_SPEC "\
%{profile:-lgmon -lc_p} %{!profile:-lc}"

#define	STARTFILE_NETBSD_SPEC "\
ncrti.o%s crt0.o%s \
%{!shared:crtbegin.o%s} %{shared:crtbeginS.o%s}"

#define ENDFILE_NETBSD_SPEC "\
%{!shared:crtend.o%s} %{shared:crtendS.o%s} \
ncrtn.o%s"

#define LINK_START_NETBSD_SPEC "\
"

#define LINK_OS_NETBSD_SPEC "\
%{!shared: %{!static: \
  %{rdynamic:-export-dynamic} \
  %{!dynamic-linker:-dynamic-linker /usr/libexec/ld.elf_so}}}"

#define CPP_OS_NETBSD_SPEC "\
-D__powerpc__ -D__NetBSD__ -D__KPRINTF_ATTRIBUTE__"

/* OpenBSD support.  */
#ifndef	LIB_OPENBSD_SPEC
#define LIB_OPENBSD_SPEC "%{!shared:%{pthread:-lpthread%{p:_p}%{!p:%{pg:_p}}}} %{!shared:-lc%{p:_p}%{!p:%{pg:_p}}}"
#endif

#ifndef	STARTFILE_OPENBSD_SPEC
#define	STARTFILE_OPENBSD_SPEC "\
%{!shared: %{pg:gcrt0.o%s} %{!pg:%{p:gcrt0.o%s} %{!p:crt0.o%s}}} \
%{!shared:crtbegin.o%s} %{shared:crtbeginS.o%s}"
#endif

#ifndef	ENDFILE_OPENBSD_SPEC
#define	ENDFILE_OPENBSD_SPEC "\
%{!shared:crtend.o%s} %{shared:crtendS.o%s}"
#endif

#ifndef LINK_START_OPENBSD_SPEC
#define LINK_START_OPENBSD_SPEC "-Ttext 0x400074"
#endif

#ifndef LINK_OS_OPENBSD_SPEC
#define LINK_OS_OPENBSD_SPEC ""
#endif

#ifndef CPP_OS_OPENBSD_SPEC
#define CPP_OS_OPENBSD_SPEC "%{posix:-D_POSIX_SOURCE} %{pthread:-D_POSIX_THREADS}"
#endif

/* WindISS support.  */

#define LIB_WINDISS_SPEC "--start-group -li -lcfp -lwindiss -lram -limpl -limpfp --end-group"

#define CPP_OS_WINDISS_SPEC "\
-D__rtasim \
-D__EABI__ \
-D__ppc \
%{!msoft-float: -D__hardfp} \
"

#define STARTFILE_WINDISS_SPEC "crt0.o%s crtbegin.o%s"

#define ENDFILE_WINDISS_SPEC "crtend.o%s"

#define LINK_START_WINDISS_SPEC ""

#define LINK_OS_WINDISS_SPEC ""

/* Define any extra SPECS that the compiler needs to generate.  */
/* Override rs6000.h definition.  */
#undef	SUBTARGET_EXTRA_SPECS
#define	SUBTARGET_EXTRA_SPECS						\
  { "crtsavres_default",        CRTSAVRES_DEFAULT_SPEC },              \
  { "lib_ads",			LIB_ADS_SPEC },				\
  { "lib_yellowknife",		LIB_YELLOWKNIFE_SPEC },			\
  { "lib_mvme",			LIB_MVME_SPEC },			\
  { "lib_sim",			LIB_SIM_SPEC },				\
  { "lib_freebsd",		LIB_FREEBSD_SPEC },			\
  { "lib_gnu",			LIB_GNU_SPEC },				\
  { "lib_linux",		LIB_LINUX_SPEC },			\
  { "lib_netbsd",		LIB_NETBSD_SPEC },			\
  { "lib_openbsd",		LIB_OPENBSD_SPEC },			\
  { "lib_windiss",              LIB_WINDISS_SPEC },                     \
  { "lib_default",		LIB_DEFAULT_SPEC },			\
  { "startfile_ads",		STARTFILE_ADS_SPEC },			\
  { "startfile_yellowknife",	STARTFILE_YELLOWKNIFE_SPEC },		\
  { "startfile_mvme",		STARTFILE_MVME_SPEC },			\
  { "startfile_sim",		STARTFILE_SIM_SPEC },			\
  { "startfile_freebsd",	STARTFILE_FREEBSD_SPEC },		\
  { "startfile_gnu",		STARTFILE_GNU_SPEC },			\
  { "startfile_linux",		STARTFILE_LINUX_SPEC },			\
  { "startfile_netbsd",		STARTFILE_NETBSD_SPEC },		\
  { "startfile_openbsd",	STARTFILE_OPENBSD_SPEC },		\
  { "startfile_windiss",        STARTFILE_WINDISS_SPEC },               \
  { "startfile_default",	STARTFILE_DEFAULT_SPEC },		\
  { "endfile_ads",		ENDFILE_ADS_SPEC },			\
  { "endfile_yellowknife",	ENDFILE_YELLOWKNIFE_SPEC },		\
  { "endfile_mvme",		ENDFILE_MVME_SPEC },			\
  { "endfile_sim",		ENDFILE_SIM_SPEC },			\
  { "endfile_freebsd",		ENDFILE_FREEBSD_SPEC },			\
  { "endfile_gnu",		ENDFILE_GNU_SPEC },			\
  { "endfile_linux",		ENDFILE_LINUX_SPEC },			\
  { "endfile_netbsd",		ENDFILE_NETBSD_SPEC },			\
  { "endfile_openbsd",		ENDFILE_OPENBSD_SPEC },			\
  { "endfile_windiss",          ENDFILE_WINDISS_SPEC },                 \
  { "endfile_default",		ENDFILE_DEFAULT_SPEC },			\
  { "link_path",		LINK_PATH_SPEC },			\
  { "link_shlib",		LINK_SHLIB_SPEC },			\
  { "link_target",		LINK_TARGET_SPEC },			\
  { "link_start",		LINK_START_SPEC },			\
  { "link_start_ads",		LINK_START_ADS_SPEC },			\
  { "link_start_yellowknife",	LINK_START_YELLOWKNIFE_SPEC },		\
  { "link_start_mvme",		LINK_START_MVME_SPEC },			\
  { "link_start_sim",		LINK_START_SIM_SPEC },			\
  { "link_start_freebsd",	LINK_START_FREEBSD_SPEC },		\
  { "link_start_gnu",		LINK_START_GNU_SPEC },			\
  { "link_start_linux",		LINK_START_LINUX_SPEC },		\
  { "link_start_netbsd",	LINK_START_NETBSD_SPEC },		\
  { "link_start_openbsd",	LINK_START_OPENBSD_SPEC },		\
  { "link_start_windiss",	LINK_START_WINDISS_SPEC },		\
  { "link_start_default",	LINK_START_DEFAULT_SPEC },		\
  { "link_os",			LINK_OS_SPEC },				\
  { "link_os_ads",		LINK_OS_ADS_SPEC },			\
  { "link_os_yellowknife",	LINK_OS_YELLOWKNIFE_SPEC },		\
  { "link_os_mvme",		LINK_OS_MVME_SPEC },			\
  { "link_os_sim",		LINK_OS_SIM_SPEC },			\
  { "link_os_freebsd",		LINK_OS_FREEBSD_SPEC },			\
  { "link_os_linux",		LINK_OS_LINUX_SPEC },			\
  { "link_os_gnu",		LINK_OS_GNU_SPEC },			\
  { "link_os_netbsd",		LINK_OS_NETBSD_SPEC },			\
  { "link_os_openbsd",		LINK_OS_OPENBSD_SPEC },			\
  { "link_os_windiss",		LINK_OS_WINDISS_SPEC },			\
  { "link_os_default",		LINK_OS_DEFAULT_SPEC },			\
  { "cc1_endian_big",		CC1_ENDIAN_BIG_SPEC },			\
  { "cc1_endian_little",	CC1_ENDIAN_LITTLE_SPEC },		\
  { "cc1_endian_default",	CC1_ENDIAN_DEFAULT_SPEC },		\
  { "cpp_os_ads",		CPP_OS_ADS_SPEC },			\
  { "cpp_os_yellowknife",	CPP_OS_YELLOWKNIFE_SPEC },		\
  { "cpp_os_mvme",		CPP_OS_MVME_SPEC },			\
  { "cpp_os_sim",		CPP_OS_SIM_SPEC },			\
  { "cpp_os_freebsd",		CPP_OS_FREEBSD_SPEC },			\
  { "cpp_os_gnu",		CPP_OS_GNU_SPEC },			\
  { "cpp_os_linux",		CPP_OS_LINUX_SPEC },			\
  { "cpp_os_netbsd",		CPP_OS_NETBSD_SPEC },			\
  { "cpp_os_openbsd",		CPP_OS_OPENBSD_SPEC },			\
  { "cpp_os_windiss",           CPP_OS_WINDISS_SPEC },                  \
  { "cpp_os_default",		CPP_OS_DEFAULT_SPEC },			\
  { "fbsd_dynamic_linker",	FBSD_DYNAMIC_LINKER },			\
  SUBSUBTARGET_EXTRA_SPECS

#define	SUBSUBTARGET_EXTRA_SPECS

/* Define this macro as a C expression for the initializer of an
   array of string to tell the driver program which options are
   defaults for this target and thus do not need to be handled
   specially when using `MULTILIB_OPTIONS'.

   Do not define this macro if `MULTILIB_OPTIONS' is not defined in
   the target makefile fragment or if none of the options listed in
   `MULTILIB_OPTIONS' are set by default.  *Note Target Fragment::.  */

#define	MULTILIB_DEFAULTS { "mbig", "mcall-sysv" }

/* Define this macro if the code for function profiling should come
   before the function prologue.  Normally, the profiling code comes
   after.  */
#define PROFILE_BEFORE_PROLOGUE 1

/* Function name to call to do profiling.  */
#define RS6000_MCOUNT "_mcount"

/* Define this macro (to a value of 1) if you want to support the
   Win32 style pragmas #pragma pack(push,<n>)' and #pragma
   pack(pop)'.  The pack(push,<n>) pragma specifies the maximum
   alignment (in bytes) of fields within a structure, in much the
   same way as the __aligned__' and __packed__' __attribute__'s
   do.  A pack value of zero resets the behavior to the default.
   Successive invocations of this pragma cause the previous values to
   be stacked, so that invocations of #pragma pack(pop)' will return
   to the previous value.  */

#define HANDLE_PRAGMA_PACK_PUSH_POP 1

/* Select a format to encode pointers in exception handling data.  CODE
   is 0 for data, 1 for code labels, 2 for function pointers.  GLOBAL is
   true if the symbol may be affected by dynamic relocations.  */
#define ASM_PREFERRED_EH_DATA_FORMAT(CODE,GLOBAL)			     \
  ((flag_pic || TARGET_RELOCATABLE)					     \
   ? (((GLOBAL) ? DW_EH_PE_indirect : 0) | DW_EH_PE_pcrel | DW_EH_PE_sdata4) \
   : DW_EH_PE_absptr)

#define DOUBLE_INT_ASM_OP "\t.quad\t"

/* Generate entries in .fixup for relocatable addresses.  */
#define RELOCATABLE_NEEDS_FIXUP 1
