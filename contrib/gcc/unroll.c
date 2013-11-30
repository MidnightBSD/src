/* Try to unroll loops, and split induction variables.
   Copyright (C) 1992, 1993, 1994, 1995, 1997, 1998, 1999, 2000, 2001,
   2002, 2003
   Free Software Foundation, Inc.
   Contributed by James E. Wilson, Cygnus Support/UC Berkeley.

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

/* Try to unroll a loop, and split induction variables.

   Loops for which the number of iterations can be calculated exactly are
   handled specially.  If the number of iterations times the insn_count is
   less than MAX_UNROLLED_INSNS, then the loop is unrolled completely.
   Otherwise, we try to unroll the loop a number of times modulo the number
   of iterations, so that only one exit test will be needed.  It is unrolled
   a number of times approximately equal to MAX_UNROLLED_INSNS divided by
   the insn count.

   Otherwise, if the number of iterations can be calculated exactly at
   run time, and the loop is always entered at the top, then we try to
   precondition the loop.  That is, at run time, calculate how many times
   the loop will execute, and then execute the loop body a few times so
   that the remaining iterations will be some multiple of 4 (or 2 if the
   loop is large).  Then fall through to a loop unrolled 4 (or 2) times,
   with only one exit test needed at the end of the loop.

   Otherwise, if the number of iterations can not be calculated exactly,
   not even at run time, then we still unroll the loop a number of times
   approximately equal to MAX_UNROLLED_INSNS divided by the insn count,
   but there must be an exit test after each copy of the loop body.

   For each induction variable, which is dead outside the loop (replaceable)
   or for which we can easily calculate the final value, if we can easily
   calculate its value at each place where it is set as a function of the
   current loop unroll count and the variable's value at loop entry, then
   the induction variable is split into `N' different variables, one for
   each copy of the loop body.  One variable is live across the backward
   branch, and the others are all calculated as a function of this variable.
   This helps eliminate data dependencies, and leads to further opportunities
   for cse.  */

/* Possible improvements follow:  */

/* ??? Add an extra pass somewhere to determine whether unrolling will
   give any benefit.  E.g. after generating all unrolled insns, compute the
   cost of all insns and compare against cost of insns in rolled loop.

   - On traditional architectures, unrolling a non-constant bound loop
     is a win if there is a giv whose only use is in memory addresses, the
     memory addresses can be split, and hence giv increments can be
     eliminated.
   - It is also a win if the loop is executed many times, and preconditioning
     can be performed for the loop.
   Add code to check for these and similar cases.  */

/* ??? Improve control of which loops get unrolled.  Could use profiling
   info to only unroll the most commonly executed loops.  Perhaps have
   a user specifiable option to control the amount of code expansion,
   or the percent of loops to consider for unrolling.  Etc.  */

/* ??? Look at the register copies inside the loop to see if they form a
   simple permutation.  If so, iterate the permutation until it gets back to
   the start state.  This is how many times we should unroll the loop, for
   best results, because then all register copies can be eliminated.
   For example, the lisp nreverse function should be unrolled 3 times
   while (this)
     {
       next = this->cdr;
       this->cdr = prev;
       prev = this;
       this = next;
     }

   ??? The number of times to unroll the loop may also be based on data
   references in the loop.  For example, if we have a loop that references
   x[i-1], x[i], and x[i+1], we should unroll it a multiple of 3 times.  */

/* ??? Add some simple linear equation solving capability so that we can
   determine the number of loop iterations for more complex loops.
   For example, consider this loop from gdb
   #define SWAP_TARGET_AND_HOST(buffer,len)
     {
       char tmp;
       char *p = (char *) buffer;
       char *q = ((char *) buffer) + len - 1;
       int iterations = (len + 1) >> 1;
       int i;
       for (p; p < q; p++, q--;)
	 {
	   tmp = *q;
	   *q = *p;
	   *p = tmp;
	 }
     }
   Note that:
     start value = p = &buffer + current_iteration
     end value   = q = &buffer + len - 1 - current_iteration
   Given the loop exit test of "p < q", then there must be "q - p" iterations,
   set equal to zero and solve for number of iterations:
     q - p = len - 1 - 2*current_iteration = 0
     current_iteration = (len - 1) / 2
   Hence, there are (len - 1) / 2 (rounded up to the nearest integer)
   iterations of this loop.  */

/* ??? Currently, no labels are marked as loop invariant when doing loop
   unrolling.  This is because an insn inside the loop, that loads the address
   of a label inside the loop into a register, could be moved outside the loop
   by the invariant code motion pass if labels were invariant.  If the loop
   is subsequently unrolled, the code will be wrong because each unrolled
   body of the loop will use the same address, whereas each actually needs a
   different address.  A case where this happens is when a loop containing
   a switch statement is unrolled.

   It would be better to let labels be considered invariant.  When we
   unroll loops here, check to see if any insns using a label local to the
   loop were moved before the loop.  If so, then correct the problem, by
   moving the insn back into the loop, or perhaps replicate the insn before
   the loop, one copy for each time the loop is unrolled.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "tm_p.h"
#include "insn-config.h"
#include "integrate.h"
#include "regs.h"
#include "recog.h"
#include "flags.h"
#include "function.h"
#include "expr.h"
#include "loop.h"
#include "toplev.h"
#include "hard-reg-set.h"
#include "basic-block.h"
#include "predict.h"
#include "params.h"
#include "cfgloop.h"

/* The prime factors looked for when trying to unroll a loop by some
   number which is modulo the total number of iterations.  Just checking
   for these 4 prime factors will find at least one factor for 75% of
   all numbers theoretically.  Practically speaking, this will succeed
   almost all of the time since loops are generally a multiple of 2
   and/or 5.  */

#define NUM_FACTORS 4

static struct _factor { const int factor; int count; }
factors[NUM_FACTORS] = { {2, 0}, {3, 0}, {5, 0}, {7, 0}};

/* Describes the different types of loop unrolling performed.  */

enum unroll_types
{
  UNROLL_COMPLETELY,
  UNROLL_MODULO,
  UNROLL_NAIVE
};

/* Indexed by register number, if nonzero, then it contains a pointer
   to a struct induction for a DEST_REG giv which has been combined with
   one of more address givs.  This is needed because whenever such a DEST_REG
   giv is modified, we must modify the value of all split address givs
   that were combined with this DEST_REG giv.  */

static struct induction **addr_combined_regs;

/* Indexed by register number, if this is a splittable induction variable,
   then this will hold the current value of the register, which depends on the
   iteration number.  */

static rtx *splittable_regs;

/* Indexed by register number, if this is a splittable induction variable,
   then this will hold the number of instructions in the loop that modify
   the induction variable.  Used to ensure that only the last insn modifying
   a split iv will update the original iv of the dest.  */

static int *splittable_regs_updates;

/* Forward declarations.  */

static rtx simplify_cmp_and_jump_insns (enum rtx_code, enum machine_mode,
					rtx, rtx, rtx);
static void init_reg_map (struct inline_remap *, int);
static rtx calculate_giv_inc (rtx, rtx, unsigned int);
static rtx initial_reg_note_copy (rtx, struct inline_remap *);
static void final_reg_note_copy (rtx *, struct inline_remap *);
static void copy_loop_body (struct loop *, rtx, rtx,
			    struct inline_remap *, rtx, int,
			    enum unroll_types, rtx, rtx, rtx, rtx);
static int find_splittable_regs (const struct loop *, enum unroll_types,
				 int);
static int find_splittable_givs (const struct loop *, struct iv_class *,
				 enum unroll_types, rtx, int);
static int reg_dead_after_loop (const struct loop *, rtx);
static rtx fold_rtx_mult_add (rtx, rtx, rtx, enum machine_mode);
static rtx remap_split_bivs (struct loop *, rtx);
static rtx find_common_reg_term (rtx, rtx);
static rtx subtract_reg_term (rtx, rtx);
static rtx loop_find_equiv_value (const struct loop *, rtx);
static rtx ujump_to_loop_cont (rtx, rtx);

/* Try to unroll one loop and split induction variables in the loop.

   The loop is described by the arguments LOOP and INSN_COUNT.
   STRENGTH_REDUCTION_P indicates whether information generated in the
   strength reduction pass is available.

   This function is intended to be called from within `strength_reduce'
   in loop.c.  */

