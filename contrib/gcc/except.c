/* Implements exception handling.
   Copyright (C) 1989, 1992, 1993, 1994, 1995, 1996, 1997, 1998,
   1999, 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.
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


/* An exception is an event that can be signaled from within a
   function. This event can then be "caught" or "trapped" by the
   callers of this function. This potentially allows program flow to
   be transferred to any arbitrary code associated with a function call
   several levels up the stack.

   The intended use for this mechanism is for signaling "exceptional
   events" in an out-of-band fashion, hence its name. The C++ language
   (and many other OO-styled or functional languages) practically
   requires such a mechanism, as otherwise it becomes very difficult
   or even impossible to signal failure conditions in complex
   situations.  The traditional C++ example is when an error occurs in
   the process of constructing an object; without such a mechanism, it
   is impossible to signal that the error occurs without adding global
   state variables and error checks around every object construction.

   The act of causing this event to occur is referred to as "throwing
   an exception". (Alternate terms include "raising an exception" or
   "signaling an exception".) The term "throw" is used because control
   is returned to the callers of the function that is signaling the
   exception, and thus there is the concept of "throwing" the
   exception up the call stack.

   [ Add updated documentation on how to use this.  ]  */


#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "tree.h"
#include "flags.h"
#include "function.h"
#include "expr.h"
#include "libfuncs.h"
#include "insn-config.h"
#include "except.h"
#include "integrate.h"
#include "hard-reg-set.h"
#include "basic-block.h"
#include "output.h"
#include "dwarf2asm.h"
#include "dwarf2out.h"
#include "dwarf2.h"
#include "toplev.h"
#include "hashtab.h"
#include "intl.h"
#include "ggc.h"
#include "tm_p.h"
#include "target.h"
#include "langhooks.h"
#include "cgraph.h"

/* Provide defaults for stuff that may not be defined when using
   sjlj exceptions.  */
#ifndef EH_RETURN_DATA_REGNO
#define EH_RETURN_DATA_REGNO(N) INVALID_REGNUM
#endif


/* Nonzero means enable synchronous exceptions for non-call instructions.  */
int flag_non_call_exceptions;

/* Protect cleanup actions with must-not-throw regions, with a call
   to the given failure handler.  */
tree (*lang_protect_cleanup_actions) (void);

/* Return true if type A catches type B.  */
int (*lang_eh_type_covers) (tree a, tree b);

/* Map a type to a runtime object to match type.  */
tree (*lang_eh_runtime_type) (tree);

/* A hash table of label to region number.  */

struct ehl_map_entry GTY(())
{
  rtx label;
  struct eh_region *region;
};

static GTY(()) int call_site_base;
static GTY ((param_is (union tree_node)))
  htab_t type_to_runtime_map;

/* Describe the SjLj_Function_Context structure.  */
static GTY(()) tree sjlj_fc_type_node;
static int sjlj_fc_call_site_ofs;
static int sjlj_fc_data_ofs;
static int sjlj_fc_personality_ofs;
static int sjlj_fc_lsda_ofs;
static int sjlj_fc_jbuf_ofs;

/* Describes one exception region.  */
struct eh_region GTY(())
{
  /* The immediately surrounding region.  */
  struct eh_region *outer;

  /* The list of immediately contained regions.  */
  struct eh_region *inner;
  struct eh_region *next_peer;

  /* An identifier for this region.  */
  int region_number;

  /* When a region is deleted, its parents inherit the REG_EH_REGION
     numbers already assigned.  */
  bitmap aka;

  /* Each region does exactly one thing.  */
  enum eh_region_type
  {
    ERT_UNKNOWN = 0,
    ERT_CLEANUP,
    ERT_TRY,
    ERT_CATCH,
    ERT_ALLOWED_EXCEPTIONS,
    ERT_MUST_NOT_THROW,
    ERT_THROW,
    ERT_FIXUP
  } type;

  /* Holds the action to perform based on the preceding type.  */
  union eh_region_u {
    /* A list of catch blocks, a surrounding try block,
       and the label for continuing after a catch.  */
    struct eh_region_u_try {
      struct eh_region *catch;
      struct eh_region *last_catch;
      struct eh_region *prev_try;
      rtx continue_label;
    } GTY ((tag ("ERT_TRY"))) try;

    /* The list through the catch handlers, the list of type objects
       matched, and the list of associated filters.  */
    struct eh_region_u_catch {
      struct eh_region *next_catch;
      struct eh_region *prev_catch;
      tree type_list;
      tree filter_list;
    } GTY ((tag ("ERT_CATCH"))) catch;

    /* A tree_list of allowed types.  */
    struct eh_region_u_allowed {
      tree type_list;
      int filter;
    } GTY ((tag ("ERT_ALLOWED_EXCEPTIONS"))) allowed;

    /* The type given by a call to "throw foo();", or discovered
       for a throw.  */
    struct eh_region_u_throw {
      tree type;
    } GTY ((tag ("ERT_THROW"))) throw;

    /* Retain the cleanup expression even after expansion so that
       we can match up fixup regions.  */
    struct eh_region_u_cleanup {
      tree exp;
      struct eh_region *prev_try;
    } GTY ((tag ("ERT_CLEANUP"))) cleanup;

    /* The real region (by expression and by pointer) that fixup code
       should live in.  */
    struct eh_region_u_fixup {
      tree cleanup_exp;
      struct eh_region *real_region;
      bool resolved;
    } GTY ((tag ("ERT_FIXUP"))) fixup;
  } GTY ((desc ("%0.type"))) u;

  /* Entry point for this region's handler before landing pads are built.  */
  rtx label;

  /* Entry point for this region's handler from the runtime eh library.  */
  rtx landing_pad;

  /* Entry point for this region's handler from an inner region.  */
  rtx post_landing_pad;

  /* The RESX insn for handing off control to the next outermost handler,
     if appropriate.  */
  rtx resume;

  /* True if something in this region may throw.  */
  unsigned may_contain_throw : 1;
};

struct call_site_record GTY(())
{
  rtx landing_pad;
  int action;
};

/* Used to save exception status for each function.  */
struct eh_status GTY(())
{
  /* The tree of all regions for this function.  */
  struct eh_region *region_tree;

  /* The same information as an indexable array.  */
  struct eh_region ** GTY ((length ("%h.last_region_number"))) region_array;

  /* The most recently open region.  */
  struct eh_region *cur_region;

  /* This is the region for which we are processing catch blocks.  */
  struct eh_region *try_region;

  rtx filter;
  rtx exc_ptr;

  int built_landing_pads;
  int last_region_number;

  varray_type ttype_data;
  varray_type ehspec_data;
  varray_type action_record_data;

  htab_t GTY ((param_is (struct ehl_map_entry))) exception_handler_label_map;

  struct call_site_record * GTY ((length ("%h.call_site_data_used")))
    call_site_data;
  int call_site_data_used;
  int call_site_data_size;

  rtx ehr_stackadj;
  rtx ehr_handler;
  rtx ehr_label;

  rtx sjlj_fc;
  rtx sjlj_exit_after;
};


static int t2r_eq (const void *, const void *);
static hashval_t t2r_hash (const void *);
static void add_type_for_runtime (tree);
static tree lookup_type_for_runtime (tree);

static struct eh_region *expand_eh_region_end (void);

static rtx get_exception_filter (struct function *);

static void collect_eh_region_array (void);
static void resolve_fixup_regions (void);
static void remove_fixup_regions (void);
static void remove_unreachable_regions (rtx);
static void convert_from_eh_region_ranges_1 (rtx *, int *, int);

static struct eh_region *duplicate_eh_region_1 (struct eh_region *,
						struct inline_remap *);
static void duplicate_eh_region_2 (struct eh_region *, struct eh_region **);
static int ttypes_filter_eq (const void *, const void *);
static hashval_t ttypes_filter_hash (const void *);
static int ehspec_filter_eq (const void *, const void *);
static hashval_t ehspec_filter_hash (const void *);
static int add_ttypes_entry (htab_t, tree);
static int add_ehspec_entry (htab_t, htab_t, tree);
static void assign_filter_values (void);
static void build_post_landing_pads (void);
static void connect_post_landing_pads (void);
static void dw2_build_landing_pads (void);

struct sjlj_lp_info;
static bool sjlj_find_directly_reachable_regions (struct sjlj_lp_info *);
static void sjlj_assign_call_site_values (rtx, struct sjlj_lp_info *);
static void sjlj_mark_call_sites (struct sjlj_lp_info *);
static void sjlj_emit_function_enter (rtx);
static void sjlj_emit_function_exit (void);
static void sjlj_emit_dispatch_table (rtx, struct sjlj_lp_info *);
static void sjlj_build_landing_pads (void);

static hashval_t ehl_hash (const void *);
static int ehl_eq (const void *, const void *);
static void add_ehl_entry (rtx, struct eh_region *);
static void remove_exception_handler_label (rtx);
static void remove_eh_handler (struct eh_region *);
static int for_each_eh_label_1 (void **, void *);

struct reachable_info;

/* The return value of reachable_next_level.  */
enum reachable_code
{
  /* The given exception is not processed by the given region.  */
  RNL_NOT_CAUGHT,
  /* The given exception may need processing by the given region.  */
  RNL_MAYBE_CAUGHT,
  /* The given exception is completely processed by the given region.  */
  RNL_CAUGHT,
  /* The given exception is completely processed by the runtime.  */
  RNL_BLOCKED
};

static int check_handled (tree, tree);
static void add_reachable_handler (struct reachable_info *,
				   struct eh_region *, struct eh_region *);
static enum reachable_code reachable_next_level (struct eh_region *, tree,
						 struct reachable_info *);

static int action_record_eq (const void *, const void *);
static hashval_t action_record_hash (const void *);
static int add_action_record (htab_t, int, int);
static int collect_one_action_chain (htab_t, struct eh_region *);
static int add_call_site (rtx, int);

static void push_uleb128 (varray_type *, unsigned int);
static void push_sleb128 (varray_type *, int);
#ifndef HAVE_AS_LEB128
static int dw2_size_of_call_site_table (void);
static int sjlj_size_of_call_site_table (void);
#endif
static void dw2_output_call_site_table (void);
static void sjlj_output_call_site_table (void);


/* Routine to see if exception handling is turned on.
   DO_WARN is nonzero if we want to inform the user that exception
   handling is turned off.

   This is used to ensure that -fexceptions has been specified if the
   compiler tries to use any exception-specific functions.  */

int
doing_eh (int do_warn)
{
  if (! flag_exceptions)
    {
      static int warned = 0;
      if (! warned && do_warn)
	{
	  error ("exception handling disabled, use -fexceptions to enable");
	  warned = 1;
	}
      return 0;
    }
  return 1;
}


void
init_eh (void)
{
  if (! flag_exceptions)
    return;

  type_to_runtime_map = htab_create_ggc (31, t2r_hash, t2r_eq, NULL);

  /* Create the SjLj_Function_Context structure.  This should match
     the definition in unwind-sjlj.c.  */
  if (USING_SJLJ_EXCEPTIONS)
    {
      tree f_jbuf, f_per, f_lsda, f_prev, f_cs, f_data, tmp;

      sjlj_fc_type_node = (*lang_hooks.types.make_type) (RECORD_TYPE);

      f_prev = build_decl (FIELD_DECL, get_identifier ("__prev"),
			   build_pointer_type (sjlj_fc_type_node));
      DECL_FIELD_CONTEXT (f_prev) = sjlj_fc_type_node;

      f_cs = build_decl (FIELD_DECL, get_identifier ("__call_site"),
			 integer_type_node);
      DECL_FIELD_CONTEXT (f_cs) = sjlj_fc_type_node;

      tmp = build_index_type (build_int_2 (4 - 1, 0));
      tmp = build_array_type ((*lang_hooks.types.type_for_mode) (word_mode, 1),
			      tmp);
      f_data = build_decl (FIELD_DECL, get_identifier ("__data"), tmp);
      DECL_FIELD_CONTEXT (f_data) = sjlj_fc_type_node;

      f_per = build_decl (FIELD_DECL, get_identifier ("__personality"),
			  ptr_type_node);
      DECL_FIELD_CONTEXT (f_per) = sjlj_fc_type_node;

      f_lsda = build_decl (FIELD_DECL, get_identifier ("__lsda"),
			   ptr_type_node);
      DECL_FIELD_CONTEXT (f_lsda) = sjlj_fc_type_node;

#ifdef DONT_USE_BUILTIN_SETJMP
#ifdef JMP_BUF_SIZE
      tmp = build_int_2 (JMP_BUF_SIZE - 1, 0);
#else
      /* Should be large enough for most systems, if it is not,
	 JMP_BUF_SIZE should be defined with the proper value.  It will
	 also tend to be larger than necessary for most systems, a more
	 optimal port will define JMP_BUF_SIZE.  */
      tmp = build_int_2 (FIRST_PSEUDO_REGISTER + 2 - 1, 0);
#endif
#else
      /* builtin_setjmp takes a pointer to 5 words.  */
      tmp = build_int_2 (5 * BITS_PER_WORD / POINTER_SIZE - 1, 0);
#endif
      tmp = build_index_type (tmp);
      tmp = build_array_type (ptr_type_node, tmp);
      f_jbuf = build_decl (FIELD_DECL, get_identifier ("__jbuf"), tmp);
#ifdef DONT_USE_BUILTIN_SETJMP
      /* We don't know what the alignment requirements of the
	 runtime's jmp_buf has.  Overestimate.  */
      DECL_ALIGN (f_jbuf) = BIGGEST_ALIGNMENT;
      DECL_USER_ALIGN (f_jbuf) = 1;
#endif
      DECL_FIELD_CONTEXT (f_jbuf) = sjlj_fc_type_node;

      TYPE_FIELDS (sjlj_fc_type_node) = f_prev;
      TREE_CHAIN (f_prev) = f_cs;
      TREE_CHAIN (f_cs) = f_data;
      TREE_CHAIN (f_data) = f_per;
      TREE_CHAIN (f_per) = f_lsda;
      TREE_CHAIN (f_lsda) = f_jbuf;

      layout_type (sjlj_fc_type_node);

      /* Cache the interesting field offsets so that we have
	 easy access from rtl.  */
      sjlj_fc_call_site_ofs
	= (tree_low_cst (DECL_FIELD_OFFSET (f_cs), 1)
	   + tree_low_cst (DECL_FIELD_BIT_OFFSET (f_cs), 1) / BITS_PER_UNIT);
      sjlj_fc_data_ofs
	= (tree_low_cst (DECL_FIELD_OFFSET (f_data), 1)
	   + tree_low_cst (DECL_FIELD_BIT_OFFSET (f_data), 1) / BITS_PER_UNIT);
      sjlj_fc_personality_ofs
	= (tree_low_cst (DECL_FIELD_OFFSET (f_per), 1)
	   + tree_low_cst (DECL_FIELD_BIT_OFFSET (f_per), 1) / BITS_PER_UNIT);
      sjlj_fc_lsda_ofs
	= (tree_low_cst (DECL_FIELD_OFFSET (f_lsda), 1)
	   + tree_low_cst (DECL_FIELD_BIT_OFFSET (f_lsda), 1) / BITS_PER_UNIT);
      sjlj_fc_jbuf_ofs
	= (tree_low_cst (DECL_FIELD_OFFSET (f_jbuf), 1)
	   + tree_low_cst (DECL_FIELD_BIT_OFFSET (f_jbuf), 1) / BITS_PER_UNIT);
    }
}

