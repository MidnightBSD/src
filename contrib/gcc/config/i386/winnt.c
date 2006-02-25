/* Subroutines for insn-output.c for Windows NT.
   Contributed by Douglas Rupp (drupp@cs.washington.edu)
   Copyright (C) 1995, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.

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
02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "output.h"
#include "tree.h"
#include "flags.h"
#include "tm_p.h"
#include "toplev.h"
#include "hashtab.h"
#include "ggc.h"

/* i386/PE specific attribute support.

   i386/PE has two new attributes:
   dllexport - for exporting a function/variable that will live in a dll
   dllimport - for importing a function/variable from a dll

   Microsoft allows multiple declspecs in one __declspec, separating
   them with spaces.  We do NOT support this.  Instead, use __declspec
   multiple times.
*/

static tree associated_type (tree);
static const char * gen_stdcall_suffix (tree);
static const char * gen_fastcall_suffix (tree);
static int i386_pe_dllexport_p (tree);
static int i386_pe_dllimport_p (tree);
static void i386_pe_mark_dllexport (tree);
static void i386_pe_mark_dllimport (tree);

/* This is we how mark internal identifiers with dllimport or dllexport
   attributes.  */
#ifndef DLL_IMPORT_PREFIX
#define DLL_IMPORT_PREFIX "#i."
#endif
#ifndef DLL_EXPORT_PREFIX
#define DLL_EXPORT_PREFIX "#e."
#endif

/* Handle a "dllimport" or "dllexport" attribute;
   arguments as in struct attribute_spec.handler.  */
tree
ix86_handle_dll_attribute (tree * pnode, tree name, tree args, int flags,
			   bool *no_add_attrs)
{
  tree node = *pnode;

  /* These attributes may apply to structure and union types being created,
     but otherwise should pass to the declaration involved.  */
  if (!DECL_P (node))
    {
      if (flags & ((int) ATTR_FLAG_DECL_NEXT | (int) ATTR_FLAG_FUNCTION_NEXT
		   | (int) ATTR_FLAG_ARRAY_NEXT))
	{
	  *no_add_attrs = true;
	  return tree_cons (name, args, NULL_TREE);
	}
      if (TREE_CODE (node) != RECORD_TYPE && TREE_CODE (node) != UNION_TYPE)
	{
	  warning ("`%s' attribute ignored", IDENTIFIER_POINTER (name));
	  *no_add_attrs = true;
	}

      return NULL_TREE;
    }

  /* Report error on dllimport ambiguities seen now before they cause
     any damage.  */
  else if (is_attribute_p ("dllimport", name))
    {
      /* Like MS, treat definition of dllimported variables and
	 non-inlined functions on declaration as syntax errors.
	 We allow the attribute for function definitions if declared
	 inline, but just ignore it in i386_pe_dllimport_p.  */
      if (TREE_CODE (node) == FUNCTION_DECL  && DECL_INITIAL (node)
          && !DECL_INLINE (node))
	{
	  error ("%Jfunction `%D' definition is marked dllimport.", node, node);
	  *no_add_attrs = true;
	}

      else if (TREE_CODE (node) == VAR_DECL)
	{
	  if (DECL_INITIAL (node))
	    {
	      error ("%Jvariable `%D' definition is marked dllimport.",
		     node, node);
	      *no_add_attrs = true;
	    }

	  /* `extern' needn't be specified with dllimport.
	     Specify `extern' now and hope for the best.  Sigh.  */
	  DECL_EXTERNAL (node) = 1;
	  /* Also, implicitly give dllimport'd variables declared within
	     a function global scope, unless declared static.  */
	  if (current_function_decl != NULL_TREE && !TREE_STATIC (node))
	    TREE_PUBLIC (node) = 1;
	}
    }

  /*  Report error if symbol is not accessible at global scope.  */
  if (!TREE_PUBLIC (node)
      && (TREE_CODE (node) == VAR_DECL
	  || TREE_CODE (node) == FUNCTION_DECL))
    {
      error ("%Jexternal linkage required for symbol '%D' because of "
	     "'%s' attribute.", node, node, IDENTIFIER_POINTER (name));
      *no_add_attrs = true;
    }

  return NULL_TREE;
}

/* Handle a "shared" attribute;
   arguments as in struct attribute_spec.handler.  */
