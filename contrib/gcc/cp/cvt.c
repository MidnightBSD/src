/* Language-level data type conversion for GNU C++.
   Copyright (C) 1987, 1988, 1992, 1993, 1994, 1995, 1996, 1997, 1998,
   1999, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.
   Hacked by Michael Tiemann (tiemann@cygnus.com)

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


/* This file contains the functions for converting C++ expressions
   to different data types.  The only entry point is `convert'.
   Every language front end must have a `convert' function
   but what kind of conversions it does will depend on the language.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "flags.h"
#include "cp-tree.h"
#include "convert.h"
#include "toplev.h"
#include "decl.h"

static tree cp_convert_to_pointer (tree, tree, bool);
static tree convert_to_pointer_force (tree, tree);
static tree build_up_reference (tree, tree, int, tree);
static void warn_ref_binding (tree, tree, tree);

/* Change of width--truncation and extension of integers or reals--
   is represented with NOP_EXPR.  Proper functioning of many things
   assumes that no other conversions can be NOP_EXPRs.

   Conversion between integer and pointer is represented with CONVERT_EXPR.
   Converting integer to real uses FLOAT_EXPR
   and real to integer uses FIX_TRUNC_EXPR.

   Here is a list of all the functions that assume that widening and
   narrowing is always done with a NOP_EXPR:
     In convert.c, convert_to_integer.
     In c-typeck.c, build_binary_op_nodefault (boolean ops),
        and c_common_truthvalue_conversion.
     In expr.c: expand_expr, for operands of a MULT_EXPR.
     In fold-const.c: fold.
     In tree.c: get_narrower and get_unwidened.

   C++: in multiple-inheritance, converting between pointers may involve
   adjusting them by a delta stored within the class definition.  */

/* Subroutines of `convert'.  */

/* if converting pointer to pointer
     if dealing with classes, check for derived->base or vice versa
     else if dealing with method pointers, delegate
     else convert blindly
   else if converting class, pass off to build_type_conversion
   else try C-style pointer conversion.  If FORCE is true then allow
   conversions via virtual bases (these are permitted by reinterpret_cast,
   but not static_cast).  */