void
init_eh_for_function (void)
{
  cfun->eh = ggc_alloc_cleared (sizeof (struct eh_status));
}

/* Start an exception handling region.  All instructions emitted
   after this point are considered to be part of the region until
   expand_eh_region_end is invoked.  */

void
expand_eh_region_start (void)
{
  struct eh_region *new_region;
  struct eh_region *cur_region;
  rtx note;

  if (! doing_eh (0))
    return;

  /* Insert a new blank region as a leaf in the tree.  */
  new_region = ggc_alloc_cleared (sizeof (*new_region));
  cur_region = cfun->eh->cur_region;
  new_region->outer = cur_region;
  if (cur_region)
    {
      new_region->next_peer = cur_region->inner;
      cur_region->inner = new_region;
    }
  else
    {
      new_region->next_peer = cfun->eh->region_tree;
      cfun->eh->region_tree = new_region;
    }
  cfun->eh->cur_region = new_region;

  /* Create a note marking the start of this region.  */
  new_region->region_number = ++cfun->eh->last_region_number;
  note = emit_note (NOTE_INSN_EH_REGION_BEG);
  NOTE_EH_HANDLER (note) = new_region->region_number;
}

/* Common code to end a region.  Returns the region just ended.  */

static struct eh_region *
expand_eh_region_end (void)
{
  struct eh_region *cur_region = cfun->eh->cur_region;
  rtx note;

  /* Create a note marking the end of this region.  */
  note = emit_note (NOTE_INSN_EH_REGION_END);
  NOTE_EH_HANDLER (note) = cur_region->region_number;

  /* Pop.  */
  cfun->eh->cur_region = cur_region->outer;

  return cur_region;
}

/* End an exception handling region for a cleanup.  HANDLER is an
   expression to expand for the cleanup.  */

void
expand_eh_region_end_cleanup (tree handler)
{
  struct eh_region *region;
  tree protect_cleanup_actions;
  rtx around_label;
  rtx data_save[2];

  if (! doing_eh (0))
    return;

  region = expand_eh_region_end ();
  region->type = ERT_CLEANUP;
  region->label = gen_label_rtx ();
  region->u.cleanup.exp = handler;
  region->u.cleanup.prev_try = cfun->eh->try_region;

  around_label = gen_label_rtx ();
  emit_jump (around_label);

  emit_label (region->label);

  if (flag_non_call_exceptions || region->may_contain_throw)
    {
      /* Give the language a chance to specify an action to be taken if an
	 exception is thrown that would propagate out of the HANDLER.  */
      protect_cleanup_actions
	= (lang_protect_cleanup_actions
	   ? (*lang_protect_cleanup_actions) ()
	   : NULL_TREE);

      if (protect_cleanup_actions)
	expand_eh_region_start ();

      /* In case this cleanup involves an inline destructor with a try block in
	 it, we need to save the EH return data registers around it.  */
      data_save[0] = gen_reg_rtx (ptr_mode);
      emit_move_insn (data_save[0], get_exception_pointer (cfun));
      data_save[1] = gen_reg_rtx (word_mode);
      emit_move_insn (data_save[1], get_exception_filter (cfun));

      expand_expr (handler, const0_rtx, VOIDmode, 0);

      emit_move_insn (cfun->eh->exc_ptr, data_save[0]);
      emit_move_insn (cfun->eh->filter, data_save[1]);

      if (protect_cleanup_actions)
	expand_eh_region_end_must_not_throw (protect_cleanup_actions);

      /* We need any stack adjustment complete before the around_label.  */
      do_pending_stack_adjust ();
    }

  /* We delay the generation of the _Unwind_Resume until we generate
     landing pads.  We emit a marker here so as to get good control
     flow data in the meantime.  */
  region->resume
    = emit_jump_insn (gen_rtx_RESX (VOIDmode, region->region_number));
  emit_barrier ();

  emit_label (around_label);
}

/* End an exception handling region for a try block, and prepares
   for subsequent calls to expand_start_catch.  */

void
expand_start_all_catch (void)
{
  struct eh_region *region;

  if (! doing_eh (1))
    return;

  region = expand_eh_region_end ();
  region->type = ERT_TRY;
  region->u.try.prev_try = cfun->eh->try_region;
  region->u.try.continue_label = gen_label_rtx ();

  cfun->eh->try_region = region;

  emit_jump (region->u.try.continue_label);
}

/* Begin a catch clause.  TYPE is the type caught, a list of such types, or
   null if this is a catch-all clause. Providing a type list enables to
   associate the catch region with potentially several exception types, which
   is useful e.g. for Ada.  */

void
expand_start_catch (tree type_or_list)
{
  struct eh_region *t, *c, *l;
  tree type_list;

  if (! doing_eh (0))
    return;

  type_list = type_or_list;

  if (type_or_list)
    {
      /* Ensure to always end up with a type list to normalize further
         processing, then register each type against the runtime types
         map.  */
      tree type_node;

      if (TREE_CODE (type_or_list) != TREE_LIST)
	type_list = tree_cons (NULL_TREE, type_or_list, NULL_TREE);

      type_node = type_list;
      for (; type_node; type_node = TREE_CHAIN (type_node))
	add_type_for_runtime (TREE_VALUE (type_node));
    }

  expand_eh_region_start ();

  t = cfun->eh->try_region;
  c = cfun->eh->cur_region;
  c->type = ERT_CATCH;
  c->u.catch.type_list = type_list;
  c->label = gen_label_rtx ();

  l = t->u.try.last_catch;
  c->u.catch.prev_catch = l;
  if (l)
    l->u.catch.next_catch = c;
  else
    t->u.try.catch = c;
  t->u.try.last_catch = c;

  emit_label (c->label);
}

/* End a catch clause.  Control will resume after the try/catch block.  */

void
expand_end_catch (void)
{
  struct eh_region *try_region;

  if (! doing_eh (0))
    return;

  expand_eh_region_end ();
  try_region = cfun->eh->try_region;

  emit_jump (try_region->u.try.continue_label);
}

/* End a sequence of catch handlers for a try block.  */

void
expand_end_all_catch (void)
{
  struct eh_region *try_region;

  if (! doing_eh (0))
    return;

  try_region = cfun->eh->try_region;
  cfun->eh->try_region = try_region->u.try.prev_try;

  emit_label (try_region->u.try.continue_label);
}

/* End an exception region for an exception type filter.  ALLOWED is a
   TREE_LIST of types to be matched by the runtime.  FAILURE is an
   expression to invoke if a mismatch occurs.

   ??? We could use these semantics for calls to rethrow, too; if we can
   see the surrounding catch clause, we know that the exception we're
   rethrowing satisfies the "filter" of the catch type.  */

void
expand_eh_region_end_allowed (tree allowed, tree failure)
{
  struct eh_region *region;
  rtx around_label;

  if (! doing_eh (0))
    return;

  region = expand_eh_region_end ();
  region->type = ERT_ALLOWED_EXCEPTIONS;
  region->u.allowed.type_list = allowed;
  region->label = gen_label_rtx ();

  for (; allowed ; allowed = TREE_CHAIN (allowed))
    add_type_for_runtime (TREE_VALUE (allowed));

  /* We must emit the call to FAILURE here, so that if this function
     throws a different exception, that it will be processed by the
     correct region.  */

  around_label = gen_label_rtx ();
  emit_jump (around_label);

  emit_label (region->label);
  expand_expr (failure, const0_rtx, VOIDmode, EXPAND_NORMAL);
  /* We must adjust the stack before we reach the AROUND_LABEL because
     the call to FAILURE does not occur on all paths to the
     AROUND_LABEL.  */
  do_pending_stack_adjust ();

  emit_label (around_label);
}

/* End an exception region for a must-not-throw filter.  FAILURE is an
   expression invoke if an uncaught exception propagates this far.

   This is conceptually identical to expand_eh_region_end_allowed with
   an empty allowed list (if you passed "std::terminate" instead of
   "__cxa_call_unexpected"), but they are represented differently in
   the C++ LSDA.  */

void
expand_eh_region_end_must_not_throw (tree failure)
{
  struct eh_region *region;
  rtx around_label;

  if (! doing_eh (0))
    return;

  region = expand_eh_region_end ();
  region->type = ERT_MUST_NOT_THROW;
  region->label = gen_label_rtx ();

  /* We must emit the call to FAILURE here, so that if this function
     throws a different exception, that it will be processed by the
     correct region.  */

  around_label = gen_label_rtx ();
  emit_jump (around_label);

  emit_label (region->label);
  expand_expr (failure, const0_rtx, VOIDmode, EXPAND_NORMAL);

  emit_label (around_label);
}

/* End an exception region for a throw.  No handling goes on here,
   but it's the easiest way for the front-end to indicate what type
   is being thrown.  */

void
expand_eh_region_end_throw (tree type)
{
  struct eh_region *region;

  if (! doing_eh (0))
    return;

  region = expand_eh_region_end ();
  region->type = ERT_THROW;
  region->u.throw.type = type;
}

/* End a fixup region.  Within this region the cleanups for the immediately
   enclosing region are _not_ run.  This is used for goto cleanup to avoid
   destroying an object twice.

   This would be an extraordinarily simple prospect, were it not for the
   fact that we don't actually know what the immediately enclosing region
   is.  This surprising fact is because expand_cleanups is currently
   generating a sequence that it will insert somewhere else.  We collect
   the proper notion of "enclosing" in convert_from_eh_region_ranges.  */

void
expand_eh_region_end_fixup (tree handler)
{
  struct eh_region *fixup;

  if (! doing_eh (0))
    return;

  fixup = expand_eh_region_end ();
  fixup->type = ERT_FIXUP;
  fixup->u.fixup.cleanup_exp = handler;
}

/* Note that the current EH region (if any) may contain a throw, or a
   call to a function which itself may contain a throw.  */

void
note_eh_region_may_contain_throw (void)
{
  struct eh_region *region;

  region = cfun->eh->cur_region;
  while (region && !region->may_contain_throw)
    {
      region->may_contain_throw = 1;
      region = region->outer;
    }
}

/* Return an rtl expression for a pointer to the exception object
   within a handler.  */

rtx
get_exception_pointer (struct function *fun)
{
  rtx exc_ptr = fun->eh->exc_ptr;
  if (fun == cfun && ! exc_ptr)
    {
      exc_ptr = gen_reg_rtx (ptr_mode);
      fun->eh->exc_ptr = exc_ptr;
    }
  return exc_ptr;
}

/* Return an rtl expression for the exception dispatch filter
   within a handler.  */

static rtx
get_exception_filter (struct function *fun)
{
  rtx filter = fun->eh->filter;
  if (fun == cfun && ! filter)
    {
      filter = gen_reg_rtx (word_mode);
      fun->eh->filter = filter;
    }
  return filter;
}

/* This section is for the exception handling specific optimization pass.  */

/* Random access the exception region tree.  It's just as simple to
   collect the regions this way as in expand_eh_region_start, but
   without having to realloc memory.  */

static void
collect_eh_region_array (void)
{
  struct eh_region **array, *i;

  i = cfun->eh->region_tree;
  if (! i)
    return;

  array = ggc_alloc_cleared ((cfun->eh->last_region_number + 1)
			     * sizeof (*array));
  cfun->eh->region_array = array;

  while (1)
    {
      array[i->region_number] = i;

      /* If there are sub-regions, process them.  */
      if (i->inner)
	i = i->inner;
      /* If there are peers, process them.  */
      else if (i->next_peer)
	i = i->next_peer;
      /* Otherwise, step back up the tree to the next peer.  */
      else
	{
	  do {
	    i = i->outer;
	    if (i == NULL)
	      return;
	  } while (i->next_peer == NULL);
	  i = i->next_peer;
	}
    }
}

static void
resolve_one_fixup_region (struct eh_region *fixup)
{
  struct eh_region *cleanup, *real;
  int j, n;

  n = cfun->eh->last_region_number;
  cleanup = 0;

  for (j = 1; j <= n; ++j)
    {
      cleanup = cfun->eh->region_array[j];
      if (cleanup && cleanup->type == ERT_CLEANUP
	  && cleanup->u.cleanup.exp == fixup->u.fixup.cleanup_exp)
	break;
    }
  if (j > n)
    abort ();

  real = cleanup->outer;
  if (real && real->type == ERT_FIXUP)
    {
      if (!real->u.fixup.resolved)
	resolve_one_fixup_region (real);
      real = real->u.fixup.real_region;
    }

  fixup->u.fixup.real_region = real;
  fixup->u.fixup.resolved = true;
}

static void
resolve_fixup_regions (void)
{
  int i, n = cfun->eh->last_region_number;

  for (i = 1; i <= n; ++i)
    {
      struct eh_region *fixup = cfun->eh->region_array[i];

      if (!fixup || fixup->type != ERT_FIXUP || fixup->u.fixup.resolved)
	continue;

      resolve_one_fixup_region (fixup);
    }
}

/* Now that we've discovered what region actually encloses a fixup,
   we can shuffle pointers and remove them from the tree.  */