tree
ix86_handle_shared_attribute (tree *node, tree name,
			      tree args ATTRIBUTE_UNUSED,
			      int flags ATTRIBUTE_UNUSED, bool *no_add_attrs)
{
  if (TREE_CODE (*node) != VAR_DECL)
    {
      warning ("`%s' attribute only applies to variables",
	       IDENTIFIER_POINTER (name));
      *no_add_attrs = true;
    }

  return NULL_TREE;
}

/* Return the type that we should use to determine if DECL is
   imported or exported.  */

static tree
associated_type (tree decl)
{
  tree t = NULL_TREE;

  /* In the C++ frontend, DECL_CONTEXT for a method doesn't actually refer
     to the containing class.  So we look at the 'this' arg.  */
  if (TREE_CODE (TREE_TYPE (decl)) == METHOD_TYPE)
    {
      /* Artificial methods are not affected by the import/export status
	 of their class unless they are COMDAT.  Implicit copy ctor's and
	 dtor's are not affected by class status but virtual and
	 non-virtual thunks are.  */
      if (!DECL_ARTIFICIAL (decl) || DECL_COMDAT (decl))
	t = TYPE_MAIN_VARIANT
	  (TREE_TYPE (TREE_VALUE (TYPE_ARG_TYPES (TREE_TYPE (decl)))));
    }
  else if (DECL_CONTEXT (decl)
	   && TREE_CODE_CLASS (TREE_CODE (DECL_CONTEXT (decl))) == 't')
    t = DECL_CONTEXT (decl);

  return t;
}

/* Return nonzero if DECL is a dllexport'd object.  */

static int
i386_pe_dllexport_p (tree decl)
{
  tree exp;

  if (TREE_CODE (decl) != VAR_DECL
      && TREE_CODE (decl) != FUNCTION_DECL)
    return 0;
  exp = lookup_attribute ("dllexport", DECL_ATTRIBUTES (decl));
  if (exp)
    return 1;

  /* Class members get the dllexport status of their class.  */
  if (associated_type (decl))
    {
      exp = lookup_attribute ("dllexport",
			      TYPE_ATTRIBUTES (associated_type (decl)));
      if (exp)
	return 1;
    }

  return 0;
}

/* Return nonzero if DECL is a dllimport'd object.  */

static int
i386_pe_dllimport_p (tree decl)
{
  tree imp;
  int context_imp = 0;

  if (TREE_CODE (decl) == FUNCTION_DECL
      && TARGET_NOP_FUN_DLLIMPORT)
    return 0;

  if (TREE_CODE (decl) != VAR_DECL
      && TREE_CODE (decl) != FUNCTION_DECL)
    return 0;

  imp = lookup_attribute ("dllimport", DECL_ATTRIBUTES (decl));

  /* Class members get the dllimport status of their class.  */
  if (!imp && associated_type (decl))
    {
      imp = lookup_attribute ("dllimport",
			      TYPE_ATTRIBUTES (associated_type (decl)));
      if (imp)
	context_imp = 1;
    }

  if (imp)
    {
      /* Don't mark defined functions as dllimport.  If the definition
	 itself was marked with dllimport, than ix86_handle_dll_attribute
	 reports an error. This handles the case when the definition
	 overrides an earlier declaration.  */
      if (TREE_CODE (decl) ==  FUNCTION_DECL && DECL_INITIAL (decl)
	  && !DECL_INLINE (decl))
	{
	   /* Don't warn about artificial methods.  */
	  if (!DECL_ARTIFICIAL (decl))
	    warning ("%Jfunction '%D' is defined after prior declaration "
		     "as dllimport: attribute ignored", decl, decl);
	  return 0;
	}

      /* We ignore the dllimport attribute for inline member functions.
	 This differs from MSVC behavior which treats it like GNUC
	 'extern inline' extension.  */
      else if (TREE_CODE (decl) == FUNCTION_DECL && DECL_INLINE (decl))
        {
	  if (extra_warnings)
	    warning ("%Jinline function '%D' is declared as dllimport: "
		     "attribute ignored.", decl, decl);
	  return 0;
	}

      /*  Don't allow definitions of static data members in dllimport class,
	  Just ignore attribute for vtable data.  */
      else if (TREE_CODE (decl) == VAR_DECL
	       && TREE_STATIC (decl) && TREE_PUBLIC (decl)
	       && !DECL_EXTERNAL (decl) && context_imp)
	{
	  if (!DECL_VIRTUAL_P (decl))
            error ("%Jdefinition of static data member '%D' of "
		   "dllimport'd class.", decl, decl);
	  return 0;
	}

      /* Since we can't treat a pointer to a dllimport'd symbol as a
	 constant address, we turn off the attribute on C++ virtual
	 methods to allow creation of vtables using thunks.  Don't mark
	 artificial methods either (in associated_type, only COMDAT
	 artificial method get import status from class context).  */
      else if (TREE_CODE (TREE_TYPE (decl)) == METHOD_TYPE
	       && (DECL_VIRTUAL_P (decl) || DECL_ARTIFICIAL (decl)))
	return 0;

      return 1;
    }

  return 0;
}

