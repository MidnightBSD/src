/* Functions related to invoking methods and overloaded functions.
   Copyright (C) 1987, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 
   1999, 2000, 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.
   Contributed by Michael Tiemann (tiemann@cygnus.com) and
   modified by Brendan Kehoe (brendan@cygnus.com).

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


/* High-level class interface.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "cp-tree.h"
#include "output.h"
#include "flags.h"
#include "rtl.h"
#include "toplev.h"
#include "expr.h"
#include "diagnostic.h"
#include "intl.h"
#include "target.h"
#include "convert.h"

static struct z_candidate * tourney (struct z_candidate *);
static int equal_functions (tree, tree);
static int joust (struct z_candidate *, struct z_candidate *, bool);
static int compare_ics (tree, tree);
static tree build_over_call (struct z_candidate *, int);
static tree build_java_interface_fn_ref (tree, tree);
#define convert_like(CONV, EXPR)				\
  convert_like_real ((CONV), (EXPR), NULL_TREE, 0, 0, 		\
		     /*issue_conversion_warnings=*/true)
#define convert_like_with_context(CONV, EXPR, FN, ARGNO)	\
  convert_like_real ((CONV), (EXPR), (FN), (ARGNO), 0, 		\
		     /*issue_conversion_warnings=*/true)
static tree convert_like_real (tree, tree, tree, int, int, bool);
static void op_error (enum tree_code, enum tree_code, tree, tree,
			    tree, const char *);
static tree build_object_call (tree, tree);
static tree resolve_args (tree);
static struct z_candidate *build_user_type_conversion_1 (tree, tree, int);
static void print_z_candidate (const char *, struct z_candidate *);
static void print_z_candidates (struct z_candidate *);
static tree build_this (tree);
static struct z_candidate *splice_viable (struct z_candidate *, bool, bool *);
static bool any_strictly_viable (struct z_candidate *);
static struct z_candidate *add_template_candidate
        (struct z_candidate **, tree, tree, tree, tree, tree, 
         tree, tree, int, unification_kind_t);
static struct z_candidate *add_template_candidate_real
	(struct z_candidate **, tree, tree, tree, tree, tree, 
           tree, tree, int, tree, unification_kind_t);
static struct z_candidate *add_template_conv_candidate 
        (struct z_candidate **, tree, tree, tree, tree, tree, tree);
static void add_builtin_candidates
	(struct z_candidate **, enum tree_code, enum tree_code,
	       tree, tree *, int);
static void add_builtin_candidate
	(struct z_candidate **, enum tree_code, enum tree_code,
	       tree, tree, tree, tree *, tree *, int);
static bool is_complete (tree);
static void build_builtin_candidate 
	(struct z_candidate **, tree, tree, tree, tree *, tree *,
	       int);
static struct z_candidate *add_conv_candidate 
	(struct z_candidate **, tree, tree, tree, tree, tree);
static struct z_candidate *add_function_candidate 
	(struct z_candidate **, tree, tree, tree, tree, tree, int);
static tree implicit_conversion (tree, tree, tree, int);
static tree standard_conversion (tree, tree, tree, int);
static tree reference_binding (tree, tree, tree, int);
static tree build_conv (enum tree_code, tree, tree);
static bool is_subseq (tree, tree);
static tree maybe_handle_ref_bind (tree *);
static void maybe_handle_implicit_object (tree *);
static struct z_candidate *add_candidate 
        (struct z_candidate **, tree, tree, tree, tree, tree, int);
static tree source_type (tree);
static void add_warning (struct z_candidate *, struct z_candidate *);
static bool reference_related_p (tree, tree);
static bool reference_compatible_p (tree, tree);
static tree convert_class_to_reference (tree, tree, tree);
static tree direct_reference_binding (tree, tree);
static bool promoted_arithmetic_type_p (tree);
static tree conditional_conversion (tree, tree);
static char *name_as_c_string (tree, tree, bool *);
static tree call_builtin_trap (void);
static tree prep_operand (tree);
static void add_candidates (tree, tree, tree, bool, tree, tree,
			    int, struct z_candidate **);
static tree merge_conversion_sequences (tree, tree);
static bool magic_varargs_p (tree);
static tree build_temp (tree, tree, int, void (**)(const char *, ...));
static void check_constructor_callable (tree, tree);

tree
build_vfield_ref (tree datum, tree type)
{
  if (datum == error_mark_node)
    return error_mark_node;

  if (TREE_CODE (TREE_TYPE (datum)) == REFERENCE_TYPE)
    datum = convert_from_reference (datum);

  if (TYPE_BASE_CONVS_MAY_REQUIRE_CODE_P (type)
      && !same_type_ignoring_top_level_qualifiers_p (TREE_TYPE (datum), type))
    datum = convert_to_base (datum, type, /*check_access=*/false);

  return build (COMPONENT_REF, TREE_TYPE (TYPE_VFIELD (type)),
		datum, TYPE_VFIELD (type));
}

/* Returns nonzero iff the destructor name specified in NAME
   (a BIT_NOT_EXPR) matches BASETYPE.  The operand of NAME can take many
   forms...  */

bool
check_dtor_name (tree basetype, tree name)
{
  name = TREE_OPERAND (name, 0);

  /* Just accept something we've already complained about.  */
  if (name == error_mark_node)
    return true;

  if (TREE_CODE (name) == TYPE_DECL)
    name = TREE_TYPE (name);
  else if (TYPE_P (name))
    /* OK */;
  else if (TREE_CODE (name) == IDENTIFIER_NODE)
    {
      if ((IS_AGGR_TYPE (basetype) && name == constructor_name (basetype))
	  || (TREE_CODE (basetype) == ENUMERAL_TYPE
	      && name == TYPE_IDENTIFIER (basetype)))
	name = basetype;
      else
	name = get_type_value (name);
    }
  /* In the case of:
      
       template <class T> struct S { ~S(); };
       int i;
       i.~S();

     NAME will be a class template.  */
  else if (DECL_CLASS_TEMPLATE_P (name))
    return false;
  else
    abort ();

  if (name && TYPE_MAIN_VARIANT (basetype) == TYPE_MAIN_VARIANT (name))
    return true;
  return false;
}

/* We want the address of a function or method.  We avoid creating a
   pointer-to-member function.  */

tree
build_addr_func (tree function)
{
  tree type = TREE_TYPE (function);

  /* We have to do these by hand to avoid real pointer to member
     functions.  */
  if (TREE_CODE (type) == METHOD_TYPE)
    {
      if (TREE_CODE (function) == OFFSET_REF)
	{
	  tree object = build_address (TREE_OPERAND (function, 0));
	  return get_member_function_from_ptrfunc (&object,
						   TREE_OPERAND (function, 1));
	}
      function = build_address (function);
    }
  else
    function = decay_conversion (function);

  return function;
}

/* Build a CALL_EXPR, we can handle FUNCTION_TYPEs, METHOD_TYPEs, or
   POINTER_TYPE to those.  Note, pointer to member function types
   (TYPE_PTRMEMFUNC_P) must be handled by our callers.  */

tree
build_call (tree function, tree parms)
{
  int is_constructor = 0;
  int nothrow;
  tree tmp;
  tree decl;
  tree result_type;
  tree fntype;

  function = build_addr_func (function);

  if (TYPE_PTRMEMFUNC_P (TREE_TYPE (function)))
    {
      sorry ("unable to call pointer to member function here");
      return error_mark_node;
    }

  fntype = TREE_TYPE (TREE_TYPE (function));
  result_type = TREE_TYPE (fntype);

  if (TREE_CODE (function) == ADDR_EXPR
      && TREE_CODE (TREE_OPERAND (function, 0)) == FUNCTION_DECL)
    decl = TREE_OPERAND (function, 0);
  else
    decl = NULL_TREE;

  /* We check both the decl and the type; a function may be known not to
     throw without being declared throw().  */
  nothrow = ((decl && TREE_NOTHROW (decl))
	     || TYPE_NOTHROW_P (TREE_TYPE (TREE_TYPE (function))));

  if (decl && TREE_THIS_VOLATILE (decl) && cfun)
    current_function_returns_abnormally = 1;

  if (decl && TREE_DEPRECATED (decl))
    warn_deprecated_use (decl);
  require_complete_eh_spec_types (fntype, decl);

  if (decl && DECL_CONSTRUCTOR_P (decl))
    is_constructor = 1;

  if (decl && ! TREE_USED (decl))
    {
      /* We invoke build_call directly for several library functions.
	 These may have been declared normally if we're building libgcc,
	 so we can't just check DECL_ARTIFICIAL.  */
      if (DECL_ARTIFICIAL (decl)
	  || !strncmp (IDENTIFIER_POINTER (DECL_NAME (decl)), "__", 2))
	mark_used (decl);
      else
	abort ();
    }

  /* Don't pass empty class objects by value.  This is useful
     for tags in STL, which are used to control overload resolution.
     We don't need to handle other cases of copying empty classes.  */
  if (! decl || ! DECL_BUILT_IN (decl))
    for (tmp = parms; tmp; tmp = TREE_CHAIN (tmp))
      if (is_empty_class (TREE_TYPE (TREE_VALUE (tmp)))
	  && ! TREE_ADDRESSABLE (TREE_TYPE (TREE_VALUE (tmp))))
	{
	  tree t = build (EMPTY_CLASS_EXPR, TREE_TYPE (TREE_VALUE (tmp)));
	  TREE_VALUE (tmp) = build (COMPOUND_EXPR, TREE_TYPE (t),
				    TREE_VALUE (tmp), t);
	}

  function = build (CALL_EXPR, result_type, function, parms);
  TREE_HAS_CONSTRUCTOR (function) = is_constructor;
  TREE_NOTHROW (function) = nothrow;
  
  return function;
}

/* Build something of the form ptr->method (args)
   or object.method (args).  This can also build
   calls to constructors, and find friends.

   Member functions always take their class variable
   as a pointer.

   INSTANCE is a class instance.

   NAME is the name of the method desired, usually an IDENTIFIER_NODE.

   PARMS help to figure out what that NAME really refers to.

   BASETYPE_PATH, if non-NULL, contains a chain from the type of INSTANCE
   down to the real instance type to use for access checking.  We need this
   information to get protected accesses correct.

   FLAGS is the logical disjunction of zero or more LOOKUP_
   flags.  See cp-tree.h for more info.

   If this is all OK, calls build_function_call with the resolved
   member function.

   This function must also handle being called to perform
   initialization, promotion/coercion of arguments, and
   instantiation of default parameters.

   Note that NAME may refer to an instance variable name.  If
   `operator()()' is defined for the type of that field, then we return
   that result.  */

/* New overloading code.  */

struct z_candidate GTY(()) {
  /* The FUNCTION_DECL that will be called if this candidate is
     selected by overload resolution.  */
  tree fn;
  /* The arguments to use when calling this function.  */
  tree args;
  /* The implicit conversion sequences for each of the arguments to
     FN.  */
  tree convs;
  /* If FN is a user-defined conversion, the standard conversion
     sequence from the type returned by FN to the desired destination
     type.  */
  tree second_conv;
  int viable;
  /* If FN is a member function, the binfo indicating the path used to
     qualify the name of FN at the call site.  This path is used to
     determine whether or not FN is accessible if it is selected by
     overload resolution.  The DECL_CONTEXT of FN will always be a
     (possibly improper) base of this binfo.  */
  tree access_path;
  /* If FN is a non-static member function, the binfo indicating the
     subobject to which the `this' pointer should be converted if FN
     is selected by overload resolution.  The type pointed to the by
     the `this' pointer must correspond to the most derived class
     indicated by the CONVERSION_PATH.  */
  tree conversion_path;
  tree template;
  tree warnings;
  struct z_candidate *next;
};

#define IDENTITY_RANK 0
#define EXACT_RANK 1
#define PROMO_RANK 2
#define STD_RANK 3
#define PBOOL_RANK 4
#define USER_RANK 5
#define ELLIPSIS_RANK 6
#define BAD_RANK 7

#define ICS_RANK(NODE)				\
  (ICS_BAD_FLAG (NODE) ? BAD_RANK   		\
   : ICS_ELLIPSIS_FLAG (NODE) ? ELLIPSIS_RANK	\
   : ICS_USER_FLAG (NODE) ? USER_RANK		\
   : ICS_STD_RANK (NODE))

#define ICS_STD_RANK(NODE) TREE_COMPLEXITY (NODE)

#define ICS_USER_FLAG(NODE) TREE_LANG_FLAG_0 (NODE)
#define ICS_ELLIPSIS_FLAG(NODE) TREE_LANG_FLAG_1 (NODE)
#define ICS_THIS_FLAG(NODE) TREE_LANG_FLAG_2 (NODE)
#define ICS_BAD_FLAG(NODE) TREE_LANG_FLAG_3 (NODE)

/* In a REF_BIND or a BASE_CONV, this indicates that a temporary
   should be created to hold the result of the conversion.  */
#define NEED_TEMPORARY_P(NODE) TREE_LANG_FLAG_4 (NODE)

/* TRUE in an IDENTITY_CONV or BASE_CONV if the copy constructor must
   be accessible, even though it is not being used.  */
#define CHECK_COPY_CONSTRUCTOR_P(NODE) TREE_LANG_FLAG_5 (NODE)

#define USER_CONV_CAND(NODE) WRAPPER_ZC (TREE_OPERAND (NODE, 1))
#define USER_CONV_FN(NODE) (USER_CONV_CAND (NODE)->fn)

/* Returns true iff T is a null pointer constant in the sense of
   [conv.ptr].  */

bool
null_ptr_cst_p (tree t)
{
  /* [conv.ptr]

     A null pointer constant is an integral constant expression
     (_expr.const_) rvalue of integer type that evaluates to zero.  */
  if (DECL_INTEGRAL_CONSTANT_VAR_P (t))
    t = decl_constant_value (t);
  if (t == null_node
      || (CP_INTEGRAL_TYPE_P (TREE_TYPE (t)) && integer_zerop (t)))
    return true;
  return false;
}


/* Returns nonzero if PARMLIST consists of only default parms and/or
   ellipsis.  */

bool
sufficient_parms_p (tree parmlist)
{
  for (; parmlist && parmlist != void_list_node;
       parmlist = TREE_CHAIN (parmlist))
    if (!TREE_PURPOSE (parmlist))
      return false;
  return true;
}

static tree
build_conv (enum tree_code code, tree type, tree from)
{
  tree t;
  int rank = ICS_STD_RANK (from);

  /* We can't use buildl1 here because CODE could be USER_CONV, which
     takes two arguments.  In that case, the caller is responsible for
     filling in the second argument.  */
  t = make_node (code);
  TREE_TYPE (t) = type;
  TREE_OPERAND (t, 0) = from;

  switch (code)
    {
    case PTR_CONV:
    case PMEM_CONV:
    case BASE_CONV:
    case STD_CONV:
      if (rank < STD_RANK)
	rank = STD_RANK;
      break;

    case QUAL_CONV:
      if (rank < EXACT_RANK)
	rank = EXACT_RANK;

    default:
      break;
    }
  ICS_STD_RANK (t) = rank;
  ICS_USER_FLAG (t) = (code == USER_CONV || ICS_USER_FLAG (from));
  ICS_BAD_FLAG (t) = ICS_BAD_FLAG (from);
  return t;
}

tree
strip_top_quals (tree t)
{
  if (TREE_CODE (t) == ARRAY_TYPE)
    return t;
  return cp_build_qualified_type (t, 0);
}

/* Returns the standard conversion path (see [conv]) from type FROM to type
   TO, if any.  For proper handling of null pointer constants, you must
   also pass the expression EXPR to convert from.  */

static tree
standard_conversion (tree to, tree from, tree expr, int flags)
{
  enum tree_code fcode, tcode;
  tree conv;
  bool fromref = false;

  to = non_reference (to);
  if (TREE_CODE (from) == REFERENCE_TYPE)
    {
      fromref = true;
      from = TREE_TYPE (from);
    }
  to = strip_top_quals (to);
  from = strip_top_quals (from);

  if ((TYPE_PTRFN_P (to) || TYPE_PTRMEMFUNC_P (to))
      && expr && type_unknown_p (expr))
    {
      expr = instantiate_type (to, expr, tf_conv);
      if (expr == error_mark_node)
	return NULL_TREE;
      from = TREE_TYPE (expr);
    }

  fcode = TREE_CODE (from);
  tcode = TREE_CODE (to);

  conv = build1 (IDENTITY_CONV, from, expr);

  if (fcode == FUNCTION_TYPE)
    {
      from = build_pointer_type (from);
      fcode = TREE_CODE (from);
      conv = build_conv (LVALUE_CONV, from, conv);
    }
  else if (fcode == ARRAY_TYPE)
    {
      from = build_pointer_type (TREE_TYPE (from));
      fcode = TREE_CODE (from);
      conv = build_conv (LVALUE_CONV, from, conv);
    }
  else if (fromref || (expr && lvalue_p (expr)))
    conv = build_conv (RVALUE_CONV, from, conv);

   /* Allow conversion between `__complex__' data types.  */
  if (tcode == COMPLEX_TYPE && fcode == COMPLEX_TYPE)
    {
      /* The standard conversion sequence to convert FROM to TO is
         the standard conversion sequence to perform componentwise
         conversion.  */
      tree part_conv = standard_conversion
        (TREE_TYPE (to), TREE_TYPE (from), NULL_TREE, flags);
      
      if (part_conv)
        {
          conv = build_conv (TREE_CODE (part_conv), to, conv);
          ICS_STD_RANK (conv) = ICS_STD_RANK (part_conv);
        }
      else
        conv = NULL_TREE;

      return conv;
    }

  if (same_type_p (from, to))
    return conv;

  if ((tcode == POINTER_TYPE || TYPE_PTR_TO_MEMBER_P (to))
      && expr && null_ptr_cst_p (expr))
    conv = build_conv (STD_CONV, to, conv);
  else if (tcode == POINTER_TYPE && fcode == POINTER_TYPE
	   && TREE_CODE (TREE_TYPE (to)) == VECTOR_TYPE
	   && TREE_CODE (TREE_TYPE (from)) == VECTOR_TYPE
	   && ((*targetm.vector_opaque_p) (TREE_TYPE (to))
	       || (*targetm.vector_opaque_p) (TREE_TYPE (from))))
    conv = build_conv (STD_CONV, to, conv);
  else if ((tcode == INTEGER_TYPE && fcode == POINTER_TYPE)
	   || (tcode == POINTER_TYPE && fcode == INTEGER_TYPE))
    {
      /* For backwards brain damage compatibility, allow interconversion of
	 pointers and integers with a pedwarn.  */
      conv = build_conv (STD_CONV, to, conv);
      ICS_BAD_FLAG (conv) = 1;
    }
  else if (tcode == ENUMERAL_TYPE && fcode == INTEGER_TYPE)
    {
      /* For backwards brain damage compatibility, allow interconversion of
	 enums and integers with a pedwarn.  */
      conv = build_conv (STD_CONV, to, conv);
      ICS_BAD_FLAG (conv) = 1;
    }
  else if ((tcode == POINTER_TYPE && fcode == POINTER_TYPE)
	   || (TYPE_PTRMEM_P (to) && TYPE_PTRMEM_P (from)))
    {
      tree to_pointee;
      tree from_pointee;

      if (tcode == POINTER_TYPE
	  && same_type_ignoring_top_level_qualifiers_p (TREE_TYPE (from),
							TREE_TYPE (to)))
	;
      else if (VOID_TYPE_P (TREE_TYPE (to))
	       && !TYPE_PTRMEM_P (from)
	       && TREE_CODE (TREE_TYPE (from)) != FUNCTION_TYPE)
	{
	  from = build_pointer_type
	    (cp_build_qualified_type (void_type_node, 
				      cp_type_quals (TREE_TYPE (from))));
	  conv = build_conv (PTR_CONV, from, conv);
	}
      else if (TYPE_PTRMEM_P (from))
	{
	  tree fbase = TYPE_PTRMEM_CLASS_TYPE (from);
	  tree tbase = TYPE_PTRMEM_CLASS_TYPE (to);

	  if (DERIVED_FROM_P (fbase, tbase)
	      && (same_type_ignoring_top_level_qualifiers_p
		  (TYPE_PTRMEM_POINTED_TO_TYPE (from),
		   TYPE_PTRMEM_POINTED_TO_TYPE (to))))
	    {
	      from = build_ptrmem_type (tbase, 
					TYPE_PTRMEM_POINTED_TO_TYPE (from));
	      conv = build_conv (PMEM_CONV, from, conv);
	    }
	  else if (!same_type_p (fbase, tbase))
	    return NULL;
	}
      else if (IS_AGGR_TYPE (TREE_TYPE (from))
	       && IS_AGGR_TYPE (TREE_TYPE (to))
	       /* [conv.ptr]
		  
	          An rvalue of type "pointer to cv D," where D is a
		  class type, can be converted to an rvalue of type
		  "pointer to cv B," where B is a base class (clause
		  _class.derived_) of D.  If B is an inaccessible
		  (clause _class.access_) or ambiguous
		  (_class.member.lookup_) base class of D, a program
		  that necessitates this conversion is ill-formed.  */
	       /* Therefore, we use DERIVED_FROM_P, and not
		  ACESSIBLY_UNIQUELY_DERIVED_FROM_P, in this test.  */
	       && DERIVED_FROM_P (TREE_TYPE (to), TREE_TYPE (from)))
	{
	  from = 
	    cp_build_qualified_type (TREE_TYPE (to),
				     cp_type_quals (TREE_TYPE (from)));
	  from = build_pointer_type (from);
	  conv = build_conv (PTR_CONV, from, conv);
	}

      if (tcode == POINTER_TYPE)
	{
	  to_pointee = TREE_TYPE (to);
	  from_pointee = TREE_TYPE (from);
	}
      else
	{
	  to_pointee = TYPE_PTRMEM_POINTED_TO_TYPE (to);
	  from_pointee = TYPE_PTRMEM_POINTED_TO_TYPE (from);
	}

      if (same_type_p (from, to))
	/* OK */;
      else if (comp_ptr_ttypes (to_pointee, from_pointee))
	conv = build_conv (QUAL_CONV, to, conv);
      else if (expr && string_conv_p (to, expr, 0))
	/* converting from string constant to char *.  */
	conv = build_conv (QUAL_CONV, to, conv);
      else if (ptr_reasonably_similar (to_pointee, from_pointee))
	{
	  conv = build_conv (PTR_CONV, to, conv);
	  ICS_BAD_FLAG (conv) = 1;
	}
      else
	return 0;

      from = to;
    }
  else if (TYPE_PTRMEMFUNC_P (to) && TYPE_PTRMEMFUNC_P (from))
    {
      tree fromfn = TREE_TYPE (TYPE_PTRMEMFUNC_FN_TYPE (from));
      tree tofn = TREE_TYPE (TYPE_PTRMEMFUNC_FN_TYPE (to));
      tree fbase = TREE_TYPE (TREE_VALUE (TYPE_ARG_TYPES (fromfn)));
      tree tbase = TREE_TYPE (TREE_VALUE (TYPE_ARG_TYPES (tofn)));

      if (!DERIVED_FROM_P (fbase, tbase)
	  || !same_type_p (TREE_TYPE (fromfn), TREE_TYPE (tofn))
	  || !compparms (TREE_CHAIN (TYPE_ARG_TYPES (fromfn)),
			 TREE_CHAIN (TYPE_ARG_TYPES (tofn)))
	  || cp_type_quals (fbase) != cp_type_quals (tbase))
	return 0;

      from = cp_build_qualified_type (tbase, cp_type_quals (fbase));
      from = build_method_type_directly (from, 
					 TREE_TYPE (fromfn),
					 TREE_CHAIN (TYPE_ARG_TYPES (fromfn)));
      from = build_ptrmemfunc_type (build_pointer_type (from));
      conv = build_conv (PMEM_CONV, from, conv);
    }
  else if (tcode == BOOLEAN_TYPE)
    {
      /* [conv.bool]

          An rvalue of arithmetic, enumeration, pointer, or pointer to
	  member type can be converted to an rvalue of type bool.  */
      if (ARITHMETIC_TYPE_P (from)
	  || fcode == ENUMERAL_TYPE
	  || fcode == POINTER_TYPE
	  || TYPE_PTR_TO_MEMBER_P (from))
	{
	  conv = build_conv (STD_CONV, to, conv);
	  if (fcode == POINTER_TYPE
	      || TYPE_PTRMEM_P (from)
	      || (TYPE_PTRMEMFUNC_P (from) 
		  && ICS_STD_RANK (conv) < PBOOL_RANK))
	    ICS_STD_RANK (conv) = PBOOL_RANK;
	  return conv;
	}
      
      return NULL_TREE;
    }
  /* We don't check for ENUMERAL_TYPE here because there are no standard
     conversions to enum type.  */
  else if (tcode == INTEGER_TYPE || tcode == BOOLEAN_TYPE
	   || tcode == REAL_TYPE)
    {
      if (! (INTEGRAL_CODE_P (fcode) || fcode == REAL_TYPE))
	return 0;
      conv = build_conv (STD_CONV, to, conv);

      /* Give this a better rank if it's a promotion.  */
      if (same_type_p (to, type_promotes_to (from))
	  && ICS_STD_RANK (TREE_OPERAND (conv, 0)) <= PROMO_RANK)
	ICS_STD_RANK (conv) = PROMO_RANK;
    }
  else if (fcode == VECTOR_TYPE && tcode == VECTOR_TYPE
      && ((*targetm.vector_opaque_p) (from)
	  || (*targetm.vector_opaque_p) (to)))
    return build_conv (STD_CONV, to, conv);
  else if (!(flags & LOOKUP_CONSTRUCTOR_CALLABLE)
	   && IS_AGGR_TYPE (to) && IS_AGGR_TYPE (from)
	   && is_properly_derived_from (from, to))
    {
      if (TREE_CODE (conv) == RVALUE_CONV)
	conv = TREE_OPERAND (conv, 0);
      conv = build_conv (BASE_CONV, to, conv);
      /* The derived-to-base conversion indicates the initialization
	 of a parameter with base type from an object of a derived
	 type.  A temporary object is created to hold the result of
	 the conversion.  */
      NEED_TEMPORARY_P (conv) = 1;
    }
  else
    return 0;

  return conv;
}

/* Returns nonzero if T1 is reference-related to T2.  */

static bool
reference_related_p (tree t1, tree t2)
{
  t1 = TYPE_MAIN_VARIANT (t1);
  t2 = TYPE_MAIN_VARIANT (t2);

  /* [dcl.init.ref]

     Given types "cv1 T1" and "cv2 T2," "cv1 T1" is reference-related
     to "cv2 T2" if T1 is the same type as T2, or T1 is a base class
     of T2.  */
  return (same_type_p (t1, t2)
	  || (CLASS_TYPE_P (t1) && CLASS_TYPE_P (t2)
	      && DERIVED_FROM_P (t1, t2)));
}

/* Returns nonzero if T1 is reference-compatible with T2.  */