static void
remove_fixup_regions (void)
{
  int i;
  rtx insn, note;
  struct eh_region *fixup;

  /* Walk the insn chain and adjust the REG_EH_REGION numbers
     for instructions referencing fixup regions.  This is only
     strictly necessary for fixup regions with no parent, but
     doesn't hurt to do it for all regions.  */
  for (insn = get_insns(); insn ; insn = NEXT_INSN (insn))
    if (INSN_P (insn)
	&& (note = find_reg_note (insn, REG_EH_REGION, NULL))
	&& INTVAL (XEXP (note, 0)) > 0
	&& (fixup = cfun->eh->region_array[INTVAL (XEXP (note, 0))])
	&& fixup->type == ERT_FIXUP)
      {
	if (fixup->u.fixup.real_region)
	  XEXP (note, 0) = GEN_INT (fixup->u.fixup.real_region->region_number);
	else
	  remove_note (insn, note);
      }

  /* Remove the fixup regions from the tree.  */
  for (i = cfun->eh->last_region_number; i > 0; --i)
    {
      fixup = cfun->eh->region_array[i];
      if (! fixup)
	continue;

      /* Allow GC to maybe free some memory.  */
      if (fixup->type == ERT_CLEANUP)
	fixup->u.cleanup.exp = NULL_TREE;

      if (fixup->type != ERT_FIXUP)
	continue;

      if (fixup->inner)
	{
	  struct eh_region *parent, *p, **pp;

	  parent = fixup->u.fixup.real_region;

	  /* Fix up the children's parent pointers; find the end of
	     the list.  */
	  for (p = fixup->inner; ; p = p->next_peer)
	    {
	      p->outer = parent;
	      if (! p->next_peer)
		break;
	    }

	  /* In the tree of cleanups, only outer-inner ordering matters.
	     So link the children back in anywhere at the correct level.  */
	  if (parent)
	    pp = &parent->inner;
	  else
	    pp = &cfun->eh->region_tree;
	  p->next_peer = *pp;
	  *pp = fixup->inner;
	  fixup->inner = NULL;
	}

      remove_eh_handler (fixup);
    }
}

/* Remove all regions whose labels are not reachable from insns.  */

static void
remove_unreachable_regions (rtx insns)
{
  int i, *uid_region_num;
  bool *reachable;
  struct eh_region *r;
  rtx insn;

  uid_region_num = xcalloc (get_max_uid (), sizeof(int));
  reachable = xcalloc (cfun->eh->last_region_number + 1, sizeof(bool));

  for (i = cfun->eh->last_region_number; i > 0; --i)
    {
      r = cfun->eh->region_array[i];
      if (!r || r->region_number != i)
	continue;

      if (r->resume)
	{
	  if (uid_region_num[INSN_UID (r->resume)])
	    abort ();
	  uid_region_num[INSN_UID (r->resume)] = i;
	}
      if (r->label)
	{
	  if (uid_region_num[INSN_UID (r->label)])
	    abort ();
	  uid_region_num[INSN_UID (r->label)] = i;
	}
      if (r->type == ERT_TRY && r->u.try.continue_label)
	{
	  if (uid_region_num[INSN_UID (r->u.try.continue_label)])
	    abort ();
	  uid_region_num[INSN_UID (r->u.try.continue_label)] = i;
	}
    }

  for (insn = insns; insn; insn = NEXT_INSN (insn))
    {
      reachable[uid_region_num[INSN_UID (insn)]] = true;

      if (GET_CODE (insn) == CALL_INSN
	  && GET_CODE (PATTERN (insn)) == CALL_PLACEHOLDER)
	for (i = 0; i < 3; i++)
	  {
	    rtx sub = XEXP (PATTERN (insn), i);
	    for (; sub ; sub = NEXT_INSN (sub))
	      reachable[uid_region_num[INSN_UID (sub)]] = true;
	  }
    }

  for (i = cfun->eh->last_region_number; i > 0; --i)
    {
      r = cfun->eh->region_array[i];
      if (r && r->region_number == i && !reachable[i])
	{
	  /* Don't remove ERT_THROW regions if their outer region
	     is reachable.  */
	  if (r->type == ERT_THROW
	      && r->outer
	      && reachable[r->outer->region_number])
	    continue;

	  remove_eh_handler (r);
	}
    }

  free (reachable);
  free (uid_region_num);
}

/* Turn NOTE_INSN_EH_REGION notes into REG_EH_REGION notes for each
   can_throw instruction in the region.  */

static void
convert_from_eh_region_ranges_1 (rtx *pinsns, int *orig_sp, int cur)
{
  int *sp = orig_sp;
  rtx insn, next;

  for (insn = *pinsns; insn ; insn = next)
    {
      next = NEXT_INSN (insn);
      if (GET_CODE (insn) == NOTE)
	{
	  int kind = NOTE_LINE_NUMBER (insn);
	  if (kind == NOTE_INSN_EH_REGION_BEG
	      || kind == NOTE_INSN_EH_REGION_END)
	    {
	      if (kind == NOTE_INSN_EH_REGION_BEG)
		{
		  struct eh_region *r;

		  *sp++ = cur;
		  cur = NOTE_EH_HANDLER (insn);

		  r = cfun->eh->region_array[cur];
		  if (r->type == ERT_FIXUP)
		    {
		      r = r->u.fixup.real_region;
		      cur = r ? r->region_number : 0;
		    }
		  else if (r->type == ERT_CATCH)
		    {
		      r = r->outer;
		      cur = r ? r->region_number : 0;
		    }
		}
	      else
		cur = *--sp;

	      /* Removing the first insn of a CALL_PLACEHOLDER sequence
		 requires extra care to adjust sequence start.  */
	      if (insn == *pinsns)
		*pinsns = next;
	      remove_insn (insn);
	      continue;
	    }
	}
      else if (INSN_P (insn))
	{
	  if (cur > 0
	      && ! find_reg_note (insn, REG_EH_REGION, NULL_RTX)
	      /* Calls can always potentially throw exceptions, unless
		 they have a REG_EH_REGION note with a value of 0 or less.
		 Which should be the only possible kind so far.  */
	      && (GET_CODE (insn) == CALL_INSN
		  /* If we wanted exceptions for non-call insns, then
		     any may_trap_p instruction could throw.  */
		  || (flag_non_call_exceptions
		      && GET_CODE (PATTERN (insn)) != CLOBBER
		      && GET_CODE (PATTERN (insn)) != USE
		      && may_trap_p (PATTERN (insn)))))
	    {
	      REG_NOTES (insn) = alloc_EXPR_LIST (REG_EH_REGION, GEN_INT (cur),
						  REG_NOTES (insn));
	    }

	  if (GET_CODE (insn) == CALL_INSN
	      && GET_CODE (PATTERN (insn)) == CALL_PLACEHOLDER)
	    {
	      convert_from_eh_region_ranges_1 (&XEXP (PATTERN (insn), 0),
					       sp, cur);
	      convert_from_eh_region_ranges_1 (&XEXP (PATTERN (insn), 1),
					       sp, cur);
	      convert_from_eh_region_ranges_1 (&XEXP (PATTERN (insn), 2),
					       sp, cur);
	    }
	}
    }

  if (sp != orig_sp)
    abort ();
}

void
convert_from_eh_region_ranges (void)
{
  int *stack;
  rtx insns;

  collect_eh_region_array ();
  resolve_fixup_regions ();

  stack = xmalloc (sizeof (int) * (cfun->eh->last_region_number + 1));
  insns = get_insns ();
  convert_from_eh_region_ranges_1 (&insns, stack, 0);
  free (stack);

  remove_fixup_regions ();
  remove_unreachable_regions (insns);
}

static void
add_ehl_entry (rtx label, struct eh_region *region)
{
  struct ehl_map_entry **slot, *entry;

  LABEL_PRESERVE_P (label) = 1;

  entry = ggc_alloc (sizeof (*entry));
  entry->label = label;
  entry->region = region;

  slot = (struct ehl_map_entry **)
    htab_find_slot (cfun->eh->exception_handler_label_map, entry, INSERT);

  /* Before landing pad creation, each exception handler has its own
     label.  After landing pad creation, the exception handlers may
     share landing pads.  This is ok, since maybe_remove_eh_handler
     only requires the 1-1 mapping before landing pad creation.  */
  if (*slot && !cfun->eh->built_landing_pads)
    abort ();

  *slot = entry;
}

void
find_exception_handler_labels (void)
{
  int i;

  if (cfun->eh->exception_handler_label_map)
    htab_empty (cfun->eh->exception_handler_label_map);
  else
    {
      /* ??? The expansion factor here (3/2) must be greater than the htab
	 occupancy factor (4/3) to avoid unnecessary resizing.  */
      cfun->eh->exception_handler_label_map
        = htab_create_ggc (cfun->eh->last_region_number * 3 / 2,
			   ehl_hash, ehl_eq, NULL);
    }

  if (cfun->eh->region_tree == NULL)
    return;

  for (i = cfun->eh->last_region_number; i > 0; --i)
    {
      struct eh_region *region = cfun->eh->region_array[i];
      rtx lab;

      if (! region || region->region_number != i)
	continue;
      if (cfun->eh->built_landing_pads)
	lab = region->landing_pad;
      else
	lab = region->label;

      if (lab)
	add_ehl_entry (lab, region);
    }

  /* For sjlj exceptions, need the return label to remain live until
     after landing pad generation.  */
  if (USING_SJLJ_EXCEPTIONS && ! cfun->eh->built_landing_pads)
    add_ehl_entry (return_label, NULL);
}

bool
current_function_has_exception_handlers (void)
{
  int i;

  for (i = cfun->eh->last_region_number; i > 0; --i)
    {
      struct eh_region *region = cfun->eh->region_array[i];

      if (! region || region->region_number != i)
	continue;
      if (region->type != ERT_THROW)
	return true;
    }

  return false;
}

static struct eh_region *
duplicate_eh_region_1 (struct eh_region *o, struct inline_remap *map)
{
  struct eh_region *n = ggc_alloc_cleared (sizeof (struct eh_region));

  n->region_number = o->region_number + cfun->eh->last_region_number;
  n->type = o->type;

  switch (n->type)
    {
    case ERT_CLEANUP:
    case ERT_MUST_NOT_THROW:
      break;

    case ERT_TRY:
      if (o->u.try.continue_label)
	n->u.try.continue_label
	  = get_label_from_map (map,
				CODE_LABEL_NUMBER (o->u.try.continue_label));
      break;

    case ERT_CATCH:
      n->u.catch.type_list = o->u.catch.type_list;
      break;

    case ERT_ALLOWED_EXCEPTIONS:
      n->u.allowed.type_list = o->u.allowed.type_list;
      break;

    case ERT_THROW:
      n->u.throw.type = o->u.throw.type;

    default:
      abort ();
    }

  if (o->label)
    n->label = get_label_from_map (map, CODE_LABEL_NUMBER (o->label));
  if (o->resume)
    {
      n->resume = map->insn_map[INSN_UID (o->resume)];
      if (n->resume == NULL)
	abort ();
    }

  return n;
}

static void
duplicate_eh_region_2 (struct eh_region *o, struct eh_region **n_array)
{
  struct eh_region *n = n_array[o->region_number];

  switch (n->type)
    {
    case ERT_TRY:
      n->u.try.catch = n_array[o->u.try.catch->region_number];
      n->u.try.last_catch = n_array[o->u.try.last_catch->region_number];
      break;

    case ERT_CATCH:
      if (o->u.catch.next_catch)
	n->u.catch.next_catch = n_array[o->u.catch.next_catch->region_number];
      if (o->u.catch.prev_catch)
	n->u.catch.prev_catch = n_array[o->u.catch.prev_catch->region_number];
      break;

    default:
      break;
    }

  if (o->outer)
    n->outer = n_array[o->outer->region_number];
  if (o->inner)
    n->inner = n_array[o->inner->region_number];
  if (o->next_peer)
    n->next_peer = n_array[o->next_peer->region_number];
}

int
duplicate_eh_regions (struct function *ifun, struct inline_remap *map)
{
  int ifun_last_region_number = ifun->eh->last_region_number;
  struct eh_region **n_array, *root, *cur;
  int i;

  if (ifun_last_region_number == 0)
    return 0;

  n_array = xcalloc (ifun_last_region_number + 1, sizeof (*n_array));

  for (i = 1; i <= ifun_last_region_number; ++i)
    {
      cur = ifun->eh->region_array[i];
      if (!cur || cur->region_number != i)
	continue;
      n_array[i] = duplicate_eh_region_1 (cur, map);
    }
  for (i = 1; i <= ifun_last_region_number; ++i)
    {
      cur = ifun->eh->region_array[i];
      if (!cur || cur->region_number != i)
	continue;
      duplicate_eh_region_2 (cur, n_array);
    }

  root = n_array[ifun->eh->region_tree->region_number];
  cur = cfun->eh->cur_region;
  if (cur)
    {
      struct eh_region *p = cur->inner;
      if (p)
	{
	  while (p->next_peer)
	    p = p->next_peer;
	  p->next_peer = root;
	}
      else
	cur->inner = root;

      for (i = 1; i <= ifun_last_region_number; ++i)
	if (n_array[i] && n_array[i]->outer == NULL)
	  n_array[i]->outer = cur;
    }
  else
    {
      struct eh_region *p = cfun->eh->region_tree;
      if (p)
	{
	  while (p->next_peer)
	    p = p->next_peer;
	  p->next_peer = root;
	}
      else
	cfun->eh->region_tree = root;
    }

  free (n_array);

  i = cfun->eh->last_region_number;
  cfun->eh->last_region_number = i + ifun_last_region_number;
  return i;
}


static int
t2r_eq (const void *pentry, const void *pdata)
{
  tree entry = (tree) pentry;
  tree data = (tree) pdata;

  return TREE_PURPOSE (entry) == data;
}

static hashval_t
t2r_hash (const void *pentry)
{
  tree entry = (tree) pentry;
  return TYPE_HASH (TREE_PURPOSE (entry));
}

static void
add_type_for_runtime (tree type)
{
  tree *slot;

  slot = (tree *) htab_find_slot_with_hash (type_to_runtime_map, type,
					    TYPE_HASH (type), INSERT);
  if (*slot == NULL)
    {
      tree runtime = (*lang_eh_runtime_type) (type);
      *slot = tree_cons (type, runtime, NULL_TREE);
    }
}

static tree
lookup_type_for_runtime (tree type)
{
  tree *slot;

  slot = (tree *) htab_find_slot_with_hash (type_to_runtime_map, type,
					    TYPE_HASH (type), NO_INSERT);

  /* We should have always inserted the data earlier.  */
  return TREE_VALUE (*slot);
}


/* Represent an entry in @TTypes for either catch actions
   or exception filter actions.  */
struct ttypes_filter GTY(())
{
  tree t;
  int filter;
};

/* Compare ENTRY (a ttypes_filter entry in the hash table) with DATA
   (a tree) for a @TTypes type node we are thinking about adding.  */