void
unroll_loop (struct loop *loop, int insn_count, int strength_reduce_p)
{
  struct loop_info *loop_info = LOOP_INFO (loop);
  struct loop_ivs *ivs = LOOP_IVS (loop);
  int i, j;
  unsigned int r;
  unsigned HOST_WIDE_INT temp;
  int unroll_number = 1;
  rtx copy_start, copy_end;
  rtx insn, sequence, pattern, tem;
  int max_labelno, max_insnno;
  rtx insert_before;
  struct inline_remap *map;
  char *local_label = NULL;
  char *local_regno;
  unsigned int max_local_regnum;
  unsigned int maxregnum;
  rtx exit_label = 0;
  rtx start_label;
  struct iv_class *bl;
  int splitting_not_safe = 0;
  enum unroll_types unroll_type = UNROLL_NAIVE;
  int loop_preconditioned = 0;
  rtx safety_label;
  /* This points to the last real insn in the loop, which should be either
     a JUMP_INSN (for conditional jumps) or a BARRIER (for unconditional
     jumps).  */
  rtx last_loop_insn;
  rtx loop_start = loop->start;
  rtx loop_end = loop->end;

  /* Don't bother unrolling huge loops.  Since the minimum factor is
     two, loops greater than one half of MAX_UNROLLED_INSNS will never
     be unrolled.  */
  if (insn_count > MAX_UNROLLED_INSNS / 2)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream, "Unrolling failure: Loop too big.\n");
      return;
    }

  /* Determine type of unroll to perform.  Depends on the number of iterations
     and the size of the loop.  */

  /* If there is no strength reduce info, then set
     loop_info->n_iterations to zero.  This can happen if
     strength_reduce can't find any bivs in the loop.  A value of zero
     indicates that the number of iterations could not be calculated.  */

  if (! strength_reduce_p)
    loop_info->n_iterations = 0;

  if (loop_dump_stream && loop_info->n_iterations > 0)
    fprintf (loop_dump_stream, "Loop unrolling: " HOST_WIDE_INT_PRINT_DEC
	     " iterations.\n", loop_info->n_iterations);

  /* Find and save a pointer to the last nonnote insn in the loop.  */

  last_loop_insn = prev_nonnote_insn (loop_end);

  /* Calculate how many times to unroll the loop.  Indicate whether or
     not the loop is being completely unrolled.  */

  if (loop_info->n_iterations == 1)
    {
      /* Handle the case where the loop begins with an unconditional
	 jump to the loop condition.  Make sure to delete the jump
	 insn, otherwise the loop body will never execute.  */

      /* FIXME this actually checks for a jump to the continue point, which
	 is not the same as the condition in a for loop.  As a result, this
	 optimization fails for most for loops.  We should really use flow
	 information rather than instruction pattern matching.  */
      rtx ujump = ujump_to_loop_cont (loop->start, loop->cont);

      /* If number of iterations is exactly 1, then eliminate the compare and
	 branch at the end of the loop since they will never be taken.
	 Then return, since no other action is needed here.  */

      /* If the last instruction is not a BARRIER or a JUMP_INSN, then
	 don't do anything.  */

      if (GET_CODE (last_loop_insn) == BARRIER)
	{
	  /* Delete the jump insn.  This will delete the barrier also.  */
	  last_loop_insn = PREV_INSN (last_loop_insn);
	}

      if (ujump && GET_CODE (last_loop_insn) == JUMP_INSN)
	{
#ifdef HAVE_cc0
	  rtx prev = PREV_INSN (last_loop_insn);
#endif
	  delete_related_insns (last_loop_insn);
#ifdef HAVE_cc0
	  /* The immediately preceding insn may be a compare which must be
	     deleted.  */
	  if (only_sets_cc0_p (prev))
	    delete_related_insns (prev);
#endif

	  delete_related_insns (ujump);

	  /* Remove the loop notes since this is no longer a loop.  */
	  if (loop->vtop)
	    delete_related_insns (loop->vtop);
	  if (loop->cont)
	    delete_related_insns (loop->cont);
	  if (loop_start)
	    delete_related_insns (loop_start);
	  if (loop_end)
	    delete_related_insns (loop_end);

	  return;
	}
    }

  if (loop_info->n_iterations > 0
      /* Avoid overflow in the next expression.  */
      && loop_info->n_iterations < (unsigned) MAX_UNROLLED_INSNS
      && loop_info->n_iterations * insn_count < (unsigned) MAX_UNROLLED_INSNS)
    {
      unroll_number = loop_info->n_iterations;
      unroll_type = UNROLL_COMPLETELY;
    }
  else if (loop_info->n_iterations > 0)
    {
      /* Try to factor the number of iterations.  Don't bother with the
	 general case, only using 2, 3, 5, and 7 will get 75% of all
	 numbers theoretically, and almost all in practice.  */

      for (i = 0; i < NUM_FACTORS; i++)
	factors[i].count = 0;

      temp = loop_info->n_iterations;
      for (i = NUM_FACTORS - 1; i >= 0; i--)
	while (temp % factors[i].factor == 0)
	  {
	    factors[i].count++;
	    temp = temp / factors[i].factor;
	  }

      /* Start with the larger factors first so that we generally
	 get lots of unrolling.  */

      unroll_number = 1;
      temp = insn_count;
      for (i = 3; i >= 0; i--)
	while (factors[i].count--)
	  {
	    if (temp * factors[i].factor < (unsigned) MAX_UNROLLED_INSNS)
	      {
		unroll_number *= factors[i].factor;
		temp *= factors[i].factor;
	      }
	    else
	      break;
	  }

      /* If we couldn't find any factors, then unroll as in the normal
	 case.  */
      if (unroll_number == 1)
	{
	  if (loop_dump_stream)
	    fprintf (loop_dump_stream, "Loop unrolling: No factors found.\n");
	}
      else
	unroll_type = UNROLL_MODULO;
    }

  /* Default case, calculate number of times to unroll loop based on its
     size.  */
  if (unroll_type == UNROLL_NAIVE)
    {
      if (8 * insn_count < MAX_UNROLLED_INSNS)
	unroll_number = 8;
      else if (4 * insn_count < MAX_UNROLLED_INSNS)
	unroll_number = 4;
      else
	unroll_number = 2;
    }

  /* Now we know how many times to unroll the loop.  */

  if (loop_dump_stream)
    fprintf (loop_dump_stream, "Unrolling loop %d times.\n", unroll_number);

  if (unroll_type == UNROLL_COMPLETELY || unroll_type == UNROLL_MODULO)
    {
      /* Loops of these types can start with jump down to the exit condition
	 in rare circumstances.

	 Consider a pair of nested loops where the inner loop is part
	 of the exit code for the outer loop.

	 In this case jump.c will not duplicate the exit test for the outer
	 loop, so it will start with a jump to the exit code.

	 Then consider if the inner loop turns out to iterate once and
	 only once.  We will end up deleting the jumps associated with
	 the inner loop.  However, the loop notes are not removed from
	 the instruction stream.

	 And finally assume that we can compute the number of iterations
	 for the outer loop.

	 In this case unroll may want to unroll the outer loop even though
	 it starts with a jump to the outer loop's exit code.

	 We could try to optimize this case, but it hardly seems worth it.
	 Just return without unrolling the loop in such cases.  */

      insn = loop_start;
      while (GET_CODE (insn) != CODE_LABEL && GET_CODE (insn) != JUMP_INSN)
	insn = NEXT_INSN (insn);
      if (GET_CODE (insn) == JUMP_INSN)
	return;
    }

  if (unroll_type == UNROLL_COMPLETELY)
    {
      /* Completely unrolling the loop:  Delete the compare and branch at
	 the end (the last two instructions).   This delete must done at the
	 very end of loop unrolling, to avoid problems with calls to
	 back_branch_in_range_p, which is called by find_splittable_regs.
	 All increments of splittable bivs/givs are changed to load constant
	 instructions.  */

      copy_start = loop_start;

      /* Set insert_before to the instruction immediately after the JUMP_INSN
	 (or BARRIER), so that any NOTEs between the JUMP_INSN and the end of
	 the loop will be correctly handled by copy_loop_body.  */
      insert_before = NEXT_INSN (last_loop_insn);

      /* Set copy_end to the insn before the jump at the end of the loop.  */
      if (GET_CODE (last_loop_insn) == BARRIER)
	copy_end = PREV_INSN (PREV_INSN (last_loop_insn));
      else if (GET_CODE (last_loop_insn) == JUMP_INSN)
	{
	  copy_end = PREV_INSN (last_loop_insn);
#ifdef HAVE_cc0
	  /* The instruction immediately before the JUMP_INSN may be a compare
	     instruction which we do not want to copy.  */
	  if (sets_cc0_p (PREV_INSN (copy_end)))
	    copy_end = PREV_INSN (copy_end);
#endif
	}
      else
	{
	  /* We currently can't unroll a loop if it doesn't end with a
	     JUMP_INSN.  There would need to be a mechanism that recognizes
	     this case, and then inserts a jump after each loop body, which
	     jumps to after the last loop body.  */
	  if (loop_dump_stream)
	    fprintf (loop_dump_stream,
		     "Unrolling failure: loop does not end with a JUMP_INSN.\n");
	  return;
	}
    }
  else if (unroll_type == UNROLL_MODULO)
    {
      /* Partially unrolling the loop:  The compare and branch at the end
	 (the last two instructions) must remain.  Don't copy the compare
	 and branch instructions at the end of the loop.  Insert the unrolled
	 code immediately before the compare/branch at the end so that the
	 code will fall through to them as before.  */

      copy_start = loop_start;

      /* Set insert_before to the jump insn at the end of the loop.
	 Set copy_end to before the jump insn at the end of the loop.  */
      if (GET_CODE (last_loop_insn) == BARRIER)
	{
	  insert_before = PREV_INSN (last_loop_insn);
	  copy_end = PREV_INSN (insert_before);
	}
      else if (GET_CODE (last_loop_insn) == JUMP_INSN)
	{
	  insert_before = last_loop_insn;
#ifdef HAVE_cc0
	  /* The instruction immediately before the JUMP_INSN may be a compare
	     instruction which we do not want to copy or delete.  */
	  if (sets_cc0_p (PREV_INSN (insert_before)))
	    insert_before = PREV_INSN (insert_before);
#endif
	  copy_end = PREV_INSN (insert_before);
	}
      else
	{
	  /* We currently can't unroll a loop if it doesn't end with a
	     JUMP_INSN.  There would need to be a mechanism that recognizes
	     this case, and then inserts a jump after each loop body, which
	     jumps to after the last loop body.  */
	  if (loop_dump_stream)
	    fprintf (loop_dump_stream,
		     "Unrolling failure: loop does not end with a JUMP_INSN.\n");
	  return;
	}
    }
  else
    {
      /* Normal case: Must copy the compare and branch instructions at the
	 end of the loop.  */

      if (GET_CODE (last_loop_insn) == BARRIER)
	{
	  /* Loop ends with an unconditional jump and a barrier.
	     Handle this like above, don't copy jump and barrier.
	     This is not strictly necessary, but doing so prevents generating
	     unconditional jumps to an immediately following label.

	     This will be corrected below if the target of this jump is
	     not the start_label.  */

	  insert_before = PREV_INSN (last_loop_insn);
	  copy_end = PREV_INSN (insert_before);
	}
      else if (GET_CODE (last_loop_insn) == JUMP_INSN)
	{
	  /* Set insert_before to immediately after the JUMP_INSN, so that
	     NOTEs at the end of the loop will be correctly handled by
	     copy_loop_body.  */
	  insert_before = NEXT_INSN (last_loop_insn);
	  copy_end = last_loop_insn;
	}
      else
	{
	  /* We currently can't unroll a loop if it doesn't end with a
	     JUMP_INSN.  There would need to be a mechanism that recognizes
	     this case, and then inserts a jump after each loop body, which
	     jumps to after the last loop body.  */
	  if (loop_dump_stream)
	    fprintf (loop_dump_stream,
		     "Unrolling failure: loop does not end with a JUMP_INSN.\n");
	  return;
	}

      /* If copying exit test branches because they can not be eliminated,
	 then must convert the fall through case of the branch to a jump past
	 the end of the loop.  Create a label to emit after the loop and save
	 it for later use.  Do not use the label after the loop, if any, since
	 it might be used by insns outside the loop, or there might be insns
	 added before it later by final_[bg]iv_value which must be after
	 the real exit label.  */
      exit_label = gen_label_rtx ();

      insn = loop_start;
      while (GET_CODE (insn) != CODE_LABEL && GET_CODE (insn) != JUMP_INSN)
	insn = NEXT_INSN (insn);

      if (GET_CODE (insn) == JUMP_INSN)
	{
	  /* The loop starts with a jump down to the exit condition test.
	     Start copying the loop after the barrier following this
	     jump insn.  */
	  copy_start = NEXT_INSN (insn);

	  /* Splitting induction variables doesn't work when the loop is
	     entered via a jump to the bottom, because then we end up doing
	     a comparison against a new register for a split variable, but
	     we did not execute the set insn for the new register because
	     it was skipped over.  */
	  splitting_not_safe = 1;
	  if (loop_dump_stream)
	    fprintf (loop_dump_stream,
		     "Splitting not safe, because loop not entered at top.\n");
	}
      else
	copy_start = loop_start;
    }

  /* This should always be the first label in the loop.  */
  start_label = NEXT_INSN (copy_start);
  /* There may be a line number note and/or a loop continue note here.  */
  while (GET_CODE (start_label) == NOTE)
    start_label = NEXT_INSN (start_label);
  if (GET_CODE (start_label) != CODE_LABEL)
    {
      /* This can happen as a result of jump threading.  If the first insns in
	 the loop test the same condition as the loop's backward jump, or the
	 opposite condition, then the backward jump will be modified to point
	 to elsewhere, and the loop's start label is deleted.

	 This case currently can not be handled by the loop unrolling code.  */

      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Unrolling failure: unknown insns between BEG note and loop label.\n");
      return;
    }
  if (LABEL_NAME (start_label))
    {
      /* The jump optimization pass must have combined the original start label
	 with a named label for a goto.  We can't unroll this case because
	 jumps which go to the named label must be handled differently than
	 jumps to the loop start, and it is impossible to differentiate them
	 in this case.  */
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Unrolling failure: loop start label is gone\n");
      return;
    }

  if (unroll_type == UNROLL_NAIVE
      && GET_CODE (last_loop_insn) == BARRIER
      && GET_CODE (PREV_INSN (last_loop_insn)) == JUMP_INSN
      && start_label != JUMP_LABEL (PREV_INSN (last_loop_insn)))
    {
      /* In this case, we must copy the jump and barrier, because they will
	 not be converted to jumps to an immediately following label.  */

      insert_before = NEXT_INSN (last_loop_insn);
      copy_end = last_loop_insn;
    }

  if (unroll_type == UNROLL_NAIVE
      && GET_CODE (last_loop_insn) == JUMP_INSN
      && start_label != JUMP_LABEL (last_loop_insn))
    {
      /* ??? The loop ends with a conditional branch that does not branch back
	 to the loop start label.  In this case, we must emit an unconditional
	 branch to the loop exit after emitting the final branch.
	 copy_loop_body does not have support for this currently, so we
	 give up.  It doesn't seem worthwhile to unroll anyways since
	 unrolling would increase the number of branch instructions
	 executed.  */
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Unrolling failure: final conditional branch not to loop start\n");
      return;
    }

  /* Allocate a translation table for the labels and insn numbers.
     They will be filled in as we copy the insns in the loop.  */

  max_labelno = max_label_num ();
  max_insnno = get_max_uid ();

  /* Various paths through the unroll code may reach the "egress" label
     without initializing fields within the map structure.

     To be safe, we use xcalloc to zero the memory.  */
  map = xcalloc (1, sizeof (struct inline_remap));

  /* Allocate the label map.  */

  if (max_labelno > 0)
    {
      map->label_map = xcalloc (max_labelno, sizeof (rtx));
      local_label = xcalloc (max_labelno, sizeof (char));
    }

  /* Search the loop and mark all local labels, i.e. the ones which have to
     be distinct labels when copied.  For all labels which might be
     non-local, set their label_map entries to point to themselves.
     If they happen to be local their label_map entries will be overwritten
     before the loop body is copied.  The label_map entries for local labels
     will be set to a different value each time the loop body is copied.  */

  for (insn = copy_start; insn != loop_end; insn = NEXT_INSN (insn))
    {
      rtx note;

      if (GET_CODE (insn) == CODE_LABEL)
	local_label[CODE_LABEL_NUMBER (insn)] = 1;
      else if (GET_CODE (insn) == JUMP_INSN)
	{
	  if (JUMP_LABEL (insn))
	    set_label_in_map (map,
			      CODE_LABEL_NUMBER (JUMP_LABEL (insn)),
			      JUMP_LABEL (insn));
	  else if (GET_CODE (PATTERN (insn)) == ADDR_VEC
		   || GET_CODE (PATTERN (insn)) == ADDR_DIFF_VEC)
	    {
	      rtx pat = PATTERN (insn);
	      int diff_vec_p = GET_CODE (PATTERN (insn)) == ADDR_DIFF_VEC;
	      int len = XVECLEN (pat, diff_vec_p);
	      rtx label;

	      for (i = 0; i < len; i++)
		{
		  label = XEXP (XVECEXP (pat, diff_vec_p, i), 0);
		  set_label_in_map (map, CODE_LABEL_NUMBER (label), label);
		}
	    }
	}
      if ((note = find_reg_note (insn, REG_LABEL, NULL_RTX)))
	set_label_in_map (map, CODE_LABEL_NUMBER (XEXP (note, 0)),
			  XEXP (note, 0));
    }

  /* Allocate space for the insn map.  */

  map->insn_map = xmalloc (max_insnno * sizeof (rtx));

  /* Set this to zero, to indicate that we are doing loop unrolling,
     not function inlining.  */
  map->inline_target = 0;

  /* The register and constant maps depend on the number of registers
     present, so the final maps can't be created until after
     find_splittable_regs is called.  However, they are needed for
     preconditioning, so we create temporary maps when preconditioning
     is performed.  */

  /* The preconditioning code may allocate two new pseudo registers.  */
  maxregnum = max_reg_num ();

  /* local_regno is only valid for regnos < max_local_regnum.  */
  max_local_regnum = maxregnum;

  /* Allocate and zero out the splittable_regs and addr_combined_regs
     arrays.  These must be zeroed here because they will be used if
     loop preconditioning is performed, and must be zero for that case.

     It is safe to do this here, since the extra registers created by the
     preconditioning code and find_splittable_regs will never be used
     to access the splittable_regs[] and addr_combined_regs[] arrays.  */

  splittable_regs = xcalloc (maxregnum, sizeof (rtx));
  splittable_regs_updates = xcalloc (maxregnum, sizeof (int));
  addr_combined_regs = xcalloc (maxregnum, sizeof (struct induction *));
  local_regno = xcalloc (maxregnum, sizeof (char));

  /* Mark all local registers, i.e. the ones which are referenced only
     inside the loop.  */
  if (INSN_UID (copy_end) < max_uid_for_loop)
    {
      int copy_start_luid = INSN_LUID (copy_start);
      int copy_end_luid = INSN_LUID (copy_end);

      /* If a register is used in the jump insn, we must not duplicate it
	 since it will also be used outside the loop.  */
      if (GET_CODE (copy_end) == JUMP_INSN)
	copy_end_luid--;

      /* If we have a target that uses cc0, then we also must not duplicate
	 the insn that sets cc0 before the jump insn, if one is present.  */
#ifdef HAVE_cc0
      if (GET_CODE (copy_end) == JUMP_INSN
	  && sets_cc0_p (PREV_INSN (copy_end)))
	copy_end_luid--;
#endif

      /* If copy_start points to the NOTE that starts the loop, then we must
	 use the next luid, because invariant pseudo-regs moved out of the loop
	 have their lifetimes modified to start here, but they are not safe
	 to duplicate.  */
      if (copy_start == loop_start)
	copy_start_luid++;

      /* If a pseudo's lifetime is entirely contained within this loop, then we
	 can use a different pseudo in each unrolled copy of the loop.  This
	 results in better code.  */
      /* We must limit the generic test to max_reg_before_loop, because only
	 these pseudo registers have valid regno_first_uid info.  */
      for (r = FIRST_PSEUDO_REGISTER; r < max_reg_before_loop; ++r)
	if (REGNO_FIRST_UID (r) > 0 && REGNO_FIRST_UID (r) < max_uid_for_loop
	    && REGNO_FIRST_LUID (r) >= copy_start_luid
	    && REGNO_LAST_UID (r) > 0 && REGNO_LAST_UID (r) < max_uid_for_loop
	    && REGNO_LAST_LUID (r) <= copy_end_luid)
	  {
	    /* However, we must also check for loop-carried dependencies.
	       If the value the pseudo has at the end of iteration X is
	       used by iteration X+1, then we can not use a different pseudo
	       for each unrolled copy of the loop.  */
	    /* A pseudo is safe if regno_first_uid is a set, and this
	       set dominates all instructions from regno_first_uid to
	       regno_last_uid.  */
	    /* ??? This check is simplistic.  We would get better code if
	       this check was more sophisticated.  */
	    if (set_dominates_use (r, REGNO_FIRST_UID (r), REGNO_LAST_UID (r),
				   copy_start, copy_end))
	      local_regno[r] = 1;

	    if (loop_dump_stream)
	      {
		if (local_regno[r])
		  fprintf (loop_dump_stream, "Marked reg %d as local\n", r);
		else
		  fprintf (loop_dump_stream, "Did not mark reg %d as local\n",
			   r);
	      }
	  }
    }

  /* If this loop requires exit tests when unrolled, check to see if we
     can precondition the loop so as to make the exit tests unnecessary.
     Just like variable splitting, this is not safe if the loop is entered
     via a jump to the bottom.  Also, can not do this if no strength
     reduce info, because precondition_loop_p uses this info.  */

  /* Must copy the loop body for preconditioning before the following
     find_splittable_regs call since that will emit insns which need to
     be after the preconditioned loop copies, but immediately before the
     unrolled loop copies.  */

  /* Also, it is not safe to split induction variables for the preconditioned
     copies of the loop body.  If we split induction variables, then the code
     assumes that each induction variable can be represented as a function
     of its initial value and the loop iteration number.  This is not true
     in this case, because the last preconditioned copy of the loop body
     could be any iteration from the first up to the `unroll_number-1'th,
     depending on the initial value of the iteration variable.  Therefore
     we can not split induction variables here, because we can not calculate
     their value.  Hence, this code must occur before find_splittable_regs
     is called.  */

  if (unroll_type == UNROLL_NAIVE && ! splitting_not_safe && strength_reduce_p)
    {
      rtx initial_value, final_value, increment;
      enum machine_mode mode;

      if (precondition_loop_p (loop,
			       &initial_value, &final_value, &increment,
			       &mode))
	{
	  rtx diff, insn;
	  rtx *labels;
	  int abs_inc, neg_inc;
	  enum rtx_code cc = loop_info->comparison_code;
	  int less_p     = (cc == LE  || cc == LEU || cc == LT  || cc == LTU);
	  int unsigned_p = (cc == LEU || cc == GEU || cc == LTU || cc == GTU);

	  map->reg_map = xmalloc (maxregnum * sizeof (rtx));

	  VARRAY_CONST_EQUIV_INIT (map->const_equiv_varray, maxregnum,
				   "unroll_loop_precondition");
	  global_const_equiv_varray = map->const_equiv_varray;

	  init_reg_map (map, maxregnum);

	  /* Limit loop unrolling to 4, since this will make 7 copies of
	     the loop body.  */
	  if (unroll_number > 4)
	    unroll_number = 4;

	  /* Save the absolute value of the increment, and also whether or
	     not it is negative.  */
	  neg_inc = 0;
	  abs_inc = INTVAL (increment);
	  if (abs_inc < 0)
	    {
	      abs_inc = -abs_inc;
	      neg_inc = 1;
	    }

	  start_sequence ();

	  /* We must copy the final and initial values here to avoid
	     improperly shared rtl.  */
	  final_value = copy_rtx (final_value);
	  initial_value = copy_rtx (initial_value);

	  /* Final value may have form of (PLUS val1 const1_rtx).  We need
	     to convert it into general operand, so compute the real value.  */

	  final_value = force_operand (final_value, NULL_RTX);
	  if (!nonmemory_operand (final_value, VOIDmode))
	    final_value = force_reg (mode, final_value);

	  /* Calculate the difference between the final and initial values.
	     Final value may be a (plus (reg x) (const_int 1)) rtx.

	     We have to deal with for (i = 0; --i < 6;) type loops.
	     For such loops the real final value is the first time the
	     loop variable overflows, so the diff we calculate is the
	     distance from the overflow value.  This is 0 or ~0 for
	     unsigned loops depending on the direction, or INT_MAX,
	     INT_MAX+1 for signed loops.  We really do not need the
	     exact value, since we are only interested in the diff
	     modulo the increment, and the increment is a power of 2,
	     so we can pretend that the overflow value is 0/~0.  */

	  if (cc == NE || less_p != neg_inc)
	    diff = simplify_gen_binary (MINUS, mode, final_value,
					initial_value);
	  else
	    diff = simplify_gen_unary (neg_inc ? NOT : NEG, mode,
				       initial_value, mode);
	  diff = force_operand (diff, NULL_RTX);

	  /* Now calculate (diff % (unroll * abs (increment))) by using an
	     and instruction.  */
	  diff = simplify_gen_binary (AND, mode, diff,
				      GEN_INT (unroll_number*abs_inc - 1));
	  diff = force_operand (diff, NULL_RTX);

	  /* Now emit a sequence of branches to jump to the proper precond
	     loop entry point.  */

	  labels = xmalloc (sizeof (rtx) * unroll_number);
	  for (i = 0; i < unroll_number; i++)
	    labels[i] = gen_label_rtx ();

	  /* Check for the case where the initial value is greater than or
	     equal to the final value.  In that case, we want to execute
	     exactly one loop iteration.  The code below will fail for this
	     case.  This check does not apply if the loop has a NE
	     comparison at the end.  */

	  if (cc != NE)
	    {
	      rtx incremented_initval;
	      enum rtx_code cmp_code;

	      incremented_initval
		= simplify_gen_binary (PLUS, mode, initial_value, increment);
	      incremented_initval
		= force_operand (incremented_initval, NULL_RTX);

	      cmp_code = (less_p
			  ? (unsigned_p ? GEU : GE)
			  : (unsigned_p ? LEU : LE));

	      insn = simplify_cmp_and_jump_insns (cmp_code, mode,
						  incremented_initval,
						  final_value, labels[1]);
	      if (insn)
	        predict_insn_def (insn, PRED_LOOP_CONDITION, TAKEN);
	    }

	  /* Assuming the unroll_number is 4, and the increment is 2, then
	     for a negative increment:	for a positive increment:
	     diff = 0,1   precond 0	diff = 0,7   precond 0
	     diff = 2,3   precond 3     diff = 1,2   precond 1
	     diff = 4,5   precond 2     diff = 3,4   precond 2
	     diff = 6,7   precond 1     diff = 5,6   precond 3  */

	  /* We only need to emit (unroll_number - 1) branches here, the
	     last case just falls through to the following code.  */

	  /* ??? This would give better code if we emitted a tree of branches
	     instead of the current linear list of branches.  */

	  for (i = 0; i < unroll_number - 1; i++)
	    {
	      int cmp_const;
	      enum rtx_code cmp_code;

	      /* For negative increments, must invert the constant compared
		 against, except when comparing against zero.  */
	      if (i == 0)
		{
		  cmp_const = 0;
		  cmp_code = EQ;
		}
	      else if (neg_inc)
		{
		  cmp_const = unroll_number - i;
		  cmp_code = GE;
		}
	      else
		{
		  cmp_const = i;
		  cmp_code = LE;
		}

	      insn = simplify_cmp_and_jump_insns (cmp_code, mode, diff,
						  GEN_INT (abs_inc*cmp_const),
						  labels[i]);
	      if (insn)
	        predict_insn (insn, PRED_LOOP_PRECONDITIONING,
			      REG_BR_PROB_BASE / (unroll_number - i));
	    }

	  /* If the increment is greater than one, then we need another branch,
	     to handle other cases equivalent to 0.  */

	  /* ??? This should be merged into the code above somehow to help
	     simplify the code here, and reduce the number of branches emitted.
	     For the negative increment case, the branch here could easily
	     be merged with the `0' case branch above.  For the positive
	     increment case, it is not clear how this can be simplified.  */

	  if (abs_inc != 1)
	    {
	      int cmp_const;
	      enum rtx_code cmp_code;

	      if (neg_inc)
		{
		  cmp_const = abs_inc - 1;
		  cmp_code = LE;
		}
	      else
		{
		  cmp_const = abs_inc * (unroll_number - 1) + 1;
		  cmp_code = GE;
		}

	      simplify_cmp_and_jump_insns (cmp_code, mode, diff,
					   GEN_INT (cmp_const), labels[0]);
	    }

	  sequence = get_insns ();
	  end_sequence ();
	  loop_insn_hoist (loop, sequence);

	  /* Only the last copy of the loop body here needs the exit
	     test, so set copy_end to exclude the compare/branch here,
	     and then reset it inside the loop when get to the last
	     copy.  */

	  if (GET_CODE (last_loop_insn) == BARRIER)
	    copy_end = PREV_INSN (PREV_INSN (last_loop_insn));
	  else if (GET_CODE (last_loop_insn) == JUMP_INSN)
	    {
	      copy_end = PREV_INSN (last_loop_insn);
#ifdef HAVE_cc0
	      /* The immediately preceding insn may be a compare which
		 we do not want to copy.  */
	      if (sets_cc0_p (PREV_INSN (copy_end)))
		copy_end = PREV_INSN (copy_end);
#endif
	    }
	  else
	    abort ();

	  for (i = 1; i < unroll_number; i++)
	    {
	      emit_label_after (labels[unroll_number - i],
				PREV_INSN (loop_start));

	      memset (map->insn_map, 0, max_insnno * sizeof (rtx));
	      memset (&VARRAY_CONST_EQUIV (map->const_equiv_varray, 0),
		      0, (VARRAY_SIZE (map->const_equiv_varray)
			  * sizeof (struct const_equiv_data)));
	      map->const_age = 0;

	      for (j = 0; j < max_labelno; j++)
		if (local_label[j])
		  set_label_in_map (map, j, gen_label_rtx ());

	      for (r = FIRST_PSEUDO_REGISTER; r < max_local_regnum; r++)
		if (local_regno[r])
		  {
		    map->reg_map[r]
		      = gen_reg_rtx (GET_MODE (regno_reg_rtx[r]));
		    record_base_value (REGNO (map->reg_map[r]),
				       regno_reg_rtx[r], 0);
		  }
	      /* The last copy needs the compare/branch insns at the end,
		 so reset copy_end here if the loop ends with a conditional
		 branch.  */

	      if (i == unroll_number - 1)
		{
		  if (GET_CODE (last_loop_insn) == BARRIER)
		    copy_end = PREV_INSN (PREV_INSN (last_loop_insn));
		  else
		    copy_end = last_loop_insn;
		}

	      /* None of the copies are the `last_iteration', so just
		 pass zero for that parameter.  */
	      copy_loop_body (loop, copy_start, copy_end, map, exit_label, 0,
			      unroll_type, start_label, loop_end,
			      loop_start, copy_end);
	    }
	  emit_label_after (labels[0], PREV_INSN (loop_start));

	  if (GET_CODE (last_loop_insn) == BARRIER)
	    {
	      insert_before = PREV_INSN (last_loop_insn);
	      copy_end = PREV_INSN (insert_before);
	    }
	  else
	    {
	      insert_before = last_loop_insn;
#ifdef HAVE_cc0
	      /* The instruction immediately before the JUMP_INSN may
		 be a compare instruction which we do not want to copy
		 or delete.  */
	      if (sets_cc0_p (PREV_INSN (insert_before)))
		insert_before = PREV_INSN (insert_before);
#endif
	      copy_end = PREV_INSN (insert_before);
	    }

	  /* Set unroll type to MODULO now.  */
	  unroll_type = UNROLL_MODULO;
	  loop_preconditioned = 1;

	  /* Clean up.  */
	  free (labels);
	}
    }

  /* If reach here, and the loop type is UNROLL_NAIVE, then don't unroll
     the loop unless all loops are being unrolled.  */
  if (unroll_type == UNROLL_NAIVE && ! flag_old_unroll_all_loops)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Unrolling failure: Naive unrolling not being done.\n");
      goto egress;
    }

  /* At this point, we are guaranteed to unroll the loop.  */

  /* Keep track of the unroll factor for the loop.  */
  loop_info->unroll_number = unroll_number;

  /* And whether the loop has been preconditioned.  */
  loop_info->preconditioned = loop_preconditioned;

  /* Remember whether it was preconditioned for the second loop pass.  */
  NOTE_PRECONDITIONED (loop->end) = loop_preconditioned;

  /* For each biv and giv, determine whether it can be safely split into
     a different variable for each unrolled copy of the loop body.
     We precalculate and save this info here, since computing it is
     expensive.

     Do this before deleting any instructions from the loop, so that
     back_branch_in_range_p will work correctly.  */

  if (splitting_not_safe)
    temp = 0;
  else
    temp = find_splittable_regs (loop, unroll_type, unroll_number);

  /* find_splittable_regs may have created some new registers, so must
     reallocate the reg_map with the new larger size, and must realloc
     the constant maps also.  */

  maxregnum = max_reg_num ();
  map->reg_map = xmalloc (maxregnum * sizeof (rtx));

  init_reg_map (map, maxregnum);

  if (map->const_equiv_varray == 0)
    VARRAY_CONST_EQUIV_INIT (map->const_equiv_varray,
			     maxregnum + temp * unroll_number * 2,
			     "unroll_loop");
  global_const_equiv_varray = map->const_equiv_varray;

  /* Search the list of bivs and givs to find ones which need to be remapped
     when split, and set their reg_map entry appropriately.  */

  for (bl = ivs->list; bl; bl = bl->next)
    {
      if (REGNO (bl->biv->src_reg) != bl->regno)
	map->reg_map[bl->regno] = bl->biv->src_reg;
#if 0
      /* Currently, non-reduced/final-value givs are never split.  */
      for (v = bl->giv; v; v = v->next_iv)
	if (REGNO (v->src_reg) != bl->regno)
	  map->reg_map[REGNO (v->dest_reg)] = v->src_reg;
#endif
    }

  /* Use our current register alignment and pointer flags.  */
  map->regno_pointer_align = cfun->emit->regno_pointer_align;
  map->x_regno_reg_rtx = cfun->emit->x_regno_reg_rtx;

  /* If the loop is being partially unrolled, and the iteration variables
     are being split, and are being renamed for the split, then must fix up
     the compare/jump instruction at the end of the loop to refer to the new
     registers.  This compare isn't copied, so the registers used in it
     will never be replaced if it isn't done here.  */

  if (unroll_type == UNROLL_MODULO)
    {
      insn = NEXT_INSN (copy_end);
      if (GET_CODE (insn) == INSN || GET_CODE (insn) == JUMP_INSN)
	PATTERN (insn) = remap_split_bivs (loop, PATTERN (insn));
    }

  /* For unroll_number times, make a copy of each instruction
     between copy_start and copy_end, and insert these new instructions
     before the end of the loop.  */

  for (i = 0; i < unroll_number; i++)
    {
      memset (map->insn_map, 0, max_insnno * sizeof (rtx));
      memset (&VARRAY_CONST_EQUIV (map->const_equiv_varray, 0), 0,
	      VARRAY_SIZE (map->const_equiv_varray) * sizeof (struct const_equiv_data));
      map->const_age = 0;

      for (j = 0; j < max_labelno; j++)
	if (local_label[j])
	  set_label_in_map (map, j, gen_label_rtx ());

      for (r = FIRST_PSEUDO_REGISTER; r < max_local_regnum; r++)
	if (local_regno[r])
	  {
	    map->reg_map[r] = gen_reg_rtx (GET_MODE (regno_reg_rtx[r]));
	    record_base_value (REGNO (map->reg_map[r]),
			       regno_reg_rtx[r], 0);
	  }

      /* If loop starts with a branch to the test, then fix it so that
	 it points to the test of the first unrolled copy of the loop.  */
      if (i == 0 && loop_start != copy_start)
	{
	  insn = PREV_INSN (copy_start);
	  pattern = PATTERN (insn);

	  tem = get_label_from_map (map,
				    CODE_LABEL_NUMBER
				    (XEXP (SET_SRC (pattern), 0)));
	  SET_SRC (pattern) = gen_rtx_LABEL_REF (VOIDmode, tem);

	  /* Set the jump label so that it can be used by later loop unrolling
	     passes.  */
	  JUMP_LABEL (insn) = tem;
	  LABEL_NUSES (tem)++;
	}

      copy_loop_body (loop, copy_start, copy_end, map, exit_label,
		      i == unroll_number - 1, unroll_type, start_label,
		      loop_end, insert_before, insert_before);
    }

  /* Before deleting any insns, emit a CODE_LABEL immediately after the last
     insn to be deleted.  This prevents any runaway delete_insn call from
     more insns that it should, as it always stops at a CODE_LABEL.  */

  /* Delete the compare and branch at the end of the loop if completely
     unrolling the loop.  Deleting the backward branch at the end also
     deletes the code label at the start of the loop.  This is done at
     the very end to avoid problems with back_branch_in_range_p.  */

  if (unroll_type == UNROLL_COMPLETELY)
    safety_label = emit_label_after (gen_label_rtx (), last_loop_insn);
  else
    safety_label = emit_label_after (gen_label_rtx (), copy_end);

  /* Delete all of the original loop instructions.  Don't delete the
     LOOP_BEG note, or the first code label in the loop.  */

  insn = NEXT_INSN (copy_start);
  while (insn != safety_label)
    {
      /* ??? Don't delete named code labels.  They will be deleted when the
	 jump that references them is deleted.  Otherwise, we end up deleting
	 them twice, which causes them to completely disappear instead of turn
	 into NOTE_INSN_DELETED_LABEL notes.  This in turn causes aborts in
	 dwarfout.c/dwarf2out.c.  We could perhaps fix the dwarf*out.c files
	 to handle deleted labels instead.  Or perhaps fix DECL_RTL of the
	 associated LABEL_DECL to point to one of the new label instances.  */
      /* ??? Likewise, we can't delete a NOTE_INSN_DELETED_LABEL note.  */
      if (insn != start_label
	  && ! (GET_CODE (insn) == CODE_LABEL && LABEL_NAME (insn))
	  && ! (GET_CODE (insn) == NOTE
		&& NOTE_LINE_NUMBER (insn) == NOTE_INSN_DELETED_LABEL))
	insn = delete_related_insns (insn);
      else
	insn = NEXT_INSN (insn);
    }

  /* Can now delete the 'safety' label emitted to protect us from runaway
     delete_related_insns calls.  */
  if (INSN_DELETED_P (safety_label))
    abort ();
  delete_related_insns (safety_label);

  /* If exit_label exists, emit it after the loop.  Doing the emit here
     forces it to have a higher INSN_UID than any insn in the unrolled loop.
     This is needed so that mostly_true_jump in reorg.c will treat jumps
     to this loop end label correctly, i.e. predict that they are usually
     not taken.  */
  if (exit_label)
    emit_label_after (exit_label, loop_end);

 egress:
  if (unroll_type == UNROLL_COMPLETELY)
    {
      /* Remove the loop notes since this is no longer a loop.  */
      if (loop->vtop)
	delete_related_insns (loop->vtop);
      if (loop->cont)
	delete_related_insns (loop->cont);
      if (loop_start)
	delete_related_insns (loop_start);
      if (loop_end)
	delete_related_insns (loop_end);
    }

  if (map->const_equiv_varray)
    VARRAY_FREE (map->const_equiv_varray);
  if (map->label_map)
    {
      free (map->label_map);
      free (local_label);
    }
  free (map->insn_map);
  free (splittable_regs);
  free (splittable_regs_updates);
  free (addr_combined_regs);
  free (local_regno);
  if (map->reg_map)
    free (map->reg_map);
  free (map);
}

