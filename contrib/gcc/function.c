/* Expands front end tree to back end RTL for GCC.
   Copyright (C) 1987, 1988, 1989, 1991, 1992, 1993, 1994, 1995, 1996, 1997,
   1998, 1999, 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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

/* $FreeBSD: src/contrib/gcc/function.c,v 1.22 2005/06/03 04:02:19 kan Exp $ */

/* This file handles the generation of rtl code from tree structure
   at the level of the function as a whole.
   It creates the rtl expressions for parameters and auto variables
   and has full responsibility for allocating stack slots.

   `expand_function_start' is called at the beginning of a function,
   before the function body is parsed, and `expand_function_end' is
   called after parsing the body.

   Call `assign_stack_local' to allocate a stack slot for a local variable.
   This is usually done during the RTL generation for the function body,
   but it can also be done in the reload pass when a pseudo-register does
   not get a hard register.

   Call `put_var_into_stack' when you learn, belatedly, that a variable
   previously given a pseudo-register must in fact go in the stack.
   This function changes the DECL_RTL to be a stack slot instead of a reg
   then scans all the RTL instructions so far generated to correct them.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "tree.h"
#include "flags.h"
#include "except.h"
#include "function.h"
#include "expr.h"
#include "optabs.h"
#include "libfuncs.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "insn-config.h"
#include "recog.h"
#include "output.h"
#include "basic-block.h"
#include "toplev.h"
#include "hashtab.h"
#include "ggc.h"
#include "tm_p.h"
#include "integrate.h"
#include "langhooks.h"
#include "target.h"

#ifndef TRAMPOLINE_ALIGNMENT
#define TRAMPOLINE_ALIGNMENT FUNCTION_BOUNDARY
#endif

#ifndef LOCAL_ALIGNMENT
#define LOCAL_ALIGNMENT(TYPE, ALIGNMENT) ALIGNMENT
#endif

#ifndef STACK_ALIGNMENT_NEEDED
#define STACK_ALIGNMENT_NEEDED 1
#endif

#define STACK_BYTES (STACK_BOUNDARY / BITS_PER_UNIT)

/* Some systems use __main in a way incompatible with its use in gcc, in these
   cases use the macros NAME__MAIN to give a quoted symbol and SYMBOL__MAIN to
   give the same symbol without quotes for an alternative entry point.  You
   must define both, or neither.  */
#ifndef NAME__MAIN
#define NAME__MAIN "__main"
#endif

/* Round a value to the lowest integer less than it that is a multiple of
   the required alignment.  Avoid using division in case the value is
   negative.  Assume the alignment is a power of two.  */
#define FLOOR_ROUND(VALUE,ALIGN) ((VALUE) & ~((ALIGN) - 1))

/* Similar, but round to the next highest integer that meets the
   alignment.  */
#define CEIL_ROUND(VALUE,ALIGN)	(((VALUE) + (ALIGN) - 1) & ~((ALIGN)- 1))

/* NEED_SEPARATE_AP means that we cannot derive ap from the value of fp
   during rtl generation.  If they are different register numbers, this is
   always true.  It may also be true if
   FIRST_PARM_OFFSET - STARTING_FRAME_OFFSET is not a constant during rtl
   generation.  See fix_lexical_addr for details.  */

#if ARG_POINTER_REGNUM != FRAME_POINTER_REGNUM
#define NEED_SEPARATE_AP
#endif

/* Nonzero if function being compiled doesn't contain any calls
   (ignoring the prologue and epilogue).  This is set prior to
   local register allocation and is valid for the remaining
   compiler passes.  */
int current_function_is_leaf;

/* Nonzero if function being compiled doesn't contain any instructions
   that can throw an exception.  This is set prior to final.  */

int current_function_nothrow;

/* Nonzero if function being compiled doesn't modify the stack pointer
   (ignoring the prologue and epilogue).  This is only valid after
   life_analysis has run.  */
int current_function_sp_is_unchanging;

/* Nonzero if the function being compiled is a leaf function which only
   uses leaf registers.  This is valid after reload (specifically after
   sched2) and is useful only if the port defines LEAF_REGISTERS.  */
int current_function_uses_only_leaf_regs;

/* Nonzero once virtual register instantiation has been done.
   assign_stack_local uses frame_pointer_rtx when this is nonzero.
   calls.c:emit_library_call_value_1 uses it to set up
   post-instantiation libcalls.  */
int virtuals_instantiated;

/* Nonzero if at least one trampoline has been created.  */
int trampolines_created;

/* Assign unique numbers to labels generated for profiling, debugging, etc.  */
static GTY(()) int funcdef_no;

/* These variables hold pointers to functions to create and destroy
   target specific, per-function data structures.  */
struct machine_function * (*init_machine_status) (void);

/* The FUNCTION_DECL for an inline function currently being expanded.  */
tree inline_function_decl;

/* The currently compiled function.  */
struct function *cfun = 0;

/* These arrays record the INSN_UIDs of the prologue and epilogue insns.  */
static GTY(()) varray_type prologue;
static GTY(()) varray_type epilogue;

/* Array of INSN_UIDs to hold the INSN_UIDs for each sibcall epilogue
   in this function.  */
static GTY(()) varray_type sibcall_epilogue;

/* In order to evaluate some expressions, such as function calls returning
   structures in memory, we need to temporarily allocate stack locations.
   We record each allocated temporary in the following structure.

   Associated with each temporary slot is a nesting level.  When we pop up
   one level, all temporaries associated with the previous level are freed.
   Normally, all temporaries are freed after the execution of the statement
   in which they were created.  However, if we are inside a ({...}) grouping,
   the result may be in a temporary and hence must be preserved.  If the
   result could be in a temporary, we preserve it if we can determine which
   one it is in.  If we cannot determine which temporary may contain the
   result, all temporaries are preserved.  A temporary is preserved by
   pretending it was allocated at the previous nesting level.

   Automatic variables are also assigned temporary slots, at the nesting
   level where they are defined.  They are marked a "kept" so that
   free_temp_slots will not free them.  */

struct temp_slot GTY(())
{
  /* Points to next temporary slot.  */
  struct temp_slot *next;
  /* The rtx to used to reference the slot.  */
  rtx slot;
  /* The rtx used to represent the address if not the address of the
     slot above.  May be an EXPR_LIST if multiple addresses exist.  */
  rtx address;
  /* The alignment (in bits) of the slot.  */
  unsigned int align;
  /* The size, in units, of the slot.  */
  HOST_WIDE_INT size;
  /* The type of the object in the slot, or zero if it doesn't correspond
     to a type.  We use this to determine whether a slot can be reused.
     It can be reused if objects of the type of the new slot will always
     conflict with objects of the type of the old slot.  */
  tree type;
  /* The value of `sequence_rtl_expr' when this temporary is allocated.  */
  tree rtl_expr;
  /* Nonzero if this temporary is currently in use.  */
  char in_use;
  /* Nonzero if this temporary has its address taken.  */
  char addr_taken;
  /* Nesting level at which this slot is being used.  */
  int level;
  /* Nonzero if this should survive a call to free_temp_slots.  */
  int keep;
  /* The offset of the slot from the frame_pointer, including extra space
     for alignment.  This info is for combine_temp_slots.  */
  HOST_WIDE_INT base_offset;
  /* The size of the slot, including extra space for alignment.  This
     info is for combine_temp_slots.  */
  HOST_WIDE_INT full_size;
};

/* This structure is used to record MEMs or pseudos used to replace VAR, any
   SUBREGs of VAR, and any MEMs containing VAR as an address.  We need to
   maintain this list in case two operands of an insn were required to match;
   in that case we must ensure we use the same replacement.  */

struct fixup_replacement GTY(())
{
  rtx old;
  rtx new;
  struct fixup_replacement *next;
};

struct insns_for_mem_entry
{
  /* A MEM.  */
  rtx key;
  /* These are the INSNs which reference the MEM.  */
  rtx insns;
};

/* Forward declarations.  */

static rtx assign_stack_local_1 (enum machine_mode, HOST_WIDE_INT, int,
				 struct function *);
static struct temp_slot *find_temp_slot_from_address (rtx);
static void put_reg_into_stack (struct function *, rtx, tree, enum machine_mode,
				unsigned int, bool, bool, bool, htab_t);
static void schedule_fixup_var_refs (struct function *, rtx, tree, enum machine_mode,
				     htab_t);
static void fixup_var_refs (rtx, enum machine_mode, int, rtx, htab_t);
static struct fixup_replacement
  *find_fixup_replacement (struct fixup_replacement **, rtx);
static void fixup_var_refs_insns (rtx, rtx, enum machine_mode, int, int, rtx);
static void fixup_var_refs_insns_with_hash (htab_t, rtx, enum machine_mode, int, rtx);
static void fixup_var_refs_insn (rtx, rtx, enum machine_mode, int, int, rtx);
static void fixup_var_refs_1 (rtx, enum machine_mode, rtx *, rtx,
			      struct fixup_replacement **, rtx);
static rtx fixup_memory_subreg (rtx, rtx, enum machine_mode, int);
static rtx walk_fixup_memory_subreg (rtx, rtx, enum machine_mode, int);
static rtx fixup_stack_1 (rtx, rtx);
static void optimize_bit_field (rtx, rtx, rtx *);
static void instantiate_decls (tree, int);
static void instantiate_decls_1 (tree, int);
static void instantiate_decl (rtx, HOST_WIDE_INT, int);
static rtx instantiate_new_reg (rtx, HOST_WIDE_INT *);
static int instantiate_virtual_regs_1 (rtx *, rtx, int);
static void delete_handlers (void);
static void pad_to_arg_alignment (struct args_size *, int, struct args_size *);
static void pad_below (struct args_size *, enum machine_mode, tree);
static rtx round_trampoline_addr (rtx);
static rtx adjust_trampoline_addr (rtx);
static tree *identify_blocks_1 (rtx, tree *, tree *, tree *);
static void reorder_blocks_0 (tree);
static void reorder_blocks_1 (rtx, tree, varray_type *);
static void reorder_fix_fragments (tree);
static tree blocks_nreverse (tree);
static int all_blocks (tree, tree *);
static tree *get_block_vector (tree, int *);
extern tree debug_find_var_in_block_tree (tree, tree);
/* We always define `record_insns' even if its not used so that we
   can always export `prologue_epilogue_contains'.  */
static void record_insns (rtx, varray_type *) ATTRIBUTE_UNUSED;
static int contains (rtx, varray_type);
#ifdef HAVE_return
static void emit_return_into_block (basic_block, rtx);
#endif
static void put_addressof_into_stack (rtx, htab_t);
static bool purge_addressof_1 (rtx *, rtx, int, int, int, htab_t);
static void purge_single_hard_subreg_set (rtx);
#if defined(HAVE_epilogue) && defined(INCOMING_RETURN_ADDR_RTX)
static rtx keep_stack_depressed (rtx);
#endif
static int is_addressof (rtx *, void *);
static hashval_t insns_for_mem_hash (const void *);
static int insns_for_mem_comp (const void *, const void *);
static int insns_for_mem_walk (rtx *, void *);
static void compute_insns_for_mem (rtx, rtx, htab_t);
static void prepare_function_start (tree);
static void do_clobber_return_reg (rtx, void *);
static void do_use_return_reg (rtx, void *);
static void instantiate_virtual_regs_lossage (rtx);
static tree split_complex_args (tree);
static void set_insn_locators (rtx, int) ATTRIBUTE_UNUSED;

/* Pointer to chain of `struct function' for containing functions.  */
struct function *outer_function_chain;

/* List of insns that were postponed by purge_addressof_1.  */
static rtx postponed_insns;

/* Given a function decl for a containing function,
   return the `struct function' for it.  */

struct function *
find_function_data (tree decl)
{
  struct function *p;

  for (p = outer_function_chain; p; p = p->outer)
    if (p->decl == decl)
      return p;

  abort ();
}

/* Save the current context for compilation of a nested function.
   This is called from language-specific code.  The caller should use
   the enter_nested langhook to save any language-specific state,
   since this function knows only about language-independent
   variables.  */

void
push_function_context_to (tree context)
{
  struct function *p;

  if (context)
    {
      if (context == current_function_decl)
	cfun->contains_functions = 1;
      else
	{
	  struct function *containing = find_function_data (context);
	  containing->contains_functions = 1;
	}
    }

  if (cfun == 0)
    init_dummy_function_start ();
  p = cfun;

  p->outer = outer_function_chain;
  outer_function_chain = p;
  p->fixup_var_refs_queue = 0;

  (*lang_hooks.function.enter_nested) (p);

  cfun = 0;
}

void
push_function_context (void)
{
  push_function_context_to (current_function_decl);
}

/* Restore the last saved context, at the end of a nested function.
   This function is called from language-specific code.  */

void
pop_function_context_from (tree context ATTRIBUTE_UNUSED)
{
  struct function *p = outer_function_chain;
  struct var_refs_queue *queue;

  cfun = p;
  outer_function_chain = p->outer;

  current_function_decl = p->decl;
  reg_renumber = 0;

  restore_emit_status (p);

  (*lang_hooks.function.leave_nested) (p);

  /* Finish doing put_var_into_stack for any of our variables which became
     addressable during the nested function.  If only one entry has to be
     fixed up, just do that one.  Otherwise, first make a list of MEMs that
     are not to be unshared.  */
  if (p->fixup_var_refs_queue == 0)
    ;
  else if (p->fixup_var_refs_queue->next == 0)
    fixup_var_refs (p->fixup_var_refs_queue->modified,
		    p->fixup_var_refs_queue->promoted_mode,
		    p->fixup_var_refs_queue->unsignedp,
		    p->fixup_var_refs_queue->modified, 0);
  else
    {
      rtx list = 0;

      for (queue = p->fixup_var_refs_queue; queue; queue = queue->next)
	list = gen_rtx_EXPR_LIST (VOIDmode, queue->modified, list);

      for (queue = p->fixup_var_refs_queue; queue; queue = queue->next)
	fixup_var_refs (queue->modified, queue->promoted_mode,
			queue->unsignedp, list, 0);

    }

  p->fixup_var_refs_queue = 0;

  /* Reset variables that have known state during rtx generation.  */
  rtx_equal_function_value_matters = 1;
  virtuals_instantiated = 0;
  generating_concat_p = 1;
}

void
pop_function_context (void)
{
  pop_function_context_from (current_function_decl);
}

/* Clear out all parts of the state in F that can safely be discarded
   after the function has been parsed, but not compiled, to let
   garbage collection reclaim the memory.  */

void
free_after_parsing (struct function *f)
{
  /* f->expr->forced_labels is used by code generation.  */
  /* f->emit->regno_reg_rtx is used by code generation.  */
  /* f->varasm is used by code generation.  */
  /* f->eh->eh_return_stub_label is used by code generation.  */

  (*lang_hooks.function.final) (f);
  f->stmt = NULL;
}

/* Clear out all parts of the state in F that can safely be discarded
   after the function has been compiled, to let garbage collection
   reclaim the memory.  */

void
free_after_compilation (struct function *f)
{
  f->eh = NULL;
  f->expr = NULL;
  f->emit = NULL;
  f->varasm = NULL;
  f->machine = NULL;

  f->x_temp_slots = NULL;
  f->arg_offset_rtx = NULL;
  f->return_rtx = NULL;
  f->internal_arg_pointer = NULL;
  f->x_nonlocal_labels = NULL;
  f->x_nonlocal_goto_handler_slots = NULL;
  f->x_nonlocal_goto_handler_labels = NULL;
  f->x_nonlocal_goto_stack_level = NULL;
  f->x_cleanup_label = NULL;
  f->x_return_label = NULL;
  f->x_naked_return_label = NULL;
  f->computed_goto_common_label = NULL;
  f->computed_goto_common_reg = NULL;
  f->x_save_expr_regs = NULL;
  f->x_stack_slot_list = NULL;
  f->x_rtl_expr_chain = NULL;
  f->x_tail_recursion_label = NULL;
  f->x_tail_recursion_reentry = NULL;
  f->x_arg_pointer_save_area = NULL;
  f->x_clobber_return_insn = NULL;
  f->x_context_display = NULL;
  f->x_trampoline_list = NULL;
  f->x_parm_birth_insn = NULL;
  f->x_last_parm_insn = NULL;
  f->x_parm_reg_stack_loc = NULL;
  f->fixup_var_refs_queue = NULL;
  f->original_arg_vector = NULL;
  f->original_decl_initial = NULL;
  f->inl_last_parm_insn = NULL;
  f->epilogue_delay_list = NULL;
}

/* Allocate fixed slots in the stack frame of the current function.  */

/* Return size needed for stack frame based on slots so far allocated in
   function F.
   This size counts from zero.  It is not rounded to PREFERRED_STACK_BOUNDARY;
   the caller may have to do that.  */

HOST_WIDE_INT
get_func_frame_size (struct function *f)
{
#ifdef FRAME_GROWS_DOWNWARD
  return -f->x_frame_offset;
#else
  return f->x_frame_offset;
#endif
}

/* Return size needed for stack frame based on slots so far allocated.
   This size counts from zero.  It is not rounded to PREFERRED_STACK_BOUNDARY;
   the caller may have to do that.  */
HOST_WIDE_INT
get_frame_size (void)
{
  return get_func_frame_size (cfun);
}

/* Allocate a stack slot of SIZE bytes and return a MEM rtx for it
   with machine mode MODE.

   ALIGN controls the amount of alignment for the address of the slot:
   0 means according to MODE,
   -1 means use BIGGEST_ALIGNMENT and round size to multiple of that,
   -2 means use BITS_PER_UNIT,
   positive specifies alignment boundary in bits.

   We do not round to stack_boundary here.

   FUNCTION specifies the function to allocate in.  */

static rtx
assign_stack_local_1 (enum machine_mode mode, HOST_WIDE_INT size, int align,
		      struct function *function)
{
  rtx x, addr;
  int bigend_correction = 0;
  int alignment;
  int frame_off, frame_alignment, frame_phase;

  if (align == 0)
    {
      tree type;

      if (mode == BLKmode)
	alignment = BIGGEST_ALIGNMENT;
      else
	alignment = GET_MODE_ALIGNMENT (mode);

      /* Allow the target to (possibly) increase the alignment of this
	 stack slot.  */
      type = (*lang_hooks.types.type_for_mode) (mode, 0);
      if (type)
	alignment = LOCAL_ALIGNMENT (type, alignment);

      alignment /= BITS_PER_UNIT;
    }
  else if (align == -1)
    {
      alignment = BIGGEST_ALIGNMENT / BITS_PER_UNIT;
      size = CEIL_ROUND (size, alignment);
    }
  else if (align == -2)
    alignment = 1; /* BITS_PER_UNIT / BITS_PER_UNIT */
  else
    alignment = align / BITS_PER_UNIT;

#ifdef FRAME_GROWS_DOWNWARD
  function->x_frame_offset -= size;
#endif

  /* Ignore alignment we can't do with expected alignment of the boundary.  */
  if (alignment * BITS_PER_UNIT > PREFERRED_STACK_BOUNDARY)
    alignment = PREFERRED_STACK_BOUNDARY / BITS_PER_UNIT;

  if (function->stack_alignment_needed < alignment * BITS_PER_UNIT)
    function->stack_alignment_needed = alignment * BITS_PER_UNIT;

  /* Calculate how many bytes the start of local variables is off from
     stack alignment.  */
  frame_alignment = PREFERRED_STACK_BOUNDARY / BITS_PER_UNIT;
  frame_off = STARTING_FRAME_OFFSET % frame_alignment;
  frame_phase = frame_off ? frame_alignment - frame_off : 0;

  /* Round the frame offset to the specified alignment.  The default is
     to always honor requests to align the stack but a port may choose to
     do its own stack alignment by defining STACK_ALIGNMENT_NEEDED.  */
  if (STACK_ALIGNMENT_NEEDED
      || mode != BLKmode
      || size != 0)
    {
      /*  We must be careful here, since FRAME_OFFSET might be negative and
	  division with a negative dividend isn't as well defined as we might
	  like.  So we instead assume that ALIGNMENT is a power of two and
	  use logical operations which are unambiguous.  */
#ifdef FRAME_GROWS_DOWNWARD
      function->x_frame_offset
	= (FLOOR_ROUND (function->x_frame_offset - frame_phase, alignment)
	   + frame_phase);
#else
      function->x_frame_offset
	= (CEIL_ROUND (function->x_frame_offset - frame_phase, alignment)
	   + frame_phase);
#endif
    }

  /* On a big-endian machine, if we are allocating more space than we will use,
     use the least significant bytes of those that are allocated.  */
  if (BYTES_BIG_ENDIAN && mode != BLKmode)
    bigend_correction = size - GET_MODE_SIZE (mode);

  /* If we have already instantiated virtual registers, return the actual
     address relative to the frame pointer.  */
  if (function == cfun && virtuals_instantiated)
    addr = plus_constant (frame_pointer_rtx,
			  trunc_int_for_mode
			  (frame_offset + bigend_correction
			   + STARTING_FRAME_OFFSET, Pmode));
  else
    addr = plus_constant (virtual_stack_vars_rtx,
			  trunc_int_for_mode
			  (function->x_frame_offset + bigend_correction,
			   Pmode));

#ifndef FRAME_GROWS_DOWNWARD
  function->x_frame_offset += size;
#endif

  x = gen_rtx_MEM (mode, addr);

  function->x_stack_slot_list
    = gen_rtx_EXPR_LIST (VOIDmode, x, function->x_stack_slot_list);

  return x;
}

/* Wrapper around assign_stack_local_1;  assign a local stack slot for the
   current function.  */

rtx
assign_stack_local (enum machine_mode mode, HOST_WIDE_INT size, int align)
{
  return assign_stack_local_1 (mode, size, align, cfun);
}

/* Allocate a temporary stack slot and record it for possible later
   reuse.

   MODE is the machine mode to be given to the returned rtx.

   SIZE is the size in units of the space required.  We do no rounding here
   since assign_stack_local will do any required rounding.

   KEEP is 1 if this slot is to be retained after a call to
   free_temp_slots.  Automatic variables for a block are allocated
   with this flag.  KEEP is 2 if we allocate a longer term temporary,
   whose lifetime is controlled by CLEANUP_POINT_EXPRs.  KEEP is 3
   if we are to allocate something at an inner level to be treated as
   a variable in the block (e.g., a SAVE_EXPR).

   TYPE is the type that will be used for the stack slot.  */

rtx
assign_stack_temp_for_type (enum machine_mode mode, HOST_WIDE_INT size, int keep,
			    tree type)
{
  unsigned int align;
  struct temp_slot *p, *best_p = 0;
  rtx slot;

  /* If SIZE is -1 it means that somebody tried to allocate a temporary
     of a variable size.  */
  if (size == -1)
    abort ();

  if (mode == BLKmode)
    align = BIGGEST_ALIGNMENT;
  else
    align = GET_MODE_ALIGNMENT (mode);

  if (! type)
    type = (*lang_hooks.types.type_for_mode) (mode, 0);

  if (type)
    align = LOCAL_ALIGNMENT (type, align);

  /* Try to find an available, already-allocated temporary of the proper
     mode which meets the size and alignment requirements.  Choose the
     smallest one with the closest alignment.  */
  for (p = temp_slots; p; p = p->next)
    if (p->align >= align && p->size >= size && GET_MODE (p->slot) == mode
	&& ! p->in_use
	&& objects_must_conflict_p (p->type, type)
	&& (best_p == 0 || best_p->size > p->size
	    || (best_p->size == p->size && best_p->align > p->align)))
      {
	if (p->align == align && p->size == size)
	  {
	    best_p = 0;
	    break;
	  }
	best_p = p;
      }

  /* Make our best, if any, the one to use.  */
  if (best_p)
    {
      /* If there are enough aligned bytes left over, make them into a new
	 temp_slot so that the extra bytes don't get wasted.  Do this only
	 for BLKmode slots, so that we can be sure of the alignment.  */
      if (GET_MODE (best_p->slot) == BLKmode)
	{
	  int alignment = best_p->align / BITS_PER_UNIT;
	  HOST_WIDE_INT rounded_size = CEIL_ROUND (size, alignment);

	  if (best_p->size - rounded_size >= alignment)
	    {
	      p = ggc_alloc (sizeof (struct temp_slot));
	      p->in_use = p->addr_taken = 0;
	      p->size = best_p->size - rounded_size;
	      p->base_offset = best_p->base_offset + rounded_size;
	      p->full_size = best_p->full_size - rounded_size;
	      p->slot = gen_rtx_MEM (BLKmode,
				     plus_constant (XEXP (best_p->slot, 0),
						    rounded_size));
	      p->align = best_p->align;
	      p->address = 0;
	      p->rtl_expr = 0;
	      p->type = best_p->type;
	      p->next = temp_slots;
	      temp_slots = p;

	      stack_slot_list = gen_rtx_EXPR_LIST (VOIDmode, p->slot,
						   stack_slot_list);

	      best_p->size = rounded_size;
	      best_p->full_size = rounded_size;
	    }
	}

      p = best_p;
    }

  /* If we still didn't find one, make a new temporary.  */
  if (p == 0)
    {
      HOST_WIDE_INT frame_offset_old = frame_offset;

      p = ggc_alloc (sizeof (struct temp_slot));

      /* We are passing an explicit alignment request to assign_stack_local.
	 One side effect of that is assign_stack_local will not round SIZE
	 to ensure the frame offset remains suitably aligned.

	 So for requests which depended on the rounding of SIZE, we go ahead
	 and round it now.  We also make sure ALIGNMENT is at least
	 BIGGEST_ALIGNMENT.  */
      if (mode == BLKmode && align < BIGGEST_ALIGNMENT)
	abort ();
      p->slot = assign_stack_local (mode,
				    (mode == BLKmode
				     ? CEIL_ROUND (size, (int) align / BITS_PER_UNIT)
				     : size),
				    align);

      p->align = align;

      /* The following slot size computation is necessary because we don't
	 know the actual size of the temporary slot until assign_stack_local
	 has performed all the frame alignment and size rounding for the
	 requested temporary.  Note that extra space added for alignment
	 can be either above or below this stack slot depending on which
	 way the frame grows.  We include the extra space if and only if it
	 is above this slot.  */
#ifdef FRAME_GROWS_DOWNWARD
      p->size = frame_offset_old - frame_offset;
#else
      p->size = size;
#endif

      /* Now define the fields used by combine_temp_slots.  */
#ifdef FRAME_GROWS_DOWNWARD
      p->base_offset = frame_offset;
      p->full_size = frame_offset_old - frame_offset;
#else
      p->base_offset = frame_offset_old;
      p->full_size = frame_offset - frame_offset_old;
#endif
      p->address = 0;
      p->next = temp_slots;
      temp_slots = p;
    }

  p->in_use = 1;
  p->addr_taken = 0;
  p->rtl_expr = seq_rtl_expr;
  p->type = type;

  if (keep == 2)
    {
      p->level = target_temp_slot_level;
      p->keep = 1;
    }
  else if (keep == 3)
    {
      p->level = var_temp_slot_level;
      p->keep = 0;
    }
  else
    {
      p->level = temp_slot_level;
      p->keep = keep;
    }


  /* Create a new MEM rtx to avoid clobbering MEM flags of old slots.  */
  slot = gen_rtx_MEM (mode, XEXP (p->slot, 0));
  stack_slot_list = gen_rtx_EXPR_LIST (VOIDmode, slot, stack_slot_list);

  /* If we know the alias set for the memory that will be used, use
     it.  If there's no TYPE, then we don't know anything about the
     alias set for the memory.  */
  set_mem_alias_set (slot, type ? get_alias_set (type) : 0);
  set_mem_align (slot, align);

  /* If a type is specified, set the relevant flags.  */
  if (type != 0)
    {
      RTX_UNCHANGING_P (slot) = (lang_hooks.honor_readonly
				 && TYPE_READONLY (type));
      MEM_VOLATILE_P (slot) = TYPE_VOLATILE (type);
      MEM_SET_IN_STRUCT_P (slot, AGGREGATE_TYPE_P (type));
    }

  return slot;
}

/* Allocate a temporary stack slot and record it for possible later
   reuse.  First three arguments are same as in preceding function.  */

rtx
assign_stack_temp (enum machine_mode mode, HOST_WIDE_INT size, int keep)
{
  return assign_stack_temp_for_type (mode, size, keep, NULL_TREE);
}

/* Assign a temporary.
   If TYPE_OR_DECL is a decl, then we are doing it on behalf of the decl
   and so that should be used in error messages.  In either case, we
   allocate of the given type.
   KEEP is as for assign_stack_temp.
   MEMORY_REQUIRED is 1 if the result must be addressable stack memory;
   it is 0 if a register is OK.
   DONT_PROMOTE is 1 if we should not promote values in register
   to wider modes.  */

rtx
assign_temp (tree type_or_decl, int keep, int memory_required,
	     int dont_promote ATTRIBUTE_UNUSED)
{
  tree type, decl;
  enum machine_mode mode;
#ifndef PROMOTE_FOR_CALL_ONLY
  int unsignedp;
#endif

  if (DECL_P (type_or_decl))
    decl = type_or_decl, type = TREE_TYPE (decl);
  else
    decl = NULL, type = type_or_decl;

  mode = TYPE_MODE (type);
#ifndef PROMOTE_FOR_CALL_ONLY
  unsignedp = TREE_UNSIGNED (type);
#endif

  if (mode == BLKmode || memory_required)
    {
      HOST_WIDE_INT size = int_size_in_bytes (type);
      rtx tmp;

      /* Zero sized arrays are GNU C extension.  Set size to 1 to avoid
	 problems with allocating the stack space.  */
      if (size == 0)
	size = 1;

      /* Unfortunately, we don't yet know how to allocate variable-sized
	 temporaries.  However, sometimes we have a fixed upper limit on
	 the size (which is stored in TYPE_ARRAY_MAX_SIZE) and can use that
	 instead.  This is the case for Chill variable-sized strings.  */
      if (size == -1 && TREE_CODE (type) == ARRAY_TYPE
	  && TYPE_ARRAY_MAX_SIZE (type) != NULL_TREE
	  && host_integerp (TYPE_ARRAY_MAX_SIZE (type), 1))
	size = tree_low_cst (TYPE_ARRAY_MAX_SIZE (type), 1);

      /* The size of the temporary may be too large to fit into an integer.  */
      /* ??? Not sure this should happen except for user silliness, so limit
	 this to things that aren't compiler-generated temporaries.  The
	 rest of the time we'll abort in assign_stack_temp_for_type.  */
      if (decl && size == -1
	  && TREE_CODE (TYPE_SIZE_UNIT (type)) == INTEGER_CST)
	{
	  error ("%Jsize of variable '%D' is too large", decl, decl);
	  size = 1;
	}

      tmp = assign_stack_temp_for_type (mode, size, keep, type);
      return tmp;
    }

#ifndef PROMOTE_FOR_CALL_ONLY
  if (! dont_promote)
    mode = promote_mode (type, mode, &unsignedp, 0);
#endif

  return gen_reg_rtx (mode);
}

/* Combine temporary stack slots which are adjacent on the stack.

   This allows for better use of already allocated stack space.  This is only
   done for BLKmode slots because we can be sure that we won't have alignment
   problems in this case.  */

void
combine_temp_slots (void)
{
  struct temp_slot *p, *q;
  struct temp_slot *prev_p, *prev_q;
  int num_slots;

  /* We can't combine slots, because the information about which slot
     is in which alias set will be lost.  */
  if (flag_strict_aliasing)
    return;

  /* If there are a lot of temp slots, don't do anything unless
     high levels of optimization.  */
  if (! flag_expensive_optimizations)
    for (p = temp_slots, num_slots = 0; p; p = p->next, num_slots++)
      if (num_slots > 100 || (num_slots > 10 && optimize == 0))
	return;

  for (p = temp_slots, prev_p = 0; p; p = prev_p ? prev_p->next : temp_slots)
    {
      int delete_p = 0;

      if (! p->in_use && GET_MODE (p->slot) == BLKmode)
	for (q = p->next, prev_q = p; q; q = prev_q->next)
	  {
	    int delete_q = 0;
	    if (! q->in_use && GET_MODE (q->slot) == BLKmode)
	      {
		if (p->base_offset + p->full_size == q->base_offset)
		  {
		    /* Q comes after P; combine Q into P.  */
		    p->size += q->size;
		    p->full_size += q->full_size;
		    delete_q = 1;
		  }
		else if (q->base_offset + q->full_size == p->base_offset)
		  {
		    /* P comes after Q; combine P into Q.  */
		    q->size += p->size;
		    q->full_size += p->full_size;
		    delete_p = 1;
		    break;
		  }
	      }
	    /* Either delete Q or advance past it.  */
	    if (delete_q)
	      prev_q->next = q->next;
	    else
	      prev_q = q;
	  }
      /* Either delete P or advance past it.  */
      if (delete_p)
	{
	  if (prev_p)
	    prev_p->next = p->next;
	  else
	    temp_slots = p->next;
	}
      else
	prev_p = p;
    }
}

/* Find the temp slot corresponding to the object at address X.  */

static struct temp_slot *
find_temp_slot_from_address (rtx x)
{
  struct temp_slot *p;
  rtx next;

  for (p = temp_slots; p; p = p->next)
    {
      if (! p->in_use)
	continue;

      else if (XEXP (p->slot, 0) == x
	       || p->address == x
	       || (GET_CODE (x) == PLUS
		   && XEXP (x, 0) == virtual_stack_vars_rtx
		   && GET_CODE (XEXP (x, 1)) == CONST_INT
		   && INTVAL (XEXP (x, 1)) >= p->base_offset
		   && INTVAL (XEXP (x, 1)) < p->base_offset + p->full_size))
	return p;

      else if (p->address != 0 && GET_CODE (p->address) == EXPR_LIST)
	for (next = p->address; next; next = XEXP (next, 1))
	  if (XEXP (next, 0) == x)
	    return p;
    }

  /* If we have a sum involving a register, see if it points to a temp
     slot.  */
  if (GET_CODE (x) == PLUS && GET_CODE (XEXP (x, 0)) == REG
      && (p = find_temp_slot_from_address (XEXP (x, 0))) != 0)
    return p;
  else if (GET_CODE (x) == PLUS && GET_CODE (XEXP (x, 1)) == REG
	   && (p = find_temp_slot_from_address (XEXP (x, 1))) != 0)
    return p;

  return 0;
}

/* Indicate that NEW is an alternate way of referring to the temp slot
   that previously was known by OLD.  */

void
update_temp_slot_address (rtx old, rtx new)
{
  struct temp_slot *p;

  if (rtx_equal_p (old, new))
    return;

  p = find_temp_slot_from_address (old);

  /* If we didn't find one, see if both OLD is a PLUS.  If so, and NEW
     is a register, see if one operand of the PLUS is a temporary
     location.  If so, NEW points into it.  Otherwise, if both OLD and
     NEW are a PLUS and if there is a register in common between them.
     If so, try a recursive call on those values.  */
  if (p == 0)
    {
      if (GET_CODE (old) != PLUS)
	return;

      if (GET_CODE (new) == REG)
	{
	  update_temp_slot_address (XEXP (old, 0), new);
	  update_temp_slot_address (XEXP (old, 1), new);
	  return;
	}
      else if (GET_CODE (new) != PLUS)
	return;

      if (rtx_equal_p (XEXP (old, 0), XEXP (new, 0)))
	update_temp_slot_address (XEXP (old, 1), XEXP (new, 1));
      else if (rtx_equal_p (XEXP (old, 1), XEXP (new, 0)))
	update_temp_slot_address (XEXP (old, 0), XEXP (new, 1));
      else if (rtx_equal_p (XEXP (old, 0), XEXP (new, 1)))
	update_temp_slot_address (XEXP (old, 1), XEXP (new, 0));
      else if (rtx_equal_p (XEXP (old, 1), XEXP (new, 1)))
	update_temp_slot_address (XEXP (old, 0), XEXP (new, 0));

      return;
    }

  /* Otherwise add an alias for the temp's address.  */
  else if (p->address == 0)
    p->address = new;
  else
    {
      if (GET_CODE (p->address) != EXPR_LIST)
	p->address = gen_rtx_EXPR_LIST (VOIDmode, p->address, NULL_RTX);

      p->address = gen_rtx_EXPR_LIST (VOIDmode, new, p->address);
    }
}

