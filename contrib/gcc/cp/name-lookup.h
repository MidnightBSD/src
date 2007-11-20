/* Declarations for C++ name lookup routines.
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.
   Contributed by Gabriel Dos Reis <gdr@integrable-solutions.net>

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

#ifndef GCC_CP_NAME_LOOKUP_H
#define GCC_CP_NAME_LOOKUP_H

#include "c-common.h"

/* The type of dictionary used to map names to types declared at
   a given scope.  */
typedef struct binding_table_s *binding_table;
typedef struct binding_entry_s *binding_entry;

/* The type of a routine repeatedly called by binding_table_foreach.  */
typedef void (*bt_foreach_proc) (binding_entry, void *);

struct binding_entry_s GTY(())
{
  binding_entry chain;
  tree name;
  tree type;
};

/* These macros indicate the initial chains count for binding_table.  */
#define SCOPE_DEFAULT_HT_SIZE                        (1 << 3)
#define CLASS_SCOPE_HT_SIZE                          (1 << 3)
#define NAMESPACE_ORDINARY_HT_SIZE                   (1 << 5)
#define NAMESPACE_STD_HT_SIZE                        (1 << 8)
#define GLOBAL_SCOPE_HT_SIZE                         (1 << 8)

extern void binding_table_remove_anonymous_types (binding_table);
extern void binding_table_foreach (binding_table, bt_foreach_proc, void *);
extern binding_entry binding_table_find (binding_table, tree);
extern void cxx_remember_type_decls (binding_table);

/* Datatype used to temporarily save C++ bindings (for implicit
   instantiations purposes and like).  Implemented in decl.c.  */
typedef struct cxx_saved_binding cxx_saved_binding;

/* Datatype that represents binding established by a declaration between
   a name and a C++ entity.  */
typedef struct cxx_binding cxx_binding;

/* The datatype used to implement C++ scope.  */
typedef struct cp_binding_level cxx_scope;

/* Nonzero if this binding is for a local scope, as opposed to a class
   or namespace scope.  */
#define LOCAL_BINDING_P(NODE) ((NODE)->is_local)

/* True if NODE->value is from a base class of the class which is
   currently being defined.  */
#define INHERITED_VALUE_BINDING_P(NODE) ((NODE)->value_is_inherited)

struct cxx_binding GTY(())
{
  /* Link to chain together various bindings for this name.  */
  cxx_binding *previous;
  /* The non-type entity this name is bound to.  */
  tree value;
  /* The type entity this name is bound to.  */
  tree type;
  /* The scope at which this binding was made.  */
  cxx_scope *scope;
  unsigned value_is_inherited : 1;
  unsigned is_local : 1;
};

extern tree identifier_type_value (tree);
extern void set_identifier_type_value (tree, tree);
extern void pop_binding (tree, tree);
extern void clear_identifier_class_values (void);
extern tree constructor_name_full (tree);
extern tree constructor_name (tree);
extern bool constructor_name_p (tree, tree);

/* The kinds of scopes we recognize.  */
typedef enum scope_kind {
  sk_block = 0,      /* An ordinary block scope.  This enumerator must
			have the value zero because "cp_binding_level"
			is initialized by using "memset" to set the
			contents to zero, and the default scope kind
			is "sk_block".  */
  sk_cleanup,        /* A scope for (pseudo-)scope for cleanup.  It is
                        peusdo in that it is transparent to name lookup
                        activities.  */
  sk_try,	     /* A try-block.  */
  sk_catch,          /* A catch-block.  */
  sk_for,            /* The scope of the variable declared in a
			for-init-statement.  */
  sk_function_parms, /* The scope containing function parameters.  */
  sk_class,          /* The scope containing the members of a class.  */
  sk_namespace,      /* The scope containing the members of a
			namespace, including the global scope.  */
  sk_template_parms, /* A scope for template parameters.  */
  sk_template_spec   /* Like sk_template_parms, but for an explicit
			specialization.  Since, by definition, an
			explicit specialization is introduced by
			"template <>", this scope is always empty.  */
} scope_kind;