/* A helper function for unroll_loop.  Emit a compare and branch to
   satisfy (CMP OP1 OP2), but pass this through the simplifier first.
   If the branch turned out to be conditional, return it, otherwise
   return NULL.  */

static rtx
simplify_cmp_and_jump_insns (enum rtx_code code, enum machine_mode mode,
			     rtx op0, rtx op1, rtx label)
{
  rtx t, insn;

  t = simplify_relational_operation (code, mode, op0, op1);
  if (!t)
    {
      enum rtx_code scode = signed_condition (code);
      emit_cmp_and_jump_insns (op0, op1, scode, NULL_RTX, mode,
			       code != scode, label);
      insn = get_last_insn ();

      JUMP_LABEL (insn) = label;
      LABEL_NUSES (label) += 1;

      return insn;
    }
  else if (t == const_true_rtx)
    {
      insn = emit_jump_insn (gen_jump (label));
      emit_barrier ();
      JUMP_LABEL (insn) = label;
      LABEL_NUSES (label) += 1;
    }

  return NULL_RTX;
}

/* Return true if the loop can be safely, and profitably, preconditioned
   so that the unrolled copies of the loop body don't need exit tests.

   This only works if final_value, initial_value and increment can be
   determined, and if increment is a constant power of 2.
   If increment is not a power of 2, then the preconditioning modulo
   operation would require a real modulo instead of a boolean AND, and this
   is not considered `profitable'.  */

/* ??? If the loop is known to be executed very many times, or the machine
   has a very cheap divide instruction, then preconditioning is a win even
   when the increment is not a power of 2.  Use RTX_COST to compute
   whether divide is cheap.
   ??? A divide by constant doesn't actually need a divide, look at
   expand_divmod.  The reduced cost of this optimized modulo is not
   reflected in RTX_COST.  */

int
precondition_loop_p (const struct loop *loop, rtx *initial_value,
		     rtx *final_value, rtx *increment,
		     enum machine_mode *mode)
{
  rtx loop_start = loop->start;
  struct loop_info *loop_info = LOOP_INFO (loop);

  if (loop_info->n_iterations > 0)
    {
      if (INTVAL (loop_info->increment) > 0)
	{
	  *initial_value = const0_rtx;
	  *increment = const1_rtx;
	  *final_value = GEN_INT (loop_info->n_iterations);
	}
      else
	{
	  *initial_value = GEN_INT (loop_info->n_iterations);
	  *increment = constm1_rtx;
	  *final_value = const0_rtx;
	}
      *mode = word_mode;

      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: Success, number of iterations known, "
		 HOST_WIDE_INT_PRINT_DEC ".\n",
		 loop_info->n_iterations);
      return 1;
    }

  if (loop_info->iteration_var == 0)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: Could not find iteration variable.\n");
      return 0;
    }
  else if (loop_info->initial_value == 0)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: Could not find initial value.\n");
      return 0;
    }
  else if (loop_info->increment == 0)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: Could not find increment value.\n");
      return 0;
    }
  else if (GET_CODE (loop_info->increment) != CONST_INT)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: Increment not a constant.\n");
      return 0;
    }
  else if ((exact_log2 (INTVAL (loop_info->increment)) < 0)
	   && (exact_log2 (-INTVAL (loop_info->increment)) < 0))
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: Increment not a constant power of 2.\n");
      return 0;
    }

  /* Unsigned_compare and compare_dir can be ignored here, since they do
     not matter for preconditioning.  */

  if (loop_info->final_value == 0)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: EQ comparison loop.\n");
      return 0;
    }

  /* Must ensure that final_value is invariant, so call
     loop_invariant_p to check.  Before doing so, must check regno
     against max_reg_before_loop to make sure that the register is in
     the range covered by loop_invariant_p.  If it isn't, then it is
     most likely a biv/giv which by definition are not invariant.  */
  if ((GET_CODE (loop_info->final_value) == REG
       && REGNO (loop_info->final_value) >= max_reg_before_loop)
      || (GET_CODE (loop_info->final_value) == PLUS
	  && REGNO (XEXP (loop_info->final_value, 0)) >= max_reg_before_loop)
      || ! loop_invariant_p (loop, loop_info->final_value))
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: Final value not invariant.\n");
      return 0;
    }

  /* Fail for floating point values, since the caller of this function
     does not have code to deal with them.  */
  if (GET_MODE_CLASS (GET_MODE (loop_info->final_value)) == MODE_FLOAT
      || GET_MODE_CLASS (GET_MODE (loop_info->initial_value)) == MODE_FLOAT)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: Floating point final or initial value.\n");
      return 0;
    }

  /* Fail if loop_info->iteration_var is not live before loop_start,
     since we need to test its value in the preconditioning code.  */

  if (REGNO_FIRST_LUID (REGNO (loop_info->iteration_var))
      > INSN_LUID (loop_start))
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: Iteration var not live before loop start.\n");
      return 0;
    }

  /* Note that loop_iterations biases the initial value for GIV iterators
     such as "while (i-- > 0)" so that we can calculate the number of
     iterations just like for BIV iterators.

     Also note that the absolute values of initial_value and
     final_value are unimportant as only their difference is used for
     calculating the number of loop iterations.  */
  *initial_value = loop_info->initial_value;
  *increment = loop_info->increment;
  *final_value = loop_info->final_value;

  /* Decide what mode to do these calculations in.  Choose the larger
     of final_value's mode and initial_value's mode, or a full-word if
     both are constants.  */
  *mode = GET_MODE (*final_value);
  if (*mode == VOIDmode)
    {
      *mode = GET_MODE (*initial_value);
      if (*mode == VOIDmode)
	*mode = word_mode;
    }
  else if (*mode != GET_MODE (*initial_value)
	   && (GET_MODE_SIZE (*mode)
	       < GET_MODE_SIZE (GET_MODE (*initial_value))))
    *mode = GET_MODE (*initial_value);

  /* Success!  */
  if (loop_dump_stream)
    fprintf (loop_dump_stream, "Preconditioning: Successful.\n");
  return 1;
}

