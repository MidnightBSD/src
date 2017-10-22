/* Functions to support general ended bitmaps.
   Copyright (C) 1997, 1998, 1999, 2000, 2001, 2003
   Free Software Foundation, Inc.

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

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "flags.h"
#include "obstack.h"
#include "ggc.h"
#include "bitmap.h"

/* Obstack to allocate bitmap elements from.  */
static struct obstack bitmap_obstack;
static int bitmap_obstack_init = FALSE;

#ifndef INLINE
#ifndef __GNUC__
#define INLINE
#else
#define INLINE __inline__
#endif
#endif

/* Global data */
bitmap_element bitmap_zero_bits;	/* An element of all zero bits.  */
static bitmap_element *bitmap_free;	/* Freelist of bitmap elements.  */
static GTY((deletable (""))) bitmap_element *bitmap_ggc_free;

static void bitmap_elem_to_freelist (bitmap, bitmap_element *);
static void bitmap_element_free (bitmap, bitmap_element *);
static bitmap_element *bitmap_element_allocate (bitmap);
static int bitmap_element_zerop (bitmap_element *);
static void bitmap_element_link (bitmap, bitmap_element *);
static bitmap_element *bitmap_find_bit (bitmap, unsigned int);

/* Add ELEM to the appropriate freelist.  */
static INLINE void
bitmap_elem_to_freelist (bitmap head, bitmap_element *elt)
{
  if (head->using_obstack)
    {
      elt->next = bitmap_free;
      bitmap_free = elt;
    }
  else
    {
      elt->next = bitmap_ggc_free;
      bitmap_ggc_free = elt;
    }
}

/* Free a bitmap element.  Since these are allocated off the
   bitmap_obstack, "free" actually means "put onto the freelist".  */

static INLINE void
bitmap_element_free (bitmap head, bitmap_element *elt)
{
  bitmap_element *next = elt->next;
  bitmap_element *prev = elt->prev;

  if (prev)
    prev->next = next;

  if (next)
    next->prev = prev;

  if (head->first == elt)
    head->first = next;

  /* Since the first thing we try is to insert before current,
     make current the next entry in preference to the previous.  */
  if (head->current == elt)
    {
      head->current = next != 0 ? next : prev;
      if (head->current)
	head->indx = head->current->indx;
    }
  bitmap_elem_to_freelist (head, elt);
}

/* Allocate a bitmap element.  The bits are cleared, but nothing else is.  */

static INLINE bitmap_element *
bitmap_element_allocate (bitmap head)
{
  bitmap_element *element;

  if (head->using_obstack)
    {
      if (bitmap_free != 0)
	{
	  element = bitmap_free;
	  bitmap_free = element->next;
	}
      else
	{
	  /* We can't use gcc_obstack_init to initialize the obstack since
	     print-rtl.c now calls bitmap functions, and bitmap is linked
	     into the gen* functions.  */
	  if (!bitmap_obstack_init)
	    {
	      bitmap_obstack_init = TRUE;

#if !defined(__GNUC__) || (__GNUC__ < 2)
#define __alignof__(type) 0
#endif

	      obstack_specify_allocation (&bitmap_obstack, OBSTACK_CHUNK_SIZE,
					  __alignof__ (bitmap_element),
					  obstack_chunk_alloc,
					  obstack_chunk_free);
	    }

	  element = obstack_alloc (&bitmap_obstack, sizeof (bitmap_element));
	}
    }
  else
    {
      if (bitmap_ggc_free != NULL)
	{
          element = bitmap_ggc_free;
          bitmap_ggc_free = element->next;
	}
      else
	element = ggc_alloc (sizeof (bitmap_element));
    }

  memset (element->bits, 0, sizeof (element->bits));

  return element;
}

/* Release any memory allocated by bitmaps.  */

void
bitmap_release_memory (void)
{
  bitmap_free = 0;
  if (bitmap_obstack_init)
    {
      bitmap_obstack_init = FALSE;
      obstack_free (&bitmap_obstack, NULL);
    }
}

/* Return nonzero if all bits in an element are zero.  */

static INLINE int
bitmap_element_zerop (bitmap_element *element)
{
#if BITMAP_ELEMENT_WORDS == 2
  return (element->bits[0] | element->bits[1]) == 0;
#else
  int i;

  for (i = 0; i < BITMAP_ELEMENT_WORDS; i++)
    if (element->bits[i] != 0)
      return 0;

  return 1;
#endif
}

