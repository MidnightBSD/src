/* readtokens.c  -- Functions for reading tokens from an input stream.

   Copyright (C) 1990-1991, 1999-2004, 2006 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

   Written by Jim Meyering. */

/* This almost supercedes xreadline stuff -- using delim="\n"
   gives the same functionality, except that these functions
   would never return empty lines. */

#include <config.h>

#include "readtokens.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "xalloc.h"

#if USE_UNLOCKED_IO
# include "unlocked-io.h"
#endif

#define STREQ(a,b) ((a) == (b) || ((a) && (b) && *(a) == *(b) \
				   && strcmp(a, b) == 0))

/* Initialize a tokenbuffer. */

void
init_tokenbuffer (token_buffer *tokenbuffer)
{
  tokenbuffer->size = 0;
  tokenbuffer->buffer = NULL;
}

/* Read a token from STREAM into TOKENBUFFER.
   A token is delimited by any of the N_DELIM bytes in DELIM.
   Upon return, the token is in tokenbuffer->buffer and
   has a trailing '\0' instead of any original delimiter.
   The function value is the length of the token not including
   the final '\0'.  Upon EOF (i.e. on the call after the last
   token is read) or error, return -1 without modifying tokenbuffer.
   The EOF and error conditions may be distinguished in the caller
   by testing ferror (STREAM).

   This function works properly on lines containing NUL bytes
   and on files do not end with a delimiter.  */

size_t
readtoken (FILE *stream,
	   const char *delim,
	   size_t n_delim,
	   token_buffer *tokenbuffer)
{
  char *p;
  int c;
  size_t i, n;
  static const char *saved_delim = NULL;
  static char isdelim[256];
  bool same_delimiters;

  if (delim == NULL && saved_delim == NULL)
    abort ();

  same_delimiters = false;
  if (delim != saved_delim && saved_delim != NULL)
    {
      same_delimiters = true;
      for (i = 0; i < n_delim; i++)
	{
	  if (delim[i] != saved_delim[i])
	    {
	      same_delimiters = false;
	      break;
	    }
	}
    }

  if (!same_delimiters)
    {
      size_t j;
      saved_delim = delim;
      memset (isdelim, 0, sizeof isdelim);
      for (j = 0; j < n_delim; j++)
	{
	  unsigned char ch = delim[j];
	  isdelim[ch] = 1;
	}
    }

  /* FIXME: don't fool with this caching.  Use strchr instead.  */
  /* skip over any leading delimiters */
  for (c = getc (stream); c >= 0 && isdelim[c]; c = getc (stream))
    {
      /* empty */
    }

  p = tokenbuffer->buffer;
  n = tokenbuffer->size;
  i = 0;
  for (;;)
    {
      if (c < 0 && i == 0)
	return -1;

      if (i == n)
	p = x2nrealloc (p, &n, sizeof *p);

      if (c < 0)
	{
	  p[i] = 0;
	  break;
	}
      if (isdelim[c])
	{
	  p[i] = 0;
	  break;
	}
      p[i++] = c;
      c = getc (stream);
    }

  tokenbuffer->buffer = p;
  tokenbuffer->size = n;
  return i;
}

/* Build a NULL-terminated array of pointers to tokens
   read from STREAM.  Return the number of tokens read.
   All storage is obtained through calls to xmalloc-like functions.

   %%% Question: is it worth it to do a single
   %%% realloc() of `tokens' just before returning? */

size_t
readtokens (FILE *stream,
	    size_t projected_n_tokens,
	    const char *delim,
	    size_t n_delim,
	    char ***tokens_out,
	    size_t **token_lengths)
{
  token_buffer tb, *token = &tb;
  char **tokens;
  size_t *lengths;
  size_t sz;
  size_t n_tokens;

  if (projected_n_tokens == 0)
    projected_n_tokens = 64;
  else
    projected_n_tokens++;	/* add one for trailing NULL pointer */

  sz = projected_n_tokens;
  tokens = xnmalloc (sz, sizeof *tokens);
  lengths = xnmalloc (sz, sizeof *lengths);

  n_tokens = 0;
  init_tokenbuffer (token);
  for (;;)
    {
      char *tmp;
      size_t token_length = readtoken (stream, delim, n_delim, token);
      if (n_tokens >= sz)
	{
	  tokens = x2nrealloc (tokens, &sz, sizeof *tokens);
	  lengths = xnrealloc (lengths, sz, sizeof *lengths);
	}

      if (token_length == (size_t) -1)
	{
	  /* don't increment n_tokens for NULL entry */
	  tokens[n_tokens] = NULL;
	  lengths[n_tokens] = 0;
	  break;
	}
      tmp = xnmalloc (token_length + 1, sizeof *tmp);
      lengths[n_tokens] = token_length;
      tokens[n_tokens] = memcpy (tmp, token->buffer, token_length + 1);
      n_tokens++;
    }

  free (token->buffer);
  *tokens_out = tokens;
  if (token_lengths != NULL)
    *token_lengths = lengths;
  return n_tokens;
}