/* If X could be a reference to a temporary slot, mark the fact that its
   address was taken.  */

void
mark_temp_addr_taken (rtx x)
{
  struct temp_slot *p;

  if (x == 0)
    return;

  /* If X is not in memory or is at a constant address, it cannot be in
     a temporary slot.  */
  if (GET_CODE (x) != MEM || CONSTANT_P (XEXP (x, 0)))
    return;

  p = find_temp_slot_from_address (XEXP (x, 0));
  if (p != 0)
    p->addr_taken = 1;
}

/* If X could be a reference to a temporary slot, mark that slot as
   belonging to the to one level higher than the current level.  If X
   matched one of our slots, just mark that one.  Otherwise, we can't
   easily predict which it is, so upgrade all of them.  Kept slots
   need not be touched.

   This is called when an ({...}) construct occurs and a statement
   returns a value in memory.  */

void
preserve_temp_slots (rtx x)
{
  struct temp_slot *p = 0;

  /* If there is no result, we still might have some objects whose address
     were taken, so we need to make sure they stay around.  */
  if (x == 0)
    {
      for (p = temp_slots; p; p = p->next)
	if (p->in_use && p->level == temp_slot_level && p->addr_taken)
	  p->level--;

      return;
    }

  /* If X is a register that is being used as a pointer, see if we have
     a temporary slot we know it points to.  To be consistent with
     the code below, we really should preserve all non-kept slots
     if we can't find a match, but that seems to be much too costly.  */
  if (GET_CODE (x) == REG && REG_POINTER (x))
    p = find_temp_slot_from_address (x);

  /* If X is not in memory or is at a constant address, it cannot be in
     a temporary slot, but it can contain something whose address was
     taken.  */
  if (p == 0 && (GET_CODE (x) != MEM || CONSTANT_P (XEXP (x, 0))))
    {
      for (p = temp_slots; p; p = p->next)
	if (p->in_use && p->level == temp_slot_level && p->addr_taken)
	  p->level--;

      return;
    }

  /* First see if we can find a match.  */
  if (p == 0)
    p = find_temp_slot_from_address (XEXP (x, 0));

  if (p != 0)
    {
      /* Move everything at our level whose address was taken to our new
	 level in case we used its address.  */
      struct temp_slot *q;

      if (p->level == temp_slot_level)
	{
	  for (q = temp_slots; q; q = q->next)
	    if (q != p && q->addr_taken && q->level == p->level)
	      q->level--;

	  p->level--;
	  p->addr_taken = 0;
	}
      return;
    }

  /* Otherwise, preserve all non-kept slots at this level.  */
  for (p = temp_slots; p; p = p->next)
    if (p->in_use && p->level == temp_slot_level && ! p->keep)
      p->level--;
}

/* X is the result of an RTL_EXPR.  If it is a temporary slot associated
   with that RTL_EXPR, promote it into a temporary slot at the present
   level so it will not be freed when we free slots made in the
   RTL_EXPR.  */

void
preserve_rtl_expr_result (rtx x)
{
  struct temp_slot *p;

  /* If X is not in memory or is at a constant address, it cannot be in
     a temporary slot.  */
  if (x == 0 || GET_CODE (x) != MEM || CONSTANT_P (XEXP (x, 0)))
    return;

  /* If we can find a match, move it to our level unless it is already at
     an upper level.  */
  p = find_temp_slot_from_address (XEXP (x, 0));
  if (p != 0)
    {
      p->level = MIN (p->level, temp_slot_level);
      p->rtl_expr = 0;
    }

  return;
}

/* Free all temporaries used so far.  This is normally called at the end
   of generating code for a statement.  Don't free any temporaries
   currently in use for an RTL_EXPR that hasn't yet been emitted.
   We could eventually do better than this since it can be reused while
   generating the same RTL_EXPR, but this is complex and probably not
   worthwhile.  */

void
free_temp_slots (void)
{
  struct temp_slot *p;

  for (p = temp_slots; p; p = p->next)
    if (p->in_use && p->level == temp_slot_level && ! p->keep
	&& p->rtl_expr == 0)
      p->in_use = 0;

  combine_temp_slots ();
}

/* Free all temporary slots used in T, an RTL_EXPR node.  */

void
free_temps_for_rtl_expr (tree t)
{
  struct temp_slot *p;

  for (p = temp_slots; p; p = p->next)
    if (p->rtl_expr == t)
      {
	/* If this slot is below the current TEMP_SLOT_LEVEL, then it
	   needs to be preserved.  This can happen if a temporary in
	   the RTL_EXPR was addressed; preserve_temp_slots will move
	   the temporary into a higher level.  */
	if (temp_slot_level <= p->level)
	  p->in_use = 0;
	else
	  p->rtl_expr = NULL_TREE;
      }

  combine_temp_slots ();
}

/* Mark all temporaries ever allocated in this function as not suitable
   for reuse until the current level is exited.  */

void
mark_all_temps_used (void)
{
  struct temp_slot *p;

  for (p = temp_slots; p; p = p->next)
    {
      p->in_use = p->keep = 1;
      p->level = MIN (p->level, temp_slot_level);
    }
}

/* Push deeper into the nesting level for stack temporaries.  */

void
push_temp_slots (void)
{
  temp_slot_level++;
}

/* Pop a temporary nesting level.  All slots in use in the current level
   are freed.  */

void
pop_temp_slots (void)
{
  struct temp_slot *p;

  for (p = temp_slots; p; p = p->next)
    if (p->in_use && p->level == temp_slot_level && p->rtl_expr == 0)
      p->in_use = 0;

  combine_temp_slots ();

  temp_slot_level--;
}

/* Initialize temporary slots.  */

void
init_temp_slots (void)
{
  /* We have not allocated any temporaries yet.  */
  temp_slots = 0;
  temp_slot_level = 0;
  var_temp_slot_level = 0;
  target_temp_slot_level = 0;
}

/* Retroactively move an auto variable from a register to a stack
   slot.  This is done when an address-reference to the variable is
   seen.  If RESCAN is true, all previously emitted instructions are
   examined and modified to handle the fact that DECL is now
   addressable.  */

void
put_var_into_stack (tree decl, int rescan)
{
  rtx reg;
  enum machine_mode promoted_mode, decl_mode;
  struct function *function = 0;
  tree context;
  bool can_use_addressof_p;
  bool volatile_p = TREE_CODE (decl) != SAVE_EXPR && TREE_THIS_VOLATILE (decl);
  bool used_p = (TREE_USED (decl)
	       || (TREE_CODE (decl) != SAVE_EXPR && DECL_INITIAL (decl) != 0));

  context = decl_function_context (decl);

  /* Get the current rtl used for this object and its original mode.  */
  reg = (TREE_CODE (decl) == SAVE_EXPR
	 ? SAVE_EXPR_RTL (decl)
	 : DECL_RTL_IF_SET (decl));

  /* No need to do anything if decl has no rtx yet
     since in that case caller is setting TREE_ADDRESSABLE
     and a stack slot will be assigned when the rtl is made.  */
  if (reg == 0)
    return;

  /* Get the declared mode for this object.  */
  decl_mode = (TREE_CODE (decl) == SAVE_EXPR ? TYPE_MODE (TREE_TYPE (decl))
	       : DECL_MODE (decl));
  /* Get the mode it's actually stored in.  */
  promoted_mode = GET_MODE (reg);

  /* If this variable comes from an outer function, find that
     function's saved context.  Don't use find_function_data here,
     because it might not be in any active function.
     FIXME: Is that really supposed to happen?
     It does in ObjC at least.  */
  if (context != current_function_decl && context != inline_function_decl)
    for (function = outer_function_chain; function; function = function->outer)
      if (function->decl == context)
	break;

  /* If this is a variable-sized object or a structure passed by invisible
     reference, with a pseudo to address it, put that pseudo into the stack
     if the var is non-local.  */
  if (TREE_CODE (decl) != SAVE_EXPR && DECL_NONLOCAL (decl)
      && GET_CODE (reg) == MEM
      && GET_CODE (XEXP (reg, 0)) == REG
      && REGNO (XEXP (reg, 0)) > LAST_VIRTUAL_REGISTER)
    {
      reg = XEXP (reg, 0);
      decl_mode = promoted_mode = GET_MODE (reg);
    }

  /* If this variable lives in the current function and we don't need to put it
     in the stack for the sake of setjmp or the non-locality, try to keep it in
     a register until we know we actually need the address.  */
  can_use_addressof_p
    = (function == 0
       && ! (TREE_CODE (decl) != SAVE_EXPR && DECL_NONLOCAL (decl))
       && optimize > 0
       /* FIXME make it work for promoted modes too */
       && decl_mode == promoted_mode
#ifdef NON_SAVING_SETJMP
       && ! (NON_SAVING_SETJMP && current_function_calls_setjmp)
#endif
       );

  /* If we can't use ADDRESSOF, make sure we see through one we already
     generated.  */
  if (! can_use_addressof_p
      && GET_CODE (reg) == MEM
      && GET_CODE (XEXP (reg, 0)) == ADDRESSOF)
    reg = XEXP (XEXP (reg, 0), 0);

  /* Now we should have a value that resides in one or more pseudo regs.  */

  if (GET_CODE (reg) == REG)
    {
      if (can_use_addressof_p)
	gen_mem_addressof (reg, decl, rescan);
      else
	put_reg_into_stack (function, reg, TREE_TYPE (decl), decl_mode,
			    0, volatile_p, used_p, false, 0);
    }
  else if (GET_CODE (reg) == CONCAT)
    {
      /* A CONCAT contains two pseudos; put them both in the stack.
	 We do it so they end up consecutive.
	 We fixup references to the parts only after we fixup references
	 to the whole CONCAT, lest we do double fixups for the latter
	 references.  */
      enum machine_mode part_mode = GET_MODE (XEXP (reg, 0));
      tree part_type = (*lang_hooks.types.type_for_mode) (part_mode, 0);
      rtx lopart = XEXP (reg, 0);
      rtx hipart = XEXP (reg, 1);
#ifdef FRAME_GROWS_DOWNWARD
      /* Since part 0 should have a lower address, do it second.  */
      put_reg_into_stack (function, hipart, part_type, part_mode,
			  0, volatile_p, false, false, 0);
      put_reg_into_stack (function, lopart, part_type, part_mode,
			  0, volatile_p, false, true, 0);
#else
      put_reg_into_stack (function, lopart, part_type, part_mode,
			  0, volatile_p, false, false, 0);
      put_reg_into_stack (function, hipart, part_type, part_mode,
			  0, volatile_p, false, true, 0);
#endif

      /* Change the CONCAT into a combined MEM for both parts.  */
      PUT_CODE (reg, MEM);
      MEM_ATTRS (reg) = 0;

      /* set_mem_attributes uses DECL_RTL to avoid re-generating of
         already computed alias sets.  Here we want to re-generate.  */
      if (DECL_P (decl))
	SET_DECL_RTL (decl, NULL);
      set_mem_attributes (reg, decl, 1);
      if (DECL_P (decl))
	SET_DECL_RTL (decl, reg);

      /* The two parts are in memory order already.
	 Use the lower parts address as ours.  */
      XEXP (reg, 0) = XEXP (XEXP (reg, 0), 0);
      /* Prevent sharing of rtl that might lose.  */
      if (GET_CODE (XEXP (reg, 0)) == PLUS)
	XEXP (reg, 0) = copy_rtx (XEXP (reg, 0));
      if (used_p && rescan)
	{
	  schedule_fixup_var_refs (function, reg, TREE_TYPE (decl),
				   promoted_mode, 0);
	  schedule_fixup_var_refs (function, lopart, part_type, part_mode, 0);
	  schedule_fixup_var_refs (function, hipart, part_type, part_mode, 0);
	}
    }
  else
    return;
}

/* Subroutine of put_var_into_stack.  This puts a single pseudo reg REG
   into the stack frame of FUNCTION (0 means the current function).
   TYPE is the user-level data type of the value hold in the register.
   DECL_MODE is the machine mode of the user-level data type.
   ORIGINAL_REGNO must be set if the real regno is not visible in REG.
   VOLATILE_P is true if this is for a "volatile" decl.
   USED_P is true if this reg might have already been used in an insn.
   CONSECUTIVE_P is true if the stack slot assigned to reg must be
   consecutive with the previous stack slot.  */

static void
put_reg_into_stack (struct function *function, rtx reg, tree type,
		    enum machine_mode decl_mode, unsigned int original_regno,
		    bool volatile_p, bool used_p, bool consecutive_p,
		    htab_t ht)
{
  struct function *func = function ? function : cfun;
  enum machine_mode mode = GET_MODE (reg);
  unsigned int regno = original_regno;
  rtx new = 0;

  if (regno == 0)
    regno = REGNO (reg);

  if (regno < func->x_max_parm_reg)
    {
      if (!func->x_parm_reg_stack_loc)
	abort ();
      new = func->x_parm_reg_stack_loc[regno];
    }

  if (new == 0)
    new = assign_stack_local_1 (decl_mode, GET_MODE_SIZE (decl_mode),
				consecutive_p ? -2 : 0, func);

  PUT_CODE (reg, MEM);
  PUT_MODE (reg, decl_mode);
  XEXP (reg, 0) = XEXP (new, 0);
  MEM_ATTRS (reg) = 0;
  /* `volatil' bit means one thing for MEMs, another entirely for REGs.  */
  MEM_VOLATILE_P (reg) = volatile_p;

  /* If this is a memory ref that contains aggregate components,
     mark it as such for cse and loop optimize.  If we are reusing a
     previously generated stack slot, then we need to copy the bit in
     case it was set for other reasons.  For instance, it is set for
     __builtin_va_alist.  */
  if (type)
    {
      MEM_SET_IN_STRUCT_P (reg,
			   AGGREGATE_TYPE_P (type) || MEM_IN_STRUCT_P (new));
      set_mem_alias_set (reg, get_alias_set (type));
    }

  if (used_p)
    schedule_fixup_var_refs (function, reg, type, mode, ht);
}

/* Make sure that all refs to the variable, previously made
   when it was a register, are fixed up to be valid again.
   See function above for meaning of arguments.  */

static void
schedule_fixup_var_refs (struct function *function, rtx reg, tree type,
			 enum machine_mode promoted_mode, htab_t ht)
{
  int unsigned_p = type ? TREE_UNSIGNED (type) : 0;

  if (function != 0)
    {
      struct var_refs_queue *temp;

      temp = ggc_alloc (sizeof (struct var_refs_queue));
      temp->modified = reg;
      temp->promoted_mode = promoted_mode;
      temp->unsignedp = unsigned_p;
      temp->next = function->fixup_var_refs_queue;
      function->fixup_var_refs_queue = temp;
    }
  else
    /* Variable is local; fix it up now.  */
    fixup_var_refs (reg, promoted_mode, unsigned_p, reg, ht);
}

static void
fixup_var_refs (rtx var, enum machine_mode promoted_mode, int unsignedp,
		rtx may_share, htab_t ht)
{
  tree pending;
  rtx first_insn = get_insns ();
  struct sequence_stack *stack = seq_stack;
  tree rtl_exps = rtl_expr_chain;

  /* If there's a hash table, it must record all uses of VAR.  */
  if (ht)
    {
      if (stack != 0)
	abort ();
      fixup_var_refs_insns_with_hash (ht, var, promoted_mode, unsignedp,
				      may_share);
      return;
    }

  fixup_var_refs_insns (first_insn, var, promoted_mode, unsignedp,
			stack == 0, may_share);

  /* Scan all pending sequences too.  */
  for (; stack; stack = stack->next)
    {
      push_to_full_sequence (stack->first, stack->last);
      fixup_var_refs_insns (stack->first, var, promoted_mode, unsignedp,
			    stack->next != 0, may_share);
      /* Update remembered end of sequence
	 in case we added an insn at the end.  */
      stack->last = get_last_insn ();
      end_sequence ();
    }

  /* Scan all waiting RTL_EXPRs too.  */
  for (pending = rtl_exps; pending; pending = TREE_CHAIN (pending))
    {
      rtx seq = RTL_EXPR_SEQUENCE (TREE_VALUE (pending));
      if (seq != const0_rtx && seq != 0)
	{
	  push_to_sequence (seq);
	  fixup_var_refs_insns (seq, var, promoted_mode, unsignedp, 0,
				may_share);
	  end_sequence ();
	}
    }
}

/* REPLACEMENTS is a pointer to a list of the struct fixup_replacement and X is
   some part of an insn.  Return a struct fixup_replacement whose OLD
   value is equal to X.  Allocate a new structure if no such entry exists.  */

static struct fixup_replacement *
find_fixup_replacement (struct fixup_replacement **replacements, rtx x)
{
  struct fixup_replacement *p;

  /* See if we have already replaced this.  */
  for (p = *replacements; p != 0 && ! rtx_equal_p (p->old, x); p = p->next)
    ;

  if (p == 0)
    {
      p = xmalloc (sizeof (struct fixup_replacement));
      p->old = x;
      p->new = 0;
      p->next = *replacements;
      *replacements = p;
    }

  return p;
}

/* Scan the insn-chain starting with INSN for refs to VAR and fix them
   up.  TOPLEVEL is nonzero if this chain is the main chain of insns
   for the current function.  MAY_SHARE is either a MEM that is not
   to be unshared or a list of them.  */

static void
fixup_var_refs_insns (rtx insn, rtx var, enum machine_mode promoted_mode,
		      int unsignedp, int toplevel, rtx may_share)
{
  while (insn)
    {
      /* fixup_var_refs_insn might modify insn, so save its next
         pointer now.  */
      rtx next = NEXT_INSN (insn);

      /* CALL_PLACEHOLDERs are special; we have to switch into each of
	 the three sequences they (potentially) contain, and process
	 them recursively.  The CALL_INSN itself is not interesting.  */

      if (GET_CODE (insn) == CALL_INSN
	  && GET_CODE (PATTERN (insn)) == CALL_PLACEHOLDER)
	{
	  int i;

	  /* Look at the Normal call, sibling call and tail recursion
	     sequences attached to the CALL_PLACEHOLDER.  */
	  for (i = 0; i < 3; i++)
	    {
	      rtx seq = XEXP (PATTERN (insn), i);
	      if (seq)
		{
		  push_to_sequence (seq);
		  fixup_var_refs_insns (seq, var, promoted_mode, unsignedp, 0,
					may_share);
		  XEXP (PATTERN (insn), i) = get_insns ();
		  end_sequence ();
		}
	    }
	}

      else if (INSN_P (insn))
	fixup_var_refs_insn (insn, var, promoted_mode, unsignedp, toplevel,
			     may_share);

      insn = next;
    }
}

/* Look up the insns which reference VAR in HT and fix them up.  Other
   arguments are the same as fixup_var_refs_insns.

   N.B. No need for special processing of CALL_PLACEHOLDERs here,
   because the hash table will point straight to the interesting insn
   (inside the CALL_PLACEHOLDER).  */

static void
fixup_var_refs_insns_with_hash (htab_t ht, rtx var, enum machine_mode promoted_mode,
				int unsignedp, rtx may_share)
{
  struct insns_for_mem_entry tmp;
  struct insns_for_mem_entry *ime;
  rtx insn_list;

  tmp.key = var;
  ime = htab_find (ht, &tmp);
  for (insn_list = ime->insns; insn_list != 0; insn_list = XEXP (insn_list, 1))
    if (INSN_P (XEXP (insn_list, 0)) && !INSN_DELETED_P (XEXP (insn_list, 0)))
      fixup_var_refs_insn (XEXP (insn_list, 0), var, promoted_mode,
			   unsignedp, 1, may_share);
}


/* Per-insn processing by fixup_var_refs_insns(_with_hash).  INSN is
   the insn under examination, VAR is the variable to fix up
   references to, PROMOTED_MODE and UNSIGNEDP describe VAR, and
   TOPLEVEL is nonzero if this is the main insn chain for this
   function.  */

static void
fixup_var_refs_insn (rtx insn, rtx var, enum machine_mode promoted_mode,
		     int unsignedp, int toplevel, rtx no_share)
{
  rtx call_dest = 0;
  rtx set, prev, prev_set;
  rtx note;

  /* Remember the notes in case we delete the insn.  */
  note = REG_NOTES (insn);

  /* If this is a CLOBBER of VAR, delete it.

     If it has a REG_LIBCALL note, delete the REG_LIBCALL
     and REG_RETVAL notes too.  */
  if (GET_CODE (PATTERN (insn)) == CLOBBER
      && (XEXP (PATTERN (insn), 0) == var
	  || (GET_CODE (XEXP (PATTERN (insn), 0)) == CONCAT
	      && (XEXP (XEXP (PATTERN (insn), 0), 0) == var
		  || XEXP (XEXP (PATTERN (insn), 0), 1) == var))))
    {
      if ((note = find_reg_note (insn, REG_LIBCALL, NULL_RTX)) != 0)
	/* The REG_LIBCALL note will go away since we are going to
	   turn INSN into a NOTE, so just delete the
	   corresponding REG_RETVAL note.  */
	remove_note (XEXP (note, 0),
		     find_reg_note (XEXP (note, 0), REG_RETVAL,
				    NULL_RTX));

      delete_insn (insn);
    }

  /* The insn to load VAR from a home in the arglist
     is now a no-op.  When we see it, just delete it.
     Similarly if this is storing VAR from a register from which
     it was loaded in the previous insn.  This will occur
     when an ADDRESSOF was made for an arglist slot.  */
  else if (toplevel
	   && (set = single_set (insn)) != 0
	   && SET_DEST (set) == var
	   /* If this represents the result of an insn group,
	      don't delete the insn.  */
	   && find_reg_note (insn, REG_RETVAL, NULL_RTX) == 0
	   && (rtx_equal_p (SET_SRC (set), var)
	       || (GET_CODE (SET_SRC (set)) == REG
		   && (prev = prev_nonnote_insn (insn)) != 0
		   && (prev_set = single_set (prev)) != 0
		   && SET_DEST (prev_set) == SET_SRC (set)
		   && rtx_equal_p (SET_SRC (prev_set), var))))
    {
      delete_insn (insn);
    }
  else
    {
      struct fixup_replacement *replacements = 0;
      rtx next_insn = NEXT_INSN (insn);

      if (SMALL_REGISTER_CLASSES)
	{
	  /* If the insn that copies the results of a CALL_INSN
	     into a pseudo now references VAR, we have to use an
	     intermediate pseudo since we want the life of the
	     return value register to be only a single insn.

	     If we don't use an intermediate pseudo, such things as
	     address computations to make the address of VAR valid
	     if it is not can be placed between the CALL_INSN and INSN.

	     To make sure this doesn't happen, we record the destination
	     of the CALL_INSN and see if the next insn uses both that
	     and VAR.  */

	  if (call_dest != 0 && GET_CODE (insn) == INSN
	      && reg_mentioned_p (var, PATTERN (insn))
	      && reg_mentioned_p (call_dest, PATTERN (insn)))
	    {
	      rtx temp = gen_reg_rtx (GET_MODE (call_dest));

	      emit_insn_before (gen_move_insn (temp, call_dest), insn);

	      PATTERN (insn) = replace_rtx (PATTERN (insn),
					    call_dest, temp);
	    }

	  if (GET_CODE (insn) == CALL_INSN
	      && GET_CODE (PATTERN (insn)) == SET)
	    call_dest = SET_DEST (PATTERN (insn));
	  else if (GET_CODE (insn) == CALL_INSN
		   && GET_CODE (PATTERN (insn)) == PARALLEL
		   && GET_CODE (XVECEXP (PATTERN (insn), 0, 0)) == SET)
	    call_dest = SET_DEST (XVECEXP (PATTERN (insn), 0, 0));
	  else
	    call_dest = 0;
	}

      /* See if we have to do anything to INSN now that VAR is in
	 memory.  If it needs to be loaded into a pseudo, use a single
	 pseudo for the entire insn in case there is a MATCH_DUP
	 between two operands.  We pass a pointer to the head of
	 a list of struct fixup_replacements.  If fixup_var_refs_1
	 needs to allocate pseudos or replacement MEMs (for SUBREGs),
	 it will record them in this list.

	 If it allocated a pseudo for any replacement, we copy into
	 it here.  */

      fixup_var_refs_1 (var, promoted_mode, &PATTERN (insn), insn,
			&replacements, no_share);

      /* If this is last_parm_insn, and any instructions were output
	 after it to fix it up, then we must set last_parm_insn to
	 the last such instruction emitted.  */
      if (insn == last_parm_insn)
	last_parm_insn = PREV_INSN (next_insn);

      while (replacements)
	{
	  struct fixup_replacement *next;

	  if (GET_CODE (replacements->new) == REG)
	    {
	      rtx insert_before;
	      rtx seq;

	      /* OLD might be a (subreg (mem)).  */
	      if (GET_CODE (replacements->old) == SUBREG)
		replacements->old
		  = fixup_memory_subreg (replacements->old, insn,
					 promoted_mode, 0);
	      else
		replacements->old
		  = fixup_stack_1 (replacements->old, insn);

	      insert_before = insn;

	      /* If we are changing the mode, do a conversion.
		 This might be wasteful, but combine.c will
		 eliminate much of the waste.  */

	      if (GET_MODE (replacements->new)
		  != GET_MODE (replacements->old))
		{
		  start_sequence ();
		  convert_move (replacements->new,
				replacements->old, unsignedp);
		  seq = get_insns ();
		  end_sequence ();
		}
	      else
		seq = gen_move_insn (replacements->new,
				     replacements->old);

	      emit_insn_before (seq, insert_before);
	    }

	  next = replacements->next;
	  free (replacements);
	  replacements = next;
	}
    }

  /* Also fix up any invalid exprs in the REG_NOTES of this insn.
     But don't touch other insns referred to by reg-notes;
     we will get them elsewhere.  */
  while (note)
    {
      if (GET_CODE (note) != INSN_LIST)
	XEXP (note, 0)
	  = walk_fixup_memory_subreg (XEXP (note, 0), insn,
				      promoted_mode, 1);
      note = XEXP (note, 1);
    }
}

/* VAR is a MEM that used to be a pseudo register with mode PROMOTED_MODE.
   See if the rtx expression at *LOC in INSN needs to be changed.

   REPLACEMENTS is a pointer to a list head that starts out zero, but may
   contain a list of original rtx's and replacements. If we find that we need
   to modify this insn by replacing a memory reference with a pseudo or by
   making a new MEM to implement a SUBREG, we consult that list to see if
   we have already chosen a replacement. If none has already been allocated,
   we allocate it and update the list.  fixup_var_refs_insn will copy VAR
   or the SUBREG, as appropriate, to the pseudo.  */

static void
fixup_var_refs_1 (rtx var, enum machine_mode promoted_mode, rtx *loc, rtx insn,
		  struct fixup_replacement **replacements, rtx no_share)
{
  int i;
  rtx x = *loc;
  RTX_CODE code = GET_CODE (x);
  const char *fmt;
  rtx tem, tem1;
  struct fixup_replacement *replacement;