/* Link the bitmap element into the current bitmap linked list.  */

static INLINE void
bitmap_element_link (bitmap head, bitmap_element *element)
{
  unsigned int indx = element->indx;
  bitmap_element *ptr;

  /* If this is the first and only element, set it in.  */
  if (head->first == 0)
    {
      element->next = element->prev = 0;
      head->first = element;
    }

  /* If this index is less than that of the current element, it goes someplace
     before the current element.  */
  else if (indx < head->indx)
    {
      for (ptr = head->current;
	   ptr->prev != 0 && ptr->prev->indx > indx;
	   ptr = ptr->prev)
	;

      if (ptr->prev)
	ptr->prev->next = element;
      else
	head->first = element;

      element->prev = ptr->prev;
      element->next = ptr;
      ptr->prev = element;
    }

  /* Otherwise, it must go someplace after the current element.  */
  else
    {
      for (ptr = head->current;
	   ptr->next != 0 && ptr->next->indx < indx;
	   ptr = ptr->next)
	;

      if (ptr->next)
	ptr->next->prev = element;

      element->next = ptr->next;
      element->prev = ptr;
      ptr->next = element;
    }

  /* Set up so this is the first element searched.  */
  head->current = element;
  head->indx = indx;
}

/* Clear a bitmap by freeing the linked list.  */

INLINE void
bitmap_clear (bitmap head)
{
  bitmap_element *element, *next;

  for (element = head->first; element != 0; element = next)
    {
      next = element->next;
      bitmap_elem_to_freelist (head, element);
    }

  head->first = head->current = 0;
}

/* Copy a bitmap to another bitmap.  */

void
bitmap_copy (bitmap to, bitmap from)
{
  bitmap_element *from_ptr, *to_ptr = 0;
#if BITMAP_ELEMENT_WORDS != 2
  int i;
#endif

  bitmap_clear (to);

  /* Copy elements in forward direction one at a time.  */
  for (from_ptr = from->first; from_ptr; from_ptr = from_ptr->next)
    {
      bitmap_element *to_elt = bitmap_element_allocate (to);

      to_elt->indx = from_ptr->indx;

#if BITMAP_ELEMENT_WORDS == 2
      to_elt->bits[0] = from_ptr->bits[0];
      to_elt->bits[1] = from_ptr->bits[1];
#else
      for (i = 0; i < BITMAP_ELEMENT_WORDS; i++)
	to_elt->bits[i] = from_ptr->bits[i];
#endif

      /* Here we have a special case of bitmap_element_link, for the case
	 where we know the links are being entered in sequence.  */
      if (to_ptr == 0)
	{
	  to->first = to->current = to_elt;
	  to->indx = from_ptr->indx;
	  to_elt->next = to_elt->prev = 0;
	}
      else
	{
	  to_elt->prev = to_ptr;
	  to_elt->next = 0;
	  to_ptr->next = to_elt;
	}

      to_ptr = to_elt;
    }
}

/* Find a bitmap element that would hold a bitmap's bit.
   Update the `current' field even if we can't find an element that
   would hold the bitmap's bit to make eventual allocation
   faster.  */

static INLINE bitmap_element *
bitmap_find_bit (bitmap head, unsigned int bit)
{
  bitmap_element *element;
  unsigned int indx = bit / BITMAP_ELEMENT_ALL_BITS;

  if (head->current == 0
      || head->indx == indx)
    return head->current;

  if (head->indx > indx)
    for (element = head->current;
	 element->prev != 0 && element->indx > indx;
	 element = element->prev)
      ;

  else
    for (element = head->current;
	 element->next != 0 && element->indx < indx;
	 element = element->next)
      ;

  /* `element' is the nearest to the one we want.  If it's not the one we
     want, the one we want doesn't exist.  */
  head->current = element;
  head->indx = element->indx;
  if (element != 0 && element->indx != indx)
    element = 0;

  return element;
}

/* Clear a single bit in a bitmap.  */

void
bitmap_clear_bit (bitmap head, int bit)
{
  bitmap_element *ptr = bitmap_find_bit (head, bit);

  if (ptr != 0)
    {
      unsigned bit_num  = bit % BITMAP_WORD_BITS;
      unsigned word_num = bit / BITMAP_WORD_BITS % BITMAP_ELEMENT_WORDS;
      ptr->bits[word_num] &= ~ (((BITMAP_WORD) 1) << bit_num);

      /* If we cleared the entire word, free up the element.  */
      if (bitmap_element_zerop (ptr))
	bitmap_element_free (head, ptr);
    }
}