/* All pseudo-registers must be mapped to themselves.  Two hard registers
   must be mapped, VIRTUAL_STACK_VARS_REGNUM and VIRTUAL_INCOMING_ARGS_
   REGNUM, to avoid function-inlining specific conversions of these
   registers.  All other hard regs can not be mapped because they may be
   used with different
   modes.  */

static void
init_reg_map (struct inline_remap *map, int maxregnum)
{
  int i;

  for (i = maxregnum - 1; i > LAST_VIRTUAL_REGISTER; i--)
    map->reg_map[i] = regno_reg_rtx[i];
  /* Just clear the rest of the entries.  */
  for (i = LAST_VIRTUAL_REGISTER; i >= 0; i--)
    map->reg_map[i] = 0;

  map->reg_map[VIRTUAL_STACK_VARS_REGNUM]
    = regno_reg_rtx[VIRTUAL_STACK_VARS_REGNUM];
  map->reg_map[VIRTUAL_INCOMING_ARGS_REGNUM]
    = regno_reg_rtx[VIRTUAL_INCOMING_ARGS_REGNUM];
}

/* Strength-reduction will often emit code for optimized biv/givs which
   calculates their value in a temporary register, and then copies the result
   to the iv.  This procedure reconstructs the pattern computing the iv;
   verifying that all operands are of the proper form.

   PATTERN must be the result of single_set.
   The return value is the amount that the giv is incremented by.  */

static rtx
calculate_giv_inc (rtx pattern, rtx src_insn, unsigned int regno)
{
  rtx increment;
  rtx increment_total = 0;
  int tries = 0;

 retry:
  /* Verify that we have an increment insn here.  First check for a plus
     as the set source.  */
  if (GET_CODE (SET_SRC (pattern)) != PLUS)
    {
      /* SR sometimes computes the new giv value in a temp, then copies it
	 to the new_reg.  */
      src_insn = PREV_INSN (src_insn);
      pattern = single_set (src_insn);
      if (GET_CODE (SET_SRC (pattern)) != PLUS)
	abort ();

      /* The last insn emitted is not needed, so delete it to avoid confusing
	 the second cse pass.  This insn sets the giv unnecessarily.  */
      delete_related_insns (get_last_insn ());
    }

  /* Verify that we have a constant as the second operand of the plus.  */
  increment = XEXP (SET_SRC (pattern), 1);
  if (GET_CODE (increment) != CONST_INT)
    {
      /* SR sometimes puts the constant in a register, especially if it is
	 too big to be an add immed operand.  */
      increment = find_last_value (increment, &src_insn, NULL_RTX, 0);

      /* SR may have used LO_SUM to compute the constant if it is too large
	 for a load immed operand.  In this case, the constant is in operand
	 one of the LO_SUM rtx.  */
      if (GET_CODE (increment) == LO_SUM)
	increment = XEXP (increment, 1);

      /* Some ports store large constants in memory and add a REG_EQUAL
	 note to the store insn.  */
      else if (GET_CODE (increment) == MEM)
	{
	  rtx note = find_reg_note (src_insn, REG_EQUAL, 0);
	  if (note)
	    increment = XEXP (note, 0);
	}

      else if (GET_CODE (increment) == IOR
	       || GET_CODE (increment) == PLUS
	       || GET_CODE (increment) == ASHIFT
	       || GET_CODE (increment) == LSHIFTRT)
	{
	  /* The rs6000 port loads some constants with IOR.
	     The alpha port loads some constants with ASHIFT and PLUS.
	     The sparc64 port loads some constants with LSHIFTRT.  */
	  rtx second_part = XEXP (increment, 1);
	  enum rtx_code code = GET_CODE (increment);

	  increment = find_last_value (XEXP (increment, 0),
				       &src_insn, NULL_RTX, 0);
	  /* Don't need the last insn anymore.  */
	  delete_related_insns (get_last_insn ());

	  if (GET_CODE (second_part) != CONST_INT
	      || GET_CODE (increment) != CONST_INT)
	    abort ();

	  if (code == IOR)
	    increment = GEN_INT (INTVAL (increment) | INTVAL (second_part));
	  else if (code == PLUS)
	    increment = GEN_INT (INTVAL (increment) + INTVAL (second_part));
	  else if (code == ASHIFT)
	    increment = GEN_INT (INTVAL (increment) << INTVAL (second_part));
	  else
	    increment = GEN_INT ((unsigned HOST_WIDE_INT) INTVAL (increment) >> INTVAL (second_part));
	}

      if (GET_CODE (increment) != CONST_INT)
	abort ();

      /* The insn loading the constant into a register is no longer needed,
	 so delete it.  */
      delete_related_insns (get_last_insn ());
    }

  if (increment_total)
    increment_total = GEN_INT (INTVAL (increment_total) + INTVAL (increment));
  else
    increment_total = increment;

  /* Check that the source register is the same as the register we expected
     to see as the source.  If not, something is seriously wrong.  */
  if (GET_CODE (XEXP (SET_SRC (pattern), 0)) != REG
      || REGNO (XEXP (SET_SRC (pattern), 0)) != regno)
    {
      /* Some machines (e.g. the romp), may emit two add instructions for
	 certain constants, so lets try looking for another add immediately
	 before this one if we have only seen one add insn so far.  */

      if (tries == 0)
	{
	  tries++;

	  src_insn = PREV_INSN (src_insn);
	  pattern = single_set (src_insn);

	  delete_related_insns (get_last_insn ());

	  goto retry;
	}

      abort ();
    }

  return increment_total;
}

/* Copy REG_NOTES, except for insn references, because not all insn_map
   entries are valid yet.  We do need to copy registers now though, because
   the reg_map entries can change during copying.  */

static rtx
initial_reg_note_copy (rtx notes, struct inline_remap *map)
{
  rtx copy;

  if (notes == 0)
    return 0;

  copy = rtx_alloc (GET_CODE (notes));
  PUT_REG_NOTE_KIND (copy, REG_NOTE_KIND (notes));

  if (GET_CODE (notes) == EXPR_LIST)
    XEXP (copy, 0) = copy_rtx_and_substitute (XEXP (notes, 0), map, 0);
  else if (GET_CODE (notes) == INSN_LIST)
    /* Don't substitute for these yet.  */
    XEXP (copy, 0) = copy_rtx (XEXP (notes, 0));
  else
    abort ();

  XEXP (copy, 1) = initial_reg_note_copy (XEXP (notes, 1), map);

  return copy;
}

/* Fixup insn references in copied REG_NOTES.  */

static void
final_reg_note_copy (rtx *notesp, struct inline_remap *map)
{
  while (*notesp)
    {
      rtx note = *notesp;

      if (GET_CODE (note) == INSN_LIST)
	{
	  rtx insn = map->insn_map[INSN_UID (XEXP (note, 0))];

	  /* If we failed to remap the note, something is awry.
	     Allow REG_LABEL as it may reference label outside
	     the unrolled loop.  */
	  if (!insn)
	    {
	      if (REG_NOTE_KIND (note) != REG_LABEL)
		abort ();
	    }
	  else
	    XEXP (note, 0) = insn;
	}

      notesp = &XEXP (note, 1);
    }
}

/* Copy each instruction in the loop, substituting from map as appropriate.
   This is very similar to a loop in expand_inline_function.  */