  switch (code)
    {
    case ADDRESSOF:
      if (XEXP (x, 0) == var)
	{
	  /* Prevent sharing of rtl that might lose.  */
	  rtx sub = copy_rtx (XEXP (var, 0));

	  if (! validate_change (insn, loc, sub, 0))
	    {
	      rtx y = gen_reg_rtx (GET_MODE (sub));
	      rtx seq, new_insn;

	      /* We should be able to replace with a register or all is lost.
		 Note that we can't use validate_change to verify this, since
		 we're not caring for replacing all dups simultaneously.  */
	      if (! validate_replace_rtx (*loc, y, insn))
		abort ();

	      /* Careful!  First try to recognize a direct move of the
		 value, mimicking how things are done in gen_reload wrt
		 PLUS.  Consider what happens when insn is a conditional
		 move instruction and addsi3 clobbers flags.  */

	      start_sequence ();
	      new_insn = emit_insn (gen_rtx_SET (VOIDmode, y, sub));
	      seq = get_insns ();
	      end_sequence ();

	      if (recog_memoized (new_insn) < 0)
		{
		  /* That failed.  Fall back on force_operand and hope.  */

		  start_sequence ();
		  sub = force_operand (sub, y);
		  if (sub != y)
		    emit_insn (gen_move_insn (y, sub));
		  seq = get_insns ();
		  end_sequence ();
		}

#ifdef HAVE_cc0
	      /* Don't separate setter from user.  */
	      if (PREV_INSN (insn) && sets_cc0_p (PREV_INSN (insn)))
		insn = PREV_INSN (insn);
#endif

	      emit_insn_before (seq, insn);
	    }
	}
      return;

    case MEM:
      if (var == x)
	{
	  /* If we already have a replacement, use it.  Otherwise,
	     try to fix up this address in case it is invalid.  */

	  replacement = find_fixup_replacement (replacements, var);
	  if (replacement->new)
	    {
	      *loc = replacement->new;
	      return;
	    }

	  *loc = replacement->new = x = fixup_stack_1 (x, insn);

	  /* Unless we are forcing memory to register or we changed the mode,
	     we can leave things the way they are if the insn is valid.  */

	  INSN_CODE (insn) = -1;
	  if (! flag_force_mem && GET_MODE (x) == promoted_mode
	      && recog_memoized (insn) >= 0)
	    return;

	  *loc = replacement->new = gen_reg_rtx (promoted_mode);
	  return;
	}

      /* If X contains VAR, we need to unshare it here so that we update
	 each occurrence separately.  But all identical MEMs in one insn
	 must be replaced with the same rtx because of the possibility of
	 MATCH_DUPs.  */

      if (reg_mentioned_p (var, x))
	{
	  replacement = find_fixup_replacement (replacements, x);
	  if (replacement->new == 0)
	    replacement->new = copy_most_rtx (x, no_share);

	  *loc = x = replacement->new;
	  code = GET_CODE (x);
	}
      break;

    case REG:
    case CC0:
    case PC:
    case CONST_INT:
    case CONST:
    case SYMBOL_REF:
    case LABEL_REF:
    case CONST_DOUBLE:
    case CONST_VECTOR:
      return;

    case SIGN_EXTRACT:
    case ZERO_EXTRACT:
      /* Note that in some cases those types of expressions are altered
	 by optimize_bit_field, and do not survive to get here.  */
      if (XEXP (x, 0) == var
	  || (GET_CODE (XEXP (x, 0)) == SUBREG
	      && SUBREG_REG (XEXP (x, 0)) == var))
	{
	  /* Get TEM as a valid MEM in the mode presently in the insn.

	     We don't worry about the possibility of MATCH_DUP here; it
	     is highly unlikely and would be tricky to handle.  */

	  tem = XEXP (x, 0);
	  if (GET_CODE (tem) == SUBREG)
	    {
	      if (GET_MODE_BITSIZE (GET_MODE (tem))
		  > GET_MODE_BITSIZE (GET_MODE (var)))
		{
		  replacement = find_fixup_replacement (replacements, var);
		  if (replacement->new == 0)
		    replacement->new = gen_reg_rtx (GET_MODE (var));
		  SUBREG_REG (tem) = replacement->new;

		  /* The following code works only if we have a MEM, so we
		     need to handle the subreg here.  We directly substitute
		     it assuming that a subreg must be OK here.  We already
		     scheduled a replacement to copy the mem into the
		     subreg.  */
		  XEXP (x, 0) = tem;
		  return;
		}
	      else
		tem = fixup_memory_subreg (tem, insn, promoted_mode, 0);
	    }
	  else
	    tem = fixup_stack_1 (tem, insn);

	  /* Unless we want to load from memory, get TEM into the proper mode
	     for an extract from memory.  This can only be done if the
	     extract is at a constant position and length.  */

	  if (! flag_force_mem && GET_CODE (XEXP (x, 1)) == CONST_INT
	      && GET_CODE (XEXP (x, 2)) == CONST_INT
	      && ! mode_dependent_address_p (XEXP (tem, 0))
	      && ! MEM_VOLATILE_P (tem))
	    {
	      enum machine_mode wanted_mode = VOIDmode;
	      enum machine_mode is_mode = GET_MODE (tem);
	      HOST_WIDE_INT pos = INTVAL (XEXP (x, 2));

	      if (GET_CODE (x) == ZERO_EXTRACT)
		{
		  enum machine_mode new_mode
		    = mode_for_extraction (EP_extzv, 1);
		  if (new_mode != MAX_MACHINE_MODE)
		    wanted_mode = new_mode;
		}
	      else if (GET_CODE (x) == SIGN_EXTRACT)
		{
		  enum machine_mode new_mode
		    = mode_for_extraction (EP_extv, 1);
		  if (new_mode != MAX_MACHINE_MODE)
		    wanted_mode = new_mode;
		}

	      /* If we have a narrower mode, we can do something.  */
	      if (wanted_mode != VOIDmode
		  && GET_MODE_SIZE (wanted_mode) < GET_MODE_SIZE (is_mode))
		{
		  HOST_WIDE_INT offset = pos / BITS_PER_UNIT;
		  rtx old_pos = XEXP (x, 2);
		  rtx newmem;

		  /* If the bytes and bits are counted differently, we
		     must adjust the offset.  */
		  if (BYTES_BIG_ENDIAN != BITS_BIG_ENDIAN)
		    offset = (GET_MODE_SIZE (is_mode)
			      - GET_MODE_SIZE (wanted_mode) - offset);

		  pos %= GET_MODE_BITSIZE (wanted_mode);

		  newmem = adjust_address_nv (tem, wanted_mode, offset);

		  /* Make the change and see if the insn remains valid.  */
		  INSN_CODE (insn) = -1;
		  XEXP (x, 0) = newmem;
		  XEXP (x, 2) = GEN_INT (pos);

		  if (recog_memoized (insn) >= 0)
		    return;

		  /* Otherwise, restore old position.  XEXP (x, 0) will be
		     restored later.  */
		  XEXP (x, 2) = old_pos;
		}
	    }

	  /* If we get here, the bitfield extract insn can't accept a memory
	     reference.  Copy the input into a register.  */

	  tem1 = gen_reg_rtx (GET_MODE (tem));
	  emit_insn_before (gen_move_insn (tem1, tem), insn);
	  XEXP (x, 0) = tem1;
	  return;
	}
      break;

    case SUBREG:
      if (SUBREG_REG (x) == var)
	{
	  /* If this is a special SUBREG made because VAR was promoted
	     from a wider mode, replace it with VAR and call ourself
	     recursively, this time saying that the object previously
	     had its current mode (by virtue of the SUBREG).  */

	  if (SUBREG_PROMOTED_VAR_P (x))
	    {
	      *loc = var;
	      fixup_var_refs_1 (var, GET_MODE (var), loc, insn, replacements,
				no_share);
	      return;
	    }

	  /* If this SUBREG makes VAR wider, it has become a paradoxical
	     SUBREG with VAR in memory, but these aren't allowed at this
	     stage of the compilation.  So load VAR into a pseudo and take
	     a SUBREG of that pseudo.  */
	  if (GET_MODE_SIZE (GET_MODE (x)) > GET_MODE_SIZE (GET_MODE (var)))
	    {
	      replacement = find_fixup_replacement (replacements, var);
	      if (replacement->new == 0)
		replacement->new = gen_reg_rtx (promoted_mode);
	      SUBREG_REG (x) = replacement->new;
	      return;
	    }

	  /* See if we have already found a replacement for this SUBREG.
	     If so, use it.  Otherwise, make a MEM and see if the insn
	     is recognized.  If not, or if we should force MEM into a register,
	     make a pseudo for this SUBREG.  */
	  replacement = find_fixup_replacement (replacements, x);
	  if (replacement->new)
	    {
	      enum machine_mode mode = GET_MODE (x);
	      *loc = replacement->new;

	      /* Careful!  We may have just replaced a SUBREG by a MEM, which
		 means that the insn may have become invalid again.  We can't
		 in this case make a new replacement since we already have one
		 and we must deal with MATCH_DUPs.  */
	      if (GET_CODE (replacement->new) == MEM)
		{
		  INSN_CODE (insn) = -1;
		  if (recog_memoized (insn) >= 0)
		    return;

		  fixup_var_refs_1 (replacement->new, mode, &PATTERN (insn),
				    insn, replacements, no_share);
		}

	      return;
	    }

	  replacement->new = *loc = fixup_memory_subreg (x, insn,
							 promoted_mode, 0);

	  INSN_CODE (insn) = -1;
	  if (! flag_force_mem && recog_memoized (insn) >= 0)
	    return;

	  *loc = replacement->new = gen_reg_rtx (GET_MODE (x));
	  return;
	}
      break;

    case SET:
      /* First do special simplification of bit-field references.  */
      if (GET_CODE (SET_DEST (x)) == SIGN_EXTRACT
	  || GET_CODE (SET_DEST (x)) == ZERO_EXTRACT)
	optimize_bit_field (x, insn, 0);
      if (GET_CODE (SET_SRC (x)) == SIGN_EXTRACT
	  || GET_CODE (SET_SRC (x)) == ZERO_EXTRACT)
	optimize_bit_field (x, insn, 0);

      /* For a paradoxical SUBREG inside a ZERO_EXTRACT, load the object
	 into a register and then store it back out.  */
      if (GET_CODE (SET_DEST (x)) == ZERO_EXTRACT
	  && GET_CODE (XEXP (SET_DEST (x), 0)) == SUBREG
	  && SUBREG_REG (XEXP (SET_DEST (x), 0)) == var
	  && (GET_MODE_SIZE (GET_MODE (XEXP (SET_DEST (x), 0)))
	      > GET_MODE_SIZE (GET_MODE (var))))
	{
	  replacement = find_fixup_replacement (replacements, var);
	  if (replacement->new == 0)
	    replacement->new = gen_reg_rtx (GET_MODE (var));

	  SUBREG_REG (XEXP (SET_DEST (x), 0)) = replacement->new;
	  emit_insn_after (gen_move_insn (var, replacement->new), insn);
	}

      /* If SET_DEST is now a paradoxical SUBREG, put the result of this
	 insn into a pseudo and store the low part of the pseudo into VAR.  */
      if (GET_CODE (SET_DEST (x)) == SUBREG
	  && SUBREG_REG (SET_DEST (x)) == var
	  && (GET_MODE_SIZE (GET_MODE (SET_DEST (x)))
	      > GET_MODE_SIZE (GET_MODE (var))))
	{
	  SET_DEST (x) = tem = gen_reg_rtx (GET_MODE (SET_DEST (x)));
	  emit_insn_after (gen_move_insn (var, gen_lowpart (GET_MODE (var),
							    tem)),
			   insn);
	  break;
	}

      {
	rtx dest = SET_DEST (x);
	rtx src = SET_SRC (x);
	rtx outerdest = dest;

	while (GET_CODE (dest) == SUBREG || GET_CODE (dest) == STRICT_LOW_PART
	       || GET_CODE (dest) == SIGN_EXTRACT
	       || GET_CODE (dest) == ZERO_EXTRACT)
	  dest = XEXP (dest, 0);

	if (GET_CODE (src) == SUBREG)
	  src = SUBREG_REG (src);

	/* If VAR does not appear at the top level of the SET
	   just scan the lower levels of the tree.  */

	if (src != var && dest != var)
	  break;

	/* We will need to rerecognize this insn.  */
	INSN_CODE (insn) = -1;

	if (GET_CODE (outerdest) == ZERO_EXTRACT && dest == var
	    && mode_for_extraction (EP_insv, -1) != MAX_MACHINE_MODE)
	  {
	    /* Since this case will return, ensure we fixup all the
	       operands here.  */
	    fixup_var_refs_1 (var, promoted_mode, &XEXP (outerdest, 1),
			      insn, replacements, no_share);
	    fixup_var_refs_1 (var, promoted_mode, &XEXP (outerdest, 2),
			      insn, replacements, no_share);
	    fixup_var_refs_1 (var, promoted_mode, &SET_SRC (x),
			      insn, replacements, no_share);

	    tem = XEXP (outerdest, 0);

	    /* Clean up (SUBREG:SI (MEM:mode ...) 0)
	       that may appear inside a ZERO_EXTRACT.
	       This was legitimate when the MEM was a REG.  */
	    if (GET_CODE (tem) == SUBREG
		&& SUBREG_REG (tem) == var)
	      tem = fixup_memory_subreg (tem, insn, promoted_mode, 0);
	    else
	      tem = fixup_stack_1 (tem, insn);

	    if (GET_CODE (XEXP (outerdest, 1)) == CONST_INT
		&& GET_CODE (XEXP (outerdest, 2)) == CONST_INT
		&& ! mode_dependent_address_p (XEXP (tem, 0))
		&& ! MEM_VOLATILE_P (tem))
	      {
		enum machine_mode wanted_mode;
		enum machine_mode is_mode = GET_MODE (tem);
		HOST_WIDE_INT pos = INTVAL (XEXP (outerdest, 2));

		wanted_mode = mode_for_extraction (EP_insv, 0);

		/* If we have a narrower mode, we can do something.  */
		if (GET_MODE_SIZE (wanted_mode) < GET_MODE_SIZE (is_mode))
		  {
		    HOST_WIDE_INT offset = pos / BITS_PER_UNIT;
		    rtx old_pos = XEXP (outerdest, 2);
		    rtx newmem;

		    if (BYTES_BIG_ENDIAN != BITS_BIG_ENDIAN)
		      offset = (GET_MODE_SIZE (is_mode)
				- GET_MODE_SIZE (wanted_mode) - offset);

		    pos %= GET_MODE_BITSIZE (wanted_mode);

		    newmem = adjust_address_nv (tem, wanted_mode, offset);

		    /* Make the change and see if the insn remains valid.  */
		    INSN_CODE (insn) = -1;
		    XEXP (outerdest, 0) = newmem;
		    XEXP (outerdest, 2) = GEN_INT (pos);

		    if (recog_memoized (insn) >= 0)
		      return;

		    /* Otherwise, restore old position.  XEXP (x, 0) will be
		       restored later.  */
		    XEXP (outerdest, 2) = old_pos;
		  }
	      }

	    /* If we get here, the bit-field store doesn't allow memory
	       or isn't located at a constant position.  Load the value into
	       a register, do the store, and put it back into memory.  */

	    tem1 = gen_reg_rtx (GET_MODE (tem));
	    emit_insn_before (gen_move_insn (tem1, tem), insn);
	    emit_insn_after (gen_move_insn (tem, tem1), insn);
	    XEXP (outerdest, 0) = tem1;
	    return;
	  }

	/* STRICT_LOW_PART is a no-op on memory references
	   and it can cause combinations to be unrecognizable,
	   so eliminate it.  */

	if (dest == var && GET_CODE (SET_DEST (x)) == STRICT_LOW_PART)
	  SET_DEST (x) = XEXP (SET_DEST (x), 0);

	/* A valid insn to copy VAR into or out of a register
	   must be left alone, to avoid an infinite loop here.
	   If the reference to VAR is by a subreg, fix that up,
	   since SUBREG is not valid for a memref.
	   Also fix up the address of the stack slot.

	   Note that we must not try to recognize the insn until
	   after we know that we have valid addresses and no
	   (subreg (mem ...) ...) constructs, since these interfere
	   with determining the validity of the insn.  */

	if ((SET_SRC (x) == var
	     || (GET_CODE (SET_SRC (x)) == SUBREG
		 && SUBREG_REG (SET_SRC (x)) == var))
	    && (GET_CODE (SET_DEST (x)) == REG
		|| (GET_CODE (SET_DEST (x)) == SUBREG
		    && GET_CODE (SUBREG_REG (SET_DEST (x))) == REG))
	    && GET_MODE (var) == promoted_mode
	    && x == single_set (insn))
	  {
	    rtx pat, last;

	    if (GET_CODE (SET_SRC (x)) == SUBREG
		&& (GET_MODE_SIZE (GET_MODE (SET_SRC (x)))
		    > GET_MODE_SIZE (GET_MODE (var))))
	      {
		/* This (subreg VAR) is now a paradoxical subreg.  We need
		   to replace VAR instead of the subreg.  */
		replacement = find_fixup_replacement (replacements, var);
		if (replacement->new == NULL_RTX)
		  replacement->new = gen_reg_rtx (GET_MODE (var));
		SUBREG_REG (SET_SRC (x)) = replacement->new;
	      }
	    else
	      {
		replacement = find_fixup_replacement (replacements, SET_SRC (x));
		if (replacement->new)
		  SET_SRC (x) = replacement->new;
		else if (GET_CODE (SET_SRC (x)) == SUBREG)
		  SET_SRC (x) = replacement->new
		    = fixup_memory_subreg (SET_SRC (x), insn, promoted_mode,
					   0);
		else
		  SET_SRC (x) = replacement->new
		    = fixup_stack_1 (SET_SRC (x), insn);
	      }

	    if (recog_memoized (insn) >= 0)
	      return;

	    /* INSN is not valid, but we know that we want to
	       copy SET_SRC (x) to SET_DEST (x) in some way.  So
	       we generate the move and see whether it requires more
	       than one insn.  If it does, we emit those insns and
	       delete INSN.  Otherwise, we can just replace the pattern
	       of INSN; we have already verified above that INSN has
	       no other function that to do X.  */

	    pat = gen_move_insn (SET_DEST (x), SET_SRC (x));
	    if (NEXT_INSN (pat) != NULL_RTX)
	      {
		last = emit_insn_before (pat, insn);

		/* INSN might have REG_RETVAL or other important notes, so
		   we need to store the pattern of the last insn in the
		   sequence into INSN similarly to the normal case.  LAST
		   should not have REG_NOTES, but we allow them if INSN has
		   no REG_NOTES.  */
		if (REG_NOTES (last) && REG_NOTES (insn))
		  abort ();
		if (REG_NOTES (last))
		  REG_NOTES (insn) = REG_NOTES (last);
		PATTERN (insn) = PATTERN (last);

		delete_insn (last);
	      }
	    else
	      PATTERN (insn) = PATTERN (pat);

	    return;
	  }

	if ((SET_DEST (x) == var
	     || (GET_CODE (SET_DEST (x)) == SUBREG
		 && SUBREG_REG (SET_DEST (x)) == var))
	    && (GET_CODE (SET_SRC (x)) == REG
		|| (GET_CODE (SET_SRC (x)) == SUBREG
		    && GET_CODE (SUBREG_REG (SET_SRC (x))) == REG))
	    && GET_MODE (var) == promoted_mode
	    && x == single_set (insn))
	  {
	    rtx pat, last;

	    if (GET_CODE (SET_DEST (x)) == SUBREG)
	      SET_DEST (x) = fixup_memory_subreg (SET_DEST (x), insn,
						  promoted_mode, 0);
	    else
	      SET_DEST (x) = fixup_stack_1 (SET_DEST (x), insn);

	    if (recog_memoized (insn) >= 0)
	      return;

	    pat = gen_move_insn (SET_DEST (x), SET_SRC (x));
	    if (NEXT_INSN (pat) != NULL_RTX)
	      {
		last = emit_insn_before (pat, insn);

		/* INSN might have REG_RETVAL or other important notes, so
		   we need to store the pattern of the last insn in the
		   sequence into INSN similarly to the normal case.  LAST
		   should not have REG_NOTES, but we allow them if INSN has
		   no REG_NOTES.  */
		if (REG_NOTES (last) && REG_NOTES (insn))
		  abort ();
		if (REG_NOTES (last))
		  REG_NOTES (insn) = REG_NOTES (last);
		PATTERN (insn) = PATTERN (last);

		delete_insn (last);
	      }
	    else
	      PATTERN (insn) = PATTERN (pat);

	    return;
	  }

	/* Otherwise, storing into VAR must be handled specially
	   by storing into a temporary and copying that into VAR
	   with a new insn after this one.  Note that this case
	   will be used when storing into a promoted scalar since
	   the insn will now have different modes on the input
	   and output and hence will be invalid (except for the case
	   of setting it to a constant, which does not need any
	   change if it is valid).  We generate extra code in that case,
	   but combine.c will eliminate it.  */

	if (dest == var)
	  {
	    rtx temp;
	    rtx fixeddest = SET_DEST (x);
	    enum machine_mode temp_mode;

	    /* STRICT_LOW_PART can be discarded, around a MEM.  */
	    if (GET_CODE (fixeddest) == STRICT_LOW_PART)
	      fixeddest = XEXP (fixeddest, 0);
	    /* Convert (SUBREG (MEM)) to a MEM in a changed mode.  */
	    if (GET_CODE (fixeddest) == SUBREG)
	      {
		fixeddest = fixup_memory_subreg (fixeddest, insn,
						 promoted_mode, 0);
		temp_mode = GET_MODE (fixeddest);
	      }
	    else
	      {
		fixeddest = fixup_stack_1 (fixeddest, insn);
		temp_mode = promoted_mode;
	      }

	    temp = gen_reg_rtx (temp_mode);

	    emit_insn_after (gen_move_insn (fixeddest,
					    gen_lowpart (GET_MODE (fixeddest),
							 temp)),
			     insn);

	    SET_DEST (x) = temp;
	  }
      }

    default:
      break;
    }

  /* Nothing special about this RTX; fix its operands.  */

  fmt = GET_RTX_FORMAT (code);
  for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
    {
      if (fmt[i] == 'e')
	fixup_var_refs_1 (var, promoted_mode, &XEXP (x, i), insn, replacements,
			  no_share);
      else if (fmt[i] == 'E')
	{
	  int j;
	  for (j = 0; j < XVECLEN (x, i); j++)
	    fixup_var_refs_1 (var, promoted_mode, &XVECEXP (x, i, j),
			      insn, replacements, no_share);
	}
    }
}

/* Previously, X had the form (SUBREG:m1 (REG:PROMOTED_MODE ...)).
   The REG  was placed on the stack, so X now has the form (SUBREG:m1
   (MEM:m2 ...)).

   Return an rtx (MEM:m1 newaddr) which is equivalent.  If any insns
   must be emitted to compute NEWADDR, put them before INSN.

   UNCRITICAL nonzero means accept paradoxical subregs.
   This is used for subregs found inside REG_NOTES.  */

static rtx
fixup_memory_subreg (rtx x, rtx insn, enum machine_mode promoted_mode, int uncritical)
{
  int offset;
  rtx mem = SUBREG_REG (x);
  rtx addr = XEXP (mem, 0);
  enum machine_mode mode = GET_MODE (x);
  rtx result, seq;

  /* Paradoxical SUBREGs are usually invalid during RTL generation.  */
  if (GET_MODE_SIZE (mode) > GET_MODE_SIZE (GET_MODE (mem)) && ! uncritical)
    abort ();

  offset = SUBREG_BYTE (x);
  if (BYTES_BIG_ENDIAN)
    /* If the PROMOTED_MODE is wider than the mode of the MEM, adjust
       the offset so that it points to the right location within the
       MEM.  */
    offset -= (GET_MODE_SIZE (promoted_mode) - GET_MODE_SIZE (GET_MODE (mem)));

  if (!flag_force_addr
      && memory_address_p (mode, plus_constant (addr, offset)))
    /* Shortcut if no insns need be emitted.  */
    return adjust_address (mem, mode, offset);

  start_sequence ();
  result = adjust_address (mem, mode, offset);
  seq = get_insns ();
  end_sequence ();

  emit_insn_before (seq, insn);
  return result;
}

/* Do fixup_memory_subreg on all (SUBREG (MEM ...) ...) contained in X.
   Replace subexpressions of X in place.
   If X itself is a (SUBREG (MEM ...) ...), return the replacement expression.
   Otherwise return X, with its contents possibly altered.

   INSN, PROMOTED_MODE and UNCRITICAL are as for
   fixup_memory_subreg.  */

static rtx
walk_fixup_memory_subreg (rtx x, rtx insn, enum machine_mode promoted_mode,
			  int uncritical)
{
  enum rtx_code code;
  const char *fmt;
  int i;

  if (x == 0)
    return 0;

  code = GET_CODE (x);

  if (code == SUBREG && GET_CODE (SUBREG_REG (x)) == MEM)
    return fixup_memory_subreg (x, insn, promoted_mode, uncritical);

  /* Nothing special about this RTX; fix its operands.  */

  fmt = GET_RTX_FORMAT (code);
  for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
    {
      if (fmt[i] == 'e')
	XEXP (x, i) = walk_fixup_memory_subreg (XEXP (x, i), insn,
						promoted_mode, uncritical);
      else if (fmt[i] == 'E')
	{
	  int j;
	  for (j = 0; j < XVECLEN (x, i); j++)
	    XVECEXP (x, i, j)
	      = walk_fixup_memory_subreg (XVECEXP (x, i, j), insn,
					  promoted_mode, uncritical);
	}
    }
  return x;
}

/* For each memory ref within X, if it refers to a stack slot
   with an out of range displacement, put the address in a temp register
   (emitting new insns before INSN to load these registers)
   and alter the memory ref to use that register.
   Replace each such MEM rtx with a copy, to avoid clobberage.  */

static rtx
fixup_stack_1 (rtx x, rtx insn)
{
  int i;
  RTX_CODE code = GET_CODE (x);
  const char *fmt;

  if (code == MEM)
    {
      rtx ad = XEXP (x, 0);
      /* If we have address of a stack slot but it's not valid
	 (displacement is too large), compute the sum in a register.  */
      if (GET_CODE (ad) == PLUS
	  && GET_CODE (XEXP (ad, 0)) == REG
	  && ((REGNO (XEXP (ad, 0)) >= FIRST_VIRTUAL_REGISTER
	       && REGNO (XEXP (ad, 0)) <= LAST_VIRTUAL_REGISTER)
	      || REGNO (XEXP (ad, 0)) == FRAME_POINTER_REGNUM
#if HARD_FRAME_POINTER_REGNUM != FRAME_POINTER_REGNUM
	      || REGNO (XEXP (ad, 0)) == HARD_FRAME_POINTER_REGNUM
#endif
	      || REGNO (XEXP (ad, 0)) == STACK_POINTER_REGNUM
	      || REGNO (XEXP (ad, 0)) == ARG_POINTER_REGNUM
	      || XEXP (ad, 0) == current_function_internal_arg_pointer)
	  && GET_CODE (XEXP (ad, 1)) == CONST_INT)
	{
	  rtx temp, seq;
	  if (memory_address_p (GET_MODE (x), ad))
	    return x;

	  start_sequence ();
	  temp = copy_to_reg (ad);
	  seq = get_insns ();
	  end_sequence ();
	  emit_insn_before (seq, insn);
	  return replace_equiv_address (x, temp);
	}
      return x;
    }

  fmt = GET_RTX_FORMAT (code);
  for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
    {
      if (fmt[i] == 'e')
	XEXP (x, i) = fixup_stack_1 (XEXP (x, i), insn);
      else if (fmt[i] == 'E')
	{
	  int j;
	  for (j = 0; j < XVECLEN (x, i); j++)
	    XVECEXP (x, i, j) = fixup_stack_1 (XVECEXP (x, i, j), insn);
	}
    }
  return x;
}

/* Optimization: a bit-field instruction whose field
   happens to be a byte or halfword in memory
   can be changed to a move instruction.

   We call here when INSN is an insn to examine or store into a bit-field.
   BODY is the SET-rtx to be altered.

   EQUIV_MEM is the table `reg_equiv_mem' if that is available; else 0.
   (Currently this is called only from function.c, and EQUIV_MEM
   is always 0.)  */

static void
optimize_bit_field (rtx body, rtx insn, rtx *equiv_mem)
{
  rtx bitfield;
  int destflag;
  rtx seq = 0;
  enum machine_mode mode;

  if (GET_CODE (SET_DEST (body)) == SIGN_EXTRACT
      || GET_CODE (SET_DEST (body)) == ZERO_EXTRACT)
    bitfield = SET_DEST (body), destflag = 1;
  else
    bitfield = SET_SRC (body), destflag = 0;

  /* First check that the field being stored has constant size and position
     and is in fact a byte or halfword suitably aligned.  */

  if (GET_CODE (XEXP (bitfield, 1)) == CONST_INT
      && GET_CODE (XEXP (bitfield, 2)) == CONST_INT
      && ((mode = mode_for_size (INTVAL (XEXP (bitfield, 1)), MODE_INT, 1))
	  != BLKmode)
      && INTVAL (XEXP (bitfield, 2)) % INTVAL (XEXP (bitfield, 1)) == 0)
    {
      rtx memref = 0;

      /* Now check that the containing word is memory, not a register,
	 and that it is safe to change the machine mode.  */

      if (GET_CODE (XEXP (bitfield, 0)) == MEM)
	memref = XEXP (bitfield, 0);
      else if (GET_CODE (XEXP (bitfield, 0)) == REG
	       && equiv_mem != 0)
	memref = equiv_mem[REGNO (XEXP (bitfield, 0))];
      else if (GET_CODE (XEXP (bitfield, 0)) == SUBREG
	       && GET_CODE (SUBREG_REG (XEXP (bitfield, 0))) == MEM)
	memref = SUBREG_REG (XEXP (bitfield, 0));
      else if (GET_CODE (XEXP (bitfield, 0)) == SUBREG
	       && equiv_mem != 0
	       && GET_CODE (SUBREG_REG (XEXP (bitfield, 0))) == REG)
	memref = equiv_mem[REGNO (SUBREG_REG (XEXP (bitfield, 0)))];

      if (memref
	  && ! mode_dependent_address_p (XEXP (memref, 0))
	  && ! MEM_VOLATILE_P (memref))
	{
	  /* Now adjust the address, first for any subreg'ing
	     that we are now getting rid of,
	     and then for which byte of the word is wanted.  */

	  HOST_WIDE_INT offset = INTVAL (XEXP (bitfield, 2));
	  rtx insns;

	  /* Adjust OFFSET to count bits from low-address byte.  */
	  if (BITS_BIG_ENDIAN != BYTES_BIG_ENDIAN)
	    offset = (GET_MODE_BITSIZE (GET_MODE (XEXP (bitfield, 0)))
		      - offset - INTVAL (XEXP (bitfield, 1)));

	  /* Adjust OFFSET to count bytes from low-address byte.  */
	  offset /= BITS_PER_UNIT;
	  if (GET_CODE (XEXP (bitfield, 0)) == SUBREG)
	    {
	      offset += (SUBREG_BYTE (XEXP (bitfield, 0))
			 / UNITS_PER_WORD) * UNITS_PER_WORD;
	      if (BYTES_BIG_ENDIAN)
		offset -= (MIN (UNITS_PER_WORD,
				GET_MODE_SIZE (GET_MODE (XEXP (bitfield, 0))))
			   - MIN (UNITS_PER_WORD,
				  GET_MODE_SIZE (GET_MODE (memref))));
	    }

	  start_sequence ();
	  memref = adjust_address (memref, mode, offset);
	  insns = get_insns ();
	  end_sequence ();
	  emit_insn_before (insns, insn);

	  /* Store this memory reference where
	     we found the bit field reference.  */

	  if (destflag)
	    {
	      validate_change (insn, &SET_DEST (body), memref, 1);
	      if (! CONSTANT_ADDRESS_P (SET_SRC (body)))
		{
		  rtx src = SET_SRC (body);
		  while (GET_CODE (src) == SUBREG
			 && SUBREG_BYTE (src) == 0)
		    src = SUBREG_REG (src);
		  if (GET_MODE (src) != GET_MODE (memref))
		    src = gen_lowpart (GET_MODE (memref), SET_SRC (body));
		  validate_change (insn, &SET_SRC (body), src, 1);
		}
	      else if (GET_MODE (SET_SRC (body)) != VOIDmode
		       && GET_MODE (SET_SRC (body)) != GET_MODE (memref))
		/* This shouldn't happen because anything that didn't have
		   one of these modes should have got converted explicitly
		   and then referenced through a subreg.
		   This is so because the original bit-field was
		   handled by agg_mode and so its tree structure had
		   the same mode that memref now has.  */
		abort ();
	    }
	  else
	    {
	      rtx dest = SET_DEST (body);

	      while (GET_CODE (dest) == SUBREG
		     && SUBREG_BYTE (dest) == 0
		     && (GET_MODE_CLASS (GET_MODE (dest))
			 == GET_MODE_CLASS (GET_MODE (SUBREG_REG (dest))))
		     && (GET_MODE_SIZE (GET_MODE (SUBREG_REG (dest)))
			 <= UNITS_PER_WORD))
		dest = SUBREG_REG (dest);

	      validate_change (insn, &SET_DEST (body), dest, 1);

	      if (GET_MODE (dest) == GET_MODE (memref))
		validate_change (insn, &SET_SRC (body), memref, 1);
	      else
		{
		  /* Convert the mem ref to the destination mode.  */
		  rtx newreg = gen_reg_rtx (GET_MODE (dest));

		  start_sequence ();
		  convert_move (newreg, memref,
				GET_CODE (SET_SRC (body)) == ZERO_EXTRACT);
		  seq = get_insns ();
		  end_sequence ();

		  validate_change (insn, &SET_SRC (body), newreg, 1);
		}
	    }

	  /* See if we can convert this extraction or insertion into
	     a simple move insn.  We might not be able to do so if this
	     was, for example, part of a PARALLEL.

	     If we succeed, write out any needed conversions.  If we fail,
	     it is hard to guess why we failed, so don't do anything
	     special; just let the optimization be suppressed.  */

	  if (apply_change_group () && seq)
	    emit_insn_before (seq, insn);
	}
    }
}

/* These routines are responsible for converting virtual register references
   to the actual hard register references once RTL generation is complete.

   The following four variables are used for communication between the
   routines.  They contain the offsets of the virtual registers from their
   respective hard registers.  */

static int in_arg_offset;
static int var_offset;
static int dynamic_offset;
static int out_arg_offset;
static int cfa_offset;

/* In most machines, the stack pointer register is equivalent to the bottom
   of the stack.  */

#ifndef STACK_POINTER_OFFSET
#define STACK_POINTER_OFFSET	0
#endif

/* If not defined, pick an appropriate default for the offset of dynamically
   allocated memory depending on the value of ACCUMULATE_OUTGOING_ARGS,
   REG_PARM_STACK_SPACE, and OUTGOING_REG_PARM_STACK_SPACE.  */

#ifndef STACK_DYNAMIC_OFFSET

/* The bottom of the stack points to the actual arguments.  If
   REG_PARM_STACK_SPACE is defined, this includes the space for the register
   parameters.  However, if OUTGOING_REG_PARM_STACK space is not defined,
   stack space for register parameters is not pushed by the caller, but
   rather part of the fixed stack areas and hence not included in
   `current_function_outgoing_args_size'.  Nevertheless, we must allow
   for it when allocating stack dynamic objects.  */

#if defined(REG_PARM_STACK_SPACE) && ! defined(OUTGOING_REG_PARM_STACK_SPACE)
#define STACK_DYNAMIC_OFFSET(FNDECL)	\
((ACCUMULATE_OUTGOING_ARGS						      \
  ? (current_function_outgoing_args_size + REG_PARM_STACK_SPACE (FNDECL)) : 0)\
 + (STACK_POINTER_OFFSET))						      \

#else
#define STACK_DYNAMIC_OFFSET(FNDECL)	\
((ACCUMULATE_OUTGOING_ARGS ? current_function_outgoing_args_size : 0)	      \
 + (STACK_POINTER_OFFSET))
#endif
#endif

/* On most machines, the CFA coincides with the first incoming parm.  */

#ifndef ARG_POINTER_CFA_OFFSET
#define ARG_POINTER_CFA_OFFSET(FNDECL) FIRST_PARM_OFFSET (FNDECL)
#endif

/* Build up a (MEM (ADDRESSOF (REG))) rtx for a register REG that just
   had its address taken.  DECL is the decl or SAVE_EXPR for the
   object stored in the register, for later use if we do need to force
   REG into the stack.  REG is overwritten by the MEM like in
   put_reg_into_stack.  RESCAN is true if previously emitted
   instructions must be rescanned and modified now that the REG has
   been transformed.  */

rtx
gen_mem_addressof (rtx reg, tree decl, int rescan)
{
  rtx r = gen_rtx_ADDRESSOF (Pmode, gen_reg_rtx (GET_MODE (reg)),
			     REGNO (reg), decl);

  /* Calculate this before we start messing with decl's RTL.  */
  HOST_WIDE_INT set = decl ? get_alias_set (decl) : 0;

  /* If the original REG was a user-variable, then so is the REG whose
     address is being taken.  Likewise for unchanging.  */
  REG_USERVAR_P (XEXP (r, 0)) = REG_USERVAR_P (reg);
  RTX_UNCHANGING_P (XEXP (r, 0)) = RTX_UNCHANGING_P (reg);

  PUT_CODE (reg, MEM);
  MEM_ATTRS (reg) = 0;
  XEXP (reg, 0) = r;

  if (decl)
    {
      tree type = TREE_TYPE (decl);
      enum machine_mode decl_mode
	= (DECL_P (decl) ? DECL_MODE (decl) : TYPE_MODE (TREE_TYPE (decl)));
      rtx decl_rtl = (TREE_CODE (decl) == SAVE_EXPR ? SAVE_EXPR_RTL (decl)
		      : DECL_RTL_IF_SET (decl));

      PUT_MODE (reg, decl_mode);

      /* Clear DECL_RTL momentarily so functions below will work
	 properly, then set it again.  */
      if (DECL_P (decl) && decl_rtl == reg)
	SET_DECL_RTL (decl, 0);

      set_mem_attributes (reg, decl, 1);
      set_mem_alias_set (reg, set);

      if (DECL_P (decl) && decl_rtl == reg)
	SET_DECL_RTL (decl, reg);

      if (rescan
	  && (TREE_USED (decl) || (DECL_P (decl) && DECL_INITIAL (decl) != 0)))
	fixup_var_refs (reg, GET_MODE (reg), TREE_UNSIGNED (type), reg, 0);
    }
  else if (rescan)
    {
      /* This can only happen during reload.  Clear the same flag bits as
	 reload.  */
      MEM_VOLATILE_P (reg) = 0;
      RTX_UNCHANGING_P (reg) = 0;
      MEM_IN_STRUCT_P (reg) = 0;
      MEM_SCALAR_P (reg) = 0;
      MEM_ATTRS (reg) = 0;

      fixup_var_refs (reg, GET_MODE (reg), 0, reg, 0);
    }

  return reg;
}

/* If DECL has an RTL that is an ADDRESSOF rtx, put it into the stack.  */

void
flush_addressof (tree decl)
{
  if ((TREE_CODE (decl) == PARM_DECL || TREE_CODE (decl) == VAR_DECL)
      && DECL_RTL (decl) != 0
      && GET_CODE (DECL_RTL (decl)) == MEM
      && GET_CODE (XEXP (DECL_RTL (decl), 0)) == ADDRESSOF
      && GET_CODE (XEXP (XEXP (DECL_RTL (decl), 0), 0)) == REG)
    put_addressof_into_stack (XEXP (DECL_RTL (decl), 0), 0);
}

/* Force the register pointed to by R, an ADDRESSOF rtx, into the stack.  */

static void
put_addressof_into_stack (rtx r, htab_t ht)
{
  tree decl, type;
  bool volatile_p, used_p;

  rtx reg = XEXP (r, 0);

  if (GET_CODE (reg) != REG)
    abort ();

  decl = ADDRESSOF_DECL (r);
  if (decl)
    {
      type = TREE_TYPE (decl);
      volatile_p = (TREE_CODE (decl) != SAVE_EXPR
		    && TREE_THIS_VOLATILE (decl));
      used_p = (TREE_USED (decl)
		|| (DECL_P (decl) && DECL_INITIAL (decl) != 0));
    }
  else
    {
      type = NULL_TREE;
      volatile_p = false;
      used_p = true;
    }

  put_reg_into_stack (0, reg, type, GET_MODE (reg), ADDRESSOF_REGNO (r),
		      volatile_p, used_p, false, ht);
}

/* List of replacements made below in purge_addressof_1 when creating
   bitfield insertions.  */
static rtx purge_bitfield_addressof_replacements;

/* List of replacements made below in purge_addressof_1 for patterns
   (MEM (ADDRESSOF (REG ...))).  The key of the list entry is the
   corresponding (ADDRESSOF (REG ...)) and value is a substitution for
   the all pattern.  List PURGE_BITFIELD_ADDRESSOF_REPLACEMENTS is not
   enough in complex cases, e.g. when some field values can be
   extracted by usage MEM with narrower mode.  */
static rtx purge_addressof_replacements;

/* Helper function for purge_addressof.  See if the rtx expression at *LOC
   in INSN needs to be changed.  If FORCE, always put any ADDRESSOFs into
   the stack.  If the function returns FALSE then the replacement could not
   be made.  If MAY_POSTPONE is true and we would not put the addressof
   to stack, postpone processing of the insn.  */