static int
ttypes_filter_eq (const void *pentry, const void *pdata)
{
  const struct ttypes_filter *entry = (const struct ttypes_filter *) pentry;
  tree data = (tree) pdata;

  return entry->t == data;
}

static hashval_t
ttypes_filter_hash (const void *pentry)
{
  const struct ttypes_filter *entry = (const struct ttypes_filter *) pentry;
  return TYPE_HASH (entry->t);
}

/* Compare ENTRY with DATA (both struct ttypes_filter) for a @TTypes
   exception specification list we are thinking about adding.  */
/* ??? Currently we use the type lists in the order given.  Someone
   should put these in some canonical order.  */

static int
ehspec_filter_eq (const void *pentry, const void *pdata)
{
  const struct ttypes_filter *entry = (const struct ttypes_filter *) pentry;
  const struct ttypes_filter *data = (const struct ttypes_filter *) pdata;

  return type_list_equal (entry->t, data->t);
}

/* Hash function for exception specification lists.  */

static hashval_t
ehspec_filter_hash (const void *pentry)
{
  const struct ttypes_filter *entry = (const struct ttypes_filter *) pentry;
  hashval_t h = 0;
  tree list;

  for (list = entry->t; list ; list = TREE_CHAIN (list))
    h = (h << 5) + (h >> 27) + TYPE_HASH (TREE_VALUE (list));
  return h;
}

/* Add TYPE to cfun->eh->ttype_data, using TYPES_HASH to speed
   up the search.  Return the filter value to be used.  */

static int
add_ttypes_entry (htab_t ttypes_hash, tree type)
{
  struct ttypes_filter **slot, *n;

  slot = (struct ttypes_filter **)
    htab_find_slot_with_hash (ttypes_hash, type, TYPE_HASH (type), INSERT);

  if ((n = *slot) == NULL)
    {
      /* Filter value is a 1 based table index.  */

      n = xmalloc (sizeof (*n));
      n->t = type;
      n->filter = VARRAY_ACTIVE_SIZE (cfun->eh->ttype_data) + 1;
      *slot = n;

      VARRAY_PUSH_TREE (cfun->eh->ttype_data, type);
    }

  return n->filter;
}

/* Add LIST to cfun->eh->ehspec_data, using EHSPEC_HASH and TYPES_HASH
   to speed up the search.  Return the filter value to be used.  */

static int
add_ehspec_entry (htab_t ehspec_hash, htab_t ttypes_hash, tree list)
{
  struct ttypes_filter **slot, *n;
  struct ttypes_filter dummy;

  dummy.t = list;
  slot = (struct ttypes_filter **)
    htab_find_slot (ehspec_hash, &dummy, INSERT);

  if ((n = *slot) == NULL)
    {
      /* Filter value is a -1 based byte index into a uleb128 buffer.  */

      n = xmalloc (sizeof (*n));
      n->t = list;
      n->filter = -(VARRAY_ACTIVE_SIZE (cfun->eh->ehspec_data) + 1);
      *slot = n;

      /* Look up each type in the list and encode its filter
	 value as a uleb128.  Terminate the list with 0.  */
      for (; list ; list = TREE_CHAIN (list))
	push_uleb128 (&cfun->eh->ehspec_data,
		      add_ttypes_entry (ttypes_hash, TREE_VALUE (list)));
      VARRAY_PUSH_UCHAR (cfun->eh->ehspec_data, 0);
    }

  return n->filter;
}

/* Generate the action filter values to be used for CATCH and
   ALLOWED_EXCEPTIONS regions.  When using dwarf2 exception regions,
   we use lots of landing pads, and so every type or list can share
   the same filter value, which saves table space.  */

static void
assign_filter_values (void)
{
  int i;
  htab_t ttypes, ehspec;

  VARRAY_TREE_INIT (cfun->eh->ttype_data, 16, "ttype_data");
  VARRAY_UCHAR_INIT (cfun->eh->ehspec_data, 64, "ehspec_data");

  ttypes = htab_create (31, ttypes_filter_hash, ttypes_filter_eq, free);
  ehspec = htab_create (31, ehspec_filter_hash, ehspec_filter_eq, free);

  for (i = cfun->eh->last_region_number; i > 0; --i)
    {
      struct eh_region *r = cfun->eh->region_array[i];

      /* Mind we don't process a region more than once.  */
      if (!r || r->region_number != i)
	continue;

      switch (r->type)
	{
	case ERT_CATCH:
	  /* Whatever type_list is (NULL or true list), we build a list
	     of filters for the region.  */
	  r->u.catch.filter_list = NULL_TREE;

	  if (r->u.catch.type_list != NULL)
	    {
	      /* Get a filter value for each of the types caught and store
		 them in the region's dedicated list.  */
	      tree tp_node = r->u.catch.type_list;

	      for (;tp_node; tp_node = TREE_CHAIN (tp_node))
		{
		  int flt = add_ttypes_entry (ttypes, TREE_VALUE (tp_node));
		  tree flt_node = build_int_2 (flt, 0);

		  r->u.catch.filter_list
		    = tree_cons (NULL_TREE, flt_node, r->u.catch.filter_list);
		}
	    }
	  else
	    {
	      /* Get a filter value for the NULL list also since it will need
		 an action record anyway.  */
	      int flt = add_ttypes_entry (ttypes, NULL);
	      tree flt_node = build_int_2 (flt, 0);

	      r->u.catch.filter_list
		= tree_cons (NULL_TREE, flt_node, r->u.catch.filter_list);
	    }

	  break;

	case ERT_ALLOWED_EXCEPTIONS:
	  r->u.allowed.filter
	    = add_ehspec_entry (ehspec, ttypes, r->u.allowed.type_list);
	  break;

	default:
	  break;
	}
    }

  htab_delete (ttypes);
  htab_delete (ehspec);
}

/* Generate the code to actually handle exceptions, which will follow the
   landing pads.  */

static void
build_post_landing_pads (void)
{
  int i;

  for (i = cfun->eh->last_region_number; i > 0; --i)
    {
      struct eh_region *region = cfun->eh->region_array[i];
      rtx seq;

      /* Mind we don't process a region more than once.  */
      if (!region || region->region_number != i)
	continue;

      switch (region->type)
	{
	case ERT_TRY:
	  /* ??? Collect the set of all non-overlapping catch handlers
	       all the way up the chain until blocked by a cleanup.  */
	  /* ??? Outer try regions can share landing pads with inner
	     try regions if the types are completely non-overlapping,
	     and there are no intervening cleanups.  */

	  region->post_landing_pad = gen_label_rtx ();

	  start_sequence ();

	  emit_label (region->post_landing_pad);

	  /* ??? It is mighty inconvenient to call back into the
	     switch statement generation code in expand_end_case.
	     Rapid prototyping sez a sequence of ifs.  */
	  {
	    struct eh_region *c;
	    for (c = region->u.try.catch; c ; c = c->u.catch.next_catch)
	      {
		if (c->u.catch.type_list == NULL)
		  emit_jump (c->label);
		else
		  {
		    /* Need for one cmp/jump per type caught. Each type
		       list entry has a matching entry in the filter list
		       (see assign_filter_values).  */
		    tree tp_node = c->u.catch.type_list;
		    tree flt_node = c->u.catch.filter_list;

		    for (; tp_node; )
		      {
			emit_cmp_and_jump_insns
			  (cfun->eh->filter,
			   GEN_INT (tree_low_cst (TREE_VALUE (flt_node), 0)),
			   EQ, NULL_RTX, word_mode, 0, c->label);

			tp_node = TREE_CHAIN (tp_node);
			flt_node = TREE_CHAIN (flt_node);
		      }
		  }
	      }
	  }

	  /* We delay the generation of the _Unwind_Resume until we generate
	     landing pads.  We emit a marker here so as to get good control
	     flow data in the meantime.  */
	  region->resume
	    = emit_jump_insn (gen_rtx_RESX (VOIDmode, region->region_number));
	  emit_barrier ();

	  seq = get_insns ();
	  end_sequence ();

	  emit_insn_before (seq, region->u.try.catch->label);
	  break;

	case ERT_ALLOWED_EXCEPTIONS:
	  region->post_landing_pad = gen_label_rtx ();

	  start_sequence ();

	  emit_label (region->post_landing_pad);

	  emit_cmp_and_jump_insns (cfun->eh->filter,
				   GEN_INT (region->u.allowed.filter),
				   EQ, NULL_RTX, word_mode, 0, region->label);

	  /* We delay the generation of the _Unwind_Resume until we generate
	     landing pads.  We emit a marker here so as to get good control
	     flow data in the meantime.  */
	  region->resume
	    = emit_jump_insn (gen_rtx_RESX (VOIDmode, region->region_number));
	  emit_barrier ();

	  seq = get_insns ();
	  end_sequence ();

	  emit_insn_before (seq, region->label);
	  break;

	case ERT_CLEANUP:
	case ERT_MUST_NOT_THROW:
	  region->post_landing_pad = region->label;
	  break;

	case ERT_CATCH:
	case ERT_THROW:
	  /* Nothing to do.  */
	  break;

	default:
	  abort ();
	}
    }
}

/* Replace RESX patterns with jumps to the next handler if any, or calls to
   _Unwind_Resume otherwise.  */

static void
connect_post_landing_pads (void)
{
  int i;

  for (i = cfun->eh->last_region_number; i > 0; --i)
    {
      struct eh_region *region = cfun->eh->region_array[i];
      struct eh_region *outer;
      rtx seq;

      /* Mind we don't process a region more than once.  */
      if (!region || region->region_number != i)
	continue;

      /* If there is no RESX, or it has been deleted by flow, there's
	 nothing to fix up.  */
      if (! region->resume || INSN_DELETED_P (region->resume))
	continue;

      /* Search for another landing pad in this function.  */
      for (outer = region->outer; outer ; outer = outer->outer)
	if (outer->post_landing_pad)
	  break;

      start_sequence ();

      if (outer)
	emit_jump (outer->post_landing_pad);
      else
	emit_library_call (unwind_resume_libfunc, LCT_THROW,
			   VOIDmode, 1, cfun->eh->exc_ptr, ptr_mode);

      seq = get_insns ();
      end_sequence ();
      emit_insn_before (seq, region->resume);
      delete_insn (region->resume);
    }
}


static void
dw2_build_landing_pads (void)
{
  int i;
  unsigned int j;

  for (i = cfun->eh->last_region_number; i > 0; --i)
    {
      struct eh_region *region = cfun->eh->region_array[i];
      rtx seq;
      bool clobbers_hard_regs = false;

      /* Mind we don't process a region more than once.  */
      if (!region || region->region_number != i)
	continue;

      if (region->type != ERT_CLEANUP
	  && region->type != ERT_TRY
	  && region->type != ERT_ALLOWED_EXCEPTIONS)
	continue;

      start_sequence ();

      region->landing_pad = gen_label_rtx ();
      emit_label (region->landing_pad);

#ifdef HAVE_exception_receiver
      if (HAVE_exception_receiver)
	emit_insn (gen_exception_receiver ());
      else
#endif
#ifdef HAVE_nonlocal_goto_receiver
	if (HAVE_nonlocal_goto_receiver)
	  emit_insn (gen_nonlocal_goto_receiver ());
	else
#endif
	  { /* Nothing */ }

      /* If the eh_return data registers are call-saved, then we
	 won't have considered them clobbered from the call that
	 threw.  Kill them now.  */
      for (j = 0; ; ++j)
	{
	  unsigned r = EH_RETURN_DATA_REGNO (j);
	  if (r == INVALID_REGNUM)
	    break;
	  if (! call_used_regs[r])
	    {
	      emit_insn (gen_rtx_CLOBBER (VOIDmode, gen_rtx_REG (Pmode, r)));
	      clobbers_hard_regs = true;
	    }
	}

      if (clobbers_hard_regs)
	{
	  /* @@@ This is a kludge.  Not all machine descriptions define a
	     blockage insn, but we must not allow the code we just generated
	     to be reordered by scheduling.  So emit an ASM_INPUT to act as
	     blockage insn.  */
	  emit_insn (gen_rtx_ASM_INPUT (VOIDmode, ""));
	}

      emit_move_insn (cfun->eh->exc_ptr,
		      gen_rtx_REG (ptr_mode, EH_RETURN_DATA_REGNO (0)));
      emit_move_insn (cfun->eh->filter,
		      gen_rtx_REG (word_mode, EH_RETURN_DATA_REGNO (1)));

      seq = get_insns ();
      end_sequence ();

      emit_insn_before (seq, region->post_landing_pad);
    }
}


struct sjlj_lp_info
{
  int directly_reachable;
  int action_index;
  int dispatch_index;
  int call_site_index;
};

static bool
sjlj_find_directly_reachable_regions (struct sjlj_lp_info *lp_info)
{
  rtx insn;
  bool found_one = false;

  for (insn = get_insns (); insn ; insn = NEXT_INSN (insn))
    {
      struct eh_region *region;
      enum reachable_code rc;
      tree type_thrown;
      rtx note;

      if (! INSN_P (insn))
	continue;

      note = find_reg_note (insn, REG_EH_REGION, NULL_RTX);
      if (!note || INTVAL (XEXP (note, 0)) <= 0)
	continue;

      region = cfun->eh->region_array[INTVAL (XEXP (note, 0))];

      type_thrown = NULL_TREE;
      if (region->type == ERT_THROW)
	{
	  type_thrown = region->u.throw.type;
	  region = region->outer;
	}

      /* Find the first containing region that might handle the exception.
	 That's the landing pad to which we will transfer control.  */
      rc = RNL_NOT_CAUGHT;
      for (; region; region = region->outer)
	{
	  rc = reachable_next_level (region, type_thrown, 0);
	  if (rc != RNL_NOT_CAUGHT)
	    break;
	}
      if (rc == RNL_MAYBE_CAUGHT || rc == RNL_CAUGHT)
	{
	  lp_info[region->region_number].directly_reachable = 1;
	  found_one = true;
	}
    }

  return found_one;
}