/* For each binding contour we allocate a binding_level structure
   which records the names defined in that contour.
   Contours include:
    0) the global one
    1) one for each function definition,
       where internal declarations of the parameters appear.
    2) one for each compound statement,
       to record its declarations.

   The current meaning of a name can be found by searching the levels
   from the current one out to the global one.

   Off to the side, may be the class_binding_level.  This exists only
   to catch class-local declarations.  It is otherwise nonexistent.

   Also there may be binding levels that catch cleanups that must be
   run when exceptions occur.  Thus, to see whether a name is bound in
   the current scope, it is not enough to look in the
   CURRENT_BINDING_LEVEL.  You should use lookup_name_current_level
   instead.  */

/* Note that the information in the `names' component of the global contour
   is duplicated in the IDENTIFIER_GLOBAL_VALUEs of all identifiers.  */

struct cp_binding_level GTY(())
  {
    /* A chain of _DECL nodes for all variables, constants, functions,
       and typedef types.  These are in the reverse of the order
       supplied.  There may be OVERLOADs on this list, too, but they
       are wrapped in TREE_LISTs; the TREE_VALUE is the OVERLOAD.  */
    tree names;

    /* Count of elements in names chain.  */
    size_t names_size;

    /* A chain of NAMESPACE_DECL nodes.  */
    tree namespaces;

    /* An array of static functions and variables (for namespaces only) */
    varray_type static_decls;

    /* A chain of VTABLE_DECL nodes.  */
    tree vtables; 

    /* A dictionary for looking up user-defined-types.  */
    binding_table type_decls;

    /* A list of USING_DECL nodes.  */
    tree usings;

    /* A list of used namespaces. PURPOSE is the namespace,
       VALUE the common ancestor with this binding_level's namespace.  */
    tree using_directives;

    /* If this binding level is the binding level for a class, then
       class_shadowed is a TREE_LIST.  The TREE_PURPOSE of each node
       is the name of an entity bound in the class.  The TREE_TYPE is
       the DECL bound by this name in the class.  */
    tree class_shadowed;

    /* Similar to class_shadowed, but for IDENTIFIER_TYPE_VALUE, and
       is used for all binding levels. In addition the TREE_VALUE is the
       IDENTIFIER_TYPE_VALUE before we entered the class.  */
    tree type_shadowed;

    /* A TREE_LIST.  Each TREE_VALUE is the LABEL_DECL for a local
       label in this scope.  The TREE_PURPOSE is the previous value of
       the IDENTIFIER_LABEL VALUE.  */
    tree shadowed_labels;

    /* For each level (except not the global one),
       a chain of BLOCK nodes for all the levels
       that were entered and exited one level down.  */
    tree blocks;

    /* The entity (namespace, class, function) the scope of which this
       binding contour corresponds to.  Otherwise NULL.  */
    tree this_entity;

    /* The binding level which this one is contained in (inherits from).  */
    struct cp_binding_level *level_chain;

    /* List of VAR_DECLS saved from a previous for statement.
       These would be dead in ISO-conforming code, but might
       be referenced in ARM-era code.  These are stored in a
       TREE_LIST; the TREE_VALUE is the actual declaration.  */
    tree dead_vars_from_for;

    /* Binding depth at which this level began.  */
    int binding_depth;

    /* The kind of scope that this object represents.  However, a
       SK_TEMPLATE_SPEC scope is represented with KIND set to
       SK_TEMPALTE_PARMS and EXPLICIT_SPEC_P set to true.  */
    ENUM_BITFIELD (scope_kind) kind : 4;

    /* True if this scope is an SK_TEMPLATE_SPEC scope.  This field is
       only valid if KIND == SK_TEMPLATE_PARMS.  */
    BOOL_BITFIELD explicit_spec_p : 1;

    /* true means make a BLOCK for this level regardless of all else.  */
    unsigned keep : 1;

    /* Nonzero if this level can safely have additional
       cleanup-needing variables added to it.  */
    unsigned more_cleanups_ok : 1;
    unsigned have_cleanups : 1;

    /* 22 bits left to fill a 32-bit word.  */
  };

