/* Generate code from machine description to compute values of attributes.
   Copyright (C) 1991, 1993, 1994, 1995, 1996, 1997, 1998,
   1999, 2000, 2002, 2003, 2004 Free Software Foundation, Inc.
   Contributed by Richard Kenner (kenner@vlsi1.ultra.nyu.edu)

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

/* This program handles insn attributes and the DEFINE_DELAY and
   DEFINE_FUNCTION_UNIT definitions.

   It produces a series of functions named `get_attr_...', one for each insn
   attribute.  Each of these is given the rtx for an insn and returns a member
   of the enum for the attribute.

   These subroutines have the form of a `switch' on the INSN_CODE (via
   `recog_memoized').  Each case either returns a constant attribute value
   or a value that depends on tests on other attributes, the form of
   operands, or some random C expression (encoded with a SYMBOL_REF
   expression).

   If the attribute `alternative', or a random C expression is present,
   `constrain_operands' is called.  If either of these cases of a reference to
   an operand is found, `extract_insn' is called.

   The special attribute `length' is also recognized.  For this operand,
   expressions involving the address of an operand or the current insn,
   (address (pc)), are valid.  In this case, an initial pass is made to
   set all lengths that do not depend on address.  Those that do are set to
   the maximum length.  Then each insn that depends on an address is checked
   and possibly has its length changed.  The process repeats until no further
   changed are made.  The resulting lengths are saved for use by
   `get_attr_length'.

   A special form of DEFINE_ATTR, where the expression for default value is a
   CONST expression, indicates an attribute that is constant for a given run
   of the compiler.  The subroutine generated for these attributes has no
   parameters as it does not depend on any particular insn.  Constant
   attributes are typically used to specify which variety of processor is
   used.

   Internal attributes are defined to handle DEFINE_DELAY and
   DEFINE_FUNCTION_UNIT.  Special routines are output for these cases.

   This program works by keeping a list of possible values for each attribute.
   These include the basic attribute choices, default values for attribute, and
   all derived quantities.

   As the description file is read, the definition for each insn is saved in a
   `struct insn_def'.   When the file reading is complete, a `struct insn_ent'
   is created for each insn and chained to the corresponding attribute value,
   either that specified, or the default.

   An optimization phase is then run.  This simplifies expressions for each
   insn.  EQ_ATTR tests are resolved, whenever possible, to a test that
   indicates when the attribute has the specified value for the insn.  This
   avoids recursive calls during compilation.

   The strategy used when processing DEFINE_DELAY and DEFINE_FUNCTION_UNIT
   definitions is to create arbitrarily complex expressions and have the
   optimization simplify them.

   Once optimization is complete, any required routines and definitions
   will be written.

   An optimization that is not yet implemented is to hoist the constant
   expressions entirely out of the routines and definitions that are written.
   A way to do this is to iterate over all possible combinations of values
   for constant attributes and generate a set of functions for that given
   combination.  An initialization function would be written that evaluates
   the attributes and installs the corresponding set of routines and
   definitions (each would be accessed through a pointer).

   We use the flags in an RTX as follows:
   `unchanging' (ATTR_IND_SIMPLIFIED_P): This rtx is fully simplified
      independent of the insn code.
   `in_struct' (ATTR_CURR_SIMPLIFIED_P): This rtx is fully simplified
      for the insn code currently being processed (see optimize_attrs).
   `integrated' (ATTR_PERMANENT_P): This rtx is permanent and unique
      (see attr_rtx).
   `volatil' (ATTR_EQ_ATTR_P): During simplify_by_exploding the value of an
      EQ_ATTR rtx is true if !volatil and false if volatil.  */

#define ATTR_IND_SIMPLIFIED_P(RTX) (RTX_FLAG((RTX), unchanging))
#define ATTR_CURR_SIMPLIFIED_P(RTX) (RTX_FLAG((RTX), in_struct))
#define ATTR_PERMANENT_P(RTX) (RTX_FLAG((RTX), integrated))
#define ATTR_EQ_ATTR_P(RTX) (RTX_FLAG((RTX), volatil))

#if 0
#define strcmp_check(S1, S2) ((S1) == (S2)		\
			      ? 0			\
			      : (strcmp ((S1), (S2))	\
				 ? 1			\
				 : (abort (), 0)))
#else
#define strcmp_check(S1, S2) ((S1) != (S2))
#endif

#include "bconfig.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "ggc.h"
#include "gensupport.h"

#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

/* We must include obstack.h after <sys/time.h>, to avoid lossage with
   /usr/include/sys/stdtypes.h on Sun OS 4.x.  */
#include "obstack.h"
#include "errors.h"

#include "genattrtab.h"

static struct obstack obstack1, obstack2;
struct obstack *hash_obstack = &obstack1;
struct obstack *temp_obstack = &obstack2;

/* enough space to reserve for printing out ints */
#define MAX_DIGITS (HOST_BITS_PER_INT * 3 / 10 + 3)

/* Define structures used to record attributes and values.  */

/* As each DEFINE_INSN, DEFINE_PEEPHOLE, or DEFINE_ASM_ATTRIBUTES is
   encountered, we store all the relevant information into a
   `struct insn_def'.  This is done to allow attribute definitions to occur
   anywhere in the file.  */

struct insn_def
{
  struct insn_def *next;	/* Next insn in chain.  */
  rtx def;			/* The DEFINE_...  */
  int insn_code;		/* Instruction number.  */
  int insn_index;		/* Expression numer in file, for errors.  */
  int lineno;			/* Line number.  */
  int num_alternatives;		/* Number of alternatives.  */
  int vec_idx;			/* Index of attribute vector in `def'.  */
};

/* Once everything has been read in, we store in each attribute value a list
   of insn codes that have that value.  Here is the structure used for the
   list.  */

struct insn_ent
{
  struct insn_ent *next;	/* Next in chain.  */
  int insn_code;		/* Instruction number.  */
  int insn_index;		/* Index of definition in file */
  int lineno;			/* Line number.  */
};

/* Each value of an attribute (either constant or computed) is assigned a
   structure which is used as the listhead of the insns that have that
   value.  */

struct attr_value
{
  rtx value;			/* Value of attribute.  */
  struct attr_value *next;	/* Next attribute value in chain.  */
  struct insn_ent *first_insn;	/* First insn with this value.  */
  int num_insns;		/* Number of insns with this value.  */
  int has_asm_insn;		/* True if this value used for `asm' insns */
};

/* Structure for each attribute.  */

struct attr_desc
{
  char *name;			/* Name of attribute.  */
  struct attr_desc *next;	/* Next attribute.  */
  struct attr_value *first_value; /* First value of this attribute.  */
  struct attr_value *default_val; /* Default value for this attribute.  */
  int lineno : 24;		/* Line number.  */
  unsigned is_numeric	: 1;	/* Values of this attribute are numeric.  */
  unsigned negative_ok	: 1;	/* Allow negative numeric values.  */
  unsigned unsigned_p	: 1;	/* Make the output function unsigned int.  */
  unsigned is_const	: 1;	/* Attribute value constant for each run.  */
  unsigned is_special	: 1;	/* Don't call `write_attr_set'.  */
  unsigned func_units_p	: 1;	/* This is the function_units attribute.  */
  unsigned blockage_p	: 1;	/* This is the blockage range function.  */
  unsigned static_p	: 1;	/* Make the output function static.  */
};

#define NULL_ATTR (struct attr_desc *) NULL

/* A range of values.  */

struct range
{
  int min;
  int max;
};

/* Structure for each DEFINE_DELAY.  */

struct delay_desc
{
  rtx def;			/* DEFINE_DELAY expression.  */
  struct delay_desc *next;	/* Next DEFINE_DELAY.  */
  int num;			/* Number of DEFINE_DELAY, starting at 1.  */
  int lineno;			/* Line number.  */
};

/* Record information about each DEFINE_FUNCTION_UNIT.  */

struct function_unit_op
{
  rtx condexp;			/* Expression TRUE for applicable insn.  */
  struct function_unit_op *next; /* Next operation for this function unit.  */
  int num;			/* Ordinal for this operation type in unit.  */
  int ready;			/* Cost until data is ready.  */
  int issue_delay;		/* Cost until unit can accept another insn.  */
  rtx conflict_exp;		/* Expression TRUE for insns incurring issue delay.  */
  rtx issue_exp;		/* Expression computing issue delay.  */
  int lineno;			/* Line number.  */
};

/* Record information about each function unit mentioned in a
   DEFINE_FUNCTION_UNIT.  */

struct function_unit
{
  const char *name;		/* Function unit name.  */
  struct function_unit *next;	/* Next function unit.  */
  int num;			/* Ordinal of this unit type.  */
  int multiplicity;		/* Number of units of this type.  */
  int simultaneity;		/* Maximum number of simultaneous insns
				   on this function unit or 0 if unlimited.  */
  rtx condexp;			/* Expression TRUE for insn needing unit.  */
  int num_opclasses;		/* Number of different operation types.  */
  struct function_unit_op *ops;	/* Pointer to first operation type.  */
  int needs_conflict_function;	/* Nonzero if a conflict function required.  */
  int needs_blockage_function;	/* Nonzero if a blockage function required.  */
  int needs_range_function;	/* Nonzero if blockage range function needed.  */
  rtx default_cost;		/* Conflict cost, if constant.  */
  struct range issue_delay;	/* Range of issue delay values.  */
  int max_blockage;		/* Maximum time an insn blocks the unit.  */
  int first_lineno;		/* First seen line number.  */
};

/* Listheads of above structures.  */

/* This one is indexed by the first character of the attribute name.  */
#define MAX_ATTRS_INDEX 256
static struct attr_desc *attrs[MAX_ATTRS_INDEX];
static struct insn_def *defs;
static struct delay_desc *delays;
static struct function_unit *units;

/* An expression where all the unknown terms are EQ_ATTR tests can be
   rearranged into a COND provided we can enumerate all possible
   combinations of the unknown values.  The set of combinations become the
   tests of the COND; the value of the expression given that combination is
   computed and becomes the corresponding value.  To do this, we must be
   able to enumerate all values for each attribute used in the expression
   (currently, we give up if we find a numeric attribute).

   If the set of EQ_ATTR tests used in an expression tests the value of N
   different attributes, the list of all possible combinations can be made
   by walking the N-dimensional attribute space defined by those
   attributes.  We record each of these as a struct dimension.

   The algorithm relies on sharing EQ_ATTR nodes: if two nodes in an
   expression are the same, the will also have the same address.  We find
   all the EQ_ATTR nodes by marking them ATTR_EQ_ATTR_P.  This bit later
   represents the value of an EQ_ATTR node, so once all nodes are marked,
   they are also given an initial value of FALSE.

   We then separate the set of EQ_ATTR nodes into dimensions for each
   attribute and put them on the VALUES list.  Terms are added as needed by
   `add_values_to_cover' so that all possible values of the attribute are
   tested.

   Each dimension also has a current value.  This is the node that is
   currently considered to be TRUE.  If this is one of the nodes added by
   `add_values_to_cover', all the EQ_ATTR tests in the original expression
   will be FALSE.  Otherwise, only the CURRENT_VALUE will be true.

   NUM_VALUES is simply the length of the VALUES list and is there for
   convenience.

   Once the dimensions are created, the algorithm enumerates all possible
   values and computes the current value of the given expression.  */

struct dimension
{
  struct attr_desc *attr;	/* Attribute for this dimension.  */
  rtx values;			/* List of attribute values used.  */
  rtx current_value;		/* Position in the list for the TRUE value.  */
  int num_values;		/* Length of the values list.  */
};

/* Other variables.  */

static int insn_code_number;
static int insn_index_number;
static int got_define_asm_attributes;
static int must_extract;
static int must_constrain;
static int address_used;
static int length_used;
static int num_delays;
static int have_annul_true, have_annul_false;
static int num_units, num_unit_opclasses;
static int num_insn_ents;

int num_dfa_decls;

/* Used as operand to `operate_exp':  */

enum operator {PLUS_OP, MINUS_OP, POS_MINUS_OP, EQ_OP, OR_OP, ORX_OP, MAX_OP, MIN_OP, RANGE_OP};

/* Stores, for each insn code, the number of constraint alternatives.  */

static int *insn_n_alternatives;

/* Stores, for each insn code, a bitmap that has bits on for each possible
   alternative.  */

static int *insn_alternatives;

/* If nonzero, assume that the `alternative' attr has this value.
   This is the hashed, unique string for the numeral
   whose value is chosen alternative.  */

static const char *current_alternative_string;

/* Used to simplify expressions.  */

static rtx true_rtx, false_rtx;

/* Used to reduce calls to `strcmp' */

static char *alternative_name;
static const char *length_str;
static const char *delay_type_str;
static const char *delay_1_0_str;
static const char *num_delay_slots_str;

/* Indicate that REG_DEAD notes are valid if dead_or_set_p is ever
   called.  */

int reload_completed = 0;

/* Some machines test `optimize' in macros called from rtlanal.c, so we need
   to define it here.  */

int optimize = 0;

/* Simplify an expression.  Only call the routine if there is something to
   simplify.  */
#define SIMPLIFY_TEST_EXP(EXP,INSN_CODE,INSN_INDEX)	\
  (ATTR_IND_SIMPLIFIED_P (EXP) || ATTR_CURR_SIMPLIFIED_P (EXP) ? (EXP)	\
   : simplify_test_exp (EXP, INSN_CODE, INSN_INDEX))

/* Simplify (eq_attr ("alternative") ...)
   when we are working with a particular alternative.  */
#define SIMPLIFY_ALTERNATIVE(EXP)				\
  if (current_alternative_string				\
      && GET_CODE ((EXP)) == EQ_ATTR				\
      && XSTR ((EXP), 0) == alternative_name)			\
    (EXP) = (XSTR ((EXP), 1) == current_alternative_string	\
	    ? true_rtx : false_rtx);

#define DEF_ATTR_STRING(S) (attr_string ((S), strlen (S)))

/* These are referenced by rtlanal.c and hence need to be defined somewhere.
   They won't actually be used.  */

rtx global_rtl[GR_MAX];
rtx pic_offset_table_rtx;

static void attr_hash_add_rtx	(int, rtx);
static void attr_hash_add_string (int, char *);
static rtx attr_rtx		(enum rtx_code, ...);
static rtx attr_rtx_1		(enum rtx_code, va_list);
static char *attr_string        (const char *, int);
static rtx check_attr_value	(rtx, struct attr_desc *);
static rtx convert_set_attr_alternative (rtx, struct insn_def *);
static rtx convert_set_attr	(rtx, struct insn_def *);
static void check_defs		(void);
static rtx make_canonical	(struct attr_desc *, rtx);
static struct attr_value *get_attr_value (rtx, struct attr_desc *, int);
static rtx copy_rtx_unchanging	(rtx);
static rtx copy_boolean		(rtx);
static void expand_delays	(void);
static rtx operate_exp		(enum operator, rtx, rtx);
static void expand_units	(void);
static rtx simplify_knowing	(rtx, rtx);
static rtx encode_units_mask	(rtx);
static void fill_attr		(struct attr_desc *);
static rtx substitute_address	(rtx, rtx (*) (rtx), rtx (*) (rtx));
static void make_length_attrs	(void);
static rtx identity_fn		(rtx);
static rtx zero_fn		(rtx);
static rtx one_fn		(rtx);
static rtx max_fn		(rtx);
static void write_length_unit_log (void);
static rtx simplify_cond	(rtx, int, int);
static rtx simplify_by_exploding (rtx);
static int find_and_mark_used_attributes (rtx, rtx *, int *);
static void unmark_used_attributes (rtx, struct dimension *, int);
static int add_values_to_cover	(struct dimension *);
static int increment_current_value (struct dimension *, int);
static rtx test_for_current_value (struct dimension *, int);
static rtx simplify_with_current_value (rtx, struct dimension *, int);
static rtx simplify_with_current_value_aux (rtx);
static void clear_struct_flag (rtx);
static void remove_insn_ent  (struct attr_value *, struct insn_ent *);
static void insert_insn_ent  (struct attr_value *, struct insn_ent *);
static rtx insert_right_side	(enum rtx_code, rtx, rtx, int, int);
static rtx make_alternative_compare (int);
static int compute_alternative_mask (rtx, enum rtx_code);
static rtx evaluate_eq_attr	(rtx, rtx, int, int);
static rtx simplify_and_tree	(rtx, rtx *, int, int);
static rtx simplify_or_tree	(rtx, rtx *, int, int);
static rtx simplify_test_exp	(rtx, int, int);
static rtx simplify_test_exp_in_temp (rtx, int, int);
static void optimize_attrs	(void);
static void gen_attr		(rtx, int);
static int count_alternatives	(rtx);
static int compares_alternatives_p (rtx);
static int contained_in_p	(rtx, rtx);
static void gen_insn		(rtx, int);
static void gen_delay		(rtx, int);
static void gen_unit		(rtx, int);
static void write_test_expr	(rtx, int);
static int max_attr_value	(rtx, int*);
static int or_attr_value	(rtx, int*);
static void walk_attr_value	(rtx);
static void write_attr_get	(struct attr_desc *);
static rtx eliminate_known_true (rtx, rtx, int, int);
static void write_attr_set	(struct attr_desc *, int, rtx,
				 const char *, const char *, rtx,
				 int, int);
static void write_attr_case	(struct attr_desc *, struct attr_value *,
				 int, const char *, const char *, int, rtx);
static void write_unit_name	(const char *, int, const char *);
static void write_attr_valueq	(struct attr_desc *, const char *);
static void write_attr_value	(struct attr_desc *, rtx);
static void write_upcase	(const char *);
static void write_indent	(int);
static void write_eligible_delay (const char *);
static void write_function_unit_info (void);
static void write_complex_function (struct function_unit *, const char *,
				    const char *);
static int write_expr_attr_cache (rtx, struct attr_desc *);
static void write_toplevel_expr	(rtx);
static void write_const_num_delay_slots (void);
static char *next_comma_elt	(const char **);
static struct attr_desc *find_attr (const char **, int);
static struct attr_value *find_most_used  (struct attr_desc *);
static rtx find_single_value	(struct attr_desc *);
static void extend_range	(struct range *, int, int);
static rtx attr_eq		(const char *, const char *);
static const char *attr_numeral	(int);
static int attr_equal_p		(rtx, rtx);
static rtx attr_copy_rtx	(rtx);
static int attr_rtx_cost	(rtx);
static bool attr_alt_subset_p (rtx, rtx);
static bool attr_alt_subset_of_compl_p (rtx, rtx);
static rtx attr_alt_intersection (rtx, rtx);
static rtx attr_alt_union (rtx, rtx);
static rtx attr_alt_complement (rtx);
static bool attr_alt_bit_p (rtx, int);
static rtx mk_attr_alt (int);

#define oballoc(size) obstack_alloc (hash_obstack, size)

/* Hash table for sharing RTL and strings.  */

/* Each hash table slot is a bucket containing a chain of these structures.
   Strings are given negative hash codes; RTL expressions are given positive
   hash codes.  */

struct attr_hash
{
  struct attr_hash *next;	/* Next structure in the bucket.  */
  int hashcode;			/* Hash code of this rtx or string.  */
  union
    {
      char *str;		/* The string (negative hash codes) */
      rtx rtl;			/* or the RTL recorded here.  */
    } u;
};

/* Now here is the hash table.  When recording an RTL, it is added to
   the slot whose index is the hash code mod the table size.  Note
   that the hash table is used for several kinds of RTL (see attr_rtx)
   and for strings.  While all these live in the same table, they are
   completely independent, and the hash code is computed differently
   for each.  */

#define RTL_HASH_SIZE 4093
struct attr_hash *attr_hash_table[RTL_HASH_SIZE];

/* Here is how primitive or already-shared RTL's hash
   codes are made.  */
#define RTL_HASH(RTL) ((long) (RTL) & 0777777)

/* Add an entry to the hash table for RTL with hash code HASHCODE.  */

static void
attr_hash_add_rtx (int hashcode, rtx rtl)
{
  struct attr_hash *h;

  h = obstack_alloc (hash_obstack, sizeof (struct attr_hash));
  h->hashcode = hashcode;
  h->u.rtl = rtl;
  h->next = attr_hash_table[hashcode % RTL_HASH_SIZE];
  attr_hash_table[hashcode % RTL_HASH_SIZE] = h;
}

/* Add an entry to the hash table for STRING with hash code HASHCODE.  */

static void
attr_hash_add_string (int hashcode, char *str)
{
  struct attr_hash *h;

  h = obstack_alloc (hash_obstack, sizeof (struct attr_hash));
  h->hashcode = -hashcode;
  h->u.str = str;
  h->next = attr_hash_table[hashcode % RTL_HASH_SIZE];
  attr_hash_table[hashcode % RTL_HASH_SIZE] = h;
}

/* Generate an RTL expression, but avoid duplicates.
   Set the ATTR_PERMANENT_P flag for these permanent objects.

   In some cases we cannot uniquify; then we return an ordinary
   impermanent rtx with ATTR_PERMANENT_P clear.

   Args are like gen_rtx, but without the mode:

   rtx attr_rtx (code, [element1, ..., elementn])  */

static rtx
attr_rtx_1 (enum rtx_code code, va_list p)
{
  rtx rt_val = NULL_RTX;/* RTX to return to caller...		*/
  int hashcode;
  struct attr_hash *h;
  struct obstack *old_obstack = rtl_obstack;

  /* For each of several cases, search the hash table for an existing entry.
     Use that entry if one is found; otherwise create a new RTL and add it
     to the table.  */

  if (GET_RTX_CLASS (code) == '1')
    {
      rtx arg0 = va_arg (p, rtx);

      /* A permanent object cannot point to impermanent ones.  */
      if (! ATTR_PERMANENT_P (arg0))
	{
	  rt_val = rtx_alloc (code);
	  XEXP (rt_val, 0) = arg0;
	  return rt_val;
	}

      hashcode = ((HOST_WIDE_INT) code + RTL_HASH (arg0));
      for (h = attr_hash_table[hashcode % RTL_HASH_SIZE]; h; h = h->next)
	if (h->hashcode == hashcode
	    && GET_CODE (h->u.rtl) == code
	    && XEXP (h->u.rtl, 0) == arg0)
	  return h->u.rtl;

      if (h == 0)
	{
	  rtl_obstack = hash_obstack;
	  rt_val = rtx_alloc (code);
	  XEXP (rt_val, 0) = arg0;
	}
    }
  else if (GET_RTX_CLASS (code) == 'c'
	   || GET_RTX_CLASS (code) == '2'
	   || GET_RTX_CLASS (code) == '<')
    {
      rtx arg0 = va_arg (p, rtx);
      rtx arg1 = va_arg (p, rtx);

      /* A permanent object cannot point to impermanent ones.  */
      if (! ATTR_PERMANENT_P (arg0) || ! ATTR_PERMANENT_P (arg1))
	{
	  rt_val = rtx_alloc (code);
	  XEXP (rt_val, 0) = arg0;
	  XEXP (rt_val, 1) = arg1;
	  return rt_val;
	}

      hashcode = ((HOST_WIDE_INT) code + RTL_HASH (arg0) + RTL_HASH (arg1));
      for (h = attr_hash_table[hashcode % RTL_HASH_SIZE]; h; h = h->next)
	if (h->hashcode == hashcode
	    && GET_CODE (h->u.rtl) == code
	    && XEXP (h->u.rtl, 0) == arg0
	    && XEXP (h->u.rtl, 1) == arg1)
	  return h->u.rtl;

      if (h == 0)
	{
	  rtl_obstack = hash_obstack;
	  rt_val = rtx_alloc (code);
	  XEXP (rt_val, 0) = arg0;
	  XEXP (rt_val, 1) = arg1;
	}
    }
  else if (GET_RTX_LENGTH (code) == 1
	   && GET_RTX_FORMAT (code)[0] == 's')
    {
      char *arg0 = va_arg (p, char *);

      arg0 = DEF_ATTR_STRING (arg0);

      hashcode = ((HOST_WIDE_INT) code + RTL_HASH (arg0));
      for (h = attr_hash_table[hashcode % RTL_HASH_SIZE]; h; h = h->next)
	if (h->hashcode == hashcode
	    && GET_CODE (h->u.rtl) == code
	    && XSTR (h->u.rtl, 0) == arg0)
	  return h->u.rtl;

      if (h == 0)
	{
	  rtl_obstack = hash_obstack;
	  rt_val = rtx_alloc (code);
	  XSTR (rt_val, 0) = arg0;
	}
    }
  else if (GET_RTX_LENGTH (code) == 2
	   && GET_RTX_FORMAT (code)[0] == 's'
	   && GET_RTX_FORMAT (code)[1] == 's')
    {
      char *arg0 = va_arg (p, char *);
      char *arg1 = va_arg (p, char *);

      hashcode = ((HOST_WIDE_INT) code + RTL_HASH (arg0) + RTL_HASH (arg1));
      for (h = attr_hash_table[hashcode % RTL_HASH_SIZE]; h; h = h->next)
	if (h->hashcode == hashcode
	    && GET_CODE (h->u.rtl) == code
	    && XSTR (h->u.rtl, 0) == arg0
	    && XSTR (h->u.rtl, 1) == arg1)
	  return h->u.rtl;

      if (h == 0)
	{
	  rtl_obstack = hash_obstack;
	  rt_val = rtx_alloc (code);
	  XSTR (rt_val, 0) = arg0;
	  XSTR (rt_val, 1) = arg1;
	}
    }
  else if (code == CONST_INT)
    {
      HOST_WIDE_INT arg0 = va_arg (p, HOST_WIDE_INT);
      if (arg0 == 0)
	return false_rtx;
      else if (arg0 == 1)
	return true_rtx;
      else
	goto nohash;
    }
  else
    {
      int i;		/* Array indices...			*/
      const char *fmt;	/* Current rtx's format...		*/
    nohash:
      rt_val = rtx_alloc (code);	/* Allocate the storage space.  */

      fmt = GET_RTX_FORMAT (code);	/* Find the right format...  */
      for (i = 0; i < GET_RTX_LENGTH (code); i++)
	{
	  switch (*fmt++)
	    {
	    case '0':		/* Unused field.  */
	      break;

	    case 'i':		/* An integer?  */
	      XINT (rt_val, i) = va_arg (p, int);
	      break;

	    case 'w':		/* A wide integer? */
	      XWINT (rt_val, i) = va_arg (p, HOST_WIDE_INT);
	      break;

	    case 's':		/* A string?  */
	      XSTR (rt_val, i) = va_arg (p, char *);
	      break;

	    case 'e':		/* An expression?  */
	    case 'u':		/* An insn?  Same except when printing.  */
	      XEXP (rt_val, i) = va_arg (p, rtx);
	      break;

	    case 'E':		/* An RTX vector?  */
	      XVEC (rt_val, i) = va_arg (p, rtvec);
	      break;

	    default:
	      abort ();
	    }
	}
      return rt_val;
    }

  rtl_obstack = old_obstack;
  attr_hash_add_rtx (hashcode, rt_val);
  ATTR_PERMANENT_P (rt_val) = 1;
  return rt_val;
}