static void
sjlj_assign_call_site_values (rtx dispatch_label, struct sjlj_lp_info *lp_info)
{
  htab_t ar_hash;
  int i, index;

  /* First task: build the action table.  */

  VARRAY_UCHAR_INIT (cfun->eh->action_record_data, 64, "action_record_data");
  ar_hash = htab_create (31, action_record_hash, action_record_eq, free);

  for (i = cfun->eh->last_region_number; i > 0; --i)
    if (lp_info[i].directly_reachable)
      {
	struct eh_region *r = cfun->eh->region_array[i];
	r->landing_pad = dispatch_label;
	lp_info[i].action_index = collect_one_action_chain (ar_hash, r);
	if (lp_info[i].action_index != -1)
	  cfun->uses_eh_lsda = 1;
      }

  htab_delete (ar_hash);

  /* Next: assign dispatch values.  In dwarf2 terms, this would be the
     landing pad label for the region.  For sjlj though, there is one
     common landing pad from which we dispatch to the post-landing pads.

     A region receives a dispatch index if it is directly reachable
     and requires in-function processing.  Regions that share post-landing
     pads may share dispatch indices.  */
  /* ??? Post-landing pad sharing doesn't actually happen at the moment
     (see build_post_landing_pads) so we don't bother checking for it.  */

  index = 0;
  for (i = cfun->eh->last_region_number; i > 0; --i)
    if (lp_info[i].directly_reachable)
      lp_info[i].dispatch_index = index++;

  /* Finally: assign call-site values.  If dwarf2 terms, this would be
     the region number assigned by convert_to_eh_region_ranges, but
     handles no-action and must-not-throw differently.  */

  call_site_base = 1;
  for (i = cfun->eh->last_region_number; i > 0; --i)
    if (lp_info[i].directly_reachable)
      {
	int action = lp_info[i].action_index;

	/* Map must-not-throw to otherwise unused call-site index 0.  */
	if (action == -2)
	  index = 0;
	/* Map no-action to otherwise unused call-site index -1.  */
	else if (action == -1)
	  index = -1;
	/* Otherwise, look it up in the table.  */
	else
	  index = add_call_site (GEN_INT (lp_info[i].dispatch_index), action);

	lp_info[i].call_site_index = index;
      }
}

static void
sjlj_mark_call_sites (struct sjlj_lp_info *lp_info)
{
  int last_call_site = -2;
  rtx insn, mem;

  for (insn = get_insns (); insn ; insn = NEXT_INSN (insn))
    {
      struct eh_region *region;
      int this_call_site;
      rtx note, before, p;

      /* Reset value tracking at extended basic block boundaries.  */
      if (GET_CODE (insn) == CODE_LABEL)
	last_call_site = -2;

      if (! INSN_P (insn))
	continue;

      note = find_reg_note (insn, REG_EH_REGION, NULL_RTX);
      if (!note)
	{
	  /* Calls (and trapping insns) without notes are outside any
	     exception handling region in this function.  Mark them as
	     no action.  */
	  if (GET_CODE (insn) == CALL_INSN
	      || (flag_non_call_exceptions
		  && may_trap_p (PATTERN (insn))))
	    this_call_site = -1;
	  else
	    continue;
	}
      else
	{
	  /* Calls that are known to not throw need not be marked.  */
	  if (INTVAL (XEXP (note, 0)) <= 0)
	    continue;

	  region = cfun->eh->region_array[INTVAL (XEXP (note, 0))];
	  this_call_site = lp_info[region->region_number].call_site_index;
	}

      if (this_call_site == last_call_site)
	continue;

      /* Don't separate a call from it's argument loads.  */
      before = insn;
      if (GET_CODE (insn) == CALL_INSN)
	before = find_first_parameter_load (insn, NULL_RTX);

      start_sequence ();
      mem = adjust_address (cfun->eh->sjlj_fc, TYPE_MODE (integer_type_node),
			    sjlj_fc_call_site_ofs);
      emit_move_insn (mem, GEN_INT (this_call_site));
      p = get_insns ();
      end_sequence ();

      emit_insn_before (p, before);
      last_call_site = this_call_site;
    }
}

/* Construct the SjLj_Function_Context.  */

static void
sjlj_emit_function_enter (rtx dispatch_label)
{
  rtx fn_begin, fc, mem, seq;

  fc = cfun->eh->sjlj_fc;

  start_sequence ();

  /* We're storing this libcall's address into memory instead of
     calling it directly.  Thus, we must call assemble_external_libcall
     here, as we can not depend on emit_library_call to do it for us.  */
  assemble_external_libcall (eh_personality_libfunc);
  mem = adjust_address (fc, Pmode, sjlj_fc_personality_ofs);
  emit_move_insn (mem, eh_personality_libfunc);

  mem = adjust_address (fc, Pmode, sjlj_fc_lsda_ofs);
  if (cfun->uses_eh_lsda)
    {
      char buf[20];
      rtx sym;

      ASM_GENERATE_INTERNAL_LABEL (buf, "LLSDA", current_function_funcdef_no);
      sym = gen_rtx_SYMBOL_REF (Pmode, ggc_strdup (buf));
      SYMBOL_REF_FLAGS (sym) = SYMBOL_FLAG_LOCAL;
      emit_move_insn (mem, sym);
    }
  else
    emit_move_insn (mem, const0_rtx);

#ifdef DONT_USE_BUILTIN_SETJMP
  {
    rtx x, note;
    x = emit_library_call_value (setjmp_libfunc, NULL_RTX, LCT_RETURNS_TWICE,
				 TYPE_MODE (integer_type_node), 1,
				 plus_constant (XEXP (fc, 0),
						sjlj_fc_jbuf_ofs), Pmode);

    note = emit_note (NOTE_INSN_EXPECTED_VALUE);
    NOTE_EXPECTED_VALUE (note) = gen_rtx_EQ (VOIDmode, x, const0_rtx);

    emit_cmp_and_jump_insns (x, const0_rtx, NE, 0,
			     TYPE_MODE (integer_type_node), 0, dispatch_label);
  }
#else
  expand_builtin_setjmp_setup (plus_constant (XEXP (fc, 0), sjlj_fc_jbuf_ofs),
			       dispatch_label);
#endif

  emit_library_call (unwind_sjlj_register_libfunc, LCT_NORMAL, VOIDmode,
		     1, XEXP (fc, 0), Pmode);

  seq = get_insns ();
  end_sequence ();

  /* ??? Instead of doing this at the beginning of the function,
     do this in a block that is at loop level 0 and dominates all
     can_throw_internal instructions.  */

  for (fn_begin = get_insns (); ; fn_begin = NEXT_INSN (fn_begin))
    if (GET_CODE (fn_begin) == NOTE
	&& NOTE_LINE_NUMBER (fn_begin) == NOTE_INSN_FUNCTION_BEG)
      break;
  emit_insn_after (seq, fn_begin);
}

/* Call back from expand_function_end to know where we should put
   the call to unwind_sjlj_unregister_libfunc if needed.  */

void
sjlj_emit_function_exit_after (rtx after)
{
  cfun->eh->sjlj_exit_after = after;
}

static void
sjlj_emit_function_exit (void)
{
  rtx seq;

  start_sequence ();

  emit_library_call (unwind_sjlj_unregister_libfunc, LCT_NORMAL, VOIDmode,
		     1, XEXP (cfun->eh->sjlj_fc, 0), Pmode);

  seq = get_insns ();
  end_sequence ();

  /* ??? Really this can be done in any block at loop level 0 that
     post-dominates all can_throw_internal instructions.  This is
     the last possible moment.  */

  emit_insn_after (seq, cfun->eh->sjlj_exit_after);
}

static void
sjlj_emit_dispatch_table (rtx dispatch_label, struct sjlj_lp_info *lp_info)
{
  int i, first_reachable;
  rtx mem, dispatch, seq, fc;

  fc = cfun->eh->sjlj_fc;

  start_sequence ();

  emit_label (dispatch_label);

#ifndef DONT_USE_BUILTIN_SETJMP
  expand_builtin_setjmp_receiver (dispatch_label);
#endif

  /* Load up dispatch index, exc_ptr and filter values from the
     function context.  */
  mem = adjust_address (fc, TYPE_MODE (integer_type_node),
			sjlj_fc_call_site_ofs);
  dispatch = copy_to_reg (mem);

  mem = adjust_address (fc, word_mode, sjlj_fc_data_ofs);
  if (word_mode != ptr_mode)
    {
#ifdef POINTERS_EXTEND_UNSIGNED
      mem = convert_memory_address (ptr_mode, mem);
#else
      mem = convert_to_mode (ptr_mode, mem, 0);
#endif
    }
  emit_move_insn (cfun->eh->exc_ptr, mem);

  mem = adjust_address (fc, word_mode, sjlj_fc_data_ofs + UNITS_PER_WORD);
  emit_move_insn (cfun->eh->filter, mem);

  /* Jump to one of the directly reachable regions.  */
  /* ??? This really ought to be using a switch statement.  */

  first_reachable = 0;
  for (i = cfun->eh->last_region_number; i > 0; --i)
    {
      if (! lp_info[i].directly_reachable)
	continue;

      if (! first_reachable)
	{
	  first_reachable = i;
	  continue;
	}

      emit_cmp_and_jump_insns (dispatch, GEN_INT (lp_info[i].dispatch_index),
			       EQ, NULL_RTX, TYPE_MODE (integer_type_node), 0,
			       cfun->eh->region_array[i]->post_landing_pad);
    }

  seq = get_insns ();
  end_sequence ();

  emit_insn_before (seq, (cfun->eh->region_array[first_reachable]
			  ->post_landing_pad));
}

static void
sjlj_build_landing_pads (void)
{
  struct sjlj_lp_info *lp_info;

  lp_info = xcalloc (cfun->eh->last_region_number + 1,
		     sizeof (struct sjlj_lp_info));

  if (sjlj_find_directly_reachable_regions (lp_info))
    {
      rtx dispatch_label = gen_label_rtx ();

      cfun->eh->sjlj_fc
	= assign_stack_local (TYPE_MODE (sjlj_fc_type_node),
			      int_size_in_bytes (sjlj_fc_type_node),
			      TYPE_ALIGN (sjlj_fc_type_node));

      sjlj_assign_call_site_values (dispatch_label, lp_info);
      sjlj_mark_call_sites (lp_info);

      sjlj_emit_function_enter (dispatch_label);
      sjlj_emit_dispatch_table (dispatch_label, lp_info);
      sjlj_emit_function_exit ();
    }

  free (lp_info);
}

void
finish_eh_generation (void)
{
  /* Nothing to do if no regions created.  */
  if (cfun->eh->region_tree == NULL)
    return;

  /* The object here is to provide find_basic_blocks with detailed
     information (via reachable_handlers) on how exception control
     flows within the function.  In this first pass, we can include
     type information garnered from ERT_THROW and ERT_ALLOWED_EXCEPTIONS
     regions, and hope that it will be useful in deleting unreachable
     handlers.  Subsequently, we will generate landing pads which will
     connect many of the handlers, and then type information will not
     be effective.  Still, this is a win over previous implementations.  */

  cleanup_cfg (CLEANUP_PRE_LOOP | CLEANUP_NO_INSN_DEL);

  /* These registers are used by the landing pads.  Make sure they
     have been generated.  */
  get_exception_pointer (cfun);
  get_exception_filter (cfun);

  /* Construct the landing pads.  */

  assign_filter_values ();
  build_post_landing_pads ();
  connect_post_landing_pads ();
  if (USING_SJLJ_EXCEPTIONS)
    sjlj_build_landing_pads ();
  else
    dw2_build_landing_pads ();

  cfun->eh->built_landing_pads = 1;

  /* We've totally changed the CFG.  Start over.  */
  find_exception_handler_labels ();
  rebuild_jump_labels (get_insns ());
  find_basic_blocks (get_insns (), max_reg_num (), 0);
  cleanup_cfg (CLEANUP_PRE_LOOP | CLEANUP_NO_INSN_DEL);
}

static hashval_t
ehl_hash (const void *pentry)
{
  struct ehl_map_entry *entry = (struct ehl_map_entry *) pentry;

  /* 2^32 * ((sqrt(5) - 1) / 2) */
  const hashval_t scaled_golden_ratio = 0x9e3779b9;
  return CODE_LABEL_NUMBER (entry->label) * scaled_golden_ratio;
}

static int
ehl_eq (const void *pentry, const void *pdata)
{
  struct ehl_map_entry *entry = (struct ehl_map_entry *) pentry;
  struct ehl_map_entry *data = (struct ehl_map_entry *) pdata;

  return entry->label == data->label;
}

/* This section handles removing dead code for flow.  */

/* Remove LABEL from exception_handler_label_map.  */

static void
remove_exception_handler_label (rtx label)
{
  struct ehl_map_entry **slot, tmp;

  /* If exception_handler_label_map was not built yet,
     there is nothing to do.  */
  if (cfun->eh->exception_handler_label_map == NULL)
    return;

  tmp.label = label;
  slot = (struct ehl_map_entry **)
    htab_find_slot (cfun->eh->exception_handler_label_map, &tmp, NO_INSERT);
  if (! slot)
    abort ();

  htab_clear_slot (cfun->eh->exception_handler_label_map, (void **) slot);
}

/* Splice REGION from the region tree etc.  */

static void
remove_eh_handler (struct eh_region *region)
{
  struct eh_region **pp, **pp_start, *p, *outer, *inner;
  rtx lab;

  /* For the benefit of efficiently handling REG_EH_REGION notes,
     replace this region in the region array with its containing
     region.  Note that previous region deletions may result in
     multiple copies of this region in the array, so we have a
     list of alternate numbers by which we are known.  */

  outer = region->outer;
  cfun->eh->region_array[region->region_number] = outer;
  if (region->aka)
    {
      int i;
      EXECUTE_IF_SET_IN_BITMAP (region->aka, 0, i,
	{ cfun->eh->region_array[i] = outer; });
    }

  if (outer)
    {
      if (!outer->aka)
        outer->aka = BITMAP_GGC_ALLOC ();
      if (region->aka)
	bitmap_a_or_b (outer->aka, outer->aka, region->aka);
      bitmap_set_bit (outer->aka, region->region_number);
    }

  if (cfun->eh->built_landing_pads)
    lab = region->landing_pad;
  else
    lab = region->label;
  if (lab)
    remove_exception_handler_label (lab);

  if (outer)
    pp_start = &outer->inner;
  else
    pp_start = &cfun->eh->region_tree;
  for (pp = pp_start, p = *pp; p != region; pp = &p->next_peer, p = *pp)
    continue;
  *pp = region->next_peer;

  inner = region->inner;
  if (inner)
    {
      for (p = inner; p->next_peer ; p = p->next_peer)
	p->outer = outer;
      p->outer = outer;

      p->next_peer = *pp_start;
      *pp_start = inner;
    }

  if (region->type == ERT_CATCH)
    {
      struct eh_region *try, *next, *prev;

      for (try = region->next_peer;
	   try->type == ERT_CATCH;
	   try = try->next_peer)
	continue;
      if (try->type != ERT_TRY)
	abort ();

      next = region->u.catch.next_catch;
      prev = region->u.catch.prev_catch;

      if (next)
	next->u.catch.prev_catch = prev;
      else
	try->u.try.last_catch = prev;
      if (prev)
	prev->u.catch.next_catch = next;
      else
	{
	  try->u.try.catch = next;
	  if (! next)
	    remove_eh_handler (try);
	}
    }
}