/* Set a single bit in a bitmap.  */

void
bitmap_set_bit (bitmap head, int bit)
{
  bitmap_element *ptr = bitmap_find_bit (head, bit);
  unsigned word_num = bit / BITMAP_WORD_BITS % BITMAP_ELEMENT_WORDS;
  unsigned bit_num  = bit % BITMAP_WORD_BITS;
  BITMAP_WORD bit_val = ((BITMAP_WORD) 1) << bit_num;

  if (ptr == 0)
    {
      ptr = bitmap_element_allocate (head);
      ptr->indx = bit / BITMAP_ELEMENT_ALL_BITS;
      ptr->bits[word_num] = bit_val;
      bitmap_element_link (head, ptr);
    }
  else
    ptr->bits[word_num] |= bit_val;
}

/* Return whether a bit is set within a bitmap.  */

int
bitmap_bit_p (bitmap head, int bit)
{
  bitmap_element *ptr;
  unsigned bit_num;
  unsigned word_num;

  ptr = bitmap_find_bit (head, bit);
  if (ptr == 0)
    return 0;

  bit_num = bit % BITMAP_WORD_BITS;
  word_num = bit / BITMAP_WORD_BITS % BITMAP_ELEMENT_WORDS;

  return (ptr->bits[word_num] >> bit_num) & 1;
}

/* Return the bit number of the first set bit in the bitmap, or -1
   if the bitmap is empty.  */

int
bitmap_first_set_bit (bitmap a)
{
  bitmap_element *ptr = a->first;
  BITMAP_WORD word;
  unsigned word_num, bit_num;

  if (ptr == NULL)
    return -1;

#if BITMAP_ELEMENT_WORDS == 2
  word_num = 0, word = ptr->bits[0];
  if (word == 0)
    word_num = 1, word = ptr->bits[1];
#else
  for (word_num = 0; word_num < BITMAP_ELEMENT_WORDS; ++word_num)
    if ((word = ptr->bits[word_num]) != 0)
      break;
#endif

  /* Binary search for the first set bit.  */
  /* ??? It'd be nice to know if ffs or ffsl was available.  */

  bit_num = 0;
  word = word & -word;

#if nBITMAP_WORD_BITS > 64
 #error "Fill out the table."
#endif
#if nBITMAP_WORD_BITS > 32
  if ((word & 0xffffffff) == 0)
    word >>= 32, bit_num += 32;
#endif
  if ((word & 0xffff) == 0)
    word >>= 16, bit_num += 16;
  if ((word & 0xff) == 0)
    word >>= 8, bit_num += 8;
  if (word & 0xf0)
    bit_num += 4;
  if (word & 0xcc)
    bit_num += 2;
  if (word & 0xaa)
    bit_num += 1;

  return (ptr->indx * BITMAP_ELEMENT_ALL_BITS
	  + word_num * BITMAP_WORD_BITS
	  + bit_num);
}

/* Return the bit number of the last set bit in the bitmap, or -1
   if the bitmap is empty.  */

int
bitmap_last_set_bit (bitmap a)
{
  bitmap_element *ptr = a->first;
  BITMAP_WORD word;
  unsigned word_num, bit_num;

  if (ptr == NULL)
    return -1;

  while (ptr->next != NULL)
    ptr = ptr->next;

#if BITMAP_ELEMENT_WORDS == 2
  word_num = 1, word = ptr->bits[1];
  if (word == 0)
    word_num = 0, word = ptr->bits[0];
#else
  for (word_num = BITMAP_ELEMENT_WORDS; word_num-- > 0; )
    if ((word = ptr->bits[word_num]) != 0)
      break;
#endif

  /* Binary search for the last set bit.  */

  bit_num = 0;
#if nBITMAP_WORD_BITS > 64
 #error "Fill out the table."
#endif
#if nBITMAP_WORD_BITS > 32
  if (word & ~(BITMAP_WORD)0xffffffff)
    word >>= 32, bit_num += 32;
#endif
  if (word & 0xffff0000)
    word >>= 16, bit_num += 16;
  if (word & 0xff00)
    word >>= 8, bit_num += 8;
  if (word & 0xf0)
    word >>= 4, bit_num += 4;
  if (word & 0xc)
    word >>= 2, bit_num += 2;
  if (word & 0x2)
    bit_num += 1;

  return (ptr->indx * BITMAP_ELEMENT_ALL_BITS
	  + word_num * BITMAP_WORD_BITS
	  + bit_num);
}

