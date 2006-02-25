/* Control flow optimization code for GNU compiler.
   Copyright (C) 1987, 1988, 1992, 1993, 1994, 1995, 1996, 1997, 1998,
   1999, 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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

/* This file contains optimizer of the control flow.  The main entrypoint is
   cleanup_cfg.  Following optimizations are performed:

   - Unreachable blocks removal
   - Edge forwarding (edge to the forwarder block is forwarded to it's
     successor.  Simplification of the branch instruction is performed by
     underlying infrastructure so branch can be converted to simplejump or
     eliminated).
   - Cross jumping (tail merging)
   - Conditional jump-around-simplejump simplification
   - Basic block merging.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "hard-reg-set.h"
#include "basic-block.h"
#include "timevar.h"
#include "output.h"
#include "insn-config.h"
#include "flags.h"
#include "recog.h"
#include "toplev.h"
#include "cselib.h"
#include "params.h"
#include "tm_p.h"
#include "target.h"
#include "expr.h"

/* cleanup_cfg maintains following flags for each basic block.  */

enum bb_flags
{
    /* Set if BB is the forwarder block to avoid too many
       forwarder_block_p calls.  */
    BB_FORWARDER_BLOCK = 1,
    BB_NONTHREADABLE_BLOCK = 2
};

#define BB_FLAGS(BB) (enum bb_flags) (BB)->aux
#define BB_SET_FLAG(BB, FLAG) \
  (BB)->aux = (void *) (long) ((enum bb_flags) (BB)->aux | (FLAG))
#define BB_CLEAR_FLAG(BB, FLAG) \
  (BB)->aux = (void *) (long) ((enum bb_flags) (BB)->aux & ~(FLAG))

#define FORWARDER_BLOCK_P(BB) (BB_FLAGS (BB) & BB_FORWARDER_BLOCK)

static bool try_crossjump_to_edge (int, edge, edge);
static bool try_crossjump_bb (int, basic_block);
static bool outgoing_edges_match (int, basic_block, basic_block);
static int flow_find_cross_jump (int, basic_block, basic_block, rtx *, rtx *);
static bool insns_match_p (int, rtx, rtx);

static bool tail_recursion_label_p (rtx);
static void merge_blocks_move_predecessor_nojumps (basic_block, basic_block);
static void merge_blocks_move_successor_nojumps (basic_block, basic_block);
static bool try_optimize_cfg (int);
static bool try_simplify_condjump (basic_block);
static bool try_forward_edges (int, basic_block);
static edge thread_jump (int, edge, basic_block);
static bool mark_effect (rtx, bitmap);
static void notice_new_block (basic_block);
static void update_forwarder_flag (basic_block);
static int mentions_nonequal_regs (rtx *, void *);
static void merge_memattrs (rtx, rtx);

/* Set flags for newly created block.  */

static void
notice_new_block (basic_block bb)
{
  if (!bb)
    return;

  if (forwarder_block_p (bb))
    BB_SET_FLAG (bb, BB_FORWARDER_BLOCK);
}

/* Recompute forwarder flag after block has been modified.  */

static void
update_forwarder_flag (basic_block bb)
{
  if (forwarder_block_p (bb))
    BB_SET_FLAG (bb, BB_FORWARDER_BLOCK);
  else
    BB_CLEAR_FLAG (bb, BB_FORWARDER_BLOCK);
}

/* Simplify a conditional jump around an unconditional jump.
   Return true if something changed.  */

static bool
try_simplify_condjump (basic_block cbranch_block)
{
  basic_block jump_block, jump_dest_block, cbranch_dest_block;
  edge cbranch_jump_edge, cbranch_fallthru_edge;
  rtx cbranch_insn;
  rtx insn, next;
  rtx end;

  /* Verify that there are exactly two successors.  */
  if (!cbranch_block->succ
      || !cbranch_block->succ->succ_next
      || cbranch_block->succ->succ_next->succ_next)
    return false;

  /* Verify that we've got a normal conditional branch at the end
     of the block.  */
  cbranch_insn = BB_END (cbranch_block);
  if (!any_condjump_p (cbranch_insn))
    return false;

  cbranch_fallthru_edge = FALLTHRU_EDGE (cbranch_block);
  cbranch_jump_edge = BRANCH_EDGE (cbranch_block);

  /* The next block must not have multiple predecessors, must not
     be the last block in the function, and must contain just the
     unconditional jump.  */
  jump_block = cbranch_fallthru_edge->dest;
  if (jump_block->pred->pred_next
      || jump_block->next_bb == EXIT_BLOCK_PTR
      || !FORWARDER_BLOCK_P (jump_block))
    return false;
  jump_dest_block = jump_block->succ->dest;

  /* The conditional branch must target the block after the
     unconditional branch.  */
  cbranch_dest_block = cbranch_jump_edge->dest;

  if (!can_fallthru (jump_block, cbranch_dest_block))
    return false;

  /* Invert the conditional branch.  */
  if (!invert_jump (cbranch_insn, block_label (jump_dest_block), 0))
    return false;

  if (rtl_dump_file)
    fprintf (rtl_dump_file, "Simplifying condjump %i around jump %i\n",
	     INSN_UID (cbranch_insn), INSN_UID (BB_END (jump_block)));

  /* Success.  Update the CFG to match.  Note that after this point
     the edge variable names appear backwards; the redirection is done
     this way to preserve edge profile data.  */
  cbranch_jump_edge = redirect_edge_succ_nodup (cbranch_jump_edge,
						cbranch_dest_block);
  cbranch_fallthru_edge = redirect_edge_succ_nodup (cbranch_fallthru_edge,
						    jump_dest_block);
  cbranch_jump_edge->flags |= EDGE_FALLTHRU;
  cbranch_fallthru_edge->flags &= ~EDGE_FALLTHRU;
  update_br_prob_note (cbranch_block);

  end = BB_END (jump_block);
  /* Deleting a block may produce unreachable code warning even when we are
     not deleting anything live.  Suppress it by moving all the line number
     notes out of the block.  */
  for (insn = BB_HEAD (jump_block); insn != NEXT_INSN (BB_END (jump_block));
       insn = next)
    {
      next = NEXT_INSN (insn);
      if (GET_CODE (insn) == NOTE && NOTE_LINE_NUMBER (insn) > 0)
	{
	  if (insn == BB_END (jump_block))
	    {
	      BB_END (jump_block) = PREV_INSN (insn);
	      if (insn == end)
	        break;
	    }
	  reorder_insns_nobb (insn, insn, end);
	  end = insn;
	}
    }
  /* Delete the block with the unconditional jump, and clean up the mess.  */
  delete_block (jump_block);
  tidy_fallthru_edge (cbranch_jump_edge, cbranch_block, cbranch_dest_block);

  return true;
}

/* Attempt to prove that operation is NOOP using CSElib or mark the effect
   on register.  Used by jump threading.  */

static bool
mark_effect (rtx exp, regset nonequal)
{
  int regno;
  rtx dest;
  switch (GET_CODE (exp))
    {
      /* In case we do clobber the register, mark it as equal, as we know the
         value is dead so it don't have to match.  */
    case CLOBBER:
      if (REG_P (XEXP (exp, 0)))
	{
	  dest = XEXP (exp, 0);
	  regno = REGNO (dest);
	  CLEAR_REGNO_REG_SET (nonequal, regno);
	  if (regno < FIRST_PSEUDO_REGISTER)
	    {
	      int n = HARD_REGNO_NREGS (regno, GET_MODE (dest));
	      while (--n > 0)
		CLEAR_REGNO_REG_SET (nonequal, regno + n);
	    }
	}
      return false;

    case SET:
      if (rtx_equal_for_cselib_p (SET_DEST (exp), SET_SRC (exp)))
	return false;
      dest = SET_DEST (exp);
      if (dest == pc_rtx)
	return false;
      if (!REG_P (dest))
	return true;
      regno = REGNO (dest);
      SET_REGNO_REG_SET (nonequal, regno);
      if (regno < FIRST_PSEUDO_REGISTER)
	{
	  int n = HARD_REGNO_NREGS (regno, GET_MODE (dest));
	  while (--n > 0)
	    SET_REGNO_REG_SET (nonequal, regno + n);
	}
      return false;

    default:
      return false;
    }
}

/* Return nonzero if X is a register set in regset DATA.
   Called via for_each_rtx.  */