static rtx
attr_rtx (enum rtx_code code, ...)
{
  rtx result;
  va_list p;

  va_start (p, code);
  result = attr_rtx_1 (code, p);
  va_end (p);
  return result;
}

/* Create a new string printed with the printf line arguments into a space
   of at most LEN bytes:

   rtx attr_printf (len, format, [arg1, ..., argn])  */

char *
attr_printf (unsigned int len, const char *fmt, ...)
{
  char str[256];
  va_list p;

  va_start (p, fmt);

  if (len > sizeof str - 1) /* Leave room for \0.  */
    abort ();

  vsprintf (str, fmt, p);
  va_end (p);

  return DEF_ATTR_STRING (str);
}

static rtx
attr_eq (const char *name, const char *value)
{
  return attr_rtx (EQ_ATTR, DEF_ATTR_STRING (name), DEF_ATTR_STRING (value));
}

static const char *
attr_numeral (int n)
{
  return XSTR (make_numeric_value (n), 0);
}

/* Return a permanent (possibly shared) copy of a string STR (not assumed
   to be null terminated) with LEN bytes.  */

static char *
attr_string (const char *str, int len)
{
  struct attr_hash *h;
  int hashcode;
  int i;
  char *new_str;

  /* Compute the hash code.  */
  hashcode = (len + 1) * 613 + (unsigned) str[0];
  for (i = 1; i <= len; i += 2)
    hashcode = ((hashcode * 613) + (unsigned) str[i]);
  if (hashcode < 0)
    hashcode = -hashcode;

  /* Search the table for the string.  */
  for (h = attr_hash_table[hashcode % RTL_HASH_SIZE]; h; h = h->next)
    if (h->hashcode == -hashcode && h->u.str[0] == str[0]
	&& !strncmp (h->u.str, str, len))
      return h->u.str;			/* <-- return if found.  */

  /* Not found; create a permanent copy and add it to the hash table.  */
  new_str = obstack_alloc (hash_obstack, len + 1);
  memcpy (new_str, str, len);
  new_str[len] = '\0';
  attr_hash_add_string (hashcode, new_str);

  return new_str;			/* Return the new string.  */
}

/* Check two rtx's for equality of contents,
   taking advantage of the fact that if both are hashed
   then they can't be equal unless they are the same object.  */

static int
attr_equal_p (rtx x, rtx y)
{
  return (x == y || (! (ATTR_PERMANENT_P (x) && ATTR_PERMANENT_P (y))
		     && rtx_equal_p (x, y)));
}

/* Copy an attribute value expression,
   descending to all depths, but not copying any
   permanent hashed subexpressions.  */

static rtx
attr_copy_rtx (rtx orig)
{
  rtx copy;
  int i, j;
  RTX_CODE code;
  const char *format_ptr;

  /* No need to copy a permanent object.  */
  if (ATTR_PERMANENT_P (orig))
    return orig;

  code = GET_CODE (orig);

  switch (code)
    {
    case REG:
    case QUEUED:
    case CONST_INT:
    case CONST_DOUBLE:
    case CONST_VECTOR:
    case SYMBOL_REF:
    case CODE_LABEL:
    case PC:
    case CC0:
      return orig;

    default:
      break;
    }

  copy = rtx_alloc (code);
  PUT_MODE (copy, GET_MODE (orig));
  ATTR_IND_SIMPLIFIED_P (copy) = ATTR_IND_SIMPLIFIED_P (orig);
  ATTR_CURR_SIMPLIFIED_P (copy) = ATTR_CURR_SIMPLIFIED_P (orig);
  ATTR_PERMANENT_P (copy) = ATTR_PERMANENT_P (orig);
  ATTR_EQ_ATTR_P (copy) = ATTR_EQ_ATTR_P (orig);

  format_ptr = GET_RTX_FORMAT (GET_CODE (copy));

  for (i = 0; i < GET_RTX_LENGTH (GET_CODE (copy)); i++)
    {
      switch (*format_ptr++)
	{
	case 'e':
	  XEXP (copy, i) = XEXP (orig, i);
	  if (XEXP (orig, i) != NULL)
	    XEXP (copy, i) = attr_copy_rtx (XEXP (orig, i));
	  break;

	case 'E':
	case 'V':
	  XVEC (copy, i) = XVEC (orig, i);
	  if (XVEC (orig, i) != NULL)
	    {
	      XVEC (copy, i) = rtvec_alloc (XVECLEN (orig, i));
	      for (j = 0; j < XVECLEN (copy, i); j++)
		XVECEXP (copy, i, j) = attr_copy_rtx (XVECEXP (orig, i, j));
	    }
	  break;

	case 'n':
	case 'i':
	  XINT (copy, i) = XINT (orig, i);
	  break;

	case 'w':
	  XWINT (copy, i) = XWINT (orig, i);
	  break;

	case 's':
	case 'S':
	  XSTR (copy, i) = XSTR (orig, i);
	  break;

	default:
	  abort ();
	}
    }
  return copy;
}

/* Given a test expression for an attribute, ensure it is validly formed.
   IS_CONST indicates whether the expression is constant for each compiler
   run (a constant expression may not test any particular insn).

   Convert (eq_attr "att" "a1,a2") to (ior (eq_attr ... ) (eq_attrq ..))
   and (eq_attr "att" "!a1") to (not (eq_attr "att" "a1")).  Do the latter
   test first so that (eq_attr "att" "!a1,a2,a3") works as expected.

   Update the string address in EQ_ATTR expression to be the same used
   in the attribute (or `alternative_name') to speed up subsequent
   `find_attr' calls and eliminate most `strcmp' calls.

   Return the new expression, if any.  */

rtx
check_attr_test (rtx exp, int is_const, int lineno)
{
  struct attr_desc *attr;
  struct attr_value *av;
  const char *name_ptr, *p;
  rtx orexp, newexp;

  switch (GET_CODE (exp))
    {
    case EQ_ATTR:
      /* Handle negation test.  */
      if (XSTR (exp, 1)[0] == '!')
	return check_attr_test (attr_rtx (NOT,
					  attr_eq (XSTR (exp, 0),
						   &XSTR (exp, 1)[1])),
				is_const, lineno);

      else if (n_comma_elts (XSTR (exp, 1)) == 1)
	{
	  attr = find_attr (&XSTR (exp, 0), 0);
	  if (attr == NULL)
	    {
	      if (! strcmp (XSTR (exp, 0), "alternative"))
		return mk_attr_alt (1 << atoi (XSTR (exp, 1)));
	      else
		fatal ("unknown attribute `%s' in EQ_ATTR", XSTR (exp, 0));
	    }

	  if (is_const && ! attr->is_const)
	    fatal ("constant expression uses insn attribute `%s' in EQ_ATTR",
		   XSTR (exp, 0));

	  /* Copy this just to make it permanent,
	     so expressions using it can be permanent too.  */
	  exp = attr_eq (XSTR (exp, 0), XSTR (exp, 1));

	  /* It shouldn't be possible to simplify the value given to a
	     constant attribute, so don't expand this until it's time to
	     write the test expression.  */
	  if (attr->is_const)
	    ATTR_IND_SIMPLIFIED_P (exp) = 1;

	  if (attr->is_numeric)
	    {
	      for (p = XSTR (exp, 1); *p; p++)
		if (! ISDIGIT (*p))
		  fatal ("attribute `%s' takes only numeric values",
			 XSTR (exp, 0));
	    }
	  else
	    {
	      for (av = attr->first_value; av; av = av->next)
		if (GET_CODE (av->value) == CONST_STRING
		    && ! strcmp (XSTR (exp, 1), XSTR (av->value, 0)))
		  break;

	      if (av == NULL)
		fatal ("unknown value `%s' for `%s' attribute",
		       XSTR (exp, 1), XSTR (exp, 0));
	    }
	}
      else
	{
	  if (! strcmp (XSTR (exp, 0), "alternative"))
	    {
	      int set = 0;

	      name_ptr = XSTR (exp, 1);
	      while ((p = next_comma_elt (&name_ptr)) != NULL)
		set |= 1 << atoi (p);

	      return mk_attr_alt (set);
	    }
	  else
	    {
	      /* Make an IOR tree of the possible values.  */
	      orexp = false_rtx;
	      name_ptr = XSTR (exp, 1);
	      while ((p = next_comma_elt (&name_ptr)) != NULL)
		{
		  newexp = attr_eq (XSTR (exp, 0), p);
		  orexp = insert_right_side (IOR, orexp, newexp, -2, -2);
		}

	      return check_attr_test (orexp, is_const, lineno);
	    }
	}
      break;

    case ATTR_FLAG:
      break;

    case CONST_INT:
      /* Either TRUE or FALSE.  */
      if (XWINT (exp, 0))
	return true_rtx;
      else
	return false_rtx;

    case IOR:
    case AND:
      XEXP (exp, 0) = check_attr_test (XEXP (exp, 0), is_const, lineno);
      XEXP (exp, 1) = check_attr_test (XEXP (exp, 1), is_const, lineno);
      break;

    case NOT:
      XEXP (exp, 0) = check_attr_test (XEXP (exp, 0), is_const, lineno);
      break;

    case MATCH_INSN:
    case MATCH_OPERAND:
      if (is_const)
	fatal ("RTL operator \"%s\" not valid in constant attribute test",
	       GET_RTX_NAME (GET_CODE (exp)));
      /* These cases can't be simplified.  */
      ATTR_IND_SIMPLIFIED_P (exp) = 1;
      break;

    case LE:  case LT:  case GT:  case GE:
    case LEU: case LTU: case GTU: case GEU:
    case NE:  case EQ:
      if (GET_CODE (XEXP (exp, 0)) == SYMBOL_REF
	  && GET_CODE (XEXP (exp, 1)) == SYMBOL_REF)
	exp = attr_rtx (GET_CODE (exp),
			attr_rtx (SYMBOL_REF, XSTR (XEXP (exp, 0), 0)),
			attr_rtx (SYMBOL_REF, XSTR (XEXP (exp, 1), 0)));
      /* These cases can't be simplified.  */
      ATTR_IND_SIMPLIFIED_P (exp) = 1;
      break;

    case SYMBOL_REF:
      if (is_const)
	{
	  /* These cases are valid for constant attributes, but can't be
	     simplified.  */
	  exp = attr_rtx (SYMBOL_REF, XSTR (exp, 0));
	  ATTR_IND_SIMPLIFIED_P (exp) = 1;
	  break;
	}
    default:
      fatal ("RTL operator \"%s\" not valid in attribute test",
	     GET_RTX_NAME (GET_CODE (exp)));
    }

  return exp;
}

/* Given an expression, ensure that it is validly formed and that all named
   attribute values are valid for the given attribute.  Issue a fatal error
   if not.  If no attribute is specified, assume a numeric attribute.

   Return a perhaps modified replacement expression for the value.  */

static rtx
check_attr_value (rtx exp, struct attr_desc *attr)
{
  struct attr_value *av;
  const char *p;
  int i;

  switch (GET_CODE (exp))
    {
    case CONST_INT:
      if (attr && ! attr->is_numeric)
	{
	  message_with_line (attr->lineno,
			     "CONST_INT not valid for non-numeric attribute %s",
			     attr->name);
	  have_error = 1;
	  break;
	}

      if (INTVAL (exp) < 0 && ! attr->negative_ok)
	{
	  message_with_line (attr->lineno,
			     "negative numeric value specified for attribute %s",
			     attr->name);
	  have_error = 1;
	  break;
	}
      break;

    case CONST_STRING:
      if (! strcmp (XSTR (exp, 0), "*"))
	break;

      if (attr == 0 || attr->is_numeric)
	{
	  p = XSTR (exp, 0);
	  if (attr && attr->negative_ok && *p == '-')
	    p++;
	  for (; *p; p++)
	    if (! ISDIGIT (*p))
	      {
		message_with_line (attr ? attr->lineno : 0,
				   "non-numeric value for numeric attribute %s",
				   attr ? attr->name : "internal");
		have_error = 1;
		break;
	      }
	  break;
	}

      for (av = attr->first_value; av; av = av->next)
	if (GET_CODE (av->value) == CONST_STRING
	    && ! strcmp (XSTR (av->value, 0), XSTR (exp, 0)))
	  break;

      if (av == NULL)
	{
	  message_with_line (attr->lineno,
			     "unknown value `%s' for `%s' attribute",
			     XSTR (exp, 0), attr ? attr->name : "internal");
	  have_error = 1;
	}
      break;

    case IF_THEN_ELSE:
      XEXP (exp, 0) = check_attr_test (XEXP (exp, 0),
				       attr ? attr->is_const : 0,
				       attr ? attr->lineno : 0);
      XEXP (exp, 1) = check_attr_value (XEXP (exp, 1), attr);
      XEXP (exp, 2) = check_attr_value (XEXP (exp, 2), attr);
      break;

    case PLUS:
    case MINUS:
    case MULT:
    case DIV:
    case MOD:
      if (attr && !attr->is_numeric)
	{
	  message_with_line (attr->lineno,
			     "invalid operation `%s' for non-numeric attribute value",
			     GET_RTX_NAME (GET_CODE (exp)));
	  have_error = 1;
	  break;
	}
      /* Fall through.  */

    case IOR:
    case AND:
      XEXP (exp, 0) = check_attr_value (XEXP (exp, 0), attr);
      XEXP (exp, 1) = check_attr_value (XEXP (exp, 1), attr);
      break;

    case FFS:
    case CLZ:
    case CTZ:
    case POPCOUNT:
    case PARITY:
      XEXP (exp, 0) = check_attr_value (XEXP (exp, 0), attr);
      break;

    case COND:
      if (XVECLEN (exp, 0) % 2 != 0)
	{
	  message_with_line (attr->lineno,
			     "first operand of COND must have even length");
	  have_error = 1;
	  break;
	}

      for (i = 0; i < XVECLEN (exp, 0); i += 2)
	{
	  XVECEXP (exp, 0, i) = check_attr_test (XVECEXP (exp, 0, i),
						 attr ? attr->is_const : 0,
						 attr ? attr->lineno : 0);
	  XVECEXP (exp, 0, i + 1)
	    = check_attr_value (XVECEXP (exp, 0, i + 1), attr);
	}

      XEXP (exp, 1) = check_attr_value (XEXP (exp, 1), attr);
      break;

    case ATTR:
      {
	struct attr_desc *attr2 = find_attr (&XSTR (exp, 0), 0);
	if (attr2 == NULL)
	  {
	    message_with_line (attr ? attr->lineno : 0,
			       "unknown attribute `%s' in ATTR",
			       XSTR (exp, 0));
	    have_error = 1;
	  }
	else if (attr && attr->is_const && ! attr2->is_const)
	  {
	    message_with_line (attr->lineno,
		"non-constant attribute `%s' referenced from `%s'",
		XSTR (exp, 0), attr->name);
	    have_error = 1;
	  }
	else if (attr
		 && (attr->is_numeric != attr2->is_numeric
		     || (! attr->negative_ok && attr2->negative_ok)))
	  {
	    message_with_line (attr->lineno,
		"numeric attribute mismatch calling `%s' from `%s'",
		XSTR (exp, 0), attr->name);
	    have_error = 1;
	  }
      }
      break;

    case SYMBOL_REF:
      /* A constant SYMBOL_REF is valid as a constant attribute test and
         is expanded later by make_canonical into a COND.  In a non-constant
         attribute test, it is left be.  */
      return attr_rtx (SYMBOL_REF, XSTR (exp, 0));

    default:
      message_with_line (attr ? attr->lineno : 0,
			 "invalid operation `%s' for attribute value",
			 GET_RTX_NAME (GET_CODE (exp)));
      have_error = 1;
      break;
    }

  return exp;
}

/* Given an SET_ATTR_ALTERNATIVE expression, convert to the canonical SET.
   It becomes a COND with each test being (eq_attr "alternative "n") */

static rtx
convert_set_attr_alternative (rtx exp, struct insn_def *id)
{
  int num_alt = id->num_alternatives;
  rtx condexp;
  int i;

  if (XVECLEN (exp, 1) != num_alt)
    {
      message_with_line (id->lineno,
			 "bad number of entries in SET_ATTR_ALTERNATIVE");
      have_error = 1;
      return NULL_RTX;
    }

  /* Make a COND with all tests but the last.  Select the last value via the
     default.  */
  condexp = rtx_alloc (COND);
  XVEC (condexp, 0) = rtvec_alloc ((num_alt - 1) * 2);

  for (i = 0; i < num_alt - 1; i++)
    {
      const char *p;
      p = attr_numeral (i);

      XVECEXP (condexp, 0, 2 * i) = attr_eq (alternative_name, p);
      XVECEXP (condexp, 0, 2 * i + 1) = XVECEXP (exp, 1, i);
    }

  XEXP (condexp, 1) = XVECEXP (exp, 1, i);

  return attr_rtx (SET, attr_rtx (ATTR, XSTR (exp, 0)), condexp);
}

/* Given a SET_ATTR, convert to the appropriate SET.  If a comma-separated
   list of values is given, convert to SET_ATTR_ALTERNATIVE first.  */

static rtx
convert_set_attr (rtx exp, struct insn_def *id)
{
  rtx newexp;
  const char *name_ptr;
  char *p;
  int n;

  /* See how many alternative specified.  */
  n = n_comma_elts (XSTR (exp, 1));
  if (n == 1)
    return attr_rtx (SET,
		     attr_rtx (ATTR, XSTR (exp, 0)),
		     attr_rtx (CONST_STRING, XSTR (exp, 1)));

  newexp = rtx_alloc (SET_ATTR_ALTERNATIVE);
  XSTR (newexp, 0) = XSTR (exp, 0);
  XVEC (newexp, 1) = rtvec_alloc (n);

  /* Process each comma-separated name.  */
  name_ptr = XSTR (exp, 1);
  n = 0;
  while ((p = next_comma_elt (&name_ptr)) != NULL)
    XVECEXP (newexp, 1, n++) = attr_rtx (CONST_STRING, p);

  return convert_set_attr_alternative (newexp, id);
}

/* Scan all definitions, checking for validity.  Also, convert any SET_ATTR
   and SET_ATTR_ALTERNATIVE expressions to the corresponding SET
   expressions.  */

static void
check_defs (void)
{
  struct insn_def *id;
  struct attr_desc *attr;
  int i;
  rtx value;

  for (id = defs; id; id = id->next)
    {
      if (XVEC (id->def, id->vec_idx) == NULL)
	continue;

      for (i = 0; i < XVECLEN (id->def, id->vec_idx); i++)
	{
	  value = XVECEXP (id->def, id->vec_idx, i);
	  switch (GET_CODE (value))
	    {
	    case SET:
	      if (GET_CODE (XEXP (value, 0)) != ATTR)
		{
		  message_with_line (id->lineno, "bad attribute set");
		  have_error = 1;
		  value = NULL_RTX;
		}
	      break;

	    case SET_ATTR_ALTERNATIVE:
	      value = convert_set_attr_alternative (value, id);
	      break;

	    case SET_ATTR:
	      value = convert_set_attr (value, id);
	      break;

	    default:
	      message_with_line (id->lineno, "invalid attribute code %s",
				 GET_RTX_NAME (GET_CODE (value)));
	      have_error = 1;
	      value = NULL_RTX;
	    }
	  if (value == NULL_RTX)
	    continue;

	  if ((attr = find_attr (&XSTR (XEXP (value, 0), 0), 0)) == NULL)
	    {
	      message_with_line (id->lineno, "unknown attribute %s",
				 XSTR (XEXP (value, 0), 0));
	      have_error = 1;
	      continue;
	    }

	  XVECEXP (id->def, id->vec_idx, i) = value;
	  XEXP (value, 1) = check_attr_value (XEXP (value, 1), attr);
	}
    }
}

/* Given a valid expression for an attribute value, remove any IF_THEN_ELSE
   expressions by converting them into a COND.  This removes cases from this
   program.  Also, replace an attribute value of "*" with the default attribute
   value.  */

static rtx
make_canonical (struct attr_desc *attr, rtx exp)
{
  int i;
  rtx newexp;

  switch (GET_CODE (exp))
    {
    case CONST_INT:
      exp = make_numeric_value (INTVAL (exp));
      break;

    case CONST_STRING:
      if (! strcmp (XSTR (exp, 0), "*"))
	{
	  if (attr == 0 || attr->default_val == 0)
	    fatal ("(attr_value \"*\") used in invalid context");
	  exp = attr->default_val->value;
	}
      else
	XSTR (exp, 0) = DEF_ATTR_STRING (XSTR (exp, 0));

      break;

    case SYMBOL_REF:
      if (!attr->is_const || ATTR_IND_SIMPLIFIED_P (exp))
	break;
      /* The SYMBOL_REF is constant for a given run, so mark it as unchanging.
	 This makes the COND something that won't be considered an arbitrary
	 expression by walk_attr_value.  */
      ATTR_IND_SIMPLIFIED_P (exp) = 1;
      exp = check_attr_value (exp, attr);
      break;

    case IF_THEN_ELSE:
      newexp = rtx_alloc (COND);
      XVEC (newexp, 0) = rtvec_alloc (2);
      XVECEXP (newexp, 0, 0) = XEXP (exp, 0);
      XVECEXP (newexp, 0, 1) = XEXP (exp, 1);

      XEXP (newexp, 1) = XEXP (exp, 2);

      exp = newexp;
      /* Fall through to COND case since this is now a COND.  */

    case COND:
      {
	int allsame = 1;
	rtx defval;

	/* First, check for degenerate COND.  */
	if (XVECLEN (exp, 0) == 0)
	  return make_canonical (attr, XEXP (exp, 1));
	defval = XEXP (exp, 1) = make_canonical (attr, XEXP (exp, 1));

	for (i = 0; i < XVECLEN (exp, 0); i += 2)
	  {
	    XVECEXP (exp, 0, i) = copy_boolean (XVECEXP (exp, 0, i));
	    XVECEXP (exp, 0, i + 1)
	      = make_canonical (attr, XVECEXP (exp, 0, i + 1));
	    if (! rtx_equal_p (XVECEXP (exp, 0, i + 1), defval))
	      allsame = 0;
	  }
	if (allsame)
	  return defval;
      }
      break;

    default:
      break;
    }

  return exp;
}