static void
copy_loop_body (struct loop *loop, rtx copy_start, rtx copy_end,
		struct inline_remap *map, rtx exit_label,
		int last_iteration, enum unroll_types unroll_type,
		rtx start_label, rtx loop_end, rtx insert_before,
		rtx copy_notes_from)
{
  struct loop_ivs *ivs = LOOP_IVS (loop);
  rtx insn, pattern;
  rtx set, tem, copy = NULL_RTX;
  int dest_reg_was_split, i;
#ifdef HAVE_cc0
  rtx cc0_insn = 0;
#endif
  rtx final_label = 0;
  rtx giv_inc, giv_dest_reg, giv_src_reg;

  /* If this isn't the last iteration, then map any references to the
     start_label to final_label.  Final label will then be emitted immediately
     after the end of this loop body if it was ever used.

     If this is the last iteration, then map references to the start_label
     to itself.  */
  if (! last_iteration)
    {
      final_label = gen_label_rtx ();
      set_label_in_map (map, CODE_LABEL_NUMBER (start_label), final_label);
    }
  else
    set_label_in_map (map, CODE_LABEL_NUMBER (start_label), start_label);

  start_sequence ();

  insn = copy_start;
  do
    {
      insn = NEXT_INSN (insn);

      map->orig_asm_operands_vector = 0;

      switch (GET_CODE (insn))
	{
	case INSN:
	  pattern = PATTERN (insn);
	  copy = 0;
	  giv_inc = 0;

	  /* Check to see if this is a giv that has been combined with
	     some split address givs.  (Combined in the sense that
	     `combine_givs' in loop.c has put two givs in the same register.)
	     In this case, we must search all givs based on the same biv to
	     find the address givs.  Then split the address givs.
	     Do this before splitting the giv, since that may map the
	     SET_DEST to a new register.  */

	  if ((set = single_set (insn))
	      && GET_CODE (SET_DEST (set)) == REG
	      && addr_combined_regs[REGNO (SET_DEST (set))])
	    {
	      struct iv_class *bl;
	      struct induction *v, *tv;
	      unsigned int regno = REGNO (SET_DEST (set));

	      v = addr_combined_regs[REGNO (SET_DEST (set))];
	      bl = REG_IV_CLASS (ivs, REGNO (v->src_reg));

	      /* Although the giv_inc amount is not needed here, we must call
		 calculate_giv_inc here since it might try to delete the
		 last insn emitted.  If we wait until later to call it,
		 we might accidentally delete insns generated immediately
		 below by emit_unrolled_add.  */

	      giv_inc = calculate_giv_inc (set, insn, regno);

	      /* Now find all address giv's that were combined with this
		 giv 'v'.  */
	      for (tv = bl->giv; tv; tv = tv->next_iv)
		if (tv->giv_type == DEST_ADDR && tv->same == v)
		  {
		    int this_giv_inc;

		    /* If this DEST_ADDR giv was not split, then ignore it.  */
		    if (*tv->location != tv->dest_reg)
		      continue;

		    /* Scale this_giv_inc if the multiplicative factors of
		       the two givs are different.  */
		    this_giv_inc = INTVAL (giv_inc);
		    if (tv->mult_val != v->mult_val)
		      this_giv_inc = (this_giv_inc / INTVAL (v->mult_val)
				      * INTVAL (tv->mult_val));

		    tv->dest_reg = plus_constant (tv->dest_reg, this_giv_inc);
		    *tv->location = tv->dest_reg;

		    if (last_iteration && unroll_type != UNROLL_COMPLETELY)
		      {
			/* Must emit an insn to increment the split address
			   giv.  Add in the const_adjust field in case there
			   was a constant eliminated from the address.  */
			rtx value, dest_reg;

			/* tv->dest_reg will be either a bare register,
			   or else a register plus a constant.  */
			if (GET_CODE (tv->dest_reg) == REG)
			  dest_reg = tv->dest_reg;
			else
			  dest_reg = XEXP (tv->dest_reg, 0);

			/* Check for shared address givs, and avoid
			   incrementing the shared pseudo reg more than
			   once.  */
			if (! tv->same_insn && ! tv->shared)
			  {
			    /* tv->dest_reg may actually be a (PLUS (REG)
			       (CONST)) here, so we must call plus_constant
			       to add the const_adjust amount before calling
			       emit_unrolled_add below.  */
			    value = plus_constant (tv->dest_reg,
						   tv->const_adjust);

			    if (GET_CODE (value) == PLUS)
			      {
				/* The constant could be too large for an add
				   immediate, so can't directly emit an insn
				   here.  */
				emit_unrolled_add (dest_reg, XEXP (value, 0),
						   XEXP (value, 1));
			      }
			  }

			/* Reset the giv to be just the register again, in case
			   it is used after the set we have just emitted.
			   We must subtract the const_adjust factor added in
			   above.  */
			tv->dest_reg = plus_constant (dest_reg,
						      -tv->const_adjust);
			*tv->location = tv->dest_reg;
		      }
		  }
	    }

	  /* If this is a setting of a splittable variable, then determine
	     how to split the variable, create a new set based on this split,
	     and set up the reg_map so that later uses of the variable will
	     use the new split variable.  */

	  dest_reg_was_split = 0;

	  if ((set = single_set (insn))
	      && GET_CODE (SET_DEST (set)) == REG
	      && splittable_regs[REGNO (SET_DEST (set))])
	    {
	      unsigned int regno = REGNO (SET_DEST (set));
	      unsigned int src_regno;

	      dest_reg_was_split = 1;

	      giv_dest_reg = SET_DEST (set);
	      giv_src_reg = giv_dest_reg;
	      /* Compute the increment value for the giv, if it wasn't
		 already computed above.  */
	      if (giv_inc == 0)
		giv_inc = calculate_giv_inc (set, insn, regno);

	      src_regno = REGNO (giv_src_reg);

	      if (unroll_type == UNROLL_COMPLETELY)
		{
		  /* Completely unrolling the loop.  Set the induction
		     variable to a known constant value.  */

		  /* The value in splittable_regs may be an invariant
		     value, so we must use plus_constant here.  */
		  splittable_regs[regno]
		    = plus_constant (splittable_regs[src_regno],
				     INTVAL (giv_inc));

		  if (GET_CODE (splittable_regs[regno]) == PLUS)
		    {
		      giv_src_reg = XEXP (splittable_regs[regno], 0);
		      giv_inc = XEXP (splittable_regs[regno], 1);
		    }
		  else
		    {
		      /* The splittable_regs value must be a REG or a
			 CONST_INT, so put the entire value in the giv_src_reg
			 variable.  */
		      giv_src_reg = splittable_regs[regno];
		      giv_inc = const0_rtx;
		    }
		}
	      else
		{
		  /* Partially unrolling loop.  Create a new pseudo
		     register for the iteration variable, and set it to
		     be a constant plus the original register.  Except
		     on the last iteration, when the result has to
		     go back into the original iteration var register.  */

		  /* Handle bivs which must be mapped to a new register
		     when split.  This happens for bivs which need their
		     final value set before loop entry.  The new register
		     for the biv was stored in the biv's first struct
		     induction entry by find_splittable_regs.  */

		  if (regno < ivs->n_regs
		      && REG_IV_TYPE (ivs, regno) == BASIC_INDUCT)
		    {
		      giv_src_reg = REG_IV_CLASS (ivs, regno)->biv->src_reg;
		      giv_dest_reg = giv_src_reg;
		    }

#if 0
		  /* If non-reduced/final-value givs were split, then
		     this would have to remap those givs also.  See
		     find_splittable_regs.  */
#endif

		  splittable_regs[regno]
		    = simplify_gen_binary (PLUS, GET_MODE (giv_src_reg),
					   giv_inc,
					   splittable_regs[src_regno]);
		  giv_inc = splittable_regs[regno];

		  /* Now split the induction variable by changing the dest
		     of this insn to a new register, and setting its
		     reg_map entry to point to this new register.

		     If this is the last iteration, and this is the last insn
		     that will update the iv, then reuse the original dest,
		     to ensure that the iv will have the proper value when
		     the loop exits or repeats.

		     Using splittable_regs_updates here like this is safe,
		     because it can only be greater than one if all
		     instructions modifying the iv are always executed in
		     order.  */

		  if (! last_iteration
		      || (splittable_regs_updates[regno]-- != 1))
		    {
		      tem = gen_reg_rtx (GET_MODE (giv_src_reg));
		      giv_dest_reg = tem;
		      map->reg_map[regno] = tem;
		      record_base_value (REGNO (tem),
					 giv_inc == const0_rtx
					 ? giv_src_reg
					 : gen_rtx_PLUS (GET_MODE (giv_src_reg),
							 giv_src_reg, giv_inc),
					 1);
		    }
		  else
		    map->reg_map[regno] = giv_src_reg;
		}

	      /* The constant being added could be too large for an add
		 immediate, so can't directly emit an insn here.  */
	      emit_unrolled_add (giv_dest_reg, giv_src_reg, giv_inc);
	      copy = get_last_insn ();
	      pattern = PATTERN (copy);
	    }
	  else
	    {
	      pattern = copy_rtx_and_substitute (pattern, map, 0);
	      copy = emit_insn (pattern);
	    }
	  REG_NOTES (copy) = initial_reg_note_copy (REG_NOTES (insn), map);
	  INSN_LOCATOR (copy) = INSN_LOCATOR (insn);

	  /* If there is a REG_EQUAL note present whose value
	     is not loop invariant, then delete it, since it
	     may cause problems with later optimization passes.  */
	  if ((tem = find_reg_note (copy, REG_EQUAL, NULL_RTX))
	      && !loop_invariant_p (loop, XEXP (tem, 0)))
	    remove_note (copy, tem);

#ifdef HAVE_cc0
	  /* If this insn is setting CC0, it may need to look at
	     the insn that uses CC0 to see what type of insn it is.
	     In that case, the call to recog via validate_change will
	     fail.  So don't substitute constants here.  Instead,
	     do it when we emit the following insn.

	     For example, see the pyr.md file.  That machine has signed and
	     unsigned compares.  The compare patterns must check the
	     following branch insn to see which what kind of compare to
	     emit.

	     If the previous insn set CC0, substitute constants on it as
	     well.  */
	  if (sets_cc0_p (PATTERN (copy)) != 0)
	    cc0_insn = copy;
	  else
	    {
	      if (cc0_insn)
		try_constants (cc0_insn, map);
	      cc0_insn = 0;
	      try_constants (copy, map);
	    }
#else
	  try_constants (copy, map);
#endif

	  /* Make split induction variable constants `permanent' since we
	     know there are no backward branches across iteration variable
	     settings which would invalidate this.  */
	  if (dest_reg_was_split)
	    {
	      int regno = REGNO (SET_DEST (set));

	      if ((size_t) regno < VARRAY_SIZE (map->const_equiv_varray)
		  && (VARRAY_CONST_EQUIV (map->const_equiv_varray, regno).age
		      == map->const_age))
		VARRAY_CONST_EQUIV (map->const_equiv_varray, regno).age = -1;
	    }
	  break;

	case JUMP_INSN:
	  pattern = copy_rtx_and_substitute (PATTERN (insn), map, 0);
	  copy = emit_jump_insn (pattern);
	  REG_NOTES (copy) = initial_reg_note_copy (REG_NOTES (insn), map);
	  INSN_LOCATOR (copy) = INSN_LOCATOR (insn);

	  if (JUMP_LABEL (insn))
	    {
	      JUMP_LABEL (copy) = get_label_from_map (map,
						      CODE_LABEL_NUMBER
						      (JUMP_LABEL (insn)));
	      LABEL_NUSES (JUMP_LABEL (copy))++;
	    }
	  if (JUMP_LABEL (insn) == start_label && insn == copy_end
	      && ! last_iteration)
	    {

	      /* This is a branch to the beginning of the loop; this is the
		 last insn being copied; and this is not the last iteration.
		 In this case, we want to change the original fall through
		 case to be a branch past the end of the loop, and the
		 original jump label case to fall_through.  */

	      if (!invert_jump (copy, exit_label, 0))
		{
		  rtx jmp;
		  rtx lab = gen_label_rtx ();
		  /* Can't do it by reversing the jump (probably because we
		     couldn't reverse the conditions), so emit a new
		     jump_insn after COPY, and redirect the jump around
		     that.  */
		  jmp = emit_jump_insn_after (gen_jump (exit_label), copy);
		  JUMP_LABEL (jmp) = exit_label;
		  LABEL_NUSES (exit_label)++;
		  jmp = emit_barrier_after (jmp);
		  emit_label_after (lab, jmp);
		  LABEL_NUSES (lab) = 0;
		  if (!redirect_jump (copy, lab, 0))
		    abort ();
		}
	    }

#ifdef HAVE_cc0
	  if (cc0_insn)
	    try_constants (cc0_insn, map);
	  cc0_insn = 0;
#endif
	  try_constants (copy, map);

	  /* Set the jump label of COPY correctly to avoid problems with
	     later passes of unroll_loop, if INSN had jump label set.  */
	  if (JUMP_LABEL (insn))
	    {
	      rtx label = 0;

	      /* Can't use the label_map for every insn, since this may be
		 the backward branch, and hence the label was not mapped.  */
	      if ((set = single_set (copy)))
		{
		  tem = SET_SRC (set);
		  if (GET_CODE (tem) == LABEL_REF)
		    label = XEXP (tem, 0);
		  else if (GET_CODE (tem) == IF_THEN_ELSE)
		    {
		      if (XEXP (tem, 1) != pc_rtx)
			label = XEXP (XEXP (tem, 1), 0);
		      else
			label = XEXP (XEXP (tem, 2), 0);
		    }
		}

	      if (label && GET_CODE (label) == CODE_LABEL)
		JUMP_LABEL (copy) = label;
	      else
		{
		  /* An unrecognizable jump insn, probably the entry jump
		     for a switch statement.  This label must have been mapped,
		     so just use the label_map to get the new jump label.  */
		  JUMP_LABEL (copy)
		    = get_label_from_map (map,
					  CODE_LABEL_NUMBER (JUMP_LABEL (insn)));
		}

	      /* If this is a non-local jump, then must increase the label
		 use count so that the label will not be deleted when the
		 original jump is deleted.  */
	      LABEL_NUSES (JUMP_LABEL (copy))++;
	    }
	  else if (GET_CODE (PATTERN (copy)) == ADDR_VEC
		   || GET_CODE (PATTERN (copy)) == ADDR_DIFF_VEC)
	    {
	      rtx pat = PATTERN (copy);
	      int diff_vec_p = GET_CODE (pat) == ADDR_DIFF_VEC;
	      int len = XVECLEN (pat, diff_vec_p);
	      int i;

	      for (i = 0; i < len; i++)
		LABEL_NUSES (XEXP (XVECEXP (pat, diff_vec_p, i), 0))++;
	    }

	  /* If this used to be a conditional jump insn but whose branch
	     direction is now known, we must do something special.  */
	  if (any_condjump_p (insn) && onlyjump_p (insn) && map->last_pc_value)
	    {
#ifdef HAVE_cc0
	      /* If the previous insn set cc0 for us, delete it.  */
	      if (only_sets_cc0_p (PREV_INSN (copy)))
		delete_related_insns (PREV_INSN (copy));
#endif

	      /* If this is now a no-op, delete it.  */
	      if (map->last_pc_value == pc_rtx)
		{
		  delete_insn (copy);
		  copy = 0;
		}
	      else
		/* Otherwise, this is unconditional jump so we must put a
		   BARRIER after it.  We could do some dead code elimination
		   here, but jump.c will do it just as well.  */
		emit_barrier ();
	    }
	  break;

	case CALL_INSN:
	  pattern = copy_rtx_and_substitute (PATTERN (insn), map, 0);
	  copy = emit_call_insn (pattern);
	  REG_NOTES (copy) = initial_reg_note_copy (REG_NOTES (insn), map);
	  INSN_LOCATOR (copy) = INSN_LOCATOR (insn);
	  SIBLING_CALL_P (copy) = SIBLING_CALL_P (insn);
	  CONST_OR_PURE_CALL_P (copy) = CONST_OR_PURE_CALL_P (insn);

	  /* Because the USAGE information potentially contains objects other
	     than hard registers, we need to copy it.  */
	  CALL_INSN_FUNCTION_USAGE (copy)
	    = copy_rtx_and_substitute (CALL_INSN_FUNCTION_USAGE (insn),
				       map, 0);

#ifdef HAVE_cc0
	  if (cc0_insn)
	    try_constants (cc0_insn, map);
	  cc0_insn = 0;
#endif
	  try_constants (copy, map);

	  /* Be lazy and assume CALL_INSNs clobber all hard registers.  */
	  for (i = 0; i < FIRST_PSEUDO_REGISTER; i++)
	    VARRAY_CONST_EQUIV (map->const_equiv_varray, i).rtx = 0;
	  break;

	case CODE_LABEL:
	  /* If this is the loop start label, then we don't need to emit a
	     copy of this label since no one will use it.  */

	  if (insn != start_label)
	    {
	      copy = emit_label (get_label_from_map (map,
						     CODE_LABEL_NUMBER (insn)));
	      map->const_age++;
	    }
	  break;

	case BARRIER:
	  copy = emit_barrier ();
	  break;

	case NOTE:
	  /* VTOP and CONT notes are valid only before the loop exit test.
	     If placed anywhere else, loop may generate bad code.  */
	  /* BASIC_BLOCK notes exist to stabilize basic block structures with
	     the associated rtl.  We do not want to share the structure in
	     this new block.  */

	  if (NOTE_LINE_NUMBER (insn) != NOTE_INSN_DELETED
		   && NOTE_LINE_NUMBER (insn) != NOTE_INSN_DELETED_LABEL
		   && NOTE_LINE_NUMBER (insn) != NOTE_INSN_BASIC_BLOCK
		   && ((NOTE_LINE_NUMBER (insn) != NOTE_INSN_LOOP_VTOP
			&& NOTE_LINE_NUMBER (insn) != NOTE_INSN_LOOP_CONT)
		       || (last_iteration
			   && unroll_type != UNROLL_COMPLETELY)))
	    copy = emit_note_copy (insn);
	  else
	    copy = 0;
	  break;

	default:
	  abort ();
	}

      map->insn_map[INSN_UID (insn)] = copy;
    }
  while (insn != copy_end);

  /* Now finish coping the REG_NOTES.  */
  insn = copy_start;
  do
    {
      insn = NEXT_INSN (insn);
      if ((GET_CODE (insn) == INSN || GET_CODE (insn) == JUMP_INSN
	   || GET_CODE (insn) == CALL_INSN)
	  && map->insn_map[INSN_UID (insn)])
	final_reg_note_copy (&REG_NOTES (map->insn_map[INSN_UID (insn)]), map);
    }
  while (insn != copy_end);

  /* There may be notes between copy_notes_from and loop_end.  Emit a copy of
     each of these notes here, since there may be some important ones, such as
     NOTE_INSN_BLOCK_END notes, in this group.  We don't do this on the last
     iteration, because the original notes won't be deleted.

     We can't use insert_before here, because when from preconditioning,
     insert_before points before the loop.  We can't use copy_end, because
     there may be insns already inserted after it (which we don't want to
     copy) when not from preconditioning code.  */

  if (! last_iteration)
    {
      for (insn = copy_notes_from; insn != loop_end; insn = NEXT_INSN (insn))
	{
	  /* VTOP notes are valid only before the loop exit test.
	     If placed anywhere else, loop may generate bad code.
	     Although COPY_NOTES_FROM will be at most one or two (for cc0)
	     instructions before the last insn in the loop, COPY_NOTES_FROM
	     can be a NOTE_INSN_LOOP_CONT note if there is no VTOP note,
	     as in a do .. while loop.  */
	  if (GET_CODE (insn) == NOTE
	      && ((NOTE_LINE_NUMBER (insn) != NOTE_INSN_DELETED
		   && NOTE_LINE_NUMBER (insn) != NOTE_INSN_BASIC_BLOCK
		   && NOTE_LINE_NUMBER (insn) != NOTE_INSN_LOOP_VTOP
		   && NOTE_LINE_NUMBER (insn) != NOTE_INSN_LOOP_CONT)))
	    emit_note_copy (insn);
	}
    }

  if (final_label && LABEL_NUSES (final_label) > 0)
    emit_label (final_label);

  tem = get_insns ();
  end_sequence ();
  loop_insn_emit_before (loop, 0, insert_before, tem);
}

/* Emit an insn, using the expand_binop to ensure that a valid insn is
   emitted.  This will correctly handle the case where the increment value
   won't fit in the immediate field of a PLUS insns.  */

void
emit_unrolled_add (rtx dest_reg, rtx src_reg, rtx increment)
{
  rtx result;

  result = expand_simple_binop (GET_MODE (dest_reg), PLUS, src_reg, increment,
				dest_reg, 0, OPTAB_LIB_WIDEN);

  if (dest_reg != result)
    emit_move_insn (dest_reg, result);
}

/* Searches the insns between INSN and LOOP->END.  Returns 1 if there
   is a backward branch in that range that branches to somewhere between
   LOOP->START and INSN.  Returns 0 otherwise.  */

/* ??? This is quadratic algorithm.  Could be rewritten to be linear.
   In practice, this is not a problem, because this function is seldom called,
   and uses a negligible amount of CPU time on average.  */

int
back_branch_in_range_p (const struct loop *loop, rtx insn)
{
  rtx p, q, target_insn;
  rtx loop_start = loop->start;
  rtx loop_end = loop->end;
  rtx orig_loop_end = loop->end;

  /* Stop before we get to the backward branch at the end of the loop.  */
  loop_end = prev_nonnote_insn (loop_end);
  if (GET_CODE (loop_end) == BARRIER)
    loop_end = PREV_INSN (loop_end);

  /* Check in case insn has been deleted, search forward for first non
     deleted insn following it.  */
  while (INSN_DELETED_P (insn))
    insn = NEXT_INSN (insn);

  /* Check for the case where insn is the last insn in the loop.  Deal
     with the case where INSN was a deleted loop test insn, in which case
     it will now be the NOTE_LOOP_END.  */
  if (insn == loop_end || insn == orig_loop_end)
    return 0;

  for (p = NEXT_INSN (insn); p != loop_end; p = NEXT_INSN (p))
    {
      if (GET_CODE (p) == JUMP_INSN)
	{
	  target_insn = JUMP_LABEL (p);

	  /* Search from loop_start to insn, to see if one of them is
	     the target_insn.  We can't use INSN_LUID comparisons here,
	     since insn may not have an LUID entry.  */
	  for (q = loop_start; q != insn; q = NEXT_INSN (q))
	    if (q == target_insn)
	      return 1;
	}
    }

  return 0;
}

/* Try to generate the simplest rtx for the expression
   (PLUS (MULT mult1 mult2) add1).  This is used to calculate the initial
   value of giv's.  */

static rtx
fold_rtx_mult_add (rtx mult1, rtx mult2, rtx add1, enum machine_mode mode)
{
  rtx temp, mult_res;
  rtx result;

  /* The modes must all be the same.  This should always be true.  For now,
     check to make sure.  */
  if ((GET_MODE (mult1) != mode && GET_MODE (mult1) != VOIDmode)
      || (GET_MODE (mult2) != mode && GET_MODE (mult2) != VOIDmode)
      || (GET_MODE (add1) != mode && GET_MODE (add1) != VOIDmode))
    abort ();

  /* Ensure that if at least one of mult1/mult2 are constant, then mult2
     will be a constant.  */
  if (GET_CODE (mult1) == CONST_INT)
    {
      temp = mult2;
      mult2 = mult1;
      mult1 = temp;
    }

  mult_res = simplify_binary_operation (MULT, mode, mult1, mult2);
  if (! mult_res)
    mult_res = gen_rtx_MULT (mode, mult1, mult2);

  /* Again, put the constant second.  */
  if (GET_CODE (add1) == CONST_INT)
    {
      temp = add1;
      add1 = mult_res;
      mult_res = temp;
    }

  result = simplify_binary_operation (PLUS, mode, add1, mult_res);
  if (! result)
    result = gen_rtx_PLUS (mode, add1, mult_res);

  return result;
}

/* Searches the list of induction struct's for the biv BL, to try to calculate
   the total increment value for one iteration of the loop as a constant.

   Returns the increment value as an rtx, simplified as much as possible,
   if it can be calculated.  Otherwise, returns 0.  */

rtx
biv_total_increment (const struct iv_class *bl)
{
  struct induction *v;
  rtx result;

  /* For increment, must check every instruction that sets it.  Each
     instruction must be executed only once each time through the loop.
     To verify this, we check that the insn is always executed, and that
     there are no backward branches after the insn that branch to before it.
     Also, the insn must have a mult_val of one (to make sure it really is
     an increment).  */

  result = const0_rtx;
  for (v = bl->biv; v; v = v->next_iv)
    {
      if (v->always_computable && v->mult_val == const1_rtx
	  && ! v->maybe_multiple
	  && SCALAR_INT_MODE_P (v->mode))
	{
	  /* If we have already counted it, skip it.  */
	  if (v->same)
	    continue;

	  result = fold_rtx_mult_add (result, const1_rtx, v->add_val, v->mode);
	}
      else
	return 0;
    }

  return result;
}

/* For each biv and giv, determine whether it can be safely split into
   a different variable for each unrolled copy of the loop body.  If it
   is safe to split, then indicate that by saving some useful info
   in the splittable_regs array.

   If the loop is being completely unrolled, then splittable_regs will hold
   the current value of the induction variable while the loop is unrolled.
   It must be set to the initial value of the induction variable here.
   Otherwise, splittable_regs will hold the difference between the current
   value of the induction variable and the value the induction variable had
   at the top of the loop.  It must be set to the value 0 here.

   Returns the total number of instructions that set registers that are
   splittable.  */

/* ?? If the loop is only unrolled twice, then most of the restrictions to
   constant values are unnecessary, since we can easily calculate increment
   values in this case even if nothing is constant.  The increment value
   should not involve a multiply however.  */

/* ?? Even if the biv/giv increment values aren't constant, it may still
   be beneficial to split the variable if the loop is only unrolled a few
   times, since multiplies by small integers (1,2,3,4) are very cheap.  */

static int
find_splittable_regs (const struct loop *loop,
		      enum unroll_types unroll_type, int unroll_number)
{
  struct loop_ivs *ivs = LOOP_IVS (loop);
  struct iv_class *bl;
  struct induction *v;
  rtx increment, tem;
  rtx biv_final_value;
  int biv_splittable;
  int result = 0;

