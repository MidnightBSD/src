/* C++ Parser.
   Copyright (C) 2000, 2001, 2002, 2003, 2004,
   2005 Free Software Foundation, Inc.
   Written by Mark Mitchell <mark@codesourcery.com>.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GCC is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "dyn-string.h"
#include "varray.h"
#include "cpplib.h"
#include "tree.h"
#include "cp-tree.h"
#include "c-pragma.h"
#include "decl.h"
#include "flags.h"
#include "diagnostic.h"
#include "toplev.h"
#include "output.h"


/* The lexer.  */

/* Overview
   --------

   A cp_lexer represents a stream of cp_tokens.  It allows arbitrary
   look-ahead.

   Methodology
   -----------

   We use a circular buffer to store incoming tokens.

   Some artifacts of the C++ language (such as the
   expression/declaration ambiguity) require arbitrary look-ahead.
   The strategy we adopt for dealing with these problems is to attempt
   to parse one construct (e.g., the declaration) and fall back to the
   other (e.g., the expression) if that attempt does not succeed.
   Therefore, we must sometimes store an arbitrary number of tokens.

   The parser routinely peeks at the next token, and then consumes it
   later.  That also requires a buffer in which to store the tokens.
     
   In order to easily permit adding tokens to the end of the buffer,
   while removing them from the beginning of the buffer, we use a
   circular buffer.  */

/* A C++ token.  */

typedef struct cp_token GTY (())
{
  /* The kind of token.  */
  ENUM_BITFIELD (cpp_ttype) type : 8;
  /* If this token is a keyword, this value indicates which keyword.
     Otherwise, this value is RID_MAX.  */
  ENUM_BITFIELD (rid) keyword : 8;
  /* Token flags.  */
  unsigned char flags;
  /* The value associated with this token, if any.  */
  tree value;
  /* The location at which this token was found.  */
  location_t location;
} cp_token;

/* The number of tokens in a single token block.
   Computed so that cp_token_block fits in a 512B allocation unit.  */

#define CP_TOKEN_BLOCK_NUM_TOKENS ((512 - 3*sizeof (char*))/sizeof (cp_token))

/* A group of tokens.  These groups are chained together to store
   large numbers of tokens.  (For example, a token block is created
   when the body of an inline member function is first encountered;
   the tokens are processed later after the class definition is
   complete.)  

   This somewhat ungainly data structure (as opposed to, say, a
   variable-length array), is used due to constraints imposed by the
   current garbage-collection methodology.  If it is made more
   flexible, we could perhaps simplify the data structures involved.  */

typedef struct cp_token_block GTY (())
{
  /* The tokens.  */
  cp_token tokens[CP_TOKEN_BLOCK_NUM_TOKENS];
  /* The number of tokens in this block.  */
  size_t num_tokens;
  /* The next token block in the chain.  */
  struct cp_token_block *next;
  /* The previous block in the chain.  */
  struct cp_token_block *prev;
} cp_token_block;

typedef struct cp_token_cache GTY (())
{
  /* The first block in the cache.  NULL if there are no tokens in the
     cache.  */
  cp_token_block *first;
  /* The last block in the cache.  NULL If there are no tokens in the
     cache.  */
  cp_token_block *last;
} cp_token_cache;

/* Prototypes.  */

static cp_token_cache *cp_token_cache_new 
  (void);
static void cp_token_cache_push_token
  (cp_token_cache *, cp_token *);

/* Create a new cp_token_cache.  */

static cp_token_cache *
cp_token_cache_new (void)
{
  return ggc_alloc_cleared (sizeof (cp_token_cache));
}

/* Add *TOKEN to *CACHE.  */

static void
cp_token_cache_push_token (cp_token_cache *cache,
			   cp_token *token)
{
  cp_token_block *b = cache->last;

  /* See if we need to allocate a new token block.  */
  if (!b || b->num_tokens == CP_TOKEN_BLOCK_NUM_TOKENS)
    {
      b = ggc_alloc_cleared (sizeof (cp_token_block));
      b->prev = cache->last;
      if (cache->last)
	{
	  cache->last->next = b;
	  cache->last = b;
	}
      else
	cache->first = cache->last = b;
    }
  /* Add this token to the current token block.  */
  b->tokens[b->num_tokens++] = *token;
}

/* The cp_lexer structure represents the C++ lexer.  It is responsible
   for managing the token stream from the preprocessor and supplying
   it to the parser.  */

typedef struct cp_lexer GTY (())
{
  /* The memory allocated for the buffer.  Never NULL.  */
  cp_token * GTY ((length ("(%h.buffer_end - %h.buffer)"))) buffer;
  /* A pointer just past the end of the memory allocated for the buffer.  */
  cp_token * GTY ((skip (""))) buffer_end;
  /* The first valid token in the buffer, or NULL if none.  */
  cp_token * GTY ((skip (""))) first_token;
  /* The next available token.  If NEXT_TOKEN is NULL, then there are
     no more available tokens.  */
  cp_token * GTY ((skip (""))) next_token;
  /* A pointer just past the last available token.  If FIRST_TOKEN is
     NULL, however, there are no available tokens, and then this
     location is simply the place in which the next token read will be
     placed.  If LAST_TOKEN == FIRST_TOKEN, then the buffer is full.
     When the LAST_TOKEN == BUFFER, then the last token is at the
     highest memory address in the BUFFER.  */
  cp_token * GTY ((skip (""))) last_token;

  /* A stack indicating positions at which cp_lexer_save_tokens was
     called.  The top entry is the most recent position at which we
     began saving tokens.  The entries are differences in token
     position between FIRST_TOKEN and the first saved token.

     If the stack is non-empty, we are saving tokens.  When a token is
     consumed, the NEXT_TOKEN pointer will move, but the FIRST_TOKEN
     pointer will not.  The token stream will be preserved so that it
     can be reexamined later.

     If the stack is empty, then we are not saving tokens.  Whenever a
     token is consumed, the FIRST_TOKEN pointer will be moved, and the
     consumed token will be gone forever.  */
  varray_type saved_tokens;

  /* The STRING_CST tokens encountered while processing the current
     string literal.  */
  varray_type string_tokens;

  /* True if we should obtain more tokens from the preprocessor; false
     if we are processing a saved token cache.  */
  bool main_lexer_p;

  /* True if we should output debugging information.  */
  bool debugging_p;

  /* The next lexer in a linked list of lexers.  */
  struct cp_lexer *next;
} cp_lexer;

/* Prototypes.  */

static cp_lexer *cp_lexer_new_main
  (void);
static cp_lexer *cp_lexer_new_from_tokens
  (struct cp_token_cache *);
static int cp_lexer_saving_tokens
  (const cp_lexer *);
static cp_token *cp_lexer_next_token
  (cp_lexer *, cp_token *);
static cp_token *cp_lexer_prev_token
  (cp_lexer *, cp_token *);
static ptrdiff_t cp_lexer_token_difference 
  (cp_lexer *, cp_token *, cp_token *);
static cp_token *cp_lexer_read_token
  (cp_lexer *);
static void cp_lexer_maybe_grow_buffer
  (cp_lexer *);
static void cp_lexer_get_preprocessor_token
  (cp_lexer *, cp_token *);
static cp_token *cp_lexer_peek_token
  (cp_lexer *);
static cp_token *cp_lexer_peek_nth_token
  (cp_lexer *, size_t);
static inline bool cp_lexer_next_token_is
  (cp_lexer *, enum cpp_ttype);
static bool cp_lexer_next_token_is_not
  (cp_lexer *, enum cpp_ttype);
static bool cp_lexer_next_token_is_keyword
  (cp_lexer *, enum rid);
static cp_token *cp_lexer_consume_token 
  (cp_lexer *);
static void cp_lexer_purge_token
  (cp_lexer *);
static void cp_lexer_purge_tokens_after
  (cp_lexer *, cp_token *);
static void cp_lexer_save_tokens
  (cp_lexer *);
static void cp_lexer_commit_tokens
  (cp_lexer *);
static void cp_lexer_rollback_tokens
  (cp_lexer *);
static inline void cp_lexer_set_source_position_from_token 
  (cp_lexer *, const cp_token *);
static void cp_lexer_print_token
  (FILE *, cp_token *);
static inline bool cp_lexer_debugging_p 
  (cp_lexer *);
static void cp_lexer_start_debugging
  (cp_lexer *) ATTRIBUTE_UNUSED;
static void cp_lexer_stop_debugging
  (cp_lexer *) ATTRIBUTE_UNUSED;

/* Manifest constants.  */

#define CP_TOKEN_BUFFER_SIZE 5
#define CP_SAVED_TOKENS_SIZE 5

/* A token type for keywords, as opposed to ordinary identifiers.  */
#define CPP_KEYWORD ((enum cpp_ttype) (N_TTYPES + 1))

/* A token type for template-ids.  If a template-id is processed while
   parsing tentatively, it is replaced with a CPP_TEMPLATE_ID token;
   the value of the CPP_TEMPLATE_ID is whatever was returned by
   cp_parser_template_id.  */
#define CPP_TEMPLATE_ID ((enum cpp_ttype) (CPP_KEYWORD + 1))

/* A token type for nested-name-specifiers.  If a
   nested-name-specifier is processed while parsing tentatively, it is
   replaced with a CPP_NESTED_NAME_SPECIFIER token; the value of the
   CPP_NESTED_NAME_SPECIFIER is whatever was returned by
   cp_parser_nested_name_specifier_opt.  */
#define CPP_NESTED_NAME_SPECIFIER ((enum cpp_ttype) (CPP_TEMPLATE_ID + 1))

/* A token type for tokens that are not tokens at all; these are used
   to mark the end of a token block.  */
#define CPP_NONE (CPP_NESTED_NAME_SPECIFIER + 1)

/* Variables.  */

/* The stream to which debugging output should be written.  */
static FILE *cp_lexer_debug_stream;

/* Create a new main C++ lexer, the lexer that gets tokens from the
   preprocessor.  */

static cp_lexer *
cp_lexer_new_main (void)
{
  cp_lexer *lexer;
  cp_token first_token;

  /* It's possible that lexing the first token will load a PCH file,
     which is a GC collection point.  So we have to grab the first
     token before allocating any memory.  */
  cp_lexer_get_preprocessor_token (NULL, &first_token);
  c_common_no_more_pch ();

  /* Allocate the memory.  */
  lexer = ggc_alloc_cleared (sizeof (cp_lexer));

  /* Create the circular buffer.  */
  lexer->buffer = ggc_calloc (CP_TOKEN_BUFFER_SIZE, sizeof (cp_token));
  lexer->buffer_end = lexer->buffer + CP_TOKEN_BUFFER_SIZE;

  /* There is one token in the buffer.  */
  lexer->last_token = lexer->buffer + 1;
  lexer->first_token = lexer->buffer;
  lexer->next_token = lexer->buffer;
  memcpy (lexer->buffer, &first_token, sizeof (cp_token));

  /* This lexer obtains more tokens by calling c_lex.  */
  lexer->main_lexer_p = true;

  /* Create the SAVED_TOKENS stack.  */
  VARRAY_INT_INIT (lexer->saved_tokens, CP_SAVED_TOKENS_SIZE, "saved_tokens");
  
  /* Create the STRINGS array.  */
  VARRAY_TREE_INIT (lexer->string_tokens, 32, "strings");

  /* Assume we are not debugging.  */
  lexer->debugging_p = false;

  return lexer;
}

/* Create a new lexer whose token stream is primed with the TOKENS.
   When these tokens are exhausted, no new tokens will be read.  */

static cp_lexer *
cp_lexer_new_from_tokens (cp_token_cache *tokens)
{
  cp_lexer *lexer;
  cp_token *token;
  cp_token_block *block;
  ptrdiff_t num_tokens;

  /* Allocate the memory.  */
  lexer = ggc_alloc_cleared (sizeof (cp_lexer));

  /* Create a new buffer, appropriately sized.  */
  num_tokens = 0;
  for (block = tokens->first; block != NULL; block = block->next)
    num_tokens += block->num_tokens;
  lexer->buffer = ggc_alloc (num_tokens * sizeof (cp_token));
  lexer->buffer_end = lexer->buffer + num_tokens;
  
  /* Install the tokens.  */
  token = lexer->buffer;
  for (block = tokens->first; block != NULL; block = block->next)
    {
      memcpy (token, block->tokens, block->num_tokens * sizeof (cp_token));
      token += block->num_tokens;
    }

  /* The FIRST_TOKEN is the beginning of the buffer.  */
  lexer->first_token = lexer->buffer;
  /* The next available token is also at the beginning of the buffer.  */
  lexer->next_token = lexer->buffer;
  /* The buffer is full.  */
  lexer->last_token = lexer->first_token;

  /* This lexer doesn't obtain more tokens.  */
  lexer->main_lexer_p = false;

  /* Create the SAVED_TOKENS stack.  */
  VARRAY_INT_INIT (lexer->saved_tokens, CP_SAVED_TOKENS_SIZE, "saved_tokens");
  
  /* Create the STRINGS array.  */
  VARRAY_TREE_INIT (lexer->string_tokens, 32, "strings");

  /* Assume we are not debugging.  */
  lexer->debugging_p = false;

  return lexer;
}

/* Returns nonzero if debugging information should be output.  */

static inline bool
cp_lexer_debugging_p (cp_lexer *lexer)
{
  return lexer->debugging_p;
}

/* Set the current source position from the information stored in
   TOKEN.  */

static inline void
cp_lexer_set_source_position_from_token (cp_lexer *lexer ATTRIBUTE_UNUSED ,
                                         const cp_token *token)
{
  /* Ideally, the source position information would not be a global
     variable, but it is.  */

  /* Update the line number.  */
  if (token->type != CPP_EOF)
    input_location = token->location;
}

/* TOKEN points into the circular token buffer.  Return a pointer to
   the next token in the buffer.  */

static inline cp_token *
cp_lexer_next_token (cp_lexer* lexer, cp_token* token)
{
  token++;
  if (token == lexer->buffer_end)
    token = lexer->buffer;
  return token;
}

/* TOKEN points into the circular token buffer.  Return a pointer to
   the previous token in the buffer.  */

static inline cp_token *
cp_lexer_prev_token (cp_lexer* lexer, cp_token* token)
{
  if (token == lexer->buffer)
    token = lexer->buffer_end;
  return token - 1;
}

/* nonzero if we are presently saving tokens.  */

static int
cp_lexer_saving_tokens (const cp_lexer* lexer)
{
  return VARRAY_ACTIVE_SIZE (lexer->saved_tokens) != 0;
}

/* Return a pointer to the token that is N tokens beyond TOKEN in the
   buffer.  */

static cp_token *
cp_lexer_advance_token (cp_lexer *lexer, cp_token *token, ptrdiff_t n)
{
  token += n;
  if (token >= lexer->buffer_end)
    token = lexer->buffer + (token - lexer->buffer_end);
  return token;
}

/* Returns the number of times that START would have to be incremented
   to reach FINISH.  If START and FINISH are the same, returns zero.  */

static ptrdiff_t
cp_lexer_token_difference (cp_lexer* lexer, cp_token* start, cp_token* finish)
{
  if (finish >= start)
    return finish - start;
  else
    return ((lexer->buffer_end - lexer->buffer)
	    - (start - finish));
}

/* Obtain another token from the C preprocessor and add it to the
   token buffer.  Returns the newly read token.  */

static cp_token *
cp_lexer_read_token (cp_lexer* lexer)
{
  cp_token *token;

  /* Make sure there is room in the buffer.  */
  cp_lexer_maybe_grow_buffer (lexer);

  /* If there weren't any tokens, then this one will be the first.  */
  if (!lexer->first_token)
    lexer->first_token = lexer->last_token;
  /* Similarly, if there were no available tokens, there is one now.  */
  if (!lexer->next_token)
    lexer->next_token = lexer->last_token;

  /* Figure out where we're going to store the new token.  */
  token = lexer->last_token;

  /* Get a new token from the preprocessor.  */
  cp_lexer_get_preprocessor_token (lexer, token);

  /* Increment LAST_TOKEN.  */
  lexer->last_token = cp_lexer_next_token (lexer, token);

  /* Strings should have type `const char []'.  Right now, we will
     have an ARRAY_TYPE that is constant rather than an array of
     constant elements.
     FIXME: Make fix_string_type get this right in the first place.  */
  if ((token->type == CPP_STRING || token->type == CPP_WSTRING)
      && flag_const_strings)
    {
      tree type;

      /* Get the current type.  It will be an ARRAY_TYPE.  */
      type = TREE_TYPE (token->value);
      /* Use build_cplus_array_type to rebuild the array, thereby
	 getting the right type.  */
      type = build_cplus_array_type (TREE_TYPE (type), TYPE_DOMAIN (type));
      /* Reset the type of the token.  */
      TREE_TYPE (token->value) = type;
    }

  return token;
}

/* If the circular buffer is full, make it bigger.  */

static void
cp_lexer_maybe_grow_buffer (cp_lexer* lexer)
{
  /* If the buffer is full, enlarge it.  */
  if (lexer->last_token == lexer->first_token)
    {
      cp_token *new_buffer;
      cp_token *old_buffer;
      cp_token *new_first_token;
      ptrdiff_t buffer_length;
      size_t num_tokens_to_copy;

      /* Remember the current buffer pointer.  It will become invalid,
	 but we will need to do pointer arithmetic involving this
	 value.  */
      old_buffer = lexer->buffer;
      /* Compute the current buffer size.  */
      buffer_length = lexer->buffer_end - lexer->buffer;
      /* Allocate a buffer twice as big.  */
      new_buffer = ggc_realloc (lexer->buffer, 
				2 * buffer_length * sizeof (cp_token));
      
      /* Because the buffer is circular, logically consecutive tokens
	 are not necessarily placed consecutively in memory.
	 Therefore, we must keep move the tokens that were before
	 FIRST_TOKEN to the second half of the newly allocated
	 buffer.  */
      num_tokens_to_copy = (lexer->first_token - old_buffer);
      memcpy (new_buffer + buffer_length,
	      new_buffer,
	      num_tokens_to_copy * sizeof (cp_token));
      /* Clear the rest of the buffer.  We never look at this storage,
	 but the garbage collector may.  */
      memset (new_buffer + buffer_length + num_tokens_to_copy, 0, 
	      (buffer_length - num_tokens_to_copy) * sizeof (cp_token));

      /* Now recompute all of the buffer pointers.  */
      new_first_token 
	= new_buffer + (lexer->first_token - old_buffer);
      if (lexer->next_token != NULL)
	{
	  ptrdiff_t next_token_delta;

	  if (lexer->next_token > lexer->first_token)
	    next_token_delta = lexer->next_token - lexer->first_token;
	  else
	    next_token_delta = 
	      buffer_length - (lexer->first_token - lexer->next_token);
	  lexer->next_token = new_first_token + next_token_delta;
	}
      lexer->last_token = new_first_token + buffer_length;
      lexer->buffer = new_buffer;
      lexer->buffer_end = new_buffer + buffer_length * 2;
      lexer->first_token = new_first_token;
    }
}

/* Store the next token from the preprocessor in *TOKEN.  */

static void 
cp_lexer_get_preprocessor_token (cp_lexer *lexer ATTRIBUTE_UNUSED ,
                                 cp_token *token)
{
  bool done;

  /* If this not the main lexer, return a terminating CPP_EOF token.  */
  if (lexer != NULL && !lexer->main_lexer_p)
    {
      token->type = CPP_EOF;
      token->location.line = 0;
      token->location.file = NULL;
      token->value = NULL_TREE;
      token->keyword = RID_MAX;

      return;
    }

  done = false;
  /* Keep going until we get a token we like.  */
  while (!done)
    {
      /* Get a new token from the preprocessor.  */
      token->type = c_lex_with_flags (&token->value, &token->flags);
      /* Issue messages about tokens we cannot process.  */
      switch (token->type)
	{
	case CPP_ATSIGN:
	case CPP_HASH:
	case CPP_PASTE:
	  error ("invalid token");
	  break;

	default:
	  /* This is a good token, so we exit the loop.  */
	  done = true;
	  break;
	}
    }
  /* Now we've got our token.  */
  token->location = input_location;

  /* Check to see if this token is a keyword.  */
  if (token->type == CPP_NAME 
      && C_IS_RESERVED_WORD (token->value))
    {
      /* Mark this token as a keyword.  */
      token->type = CPP_KEYWORD;
      /* Record which keyword.  */
      token->keyword = C_RID_CODE (token->value);
      /* Update the value.  Some keywords are mapped to particular
	 entities, rather than simply having the value of the
	 corresponding IDENTIFIER_NODE.  For example, `__const' is
	 mapped to `const'.  */
      token->value = ridpointers[token->keyword];
    }
  else
    token->keyword = RID_MAX;
}

/* Return a pointer to the next token in the token stream, but do not
   consume it.  */

static cp_token *
cp_lexer_peek_token (cp_lexer* lexer)
{
  cp_token *token;

  /* If there are no tokens, read one now.  */
  if (!lexer->next_token)
    cp_lexer_read_token (lexer);

  /* Provide debugging output.  */
  if (cp_lexer_debugging_p (lexer))
    {
      fprintf (cp_lexer_debug_stream, "cp_lexer: peeking at token: ");
      cp_lexer_print_token (cp_lexer_debug_stream, lexer->next_token);
      fprintf (cp_lexer_debug_stream, "\n");
    }

  token = lexer->next_token;
  cp_lexer_set_source_position_from_token (lexer, token);
  return token;
}

/* Return true if the next token has the indicated TYPE.  */

static bool
cp_lexer_next_token_is (cp_lexer* lexer, enum cpp_ttype type)
{
  cp_token *token;

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (lexer);
  /* Check to see if it has the indicated TYPE.  */
  return token->type == type;
}

/* Return true if the next token does not have the indicated TYPE.  */

static bool
cp_lexer_next_token_is_not (cp_lexer* lexer, enum cpp_ttype type)
{
  return !cp_lexer_next_token_is (lexer, type);
}

/* Return true if the next token is the indicated KEYWORD.  */

static bool
cp_lexer_next_token_is_keyword (cp_lexer* lexer, enum rid keyword)
{
  cp_token *token;

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (lexer);
  /* Check to see if it is the indicated keyword.  */
  return token->keyword == keyword;
}

/* Return a pointer to the Nth token in the token stream.  If N is 1,
   then this is precisely equivalent to cp_lexer_peek_token.  */

static cp_token *
cp_lexer_peek_nth_token (cp_lexer* lexer, size_t n)
{
  cp_token *token;

  /* N is 1-based, not zero-based.  */
  my_friendly_assert (n > 0, 20000224);

  /* Skip ahead from NEXT_TOKEN, reading more tokens as necessary.  */
  token = lexer->next_token;
  /* If there are no tokens in the buffer, get one now.  */
  if (!token)
    {
      cp_lexer_read_token (lexer);
      token = lexer->next_token;
    }

  /* Now, read tokens until we have enough.  */
  while (--n > 0)
    {
      /* Advance to the next token.  */
      token = cp_lexer_next_token (lexer, token);
      /* If that's all the tokens we have, read a new one.  */
      if (token == lexer->last_token)
	token = cp_lexer_read_token (lexer);
    }

  return token;
}

/* Consume the next token.  The pointer returned is valid only until
   another token is read.  Callers should preserve copy the token
   explicitly if they will need its value for a longer period of
   time.  */

static cp_token *
cp_lexer_consume_token (cp_lexer* lexer)
{
  cp_token *token;

  /* If there are no tokens, read one now.  */
  if (!lexer->next_token)
    cp_lexer_read_token (lexer);

  /* Remember the token we'll be returning.  */
  token = lexer->next_token;

  /* Increment NEXT_TOKEN.  */
  lexer->next_token = cp_lexer_next_token (lexer, 
					   lexer->next_token);
  /* Check to see if we're all out of tokens.  */
  if (lexer->next_token == lexer->last_token)
    lexer->next_token = NULL;

  /* If we're not saving tokens, then move FIRST_TOKEN too.  */
  if (!cp_lexer_saving_tokens (lexer))
    {
      /* If there are no tokens available, set FIRST_TOKEN to NULL.  */
      if (!lexer->next_token)
	lexer->first_token = NULL;
      else
	lexer->first_token = lexer->next_token;
    }

  /* Provide debugging output.  */
  if (cp_lexer_debugging_p (lexer))
    {
      fprintf (cp_lexer_debug_stream, "cp_lexer: consuming token: ");
      cp_lexer_print_token (cp_lexer_debug_stream, token);
      fprintf (cp_lexer_debug_stream, "\n");
    }

  return token;
}

/* Permanently remove the next token from the token stream.  There
   must be a valid next token already; this token never reads
   additional tokens from the preprocessor.  */

static void
cp_lexer_purge_token (cp_lexer *lexer)
{
  cp_token *token;
  cp_token *next_token;

  token = lexer->next_token;
  while (true) 
    {
      next_token = cp_lexer_next_token (lexer, token);
      if (next_token == lexer->last_token)
	break;
      *token = *next_token;
      token = next_token;
    }

  lexer->last_token = token;
  /* The token purged may have been the only token remaining; if so,
     clear NEXT_TOKEN.  */
  if (lexer->next_token == token)
    lexer->next_token = NULL;
}

/* Permanently remove all tokens after TOKEN, up to, but not
   including, the token that will be returned next by
   cp_lexer_peek_token.  */

static void
cp_lexer_purge_tokens_after (cp_lexer *lexer, cp_token *token)
{
  cp_token *peek;
  cp_token *t1;
  cp_token *t2;

  if (lexer->next_token)
    {
      /* Copy the tokens that have not yet been read to the location
	 immediately following TOKEN.  */
      t1 = cp_lexer_next_token (lexer, token);
      t2 = peek = cp_lexer_peek_token (lexer);
      /* Move tokens into the vacant area between TOKEN and PEEK.  */
      while (t2 != lexer->last_token)
	{
	  *t1 = *t2;
	  t1 = cp_lexer_next_token (lexer, t1);
	  t2 = cp_lexer_next_token (lexer, t2);
	}
      /* Now, the next available token is right after TOKEN.  */
      lexer->next_token = cp_lexer_next_token (lexer, token);
      /* And the last token is wherever we ended up.  */
      lexer->last_token = t1;
    }
  else
    {
      /* There are no tokens in the buffer, so there is nothing to
	 copy.  The last token in the buffer is TOKEN itself.  */
      lexer->last_token = cp_lexer_next_token (lexer, token);
    }
}

/* Begin saving tokens.  All tokens consumed after this point will be
   preserved.  */

static void
cp_lexer_save_tokens (cp_lexer* lexer)
{
  /* Provide debugging output.  */
  if (cp_lexer_debugging_p (lexer))
    fprintf (cp_lexer_debug_stream, "cp_lexer: saving tokens\n");

  /* Make sure that LEXER->NEXT_TOKEN is non-NULL so that we can
     restore the tokens if required.  */
  if (!lexer->next_token)
    cp_lexer_read_token (lexer);

  VARRAY_PUSH_INT (lexer->saved_tokens,
		   cp_lexer_token_difference (lexer,
					      lexer->first_token,
					      lexer->next_token));
}

/* Commit to the portion of the token stream most recently saved.  */

static void
cp_lexer_commit_tokens (cp_lexer* lexer)
{
  /* Provide debugging output.  */
  if (cp_lexer_debugging_p (lexer))
    fprintf (cp_lexer_debug_stream, "cp_lexer: committing tokens\n");

  VARRAY_POP (lexer->saved_tokens);
}

/* Return all tokens saved since the last call to cp_lexer_save_tokens
   to the token stream.  Stop saving tokens.  */

static void
cp_lexer_rollback_tokens (cp_lexer* lexer)
{
  size_t delta;

  /* Provide debugging output.  */
  if (cp_lexer_debugging_p (lexer))
    fprintf (cp_lexer_debug_stream, "cp_lexer: restoring tokens\n");

  /* Find the token that was the NEXT_TOKEN when we started saving
     tokens.  */
  delta = VARRAY_TOP_INT(lexer->saved_tokens);
  /* Make it the next token again now.  */
  lexer->next_token = cp_lexer_advance_token (lexer,
					      lexer->first_token, 
					      delta);
  /* It might be the case that there were no tokens when we started
     saving tokens, but that there are some tokens now.  */
  if (!lexer->next_token && lexer->first_token)
    lexer->next_token = lexer->first_token;

  /* Stop saving tokens.  */
  VARRAY_POP (lexer->saved_tokens);
}

/* Print a representation of the TOKEN on the STREAM.  */

static void
cp_lexer_print_token (FILE * stream, cp_token* token)
{
  const char *token_type = NULL;

  /* Figure out what kind of token this is.  */
  switch (token->type)
    {
    case CPP_EQ:
      token_type = "EQ";
      break;

    case CPP_COMMA:
      token_type = "COMMA";
      break;

    case CPP_OPEN_PAREN:
      token_type = "OPEN_PAREN";
      break;

    case CPP_CLOSE_PAREN:
      token_type = "CLOSE_PAREN";
      break;

    case CPP_OPEN_BRACE:
      token_type = "OPEN_BRACE";
      break;

    case CPP_CLOSE_BRACE:
      token_type = "CLOSE_BRACE";
      break;

    case CPP_SEMICOLON:
      token_type = "SEMICOLON";
      break;

    case CPP_NAME:
      token_type = "NAME";
      break;

    case CPP_EOF:
      token_type = "EOF";
      break;

    case CPP_KEYWORD:
      token_type = "keyword";
      break;

      /* This is not a token that we know how to handle yet.  */
    default:
      break;
    }

  /* If we have a name for the token, print it out.  Otherwise, we
     simply give the numeric code.  */
  if (token_type)
    fprintf (stream, "%s", token_type);
  else
    fprintf (stream, "%d", token->type);
  /* And, for an identifier, print the identifier name.  */
  if (token->type == CPP_NAME 
      /* Some keywords have a value that is not an IDENTIFIER_NODE.
	 For example, `struct' is mapped to an INTEGER_CST.  */
      || (token->type == CPP_KEYWORD 
	  && TREE_CODE (token->value) == IDENTIFIER_NODE))
    fprintf (stream, " %s", IDENTIFIER_POINTER (token->value));
}

/* Start emitting debugging information.  */

static void
cp_lexer_start_debugging (cp_lexer* lexer)
{
  ++lexer->debugging_p;
}
  
/* Stop emitting debugging information.  */

static void
cp_lexer_stop_debugging (cp_lexer* lexer)
{
  --lexer->debugging_p;
}


/* The parser.  */

/* Overview
   --------

   A cp_parser parses the token stream as specified by the C++
   grammar.  Its job is purely parsing, not semantic analysis.  For
   example, the parser breaks the token stream into declarators,
   expressions, statements, and other similar syntactic constructs.
   It does not check that the types of the expressions on either side
   of an assignment-statement are compatible, or that a function is
   not declared with a parameter of type `void'.

   The parser invokes routines elsewhere in the compiler to perform
   semantic analysis and to build up the abstract syntax tree for the
   code processed.  

   The parser (and the template instantiation code, which is, in a
   way, a close relative of parsing) are the only parts of the
   compiler that should be calling push_scope and pop_scope, or
   related functions.  The parser (and template instantiation code)
   keeps track of what scope is presently active; everything else
   should simply honor that.  (The code that generates static
   initializers may also need to set the scope, in order to check
   access control correctly when emitting the initializers.)

   Methodology
   -----------
   
   The parser is of the standard recursive-descent variety.  Upcoming
   tokens in the token stream are examined in order to determine which
   production to use when parsing a non-terminal.  Some C++ constructs
   require arbitrary look ahead to disambiguate.  For example, it is
   impossible, in the general case, to tell whether a statement is an
   expression or declaration without scanning the entire statement.
   Therefore, the parser is capable of "parsing tentatively."  When the
   parser is not sure what construct comes next, it enters this mode.
   Then, while we attempt to parse the construct, the parser queues up
   error messages, rather than issuing them immediately, and saves the
   tokens it consumes.  If the construct is parsed successfully, the
   parser "commits", i.e., it issues any queued error messages and
   the tokens that were being preserved are permanently discarded.
   If, however, the construct is not parsed successfully, the parser
   rolls back its state completely so that it can resume parsing using
   a different alternative.

   Future Improvements
   -------------------
   
   The performance of the parser could probably be improved
   substantially.  Some possible improvements include:

     - The expression parser recurses through the various levels of
       precedence as specified in the grammar, rather than using an
       operator-precedence technique.  Therefore, parsing a simple
       identifier requires multiple recursive calls.

     - We could often eliminate the need to parse tentatively by
       looking ahead a little bit.  In some places, this approach
       might not entirely eliminate the need to parse tentatively, but
       it might still speed up the average case.  */

/* Flags that are passed to some parsing functions.  These values can
   be bitwise-ored together.  */

typedef enum cp_parser_flags
{
  /* No flags.  */
  CP_PARSER_FLAGS_NONE = 0x0,
  /* The construct is optional.  If it is not present, then no error
     should be issued.  */
  CP_PARSER_FLAGS_OPTIONAL = 0x1,
  /* When parsing a type-specifier, do not allow user-defined types.  */
  CP_PARSER_FLAGS_NO_USER_DEFINED_TYPES = 0x2
} cp_parser_flags;

/* The different kinds of declarators we want to parse.  */

typedef enum cp_parser_declarator_kind
{
  /* We want an abstract declartor.  */
  CP_PARSER_DECLARATOR_ABSTRACT,
  /* We want a named declarator.  */
  CP_PARSER_DECLARATOR_NAMED,
  /* We don't mind, but the name must be an unqualified-id.  */
  CP_PARSER_DECLARATOR_EITHER
} cp_parser_declarator_kind;

/* A mapping from a token type to a corresponding tree node type.  */

typedef struct cp_parser_token_tree_map_node
{
  /* The token type.  */
  ENUM_BITFIELD (cpp_ttype) token_type : 8;
  /* The corresponding tree code.  */
  ENUM_BITFIELD (tree_code) tree_type : 8;
} cp_parser_token_tree_map_node;

/* A complete map consists of several ordinary entries, followed by a
   terminator.  The terminating entry has a token_type of CPP_EOF.  */

typedef cp_parser_token_tree_map_node cp_parser_token_tree_map[];

/* The status of a tentative parse.  */

typedef enum cp_parser_status_kind
{
  /* No errors have occurred.  */
  CP_PARSER_STATUS_KIND_NO_ERROR,
  /* An error has occurred.  */
  CP_PARSER_STATUS_KIND_ERROR,
  /* We are committed to this tentative parse, whether or not an error
     has occurred.  */
  CP_PARSER_STATUS_KIND_COMMITTED
} cp_parser_status_kind;

/* Context that is saved and restored when parsing tentatively.  */

typedef struct cp_parser_context GTY (())
{
  /* If this is a tentative parsing context, the status of the
     tentative parse.  */
  enum cp_parser_status_kind status;
  /* If non-NULL, we have just seen a `x->' or `x.' expression.  Names
     that are looked up in this context must be looked up both in the
     scope given by OBJECT_TYPE (the type of `x' or `*x') and also in
     the context of the containing expression.  */
  tree object_type;
  /* The next parsing context in the stack.  */
  struct cp_parser_context *next;
} cp_parser_context;

/* Prototypes.  */

/* Constructors and destructors.  */

static cp_parser_context *cp_parser_context_new
  (cp_parser_context *);

/* Class variables.  */

static GTY((deletable (""))) cp_parser_context* cp_parser_context_free_list;

/* Constructors and destructors.  */

/* Construct a new context.  The context below this one on the stack
   is given by NEXT.  */

static cp_parser_context *
cp_parser_context_new (cp_parser_context* next)
{
  cp_parser_context *context;

  /* Allocate the storage.  */
  if (cp_parser_context_free_list != NULL)
    {
      /* Pull the first entry from the free list.  */
      context = cp_parser_context_free_list;
      cp_parser_context_free_list = context->next;
      memset (context, 0, sizeof (*context));
    }
  else
    context = ggc_alloc_cleared (sizeof (cp_parser_context));
  /* No errors have occurred yet in this context.  */
  context->status = CP_PARSER_STATUS_KIND_NO_ERROR;
  /* If this is not the bottomost context, copy information that we
     need from the previous context.  */
  if (next)
    {
      /* If, in the NEXT context, we are parsing an `x->' or `x.'
	 expression, then we are parsing one in this context, too.  */
      context->object_type = next->object_type;
      /* Thread the stack.  */
      context->next = next;
    }

  return context;
}

/* The cp_parser structure represents the C++ parser.  */

typedef struct cp_parser GTY(())
{
  /* The lexer from which we are obtaining tokens.  */
  cp_lexer *lexer;

  /* The scope in which names should be looked up.  If NULL_TREE, then
     we look up names in the scope that is currently open in the
     source program.  If non-NULL, this is either a TYPE or
     NAMESPACE_DECL for the scope in which we should look.  

     This value is not cleared automatically after a name is looked
     up, so we must be careful to clear it before starting a new look
     up sequence.  (If it is not cleared, then `X::Y' followed by `Z'
     will look up `Z' in the scope of `X', rather than the current
     scope.)  Unfortunately, it is difficult to tell when name lookup
     is complete, because we sometimes peek at a token, look it up,
     and then decide not to consume it.  */
  tree scope;

  /* OBJECT_SCOPE and QUALIFYING_SCOPE give the scopes in which the
     last lookup took place.  OBJECT_SCOPE is used if an expression
     like "x->y" or "x.y" was used; it gives the type of "*x" or "x",
     respectively.  QUALIFYING_SCOPE is used for an expression of the 
     form "X::Y"; it refers to X.  */
  tree object_scope;
  tree qualifying_scope;

  /* A stack of parsing contexts.  All but the bottom entry on the
     stack will be tentative contexts.

     We parse tentatively in order to determine which construct is in
     use in some situations.  For example, in order to determine
     whether a statement is an expression-statement or a
     declaration-statement we parse it tentatively as a
     declaration-statement.  If that fails, we then reparse the same
     token stream as an expression-statement.  */
  cp_parser_context *context;

  /* True if we are parsing GNU C++.  If this flag is not set, then
     GNU extensions are not recognized.  */
  bool allow_gnu_extensions_p;

  /* TRUE if the `>' token should be interpreted as the greater-than
     operator.  FALSE if it is the end of a template-id or
     template-parameter-list.  */
  bool greater_than_is_operator_p;

  /* TRUE if default arguments are allowed within a parameter list
     that starts at this point. FALSE if only a gnu extension makes
     them permissible.  */
  bool default_arg_ok_p;
  
  /* TRUE if we are parsing an integral constant-expression.  See
     [expr.const] for a precise definition.  */
  bool integral_constant_expression_p;

  /* TRUE if we are parsing an integral constant-expression -- but a
     non-constant expression should be permitted as well.  This flag
     is used when parsing an array bound so that GNU variable-length
     arrays are tolerated.  */
  bool allow_non_integral_constant_expression_p;

  /* TRUE if ALLOW_NON_CONSTANT_EXPRESSION_P is TRUE and something has
     been seen that makes the expression non-constant.  */
  bool non_integral_constant_expression_p;

  /* TRUE if we are parsing the argument to "__offsetof__".  */
  bool in_offsetof_p;

  /* TRUE if local variable names and `this' are forbidden in the
     current context.  */
  bool local_variables_forbidden_p;

  /* TRUE if the declaration we are parsing is part of a
     linkage-specification of the form `extern string-literal
     declaration'.  */
  bool in_unbraced_linkage_specification_p;

  /* TRUE if we are presently parsing a declarator, after the
     direct-declarator.  */
  bool in_declarator_p;

  /* TRUE if we are presently parsing a template-argument-list.  */
  bool in_template_argument_list_p;

  /* TRUE if we are presently parsing the body of an
     iteration-statement.  */
  bool in_iteration_statement_p;

  /* TRUE if we are presently parsing the body of a switch
     statement.  */
  bool in_switch_statement_p;

  /* TRUE if we are parsing a type-id in an expression context.  In
     such a situation, both "type (expr)" and "type (type)" are valid
     alternatives.  */
  bool in_type_id_in_expr_p;

  /* If non-NULL, then we are parsing a construct where new type
     definitions are not permitted.  The string stored here will be
     issued as an error message if a type is defined.  */
  const char *type_definition_forbidden_message;

  /* A list of lists. The outer list is a stack, used for member
     functions of local classes. At each level there are two sub-list,
     one on TREE_VALUE and one on TREE_PURPOSE. Each of those
     sub-lists has a FUNCTION_DECL or TEMPLATE_DECL on their
     TREE_VALUE's. The functions are chained in reverse declaration
     order.

     The TREE_PURPOSE sublist contains those functions with default
     arguments that need post processing, and the TREE_VALUE sublist
     contains those functions with definitions that need post
     processing.

     These lists can only be processed once the outermost class being
     defined is complete.  */
  tree unparsed_functions_queues;

  /* The number of classes whose definitions are currently in
     progress.  */
  unsigned num_classes_being_defined;

  /* The number of template parameter lists that apply directly to the
     current declaration.  */
  unsigned num_template_parameter_lists;
} cp_parser;

/* The type of a function that parses some kind of expression.  */
typedef tree (*cp_parser_expression_fn) (cp_parser *);

/* Prototypes.  */

/* Constructors and destructors.  */

static cp_parser *cp_parser_new
  (void);

/* Routines to parse various constructs.  

   Those that return `tree' will return the error_mark_node (rather
   than NULL_TREE) if a parse error occurs, unless otherwise noted.
   Sometimes, they will return an ordinary node if error-recovery was
   attempted, even though a parse error occurred.  So, to check
   whether or not a parse error occurred, you should always use
   cp_parser_error_occurred.  If the construct is optional (indicated
   either by an `_opt' in the name of the function that does the
   parsing or via a FLAGS parameter), then NULL_TREE is returned if
   the construct is not present.  */

/* Lexical conventions [gram.lex]  */

static tree cp_parser_identifier
  (cp_parser *);

/* Basic concepts [gram.basic]  */

static bool cp_parser_translation_unit
  (cp_parser *);

/* Expressions [gram.expr]  */

static tree cp_parser_primary_expression
  (cp_parser *, cp_id_kind *, tree *);
static tree cp_parser_id_expression
  (cp_parser *, bool, bool, bool *, bool);
static tree cp_parser_unqualified_id
  (cp_parser *, bool, bool, bool);
static tree cp_parser_nested_name_specifier_opt
  (cp_parser *, bool, bool, bool, bool);
static tree cp_parser_nested_name_specifier
  (cp_parser *, bool, bool, bool, bool);
static tree cp_parser_class_or_namespace_name
  (cp_parser *, bool, bool, bool, bool, bool);
static tree cp_parser_postfix_expression
  (cp_parser *, bool);
static tree cp_parser_parenthesized_expression_list
  (cp_parser *, bool, bool *);
static void cp_parser_pseudo_destructor_name
  (cp_parser *, tree *, tree *);
static tree cp_parser_unary_expression
  (cp_parser *, bool);
static enum tree_code cp_parser_unary_operator
  (cp_token *);
static tree cp_parser_new_expression
  (cp_parser *);
static tree cp_parser_new_placement
  (cp_parser *);
static tree cp_parser_new_type_id
  (cp_parser *);
static tree cp_parser_new_declarator_opt
  (cp_parser *);
static tree cp_parser_direct_new_declarator
  (cp_parser *);
static tree cp_parser_new_initializer
  (cp_parser *);
static tree cp_parser_delete_expression
  (cp_parser *);
static tree cp_parser_cast_expression 
  (cp_parser *, bool);
static tree cp_parser_pm_expression
  (cp_parser *);
static tree cp_parser_multiplicative_expression
  (cp_parser *);
static tree cp_parser_additive_expression
  (cp_parser *);
static tree cp_parser_shift_expression
  (cp_parser *);
static tree cp_parser_relational_expression
  (cp_parser *);
static tree cp_parser_equality_expression
  (cp_parser *);
static tree cp_parser_and_expression
  (cp_parser *);
static tree cp_parser_exclusive_or_expression
  (cp_parser *);
static tree cp_parser_inclusive_or_expression
  (cp_parser *);
static tree cp_parser_logical_and_expression
  (cp_parser *);
static tree cp_parser_logical_or_expression 
  (cp_parser *);
static tree cp_parser_question_colon_clause
  (cp_parser *, tree);
static tree cp_parser_assignment_expression
  (cp_parser *);
static enum tree_code cp_parser_assignment_operator_opt
  (cp_parser *);
static tree cp_parser_expression
  (cp_parser *);
static tree cp_parser_constant_expression
  (cp_parser *, bool, bool *);

/* Statements [gram.stmt.stmt]  */

static void cp_parser_statement
  (cp_parser *, bool);
static tree cp_parser_labeled_statement
  (cp_parser *, bool);
static tree cp_parser_expression_statement
  (cp_parser *, bool);
static tree cp_parser_compound_statement
  (cp_parser *, bool);
static void cp_parser_statement_seq_opt
  (cp_parser *, bool);
static tree cp_parser_selection_statement
  (cp_parser *);
static tree cp_parser_condition
  (cp_parser *);
static tree cp_parser_iteration_statement
  (cp_parser *);
static void cp_parser_for_init_statement
  (cp_parser *);
static tree cp_parser_jump_statement
  (cp_parser *);
static void cp_parser_declaration_statement
  (cp_parser *);

static tree cp_parser_implicitly_scoped_statement
  (cp_parser *);
static void cp_parser_already_scoped_statement
  (cp_parser *);

/* Declarations [gram.dcl.dcl] */

static void cp_parser_declaration_seq_opt
  (cp_parser *);
static void cp_parser_declaration
  (cp_parser *);
static void cp_parser_block_declaration
  (cp_parser *, bool);
static void cp_parser_simple_declaration
  (cp_parser *, bool);
static tree cp_parser_decl_specifier_seq 
  (cp_parser *, cp_parser_flags, tree *, int *);
static tree cp_parser_storage_class_specifier_opt
  (cp_parser *);
static tree cp_parser_function_specifier_opt
  (cp_parser *);
static tree cp_parser_type_specifier
  (cp_parser *, cp_parser_flags, bool, bool, int *, bool *);
static tree cp_parser_simple_type_specifier
  (cp_parser *, cp_parser_flags, bool);
static tree cp_parser_type_name
  (cp_parser *);
static tree cp_parser_elaborated_type_specifier
  (cp_parser *, bool, bool);
static tree cp_parser_enum_specifier
  (cp_parser *);
static void cp_parser_enumerator_list
  (cp_parser *, tree);
static void cp_parser_enumerator_definition 
  (cp_parser *, tree);
static tree cp_parser_namespace_name
  (cp_parser *);
static void cp_parser_namespace_definition
  (cp_parser *);
static void cp_parser_namespace_body
  (cp_parser *);
static tree cp_parser_qualified_namespace_specifier
  (cp_parser *);
static void cp_parser_namespace_alias_definition
  (cp_parser *);
static void cp_parser_using_declaration
  (cp_parser *);
static void cp_parser_using_directive
  (cp_parser *);
static void cp_parser_asm_definition
  (cp_parser *);
static void cp_parser_linkage_specification
  (cp_parser *);

/* Declarators [gram.dcl.decl] */

static tree cp_parser_init_declarator
  (cp_parser *, tree, tree, bool, bool, int, bool *);
static tree cp_parser_declarator
  (cp_parser *, cp_parser_declarator_kind, int *, bool *, bool);
static tree cp_parser_direct_declarator
  (cp_parser *, cp_parser_declarator_kind, int *, bool);
static enum tree_code cp_parser_ptr_operator
  (cp_parser *, tree *, tree *);
static tree cp_parser_cv_qualifier_seq_opt
  (cp_parser *);
static tree cp_parser_cv_qualifier_opt
  (cp_parser *);
static tree cp_parser_declarator_id
  (cp_parser *);
static tree cp_parser_type_id
  (cp_parser *);
static tree cp_parser_type_specifier_seq
  (cp_parser *);
static tree cp_parser_parameter_declaration_clause
  (cp_parser *);
static tree cp_parser_parameter_declaration_list
  (cp_parser *);
static tree cp_parser_parameter_declaration
  (cp_parser *, bool, bool *);
static void cp_parser_function_body
  (cp_parser *);
static tree cp_parser_initializer
  (cp_parser *, bool *, bool *);
static tree cp_parser_initializer_clause
  (cp_parser *, bool *);
static tree cp_parser_initializer_list
  (cp_parser *, bool *);

static bool cp_parser_ctor_initializer_opt_and_function_body
  (cp_parser *);

/* Classes [gram.class] */

static tree cp_parser_class_name
  (cp_parser *, bool, bool, bool, bool, bool, bool);
static tree cp_parser_class_specifier
  (cp_parser *);
static tree cp_parser_class_head
  (cp_parser *, bool *, tree *);
static enum tag_types cp_parser_class_key
  (cp_parser *);
static void cp_parser_member_specification_opt
  (cp_parser *);
static void cp_parser_member_declaration
  (cp_parser *);
static tree cp_parser_pure_specifier
  (cp_parser *);
static tree cp_parser_constant_initializer
  (cp_parser *);

/* Derived classes [gram.class.derived] */

static tree cp_parser_base_clause
  (cp_parser *);
static tree cp_parser_base_specifier
  (cp_parser *);

/* Special member functions [gram.special] */

static tree cp_parser_conversion_function_id
  (cp_parser *);
static tree cp_parser_conversion_type_id
  (cp_parser *);
static tree cp_parser_conversion_declarator_opt
  (cp_parser *);
static bool cp_parser_ctor_initializer_opt
  (cp_parser *);
static void cp_parser_mem_initializer_list
  (cp_parser *);
static tree cp_parser_mem_initializer
  (cp_parser *);
static tree cp_parser_mem_initializer_id
  (cp_parser *);

/* Overloading [gram.over] */

static tree cp_parser_operator_function_id
  (cp_parser *);
static tree cp_parser_operator
  (cp_parser *);

/* Templates [gram.temp] */

static void cp_parser_template_declaration
  (cp_parser *, bool);
static tree cp_parser_template_parameter_list
  (cp_parser *);
static tree cp_parser_template_parameter
  (cp_parser *);
static tree cp_parser_type_parameter
  (cp_parser *);
static tree cp_parser_template_id
  (cp_parser *, bool, bool, bool);
static tree cp_parser_template_name
  (cp_parser *, bool, bool, bool, bool *);
static tree cp_parser_template_argument_list
  (cp_parser *);
static tree cp_parser_template_argument
  (cp_parser *);
static void cp_parser_explicit_instantiation
  (cp_parser *);
static void cp_parser_explicit_specialization
  (cp_parser *);

/* Exception handling [gram.exception] */

static tree cp_parser_try_block 
  (cp_parser *);
static bool cp_parser_function_try_block
  (cp_parser *);
static void cp_parser_handler_seq
  (cp_parser *);
static void cp_parser_handler
  (cp_parser *);
static tree cp_parser_exception_declaration
  (cp_parser *);
static tree cp_parser_throw_expression
  (cp_parser *);
static tree cp_parser_exception_specification_opt
  (cp_parser *);
static tree cp_parser_type_id_list
  (cp_parser *);

/* GNU Extensions */

static tree cp_parser_asm_specification_opt
  (cp_parser *);
static tree cp_parser_asm_operand_list
  (cp_parser *);
static tree cp_parser_asm_clobber_list
  (cp_parser *);
static tree cp_parser_attributes_opt
  (cp_parser *);
static tree cp_parser_attribute_list
  (cp_parser *);
static bool cp_parser_extension_opt
  (cp_parser *, int *);
static void cp_parser_label_declaration
  (cp_parser *);

/* Utility Routines */

static tree cp_parser_lookup_name
  (cp_parser *, tree, bool, bool, bool, bool);
static tree cp_parser_lookup_name_simple
  (cp_parser *, tree);
static tree cp_parser_maybe_treat_template_as_class
  (tree, bool);
static bool cp_parser_check_declarator_template_parameters
  (cp_parser *, tree);
static bool cp_parser_check_template_parameters
  (cp_parser *, unsigned);
static tree cp_parser_simple_cast_expression
  (cp_parser *);
static tree cp_parser_binary_expression
  (cp_parser *, const cp_parser_token_tree_map, cp_parser_expression_fn);
static tree cp_parser_global_scope_opt
  (cp_parser *, bool);
static bool cp_parser_constructor_declarator_p
  (cp_parser *, bool);
static tree cp_parser_function_definition_from_specifiers_and_declarator
  (cp_parser *, tree, tree, tree);
static tree cp_parser_function_definition_after_declarator
  (cp_parser *, bool);
static void cp_parser_template_declaration_after_export
  (cp_parser *, bool);
static tree cp_parser_single_declaration
  (cp_parser *, bool, bool *);
static tree cp_parser_functional_cast
  (cp_parser *, tree);
static tree cp_parser_save_member_function_body
  (cp_parser *, tree, tree, tree);
static tree cp_parser_enclosed_template_argument_list
  (cp_parser *);
static void cp_parser_save_default_args
  (cp_parser *, tree);
static void cp_parser_late_parsing_for_member
  (cp_parser *, tree);
static void cp_parser_late_parsing_default_args
  (cp_parser *, tree);
static tree cp_parser_sizeof_operand
  (cp_parser *, enum rid);
static bool cp_parser_declares_only_class_p
  (cp_parser *);
static bool cp_parser_friend_p
  (tree);
static bool cp_parser_typedef_p
  (tree);
static cp_token *cp_parser_require
  (cp_parser *, enum cpp_ttype, const char *);
static cp_token *cp_parser_require_keyword
  (cp_parser *, enum rid, const char *);
static bool cp_parser_token_starts_function_definition_p 
  (cp_token *);
static bool cp_parser_next_token_starts_class_definition_p
  (cp_parser *);
static bool cp_parser_next_token_ends_template_argument_p
  (cp_parser *);
static bool cp_parser_nth_token_starts_template_argument_list_p
  (cp_parser *, size_t);
static enum tag_types cp_parser_token_is_class_key
  (cp_token *);
static void cp_parser_check_class_key
  (enum tag_types, tree type);
static void cp_parser_check_access_in_redeclaration
  (tree type);
static bool cp_parser_optional_template_keyword
  (cp_parser *);
static void cp_parser_pre_parsed_nested_name_specifier 
  (cp_parser *);
static void cp_parser_cache_group
  (cp_parser *, cp_token_cache *, enum cpp_ttype, unsigned);
static void cp_parser_parse_tentatively 
  (cp_parser *);
static void cp_parser_commit_to_tentative_parse
  (cp_parser *);
static void cp_parser_abort_tentative_parse
  (cp_parser *);
static bool cp_parser_parse_definitely
  (cp_parser *);
static inline bool cp_parser_parsing_tentatively
  (cp_parser *);
static bool cp_parser_committed_to_tentative_parse
  (cp_parser *);
static void cp_parser_error
  (cp_parser *, const char *);
static void cp_parser_name_lookup_error
  (cp_parser *, tree, tree, const char *);
static bool cp_parser_simulate_error
  (cp_parser *);
static void cp_parser_check_type_definition
  (cp_parser *);
static void cp_parser_check_for_definition_in_return_type
  (tree, tree);
static void cp_parser_check_for_invalid_template_id
  (cp_parser *, tree);
static bool cp_parser_non_integral_constant_expression
  (cp_parser *, const char *);
static bool cp_parser_diagnose_invalid_type_name
  (cp_parser *);
static int cp_parser_skip_to_closing_parenthesis
  (cp_parser *, bool, bool, bool);
static void cp_parser_skip_to_end_of_statement
  (cp_parser *);
static void cp_parser_consume_semicolon_at_end_of_statement
  (cp_parser *);
static void cp_parser_skip_to_end_of_block_or_statement
  (cp_parser *);
static void cp_parser_skip_to_closing_brace
  (cp_parser *);
static void cp_parser_skip_until_found
  (cp_parser *, enum cpp_ttype, const char *);
static bool cp_parser_error_occurred
  (cp_parser *);
static bool cp_parser_allow_gnu_extensions_p
  (cp_parser *);
static bool cp_parser_is_string_literal
  (cp_token *);
static bool cp_parser_is_keyword 
  (cp_token *, enum rid);

/* Returns nonzero if we are parsing tentatively.  */

static inline bool
cp_parser_parsing_tentatively (cp_parser* parser)
{
  return parser->context->next != NULL;
}

/* Returns nonzero if TOKEN is a string literal.  */

static bool
cp_parser_is_string_literal (cp_token* token)
{
  return (token->type == CPP_STRING || token->type == CPP_WSTRING);
}

/* Returns nonzero if TOKEN is the indicated KEYWORD.  */

static bool
cp_parser_is_keyword (cp_token* token, enum rid keyword)
{
  return token->keyword == keyword;
}

/* Issue the indicated error MESSAGE.  */

static void
cp_parser_error (cp_parser* parser, const char* message)
{
  /* Output the MESSAGE -- unless we're parsing tentatively.  */
  if (!cp_parser_simulate_error (parser))
    {
      cp_token *token;
      token = cp_lexer_peek_token (parser->lexer);
      c_parse_error (message, 
		     /* Because c_parser_error does not understand
			CPP_KEYWORD, keywords are treated like
			identifiers.  */
		     (token->type == CPP_KEYWORD ? CPP_NAME : token->type), 
		     token->value);
    }
}

/* Issue an error about name-lookup failing.  NAME is the
   IDENTIFIER_NODE DECL is the result of
   the lookup (as returned from cp_parser_lookup_name).  DESIRED is
   the thing that we hoped to find.  */

static void
cp_parser_name_lookup_error (cp_parser* parser,
			     tree name,
			     tree decl,
			     const char* desired)
{
  /* If name lookup completely failed, tell the user that NAME was not
     declared.  */
  if (decl == error_mark_node)
    {
      if (parser->scope && parser->scope != global_namespace)
	error ("`%D::%D' has not been declared", 
	       parser->scope, name);
      else if (parser->scope == global_namespace)
	error ("`::%D' has not been declared", name);
      else
	error ("`%D' has not been declared", name);
    }
  else if (parser->scope && parser->scope != global_namespace)
    error ("`%D::%D' %s", parser->scope, name, desired);
  else if (parser->scope == global_namespace)
    error ("`::%D' %s", name, desired);
  else
    error ("`%D' %s", name, desired);
}

/* If we are parsing tentatively, remember that an error has occurred
   during this tentative parse.  Returns true if the error was
   simulated; false if a messgae should be issued by the caller.  */

static bool
cp_parser_simulate_error (cp_parser* parser)
{
  if (cp_parser_parsing_tentatively (parser)
      && !cp_parser_committed_to_tentative_parse (parser))
    {
      parser->context->status = CP_PARSER_STATUS_KIND_ERROR;
      return true;
    }
  return false;
}

/* This function is called when a type is defined.  If type
   definitions are forbidden at this point, an error message is
   issued.  */

static void
cp_parser_check_type_definition (cp_parser* parser)
{
  /* If types are forbidden here, issue a message.  */
  if (parser->type_definition_forbidden_message)
    /* Use `%s' to print the string in case there are any escape
       characters in the message.  */
    error ("%s", parser->type_definition_forbidden_message);
}

/* This function is called when the DECLARATOR is processed.  The TYPE
   was a type defined in the decl-specifiers.  If it is invalid to
   define a type in the decl-specifiers for DECLARATOR, an error is
   issued.  */

static void
cp_parser_check_for_definition_in_return_type (tree declarator, tree type)
{
  /* [dcl.fct] forbids type definitions in return types.
     Unfortunately, it's not easy to know whether or not we are
     processing a return type until after the fact.  */
  while (declarator
	 && (TREE_CODE (declarator) == INDIRECT_REF
	     || TREE_CODE (declarator) == ADDR_EXPR))
    declarator = TREE_OPERAND (declarator, 0);
  if (declarator
      && TREE_CODE (declarator) == CALL_EXPR)
    {
      error ("new types may not be defined in a return type");
      inform ("(perhaps a semicolon is missing after the definition of `%T')",
	      type);
    }
}

/* A type-specifier (TYPE) has been parsed which cannot be followed by
   "<" in any valid C++ program.  If the next token is indeed "<",
   issue a message warning the user about what appears to be an
   invalid attempt to form a template-id.  */

static void
cp_parser_check_for_invalid_template_id (cp_parser* parser, 
					 tree type)
{
  ptrdiff_t start;
  cp_token *token;

  if (cp_lexer_next_token_is (parser->lexer, CPP_LESS))
    {
      if (TYPE_P (type))
	error ("`%T' is not a template", type);
      else if (TREE_CODE (type) == IDENTIFIER_NODE)
	error ("`%s' is not a template", IDENTIFIER_POINTER (type));
      else
	error ("invalid template-id");
      /* Remember the location of the invalid "<".  */
      if (cp_parser_parsing_tentatively (parser)
	  && !cp_parser_committed_to_tentative_parse (parser))
	{
	  token = cp_lexer_peek_token (parser->lexer);
	  token = cp_lexer_prev_token (parser->lexer, token);
	  start = cp_lexer_token_difference (parser->lexer,
					     parser->lexer->first_token,
					     token);
	}
      else
	start = -1;
      /* Consume the "<".  */
      cp_lexer_consume_token (parser->lexer);
      /* Parse the template arguments.  */
      cp_parser_enclosed_template_argument_list (parser);
      /* Permanently remove the invalid template arguments so that
	 this error message is not issued again.  */
      if (start >= 0)
	{
	  token = cp_lexer_advance_token (parser->lexer,
					  parser->lexer->first_token,
					  start);
	  cp_lexer_purge_tokens_after (parser->lexer, token);
	}
    }
}

/* If parsing an integral constant-expression, issue an error message
   about the fact that THING appeared and return true.  Otherwise,
   return false, marking the current expression as non-constant.  */

static bool
cp_parser_non_integral_constant_expression (cp_parser  *parser,
					    const char *thing)
{
  if (parser->integral_constant_expression_p)
    {
      if (!parser->allow_non_integral_constant_expression_p)
	{
	  error ("%s cannot appear in a constant-expression", thing);
	  return true;
	}
      parser->non_integral_constant_expression_p = true;
    }
  return false;
}

/* Check for a common situation where a type-name should be present,
   but is not, and issue a sensible error message.  Returns true if an
   invalid type-name was detected.  */

static bool
cp_parser_diagnose_invalid_type_name (cp_parser *parser)
{
  /* If the next two tokens are both identifiers, the code is
     erroneous. The usual cause of this situation is code like:

       T t;

     where "T" should name a type -- but does not.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_NAME)
      && cp_lexer_peek_nth_token (parser->lexer, 2)->type == CPP_NAME)
    {
      tree name;

      /* If parsing tentatively, we should commit; we really are
	 looking at a declaration.  */
      /* Consume the first identifier.  */
      name = cp_lexer_consume_token (parser->lexer)->value;
      /* Issue an error message.  */
      error ("`%s' does not name a type", IDENTIFIER_POINTER (name));
      /* If we're in a template class, it's possible that the user was
	 referring to a type from a base class.  For example:

	   template <typename T> struct A { typedef T X; };
	   template <typename T> struct B : public A<T> { X x; };

	 The user should have said "typename A<T>::X".  */
      if (processing_template_decl && current_class_type)
	{
	  tree b;

	  for (b = TREE_CHAIN (TYPE_BINFO (current_class_type));
	       b;
	       b = TREE_CHAIN (b))
	    {
	      tree base_type = BINFO_TYPE (b);
	      if (CLASS_TYPE_P (base_type) 
		  && dependent_type_p (base_type))
		{
		  tree field;
		  /* Go from a particular instantiation of the
		     template (which will have an empty TYPE_FIELDs),
		     to the main version.  */
		  base_type = CLASSTYPE_PRIMARY_TEMPLATE_TYPE (base_type);
		  for (field = TYPE_FIELDS (base_type);
		       field;
		       field = TREE_CHAIN (field))
		    if (TREE_CODE (field) == TYPE_DECL
			&& DECL_NAME (field) == name)
		      {
			error ("(perhaps `typename %T::%s' was intended)",
			       BINFO_TYPE (b), IDENTIFIER_POINTER (name));
			break;
		      }
		  if (field)
		    break;
		}
	    }
	}
      /* Skip to the end of the declaration; there's no point in
	 trying to process it.  */
      cp_parser_skip_to_end_of_statement (parser);
      
      return true;
    }

  return false;
}

/* Consume tokens up to, and including, the next non-nested closing `)'. 
   Returns 1 iff we found a closing `)'.  RECOVERING is true, if we
   are doing error recovery. Returns -1 if OR_COMMA is true and we
   found an unnested comma.  */

static int
cp_parser_skip_to_closing_parenthesis (cp_parser *parser,
				       bool recovering, 
				       bool or_comma,
				       bool consume_paren)
{
  unsigned paren_depth = 0;
  unsigned brace_depth = 0;

  if (recovering && !or_comma && cp_parser_parsing_tentatively (parser)
      && !cp_parser_committed_to_tentative_parse (parser))
    return 0;
  
  while (true)
    {
      cp_token *token;
      
      /* If we've run out of tokens, then there is no closing `)'.  */
      if (cp_lexer_next_token_is (parser->lexer, CPP_EOF))
	return 0;

      token = cp_lexer_peek_token (parser->lexer);
      
      /* This matches the processing in skip_to_end_of_statement.  */
      if (token->type == CPP_SEMICOLON && !brace_depth)
	return 0;
      if (token->type == CPP_OPEN_BRACE)
	++brace_depth;
      if (token->type == CPP_CLOSE_BRACE)
	{
	  if (!brace_depth--)
	    return 0;
	}
      if (recovering && or_comma && token->type == CPP_COMMA
	  && !brace_depth && !paren_depth)
	return -1;
      
      if (!brace_depth)
	{
	  /* If it is an `(', we have entered another level of nesting.  */
	  if (token->type == CPP_OPEN_PAREN)
	    ++paren_depth;
	  /* If it is a `)', then we might be done.  */
	  else if (token->type == CPP_CLOSE_PAREN && !paren_depth--)
	    {
	      if (consume_paren)
		cp_lexer_consume_token (parser->lexer);
	      return 1;
	    }
	}
      
      /* Consume the token.  */
      cp_lexer_consume_token (parser->lexer);
    }
}

/* Consume tokens until we reach the end of the current statement.
   Normally, that will be just before consuming a `;'.  However, if a
   non-nested `}' comes first, then we stop before consuming that.  */

static void
cp_parser_skip_to_end_of_statement (cp_parser* parser)
{
  unsigned nesting_depth = 0;

  while (true)
    {
      cp_token *token;

      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);
      /* If we've run out of tokens, stop.  */
      if (token->type == CPP_EOF)
	break;
      /* If the next token is a `;', we have reached the end of the
	 statement.  */
      if (token->type == CPP_SEMICOLON && !nesting_depth)
	break;
      /* If the next token is a non-nested `}', then we have reached
	 the end of the current block.  */
      if (token->type == CPP_CLOSE_BRACE)
	{
	  /* If this is a non-nested `}', stop before consuming it.
	     That way, when confronted with something like:

	       { 3 + } 

	     we stop before consuming the closing `}', even though we
	     have not yet reached a `;'.  */
	  if (nesting_depth == 0)
	    break;
	  /* If it is the closing `}' for a block that we have
	     scanned, stop -- but only after consuming the token.
	     That way given:

	        void f g () { ... }
		typedef int I;

	     we will stop after the body of the erroneously declared
	     function, but before consuming the following `typedef'
	     declaration.  */
	  if (--nesting_depth == 0)
	    {
	      cp_lexer_consume_token (parser->lexer);
	      break;
	    }
	}
      /* If it the next token is a `{', then we are entering a new
	 block.  Consume the entire block.  */
      else if (token->type == CPP_OPEN_BRACE)
	++nesting_depth;
      /* Consume the token.  */
      cp_lexer_consume_token (parser->lexer);
    }
}

/* This function is called at the end of a statement or declaration.
   If the next token is a semicolon, it is consumed; otherwise, error
   recovery is attempted.  */

static void
cp_parser_consume_semicolon_at_end_of_statement (cp_parser *parser)
{
  /* Look for the trailing `;'.  */
  if (!cp_parser_require (parser, CPP_SEMICOLON, "`;'"))
    {
      /* If there is additional (erroneous) input, skip to the end of
	 the statement.  */
      cp_parser_skip_to_end_of_statement (parser);
      /* If the next token is now a `;', consume it.  */
      if (cp_lexer_next_token_is (parser->lexer, CPP_SEMICOLON))
	cp_lexer_consume_token (parser->lexer);
    }
}

/* Skip tokens until we have consumed an entire block, or until we
   have consumed a non-nested `;'.  */

static void
cp_parser_skip_to_end_of_block_or_statement (cp_parser* parser)
{
  unsigned nesting_depth = 0;

  while (true)
    {
      cp_token *token;

      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);
      /* If we've run out of tokens, stop.  */
      if (token->type == CPP_EOF)
	break;
      /* If the next token is a `;', we have reached the end of the
	 statement.  */
      if (token->type == CPP_SEMICOLON && !nesting_depth)
	{
	  /* Consume the `;'.  */
	  cp_lexer_consume_token (parser->lexer);
	  break;
	}
      /* Consume the token.  */
      token = cp_lexer_consume_token (parser->lexer);
      /* If the next token is a non-nested `}', then we have reached
	 the end of the current block.  */
      if (token->type == CPP_CLOSE_BRACE 
	  && (nesting_depth == 0 || --nesting_depth == 0))
	break;
      /* If it the next token is a `{', then we are entering a new
	 block.  Consume the entire block.  */
      if (token->type == CPP_OPEN_BRACE)
	++nesting_depth;
    }
}

/* Skip tokens until a non-nested closing curly brace is the next
   token.  */

static void
cp_parser_skip_to_closing_brace (cp_parser *parser)
{
  unsigned nesting_depth = 0;

  while (true)
    {
      cp_token *token;

      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);
      /* If we've run out of tokens, stop.  */
      if (token->type == CPP_EOF)
	break;
      /* If the next token is a non-nested `}', then we have reached
	 the end of the current block.  */
      if (token->type == CPP_CLOSE_BRACE && nesting_depth-- == 0)
	break;
      /* If it the next token is a `{', then we are entering a new
	 block.  Consume the entire block.  */
      else if (token->type == CPP_OPEN_BRACE)
	++nesting_depth;
      /* Consume the token.  */
      cp_lexer_consume_token (parser->lexer);
    }
}

/* Create a new C++ parser.  */

static cp_parser *
cp_parser_new (void)
{
  cp_parser *parser;
  cp_lexer *lexer;

  /* cp_lexer_new_main is called before calling ggc_alloc because
     cp_lexer_new_main might load a PCH file.  */
  lexer = cp_lexer_new_main ();

  parser = ggc_alloc_cleared (sizeof (cp_parser));
  parser->lexer = lexer;
  parser->context = cp_parser_context_new (NULL);

  /* For now, we always accept GNU extensions.  */
  parser->allow_gnu_extensions_p = 1;

  /* The `>' token is a greater-than operator, not the end of a
     template-id.  */
  parser->greater_than_is_operator_p = true;

  parser->default_arg_ok_p = true;
  
  /* We are not parsing a constant-expression.  */
  parser->integral_constant_expression_p = false;
  parser->allow_non_integral_constant_expression_p = false;
  parser->non_integral_constant_expression_p = false;

  /* We are not parsing offsetof.  */
  parser->in_offsetof_p = false;

  /* Local variable names are not forbidden.  */
  parser->local_variables_forbidden_p = false;

  /* We are not processing an `extern "C"' declaration.  */
  parser->in_unbraced_linkage_specification_p = false;

  /* We are not processing a declarator.  */
  parser->in_declarator_p = false;

  /* We are not processing a template-argument-list.  */
  parser->in_template_argument_list_p = false;

  /* We are not in an iteration statement.  */
  parser->in_iteration_statement_p = false;

  /* We are not in a switch statement.  */
  parser->in_switch_statement_p = false;

  /* We are not parsing a type-id inside an expression.  */
  parser->in_type_id_in_expr_p = false;

  /* The unparsed function queue is empty.  */
  parser->unparsed_functions_queues = build_tree_list (NULL_TREE, NULL_TREE);

  /* There are no classes being defined.  */
  parser->num_classes_being_defined = 0;

  /* No template parameters apply.  */
  parser->num_template_parameter_lists = 0;

  return parser;
}

/* Lexical conventions [gram.lex]  */

/* Parse an identifier.  Returns an IDENTIFIER_NODE representing the
   identifier.  */

static tree 
cp_parser_identifier (cp_parser* parser)
{
  cp_token *token;

  /* Look for the identifier.  */
  token = cp_parser_require (parser, CPP_NAME, "identifier");
  /* Return the value.  */
  return token ? token->value : error_mark_node;
}

/* Basic concepts [gram.basic]  */

/* Parse a translation-unit.

   translation-unit:
     declaration-seq [opt]  

   Returns TRUE if all went well.  */

static bool
cp_parser_translation_unit (cp_parser* parser)
{
  while (true)
    {
      cp_parser_declaration_seq_opt (parser);

      /* If there are no tokens left then all went well.  */
      if (cp_lexer_next_token_is (parser->lexer, CPP_EOF))
	break;
      
      /* Otherwise, issue an error message.  */
      cp_parser_error (parser, "expected declaration");
      return false;
    }

  /* Consume the EOF token.  */
  cp_parser_require (parser, CPP_EOF, "end-of-file");
  
  /* Finish up.  */
  finish_translation_unit ();

  /* All went well.  */
  return true;
}

/* Expressions [gram.expr] */

/* Parse a primary-expression.

   primary-expression:
     literal
     this
     ( expression )
     id-expression

   GNU Extensions:

   primary-expression:
     ( compound-statement )
     __builtin_va_arg ( assignment-expression , type-id )

   literal:
     __null

   Returns a representation of the expression.  

   *IDK indicates what kind of id-expression (if any) was present.  

   *QUALIFYING_CLASS is set to a non-NULL value if the id-expression can be
   used as the operand of a pointer-to-member.  In that case,
   *QUALIFYING_CLASS gives the class that is used as the qualifying
   class in the pointer-to-member.  */

static tree
cp_parser_primary_expression (cp_parser *parser, 
			      cp_id_kind *idk,
			      tree *qualifying_class)
{
  cp_token *token;

  /* Assume the primary expression is not an id-expression.  */
  *idk = CP_ID_KIND_NONE;
  /* And that it cannot be used as pointer-to-member.  */
  *qualifying_class = NULL_TREE;

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  switch (token->type)
    {
      /* literal:
	   integer-literal
	   character-literal
	   floating-literal
	   string-literal
	   boolean-literal  */
    case CPP_CHAR:
    case CPP_WCHAR:
    case CPP_STRING:
    case CPP_WSTRING:
    case CPP_NUMBER:
      token = cp_lexer_consume_token (parser->lexer);
      return token->value;

    case CPP_OPEN_PAREN:
      {
	tree expr;
	bool saved_greater_than_is_operator_p;

	/* Consume the `('.  */
	cp_lexer_consume_token (parser->lexer);
	/* Within a parenthesized expression, a `>' token is always
	   the greater-than operator.  */
	saved_greater_than_is_operator_p 
	  = parser->greater_than_is_operator_p;
	parser->greater_than_is_operator_p = true;
	/* If we see `( { ' then we are looking at the beginning of
	   a GNU statement-expression.  */
	if (cp_parser_allow_gnu_extensions_p (parser)
	    && cp_lexer_next_token_is (parser->lexer, CPP_OPEN_BRACE))
	  {
	    /* Statement-expressions are not allowed by the standard.  */
	    if (pedantic)
	      pedwarn ("ISO C++ forbids braced-groups within expressions");  
	    
	    /* And they're not allowed outside of a function-body; you
	       cannot, for example, write:
	       
	         int i = ({ int j = 3; j + 1; });
	       
	       at class or namespace scope.  */
	    if (!at_function_scope_p ())
	      error ("statement-expressions are allowed only inside functions");
	    /* Start the statement-expression.  */
	    expr = begin_stmt_expr ();
	    /* Parse the compound-statement.  */
	    cp_parser_compound_statement (parser, true);
	    /* Finish up.  */
	    expr = finish_stmt_expr (expr, false);
	  }
	else
	  {
	    /* Parse the parenthesized expression.  */
	    expr = cp_parser_expression (parser);
	    /* Let the front end know that this expression was
	       enclosed in parentheses. This matters in case, for
	       example, the expression is of the form `A::B', since
	       `&A::B' might be a pointer-to-member, but `&(A::B)' is
	       not.  */
	    finish_parenthesized_expr (expr);
	  }
	/* The `>' token might be the end of a template-id or
	   template-parameter-list now.  */
	parser->greater_than_is_operator_p 
	  = saved_greater_than_is_operator_p;
	/* Consume the `)'.  */
	if (!cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'"))
	  cp_parser_skip_to_end_of_statement (parser);

	return expr;
      }

    case CPP_KEYWORD:
      switch (token->keyword)
	{
	  /* These two are the boolean literals.  */
	case RID_TRUE:
	  cp_lexer_consume_token (parser->lexer);
	  return boolean_true_node;
	case RID_FALSE:
	  cp_lexer_consume_token (parser->lexer);
	  return boolean_false_node;
	  
	  /* The `__null' literal.  */
	case RID_NULL:
	  cp_lexer_consume_token (parser->lexer);
	  return null_node;

	  /* Recognize the `this' keyword.  */
	case RID_THIS:
	  cp_lexer_consume_token (parser->lexer);
	  if (parser->local_variables_forbidden_p)
	    {
	      error ("`this' may not be used in this context");
	      return error_mark_node;
	    }
	  /* Pointers cannot appear in constant-expressions.  */
	  if (cp_parser_non_integral_constant_expression (parser,
							  "`this'"))
	    return error_mark_node;
	  return finish_this_expr ();

	  /* The `operator' keyword can be the beginning of an
	     id-expression.  */
	case RID_OPERATOR:
	  goto id_expression;

	case RID_FUNCTION_NAME:
	case RID_PRETTY_FUNCTION_NAME:
	case RID_C99_FUNCTION_NAME:
	  /* The symbols __FUNCTION__, __PRETTY_FUNCTION__, and
	     __func__ are the names of variables -- but they are
	     treated specially.  Therefore, they are handled here,
	     rather than relying on the generic id-expression logic
	     below.  Grammatically, these names are id-expressions.  

	     Consume the token.  */
	  token = cp_lexer_consume_token (parser->lexer);
	  /* Look up the name.  */
	  return finish_fname (token->value);

	case RID_VA_ARG:
	  {
	    tree expression;
	    tree type;

	    /* The `__builtin_va_arg' construct is used to handle
	       `va_arg'.  Consume the `__builtin_va_arg' token.  */
	    cp_lexer_consume_token (parser->lexer);
	    /* Look for the opening `('.  */
	    cp_parser_require (parser, CPP_OPEN_PAREN, "`('");
	    /* Now, parse the assignment-expression.  */
	    expression = cp_parser_assignment_expression (parser);
	    /* Look for the `,'.  */
	    cp_parser_require (parser, CPP_COMMA, "`,'");
	    /* Parse the type-id.  */
	    type = cp_parser_type_id (parser);
	    /* Look for the closing `)'.  */
	    cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");
	    /* Using `va_arg' in a constant-expression is not
	       allowed.  */
	    if (cp_parser_non_integral_constant_expression (parser,
							    "`va_arg'"))
	      return error_mark_node;
	    return build_x_va_arg (expression, type);
	  }

	case RID_OFFSETOF:
	  {
	    tree expression;
	    bool saved_in_offsetof_p;

	    /* Consume the "__offsetof__" token.  */
	    cp_lexer_consume_token (parser->lexer);
	    /* Consume the opening `('.  */
	    cp_parser_require (parser, CPP_OPEN_PAREN, "`('");
	    /* Parse the parenthesized (almost) constant-expression.  */
	    saved_in_offsetof_p = parser->in_offsetof_p;
	    parser->in_offsetof_p = true;
	    expression 
	      = cp_parser_constant_expression (parser,
					       /*allow_non_constant_p=*/false,
					       /*non_constant_p=*/NULL);
	    parser->in_offsetof_p = saved_in_offsetof_p;
	    /* Consume the closing ')'.  */
	    cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");

	    return expression;
	  }

	default:
	  cp_parser_error (parser, "expected primary-expression");
	  return error_mark_node;
	}

      /* An id-expression can start with either an identifier, a
	 `::' as the beginning of a qualified-id, or the "operator"
	 keyword.  */
    case CPP_NAME:
    case CPP_SCOPE:
    case CPP_TEMPLATE_ID:
    case CPP_NESTED_NAME_SPECIFIER:
      {
	tree id_expression;
	tree decl;
	const char *error_msg;

      id_expression:
	/* Parse the id-expression.  */
	id_expression 
	  = cp_parser_id_expression (parser, 
				     /*template_keyword_p=*/false,
				     /*check_dependency_p=*/true,
				     /*template_p=*/NULL,
				     /*declarator_p=*/false);
	if (id_expression == error_mark_node)
	  return error_mark_node;
	/* If we have a template-id, then no further lookup is
	   required.  If the template-id was for a template-class, we
	   will sometimes have a TYPE_DECL at this point.  */
	else if (TREE_CODE (id_expression) == TEMPLATE_ID_EXPR
	    || TREE_CODE (id_expression) == TYPE_DECL)
	  decl = id_expression;
	/* Look up the name.  */
	else 
	  {
	    decl = cp_parser_lookup_name_simple (parser, id_expression);
	    /* If name lookup gives us a SCOPE_REF, then the
	       qualifying scope was dependent.  Just propagate the
	       name.  */
	    if (TREE_CODE (decl) == SCOPE_REF)
	      {
		if (TYPE_P (TREE_OPERAND (decl, 0)))
		  *qualifying_class = TREE_OPERAND (decl, 0);
		return decl;
	      }
	    /* Check to see if DECL is a local variable in a context
	       where that is forbidden.  */
	    if (parser->local_variables_forbidden_p
		&& local_variable_p (decl))
	      {
		/* It might be that we only found DECL because we are
		   trying to be generous with pre-ISO scoping rules.
		   For example, consider:

		     int i;
		     void g() {
		       for (int i = 0; i < 10; ++i) {}
		       extern void f(int j = i);
		     }

		   Here, name look up will originally find the out 
		   of scope `i'.  We need to issue a warning message,
		   but then use the global `i'.  */
		decl = check_for_out_of_scope_variable (decl);
		if (local_variable_p (decl))
		  {
		    error ("local variable `%D' may not appear in this context",
			   decl);
		    return error_mark_node;
		  }
	      }
	  }
	
	decl = finish_id_expression (id_expression, decl, parser->scope, 
				     idk, qualifying_class,
				     parser->integral_constant_expression_p,
				     parser->allow_non_integral_constant_expression_p,
				     &parser->non_integral_constant_expression_p,
				     &error_msg);
	if (error_msg)
	  cp_parser_error (parser, error_msg);
	return decl;
      }

      /* Anything else is an error.  */
    default:
      cp_parser_error (parser, "expected primary-expression");
      return error_mark_node;
    }
}

/* Parse an id-expression.

   id-expression:
     unqualified-id
     qualified-id

   qualified-id:
     :: [opt] nested-name-specifier template [opt] unqualified-id
     :: identifier
     :: operator-function-id
     :: template-id

   Return a representation of the unqualified portion of the
   identifier.  Sets PARSER->SCOPE to the qualifying scope if there is
   a `::' or nested-name-specifier.

   Often, if the id-expression was a qualified-id, the caller will
   want to make a SCOPE_REF to represent the qualified-id.  This
   function does not do this in order to avoid wastefully creating
   SCOPE_REFs when they are not required.

   If TEMPLATE_KEYWORD_P is true, then we have just seen the
   `template' keyword.

   If CHECK_DEPENDENCY_P is false, then names are looked up inside
   uninstantiated templates.  

   If *TEMPLATE_P is non-NULL, it is set to true iff the
   `template' keyword is used to explicitly indicate that the entity
   named is a template.  

   If DECLARATOR_P is true, the id-expression is appearing as part of
   a declarator, rather than as part of an expression.  */

static tree
cp_parser_id_expression (cp_parser *parser,
			 bool template_keyword_p,
			 bool check_dependency_p,
			 bool *template_p,
			 bool declarator_p)
{
  bool global_scope_p;
  bool nested_name_specifier_p;

  /* Assume the `template' keyword was not used.  */
  if (template_p)
    *template_p = false;

  /* Look for the optional `::' operator.  */
  global_scope_p 
    = (cp_parser_global_scope_opt (parser, /*current_scope_valid_p=*/false) 
       != NULL_TREE);
  /* Look for the optional nested-name-specifier.  */
  nested_name_specifier_p 
    = (cp_parser_nested_name_specifier_opt (parser,
					    /*typename_keyword_p=*/false,
					    check_dependency_p,
					    /*type_p=*/false,
					    declarator_p)
       != NULL_TREE);
  /* If there is a nested-name-specifier, then we are looking at
     the first qualified-id production.  */
  if (nested_name_specifier_p)
    {
      tree saved_scope;
      tree saved_object_scope;
      tree saved_qualifying_scope;
      tree unqualified_id;
      bool is_template;

      /* See if the next token is the `template' keyword.  */
      if (!template_p)
	template_p = &is_template;
      *template_p = cp_parser_optional_template_keyword (parser);
      /* Name lookup we do during the processing of the
	 unqualified-id might obliterate SCOPE.  */
      saved_scope = parser->scope;
      saved_object_scope = parser->object_scope;
      saved_qualifying_scope = parser->qualifying_scope;
      /* Process the final unqualified-id.  */
      unqualified_id = cp_parser_unqualified_id (parser, *template_p,
						 check_dependency_p,
						 declarator_p);
      /* Restore the SAVED_SCOPE for our caller.  */
      parser->scope = saved_scope;
      parser->object_scope = saved_object_scope;
      parser->qualifying_scope = saved_qualifying_scope;

      return unqualified_id;
    }
  /* Otherwise, if we are in global scope, then we are looking at one
     of the other qualified-id productions.  */
  else if (global_scope_p)
    {
      cp_token *token;
      tree id;

      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);

      /* If it's an identifier, and the next token is not a "<", then
	 we can avoid the template-id case.  This is an optimization
	 for this common case.  */
      if (token->type == CPP_NAME 
	  && !cp_parser_nth_token_starts_template_argument_list_p 
	       (parser, 2))
	return cp_parser_identifier (parser);

      cp_parser_parse_tentatively (parser);
      /* Try a template-id.  */
      id = cp_parser_template_id (parser, 
				  /*template_keyword_p=*/false,
				  /*check_dependency_p=*/true,
				  declarator_p);
      /* If that worked, we're done.  */
      if (cp_parser_parse_definitely (parser))
	return id;

      /* Peek at the next token.  (Changes in the token buffer may
	 have invalidated the pointer obtained above.)  */
      token = cp_lexer_peek_token (parser->lexer);

      switch (token->type)
	{
	case CPP_NAME:
	  return cp_parser_identifier (parser);

	case CPP_KEYWORD:
	  if (token->keyword == RID_OPERATOR)
	    return cp_parser_operator_function_id (parser);
	  /* Fall through.  */
	  
	default:
	  cp_parser_error (parser, "expected id-expression");
	  return error_mark_node;
	}
    }
  else
    return cp_parser_unqualified_id (parser, template_keyword_p,
				     /*check_dependency_p=*/true,
				     declarator_p);
}

/* Parse an unqualified-id.

   unqualified-id:
     identifier
     operator-function-id
     conversion-function-id
     ~ class-name
     template-id

   If TEMPLATE_KEYWORD_P is TRUE, we have just seen the `template'
   keyword, in a construct like `A::template ...'.

   Returns a representation of unqualified-id.  For the `identifier'
   production, an IDENTIFIER_NODE is returned.  For the `~ class-name'
   production a BIT_NOT_EXPR is returned; the operand of the
   BIT_NOT_EXPR is an IDENTIFIER_NODE for the class-name.  For the
   other productions, see the documentation accompanying the
   corresponding parsing functions.  If CHECK_DEPENDENCY_P is false,
   names are looked up in uninstantiated templates.  If DECLARATOR_P
   is true, the unqualified-id is appearing as part of a declarator,
   rather than as part of an expression.  */

static tree
cp_parser_unqualified_id (cp_parser* parser, 
                          bool template_keyword_p,
			  bool check_dependency_p,
			  bool declarator_p)
{
  cp_token *token;

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  
  switch (token->type)
    {
    case CPP_NAME:
      {
	tree id;

	/* We don't know yet whether or not this will be a
	   template-id.  */
	cp_parser_parse_tentatively (parser);
	/* Try a template-id.  */
	id = cp_parser_template_id (parser, template_keyword_p,
				    check_dependency_p,
				    declarator_p);
	/* If it worked, we're done.  */
	if (cp_parser_parse_definitely (parser))
	  return id;
	/* Otherwise, it's an ordinary identifier.  */
	return cp_parser_identifier (parser);
      }

    case CPP_TEMPLATE_ID:
      return cp_parser_template_id (parser, template_keyword_p,
				    check_dependency_p,
				    declarator_p);

    case CPP_COMPL:
      {
	tree type_decl;
	tree qualifying_scope;
	tree object_scope;
	tree scope;
	bool done;

	/* Consume the `~' token.  */
	cp_lexer_consume_token (parser->lexer);
	/* Parse the class-name.  The standard, as written, seems to
	   say that:

	     template <typename T> struct S { ~S (); };
	     template <typename T> S<T>::~S() {}

           is invalid, since `~' must be followed by a class-name, but
	   `S<T>' is dependent, and so not known to be a class.
	   That's not right; we need to look in uninstantiated
	   templates.  A further complication arises from:

	     template <typename T> void f(T t) {
	       t.T::~T();
	     } 

	   Here, it is not possible to look up `T' in the scope of `T'
	   itself.  We must look in both the current scope, and the
	   scope of the containing complete expression.  

	   Yet another issue is:

             struct S {
               int S;
               ~S();
             };

             S::~S() {}

           The standard does not seem to say that the `S' in `~S'
	   should refer to the type `S' and not the data member
	   `S::S'.  */

	/* DR 244 says that we look up the name after the "~" in the
	   same scope as we looked up the qualifying name.  That idea
	   isn't fully worked out; it's more complicated than that.  */
	scope = parser->scope;
	object_scope = parser->object_scope;
	qualifying_scope = parser->qualifying_scope;

	/* If the name is of the form "X::~X" it's OK.  */
	if (scope && TYPE_P (scope)
	    && cp_lexer_next_token_is (parser->lexer, CPP_NAME)
	    && (cp_lexer_peek_nth_token (parser->lexer, 2)->type 
		== CPP_OPEN_PAREN)
	    && (cp_lexer_peek_token (parser->lexer)->value 
		== TYPE_IDENTIFIER (scope)))
	  {
	    cp_lexer_consume_token (parser->lexer);
	    return build_nt (BIT_NOT_EXPR, scope);
	  }

	/* If there was an explicit qualification (S::~T), first look
	   in the scope given by the qualification (i.e., S).  */
	done = false;
	type_decl = NULL_TREE;
	if (scope)
	  {
	    cp_parser_parse_tentatively (parser);
	    type_decl = cp_parser_class_name (parser, 
					      /*typename_keyword_p=*/false,
					      /*template_keyword_p=*/false,
					      /*type_p=*/false,
					      /*check_dependency=*/false,
					      /*class_head_p=*/false,
					      declarator_p);
	    if (cp_parser_parse_definitely (parser))
	      done = true;
	  }
	/* In "N::S::~S", look in "N" as well.  */
	if (!done && scope && qualifying_scope)
	  {
	    cp_parser_parse_tentatively (parser);
	    parser->scope = qualifying_scope;
	    parser->object_scope = NULL_TREE;
	    parser->qualifying_scope = NULL_TREE;
	    type_decl 
	      = cp_parser_class_name (parser, 
				      /*typename_keyword_p=*/false,
				      /*template_keyword_p=*/false,
				      /*type_p=*/false,
				      /*check_dependency=*/false,
				      /*class_head_p=*/false,
				      declarator_p);
	    if (cp_parser_parse_definitely (parser))
	      done = true;
	  }
	/* In "p->S::~T", look in the scope given by "*p" as well.  */
	else if (!done && object_scope)
	  {
	    cp_parser_parse_tentatively (parser);
	    parser->scope = object_scope;
	    parser->object_scope = NULL_TREE;
	    parser->qualifying_scope = NULL_TREE;
	    type_decl 
	      = cp_parser_class_name (parser, 
				      /*typename_keyword_p=*/false,
				      /*template_keyword_p=*/false,
				      /*type_p=*/false,
				      /*check_dependency=*/false,
				      /*class_head_p=*/false,
				      declarator_p);
	    if (cp_parser_parse_definitely (parser))
	      done = true;
	  }
	/* Look in the surrounding context.  */
	if (!done)
	  {
	    parser->scope = NULL_TREE;
	    parser->object_scope = NULL_TREE;
	    parser->qualifying_scope = NULL_TREE;
	    type_decl 
	      = cp_parser_class_name (parser, 
				      /*typename_keyword_p=*/false,
				      /*template_keyword_p=*/false,
				      /*type_p=*/false,
				      /*check_dependency=*/false,
				      /*class_head_p=*/false,
				      declarator_p);
	  }
	/* If an error occurred, assume that the name of the
	   destructor is the same as the name of the qualifying
	   class.  That allows us to keep parsing after running
	   into ill-formed destructor names.  */
	if (type_decl == error_mark_node && scope && TYPE_P (scope))
	  return build_nt (BIT_NOT_EXPR, scope);
	else if (type_decl == error_mark_node)
	  return error_mark_node;

	/* [class.dtor]

	   A typedef-name that names a class shall not be used as the
	   identifier in the declarator for a destructor declaration.  */
	if (declarator_p 
	    && !DECL_IMPLICIT_TYPEDEF_P (type_decl)
	    && !DECL_SELF_REFERENCE_P (type_decl))
	  error ("typedef-name `%D' used as destructor declarator",
		 type_decl);

	return build_nt (BIT_NOT_EXPR, TREE_TYPE (type_decl));
      }

    case CPP_KEYWORD:
      if (token->keyword == RID_OPERATOR)
	{
	  tree id;

	  /* This could be a template-id, so we try that first.  */
	  cp_parser_parse_tentatively (parser);
	  /* Try a template-id.  */
	  id = cp_parser_template_id (parser, template_keyword_p,
				      /*check_dependency_p=*/true,
				      declarator_p);
	  /* If that worked, we're done.  */
	  if (cp_parser_parse_definitely (parser))
	    return id;
	  /* We still don't know whether we're looking at an
	     operator-function-id or a conversion-function-id.  */
	  cp_parser_parse_tentatively (parser);
	  /* Try an operator-function-id.  */
	  id = cp_parser_operator_function_id (parser);
	  /* If that didn't work, try a conversion-function-id.  */
	  if (!cp_parser_parse_definitely (parser))
	    id = cp_parser_conversion_function_id (parser);

	  return id;
	}
      /* Fall through.  */

    default:
      cp_parser_error (parser, "expected unqualified-id");
      return error_mark_node;
    }
}

/* Parse an (optional) nested-name-specifier.

   nested-name-specifier:
     class-or-namespace-name :: nested-name-specifier [opt]
     class-or-namespace-name :: template nested-name-specifier [opt]

   PARSER->SCOPE should be set appropriately before this function is
   called.  TYPENAME_KEYWORD_P is TRUE if the `typename' keyword is in
   effect.  TYPE_P is TRUE if we non-type bindings should be ignored
   in name lookups.

   Sets PARSER->SCOPE to the class (TYPE) or namespace
   (NAMESPACE_DECL) specified by the nested-name-specifier, or leaves
   it unchanged if there is no nested-name-specifier.  Returns the new
   scope iff there is a nested-name-specifier, or NULL_TREE otherwise.  

   If IS_DECLARATION is TRUE, the nested-name-specifier is known to be
   part of a declaration and/or decl-specifier.  */

static tree
cp_parser_nested_name_specifier_opt (cp_parser *parser, 
				     bool typename_keyword_p, 
				     bool check_dependency_p,
				     bool type_p,
				     bool is_declaration)
{
  bool success = false;
  tree access_check = NULL_TREE;
  ptrdiff_t start;
  cp_token* token;

  /* If the next token corresponds to a nested name specifier, there
     is no need to reparse it.  However, if CHECK_DEPENDENCY_P is
     false, it may have been true before, in which case something 
     like `A<X>::B<Y>::C' may have resulted in a nested-name-specifier
     of `A<X>::', where it should now be `A<X>::B<Y>::'.  So, when
     CHECK_DEPENDENCY_P is false, we have to fall through into the
     main loop.  */
  if (check_dependency_p
      && cp_lexer_next_token_is (parser->lexer, CPP_NESTED_NAME_SPECIFIER))
    {
      cp_parser_pre_parsed_nested_name_specifier (parser);
      return parser->scope;
    }

  /* Remember where the nested-name-specifier starts.  */
  if (cp_parser_parsing_tentatively (parser)
      && !cp_parser_committed_to_tentative_parse (parser))
    {
      token = cp_lexer_peek_token (parser->lexer);
      start = cp_lexer_token_difference (parser->lexer,
					 parser->lexer->first_token,
					 token);
    }
  else
    start = -1;

  push_deferring_access_checks (dk_deferred);

  while (true)
    {
      tree new_scope;
      tree old_scope;
      tree saved_qualifying_scope;
      bool template_keyword_p;

      /* Spot cases that cannot be the beginning of a
	 nested-name-specifier.  */
      token = cp_lexer_peek_token (parser->lexer);

      /* If the next token is CPP_NESTED_NAME_SPECIFIER, just process
	 the already parsed nested-name-specifier.  */
      if (token->type == CPP_NESTED_NAME_SPECIFIER)
	{
	  /* Grab the nested-name-specifier and continue the loop.  */
	  cp_parser_pre_parsed_nested_name_specifier (parser);
	  success = true;
	  continue;
	}

      /* Spot cases that cannot be the beginning of a
	 nested-name-specifier.  On the second and subsequent times
	 through the loop, we look for the `template' keyword.  */
      if (success && token->keyword == RID_TEMPLATE)
	;
      /* A template-id can start a nested-name-specifier.  */
      else if (token->type == CPP_TEMPLATE_ID)
	;
      else
	{
	  /* If the next token is not an identifier, then it is
	     definitely not a class-or-namespace-name.  */
	  if (token->type != CPP_NAME)
	    break;
	  /* If the following token is neither a `<' (to begin a
	     template-id), nor a `::', then we are not looking at a
	     nested-name-specifier.  */
	  token = cp_lexer_peek_nth_token (parser->lexer, 2);
	  if (token->type != CPP_SCOPE
	      && !cp_parser_nth_token_starts_template_argument_list_p
		  (parser, 2))
	    break;
	}

      /* The nested-name-specifier is optional, so we parse
	 tentatively.  */
      cp_parser_parse_tentatively (parser);

      /* Look for the optional `template' keyword, if this isn't the
	 first time through the loop.  */
      if (success)
	template_keyword_p = cp_parser_optional_template_keyword (parser);
      else
	template_keyword_p = false;

      /* Save the old scope since the name lookup we are about to do
	 might destroy it.  */
      old_scope = parser->scope;
      saved_qualifying_scope = parser->qualifying_scope;
      /* In a declarator-id like "X<T>::I::Y<T>" we must be able to
	 look up names in "X<T>::I" in order to determine that "Y" is
	 a template.  So, if we have a typename at this point, we make
	 an effort to look through it.  */
      if (is_declaration 
	  && !typename_keyword_p
	  && parser->scope 
	  && TREE_CODE (parser->scope) == TYPENAME_TYPE)
	parser->scope = resolve_typename_type (parser->scope, 
					       /*only_current_p=*/false);
      /* Parse the qualifying entity.  */
      new_scope 
	= cp_parser_class_or_namespace_name (parser,
					     typename_keyword_p,
					     template_keyword_p,
					     check_dependency_p,
					     type_p,
					     is_declaration);
      /* Look for the `::' token.  */
      cp_parser_require (parser, CPP_SCOPE, "`::'");

      /* If we found what we wanted, we keep going; otherwise, we're
	 done.  */
      if (!cp_parser_parse_definitely (parser))
	{
	  bool error_p = false;

	  /* Restore the OLD_SCOPE since it was valid before the
	     failed attempt at finding the last
	     class-or-namespace-name.  */
	  parser->scope = old_scope;
	  parser->qualifying_scope = saved_qualifying_scope;
	  /* If the next token is an identifier, and the one after
	     that is a `::', then any valid interpretation would have
	     found a class-or-namespace-name.  */
	  while (cp_lexer_next_token_is (parser->lexer, CPP_NAME)
		 && (cp_lexer_peek_nth_token (parser->lexer, 2)->type 
		     == CPP_SCOPE)
		 && (cp_lexer_peek_nth_token (parser->lexer, 3)->type 
		     != CPP_COMPL))
	    {
	      token = cp_lexer_consume_token (parser->lexer);
	      if (!error_p) 
		{
		  tree decl;

		  decl = cp_parser_lookup_name_simple (parser, token->value);
		  if (TREE_CODE (decl) == TEMPLATE_DECL)
		    error ("`%D' used without template parameters",
			   decl);
		  else
		    cp_parser_name_lookup_error 
		      (parser, token->value, decl, 
		       "is not a class or namespace");
		  parser->scope = NULL_TREE;
		  error_p = true;
		  /* Treat this as a successful nested-name-specifier
		     due to:

		     [basic.lookup.qual]

		     If the name found is not a class-name (clause
		     _class_) or namespace-name (_namespace.def_), the
		     program is ill-formed.  */
		  success = true;
		}
	      cp_lexer_consume_token (parser->lexer);
	    }
	  break;
	}

      /* We've found one valid nested-name-specifier.  */
      success = true;
      /* Make sure we look in the right scope the next time through
	 the loop.  */
      parser->scope = (TREE_CODE (new_scope) == TYPE_DECL 
		       ? TREE_TYPE (new_scope)
		       : new_scope);
      /* If it is a class scope, try to complete it; we are about to
	 be looking up names inside the class.  */
      if (TYPE_P (parser->scope)
	  /* Since checking types for dependency can be expensive,
	     avoid doing it if the type is already complete.  */
	  && !COMPLETE_TYPE_P (parser->scope)
	  /* Do not try to complete dependent types.  */
	  && !dependent_type_p (parser->scope))
	complete_type (parser->scope);
    }

  /* Retrieve any deferred checks.  Do not pop this access checks yet
     so the memory will not be reclaimed during token replacing below.  */
  access_check = get_deferred_access_checks ();

  /* If parsing tentatively, replace the sequence of tokens that makes
     up the nested-name-specifier with a CPP_NESTED_NAME_SPECIFIER
     token.  That way, should we re-parse the token stream, we will
     not have to repeat the effort required to do the parse, nor will
     we issue duplicate error messages.  */
  if (success && start >= 0)
    {
      /* Find the token that corresponds to the start of the
	 template-id.  */
      token = cp_lexer_advance_token (parser->lexer, 
				      parser->lexer->first_token,
				      start);

      /* Reset the contents of the START token.  */
      token->type = CPP_NESTED_NAME_SPECIFIER;
      token->value = build_tree_list (access_check, parser->scope);
      TREE_TYPE (token->value) = parser->qualifying_scope;
      token->keyword = RID_MAX;
      /* Purge all subsequent tokens.  */
      cp_lexer_purge_tokens_after (parser->lexer, token);
    }

  pop_deferring_access_checks ();
  return success ? parser->scope : NULL_TREE;
}

/* Parse a nested-name-specifier.  See
   cp_parser_nested_name_specifier_opt for details.  This function
   behaves identically, except that it will an issue an error if no
   nested-name-specifier is present, and it will return
   ERROR_MARK_NODE, rather than NULL_TREE, if no nested-name-specifier
   is present.  */

static tree
cp_parser_nested_name_specifier (cp_parser *parser, 
				 bool typename_keyword_p, 
				 bool check_dependency_p,
				 bool type_p,
				 bool is_declaration)
{
  tree scope;

  /* Look for the nested-name-specifier.  */
  scope = cp_parser_nested_name_specifier_opt (parser,
					       typename_keyword_p,
					       check_dependency_p,
					       type_p,
					       is_declaration);
  /* If it was not present, issue an error message.  */
  if (!scope)
    {
      cp_parser_error (parser, "expected nested-name-specifier");
      parser->scope = NULL_TREE;
      return error_mark_node;
    }

  return scope;
}

/* Parse a class-or-namespace-name.

   class-or-namespace-name:
     class-name
     namespace-name

   TYPENAME_KEYWORD_P is TRUE iff the `typename' keyword is in effect.
   TEMPLATE_KEYWORD_P is TRUE iff the `template' keyword is in effect.
   CHECK_DEPENDENCY_P is FALSE iff dependent names should be looked up.
   TYPE_P is TRUE iff the next name should be taken as a class-name,
   even the same name is declared to be another entity in the same
   scope.

   Returns the class (TYPE_DECL) or namespace (NAMESPACE_DECL)
   specified by the class-or-namespace-name.  If neither is found the
   ERROR_MARK_NODE is returned.  */

static tree
cp_parser_class_or_namespace_name (cp_parser *parser, 
				   bool typename_keyword_p,
				   bool template_keyword_p,
				   bool check_dependency_p,
				   bool type_p,
				   bool is_declaration)
{
  tree saved_scope;
  tree saved_qualifying_scope;
  tree saved_object_scope;
  tree scope;
  bool only_class_p;

  /* Before we try to parse the class-name, we must save away the
     current PARSER->SCOPE since cp_parser_class_name will destroy
     it.  */
  saved_scope = parser->scope;
  saved_qualifying_scope = parser->qualifying_scope;
  saved_object_scope = parser->object_scope;
  /* Try for a class-name first.  If the SAVED_SCOPE is a type, then
     there is no need to look for a namespace-name.  */
  only_class_p = template_keyword_p || (saved_scope && TYPE_P (saved_scope));
  if (!only_class_p)
    cp_parser_parse_tentatively (parser);
  scope = cp_parser_class_name (parser, 
				typename_keyword_p,
				template_keyword_p,
				type_p,
				check_dependency_p,
				/*class_head_p=*/false,
				is_declaration);
  /* If that didn't work, try for a namespace-name.  */
  if (!only_class_p && !cp_parser_parse_definitely (parser))
    {
      /* Restore the saved scope.  */
      parser->scope = saved_scope;
      parser->qualifying_scope = saved_qualifying_scope;
      parser->object_scope = saved_object_scope;
      /* If we are not looking at an identifier followed by the scope
	 resolution operator, then this is not part of a
	 nested-name-specifier.  (Note that this function is only used
	 to parse the components of a nested-name-specifier.)  */
      if (cp_lexer_next_token_is_not (parser->lexer, CPP_NAME)
	  || cp_lexer_peek_nth_token (parser->lexer, 2)->type != CPP_SCOPE)
	return error_mark_node;
      scope = cp_parser_namespace_name (parser);
    }

  return scope;
}

/* Parse a postfix-expression.

   postfix-expression:
     primary-expression
     postfix-expression [ expression ]
     postfix-expression ( expression-list [opt] )
     simple-type-specifier ( expression-list [opt] )
     typename :: [opt] nested-name-specifier identifier 
       ( expression-list [opt] )
     typename :: [opt] nested-name-specifier template [opt] template-id
       ( expression-list [opt] )
     postfix-expression . template [opt] id-expression
     postfix-expression -> template [opt] id-expression
     postfix-expression . pseudo-destructor-name
     postfix-expression -> pseudo-destructor-name
     postfix-expression ++
     postfix-expression --
     dynamic_cast < type-id > ( expression )
     static_cast < type-id > ( expression )
     reinterpret_cast < type-id > ( expression )
     const_cast < type-id > ( expression )
     typeid ( expression )
     typeid ( type-id )

   GNU Extension:
     
   postfix-expression:
     ( type-id ) { initializer-list , [opt] }

   This extension is a GNU version of the C99 compound-literal
   construct.  (The C99 grammar uses `type-name' instead of `type-id',
   but they are essentially the same concept.)

   If ADDRESS_P is true, the postfix expression is the operand of the
   `&' operator.

   Returns a representation of the expression.  */

static tree
cp_parser_postfix_expression (cp_parser *parser, bool address_p)
{
  cp_token *token;
  enum rid keyword;
  cp_id_kind idk = CP_ID_KIND_NONE;
  tree postfix_expression = NULL_TREE;
  /* Non-NULL only if the current postfix-expression can be used to
     form a pointer-to-member.  In that case, QUALIFYING_CLASS is the
     class used to qualify the member.  */
  tree qualifying_class = NULL_TREE;

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* Some of the productions are determined by keywords.  */
  keyword = token->keyword;
  switch (keyword)
    {
    case RID_DYNCAST:
    case RID_STATCAST:
    case RID_REINTCAST:
    case RID_CONSTCAST:
      {
	tree type;
	tree expression;
	const char *saved_message;

	/* All of these can be handled in the same way from the point
	   of view of parsing.  Begin by consuming the token
	   identifying the cast.  */
	cp_lexer_consume_token (parser->lexer);
	
	/* New types cannot be defined in the cast.  */
	saved_message = parser->type_definition_forbidden_message;
	parser->type_definition_forbidden_message
	  = "types may not be defined in casts";

	/* Look for the opening `<'.  */
	cp_parser_require (parser, CPP_LESS, "`<'");
	/* Parse the type to which we are casting.  */
	type = cp_parser_type_id (parser);
	/* Look for the closing `>'.  */
	cp_parser_require (parser, CPP_GREATER, "`>'");
	/* Restore the old message.  */
	parser->type_definition_forbidden_message = saved_message;

	/* And the expression which is being cast.  */
	cp_parser_require (parser, CPP_OPEN_PAREN, "`('");
	expression = cp_parser_expression (parser);
	cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");

	/* Only type conversions to integral or enumeration types
	   can be used in constant-expressions.  */
	if (parser->integral_constant_expression_p
	    && !dependent_type_p (type)
	    && !INTEGRAL_OR_ENUMERATION_TYPE_P (type)
	    /* A cast to pointer or reference type is allowed in the
	       implementation of "offsetof".  */
	    && !(parser->in_offsetof_p && POINTER_TYPE_P (type))
	    && (cp_parser_non_integral_constant_expression 
		(parser,
		 "a cast to a type other than an integral or "
		 "enumeration type")))
	  return error_mark_node;

	switch (keyword)
	  {
	  case RID_DYNCAST:
	    postfix_expression
	      = build_dynamic_cast (type, expression);
	    break;
	  case RID_STATCAST:
	    postfix_expression
	      = build_static_cast (type, expression);
	    break;
	  case RID_REINTCAST:
	    postfix_expression
	      = build_reinterpret_cast (type, expression);
	    break;
	  case RID_CONSTCAST:
	    postfix_expression
	      = build_const_cast (type, expression);
	    break;
	  default:
	    abort ();
	  }
      }
      break;

    case RID_TYPEID:
      {
	tree type;
	const char *saved_message;
	bool saved_in_type_id_in_expr_p;

	/* Consume the `typeid' token.  */
	cp_lexer_consume_token (parser->lexer);
	/* Look for the `(' token.  */
	cp_parser_require (parser, CPP_OPEN_PAREN, "`('");
	/* Types cannot be defined in a `typeid' expression.  */
	saved_message = parser->type_definition_forbidden_message;
	parser->type_definition_forbidden_message
	  = "types may not be defined in a `typeid\' expression";
	/* We can't be sure yet whether we're looking at a type-id or an
	   expression.  */
	cp_parser_parse_tentatively (parser);
	/* Try a type-id first.  */
	saved_in_type_id_in_expr_p = parser->in_type_id_in_expr_p;
	parser->in_type_id_in_expr_p = true;
	type = cp_parser_type_id (parser);
	parser->in_type_id_in_expr_p = saved_in_type_id_in_expr_p;
	/* Look for the `)' token.  Otherwise, we can't be sure that
	   we're not looking at an expression: consider `typeid (int
	   (3))', for example.  */
	cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");
	/* If all went well, simply lookup the type-id.  */
	if (cp_parser_parse_definitely (parser))
	  postfix_expression = get_typeid (type);
	/* Otherwise, fall back to the expression variant.  */
	else
	  {
	    tree expression;

	    /* Look for an expression.  */
	    expression = cp_parser_expression (parser);
	    /* Compute its typeid.  */
	    postfix_expression = build_typeid (expression);
	    /* Look for the `)' token.  */
	    cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");
	  }
	/* `typeid' may not appear in an integral constant expression.  */
	if (cp_parser_non_integral_constant_expression(parser, 
						       "`typeid' operator"))
	  return error_mark_node;
	/* Restore the saved message.  */
	parser->type_definition_forbidden_message = saved_message;
      }
      break;
      
    case RID_TYPENAME:
      {
	tree type;

	/* The syntax permitted here is the same permitted for an
	   elaborated-type-specifier.  */
	type = cp_parser_elaborated_type_specifier (parser,
						    /*is_friend=*/false,
						    /*is_declaration=*/false);
	postfix_expression = cp_parser_functional_cast (parser, type);
      }
      break;

    default:
      {
	tree type;

	/* If the next thing is a simple-type-specifier, we may be
	   looking at a functional cast.  We could also be looking at
	   an id-expression.  So, we try the functional cast, and if
	   that doesn't work we fall back to the primary-expression.  */
	cp_parser_parse_tentatively (parser);
	/* Look for the simple-type-specifier.  */
	type = cp_parser_simple_type_specifier (parser, 
						CP_PARSER_FLAGS_NONE,
						/*identifier_p=*/false);
	/* Parse the cast itself.  */
	if (!cp_parser_error_occurred (parser))
	  postfix_expression 
	    = cp_parser_functional_cast (parser, type);
	/* If that worked, we're done.  */
	if (cp_parser_parse_definitely (parser))
	  break;

	/* If the functional-cast didn't work out, try a
	   compound-literal.  */
	if (cp_parser_allow_gnu_extensions_p (parser)
	    && cp_lexer_next_token_is (parser->lexer, CPP_OPEN_PAREN))
	  {
	    tree initializer_list = NULL_TREE;
	    bool saved_in_type_id_in_expr_p;

	    cp_parser_parse_tentatively (parser);
	    /* Consume the `('.  */
	    cp_lexer_consume_token (parser->lexer);
	    /* Parse the type.  */
	    saved_in_type_id_in_expr_p = parser->in_type_id_in_expr_p;
	    parser->in_type_id_in_expr_p = true;
	    type = cp_parser_type_id (parser);
	    parser->in_type_id_in_expr_p = saved_in_type_id_in_expr_p;
	    /* Look for the `)'.  */
	    cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");
	    /* Look for the `{'.  */
	    cp_parser_require (parser, CPP_OPEN_BRACE, "`{'");
	    /* If things aren't going well, there's no need to
	       keep going.  */
	    if (!cp_parser_error_occurred (parser))
	      {
		bool non_constant_p;
		/* Parse the initializer-list.  */
		initializer_list 
		  = cp_parser_initializer_list (parser, &non_constant_p);
		/* Allow a trailing `,'.  */
		if (cp_lexer_next_token_is (parser->lexer, CPP_COMMA))
		  cp_lexer_consume_token (parser->lexer);
		/* Look for the final `}'.  */
		cp_parser_require (parser, CPP_CLOSE_BRACE, "`}'");
	      }
	    /* If that worked, we're definitely looking at a
	       compound-literal expression.  */
	    if (cp_parser_parse_definitely (parser))
	      {
		/* Warn the user that a compound literal is not
		   allowed in standard C++.  */
		if (pedantic)
		  pedwarn ("ISO C++ forbids compound-literals");
		/* Form the representation of the compound-literal.  */
		postfix_expression 
		  = finish_compound_literal (type, initializer_list);
		break;
	      }
	  }

	/* It must be a primary-expression.  */
	postfix_expression = cp_parser_primary_expression (parser, 
							   &idk,
							   &qualifying_class);
      }
      break;
    }

  /* If we were avoiding committing to the processing of a
     qualified-id until we knew whether or not we had a
     pointer-to-member, we now know.  */
  if (qualifying_class)
    {
      bool done;

      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);
      done = (token->type != CPP_OPEN_SQUARE
	      && token->type != CPP_OPEN_PAREN
	      && token->type != CPP_DOT
	      && token->type != CPP_DEREF
	      && token->type != CPP_PLUS_PLUS
	      && token->type != CPP_MINUS_MINUS);

      postfix_expression = finish_qualified_id_expr (qualifying_class,
						     postfix_expression,
						     done,
						     address_p);
      if (done)
	return postfix_expression;
    }

  /* Keep looping until the postfix-expression is complete.  */
  while (true)
    {
      if (idk == CP_ID_KIND_UNQUALIFIED
	  && TREE_CODE (postfix_expression) == IDENTIFIER_NODE
	  && cp_lexer_next_token_is_not (parser->lexer, CPP_OPEN_PAREN))
	/* It is not a Koenig lookup function call.  */
	postfix_expression 
	  = unqualified_name_lookup_error (postfix_expression);
      
      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);

      switch (token->type)
	{
	case CPP_OPEN_SQUARE:
	  /* postfix-expression [ expression ] */
	  {
	    tree index;

	    /* Consume the `[' token.  */
	    cp_lexer_consume_token (parser->lexer);
	    /* Parse the index expression.  */
	    index = cp_parser_expression (parser);
	    /* Look for the closing `]'.  */
	    cp_parser_require (parser, CPP_CLOSE_SQUARE, "`]'");

	    /* Build the ARRAY_REF.  */
	    postfix_expression 
	      = grok_array_decl (postfix_expression, index);
	    idk = CP_ID_KIND_NONE;
	    /* Array references are not permitted in
	       constant-expressions (but they are allowed
	       in offsetof).  */
	    if (!parser->in_offsetof_p
		&& cp_parser_non_integral_constant_expression
		    (parser, "an array reference"))
	      postfix_expression = error_mark_node;
	  }
	  break;

	case CPP_OPEN_PAREN:
	  /* postfix-expression ( expression-list [opt] ) */
	  {
	    bool koenig_p;
	    tree args = (cp_parser_parenthesized_expression_list 
			 (parser, false, /*non_constant_p=*/NULL));

	    if (args == error_mark_node)
	      {
		postfix_expression = error_mark_node;
		break;
	      }
	    
	    /* Function calls are not permitted in
	       constant-expressions.  */
	    if (cp_parser_non_integral_constant_expression (parser,
							    "a function call"))
	      {
		postfix_expression = error_mark_node;
		break;
	      }

	    koenig_p = false;
	    if (idk == CP_ID_KIND_UNQUALIFIED)
	      {
		if (TREE_CODE (postfix_expression) == IDENTIFIER_NODE)
		  {
		    if (args)
		      {
			koenig_p = true;
			postfix_expression
			  = perform_koenig_lookup (postfix_expression, args);
		      }
		    else
		      postfix_expression
			= unqualified_fn_lookup_error (postfix_expression);
 		  }
		/* We do not perform argument-dependent lookup if
		   normal lookup finds a non-function, in accordance
		   with the expected resolution of DR 218.  */
		else if (args && is_overloaded_fn (postfix_expression))
		  {
		    tree fn = get_first_fn (postfix_expression);
		    if (TREE_CODE (fn) == TEMPLATE_ID_EXPR)
		      fn = OVL_CURRENT (TREE_OPERAND (fn, 0));
		    /* Only do argument dependent lookup if regular
		       lookup does not find a set of member functions.
		       [basic.lookup.koenig]/2a  */
 		    if (!DECL_FUNCTION_MEMBER_P (fn))
 		      {
 			koenig_p = true;
 			postfix_expression
 			  = perform_koenig_lookup (postfix_expression, args);
 		      }
		  }
	      }
	  
	    if (TREE_CODE (postfix_expression) == COMPONENT_REF)
	      {
		tree instance = TREE_OPERAND (postfix_expression, 0);
		tree fn = TREE_OPERAND (postfix_expression, 1);

		if (processing_template_decl
		    && (type_dependent_expression_p (instance)
			|| (!BASELINK_P (fn)
			    && TREE_CODE (fn) != FIELD_DECL)
			|| type_dependent_expression_p (fn)
			|| any_type_dependent_arguments_p (args)))
		  {
		    postfix_expression
		      = build_min_nt (CALL_EXPR, postfix_expression, args);
		    break;
		  }

		if (BASELINK_P (fn))
		  postfix_expression
		    = (build_new_method_call 
		       (instance, fn, args, NULL_TREE, 
			(idk == CP_ID_KIND_QUALIFIED 
			 ? LOOKUP_NONVIRTUAL : LOOKUP_NORMAL)));
		else
		  postfix_expression
		    = finish_call_expr (postfix_expression, args,
					/*disallow_virtual=*/false,
					/*koenig_p=*/false);
	      }
	    else if (TREE_CODE (postfix_expression) == OFFSET_REF
		     || TREE_CODE (postfix_expression) == MEMBER_REF
		     || TREE_CODE (postfix_expression) == DOTSTAR_EXPR)
	      postfix_expression = (build_offset_ref_call_from_tree
				    (postfix_expression, args));
	    else if (idk == CP_ID_KIND_QUALIFIED)
	      /* A call to a static class member, or a namespace-scope
		 function.  */
	      postfix_expression
		= finish_call_expr (postfix_expression, args,
				    /*disallow_virtual=*/true,
				    koenig_p);
	    else
	      /* All other function calls.  */
	      postfix_expression 
		= finish_call_expr (postfix_expression, args, 
				    /*disallow_virtual=*/false,
				    koenig_p);

	    /* The POSTFIX_EXPRESSION is certainly no longer an id.  */
	    idk = CP_ID_KIND_NONE;
	  }
	  break;
	  
	case CPP_DOT:
	case CPP_DEREF:
	  /* postfix-expression . template [opt] id-expression  
	     postfix-expression . pseudo-destructor-name 
	     postfix-expression -> template [opt] id-expression
	     postfix-expression -> pseudo-destructor-name */
	  {
	    tree name;
	    bool dependent_p;
	    bool template_p;
	    bool pseudo_destructor_p;
	    tree scope = NULL_TREE;
	    enum cpp_ttype token_type = token->type;

	    /* If this is a `->' operator, dereference the pointer.  */
	    if (token->type == CPP_DEREF)
	      postfix_expression = build_x_arrow (postfix_expression);
	    /* Check to see whether or not the expression is
	       type-dependent.  */
	    dependent_p = type_dependent_expression_p (postfix_expression);
	    /* The identifier following the `->' or `.' is not
	       qualified.  */
	    parser->scope = NULL_TREE;
	    parser->qualifying_scope = NULL_TREE;
	    parser->object_scope = NULL_TREE;
	    idk = CP_ID_KIND_NONE;
	    /* Enter the scope corresponding to the type of the object
	       given by the POSTFIX_EXPRESSION.  */
	    if (!dependent_p 
		&& TREE_TYPE (postfix_expression) != NULL_TREE)
	      {
		scope = TREE_TYPE (postfix_expression);
		/* According to the standard, no expression should
		   ever have reference type.  Unfortunately, we do not
		   currently match the standard in this respect in
		   that our internal representation of an expression
		   may have reference type even when the standard says
		   it does not.  Therefore, we have to manually obtain
		   the underlying type here.  */
		scope = non_reference (scope);
		/* The type of the POSTFIX_EXPRESSION must be
		   complete.  */
		scope = complete_type_or_else (scope, NULL_TREE);
		/* Let the name lookup machinery know that we are
		   processing a class member access expression.  */
		parser->context->object_type = scope;
		/* If something went wrong, we want to be able to
		   discern that case, as opposed to the case where
		   there was no SCOPE due to the type of expression
		   being dependent.  */
		if (!scope)
		  scope = error_mark_node;
		/* If the SCOPE was erroneous, make the various
		   semantic analysis functions exit quickly -- and
		   without issuing additional error messages.  */
		if (scope == error_mark_node)
		  postfix_expression = error_mark_node;
	      }

	    /* Consume the `.' or `->' operator.  */
	    cp_lexer_consume_token (parser->lexer);
	    
	    /* Assume this expression is not a pseudo-destructor access.  */
	    pseudo_destructor_p = false;

	    /* If the SCOPE is a scalar type, then, if this is a valid program,
	       we must be looking at a pseudo-destructor-name.  */
	    if (scope && SCALAR_TYPE_P (scope))
	      {
		tree s = NULL_TREE;
		tree type;

		cp_parser_parse_tentatively (parser);
		/* Parse the pseudo-destructor-name.  */
		cp_parser_pseudo_destructor_name (parser, &s, &type);
		if (cp_parser_parse_definitely (parser))
		  {
		    pseudo_destructor_p = true;
		    postfix_expression
		      = finish_pseudo_destructor_expr (postfix_expression,
						       s, TREE_TYPE (type));
		  }
	      }

	    if (!pseudo_destructor_p)
	      {
		/* If the SCOPE is not a scalar type, we are looking
		   at an ordinary class member access expression,
		   rather than a pseudo-destructor-name.  */
		template_p = cp_parser_optional_template_keyword (parser);
		/* Parse the id-expression.  */
		name = cp_parser_id_expression (parser,
						template_p,
						/*check_dependency_p=*/true,
						/*template_p=*/NULL,
						/*declarator_p=*/false);
		/* In general, build a SCOPE_REF if the member name is
		   qualified.  However, if the name was not dependent
		   and has already been resolved; there is no need to
		   build the SCOPE_REF.  For example;

                     struct X { void f(); };
                     template <typename T> void f(T* t) { t->X::f(); }
 
                   Even though "t" is dependent, "X::f" is not and has
		   been resolved to a BASELINK; there is no need to
		   include scope information.  */

		/* But we do need to remember that there was an explicit
		   scope for virtual function calls.  */
		if (parser->scope)
		  idk = CP_ID_KIND_QUALIFIED;

		/* If the name is a template-id that names a type, we will
		   get a TYPE_DECL here.  That is invalid code.  */
		if (TREE_CODE (name) == TYPE_DECL)
		  {
		    error ("invalid use of `%D'", name);
		    postfix_expression = error_mark_node;
		  }
		else
		  {
		    if (name != error_mark_node && !BASELINK_P (name)
			&& parser->scope)
		      {
			name = build_nt (SCOPE_REF, parser->scope, name);
			parser->scope = NULL_TREE;
			parser->qualifying_scope = NULL_TREE;
			parser->object_scope = NULL_TREE;
		      }
		    if (scope && name && BASELINK_P (name))
		      adjust_result_of_qualified_name_lookup
			(name, BINFO_TYPE (BASELINK_BINFO (name)), scope);
		    postfix_expression = finish_class_member_access_expr
					   (postfix_expression, name);
		  }
	      }

	    /* We no longer need to look up names in the scope of the
	       object on the left-hand side of the `.' or `->'
	       operator.  */
	    parser->context->object_type = NULL_TREE;
	    /* These operators may not appear in constant-expressions.  */
	    if (/* The "->" operator is allowed in the implementation
		   of "offsetof".  The "." operator may appear in the
		   name of the member.  */
		!parser->in_offsetof_p
		&& (cp_parser_non_integral_constant_expression 
		    (parser,
		     token_type == CPP_DEREF ? "'->'" : "`.'")))
	      postfix_expression = error_mark_node;
	  }
	  break;

	case CPP_PLUS_PLUS:
	  /* postfix-expression ++  */
	  /* Consume the `++' token.  */
	  cp_lexer_consume_token (parser->lexer);
	  /* Generate a representation for the complete expression.  */
	  postfix_expression 
	    = finish_increment_expr (postfix_expression, 
				     POSTINCREMENT_EXPR);
	  /* Increments may not appear in constant-expressions.  */
	  if (cp_parser_non_integral_constant_expression (parser,
							  "an increment"))
	    postfix_expression = error_mark_node;
	  idk = CP_ID_KIND_NONE;
	  break;

	case CPP_MINUS_MINUS:
	  /* postfix-expression -- */
	  /* Consume the `--' token.  */
	  cp_lexer_consume_token (parser->lexer);
	  /* Generate a representation for the complete expression.  */
	  postfix_expression 
	    = finish_increment_expr (postfix_expression, 
				     POSTDECREMENT_EXPR);
	  /* Decrements may not appear in constant-expressions.  */
	  if (cp_parser_non_integral_constant_expression (parser,
							  "a decrement"))
	    postfix_expression = error_mark_node;
	  idk = CP_ID_KIND_NONE;
	  break;

	default:
	  return postfix_expression;
	}
    }

  /* We should never get here.  */
  abort ();
  return error_mark_node;
}

/* Parse a parenthesized expression-list.

   expression-list:
     assignment-expression
     expression-list, assignment-expression

   attribute-list:
     expression-list
     identifier
     identifier, expression-list

   Returns a TREE_LIST.  The TREE_VALUE of each node is a
   representation of an assignment-expression.  Note that a TREE_LIST
   is returned even if there is only a single expression in the list.
   error_mark_node is returned if the ( and or ) are
   missing. NULL_TREE is returned on no expressions. The parentheses
   are eaten. IS_ATTRIBUTE_LIST is true if this is really an attribute
   list being parsed.  If NON_CONSTANT_P is non-NULL, *NON_CONSTANT_P
   indicates whether or not all of the expressions in the list were
   constant.  */

static tree
cp_parser_parenthesized_expression_list (cp_parser* parser, 
					 bool is_attribute_list,
					 bool *non_constant_p)
{
  tree expression_list = NULL_TREE;
  tree identifier = NULL_TREE;

  /* Assume all the expressions will be constant.  */
  if (non_constant_p)
    *non_constant_p = false;

  if (!cp_parser_require (parser, CPP_OPEN_PAREN, "`('"))
    return error_mark_node;
  
  /* Consume expressions until there are no more.  */
  if (cp_lexer_next_token_is_not (parser->lexer, CPP_CLOSE_PAREN))
    while (true)
      {
	tree expr;
	
	/* At the beginning of attribute lists, check to see if the
	   next token is an identifier.  */
	if (is_attribute_list
	    && cp_lexer_peek_token (parser->lexer)->type == CPP_NAME)
	  {
	    cp_token *token;
	    
	    /* Consume the identifier.  */
	    token = cp_lexer_consume_token (parser->lexer);
	    /* Save the identifier.  */
	    identifier = token->value;
	  }
	else
	  {
	    /* Parse the next assignment-expression.  */
	    if (non_constant_p)
	      {
		bool expr_non_constant_p;
		expr = (cp_parser_constant_expression 
			(parser, /*allow_non_constant_p=*/true,
			 &expr_non_constant_p));
		if (expr_non_constant_p)
		  *non_constant_p = true;
	      }
	    else
	      expr = cp_parser_assignment_expression (parser);

	     /* Add it to the list.  We add error_mark_node
		expressions to the list, so that we can still tell if
		the correct form for a parenthesized expression-list
		is found. That gives better errors.  */
	    expression_list = tree_cons (NULL_TREE, expr, expression_list);

	    if (expr == error_mark_node)
	      goto skip_comma;
	  }

	/* After the first item, attribute lists look the same as
	   expression lists.  */
	is_attribute_list = false;
	
      get_comma:;
	/* If the next token isn't a `,', then we are done.  */
	if (cp_lexer_next_token_is_not (parser->lexer, CPP_COMMA))
	  break;

	/* Otherwise, consume the `,' and keep going.  */
	cp_lexer_consume_token (parser->lexer);
      }
  
  if (!cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'"))
    {
      int ending;
      
    skip_comma:;
      /* We try and resync to an unnested comma, as that will give the
	 user better diagnostics.  */
      ending = cp_parser_skip_to_closing_parenthesis (parser, 
						      /*recovering=*/true, 
						      /*or_comma=*/true,
						      /*consume_paren=*/true);
      if (ending < 0)
	goto get_comma;
      if (!ending)
	return error_mark_node;
    }

  /* We built up the list in reverse order so we must reverse it now.  */
  expression_list = nreverse (expression_list);
  if (identifier)
    expression_list = tree_cons (NULL_TREE, identifier, expression_list);
  
  return expression_list;
}

/* Parse a pseudo-destructor-name.

   pseudo-destructor-name:
     :: [opt] nested-name-specifier [opt] type-name :: ~ type-name
     :: [opt] nested-name-specifier template template-id :: ~ type-name
     :: [opt] nested-name-specifier [opt] ~ type-name

   If either of the first two productions is used, sets *SCOPE to the
   TYPE specified before the final `::'.  Otherwise, *SCOPE is set to
   NULL_TREE.  *TYPE is set to the TYPE_DECL for the final type-name,
   or ERROR_MARK_NODE if the parse fails.  */

static void
cp_parser_pseudo_destructor_name (cp_parser* parser, 
                                  tree* scope, 
                                  tree* type)
{
  bool nested_name_specifier_p;

  /* Look for the optional `::' operator.  */
  cp_parser_global_scope_opt (parser, /*current_scope_valid_p=*/true);
  /* Look for the optional nested-name-specifier.  */
  nested_name_specifier_p 
    = (cp_parser_nested_name_specifier_opt (parser,
					    /*typename_keyword_p=*/false,
					    /*check_dependency_p=*/true,
					    /*type_p=*/false,
					    /*is_declaration=*/true) 
       != NULL_TREE);
  /* Now, if we saw a nested-name-specifier, we might be doing the
     second production.  */
  if (nested_name_specifier_p 
      && cp_lexer_next_token_is_keyword (parser->lexer, RID_TEMPLATE))
    {
      /* Consume the `template' keyword.  */
      cp_lexer_consume_token (parser->lexer);
      /* Parse the template-id.  */
      cp_parser_template_id (parser, 
			     /*template_keyword_p=*/true,
			     /*check_dependency_p=*/false,
			     /*is_declaration=*/true);
      /* Look for the `::' token.  */
      cp_parser_require (parser, CPP_SCOPE, "`::'");
    }
  /* If the next token is not a `~', then there might be some
     additional qualification.  */
  else if (cp_lexer_next_token_is_not (parser->lexer, CPP_COMPL))
    {
      /* Look for the type-name.  */
      *scope = TREE_TYPE (cp_parser_type_name (parser));

      /* If we didn't get an aggregate type, or we don't have ::~,
	 then something has gone wrong.  Since the only caller of this
	 function is looking for something after `.' or `->' after a
	 scalar type, most likely the program is trying to get a
	 member of a non-aggregate type.  */
      if (*scope == error_mark_node
	  || cp_lexer_next_token_is_not (parser->lexer, CPP_SCOPE)
	  || cp_lexer_peek_nth_token (parser->lexer, 2)->type != CPP_COMPL)
	{
	  cp_parser_error (parser, "request for member of non-aggregate type");
	  *type = error_mark_node;
	  return;
	}

      /* Look for the `::' token.  */
      cp_parser_require (parser, CPP_SCOPE, "`::'");
    }
  else
    *scope = NULL_TREE;

  /* Look for the `~'.  */
  cp_parser_require (parser, CPP_COMPL, "`~'");
  /* Look for the type-name again.  We are not responsible for
     checking that it matches the first type-name.  */
  *type = cp_parser_type_name (parser);
}

/* Parse a unary-expression.

   unary-expression:
     postfix-expression
     ++ cast-expression
     -- cast-expression
     unary-operator cast-expression
     sizeof unary-expression
     sizeof ( type-id )
     new-expression
     delete-expression

   GNU Extensions:

   unary-expression:
     __extension__ cast-expression
     __alignof__ unary-expression
     __alignof__ ( type-id )
     __real__ cast-expression
     __imag__ cast-expression
     && identifier

   ADDRESS_P is true iff the unary-expression is appearing as the
   operand of the `&' operator.

   Returns a representation of the expression.  */

static tree
cp_parser_unary_expression (cp_parser *parser, bool address_p)
{
  cp_token *token;
  enum tree_code unary_operator;

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* Some keywords give away the kind of expression.  */
  if (token->type == CPP_KEYWORD)
    {
      enum rid keyword = token->keyword;

      switch (keyword)
	{
	case RID_ALIGNOF:
	case RID_SIZEOF:
	  {
	    tree operand;
	    enum tree_code op;
	    
	    op = keyword == RID_ALIGNOF ? ALIGNOF_EXPR : SIZEOF_EXPR;
	    /* Consume the token.  */
	    cp_lexer_consume_token (parser->lexer);
	    /* Parse the operand.  */
	    operand = cp_parser_sizeof_operand (parser, keyword);

	    if (TYPE_P (operand))
	      return cxx_sizeof_or_alignof_type (operand, op, true);
	    else
	      return cxx_sizeof_or_alignof_expr (operand, op);
	  }

	case RID_NEW:
	  return cp_parser_new_expression (parser);

	case RID_DELETE:
	  return cp_parser_delete_expression (parser);
	  
	case RID_EXTENSION:
	  {
	    /* The saved value of the PEDANTIC flag.  */
	    int saved_pedantic;
	    tree expr;

	    /* Save away the PEDANTIC flag.  */
	    cp_parser_extension_opt (parser, &saved_pedantic);
	    /* Parse the cast-expression.  */
	    expr = cp_parser_simple_cast_expression (parser);
	    /* Restore the PEDANTIC flag.  */
	    pedantic = saved_pedantic;

	    return expr;
	  }

	case RID_REALPART:
	case RID_IMAGPART:
	  {
	    tree expression;

	    /* Consume the `__real__' or `__imag__' token.  */
	    cp_lexer_consume_token (parser->lexer);
	    /* Parse the cast-expression.  */
	    expression = cp_parser_simple_cast_expression (parser);
	    /* Create the complete representation.  */
	    return build_x_unary_op ((keyword == RID_REALPART
				      ? REALPART_EXPR : IMAGPART_EXPR),
				     expression);
	  }
	  break;

	default:
	  break;
	}
    }

  /* Look for the `:: new' and `:: delete', which also signal the
     beginning of a new-expression, or delete-expression,
     respectively.  If the next token is `::', then it might be one of
     these.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_SCOPE))
    {
      enum rid keyword;

      /* See if the token after the `::' is one of the keywords in
	 which we're interested.  */
      keyword = cp_lexer_peek_nth_token (parser->lexer, 2)->keyword;
      /* If it's `new', we have a new-expression.  */
      if (keyword == RID_NEW)
	return cp_parser_new_expression (parser);
      /* Similarly, for `delete'.  */
      else if (keyword == RID_DELETE)
	return cp_parser_delete_expression (parser);
    }

  /* Look for a unary operator.  */
  unary_operator = cp_parser_unary_operator (token);
  /* The `++' and `--' operators can be handled similarly, even though
     they are not technically unary-operators in the grammar.  */
  if (unary_operator == ERROR_MARK)
    {
      if (token->type == CPP_PLUS_PLUS)
	unary_operator = PREINCREMENT_EXPR;
      else if (token->type == CPP_MINUS_MINUS)
	unary_operator = PREDECREMENT_EXPR;
      /* Handle the GNU address-of-label extension.  */
      else if (cp_parser_allow_gnu_extensions_p (parser)
	       && token->type == CPP_AND_AND)
	{
	  tree identifier;

	  /* Consume the '&&' token.  */
	  cp_lexer_consume_token (parser->lexer);
	  /* Look for the identifier.  */
	  identifier = cp_parser_identifier (parser);
	  /* Create an expression representing the address.  */
	  return finish_label_address_expr (identifier);
	}
    }
  if (unary_operator != ERROR_MARK)
    {
      tree cast_expression;
      tree expression = error_mark_node;
      const char *non_constant_p = NULL;

      /* Consume the operator token.  */
      token = cp_lexer_consume_token (parser->lexer);
      /* Parse the cast-expression.  */
      cast_expression 
	= cp_parser_cast_expression (parser, unary_operator == ADDR_EXPR);
      /* Now, build an appropriate representation.  */
      switch (unary_operator)
	{
	case INDIRECT_REF:
	  non_constant_p = "`*'";
	  expression = build_x_indirect_ref (cast_expression, "unary *");
	  break;

	case ADDR_EXPR:
	  /* The "&" operator is allowed in the implementation of
	     "offsetof".  */
	  if (!parser->in_offsetof_p)
	    non_constant_p = "`&'";
	  /* Fall through.  */
	case BIT_NOT_EXPR:
	  expression = build_x_unary_op (unary_operator, cast_expression);
	  break;

	case PREINCREMENT_EXPR:
	case PREDECREMENT_EXPR:
	  non_constant_p = (unary_operator == PREINCREMENT_EXPR
			    ? "`++'" : "`--'");
	  /* Fall through.  */
	case CONVERT_EXPR:
	case NEGATE_EXPR:
	case TRUTH_NOT_EXPR:
	  expression = finish_unary_op_expr (unary_operator, cast_expression);
	  break;

	default:
	  abort ();
	}

      if (non_constant_p 
	  && cp_parser_non_integral_constant_expression (parser,
							 non_constant_p))
	expression = error_mark_node;

      return expression;
    }

  return cp_parser_postfix_expression (parser, address_p);
}

/* Returns ERROR_MARK if TOKEN is not a unary-operator.  If TOKEN is a
   unary-operator, the corresponding tree code is returned.  */

static enum tree_code
cp_parser_unary_operator (cp_token* token)
{
  switch (token->type)
    {
    case CPP_MULT:
      return INDIRECT_REF;

    case CPP_AND:
      return ADDR_EXPR;

    case CPP_PLUS:
      return CONVERT_EXPR;

    case CPP_MINUS:
      return NEGATE_EXPR;

    case CPP_NOT:
      return TRUTH_NOT_EXPR;
      
    case CPP_COMPL:
      return BIT_NOT_EXPR;

    default:
      return ERROR_MARK;
    }
}

/* Parse a new-expression.

   new-expression:
     :: [opt] new new-placement [opt] new-type-id new-initializer [opt]
     :: [opt] new new-placement [opt] ( type-id ) new-initializer [opt]

   Returns a representation of the expression.  */

static tree
cp_parser_new_expression (cp_parser* parser)
{
  bool global_scope_p;
  tree placement;
  tree type;
  tree initializer;

  /* Look for the optional `::' operator.  */
  global_scope_p 
    = (cp_parser_global_scope_opt (parser,
				   /*current_scope_valid_p=*/false)
       != NULL_TREE);
  /* Look for the `new' operator.  */
  cp_parser_require_keyword (parser, RID_NEW, "`new'");
  /* There's no easy way to tell a new-placement from the
     `( type-id )' construct.  */
  cp_parser_parse_tentatively (parser);
  /* Look for a new-placement.  */
  placement = cp_parser_new_placement (parser);
  /* If that didn't work out, there's no new-placement.  */
  if (!cp_parser_parse_definitely (parser))
    placement = NULL_TREE;

  /* If the next token is a `(', then we have a parenthesized
     type-id.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_OPEN_PAREN))
    {
      /* Consume the `('.  */
      cp_lexer_consume_token (parser->lexer);
      /* Parse the type-id.  */
      type = cp_parser_type_id (parser);
      /* Look for the closing `)'.  */
      cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");
      /* There should not be a direct-new-declarator in this production, 
         but GCC used to allowed this, so we check and emit a sensible error
	 message for this case.  */
      if (cp_lexer_next_token_is (parser->lexer, CPP_OPEN_SQUARE))
	{
	  error ("array bound forbidden after parenthesized type-id");
	  inform ("try removing the parentheses around the type-id");
	  cp_parser_direct_new_declarator (parser);
	}
    }
  /* Otherwise, there must be a new-type-id.  */
  else
    type = cp_parser_new_type_id (parser);

  /* If the next token is a `(', then we have a new-initializer.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_OPEN_PAREN))
    initializer = cp_parser_new_initializer (parser);
  else
    initializer = NULL_TREE;

  /* A new-expression may not appear in an integral constant
     expression.  */
  if (cp_parser_non_integral_constant_expression (parser, "`new'"))
    return error_mark_node;

  /* Create a representation of the new-expression.  */
  return build_new (placement, type, initializer, global_scope_p);
}

/* Parse a new-placement.

   new-placement:
     ( expression-list )

   Returns the same representation as for an expression-list.  */

static tree
cp_parser_new_placement (cp_parser* parser)
{
  tree expression_list;

  /* Parse the expression-list.  */
  expression_list = (cp_parser_parenthesized_expression_list 
		     (parser, false, /*non_constant_p=*/NULL));

  return expression_list;
}

/* Parse a new-type-id.

   new-type-id:
     type-specifier-seq new-declarator [opt]

   Returns a TREE_LIST whose TREE_PURPOSE is the type-specifier-seq,
   and whose TREE_VALUE is the new-declarator.  */

static tree
cp_parser_new_type_id (cp_parser* parser)
{
  tree type_specifier_seq;
  tree declarator;
  const char *saved_message;

  /* The type-specifier sequence must not contain type definitions.
     (It cannot contain declarations of new types either, but if they
     are not definitions we will catch that because they are not
     complete.)  */
  saved_message = parser->type_definition_forbidden_message;
  parser->type_definition_forbidden_message
    = "types may not be defined in a new-type-id";
  /* Parse the type-specifier-seq.  */
  type_specifier_seq = cp_parser_type_specifier_seq (parser);
  /* Restore the old message.  */
  parser->type_definition_forbidden_message = saved_message;
  /* Parse the new-declarator.  */
  declarator = cp_parser_new_declarator_opt (parser);

  return build_tree_list (type_specifier_seq, declarator);
}

/* Parse an (optional) new-declarator.

   new-declarator:
     ptr-operator new-declarator [opt]
     direct-new-declarator

   Returns a representation of the declarator.  See
   cp_parser_declarator for the representations used.  */

static tree
cp_parser_new_declarator_opt (cp_parser* parser)
{
  enum tree_code code;
  tree type;
  tree cv_qualifier_seq;

  /* We don't know if there's a ptr-operator next, or not.  */
  cp_parser_parse_tentatively (parser);
  /* Look for a ptr-operator.  */
  code = cp_parser_ptr_operator (parser, &type, &cv_qualifier_seq);
  /* If that worked, look for more new-declarators.  */
  if (cp_parser_parse_definitely (parser))
    {
      tree declarator;

      /* Parse another optional declarator.  */
      declarator = cp_parser_new_declarator_opt (parser);

      /* Create the representation of the declarator.  */
      if (code == INDIRECT_REF)
	declarator = make_pointer_declarator (cv_qualifier_seq,
					      declarator);
      else
	declarator = make_reference_declarator (cv_qualifier_seq,
						declarator);

     /* Handle the pointer-to-member case.  */
     if (type)
       declarator = build_nt (SCOPE_REF, type, declarator);

      return declarator;
    }

  /* If the next token is a `[', there is a direct-new-declarator.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_OPEN_SQUARE))
    return cp_parser_direct_new_declarator (parser);

  return NULL_TREE;
}

/* Parse a direct-new-declarator.

   direct-new-declarator:
     [ expression ]
     direct-new-declarator [constant-expression]  

   Returns an ARRAY_REF, following the same conventions as are
   documented for cp_parser_direct_declarator.  */

static tree
cp_parser_direct_new_declarator (cp_parser* parser)
{
  tree declarator = NULL_TREE;

  while (true)
    {
      tree expression;

      /* Look for the opening `['.  */
      cp_parser_require (parser, CPP_OPEN_SQUARE, "`['");
      /* The first expression is not required to be constant.  */
      if (!declarator)
	{
	  expression = cp_parser_expression (parser);
	  /* The standard requires that the expression have integral
	     type.  DR 74 adds enumeration types.  We believe that the
	     real intent is that these expressions be handled like the
	     expression in a `switch' condition, which also allows
	     classes with a single conversion to integral or
	     enumeration type.  */
	  if (!processing_template_decl)
	    {
	      expression 
		= build_expr_type_conversion (WANT_INT | WANT_ENUM,
					      expression,
					      /*complain=*/true);
	      if (!expression)
		{
		  error ("expression in new-declarator must have integral or enumeration type");
		  expression = error_mark_node;
		}
	    }
	}
      /* But all the other expressions must be.  */
      else
	expression 
	  = cp_parser_constant_expression (parser, 
					   /*allow_non_constant=*/false,
					   NULL);
      /* Look for the closing `]'.  */
      cp_parser_require (parser, CPP_CLOSE_SQUARE, "`]'");

      /* Add this bound to the declarator.  */
      declarator = build_nt (ARRAY_REF, declarator, expression);

      /* If the next token is not a `[', then there are no more
	 bounds.  */
      if (cp_lexer_next_token_is_not (parser->lexer, CPP_OPEN_SQUARE))
	break;
    }

  return declarator;
}

/* Parse a new-initializer.

   new-initializer:
     ( expression-list [opt] )

   Returns a representation of the expression-list.  If there is no
   expression-list, VOID_ZERO_NODE is returned.  */

static tree
cp_parser_new_initializer (cp_parser* parser)
{
  tree expression_list;

  expression_list = (cp_parser_parenthesized_expression_list 
		     (parser, false, /*non_constant_p=*/NULL));
  if (!expression_list)
    expression_list = void_zero_node;

  return expression_list;
}

/* Parse a delete-expression.

   delete-expression:
     :: [opt] delete cast-expression
     :: [opt] delete [ ] cast-expression

   Returns a representation of the expression.  */

static tree
cp_parser_delete_expression (cp_parser* parser)
{
  bool global_scope_p;
  bool array_p;
  tree expression;

  /* Look for the optional `::' operator.  */
  global_scope_p
    = (cp_parser_global_scope_opt (parser,
				   /*current_scope_valid_p=*/false)
       != NULL_TREE);
  /* Look for the `delete' keyword.  */
  cp_parser_require_keyword (parser, RID_DELETE, "`delete'");
  /* See if the array syntax is in use.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_OPEN_SQUARE))
    {
      /* Consume the `[' token.  */
      cp_lexer_consume_token (parser->lexer);
      /* Look for the `]' token.  */
      cp_parser_require (parser, CPP_CLOSE_SQUARE, "`]'");
      /* Remember that this is the `[]' construct.  */
      array_p = true;
    }
  else
    array_p = false;

  /* Parse the cast-expression.  */
  expression = cp_parser_simple_cast_expression (parser);

  /* A delete-expression may not appear in an integral constant
     expression.  */
  if (cp_parser_non_integral_constant_expression (parser, "`delete'"))
    return error_mark_node;

  return delete_sanity (expression, NULL_TREE, array_p, global_scope_p);
}

/* Parse a cast-expression.

   cast-expression:
     unary-expression
     ( type-id ) cast-expression

   Returns a representation of the expression.  */

static tree
cp_parser_cast_expression (cp_parser *parser, bool address_p)
{
  /* If it's a `(', then we might be looking at a cast.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_OPEN_PAREN))
    {
      tree type = NULL_TREE;
      tree expr = NULL_TREE;
      bool compound_literal_p;
      const char *saved_message;

      /* There's no way to know yet whether or not this is a cast.
	 For example, `(int (3))' is a unary-expression, while `(int)
	 3' is a cast.  So, we resort to parsing tentatively.  */
      cp_parser_parse_tentatively (parser);
      /* Types may not be defined in a cast.  */
      saved_message = parser->type_definition_forbidden_message;
      parser->type_definition_forbidden_message
	= "types may not be defined in casts";
      /* Consume the `('.  */
      cp_lexer_consume_token (parser->lexer);
      /* A very tricky bit is that `(struct S) { 3 }' is a
	 compound-literal (which we permit in C++ as an extension).
	 But, that construct is not a cast-expression -- it is a
	 postfix-expression.  (The reason is that `(struct S) { 3 }.i'
	 is legal; if the compound-literal were a cast-expression,
	 you'd need an extra set of parentheses.)  But, if we parse
	 the type-id, and it happens to be a class-specifier, then we
	 will commit to the parse at that point, because we cannot
	 undo the action that is done when creating a new class.  So,
	 then we cannot back up and do a postfix-expression.  

	 Therefore, we scan ahead to the closing `)', and check to see
	 if the token after the `)' is a `{'.  If so, we are not
	 looking at a cast-expression.  

	 Save tokens so that we can put them back.  */
      cp_lexer_save_tokens (parser->lexer);
      /* Skip tokens until the next token is a closing parenthesis.
	 If we find the closing `)', and the next token is a `{', then
	 we are looking at a compound-literal.  */
      compound_literal_p 
	= (cp_parser_skip_to_closing_parenthesis (parser, false, false,
						  /*consume_paren=*/true)
	   && cp_lexer_next_token_is (parser->lexer, CPP_OPEN_BRACE));
      /* Roll back the tokens we skipped.  */
      cp_lexer_rollback_tokens (parser->lexer);
      /* If we were looking at a compound-literal, simulate an error
	 so that the call to cp_parser_parse_definitely below will
	 fail.  */
      if (compound_literal_p)
	cp_parser_simulate_error (parser);
      else
	{
	  bool saved_in_type_id_in_expr_p = parser->in_type_id_in_expr_p;
	  parser->in_type_id_in_expr_p = true;
	  /* Look for the type-id.  */
	  type = cp_parser_type_id (parser);
	  /* Look for the closing `)'.  */
	  cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");
	  parser->in_type_id_in_expr_p = saved_in_type_id_in_expr_p;
	}

      /* Restore the saved message.  */
      parser->type_definition_forbidden_message = saved_message;

      /* If ok so far, parse the dependent expression. We cannot be
         sure it is a cast. Consider `(T ())'.  It is a parenthesized
         ctor of T, but looks like a cast to function returning T
         without a dependent expression.  */
      if (!cp_parser_error_occurred (parser))
	expr = cp_parser_simple_cast_expression (parser);

      if (cp_parser_parse_definitely (parser))
	{
	  /* Warn about old-style casts, if so requested.  */
	  if (warn_old_style_cast 
	      && !in_system_header 
	      && !VOID_TYPE_P (type) 
	      && current_lang_name != lang_name_c)
	    warning ("use of old-style cast");

	  /* Only type conversions to integral or enumeration types
	     can be used in constant-expressions.  */
	  if (parser->integral_constant_expression_p
	      && !dependent_type_p (type)
	      && !INTEGRAL_OR_ENUMERATION_TYPE_P (type)
	      && (cp_parser_non_integral_constant_expression 
		  (parser,
		   "a casts to a type other than an integral or "
		   "enumeration type")))
	    return error_mark_node;

	  /* Perform the cast.  */
	  expr = build_c_cast (type, expr);
	  return expr;
	}
    }

  /* If we get here, then it's not a cast, so it must be a
     unary-expression.  */
  return cp_parser_unary_expression (parser, address_p);
}

/* Parse a pm-expression.

   pm-expression:
     cast-expression
     pm-expression .* cast-expression
     pm-expression ->* cast-expression

     Returns a representation of the expression.  */

static tree
cp_parser_pm_expression (cp_parser* parser)
{
  static const cp_parser_token_tree_map map = {
    { CPP_DEREF_STAR, MEMBER_REF },
    { CPP_DOT_STAR, DOTSTAR_EXPR },
    { CPP_EOF, ERROR_MARK }
  };

  return cp_parser_binary_expression (parser, map, 
				      cp_parser_simple_cast_expression);
}

/* Parse a multiplicative-expression.

   mulitplicative-expression:
     pm-expression
     multiplicative-expression * pm-expression
     multiplicative-expression / pm-expression
     multiplicative-expression % pm-expression

   Returns a representation of the expression.  */

static tree
cp_parser_multiplicative_expression (cp_parser* parser)
{
  static const cp_parser_token_tree_map map = {
    { CPP_MULT, MULT_EXPR },
    { CPP_DIV, TRUNC_DIV_EXPR },
    { CPP_MOD, TRUNC_MOD_EXPR },
    { CPP_EOF, ERROR_MARK }
  };

  return cp_parser_binary_expression (parser,
				      map,
				      cp_parser_pm_expression);
}

/* Parse an additive-expression.

   additive-expression:
     multiplicative-expression
     additive-expression + multiplicative-expression
     additive-expression - multiplicative-expression

   Returns a representation of the expression.  */

static tree
cp_parser_additive_expression (cp_parser* parser)
{
  static const cp_parser_token_tree_map map = {
    { CPP_PLUS, PLUS_EXPR },
    { CPP_MINUS, MINUS_EXPR },
    { CPP_EOF, ERROR_MARK }
  };

  return cp_parser_binary_expression (parser,
				      map,
				      cp_parser_multiplicative_expression);
}

/* Parse a shift-expression.

   shift-expression:
     additive-expression
     shift-expression << additive-expression
     shift-expression >> additive-expression

   Returns a representation of the expression.  */

static tree
cp_parser_shift_expression (cp_parser* parser)
{
  static const cp_parser_token_tree_map map = {
    { CPP_LSHIFT, LSHIFT_EXPR },
    { CPP_RSHIFT, RSHIFT_EXPR },
    { CPP_EOF, ERROR_MARK }
  };

  return cp_parser_binary_expression (parser,
				      map,
				      cp_parser_additive_expression);
}

/* Parse a relational-expression.

   relational-expression:
     shift-expression
     relational-expression < shift-expression
     relational-expression > shift-expression
     relational-expression <= shift-expression
     relational-expression >= shift-expression

   GNU Extension:

   relational-expression:
     relational-expression <? shift-expression
     relational-expression >? shift-expression

   Returns a representation of the expression.  */

static tree
cp_parser_relational_expression (cp_parser* parser)
{
  static const cp_parser_token_tree_map map = {
    { CPP_LESS, LT_EXPR },
    { CPP_GREATER, GT_EXPR },
    { CPP_LESS_EQ, LE_EXPR },
    { CPP_GREATER_EQ, GE_EXPR },
    { CPP_MIN, MIN_EXPR },
    { CPP_MAX, MAX_EXPR },
    { CPP_EOF, ERROR_MARK }
  };

  return cp_parser_binary_expression (parser,
				      map,
				      cp_parser_shift_expression);
}

/* Parse an equality-expression.

   equality-expression:
     relational-expression
     equality-expression == relational-expression
     equality-expression != relational-expression

   Returns a representation of the expression.  */

static tree
cp_parser_equality_expression (cp_parser* parser)
{
  static const cp_parser_token_tree_map map = {
    { CPP_EQ_EQ, EQ_EXPR },
    { CPP_NOT_EQ, NE_EXPR },
    { CPP_EOF, ERROR_MARK }
  };

  return cp_parser_binary_expression (parser,
				      map,
				      cp_parser_relational_expression);
}

/* Parse an and-expression.

   and-expression:
     equality-expression
     and-expression & equality-expression

   Returns a representation of the expression.  */

static tree
cp_parser_and_expression (cp_parser* parser)
{
  static const cp_parser_token_tree_map map = {
    { CPP_AND, BIT_AND_EXPR },
    { CPP_EOF, ERROR_MARK }
  };

  return cp_parser_binary_expression (parser,
				      map,
				      cp_parser_equality_expression);
}

/* Parse an exclusive-or-expression.

   exclusive-or-expression:
     and-expression
     exclusive-or-expression ^ and-expression

   Returns a representation of the expression.  */

static tree
cp_parser_exclusive_or_expression (cp_parser* parser)
{
  static const cp_parser_token_tree_map map = {
    { CPP_XOR, BIT_XOR_EXPR },
    { CPP_EOF, ERROR_MARK }
  };

  return cp_parser_binary_expression (parser,
				      map,
				      cp_parser_and_expression);
}


/* Parse an inclusive-or-expression.

   inclusive-or-expression:
     exclusive-or-expression
     inclusive-or-expression | exclusive-or-expression

   Returns a representation of the expression.  */

static tree
cp_parser_inclusive_or_expression (cp_parser* parser)
{
  static const cp_parser_token_tree_map map = {
    { CPP_OR, BIT_IOR_EXPR },
    { CPP_EOF, ERROR_MARK }
  };

  return cp_parser_binary_expression (parser,
				      map,
				      cp_parser_exclusive_or_expression);
}

/* Parse a logical-and-expression.

   logical-and-expression:
     inclusive-or-expression
     logical-and-expression && inclusive-or-expression

   Returns a representation of the expression.  */

static tree
cp_parser_logical_and_expression (cp_parser* parser)
{
  static const cp_parser_token_tree_map map = {
    { CPP_AND_AND, TRUTH_ANDIF_EXPR },
    { CPP_EOF, ERROR_MARK }
  };

  return cp_parser_binary_expression (parser,
				      map,
				      cp_parser_inclusive_or_expression);
}

/* Parse a logical-or-expression.

   logical-or-expression:
     logical-and-expression
     logical-or-expression || logical-and-expression

   Returns a representation of the expression.  */

static tree
cp_parser_logical_or_expression (cp_parser* parser)
{
  static const cp_parser_token_tree_map map = {
    { CPP_OR_OR, TRUTH_ORIF_EXPR },
    { CPP_EOF, ERROR_MARK }
  };

  return cp_parser_binary_expression (parser,
				      map,
				      cp_parser_logical_and_expression);
}

/* Parse the `? expression : assignment-expression' part of a
   conditional-expression.  The LOGICAL_OR_EXPR is the
   logical-or-expression that started the conditional-expression.
   Returns a representation of the entire conditional-expression.

   This routine is used by cp_parser_assignment_expression.

     ? expression : assignment-expression
   
   GNU Extensions:
   
     ? : assignment-expression */

static tree
cp_parser_question_colon_clause (cp_parser* parser, tree logical_or_expr)
{
  tree expr;
  tree assignment_expr;

  /* Consume the `?' token.  */
  cp_lexer_consume_token (parser->lexer);
  if (cp_parser_allow_gnu_extensions_p (parser)
      && cp_lexer_next_token_is (parser->lexer, CPP_COLON))
    /* Implicit true clause.  */
    expr = NULL_TREE;
  else
    /* Parse the expression.  */
    expr = cp_parser_expression (parser);
  
  /* The next token should be a `:'.  */
  cp_parser_require (parser, CPP_COLON, "`:'");
  /* Parse the assignment-expression.  */
  assignment_expr = cp_parser_assignment_expression (parser);

  /* Build the conditional-expression.  */
  return build_x_conditional_expr (logical_or_expr,
				   expr,
				   assignment_expr);
}

/* Parse an assignment-expression.

   assignment-expression:
     conditional-expression
     logical-or-expression assignment-operator assignment_expression
     throw-expression

   Returns a representation for the expression.  */

static tree
cp_parser_assignment_expression (cp_parser* parser)
{
  tree expr;

  /* If the next token is the `throw' keyword, then we're looking at
     a throw-expression.  */
  if (cp_lexer_next_token_is_keyword (parser->lexer, RID_THROW))
    expr = cp_parser_throw_expression (parser);
  /* Otherwise, it must be that we are looking at a
     logical-or-expression.  */
  else
    {
      /* Parse the logical-or-expression.  */
      expr = cp_parser_logical_or_expression (parser);
      /* If the next token is a `?' then we're actually looking at a
	 conditional-expression.  */
      if (cp_lexer_next_token_is (parser->lexer, CPP_QUERY))
	return cp_parser_question_colon_clause (parser, expr);
      else 
	{
	  enum tree_code assignment_operator;

	  /* If it's an assignment-operator, we're using the second
	     production.  */
	  assignment_operator 
	    = cp_parser_assignment_operator_opt (parser);
	  if (assignment_operator != ERROR_MARK)
	    {
	      tree rhs;

	      /* Parse the right-hand side of the assignment.  */
	      rhs = cp_parser_assignment_expression (parser);
	      /* An assignment may not appear in a
		 constant-expression.  */
	      if (cp_parser_non_integral_constant_expression (parser,
							      "an assignment"))
		return error_mark_node;
	      /* Build the assignment expression.  */
	      expr = build_x_modify_expr (expr, 
					  assignment_operator, 
					  rhs);
	    }
	}
    }

  return expr;
}

/* Parse an (optional) assignment-operator.

   assignment-operator: one of 
     = *= /= %= += -= >>= <<= &= ^= |=  

   GNU Extension:
   
   assignment-operator: one of
     <?= >?=

   If the next token is an assignment operator, the corresponding tree
   code is returned, and the token is consumed.  For example, for
   `+=', PLUS_EXPR is returned.  For `=' itself, the code returned is
   NOP_EXPR.  For `/', TRUNC_DIV_EXPR is returned; for `%',
   TRUNC_MOD_EXPR is returned.  If TOKEN is not an assignment
   operator, ERROR_MARK is returned.  */

static enum tree_code
cp_parser_assignment_operator_opt (cp_parser* parser)
{
  enum tree_code op;
  cp_token *token;

  /* Peek at the next toen.  */
  token = cp_lexer_peek_token (parser->lexer);

  switch (token->type)
    {
    case CPP_EQ:
      op = NOP_EXPR;
      break;

    case CPP_MULT_EQ:
      op = MULT_EXPR;
      break;

    case CPP_DIV_EQ:
      op = TRUNC_DIV_EXPR;
      break;

    case CPP_MOD_EQ:
      op = TRUNC_MOD_EXPR;
      break;

    case CPP_PLUS_EQ:
      op = PLUS_EXPR;
      break;

    case CPP_MINUS_EQ:
      op = MINUS_EXPR;
      break;

    case CPP_RSHIFT_EQ:
      op = RSHIFT_EXPR;
      break;

    case CPP_LSHIFT_EQ:
      op = LSHIFT_EXPR;
      break;

    case CPP_AND_EQ:
      op = BIT_AND_EXPR;
      break;

    case CPP_XOR_EQ:
      op = BIT_XOR_EXPR;
      break;

    case CPP_OR_EQ:
      op = BIT_IOR_EXPR;
      break;

    case CPP_MIN_EQ:
      op = MIN_EXPR;
      break;

    case CPP_MAX_EQ:
      op = MAX_EXPR;
      break;

    default: 
      /* Nothing else is an assignment operator.  */
      op = ERROR_MARK;
    }

  /* If it was an assignment operator, consume it.  */
  if (op != ERROR_MARK)
    cp_lexer_consume_token (parser->lexer);

  return op;
}

/* Parse an expression.

   expression:
     assignment-expression
     expression , assignment-expression

   Returns a representation of the expression.  */

static tree
cp_parser_expression (cp_parser* parser)
{
  tree expression = NULL_TREE;

  while (true)
    {
      tree assignment_expression;

      /* Parse the next assignment-expression.  */
      assignment_expression 
	= cp_parser_assignment_expression (parser);
      /* If this is the first assignment-expression, we can just
	 save it away.  */
      if (!expression)
	expression = assignment_expression;
      else
	expression = build_x_compound_expr (expression,
					    assignment_expression);
      /* If the next token is not a comma, then we are done with the
	 expression.  */
      if (cp_lexer_next_token_is_not (parser->lexer, CPP_COMMA))
	break;
      /* Consume the `,'.  */
      cp_lexer_consume_token (parser->lexer);
      /* A comma operator cannot appear in a constant-expression.  */
      if (cp_parser_non_integral_constant_expression (parser,
						      "a comma operator"))
	expression = error_mark_node;
    }

  return expression;
}

/* Parse a constant-expression. 

   constant-expression:
     conditional-expression  

  If ALLOW_NON_CONSTANT_P a non-constant expression is silently
  accepted.  If ALLOW_NON_CONSTANT_P is true and the expression is not
  constant, *NON_CONSTANT_P is set to TRUE.  If ALLOW_NON_CONSTANT_P
  is false, NON_CONSTANT_P should be NULL.  */

static tree
cp_parser_constant_expression (cp_parser* parser, 
			       bool allow_non_constant_p,
			       bool *non_constant_p)
{
  bool saved_integral_constant_expression_p;
  bool saved_allow_non_integral_constant_expression_p;
  bool saved_non_integral_constant_expression_p;
  tree expression;

  /* It might seem that we could simply parse the
     conditional-expression, and then check to see if it were
     TREE_CONSTANT.  However, an expression that is TREE_CONSTANT is
     one that the compiler can figure out is constant, possibly after
     doing some simplifications or optimizations.  The standard has a
     precise definition of constant-expression, and we must honor
     that, even though it is somewhat more restrictive.

     For example:

       int i[(2, 3)];

     is not a legal declaration, because `(2, 3)' is not a
     constant-expression.  The `,' operator is forbidden in a
     constant-expression.  However, GCC's constant-folding machinery
     will fold this operation to an INTEGER_CST for `3'.  */

  /* Save the old settings.  */
  saved_integral_constant_expression_p = parser->integral_constant_expression_p;
  saved_allow_non_integral_constant_expression_p 
    = parser->allow_non_integral_constant_expression_p;
  saved_non_integral_constant_expression_p = parser->non_integral_constant_expression_p;
  /* We are now parsing a constant-expression.  */
  parser->integral_constant_expression_p = true;
  parser->allow_non_integral_constant_expression_p = allow_non_constant_p;
  parser->non_integral_constant_expression_p = false;
  /* Although the grammar says "conditional-expression", we parse an
     "assignment-expression", which also permits "throw-expression"
     and the use of assignment operators.  In the case that
     ALLOW_NON_CONSTANT_P is false, we get better errors than we would
     otherwise.  In the case that ALLOW_NON_CONSTANT_P is true, it is
     actually essential that we look for an assignment-expression.
     For example, cp_parser_initializer_clauses uses this function to
     determine whether a particular assignment-expression is in fact
     constant.  */
  expression = cp_parser_assignment_expression (parser);
  /* Restore the old settings.  */
  parser->integral_constant_expression_p = saved_integral_constant_expression_p;
  parser->allow_non_integral_constant_expression_p 
    = saved_allow_non_integral_constant_expression_p;
  if (allow_non_constant_p)
    *non_constant_p = parser->non_integral_constant_expression_p;
  parser->non_integral_constant_expression_p = saved_non_integral_constant_expression_p;

  return expression;
}

/* Statements [gram.stmt.stmt]  */

/* Parse a statement.  

   statement:
     labeled-statement
     expression-statement
     compound-statement
     selection-statement
     iteration-statement
     jump-statement
     declaration-statement
     try-block  */

static void
cp_parser_statement (cp_parser* parser, bool in_statement_expr_p)
{
  tree statement;
  cp_token *token;
  int statement_line_number;

  /* There is no statement yet.  */
  statement = NULL_TREE;
  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* Remember the line number of the first token in the statement.  */
  statement_line_number = token->location.line;
  /* If this is a keyword, then that will often determine what kind of
     statement we have.  */
  if (token->type == CPP_KEYWORD)
    {
      enum rid keyword = token->keyword;

      switch (keyword)
	{
	case RID_CASE:
	case RID_DEFAULT:
	  statement = cp_parser_labeled_statement (parser,
						   in_statement_expr_p);
	  break;

	case RID_IF:
	case RID_SWITCH:
	  statement = cp_parser_selection_statement (parser);
	  break;

	case RID_WHILE:
	case RID_DO:
	case RID_FOR:
	  statement = cp_parser_iteration_statement (parser);
	  break;

	case RID_BREAK:
	case RID_CONTINUE:
	case RID_RETURN:
	case RID_GOTO:
	  statement = cp_parser_jump_statement (parser);
	  break;

	case RID_TRY:
	  statement = cp_parser_try_block (parser);
	  break;

	default:
	  /* It might be a keyword like `int' that can start a
	     declaration-statement.  */
	  break;
	}
    }
  else if (token->type == CPP_NAME)
    {
      /* If the next token is a `:', then we are looking at a
	 labeled-statement.  */
      token = cp_lexer_peek_nth_token (parser->lexer, 2);
      if (token->type == CPP_COLON)
	statement = cp_parser_labeled_statement (parser, in_statement_expr_p);
    }
  /* Anything that starts with a `{' must be a compound-statement.  */
  else if (token->type == CPP_OPEN_BRACE)
    statement = cp_parser_compound_statement (parser, false);

  /* Everything else must be a declaration-statement or an
     expression-statement.  Try for the declaration-statement 
     first, unless we are looking at a `;', in which case we know that
     we have an expression-statement.  */
  if (!statement)
    {
      if (cp_lexer_next_token_is_not (parser->lexer, CPP_SEMICOLON))
	{
	  cp_parser_parse_tentatively (parser);
	  /* Try to parse the declaration-statement.  */
	  cp_parser_declaration_statement (parser);
	  /* If that worked, we're done.  */
	  if (cp_parser_parse_definitely (parser))
	    return;
	}
      /* Look for an expression-statement instead.  */
      statement = cp_parser_expression_statement (parser, in_statement_expr_p);
    }

  /* Set the line number for the statement.  */
  if (statement && STATEMENT_CODE_P (TREE_CODE (statement)))
    STMT_LINENO (statement) = statement_line_number;
}

/* Parse a labeled-statement.

   labeled-statement:
     identifier : statement
     case constant-expression : statement
     default : statement

   GNU Extension:
   
   labeled-statement:
     case constant-expression ... constant-expression : statement

   Returns the new CASE_LABEL, for a `case' or `default' label.  For
   an ordinary label, returns a LABEL_STMT.  */

static tree
cp_parser_labeled_statement (cp_parser* parser, bool in_statement_expr_p)
{
  cp_token *token;
  tree statement = error_mark_node;

  /* The next token should be an identifier.  */
  token = cp_lexer_peek_token (parser->lexer);
  if (token->type != CPP_NAME
      && token->type != CPP_KEYWORD)
    {
      cp_parser_error (parser, "expected labeled-statement");
      return error_mark_node;
    }

  switch (token->keyword)
    {
    case RID_CASE:
      {
	tree expr, expr_hi;
	cp_token *ellipsis;

	/* Consume the `case' token.  */
	cp_lexer_consume_token (parser->lexer);
	/* Parse the constant-expression.  */
	expr = cp_parser_constant_expression (parser, 
					      /*allow_non_constant_p=*/false,
					      NULL);

	ellipsis = cp_lexer_peek_token (parser->lexer);
	if (ellipsis->type == CPP_ELLIPSIS)
	  {
            /* Consume the `...' token.  */
	    cp_lexer_consume_token (parser->lexer);
	    expr_hi =
	      cp_parser_constant_expression (parser,
	    				     /*allow_non_constant_p=*/false,
					     NULL);
	    /* We don't need to emit warnings here, as the common code
	       will do this for us.  */
	  }
	else
	  expr_hi = NULL_TREE;

	if (!parser->in_switch_statement_p)
	  error ("case label `%E' not within a switch statement", expr);
	else
	  statement = finish_case_label (expr, expr_hi);
      }
      break;

    case RID_DEFAULT:
      /* Consume the `default' token.  */
      cp_lexer_consume_token (parser->lexer);
      if (!parser->in_switch_statement_p)
	error ("case label not within a switch statement");
      else
	statement = finish_case_label (NULL_TREE, NULL_TREE);
      break;

    default:
      /* Anything else must be an ordinary label.  */
      statement = finish_label_stmt (cp_parser_identifier (parser));
      break;
    }

  /* Require the `:' token.  */
  cp_parser_require (parser, CPP_COLON, "`:'");
  /* Parse the labeled statement.  */
  cp_parser_statement (parser, in_statement_expr_p);

  /* Return the label, in the case of a `case' or `default' label.  */
  return statement;
}

/* Parse an expression-statement.

   expression-statement:
     expression [opt] ;

   Returns the new EXPR_STMT -- or NULL_TREE if the expression
   statement consists of nothing more than an `;'. IN_STATEMENT_EXPR_P
   indicates whether this expression-statement is part of an
   expression statement.  */

static tree
cp_parser_expression_statement (cp_parser* parser, bool in_statement_expr_p)
{
  tree statement = NULL_TREE;

  /* If the next token is a ';', then there is no expression
     statement.  */
  if (cp_lexer_next_token_is_not (parser->lexer, CPP_SEMICOLON))
    statement = cp_parser_expression (parser);
  
  /* Consume the final `;'.  */
  cp_parser_consume_semicolon_at_end_of_statement (parser);

  if (in_statement_expr_p
      && cp_lexer_next_token_is (parser->lexer, CPP_CLOSE_BRACE))
    {
      /* This is the final expression statement of a statement
	 expression.  */
      statement = finish_stmt_expr_expr (statement);
    }
  else if (statement)
    statement = finish_expr_stmt (statement);
  else
    finish_stmt ();
  
  return statement;
}

/* Parse a compound-statement.

   compound-statement:
     { statement-seq [opt] }
     
   Returns a COMPOUND_STMT representing the statement.  */

static tree
cp_parser_compound_statement (cp_parser *parser, bool in_statement_expr_p)
{
  tree compound_stmt;

  /* Consume the `{'.  */
  if (!cp_parser_require (parser, CPP_OPEN_BRACE, "`{'"))
    return error_mark_node;
  /* Begin the compound-statement.  */
  compound_stmt = begin_compound_stmt (/*has_no_scope=*/false);
  /* Parse an (optional) statement-seq.  */
  cp_parser_statement_seq_opt (parser, in_statement_expr_p);
  /* Finish the compound-statement.  */
  finish_compound_stmt (compound_stmt);
  /* Consume the `}'.  */
  cp_parser_require (parser, CPP_CLOSE_BRACE, "`}'");

  return compound_stmt;
}

/* Parse an (optional) statement-seq.

   statement-seq:
     statement
     statement-seq [opt] statement  */

static void
cp_parser_statement_seq_opt (cp_parser* parser, bool in_statement_expr_p)
{
  /* Scan statements until there aren't any more.  */
  while (true)
    {
      /* If we're looking at a `}', then we've run out of statements.  */
      if (cp_lexer_next_token_is (parser->lexer, CPP_CLOSE_BRACE)
	  || cp_lexer_next_token_is (parser->lexer, CPP_EOF))
	break;

      /* Parse the statement.  */
      cp_parser_statement (parser, in_statement_expr_p);
    }
}

/* Parse a selection-statement.

   selection-statement:
     if ( condition ) statement
     if ( condition ) statement else statement
     switch ( condition ) statement  

   Returns the new IF_STMT or SWITCH_STMT.  */

static tree
cp_parser_selection_statement (cp_parser* parser)
{
  cp_token *token;
  enum rid keyword;

  /* Peek at the next token.  */
  token = cp_parser_require (parser, CPP_KEYWORD, "selection-statement");

  /* See what kind of keyword it is.  */
  keyword = token->keyword;
  switch (keyword)
    {
    case RID_IF:
    case RID_SWITCH:
      {
	tree statement;
	tree condition;

	/* Look for the `('.  */
	if (!cp_parser_require (parser, CPP_OPEN_PAREN, "`('"))
	  {
	    cp_parser_skip_to_end_of_statement (parser);
	    return error_mark_node;
	  }

	/* Begin the selection-statement.  */
	if (keyword == RID_IF)
	  statement = begin_if_stmt ();
	else
	  statement = begin_switch_stmt ();

	/* Parse the condition.  */
	condition = cp_parser_condition (parser);
	/* Look for the `)'.  */
	if (!cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'"))
	  cp_parser_skip_to_closing_parenthesis (parser, true, false,
						 /*consume_paren=*/true);

	if (keyword == RID_IF)
	  {
	    tree then_stmt;

	    /* Add the condition.  */
	    finish_if_stmt_cond (condition, statement);

	    /* Parse the then-clause.  */
	    then_stmt = cp_parser_implicitly_scoped_statement (parser);
	    finish_then_clause (statement);

	    /* If the next token is `else', parse the else-clause.  */
	    if (cp_lexer_next_token_is_keyword (parser->lexer,
						RID_ELSE))
	      {
		tree else_stmt;

		/* Consume the `else' keyword.  */
		cp_lexer_consume_token (parser->lexer);
		/* Parse the else-clause.  */
		else_stmt 
		  = cp_parser_implicitly_scoped_statement (parser);
		finish_else_clause (statement);
	      }

	    /* Now we're all done with the if-statement.  */
	    finish_if_stmt ();
	  }
	else
	  {
	    tree body;
	    bool in_switch_statement_p;

	    /* Add the condition.  */
	    finish_switch_cond (condition, statement);

	    /* Parse the body of the switch-statement.  */
	    in_switch_statement_p = parser->in_switch_statement_p;
	    parser->in_switch_statement_p = true;
	    body = cp_parser_implicitly_scoped_statement (parser);
	    parser->in_switch_statement_p = in_switch_statement_p;

	    /* Now we're all done with the switch-statement.  */
	    finish_switch_stmt (statement);
	  }

	return statement;
      }
      break;

    default:
      cp_parser_error (parser, "expected selection-statement");
      return error_mark_node;
    }
}

/* Parse a condition. 

   condition:
     expression
     type-specifier-seq declarator = assignment-expression  

   GNU Extension:
   
   condition:
     type-specifier-seq declarator asm-specification [opt] 
       attributes [opt] = assignment-expression
 
   Returns the expression that should be tested.  */

static tree
cp_parser_condition (cp_parser* parser)
{
  tree type_specifiers;
  const char *saved_message;

  /* Try the declaration first.  */
  cp_parser_parse_tentatively (parser);
  /* New types are not allowed in the type-specifier-seq for a
     condition.  */
  saved_message = parser->type_definition_forbidden_message;
  parser->type_definition_forbidden_message
    = "types may not be defined in conditions";
  /* Parse the type-specifier-seq.  */
  type_specifiers = cp_parser_type_specifier_seq (parser);
  /* Restore the saved message.  */
  parser->type_definition_forbidden_message = saved_message;
  /* If all is well, we might be looking at a declaration.  */
  if (!cp_parser_error_occurred (parser))
    {
      tree decl;
      tree asm_specification;
      tree attributes;
      tree declarator;
      tree initializer = NULL_TREE;
      
      /* Parse the declarator.  */
      declarator = cp_parser_declarator (parser, CP_PARSER_DECLARATOR_NAMED,
					 /*ctor_dtor_or_conv_p=*/NULL,
					 /*parenthesized_p=*/NULL,
					 /*member_p=*/false);
      /* Parse the attributes.  */
      attributes = cp_parser_attributes_opt (parser);
      /* Parse the asm-specification.  */
      asm_specification = cp_parser_asm_specification_opt (parser);
      /* If the next token is not an `=', then we might still be
	 looking at an expression.  For example:
	 
	   if (A(a).x)
	  
	 looks like a decl-specifier-seq and a declarator -- but then
	 there is no `=', so this is an expression.  */
      cp_parser_require (parser, CPP_EQ, "`='");
      /* If we did see an `=', then we are looking at a declaration
	 for sure.  */
      if (cp_parser_parse_definitely (parser))
	{
	  /* Create the declaration.  */
	  decl = start_decl (declarator, type_specifiers, 
			     /*initialized_p=*/true,
			     attributes, /*prefix_attributes=*/NULL_TREE);
	  /* Parse the assignment-expression.  */
	  initializer = cp_parser_assignment_expression (parser);
	  
	  /* Process the initializer.  */
	  cp_finish_decl (decl, 
			  initializer, 
			  asm_specification, 
			  LOOKUP_ONLYCONVERTING);
	  
	  return convert_from_reference (decl);
	}
    }
  /* If we didn't even get past the declarator successfully, we are
     definitely not looking at a declaration.  */
  else
    cp_parser_abort_tentative_parse (parser);

  /* Otherwise, we are looking at an expression.  */
  return cp_parser_expression (parser);
}

/* Parse an iteration-statement.

   iteration-statement:
     while ( condition ) statement
     do statement while ( expression ) ;
     for ( for-init-statement condition [opt] ; expression [opt] )
       statement

   Returns the new WHILE_STMT, DO_STMT, or FOR_STMT.  */

static tree
cp_parser_iteration_statement (cp_parser* parser)
{
  cp_token *token;
  enum rid keyword;
  tree statement;
  bool in_iteration_statement_p;


  /* Peek at the next token.  */
  token = cp_parser_require (parser, CPP_KEYWORD, "iteration-statement");
  if (!token)
    return error_mark_node;

  /* Remember whether or not we are already within an iteration
     statement.  */ 
  in_iteration_statement_p = parser->in_iteration_statement_p;

  /* See what kind of keyword it is.  */
  keyword = token->keyword;
  switch (keyword)
    {
    case RID_WHILE:
      {
	tree condition;

	/* Begin the while-statement.  */
	statement = begin_while_stmt ();
	/* Look for the `('.  */
	cp_parser_require (parser, CPP_OPEN_PAREN, "`('");
	/* Parse the condition.  */
	condition = cp_parser_condition (parser);
	finish_while_stmt_cond (condition, statement);
	/* Look for the `)'.  */
	cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");
	/* Parse the dependent statement.  */
	parser->in_iteration_statement_p = true;
	cp_parser_already_scoped_statement (parser);
	parser->in_iteration_statement_p = in_iteration_statement_p;
	/* We're done with the while-statement.  */
	finish_while_stmt (statement);
      }
      break;

    case RID_DO:
      {
	tree expression;

	/* Begin the do-statement.  */
	statement = begin_do_stmt ();
	/* Parse the body of the do-statement.  */
	parser->in_iteration_statement_p = true;
	cp_parser_implicitly_scoped_statement (parser);
	parser->in_iteration_statement_p = in_iteration_statement_p;
	finish_do_body (statement);
	/* Look for the `while' keyword.  */
	cp_parser_require_keyword (parser, RID_WHILE, "`while'");
	/* Look for the `('.  */
	cp_parser_require (parser, CPP_OPEN_PAREN, "`('");
	/* Parse the expression.  */
	expression = cp_parser_expression (parser);
	/* We're done with the do-statement.  */
	finish_do_stmt (expression, statement);
	/* Look for the `)'.  */
	cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");
	/* Look for the `;'.  */
	cp_parser_require (parser, CPP_SEMICOLON, "`;'");
      }
      break;

    case RID_FOR:
      {
	tree condition = NULL_TREE;
	tree expression = NULL_TREE;

	/* Begin the for-statement.  */
	statement = begin_for_stmt ();
	/* Look for the `('.  */
	cp_parser_require (parser, CPP_OPEN_PAREN, "`('");
	/* Parse the initialization.  */
	cp_parser_for_init_statement (parser);
	finish_for_init_stmt (statement);

	/* If there's a condition, process it.  */
	if (cp_lexer_next_token_is_not (parser->lexer, CPP_SEMICOLON))
	  condition = cp_parser_condition (parser);
	finish_for_cond (condition, statement);
	/* Look for the `;'.  */
	cp_parser_require (parser, CPP_SEMICOLON, "`;'");

	/* If there's an expression, process it.  */
	if (cp_lexer_next_token_is_not (parser->lexer, CPP_CLOSE_PAREN))
	  expression = cp_parser_expression (parser);
	finish_for_expr (expression, statement);
	/* Look for the `)'.  */
	cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");
	
	/* Parse the body of the for-statement.  */
	parser->in_iteration_statement_p = true;
	cp_parser_already_scoped_statement (parser);
	parser->in_iteration_statement_p = in_iteration_statement_p;

	/* We're done with the for-statement.  */
	finish_for_stmt (statement);
      }
      break;

    default:
      cp_parser_error (parser, "expected iteration-statement");
      statement = error_mark_node;
      break;
    }

  return statement;
}

/* Parse a for-init-statement.

   for-init-statement:
     expression-statement
     simple-declaration  */

static void
cp_parser_for_init_statement (cp_parser* parser)
{
  /* If the next token is a `;', then we have an empty
     expression-statement.  Grammatically, this is also a
     simple-declaration, but an invalid one, because it does not
     declare anything.  Therefore, if we did not handle this case
     specially, we would issue an error message about an invalid
     declaration.  */
  if (cp_lexer_next_token_is_not (parser->lexer, CPP_SEMICOLON))
    {
      /* We're going to speculatively look for a declaration, falling back
	 to an expression, if necessary.  */
      cp_parser_parse_tentatively (parser);
      /* Parse the declaration.  */
      cp_parser_simple_declaration (parser,
				    /*function_definition_allowed_p=*/false);
      /* If the tentative parse failed, then we shall need to look for an
	 expression-statement.  */
      if (cp_parser_parse_definitely (parser))
	return;
    }

  cp_parser_expression_statement (parser, false);
}

/* Parse a jump-statement.

   jump-statement:
     break ;
     continue ;
     return expression [opt] ;
     goto identifier ;  

   GNU extension:

   jump-statement:
     goto * expression ;

   Returns the new BREAK_STMT, CONTINUE_STMT, RETURN_STMT, or
   GOTO_STMT.  */

static tree
cp_parser_jump_statement (cp_parser* parser)
{
  tree statement = error_mark_node;
  cp_token *token;
  enum rid keyword;

  /* Peek at the next token.  */
  token = cp_parser_require (parser, CPP_KEYWORD, "jump-statement");
  if (!token)
    return error_mark_node;

  /* See what kind of keyword it is.  */
  keyword = token->keyword;
  switch (keyword)
    {
    case RID_BREAK:
      if (!parser->in_switch_statement_p
	  && !parser->in_iteration_statement_p)
	{
	  error ("break statement not within loop or switch");
	  statement = error_mark_node;
	}
      else
	statement = finish_break_stmt ();
      cp_parser_require (parser, CPP_SEMICOLON, "`;'");
      break;

    case RID_CONTINUE:
      if (!parser->in_iteration_statement_p)
	{
	  error ("continue statement not within a loop");
	  statement = error_mark_node;
	}
      else
	statement = finish_continue_stmt ();
      cp_parser_require (parser, CPP_SEMICOLON, "`;'");
      break;

    case RID_RETURN:
      {
	tree expr;

	/* If the next token is a `;', then there is no 
	   expression.  */
	if (cp_lexer_next_token_is_not (parser->lexer, CPP_SEMICOLON))
	  expr = cp_parser_expression (parser);
	else
	  expr = NULL_TREE;
	/* Build the return-statement.  */
	statement = finish_return_stmt (expr);
	/* Look for the final `;'.  */
	cp_parser_require (parser, CPP_SEMICOLON, "`;'");
      }
      break;

    case RID_GOTO:
      /* Create the goto-statement.  */
      if (cp_lexer_next_token_is (parser->lexer, CPP_MULT))
	{
	  /* Issue a warning about this use of a GNU extension.  */
	  if (pedantic)
	    pedwarn ("ISO C++ forbids computed gotos");
	  /* Consume the '*' token.  */
	  cp_lexer_consume_token (parser->lexer);
	  /* Parse the dependent expression.  */
	  finish_goto_stmt (cp_parser_expression (parser));
	}
      else
	finish_goto_stmt (cp_parser_identifier (parser));
      /* Look for the final `;'.  */
      cp_parser_require (parser, CPP_SEMICOLON, "`;'");
      break;

    default:
      cp_parser_error (parser, "expected jump-statement");
      break;
    }

  return statement;
}

/* Parse a declaration-statement.

   declaration-statement:
     block-declaration  */

static void
cp_parser_declaration_statement (cp_parser* parser)
{
  /* Parse the block-declaration.  */
  cp_parser_block_declaration (parser, /*statement_p=*/true);

  /* Finish off the statement.  */
  finish_stmt ();
}

/* Some dependent statements (like `if (cond) statement'), are
   implicitly in their own scope.  In other words, if the statement is
   a single statement (as opposed to a compound-statement), it is
   none-the-less treated as if it were enclosed in braces.  Any
   declarations appearing in the dependent statement are out of scope
   after control passes that point.  This function parses a statement,
   but ensures that is in its own scope, even if it is not a
   compound-statement.  

   Returns the new statement.  */

static tree
cp_parser_implicitly_scoped_statement (cp_parser* parser)
{
  tree statement;

  /* If the token is not a `{', then we must take special action.  */
  if (cp_lexer_next_token_is_not (parser->lexer, CPP_OPEN_BRACE))
    {
      /* Create a compound-statement.  */
      statement = begin_compound_stmt (/*has_no_scope=*/false);
      /* Parse the dependent-statement.  */
      cp_parser_statement (parser, false);
      /* Finish the dummy compound-statement.  */
      finish_compound_stmt (statement);
    }
  /* Otherwise, we simply parse the statement directly.  */
  else
    statement = cp_parser_compound_statement (parser, false);

  /* Return the statement.  */
  return statement;
}

/* For some dependent statements (like `while (cond) statement'), we
   have already created a scope.  Therefore, even if the dependent
   statement is a compound-statement, we do not want to create another
   scope.  */

static void
cp_parser_already_scoped_statement (cp_parser* parser)
{
  /* If the token is not a `{', then we must take special action.  */
  if (cp_lexer_next_token_is_not(parser->lexer, CPP_OPEN_BRACE))
    {
      tree statement;

      /* Create a compound-statement.  */
      statement = begin_compound_stmt (/*has_no_scope=*/true);
      /* Parse the dependent-statement.  */
      cp_parser_statement (parser, false);
      /* Finish the dummy compound-statement.  */
      finish_compound_stmt (statement);
    }
  /* Otherwise, we simply parse the statement directly.  */
  else
    cp_parser_statement (parser, false);
}

/* Declarations [gram.dcl.dcl] */

/* Parse an optional declaration-sequence.

   declaration-seq:
     declaration
     declaration-seq declaration  */

static void
cp_parser_declaration_seq_opt (cp_parser* parser)
{
  while (true)
    {
      cp_token *token;

      token = cp_lexer_peek_token (parser->lexer);

      if (token->type == CPP_CLOSE_BRACE
	  || token->type == CPP_EOF)
	break;

      if (token->type == CPP_SEMICOLON) 
	{
	  /* A declaration consisting of a single semicolon is
	     invalid.  Allow it unless we're being pedantic.  */
	  if (pedantic && !in_system_header)
	    pedwarn ("extra `;'");
	  cp_lexer_consume_token (parser->lexer);
	  continue;
	}

      /* The C lexer modifies PENDING_LANG_CHANGE when it wants the
	 parser to enter or exit implicit `extern "C"' blocks.  */
      while (pending_lang_change > 0)
	{
	  push_lang_context (lang_name_c);
	  --pending_lang_change;
	}
      while (pending_lang_change < 0)
	{
	  pop_lang_context ();
	  ++pending_lang_change;
	}

      /* Parse the declaration itself.  */
      cp_parser_declaration (parser);
    }
}

/* Parse a declaration.

   declaration:
     block-declaration
     function-definition
     template-declaration
     explicit-instantiation
     explicit-specialization
     linkage-specification
     namespace-definition    

   GNU extension:

   declaration:
      __extension__ declaration */

static void
cp_parser_declaration (cp_parser* parser)
{
  cp_token token1;
  cp_token token2;
  int saved_pedantic;

  /* Check for the `__extension__' keyword.  */
  if (cp_parser_extension_opt (parser, &saved_pedantic))
    {
      /* Parse the qualified declaration.  */
      cp_parser_declaration (parser);
      /* Restore the PEDANTIC flag.  */
      pedantic = saved_pedantic;

      return;
    }

  /* Try to figure out what kind of declaration is present.  */
  token1 = *cp_lexer_peek_token (parser->lexer);
  if (token1.type != CPP_EOF)
    token2 = *cp_lexer_peek_nth_token (parser->lexer, 2);

  /* If the next token is `extern' and the following token is a string
     literal, then we have a linkage specification.  */
  if (token1.keyword == RID_EXTERN
      && cp_parser_is_string_literal (&token2))
    cp_parser_linkage_specification (parser);
  /* If the next token is `template', then we have either a template
     declaration, an explicit instantiation, or an explicit
     specialization.  */
  else if (token1.keyword == RID_TEMPLATE)
    {
      /* `template <>' indicates a template specialization.  */
      if (token2.type == CPP_LESS
	  && cp_lexer_peek_nth_token (parser->lexer, 3)->type == CPP_GREATER)
	cp_parser_explicit_specialization (parser);
      /* `template <' indicates a template declaration.  */
      else if (token2.type == CPP_LESS)
	cp_parser_template_declaration (parser, /*member_p=*/false);
      /* Anything else must be an explicit instantiation.  */
      else
	cp_parser_explicit_instantiation (parser);
    }
  /* If the next token is `export', then we have a template
     declaration.  */
  else if (token1.keyword == RID_EXPORT)
    cp_parser_template_declaration (parser, /*member_p=*/false);
  /* If the next token is `extern', 'static' or 'inline' and the one
     after that is `template', we have a GNU extended explicit
     instantiation directive.  */
  else if (cp_parser_allow_gnu_extensions_p (parser)
	   && (token1.keyword == RID_EXTERN
	       || token1.keyword == RID_STATIC
	       || token1.keyword == RID_INLINE)
	   && token2.keyword == RID_TEMPLATE)
    cp_parser_explicit_instantiation (parser);
  /* If the next token is `namespace', check for a named or unnamed
     namespace definition.  */
  else if (token1.keyword == RID_NAMESPACE
	   && (/* A named namespace definition.  */
	       (token2.type == CPP_NAME
		&& (cp_lexer_peek_nth_token (parser->lexer, 3)->type 
		    == CPP_OPEN_BRACE))
	       /* An unnamed namespace definition.  */
	       || token2.type == CPP_OPEN_BRACE))
    cp_parser_namespace_definition (parser);
  /* We must have either a block declaration or a function
     definition.  */
  else
    /* Try to parse a block-declaration, or a function-definition.  */
    cp_parser_block_declaration (parser, /*statement_p=*/false);
}

/* Parse a block-declaration.  

   block-declaration:
     simple-declaration
     asm-definition
     namespace-alias-definition
     using-declaration
     using-directive  

   GNU Extension:

   block-declaration:
     __extension__ block-declaration 
     label-declaration

   If STATEMENT_P is TRUE, then this block-declaration is occurring as
   part of a declaration-statement.  */

static void
cp_parser_block_declaration (cp_parser *parser, 
			     bool      statement_p)
{
  cp_token *token1;
  int saved_pedantic;

  /* Check for the `__extension__' keyword.  */
  if (cp_parser_extension_opt (parser, &saved_pedantic))
    {
      /* Parse the qualified declaration.  */
      cp_parser_block_declaration (parser, statement_p);
      /* Restore the PEDANTIC flag.  */
      pedantic = saved_pedantic;

      return;
    }

  /* Peek at the next token to figure out which kind of declaration is
     present.  */
  token1 = cp_lexer_peek_token (parser->lexer);

  /* If the next keyword is `asm', we have an asm-definition.  */
  if (token1->keyword == RID_ASM)
    {
      if (statement_p)
	cp_parser_commit_to_tentative_parse (parser);
      cp_parser_asm_definition (parser);
    }
  /* If the next keyword is `namespace', we have a
     namespace-alias-definition.  */
  else if (token1->keyword == RID_NAMESPACE)
    cp_parser_namespace_alias_definition (parser);
  /* If the next keyword is `using', we have either a
     using-declaration or a using-directive.  */
  else if (token1->keyword == RID_USING)
    {
      cp_token *token2;

      if (statement_p)
	cp_parser_commit_to_tentative_parse (parser);
      /* If the token after `using' is `namespace', then we have a
	 using-directive.  */
      token2 = cp_lexer_peek_nth_token (parser->lexer, 2);
      if (token2->keyword == RID_NAMESPACE)
	cp_parser_using_directive (parser);
      /* Otherwise, it's a using-declaration.  */
      else
	cp_parser_using_declaration (parser);
    }
  /* If the next keyword is `__label__' we have a label declaration.  */
  else if (token1->keyword == RID_LABEL)
    {
      if (statement_p)
	cp_parser_commit_to_tentative_parse (parser);
      cp_parser_label_declaration (parser);
    }
  /* Anything else must be a simple-declaration.  */
  else
    cp_parser_simple_declaration (parser, !statement_p);
}

/* Parse a simple-declaration.

   simple-declaration:
     decl-specifier-seq [opt] init-declarator-list [opt] ;  

   init-declarator-list:
     init-declarator
     init-declarator-list , init-declarator 

   If FUNCTION_DEFINITION_ALLOWED_P is TRUE, then we also recognize a
   function-definition as a simple-declaration.  */

static void
cp_parser_simple_declaration (cp_parser* parser, 
                              bool function_definition_allowed_p)
{
  tree decl_specifiers;
  tree attributes;
  int declares_class_or_enum;
  bool saw_declarator;

  /* Defer access checks until we know what is being declared; the
     checks for names appearing in the decl-specifier-seq should be
     done as if we were in the scope of the thing being declared.  */
  push_deferring_access_checks (dk_deferred);

  /* Parse the decl-specifier-seq.  We have to keep track of whether
     or not the decl-specifier-seq declares a named class or
     enumeration type, since that is the only case in which the
     init-declarator-list is allowed to be empty.  

     [dcl.dcl]

     In a simple-declaration, the optional init-declarator-list can be
     omitted only when declaring a class or enumeration, that is when
     the decl-specifier-seq contains either a class-specifier, an
     elaborated-type-specifier, or an enum-specifier.  */
  decl_specifiers
    = cp_parser_decl_specifier_seq (parser, 
				    CP_PARSER_FLAGS_OPTIONAL,
				    &attributes,
				    &declares_class_or_enum);
  /* We no longer need to defer access checks.  */
  stop_deferring_access_checks ();

  /* In a block scope, a valid declaration must always have a
     decl-specifier-seq.  By not trying to parse declarators, we can
     resolve the declaration/expression ambiguity more quickly.  */
  if (!function_definition_allowed_p && !decl_specifiers)
    {
      cp_parser_error (parser, "expected declaration");
      goto done;
    }

  /* If the next two tokens are both identifiers, the code is
     erroneous. The usual cause of this situation is code like:

       T t;

     where "T" should name a type -- but does not.  */
  if (cp_parser_diagnose_invalid_type_name (parser))
    {
      /* If parsing tentatively, we should commit; we really are
	 looking at a declaration.  */
      cp_parser_commit_to_tentative_parse (parser);
      /* Give up.  */
      goto done;
    }
  
  /* If we have seen at least one decl-specifier, and the next token
     is not a parenthesis, then we must be looking at a declaration.
     (After "int (" we might be looking at a functional cast.)  */
  if (decl_specifiers
      && cp_lexer_next_token_is_not (parser->lexer, CPP_OPEN_PAREN))
    cp_parser_commit_to_tentative_parse (parser);

  /* Keep going until we hit the `;' at the end of the simple
     declaration.  */
  saw_declarator = false;
  while (cp_lexer_next_token_is_not (parser->lexer, 
				     CPP_SEMICOLON))
    {
      cp_token *token;
      bool function_definition_p;
      tree decl;

      saw_declarator = true;
      /* Parse the init-declarator.  */
      decl = cp_parser_init_declarator (parser, decl_specifiers, attributes,
					function_definition_allowed_p,
					/*member_p=*/false,
					declares_class_or_enum,
					&function_definition_p);
      /* If an error occurred while parsing tentatively, exit quickly.
	 (That usually happens when in the body of a function; each
	 statement is treated as a declaration-statement until proven
	 otherwise.)  */
      if (cp_parser_error_occurred (parser))
	goto done;
      /* Handle function definitions specially.  */
      if (function_definition_p)
	{
	  /* If the next token is a `,', then we are probably
	     processing something like:

	       void f() {}, *p;

	     which is erroneous.  */
	  if (cp_lexer_next_token_is (parser->lexer, CPP_COMMA))
	    error ("mixing declarations and function-definitions is forbidden");
	  /* Otherwise, we're done with the list of declarators.  */
	  else
	    {
	      pop_deferring_access_checks ();
	      return;
	    }
	}
      /* The next token should be either a `,' or a `;'.  */
      token = cp_lexer_peek_token (parser->lexer);
      /* If it's a `,', there are more declarators to come.  */
      if (token->type == CPP_COMMA)
	cp_lexer_consume_token (parser->lexer);
      /* If it's a `;', we are done.  */
      else if (token->type == CPP_SEMICOLON)
	break;
      /* Anything else is an error.  */
      else
	{
	  /* If we have already issued an error message we don't need
	     to issue another one.  */
	  if (decl != error_mark_node
	      || (cp_parser_parsing_tentatively (parser)
		  && !cp_parser_committed_to_tentative_parse (parser)))
	    cp_parser_error (parser, "expected `,' or `;'");
	  /* Skip tokens until we reach the end of the statement.  */
	  cp_parser_skip_to_end_of_statement (parser);
	  /* If the next token is now a `;', consume it.  */
	  if (cp_lexer_next_token_is (parser->lexer, CPP_SEMICOLON))
	    cp_lexer_consume_token (parser->lexer);
	  goto done;
	}
      /* After the first time around, a function-definition is not
	 allowed -- even if it was OK at first.  For example:

           int i, f() {}

         is not valid.  */
      function_definition_allowed_p = false;
    }

  /* Issue an error message if no declarators are present, and the
     decl-specifier-seq does not itself declare a class or
     enumeration.  */
  if (!saw_declarator)
    {
      if (cp_parser_declares_only_class_p (parser))
	shadow_tag (decl_specifiers);
      /* Perform any deferred access checks.  */
      perform_deferred_access_checks ();
    }

  /* Consume the `;'.  */
  cp_parser_require (parser, CPP_SEMICOLON, "`;'");

 done:
  pop_deferring_access_checks ();
}

/* Parse a decl-specifier-seq.

   decl-specifier-seq:
     decl-specifier-seq [opt] decl-specifier

   decl-specifier:
     storage-class-specifier
     type-specifier
     function-specifier
     friend
     typedef  

   GNU Extension:

   decl-specifier:
     attributes

   Returns a TREE_LIST, giving the decl-specifiers in the order they
   appear in the source code.  The TREE_VALUE of each node is the
   decl-specifier.  For a keyword (such as `auto' or `friend'), the
   TREE_VALUE is simply the corresponding TREE_IDENTIFIER.  For the
   representation of a type-specifier, see cp_parser_type_specifier.  

   If there are attributes, they will be stored in *ATTRIBUTES,
   represented as described above cp_parser_attributes.  

   If FRIEND_IS_NOT_CLASS_P is non-NULL, and the `friend' specifier
   appears, and the entity that will be a friend is not going to be a
   class, then *FRIEND_IS_NOT_CLASS_P will be set to TRUE.  Note that
   even if *FRIEND_IS_NOT_CLASS_P is FALSE, the entity to which
   friendship is granted might not be a class.  

   *DECLARES_CLASS_OR_ENUM is set to the bitwise or of the following
   flags:

     1: one of the decl-specifiers is an elaborated-type-specifier
        (i.e., a type declaration)
     2: one of the decl-specifiers is an enum-specifier or a
        class-specifier (i.e., a type definition)

   */

static tree
cp_parser_decl_specifier_seq (cp_parser* parser, 
                              cp_parser_flags flags, 
                              tree* attributes,
			      int* declares_class_or_enum)
{
  tree decl_specs = NULL_TREE;
  bool friend_p = false;
  bool constructor_possible_p = !parser->in_declarator_p;
  
  /* Assume no class or enumeration type is declared.  */
  *declares_class_or_enum = 0;

  /* Assume there are no attributes.  */
  *attributes = NULL_TREE;

  /* Keep reading specifiers until there are no more to read.  */
  while (true)
    {
      tree decl_spec = NULL_TREE;
      bool constructor_p;
      cp_token *token;

      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);
      /* Handle attributes.  */
      if (token->keyword == RID_ATTRIBUTE)
	{
	  /* Parse the attributes.  */
	  decl_spec = cp_parser_attributes_opt (parser);
	  /* Add them to the list.  */
	  *attributes = chainon (*attributes, decl_spec);
	  continue;
	}
      /* If the next token is an appropriate keyword, we can simply
	 add it to the list.  */
      switch (token->keyword)
	{
	case RID_FRIEND:
	  /* decl-specifier:
	       friend  */
	  if (friend_p)
	    error ("duplicate `friend'");
	  else
	    friend_p = true;
	  /* The representation of the specifier is simply the
	     appropriate TREE_IDENTIFIER node.  */
	  decl_spec = token->value;
	  /* Consume the token.  */
	  cp_lexer_consume_token (parser->lexer);
	  break;

	  /* function-specifier:
	       inline
	       virtual
	       explicit  */
	case RID_INLINE:
	case RID_VIRTUAL:
	case RID_EXPLICIT:
	  decl_spec = cp_parser_function_specifier_opt (parser);
	  break;
	  
	  /* decl-specifier:
	       typedef  */
	case RID_TYPEDEF:
	  /* The representation of the specifier is simply the
	     appropriate TREE_IDENTIFIER node.  */
	  decl_spec = token->value;
	  /* Consume the token.  */
	  cp_lexer_consume_token (parser->lexer);
	  /* A constructor declarator cannot appear in a typedef.  */
	  constructor_possible_p = false;
	  /* The "typedef" keyword can only occur in a declaration; we
	     may as well commit at this point.  */
	  cp_parser_commit_to_tentative_parse (parser);
	  break;

	  /* storage-class-specifier:
	       auto
	       register
	       static
	       extern
	       mutable  

             GNU Extension:
	       thread  */
	case RID_AUTO:
	case RID_REGISTER:
	case RID_STATIC:
	case RID_EXTERN:
	case RID_MUTABLE:
	case RID_THREAD:
	  decl_spec = cp_parser_storage_class_specifier_opt (parser);
	  break;
	  
	default:
	  break;
	}

      /* Constructors are a special case.  The `S' in `S()' is not a
	 decl-specifier; it is the beginning of the declarator.  */
      constructor_p = (!decl_spec 
		       && constructor_possible_p
		       && cp_parser_constructor_declarator_p (parser,
							      friend_p));

      /* If we don't have a DECL_SPEC yet, then we must be looking at
	 a type-specifier.  */
      if (!decl_spec && !constructor_p)
	{
	  int decl_spec_declares_class_or_enum;
	  bool is_cv_qualifier;

	  decl_spec
	    = cp_parser_type_specifier (parser, flags,
					friend_p,
					/*is_declaration=*/true,
					&decl_spec_declares_class_or_enum,
					&is_cv_qualifier);

	  *declares_class_or_enum |= decl_spec_declares_class_or_enum;

	  /* If this type-specifier referenced a user-defined type
	     (a typedef, class-name, etc.), then we can't allow any
	     more such type-specifiers henceforth.

	     [dcl.spec]

	     The longest sequence of decl-specifiers that could
	     possibly be a type name is taken as the
	     decl-specifier-seq of a declaration.  The sequence shall
	     be self-consistent as described below.

	     [dcl.type]

	     As a general rule, at most one type-specifier is allowed
	     in the complete decl-specifier-seq of a declaration.  The
	     only exceptions are the following:

	     -- const or volatile can be combined with any other
		type-specifier. 

	     -- signed or unsigned can be combined with char, long,
		short, or int.

	     -- ..

	     Example:

	       typedef char* Pc;
	       void g (const int Pc);

	     Here, Pc is *not* part of the decl-specifier seq; it's
	     the declarator.  Therefore, once we see a type-specifier
	     (other than a cv-qualifier), we forbid any additional
	     user-defined types.  We *do* still allow things like `int
	     int' to be considered a decl-specifier-seq, and issue the
	     error message later.  */
	  if (decl_spec && !is_cv_qualifier)
	    flags |= CP_PARSER_FLAGS_NO_USER_DEFINED_TYPES;
	  /* A constructor declarator cannot follow a type-specifier.  */
	  if (decl_spec)
	    constructor_possible_p = false;
	}

      /* If we still do not have a DECL_SPEC, then there are no more
	 decl-specifiers.  */
      if (!decl_spec)
	{
	  /* Issue an error message, unless the entire construct was
             optional.  */
	  if (!(flags & CP_PARSER_FLAGS_OPTIONAL))
	    {
	      cp_parser_error (parser, "expected decl specifier");
	      return error_mark_node;
	    }

	  break;
	}

      /* Add the DECL_SPEC to the list of specifiers.  */
      if (decl_specs == NULL || TREE_VALUE (decl_specs) != error_mark_node)
	decl_specs = tree_cons (NULL_TREE, decl_spec, decl_specs);

      /* After we see one decl-specifier, further decl-specifiers are
	 always optional.  */
      flags |= CP_PARSER_FLAGS_OPTIONAL;
    }

  /* Don't allow a friend specifier with a class definition.  */
  if (friend_p && (*declares_class_or_enum & 2))
    error ("class definition may not be declared a friend");

  /* We have built up the DECL_SPECS in reverse order.  Return them in
     the correct order.  */
  return nreverse (decl_specs);
}

/* Parse an (optional) storage-class-specifier. 

   storage-class-specifier:
     auto
     register
     static
     extern
     mutable  

   GNU Extension:

   storage-class-specifier:
     thread

   Returns an IDENTIFIER_NODE corresponding to the keyword used.  */
   
static tree
cp_parser_storage_class_specifier_opt (cp_parser* parser)
{
  switch (cp_lexer_peek_token (parser->lexer)->keyword)
    {
    case RID_AUTO:
    case RID_REGISTER:
    case RID_STATIC:
    case RID_EXTERN:
    case RID_MUTABLE:
    case RID_THREAD:
      /* Consume the token.  */
      return cp_lexer_consume_token (parser->lexer)->value;

    default:
      return NULL_TREE;
    }
}

/* Parse an (optional) function-specifier. 

   function-specifier:
     inline
     virtual
     explicit

   Returns an IDENTIFIER_NODE corresponding to the keyword used.  */
   
static tree
cp_parser_function_specifier_opt (cp_parser* parser)
{
  switch (cp_lexer_peek_token (parser->lexer)->keyword)
    {
    case RID_INLINE:
    case RID_VIRTUAL:
    case RID_EXPLICIT:
      /* Consume the token.  */
      return cp_lexer_consume_token (parser->lexer)->value;

    default:
      return NULL_TREE;
    }
}

/* Parse a linkage-specification.

   linkage-specification:
     extern string-literal { declaration-seq [opt] }
     extern string-literal declaration  */

static void
cp_parser_linkage_specification (cp_parser* parser)
{
  cp_token *token;
  tree linkage;

  /* Look for the `extern' keyword.  */
  cp_parser_require_keyword (parser, RID_EXTERN, "`extern'");

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* If it's not a string-literal, then there's a problem.  */
  if (!cp_parser_is_string_literal (token))
    {
      cp_parser_error (parser, "expected language-name");
      return;
    }
  /* Consume the token.  */
  cp_lexer_consume_token (parser->lexer);

  /* Transform the literal into an identifier.  If the literal is a
     wide-character string, or contains embedded NULs, then we can't
     handle it as the user wants.  */
  if (token->type == CPP_WSTRING
      || (strlen (TREE_STRING_POINTER (token->value))
	  != (size_t) (TREE_STRING_LENGTH (token->value) - 1)))
    {
      cp_parser_error (parser, "invalid linkage-specification");
      /* Assume C++ linkage.  */
      linkage = get_identifier ("c++");
    }
  /* If it's a simple string constant, things are easier.  */
  else
    linkage = get_identifier (TREE_STRING_POINTER (token->value));

  /* We're now using the new linkage.  */
  push_lang_context (linkage);

  /* If the next token is a `{', then we're using the first
     production.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_OPEN_BRACE))
    {
      /* Consume the `{' token.  */
      cp_lexer_consume_token (parser->lexer);
      /* Parse the declarations.  */
      cp_parser_declaration_seq_opt (parser);
      /* Look for the closing `}'.  */
      cp_parser_require (parser, CPP_CLOSE_BRACE, "`}'");
    }
  /* Otherwise, there's just one declaration.  */
  else
    {
      bool saved_in_unbraced_linkage_specification_p;

      saved_in_unbraced_linkage_specification_p 
	= parser->in_unbraced_linkage_specification_p;
      parser->in_unbraced_linkage_specification_p = true;
      have_extern_spec = true;
      cp_parser_declaration (parser);
      have_extern_spec = false;
      parser->in_unbraced_linkage_specification_p 
	= saved_in_unbraced_linkage_specification_p;
    }

  /* We're done with the linkage-specification.  */
  pop_lang_context ();
}

/* Special member functions [gram.special] */

/* Parse a conversion-function-id.

   conversion-function-id:
     operator conversion-type-id  

   Returns an IDENTIFIER_NODE representing the operator.  */

static tree 
cp_parser_conversion_function_id (cp_parser* parser)
{
  tree type;
  tree saved_scope;
  tree saved_qualifying_scope;
  tree saved_object_scope;
  bool pop_p = false;

  /* Look for the `operator' token.  */
  if (!cp_parser_require_keyword (parser, RID_OPERATOR, "`operator'"))
    return error_mark_node;
  /* When we parse the conversion-type-id, the current scope will be
     reset.  However, we need that information in able to look up the
     conversion function later, so we save it here.  */
  saved_scope = parser->scope;
  saved_qualifying_scope = parser->qualifying_scope;
  saved_object_scope = parser->object_scope;
  /* We must enter the scope of the class so that the names of
     entities declared within the class are available in the
     conversion-type-id.  For example, consider:

       struct S { 
         typedef int I;
	 operator I();
       };

       S::operator I() { ... }

     In order to see that `I' is a type-name in the definition, we
     must be in the scope of `S'.  */
  if (saved_scope)
    pop_p = push_scope (saved_scope);
  /* Parse the conversion-type-id.  */
  type = cp_parser_conversion_type_id (parser);
  /* Leave the scope of the class, if any.  */
  if (pop_p)
    pop_scope (saved_scope);
  /* Restore the saved scope.  */
  parser->scope = saved_scope;
  parser->qualifying_scope = saved_qualifying_scope;
  parser->object_scope = saved_object_scope;
  /* If the TYPE is invalid, indicate failure.  */
  if (type == error_mark_node)
    return error_mark_node;
  return mangle_conv_op_name_for_type (type);
}

/* Parse a conversion-type-id:

   conversion-type-id:
     type-specifier-seq conversion-declarator [opt]

   Returns the TYPE specified.  */

static tree
cp_parser_conversion_type_id (cp_parser* parser)
{
  tree attributes;
  tree type_specifiers;
  tree declarator;

  /* Parse the attributes.  */
  attributes = cp_parser_attributes_opt (parser);
  /* Parse the type-specifiers.  */
  type_specifiers = cp_parser_type_specifier_seq (parser);
  /* If that didn't work, stop.  */
  if (type_specifiers == error_mark_node)
    return error_mark_node;
  /* Parse the conversion-declarator.  */
  declarator = cp_parser_conversion_declarator_opt (parser);

  return grokdeclarator (declarator, type_specifiers, TYPENAME,
			 /*initialized=*/0, &attributes);
}

/* Parse an (optional) conversion-declarator.

   conversion-declarator:
     ptr-operator conversion-declarator [opt]  

   Returns a representation of the declarator.  See
   cp_parser_declarator for details.  */

static tree
cp_parser_conversion_declarator_opt (cp_parser* parser)
{
  enum tree_code code;
  tree class_type;
  tree cv_qualifier_seq;

  /* We don't know if there's a ptr-operator next, or not.  */
  cp_parser_parse_tentatively (parser);
  /* Try the ptr-operator.  */
  code = cp_parser_ptr_operator (parser, &class_type, 
				 &cv_qualifier_seq);
  /* If it worked, look for more conversion-declarators.  */
  if (cp_parser_parse_definitely (parser))
    {
     tree declarator;

     /* Parse another optional declarator.  */
     declarator = cp_parser_conversion_declarator_opt (parser);

     /* Create the representation of the declarator.  */
     if (code == INDIRECT_REF)
       declarator = make_pointer_declarator (cv_qualifier_seq,
					     declarator);
     else
       declarator =  make_reference_declarator (cv_qualifier_seq,
						declarator);

     /* Handle the pointer-to-member case.  */
     if (class_type)
       declarator = build_nt (SCOPE_REF, class_type, declarator);

     return declarator;
   }

  return NULL_TREE;
}

/* Parse an (optional) ctor-initializer.

   ctor-initializer:
     : mem-initializer-list  

   Returns TRUE iff the ctor-initializer was actually present.  */

static bool
cp_parser_ctor_initializer_opt (cp_parser* parser)
{
  /* If the next token is not a `:', then there is no
     ctor-initializer.  */
  if (cp_lexer_next_token_is_not (parser->lexer, CPP_COLON))
    {
      /* Do default initialization of any bases and members.  */
      if (DECL_CONSTRUCTOR_P (current_function_decl))
	finish_mem_initializers (NULL_TREE);

      return false;
    }

  /* Consume the `:' token.  */
  cp_lexer_consume_token (parser->lexer);
  /* And the mem-initializer-list.  */
  cp_parser_mem_initializer_list (parser);

  return true;
}

/* Parse a mem-initializer-list.

   mem-initializer-list:
     mem-initializer
     mem-initializer , mem-initializer-list  */

static void
cp_parser_mem_initializer_list (cp_parser* parser)
{
  tree mem_initializer_list = NULL_TREE;

  /* Let the semantic analysis code know that we are starting the
     mem-initializer-list.  */
  if (!DECL_CONSTRUCTOR_P (current_function_decl))
    error ("only constructors take base initializers");

  /* Loop through the list.  */
  while (true)
    {
      tree mem_initializer;

      /* Parse the mem-initializer.  */
      mem_initializer = cp_parser_mem_initializer (parser);
      /* Add it to the list, unless it was erroneous.  */
      if (mem_initializer)
	{
	  TREE_CHAIN (mem_initializer) = mem_initializer_list;
	  mem_initializer_list = mem_initializer;
	}
      /* If the next token is not a `,', we're done.  */
      if (cp_lexer_next_token_is_not (parser->lexer, CPP_COMMA))
	break;
      /* Consume the `,' token.  */
      cp_lexer_consume_token (parser->lexer);
    }

  /* Perform semantic analysis.  */
  if (DECL_CONSTRUCTOR_P (current_function_decl))
    finish_mem_initializers (mem_initializer_list);
}

/* Parse a mem-initializer.

   mem-initializer:
     mem-initializer-id ( expression-list [opt] )  

   GNU extension:
  
   mem-initializer:
     ( expression-list [opt] )

   Returns a TREE_LIST.  The TREE_PURPOSE is the TYPE (for a base
   class) or FIELD_DECL (for a non-static data member) to initialize;
   the TREE_VALUE is the expression-list.  */

static tree
cp_parser_mem_initializer (cp_parser* parser)
{
  tree mem_initializer_id;
  tree expression_list;
  tree member;
  
  /* Find out what is being initialized.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_OPEN_PAREN))
    {
      pedwarn ("anachronistic old-style base class initializer");
      mem_initializer_id = NULL_TREE;
    }
  else
    mem_initializer_id = cp_parser_mem_initializer_id (parser);
  member = expand_member_init (mem_initializer_id);
  if (member && !DECL_P (member))
    in_base_initializer = 1;

  expression_list 
    = cp_parser_parenthesized_expression_list (parser, false,
					       /*non_constant_p=*/NULL);
  if (!expression_list)
    expression_list = void_type_node;

  in_base_initializer = 0;
  
  return member ? build_tree_list (member, expression_list) : NULL_TREE;
}

/* Parse a mem-initializer-id.

   mem-initializer-id:
     :: [opt] nested-name-specifier [opt] class-name
     identifier  

   Returns a TYPE indicating the class to be initializer for the first
   production.  Returns an IDENTIFIER_NODE indicating the data member
   to be initialized for the second production.  */

static tree
cp_parser_mem_initializer_id (cp_parser* parser)
{
  bool global_scope_p;
  bool nested_name_specifier_p;
  bool template_p = false;
  tree id;

  /* `typename' is not allowed in this context ([temp.res]).  */
  if (cp_lexer_next_token_is_keyword (parser->lexer, RID_TYPENAME))
    {
      error ("keyword `typename' not allowed in this context (a qualified "
	     "member initializer is implicitly a type)");
      cp_lexer_consume_token (parser->lexer);
    }
  /* Look for the optional `::' operator.  */
  global_scope_p 
    = (cp_parser_global_scope_opt (parser, 
				   /*current_scope_valid_p=*/false) 
       != NULL_TREE);
  /* Look for the optional nested-name-specifier.  The simplest way to
     implement:

       [temp.res]

       The keyword `typename' is not permitted in a base-specifier or
       mem-initializer; in these contexts a qualified name that
       depends on a template-parameter is implicitly assumed to be a
       type name.

     is to assume that we have seen the `typename' keyword at this
     point.  */
  nested_name_specifier_p 
    = (cp_parser_nested_name_specifier_opt (parser,
					    /*typename_keyword_p=*/true,
					    /*check_dependency_p=*/true,
					    /*type_p=*/true,
					    /*is_declaration=*/true)
       != NULL_TREE);
  if (nested_name_specifier_p)
    template_p = cp_parser_optional_template_keyword (parser);
  /* If there is a `::' operator or a nested-name-specifier, then we
     are definitely looking for a class-name.  */
  if (global_scope_p || nested_name_specifier_p)
    return cp_parser_class_name (parser,
				 /*typename_keyword_p=*/true,
				 /*template_keyword_p=*/template_p,
				 /*type_p=*/false,
				 /*check_dependency_p=*/true,
				 /*class_head_p=*/false,
				 /*is_declaration=*/true);
  /* Otherwise, we could also be looking for an ordinary identifier.  */
  cp_parser_parse_tentatively (parser);
  /* Try a class-name.  */
  id = cp_parser_class_name (parser, 
			     /*typename_keyword_p=*/true,
			     /*template_keyword_p=*/false,
			     /*type_p=*/false,
			     /*check_dependency_p=*/true,
			     /*class_head_p=*/false,
			     /*is_declaration=*/true);
  /* If we found one, we're done.  */
  if (cp_parser_parse_definitely (parser))
    return id;
  /* Otherwise, look for an ordinary identifier.  */
  return cp_parser_identifier (parser);
}

/* Overloading [gram.over] */

/* Parse an operator-function-id.

   operator-function-id:
     operator operator  

   Returns an IDENTIFIER_NODE for the operator which is a
   human-readable spelling of the identifier, e.g., `operator +'.  */

static tree 
cp_parser_operator_function_id (cp_parser* parser)
{
  /* Look for the `operator' keyword.  */
  if (!cp_parser_require_keyword (parser, RID_OPERATOR, "`operator'"))
    return error_mark_node;
  /* And then the name of the operator itself.  */
  return cp_parser_operator (parser);
}

/* Parse an operator.

   operator:
     new delete new[] delete[] + - * / % ^ & | ~ ! = < >
     += -= *= /= %= ^= &= |= << >> >>= <<= == != <= >= &&
     || ++ -- , ->* -> () []

   GNU Extensions:
   
   operator:
     <? >? <?= >?=

   Returns an IDENTIFIER_NODE for the operator which is a
   human-readable spelling of the identifier, e.g., `operator +'.  */
   
static tree
cp_parser_operator (cp_parser* parser)
{
  tree id = NULL_TREE;
  cp_token *token;

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* Figure out which operator we have.  */
  switch (token->type)
    {
    case CPP_KEYWORD:
      {
	enum tree_code op;

	/* The keyword should be either `new' or `delete'.  */
	if (token->keyword == RID_NEW)
	  op = NEW_EXPR;
	else if (token->keyword == RID_DELETE)
	  op = DELETE_EXPR;
	else
	  break;

	/* Consume the `new' or `delete' token.  */
	cp_lexer_consume_token (parser->lexer);

	/* Peek at the next token.  */
	token = cp_lexer_peek_token (parser->lexer);
	/* If it's a `[' token then this is the array variant of the
	   operator.  */
	if (token->type == CPP_OPEN_SQUARE)
	  {
	    /* Consume the `[' token.  */
	    cp_lexer_consume_token (parser->lexer);
	    /* Look for the `]' token.  */
	    cp_parser_require (parser, CPP_CLOSE_SQUARE, "`]'");
	    id = ansi_opname (op == NEW_EXPR 
			      ? VEC_NEW_EXPR : VEC_DELETE_EXPR);
	  }
	/* Otherwise, we have the non-array variant.  */
	else
	  id = ansi_opname (op);

	return id;
      }

    case CPP_PLUS:
      id = ansi_opname (PLUS_EXPR);
      break;

    case CPP_MINUS:
      id = ansi_opname (MINUS_EXPR);
      break;

    case CPP_MULT:
      id = ansi_opname (MULT_EXPR);
      break;

    case CPP_DIV:
      id = ansi_opname (TRUNC_DIV_EXPR);
      break;

    case CPP_MOD:
      id = ansi_opname (TRUNC_MOD_EXPR);
      break;

    case CPP_XOR:
      id = ansi_opname (BIT_XOR_EXPR);
      break;

    case CPP_AND:
      id = ansi_opname (BIT_AND_EXPR);
      break;

    case CPP_OR:
      id = ansi_opname (BIT_IOR_EXPR);
      break;

    case CPP_COMPL:
      id = ansi_opname (BIT_NOT_EXPR);
      break;
      
    case CPP_NOT:
      id = ansi_opname (TRUTH_NOT_EXPR);
      break;

    case CPP_EQ:
      id = ansi_assopname (NOP_EXPR);
      break;

    case CPP_LESS:
      id = ansi_opname (LT_EXPR);
      break;

    case CPP_GREATER:
      id = ansi_opname (GT_EXPR);
      break;

    case CPP_PLUS_EQ:
      id = ansi_assopname (PLUS_EXPR);
      break;

    case CPP_MINUS_EQ:
      id = ansi_assopname (MINUS_EXPR);
      break;

    case CPP_MULT_EQ:
      id = ansi_assopname (MULT_EXPR);
      break;

    case CPP_DIV_EQ:
      id = ansi_assopname (TRUNC_DIV_EXPR);
      break;

    case CPP_MOD_EQ:
      id = ansi_assopname (TRUNC_MOD_EXPR);
      break;

    case CPP_XOR_EQ:
      id = ansi_assopname (BIT_XOR_EXPR);
      break;

    case CPP_AND_EQ:
      id = ansi_assopname (BIT_AND_EXPR);
      break;

    case CPP_OR_EQ:
      id = ansi_assopname (BIT_IOR_EXPR);
      break;

    case CPP_LSHIFT:
      id = ansi_opname (LSHIFT_EXPR);
      break;

    case CPP_RSHIFT:
      id = ansi_opname (RSHIFT_EXPR);
      break;

    case CPP_LSHIFT_EQ:
      id = ansi_assopname (LSHIFT_EXPR);
      break;

    case CPP_RSHIFT_EQ:
      id = ansi_assopname (RSHIFT_EXPR);
      break;

    case CPP_EQ_EQ:
      id = ansi_opname (EQ_EXPR);
      break;

    case CPP_NOT_EQ:
      id = ansi_opname (NE_EXPR);
      break;

    case CPP_LESS_EQ:
      id = ansi_opname (LE_EXPR);
      break;

    case CPP_GREATER_EQ:
      id = ansi_opname (GE_EXPR);
      break;

    case CPP_AND_AND:
      id = ansi_opname (TRUTH_ANDIF_EXPR);
      break;

    case CPP_OR_OR:
      id = ansi_opname (TRUTH_ORIF_EXPR);
      break;
      
    case CPP_PLUS_PLUS:
      id = ansi_opname (POSTINCREMENT_EXPR);
      break;

    case CPP_MINUS_MINUS:
      id = ansi_opname (PREDECREMENT_EXPR);
      break;

    case CPP_COMMA:
      id = ansi_opname (COMPOUND_EXPR);
      break;

    case CPP_DEREF_STAR:
      id = ansi_opname (MEMBER_REF);
      break;

    case CPP_DEREF:
      id = ansi_opname (COMPONENT_REF);
      break;

    case CPP_OPEN_PAREN:
      /* Consume the `('.  */
      cp_lexer_consume_token (parser->lexer);
      /* Look for the matching `)'.  */
      cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");
      return ansi_opname (CALL_EXPR);

    case CPP_OPEN_SQUARE:
      /* Consume the `['.  */
      cp_lexer_consume_token (parser->lexer);
      /* Look for the matching `]'.  */
      cp_parser_require (parser, CPP_CLOSE_SQUARE, "`]'");
      return ansi_opname (ARRAY_REF);

      /* Extensions.  */
    case CPP_MIN:
      id = ansi_opname (MIN_EXPR);
      break;

    case CPP_MAX:
      id = ansi_opname (MAX_EXPR);
      break;

    case CPP_MIN_EQ:
      id = ansi_assopname (MIN_EXPR);
      break;

    case CPP_MAX_EQ:
      id = ansi_assopname (MAX_EXPR);
      break;

    default:
      /* Anything else is an error.  */
      break;
    }

  /* If we have selected an identifier, we need to consume the
     operator token.  */
  if (id)
    cp_lexer_consume_token (parser->lexer);
  /* Otherwise, no valid operator name was present.  */
  else
    {
      cp_parser_error (parser, "expected operator");
      id = error_mark_node;
    }

  return id;
}

/* Parse a template-declaration.

   template-declaration:
     export [opt] template < template-parameter-list > declaration  

   If MEMBER_P is TRUE, this template-declaration occurs within a
   class-specifier.  

   The grammar rule given by the standard isn't correct.  What
   is really meant is:

   template-declaration:
     export [opt] template-parameter-list-seq 
       decl-specifier-seq [opt] init-declarator [opt] ;
     export [opt] template-parameter-list-seq 
       function-definition

   template-parameter-list-seq:
     template-parameter-list-seq [opt]
     template < template-parameter-list >  */

static void
cp_parser_template_declaration (cp_parser* parser, bool member_p)
{
  /* Check for `export'.  */
  if (cp_lexer_next_token_is_keyword (parser->lexer, RID_EXPORT))
    {
      /* Consume the `export' token.  */
      cp_lexer_consume_token (parser->lexer);
      /* Warn that we do not support `export'.  */
      warning ("keyword `export' not implemented, and will be ignored");
    }

  cp_parser_template_declaration_after_export (parser, member_p);
}

/* Parse a template-parameter-list.

   template-parameter-list:
     template-parameter
     template-parameter-list , template-parameter

   Returns a TREE_LIST.  Each node represents a template parameter.
   The nodes are connected via their TREE_CHAINs.  */

static tree
cp_parser_template_parameter_list (cp_parser* parser)
{
  tree parameter_list = NULL_TREE;

  while (true)
    {
      tree parameter;
      cp_token *token;

      /* Parse the template-parameter.  */
      parameter = cp_parser_template_parameter (parser);
      /* Add it to the list.  */
      parameter_list = process_template_parm (parameter_list,
					      parameter);

      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);
      /* If it's not a `,', we're done.  */
      if (token->type != CPP_COMMA)
	break;
      /* Otherwise, consume the `,' token.  */
      cp_lexer_consume_token (parser->lexer);
    }

  return parameter_list;
}

/* Parse a template-parameter.

   template-parameter:
     type-parameter
     parameter-declaration

   Returns a TREE_LIST.  The TREE_VALUE represents the parameter.  The
   TREE_PURPOSE is the default value, if any.  */

static tree
cp_parser_template_parameter (cp_parser* parser)
{
  cp_token *token;

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* If it is `class' or `template', we have a type-parameter.  */
  if (token->keyword == RID_TEMPLATE)
    return cp_parser_type_parameter (parser);
  /* If it is `class' or `typename' we do not know yet whether it is a
     type parameter or a non-type parameter.  Consider:

       template <typename T, typename T::X X> ...

     or:
     
       template <class C, class D*> ...

     Here, the first parameter is a type parameter, and the second is
     a non-type parameter.  We can tell by looking at the token after
     the identifier -- if it is a `,', `=', or `>' then we have a type
     parameter.  */
  if (token->keyword == RID_TYPENAME || token->keyword == RID_CLASS)
    {
      /* Peek at the token after `class' or `typename'.  */
      token = cp_lexer_peek_nth_token (parser->lexer, 2);
      /* If it's an identifier, skip it.  */
      if (token->type == CPP_NAME)
	token = cp_lexer_peek_nth_token (parser->lexer, 3);
      /* Now, see if the token looks like the end of a template
	 parameter.  */
      if (token->type == CPP_COMMA 
	  || token->type == CPP_EQ
	  || token->type == CPP_GREATER)
	return cp_parser_type_parameter (parser);
    }

  /* Otherwise, it is a non-type parameter.  

     [temp.param]

     When parsing a default template-argument for a non-type
     template-parameter, the first non-nested `>' is taken as the end
     of the template parameter-list rather than a greater-than
     operator.  */
  return 
    cp_parser_parameter_declaration (parser, /*template_parm_p=*/true,
				     /*parenthesized_p=*/NULL);
}

/* Parse a type-parameter.

   type-parameter:
     class identifier [opt]
     class identifier [opt] = type-id
     typename identifier [opt]
     typename identifier [opt] = type-id
     template < template-parameter-list > class identifier [opt]
     template < template-parameter-list > class identifier [opt] 
       = id-expression  

   Returns a TREE_LIST.  The TREE_VALUE is itself a TREE_LIST.  The
   TREE_PURPOSE is the default-argument, if any.  The TREE_VALUE is
   the declaration of the parameter.  */

static tree
cp_parser_type_parameter (cp_parser* parser)
{
  cp_token *token;
  tree parameter;

  /* Look for a keyword to tell us what kind of parameter this is.  */
  token = cp_parser_require (parser, CPP_KEYWORD, 
			     "`class', `typename', or `template'");
  if (!token)
    return error_mark_node;

  switch (token->keyword)
    {
    case RID_CLASS:
    case RID_TYPENAME:
      {
	tree identifier;
	tree default_argument;

	/* If the next token is an identifier, then it names the
           parameter.  */
	if (cp_lexer_next_token_is (parser->lexer, CPP_NAME))
	  identifier = cp_parser_identifier (parser);
	else
	  identifier = NULL_TREE;

	/* Create the parameter.  */
	parameter = finish_template_type_parm (class_type_node, identifier);

	/* If the next token is an `=', we have a default argument.  */
	if (cp_lexer_next_token_is (parser->lexer, CPP_EQ))
	  {
	    /* Consume the `=' token.  */
	    cp_lexer_consume_token (parser->lexer);
	    /* Parse the default-argument.  */
	    default_argument = cp_parser_type_id (parser);
	  }
	else
	  default_argument = NULL_TREE;

	/* Create the combined representation of the parameter and the
	   default argument.  */
	parameter = build_tree_list (default_argument, parameter);
      }
      break;

    case RID_TEMPLATE:
      {
	tree parameter_list;
	tree identifier;
	tree default_argument;

	/* Look for the `<'.  */
	cp_parser_require (parser, CPP_LESS, "`<'");
	/* Parse the template-parameter-list.  */
	begin_template_parm_list ();
	parameter_list 
	  = cp_parser_template_parameter_list (parser);
	parameter_list = end_template_parm_list (parameter_list);
	/* Look for the `>'.  */
	cp_parser_require (parser, CPP_GREATER, "`>'");
	/* Look for the `class' keyword.  */
	cp_parser_require_keyword (parser, RID_CLASS, "`class'");
	/* If the next token is an `=', then there is a
	   default-argument.  If the next token is a `>', we are at
	   the end of the parameter-list.  If the next token is a `,',
	   then we are at the end of this parameter.  */
	if (cp_lexer_next_token_is_not (parser->lexer, CPP_EQ)
	    && cp_lexer_next_token_is_not (parser->lexer, CPP_GREATER)
	    && cp_lexer_next_token_is_not (parser->lexer, CPP_COMMA))
	  {
	    identifier = cp_parser_identifier (parser);
	    /* Treat invalid names as if the parameter were nameless. */
	    if (identifier == error_mark_node)
	      identifier = NULL_TREE;
	  }
	else
	  identifier = NULL_TREE;

	/* Create the template parameter.  */
	parameter = finish_template_template_parm (class_type_node,
						   identifier);
						   
	/* If the next token is an `=', then there is a
	   default-argument.  */
	if (cp_lexer_next_token_is (parser->lexer, CPP_EQ))
	  {
	    bool is_template;

	    /* Consume the `='.  */
	    cp_lexer_consume_token (parser->lexer);
	    /* Parse the id-expression.  */
	    default_argument 
	      = cp_parser_id_expression (parser,
					 /*template_keyword_p=*/false,
					 /*check_dependency_p=*/true,
					 /*template_p=*/&is_template,
					 /*declarator_p=*/false);
	    if (TREE_CODE (default_argument) == TYPE_DECL)
	      /* If the id-expression was a template-id that refers to
		 a template-class, we already have the declaration here,
		 so no further lookup is needed.  */
		 ;
	    else
	      /* Look up the name.  */
	      default_argument 
		= cp_parser_lookup_name (parser, default_argument,
					/*is_type=*/false,
					/*is_template=*/is_template,
					/*is_namespace=*/false,
					/*check_dependency=*/true);
	    /* See if the default argument is valid.  */
	    default_argument
	      = check_template_template_default_arg (default_argument);
	  }
	else
	  default_argument = NULL_TREE;

	/* Create the combined representation of the parameter and the
	   default argument.  */
	parameter = build_tree_list (default_argument, parameter);
      }
      break;

    default:
      abort ();
      break;
    }
  
  return parameter;
}

/* Parse a template-id.

   template-id:
     template-name < template-argument-list [opt] >

   If TEMPLATE_KEYWORD_P is TRUE, then we have just seen the
   `template' keyword.  In this case, a TEMPLATE_ID_EXPR will be
   returned.  Otherwise, if the template-name names a function, or set
   of functions, returns a TEMPLATE_ID_EXPR.  If the template-name
   names a class, returns a TYPE_DECL for the specialization.  

   If CHECK_DEPENDENCY_P is FALSE, names are looked up in
   uninstantiated templates.  */

static tree
cp_parser_template_id (cp_parser *parser, 
		       bool template_keyword_p, 
		       bool check_dependency_p,
		       bool is_declaration)
{
  tree template;
  tree arguments;
  tree template_id;
  ptrdiff_t start_of_id;
  tree access_check = NULL_TREE;
  cp_token *next_token, *next_token_2;
  bool is_identifier;

  /* If the next token corresponds to a template-id, there is no need
     to reparse it.  */
  next_token = cp_lexer_peek_token (parser->lexer);
  if (next_token->type == CPP_TEMPLATE_ID)
    {
      tree value;
      tree check;

      /* Get the stored value.  */
      value = cp_lexer_consume_token (parser->lexer)->value;
      /* Perform any access checks that were deferred.  */
      for (check = TREE_PURPOSE (value); check; check = TREE_CHAIN (check))
	perform_or_defer_access_check (TREE_PURPOSE (check),
				       TREE_VALUE (check));
      /* Return the stored value.  */
      return TREE_VALUE (value);
    }

  /* Avoid performing name lookup if there is no possibility of
     finding a template-id.  */
  if ((next_token->type != CPP_NAME && next_token->keyword != RID_OPERATOR)
      || (next_token->type == CPP_NAME
	  && !cp_parser_nth_token_starts_template_argument_list_p 
	       (parser, 2)))
    {
      cp_parser_error (parser, "expected template-id");
      return error_mark_node;
    }

  /* Remember where the template-id starts.  */
  if (cp_parser_parsing_tentatively (parser)
      && !cp_parser_committed_to_tentative_parse (parser))
    {
      next_token = cp_lexer_peek_token (parser->lexer);
      start_of_id = cp_lexer_token_difference (parser->lexer,
					       parser->lexer->first_token,
					       next_token);
    }
  else
    start_of_id = -1;

  push_deferring_access_checks (dk_deferred);

  /* Parse the template-name.  */
  is_identifier = false;
  template = cp_parser_template_name (parser, template_keyword_p,
				      check_dependency_p,
				      is_declaration,
				      &is_identifier);
  if (template == error_mark_node || is_identifier)
    {
      pop_deferring_access_checks ();
      return template;
    }

  /* If we find the sequence `[:' after a template-name, it's probably 
     a digraph-typo for `< ::'. Substitute the tokens and check if we can
     parse correctly the argument list.  */
  next_token = cp_lexer_peek_nth_token (parser->lexer, 1);
  next_token_2 = cp_lexer_peek_nth_token (parser->lexer, 2);
  if (next_token->type == CPP_OPEN_SQUARE 
      && next_token->flags & DIGRAPH
      && next_token_2->type == CPP_COLON 
      && !(next_token_2->flags & PREV_WHITE))
    {
      cp_parser_parse_tentatively (parser);
      /* Change `:' into `::'.  */
      next_token_2->type = CPP_SCOPE;
      /* Consume the first token (CPP_OPEN_SQUARE - which we pretend it is
         CPP_LESS.  */
      cp_lexer_consume_token (parser->lexer);
      /* Parse the arguments.  */
      arguments = cp_parser_enclosed_template_argument_list (parser);
      if (!cp_parser_parse_definitely (parser))
	{
	  /* If we couldn't parse an argument list, then we revert our changes
	     and return simply an error. Maybe this is not a template-id
	     after all.  */
	  next_token_2->type = CPP_COLON;
	  cp_parser_error (parser, "expected `<'");
	  pop_deferring_access_checks ();
	  return error_mark_node;
	}
      /* Otherwise, emit an error about the invalid digraph, but continue
         parsing because we got our argument list.  */
      pedwarn ("`<::' cannot begin a template-argument list");
      inform ("`<:' is an alternate spelling for `['. Insert whitespace "
	      "between `<' and `::'");
      if (!flag_permissive)
	{
	  static bool hint;
	  if (!hint)
	    {
	      inform ("(if you use `-fpermissive' G++ will accept your code)");
	      hint = true;
	    }
	}
    }
  else
    {
      /* Look for the `<' that starts the template-argument-list.  */
      if (!cp_parser_require (parser, CPP_LESS, "`<'"))
	{
	  pop_deferring_access_checks ();
	  return error_mark_node;
	}
      /* Parse the arguments.  */
      arguments = cp_parser_enclosed_template_argument_list (parser);
    }

  /* Build a representation of the specialization.  */
  if (TREE_CODE (template) == IDENTIFIER_NODE)
    template_id = build_min_nt (TEMPLATE_ID_EXPR, template, arguments);
  else if (DECL_CLASS_TEMPLATE_P (template)
	   || DECL_TEMPLATE_TEMPLATE_PARM_P (template))
    template_id 
      = finish_template_type (template, arguments, 
			      cp_lexer_next_token_is (parser->lexer, 
						      CPP_SCOPE));
  else
    {
      /* If it's not a class-template or a template-template, it should be
	 a function-template.  */
      my_friendly_assert ((DECL_FUNCTION_TEMPLATE_P (template)
			   || TREE_CODE (template) == OVERLOAD
			   || BASELINK_P (template)),
			  20010716);
      
      template_id = lookup_template_function (template, arguments);
    }
  
  /* Retrieve any deferred checks.  Do not pop this access checks yet
     so the memory will not be reclaimed during token replacing below.  */
  access_check = get_deferred_access_checks ();

  /* If parsing tentatively, replace the sequence of tokens that makes
     up the template-id with a CPP_TEMPLATE_ID token.  That way,
     should we re-parse the token stream, we will not have to repeat
     the effort required to do the parse, nor will we issue duplicate
     error messages about problems during instantiation of the
     template.  */
  if (start_of_id >= 0)
    {
      cp_token *token;

      /* Find the token that corresponds to the start of the
	 template-id.  */
      token = cp_lexer_advance_token (parser->lexer, 
				      parser->lexer->first_token,
				      start_of_id);

      /* Reset the contents of the START_OF_ID token.  */
      token->type = CPP_TEMPLATE_ID;
      token->value = build_tree_list (access_check, template_id);
      token->keyword = RID_MAX;
      /* Purge all subsequent tokens.  */
      cp_lexer_purge_tokens_after (parser->lexer, token);

      /* ??? Can we actually assume that, if template_id ==
	 error_mark_node, we will have issued a diagnostic to the
	 user, as opposed to simply marking the tentative parse as
	 failed?  */
      if (cp_parser_error_occurred (parser) && template_id != error_mark_node)
	error ("parse error in template argument list");
    }

  pop_deferring_access_checks ();
  return template_id;
}

/* Parse a template-name.

   template-name:
     identifier
 
   The standard should actually say:

   template-name:
     identifier
     operator-function-id

   A defect report has been filed about this issue.

   A conversion-function-id cannot be a template name because they cannot
   be part of a template-id. In fact, looking at this code:

   a.operator K<int>()

   the conversion-function-id is "operator K<int>", and K<int> is a type-id.
   It is impossible to call a templated conversion-function-id with an 
   explicit argument list, since the only allowed template parameter is
   the type to which it is converting.

   If TEMPLATE_KEYWORD_P is true, then we have just seen the
   `template' keyword, in a construction like:

     T::template f<3>()

   In that case `f' is taken to be a template-name, even though there
   is no way of knowing for sure.

   Returns the TEMPLATE_DECL for the template, or an OVERLOAD if the
   name refers to a set of overloaded functions, at least one of which
   is a template, or an IDENTIFIER_NODE with the name of the template,
   if TEMPLATE_KEYWORD_P is true.  If CHECK_DEPENDENCY_P is FALSE,
   names are looked up inside uninstantiated templates.  */

static tree
cp_parser_template_name (cp_parser* parser, 
                         bool template_keyword_p, 
                         bool check_dependency_p,
			 bool is_declaration,
			 bool *is_identifier)
{
  tree identifier;
  tree decl;
  tree fns;

  /* If the next token is `operator', then we have either an
     operator-function-id or a conversion-function-id.  */
  if (cp_lexer_next_token_is_keyword (parser->lexer, RID_OPERATOR))
    {
      /* We don't know whether we're looking at an
	 operator-function-id or a conversion-function-id.  */
      cp_parser_parse_tentatively (parser);
      /* Try an operator-function-id.  */
      identifier = cp_parser_operator_function_id (parser);
      /* If that didn't work, try a conversion-function-id.  */
      if (!cp_parser_parse_definitely (parser))
        {
	  cp_parser_error (parser, "expected template-name");
	  return error_mark_node;
        }
    }
  /* Look for the identifier.  */
  else
    identifier = cp_parser_identifier (parser);
  
  /* If we didn't find an identifier, we don't have a template-id.  */
  if (identifier == error_mark_node)
    return error_mark_node;

  /* If the name immediately followed the `template' keyword, then it
     is a template-name.  However, if the next token is not `<', then
     we do not treat it as a template-name, since it is not being used
     as part of a template-id.  This enables us to handle constructs
     like:

       template <typename T> struct S { S(); };
       template <typename T> S<T>::S();

     correctly.  We would treat `S' as a template -- if it were `S<T>'
     -- but we do not if there is no `<'.  */

  if (processing_template_decl
      && cp_parser_nth_token_starts_template_argument_list_p (parser, 1))
    {
      /* In a declaration, in a dependent context, we pretend that the
	 "template" keyword was present in order to improve error
	 recovery.  For example, given:
	 
	   template <typename T> void f(T::X<int>);
	 
	 we want to treat "X<int>" as a template-id.  */
      if (is_declaration 
	  && !template_keyword_p 
	  && parser->scope && TYPE_P (parser->scope)
	  && check_dependency_p
	  && dependent_type_p (parser->scope)
	  /* Do not do this for dtors (or ctors), since they never
	     need the template keyword before their name.  */
	  && !constructor_name_p (identifier, parser->scope))
	{
	  ptrdiff_t start;
	  cp_token* token;
	  /* Explain what went wrong.  */
	  error ("non-template `%D' used as template", identifier);
	  inform ("use `%T::template %D' to indicate that it is a template",
		  parser->scope, identifier);
	  /* If parsing tentatively, find the location of the "<"
	     token.  */
	  if (cp_parser_parsing_tentatively (parser)
	      && !cp_parser_committed_to_tentative_parse (parser))
	    {
	      cp_parser_simulate_error (parser);
	      token = cp_lexer_peek_token (parser->lexer);
	      token = cp_lexer_prev_token (parser->lexer, token);
	      start = cp_lexer_token_difference (parser->lexer,
						 parser->lexer->first_token,
						 token);
	    }
	  else
	    start = -1;
	  /* Parse the template arguments so that we can issue error
	     messages about them.  */
	  cp_lexer_consume_token (parser->lexer);
	  cp_parser_enclosed_template_argument_list (parser);
	  /* Skip tokens until we find a good place from which to
	     continue parsing.  */
	  cp_parser_skip_to_closing_parenthesis (parser,
						 /*recovering=*/true,
						 /*or_comma=*/true,
						 /*consume_paren=*/false);
	  /* If parsing tentatively, permanently remove the
	     template argument list.  That will prevent duplicate
	     error messages from being issued about the missing
	     "template" keyword.  */
	  if (start >= 0)
	    {
	      token = cp_lexer_advance_token (parser->lexer,
					      parser->lexer->first_token,
					      start);
	      cp_lexer_purge_tokens_after (parser->lexer, token);
	    }
	  if (is_identifier)
	    *is_identifier = true;
	  return identifier;
	}

      /* If the "template" keyword is present, then there is generally
	 no point in doing name-lookup, so we just return IDENTIFIER.
	 But, if the qualifying scope is non-dependent then we can
	 (and must) do name-lookup normally.  */
      if (template_keyword_p
	  && (!parser->scope
	      || (TYPE_P (parser->scope) 
		  && dependent_type_p (parser->scope))))
	return identifier;
    }

  /* Look up the name.  */
  decl = cp_parser_lookup_name (parser, identifier,
				/*is_type=*/false,
				/*is_template=*/false,
				/*is_namespace=*/false,
				check_dependency_p);
  decl = maybe_get_template_decl_from_type_decl (decl);

  /* If DECL is a template, then the name was a template-name.  */
  if (TREE_CODE (decl) == TEMPLATE_DECL)
    ;
  else 
    {
      tree fn = NULL_TREE;

      /* The standard does not explicitly indicate whether a name that
	 names a set of overloaded declarations, some of which are
	 templates, is a template-name.  However, such a name should
	 be a template-name; otherwise, there is no way to form a
	 template-id for the overloaded templates.  */
      fns = BASELINK_P (decl) ? BASELINK_FUNCTIONS (decl) : decl;
      if (TREE_CODE (fns) == OVERLOAD)
	for (fn = fns; fn; fn = OVL_NEXT (fn))
	  if (TREE_CODE (OVL_CURRENT (fn)) == TEMPLATE_DECL)
	    break;

      if (!fn)
	{
	  /* Otherwise, the name does not name a template.  */
	  cp_parser_error (parser, "expected template-name");
	  return error_mark_node;
	}
    }

  /* If DECL is dependent, and refers to a function, then just return
     its name; we will look it up again during template instantiation.  */
  if (DECL_FUNCTION_TEMPLATE_P (decl) || !DECL_P (decl))
    {
      tree scope = CP_DECL_CONTEXT (get_first_fn (decl));
      if (TYPE_P (scope) && dependent_type_p (scope))
	return identifier;
    }

  return decl;
}

/* Parse a template-argument-list.

   template-argument-list:
     template-argument
     template-argument-list , template-argument

   Returns a TREE_VEC containing the arguments.  */

static tree
cp_parser_template_argument_list (cp_parser* parser)
{
  tree fixed_args[10];
  unsigned n_args = 0;
  unsigned alloced = 10;
  tree *arg_ary = fixed_args;
  tree vec;
  bool saved_in_template_argument_list_p;

  saved_in_template_argument_list_p = parser->in_template_argument_list_p;
  parser->in_template_argument_list_p = true;
  do
    {
      tree argument;

      if (n_args)
	/* Consume the comma.  */
	cp_lexer_consume_token (parser->lexer);
      
      /* Parse the template-argument.  */
      argument = cp_parser_template_argument (parser);
      if (n_args == alloced)
	{
	  alloced *= 2;
	  
	  if (arg_ary == fixed_args)
	    {
	      arg_ary = xmalloc (sizeof (tree) * alloced);
	      memcpy (arg_ary, fixed_args, sizeof (tree) * n_args);
	    }
	  else
	    arg_ary = xrealloc (arg_ary, sizeof (tree) * alloced);
	}
      arg_ary[n_args++] = argument;
    }
  while (cp_lexer_next_token_is (parser->lexer, CPP_COMMA));

  vec = make_tree_vec (n_args);

  while (n_args--)
    TREE_VEC_ELT (vec, n_args) = arg_ary[n_args];
  
  if (arg_ary != fixed_args)
    free (arg_ary);
  parser->in_template_argument_list_p = saved_in_template_argument_list_p;
  return vec;
}

/* Parse a template-argument.

   template-argument:
     assignment-expression
     type-id
     id-expression

   The representation is that of an assignment-expression, type-id, or
   id-expression -- except that the qualified id-expression is
   evaluated, so that the value returned is either a DECL or an
   OVERLOAD.  

   Although the standard says "assignment-expression", it forbids
   throw-expressions or assignments in the template argument.
   Therefore, we use "conditional-expression" instead.  */

static tree
cp_parser_template_argument (cp_parser* parser)
{
  tree argument;
  bool template_p;
  bool address_p;
  bool maybe_type_id = false;
  cp_token *token;
  cp_id_kind idk;
  tree qualifying_class;

  /* There's really no way to know what we're looking at, so we just
     try each alternative in order.  

       [temp.arg]

       In a template-argument, an ambiguity between a type-id and an
       expression is resolved to a type-id, regardless of the form of
       the corresponding template-parameter.  

     Therefore, we try a type-id first.  */
  cp_parser_parse_tentatively (parser);
  argument = cp_parser_type_id (parser);
  /* If there was no error parsing the type-id but the next token is a '>>',
     we probably found a typo for '> >'. But there are type-id which are 
     also valid expressions. For instance:

     struct X { int operator >> (int); };
     template <int V> struct Foo {};
     Foo<X () >> 5> r;

     Here 'X()' is a valid type-id of a function type, but the user just
     wanted to write the expression "X() >> 5". Thus, we remember that we
     found a valid type-id, but we still try to parse the argument as an
     expression to see what happens.  */
  if (!cp_parser_error_occurred (parser)
      && cp_lexer_next_token_is (parser->lexer, CPP_RSHIFT))
    {
      maybe_type_id = true;
      cp_parser_abort_tentative_parse (parser);
    }
  else
    {
      /* If the next token isn't a `,' or a `>', then this argument wasn't
      really finished. This means that the argument is not a valid
      type-id.  */
      if (!cp_parser_next_token_ends_template_argument_p (parser))
	cp_parser_error (parser, "expected template-argument");
      /* If that worked, we're done.  */
      if (cp_parser_parse_definitely (parser))
	return argument;
    }
  /* We're still not sure what the argument will be.  */
  cp_parser_parse_tentatively (parser);
  /* Try a template.  */
  argument = cp_parser_id_expression (parser, 
				      /*template_keyword_p=*/false,
				      /*check_dependency_p=*/true,
				      &template_p,
				      /*declarator_p=*/false);
  /* If the next token isn't a `,' or a `>', then this argument wasn't
     really finished.  */
  if (!cp_parser_next_token_ends_template_argument_p (parser))
    cp_parser_error (parser, "expected template-argument");
  if (!cp_parser_error_occurred (parser))
    {
      /* Figure out what is being referred to.  If the id-expression
	 was for a class template specialization, then we will have a
	 TYPE_DECL at this point.  There is no need to do name lookup
	 at this point in that case.  */
      if (TREE_CODE (argument) != TYPE_DECL)
	argument = cp_parser_lookup_name (parser, argument,
					  /*is_type=*/false,
					  /*is_template=*/template_p,
					  /*is_namespace=*/false,
					  /*check_dependency=*/true);
      if (TREE_CODE (argument) != TEMPLATE_DECL
	  && TREE_CODE (argument) != UNBOUND_CLASS_TEMPLATE)
	cp_parser_error (parser, "expected template-name");
    }
  if (cp_parser_parse_definitely (parser))
    return argument;
  /* It must be a non-type argument.  There permitted cases are given
     in [temp.arg.nontype]:

     -- an integral constant-expression of integral or enumeration
        type; or

     -- the name of a non-type template-parameter; or

     -- the name of an object or function with external linkage...

     -- the address of an object or function with external linkage...

     -- a pointer to member...  */
  /* Look for a non-type template parameter.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_NAME))
    {
      cp_parser_parse_tentatively (parser);
      argument = cp_parser_primary_expression (parser,
					       &idk,
					       &qualifying_class);
      if (TREE_CODE (argument) != TEMPLATE_PARM_INDEX
	  || !cp_parser_next_token_ends_template_argument_p (parser))
	cp_parser_simulate_error (parser);
      if (cp_parser_parse_definitely (parser))
	return argument;
    }
  /* If the next token is "&", the argument must be the address of an
     object or function with external linkage.  */
  address_p = cp_lexer_next_token_is (parser->lexer, CPP_AND);
  if (address_p)
    cp_lexer_consume_token (parser->lexer);
  /* See if we might have an id-expression.  */
  token = cp_lexer_peek_token (parser->lexer);
  if (token->type == CPP_NAME
      || token->keyword == RID_OPERATOR
      || token->type == CPP_SCOPE
      || token->type == CPP_TEMPLATE_ID
      || token->type == CPP_NESTED_NAME_SPECIFIER)
    {
      cp_parser_parse_tentatively (parser);
      argument = cp_parser_primary_expression (parser,
					       &idk,
					       &qualifying_class);
      if (cp_parser_error_occurred (parser)
	  || !cp_parser_next_token_ends_template_argument_p (parser))
	cp_parser_abort_tentative_parse (parser);
      else
	{
	  if (qualifying_class)
	    argument = finish_qualified_id_expr (qualifying_class,
						 argument,
						 /*done=*/true,
						 address_p);
	  if (TREE_CODE (argument) == VAR_DECL)
	    {
	      /* A variable without external linkage might still be a
		 valid constant-expression, so no error is issued here
		 if the external-linkage check fails.  */
	      if (!DECL_EXTERNAL_LINKAGE_P (argument))
		cp_parser_simulate_error (parser);
	    }
	  else if (is_overloaded_fn (argument))
	    /* All overloaded functions are allowed; if the external
	       linkage test does not pass, an error will be issued
	       later.  */
	    ;
	  else if (address_p
		   && (TREE_CODE (argument) == OFFSET_REF 
		       || TREE_CODE (argument) == SCOPE_REF))
	    /* A pointer-to-member.  */
	    ;
	  else
	    cp_parser_simulate_error (parser);

	  if (cp_parser_parse_definitely (parser))
	    {
	      if (address_p)
		argument = build_x_unary_op (ADDR_EXPR, argument);
	      return argument;
	    }
	}
    }
  /* If the argument started with "&", there are no other valid
     alternatives at this point.  */
  if (address_p)
    {
      cp_parser_error (parser, "invalid non-type template argument");
      return error_mark_node;
    }
  /* If the argument wasn't successfully parsed as a type-id followed
     by '>>', the argument can only be a constant expression now.  
     Otherwise, we try parsing the constant-expression tentatively,
     because the argument could really be a type-id.  */
  if (maybe_type_id)
    cp_parser_parse_tentatively (parser);
  argument = cp_parser_constant_expression (parser, 
					    /*allow_non_constant_p=*/false,
					    /*non_constant_p=*/NULL);
  argument = fold_non_dependent_expr (argument);
  if (!maybe_type_id)
    return argument;
  if (!cp_parser_next_token_ends_template_argument_p (parser))
    cp_parser_error (parser, "expected template-argument");
  if (cp_parser_parse_definitely (parser))
    return argument;
  /* We did our best to parse the argument as a non type-id, but that
     was the only alternative that matched (albeit with a '>' after
     it). We can assume it's just a typo from the user, and a 
     diagnostic will then be issued.  */
  return cp_parser_type_id (parser);
}

/* Parse an explicit-instantiation.

   explicit-instantiation:
     template declaration  

   Although the standard says `declaration', what it really means is:

   explicit-instantiation:
     template decl-specifier-seq [opt] declarator [opt] ; 

   Things like `template int S<int>::i = 5, int S<double>::j;' are not
   supposed to be allowed.  A defect report has been filed about this
   issue.  

   GNU Extension:
  
   explicit-instantiation:
     storage-class-specifier template 
       decl-specifier-seq [opt] declarator [opt] ;
     function-specifier template 
       decl-specifier-seq [opt] declarator [opt] ;  */

static void
cp_parser_explicit_instantiation (cp_parser* parser)
{
  int declares_class_or_enum;
  tree decl_specifiers;
  tree attributes;
  tree extension_specifier = NULL_TREE;

  /* Look for an (optional) storage-class-specifier or
     function-specifier.  */
  if (cp_parser_allow_gnu_extensions_p (parser))
    {
      extension_specifier 
	= cp_parser_storage_class_specifier_opt (parser);
      if (!extension_specifier)
	extension_specifier = cp_parser_function_specifier_opt (parser);
    }

  /* Look for the `template' keyword.  */
  cp_parser_require_keyword (parser, RID_TEMPLATE, "`template'");
  /* Let the front end know that we are processing an explicit
     instantiation.  */
  begin_explicit_instantiation ();
  /* [temp.explicit] says that we are supposed to ignore access
     control while processing explicit instantiation directives.  */
  push_deferring_access_checks (dk_no_check);
  /* Parse a decl-specifier-seq.  */
  decl_specifiers 
    = cp_parser_decl_specifier_seq (parser,
				    CP_PARSER_FLAGS_OPTIONAL,
				    &attributes,
				    &declares_class_or_enum);
  /* If there was exactly one decl-specifier, and it declared a class,
     and there's no declarator, then we have an explicit type
     instantiation.  */
  if (declares_class_or_enum && cp_parser_declares_only_class_p (parser))
    {
      tree type;

      type = check_tag_decl (decl_specifiers);
      /* Turn access control back on for names used during
	 template instantiation.  */
      pop_deferring_access_checks ();
      if (type)
	do_type_instantiation (type, extension_specifier, /*complain=*/1);
    }
  else
    {
      tree declarator;
      tree decl;

      /* Parse the declarator.  */
      declarator 
	= cp_parser_declarator (parser, CP_PARSER_DECLARATOR_NAMED,
				/*ctor_dtor_or_conv_p=*/NULL,
				/*parenthesized_p=*/NULL,
				/*member_p=*/false);
      if (declares_class_or_enum & 2)
	cp_parser_check_for_definition_in_return_type
	  (declarator, TREE_VALUE (decl_specifiers));
      if (declarator != error_mark_node)
	{
	  decl = grokdeclarator (declarator, decl_specifiers, 
				 NORMAL, 0, NULL);
	  /* Turn access control back on for names used during
	     template instantiation.  */
	  pop_deferring_access_checks ();
	  /* Do the explicit instantiation.  */
	  do_decl_instantiation (decl, extension_specifier);
	}
      else
	{
	  pop_deferring_access_checks ();
	  /* Skip the body of the explicit instantiation.  */
	  cp_parser_skip_to_end_of_statement (parser);
	}
    }
  /* We're done with the instantiation.  */
  end_explicit_instantiation ();

  cp_parser_consume_semicolon_at_end_of_statement (parser);
}

/* Parse an explicit-specialization.

   explicit-specialization:
     template < > declaration  

   Although the standard says `declaration', what it really means is:

   explicit-specialization:
     template <> decl-specifier [opt] init-declarator [opt] ;
     template <> function-definition 
     template <> explicit-specialization
     template <> template-declaration  */

static void
cp_parser_explicit_specialization (cp_parser* parser)
{
  /* Look for the `template' keyword.  */
  cp_parser_require_keyword (parser, RID_TEMPLATE, "`template'");
  /* Look for the `<'.  */
  cp_parser_require (parser, CPP_LESS, "`<'");
  /* Look for the `>'.  */
  cp_parser_require (parser, CPP_GREATER, "`>'");
  /* We have processed another parameter list.  */
  ++parser->num_template_parameter_lists;
  /* Let the front end know that we are beginning a specialization.  */
  begin_specialization ();

  /* If the next keyword is `template', we need to figure out whether
     or not we're looking a template-declaration.  */
  if (cp_lexer_next_token_is_keyword (parser->lexer, RID_TEMPLATE))
    {
      if (cp_lexer_peek_nth_token (parser->lexer, 2)->type == CPP_LESS
	  && cp_lexer_peek_nth_token (parser->lexer, 3)->type != CPP_GREATER)
	cp_parser_template_declaration_after_export (parser,
						     /*member_p=*/false);
      else
	cp_parser_explicit_specialization (parser);
    }
  else
    /* Parse the dependent declaration.  */
    cp_parser_single_declaration (parser, 
				  /*member_p=*/false,
				  /*friend_p=*/NULL);

  /* We're done with the specialization.  */
  end_specialization ();
  /* We're done with this parameter list.  */
  --parser->num_template_parameter_lists;
}

/* Parse a type-specifier.

   type-specifier:
     simple-type-specifier
     class-specifier
     enum-specifier
     elaborated-type-specifier
     cv-qualifier

   GNU Extension:

   type-specifier:
     __complex__

   Returns a representation of the type-specifier.  If the
   type-specifier is a keyword (like `int' or `const', or
   `__complex__') then the corresponding IDENTIFIER_NODE is returned.
   For a class-specifier, enum-specifier, or elaborated-type-specifier
   a TREE_TYPE is returned; otherwise, a TYPE_DECL is returned.

   If IS_FRIEND is TRUE then this type-specifier is being declared a
   `friend'.  If IS_DECLARATION is TRUE, then this type-specifier is
   appearing in a decl-specifier-seq.

   If DECLARES_CLASS_OR_ENUM is non-NULL, and the type-specifier is a
   class-specifier, enum-specifier, or elaborated-type-specifier, then
   *DECLARES_CLASS_OR_ENUM is set to a nonzero value.  The value is 1
   if a type is declared; 2 if it is defined.  Otherwise, it is set to
   zero.

   If IS_CV_QUALIFIER is non-NULL, and the type-specifier is a
   cv-qualifier, then IS_CV_QUALIFIER is set to TRUE.  Otherwise, it
   is set to FALSE.  */

static tree
cp_parser_type_specifier (cp_parser* parser, 
			  cp_parser_flags flags, 
			  bool is_friend,
			  bool is_declaration,
			  int* declares_class_or_enum,
			  bool* is_cv_qualifier)
{
  tree type_spec = NULL_TREE;
  cp_token *token;
  enum rid keyword;

  /* Assume this type-specifier does not declare a new type.  */
  if (declares_class_or_enum)
    *declares_class_or_enum = 0;
  /* And that it does not specify a cv-qualifier.  */
  if (is_cv_qualifier)
    *is_cv_qualifier = false;
  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);

  /* If we're looking at a keyword, we can use that to guide the
     production we choose.  */
  keyword = token->keyword;
  switch (keyword)
    {
    case RID_ENUM:
      /* 'enum' [identifier] '{' introduces an enum-specifier;
	 'enum' <anything else> introduces an elaborated-type-specifier.  */
      if (cp_lexer_peek_nth_token (parser->lexer, 2)->type == CPP_OPEN_BRACE
	  || (cp_lexer_peek_nth_token (parser->lexer, 2)->type == CPP_NAME
	      && cp_lexer_peek_nth_token (parser->lexer, 3)->type
	         == CPP_OPEN_BRACE))
	{
	  if (parser->num_template_parameter_lists)
	    {
	      error ("template declaration of `enum'");
	      cp_parser_skip_to_end_of_block_or_statement (parser);
	      type_spec = error_mark_node;
	    }
	  else
	    type_spec = cp_parser_enum_specifier (parser);

	  if (declares_class_or_enum)
	    *declares_class_or_enum = 2;
	  return type_spec;
	}
      else
	goto elaborated_type_specifier;

      /* Any of these indicate either a class-specifier, or an
	 elaborated-type-specifier.  */
    case RID_CLASS:
    case RID_STRUCT:
    case RID_UNION:
      /* Parse tentatively so that we can back up if we don't find a
	 class-specifier or enum-specifier.  */
      cp_parser_parse_tentatively (parser);
      /* Look for the class-specifier.  */
      type_spec = cp_parser_class_specifier (parser);
      /* If that worked, we're done.  */
      if (cp_parser_parse_definitely (parser))
	{
	  if (declares_class_or_enum)
	    *declares_class_or_enum = 2;
	  return type_spec;
	}

      /* Fall through.  */

    case RID_TYPENAME:
    elaborated_type_specifier:
      /* Look for an elaborated-type-specifier.  */
      type_spec = cp_parser_elaborated_type_specifier (parser,
						       is_friend,
						       is_declaration);
      /* We're declaring a class or enum -- unless we're using
	 `typename'.  */
      if (declares_class_or_enum && keyword != RID_TYPENAME)
	*declares_class_or_enum = 1;
      return type_spec;

    case RID_CONST:
    case RID_VOLATILE:
    case RID_RESTRICT:
      type_spec = cp_parser_cv_qualifier_opt (parser);
      /* Even though we call a routine that looks for an optional
	 qualifier, we know that there should be one.  */
      my_friendly_assert (type_spec != NULL, 20000328);
      /* This type-specifier was a cv-qualified.  */
      if (is_cv_qualifier)
	*is_cv_qualifier = true;

      return type_spec;

    case RID_COMPLEX:
      /* The `__complex__' keyword is a GNU extension.  */
      return cp_lexer_consume_token (parser->lexer)->value;

    default:
      break;
    }

  /* If we do not already have a type-specifier, assume we are looking
     at a simple-type-specifier.  */
  type_spec = cp_parser_simple_type_specifier (parser, flags, 
					       /*identifier_p=*/true);

  /* If we didn't find a type-specifier, and a type-specifier was not
     optional in this context, issue an error message.  */
  if (!type_spec && !(flags & CP_PARSER_FLAGS_OPTIONAL))
    {
      cp_parser_error (parser, "expected type specifier");
      return error_mark_node;
    }

  return type_spec;
}

/* Parse a simple-type-specifier.

   simple-type-specifier:
     :: [opt] nested-name-specifier [opt] type-name
     :: [opt] nested-name-specifier template template-id
     char
     wchar_t
     bool
     short
     int
     long
     signed
     unsigned
     float
     double
     void  

   GNU Extension:

   simple-type-specifier:
     __typeof__ unary-expression
     __typeof__ ( type-id )

   For the various keywords, the value returned is simply the
   TREE_IDENTIFIER representing the keyword if IDENTIFIER_P is true.
   For the first two productions, and if IDENTIFIER_P is false, the
   value returned is the indicated TYPE_DECL.  */

static tree
cp_parser_simple_type_specifier (cp_parser* parser, cp_parser_flags flags,
				 bool identifier_p)
{
  tree type = NULL_TREE;
  cp_token *token;

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);

  /* If we're looking at a keyword, things are easy.  */
  switch (token->keyword)
    {
    case RID_CHAR:
      type = char_type_node;
      break;
    case RID_WCHAR:
      type = wchar_type_node;
      break;
    case RID_BOOL:
      type = boolean_type_node;
      break;
    case RID_SHORT:
      type = short_integer_type_node;
      break;
    case RID_INT:
      type = integer_type_node;
      break;
    case RID_LONG:
      type = long_integer_type_node;
      break;
    case RID_SIGNED:
      type = integer_type_node;
      break;
    case RID_UNSIGNED:
      type = unsigned_type_node;
      break;
    case RID_FLOAT:
      type = float_type_node;
      break;
    case RID_DOUBLE:
      type = double_type_node;
      break;
    case RID_VOID:
      type = void_type_node;
      break;

    case RID_TYPEOF:
      {
	tree operand;

	/* Consume the `typeof' token.  */
	cp_lexer_consume_token (parser->lexer);
	/* Parse the operand to `typeof'.  */
	operand = cp_parser_sizeof_operand (parser, RID_TYPEOF);
	/* If it is not already a TYPE, take its type.  */
	if (!TYPE_P (operand))
	  operand = finish_typeof (operand);

	return operand;
      }

    default:
      break;
    }

  /* If the type-specifier was for a built-in type, we're done.  */
  if (type)
    {
      tree id;

      /* Consume the token.  */
      id = cp_lexer_consume_token (parser->lexer)->value;

      /* There is no valid C++ program where a non-template type is
	 followed by a "<".  That usually indicates that the user thought
	 that the type was a template.  */
      cp_parser_check_for_invalid_template_id (parser, type);

      return identifier_p ? id : TYPE_NAME (type);
    }

  /* The type-specifier must be a user-defined type.  */
  if (!(flags & CP_PARSER_FLAGS_NO_USER_DEFINED_TYPES)) 
    {
      bool qualified_p;
      bool global_p;

      /* Don't gobble tokens or issue error messages if this is an
	 optional type-specifier.  */
      if (flags & CP_PARSER_FLAGS_OPTIONAL)
	cp_parser_parse_tentatively (parser);

      /* Look for the optional `::' operator.  */
      global_p
	= (cp_parser_global_scope_opt (parser,
				       /*current_scope_valid_p=*/false)
	   != NULL_TREE);
      /* Look for the nested-name specifier.  */
      qualified_p
	= (cp_parser_nested_name_specifier_opt (parser,
						/*typename_keyword_p=*/false,
						/*check_dependency_p=*/true,
						/*type_p=*/false,
						/*is_declaration=*/false)
	   != NULL_TREE);
      /* If we have seen a nested-name-specifier, and the next token
	 is `template', then we are using the template-id production.  */
      if (parser->scope 
	  && cp_parser_optional_template_keyword (parser))
	{
	  /* Look for the template-id.  */
	  type = cp_parser_template_id (parser, 
					/*template_keyword_p=*/true,
					/*check_dependency_p=*/true,
					/*is_declaration=*/false);
	  /* If the template-id did not name a type, we are out of
	     luck.  */
	  if (TREE_CODE (type) != TYPE_DECL)
	    {
	      cp_parser_error (parser, "expected template-id for type");
	      type = NULL_TREE;
	    }
	}
      /* Otherwise, look for a type-name.  */
      else
	type = cp_parser_type_name (parser);
      /* Keep track of all name-lookups performed in class scopes.  */
      if (type  
	  && !global_p
	  && !qualified_p
	  && TREE_CODE (type) == TYPE_DECL 
	  && TREE_CODE (DECL_NAME (type)) == IDENTIFIER_NODE)
	maybe_note_name_used_in_class (DECL_NAME (type), type);
      /* If it didn't work out, we don't have a TYPE.  */
      if ((flags & CP_PARSER_FLAGS_OPTIONAL) 
	  && !cp_parser_parse_definitely (parser))
	type = NULL_TREE;
    }

  /* If we didn't get a type-name, issue an error message.  */
  if (!type && !(flags & CP_PARSER_FLAGS_OPTIONAL))
    {
      cp_parser_error (parser, "expected type-name");
      return error_mark_node;
    }

  /* There is no valid C++ program where a non-template type is
     followed by a "<".  That usually indicates that the user thought
     that the type was a template.  */
  if (type && type != error_mark_node)
    cp_parser_check_for_invalid_template_id (parser, TREE_TYPE (type));

  return type;
}

/* Parse a type-name.

   type-name:
     class-name
     enum-name
     typedef-name  

   enum-name:
     identifier

   typedef-name:
     identifier 

   Returns a TYPE_DECL for the the type.  */

static tree
cp_parser_type_name (cp_parser* parser)
{
  tree type_decl;
  tree identifier;

  /* We can't know yet whether it is a class-name or not.  */
  cp_parser_parse_tentatively (parser);
  /* Try a class-name.  */
  type_decl = cp_parser_class_name (parser, 
				    /*typename_keyword_p=*/false,
				    /*template_keyword_p=*/false,
				    /*type_p=*/false,
				    /*check_dependency_p=*/true,
				    /*class_head_p=*/false,
				    /*is_declaration=*/false);
  /* If it's not a class-name, keep looking.  */
  if (!cp_parser_parse_definitely (parser))
    {
      /* It must be a typedef-name or an enum-name.  */
      identifier = cp_parser_identifier (parser);
      if (identifier == error_mark_node)
	return error_mark_node;
      
      /* Look up the type-name.  */
      type_decl = cp_parser_lookup_name_simple (parser, identifier);
      /* Issue an error if we did not find a type-name.  */
      if (TREE_CODE (type_decl) != TYPE_DECL)
	{
	  if (!cp_parser_simulate_error (parser))
	    cp_parser_name_lookup_error (parser, identifier, type_decl, 
					 "is not a type");
	  type_decl = error_mark_node;
	}
      /* Remember that the name was used in the definition of the
	 current class so that we can check later to see if the
	 meaning would have been different after the class was
	 entirely defined.  */
      else if (type_decl != error_mark_node
	       && !parser->scope)
	maybe_note_name_used_in_class (identifier, type_decl);
    }
  
  return type_decl;
}


/* Parse an elaborated-type-specifier.  Note that the grammar given
   here incorporates the resolution to DR68.

   elaborated-type-specifier:
     class-key :: [opt] nested-name-specifier [opt] identifier
     class-key :: [opt] nested-name-specifier [opt] template [opt] template-id
     enum :: [opt] nested-name-specifier [opt] identifier
     typename :: [opt] nested-name-specifier identifier
     typename :: [opt] nested-name-specifier template [opt] 
       template-id 

   GNU extension:

   elaborated-type-specifier:
     class-key attributes :: [opt] nested-name-specifier [opt] identifier
     class-key attributes :: [opt] nested-name-specifier [opt] 
               template [opt] template-id
     enum attributes :: [opt] nested-name-specifier [opt] identifier

   If IS_FRIEND is TRUE, then this elaborated-type-specifier is being
   declared `friend'.  If IS_DECLARATION is TRUE, then this
   elaborated-type-specifier appears in a decl-specifiers-seq, i.e.,
   something is being declared.

   Returns the TYPE specified.  */

static tree
cp_parser_elaborated_type_specifier (cp_parser* parser, 
                                     bool is_friend, 
                                     bool is_declaration)
{
  enum tag_types tag_type;
  tree identifier;
  tree type = NULL_TREE;
  tree attributes = NULL_TREE;

  /* See if we're looking at the `enum' keyword.  */
  if (cp_lexer_next_token_is_keyword (parser->lexer, RID_ENUM))
    {
      /* Consume the `enum' token.  */
      cp_lexer_consume_token (parser->lexer);
      /* Remember that it's an enumeration type.  */
      tag_type = enum_type;
      /* Parse the attributes.  */
      attributes = cp_parser_attributes_opt (parser);
    }
  /* Or, it might be `typename'.  */
  else if (cp_lexer_next_token_is_keyword (parser->lexer,
					   RID_TYPENAME))
    {
      /* Consume the `typename' token.  */
      cp_lexer_consume_token (parser->lexer);
      /* Remember that it's a `typename' type.  */
      tag_type = typename_type;
      /* The `typename' keyword is only allowed in templates.  */
      if (!processing_template_decl)
	pedwarn ("using `typename' outside of template");
    }
  /* Otherwise it must be a class-key.  */
  else
    {
      tag_type = cp_parser_class_key (parser);
      if (tag_type == none_type)
	return error_mark_node;
      /* Parse the attributes.  */
      attributes = cp_parser_attributes_opt (parser);
    }

  /* Look for the `::' operator.  */
  cp_parser_global_scope_opt (parser, 
			      /*current_scope_valid_p=*/false);
  /* Look for the nested-name-specifier.  */
  if (tag_type == typename_type)
    {
      if (cp_parser_nested_name_specifier (parser,
					   /*typename_keyword_p=*/true,
					   /*check_dependency_p=*/true,
					   /*type_p=*/true,
					   is_declaration) 
	  == error_mark_node)
	return error_mark_node;
    }
  else
    /* Even though `typename' is not present, the proposed resolution
       to Core Issue 180 says that in `class A<T>::B', `B' should be
       considered a type-name, even if `A<T>' is dependent.  */
    cp_parser_nested_name_specifier_opt (parser,
					 /*typename_keyword_p=*/true,
					 /*check_dependency_p=*/true,
					 /*type_p=*/true,
					 is_declaration);
  /* For everything but enumeration types, consider a template-id.  */
  if (tag_type != enum_type)
    {
      bool template_p = false;
      tree decl;

      /* Allow the `template' keyword.  */
      template_p = cp_parser_optional_template_keyword (parser);
      /* If we didn't see `template', we don't know if there's a
         template-id or not.  */
      if (!template_p)
	cp_parser_parse_tentatively (parser);
      /* Parse the template-id.  */
      decl = cp_parser_template_id (parser, template_p,
				    /*check_dependency_p=*/true,
				    is_declaration);
      /* If we didn't find a template-id, look for an ordinary
         identifier.  */
      if (!template_p && !cp_parser_parse_definitely (parser))
	;
      /* If DECL is a TEMPLATE_ID_EXPR, and the `typename' keyword is
	 in effect, then we must assume that, upon instantiation, the
	 template will correspond to a class.  */
      else if (TREE_CODE (decl) == TEMPLATE_ID_EXPR
	       && tag_type == typename_type)
	type = make_typename_type (parser->scope, decl,
				   /*complain=*/1);
      else 
	type = TREE_TYPE (decl);
    }

  /* For an enumeration type, consider only a plain identifier.  */
  if (!type)
    {
      identifier = cp_parser_identifier (parser);

      if (identifier == error_mark_node)
	{
	  parser->scope = NULL_TREE;
	  return error_mark_node;
	}

      /* For a `typename', we needn't call xref_tag.  */
      if (tag_type == typename_type
	  && TREE_CODE (parser->scope) != NAMESPACE_DECL)
	return make_typename_type (parser->scope, identifier, 
				   /*complain=*/1);
      /* Look up a qualified name in the usual way.  */
      if (parser->scope)
	{
	  tree decl;

	  /* In an elaborated-type-specifier, names are assumed to name
	     types, so we set IS_TYPE to TRUE when calling
	     cp_parser_lookup_name.  */
	  decl = cp_parser_lookup_name (parser, identifier, 
					/*is_type=*/true,
					/*is_template=*/false,
					/*is_namespace=*/false,
					/*check_dependency=*/true);

	  /* If we are parsing friend declaration, DECL may be a
	     TEMPLATE_DECL tree node here.  However, we need to check
	     whether this TEMPLATE_DECL results in valid code.  Consider
	     the following example:

	       namespace N {
		 template <class T> class C {};
	       }
	       class X {
		 template <class T> friend class N::C; // #1, valid code
	       };
	       template <class T> class Y {
		 friend class N::C;		       // #2, invalid code
	       };

	     For both case #1 and #2, we arrive at a TEMPLATE_DECL after
	     name lookup of `N::C'.  We see that friend declaration must
	     be template for the code to be valid.  Note that
	     processing_template_decl does not work here since it is
	     always 1 for the above two cases.  */

	  decl = (cp_parser_maybe_treat_template_as_class 
		  (decl, /*tag_name_p=*/is_friend
			 && parser->num_template_parameter_lists));

	  if (TREE_CODE (decl) != TYPE_DECL)
	    {
	      cp_parser_diagnose_invalid_type_name (parser);
	      return error_mark_node;
	    }

	  if (TREE_CODE (TREE_TYPE (decl)) != TYPENAME_TYPE)
	    check_elaborated_type_specifier 
	      (tag_type, decl,
	       (parser->num_template_parameter_lists
		|| DECL_SELF_REFERENCE_P (decl)));

	  type = TREE_TYPE (decl);
	}
      else 
	{
	  /* An elaborated-type-specifier sometimes introduces a new type and
	     sometimes names an existing type.  Normally, the rule is that it
	     introduces a new type only if there is not an existing type of
	     the same name already in scope.  For example, given:

	       struct S {};
	       void f() { struct S s; }

	     the `struct S' in the body of `f' is the same `struct S' as in
	     the global scope; the existing definition is used.  However, if
	     there were no global declaration, this would introduce a new 
	     local class named `S'.

	     An exception to this rule applies to the following code:

	       namespace N { struct S; }

	     Here, the elaborated-type-specifier names a new type
	     unconditionally; even if there is already an `S' in the
	     containing scope this declaration names a new type.
	     This exception only applies if the elaborated-type-specifier
	     forms the complete declaration:

	       [class.name] 

	       A declaration consisting solely of `class-key identifier ;' is
	       either a redeclaration of the name in the current scope or a
	       forward declaration of the identifier as a class name.  It
	       introduces the name into the current scope.

	     We are in this situation precisely when the next token is a `;'.

	     An exception to the exception is that a `friend' declaration does
	     *not* name a new type; i.e., given:

	       struct S { friend struct T; };

	     `T' is not a new type in the scope of `S'.  

	     Also, `new struct S' or `sizeof (struct S)' never results in the
	     definition of a new type; a new type can only be declared in a
	     declaration context.  */

 	  /* Warn about attributes. They are ignored.  */
 	  if (attributes)
	    warning ("type attributes are honored only at type definition");

	  type = xref_tag (tag_type, identifier, 
			   (is_friend 
			    || !is_declaration
			    || cp_lexer_next_token_is_not (parser->lexer, 
							   CPP_SEMICOLON)),
			   parser->num_template_parameter_lists);
	}
    }
  if (tag_type != enum_type)
    cp_parser_check_class_key (tag_type, type);

  /* A "<" cannot follow an elaborated type specifier.  If that
     happens, the user was probably trying to form a template-id.  */
  cp_parser_check_for_invalid_template_id (parser, type);

  return type;
}

/* Parse an enum-specifier.

   enum-specifier:
     enum identifier [opt] { enumerator-list [opt] }

   Returns an ENUM_TYPE representing the enumeration.  */

static tree
cp_parser_enum_specifier (cp_parser* parser)
{
  cp_token *token;
  tree identifier = NULL_TREE;
  tree type;

  /* Look for the `enum' keyword.  */
  if (!cp_parser_require_keyword (parser, RID_ENUM, "`enum'"))
    return error_mark_node;
  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);

  /* See if it is an identifier.  */
  if (token->type == CPP_NAME)
    identifier = cp_parser_identifier (parser);

  /* Look for the `{'.  */
  if (!cp_parser_require (parser, CPP_OPEN_BRACE, "`{'"))
    return error_mark_node;

  /* At this point, we're going ahead with the enum-specifier, even
     if some other problem occurs.  */
  cp_parser_commit_to_tentative_parse (parser);

  /* Issue an error message if type-definitions are forbidden here.  */
  cp_parser_check_type_definition (parser);

  /* Create the new type.  */
  type = start_enum (identifier ? identifier : make_anon_name ());

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* If it's not a `}', then there are some enumerators.  */
  if (token->type != CPP_CLOSE_BRACE)
    cp_parser_enumerator_list (parser, type);
  /* Look for the `}'.  */
  cp_parser_require (parser, CPP_CLOSE_BRACE, "`}'");

  /* Finish up the enumeration.  */
  finish_enum (type);

  return type;
}

/* Parse an enumerator-list.  The enumerators all have the indicated
   TYPE.  

   enumerator-list:
     enumerator-definition
     enumerator-list , enumerator-definition  */

static void
cp_parser_enumerator_list (cp_parser* parser, tree type)
{
  while (true)
    {
      cp_token *token;

      /* Parse an enumerator-definition.  */
      cp_parser_enumerator_definition (parser, type);
      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);
      /* If it's not a `,', then we've reached the end of the 
	 list.  */
      if (token->type != CPP_COMMA)
	break;
      /* Otherwise, consume the `,' and keep going.  */
      cp_lexer_consume_token (parser->lexer);
      /* If the next token is a `}', there is a trailing comma.  */
      if (cp_lexer_next_token_is (parser->lexer, CPP_CLOSE_BRACE))
	{
	  if (pedantic && !in_system_header)
	    pedwarn ("comma at end of enumerator list");
	  break;
	}
    }
}

/* Parse an enumerator-definition.  The enumerator has the indicated
   TYPE.

   enumerator-definition:
     enumerator
     enumerator = constant-expression
    
   enumerator:
     identifier  */

static void
cp_parser_enumerator_definition (cp_parser* parser, tree type)
{
  cp_token *token;
  tree identifier;
  tree value;

  /* Look for the identifier.  */
  identifier = cp_parser_identifier (parser);
  if (identifier == error_mark_node)
    return;
  
  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* If it's an `=', then there's an explicit value.  */
  if (token->type == CPP_EQ)
    {
      /* Consume the `=' token.  */
      cp_lexer_consume_token (parser->lexer);
      /* Parse the value.  */
      value = cp_parser_constant_expression (parser, 
					     /*allow_non_constant_p=*/false,
					     NULL);
    }
  else
    value = NULL_TREE;

  /* Create the enumerator.  */
  build_enumerator (identifier, value, type);
}

/* Parse a namespace-name.

   namespace-name:
     original-namespace-name
     namespace-alias

   Returns the NAMESPACE_DECL for the namespace.  */

static tree
cp_parser_namespace_name (cp_parser* parser)
{
  tree identifier;
  tree namespace_decl;

  /* Get the name of the namespace.  */
  identifier = cp_parser_identifier (parser);
  if (identifier == error_mark_node)
    return error_mark_node;

  /* Look up the identifier in the currently active scope.  Look only
     for namespaces, due to:

       [basic.lookup.udir]

       When looking up a namespace-name in a using-directive or alias
       definition, only namespace names are considered.  

     And:

       [basic.lookup.qual]

       During the lookup of a name preceding the :: scope resolution
       operator, object, function, and enumerator names are ignored.  

     (Note that cp_parser_class_or_namespace_name only calls this
     function if the token after the name is the scope resolution
     operator.)  */
  namespace_decl = cp_parser_lookup_name (parser, identifier,
					  /*is_type=*/false,
					  /*is_template=*/false,
					  /*is_namespace=*/true,
					  /*check_dependency=*/true);
  /* If it's not a namespace, issue an error.  */
  if (namespace_decl == error_mark_node
      || TREE_CODE (namespace_decl) != NAMESPACE_DECL)
    {
      if (!cp_parser_parsing_tentatively (parser)
	  || cp_parser_committed_to_tentative_parse (parser))
	error ("`%D' is not a namespace-name", identifier);
      cp_parser_error (parser, "expected namespace-name");
      namespace_decl = error_mark_node;
    }
  
  return namespace_decl;
}

/* Parse a namespace-definition.

   namespace-definition:
     named-namespace-definition
     unnamed-namespace-definition  

   named-namespace-definition:
     original-namespace-definition
     extension-namespace-definition

   original-namespace-definition:
     namespace identifier { namespace-body }
   
   extension-namespace-definition:
     namespace original-namespace-name { namespace-body }
 
   unnamed-namespace-definition:
     namespace { namespace-body } */

static void
cp_parser_namespace_definition (cp_parser* parser)
{
  tree identifier;

  /* Look for the `namespace' keyword.  */
  cp_parser_require_keyword (parser, RID_NAMESPACE, "`namespace'");

  /* Get the name of the namespace.  We do not attempt to distinguish
     between an original-namespace-definition and an
     extension-namespace-definition at this point.  The semantic
     analysis routines are responsible for that.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_NAME))
    identifier = cp_parser_identifier (parser);
  else
    identifier = NULL_TREE;

  /* Look for the `{' to start the namespace.  */
  cp_parser_require (parser, CPP_OPEN_BRACE, "`{'");
  /* Start the namespace.  */
  push_namespace (identifier);
  /* Parse the body of the namespace.  */
  cp_parser_namespace_body (parser);
  /* Finish the namespace.  */
  pop_namespace ();
  /* Look for the final `}'.  */
  cp_parser_require (parser, CPP_CLOSE_BRACE, "`}'");
}

/* Parse a namespace-body.

   namespace-body:
     declaration-seq [opt]  */

static void
cp_parser_namespace_body (cp_parser* parser)
{
  cp_parser_declaration_seq_opt (parser);
}

/* Parse a namespace-alias-definition.

   namespace-alias-definition:
     namespace identifier = qualified-namespace-specifier ;  */

static void
cp_parser_namespace_alias_definition (cp_parser* parser)
{
  tree identifier;
  tree namespace_specifier;

  /* Look for the `namespace' keyword.  */
  cp_parser_require_keyword (parser, RID_NAMESPACE, "`namespace'");
  /* Look for the identifier.  */
  identifier = cp_parser_identifier (parser);
  if (identifier == error_mark_node)
    return;
  /* Look for the `=' token.  */
  cp_parser_require (parser, CPP_EQ, "`='");
  /* Look for the qualified-namespace-specifier.  */
  namespace_specifier 
    = cp_parser_qualified_namespace_specifier (parser);
  /* Look for the `;' token.  */
  cp_parser_require (parser, CPP_SEMICOLON, "`;'");

  /* Register the alias in the symbol table.  */
  do_namespace_alias (identifier, namespace_specifier);
}

/* Parse a qualified-namespace-specifier.

   qualified-namespace-specifier:
     :: [opt] nested-name-specifier [opt] namespace-name

   Returns a NAMESPACE_DECL corresponding to the specified
   namespace.  */

static tree
cp_parser_qualified_namespace_specifier (cp_parser* parser)
{
  /* Look for the optional `::'.  */
  cp_parser_global_scope_opt (parser, 
			      /*current_scope_valid_p=*/false);

  /* Look for the optional nested-name-specifier.  */
  cp_parser_nested_name_specifier_opt (parser,
				       /*typename_keyword_p=*/false,
				       /*check_dependency_p=*/true,
				       /*type_p=*/false,
				       /*is_declaration=*/true);

  return cp_parser_namespace_name (parser);
}

/* Parse a using-declaration.

   using-declaration:
     using typename [opt] :: [opt] nested-name-specifier unqualified-id ;
     using :: unqualified-id ;  */

static void
cp_parser_using_declaration (cp_parser* parser)
{
  cp_token *token;
  bool typename_p = false;
  bool global_scope_p;
  tree decl;
  tree identifier;
  tree scope;
  tree qscope;

  /* Look for the `using' keyword.  */
  cp_parser_require_keyword (parser, RID_USING, "`using'");
  
  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* See if it's `typename'.  */
  if (token->keyword == RID_TYPENAME)
    {
      /* Remember that we've seen it.  */
      typename_p = true;
      /* Consume the `typename' token.  */
      cp_lexer_consume_token (parser->lexer);
    }

  /* Look for the optional global scope qualification.  */
  global_scope_p 
    = (cp_parser_global_scope_opt (parser,
				   /*current_scope_valid_p=*/false) 
       != NULL_TREE);

  /* If we saw `typename', or didn't see `::', then there must be a
     nested-name-specifier present.  */
  if (typename_p || !global_scope_p)
    qscope = cp_parser_nested_name_specifier (parser, typename_p, 
					      /*check_dependency_p=*/true,
					      /*type_p=*/false,
					      /*is_declaration=*/true);
  /* Otherwise, we could be in either of the two productions.  In that
     case, treat the nested-name-specifier as optional.  */
  else
    qscope = cp_parser_nested_name_specifier_opt (parser,
						  /*typename_keyword_p=*/false,
						  /*check_dependency_p=*/true,
						  /*type_p=*/false,
						  /*is_declaration=*/true);
  if (!qscope)
    qscope = global_namespace;

  /* Parse the unqualified-id.  */
  identifier = cp_parser_unqualified_id (parser, 
					 /*template_keyword_p=*/false,
					 /*check_dependency_p=*/true,
					 /*declarator_p=*/true);

  /* The function we call to handle a using-declaration is different
     depending on what scope we are in.  */
  if (identifier == error_mark_node)
    ;
  else if (TREE_CODE (identifier) != IDENTIFIER_NODE
	   && TREE_CODE (identifier) != BIT_NOT_EXPR)
    /* [namespace.udecl]

       A using declaration shall not name a template-id.  */
    error ("a template-id may not appear in a using-declaration");
  else
    {
      scope = current_scope ();
      if (scope && TYPE_P (scope))
	{
	  /* Create the USING_DECL.  */
	  decl = do_class_using_decl (build_nt (SCOPE_REF,
						parser->scope,
						identifier));
	  /* Add it to the list of members in this class.  */
	  finish_member_declaration (decl);
	}
      else
	{
	  decl = cp_parser_lookup_name_simple (parser, identifier);
	  if (decl == error_mark_node)
	    cp_parser_name_lookup_error (parser, identifier, decl, NULL);
	  else if (scope)
	    do_local_using_decl (decl, qscope, identifier);
	  else
	    do_toplevel_using_decl (decl, qscope, identifier);
	}
    }

  /* Look for the final `;'.  */
  cp_parser_require (parser, CPP_SEMICOLON, "`;'");
}

/* Parse a using-directive.  
 
   using-directive:
     using namespace :: [opt] nested-name-specifier [opt]
       namespace-name ;  */

static void
cp_parser_using_directive (cp_parser* parser)
{
  tree namespace_decl;
  tree attribs;

  /* Look for the `using' keyword.  */
  cp_parser_require_keyword (parser, RID_USING, "`using'");
  /* And the `namespace' keyword.  */
  cp_parser_require_keyword (parser, RID_NAMESPACE, "`namespace'");
  /* Look for the optional `::' operator.  */
  cp_parser_global_scope_opt (parser, /*current_scope_valid_p=*/false);
  /* And the optional nested-name-specifier.  */
  cp_parser_nested_name_specifier_opt (parser,
				       /*typename_keyword_p=*/false,
				       /*check_dependency_p=*/true,
				       /*type_p=*/false,
				       /*is_declaration=*/true);
  /* Get the namespace being used.  */
  namespace_decl = cp_parser_namespace_name (parser);
  /* And any specified attributes.  */
  attribs = cp_parser_attributes_opt (parser);
  /* Update the symbol table.  */
  parse_using_directive (namespace_decl, attribs);
  /* Look for the final `;'.  */
  cp_parser_require (parser, CPP_SEMICOLON, "`;'");
}

/* Parse an asm-definition.

   asm-definition:
     asm ( string-literal ) ;  

   GNU Extension:

   asm-definition:
     asm volatile [opt] ( string-literal ) ;
     asm volatile [opt] ( string-literal : asm-operand-list [opt] ) ;
     asm volatile [opt] ( string-literal : asm-operand-list [opt]
                          : asm-operand-list [opt] ) ;
     asm volatile [opt] ( string-literal : asm-operand-list [opt] 
                          : asm-operand-list [opt] 
                          : asm-operand-list [opt] ) ;  */

static void
cp_parser_asm_definition (cp_parser* parser)
{
  cp_token *token;
  tree string;
  tree outputs = NULL_TREE;
  tree inputs = NULL_TREE;
  tree clobbers = NULL_TREE;
  tree asm_stmt;
  bool volatile_p = false;
  bool extended_p = false;

  /* Look for the `asm' keyword.  */
  cp_parser_require_keyword (parser, RID_ASM, "`asm'");
  /* See if the next token is `volatile'.  */
  if (cp_parser_allow_gnu_extensions_p (parser)
      && cp_lexer_next_token_is_keyword (parser->lexer, RID_VOLATILE))
    {
      /* Remember that we saw the `volatile' keyword.  */
      volatile_p = true;
      /* Consume the token.  */
      cp_lexer_consume_token (parser->lexer);
    }
  /* Look for the opening `('.  */
  cp_parser_require (parser, CPP_OPEN_PAREN, "`('");
  /* Look for the string.  */
  token = cp_parser_require (parser, CPP_STRING, "asm body");
  if (!token)
    return;
  string = token->value;
  /* If we're allowing GNU extensions, check for the extended assembly
     syntax.  Unfortunately, the `:' tokens need not be separated by 
     a space in C, and so, for compatibility, we tolerate that here
     too.  Doing that means that we have to treat the `::' operator as
     two `:' tokens.  */
  if (cp_parser_allow_gnu_extensions_p (parser)
      && at_function_scope_p ()
      && (cp_lexer_next_token_is (parser->lexer, CPP_COLON)
	  || cp_lexer_next_token_is (parser->lexer, CPP_SCOPE)))
    {
      bool inputs_p = false;
      bool clobbers_p = false;

      /* The extended syntax was used.  */
      extended_p = true;

      /* Look for outputs.  */
      if (cp_lexer_next_token_is (parser->lexer, CPP_COLON))
	{
	  /* Consume the `:'.  */
	  cp_lexer_consume_token (parser->lexer);
	  /* Parse the output-operands.  */
	  if (cp_lexer_next_token_is_not (parser->lexer, 
					  CPP_COLON)
	      && cp_lexer_next_token_is_not (parser->lexer,
					     CPP_SCOPE)
	      && cp_lexer_next_token_is_not (parser->lexer,
					     CPP_CLOSE_PAREN))
	    outputs = cp_parser_asm_operand_list (parser);
	}
      /* If the next token is `::', there are no outputs, and the
	 next token is the beginning of the inputs.  */
      else if (cp_lexer_next_token_is (parser->lexer, CPP_SCOPE))
	/* The inputs are coming next.  */
	inputs_p = true;

      /* Look for inputs.  */
      if (inputs_p
	  || cp_lexer_next_token_is (parser->lexer, CPP_COLON))
	{
	  /* Consume the `:' or `::'.  */
	  cp_lexer_consume_token (parser->lexer);
	  /* Parse the output-operands.  */
	  if (cp_lexer_next_token_is_not (parser->lexer, 
					  CPP_COLON)
	      && cp_lexer_next_token_is_not (parser->lexer,
					     CPP_CLOSE_PAREN))
	    inputs = cp_parser_asm_operand_list (parser);
	}
      else if (cp_lexer_next_token_is (parser->lexer, CPP_SCOPE))
	/* The clobbers are coming next.  */
	clobbers_p = true;

      /* Look for clobbers.  */
      if (clobbers_p 
	  || cp_lexer_next_token_is (parser->lexer, CPP_COLON))
	{
	  /* Consume the `:' or `::'.  */
	  cp_lexer_consume_token (parser->lexer);
	  /* Parse the clobbers.  */
	  if (cp_lexer_next_token_is_not (parser->lexer,
					  CPP_CLOSE_PAREN))
	    clobbers = cp_parser_asm_clobber_list (parser);
	}
    }
  /* Look for the closing `)'.  */
  if (!cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'"))
    cp_parser_skip_to_closing_parenthesis (parser, true, false,
					   /*consume_paren=*/true);
  cp_parser_require (parser, CPP_SEMICOLON, "`;'");

  /* Create the ASM_STMT.  */
  if (at_function_scope_p ())
    {
      asm_stmt = 
	finish_asm_stmt (volatile_p 
			 ? ridpointers[(int) RID_VOLATILE] : NULL_TREE,
			 string, outputs, inputs, clobbers);
      /* If the extended syntax was not used, mark the ASM_STMT.  */
      if (!extended_p)
	ASM_INPUT_P (asm_stmt) = 1;
    }
  else
    assemble_asm (string);
}

/* Declarators [gram.dcl.decl] */

/* Parse an init-declarator.

   init-declarator:
     declarator initializer [opt]

   GNU Extension:

   init-declarator:
     declarator asm-specification [opt] attributes [opt] initializer [opt]

   function-definition:
     decl-specifier-seq [opt] declarator ctor-initializer [opt]
       function-body 
     decl-specifier-seq [opt] declarator function-try-block  

   GNU Extension:

   function-definition:
     __extension__ function-definition 

   The DECL_SPECIFIERS and PREFIX_ATTRIBUTES apply to this declarator.
   Returns a representation of the entity declared.  If MEMBER_P is TRUE,
   then this declarator appears in a class scope.  The new DECL created
   by this declarator is returned.

   If FUNCTION_DEFINITION_ALLOWED_P then we handle the declarator and
   for a function-definition here as well.  If the declarator is a
   declarator for a function-definition, *FUNCTION_DEFINITION_P will
   be TRUE upon return.  By that point, the function-definition will
   have been completely parsed.

   FUNCTION_DEFINITION_P may be NULL if FUNCTION_DEFINITION_ALLOWED_P
   is FALSE.  */

static tree
cp_parser_init_declarator (cp_parser* parser, 
			   tree decl_specifiers, 
			   tree prefix_attributes,
			   bool function_definition_allowed_p,
			   bool member_p,
			   int declares_class_or_enum,
			   bool* function_definition_p)
{
  cp_token *token;
  tree declarator;
  tree attributes;
  tree asm_specification;
  tree initializer;
  tree decl = NULL_TREE;
  tree scope;
  bool is_initialized;
  bool is_parenthesized_init;
  bool is_non_constant_init;
  int ctor_dtor_or_conv_p;
  bool friend_p;
  bool pop_p = false;

  /* Assume that this is not the declarator for a function
     definition.  */
  if (function_definition_p)
    *function_definition_p = false;

  /* Defer access checks while parsing the declarator; we cannot know
     what names are accessible until we know what is being 
     declared.  */
  resume_deferring_access_checks ();

  /* Parse the declarator.  */
  declarator 
    = cp_parser_declarator (parser, CP_PARSER_DECLARATOR_NAMED,
			    &ctor_dtor_or_conv_p,
			    /*parenthesized_p=*/NULL,
			    /*member_p=*/false);
  /* Gather up the deferred checks.  */
  stop_deferring_access_checks ();

  /* If the DECLARATOR was erroneous, there's no need to go
     further.  */
  if (declarator == error_mark_node)
    return error_mark_node;

  if (declares_class_or_enum & 2)
    cp_parser_check_for_definition_in_return_type
      (declarator, TREE_VALUE (decl_specifiers));

  /* Figure out what scope the entity declared by the DECLARATOR is
     located in.  `grokdeclarator' sometimes changes the scope, so
     we compute it now.  */
  scope = get_scope_of_declarator (declarator);

  /* If we're allowing GNU extensions, look for an asm-specification
     and attributes.  */
  if (cp_parser_allow_gnu_extensions_p (parser))
    {
      /* Look for an asm-specification.  */
      asm_specification = cp_parser_asm_specification_opt (parser);
      /* And attributes.  */
      attributes = cp_parser_attributes_opt (parser);
    }
  else
    {
      asm_specification = NULL_TREE;
      attributes = NULL_TREE;
    }

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* Check to see if the token indicates the start of a
     function-definition.  */
  if (cp_parser_token_starts_function_definition_p (token))
    {
      if (!function_definition_allowed_p)
	{
	  /* If a function-definition should not appear here, issue an
	     error message.  */
	  cp_parser_error (parser,
			   "a function-definition is not allowed here");
	  return error_mark_node;
	}
      else
	{
	  /* Neither attributes nor an asm-specification are allowed
	     on a function-definition.  */
	  if (asm_specification)
	    error ("an asm-specification is not allowed on a function-definition");
	  if (attributes)
	    error ("attributes are not allowed on a function-definition");
	  /* This is a function-definition.  */
	  *function_definition_p = true;

	  /* Parse the function definition.  */
	  if (member_p)
	    decl = cp_parser_save_member_function_body (parser,
							decl_specifiers,
							declarator,
							prefix_attributes);
	  else
	    decl 
	      = (cp_parser_function_definition_from_specifiers_and_declarator
		 (parser, decl_specifiers, prefix_attributes, declarator));

	  return decl;
	}
    }

  /* [dcl.dcl]

     Only in function declarations for constructors, destructors, and
     type conversions can the decl-specifier-seq be omitted.  

     We explicitly postpone this check past the point where we handle
     function-definitions because we tolerate function-definitions
     that are missing their return types in some modes.  */
  if (!decl_specifiers && ctor_dtor_or_conv_p <= 0)
    {
      cp_parser_error (parser, 
		       "expected constructor, destructor, or type conversion");
      return error_mark_node;
    }

  /* An `=' or an `(' indicates an initializer.  */
  is_initialized = (token->type == CPP_EQ 
		     || token->type == CPP_OPEN_PAREN);
  /* If the init-declarator isn't initialized and isn't followed by a
     `,' or `;', it's not a valid init-declarator.  */
  if (!is_initialized 
      && token->type != CPP_COMMA
      && token->type != CPP_SEMICOLON)
    {
      cp_parser_error (parser, "expected initializer");
      return error_mark_node;
    }

  /* Because start_decl has side-effects, we should only call it if we
     know we're going ahead.  By this point, we know that we cannot
     possibly be looking at any other construct.  */
  cp_parser_commit_to_tentative_parse (parser);

  /* If the decl specifiers were bad, issue an error now that we're
     sure this was intended to be a declarator.  Then continue
     declaring the variable(s), as int, to try to cut down on further
     errors.  */
  if (decl_specifiers != NULL
      && TREE_VALUE (decl_specifiers) == error_mark_node)
    {
      cp_parser_error (parser, "invalid type in declaration");
      TREE_VALUE (decl_specifiers) = integer_type_node;
    }

  /* Check to see whether or not this declaration is a friend.  */
  friend_p = cp_parser_friend_p (decl_specifiers);

  /* Check that the number of template-parameter-lists is OK.  */
  if (!cp_parser_check_declarator_template_parameters (parser, declarator))
    return error_mark_node;

  /* Enter the newly declared entry in the symbol table.  If we're
     processing a declaration in a class-specifier, we wait until
     after processing the initializer.  */
  if (!member_p)
    {
      if (parser->in_unbraced_linkage_specification_p)
	{
	  decl_specifiers = tree_cons (error_mark_node,
				       get_identifier ("extern"),
				       decl_specifiers);
	  have_extern_spec = false;
	}
      decl = start_decl (declarator, decl_specifiers,
			 is_initialized, attributes, prefix_attributes);
    }

  /* Enter the SCOPE.  That way unqualified names appearing in the
     initializer will be looked up in SCOPE.  */
  if (scope)
    pop_p = push_scope (scope);

  /* Perform deferred access control checks, now that we know in which
     SCOPE the declared entity resides.  */
  if (!member_p && decl) 
    {
      tree saved_current_function_decl = NULL_TREE;

      /* If the entity being declared is a function, pretend that we
	 are in its scope.  If it is a `friend', it may have access to
	 things that would not otherwise be accessible.  */
      if (TREE_CODE (decl) == FUNCTION_DECL)
	{
	  saved_current_function_decl = current_function_decl;
	  current_function_decl = decl;
	}
	
      /* Perform the access control checks for the declarator and the
	 the decl-specifiers.  */
      perform_deferred_access_checks ();

      /* Restore the saved value.  */
      if (TREE_CODE (decl) == FUNCTION_DECL)
	current_function_decl = saved_current_function_decl;
    }

  /* Parse the initializer.  */
  if (is_initialized)
    initializer = cp_parser_initializer (parser, 
					 &is_parenthesized_init,
					 &is_non_constant_init);
  else
    {
      initializer = NULL_TREE;
      is_parenthesized_init = false;
      is_non_constant_init = true;
    }

  /* The old parser allows attributes to appear after a parenthesized
     initializer.  Mark Mitchell proposed removing this functionality
     on the GCC mailing lists on 2002-08-13.  This parser accepts the
     attributes -- but ignores them.  */
  if (cp_parser_allow_gnu_extensions_p (parser) && is_parenthesized_init)
    if (cp_parser_attributes_opt (parser))
      warning ("attributes after parenthesized initializer ignored");

  /* Leave the SCOPE, now that we have processed the initializer.  It
     is important to do this before calling cp_finish_decl because it
     makes decisions about whether to create DECL_STMTs or not based
     on the current scope.  */
  if (pop_p)
    pop_scope (scope);

  /* For an in-class declaration, use `grokfield' to create the
     declaration.  */
  if (member_p)
    {
      decl = grokfield (declarator, decl_specifiers,
			initializer, /*asmspec=*/NULL_TREE,
			/*attributes=*/NULL_TREE);
      if (decl && TREE_CODE (decl) == FUNCTION_DECL)
	cp_parser_save_default_args (parser, decl);
    }
  
  /* Finish processing the declaration.  But, skip friend
     declarations.  */
  if (!friend_p && decl)
    cp_finish_decl (decl, 
		    initializer, 
		    asm_specification,
		    /* If the initializer is in parentheses, then this is
		       a direct-initialization, which means that an
		       `explicit' constructor is OK.  Otherwise, an
		       `explicit' constructor cannot be used.  */
		    ((is_parenthesized_init || !is_initialized)
		     ? 0 : LOOKUP_ONLYCONVERTING));

  /* Remember whether or not variables were initialized by
     constant-expressions.  */
  if (decl && TREE_CODE (decl) == VAR_DECL 
      && is_initialized && !is_non_constant_init)
    DECL_INITIALIZED_BY_CONSTANT_EXPRESSION_P (decl) = true;

  return decl;
}

/* Parse a declarator.
   
   declarator:
     direct-declarator
     ptr-operator declarator  

   abstract-declarator:
     ptr-operator abstract-declarator [opt]
     direct-abstract-declarator

   GNU Extensions:

   declarator:
     attributes [opt] direct-declarator
     attributes [opt] ptr-operator declarator  

   abstract-declarator:
     attributes [opt] ptr-operator abstract-declarator [opt]
     attributes [opt] direct-abstract-declarator
     
   Returns a representation of the declarator.  If the declarator has
   the form `* declarator', then an INDIRECT_REF is returned, whose
   only operand is the sub-declarator.  Analogously, `& declarator' is
   represented as an ADDR_EXPR.  For `X::* declarator', a SCOPE_REF is
   used.  The first operand is the TYPE for `X'.  The second operand
   is an INDIRECT_REF whose operand is the sub-declarator.

   Otherwise, the representation is as for a direct-declarator.

   (It would be better to define a structure type to represent
   declarators, rather than abusing `tree' nodes to represent
   declarators.  That would be much clearer and save some memory.
   There is no reason for declarators to be garbage-collected, for
   example; they are created during parser and no longer needed after
   `grokdeclarator' has been called.)

   For a ptr-operator that has the optional cv-qualifier-seq,
   cv-qualifiers will be stored in the TREE_TYPE of the INDIRECT_REF
   node.

   If CTOR_DTOR_OR_CONV_P is not NULL, *CTOR_DTOR_OR_CONV_P is used to
   detect constructor, destructor or conversion operators. It is set
   to -1 if the declarator is a name, and +1 if it is a
   function. Otherwise it is set to zero. Usually you just want to
   test for >0, but internally the negative value is used.
   
   (The reason for CTOR_DTOR_OR_CONV_P is that a declaration must have
   a decl-specifier-seq unless it declares a constructor, destructor,
   or conversion.  It might seem that we could check this condition in
   semantic analysis, rather than parsing, but that makes it difficult
   to handle something like `f()'.  We want to notice that there are
   no decl-specifiers, and therefore realize that this is an
   expression, not a declaration.)  
 
   If PARENTHESIZED_P is non-NULL, *PARENTHESIZED_P is set to true iff
   the declarator is a direct-declarator of the form "(...)".

   MEMBER_P is true iff this declarator is a member-declarator.  */

static tree
cp_parser_declarator (cp_parser* parser, 
                      cp_parser_declarator_kind dcl_kind, 
                      int* ctor_dtor_or_conv_p,
		      bool* parenthesized_p,
		      bool member_p)
{
  cp_token *token;
  tree declarator;
  enum tree_code code;
  tree cv_qualifier_seq;
  tree class_type;
  tree attributes = NULL_TREE;

  /* Assume this is not a constructor, destructor, or type-conversion
     operator.  */
  if (ctor_dtor_or_conv_p)
    *ctor_dtor_or_conv_p = 0;

  if (cp_parser_allow_gnu_extensions_p (parser))
    attributes = cp_parser_attributes_opt (parser);
  
  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  
  /* Check for the ptr-operator production.  */
  cp_parser_parse_tentatively (parser);
  /* Parse the ptr-operator.  */
  code = cp_parser_ptr_operator (parser, 
				 &class_type, 
				 &cv_qualifier_seq);
  /* If that worked, then we have a ptr-operator.  */
  if (cp_parser_parse_definitely (parser))
    {
      /* If a ptr-operator was found, then this declarator was not
	 parenthesized.  */
      if (parenthesized_p)
	*parenthesized_p = true;
      /* The dependent declarator is optional if we are parsing an
	 abstract-declarator.  */
      if (dcl_kind != CP_PARSER_DECLARATOR_NAMED)
	cp_parser_parse_tentatively (parser);

      /* Parse the dependent declarator.  */
      declarator = cp_parser_declarator (parser, dcl_kind,
					 /*ctor_dtor_or_conv_p=*/NULL,
					 /*parenthesized_p=*/NULL,
					 /*member_p=*/false);

      /* If we are parsing an abstract-declarator, we must handle the
	 case where the dependent declarator is absent.  */
      if (dcl_kind != CP_PARSER_DECLARATOR_NAMED
	  && !cp_parser_parse_definitely (parser))
	declarator = NULL_TREE;
	
      /* Build the representation of the ptr-operator.  */
      if (code == INDIRECT_REF)
	declarator = make_pointer_declarator (cv_qualifier_seq, 
					      declarator);
      else
	declarator = make_reference_declarator (cv_qualifier_seq,
						declarator);
      /* Handle the pointer-to-member case.  */
      if (class_type)
	declarator = build_nt (SCOPE_REF, class_type, declarator);
    }
  /* Everything else is a direct-declarator.  */
  else
    {
      if (parenthesized_p)
	*parenthesized_p = cp_lexer_next_token_is (parser->lexer,
						   CPP_OPEN_PAREN);
      declarator = cp_parser_direct_declarator (parser, dcl_kind,
						ctor_dtor_or_conv_p,
						member_p);
    }

  if (attributes && declarator != error_mark_node)
    declarator = tree_cons (attributes, declarator, NULL_TREE);
  
  return declarator;
}

/* Parse a direct-declarator or direct-abstract-declarator.

   direct-declarator:
     declarator-id
     direct-declarator ( parameter-declaration-clause )
       cv-qualifier-seq [opt] 
       exception-specification [opt]
     direct-declarator [ constant-expression [opt] ]
     ( declarator )  

   direct-abstract-declarator:
     direct-abstract-declarator [opt]
       ( parameter-declaration-clause ) 
       cv-qualifier-seq [opt]
       exception-specification [opt]
     direct-abstract-declarator [opt] [ constant-expression [opt] ]
     ( abstract-declarator )

   Returns a representation of the declarator.  DCL_KIND is
   CP_PARSER_DECLARATOR_ABSTRACT, if we are parsing a
   direct-abstract-declarator.  It is CP_PARSER_DECLARATOR_NAMED, if
   we are parsing a direct-declarator.  It is
   CP_PARSER_DECLARATOR_EITHER, if we can accept either - in the case
   of ambiguity we prefer an abstract declarator, as per
   [dcl.ambig.res].  CTOR_DTOR_OR_CONV_P is as for
   cp_parser_declarator.

   For the declarator-id production, the representation is as for an
   id-expression, except that a qualified name is represented as a
   SCOPE_REF.  A function-declarator is represented as a CALL_EXPR;
   see the documentation of the FUNCTION_DECLARATOR_* macros for
   information about how to find the various declarator components.
   An array-declarator is represented as an ARRAY_REF.  The
   direct-declarator is the first operand; the constant-expression
   indicating the size of the array is the second operand.  */

static tree
cp_parser_direct_declarator (cp_parser* parser,
                             cp_parser_declarator_kind dcl_kind,
                             int* ctor_dtor_or_conv_p,
			     bool member_p)
{
  cp_token *token;
  tree declarator = NULL_TREE;
  tree scope = NULL_TREE;
  bool saved_default_arg_ok_p = parser->default_arg_ok_p;
  bool saved_in_declarator_p = parser->in_declarator_p;
  bool first = true;
  bool pop_p = false;

  while (true)
    {
      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);
      if (token->type == CPP_OPEN_PAREN)
	{
	  /* This is either a parameter-declaration-clause, or a
  	     parenthesized declarator. When we know we are parsing a
  	     named declarator, it must be a parenthesized declarator
  	     if FIRST is true. For instance, `(int)' is a
  	     parameter-declaration-clause, with an omitted
  	     direct-abstract-declarator. But `((*))', is a
  	     parenthesized abstract declarator. Finally, when T is a
  	     template parameter `(T)' is a
  	     parameter-declaration-clause, and not a parenthesized
  	     named declarator.
	     
	     We first try and parse a parameter-declaration-clause,
	     and then try a nested declarator (if FIRST is true).

	     It is not an error for it not to be a
	     parameter-declaration-clause, even when FIRST is
	     false. Consider,

	       int i (int);
	       int i (3);

	     The first is the declaration of a function while the
	     second is a the definition of a variable, including its
	     initializer.

	     Having seen only the parenthesis, we cannot know which of
	     these two alternatives should be selected.  Even more
	     complex are examples like:

               int i (int (a));
	       int i (int (3));

	     The former is a function-declaration; the latter is a
	     variable initialization.  

	     Thus again, we try a parameter-declaration-clause, and if
	     that fails, we back out and return.  */

	  if (!first || dcl_kind != CP_PARSER_DECLARATOR_NAMED)
	    {
	      tree params;
	      unsigned saved_num_template_parameter_lists;
	      
	      /* In a member-declarator, the only valid interpretation
		 of a parenthesis is the start of a
		 parameter-declaration-clause.  (It is invalid to
		 initialize a static data member with a parenthesized
		 initializer; only the "=" form of initialization is
		 permitted.)  */
	      if (!member_p)
		cp_parser_parse_tentatively (parser);

	      /* Consume the `('.  */
	      cp_lexer_consume_token (parser->lexer);
	      if (first)
		{
		  /* If this is going to be an abstract declarator, we're
		     in a declarator and we can't have default args.  */
		  parser->default_arg_ok_p = false;
		  parser->in_declarator_p = true;
		}
	  
	      /* Inside the function parameter list, surrounding
		 template-parameter-lists do not apply.  */
	      saved_num_template_parameter_lists
		= parser->num_template_parameter_lists;
	      parser->num_template_parameter_lists = 0;

	      /* Parse the parameter-declaration-clause.  */
	      params = cp_parser_parameter_declaration_clause (parser);

	      parser->num_template_parameter_lists
		= saved_num_template_parameter_lists;

	      /* If all went well, parse the cv-qualifier-seq and the
	     	 exception-specification.  */
	      if (member_p || cp_parser_parse_definitely (parser))
		{
		  tree cv_qualifiers;
		  tree exception_specification;

		  if (ctor_dtor_or_conv_p)
		    *ctor_dtor_or_conv_p = *ctor_dtor_or_conv_p < 0;
		  first = false;
		  /* Consume the `)'.  */
		  cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");

		  /* Parse the cv-qualifier-seq.  */
		  cv_qualifiers = cp_parser_cv_qualifier_seq_opt (parser);
		  /* And the exception-specification.  */
		  exception_specification 
		    = cp_parser_exception_specification_opt (parser);

		  /* Create the function-declarator.  */
		  declarator = make_call_declarator (declarator,
						     params,
						     cv_qualifiers,
						     exception_specification);
		  /* Any subsequent parameter lists are to do with
	 	     return type, so are not those of the declared
	 	     function.  */
		  parser->default_arg_ok_p = false;
		  
		  /* Repeat the main loop.  */
		  continue;
		}
	    }
	  
	  /* If this is the first, we can try a parenthesized
	     declarator.  */
	  if (first)
	    {
	      bool saved_in_type_id_in_expr_p;

	      parser->default_arg_ok_p = saved_default_arg_ok_p;
	      parser->in_declarator_p = saved_in_declarator_p;
	      
	      /* Consume the `('.  */
	      cp_lexer_consume_token (parser->lexer);
	      /* Parse the nested declarator.  */
	      saved_in_type_id_in_expr_p = parser->in_type_id_in_expr_p;
	      parser->in_type_id_in_expr_p = true;
	      declarator 
		= cp_parser_declarator (parser, dcl_kind, ctor_dtor_or_conv_p,
					/*parenthesized_p=*/NULL,
					member_p);
	      parser->in_type_id_in_expr_p = saved_in_type_id_in_expr_p;
	      first = false;
	      /* Expect a `)'.  */
	      if (!cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'"))
		declarator = error_mark_node;
	      if (declarator == error_mark_node)
		break;
	      
	      goto handle_declarator;
	    }
	  /* Otherwise, we must be done.  */
	  else
	    break;
	}
      else if ((!first || dcl_kind != CP_PARSER_DECLARATOR_NAMED)
	       && token->type == CPP_OPEN_SQUARE)
	{
	  /* Parse an array-declarator.  */
	  tree bounds;

	  if (ctor_dtor_or_conv_p)
	    *ctor_dtor_or_conv_p = 0;
	  
	  first = false;
	  parser->default_arg_ok_p = false;
	  parser->in_declarator_p = true;
	  /* Consume the `['.  */
	  cp_lexer_consume_token (parser->lexer);
	  /* Peek at the next token.  */
	  token = cp_lexer_peek_token (parser->lexer);
	  /* If the next token is `]', then there is no
	     constant-expression.  */
	  if (token->type != CPP_CLOSE_SQUARE)
	    {
	      bool non_constant_p;

	      bounds 
		= cp_parser_constant_expression (parser,
						 /*allow_non_constant=*/true,
						 &non_constant_p);
	      if (!non_constant_p)
		bounds = fold_non_dependent_expr (bounds);
	    }
	  else
	    bounds = NULL_TREE;
	  /* Look for the closing `]'.  */
	  if (!cp_parser_require (parser, CPP_CLOSE_SQUARE, "`]'"))
	    {
	      declarator = error_mark_node;
	      break;
	    }

	  declarator = build_nt (ARRAY_REF, declarator, bounds);
	}
      else if (first && dcl_kind != CP_PARSER_DECLARATOR_ABSTRACT)
	{
	  /* Parse a declarator-id */
	  if (dcl_kind == CP_PARSER_DECLARATOR_EITHER)
	    cp_parser_parse_tentatively (parser);
	  declarator = cp_parser_declarator_id (parser);
	  if (dcl_kind == CP_PARSER_DECLARATOR_EITHER)
	    {
	      if (!cp_parser_parse_definitely (parser))
		declarator = error_mark_node;
	      else if (TREE_CODE (declarator) != IDENTIFIER_NODE)
		{
		  cp_parser_error (parser, "expected unqualified-id");
		  declarator = error_mark_node;
		}
	    }
	  
	  if (declarator == error_mark_node)
	    break;
	  
	  if (TREE_CODE (declarator) == SCOPE_REF
	      && !current_scope ())
	    {
	      tree scope = TREE_OPERAND (declarator, 0);

	      /* In the declaration of a member of a template class
	     	 outside of the class itself, the SCOPE will sometimes
	     	 be a TYPENAME_TYPE.  For example, given:
	     	  
               	 template <typename T>
	       	 int S<T>::R::i = 3;
		  
             	 the SCOPE will be a TYPENAME_TYPE for `S<T>::R'.  In
             	 this context, we must resolve S<T>::R to an ordinary
             	 type, rather than a typename type.
		  
	     	 The reason we normally avoid resolving TYPENAME_TYPEs
	     	 is that a specialization of `S' might render
	     	 `S<T>::R' not a type.  However, if `S' is
	     	 specialized, then this `i' will not be used, so there
	     	 is no harm in resolving the types here.  */
	      if (TREE_CODE (scope) == TYPENAME_TYPE)
		{
		  tree type;

		  /* Resolve the TYPENAME_TYPE.  */
		  type = resolve_typename_type (scope,
						/*only_current_p=*/false);
		  /* If that failed, the declarator is invalid.  */
		  if (type == error_mark_node)
		    error ("`%T::%D' is not a type",
			   TYPE_CONTEXT (scope),
			   TYPE_IDENTIFIER (scope));
		  /* Build a new DECLARATOR.  */
		  declarator = build_nt (SCOPE_REF, 
					 type,
					 TREE_OPERAND (declarator, 1));
		}
	    }
      
	  /* Check to see whether the declarator-id names a constructor, 
	     destructor, or conversion.  */
	  if (declarator && ctor_dtor_or_conv_p 
	      && ((TREE_CODE (declarator) == SCOPE_REF 
		   && CLASS_TYPE_P (TREE_OPERAND (declarator, 0)))
		  || (TREE_CODE (declarator) != SCOPE_REF
		      && at_class_scope_p ())))
	    {
	      tree unqualified_name;
	      tree class_type;

	      /* Get the unqualified part of the name.  */
	      if (TREE_CODE (declarator) == SCOPE_REF)
		{
		  class_type = TREE_OPERAND (declarator, 0);
		  unqualified_name = TREE_OPERAND (declarator, 1);
		}
	      else
		{
		  class_type = current_class_type;
		  unqualified_name = declarator;
		}

	      /* See if it names ctor, dtor or conv.  */
	      if (TREE_CODE (unqualified_name) == BIT_NOT_EXPR
		  || IDENTIFIER_TYPENAME_P (unqualified_name)
		  || constructor_name_p (unqualified_name, class_type)
		  || (TREE_CODE (unqualified_name) == TYPE_DECL
		      && same_type_p (TREE_TYPE (unqualified_name),
				      class_type)))
		*ctor_dtor_or_conv_p = -1;
	    }

	handle_declarator:;
	  scope = get_scope_of_declarator (declarator);
	  if (scope)
	    /* Any names that appear after the declarator-id for a
	       member are looked up in the containing scope.  */
	    pop_p = push_scope (scope);
	  parser->in_declarator_p = true;
	  if ((ctor_dtor_or_conv_p && *ctor_dtor_or_conv_p)
	      || (declarator
		  && (TREE_CODE (declarator) == SCOPE_REF
		      || TREE_CODE (declarator) == IDENTIFIER_NODE)))
	    /* Default args are only allowed on function
	       declarations.  */
	    parser->default_arg_ok_p = saved_default_arg_ok_p;
	  else
	    parser->default_arg_ok_p = false;

	  first = false;
	}
      /* We're done.  */
      else
	break;
    }

  /* For an abstract declarator, we might wind up with nothing at this
     point.  That's an error; the declarator is not optional.  */
  if (!declarator)
    cp_parser_error (parser, "expected declarator");

  /* If we entered a scope, we must exit it now.  */
  if (pop_p)
    pop_scope (scope);

  parser->default_arg_ok_p = saved_default_arg_ok_p;
  parser->in_declarator_p = saved_in_declarator_p;
  
  return declarator;
}

/* Parse a ptr-operator.  

   ptr-operator:
     * cv-qualifier-seq [opt]
     &
     :: [opt] nested-name-specifier * cv-qualifier-seq [opt]

   GNU Extension:

   ptr-operator:
     & cv-qualifier-seq [opt]

   Returns INDIRECT_REF if a pointer, or pointer-to-member, was
   used.  Returns ADDR_EXPR if a reference was used.  In the
   case of a pointer-to-member, *TYPE is filled in with the 
   TYPE containing the member.  *CV_QUALIFIER_SEQ is filled in
   with the cv-qualifier-seq, or NULL_TREE, if there are no
   cv-qualifiers.  Returns ERROR_MARK if an error occurred.  */
   
static enum tree_code
cp_parser_ptr_operator (cp_parser* parser, 
                        tree* type, 
                        tree* cv_qualifier_seq)
{
  enum tree_code code = ERROR_MARK;
  cp_token *token;

  /* Assume that it's not a pointer-to-member.  */
  *type = NULL_TREE;
  /* And that there are no cv-qualifiers.  */
  *cv_qualifier_seq = NULL_TREE;

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* If it's a `*' or `&' we have a pointer or reference.  */
  if (token->type == CPP_MULT || token->type == CPP_AND)
    {
      /* Remember which ptr-operator we were processing.  */
      code = (token->type == CPP_AND ? ADDR_EXPR : INDIRECT_REF);

      /* Consume the `*' or `&'.  */
      cp_lexer_consume_token (parser->lexer);

      /* A `*' can be followed by a cv-qualifier-seq, and so can a
	 `&', if we are allowing GNU extensions.  (The only qualifier
	 that can legally appear after `&' is `restrict', but that is
	 enforced during semantic analysis.  */
      if (code == INDIRECT_REF 
	  || cp_parser_allow_gnu_extensions_p (parser))
	*cv_qualifier_seq = cp_parser_cv_qualifier_seq_opt (parser);
    }
  else
    {
      /* Try the pointer-to-member case.  */
      cp_parser_parse_tentatively (parser);
      /* Look for the optional `::' operator.  */
      cp_parser_global_scope_opt (parser,
				  /*current_scope_valid_p=*/false);
      /* Look for the nested-name specifier.  */
      cp_parser_nested_name_specifier (parser,
				       /*typename_keyword_p=*/false,
				       /*check_dependency_p=*/true,
				       /*type_p=*/false,
				       /*is_declaration=*/false);
      /* If we found it, and the next token is a `*', then we are
	 indeed looking at a pointer-to-member operator.  */
      if (!cp_parser_error_occurred (parser)
	  && cp_parser_require (parser, CPP_MULT, "`*'"))
	{
	  /* The type of which the member is a member is given by the
	     current SCOPE.  */
	  *type = parser->scope;
	  /* The next name will not be qualified.  */
	  parser->scope = NULL_TREE;
	  parser->qualifying_scope = NULL_TREE;
	  parser->object_scope = NULL_TREE;
	  /* Indicate that the `*' operator was used.  */
	  code = INDIRECT_REF;
	  /* Look for the optional cv-qualifier-seq.  */
	  *cv_qualifier_seq = cp_parser_cv_qualifier_seq_opt (parser);
	}
      /* If that didn't work we don't have a ptr-operator.  */
      if (!cp_parser_parse_definitely (parser))
	cp_parser_error (parser, "expected ptr-operator");
    }

  return code;
}

/* Parse an (optional) cv-qualifier-seq.

   cv-qualifier-seq:
     cv-qualifier cv-qualifier-seq [opt]  

   Returns a TREE_LIST.  The TREE_VALUE of each node is the
   representation of a cv-qualifier.  */

static tree
cp_parser_cv_qualifier_seq_opt (cp_parser* parser)
{
  tree cv_qualifiers = NULL_TREE;
  
  while (true)
    {
      tree cv_qualifier;

      /* Look for the next cv-qualifier.  */
      cv_qualifier = cp_parser_cv_qualifier_opt (parser);
      /* If we didn't find one, we're done.  */
      if (!cv_qualifier)
	break;

      /* Add this cv-qualifier to the list.  */
      cv_qualifiers 
	= tree_cons (NULL_TREE, cv_qualifier, cv_qualifiers);
    }

  /* We built up the list in reverse order.  */
  return nreverse (cv_qualifiers);
}

/* Parse an (optional) cv-qualifier.

   cv-qualifier:
     const
     volatile  

   GNU Extension:

   cv-qualifier:
     __restrict__ */

static tree
cp_parser_cv_qualifier_opt (cp_parser* parser)
{
  cp_token *token;
  tree cv_qualifier = NULL_TREE;

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* See if it's a cv-qualifier.  */
  switch (token->keyword)
    {
    case RID_CONST:
    case RID_VOLATILE:
    case RID_RESTRICT:
      /* Save the value of the token.  */
      cv_qualifier = token->value;
      /* Consume the token.  */
      cp_lexer_consume_token (parser->lexer);
      break;

    default:
      break;
    }

  return cv_qualifier;
}

/* Parse a declarator-id.

   declarator-id:
     id-expression
     :: [opt] nested-name-specifier [opt] type-name  

   In the `id-expression' case, the value returned is as for
   cp_parser_id_expression if the id-expression was an unqualified-id.
   If the id-expression was a qualified-id, then a SCOPE_REF is
   returned.  The first operand is the scope (either a NAMESPACE_DECL
   or TREE_TYPE), but the second is still just a representation of an
   unqualified-id.  */

static tree
cp_parser_declarator_id (cp_parser* parser)
{
  tree id_expression;

  /* The expression must be an id-expression.  Assume that qualified
     names are the names of types so that:

       template <class T>
       int S<T>::R::i = 3;

     will work; we must treat `S<T>::R' as the name of a type.
     Similarly, assume that qualified names are templates, where
     required, so that:

       template <class T>
       int S<T>::R<T>::i = 3;

     will work, too.  */
  id_expression = cp_parser_id_expression (parser,
					   /*template_keyword_p=*/false,
					   /*check_dependency_p=*/false,
					   /*template_p=*/NULL,
					   /*declarator_p=*/true);
  /* If the name was qualified, create a SCOPE_REF to represent 
     that.  */
  if (parser->scope && id_expression != error_mark_node)
    {
      id_expression = build_nt (SCOPE_REF, parser->scope, id_expression);
      parser->scope = NULL_TREE;
    }

  return id_expression;
}

/* Parse a type-id.

   type-id:
     type-specifier-seq abstract-declarator [opt]

   Returns the TYPE specified.  */

static tree
cp_parser_type_id (cp_parser* parser)
{
  tree type_specifier_seq;
  tree abstract_declarator;

  /* Parse the type-specifier-seq.  */
  type_specifier_seq 
    = cp_parser_type_specifier_seq (parser);
  if (type_specifier_seq == error_mark_node)
    return error_mark_node;

  /* There might or might not be an abstract declarator.  */
  cp_parser_parse_tentatively (parser);
  /* Look for the declarator.  */
  abstract_declarator 
    = cp_parser_declarator (parser, CP_PARSER_DECLARATOR_ABSTRACT, NULL,
			    /*parenthesized_p=*/NULL,
			    /*member_p=*/false);
  /* Check to see if there really was a declarator.  */
  if (!cp_parser_parse_definitely (parser))
    abstract_declarator = NULL_TREE;

  return groktypename (build_tree_list (type_specifier_seq,
					abstract_declarator));
}

/* Parse a type-specifier-seq.

   type-specifier-seq:
     type-specifier type-specifier-seq [opt]

   GNU extension:

   type-specifier-seq:
     attributes type-specifier-seq [opt]

   Returns a TREE_LIST.  Either the TREE_VALUE of each node is a
   type-specifier, or the TREE_PURPOSE is a list of attributes.  */

static tree
cp_parser_type_specifier_seq (cp_parser* parser)
{
  bool seen_type_specifier = false;
  tree type_specifier_seq = NULL_TREE;

  /* Parse the type-specifiers and attributes.  */
  while (true)
    {
      tree type_specifier;

      /* Check for attributes first.  */
      if (cp_lexer_next_token_is_keyword (parser->lexer, RID_ATTRIBUTE))
	{
	  type_specifier_seq = tree_cons (cp_parser_attributes_opt (parser),
					  NULL_TREE,
					  type_specifier_seq);
	  continue;
	}

      /* After the first type-specifier, others are optional.  */
      if (seen_type_specifier)
	cp_parser_parse_tentatively (parser);
      /* Look for the type-specifier.  */
      type_specifier = cp_parser_type_specifier (parser, 
						 CP_PARSER_FLAGS_NONE,
						 /*is_friend=*/false,
						 /*is_declaration=*/false,
						 NULL,
						 NULL);
      /* If the first type-specifier could not be found, this is not a
	 type-specifier-seq at all.  */
      if (!seen_type_specifier && type_specifier == error_mark_node)
	return error_mark_node;
      /* If subsequent type-specifiers could not be found, the
	 type-specifier-seq is complete.  */
      else if (seen_type_specifier && !cp_parser_parse_definitely (parser))
	break;

      /* Add the new type-specifier to the list.  */
      type_specifier_seq 
	= tree_cons (NULL_TREE, type_specifier, type_specifier_seq);
      seen_type_specifier = true;
    }

  /* We built up the list in reverse order.  */
  return nreverse (type_specifier_seq);
}

/* Parse a parameter-declaration-clause.

   parameter-declaration-clause:
     parameter-declaration-list [opt] ... [opt]
     parameter-declaration-list , ...

   Returns a representation for the parameter declarations.  Each node
   is a TREE_LIST.  (See cp_parser_parameter_declaration for the exact
   representation.)  If the parameter-declaration-clause ends with an
   ellipsis, PARMLIST_ELLIPSIS_P will hold of the first node in the
   list.  A return value of NULL_TREE indicates a
   parameter-declaration-clause consisting only of an ellipsis.  */

static tree
cp_parser_parameter_declaration_clause (cp_parser* parser)
{
  tree parameters;
  cp_token *token;
  bool ellipsis_p;

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* Check for trivial parameter-declaration-clauses.  */
  if (token->type == CPP_ELLIPSIS)
    {
      /* Consume the `...' token.  */
      cp_lexer_consume_token (parser->lexer);
      return NULL_TREE;
    }
  else if (token->type == CPP_CLOSE_PAREN)
    /* There are no parameters.  */
    {
#ifndef NO_IMPLICIT_EXTERN_C
      if (in_system_header && current_class_type == NULL
	  && current_lang_name == lang_name_c)
	return NULL_TREE;
      else
#endif
	return void_list_node;
    }
  /* Check for `(void)', too, which is a special case.  */
  else if (token->keyword == RID_VOID
	   && (cp_lexer_peek_nth_token (parser->lexer, 2)->type 
	       == CPP_CLOSE_PAREN))
    {
      /* Consume the `void' token.  */
      cp_lexer_consume_token (parser->lexer);
      /* There are no parameters.  */
      return void_list_node;
    }
  
  /* Parse the parameter-declaration-list.  */
  parameters = cp_parser_parameter_declaration_list (parser);
  /* If a parse error occurred while parsing the
     parameter-declaration-list, then the entire
     parameter-declaration-clause is erroneous.  */
  if (parameters == error_mark_node)
    return error_mark_node;

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* If it's a `,', the clause should terminate with an ellipsis.  */
  if (token->type == CPP_COMMA)
    {
      /* Consume the `,'.  */
      cp_lexer_consume_token (parser->lexer);
      /* Expect an ellipsis.  */
      ellipsis_p 
	= (cp_parser_require (parser, CPP_ELLIPSIS, "`...'") != NULL);
    }
  /* It might also be `...' if the optional trailing `,' was 
     omitted.  */
  else if (token->type == CPP_ELLIPSIS)
    {
      /* Consume the `...' token.  */
      cp_lexer_consume_token (parser->lexer);
      /* And remember that we saw it.  */
      ellipsis_p = true;
    }
  else
    ellipsis_p = false;

  /* Finish the parameter list.  */
  return finish_parmlist (parameters, ellipsis_p);
}

/* Parse a parameter-declaration-list.

   parameter-declaration-list:
     parameter-declaration
     parameter-declaration-list , parameter-declaration

   Returns a representation of the parameter-declaration-list, as for
   cp_parser_parameter_declaration_clause.  However, the
   `void_list_node' is never appended to the list.  */

static tree
cp_parser_parameter_declaration_list (cp_parser* parser)
{
  tree parameters = NULL_TREE;

  /* Look for more parameters.  */
  while (true)
    {
      tree parameter;
      bool parenthesized_p;
      /* Parse the parameter.  */
      parameter 
	= cp_parser_parameter_declaration (parser, 
					   /*template_parm_p=*/false,
					   &parenthesized_p);

      /* If a parse error occurred parsing the parameter declaration,
	 then the entire parameter-declaration-list is erroneous.  */
      if (parameter == error_mark_node)
	{
	  parameters = error_mark_node;
	  break;
	}
      /* Add the new parameter to the list.  */
      TREE_CHAIN (parameter) = parameters;
      parameters = parameter;

      /* Peek at the next token.  */
      if (cp_lexer_next_token_is (parser->lexer, CPP_CLOSE_PAREN)
	  || cp_lexer_next_token_is (parser->lexer, CPP_ELLIPSIS))
	/* The parameter-declaration-list is complete.  */
	break;
      else if (cp_lexer_next_token_is (parser->lexer, CPP_COMMA))
	{
	  cp_token *token;

	  /* Peek at the next token.  */
	  token = cp_lexer_peek_nth_token (parser->lexer, 2);
	  /* If it's an ellipsis, then the list is complete.  */
	  if (token->type == CPP_ELLIPSIS)
	    break;
	  /* Otherwise, there must be more parameters.  Consume the
	     `,'.  */
	  cp_lexer_consume_token (parser->lexer);
	  /* When parsing something like:

	        int i(float f, double d)
		
             we can tell after seeing the declaration for "f" that we
	     are not looking at an initialization of a variable "i",
	     but rather at the declaration of a function "i".  

	     Due to the fact that the parsing of template arguments
	     (as specified to a template-id) requires backtracking we
	     cannot use this technique when inside a template argument
	     list.  */
	  if (!parser->in_template_argument_list_p
	      && !parser->in_type_id_in_expr_p
	      && cp_parser_parsing_tentatively (parser)
	      && !cp_parser_committed_to_tentative_parse (parser)
	      /* However, a parameter-declaration of the form
		 "foat(f)" (which is a valid declaration of a
		 parameter "f") can also be interpreted as an
		 expression (the conversion of "f" to "float").  */
	      && !parenthesized_p)
	    cp_parser_commit_to_tentative_parse (parser);
	}
      else
	{
	  cp_parser_error (parser, "expected `,' or `...'");
	  if (!cp_parser_parsing_tentatively (parser)
	      || cp_parser_committed_to_tentative_parse (parser))
	    cp_parser_skip_to_closing_parenthesis (parser, 
						   /*recovering=*/true,
						   /*or_comma=*/false,
						   /*consume_paren=*/false);
	  break;
	}
    }

  /* We built up the list in reverse order; straighten it out now.  */
  return nreverse (parameters);
}

/* Parse a parameter declaration.

   parameter-declaration:
     decl-specifier-seq declarator
     decl-specifier-seq declarator = assignment-expression
     decl-specifier-seq abstract-declarator [opt]
     decl-specifier-seq abstract-declarator [opt] = assignment-expression

   If TEMPLATE_PARM_P is TRUE, then this parameter-declaration
   declares a template parameter.  (In that case, a non-nested `>'
   token encountered during the parsing of the assignment-expression
   is not interpreted as a greater-than operator.)

   Returns a TREE_LIST representing the parameter-declaration.  The
   TREE_PURPOSE is the default argument expression, or NULL_TREE if
   there is no default argument.  The TREE_VALUE is a representation
   of the decl-specifier-seq and declarator.  In particular, the
   TREE_VALUE will be a TREE_LIST whose TREE_PURPOSE represents the
   decl-specifier-seq and whose TREE_VALUE represents the declarator.
   If PARENTHESIZED_P is non-NULL, *PARENTHESIZED_P is set to true iff
   the declarator is of the form "(p)".  */

static tree
cp_parser_parameter_declaration (cp_parser *parser, 
				 bool template_parm_p,
				 bool *parenthesized_p)
{
  int declares_class_or_enum;
  bool greater_than_is_operator_p;
  tree decl_specifiers;
  tree attributes;
  tree declarator;
  tree default_argument;
  tree parameter;
  cp_token *token;
  const char *saved_message;

  /* In a template parameter, `>' is not an operator.

     [temp.param]

     When parsing a default template-argument for a non-type
     template-parameter, the first non-nested `>' is taken as the end
     of the template parameter-list rather than a greater-than
     operator.  */
  greater_than_is_operator_p = !template_parm_p;

  /* Type definitions may not appear in parameter types.  */
  saved_message = parser->type_definition_forbidden_message;
  parser->type_definition_forbidden_message 
    = "types may not be defined in parameter types";

  /* Parse the declaration-specifiers.  */
  decl_specifiers 
    = cp_parser_decl_specifier_seq (parser,
				    CP_PARSER_FLAGS_NONE,
				    &attributes,
				    &declares_class_or_enum);
  /* If an error occurred, there's no reason to attempt to parse the
     rest of the declaration.  */
  if (cp_parser_error_occurred (parser))
    {
      parser->type_definition_forbidden_message = saved_message;
      return error_mark_node;
    }

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* If the next token is a `)', `,', `=', `>', or `...', then there
     is no declarator.  */
  if (token->type == CPP_CLOSE_PAREN 
      || token->type == CPP_COMMA
      || token->type == CPP_EQ
      || token->type == CPP_ELLIPSIS
      || token->type == CPP_GREATER)
    {
      declarator = NULL_TREE;
      if (parenthesized_p)
	*parenthesized_p = false;
    }
  /* Otherwise, there should be a declarator.  */
  else
    {
      bool saved_default_arg_ok_p = parser->default_arg_ok_p;
      parser->default_arg_ok_p = false;
  
      /* After seeing a decl-specifier-seq, if the next token is not a
	 "(", there is no possibility that the code is a valid
	 expression.  Therefore, if parsing tentatively, we commit at
	 this point.  */
      if (!parser->in_template_argument_list_p
	  /* In an expression context, having seen:

	       (int((char ...

	     we cannot be sure whether we are looking at a
	     function-type (taking a "char" as a parameter) or a cast
	     of some object of type "char" to "int".  */
	  && !parser->in_type_id_in_expr_p
	  && cp_parser_parsing_tentatively (parser)
	  && !cp_parser_committed_to_tentative_parse (parser)
	  && cp_lexer_next_token_is_not (parser->lexer, CPP_OPEN_PAREN))
	cp_parser_commit_to_tentative_parse (parser);
      /* Parse the declarator.  */
      declarator = cp_parser_declarator (parser,
					 CP_PARSER_DECLARATOR_EITHER,
					 /*ctor_dtor_or_conv_p=*/NULL,
					 parenthesized_p,
					 /*member_p=*/false);
      parser->default_arg_ok_p = saved_default_arg_ok_p;
      /* After the declarator, allow more attributes.  */
      attributes = chainon (attributes, cp_parser_attributes_opt (parser));
    }

  /* The restriction on defining new types applies only to the type
     of the parameter, not to the default argument.  */
  parser->type_definition_forbidden_message = saved_message;

  /* If the next token is `=', then process a default argument.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_EQ))
    {
      bool saved_greater_than_is_operator_p;
      /* Consume the `='.  */
      cp_lexer_consume_token (parser->lexer);

      /* If we are defining a class, then the tokens that make up the
	 default argument must be saved and processed later.  */
      if (!template_parm_p && at_class_scope_p () 
	  && TYPE_BEING_DEFINED (current_class_type))
	{
	  unsigned depth = 0;

	  /* Create a DEFAULT_ARG to represented the unparsed default
             argument.  */
	  default_argument = make_node (DEFAULT_ARG);
	  DEFARG_TOKENS (default_argument) = cp_token_cache_new ();

	  /* Add tokens until we have processed the entire default
	     argument.  */
	  while (true)
	    {
	      bool done = false;
	      cp_token *token;

	      /* Peek at the next token.  */
	      token = cp_lexer_peek_token (parser->lexer);
	      /* What we do depends on what token we have.  */
	      switch (token->type)
		{
		  /* In valid code, a default argument must be
		     immediately followed by a `,' `)', or `...'.  */
		case CPP_COMMA:
		case CPP_CLOSE_PAREN:
		case CPP_ELLIPSIS:
		  /* If we run into a non-nested `;', `}', or `]',
		     then the code is invalid -- but the default
		     argument is certainly over.  */
		case CPP_SEMICOLON:
		case CPP_CLOSE_BRACE:
		case CPP_CLOSE_SQUARE:
		  if (depth == 0)
		    done = true;
		  /* Update DEPTH, if necessary.  */
		  else if (token->type == CPP_CLOSE_PAREN
			   || token->type == CPP_CLOSE_BRACE
			   || token->type == CPP_CLOSE_SQUARE)
		    --depth;
		  break;

		case CPP_OPEN_PAREN:
		case CPP_OPEN_SQUARE:
		case CPP_OPEN_BRACE:
		  ++depth;
		  break;

		case CPP_GREATER:
		  /* If we see a non-nested `>', and `>' is not an
		     operator, then it marks the end of the default
		     argument.  */
		  if (!depth && !greater_than_is_operator_p)
		    done = true;
		  break;

		  /* If we run out of tokens, issue an error message.  */
		case CPP_EOF:
		  error ("file ends in default argument");
		  done = true;
		  break;

		case CPP_NAME:
		case CPP_SCOPE:
		  /* In these cases, we should look for template-ids.
		     For example, if the default argument is 
		     `X<int, double>()', we need to do name lookup to
		     figure out whether or not `X' is a template; if
		     so, the `,' does not end the default argument.

		     That is not yet done.  */
		  break;

		default:
		  break;
		}

	      /* If we've reached the end, stop.  */
	      if (done)
		break;
	      
	      /* Add the token to the token block.  */
	      token = cp_lexer_consume_token (parser->lexer);
	      cp_token_cache_push_token (DEFARG_TOKENS (default_argument),
					 token);
	    }
	}
      /* Outside of a class definition, we can just parse the
         assignment-expression.  */
      else
	{
	  bool saved_local_variables_forbidden_p;

	  /* Make sure that PARSER->GREATER_THAN_IS_OPERATOR_P is
	     set correctly.  */
	  saved_greater_than_is_operator_p 
	    = parser->greater_than_is_operator_p;
	  parser->greater_than_is_operator_p = greater_than_is_operator_p;
	  /* Local variable names (and the `this' keyword) may not
	     appear in a default argument.  */
	  saved_local_variables_forbidden_p 
	    = parser->local_variables_forbidden_p;
	  parser->local_variables_forbidden_p = true;
	  /* Parse the assignment-expression.  */
	  default_argument = cp_parser_assignment_expression (parser);
	  /* Restore saved state.  */
	  parser->greater_than_is_operator_p 
	    = saved_greater_than_is_operator_p;
	  parser->local_variables_forbidden_p 
	    = saved_local_variables_forbidden_p; 
	}
      if (!parser->default_arg_ok_p)
	{
	  if (!flag_pedantic_errors)
	    warning ("deprecated use of default argument for parameter of non-function");
	  else
	    {
	      error ("default arguments are only permitted for function parameters");
	      default_argument = NULL_TREE;
	    }
	}
    }
  else
    default_argument = NULL_TREE;
  
  /* Create the representation of the parameter.  */
  if (attributes)
    decl_specifiers = tree_cons (attributes, NULL_TREE, decl_specifiers);
  parameter = build_tree_list (default_argument, 
			       build_tree_list (decl_specifiers,
						declarator));

  return parameter;
}

/* Parse a function-body.

   function-body:
     compound_statement  */

static void
cp_parser_function_body (cp_parser *parser)
{
  cp_parser_compound_statement (parser, false);
}

/* Parse a ctor-initializer-opt followed by a function-body.  Return
   true if a ctor-initializer was present.  */

static bool
cp_parser_ctor_initializer_opt_and_function_body (cp_parser *parser)
{
  tree body;
  bool ctor_initializer_p;

  /* Begin the function body.  */
  body = begin_function_body ();
  /* Parse the optional ctor-initializer.  */
  ctor_initializer_p = cp_parser_ctor_initializer_opt (parser);
  /* Parse the function-body.  */
  cp_parser_function_body (parser);
  /* Finish the function body.  */
  finish_function_body (body);

  return ctor_initializer_p;
}

/* Parse an initializer.

   initializer:
     = initializer-clause
     ( expression-list )  

   Returns a expression representing the initializer.  If no
   initializer is present, NULL_TREE is returned.  

   *IS_PARENTHESIZED_INIT is set to TRUE if the `( expression-list )'
   production is used, and zero otherwise.  *IS_PARENTHESIZED_INIT is
   set to FALSE if there is no initializer present.  If there is an
   initializer, and it is not a constant-expression, *NON_CONSTANT_P
   is set to true; otherwise it is set to false.  */

static tree
cp_parser_initializer (cp_parser* parser, bool* is_parenthesized_init,
		       bool* non_constant_p)
{
  cp_token *token;
  tree init;

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);

  /* Let our caller know whether or not this initializer was
     parenthesized.  */
  *is_parenthesized_init = (token->type == CPP_OPEN_PAREN);
  /* Assume that the initializer is constant.  */
  *non_constant_p = false;

  if (token->type == CPP_EQ)
    {
      /* Consume the `='.  */
      cp_lexer_consume_token (parser->lexer);
      /* Parse the initializer-clause.  */
      init = cp_parser_initializer_clause (parser, non_constant_p);
    }
  else if (token->type == CPP_OPEN_PAREN)
    init = cp_parser_parenthesized_expression_list (parser, false,
						    non_constant_p);
  else
    {
      /* Anything else is an error.  */
      cp_parser_error (parser, "expected initializer");
      init = error_mark_node;
    }

  return init;
}

/* Parse an initializer-clause.  

   initializer-clause:
     assignment-expression
     { initializer-list , [opt] }
     { }

   Returns an expression representing the initializer.  

   If the `assignment-expression' production is used the value
   returned is simply a representation for the expression.  

   Otherwise, a CONSTRUCTOR is returned.  The CONSTRUCTOR_ELTS will be
   the elements of the initializer-list (or NULL_TREE, if the last
   production is used).  The TREE_TYPE for the CONSTRUCTOR will be
   NULL_TREE.  There is no way to detect whether or not the optional
   trailing `,' was provided.  NON_CONSTANT_P is as for
   cp_parser_initializer.  */

static tree
cp_parser_initializer_clause (cp_parser* parser, bool* non_constant_p)
{
  tree initializer = NULL_TREE;

  /* Assume the expression is constant.  */
  *non_constant_p = false;

  /* If it is not a `{', then we are looking at an
     assignment-expression.  */
  if (cp_lexer_next_token_is_not (parser->lexer, CPP_OPEN_BRACE))
    {
      /* Speed up common initializers (simply a literal).  */
      cp_token* token = cp_lexer_peek_token (parser->lexer);
      cp_token* token2 = cp_lexer_peek_nth_token (parser->lexer, 2);

      if (token2->type == CPP_COMMA)
	switch (token->type)
	  {
	  case CPP_CHAR:
	  case CPP_WCHAR:
	  case CPP_NUMBER:
	    token = cp_lexer_consume_token (parser->lexer);
	    initializer = token->value;
	    break;

	  case CPP_STRING:
	  case CPP_WSTRING:
	    token = cp_lexer_consume_token (parser->lexer);
	    if (TREE_CHAIN (token->value))
	      initializer = TREE_CHAIN (token->value);
	    else
	      initializer = token->value;
	    break;

	  default:
	    break;
	  }

      /* Otherwise, fall back to the generic assignment expression.  */
      if (!initializer)
	{
	  initializer
	    = cp_parser_constant_expression (parser,
					    /*allow_non_constant_p=*/true,
					    non_constant_p);
	  if (!*non_constant_p)
	    initializer = fold_non_dependent_expr (initializer);
	}
    }
  else
    {
      /* Consume the `{' token.  */
      cp_lexer_consume_token (parser->lexer);
      /* Create a CONSTRUCTOR to represent the braced-initializer.  */
      initializer = make_node (CONSTRUCTOR);
      /* Mark it with TREE_HAS_CONSTRUCTOR.  This should not be
	 necessary, but check_initializer depends upon it, for 
	 now.  */
      TREE_HAS_CONSTRUCTOR (initializer) = 1;
      /* If it's not a `}', then there is a non-trivial initializer.  */
      if (cp_lexer_next_token_is_not (parser->lexer, CPP_CLOSE_BRACE))
	{
	  /* Parse the initializer list.  */
	  CONSTRUCTOR_ELTS (initializer)
	    = cp_parser_initializer_list (parser, non_constant_p);
	  /* A trailing `,' token is allowed.  */
	  if (cp_lexer_next_token_is (parser->lexer, CPP_COMMA))
	    cp_lexer_consume_token (parser->lexer);
	}
      /* Now, there should be a trailing `}'.  */
      cp_parser_require (parser, CPP_CLOSE_BRACE, "`}'");
    }

  return initializer;
}

/* Parse an initializer-list.

   initializer-list:
     initializer-clause
     initializer-list , initializer-clause

   GNU Extension:
   
   initializer-list:
     identifier : initializer-clause
     initializer-list, identifier : initializer-clause

   Returns a TREE_LIST.  The TREE_VALUE of each node is an expression
   for the initializer.  If the TREE_PURPOSE is non-NULL, it is the
   IDENTIFIER_NODE naming the field to initialize.  NON_CONSTANT_P is
   as for cp_parser_initializer.  */

static tree
cp_parser_initializer_list (cp_parser* parser, bool* non_constant_p)
{
  tree initializers = NULL_TREE;

  /* Assume all of the expressions are constant.  */
  *non_constant_p = false;

  /* Parse the rest of the list.  */
  while (true)
    {
      cp_token *token;
      tree identifier;
      tree initializer;
      bool clause_non_constant_p;

      /* If the next token is an identifier and the following one is a
	 colon, we are looking at the GNU designated-initializer
	 syntax.  */
      if (cp_parser_allow_gnu_extensions_p (parser)
	  && cp_lexer_next_token_is (parser->lexer, CPP_NAME)
	  && cp_lexer_peek_nth_token (parser->lexer, 2)->type == CPP_COLON)
	{
	  /* Consume the identifier.  */
	  identifier = cp_lexer_consume_token (parser->lexer)->value;
	  /* Consume the `:'.  */
	  cp_lexer_consume_token (parser->lexer);
	}
      else
	identifier = NULL_TREE;

      /* Parse the initializer.  */
      initializer = cp_parser_initializer_clause (parser, 
						  &clause_non_constant_p);
      /* If any clause is non-constant, so is the entire initializer.  */
      if (clause_non_constant_p)
	*non_constant_p = true;
      /* Add it to the list.  */
      initializers = tree_cons (identifier, initializer, initializers);

      /* If the next token is not a comma, we have reached the end of
	 the list.  */
      if (cp_lexer_next_token_is_not (parser->lexer, CPP_COMMA))
	break;

      /* Peek at the next token.  */
      token = cp_lexer_peek_nth_token (parser->lexer, 2);
      /* If the next token is a `}', then we're still done.  An
	 initializer-clause can have a trailing `,' after the
	 initializer-list and before the closing `}'.  */
      if (token->type == CPP_CLOSE_BRACE)
	break;

      /* Consume the `,' token.  */
      cp_lexer_consume_token (parser->lexer);
    }

  /* The initializers were built up in reverse order, so we need to
     reverse them now.  */
  return nreverse (initializers);
}

/* Classes [gram.class] */

/* Parse a class-name.

   class-name:
     identifier
     template-id

   TYPENAME_KEYWORD_P is true iff the `typename' keyword has been used
   to indicate that names looked up in dependent types should be
   assumed to be types.  TEMPLATE_KEYWORD_P is true iff the `template'
   keyword has been used to indicate that the name that appears next
   is a template.  TYPE_P is true iff the next name should be treated
   as class-name, even if it is declared to be some other kind of name
   as well.  If CHECK_DEPENDENCY_P is FALSE, names are looked up in
   dependent scopes.  If CLASS_HEAD_P is TRUE, this class is the class
   being defined in a class-head.

   Returns the TYPE_DECL representing the class.  */

static tree
cp_parser_class_name (cp_parser *parser, 
		      bool typename_keyword_p, 
		      bool template_keyword_p, 
		      bool type_p,
		      bool check_dependency_p,
		      bool class_head_p,
		      bool is_declaration)
{
  tree decl;
  tree scope;
  bool typename_p;
  cp_token *token;

  /* All class-names start with an identifier.  */
  token = cp_lexer_peek_token (parser->lexer);
  if (token->type != CPP_NAME && token->type != CPP_TEMPLATE_ID)
    {
      cp_parser_error (parser, "expected class-name");
      return error_mark_node;
    }
    
  /* PARSER->SCOPE can be cleared when parsing the template-arguments
     to a template-id, so we save it here.  */
  scope = parser->scope;
  if (scope == error_mark_node)
    return error_mark_node;
  
  /* Any name names a type if we're following the `typename' keyword
     in a qualified name where the enclosing scope is type-dependent.  */
  typename_p = (typename_keyword_p && scope && TYPE_P (scope)
		&& dependent_type_p (scope));
  /* Handle the common case (an identifier, but not a template-id)
     efficiently.  */
  if (token->type == CPP_NAME 
      && !cp_parser_nth_token_starts_template_argument_list_p (parser, 2))
    {
      tree identifier;

      /* Look for the identifier.  */
      identifier = cp_parser_identifier (parser);
      /* If the next token isn't an identifier, we are certainly not
	 looking at a class-name.  */
      if (identifier == error_mark_node)
	decl = error_mark_node;
      /* If we know this is a type-name, there's no need to look it
	 up.  */
      else if (typename_p)
	decl = identifier;
      else
	{
	  /* If the next token is a `::', then the name must be a type
	     name.

	     [basic.lookup.qual]

	     During the lookup for a name preceding the :: scope
	     resolution operator, object, function, and enumerator
	     names are ignored.  */
	  if (cp_lexer_next_token_is (parser->lexer, CPP_SCOPE))
	    type_p = true;
	  /* Look up the name.  */
	  decl = cp_parser_lookup_name (parser, identifier, 
					type_p,
					/*is_template=*/false,
					/*is_namespace=*/false,
					check_dependency_p);
	}
    }
  else
    {
      /* Try a template-id.  */
      decl = cp_parser_template_id (parser, template_keyword_p,
				    check_dependency_p,
				    is_declaration);
      if (decl == error_mark_node)
	return error_mark_node;
    }

  decl = cp_parser_maybe_treat_template_as_class (decl, class_head_p);

  /* If this is a typename, create a TYPENAME_TYPE.  */
  if (typename_p && decl != error_mark_node)
    {
      decl = make_typename_type (scope, decl, /*complain=*/1);
      if (decl != error_mark_node)
	decl = TYPE_NAME (decl);
    }

  /* Check to see that it is really the name of a class.  */
  if (TREE_CODE (decl) == TEMPLATE_ID_EXPR 
      && TREE_CODE (TREE_OPERAND (decl, 0)) == IDENTIFIER_NODE
      && cp_lexer_next_token_is (parser->lexer, CPP_SCOPE))
    /* Situations like this:

	 template <typename T> struct A {
	   typename T::template X<int>::I i; 
	 };

       are problematic.  Is `T::template X<int>' a class-name?  The
       standard does not seem to be definitive, but there is no other
       valid interpretation of the following `::'.  Therefore, those
       names are considered class-names.  */
    decl = TYPE_NAME (make_typename_type (scope, decl, tf_error));
  else if (decl == error_mark_node
	   || TREE_CODE (decl) != TYPE_DECL
	   || !IS_AGGR_TYPE (TREE_TYPE (decl)))
    {
      cp_parser_error (parser, "expected class-name");
      return error_mark_node;
    }

  return decl;
}

/* Parse a class-specifier.

   class-specifier:
     class-head { member-specification [opt] }

   Returns the TREE_TYPE representing the class.  */

static tree
cp_parser_class_specifier (cp_parser* parser)
{
  cp_token *token;
  tree type;
  tree attributes;
  int has_trailing_semicolon;
  bool nested_name_specifier_p;
  unsigned saved_num_template_parameter_lists;
  bool pop_p = false;
  tree scope = NULL_TREE;

  push_deferring_access_checks (dk_no_deferred);

  /* Parse the class-head.  */
  type = cp_parser_class_head (parser,
			       &nested_name_specifier_p,
			       &attributes);
  /* If the class-head was a semantic disaster, skip the entire body
     of the class.  */
  if (!type)
    {
      cp_parser_skip_to_end_of_block_or_statement (parser);
      pop_deferring_access_checks ();
      return error_mark_node;
    }

  /* Look for the `{'.  */
  if (!cp_parser_require (parser, CPP_OPEN_BRACE, "`{'"))
    {
      pop_deferring_access_checks ();
      return error_mark_node;
    }

  /* Issue an error message if type-definitions are forbidden here.  */
  cp_parser_check_type_definition (parser);
  /* Remember that we are defining one more class.  */
  ++parser->num_classes_being_defined;
  /* Inside the class, surrounding template-parameter-lists do not
     apply.  */
  saved_num_template_parameter_lists 
    = parser->num_template_parameter_lists; 
  parser->num_template_parameter_lists = 0;

  /* Start the class.  */
  if (nested_name_specifier_p)
    {
      scope = CP_DECL_CONTEXT (TYPE_MAIN_DECL (type));
      pop_p = push_scope (scope);
    }
  type = begin_class_definition (type);
  if (type == error_mark_node)
    /* If the type is erroneous, skip the entire body of the class.  */
    cp_parser_skip_to_closing_brace (parser);
  else
    /* Parse the member-specification.  */
    cp_parser_member_specification_opt (parser);
  /* Look for the trailing `}'.  */
  cp_parser_require (parser, CPP_CLOSE_BRACE, "`}'");
  /* We get better error messages by noticing a common problem: a
     missing trailing `;'.  */
  token = cp_lexer_peek_token (parser->lexer);
  has_trailing_semicolon = (token->type == CPP_SEMICOLON);
  /* Look for trailing attributes to apply to this class.  */
  if (cp_parser_allow_gnu_extensions_p (parser))
    {
      tree sub_attr = cp_parser_attributes_opt (parser);
      attributes = chainon (attributes, sub_attr);
    }
  if (type != error_mark_node)
    type = finish_struct (type, attributes);
  if (pop_p)
    pop_scope (scope);
  /* If this class is not itself within the scope of another class,
     then we need to parse the bodies of all of the queued function
     definitions.  Note that the queued functions defined in a class
     are not always processed immediately following the
     class-specifier for that class.  Consider:

       struct A {
         struct B { void f() { sizeof (A); } };
       };

     If `f' were processed before the processing of `A' were
     completed, there would be no way to compute the size of `A'.
     Note that the nesting we are interested in here is lexical --
     not the semantic nesting given by TYPE_CONTEXT.  In particular,
     for:

       struct A { struct B; };
       struct A::B { void f() { } };

     there is no need to delay the parsing of `A::B::f'.  */
  if (--parser->num_classes_being_defined == 0) 
    {
      tree queue_entry;
      tree fn;

      /* In a first pass, parse default arguments to the functions.
	 Then, in a second pass, parse the bodies of the functions.
	 This two-phased approach handles cases like:
	 
	    struct S { 
              void f() { g(); } 
              void g(int i = 3);
            };

         */
      for (TREE_PURPOSE (parser->unparsed_functions_queues)
	     = nreverse (TREE_PURPOSE (parser->unparsed_functions_queues));
	   (queue_entry = TREE_PURPOSE (parser->unparsed_functions_queues));
	   TREE_PURPOSE (parser->unparsed_functions_queues)
	     = TREE_CHAIN (TREE_PURPOSE (parser->unparsed_functions_queues)))
	{
	  fn = TREE_VALUE (queue_entry);
	  /* Make sure that any template parameters are in scope.  */
	  maybe_begin_member_template_processing (fn);
	  /* If there are default arguments that have not yet been processed,
	     take care of them now.  */
	  cp_parser_late_parsing_default_args (parser, fn);
	  /* Remove any template parameters from the symbol table.  */
	  maybe_end_member_template_processing ();
	}
      /* Now parse the body of the functions.  */
      for (TREE_VALUE (parser->unparsed_functions_queues)
	     = nreverse (TREE_VALUE (parser->unparsed_functions_queues));
	   (queue_entry = TREE_VALUE (parser->unparsed_functions_queues));
	   TREE_VALUE (parser->unparsed_functions_queues)
	     = TREE_CHAIN (TREE_VALUE (parser->unparsed_functions_queues)))
	{
	  /* Figure out which function we need to process.  */
	  fn = TREE_VALUE (queue_entry);

	  /* A hack to prevent garbage collection.  */
	  function_depth++;

	  /* Parse the function.  */
	  cp_parser_late_parsing_for_member (parser, fn);
	  function_depth--;
	}

    }

  /* Put back any saved access checks.  */
  pop_deferring_access_checks ();

  /* Restore the count of active template-parameter-lists.  */
  parser->num_template_parameter_lists
    = saved_num_template_parameter_lists;

  return type;
}

/* Parse a class-head.

   class-head:
     class-key identifier [opt] base-clause [opt]
     class-key nested-name-specifier identifier base-clause [opt]
     class-key nested-name-specifier [opt] template-id 
       base-clause [opt]  

   GNU Extensions:
     class-key attributes identifier [opt] base-clause [opt]
     class-key attributes nested-name-specifier identifier base-clause [opt]
     class-key attributes nested-name-specifier [opt] template-id 
       base-clause [opt]  

   Returns the TYPE of the indicated class.  Sets
   *NESTED_NAME_SPECIFIER_P to TRUE iff one of the productions
   involving a nested-name-specifier was used, and FALSE otherwise.

   Returns NULL_TREE if the class-head is syntactically valid, but
   semantically invalid in a way that means we should skip the entire
   body of the class.  */

static tree
cp_parser_class_head (cp_parser* parser, 
		      bool* nested_name_specifier_p,
		      tree *attributes_p)
{
  cp_token *token;
  tree nested_name_specifier;
  enum tag_types class_key;
  tree id = NULL_TREE;
  tree type = NULL_TREE;
  tree attributes;
  bool template_id_p = false;
  bool qualified_p = false;
  bool invalid_nested_name_p = false;
  bool invalid_explicit_specialization_p = false;
  bool pop_p = false;
  unsigned num_templates;

  /* Assume no nested-name-specifier will be present.  */
  *nested_name_specifier_p = false;
  /* Assume no template parameter lists will be used in defining the
     type.  */
  num_templates = 0;

  /* Look for the class-key.  */
  class_key = cp_parser_class_key (parser);
  if (class_key == none_type)
    return error_mark_node;

  /* Parse the attributes.  */
  attributes = cp_parser_attributes_opt (parser);

  /* If the next token is `::', that is invalid -- but sometimes
     people do try to write:

       struct ::S {};  

     Handle this gracefully by accepting the extra qualifier, and then
     issuing an error about it later if this really is a
     class-head.  If it turns out just to be an elaborated type
     specifier, remain silent.  */
  if (cp_parser_global_scope_opt (parser, /*current_scope_valid_p=*/false))
    qualified_p = true;

  push_deferring_access_checks (dk_no_check);

  /* Determine the name of the class.  Begin by looking for an
     optional nested-name-specifier.  */
  nested_name_specifier 
    = cp_parser_nested_name_specifier_opt (parser,
					   /*typename_keyword_p=*/false,
					   /*check_dependency_p=*/false,
					   /*type_p=*/false,
					   /*is_declaration=*/false);
  /* If there was a nested-name-specifier, then there *must* be an
     identifier.  */
  if (nested_name_specifier)
    {
      /* Although the grammar says `identifier', it really means
	 `class-name' or `template-name'.  You are only allowed to
	 define a class that has already been declared with this
	 syntax.  

	 The proposed resolution for Core Issue 180 says that whever
	 you see `class T::X' you should treat `X' as a type-name.
	 
	 It is OK to define an inaccessible class; for example:
	 
           class A { class B; };
           class A::B {};
	 
         We do not know if we will see a class-name, or a
	 template-name.  We look for a class-name first, in case the
	 class-name is a template-id; if we looked for the
	 template-name first we would stop after the template-name.  */
      cp_parser_parse_tentatively (parser);
      type = cp_parser_class_name (parser,
				   /*typename_keyword_p=*/false,
				   /*template_keyword_p=*/false,
				   /*type_p=*/true,
				   /*check_dependency_p=*/false,
				   /*class_head_p=*/true,
				   /*is_declaration=*/false);
      /* If that didn't work, ignore the nested-name-specifier.  */
      if (!cp_parser_parse_definitely (parser))
	{
	  invalid_nested_name_p = true;
	  id = cp_parser_identifier (parser);
	  if (id == error_mark_node)
	    id = NULL_TREE;
	}
      /* If we could not find a corresponding TYPE, treat this
	 declaration like an unqualified declaration.  */
      if (type == error_mark_node)
	nested_name_specifier = NULL_TREE;
      /* Otherwise, count the number of templates used in TYPE and its
	 containing scopes.  */
      else 
	{
	  tree scope;

	  for (scope = TREE_TYPE (type); 
	       scope && TREE_CODE (scope) != NAMESPACE_DECL;
	       scope = (TYPE_P (scope) 
			? TYPE_CONTEXT (scope)
			: DECL_CONTEXT (scope))) 
	    if (TYPE_P (scope) 
		&& CLASS_TYPE_P (scope)
		&& CLASSTYPE_TEMPLATE_INFO (scope)
		&& PRIMARY_TEMPLATE_P (CLASSTYPE_TI_TEMPLATE (scope))
		&& !CLASSTYPE_TEMPLATE_SPECIALIZATION (scope))
	      ++num_templates;
	}
    }
  /* Otherwise, the identifier is optional.  */
  else
    {
      /* We don't know whether what comes next is a template-id,
	 an identifier, or nothing at all.  */
      cp_parser_parse_tentatively (parser);
      /* Check for a template-id.  */
      id = cp_parser_template_id (parser, 
				  /*template_keyword_p=*/false,
				  /*check_dependency_p=*/true,
				  /*is_declaration=*/true);
      /* If that didn't work, it could still be an identifier.  */
      if (!cp_parser_parse_definitely (parser))
	{
	  if (cp_lexer_next_token_is (parser->lexer, CPP_NAME))
	    id = cp_parser_identifier (parser);
	  else
	    id = NULL_TREE;
	}
      else
	{
	  template_id_p = true;
	  ++num_templates;
	}
    }

  pop_deferring_access_checks ();

  if (id)
    cp_parser_check_for_invalid_template_id (parser, id);

  /* If it's not a `:' or a `{' then we can't really be looking at a
     class-head, since a class-head only appears as part of a
     class-specifier.  We have to detect this situation before calling
     xref_tag, since that has irreversible side-effects.  */
  if (!cp_parser_next_token_starts_class_definition_p (parser))
    {
      cp_parser_error (parser, "expected `{' or `:'");
      return error_mark_node;
    }

  /* At this point, we're going ahead with the class-specifier, even
     if some other problem occurs.  */
  cp_parser_commit_to_tentative_parse (parser);
  /* Issue the error about the overly-qualified name now.  */
  if (qualified_p)
    cp_parser_error (parser,
		     "global qualification of class name is invalid");
  else if (invalid_nested_name_p)
    cp_parser_error (parser,
		     "qualified name does not name a class");
  else if (nested_name_specifier)
    {
      tree scope;

      /* Reject typedef-names in class heads.  */
      if (!DECL_IMPLICIT_TYPEDEF_P (type))
	{
	  error ("invalid class name in declaration of `%D'", type);
	  type = NULL_TREE;
	  goto done;
	}

      /* Figure out in what scope the declaration is being placed.  */
      scope = current_scope ();
      if (!scope)
	scope = current_namespace;
      /* If that scope does not contain the scope in which the
	 class was originally declared, the program is invalid.  */
      if (scope && !is_ancestor (scope, nested_name_specifier))
	{
	  error ("declaration of `%D' in `%D' which does not "
		 "enclose `%D'", type, scope, nested_name_specifier);
	  type = NULL_TREE;
	  goto done;
	}
      /* [dcl.meaning]

         A declarator-id shall not be qualified exception of the
	 definition of a ... nested class outside of its class
	 ... [or] a the definition or explicit instantiation of a
	 class member of a namespace outside of its namespace.  */
      if (scope == nested_name_specifier)
	{
	  pedwarn ("extra qualification ignored");
	  nested_name_specifier = NULL_TREE;
	  num_templates = 0;
	}
    }
  /* An explicit-specialization must be preceded by "template <>".  If
     it is not, try to recover gracefully.  */
  if (at_namespace_scope_p () 
      && parser->num_template_parameter_lists == 0
      && template_id_p)
    {
      error ("an explicit specialization must be preceded by 'template <>'");
      invalid_explicit_specialization_p = true;
      /* Take the same action that would have been taken by
	 cp_parser_explicit_specialization.  */
      ++parser->num_template_parameter_lists;
      begin_specialization ();
    }
  /* There must be no "return" statements between this point and the
     end of this function; set "type "to the correct return value and
     use "goto done;" to return.  */
  /* Make sure that the right number of template parameters were
     present.  */
  if (!cp_parser_check_template_parameters (parser, num_templates))
    {
      /* If something went wrong, there is no point in even trying to
	 process the class-definition.  */
      type = NULL_TREE;
      goto done;
    }

  /* Look up the type.  */
  if (template_id_p)
    {
      type = TREE_TYPE (id);
      maybe_process_partial_specialization (type);
    }
  else if (!nested_name_specifier)
    {
      /* If the class was unnamed, create a dummy name.  */
      if (!id)
	id = make_anon_name ();
      type = xref_tag (class_key, id, /*globalize=*/false,
		       parser->num_template_parameter_lists);
    }
  else
    {
      tree class_type;
      bool pop_p = false;

      /* Given:

	    template <typename T> struct S { struct T };
	    template <typename T> struct S<T>::T { };

	 we will get a TYPENAME_TYPE when processing the definition of
	 `S::T'.  We need to resolve it to the actual type before we
	 try to define it.  */
      if (TREE_CODE (TREE_TYPE (type)) == TYPENAME_TYPE)
	{
	  class_type = resolve_typename_type (TREE_TYPE (type),
					      /*only_current_p=*/false);
	  if (class_type != error_mark_node)
	    type = TYPE_NAME (class_type);
	  else
	    {
	      cp_parser_error (parser, "could not resolve typename type");
	      type = error_mark_node;
	    }
	}

      maybe_process_partial_specialization (TREE_TYPE (type));
      class_type = current_class_type;
      /* Enter the scope indicated by the nested-name-specifier.  */
      if (nested_name_specifier)
	pop_p = push_scope (nested_name_specifier);
      /* Get the canonical version of this type.  */
      type = TYPE_MAIN_DECL (TREE_TYPE (type));
      if (PROCESSING_REAL_TEMPLATE_DECL_P ()
	  && !CLASSTYPE_TEMPLATE_SPECIALIZATION (TREE_TYPE (type)))
	type = push_template_decl (type);
      type = TREE_TYPE (type);
      if (nested_name_specifier)
	{
	  *nested_name_specifier_p = true;
	  if (pop_p)
	    pop_scope (nested_name_specifier);
	}
    }
  /* Indicate whether this class was declared as a `class' or as a
     `struct'.  */
  if (TREE_CODE (type) == RECORD_TYPE)
    CLASSTYPE_DECLARED_CLASS (type) = (class_key == class_type);
  cp_parser_check_class_key (class_key, type);

  /* Enter the scope containing the class; the names of base classes
     should be looked up in that context.  For example, given:

       struct A { struct B {}; struct C; };
       struct A::C : B {};

     is valid.  */
  if (nested_name_specifier)
    pop_p = push_scope (nested_name_specifier);
  /* Now, look for the base-clause.  */
  token = cp_lexer_peek_token (parser->lexer);
  if (token->type == CPP_COLON)
    {
      tree bases;

      /* Get the list of base-classes.  */
      bases = cp_parser_base_clause (parser);
      /* Process them.  */
      xref_basetypes (type, bases);
    }
  /* Leave the scope given by the nested-name-specifier.  We will
     enter the class scope itself while processing the members.  */
  if (pop_p)
    pop_scope (nested_name_specifier);

 done:
  if (invalid_explicit_specialization_p)
    {
      end_specialization ();
      --parser->num_template_parameter_lists;
    }
  *attributes_p = attributes;
  return type;
}

/* Parse a class-key.

   class-key:
     class
     struct
     union

   Returns the kind of class-key specified, or none_type to indicate
   error.  */

static enum tag_types
cp_parser_class_key (cp_parser* parser)
{
  cp_token *token;
  enum tag_types tag_type;

  /* Look for the class-key.  */
  token = cp_parser_require (parser, CPP_KEYWORD, "class-key");
  if (!token)
    return none_type;

  /* Check to see if the TOKEN is a class-key.  */
  tag_type = cp_parser_token_is_class_key (token);
  if (!tag_type)
    cp_parser_error (parser, "expected class-key");
  return tag_type;
}

/* Parse an (optional) member-specification.

   member-specification:
     member-declaration member-specification [opt]
     access-specifier : member-specification [opt]  */

static void
cp_parser_member_specification_opt (cp_parser* parser)
{
  while (true)
    {
      cp_token *token;
      enum rid keyword;

      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);
      /* If it's a `}', or EOF then we've seen all the members.  */
      if (token->type == CPP_CLOSE_BRACE || token->type == CPP_EOF)
	break;

      /* See if this token is a keyword.  */
      keyword = token->keyword;
      switch (keyword)
	{
	case RID_PUBLIC:
	case RID_PROTECTED:
	case RID_PRIVATE:
	  /* Consume the access-specifier.  */
	  cp_lexer_consume_token (parser->lexer);
	  /* Remember which access-specifier is active.  */
	  current_access_specifier = token->value;
	  /* Look for the `:'.  */
	  cp_parser_require (parser, CPP_COLON, "`:'");
	  break;

	default:
	  /* Otherwise, the next construction must be a
	     member-declaration.  */
	  cp_parser_member_declaration (parser);
	}
    }
}

/* Parse a member-declaration.  

   member-declaration:
     decl-specifier-seq [opt] member-declarator-list [opt] ;
     function-definition ; [opt]
     :: [opt] nested-name-specifier template [opt] unqualified-id ;
     using-declaration
     template-declaration 

   member-declarator-list:
     member-declarator
     member-declarator-list , member-declarator

   member-declarator:
     declarator pure-specifier [opt] 
     declarator constant-initializer [opt]
     identifier [opt] : constant-expression 

   GNU Extensions:

   member-declaration:
     __extension__ member-declaration

   member-declarator:
     declarator attributes [opt] pure-specifier [opt]
     declarator attributes [opt] constant-initializer [opt]
     identifier [opt] attributes [opt] : constant-expression  */

static void
cp_parser_member_declaration (cp_parser* parser)
{
  tree decl_specifiers;
  tree prefix_attributes;
  tree decl;
  int declares_class_or_enum;
  bool friend_p;
  cp_token *token;
  int saved_pedantic;

  /* Check for the `__extension__' keyword.  */
  if (cp_parser_extension_opt (parser, &saved_pedantic))
    {
      /* Recurse.  */
      cp_parser_member_declaration (parser);
      /* Restore the old value of the PEDANTIC flag.  */
      pedantic = saved_pedantic;

      return;
    }

  /* Check for a template-declaration.  */
  if (cp_lexer_next_token_is_keyword (parser->lexer, RID_TEMPLATE))
    {
      /* An explicit specialization here is an error condition, and we
	 expect the specialization handler to detect and report this.  */
      if (cp_lexer_peek_nth_token (parser->lexer, 2)->type == CPP_LESS
	  && cp_lexer_peek_nth_token (parser->lexer, 3)->type == CPP_GREATER)
	cp_parser_explicit_specialization (parser);
      else
	cp_parser_template_declaration (parser, /*member_p=*/true);

      return;
    }

  /* Check for a using-declaration.  */
  if (cp_lexer_next_token_is_keyword (parser->lexer, RID_USING))
    {
      /* Parse the using-declaration.  */
      cp_parser_using_declaration (parser);

      return;
    }
  
  /* Parse the decl-specifier-seq.  */
  decl_specifiers 
    = cp_parser_decl_specifier_seq (parser,
				    CP_PARSER_FLAGS_OPTIONAL,
				    &prefix_attributes,
				    &declares_class_or_enum);
  /* Check for an invalid type-name.  */
  if (cp_parser_diagnose_invalid_type_name (parser))
    return;
  /* If there is no declarator, then the decl-specifier-seq should
     specify a type.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_SEMICOLON))
    {
      /* If there was no decl-specifier-seq, and the next token is a
	 `;', then we have something like:

	   struct S { ; };

	 [class.mem]

	 Each member-declaration shall declare at least one member
	 name of the class.  */
      if (!decl_specifiers)
	{
	  if (pedantic)
	    pedwarn ("extra semicolon");
	}
      else 
	{
	  tree type;
	  
	  /* See if this declaration is a friend.  */
	  friend_p = cp_parser_friend_p (decl_specifiers);
	  /* If there were decl-specifiers, check to see if there was
	     a class-declaration.  */
	  type = check_tag_decl (decl_specifiers);
	  /* Nested classes have already been added to the class, but
	     a `friend' needs to be explicitly registered.  */
	  if (friend_p)
	    {
	      /* If the `friend' keyword was present, the friend must
		 be introduced with a class-key.  */
	       if (!declares_class_or_enum)
		 error ("a class-key must be used when declaring a friend");
	       /* In this case:

		    template <typename T> struct A { 
                      friend struct A<T>::B; 
                    };
 
		  A<T>::B will be represented by a TYPENAME_TYPE, and
		  therefore not recognized by check_tag_decl.  */
	       if (!type)
		 {
		   tree specifier;

		   for (specifier = decl_specifiers; 
			specifier;
			specifier = TREE_CHAIN (specifier))
		     {
		       tree s = TREE_VALUE (specifier);

		       if (TREE_CODE (s) == IDENTIFIER_NODE)
                         get_global_value_if_present (s, &type);
		       if (TREE_CODE (s) == TYPE_DECL)
			 s = TREE_TYPE (s);
		       if (TYPE_P (s))
			 {
			   type = s;
			   break;
			 }
		     }
		 }
	       if (!type || !TYPE_P (type))
		 error ("friend declaration does not name a class or "
			"function");
	       else
		 make_friend_class (current_class_type, type,
				    /*complain=*/true);
	    }
	  /* If there is no TYPE, an error message will already have
	     been issued.  */
	  else if (!type)
	    ;
	  /* An anonymous aggregate has to be handled specially; such
	     a declaration really declares a data member (with a
	     particular type), as opposed to a nested class.  */
	  else if (ANON_AGGR_TYPE_P (type))
	    {
	      /* Remove constructors and such from TYPE, now that we
		 know it is an anonymous aggregate.  */
	      fixup_anonymous_aggr (type);
	      /* And make the corresponding data member.  */
	      decl = build_decl (FIELD_DECL, NULL_TREE, type);
	      /* Add it to the class.  */
	      finish_member_declaration (decl);
	    }
	  else
	    cp_parser_check_access_in_redeclaration (TYPE_NAME (type));
	}
    }
  else
    {
      /* See if these declarations will be friends.  */
      friend_p = cp_parser_friend_p (decl_specifiers);

      /* Keep going until we hit the `;' at the end of the 
	 declaration.  */
      while (cp_lexer_next_token_is_not (parser->lexer, CPP_SEMICOLON))
	{
	  tree attributes = NULL_TREE;
	  tree first_attribute;

	  /* Peek at the next token.  */
	  token = cp_lexer_peek_token (parser->lexer);

	  /* Check for a bitfield declaration.  */
	  if (token->type == CPP_COLON
	      || (token->type == CPP_NAME
		  && cp_lexer_peek_nth_token (parser->lexer, 2)->type 
		  == CPP_COLON))
	    {
	      tree identifier;
	      tree width;

	      /* Get the name of the bitfield.  Note that we cannot just
		 check TOKEN here because it may have been invalidated by
		 the call to cp_lexer_peek_nth_token above.  */
	      if (cp_lexer_peek_token (parser->lexer)->type != CPP_COLON)
		identifier = cp_parser_identifier (parser);
	      else
		identifier = NULL_TREE;

	      /* Consume the `:' token.  */
	      cp_lexer_consume_token (parser->lexer);
	      /* Get the width of the bitfield.  */
	      width 
		= cp_parser_constant_expression (parser,
						 /*allow_non_constant=*/false,
						 NULL);

	      /* Look for attributes that apply to the bitfield.  */
	      attributes = cp_parser_attributes_opt (parser);
	      /* Remember which attributes are prefix attributes and
		 which are not.  */
	      first_attribute = attributes;
	      /* Combine the attributes.  */
	      attributes = chainon (prefix_attributes, attributes);

	      /* Create the bitfield declaration.  */
	      decl = grokbitfield (identifier, 
				   decl_specifiers,
				   width);
	      /* Apply the attributes.  */
	      cplus_decl_attributes (&decl, attributes, /*flags=*/0);
	    }
	  else
	    {
	      tree declarator;
	      tree initializer;
	      tree asm_specification;
	      int ctor_dtor_or_conv_p;

	      /* Parse the declarator.  */
	      declarator 
		= cp_parser_declarator (parser, CP_PARSER_DECLARATOR_NAMED,
					&ctor_dtor_or_conv_p,
					/*parenthesized_p=*/NULL,
					/*member_p=*/true);

	      /* If something went wrong parsing the declarator, make sure
		 that we at least consume some tokens.  */
	      if (declarator == error_mark_node)
		{
		  /* Skip to the end of the statement.  */
		  cp_parser_skip_to_end_of_statement (parser);
		  /* If the next token is not a semicolon, that is
		     probably because we just skipped over the body of
		     a function.  So, we consume a semicolon if
		     present, but do not issue an error message if it
		     is not present.  */
		  if (cp_lexer_next_token_is (parser->lexer,
					      CPP_SEMICOLON))
		    cp_lexer_consume_token (parser->lexer);
		  return;
		}

	      if (declares_class_or_enum & 2)
		cp_parser_check_for_definition_in_return_type
		  (declarator, TREE_VALUE (decl_specifiers));

	      /* Look for an asm-specification.  */
	      asm_specification = cp_parser_asm_specification_opt (parser);
	      /* Look for attributes that apply to the declaration.  */
	      attributes = cp_parser_attributes_opt (parser);
	      /* Remember which attributes are prefix attributes and
		 which are not.  */
	      first_attribute = attributes;
	      /* Combine the attributes.  */
	      attributes = chainon (prefix_attributes, attributes);

	      /* If it's an `=', then we have a constant-initializer or a
		 pure-specifier.  It is not correct to parse the
		 initializer before registering the member declaration
		 since the member declaration should be in scope while
		 its initializer is processed.  However, the rest of the
		 front end does not yet provide an interface that allows
		 us to handle this correctly.  */
	      if (cp_lexer_next_token_is (parser->lexer, CPP_EQ))
		{
		  /* In [class.mem]:

		     A pure-specifier shall be used only in the declaration of
		     a virtual function.  

		     A member-declarator can contain a constant-initializer
		     only if it declares a static member of integral or
		     enumeration type.  

		     Therefore, if the DECLARATOR is for a function, we look
		     for a pure-specifier; otherwise, we look for a
		     constant-initializer.  When we call `grokfield', it will
		     perform more stringent semantics checks.  */
		  if (TREE_CODE (declarator) == CALL_EXPR)
		    initializer = cp_parser_pure_specifier (parser);
		  else
		    /* Parse the initializer.  */
		    initializer = cp_parser_constant_initializer (parser);
		}
	      /* Otherwise, there is no initializer.  */
	      else
		initializer = NULL_TREE;

	      /* See if we are probably looking at a function
		 definition.  We are certainly not looking at at a
		 member-declarator.  Calling `grokfield' has
		 side-effects, so we must not do it unless we are sure
		 that we are looking at a member-declarator.  */
	      if (cp_parser_token_starts_function_definition_p 
		  (cp_lexer_peek_token (parser->lexer)))
		{
		  /* The grammar does not allow a pure-specifier to be
		     used when a member function is defined.  (It is
		     possible that this fact is an oversight in the
		     standard, since a pure function may be defined
		     outside of the class-specifier.  */
		  if (initializer)
		    error ("pure-specifier on function-definition");
		  decl = cp_parser_save_member_function_body (parser,
							      decl_specifiers,
							      declarator,
							      attributes);
		  /* If the member was not a friend, declare it here.  */
		  if (!friend_p)
		    finish_member_declaration (decl);
		  /* Peek at the next token.  */
		  token = cp_lexer_peek_token (parser->lexer);
		  /* If the next token is a semicolon, consume it.  */
		  if (token->type == CPP_SEMICOLON)
		    cp_lexer_consume_token (parser->lexer);
		  return;
		}
	      else
		{
		  /* Create the declaration.  */
		  decl = grokfield (declarator, decl_specifiers, 
				    initializer, asm_specification,
				    attributes);
		  /* Any initialization must have been from a
		     constant-expression.  */
		  if (decl && TREE_CODE (decl) == VAR_DECL && initializer)
		    DECL_INITIALIZED_BY_CONSTANT_EXPRESSION_P (decl) = 1;
		}
	    }

	  /* Reset PREFIX_ATTRIBUTES.  */
	  while (attributes && TREE_CHAIN (attributes) != first_attribute)
	    attributes = TREE_CHAIN (attributes);
	  if (attributes)
	    TREE_CHAIN (attributes) = NULL_TREE;

	  /* If there is any qualification still in effect, clear it
	     now; we will be starting fresh with the next declarator.  */
	  parser->scope = NULL_TREE;
	  parser->qualifying_scope = NULL_TREE;
	  parser->object_scope = NULL_TREE;
	  /* If it's a `,', then there are more declarators.  */
	  if (cp_lexer_next_token_is (parser->lexer, CPP_COMMA))
	    cp_lexer_consume_token (parser->lexer);
	  /* If the next token isn't a `;', then we have a parse error.  */
	  else if (cp_lexer_next_token_is_not (parser->lexer,
					       CPP_SEMICOLON))
	    {
	      cp_parser_error (parser, "expected `;'");
	      /* Skip tokens until we find a `;'.  */
	      cp_parser_skip_to_end_of_statement (parser);

	      break;
	    }

	  if (decl)
	    {
	      /* Add DECL to the list of members.  */
	      if (!friend_p)
		finish_member_declaration (decl);

	      if (TREE_CODE (decl) == FUNCTION_DECL)
		cp_parser_save_default_args (parser, decl);
	    }
	}
    }

  cp_parser_require (parser, CPP_SEMICOLON, "`;'");
}

/* Parse a pure-specifier.

   pure-specifier:
     = 0

   Returns INTEGER_ZERO_NODE if a pure specifier is found.
   Otherwise, ERROR_MARK_NODE is returned.  */

static tree
cp_parser_pure_specifier (cp_parser* parser)
{
  cp_token *token;

  /* Look for the `=' token.  */
  if (!cp_parser_require (parser, CPP_EQ, "`='"))
    return error_mark_node;
  /* Look for the `0' token.  */
  token = cp_parser_require (parser, CPP_NUMBER, "`0'");
  /* Unfortunately, this will accept `0L' and `0x00' as well.  We need
     to get information from the lexer about how the number was
     spelled in order to fix this problem.  */
  if (!token || !integer_zerop (token->value))
    return error_mark_node;

  return integer_zero_node;
}

/* Parse a constant-initializer.

   constant-initializer:
     = constant-expression

   Returns a representation of the constant-expression.  */

static tree
cp_parser_constant_initializer (cp_parser* parser)
{
  /* Look for the `=' token.  */
  if (!cp_parser_require (parser, CPP_EQ, "`='"))
    return error_mark_node;

  /* It is invalid to write:

       struct S { static const int i = { 7 }; };

     */
  if (cp_lexer_next_token_is (parser->lexer, CPP_OPEN_BRACE))
    {
      cp_parser_error (parser,
		       "a brace-enclosed initializer is not allowed here");
      /* Consume the opening brace.  */
      cp_lexer_consume_token (parser->lexer);
      /* Skip the initializer.  */
      cp_parser_skip_to_closing_brace (parser);
      /* Look for the trailing `}'.  */
      cp_parser_require (parser, CPP_CLOSE_BRACE, "`}'");
      
      return error_mark_node;
    }

  return cp_parser_constant_expression (parser, 
					/*allow_non_constant=*/false,
					NULL);
}

/* Derived classes [gram.class.derived] */

/* Parse a base-clause.

   base-clause:
     : base-specifier-list  

   base-specifier-list:
     base-specifier
     base-specifier-list , base-specifier

   Returns a TREE_LIST representing the base-classes, in the order in
   which they were declared.  The representation of each node is as
   described by cp_parser_base_specifier.  

   In the case that no bases are specified, this function will return
   NULL_TREE, not ERROR_MARK_NODE.  */

static tree
cp_parser_base_clause (cp_parser* parser)
{
  tree bases = NULL_TREE;

  /* Look for the `:' that begins the list.  */
  cp_parser_require (parser, CPP_COLON, "`:'");

  /* Scan the base-specifier-list.  */
  while (true)
    {
      cp_token *token;
      tree base;

      /* Look for the base-specifier.  */
      base = cp_parser_base_specifier (parser);
      /* Add BASE to the front of the list.  */
      if (base != error_mark_node)
	{
	  TREE_CHAIN (base) = bases;
	  bases = base;
	}
      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);
      /* If it's not a comma, then the list is complete.  */
      if (token->type != CPP_COMMA)
	break;
      /* Consume the `,'.  */
      cp_lexer_consume_token (parser->lexer);
    }

  /* PARSER->SCOPE may still be non-NULL at this point, if the last
     base class had a qualified name.  However, the next name that
     appears is certainly not qualified.  */
  parser->scope = NULL_TREE;
  parser->qualifying_scope = NULL_TREE;
  parser->object_scope = NULL_TREE;

  return nreverse (bases);
}

/* Parse a base-specifier.

   base-specifier:
     :: [opt] nested-name-specifier [opt] class-name
     virtual access-specifier [opt] :: [opt] nested-name-specifier
       [opt] class-name
     access-specifier virtual [opt] :: [opt] nested-name-specifier
       [opt] class-name

   Returns a TREE_LIST.  The TREE_PURPOSE will be one of
   ACCESS_{DEFAULT,PUBLIC,PROTECTED,PRIVATE}_[VIRTUAL]_NODE to
   indicate the specifiers provided.  The TREE_VALUE will be a TYPE
   (or the ERROR_MARK_NODE) indicating the type that was specified.  */
       
static tree
cp_parser_base_specifier (cp_parser* parser)
{
  cp_token *token;
  bool done = false;
  bool virtual_p = false;
  bool duplicate_virtual_error_issued_p = false;
  bool duplicate_access_error_issued_p = false;
  bool class_scope_p, template_p;
  tree access = access_default_node;
  tree type;

  /* Process the optional `virtual' and `access-specifier'.  */
  while (!done)
    {
      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);
      /* Process `virtual'.  */
      switch (token->keyword)
	{
	case RID_VIRTUAL:
	  /* If `virtual' appears more than once, issue an error.  */
	  if (virtual_p && !duplicate_virtual_error_issued_p)
	    {
	      cp_parser_error (parser,
			       "`virtual' specified more than once in base-specified");
	      duplicate_virtual_error_issued_p = true;
	    }

	  virtual_p = true;

	  /* Consume the `virtual' token.  */
	  cp_lexer_consume_token (parser->lexer);

	  break;

	case RID_PUBLIC:
	case RID_PROTECTED:
	case RID_PRIVATE:
	  /* If more than one access specifier appears, issue an
	     error.  */
	  if (access != access_default_node
	      && !duplicate_access_error_issued_p)
	    {
	      cp_parser_error (parser,
			       "more than one access specifier in base-specified");
	      duplicate_access_error_issued_p = true;
	    }

	  access = ridpointers[(int) token->keyword];

	  /* Consume the access-specifier.  */
	  cp_lexer_consume_token (parser->lexer);

	  break;

	default:
	  done = true;
	  break;
	}
    }
  /* It is not uncommon to see programs mechanically, errouneously, use
     the 'typename' keyword to denote (dependent) qualified types
     as base classes.  */
  if (cp_lexer_next_token_is_keyword (parser->lexer, RID_TYPENAME))
    {
      if (!processing_template_decl)
	error ("keyword `typename' not allowed outside of templates");
      else
	error ("keyword `typename' not allowed in this context "
	       "(the base class is implicitly a type)");
      cp_lexer_consume_token (parser->lexer);
    }

  /* Look for the optional `::' operator.  */
  cp_parser_global_scope_opt (parser, /*current_scope_valid_p=*/false);
  /* Look for the nested-name-specifier.  The simplest way to
     implement:

       [temp.res]

       The keyword `typename' is not permitted in a base-specifier or
       mem-initializer; in these contexts a qualified name that
       depends on a template-parameter is implicitly assumed to be a
       type name.

     is to pretend that we have seen the `typename' keyword at this
     point.  */ 
  cp_parser_nested_name_specifier_opt (parser,
				       /*typename_keyword_p=*/true,
				       /*check_dependency_p=*/true,
				       /*type_p=*/true,
				       /*is_declaration=*/true);
  /* If the base class is given by a qualified name, assume that names
     we see are type names or templates, as appropriate.  */
  class_scope_p = (parser->scope && TYPE_P (parser->scope));
  template_p = class_scope_p && cp_parser_optional_template_keyword (parser);
  
  /* Finally, look for the class-name.  */
  type = cp_parser_class_name (parser, 
			       class_scope_p,
			       template_p,
			       /*type_p=*/true,
			       /*check_dependency_p=*/true,
			       /*class_head_p=*/false,
			       /*is_declaration=*/true);

  if (type == error_mark_node)
    return error_mark_node;

  return finish_base_specifier (TREE_TYPE (type), access, virtual_p);
}

/* Exception handling [gram.exception] */

/* Parse an (optional) exception-specification.

   exception-specification:
     throw ( type-id-list [opt] )

   Returns a TREE_LIST representing the exception-specification.  The
   TREE_VALUE of each node is a type.  */

static tree
cp_parser_exception_specification_opt (cp_parser* parser)
{
  cp_token *token;
  tree type_id_list;

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* If it's not `throw', then there's no exception-specification.  */
  if (!cp_parser_is_keyword (token, RID_THROW))
    return NULL_TREE;

  /* Consume the `throw'.  */
  cp_lexer_consume_token (parser->lexer);

  /* Look for the `('.  */
  cp_parser_require (parser, CPP_OPEN_PAREN, "`('");

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* If it's not a `)', then there is a type-id-list.  */
  if (token->type != CPP_CLOSE_PAREN)
    {
      const char *saved_message;

      /* Types may not be defined in an exception-specification.  */
      saved_message = parser->type_definition_forbidden_message;
      parser->type_definition_forbidden_message
	= "types may not be defined in an exception-specification";
      /* Parse the type-id-list.  */
      type_id_list = cp_parser_type_id_list (parser);
      /* Restore the saved message.  */
      parser->type_definition_forbidden_message = saved_message;
    }
  else
    type_id_list = empty_except_spec;

  /* Look for the `)'.  */
  cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");

  return type_id_list;
}

/* Parse an (optional) type-id-list.

   type-id-list:
     type-id
     type-id-list , type-id

   Returns a TREE_LIST.  The TREE_VALUE of each node is a TYPE,
   in the order that the types were presented.  */

static tree
cp_parser_type_id_list (cp_parser* parser)
{
  tree types = NULL_TREE;

  while (true)
    {
      cp_token *token;
      tree type;

      /* Get the next type-id.  */
      type = cp_parser_type_id (parser);
      /* Add it to the list.  */
      types = add_exception_specifier (types, type, /*complain=*/1);
      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);
      /* If it is not a `,', we are done.  */
      if (token->type != CPP_COMMA)
	break;
      /* Consume the `,'.  */
      cp_lexer_consume_token (parser->lexer);
    }

  return nreverse (types);
}

/* Parse a try-block.

   try-block:
     try compound-statement handler-seq  */

static tree
cp_parser_try_block (cp_parser* parser)
{
  tree try_block;

  cp_parser_require_keyword (parser, RID_TRY, "`try'");
  try_block = begin_try_block ();
  cp_parser_compound_statement (parser, false);
  finish_try_block (try_block);
  cp_parser_handler_seq (parser);
  finish_handler_sequence (try_block);

  return try_block;
}

/* Parse a function-try-block.

   function-try-block:
     try ctor-initializer [opt] function-body handler-seq  */

static bool
cp_parser_function_try_block (cp_parser* parser)
{
  tree try_block;
  bool ctor_initializer_p;

  /* Look for the `try' keyword.  */
  if (!cp_parser_require_keyword (parser, RID_TRY, "`try'"))
    return false;
  /* Let the rest of the front-end know where we are.  */
  try_block = begin_function_try_block ();
  /* Parse the function-body.  */
  ctor_initializer_p 
    = cp_parser_ctor_initializer_opt_and_function_body (parser);
  /* We're done with the `try' part.  */
  finish_function_try_block (try_block);
  /* Parse the handlers.  */
  cp_parser_handler_seq (parser);
  /* We're done with the handlers.  */
  finish_function_handler_sequence (try_block);

  return ctor_initializer_p;
}

/* Parse a handler-seq.

   handler-seq:
     handler handler-seq [opt]  */

static void
cp_parser_handler_seq (cp_parser* parser)
{
  while (true)
    {
      cp_token *token;

      /* Parse the handler.  */
      cp_parser_handler (parser);
      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);
      /* If it's not `catch' then there are no more handlers.  */
      if (!cp_parser_is_keyword (token, RID_CATCH))
	break;
    }
}

/* Parse a handler.

   handler:
     catch ( exception-declaration ) compound-statement  */

static void
cp_parser_handler (cp_parser* parser)
{
  tree handler;
  tree declaration;

  cp_parser_require_keyword (parser, RID_CATCH, "`catch'");
  handler = begin_handler ();
  cp_parser_require (parser, CPP_OPEN_PAREN, "`('");
  declaration = cp_parser_exception_declaration (parser);
  finish_handler_parms (declaration, handler);
  cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");
  cp_parser_compound_statement (parser, false);
  finish_handler (handler);
}

/* Parse an exception-declaration.

   exception-declaration:
     type-specifier-seq declarator
     type-specifier-seq abstract-declarator
     type-specifier-seq
     ...  

   Returns a VAR_DECL for the declaration, or NULL_TREE if the
   ellipsis variant is used.  */

static tree
cp_parser_exception_declaration (cp_parser* parser)
{
  tree type_specifiers;
  tree declarator;
  const char *saved_message;

  /* If it's an ellipsis, it's easy to handle.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_ELLIPSIS))
    {
      /* Consume the `...' token.  */
      cp_lexer_consume_token (parser->lexer);
      return NULL_TREE;
    }

  /* Types may not be defined in exception-declarations.  */
  saved_message = parser->type_definition_forbidden_message;
  parser->type_definition_forbidden_message
    = "types may not be defined in exception-declarations";

  /* Parse the type-specifier-seq.  */
  type_specifiers = cp_parser_type_specifier_seq (parser);
  /* If it's a `)', then there is no declarator.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_CLOSE_PAREN))
    declarator = NULL_TREE;
  else
    declarator = cp_parser_declarator (parser, CP_PARSER_DECLARATOR_EITHER,
				       /*ctor_dtor_or_conv_p=*/NULL,
				       /*parenthesized_p=*/NULL,
				       /*member_p=*/false);

  /* Restore the saved message.  */
  parser->type_definition_forbidden_message = saved_message;

  return start_handler_parms (type_specifiers, declarator);
}

/* Parse a throw-expression. 

   throw-expression:
     throw assignment-expression [opt]

   Returns a THROW_EXPR representing the throw-expression.  */

static tree
cp_parser_throw_expression (cp_parser* parser)
{
  tree expression;
  cp_token* token;

  cp_parser_require_keyword (parser, RID_THROW, "`throw'");
  token = cp_lexer_peek_token (parser->lexer);
  /* Figure out whether or not there is an assignment-expression
     following the "throw" keyword.  */
  if (token->type == CPP_COMMA
      || token->type == CPP_SEMICOLON
      || token->type == CPP_CLOSE_PAREN
      || token->type == CPP_CLOSE_SQUARE
      || token->type == CPP_CLOSE_BRACE
      || token->type == CPP_COLON)
    expression = NULL_TREE;
  else
    expression = cp_parser_assignment_expression (parser);

  return build_throw (expression);
}

/* GNU Extensions */

/* Parse an (optional) asm-specification.

   asm-specification:
     asm ( string-literal )

   If the asm-specification is present, returns a STRING_CST
   corresponding to the string-literal.  Otherwise, returns
   NULL_TREE.  */

static tree
cp_parser_asm_specification_opt (cp_parser* parser)
{
  cp_token *token;
  tree asm_specification;

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* If the next token isn't the `asm' keyword, then there's no 
     asm-specification.  */
  if (!cp_parser_is_keyword (token, RID_ASM))
    return NULL_TREE;

  /* Consume the `asm' token.  */
  cp_lexer_consume_token (parser->lexer);
  /* Look for the `('.  */
  cp_parser_require (parser, CPP_OPEN_PAREN, "`('");

  /* Look for the string-literal.  */
  token = cp_parser_require (parser, CPP_STRING, "string-literal");
  if (token)
    asm_specification = token->value;
  else
    asm_specification = NULL_TREE;

  /* Look for the `)'.  */
  cp_parser_require (parser, CPP_CLOSE_PAREN, "`('");

  return asm_specification;
}

/* Parse an asm-operand-list.  

   asm-operand-list:
     asm-operand
     asm-operand-list , asm-operand
     
   asm-operand:
     string-literal ( expression )  
     [ string-literal ] string-literal ( expression )

   Returns a TREE_LIST representing the operands.  The TREE_VALUE of
   each node is the expression.  The TREE_PURPOSE is itself a
   TREE_LIST whose TREE_PURPOSE is a STRING_CST for the bracketed
   string-literal (or NULL_TREE if not present) and whose TREE_VALUE
   is a STRING_CST for the string literal before the parenthesis.  */

static tree
cp_parser_asm_operand_list (cp_parser* parser)
{
  tree asm_operands = NULL_TREE;

  while (true)
    {
      tree string_literal;
      tree expression;
      tree name;
      cp_token *token;
      
      if (cp_lexer_next_token_is (parser->lexer, CPP_OPEN_SQUARE)) 
	{
	  /* Consume the `[' token.  */
	  cp_lexer_consume_token (parser->lexer);
	  /* Read the operand name.  */
	  name = cp_parser_identifier (parser);
	  if (name != error_mark_node) 
	    name = build_string (IDENTIFIER_LENGTH (name),
				 IDENTIFIER_POINTER (name));
	  /* Look for the closing `]'.  */
	  cp_parser_require (parser, CPP_CLOSE_SQUARE, "`]'");
	}
      else
	name = NULL_TREE;
      /* Look for the string-literal.  */
      token = cp_parser_require (parser, CPP_STRING, "string-literal");
      string_literal = token ? token->value : error_mark_node;
      /* Look for the `('.  */
      cp_parser_require (parser, CPP_OPEN_PAREN, "`('");
      /* Parse the expression.  */
      expression = cp_parser_expression (parser);
      /* Look for the `)'.  */
      cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");
      /* Add this operand to the list.  */
      asm_operands = tree_cons (build_tree_list (name, string_literal),
				expression, 
				asm_operands);
      /* If the next token is not a `,', there are no more 
	 operands.  */
      if (cp_lexer_next_token_is_not (parser->lexer, CPP_COMMA))
	break;
      /* Consume the `,'.  */
      cp_lexer_consume_token (parser->lexer);
    }

  return nreverse (asm_operands);
}

/* Parse an asm-clobber-list.  

   asm-clobber-list:
     string-literal
     asm-clobber-list , string-literal  

   Returns a TREE_LIST, indicating the clobbers in the order that they
   appeared.  The TREE_VALUE of each node is a STRING_CST.  */

static tree
cp_parser_asm_clobber_list (cp_parser* parser)
{
  tree clobbers = NULL_TREE;

  while (true)
    {
      cp_token *token;
      tree string_literal;

      /* Look for the string literal.  */
      token = cp_parser_require (parser, CPP_STRING, "string-literal");
      string_literal = token ? token->value : error_mark_node;
      /* Add it to the list.  */
      clobbers = tree_cons (NULL_TREE, string_literal, clobbers);
      /* If the next token is not a `,', then the list is 
	 complete.  */
      if (cp_lexer_next_token_is_not (parser->lexer, CPP_COMMA))
	break;
      /* Consume the `,' token.  */
      cp_lexer_consume_token (parser->lexer);
    }

  return clobbers;
}

/* Parse an (optional) series of attributes.

   attributes:
     attributes attribute

   attribute:
     __attribute__ (( attribute-list [opt] ))  

   The return value is as for cp_parser_attribute_list.  */
     
static tree
cp_parser_attributes_opt (cp_parser* parser)
{
  tree attributes = NULL_TREE;

  while (true)
    {
      cp_token *token;
      tree attribute_list;

      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);
      /* If it's not `__attribute__', then we're done.  */
      if (token->keyword != RID_ATTRIBUTE)
	break;

      /* Consume the `__attribute__' keyword.  */
      cp_lexer_consume_token (parser->lexer);
      /* Look for the two `(' tokens.  */
      cp_parser_require (parser, CPP_OPEN_PAREN, "`('");
      cp_parser_require (parser, CPP_OPEN_PAREN, "`('");

      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);
      if (token->type != CPP_CLOSE_PAREN)
	/* Parse the attribute-list.  */
	attribute_list = cp_parser_attribute_list (parser);
      else
	/* If the next token is a `)', then there is no attribute
	   list.  */
	attribute_list = NULL;

      /* Look for the two `)' tokens.  */
      cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");
      cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");

      /* Add these new attributes to the list.  */
      attributes = chainon (attributes, attribute_list);
    }

  return attributes;
}

/* Parse an attribute-list.  

   attribute-list:  
     attribute 
     attribute-list , attribute

   attribute:
     identifier     
     identifier ( identifier )
     identifier ( identifier , expression-list )
     identifier ( expression-list ) 

   Returns a TREE_LIST.  Each node corresponds to an attribute.  THe
   TREE_PURPOSE of each node is the identifier indicating which
   attribute is in use.  The TREE_VALUE represents the arguments, if
   any.  */

static tree
cp_parser_attribute_list (cp_parser* parser)
{
  tree attribute_list = NULL_TREE;

  while (true)
    {
      cp_token *token;
      tree identifier;
      tree attribute;

      /* Look for the identifier.  We also allow keywords here; for
	 example `__attribute__ ((const))' is legal.  */
      token = cp_lexer_peek_token (parser->lexer);
      if (token->type == CPP_NAME 
	  || token->type == CPP_KEYWORD)
	{
	  /* Consume the token.  */
	  token = cp_lexer_consume_token (parser->lexer);

	  /* Save away the identifier that indicates which attribute 
	     this is.  */
	  identifier = token->value;
	  attribute = build_tree_list (identifier, NULL_TREE);

	  /* Peek at the next token.  */
	  token = cp_lexer_peek_token (parser->lexer);
	  /* If it's an `(', then parse the attribute arguments.  */
	  if (token->type == CPP_OPEN_PAREN)
	    {
	      tree arguments;

	      arguments = (cp_parser_parenthesized_expression_list 
			   (parser, true, /*non_constant_p=*/NULL));
	      /* Save the identifier and arguments away.  */
	      TREE_VALUE (attribute) = arguments;
	    }

	  /* Add this attribute to the list.  */
	  TREE_CHAIN (attribute) = attribute_list;
	  attribute_list = attribute;

	  /* Now, look for more attributes.  */
	  token = cp_lexer_peek_token (parser->lexer);
	}
      /* Now, look for more attributes.  If the next token isn't a
	 `,', we're done.  */
      if (token->type != CPP_COMMA)
	break;

      /* Consume the comma and keep going.  */
      cp_lexer_consume_token (parser->lexer);
    }

  /* We built up the list in reverse order.  */
  return nreverse (attribute_list);
}

/* Parse an optional `__extension__' keyword.  Returns TRUE if it is
   present, and FALSE otherwise.  *SAVED_PEDANTIC is set to the
   current value of the PEDANTIC flag, regardless of whether or not
   the `__extension__' keyword is present.  The caller is responsible
   for restoring the value of the PEDANTIC flag.  */

static bool
cp_parser_extension_opt (cp_parser* parser, int* saved_pedantic)
{
  /* Save the old value of the PEDANTIC flag.  */
  *saved_pedantic = pedantic;

  if (cp_lexer_next_token_is_keyword (parser->lexer, RID_EXTENSION))
    {
      /* Consume the `__extension__' token.  */
      cp_lexer_consume_token (parser->lexer);
      /* We're not being pedantic while the `__extension__' keyword is
	 in effect.  */
      pedantic = 0;

      return true;
    }

  return false;
}

/* Parse a label declaration.

   label-declaration:
     __label__ label-declarator-seq ;

   label-declarator-seq:
     identifier , label-declarator-seq
     identifier  */

static void
cp_parser_label_declaration (cp_parser* parser)
{
  /* Look for the `__label__' keyword.  */
  cp_parser_require_keyword (parser, RID_LABEL, "`__label__'");

  while (true)
    {
      tree identifier;

      /* Look for an identifier.  */
      identifier = cp_parser_identifier (parser);
      /* If we failed, stop.  */
      if (identifier == error_mark_node)
	break;
      /* Declare it as a label.  */
      finish_label_decl (identifier);
      /* If the next token is a `;', stop.  */
      if (cp_lexer_next_token_is (parser->lexer, CPP_SEMICOLON))
	break;
      /* Look for the `,' separating the label declarations.  */
      cp_parser_require (parser, CPP_COMMA, "`,'");
    }

  /* Look for the final `;'.  */
  cp_parser_require (parser, CPP_SEMICOLON, "`;'");
}

/* Support Functions */

/* Looks up NAME in the current scope, as given by PARSER->SCOPE.
   NAME should have one of the representations used for an
   id-expression.  If NAME is the ERROR_MARK_NODE, the ERROR_MARK_NODE
   is returned.  If PARSER->SCOPE is a dependent type, then a
   SCOPE_REF is returned.

   If NAME is a TEMPLATE_ID_EXPR, then it will be immediately
   returned; the name was already resolved when the TEMPLATE_ID_EXPR
   was formed.  Abstractly, such entities should not be passed to this
   function, because they do not need to be looked up, but it is
   simpler to check for this special case here, rather than at the
   call-sites.

   In cases not explicitly covered above, this function returns a
   DECL, OVERLOAD, or baselink representing the result of the lookup.
   If there was no entity with the indicated NAME, the ERROR_MARK_NODE
   is returned.

   If IS_TYPE is TRUE, bindings that do not refer to types are
   ignored.

   If IS_TEMPLATE is TRUE, bindings that do not refer to templates are
   ignored.

   If IS_NAMESPACE is TRUE, bindings that do not refer to namespaces
   are ignored.

   If CHECK_DEPENDENCY is TRUE, names are not looked up in dependent
   types.  */

static tree
cp_parser_lookup_name (cp_parser *parser, tree name, 
		       bool is_type, bool is_template, bool is_namespace,
		       bool check_dependency)
{
  int flags = 0;
  tree decl;
  tree object_type = parser->context->object_type;

  /* Now that we have looked up the name, the OBJECT_TYPE (if any) is
     no longer valid.  Note that if we are parsing tentatively, and
     the parse fails, OBJECT_TYPE will be automatically restored.  */
  parser->context->object_type = NULL_TREE;

  if (name == error_mark_node)
    return error_mark_node;

  if (!cp_parser_parsing_tentatively (parser)
      || cp_parser_committed_to_tentative_parse (parser))
    flags |= LOOKUP_COMPLAIN;

  /* A template-id has already been resolved; there is no lookup to
     do.  */
  if (TREE_CODE (name) == TEMPLATE_ID_EXPR)
    return name;
  if (BASELINK_P (name))
    {
      my_friendly_assert ((TREE_CODE (BASELINK_FUNCTIONS (name))
			   == TEMPLATE_ID_EXPR),
			  20020909);
      return name;
    }

  /* A BIT_NOT_EXPR is used to represent a destructor.  By this point,
     it should already have been checked to make sure that the name
     used matches the type being destroyed.  */
  if (TREE_CODE (name) == BIT_NOT_EXPR)
    {
      tree type;

      /* Figure out to which type this destructor applies.  */
      if (parser->scope)
	type = parser->scope;
      else if (object_type)
	type = object_type;
      else
	type = current_class_type;
      /* If that's not a class type, there is no destructor.  */
      if (!type || !CLASS_TYPE_P (type))
	return error_mark_node;
      if (!CLASSTYPE_DESTRUCTORS (type))
	  return error_mark_node;
      /* If it was a class type, return the destructor.  */
      return CLASSTYPE_DESTRUCTORS (type);
    }

  /* By this point, the NAME should be an ordinary identifier.  If
     the id-expression was a qualified name, the qualifying scope is
     stored in PARSER->SCOPE at this point.  */
  my_friendly_assert (TREE_CODE (name) == IDENTIFIER_NODE,
		      20000619);
  
  /* Perform the lookup.  */
  if (parser->scope)
    { 
      bool dependent_p;

      if (parser->scope == error_mark_node)
	return error_mark_node;

      /* If the SCOPE is dependent, the lookup must be deferred until
	 the template is instantiated -- unless we are explicitly
	 looking up names in uninstantiated templates.  Even then, we
	 cannot look up the name if the scope is not a class type; it
	 might, for example, be a template type parameter.  */
      dependent_p = (TYPE_P (parser->scope)
		     && !(parser->in_declarator_p
			  && currently_open_class (parser->scope))
		     && dependent_type_p (parser->scope));
      if ((check_dependency || !CLASS_TYPE_P (parser->scope))
	   && dependent_p)
	{
	  if (is_type)
	    /* The resolution to Core Issue 180 says that `struct A::B'
	       should be considered a type-name, even if `A' is
	       dependent.  */
	    decl = TYPE_NAME (make_typename_type (parser->scope,
						  name,
						  /*complain=*/1));
	  else if (is_template)
	    decl = make_unbound_class_template (parser->scope,
						name,
						/*complain=*/1);
	  else
	    decl = build_nt (SCOPE_REF, parser->scope, name);
	}
      else
	{
	  bool pop_p = false;

	  /* If PARSER->SCOPE is a dependent type, then it must be a
	     class type, and we must not be checking dependencies;
	     otherwise, we would have processed this lookup above.  So
	     that PARSER->SCOPE is not considered a dependent base by
	     lookup_member, we must enter the scope here.  */
	  if (dependent_p)
	    pop_p = push_scope (parser->scope);
	  /* If the PARSER->SCOPE is a a template specialization, it
	     may be instantiated during name lookup.  In that case,
	     errors may be issued.  Even if we rollback the current
	     tentative parse, those errors are valid.  */
	  decl = lookup_qualified_name (parser->scope, name, is_type,
					/*complain=*/true);
	  if (pop_p)
	    pop_scope (parser->scope);
	}
      parser->qualifying_scope = parser->scope;
      parser->object_scope = NULL_TREE;
    }
  else if (object_type)
    {
      tree object_decl = NULL_TREE;
      /* Look up the name in the scope of the OBJECT_TYPE, unless the
	 OBJECT_TYPE is not a class.  */
      if (CLASS_TYPE_P (object_type))
	/* If the OBJECT_TYPE is a template specialization, it may
	   be instantiated during name lookup.  In that case, errors
	   may be issued.  Even if we rollback the current tentative
	   parse, those errors are valid.  */
	object_decl = lookup_member (object_type,
				     name,
				     /*protect=*/0, is_type);
      /* Look it up in the enclosing context, too.  */
      decl = lookup_name_real (name, is_type, /*nonclass=*/0, 
			       is_namespace, flags);
      parser->object_scope = object_type;
      parser->qualifying_scope = NULL_TREE;
      if (object_decl)
	decl = object_decl;
    }
  else
    {
      decl = lookup_name_real (name, is_type, /*nonclass=*/0, 
			       is_namespace, flags);
      parser->qualifying_scope = NULL_TREE;
      parser->object_scope = NULL_TREE;
    }

  /* If the lookup failed, let our caller know.  */
  if (!decl 
      || decl == error_mark_node
      || (TREE_CODE (decl) == FUNCTION_DECL 
	  && DECL_ANTICIPATED (decl)))
    return error_mark_node;

  /* If it's a TREE_LIST, the result of the lookup was ambiguous.  */
  if (TREE_CODE (decl) == TREE_LIST)
    {
      /* The error message we have to print is too complicated for
	 cp_parser_error, so we incorporate its actions directly.  */
      if (!cp_parser_simulate_error (parser))
	{
	  error ("reference to `%D' is ambiguous", name);
	  print_candidates (decl);
	}
      return error_mark_node;
    }

  my_friendly_assert (DECL_P (decl) 
		      || TREE_CODE (decl) == OVERLOAD
		      || TREE_CODE (decl) == SCOPE_REF
		      || TREE_CODE (decl) == UNBOUND_CLASS_TEMPLATE
		      || BASELINK_P (decl),
		      20000619);

  /* If we have resolved the name of a member declaration, check to
     see if the declaration is accessible.  When the name resolves to
     set of overloaded functions, accessibility is checked when
     overload resolution is done.  

     During an explicit instantiation, access is not checked at all,
     as per [temp.explicit].  */
  if (DECL_P (decl))
    check_accessibility_of_qualified_id (decl, object_type, parser->scope);

  return decl;
}

/* Like cp_parser_lookup_name, but for use in the typical case where
   CHECK_ACCESS is TRUE, IS_TYPE is FALSE, IS_TEMPLATE is FALSE,
   IS_NAMESPACE is FALSE, and CHECK_DEPENDENCY is TRUE.  */

static tree
cp_parser_lookup_name_simple (cp_parser* parser, tree name)
{
  return cp_parser_lookup_name (parser, name, 
				/*is_type=*/false,
				/*is_template=*/false,
				/*is_namespace=*/false,
				/*check_dependency=*/true);
}

/* If DECL is a TEMPLATE_DECL that can be treated like a TYPE_DECL in
   the current context, return the TYPE_DECL.  If TAG_NAME_P is
   true, the DECL indicates the class being defined in a class-head,
   or declared in an elaborated-type-specifier.

   Otherwise, return DECL.  */

static tree
cp_parser_maybe_treat_template_as_class (tree decl, bool tag_name_p)
{
  /* If the TEMPLATE_DECL is being declared as part of a class-head,
     the translation from TEMPLATE_DECL to TYPE_DECL occurs:

       struct A { 
         template <typename T> struct B;
       };

       template <typename T> struct A::B {}; 
   
     Similarly, in a elaborated-type-specifier:

       namespace N { struct X{}; }

       struct A {
         template <typename T> friend struct N::X;
       };

     However, if the DECL refers to a class type, and we are in
     the scope of the class, then the name lookup automatically
     finds the TYPE_DECL created by build_self_reference rather
     than a TEMPLATE_DECL.  For example, in:

       template <class T> struct S {
         S s;
       };

     there is no need to handle such case.  */

  if (DECL_CLASS_TEMPLATE_P (decl) && tag_name_p)
    return DECL_TEMPLATE_RESULT (decl);

  return decl;
}

/* If too many, or too few, template-parameter lists apply to the
   declarator, issue an error message.  Returns TRUE if all went well,
   and FALSE otherwise.  */

static bool
cp_parser_check_declarator_template_parameters (cp_parser* parser, 
                                                tree declarator)
{
  unsigned num_templates;

  /* We haven't seen any classes that involve template parameters yet.  */
  num_templates = 0;

  switch (TREE_CODE (declarator))
    {
    case CALL_EXPR:
    case ARRAY_REF:
    case INDIRECT_REF:
    case ADDR_EXPR:
      {
	tree main_declarator = TREE_OPERAND (declarator, 0);
	return
	  cp_parser_check_declarator_template_parameters (parser, 
							  main_declarator);
      }

    case SCOPE_REF:
      {
	tree scope;
	tree member;

	scope = TREE_OPERAND (declarator, 0);
	member = TREE_OPERAND (declarator, 1);

	/* If this is a pointer-to-member, then we are not interested
	   in the SCOPE, because it does not qualify the thing that is
	   being declared.  */
	if (TREE_CODE (member) == INDIRECT_REF)
	  return (cp_parser_check_declarator_template_parameters
		  (parser, member));

	while (scope && CLASS_TYPE_P (scope))
	  {
	    /* You're supposed to have one `template <...>'
	       for every template class, but you don't need one
	       for a full specialization.  For example:
	       
	       template <class T> struct S{};
	       template <> struct S<int> { void f(); };
	       void S<int>::f () {}
	       
	       is correct; there shouldn't be a `template <>' for
	       the definition of `S<int>::f'.  */
	    if (CLASSTYPE_TEMPLATE_INFO (scope)
		&& (CLASSTYPE_TEMPLATE_INSTANTIATION (scope)
		    || uses_template_parms (CLASSTYPE_TI_ARGS (scope)))
		&& PRIMARY_TEMPLATE_P (CLASSTYPE_TI_TEMPLATE (scope)))
	      ++num_templates;

	    scope = TYPE_CONTEXT (scope);
	  }
      }

      /* Fall through.  */

    default:
      /* If the DECLARATOR has the form `X<y>' then it uses one
	 additional level of template parameters.  */
      if (TREE_CODE (declarator) == TEMPLATE_ID_EXPR)
	++num_templates;

      return cp_parser_check_template_parameters (parser, 
						  num_templates);
    }
}

/* NUM_TEMPLATES were used in the current declaration.  If that is
   invalid, return FALSE and issue an error messages.  Otherwise,
   return TRUE.  */

static bool
cp_parser_check_template_parameters (cp_parser* parser,
                                     unsigned num_templates)
{
  /* If there are more template classes than parameter lists, we have
     something like:
     
       template <class T> void S<T>::R<T>::f ();  */
  if (parser->num_template_parameter_lists < num_templates)
    {
      error ("too few template-parameter-lists");
      return false;
    }
  /* If there are the same number of template classes and parameter
     lists, that's OK.  */
  if (parser->num_template_parameter_lists == num_templates)
    return true;
  /* If there are more, but only one more, then we are referring to a
     member template.  That's OK too.  */
  if (parser->num_template_parameter_lists == num_templates + 1)
      return true;
  /* Otherwise, there are too many template parameter lists.  We have
     something like:

     template <class T> template <class U> void S::f();  */
  error ("too many template-parameter-lists");
  return false;
}

/* Parse a binary-expression of the general form:

   binary-expression:
     <expr>
     binary-expression <token> <expr>

   The TOKEN_TREE_MAP maps <token> types to <expr> codes.  FN is used
   to parser the <expr>s.  If the first production is used, then the
   value returned by FN is returned directly.  Otherwise, a node with
   the indicated EXPR_TYPE is returned, with operands corresponding to
   the two sub-expressions.  */

static tree
cp_parser_binary_expression (cp_parser* parser, 
                             const cp_parser_token_tree_map token_tree_map, 
                             cp_parser_expression_fn fn)
{
  tree lhs;

  /* Parse the first expression.  */
  lhs = (*fn) (parser);
  /* Now, look for more expressions.  */
  while (true)
    {
      cp_token *token;
      const cp_parser_token_tree_map_node *map_node;
      tree rhs;

      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);
      /* If the token is `>', and that's not an operator at the
	 moment, then we're done.  */
      if (token->type == CPP_GREATER
	  && !parser->greater_than_is_operator_p)
	break;
      /* If we find one of the tokens we want, build the corresponding
	 tree representation.  */
      for (map_node = token_tree_map; 
	   map_node->token_type != CPP_EOF;
	   ++map_node)
	if (map_node->token_type == token->type)
	  {
	    /* Assume that an overloaded operator will not be used.  */
	    bool overloaded_p = false;

	    /* Consume the operator token.  */
	    cp_lexer_consume_token (parser->lexer);
	    /* Parse the right-hand side of the expression.  */
	    rhs = (*fn) (parser);
	    /* Build the binary tree node.  */
	    lhs = build_x_binary_op (map_node->tree_type, lhs, rhs, 
				     &overloaded_p);
	    /* If the binary operator required the use of an
	       overloaded operator, then this expression cannot be an
	       integral constant-expression.  An overloaded operator
	       can be used even if both operands are otherwise
	       permissible in an integral constant-expression if at
	       least one of the operands is of enumeration type.  */
	    if (overloaded_p
		&& (cp_parser_non_integral_constant_expression 
		    (parser, "calls to overloaded operators")))
	      lhs = error_mark_node;
	    break;
	  }

      /* If the token wasn't one of the ones we want, we're done.  */
      if (map_node->token_type == CPP_EOF)
	break;
    }

  return lhs;
}

/* Parse an optional `::' token indicating that the following name is
   from the global namespace.  If so, PARSER->SCOPE is set to the
   GLOBAL_NAMESPACE. Otherwise, PARSER->SCOPE is set to NULL_TREE,
   unless CURRENT_SCOPE_VALID_P is TRUE, in which case it is left alone.
   Returns the new value of PARSER->SCOPE, if the `::' token is
   present, and NULL_TREE otherwise.  */

static tree
cp_parser_global_scope_opt (cp_parser* parser, bool current_scope_valid_p)
{
  cp_token *token;

  /* Peek at the next token.  */
  token = cp_lexer_peek_token (parser->lexer);
  /* If we're looking at a `::' token then we're starting from the
     global namespace, not our current location.  */
  if (token->type == CPP_SCOPE)
    {
      /* Consume the `::' token.  */
      cp_lexer_consume_token (parser->lexer);
      /* Set the SCOPE so that we know where to start the lookup.  */
      parser->scope = global_namespace;
      parser->qualifying_scope = global_namespace;
      parser->object_scope = NULL_TREE;

      return parser->scope;
    }
  else if (!current_scope_valid_p)
    {
      parser->scope = NULL_TREE;
      parser->qualifying_scope = NULL_TREE;
      parser->object_scope = NULL_TREE;
    }

  return NULL_TREE;
}

/* Returns TRUE if the upcoming token sequence is the start of a
   constructor declarator.  If FRIEND_P is true, the declarator is
   preceded by the `friend' specifier.  */

static bool
cp_parser_constructor_declarator_p (cp_parser *parser, bool friend_p)
{
  bool constructor_p;
  tree type_decl = NULL_TREE;
  bool nested_name_p;
  cp_token *next_token;

  /* The common case is that this is not a constructor declarator, so
     try to avoid doing lots of work if at all possible.  It's not
     valid declare a constructor at function scope.  */
  if (at_function_scope_p ())
    return false;
  /* And only certain tokens can begin a constructor declarator.  */
  next_token = cp_lexer_peek_token (parser->lexer);
  if (next_token->type != CPP_NAME
      && next_token->type != CPP_SCOPE
      && next_token->type != CPP_NESTED_NAME_SPECIFIER
      && next_token->type != CPP_TEMPLATE_ID)
    return false;

  /* Parse tentatively; we are going to roll back all of the tokens
     consumed here.  */
  cp_parser_parse_tentatively (parser);
  /* Assume that we are looking at a constructor declarator.  */
  constructor_p = true;

  /* Look for the optional `::' operator.  */
  cp_parser_global_scope_opt (parser,
			      /*current_scope_valid_p=*/false);
  /* Look for the nested-name-specifier.  */
  nested_name_p 
    = (cp_parser_nested_name_specifier_opt (parser,
					    /*typename_keyword_p=*/false,
					    /*check_dependency_p=*/false,
					    /*type_p=*/false,
					    /*is_declaration=*/false)
       != NULL_TREE);
  /* Outside of a class-specifier, there must be a
     nested-name-specifier.  */
  if (!nested_name_p && 
      (!at_class_scope_p () || !TYPE_BEING_DEFINED (current_class_type)
       || friend_p))
    constructor_p = false;
  /* If we still think that this might be a constructor-declarator,
     look for a class-name.  */
  if (constructor_p)
    {
      /* If we have:

	   template <typename T> struct S { S(); };
	   template <typename T> S<T>::S ();

	 we must recognize that the nested `S' names a class.
	 Similarly, for:

	   template <typename T> S<T>::S<T> ();

	 we must recognize that the nested `S' names a template.  */
      type_decl = cp_parser_class_name (parser,
					/*typename_keyword_p=*/false,
					/*template_keyword_p=*/false,
					/*type_p=*/false,
					/*check_dependency_p=*/false,
					/*class_head_p=*/false,
					/*is_declaration=*/false);
      /* If there was no class-name, then this is not a constructor.  */
      constructor_p = !cp_parser_error_occurred (parser);
    }

  /* If we're still considering a constructor, we have to see a `(',
     to begin the parameter-declaration-clause, followed by either a
     `)', an `...', or a decl-specifier.  We need to check for a
     type-specifier to avoid being fooled into thinking that:

       S::S (f) (int);

     is a constructor.  (It is actually a function named `f' that
     takes one parameter (of type `int') and returns a value of type
     `S::S'.  */
  if (constructor_p 
      && cp_parser_require (parser, CPP_OPEN_PAREN, "`('"))
    {
      if (cp_lexer_next_token_is_not (parser->lexer, CPP_CLOSE_PAREN)
	  && cp_lexer_next_token_is_not (parser->lexer, CPP_ELLIPSIS)
	  /* A parameter declaration begins with a decl-specifier,
	     which is either the "attribute" keyword, a storage class
	     specifier, or (usually) a type-specifier.  */
	  && !cp_lexer_next_token_is_keyword (parser->lexer, RID_ATTRIBUTE)
	  && !cp_parser_storage_class_specifier_opt (parser))
	{
	  tree type;
	  bool pop_p = false;
	  unsigned saved_num_template_parameter_lists;

	  /* Names appearing in the type-specifier should be looked up
	     in the scope of the class.  */
	  if (current_class_type)
	    type = NULL_TREE;
	  else
	    {
	      type = TREE_TYPE (type_decl);
	      if (TREE_CODE (type) == TYPENAME_TYPE)
		{
		  type = resolve_typename_type (type, 
						/*only_current_p=*/false);
		  if (type == error_mark_node)
		    {
		      cp_parser_abort_tentative_parse (parser);
		      return false;
		    }
		}
	      pop_p = push_scope (type);
	    }

	  /* Inside the constructor parameter list, surrounding
	     template-parameter-lists do not apply.  */
	  saved_num_template_parameter_lists
	    = parser->num_template_parameter_lists;
	  parser->num_template_parameter_lists = 0;

	  /* Look for the type-specifier.  */
	  cp_parser_type_specifier (parser,
				    CP_PARSER_FLAGS_NONE,
				    /*is_friend=*/false,
				    /*is_declarator=*/true,
				    /*declares_class_or_enum=*/NULL,
				    /*is_cv_qualifier=*/NULL);

	  parser->num_template_parameter_lists
	    = saved_num_template_parameter_lists;

	  /* Leave the scope of the class.  */
	  if (pop_p)
	    pop_scope (type);

	  constructor_p = !cp_parser_error_occurred (parser);
	}
    }
  else
    constructor_p = false;
  /* We did not really want to consume any tokens.  */
  cp_parser_abort_tentative_parse (parser);

  return constructor_p;
}

/* Parse the definition of the function given by the DECL_SPECIFIERS,
   ATTRIBUTES, and DECLARATOR.  The access checks have been deferred;
   they must be performed once we are in the scope of the function.

   Returns the function defined.  */

static tree
cp_parser_function_definition_from_specifiers_and_declarator
  (cp_parser* parser,
   tree decl_specifiers,
   tree attributes,
   tree declarator)
{
  tree fn;
  bool success_p;

  /* Begin the function-definition.  */
  success_p = begin_function_definition (decl_specifiers, 
					 attributes, 
					 declarator);

  /* If there were names looked up in the decl-specifier-seq that we
     did not check, check them now.  We must wait until we are in the
     scope of the function to perform the checks, since the function
     might be a friend.  */
  perform_deferred_access_checks ();

  if (!success_p)
    {
      /* If begin_function_definition didn't like the definition, skip
	 the entire function.  */
      error ("invalid function declaration");
      cp_parser_skip_to_end_of_block_or_statement (parser);
      fn = error_mark_node;
    }
  else
    fn = cp_parser_function_definition_after_declarator (parser,
							 /*inline_p=*/false);

  return fn;
}

/* Parse the part of a function-definition that follows the
   declarator.  INLINE_P is TRUE iff this function is an inline
   function defined with a class-specifier.

   Returns the function defined.  */

static tree 
cp_parser_function_definition_after_declarator (cp_parser* parser, 
						bool inline_p)
{
  tree fn;
  bool ctor_initializer_p = false;
  bool saved_in_unbraced_linkage_specification_p;
  unsigned saved_num_template_parameter_lists;

  /* If the next token is `return', then the code may be trying to
     make use of the "named return value" extension that G++ used to
     support.  */
  if (cp_lexer_next_token_is_keyword (parser->lexer, RID_RETURN))
    {
      /* Consume the `return' keyword.  */
      cp_lexer_consume_token (parser->lexer);
      /* Look for the identifier that indicates what value is to be
	 returned.  */
      cp_parser_identifier (parser);
      /* Issue an error message.  */
      error ("named return values are no longer supported");
      /* Skip tokens until we reach the start of the function body.  */
      while (cp_lexer_next_token_is_not (parser->lexer, CPP_OPEN_BRACE)
	     && cp_lexer_next_token_is_not (parser->lexer, CPP_EOF))
	cp_lexer_consume_token (parser->lexer);
    }
  /* The `extern' in `extern "C" void f () { ... }' does not apply to
     anything declared inside `f'.  */
  saved_in_unbraced_linkage_specification_p 
    = parser->in_unbraced_linkage_specification_p;
  parser->in_unbraced_linkage_specification_p = false;
  /* Inside the function, surrounding template-parameter-lists do not
     apply.  */
  saved_num_template_parameter_lists 
    = parser->num_template_parameter_lists; 
  parser->num_template_parameter_lists = 0;
  /* If the next token is `try', then we are looking at a
     function-try-block.  */
  if (cp_lexer_next_token_is_keyword (parser->lexer, RID_TRY))
    ctor_initializer_p = cp_parser_function_try_block (parser);
  /* A function-try-block includes the function-body, so we only do
     this next part if we're not processing a function-try-block.  */
  else
    ctor_initializer_p 
      = cp_parser_ctor_initializer_opt_and_function_body (parser);

  /* Finish the function.  */
  fn = finish_function ((ctor_initializer_p ? 1 : 0) | 
			(inline_p ? 2 : 0));
  /* Generate code for it, if necessary.  */
  expand_or_defer_fn (fn);
  /* Restore the saved values.  */
  parser->in_unbraced_linkage_specification_p 
    = saved_in_unbraced_linkage_specification_p;
  parser->num_template_parameter_lists 
    = saved_num_template_parameter_lists;

  return fn;
}

/* Parse a template-declaration, assuming that the `export' (and
   `extern') keywords, if present, has already been scanned.  MEMBER_P
   is as for cp_parser_template_declaration.  */

static void
cp_parser_template_declaration_after_export (cp_parser* parser, bool member_p)
{
  tree decl = NULL_TREE;
  tree parameter_list;
  bool friend_p = false;

  /* Look for the `template' keyword.  */
  if (!cp_parser_require_keyword (parser, RID_TEMPLATE, "`template'"))
    return;
      
  /* And the `<'.  */
  if (!cp_parser_require (parser, CPP_LESS, "`<'"))
    return;
      
  /* If the next token is `>', then we have an invalid
     specialization.  Rather than complain about an invalid template
     parameter, issue an error message here.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_GREATER))
    {
      cp_parser_error (parser, "invalid explicit specialization");
      begin_specialization ();
      parameter_list = NULL_TREE;
    }
  else
    {
      /* Parse the template parameters.  */
      begin_template_parm_list ();
      parameter_list = cp_parser_template_parameter_list (parser);
      parameter_list = end_template_parm_list (parameter_list);
    }

  /* Look for the `>'.  */
  cp_parser_skip_until_found (parser, CPP_GREATER, "`>'");
  /* We just processed one more parameter list.  */
  ++parser->num_template_parameter_lists;
  /* If the next token is `template', there are more template
     parameters.  */
  if (cp_lexer_next_token_is_keyword (parser->lexer, 
				      RID_TEMPLATE))
    cp_parser_template_declaration_after_export (parser, member_p);
  else
    {
      decl = cp_parser_single_declaration (parser,
					   member_p,
					   &friend_p);

      /* If this is a member template declaration, let the front
	 end know.  */
      if (member_p && !friend_p && decl)
	{
	  if (TREE_CODE (decl) == TYPE_DECL)
	    cp_parser_check_access_in_redeclaration (decl);

	  decl = finish_member_template_decl (decl);
	}
      else if (friend_p && decl && TREE_CODE (decl) == TYPE_DECL)
	make_friend_class (current_class_type, TREE_TYPE (decl),
			   /*complain=*/true);
    }
  /* We are done with the current parameter list.  */
  --parser->num_template_parameter_lists;

  /* Finish up.  */
  finish_template_decl (parameter_list);

  /* Register member declarations.  */
  if (member_p && !friend_p && decl && !DECL_CLASS_TEMPLATE_P (decl))
    finish_member_declaration (decl);

  /* If DECL is a function template, we must return to parse it later.
     (Even though there is no definition, there might be default
     arguments that need handling.)  */
  if (member_p && decl 
      && (TREE_CODE (decl) == FUNCTION_DECL
	  || DECL_FUNCTION_TEMPLATE_P (decl)))
    TREE_VALUE (parser->unparsed_functions_queues)
      = tree_cons (NULL_TREE, decl, 
		   TREE_VALUE (parser->unparsed_functions_queues));
}

/* Parse a `decl-specifier-seq [opt] init-declarator [opt] ;' or
   `function-definition' sequence.  MEMBER_P is true, this declaration
   appears in a class scope.

   Returns the DECL for the declared entity.  If FRIEND_P is non-NULL,
   *FRIEND_P is set to TRUE iff the declaration is a friend.  */

static tree
cp_parser_single_declaration (cp_parser* parser, 
			      bool member_p,
			      bool* friend_p)
{
  int declares_class_or_enum;
  tree decl = NULL_TREE;
  tree decl_specifiers;
  tree attributes;
  bool function_definition_p = false;

  /* This function is only used when processing a template
     declaration.  */
  if (innermost_scope_kind () != sk_template_parms
      && innermost_scope_kind () != sk_template_spec)
    abort ();

  /* Defer access checks until we know what is being declared.  */
  push_deferring_access_checks (dk_deferred);

  /* Try the `decl-specifier-seq [opt] init-declarator [opt]'
     alternative.  */
  decl_specifiers 
    = cp_parser_decl_specifier_seq (parser,
				    CP_PARSER_FLAGS_OPTIONAL,
				    &attributes,
				    &declares_class_or_enum);
  if (friend_p)
    *friend_p = cp_parser_friend_p (decl_specifiers);

  /* There are no template typedefs.  */
  if (cp_parser_typedef_p (decl_specifiers))
    {
      error ("template declaration of `typedef'");
      decl = error_mark_node;
    }

  /* Gather up the access checks that occurred the
     decl-specifier-seq.  */
  stop_deferring_access_checks ();

  /* Check for the declaration of a template class.  */
  if (declares_class_or_enum)
    {
      if (cp_parser_declares_only_class_p (parser))
	{
	  decl = shadow_tag (decl_specifiers);
	  if (decl)
	    decl = TYPE_NAME (decl);
	  else
	    decl = error_mark_node;
	}
    }
  /* If it's not a template class, try for a template function.  If
     the next token is a `;', then this declaration does not declare
     anything.  But, if there were errors in the decl-specifiers, then
     the error might well have come from an attempted class-specifier.
     In that case, there's no need to warn about a missing declarator.  */
  if (!decl
      && (cp_lexer_next_token_is_not (parser->lexer, CPP_SEMICOLON)
	  || !value_member (error_mark_node, decl_specifiers)))
    decl = cp_parser_init_declarator (parser, 
				      decl_specifiers,
				      attributes,
				      /*function_definition_allowed_p=*/true,
				      member_p,
				      declares_class_or_enum,
				      &function_definition_p);

  pop_deferring_access_checks ();

  /* Clear any current qualification; whatever comes next is the start
     of something new.  */
  parser->scope = NULL_TREE;
  parser->qualifying_scope = NULL_TREE;
  parser->object_scope = NULL_TREE;
  /* Look for a trailing `;' after the declaration.  */
  if (!function_definition_p
      && (decl == error_mark_node
	  || !cp_parser_require (parser, CPP_SEMICOLON, "`;'")))
    cp_parser_skip_to_end_of_block_or_statement (parser);

  return decl;
}

/* Parse a cast-expression that is not the operand of a unary "&".  */

static tree
cp_parser_simple_cast_expression (cp_parser *parser)
{
  return cp_parser_cast_expression (parser, /*address_p=*/false);
}

/* Parse a functional cast to TYPE.  Returns an expression
   representing the cast.  */

static tree
cp_parser_functional_cast (cp_parser* parser, tree type)
{
  tree expression_list;
  tree cast;

  expression_list
    = cp_parser_parenthesized_expression_list (parser, false,
					       /*non_constant_p=*/NULL);

  cast = build_functional_cast (type, expression_list);
  /* [expr.const]/1: In an integral constant expression "only type
     conversions to integral or enumeration type can be used".  */
  if (TREE_CODE (type) == TYPE_DECL)
    type = TREE_TYPE (type);
  if (cast != error_mark_node && !dependent_type_p (type)
      && !INTEGRAL_OR_ENUMERATION_TYPE_P (type))
    {
      if (cp_parser_non_integral_constant_expression 
	  (parser, "a call to a constructor"))
	return error_mark_node;
    }
  return cast;
}

/* Save the tokens that make up the body of a member function defined
   in a class-specifier.  The DECL_SPECIFIERS and DECLARATOR have
   already been parsed.  The ATTRIBUTES are any GNU "__attribute__"
   specifiers applied to the declaration.  Returns the FUNCTION_DECL
   for the member function.  */

static tree
cp_parser_save_member_function_body (cp_parser* parser,
				     tree decl_specifiers,
				     tree declarator,
				     tree attributes)
{
  cp_token_cache *cache;
  tree fn;

  /* Create the function-declaration.  */
  fn = start_method (decl_specifiers, declarator, attributes);
  /* If something went badly wrong, bail out now.  */
  if (fn == error_mark_node)
    {
      /* If there's a function-body, skip it.  */
      if (cp_parser_token_starts_function_definition_p 
	  (cp_lexer_peek_token (parser->lexer)))
	cp_parser_skip_to_end_of_block_or_statement (parser);
      return error_mark_node;
    }

  /* Remember it, if there default args to post process.  */
  cp_parser_save_default_args (parser, fn);

  /* Create a token cache.  */
  cache = cp_token_cache_new ();
  /* Save away the tokens that make up the body of the 
     function.  */
  cp_parser_cache_group (parser, cache, CPP_CLOSE_BRACE, /*depth=*/0);
  /* Handle function try blocks.  */
  while (cp_lexer_next_token_is_keyword (parser->lexer, RID_CATCH))
    cp_parser_cache_group (parser, cache, CPP_CLOSE_BRACE, /*depth=*/0);

  /* Save away the inline definition; we will process it when the
     class is complete.  */
  DECL_PENDING_INLINE_INFO (fn) = cache;
  DECL_PENDING_INLINE_P (fn) = 1;

  /* We need to know that this was defined in the class, so that
     friend templates are handled correctly.  */
  DECL_INITIALIZED_IN_CLASS_P (fn) = 1;

  /* We're done with the inline definition.  */
  finish_method (fn);

  /* Add FN to the queue of functions to be parsed later.  */
  TREE_VALUE (parser->unparsed_functions_queues)
    = tree_cons (NULL_TREE, fn, 
		 TREE_VALUE (parser->unparsed_functions_queues));

  return fn;
}

/* Parse a template-argument-list, as well as the trailing ">" (but
   not the opening ">").  See cp_parser_template_argument_list for the
   return value.  */

static tree
cp_parser_enclosed_template_argument_list (cp_parser* parser)
{
  tree arguments;
  tree saved_scope;
  tree saved_qualifying_scope;
  tree saved_object_scope;
  bool saved_greater_than_is_operator_p;

  /* [temp.names]

     When parsing a template-id, the first non-nested `>' is taken as
     the end of the template-argument-list rather than a greater-than
     operator.  */
  saved_greater_than_is_operator_p 
    = parser->greater_than_is_operator_p;
  parser->greater_than_is_operator_p = false;
  /* Parsing the argument list may modify SCOPE, so we save it
     here.  */
  saved_scope = parser->scope;
  saved_qualifying_scope = parser->qualifying_scope;
  saved_object_scope = parser->object_scope;
  /* Parse the template-argument-list itself.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_GREATER))
    arguments = NULL_TREE;
  else
    arguments = cp_parser_template_argument_list (parser);
  /* Look for the `>' that ends the template-argument-list. If we find
     a '>>' instead, it's probably just a typo.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_RSHIFT))
    {
      if (!saved_greater_than_is_operator_p)
	{
	  /* If we're in a nested template argument list, the '>>' has to be
	    a typo for '> >'. We emit the error message, but we continue
	    parsing and we push a '>' as next token, so that the argument
	    list will be parsed correctly..  */
	  cp_token* token;
	  error ("`>>' should be `> >' within a nested template argument list");
	  token = cp_lexer_peek_token (parser->lexer);
	  token->type = CPP_GREATER;
	}
      else
	{
	  /* If this is not a nested template argument list, the '>>' is
	    a typo for '>'. Emit an error message and continue.  */
	  error ("spurious `>>', use `>' to terminate a template argument list");
	  cp_lexer_consume_token (parser->lexer);
	}
    }
  else
    cp_parser_skip_until_found (parser, CPP_GREATER, "`>'");
  /* The `>' token might be a greater-than operator again now.  */
  parser->greater_than_is_operator_p 
    = saved_greater_than_is_operator_p;
  /* Restore the SAVED_SCOPE.  */
  parser->scope = saved_scope;
  parser->qualifying_scope = saved_qualifying_scope;
  parser->object_scope = saved_object_scope;

  return arguments;
}

/* MEMBER_FUNCTION is a member function, or a friend.  If default
   arguments, or the body of the function have not yet been parsed,
   parse them now.  */

static void
cp_parser_late_parsing_for_member (cp_parser* parser, tree member_function)
{
  cp_lexer *saved_lexer;

  /* If this member is a template, get the underlying
     FUNCTION_DECL.  */
  if (DECL_FUNCTION_TEMPLATE_P (member_function))
    member_function = DECL_TEMPLATE_RESULT (member_function);

  /* There should not be any class definitions in progress at this
     point; the bodies of members are only parsed outside of all class
     definitions.  */
  my_friendly_assert (parser->num_classes_being_defined == 0, 20010816);
  /* While we're parsing the member functions we might encounter more
     classes.  We want to handle them right away, but we don't want
     them getting mixed up with functions that are currently in the
     queue.  */
  parser->unparsed_functions_queues
    = tree_cons (NULL_TREE, NULL_TREE, parser->unparsed_functions_queues);

  /* Make sure that any template parameters are in scope.  */
  maybe_begin_member_template_processing (member_function);

  /* If the body of the function has not yet been parsed, parse it
     now.  */
  if (DECL_PENDING_INLINE_P (member_function))
    {
      tree function_scope;
      cp_token_cache *tokens;

      /* The function is no longer pending; we are processing it.  */
      tokens = DECL_PENDING_INLINE_INFO (member_function);
      DECL_PENDING_INLINE_INFO (member_function) = NULL;
      DECL_PENDING_INLINE_P (member_function) = 0;
      
      /* If this is a local class, enter the scope of the containing
	 function.  */
      function_scope = current_function_decl;
      if (function_scope)
	push_function_context_to (function_scope);
      
      /* Save away the current lexer.  */
      saved_lexer = parser->lexer;
      /* Make a new lexer to feed us the tokens saved for this function.  */
      parser->lexer = cp_lexer_new_from_tokens (tokens);
      parser->lexer->next = saved_lexer;
      
      /* Set the current source position to be the location of the first
	 token in the saved inline body.  */
      cp_lexer_peek_token (parser->lexer);
      
      /* Let the front end know that we going to be defining this
	 function.  */
      start_function (NULL_TREE, member_function, NULL_TREE,
		      SF_PRE_PARSED | SF_INCLASS_INLINE);
      
      /* Now, parse the body of the function.  */
      cp_parser_function_definition_after_declarator (parser,
						      /*inline_p=*/true);
      
      /* Leave the scope of the containing function.  */
      if (function_scope)
	pop_function_context_from (function_scope);
      /* Restore the lexer.  */
      parser->lexer = saved_lexer;
    }

  /* Remove any template parameters from the symbol table.  */
  maybe_end_member_template_processing ();

  /* Restore the queue.  */
  parser->unparsed_functions_queues 
    = TREE_CHAIN (parser->unparsed_functions_queues);
}

/* If DECL contains any default args, remember it on the unparsed
   functions queue.  */

static void
cp_parser_save_default_args (cp_parser* parser, tree decl)
{
  tree probe;

  for (probe = TYPE_ARG_TYPES (TREE_TYPE (decl));
       probe;
       probe = TREE_CHAIN (probe))
    if (TREE_PURPOSE (probe))
      {
	TREE_PURPOSE (parser->unparsed_functions_queues)
	  = tree_cons (NULL_TREE, decl, 
		       TREE_PURPOSE (parser->unparsed_functions_queues));
	break;
      }
  return;
}

/* FN is a FUNCTION_DECL which may contains a parameter with an
   unparsed DEFAULT_ARG.  Parse the default args now.  */

static void
cp_parser_late_parsing_default_args (cp_parser *parser, tree fn)
{
  cp_lexer *saved_lexer;
  cp_token_cache *tokens;
  bool saved_local_variables_forbidden_p;
  tree parameters;

  /* While we're parsing the default args, we might (due to the
     statement expression extension) encounter more classes.  We want
     to handle them right away, but we don't want them getting mixed
     up with default args that are currently in the queue.  */
  parser->unparsed_functions_queues
    = tree_cons (NULL_TREE, NULL_TREE, parser->unparsed_functions_queues);

  for (parameters = TYPE_ARG_TYPES (TREE_TYPE (fn));
       parameters;
       parameters = TREE_CHAIN (parameters))
    {
      tree default_arg = TREE_PURPOSE (parameters);
      tree parsed_arg;

      if (!default_arg)
	continue;

      if (TREE_CODE (default_arg) != DEFAULT_ARG)
	/* This can happen for a friend declaration for a function
	   already declared with default arguments.  */
	continue;
  
      /* Save away the current lexer.  */
      saved_lexer = parser->lexer;
      /* Create a new one, using the tokens we have saved.  */
      tokens =  DEFARG_TOKENS (default_arg);
      parser->lexer = cp_lexer_new_from_tokens (tokens);

      /* Set the current source position to be the location of the
         first token in the default argument.  */
      cp_lexer_peek_token (parser->lexer);

      /* Local variable names (and the `this' keyword) may not appear
     	 in a default argument.  */
      saved_local_variables_forbidden_p = parser->local_variables_forbidden_p;
      parser->local_variables_forbidden_p = true;
      
      /* Parse the assignment-expression.  */
      if (DECL_FRIEND_CONTEXT (fn))
	push_nested_class (DECL_FRIEND_CONTEXT (fn));
      else if (DECL_CLASS_SCOPE_P (fn))
	push_nested_class (DECL_CONTEXT (fn));
      parsed_arg = cp_parser_assignment_expression (parser);
      if (DECL_FRIEND_CONTEXT (fn) || DECL_CLASS_SCOPE_P (fn))
	pop_nested_class ();
      
      TREE_PURPOSE (parameters) = parsed_arg;
      
      /* Update any instantiations we've already created.  */
      for (default_arg = TREE_CHAIN (default_arg);
	   default_arg;
	   default_arg = TREE_CHAIN (default_arg))
	TREE_PURPOSE (TREE_PURPOSE (default_arg)) = parsed_arg;
     
      /* If the token stream has not been completely used up, then
	 there was extra junk after the end of the default
	 argument.  */
      if (!cp_lexer_next_token_is (parser->lexer, CPP_EOF))
	cp_parser_error (parser, "expected `,'");

       /* Restore saved state.  */
      parser->lexer = saved_lexer;
      parser->local_variables_forbidden_p = saved_local_variables_forbidden_p;
    }

  /* Make sure no default arg is missing.  */
  check_default_args (fn);

  /* Restore the queue.  */
  parser->unparsed_functions_queues 
    = TREE_CHAIN (parser->unparsed_functions_queues);
}

/* Parse the operand of `sizeof' (or a similar operator).  Returns
   either a TYPE or an expression, depending on the form of the
   input.  The KEYWORD indicates which kind of expression we have
   encountered.  */

static tree
cp_parser_sizeof_operand (cp_parser* parser, enum rid keyword)
{
  static const char *format;
  tree expr = NULL_TREE;
  const char *saved_message;
  bool saved_integral_constant_expression_p;

  /* Initialize FORMAT the first time we get here.  */
  if (!format)
    format = "types may not be defined in `%s' expressions";

  /* Types cannot be defined in a `sizeof' expression.  Save away the
     old message.  */
  saved_message = parser->type_definition_forbidden_message;
  /* And create the new one.  */
  parser->type_definition_forbidden_message 
    = xmalloc (strlen (format) 
	       + strlen (IDENTIFIER_POINTER (ridpointers[keyword]))
	       + 1 /* `\0' */);
  sprintf ((char *) parser->type_definition_forbidden_message,
	   format, IDENTIFIER_POINTER (ridpointers[keyword]));

  /* The restrictions on constant-expressions do not apply inside
     sizeof expressions.  */
  saved_integral_constant_expression_p = parser->integral_constant_expression_p;
  parser->integral_constant_expression_p = false;

  /* Do not actually evaluate the expression.  */
  ++skip_evaluation;
  /* If it's a `(', then we might be looking at the type-id
     construction.  */
  if (cp_lexer_next_token_is (parser->lexer, CPP_OPEN_PAREN))
    {
      tree type;
      bool saved_in_type_id_in_expr_p;

      /* We can't be sure yet whether we're looking at a type-id or an
	 expression.  */
      cp_parser_parse_tentatively (parser);
      /* Consume the `('.  */
      cp_lexer_consume_token (parser->lexer);
      /* Parse the type-id.  */
      saved_in_type_id_in_expr_p = parser->in_type_id_in_expr_p;
      parser->in_type_id_in_expr_p = true;
      type = cp_parser_type_id (parser);
      parser->in_type_id_in_expr_p = saved_in_type_id_in_expr_p;
      /* Now, look for the trailing `)'.  */
      cp_parser_require (parser, CPP_CLOSE_PAREN, "`)'");
      /* If all went well, then we're done.  */
      if (cp_parser_parse_definitely (parser))
	{
	  /* Build a list of decl-specifiers; right now, we have only
	     a single type-specifier.  */
	  type = build_tree_list (NULL_TREE,
				  type);

	  /* Call grokdeclarator to figure out what type this is.  */
	  expr = grokdeclarator (NULL_TREE,
				 type,
				 TYPENAME,
				 /*initialized=*/0,
				 /*attrlist=*/NULL);
	}
    }

  /* If the type-id production did not work out, then we must be
     looking at the unary-expression production.  */
  if (!expr)
    expr = cp_parser_unary_expression (parser, /*address_p=*/false);
  /* Go back to evaluating expressions.  */
  --skip_evaluation;

  /* Free the message we created.  */
  free ((char *) parser->type_definition_forbidden_message);
  /* And restore the old one.  */
  parser->type_definition_forbidden_message = saved_message;
  parser->integral_constant_expression_p = saved_integral_constant_expression_p;

  return expr;
}

/* If the current declaration has no declarator, return true.  */

static bool
cp_parser_declares_only_class_p (cp_parser *parser)
{
  /* If the next token is a `;' or a `,' then there is no 
     declarator.  */
  return (cp_lexer_next_token_is (parser->lexer, CPP_SEMICOLON)
	  || cp_lexer_next_token_is (parser->lexer, CPP_COMMA));
}

/* DECL_SPECIFIERS is the representation of a decl-specifier-seq.
   Returns TRUE iff `friend' appears among the DECL_SPECIFIERS.  */

static bool
cp_parser_friend_p (tree decl_specifiers)
{
  while (decl_specifiers)
    {
      /* See if this decl-specifier is `friend'.  */
      if (TREE_CODE (TREE_VALUE (decl_specifiers)) == IDENTIFIER_NODE
	  && C_RID_CODE (TREE_VALUE (decl_specifiers)) == RID_FRIEND)
	return true;

      /* Go on to the next decl-specifier.  */
      decl_specifiers = TREE_CHAIN (decl_specifiers);
    }

  return false;
}

/* DECL_SPECIFIERS is the representation of a decl-specifier-seq.
   Returns TRUE iff `typedef' appears among the DECL_SPECIFIERS.  */

static bool
cp_parser_typedef_p (tree decl_specifiers)
{
  while (decl_specifiers)
    {
      /* See if this decl-specifier is `typedef'.  */
      if (TREE_CODE (TREE_VALUE (decl_specifiers)) == IDENTIFIER_NODE
	  && C_RID_CODE (TREE_VALUE (decl_specifiers)) == RID_TYPEDEF)
	return true;

      /* Go on to the next decl-specifier.  */
      decl_specifiers = TREE_CHAIN (decl_specifiers);
    }

  return false;
}


/* If the next token is of the indicated TYPE, consume it.  Otherwise,
   issue an error message indicating that TOKEN_DESC was expected.
   
   Returns the token consumed, if the token had the appropriate type.
   Otherwise, returns NULL.  */

static cp_token *
cp_parser_require (cp_parser* parser,
                   enum cpp_ttype type,
                   const char* token_desc)
{
  if (cp_lexer_next_token_is (parser->lexer, type))
    return cp_lexer_consume_token (parser->lexer);
  else
    {
      /* Output the MESSAGE -- unless we're parsing tentatively.  */
      if (!cp_parser_simulate_error (parser))
	{
	  char *message = concat ("expected ", token_desc, NULL);
	  cp_parser_error (parser, message);
	  free (message);
	}
      return NULL;
    }
}

/* Like cp_parser_require, except that tokens will be skipped until
   the desired token is found.  An error message is still produced if
   the next token is not as expected.  */

static void
cp_parser_skip_until_found (cp_parser* parser, 
                            enum cpp_ttype type, 
                            const char* token_desc)
{
  cp_token *token;
  unsigned nesting_depth = 0;

  if (cp_parser_require (parser, type, token_desc))
    return;

  /* Skip tokens until the desired token is found.  */
  while (true)
    {
      /* Peek at the next token.  */
      token = cp_lexer_peek_token (parser->lexer);
      /* If we've reached the token we want, consume it and 
	 stop.  */
      if (token->type == type && !nesting_depth)
	{
	  cp_lexer_consume_token (parser->lexer);
	  return;
	}
      /* If we've run out of tokens, stop.  */
      if (token->type == CPP_EOF)
	return;
      if (token->type == CPP_OPEN_BRACE 
	  || token->type == CPP_OPEN_PAREN
	  || token->type == CPP_OPEN_SQUARE)
	++nesting_depth;
      else if (token->type == CPP_CLOSE_BRACE 
	       || token->type == CPP_CLOSE_PAREN
	       || token->type == CPP_CLOSE_SQUARE)
	{
	  if (nesting_depth-- == 0)
	    return;
	}
      /* Consume this token.  */
      cp_lexer_consume_token (parser->lexer);
    }
}

/* If the next token is the indicated keyword, consume it.  Otherwise,
   issue an error message indicating that TOKEN_DESC was expected.
   
   Returns the token consumed, if the token had the appropriate type.
   Otherwise, returns NULL.  */

static cp_token *
cp_parser_require_keyword (cp_parser* parser,
                           enum rid keyword,
                           const char* token_desc)
{
  cp_token *token = cp_parser_require (parser, CPP_KEYWORD, token_desc);

  if (token && token->keyword != keyword)
    {
      dyn_string_t error_msg;

      /* Format the error message.  */
      error_msg = dyn_string_new (0);
      dyn_string_append_cstr (error_msg, "expected ");
      dyn_string_append_cstr (error_msg, token_desc);
      cp_parser_error (parser, error_msg->s);
      dyn_string_delete (error_msg);
      return NULL;
    }

  return token;
}

/* Returns TRUE iff TOKEN is a token that can begin the body of a
   function-definition.  */

static bool 
cp_parser_token_starts_function_definition_p (cp_token* token)
{
  return (/* An ordinary function-body begins with an `{'.  */
	  token->type == CPP_OPEN_BRACE
	  /* A ctor-initializer begins with a `:'.  */
	  || token->type == CPP_COLON
	  /* A function-try-block begins with `try'.  */
	  || token->keyword == RID_TRY
	  /* The named return value extension begins with `return'.  */
	  || token->keyword == RID_RETURN);
}

/* Returns TRUE iff the next token is the ":" or "{" beginning a class
   definition.  */

static bool
cp_parser_next_token_starts_class_definition_p (cp_parser *parser)
{
  cp_token *token;

  token = cp_lexer_peek_token (parser->lexer);
  return (token->type == CPP_OPEN_BRACE || token->type == CPP_COLON);
}

/* Returns TRUE iff the next token is the "," or ">" ending a
   template-argument.   */

static bool
cp_parser_next_token_ends_template_argument_p (cp_parser *parser)
{
  cp_token *token;

  token = cp_lexer_peek_token (parser->lexer);
  return (token->type == CPP_COMMA || token->type == CPP_GREATER);
}

/* Returns TRUE iff the n-th token is a ">", or the n-th is a "[" and the
   (n+1)-th is a ":" (which is a possible digraph typo for "< ::").  */

static bool
cp_parser_nth_token_starts_template_argument_list_p (cp_parser * parser, 
						     size_t n)
{
  cp_token *token;

  token = cp_lexer_peek_nth_token (parser->lexer, n);
  if (token->type == CPP_LESS)
    return true;
  /* Check for the sequence `<::' in the original code. It would be lexed as
     `[:', where `[' is a digraph, and there is no whitespace before
     `:'.  */
  if (token->type == CPP_OPEN_SQUARE && token->flags & DIGRAPH)
    {
      cp_token *token2;
      token2 = cp_lexer_peek_nth_token (parser->lexer, n+1);
      if (token2->type == CPP_COLON && !(token2->flags & PREV_WHITE))
	return true;
    }
  return false;
}
 
/* Returns the kind of tag indicated by TOKEN, if it is a class-key,
   or none_type otherwise.  */

static enum tag_types
cp_parser_token_is_class_key (cp_token* token)
{
  switch (token->keyword)
    {
    case RID_CLASS:
      return class_type;
    case RID_STRUCT:
      return record_type;
    case RID_UNION:
      return union_type;
      
    default:
      return none_type;
    }
}

/* Issue an error message if the CLASS_KEY does not match the TYPE.  */

static void
cp_parser_check_class_key (enum tag_types class_key, tree type)
{
  if ((TREE_CODE (type) == UNION_TYPE) != (class_key == union_type))
    pedwarn ("`%s' tag used in naming `%#T'",
	    class_key == union_type ? "union"
	     : class_key == record_type ? "struct" : "class", 
	     type);
}
			   
/* Issue an error message if DECL is redeclared with different
   access than its original declaration [class.access.spec/3].
   This applies to nested classes and nested class templates.
   [class.mem/1].  */

static void cp_parser_check_access_in_redeclaration (tree decl)
{
  if (!CLASS_TYPE_P (TREE_TYPE (decl)))
    return;

  if ((TREE_PRIVATE (decl)
       != (current_access_specifier == access_private_node))
      || (TREE_PROTECTED (decl)
	  != (current_access_specifier == access_protected_node)))
    error ("%D redeclared with different access", decl);
}

/* Look for the `template' keyword, as a syntactic disambiguator.
   Return TRUE iff it is present, in which case it will be 
   consumed.  */

static bool
cp_parser_optional_template_keyword (cp_parser *parser)
{
  if (cp_lexer_next_token_is_keyword (parser->lexer, RID_TEMPLATE))
    {
      /* The `template' keyword can only be used within templates;
	 outside templates the parser can always figure out what is a
	 template and what is not.  */
      if (!processing_template_decl)
	{
	  error ("`template' (as a disambiguator) is only allowed "
		 "within templates");
	  /* If this part of the token stream is rescanned, the same
	     error message would be generated.  So, we purge the token
	     from the stream.  */
	  cp_lexer_purge_token (parser->lexer);
	  return false;
	}
      else
	{
	  /* Consume the `template' keyword.  */
	  cp_lexer_consume_token (parser->lexer);
	  return true;
	}
    }

  return false;
}

/* The next token is a CPP_NESTED_NAME_SPECIFIER.  Consume the token,
   set PARSER->SCOPE, and perform other related actions.  */

static void
cp_parser_pre_parsed_nested_name_specifier (cp_parser *parser)
{
  tree value;
  tree check;

  /* Get the stored value.  */
  value = cp_lexer_consume_token (parser->lexer)->value;
  /* Perform any access checks that were deferred.  */
  for (check = TREE_PURPOSE (value); check; check = TREE_CHAIN (check))
    perform_or_defer_access_check (TREE_PURPOSE (check), TREE_VALUE (check));
  /* Set the scope from the stored value.  */
  parser->scope = TREE_VALUE (value);
  parser->qualifying_scope = TREE_TYPE (value);
  parser->object_scope = NULL_TREE;
}

/* Add tokens to CACHE until an non-nested END token appears.  */

static void
cp_parser_cache_group (cp_parser *parser, 
		       cp_token_cache *cache,
		       enum cpp_ttype end,
		       unsigned depth)
{
  while (true)
    {
      cp_token *token;

      /* Abort a parenthesized expression if we encounter a brace.  */
      if ((end == CPP_CLOSE_PAREN || depth == 0)
	  && cp_lexer_next_token_is (parser->lexer, CPP_SEMICOLON))
	return;
      /* If we've reached the end of the file, stop.  */
      if (cp_lexer_next_token_is (parser->lexer, CPP_EOF))
	return;
      /* Consume the next token.  */
      token = cp_lexer_consume_token (parser->lexer);
      /* Add this token to the tokens we are saving.  */
      cp_token_cache_push_token (cache, token);
      /* See if it starts a new group.  */
      if (token->type == CPP_OPEN_BRACE)
	{
	  cp_parser_cache_group (parser, cache, CPP_CLOSE_BRACE, depth + 1);
	  if (depth == 0)
	    return;
	}
      else if (token->type == CPP_OPEN_PAREN)
	cp_parser_cache_group (parser, cache, CPP_CLOSE_PAREN, depth + 1);
      else if (token->type == end)
	return;
    }
}

/* Begin parsing tentatively.  We always save tokens while parsing
   tentatively so that if the tentative parsing fails we can restore the
   tokens.  */

static void
cp_parser_parse_tentatively (cp_parser* parser)
{
  /* Enter a new parsing context.  */
  parser->context = cp_parser_context_new (parser->context);
  /* Begin saving tokens.  */
  cp_lexer_save_tokens (parser->lexer);
  /* In order to avoid repetitive access control error messages,
     access checks are queued up until we are no longer parsing
     tentatively.  */
  push_deferring_access_checks (dk_deferred);
}

/* Commit to the currently active tentative parse.  */

static void
cp_parser_commit_to_tentative_parse (cp_parser* parser)
{
  cp_parser_context *context;
  cp_lexer *lexer;

  /* Mark all of the levels as committed.  */
  lexer = parser->lexer;
  for (context = parser->context; context->next; context = context->next)
    {
      if (context->status == CP_PARSER_STATUS_KIND_COMMITTED)
	break;
      context->status = CP_PARSER_STATUS_KIND_COMMITTED;
      while (!cp_lexer_saving_tokens (lexer))
	lexer = lexer->next;
      cp_lexer_commit_tokens (lexer);
    }
}

/* Abort the currently active tentative parse.  All consumed tokens
   will be rolled back, and no diagnostics will be issued.  */

static void
cp_parser_abort_tentative_parse (cp_parser* parser)
{
  cp_parser_simulate_error (parser);
  /* Now, pretend that we want to see if the construct was
     successfully parsed.  */
  cp_parser_parse_definitely (parser);
}

/* Stop parsing tentatively.  If a parse error has occurred, restore the
   token stream.  Otherwise, commit to the tokens we have consumed.
   Returns true if no error occurred; false otherwise.  */

static bool
cp_parser_parse_definitely (cp_parser* parser)
{
  bool error_occurred;
  cp_parser_context *context;

  /* Remember whether or not an error occurred, since we are about to
     destroy that information.  */
  error_occurred = cp_parser_error_occurred (parser);
  /* Remove the topmost context from the stack.  */
  context = parser->context;
  parser->context = context->next;
  /* If no parse errors occurred, commit to the tentative parse.  */
  if (!error_occurred)
    {
      /* Commit to the tokens read tentatively, unless that was
	 already done.  */
      if (context->status != CP_PARSER_STATUS_KIND_COMMITTED)
	cp_lexer_commit_tokens (parser->lexer);

      pop_to_parent_deferring_access_checks ();
    }
  /* Otherwise, if errors occurred, roll back our state so that things
     are just as they were before we began the tentative parse.  */
  else
    {
      cp_lexer_rollback_tokens (parser->lexer);
      pop_deferring_access_checks ();
    }
  /* Add the context to the front of the free list.  */
  context->next = cp_parser_context_free_list;
  cp_parser_context_free_list = context;

  return !error_occurred;
}

/* Returns true if we are parsing tentatively -- but have decided that
   we will stick with this tentative parse, even if errors occur.  */

static bool
cp_parser_committed_to_tentative_parse (cp_parser* parser)
{
  return (cp_parser_parsing_tentatively (parser)
	  && parser->context->status == CP_PARSER_STATUS_KIND_COMMITTED);
}

/* Returns nonzero iff an error has occurred during the most recent
   tentative parse.  */
   
static bool
cp_parser_error_occurred (cp_parser* parser)
{
  return (cp_parser_parsing_tentatively (parser)
	  && parser->context->status == CP_PARSER_STATUS_KIND_ERROR);
}

/* Returns nonzero if GNU extensions are allowed.  */

static bool
cp_parser_allow_gnu_extensions_p (cp_parser* parser)
{
  return parser->allow_gnu_extensions_p;
}



/* The parser.  */

static GTY (()) cp_parser *the_parser;

/* External interface.  */

/* Parse one entire translation unit.  */

void
c_parse_file (void)
{
  bool error_occurred;

  the_parser = cp_parser_new ();
  push_deferring_access_checks (flag_access_control
				? dk_no_deferred : dk_no_check);
  error_occurred = cp_parser_translation_unit (the_parser);
  the_parser = NULL;
}

/* This variable must be provided by every front end.  */

int yydebug;

#include "gt-cp-parser.h"