static rtx
copy_boolean (rtx exp)
{
  if (GET_CODE (exp) == AND || GET_CODE (exp) == IOR)
    return attr_rtx (GET_CODE (exp), copy_boolean (XEXP (exp, 0)),
		     copy_boolean (XEXP (exp, 1)));
  if (GET_CODE (exp) == MATCH_OPERAND)
    {
      XSTR (exp, 1) = DEF_ATTR_STRING (XSTR (exp, 1));
      XSTR (exp, 2) = DEF_ATTR_STRING (XSTR (exp, 2));
    }
  else if (GET_CODE (exp) == EQ_ATTR)
    {
      XSTR (exp, 0) = DEF_ATTR_STRING (XSTR (exp, 0));
      XSTR (exp, 1) = DEF_ATTR_STRING (XSTR (exp, 1));
    }

  return exp;
}

/* Given a value and an attribute description, return a `struct attr_value *'
   that represents that value.  This is either an existing structure, if the
   value has been previously encountered, or a newly-created structure.

   `insn_code' is the code of an insn whose attribute has the specified
   value (-2 if not processing an insn).  We ensure that all insns for
   a given value have the same number of alternatives if the value checks
   alternatives.  */

static struct attr_value *
get_attr_value (rtx value, struct attr_desc *attr, int insn_code)
{
  struct attr_value *av;
  int num_alt = 0;

  value = make_canonical (attr, value);
  if (compares_alternatives_p (value))
    {
      if (insn_code < 0 || insn_alternatives == NULL)
	fatal ("(eq_attr \"alternatives\" ...) used in non-insn context");
      else
	num_alt = insn_alternatives[insn_code];
    }

  for (av = attr->first_value; av; av = av->next)
    if (rtx_equal_p (value, av->value)
	&& (num_alt == 0 || av->first_insn == NULL
	    || insn_alternatives[av->first_insn->insn_code]))
      return av;

  av = oballoc (sizeof (struct attr_value));
  av->value = value;
  av->next = attr->first_value;
  attr->first_value = av;
  av->first_insn = NULL;
  av->num_insns = 0;
  av->has_asm_insn = 0;

  return av;
}

/* After all DEFINE_DELAYs have been read in, create internal attributes
   to generate the required routines.

   First, we compute the number of delay slots for each insn (as a COND of
   each of the test expressions in DEFINE_DELAYs).  Then, if more than one
   delay type is specified, we compute a similar function giving the
   DEFINE_DELAY ordinal for each insn.

   Finally, for each [DEFINE_DELAY, slot #] pair, we compute an attribute that
   tells whether a given insn can be in that delay slot.

   Normal attribute filling and optimization expands these to contain the
   information needed to handle delay slots.  */

static void
expand_delays (void)
{
  struct delay_desc *delay;
  rtx condexp;
  rtx newexp;
  int i;
  char *p;

  /* First, generate data for `num_delay_slots' function.  */

  condexp = rtx_alloc (COND);
  XVEC (condexp, 0) = rtvec_alloc (num_delays * 2);
  XEXP (condexp, 1) = make_numeric_value (0);

  for (i = 0, delay = delays; delay; i += 2, delay = delay->next)
    {
      XVECEXP (condexp, 0, i) = XEXP (delay->def, 0);
      XVECEXP (condexp, 0, i + 1)
	= make_numeric_value (XVECLEN (delay->def, 1) / 3);
    }

  make_internal_attr (num_delay_slots_str, condexp, ATTR_NONE);

  /* If more than one delay type, do the same for computing the delay type.  */
  if (num_delays > 1)
    {
      condexp = rtx_alloc (COND);
      XVEC (condexp, 0) = rtvec_alloc (num_delays * 2);
      XEXP (condexp, 1) = make_numeric_value (0);

      for (i = 0, delay = delays; delay; i += 2, delay = delay->next)
	{
	  XVECEXP (condexp, 0, i) = XEXP (delay->def, 0);
	  XVECEXP (condexp, 0, i + 1) = make_numeric_value (delay->num);
	}

      make_internal_attr (delay_type_str, condexp, ATTR_SPECIAL);
    }

  /* For each delay possibility and delay slot, compute an eligibility
     attribute for non-annulled insns and for each type of annulled (annul
     if true and annul if false).  */
  for (delay = delays; delay; delay = delay->next)
    {
      for (i = 0; i < XVECLEN (delay->def, 1); i += 3)
	{
	  condexp = XVECEXP (delay->def, 1, i);
	  if (condexp == 0)
	    condexp = false_rtx;
	  newexp = attr_rtx (IF_THEN_ELSE, condexp,
			     make_numeric_value (1), make_numeric_value (0));

	  p = attr_printf (sizeof "*delay__" + MAX_DIGITS * 2,
			   "*delay_%d_%d", delay->num, i / 3);
	  make_internal_attr (p, newexp, ATTR_SPECIAL);

	  if (have_annul_true)
	    {
	      condexp = XVECEXP (delay->def, 1, i + 1);
	      if (condexp == 0) condexp = false_rtx;
	      newexp = attr_rtx (IF_THEN_ELSE, condexp,
				 make_numeric_value (1),
				 make_numeric_value (0));
	      p = attr_printf (sizeof "*annul_true__" + MAX_DIGITS * 2,
			       "*annul_true_%d_%d", delay->num, i / 3);
	      make_internal_attr (p, newexp, ATTR_SPECIAL);
	    }

	  if (have_annul_false)
	    {
	      condexp = XVECEXP (delay->def, 1, i + 2);
	      if (condexp == 0) condexp = false_rtx;
	      newexp = attr_rtx (IF_THEN_ELSE, condexp,
				 make_numeric_value (1),
				 make_numeric_value (0));
	      p = attr_printf (sizeof "*annul_false__" + MAX_DIGITS * 2,
			       "*annul_false_%d_%d", delay->num, i / 3);
	      make_internal_attr (p, newexp, ATTR_SPECIAL);
	    }
	}
    }
}

/* This function is given a left and right side expression and an operator.
   Each side is a conditional expression, each alternative of which has a
   numerical value.  The function returns another conditional expression
   which, for every possible set of condition values, returns a value that is
   the operator applied to the values of the two sides.

   Since this is called early, it must also support IF_THEN_ELSE.  */

static rtx
operate_exp (enum operator op, rtx left, rtx right)
{
  int left_value, right_value;
  rtx newexp;
  int i;

  /* If left is a string, apply operator to it and the right side.  */
  if (GET_CODE (left) == CONST_STRING)
    {
      /* If right is also a string, just perform the operation.  */
      if (GET_CODE (right) == CONST_STRING)
	{
	  left_value = atoi (XSTR (left, 0));
	  right_value = atoi (XSTR (right, 0));
	  switch (op)
	    {
	    case PLUS_OP:
	      i = left_value + right_value;
	      break;

	    case MINUS_OP:
	      i = left_value - right_value;
	      break;

	    case POS_MINUS_OP:  /* The positive part of LEFT - RIGHT.  */
	      if (left_value > right_value)
		i = left_value - right_value;
	      else
		i = 0;
	      break;

	    case OR_OP:
	    case ORX_OP:
	      i = left_value | right_value;
	      break;

	    case EQ_OP:
	      i = left_value == right_value;
	      break;

	    case RANGE_OP:
	      i = (left_value << (HOST_BITS_PER_INT / 2)) | right_value;
	      break;

	    case MAX_OP:
	      if (left_value > right_value)
		i = left_value;
	      else
		i = right_value;
	      break;

	    case MIN_OP:
	      if (left_value < right_value)
		i = left_value;
	      else
		i = right_value;
	      break;

	    default:
	      abort ();
	    }

	  if (i == left_value)
	    return left;
	  if (i == right_value)
	    return right;
	  return make_numeric_value (i);
	}
      else if (GET_CODE (right) == IF_THEN_ELSE)
	{
	  /* Apply recursively to all values within.  */
	  rtx newleft = operate_exp (op, left, XEXP (right, 1));
	  rtx newright = operate_exp (op, left, XEXP (right, 2));
	  if (rtx_equal_p (newleft, newright))
	    return newleft;
	  return attr_rtx (IF_THEN_ELSE, XEXP (right, 0), newleft, newright);
	}
      else if (GET_CODE (right) == COND)
	{
	  int allsame = 1;
	  rtx defval;

	  newexp = rtx_alloc (COND);
	  XVEC (newexp, 0) = rtvec_alloc (XVECLEN (right, 0));
	  defval = XEXP (newexp, 1) = operate_exp (op, left, XEXP (right, 1));

	  for (i = 0; i < XVECLEN (right, 0); i += 2)
	    {
	      XVECEXP (newexp, 0, i) = XVECEXP (right, 0, i);
	      XVECEXP (newexp, 0, i + 1)
		= operate_exp (op, left, XVECEXP (right, 0, i + 1));
	      if (! rtx_equal_p (XVECEXP (newexp, 0, i + 1),
				 defval))
		allsame = 0;
	    }

	  /* If the resulting cond is trivial (all alternatives
	     give the same value), optimize it away.  */
	  if (allsame)
	    return operate_exp (op, left, XEXP (right, 1));

	  return newexp;
	}
      else
	fatal ("badly formed attribute value");
    }

  /* A hack to prevent expand_units from completely blowing up: ORX_OP does
     not associate through IF_THEN_ELSE.  */
  else if (op == ORX_OP && GET_CODE (right) == IF_THEN_ELSE)
    {
      return attr_rtx (IOR, left, right);
    }

  /* Otherwise, do recursion the other way.  */
  else if (GET_CODE (left) == IF_THEN_ELSE)
    {
      rtx newleft = operate_exp (op, XEXP (left, 1), right);
      rtx newright = operate_exp (op, XEXP (left, 2), right);
      if (rtx_equal_p (newleft, newright))
	return newleft;
      return attr_rtx (IF_THEN_ELSE, XEXP (left, 0), newleft, newright);
    }
  else if (GET_CODE (left) == COND)
    {
      int allsame = 1;
      rtx defval;

      newexp = rtx_alloc (COND);
      XVEC (newexp, 0) = rtvec_alloc (XVECLEN (left, 0));
      defval = XEXP (newexp, 1) = operate_exp (op, XEXP (left, 1), right);

      for (i = 0; i < XVECLEN (left, 0); i += 2)
	{
	  XVECEXP (newexp, 0, i) = XVECEXP (left, 0, i);
	  XVECEXP (newexp, 0, i + 1)
	    = operate_exp (op, XVECEXP (left, 0, i + 1), right);
	  if (! rtx_equal_p (XVECEXP (newexp, 0, i + 1),
			     defval))
	    allsame = 0;
	}

      /* If the cond is trivial (all alternatives give the same value),
	 optimize it away.  */
      if (allsame)
	return operate_exp (op, XEXP (left, 1), right);

      /* If the result is the same as the LEFT operand,
	 just use that.  */
      if (rtx_equal_p (newexp, left))
	return left;

      return newexp;
    }

  else
    fatal ("badly formed attribute value");
  /* NOTREACHED */
  return NULL;
}

/* Once all attributes and DEFINE_FUNCTION_UNITs have been read, we
   construct a number of attributes.

   The first produces a function `function_units_used' which is given an
   insn and produces an encoding showing which function units are required
   for the execution of that insn.  If the value is non-negative, the insn
   uses that unit; otherwise, the value is a one's complement mask of units
   used.

   The second produces a function `result_ready_cost' which is used to
   determine the time that the result of an insn will be ready and hence
   a worst-case schedule.

   Both of these produce quite complex expressions which are then set as the
   default value of internal attributes.  Normal attribute simplification
   should produce reasonable expressions.

   For each unit, a `<name>_unit_ready_cost' function will take an
   insn and give the delay until that unit will be ready with the result
   and a `<name>_unit_conflict_cost' function is given an insn already
   executing on the unit and a candidate to execute and will give the
   cost from the time the executing insn started until the candidate
   can start (ignore limitations on the number of simultaneous insns).

   For each unit, a `<name>_unit_blockage' function is given an insn
   already executing on the unit and a candidate to execute and will
   give the delay incurred due to function unit conflicts.  The range of
   blockage cost values for a given executing insn is given by the
   `<name>_unit_blockage_range' function.  These values are encoded in
   an int where the upper half gives the minimum value and the lower
   half gives the maximum value.  */

static void
expand_units (void)
{
  struct function_unit *unit, **unit_num;
  struct function_unit_op *op, **op_array, ***unit_ops;
  rtx unitsmask;
  rtx readycost;
  rtx newexp;
  const char *str;
  int i, j, u, num, nvalues;

  /* Rebuild the condition for the unit to share the RTL expressions.
     Sharing is required by simplify_by_exploding.  Build the issue delay
     expressions.  Validate the expressions we were given for the conditions
     and conflict vector.  Then make attributes for use in the conflict
     function.  */

  for (unit = units; unit; unit = unit->next)
    {
      unit->condexp = check_attr_test (unit->condexp, 0, unit->first_lineno);

      for (op = unit->ops; op; op = op->next)
	{
	  rtx issue_delay = make_numeric_value (op->issue_delay);
	  rtx issue_exp = issue_delay;

	  /* Build, validate, and simplify the issue delay expression.  */
	  if (op->conflict_exp != true_rtx)
	    issue_exp = attr_rtx (IF_THEN_ELSE, op->conflict_exp,
				  issue_exp, make_numeric_value (0));
	  issue_exp = check_attr_value (make_canonical (NULL_ATTR,
							issue_exp),
					NULL_ATTR);
	  issue_exp = simplify_knowing (issue_exp, unit->condexp);
	  op->issue_exp = issue_exp;

	  /* Make an attribute for use in the conflict function if needed.  */
	  unit->needs_conflict_function = (unit->issue_delay.min
					   != unit->issue_delay.max);
	  if (unit->needs_conflict_function)
	    {
	      str = attr_printf ((strlen (unit->name) + sizeof "*_cost_"
				  + MAX_DIGITS),
				 "*%s_cost_%d", unit->name, op->num);
	      make_internal_attr (str, issue_exp, ATTR_SPECIAL);
	    }

	  /* Validate the condition.  */
	  op->condexp = check_attr_test (op->condexp, 0, op->lineno);
	}
    }

  /* Compute the mask of function units used.  Initially, the unitsmask is
     zero.   Set up a conditional to compute each unit's contribution.  */
  unitsmask = make_numeric_value (0);
  newexp = rtx_alloc (IF_THEN_ELSE);
  XEXP (newexp, 2) = make_numeric_value (0);

  /* If we have just a few units, we may be all right expanding the whole
     thing.  But the expansion is 2**N in space on the number of opclasses,
     so we can't do this for very long -- Alpha and MIPS in particular have
     problems with this.  So in that situation, we fall back on an alternate
     implementation method.  */
#define NUM_UNITOP_CUTOFF 20

  if (num_unit_opclasses < NUM_UNITOP_CUTOFF)
    {
      /* Merge each function unit into the unit mask attributes.  */
      for (unit = units; unit; unit = unit->next)
	{
	  XEXP (newexp, 0) = unit->condexp;
	  XEXP (newexp, 1) = make_numeric_value (1 << unit->num);
	  unitsmask = operate_exp (OR_OP, unitsmask, newexp);
	}
    }
  else
    {
      /* Merge each function unit into the unit mask attributes.  */
      for (unit = units; unit; unit = unit->next)
	{
	  XEXP (newexp, 0) = unit->condexp;
	  XEXP (newexp, 1) = make_numeric_value (1 << unit->num);
	  unitsmask = operate_exp (ORX_OP, unitsmask, attr_copy_rtx (newexp));
	}
    }

  /* Simplify the unit mask expression, encode it, and make an attribute
     for the function_units_used function.  */
  unitsmask = simplify_by_exploding (unitsmask);

  if (num_unit_opclasses < NUM_UNITOP_CUTOFF)
    unitsmask = encode_units_mask (unitsmask);
  else
    {
      /* We can no longer encode unitsmask at compile time, so emit code to
         calculate it at runtime.  Rather, put a marker for where we'd do
	 the code, and actually output it in write_attr_get().  */
      unitsmask = attr_rtx (FFS, unitsmask);
    }

  make_internal_attr ("*function_units_used", unitsmask,
		      (ATTR_NEGATIVE_OK | ATTR_FUNC_UNITS));

  /* Create an array of ops for each unit.  Add an extra unit for the
     result_ready_cost function that has the ops of all other units.  */
  unit_ops = xmalloc ((num_units + 1) * sizeof (struct function_unit_op **));
  unit_num = xmalloc ((num_units + 1) * sizeof (struct function_unit *));

  unit_num[num_units] = unit = xmalloc (sizeof (struct function_unit));
  unit->num = num_units;
  unit->num_opclasses = 0;

  for (unit = units; unit; unit = unit->next)
    {
      unit_num[num_units]->num_opclasses += unit->num_opclasses;
      unit_num[unit->num] = unit;
      unit_ops[unit->num] = op_array =
	xmalloc (unit->num_opclasses * sizeof (struct function_unit_op *));

      for (op = unit->ops; op; op = op->next)
	op_array[op->num] = op;
    }

  /* Compose the array of ops for the extra unit.  */
  unit_ops[num_units] = op_array =
    xmalloc (unit_num[num_units]->num_opclasses
	    * sizeof (struct function_unit_op *));

  for (unit = units, i = 0; unit; i += unit->num_opclasses, unit = unit->next)
    memcpy (&op_array[i], unit_ops[unit->num],
	    unit->num_opclasses * sizeof (struct function_unit_op *));

  /* Compute the ready cost function for each unit by computing the
     condition for each non-default value.  */
  for (u = 0; u <= num_units; u++)
    {
      rtx orexp;
      int value;

      unit = unit_num[u];
      op_array = unit_ops[unit->num];
      num = unit->num_opclasses;

      /* Sort the array of ops into increasing ready cost order.  */
      for (i = 0; i < num; i++)
	for (j = num - 1; j > i; j--)
	  if (op_array[j - 1]->ready < op_array[j]->ready)
	    {
	      op = op_array[j];
	      op_array[j] = op_array[j - 1];
	      op_array[j - 1] = op;
	    }

      /* Determine how many distinct non-default ready cost values there
	 are.  We use a default ready cost value of 1.  */
      nvalues = 0; value = 1;
      for (i = num - 1; i >= 0; i--)
	if (op_array[i]->ready > value)
	  {
	    value = op_array[i]->ready;
	    nvalues++;
	  }

      if (nvalues == 0)
	readycost = make_numeric_value (1);
      else
	{
	  /* Construct the ready cost expression as a COND of each value from
	     the largest to the smallest.  */
	  readycost = rtx_alloc (COND);
	  XVEC (readycost, 0) = rtvec_alloc (nvalues * 2);
	  XEXP (readycost, 1) = make_numeric_value (1);

	  nvalues = 0;
	  orexp = false_rtx;
	  value = op_array[0]->ready;
	  for (i = 0; i < num; i++)
	    {
	      op = op_array[i];
	      if (op->ready <= 1)
		break;
	      else if (op->ready == value)
		orexp = insert_right_side (IOR, orexp, op->condexp, -2, -2);
	      else
		{
		  XVECEXP (readycost, 0, nvalues * 2) = orexp;
		  XVECEXP (readycost, 0, nvalues * 2 + 1)
		    = make_numeric_value (value);
		  nvalues++;
		  value = op->ready;
		  orexp = op->condexp;
		}
	    }
	  XVECEXP (readycost, 0, nvalues * 2) = orexp;
	  XVECEXP (readycost, 0, nvalues * 2 + 1) = make_numeric_value (value);
	}

      if (u < num_units)
	{
	  rtx max_blockage = 0, min_blockage = 0;

	  /* Simplify the readycost expression by only considering insns
	     that use the unit.  */
	  readycost = simplify_knowing (readycost, unit->condexp);

	  /* Determine the blockage cost the executing insn (E) given
	     the candidate insn (C).  This is the maximum of the issue
	     delay, the pipeline delay, and the simultaneity constraint.
	     Each function_unit_op represents the characteristics of the
	     candidate insn, so in the expressions below, C is a known
	     term and E is an unknown term.

	     We compute the blockage cost for each E for every possible C.
	     Thus OP represents E, and READYCOST is a list of values for
	     every possible C.

	     The issue delay function for C is op->issue_exp and is used to
	     write the `<name>_unit_conflict_cost' function.  Symbolically
	     this is "ISSUE-DELAY (E,C)".

	     The pipeline delay results form the FIFO constraint on the
	     function unit and is "READY-COST (E) + 1 - READY-COST (C)".

	     The simultaneity constraint is based on how long it takes to
	     fill the unit given the minimum issue delay.  FILL-TIME is the
	     constant "MIN (ISSUE-DELAY (*,*)) * (SIMULTANEITY - 1)", and
	     the simultaneity constraint is "READY-COST (E) - FILL-TIME"
	     if SIMULTANEITY is nonzero and zero otherwise.

	     Thus, BLOCKAGE (E,C) when SIMULTANEITY is zero is

	         MAX (ISSUE-DELAY (E,C),
		      READY-COST (E) - (READY-COST (C) - 1))

	     and otherwise

	         MAX (ISSUE-DELAY (E,C),
		      READY-COST (E) - (READY-COST (C) - 1),
		      READY-COST (E) - FILL-TIME)

	     The `<name>_unit_blockage' function is computed by determining
	     this value for each candidate insn.  As these values are
	     computed, we also compute the upper and lower bounds for
	     BLOCKAGE (E,*).  These are combined to form the function
	     `<name>_unit_blockage_range'.  Finally, the maximum blockage
	     cost, MAX (BLOCKAGE (*,*)), is computed.  */

	  for (op = unit->ops; op; op = op->next)
	    {
	      rtx blockage = op->issue_exp;
	      blockage = simplify_knowing (blockage, unit->condexp);

	      /* Add this op's contribution to MAX (BLOCKAGE (E,*)) and
		 MIN (BLOCKAGE (E,*)).  */
	      if (max_blockage == 0)
		max_blockage = min_blockage = blockage;
	      else
		{
		  max_blockage
		    = simplify_knowing (operate_exp (MAX_OP, max_blockage,
						     blockage),
					unit->condexp);
		  min_blockage
		    = simplify_knowing (operate_exp (MIN_OP, min_blockage,
						     blockage),
					unit->condexp);
		}

	      /* Make an attribute for use in the blockage function.  */
	      str = attr_printf ((strlen (unit->name) + sizeof "*_block_"
				  + MAX_DIGITS),
				 "*%s_block_%d", unit->name, op->num);
	      make_internal_attr (str, blockage, ATTR_SPECIAL);
	    }

	  /* Record MAX (BLOCKAGE (*,*)).  */
	  {
	    int unknown;
	    unit->max_blockage = max_attr_value (max_blockage, &unknown);
	  }

	  /* See if the upper and lower bounds of BLOCKAGE (E,*) are the
	     same.  If so, the blockage function carries no additional
	     information and is not written.  */
	  newexp = operate_exp (EQ_OP, max_blockage, min_blockage);
	  newexp = simplify_knowing (newexp, unit->condexp);
	  unit->needs_blockage_function
	    = (GET_CODE (newexp) != CONST_STRING
	       || atoi (XSTR (newexp, 0)) != 1);

	  /* If the all values of BLOCKAGE (E,C) have the same value,
	     neither blockage function is written.  */
	  unit->needs_range_function
	    = (unit->needs_blockage_function
	       || GET_CODE (max_blockage) != CONST_STRING);

	  if (unit->needs_range_function)
	    {
	      /* Compute the blockage range function and make an attribute
		 for writing its value.  */
	      newexp = operate_exp (RANGE_OP, min_blockage, max_blockage);
	      newexp = simplify_knowing (newexp, unit->condexp);

	      str = attr_printf ((strlen (unit->name)
				  + sizeof "*_unit_blockage_range"),
				 "*%s_unit_blockage_range", unit->name);
	      make_internal_attr (str, newexp, (ATTR_STATIC|ATTR_BLOCKAGE|ATTR_UNSIGNED));
	    }

	  str = attr_printf (strlen (unit->name) + sizeof "*_unit_ready_cost",
			     "*%s_unit_ready_cost", unit->name);
	  make_internal_attr (str, readycost, ATTR_STATIC);
	}
      else
        {
	  /* Make an attribute for the ready_cost function.  Simplifying
	     further with simplify_by_exploding doesn't win.  */
	  str = "*result_ready_cost";
	  make_internal_attr (str, readycost, ATTR_NONE);
	}
    }

  /* For each unit that requires a conflict cost function, make an attribute
     that maps insns to the operation number.  */
  for (unit = units; unit; unit = unit->next)
    {
      rtx caseexp;

      if (! unit->needs_conflict_function
	  && ! unit->needs_blockage_function)
	continue;

      caseexp = rtx_alloc (COND);
      XVEC (caseexp, 0) = rtvec_alloc ((unit->num_opclasses - 1) * 2);

      for (op = unit->ops; op; op = op->next)
	{
	  /* Make our adjustment to the COND being computed.  If we are the
	     last operation class, place our values into the default of the
	     COND.  */
	  if (op->num == unit->num_opclasses - 1)
	    {
	      XEXP (caseexp, 1) = make_numeric_value (op->num);
	    }
	  else
	    {
	      XVECEXP (caseexp, 0, op->num * 2) = op->condexp;
	      XVECEXP (caseexp, 0, op->num * 2 + 1)
		= make_numeric_value (op->num);
	    }
	}

      /* Simplifying caseexp with simplify_by_exploding doesn't win.  */
      str = attr_printf (strlen (unit->name) + sizeof "*_cases",
			 "*%s_cases", unit->name);
      make_internal_attr (str, caseexp, ATTR_SPECIAL);
    }
}