static bool
reference_compatible_p (tree t1, tree t2)
{
  /* [dcl.init.ref]

     "cv1 T1" is reference compatible with "cv2 T2" if T1 is
     reference-related to T2 and cv1 is the same cv-qualification as,
     or greater cv-qualification than, cv2.  */
  return (reference_related_p (t1, t2)
	  && at_least_as_qualified_p (t1, t2));
}

/* Determine whether or not the EXPR (of class type S) can be
   converted to T as in [over.match.ref].  */

static tree
convert_class_to_reference (tree t, tree s, tree expr)
{
  tree conversions;
  tree arglist;
  tree conv;
  tree reference_type;
  struct z_candidate *candidates;
  struct z_candidate *cand;
  bool any_viable_p;

  conversions = lookup_conversions (s);
  if (!conversions)
    return NULL_TREE;

  /* [over.match.ref]

     Assuming that "cv1 T" is the underlying type of the reference
     being initialized, and "cv S" is the type of the initializer
     expression, with S a class type, the candidate functions are
     selected as follows:

     --The conversion functions of S and its base classes are
       considered.  Those that are not hidden within S and yield type
       "reference to cv2 T2", where "cv1 T" is reference-compatible
       (_dcl.init.ref_) with "cv2 T2", are candidate functions.

     The argument list has one argument, which is the initializer
     expression.  */

  candidates = 0;

  /* Conceptually, we should take the address of EXPR and put it in
     the argument list.  Unfortunately, however, that can result in
     error messages, which we should not issue now because we are just
     trying to find a conversion operator.  Therefore, we use NULL,
     cast to the appropriate type.  */
  arglist = build_int_2 (0, 0);
  TREE_TYPE (arglist) = build_pointer_type (s);
  arglist = build_tree_list (NULL_TREE, arglist);

  reference_type = build_reference_type (t);

  while (conversions)
    {
      tree fns = TREE_VALUE (conversions);

      for (; fns; fns = OVL_NEXT (fns))
	{
	  tree f = OVL_CURRENT (fns);
	  tree t2 = TREE_TYPE (TREE_TYPE (f));
	  
	  cand = NULL;

	  /* If this is a template function, try to get an exact
             match.  */
	  if (TREE_CODE (f) == TEMPLATE_DECL)
	    {
	      cand = add_template_candidate (&candidates,
					     f, s,
					     NULL_TREE,
					     arglist,
					     reference_type,
					     TYPE_BINFO (s),
					     TREE_PURPOSE (conversions),
					     LOOKUP_NORMAL,
					     DEDUCE_CONV);
	      
	      if (cand)
		{
		  /* Now, see if the conversion function really returns
		     an lvalue of the appropriate type.  From the
		     point of view of unification, simply returning an
		     rvalue of the right type is good enough.  */
		  f = cand->fn;
		  t2 = TREE_TYPE (TREE_TYPE (f));
		  if (TREE_CODE (t2) != REFERENCE_TYPE
		      || !reference_compatible_p (t, TREE_TYPE (t2)))
		    {
		      candidates = candidates->next;
		      cand = NULL;
		    }
		}
	    }
	  else if (TREE_CODE (t2) == REFERENCE_TYPE
		   && reference_compatible_p (t, TREE_TYPE (t2)))
	    cand = add_function_candidate (&candidates, f, s, arglist, 
					   TYPE_BINFO (s), 	
					   TREE_PURPOSE (conversions),
					   LOOKUP_NORMAL);
	  
	  if (cand)
	    {
	      /* Build a standard conversion sequence indicating the
		 binding from the reference type returned by the
		 function to the desired REFERENCE_TYPE.  */
	      cand->second_conv
		= (direct_reference_binding 
		   (reference_type, 
		    build1 (IDENTITY_CONV, 
			    TREE_TYPE (TREE_TYPE (TREE_TYPE (cand->fn))),
			    NULL_TREE)));
	      ICS_BAD_FLAG (cand->second_conv) 
		|= ICS_BAD_FLAG (TREE_VEC_ELT (cand->convs, 0));
	    }
	}
      conversions = TREE_CHAIN (conversions);
    }

  candidates = splice_viable (candidates, pedantic, &any_viable_p);
  /* If none of the conversion functions worked out, let our caller
     know.  */
  if (!any_viable_p)
    return NULL_TREE;

  cand = tourney (candidates);
  if (!cand)
    return NULL_TREE;

  /* Now that we know that this is the function we're going to use fix
     the dummy first argument.  */
  cand->args = tree_cons (NULL_TREE,
			  build_this (expr),
			  TREE_CHAIN (cand->args));

  /* Build a user-defined conversion sequence representing the
     conversion.  */
  conv = build_conv (USER_CONV,
		     TREE_TYPE (TREE_TYPE (cand->fn)),
		     build1 (IDENTITY_CONV, TREE_TYPE (expr), expr));
  TREE_OPERAND (conv, 1) = build_zc_wrapper (cand);

  /* Merge it with the standard conversion sequence from the
     conversion function's return type to the desired type.  */
  cand->second_conv = merge_conversion_sequences (conv, cand->second_conv);

  if (cand->viable == -1)
    ICS_BAD_FLAG (conv) = 1;
  
  return cand->second_conv;
}

/* A reference of the indicated TYPE is being bound directly to the
   expression represented by the implicit conversion sequence CONV.
   Return a conversion sequence for this binding.  */

static tree
direct_reference_binding (tree type, tree conv)
{
  tree t;

  my_friendly_assert (TREE_CODE (type) == REFERENCE_TYPE, 20030306);
  my_friendly_assert (TREE_CODE (TREE_TYPE (conv)) != REFERENCE_TYPE,
		      20030306);

  t = TREE_TYPE (type);

  /* [over.ics.rank] 
     
     When a parameter of reference type binds directly
     (_dcl.init.ref_) to an argument expression, the implicit
     conversion sequence is the identity conversion, unless the
     argument expression has a type that is a derived class of the
     parameter type, in which case the implicit conversion sequence is
     a derived-to-base Conversion.
	 
     If the parameter binds directly to the result of applying a
     conversion function to the argument expression, the implicit
     conversion sequence is a user-defined conversion sequence
     (_over.ics.user_), with the second standard conversion sequence
     either an identity conversion or, if the conversion function
     returns an entity of a type that is a derived class of the
     parameter type, a derived-to-base conversion.  */
  if (!same_type_ignoring_top_level_qualifiers_p (t, TREE_TYPE (conv)))
    {
      /* Represent the derived-to-base conversion.  */
      conv = build_conv (BASE_CONV, t, conv);
      /* We will actually be binding to the base-class subobject in
	 the derived class, so we mark this conversion appropriately.
	 That way, convert_like knows not to generate a temporary.  */
      NEED_TEMPORARY_P (conv) = 0;
    }
  return build_conv (REF_BIND, type, conv);
}

/* Returns the conversion path from type FROM to reference type TO for
   purposes of reference binding.  For lvalue binding, either pass a
   reference type to FROM or an lvalue expression to EXPR.  If the
   reference will be bound to a temporary, NEED_TEMPORARY_P is set for
   the conversion returned.  */

static tree
reference_binding (tree rto, tree rfrom, tree expr, int flags)
{
  tree conv = NULL_TREE;
  tree to = TREE_TYPE (rto);
  tree from = rfrom;
  bool related_p;
  bool compatible_p;
  cp_lvalue_kind lvalue_p = clk_none;

  if (TREE_CODE (to) == FUNCTION_TYPE && expr && type_unknown_p (expr))
    {
      expr = instantiate_type (to, expr, tf_none);
      if (expr == error_mark_node)
	return NULL_TREE;
      from = TREE_TYPE (expr);
    }

  if (TREE_CODE (from) == REFERENCE_TYPE)
    {
      /* Anything with reference type is an lvalue.  */
      lvalue_p = clk_ordinary;
      from = TREE_TYPE (from);
    }
  else if (expr)
    lvalue_p = real_lvalue_p (expr);

  /* Figure out whether or not the types are reference-related and
     reference compatible.  We have do do this after stripping
     references from FROM.  */
  related_p = reference_related_p (to, from);
  compatible_p = reference_compatible_p (to, from);

  if (lvalue_p && compatible_p)
    {
      /* [dcl.init.ref]

	 If the initializer expression 
	 
	 -- is an lvalue (but not an lvalue for a bit-field), and "cv1 T1"
	    is reference-compatible with "cv2 T2,"
	 
	 the reference is bound directly to the initializer expression
	 lvalue.  */
      conv = build1 (IDENTITY_CONV, from, expr);
      conv = direct_reference_binding (rto, conv);
      if ((lvalue_p & clk_bitfield) != 0
	  || ((lvalue_p & clk_packed) != 0 && !TYPE_PACKED (to)))
	/* For the purposes of overload resolution, we ignore the fact
	   this expression is a bitfield or packed field. (In particular,
	   [over.ics.ref] says specifically that a function with a
	   non-const reference parameter is viable even if the
	   argument is a bitfield.)

	   However, when we actually call the function we must create
	   a temporary to which to bind the reference.  If the
	   reference is volatile, or isn't const, then we cannot make
	   a temporary, so we just issue an error when the conversion
	   actually occurs.  */
	NEED_TEMPORARY_P (conv) = 1;
					
      return conv;
    }
  else if (CLASS_TYPE_P (from) && !(flags & LOOKUP_NO_CONVERSION))
    {
      /* [dcl.init.ref]

	 If the initializer expression

	 -- has a class type (i.e., T2 is a class type) can be
	    implicitly converted to an lvalue of type "cv3 T3," where
	    "cv1 T1" is reference-compatible with "cv3 T3".  (this
	    conversion is selected by enumerating the applicable
	    conversion functions (_over.match.ref_) and choosing the
	    best one through overload resolution.  (_over.match_). 

        the reference is bound to the lvalue result of the conversion
	in the second case.  */
      conv = convert_class_to_reference (to, from, expr);
      if (conv)
	return conv;
    }

  /* From this point on, we conceptually need temporaries, even if we
     elide them.  Only the cases above are "direct bindings".  */
  if (flags & LOOKUP_NO_TEMP_BIND)
    return NULL_TREE;

  /* [over.ics.rank]
     
     When a parameter of reference type is not bound directly to an
     argument expression, the conversion sequence is the one required
     to convert the argument expression to the underlying type of the
     reference according to _over.best.ics_.  Conceptually, this
     conversion sequence corresponds to copy-initializing a temporary
     of the underlying type with the argument expression.  Any
     difference in top-level cv-qualification is subsumed by the
     initialization itself and does not constitute a conversion.  */

  /* [dcl.init.ref]

     Otherwise, the reference shall be to a non-volatile const type.  */
  if (!CP_TYPE_CONST_NON_VOLATILE_P (to))
    return NULL_TREE;

  /* [dcl.init.ref]
     
     If the initializer expression is an rvalue, with T2 a class type,
     and "cv1 T1" is reference-compatible with "cv2 T2", the reference
     is bound in one of the following ways:
     
     -- The reference is bound to the object represented by the rvalue
        or to a sub-object within that object.  

     -- ...
	
     We use the first alternative.  The implicit conversion sequence
     is supposed to be same as we would obtain by generating a
     temporary.  Fortunately, if the types are reference compatible,
     then this is either an identity conversion or the derived-to-base
     conversion, just as for direct binding.  */
  if (CLASS_TYPE_P (from) && compatible_p)
    {
      conv = build1 (IDENTITY_CONV, from, expr);
      conv = direct_reference_binding (rto, conv);
      if (!(flags & LOOKUP_CONSTRUCTOR_CALLABLE))
	CHECK_COPY_CONSTRUCTOR_P (TREE_OPERAND (conv, 0)) = 1;
      return conv;
    }

  /* [dcl.init.ref]

     Otherwise, a temporary of type "cv1 T1" is created and
     initialized from the initializer expression using the rules for a
     non-reference copy initialization.  If T1 is reference-related to
     T2, cv1 must be the same cv-qualification as, or greater
     cv-qualification than, cv2; otherwise, the program is ill-formed.  */
  if (related_p && !at_least_as_qualified_p (to, from))
    return NULL_TREE;

  conv = implicit_conversion (to, from, expr, flags);
  if (!conv)
    return NULL_TREE;

  conv = build_conv (REF_BIND, rto, conv);
  /* This reference binding, unlike those above, requires the
     creation of a temporary.  */
  NEED_TEMPORARY_P (conv) = 1;

  return conv;
}

/* Returns the implicit conversion sequence (see [over.ics]) from type FROM
   to type TO.  The optional expression EXPR may affect the conversion.
   FLAGS are the usual overloading flags.  Only LOOKUP_NO_CONVERSION is
   significant.  */

static tree
implicit_conversion (tree to, tree from, tree expr, int flags)
{
  tree conv;

  if (from == error_mark_node || to == error_mark_node
      || expr == error_mark_node)
    return NULL_TREE;

  if (TREE_CODE (to) == REFERENCE_TYPE)
    conv = reference_binding (to, from, expr, flags);
  else
    conv = standard_conversion (to, from, expr, flags);

  if (conv)
    return conv;

  if (expr != NULL_TREE
      && (IS_AGGR_TYPE (from)
	  || IS_AGGR_TYPE (to))
      && (flags & LOOKUP_NO_CONVERSION) == 0)
    {
      struct z_candidate *cand;

      cand = build_user_type_conversion_1
	(to, expr, LOOKUP_ONLYCONVERTING);
      if (cand)
	conv = cand->second_conv;

      /* We used to try to bind a reference to a temporary here, but that
	 is now handled by the recursive call to this function at the end
	 of reference_binding.  */
      return conv;
    }

  return NULL_TREE;
}

/* Add a new entry to the list of candidates.  Used by the add_*_candidate
   functions.  */

static struct z_candidate *
add_candidate (struct z_candidate **candidates, 
	       tree fn, tree args, tree convs, tree access_path, 
	       tree conversion_path, int viable)
{
  struct z_candidate *cand = ggc_alloc_cleared (sizeof (struct z_candidate));

  cand->fn = fn;
  cand->args = args;
  cand->convs = convs;
  cand->access_path = access_path;
  cand->conversion_path = conversion_path;
  cand->viable = viable;
  cand->next = *candidates;
  *candidates = cand;

  return cand;
}

/* Create an overload candidate for the function or method FN called with
   the argument list ARGLIST and add it to CANDIDATES.  FLAGS is passed on
   to implicit_conversion.

   CTYPE, if non-NULL, is the type we want to pretend this function
   comes from for purposes of overload resolution.  */

static struct z_candidate *
add_function_candidate (struct z_candidate **candidates, 
			tree fn, tree ctype, tree arglist, 
			tree access_path, tree conversion_path,
			int flags)
{
  tree parmlist = TYPE_ARG_TYPES (TREE_TYPE (fn));
  int i, len;
  tree convs;
  tree parmnode, argnode;
  tree orig_arglist;
  int viable = 1;

  /* Built-in functions that haven't been declared don't really
     exist.  */
  if (DECL_ANTICIPATED (fn))
    return NULL;

  /* The `this', `in_chrg' and VTT arguments to constructors are not
     considered in overload resolution.  */
  if (DECL_CONSTRUCTOR_P (fn))
    {
      parmlist = skip_artificial_parms_for (fn, parmlist);
      orig_arglist = arglist;
      arglist = skip_artificial_parms_for (fn, arglist);
    }
  else 
    orig_arglist = arglist;

  len = list_length (arglist);
  convs = make_tree_vec (len);

  /* 13.3.2 - Viable functions [over.match.viable]
     First, to be a viable function, a candidate function shall have enough
     parameters to agree in number with the arguments in the list.

     We need to check this first; otherwise, checking the ICSes might cause
     us to produce an ill-formed template instantiation.  */

  parmnode = parmlist;
  for (i = 0; i < len; ++i)
    {
      if (parmnode == NULL_TREE || parmnode == void_list_node)
	break;
      parmnode = TREE_CHAIN (parmnode);
    }

  if (i < len && parmnode)
    viable = 0;

  /* Make sure there are default args for the rest of the parms.  */
  else if (!sufficient_parms_p (parmnode))
    viable = 0;

  if (! viable)
    goto out;

  /* Second, for F to be a viable function, there shall exist for each
     argument an implicit conversion sequence that converts that argument
     to the corresponding parameter of F.  */

  parmnode = parmlist;
  argnode = arglist;

  for (i = 0; i < len; ++i)
    {
      tree arg = TREE_VALUE (argnode);
      tree argtype = lvalue_type (arg);
      tree t;
      int is_this;

      if (parmnode == void_list_node)
	break;

      is_this = (i == 0 && DECL_NONSTATIC_MEMBER_FUNCTION_P (fn)
		 && ! DECL_CONSTRUCTOR_P (fn));

      if (parmnode)
	{
	  tree parmtype = TREE_VALUE (parmnode);

	  /* The type of the implicit object parameter ('this') for
	     overload resolution is not always the same as for the
	     function itself; conversion functions are considered to
	     be members of the class being converted, and functions
	     introduced by a using-declaration are considered to be
	     members of the class that uses them.

	     Since build_over_call ignores the ICS for the `this'
	     parameter, we can just change the parm type.  */
	  if (ctype && is_this)
	    {
	      parmtype
		= build_qualified_type (ctype,
					TYPE_QUALS (TREE_TYPE (parmtype)));
	      parmtype = build_pointer_type (parmtype);
	    }

	  t = implicit_conversion (parmtype, argtype, arg, flags);
	}
      else
	{
	  t = build1 (IDENTITY_CONV, argtype, arg);
	  ICS_ELLIPSIS_FLAG (t) = 1;
	}

      if (t && is_this)
	ICS_THIS_FLAG (t) = 1;

      TREE_VEC_ELT (convs, i) = t;
      if (! t)
	{
	  viable = 0;
	  break;
	}

      if (ICS_BAD_FLAG (t))
	viable = -1;

      if (parmnode)
	parmnode = TREE_CHAIN (parmnode);
      argnode = TREE_CHAIN (argnode);
    }

 out:
  return add_candidate (candidates, fn, orig_arglist, convs, access_path,
			conversion_path, viable);
}

/* Create an overload candidate for the conversion function FN which will
   be invoked for expression OBJ, producing a pointer-to-function which
   will in turn be called with the argument list ARGLIST, and add it to
   CANDIDATES.  FLAGS is passed on to implicit_conversion.

   Actually, we don't really care about FN; we care about the type it
   converts to.  There may be multiple conversion functions that will
   convert to that type, and we rely on build_user_type_conversion_1 to
   choose the best one; so when we create our candidate, we record the type
   instead of the function.  */

static struct z_candidate *
add_conv_candidate (struct z_candidate **candidates, tree fn, tree obj,
                    tree arglist, tree access_path, tree conversion_path)
{
  tree totype = TREE_TYPE (TREE_TYPE (fn));
  int i, len, viable, flags;
  tree parmlist, convs, parmnode, argnode;

  for (parmlist = totype; TREE_CODE (parmlist) != FUNCTION_TYPE; )
    parmlist = TREE_TYPE (parmlist);
  parmlist = TYPE_ARG_TYPES (parmlist);

  len = list_length (arglist) + 1;
  convs = make_tree_vec (len);
  parmnode = parmlist;
  argnode = arglist;
  viable = 1;
  flags = LOOKUP_NORMAL;

  /* Don't bother looking up the same type twice.  */
  if (*candidates && (*candidates)->fn == totype)
    return NULL;

  for (i = 0; i < len; ++i)
    {
      tree arg = i == 0 ? obj : TREE_VALUE (argnode);
      tree argtype = lvalue_type (arg);
      tree t;

      if (i == 0)
	t = implicit_conversion (totype, argtype, arg, flags);
      else if (parmnode == void_list_node)
	break;
      else if (parmnode)
	t = implicit_conversion (TREE_VALUE (parmnode), argtype, arg, flags);
      else
	{
	  t = build1 (IDENTITY_CONV, argtype, arg);
	  ICS_ELLIPSIS_FLAG (t) = 1;
	}

      TREE_VEC_ELT (convs, i) = t;
      if (! t)
	break;

      if (ICS_BAD_FLAG (t))
	viable = -1;

      if (i == 0)
	continue;

      if (parmnode)
	parmnode = TREE_CHAIN (parmnode);
      argnode = TREE_CHAIN (argnode);
    }

  if (i < len)
    viable = 0;

  if (!sufficient_parms_p (parmnode))
    viable = 0;

  return add_candidate (candidates, totype, arglist, convs, access_path,
			conversion_path, viable);
}

static void
build_builtin_candidate (struct z_candidate **candidates, tree fnname,
                         tree type1, tree type2, tree *args, tree *argtypes,
                         int flags)
{
  tree t, convs;
  int viable = 1, i;
  tree types[2];

  types[0] = type1;
  types[1] = type2;

  convs = make_tree_vec (args[2] ? 3 : (args[1] ? 2 : 1));

  for (i = 0; i < 2; ++i)
    {
      if (! args[i])
	break;

      t = implicit_conversion (types[i], argtypes[i], args[i], flags);
      if (! t)
	{
	  viable = 0;
	  /* We need something for printing the candidate.  */
	  t = build1 (IDENTITY_CONV, types[i], NULL_TREE);
	}
      else if (ICS_BAD_FLAG (t))
	viable = 0;
      TREE_VEC_ELT (convs, i) = t;
    }

  /* For COND_EXPR we rearranged the arguments; undo that now.  */
  if (args[2])
    {
      TREE_VEC_ELT (convs, 2) = TREE_VEC_ELT (convs, 1);
      TREE_VEC_ELT (convs, 1) = TREE_VEC_ELT (convs, 0);
      t = implicit_conversion (boolean_type_node, argtypes[2], args[2], flags);
      if (t)
	TREE_VEC_ELT (convs, 0) = t;
      else
	viable = 0;
    }      

  add_candidate (candidates, fnname, /*args=*/NULL_TREE, convs, 
		 /*access_path=*/NULL_TREE,
		 /*conversion_path=*/NULL_TREE,
		 viable);
}

static bool
is_complete (tree t)
{
  return COMPLETE_TYPE_P (complete_type (t));
}

/* Returns nonzero if TYPE is a promoted arithmetic type.  */

static bool
promoted_arithmetic_type_p (tree type)
{
  /* [over.built]

     In this section, the term promoted integral type is used to refer
     to those integral types which are preserved by integral promotion
     (including e.g.  int and long but excluding e.g.  char).
     Similarly, the term promoted arithmetic type refers to promoted
     integral types plus floating types.  */
  return ((INTEGRAL_TYPE_P (type)
	   && same_type_p (type_promotes_to (type), type))
	  || TREE_CODE (type) == REAL_TYPE);
}

/* Create any builtin operator overload candidates for the operator in
   question given the converted operand types TYPE1 and TYPE2.  The other
   args are passed through from add_builtin_candidates to
   build_builtin_candidate.  
   
   TYPE1 and TYPE2 may not be permissible, and we must filter them. 
   If CODE is requires candidates operands of the same type of the kind
   of which TYPE1 and TYPE2 are, we add both candidates
   CODE (TYPE1, TYPE1) and CODE (TYPE2, TYPE2).  */