/* The binding level currently in effect.  */

#define current_binding_level			\
  (*(cfun && cp_function_chain->bindings	\
   ? &cp_function_chain->bindings		\
   : &scope_chain->bindings))

/* The binding level of the current class, if any.  */

#define class_binding_level scope_chain->class_bindings

/* The tree node representing the global scope.  */
extern GTY(()) tree global_namespace;
extern GTY(()) tree global_scope_name;

/* Indicates that there is a type value in some namespace, although
   that is not necessarily in scope at the moment.  */

extern GTY(()) tree global_type_node;

/* True if SCOPE designates the global scope binding contour.  */
#define global_scope_p(SCOPE) \
  ((SCOPE) == NAMESPACE_LEVEL (global_namespace))

extern cxx_scope *leave_scope (void);
extern bool kept_level_p (void);
extern int global_bindings_p (void);
extern bool toplevel_bindings_p	(void);
extern bool namespace_bindings_p (void);
extern bool template_parm_scope_p (void);
extern scope_kind innermost_scope_kind (void);
extern cxx_scope *begin_scope (scope_kind, tree);
extern void print_binding_stack	(void);
extern void print_binding_level	(cxx_scope *);
extern void push_to_top_level (void);
extern void pop_from_top_level (void);
extern void pop_everything (void);
extern void keep_next_level (bool);
extern bool is_ancestor (tree, tree);
extern bool push_scope (tree);
extern void pop_scope (tree);

extern void push_namespace (tree);
extern void pop_namespace (void);
extern void push_nested_namespace (tree);
extern void pop_nested_namespace (tree);
extern void pushlevel_class (void);
extern void poplevel_class (void);
extern tree pushdecl_with_scope (tree, cxx_scope *);
extern tree lookup_tag (enum tree_code, tree, cxx_scope *, int);
extern tree lookup_tag_reverse (tree, tree);
extern tree lookup_name	(tree, int);
extern tree lookup_name_real (tree, int, int, int, int);
extern tree namespace_binding (tree, tree);
extern void set_namespace_binding (tree, tree, tree);
extern tree lookup_namespace_name (tree, tree);
extern tree lookup_qualified_name (tree, tree, bool, bool);
extern tree lookup_name_nonclass (tree);
extern tree lookup_function_nonclass (tree, tree);
extern void push_local_binding (tree, tree, int);
extern int push_class_binding (tree, tree);
extern bool pushdecl_class_level (tree);
extern tree pushdecl_namespace_level (tree);
extern bool push_class_level_binding (tree, tree);
extern void storetags (tree);
extern tree getdecls (void);
extern tree cp_namespace_decls (tree);
extern void set_class_shadows (tree);
extern void set_decl_namespace (tree, tree, bool);
extern tree current_decl_namespace (void);
extern void push_decl_namespace (tree);
extern void pop_decl_namespace (void);
extern void do_namespace_alias (tree, tree);
extern void do_toplevel_using_decl (tree, tree, tree);
extern void do_local_using_decl (tree, tree, tree);
extern tree do_class_using_decl (tree);
extern void do_using_directive (tree);
extern tree lookup_arg_dependent (tree, tree, tree);
extern bool is_associated_namespace (tree, tree);
extern void parse_using_directive (tree, tree);


/* Set *DECL to the (non-hidden) declaration for ID at global scope,
   if present and return true; otherwise return false.  */

static inline bool
get_global_value_if_present (tree id, tree *decl)
{
  tree global_value = namespace_binding (id, global_namespace);

  if (global_value)
    *decl = global_value;
  return global_value != NULL;
}

/* True is the binding of IDENTIFIER at global scope names a type.  */

static inline bool
is_typename_at_global_scope (tree id)
{
  tree global_value = namespace_binding (id, global_namespace);

  return global_value && TREE_CODE (global_value) == TYPE_DECL;
}

#endif /* GCC_CP_NAME_LOOKUP_H */