static tree
cp_convert_to_pointer (tree type, tree expr, bool force)
{
  tree intype = TREE_TYPE (expr);
  enum tree_code form;
  tree rval;
  if (intype == error_mark_node)
    return error_mark_node;

  if (IS_AGGR_TYPE (intype))
    {
      intype = complete_type (intype);
      if (!COMPLETE_TYPE_P (intype))
	{
	  error ("can't convert from incomplete type `%T' to `%T'",
		    intype, type);
	  return error_mark_node;
	}

      rval = build_type_conversion (type, expr);
      if (rval)
	{
	  if (rval == error_mark_node)
	    error ("conversion of `%E' from `%T' to `%T' is ambiguous",
		      expr, intype, type);
	  return rval;
	}
    }

  /* Handle anachronistic conversions from (::*)() to cv void* or (*)().  */
  if (TREE_CODE (type) == POINTER_TYPE
      && (TREE_CODE (TREE_TYPE (type)) == FUNCTION_TYPE
	  || VOID_TYPE_P (TREE_TYPE (type))))
    {
      /* Allow an implicit this pointer for pointer to member
	 functions.  */
      if (TYPE_PTRMEMFUNC_P (intype))
	{
	  if (pedantic || warn_pmf2ptr)
	    pedwarn ("converting from `%T' to `%T'", intype, type);
	  if (TREE_CODE (expr) == PTRMEM_CST)
	    expr = build_address (PTRMEM_CST_MEMBER (expr));
	  else
	    {
	      tree decl = maybe_dummy_object (TYPE_PTRMEM_CLASS_TYPE (intype), 
					      0);
	      decl = build_address (decl);
	      expr = get_member_function_from_ptrfunc (&decl, expr);
	    }
	}
      else if (TREE_CODE (TREE_TYPE (expr)) == METHOD_TYPE)
	{
	  if (pedantic || warn_pmf2ptr)
	    pedwarn ("converting from `%T' to `%T'", intype, type);
	  expr = build_addr_func (expr);
	}
      if (TREE_CODE (TREE_TYPE (expr)) == POINTER_TYPE)
	return build_nop (type, expr);
      intype = TREE_TYPE (expr);
    }

  if (expr == error_mark_node)
    return error_mark_node;

  form = TREE_CODE (intype);

  if (POINTER_TYPE_P (intype))
    {
      intype = TYPE_MAIN_VARIANT (intype);

      if (TYPE_MAIN_VARIANT (type) != intype
	  && TREE_CODE (type) == POINTER_TYPE
	  && TREE_CODE (TREE_TYPE (type)) == RECORD_TYPE
	  && IS_AGGR_TYPE (TREE_TYPE (type))
	  && IS_AGGR_TYPE (TREE_TYPE (intype))
	  && TREE_CODE (TREE_TYPE (intype)) == RECORD_TYPE)
	{
	  enum tree_code code = PLUS_EXPR;
	  tree binfo;
	  tree intype_class;
	  tree type_class;
	  bool same_p;

	  intype_class = TREE_TYPE (intype);
	  type_class = TREE_TYPE (type);

	  same_p = same_type_p (TYPE_MAIN_VARIANT (intype_class), 
				TYPE_MAIN_VARIANT (type_class));
	  binfo = NULL_TREE;
	  /* Try derived to base conversion.  */
	  if (!same_p)
	    binfo = lookup_base (intype_class, type_class, ba_check, NULL);
	  if (!same_p && !binfo)
	    {
	      /* Try base to derived conversion.  */
	      binfo = lookup_base (type_class, intype_class, ba_check, NULL);
	      code = MINUS_EXPR;
	    }
	  if (binfo == error_mark_node)
	    return error_mark_node;
	  if (binfo || same_p)
	    {
	      if (binfo)
		expr = build_base_path (code, expr, binfo, 0);
	      /* Add any qualifier conversions.  */
	      return build_nop (type, expr);
	    }
	}

      if (TYPE_PTRMEMFUNC_P (type))
	{
	  error ("cannot convert `%E' from type `%T' to type `%T'",
		    expr, intype, type);
	  return error_mark_node;
	}

      return build_nop (type, expr);
    }
  else if (TYPE_PTRMEM_P (type) && TYPE_PTRMEM_P (intype))
    {
      tree b1; 
      tree b2;
      tree binfo;
      enum tree_code code = PLUS_EXPR;
      base_kind bk;

      b1 = TYPE_PTRMEM_CLASS_TYPE (type);
      b2 = TYPE_PTRMEM_CLASS_TYPE (intype);
      binfo = lookup_base (b1, b2, ba_check, &bk);
      if (!binfo)
	{
	  binfo = lookup_base (b2, b1, ba_check, &bk);
	  code = MINUS_EXPR;
	}
      if (binfo == error_mark_node)
	return error_mark_node;

      if (bk == bk_via_virtual)
	{
	  if (force)
	    warning ("pointer to member cast from `%T' to `%T' is via virtual base",
		     intype, type);
	  else
	    {
	      error ("pointer to member cast from `%T' to `%T' is via virtual base",
		     intype, type);
	      return error_mark_node;
	    }
	  /* This is a reinterpret cast, whose result is unspecified.
	     We choose to do nothing.  */
	  return build1 (NOP_EXPR, type, expr);
	}

      if (TREE_CODE (expr) == PTRMEM_CST)
	expr = cplus_expand_constant (expr);

      if (binfo && !integer_zerop (BINFO_OFFSET (binfo)))
	expr = size_binop (code, 
			   build_nop (sizetype, expr),
			   BINFO_OFFSET (binfo));
      return build_nop (type, expr);
    }
  else if (TYPE_PTRMEMFUNC_P (type) && TYPE_PTRMEMFUNC_P (intype))
    return build_ptrmemfunc (TYPE_PTRMEMFUNC_FN_TYPE (type), expr, 0);
  else if (TYPE_PTRMEMFUNC_P (intype))
    {
      if (!warn_pmf2ptr)
	{
	  if (TREE_CODE (expr) == PTRMEM_CST)
	    return cp_convert_to_pointer (type,
					  PTRMEM_CST_MEMBER (expr),
					  force);
	  else if (TREE_CODE (expr) == OFFSET_REF)
	    {
	      tree object = TREE_OPERAND (expr, 0);
	      return get_member_function_from_ptrfunc (&object,
						       TREE_OPERAND (expr, 1));
	    }
	}
      error ("cannot convert `%E' from type `%T' to type `%T'",
		expr, intype, type);
      return error_mark_node;
    }

  if (integer_zerop (expr))
    {
      if (TYPE_PTRMEMFUNC_P (type))
	return build_ptrmemfunc (TYPE_PTRMEMFUNC_FN_TYPE (type), expr, 0);

      if (TYPE_PTRMEM_P (type))
	/* A NULL pointer-to-member is represented by -1, not by
	   zero.  */
	expr = build_int_2 (-1, -1);
      else
	expr = build_int_2 (0, 0);
      TREE_TYPE (expr) = type;
      /* Fix up the representation of -1 if appropriate.  */
      force_fit_type (expr, 0);
      return expr;
    }
  else if (TYPE_PTR_TO_MEMBER_P (type) && INTEGRAL_CODE_P (form))
    {
      error ("invalid conversion from '%T' to '%T'", intype, type);
      return error_mark_node;
    }

  if (INTEGRAL_CODE_P (form))
    {
      if (TYPE_PRECISION (intype) == POINTER_SIZE)
	return build1 (CONVERT_EXPR, type, expr);
      expr = cp_convert (c_common_type_for_size (POINTER_SIZE, 0), expr);
      /* Modes may be different but sizes should be the same.  */
      if (GET_MODE_SIZE (TYPE_MODE (TREE_TYPE (expr)))
	  != GET_MODE_SIZE (TYPE_MODE (type)))
	/* There is supposed to be some integral type
	   that is the same width as a pointer.  */
	abort ();
      return convert_to_pointer (type, expr);
    }

  if (type_unknown_p (expr))
    return instantiate_type (type, expr, tf_error | tf_warning);

  error ("cannot convert `%E' from type `%T' to type `%T'",
	    expr, intype, type);
  return error_mark_node;
}

/* Like convert, except permit conversions to take place which
   are not normally allowed due to access restrictions
   (such as conversion from sub-type to private super-type).  */