/* Simplify EXP given KNOWN_TRUE.  */

static rtx
simplify_knowing (rtx exp, rtx known_true)
{
  if (GET_CODE (exp) != CONST_STRING)
    {
      int unknown = 0, max;
      max = max_attr_value (exp, &unknown);
      if (! unknown)
	{
	  exp = attr_rtx (IF_THEN_ELSE, known_true, exp,
			  make_numeric_value (max));
	  exp = simplify_by_exploding (exp);
	}
    }
  return exp;
}

/* Translate the CONST_STRING expressions in X to change the encoding of
   value.  On input, the value is a bitmask with a one bit for each unit
   used; on output, the value is the unit number (zero based) if one
   and only one unit is used or the one's complement of the bitmask.  */

static rtx
encode_units_mask (rtx x)
{
  int i;
  int j;
  enum rtx_code code;
  const char *fmt;

  code = GET_CODE (x);

  switch (code)
    {
    case CONST_STRING:
      i = atoi (XSTR (x, 0));
      if (i < 0)
	/* The sign bit encodes a one's complement mask.  */
	abort ();
      else if (i != 0 && i == (i & -i))
	/* Only one bit is set, so yield that unit number.  */
	for (j = 0; (i >>= 1) != 0; j++)
	  ;
      else
	j = ~i;
      return attr_rtx (CONST_STRING, attr_printf (MAX_DIGITS, "%d", j));

    case REG:
    case QUEUED:
    case CONST_INT:
    case CONST_DOUBLE:
    case CONST_VECTOR:
    case SYMBOL_REF:
    case CODE_LABEL:
    case PC:
    case CC0:
    case EQ_ATTR:
    case EQ_ATTR_ALT:
      return x;

    default:
      break;
    }

  /* Compare the elements.  If any pair of corresponding elements
     fail to match, return 0 for the whole things.  */

  fmt = GET_RTX_FORMAT (code);
  for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
    {
      switch (fmt[i])
	{
	case 'V':
	case 'E':
	  for (j = 0; j < XVECLEN (x, i); j++)
	    XVECEXP (x, i, j) = encode_units_mask (XVECEXP (x, i, j));
	  break;

	case 'e':
	  XEXP (x, i) = encode_units_mask (XEXP (x, i));
	  break;
	}
    }
  return x;
}

/* Once all attributes and insns have been read and checked, we construct for
   each attribute value a list of all the insns that have that value for
   the attribute.  */

static void
fill_attr (struct attr_desc *attr)
{
  struct attr_value *av;
  struct insn_ent *ie;
  struct insn_def *id;
  int i;
  rtx value;

  /* Don't fill constant attributes.  The value is independent of
     any particular insn.  */
  if (attr->is_const)
    return;

  for (id = defs; id; id = id->next)
    {
      /* If no value is specified for this insn for this attribute, use the
	 default.  */
      value = NULL;
      if (XVEC (id->def, id->vec_idx))
	for (i = 0; i < XVECLEN (id->def, id->vec_idx); i++)
	  if (! strcmp_check (XSTR (XEXP (XVECEXP (id->def, id->vec_idx, i), 0), 0),
			      attr->name))
	    value = XEXP (XVECEXP (id->def, id->vec_idx, i), 1);

      if (value == NULL)
	av = attr->default_val;
      else
	av = get_attr_value (value, attr, id->insn_code);

      ie = oballoc (sizeof (struct insn_ent));
      ie->insn_code = id->insn_code;
      ie->insn_index = id->insn_code;
      insert_insn_ent (av, ie);
    }
}

/* Given an expression EXP, see if it is a COND or IF_THEN_ELSE that has a
   test that checks relative positions of insns (uses MATCH_DUP or PC).
   If so, replace it with what is obtained by passing the expression to
   ADDRESS_FN.  If not but it is a COND or IF_THEN_ELSE, call this routine
   recursively on each value (including the default value).  Otherwise,
   return the value returned by NO_ADDRESS_FN applied to EXP.  */

static rtx
substitute_address (rtx exp, rtx (*no_address_fn) (rtx),
		    rtx (*address_fn) (rtx))
{
  int i;
  rtx newexp;

  if (GET_CODE (exp) == COND)
    {
      /* See if any tests use addresses.  */
      address_used = 0;
      for (i = 0; i < XVECLEN (exp, 0); i += 2)
	walk_attr_value (XVECEXP (exp, 0, i));

      if (address_used)
	return (*address_fn) (exp);

      /* Make a new copy of this COND, replacing each element.  */
      newexp = rtx_alloc (COND);
      XVEC (newexp, 0) = rtvec_alloc (XVECLEN (exp, 0));
      for (i = 0; i < XVECLEN (exp, 0); i += 2)
	{
	  XVECEXP (newexp, 0, i) = XVECEXP (exp, 0, i);
	  XVECEXP (newexp, 0, i + 1)
	    = substitute_address (XVECEXP (exp, 0, i + 1),
				  no_address_fn, address_fn);
	}

      XEXP (newexp, 1) = substitute_address (XEXP (exp, 1),
					     no_address_fn, address_fn);

      return newexp;
    }

  else if (GET_CODE (exp) == IF_THEN_ELSE)
    {
      address_used = 0;
      walk_attr_value (XEXP (exp, 0));
      if (address_used)
	return (*address_fn) (exp);

      return attr_rtx (IF_THEN_ELSE,
		       substitute_address (XEXP (exp, 0),
					   no_address_fn, address_fn),
		       substitute_address (XEXP (exp, 1),
					   no_address_fn, address_fn),
		       substitute_address (XEXP (exp, 2),
					   no_address_fn, address_fn));
    }

  return (*no_address_fn) (exp);
}

/* Make new attributes from the `length' attribute.  The following are made,
   each corresponding to a function called from `shorten_branches' or
   `get_attr_length':

   *insn_default_length		This is the length of the insn to be returned
				by `get_attr_length' before `shorten_branches'
				has been called.  In each case where the length
				depends on relative addresses, the largest
				possible is used.  This routine is also used
				to compute the initial size of the insn.

   *insn_variable_length_p	This returns 1 if the insn's length depends
				on relative addresses, zero otherwise.

   *insn_current_length		This is only called when it is known that the
				insn has a variable length and returns the
				current length, based on relative addresses.
  */

static void
make_length_attrs (void)
{
  static const char *new_names[] =
    {
      "*insn_default_length",
      "*insn_variable_length_p",
      "*insn_current_length"
    };
  static rtx (*const no_address_fn[]) (rtx) = {identity_fn, zero_fn, zero_fn};
  static rtx (*const address_fn[]) (rtx) = {max_fn, one_fn, identity_fn};
  size_t i;
  struct attr_desc *length_attr, *new_attr;
  struct attr_value *av, *new_av;
  struct insn_ent *ie, *new_ie;

  /* See if length attribute is defined.  If so, it must be numeric.  Make
     it special so we don't output anything for it.  */
  length_attr = find_attr (&length_str, 0);
  if (length_attr == 0)
    return;

  if (! length_attr->is_numeric)
    fatal ("length attribute must be numeric");

  length_attr->is_const = 0;
  length_attr->is_special = 1;

  /* Make each new attribute, in turn.  */
  for (i = 0; i < ARRAY_SIZE (new_names); i++)
    {
      make_internal_attr (new_names[i],
			  substitute_address (length_attr->default_val->value,
					      no_address_fn[i], address_fn[i]),
			  ATTR_NONE);
      new_attr = find_attr (&new_names[i], 0);
      for (av = length_attr->first_value; av; av = av->next)
	for (ie = av->first_insn; ie; ie = ie->next)
	  {
	    new_av = get_attr_value (substitute_address (av->value,
							 no_address_fn[i],
							 address_fn[i]),
				     new_attr, ie->insn_code);
	    new_ie = oballoc (sizeof (struct insn_ent));
	    new_ie->insn_code = ie->insn_code;
	    new_ie->insn_index = ie->insn_index;
	    insert_insn_ent (new_av, new_ie);
	  }
    }
}

/* Utility functions called from above routine.  */

static rtx
identity_fn (rtx exp)
{
  return exp;
}

static rtx
zero_fn (rtx exp ATTRIBUTE_UNUSED)
{
  return make_numeric_value (0);
}

static rtx
one_fn (rtx exp ATTRIBUTE_UNUSED)
{
  return make_numeric_value (1);
}

static rtx
max_fn (rtx exp)
{
  int unknown;
  return make_numeric_value (max_attr_value (exp, &unknown));
}

static void
write_length_unit_log (void)
{
  struct attr_desc *length_attr = find_attr (&length_str, 0);
  struct attr_value *av;
  struct insn_ent *ie;
  unsigned int length_unit_log, length_or;
  int unknown = 0;

  if (length_attr == 0)
    return;
  length_or = or_attr_value (length_attr->default_val->value, &unknown);
  for (av = length_attr->first_value; av; av = av->next)
    for (ie = av->first_insn; ie; ie = ie->next)
      length_or |= or_attr_value (av->value, &unknown);

  if (unknown)
    length_unit_log = 0;
  else
    {
      length_or = ~length_or;
      for (length_unit_log = 0; length_or & 1; length_or >>= 1)
	length_unit_log++;
    }
  printf ("int length_unit_log = %u;\n", length_unit_log);
}

/* Take a COND expression and see if any of the conditions in it can be
   simplified.  If any are known true or known false for the particular insn
   code, the COND can be further simplified.

   Also call ourselves on any COND operations that are values of this COND.

   We do not modify EXP; rather, we make and return a new rtx.  */

static rtx
simplify_cond (rtx exp, int insn_code, int insn_index)
{
  int i, j;
  /* We store the desired contents here,
     then build a new expression if they don't match EXP.  */
  rtx defval = XEXP (exp, 1);
  rtx new_defval = XEXP (exp, 1);
  int len = XVECLEN (exp, 0);
  rtx *tests = xmalloc (len * sizeof (rtx));
  int allsame = 1;
  rtx ret;

  /* This lets us free all storage allocated below, if appropriate.  */
  obstack_finish (rtl_obstack);

  memcpy (tests, XVEC (exp, 0)->elem, len * sizeof (rtx));

  /* See if default value needs simplification.  */
  if (GET_CODE (defval) == COND)
    new_defval = simplify_cond (defval, insn_code, insn_index);

  /* Simplify the subexpressions, and see what tests we can get rid of.  */

  for (i = 0; i < len; i += 2)
    {
      rtx newtest, newval;

      /* Simplify this test.  */
      newtest = simplify_test_exp_in_temp (tests[i], insn_code, insn_index);
      tests[i] = newtest;

      newval = tests[i + 1];
      /* See if this value may need simplification.  */
      if (GET_CODE (newval) == COND)
	newval = simplify_cond (newval, insn_code, insn_index);

      /* Look for ways to delete or combine this test.  */
      if (newtest == true_rtx)
	{
	  /* If test is true, make this value the default
	     and discard this + any following tests.  */
	  len = i;
	  defval = tests[i + 1];
	  new_defval = newval;
	}

      else if (newtest == false_rtx)
	{
	  /* If test is false, discard it and its value.  */
	  for (j = i; j < len - 2; j++)
	    tests[j] = tests[j + 2];
	  i -= 2;
	  len -= 2;
	}

      else if (i > 0 && attr_equal_p (newval, tests[i - 1]))
	{
	  /* If this value and the value for the prev test are the same,
	     merge the tests.  */

	  tests[i - 2]
	    = insert_right_side (IOR, tests[i - 2], newtest,
				 insn_code, insn_index);

	  /* Delete this test/value.  */
	  for (j = i; j < len - 2; j++)
	    tests[j] = tests[j + 2];
	  len -= 2;
	  i -= 2;
	}

      else
	tests[i + 1] = newval;
    }

  /* If the last test in a COND has the same value
     as the default value, that test isn't needed.  */

  while (len > 0 && attr_equal_p (tests[len - 1], new_defval))
    len -= 2;

  /* See if we changed anything.  */
  if (len != XVECLEN (exp, 0) || new_defval != XEXP (exp, 1))
    allsame = 0;
  else
    for (i = 0; i < len; i++)
      if (! attr_equal_p (tests[i], XVECEXP (exp, 0, i)))
	{
	  allsame = 0;
	  break;
	}

  if (len == 0)
    {
      if (GET_CODE (defval) == COND)
	ret = simplify_cond (defval, insn_code, insn_index);
      else
	ret = defval;
    }
  else if (allsame)
    ret = exp;
  else
    {
      rtx newexp = rtx_alloc (COND);

      XVEC (newexp, 0) = rtvec_alloc (len);
      memcpy (XVEC (newexp, 0)->elem, tests, len * sizeof (rtx));
      XEXP (newexp, 1) = new_defval;
      ret = newexp;
    }
  free (tests);
  return ret;
}

/* Remove an insn entry from an attribute value.  */

static void
remove_insn_ent (struct attr_value *av, struct insn_ent *ie)
{
  struct insn_ent *previe;

  if (av->first_insn == ie)
    av->first_insn = ie->next;
  else
    {
      for (previe = av->first_insn; previe->next != ie; previe = previe->next)
	;
      previe->next = ie->next;
    }

  av->num_insns--;
  if (ie->insn_code == -1)
    av->has_asm_insn = 0;

  num_insn_ents--;
}

/* Insert an insn entry in an attribute value list.  */

static void
insert_insn_ent (struct attr_value *av, struct insn_ent *ie)
{
  ie->next = av->first_insn;
  av->first_insn = ie;
  av->num_insns++;
  if (ie->insn_code == -1)
    av->has_asm_insn = 1;

  num_insn_ents++;
}

/* This is a utility routine to take an expression that is a tree of either
   AND or IOR expressions and insert a new term.  The new term will be
   inserted at the right side of the first node whose code does not match
   the root.  A new node will be created with the root's code.  Its left
   side will be the old right side and its right side will be the new
   term.

   If the `term' is itself a tree, all its leaves will be inserted.  */

static rtx
insert_right_side (enum rtx_code code, rtx exp, rtx term, int insn_code, int insn_index)
{
  rtx newexp;

  /* Avoid consing in some special cases.  */
  if (code == AND && term == true_rtx)
    return exp;
  if (code == AND && term == false_rtx)
    return false_rtx;
  if (code == AND && exp == true_rtx)
    return term;
  if (code == AND && exp == false_rtx)
    return false_rtx;
  if (code == IOR && term == true_rtx)
    return true_rtx;
  if (code == IOR && term == false_rtx)
    return exp;
  if (code == IOR && exp == true_rtx)
    return true_rtx;
  if (code == IOR && exp == false_rtx)
    return term;
  if (attr_equal_p (exp, term))
    return exp;

  if (GET_CODE (term) == code)
    {
      exp = insert_right_side (code, exp, XEXP (term, 0),
			       insn_code, insn_index);
      exp = insert_right_side (code, exp, XEXP (term, 1),
			       insn_code, insn_index);

      return exp;
    }

  if (GET_CODE (exp) == code)
    {
      rtx new = insert_right_side (code, XEXP (exp, 1),
				   term, insn_code, insn_index);
      if (new != XEXP (exp, 1))
	/* Make a copy of this expression and call recursively.  */
	newexp = attr_rtx (code, XEXP (exp, 0), new);
      else
	newexp = exp;
    }
  else
    {
      /* Insert the new term.  */
      newexp = attr_rtx (code, exp, term);
    }

  return simplify_test_exp_in_temp (newexp, insn_code, insn_index);
}

/* If we have an expression which AND's a bunch of
	(not (eq_attrq "alternative" "n"))
   terms, we may have covered all or all but one of the possible alternatives.
   If so, we can optimize.  Similarly for IOR's of EQ_ATTR.

   This routine is passed an expression and either AND or IOR.  It returns a
   bitmask indicating which alternatives are mentioned within EXP.  */

static int
compute_alternative_mask (rtx exp, enum rtx_code code)
{
  const char *string;
  if (GET_CODE (exp) == code)
    return compute_alternative_mask (XEXP (exp, 0), code)
	   | compute_alternative_mask (XEXP (exp, 1), code);

  else if (code == AND && GET_CODE (exp) == NOT
	   && GET_CODE (XEXP (exp, 0)) == EQ_ATTR
	   && XSTR (XEXP (exp, 0), 0) == alternative_name)
    string = XSTR (XEXP (exp, 0), 1);

  else if (code == IOR && GET_CODE (exp) == EQ_ATTR
	   && XSTR (exp, 0) == alternative_name)
    string = XSTR (exp, 1);

  else if (GET_CODE (exp) == EQ_ATTR_ALT)
    {
      if (code == AND && XINT (exp, 1))
	return XINT (exp, 0);

      if (code == IOR && !XINT (exp, 1))
	return XINT (exp, 0);

      return 0;
    }
  else
    return 0;

  if (string[1] == 0)
    return 1 << (string[0] - '0');
  return 1 << atoi (string);
}

/* Given I, a single-bit mask, return RTX to compare the `alternative'
   attribute with the value represented by that bit.  */

static rtx
make_alternative_compare (int mask)
{
  return mk_attr_alt (mask);
}

/* If we are processing an (eq_attr "attr" "value") test, we find the value
   of "attr" for this insn code.  From that value, we can compute a test
   showing when the EQ_ATTR will be true.  This routine performs that
   computation.  If a test condition involves an address, we leave the EQ_ATTR
   intact because addresses are only valid for the `length' attribute.

   EXP is the EQ_ATTR expression and VALUE is the value of that attribute
   for the insn corresponding to INSN_CODE and INSN_INDEX.  */

static rtx
evaluate_eq_attr (rtx exp, rtx value, int insn_code, int insn_index)
{
  rtx orexp, andexp;
  rtx right;
  rtx newexp;
  int i;

  if (GET_CODE (value) == CONST_STRING)
    {
      if (! strcmp_check (XSTR (value, 0), XSTR (exp, 1)))
	newexp = true_rtx;
      else
	newexp = false_rtx;
    }
  else if (GET_CODE (value) == SYMBOL_REF)
    {
      char *p;
      char string[256];

      if (GET_CODE (exp) != EQ_ATTR)
	abort ();

      if (strlen (XSTR (exp, 0)) + strlen (XSTR (exp, 1)) + 2 > 256)
	abort ();

      strcpy (string, XSTR (exp, 0));
      strcat (string, "_");
      strcat (string, XSTR (exp, 1));
      for (p = string; *p; p++)
	*p = TOUPPER (*p);

      newexp = attr_rtx (EQ, value,
			 attr_rtx (SYMBOL_REF,
				   DEF_ATTR_STRING (string)));
    }
  else if (GET_CODE (value) == COND)
    {
      /* We construct an IOR of all the cases for which the requested attribute
	 value is present.  Since we start with FALSE, if it is not present,
	 FALSE will be returned.

	 Each case is the AND of the NOT's of the previous conditions with the
	 current condition; in the default case the current condition is TRUE.

	 For each possible COND value, call ourselves recursively.

	 The extra TRUE and FALSE expressions will be eliminated by another
	 call to the simplification routine.  */

      orexp = false_rtx;
      andexp = true_rtx;

      if (current_alternative_string)
	clear_struct_flag (value);

      for (i = 0; i < XVECLEN (value, 0); i += 2)
	{
	  rtx this = simplify_test_exp_in_temp (XVECEXP (value, 0, i),
						insn_code, insn_index);

	  SIMPLIFY_ALTERNATIVE (this);

	  right = insert_right_side (AND, andexp, this,
				     insn_code, insn_index);
	  right = insert_right_side (AND, right,
				     evaluate_eq_attr (exp,
						       XVECEXP (value, 0,
								i + 1),
						       insn_code, insn_index),
				     insn_code, insn_index);
	  orexp = insert_right_side (IOR, orexp, right,
				     insn_code, insn_index);

	  /* Add this condition into the AND expression.  */
	  newexp = attr_rtx (NOT, this);
	  andexp = insert_right_side (AND, andexp, newexp,
				      insn_code, insn_index);
	}

      /* Handle the default case.  */
      right = insert_right_side (AND, andexp,
				 evaluate_eq_attr (exp, XEXP (value, 1),
						   insn_code, insn_index),
				 insn_code, insn_index);
      newexp = insert_right_side (IOR, orexp, right, insn_code, insn_index);
    }
  else
    abort ();

  /* If uses an address, must return original expression.  But set the
     ATTR_IND_SIMPLIFIED_P bit so we don't try to simplify it again.  */

  address_used = 0;
  walk_attr_value (newexp);

  if (address_used)
    {
      /* This had `&& current_alternative_string', which seems to be wrong.  */
      if (! ATTR_IND_SIMPLIFIED_P (exp))
	return copy_rtx_unchanging (exp);
      return exp;
    }
  else
    return newexp;
}

/* This routine is called when an AND of a term with a tree of AND's is
   encountered.  If the term or its complement is present in the tree, it
   can be replaced with TRUE or FALSE, respectively.

   Note that (eq_attr "att" "v1") and (eq_attr "att" "v2") cannot both
   be true and hence are complementary.

   There is one special case:  If we see
	(and (not (eq_attr "att" "v1"))
	     (eq_attr "att" "v2"))
   this can be replaced by (eq_attr "att" "v2").  To do this we need to
   replace the term, not anything in the AND tree.  So we pass a pointer to
   the term.  */

