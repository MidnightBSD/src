/* Hash tables.
   Copyright (C) 2000, 2001, 2003 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef GCC_HASHTABLE_H
#define GCC_HASHTABLE_H

#include "obstack.h"
#define GTY(x) /* nothing */

/* This is what each hash table entry points to.  It may be embedded
   deeply within another object.  */
typedef struct ht_identifier ht_identifier;
struct ht_identifier GTY(())
{
  const unsigned char *str;
  unsigned int len;
  unsigned int hash_value;
};

#define HT_LEN(NODE) ((NODE)->len)
#define HT_STR(NODE) ((NODE)->str)

typedef struct ht hash_table;
typedef struct ht_identifier *hashnode;

enum ht_lookup_option {HT_NO_INSERT = 0, HT_ALLOC, HT_ALLOCED};

/* An identifier hash table for cpplib and the front ends.  */
struct ht
{
  /* Identifiers are allocated from here.  */
  struct obstack stack;

  hashnode *entries;
  /* Call back.  */
  hashnode (*alloc_node) (hash_table *);

  unsigned int nslots;		/* Total slots in the entries array.  */
  unsigned int nelements;	/* Number of live elements.  */

  /* Link to reader, if any.  For the benefit of cpplib.  */
  struct cpp_reader *pfile;

  /* Table usage statistics.  */
  unsigned int searches;
  unsigned int collisions;
};

/* Initialize the hashtable with 2 ^ order entries.  */
extern hash_table *ht_create (unsigned int order);

/* Frees all memory associated with a hash table.  */
extern void ht_destroy (hash_table *);

extern hashnode ht_lookup (hash_table *, const unsigned char *,
			   size_t, enum ht_lookup_option);

/* For all nodes in TABLE, make a callback.  The callback takes
   TABLE->PFILE, the node, and a PTR, and the callback sequence stops
   if the callback returns zero.  */
typedef int (*ht_cb) (struct cpp_reader *, hashnode, const void *);
extern void ht_forall (hash_table *, ht_cb, const void *);

/* Dump allocation statistics to stderr.  */
extern void ht_dump_statistics (hash_table *);

#endif /* GCC_HASHTABLE_H */