static void
add_builtin_candidate (struct z_candidate **candidates, enum tree_code code,
                       enum tree_code code2, tree fnname, tree type1,
                       tree type2, tree *args, tree *argtypes, int flags)
{
  switch (code)
    {
    case POSTINCREMENT_EXPR:
    case POSTDECREMENT_EXPR:
      args[1] = integer_zero_node;
      type2 = integer_type_node;
      break;
    default:
      break;
    }

  switch (code)
    {

/* 4 For every pair T, VQ), where T is an arithmetic or  enumeration  type,
     and  VQ  is  either  volatile or empty, there exist candidate operator
     functions of the form
	     VQ T&   operator++(VQ T&);
	     T       operator++(VQ T&, int);
   5 For every pair T, VQ), where T is an enumeration type or an arithmetic
     type  other than bool, and VQ is either volatile or empty, there exist
     candidate operator functions of the form
	     VQ T&   operator--(VQ T&);
	     T       operator--(VQ T&, int);
   6 For every pair T, VQ), where T is  a  cv-qualified  or  cv-unqualified
     complete  object type, and VQ is either volatile or empty, there exist
     candidate operator functions of the form
	     T*VQ&   operator++(T*VQ&);
	     T*VQ&   operator--(T*VQ&);
	     T*      operator++(T*VQ&, int);
	     T*      operator--(T*VQ&, int);  */

    case POSTDECREMENT_EXPR:
    case PREDECREMENT_EXPR:
      if (TREE_CODE (type1) == BOOLEAN_TYPE)
	return;
    case POSTINCREMENT_EXPR:
    case PREINCREMENT_EXPR:
      if (ARITHMETIC_TYPE_P (type1) || TYPE_PTROB_P (type1))
	{
	  type1 = build_reference_type (type1);
	  break;
	}
      return;

/* 7 For every cv-qualified or cv-unqualified complete object type T, there
     exist candidate operator functions of the form

	     T&      operator*(T*);

   8 For every function type T, there exist candidate operator functions of
     the form
	     T&      operator*(T*);  */

    case INDIRECT_REF:
      if (TREE_CODE (type1) == POINTER_TYPE
	  && (TYPE_PTROB_P (type1)
	      || TREE_CODE (TREE_TYPE (type1)) == FUNCTION_TYPE))
	break;
      return; 

/* 9 For every type T, there exist candidate operator functions of the form
	     T*      operator+(T*);

   10For  every  promoted arithmetic type T, there exist candidate operator
     functions of the form
	     T       operator+(T);
	     T       operator-(T);  */

    case CONVERT_EXPR: /* unary + */
      if (TREE_CODE (type1) == POINTER_TYPE)
	break;
    case NEGATE_EXPR:
      if (ARITHMETIC_TYPE_P (type1))
	break;
      return;

/* 11For every promoted integral type T,  there  exist  candidate  operator
     functions of the form
	     T       operator~(T);  */

    case BIT_NOT_EXPR:
      if (INTEGRAL_TYPE_P (type1))
	break;
      return;

/* 12For every quintuple C1, C2, T, CV1, CV2), where C2 is a class type, C1
     is the same type as C2 or is a derived class of C2, T  is  a  complete
     object type or a function type, and CV1 and CV2 are cv-qualifier-seqs,
     there exist candidate operator functions of the form
	     CV12 T& operator->*(CV1 C1*, CV2 T C2::*);
     where CV12 is the union of CV1 and CV2.  */

    case MEMBER_REF:
      if (TREE_CODE (type1) == POINTER_TYPE
	  && TYPE_PTR_TO_MEMBER_P (type2))
	{
	  tree c1 = TREE_TYPE (type1);
	  tree c2 = TYPE_PTRMEM_CLASS_TYPE (type2);

	  if (IS_AGGR_TYPE (c1) && DERIVED_FROM_P (c2, c1)
	      && (TYPE_PTRMEMFUNC_P (type2)
		  || is_complete (TREE_TYPE (TREE_TYPE (type2)))))
	    break;
	}
      return;

/* 13For every pair of promoted arithmetic types L and R, there exist  can-
     didate operator functions of the form
	     LR      operator*(L, R);
	     LR      operator/(L, R);
	     LR      operator+(L, R);
	     LR      operator-(L, R);
	     bool    operator<(L, R);
	     bool    operator>(L, R);
	     bool    operator<=(L, R);
	     bool    operator>=(L, R);
	     bool    operator==(L, R);
	     bool    operator!=(L, R);
     where  LR  is  the  result of the usual arithmetic conversions between
     types L and R.

   14For every pair of types T and I, where T  is  a  cv-qualified  or  cv-
     unqualified  complete  object  type and I is a promoted integral type,
     there exist candidate operator functions of the form
	     T*      operator+(T*, I);
	     T&      operator[](T*, I);
	     T*      operator-(T*, I);
	     T*      operator+(I, T*);
	     T&      operator[](I, T*);

   15For every T, where T is a pointer to complete object type, there exist
     candidate operator functions of the form112)
	     ptrdiff_t operator-(T, T);

   16For every pointer or enumeration type T, there exist candidate operator
     functions of the form
	     bool    operator<(T, T);
	     bool    operator>(T, T);
	     bool    operator<=(T, T);
	     bool    operator>=(T, T);
	     bool    operator==(T, T);
	     bool    operator!=(T, T);

   17For every pointer to member type T,  there  exist  candidate  operator
     functions of the form
	     bool    operator==(T, T);
	     bool    operator!=(T, T);  */

    case MINUS_EXPR:
      if (TYPE_PTROB_P (type1) && TYPE_PTROB_P (type2))
	break;
      if (TYPE_PTROB_P (type1) && INTEGRAL_TYPE_P (type2))
	{
	  type2 = ptrdiff_type_node;
	  break;
	}
    case MULT_EXPR:
    case TRUNC_DIV_EXPR:
      if (ARITHMETIC_TYPE_P (type1) && ARITHMETIC_TYPE_P (type2))
	break;
      return;

    case EQ_EXPR:
    case NE_EXPR:
      if ((TYPE_PTRMEMFUNC_P (type1) && TYPE_PTRMEMFUNC_P (type2))
	  || (TYPE_PTRMEM_P (type1) && TYPE_PTRMEM_P (type2)))
	break;
      if (TYPE_PTR_TO_MEMBER_P (type1) && null_ptr_cst_p (args[1]))
	{
	  type2 = type1;
	  break;
	}
      if (TYPE_PTR_TO_MEMBER_P (type2) && null_ptr_cst_p (args[0]))
	{
	  type1 = type2;
	  break;
	}
      /* Fall through.  */
    case LT_EXPR:
    case GT_EXPR:
    case LE_EXPR:
    case GE_EXPR:
    case MAX_EXPR:
    case MIN_EXPR:
      if (ARITHMETIC_TYPE_P (type1) && ARITHMETIC_TYPE_P (type2))
        break;
      if (TYPE_PTR_P (type1) && TYPE_PTR_P (type2))
	break;
      if (TREE_CODE (type1) == ENUMERAL_TYPE && TREE_CODE (type2) == ENUMERAL_TYPE)
        break;
      if (TYPE_PTR_P (type1) && null_ptr_cst_p (args[1]))
	{
	  type2 = type1;
	  break;
	}
      if (null_ptr_cst_p (args[0]) && TYPE_PTR_P (type2))
	{
	  type1 = type2;
	  break;
	}
      return;

    case PLUS_EXPR:
      if (ARITHMETIC_TYPE_P (type1) && ARITHMETIC_TYPE_P (type2))
	break;
    case ARRAY_REF:
      if (INTEGRAL_TYPE_P (type1) && TYPE_PTROB_P (type2))
	{
	  type1 = ptrdiff_type_node;
	  break;
	}
      if (TYPE_PTROB_P (type1) && INTEGRAL_TYPE_P (type2))
	{
	  type2 = ptrdiff_type_node;
	  break;
	}
      return;

/* 18For  every pair of promoted integral types L and R, there exist candi-
     date operator functions of the form
	     LR      operator%(L, R);
	     LR      operator&(L, R);
	     LR      operator^(L, R);
	     LR      operator|(L, R);
	     L       operator<<(L, R);
	     L       operator>>(L, R);
     where LR is the result of the  usual  arithmetic  conversions  between
     types L and R.  */

    case TRUNC_MOD_EXPR:
    case BIT_AND_EXPR:
    case BIT_IOR_EXPR:
    case BIT_XOR_EXPR:
    case LSHIFT_EXPR:
    case RSHIFT_EXPR:
      if (INTEGRAL_TYPE_P (type1) && INTEGRAL_TYPE_P (type2))
	break;
      return;

/* 19For  every  triple  L, VQ, R), where L is an arithmetic or enumeration
     type, VQ is either volatile or empty, and R is a  promoted  arithmetic
     type, there exist candidate operator functions of the form
	     VQ L&   operator=(VQ L&, R);
	     VQ L&   operator*=(VQ L&, R);
	     VQ L&   operator/=(VQ L&, R);
	     VQ L&   operator+=(VQ L&, R);
	     VQ L&   operator-=(VQ L&, R);

   20For  every  pair T, VQ), where T is any type and VQ is either volatile
     or empty, there exist candidate operator functions of the form
	     T*VQ&   operator=(T*VQ&, T*);

   21For every pair T, VQ), where T is a pointer to member type and  VQ  is
     either  volatile or empty, there exist candidate operator functions of
     the form
	     VQ T&   operator=(VQ T&, T);

   22For every triple  T,  VQ,  I),  where  T  is  a  cv-qualified  or  cv-
     unqualified  complete object type, VQ is either volatile or empty, and
     I is a promoted integral type, there exist  candidate  operator  func-
     tions of the form
	     T*VQ&   operator+=(T*VQ&, I);
	     T*VQ&   operator-=(T*VQ&, I);

   23For  every  triple  L,  VQ,  R), where L is an integral or enumeration
     type, VQ is either volatile or empty, and R  is  a  promoted  integral
     type, there exist candidate operator functions of the form

	     VQ L&   operator%=(VQ L&, R);
	     VQ L&   operator<<=(VQ L&, R);
	     VQ L&   operator>>=(VQ L&, R);
	     VQ L&   operator&=(VQ L&, R);
	     VQ L&   operator^=(VQ L&, R);
	     VQ L&   operator|=(VQ L&, R);  */

    case MODIFY_EXPR:
      switch (code2)
	{
	case PLUS_EXPR:
	case MINUS_EXPR:
	  if (TYPE_PTROB_P (type1) && INTEGRAL_TYPE_P (type2))
	    {
	      type2 = ptrdiff_type_node;
	      break;
	    }
	case MULT_EXPR:
	case TRUNC_DIV_EXPR:
	  if (ARITHMETIC_TYPE_P (type1) && ARITHMETIC_TYPE_P (type2))
	    break;
	  return;

	case TRUNC_MOD_EXPR:
	case BIT_AND_EXPR:
	case BIT_IOR_EXPR:
	case BIT_XOR_EXPR:
	case LSHIFT_EXPR:
	case RSHIFT_EXPR:
	  if (INTEGRAL_TYPE_P (type1) && INTEGRAL_TYPE_P (type2))
	    break;
	  return;

	case NOP_EXPR:
	  if (ARITHMETIC_TYPE_P (type1) && ARITHMETIC_TYPE_P (type2))
	    break;
	  if ((TYPE_PTRMEMFUNC_P (type1) && TYPE_PTRMEMFUNC_P (type2))
	      || (TYPE_PTR_P (type1) && TYPE_PTR_P (type2))
	      || (TYPE_PTRMEM_P (type1) && TYPE_PTRMEM_P (type2))
	      || ((TYPE_PTRMEMFUNC_P (type1)
		   || TREE_CODE (type1) == POINTER_TYPE)
		  && null_ptr_cst_p (args[1])))
	    {
	      type2 = type1;
	      break;
	    }
	  return;

	default:
	  abort ();
	}
      type1 = build_reference_type (type1);
      break;

    case COND_EXPR:
      /* [over.built]

	 For every pair of promoted arithmetic types L and R, there
	 exist candidate operator functions of the form 

	 LR operator?(bool, L, R); 

	 where LR is the result of the usual arithmetic conversions
	 between types L and R.

	 For every type T, where T is a pointer or pointer-to-member
	 type, there exist candidate operator functions of the form T
	 operator?(bool, T, T);  */

      if (promoted_arithmetic_type_p (type1)
	  && promoted_arithmetic_type_p (type2))
	/* That's OK.  */
	break;

      /* Otherwise, the types should be pointers.  */
      if (!(TYPE_PTR_P (type1) || TYPE_PTR_TO_MEMBER_P (type1))
	  || !(TYPE_PTR_P (type2) || TYPE_PTR_TO_MEMBER_P (type2)))
	return;
      
      /* We don't check that the two types are the same; the logic
	 below will actually create two candidates; one in which both
	 parameter types are TYPE1, and one in which both parameter
	 types are TYPE2.  */
      break;

    default:
      abort ();
    }

  /* If we're dealing with two pointer types or two enumeral types,
     we need candidates for both of them.  */
  if (type2 && !same_type_p (type1, type2)
      && TREE_CODE (type1) == TREE_CODE (type2)
      && (TREE_CODE (type1) == REFERENCE_TYPE
	  || (TYPE_PTR_P (type1) && TYPE_PTR_P (type2))
	  || (TYPE_PTRMEM_P (type1) && TYPE_PTRMEM_P (type2))
	  || TYPE_PTRMEMFUNC_P (type1)
	  || IS_AGGR_TYPE (type1)
	  || TREE_CODE (type1) == ENUMERAL_TYPE))
    {
      build_builtin_candidate
	(candidates, fnname, type1, type1, args, argtypes, flags);
      build_builtin_candidate
	(candidates, fnname, type2, type2, args, argtypes, flags);
      return;
    }

  build_builtin_candidate
    (candidates, fnname, type1, type2, args, argtypes, flags);
}

tree
type_decays_to (tree type)
{
  if (TREE_CODE (type) == ARRAY_TYPE)
    return build_pointer_type (TREE_TYPE (type));
  if (TREE_CODE (type) == FUNCTION_TYPE)
    return build_pointer_type (type);
  return type;
}

/* There are three conditions of builtin candidates:

   1) bool-taking candidates.  These are the same regardless of the input.
   2) pointer-pair taking candidates.  These are generated for each type
      one of the input types converts to.
   3) arithmetic candidates.  According to the standard, we should generate
      all of these, but I'm trying not to...
   
   Here we generate a superset of the possible candidates for this particular
   case.  That is a subset of the full set the standard defines, plus some
   other cases which the standard disallows. add_builtin_candidate will
   filter out the invalid set.  */

static void
add_builtin_candidates (struct z_candidate **candidates, enum tree_code code,
                        enum tree_code code2, tree fnname, tree *args,
                        int flags)
{
  int ref1, i;
  int enum_p = 0;
  tree type, argtypes[3];
  /* TYPES[i] is the set of possible builtin-operator parameter types
     we will consider for the Ith argument.  These are represented as
     a TREE_LIST; the TREE_VALUE of each node is the potential
     parameter type.  */
  tree types[2];

  for (i = 0; i < 3; ++i)
    {
      if (args[i])
	argtypes[i]  = lvalue_type (args[i]);
      else
	argtypes[i] = NULL_TREE;
    }

  switch (code)
    {
/* 4 For every pair T, VQ), where T is an arithmetic or  enumeration  type,
     and  VQ  is  either  volatile or empty, there exist candidate operator
     functions of the form
		 VQ T&   operator++(VQ T&);  */

    case POSTINCREMENT_EXPR:
    case PREINCREMENT_EXPR:
    case POSTDECREMENT_EXPR:
    case PREDECREMENT_EXPR:
    case MODIFY_EXPR:
      ref1 = 1;
      break;

/* 24There also exist candidate operator functions of the form
	     bool    operator!(bool);
	     bool    operator&&(bool, bool);
	     bool    operator||(bool, bool);  */

    case TRUTH_NOT_EXPR:
      build_builtin_candidate
	(candidates, fnname, boolean_type_node,
	 NULL_TREE, args, argtypes, flags);
      return;

    case TRUTH_ORIF_EXPR:
    case TRUTH_ANDIF_EXPR:
      build_builtin_candidate
	(candidates, fnname, boolean_type_node,
	 boolean_type_node, args, argtypes, flags);
      return;

    case ADDR_EXPR:
    case COMPOUND_EXPR:
    case COMPONENT_REF:
      return;

    case COND_EXPR:
    case EQ_EXPR:
    case NE_EXPR:
    case LT_EXPR:
    case LE_EXPR:
    case GT_EXPR:
    case GE_EXPR:
      enum_p = 1;
      /* Fall through.  */
    
    default:
      ref1 = 0;
    }

  types[0] = types[1] = NULL_TREE;

  for (i = 0; i < 2; ++i)
    {
      if (! args[i])
	;
      else if (IS_AGGR_TYPE (argtypes[i]))
	{
	  tree convs;

	  if (i == 0 && code == MODIFY_EXPR && code2 == NOP_EXPR)
	    return;

	  convs = lookup_conversions (argtypes[i]);

	  if (code == COND_EXPR)
	    {
	      if (real_lvalue_p (args[i]))
		types[i] = tree_cons
		  (NULL_TREE, build_reference_type (argtypes[i]), types[i]);

	      types[i] = tree_cons
		(NULL_TREE, TYPE_MAIN_VARIANT (argtypes[i]), types[i]);
	    }

	  else if (! convs)
	    return;

	  for (; convs; convs = TREE_CHAIN (convs))
	    {
	      type = TREE_TYPE (TREE_TYPE (OVL_CURRENT (TREE_VALUE (convs))));

	      if (i == 0 && ref1
		  && (TREE_CODE (type) != REFERENCE_TYPE
		      || CP_TYPE_CONST_P (TREE_TYPE (type))))
		continue;

	      if (code == COND_EXPR && TREE_CODE (type) == REFERENCE_TYPE)
		types[i] = tree_cons (NULL_TREE, type, types[i]);

	      type = non_reference (type);
	      if (i != 0 || ! ref1)
		{
		  type = TYPE_MAIN_VARIANT (type_decays_to (type));
	          if (enum_p && TREE_CODE (type) == ENUMERAL_TYPE)
	            types[i] = tree_cons (NULL_TREE, type, types[i]);
		  if (INTEGRAL_TYPE_P (type))
		    type = type_promotes_to (type);
		}

	      if (! value_member (type, types[i]))
		types[i] = tree_cons (NULL_TREE, type, types[i]);
	    }
	}
      else
	{
	  if (code == COND_EXPR && real_lvalue_p (args[i]))
	    types[i] = tree_cons
	      (NULL_TREE, build_reference_type (argtypes[i]), types[i]);
	  type = non_reference (argtypes[i]);
	  if (i != 0 || ! ref1)
	    {
	      type = TYPE_MAIN_VARIANT (type_decays_to (type));
	      if (enum_p && TREE_CODE (type) == ENUMERAL_TYPE)
	        types[i] = tree_cons (NULL_TREE, type, types[i]);
	      if (INTEGRAL_TYPE_P (type))
		type = type_promotes_to (type);
	    }
	  types[i] = tree_cons (NULL_TREE, type, types[i]);
	}
    }

  /* Run through the possible parameter types of both arguments,
     creating candidates with those parameter types.  */
  for (; types[0]; types[0] = TREE_CHAIN (types[0]))
    {
      if (types[1])
	for (type = types[1]; type; type = TREE_CHAIN (type))
	  add_builtin_candidate
	    (candidates, code, code2, fnname, TREE_VALUE (types[0]),
	     TREE_VALUE (type), args, argtypes, flags);
      else
	add_builtin_candidate
	  (candidates, code, code2, fnname, TREE_VALUE (types[0]),
	   NULL_TREE, args, argtypes, flags);
    }

  return;
}


/* If TMPL can be successfully instantiated as indicated by
   EXPLICIT_TARGS and ARGLIST, adds the instantiation to CANDIDATES.

   TMPL is the template.  EXPLICIT_TARGS are any explicit template
   arguments.  ARGLIST is the arguments provided at the call-site.
   The RETURN_TYPE is the desired type for conversion operators.  If
   OBJ is NULL_TREE, FLAGS and CTYPE are as for add_function_candidate.
   If an OBJ is supplied, FLAGS and CTYPE are ignored, and OBJ is as for
   add_conv_candidate.  */

static struct z_candidate*
add_template_candidate_real (struct z_candidate **candidates, tree tmpl,
                             tree ctype, tree explicit_targs, tree arglist,
                             tree return_type, tree access_path,
			     tree conversion_path, int flags, tree obj,
                             unification_kind_t strict)
{
  int ntparms = DECL_NTPARMS (tmpl);
  tree targs = make_tree_vec (ntparms);
  tree args_without_in_chrg = arglist;
  struct z_candidate *cand;
  int i;
  tree fn;

  /* We don't do deduction on the in-charge parameter, the VTT
     parameter or 'this'.  */
  if (DECL_NONSTATIC_MEMBER_FUNCTION_P (tmpl))
    args_without_in_chrg = TREE_CHAIN (args_without_in_chrg);

  if ((DECL_MAYBE_IN_CHARGE_CONSTRUCTOR_P (tmpl)
       || DECL_BASE_CONSTRUCTOR_P (tmpl))
      && TYPE_USES_VIRTUAL_BASECLASSES (DECL_CONTEXT (tmpl)))
    args_without_in_chrg = TREE_CHAIN (args_without_in_chrg);

  i = fn_type_unification (tmpl, explicit_targs, targs,
			   args_without_in_chrg,
			   return_type, strict, -1);

  if (i != 0)
    return NULL;

  fn = instantiate_template (tmpl, targs, tf_none);
  if (fn == error_mark_node)
    return NULL;

  /* In [class.copy]:

       A member function template is never instantiated to perform the
       copy of a class object to an object of its class type.  

     It's a little unclear what this means; the standard explicitly
     does allow a template to be used to copy a class.  For example,
     in:

       struct A {
         A(A&);
	 template <class T> A(const T&);
       };
       const A f ();
       void g () { A a (f ()); }
       
     the member template will be used to make the copy.  The section
     quoted above appears in the paragraph that forbids constructors
     whose only parameter is (a possibly cv-qualified variant of) the
     class type, and a logical interpretation is that the intent was
     to forbid the instantiation of member templates which would then
     have that form.  */
  if (DECL_CONSTRUCTOR_P (fn) && list_length (arglist) == 2) 
    {
      tree arg_types = FUNCTION_FIRST_USER_PARMTYPE (fn);
      if (arg_types && same_type_p (TYPE_MAIN_VARIANT (TREE_VALUE (arg_types)),
				    ctype))
	return NULL;
    }

  if (obj != NULL_TREE)
    /* Aha, this is a conversion function.  */
    cand = add_conv_candidate (candidates, fn, obj, access_path,
			       conversion_path, arglist);
  else
    cand = add_function_candidate (candidates, fn, ctype,
				   arglist, access_path, 
				   conversion_path, flags);
  if (DECL_TI_TEMPLATE (fn) != tmpl)
    /* This situation can occur if a member template of a template
       class is specialized.  Then, instantiate_template might return
       an instantiation of the specialization, in which case the
       DECL_TI_TEMPLATE field will point at the original
       specialization.  For example:

	 template <class T> struct S { template <class U> void f(U);
				       template <> void f(int) {}; };
	 S<double> sd;
	 sd.f(3);

       Here, TMPL will be template <class U> S<double>::f(U).
       And, instantiate template will give us the specialization
       template <> S<double>::f(int).  But, the DECL_TI_TEMPLATE field
       for this will point at template <class T> template <> S<T>::f(int),
       so that we can find the definition.  For the purposes of
       overload resolution, however, we want the original TMPL.  */
    cand->template = tree_cons (tmpl, targs, NULL_TREE);
  else
    cand->template = DECL_TEMPLATE_INFO (fn);

  return cand;
}


static struct z_candidate *
add_template_candidate (struct z_candidate **candidates, tree tmpl, tree ctype,
                        tree explicit_targs, tree arglist, tree return_type,
                        tree access_path, tree conversion_path, int flags,
                        unification_kind_t strict)
{
  return 
    add_template_candidate_real (candidates, tmpl, ctype,
				 explicit_targs, arglist, return_type, 
				 access_path, conversion_path,
				 flags, NULL_TREE, strict);
}


static struct z_candidate *
add_template_conv_candidate (struct z_candidate **candidates, tree tmpl,
                             tree obj, tree arglist, tree return_type,
			     tree access_path, tree conversion_path)
{
  return 
    add_template_candidate_real (candidates, tmpl, NULL_TREE, NULL_TREE,
				 arglist, return_type, access_path,
				 conversion_path, 0, obj, DEDUCE_CONV);
}

/* The CANDS are the set of candidates that were considered for
   overload resolution.  Return the set of viable candidates.  If none
   of the candidates were viable, set *ANY_VIABLE_P to true.  STRICT_P
   is true if a candidate should be considered viable only if it is
   strictly viable.  */

static struct z_candidate*
splice_viable (struct z_candidate *cands,
	       bool strict_p,
	       bool *any_viable_p)
{
  struct z_candidate *viable;
  struct z_candidate **last_viable;
  struct z_candidate **cand;

  viable = NULL;
  last_viable = &viable;
  *any_viable_p = false;

  cand = &cands; 
  while (*cand) 
    {
      struct z_candidate *c = *cand;
      if (strict_p ? c->viable == 1 : c->viable)
	{
	  *last_viable = c;
	  *cand = c->next;
	  c->next = NULL;
	  last_viable = &c->next;
	  *any_viable_p = true;
	}
      else
	cand = &c->next;
    }

  return viable ? viable : cands;
}

static bool
any_strictly_viable (struct z_candidate *cands)
{
  for (; cands; cands = cands->next)
    if (cands->viable == 1)
      return true;
  return false;
}

/* OBJ is being used in an expression like "OBJ.f (...)".  In other
   words, it is about to become the "this" pointer for a member
   function call.  Take the address of the object.  */

static tree
build_this (tree obj)
{
  /* In a template, we are only concerned about the type of the
     expression, so we can take a shortcut.  */
  if (processing_template_decl)
    return build_address (obj);

  return build_unary_op (ADDR_EXPR, obj, 0);
}

/* Returns true iff functions are equivalent. Equivalent functions are
   not '==' only if one is a function-local extern function or if
   both are extern "C".  */

static inline int
equal_functions (tree fn1, tree fn2)
{
  if (DECL_LOCAL_FUNCTION_P (fn1) || DECL_LOCAL_FUNCTION_P (fn2)
      || DECL_EXTERN_C_FUNCTION_P (fn1))
    return decls_match (fn1, fn2);
  return fn1 == fn2;
}

/* Print information about one overload candidate CANDIDATE.  MSGSTR
   is the text to print before the candidate itself.

   NOTE: Unlike most diagnostic functions in GCC, MSGSTR is expected
   to have been run through gettext by the caller.  This wart makes
   life simpler in print_z_candidates and for the translators.  */

static void
print_z_candidate (const char *msgstr, struct z_candidate *candidate)
{
  if (TREE_CODE (candidate->fn) == IDENTIFIER_NODE)
    {
      if (TREE_VEC_LENGTH (candidate->convs) == 3)
	inform ("%s %D(%T, %T, %T) <built-in>", msgstr, candidate->fn,
		TREE_TYPE (TREE_VEC_ELT (candidate->convs, 0)),
		TREE_TYPE (TREE_VEC_ELT (candidate->convs, 1)),
		TREE_TYPE (TREE_VEC_ELT (candidate->convs, 2)));
      else if (TREE_VEC_LENGTH (candidate->convs) == 2)
	inform ("%s %D(%T, %T) <built-in>", msgstr, candidate->fn,
		TREE_TYPE (TREE_VEC_ELT (candidate->convs, 0)),
		TREE_TYPE (TREE_VEC_ELT (candidate->convs, 1)));
      else
	inform ("%s %D(%T) <built-in>", msgstr, candidate->fn,
		TREE_TYPE (TREE_VEC_ELT (candidate->convs, 0)));
    }
  else if (TYPE_P (candidate->fn))
    inform ("%s %T <conversion>", msgstr, candidate->fn);
  else if (candidate->viable == -1)
    inform ("%J%s %+#D <near match>", candidate->fn, msgstr, candidate->fn);
  else
    inform ("%J%s %+#D", candidate->fn, msgstr, candidate->fn);
}

static void
print_z_candidates (struct z_candidate *candidates)
{
  const char *str;
  struct z_candidate *cand1;
  struct z_candidate **cand2;

  /* There may be duplicates in the set of candidates.  We put off
     checking this condition as long as possible, since we have no way
     to eliminate duplicates from a set of functions in less than n^2
     time.  Now we are about to emit an error message, so it is more
     permissible to go slowly.  */
  for (cand1 = candidates; cand1; cand1 = cand1->next)
    {
      tree fn = cand1->fn;
      /* Skip builtin candidates and conversion functions.  */
      if (TREE_CODE (fn) != FUNCTION_DECL)
	continue;
      cand2 = &cand1->next;
      while (*cand2)
	{
	  if (TREE_CODE ((*cand2)->fn) == FUNCTION_DECL
	      && equal_functions (fn, (*cand2)->fn))
	    *cand2 = (*cand2)->next;
	  else
	    cand2 = &(*cand2)->next;
	}
    }

  if (!candidates)
    return;

  str = _("candidates are:");
  print_z_candidate (str, candidates);
  if (candidates->next)
    {
      /* Indent successive candidates by the width of the translation
	 of the above string.  */
      size_t len = gcc_gettext_width (str) + 1;
      char *spaces = alloca (len);
      memset (spaces, ' ', len-1);
      spaces[len - 1] = '\0';

      candidates = candidates->next;
      do
	{
	  print_z_candidate (spaces, candidates);
	  candidates = candidates->next;
	}
      while (candidates);
    }
}

/* USER_SEQ is a user-defined conversion sequence, beginning with a
   USER_CONV.  STD_SEQ is the standard conversion sequence applied to
   the result of the conversion function to convert it to the final
   desired type.  Merge the the two sequences into a single sequence,
   and return the merged sequence.  */

static tree
merge_conversion_sequences (tree user_seq, tree std_seq)
{
  tree *t;

  my_friendly_assert (TREE_CODE (user_seq) == USER_CONV,
		      20030306);

  /* Find the end of the second conversion sequence.  */
  t = &(std_seq); 
  while (TREE_CODE (*t) != IDENTITY_CONV)
    t = &TREE_OPERAND (*t, 0);

  /* Replace the identity conversion with the user conversion
     sequence.  */
  *t = user_seq;

  /* The entire sequence is a user-conversion sequence.  */
  ICS_USER_FLAG (std_seq) = 1;

  return std_seq;
}

/* Returns the best overload candidate to perform the requested
   conversion.  This function is used for three the overloading situations
   described in [over.match.copy], [over.match.conv], and [over.match.ref].
   If TOTYPE is a REFERENCE_TYPE, we're trying to find an lvalue binding as
   per [dcl.init.ref], so we ignore temporary bindings.  */