static tree
convert_to_pointer_force (tree type, tree expr)
{
  tree intype = TREE_TYPE (expr);
  enum tree_code form = TREE_CODE (intype);
  
  if (form == POINTER_TYPE)
    {
      intype = TYPE_MAIN_VARIANT (intype);

      if (TYPE_MAIN_VARIANT (type) != intype
	  && TREE_CODE (TREE_TYPE (type)) == RECORD_TYPE
	  && IS_AGGR_TYPE (TREE_TYPE (type))
	  && IS_AGGR_TYPE (TREE_TYPE (intype))
	  && TREE_CODE (TREE_TYPE (intype)) == RECORD_TYPE)
	{
	  enum tree_code code = PLUS_EXPR;
	  tree binfo;

	  binfo = lookup_base (TREE_TYPE (intype), TREE_TYPE (type),
			       ba_ignore, NULL);
	  if (!binfo)
	    {
	      binfo = lookup_base (TREE_TYPE (type), TREE_TYPE (intype),
				   ba_ignore, NULL);
	      code = MINUS_EXPR;
	    }
	  if (binfo == error_mark_node)
	    return error_mark_node;
	  if (binfo)
	    {
	      expr = build_base_path (code, expr, binfo, 0);
              if (expr == error_mark_node)
                 return error_mark_node;
	      /* Add any qualifier conversions.  */
	      if (!same_type_p (TREE_TYPE (TREE_TYPE (expr)),
				TREE_TYPE (type)))
		expr = build_nop (type, expr);
	      return expr;
	    }
	}
    }

  return cp_convert_to_pointer (type, expr, true);
}

/* We are passing something to a function which requires a reference.
   The type we are interested in is in TYPE. The initial
   value we have to begin with is in ARG.

   FLAGS controls how we manage access checking.
   DIRECT_BIND in FLAGS controls how any temporaries are generated.
     If DIRECT_BIND is set, DECL is the reference we're binding to.  */

static tree
build_up_reference (tree type, tree arg, int flags, tree decl)
{
  tree rval;
  tree argtype = TREE_TYPE (arg);
  tree target_type = TREE_TYPE (type);

  my_friendly_assert (TREE_CODE (type) == REFERENCE_TYPE, 187);

  if ((flags & DIRECT_BIND) && ! real_lvalue_p (arg))
    {
      /* Create a new temporary variable.  We can't just use a TARGET_EXPR
	 here because it needs to live as long as DECL.  */
      tree targ = arg;

      arg = make_temporary_var_for_ref_to_temp (decl, TREE_TYPE (arg));

      /* Process the initializer for the declaration.  */
      DECL_INITIAL (arg) = targ;
      cp_finish_decl (arg, targ, NULL_TREE, 
		      LOOKUP_ONLYCONVERTING|DIRECT_BIND);
    }
  else if (!(flags & DIRECT_BIND) && ! lvalue_p (arg))
    return get_target_expr (arg);

  /* If we had a way to wrap this up, and say, if we ever needed its
     address, transform all occurrences of the register, into a memory
     reference we could win better.  */
  rval = build_unary_op (ADDR_EXPR, arg, 1);
  if (rval == error_mark_node)
    return error_mark_node;

  if ((flags & LOOKUP_PROTECT)
      && TYPE_MAIN_VARIANT (argtype) != TYPE_MAIN_VARIANT (target_type)
      && IS_AGGR_TYPE (argtype)
      && IS_AGGR_TYPE (target_type))
    {
      /* We go through lookup_base for the access control.  */
      tree binfo = lookup_base (argtype, target_type, ba_check, NULL);
      if (binfo == error_mark_node)
	return error_mark_node;
      if (binfo == NULL_TREE)
	return error_not_base_type (target_type, argtype);
      rval = build_base_path (PLUS_EXPR, rval, binfo, 1);
    }
  else
    rval
      = convert_to_pointer_force (build_pointer_type (target_type), rval);
  return build_nop (type, rval);
}

/* Subroutine of convert_to_reference. REFTYPE is the target reference type.
   INTYPE is the original rvalue type and DECL is an optional _DECL node
   for diagnostics.
   
   [dcl.init.ref] says that if an rvalue is used to
   initialize a reference, then the reference must be to a
   non-volatile const type.  */

static void
warn_ref_binding (tree reftype, tree intype, tree decl)
{
  tree ttl = TREE_TYPE (reftype);
  
  if (!CP_TYPE_CONST_NON_VOLATILE_P (ttl))
    {
      const char *msg;

      if (CP_TYPE_VOLATILE_P (ttl) && decl)
	  msg = "initialization of volatile reference type `%#T' from rvalue of type `%T'";
      else if (CP_TYPE_VOLATILE_P (ttl))
	  msg = "conversion to volatile reference type `%#T' from rvalue of type `%T'";
      else if (decl)
	  msg = "initialization of non-const reference type `%#T' from rvalue of type `%T'";
      else
	  msg = "conversion to non-const reference type `%#T' from rvalue of type `%T'";

      pedwarn (msg, reftype, intype);
    }
}

/* For C++: Only need to do one-level references, but cannot
   get tripped up on signed/unsigned differences.

   DECL is either NULL_TREE or the _DECL node for a reference that is being
   initialized.  It can be error_mark_node if we don't know the _DECL but
   we know it's an initialization.  */