/* LABEL heads a basic block that is about to be deleted.  If this
   label corresponds to an exception region, we may be able to
   delete the region.  */

void
maybe_remove_eh_handler (rtx label)
{
  struct ehl_map_entry **slot, tmp;
  struct eh_region *region;

  /* ??? After generating landing pads, it's not so simple to determine
     if the region data is completely unused.  One must examine the
     landing pad and the post landing pad, and whether an inner try block
     is referencing the catch handlers directly.  */
  if (cfun->eh->built_landing_pads)
    return;

  tmp.label = label;
  slot = (struct ehl_map_entry **)
    htab_find_slot (cfun->eh->exception_handler_label_map, &tmp, NO_INSERT);
  if (! slot)
    return;
  region = (*slot)->region;
  if (! region)
    return;

  /* Flow will want to remove MUST_NOT_THROW regions as unreachable
     because there is no path to the fallback call to terminate.
     But the region continues to affect call-site data until there
     are no more contained calls, which we don't see here.  */
  if (region->type == ERT_MUST_NOT_THROW)
    {
      htab_clear_slot (cfun->eh->exception_handler_label_map, (void **) slot);
      region->label = NULL_RTX;
    }
  else
    remove_eh_handler (region);
}

/* Invokes CALLBACK for every exception handler label.  Only used by old
   loop hackery; should not be used by new code.  */

void
for_each_eh_label (void (*callback) (rtx))
{
  htab_traverse (cfun->eh->exception_handler_label_map, for_each_eh_label_1,
		 (void *) &callback);
}

static int
for_each_eh_label_1 (void **pentry, void *data)
{
  struct ehl_map_entry *entry = *(struct ehl_map_entry **)pentry;
  void (*callback) (rtx) = *(void (**) (rtx)) data;

  (*callback) (entry->label);
  return 1;
}

/* This section describes CFG exception edges for flow.  */

/* For communicating between calls to reachable_next_level.  */
struct reachable_info GTY(())
{
  tree types_caught;
  tree types_allowed;
  rtx handlers;
};

/* A subroutine of reachable_next_level.  Return true if TYPE, or a
   base class of TYPE, is in HANDLED.  */

static int
check_handled (tree handled, tree type)
{
  tree t;

  /* We can check for exact matches without front-end help.  */
  if (! lang_eh_type_covers)
    {
      for (t = handled; t ; t = TREE_CHAIN (t))
	if (TREE_VALUE (t) == type)
	  return 1;
    }
  else
    {
      for (t = handled; t ; t = TREE_CHAIN (t))
	if ((*lang_eh_type_covers) (TREE_VALUE (t), type))
	  return 1;
    }

  return 0;
}

/* A subroutine of reachable_next_level.  If we are collecting a list
   of handlers, add one.  After landing pad generation, reference
   it instead of the handlers themselves.  Further, the handlers are
   all wired together, so by referencing one, we've got them all.
   Before landing pad generation we reference each handler individually.

   LP_REGION contains the landing pad; REGION is the handler.  */

static void
add_reachable_handler (struct reachable_info *info, struct eh_region *lp_region, struct eh_region *region)
{
  if (! info)
    return;

  if (cfun->eh->built_landing_pads)
    {
      if (! info->handlers)
	info->handlers = alloc_INSN_LIST (lp_region->landing_pad, NULL_RTX);
    }
  else
    info->handlers = alloc_INSN_LIST (region->label, info->handlers);
}

/* Process one level of exception regions for reachability.
   If TYPE_THROWN is non-null, then it is the *exact* type being
   propagated.  If INFO is non-null, then collect handler labels
   and caught/allowed type information between invocations.  */

static enum reachable_code
reachable_next_level (struct eh_region *region, tree type_thrown,
		      struct reachable_info *info)
{
  switch (region->type)
    {
    case ERT_CLEANUP:
      /* Before landing-pad generation, we model control flow
	 directly to the individual handlers.  In this way we can
	 see that catch handler types may shadow one another.  */
      add_reachable_handler (info, region, region);
      return RNL_MAYBE_CAUGHT;

    case ERT_TRY:
      {
	struct eh_region *c;
	enum reachable_code ret = RNL_NOT_CAUGHT;

	for (c = region->u.try.catch; c ; c = c->u.catch.next_catch)
	  {
	    /* A catch-all handler ends the search.  */
	    if (c->u.catch.type_list == NULL)
	      {
		add_reachable_handler (info, region, c);
		return RNL_CAUGHT;
	      }

	    if (type_thrown)
	      {
		/* If we have at least one type match, end the search.  */
		tree tp_node = c->u.catch.type_list;

		for (; tp_node; tp_node = TREE_CHAIN (tp_node))
		  {
		    tree type = TREE_VALUE (tp_node);

		    if (type == type_thrown
			|| (lang_eh_type_covers
			    && (*lang_eh_type_covers) (type, type_thrown)))
		      {
			add_reachable_handler (info, region, c);
			return RNL_CAUGHT;
		      }
		  }

		/* If we have definitive information of a match failure,
		   the catch won't trigger.  */
		if (lang_eh_type_covers)
		  return RNL_NOT_CAUGHT;
	      }

	    /* At this point, we either don't know what type is thrown or
	       don't have front-end assistance to help deciding if it is
	       covered by one of the types in the list for this region.

	       We'd then like to add this region to the list of reachable
	       handlers since it is indeed potentially reachable based on the
	       information we have.

	       Actually, this handler is for sure not reachable if all the
	       types it matches have already been caught. That is, it is only
	       potentially reachable if at least one of the types it catches
	       has not been previously caught.  */

	    if (! info)
	      ret = RNL_MAYBE_CAUGHT;
	    else
	      {
		tree tp_node = c->u.catch.type_list;
		bool maybe_reachable = false;

		/* Compute the potential reachability of this handler and
		   update the list of types caught at the same time.  */
		for (; tp_node; tp_node = TREE_CHAIN (tp_node))
		  {
		    tree type = TREE_VALUE (tp_node);

		    if (! check_handled (info->types_caught, type))
		      {
			info->types_caught
			  = tree_cons (NULL, type, info->types_caught);

			maybe_reachable = true;
		      }
		  }

		if (maybe_reachable)
		  {
		    add_reachable_handler (info, region, c);

		    /* ??? If the catch type is a base class of every allowed
		       type, then we know we can stop the search.  */
		    ret = RNL_MAYBE_CAUGHT;
		  }
	      }
	  }

	return ret;
      }

    case ERT_ALLOWED_EXCEPTIONS:
      /* An empty list of types definitely ends the search.  */
      if (region->u.allowed.type_list == NULL_TREE)
	{
	  add_reachable_handler (info, region, region);
	  return RNL_CAUGHT;
	}

      /* Collect a list of lists of allowed types for use in detecting
	 when a catch may be transformed into a catch-all.  */
      if (info)
	info->types_allowed = tree_cons (NULL_TREE,
					 region->u.allowed.type_list,
					 info->types_allowed);

      /* If we have definitive information about the type hierarchy,
	 then we can tell if the thrown type will pass through the
	 filter.  */
      if (type_thrown && lang_eh_type_covers)
	{
	  if (check_handled (region->u.allowed.type_list, type_thrown))
	    return RNL_NOT_CAUGHT;
	  else
	    {
	      add_reachable_handler (info, region, region);
	      return RNL_CAUGHT;
	    }
	}

      add_reachable_handler (info, region, region);
      return RNL_MAYBE_CAUGHT;

    case ERT_CATCH:
      /* Catch regions are handled by their controlling try region.  */
      return RNL_NOT_CAUGHT;

    case ERT_MUST_NOT_THROW:
      /* Here we end our search, since no exceptions may propagate.
	 If we've touched down at some landing pad previous, then the
	 explicit function call we generated may be used.  Otherwise
	 the call is made by the runtime.  */
      if (info && info->handlers)
	{
	  add_reachable_handler (info, region, region);
	  return RNL_CAUGHT;
	}
      else
	return RNL_BLOCKED;

    case ERT_THROW:
    case ERT_FIXUP:
    case ERT_UNKNOWN:
      /* Shouldn't see these here.  */
      break;
    }

  abort ();
}

/* Retrieve a list of labels of exception handlers which can be
   reached by a given insn.  */

rtx
reachable_handlers (rtx insn)
{
  struct reachable_info info;
  struct eh_region *region;
  tree type_thrown;
  int region_number;

  if (GET_CODE (insn) == JUMP_INSN
      && GET_CODE (PATTERN (insn)) == RESX)
    region_number = XINT (PATTERN (insn), 0);
  else
    {
      rtx note = find_reg_note (insn, REG_EH_REGION, NULL_RTX);
      if (!note || INTVAL (XEXP (note, 0)) <= 0)
	return NULL;
      region_number = INTVAL (XEXP (note, 0));
    }

  memset (&info, 0, sizeof (info));

  region = cfun->eh->region_array[region_number];

  type_thrown = NULL_TREE;
  if (GET_CODE (insn) == JUMP_INSN
      && GET_CODE (PATTERN (insn)) == RESX)
    {
      /* A RESX leaves a region instead of entering it.  Thus the
	 region itself may have been deleted out from under us.  */
      if (region == NULL)
	return NULL;
      region = region->outer;
    }
  else if (region->type == ERT_THROW)
    {
      type_thrown = region->u.throw.type;
      region = region->outer;
    }

  while (region)
    {
      if (reachable_next_level (region, type_thrown, &info) >= RNL_CAUGHT)
	break;
      /* If we have processed one cleanup, there is no point in
	 processing any more of them.  Each cleanup will have an edge
	 to the next outer cleanup region, so the flow graph will be
	 accurate.  */
      if (region->type == ERT_CLEANUP)
	region = region->u.cleanup.prev_try;
      else
	region = region->outer;
    }

  return info.handlers;
}

/* Determine if the given INSN can throw an exception that is caught
   within the function.  */

bool
can_throw_internal (rtx insn)
{
  struct eh_region *region;
  tree type_thrown;
  rtx note;

  if (! INSN_P (insn))
    return false;

  if (GET_CODE (insn) == INSN
      && GET_CODE (PATTERN (insn)) == SEQUENCE)
    insn = XVECEXP (PATTERN (insn), 0, 0);

  if (GET_CODE (insn) == CALL_INSN
      && GET_CODE (PATTERN (insn)) == CALL_PLACEHOLDER)
    {
      int i;
      for (i = 0; i < 3; ++i)
	{
	  rtx sub = XEXP (PATTERN (insn), i);
	  for (; sub ; sub = NEXT_INSN (sub))
	    if (can_throw_internal (sub))
	      return true;
	}
      return false;
    }

  /* Every insn that might throw has an EH_REGION note.  */
  note = find_reg_note (insn, REG_EH_REGION, NULL_RTX);
  if (!note || INTVAL (XEXP (note, 0)) <= 0)
    return false;

  region = cfun->eh->region_array[INTVAL (XEXP (note, 0))];

  type_thrown = NULL_TREE;
  if (region->type == ERT_THROW)
    {
      type_thrown = region->u.throw.type;
      region = region->outer;
    }

  /* If this exception is ignored by each and every containing region,
     then control passes straight out.  The runtime may handle some
     regions, which also do not require processing internally.  */
  for (; region; region = region->outer)
    {
      enum reachable_code how = reachable_next_level (region, type_thrown, 0);
      if (how == RNL_BLOCKED)
	return false;
      if (how != RNL_NOT_CAUGHT)
	return true;
    }

  return false;
}

/* Determine if the given INSN can throw an exception that is
   visible outside the function.  */

bool
can_throw_external (rtx insn)
{
  struct eh_region *region;
  tree type_thrown;
  rtx note;

  if (! INSN_P (insn))
    return false;

  if (GET_CODE (insn) == INSN
      && GET_CODE (PATTERN (insn)) == SEQUENCE)
    insn = XVECEXP (PATTERN (insn), 0, 0);

  if (GET_CODE (insn) == CALL_INSN
      && GET_CODE (PATTERN (insn)) == CALL_PLACEHOLDER)
    {
      int i;
      for (i = 0; i < 3; ++i)
	{
	  rtx sub = XEXP (PATTERN (insn), i);
	  for (; sub ; sub = NEXT_INSN (sub))
	    if (can_throw_external (sub))
	      return true;
	}
      return false;
    }

  note = find_reg_note (insn, REG_EH_REGION, NULL_RTX);
  if (!note)
    {
      /* Calls (and trapping insns) without notes are outside any
	 exception handling region in this function.  We have to
	 assume it might throw.  Given that the front end and middle
	 ends mark known NOTHROW functions, this isn't so wildly
	 inaccurate.  */
      return (GET_CODE (insn) == CALL_INSN
	      || (flag_non_call_exceptions
		  && may_trap_p (PATTERN (insn))));
    }
  if (INTVAL (XEXP (note, 0)) <= 0)
    return false;

  region = cfun->eh->region_array[INTVAL (XEXP (note, 0))];

  type_thrown = NULL_TREE;
  if (region->type == ERT_THROW)
    {
      type_thrown = region->u.throw.type;
      region = region->outer;
    }

  /* If the exception is caught or blocked by any containing region,
     then it is not seen by any calling function.  */
  for (; region ; region = region->outer)
    if (reachable_next_level (region, type_thrown, NULL) >= RNL_CAUGHT)
      return false;

  return true;
}

/* Set current_function_nothrow and cfun->all_throwers_are_sibcalls.  */

void
set_nothrow_function_flags (void)
{
  rtx insn;

  current_function_nothrow = 1;

  /* Assume cfun->all_throwers_are_sibcalls until we encounter
     something that can throw an exception.  We specifically exempt
     CALL_INSNs that are SIBLING_CALL_P, as these are really jumps,
     and can't throw.  Most CALL_INSNs are not SIBLING_CALL_P, so this
     is optimistic.  */

  cfun->all_throwers_are_sibcalls = 1;

  if (! flag_exceptions)
    return;

  for (insn = get_insns (); insn; insn = NEXT_INSN (insn))
    if (can_throw_external (insn))
      {
	current_function_nothrow = 0;

	if (GET_CODE (insn) != CALL_INSN || !SIBLING_CALL_P (insn))
	  {
	    cfun->all_throwers_are_sibcalls = 0;
	    return;
	  }
      }

  for (insn = current_function_epilogue_delay_list; insn;
       insn = XEXP (insn, 1))
    if (can_throw_external (insn))
      {
	current_function_nothrow = 0;

	if (GET_CODE (insn) != CALL_INSN || !SIBLING_CALL_P (insn))
	  {
	    cfun->all_throwers_are_sibcalls = 0;
	    return;
	  }
      }
}