  for (bl = ivs->list; bl; bl = bl->next)
    {
      /* Biv_total_increment must return a constant value,
	 otherwise we can not calculate the split values.  */

      increment = biv_total_increment (bl);
      if (! increment || GET_CODE (increment) != CONST_INT)
	continue;

      /* The loop must be unrolled completely, or else have a known number
	 of iterations and only one exit, or else the biv must be dead
	 outside the loop, or else the final value must be known.  Otherwise,
	 it is unsafe to split the biv since it may not have the proper
	 value on loop exit.  */

      /* loop_number_exit_count is nonzero if the loop has an exit other than
	 a fall through at the end.  */

      biv_splittable = 1;
      biv_final_value = 0;
      if (unroll_type != UNROLL_COMPLETELY
	  && (loop->exit_count || unroll_type == UNROLL_NAIVE)
	  && (REGNO_LAST_LUID (bl->regno) >= INSN_LUID (loop->end)
	      || ! bl->init_insn
	      || INSN_UID (bl->init_insn) >= max_uid_for_loop
	      || (REGNO_FIRST_LUID (bl->regno)
		  < INSN_LUID (bl->init_insn))
	      || reg_mentioned_p (bl->biv->dest_reg, SET_SRC (bl->init_set)))
	  && ! (biv_final_value = final_biv_value (loop, bl)))
	biv_splittable = 0;

      /* If any of the insns setting the BIV don't do so with a simple
	 PLUS, we don't know how to split it.  */
      for (v = bl->biv; biv_splittable && v; v = v->next_iv)
	if ((tem = single_set (v->insn)) == 0
	    || GET_CODE (SET_DEST (tem)) != REG
	    || REGNO (SET_DEST (tem)) != bl->regno
	    || GET_CODE (SET_SRC (tem)) != PLUS)
	  biv_splittable = 0;

      /* If final value is nonzero, then must emit an instruction which sets
	 the value of the biv to the proper value.  This is done after
	 handling all of the givs, since some of them may need to use the
	 biv's value in their initialization code.  */

      /* This biv is splittable.  If completely unrolling the loop, save
	 the biv's initial value.  Otherwise, save the constant zero.  */

      if (biv_splittable == 1)
	{
	  if (unroll_type == UNROLL_COMPLETELY)
	    {
	      /* If the initial value of the biv is itself (i.e. it is too
		 complicated for strength_reduce to compute), or is a hard
		 register, or it isn't invariant, then we must create a new
		 pseudo reg to hold the initial value of the biv.  */

	      if (GET_CODE (bl->initial_value) == REG
		  && (REGNO (bl->initial_value) == bl->regno
		      || REGNO (bl->initial_value) < FIRST_PSEUDO_REGISTER
		      || ! loop_invariant_p (loop, bl->initial_value)))
		{
		  rtx tem = gen_reg_rtx (bl->biv->mode);

		  record_base_value (REGNO (tem), bl->biv->add_val, 0);
		  loop_insn_hoist (loop,
				   gen_move_insn (tem, bl->biv->src_reg));

		  if (loop_dump_stream)
		    fprintf (loop_dump_stream,
			     "Biv %d initial value remapped to %d.\n",
			     bl->regno, REGNO (tem));

		  splittable_regs[bl->regno] = tem;
		}
	      else
		splittable_regs[bl->regno] = bl->initial_value;
	    }
	  else
	    splittable_regs[bl->regno] = const0_rtx;

	  /* Save the number of instructions that modify the biv, so that
	     we can treat the last one specially.  */

	  splittable_regs_updates[bl->regno] = bl->biv_count;
	  result += bl->biv_count;

	  if (loop_dump_stream)
	    fprintf (loop_dump_stream,
		     "Biv %d safe to split.\n", bl->regno);
	}

      /* Check every giv that depends on this biv to see whether it is
	 splittable also.  Even if the biv isn't splittable, givs which
	 depend on it may be splittable if the biv is live outside the
	 loop, and the givs aren't.  */

      result += find_splittable_givs (loop, bl, unroll_type, increment,
				      unroll_number);

      /* If final value is nonzero, then must emit an instruction which sets
	 the value of the biv to the proper value.  This is done after
	 handling all of the givs, since some of them may need to use the
	 biv's value in their initialization code.  */
      if (biv_final_value)
	{
	  /* If the loop has multiple exits, emit the insns before the
	     loop to ensure that it will always be executed no matter
	     how the loop exits.  Otherwise emit the insn after the loop,
	     since this is slightly more efficient.  */
	  if (! loop->exit_count)
	    loop_insn_sink (loop, gen_move_insn (bl->biv->src_reg,
						 biv_final_value));
	  else
	    {
	      /* Create a new register to hold the value of the biv, and then
		 set the biv to its final value before the loop start.  The biv
		 is set to its final value before loop start to ensure that
		 this insn will always be executed, no matter how the loop
		 exits.  */
	      rtx tem = gen_reg_rtx (bl->biv->mode);
	      record_base_value (REGNO (tem), bl->biv->add_val, 0);

	      loop_insn_hoist (loop, gen_move_insn (tem, bl->biv->src_reg));
	      loop_insn_hoist (loop, gen_move_insn (bl->biv->src_reg,
						    biv_final_value));

	      if (loop_dump_stream)
		fprintf (loop_dump_stream, "Biv %d mapped to %d for split.\n",
			 REGNO (bl->biv->src_reg), REGNO (tem));

	      /* Set up the mapping from the original biv register to the new
		 register.  */
	      bl->biv->src_reg = tem;
	    }
	}
    }
  return result;
}

/* For every giv based on the biv BL, check to determine whether it is
   splittable.  This is a subroutine to find_splittable_regs ().

   Return the number of instructions that set splittable registers.  */

static int
find_splittable_givs (const struct loop *loop, struct iv_class *bl,
		      enum unroll_types unroll_type, rtx increment,
		      int unroll_number ATTRIBUTE_UNUSED)
{
  struct loop_ivs *ivs = LOOP_IVS (loop);
  struct induction *v, *v2;
  rtx final_value;
  rtx tem;
  int result = 0;

  /* Scan the list of givs, and set the same_insn field when there are
     multiple identical givs in the same insn.  */
  for (v = bl->giv; v; v = v->next_iv)
    for (v2 = v->next_iv; v2; v2 = v2->next_iv)
      if (v->insn == v2->insn && rtx_equal_p (v->new_reg, v2->new_reg)
	  && ! v2->same_insn)
	v2->same_insn = v;

  for (v = bl->giv; v; v = v->next_iv)
    {
      rtx giv_inc, value;

      /* Only split the giv if it has already been reduced, or if the loop is
	 being completely unrolled.  */
      if (unroll_type != UNROLL_COMPLETELY && v->ignore)
	continue;

      /* The giv can be split if the insn that sets the giv is executed once
	 and only once on every iteration of the loop.  */
      /* An address giv can always be split.  v->insn is just a use not a set,
	 and hence it does not matter whether it is always executed.  All that
	 matters is that all the biv increments are always executed, and we
	 won't reach here if they aren't.  */
      if (v->giv_type != DEST_ADDR
	  && (! v->always_computable
	      || back_branch_in_range_p (loop, v->insn)))
	continue;

      /* The giv increment value must be a constant.  */
      giv_inc = fold_rtx_mult_add (v->mult_val, increment, const0_rtx,
				   v->mode);
      if (! giv_inc || GET_CODE (giv_inc) != CONST_INT)
	continue;

      /* The loop must be unrolled completely, or else have a known number of
	 iterations and only one exit, or else the giv must be dead outside
	 the loop, or else the final value of the giv must be known.
	 Otherwise, it is not safe to split the giv since it may not have the
	 proper value on loop exit.  */

      /* The used outside loop test will fail for DEST_ADDR givs.  They are
	 never used outside the loop anyways, so it is always safe to split a
	 DEST_ADDR giv.  */

      final_value = 0;
      if (unroll_type != UNROLL_COMPLETELY
	  && (loop->exit_count || unroll_type == UNROLL_NAIVE)
	  && v->giv_type != DEST_ADDR
	  /* The next part is true if the pseudo is used outside the loop.
	     We assume that this is true for any pseudo created after loop
	     starts, because we don't have a reg_n_info entry for them.  */
	  && (REGNO (v->dest_reg) >= max_reg_before_loop
	      || (REGNO_FIRST_UID (REGNO (v->dest_reg)) != INSN_UID (v->insn)
		  /* Check for the case where the pseudo is set by a shift/add
		     sequence, in which case the first insn setting the pseudo
		     is the first insn of the shift/add sequence.  */
		  && (! (tem = find_reg_note (v->insn, REG_RETVAL, NULL_RTX))
		      || (REGNO_FIRST_UID (REGNO (v->dest_reg))
			  != INSN_UID (XEXP (tem, 0)))))
	      /* Line above always fails if INSN was moved by loop opt.  */
	      || (REGNO_LAST_LUID (REGNO (v->dest_reg))
		  >= INSN_LUID (loop->end)))
	  && ! (final_value = v->final_value))
	continue;

#if 0
      /* Currently, non-reduced/final-value givs are never split.  */
      /* Should emit insns after the loop if possible, as the biv final value
	 code below does.  */

      /* If the final value is nonzero, and the giv has not been reduced,
	 then must emit an instruction to set the final value.  */
      if (final_value && !v->new_reg)
	{
	  /* Create a new register to hold the value of the giv, and then set
	     the giv to its final value before the loop start.  The giv is set
	     to its final value before loop start to ensure that this insn
	     will always be executed, no matter how we exit.  */
	  tem = gen_reg_rtx (v->mode);
	  loop_insn_hoist (loop, gen_move_insn (tem, v->dest_reg));
	  loop_insn_hoist (loop, gen_move_insn (v->dest_reg, final_value));

	  if (loop_dump_stream)
	    fprintf (loop_dump_stream, "Giv %d mapped to %d for split.\n",
		     REGNO (v->dest_reg), REGNO (tem));

	  v->src_reg = tem;
	}
#endif

      /* This giv is splittable.  If completely unrolling the loop, save the
	 giv's initial value.  Otherwise, save the constant zero for it.  */

      if (unroll_type == UNROLL_COMPLETELY)
	{
	  /* It is not safe to use bl->initial_value here, because it may not
	     be invariant.  It is safe to use the initial value stored in
	     the splittable_regs array if it is set.  In rare cases, it won't
	     be set, so then we do exactly the same thing as
	     find_splittable_regs does to get a safe value.  */
	  rtx biv_initial_value;

	  if (splittable_regs[bl->regno])
	    biv_initial_value = splittable_regs[bl->regno];
	  else if (GET_CODE (bl->initial_value) != REG
		   || (REGNO (bl->initial_value) != bl->regno
		       && REGNO (bl->initial_value) >= FIRST_PSEUDO_REGISTER))
	    biv_initial_value = bl->initial_value;
	  else
	    {
	      rtx tem = gen_reg_rtx (bl->biv->mode);

	      record_base_value (REGNO (tem), bl->biv->add_val, 0);
	      loop_insn_hoist (loop, gen_move_insn (tem, bl->biv->src_reg));
	      biv_initial_value = tem;
	    }
	  biv_initial_value = extend_value_for_giv (v, biv_initial_value);
	  value = fold_rtx_mult_add (v->mult_val, biv_initial_value,
				     v->add_val, v->mode);
	}
      else
	value = const0_rtx;

      if (v->new_reg)
	{
	  /* If a giv was combined with another giv, then we can only split
	     this giv if the giv it was combined with was reduced.  This
	     is because the value of v->new_reg is meaningless in this
	     case.  */
	  if (v->same && ! v->same->new_reg)
	    {
	      if (loop_dump_stream)
		fprintf (loop_dump_stream,
			 "giv combined with unreduced giv not split.\n");
	      continue;
	    }
	  /* If the giv is an address destination, it could be something other
	     than a simple register, these have to be treated differently.  */
	  else if (v->giv_type == DEST_REG)
	    {
	      /* If value is not a constant, register, or register plus
		 constant, then compute its value into a register before
		 loop start.  This prevents invalid rtx sharing, and should
		 generate better code.  We can use bl->initial_value here
		 instead of splittable_regs[bl->regno] because this code
		 is going before the loop start.  */
	      if (unroll_type == UNROLL_COMPLETELY
		  && GET_CODE (value) != CONST_INT
		  && GET_CODE (value) != REG
		  && (GET_CODE (value) != PLUS
		      || GET_CODE (XEXP (value, 0)) != REG
		      || GET_CODE (XEXP (value, 1)) != CONST_INT))
		{
		  rtx tem = gen_reg_rtx (v->mode);
		  record_base_value (REGNO (tem), v->add_val, 0);
		  loop_iv_add_mult_hoist (loop, 
				extend_value_for_giv (v, bl->initial_value), 
				v->mult_val, v->add_val, tem);
		  value = tem;
		}

	      splittable_regs[reg_or_subregno (v->new_reg)] = value;
	    }
	  else
	    continue;
	}
      else
	{
#if 0
	  /* Currently, unreduced giv's can't be split.  This is not too much
	     of a problem since unreduced giv's are not live across loop
	     iterations anyways.  When unrolling a loop completely though,
	     it makes sense to reduce&split givs when possible, as this will
	     result in simpler instructions, and will not require that a reg
	     be live across loop iterations.  */

	  splittable_regs[REGNO (v->dest_reg)] = value;
	  fprintf (stderr, "Giv %d at insn %d not reduced\n",
		   REGNO (v->dest_reg), INSN_UID (v->insn));
#else
	  continue;
#endif
	}

      /* Unreduced givs are only updated once by definition.  Reduced givs
	 are updated as many times as their biv is.  Mark it so if this is
	 a splittable register.  Don't need to do anything for address givs
	 where this may not be a register.  */

      if (GET_CODE (v->new_reg) == REG)
	{
	  int count = 1;
	  if (! v->ignore)
	    count = REG_IV_CLASS (ivs, REGNO (v->src_reg))->biv_count;

	  splittable_regs_updates[reg_or_subregno (v->new_reg)] = count;
	}

      result++;

      if (loop_dump_stream)
	{
	  int regnum;

	  if (GET_CODE (v->dest_reg) == CONST_INT)
	    regnum = -1;
	  else if (GET_CODE (v->dest_reg) != REG)
	    regnum = REGNO (XEXP (v->dest_reg, 0));
	  else
	    regnum = REGNO (v->dest_reg);
	  fprintf (loop_dump_stream, "Giv %d at insn %d safe to split.\n",
		   regnum, INSN_UID (v->insn));
	}
    }

  return result;
}

/* Try to prove that the register is dead after the loop exits.  Trace every
   loop exit looking for an insn that will always be executed, which sets
   the register to some value, and appears before the first use of the register
   is found.  If successful, then return 1, otherwise return 0.  */

/* ?? Could be made more intelligent in the handling of jumps, so that
   it can search past if statements and other similar structures.  */

static int
reg_dead_after_loop (const struct loop *loop, rtx reg)
{
  rtx insn, label;
  enum rtx_code code;
  int jump_count = 0;
  int label_count = 0;

  /* In addition to checking all exits of this loop, we must also check
     all exits of inner nested loops that would exit this loop.  We don't
     have any way to identify those, so we just give up if there are any
     such inner loop exits.  */

  for (label = loop->exit_labels; label; label = LABEL_NEXTREF (label))
    label_count++;

  if (label_count != loop->exit_count)
    return 0;

  /* HACK: Must also search the loop fall through exit, create a label_ref
     here which points to the loop->end, and append the loop_number_exit_labels
     list to it.  */
  label = gen_rtx_LABEL_REF (VOIDmode, loop->end);
  LABEL_NEXTREF (label) = loop->exit_labels;

  for (; label; label = LABEL_NEXTREF (label))
    {
      /* Succeed if find an insn which sets the biv or if reach end of
	 function.  Fail if find an insn that uses the biv, or if come to
	 a conditional jump.  */

      insn = NEXT_INSN (XEXP (label, 0));
      while (insn)
	{
	  code = GET_CODE (insn);
	  if (GET_RTX_CLASS (code) == 'i')
	    {
	      rtx set, note;

	      if (reg_referenced_p (reg, PATTERN (insn)))
		return 0;

	      note = find_reg_equal_equiv_note (insn);
	      if (note && reg_overlap_mentioned_p (reg, XEXP (note, 0)))
		return 0;

	      set = single_set (insn);
	      if (set && rtx_equal_p (SET_DEST (set), reg))
		break;
	    }

	  if (code == JUMP_INSN)
	    {
	      if (GET_CODE (PATTERN (insn)) == RETURN)
		break;
	      else if (!any_uncondjump_p (insn)
		       /* Prevent infinite loop following infinite loops.  */
		       || jump_count++ > 20)
		return 0;
	      else
		insn = JUMP_LABEL (insn);
	    }

	  insn = NEXT_INSN (insn);
	}
    }

  /* Success, the register is dead on all loop exits.  */
  return 1;
}

/* Try to calculate the final value of the biv, the value it will have at
   the end of the loop.  If we can do it, return that value.  */