static struct z_candidate *
build_user_type_conversion_1 (tree totype, tree expr, int flags)
{
  struct z_candidate *candidates, *cand;
  tree fromtype = TREE_TYPE (expr);
  tree ctors = NULL_TREE, convs = NULL_TREE;
  tree args = NULL_TREE;
  bool any_viable_p;

  /* We represent conversion within a hierarchy using RVALUE_CONV and
     BASE_CONV, as specified by [over.best.ics]; these become plain
     constructor calls, as specified in [dcl.init].  */
  my_friendly_assert (!IS_AGGR_TYPE (fromtype) || !IS_AGGR_TYPE (totype)
		      || !DERIVED_FROM_P (totype, fromtype), 20011226);

  if (IS_AGGR_TYPE (totype))
    ctors = lookup_fnfields (TYPE_BINFO (totype),
			     complete_ctor_identifier,
			     0);

  if (IS_AGGR_TYPE (fromtype))
    convs = lookup_conversions (fromtype);

  candidates = 0;
  flags |= LOOKUP_NO_CONVERSION;

  if (ctors)
    {
      tree t;

      ctors = BASELINK_FUNCTIONS (ctors);

      t = build_int_2 (0, 0);
      TREE_TYPE (t) = build_pointer_type (totype);
      args = build_tree_list (NULL_TREE, expr);
      /* We should never try to call the abstract or base constructor
	 from here.  */
      my_friendly_assert (!DECL_HAS_IN_CHARGE_PARM_P (OVL_CURRENT (ctors))
			  && !DECL_HAS_VTT_PARM_P (OVL_CURRENT (ctors)),
			  20011226);
      args = tree_cons (NULL_TREE, t, args);
    }
  for (; ctors; ctors = OVL_NEXT (ctors))
    {
      tree ctor = OVL_CURRENT (ctors);
      if (DECL_NONCONVERTING_P (ctor))
	continue;

      if (TREE_CODE (ctor) == TEMPLATE_DECL) 
	cand = add_template_candidate (&candidates, ctor, totype,
				       NULL_TREE, args, NULL_TREE, 
				       TYPE_BINFO (totype),
				       TYPE_BINFO (totype),
				       flags,
				       DEDUCE_CALL);
      else 
	cand = add_function_candidate (&candidates, ctor, totype,
				       args, TYPE_BINFO (totype), 
				       TYPE_BINFO (totype),
				       flags); 

      if (cand)
	cand->second_conv = build1 (IDENTITY_CONV, totype, NULL_TREE);
    }

  if (convs)
    args = build_tree_list (NULL_TREE, build_this (expr));

  for (; convs; convs = TREE_CHAIN (convs))
    {
      tree fns;
      tree conversion_path = TREE_PURPOSE (convs);
      int convflags = LOOKUP_NO_CONVERSION;

      /* If we are called to convert to a reference type, we are trying to
	 find an lvalue binding, so don't even consider temporaries.  If
	 we don't find an lvalue binding, the caller will try again to
	 look for a temporary binding.  */
      if (TREE_CODE (totype) == REFERENCE_TYPE)
	convflags |= LOOKUP_NO_TEMP_BIND;
      
      for (fns = TREE_VALUE (convs); fns; fns = OVL_NEXT (fns))
	{
	  tree fn = OVL_CURRENT (fns);
	  
	  /* [over.match.funcs] For conversion functions, the function
	     is considered to be a member of the class of the implicit
	     object argument for the purpose of defining the type of
	     the implicit object parameter.

	     So we pass fromtype as CTYPE to add_*_candidate.  */

	  if (TREE_CODE (fn) == TEMPLATE_DECL)
	    cand = add_template_candidate (&candidates, fn, fromtype, 
					   NULL_TREE,
					   args, totype, 
					   TYPE_BINFO (fromtype), 
					   conversion_path,
					   flags,
					   DEDUCE_CONV);
	  else 
	    cand = add_function_candidate (&candidates, fn, fromtype,
					   args,
					   TYPE_BINFO (fromtype),
					   conversion_path,
					   flags); 

	  if (cand)
	    {
	      tree ics = implicit_conversion (totype, 
					      TREE_TYPE (TREE_TYPE (cand->fn)),
					      0, convflags);

	      cand->second_conv = ics;
	      
	      if (ics == NULL_TREE)
		cand->viable = 0;
	      else if (candidates->viable == 1 && ICS_BAD_FLAG (ics))
		cand->viable = -1;
	    }
	}
    }

  candidates = splice_viable (candidates, pedantic, &any_viable_p);
  if (!any_viable_p)
    return 0;

  cand = tourney (candidates);
  if (cand == 0)
    {
      if (flags & LOOKUP_COMPLAIN)
	{
	  error ("conversion from `%T' to `%T' is ambiguous",
		    fromtype, totype);
	  print_z_candidates (candidates);
	}

      cand = candidates;	/* any one will do */
      cand->second_conv = build1 (AMBIG_CONV, totype, expr);
      ICS_USER_FLAG (cand->second_conv) = 1;
      if (!any_strictly_viable (candidates))
	ICS_BAD_FLAG (cand->second_conv) = 1;
      /* If there are viable candidates, don't set ICS_BAD_FLAG; an
	 ambiguous conversion is no worse than another user-defined
	 conversion.  */

      return cand;
    }

  /* Build the user conversion sequence.  */
  convs = build_conv
    (USER_CONV,
     (DECL_CONSTRUCTOR_P (cand->fn)
      ? totype : non_reference (TREE_TYPE (TREE_TYPE (cand->fn)))),
     build1 (IDENTITY_CONV, TREE_TYPE (expr), expr));
  TREE_OPERAND (convs, 1) = build_zc_wrapper (cand);

  /* Combine it with the second conversion sequence.  */
  cand->second_conv = merge_conversion_sequences (convs,
						  cand->second_conv);

  if (cand->viable == -1)
    ICS_BAD_FLAG (cand->second_conv) = 1;

  return cand;
}

tree
build_user_type_conversion (tree totype, tree expr, int flags)
{
  struct z_candidate *cand
    = build_user_type_conversion_1 (totype, expr, flags);

  if (cand)
    {
      if (TREE_CODE (cand->second_conv) == AMBIG_CONV)
	return error_mark_node;
      return convert_from_reference (convert_like (cand->second_conv, expr));
    }
  return NULL_TREE;
}

/* Do any initial processing on the arguments to a function call.  */

static tree
resolve_args (tree args)
{
  tree t;
  for (t = args; t; t = TREE_CHAIN (t))
    {
      tree arg = TREE_VALUE (t);
      
      if (arg == error_mark_node)
	return error_mark_node;
      else if (VOID_TYPE_P (TREE_TYPE (arg)))
	{
	  error ("invalid use of void expression");
	  return error_mark_node;
	}
      arg = convert_from_reference (arg);
      TREE_VALUE (t) = arg;
    }
  return args;
}

/* Perform overload resolution on FN, which is called with the ARGS.

   Return the candidate function selected by overload resolution, or
   NULL if the event that overload resolution failed.  In the case
   that overload resolution fails, *CANDIDATES will be the set of
   candidates considered, and ANY_VIABLE_P will be set to true or
   false to indicate whether or not any of the candidates were
   viable.  

   The ARGS should already have gone through RESOLVE_ARGS before this
   function is called.  */

static struct z_candidate *
perform_overload_resolution (tree fn, 
			     tree args, 
			     struct z_candidate **candidates,
			     bool *any_viable_p)
{
  struct z_candidate *cand;
  tree explicit_targs = NULL_TREE;
  int template_only = 0;

  *candidates = NULL;
  *any_viable_p = true;

  /* Check FN and ARGS.  */
  my_friendly_assert (TREE_CODE (fn) == FUNCTION_DECL 
		      || TREE_CODE (fn) == TEMPLATE_DECL
		      || TREE_CODE (fn) == OVERLOAD
		      || TREE_CODE (fn) == TEMPLATE_ID_EXPR,
		      20020712);
  my_friendly_assert (!args || TREE_CODE (args) == TREE_LIST,
		      20020712);

  if (TREE_CODE (fn) == TEMPLATE_ID_EXPR)
    {
      explicit_targs = TREE_OPERAND (fn, 1);
      fn = TREE_OPERAND (fn, 0);
      template_only = 1;
    }

  /* Add the various candidate functions.  */
  add_candidates (fn, args, explicit_targs, template_only,
		  /*conversion_path=*/NULL_TREE,
		  /*access_path=*/NULL_TREE,
		  LOOKUP_NORMAL,
		  candidates);

  *candidates = splice_viable (*candidates, pedantic, any_viable_p);
  if (!*any_viable_p)
    return NULL;

  cand = tourney (*candidates);
  return cand;
}

/* Return an expression for a call to FN (a namespace-scope function,
   or a static member function) with the ARGS.  */
      
tree
build_new_function_call (tree fn, tree args)
{
  struct z_candidate *candidates, *cand;
  bool any_viable_p;

  args = resolve_args (args);
  if (args == error_mark_node)
    return error_mark_node;

  cand = perform_overload_resolution (fn, args, &candidates, &any_viable_p);

  if (!cand)
    {
      if (!any_viable_p && candidates && ! candidates->next)
	return build_function_call (candidates->fn, args);
      if (TREE_CODE (fn) == TEMPLATE_ID_EXPR)
	fn = TREE_OPERAND (fn, 0);
      if (!any_viable_p)
	error ("no matching function for call to `%D(%A)'",
	       DECL_NAME (OVL_CURRENT (fn)), args);
      else
	error ("call of overloaded `%D(%A)' is ambiguous",
	       DECL_NAME (OVL_CURRENT (fn)), args);
      if (candidates)
	print_z_candidates (candidates);
      return error_mark_node;
    }

  return build_over_call (cand, LOOKUP_NORMAL);
}

/* Build a call to a global operator new.  FNNAME is the name of the
   operator (either "operator new" or "operator new[]") and ARGS are
   the arguments provided.  *SIZE points to the total number of bytes
   required by the allocation, and is updated if that is changed here.
   *COOKIE_SIZE is non-NULL if a cookie should be used.  If this
   function determines that no cookie should be used, after all,
   *COOKIE_SIZE is set to NULL_TREE.  */

tree
build_operator_new_call (tree fnname, tree args, tree *size, tree *cookie_size)
{
  tree fns;
  struct z_candidate *candidates;
  struct z_candidate *cand;
  bool any_viable_p;

  args = tree_cons (NULL_TREE, *size, args);
  args = resolve_args (args);
  if (args == error_mark_node)
    return args;

  fns = lookup_function_nonclass (fnname, args);

  /* Figure out what function is being called.  */
  cand = perform_overload_resolution (fns, args, &candidates, &any_viable_p);
  
  /* If no suitable function could be found, issue an error message
     and give up.  */
  if (!cand)
    {
      if (!any_viable_p)
	error ("no matching function for call to `%D(%A)'",
	       DECL_NAME (OVL_CURRENT (fns)), args);
      else
	error ("call of overloaded `%D(%A)' is ambiguous",
	       DECL_NAME (OVL_CURRENT (fns)), args);
      if (candidates)
	print_z_candidates (candidates);
      return error_mark_node;
    }

   /* If a cookie is required, add some extra space.  Whether
      or not a cookie is required cannot be determined until
      after we know which function was called.  */
   if (*cookie_size)
     {
       bool use_cookie = true;
       if (!abi_version_at_least (2))
	 {
	   tree placement = TREE_CHAIN (args);
	   /* In G++ 3.2, the check was implemented incorrectly; it
	      looked at the placement expression, rather than the
	      type of the function.  */
	   if (placement && !TREE_CHAIN (placement)
	       && same_type_p (TREE_TYPE (TREE_VALUE (placement)),
			       ptr_type_node))
	     use_cookie = false;
	 }
       else
	 {
	   tree arg_types;

	   arg_types = TYPE_ARG_TYPES (TREE_TYPE (cand->fn));
	   /* Skip the size_t parameter.  */
	   arg_types = TREE_CHAIN (arg_types);
	   /* Check the remaining parameters (if any).  */
	   if (arg_types 
	       && TREE_CHAIN (arg_types) == void_list_node
	       && same_type_p (TREE_VALUE (arg_types),
			       ptr_type_node))
	     use_cookie = false;
	 }
       /* If we need a cookie, adjust the number of bytes allocated.  */
       if (use_cookie)
	 {
	   /* Update the total size.  */
	   *size = size_binop (PLUS_EXPR, *size, *cookie_size);
	   /* Update the argument list to reflect the adjusted size.  */
	   TREE_VALUE (args) = *size;
	 }
       else
	 *cookie_size = NULL_TREE;
     }

   /* Build the CALL_EXPR.  */
   return build_over_call (cand, LOOKUP_NORMAL);
}

static tree
build_object_call (tree obj, tree args)
{
  struct z_candidate *candidates = 0, *cand;
  tree fns, convs, mem_args = NULL_TREE;
  tree type = TREE_TYPE (obj);
  bool any_viable_p;

  if (TYPE_PTRMEMFUNC_P (type))
    {
      /* It's no good looking for an overloaded operator() on a
	 pointer-to-member-function.  */
      error ("pointer-to-member function %E cannot be called without an object; consider using .* or ->*", obj);
      return error_mark_node;
    }

  fns = lookup_fnfields (TYPE_BINFO (type), ansi_opname (CALL_EXPR), 1);
  if (fns == error_mark_node)
    return error_mark_node;

  args = resolve_args (args);

  if (args == error_mark_node)
    return error_mark_node;

  if (fns)
    {
      tree base = BINFO_TYPE (BASELINK_BINFO (fns));
      mem_args = tree_cons (NULL_TREE, build_this (obj), args);

      for (fns = BASELINK_FUNCTIONS (fns); fns; fns = OVL_NEXT (fns))
	{
	  tree fn = OVL_CURRENT (fns);
	  if (TREE_CODE (fn) == TEMPLATE_DECL)
	    add_template_candidate (&candidates, fn, base, NULL_TREE,
				    mem_args, NULL_TREE, 
				    TYPE_BINFO (type),
				    TYPE_BINFO (type),
				    LOOKUP_NORMAL, DEDUCE_CALL);
	  else
	    add_function_candidate
	      (&candidates, fn, base, mem_args, TYPE_BINFO (type),
	       TYPE_BINFO (type), LOOKUP_NORMAL);
	}
    }

  convs = lookup_conversions (type);

  for (; convs; convs = TREE_CHAIN (convs))
    {
      tree fns = TREE_VALUE (convs);
      tree totype = TREE_TYPE (TREE_TYPE (OVL_CURRENT (fns)));

      if ((TREE_CODE (totype) == POINTER_TYPE
	   && TREE_CODE (TREE_TYPE (totype)) == FUNCTION_TYPE)
	  || (TREE_CODE (totype) == REFERENCE_TYPE
	      && TREE_CODE (TREE_TYPE (totype)) == FUNCTION_TYPE)
	  || (TREE_CODE (totype) == REFERENCE_TYPE
	      && TREE_CODE (TREE_TYPE (totype)) == POINTER_TYPE
	      && TREE_CODE (TREE_TYPE (TREE_TYPE (totype))) == FUNCTION_TYPE))
	for (; fns; fns = OVL_NEXT (fns))
	  {
	    tree fn = OVL_CURRENT (fns);
	    if (TREE_CODE (fn) == TEMPLATE_DECL) 
	      add_template_conv_candidate 
		(&candidates, fn, obj, args, totype,
		 /*access_path=*/NULL_TREE,
		 /*conversion_path=*/NULL_TREE);
	    else
	      add_conv_candidate (&candidates, fn, obj, args,
				  /*conversion_path=*/NULL_TREE,
				  /*access_path=*/NULL_TREE);
	  }
    }

  candidates = splice_viable (candidates, pedantic, &any_viable_p);
  if (!any_viable_p)
    {
      error ("no match for call to `(%T) (%A)'", TREE_TYPE (obj), args);
      print_z_candidates (candidates);
      return error_mark_node;
    }

  cand = tourney (candidates);
  if (cand == 0)
    {
      error ("call of `(%T) (%A)' is ambiguous", TREE_TYPE (obj), args);
      print_z_candidates (candidates);
      return error_mark_node;
    }

  /* Since cand->fn will be a type, not a function, for a conversion
     function, we must be careful not to unconditionally look at
     DECL_NAME here.  */
  if (TREE_CODE (cand->fn) == FUNCTION_DECL
      && DECL_OVERLOADED_OPERATOR_P (cand->fn) == CALL_EXPR)
    return build_over_call (cand, LOOKUP_NORMAL);

  obj = convert_like_with_context
          (TREE_VEC_ELT (cand->convs, 0), obj, cand->fn, -1);

  /* FIXME */
  return build_function_call (obj, args);
}

static void
op_error (enum tree_code code, enum tree_code code2,
          tree arg1, tree arg2, tree arg3, const char *problem)
{
  const char *opname;

  if (code == MODIFY_EXPR)
    opname = assignment_operator_name_info[code2].name;
  else
    opname = operator_name_info[code].name;

  switch (code)
    {
    case COND_EXPR:
      error ("%s for ternary 'operator?:' in '%E ? %E : %E'",
             problem, arg1, arg2, arg3);
      break;
      
    case POSTINCREMENT_EXPR:
    case POSTDECREMENT_EXPR:
      error ("%s for 'operator%s' in '%E%s'", problem, opname, arg1, opname);
      break;
      
    case ARRAY_REF:
      error ("%s for 'operator[]' in '%E[%E]'", problem, arg1, arg2);
      break;

    case REALPART_EXPR:
    case IMAGPART_EXPR:
      error ("%s for '%s' in '%s %E'", problem, opname, opname, arg1);
      break;
      
    default:
      if (arg2)
	error ("%s for 'operator%s' in '%E %s %E'",
               problem, opname, arg1, opname, arg2);
      else
	error ("%s for 'operator%s' in '%s%E'",
               problem, opname, opname, arg1);
      break;
    }
}

/* Return the implicit conversion sequence that could be used to
   convert E1 to E2 in [expr.cond].  */

static tree
conditional_conversion (tree e1, tree e2)
{
  tree t1 = non_reference (TREE_TYPE (e1));
  tree t2 = non_reference (TREE_TYPE (e2));
  tree conv;
  bool good_base;

  /* [expr.cond]

     If E2 is an lvalue: E1 can be converted to match E2 if E1 can be
     implicitly converted (clause _conv_) to the type "reference to
     T2", subject to the constraint that in the conversion the
     reference must bind directly (_dcl.init.ref_) to E1.  */
  if (real_lvalue_p (e2))
    {
      conv = implicit_conversion (build_reference_type (t2), 
				  t1,
				  e1,
				  LOOKUP_NO_TEMP_BIND);
      if (conv)
	return conv;
    }

  /* [expr.cond]

     If E1 and E2 have class type, and the underlying class types are
     the same or one is a base class of the other: E1 can be converted
     to match E2 if the class of T2 is the same type as, or a base
     class of, the class of T1, and the cv-qualification of T2 is the
     same cv-qualification as, or a greater cv-qualification than, the
     cv-qualification of T1.  If the conversion is applied, E1 is
     changed to an rvalue of type T2 that still refers to the original
     source class object (or the appropriate subobject thereof).  */
  if (CLASS_TYPE_P (t1) && CLASS_TYPE_P (t2)
      && ((good_base = DERIVED_FROM_P (t2, t1)) || DERIVED_FROM_P (t1, t2)))
    {
      if (good_base && at_least_as_qualified_p (t2, t1))
	{
	  conv = build1 (IDENTITY_CONV, t1, e1);
	  if (!same_type_p (TYPE_MAIN_VARIANT (t1), 
			    TYPE_MAIN_VARIANT (t2)))
	    conv = build_conv (BASE_CONV, t2, conv);
	  else
	    conv = build_conv (RVALUE_CONV, t2, conv);
	  return conv;
	}
      else
	return NULL_TREE;
    }
  else
    /* [expr.cond]

       Otherwise: E1 can be converted to match E2 if E1 can be implicitly
       converted to the type that expression E2 would have if E2 were
       converted to an rvalue (or the type it has, if E2 is an rvalue).  */
    return implicit_conversion (t2, t1, e1, LOOKUP_NORMAL);
}

/* Implement [expr.cond].  ARG1, ARG2, and ARG3 are the three
   arguments to the conditional expression.  */

tree
build_conditional_expr (tree arg1, tree arg2, tree arg3)
{
  tree arg2_type;
  tree arg3_type;
  tree result;
  tree result_type = NULL_TREE;
  bool lvalue_p = true;
  struct z_candidate *candidates = 0;
  struct z_candidate *cand;

  /* As a G++ extension, the second argument to the conditional can be
     omitted.  (So that `a ? : c' is roughly equivalent to `a ? a :
     c'.)  If the second operand is omitted, make sure it is
     calculated only once.  */
  if (!arg2)
    {
      if (pedantic)
	pedwarn ("ISO C++ forbids omitting the middle term of a ?: expression");

      /* Make sure that lvalues remain lvalues.  See g++.oliva/ext1.C.  */
      if (real_lvalue_p (arg1))
	arg2 = arg1 = stabilize_reference (arg1);
      else
	arg2 = arg1 = save_expr (arg1);
    }

  /* [expr.cond]
  
     The first expr ession is implicitly converted to bool (clause
     _conv_).  */
  arg1 = perform_implicit_conversion (boolean_type_node, arg1);

  /* If something has already gone wrong, just pass that fact up the
     tree.  */
  if (error_operand_p (arg1)
      || error_operand_p (arg2)
      || error_operand_p (arg3))
    return error_mark_node;

  /* [expr.cond]

     If either the second or the third operand has type (possibly
     cv-qualified) void, then the lvalue-to-rvalue (_conv.lval_),
     array-to-pointer (_conv.array_), and function-to-pointer
     (_conv.func_) standard conversions are performed on the second
     and third operands.  */
  arg2_type = TREE_TYPE (arg2);
  arg3_type = TREE_TYPE (arg3);
  if (VOID_TYPE_P (arg2_type) || VOID_TYPE_P (arg3_type))
    {
      /* Do the conversions.  We don't these for `void' type arguments
	 since it can't have any effect and since decay_conversion
	 does not handle that case gracefully.  */
      if (!VOID_TYPE_P (arg2_type))
	arg2 = decay_conversion (arg2);
      if (!VOID_TYPE_P (arg3_type))
	arg3 = decay_conversion (arg3);
      arg2_type = TREE_TYPE (arg2);
      arg3_type = TREE_TYPE (arg3);

      /* [expr.cond]

	 One of the following shall hold:

	 --The second or the third operand (but not both) is a
	   throw-expression (_except.throw_); the result is of the
	   type of the other and is an rvalue.

	 --Both the second and the third operands have type void; the
	   result is of type void and is an rvalue.  

         We must avoid calling force_rvalue for expressions of type
	 "void" because it will complain that their value is being
	 used.   */
      if (TREE_CODE (arg2) == THROW_EXPR 
	  && TREE_CODE (arg3) != THROW_EXPR)
	{
	  if (!VOID_TYPE_P (arg3_type))
	    arg3 = force_rvalue (arg3);
	  arg3_type = TREE_TYPE (arg3);
	  result_type = arg3_type;
	}
      else if (TREE_CODE (arg2) != THROW_EXPR 
	       && TREE_CODE (arg3) == THROW_EXPR)
	{
	  if (!VOID_TYPE_P (arg2_type))
	    arg2 = force_rvalue (arg2);
	  arg2_type = TREE_TYPE (arg2);
	  result_type = arg2_type;
	}
      else if (VOID_TYPE_P (arg2_type) && VOID_TYPE_P (arg3_type))
	result_type = void_type_node;
      else
	{
	  error ("`%E' has type `void' and is not a throw-expression",
		    VOID_TYPE_P (arg2_type) ? arg2 : arg3);
	  return error_mark_node;
	}

      lvalue_p = false;
      goto valid_operands;
    }
  /* [expr.cond]

     Otherwise, if the second and third operand have different types,
     and either has (possibly cv-qualified) class type, an attempt is
     made to convert each of those operands to the type of the other.  */
  else if (!same_type_p (arg2_type, arg3_type)
	   && (CLASS_TYPE_P (arg2_type) || CLASS_TYPE_P (arg3_type)))
    {
      tree conv2 = conditional_conversion (arg2, arg3);
      tree conv3 = conditional_conversion (arg3, arg2);
      
      /* [expr.cond]

	 If both can be converted, or one can be converted but the
	 conversion is ambiguous, the program is ill-formed.  If
	 neither can be converted, the operands are left unchanged and
	 further checking is performed as described below.  If exactly
	 one conversion is possible, that conversion is applied to the
	 chosen operand and the converted operand is used in place of
	 the original operand for the remainder of this section.  */
      if ((conv2 && !ICS_BAD_FLAG (conv2) 
	   && conv3 && !ICS_BAD_FLAG (conv3))
	  || (conv2 && TREE_CODE (conv2) == AMBIG_CONV)
	  || (conv3 && TREE_CODE (conv3) == AMBIG_CONV))
	{
	  error ("operands to ?: have different types");
	  return error_mark_node;
	}
      else if (conv2 && !ICS_BAD_FLAG (conv2))
	{
	  arg2 = convert_like (conv2, arg2);
	  arg2 = convert_from_reference (arg2);
	  arg2_type = TREE_TYPE (arg2);
	}
      else if (conv3 && !ICS_BAD_FLAG (conv3))
	{
	  arg3 = convert_like (conv3, arg3);
	  arg3 = convert_from_reference (arg3);
	  arg3_type = TREE_TYPE (arg3);
	}

      /* If, after the conversion, both operands have class type,
	 treat the cv-qualification of both operands as if it were the
	 union of the cv-qualification of the operands.  

	 The standard is not clear about what to do in this
	 circumstance.  For example, if the first operand has type
	 "const X" and the second operand has a user-defined
	 conversion to "volatile X", what is the type of the second
	 operand after this step?  Making it be "const X" (matching
	 the first operand) seems wrong, as that discards the
	 qualification without actuall performing a copy.  Leaving it
	 as "volatile X" seems wrong as that will result in the
	 conditional expression failing altogether, even though,
	 according to this step, the one operand could be converted to
	 the type of the other.  */
      if ((conv2 || conv3)
	  && CLASS_TYPE_P (arg2_type)
	  && TYPE_QUALS (arg2_type) != TYPE_QUALS (arg3_type))
	arg2_type = arg3_type = 
	  cp_build_qualified_type (arg2_type,
				   TYPE_QUALS (arg2_type)
				   | TYPE_QUALS (arg3_type));
    }

  /* [expr.cond]

     If the second and third operands are lvalues and have the same
     type, the result is of that type and is an lvalue.  */
  if (real_lvalue_p (arg2) 
      && real_lvalue_p (arg3) 
      && same_type_p (arg2_type, arg3_type))
    {
      result_type = arg2_type;
      goto valid_operands;
    }

  /* [expr.cond]

     Otherwise, the result is an rvalue.  If the second and third
     operand do not have the same type, and either has (possibly
     cv-qualified) class type, overload resolution is used to
     determine the conversions (if any) to be applied to the operands
     (_over.match.oper_, _over.built_).  */
  lvalue_p = false;
  if (!same_type_p (arg2_type, arg3_type)
      && (CLASS_TYPE_P (arg2_type) || CLASS_TYPE_P (arg3_type)))
    {
      tree args[3];
      tree conv;
      bool any_viable_p;

      /* Rearrange the arguments so that add_builtin_candidate only has
	 to know about two args.  In build_builtin_candidates, the
	 arguments are unscrambled.  */
      args[0] = arg2;
      args[1] = arg3;
      args[2] = arg1;
      add_builtin_candidates (&candidates, 
			      COND_EXPR, 
			      NOP_EXPR,
			      ansi_opname (COND_EXPR),
			      args,
			      LOOKUP_NORMAL);

      /* [expr.cond]

	 If the overload resolution fails, the program is
	 ill-formed.  */
      candidates = splice_viable (candidates, pedantic, &any_viable_p);
      if (!any_viable_p)
	{
	  op_error (COND_EXPR, NOP_EXPR, arg1, arg2, arg3, "no match");
	  print_z_candidates (candidates);
	  return error_mark_node;
	}
      cand = tourney (candidates);
      if (!cand)
	{
	  op_error (COND_EXPR, NOP_EXPR, arg1, arg2, arg3, "no match");
	  print_z_candidates (candidates);
	  return error_mark_node;
	}

      /* [expr.cond]

	 Otherwise, the conversions thus determined are applied, and
	 the converted operands are used in place of the original
	 operands for the remainder of this section.  */
      conv = TREE_VEC_ELT (cand->convs, 0);
      arg1 = convert_like (conv, arg1);
      conv = TREE_VEC_ELT (cand->convs, 1);
      arg2 = convert_like (conv, arg2);
      conv = TREE_VEC_ELT (cand->convs, 2);
      arg3 = convert_like (conv, arg3);
    }

  /* [expr.cond]

     Lvalue-to-rvalue (_conv.lval_), array-to-pointer (_conv.array_),
     and function-to-pointer (_conv.func_) standard conversions are
     performed on the second and third operands.

     We need to force the lvalue-to-rvalue conversion here for class types,
     so we get TARGET_EXPRs; trying to deal with a COND_EXPR of class rvalues
     that isn't wrapped with a TARGET_EXPR plays havoc with exception
     regions.  */

  arg2 = force_rvalue (arg2);
  if (!CLASS_TYPE_P (arg2_type))
    arg2_type = TREE_TYPE (arg2);

  arg3 = force_rvalue (arg3);
  if (!CLASS_TYPE_P (arg2_type))
    arg3_type = TREE_TYPE (arg3);

  if (arg2 == error_mark_node || arg3 == error_mark_node)
    return error_mark_node;
  
  /* [expr.cond]
     
     After those conversions, one of the following shall hold:

     --The second and third operands have the same type; the result  is  of
       that type.  */
  if (same_type_p (arg2_type, arg3_type))
    result_type = arg2_type;
  /* [expr.cond]

     --The second and third operands have arithmetic or enumeration
       type; the usual arithmetic conversions are performed to bring
       them to a common type, and the result is of that type.  */
  else if ((ARITHMETIC_TYPE_P (arg2_type) 
	    || TREE_CODE (arg2_type) == ENUMERAL_TYPE)
	   && (ARITHMETIC_TYPE_P (arg3_type)
	       || TREE_CODE (arg3_type) == ENUMERAL_TYPE))
    {
      /* In this case, there is always a common type.  */
      result_type = type_after_usual_arithmetic_conversions (arg2_type, 
							     arg3_type);
      
      if (TREE_CODE (arg2_type) == ENUMERAL_TYPE
          && TREE_CODE (arg3_type) == ENUMERAL_TYPE)
         warning ("enumeral mismatch in conditional expression: `%T' vs `%T'",
                   arg2_type, arg3_type);
      else if (extra_warnings
               && ((TREE_CODE (arg2_type) == ENUMERAL_TYPE
                    && !same_type_p (arg3_type, type_promotes_to (arg2_type)))
                   || (TREE_CODE (arg3_type) == ENUMERAL_TYPE
                       && !same_type_p (arg2_type, type_promotes_to (arg3_type)))))
        warning ("enumeral and non-enumeral type in conditional expression");
      
      arg2 = perform_implicit_conversion (result_type, arg2);
      arg3 = perform_implicit_conversion (result_type, arg3);
    }
  /* [expr.cond]

     --The second and third operands have pointer type, or one has
       pointer type and the other is a null pointer constant; pointer
       conversions (_conv.ptr_) and qualification conversions
       (_conv.qual_) are performed to bring them to their composite
       pointer type (_expr.rel_).  The result is of the composite
       pointer type.

     --The second and third operands have pointer to member type, or
       one has pointer to member type and the other is a null pointer
       constant; pointer to member conversions (_conv.mem_) and
       qualification conversions (_conv.qual_) are performed to bring
       them to a common type, whose cv-qualification shall match the
       cv-qualification of either the second or the third operand.
       The result is of the common type.  */
  else if ((null_ptr_cst_p (arg2) 
	    && (TYPE_PTR_P (arg3_type) || TYPE_PTR_TO_MEMBER_P (arg3_type)))
	   || (null_ptr_cst_p (arg3) 
	       && (TYPE_PTR_P (arg2_type) || TYPE_PTR_TO_MEMBER_P (arg2_type)))
	   || (TYPE_PTR_P (arg2_type) && TYPE_PTR_P (arg3_type))
	   || (TYPE_PTRMEM_P (arg2_type) && TYPE_PTRMEM_P (arg3_type))
	   || (TYPE_PTRMEMFUNC_P (arg2_type) && TYPE_PTRMEMFUNC_P (arg3_type)))
    {
      result_type = composite_pointer_type (arg2_type, arg3_type, arg2,
					    arg3, "conditional expression");
      if (result_type == error_mark_node)
	return error_mark_node;
      arg2 = perform_implicit_conversion (result_type, arg2);
      arg3 = perform_implicit_conversion (result_type, arg3);
    }

  if (!result_type)
    {
      error ("operands to ?: have different types");
      return error_mark_node;
    }

 valid_operands:
  result = fold_if_not_in_template (build (COND_EXPR, result_type,
					   arg1, arg2, arg3));
  /* We can't use result_type below, as fold might have returned a
     throw_expr.  */

  /* Expand both sides into the same slot, hopefully the target of the
     ?: expression.  We used to check for TARGET_EXPRs here, but now we
     sometimes wrap them in NOP_EXPRs so the test would fail.  */
  if (!lvalue_p && CLASS_TYPE_P (TREE_TYPE (result)))
    result = get_target_expr (result);
  
  /* If this expression is an rvalue, but might be mistaken for an
     lvalue, we must add a NON_LVALUE_EXPR.  */
  if (!lvalue_p && real_lvalue_p (result))
    result = build1 (NON_LVALUE_EXPR, TREE_TYPE (result), result);

  return result;
}