tree
convert_to_reference (tree reftype, tree expr, int convtype,
                      int flags, tree decl)
{
  tree type = TYPE_MAIN_VARIANT (TREE_TYPE (reftype));
  tree intype;
  tree rval = NULL_TREE;
  tree rval_as_conversion = NULL_TREE;
  bool can_convert_intype_to_type;

  if (TREE_CODE (type) == FUNCTION_TYPE 
      && TREE_TYPE (expr) == unknown_type_node)
    expr = instantiate_type (type, expr, 
			     (flags & LOOKUP_COMPLAIN)
			     ? tf_error | tf_warning : tf_none);
  else
    expr = convert_from_reference (expr);

  if (expr == error_mark_node)
    return error_mark_node;

  intype = TREE_TYPE (expr);

  my_friendly_assert (TREE_CODE (intype) != REFERENCE_TYPE, 364);

  intype = TYPE_MAIN_VARIANT (intype);

  can_convert_intype_to_type = can_convert (type, intype);
  if (!can_convert_intype_to_type
      && (convtype & CONV_IMPLICIT) && IS_AGGR_TYPE (intype)
      && ! (flags & LOOKUP_NO_CONVERSION))
    {
      /* Look for a user-defined conversion to lvalue that we can use.  */

      rval_as_conversion
	= build_type_conversion (reftype, expr);

      if (rval_as_conversion && rval_as_conversion != error_mark_node
	  && real_lvalue_p (rval_as_conversion))
	{
	  expr = rval_as_conversion;
	  rval_as_conversion = NULL_TREE;
	  intype = type;
	  can_convert_intype_to_type = 1;
	}
    }

  if (((convtype & CONV_STATIC) && can_convert (intype, type))
      || ((convtype & CONV_IMPLICIT) && can_convert_intype_to_type))
    {
      if (flags & LOOKUP_COMPLAIN)
	{
	  tree ttl = TREE_TYPE (reftype);
	  tree ttr = lvalue_type (expr);

	  if (! real_lvalue_p (expr))
	    warn_ref_binding (reftype, intype, decl);
	  
	  if (! (convtype & CONV_CONST)
		   && !at_least_as_qualified_p (ttl, ttr))
	    pedwarn ("conversion from `%T' to `%T' discards qualifiers",
			ttr, reftype);
	}

      return build_up_reference (reftype, expr, flags, decl);
    }
  else if ((convtype & CONV_REINTERPRET) && lvalue_p (expr))
    {
      /* When casting an lvalue to a reference type, just convert into
	 a pointer to the new type and deference it.  This is allowed
	 by San Diego WP section 5.2.9 paragraph 12, though perhaps it
	 should be done directly (jason).  (int &)ri ---> *(int*)&ri */

      /* B* bp; A& ar = (A&)bp; is valid, but it's probably not what they
         meant.  */
      if (TREE_CODE (intype) == POINTER_TYPE
	  && (comptypes (TREE_TYPE (intype), type,
			 COMPARE_BASE | COMPARE_DERIVED)))
	warning ("casting `%T' to `%T' does not dereference pointer",
		 intype, reftype);
	  
      rval = build_unary_op (ADDR_EXPR, expr, 0);
      if (rval != error_mark_node)
	rval = convert_force (build_pointer_type (TREE_TYPE (reftype)),
			      rval, 0);
      if (rval != error_mark_node)
	rval = build1 (NOP_EXPR, reftype, rval);
    }
  else
    {
      rval = convert_for_initialization (NULL_TREE, type, expr, flags,
					 "converting", 0, 0);
      if (rval == NULL_TREE || rval == error_mark_node)
	return rval;
      warn_ref_binding (reftype, intype, decl);
      rval = build_up_reference (reftype, rval, flags, decl);
    }

  if (rval)
    {
      /* If we found a way to convert earlier, then use it.  */
      return rval;
    }

  if (flags & LOOKUP_COMPLAIN)
    error ("cannot convert type `%T' to type `%T'", intype, reftype);

  if (flags & LOOKUP_SPECULATIVELY)
    return NULL_TREE;

  return error_mark_node;
}

/* We are using a reference VAL for its value. Bash that reference all the
   way down to its lowest form.  */

tree
convert_from_reference (tree val)
{
  if (TREE_CODE (TREE_TYPE (val)) == REFERENCE_TYPE)
    return build_indirect_ref (val, NULL);
  return val;
}

/* Implicitly convert the lvalue EXPR to another lvalue of type TOTYPE,
   preserving cv-qualification.  */

tree
convert_lvalue (tree totype, tree expr)
{
  totype = cp_build_qualified_type (totype, TYPE_QUALS (TREE_TYPE (expr)));
  totype = build_reference_type (totype);
  expr = convert_to_reference (totype, expr, CONV_IMPLICIT, LOOKUP_NORMAL,
			       NULL_TREE);
  return convert_from_reference (expr);
}

/* Really perform an lvalue-to-rvalue conversion, including copying an
   argument of class type into a temporary.  */

tree
force_rvalue (tree expr)
{
  if (IS_AGGR_TYPE (TREE_TYPE (expr)) && TREE_CODE (expr) != TARGET_EXPR)
    expr = ocp_convert (TREE_TYPE (expr), expr,
			CONV_IMPLICIT|CONV_FORCE_TEMP, LOOKUP_NORMAL);
  else
    expr = decay_conversion (expr);

  return expr;
}