/* Various hooks for unwind library.  */

/* Do any necessary initialization to access arbitrary stack frames.
   On the SPARC, this means flushing the register windows.  */

void
expand_builtin_unwind_init (void)
{
  /* Set this so all the registers get saved in our frame; we need to be
     able to copy the saved values for any registers from frames we unwind.  */
  current_function_has_nonlocal_label = 1;

#ifdef SETUP_FRAME_ADDRESSES
  SETUP_FRAME_ADDRESSES ();
#endif
}

rtx
expand_builtin_eh_return_data_regno (tree arglist)
{
  tree which = TREE_VALUE (arglist);
  unsigned HOST_WIDE_INT iwhich;

  if (TREE_CODE (which) != INTEGER_CST)
    {
      error ("argument of `__builtin_eh_return_regno' must be constant");
      return constm1_rtx;
    }

  iwhich = tree_low_cst (which, 1);
  iwhich = EH_RETURN_DATA_REGNO (iwhich);
  if (iwhich == INVALID_REGNUM)
    return constm1_rtx;

#ifdef DWARF_FRAME_REGNUM
  iwhich = DWARF_FRAME_REGNUM (iwhich);
#else
  iwhich = DBX_REGISTER_NUMBER (iwhich);
#endif

  return GEN_INT (iwhich);
}

/* Given a value extracted from the return address register or stack slot,
   return the actual address encoded in that value.  */

rtx
expand_builtin_extract_return_addr (tree addr_tree)
{
  rtx addr = expand_expr (addr_tree, NULL_RTX, Pmode, 0);

  if (GET_MODE (addr) != Pmode
      && GET_MODE (addr) != VOIDmode)
    {
#ifdef POINTERS_EXTEND_UNSIGNED
      addr = convert_memory_address (Pmode, addr);
#else
      addr = convert_to_mode (Pmode, addr, 0);
#endif
    }

  /* First mask out any unwanted bits.  */
#ifdef MASK_RETURN_ADDR
  expand_and (Pmode, addr, MASK_RETURN_ADDR, addr);
#endif

  /* Then adjust to find the real return address.  */
#if defined (RETURN_ADDR_OFFSET)
  addr = plus_constant (addr, RETURN_ADDR_OFFSET);
#endif

  return addr;
}

/* Given an actual address in addr_tree, do any necessary encoding
   and return the value to be stored in the return address register or
   stack slot so the epilogue will return to that address.  */

rtx
expand_builtin_frob_return_addr (tree addr_tree)
{
  rtx addr = expand_expr (addr_tree, NULL_RTX, ptr_mode, 0);

  addr = convert_memory_address (Pmode, addr);

#ifdef RETURN_ADDR_OFFSET
  addr = force_reg (Pmode, addr);
  addr = plus_constant (addr, -RETURN_ADDR_OFFSET);
#endif

  return addr;
}

/* Set up the epilogue with the magic bits we'll need to return to the
   exception handler.  */

void
expand_builtin_eh_return (tree stackadj_tree ATTRIBUTE_UNUSED,
			  tree handler_tree)
{
  rtx tmp;

#ifdef EH_RETURN_STACKADJ_RTX
  tmp = expand_expr (stackadj_tree, cfun->eh->ehr_stackadj, VOIDmode, 0);
  tmp = convert_memory_address (Pmode, tmp);
  if (!cfun->eh->ehr_stackadj)
    cfun->eh->ehr_stackadj = copy_to_reg (tmp);
  else if (tmp != cfun->eh->ehr_stackadj)
    emit_move_insn (cfun->eh->ehr_stackadj, tmp);
#endif

  tmp = expand_expr (handler_tree, cfun->eh->ehr_handler, VOIDmode, 0);
  tmp = convert_memory_address (Pmode, tmp);
  if (!cfun->eh->ehr_handler)
    cfun->eh->ehr_handler = copy_to_reg (tmp);
  else if (tmp != cfun->eh->ehr_handler)
    emit_move_insn (cfun->eh->ehr_handler, tmp);

  if (!cfun->eh->ehr_label)
    cfun->eh->ehr_label = gen_label_rtx ();
  emit_jump (cfun->eh->ehr_label);
}

void
expand_eh_return (void)
{
  rtx around_label;

  if (! cfun->eh->ehr_label)
    return;

  current_function_calls_eh_return = 1;

#ifdef EH_RETURN_STACKADJ_RTX
  emit_move_insn (EH_RETURN_STACKADJ_RTX, const0_rtx);
#endif

  around_label = gen_label_rtx ();
  emit_jump (around_label);

  emit_label (cfun->eh->ehr_label);
  clobber_return_register ();

#ifdef EH_RETURN_STACKADJ_RTX
  emit_move_insn (EH_RETURN_STACKADJ_RTX, cfun->eh->ehr_stackadj);
#endif

#ifdef HAVE_eh_return
  if (HAVE_eh_return)
    emit_insn (gen_eh_return (cfun->eh->ehr_handler));
  else
#endif
    {
#ifdef EH_RETURN_HANDLER_RTX
      emit_move_insn (EH_RETURN_HANDLER_RTX, cfun->eh->ehr_handler);
#else
      error ("__builtin_eh_return not supported on this target");
#endif
    }

  emit_label (around_label);
}

/* Convert a ptr_mode address ADDR_TREE to a Pmode address controlled by
   POINTERS_EXTEND_UNSIGNED and return it.  */

rtx
expand_builtin_extend_pointer (tree addr_tree)
{
  rtx addr = expand_expr (addr_tree, NULL_RTX, ptr_mode, 0);
  int extend;

#ifdef POINTERS_EXTEND_UNSIGNED
  extend = POINTERS_EXTEND_UNSIGNED;
#else
  /* The previous EH code did an unsigned extend by default, so we do this also
     for consistency.  */
  extend = 1;
#endif

  return convert_modes (word_mode, ptr_mode, addr, extend);
}

/* In the following functions, we represent entries in the action table
   as 1-based indices.  Special cases are:

	 0:	null action record, non-null landing pad; implies cleanups
	-1:	null action record, null landing pad; implies no action
	-2:	no call-site entry; implies must_not_throw
	-3:	we have yet to process outer regions

   Further, no special cases apply to the "next" field of the record.
   For next, 0 means end of list.  */

struct action_record
{
  int offset;
  int filter;
  int next;
};

static int
action_record_eq (const void *pentry, const void *pdata)
{
  const struct action_record *entry = (const struct action_record *) pentry;
  const struct action_record *data = (const struct action_record *) pdata;
  return entry->filter == data->filter && entry->next == data->next;
}

static hashval_t
action_record_hash (const void *pentry)
{
  const struct action_record *entry = (const struct action_record *) pentry;
  return entry->next * 1009 + entry->filter;
}

static int
add_action_record (htab_t ar_hash, int filter, int next)
{
  struct action_record **slot, *new, tmp;

  tmp.filter = filter;
  tmp.next = next;
  slot = (struct action_record **) htab_find_slot (ar_hash, &tmp, INSERT);

  if ((new = *slot) == NULL)
    {
      new = xmalloc (sizeof (*new));
      new->offset = VARRAY_ACTIVE_SIZE (cfun->eh->action_record_data) + 1;
      new->filter = filter;
      new->next = next;
      *slot = new;

      /* The filter value goes in untouched.  The link to the next
	 record is a "self-relative" byte offset, or zero to indicate
	 that there is no next record.  So convert the absolute 1 based
	 indices we've been carrying around into a displacement.  */

      push_sleb128 (&cfun->eh->action_record_data, filter);
      if (next)
	next -= VARRAY_ACTIVE_SIZE (cfun->eh->action_record_data) + 1;
      push_sleb128 (&cfun->eh->action_record_data, next);
    }

  return new->offset;
}

static int
collect_one_action_chain (htab_t ar_hash, struct eh_region *region)
{
  struct eh_region *c;
  int next;

  /* If we've reached the top of the region chain, then we have
     no actions, and require no landing pad.  */
  if (region == NULL)
    return -1;

  switch (region->type)
    {
    case ERT_CLEANUP:
      /* A cleanup adds a zero filter to the beginning of the chain, but
	 there are special cases to look out for.  If there are *only*
	 cleanups along a path, then it compresses to a zero action.
	 Further, if there are multiple cleanups along a path, we only
	 need to represent one of them, as that is enough to trigger
	 entry to the landing pad at runtime.  */
      next = collect_one_action_chain (ar_hash, region->outer);
      if (next <= 0)
	return 0;
      for (c = region->outer; c ; c = c->outer)
	if (c->type == ERT_CLEANUP)
	  return next;
      return add_action_record (ar_hash, 0, next);

    case ERT_TRY:
      /* Process the associated catch regions in reverse order.
	 If there's a catch-all handler, then we don't need to
	 search outer regions.  Use a magic -3 value to record
	 that we haven't done the outer search.  */
      next = -3;
      for (c = region->u.try.last_catch; c ; c = c->u.catch.prev_catch)
	{
	  if (c->u.catch.type_list == NULL)
	    {
	      /* Retrieve the filter from the head of the filter list
		 where we have stored it (see assign_filter_values).  */
	      int filter
		= TREE_INT_CST_LOW (TREE_VALUE (c->u.catch.filter_list));

	      next = add_action_record (ar_hash, filter, 0);
	    }
	  else
	    {
	      /* Once the outer search is done, trigger an action record for
                 each filter we have.  */
	      tree flt_node;

	      if (next == -3)
		{
		  next = collect_one_action_chain (ar_hash, region->outer);

		  /* If there is no next action, terminate the chain.  */
		  if (next == -1)
		    next = 0;
		  /* If all outer actions are cleanups or must_not_throw,
		     we'll have no action record for it, since we had wanted
		     to encode these states in the call-site record directly.
		     Add a cleanup action to the chain to catch these.  */
		  else if (next <= 0)
		    next = add_action_record (ar_hash, 0, 0);
		}

	      flt_node = c->u.catch.filter_list;
	      for (; flt_node; flt_node = TREE_CHAIN (flt_node))
		{
		  int filter = TREE_INT_CST_LOW (TREE_VALUE (flt_node));
		  next = add_action_record (ar_hash, filter, next);
		}
	    }
	}
      return next;

    case ERT_ALLOWED_EXCEPTIONS:
      /* An exception specification adds its filter to the
	 beginning of the chain.  */
      next = collect_one_action_chain (ar_hash, region->outer);

      /* If there is no next action, terminate the chain.  */
      if (next == -1)
	next = 0;
      /* If all outer actions are cleanups or must_not_throw,
	 we'll have no action record for it, since we had wanted
	 to encode these states in the call-site record directly.
	 Add a cleanup action to the chain to catch these.  */
      else if (next <= 0)
	next = add_action_record (ar_hash, 0, 0);
      
      return add_action_record (ar_hash, region->u.allowed.filter, next);

    case ERT_MUST_NOT_THROW:
      /* A must-not-throw region with no inner handlers or cleanups
	 requires no call-site entry.  Note that this differs from
	 the no handler or cleanup case in that we do require an lsda
	 to be generated.  Return a magic -2 value to record this.  */
      return -2;

    case ERT_CATCH:
    case ERT_THROW:
      /* CATCH regions are handled in TRY above.  THROW regions are
	 for optimization information only and produce no output.  */
      return collect_one_action_chain (ar_hash, region->outer);

    default:
      abort ();
    }
}

static int
add_call_site (rtx landing_pad, int action)
{
  struct call_site_record *data = cfun->eh->call_site_data;
  int used = cfun->eh->call_site_data_used;
  int size = cfun->eh->call_site_data_size;

  if (used >= size)
    {
      size = (size ? size * 2 : 64);
      data = ggc_realloc (data, sizeof (*data) * size);
      cfun->eh->call_site_data = data;
      cfun->eh->call_site_data_size = size;
    }

  data[used].landing_pad = landing_pad;
  data[used].action = action;

  cfun->eh->call_site_data_used = used + 1;

  return used + call_site_base;
}

/* Turn REG_EH_REGION notes back into NOTE_INSN_EH_REGION notes.
   The new note numbers will not refer to region numbers, but
   instead to call site entries.  */