static int
mentions_nonequal_regs (rtx *x, void *data)
{
  regset nonequal = (regset) data;
  if (REG_P (*x))
    {
      int regno;

      regno = REGNO (*x);
      if (REGNO_REG_SET_P (nonequal, regno))
	return 1;
      if (regno < FIRST_PSEUDO_REGISTER)
	{
	  int n = HARD_REGNO_NREGS (regno, GET_MODE (*x));
	  while (--n > 0)
	    if (REGNO_REG_SET_P (nonequal, regno + n))
	      return 1;
	}
    }
  return 0;
}
/* Attempt to prove that the basic block B will have no side effects and
   always continues in the same edge if reached via E.  Return the edge
   if exist, NULL otherwise.  */

static edge
thread_jump (int mode, edge e, basic_block b)
{
  rtx set1, set2, cond1, cond2, insn;
  enum rtx_code code1, code2, reversed_code2;
  bool reverse1 = false;
  int i;
  regset nonequal;
  bool failed = false;

  if (BB_FLAGS (b) & BB_NONTHREADABLE_BLOCK)
    return NULL;

  /* At the moment, we do handle only conditional jumps, but later we may
     want to extend this code to tablejumps and others.  */
  if (!e->src->succ->succ_next || e->src->succ->succ_next->succ_next)
    return NULL;
  if (!b->succ || !b->succ->succ_next || b->succ->succ_next->succ_next)
    {
      BB_SET_FLAG (b, BB_NONTHREADABLE_BLOCK);
      return NULL;
    }

  /* Second branch must end with onlyjump, as we will eliminate the jump.  */
  if (!any_condjump_p (BB_END (e->src)))
    return NULL;

  if (!any_condjump_p (BB_END (b)) || !onlyjump_p (BB_END (b)))
    {
      BB_SET_FLAG (b, BB_NONTHREADABLE_BLOCK);
      return NULL;
    }

  set1 = pc_set (BB_END (e->src));
  set2 = pc_set (BB_END (b));
  if (((e->flags & EDGE_FALLTHRU) != 0)
      != (XEXP (SET_SRC (set1), 1) == pc_rtx))
    reverse1 = true;

  cond1 = XEXP (SET_SRC (set1), 0);
  cond2 = XEXP (SET_SRC (set2), 0);
  if (reverse1)
    code1 = reversed_comparison_code (cond1, BB_END (e->src));
  else
    code1 = GET_CODE (cond1);

  code2 = GET_CODE (cond2);
  reversed_code2 = reversed_comparison_code (cond2, BB_END (b));

  if (!comparison_dominates_p (code1, code2)
      && !comparison_dominates_p (code1, reversed_code2))
    return NULL;

  /* Ensure that the comparison operators are equivalent.
     ??? This is far too pessimistic.  We should allow swapped operands,
     different CCmodes, or for example comparisons for interval, that
     dominate even when operands are not equivalent.  */
  if (!rtx_equal_p (XEXP (cond1, 0), XEXP (cond2, 0))
      || !rtx_equal_p (XEXP (cond1, 1), XEXP (cond2, 1)))
    return NULL;

  /* Short circuit cases where block B contains some side effects, as we can't
     safely bypass it.  */
  for (insn = NEXT_INSN (BB_HEAD (b)); insn != NEXT_INSN (BB_END (b));
       insn = NEXT_INSN (insn))
    if (INSN_P (insn) && side_effects_p (PATTERN (insn)))
      {
	BB_SET_FLAG (b, BB_NONTHREADABLE_BLOCK);
	return NULL;
      }

  cselib_init ();

  /* First process all values computed in the source basic block.  */
  for (insn = NEXT_INSN (BB_HEAD (e->src)); insn != NEXT_INSN (BB_END (e->src));
       insn = NEXT_INSN (insn))
    if (INSN_P (insn))
      cselib_process_insn (insn);

  nonequal = BITMAP_XMALLOC();
  CLEAR_REG_SET (nonequal);

  /* Now assume that we've continued by the edge E to B and continue
     processing as if it were same basic block.
     Our goal is to prove that whole block is an NOOP.  */

  for (insn = NEXT_INSN (BB_HEAD (b)); insn != NEXT_INSN (BB_END (b)) && !failed;
       insn = NEXT_INSN (insn))
    {
      if (INSN_P (insn))
	{
	  rtx pat = PATTERN (insn);

	  if (GET_CODE (pat) == PARALLEL)
	    {
	      for (i = 0; i < XVECLEN (pat, 0); i++)
		failed |= mark_effect (XVECEXP (pat, 0, i), nonequal);
	    }
	  else
	    failed |= mark_effect (pat, nonequal);
	}

      cselib_process_insn (insn);
    }

  /* Later we should clear nonequal of dead registers.  So far we don't
     have life information in cfg_cleanup.  */
  if (failed)
    {
      BB_SET_FLAG (b, BB_NONTHREADABLE_BLOCK);
      goto failed_exit;
    }

  /* cond2 must not mention any register that is not equal to the
     former block.  */
  if (for_each_rtx (&cond2, mentions_nonequal_regs, nonequal))
    goto failed_exit;

  /* In case liveness information is available, we need to prove equivalence
     only of the live values.  */
  if (mode & CLEANUP_UPDATE_LIFE)
    AND_REG_SET (nonequal, b->global_live_at_end);

  EXECUTE_IF_SET_IN_REG_SET (nonequal, 0, i, goto failed_exit;);

  BITMAP_XFREE (nonequal);
  cselib_finish ();
  if ((comparison_dominates_p (code1, code2) != 0)
      != (XEXP (SET_SRC (set2), 1) == pc_rtx))
    return BRANCH_EDGE (b);
  else
    return FALLTHRU_EDGE (b);

failed_exit:
  BITMAP_XFREE (nonequal);
  cselib_finish ();
  return NULL;
}

/* Attempt to forward edges leaving basic block B.
   Return true if successful.  */