static bool
purge_addressof_1 (rtx *loc, rtx insn, int force, int store, int may_postpone,
		   htab_t ht)
{
  rtx x;
  RTX_CODE code;
  int i, j;
  const char *fmt;
  bool result = true;
  bool libcall = false;

  /* Re-start here to avoid recursion in common cases.  */
 restart:

  x = *loc;
  if (x == 0)
    return true;

  /* Is this a libcall?  */
  if (!insn)
    libcall = REG_NOTE_KIND (*loc) == REG_RETVAL;

  code = GET_CODE (x);

  /* If we don't return in any of the cases below, we will recurse inside
     the RTX, which will normally result in any ADDRESSOF being forced into
     memory.  */
  if (code == SET)
    {
      result = purge_addressof_1 (&SET_DEST (x), insn, force, 1,
				  may_postpone, ht);
      result &= purge_addressof_1 (&SET_SRC (x), insn, force, 0,
				   may_postpone, ht);
      return result;
    }
  else if (code == ADDRESSOF)
    {
      rtx sub, insns;

      if (GET_CODE (XEXP (x, 0)) != MEM)
	put_addressof_into_stack (x, ht);

      /* We must create a copy of the rtx because it was created by
	 overwriting a REG rtx which is always shared.  */
      sub = copy_rtx (XEXP (XEXP (x, 0), 0));
      if (validate_change (insn, loc, sub, 0)
	  || validate_replace_rtx (x, sub, insn))
	return true;

      start_sequence ();

      /* If SUB is a hard or virtual register, try it as a pseudo-register.
	 Otherwise, perhaps SUB is an expression, so generate code to compute
	 it.  */
      if (GET_CODE (sub) == REG && REGNO (sub) <= LAST_VIRTUAL_REGISTER)
	sub = copy_to_reg (sub);
      else
	sub = force_operand (sub, NULL_RTX);

      if (! validate_change (insn, loc, sub, 0)
	  && ! validate_replace_rtx (x, sub, insn))
	abort ();

      insns = get_insns ();
      end_sequence ();
      emit_insn_before (insns, insn);
      return true;
    }

  else if (code == MEM && GET_CODE (XEXP (x, 0)) == ADDRESSOF && ! force)
    {
      rtx sub = XEXP (XEXP (x, 0), 0);

      if (GET_CODE (sub) == MEM)
	sub = adjust_address_nv (sub, GET_MODE (x), 0);
      else if (GET_CODE (sub) == REG
	       && (MEM_VOLATILE_P (x) || GET_MODE (x) == BLKmode))
	;
      else if (GET_CODE (sub) == REG && GET_MODE (x) != GET_MODE (sub))
	{
	  int size_x, size_sub;

	  if (may_postpone)
	    {
	      /* Postpone for now, so that we do not emit bitfield arithmetics
		 unless there is some benefit from it.  */
	      if (!postponed_insns || XEXP (postponed_insns, 0) != insn)
		postponed_insns = alloc_INSN_LIST (insn, postponed_insns);
	      return true;
	    }

	  if (!insn)
	    {
	      /* When processing REG_NOTES look at the list of
		 replacements done on the insn to find the register that X
		 was replaced by.  */
	      rtx tem;

	      for (tem = purge_bitfield_addressof_replacements;
		   tem != NULL_RTX;
		   tem = XEXP (XEXP (tem, 1), 1))
		if (rtx_equal_p (x, XEXP (tem, 0)))
		  {
		    *loc = XEXP (XEXP (tem, 1), 0);
		    return true;
		  }

	      /* See comment for purge_addressof_replacements.  */
	      for (tem = purge_addressof_replacements;
		   tem != NULL_RTX;
		   tem = XEXP (XEXP (tem, 1), 1))
		if (rtx_equal_p (XEXP (x, 0), XEXP (tem, 0)))
		  {
		    rtx z = XEXP (XEXP (tem, 1), 0);

		    if (GET_MODE (x) == GET_MODE (z)
			|| (GET_CODE (XEXP (XEXP (tem, 1), 0)) != REG
			    && GET_CODE (XEXP (XEXP (tem, 1), 0)) != SUBREG))
		      abort ();

		    /* It can happen that the note may speak of things
		       in a wider (or just different) mode than the
		       code did.  This is especially true of
		       REG_RETVAL.  */

		    if (GET_CODE (z) == SUBREG && SUBREG_BYTE (z) == 0)
		      z = SUBREG_REG (z);

		    if (GET_MODE_SIZE (GET_MODE (x)) > UNITS_PER_WORD
			&& (GET_MODE_SIZE (GET_MODE (x))
			    > GET_MODE_SIZE (GET_MODE (z))))
		      {
			/* This can occur as a result in invalid
			   pointer casts, e.g. float f; ...
			   *(long long int *)&f.
			   ??? We could emit a warning here, but
			   without a line number that wouldn't be
			   very helpful.  */
			z = gen_rtx_SUBREG (GET_MODE (x), z, 0);
		      }
		    else
		      z = gen_lowpart (GET_MODE (x), z);

		    *loc = z;
		    return true;
		  }

	      /* When we are processing the REG_NOTES of the last instruction
		 of a libcall, there will be typically no replacements
		 for that insn; the replacements happened before, piecemeal
		 fashion.  OTOH we are not interested in the details of
		 this for the REG_EQUAL note, we want to know the big picture,
		 which can be succinctly described with a simple SUBREG.
		 Note that removing the REG_EQUAL note is not an option
		 on the last insn of a libcall, so we must do a replacement.  */

	      /* In compile/990107-1.c:7 compiled at -O1 -m1 for sh-elf,
		 we got
		 (mem:DI (addressof:SI (reg/v:DF 160) 159 0x401c8510)
		 [0 S8 A32]), which can be expressed with a simple
		 same-size subreg  */
	      if ((GET_MODE_SIZE (GET_MODE (x))
		   <= GET_MODE_SIZE (GET_MODE (sub)))
		  /* Again, invalid pointer casts (as in
		     compile/990203-1.c) can require paradoxical
		     subregs.  */
		  || (GET_MODE_SIZE (GET_MODE (x)) > UNITS_PER_WORD
		      && (GET_MODE_SIZE (GET_MODE (x))
			  > GET_MODE_SIZE (GET_MODE (sub)))
		      && libcall))
		{
		  *loc = gen_rtx_SUBREG (GET_MODE (x), sub, 0);
		  return true;
		}
	      /* ??? Are there other cases we should handle?  */

	      /* Sometimes we may not be able to find the replacement.  For
		 example when the original insn was a MEM in a wider mode,
		 and the note is part of a sign extension of a narrowed
		 version of that MEM.  Gcc testcase compile/990829-1.c can
		 generate an example of this situation.  Rather than complain
		 we return false, which will prompt our caller to remove the
		 offending note.  */
	      return false;
	    }

	  size_x = GET_MODE_BITSIZE (GET_MODE (x));
	  size_sub = GET_MODE_BITSIZE (GET_MODE (sub));

	  /* Do not frob unchanging MEMs.  If a later reference forces the
	     pseudo to the stack, we can wind up with multiple writes to
	     an unchanging memory, which is invalid.  */
	  if (RTX_UNCHANGING_P (x) && size_x != size_sub)
	    ;

	  /* Don't even consider working with paradoxical subregs,
	     or the moral equivalent seen here.  */
	  else if (size_x <= size_sub
	           && int_mode_for_mode (GET_MODE (sub)) != BLKmode)
	    {
	      /* Do a bitfield insertion to mirror what would happen
		 in memory.  */

	      rtx val, seq;

	      if (store)
		{
		  rtx p = PREV_INSN (insn);

		  start_sequence ();
		  val = gen_reg_rtx (GET_MODE (x));
		  if (! validate_change (insn, loc, val, 0))
		    {
		      /* Discard the current sequence and put the
			 ADDRESSOF on stack.  */
		      end_sequence ();
		      goto give_up;
		    }
		  seq = get_insns ();
		  end_sequence ();
		  emit_insn_before (seq, insn);
		  compute_insns_for_mem (p ? NEXT_INSN (p) : get_insns (),
					 insn, ht);

		  start_sequence ();
		  store_bit_field (sub, size_x, 0, GET_MODE (x),
				   val, GET_MODE_SIZE (GET_MODE (sub)));

		  /* Make sure to unshare any shared rtl that store_bit_field
		     might have created.  */
		  unshare_all_rtl_again (get_insns ());

		  seq = get_insns ();
		  end_sequence ();
		  p = emit_insn_after (seq, insn);
		  if (NEXT_INSN (insn))
		    compute_insns_for_mem (NEXT_INSN (insn),
					   p ? NEXT_INSN (p) : NULL_RTX,
					   ht);
		}
	      else
		{
		  rtx p = PREV_INSN (insn);

		  start_sequence ();
		  val = extract_bit_field (sub, size_x, 0, 1, NULL_RTX,
					   GET_MODE (x), GET_MODE (x),
					   GET_MODE_SIZE (GET_MODE (sub)));

		  if (! validate_change (insn, loc, val, 0))
		    {
		      /* Discard the current sequence and put the
			 ADDRESSOF on stack.  */
		      end_sequence ();
		      goto give_up;
		    }

		  seq = get_insns ();
		  end_sequence ();
		  emit_insn_before (seq, insn);
		  compute_insns_for_mem (p ? NEXT_INSN (p) : get_insns (),
					 insn, ht);
		}

	      /* Remember the replacement so that the same one can be done
		 on the REG_NOTES.  */
	      purge_bitfield_addressof_replacements
		= gen_rtx_EXPR_LIST (VOIDmode, x,
				     gen_rtx_EXPR_LIST
				     (VOIDmode, val,
				      purge_bitfield_addressof_replacements));

	      /* We replaced with a reg -- all done.  */
	      return true;
	    }
	}

      else if (validate_change (insn, loc, sub, 0))
	{
	  /* Remember the replacement so that the same one can be done
	     on the REG_NOTES.  */
	  if (GET_CODE (sub) == REG || GET_CODE (sub) == SUBREG)
	    {
	      rtx tem;

	      for (tem = purge_addressof_replacements;
		   tem != NULL_RTX;
		   tem = XEXP (XEXP (tem, 1), 1))
		if (rtx_equal_p (XEXP (x, 0), XEXP (tem, 0)))
		  {
		    XEXP (XEXP (tem, 1), 0) = sub;
		    return true;
		  }
	      purge_addressof_replacements
		= gen_rtx (EXPR_LIST, VOIDmode, XEXP (x, 0),
			   gen_rtx_EXPR_LIST (VOIDmode, sub,
					      purge_addressof_replacements));
	      return true;
	    }
	  goto restart;
	}
    }

 give_up:
  /* Scan all subexpressions.  */
  fmt = GET_RTX_FORMAT (code);
  for (i = 0; i < GET_RTX_LENGTH (code); i++, fmt++)
    {
      if (*fmt == 'e')
	result &= purge_addressof_1 (&XEXP (x, i), insn, force, 0,
				     may_postpone, ht);
      else if (*fmt == 'E')
	for (j = 0; j < XVECLEN (x, i); j++)
	  result &= purge_addressof_1 (&XVECEXP (x, i, j), insn, force, 0,
				       may_postpone, ht);
    }

  return result;
}

/* Return a hash value for K, a REG.  */

static hashval_t
insns_for_mem_hash (const void *k)
{
  /* Use the address of the key for the hash value.  */
  struct insns_for_mem_entry *m = (struct insns_for_mem_entry *) k;
  return htab_hash_pointer (m->key);
}

/* Return nonzero if K1 and K2 (two REGs) are the same.  */

static int
insns_for_mem_comp (const void *k1, const void *k2)
{
  struct insns_for_mem_entry *m1 = (struct insns_for_mem_entry *) k1;
  struct insns_for_mem_entry *m2 = (struct insns_for_mem_entry *) k2;
  return m1->key == m2->key;
}

struct insns_for_mem_walk_info
{
  /* The hash table that we are using to record which INSNs use which
     MEMs.  */
  htab_t ht;

  /* The INSN we are currently processing.  */
  rtx insn;

  /* Zero if we are walking to find ADDRESSOFs, one if we are walking
     to find the insns that use the REGs in the ADDRESSOFs.  */
  int pass;
};

/* Called from compute_insns_for_mem via for_each_rtx.  If R is a REG
   that might be used in an ADDRESSOF expression, record this INSN in
   the hash table given by DATA (which is really a pointer to an
   insns_for_mem_walk_info structure).  */

static int
insns_for_mem_walk (rtx *r, void *data)
{
  struct insns_for_mem_walk_info *ifmwi
    = (struct insns_for_mem_walk_info *) data;
  struct insns_for_mem_entry tmp;
  tmp.insns = NULL_RTX;

  if (ifmwi->pass == 0 && *r && GET_CODE (*r) == ADDRESSOF
      && GET_CODE (XEXP (*r, 0)) == REG)
    {
      void **e;
      tmp.key = XEXP (*r, 0);
      e = htab_find_slot (ifmwi->ht, &tmp, INSERT);
      if (*e == NULL)
	{
	  *e = ggc_alloc (sizeof (tmp));
	  memcpy (*e, &tmp, sizeof (tmp));
	}
    }
  else if (ifmwi->pass == 1 && *r && GET_CODE (*r) == REG)
    {
      struct insns_for_mem_entry *ifme;
      tmp.key = *r;
      ifme = htab_find (ifmwi->ht, &tmp);

      /* If we have not already recorded this INSN, do so now.  Since
	 we process the INSNs in order, we know that if we have
	 recorded it it must be at the front of the list.  */
      if (ifme && (!ifme->insns || XEXP (ifme->insns, 0) != ifmwi->insn))
	ifme->insns = gen_rtx_EXPR_LIST (VOIDmode, ifmwi->insn,
					 ifme->insns);
    }

  return 0;
}

/* Walk the INSNS, until we reach LAST_INSN, recording which INSNs use
   which REGs in HT.  */

static void
compute_insns_for_mem (rtx insns, rtx last_insn, htab_t ht)
{
  rtx insn;
  struct insns_for_mem_walk_info ifmwi;
  ifmwi.ht = ht;

  for (ifmwi.pass = 0; ifmwi.pass < 2; ++ifmwi.pass)
    for (insn = insns; insn != last_insn; insn = NEXT_INSN (insn))
      if (INSN_P (insn))
	{
	  ifmwi.insn = insn;
	  for_each_rtx (&insn, insns_for_mem_walk, &ifmwi);
	}
}

/* Helper function for purge_addressof called through for_each_rtx.
   Returns true iff the rtl is an ADDRESSOF.  */

static int
is_addressof (rtx *rtl, void *data ATTRIBUTE_UNUSED)
{
  return GET_CODE (*rtl) == ADDRESSOF;
}

/* Eliminate all occurrences of ADDRESSOF from INSNS.  Elide any remaining
   (MEM (ADDRESSOF)) patterns, and force any needed registers into the
   stack.  */

void
purge_addressof (rtx insns)
{
  rtx insn, tmp;
  htab_t ht;

  /* When we actually purge ADDRESSOFs, we turn REGs into MEMs.  That
     requires a fixup pass over the instruction stream to correct
     INSNs that depended on the REG being a REG, and not a MEM.  But,
     these fixup passes are slow.  Furthermore, most MEMs are not
     mentioned in very many instructions.  So, we speed up the process
     by pre-calculating which REGs occur in which INSNs; that allows
     us to perform the fixup passes much more quickly.  */
  ht = htab_create_ggc (1000, insns_for_mem_hash, insns_for_mem_comp, NULL);
  compute_insns_for_mem (insns, NULL_RTX, ht);

  postponed_insns = NULL;

  for (insn = insns; insn; insn = NEXT_INSN (insn))
    if (INSN_P (insn))
      {
	if (! purge_addressof_1 (&PATTERN (insn), insn,
				 asm_noperands (PATTERN (insn)) > 0, 0, 1, ht))
	  /* If we could not replace the ADDRESSOFs in the insn,
	     something is wrong.  */
	  abort ();

	if (! purge_addressof_1 (&REG_NOTES (insn), NULL_RTX, 0, 0, 0, ht))
	  {
	    /* If we could not replace the ADDRESSOFs in the insn's notes,
	       we can just remove the offending notes instead.  */
	    rtx note;

	    for (note = REG_NOTES (insn); note; note = XEXP (note, 1))
	      {
		/* If we find a REG_RETVAL note then the insn is a libcall.
		   Such insns must have REG_EQUAL notes as well, in order
		   for later passes of the compiler to work.  So it is not
		   safe to delete the notes here, and instead we abort.  */
		if (REG_NOTE_KIND (note) == REG_RETVAL)
		  abort ();
		if (for_each_rtx (&note, is_addressof, NULL))
		  remove_note (insn, note);
	      }
	  }
      }

  /* Process the postponed insns.  */
  while (postponed_insns)
    {
      insn = XEXP (postponed_insns, 0);
      tmp = postponed_insns;
      postponed_insns = XEXP (postponed_insns, 1);
      free_INSN_LIST_node (tmp);

      if (! purge_addressof_1 (&PATTERN (insn), insn,
			       asm_noperands (PATTERN (insn)) > 0, 0, 0, ht))
	abort ();
    }

  /* Clean up.  */
  purge_bitfield_addressof_replacements = 0;
  purge_addressof_replacements = 0;

  /* REGs are shared.  purge_addressof will destructively replace a REG
     with a MEM, which creates shared MEMs.

     Unfortunately, the children of put_reg_into_stack assume that MEMs
     referring to the same stack slot are shared (fixup_var_refs and
     the associated hash table code).

     So, we have to do another unsharing pass after we have flushed any
     REGs that had their address taken into the stack.

     It may be worth tracking whether or not we converted any REGs into
     MEMs to avoid this overhead when it is not needed.  */
  unshare_all_rtl_again (get_insns ());
}

/* Convert a SET of a hard subreg to a set of the appropriate hard
   register.  A subroutine of purge_hard_subreg_sets.  */

static void
purge_single_hard_subreg_set (rtx pattern)
{
  rtx reg = SET_DEST (pattern);
  enum machine_mode mode = GET_MODE (SET_DEST (pattern));
  int offset = 0;

  if (GET_CODE (reg) == SUBREG && GET_CODE (SUBREG_REG (reg)) == REG
      && REGNO (SUBREG_REG (reg)) < FIRST_PSEUDO_REGISTER)
    {
      offset = subreg_regno_offset (REGNO (SUBREG_REG (reg)),
				    GET_MODE (SUBREG_REG (reg)),
				    SUBREG_BYTE (reg),
				    GET_MODE (reg));
      reg = SUBREG_REG (reg);
    }


  if (GET_CODE (reg) == REG && REGNO (reg) < FIRST_PSEUDO_REGISTER)
    {
      reg = gen_rtx_REG (mode, REGNO (reg) + offset);
      SET_DEST (pattern) = reg;
    }
}

/* Eliminate all occurrences of SETs of hard subregs from INSNS.  The
   only such SETs that we expect to see are those left in because
   integrate can't handle sets of parts of a return value register.

   We don't use alter_subreg because we only want to eliminate subregs
   of hard registers.  */

void
purge_hard_subreg_sets (rtx insn)
{
  for (; insn; insn = NEXT_INSN (insn))
    {
      if (INSN_P (insn))
	{
	  rtx pattern = PATTERN (insn);
	  switch (GET_CODE (pattern))
	    {
	    case SET:
	      if (GET_CODE (SET_DEST (pattern)) == SUBREG)
		purge_single_hard_subreg_set (pattern);
	      break;
	    case PARALLEL:
	      {
		int j;
		for (j = XVECLEN (pattern, 0) - 1; j >= 0; j--)
		  {
		    rtx inner_pattern = XVECEXP (pattern, 0, j);
		    if (GET_CODE (inner_pattern) == SET
			&& GET_CODE (SET_DEST (inner_pattern)) == SUBREG)
		      purge_single_hard_subreg_set (inner_pattern);
		  }
	      }
	      break;
	    default:
	      break;
	    }
	}
    }
}

/* Pass through the INSNS of function FNDECL and convert virtual register
   references to hard register references.  */

void
instantiate_virtual_regs (tree fndecl, rtx insns)
{
  rtx insn;
  unsigned int i;

  /* Compute the offsets to use for this function.  */
  in_arg_offset = FIRST_PARM_OFFSET (fndecl);
  var_offset = STARTING_FRAME_OFFSET;
  dynamic_offset = STACK_DYNAMIC_OFFSET (fndecl);
  out_arg_offset = STACK_POINTER_OFFSET;
  cfa_offset = ARG_POINTER_CFA_OFFSET (fndecl);

  /* Scan all variables and parameters of this function.  For each that is
     in memory, instantiate all virtual registers if the result is a valid
     address.  If not, we do it later.  That will handle most uses of virtual
     regs on many machines.  */
  instantiate_decls (fndecl, 1);

  /* Initialize recognition, indicating that volatile is OK.  */
  init_recog ();

  /* Scan through all the insns, instantiating every virtual register still
     present.  */
  for (insn = insns; insn; insn = NEXT_INSN (insn))
    if (GET_CODE (insn) == INSN || GET_CODE (insn) == JUMP_INSN
	|| GET_CODE (insn) == CALL_INSN)
      {
	instantiate_virtual_regs_1 (&PATTERN (insn), insn, 1);
	if (INSN_DELETED_P (insn))
	  continue;
	instantiate_virtual_regs_1 (&REG_NOTES (insn), NULL_RTX, 0);
	/* Instantiate any virtual registers in CALL_INSN_FUNCTION_USAGE.  */
	if (GET_CODE (insn) == CALL_INSN)
	  instantiate_virtual_regs_1 (&CALL_INSN_FUNCTION_USAGE (insn),
				      NULL_RTX, 0);

	/* Past this point all ASM statements should match.  Verify that
	   to avoid failures later in the compilation process.  */
        if (asm_noperands (PATTERN (insn)) >= 0
	    && ! check_asm_operands (PATTERN (insn)))
          instantiate_virtual_regs_lossage (insn);
      }

  /* Instantiate the stack slots for the parm registers, for later use in
     addressof elimination.  */
  for (i = 0; i < max_parm_reg; ++i)
    if (parm_reg_stack_loc[i])
      instantiate_virtual_regs_1 (&parm_reg_stack_loc[i], NULL_RTX, 0);

  /* Now instantiate the remaining register equivalences for debugging info.
     These will not be valid addresses.  */
  instantiate_decls (fndecl, 0);

  /* Indicate that, from now on, assign_stack_local should use
     frame_pointer_rtx.  */
  virtuals_instantiated = 1;
}

/* Scan all decls in FNDECL (both variables and parameters) and instantiate
   all virtual registers in their DECL_RTL's.

   If VALID_ONLY, do this only if the resulting address is still valid.
   Otherwise, always do it.  */

static void
instantiate_decls (tree fndecl, int valid_only)
{
  tree decl;

  /* Process all parameters of the function.  */
  for (decl = DECL_ARGUMENTS (fndecl); decl; decl = TREE_CHAIN (decl))
    {
      HOST_WIDE_INT size = int_size_in_bytes (TREE_TYPE (decl));
      HOST_WIDE_INT size_rtl;

      instantiate_decl (DECL_RTL (decl), size, valid_only);

      /* If the parameter was promoted, then the incoming RTL mode may be
	 larger than the declared type size.  We must use the larger of
	 the two sizes.  */
      size_rtl = GET_MODE_SIZE (GET_MODE (DECL_INCOMING_RTL (decl)));
      size = MAX (size_rtl, size);
      instantiate_decl (DECL_INCOMING_RTL (decl), size, valid_only);
    }

  /* Now process all variables defined in the function or its subblocks.  */
  instantiate_decls_1 (DECL_INITIAL (fndecl), valid_only);
}

/* Subroutine of instantiate_decls: Process all decls in the given
   BLOCK node and all its subblocks.  */

static void
instantiate_decls_1 (tree let, int valid_only)
{
  tree t;

  for (t = BLOCK_VARS (let); t; t = TREE_CHAIN (t))
    if (DECL_RTL_SET_P (t))
      instantiate_decl (DECL_RTL (t),
			int_size_in_bytes (TREE_TYPE (t)),
			valid_only);

  /* Process all subblocks.  */
  for (t = BLOCK_SUBBLOCKS (let); t; t = TREE_CHAIN (t))
    instantiate_decls_1 (t, valid_only);
}

/* Subroutine of the preceding procedures: Given RTL representing a
   decl and the size of the object, do any instantiation required.

   If VALID_ONLY is nonzero, it means that the RTL should only be
   changed if the new address is valid.  */

static void
instantiate_decl (rtx x, HOST_WIDE_INT size, int valid_only)
{
  enum machine_mode mode;
  rtx addr;

  if (x == 0)
    return;

  /* If this is a CONCAT, recurse for the pieces.  */
  if (GET_CODE (x) == CONCAT)
    {
      instantiate_decl (XEXP (x, 0), size / 2, valid_only);
      instantiate_decl (XEXP (x, 1), size / 2, valid_only);
      return;
    }

  /* If this is not a MEM, no need to do anything.  Similarly if the
     address is a constant or a register that is not a virtual register.  */
  if (GET_CODE (x) != MEM)
    return;

  addr = XEXP (x, 0);
  if (CONSTANT_P (addr)
      || (GET_CODE (addr) == ADDRESSOF && GET_CODE (XEXP (addr, 0)) == REG)
      || (GET_CODE (addr) == REG
	  && (REGNO (addr) < FIRST_VIRTUAL_REGISTER
	      || REGNO (addr) > LAST_VIRTUAL_REGISTER)))
    return;

  /* If we should only do this if the address is valid, copy the address.
     We need to do this so we can undo any changes that might make the
     address invalid.  This copy is unfortunate, but probably can't be
     avoided.  */

  if (valid_only)
    addr = copy_rtx (addr);

  instantiate_virtual_regs_1 (&addr, NULL_RTX, 0);

  if (valid_only && size >= 0)
    {
      unsigned HOST_WIDE_INT decl_size = size;

      /* Now verify that the resulting address is valid for every integer or
	 floating-point mode up to and including SIZE bytes long.  We do this
	 since the object might be accessed in any mode and frame addresses
	 are shared.  */

      for (mode = GET_CLASS_NARROWEST_MODE (MODE_INT);
	   mode != VOIDmode && GET_MODE_SIZE (mode) <= decl_size;
	   mode = GET_MODE_WIDER_MODE (mode))
	if (! memory_address_p (mode, addr))
	  return;

      for (mode = GET_CLASS_NARROWEST_MODE (MODE_FLOAT);
	   mode != VOIDmode && GET_MODE_SIZE (mode) <= decl_size;
	   mode = GET_MODE_WIDER_MODE (mode))
	if (! memory_address_p (mode, addr))
	  return;
    }

  /* Put back the address now that we have updated it and we either know
     it is valid or we don't care whether it is valid.  */

  XEXP (x, 0) = addr;
}

/* Given a piece of RTX and a pointer to a HOST_WIDE_INT, if the RTX
   is a virtual register, return the equivalent hard register and set the
   offset indirectly through the pointer.  Otherwise, return 0.  */

static rtx
instantiate_new_reg (rtx x, HOST_WIDE_INT *poffset)
{
  rtx new;
  HOST_WIDE_INT offset;

  if (x == virtual_incoming_args_rtx)
    new = arg_pointer_rtx, offset = in_arg_offset;
  else if (x == virtual_stack_vars_rtx)
    new = frame_pointer_rtx, offset = var_offset;
  else if (x == virtual_stack_dynamic_rtx)
    new = stack_pointer_rtx, offset = dynamic_offset;
  else if (x == virtual_outgoing_args_rtx)
    new = stack_pointer_rtx, offset = out_arg_offset;
  else if (x == virtual_cfa_rtx)
    new = arg_pointer_rtx, offset = cfa_offset;
  else
    return 0;

  *poffset = offset;
  return new;
}


/* Called when instantiate_virtual_regs has failed to update the instruction.
   Usually this means that non-matching instruction has been emit, however for
   asm statements it may be the problem in the constraints.  */
static void
instantiate_virtual_regs_lossage (rtx insn)
{
  if (asm_noperands (PATTERN (insn)) >= 0)
    {
      error_for_asm (insn, "impossible constraint in `asm'");
      delete_insn (insn);
    }
  else
    abort ();
}
/* Given a pointer to a piece of rtx and an optional pointer to the
   containing object, instantiate any virtual registers present in it.

   If EXTRA_INSNS, we always do the replacement and generate
   any extra insns before OBJECT.  If it zero, we do nothing if replacement
   is not valid.

   Return 1 if we either had nothing to do or if we were able to do the
   needed replacement.  Return 0 otherwise; we only return zero if
   EXTRA_INSNS is zero.

   We first try some simple transformations to avoid the creation of extra
   pseudos.  */

static int
instantiate_virtual_regs_1 (rtx *loc, rtx object, int extra_insns)
{
  rtx x;
  RTX_CODE code;
  rtx new = 0;
  HOST_WIDE_INT offset = 0;
  rtx temp;
  rtx seq;
  int i, j;
  const char *fmt;

  /* Re-start here to avoid recursion in common cases.  */
 restart:

  x = *loc;
  if (x == 0)
    return 1;

  /* We may have detected and deleted invalid asm statements.  */
  if (object && INSN_P (object) && INSN_DELETED_P (object))
    return 1;

  code = GET_CODE (x);

  /* Check for some special cases.  */
  switch (code)
    {
    case CONST_INT:
    case CONST_DOUBLE:
    case CONST_VECTOR:
    case CONST:
    case SYMBOL_REF:
    case CODE_LABEL:
    case PC:
    case CC0:
    case ASM_INPUT:
    case ADDR_VEC:
    case ADDR_DIFF_VEC:
    case RETURN:
      return 1;

    case SET:
      /* We are allowed to set the virtual registers.  This means that
	 the actual register should receive the source minus the
	 appropriate offset.  This is used, for example, in the handling
	 of non-local gotos.  */
      if ((new = instantiate_new_reg (SET_DEST (x), &offset)) != 0)
	{
	  rtx src = SET_SRC (x);

	  /* We are setting the register, not using it, so the relevant
	     offset is the negative of the offset to use were we using
	     the register.  */
	  offset = - offset;
	  instantiate_virtual_regs_1 (&src, NULL_RTX, 0);

	  /* The only valid sources here are PLUS or REG.  Just do
	     the simplest possible thing to handle them.  */
	  if (GET_CODE (src) != REG && GET_CODE (src) != PLUS)
	    {
	      instantiate_virtual_regs_lossage (object);
	      return 1;
	    }

	  start_sequence ();
	  if (GET_CODE (src) != REG)
	    temp = force_operand (src, NULL_RTX);
	  else
	    temp = src;
	  temp = force_operand (plus_constant (temp, offset), NULL_RTX);
	  seq = get_insns ();
	  end_sequence ();

	  emit_insn_before (seq, object);
	  SET_DEST (x) = new;

	  if (! validate_change (object, &SET_SRC (x), temp, 0)
	      || ! extra_insns)
	    instantiate_virtual_regs_lossage (object);

	  return 1;
	}

      instantiate_virtual_regs_1 (&SET_DEST (x), object, extra_insns);
      loc = &SET_SRC (x);
      goto restart;

    case PLUS:
      /* Handle special case of virtual register plus constant.  */
      if (CONSTANT_P (XEXP (x, 1)))
	{
	  rtx old, new_offset;

	  /* Check for (plus (plus VIRT foo) (const_int)) first.  */
	  if (GET_CODE (XEXP (x, 0)) == PLUS)
	    {
	      if ((new = instantiate_new_reg (XEXP (XEXP (x, 0), 0), &offset)))
		{
		  instantiate_virtual_regs_1 (&XEXP (XEXP (x, 0), 1), object,
					      extra_insns);
		  new = gen_rtx_PLUS (Pmode, new, XEXP (XEXP (x, 0), 1));
		}
	      else
		{
		  loc = &XEXP (x, 0);
		  goto restart;
		}
	    }

#ifdef POINTERS_EXTEND_UNSIGNED
	  /* If we have (plus (subreg (virtual-reg)) (const_int)), we know
	     we can commute the PLUS and SUBREG because pointers into the
	     frame are well-behaved.  */
	  else if (GET_CODE (XEXP (x, 0)) == SUBREG && GET_MODE (x) == ptr_mode
		   && GET_CODE (XEXP (x, 1)) == CONST_INT
		   && 0 != (new
			    = instantiate_new_reg (SUBREG_REG (XEXP (x, 0)),
						   &offset))
		   && validate_change (object, loc,
				       plus_constant (gen_lowpart (ptr_mode,
								   new),
						      offset
						      + INTVAL (XEXP (x, 1))),
				       0))
		return 1;
#endif
	  else if ((new = instantiate_new_reg (XEXP (x, 0), &offset)) == 0)
	    {
	      /* We know the second operand is a constant.  Unless the
		 first operand is a REG (which has been already checked),
		 it needs to be checked.  */
	      if (GET_CODE (XEXP (x, 0)) != REG)
		{
		  loc = &XEXP (x, 0);
		  goto restart;
		}
	      return 1;
	    }

	  new_offset = plus_constant (XEXP (x, 1), offset);

	  /* If the new constant is zero, try to replace the sum with just
	     the register.  */
	  if (new_offset == const0_rtx
	      && validate_change (object, loc, new, 0))
	    return 1;

	  /* Next try to replace the register and new offset.
	     There are two changes to validate here and we can't assume that
	     in the case of old offset equals new just changing the register
	     will yield a valid insn.  In the interests of a little efficiency,
	     however, we only call validate change once (we don't queue up the
	     changes and then call apply_change_group).  */

	  old = XEXP (x, 0);
	  if (offset == 0
	      ? ! validate_change (object, &XEXP (x, 0), new, 0)
	      : (XEXP (x, 0) = new,
		 ! validate_change (object, &XEXP (x, 1), new_offset, 0)))
	    {
	      if (! extra_insns)
		{
		  XEXP (x, 0) = old;
		  return 0;
		}

	      /* Otherwise copy the new constant into a register and replace
		 constant with that register.  */
	      temp = gen_reg_rtx (Pmode);
	      XEXP (x, 0) = new;
	      if (validate_change (object, &XEXP (x, 1), temp, 0))
		emit_insn_before (gen_move_insn (temp, new_offset), object);
	      else
		{
		  /* If that didn't work, replace this expression with a
		     register containing the sum.  */

		  XEXP (x, 0) = old;
		  new = gen_rtx_PLUS (Pmode, new, new_offset);

		  start_sequence ();
		  temp = force_operand (new, NULL_RTX);
		  seq = get_insns ();
		  end_sequence ();

		  emit_insn_before (seq, object);
		  if (! validate_change (object, loc, temp, 0)
		      && ! validate_replace_rtx (x, temp, object))
		    {
		      instantiate_virtual_regs_lossage (object);
		      return 1;
		    }
		}
	    }

	  return 1;
	}

      /* Fall through to generic two-operand expression case.  */
    case EXPR_LIST:
    case CALL:
    case COMPARE:
    case MINUS:
    case MULT:
    case DIV:      case UDIV:
    case MOD:      case UMOD:
    case AND:      case IOR:      case XOR:
    case ROTATERT: case ROTATE:
    case ASHIFTRT: case LSHIFTRT: case ASHIFT:
    case NE:       case EQ:
    case GE:       case GT:       case GEU:    case GTU:
    case LE:       case LT:       case LEU:    case LTU:
      if (XEXP (x, 1) && ! CONSTANT_P (XEXP (x, 1)))
	instantiate_virtual_regs_1 (&XEXP (x, 1), object, extra_insns);
      loc = &XEXP (x, 0);
      goto restart;

    case MEM:
      /* Most cases of MEM that convert to valid addresses have already been
	 handled by our scan of decls.  The only special handling we
	 need here is to make a copy of the rtx to ensure it isn't being
	 shared if we have to change it to a pseudo.

	 If the rtx is a simple reference to an address via a virtual register,
	 it can potentially be shared.  In such cases, first try to make it
	 a valid address, which can also be shared.  Otherwise, copy it and
	 proceed normally.

	 First check for common cases that need no processing.  These are
	 usually due to instantiation already being done on a previous instance
	 of a shared rtx.  */

      temp = XEXP (x, 0);
      if (CONSTANT_ADDRESS_P (temp)
#if FRAME_POINTER_REGNUM != ARG_POINTER_REGNUM
	  || temp == arg_pointer_rtx
#endif
#if HARD_FRAME_POINTER_REGNUM != FRAME_POINTER_REGNUM
	  || temp == hard_frame_pointer_rtx
#endif
	  || temp == frame_pointer_rtx)
	return 1;

      if (GET_CODE (temp) == PLUS
	  && CONSTANT_ADDRESS_P (XEXP (temp, 1))
	  && (XEXP (temp, 0) == frame_pointer_rtx
#if HARD_FRAME_POINTER_REGNUM != FRAME_POINTER_REGNUM
	      || XEXP (temp, 0) == hard_frame_pointer_rtx
#endif
#if FRAME_POINTER_REGNUM != ARG_POINTER_REGNUM
	      || XEXP (temp, 0) == arg_pointer_rtx
#endif
	      ))
	return 1;

      if (temp == virtual_stack_vars_rtx
	  || temp == virtual_incoming_args_rtx
	  || (GET_CODE (temp) == PLUS
	      && CONSTANT_ADDRESS_P (XEXP (temp, 1))
	      && (XEXP (temp, 0) == virtual_stack_vars_rtx
		  || XEXP (temp, 0) == virtual_incoming_args_rtx)))
	{
	  /* This MEM may be shared.  If the substitution can be done without
	     the need to generate new pseudos, we want to do it in place
	     so all copies of the shared rtx benefit.  The call below will
	     only make substitutions if the resulting address is still
	     valid.

	     Note that we cannot pass X as the object in the recursive call
	     since the insn being processed may not allow all valid
	     addresses.  However, if we were not passed on object, we can
	     only modify X without copying it if X will have a valid
	     address.

	     ??? Also note that this can still lose if OBJECT is an insn that
	     has less restrictions on an address that some other insn.
	     In that case, we will modify the shared address.  This case
	     doesn't seem very likely, though.  One case where this could
	     happen is in the case of a USE or CLOBBER reference, but we
	     take care of that below.  */

	  if (instantiate_virtual_regs_1 (&XEXP (x, 0),
					  object ? object : x, 0))
	    return 1;

	  /* Otherwise make a copy and process that copy.  We copy the entire
	     RTL expression since it might be a PLUS which could also be
	     shared.  */
	  *loc = x = copy_rtx (x);
	}

      /* Fall through to generic unary operation case.  */
    case PREFETCH:
    case SUBREG:
    case STRICT_LOW_PART:
    case NEG:          case NOT:
    case PRE_DEC:      case PRE_INC:      case POST_DEC:    case POST_INC:
    case SIGN_EXTEND:  case ZERO_EXTEND:
    case TRUNCATE:     case FLOAT_EXTEND: case FLOAT_TRUNCATE:
    case FLOAT:        case FIX:
    case UNSIGNED_FIX: case UNSIGNED_FLOAT:
    case ABS:
    case SQRT:
    case FFS:
    case CLZ:          case CTZ:
    case POPCOUNT:     case PARITY:
      /* These case either have just one operand or we know that we need not
	 check the rest of the operands.  */
      loc = &XEXP (x, 0);
      goto restart;

    case USE:
    case CLOBBER:
      /* If the operand is a MEM, see if the change is a valid MEM.  If not,
	 go ahead and make the invalid one, but do it to a copy.  For a REG,
	 just make the recursive call, since there's no chance of a problem.  */

      if ((GET_CODE (XEXP (x, 0)) == MEM
	   && instantiate_virtual_regs_1 (&XEXP (XEXP (x, 0), 0), XEXP (x, 0),
					  0))
	  || (GET_CODE (XEXP (x, 0)) == REG
	      && instantiate_virtual_regs_1 (&XEXP (x, 0), object, 0)))
	return 1;

      XEXP (x, 0) = copy_rtx (XEXP (x, 0));
      loc = &XEXP (x, 0);
      goto restart;

    case REG:
      /* Try to replace with a PLUS.  If that doesn't work, compute the sum
	 in front of this insn and substitute the temporary.  */
      if ((new = instantiate_new_reg (x, &offset)) != 0)
	{
	  temp = plus_constant (new, offset);
	  if (!validate_change (object, loc, temp, 0))
	    {
	      if (! extra_insns)
		return 0;

	      start_sequence ();
	      temp = force_operand (temp, NULL_RTX);
	      seq = get_insns ();
	      end_sequence ();

	      emit_insn_before (seq, object);
	      if (! validate_change (object, loc, temp, 0)
		  && ! validate_replace_rtx (x, temp, object))
	        instantiate_virtual_regs_lossage (object);
	    }
	}

      return 1;

    case ADDRESSOF:
      if (GET_CODE (XEXP (x, 0)) == REG)
	return 1;

      else if (GET_CODE (XEXP (x, 0)) == MEM)
	{
	  /* If we have a (addressof (mem ..)), do any instantiation inside
	     since we know we'll be making the inside valid when we finally
	     remove the ADDRESSOF.  */
	  instantiate_virtual_regs_1 (&XEXP (XEXP (x, 0), 0), NULL_RTX, 0);
	  return 1;
	}
      break;

    default:
      break;
    }

  /* Scan all subexpressions.  */
  fmt = GET_RTX_FORMAT (code);
  for (i = 0; i < GET_RTX_LENGTH (code); i++, fmt++)
    if (*fmt == 'e')
      {
	if (!instantiate_virtual_regs_1 (&XEXP (x, i), object, extra_insns))
	  return 0;
      }
    else if (*fmt == 'E')
      for (j = 0; j < XVECLEN (x, i); j++)
	if (! instantiate_virtual_regs_1 (&XVECEXP (x, i, j), object,
					  extra_insns))
	  return 0;

  return 1;
}