/* Return nonzero if SYMBOL is marked as being dllexport'd.  */

int
i386_pe_dllexport_name_p (const char *symbol)
{
  return (strncmp (DLL_EXPORT_PREFIX, symbol,
		   strlen (DLL_EXPORT_PREFIX)) == 0);
}

/* Return nonzero if SYMBOL is marked as being dllimport'd.  */

int
i386_pe_dllimport_name_p (const char *symbol)
{
  return (strncmp (DLL_IMPORT_PREFIX, symbol,
		   strlen (DLL_IMPORT_PREFIX)) == 0);
}

/* Mark a DECL as being dllexport'd.
   Note that we override the previous setting (eg: dllimport).  */

static void
i386_pe_mark_dllexport (tree decl)
{
  const char *oldname;
  char  *newname;
  rtx rtlname;
  tree idp;

  rtlname = XEXP (DECL_RTL (decl), 0);
  if (GET_CODE (rtlname) == SYMBOL_REF)
    oldname = XSTR (rtlname, 0);
  else if (GET_CODE (rtlname) == MEM
	   && GET_CODE (XEXP (rtlname, 0)) == SYMBOL_REF)
    oldname = XSTR (XEXP (rtlname, 0), 0);
  else
    abort ();
  if (i386_pe_dllimport_name_p (oldname))
    {
      warning ("%Jinconsistent dll linkage for '%D', dllexport assumed.",
	       decl, decl);
     /* Remove DLL_IMPORT_PREFIX.  */
      oldname += strlen (DLL_IMPORT_PREFIX);
      DECL_NON_ADDR_CONST_P (decl) = 0;
    }
  else if (i386_pe_dllexport_name_p (oldname))
    return;  /*  already done  */

  newname = alloca (strlen (DLL_EXPORT_PREFIX) + strlen (oldname) + 1);
  sprintf (newname, "%s%s", DLL_EXPORT_PREFIX, oldname);

  /* We pass newname through get_identifier to ensure it has a unique
     address.  RTL processing can sometimes peek inside the symbol ref
     and compare the string's addresses to see if two symbols are
     identical.  */
  idp = get_identifier (newname);

  XEXP (DECL_RTL (decl), 0) =
    gen_rtx (SYMBOL_REF, Pmode, IDENTIFIER_POINTER (idp));
}

/* Mark a DECL as being dllimport'd.  */

static void
i386_pe_mark_dllimport (tree decl)
{
  const char *oldname;
  char  *newname;
  tree idp;
  rtx rtlname, newrtl;

  rtlname = XEXP (DECL_RTL (decl), 0);
  if (GET_CODE (rtlname) == SYMBOL_REF)
    oldname = XSTR (rtlname, 0);
  else if (GET_CODE (rtlname) == MEM
	   && GET_CODE (XEXP (rtlname, 0)) == SYMBOL_REF)
    oldname = XSTR (XEXP (rtlname, 0), 0);
  else
    abort ();
  if (i386_pe_dllexport_name_p (oldname))
    {
      error ("`%s' declared as both exported to and imported from a DLL",
             IDENTIFIER_POINTER (DECL_NAME (decl)));
      return;
    }
  else if (i386_pe_dllimport_name_p (oldname))
    {
      /* Already done, but do a sanity check to prevent assembler errors.  */
      if (!DECL_EXTERNAL (decl) || !TREE_PUBLIC (decl))
	{
	  error ("%Jfailure in redeclaration of '%D': dllimport'd "
		 "symbol lacks external linkage.", decl, decl);
	  abort();
	}
      return;
    }

  newname = alloca (strlen (DLL_IMPORT_PREFIX) + strlen (oldname) + 1);
  sprintf (newname, "%s%s", DLL_IMPORT_PREFIX, oldname);

  /* We pass newname through get_identifier to ensure it has a unique
     address.  RTL processing can sometimes peek inside the symbol ref
     and compare the string's addresses to see if two symbols are
     identical.  */
  idp = get_identifier (newname);

  newrtl = gen_rtx (MEM, Pmode,
		    gen_rtx (SYMBOL_REF, Pmode,
			     IDENTIFIER_POINTER (idp)));
  XEXP (DECL_RTL (decl), 0) = newrtl;

  /* Can't treat a pointer to this as a constant address */
  DECL_NON_ADDR_CONST_P (decl) = 1;
}