/* OPERAND is an operand to an expression.  Perform necessary steps
   required before using it.  If OPERAND is NULL_TREE, NULL_TREE is
   returned.  */

static tree
prep_operand (tree operand)
{
  if (operand)
    {
      operand = convert_from_reference (operand);
      if (CLASS_TYPE_P (TREE_TYPE (operand))
	  && CLASSTYPE_TEMPLATE_INSTANTIATION (TREE_TYPE (operand)))
	/* Make sure the template type is instantiated now.  */
	instantiate_class_template (TYPE_MAIN_VARIANT (TREE_TYPE (operand)));
    }

  return operand;
}

/* Add each of the viable functions in FNS (a FUNCTION_DECL or
   OVERLOAD) to the CANDIDATES, returning an updated list of
   CANDIDATES.  The ARGS are the arguments provided to the call,
   without any implicit object parameter.  The EXPLICIT_TARGS are
   explicit template arguments provided.  TEMPLATE_ONLY is true if
   only template functions should be considered.  CONVERSION_PATH,
   ACCESS_PATH, and FLAGS are as for add_function_candidate.  */

static void
add_candidates (tree fns, tree args, 
		tree explicit_targs, bool template_only,
		tree conversion_path, tree access_path,
		int flags,
		struct z_candidate **candidates)
{
  tree ctype;
  tree non_static_args;

  ctype = conversion_path ? BINFO_TYPE (conversion_path) : NULL_TREE;
  /* Delay creating the implicit this parameter until it is needed.  */
  non_static_args = NULL_TREE;

  while (fns) 
    {
      tree fn;
      tree fn_args;

      fn = OVL_CURRENT (fns);
      /* Figure out which set of arguments to use.  */
      if (DECL_NONSTATIC_MEMBER_FUNCTION_P (fn))
	{
	  /* If this function is a non-static member, prepend the implicit
	     object parameter.  */
	  if (!non_static_args)
	    non_static_args = tree_cons (NULL_TREE,
					 build_this (TREE_VALUE (args)),
					 TREE_CHAIN (args));
	  fn_args = non_static_args;
	}
      else
	/* Otherwise, just use the list of arguments provided.  */
	fn_args = args;

      if (TREE_CODE (fn) == TEMPLATE_DECL)
	add_template_candidate (candidates, 
				fn, 
				ctype,
				explicit_targs,
				fn_args,
				NULL_TREE,
				access_path,
				conversion_path,
				flags,
				DEDUCE_CALL);
      else if (!template_only)
	add_function_candidate (candidates,
				fn,
				ctype,
				fn_args,
				access_path,
				conversion_path,
				flags);
      fns = OVL_NEXT (fns);
    }
}

tree
build_new_op (enum tree_code code, int flags, tree arg1, tree arg2, tree arg3,
	      bool *overloaded_p)
{
  struct z_candidate *candidates = 0, *cand;
  tree arglist, fnname;
  tree args[3];
  enum tree_code code2 = NOP_EXPR;
  tree conv;
  bool strict_p;
  bool any_viable_p;

  if (error_operand_p (arg1) 
      || error_operand_p (arg2) 
      || error_operand_p (arg3))
    return error_mark_node;

  if (code == MODIFY_EXPR)
    {
      code2 = TREE_CODE (arg3);
      arg3 = NULL_TREE;
      fnname = ansi_assopname (code2);
    }
  else
    fnname = ansi_opname (code);

  arg1 = prep_operand (arg1);
  
  switch (code)
    {
    case NEW_EXPR:
    case VEC_NEW_EXPR:
    case VEC_DELETE_EXPR:
    case DELETE_EXPR:
      /* Use build_op_new_call and build_op_delete_call instead.  */
      abort ();

    case CALL_EXPR:
      return build_object_call (arg1, arg2);

    default:
      break;
    }

  arg2 = prep_operand (arg2);
  arg3 = prep_operand (arg3);
  
  if (code == COND_EXPR)
    {
      if (arg2 == NULL_TREE
	  || TREE_CODE (TREE_TYPE (arg2)) == VOID_TYPE
	  || TREE_CODE (TREE_TYPE (arg3)) == VOID_TYPE
	  || (! IS_OVERLOAD_TYPE (TREE_TYPE (arg2))
	      && ! IS_OVERLOAD_TYPE (TREE_TYPE (arg3))))
	goto builtin;
    }
  else if (! IS_OVERLOAD_TYPE (TREE_TYPE (arg1))
	   && (! arg2 || ! IS_OVERLOAD_TYPE (TREE_TYPE (arg2))))
    goto builtin;

  if (code == POSTINCREMENT_EXPR || code == POSTDECREMENT_EXPR)
    arg2 = integer_zero_node;

  arglist = NULL_TREE;
  if (arg3)
    arglist = tree_cons (NULL_TREE, arg3, arglist);
  if (arg2)
    arglist = tree_cons (NULL_TREE, arg2, arglist);
  arglist = tree_cons (NULL_TREE, arg1, arglist);

  /* Add namespace-scope operators to the list of functions to
     consider.  */
  add_candidates (lookup_function_nonclass (fnname, arglist),
		  arglist, NULL_TREE, false, NULL_TREE, NULL_TREE,
		  flags, &candidates);
  /* Add class-member operators to the candidate set.  */
  if (CLASS_TYPE_P (TREE_TYPE (arg1)))
    {
      tree fns;

      fns = lookup_fnfields (TYPE_BINFO (TREE_TYPE (arg1)), fnname, 1);
      if (fns == error_mark_node)
	return fns;
      if (fns)
	add_candidates (BASELINK_FUNCTIONS (fns), arglist, 
			NULL_TREE, false,
			BASELINK_BINFO (fns),
			TYPE_BINFO (TREE_TYPE (arg1)),
			flags, &candidates);
    }

  /* Rearrange the arguments for ?: so that add_builtin_candidate only has
     to know about two args; a builtin candidate will always have a first
     parameter of type bool.  We'll handle that in
     build_builtin_candidate.  */
  if (code == COND_EXPR)
    {
      args[0] = arg2;
      args[1] = arg3;
      args[2] = arg1;
    }
  else
    {
      args[0] = arg1;
      args[1] = arg2;
      args[2] = NULL_TREE;
    }

  add_builtin_candidates (&candidates, code, code2, fnname, args, flags);

  switch (code)
    {
    case COMPOUND_EXPR:
    case ADDR_EXPR:
      /* For these, the built-in candidates set is empty
	 [over.match.oper]/3.  We don't want non-strict matches
	 because exact matches are always possible with built-in
	 operators.  The built-in candidate set for COMPONENT_REF
	 would be empty too, but since there are no such built-in
	 operators, we accept non-strict matches for them.  */
      strict_p = true;
      break;

    default:
      strict_p = pedantic;
      break;
    }      

  candidates = splice_viable (candidates, strict_p, &any_viable_p);
  if (!any_viable_p)
    {
      switch (code)
	{
	case POSTINCREMENT_EXPR:
	case POSTDECREMENT_EXPR:
	  /* Look for an `operator++ (int)'.  If they didn't have
	     one, then we fall back to the old way of doing things.  */
	  if (flags & LOOKUP_COMPLAIN)
	    pedwarn ("no `%D(int)' declared for postfix `%s', trying prefix operator instead",
			fnname, 
			operator_name_info[code].name);
	  if (code == POSTINCREMENT_EXPR)
	    code = PREINCREMENT_EXPR;
	  else
	    code = PREDECREMENT_EXPR;	
	  return build_new_op (code, flags, arg1, NULL_TREE, NULL_TREE,
			       overloaded_p);
	  
	  /* The caller will deal with these.  */
	case ADDR_EXPR:
	case COMPOUND_EXPR:
	case COMPONENT_REF:
	  return NULL_TREE;

	default:
	  break;
	}
      if (flags & LOOKUP_COMPLAIN)
	{
	  op_error (code, code2, arg1, arg2, arg3, "no match");
	  print_z_candidates (candidates);
	}
      return error_mark_node;
    }

  cand = tourney (candidates);
  if (cand == 0)
    {
      if (flags & LOOKUP_COMPLAIN)
	{
	  op_error (code, code2, arg1, arg2, arg3, "ambiguous overload");
	  print_z_candidates (candidates);
	}
      return error_mark_node;
    }

  if (TREE_CODE (cand->fn) == FUNCTION_DECL)
    {
      if (overloaded_p)
	*overloaded_p = true;

      return build_over_call (cand, LOOKUP_NORMAL);
    }

  /* Check for comparison of different enum types.  */
  switch (code)
    {
    case GT_EXPR:
    case LT_EXPR:
    case GE_EXPR:
    case LE_EXPR:
    case EQ_EXPR:
    case NE_EXPR:
      if (TREE_CODE (TREE_TYPE (arg1)) == ENUMERAL_TYPE 
	  && TREE_CODE (TREE_TYPE (arg2)) == ENUMERAL_TYPE 
	  && (TYPE_MAIN_VARIANT (TREE_TYPE (arg1))
	      != TYPE_MAIN_VARIANT (TREE_TYPE (arg2))))
	{
	  warning ("comparison between `%#T' and `%#T'", 
		      TREE_TYPE (arg1), TREE_TYPE (arg2));
	}
      break;
    default:
      break;
    }

  /* We need to strip any leading REF_BIND so that bitfields don't cause
     errors.  This should not remove any important conversions, because
     builtins don't apply to class objects directly.  */
  conv = TREE_VEC_ELT (cand->convs, 0);
  if (TREE_CODE (conv) == REF_BIND)
    conv = TREE_OPERAND (conv, 0);
  arg1 = convert_like (conv, arg1);
  if (arg2)
    {
      conv = TREE_VEC_ELT (cand->convs, 1);
      if (TREE_CODE (conv) == REF_BIND)
        conv = TREE_OPERAND (conv, 0);
      arg2 = convert_like (conv, arg2);
    }
  if (arg3)
    {
      conv = TREE_VEC_ELT (cand->convs, 2);
      if (TREE_CODE (conv) == REF_BIND)
        conv = TREE_OPERAND (conv, 0);
      arg3 = convert_like (conv, arg3);
    }

builtin:
  switch (code)
    {
    case MODIFY_EXPR:
      return build_modify_expr (arg1, code2, arg2);

    case INDIRECT_REF:
      return build_indirect_ref (arg1, "unary *");

    case PLUS_EXPR:
    case MINUS_EXPR:
    case MULT_EXPR:
    case TRUNC_DIV_EXPR:
    case GT_EXPR:
    case LT_EXPR:
    case GE_EXPR:
    case LE_EXPR:
    case EQ_EXPR:
    case NE_EXPR:
    case MAX_EXPR:
    case MIN_EXPR:
    case LSHIFT_EXPR:
    case RSHIFT_EXPR:
    case TRUNC_MOD_EXPR:
    case BIT_AND_EXPR:
    case BIT_IOR_EXPR:
    case BIT_XOR_EXPR:
    case TRUTH_ANDIF_EXPR:
    case TRUTH_ORIF_EXPR:
      return cp_build_binary_op (code, arg1, arg2);

    case CONVERT_EXPR:
    case NEGATE_EXPR:
    case BIT_NOT_EXPR:
    case TRUTH_NOT_EXPR:
    case PREINCREMENT_EXPR:
    case POSTINCREMENT_EXPR:
    case PREDECREMENT_EXPR:
    case POSTDECREMENT_EXPR:
    case REALPART_EXPR:
    case IMAGPART_EXPR:
      return build_unary_op (code, arg1, candidates != 0);

    case ARRAY_REF:
      return build_array_ref (arg1, arg2);

    case COND_EXPR:
      return build_conditional_expr (arg1, arg2, arg3);

    case MEMBER_REF:
      return build_m_component_ref
	(build_indirect_ref (arg1, NULL), arg2);

      /* The caller will deal with these.  */
    case ADDR_EXPR:
    case COMPONENT_REF:
    case COMPOUND_EXPR:
      return NULL_TREE;

    default:
      abort ();
      return NULL_TREE;
    }
}

/* Build a call to operator delete.  This has to be handled very specially,
   because the restrictions on what signatures match are different from all
   other call instances.  For a normal delete, only a delete taking (void *)
   or (void *, size_t) is accepted.  For a placement delete, only an exact
   match with the placement new is accepted.

   CODE is either DELETE_EXPR or VEC_DELETE_EXPR.
   ADDR is the pointer to be deleted.
   SIZE is the size of the memory block to be deleted.
   FLAGS are the usual overloading flags.
   PLACEMENT is the corresponding placement new call, or NULL_TREE.  */

tree
build_op_delete_call (enum tree_code code, tree addr, tree size,
                      int flags, tree placement)
{
  tree fn = NULL_TREE;
  tree fns, fnname, argtypes, args, type;
  int pass;

  if (addr == error_mark_node)
    return error_mark_node;

  type = strip_array_types (TREE_TYPE (TREE_TYPE (addr)));

  fnname = ansi_opname (code);

  if (IS_AGGR_TYPE (type) && ! (flags & LOOKUP_GLOBAL))
    /* In [class.free]

       If the result of the lookup is ambiguous or inaccessible, or if
       the lookup selects a placement deallocation function, the
       program is ill-formed.
  
       Therefore, we ask lookup_fnfields to complain about ambiguity.  */
    {
      fns = lookup_fnfields (TYPE_BINFO (type), fnname, 1);
      if (fns == error_mark_node)
	return error_mark_node;
    }
  else
    fns = NULL_TREE;

  if (fns == NULL_TREE)
    fns = lookup_name_nonclass (fnname);

  if (placement)
    {
      tree alloc_fn;
      tree call_expr;

      /* Find the allocation function that is being called.  */
      call_expr = placement;
      /* Extract the function.  */
      alloc_fn = get_callee_fndecl (call_expr);
      my_friendly_assert (alloc_fn != NULL_TREE, 20020327);
      /* Then the second parm type.  */
      argtypes = TREE_CHAIN (TYPE_ARG_TYPES (TREE_TYPE (alloc_fn)));
      /* Also the second argument.  */
      args = TREE_CHAIN (TREE_OPERAND (call_expr, 1));
    }
  else
    {
      /* First try it without the size argument.  */
      argtypes = void_list_node;
      args = NULL_TREE;
    }

  /* Strip const and volatile from addr.  */
  addr = cp_convert (ptr_type_node, addr);

  /* We make two tries at finding a matching `operator delete'.  On
     the first pass, we look for a one-operator (or placement)
     operator delete.  If we're not doing placement delete, then on
     the second pass we look for a two-argument delete.  */
  for (pass = 0; pass < (placement ? 1 : 2); ++pass) 
    {
      /* Go through the `operator delete' functions looking for one
	 with a matching type.  */
      for (fn = BASELINK_P (fns) ? BASELINK_FUNCTIONS (fns) : fns; 
	   fn; 
	   fn = OVL_NEXT (fn))
	{
	  tree t;

	  /* The first argument must be "void *".  */
	  t = TYPE_ARG_TYPES (TREE_TYPE (OVL_CURRENT (fn)));
	  if (!same_type_p (TREE_VALUE (t), ptr_type_node))
	    continue;
	  t = TREE_CHAIN (t);
	  /* On the first pass, check the rest of the arguments.  */
	  if (pass == 0)
	    {
	      tree a = argtypes;
	      while (a && t)
		{
		  if (!same_type_p (TREE_VALUE (a), TREE_VALUE (t)))
		    break;
		  a = TREE_CHAIN (a);
		  t = TREE_CHAIN (t);
		}
	      if (!a && !t)
		break;
	    }
	  /* On the second pass, the second argument must be
	     "size_t".  */
	  else if (pass == 1
		   && same_type_p (TREE_VALUE (t), sizetype)
		   && TREE_CHAIN (t) == void_list_node)
	    break;
	}

      /* If we found a match, we're done.  */
      if (fn)
	break;
    }

  /* If we have a matching function, call it.  */
  if (fn)
    {
      /* Make sure we have the actual function, and not an
	 OVERLOAD.  */
      fn = OVL_CURRENT (fn);

      /* If the FN is a member function, make sure that it is
	 accessible.  */
      if (DECL_CLASS_SCOPE_P (fn))
	perform_or_defer_access_check (TYPE_BINFO (type), fn);

      if (pass == 0)
	args = tree_cons (NULL_TREE, addr, args);
      else
	args = tree_cons (NULL_TREE, addr, 
			  build_tree_list (NULL_TREE, size));

      if (placement)
	{
	  /* The placement args might not be suitable for overload
	     resolution at this point, so build the call directly.  */
	  mark_used (fn);
	  return build_cxx_call (fn, args, args);
	}
      else
	return build_function_call (fn, args);
    }

  /* If we are doing placement delete we do nothing if we don't find a
     matching op delete.  */
  if (placement)
    return NULL_TREE;

  error ("no suitable `operator %s' for `%T'",
	 operator_name_info[(int)code].name, type);
  return error_mark_node;
}

/* If the current scope isn't allowed to access DECL along
   BASETYPE_PATH, give an error.  The most derived class in
   BASETYPE_PATH is the one used to qualify DECL.  */

bool
enforce_access (tree basetype_path, tree decl)
{
  my_friendly_assert (TREE_CODE (basetype_path) == TREE_VEC, 20030624);
  
  if (!accessible_p (basetype_path, decl))
    {
      if (TREE_PRIVATE (decl))
	cp_error_at ("`%+#D' is private", decl);
      else if (TREE_PROTECTED (decl))
	cp_error_at ("`%+#D' is protected", decl);
      else
	cp_error_at ("`%+#D' is inaccessible", decl);
      error ("within this context");
      return false;
    }

  return true;
}

/* Check that a callable constructor to initialize a temporary of
   TYPE from an EXPR exists.  */

static void
check_constructor_callable (tree type, tree expr)
{
  build_special_member_call (NULL_TREE,
			     complete_ctor_identifier,
			     build_tree_list (NULL_TREE, expr), 
			     TYPE_BINFO (type),
			     LOOKUP_NORMAL | LOOKUP_ONLYCONVERTING
			     | LOOKUP_NO_CONVERSION
			     | LOOKUP_CONSTRUCTOR_CALLABLE);
}

/* Initialize a temporary of type TYPE with EXPR.  The FLAGS are a
   bitwise or of LOOKUP_* values.  If any errors are warnings are
   generated, set *DIAGNOSTIC_FN to "error" or "warning",
   respectively.  If no diagnostics are generated, set *DIAGNOSTIC_FN
   to NULL.  */

static tree
build_temp (tree expr, tree type, int flags, 
	    void (**diagnostic_fn)(const char *, ...))
{
  int savew, savee;
  
  savew = warningcount, savee = errorcount;
  expr = build_special_member_call (NULL_TREE,
				    complete_ctor_identifier,
				    build_tree_list (NULL_TREE, expr), 
				    TYPE_BINFO (type),
				    flags);
  if (warningcount > savew)
    *diagnostic_fn = warning;
  else if (errorcount > savee)
    *diagnostic_fn = error;
  else
    *diagnostic_fn = NULL;
  return expr;
}
	    

/* Perform the conversions in CONVS on the expression EXPR.  FN and
   ARGNUM are used for diagnostics.  ARGNUM is zero based, -1
   indicates the `this' argument of a method.  INNER is nonzero when
   being called to continue a conversion chain. It is negative when a
   reference binding will be applied, positive otherwise.  If
   ISSUE_CONVERSION_WARNINGS is true, warnings about suspicious
   conversions will be emitted if appropriate.  */