/* Optimization: assuming this function does not receive nonlocal gotos,
   delete the handlers for such, as well as the insns to establish
   and disestablish them.  */

static void
delete_handlers (void)
{
  rtx insn;
  for (insn = get_insns (); insn; insn = NEXT_INSN (insn))
    {
      /* Delete the handler by turning off the flag that would
	 prevent jump_optimize from deleting it.
	 Also permit deletion of the nonlocal labels themselves
	 if nothing local refers to them.  */
      if (GET_CODE (insn) == CODE_LABEL)
	{
	  tree t, last_t;

	  LABEL_PRESERVE_P (insn) = 0;

	  /* Remove it from the nonlocal_label list, to avoid confusing
	     flow.  */
	  for (t = nonlocal_labels, last_t = 0; t;
	       last_t = t, t = TREE_CHAIN (t))
	    if (DECL_RTL (TREE_VALUE (t)) == insn)
	      break;
	  if (t)
	    {
	      if (! last_t)
		nonlocal_labels = TREE_CHAIN (nonlocal_labels);
	      else
		TREE_CHAIN (last_t) = TREE_CHAIN (t);
	    }
	}
      if (GET_CODE (insn) == INSN)
	{
	  int can_delete = 0;
	  rtx t;
	  for (t = nonlocal_goto_handler_slots; t != 0; t = XEXP (t, 1))
	    if (reg_mentioned_p (t, PATTERN (insn)))
	      {
		can_delete = 1;
		break;
	      }
	  if (can_delete
	      || (nonlocal_goto_stack_level != 0
		  && reg_mentioned_p (nonlocal_goto_stack_level,
				      PATTERN (insn))))
	    delete_related_insns (insn);
	}
    }
}

/* Return the first insn following those generated by `assign_parms'.  */

rtx
get_first_nonparm_insn (void)
{
  if (last_parm_insn)
    return NEXT_INSN (last_parm_insn);
  return get_insns ();
}

/* Return 1 if EXP is an aggregate type (or a value with aggregate type).
   This means a type for which function calls must pass an address to the
   function or get an address back from the function.
   EXP may be a type node or an expression (whose type is tested).  */

int
aggregate_value_p (tree exp, tree fntype)
{
  int i, regno, nregs;
  rtx reg;

  tree type = (TYPE_P (exp)) ? exp : TREE_TYPE (exp);

  if (fntype)
    switch (TREE_CODE (fntype))
      {
      case CALL_EXPR:
	fntype = get_callee_fndecl (fntype);
	fntype = fntype ? TREE_TYPE (fntype) : 0;
	break;
      case FUNCTION_DECL:
	fntype = TREE_TYPE (fntype);
	break;
      case FUNCTION_TYPE:
      case METHOD_TYPE:
        break;
      case IDENTIFIER_NODE:
	fntype = 0;
	break;
      default:
	/* We don't expect other rtl types here.  */
	abort();
      }

  if (TREE_CODE (type) == VOID_TYPE)
    return 0;
  if (targetm.calls.return_in_memory (type, fntype))
    return 1;
  /* Types that are TREE_ADDRESSABLE must be constructed in memory,
     and thus can't be returned in registers.  */
  if (TREE_ADDRESSABLE (type))
    return 1;
  if (flag_pcc_struct_return && AGGREGATE_TYPE_P (type))
    return 1;
  /* Make sure we have suitable call-clobbered regs to return
     the value in; if not, we must return it in memory.  */
  reg = hard_function_value (type, 0, 0);

  /* If we have something other than a REG (e.g. a PARALLEL), then assume
     it is OK.  */
  if (GET_CODE (reg) != REG)
    return 0;

  regno = REGNO (reg);
  nregs = HARD_REGNO_NREGS (regno, TYPE_MODE (type));
  for (i = 0; i < nregs; i++)
    if (! call_used_regs[regno + i])
      return 1;
  return 0;
}

/* Assign RTL expressions to the function's parameters.
   This may involve copying them into registers and using
   those registers as the RTL for them.  */

void
assign_parms (tree fndecl)
{
  tree parm;
  CUMULATIVE_ARGS args_so_far;
  /* Total space needed so far for args on the stack,
     given as a constant and a tree-expression.  */
  struct args_size stack_args_size;
  tree fntype = TREE_TYPE (fndecl);
  tree fnargs = DECL_ARGUMENTS (fndecl), orig_fnargs;
  /* This is used for the arg pointer when referring to stack args.  */
  rtx internal_arg_pointer;
  /* This is a dummy PARM_DECL that we used for the function result if
     the function returns a structure.  */
  tree function_result_decl = 0;
  int varargs_setup = 0;
  int reg_parm_stack_space ATTRIBUTE_UNUSED = 0;
  rtx conversion_insns = 0;

  /* Nonzero if function takes extra anonymous args.
     This means the last named arg must be on the stack
     right before the anonymous ones.  */
  int stdarg
    = (TYPE_ARG_TYPES (fntype) != 0
       && (TREE_VALUE (tree_last (TYPE_ARG_TYPES (fntype)))
	   != void_type_node));

  current_function_stdarg = stdarg;

  /* If the reg that the virtual arg pointer will be translated into is
     not a fixed reg or is the stack pointer, make a copy of the virtual
     arg pointer, and address parms via the copy.  The frame pointer is
     considered fixed even though it is not marked as such.

     The second time through, simply use ap to avoid generating rtx.  */

  if ((ARG_POINTER_REGNUM == STACK_POINTER_REGNUM
       || ! (fixed_regs[ARG_POINTER_REGNUM]
	     || ARG_POINTER_REGNUM == FRAME_POINTER_REGNUM)))
    internal_arg_pointer = copy_to_reg (virtual_incoming_args_rtx);
  else
    internal_arg_pointer = virtual_incoming_args_rtx;
  current_function_internal_arg_pointer = internal_arg_pointer;

  stack_args_size.constant = 0;
  stack_args_size.var = 0;

  /* If struct value address is treated as the first argument, make it so.  */
  if (aggregate_value_p (DECL_RESULT (fndecl), fndecl)
      && ! current_function_returns_pcc_struct
      && targetm.calls.struct_value_rtx (TREE_TYPE (fndecl), 1) == 0)
    {
      tree type = build_pointer_type (TREE_TYPE (fntype));

      function_result_decl = build_decl (PARM_DECL, NULL_TREE, type);

      DECL_ARG_TYPE (function_result_decl) = type;
      TREE_CHAIN (function_result_decl) = fnargs;
      fnargs = function_result_decl;
    }

  orig_fnargs = fnargs;

  max_parm_reg = LAST_VIRTUAL_REGISTER + 1;
  parm_reg_stack_loc = ggc_alloc_cleared (max_parm_reg * sizeof (rtx));

  /* If the target wants to split complex arguments into scalars, do so.  */
  if (targetm.calls.split_complex_arg)
    fnargs = split_complex_args (fnargs);

#ifdef REG_PARM_STACK_SPACE
#ifdef MAYBE_REG_PARM_STACK_SPACE
  reg_parm_stack_space = MAYBE_REG_PARM_STACK_SPACE;
#else
  reg_parm_stack_space = REG_PARM_STACK_SPACE (fndecl);
#endif
#endif

#ifdef INIT_CUMULATIVE_INCOMING_ARGS
  INIT_CUMULATIVE_INCOMING_ARGS (args_so_far, fntype, NULL_RTX);
#else
  INIT_CUMULATIVE_ARGS (args_so_far, fntype, NULL_RTX, fndecl, -1);
#endif

  /* We haven't yet found an argument that we must push and pretend the
     caller did.  */
  current_function_pretend_args_size = 0;

  for (parm = fnargs; parm; parm = TREE_CHAIN (parm))
    {
      rtx entry_parm;
      rtx stack_parm;
      enum machine_mode promoted_mode, passed_mode;
      enum machine_mode nominal_mode, promoted_nominal_mode;
      int unsignedp;
      struct locate_and_pad_arg_data locate;
      int passed_pointer = 0;
      int did_conversion = 0;
      tree passed_type = DECL_ARG_TYPE (parm);
      tree nominal_type = TREE_TYPE (parm);
      int last_named = 0, named_arg;
      int in_regs;
      int partial = 0;
      int pretend_bytes = 0;

      /* Set LAST_NAMED if this is last named arg before last
	 anonymous args.  */
      if (stdarg)
	{
	  tree tem;

	  for (tem = TREE_CHAIN (parm); tem; tem = TREE_CHAIN (tem))
	    if (DECL_NAME (tem))
	      break;

	  if (tem == 0)
	    last_named = 1;
	}
      /* Set NAMED_ARG if this arg should be treated as a named arg.  For
	 most machines, if this is a varargs/stdarg function, then we treat
	 the last named arg as if it were anonymous too.  */
      named_arg = targetm.calls.strict_argument_naming (&args_so_far) ? 1 : ! last_named;

      if (TREE_TYPE (parm) == error_mark_node
	  /* This can happen after weird syntax errors
	     or if an enum type is defined among the parms.  */
	  || TREE_CODE (parm) != PARM_DECL
	  || passed_type == NULL)
	{
	  SET_DECL_RTL (parm, gen_rtx_MEM (BLKmode, const0_rtx));
	  DECL_INCOMING_RTL (parm) = DECL_RTL (parm);
	  TREE_USED (parm) = 1;
	  continue;
	}

      /* Find mode of arg as it is passed, and mode of arg
	 as it should be during execution of this function.  */
      passed_mode = TYPE_MODE (passed_type);
      nominal_mode = TYPE_MODE (nominal_type);

      /* If the parm's mode is VOID, its value doesn't matter,
	 and avoid the usual things like emit_move_insn that could crash.  */
      if (nominal_mode == VOIDmode)
	{
	  SET_DECL_RTL (parm, const0_rtx);
	  DECL_INCOMING_RTL (parm) = DECL_RTL (parm);
	  continue;
	}

      /* If the parm is to be passed as a transparent union, use the
	 type of the first field for the tests below.  We have already
	 verified that the modes are the same.  */
      if (DECL_TRANSPARENT_UNION (parm)
	  || (TREE_CODE (passed_type) == UNION_TYPE
	      && TYPE_TRANSPARENT_UNION (passed_type)))
	passed_type = TREE_TYPE (TYPE_FIELDS (passed_type));

      /* See if this arg was passed by invisible reference.  It is if
	 it is an object whose size depends on the contents of the
	 object itself or if the machine requires these objects be passed
	 that way.  */

      if (CONTAINS_PLACEHOLDER_P (TYPE_SIZE (passed_type))
	  || TREE_ADDRESSABLE (passed_type)
#ifdef FUNCTION_ARG_PASS_BY_REFERENCE
	  || FUNCTION_ARG_PASS_BY_REFERENCE (args_so_far, passed_mode,
					     passed_type, named_arg)
#endif
	  )
	{
	  passed_type = nominal_type = build_pointer_type (passed_type);
	  passed_pointer = 1;
	  passed_mode = nominal_mode = Pmode;
	}
      /* See if the frontend wants to pass this by invisible reference.  */
      else if (passed_type != nominal_type
	       && POINTER_TYPE_P (passed_type)
	       && TREE_TYPE (passed_type) == nominal_type)
	{
	  nominal_type = passed_type;
	  passed_pointer = 1;
	  passed_mode = nominal_mode = Pmode;
	}

      promoted_mode = passed_mode;

      if (targetm.calls.promote_function_args (TREE_TYPE (fndecl)))
	{
	  /* Compute the mode in which the arg is actually extended to.  */
	  unsignedp = TREE_UNSIGNED (passed_type);
	  promoted_mode = promote_mode (passed_type, promoted_mode, &unsignedp, 1);
	}

      /* Let machine desc say which reg (if any) the parm arrives in.
	 0 means it arrives on the stack.  */
#ifdef FUNCTION_INCOMING_ARG
      entry_parm = FUNCTION_INCOMING_ARG (args_so_far, promoted_mode,
					  passed_type, named_arg);
#else
      entry_parm = FUNCTION_ARG (args_so_far, promoted_mode,
				 passed_type, named_arg);
#endif

      if (entry_parm == 0)
	promoted_mode = passed_mode;

      /* If this is the last named parameter, do any required setup for
	 varargs or stdargs.  We need to know about the case of this being an
	 addressable type, in which case we skip the registers it
	 would have arrived in.

	 For stdargs, LAST_NAMED will be set for two parameters, the one that
	 is actually the last named, and the dummy parameter.  We only
	 want to do this action once.

	 Also, indicate when RTL generation is to be suppressed.  */
      if (last_named && !varargs_setup)
	{
	  int varargs_pretend_bytes = 0;
	  targetm.calls.setup_incoming_varargs (&args_so_far, promoted_mode,
						passed_type,
						&varargs_pretend_bytes, 0);
	  varargs_setup = 1;

	  /* If the back-end has requested extra stack space, record how
	     much is needed.  Do not change pretend_args_size otherwise
	     since it may be nonzero from an earlier partial argument.  */
	  if (varargs_pretend_bytes > 0)
	    current_function_pretend_args_size = varargs_pretend_bytes;
	}

      /* Determine parm's home in the stack,
	 in case it arrives in the stack or we should pretend it did.

	 Compute the stack position and rtx where the argument arrives
	 and its size.

	 There is one complexity here:  If this was a parameter that would
	 have been passed in registers, but wasn't only because it is
	 __builtin_va_alist, we want locate_and_pad_parm to treat it as if
	 it came in a register so that REG_PARM_STACK_SPACE isn't skipped.
	 In this case, we call FUNCTION_ARG with NAMED set to 1 instead of
	 0 as it was the previous time.  */
      in_regs = entry_parm != 0;
#ifdef STACK_PARMS_IN_REG_PARM_AREA
      in_regs = 1;
#endif
      if (!in_regs && !named_arg)
	{
	  int pretend_named =
	    targetm.calls.pretend_outgoing_varargs_named (&args_so_far);
	  if (pretend_named)
	    {
#ifdef FUNCTION_INCOMING_ARG
	      in_regs = FUNCTION_INCOMING_ARG (args_so_far, promoted_mode,
					       passed_type,
					       pretend_named) != 0;
#else
	      in_regs = FUNCTION_ARG (args_so_far, promoted_mode,
				      passed_type,
				      pretend_named) != 0;
#endif
	    }
	}

      /* If this parameter was passed both in registers and in the stack,
	 use the copy on the stack.  */
      if (MUST_PASS_IN_STACK (promoted_mode, passed_type))
	entry_parm = 0;

#ifdef FUNCTION_ARG_PARTIAL_NREGS
      if (entry_parm)
	{
	  partial = FUNCTION_ARG_PARTIAL_NREGS (args_so_far, promoted_mode,
						passed_type, named_arg);
	  if (partial
#ifndef MAYBE_REG_PARM_STACK_SPACE
	      /* The caller might already have allocated stack space
		 for the register parameters.  */
	      && reg_parm_stack_space == 0
#endif
	      )
	    {
	      /* Part of this argument is passed in registers and part
		 is passed on the stack.  Ask the prologue code to extend
		 the stack part so that we can recreate the full value.

		 PRETEND_BYTES is the size of the registers we need to store.
		 CURRENT_FUNCTION_PRETEND_ARGS_SIZE is the amount of extra
		 stack space that the prologue should allocate.

		 Internally, gcc assumes that the argument pointer is
		 aligned to STACK_BOUNDARY bits.  This is used both for
		 alignment optimizations (see init_emit) and to locate
		 arguments that are aligned to more than PARM_BOUNDARY
		 bits.  We must preserve this invariant by rounding
		 CURRENT_FUNCTION_PRETEND_ARGS_SIZE up to a stack
		 boundary.  */
	      pretend_bytes = partial * UNITS_PER_WORD;
	      current_function_pretend_args_size
		= CEIL_ROUND (pretend_bytes, STACK_BYTES);

	      /* If PRETEND_BYTES != CURRENT_FUNCTION_PRETEND_ARGS_SIZE,
		 insert the padding before the start of the first pretend
		 argument.  */
	      stack_args_size.constant
		= (current_function_pretend_args_size - pretend_bytes);
	    }
	}
#endif

      memset (&locate, 0, sizeof (locate));
      locate_and_pad_parm (promoted_mode, passed_type, in_regs,
			   entry_parm ? partial : 0, fndecl,
			   &stack_args_size, &locate);

      {
	rtx offset_rtx;
	unsigned int align, boundary;

	/* If we're passing this arg using a reg, make its stack home
	   the aligned stack slot.  */
	if (entry_parm)
	  offset_rtx = ARGS_SIZE_RTX (locate.slot_offset);
	else
	  offset_rtx = ARGS_SIZE_RTX (locate.offset);

	if (offset_rtx == const0_rtx)
	  stack_parm = gen_rtx_MEM (promoted_mode, internal_arg_pointer);
	else
	  stack_parm = gen_rtx_MEM (promoted_mode,
				    gen_rtx_PLUS (Pmode,
						  internal_arg_pointer,
						  offset_rtx));

	set_mem_attributes (stack_parm, parm, 1);

	boundary = FUNCTION_ARG_BOUNDARY (promoted_mode, passed_type);
	align = 0;

	/* If we're padding upward, we know that the alignment of the slot
	   is FUNCTION_ARG_BOUNDARY.  If we're using slot_offset, we're
	   intentionally forcing upward padding.  Otherwise we have to come
	   up with a guess at the alignment based on OFFSET_RTX.  */
	if (locate.where_pad == upward || entry_parm)
	  align = boundary;
	else if (GET_CODE (offset_rtx) == CONST_INT)
	  {
	    align = INTVAL (offset_rtx) * BITS_PER_UNIT | boundary;
	    align = align & -align;
	  }
	if (align > 0)
	  set_mem_align (stack_parm, align);

	if (entry_parm)
	  set_reg_attrs_for_parm (entry_parm, stack_parm);
      }

      /* If this parm was passed part in regs and part in memory,
	 pretend it arrived entirely in memory
	 by pushing the register-part onto the stack.

	 In the special case of a DImode or DFmode that is split,
	 we could put it together in a pseudoreg directly,
	 but for now that's not worth bothering with.  */

      if (partial)
	{
	  /* Handle calls that pass values in multiple non-contiguous
	     locations.  The Irix 6 ABI has examples of this.  */
	  if (GET_CODE (entry_parm) == PARALLEL)
	    emit_group_store (validize_mem (stack_parm), entry_parm,
			      TREE_TYPE (parm),
			      int_size_in_bytes (TREE_TYPE (parm)));

	  else
	    move_block_from_reg (REGNO (entry_parm), validize_mem (stack_parm),
				 partial);

	  entry_parm = stack_parm;
	}

      /* If we didn't decide this parm came in a register,
	 by default it came on the stack.  */
      if (entry_parm == 0)
	entry_parm = stack_parm;

      /* Record permanently how this parm was passed.  */
      DECL_INCOMING_RTL (parm) = entry_parm;

      /* If there is actually space on the stack for this parm,
	 count it in stack_args_size; otherwise set stack_parm to 0
	 to indicate there is no preallocated stack slot for the parm.  */

      if (entry_parm == stack_parm
	  || (GET_CODE (entry_parm) == PARALLEL
	      && XEXP (XVECEXP (entry_parm, 0, 0), 0) == NULL_RTX)
#if defined (REG_PARM_STACK_SPACE) && ! defined (MAYBE_REG_PARM_STACK_SPACE)
	  /* On some machines, even if a parm value arrives in a register
	     there is still an (uninitialized) stack slot allocated for it.

	     ??? When MAYBE_REG_PARM_STACK_SPACE is defined, we can't tell
	     whether this parameter already has a stack slot allocated,
	     because an arg block exists only if current_function_args_size
	     is larger than some threshold, and we haven't calculated that
	     yet.  So, for now, we just assume that stack slots never exist
	     in this case.  */
	  || REG_PARM_STACK_SPACE (fndecl) > 0
#endif
	  )
	{
	  stack_args_size.constant += pretend_bytes + locate.size.constant;
	  if (locate.size.var)
	    ADD_PARM_SIZE (stack_args_size, locate.size.var);
	}
      else
	/* No stack slot was pushed for this parm.  */
	stack_parm = 0;

      /* Update info on where next arg arrives in registers.  */

      FUNCTION_ARG_ADVANCE (args_so_far, promoted_mode,
			    passed_type, named_arg);

      /* If we can't trust the parm stack slot to be aligned enough
	 for its ultimate type, don't use that slot after entry.
	 We'll make another stack slot, if we need one.  */
      if (STRICT_ALIGNMENT && stack_parm
	  && GET_MODE_ALIGNMENT (nominal_mode) > MEM_ALIGN (stack_parm))
	stack_parm = 0;

      /* If parm was passed in memory, and we need to convert it on entry,
	 don't store it back in that same slot.  */
      if (entry_parm == stack_parm
	  && nominal_mode != BLKmode && nominal_mode != passed_mode)
	stack_parm = 0;

      /* When an argument is passed in multiple locations, we can't
	 make use of this information, but we can save some copying if
	 the whole argument is passed in a single register.  */
      if (GET_CODE (entry_parm) == PARALLEL
	  && nominal_mode != BLKmode && passed_mode != BLKmode)
	{
	  int i, len = XVECLEN (entry_parm, 0);

	  for (i = 0; i < len; i++)
	    if (XEXP (XVECEXP (entry_parm, 0, i), 0) != NULL_RTX
		&& GET_CODE (XEXP (XVECEXP (entry_parm, 0, i), 0)) == REG
		&& (GET_MODE (XEXP (XVECEXP (entry_parm, 0, i), 0))
		    == passed_mode)
		&& INTVAL (XEXP (XVECEXP (entry_parm, 0, i), 1)) == 0)
	      {
		entry_parm = XEXP (XVECEXP (entry_parm, 0, i), 0);
		DECL_INCOMING_RTL (parm) = entry_parm;
		break;
	      }
	}

      /* ENTRY_PARM is an RTX for the parameter as it arrives,
	 in the mode in which it arrives.
	 STACK_PARM is an RTX for a stack slot where the parameter can live
	 during the function (in case we want to put it there).
	 STACK_PARM is 0 if no stack slot was pushed for it.

	 Now output code if necessary to convert ENTRY_PARM to
	 the type in which this function declares it,
	 and store that result in an appropriate place,
	 which may be a pseudo reg, may be STACK_PARM,
	 or may be a local stack slot if STACK_PARM is 0.

	 Set DECL_RTL to that place.  */

      if (GET_CODE (entry_parm) == PARALLEL && nominal_mode != BLKmode
	  && XVECLEN (entry_parm, 0) > 1)
	{
	  /* Reconstitute objects the size of a register or larger using
	     register operations instead of the stack.  */
	  rtx parmreg = gen_reg_rtx (nominal_mode);

	  if (REG_P (parmreg))
	    {
	      unsigned int regno = REGNO (parmreg);

	      emit_group_store (parmreg, entry_parm, TREE_TYPE (parm),
				int_size_in_bytes (TREE_TYPE (parm)));
	      SET_DECL_RTL (parm, parmreg);

	      if (regno >= max_parm_reg)
		{
		  rtx *new;
		  int old_max_parm_reg = max_parm_reg;

		  /* It's slow to expand this one register at a time,
		     but it's also rare and we need max_parm_reg to be
		     precisely correct.  */
		  max_parm_reg = regno + 1;
		  new = ggc_realloc (parm_reg_stack_loc,
				     max_parm_reg * sizeof (rtx));
		  memset (new + old_max_parm_reg, 0,
			  (max_parm_reg - old_max_parm_reg) * sizeof (rtx));
		  parm_reg_stack_loc = new;
		  parm_reg_stack_loc[regno] = stack_parm;
		}
	    }
	}

      if (nominal_mode == BLKmode
#ifdef BLOCK_REG_PADDING
	  || (locate.where_pad == (BYTES_BIG_ENDIAN ? upward : downward)
	      && GET_MODE_SIZE (promoted_mode) < UNITS_PER_WORD)
#endif
	  || GET_CODE (entry_parm) == PARALLEL)
	{
	  /* If a BLKmode arrives in registers, copy it to a stack slot.
	     Handle calls that pass values in multiple non-contiguous
	     locations.  The Irix 6 ABI has examples of this.  */
	  if (GET_CODE (entry_parm) == REG
	      || GET_CODE (entry_parm) == PARALLEL)
	    {
	      int size = int_size_in_bytes (TREE_TYPE (parm));
	      int size_stored = CEIL_ROUND (size, UNITS_PER_WORD);
	      rtx mem;

	      /* Note that we will be storing an integral number of words.
		 So we have to be careful to ensure that we allocate an
		 integral number of words.  We do this below in the
		 assign_stack_local if space was not allocated in the argument
		 list.  If it was, this will not work if PARM_BOUNDARY is not
		 a multiple of BITS_PER_WORD.  It isn't clear how to fix this
		 if it becomes a problem.  Exception is when BLKmode arrives
		 with arguments not conforming to word_mode.  */

	      if (stack_parm == 0)
		{
		  stack_parm = assign_stack_local (BLKmode, size_stored, 0);
		  PUT_MODE (stack_parm, GET_MODE (entry_parm));
		  set_mem_attributes (stack_parm, parm, 1);
		}
	      else if (GET_CODE (entry_parm) == PARALLEL)
		;
	      else if (PARM_BOUNDARY % BITS_PER_WORD != 0)
		abort ();

	      mem = validize_mem (stack_parm);

	      /* Handle calls that pass values in multiple non-contiguous
		 locations.  The Irix 6 ABI has examples of this.  */
	      if (GET_CODE (entry_parm) == PARALLEL)
		emit_group_store (mem, entry_parm, TREE_TYPE (parm), size);

	      else if (size == 0)
		;

	      /* If SIZE is that of a mode no bigger than a word, just use
		 that mode's store operation.  */
	      else if (size <= UNITS_PER_WORD)
		{
		  enum machine_mode mode
		    = mode_for_size (size * BITS_PER_UNIT, MODE_INT, 0);

		  if (mode != BLKmode
#ifdef BLOCK_REG_PADDING
		      && (size == UNITS_PER_WORD
			  || (BLOCK_REG_PADDING (mode, TREE_TYPE (parm), 1)
			      != (BYTES_BIG_ENDIAN ? upward : downward)))
#endif
		      )
		    {
		      rtx reg = gen_rtx_REG (mode, REGNO (entry_parm));
		      emit_move_insn (change_address (mem, mode, 0), reg);
		    }

		  /* Blocks smaller than a word on a BYTES_BIG_ENDIAN
		     machine must be aligned to the left before storing
		     to memory.  Note that the previous test doesn't
		     handle all cases (e.g. SIZE == 3).  */
		  else if (size != UNITS_PER_WORD
#ifdef BLOCK_REG_PADDING
			   && (BLOCK_REG_PADDING (mode, TREE_TYPE (parm), 1)
			       == downward)
#else
			   && BYTES_BIG_ENDIAN
#endif
			   )
		    {
		      rtx tem, x;
		      int by = (UNITS_PER_WORD - size) * BITS_PER_UNIT;
		      rtx reg = gen_rtx_REG (word_mode, REGNO (entry_parm));

		      x = expand_binop (word_mode, ashl_optab, reg,
					GEN_INT (by), 0, 1, OPTAB_WIDEN);
		      tem = change_address (mem, word_mode, 0);
		      emit_move_insn (tem, x);
		    }
		  else
		    move_block_from_reg (REGNO (entry_parm), mem,
					 size_stored / UNITS_PER_WORD);
		}
	      else
		move_block_from_reg (REGNO (entry_parm), mem,
				     size_stored / UNITS_PER_WORD);
	    }
	  /* If parm is already bound to register pair, don't change 
	     this binding.  */
	  if (! DECL_RTL_SET_P (parm))
	    SET_DECL_RTL (parm, stack_parm);
	}
      else if (! ((! optimize
		   && ! DECL_REGISTER (parm))
		  || TREE_SIDE_EFFECTS (parm)
		  /* If -ffloat-store specified, don't put explicit
		     float variables into registers.  */
		  || (flag_float_store
		      && TREE_CODE (TREE_TYPE (parm)) == REAL_TYPE))
	       /* Always assign pseudo to structure return or item passed
		  by invisible reference.  */
	       || passed_pointer || parm == function_result_decl)
	{
	  /* Store the parm in a pseudoregister during the function, but we
	     may need to do it in a wider mode.  */

	  rtx parmreg;
	  unsigned int regno, regnoi = 0, regnor = 0;

	  unsignedp = TREE_UNSIGNED (TREE_TYPE (parm));

	  promoted_nominal_mode
	    = promote_mode (TREE_TYPE (parm), nominal_mode, &unsignedp, 0);

	  parmreg = gen_reg_rtx (promoted_nominal_mode);
	  mark_user_reg (parmreg);

	  /* If this was an item that we received a pointer to, set DECL_RTL
	     appropriately.  */
	  if (passed_pointer)
	    {
	      rtx x = gen_rtx_MEM (TYPE_MODE (TREE_TYPE (passed_type)),
				   parmreg);
	      set_mem_attributes (x, parm, 1);
	      SET_DECL_RTL (parm, x);
	    }
	  else
	    {
	      SET_DECL_RTL (parm, parmreg);
	      maybe_set_unchanging (DECL_RTL (parm), parm);
	    }

	  /* Copy the value into the register.  */
	  if (nominal_mode != passed_mode
	      || promoted_nominal_mode != promoted_mode)
	    {
	      int save_tree_used;
	      /* ENTRY_PARM has been converted to PROMOTED_MODE, its
		 mode, by the caller.  We now have to convert it to
		 NOMINAL_MODE, if different.  However, PARMREG may be in
		 a different mode than NOMINAL_MODE if it is being stored
		 promoted.

		 If ENTRY_PARM is a hard register, it might be in a register
		 not valid for operating in its mode (e.g., an odd-numbered
		 register for a DFmode).  In that case, moves are the only
		 thing valid, so we can't do a convert from there.  This
		 occurs when the calling sequence allow such misaligned
		 usages.

		 In addition, the conversion may involve a call, which could
		 clobber parameters which haven't been copied to pseudo
		 registers yet.  Therefore, we must first copy the parm to
		 a pseudo reg here, and save the conversion until after all
		 parameters have been moved.  */

	      rtx tempreg = gen_reg_rtx (GET_MODE (entry_parm));

	      emit_move_insn (tempreg, validize_mem (entry_parm));

	      push_to_sequence (conversion_insns);
	      tempreg = convert_to_mode (nominal_mode, tempreg, unsignedp);

	      if (GET_CODE (tempreg) == SUBREG
		  && GET_MODE (tempreg) == nominal_mode
		  && GET_CODE (SUBREG_REG (tempreg)) == REG
		  && nominal_mode == passed_mode
		  && GET_MODE (SUBREG_REG (tempreg)) == GET_MODE (entry_parm)
		  && GET_MODE_SIZE (GET_MODE (tempreg))
		     < GET_MODE_SIZE (GET_MODE (entry_parm)))
		{
		  /* The argument is already sign/zero extended, so note it
		     into the subreg.  */
		  SUBREG_PROMOTED_VAR_P (tempreg) = 1;
		  SUBREG_PROMOTED_UNSIGNED_SET (tempreg, unsignedp);
		}

	      /* TREE_USED gets set erroneously during expand_assignment.  */
	      save_tree_used = TREE_USED (parm);
	      expand_assignment (parm,
				 make_tree (nominal_type, tempreg), 0);
	      TREE_USED (parm) = save_tree_used;
	      conversion_insns = get_insns ();
	      did_conversion = 1;
	      end_sequence ();
	    }
	  else
	    emit_move_insn (parmreg, validize_mem (entry_parm));

	  /* If we were passed a pointer but the actual value
	     can safely live in a register, put it in one.  */
	  if (passed_pointer && TYPE_MODE (TREE_TYPE (parm)) != BLKmode
	      /* If by-reference argument was promoted, demote it.  */
	      && (TYPE_MODE (TREE_TYPE (parm)) != GET_MODE (DECL_RTL (parm))
		  || ! ((! optimize
			 && ! DECL_REGISTER (parm))
			|| TREE_SIDE_EFFECTS (parm)
			/* If -ffloat-store specified, don't put explicit
			   float variables into registers.  */
			|| (flag_float_store
			    && TREE_CODE (TREE_TYPE (parm)) == REAL_TYPE))))
	    {
	      /* We can't use nominal_mode, because it will have been set to
		 Pmode above.  We must use the actual mode of the parm.  */
	      parmreg = gen_reg_rtx (TYPE_MODE (TREE_TYPE (parm)));
	      mark_user_reg (parmreg);
	      if (GET_MODE (parmreg) != GET_MODE (DECL_RTL (parm)))
		{
		  rtx tempreg = gen_reg_rtx (GET_MODE (DECL_RTL (parm)));
		  int unsigned_p = TREE_UNSIGNED (TREE_TYPE (parm));
		  push_to_sequence (conversion_insns);
		  emit_move_insn (tempreg, DECL_RTL (parm));
		  SET_DECL_RTL (parm,
				convert_to_mode (GET_MODE (parmreg),
						 tempreg,
						 unsigned_p));
		  emit_move_insn (parmreg, DECL_RTL (parm));
		  conversion_insns = get_insns();
		  did_conversion = 1;
		  end_sequence ();
		}
	      else
		emit_move_insn (parmreg, DECL_RTL (parm));
	      SET_DECL_RTL (parm, parmreg);
	      /* STACK_PARM is the pointer, not the parm, and PARMREG is
		 now the parm.  */
	      stack_parm = 0;
	    }
#ifdef FUNCTION_ARG_CALLEE_COPIES
	  /* If we are passed an arg by reference and it is our responsibility
	     to make a copy, do it now.
	     PASSED_TYPE and PASSED mode now refer to the pointer, not the
	     original argument, so we must recreate them in the call to
	     FUNCTION_ARG_CALLEE_COPIES.  */
	  /* ??? Later add code to handle the case that if the argument isn't
	     modified, don't do the copy.  */

	  else if (passed_pointer
		   && FUNCTION_ARG_CALLEE_COPIES (args_so_far,
						  TYPE_MODE (TREE_TYPE (passed_type)),
						  TREE_TYPE (passed_type),
						  named_arg)
		   && ! TREE_ADDRESSABLE (TREE_TYPE (passed_type)))
	    {
	      rtx copy;
	      tree type = TREE_TYPE (passed_type);

	      /* This sequence may involve a library call perhaps clobbering
		 registers that haven't been copied to pseudos yet.  */

	      push_to_sequence (conversion_insns);

	      if (!COMPLETE_TYPE_P (type)
		  || TREE_CODE (TYPE_SIZE (type)) != INTEGER_CST)
		/* This is a variable sized object.  */
		copy = gen_rtx_MEM (BLKmode,
				    allocate_dynamic_stack_space
				    (expr_size (parm), NULL_RTX,
				     TYPE_ALIGN (type)));
	      else
		copy = assign_stack_temp (TYPE_MODE (type),
					  int_size_in_bytes (type), 1);
	      set_mem_attributes (copy, parm, 1);

	      store_expr (parm, copy, 0);
	      emit_move_insn (parmreg, XEXP (copy, 0));
	      conversion_insns = get_insns ();
	      did_conversion = 1;
	      end_sequence ();
	    }
#endif /* FUNCTION_ARG_CALLEE_COPIES */

	  /* In any case, record the parm's desired stack location
	     in case we later discover it must live in the stack.

	     If it is a COMPLEX value, store the stack location for both
	     halves.  */

	  if (GET_CODE (parmreg) == CONCAT)
	    regno = MAX (REGNO (XEXP (parmreg, 0)), REGNO (XEXP (parmreg, 1)));
	  else
	    regno = REGNO (parmreg);

	  if (regno >= max_parm_reg)
	    {
	      rtx *new;
	      int old_max_parm_reg = max_parm_reg;

	      /* It's slow to expand this one register at a time,
		 but it's also rare and we need max_parm_reg to be
		 precisely correct.  */
	      max_parm_reg = regno + 1;
	      new = ggc_realloc (parm_reg_stack_loc,
				 max_parm_reg * sizeof (rtx));
	      memset (new + old_max_parm_reg, 0,
		      (max_parm_reg - old_max_parm_reg) * sizeof (rtx));
	      parm_reg_stack_loc = new;
	    }

	  if (GET_CODE (parmreg) == CONCAT)
	    {
	      enum machine_mode submode = GET_MODE (XEXP (parmreg, 0));

	      regnor = REGNO (gen_realpart (submode, parmreg));
	      regnoi = REGNO (gen_imagpart (submode, parmreg));

	      if (stack_parm != 0)
		{
		  parm_reg_stack_loc[regnor]
		    = gen_realpart (submode, stack_parm);
		  parm_reg_stack_loc[regnoi]
		    = gen_imagpart (submode, stack_parm);
		}
	      else
		{
		  parm_reg_stack_loc[regnor] = 0;
		  parm_reg_stack_loc[regnoi] = 0;
		}
	    }
	  else
	    parm_reg_stack_loc[REGNO (parmreg)] = stack_parm;

	  /* Mark the register as eliminable if we did no conversion
	     and it was copied from memory at a fixed offset,
	     and the arg pointer was not copied to a pseudo-reg.
	     If the arg pointer is a pseudo reg or the offset formed
	     an invalid address, such memory-equivalences
	     as we make here would screw up life analysis for it.  */
	  if (nominal_mode == passed_mode
	      && ! did_conversion
	      && stack_parm != 0
	      && GET_CODE (stack_parm) == MEM
	      && locate.offset.var == 0
	      && reg_mentioned_p (virtual_incoming_args_rtx,
				  XEXP (stack_parm, 0)))
	    {
	      rtx linsn = get_last_insn ();
	      rtx sinsn, set;

	      /* Mark complex types separately.  */
	      if (GET_CODE (parmreg) == CONCAT)
		/* Scan backwards for the set of the real and
		   imaginary parts.  */
		for (sinsn = linsn; sinsn != 0;
		     sinsn = prev_nonnote_insn (sinsn))
		  {
		    set = single_set (sinsn);
		    if (set != 0
			&& SET_DEST (set) == regno_reg_rtx [regnoi])
		      REG_NOTES (sinsn)
			= gen_rtx_EXPR_LIST (REG_EQUIV,
					     parm_reg_stack_loc[regnoi],
					     REG_NOTES (sinsn));
		    else if (set != 0
			     && SET_DEST (set) == regno_reg_rtx [regnor])
		      REG_NOTES (sinsn)
			= gen_rtx_EXPR_LIST (REG_EQUIV,
					     parm_reg_stack_loc[regnor],
					     REG_NOTES (sinsn));
		  }
	      else if ((set = single_set (linsn)) != 0
		       && SET_DEST (set) == parmreg)
		REG_NOTES (linsn)
		  = gen_rtx_EXPR_LIST (REG_EQUIV,
				       stack_parm, REG_NOTES (linsn));
	    }

	  /* For pointer data type, suggest pointer register.  */
	  if (POINTER_TYPE_P (TREE_TYPE (parm)))
	    mark_reg_pointer (parmreg,
			      TYPE_ALIGN (TREE_TYPE (TREE_TYPE (parm))));

	  /* If something wants our address, try to use ADDRESSOF.  */
	  if (TREE_ADDRESSABLE (parm))
	    {
	      /* If we end up putting something into the stack,
		 fixup_var_refs_insns will need to make a pass over
		 all the instructions.  It looks through the pending
		 sequences -- but it can't see the ones in the
		 CONVERSION_INSNS, if they're not on the sequence
		 stack.  So, we go back to that sequence, just so that
		 the fixups will happen.  */
	      push_to_sequence (conversion_insns);
	      put_var_into_stack (parm, /*rescan=*/true);
	      conversion_insns = get_insns ();
	      end_sequence ();
	    }
	}
      else
	{
	  /* Value must be stored in the stack slot STACK_PARM
	     during function execution.  */

	  if (promoted_mode != nominal_mode)
	    {
	      /* Conversion is required.  */
	      rtx tempreg = gen_reg_rtx (GET_MODE (entry_parm));

	      emit_move_insn (tempreg, validize_mem (entry_parm));

	      push_to_sequence (conversion_insns);
	      entry_parm = convert_to_mode (nominal_mode, tempreg,
					    TREE_UNSIGNED (TREE_TYPE (parm)));
	      if (stack_parm)
		/* ??? This may need a big-endian conversion on sparc64.  */
		stack_parm = adjust_address (stack_parm, nominal_mode, 0);

	      conversion_insns = get_insns ();
	      did_conversion = 1;
	      end_sequence ();
	    }

	  if (entry_parm != stack_parm)
	    {
	      if (stack_parm == 0)
		{
		  stack_parm
		    = assign_stack_local (GET_MODE (entry_parm),
					  GET_MODE_SIZE (GET_MODE (entry_parm)),
					  0);
		  set_mem_attributes (stack_parm, parm, 1);
		}

	      if (promoted_mode != nominal_mode)
		{
		  push_to_sequence (conversion_insns);
		  emit_move_insn (validize_mem (stack_parm),
				  validize_mem (entry_parm));
		  conversion_insns = get_insns ();
		  end_sequence ();
		}
	      else
		emit_move_insn (validize_mem (stack_parm),
				validize_mem (entry_parm));
	    }

	  SET_DECL_RTL (parm, stack_parm);
	}
    }

  if (targetm.calls.split_complex_arg && fnargs != orig_fnargs)
    {
      for (parm = orig_fnargs; parm; parm = TREE_CHAIN (parm))
	{
	  if (TREE_CODE (TREE_TYPE (parm)) == COMPLEX_TYPE
	      && targetm.calls.split_complex_arg (TREE_TYPE (parm)))
	    {
	      rtx tmp, real, imag;
	      enum machine_mode inner = GET_MODE_INNER (DECL_MODE (parm));

	      real = DECL_RTL (fnargs);
	      imag = DECL_RTL (TREE_CHAIN (fnargs));
	      if (inner != GET_MODE (real))
		{
		  real = gen_lowpart_SUBREG (inner, real);
		  imag = gen_lowpart_SUBREG (inner, imag);
		}
	      tmp = gen_rtx_CONCAT (DECL_MODE (parm), real, imag);
	      SET_DECL_RTL (parm, tmp);

	      real = DECL_INCOMING_RTL (fnargs);
	      imag = DECL_INCOMING_RTL (TREE_CHAIN (fnargs));
	      if (inner != GET_MODE (real))
		{
		  real = gen_lowpart_SUBREG (inner, real);
		  imag = gen_lowpart_SUBREG (inner, imag);
		}
	      tmp = gen_rtx_CONCAT (DECL_MODE (parm), real, imag);
	      DECL_INCOMING_RTL (parm) = tmp;
	      fnargs = TREE_CHAIN (fnargs);
	    }
	  else
	    {
	      SET_DECL_RTL (parm, DECL_RTL (fnargs));
	      DECL_INCOMING_RTL (parm) = DECL_INCOMING_RTL (fnargs);
	    }
	  fnargs = TREE_CHAIN (fnargs);
	}
    }

  /* Output all parameter conversion instructions (possibly including calls)
     now that all parameters have been copied out of hard registers.  */
  emit_insn (conversion_insns);

  /* If we are receiving a struct value address as the first argument, set up
     the RTL for the function result. As this might require code to convert
     the transmitted address to Pmode, we do this here to ensure that possible
     preliminary conversions of the address have been emitted already.  */
  if (function_result_decl)
    {
      tree result = DECL_RESULT (fndecl);
      rtx addr = DECL_RTL (function_result_decl);
      rtx x;

      addr = convert_memory_address (Pmode, addr);
      x = gen_rtx_MEM (DECL_MODE (result), addr);
      set_mem_attributes (x, result, 1);
      SET_DECL_RTL (result, x);
    }

  last_parm_insn = get_last_insn ();

  current_function_args_size = stack_args_size.constant;

  /* Adjust function incoming argument size for alignment and
     minimum length.  */

#ifdef REG_PARM_STACK_SPACE
#ifndef MAYBE_REG_PARM_STACK_SPACE
  current_function_args_size = MAX (current_function_args_size,
				    REG_PARM_STACK_SPACE (fndecl));
#endif
#endif

  current_function_args_size
    = ((current_function_args_size + STACK_BYTES - 1)
       / STACK_BYTES) * STACK_BYTES;

#ifdef ARGS_GROW_DOWNWARD
  current_function_arg_offset_rtx
    = (stack_args_size.var == 0 ? GEN_INT (-stack_args_size.constant)
       : expand_expr (size_diffop (stack_args_size.var,
				   size_int (-stack_args_size.constant)),
		      NULL_RTX, VOIDmode, 0));
#else
  current_function_arg_offset_rtx = ARGS_SIZE_RTX (stack_args_size);
#endif

  /* See how many bytes, if any, of its args a function should try to pop
     on return.  */

  current_function_pops_args = RETURN_POPS_ARGS (fndecl, TREE_TYPE (fndecl),
						 current_function_args_size);

  /* For stdarg.h function, save info about
     regs and stack space used by the named args.  */

  current_function_args_info = args_so_far;

  /* Set the rtx used for the function return value.  Put this in its
     own variable so any optimizers that need this information don't have
     to include tree.h.  Do this here so it gets done when an inlined
     function gets output.  */

  current_function_return_rtx
    = (DECL_RTL_SET_P (DECL_RESULT (fndecl))
       ? DECL_RTL (DECL_RESULT (fndecl)) : NULL_RTX);

  /* If scalar return value was computed in a pseudo-reg, or was a named
     return value that got dumped to the stack, copy that to the hard
     return register.  */
  if (DECL_RTL_SET_P (DECL_RESULT (fndecl)))
    {
      tree decl_result = DECL_RESULT (fndecl);
      rtx decl_rtl = DECL_RTL (decl_result);

      if (REG_P (decl_rtl)
	  ? REGNO (decl_rtl) >= FIRST_PSEUDO_REGISTER
	  : DECL_REGISTER (decl_result))
	{
	  rtx real_decl_rtl;

#ifdef FUNCTION_OUTGOING_VALUE
	  real_decl_rtl = FUNCTION_OUTGOING_VALUE (TREE_TYPE (decl_result),
						   fndecl);
#else
	  real_decl_rtl = FUNCTION_VALUE (TREE_TYPE (decl_result),
					  fndecl);
#endif
	  REG_FUNCTION_VALUE_P (real_decl_rtl) = 1;
	  /* The delay slot scheduler assumes that current_function_return_rtx
	     holds the hard register containing the return value, not a
	     temporary pseudo.  */
	  current_function_return_rtx = real_decl_rtl;
	}
    }
}