static bool
try_forward_edges (int mode, basic_block b)
{
  bool changed = false;
  edge e, next, *threaded_edges = NULL;

  for (e = b->succ; e; e = next)
    {
      basic_block target, first;
      int counter;
      bool threaded = false;
      int nthreaded_edges = 0;

      next = e->succ_next;

      /* Skip complex edges because we don't know how to update them.

         Still handle fallthru edges, as we can succeed to forward fallthru
         edge to the same place as the branch edge of conditional branch
         and turn conditional branch to an unconditional branch.  */
      if (e->flags & EDGE_COMPLEX)
	continue;

      target = first = e->dest;
      counter = 0;

      while (counter < n_basic_blocks)
	{
	  basic_block new_target = NULL;
	  bool new_target_threaded = false;

	  if (FORWARDER_BLOCK_P (target)
	      && target->succ->dest != EXIT_BLOCK_PTR)
	    {
	      /* Bypass trivial infinite loops.  */
	      if (target == target->succ->dest)
		counter = n_basic_blocks;
	      new_target = target->succ->dest;
	    }

	  /* Allow to thread only over one edge at time to simplify updating
	     of probabilities.  */
	  else if (mode & CLEANUP_THREADING)
	    {
	      edge t = thread_jump (mode, e, target);
	      if (t)
		{
		  if (!threaded_edges)
		    threaded_edges = xmalloc (sizeof (*threaded_edges)
					      * n_basic_blocks);
		  else
		    {
		      int i;

		      /* Detect an infinite loop across blocks not
			 including the start block.  */
		      for (i = 0; i < nthreaded_edges; ++i)
			if (threaded_edges[i] == t)
			  break;
		      if (i < nthreaded_edges)
			{
			  counter = n_basic_blocks;
			  break;
			}
		    }

		  /* Detect an infinite loop across the start block.  */
		  if (t->dest == b)
		    break;

		  if (nthreaded_edges >= n_basic_blocks)
		    abort ();
		  threaded_edges[nthreaded_edges++] = t;

		  new_target = t->dest;
		  new_target_threaded = true;
		}
	    }

	  if (!new_target)
	    break;

	  /* Avoid killing of loop pre-headers, as it is the place loop
	     optimizer wants to hoist code to.

	     For fallthru forwarders, the LOOP_BEG note must appear between
	     the header of block and CODE_LABEL of the loop, for non forwarders
	     it must appear before the JUMP_INSN.  */
	  if ((mode & CLEANUP_PRE_LOOP) && optimize)
	    {
	      rtx insn = (target->succ->flags & EDGE_FALLTHRU
			  ? BB_HEAD (target) : prev_nonnote_insn (BB_END (target)));

	      if (GET_CODE (insn) != NOTE)
		insn = NEXT_INSN (insn);

	      for (; insn && GET_CODE (insn) != CODE_LABEL && !INSN_P (insn);
		   insn = NEXT_INSN (insn))
		if (GET_CODE (insn) == NOTE
		    && NOTE_LINE_NUMBER (insn) == NOTE_INSN_LOOP_BEG)
		  break;

	      if (GET_CODE (insn) == NOTE)
		break;

	      /* Do not clean up branches to just past the end of a loop
		 at this time; it can mess up the loop optimizer's
		 recognition of some patterns.  */

	      insn = PREV_INSN (BB_HEAD (target));
	      if (insn && GET_CODE (insn) == NOTE
		    && NOTE_LINE_NUMBER (insn) == NOTE_INSN_LOOP_END)
		break;
	    }

	  counter++;
	  target = new_target;
	  threaded |= new_target_threaded;
	}

      if (counter >= n_basic_blocks)
	{
	  if (rtl_dump_file)
	    fprintf (rtl_dump_file, "Infinite loop in BB %i.\n",
		     target->index);
	}
      else if (target == first)
	; /* We didn't do anything.  */
      else
	{
	  /* Save the values now, as the edge may get removed.  */
	  gcov_type edge_count = e->count;
	  int edge_probability = e->probability;
	  int edge_frequency;
	  int n = 0;

	  /* Don't force if target is exit block.  */
	  if (threaded && target != EXIT_BLOCK_PTR)
	    {
	      notice_new_block (redirect_edge_and_branch_force (e, target));
	      if (rtl_dump_file)
		fprintf (rtl_dump_file, "Conditionals threaded.\n");
	    }
	  else if (!redirect_edge_and_branch (e, target))
	    {
	      if (rtl_dump_file)
		fprintf (rtl_dump_file,
			 "Forwarding edge %i->%i to %i failed.\n",
			 b->index, e->dest->index, target->index);
	      continue;
	    }

	  /* We successfully forwarded the edge.  Now update profile
	     data: for each edge we traversed in the chain, remove
	     the original edge's execution count.  */
	  edge_frequency = ((edge_probability * b->frequency
			     + REG_BR_PROB_BASE / 2)
			    / REG_BR_PROB_BASE);

	  if (!FORWARDER_BLOCK_P (b) && forwarder_block_p (b))
	    BB_SET_FLAG (b, BB_FORWARDER_BLOCK);

	  do
	    {
	      edge t;

	      first->count -= edge_count;
	      if (first->count < 0)
		first->count = 0;
	      first->frequency -= edge_frequency;
	      if (first->frequency < 0)
		first->frequency = 0;
	      if (first->succ->succ_next)
		{
		  edge e;
		  int prob;
		  if (n >= nthreaded_edges)
		    abort ();
		  t = threaded_edges [n++];
		  if (t->src != first)
		    abort ();
		  if (first->frequency)
		    prob = edge_frequency * REG_BR_PROB_BASE / first->frequency;
		  else
		    prob = 0;
		  if (prob > t->probability)
		    prob = t->probability;
		  t->probability -= prob;
		  prob = REG_BR_PROB_BASE - prob;
		  if (prob <= 0)
		    {
		      first->succ->probability = REG_BR_PROB_BASE;
		      first->succ->succ_next->probability = 0;
		    }
		  else
		    for (e = first->succ; e; e = e->succ_next)
		      e->probability = ((e->probability * REG_BR_PROB_BASE)
					/ (double) prob);
		  update_br_prob_note (first);
		}
	      else
		{
		  /* It is possible that as the result of
		     threading we've removed edge as it is
		     threaded to the fallthru edge.  Avoid
		     getting out of sync.  */
		  if (n < nthreaded_edges
		      && first == threaded_edges [n]->src)
		    n++;
		  t = first->succ;
		}

	      t->count -= edge_count;
	      if (t->count < 0)
		t->count = 0;
	      first = t->dest;
	    }
	  while (first != target);

	  changed = true;
	}
    }

  if (threaded_edges)
    free (threaded_edges);
  return changed;
}

/* Return true if LABEL is used for tail recursion.  */

static bool
tail_recursion_label_p (rtx label)
{
  rtx x;

  for (x = tail_recursion_label_list; x; x = XEXP (x, 1))
    if (label == XEXP (x, 0))
      return true;

  return false;
}

/* Blocks A and B are to be merged into a single block.  A has no incoming
   fallthru edge, so it can be moved before B without adding or modifying
   any jumps (aside from the jump from A to B).  */

static void
merge_blocks_move_predecessor_nojumps (basic_block a, basic_block b)
{
  rtx barrier;

  barrier = next_nonnote_insn (BB_END (a));
  if (GET_CODE (barrier) != BARRIER)
    abort ();
  delete_insn (barrier);

  /* Move block and loop notes out of the chain so that we do not
     disturb their order.

     ??? A better solution would be to squeeze out all the non-nested notes
     and adjust the block trees appropriately.   Even better would be to have
     a tighter connection between block trees and rtl so that this is not
     necessary.  */
  if (squeeze_notes (&BB_HEAD (a), &BB_END (a)))
    abort ();

  /* Scramble the insn chain.  */
  if (BB_END (a) != PREV_INSN (BB_HEAD (b)))
    reorder_insns_nobb (BB_HEAD (a), BB_END (a), PREV_INSN (BB_HEAD (b)));
  a->flags |= BB_DIRTY;

  if (rtl_dump_file)
    fprintf (rtl_dump_file, "Moved block %d before %d and merged.\n",
	     a->index, b->index);

  /* Swap the records for the two blocks around.  */

  unlink_block (a);
  link_block (a, b->prev_bb);

  /* Now blocks A and B are contiguous.  Merge them.  */
  merge_blocks (a, b);
}

/* Blocks A and B are to be merged into a single block.  B has no outgoing
   fallthru edge, so it can be moved after A without adding or modifying
   any jumps (aside from the jump from A to B).  */

static void
merge_blocks_move_successor_nojumps (basic_block a, basic_block b)
{
  rtx barrier, real_b_end;
  rtx label, table;

  real_b_end = BB_END (b);

  /* If there is a jump table following block B temporarily add the jump table
     to block B so that it will also be moved to the correct location.  */
  if (tablejump_p (BB_END (b), &label, &table)
      && prev_active_insn (label) == BB_END (b))
    {
      BB_END (b) = table;
    }

  /* There had better have been a barrier there.  Delete it.  */
  barrier = NEXT_INSN (BB_END (b));
  if (barrier && GET_CODE (barrier) == BARRIER)
    delete_insn (barrier);

  /* Move block and loop notes out of the chain so that we do not
     disturb their order.

     ??? A better solution would be to squeeze out all the non-nested notes
     and adjust the block trees appropriately.   Even better would be to have
     a tighter connection between block trees and rtl so that this is not
     necessary.  */
  if (squeeze_notes (&BB_HEAD (b), &BB_END (b)))
    abort ();

  /* Scramble the insn chain.  */
  reorder_insns_nobb (BB_HEAD (b), BB_END (b), BB_END (a));

  /* Restore the real end of b.  */
  BB_END (b) = real_b_end;

  if (rtl_dump_file)
    fprintf (rtl_dump_file, "Moved block %d after %d and merged.\n",
	     b->index, a->index);

  /* Now blocks A and B are contiguous.  Merge them.  */
  merge_blocks (a, b);
}

/* Attempt to merge basic blocks that are potentially non-adjacent.
   Return NULL iff the attempt failed, otherwise return basic block
   where cleanup_cfg should continue.  Because the merging commonly
   moves basic block away or introduces another optimization
   possibility, return basic block just before B so cleanup_cfg don't
   need to iterate.

   It may be good idea to return basic block before C in the case
   C has been moved after B and originally appeared earlier in the
   insn sequence, but we have no information available about the
   relative ordering of these two.  Hopefully it is not too common.  */