/* Return string which is the former assembler name modified with a
   prefix consisting of FASTCALL_PREFIX and a suffix consisting of an
   atsign (@) followed by the number of bytes of arguments.  */

static const char *
gen_fastcall_suffix (tree decl)
{
  int total = 0;
  const char *asmname = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl));
  char *newsym;

  if (TYPE_ARG_TYPES (TREE_TYPE (decl)))
    if (TREE_VALUE (tree_last (TYPE_ARG_TYPES (TREE_TYPE (decl))))
        == void_type_node)
      {
	tree formal_type = TYPE_ARG_TYPES (TREE_TYPE (decl));

	/* Quit if we hit an incomplete type.  Error is reported
	   by convert_arguments in c-typeck.c or cp/typeck.c.  */
	while (TREE_VALUE (formal_type) != void_type_node
	       && COMPLETE_TYPE_P (TREE_VALUE (formal_type)))	
	  {
	    int parm_size
	      = TREE_INT_CST_LOW (TYPE_SIZE (TREE_VALUE (formal_type)));
	    /* Must round up to include padding.  This is done the same
	       way as in store_one_arg.  */
	    parm_size = ((parm_size + PARM_BOUNDARY - 1)
			 / PARM_BOUNDARY * PARM_BOUNDARY);
	    total += parm_size;
	    formal_type = TREE_CHAIN (formal_type);
	  }
      }

  /* Assume max of 8 base 10 digits in the suffix.  */
  newsym = xmalloc (1 + strlen (asmname) + 1 + 8 + 1);
  sprintf (newsym, "%c%s@%d", FASTCALL_PREFIX, asmname, total/BITS_PER_UNIT);
  return IDENTIFIER_POINTER (get_identifier (newsym));
}

/* Return string which is the former assembler name modified with a
   suffix consisting of an atsign (@) followed by the number of bytes of
   arguments */

static const char *
gen_stdcall_suffix (tree decl)
{
  int total = 0;
  /* ??? This probably should use XSTR (XEXP (DECL_RTL (decl), 0), 0) instead
     of DECL_ASSEMBLER_NAME.  */
  const char *asmname = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl));
  char *newsym;

  if (TYPE_ARG_TYPES (TREE_TYPE (decl)))
    if (TREE_VALUE (tree_last (TYPE_ARG_TYPES (TREE_TYPE (decl))))
        == void_type_node)
      {
	tree formal_type = TYPE_ARG_TYPES (TREE_TYPE (decl));

	/* Quit if we hit an incomplete type.  Error is reported
	   by convert_arguments in c-typeck.c or cp/typeck.c.  */
	while (TREE_VALUE (formal_type) != void_type_node
	       && COMPLETE_TYPE_P (TREE_VALUE (formal_type)))	
	  {
	    int parm_size
	      = TREE_INT_CST_LOW (TYPE_SIZE (TREE_VALUE (formal_type)));
	    /* Must round up to include padding.  This is done the same
	       way as in store_one_arg.  */
	    parm_size = ((parm_size + PARM_BOUNDARY - 1)
			 / PARM_BOUNDARY * PARM_BOUNDARY);
	    total += parm_size;
	    formal_type = TREE_CHAIN (formal_type);
	  }
      }

  /* Assume max of 8 base 10 digits in the suffix.  */
  newsym = xmalloc (strlen (asmname) + 1 + 8 + 1);
  sprintf (newsym, "%s@%d", asmname, total/BITS_PER_UNIT);
  return IDENTIFIER_POINTER (get_identifier (newsym));
}