/* C++ conversions, preference to static cast conversions.  */

tree
cp_convert (tree type, tree expr)
{
  return ocp_convert (type, expr, CONV_OLD_CONVERT, LOOKUP_NORMAL);
}

/* Conversion...

   FLAGS indicates how we should behave.  */

tree
ocp_convert (tree type, tree expr, int convtype, int flags)
{
  tree e = expr;
  enum tree_code code = TREE_CODE (type);

  if (error_operand_p (e) || type == error_mark_node)
    return error_mark_node;

  complete_type (type);
  complete_type (TREE_TYPE (expr));

  e = decl_constant_value (e);

  if (IS_AGGR_TYPE (type) && (convtype & CONV_FORCE_TEMP)
      /* Some internal structures (vtable_entry_type, sigtbl_ptr_type)
	 don't go through finish_struct, so they don't have the synthesized
	 constructors.  So don't force a temporary.  */
      && TYPE_HAS_CONSTRUCTOR (type))
    /* We need a new temporary; don't take this shortcut.  */;
  else if (TYPE_MAIN_VARIANT (type) == TYPE_MAIN_VARIANT (TREE_TYPE (e)))
    {
      if (same_type_p (type, TREE_TYPE (e)))
	/* The call to fold will not always remove the NOP_EXPR as
	   might be expected, since if one of the types is a typedef;
	   the comparison in fold is just equality of pointers, not a
	   call to comptypes.  We don't call fold in this case because
	   that can result in infinite recursion; fold will call
	   convert, which will call ocp_convert, etc.  */
	return e;
      /* For complex data types, we need to perform componentwise
         conversion.  */
      else if (TREE_CODE (type) == COMPLEX_TYPE)
        return fold (convert_to_complex (type, e));
      else if (TREE_CODE (e) == TARGET_EXPR)
	{
	  /* Don't build a NOP_EXPR of class type.  Instead, change the
	     type of the temporary.  Only allow this for cv-qual changes,
	     though.  */
	  if (!same_type_p (TYPE_MAIN_VARIANT (TREE_TYPE (e)),
			    TYPE_MAIN_VARIANT (type)))
	    abort ();
	  TREE_TYPE (e) = TREE_TYPE (TARGET_EXPR_SLOT (e)) = type;
	  return e;
	}
      else if (TREE_ADDRESSABLE (type))
	/* We shouldn't be treating objects of ADDRESSABLE type as rvalues.  */
	abort ();
      else
	return fold (build1 (NOP_EXPR, type, e));
    }

  if (code == VOID_TYPE && (convtype & CONV_STATIC))
    {
      e = convert_to_void (e, /*implicit=*/NULL);
      return e;
    }

  if (INTEGRAL_CODE_P (code))
    {
      tree intype = TREE_TYPE (e);
      /* enum = enum, enum = int, enum = float, (enum)pointer are all
         errors.  */
      if (TREE_CODE (type) == ENUMERAL_TYPE
	  && ((ARITHMETIC_TYPE_P (intype) && ! (convtype & CONV_STATIC))
	      || (TREE_CODE (intype) == POINTER_TYPE)))
	{
	  pedwarn ("conversion from `%#T' to `%#T'", intype, type);

	  if (flag_pedantic_errors)
	    return error_mark_node;
	}
      if (IS_AGGR_TYPE (intype))
	{
	  tree rval;
	  rval = build_type_conversion (type, e);
	  if (rval)
	    return rval;
	  if (flags & LOOKUP_COMPLAIN)
	    error ("`%#T' used where a `%T' was expected", intype, type);
	  if (flags & LOOKUP_SPECULATIVELY)
	    return NULL_TREE;
	  return error_mark_node;
	}
      if (code == BOOLEAN_TYPE)
	return cp_truthvalue_conversion (e);

      return fold (convert_to_integer (type, e));
    }
  if (POINTER_TYPE_P (type) || TYPE_PTR_TO_MEMBER_P (type))
    return fold (cp_convert_to_pointer (type, e, false));
  if (code == VECTOR_TYPE)
    return fold (convert_to_vector (type, e));
  if (code == REAL_TYPE || code == COMPLEX_TYPE)
    {
      if (IS_AGGR_TYPE (TREE_TYPE (e)))
	{
	  tree rval;
	  rval = build_type_conversion (type, e);
	  if (rval)
	    return rval;
	  else
	    if (flags & LOOKUP_COMPLAIN)
	      error ("`%#T' used where a floating point value was expected",
			TREE_TYPE (e));
	}
      if (code == REAL_TYPE)
	return fold (convert_to_real (type, e));
      else if (code == COMPLEX_TYPE)
	return fold (convert_to_complex (type, e));
    }

  /* New C++ semantics:  since assignment is now based on
     memberwise copying,  if the rhs type is derived from the
     lhs type, then we may still do a conversion.  */
  if (IS_AGGR_TYPE_CODE (code))
    {
      tree dtype = TREE_TYPE (e);
      tree ctor = NULL_TREE;

      dtype = TYPE_MAIN_VARIANT (dtype);

      /* Conversion between aggregate types.  New C++ semantics allow
	 objects of derived type to be cast to objects of base type.
	 Old semantics only allowed this between pointers.

	 There may be some ambiguity between using a constructor
	 vs. using a type conversion operator when both apply.  */

      ctor = e;

      if (abstract_virtuals_error (NULL_TREE, type))
	return error_mark_node;

      if ((flags & LOOKUP_ONLYCONVERTING)
	  && ! (IS_AGGR_TYPE (dtype) && DERIVED_FROM_P (type, dtype)))
	/* For copy-initialization, first we create a temp of the proper type
	   with a user-defined conversion sequence, then we direct-initialize
	   the target with the temp (see [dcl.init]).  */
	ctor = build_user_type_conversion (type, ctor, flags);
      else
	ctor = build_special_member_call (NULL_TREE, 
					  complete_ctor_identifier,
					  build_tree_list (NULL_TREE, ctor),
					  TYPE_BINFO (type), flags);
      if (ctor)
	return build_cplus_new (type, ctor);
    }

  if (flags & LOOKUP_COMPLAIN)
    error ("conversion from `%T' to non-scalar type `%T' requested",
	      TREE_TYPE (expr), type);
  if (flags & LOOKUP_SPECULATIVELY)
    return NULL_TREE;
  return error_mark_node;
}