static tree
convert_like_real (tree convs, tree expr, tree fn, int argnum, int inner,
		   bool issue_conversion_warnings)
{
  tree totype = TREE_TYPE (convs);
  void (*diagnostic_fn)(const char *, ...);

  if (ICS_BAD_FLAG (convs)
      && TREE_CODE (convs) != USER_CONV
      && TREE_CODE (convs) != AMBIG_CONV
      && TREE_CODE (convs) != REF_BIND)
    {
      tree t = convs; 
      for (; t; t = TREE_OPERAND (t, 0))
	{
	  if (TREE_CODE (t) == USER_CONV || !ICS_BAD_FLAG (t))
	    {
	      expr = convert_like_real (t, expr, fn, argnum, 1,
					/*issue_conversion_warnings=*/false);
	      break;
	    }
	  else if (TREE_CODE (t) == AMBIG_CONV)
	    return convert_like_real (t, expr, fn, argnum, 1,
				      /*issue_conversion_warnings=*/false);
	  else if (TREE_CODE (t) == IDENTITY_CONV)
	    break;
	}
      pedwarn ("invalid conversion from `%T' to `%T'", TREE_TYPE (expr), totype);
      if (fn)
	pedwarn ("  initializing argument %P of `%D'", argnum, fn);
      return cp_convert (totype, expr);
    }
  
  if (issue_conversion_warnings)
    expr = dubious_conversion_warnings
             (totype, expr, "converting", fn, argnum);
  switch (TREE_CODE (convs))
    {
    case USER_CONV:
      {
	struct z_candidate *cand = USER_CONV_CAND (convs);
	tree convfn = cand->fn;
	tree args;

	if (DECL_CONSTRUCTOR_P (convfn))
	  {
	    tree t = build_int_2 (0, 0);
	    TREE_TYPE (t) = build_pointer_type (DECL_CONTEXT (convfn));

	    args = build_tree_list (NULL_TREE, expr);
	    if (DECL_HAS_IN_CHARGE_PARM_P (convfn)
		|| DECL_HAS_VTT_PARM_P (convfn))
	      /* We should never try to call the abstract or base constructor
		 from here.  */
	      abort ();
	    args = tree_cons (NULL_TREE, t, args);
	  }
	else
	  args = build_this (expr);
	expr = build_over_call (cand, LOOKUP_NORMAL);

	/* If this is a constructor or a function returning an aggr type,
	   we need to build up a TARGET_EXPR.  */
	if (DECL_CONSTRUCTOR_P (convfn))
	  expr = build_cplus_new (totype, expr);

	/* The result of the call is then used to direct-initialize the object
	   that is the destination of the copy-initialization.  [dcl.init]

	   Note that this step is not reflected in the conversion sequence;
	   it affects the semantics when we actually perform the
	   conversion, but is not considered during overload resolution.

	   If the target is a class, that means call a ctor.  */
	if (IS_AGGR_TYPE (totype)
	    && (inner >= 0 || !lvalue_p (expr)))
	  {
	    expr = (build_temp 
		    (expr, totype, 
		     /* Core issue 84, now a DR, says that we don't
			allow UDCs for these args (which deliberately
			breaks copy-init of an auto_ptr<Base> from an
			auto_ptr<Derived>).  */
		     LOOKUP_NORMAL|LOOKUP_ONLYCONVERTING|LOOKUP_NO_CONVERSION,
		     &diagnostic_fn));
		    
	    if (diagnostic_fn)
	      {
		if (fn)
		  diagnostic_fn 
		    ("  initializing argument %P of `%D' from result of `%D'",
		     argnum, fn, convfn);
		else
		 diagnostic_fn 
		   ("  initializing temporary from result of `%D'",  convfn);
	      }
	    expr = build_cplus_new (totype, expr);
	  }
	return expr;
      }
    case IDENTITY_CONV:
      if (type_unknown_p (expr))
	expr = instantiate_type (totype, expr, tf_error | tf_warning);
      /* Convert a non-array constant variable to its underlying
	 value, unless we are about to bind it to a reference, in
	 which case we need to leave it as an lvalue.  */
      if (inner >= 0
	  && TREE_CODE (TREE_TYPE (expr)) != ARRAY_TYPE)
	expr = decl_constant_value (expr);
      if (CHECK_COPY_CONSTRUCTOR_P (convs))
	check_constructor_callable (totype, expr);
      
      return expr;
    case AMBIG_CONV:
      /* Call build_user_type_conversion again for the error.  */
      return build_user_type_conversion
	(totype, TREE_OPERAND (convs, 0), LOOKUP_NORMAL);

    default:
      break;
    };

  expr = convert_like_real (TREE_OPERAND (convs, 0), expr, fn, argnum,
                            TREE_CODE (convs) == REF_BIND ? -1 : 1,
			    /*issue_conversion_warnings=*/false);
  if (expr == error_mark_node)
    return error_mark_node;

  switch (TREE_CODE (convs))
    {
    case RVALUE_CONV:
      if (! IS_AGGR_TYPE (totype))
	return expr;
      /* Else fall through.  */
    case BASE_CONV:
      if (TREE_CODE (convs) == BASE_CONV && !NEED_TEMPORARY_P (convs))
	{
	  /* We are going to bind a reference directly to a base-class
	     subobject of EXPR.  */
	  if (CHECK_COPY_CONSTRUCTOR_P (convs))
	    check_constructor_callable (TREE_TYPE (expr), expr);
	  /* Build an expression for `*((base*) &expr)'.  */
	  expr = build_unary_op (ADDR_EXPR, expr, 0);
	  expr = perform_implicit_conversion (build_pointer_type (totype), 
					      expr);
	  expr = build_indirect_ref (expr, "implicit conversion");
	  return expr;
	}

      /* Copy-initialization where the cv-unqualified version of the source
	 type is the same class as, or a derived class of, the class of the
	 destination [is treated as direct-initialization].  [dcl.init] */
      expr = build_temp (expr, totype, LOOKUP_NORMAL|LOOKUP_ONLYCONVERTING,
			 &diagnostic_fn);
      if (diagnostic_fn && fn)
	diagnostic_fn ("  initializing argument %P of `%D'", argnum, fn);
      return build_cplus_new (totype, expr);

    case REF_BIND:
      {
	tree ref_type = totype;

	/* If necessary, create a temporary.  */
	if (NEED_TEMPORARY_P (convs) || !lvalue_p (expr))
	  {
	    tree type = TREE_TYPE (TREE_OPERAND (convs, 0));

	    if (!CP_TYPE_CONST_NON_VOLATILE_P (TREE_TYPE (ref_type)))
	      {
		/* If the reference is volatile or non-const, we
		   cannot create a temporary.  */
		cp_lvalue_kind lvalue = real_lvalue_p (expr);
		
		if (lvalue & clk_bitfield)
		  error ("cannot bind bitfield `%E' to `%T'",
			 expr, ref_type);
		else if (lvalue & clk_packed)
		  error ("cannot bind packed field `%E' to `%T'",
			 expr, ref_type);
		else
		  error ("cannot bind rvalue `%E' to `%T'", expr, ref_type);
		return error_mark_node;
	      }
	    expr = build_target_expr_with_type (expr, type);
	  }

	/* Take the address of the thing to which we will bind the
	   reference.  */
	expr = build_unary_op (ADDR_EXPR, expr, 1);
	if (expr == error_mark_node)
	  return error_mark_node;

	/* Convert it to a pointer to the type referred to by the
	   reference.  This will adjust the pointer if a derived to
	   base conversion is being performed.  */
	expr = cp_convert (build_pointer_type (TREE_TYPE (ref_type)), 
			   expr);
	/* Convert the pointer to the desired reference type.  */
	return build_nop (ref_type, expr);
      }

    case LVALUE_CONV:
      return decay_conversion (expr);

    case QUAL_CONV:
      /* Warn about deprecated conversion if appropriate.  */
      string_conv_p (totype, expr, 1);
      break;
      
    default:
      break;
    }
  return ocp_convert (totype, expr, CONV_IMPLICIT,
		      LOOKUP_NORMAL|LOOKUP_NO_CONVERSION);
}

/* Build a call to __builtin_trap.  */

static tree
call_builtin_trap (void)
{
  tree fn = IDENTIFIER_GLOBAL_VALUE (get_identifier ("__builtin_trap"));

  my_friendly_assert (fn != NULL, 20030927);
  fn = build_call (fn, NULL_TREE);
  return fn;
}

/* ARG is being passed to a varargs function.  Perform any conversions
   required.  Return the converted value.  */

tree
convert_arg_to_ellipsis (tree arg)
{
  /* [expr.call]

     The lvalue-to-rvalue, array-to-pointer, and function-to-pointer
     standard conversions are performed.  */
  arg = decay_conversion (arg);
  /* [expr.call]

     If the argument has integral or enumeration type that is subject
     to the integral promotions (_conv.prom_), or a floating point
     type that is subject to the floating point promotion
     (_conv.fpprom_), the value of the argument is converted to the
     promoted type before the call.  */
  if (TREE_CODE (TREE_TYPE (arg)) == REAL_TYPE
      && (TYPE_PRECISION (TREE_TYPE (arg))
	  < TYPE_PRECISION (double_type_node)))
    arg = convert_to_real (double_type_node, arg);
  else if (INTEGRAL_OR_ENUMERATION_TYPE_P (TREE_TYPE (arg)))
    arg = perform_integral_promotions (arg);

  arg = require_complete_type (arg);
  
  if (arg != error_mark_node
      && !pod_type_p (TREE_TYPE (arg)))
    {
      /* Undefined behavior [expr.call] 5.2.2/7.  We used to just warn
	 here and do a bitwise copy, but now cp_expr_size will abort if we
	 try to do that. 
	 If the call appears in the context of a sizeof expression, 
	 there is no need to emit a warning, since the expression won't be 
	 evaluated. We keep the builtin_trap just as a safety check.  */
      if (!skip_evaluation)
	warning ("cannot pass objects of non-POD type `%#T' through `...'; "
	         "call will abort at runtime", TREE_TYPE (arg));
      arg = call_builtin_trap ();
      arg = build (COMPOUND_EXPR, integer_type_node, arg,
		   integer_zero_node);
    }

  return arg;
}

/* va_arg (EXPR, TYPE) is a builtin. Make sure it is not abused.  */

tree
build_x_va_arg (tree expr, tree type)
{
  if (processing_template_decl)
    return build_min (VA_ARG_EXPR, type, expr);
  
  type = complete_type_or_else (type, NULL_TREE);

  if (expr == error_mark_node || !type)
    return error_mark_node;
  
  if (! pod_type_p (type))
    {
      /* Undefined behavior [expr.call] 5.2.2/7.  */
      warning ("cannot receive objects of non-POD type `%#T' through `...'; \
call will abort at runtime",
	       type);
      expr = convert (build_pointer_type (type), null_node);
      expr = build (COMPOUND_EXPR, TREE_TYPE (expr),
		    call_builtin_trap (), expr);
      expr = build_indirect_ref (expr, NULL);
      return expr;
    }
  
  return build_va_arg (expr, type);
}

/* TYPE has been given to va_arg.  Apply the default conversions which
   would have happened when passed via ellipsis.  Return the promoted
   type, or the passed type if there is no change.  */

tree
cxx_type_promotes_to (tree type)
{
  tree promote;

  /* Perform the array-to-pointer and function-to-pointer
     conversions.  */
  type = type_decays_to (type);

  promote = type_promotes_to (type);
  if (same_type_p (type, promote))
    promote = type;
  
  return promote;
}

/* ARG is a default argument expression being passed to a parameter of
   the indicated TYPE, which is a parameter to FN.  Do any required
   conversions.  Return the converted value.  */

tree
convert_default_arg (tree type, tree arg, tree fn, int parmnum)
{
  /* If the ARG is an unparsed default argument expression, the
     conversion cannot be performed.  */
  if (TREE_CODE (arg) == DEFAULT_ARG)
    {
      error ("the default argument for parameter %d of `%D' has "
	     "not yet been parsed",
	     parmnum, fn);
      return error_mark_node;
    }

  if (fn && DECL_TEMPLATE_INFO (fn))
    arg = tsubst_default_argument (fn, type, arg);

  arg = break_out_target_exprs (arg);

  if (TREE_CODE (arg) == CONSTRUCTOR)
    {
      arg = digest_init (type, arg, 0);
      arg = convert_for_initialization (0, type, arg, LOOKUP_NORMAL,
					"default argument", fn, parmnum);
    }
  else
    {
      /* This could get clobbered by the following call.  */
      if (TREE_HAS_CONSTRUCTOR (arg))
	arg = copy_node (arg);

      arg = convert_for_initialization (0, type, arg, LOOKUP_NORMAL,
					"default argument", fn, parmnum);
      arg = convert_for_arg_passing (type, arg);
    }

  return arg;
}

/* Returns the type which will really be used for passing an argument of
   type TYPE.  */

tree
type_passed_as (tree type)
{
  /* Pass classes with copy ctors by invisible reference.  */
  if (TREE_ADDRESSABLE (type))
    type = build_reference_type (type);
  else if (PROMOTE_PROTOTYPES
	   && INTEGRAL_TYPE_P (type)
	   && COMPLETE_TYPE_P (type)
	   && INT_CST_LT_UNSIGNED (TYPE_SIZE (type),
				   TYPE_SIZE (integer_type_node)))
    type = integer_type_node;

  return type;
}

/* Actually perform the appropriate conversion.  */

tree
convert_for_arg_passing (tree type, tree val)
{
  if (val == error_mark_node)
    ;
  /* Pass classes with copy ctors by invisible reference.  */
  else if (TREE_ADDRESSABLE (type))
    val = build1 (ADDR_EXPR, build_reference_type (type), val);
  else if (PROMOTE_PROTOTYPES
	   && INTEGRAL_TYPE_P (type)
	   && COMPLETE_TYPE_P (type)
	   && INT_CST_LT_UNSIGNED (TYPE_SIZE (type),
				   TYPE_SIZE (integer_type_node)))
    val = perform_integral_promotions (val);
  return val;
}

/* Returns true iff FN is a function with magic varargs, i.e. ones for
   which no conversions at all should be done.  This is true for some
   builtins which don't act like normal functions.  */

static bool
magic_varargs_p (tree fn)
{
  if (DECL_BUILT_IN (fn))
    switch (DECL_FUNCTION_CODE (fn))
      {
      case BUILT_IN_CLASSIFY_TYPE:
      case BUILT_IN_CONSTANT_P:
      case BUILT_IN_NEXT_ARG:
      case BUILT_IN_STDARG_START:
      case BUILT_IN_VA_START:
	return true;

      default:;
      }

  return false;
}

/* Subroutine of the various build_*_call functions.  Overload resolution
   has chosen a winning candidate CAND; build up a CALL_EXPR accordingly.
   ARGS is a TREE_LIST of the unconverted arguments to the call.  FLAGS is a
   bitmask of various LOOKUP_* flags which apply to the call itself.  */

static tree
build_over_call (struct z_candidate *cand, int flags)
{
  tree fn = cand->fn;
  tree args = cand->args;
  tree convs = cand->convs;
  tree converted_args = NULL_TREE;
  tree parm = TYPE_ARG_TYPES (TREE_TYPE (fn));
  tree conv, arg, val;
  int i = 0;
  int is_method = 0;

  /* In a template, there is no need to perform all of the work that
     is normally done.  We are only interested in the type of the call
     expression, i.e., the return type of the function.  Any semantic
     errors will be deferred until the template is instantiated.  */
  if (processing_template_decl)
    {
      tree expr;
      tree return_type;
      return_type = TREE_TYPE (TREE_TYPE (fn));
      expr = build (CALL_EXPR, return_type, fn, args);
      if (TREE_THIS_VOLATILE (fn) && cfun)
	current_function_returns_abnormally = 1;
      if (!VOID_TYPE_P (return_type))
	require_complete_type (return_type);
      return convert_from_reference (expr);
    }

  /* Give any warnings we noticed during overload resolution.  */
  if (cand->warnings)
    for (val = cand->warnings; val; val = TREE_CHAIN (val))
      joust (cand, WRAPPER_ZC (TREE_VALUE (val)), 1);

  if (DECL_FUNCTION_MEMBER_P (fn))
    {
      /* If FN is a template function, two cases must be considered.
	 For example:

	   struct A {
	     protected:
	       template <class T> void f();
	   };
	   template <class T> struct B {
	     protected:
	       void g();
	   };
	   struct C : A, B<int> {
	     using A::f;	// #1
	     using B<int>::g;	// #2
	   };

	 In case #1 where `A::f' is a member template, DECL_ACCESS is
	 recorded in the primary template but not in its specialization.
	 We check access of FN using its primary template.

	 In case #2, where `B<int>::g' has a DECL_TEMPLATE_INFO simply
	 because it is a member of class template B, DECL_ACCESS is
	 recorded in the specialization `B<int>::g'.  We cannot use its
	 primary template because `B<T>::g' and `B<int>::g' may have
	 different access.  */
      if (DECL_TEMPLATE_INFO (fn)
	  && is_member_template (DECL_TI_TEMPLATE (fn)))
	perform_or_defer_access_check (cand->access_path,
				       DECL_TI_TEMPLATE (fn));
      else
	perform_or_defer_access_check (cand->access_path, fn);
    }

  if (args && TREE_CODE (args) != TREE_LIST)
    args = build_tree_list (NULL_TREE, args);
  arg = args;

  /* The implicit parameters to a constructor are not considered by overload
     resolution, and must be of the proper type.  */
  if (DECL_CONSTRUCTOR_P (fn))
    {
      converted_args = tree_cons (NULL_TREE, TREE_VALUE (arg), converted_args);
      arg = TREE_CHAIN (arg);
      parm = TREE_CHAIN (parm);
      if (DECL_HAS_IN_CHARGE_PARM_P (fn))
	/* We should never try to call the abstract constructor.  */
	abort ();
      if (DECL_HAS_VTT_PARM_P (fn))
	{
	  converted_args = tree_cons
	    (NULL_TREE, TREE_VALUE (arg), converted_args);
	  arg = TREE_CHAIN (arg);
	  parm = TREE_CHAIN (parm);
	}
    }      
  /* Bypass access control for 'this' parameter.  */
  else if (TREE_CODE (TREE_TYPE (fn)) == METHOD_TYPE)
    {
      tree parmtype = TREE_VALUE (parm);
      tree argtype = TREE_TYPE (TREE_VALUE (arg));
      tree converted_arg;
      tree base_binfo;
      
      if (ICS_BAD_FLAG (TREE_VEC_ELT (convs, i)))
	pedwarn ("passing `%T' as `this' argument of `%#D' discards qualifiers",
		    TREE_TYPE (argtype), fn);

      /* [class.mfct.nonstatic]: If a nonstatic member function of a class
	 X is called for an object that is not of type X, or of a type
	 derived from X, the behavior is undefined.

         So we can assume that anything passed as 'this' is non-null, and
	 optimize accordingly.  */
      my_friendly_assert (TREE_CODE (parmtype) == POINTER_TYPE, 19990811);
      /* Convert to the base in which the function was declared.  */
      my_friendly_assert (cand->conversion_path != NULL_TREE, 20020730);
      converted_arg = build_base_path (PLUS_EXPR,
				       TREE_VALUE (arg),
				       cand->conversion_path,
				       1);
      /* Check that the base class is accessible.  */
      if (!accessible_base_p (TREE_TYPE (argtype), 
			      BINFO_TYPE (cand->conversion_path)))
	error ("`%T' is not an accessible base of `%T'",
	       BINFO_TYPE (cand->conversion_path),
	       TREE_TYPE (argtype));
      /* If fn was found by a using declaration, the conversion path
         will be to the derived class, not the base declaring fn. We
         must convert from derived to base.  */
      base_binfo = lookup_base (TREE_TYPE (TREE_TYPE (converted_arg)),
				TREE_TYPE (parmtype), ba_ignore, NULL);
      converted_arg = build_base_path (PLUS_EXPR, converted_arg,
				       base_binfo, 1);
      
      converted_args = tree_cons (NULL_TREE, converted_arg, converted_args);
      parm = TREE_CHAIN (parm);
      arg = TREE_CHAIN (arg);
      ++i;
      is_method = 1;
    }

  for (; arg && parm;
       parm = TREE_CHAIN (parm), arg = TREE_CHAIN (arg), ++i)
    {
      tree type = TREE_VALUE (parm);

      conv = TREE_VEC_ELT (convs, i);
      val = convert_like_with_context
	(conv, TREE_VALUE (arg), fn, i - is_method);

      val = convert_for_arg_passing (type, val);
      converted_args = tree_cons (NULL_TREE, val, converted_args);
    }

  /* Default arguments */
  for (; parm && parm != void_list_node; parm = TREE_CHAIN (parm), i++)
    converted_args 
      = tree_cons (NULL_TREE, 
		   convert_default_arg (TREE_VALUE (parm), 
					TREE_PURPOSE (parm),
					fn, i - is_method),
		   converted_args);

  /* Ellipsis */
  for (; arg; arg = TREE_CHAIN (arg))
    {
      tree a = TREE_VALUE (arg);
      if (magic_varargs_p (fn))
	/* Do no conversions for magic varargs.  */;
      else
	a = convert_arg_to_ellipsis (a);
      converted_args = tree_cons (NULL_TREE, a, converted_args);
    }

  converted_args = nreverse (converted_args);

  if (warn_format)
    check_function_format (NULL, TYPE_ATTRIBUTES (TREE_TYPE (fn)),
			   converted_args);

  /* Avoid actually calling copy constructors and copy assignment operators,
     if possible.  */

  if (! flag_elide_constructors)
    /* Do things the hard way.  */;
  else if (TREE_VEC_LENGTH (convs) == 1
	   && DECL_COPY_CONSTRUCTOR_P (fn))
    {
      tree targ;
      arg = skip_artificial_parms_for (fn, converted_args);
      arg = TREE_VALUE (arg);

      /* Pull out the real argument, disregarding const-correctness.  */
      targ = arg;
      while (TREE_CODE (targ) == NOP_EXPR
	     || TREE_CODE (targ) == NON_LVALUE_EXPR
	     || TREE_CODE (targ) == CONVERT_EXPR)
	targ = TREE_OPERAND (targ, 0);
      if (TREE_CODE (targ) == ADDR_EXPR)
	{
	  targ = TREE_OPERAND (targ, 0);
	  if (!same_type_ignoring_top_level_qualifiers_p 
	      (TREE_TYPE (TREE_TYPE (arg)), TREE_TYPE (targ)))
	    targ = NULL_TREE;
	}
      else
	targ = NULL_TREE;

      if (targ)
	arg = targ;
      else
	arg = build_indirect_ref (arg, 0);

      /* [class.copy]: the copy constructor is implicitly defined even if
	 the implementation elided its use.  */
      if (TYPE_HAS_COMPLEX_INIT_REF (DECL_CONTEXT (fn)))
	mark_used (fn);

      /* If we're creating a temp and we already have one, don't create a
         new one.  If we're not creating a temp but we get one, use
         INIT_EXPR to collapse the temp into our target.  Otherwise, if the
         ctor is trivial, do a bitwise copy with a simple TARGET_EXPR for a
         temp or an INIT_EXPR otherwise.  */
      if (integer_zerop (TREE_VALUE (args)))
	{
	  if (TREE_CODE (arg) == TARGET_EXPR)
	    return arg;
	  else if (TYPE_HAS_TRIVIAL_INIT_REF (DECL_CONTEXT (fn)))
	    return build_target_expr_with_type (arg, DECL_CONTEXT (fn));
	}
      else if (TREE_CODE (arg) == TARGET_EXPR
	       || TYPE_HAS_TRIVIAL_INIT_REF (DECL_CONTEXT (fn)))
	{
	  tree to = stabilize_reference
	    (build_indirect_ref (TREE_VALUE (args), 0));

	  val = build (INIT_EXPR, DECL_CONTEXT (fn), to, arg);
	  return val;
	}
    }
  else if (DECL_OVERLOADED_OPERATOR_P (fn) == NOP_EXPR
	   && copy_fn_p (fn)
	   && TYPE_HAS_TRIVIAL_ASSIGN_REF (DECL_CONTEXT (fn)))
    {
      tree to = stabilize_reference
	(build_indirect_ref (TREE_VALUE (converted_args), 0));
      tree type = TREE_TYPE (to);
      tree as_base = CLASSTYPE_AS_BASE (type);

      arg = build_indirect_ref (TREE_VALUE (TREE_CHAIN (converted_args)), 0);
      if (tree_int_cst_equal (TYPE_SIZE (type), TYPE_SIZE (as_base)))
	val = build (MODIFY_EXPR, TREE_TYPE (to), to, arg);
      else
	{
	  /* We must only copy the non-tail padding parts. Use
	     CLASSTYPE_AS_BASE for the bitwise copy.  */
	  tree to_ptr, arg_ptr, to_as_base, arg_as_base, base_ptr_type;
	  tree save_to;

	  to_ptr = save_expr (build_unary_op (ADDR_EXPR, to, 0));
	  arg_ptr = build_unary_op (ADDR_EXPR, arg, 0);

	  base_ptr_type = build_pointer_type (as_base);
	  to_as_base = build_nop (base_ptr_type, to_ptr);
	  to_as_base = build_indirect_ref (to_as_base, 0);
	  arg_as_base = build_nop (base_ptr_type, arg_ptr);
	  arg_as_base = build_indirect_ref (arg_as_base, 0);

	  save_to = build_indirect_ref (to_ptr, 0);

	  val = build (MODIFY_EXPR, as_base, to_as_base, arg_as_base);
	  val = convert_to_void (val, NULL);
	  val = build (COMPOUND_EXPR, type, val, save_to);
	  TREE_NO_UNUSED_WARNING (val) = 1;
	}
      
      return val;
    }

  mark_used (fn);

  if (DECL_VINDEX (fn) && (flags & LOOKUP_NONVIRTUAL) == 0)
    {
      tree t, *p = &TREE_VALUE (converted_args);
      tree binfo = lookup_base (TREE_TYPE (TREE_TYPE (*p)),
				DECL_CONTEXT (fn),
				ba_any, NULL);
      my_friendly_assert (binfo && binfo != error_mark_node, 20010730);
      
      *p = build_base_path (PLUS_EXPR, *p, binfo, 1);
      if (TREE_SIDE_EFFECTS (*p))
	*p = save_expr (*p);
      t = build_pointer_type (TREE_TYPE (fn));
      if (DECL_CONTEXT (fn) && TYPE_JAVA_INTERFACE (DECL_CONTEXT (fn)))
	fn = build_java_interface_fn_ref (fn, *p);
      else
	fn = build_vfn_ref (build_indirect_ref (*p, 0), DECL_VINDEX (fn));
      TREE_TYPE (fn) = t;
    }
  else if (DECL_INLINE (fn))
    fn = inline_conversion (fn);
  else
    fn = build_addr_func (fn);

  return build_cxx_call (fn, args, converted_args);
}

/* Build and return a call to FN, using the the CONVERTED_ARGS.  ARGS
   gives the original form of the arguments.  This function performs
   no overload resolution, conversion, or other high-level
   operations.  */

tree
build_cxx_call(tree fn, tree args, tree converted_args)
{
  tree fndecl;

  /* Recognize certain built-in functions so we can make tree-codes
     other than CALL_EXPR.  We do this when it enables fold-const.c
     to do something useful.  */
  if (TREE_CODE (fn) == ADDR_EXPR
      && TREE_CODE (TREE_OPERAND (fn, 0)) == FUNCTION_DECL
      && DECL_BUILT_IN (TREE_OPERAND (fn, 0)))
    {
      tree exp;
      exp = expand_tree_builtin (TREE_OPERAND (fn, 0), args, converted_args);
      if (exp)
	return exp;
    }

  fn = build_call (fn, converted_args);

  /* If this call might throw an exception, note that fact.  */
  fndecl = get_callee_fndecl (fn);
  if ((!fndecl || !TREE_NOTHROW (fndecl)) 
      && at_function_scope_p ()
      && cfun)
    cp_function_chain->can_throw = 1;

  /* Some built-in function calls will be evaluated at compile-time in
     fold ().  */
  fn = fold (fn);

  if (VOID_TYPE_P (TREE_TYPE (fn)))
    return fn;

  fn = require_complete_type (fn);
  if (fn == error_mark_node)
    return error_mark_node;

  if (IS_AGGR_TYPE (TREE_TYPE (fn)))
    fn = build_cplus_new (TREE_TYPE (fn), fn);
  return convert_from_reference (fn);
}