static basic_block
merge_blocks_move (edge e, basic_block b, basic_block c, int mode)
{
  basic_block next;
  /* If C has a tail recursion label, do not merge.  There is no
     edge recorded from the call_placeholder back to this label, as
     that would make optimize_sibling_and_tail_recursive_calls more
     complex for no gain.  */
  if ((mode & CLEANUP_PRE_SIBCALL)
      && GET_CODE (BB_HEAD (c)) == CODE_LABEL
      && tail_recursion_label_p (BB_HEAD (c)))
    return NULL;

  /* If B has a fallthru edge to C, no need to move anything.  */
  if (e->flags & EDGE_FALLTHRU)
    {
      int b_index = b->index, c_index = c->index;
      merge_blocks (b, c);
      update_forwarder_flag (b);

      if (rtl_dump_file)
	fprintf (rtl_dump_file, "Merged %d and %d without moving.\n",
		 b_index, c_index);

      return b->prev_bb == ENTRY_BLOCK_PTR ? b : b->prev_bb;
    }

  /* Otherwise we will need to move code around.  Do that only if expensive
     transformations are allowed.  */
  else if (mode & CLEANUP_EXPENSIVE)
    {
      edge tmp_edge, b_fallthru_edge;
      bool c_has_outgoing_fallthru;
      bool b_has_incoming_fallthru;

      /* Avoid overactive code motion, as the forwarder blocks should be
         eliminated by edge redirection instead.  One exception might have
	 been if B is a forwarder block and C has no fallthru edge, but
	 that should be cleaned up by bb-reorder instead.  */
      if (FORWARDER_BLOCK_P (b) || FORWARDER_BLOCK_P (c))
	return NULL;

      /* We must make sure to not munge nesting of lexical blocks,
	 and loop notes.  This is done by squeezing out all the notes
	 and leaving them there to lie.  Not ideal, but functional.  */

      for (tmp_edge = c->succ; tmp_edge; tmp_edge = tmp_edge->succ_next)
	if (tmp_edge->flags & EDGE_FALLTHRU)
	  break;

      c_has_outgoing_fallthru = (tmp_edge != NULL);

      for (tmp_edge = b->pred; tmp_edge; tmp_edge = tmp_edge->pred_next)
	if (tmp_edge->flags & EDGE_FALLTHRU)
	  break;

      b_has_incoming_fallthru = (tmp_edge != NULL);
      b_fallthru_edge = tmp_edge;
      next = b->prev_bb;
      if (next == c)
	next = next->prev_bb;

      /* Otherwise, we're going to try to move C after B.  If C does
	 not have an outgoing fallthru, then it can be moved
	 immediately after B without introducing or modifying jumps.  */
      if (! c_has_outgoing_fallthru)
	{
	  merge_blocks_move_successor_nojumps (b, c);
          return next == ENTRY_BLOCK_PTR ? next->next_bb : next;
	}

      /* If B does not have an incoming fallthru, then it can be moved
	 immediately before C without introducing or modifying jumps.
	 C cannot be the first block, so we do not have to worry about
	 accessing a non-existent block.  */

      if (b_has_incoming_fallthru)
	{
	  basic_block bb;

	  if (b_fallthru_edge->src == ENTRY_BLOCK_PTR)
	    return NULL;
	  bb = force_nonfallthru (b_fallthru_edge);
	  if (bb)
	    notice_new_block (bb);
	}

      merge_blocks_move_predecessor_nojumps (b, c);
      return next == ENTRY_BLOCK_PTR ? next->next_bb : next;
    }

  return NULL;
}


/* Removes the memory attributes of MEM expression
   if they are not equal.  */

void
merge_memattrs (rtx x, rtx y)
{
  int i;
  int j;
  enum rtx_code code;
  const char *fmt;

  if (x == y)
    return;
  if (x == 0 || y == 0)
    return;

  code = GET_CODE (x);

  if (code != GET_CODE (y))
    return;

  if (GET_MODE (x) != GET_MODE (y))
    return;

  if (code == MEM && MEM_ATTRS (x) != MEM_ATTRS (y))
    {
      if (! MEM_ATTRS (x))
	MEM_ATTRS (y) = 0;
      else if (! MEM_ATTRS (y))
	MEM_ATTRS (x) = 0;
      else 
	{
	  if (MEM_ALIAS_SET (x) != MEM_ALIAS_SET (y))
	    {
	      set_mem_alias_set (x, 0);
	      set_mem_alias_set (y, 0);
	    }
	  
	  if (! mem_expr_equal_p (MEM_EXPR (x), MEM_EXPR (y)))
	    {
	      set_mem_expr (x, 0);
	      set_mem_expr (y, 0);
	      set_mem_offset (x, 0);
	      set_mem_offset (y, 0);
	    }
	  else if (MEM_OFFSET (x) != MEM_OFFSET (y))
	    {
	      set_mem_offset (x, 0);
	      set_mem_offset (y, 0);
	    }
	  
	  set_mem_size (x, MAX (MEM_SIZE (x), MEM_SIZE (y)));
	  set_mem_size (y, MEM_SIZE (x));

	  set_mem_align (x, MIN (MEM_ALIGN (x), MEM_ALIGN (y)));
	  set_mem_align (y, MEM_ALIGN (x));
	}
    }
  
  fmt = GET_RTX_FORMAT (code);
  for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
    {
      switch (fmt[i])
	{
	case 'E':
	  /* Two vectors must have the same length.  */
	  if (XVECLEN (x, i) != XVECLEN (y, i))
	    return;

	  for (j = 0; j < XVECLEN (x, i); j++)
	    merge_memattrs (XVECEXP (x, i, j), XVECEXP (y, i, j));

	  break;

	case 'e':
	  merge_memattrs (XEXP (x, i), XEXP (y, i));
	}
    }
  return;
}


/* Return true if I1 and I2 are equivalent and thus can be crossjumped.  */

static bool
insns_match_p (int mode ATTRIBUTE_UNUSED, rtx i1, rtx i2)
{
  rtx p1, p2;

  /* Verify that I1 and I2 are equivalent.  */
  if (GET_CODE (i1) != GET_CODE (i2))
    return false;

  p1 = PATTERN (i1);
  p2 = PATTERN (i2);

  if (GET_CODE (p1) != GET_CODE (p2))
    return false;

  /* If this is a CALL_INSN, compare register usage information.
     If we don't check this on stack register machines, the two
     CALL_INSNs might be merged leaving reg-stack.c with mismatching
     numbers of stack registers in the same basic block.
     If we don't check this on machines with delay slots, a delay slot may
     be filled that clobbers a parameter expected by the subroutine.

     ??? We take the simple route for now and assume that if they're
     equal, they were constructed identically.  */

  if (GET_CODE (i1) == CALL_INSN
      && (!rtx_equal_p (CALL_INSN_FUNCTION_USAGE (i1),
		        CALL_INSN_FUNCTION_USAGE (i2))
	  || SIBLING_CALL_P (i1) != SIBLING_CALL_P (i2)))
    return false;

#ifdef STACK_REGS
  /* If cross_jump_death_matters is not 0, the insn's mode
     indicates whether or not the insn contains any stack-like
     regs.  */

  if ((mode & CLEANUP_POST_REGSTACK) && stack_regs_mentioned (i1))
    {
      /* If register stack conversion has already been done, then
         death notes must also be compared before it is certain that
         the two instruction streams match.  */

      rtx note;
      HARD_REG_SET i1_regset, i2_regset;

      CLEAR_HARD_REG_SET (i1_regset);
      CLEAR_HARD_REG_SET (i2_regset);

      for (note = REG_NOTES (i1); note; note = XEXP (note, 1))
	if (REG_NOTE_KIND (note) == REG_DEAD && STACK_REG_P (XEXP (note, 0)))
	  SET_HARD_REG_BIT (i1_regset, REGNO (XEXP (note, 0)));

      for (note = REG_NOTES (i2); note; note = XEXP (note, 1))
	if (REG_NOTE_KIND (note) == REG_DEAD && STACK_REG_P (XEXP (note, 0)))
	  SET_HARD_REG_BIT (i2_regset, REGNO (XEXP (note, 0)));

      GO_IF_HARD_REG_EQUAL (i1_regset, i2_regset, done);

      return false;

    done:
      ;
    }
#endif

  if (reload_completed
      ? rtx_renumbered_equal_p (p1, p2) : rtx_equal_p (p1, p2))
    return true;

  /* Do not do EQUIV substitution after reload.  First, we're undoing the
     work of reload_cse.  Second, we may be undoing the work of the post-
     reload splitting pass.  */
  /* ??? Possibly add a new phase switch variable that can be used by
     targets to disallow the troublesome insns after splitting.  */
  if (!reload_completed)
    {
      /* The following code helps take care of G++ cleanups.  */
      rtx equiv1 = find_reg_equal_equiv_note (i1);
      rtx equiv2 = find_reg_equal_equiv_note (i2);

      if (equiv1 && equiv2
	  /* If the equivalences are not to a constant, they may
	     reference pseudos that no longer exist, so we can't
	     use them.  */
	  && (! reload_completed
	      || (CONSTANT_P (XEXP (equiv1, 0))
		  && rtx_equal_p (XEXP (equiv1, 0), XEXP (equiv2, 0)))))
	{
	  rtx s1 = single_set (i1);
	  rtx s2 = single_set (i2);
	  if (s1 != 0 && s2 != 0
	      && rtx_renumbered_equal_p (SET_DEST (s1), SET_DEST (s2)))
	    {
	      validate_change (i1, &SET_SRC (s1), XEXP (equiv1, 0), 1);
	      validate_change (i2, &SET_SRC (s2), XEXP (equiv2, 0), 1);
	      if (! rtx_renumbered_equal_p (p1, p2))
		cancel_changes (0);
	      else if (apply_change_group ())
		return true;
	    }
	}
    }

  return false;
}