/* When an expression is used in a void context, its value is discarded and
   no lvalue-rvalue and similar conversions happen [expr.static.cast/4,
   stmt.expr/1, expr.comma/1].  This permits dereferencing an incomplete type
   in a void context. The C++ standard does not define what an `access' to an
   object is, but there is reason to believe that it is the lvalue to rvalue
   conversion -- if it were not, `*&*p = 1' would violate [expr]/4 in that it
   accesses `*p' not to calculate the value to be stored. But, dcl.type.cv/8
   indicates that volatile semantics should be the same between C and C++
   where ever possible. C leaves it implementation defined as to what
   constitutes an access to a volatile. So, we interpret `*vp' as a read of
   the volatile object `vp' points to, unless that is an incomplete type. For
   volatile references we do not do this interpretation, because that would
   make it impossible to ignore the reference return value from functions. We
   issue warnings in the confusing cases.
   
   IMPLICIT is tells us the context of an implicit void conversion.  */

tree
convert_to_void (tree expr, const char *implicit)
{
  if (expr == error_mark_node 
      || TREE_TYPE (expr) == error_mark_node)
    return error_mark_node;
  if (!TREE_TYPE (expr))
    return expr;
  if (invalid_nonstatic_memfn_p (expr))
    return error_mark_node;
  if (TREE_CODE (expr) == PSEUDO_DTOR_EXPR)
    {
      error ("pseudo-destructor is not called");
      return error_mark_node;
    }
  if (VOID_TYPE_P (TREE_TYPE (expr)))
    return expr;
  switch (TREE_CODE (expr))
    {
    case COND_EXPR:
      {
        /* The two parts of a cond expr might be separate lvalues.  */
        tree op1 = TREE_OPERAND (expr,1);
        tree op2 = TREE_OPERAND (expr,2);
        tree new_op1 = convert_to_void
	  (op1, (implicit && !TREE_SIDE_EFFECTS (op2)
		 ? "second operand of conditional" : NULL));
        tree new_op2 = convert_to_void
	  (op2, (implicit && !TREE_SIDE_EFFECTS (op1)
		 ? "third operand of conditional" : NULL));
        
	expr = build (COND_EXPR, TREE_TYPE (new_op1),
		      TREE_OPERAND (expr, 0), new_op1, new_op2);
        break;
      }
    
    case COMPOUND_EXPR:
      {
        /* The second part of a compound expr contains the value.  */
        tree op1 = TREE_OPERAND (expr,1);
        tree new_op1 = convert_to_void
	  (op1, (implicit && !TREE_NO_UNUSED_WARNING (expr)
		 ? "right-hand operand of comma" : NULL));
        
        if (new_op1 != op1)
	  {
	    tree t = build (COMPOUND_EXPR, TREE_TYPE (new_op1),
			    TREE_OPERAND (expr, 0), new_op1);
	    expr = t;
	  }

        break;
      }
    
    case NON_LVALUE_EXPR:
    case NOP_EXPR:
      /* These have already decayed to rvalue.  */
      break;
    
    case CALL_EXPR:   /* We have a special meaning for volatile void fn().  */
      break;
    
    case INDIRECT_REF:
      {
        tree type = TREE_TYPE (expr);
        int is_reference = TREE_CODE (TREE_TYPE (TREE_OPERAND (expr, 0)))
                           == REFERENCE_TYPE;
        int is_volatile = TYPE_VOLATILE (type);
        int is_complete = COMPLETE_TYPE_P (complete_type (type));
        
        if (is_volatile && !is_complete)
          warning ("object of incomplete type `%T' will not be accessed in %s",
                      type, implicit ? implicit : "void context");
        else if (is_reference && is_volatile)
          warning ("object of type `%T' will not be accessed in %s",
                      TREE_TYPE (TREE_OPERAND (expr, 0)),
                      implicit ? implicit : "void context");
        if (is_reference || !is_volatile || !is_complete)
          expr = TREE_OPERAND (expr, 0);
      
        break;
      }
    
    case VAR_DECL:
      {
        /* External variables might be incomplete.  */
        tree type = TREE_TYPE (expr);
        int is_complete = COMPLETE_TYPE_P (complete_type (type));
        
        if (TYPE_VOLATILE (type) && !is_complete)
          warning ("object `%E' of incomplete type `%T' will not be accessed in %s",
                      expr, type, implicit ? implicit : "void context");
        break;
      }

    default:;
    }
  {
    tree probe = expr;
  
    if (TREE_CODE (probe) == ADDR_EXPR)
      probe = TREE_OPERAND (expr, 0);
    if (type_unknown_p (probe))
      {
	/* [over.over] enumerates the places where we can take the address
	   of an overloaded function, and this is not one of them.  */
	pedwarn ("%s cannot resolve address of overloaded function",
		    implicit ? implicit : "void cast");
	expr = void_zero_node;
      }
    else if (implicit && probe == expr && is_overloaded_fn (probe))
      /* Only warn when there is no &.  */
      warning ("%s is a reference, not call, to function `%E'",
		  implicit, expr);
  }
  
  if (expr != error_mark_node && !VOID_TYPE_P (TREE_TYPE (expr)))
    {
      if (implicit && !TREE_SIDE_EFFECTS (expr) && warn_unused_value)
	warning ("%s has no effect", implicit);
      expr = build1 (CONVERT_EXPR, void_type_node, expr);
    }
  return expr;
}