rtx
final_biv_value (const struct loop *loop, struct iv_class *bl)
{
  unsigned HOST_WIDE_INT n_iterations = LOOP_INFO (loop)->n_iterations;
  rtx increment, tem;

  /* ??? This only works for MODE_INT biv's.  Reject all others for now.  */

  if (GET_MODE_CLASS (bl->biv->mode) != MODE_INT)
    return 0;

  /* The final value for reversed bivs must be calculated differently than
     for ordinary bivs.  In this case, there is already an insn after the
     loop which sets this biv's final value (if necessary), and there are
     no other loop exits, so we can return any value.  */
  if (bl->reversed)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Final biv value for %d, reversed biv.\n", bl->regno);

      return const0_rtx;
    }

  /* Try to calculate the final value as initial value + (number of iterations
     * increment).  For this to work, increment must be invariant, the only
     exit from the loop must be the fall through at the bottom (otherwise
     it may not have its final value when the loop exits), and the initial
     value of the biv must be invariant.  */

  if (n_iterations != 0
      && ! loop->exit_count
      && loop_invariant_p (loop, bl->initial_value))
    {
      increment = biv_total_increment (bl);

      if (increment && loop_invariant_p (loop, increment))
	{
	  /* Can calculate the loop exit value, emit insns after loop
	     end to calculate this value into a temporary register in
	     case it is needed later.  */

	  tem = gen_reg_rtx (bl->biv->mode);
	  record_base_value (REGNO (tem), bl->biv->add_val, 0);
	  loop_iv_add_mult_sink (loop, increment, GEN_INT (n_iterations),
				 bl->initial_value, tem);

	  if (loop_dump_stream)
	    fprintf (loop_dump_stream,
		     "Final biv value for %d, calculated.\n", bl->regno);

	  return tem;
	}
    }

  /* Check to see if the biv is dead at all loop exits.  */
  if (reg_dead_after_loop (loop, bl->biv->src_reg))
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Final biv value for %d, biv dead after loop exit.\n",
		 bl->regno);

      return const0_rtx;
    }

  return 0;
}

/* Try to calculate the final value of the giv, the value it will have at
   the end of the loop.  If we can do it, return that value.  */

rtx
final_giv_value (const struct loop *loop, struct induction *v)
{
  struct loop_ivs *ivs = LOOP_IVS (loop);
  struct iv_class *bl;
  rtx insn;
  rtx increment, tem;
  rtx seq;
  rtx loop_end = loop->end;
  unsigned HOST_WIDE_INT n_iterations = LOOP_INFO (loop)->n_iterations;

  bl = REG_IV_CLASS (ivs, REGNO (v->src_reg));

  /* The final value for givs which depend on reversed bivs must be calculated
     differently than for ordinary givs.  In this case, there is already an
     insn after the loop which sets this giv's final value (if necessary),
     and there are no other loop exits, so we can return any value.  */
  if (bl->reversed)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Final giv value for %d, depends on reversed biv\n",
		 REGNO (v->dest_reg));
      return const0_rtx;
    }

  /* Try to calculate the final value as a function of the biv it depends
     upon.  The only exit from the loop must be the fall through at the bottom
     and the insn that sets the giv must be executed on every iteration
     (otherwise the giv may not have its final value when the loop exits).  */

  /* ??? Can calculate the final giv value by subtracting off the
     extra biv increments times the giv's mult_val.  The loop must have
     only one exit for this to work, but the loop iterations does not need
     to be known.  */

  if (n_iterations != 0
      && ! loop->exit_count
      && v->always_executed)
    {
      /* ?? It is tempting to use the biv's value here since these insns will
	 be put after the loop, and hence the biv will have its final value
	 then.  However, this fails if the biv is subsequently eliminated.
	 Perhaps determine whether biv's are eliminable before trying to
	 determine whether giv's are replaceable so that we can use the
	 biv value here if it is not eliminable.  */

      /* We are emitting code after the end of the loop, so we must make
	 sure that bl->initial_value is still valid then.  It will still
	 be valid if it is invariant.  */

      increment = biv_total_increment (bl);

      if (increment && loop_invariant_p (loop, increment)
	  && loop_invariant_p (loop, bl->initial_value))
	{
	  /* Can calculate the loop exit value of its biv as
	     (n_iterations * increment) + initial_value */

	  /* The loop exit value of the giv is then
	     (final_biv_value - extra increments) * mult_val + add_val.
	     The extra increments are any increments to the biv which
	     occur in the loop after the giv's value is calculated.
	     We must search from the insn that sets the giv to the end
	     of the loop to calculate this value.  */

	  /* Put the final biv value in tem.  */
	  tem = gen_reg_rtx (v->mode);
	  record_base_value (REGNO (tem), bl->biv->add_val, 0);
	  loop_iv_add_mult_sink (loop, extend_value_for_giv (v, increment),
				 GEN_INT (n_iterations),
				 extend_value_for_giv (v, bl->initial_value),
				 tem);

	  /* Subtract off extra increments as we find them.  */
	  for (insn = NEXT_INSN (v->insn); insn != loop_end;
	       insn = NEXT_INSN (insn))
	    {
	      struct induction *biv;

	      for (biv = bl->biv; biv; biv = biv->next_iv)
		if (biv->insn == insn)
		  {
		    start_sequence ();
		    tem = expand_simple_binop (GET_MODE (tem), MINUS, tem,
					       biv->add_val, NULL_RTX, 0,
					       OPTAB_LIB_WIDEN);
		    seq = get_insns ();
		    end_sequence ();
		    loop_insn_sink (loop, seq);
		  }
	    }

	  /* Now calculate the giv's final value.  */
	  loop_iv_add_mult_sink (loop, tem, v->mult_val, v->add_val, tem);

	  if (loop_dump_stream)
	    fprintf (loop_dump_stream,
		     "Final giv value for %d, calc from biv's value.\n",
		     REGNO (v->dest_reg));

	  return tem;
	}
    }

  /* Replaceable giv's should never reach here.  */
  if (v->replaceable)
    abort ();

  /* Check to see if the biv is dead at all loop exits.  */
  if (reg_dead_after_loop (loop, v->dest_reg))
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Final giv value for %d, giv dead after loop exit.\n",
		 REGNO (v->dest_reg));

      return const0_rtx;
    }

  return 0;
}

/* Look back before LOOP->START for the insn that sets REG and return
   the equivalent constant if there is a REG_EQUAL note otherwise just
   the SET_SRC of REG.  */

static rtx
loop_find_equiv_value (const struct loop *loop, rtx reg)
{
  rtx loop_start = loop->start;
  rtx insn, set;
  rtx ret;

  ret = reg;
  for (insn = PREV_INSN (loop_start); insn; insn = PREV_INSN (insn))
    {
      if (GET_CODE (insn) == CODE_LABEL)
	break;

      else if (INSN_P (insn) && reg_set_p (reg, insn))
	{
	  /* We found the last insn before the loop that sets the register.
	     If it sets the entire register, and has a REG_EQUAL note,
	     then use the value of the REG_EQUAL note.  */
	  if ((set = single_set (insn))
	      && (SET_DEST (set) == reg))
	    {
	      rtx note = find_reg_note (insn, REG_EQUAL, NULL_RTX);

	      /* Only use the REG_EQUAL note if it is a constant.
		 Other things, divide in particular, will cause
		 problems later if we use them.  */
	      if (note && GET_CODE (XEXP (note, 0)) != EXPR_LIST
		  && CONSTANT_P (XEXP (note, 0)))
		ret = XEXP (note, 0);
	      else
		ret = SET_SRC (set);

	      /* We cannot do this if it changes between the
		 assignment and loop start though.  */
	      if (modified_between_p (ret, insn, loop_start))
		ret = reg;
	    }
	  break;
	}
    }
  return ret;
}

/* Return a simplified rtx for the expression OP - REG.

   REG must appear in OP, and OP must be a register or the sum of a register
   and a second term.

   Thus, the return value must be const0_rtx or the second term.

   The caller is responsible for verifying that REG appears in OP and OP has
   the proper form.  */

static rtx
subtract_reg_term (rtx op, rtx reg)
{
  if (op == reg)
    return const0_rtx;
  if (GET_CODE (op) == PLUS)
    {
      if (XEXP (op, 0) == reg)
	return XEXP (op, 1);
      else if (XEXP (op, 1) == reg)
	return XEXP (op, 0);
    }
  /* OP does not contain REG as a term.  */
  abort ();
}

/* Find and return register term common to both expressions OP0 and
   OP1 or NULL_RTX if no such term exists.  Each expression must be a
   REG or a PLUS of a REG.  */

static rtx
find_common_reg_term (rtx op0, rtx op1)
{
  if ((GET_CODE (op0) == REG || GET_CODE (op0) == PLUS)
      && (GET_CODE (op1) == REG || GET_CODE (op1) == PLUS))
    {
      rtx op00;
      rtx op01;
      rtx op10;
      rtx op11;

      if (GET_CODE (op0) == PLUS)
	op01 = XEXP (op0, 1), op00 = XEXP (op0, 0);
      else
	op01 = const0_rtx, op00 = op0;

      if (GET_CODE (op1) == PLUS)
	op11 = XEXP (op1, 1), op10 = XEXP (op1, 0);
      else
	op11 = const0_rtx, op10 = op1;

      /* Find and return common register term if present.  */
      if (REG_P (op00) && (op00 == op10 || op00 == op11))
	return op00;
      else if (REG_P (op01) && (op01 == op10 || op01 == op11))
	return op01;
    }

  /* No common register term found.  */
  return NULL_RTX;
}

/* Determine the loop iterator and calculate the number of loop
   iterations.  Returns the exact number of loop iterations if it can
   be calculated, otherwise returns zero.  */