static GTY(()) tree java_iface_lookup_fn;

/* Make an expression which yields the address of the Java interface
   method FN.  This is achieved by generating a call to libjava's
   _Jv_LookupInterfaceMethodIdx().  */

static tree
build_java_interface_fn_ref (tree fn, tree instance)
{
  tree lookup_args, lookup_fn, method, idx;
  tree klass_ref, iface, iface_ref;
  int i;
  
  if (!java_iface_lookup_fn)
    {
      tree endlink = build_void_list_node ();
      tree t = tree_cons (NULL_TREE, ptr_type_node,
			  tree_cons (NULL_TREE, ptr_type_node,
				     tree_cons (NULL_TREE, java_int_type_node,
						endlink)));
      java_iface_lookup_fn 
	= builtin_function ("_Jv_LookupInterfaceMethodIdx",
			    build_function_type (ptr_type_node, t),
			    0, NOT_BUILT_IN, NULL, NULL_TREE);
    }

  /* Look up the pointer to the runtime java.lang.Class object for `instance'. 
     This is the first entry in the vtable.  */
  klass_ref = build_vtbl_ref (build_indirect_ref (instance, 0), 
			      integer_zero_node);

  /* Get the java.lang.Class pointer for the interface being called.  */
  iface = DECL_CONTEXT (fn);
  iface_ref = lookup_field (iface, get_identifier ("class$"), 0, false);
  if (!iface_ref || TREE_CODE (iface_ref) != VAR_DECL
      || DECL_CONTEXT (iface_ref) != iface)
    {
      error ("could not find class$ field in java interface type `%T'", 
		iface);
      return error_mark_node;
    }
  iface_ref = build1 (ADDR_EXPR, build_pointer_type (iface), iface_ref);
  
  /* Determine the itable index of FN.  */
  i = 1;
  for (method = TYPE_METHODS (iface); method; method = TREE_CHAIN (method))
    {
      if (!DECL_VIRTUAL_P (method))
        continue;
      if (fn == method)
        break;
      i++;
    }
  idx = build_int_2 (i, 0);

  lookup_args = tree_cons (NULL_TREE, klass_ref, 
			   tree_cons (NULL_TREE, iface_ref,
				      build_tree_list (NULL_TREE, idx)));
  lookup_fn = build1 (ADDR_EXPR, 
		      build_pointer_type (TREE_TYPE (java_iface_lookup_fn)),
		      java_iface_lookup_fn);
  return build (CALL_EXPR, ptr_type_node, lookup_fn, lookup_args, NULL_TREE);
}

/* Returns the value to use for the in-charge parameter when making a
   call to a function with the indicated NAME.  */

tree
in_charge_arg_for_name (tree name)
{
  if (name == base_ctor_identifier
      || name == base_dtor_identifier)
    return integer_zero_node;
  else if (name == complete_ctor_identifier)
    return integer_one_node;
  else if (name == complete_dtor_identifier)
    return integer_two_node;
  else if (name == deleting_dtor_identifier)
    return integer_three_node;

  /* This function should only be called with one of the names listed
     above.  */
  abort ();
  return NULL_TREE;
}

/* Build a call to a constructor, destructor, or an assignment
   operator for INSTANCE, an expression with class type.  NAME
   indicates the special member function to call; ARGS are the
   arguments.  BINFO indicates the base of INSTANCE that is to be
   passed as the `this' parameter to the member function called.

   FLAGS are the LOOKUP_* flags to use when processing the call.

   If NAME indicates a complete object constructor, INSTANCE may be
   NULL_TREE.  In this case, the caller will call build_cplus_new to
   store the newly constructed object into a VAR_DECL.  */

tree
build_special_member_call (tree instance, tree name, tree args, 
			   tree binfo, int flags)
{
  tree fns;
  /* The type of the subobject to be constructed or destroyed.  */
  tree class_type;

  my_friendly_assert (name == complete_ctor_identifier
		      || name == base_ctor_identifier
		      || name == complete_dtor_identifier
		      || name == base_dtor_identifier
		      || name == deleting_dtor_identifier
		      || name == ansi_assopname (NOP_EXPR),
		      20020712);
  my_friendly_assert (binfo != NULL_TREE, 20020712);

  class_type = BINFO_TYPE (binfo);

  /* Handle the special case where INSTANCE is NULL_TREE.  */
  if (name == complete_ctor_identifier && !instance)
    {
      instance = build_int_2 (0, 0);
      TREE_TYPE (instance) = build_pointer_type (class_type);
      instance = build1 (INDIRECT_REF, class_type, instance);
    }
  else
    {
      if (name == complete_dtor_identifier 
	  || name == base_dtor_identifier
	  || name == deleting_dtor_identifier)
	my_friendly_assert (args == NULL_TREE, 20020712);

      /* Convert to the base class, if necessary.  */
      if (!same_type_ignoring_top_level_qualifiers_p 
	  (TREE_TYPE (instance), BINFO_TYPE (binfo)))
	{
	  if (name != ansi_assopname (NOP_EXPR))
	    /* For constructors and destructors, either the base is
	       non-virtual, or it is virtual but we are doing the
	       conversion from a constructor or destructor for the
	       complete object.  In either case, we can convert
	       statically.  */
	    instance = convert_to_base_statically (instance, binfo);
	  else
	    /* However, for assignment operators, we must convert
	       dynamically if the base is virtual.  */
	    instance = build_base_path (PLUS_EXPR, instance,
					binfo, /*nonnull=*/1);
	}
    }
  
  my_friendly_assert (instance != NULL_TREE, 20020712);

  /* Resolve the name.  */
  if (!complete_type_or_else (BINFO_TYPE (binfo), NULL_TREE))
    return error_mark_node;

  fns = lookup_fnfields (binfo, name, 1);
    
  /* When making a call to a constructor or destructor for a subobject
     that uses virtual base classes, pass down a pointer to a VTT for
     the subobject.  */
  if ((name == base_ctor_identifier
       || name == base_dtor_identifier)
      && TYPE_USES_VIRTUAL_BASECLASSES (class_type))
    {
      tree vtt;
      tree sub_vtt;

      /* If the current function is a complete object constructor
	 or destructor, then we fetch the VTT directly.
	 Otherwise, we look it up using the VTT we were given.  */
      vtt = TREE_CHAIN (CLASSTYPE_VTABLES (current_class_type));
      vtt = decay_conversion (vtt);
      vtt = build (COND_EXPR, TREE_TYPE (vtt),
		   build (EQ_EXPR, boolean_type_node,
			  current_in_charge_parm, integer_zero_node),
		   current_vtt_parm,
		   vtt);
      my_friendly_assert (BINFO_SUBVTT_INDEX (binfo), 20010110);
      sub_vtt = build (PLUS_EXPR, TREE_TYPE (vtt), vtt,
		       BINFO_SUBVTT_INDEX (binfo));

      args = tree_cons (NULL_TREE, sub_vtt, args);
    }

  return build_new_method_call (instance, fns, args, 
				TYPE_BINFO (BINFO_TYPE (binfo)), 
				flags);
}

/* Return the NAME, as a C string.  The NAME indicates a function that
   is a member of TYPE.  *FREE_P is set to true if the caller must
   free the memory returned.  

   Rather than go through all of this, we should simply set the names
   of constructors and destructors appropriately, and dispense with
   ctor_identifier, dtor_identifier, etc.  */

static char *
name_as_c_string (tree name, tree type, bool *free_p)
{
  char *pretty_name;

  /* Assume that we will not allocate memory.  */
  *free_p = false;
  /* Constructors and destructors are special.  */
  if (IDENTIFIER_CTOR_OR_DTOR_P (name))
    {
      pretty_name 
	= (char *) IDENTIFIER_POINTER (constructor_name (type));
      /* For a destructor, add the '~'.  */
      if (name == complete_dtor_identifier
	  || name == base_dtor_identifier
	  || name == deleting_dtor_identifier)
	{
	  pretty_name = concat ("~", pretty_name, NULL);
	  /* Remember that we need to free the memory allocated.  */
	  *free_p = true;
	}
    }
  else if (IDENTIFIER_TYPENAME_P (name))
    {
      pretty_name = concat ("operator ",
			    type_as_string (TREE_TYPE (name),
					    TFF_PLAIN_IDENTIFIER),
			    NULL);
      /* Remember that we need to free the memory allocated.  */
      *free_p = true;
    }
  else
    pretty_name = (char *) IDENTIFIER_POINTER (name);

  return pretty_name;
}

/* Build a call to "INSTANCE.FN (ARGS)".  */

tree
build_new_method_call (tree instance, tree fns, tree args, 
		       tree conversion_path, int flags)
{
  struct z_candidate *candidates = 0, *cand;
  tree explicit_targs = NULL_TREE;
  tree basetype = NULL_TREE;
  tree access_binfo;
  tree optype;
  tree mem_args = NULL_TREE, instance_ptr;
  tree name;
  tree user_args;
  tree call;
  tree fn;
  tree class_type;
  int template_only = 0;
  bool any_viable_p;
  tree orig_instance;
  tree orig_fns;
  tree orig_args;

  my_friendly_assert (instance != NULL_TREE, 20020729);

  if (error_operand_p (instance) 
      || error_operand_p (fns)
      || args == error_mark_node)
    return error_mark_node;

  orig_instance = instance;
  orig_fns = fns;
  orig_args = args;

  if (processing_template_decl)
    {
      instance = build_non_dependent_expr (instance);
      if (!BASELINK_P (fns)
	  && TREE_CODE (fns) != PSEUDO_DTOR_EXPR
	  && TREE_TYPE (fns) != unknown_type_node)
	fns = build_non_dependent_expr (fns);
      args = build_non_dependent_args (orig_args);
    }

  /* Process the argument list.  */
  user_args = args;
  args = resolve_args (args);
  if (args == error_mark_node)
    return error_mark_node;

  if (TREE_CODE (TREE_TYPE (instance)) == REFERENCE_TYPE)
    instance = convert_from_reference (instance);
  basetype = TYPE_MAIN_VARIANT (TREE_TYPE (instance));
  instance_ptr = build_this (instance);

  if (!BASELINK_P (fns))
    {
      error ("call to non-function `%D'", fns);
      return error_mark_node;
    }

  if (!conversion_path)
    conversion_path = BASELINK_BINFO (fns);
  access_binfo = BASELINK_ACCESS_BINFO (fns);
  optype = BASELINK_OPTYPE (fns);
  fns = BASELINK_FUNCTIONS (fns);

  if (TREE_CODE (fns) == TEMPLATE_ID_EXPR)
    {
      explicit_targs = TREE_OPERAND (fns, 1);
      fns = TREE_OPERAND (fns, 0);
      template_only = 1;
    }

  my_friendly_assert (TREE_CODE (fns) == FUNCTION_DECL
		      || TREE_CODE (fns) == TEMPLATE_DECL
		      || TREE_CODE (fns) == OVERLOAD,
		      20020712);

  /* XXX this should be handled before we get here.  */
  if (! IS_AGGR_TYPE (basetype))
    {
      if ((flags & LOOKUP_COMPLAIN) && basetype != error_mark_node)
	error ("request for member `%D' in `%E', which is of non-aggregate type `%T'",
	       fns, instance, basetype);

      return error_mark_node;
    }

  fn = get_first_fn (fns);
  name = DECL_NAME (fn);

  if (IDENTIFIER_CTOR_OR_DTOR_P (name))
    {
      /* Callers should explicitly indicate whether they want to construct
	 the complete object or just the part without virtual bases.  */
      my_friendly_assert (name != ctor_identifier, 20000408);
      /* Similarly for destructors.  */
      my_friendly_assert (name != dtor_identifier, 20000408);
    }

  /* It's OK to call destructors on cv-qualified objects.  Therefore,
     convert the INSTANCE_PTR to the unqualified type, if necessary.  */
  if (DECL_DESTRUCTOR_P (fn))
    {
      tree type = build_pointer_type (basetype);
      if (!same_type_p (type, TREE_TYPE (instance_ptr)))
	instance_ptr = build_nop (type, instance_ptr);
    }

  class_type = (conversion_path ? BINFO_TYPE (conversion_path) : NULL_TREE);
  mem_args = tree_cons (NULL_TREE, instance_ptr, args);

  for (fn = fns; fn; fn = OVL_NEXT (fn))
    {
      tree t = OVL_CURRENT (fn);
      tree this_arglist;

      /* We can end up here for copy-init of same or base class.  */
      if ((flags & LOOKUP_ONLYCONVERTING)
	  && DECL_NONCONVERTING_P (t))
	continue;

      if (DECL_NONSTATIC_MEMBER_FUNCTION_P (t))
	this_arglist = mem_args;
      else
	this_arglist = args;

      if (TREE_CODE (t) == TEMPLATE_DECL)
	/* A member template.  */
	add_template_candidate (&candidates, t, 
				class_type,
				explicit_targs,
				this_arglist, optype,
				access_binfo, 
				conversion_path,
				flags,
				DEDUCE_CALL);
      else if (! template_only)
	add_function_candidate (&candidates, t, 
				class_type,
				this_arglist,
				access_binfo,
				conversion_path,
				flags);
    }

  candidates = splice_viable (candidates, pedantic, &any_viable_p);
  if (!any_viable_p)
    {
      /* XXX will LOOKUP_SPECULATIVELY be needed when this is done?  */
      if (flags & LOOKUP_SPECULATIVELY)
	return NULL_TREE;
      if (!COMPLETE_TYPE_P (basetype))
	cxx_incomplete_type_error (instance_ptr, basetype);
      else
	{
	  char *pretty_name;
	  bool free_p;

	  pretty_name = name_as_c_string (name, basetype, &free_p);
	  error ("no matching function for call to `%T::%s(%A)%#V'",
		 basetype, pretty_name, user_args,
		 TREE_TYPE (TREE_TYPE (instance_ptr)));
	  if (free_p)
	    free (pretty_name);
	}
      print_z_candidates (candidates);
      return error_mark_node;
    }

  cand = tourney (candidates);
  if (cand == 0)
    {
      char *pretty_name;
      bool free_p;

      pretty_name = name_as_c_string (name, basetype, &free_p);
      error ("call of overloaded `%s(%A)' is ambiguous", pretty_name,
	     user_args);
      print_z_candidates (candidates);
      if (free_p)
	free (pretty_name);
      return error_mark_node;
    }

  if (DECL_PURE_VIRTUAL_P (cand->fn)
      && instance == current_class_ref
      && (DECL_CONSTRUCTOR_P (current_function_decl)
	  || DECL_DESTRUCTOR_P (current_function_decl))
      && ! (flags & LOOKUP_NONVIRTUAL)
      && value_member (cand->fn, CLASSTYPE_PURE_VIRTUALS (basetype)))
    error ((DECL_CONSTRUCTOR_P (current_function_decl) ? 
	       "abstract virtual `%#D' called from constructor"
	       : "abstract virtual `%#D' called from destructor"),
	      cand->fn);
  if (TREE_CODE (TREE_TYPE (cand->fn)) == METHOD_TYPE
      && is_dummy_object (instance_ptr))
    {
      error ("cannot call member function `%D' without object", cand->fn);
      return error_mark_node;
    }

  if (DECL_VINDEX (cand->fn) && ! (flags & LOOKUP_NONVIRTUAL)
      && resolves_to_fixed_type_p (instance, 0))
    flags |= LOOKUP_NONVIRTUAL;

  if (TREE_CODE (TREE_TYPE (cand->fn)) == METHOD_TYPE)
    call = build_over_call (cand, flags);
  else
    {
      call = build_over_call (cand, flags);
      /* In an expression of the form `a->f()' where `f' turns out to
	 be a static member function, `a' is none-the-less evaluated.  */
      if (!is_dummy_object (instance_ptr) && TREE_SIDE_EFFECTS (instance))
	call = build (COMPOUND_EXPR, TREE_TYPE (call), instance, call);
    }
  
  if (processing_template_decl && call != error_mark_node)
    return build_min_non_dep
      (CALL_EXPR, call,
       build_min_nt (COMPONENT_REF, orig_instance, orig_fns),
       orig_args);
  return call;
}

/* Returns true iff standard conversion sequence ICS1 is a proper
   subsequence of ICS2.  */

static bool
is_subseq (tree ics1, tree ics2)
{
  /* We can assume that a conversion of the same code
     between the same types indicates a subsequence since we only get
     here if the types we are converting from are the same.  */

  while (TREE_CODE (ics1) == RVALUE_CONV
	 || TREE_CODE (ics1) == LVALUE_CONV)
    ics1 = TREE_OPERAND (ics1, 0);

  while (1)
    {
      while (TREE_CODE (ics2) == RVALUE_CONV
	  || TREE_CODE (ics2) == LVALUE_CONV)
	ics2 = TREE_OPERAND (ics2, 0);

      if (TREE_CODE (ics2) == USER_CONV
	  || TREE_CODE (ics2) == AMBIG_CONV
	  || TREE_CODE (ics2) == IDENTITY_CONV)
	/* At this point, ICS1 cannot be a proper subsequence of
	   ICS2.  We can get a USER_CONV when we are comparing the
	   second standard conversion sequence of two user conversion
	   sequences.  */
	return false;

      ics2 = TREE_OPERAND (ics2, 0);

      if (TREE_CODE (ics2) == TREE_CODE (ics1)
	  && same_type_p (TREE_TYPE (ics2), TREE_TYPE (ics1))
	  && same_type_p (TREE_TYPE (TREE_OPERAND (ics2, 0)),
			     TREE_TYPE (TREE_OPERAND (ics1, 0))))
	return true;
    }
}

/* Returns nonzero iff DERIVED is derived from BASE.  The inputs may
   be any _TYPE nodes.  */

bool
is_properly_derived_from (tree derived, tree base)
{
  if (!IS_AGGR_TYPE_CODE (TREE_CODE (derived))
      || !IS_AGGR_TYPE_CODE (TREE_CODE (base)))
    return false;

  /* We only allow proper derivation here.  The DERIVED_FROM_P macro
     considers every class derived from itself.  */
  return (!same_type_ignoring_top_level_qualifiers_p (derived, base)
	  && DERIVED_FROM_P (base, derived));
}

/* We build the ICS for an implicit object parameter as a pointer
   conversion sequence.  However, such a sequence should be compared
   as if it were a reference conversion sequence.  If ICS is the
   implicit conversion sequence for an implicit object parameter,
   modify it accordingly.  */

static void
maybe_handle_implicit_object (tree *ics)
{
  if (ICS_THIS_FLAG (*ics))
    {
      /* [over.match.funcs]
	 
	 For non-static member functions, the type of the
	 implicit object parameter is "reference to cv X"
	 where X is the class of which the function is a
	 member and cv is the cv-qualification on the member
	 function declaration.  */
      tree t = *ics;
      tree reference_type;

      /* The `this' parameter is a pointer to a class type.  Make the
	 implicit conversion talk about a reference to that same class
	 type.  */
      reference_type = TREE_TYPE (TREE_TYPE (*ics));
      reference_type = build_reference_type (reference_type);

      if (TREE_CODE (t) == QUAL_CONV)
	t = TREE_OPERAND (t, 0);
      if (TREE_CODE (t) == PTR_CONV)
	t = TREE_OPERAND (t, 0);
      t = build1 (IDENTITY_CONV, TREE_TYPE (TREE_TYPE (t)), NULL_TREE);
      t = direct_reference_binding (reference_type, t); 
      *ics = t;
    }
}

/* If *ICS is a REF_BIND set *ICS to the remainder of the conversion,
   and return the type to which the reference refers.  Otherwise,
   leave *ICS unchanged and return NULL_TREE.  */

static tree
maybe_handle_ref_bind (tree *ics)
{
  if (TREE_CODE (*ics) == REF_BIND)
    {
      tree old_ics = *ics;
      tree type = TREE_TYPE (TREE_TYPE (old_ics));
      *ics = TREE_OPERAND (old_ics, 0);
      ICS_USER_FLAG (*ics) = ICS_USER_FLAG (old_ics);
      ICS_BAD_FLAG (*ics) = ICS_BAD_FLAG (old_ics);
      return type;
    }

  return NULL_TREE;
}

/* Compare two implicit conversion sequences according to the rules set out in
   [over.ics.rank].  Return values:

      1: ics1 is better than ics2
     -1: ics2 is better than ics1
      0: ics1 and ics2 are indistinguishable */

static int
compare_ics (tree ics1, tree ics2)
{
  tree from_type1;
  tree from_type2;
  tree to_type1;
  tree to_type2;
  tree deref_from_type1 = NULL_TREE;
  tree deref_from_type2 = NULL_TREE;
  tree deref_to_type1 = NULL_TREE;
  tree deref_to_type2 = NULL_TREE;
  int rank1, rank2;

  /* REF_BINDING is nonzero if the result of the conversion sequence
     is a reference type.   In that case TARGET_TYPE is the
     type referred to by the reference.  */
  tree target_type1;
  tree target_type2;

  /* Handle implicit object parameters.  */
  maybe_handle_implicit_object (&ics1);
  maybe_handle_implicit_object (&ics2);

  /* Handle reference parameters.  */
  target_type1 = maybe_handle_ref_bind (&ics1);
  target_type2 = maybe_handle_ref_bind (&ics2);

  /* [over.ics.rank]

     When  comparing  the  basic forms of implicit conversion sequences (as
     defined in _over.best.ics_)

     --a standard conversion sequence (_over.ics.scs_) is a better
       conversion sequence than a user-defined conversion sequence
       or an ellipsis conversion sequence, and
     
     --a user-defined conversion sequence (_over.ics.user_) is a
       better conversion sequence than an ellipsis conversion sequence
       (_over.ics.ellipsis_).  */
  rank1 = ICS_RANK (ics1);
  rank2 = ICS_RANK (ics2);
  
  if (rank1 > rank2)
    return -1;
  else if (rank1 < rank2)
    return 1;

  if (rank1 == BAD_RANK)
    {
      /* XXX Isn't this an extension? */
      /* Both ICS are bad.  We try to make a decision based on what
	 would have happened if they'd been good.  */
      if (ICS_USER_FLAG (ics1) > ICS_USER_FLAG (ics2)
	  || ICS_STD_RANK (ics1) > ICS_STD_RANK (ics2))
	return -1;
      else if (ICS_USER_FLAG (ics1) < ICS_USER_FLAG (ics2)
	       || ICS_STD_RANK (ics1) < ICS_STD_RANK (ics2))
	return 1;

      /* We couldn't make up our minds; try to figure it out below.  */
    }

  if (ICS_ELLIPSIS_FLAG (ics1))
    /* Both conversions are ellipsis conversions.  */
    return 0;

  /* User-defined  conversion sequence U1 is a better conversion sequence
     than another user-defined conversion sequence U2 if they contain the
     same user-defined conversion operator or constructor and if the sec-
     ond standard conversion sequence of U1 is  better  than  the  second
     standard conversion sequence of U2.  */

  if (ICS_USER_FLAG (ics1))
    {
      tree t1, t2;

      for (t1 = ics1; TREE_CODE (t1) != USER_CONV; t1 = TREE_OPERAND (t1, 0))
	if (TREE_CODE (t1) == AMBIG_CONV)
	  return 0;
      for (t2 = ics2; TREE_CODE (t2) != USER_CONV; t2 = TREE_OPERAND (t2, 0))
	if (TREE_CODE (t2) == AMBIG_CONV)
	  return 0;

      if (USER_CONV_FN (t1) != USER_CONV_FN (t2))
	return 0;

      /* We can just fall through here, after setting up
	 FROM_TYPE1 and FROM_TYPE2.  */
      from_type1 = TREE_TYPE (t1);
      from_type2 = TREE_TYPE (t2);
    }
  else
    {
      /* We're dealing with two standard conversion sequences. 

	 [over.ics.rank]
	 
	 Standard conversion sequence S1 is a better conversion
	 sequence than standard conversion sequence S2 if
     
	 --S1 is a proper subsequence of S2 (comparing the conversion
	   sequences in the canonical form defined by _over.ics.scs_,
	   excluding any Lvalue Transformation; the identity
	   conversion sequence is considered to be a subsequence of
	   any non-identity conversion sequence */
      
      from_type1 = ics1;
      while (TREE_CODE (from_type1) != IDENTITY_CONV)
	from_type1 = TREE_OPERAND (from_type1, 0);
      from_type1 = TREE_TYPE (from_type1);
      
      from_type2 = ics2;
      while (TREE_CODE (from_type2) != IDENTITY_CONV)
	from_type2 = TREE_OPERAND (from_type2, 0);
      from_type2 = TREE_TYPE (from_type2);
    }

  if (same_type_p (from_type1, from_type2))
    {
      if (is_subseq (ics1, ics2))
	return 1;
      if (is_subseq (ics2, ics1))
	return -1;
    }
  /* Otherwise, one sequence cannot be a subsequence of the other; they
     don't start with the same type.  This can happen when comparing the
     second standard conversion sequence in two user-defined conversion
     sequences.  */

  /* [over.ics.rank]

     Or, if not that,

     --the rank of S1 is better than the rank of S2 (by the rules
       defined below):

    Standard conversion sequences are ordered by their ranks: an Exact
    Match is a better conversion than a Promotion, which is a better
    conversion than a Conversion.

    Two conversion sequences with the same rank are indistinguishable
    unless one of the following rules applies:

    --A conversion that is not a conversion of a pointer, or pointer
      to member, to bool is better than another conversion that is such
      a conversion.  

    The ICS_STD_RANK automatically handles the pointer-to-bool rule,
    so that we do not have to check it explicitly.  */
  if (ICS_STD_RANK (ics1) < ICS_STD_RANK (ics2))
    return 1;
  else if (ICS_STD_RANK (ics2) < ICS_STD_RANK (ics1))
    return -1;

  to_type1 = TREE_TYPE (ics1);
  to_type2 = TREE_TYPE (ics2);

  if (TYPE_PTR_P (from_type1)
      && TYPE_PTR_P (from_type2)
      && TYPE_PTR_P (to_type1)
      && TYPE_PTR_P (to_type2))
    {
      deref_from_type1 = TREE_TYPE (from_type1);
      deref_from_type2 = TREE_TYPE (from_type2);
      deref_to_type1 = TREE_TYPE (to_type1);
      deref_to_type2 = TREE_TYPE (to_type2);
    }
  /* The rules for pointers to members A::* are just like the rules
     for pointers A*, except opposite: if B is derived from A then
     A::* converts to B::*, not vice versa.  For that reason, we
     switch the from_ and to_ variables here.  */
  else if ((TYPE_PTRMEM_P (from_type1) && TYPE_PTRMEM_P (from_type2)
	    && TYPE_PTRMEM_P (to_type1) && TYPE_PTRMEM_P (to_type2))
	   || (TYPE_PTRMEMFUNC_P (from_type1)
	       && TYPE_PTRMEMFUNC_P (from_type2)
	       && TYPE_PTRMEMFUNC_P (to_type1)
	       && TYPE_PTRMEMFUNC_P (to_type2)))
    {
      deref_to_type1 = TYPE_PTRMEM_CLASS_TYPE (from_type1);
      deref_to_type2 = TYPE_PTRMEM_CLASS_TYPE (from_type2);
      deref_from_type1 = TYPE_PTRMEM_CLASS_TYPE (to_type1);
      deref_from_type2 = TYPE_PTRMEM_CLASS_TYPE (to_type2);
    }

  if (deref_from_type1 != NULL_TREE
      && IS_AGGR_TYPE_CODE (TREE_CODE (deref_from_type1))
      && IS_AGGR_TYPE_CODE (TREE_CODE (deref_from_type2)))
    {
      /* This was one of the pointer or pointer-like conversions.  

	 [over.ics.rank]
	 
	 --If class B is derived directly or indirectly from class A,
	   conversion of B* to A* is better than conversion of B* to
	   void*, and conversion of A* to void* is better than
	   conversion of B* to void*.  */
      if (TREE_CODE (deref_to_type1) == VOID_TYPE
	  && TREE_CODE (deref_to_type2) == VOID_TYPE)
	{
	  if (is_properly_derived_from (deref_from_type1,
					deref_from_type2))
	    return -1;
	  else if (is_properly_derived_from (deref_from_type2,
					     deref_from_type1))
	    return 1;
	}
      else if (TREE_CODE (deref_to_type1) == VOID_TYPE
	       || TREE_CODE (deref_to_type2) == VOID_TYPE)
	{
	  if (same_type_p (deref_from_type1, deref_from_type2))
	    {
	      if (TREE_CODE (deref_to_type2) == VOID_TYPE)
		{
		  if (is_properly_derived_from (deref_from_type1,
						deref_to_type1))
		    return 1;
		}
	      /* We know that DEREF_TO_TYPE1 is `void' here.  */
	      else if (is_properly_derived_from (deref_from_type1,
						 deref_to_type2))
		return -1;
	    }
	}
      else if (IS_AGGR_TYPE_CODE (TREE_CODE (deref_to_type1))
	       && IS_AGGR_TYPE_CODE (TREE_CODE (deref_to_type2)))
	{
	  /* [over.ics.rank]

	     --If class B is derived directly or indirectly from class A
	       and class C is derived directly or indirectly from B,
	     
	     --conversion of C* to B* is better than conversion of C* to
	       A*, 
	     
	     --conversion of B* to A* is better than conversion of C* to
	       A*  */
	  if (same_type_p (deref_from_type1, deref_from_type2))
	    {
	      if (is_properly_derived_from (deref_to_type1,
					    deref_to_type2))
		return 1;
	      else if (is_properly_derived_from (deref_to_type2,
						 deref_to_type1))
		return -1;
	    }
	  else if (same_type_p (deref_to_type1, deref_to_type2))
	    {
	      if (is_properly_derived_from (deref_from_type2,
					    deref_from_type1))
		return 1;
	      else if (is_properly_derived_from (deref_from_type1,
						 deref_from_type2))
		return -1;
	    }
	}
    }
  else if (CLASS_TYPE_P (non_reference (from_type1))
	   && same_type_p (from_type1, from_type2))
    {
      tree from = non_reference (from_type1);

      /* [over.ics.rank]
	 
	 --binding of an expression of type C to a reference of type
	   B& is better than binding an expression of type C to a
	   reference of type A&

	 --conversion of C to B is better than conversion of C to A,  */
      if (is_properly_derived_from (from, to_type1)
	  && is_properly_derived_from (from, to_type2))
	{
	  if (is_properly_derived_from (to_type1, to_type2))
	    return 1;
	  else if (is_properly_derived_from (to_type2, to_type1))
	    return -1;
	}
    }
  else if (CLASS_TYPE_P (non_reference (to_type1))
	   && same_type_p (to_type1, to_type2))
    {
      tree to = non_reference (to_type1);

      /* [over.ics.rank]

	 --binding of an expression of type B to a reference of type
	   A& is better than binding an expression of type C to a
	   reference of type A&, 

	 --onversion of B to A is better than conversion of C to A  */
      if (is_properly_derived_from (from_type1, to)
	  && is_properly_derived_from (from_type2, to))
	{
	  if (is_properly_derived_from (from_type2, from_type1))
	    return 1;
	  else if (is_properly_derived_from (from_type1, from_type2))
	    return -1;
	}
    }

  /* [over.ics.rank]

     --S1 and S2 differ only in their qualification conversion and  yield
       similar  types  T1 and T2 (_conv.qual_), respectively, and the cv-
       qualification signature of type T1 is a proper subset of  the  cv-
       qualification signature of type T2  */
  if (TREE_CODE (ics1) == QUAL_CONV 
      && TREE_CODE (ics2) == QUAL_CONV
      && same_type_p (from_type1, from_type2))
    return comp_cv_qual_signature (to_type1, to_type2);

  /* [over.ics.rank]
     
     --S1 and S2 are reference bindings (_dcl.init.ref_), and the
     types to which the references refer are the same type except for
     top-level cv-qualifiers, and the type to which the reference
     initialized by S2 refers is more cv-qualified than the type to
     which the reference initialized by S1 refers */
      
  if (target_type1 && target_type2
      && same_type_ignoring_top_level_qualifiers_p (to_type1, to_type2))
    return comp_cv_qualification (target_type2, target_type1);

  /* Neither conversion sequence is better than the other.  */
  return 0;
}