static rtx
simplify_and_tree (rtx exp, rtx *pterm, int insn_code, int insn_index)
{
  rtx left, right;
  rtx newexp;
  rtx temp;
  int left_eliminates_term, right_eliminates_term;

  if (GET_CODE (exp) == AND)
    {
      left  = simplify_and_tree (XEXP (exp, 0), pterm, insn_code, insn_index);
      right = simplify_and_tree (XEXP (exp, 1), pterm, insn_code, insn_index);
      if (left != XEXP (exp, 0) || right != XEXP (exp, 1))
	{
	  newexp = attr_rtx (AND, left, right);

	  exp = simplify_test_exp_in_temp (newexp, insn_code, insn_index);
	}
    }

  else if (GET_CODE (exp) == IOR)
    {
      /* For the IOR case, we do the same as above, except that we can
         only eliminate `term' if both sides of the IOR would do so.  */
      temp = *pterm;
      left = simplify_and_tree (XEXP (exp, 0), &temp, insn_code, insn_index);
      left_eliminates_term = (temp == true_rtx);

      temp = *pterm;
      right = simplify_and_tree (XEXP (exp, 1), &temp, insn_code, insn_index);
      right_eliminates_term = (temp == true_rtx);

      if (left_eliminates_term && right_eliminates_term)
	*pterm = true_rtx;

      if (left != XEXP (exp, 0) || right != XEXP (exp, 1))
	{
	  newexp = attr_rtx (IOR, left, right);

	  exp = simplify_test_exp_in_temp (newexp, insn_code, insn_index);
	}
    }

  /* Check for simplifications.  Do some extra checking here since this
     routine is called so many times.  */

  if (exp == *pterm)
    return true_rtx;

  else if (GET_CODE (exp) == NOT && XEXP (exp, 0) == *pterm)
    return false_rtx;

  else if (GET_CODE (*pterm) == NOT && exp == XEXP (*pterm, 0))
    return false_rtx;

  else if (GET_CODE (exp) == EQ_ATTR_ALT && GET_CODE (*pterm) == EQ_ATTR_ALT)
    {
      if (attr_alt_subset_p (*pterm, exp))
	return true_rtx;

      if (attr_alt_subset_of_compl_p (*pterm, exp))
	return false_rtx;

      if (attr_alt_subset_p (exp, *pterm))
	*pterm = true_rtx;
	
      return exp;
    }

  else if (GET_CODE (exp) == EQ_ATTR && GET_CODE (*pterm) == EQ_ATTR)
    {
      if (XSTR (exp, 0) != XSTR (*pterm, 0))
	return exp;

      if (! strcmp_check (XSTR (exp, 1), XSTR (*pterm, 1)))
	return true_rtx;
      else
	return false_rtx;
    }

  else if (GET_CODE (*pterm) == EQ_ATTR && GET_CODE (exp) == NOT
	   && GET_CODE (XEXP (exp, 0)) == EQ_ATTR)
    {
      if (XSTR (*pterm, 0) != XSTR (XEXP (exp, 0), 0))
	return exp;

      if (! strcmp_check (XSTR (*pterm, 1), XSTR (XEXP (exp, 0), 1)))
	return false_rtx;
      else
	return true_rtx;
    }

  else if (GET_CODE (exp) == EQ_ATTR && GET_CODE (*pterm) == NOT
	   && GET_CODE (XEXP (*pterm, 0)) == EQ_ATTR)
    {
      if (XSTR (exp, 0) != XSTR (XEXP (*pterm, 0), 0))
	return exp;

      if (! strcmp_check (XSTR (exp, 1), XSTR (XEXP (*pterm, 0), 1)))
	return false_rtx;
      else
	*pterm = true_rtx;
    }

  else if (GET_CODE (exp) == NOT && GET_CODE (*pterm) == NOT)
    {
      if (attr_equal_p (XEXP (exp, 0), XEXP (*pterm, 0)))
	return true_rtx;
    }

  else if (GET_CODE (exp) == NOT)
    {
      if (attr_equal_p (XEXP (exp, 0), *pterm))
	return false_rtx;
    }

  else if (GET_CODE (*pterm) == NOT)
    {
      if (attr_equal_p (XEXP (*pterm, 0), exp))
	return false_rtx;
    }

  else if (attr_equal_p (exp, *pterm))
    return true_rtx;

  return exp;
}

/* Similar to `simplify_and_tree', but for IOR trees.  */

static rtx
simplify_or_tree (rtx exp, rtx *pterm, int insn_code, int insn_index)
{
  rtx left, right;
  rtx newexp;
  rtx temp;
  int left_eliminates_term, right_eliminates_term;

  if (GET_CODE (exp) == IOR)
    {
      left  = simplify_or_tree (XEXP (exp, 0), pterm, insn_code, insn_index);
      right = simplify_or_tree (XEXP (exp, 1), pterm, insn_code, insn_index);
      if (left != XEXP (exp, 0) || right != XEXP (exp, 1))
	{
	  newexp = attr_rtx (GET_CODE (exp), left, right);

	  exp = simplify_test_exp_in_temp (newexp, insn_code, insn_index);
	}
    }

  else if (GET_CODE (exp) == AND)
    {
      /* For the AND case, we do the same as above, except that we can
         only eliminate `term' if both sides of the AND would do so.  */
      temp = *pterm;
      left = simplify_or_tree (XEXP (exp, 0), &temp, insn_code, insn_index);
      left_eliminates_term = (temp == false_rtx);

      temp = *pterm;
      right = simplify_or_tree (XEXP (exp, 1), &temp, insn_code, insn_index);
      right_eliminates_term = (temp == false_rtx);

      if (left_eliminates_term && right_eliminates_term)
	*pterm = false_rtx;

      if (left != XEXP (exp, 0) || right != XEXP (exp, 1))
	{
	  newexp = attr_rtx (GET_CODE (exp), left, right);

	  exp = simplify_test_exp_in_temp (newexp, insn_code, insn_index);
	}
    }

  if (attr_equal_p (exp, *pterm))
    return false_rtx;

  else if (GET_CODE (exp) == NOT && attr_equal_p (XEXP (exp, 0), *pterm))
    return true_rtx;

  else if (GET_CODE (*pterm) == NOT && attr_equal_p (XEXP (*pterm, 0), exp))
    return true_rtx;

  else if (GET_CODE (*pterm) == EQ_ATTR && GET_CODE (exp) == NOT
	   && GET_CODE (XEXP (exp, 0)) == EQ_ATTR
	   && XSTR (*pterm, 0) == XSTR (XEXP (exp, 0), 0))
    *pterm = false_rtx;

  else if (GET_CODE (exp) == EQ_ATTR && GET_CODE (*pterm) == NOT
	   && GET_CODE (XEXP (*pterm, 0)) == EQ_ATTR
	   && XSTR (exp, 0) == XSTR (XEXP (*pterm, 0), 0))
    return false_rtx;

  return exp;
}

/* Compute approximate cost of the expression.  Used to decide whether
   expression is cheap enough for inline.  */
static int
attr_rtx_cost (rtx x)
{
  int cost = 0;
  enum rtx_code code;
  if (!x)
    return 0;
  code = GET_CODE (x);
  switch (code)
    {
    case MATCH_OPERAND:
      if (XSTR (x, 1)[0])
	return 10;
      else
	return 0;

    case EQ_ATTR_ALT:
      return 0;

    case EQ_ATTR:
      /* Alternatives don't result into function call.  */
      if (!strcmp_check (XSTR (x, 0), alternative_name))
	return 0;
      else
	return 5;
    default:
      {
	int i, j;
	const char *fmt = GET_RTX_FORMAT (code);
	for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
	  {
	    switch (fmt[i])
	      {
	      case 'V':
	      case 'E':
		for (j = 0; j < XVECLEN (x, i); j++)
		  cost += attr_rtx_cost (XVECEXP (x, i, j));
		break;
	      case 'e':
		cost += attr_rtx_cost (XEXP (x, i));
		break;
	      }
	  }
      }
      break;
    }
  return cost;
}

/* Simplify test expression and use temporary obstack in order to avoid
   memory bloat.  Use ATTR_IND_SIMPLIFIED to avoid unnecessary simplifications
   and avoid unnecessary copying if possible.  */

static rtx
simplify_test_exp_in_temp (rtx exp, int insn_code, int insn_index)
{
  rtx x;
  struct obstack *old;
  if (ATTR_IND_SIMPLIFIED_P (exp))
    return exp;
  old = rtl_obstack;
  rtl_obstack = temp_obstack;
  x = simplify_test_exp (exp, insn_code, insn_index);
  rtl_obstack = old;
  if (x == exp || rtl_obstack == temp_obstack)
    return x;
  return attr_copy_rtx (x);
}

/* Returns true if S1 is a subset of S2.  */

static bool
attr_alt_subset_p (rtx s1, rtx s2)
{
  switch ((XINT (s1, 1) << 1) | XINT (s2, 1))
    {
    case (0 << 1) | 0:
      return !(XINT (s1, 0) &~ XINT (s2, 0));

    case (0 << 1) | 1:
      return !(XINT (s1, 0) & XINT (s2, 0));

    case (1 << 1) | 0:
      return false;

    case (1 << 1) | 1:
      return !(XINT (s2, 0) &~ XINT (s1, 0));

    default:
      abort ();
    }
}

/* Returns true if S1 is a subset of complement of S2.  */

static bool attr_alt_subset_of_compl_p (rtx s1, rtx s2)
{
  switch ((XINT (s1, 1) << 1) | XINT (s2, 1))
    {
    case (0 << 1) | 0:
      return !(XINT (s1, 0) & XINT (s2, 0));

    case (0 << 1) | 1:
      return !(XINT (s1, 0) & ~XINT (s2, 0));

    case (1 << 1) | 0:
      return !(XINT (s2, 0) &~ XINT (s1, 0));

    case (1 << 1) | 1:
      return false;

    default:
      abort ();
    }
}

/* Return EQ_ATTR_ALT expression representing intersection of S1 and S2.  */

static rtx
attr_alt_intersection (rtx s1, rtx s2)
{
  rtx result = rtx_alloc (EQ_ATTR_ALT);

  switch ((XINT (s1, 1) << 1) | XINT (s2, 1))
    {
    case (0 << 1) | 0:
      XINT (result, 0) = XINT (s1, 0) & XINT (s2, 0);
      break;
    case (0 << 1) | 1:
      XINT (result, 0) = XINT (s1, 0) & ~XINT (s2, 0);
      break;
    case (1 << 1) | 0:
      XINT (result, 0) = XINT (s2, 0) & ~XINT (s1, 0);
      break;
    case (1 << 1) | 1:
      XINT (result, 0) = XINT (s1, 0) | XINT (s2, 0);
      break;
    default:
      abort ();
    }
  XINT (result, 1) = XINT (s1, 1) & XINT (s2, 1);

  return result;
}

/* Return EQ_ATTR_ALT expression representing union of S1 and S2.  */

static rtx
attr_alt_union (rtx s1, rtx s2)
{
  rtx result = rtx_alloc (EQ_ATTR_ALT);

  switch ((XINT (s1, 1) << 1) | XINT (s2, 1))
    {
    case (0 << 1) | 0:
      XINT (result, 0) = XINT (s1, 0) | XINT (s2, 0);
      break;
    case (0 << 1) | 1:
      XINT (result, 0) = XINT (s2, 0) & ~XINT (s1, 0);
      break;
    case (1 << 1) | 0:
      XINT (result, 0) = XINT (s1, 0) & ~XINT (s2, 0);
      break;
    case (1 << 1) | 1:
      XINT (result, 0) = XINT (s1, 0) & XINT (s2, 0);
      break;
    default:
      abort ();
    }

  XINT (result, 1) = XINT (s1, 1) | XINT (s2, 1);
  return result;
}

/* Return EQ_ATTR_ALT expression representing complement of S.  */

static rtx
attr_alt_complement (rtx s)
{
  rtx result = rtx_alloc (EQ_ATTR_ALT);

  XINT (result, 0) = XINT (s, 0);
  XINT (result, 1) = 1 - XINT (s, 1);

  return result;
}

/* Tests whether a bit B belongs to the set represented by S.  */

static bool
attr_alt_bit_p (rtx s, int b)
{
  return XINT (s, 1) ^ ((XINT (s, 0) >> b) & 1);
}

/* Return EQ_ATTR_ALT expression representing set containing elements set
   in E.  */

static rtx
mk_attr_alt (int e)
{
  rtx result = rtx_alloc (EQ_ATTR_ALT);

  XINT (result, 0) = e;
  XINT (result, 1) = 0;

  return result;
}

/* Given an expression, see if it can be simplified for a particular insn
   code based on the values of other attributes being tested.  This can
   eliminate nested get_attr_... calls.

   Note that if an endless recursion is specified in the patterns, the
   optimization will loop.  However, it will do so in precisely the cases where
   an infinite recursion loop could occur during compilation.  It's better that
   it occurs here!  */

static rtx
simplify_test_exp (rtx exp, int insn_code, int insn_index)
{
  rtx left, right;
  struct attr_desc *attr;
  struct attr_value *av;
  struct insn_ent *ie;
  int i;
  rtx newexp = exp;
  bool left_alt, right_alt;

  /* Don't re-simplify something we already simplified.  */
  if (ATTR_IND_SIMPLIFIED_P (exp) || ATTR_CURR_SIMPLIFIED_P (exp))
    return exp;

  switch (GET_CODE (exp))
    {
    case AND:
      left = SIMPLIFY_TEST_EXP (XEXP (exp, 0), insn_code, insn_index);
      SIMPLIFY_ALTERNATIVE (left);
      if (left == false_rtx)
	return false_rtx;
      right = SIMPLIFY_TEST_EXP (XEXP (exp, 1), insn_code, insn_index);
      SIMPLIFY_ALTERNATIVE (right);
      if (left == false_rtx)
	return false_rtx;

      if (GET_CODE (left) == EQ_ATTR_ALT
	  && GET_CODE (right) == EQ_ATTR_ALT)
	{
	  exp = attr_alt_intersection (left, right);
	  return simplify_test_exp (exp, insn_code, insn_index);
	}

      /* If either side is an IOR and we have (eq_attr "alternative" ..")
	 present on both sides, apply the distributive law since this will
	 yield simplifications.  */
      if ((GET_CODE (left) == IOR || GET_CODE (right) == IOR)
	  && compute_alternative_mask (left, IOR)
	  && compute_alternative_mask (right, IOR))
	{
	  if (GET_CODE (left) == IOR)
	    {
	      rtx tem = left;
	      left = right;
	      right = tem;
	    }

	  newexp = attr_rtx (IOR,
			     attr_rtx (AND, left, XEXP (right, 0)),
			     attr_rtx (AND, left, XEXP (right, 1)));

	  return SIMPLIFY_TEST_EXP (newexp, insn_code, insn_index);
	}

      /* Try with the term on both sides.  */
      right = simplify_and_tree (right, &left, insn_code, insn_index);
      if (left == XEXP (exp, 0) && right == XEXP (exp, 1))
	left = simplify_and_tree (left, &right, insn_code, insn_index);

      if (left == false_rtx || right == false_rtx)
	return false_rtx;
      else if (left == true_rtx)
	{
	  return right;
	}
      else if (right == true_rtx)
	{
	  return left;
	}
      /* See if all or all but one of the insn's alternatives are specified
	 in this tree.  Optimize if so.  */

      if (GET_CODE (left) == NOT)
	left_alt = (GET_CODE (XEXP (left, 0)) == EQ_ATTR
		    && XSTR (XEXP (left, 0), 0) == alternative_name);
      else
	left_alt = (GET_CODE (left) == EQ_ATTR_ALT
		    && XINT (left, 1));

      if (GET_CODE (right) == NOT)
	right_alt = (GET_CODE (XEXP (right, 0)) == EQ_ATTR
		     && XSTR (XEXP (right, 0), 0) == alternative_name);
      else
	right_alt = (GET_CODE (right) == EQ_ATTR_ALT
		     && XINT (right, 1));

      if (insn_code >= 0
	  && (GET_CODE (left) == AND
	      || left_alt
	      || GET_CODE (right) == AND
	      || right_alt))
	{
	  i = compute_alternative_mask (exp, AND);
	  if (i & ~insn_alternatives[insn_code])
	    fatal ("invalid alternative specified for pattern number %d",
		   insn_index);

	  /* If all alternatives are excluded, this is false.  */
	  i ^= insn_alternatives[insn_code];
	  if (i == 0)
	    return false_rtx;
	  else if ((i & (i - 1)) == 0 && insn_alternatives[insn_code] > 1)
	    {
	      /* If just one excluded, AND a comparison with that one to the
		 front of the tree.  The others will be eliminated by
		 optimization.  We do not want to do this if the insn has one
		 alternative and we have tested none of them!  */
	      left = make_alternative_compare (i);
	      right = simplify_and_tree (exp, &left, insn_code, insn_index);
	      newexp = attr_rtx (AND, left, right);

	      return SIMPLIFY_TEST_EXP (newexp, insn_code, insn_index);
	    }
	}

      if (left != XEXP (exp, 0) || right != XEXP (exp, 1))
	{
	  newexp = attr_rtx (AND, left, right);
	  return SIMPLIFY_TEST_EXP (newexp, insn_code, insn_index);
	}
      break;

    case IOR:
      left = SIMPLIFY_TEST_EXP (XEXP (exp, 0), insn_code, insn_index);
      SIMPLIFY_ALTERNATIVE (left);
      if (left == true_rtx)
	return true_rtx;
      right = SIMPLIFY_TEST_EXP (XEXP (exp, 1), insn_code, insn_index);
      SIMPLIFY_ALTERNATIVE (right);
      if (right == true_rtx)
	return true_rtx;

      if (GET_CODE (left) == EQ_ATTR_ALT
	  && GET_CODE (right) == EQ_ATTR_ALT)
	{
	  exp = attr_alt_union (left, right);
	  return simplify_test_exp (exp, insn_code, insn_index);
	}

      right = simplify_or_tree (right, &left, insn_code, insn_index);
      if (left == XEXP (exp, 0) && right == XEXP (exp, 1))
	left = simplify_or_tree (left, &right, insn_code, insn_index);

      if (right == true_rtx || left == true_rtx)
	return true_rtx;
      else if (left == false_rtx)
	{
	  return right;
	}
      else if (right == false_rtx)
	{
	  return left;
	}

      /* Test for simple cases where the distributive law is useful.  I.e.,
	    convert (ior (and (x) (y))
			 (and (x) (z)))
	    to      (and (x)
			 (ior (y) (z)))
       */

      else if (GET_CODE (left) == AND && GET_CODE (right) == AND
	       && attr_equal_p (XEXP (left, 0), XEXP (right, 0)))
	{
	  newexp = attr_rtx (IOR, XEXP (left, 1), XEXP (right, 1));

	  left = XEXP (left, 0);
	  right = newexp;
	  newexp = attr_rtx (AND, left, right);
	  return SIMPLIFY_TEST_EXP (newexp, insn_code, insn_index);
	}

      /* See if all or all but one of the insn's alternatives are specified
	 in this tree.  Optimize if so.  */

      else if (insn_code >= 0
	       && (GET_CODE (left) == IOR
		   || (GET_CODE (left) == EQ_ATTR_ALT
		       && !XINT (left, 1))
		   || (GET_CODE (left) == EQ_ATTR
		       && XSTR (left, 0) == alternative_name)
		   || GET_CODE (right) == IOR
		   || (GET_CODE (right) == EQ_ATTR_ALT
		       && !XINT (right, 1))
		   || (GET_CODE (right) == EQ_ATTR
		       && XSTR (right, 0) == alternative_name)))
	{
	  i = compute_alternative_mask (exp, IOR);
	  if (i & ~insn_alternatives[insn_code])
	    fatal ("invalid alternative specified for pattern number %d",
		   insn_index);

	  /* If all alternatives are included, this is true.  */
	  i ^= insn_alternatives[insn_code];
	  if (i == 0)
	    return true_rtx;
	  else if ((i & (i - 1)) == 0 && insn_alternatives[insn_code] > 1)
	    {
	      /* If just one excluded, IOR a comparison with that one to the
		 front of the tree.  The others will be eliminated by
		 optimization.  We do not want to do this if the insn has one
		 alternative and we have tested none of them!  */
	      left = make_alternative_compare (i);
	      right = simplify_and_tree (exp, &left, insn_code, insn_index);
	      newexp = attr_rtx (IOR, attr_rtx (NOT, left), right);

	      return SIMPLIFY_TEST_EXP (newexp, insn_code, insn_index);
	    }
	}

      if (left != XEXP (exp, 0) || right != XEXP (exp, 1))
	{
	  newexp = attr_rtx (IOR, left, right);
	  return SIMPLIFY_TEST_EXP (newexp, insn_code, insn_index);
	}
      break;

    case NOT:
      if (GET_CODE (XEXP (exp, 0)) == NOT)
	{
	  left = SIMPLIFY_TEST_EXP (XEXP (XEXP (exp, 0), 0),
				    insn_code, insn_index);
	  SIMPLIFY_ALTERNATIVE (left);
	  return left;
	}

      left = SIMPLIFY_TEST_EXP (XEXP (exp, 0), insn_code, insn_index);
      SIMPLIFY_ALTERNATIVE (left);
      if (GET_CODE (left) == NOT)
	return XEXP (left, 0);

      if (left == false_rtx)
	return true_rtx;
      if (left == true_rtx)
	return false_rtx;

      if (GET_CODE (left) == EQ_ATTR_ALT)
	{
	  exp = attr_alt_complement (left);
	  return simplify_test_exp (exp, insn_code, insn_index);
	}

      /* Try to apply De`Morgan's laws.  */
      if (GET_CODE (left) == IOR)
	{
	  newexp = attr_rtx (AND,
			     attr_rtx (NOT, XEXP (left, 0)),
			     attr_rtx (NOT, XEXP (left, 1)));

	  newexp = SIMPLIFY_TEST_EXP (newexp, insn_code, insn_index);
	}
      else if (GET_CODE (left) == AND)
	{
	  newexp = attr_rtx (IOR,
			     attr_rtx (NOT, XEXP (left, 0)),
			     attr_rtx (NOT, XEXP (left, 1)));

	  newexp = SIMPLIFY_TEST_EXP (newexp, insn_code, insn_index);
	}
      else if (left != XEXP (exp, 0))
	{
	  newexp = attr_rtx (NOT, left);
	}
      break;

    case EQ_ATTR_ALT:
      if (current_alternative_string)
	return attr_alt_bit_p (exp, atoi (current_alternative_string)) ? true_rtx : false_rtx;

      if (!XINT (exp, 0))
	return XINT (exp, 1) ? true_rtx : false_rtx;
      break;

    case EQ_ATTR:
      if (current_alternative_string && XSTR (exp, 0) == alternative_name)
	return (XSTR (exp, 1) == current_alternative_string
		? true_rtx : false_rtx);

      if (XSTR (exp, 0) == alternative_name)
	{
	  newexp = mk_attr_alt (1 << atoi (XSTR (exp, 1)));
	  break;
	}

      /* Look at the value for this insn code in the specified attribute.
	 We normally can replace this comparison with the condition that
	 would give this insn the values being tested for.  */
      if (XSTR (exp, 0) != alternative_name
	  && (attr = find_attr (&XSTR (exp, 0), 0)) != NULL)
	for (av = attr->first_value; av; av = av->next)
	  for (ie = av->first_insn; ie; ie = ie->next)
	    if (ie->insn_code == insn_code)
	      {
		rtx x;
		x = evaluate_eq_attr (exp, av->value, insn_code, insn_index);
		x = SIMPLIFY_TEST_EXP (x, insn_code, insn_index);
		if (attr_rtx_cost(x) < 20)
		  return x;
	      }
      break;

    default:
      break;
    }

  /* We have already simplified this expression.  Simplifying it again
     won't buy anything unless we weren't given a valid insn code
     to process (i.e., we are canonicalizing something.).  */
  if (insn_code != -2 /* Seems wrong: && current_alternative_string.  */
      && ! ATTR_IND_SIMPLIFIED_P (newexp))
    return copy_rtx_unchanging (newexp);

  return newexp;
}

/* Optimize the attribute lists by seeing if we can determine conditional
   values from the known values of other attributes.  This will save subroutine
   calls during the compilation.  */