unsigned HOST_WIDE_INT
loop_iterations (struct loop *loop)
{
  struct loop_info *loop_info = LOOP_INFO (loop);
  struct loop_ivs *ivs = LOOP_IVS (loop);
  rtx comparison, comparison_value;
  rtx iteration_var, initial_value, increment, final_value;
  enum rtx_code comparison_code;
  HOST_WIDE_INT inc;
  unsigned HOST_WIDE_INT abs_inc;
  unsigned HOST_WIDE_INT abs_diff;
  int off_by_one;
  int increment_dir;
  int unsigned_p, compare_dir, final_larger;
  rtx last_loop_insn;
  rtx reg_term;
  struct iv_class *bl;

  loop_info->n_iterations = 0;
  loop_info->initial_value = 0;
  loop_info->initial_equiv_value = 0;
  loop_info->comparison_value = 0;
  loop_info->final_value = 0;
  loop_info->final_equiv_value = 0;
  loop_info->increment = 0;
  loop_info->iteration_var = 0;
  loop_info->unroll_number = 1;
  loop_info->iv = 0;

  /* We used to use prev_nonnote_insn here, but that fails because it might
     accidentally get the branch for a contained loop if the branch for this
     loop was deleted.  We can only trust branches immediately before the
     loop_end.  */
  last_loop_insn = PREV_INSN (loop->end);

  /* ??? We should probably try harder to find the jump insn
     at the end of the loop.  The following code assumes that
     the last loop insn is a jump to the top of the loop.  */
  if (GET_CODE (last_loop_insn) != JUMP_INSN)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop iterations: No final conditional branch found.\n");
      return 0;
    }

  /* If there is a more than a single jump to the top of the loop
     we cannot (easily) determine the iteration count.  */
  if (LABEL_NUSES (JUMP_LABEL (last_loop_insn)) > 1)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop iterations: Loop has multiple back edges.\n");
      return 0;
    }

  /* If there are multiple conditionalized loop exit tests, they may jump
     back to differing CODE_LABELs.  */
  if (loop->top && loop->cont)
    {
      rtx temp = PREV_INSN (last_loop_insn);

      do
	{
	  if (GET_CODE (temp) == JUMP_INSN)
	    {
	      /* There are some kinds of jumps we can't deal with easily.  */
	      if (JUMP_LABEL (temp) == 0)
		{
		  if (loop_dump_stream)
		    fprintf
		      (loop_dump_stream,
		       "Loop iterations: Jump insn has null JUMP_LABEL.\n");
		  return 0;
		}

	      if (/* Previous unrolling may have generated new insns not
		     covered by the uid_luid array.  */
		  INSN_UID (JUMP_LABEL (temp)) < max_uid_for_loop
		  /* Check if we jump back into the loop body.  */
		  && INSN_LUID (JUMP_LABEL (temp)) > INSN_LUID (loop->top)
		  && INSN_LUID (JUMP_LABEL (temp)) < INSN_LUID (loop->cont))
		{
		  if (loop_dump_stream)
		    fprintf
		      (loop_dump_stream,
		       "Loop iterations: Loop has multiple back edges.\n");
		  return 0;
		}
	    }
	}
      while ((temp = PREV_INSN (temp)) != loop->cont);
    }

  /* Find the iteration variable.  If the last insn is a conditional
     branch, and the insn before tests a register value, make that the
     iteration variable.  */

  comparison = get_condition_for_loop (loop, last_loop_insn);
  if (comparison == 0)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop iterations: No final comparison found.\n");
      return 0;
    }

  /* ??? Get_condition may switch position of induction variable and
     invariant register when it canonicalizes the comparison.  */

  comparison_code = GET_CODE (comparison);
  iteration_var = XEXP (comparison, 0);
  comparison_value = XEXP (comparison, 1);

  if (GET_CODE (iteration_var) != REG)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop iterations: Comparison not against register.\n");
      return 0;
    }

  /* The only new registers that are created before loop iterations
     are givs made from biv increments or registers created by
     load_mems.  In the latter case, it is possible that try_copy_prop
     will propagate a new pseudo into the old iteration register but
     this will be marked by having the REG_USERVAR_P bit set.  */

  if ((unsigned) REGNO (iteration_var) >= ivs->n_regs
      && ! REG_USERVAR_P (iteration_var))
    abort ();

  /* Determine the initial value of the iteration variable, and the amount
     that it is incremented each loop.  Use the tables constructed by
     the strength reduction pass to calculate these values.  */

  /* Clear the result values, in case no answer can be found.  */
  initial_value = 0;
  increment = 0;

  /* The iteration variable can be either a giv or a biv.  Check to see
     which it is, and compute the variable's initial value, and increment
     value if possible.  */

  /* If this is a new register, can't handle it since we don't have any
     reg_iv_type entry for it.  */
  if ((unsigned) REGNO (iteration_var) >= ivs->n_regs)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop iterations: No reg_iv_type entry for iteration var.\n");
      return 0;
    }

  /* Reject iteration variables larger than the host wide int size, since they
     could result in a number of iterations greater than the range of our
     `unsigned HOST_WIDE_INT' variable loop_info->n_iterations.  */
  else if ((GET_MODE_BITSIZE (GET_MODE (iteration_var))
	    > HOST_BITS_PER_WIDE_INT))
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop iterations: Iteration var rejected because mode too large.\n");
      return 0;
    }
  else if (GET_MODE_CLASS (GET_MODE (iteration_var)) != MODE_INT)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop iterations: Iteration var not an integer.\n");
      return 0;
    }

  /* Try swapping the comparison to identify a suitable iv.  */
  if (REG_IV_TYPE (ivs, REGNO (iteration_var)) != BASIC_INDUCT
      && REG_IV_TYPE (ivs, REGNO (iteration_var)) != GENERAL_INDUCT
      && GET_CODE (comparison_value) == REG
      && REGNO (comparison_value) < ivs->n_regs)
    {
      rtx temp = comparison_value;
      comparison_code = swap_condition (comparison_code);
      comparison_value = iteration_var;
      iteration_var = temp;
    }

  if (REG_IV_TYPE (ivs, REGNO (iteration_var)) == BASIC_INDUCT)
    {
      if (REGNO (iteration_var) >= ivs->n_regs)
	abort ();

      /* Grab initial value, only useful if it is a constant.  */
      bl = REG_IV_CLASS (ivs, REGNO (iteration_var));
      initial_value = bl->initial_value;
      if (!bl->biv->always_executed || bl->biv->maybe_multiple)
	{
	  if (loop_dump_stream)
	    fprintf (loop_dump_stream,
		     "Loop iterations: Basic induction var not set once in each iteration.\n");
	  return 0;
	}

      increment = biv_total_increment (bl);
    }
  else if (REG_IV_TYPE (ivs, REGNO (iteration_var)) == GENERAL_INDUCT)
    {
      HOST_WIDE_INT offset = 0;
      struct induction *v = REG_IV_INFO (ivs, REGNO (iteration_var));
      rtx biv_initial_value;

      if (REGNO (v->src_reg) >= ivs->n_regs)
	abort ();

      if (!v->always_executed || v->maybe_multiple)
	{
	  if (loop_dump_stream)
	    fprintf (loop_dump_stream,
		     "Loop iterations: General induction var not set once in each iteration.\n");
	  return 0;
	}

      bl = REG_IV_CLASS (ivs, REGNO (v->src_reg));

      /* Increment value is mult_val times the increment value of the biv.  */

      increment = biv_total_increment (bl);
      if (increment)
	{
	  struct induction *biv_inc;

	  increment = fold_rtx_mult_add (v->mult_val,
					 extend_value_for_giv (v, increment),
					 const0_rtx, v->mode);
	  /* The caller assumes that one full increment has occurred at the
	     first loop test.  But that's not true when the biv is incremented
	     after the giv is set (which is the usual case), e.g.:
	     i = 6; do {;} while (i++ < 9) .
	     Therefore, we bias the initial value by subtracting the amount of
	     the increment that occurs between the giv set and the giv test.  */
	  for (biv_inc = bl->biv; biv_inc; biv_inc = biv_inc->next_iv)
	    {
	      if (loop_insn_first_p (v->insn, biv_inc->insn))
		{
		  if (REG_P (biv_inc->add_val))
		    {
		      if (loop_dump_stream)
			fprintf (loop_dump_stream,
				 "Loop iterations: Basic induction var add_val is REG %d.\n",
				 REGNO (biv_inc->add_val));
			return 0;
		    }

		  /* If we have already counted it, skip it.  */
		  if (biv_inc->same)
		    continue;

		  offset -= INTVAL (biv_inc->add_val);
		}
	    }
	}
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop iterations: Giv iterator, initial value bias %ld.\n",
		 (long) offset);

      /* Initial value is mult_val times the biv's initial value plus
	 add_val.  Only useful if it is a constant.  */
      biv_initial_value = extend_value_for_giv (v, bl->initial_value);
      initial_value
	= fold_rtx_mult_add (v->mult_val,
			     plus_constant (biv_initial_value, offset),
			     v->add_val, v->mode);
    }
  else
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop iterations: Not basic or general induction var.\n");
      return 0;
    }

  if (initial_value == 0)
    return 0;

  unsigned_p = 0;
  off_by_one = 0;
  switch (comparison_code)
    {
    case LEU:
      unsigned_p = 1;
    case LE:
      compare_dir = 1;
      off_by_one = 1;
      break;
    case GEU:
      unsigned_p = 1;
    case GE:
      compare_dir = -1;
      off_by_one = -1;
      break;
    case EQ:
      /* Cannot determine loop iterations with this case.  */
      compare_dir = 0;
      break;
    case LTU:
      unsigned_p = 1;
    case LT:
      compare_dir = 1;
      break;
    case GTU:
      unsigned_p = 1;
    case GT:
      compare_dir = -1;
      break;
    case NE:
      compare_dir = 0;
      break;
    default:
      abort ();
    }

  /* If the comparison value is an invariant register, then try to find
     its value from the insns before the start of the loop.  */

  final_value = comparison_value;
  if (GET_CODE (comparison_value) == REG
      && loop_invariant_p (loop, comparison_value))
    {
      final_value = loop_find_equiv_value (loop, comparison_value);

      /* If we don't get an invariant final value, we are better
	 off with the original register.  */
      if (! loop_invariant_p (loop, final_value))
	final_value = comparison_value;
    }

  /* Calculate the approximate final value of the induction variable
     (on the last successful iteration).  The exact final value
     depends on the branch operator, and increment sign.  It will be
     wrong if the iteration variable is not incremented by one each
     time through the loop and (comparison_value + off_by_one -
     initial_value) % increment != 0.
     ??? Note that the final_value may overflow and thus final_larger
     will be bogus.  A potentially infinite loop will be classified
     as immediate, e.g. for (i = 0x7ffffff0; i <= 0x7fffffff; i++)  */
  if (off_by_one)
    final_value = plus_constant (final_value, off_by_one);

  /* Save the calculated values describing this loop's bounds, in case
     precondition_loop_p will need them later.  These values can not be
     recalculated inside precondition_loop_p because strength reduction
     optimizations may obscure the loop's structure.

     These values are only required by precondition_loop_p and insert_bct
     whenever the number of iterations cannot be computed at compile time.
     Only the difference between final_value and initial_value is
     important.  Note that final_value is only approximate.  */
  loop_info->initial_value = initial_value;
  loop_info->comparison_value = comparison_value;
  loop_info->final_value = plus_constant (comparison_value, off_by_one);
  loop_info->increment = increment;
  loop_info->iteration_var = iteration_var;
  loop_info->comparison_code = comparison_code;
  loop_info->iv = bl;

  /* Try to determine the iteration count for loops such
     as (for i = init; i < init + const; i++).  When running the
     loop optimization twice, the first pass often converts simple
     loops into this form.  */

  if (REG_P (initial_value))
    {
      rtx reg1;
      rtx reg2;
      rtx const2;

      reg1 = initial_value;
      if (GET_CODE (final_value) == PLUS)
	reg2 = XEXP (final_value, 0), const2 = XEXP (final_value, 1);
      else
	reg2 = final_value, const2 = const0_rtx;

      /* Check for initial_value = reg1, final_value = reg2 + const2,
	 where reg1 != reg2.  */
      if (REG_P (reg2) && reg2 != reg1)
	{
	  rtx temp;

	  /* Find what reg1 is equivalent to.  Hopefully it will
	     either be reg2 or reg2 plus a constant.  */
	  temp = loop_find_equiv_value (loop, reg1);

	  if (find_common_reg_term (temp, reg2))
	    initial_value = temp;
	  else if (loop_invariant_p (loop, reg2))
	    {
	      /* Find what reg2 is equivalent to.  Hopefully it will
		 either be reg1 or reg1 plus a constant.  Let's ignore
		 the latter case for now since it is not so common.  */
	      temp = loop_find_equiv_value (loop, reg2);

	      if (temp == loop_info->iteration_var)
		temp = initial_value;
	      if (temp == reg1)
		final_value = (const2 == const0_rtx)
		  ? reg1 : gen_rtx_PLUS (GET_MODE (reg1), reg1, const2);
	    }
	}
      else if (loop->vtop && GET_CODE (reg2) == CONST_INT)
	{
	  rtx temp;

	  /* When running the loop optimizer twice, check_dbra_loop
	     further obfuscates reversible loops of the form:
	     for (i = init; i < init + const; i++).  We often end up with
	     final_value = 0, initial_value = temp, temp = temp2 - init,
	     where temp2 = init + const.  If the loop has a vtop we
	     can replace initial_value with const.  */

	  temp = loop_find_equiv_value (loop, reg1);

	  if (GET_CODE (temp) == MINUS && REG_P (XEXP (temp, 0)))
	    {
	      rtx temp2 = loop_find_equiv_value (loop, XEXP (temp, 0));

	      if (GET_CODE (temp2) == PLUS
		  && XEXP (temp2, 0) == XEXP (temp, 1))
		initial_value = XEXP (temp2, 1);
	    }
	}
    }

  /* If have initial_value = reg + const1 and final_value = reg +
     const2, then replace initial_value with const1 and final_value
     with const2.  This should be safe since we are protected by the
     initial comparison before entering the loop if we have a vtop.
     For example, a + b < a + c is not equivalent to b < c for all a
     when using modulo arithmetic.

     ??? Without a vtop we could still perform the optimization if we check
     the initial and final values carefully.  */
  if (loop->vtop
      && (reg_term = find_common_reg_term (initial_value, final_value))
      && loop_invariant_p (loop, reg_term))
    {
      initial_value = subtract_reg_term (initial_value, reg_term);
      final_value = subtract_reg_term (final_value, reg_term);
    }

  loop_info->initial_equiv_value = initial_value;
  loop_info->final_equiv_value = final_value;

  /* For EQ comparison loops, we don't have a valid final value.
     Check this now so that we won't leave an invalid value if we
     return early for any other reason.  */
  if (comparison_code == EQ)
    loop_info->final_equiv_value = loop_info->final_value = 0;

  if (increment == 0)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop iterations: Increment value can't be calculated.\n");
      return 0;
    }

  if (GET_CODE (increment) != CONST_INT)
    {
      /* If we have a REG, check to see if REG holds a constant value.  */
      /* ??? Other RTL, such as (neg (reg)) is possible here, but it isn't
	 clear if it is worthwhile to try to handle such RTL.  */
      if (GET_CODE (increment) == REG || GET_CODE (increment) == SUBREG)
	increment = loop_find_equiv_value (loop, increment);

      if (GET_CODE (increment) != CONST_INT)
	{
	  if (loop_dump_stream)
	    {
	      fprintf (loop_dump_stream,
		       "Loop iterations: Increment value not constant ");
	      print_simple_rtl (loop_dump_stream, increment);
	      fprintf (loop_dump_stream, ".\n");
	    }
	  return 0;
	}
      loop_info->increment = increment;
    }

  if (GET_CODE (initial_value) != CONST_INT)
    {
      if (loop_dump_stream)
	{
	  fprintf (loop_dump_stream,
		   "Loop iterations: Initial value not constant ");
	  print_simple_rtl (loop_dump_stream, initial_value);
	  fprintf (loop_dump_stream, ".\n");
	}
      return 0;
    }
  else if (GET_CODE (final_value) != CONST_INT)
    {
      if (loop_dump_stream)
	{
	  fprintf (loop_dump_stream,
		   "Loop iterations: Final value not constant ");
	  print_simple_rtl (loop_dump_stream, final_value);
	  fprintf (loop_dump_stream, ".\n");
	}
      return 0;
    }
  else if (comparison_code == EQ)
    {
      rtx inc_once;

      if (loop_dump_stream)
	fprintf (loop_dump_stream, "Loop iterations: EQ comparison loop.\n");

      inc_once = gen_int_mode (INTVAL (initial_value) + INTVAL (increment),
			       GET_MODE (iteration_var));

      if (inc_once == final_value)
	{
	  /* The iterator value once through the loop is equal to the
	     comparison value.  Either we have an infinite loop, or
	     we'll loop twice.  */
	  if (increment == const0_rtx)
	    return 0;
	  loop_info->n_iterations = 2;
	}
      else
	loop_info->n_iterations = 1;

      if (GET_CODE (loop_info->initial_value) == CONST_INT)
	loop_info->final_value
	  = gen_int_mode ((INTVAL (loop_info->initial_value)
			   + loop_info->n_iterations * INTVAL (increment)),
			  GET_MODE (iteration_var));
      else
	loop_info->final_value
	  = plus_constant (loop_info->initial_value,
			   loop_info->n_iterations * INTVAL (increment));
      loop_info->final_equiv_value
	= gen_int_mode ((INTVAL (initial_value)
			 + loop_info->n_iterations * INTVAL (increment)),
			GET_MODE (iteration_var));
      return loop_info->n_iterations;
    }

  /* Final_larger is 1 if final larger, 0 if they are equal, otherwise -1.  */
  if (unsigned_p)
    final_larger
      = ((unsigned HOST_WIDE_INT) INTVAL (final_value)
	 > (unsigned HOST_WIDE_INT) INTVAL (initial_value))
	- ((unsigned HOST_WIDE_INT) INTVAL (final_value)
	   < (unsigned HOST_WIDE_INT) INTVAL (initial_value));
  else
    final_larger = (INTVAL (final_value) > INTVAL (initial_value))
      - (INTVAL (final_value) < INTVAL (initial_value));

  if (INTVAL (increment) > 0)
    increment_dir = 1;
  else if (INTVAL (increment) == 0)
    increment_dir = 0;
  else
    increment_dir = -1;

  /* There are 27 different cases: compare_dir = -1, 0, 1;
     final_larger = -1, 0, 1; increment_dir = -1, 0, 1.
     There are 4 normal cases, 4 reverse cases (where the iteration variable
     will overflow before the loop exits), 4 infinite loop cases, and 15
     immediate exit (0 or 1 iteration depending on loop type) cases.
     Only try to optimize the normal cases.  */

  /* (compare_dir/final_larger/increment_dir)
     Normal cases: (0/-1/-1), (0/1/1), (-1/-1/-1), (1/1/1)
     Reverse cases: (0/-1/1), (0/1/-1), (-1/-1/1), (1/1/-1)
     Infinite loops: (0/-1/0), (0/1/0), (-1/-1/0), (1/1/0)
     Immediate exit: (0/0/X), (-1/0/X), (-1/1/X), (1/0/X), (1/-1/X) */

  /* ?? If the meaning of reverse loops (where the iteration variable
     will overflow before the loop exits) is undefined, then could
     eliminate all of these special checks, and just always assume
     the loops are normal/immediate/infinite.  Note that this means
     the sign of increment_dir does not have to be known.  Also,
     since it does not really hurt if immediate exit loops or infinite loops
     are optimized, then that case could be ignored also, and hence all
     loops can be optimized.

     According to ANSI Spec, the reverse loop case result is undefined,
     because the action on overflow is undefined.

     See also the special test for NE loops below.  */

  if (final_larger == increment_dir && final_larger != 0
      && (final_larger == compare_dir || compare_dir == 0))
    /* Normal case.  */
    ;
  else
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream, "Loop iterations: Not normal loop.\n");
      return 0;
    }

  /* Calculate the number of iterations, final_value is only an approximation,
     so correct for that.  Note that abs_diff and n_iterations are
     unsigned, because they can be as large as 2^n - 1.  */

  inc = INTVAL (increment);
  if (inc > 0)
    {
      abs_diff = INTVAL (final_value) - INTVAL (initial_value);
      abs_inc = inc;
    }
  else if (inc < 0)
    {
      abs_diff = INTVAL (initial_value) - INTVAL (final_value);
      abs_inc = -inc;
    }
  else
    abort ();

  /* Given that iteration_var is going to iterate over its own mode,
     not HOST_WIDE_INT, disregard higher bits that might have come
     into the picture due to sign extension of initial and final
     values.  */
  abs_diff &= ((unsigned HOST_WIDE_INT) 1
	       << (GET_MODE_BITSIZE (GET_MODE (iteration_var)) - 1)
	       << 1) - 1;

  /* For NE tests, make sure that the iteration variable won't miss
     the final value.  If abs_diff mod abs_incr is not zero, then the
     iteration variable will overflow before the loop exits, and we
     can not calculate the number of iterations.  */
  if (compare_dir == 0 && (abs_diff % abs_inc) != 0)
    return 0;

  /* Note that the number of iterations could be calculated using
     (abs_diff + abs_inc - 1) / abs_inc, provided care was taken to
     handle potential overflow of the summation.  */
  loop_info->n_iterations = abs_diff / abs_inc + ((abs_diff % abs_inc) != 0);
  return loop_info->n_iterations;
}

/* Replace uses of split bivs with their split pseudo register.  This is
   for original instructions which remain after loop unrolling without
   copying.  */

static rtx
remap_split_bivs (struct loop *loop, rtx x)
{
  struct loop_ivs *ivs = LOOP_IVS (loop);
  enum rtx_code code;
  int i;
  const char *fmt;

  if (x == 0)
    return x;

  code = GET_CODE (x);
  switch (code)
    {
    case SCRATCH:
    case PC:
    case CC0:
    case CONST_INT:
    case CONST_DOUBLE:
    case CONST:
    case SYMBOL_REF:
    case LABEL_REF:
      return x;

    case REG:
#if 0
      /* If non-reduced/final-value givs were split, then this would also
	 have to remap those givs also.  */
#endif
      if (REGNO (x) < ivs->n_regs
	  && REG_IV_TYPE (ivs, REGNO (x)) == BASIC_INDUCT)
	return REG_IV_CLASS (ivs, REGNO (x))->biv->src_reg;
      break;

    default:
      break;
    }

  fmt = GET_RTX_FORMAT (code);
  for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
    {
      if (fmt[i] == 'e')
	XEXP (x, i) = remap_split_bivs (loop, XEXP (x, i));
      else if (fmt[i] == 'E')
	{
	  int j;
	  for (j = 0; j < XVECLEN (x, i); j++)
	    XVECEXP (x, i, j) = remap_split_bivs (loop, XVECEXP (x, i, j));
	}
    }
  return x;
}

/* If FIRST_UID is a set of REGNO, and FIRST_UID dominates LAST_UID (e.g.
   FIST_UID is always executed if LAST_UID is), then return 1.  Otherwise
   return 0.  COPY_START is where we can start looking for the insns
   FIRST_UID and LAST_UID.  COPY_END is where we stop looking for these
   insns.

   If there is no JUMP_INSN between LOOP_START and FIRST_UID, then FIRST_UID
   must dominate LAST_UID.

   If there is a CODE_LABEL between FIRST_UID and LAST_UID, then FIRST_UID
   may not dominate LAST_UID.

   If there is no CODE_LABEL between FIRST_UID and LAST_UID, then FIRST_UID
   must dominate LAST_UID.  */

int
set_dominates_use (int regno, int first_uid, int last_uid, rtx copy_start,
		   rtx copy_end)
{
  int passed_jump = 0;
  rtx p = NEXT_INSN (copy_start);

  while (INSN_UID (p) != first_uid)
    {
      if (GET_CODE (p) == JUMP_INSN)
	passed_jump = 1;
      /* Could not find FIRST_UID.  */
      if (p == copy_end)
	return 0;
      p = NEXT_INSN (p);
    }

  /* Verify that FIRST_UID is an insn that entirely sets REGNO.  */
  if (! INSN_P (p) || ! dead_or_set_regno_p (p, regno))
    return 0;

  /* FIRST_UID is always executed.  */
  if (passed_jump == 0)
    return 1;

  while (INSN_UID (p) != last_uid)
    {
      /* If we see a CODE_LABEL between FIRST_UID and LAST_UID, then we
	 can not be sure that FIRST_UID dominates LAST_UID.  */
      if (GET_CODE (p) == CODE_LABEL)
	return 0;
      /* Could not find LAST_UID, but we reached the end of the loop, so
	 it must be safe.  */
      else if (p == copy_end)
	return 1;
      p = NEXT_INSN (p);
    }

  /* FIRST_UID is always executed if LAST_UID is executed.  */
  return 1;
}

/* This routine is called when the number of iterations for the unrolled
   loop is one.   The goal is to identify a loop that begins with an
   unconditional branch to the loop continuation note (or a label just after).
   In this case, the unconditional branch that starts the loop needs to be
   deleted so that we execute the single iteration.  */

static rtx
ujump_to_loop_cont (rtx loop_start, rtx loop_cont)
{
  rtx x, label, label_ref;

  /* See if loop start, or the next insn is an unconditional jump.  */
  loop_start = next_nonnote_insn (loop_start);

  x = pc_set (loop_start);
  if (!x)
    return NULL_RTX;

  label_ref = SET_SRC (x);
  if (!label_ref)
    return NULL_RTX;

  /* Examine insn after loop continuation note.  Return if not a label.  */
  label = next_nonnote_insn (loop_cont);
  if (label == 0 || GET_CODE (label) != CODE_LABEL)
    return NULL_RTX;

  /* Return the loop start if the branch label matches the code label.  */
  if (CODE_LABEL_NUMBER (label) == CODE_LABEL_NUMBER (XEXP (label_ref, 0)))
    return loop_start;
  else
    return NULL_RTX;
}