/* Look through the insns at the end of BB1 and BB2 and find the longest
   sequence that are equivalent.  Store the first insns for that sequence
   in *F1 and *F2 and return the sequence length.

   To simplify callers of this function, if the blocks match exactly,
   store the head of the blocks in *F1 and *F2.  */

static int
flow_find_cross_jump (int mode ATTRIBUTE_UNUSED, basic_block bb1,
		      basic_block bb2, rtx *f1, rtx *f2)
{
  rtx i1, i2, last1, last2, afterlast1, afterlast2;
  int ninsns = 0;

  /* Skip simple jumps at the end of the blocks.  Complex jumps still
     need to be compared for equivalence, which we'll do below.  */

  i1 = BB_END (bb1);
  last1 = afterlast1 = last2 = afterlast2 = NULL_RTX;
  if (onlyjump_p (i1)
      || (returnjump_p (i1) && !side_effects_p (PATTERN (i1))))
    {
      last1 = i1;
      i1 = PREV_INSN (i1);
    }

  i2 = BB_END (bb2);
  if (onlyjump_p (i2)
      || (returnjump_p (i2) && !side_effects_p (PATTERN (i2))))
    {
      last2 = i2;
      /* Count everything except for unconditional jump as insn.  */
      if (!simplejump_p (i2) && !returnjump_p (i2) && last1)
	ninsns++;
      i2 = PREV_INSN (i2);
    }

  while (true)
    {
      /* Ignore notes.  */
      while (!INSN_P (i1) && i1 != BB_HEAD (bb1))
	i1 = PREV_INSN (i1);

      while (!INSN_P (i2) && i2 != BB_HEAD (bb2))
	i2 = PREV_INSN (i2);

      if (i1 == BB_HEAD (bb1) || i2 == BB_HEAD (bb2))
	break;

      if (!insns_match_p (mode, i1, i2))
	break;

      merge_memattrs (i1, i2);

      /* Don't begin a cross-jump with a NOTE insn.  */
      if (INSN_P (i1))
	{
	  /* If the merged insns have different REG_EQUAL notes, then
	     remove them.  */
	  rtx equiv1 = find_reg_equal_equiv_note (i1);
	  rtx equiv2 = find_reg_equal_equiv_note (i2);

	  if (equiv1 && !equiv2)
	    remove_note (i1, equiv1);
	  else if (!equiv1 && equiv2)
	    remove_note (i2, equiv2);
	  else if (equiv1 && equiv2
		   && !rtx_equal_p (XEXP (equiv1, 0), XEXP (equiv2, 0)))
	    {
	      remove_note (i1, equiv1);
	      remove_note (i2, equiv2);
	    }

	  afterlast1 = last1, afterlast2 = last2;
	  last1 = i1, last2 = i2;
	  ninsns++;
	}

      i1 = PREV_INSN (i1);
      i2 = PREV_INSN (i2);
    }

#ifdef HAVE_cc0
  /* Don't allow the insn after a compare to be shared by
     cross-jumping unless the compare is also shared.  */
  if (ninsns && reg_mentioned_p (cc0_rtx, last1) && ! sets_cc0_p (last1))
    last1 = afterlast1, last2 = afterlast2, ninsns--;
#endif

  /* Include preceding notes and labels in the cross-jump.  One,
     this may bring us to the head of the blocks as requested above.
     Two, it keeps line number notes as matched as may be.  */
  if (ninsns)
    {
      while (last1 != BB_HEAD (bb1) && !INSN_P (PREV_INSN (last1)))
	last1 = PREV_INSN (last1);

      if (last1 != BB_HEAD (bb1) && GET_CODE (PREV_INSN (last1)) == CODE_LABEL)
	last1 = PREV_INSN (last1);

      while (last2 != BB_HEAD (bb2) && !INSN_P (PREV_INSN (last2)))
	last2 = PREV_INSN (last2);

      if (last2 != BB_HEAD (bb2) && GET_CODE (PREV_INSN (last2)) == CODE_LABEL)
	last2 = PREV_INSN (last2);

      *f1 = last1;
      *f2 = last2;
    }

  return ninsns;
}

/* Return true iff outgoing edges of BB1 and BB2 match, together with
   the branch instruction.  This means that if we commonize the control
   flow before end of the basic block, the semantic remains unchanged.

   We may assume that there exists one edge with a common destination.  */