void
i386_pe_encode_section_info (tree decl, rtx rtl, int first)
{
  default_encode_section_info (decl, rtl, first);

  if (TREE_CODE (decl) == FUNCTION_DECL)
    {
      if (lookup_attribute ("stdcall",
			    TYPE_ATTRIBUTES (TREE_TYPE (decl))))
        XEXP (DECL_RTL (decl), 0) =
	  gen_rtx (SYMBOL_REF, Pmode, gen_stdcall_suffix (decl));
      else if (lookup_attribute ("fastcall",
				 TYPE_ATTRIBUTES (TREE_TYPE (decl))))
        XEXP (DECL_RTL (decl), 0) =
	  gen_rtx (SYMBOL_REF, Pmode, gen_fastcall_suffix (decl));
    }

  /* Mark the decl so we can tell from the rtl whether the object is
     dllexport'd or dllimport'd.  This also handles dllexport/dllimport
     override semantics.  */

  if (i386_pe_dllexport_p (decl))
    i386_pe_mark_dllexport (decl);
  else if (i386_pe_dllimport_p (decl))
    i386_pe_mark_dllimport (decl);
  /* It might be that DECL has already been marked as dllimport, but a
     subsequent definition nullified that.  The attribute is gone but
     DECL_RTL still has (DLL_IMPORT_PREFIX) prefixed. We need to remove
     that. Ditto for the DECL_NON_ADDR_CONST_P flag.  */
  else if ((TREE_CODE (decl) == FUNCTION_DECL
	    || TREE_CODE (decl) == VAR_DECL)
	   && DECL_RTL (decl) != NULL_RTX
	   && GET_CODE (DECL_RTL (decl)) == MEM
	   && GET_CODE (XEXP (DECL_RTL (decl), 0)) == MEM
	   && GET_CODE (XEXP (XEXP (DECL_RTL (decl), 0), 0)) == SYMBOL_REF
	   && i386_pe_dllimport_name_p (XSTR (XEXP (XEXP (DECL_RTL (decl), 0), 0), 0)))
    {
      const char *oldname = XSTR (XEXP (XEXP (DECL_RTL (decl), 0), 0), 0);

      /* Remove DLL_IMPORT_PREFIX.  */
      tree idp = get_identifier (oldname + strlen (DLL_IMPORT_PREFIX));
      rtx newrtl = gen_rtx (SYMBOL_REF, Pmode, IDENTIFIER_POINTER (idp));

      if (DECL_INITIAL (decl) || !DECL_EXTERNAL (decl))
	warning ("%J'%D' defined locally after being "
		 "referenced with dllimport linkage", decl, decl);
      else
	warning ("%J'%D' redeclared without dllimport attribute "
		 "after being referenced with dllimport linkage", decl, decl);

      XEXP (DECL_RTL (decl), 0) = newrtl;

      DECL_NON_ADDR_CONST_P (decl) = 0;

      /* We previously set TREE_PUBLIC and DECL_EXTERNAL.
	 We leave these alone for now.  */
    }
}

/* Strip only the leading encoding, leaving the stdcall suffix and fastcall
   prefix if it exists.  */

const char *
i386_pe_strip_name_encoding (const char *str)
{
  if (strncmp (str, DLL_IMPORT_PREFIX, strlen (DLL_IMPORT_PREFIX))
      == 0)
    str += strlen (DLL_IMPORT_PREFIX);
  else if (strncmp (str, DLL_EXPORT_PREFIX, strlen (DLL_EXPORT_PREFIX))
	   == 0)
    str += strlen (DLL_EXPORT_PREFIX);
  if (*str == '*')
    str += 1;
  return str;
}

/* Also strip the stdcall suffix.  */

const char *
i386_pe_strip_name_encoding_full (const char *str)
{
  const char *p;
  const char *name = i386_pe_strip_name_encoding (str);

  p = strchr (name, '@');
  if (p)
    return ggc_alloc_string (name, p - name);

  return name;
}