void
convert_to_eh_region_ranges (void)
{
  rtx insn, iter, note;
  htab_t ar_hash;
  int last_action = -3;
  rtx last_action_insn = NULL_RTX;
  rtx last_landing_pad = NULL_RTX;
  rtx first_no_action_insn = NULL_RTX;
  int call_site = 0;

  if (USING_SJLJ_EXCEPTIONS || cfun->eh->region_tree == NULL)
    return;

  VARRAY_UCHAR_INIT (cfun->eh->action_record_data, 64, "action_record_data");

  ar_hash = htab_create (31, action_record_hash, action_record_eq, free);

  for (iter = get_insns (); iter ; iter = NEXT_INSN (iter))
    if (INSN_P (iter))
      {
	struct eh_region *region;
	int this_action;
	rtx this_landing_pad;

	insn = iter;
	if (GET_CODE (insn) == INSN
	    && GET_CODE (PATTERN (insn)) == SEQUENCE)
	  insn = XVECEXP (PATTERN (insn), 0, 0);

	note = find_reg_note (insn, REG_EH_REGION, NULL_RTX);
	if (!note)
	  {
	    if (! (GET_CODE (insn) == CALL_INSN
		   || (flag_non_call_exceptions
		       && may_trap_p (PATTERN (insn)))))
	      continue;
	    this_action = -1;
	    region = NULL;
	  }
	else
	  {
	    if (INTVAL (XEXP (note, 0)) <= 0)
	      continue;
	    region = cfun->eh->region_array[INTVAL (XEXP (note, 0))];
	    this_action = collect_one_action_chain (ar_hash, region);
	  }

	/* Existence of catch handlers, or must-not-throw regions
	   implies that an lsda is needed (even if empty).  */
	if (this_action != -1)
	  cfun->uses_eh_lsda = 1;

	/* Delay creation of region notes for no-action regions
	   until we're sure that an lsda will be required.  */
	else if (last_action == -3)
	  {
	    first_no_action_insn = iter;
	    last_action = -1;
	  }

	/* Cleanups and handlers may share action chains but not
	   landing pads.  Collect the landing pad for this region.  */
	if (this_action >= 0)
	  {
	    struct eh_region *o;
	    for (o = region; ! o->landing_pad ; o = o->outer)
	      continue;
	    this_landing_pad = o->landing_pad;
	  }
	else
	  this_landing_pad = NULL_RTX;

	/* Differing actions or landing pads implies a change in call-site
	   info, which implies some EH_REGION note should be emitted.  */
	if (last_action != this_action
	    || last_landing_pad != this_landing_pad)
	  {
	    /* If we'd not seen a previous action (-3) or the previous
	       action was must-not-throw (-2), then we do not need an
	       end note.  */
	    if (last_action >= -1)
	      {
		/* If we delayed the creation of the begin, do it now.  */
		if (first_no_action_insn)
		  {
		    call_site = add_call_site (NULL_RTX, 0);
		    note = emit_note_before (NOTE_INSN_EH_REGION_BEG,
					     first_no_action_insn);
		    NOTE_EH_HANDLER (note) = call_site;
		    first_no_action_insn = NULL_RTX;
		  }

		note = emit_note_after (NOTE_INSN_EH_REGION_END,
					last_action_insn);
		NOTE_EH_HANDLER (note) = call_site;
	      }

	    /* If the new action is must-not-throw, then no region notes
	       are created.  */
	    if (this_action >= -1)
	      {
		call_site = add_call_site (this_landing_pad,
					   this_action < 0 ? 0 : this_action);
		note = emit_note_before (NOTE_INSN_EH_REGION_BEG, iter);
		NOTE_EH_HANDLER (note) = call_site;
	      }

	    last_action = this_action;
	    last_landing_pad = this_landing_pad;
	  }
	last_action_insn = iter;
      }

  if (last_action >= -1 && ! first_no_action_insn)
    {
      note = emit_note_after (NOTE_INSN_EH_REGION_END, last_action_insn);
      NOTE_EH_HANDLER (note) = call_site;
    }

  htab_delete (ar_hash);
}


static void
push_uleb128 (varray_type *data_area, unsigned int value)
{
  do
    {
      unsigned char byte = value & 0x7f;
      value >>= 7;
      if (value)
	byte |= 0x80;
      VARRAY_PUSH_UCHAR (*data_area, byte);
    }
  while (value);
}

static void
push_sleb128 (varray_type *data_area, int value)
{
  unsigned char byte;
  int more;

  do
    {
      byte = value & 0x7f;
      value >>= 7;
      more = ! ((value == 0 && (byte & 0x40) == 0)
		|| (value == -1 && (byte & 0x40) != 0));
      if (more)
	byte |= 0x80;
      VARRAY_PUSH_UCHAR (*data_area, byte);
    }
  while (more);
}


#ifndef HAVE_AS_LEB128
static int
dw2_size_of_call_site_table (void)
{
  int n = cfun->eh->call_site_data_used;
  int size = n * (4 + 4 + 4);
  int i;

  for (i = 0; i < n; ++i)
    {
      struct call_site_record *cs = &cfun->eh->call_site_data[i];
      size += size_of_uleb128 (cs->action);
    }

  return size;
}

static int
sjlj_size_of_call_site_table (void)
{
  int n = cfun->eh->call_site_data_used;
  int size = 0;
  int i;

  for (i = 0; i < n; ++i)
    {
      struct call_site_record *cs = &cfun->eh->call_site_data[i];
      size += size_of_uleb128 (INTVAL (cs->landing_pad));
      size += size_of_uleb128 (cs->action);
    }

  return size;
}
#endif

static void
dw2_output_call_site_table (void)
{
  const char *const function_start_lab
    = IDENTIFIER_POINTER (current_function_func_begin_label);
  int n = cfun->eh->call_site_data_used;
  int i;

  for (i = 0; i < n; ++i)
    {
      struct call_site_record *cs = &cfun->eh->call_site_data[i];
      char reg_start_lab[32];
      char reg_end_lab[32];
      char landing_pad_lab[32];

      ASM_GENERATE_INTERNAL_LABEL (reg_start_lab, "LEHB", call_site_base + i);
      ASM_GENERATE_INTERNAL_LABEL (reg_end_lab, "LEHE", call_site_base + i);

      if (cs->landing_pad)
	ASM_GENERATE_INTERNAL_LABEL (landing_pad_lab, "L",
				     CODE_LABEL_NUMBER (cs->landing_pad));

      /* ??? Perhaps use insn length scaling if the assembler supports
	 generic arithmetic.  */
      /* ??? Perhaps use attr_length to choose data1 or data2 instead of
	 data4 if the function is small enough.  */
#ifdef HAVE_AS_LEB128
      dw2_asm_output_delta_uleb128 (reg_start_lab, function_start_lab,
				    "region %d start", i);
      dw2_asm_output_delta_uleb128 (reg_end_lab, reg_start_lab,
				    "length");
      if (cs->landing_pad)
	dw2_asm_output_delta_uleb128 (landing_pad_lab, function_start_lab,
				      "landing pad");
      else
	dw2_asm_output_data_uleb128 (0, "landing pad");
#else
      dw2_asm_output_delta (4, reg_start_lab, function_start_lab,
			    "region %d start", i);
      dw2_asm_output_delta (4, reg_end_lab, reg_start_lab, "length");
      if (cs->landing_pad)
	dw2_asm_output_delta (4, landing_pad_lab, function_start_lab,
			      "landing pad");
      else
	dw2_asm_output_data (4, 0, "landing pad");
#endif
      dw2_asm_output_data_uleb128 (cs->action, "action");
    }

  call_site_base += n;
}

static void
sjlj_output_call_site_table (void)
{
  int n = cfun->eh->call_site_data_used;
  int i;

  for (i = 0; i < n; ++i)
    {
      struct call_site_record *cs = &cfun->eh->call_site_data[i];

      dw2_asm_output_data_uleb128 (INTVAL (cs->landing_pad),
				   "region %d landing pad", i);
      dw2_asm_output_data_uleb128 (cs->action, "action");
    }

  call_site_base += n;
}

/* Tell assembler to switch to the section for the exception handling
   table.  */

void
default_exception_section (void)
{
  if (targetm.have_named_sections)
    {
      int flags;
#ifdef HAVE_LD_RO_RW_SECTION_MIXING
      int tt_format = ASM_PREFERRED_EH_DATA_FORMAT (/*code=*/0, /*global=*/1);

      flags = (! flag_pic
	       || ((tt_format & 0x70) != DW_EH_PE_absptr
		   && (tt_format & 0x70) != DW_EH_PE_aligned))
	      ? 0 : SECTION_WRITE;
#else
      flags = SECTION_WRITE;
#endif
      named_section_flags (".gcc_except_table", flags);
    }
  else if (flag_pic)
    data_section ();
  else
    readonly_data_section ();
}

void
output_function_exception_table (void)
{
  int tt_format, cs_format, lp_format, i, n;
#ifdef HAVE_AS_LEB128
  char ttype_label[32];
  char cs_after_size_label[32];
  char cs_end_label[32];
#else
  int call_site_len;
#endif
  int have_tt_data;
  int tt_format_size = 0;

  /* Not all functions need anything.  */
  if (! cfun->uses_eh_lsda)
    return;

#ifdef IA64_UNWIND_INFO
  fputs ("\t.personality\t", asm_out_file);
  output_addr_const (asm_out_file, eh_personality_libfunc);
  fputs ("\n\t.handlerdata\n", asm_out_file);
  /* Note that varasm still thinks we're in the function's code section.
     The ".endp" directive that will immediately follow will take us back.  */
#else
  (*targetm.asm_out.exception_section) ();
#endif

  have_tt_data = (VARRAY_ACTIVE_SIZE (cfun->eh->ttype_data) > 0
		  || VARRAY_ACTIVE_SIZE (cfun->eh->ehspec_data) > 0);

  /* Indicate the format of the @TType entries.  */
  if (! have_tt_data)
    tt_format = DW_EH_PE_omit;
  else
    {
      tt_format = ASM_PREFERRED_EH_DATA_FORMAT (/*code=*/0, /*global=*/1);
#ifdef HAVE_AS_LEB128
      ASM_GENERATE_INTERNAL_LABEL (ttype_label, "LLSDATT",
				   current_function_funcdef_no);
#endif
      tt_format_size = size_of_encoded_value (tt_format);

      assemble_align (tt_format_size * BITS_PER_UNIT);
    }

  (*targetm.asm_out.internal_label) (asm_out_file, "LLSDA",
			     current_function_funcdef_no);

  /* The LSDA header.  */

  /* Indicate the format of the landing pad start pointer.  An omitted
     field implies @LPStart == @Start.  */
  /* Currently we always put @LPStart == @Start.  This field would
     be most useful in moving the landing pads completely out of
     line to another section, but it could also be used to minimize
     the size of uleb128 landing pad offsets.  */
  lp_format = DW_EH_PE_omit;
  dw2_asm_output_data (1, lp_format, "@LPStart format (%s)",
		       eh_data_format_name (lp_format));

  /* @LPStart pointer would go here.  */

  dw2_asm_output_data (1, tt_format, "@TType format (%s)",
		       eh_data_format_name (tt_format));

#ifndef HAVE_AS_LEB128
  if (USING_SJLJ_EXCEPTIONS)
    call_site_len = sjlj_size_of_call_site_table ();
  else
    call_site_len = dw2_size_of_call_site_table ();
#endif

  /* A pc-relative 4-byte displacement to the @TType data.  */
  if (have_tt_data)
    {
#ifdef HAVE_AS_LEB128
      char ttype_after_disp_label[32];
      ASM_GENERATE_INTERNAL_LABEL (ttype_after_disp_label, "LLSDATTD",
				   current_function_funcdef_no);
      dw2_asm_output_delta_uleb128 (ttype_label, ttype_after_disp_label,
				    "@TType base offset");
      ASM_OUTPUT_LABEL (asm_out_file, ttype_after_disp_label);
#else
      /* Ug.  Alignment queers things.  */
      unsigned int before_disp, after_disp, last_disp, disp;

      before_disp = 1 + 1;
      after_disp = (1 + size_of_uleb128 (call_site_len)
		    + call_site_len
		    + VARRAY_ACTIVE_SIZE (cfun->eh->action_record_data)
		    + (VARRAY_ACTIVE_SIZE (cfun->eh->ttype_data)
		       * tt_format_size));

      disp = after_disp;
      do
	{
	  unsigned int disp_size, pad;

	  last_disp = disp;
	  disp_size = size_of_uleb128 (disp);
	  pad = before_disp + disp_size + after_disp;
	  if (pad % tt_format_size)
	    pad = tt_format_size - (pad % tt_format_size);
	  else
	    pad = 0;
	  disp = after_disp + pad;
	}
      while (disp != last_disp);

      dw2_asm_output_data_uleb128 (disp, "@TType base offset");
#endif
    }

  /* Indicate the format of the call-site offsets.  */
#ifdef HAVE_AS_LEB128
  cs_format = DW_EH_PE_uleb128;
#else
  cs_format = DW_EH_PE_udata4;
#endif
  dw2_asm_output_data (1, cs_format, "call-site format (%s)",
		       eh_data_format_name (cs_format));

#ifdef HAVE_AS_LEB128
  ASM_GENERATE_INTERNAL_LABEL (cs_after_size_label, "LLSDACSB",
			       current_function_funcdef_no);
  ASM_GENERATE_INTERNAL_LABEL (cs_end_label, "LLSDACSE",
			       current_function_funcdef_no);
  dw2_asm_output_delta_uleb128 (cs_end_label, cs_after_size_label,
				"Call-site table length");
  ASM_OUTPUT_LABEL (asm_out_file, cs_after_size_label);
  if (USING_SJLJ_EXCEPTIONS)
    sjlj_output_call_site_table ();
  else
    dw2_output_call_site_table ();
  ASM_OUTPUT_LABEL (asm_out_file, cs_end_label);
#else
  dw2_asm_output_data_uleb128 (call_site_len,"Call-site table length");
  if (USING_SJLJ_EXCEPTIONS)
    sjlj_output_call_site_table ();
  else
    dw2_output_call_site_table ();
#endif

  /* ??? Decode and interpret the data for flag_debug_asm.  */
  n = VARRAY_ACTIVE_SIZE (cfun->eh->action_record_data);
  for (i = 0; i < n; ++i)
    dw2_asm_output_data (1, VARRAY_UCHAR (cfun->eh->action_record_data, i),
			 (i ? NULL : "Action record table"));

  if (have_tt_data)
    assemble_align (tt_format_size * BITS_PER_UNIT);

  i = VARRAY_ACTIVE_SIZE (cfun->eh->ttype_data);
  while (i-- > 0)
    {
      tree type = VARRAY_TREE (cfun->eh->ttype_data, i);
      rtx value;

      if (type == NULL_TREE)
	value = const0_rtx;
      else
	{
	  struct cgraph_varpool_node *node;

	  type = lookup_type_for_runtime (type);
	  value = expand_expr (type, NULL_RTX, VOIDmode, EXPAND_INITIALIZER);

	  /* Let cgraph know that the rtti decl is used.  Not all of the
	     paths below go through assemble_integer, which would take
	     care of this for us.  */
	  if (TREE_CODE (type) == ADDR_EXPR)
	    {
	      type = TREE_OPERAND (type, 0);
	      node = cgraph_varpool_node (type);
	      if (node)
		cgraph_varpool_mark_needed_node (node);
	    }
	  else if (TREE_CODE (type) != INTEGER_CST)
	    abort ();
	}

      if (tt_format == DW_EH_PE_absptr || tt_format == DW_EH_PE_aligned)
	assemble_integer (value, tt_format_size,
			  tt_format_size * BITS_PER_UNIT, 1);
      else
	dw2_asm_output_encoded_addr_rtx (tt_format, value, NULL);
    }

#ifdef HAVE_AS_LEB128
  if (have_tt_data)
      ASM_OUTPUT_LABEL (asm_out_file, ttype_label);
#endif

  /* ??? Decode and interpret the data for flag_debug_asm.  */
  n = VARRAY_ACTIVE_SIZE (cfun->eh->ehspec_data);
  for (i = 0; i < n; ++i)
    dw2_asm_output_data (1, VARRAY_UCHAR (cfun->eh->ehspec_data, i),
			 (i ? NULL : "Exception specification table"));

  function_section (current_function_decl);
}

#include "gt-except.h"