static bool
outgoing_edges_match (int mode, basic_block bb1, basic_block bb2)
{
  int nehedges1 = 0, nehedges2 = 0;
  edge fallthru1 = 0, fallthru2 = 0;
  edge e1, e2;

  /* If BB1 has only one successor, we may be looking at either an
     unconditional jump, or a fake edge to exit.  */
  if (bb1->succ && !bb1->succ->succ_next
      && (bb1->succ->flags & (EDGE_COMPLEX | EDGE_FAKE)) == 0
      && (GET_CODE (BB_END (bb1)) != JUMP_INSN || simplejump_p (BB_END (bb1))))
    return (bb2->succ &&  !bb2->succ->succ_next
	    && (bb2->succ->flags & (EDGE_COMPLEX | EDGE_FAKE)) == 0
	    && (GET_CODE (BB_END (bb2)) != JUMP_INSN || simplejump_p (BB_END (bb2))));

  /* Match conditional jumps - this may get tricky when fallthru and branch
     edges are crossed.  */
  if (bb1->succ
      && bb1->succ->succ_next
      && !bb1->succ->succ_next->succ_next
      && any_condjump_p (BB_END (bb1))
      && onlyjump_p (BB_END (bb1)))
    {
      edge b1, f1, b2, f2;
      bool reverse, match;
      rtx set1, set2, cond1, cond2;
      enum rtx_code code1, code2;

      if (!bb2->succ
	  || !bb2->succ->succ_next
	  || bb2->succ->succ_next->succ_next
	  || !any_condjump_p (BB_END (bb2))
	  || !onlyjump_p (BB_END (bb2)))
	return false;

      b1 = BRANCH_EDGE (bb1);
      b2 = BRANCH_EDGE (bb2);
      f1 = FALLTHRU_EDGE (bb1);
      f2 = FALLTHRU_EDGE (bb2);

      /* Get around possible forwarders on fallthru edges.  Other cases
         should be optimized out already.  */
      if (FORWARDER_BLOCK_P (f1->dest))
	f1 = f1->dest->succ;

      if (FORWARDER_BLOCK_P (f2->dest))
	f2 = f2->dest->succ;

      /* To simplify use of this function, return false if there are
	 unneeded forwarder blocks.  These will get eliminated later
	 during cleanup_cfg.  */
      if (FORWARDER_BLOCK_P (f1->dest)
	  || FORWARDER_BLOCK_P (f2->dest)
	  || FORWARDER_BLOCK_P (b1->dest)
	  || FORWARDER_BLOCK_P (b2->dest))
	return false;

      if (f1->dest == f2->dest && b1->dest == b2->dest)
	reverse = false;
      else if (f1->dest == b2->dest && b1->dest == f2->dest)
	reverse = true;
      else
	return false;

      set1 = pc_set (BB_END (bb1));
      set2 = pc_set (BB_END (bb2));
      if ((XEXP (SET_SRC (set1), 1) == pc_rtx)
	  != (XEXP (SET_SRC (set2), 1) == pc_rtx))
	reverse = !reverse;

      cond1 = XEXP (SET_SRC (set1), 0);
      cond2 = XEXP (SET_SRC (set2), 0);
      code1 = GET_CODE (cond1);
      if (reverse)
	code2 = reversed_comparison_code (cond2, BB_END (bb2));
      else
	code2 = GET_CODE (cond2);

      if (code2 == UNKNOWN)
	return false;

      /* Verify codes and operands match.  */
      match = ((code1 == code2
		&& rtx_renumbered_equal_p (XEXP (cond1, 0), XEXP (cond2, 0))
		&& rtx_renumbered_equal_p (XEXP (cond1, 1), XEXP (cond2, 1)))
	       || (code1 == swap_condition (code2)
		   && rtx_renumbered_equal_p (XEXP (cond1, 1),
					      XEXP (cond2, 0))
		   && rtx_renumbered_equal_p (XEXP (cond1, 0),
					      XEXP (cond2, 1))));

      /* If we return true, we will join the blocks.  Which means that
	 we will only have one branch prediction bit to work with.  Thus
	 we require the existing branches to have probabilities that are
	 roughly similar.  */
      if (match
	  && !optimize_size
	  && maybe_hot_bb_p (bb1)
	  && maybe_hot_bb_p (bb2))
	{
	  int prob2;

	  if (b1->dest == b2->dest)
	    prob2 = b2->probability;
	  else
	    /* Do not use f2 probability as f2 may be forwarded.  */
	    prob2 = REG_BR_PROB_BASE - b2->probability;

	  /* Fail if the difference in probabilities is greater than 50%.
	     This rules out two well-predicted branches with opposite
	     outcomes.  */
	  if (abs (b1->probability - prob2) > REG_BR_PROB_BASE / 2)
	    {
	      if (rtl_dump_file)
		fprintf (rtl_dump_file,
			 "Outcomes of branch in bb %i and %i differs to much (%i %i)\n",
			 bb1->index, bb2->index, b1->probability, prob2);

	      return false;
	    }
	}

      if (rtl_dump_file && match)
	fprintf (rtl_dump_file, "Conditionals in bb %i and %i match.\n",
		 bb1->index, bb2->index);

      return match;
    }

  /* Generic case - we are seeing a computed jump, table jump or trapping
     instruction.  */

#ifndef CASE_DROPS_THROUGH
  /* Check whether there are tablejumps in the end of BB1 and BB2.
     Return true if they are identical.  */
    {
      rtx label1, label2;
      rtx table1, table2;

      if (tablejump_p (BB_END (bb1), &label1, &table1)
	  && tablejump_p (BB_END (bb2), &label2, &table2)
	  && GET_CODE (PATTERN (table1)) == GET_CODE (PATTERN (table2)))
	{
	  /* The labels should never be the same rtx.  If they really are same
	     the jump tables are same too. So disable crossjumping of blocks BB1
	     and BB2 because when deleting the common insns in the end of BB1
	     by delete_block () the jump table would be deleted too.  */
	  /* If LABEL2 is referenced in BB1->END do not do anything
	     because we would loose information when replacing
	     LABEL1 by LABEL2 and then LABEL2 by LABEL1 in BB1->END.  */
	  if (label1 != label2 && !rtx_referenced_p (label2, BB_END (bb1)))
	    {
	      /* Set IDENTICAL to true when the tables are identical.  */
	      bool identical = false;
	      rtx p1, p2;

	      p1 = PATTERN (table1);
	      p2 = PATTERN (table2);
	      if (GET_CODE (p1) == ADDR_VEC && rtx_equal_p (p1, p2))
		{
		  identical = true;
		}
	      else if (GET_CODE (p1) == ADDR_DIFF_VEC
		       && (XVECLEN (p1, 1) == XVECLEN (p2, 1))
		       && rtx_equal_p (XEXP (p1, 2), XEXP (p2, 2))
		       && rtx_equal_p (XEXP (p1, 3), XEXP (p2, 3)))
		{
		  int i;

		  identical = true;
		  for (i = XVECLEN (p1, 1) - 1; i >= 0 && identical; i--)
		    if (!rtx_equal_p (XVECEXP (p1, 1, i), XVECEXP (p2, 1, i)))
		      identical = false;
		}

	      if (identical)
		{
		  replace_label_data rr;
		  bool match;

		  /* Temporarily replace references to LABEL1 with LABEL2
		     in BB1->END so that we could compare the instructions.  */
		  rr.r1 = label1;
		  rr.r2 = label2;
		  rr.update_label_nuses = false;
		  for_each_rtx (&BB_END (bb1), replace_label, &rr);

		  match = insns_match_p (mode, BB_END (bb1), BB_END (bb2));
		  if (rtl_dump_file && match)
		    fprintf (rtl_dump_file,
			     "Tablejumps in bb %i and %i match.\n",
			     bb1->index, bb2->index);

		  /* Set the original label in BB1->END because when deleting
		     a block whose end is a tablejump, the tablejump referenced
		     from the instruction is deleted too.  */
		  rr.r1 = label2;
		  rr.r2 = label1;
		  for_each_rtx (&BB_END (bb1), replace_label, &rr);

		  return match;
		}
	    }
	  return false;
	}
    }
#endif

  /* First ensure that the instructions match.  There may be many outgoing
     edges so this test is generally cheaper.  */
  if (!insns_match_p (mode, BB_END (bb1), BB_END (bb2)))
    return false;

  /* Search the outgoing edges, ensure that the counts do match, find possible
     fallthru and exception handling edges since these needs more
     validation.  */
  for (e1 = bb1->succ, e2 = bb2->succ; e1 && e2;
       e1 = e1->succ_next, e2 = e2->succ_next)
    {
      if (e1->flags & EDGE_EH)
	nehedges1++;

      if (e2->flags & EDGE_EH)
	nehedges2++;

      if (e1->flags & EDGE_FALLTHRU)
	fallthru1 = e1;
      if (e2->flags & EDGE_FALLTHRU)
	fallthru2 = e2;
    }

  /* If number of edges of various types does not match, fail.  */
  if (e1 || e2
      || nehedges1 != nehedges2
      || (fallthru1 != 0) != (fallthru2 != 0))
    return false;

  /* fallthru edges must be forwarded to the same destination.  */
  if (fallthru1)
    {
      basic_block d1 = (forwarder_block_p (fallthru1->dest)
			? fallthru1->dest->succ->dest: fallthru1->dest);
      basic_block d2 = (forwarder_block_p (fallthru2->dest)
			? fallthru2->dest->succ->dest: fallthru2->dest);

      if (d1 != d2)
	return false;
    }

  /* Ensure the same EH region.  */
  {
    rtx n1 = find_reg_note (BB_END (bb1), REG_EH_REGION, 0);
    rtx n2 = find_reg_note (BB_END (bb2), REG_EH_REGION, 0);

    if (!n1 && n2)
      return false;

    if (n1 && (!n2 || XEXP (n1, 0) != XEXP (n2, 0)))
      return false;
  }

  /* We don't need to match the rest of edges as above checks should be enough
     to ensure that they are equivalent.  */
  return true;
}

/* E1 and E2 are edges with the same destination block.  Search their
   predecessors for common code.  If found, redirect control flow from
   (maybe the middle of) E1->SRC to (maybe the middle of) E2->SRC.  */

