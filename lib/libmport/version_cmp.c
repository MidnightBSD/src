/*-
 * Copyright (c) 2007 Chris Reinhardt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $MidnightBSD: src/lib/libmport/version_cmp.c,v 1.2 2007/11/26 21:41:56 ctriv Exp $
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include "mport.h"

struct version {
  char *version;
  int revision;
  int epoch;
};

static void parse_version(const char *, struct version *);
static int cmp_versions(char *, char *);
static int cmp_ints(int, int);

int mport_version_cmp(const char *astr, const char *bstr)
{
  struct version a;
  struct version b;
  int result;
  
  parse_version(astr, &a);
  parse_version(bstr, &b);

  /* remember that a.version/b.version are useless after calling
     cmp_versions (but astr and bstr are unchanged.) */
  if ((result = cmp_ints(a.epoch, b.epoch)) == 0) {
    if ((result = cmp_versions(a.version, b.version)) == 0) {
      result = cmp_ints(a.revision, b.revision);
    }
  }
  
  free(a.version);
  free(b.version);
  
  return result;
}

void mport_version_cmp_sqlite(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  char *a, *b;

  assert(argc == 2);
  
  a = strdup(sqlite3_value_text(argv[0]));
  b = strdup(sqlite3_value_text(argv[1]));
  
  assert(a != NULL);
  assert(b != NULL);
  
  sqlite3_result_int(context, mport_version_cmp(a, b));
  
  free(a);
  free(b);
}  

static void parse_version(const char *in, struct version *v) 
{
  char *s = strdup(in);
  char *underscore;
  char *comma;
  
  underscore = rindex(s, '_');
  comma      = rindex(s, ',');
  
  if (comma == NULL) {
    v->epoch = 0;
  } else {
    *comma = '\0';
    v->epoch = (int)strtol(comma + 1, NULL, 10);
  }
  
  if (underscore == NULL) {
    v->revision = 0;
  } else {
    *underscore = '\0';
    v->revision = (int)strtol(underscore + 1, NULL, 10);
  }
  
  v->version = s;
}

static int cmp_ints(int a, int b) 
{
  if (a == b)
    return 0;
  if (a < b)
    return -1;
    
  return 1;
}

static int cmp_versions(char *a, char *b)
{
  int a_sub, b_sub, result;

  while (*a || *b) {
    if (*a) {
      while (*a == '.' || *a == '+') 
        a++;
        
      if (isdigit(*a)) {
        a_sub  = (int)strtol(a, &a, 10);
      } else {
        a_sub  = (int)*a;
        a++;
      }
    } else {
      a_sub = 0;
    }
    
    if (*b) {
      while (*b == '.' || *b == '+')
        b++;
        
      if (isdigit(*b)) { 
        b_sub = (int)strtol(b, &b, 10);
      } else {
        b_sub = (int)*b;
        b++;
      }
    } else {
      b_sub = 0;
    }

    result = cmp_ints(a_sub, b_sub);
    
    if (result != 0) 
      break;
  }
    
  
  return result;
}    