/* If ARGS contains entries with complex types, split the entry into two
   entries of the component type.  Return a new list of substitutions are
   needed, else the old list.  */

static tree
split_complex_args (tree args)
{
  tree p;

  /* Before allocating memory, check for the common case of no complex.  */
  for (p = args; p; p = TREE_CHAIN (p))
    {
      tree type = TREE_TYPE (p);
      if (TREE_CODE (type) == COMPLEX_TYPE
	  && targetm.calls.split_complex_arg (type))
        goto found;
    }
  return args;

 found:
  args = copy_list (args);

  for (p = args; p; p = TREE_CHAIN (p))
    {
      tree type = TREE_TYPE (p);
      if (TREE_CODE (type) == COMPLEX_TYPE
	  && targetm.calls.split_complex_arg (type))
	{
	  tree decl;
	  tree subtype = TREE_TYPE (type);

	  /* Rewrite the PARM_DECL's type with its component.  */
	  TREE_TYPE (p) = subtype;
	  DECL_ARG_TYPE (p) = TREE_TYPE (DECL_ARG_TYPE (p));
	  DECL_MODE (p) = VOIDmode;
	  DECL_SIZE (p) = NULL;
	  DECL_SIZE_UNIT (p) = NULL;
	  layout_decl (p, 0);

	  /* Build a second synthetic decl.  */
	  decl = build_decl (PARM_DECL, NULL_TREE, subtype);
	  DECL_ARG_TYPE (decl) = DECL_ARG_TYPE (p);
	  layout_decl (decl, 0);

	  /* Splice it in; skip the new decl.  */
	  TREE_CHAIN (decl) = TREE_CHAIN (p);
	  TREE_CHAIN (p) = decl;
	  p = decl;
	}
    }

  return args;
}

/* Indicate whether REGNO is an incoming argument to the current function
   that was promoted to a wider mode.  If so, return the RTX for the
   register (to get its mode).  PMODE and PUNSIGNEDP are set to the mode
   that REGNO is promoted from and whether the promotion was signed or
   unsigned.  */

rtx
promoted_input_arg (unsigned int regno, enum machine_mode *pmode, int *punsignedp)
{
  tree arg;

  for (arg = DECL_ARGUMENTS (current_function_decl); arg;
       arg = TREE_CHAIN (arg))
    if (GET_CODE (DECL_INCOMING_RTL (arg)) == REG
	&& REGNO (DECL_INCOMING_RTL (arg)) == regno
	&& TYPE_MODE (DECL_ARG_TYPE (arg)) == TYPE_MODE (TREE_TYPE (arg)))
      {
	enum machine_mode mode = TYPE_MODE (TREE_TYPE (arg));
	int unsignedp = TREE_UNSIGNED (TREE_TYPE (arg));

	mode = promote_mode (TREE_TYPE (arg), mode, &unsignedp, 1);
	if (mode == GET_MODE (DECL_INCOMING_RTL (arg))
	    && mode != DECL_MODE (arg))
	  {
	    *pmode = DECL_MODE (arg);
	    *punsignedp = unsignedp;
	    return DECL_INCOMING_RTL (arg);
	  }
      }

  return 0;
}


/* Compute the size and offset from the start of the stacked arguments for a
   parm passed in mode PASSED_MODE and with type TYPE.

   INITIAL_OFFSET_PTR points to the current offset into the stacked
   arguments.

   The starting offset and size for this parm are returned in
   LOCATE->OFFSET and LOCATE->SIZE, respectively.  When IN_REGS is
   nonzero, the offset is that of stack slot, which is returned in
   LOCATE->SLOT_OFFSET.  LOCATE->ALIGNMENT_PAD is the amount of
   padding required from the initial offset ptr to the stack slot.

   IN_REGS is nonzero if the argument will be passed in registers.  It will
   never be set if REG_PARM_STACK_SPACE is not defined.

   FNDECL is the function in which the argument was defined.

   There are two types of rounding that are done.  The first, controlled by
   FUNCTION_ARG_BOUNDARY, forces the offset from the start of the argument
   list to be aligned to the specific boundary (in bits).  This rounding
   affects the initial and starting offsets, but not the argument size.

   The second, controlled by FUNCTION_ARG_PADDING and PARM_BOUNDARY,
   optionally rounds the size of the parm to PARM_BOUNDARY.  The
   initial offset is not affected by this rounding, while the size always
   is and the starting offset may be.  */

/*  LOCATE->OFFSET will be negative for ARGS_GROW_DOWNWARD case;
    INITIAL_OFFSET_PTR is positive because locate_and_pad_parm's
    callers pass in the total size of args so far as
    INITIAL_OFFSET_PTR.  LOCATE->SIZE is always positive.  */

void
locate_and_pad_parm (enum machine_mode passed_mode, tree type, int in_regs,
		     int partial, tree fndecl ATTRIBUTE_UNUSED,
		     struct args_size *initial_offset_ptr,
		     struct locate_and_pad_arg_data *locate)
{
  tree sizetree;
  enum direction where_pad;
  int boundary;
  int reg_parm_stack_space = 0;
  int part_size_in_regs;

#ifdef REG_PARM_STACK_SPACE
#ifdef MAYBE_REG_PARM_STACK_SPACE
  reg_parm_stack_space = MAYBE_REG_PARM_STACK_SPACE;
#else
  reg_parm_stack_space = REG_PARM_STACK_SPACE (fndecl);
#endif

  /* If we have found a stack parm before we reach the end of the
     area reserved for registers, skip that area.  */
  if (! in_regs)
    {
      if (reg_parm_stack_space > 0)
	{
	  if (initial_offset_ptr->var)
	    {
	      initial_offset_ptr->var
		= size_binop (MAX_EXPR, ARGS_SIZE_TREE (*initial_offset_ptr),
			      ssize_int (reg_parm_stack_space));
	      initial_offset_ptr->constant = 0;
	    }
	  else if (initial_offset_ptr->constant < reg_parm_stack_space)
	    initial_offset_ptr->constant = reg_parm_stack_space;
	}
    }
#endif /* REG_PARM_STACK_SPACE */

  part_size_in_regs = 0;
  if (reg_parm_stack_space == 0)
    part_size_in_regs = ((partial * UNITS_PER_WORD)
			 / (PARM_BOUNDARY / BITS_PER_UNIT)
			 * (PARM_BOUNDARY / BITS_PER_UNIT));

  sizetree
    = type ? size_in_bytes (type) : size_int (GET_MODE_SIZE (passed_mode));
  where_pad = FUNCTION_ARG_PADDING (passed_mode, type);
  boundary = FUNCTION_ARG_BOUNDARY (passed_mode, type);
  locate->where_pad = where_pad;

#ifdef ARGS_GROW_DOWNWARD
  locate->slot_offset.constant = -initial_offset_ptr->constant;
  if (initial_offset_ptr->var)
    locate->slot_offset.var = size_binop (MINUS_EXPR, ssize_int (0),
					  initial_offset_ptr->var);

  {
    tree s2 = sizetree;
    if (where_pad != none
	&& (!host_integerp (sizetree, 1)
	    || (tree_low_cst (sizetree, 1) * BITS_PER_UNIT) % PARM_BOUNDARY))
      s2 = round_up (s2, PARM_BOUNDARY / BITS_PER_UNIT);
    SUB_PARM_SIZE (locate->slot_offset, s2);
  }

  locate->slot_offset.constant += part_size_in_regs;

  if (!in_regs
#ifdef REG_PARM_STACK_SPACE
      || REG_PARM_STACK_SPACE (fndecl) > 0
#endif
     )
    pad_to_arg_alignment (&locate->slot_offset, boundary,
			  &locate->alignment_pad);

  locate->size.constant = (-initial_offset_ptr->constant
			   - locate->slot_offset.constant);
  if (initial_offset_ptr->var)
    locate->size.var = size_binop (MINUS_EXPR,
				   size_binop (MINUS_EXPR,
					       ssize_int (0),
					       initial_offset_ptr->var),
				   locate->slot_offset.var);

  /* Pad_below needs the pre-rounded size to know how much to pad
     below.  */
  locate->offset = locate->slot_offset;
  if (where_pad == downward)
    pad_below (&locate->offset, passed_mode, sizetree);

#else /* !ARGS_GROW_DOWNWARD */
  if (!in_regs
#ifdef REG_PARM_STACK_SPACE
      || REG_PARM_STACK_SPACE (fndecl) > 0
#endif
      )
    pad_to_arg_alignment (initial_offset_ptr, boundary,
			  &locate->alignment_pad);
  locate->slot_offset = *initial_offset_ptr;

#ifdef PUSH_ROUNDING
  if (passed_mode != BLKmode)
    sizetree = size_int (PUSH_ROUNDING (TREE_INT_CST_LOW (sizetree)));
#endif

  /* Pad_below needs the pre-rounded size to know how much to pad below
     so this must be done before rounding up.  */
  locate->offset = locate->slot_offset;
  if (where_pad == downward)
    pad_below (&locate->offset, passed_mode, sizetree);

  if (where_pad != none
      && (!host_integerp (sizetree, 1)
	  || (tree_low_cst (sizetree, 1) * BITS_PER_UNIT) % PARM_BOUNDARY))
    sizetree = round_up (sizetree, PARM_BOUNDARY / BITS_PER_UNIT);

  ADD_PARM_SIZE (locate->size, sizetree);

  locate->size.constant -= part_size_in_regs;
#endif /* ARGS_GROW_DOWNWARD */
}

/* Round the stack offset in *OFFSET_PTR up to a multiple of BOUNDARY.
   BOUNDARY is measured in bits, but must be a multiple of a storage unit.  */

static void
pad_to_arg_alignment (struct args_size *offset_ptr, int boundary,
		      struct args_size *alignment_pad)
{
  tree save_var = NULL_TREE;
  HOST_WIDE_INT save_constant = 0;
  int boundary_in_bytes = boundary / BITS_PER_UNIT;
  HOST_WIDE_INT sp_offset = STACK_POINTER_OFFSET;

#ifdef SPARC_STACK_BOUNDARY_HACK
  /* The sparc port has a bug.  It sometimes claims a STACK_BOUNDARY
     higher than the real alignment of %sp.  However, when it does this,
     the alignment of %sp+STACK_POINTER_OFFSET will be STACK_BOUNDARY.
     This is a temporary hack while the sparc port is fixed.  */
  if (SPARC_STACK_BOUNDARY_HACK)
    sp_offset = 0;
#endif

  if (boundary > PARM_BOUNDARY && boundary > STACK_BOUNDARY)
    {
      save_var = offset_ptr->var;
      save_constant = offset_ptr->constant;
    }

  alignment_pad->var = NULL_TREE;
  alignment_pad->constant = 0;

  if (boundary > BITS_PER_UNIT)
    {
      if (offset_ptr->var)
	{
	  tree sp_offset_tree = ssize_int (sp_offset);
	  tree offset = size_binop (PLUS_EXPR,
				    ARGS_SIZE_TREE (*offset_ptr),
				    sp_offset_tree);
#ifdef ARGS_GROW_DOWNWARD
	  tree rounded = round_down (offset, boundary / BITS_PER_UNIT);
#else
	  tree rounded = round_up   (offset, boundary / BITS_PER_UNIT);
#endif

	  offset_ptr->var = size_binop (MINUS_EXPR, rounded, sp_offset_tree);
	  /* ARGS_SIZE_TREE includes constant term.  */
	  offset_ptr->constant = 0;
	  if (boundary > PARM_BOUNDARY && boundary > STACK_BOUNDARY)
	    alignment_pad->var = size_binop (MINUS_EXPR, offset_ptr->var,
					     save_var);
	}
      else
	{
	  offset_ptr->constant = -sp_offset +
#ifdef ARGS_GROW_DOWNWARD
	    FLOOR_ROUND (offset_ptr->constant + sp_offset, boundary_in_bytes);
#else
	    CEIL_ROUND (offset_ptr->constant + sp_offset, boundary_in_bytes);
#endif
	    if (boundary > PARM_BOUNDARY && boundary > STACK_BOUNDARY)
	      alignment_pad->constant = offset_ptr->constant - save_constant;
	}
    }
}

static void
pad_below (struct args_size *offset_ptr, enum machine_mode passed_mode, tree sizetree)
{
  if (passed_mode != BLKmode)
    {
      if (GET_MODE_BITSIZE (passed_mode) % PARM_BOUNDARY)
	offset_ptr->constant
	  += (((GET_MODE_BITSIZE (passed_mode) + PARM_BOUNDARY - 1)
	       / PARM_BOUNDARY * PARM_BOUNDARY / BITS_PER_UNIT)
	      - GET_MODE_SIZE (passed_mode));
    }
  else
    {
      if (TREE_CODE (sizetree) != INTEGER_CST
	  || (TREE_INT_CST_LOW (sizetree) * BITS_PER_UNIT) % PARM_BOUNDARY)
	{
	  /* Round the size up to multiple of PARM_BOUNDARY bits.  */
	  tree s2 = round_up (sizetree, PARM_BOUNDARY / BITS_PER_UNIT);
	  /* Add it in.  */
	  ADD_PARM_SIZE (*offset_ptr, s2);
	  SUB_PARM_SIZE (*offset_ptr, sizetree);
	}
    }
}

/* Walk the tree of blocks describing the binding levels within a function
   and warn about uninitialized variables.
   This is done after calling flow_analysis and before global_alloc
   clobbers the pseudo-regs to hard regs.  */

void
uninitialized_vars_warning (tree block)
{
  tree decl, sub;
  for (decl = BLOCK_VARS (block); decl; decl = TREE_CHAIN (decl))
    {
      if (warn_uninitialized
	  && TREE_CODE (decl) == VAR_DECL
	  /* These warnings are unreliable for and aggregates
	     because assigning the fields one by one can fail to convince
	     flow.c that the entire aggregate was initialized.
	     Unions are troublesome because members may be shorter.  */
	  && ! AGGREGATE_TYPE_P (TREE_TYPE (decl))
	  && DECL_RTL_SET_P (decl)
	  && GET_CODE (DECL_RTL (decl)) == REG
	  /* Global optimizations can make it difficult to determine if a
	     particular variable has been initialized.  However, a VAR_DECL
	     with a nonzero DECL_INITIAL had an initializer, so do not
	     claim it is potentially uninitialized.

	     When the DECL_INITIAL is NULL call the language hook to tell us
	     if we want to warn.  */
	  && (DECL_INITIAL (decl) == NULL_TREE || lang_hooks.decl_uninit (decl))
	  && regno_uninitialized (REGNO (DECL_RTL (decl))))
	warning ("%J'%D' might be used uninitialized in this function",
		 decl, decl);
      if (extra_warnings
	  && TREE_CODE (decl) == VAR_DECL
	  && DECL_RTL_SET_P (decl)
	  && GET_CODE (DECL_RTL (decl)) == REG
	  && regno_clobbered_at_setjmp (REGNO (DECL_RTL (decl))))
	warning ("%Jvariable '%D' might be clobbered by `longjmp' or `vfork'",
		 decl, decl);
    }
  for (sub = BLOCK_SUBBLOCKS (block); sub; sub = TREE_CHAIN (sub))
    uninitialized_vars_warning (sub);
}

/* Do the appropriate part of uninitialized_vars_warning
   but for arguments instead of local variables.  */

void
setjmp_args_warning (void)
{
  tree decl;
  for (decl = DECL_ARGUMENTS (current_function_decl);
       decl; decl = TREE_CHAIN (decl))
    if (DECL_RTL (decl) != 0
	&& GET_CODE (DECL_RTL (decl)) == REG
	&& regno_clobbered_at_setjmp (REGNO (DECL_RTL (decl))))
      warning ("%Jargument '%D' might be clobbered by `longjmp' or `vfork'",
	       decl, decl);
}

/* If this function call setjmp, put all vars into the stack
   unless they were declared `register'.  */

void
setjmp_protect (tree block)
{
  tree decl, sub;
  for (decl = BLOCK_VARS (block); decl; decl = TREE_CHAIN (decl))
    if ((TREE_CODE (decl) == VAR_DECL
	 || TREE_CODE (decl) == PARM_DECL)
	&& DECL_RTL (decl) != 0
	&& (GET_CODE (DECL_RTL (decl)) == REG
	    || (GET_CODE (DECL_RTL (decl)) == MEM
		&& GET_CODE (XEXP (DECL_RTL (decl), 0)) == ADDRESSOF))
	/* If this variable came from an inline function, it must be
	   that its life doesn't overlap the setjmp.  If there was a
	   setjmp in the function, it would already be in memory.  We
	   must exclude such variable because their DECL_RTL might be
	   set to strange things such as virtual_stack_vars_rtx.  */
	&& ! DECL_FROM_INLINE (decl)
	&& (
#ifdef NON_SAVING_SETJMP
	    /* If longjmp doesn't restore the registers,
	       don't put anything in them.  */
	    NON_SAVING_SETJMP
	    ||
#endif
	    ! DECL_REGISTER (decl)))
      put_var_into_stack (decl, /*rescan=*/true);
  for (sub = BLOCK_SUBBLOCKS (block); sub; sub = TREE_CHAIN (sub))
    setjmp_protect (sub);
}

/* Like the previous function, but for args instead of local variables.  */

void
setjmp_protect_args (void)
{
  tree decl;
  for (decl = DECL_ARGUMENTS (current_function_decl);
       decl; decl = TREE_CHAIN (decl))
    if ((TREE_CODE (decl) == VAR_DECL
	 || TREE_CODE (decl) == PARM_DECL)
	&& DECL_RTL (decl) != 0
	&& (GET_CODE (DECL_RTL (decl)) == REG
	    || (GET_CODE (DECL_RTL (decl)) == MEM
		&& GET_CODE (XEXP (DECL_RTL (decl), 0)) == ADDRESSOF))
	&& (
	    /* If longjmp doesn't restore the registers,
	       don't put anything in them.  */
#ifdef NON_SAVING_SETJMP
	    NON_SAVING_SETJMP
	    ||
#endif
	    ! DECL_REGISTER (decl)))
      put_var_into_stack (decl, /*rescan=*/true);
}

/* Return the context-pointer register corresponding to DECL,
   or 0 if it does not need one.  */

rtx
lookup_static_chain (tree decl)
{
  tree context = decl_function_context (decl);
  tree link;

  if (context == 0
      || (TREE_CODE (decl) == FUNCTION_DECL && DECL_NO_STATIC_CHAIN (decl)))
    return 0;

  /* We treat inline_function_decl as an alias for the current function
     because that is the inline function whose vars, types, etc.
     are being merged into the current function.
     See expand_inline_function.  */
  if (context == current_function_decl || context == inline_function_decl)
    return virtual_stack_vars_rtx;

  for (link = context_display; link; link = TREE_CHAIN (link))
    if (TREE_PURPOSE (link) == context)
      return RTL_EXPR_RTL (TREE_VALUE (link));

  abort ();
}

/* Convert a stack slot address ADDR for variable VAR
   (from a containing function)
   into an address valid in this function (using a static chain).  */

rtx
fix_lexical_addr (rtx addr, tree var)
{
  rtx basereg;
  HOST_WIDE_INT displacement;
  tree context = decl_function_context (var);
  struct function *fp;
  rtx base = 0;

  /* If this is the present function, we need not do anything.  */
  if (context == current_function_decl || context == inline_function_decl)
    return addr;

  fp = find_function_data (context);

  if (GET_CODE (addr) == ADDRESSOF && GET_CODE (XEXP (addr, 0)) == MEM)
    addr = XEXP (XEXP (addr, 0), 0);

  /* Decode given address as base reg plus displacement.  */
  if (GET_CODE (addr) == REG)
    basereg = addr, displacement = 0;
  else if (GET_CODE (addr) == PLUS && GET_CODE (XEXP (addr, 1)) == CONST_INT)
    basereg = XEXP (addr, 0), displacement = INTVAL (XEXP (addr, 1));
  else
    abort ();

  /* We accept vars reached via the containing function's
     incoming arg pointer and via its stack variables pointer.  */
  if (basereg == fp->internal_arg_pointer)
    {
      /* If reached via arg pointer, get the arg pointer value
	 out of that function's stack frame.

	 There are two cases:  If a separate ap is needed, allocate a
	 slot in the outer function for it and dereference it that way.
	 This is correct even if the real ap is actually a pseudo.
	 Otherwise, just adjust the offset from the frame pointer to
	 compensate.  */

#ifdef NEED_SEPARATE_AP
      rtx addr;

      addr = get_arg_pointer_save_area (fp);
      addr = fix_lexical_addr (XEXP (addr, 0), var);
      addr = memory_address (Pmode, addr);

      base = gen_rtx_MEM (Pmode, addr);
      set_mem_alias_set (base, get_frame_alias_set ());
      base = copy_to_reg (base);
#else
      displacement += (FIRST_PARM_OFFSET (context) - STARTING_FRAME_OFFSET);
      base = lookup_static_chain (var);
#endif
    }

  else if (basereg == virtual_stack_vars_rtx)
    {
      /* This is the same code as lookup_static_chain, duplicated here to
	 avoid an extra call to decl_function_context.  */
      tree link;

      for (link = context_display; link; link = TREE_CHAIN (link))
	if (TREE_PURPOSE (link) == context)
	  {
	    base = RTL_EXPR_RTL (TREE_VALUE (link));
	    break;
	  }
    }

  if (base == 0)
    abort ();

  /* Use same offset, relative to appropriate static chain or argument
     pointer.  */
  return plus_constant (base, displacement);
}

/* Return the address of the trampoline for entering nested fn FUNCTION.
   If necessary, allocate a trampoline (in the stack frame)
   and emit rtl to initialize its contents (at entry to this function).  */

rtx
trampoline_address (tree function)
{
  tree link;
  tree rtlexp;
  rtx tramp;
  struct function *fp;
  tree fn_context;

  /* Find an existing trampoline and return it.  */
  for (link = trampoline_list; link; link = TREE_CHAIN (link))
    if (TREE_PURPOSE (link) == function)
      return
	adjust_trampoline_addr (XEXP (RTL_EXPR_RTL (TREE_VALUE (link)), 0));

  for (fp = outer_function_chain; fp; fp = fp->outer)
    for (link = fp->x_trampoline_list; link; link = TREE_CHAIN (link))
      if (TREE_PURPOSE (link) == function)
	{
	  tramp = fix_lexical_addr (XEXP (RTL_EXPR_RTL (TREE_VALUE (link)), 0),
				    function);
	  return adjust_trampoline_addr (tramp);
	}

  /* None exists; we must make one.  */

  /* Find the `struct function' for the function containing FUNCTION.  */
  fp = 0;
  fn_context = decl_function_context (function);
  if (fn_context != current_function_decl
      && fn_context != inline_function_decl)
    fp = find_function_data (fn_context);

  /* Allocate run-time space for this trampoline.  */
  /* If rounding needed, allocate extra space
     to ensure we have TRAMPOLINE_SIZE bytes left after rounding up.  */
#define TRAMPOLINE_REAL_SIZE \
  (TRAMPOLINE_SIZE + (TRAMPOLINE_ALIGNMENT / BITS_PER_UNIT) - 1)
  tramp = assign_stack_local_1 (BLKmode, TRAMPOLINE_REAL_SIZE, 0,
				fp ? fp : cfun);
  /* Record the trampoline for reuse and note it for later initialization
     by expand_function_end.  */
  if (fp != 0)
    {
      rtlexp = make_node (RTL_EXPR);
      RTL_EXPR_RTL (rtlexp) = tramp;
      fp->x_trampoline_list = tree_cons (function, rtlexp,
					 fp->x_trampoline_list);
    }
  else
    {
      /* Make the RTL_EXPR node temporary, not momentary, so that the
	 trampoline_list doesn't become garbage.  */
      rtlexp = make_node (RTL_EXPR);

      RTL_EXPR_RTL (rtlexp) = tramp;
      trampoline_list = tree_cons (function, rtlexp, trampoline_list);
    }

  tramp = fix_lexical_addr (XEXP (tramp, 0), function);
  return adjust_trampoline_addr (tramp);
}

/* Given a trampoline address,
   round it to multiple of TRAMPOLINE_ALIGNMENT.  */

static rtx
round_trampoline_addr (rtx tramp)
{
  /* Round address up to desired boundary.  */
  rtx temp = gen_reg_rtx (Pmode);
  rtx addend = GEN_INT (TRAMPOLINE_ALIGNMENT / BITS_PER_UNIT - 1);
  rtx mask = GEN_INT (-TRAMPOLINE_ALIGNMENT / BITS_PER_UNIT);

  temp  = expand_simple_binop (Pmode, PLUS, tramp, addend,
			       temp, 0, OPTAB_LIB_WIDEN);
  tramp = expand_simple_binop (Pmode, AND, temp, mask,
			       temp, 0, OPTAB_LIB_WIDEN);

  return tramp;
}