/* Create an expression whose value is that of EXPR,
   converted to type TYPE.  The TREE_TYPE of the value
   is always TYPE.  This function implements all reasonable
   conversions; callers should filter out those that are
   not permitted by the language being compiled.

   Most of this routine is from build_reinterpret_cast.

   The backend cannot call cp_convert (what was convert) because
   conversions to/from basetypes may involve memory references
   (vbases) and adding or subtracting small values (multiple
   inheritance), but it calls convert from the constant folding code
   on subtrees of already built trees after it has ripped them apart.

   Also, if we ever support range variables, we'll probably also have to
   do a little bit more work.  */

tree
convert (tree type, tree expr)
{
  tree intype;

  if (type == error_mark_node || expr == error_mark_node)
    return error_mark_node;

  intype = TREE_TYPE (expr);

  if (POINTER_TYPE_P (type) && POINTER_TYPE_P (intype))
    {
      expr = decl_constant_value (expr);
      return fold (build1 (NOP_EXPR, type, expr));
    }

  return ocp_convert (type, expr, CONV_OLD_CONVERT,
		      LOOKUP_NORMAL|LOOKUP_NO_CONVERSION);
}

/* Like cp_convert, except permit conversions to take place which
   are not normally allowed due to access restrictions
   (such as conversion from sub-type to private super-type).  */

tree
convert_force (tree type, tree expr, int convtype)
{
  tree e = expr;
  enum tree_code code = TREE_CODE (type);

  if (code == REFERENCE_TYPE)
    return fold (convert_to_reference (type, e, CONV_C_CAST, LOOKUP_COMPLAIN,
				       NULL_TREE));
  else if (TREE_CODE (TREE_TYPE (e)) == REFERENCE_TYPE)
    e = convert_from_reference (e);

  if (code == POINTER_TYPE)
    return fold (convert_to_pointer_force (type, e));

  /* From typeck.c convert_for_assignment */
  if (((TREE_CODE (TREE_TYPE (e)) == POINTER_TYPE && TREE_CODE (e) == ADDR_EXPR
	&& TREE_CODE (TREE_TYPE (e)) == POINTER_TYPE
	&& TREE_CODE (TREE_TYPE (TREE_TYPE (e))) == METHOD_TYPE)
       || integer_zerop (e)
       || TYPE_PTRMEMFUNC_P (TREE_TYPE (e)))
      && TYPE_PTRMEMFUNC_P (type))
    {
      /* compatible pointer to member functions.  */
      return build_ptrmemfunc (TYPE_PTRMEMFUNC_FN_TYPE (type), e, 1);
    }

  return ocp_convert (type, e, CONV_C_CAST|convtype, LOOKUP_NORMAL);
}

/* Convert an aggregate EXPR to type XTYPE.  If a conversion
   exists, return the attempted conversion.  This may
   return ERROR_MARK_NODE if the conversion is not
   allowed (references private members, etc).
   If no conversion exists, NULL_TREE is returned.

   FIXME: Ambiguity checking is wrong.  Should choose one by the implicit
   object parameter, or by the second standard conversion sequence if
   that doesn't do it.  This will probably wait for an overloading rewrite.
   (jason 8/9/95)  */

tree
build_type_conversion (tree xtype, tree expr)
{
  /* C++: check to see if we can convert this aggregate type
     into the required type.  */
  return build_user_type_conversion (xtype, expr, LOOKUP_NORMAL);
}

/* Convert the given EXPR to one of a group of types suitable for use in an
   expression.  DESIRES is a combination of various WANT_* flags (q.v.)
   which indicates which types are suitable.  If COMPLAIN is true, complain
   about ambiguity; otherwise, the caller will deal with it.  */