/* Output a reference to a label. Fastcall symbols are prefixed with @,
   whereas symbols for functions using other calling conventions don't
   have a prefix (unless they are marked dllimport or dllexport).  */

void i386_pe_output_labelref (FILE *stream, const char *name)
{
  if (strncmp (name, DLL_IMPORT_PREFIX, strlen (DLL_IMPORT_PREFIX))
      == 0)
    /* A dll import */
    {
      if (name[strlen (DLL_IMPORT_PREFIX)] == FASTCALL_PREFIX)
      /* A dllimport fastcall symbol.  */
        {
          fprintf (stream, "__imp_%s",
                   i386_pe_strip_name_encoding (name));
        }
      else
      /* A dllimport non-fastcall symbol.  */
        {
          fprintf (stream, "__imp__%s",
                   i386_pe_strip_name_encoding (name));
        }
    }
  else if ((name[0] == FASTCALL_PREFIX)
           || (strncmp (name, DLL_EXPORT_PREFIX, strlen (DLL_EXPORT_PREFIX)
	       == 0
	       && name[strlen (DLL_EXPORT_PREFIX)] == FASTCALL_PREFIX)))
    /* A fastcall symbol.  */
    {
      fprintf (stream, "%s",
               i386_pe_strip_name_encoding (name));
    }
  else
    /* Everything else.  */
    {
      fprintf (stream, "%s%s", USER_LABEL_PREFIX,
               i386_pe_strip_name_encoding (name));
    }
}

void
i386_pe_unique_section (tree decl, int reloc)
{
  int len;
  const char *name, *prefix;
  char *string;

  name = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl));
  name = i386_pe_strip_name_encoding_full (name);

  /* The object is put in, for example, section .text$foo.
     The linker will then ultimately place them in .text
     (everything from the $ on is stripped). Don't put
     read-only data in .rdata section to avoid a PE linker
     bug when .rdata$* grouped sections are used in code
     without a .rdata section.  */
  if (TREE_CODE (decl) == FUNCTION_DECL)
    prefix = ".text$";
  else if (decl_readonly_section (decl, reloc))
    prefix = ".rdata$";
  else
    prefix = ".data$";
  len = strlen (name) + strlen (prefix);
  string = alloca (len + 1);
  sprintf (string, "%s%s", prefix, name);

  DECL_SECTION_NAME (decl) = build_string (len, string);
}

/* Select a set of attributes for section NAME based on the properties
   of DECL and whether or not RELOC indicates that DECL's initializer
   might contain runtime relocations.

   We make the section read-only and executable for a function decl,
   read-only for a const data decl, and writable for a non-const data decl.

   If the section has already been defined, to not allow it to have
   different attributes, as (1) this is ambiguous since we're not seeing
   all the declarations up front and (2) some assemblers (e.g. SVR4)
   do not recognize section redefinitions.  */
/* ??? This differs from the "standard" PE implementation in that we
   handle the SHARED variable attribute.  Should this be done for all
   PE targets?  */

#define SECTION_PE_SHARED	SECTION_MACH_DEP

unsigned int
i386_pe_section_type_flags (tree decl, const char *name, int reloc)
{
  static htab_t htab;
  unsigned int flags;
  unsigned int **slot;

  /* The names we put in the hashtable will always be the unique
     versions gived to us by the stringtable, so we can just use
     their addresses as the keys.  */
  if (!htab)
    htab = htab_create (31, htab_hash_pointer, htab_eq_pointer, NULL);

  if (decl && TREE_CODE (decl) == FUNCTION_DECL)
    flags = SECTION_CODE;
  else if (decl && decl_readonly_section (decl, reloc))
    flags = 0;
  else
    {
      flags = SECTION_WRITE;

      if (decl && TREE_CODE (decl) == VAR_DECL
	  && lookup_attribute ("shared", DECL_ATTRIBUTES (decl)))
	flags |= SECTION_PE_SHARED;
    }

  if (decl && DECL_ONE_ONLY (decl))
    flags |= SECTION_LINKONCE;

  /* See if we already have an entry for this section.  */
  slot = (unsigned int **) htab_find_slot (htab, name, INSERT);
  if (!*slot)
    {
      *slot = (unsigned int *) xmalloc (sizeof (unsigned int));
      **slot = flags;
    }
  else
    {
      if (decl && **slot != flags)
	error ("%J'%D' causes a section type conflict", decl, decl);
    }

  return flags;
}