/* Given a trampoline address, round it then apply any
   platform-specific adjustments so that the result can be used for a
   function call .  */

static rtx
adjust_trampoline_addr (rtx tramp)
{
  tramp = round_trampoline_addr (tramp);
#ifdef TRAMPOLINE_ADJUST_ADDRESS
  TRAMPOLINE_ADJUST_ADDRESS (tramp);
#endif
  return tramp;
}

/* Put all this function's BLOCK nodes including those that are chained
   onto the first block into a vector, and return it.
   Also store in each NOTE for the beginning or end of a block
   the index of that block in the vector.
   The arguments are BLOCK, the chain of top-level blocks of the function,
   and INSNS, the insn chain of the function.  */

void
identify_blocks (void)
{
  int n_blocks;
  tree *block_vector, *last_block_vector;
  tree *block_stack;
  tree block = DECL_INITIAL (current_function_decl);

  if (block == 0)
    return;

  /* Fill the BLOCK_VECTOR with all of the BLOCKs in this function, in
     depth-first order.  */
  block_vector = get_block_vector (block, &n_blocks);
  block_stack = xmalloc (n_blocks * sizeof (tree));

  last_block_vector = identify_blocks_1 (get_insns (),
					 block_vector + 1,
					 block_vector + n_blocks,
					 block_stack);

  /* If we didn't use all of the subblocks, we've misplaced block notes.  */
  /* ??? This appears to happen all the time.  Latent bugs elsewhere?  */
  if (0 && last_block_vector != block_vector + n_blocks)
    abort ();

  free (block_vector);
  free (block_stack);
}

/* Subroutine of identify_blocks.  Do the block substitution on the
   insn chain beginning with INSNS.  Recurse for CALL_PLACEHOLDER chains.

   BLOCK_STACK is pushed and popped for each BLOCK_BEGIN/BLOCK_END pair.
   BLOCK_VECTOR is incremented for each block seen.  */

static tree *
identify_blocks_1 (rtx insns, tree *block_vector, tree *end_block_vector,
		   tree *orig_block_stack)
{
  rtx insn;
  tree *block_stack = orig_block_stack;

  for (insn = insns; insn; insn = NEXT_INSN (insn))
    {
      if (GET_CODE (insn) == NOTE)
	{
	  if (NOTE_LINE_NUMBER (insn) == NOTE_INSN_BLOCK_BEG)
	    {
	      tree b;

	      /* If there are more block notes than BLOCKs, something
		 is badly wrong.  */
	      if (block_vector == end_block_vector)
		abort ();

	      b = *block_vector++;
	      NOTE_BLOCK (insn) = b;
	      *block_stack++ = b;
	    }
	  else if (NOTE_LINE_NUMBER (insn) == NOTE_INSN_BLOCK_END)
	    {
	      /* If there are more NOTE_INSN_BLOCK_ENDs than
		 NOTE_INSN_BLOCK_BEGs, something is badly wrong.  */
	      if (block_stack == orig_block_stack)
		abort ();

	      NOTE_BLOCK (insn) = *--block_stack;
	    }
	}
      else if (GET_CODE (insn) == CALL_INSN
	       && GET_CODE (PATTERN (insn)) == CALL_PLACEHOLDER)
	{
	  rtx cp = PATTERN (insn);

	  block_vector = identify_blocks_1 (XEXP (cp, 0), block_vector,
					    end_block_vector, block_stack);
	  if (XEXP (cp, 1))
	    block_vector = identify_blocks_1 (XEXP (cp, 1), block_vector,
					      end_block_vector, block_stack);
	  if (XEXP (cp, 2))
	    block_vector = identify_blocks_1 (XEXP (cp, 2), block_vector,
					      end_block_vector, block_stack);
	}
    }

  /* If there are more NOTE_INSN_BLOCK_BEGINs than NOTE_INSN_BLOCK_ENDs,
     something is badly wrong.  */
  if (block_stack != orig_block_stack)
    abort ();

  return block_vector;
}

/* Identify BLOCKs referenced by more than one NOTE_INSN_BLOCK_{BEG,END},
   and create duplicate blocks.  */
/* ??? Need an option to either create block fragments or to create
   abstract origin duplicates of a source block.  It really depends
   on what optimization has been performed.  */

void
reorder_blocks (void)
{
  tree block = DECL_INITIAL (current_function_decl);
  varray_type block_stack;

  if (block == NULL_TREE)
    return;

  VARRAY_TREE_INIT (block_stack, 10, "block_stack");

  /* Reset the TREE_ASM_WRITTEN bit for all blocks.  */
  reorder_blocks_0 (block);

  /* Prune the old trees away, so that they don't get in the way.  */
  BLOCK_SUBBLOCKS (block) = NULL_TREE;
  BLOCK_CHAIN (block) = NULL_TREE;

  /* Recreate the block tree from the note nesting.  */
  reorder_blocks_1 (get_insns (), block, &block_stack);
  BLOCK_SUBBLOCKS (block) = blocks_nreverse (BLOCK_SUBBLOCKS (block));

  /* Remove deleted blocks from the block fragment chains.  */
  reorder_fix_fragments (block);
}

/* Helper function for reorder_blocks.  Reset TREE_ASM_WRITTEN.  */

static void
reorder_blocks_0 (tree block)
{
  while (block)
    {
      TREE_ASM_WRITTEN (block) = 0;
      reorder_blocks_0 (BLOCK_SUBBLOCKS (block));
      block = BLOCK_CHAIN (block);
    }
}

static void
reorder_blocks_1 (rtx insns, tree current_block, varray_type *p_block_stack)
{
  rtx insn;

  for (insn = insns; insn; insn = NEXT_INSN (insn))
    {
      if (GET_CODE (insn) == NOTE)
	{
	  if (NOTE_LINE_NUMBER (insn) == NOTE_INSN_BLOCK_BEG)
	    {
	      tree block = NOTE_BLOCK (insn);

	      /* If we have seen this block before, that means it now
		 spans multiple address regions.  Create a new fragment.  */
	      if (TREE_ASM_WRITTEN (block))
		{
		  tree new_block = copy_node (block);
		  tree origin;

		  origin = (BLOCK_FRAGMENT_ORIGIN (block)
			    ? BLOCK_FRAGMENT_ORIGIN (block)
			    : block);
		  BLOCK_FRAGMENT_ORIGIN (new_block) = origin;
		  BLOCK_FRAGMENT_CHAIN (new_block)
		    = BLOCK_FRAGMENT_CHAIN (origin);
		  BLOCK_FRAGMENT_CHAIN (origin) = new_block;

		  NOTE_BLOCK (insn) = new_block;
		  block = new_block;
		}

	      BLOCK_SUBBLOCKS (block) = 0;
	      TREE_ASM_WRITTEN (block) = 1;
	      /* When there's only one block for the entire function,
		 current_block == block and we mustn't do this, it
		 will cause infinite recursion.  */
	      if (block != current_block)
		{
		  BLOCK_SUPERCONTEXT (block) = current_block;
		  BLOCK_CHAIN (block) = BLOCK_SUBBLOCKS (current_block);
		  BLOCK_SUBBLOCKS (current_block) = block;
		  current_block = block;
		}
	      VARRAY_PUSH_TREE (*p_block_stack, block);
	    }
	  else if (NOTE_LINE_NUMBER (insn) == NOTE_INSN_BLOCK_END)
	    {
	      NOTE_BLOCK (insn) = VARRAY_TOP_TREE (*p_block_stack);
	      VARRAY_POP (*p_block_stack);
	      BLOCK_SUBBLOCKS (current_block)
		= blocks_nreverse (BLOCK_SUBBLOCKS (current_block));
	      current_block = BLOCK_SUPERCONTEXT (current_block);
	    }
	}
      else if (GET_CODE (insn) == CALL_INSN
	       && GET_CODE (PATTERN (insn)) == CALL_PLACEHOLDER)
	{
	  rtx cp = PATTERN (insn);
	  reorder_blocks_1 (XEXP (cp, 0), current_block, p_block_stack);
	  if (XEXP (cp, 1))
	    reorder_blocks_1 (XEXP (cp, 1), current_block, p_block_stack);
	  if (XEXP (cp, 2))
	    reorder_blocks_1 (XEXP (cp, 2), current_block, p_block_stack);
	}
    }
}

/* Rationalize BLOCK_FRAGMENT_ORIGIN.  If an origin block no longer
   appears in the block tree, select one of the fragments to become
   the new origin block.  */

static void
reorder_fix_fragments (tree block)
{
  while (block)
    {
      tree dup_origin = BLOCK_FRAGMENT_ORIGIN (block);
      tree new_origin = NULL_TREE;

      if (dup_origin)
	{
	  if (! TREE_ASM_WRITTEN (dup_origin))
	    {
	      new_origin = BLOCK_FRAGMENT_CHAIN (dup_origin);

	      /* Find the first of the remaining fragments.  There must
		 be at least one -- the current block.  */
	      while (! TREE_ASM_WRITTEN (new_origin))
		new_origin = BLOCK_FRAGMENT_CHAIN (new_origin);
	      BLOCK_FRAGMENT_ORIGIN (new_origin) = NULL_TREE;
	    }
	}
      else if (! dup_origin)
	new_origin = block;

      /* Re-root the rest of the fragments to the new origin.  In the
	 case that DUP_ORIGIN was null, that means BLOCK was the origin
	 of a chain of fragments and we want to remove those fragments
	 that didn't make it to the output.  */
      if (new_origin)
	{
	  tree *pp = &BLOCK_FRAGMENT_CHAIN (new_origin);
	  tree chain = *pp;

	  while (chain)
	    {
	      if (TREE_ASM_WRITTEN (chain))
		{
		  BLOCK_FRAGMENT_ORIGIN (chain) = new_origin;
		  *pp = chain;
		  pp = &BLOCK_FRAGMENT_CHAIN (chain);
		}
	      chain = BLOCK_FRAGMENT_CHAIN (chain);
	    }
	  *pp = NULL_TREE;
	}

      reorder_fix_fragments (BLOCK_SUBBLOCKS (block));
      block = BLOCK_CHAIN (block);
    }
}

/* Reverse the order of elements in the chain T of blocks,
   and return the new head of the chain (old last element).  */

static tree
blocks_nreverse (tree t)
{
  tree prev = 0, decl, next;
  for (decl = t; decl; decl = next)
    {
      next = BLOCK_CHAIN (decl);
      BLOCK_CHAIN (decl) = prev;
      prev = decl;
    }
  return prev;
}

/* Count the subblocks of the list starting with BLOCK.  If VECTOR is
   non-NULL, list them all into VECTOR, in a depth-first preorder
   traversal of the block tree.  Also clear TREE_ASM_WRITTEN in all
   blocks.  */

static int
all_blocks (tree block, tree *vector)
{
  int n_blocks = 0;

  while (block)
    {
      TREE_ASM_WRITTEN (block) = 0;

      /* Record this block.  */
      if (vector)
	vector[n_blocks] = block;

      ++n_blocks;

      /* Record the subblocks, and their subblocks...  */
      n_blocks += all_blocks (BLOCK_SUBBLOCKS (block),
			      vector ? vector + n_blocks : 0);
      block = BLOCK_CHAIN (block);
    }

  return n_blocks;
}

/* Return a vector containing all the blocks rooted at BLOCK.  The
   number of elements in the vector is stored in N_BLOCKS_P.  The
   vector is dynamically allocated; it is the caller's responsibility
   to call `free' on the pointer returned.  */

static tree *
get_block_vector (tree block, int *n_blocks_p)
{
  tree *block_vector;

  *n_blocks_p = all_blocks (block, NULL);
  block_vector = xmalloc (*n_blocks_p * sizeof (tree));
  all_blocks (block, block_vector);

  return block_vector;
}

static GTY(()) int next_block_index = 2;

/* Set BLOCK_NUMBER for all the blocks in FN.  */

void
number_blocks (tree fn)
{
  int i;
  int n_blocks;
  tree *block_vector;

  /* For SDB and XCOFF debugging output, we start numbering the blocks
     from 1 within each function, rather than keeping a running
     count.  */
#if defined (SDB_DEBUGGING_INFO) || defined (XCOFF_DEBUGGING_INFO)
  if (write_symbols == SDB_DEBUG || write_symbols == XCOFF_DEBUG)
    next_block_index = 1;
#endif

  block_vector = get_block_vector (DECL_INITIAL (fn), &n_blocks);

  /* The top-level BLOCK isn't numbered at all.  */
  for (i = 1; i < n_blocks; ++i)
    /* We number the blocks from two.  */
    BLOCK_NUMBER (block_vector[i]) = next_block_index++;

  free (block_vector);

  return;
}

/* If VAR is present in a subblock of BLOCK, return the subblock.  */

tree
debug_find_var_in_block_tree (tree var, tree block)
{
  tree t;

  for (t = BLOCK_VARS (block); t; t = TREE_CHAIN (t))
    if (t == var)
      return block;

  for (t = BLOCK_SUBBLOCKS (block); t; t = TREE_CHAIN (t))
    {
      tree ret = debug_find_var_in_block_tree (var, t);
      if (ret)
	return ret;
    }

  return NULL_TREE;
}

/* Allocate a function structure for FNDECL and set its contents
   to the defaults.  */

void
allocate_struct_function (tree fndecl)
{
  tree result;

  cfun = ggc_alloc_cleared (sizeof (struct function));

  max_parm_reg = LAST_VIRTUAL_REGISTER + 1;

  cfun->stack_alignment_needed = STACK_BOUNDARY;
  cfun->preferred_stack_boundary = STACK_BOUNDARY;

  current_function_funcdef_no = funcdef_no++;

  cfun->function_frequency = FUNCTION_FREQUENCY_NORMAL;

  init_stmt_for_function ();
  init_eh_for_function ();

  (*lang_hooks.function.init) (cfun);
  if (init_machine_status)
    cfun->machine = (*init_machine_status) ();

  if (fndecl == NULL)
    return;

  DECL_SAVED_INSNS (fndecl) = cfun;
  cfun->decl = fndecl;

  result = DECL_RESULT (fndecl);
  if (aggregate_value_p (result, fndecl))
    {
#ifdef PCC_STATIC_STRUCT_RETURN
      current_function_returns_pcc_struct = 1;
#endif
      current_function_returns_struct = 1;
    }

  current_function_returns_pointer = POINTER_TYPE_P (TREE_TYPE (result));

  current_function_needs_context
    = (decl_function_context (current_function_decl) != 0
       && ! DECL_NO_STATIC_CHAIN (current_function_decl));
}

/* Reset cfun, and other non-struct-function variables to defaults as
   appropriate for emitting rtl at the start of a function.  */

static void
prepare_function_start (tree fndecl)
{
  if (fndecl && DECL_SAVED_INSNS (fndecl))
    cfun = DECL_SAVED_INSNS (fndecl);
  else
    allocate_struct_function (fndecl);
  init_emit ();
  init_varasm_status (cfun);
  init_expr ();

  cse_not_expected = ! optimize;

  /* Caller save not needed yet.  */
  caller_save_needed = 0;

  /* We haven't done register allocation yet.  */
  reg_renumber = 0;

  /* Indicate that we need to distinguish between the return value of the
     present function and the return value of a function being called.  */
  rtx_equal_function_value_matters = 1;

  /* Indicate that we have not instantiated virtual registers yet.  */
  virtuals_instantiated = 0;

  /* Indicate that we want CONCATs now.  */
  generating_concat_p = 1;

  /* Indicate we have no need of a frame pointer yet.  */
  frame_pointer_needed = 0;
}

/* Initialize the rtl expansion mechanism so that we can do simple things
   like generate sequences.  This is used to provide a context during global
   initialization of some passes.  */
void
init_dummy_function_start (void)
{
  prepare_function_start (NULL);
}

/* Generate RTL for the start of the function SUBR (a FUNCTION_DECL tree node)
   and initialize static variables for generating RTL for the statements
   of the function.  */

void
init_function_start (tree subr)
{
  prepare_function_start (subr);

  /* Within function body, compute a type's size as soon it is laid out.  */
  immediate_size_expand++;

  /* Prevent ever trying to delete the first instruction of a
     function.  Also tell final how to output a linenum before the
     function prologue.  Note linenums could be missing, e.g. when
     compiling a Java .class file.  */
  if (DECL_SOURCE_LINE (subr))
    emit_line_note (DECL_SOURCE_LOCATION (subr));

  /* Make sure first insn is a note even if we don't want linenums.
     This makes sure the first insn will never be deleted.
     Also, final expects a note to appear there.  */
  emit_note (NOTE_INSN_DELETED);

  /* Warn if this value is an aggregate type,
     regardless of which calling convention we are using for it.  */
  if (warn_aggregate_return
      && AGGREGATE_TYPE_P (TREE_TYPE (DECL_RESULT (subr))))
    warning ("function returns an aggregate");
}

/* Make sure all values used by the optimization passes have sane
   defaults.  */
void
init_function_for_compilation (void)
{
  reg_renumber = 0;

  /* No prologue/epilogue insns yet.  */
  VARRAY_GROW (prologue, 0);
  VARRAY_GROW (epilogue, 0);
  VARRAY_GROW (sibcall_epilogue, 0);
}

/* Expand a call to __main at the beginning of a possible main function.  */

#if defined(INIT_SECTION_ASM_OP) && !defined(INVOKE__main)
#undef HAS_INIT_SECTION
#define HAS_INIT_SECTION
#endif

void
expand_main_function (void)
{
#ifdef FORCE_PREFERRED_STACK_BOUNDARY_IN_MAIN
  if (FORCE_PREFERRED_STACK_BOUNDARY_IN_MAIN)
    {
      int align = PREFERRED_STACK_BOUNDARY / BITS_PER_UNIT;
      rtx tmp, seq;

      start_sequence ();
      /* Forcibly align the stack.  */
#ifdef STACK_GROWS_DOWNWARD
      tmp = expand_simple_binop (Pmode, AND, stack_pointer_rtx, GEN_INT(-align),
				 stack_pointer_rtx, 1, OPTAB_WIDEN);
#else
      tmp = expand_simple_binop (Pmode, PLUS, stack_pointer_rtx,
				 GEN_INT (align - 1), NULL_RTX, 1, OPTAB_WIDEN);
      tmp = expand_simple_binop (Pmode, AND, tmp, GEN_INT (-align),
				 stack_pointer_rtx, 1, OPTAB_WIDEN);
#endif
      if (tmp != stack_pointer_rtx)
	emit_move_insn (stack_pointer_rtx, tmp);

      /* Enlist allocate_dynamic_stack_space to pick up the pieces.  */
      tmp = force_reg (Pmode, const0_rtx);
      allocate_dynamic_stack_space (tmp, NULL_RTX, BIGGEST_ALIGNMENT);
      seq = get_insns ();
      end_sequence ();

      for (tmp = get_last_insn (); tmp; tmp = PREV_INSN (tmp))
	if (NOTE_P (tmp) && NOTE_LINE_NUMBER (tmp) == NOTE_INSN_FUNCTION_BEG)
	  break;
      if (tmp)
	emit_insn_before (seq, tmp);
      else
	emit_insn (seq);
    }
#endif

#ifndef HAS_INIT_SECTION
  emit_library_call (init_one_libfunc (NAME__MAIN), LCT_NORMAL, VOIDmode, 0);
#endif
}

/* The PENDING_SIZES represent the sizes of variable-sized types.
   Create RTL for the various sizes now (using temporary variables),
   so that we can refer to the sizes from the RTL we are generating
   for the current function.  The PENDING_SIZES are a TREE_LIST.  The
   TREE_VALUE of each node is a SAVE_EXPR.  */

void
expand_pending_sizes (tree pending_sizes)
{
  tree tem;

  /* Evaluate now the sizes of any types declared among the arguments.  */
  for (tem = pending_sizes; tem; tem = TREE_CHAIN (tem))
    {
      expand_expr (TREE_VALUE (tem), const0_rtx, VOIDmode, 0);
      /* Flush the queue in case this parameter declaration has
	 side-effects.  */
      emit_queue ();
    }
}

/* Start the RTL for a new function, and set variables used for
   emitting RTL.
   SUBR is the FUNCTION_DECL node.
   PARMS_HAVE_CLEANUPS is nonzero if there are cleanups associated with
   the function's parameters, which must be run at any return statement.  */

void
expand_function_start (tree subr, int parms_have_cleanups)
{
  tree tem;
  rtx last_ptr = NULL_RTX;

  /* Make sure volatile mem refs aren't considered
     valid operands of arithmetic insns.  */
  init_recog_no_volatile ();

  current_function_instrument_entry_exit
    = (flag_instrument_function_entry_exit
       && ! DECL_NO_INSTRUMENT_FUNCTION_ENTRY_EXIT (subr));

  current_function_profile
    = (profile_flag
       && ! DECL_NO_INSTRUMENT_FUNCTION_ENTRY_EXIT (subr));

  current_function_limit_stack
    = (stack_limit_rtx != NULL_RTX && ! DECL_NO_LIMIT_STACK (subr));

  /* If function gets a static chain arg, store it in the stack frame.
     Do this first, so it gets the first stack slot offset.  */
  if (current_function_needs_context)
    {
      last_ptr = assign_stack_local (Pmode, GET_MODE_SIZE (Pmode), 0);

      /* Delay copying static chain if it is not a register to avoid
	 conflicts with regs used for parameters.  */
      if (! SMALL_REGISTER_CLASSES
	  || GET_CODE (static_chain_incoming_rtx) == REG)
	emit_move_insn (last_ptr, static_chain_incoming_rtx);
    }

  /* If the parameters of this function need cleaning up, get a label
     for the beginning of the code which executes those cleanups.  This must
     be done before doing anything with return_label.  */
  if (parms_have_cleanups)
    cleanup_label = gen_label_rtx ();
  else
    cleanup_label = 0;

  /* Make the label for return statements to jump to.  Do not special
     case machines with special return instructions -- they will be
     handled later during jump, ifcvt, or epilogue creation.  */
  return_label = gen_label_rtx ();

  /* Initialize rtx used to return the value.  */
  /* Do this before assign_parms so that we copy the struct value address
     before any library calls that assign parms might generate.  */

  /* Decide whether to return the value in memory or in a register.  */
  if (aggregate_value_p (DECL_RESULT (subr), subr))
    {
      /* Returning something that won't go in a register.  */
      rtx value_address = 0;

#ifdef PCC_STATIC_STRUCT_RETURN
      if (current_function_returns_pcc_struct)
	{
	  int size = int_size_in_bytes (TREE_TYPE (DECL_RESULT (subr)));
	  value_address = assemble_static_space (size);
	}
      else
#endif
	{
	  rtx sv = targetm.calls.struct_value_rtx (TREE_TYPE (subr), 1);
	  /* Expect to be passed the address of a place to store the value.
	     If it is passed as an argument, assign_parms will take care of
	     it.  */
	  if (sv)
	    {
	      value_address = gen_reg_rtx (Pmode);
	      emit_move_insn (value_address, sv);
	    }
	}
      if (value_address)
	{
	  rtx x = gen_rtx_MEM (DECL_MODE (DECL_RESULT (subr)), value_address);
	  set_mem_attributes (x, DECL_RESULT (subr), 1);
	  SET_DECL_RTL (DECL_RESULT (subr), x);
	}
    }
  else if (DECL_MODE (DECL_RESULT (subr)) == VOIDmode)
    /* If return mode is void, this decl rtl should not be used.  */
    SET_DECL_RTL (DECL_RESULT (subr), NULL_RTX);
  else
    {
      /* Compute the return values into a pseudo reg, which we will copy
	 into the true return register after the cleanups are done.  */

      /* In order to figure out what mode to use for the pseudo, we
	 figure out what the mode of the eventual return register will
	 actually be, and use that.  */
      rtx hard_reg
	= hard_function_value (TREE_TYPE (DECL_RESULT (subr)),
			       subr, 1);

      /* Structures that are returned in registers are not aggregate_value_p,
	 so we may see a PARALLEL or a REG.  */
      if (REG_P (hard_reg))
	SET_DECL_RTL (DECL_RESULT (subr), gen_reg_rtx (GET_MODE (hard_reg)));
      else if (GET_CODE (hard_reg) == PARALLEL)
	SET_DECL_RTL (DECL_RESULT (subr), gen_group_rtx (hard_reg));
      else
	abort ();

      /* Set DECL_REGISTER flag so that expand_function_end will copy the
	 result to the real return register(s).  */
      DECL_REGISTER (DECL_RESULT (subr)) = 1;
    }

  /* Initialize rtx for parameters and local variables.
     In some cases this requires emitting insns.  */

  assign_parms (subr);

  /* Copy the static chain now if it wasn't a register.  The delay is to
     avoid conflicts with the parameter passing registers.  */

  if (SMALL_REGISTER_CLASSES && current_function_needs_context)
    if (GET_CODE (static_chain_incoming_rtx) != REG)
      emit_move_insn (last_ptr, static_chain_incoming_rtx);

  /* The following was moved from init_function_start.
     The move is supposed to make sdb output more accurate.  */
  /* Indicate the beginning of the function body,
     as opposed to parm setup.  */
  emit_note (NOTE_INSN_FUNCTION_BEG);

  if (GET_CODE (get_last_insn ()) != NOTE)
    emit_note (NOTE_INSN_DELETED);
  parm_birth_insn = get_last_insn ();

  context_display = 0;
  if (current_function_needs_context)
    {
      /* Fetch static chain values for containing functions.  */
      tem = decl_function_context (current_function_decl);
      /* Copy the static chain pointer into a pseudo.  If we have
	 small register classes, copy the value from memory if
	 static_chain_incoming_rtx is a REG.  */
      if (tem)
	{
	  /* If the static chain originally came in a register, put it back
	     there, then move it out in the next insn.  The reason for
	     this peculiar code is to satisfy function integration.  */
	  if (SMALL_REGISTER_CLASSES
	      && GET_CODE (static_chain_incoming_rtx) == REG)
	    emit_move_insn (static_chain_incoming_rtx, last_ptr);
	  last_ptr = copy_to_reg (static_chain_incoming_rtx);
	}

      while (tem)
	{
	  tree rtlexp = make_node (RTL_EXPR);

	  RTL_EXPR_RTL (rtlexp) = last_ptr;
	  context_display = tree_cons (tem, rtlexp, context_display);
	  tem = decl_function_context (tem);
	  if (tem == 0)
	    break;
	  /* Chain through stack frames, assuming pointer to next lexical frame
	     is found at the place we always store it.  */
#ifdef FRAME_GROWS_DOWNWARD
	  last_ptr = plus_constant (last_ptr,
				    -(HOST_WIDE_INT) GET_MODE_SIZE (Pmode));
#endif
	  last_ptr = gen_rtx_MEM (Pmode, memory_address (Pmode, last_ptr));
	  set_mem_alias_set (last_ptr, get_frame_alias_set ());
	  last_ptr = copy_to_reg (last_ptr);

	  /* If we are not optimizing, ensure that we know that this
	     piece of context is live over the entire function.  */
	  if (! optimize)
	    save_expr_regs = gen_rtx_EXPR_LIST (VOIDmode, last_ptr,
						save_expr_regs);
	}
    }

  if (current_function_instrument_entry_exit)
    {
      rtx fun = DECL_RTL (current_function_decl);
      if (GET_CODE (fun) == MEM)
	fun = XEXP (fun, 0);
      else
	abort ();
      emit_library_call (profile_function_entry_libfunc, LCT_NORMAL, VOIDmode,
			 2, fun, Pmode,
			 expand_builtin_return_addr (BUILT_IN_RETURN_ADDRESS,
						     0,
						     hard_frame_pointer_rtx),
			 Pmode);
    }

  if (current_function_profile)
    {
#ifdef PROFILE_HOOK
      PROFILE_HOOK (current_function_funcdef_no);
#endif
    }

  /* After the display initializations is where the tail-recursion label
     should go, if we end up needing one.   Ensure we have a NOTE here
     since some things (like trampolines) get placed before this.  */
  tail_recursion_reentry = emit_note (NOTE_INSN_DELETED);

  /* Evaluate now the sizes of any types declared among the arguments.  */
  expand_pending_sizes (nreverse (get_pending_sizes ()));

  /* Make sure there is a line number after the function entry setup code.  */
  force_next_line_note ();
}

/* Undo the effects of init_dummy_function_start.  */
void
expand_dummy_function_end (void)
{
  /* End any sequences that failed to be closed due to syntax errors.  */
  while (in_sequence_p ())
    end_sequence ();

  /* Outside function body, can't compute type's actual size
     until next function's body starts.  */

  free_after_parsing (cfun);
  free_after_compilation (cfun);
  cfun = 0;
}

/* Call DOIT for each hard register used as a return value from
   the current function.  */

void
diddle_return_value (void (*doit) (rtx, void *), void *arg)
{
  rtx outgoing = current_function_return_rtx;

  if (! outgoing)
    return;

  if (GET_CODE (outgoing) == REG)
    (*doit) (outgoing, arg);
  else if (GET_CODE (outgoing) == PARALLEL)
    {
      int i;

      for (i = 0; i < XVECLEN (outgoing, 0); i++)
	{
	  rtx x = XEXP (XVECEXP (outgoing, 0, i), 0);

	  if (GET_CODE (x) == REG && REGNO (x) < FIRST_PSEUDO_REGISTER)
	    (*doit) (x, arg);
	}
    }
}

static void
do_clobber_return_reg (rtx reg, void *arg ATTRIBUTE_UNUSED)
{
  emit_insn (gen_rtx_CLOBBER (VOIDmode, reg));
}

void
clobber_return_register (void)
{
  diddle_return_value (do_clobber_return_reg, NULL);

  /* In case we do use pseudo to return value, clobber it too.  */
  if (DECL_RTL_SET_P (DECL_RESULT (current_function_decl)))
    {
      tree decl_result = DECL_RESULT (current_function_decl);
      rtx decl_rtl = DECL_RTL (decl_result);
      if (REG_P (decl_rtl) && REGNO (decl_rtl) >= FIRST_PSEUDO_REGISTER)
	{
	  do_clobber_return_reg (decl_rtl, NULL);
	}
    }
}

static void
do_use_return_reg (rtx reg, void *arg ATTRIBUTE_UNUSED)
{
  emit_insn (gen_rtx_USE (VOIDmode, reg));
}

void
use_return_register (void)
{
  diddle_return_value (do_use_return_reg, NULL);
}

/* Possibly warn about unused parameters.  */
void
do_warn_unused_parameter (tree fn)
{
  tree decl;

  for (decl = DECL_ARGUMENTS (fn);
       decl; decl = TREE_CHAIN (decl))
    if (!TREE_USED (decl) && TREE_CODE (decl) == PARM_DECL
	&& DECL_NAME (decl) && !DECL_ARTIFICIAL (decl))
      warning ("%Junused parameter '%D'", decl, decl);
}

static GTY(()) rtx initial_trampoline;

/* Generate RTL for the end of the current function.  */