static bool
try_crossjump_to_edge (int mode, edge e1, edge e2)
{
  int nmatch;
  basic_block src1 = e1->src, src2 = e2->src;
  basic_block redirect_to, redirect_from, to_remove;
  rtx newpos1, newpos2;
  edge s;

  /* Search backward through forwarder blocks.  We don't need to worry
     about multiple entry or chained forwarders, as they will be optimized
     away.  We do this to look past the unconditional jump following a
     conditional jump that is required due to the current CFG shape.  */
  if (src1->pred
      && !src1->pred->pred_next
      && FORWARDER_BLOCK_P (src1))
    e1 = src1->pred, src1 = e1->src;

  if (src2->pred
      && !src2->pred->pred_next
      && FORWARDER_BLOCK_P (src2))
    e2 = src2->pred, src2 = e2->src;

  /* Nothing to do if we reach ENTRY, or a common source block.  */
  if (src1 == ENTRY_BLOCK_PTR || src2 == ENTRY_BLOCK_PTR)
    return false;
  if (src1 == src2)
    return false;

  /* Seeing more than 1 forwarder blocks would confuse us later...  */
  if (FORWARDER_BLOCK_P (e1->dest)
      && FORWARDER_BLOCK_P (e1->dest->succ->dest))
    return false;

  if (FORWARDER_BLOCK_P (e2->dest)
      && FORWARDER_BLOCK_P (e2->dest->succ->dest))
    return false;

  /* Likewise with dead code (possibly newly created by the other optimizations
     of cfg_cleanup).  */
  if (!src1->pred || !src2->pred)
    return false;

  /* Look for the common insn sequence, part the first ...  */
  if (!outgoing_edges_match (mode, src1, src2))
    return false;

  /* ... and part the second.  */
  nmatch = flow_find_cross_jump (mode, src1, src2, &newpos1, &newpos2);
  if (!nmatch)
    return false;

#ifndef CASE_DROPS_THROUGH
  /* Here we know that the insns in the end of SRC1 which are common with SRC2
     will be deleted.
     If we have tablejumps in the end of SRC1 and SRC2
     they have been already compared for equivalence in outgoing_edges_match ()
     so replace the references to TABLE1 by references to TABLE2.  */
    {
      rtx label1, label2;
      rtx table1, table2;

      if (tablejump_p (BB_END (src1), &label1, &table1)
	  && tablejump_p (BB_END (src2), &label2, &table2)
	  && label1 != label2)
	{
	  replace_label_data rr;
	  rtx insn;

	  /* Replace references to LABEL1 with LABEL2.  */
	  rr.r1 = label1;
	  rr.r2 = label2;
	  rr.update_label_nuses = true;
	  for (insn = get_insns (); insn; insn = NEXT_INSN (insn))
	    {
	      /* Do not replace the label in SRC1->END because when deleting
		 a block whose end is a tablejump, the tablejump referenced
		 from the instruction is deleted too.  */
	      if (insn != BB_END (src1))
		for_each_rtx (&insn, replace_label, &rr);
	    }
	}
    }
#endif

  /* Avoid splitting if possible.  */
  if (newpos2 == BB_HEAD (src2))
    redirect_to = src2;
  else
    {
      if (rtl_dump_file)
	fprintf (rtl_dump_file, "Splitting bb %i before %i insns\n",
		 src2->index, nmatch);
      redirect_to = split_block (src2, PREV_INSN (newpos2))->dest;
    }

  if (rtl_dump_file)
    fprintf (rtl_dump_file,
	     "Cross jumping from bb %i to bb %i; %i common insns\n",
	     src1->index, src2->index, nmatch);

  redirect_to->count += src1->count;
  redirect_to->frequency += src1->frequency;
  /* We may have some registers visible trought the block.  */
  redirect_to->flags |= BB_DIRTY;

  /* Recompute the frequencies and counts of outgoing edges.  */
  for (s = redirect_to->succ; s; s = s->succ_next)
    {
      edge s2;
      basic_block d = s->dest;

      if (FORWARDER_BLOCK_P (d))
	d = d->succ->dest;

      for (s2 = src1->succ; ; s2 = s2->succ_next)
	{
	  basic_block d2 = s2->dest;
	  if (FORWARDER_BLOCK_P (d2))
	    d2 = d2->succ->dest;
	  if (d == d2)
	    break;
	}

      s->count += s2->count;

      /* Take care to update possible forwarder blocks.  We verified
         that there is no more than one in the chain, so we can't run
         into infinite loop.  */
      if (FORWARDER_BLOCK_P (s->dest))
	{
	  s->dest->succ->count += s2->count;
	  s->dest->count += s2->count;
	  s->dest->frequency += EDGE_FREQUENCY (s);
	}

      if (FORWARDER_BLOCK_P (s2->dest))
	{
	  s2->dest->succ->count -= s2->count;
	  if (s2->dest->succ->count < 0)
	    s2->dest->succ->count = 0;
	  s2->dest->count -= s2->count;
	  s2->dest->frequency -= EDGE_FREQUENCY (s);
	  if (s2->dest->frequency < 0)
	    s2->dest->frequency = 0;
	  if (s2->dest->count < 0)
	    s2->dest->count = 0;
	}

      if (!redirect_to->frequency && !src1->frequency)
	s->probability = (s->probability + s2->probability) / 2;
      else
	s->probability
	  = ((s->probability * redirect_to->frequency +
	      s2->probability * src1->frequency)
	     / (redirect_to->frequency + src1->frequency));
    }

  update_br_prob_note (redirect_to);

  /* Edit SRC1 to go to REDIRECT_TO at NEWPOS1.  */

  /* Skip possible basic block header.  */
  if (GET_CODE (newpos1) == CODE_LABEL)
    newpos1 = NEXT_INSN (newpos1);

  if (GET_CODE (newpos1) == NOTE)
    newpos1 = NEXT_INSN (newpos1);

  redirect_from = split_block (src1, PREV_INSN (newpos1))->src;
  to_remove = redirect_from->succ->dest;

  redirect_edge_and_branch_force (redirect_from->succ, redirect_to);
  delete_block (to_remove);

  update_forwarder_flag (redirect_from);

  return true;
}

/* Search the predecessors of BB for common insn sequences.  When found,
   share code between them by redirecting control flow.  Return true if
   any changes made.  */

static bool
try_crossjump_bb (int mode, basic_block bb)
{
  edge e, e2, nexte2, nexte, fallthru;
  bool changed;
  int n = 0, max;

  /* Nothing to do if there is not at least two incoming edges.  */
  if (!bb->pred || !bb->pred->pred_next)
    return false;

  /* It is always cheapest to redirect a block that ends in a branch to
     a block that falls through into BB, as that adds no branches to the
     program.  We'll try that combination first.  */
  fallthru = NULL;
  max = PARAM_VALUE (PARAM_MAX_CROSSJUMP_EDGES);
  for (e = bb->pred; e ; e = e->pred_next, n++)
    {
      if (e->flags & EDGE_FALLTHRU)
	fallthru = e;
      if (n > max)
	return false;
    }

  changed = false;
  for (e = bb->pred; e; e = nexte)
    {
      nexte = e->pred_next;

      /* As noted above, first try with the fallthru predecessor.  */
      if (fallthru)
	{
	  /* Don't combine the fallthru edge into anything else.
	     If there is a match, we'll do it the other way around.  */
	  if (e == fallthru)
	    continue;

	  if (try_crossjump_to_edge (mode, e, fallthru))
	    {
	      changed = true;
	      nexte = bb->pred;
	      continue;
	    }
	}

      /* Non-obvious work limiting check: Recognize that we're going
	 to call try_crossjump_bb on every basic block.  So if we have
	 two blocks with lots of outgoing edges (a switch) and they
	 share lots of common destinations, then we would do the
	 cross-jump check once for each common destination.

	 Now, if the blocks actually are cross-jump candidates, then
	 all of their destinations will be shared.  Which means that
	 we only need check them for cross-jump candidacy once.  We
	 can eliminate redundant checks of crossjump(A,B) by arbitrarily
	 choosing to do the check from the block for which the edge
	 in question is the first successor of A.  */
      if (e->src->succ != e)
	continue;

      for (e2 = bb->pred; e2; e2 = nexte2)
	{
	  nexte2 = e2->pred_next;

	  if (e2 == e)
	    continue;

	  /* We've already checked the fallthru edge above.  */
	  if (e2 == fallthru)
	    continue;

	  /* The "first successor" check above only prevents multiple
	     checks of crossjump(A,B).  In order to prevent redundant
	     checks of crossjump(B,A), require that A be the block
	     with the lowest index.  */
	  if (e->src->index > e2->src->index)
	    continue;

	  if (try_crossjump_to_edge (mode, e, e2))
	    {
	      changed = true;
	      nexte = bb->pred;
	      break;
	    }
	}
    }

  return changed;
}

/* Do simple CFG optimizations - basic block merging, simplifying of jump
   instructions etc.  Return nonzero if changes were made.  */