void
i386_pe_asm_named_section (const char *name, unsigned int flags)
{
  char flagchars[8], *f = flagchars;

  if ((flags & (SECTION_CODE | SECTION_WRITE)) == 0)
    /* readonly data */
    {
      *f++ ='d';  /* This is necessary for older versions of gas.  */
      *f++ ='r';
    }
  else	
    {
      if (flags & SECTION_CODE)
        *f++ = 'x';
      if (flags & SECTION_WRITE)
        *f++ = 'w';
      if (flags & SECTION_PE_SHARED)
        *f++ = 's';
    }

  *f = '\0';

  fprintf (asm_out_file, "\t.section\t%s,\"%s\"\n", name, flagchars);

  if (flags & SECTION_LINKONCE)
    {
      /* Functions may have been compiled at various levels of
         optimization so we can't use `same_size' here.
         Instead, have the linker pick one.  */
      fprintf (asm_out_file, "\t.linkonce %s\n",
	       (flags & SECTION_CODE ? "discard" : "same_size"));
    }
}

/* The Microsoft linker requires that every function be marked as
   DT_FCN.  When using gas on cygwin, we must emit appropriate .type
   directives.  */

#include "gsyms.h"

/* Mark a function appropriately.  This should only be called for
   functions for which we are not emitting COFF debugging information.
   FILE is the assembler output file, NAME is the name of the
   function, and PUBLIC is nonzero if the function is globally
   visible.  */

void
i386_pe_declare_function_type (FILE *file, const char *name, int public)
{
  fprintf (file, "\t.def\t");
  assemble_name (file, name);
  fprintf (file, ";\t.scl\t%d;\t.type\t%d;\t.endef\n",
	   public ? (int) C_EXT : (int) C_STAT,
	   (int) DT_FCN << N_BTSHFT);
}

/* Keep a list of external functions.  */

struct extern_list
{
  struct extern_list *next;
  const char *name;
};

static struct extern_list *extern_head;

/* Assemble an external function reference.  We need to keep a list of
   these, so that we can output the function types at the end of the
   assembly.  We can't output the types now, because we might see a
   definition of the function later on and emit debugging information
   for it then.  */

void
i386_pe_record_external_function (const char *name)
{
  struct extern_list *p;

  p = (struct extern_list *) xmalloc (sizeof *p);
  p->next = extern_head;
  p->name = name;
  extern_head = p;
}

/* Keep a list of exported symbols.  */

struct export_list
{
  struct export_list *next;
  const char *name;
  int is_data;		/* used to type tag exported symbols.  */
};

static struct export_list *export_head;

/* Assemble an export symbol entry.  We need to keep a list of
   these, so that we can output the export list at the end of the
   assembly.  We used to output these export symbols in each function,
   but that causes problems with GNU ld when the sections are
   linkonce.  */

void
i386_pe_record_exported_symbol (const char *name, int is_data)
{
  struct export_list *p;

  p = (struct export_list *) xmalloc (sizeof *p);
  p->next = export_head;
  p->name = name;
  p->is_data = is_data;
  export_head = p;
}

/* This is called at the end of assembly.  For each external function
   which has not been defined, we output a declaration now.  We also
   output the .drectve section.  */

void
i386_pe_file_end (void)
{
  struct extern_list *p;

  ix86_file_end ();

  for (p = extern_head; p != NULL; p = p->next)
    {
      tree decl;

      decl = get_identifier (p->name);

      /* Positively ensure only one declaration for any given symbol.  */
      if (! TREE_ASM_WRITTEN (decl) && TREE_SYMBOL_REFERENCED (decl))
	{
	  TREE_ASM_WRITTEN (decl) = 1;
	  i386_pe_declare_function_type (asm_out_file, p->name,
					 TREE_PUBLIC (decl));
	}
    }

  if (export_head)
    {
      struct export_list *q;
      drectve_section ();
      for (q = export_head; q != NULL; q = q->next)
	{
	  fprintf (asm_out_file, "\t.ascii \" -export:%s%s\"\n",
		   i386_pe_strip_name_encoding (q->name),
		   (q->is_data) ? ",data" : "");
	}
    }
}