static void
optimize_attrs (void)
{
  struct attr_desc *attr;
  struct attr_value *av;
  struct insn_ent *ie;
  rtx newexp;
  int i;
  struct attr_value_list
  {
    struct attr_value *av;
    struct insn_ent *ie;
    struct attr_desc *attr;
    struct attr_value_list *next;
  };
  struct attr_value_list **insn_code_values;
  struct attr_value_list *ivbuf;
  struct attr_value_list *iv;

  /* For each insn code, make a list of all the insn_ent's for it,
     for all values for all attributes.  */

  if (num_insn_ents == 0)
    return;

  /* Make 2 extra elements, for "code" values -2 and -1.  */
  insn_code_values = xcalloc ((insn_code_number + 2),
			      sizeof (struct attr_value_list *));

  /* Offset the table address so we can index by -2 or -1.  */
  insn_code_values += 2;

  iv = ivbuf = xmalloc (num_insn_ents * sizeof (struct attr_value_list));

  for (i = 0; i < MAX_ATTRS_INDEX; i++)
    for (attr = attrs[i]; attr; attr = attr->next)
      for (av = attr->first_value; av; av = av->next)
	for (ie = av->first_insn; ie; ie = ie->next)
	  {
	    iv->attr = attr;
	    iv->av = av;
	    iv->ie = ie;
	    iv->next = insn_code_values[ie->insn_code];
	    insn_code_values[ie->insn_code] = iv;
	    iv++;
	  }

  /* Sanity check on num_insn_ents.  */
  if (iv != ivbuf + num_insn_ents)
    abort ();

  /* Process one insn code at a time.  */
  for (i = -2; i < insn_code_number; i++)
    {
      /* Clear the ATTR_CURR_SIMPLIFIED_P flag everywhere relevant.
	 We use it to mean "already simplified for this insn".  */
      for (iv = insn_code_values[i]; iv; iv = iv->next)
	clear_struct_flag (iv->av->value);

      for (iv = insn_code_values[i]; iv; iv = iv->next)
	{
	  struct obstack *old = rtl_obstack;

	  attr = iv->attr;
	  av = iv->av;
	  ie = iv->ie;
	  if (GET_CODE (av->value) != COND)
	    continue;

	  rtl_obstack = temp_obstack;
	  newexp = av->value;
	  while (GET_CODE (newexp) == COND)
	    {
	      rtx newexp2 = simplify_cond (newexp, ie->insn_code,
					   ie->insn_index);
	      if (newexp2 == newexp)
		break;
	      newexp = newexp2;
	    }

	  rtl_obstack = old;
	  if (newexp != av->value)
	    {
	      newexp = attr_copy_rtx (newexp);
	      remove_insn_ent (av, ie);
	      av = get_attr_value (newexp, attr, ie->insn_code);
	      iv->av = av;
	      insert_insn_ent (av, ie);
	    }
	}
    }

  free (ivbuf);
  free (insn_code_values - 2);
}

/* If EXP is a suitable expression, reorganize it by constructing an
   equivalent expression that is a COND with the tests being all combinations
   of attribute values and the values being simple constants.  */

static rtx
simplify_by_exploding (rtx exp)
{
  rtx list = 0, link, condexp, defval = NULL_RTX;
  struct dimension *space;
  rtx *condtest, *condval;
  int i, j, total, ndim = 0;
  int most_tests, num_marks, new_marks;
  rtx ret;

  /* Locate all the EQ_ATTR expressions.  */
  if (! find_and_mark_used_attributes (exp, &list, &ndim) || ndim == 0)
    {
      unmark_used_attributes (list, 0, 0);
      return exp;
    }

  /* Create an attribute space from the list of used attributes.  For each
     dimension in the attribute space, record the attribute, list of values
     used, and number of values used.  Add members to the list of values to
     cover the domain of the attribute.  This makes the expanded COND form
     order independent.  */

  space = xmalloc (ndim * sizeof (struct dimension));

  total = 1;
  for (ndim = 0; list; ndim++)
    {
      /* Pull the first attribute value from the list and record that
	 attribute as another dimension in the attribute space.  */
      const char *name = XSTR (XEXP (list, 0), 0);
      rtx *prev;

      space[ndim].attr = find_attr (&name, 0);
      XSTR (XEXP (list, 0), 0) = name;

      if (space[ndim].attr == 0
	  || space[ndim].attr->is_numeric)
	{
	  unmark_used_attributes (list, space, ndim);
	  return exp;
	}

      /* Add all remaining attribute values that refer to this attribute.  */
      space[ndim].num_values = 0;
      space[ndim].values = 0;
      prev = &list;
      for (link = list; link; link = *prev)
	if (! strcmp_check (XSTR (XEXP (link, 0), 0), name))
	  {
	    space[ndim].num_values++;
	    *prev = XEXP (link, 1);
	    XEXP (link, 1) = space[ndim].values;
	    space[ndim].values = link;
	  }
	else
	  prev = &XEXP (link, 1);

      /* Add sufficient members to the list of values to make the list
	 mutually exclusive and record the total size of the attribute
	 space.  */
      total *= add_values_to_cover (&space[ndim]);
    }

  /* Sort the attribute space so that the attributes go from non-constant
     to constant and from most values to least values.  */
  for (i = 0; i < ndim; i++)
    for (j = ndim - 1; j > i; j--)
      if ((space[j-1].attr->is_const && !space[j].attr->is_const)
	  || space[j-1].num_values < space[j].num_values)
	{
	  struct dimension tmp;
	  tmp = space[j];
	  space[j] = space[j - 1];
	  space[j - 1] = tmp;
	}

  /* Establish the initial current value.  */
  for (i = 0; i < ndim; i++)
    space[i].current_value = space[i].values;

  condtest = xmalloc (total * sizeof (rtx));
  condval = xmalloc (total * sizeof (rtx));

  /* Expand the tests and values by iterating over all values in the
     attribute space.  */
  for (i = 0;; i++)
    {
      condtest[i] = test_for_current_value (space, ndim);
      condval[i] = simplify_with_current_value (exp, space, ndim);
      if (! increment_current_value (space, ndim))
	break;
    }
  if (i != total - 1)
    abort ();

  /* We are now finished with the original expression.  */
  unmark_used_attributes (0, space, ndim);
  free (space);

  /* Find the most used constant value and make that the default.  */
  most_tests = -1;
  for (i = num_marks = 0; i < total; i++)
    if (GET_CODE (condval[i]) == CONST_STRING
	&& ! ATTR_EQ_ATTR_P (condval[i]))
      {
	/* Mark the unmarked constant value and count how many are marked.  */
	ATTR_EQ_ATTR_P (condval[i]) = 1;
	for (j = new_marks = 0; j < total; j++)
	  if (GET_CODE (condval[j]) == CONST_STRING
	      && ATTR_EQ_ATTR_P (condval[j]))
	    new_marks++;
	if (new_marks - num_marks > most_tests)
	  {
	    most_tests = new_marks - num_marks;
	    defval = condval[i];
	  }
	num_marks = new_marks;
      }
  /* Clear all the marks.  */
  for (i = 0; i < total; i++)
    ATTR_EQ_ATTR_P (condval[i]) = 0;

  /* Give up if nothing is constant.  */
  if (num_marks == 0)
    ret = exp;

  /* If all values are the default, use that.  */
  else if (total == most_tests)
    ret = defval;

  /* Make a COND with the most common constant value the default.  (A more
     complex method where tests with the same value were combined didn't
     seem to improve things.)  */
  else
    {
      condexp = rtx_alloc (COND);
      XVEC (condexp, 0) = rtvec_alloc ((total - most_tests) * 2);
      XEXP (condexp, 1) = defval;
      for (i = j = 0; i < total; i++)
	if (condval[i] != defval)
	  {
	    XVECEXP (condexp, 0, 2 * j) = condtest[i];
	    XVECEXP (condexp, 0, 2 * j + 1) = condval[i];
	    j++;
	  }
      ret = condexp;
    }
  free (condtest);
  free (condval);
  return ret;
}

/* Set the ATTR_EQ_ATTR_P flag for all EQ_ATTR expressions in EXP and
   verify that EXP can be simplified to a constant term if all the EQ_ATTR
   tests have known value.  */

static int
find_and_mark_used_attributes (rtx exp, rtx *terms, int *nterms)
{
  int i;

  switch (GET_CODE (exp))
    {
    case EQ_ATTR:
      if (! ATTR_EQ_ATTR_P (exp))
	{
	  rtx link = rtx_alloc (EXPR_LIST);
	  XEXP (link, 0) = exp;
	  XEXP (link, 1) = *terms;
	  *terms = link;
	  *nterms += 1;
	  ATTR_EQ_ATTR_P (exp) = 1;
	}
      return 1;

    case CONST_STRING:
    case CONST_INT:
      return 1;

    case IF_THEN_ELSE:
      if (! find_and_mark_used_attributes (XEXP (exp, 2), terms, nterms))
	return 0;
    case IOR:
    case AND:
      if (! find_and_mark_used_attributes (XEXP (exp, 1), terms, nterms))
	return 0;
    case NOT:
      if (! find_and_mark_used_attributes (XEXP (exp, 0), terms, nterms))
	return 0;
      return 1;

    case COND:
      for (i = 0; i < XVECLEN (exp, 0); i++)
	if (! find_and_mark_used_attributes (XVECEXP (exp, 0, i), terms, nterms))
	  return 0;
      if (! find_and_mark_used_attributes (XEXP (exp, 1), terms, nterms))
	return 0;
      return 1;

    default:
      return 0;
    }
}

/* Clear the ATTR_EQ_ATTR_P flag in all EQ_ATTR expressions on LIST and
   in the values of the NDIM-dimensional attribute space SPACE.  */

static void
unmark_used_attributes (rtx list, struct dimension *space, int ndim)
{
  rtx link, exp;
  int i;

  for (i = 0; i < ndim; i++)
    unmark_used_attributes (space[i].values, 0, 0);

  for (link = list; link; link = XEXP (link, 1))
    {
      exp = XEXP (link, 0);
      if (GET_CODE (exp) == EQ_ATTR)
	ATTR_EQ_ATTR_P (exp) = 0;
    }
}

/* Update the attribute dimension DIM so that all values of the attribute
   are tested.  Return the updated number of values.  */

static int
add_values_to_cover (struct dimension *dim)
{
  struct attr_value *av;
  rtx exp, link, *prev;
  int nalt = 0;

  for (av = dim->attr->first_value; av; av = av->next)
    if (GET_CODE (av->value) == CONST_STRING)
      nalt++;

  if (nalt < dim->num_values)
    abort ();
  else if (nalt == dim->num_values)
    /* OK.  */
    ;
  else if (nalt * 2 < dim->num_values * 3)
    {
      /* Most all the values of the attribute are used, so add all the unused
	 values.  */
      prev = &dim->values;
      for (link = dim->values; link; link = *prev)
	prev = &XEXP (link, 1);

      for (av = dim->attr->first_value; av; av = av->next)
	if (GET_CODE (av->value) == CONST_STRING)
	  {
	    exp = attr_eq (dim->attr->name, XSTR (av->value, 0));
	    if (ATTR_EQ_ATTR_P (exp))
	      continue;

	    link = rtx_alloc (EXPR_LIST);
	    XEXP (link, 0) = exp;
	    XEXP (link, 1) = 0;
	    *prev = link;
	    prev = &XEXP (link, 1);
	  }
      dim->num_values = nalt;
    }
  else
    {
      rtx orexp = false_rtx;

      /* Very few values are used, so compute a mutually exclusive
	 expression.  (We could do this for numeric values if that becomes
	 important.)  */
      prev = &dim->values;
      for (link = dim->values; link; link = *prev)
	{
	  orexp = insert_right_side (IOR, orexp, XEXP (link, 0), -2, -2);
	  prev = &XEXP (link, 1);
	}
      link = rtx_alloc (EXPR_LIST);
      XEXP (link, 0) = attr_rtx (NOT, orexp);
      XEXP (link, 1) = 0;
      *prev = link;
      dim->num_values++;
    }
  return dim->num_values;
}

/* Increment the current value for the NDIM-dimensional attribute space SPACE
   and return FALSE if the increment overflowed.  */

static int
increment_current_value (struct dimension *space, int ndim)
{
  int i;

  for (i = ndim - 1; i >= 0; i--)
    {
      if ((space[i].current_value = XEXP (space[i].current_value, 1)) == 0)
	space[i].current_value = space[i].values;
      else
	return 1;
    }
  return 0;
}

/* Construct an expression corresponding to the current value for the
   NDIM-dimensional attribute space SPACE.  */

static rtx
test_for_current_value (struct dimension *space, int ndim)
{
  int i;
  rtx exp = true_rtx;

  for (i = 0; i < ndim; i++)
    exp = insert_right_side (AND, exp, XEXP (space[i].current_value, 0),
			     -2, -2);

  return exp;
}

/* Given the current value of the NDIM-dimensional attribute space SPACE,
   set the corresponding EQ_ATTR expressions to that value and reduce
   the expression EXP as much as possible.  On input [and output], all
   known EQ_ATTR expressions are set to FALSE.  */

static rtx
simplify_with_current_value (rtx exp, struct dimension *space, int ndim)
{
  int i;
  rtx x;

  /* Mark each current value as TRUE.  */
  for (i = 0; i < ndim; i++)
    {
      x = XEXP (space[i].current_value, 0);
      if (GET_CODE (x) == EQ_ATTR)
	ATTR_EQ_ATTR_P (x) = 0;
    }

  exp = simplify_with_current_value_aux (exp);

  /* Change each current value back to FALSE.  */
  for (i = 0; i < ndim; i++)
    {
      x = XEXP (space[i].current_value, 0);
      if (GET_CODE (x) == EQ_ATTR)
	ATTR_EQ_ATTR_P (x) = 1;
    }

  return exp;
}

/* Reduce the expression EXP based on the ATTR_EQ_ATTR_P settings of
   all EQ_ATTR expressions.  */

static rtx
simplify_with_current_value_aux (rtx exp)
{
  int i;
  rtx cond;

  switch (GET_CODE (exp))
    {
    case EQ_ATTR:
      if (ATTR_EQ_ATTR_P (exp))
	return false_rtx;
      else
	return true_rtx;
    case CONST_STRING:
    case CONST_INT:
      return exp;

    case IF_THEN_ELSE:
      cond = simplify_with_current_value_aux (XEXP (exp, 0));
      if (cond == true_rtx)
	return simplify_with_current_value_aux (XEXP (exp, 1));
      else if (cond == false_rtx)
	return simplify_with_current_value_aux (XEXP (exp, 2));
      else
	return attr_rtx (IF_THEN_ELSE, cond,
			 simplify_with_current_value_aux (XEXP (exp, 1)),
			 simplify_with_current_value_aux (XEXP (exp, 2)));

    case IOR:
      cond = simplify_with_current_value_aux (XEXP (exp, 1));
      if (cond == true_rtx)
	return cond;
      else if (cond == false_rtx)
	return simplify_with_current_value_aux (XEXP (exp, 0));
      else
	return attr_rtx (IOR, cond,
			 simplify_with_current_value_aux (XEXP (exp, 0)));

    case AND:
      cond = simplify_with_current_value_aux (XEXP (exp, 1));
      if (cond == true_rtx)
	return simplify_with_current_value_aux (XEXP (exp, 0));
      else if (cond == false_rtx)
	return cond;
      else
	return attr_rtx (AND, cond,
			 simplify_with_current_value_aux (XEXP (exp, 0)));

    case NOT:
      cond = simplify_with_current_value_aux (XEXP (exp, 0));
      if (cond == true_rtx)
	return false_rtx;
      else if (cond == false_rtx)
	return true_rtx;
      else
	return attr_rtx (NOT, cond);

    case COND:
      for (i = 0; i < XVECLEN (exp, 0); i += 2)
	{
	  cond = simplify_with_current_value_aux (XVECEXP (exp, 0, i));
	  if (cond == true_rtx)
	    return simplify_with_current_value_aux (XVECEXP (exp, 0, i + 1));
	  else if (cond == false_rtx)
	    continue;
	  else
	    abort (); /* With all EQ_ATTR's of known value, a case should
			 have been selected.  */
	}
      return simplify_with_current_value_aux (XEXP (exp, 1));

    default:
      abort ();
    }
}

/* Clear the ATTR_CURR_SIMPLIFIED_P flag in EXP and its subexpressions.  */

static void
clear_struct_flag (rtx x)
{
  int i;
  int j;
  enum rtx_code code;
  const char *fmt;

  ATTR_CURR_SIMPLIFIED_P (x) = 0;
  if (ATTR_IND_SIMPLIFIED_P (x))
    return;

  code = GET_CODE (x);

  switch (code)
    {
    case REG:
    case QUEUED:
    case CONST_INT:
    case CONST_DOUBLE:
    case CONST_VECTOR:
    case SYMBOL_REF:
    case CODE_LABEL:
    case PC:
    case CC0:
    case EQ_ATTR:
    case ATTR_FLAG:
      return;

    default:
      break;
    }

  /* Compare the elements.  If any pair of corresponding elements
     fail to match, return 0 for the whole things.  */

  fmt = GET_RTX_FORMAT (code);
  for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
    {
      switch (fmt[i])
	{
	case 'V':
	case 'E':
	  for (j = 0; j < XVECLEN (x, i); j++)
	    clear_struct_flag (XVECEXP (x, i, j));
	  break;

	case 'e':
	  clear_struct_flag (XEXP (x, i));
	  break;
	}
    }
}

/* Create table entries for DEFINE_ATTR.  */

static void
gen_attr (rtx exp, int lineno)
{
  struct attr_desc *attr;
  struct attr_value *av;
  const char *name_ptr;
  char *p;

  /* Make a new attribute structure.  Check for duplicate by looking at
     attr->default_val, since it is initialized by this routine.  */
  attr = find_attr (&XSTR (exp, 0), 1);
  if (attr->default_val)
    {
      message_with_line (lineno, "duplicate definition for attribute %s",
			 attr->name);
      message_with_line (attr->lineno, "previous definition");
      have_error = 1;
      return;
    }
  attr->lineno = lineno;

  if (*XSTR (exp, 1) == '\0')
    attr->is_numeric = 1;
  else
    {
      name_ptr = XSTR (exp, 1);
      while ((p = next_comma_elt (&name_ptr)) != NULL)
	{
	  av = oballoc (sizeof (struct attr_value));
	  av->value = attr_rtx (CONST_STRING, p);
	  av->next = attr->first_value;
	  attr->first_value = av;
	  av->first_insn = NULL;
	  av->num_insns = 0;
	  av->has_asm_insn = 0;
	}
    }

  if (GET_CODE (XEXP (exp, 2)) == CONST)
    {
      attr->is_const = 1;
      if (attr->is_numeric)
	{
	  message_with_line (lineno,
			     "constant attributes may not take numeric values");
	  have_error = 1;
	}

      /* Get rid of the CONST node.  It is allowed only at top-level.  */
      XEXP (exp, 2) = XEXP (XEXP (exp, 2), 0);
    }

  if (! strcmp_check (attr->name, length_str) && ! attr->is_numeric)
    {
      message_with_line (lineno,
			 "`length' attribute must take numeric values");
      have_error = 1;
    }

  /* Set up the default value.  */
  XEXP (exp, 2) = check_attr_value (XEXP (exp, 2), attr);
  attr->default_val = get_attr_value (XEXP (exp, 2), attr, -2);
}

/* Given a pattern for DEFINE_PEEPHOLE or DEFINE_INSN, return the number of
   alternatives in the constraints.  Assume all MATCH_OPERANDs have the same
   number of alternatives as this should be checked elsewhere.  */

static int
count_alternatives (rtx exp)
{
  int i, j, n;
  const char *fmt;

  if (GET_CODE (exp) == MATCH_OPERAND)
    return n_comma_elts (XSTR (exp, 2));

  for (i = 0, fmt = GET_RTX_FORMAT (GET_CODE (exp));
       i < GET_RTX_LENGTH (GET_CODE (exp)); i++)
    switch (*fmt++)
      {
      case 'e':
      case 'u':
	n = count_alternatives (XEXP (exp, i));
	if (n)
	  return n;
	break;

      case 'E':
      case 'V':
	if (XVEC (exp, i) != NULL)
	  for (j = 0; j < XVECLEN (exp, i); j++)
	    {
	      n = count_alternatives (XVECEXP (exp, i, j));
	      if (n)
		return n;
	    }
      }

  return 0;
}

/* Returns nonzero if the given expression contains an EQ_ATTR with the
   `alternative' attribute.  */

static int
compares_alternatives_p (rtx exp)
{
  int i, j;
  const char *fmt;

  if (GET_CODE (exp) == EQ_ATTR && XSTR (exp, 0) == alternative_name)
    return 1;

  for (i = 0, fmt = GET_RTX_FORMAT (GET_CODE (exp));
       i < GET_RTX_LENGTH (GET_CODE (exp)); i++)
    switch (*fmt++)
      {
      case 'e':
      case 'u':
	if (compares_alternatives_p (XEXP (exp, i)))
	  return 1;
	break;

      case 'E':
	for (j = 0; j < XVECLEN (exp, i); j++)
	  if (compares_alternatives_p (XVECEXP (exp, i, j)))
	    return 1;
	break;
      }

  return 0;
}

/* Returns nonzero is INNER is contained in EXP.  */

static int
contained_in_p (rtx inner, rtx exp)
{
  int i, j;
  const char *fmt;

  if (rtx_equal_p (inner, exp))
    return 1;

  for (i = 0, fmt = GET_RTX_FORMAT (GET_CODE (exp));
       i < GET_RTX_LENGTH (GET_CODE (exp)); i++)
    switch (*fmt++)
      {
      case 'e':
      case 'u':
	if (contained_in_p (inner, XEXP (exp, i)))
	  return 1;
	break;

      case 'E':
	for (j = 0; j < XVECLEN (exp, i); j++)
	  if (contained_in_p (inner, XVECEXP (exp, i, j)))
	    return 1;
	break;
      }

  return 0;
}

/* Process DEFINE_PEEPHOLE, DEFINE_INSN, and DEFINE_ASM_ATTRIBUTES.  */

static void
gen_insn (rtx exp, int lineno)
{
  struct insn_def *id;

  id = oballoc (sizeof (struct insn_def));
  id->next = defs;
  defs = id;
  id->def = exp;
  id->lineno = lineno;

  switch (GET_CODE (exp))
    {
    case DEFINE_INSN:
      id->insn_code = insn_code_number;
      id->insn_index = insn_index_number;
      id->num_alternatives = count_alternatives (exp);
      if (id->num_alternatives == 0)
	id->num_alternatives = 1;
      id->vec_idx = 4;
      break;

    case DEFINE_PEEPHOLE:
      id->insn_code = insn_code_number;
      id->insn_index = insn_index_number;
      id->num_alternatives = count_alternatives (exp);
      if (id->num_alternatives == 0)
	id->num_alternatives = 1;
      id->vec_idx = 3;
      break;

    case DEFINE_ASM_ATTRIBUTES:
      id->insn_code = -1;
      id->insn_index = -1;
      id->num_alternatives = 1;
      id->vec_idx = 0;
      got_define_asm_attributes = 1;
      break;

    default:
      abort ();
    }
}

/* Process a DEFINE_DELAY.  Validate the vector length, check if annul
   true or annul false is specified, and make a `struct delay_desc'.  */

static void
gen_delay (rtx def, int lineno)
{
  struct delay_desc *delay;
  int i;

  if (XVECLEN (def, 1) % 3 != 0)
    {
      message_with_line (lineno,
			 "number of elements in DEFINE_DELAY must be multiple of three");
      have_error = 1;
      return;
    }

  for (i = 0; i < XVECLEN (def, 1); i += 3)
    {
      if (XVECEXP (def, 1, i + 1))
	have_annul_true = 1;
      if (XVECEXP (def, 1, i + 2))
	have_annul_false = 1;
    }

  delay = oballoc (sizeof (struct delay_desc));
  delay->def = def;
  delay->num = ++num_delays;
  delay->next = delays;
  delay->lineno = lineno;
  delays = delay;
}

/* Process a DEFINE_FUNCTION_UNIT.

   This gives information about a function unit contained in the CPU.
   We fill in a `struct function_unit_op' and a `struct function_unit'
   with information used later by `expand_unit'.  */