/* Store in bitmap TO the result of combining bitmap FROM1 and FROM2 using
   a specific bit manipulation.  Return true if TO changes.  */

int
bitmap_operation (bitmap to, bitmap from1, bitmap from2,
		  enum bitmap_bits operation)
{
#define HIGHEST_INDEX (unsigned int) ~0

  bitmap_element *from1_ptr = from1->first;
  bitmap_element *from2_ptr = from2->first;
  unsigned int indx1 = (from1_ptr) ? from1_ptr->indx : HIGHEST_INDEX;
  unsigned int indx2 = (from2_ptr) ? from2_ptr->indx : HIGHEST_INDEX;
  bitmap_element *to_ptr = to->first;
  bitmap_element *from1_tmp;
  bitmap_element *from2_tmp;
  bitmap_element *to_tmp;
  unsigned int indx;
  int changed = 0;

#if BITMAP_ELEMENT_WORDS == 2
#define DOIT(OP)					\
  do {							\
    BITMAP_WORD t0, t1, f10, f11, f20, f21;		\
    f10 = from1_tmp->bits[0];				\
    f20 = from2_tmp->bits[0];				\
    t0 = f10 OP f20;					\
    changed |= (t0 != to_tmp->bits[0]);			\
    f11 = from1_tmp->bits[1];				\
    f21 = from2_tmp->bits[1];				\
    t1 = f11 OP f21;					\
    changed |= (t1 != to_tmp->bits[1]);			\
    to_tmp->bits[0] = t0;				\
    to_tmp->bits[1] = t1;				\
  } while (0)
#else
#define DOIT(OP)					\
  do {							\
    BITMAP_WORD t, f1, f2;				\
    int i;						\
    for (i = 0; i < BITMAP_ELEMENT_WORDS; ++i)		\
      {							\
	f1 = from1_tmp->bits[i];			\
	f2 = from2_tmp->bits[i];			\
	t = f1 OP f2;					\
	changed |= (t != to_tmp->bits[i]);		\
	to_tmp->bits[i] = t;				\
      }							\
  } while (0)
#endif

  to->first = to->current = 0;

  while (from1_ptr != 0 || from2_ptr != 0)
    {
      /* Figure out whether we need to substitute zero elements for
	 missing links.  */
      if (indx1 == indx2)
	{
	  indx = indx1;
	  from1_tmp = from1_ptr;
	  from2_tmp = from2_ptr;
	  from1_ptr = from1_ptr->next;
	  indx1 = (from1_ptr) ? from1_ptr->indx : HIGHEST_INDEX;
	  from2_ptr = from2_ptr->next;
	  indx2 = (from2_ptr) ? from2_ptr->indx : HIGHEST_INDEX;
	}
      else if (indx1 < indx2)
	{
	  indx = indx1;
	  from1_tmp = from1_ptr;
	  from2_tmp = &bitmap_zero_bits;
	  from1_ptr = from1_ptr->next;
	  indx1 = (from1_ptr) ? from1_ptr->indx : HIGHEST_INDEX;
	}
      else
	{
	  indx = indx2;
	  from1_tmp = &bitmap_zero_bits;
	  from2_tmp = from2_ptr;
	  from2_ptr = from2_ptr->next;
	  indx2 = (from2_ptr) ? from2_ptr->indx : HIGHEST_INDEX;
	}

      /* Find the appropriate element from TO.  Begin by discarding
	 elements that we've skipped.  */
      while (to_ptr && to_ptr->indx < indx)
	{
	  changed = 1;
	  to_tmp = to_ptr;
	  to_ptr = to_ptr->next;
	  bitmap_elem_to_freelist (to, to_tmp);
	}
      if (to_ptr && to_ptr->indx == indx)
	{
	  to_tmp = to_ptr;
	  to_ptr = to_ptr->next;
	}
      else
	to_tmp = bitmap_element_allocate (to);

      /* Do the operation, and if any bits are set, link it into the
	 linked list.  */
      switch (operation)
	{
	default:
	  abort ();

	case BITMAP_AND:
	  DOIT (&);
	  break;

	case BITMAP_AND_COMPL:
	  DOIT (&~);
	  break;

	case BITMAP_IOR:
	  DOIT (|);
	  break;
	case BITMAP_IOR_COMPL:
	  DOIT (|~);
	  break;
	case BITMAP_XOR:
	  DOIT (^);
	  break;
	}

      if (! bitmap_element_zerop (to_tmp))
	{
	  to_tmp->indx = indx;
	  bitmap_element_link (to, to_tmp);
	}
      else
	{
	  bitmap_elem_to_freelist (to, to_tmp);
	}
    }