void
expand_function_end (void)
{
  tree link;
  rtx clobber_after;

  finish_expr_for_function ();

  /* If arg_pointer_save_area was referenced only from a nested
     function, we will not have initialized it yet.  Do that now.  */
  if (arg_pointer_save_area && ! cfun->arg_pointer_save_area_init)
    get_arg_pointer_save_area (cfun);

#ifdef NON_SAVING_SETJMP
  /* Don't put any variables in registers if we call setjmp
     on a machine that fails to restore the registers.  */
  if (NON_SAVING_SETJMP && current_function_calls_setjmp)
    {
      if (DECL_INITIAL (current_function_decl) != error_mark_node)
	setjmp_protect (DECL_INITIAL (current_function_decl));

      setjmp_protect_args ();
    }
#endif

  /* Initialize any trampolines required by this function.  */
  for (link = trampoline_list; link; link = TREE_CHAIN (link))
    {
      tree function = TREE_PURPOSE (link);
      rtx context ATTRIBUTE_UNUSED = lookup_static_chain (function);
      rtx tramp = RTL_EXPR_RTL (TREE_VALUE (link));
#ifdef TRAMPOLINE_TEMPLATE
      rtx blktramp;
#endif
      rtx seq;

#ifdef TRAMPOLINE_TEMPLATE
      /* First make sure this compilation has a template for
	 initializing trampolines.  */
      if (initial_trampoline == 0)
	{
	  initial_trampoline
	    = gen_rtx_MEM (BLKmode, assemble_trampoline_template ());
	  set_mem_align (initial_trampoline, TRAMPOLINE_ALIGNMENT);
	}
#endif

      /* Generate insns to initialize the trampoline.  */
      start_sequence ();
      tramp = round_trampoline_addr (XEXP (tramp, 0));
#ifdef TRAMPOLINE_TEMPLATE
      blktramp = replace_equiv_address (initial_trampoline, tramp);
      emit_block_move (blktramp, initial_trampoline,
		       GEN_INT (TRAMPOLINE_SIZE), BLOCK_OP_NORMAL);
#endif
      trampolines_created = 1;
      INITIALIZE_TRAMPOLINE (tramp, XEXP (DECL_RTL (function), 0), context);
      seq = get_insns ();
      end_sequence ();

      /* Put those insns at entry to the containing function (this one).  */
      emit_insn_before (seq, tail_recursion_reentry);
    }

  /* If we are doing stack checking and this function makes calls,
     do a stack probe at the start of the function to ensure we have enough
     space for another stack frame.  */
  if (flag_stack_check && ! STACK_CHECK_BUILTIN)
    {
      rtx insn, seq;

      for (insn = get_insns (); insn; insn = NEXT_INSN (insn))
	if (GET_CODE (insn) == CALL_INSN)
	  {
	    start_sequence ();
	    probe_stack_range (STACK_CHECK_PROTECT,
			       GEN_INT (STACK_CHECK_MAX_FRAME_SIZE));
	    seq = get_insns ();
	    end_sequence ();
	    emit_insn_before (seq, tail_recursion_reentry);
	    break;
	  }
    }

  /* Possibly warn about unused parameters.
     When frontend does unit-at-a-time, the warning is already
     issued at finalization time.  */
  if (warn_unused_parameter
      && !lang_hooks.callgraph.expand_function)
    do_warn_unused_parameter (current_function_decl);

  /* Delete handlers for nonlocal gotos if nothing uses them.  */
  if (nonlocal_goto_handler_slots != 0
      && ! current_function_has_nonlocal_label)
    delete_handlers ();

  /* End any sequences that failed to be closed due to syntax errors.  */
  while (in_sequence_p ())
    end_sequence ();

  /* Outside function body, can't compute type's actual size
     until next function's body starts.  */
  immediate_size_expand--;

  clear_pending_stack_adjust ();
  do_pending_stack_adjust ();

  /* ???  This is a kludge.  We want to ensure that instructions that
     may trap are not moved into the epilogue by scheduling, because
     we don't always emit unwind information for the epilogue.
     However, not all machine descriptions define a blockage insn, so
     emit an ASM_INPUT to act as one.  */
  if (flag_non_call_exceptions)
    emit_insn (gen_rtx_ASM_INPUT (VOIDmode, ""));

  /* Mark the end of the function body.
     If control reaches this insn, the function can drop through
     without returning a value.  */
  emit_note (NOTE_INSN_FUNCTION_END);

  /* Must mark the last line number note in the function, so that the test
     coverage code can avoid counting the last line twice.  This just tells
     the code to ignore the immediately following line note, since there
     already exists a copy of this note somewhere above.  This line number
     note is still needed for debugging though, so we can't delete it.  */
  if (flag_test_coverage)
    emit_note (NOTE_INSN_REPEATED_LINE_NUMBER);

  /* Output a linenumber for the end of the function.
     SDB depends on this.  */
  force_next_line_note ();
  emit_line_note (input_location);

  /* Before the return label (if any), clobber the return
     registers so that they are not propagated live to the rest of
     the function.  This can only happen with functions that drop
     through; if there had been a return statement, there would
     have either been a return rtx, or a jump to the return label.

     We delay actual code generation after the current_function_value_rtx
     is computed.  */
  clobber_after = get_last_insn ();

  /* Output the label for the actual return from the function,
     if one is expected.  This happens either because a function epilogue
     is used instead of a return instruction, or because a return was done
     with a goto in order to run local cleanups, or because of pcc-style
     structure returning.  */
  if (return_label)
    emit_label (return_label);

  if (current_function_instrument_entry_exit)
    {
      rtx fun = DECL_RTL (current_function_decl);
      if (GET_CODE (fun) == MEM)
	fun = XEXP (fun, 0);
      else
	abort ();
      emit_library_call (profile_function_exit_libfunc, LCT_NORMAL, VOIDmode,
			 2, fun, Pmode,
			 expand_builtin_return_addr (BUILT_IN_RETURN_ADDRESS,
						     0,
						     hard_frame_pointer_rtx),
			 Pmode);
    }

#ifdef TARGET_PROFILER_EPILOGUE
  if (current_function_profile && TARGET_PROFILER_EPILOGUE)
    {
      static rtx mexitcount_libfunc;
      static int initialized;

      if (!initialized)
	{
	  mexitcount_libfunc = init_one_libfunc (".mexitcount");
	  initialized = 1;
	}
      emit_library_call (mexitcount_libfunc, LCT_NORMAL, VOIDmode, 0);
    }
#endif

  /* Let except.c know where it should emit the call to unregister
     the function context for sjlj exceptions.  */
  if (flag_exceptions && USING_SJLJ_EXCEPTIONS)
    sjlj_emit_function_exit_after (get_last_insn ());

  /* If we had calls to alloca, and this machine needs
     an accurate stack pointer to exit the function,
     insert some code to save and restore the stack pointer.  */
  if (! EXIT_IGNORE_STACK
      && current_function_calls_alloca)
    {
      rtx tem = 0;

      emit_stack_save (SAVE_FUNCTION, &tem, parm_birth_insn);
      emit_stack_restore (SAVE_FUNCTION, tem, NULL_RTX);
    }

  /* If scalar return value was computed in a pseudo-reg, or was a named
     return value that got dumped to the stack, copy that to the hard
     return register.  */
  if (DECL_RTL_SET_P (DECL_RESULT (current_function_decl)))
    {
      tree decl_result = DECL_RESULT (current_function_decl);
      rtx decl_rtl = DECL_RTL (decl_result);

      if (REG_P (decl_rtl)
	  ? REGNO (decl_rtl) >= FIRST_PSEUDO_REGISTER
	  : DECL_REGISTER (decl_result))
	{
	  rtx real_decl_rtl = current_function_return_rtx;

	  /* This should be set in assign_parms.  */
	  if (! REG_FUNCTION_VALUE_P (real_decl_rtl))
	    abort ();

	  /* If this is a BLKmode structure being returned in registers,
	     then use the mode computed in expand_return.  Note that if
	     decl_rtl is memory, then its mode may have been changed,
	     but that current_function_return_rtx has not.  */
	  if (GET_MODE (real_decl_rtl) == BLKmode)
	    PUT_MODE (real_decl_rtl, GET_MODE (decl_rtl));

	  /* If a named return value dumped decl_return to memory, then
	     we may need to re-do the PROMOTE_MODE signed/unsigned
	     extension.  */
	  if (GET_MODE (real_decl_rtl) != GET_MODE (decl_rtl))
	    {
	      int unsignedp = TREE_UNSIGNED (TREE_TYPE (decl_result));

	      if (targetm.calls.promote_function_return (TREE_TYPE (current_function_decl)))
		promote_mode (TREE_TYPE (decl_result), GET_MODE (decl_rtl),
			      &unsignedp, 1);

	      convert_move (real_decl_rtl, decl_rtl, unsignedp);
	    }
	  else if (GET_CODE (real_decl_rtl) == PARALLEL)
	    {
	      /* If expand_function_start has created a PARALLEL for decl_rtl,
		 move the result to the real return registers.  Otherwise, do
		 a group load from decl_rtl for a named return.  */
	      if (GET_CODE (decl_rtl) == PARALLEL)
		emit_group_move (real_decl_rtl, decl_rtl);
	      else
		emit_group_load (real_decl_rtl, decl_rtl,
				 TREE_TYPE (decl_result),
				 int_size_in_bytes (TREE_TYPE (decl_result)));
	    }
	  else
	    emit_move_insn (real_decl_rtl, decl_rtl);
	}
    }

  /* If returning a structure, arrange to return the address of the value
     in a place where debuggers expect to find it.

     If returning a structure PCC style,
     the caller also depends on this value.
     And current_function_returns_pcc_struct is not necessarily set.  */
  if (current_function_returns_struct
      || current_function_returns_pcc_struct)
    {
      rtx value_address
	= XEXP (DECL_RTL (DECL_RESULT (current_function_decl)), 0);
      tree type = TREE_TYPE (DECL_RESULT (current_function_decl));
#ifdef FUNCTION_OUTGOING_VALUE
      rtx outgoing
	= FUNCTION_OUTGOING_VALUE (build_pointer_type (type),
				   current_function_decl);
#else
      rtx outgoing
	= FUNCTION_VALUE (build_pointer_type (type), current_function_decl);
#endif

      /* Mark this as a function return value so integrate will delete the
	 assignment and USE below when inlining this function.  */
      REG_FUNCTION_VALUE_P (outgoing) = 1;

      /* The address may be ptr_mode and OUTGOING may be Pmode.  */
      value_address = convert_memory_address (GET_MODE (outgoing),
					      value_address);

      emit_move_insn (outgoing, value_address);

      /* Show return register used to hold result (in this case the address
	 of the result.  */
      current_function_return_rtx = outgoing;
    }

  /* If this is an implementation of throw, do what's necessary to
     communicate between __builtin_eh_return and the epilogue.  */
  expand_eh_return ();

  /* Emit the actual code to clobber return register.  */
  {
    rtx seq, after;

    start_sequence ();
    clobber_return_register ();
    seq = get_insns ();
    end_sequence ();

    after = emit_insn_after (seq, clobber_after);

    if (clobber_after != after)
      cfun->x_clobber_return_insn = after;
  }

  /* Output the label for the naked return from the function, if one is
     expected.  This is currently used only by __builtin_return.  */
  if (naked_return_label)
    emit_label (naked_return_label);

  /* ??? This should no longer be necessary since stupid is no longer with
     us, but there are some parts of the compiler (eg reload_combine, and
     sh mach_dep_reorg) that still try and compute their own lifetime info
     instead of using the general framework.  */
  use_return_register ();

  /* Fix up any gotos that jumped out to the outermost
     binding level of the function.
     Must follow emitting RETURN_LABEL.  */

  /* If you have any cleanups to do at this point,
     and they need to create temporary variables,
     then you will lose.  */
  expand_fixups (get_insns ());
}

rtx
get_arg_pointer_save_area (struct function *f)
{
  rtx ret = f->x_arg_pointer_save_area;

  if (! ret)
    {
      ret = assign_stack_local_1 (Pmode, GET_MODE_SIZE (Pmode), 0, f);
      f->x_arg_pointer_save_area = ret;
    }

  if (f == cfun && ! f->arg_pointer_save_area_init)
    {
      rtx seq;

      /* Save the arg pointer at the beginning of the function.  The
	 generated stack slot may not be a valid memory address, so we
	 have to check it and fix it if necessary.  */
      start_sequence ();
      emit_move_insn (validize_mem (ret), virtual_incoming_args_rtx);
      seq = get_insns ();
      end_sequence ();

      push_topmost_sequence ();
      emit_insn_after (seq, get_insns ());
      pop_topmost_sequence ();
    }

  return ret;
}

/* Extend a vector that records the INSN_UIDs of INSNS
   (a list of one or more insns).  */

static void
record_insns (rtx insns, varray_type *vecp)
{
  int i, len;
  rtx tmp;

  tmp = insns;
  len = 0;
  while (tmp != NULL_RTX)
    {
      len++;
      tmp = NEXT_INSN (tmp);
    }

  i = VARRAY_SIZE (*vecp);
  VARRAY_GROW (*vecp, i + len);
  tmp = insns;
  while (tmp != NULL_RTX)
    {
      VARRAY_INT (*vecp, i) = INSN_UID (tmp);
      i++;
      tmp = NEXT_INSN (tmp);
    }
}

/* Set the locator of the insn chain starting at INSN to LOC.  */
static void
set_insn_locators (rtx insn, int loc)
{
  while (insn != NULL_RTX)
    {
      if (INSN_P (insn))
	INSN_LOCATOR (insn) = loc;
      insn = NEXT_INSN (insn);
    }
}

/* Determine how many INSN_UIDs in VEC are part of INSN.  Because we can
   be running after reorg, SEQUENCE rtl is possible.  */

static int
contains (rtx insn, varray_type vec)
{
  int i, j;

  if (GET_CODE (insn) == INSN
      && GET_CODE (PATTERN (insn)) == SEQUENCE)
    {
      int count = 0;
      for (i = XVECLEN (PATTERN (insn), 0) - 1; i >= 0; i--)
	for (j = VARRAY_SIZE (vec) - 1; j >= 0; --j)
	  if (INSN_UID (XVECEXP (PATTERN (insn), 0, i)) == VARRAY_INT (vec, j))
	    count++;
      return count;
    }
  else
    {
      for (j = VARRAY_SIZE (vec) - 1; j >= 0; --j)
	if (INSN_UID (insn) == VARRAY_INT (vec, j))
	  return 1;
    }
  return 0;
}

int
prologue_epilogue_contains (rtx insn)
{
  if (contains (insn, prologue))
    return 1;
  if (contains (insn, epilogue))
    return 1;
  return 0;
}

int
sibcall_epilogue_contains (rtx insn)
{
  if (sibcall_epilogue)
    return contains (insn, sibcall_epilogue);
  return 0;
}

#ifdef HAVE_return
/* Insert gen_return at the end of block BB.  This also means updating
   block_for_insn appropriately.  */

static void
emit_return_into_block (basic_block bb, rtx line_note)
{
  emit_jump_insn_after (gen_return (), BB_END (bb));
  if (line_note)
    emit_note_copy_after (line_note, PREV_INSN (BB_END (bb)));
}
#endif /* HAVE_return */

#if defined(HAVE_epilogue) && defined(INCOMING_RETURN_ADDR_RTX)

/* These functions convert the epilogue into a variant that does not modify the
   stack pointer.  This is used in cases where a function returns an object
   whose size is not known until it is computed.  The called function leaves the
   object on the stack, leaves the stack depressed, and returns a pointer to
   the object.

   What we need to do is track all modifications and references to the stack
   pointer, deleting the modifications and changing the references to point to
   the location the stack pointer would have pointed to had the modifications
   taken place.

   These functions need to be portable so we need to make as few assumptions
   about the epilogue as we can.  However, the epilogue basically contains
   three things: instructions to reset the stack pointer, instructions to
   reload registers, possibly including the frame pointer, and an
   instruction to return to the caller.

   If we can't be sure of what a relevant epilogue insn is doing, we abort.
   We also make no attempt to validate the insns we make since if they are
   invalid, we probably can't do anything valid.  The intent is that these
   routines get "smarter" as more and more machines start to use them and
   they try operating on different epilogues.

   We use the following structure to track what the part of the epilogue that
   we've already processed has done.  We keep two copies of the SP equivalence,
   one for use during the insn we are processing and one for use in the next
   insn.  The difference is because one part of a PARALLEL may adjust SP
   and the other may use it.  */

struct epi_info
{
  rtx sp_equiv_reg;		/* REG that SP is set from, perhaps SP.  */
  HOST_WIDE_INT sp_offset;	/* Offset from SP_EQUIV_REG of present SP.  */
  rtx new_sp_equiv_reg;		/* REG to be used at end of insn.  */
  HOST_WIDE_INT new_sp_offset;	/* Offset to be used at end of insn.  */
  rtx equiv_reg_src;		/* If nonzero, the value that SP_EQUIV_REG
				   should be set to once we no longer need
				   its value.  */
  rtx const_equiv[FIRST_PSEUDO_REGISTER]; /* Any known constant equivalences
					     for registers.  */
};

static void handle_epilogue_set (rtx, struct epi_info *);
static void update_epilogue_consts (rtx, rtx, void *);
static void emit_equiv_load (struct epi_info *);

/* Modify INSN, a list of one or more insns that is part of the epilogue, to
   no modifications to the stack pointer.  Return the new list of insns.  */

static rtx
keep_stack_depressed (rtx insns)
{
  int j;
  struct epi_info info;
  rtx insn, next;

  /* If the epilogue is just a single instruction, it must be OK as is.  */
  if (NEXT_INSN (insns) == NULL_RTX)
    return insns;

  /* Otherwise, start a sequence, initialize the information we have, and
     process all the insns we were given.  */
  start_sequence ();

  info.sp_equiv_reg = stack_pointer_rtx;
  info.sp_offset = 0;
  info.equiv_reg_src = 0;

  for (j = 0; j < FIRST_PSEUDO_REGISTER; j++)
    info.const_equiv[j] = 0;

  insn = insns;
  next = NULL_RTX;
  while (insn != NULL_RTX)
    {
      next = NEXT_INSN (insn);

      if (!INSN_P (insn))
	{
	  add_insn (insn);
	  insn = next;
	  continue;
	}

      /* If this insn references the register that SP is equivalent to and
	 we have a pending load to that register, we must force out the load
	 first and then indicate we no longer know what SP's equivalent is.  */
      if (info.equiv_reg_src != 0
	  && reg_referenced_p (info.sp_equiv_reg, PATTERN (insn)))
	{
	  emit_equiv_load (&info);
	  info.sp_equiv_reg = 0;
	}

      info.new_sp_equiv_reg = info.sp_equiv_reg;
      info.new_sp_offset = info.sp_offset;

      /* If this is a (RETURN) and the return address is on the stack,
	 update the address and change to an indirect jump.  */
      if (GET_CODE (PATTERN (insn)) == RETURN
	  || (GET_CODE (PATTERN (insn)) == PARALLEL
	      && GET_CODE (XVECEXP (PATTERN (insn), 0, 0)) == RETURN))
	{
	  rtx retaddr = INCOMING_RETURN_ADDR_RTX;
	  rtx base = 0;
	  HOST_WIDE_INT offset = 0;
	  rtx jump_insn, jump_set;

	  /* If the return address is in a register, we can emit the insn
	     unchanged.  Otherwise, it must be a MEM and we see what the
	     base register and offset are.  In any case, we have to emit any
	     pending load to the equivalent reg of SP, if any.  */
	  if (GET_CODE (retaddr) == REG)
	    {
	      emit_equiv_load (&info);
	      add_insn (insn);
	      insn = next;
	      continue;
	    }
	  else if (GET_CODE (retaddr) == MEM
		   && GET_CODE (XEXP (retaddr, 0)) == REG)
	    base = gen_rtx_REG (Pmode, REGNO (XEXP (retaddr, 0))), offset = 0;
	  else if (GET_CODE (retaddr) == MEM
		   && GET_CODE (XEXP (retaddr, 0)) == PLUS
		   && GET_CODE (XEXP (XEXP (retaddr, 0), 0)) == REG
		   && GET_CODE (XEXP (XEXP (retaddr, 0), 1)) == CONST_INT)
	    {
	      base = gen_rtx_REG (Pmode, REGNO (XEXP (XEXP (retaddr, 0), 0)));
	      offset = INTVAL (XEXP (XEXP (retaddr, 0), 1));
	    }
	  else
	    abort ();

	  /* If the base of the location containing the return pointer
	     is SP, we must update it with the replacement address.  Otherwise,
	     just build the necessary MEM.  */
	  retaddr = plus_constant (base, offset);
	  if (base == stack_pointer_rtx)
	    retaddr = simplify_replace_rtx (retaddr, stack_pointer_rtx,
					    plus_constant (info.sp_equiv_reg,
							   info.sp_offset));

	  retaddr = gen_rtx_MEM (Pmode, retaddr);

	  /* If there is a pending load to the equivalent register for SP
	     and we reference that register, we must load our address into
	     a scratch register and then do that load.  */
	  if (info.equiv_reg_src
	      && reg_overlap_mentioned_p (info.equiv_reg_src, retaddr))
	    {
	      unsigned int regno;
	      rtx reg;

	      for (regno = 0; regno < FIRST_PSEUDO_REGISTER; regno++)
		if (HARD_REGNO_MODE_OK (regno, Pmode)
		    && !fixed_regs[regno]
		    && TEST_HARD_REG_BIT (regs_invalidated_by_call, regno)
		    && !REGNO_REG_SET_P (EXIT_BLOCK_PTR->global_live_at_start,
					 regno)
		    && !refers_to_regno_p (regno,
					   regno + HARD_REGNO_NREGS (regno,
								     Pmode),
					   info.equiv_reg_src, NULL)
		    && info.const_equiv[regno] == 0)
		  break;

	      if (regno == FIRST_PSEUDO_REGISTER)
		abort ();

	      reg = gen_rtx_REG (Pmode, regno);
	      emit_move_insn (reg, retaddr);
	      retaddr = reg;
	    }

	  emit_equiv_load (&info);
	  jump_insn = emit_jump_insn (gen_indirect_jump (retaddr));

	  /* Show the SET in the above insn is a RETURN.  */
	  jump_set = single_set (jump_insn);
	  if (jump_set == 0)
	    abort ();
	  else
	    SET_IS_RETURN_P (jump_set) = 1;
	}

      /* If SP is not mentioned in the pattern and its equivalent register, if
	 any, is not modified, just emit it.  Otherwise, if neither is set,
	 replace the reference to SP and emit the insn.  If none of those are
	 true, handle each SET individually.  */
      else if (!reg_mentioned_p (stack_pointer_rtx, PATTERN (insn))
	       && (info.sp_equiv_reg == stack_pointer_rtx
		   || !reg_set_p (info.sp_equiv_reg, insn)))
	add_insn (insn);
      else if (! reg_set_p (stack_pointer_rtx, insn)
	       && (info.sp_equiv_reg == stack_pointer_rtx
		   || !reg_set_p (info.sp_equiv_reg, insn)))
	{
	  if (! validate_replace_rtx (stack_pointer_rtx,
				      plus_constant (info.sp_equiv_reg,
						     info.sp_offset),
				      insn))
	    abort ();

	  add_insn (insn);
	}
      else if (GET_CODE (PATTERN (insn)) == SET)
	handle_epilogue_set (PATTERN (insn), &info);
      else if (GET_CODE (PATTERN (insn)) == PARALLEL)
	{
	  for (j = 0; j < XVECLEN (PATTERN (insn), 0); j++)
	    if (GET_CODE (XVECEXP (PATTERN (insn), 0, j)) == SET)
	      handle_epilogue_set (XVECEXP (PATTERN (insn), 0, j), &info);
	}
      else
	add_insn (insn);

      info.sp_equiv_reg = info.new_sp_equiv_reg;
      info.sp_offset = info.new_sp_offset;

      /* Now update any constants this insn sets.  */
      note_stores (PATTERN (insn), update_epilogue_consts, &info);
      insn = next;
    }

  insns = get_insns ();
  end_sequence ();
  return insns;
}

/* SET is a SET from an insn in the epilogue.  P is a pointer to the epi_info
   structure that contains information about what we've seen so far.  We
   process this SET by either updating that data or by emitting one or
   more insns.  */

static void
handle_epilogue_set (rtx set, struct epi_info *p)
{
  /* First handle the case where we are setting SP.  Record what it is being
     set from.  If unknown, abort.  */
  if (reg_set_p (stack_pointer_rtx, set))
    {
      if (SET_DEST (set) != stack_pointer_rtx)
	abort ();

      if (GET_CODE (SET_SRC (set)) == PLUS)
	{
	  p->new_sp_equiv_reg = XEXP (SET_SRC (set), 0);
	  if (GET_CODE (XEXP (SET_SRC (set), 1)) == CONST_INT)
	    p->new_sp_offset = INTVAL (XEXP (SET_SRC (set), 1));
	  else if (GET_CODE (XEXP (SET_SRC (set), 1)) == REG
		   && REGNO (XEXP (SET_SRC (set), 1)) < FIRST_PSEUDO_REGISTER
		   && p->const_equiv[REGNO (XEXP (SET_SRC (set), 1))] != 0)
	    p->new_sp_offset
	      = INTVAL (p->const_equiv[REGNO (XEXP (SET_SRC (set), 1))]);
	  else
	    abort ();
	}
      else
	p->new_sp_equiv_reg = SET_SRC (set), p->new_sp_offset = 0;

      /* If we are adjusting SP, we adjust from the old data.  */
      if (p->new_sp_equiv_reg == stack_pointer_rtx)
	{
	  p->new_sp_equiv_reg = p->sp_equiv_reg;
	  p->new_sp_offset += p->sp_offset;
	}

      if (p->new_sp_equiv_reg == 0 || GET_CODE (p->new_sp_equiv_reg) != REG)
	abort ();

      return;
    }

  /* Next handle the case where we are setting SP's equivalent register.
     If we already have a value to set it to, abort.  We could update, but
     there seems little point in handling that case.  Note that we have
     to allow for the case where we are setting the register set in
     the previous part of a PARALLEL inside a single insn.  But use the
     old offset for any updates within this insn.  We must allow for the case
     where the register is being set in a different (usually wider) mode than
     Pmode).  */
  else if (p->new_sp_equiv_reg != 0 && reg_set_p (p->new_sp_equiv_reg, set))
    {
      if (p->equiv_reg_src != 0
	  || GET_CODE (p->new_sp_equiv_reg) != REG
	  || GET_CODE (SET_DEST (set)) != REG
	  || GET_MODE_BITSIZE (GET_MODE (SET_DEST (set))) > BITS_PER_WORD
	  || REGNO (p->new_sp_equiv_reg) != REGNO (SET_DEST (set)))
	abort ();
      else
	p->equiv_reg_src
	  = simplify_replace_rtx (SET_SRC (set), stack_pointer_rtx,
				  plus_constant (p->sp_equiv_reg,
						 p->sp_offset));
    }

  /* Otherwise, replace any references to SP in the insn to its new value
     and emit the insn.  */
  else
    {
      SET_SRC (set) = simplify_replace_rtx (SET_SRC (set), stack_pointer_rtx,
					    plus_constant (p->sp_equiv_reg,
							   p->sp_offset));
      SET_DEST (set) = simplify_replace_rtx (SET_DEST (set), stack_pointer_rtx,
					     plus_constant (p->sp_equiv_reg,
							    p->sp_offset));
      emit_insn (set);
    }
}

/* Update the tracking information for registers set to constants.  */

static void
update_epilogue_consts (rtx dest, rtx x, void *data)
{
  struct epi_info *p = (struct epi_info *) data;

  if (GET_CODE (dest) != REG || REGNO (dest) >= FIRST_PSEUDO_REGISTER)
    return;
  else if (GET_CODE (x) == CLOBBER || ! rtx_equal_p (dest, SET_DEST (x))
	   || GET_CODE (SET_SRC (x)) != CONST_INT)
    p->const_equiv[REGNO (dest)] = 0;
  else
    p->const_equiv[REGNO (dest)] = SET_SRC (x);
}

/* Emit an insn to do the load shown in p->equiv_reg_src, if needed.  */

static void
emit_equiv_load (struct epi_info *p)
{
  if (p->equiv_reg_src != 0)
    {
      rtx dest = p->sp_equiv_reg;

      if (GET_MODE (p->equiv_reg_src) != GET_MODE (dest))
	dest = gen_rtx_REG (GET_MODE (p->equiv_reg_src),
			    REGNO (p->sp_equiv_reg));

      emit_move_insn (dest, p->equiv_reg_src);
      p->equiv_reg_src = 0;
    }
}
#endif

/* Generate the prologue and epilogue RTL if the machine supports it.  Thread
   this into place with notes indicating where the prologue ends and where
   the epilogue begins.  Update the basic block information when possible.  */

void
thread_prologue_and_epilogue_insns (rtx f ATTRIBUTE_UNUSED)
{
  int inserted = 0;
  edge e;
#if defined (HAVE_sibcall_epilogue) || defined (HAVE_epilogue) || defined (HAVE_return) || defined (HAVE_prologue)
  rtx seq;
#endif
#ifdef HAVE_prologue
  rtx prologue_end = NULL_RTX;
#endif
#if defined (HAVE_epilogue) || defined(HAVE_return)
  rtx epilogue_end = NULL_RTX;
#endif

#ifdef HAVE_prologue
  if (HAVE_prologue)
    {
      start_sequence ();
      seq = gen_prologue ();
      emit_insn (seq);

      /* Retain a map of the prologue insns.  */
      record_insns (seq, &prologue);
      prologue_end = emit_note (NOTE_INSN_PROLOGUE_END);

      seq = get_insns ();
      end_sequence ();
      set_insn_locators (seq, prologue_locator);

      /* Can't deal with multiple successors of the entry block
         at the moment.  Function should always have at least one
         entry point.  */
      if (!ENTRY_BLOCK_PTR->succ || ENTRY_BLOCK_PTR->succ->succ_next)
	abort ();

      insert_insn_on_edge (seq, ENTRY_BLOCK_PTR->succ);
      inserted = 1;
    }
#endif

  /* If the exit block has no non-fake predecessors, we don't need
     an epilogue.  */
  for (e = EXIT_BLOCK_PTR->pred; e; e = e->pred_next)
    if ((e->flags & EDGE_FAKE) == 0)
      break;
  if (e == NULL)
    goto epilogue_done;

#ifdef HAVE_return
  if (optimize && HAVE_return)
    {
      /* If we're allowed to generate a simple return instruction,
	 then by definition we don't need a full epilogue.  Examine
	 the block that falls through to EXIT.   If it does not
	 contain any code, examine its predecessors and try to
	 emit (conditional) return instructions.  */

      basic_block last;
      edge e_next;
      rtx label;

      for (e = EXIT_BLOCK_PTR->pred; e; e = e->pred_next)
	if (e->flags & EDGE_FALLTHRU)
	  break;
      if (e == NULL)
	goto epilogue_done;
      last = e->src;

      /* Verify that there are no active instructions in the last block.  */
      label = BB_END (last);
      while (label && GET_CODE (label) != CODE_LABEL)
	{
	  if (active_insn_p (label))
	    break;
	  label = PREV_INSN (label);
	}

      if (BB_HEAD (last) == label && GET_CODE (label) == CODE_LABEL)
	{
	  rtx epilogue_line_note = NULL_RTX;

	  /* Locate the line number associated with the closing brace,
	     if we can find one.  */
	  for (seq = get_last_insn ();
	       seq && ! active_insn_p (seq);
	       seq = PREV_INSN (seq))
	    if (GET_CODE (seq) == NOTE && NOTE_LINE_NUMBER (seq) > 0)
	      {
		epilogue_line_note = seq;
		break;
	      }

	  for (e = last->pred; e; e = e_next)
	    {
	      basic_block bb = e->src;
	      rtx jump;

	      e_next = e->pred_next;
	      if (bb == ENTRY_BLOCK_PTR)
		continue;

	      jump = BB_END (bb);
	      if ((GET_CODE (jump) != JUMP_INSN) || JUMP_LABEL (jump) != label)
		continue;

	      /* If we have an unconditional jump, we can replace that
		 with a simple return instruction.  */
	      if (simplejump_p (jump))
		{
		  emit_return_into_block (bb, epilogue_line_note);
		  delete_insn (jump);
		}

	      /* If we have a conditional jump, we can try to replace
		 that with a conditional return instruction.  */
	      else if (condjump_p (jump))
		{
		  if (! redirect_jump (jump, 0, 0))
		    continue;

		  /* If this block has only one successor, it both jumps
		     and falls through to the fallthru block, so we can't
		     delete the edge.  */
		  if (bb->succ->succ_next == NULL)
		    continue;
		}
	      else
		continue;

	      /* Fix up the CFG for the successful change we just made.  */
	      redirect_edge_succ (e, EXIT_BLOCK_PTR);
	    }

	  /* Emit a return insn for the exit fallthru block.  Whether
	     this is still reachable will be determined later.  */

	  emit_barrier_after (BB_END (last));
	  emit_return_into_block (last, epilogue_line_note);
	  epilogue_end = BB_END (last);
	  last->succ->flags &= ~EDGE_FALLTHRU;
	  goto epilogue_done;
	}
    }
#endif
#ifdef HAVE_epilogue
  if (HAVE_epilogue)
    {
      /* Find the edge that falls through to EXIT.  Other edges may exist
	 due to RETURN instructions, but those don't need epilogues.
	 There really shouldn't be a mixture -- either all should have
	 been converted or none, however...  */

      for (e = EXIT_BLOCK_PTR->pred; e; e = e->pred_next)
	if (e->flags & EDGE_FALLTHRU)
	  break;
      if (e == NULL)
	goto epilogue_done;

      start_sequence ();
      epilogue_end = emit_note (NOTE_INSN_EPILOGUE_BEG);

      seq = gen_epilogue ();

#ifdef INCOMING_RETURN_ADDR_RTX
      /* If this function returns with the stack depressed and we can support
	 it, massage the epilogue to actually do that.  */
      if (TREE_CODE (TREE_TYPE (current_function_decl)) == FUNCTION_TYPE
	  && TYPE_RETURNS_STACK_DEPRESSED (TREE_TYPE (current_function_decl)))
	seq = keep_stack_depressed (seq);
#endif

      emit_jump_insn (seq);

      /* Retain a map of the epilogue insns.  */
      record_insns (seq, &epilogue);
      set_insn_locators (seq, epilogue_locator);

      seq = get_insns ();
      end_sequence ();

      insert_insn_on_edge (seq, e);
      inserted = 1;
    }
#endif
epilogue_done:

  if (inserted)
    commit_edge_insertions ();

#ifdef HAVE_sibcall_epilogue
  /* Emit sibling epilogues before any sibling call sites.  */
  for (e = EXIT_BLOCK_PTR->pred; e; e = e->pred_next)
    {
      basic_block bb = e->src;
      rtx insn = BB_END (bb);
      rtx i;
      rtx newinsn;

      if (GET_CODE (insn) != CALL_INSN
	  || ! SIBLING_CALL_P (insn))
	continue;

      start_sequence ();
      emit_insn (gen_sibcall_epilogue ());
      seq = get_insns ();
      end_sequence ();

      /* Retain a map of the epilogue insns.  Used in life analysis to
	 avoid getting rid of sibcall epilogue insns.  Do this before we
	 actually emit the sequence.  */
      record_insns (seq, &sibcall_epilogue);
      set_insn_locators (seq, epilogue_locator);

      i = PREV_INSN (insn);
      newinsn = emit_insn_before (seq, insn);
    }
#endif

#ifdef HAVE_prologue
  /* This is probably all useless now that we use locators.  */
  if (prologue_end)
    {
      rtx insn, prev;

      /* GDB handles `break f' by setting a breakpoint on the first
	 line note after the prologue.  Which means (1) that if
	 there are line number notes before where we inserted the
	 prologue we should move them, and (2) we should generate a
	 note before the end of the first basic block, if there isn't
	 one already there.

	 ??? This behavior is completely broken when dealing with
	 multiple entry functions.  We simply place the note always
	 into first basic block and let alternate entry points
	 to be missed.
       */

      for (insn = prologue_end; insn; insn = prev)
	{
	  prev = PREV_INSN (insn);
	  if (GET_CODE (insn) == NOTE && NOTE_LINE_NUMBER (insn) > 0)
	    {
	      /* Note that we cannot reorder the first insn in the
		 chain, since rest_of_compilation relies on that
		 remaining constant.  */
	      if (prev == NULL)
		break;
	      reorder_insns (insn, insn, prologue_end);
	    }
	}

      /* Find the last line number note in the first block.  */
      for (insn = BB_END (ENTRY_BLOCK_PTR->next_bb);
	   insn != prologue_end && insn;
	   insn = PREV_INSN (insn))
	if (GET_CODE (insn) == NOTE && NOTE_LINE_NUMBER (insn) > 0)
	  break;

      /* If we didn't find one, make a copy of the first line number
	 we run across.  */
      if (! insn)
	{
	  for (insn = next_active_insn (prologue_end);
	       insn;
	       insn = PREV_INSN (insn))
	    if (GET_CODE (insn) == NOTE && NOTE_LINE_NUMBER (insn) > 0)
	      {
		emit_note_copy_after (insn, prologue_end);
		break;
	      }
	}
    }
#endif
#ifdef HAVE_epilogue
  if (epilogue_end)
    {
      rtx insn, next;

      /* Similarly, move any line notes that appear after the epilogue.
         There is no need, however, to be quite so anal about the existence
	 of such a note.  Also move the NOTE_INSN_FUNCTION_END and (possibly)
	 NOTE_INSN_FUNCTION_BEG notes, as those can be relevant for debug
	 info generation.  */
      for (insn = epilogue_end; insn; insn = next)
	{
	  next = NEXT_INSN (insn);
	  if (GET_CODE (insn) == NOTE 
	      && (NOTE_LINE_NUMBER (insn) > 0
		  || NOTE_LINE_NUMBER (insn) == NOTE_INSN_FUNCTION_BEG
		  || NOTE_LINE_NUMBER (insn) == NOTE_INSN_FUNCTION_END))
	    reorder_insns (insn, insn, PREV_INSN (epilogue_end));
	}
    }
#endif
}

/* Reposition the prologue-end and epilogue-begin notes after instruction
   scheduling and delayed branch scheduling.  */

void
reposition_prologue_and_epilogue_notes (rtx f ATTRIBUTE_UNUSED)
{
#if defined (HAVE_prologue) || defined (HAVE_epilogue)
  rtx insn, last, note;
  int len;

  if ((len = VARRAY_SIZE (prologue)) > 0)
    {
      last = 0, note = 0;

      /* Scan from the beginning until we reach the last prologue insn.
	 We apparently can't depend on basic_block_{head,end} after
	 reorg has run.  */
      for (insn = f; insn; insn = NEXT_INSN (insn))
	{
	  if (GET_CODE (insn) == NOTE)
	    {
	      if (NOTE_LINE_NUMBER (insn) == NOTE_INSN_PROLOGUE_END)
		note = insn;
	    }
	  else if (contains (insn, prologue))
	    {
	      last = insn;
	      if (--len == 0)
		break;
	    }
	}

      if (last)
	{
	  /* Find the prologue-end note if we haven't already, and
	     move it to just after the last prologue insn.  */
	  if (note == 0)
	    {
	      for (note = last; (note = NEXT_INSN (note));)
		if (GET_CODE (note) == NOTE
		    && NOTE_LINE_NUMBER (note) == NOTE_INSN_PROLOGUE_END)
		  break;
	    }

	  /* Avoid placing note between CODE_LABEL and BASIC_BLOCK note.  */
	  if (GET_CODE (last) == CODE_LABEL)
	    last = NEXT_INSN (last);
	  reorder_insns (note, note, last);
	}
    }

  if ((len = VARRAY_SIZE (epilogue)) > 0)
    {
      last = 0, note = 0;

      /* Scan from the end until we reach the first epilogue insn.
	 We apparently can't depend on basic_block_{head,end} after
	 reorg has run.  */
      for (insn = get_last_insn (); insn; insn = PREV_INSN (insn))
	{
	  if (GET_CODE (insn) == NOTE)
	    {
	      if (NOTE_LINE_NUMBER (insn) == NOTE_INSN_EPILOGUE_BEG)
		note = insn;
	    }
	  else if (contains (insn, epilogue))
	    {
	      last = insn;
	      if (--len == 0)
		break;
	    }
	}

      if (last)
	{
	  /* Find the epilogue-begin note if we haven't already, and
	     move it to just before the first epilogue insn.  */
	  if (note == 0)
	    {
	      for (note = insn; (note = PREV_INSN (note));)
		if (GET_CODE (note) == NOTE
		    && NOTE_LINE_NUMBER (note) == NOTE_INSN_EPILOGUE_BEG)
		  break;
	    }

	  if (PREV_INSN (last) != note)
	    reorder_insns (note, note, PREV_INSN (last));
	}
    }
#endif /* HAVE_prologue or HAVE_epilogue */
}

/* Called once, at initialization, to initialize function.c.  */

void
init_function_once (void)
{
  VARRAY_INT_INIT (prologue, 0, "prologue");
  VARRAY_INT_INIT (epilogue, 0, "epilogue");
  VARRAY_INT_INIT (sibcall_epilogue, 0, "sibcall_epilogue");
}

/* Returns the name of the current function.  */
const char *
current_function_name (void)
{
  return (*lang_hooks.decl_printable_name) (cfun->decl, 2);
}

#include "gt-function.h"