static void
gen_unit (rtx def, int lineno)
{
  struct function_unit *unit;
  struct function_unit_op *op;
  const char *name = XSTR (def, 0);
  int multiplicity = XINT (def, 1);
  int simultaneity = XINT (def, 2);
  rtx condexp = XEXP (def, 3);
  int ready_cost = MAX (XINT (def, 4), 1);
  int issue_delay = MAX (XINT (def, 5), 1);

  /* See if we have already seen this function unit.  If so, check that
     the multiplicity and simultaneity values are the same.  If not, make
     a structure for this function unit.  */
  for (unit = units; unit; unit = unit->next)
    if (! strcmp (unit->name, name))
      {
	if (unit->multiplicity != multiplicity
	    || unit->simultaneity != simultaneity)
	  {
	    message_with_line (lineno,
			       "differing specifications given for function unit %s",
			       unit->name);
	    message_with_line (unit->first_lineno, "previous definition");
	    have_error = 1;
	    return;
	  }
	break;
      }

  if (unit == 0)
    {
      unit = oballoc (sizeof (struct function_unit));
      unit->name = name;
      unit->multiplicity = multiplicity;
      unit->simultaneity = simultaneity;
      unit->issue_delay.min = unit->issue_delay.max = issue_delay;
      unit->num = num_units++;
      unit->num_opclasses = 0;
      unit->condexp = false_rtx;
      unit->ops = 0;
      unit->next = units;
      unit->first_lineno = lineno;
      units = unit;
    }
  else
    XSTR (def, 0) = unit->name;

  /* Make a new operation class structure entry and initialize it.  */
  op = oballoc (sizeof (struct function_unit_op));
  op->condexp = condexp;
  op->num = unit->num_opclasses++;
  op->ready = ready_cost;
  op->issue_delay = issue_delay;
  op->next = unit->ops;
  op->lineno = lineno;
  unit->ops = op;
  num_unit_opclasses++;

  /* Set our issue expression based on whether or not an optional conflict
     vector was specified.  */
  if (XVEC (def, 6))
    {
      /* Compute the IOR of all the specified expressions.  */
      rtx orexp = false_rtx;
      int i;

      for (i = 0; i < XVECLEN (def, 6); i++)
	orexp = insert_right_side (IOR, orexp, XVECEXP (def, 6, i), -2, -2);

      op->conflict_exp = orexp;
      extend_range (&unit->issue_delay, 1, issue_delay);
    }
  else
    {
      op->conflict_exp = true_rtx;
      extend_range (&unit->issue_delay, issue_delay, issue_delay);
    }

  /* Merge our conditional into that of the function unit so we can determine
     which insns are used by the function unit.  */
  unit->condexp = insert_right_side (IOR, unit->condexp, op->condexp, -2, -2);
}

/* Given a piece of RTX, print a C expression to test its truth value.
   We use AND and IOR both for logical and bit-wise operations, so
   interpret them as logical unless they are inside a comparison expression.
   The first bit of FLAGS will be nonzero in that case.

   Set the second bit of FLAGS to make references to attribute values use
   a cached local variable instead of calling a function.  */

static void
write_test_expr (rtx exp, int flags)
{
  int comparison_operator = 0;
  RTX_CODE code;
  struct attr_desc *attr;

  /* In order not to worry about operator precedence, surround our part of
     the expression with parentheses.  */

  printf ("(");
  code = GET_CODE (exp);
  switch (code)
    {
    /* Binary operators.  */
    case EQ: case NE:
    case GE: case GT: case GEU: case GTU:
    case LE: case LT: case LEU: case LTU:
      comparison_operator = 1;

    case PLUS:   case MINUS:  case MULT:     case DIV:      case MOD:
    case AND:    case IOR:    case XOR:
    case ASHIFT: case LSHIFTRT: case ASHIFTRT:
      write_test_expr (XEXP (exp, 0), flags | comparison_operator);
      switch (code)
	{
	case EQ:
	  printf (" == ");
	  break;
	case NE:
	  printf (" != ");
	  break;
	case GE:
	  printf (" >= ");
	  break;
	case GT:
	  printf (" > ");
	  break;
	case GEU:
	  printf (" >= (unsigned) ");
	  break;
	case GTU:
	  printf (" > (unsigned) ");
	  break;
	case LE:
	  printf (" <= ");
	  break;
	case LT:
	  printf (" < ");
	  break;
	case LEU:
	  printf (" <= (unsigned) ");
	  break;
	case LTU:
	  printf (" < (unsigned) ");
	  break;
	case PLUS:
	  printf (" + ");
	  break;
	case MINUS:
	  printf (" - ");
	  break;
	case MULT:
	  printf (" * ");
	  break;
	case DIV:
	  printf (" / ");
	  break;
	case MOD:
	  printf (" %% ");
	  break;
	case AND:
	  if (flags & 1)
	    printf (" & ");
	  else
	    printf (" && ");
	  break;
	case IOR:
	  if (flags & 1)
	    printf (" | ");
	  else
	    printf (" || ");
	  break;
	case XOR:
	  printf (" ^ ");
	  break;
	case ASHIFT:
	  printf (" << ");
	  break;
	case LSHIFTRT:
	case ASHIFTRT:
	  printf (" >> ");
	  break;
	default:
	  abort ();
	}

      write_test_expr (XEXP (exp, 1), flags | comparison_operator);
      break;

    case NOT:
      /* Special-case (not (eq_attrq "alternative" "x")) */
      if (! (flags & 1) && GET_CODE (XEXP (exp, 0)) == EQ_ATTR
	  && XSTR (XEXP (exp, 0), 0) == alternative_name)
	{
	  printf ("which_alternative != %s", XSTR (XEXP (exp, 0), 1));
	  break;
	}

      /* Otherwise, fall through to normal unary operator.  */

    /* Unary operators.  */
    case ABS:  case NEG:
      switch (code)
	{
	case NOT:
	  if (flags & 1)
	    printf ("~ ");
	  else
	    printf ("! ");
	  break;
	case ABS:
	  printf ("abs ");
	  break;
	case NEG:
	  printf ("-");
	  break;
	default:
	  abort ();
	}

      write_test_expr (XEXP (exp, 0), flags);
      break;

    case EQ_ATTR_ALT:
	{
	  int set = XINT (exp, 0), bit = 0;

	  if (flags & 1)
	    fatal ("EQ_ATTR_ALT not valid inside comparison");

	  if (!set)
	    fatal ("Empty EQ_ATTR_ALT should be optimized out");

	  if (!(set & (set - 1)))
	    {
	      if (!(set & 0xffff))
		{
		  bit += 16;
		  set >>= 16;
		}
	      if (!(set & 0xff))
		{
		  bit += 8;
		  set >>= 8;
		}
	      if (!(set & 0xf))
		{
		  bit += 4;
		  set >>= 4;
		}
	      if (!(set & 0x3))
		{
		  bit += 2;
		  set >>= 2;
		}
	      if (!(set & 1))
		bit++;

	      printf ("which_alternative %s= %d",
		      XINT (exp, 1) ? "!" : "=", bit);
	    }
	  else
	    {
	      printf ("%s((1 << which_alternative) & 0x%x)",
		      XINT (exp, 1) ? "!" : "", set);
	    }
	}
      break;

    /* Comparison test of an attribute with a value.  Most of these will
       have been removed by optimization.   Handle "alternative"
       specially and give error if EQ_ATTR present inside a comparison.  */
    case EQ_ATTR:
      if (flags & 1)
	fatal ("EQ_ATTR not valid inside comparison");

      if (XSTR (exp, 0) == alternative_name)
	{
	  printf ("which_alternative == %s", XSTR (exp, 1));
	  break;
	}

      attr = find_attr (&XSTR (exp, 0), 0);
      if (! attr)
	abort ();

      /* Now is the time to expand the value of a constant attribute.  */
      if (attr->is_const)
	{
	  write_test_expr (evaluate_eq_attr (exp, attr->default_val->value,
					     -2, -2),
			   flags);
	}
      else
	{
	  if (flags & 2)
	    printf ("attr_%s", attr->name);
	  else
	    printf ("get_attr_%s (insn)", attr->name);
	  printf (" == ");
	  write_attr_valueq (attr, XSTR (exp, 1));
	}
      break;

    /* Comparison test of flags for define_delays.  */
    case ATTR_FLAG:
      if (flags & 1)
	fatal ("ATTR_FLAG not valid inside comparison");
      printf ("(flags & ATTR_FLAG_%s) != 0", XSTR (exp, 0));
      break;

    /* See if an operand matches a predicate.  */
    case MATCH_OPERAND:
      /* If only a mode is given, just ensure the mode matches the operand.
	 If neither a mode nor predicate is given, error.  */
      if (XSTR (exp, 1) == NULL || *XSTR (exp, 1) == '\0')
	{
	  if (GET_MODE (exp) == VOIDmode)
	    fatal ("null MATCH_OPERAND specified as test");
	  else
	    printf ("GET_MODE (operands[%d]) == %smode",
		    XINT (exp, 0), GET_MODE_NAME (GET_MODE (exp)));
	}
      else
	printf ("%s (operands[%d], %smode)",
		XSTR (exp, 1), XINT (exp, 0), GET_MODE_NAME (GET_MODE (exp)));
      break;

    case MATCH_INSN:
      printf ("%s (insn)", XSTR (exp, 0));
      break;

    /* Constant integer.  */
    case CONST_INT:
      printf (HOST_WIDE_INT_PRINT_DEC, XWINT (exp, 0));
      break;

    /* A random C expression.  */
    case SYMBOL_REF:
      printf ("%s", XSTR (exp, 0));
      break;

    /* The address of the branch target.  */
    case MATCH_DUP:
      printf ("INSN_ADDRESSES_SET_P () ? INSN_ADDRESSES (INSN_UID (GET_CODE (operands[%d]) == LABEL_REF ? XEXP (operands[%d], 0) : operands[%d])) : 0",
	      XINT (exp, 0), XINT (exp, 0), XINT (exp, 0));
      break;

    case PC:
      /* The address of the current insn.  We implement this actually as the
	 address of the current insn for backward branches, but the last
	 address of the next insn for forward branches, and both with
	 adjustments that account for the worst-case possible stretching of
	 intervening alignments between this insn and its destination.  */
      printf ("insn_current_reference_address (insn)");
      break;

    case CONST_STRING:
      printf ("%s", XSTR (exp, 0));
      break;

    case IF_THEN_ELSE:
      write_test_expr (XEXP (exp, 0), flags & 2);
      printf (" ? ");
      write_test_expr (XEXP (exp, 1), flags | 1);
      printf (" : ");
      write_test_expr (XEXP (exp, 2), flags | 1);
      break;

    default:
      fatal ("bad RTX code `%s' in attribute calculation\n",
	     GET_RTX_NAME (code));
    }

  printf (")");
}

/* Given an attribute value, return the maximum CONST_STRING argument
   encountered.  Set *UNKNOWNP and return INT_MAX if the value is unknown.  */

static int
max_attr_value (rtx exp, int *unknownp)
{
  int current_max;
  int i, n;

  switch (GET_CODE (exp))
    {
    case CONST_STRING:
      current_max = atoi (XSTR (exp, 0));
      break;

    case COND:
      current_max = max_attr_value (XEXP (exp, 1), unknownp);
      for (i = 0; i < XVECLEN (exp, 0); i += 2)
	{
	  n = max_attr_value (XVECEXP (exp, 0, i + 1), unknownp);
	  if (n > current_max)
	    current_max = n;
	}
      break;

    case IF_THEN_ELSE:
      current_max = max_attr_value (XEXP (exp, 1), unknownp);
      n = max_attr_value (XEXP (exp, 2), unknownp);
      if (n > current_max)
	current_max = n;
      break;

    default:
      *unknownp = 1;
      current_max = INT_MAX;
      break;
    }

  return current_max;
}

/* Given an attribute value, return the result of ORing together all
   CONST_STRING arguments encountered.  Set *UNKNOWNP and return -1
   if the numeric value is not known.  */

static int
or_attr_value (rtx exp, int *unknownp)
{
  int current_or;
  int i;

  switch (GET_CODE (exp))
    {
    case CONST_STRING:
      current_or = atoi (XSTR (exp, 0));
      break;

    case COND:
      current_or = or_attr_value (XEXP (exp, 1), unknownp);
      for (i = 0; i < XVECLEN (exp, 0); i += 2)
	current_or |= or_attr_value (XVECEXP (exp, 0, i + 1), unknownp);
      break;

    case IF_THEN_ELSE:
      current_or = or_attr_value (XEXP (exp, 1), unknownp);
      current_or |= or_attr_value (XEXP (exp, 2), unknownp);
      break;

    default:
      *unknownp = 1;
      current_or = -1;
      break;
    }

  return current_or;
}

/* Scan an attribute value, possibly a conditional, and record what actions
   will be required to do any conditional tests in it.

   Specifically, set
	`must_extract'	  if we need to extract the insn operands
	`must_constrain'  if we must compute `which_alternative'
	`address_used'	  if an address expression was used
	`length_used'	  if an (eq_attr "length" ...) was used
 */

static void
walk_attr_value (rtx exp)
{
  int i, j;
  const char *fmt;
  RTX_CODE code;

  if (exp == NULL)
    return;

  code = GET_CODE (exp);
  switch (code)
    {
    case SYMBOL_REF:
      if (! ATTR_IND_SIMPLIFIED_P (exp))
	/* Since this is an arbitrary expression, it can look at anything.
	   However, constant expressions do not depend on any particular
	   insn.  */
	must_extract = must_constrain = 1;
      return;

    case MATCH_OPERAND:
      must_extract = 1;
      return;

    case EQ_ATTR_ALT:
      must_extract = must_constrain = 1;
      break;

    case EQ_ATTR:
      if (XSTR (exp, 0) == alternative_name)
	must_extract = must_constrain = 1;
      else if (strcmp_check (XSTR (exp, 0), length_str) == 0)
	length_used = 1;
      return;

    case MATCH_DUP:
      must_extract = 1;
      address_used = 1;
      return;

    case PC:
      address_used = 1;
      return;

    case ATTR_FLAG:
      return;

    default:
      break;
    }

  for (i = 0, fmt = GET_RTX_FORMAT (code); i < GET_RTX_LENGTH (code); i++)
    switch (*fmt++)
      {
      case 'e':
      case 'u':
	walk_attr_value (XEXP (exp, i));
	break;

      case 'E':
	if (XVEC (exp, i) != NULL)
	  for (j = 0; j < XVECLEN (exp, i); j++)
	    walk_attr_value (XVECEXP (exp, i, j));
	break;
      }
}

/* Write out a function to obtain the attribute for a given INSN.  */

static void
write_attr_get (struct attr_desc *attr)
{
  struct attr_value *av, *common_av;

  /* Find the most used attribute value.  Handle that as the `default' of the
     switch we will generate.  */
  common_av = find_most_used (attr);

  /* Write out start of function, then all values with explicit `case' lines,
     then a `default', then the value with the most uses.  */
  if (attr->static_p)
    printf ("static ");
  if (!attr->is_numeric)
    printf ("enum attr_%s\n", attr->name);
  else if (attr->unsigned_p)
    printf ("unsigned int\n");
  else
    printf ("int\n");

  /* If the attribute name starts with a star, the remainder is the name of
     the subroutine to use, instead of `get_attr_...'.  */
  if (attr->name[0] == '*')
    printf ("%s (rtx insn ATTRIBUTE_UNUSED)\n", &attr->name[1]);
  else if (attr->is_const == 0)
    printf ("get_attr_%s (rtx insn ATTRIBUTE_UNUSED)\n", attr->name);
  else
    {
      printf ("get_attr_%s (void)\n", attr->name);
      printf ("{\n");

      for (av = attr->first_value; av; av = av->next)
	if (av->num_insns != 0)
	  write_attr_set (attr, 2, av->value, "return", ";",
			  true_rtx, av->first_insn->insn_code,
			  av->first_insn->insn_index);

      printf ("}\n\n");
      return;
    }

  printf ("{\n");

  if (GET_CODE (common_av->value) == FFS)
    {
      rtx p = XEXP (common_av->value, 0);

      /* No need to emit code to abort if the insn is unrecognized; the
         other get_attr_foo functions will do that when we call them.  */

      write_toplevel_expr (p);

      printf ("\n  if (accum && accum == (accum & -accum))\n");
      printf ("    {\n");
      printf ("      int i;\n");
      printf ("      for (i = 0; accum >>= 1; ++i) continue;\n");
      printf ("      accum = i;\n");
      printf ("    }\n  else\n");
      printf ("    accum = ~accum;\n");
      printf ("  return accum;\n}\n\n");
    }
  else
    {
      printf ("  switch (recog_memoized (insn))\n");
      printf ("    {\n");

      for (av = attr->first_value; av; av = av->next)
	if (av != common_av)
	  write_attr_case (attr, av, 1, "return", ";", 4, true_rtx);

      write_attr_case (attr, common_av, 0, "return", ";", 4, true_rtx);
      printf ("    }\n}\n\n");
    }
}

/* Given an AND tree of known true terms (because we are inside an `if' with
   that as the condition or are in an `else' clause) and an expression,
   replace any known true terms with TRUE.  Use `simplify_and_tree' to do
   the bulk of the work.  */

static rtx
eliminate_known_true (rtx known_true, rtx exp, int insn_code, int insn_index)
{
  rtx term;

  known_true = SIMPLIFY_TEST_EXP (known_true, insn_code, insn_index);

  if (GET_CODE (known_true) == AND)
    {
      exp = eliminate_known_true (XEXP (known_true, 0), exp,
				  insn_code, insn_index);
      exp = eliminate_known_true (XEXP (known_true, 1), exp,
				  insn_code, insn_index);
    }
  else
    {
      term = known_true;
      exp = simplify_and_tree (exp, &term, insn_code, insn_index);
    }

  return exp;
}

/* Write out a series of tests and assignment statements to perform tests and
   sets of an attribute value.  We are passed an indentation amount and prefix
   and suffix strings to write around each attribute value (e.g., "return"
   and ";").  */

static void
write_attr_set (struct attr_desc *attr, int indent, rtx value,
		const char *prefix, const char *suffix, rtx known_true,
		int insn_code, int insn_index)
{
  if (GET_CODE (value) == COND)
    {
      /* Assume the default value will be the default of the COND unless we
	 find an always true expression.  */
      rtx default_val = XEXP (value, 1);
      rtx our_known_true = known_true;
      rtx newexp;
      int first_if = 1;
      int i;

      for (i = 0; i < XVECLEN (value, 0); i += 2)
	{
	  rtx testexp;
	  rtx inner_true;

	  testexp = eliminate_known_true (our_known_true,
					  XVECEXP (value, 0, i),
					  insn_code, insn_index);
	  newexp = attr_rtx (NOT, testexp);
	  newexp = insert_right_side (AND, our_known_true, newexp,
				      insn_code, insn_index);

	  /* If the test expression is always true or if the next `known_true'
	     expression is always false, this is the last case, so break
	     out and let this value be the `else' case.  */
	  if (testexp == true_rtx || newexp == false_rtx)
	    {
	      default_val = XVECEXP (value, 0, i + 1);
	      break;
	    }

	  /* Compute the expression to pass to our recursive call as being
	     known true.  */
	  inner_true = insert_right_side (AND, our_known_true,
					  testexp, insn_code, insn_index);

	  /* If this is always false, skip it.  */
	  if (inner_true == false_rtx)
	    continue;

	  write_indent (indent);
	  printf ("%sif ", first_if ? "" : "else ");
	  first_if = 0;
	  write_test_expr (testexp, 0);
	  printf ("\n");
	  write_indent (indent + 2);
	  printf ("{\n");

	  write_attr_set (attr, indent + 4,
			  XVECEXP (value, 0, i + 1), prefix, suffix,
			  inner_true, insn_code, insn_index);
	  write_indent (indent + 2);
	  printf ("}\n");
	  our_known_true = newexp;
	}

      if (! first_if)
	{
	  write_indent (indent);
	  printf ("else\n");
	  write_indent (indent + 2);
	  printf ("{\n");
	}

      write_attr_set (attr, first_if ? indent : indent + 4, default_val,
		      prefix, suffix, our_known_true, insn_code, insn_index);

      if (! first_if)
	{
	  write_indent (indent + 2);
	  printf ("}\n");
	}
    }
  else
    {
      write_indent (indent);
      printf ("%s ", prefix);
      write_attr_value (attr, value);
      printf ("%s\n", suffix);
    }
}

/* Write out the computation for one attribute value.  */

static void
write_attr_case (struct attr_desc *attr, struct attr_value *av,
		 int write_case_lines, const char *prefix, const char *suffix,
		 int indent, rtx known_true)
{
  struct insn_ent *ie;

  if (av->num_insns == 0)
    return;

  if (av->has_asm_insn)
    {
      write_indent (indent);
      printf ("case -1:\n");
      write_indent (indent + 2);
      printf ("if (GET_CODE (PATTERN (insn)) != ASM_INPUT\n");
      write_indent (indent + 2);
      printf ("    && asm_noperands (PATTERN (insn)) < 0)\n");
      write_indent (indent + 2);
      printf ("  fatal_insn_not_found (insn);\n");
    }

  if (write_case_lines)
    {
      for (ie = av->first_insn; ie; ie = ie->next)
	if (ie->insn_code != -1)
	  {
	    write_indent (indent);
	    printf ("case %d:\n", ie->insn_code);
	  }
    }
  else
    {
      write_indent (indent);
      printf ("default:\n");
    }

  /* See what we have to do to output this value.  */
  must_extract = must_constrain = address_used = 0;
  walk_attr_value (av->value);

  if (must_constrain)
    {
      write_indent (indent + 2);
      printf ("extract_constrain_insn_cached (insn);\n");
    }
  else if (must_extract)
    {
      write_indent (indent + 2);
      printf ("extract_insn_cached (insn);\n");
    }

  write_attr_set (attr, indent + 2, av->value, prefix, suffix,
		  known_true, av->first_insn->insn_code,
		  av->first_insn->insn_index);

  if (strncmp (prefix, "return", 6))
    {
      write_indent (indent + 2);
      printf ("break;\n");
    }
  printf ("\n");
}

/* Search for uses of non-const attributes and write code to cache them.  */

static int
write_expr_attr_cache (rtx p, struct attr_desc *attr)
{
  const char *fmt;
  int i, ie, j, je;

  if (GET_CODE (p) == EQ_ATTR)
    {
      if (XSTR (p, 0) != attr->name)
	return 0;

      if (!attr->is_numeric)
	printf ("  enum attr_%s ", attr->name);
      else if (attr->unsigned_p)
	printf ("  unsigned int ");
      else
	printf ("  int ");

      printf ("attr_%s = get_attr_%s (insn);\n", attr->name, attr->name);
      return 1;
    }

  fmt = GET_RTX_FORMAT (GET_CODE (p));
  ie = GET_RTX_LENGTH (GET_CODE (p));
  for (i = 0; i < ie; i++)
    {
      switch (*fmt++)
	{
	case 'e':
	  if (write_expr_attr_cache (XEXP (p, i), attr))
	    return 1;
	  break;

	case 'E':
	  je = XVECLEN (p, i);
	  for (j = 0; j < je; ++j)
	    if (write_expr_attr_cache (XVECEXP (p, i, j), attr))
	      return 1;
	  break;
	}
    }

  return 0;
}

/* Evaluate an expression at top level.  A front end to write_test_expr,
   in which we cache attribute values and break up excessively large
   expressions to cater to older compilers.  */

static void
write_toplevel_expr (rtx p)
{
  struct attr_desc *attr;
  int i;

  for (i = 0; i < MAX_ATTRS_INDEX; ++i)
    for (attr = attrs[i]; attr; attr = attr->next)
      if (!attr->is_const)
	write_expr_attr_cache (p, attr);

  printf ("  unsigned long accum = 0;\n\n");

  while (GET_CODE (p) == IOR)
    {
      rtx e;
      if (GET_CODE (XEXP (p, 0)) == IOR)
	e = XEXP (p, 1), p = XEXP (p, 0);
      else
	e = XEXP (p, 0), p = XEXP (p, 1);

      printf ("  accum |= ");
      write_test_expr (e, 3);
      printf (";\n");
    }
  printf ("  accum |= ");
  write_test_expr (p, 3);
  printf (";\n");
}

/* Utilities to write names in various forms.  */

static void
write_unit_name (const char *prefix, int num, const char *suffix)
{
  struct function_unit *unit;

  for (unit = units; unit; unit = unit->next)
    if (unit->num == num)
      {
	printf ("%s%s%s", prefix, unit->name, suffix);
	return;
      }

  printf ("%s<unknown>%s", prefix, suffix);
}

static void
write_attr_valueq (struct attr_desc *attr, const char *s)
{
  if (attr->is_numeric)
    {
      int num = atoi (s);

      printf ("%d", num);

      /* Make the blockage range values and function units used values easier
         to read.  */
      if (attr->func_units_p)
	{
	  if (num == -1)
	    printf (" /* units: none */");
	  else if (num >= 0)
	    write_unit_name (" /* units: ", num, " */");
	  else
	    {
	      int i;
	      const char *sep = " /* units: ";
	      for (i = 0, num = ~num; num; i++, num >>= 1)
		if (num & 1)
		  {
		    write_unit_name (sep, i, (num == 1) ? " */" : "");
		    sep = ", ";
		  }
	    }
	}

      else if (attr->blockage_p)
	printf (" /* min %d, max %d */", num >> (HOST_BITS_PER_INT / 2),
		num & ((1 << (HOST_BITS_PER_INT / 2)) - 1));

      else if (num > 9 || num < 0)
	printf (" /* 0x%x */", num);
    }
  else
    {
      write_upcase (attr->name);
      printf ("_");
      write_upcase (s);
    }
}

