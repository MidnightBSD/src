/* Exception Handling interface routines.
   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.
   Contributed by Mike Stump <mrs@cygnus.com>.

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


struct function;

struct inline_remap;

/* Per-function EH data.  Used only in except.c, but GC and others
   manipulate pointers to the opaque type.  */
struct eh_status;

/* Internal structure describing a region.  */
struct eh_region;

/* Test: is exception handling turned on?  */
extern int doing_eh (int);

/* Start an exception handling region.  All instructions emitted after
   this point are considered to be part of the region until an
   expand_eh_region_end variant is invoked.  */
extern void expand_eh_region_start (void);

/* End an exception handling region for a cleanup.  HANDLER is an
   expression to expand for the cleanup.  */
extern void expand_eh_region_end_cleanup (tree);

/* End an exception handling region for a try block, and prepares
   for subsequent calls to expand_start_catch.  */
extern void expand_start_all_catch (void);

/* Begin a catch clause.  TYPE is an object to be matched by the
   runtime, or a list of such objects, or null if this is a catch-all
   clause.  */
extern void expand_start_catch (tree);

/* End a catch clause.  Control will resume after the try/catch block.  */
extern void expand_end_catch (void);

/* End a sequence of catch handlers for a try block.  */
extern void expand_end_all_catch (void);

/* End an exception region for an exception type filter.  ALLOWED is a
   TREE_LIST of TREE_VALUE objects to be matched by the runtime.
   FAILURE is a function to invoke if a mismatch occurs.  */
extern void expand_eh_region_end_allowed (tree, tree);

/* End an exception region for a must-not-throw filter.  FAILURE is a
   function to invoke if an uncaught exception propagates this far.  */
extern void expand_eh_region_end_must_not_throw (tree);

/* End an exception region for a throw.  No handling goes on here,
   but it's the easiest way for the front-end to indicate what type
   is being thrown.  */
extern void expand_eh_region_end_throw (tree);

/* End a fixup region.  Within this region the cleanups for the immediately
   enclosing region are _not_ run.  This is used for goto cleanup to avoid
   destroying an object twice.  */
extern void expand_eh_region_end_fixup (tree);

/* Note that the current EH region (if any) may contain a throw, or a
   call to a function which itself may contain a throw.  */
extern void note_eh_region_may_contain_throw (void);

/* Invokes CALLBACK for every exception handler label.  Only used by old
   loop hackery; should not be used by new code.  */
extern void for_each_eh_label (void (*) (rtx));

/* Determine if the given INSN can throw an exception.  */
extern bool can_throw_internal (rtx);
extern bool can_throw_external (rtx);

/* Set current_function_nothrow and cfun->all_throwers_are_sibcalls.  */
extern void set_nothrow_function_flags (void);

/* After initial rtl generation, call back to finish generating
   exception support code.  */
extern void finish_eh_generation (void);

extern void init_eh (void);
extern void init_eh_for_function (void);

extern rtx reachable_handlers (rtx);
extern void maybe_remove_eh_handler (rtx);

extern void convert_from_eh_region_ranges (void);
extern void convert_to_eh_region_ranges (void);
extern void find_exception_handler_labels (void);
extern bool current_function_has_exception_handlers (void);
extern void output_function_exception_table (void);

extern void expand_builtin_unwind_init (void);
extern rtx expand_builtin_eh_return_data_regno (tree);
extern rtx expand_builtin_extract_return_addr (tree);
extern void expand_builtin_init_dwarf_reg_sizes (tree);
extern rtx expand_builtin_frob_return_addr (tree);
extern rtx expand_builtin_dwarf_sp_column (void);
extern void expand_builtin_eh_return (tree, tree);
extern void expand_eh_return (void);
extern rtx expand_builtin_extend_pointer (tree);
extern rtx get_exception_pointer (struct function *);
extern int duplicate_eh_regions (struct function *, struct inline_remap *);

extern void sjlj_emit_function_exit_after (rtx);


/* If non-NULL, this is a function that returns an expression to be
   executed if an unhandled exception is propagated out of a cleanup
   region.  For example, in C++, an exception thrown by a destructor
   during stack unwinding is required to result in a call to
   `std::terminate', so the C++ version of this function returns a
   CALL_EXPR for `std::terminate'.  */
extern tree (*lang_protect_cleanup_actions) (void);

/* Return true if type A catches type B.  */
extern int (*lang_eh_type_covers) (tree a, tree b);

/* Map a type to a runtime object to match type.  */
extern tree (*lang_eh_runtime_type) (tree);


/* Just because the user configured --with-sjlj-exceptions=no doesn't
   mean that we can use call frame exceptions.  Detect that the target
   has appropriate support.  */

#ifndef MUST_USE_SJLJ_EXCEPTIONS
# if !(defined (EH_RETURN_DATA_REGNO)			\
       && (defined (IA64_UNWIND_INFO)			\
	   || (DWARF2_UNWIND_INFO			\
	       && (defined (EH_RETURN_HANDLER_RTX)	\
		   || defined (HAVE_eh_return)))))
#  define MUST_USE_SJLJ_EXCEPTIONS	1
# else
#  define MUST_USE_SJLJ_EXCEPTIONS	0
# endif
#endif

#ifdef CONFIG_SJLJ_EXCEPTIONS
# if CONFIG_SJLJ_EXCEPTIONS == 1
#  define USING_SJLJ_EXCEPTIONS		1
# endif
# if CONFIG_SJLJ_EXCEPTIONS == 0
#  define USING_SJLJ_EXCEPTIONS		0
#  ifndef EH_RETURN_DATA_REGNO
    #error "EH_RETURN_DATA_REGNO required"
#  endif
#  if !defined(EH_RETURN_HANDLER_RTX) && !defined(HAVE_eh_return)
    #error "EH_RETURN_HANDLER_RTX or eh_return required"
#  endif
#  if !defined(DWARF2_UNWIND_INFO) && !defined(IA64_UNWIND_INFO)
    #error "{DWARF2,IA64}_UNWIND_INFO required"
#  endif
# endif
#else
# define USING_SJLJ_EXCEPTIONS		MUST_USE_SJLJ_EXCEPTIONS
#endif