  /* If we have elements of TO left over, free the lot.  */
  if (to_ptr)
    {
      changed = 1;
      for (to_tmp = to_ptr; to_tmp->next ; to_tmp = to_tmp->next)
	continue;
      if (to->using_obstack)
	{
	  to_tmp->next = bitmap_free;
	  bitmap_free = to_ptr;
	}
      else
	{
	  to_tmp->next = bitmap_ggc_free;
	  bitmap_ggc_free = to_ptr;
	}
    }

#undef DOIT

  return changed;
}

/* Return true if two bitmaps are identical.  */

int
bitmap_equal_p (bitmap a, bitmap b)
{
  bitmap_head c;
  int ret;

  memset (&c, 0, sizeof (c));
  ret = ! bitmap_operation (&c, a, b, BITMAP_XOR);
  bitmap_clear (&c);

  return ret;
}

/* Or into bitmap TO bitmap FROM1 and'ed with the complement of
   bitmap FROM2.  */

void
bitmap_ior_and_compl (bitmap to, bitmap from1, bitmap from2)
{
  bitmap_head tmp;

  tmp.first = tmp.current = 0;
  tmp.using_obstack = 0;

  bitmap_operation (&tmp, from1, from2, BITMAP_AND_COMPL);
  bitmap_operation (to, to, &tmp, BITMAP_IOR);
  bitmap_clear (&tmp);
}

int
bitmap_union_of_diff (bitmap dst, bitmap a, bitmap b, bitmap c)
{
  bitmap_head tmp;
  int changed;

  tmp.first = tmp.current = 0;
  tmp.using_obstack = 0;

  bitmap_operation (&tmp, b, c, BITMAP_AND_COMPL);
  changed = bitmap_operation (dst, &tmp, a, BITMAP_IOR);
  bitmap_clear (&tmp);

  return changed;
}

/* Initialize a bitmap header.  */

bitmap
bitmap_initialize (bitmap head, int using_obstack)
{
  if (head == NULL && ! using_obstack)
    head = ggc_alloc (sizeof (*head));

  head->first = head->current = 0;
  head->using_obstack = using_obstack;

  return head;
}

/* Debugging function to print out the contents of a bitmap.  */

void
debug_bitmap_file (FILE *file, bitmap head)
{
  bitmap_element *ptr;

  fprintf (file, "\nfirst = " HOST_PTR_PRINTF
	   " current = " HOST_PTR_PRINTF " indx = %u\n",
	   (void *) head->first, (void *) head->current, head->indx);

  for (ptr = head->first; ptr; ptr = ptr->next)
    {
      unsigned int i, j, col = 26;

      fprintf (file, "\t" HOST_PTR_PRINTF " next = " HOST_PTR_PRINTF
	       " prev = " HOST_PTR_PRINTF " indx = %u\n\t\tbits = {",
	       (void*) ptr, (void*) ptr->next, (void*) ptr->prev, ptr->indx);

      for (i = 0; i < BITMAP_ELEMENT_WORDS; i++)
	for (j = 0; j < BITMAP_WORD_BITS; j++)
	  if ((ptr->bits[i] >> j) & 1)
	    {
	      if (col > 70)
		{
		  fprintf (file, "\n\t\t\t");
		  col = 24;
		}

	      fprintf (file, " %u", (ptr->indx * BITMAP_ELEMENT_ALL_BITS
				     + i * BITMAP_WORD_BITS + j));
	      col += 4;
	    }

      fprintf (file, " }\n");
    }
}

/* Function to be called from the debugger to print the contents
   of a bitmap.  */

void
debug_bitmap (bitmap head)
{
  debug_bitmap_file (stdout, head);
}

/* Function to print out the contents of a bitmap.  Unlike debug_bitmap_file,
   it does not print anything but the bits.  */

void
bitmap_print (FILE *file, bitmap head, const char *prefix, const char *suffix)
{
  const char *comma = "";
  int i;

  fputs (prefix, file);
  EXECUTE_IF_SET_IN_BITMAP (head, 0, i,
			    {
			      fprintf (file, "%s%d", comma, i);
			      comma = ", ";
			    });
  fputs (suffix, file);
}

#include "gt-bitmap.h"