static void
write_attr_value (struct attr_desc *attr, rtx value)
{
  int op;

  switch (GET_CODE (value))
    {
    case CONST_STRING:
      write_attr_valueq (attr, XSTR (value, 0));
      break;

    case CONST_INT:
      printf (HOST_WIDE_INT_PRINT_DEC, INTVAL (value));
      break;

    case SYMBOL_REF:
      fputs (XSTR (value, 0), stdout);
      break;

    case ATTR:
      {
	struct attr_desc *attr2 = find_attr (&XSTR (value, 0), 0);
	printf ("get_attr_%s (%s)", attr2->name,
		(attr2->is_const ? "" : "insn"));
      }
      break;

    case PLUS:
      op = '+';
      goto do_operator;
    case MINUS:
      op = '-';
      goto do_operator;
    case MULT:
      op = '*';
      goto do_operator;
    case DIV:
      op = '/';
      goto do_operator;
    case MOD:
      op = '%';
      goto do_operator;

    do_operator:
      write_attr_value (attr, XEXP (value, 0));
      putchar (' ');
      putchar (op);
      putchar (' ');
      write_attr_value (attr, XEXP (value, 1));
      break;

    default:
      abort ();
    }
}

static void
write_upcase (const char *str)
{
  while (*str)
    {
      /* The argument of TOUPPER should not have side effects.  */
      putchar (TOUPPER(*str));
      str++;
    }
}

static void
write_indent (int indent)
{
  for (; indent > 8; indent -= 8)
    printf ("\t");

  for (; indent; indent--)
    printf (" ");
}

/* Write a subroutine that is given an insn that requires a delay slot, a
   delay slot ordinal, and a candidate insn.  It returns nonzero if the
   candidate can be placed in the specified delay slot of the insn.

   We can write as many as three subroutines.  `eligible_for_delay'
   handles normal delay slots, `eligible_for_annul_true' indicates that
   the specified insn can be annulled if the branch is true, and likewise
   for `eligible_for_annul_false'.

   KIND is a string distinguishing these three cases ("delay", "annul_true",
   or "annul_false").  */

static void
write_eligible_delay (const char *kind)
{
  struct delay_desc *delay;
  int max_slots;
  char str[50];
  const char *pstr;
  struct attr_desc *attr;
  struct attr_value *av, *common_av;
  int i;

  /* Compute the maximum number of delay slots required.  We use the delay
     ordinal times this number plus one, plus the slot number as an index into
     the appropriate predicate to test.  */

  for (delay = delays, max_slots = 0; delay; delay = delay->next)
    if (XVECLEN (delay->def, 1) / 3 > max_slots)
      max_slots = XVECLEN (delay->def, 1) / 3;

  /* Write function prelude.  */

  printf ("int\n");
  printf ("eligible_for_%s (rtx delay_insn ATTRIBUTE_UNUSED, int slot, rtx candidate_insn, int flags ATTRIBUTE_UNUSED)\n",
	  kind);
  printf ("{\n");
  printf ("  rtx insn;\n");
  printf ("\n");
  printf ("  if (slot >= %d)\n", max_slots);
  printf ("    abort ();\n");
  printf ("\n");

  /* If more than one delay type, find out which type the delay insn is.  */

  if (num_delays > 1)
    {
      attr = find_attr (&delay_type_str, 0);
      if (! attr)
	abort ();
      common_av = find_most_used (attr);

      printf ("  insn = delay_insn;\n");
      printf ("  switch (recog_memoized (insn))\n");
      printf ("    {\n");

      sprintf (str, " * %d;\n      break;", max_slots);
      for (av = attr->first_value; av; av = av->next)
	if (av != common_av)
	  write_attr_case (attr, av, 1, "slot +=", str, 4, true_rtx);

      write_attr_case (attr, common_av, 0, "slot +=", str, 4, true_rtx);
      printf ("    }\n\n");

      /* Ensure matched.  Otherwise, shouldn't have been called.  */
      printf ("  if (slot < %d)\n", max_slots);
      printf ("    abort ();\n\n");
    }

  /* If just one type of delay slot, write simple switch.  */
  if (num_delays == 1 && max_slots == 1)
    {
      printf ("  insn = candidate_insn;\n");
      printf ("  switch (recog_memoized (insn))\n");
      printf ("    {\n");

      attr = find_attr (&delay_1_0_str, 0);
      if (! attr)
	abort ();
      common_av = find_most_used (attr);

      for (av = attr->first_value; av; av = av->next)
	if (av != common_av)
	  write_attr_case (attr, av, 1, "return", ";", 4, true_rtx);

      write_attr_case (attr, common_av, 0, "return", ";", 4, true_rtx);
      printf ("    }\n");
    }

  else
    {
      /* Write a nested CASE.  The first indicates which condition we need to
	 test, and the inner CASE tests the condition.  */
      printf ("  insn = candidate_insn;\n");
      printf ("  switch (slot)\n");
      printf ("    {\n");

      for (delay = delays; delay; delay = delay->next)
	for (i = 0; i < XVECLEN (delay->def, 1); i += 3)
	  {
	    printf ("    case %d:\n",
		    (i / 3) + (num_delays == 1 ? 0 : delay->num * max_slots));
	    printf ("      switch (recog_memoized (insn))\n");
	    printf ("\t{\n");

	    sprintf (str, "*%s_%d_%d", kind, delay->num, i / 3);
	    pstr = str;
	    attr = find_attr (&pstr, 0);
	    if (! attr)
	      abort ();
	    common_av = find_most_used (attr);

	    for (av = attr->first_value; av; av = av->next)
	      if (av != common_av)
		write_attr_case (attr, av, 1, "return", ";", 8, true_rtx);

	    write_attr_case (attr, common_av, 0, "return", ";", 8, true_rtx);
	    printf ("      }\n");
	  }

      printf ("    default:\n");
      printf ("      abort ();\n");
      printf ("    }\n");
    }

  printf ("}\n\n");
}

/* Write routines to compute conflict cost for function units.  Then write a
   table describing the available function units.  */

static void
write_function_unit_info (void)
{
  struct function_unit *unit;
  int i;

  /* Write out conflict routines for function units.  Don't bother writing
     one if there is only one issue delay value.  */

  for (unit = units; unit; unit = unit->next)
    {
      if (unit->needs_blockage_function)
	write_complex_function (unit, "blockage", "block");

      /* If the minimum and maximum conflict costs are the same, there
	 is only one value, so we don't need a function.  */
      if (! unit->needs_conflict_function)
	{
	  unit->default_cost = make_numeric_value (unit->issue_delay.max);
	  continue;
	}

      /* The function first computes the case from the candidate insn.  */
      unit->default_cost = make_numeric_value (0);
      write_complex_function (unit, "conflict_cost", "cost");
    }

  /* Now that all functions have been written, write the table describing
     the function units.   The name is included for documentation purposes
     only.  */

  printf ("const struct function_unit_desc function_units[] = {\n");

  /* Write out the descriptions in numeric order, but don't force that order
     on the list.  Doing so increases the runtime of genattrtab.c.  */
  for (i = 0; i < num_units; i++)
    {
      for (unit = units; unit; unit = unit->next)
	if (unit->num == i)
	  break;

      printf ("  {\"%s\", %d, %d, %d, %s, %d, %s_unit_ready_cost, ",
	      unit->name, 1 << unit->num, unit->multiplicity,
	      unit->simultaneity, XSTR (unit->default_cost, 0),
	      unit->issue_delay.max, unit->name);

      if (unit->needs_conflict_function)
	printf ("%s_unit_conflict_cost, ", unit->name);
      else
	printf ("0, ");

      printf ("%d, ", unit->max_blockage);

      if (unit->needs_range_function)
	printf ("%s_unit_blockage_range, ", unit->name);
      else
	printf ("0, ");

      if (unit->needs_blockage_function)
	printf ("%s_unit_blockage", unit->name);
      else
	printf ("0");

      printf ("}, \n");
    }

  if (num_units == 0)
    printf ("{\"dummy\", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} /* a dummy element */");
  printf ("};\n\n");
}

static void
write_complex_function (struct function_unit *unit,
			const char *name,
			const char *connection)
{
  struct attr_desc *case_attr, *attr;
  struct attr_value *av, *common_av;
  rtx value;
  char str[256];
  const char *pstr;
  int using_case;
  int i;

  printf ("static int\n");
  printf ("%s_unit_%s (rtx executing_insn, rtx candidate_insn)\n",
	  unit->name, name);
  printf ("{\n");
  printf ("  rtx insn;\n");
  printf ("  int casenum;\n\n");
  printf ("  insn = executing_insn;\n");
  printf ("  switch (recog_memoized (insn))\n");
  printf ("    {\n");

  /* Write the `switch' statement to get the case value.  */
  if (strlen (unit->name) + sizeof "*_cases" > 256)
    abort ();
  sprintf (str, "*%s_cases", unit->name);
  pstr = str;
  case_attr = find_attr (&pstr, 0);
  if (! case_attr)
    abort ();
  common_av = find_most_used (case_attr);

  for (av = case_attr->first_value; av; av = av->next)
    if (av != common_av)
      write_attr_case (case_attr, av, 1,
		       "casenum =", ";", 4, unit->condexp);

  write_attr_case (case_attr, common_av, 0,
		   "casenum =", ";", 4, unit->condexp);
  printf ("    }\n\n");

  /* Now write an outer switch statement on each case.  Then write
     the tests on the executing function within each.  */
  printf ("  insn = candidate_insn;\n");
  printf ("  switch (casenum)\n");
  printf ("    {\n");

  for (i = 0; i < unit->num_opclasses; i++)
    {
      /* Ensure using this case.  */
      using_case = 0;
      for (av = case_attr->first_value; av; av = av->next)
	if (av->num_insns
	    && contained_in_p (make_numeric_value (i), av->value))
	  using_case = 1;

      if (! using_case)
	continue;

      printf ("    case %d:\n", i);
      sprintf (str, "*%s_%s_%d", unit->name, connection, i);
      pstr = str;
      attr = find_attr (&pstr, 0);
      if (! attr)
	abort ();

      /* If single value, just write it.  */
      value = find_single_value (attr);
      if (value)
	write_attr_set (attr, 6, value, "return", ";\n", true_rtx, -2, -2);
      else
	{
	  common_av = find_most_used (attr);
	  printf ("      switch (recog_memoized (insn))\n");
	  printf ("\t{\n");

	  for (av = attr->first_value; av; av = av->next)
	    if (av != common_av)
	      write_attr_case (attr, av, 1,
			       "return", ";", 8, unit->condexp);

	  write_attr_case (attr, common_av, 0,
			   "return", ";", 8, unit->condexp);
	  printf ("      }\n\n");
	}
    }

  /* This default case should not be needed, but gcc's analysis is not
     good enough to realize that the default case is not needed for the
     second switch statement.  */
  printf ("    default:\n      abort ();\n");
  printf ("    }\n}\n\n");
}

/* This page contains miscellaneous utility routines.  */

/* Given a pointer to a (char *), return a malloc'ed string containing the
   next comma-separated element.  Advance the pointer to after the string
   scanned, or the end-of-string.  Return NULL if at end of string.  */

static char *
next_comma_elt (const char **pstr)
{
  const char *start;

  start = scan_comma_elt (pstr);

  if (start == NULL)
    return NULL;

  return attr_string (start, *pstr - start);
}

/* Return a `struct attr_desc' pointer for a given named attribute.  If CREATE
   is nonzero, build a new attribute, if one does not exist.  *NAME_P is
   replaced by a pointer to a canonical copy of the string.  */

static struct attr_desc *
find_attr (const char **name_p, int create)
{
  struct attr_desc *attr;
  int index;
  const char *name = *name_p;

  /* Before we resort to using `strcmp', see if the string address matches
     anywhere.  In most cases, it should have been canonicalized to do so.  */
  if (name == alternative_name)
    return NULL;

  index = name[0] & (MAX_ATTRS_INDEX - 1);
  for (attr = attrs[index]; attr; attr = attr->next)
    if (name == attr->name)
      return attr;

  /* Otherwise, do it the slow way.  */
  for (attr = attrs[index]; attr; attr = attr->next)
    if (name[0] == attr->name[0] && ! strcmp (name, attr->name))
      {
	*name_p = attr->name;
	return attr;
      }

  if (! create)
    return NULL;

  attr = oballoc (sizeof (struct attr_desc));
  attr->name = DEF_ATTR_STRING (name);
  attr->first_value = attr->default_val = NULL;
  attr->is_numeric = attr->negative_ok = attr->is_const = attr->is_special = 0;
  attr->unsigned_p = attr->func_units_p = attr->blockage_p = attr->static_p = 0;
  attr->next = attrs[index];
  attrs[index] = attr;

  *name_p = attr->name;

  return attr;
}

/* Create internal attribute with the given default value.  */

void
make_internal_attr (const char *name, rtx value, int special)
{
  struct attr_desc *attr;

  attr = find_attr (&name, 1);
  if (attr->default_val)
    abort ();

  attr->is_numeric = 1;
  attr->is_const = 0;
  attr->is_special = (special & ATTR_SPECIAL) != 0;
  attr->negative_ok = (special & ATTR_NEGATIVE_OK) != 0;
  attr->unsigned_p = (special & ATTR_UNSIGNED) != 0;
  attr->func_units_p = (special & ATTR_FUNC_UNITS) != 0;
  attr->blockage_p = (special & ATTR_BLOCKAGE) != 0;
  attr->static_p = (special & ATTR_STATIC) != 0;
  attr->default_val = get_attr_value (value, attr, -2);
}

/* Find the most used value of an attribute.  */

static struct attr_value *
find_most_used (struct attr_desc *attr)
{
  struct attr_value *av;
  struct attr_value *most_used;
  int nuses;

  most_used = NULL;
  nuses = -1;

  for (av = attr->first_value; av; av = av->next)
    if (av->num_insns > nuses)
      nuses = av->num_insns, most_used = av;

  return most_used;
}

/* If an attribute only has a single value used, return it.  Otherwise
   return NULL.  */

static rtx
find_single_value (struct attr_desc *attr)
{
  struct attr_value *av;
  rtx unique_value;

  unique_value = NULL;
  for (av = attr->first_value; av; av = av->next)
    if (av->num_insns)
      {
	if (unique_value)
	  return NULL;
	else
	  unique_value = av->value;
      }

  return unique_value;
}

/* Return (attr_value "n") */

rtx
make_numeric_value (int n)
{
  static rtx int_values[20];
  rtx exp;
  char *p;

  if (n < 0)
    abort ();

  if (n < 20 && int_values[n])
    return int_values[n];

  p = attr_printf (MAX_DIGITS, "%d", n);
  exp = attr_rtx (CONST_STRING, p);

  if (n < 20)
    int_values[n] = exp;

  return exp;
}

static void
extend_range (struct range *range, int min, int max)
{
  if (range->min > min)
    range->min = min;
  if (range->max < max)
    range->max = max;
}

static rtx
copy_rtx_unchanging (rtx orig)
{
  if (ATTR_IND_SIMPLIFIED_P (orig) || ATTR_CURR_SIMPLIFIED_P (orig))
    return orig;

  ATTR_CURR_SIMPLIFIED_P (orig) = 1;
  return orig;
}

/* Determine if an insn has a constant number of delay slots, i.e., the
   number of delay slots is not a function of the length of the insn.  */

static void
write_const_num_delay_slots (void)
{
  struct attr_desc *attr = find_attr (&num_delay_slots_str, 0);
  struct attr_value *av;
  struct insn_ent *ie;

  if (attr)
    {
      printf ("int\nconst_num_delay_slots (rtx insn)\n");
      printf ("{\n");
      printf ("  switch (recog_memoized (insn))\n");
      printf ("    {\n");

      for (av = attr->first_value; av; av = av->next)
	{
	  length_used = 0;
	  walk_attr_value (av->value);
	  if (length_used)
	    {
	      for (ie = av->first_insn; ie; ie = ie->next)
		if (ie->insn_code != -1)
		  printf ("    case %d:\n", ie->insn_code);
	      printf ("      return 0;\n");
	    }
	}

      printf ("    default:\n");
      printf ("      return 1;\n");
      printf ("    }\n}\n\n");
    }
}

int
main (int argc, char **argv)
{
  rtx desc;
  struct attr_desc *attr;
  struct insn_def *id;
  rtx tem;
  int i;

  progname = "genattrtab";

  if (argc <= 1)
    fatal ("no input file name");

  if (init_md_reader_args (argc, argv) != SUCCESS_EXIT_CODE)
    return (FATAL_EXIT_CODE);

  obstack_init (hash_obstack);
  obstack_init (temp_obstack);

  /* Set up true and false rtx's */
  true_rtx = rtx_alloc (CONST_INT);
  XWINT (true_rtx, 0) = 1;
  false_rtx = rtx_alloc (CONST_INT);
  XWINT (false_rtx, 0) = 0;
  ATTR_IND_SIMPLIFIED_P (true_rtx) = ATTR_IND_SIMPLIFIED_P (false_rtx) = 1;
  ATTR_PERMANENT_P (true_rtx) = ATTR_PERMANENT_P (false_rtx) = 1;

  alternative_name = DEF_ATTR_STRING ("alternative");
  length_str = DEF_ATTR_STRING ("length");
  delay_type_str = DEF_ATTR_STRING ("*delay_type");
  delay_1_0_str = DEF_ATTR_STRING ("*delay_1_0");
  num_delay_slots_str = DEF_ATTR_STRING ("*num_delay_slots");

  printf ("/* Generated automatically by the program `genattrtab'\n\
from the machine description file `md'.  */\n\n");

  /* Read the machine description.  */

  initiate_automaton_gen (argc, argv);
  while (1)
    {
      int lineno;

      desc = read_md_rtx (&lineno, &insn_code_number);
      if (desc == NULL)
	break;

      switch (GET_CODE (desc))
	{
	case DEFINE_INSN:
	case DEFINE_PEEPHOLE:
	case DEFINE_ASM_ATTRIBUTES:
	  gen_insn (desc, lineno);
	  break;

	case DEFINE_ATTR:
	  gen_attr (desc, lineno);
	  break;

	case DEFINE_DELAY:
	  gen_delay (desc, lineno);
	  break;

	case DEFINE_FUNCTION_UNIT:
	  gen_unit (desc, lineno);
	  break;

	case DEFINE_CPU_UNIT:
	  gen_cpu_unit (desc);
	  break;

	case DEFINE_QUERY_CPU_UNIT:
	  gen_query_cpu_unit (desc);
	  break;

	case DEFINE_BYPASS:
	  gen_bypass (desc);
	  break;

	case EXCLUSION_SET:
	  gen_excl_set (desc);
	  break;

	case PRESENCE_SET:
	  gen_presence_set (desc);
	  break;

	case FINAL_PRESENCE_SET:
	  gen_final_presence_set (desc);
	  break;

	case ABSENCE_SET:
	  gen_absence_set (desc);
	  break;

	case FINAL_ABSENCE_SET:
	  gen_final_absence_set (desc);
	  break;

	case DEFINE_AUTOMATON:
	  gen_automaton (desc);
	  break;

	case AUTOMATA_OPTION:
	  gen_automata_option (desc);
	  break;

	case DEFINE_RESERVATION:
	  gen_reserv (desc);
	  break;

	case DEFINE_INSN_RESERVATION:
	  gen_insn_reserv (desc);
	  break;

	default:
	  break;
	}
      if (GET_CODE (desc) != DEFINE_ASM_ATTRIBUTES)
	insn_index_number++;
    }

  if (have_error)
    return FATAL_EXIT_CODE;

  insn_code_number++;

  /* If we didn't have a DEFINE_ASM_ATTRIBUTES, make a null one.  */
  if (! got_define_asm_attributes)
    {
      tem = rtx_alloc (DEFINE_ASM_ATTRIBUTES);
      XVEC (tem, 0) = rtvec_alloc (0);
      gen_insn (tem, 0);
    }

  /* Expand DEFINE_DELAY information into new attribute.  */
  if (num_delays)
    expand_delays ();

  if (num_units || num_dfa_decls)
    {
      /* Expand DEFINE_FUNCTION_UNIT information into new attributes.  */
      expand_units ();
      /* Build DFA, output some functions and expand DFA information
	 into new attributes.  */
      expand_automata ();
    }

  printf ("#include \"config.h\"\n");
  printf ("#include \"system.h\"\n");
  printf ("#include \"coretypes.h\"\n");
  printf ("#include \"tm.h\"\n");
  printf ("#include \"rtl.h\"\n");
  printf ("#include \"tm_p.h\"\n");
  printf ("#include \"insn-config.h\"\n");
  printf ("#include \"recog.h\"\n");
  printf ("#include \"regs.h\"\n");
  printf ("#include \"real.h\"\n");
  printf ("#include \"output.h\"\n");
  printf ("#include \"insn-attr.h\"\n");
  printf ("#include \"toplev.h\"\n");
  printf ("#include \"flags.h\"\n");
  printf ("#include \"function.h\"\n");
  printf ("\n");
  printf ("#define operands recog_data.operand\n\n");

  /* Make `insn_alternatives'.  */
  insn_alternatives = oballoc (insn_code_number * sizeof (int));
  for (id = defs; id; id = id->next)
    if (id->insn_code >= 0)
      insn_alternatives[id->insn_code] = (1 << id->num_alternatives) - 1;

  /* Make `insn_n_alternatives'.  */
  insn_n_alternatives = oballoc (insn_code_number * sizeof (int));
  for (id = defs; id; id = id->next)
    if (id->insn_code >= 0)
      insn_n_alternatives[id->insn_code] = id->num_alternatives;

  /* Prepare to write out attribute subroutines by checking everything stored
     away and building the attribute cases.  */

  check_defs ();

  for (i = 0; i < MAX_ATTRS_INDEX; i++)
    for (attr = attrs[i]; attr; attr = attr->next)
      attr->default_val->value
	= check_attr_value (attr->default_val->value, attr);

  if (have_error)
    return FATAL_EXIT_CODE;

  for (i = 0; i < MAX_ATTRS_INDEX; i++)
    for (attr = attrs[i]; attr; attr = attr->next)
      fill_attr (attr);

  /* Construct extra attributes for `length'.  */
  make_length_attrs ();

  /* Perform any possible optimizations to speed up compilation.  */
  optimize_attrs ();

  /* Now write out all the `gen_attr_...' routines.  Do these before the
     special routines (specifically before write_function_unit_info), so
     that they get defined before they are used.  */

  for (i = 0; i < MAX_ATTRS_INDEX; i++)
    for (attr = attrs[i]; attr; attr = attr->next)
      {
	if (! attr->is_special && ! attr->is_const)
	  {
	    int insn_alts_p;

	    insn_alts_p
	      = (attr->name [0] == '*'
		 && strcmp (&attr->name[1], INSN_ALTS_FUNC_NAME) == 0);
	    if (insn_alts_p)
	      printf ("\n#if AUTOMATON_ALTS\n");
	    write_attr_get (attr);
	    if (insn_alts_p)
	      printf ("#endif\n\n");
	  }
      }

  /* Write out delay eligibility information, if DEFINE_DELAY present.
     (The function to compute the number of delay slots will be written
     below.)  */
  if (num_delays)
    {
      write_eligible_delay ("delay");
      if (have_annul_true)
	write_eligible_delay ("annul_true");
      if (have_annul_false)
	write_eligible_delay ("annul_false");
    }

  if (num_units || num_dfa_decls)
    {
      /* Write out information about function units.  */
      write_function_unit_info ();
      /* Output code for pipeline hazards recognition based on DFA
	 (deterministic finite state automata.  */
      write_automata ();
    }

  /* Write out constant delay slot info.  */
  write_const_num_delay_slots ();

  write_length_unit_log ();

  fflush (stdout);
  return (ferror (stdout) != 0 ? FATAL_EXIT_CODE : SUCCESS_EXIT_CODE);
}

/* Define this so we can link with print-rtl.o to get debug_rtx function.  */
const char *
get_insn_name (int code ATTRIBUTE_UNUSED)
{
  return NULL;
}