/* The source type for this standard conversion sequence.  */

static tree
source_type (tree t)
{
  for (;; t = TREE_OPERAND (t, 0))
    {
      if (TREE_CODE (t) == USER_CONV
	  || TREE_CODE (t) == AMBIG_CONV
	  || TREE_CODE (t) == IDENTITY_CONV)
	return TREE_TYPE (t);
    }
  abort ();
}

/* Note a warning about preferring WINNER to LOSER.  We do this by storing
   a pointer to LOSER and re-running joust to produce the warning if WINNER
   is actually used.  */

static void
add_warning (struct z_candidate *winner, struct z_candidate *loser)
{
  winner->warnings = tree_cons (NULL_TREE,
				build_zc_wrapper (loser),
				winner->warnings);
}

/* Compare two candidates for overloading as described in
   [over.match.best].  Return values:

      1: cand1 is better than cand2
     -1: cand2 is better than cand1
      0: cand1 and cand2 are indistinguishable */

static int
joust (struct z_candidate *cand1, struct z_candidate *cand2, bool warn)
{
  int winner = 0;
  int i, off1 = 0, off2 = 0, len;

  /* Candidates that involve bad conversions are always worse than those
     that don't.  */
  if (cand1->viable > cand2->viable)
    return 1;
  if (cand1->viable < cand2->viable)
    return -1;

  /* If we have two pseudo-candidates for conversions to the same type,
     or two candidates for the same function, arbitrarily pick one.  */
  if (cand1->fn == cand2->fn
      && (TYPE_P (cand1->fn) || DECL_P (cand1->fn)))
    return 1;

  /* a viable function F1
     is defined to be a better function than another viable function F2  if
     for  all arguments i, ICSi(F1) is not a worse conversion sequence than
     ICSi(F2), and then */

  /* for some argument j, ICSj(F1) is a better conversion  sequence  than
     ICSj(F2) */

  /* For comparing static and non-static member functions, we ignore
     the implicit object parameter of the non-static function.  The
     standard says to pretend that the static function has an object
     parm, but that won't work with operator overloading.  */
  len = TREE_VEC_LENGTH (cand1->convs);
  if (len != TREE_VEC_LENGTH (cand2->convs))
    {
      if (DECL_STATIC_FUNCTION_P (cand1->fn)
	  && ! DECL_STATIC_FUNCTION_P (cand2->fn))
	off2 = 1;
      else if (! DECL_STATIC_FUNCTION_P (cand1->fn)
	       && DECL_STATIC_FUNCTION_P (cand2->fn))
	{
	  off1 = 1;
	  --len;
	}
      else
	abort ();
    }

  for (i = 0; i < len; ++i)
    {
      tree t1 = TREE_VEC_ELT (cand1->convs, i+off1);
      tree t2 = TREE_VEC_ELT (cand2->convs, i+off2);
      int comp = compare_ics (t1, t2);

      if (comp != 0)
	{
	  if (warn_sign_promo
	      && ICS_RANK (t1) + ICS_RANK (t2) == STD_RANK + PROMO_RANK
	      && TREE_CODE (t1) == STD_CONV
	      && TREE_CODE (t2) == STD_CONV
	      && TREE_CODE (TREE_TYPE (t1)) == INTEGER_TYPE
	      && TREE_CODE (TREE_TYPE (t2)) == INTEGER_TYPE
	      && (TYPE_PRECISION (TREE_TYPE (t1))
		  == TYPE_PRECISION (TREE_TYPE (t2)))
	      && (TREE_UNSIGNED (TREE_TYPE (TREE_OPERAND (t1, 0)))
		  || (TREE_CODE (TREE_TYPE (TREE_OPERAND (t1, 0)))
		      == ENUMERAL_TYPE)))
	    {
	      tree type = TREE_TYPE (TREE_OPERAND (t1, 0));
	      tree type1, type2;
	      struct z_candidate *w, *l;
	      if (comp > 0)
		type1 = TREE_TYPE (t1), type2 = TREE_TYPE (t2),
		  w = cand1, l = cand2;
	      else
		type1 = TREE_TYPE (t2), type2 = TREE_TYPE (t1),
		  w = cand2, l = cand1;

	      if (warn)
		{
		  warning ("passing `%T' chooses `%T' over `%T'",
			      type, type1, type2);
		  warning ("  in call to `%D'", w->fn);
		}
	      else
		add_warning (w, l);
	    }

	  if (winner && comp != winner)
	    {
	      winner = 0;
	      goto tweak;
	    }
	  winner = comp;
	}
    }

  /* warn about confusing overload resolution for user-defined conversions,
     either between a constructor and a conversion op, or between two
     conversion ops.  */
  if (winner && warn_conversion && cand1->second_conv
      && (!DECL_CONSTRUCTOR_P (cand1->fn) || !DECL_CONSTRUCTOR_P (cand2->fn))
      && winner != compare_ics (cand1->second_conv, cand2->second_conv))
    {
      struct z_candidate *w, *l;
      bool give_warning = false;
      
      if (winner == 1)
	w = cand1, l = cand2;
      else
	w = cand2, l = cand1;
      
      /* We don't want to complain about `X::operator T1 ()'
	 beating `X::operator T2 () const', when T2 is a no less
	 cv-qualified version of T1.  */
      if (DECL_CONTEXT (w->fn) == DECL_CONTEXT (l->fn)
	  && !DECL_CONSTRUCTOR_P (w->fn) && !DECL_CONSTRUCTOR_P (l->fn))
	{
	  tree t = TREE_TYPE (TREE_TYPE (l->fn));
	  tree f = TREE_TYPE (TREE_TYPE (w->fn));
	  
	  if (TREE_CODE (t) == TREE_CODE (f) && POINTER_TYPE_P (t))
	    {
	      t = TREE_TYPE (t);
	      f = TREE_TYPE (f);
	    }
	  if (!comp_ptr_ttypes (t, f))
	    give_warning = true;
	}
      else
	give_warning = true;
      
      if (!give_warning)
	/*NOP*/;
      else if (warn)
	{
	  tree source = source_type (TREE_VEC_ELT (w->convs, 0));
	  if (! DECL_CONSTRUCTOR_P (w->fn))
	    source = TREE_TYPE (source);
	  warning ("choosing `%D' over `%D'", w->fn, l->fn);
	  warning ("  for conversion from `%T' to `%T'",
		   source, TREE_TYPE (w->second_conv));
	  warning ("  because conversion sequence for the argument is better");
	}
      else
	add_warning (w, l);
    }

  if (winner)
    return winner;

  /* or, if not that,
     F1 is a non-template function and F2 is a template function
     specialization.  */
         
  if (! cand1->template && cand2->template)
    return 1;
  else if (cand1->template && ! cand2->template)
    return -1;
  
  /* or, if not that,
     F1 and F2 are template functions and the function template for F1 is
     more specialized than the template for F2 according to the partial
     ordering rules.  */
  
  if (cand1->template && cand2->template)
    {
      winner = more_specialized
        (TI_TEMPLATE (cand1->template), TI_TEMPLATE (cand2->template),
         DEDUCE_ORDER,
         /* Tell the deduction code how many real function arguments
	    we saw, not counting the implicit 'this' argument.  But,
	    add_function_candidate() suppresses the "this" argument
	    for constructors.

	    [temp.func.order]: The presence of unused ellipsis and default
	    arguments has no effect on the partial ordering of function
	    templates.  */
         TREE_VEC_LENGTH (cand1->convs)
	 - (DECL_NONSTATIC_MEMBER_FUNCTION_P (cand1->fn)
	    - DECL_CONSTRUCTOR_P (cand1->fn)));
      if (winner)
        return winner;
    }

  /* or, if not that,
     the  context  is  an  initialization by user-defined conversion (see
     _dcl.init_  and  _over.match.user_)  and  the  standard   conversion
     sequence  from  the return type of F1 to the destination type (i.e.,
     the type of the entity being initialized)  is  a  better  conversion
     sequence  than the standard conversion sequence from the return type
     of F2 to the destination type.  */

  if (cand1->second_conv)
    {
      winner = compare_ics (cand1->second_conv, cand2->second_conv);
      if (winner)
        return winner;
    }
  
  /* Check whether we can discard a builtin candidate, either because we
     have two identical ones or matching builtin and non-builtin candidates.

     (Pedantically in the latter case the builtin which matched the user
     function should not be added to the overload set, but we spot it here.
     
     [over.match.oper]
     ... the builtin candidates include ...
     - do not have the same parameter type list as any non-template
       non-member candidate.  */
                            
  if (TREE_CODE (cand1->fn) == IDENTIFIER_NODE
      || TREE_CODE (cand2->fn) == IDENTIFIER_NODE)
    {
      for (i = 0; i < len; ++i)
	if (!same_type_p (TREE_TYPE (TREE_VEC_ELT (cand1->convs, i)),
			  TREE_TYPE (TREE_VEC_ELT (cand2->convs, i))))
	  break;
      if (i == TREE_VEC_LENGTH (cand1->convs))
	{
	  if (cand1->fn == cand2->fn)
	    /* Two built-in candidates; arbitrarily pick one.  */
	    return 1;
	  else if (TREE_CODE (cand1->fn) == IDENTIFIER_NODE)
	    /* cand1 is built-in; prefer cand2.  */
	    return -1;
	  else
	    /* cand2 is built-in; prefer cand1.  */
	    return 1;
	}
    }

  /* If the two functions are the same (this can happen with declarations
     in multiple scopes and arg-dependent lookup), arbitrarily choose one.  */
  if (DECL_P (cand1->fn) && DECL_P (cand2->fn)
      && equal_functions (cand1->fn, cand2->fn))
    return 1;
 
tweak:

  /* Extension: If the worst conversion for one candidate is worse than the
     worst conversion for the other, take the first.  */
  if (!pedantic)
    {
      int rank1 = IDENTITY_RANK, rank2 = IDENTITY_RANK;
      struct z_candidate *w = 0, *l = 0;

      for (i = 0; i < len; ++i)
	{
	  if (ICS_RANK (TREE_VEC_ELT (cand1->convs, i+off1)) > rank1)
	    rank1 = ICS_RANK (TREE_VEC_ELT (cand1->convs, i+off1));
	  if (ICS_RANK (TREE_VEC_ELT (cand2->convs, i+off2)) > rank2)
	    rank2 = ICS_RANK (TREE_VEC_ELT (cand2->convs, i+off2));
	}
      if (rank1 < rank2)
	winner = 1, w = cand1, l = cand2;
      if (rank1 > rank2)
	winner = -1, w = cand2, l = cand1;
      if (winner)
        {
	  if (warn)
	    {
	      pedwarn ("\
ISO C++ says that these are ambiguous, even \
though the worst conversion for the first is better than \
the worst conversion for the second:");
	      print_z_candidate (_("candidate 1:"), w);
	      print_z_candidate (_("candidate 2:"), l);
	    }
	  else
	    add_warning (w, l);
          return winner;
        }
    }

  my_friendly_assert (!winner, 20010121);
  return 0;
}

/* Given a list of candidates for overloading, find the best one, if any.
   This algorithm has a worst case of O(2n) (winner is last), and a best
   case of O(n/2) (totally ambiguous); much better than a sorting
   algorithm.  */

static struct z_candidate *
tourney (struct z_candidate *candidates)
{
  struct z_candidate *champ = candidates, *challenger;
  int fate;
  int champ_compared_to_predecessor = 0;

  /* Walk through the list once, comparing each current champ to the next
     candidate, knocking out a candidate or two with each comparison.  */

  for (challenger = champ->next; challenger; )
    {
      fate = joust (champ, challenger, 0);
      if (fate == 1)
	challenger = challenger->next;
      else
	{
	  if (fate == 0)
	    {
	      champ = challenger->next;
	      if (champ == 0)
		return 0;
	      champ_compared_to_predecessor = 0;
	    }
	  else
	    {
	      champ = challenger;
	      champ_compared_to_predecessor = 1;
	    }

	  challenger = champ->next;
	}
    }

  /* Make sure the champ is better than all the candidates it hasn't yet
     been compared to.  */

  for (challenger = candidates; 
       challenger != champ 
	 && !(champ_compared_to_predecessor && challenger->next == champ);
       challenger = challenger->next)
    {
      fate = joust (champ, challenger, 0);
      if (fate != 1)
	return 0;
    }

  return champ;
}

/* Returns nonzero if things of type FROM can be converted to TO.  */

bool
can_convert (tree to, tree from)
{
  return can_convert_arg (to, from, NULL_TREE);
}

/* Returns nonzero if ARG (of type FROM) can be converted to TO.  */

bool
can_convert_arg (tree to, tree from, tree arg)
{
  tree t = implicit_conversion (to, from, arg, LOOKUP_NORMAL);
  return (t && ! ICS_BAD_FLAG (t));
}

/* Like can_convert_arg, but allows dubious conversions as well.  */

bool
can_convert_arg_bad (tree to, tree from, tree arg)
{
  return implicit_conversion (to, from, arg, LOOKUP_NORMAL) != 0;
}

/* Convert EXPR to TYPE.  Return the converted expression.

   Note that we allow bad conversions here because by the time we get to
   this point we are committed to doing the conversion.  If we end up
   doing a bad conversion, convert_like will complain.  */

tree
perform_implicit_conversion (tree type, tree expr)
{
  tree conv;
  
  if (error_operand_p (expr))
    return error_mark_node;
  conv = implicit_conversion (type, TREE_TYPE (expr), expr,
			      LOOKUP_NORMAL);
  if (!conv)
    {
      error ("could not convert `%E' to `%T'", expr, type);
      return error_mark_node;
    }

  return convert_like (conv, expr);
}

/* Convert EXPR to TYPE (as a direct-initialization) if that is
   permitted.  If the conversion is valid, the converted expression is
   returned.  Otherwise, NULL_TREE is returned, except in the case
   that TYPE is a class type; in that case, an error is issued.  */

tree
perform_direct_initialization_if_possible (tree type, tree expr)
{
  tree conv;
  
  if (type == error_mark_node || error_operand_p (expr))
    return error_mark_node;
  /* [dcl.init]

     If the destination type is a (possibly cv-qualified) class type:

     -- If the initialization is direct-initialization ...,
     constructors are considered. ... If no constructor applies, or
     the overload resolution is ambiguous, the initialization is
     ill-formed.  */
  if (CLASS_TYPE_P (type))
    {
      expr = build_special_member_call (NULL_TREE, complete_ctor_identifier,
					build_tree_list (NULL_TREE, expr),
					TYPE_BINFO (type),
					LOOKUP_NORMAL);
      return build_cplus_new (type, expr);
    }
  conv = implicit_conversion (type, TREE_TYPE (expr), expr,
			      LOOKUP_NORMAL);
  if (!conv || ICS_BAD_FLAG (conv))
    return NULL_TREE;
  return convert_like_real (conv, expr, NULL_TREE, 0, 0, 
			    /*issue_conversion_warnings=*/false);
}

/* DECL is a VAR_DECL whose type is a REFERENCE_TYPE.  The reference
   is being bound to a temporary.  Create and return a new VAR_DECL
   with the indicated TYPE; this variable will store the value to
   which the reference is bound.  */

tree 
make_temporary_var_for_ref_to_temp (tree decl, tree type)
{
  tree var;

  /* Create the variable.  */
  var = build_decl (VAR_DECL, NULL_TREE, type);
  DECL_ARTIFICIAL (var) = 1;
  TREE_USED (var) = 1;

  /* Register the variable.  */
  if (TREE_STATIC (decl))
    {
      /* Namespace-scope or local static; give it a mangled name.  */
      tree name;

      TREE_STATIC (var) = 1;
      name = mangle_ref_init_variable (decl);
      DECL_NAME (var) = name;
      SET_DECL_ASSEMBLER_NAME (var, name);
      var = pushdecl_top_level (var);
    }
  else
    {
      /* Create a new cleanup level if necessary.  */
      maybe_push_cleanup_level (type);
      /* Don't push unnamed temps.  Do set DECL_CONTEXT, though.  */
      DECL_CONTEXT (var) = current_function_decl;
    }

  return var;
}

/* Convert EXPR to the indicated reference TYPE, in a way suitable for
   initializing a variable of that TYPE.  If DECL is non-NULL, it is
   the VAR_DECL being initialized with the EXPR.  (In that case, the
   type of DECL will be TYPE.)  If DECL is non-NULL, then CLEANUP must
   also be non-NULL, and with *CLEANUP initialized to NULL.  Upon
   return, if *CLEANUP is no longer NULL, it will be a CLEANUP_STMT
   that should be inserted after the returned expression is used to
   initialize DECL.

   Return the converted expression.  */

tree
initialize_reference (tree type, tree expr, tree decl, tree *cleanup)
{
  tree conv;

  if (type == error_mark_node || error_operand_p (expr))
    return error_mark_node;

  conv = reference_binding (type, TREE_TYPE (expr), expr, LOOKUP_NORMAL);
  if (!conv || ICS_BAD_FLAG (conv))
    {
      if (!(TYPE_QUALS (TREE_TYPE (type)) & TYPE_QUAL_CONST)
          && !real_lvalue_p (expr))
        error ("invalid initialization of non-const reference of "
               "type '%T' from a temporary of type '%T'",
               type, TREE_TYPE (expr));
      else
        error ("invalid initialization of reference of type "
	       "'%T' from expression of type '%T'", type, 
	       TREE_TYPE (expr));
      return error_mark_node;
    }

  /* If DECL is non-NULL, then this special rule applies:

       [class.temporary]

       The temporary to which the reference is bound or the temporary
       that is the complete object to which the reference is bound
       persists for the lifetime of the reference.

       The temporaries created during the evaluation of the expression
       initializing the reference, except the temporary to which the
       reference is bound, are destroyed at the end of the
       full-expression in which they are created.

     In that case, we store the converted expression into a new
     VAR_DECL in a new scope.  

     However, we want to be careful not to create temporaries when
     they are not required.  For example, given:

       struct B {}; 
       struct D : public B {};
       D f();
       const B& b = f();

     there is no need to copy the return value from "f"; we can just
     extend its lifetime.  Similarly, given:

       struct S {};
       struct T { operator S(); };
       T t;
       const S& s = t;

    we can extend the lifetime of the return value of the conversion
    operator.  */
  my_friendly_assert (TREE_CODE (conv) == REF_BIND, 20030302);
  if (decl)
    {
      tree var;
      tree base_conv_type;

      /* Skip over the REF_BIND.  */
      conv = TREE_OPERAND (conv, 0);
      /* If the next conversion is a BASE_CONV, skip that too -- but
	 remember that the conversion was required.  */
      if (TREE_CODE (conv) == BASE_CONV && !NEED_TEMPORARY_P (conv))
	{
	  if (CHECK_COPY_CONSTRUCTOR_P (conv))
	    check_constructor_callable (TREE_TYPE (expr), expr);
	  base_conv_type = TREE_TYPE (conv);
	  conv = TREE_OPERAND (conv, 0);
	}
      else
	base_conv_type = NULL_TREE;
      /* Perform the remainder of the conversion.  */
      expr = convert_like_real (conv, expr,
				/*fn=*/NULL_TREE, /*argnum=*/0,
				/*inner=*/-1,
				/*issue_conversion_warnings=*/true);
      if (error_operand_p (expr))
	return error_mark_node;
      if (!real_lvalue_p (expr))
	{
	  tree init;
	  tree type;

	  /* Create the temporary variable.  */
	  type = TREE_TYPE (expr);
	  var = make_temporary_var_for_ref_to_temp (decl, type);
	  layout_decl (var, 0);
	  /* If the rvalue is the result of a function call it will be
	     a TARGET_EXPR.  If it is some other construct (such as a
	     member access expression where the underlying object is
	     itself the result of a function call), turn it into a
	     TARGET_EXPR here.  It is important that EXPR be a
	     TARGET_EXPR below since otherwise the INIT_EXPR will
	     attempt to make a bitwise copy of EXPR to intialize
	     VAR. */
	  if (TREE_CODE (expr) != TARGET_EXPR)
	    expr = get_target_expr (expr);
	  /* Create the INIT_EXPR that will initialize the temporary
	     variable.  */
	  init = build (INIT_EXPR, type, var, expr);
	  if (at_function_scope_p ())
	    {
	      add_decl_stmt (var);
	      *cleanup = cxx_maybe_build_cleanup (var);
	      if (*cleanup)
		/* We must be careful to destroy the temporary only
		   after its initialization has taken place.  If the
		   initialization throws an exception, then the
		   destructor should not be run.  We cannot simply
		   transform INIT into something like:
	     
		     (INIT, ({ CLEANUP_STMT; }))

		   because emit_local_var always treats the
		   initializer as a full-expression.  Thus, the
		   destructor would run too early; it would run at the
		   end of initializing the reference variable, rather
		   than at the end of the block enclosing the
		   reference variable.

		   The solution is to pass back a CLEANUP_STMT which
		   the caller is responsible for attaching to the
		   statement tree.  */
		*cleanup = build_stmt (CLEANUP_STMT, var, *cleanup);
	    }
	  else
	    {
	      rest_of_decl_compilation (var, NULL, /*toplev=*/1, at_eof);
	      if (TYPE_HAS_NONTRIVIAL_DESTRUCTOR (type))
		static_aggregates = tree_cons (NULL_TREE, var,
					       static_aggregates);
	    }
	  /* Use its address to initialize the reference variable.  */
	  expr = build_address (var);
	  expr = build (COMPOUND_EXPR, TREE_TYPE (expr), init, expr);
	}
      else
	/* Take the address of EXPR.  */
	expr = build_unary_op (ADDR_EXPR, expr, 0);
      /* If a BASE_CONV was required, perform it now.  */
      if (base_conv_type)
	expr = (perform_implicit_conversion 
		(build_pointer_type (base_conv_type), expr));
      return build_nop (type, expr);
    }

  /* Perform the conversion.  */
  return convert_like (conv, expr);
}

#include "gt-cp-call.h"