static bool
try_optimize_cfg (int mode)
{
  bool changed_overall = false;
  bool changed;
  int iterations = 0;
  basic_block bb, b, next;

  if (mode & CLEANUP_CROSSJUMP)
    add_noreturn_fake_exit_edges ();

  FOR_EACH_BB (bb)
    update_forwarder_flag (bb);

  if (mode & CLEANUP_UPDATE_LIFE)
    clear_bb_flags ();

  if (! (* targetm.cannot_modify_jumps_p) ())
    {
      /* Attempt to merge blocks as made possible by edge removal.  If
	 a block has only one successor, and the successor has only
	 one predecessor, they may be combined.  */
      do
	{
	  changed = false;
	  iterations++;

	  if (rtl_dump_file)
	    fprintf (rtl_dump_file,
		     "\n\ntry_optimize_cfg iteration %i\n\n",
		     iterations);

	  for (b = ENTRY_BLOCK_PTR->next_bb; b != EXIT_BLOCK_PTR;)
	    {
	      basic_block c;
	      edge s;
	      bool changed_here = false;

	      /* Delete trivially dead basic blocks.  */
	      while (b->pred == NULL)
		{
		  c = b->prev_bb;
		  if (rtl_dump_file)
		    fprintf (rtl_dump_file, "Deleting block %i.\n",
			     b->index);

		  delete_block (b);
		  if (!(mode & CLEANUP_CFGLAYOUT))
		    changed = true;
		  b = c;
		}

	      /* Remove code labels no longer used.  Don't do this
		 before CALL_PLACEHOLDER is removed, as some branches
		 may be hidden within.  */
	      if (b->pred->pred_next == NULL
		  && (b->pred->flags & EDGE_FALLTHRU)
		  && !(b->pred->flags & EDGE_COMPLEX)
		  && GET_CODE (BB_HEAD (b)) == CODE_LABEL
		  && (!(mode & CLEANUP_PRE_SIBCALL)
		      || !tail_recursion_label_p (BB_HEAD (b)))
		  /* If the previous block ends with a branch to this
		     block, we can't delete the label.  Normally this
		     is a condjump that is yet to be simplified, but
		     if CASE_DROPS_THRU, this can be a tablejump with
		     some element going to the same place as the
		     default (fallthru).  */
		  && (b->pred->src == ENTRY_BLOCK_PTR
		      || GET_CODE (BB_END (b->pred->src)) != JUMP_INSN
		      || ! label_is_jump_target_p (BB_HEAD (b),
						   BB_END (b->pred->src))))
		{
		  rtx label = BB_HEAD (b);

		  delete_insn_chain (label, label);
		  /* In the case label is undeletable, move it after the
		     BASIC_BLOCK note.  */
		  if (NOTE_LINE_NUMBER (BB_HEAD (b)) == NOTE_INSN_DELETED_LABEL)
		    {
		      rtx bb_note = NEXT_INSN (BB_HEAD (b));

		      reorder_insns_nobb (label, label, bb_note);
		      BB_HEAD (b) = bb_note;
		    }
		  if (rtl_dump_file)
		    fprintf (rtl_dump_file, "Deleted label in block %i.\n",
			     b->index);
		}

	      /* If we fall through an empty block, we can remove it.  */
	      if (!(mode & CLEANUP_CFGLAYOUT)
		  && b->pred->pred_next == NULL
		  && (b->pred->flags & EDGE_FALLTHRU)
		  && GET_CODE (BB_HEAD (b)) != CODE_LABEL
		  && FORWARDER_BLOCK_P (b)
		  /* Note that forwarder_block_p true ensures that
		     there is a successor for this block.  */
		  && (b->succ->flags & EDGE_FALLTHRU)
		  && n_basic_blocks > 1)
		{
		  if (rtl_dump_file)
		    fprintf (rtl_dump_file,
			     "Deleting fallthru block %i.\n",
			     b->index);

		  c = b->prev_bb == ENTRY_BLOCK_PTR ? b->next_bb : b->prev_bb;
		  redirect_edge_succ_nodup (b->pred, b->succ->dest);
		  delete_block (b);
		  changed = true;
		  b = c;
		}

	      if ((s = b->succ) != NULL
		  && s->succ_next == NULL
		  && !(s->flags & EDGE_COMPLEX)
		  && (c = s->dest) != EXIT_BLOCK_PTR
		  && c->pred->pred_next == NULL
		  && b != c)
		{
		  /* When not in cfg_layout mode use code aware of reordering
		     INSN.  This code possibly creates new basic blocks so it
		     does not fit merge_blocks interface and is kept here in
		     hope that it will become useless once more of compiler
		     is transformed to use cfg_layout mode.  */
		     
		  if ((mode & CLEANUP_CFGLAYOUT)
		      && can_merge_blocks_p (b, c))
		    {
		      merge_blocks (b, c);
		      update_forwarder_flag (b);
		      changed_here = true;
		    }
		  else if (!(mode & CLEANUP_CFGLAYOUT)
			   /* If the jump insn has side effects,
			      we can't kill the edge.  */
			   && (GET_CODE (BB_END (b)) != JUMP_INSN
			       || (reload_completed
				   ? simplejump_p (BB_END (b))
				   : onlyjump_p (BB_END (b))))
			   && (next = merge_blocks_move (s, b, c, mode)))
		      {
			b = next;
			changed_here = true;
		      }
		}

	      /* Simplify branch over branch.  */
	      if ((mode & CLEANUP_EXPENSIVE)
		   && !(mode & CLEANUP_CFGLAYOUT)
		   && try_simplify_condjump (b))
		changed_here = true;

	      /* If B has a single outgoing edge, but uses a
		 non-trivial jump instruction without side-effects, we
		 can either delete the jump entirely, or replace it
		 with a simple unconditional jump.  */
	      if (b->succ
		  && ! b->succ->succ_next
		  && b->succ->dest != EXIT_BLOCK_PTR
		  && onlyjump_p (BB_END (b))
		  && try_redirect_by_replacing_jump (b->succ, b->succ->dest,
						     (mode & CLEANUP_CFGLAYOUT) != 0))
		{
		  update_forwarder_flag (b);
		  changed_here = true;
		}

	      /* Simplify branch to branch.  */
	      if (try_forward_edges (mode, b))
		changed_here = true;

	      /* Look for shared code between blocks.  */
	      if ((mode & CLEANUP_CROSSJUMP)
		  && try_crossjump_bb (mode, b))
		changed_here = true;

	      /* Don't get confused by the index shift caused by
		 deleting blocks.  */
	      if (!changed_here)
		b = b->next_bb;
	      else
		changed = true;
	    }

	  if ((mode & CLEANUP_CROSSJUMP)
	      && try_crossjump_bb (mode, EXIT_BLOCK_PTR))
	    changed = true;

#ifdef ENABLE_CHECKING
	  if (changed)
	    verify_flow_info ();
#endif

	  changed_overall |= changed;
	}
      while (changed);
    }

  if (mode & CLEANUP_CROSSJUMP)
    remove_fake_edges ();

  clear_aux_for_blocks ();

  return changed_overall;
}

/* Delete all unreachable basic blocks.  */

bool
delete_unreachable_blocks (void)
{
  bool changed = false;
  basic_block b, next_bb;

  find_unreachable_blocks ();

  /* Delete all unreachable basic blocks.  */

  for (b = ENTRY_BLOCK_PTR->next_bb; b != EXIT_BLOCK_PTR; b = next_bb)
    {
      next_bb = b->next_bb;

      if (!(b->flags & BB_REACHABLE))
	{
	  delete_block (b);
	  changed = true;
	}
    }

  if (changed)
    tidy_fallthru_edges ();
  return changed;
}

/* Tidy the CFG by deleting unreachable code and whatnot.  */

bool
cleanup_cfg (int mode)
{
  bool changed = false;

  timevar_push (TV_CLEANUP_CFG);
  if (delete_unreachable_blocks ())
    {
      changed = true;
      /* We've possibly created trivially dead code.  Cleanup it right
	 now to introduce more opportunities for try_optimize_cfg.  */
      if (!(mode & (CLEANUP_NO_INSN_DEL
		    | CLEANUP_UPDATE_LIFE | CLEANUP_PRE_SIBCALL))
	  && !reload_completed)
	delete_trivially_dead_insns (get_insns(), max_reg_num ());
    }

  compact_blocks ();

  while (try_optimize_cfg (mode))
    {
      delete_unreachable_blocks (), changed = true;
      if (mode & CLEANUP_UPDATE_LIFE)
	{
	  /* Cleaning up CFG introduces more opportunities for dead code
	     removal that in turn may introduce more opportunities for
	     cleaning up the CFG.  */
	  if (!update_life_info_in_dirty_blocks (UPDATE_LIFE_GLOBAL_RM_NOTES,
						 PROP_DEATH_NOTES
						 | PROP_SCAN_DEAD_CODE
						 | PROP_KILL_DEAD_CODE
			  			 | ((mode & CLEANUP_LOG_LINKS)
						    ? PROP_LOG_LINKS : 0)))
	    break;
	}
      else if (!(mode & (CLEANUP_NO_INSN_DEL | CLEANUP_PRE_SIBCALL))
	       && (mode & CLEANUP_EXPENSIVE)
	       && !reload_completed)
	{
	  if (!delete_trivially_dead_insns (get_insns(), max_reg_num ()))
	    break;
	}
      else
	break;
      delete_dead_jumptables ();
    }

  /* Kill the data we won't maintain.  */
  free_EXPR_LIST_list (&label_value_list);
  timevar_pop (TV_CLEANUP_CFG);

  return changed;
}