tree
build_expr_type_conversion (int desires, tree expr, bool complain)
{
  tree basetype = TREE_TYPE (expr);
  tree conv = NULL_TREE;
  tree winner = NULL_TREE;

  if (expr == null_node 
      && (desires & WANT_INT) 
      && !(desires & WANT_NULL))
    warning ("converting NULL to non-pointer type");
    
  expr = convert_from_reference (expr);
  basetype = TREE_TYPE (expr);

  if (basetype == error_mark_node)
    return error_mark_node;

  if (! IS_AGGR_TYPE (basetype))
    switch (TREE_CODE (basetype))
      {
      case INTEGER_TYPE:
	if ((desires & WANT_NULL) && null_ptr_cst_p (expr))
	  return expr;
	/* else fall through...  */

      case VECTOR_TYPE:
      case BOOLEAN_TYPE:
	return (desires & WANT_INT) ? expr : NULL_TREE;
      case ENUMERAL_TYPE:
	return (desires & WANT_ENUM) ? expr : NULL_TREE;
      case REAL_TYPE:
	return (desires & WANT_FLOAT) ? expr : NULL_TREE;
      case POINTER_TYPE:
	return (desires & WANT_POINTER) ? expr : NULL_TREE;
	
      case FUNCTION_TYPE:
      case ARRAY_TYPE:
	return (desires & WANT_POINTER) ? decay_conversion (expr)
     	                                : NULL_TREE;
      default:
	return NULL_TREE;
      }

  /* The code for conversions from class type is currently only used for
     delete expressions.  Other expressions are handled by build_new_op.  */
  if (!complete_type_or_else (basetype, expr))
    return error_mark_node;
  if (!TYPE_HAS_CONVERSION (basetype))
    return NULL_TREE;

  for (conv = lookup_conversions (basetype); conv; conv = TREE_CHAIN (conv))
    {
      int win = 0;
      tree candidate;
      tree cand = TREE_VALUE (conv);

      if (winner && winner == cand)
	continue;

      candidate = non_reference (TREE_TYPE (TREE_TYPE (cand)));

      switch (TREE_CODE (candidate))
	{
	case BOOLEAN_TYPE:
	case INTEGER_TYPE:
	  win = (desires & WANT_INT); break;
	case ENUMERAL_TYPE:
	  win = (desires & WANT_ENUM); break;
	case REAL_TYPE:
	  win = (desires & WANT_FLOAT); break;
	case POINTER_TYPE:
	  win = (desires & WANT_POINTER); break;

	default:
	  break;
	}

      if (win)
	{
	  if (winner)
	    {
	      if (complain)
		{
		  error ("ambiguous default type conversion from `%T'",
			    basetype);
		  error ("  candidate conversions include `%D' and `%D'",
			    winner, cand);
		}
	      return error_mark_node;
	    }
	  else
	    winner = cand;
	}
    }

  if (winner)
    {
      tree type = non_reference (TREE_TYPE (TREE_TYPE (winner)));
      return build_user_type_conversion (type, expr, LOOKUP_NORMAL);
    }

  return NULL_TREE;
}

/* Implements integral promotion (4.1) and float->double promotion.  */

tree
type_promotes_to (tree type)
{
  if (type == error_mark_node)
    return error_mark_node;

  type = TYPE_MAIN_VARIANT (type);

  /* bool always promotes to int (not unsigned), even if it's the same
     size.  */
  if (type == boolean_type_node)
    type = integer_type_node;

  /* Normally convert enums to int, but convert wide enums to something
     wider.  */
  else if (TREE_CODE (type) == ENUMERAL_TYPE
	   || type == wchar_type_node)
    {
      int precision = MAX (TYPE_PRECISION (type),
			   TYPE_PRECISION (integer_type_node));
      tree totype = c_common_type_for_size (precision, 0);
      if (TREE_UNSIGNED (type)
	  && ! int_fits_type_p (TYPE_MAX_VALUE (type), totype))
	type = c_common_type_for_size (precision, 1);
      else
	type = totype;
    }
  else if (c_promoting_integer_type_p (type))
    {
      /* Retain unsignedness if really not getting bigger.  */
      if (TREE_UNSIGNED (type)
	  && TYPE_PRECISION (type) == TYPE_PRECISION (integer_type_node))
	type = unsigned_type_node;
      else
	type = integer_type_node;
    }
  else if (type == float_type_node)
    type = double_type_node;
    
  return type;
}

/* The routines below this point are carefully written to conform to
   the standard.  They use the same terminology, and follow the rules
   closely.  Although they are used only in pt.c at the moment, they
   should presumably be used everywhere in the future.  */

/* Attempt to perform qualification conversions on EXPR to convert it
   to TYPE.  Return the resulting expression, or error_mark_node if
   the conversion was impossible.  */

tree 
perform_qualification_conversions (tree type, tree expr)
{
  tree expr_type;

  expr_type = TREE_TYPE (expr);

  if (TYPE_PTR_P (type) && TYPE_PTR_P (expr_type)
      && comp_ptr_ttypes (TREE_TYPE (type), TREE_TYPE (expr_type)))
    return build_nop (type, expr);
  else if (TYPE_PTR_TO_MEMBER_P (type)
	   && TYPE_PTR_TO_MEMBER_P (expr_type)
	   && same_type_p (TYPE_PTRMEM_CLASS_TYPE (type),
			   TYPE_PTRMEM_CLASS_TYPE (expr_type))
	   && comp_ptr_ttypes (TYPE_PTRMEM_POINTED_TO_TYPE (type),
			       TYPE_PTRMEM_POINTED_TO_TYPE (expr_type)))
    return build_nop (type, expr);
  else
    return error_mark_node;
}
