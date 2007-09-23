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
 * $MidnightBSD$
 */


#include "mport.h"
#include <stdlib.h>
#include <string.h>

__MBSDID("$MidnightBSD: src/usr.sbin/pkg_install/lib/plist.c,v 1.50.2.1 2006/01/10 22:15:06 krion Exp $");

static int err;
static char err_msg[256];

/* This goes with the error codes in mport.h */
static char *mport_err_defaults[] = {
  NULL,
  "Out of memory.",
  "File I/O Error.",
  "Malformed packing list.",
  "SQLite error.",
  "File not found."
};
  

int mport_err_code() 
{
  return err;
}

char * mport_err_string()
{
  size_t len = strlen(err_msg);
  char *copy = (char *)malloc(len + 1);
  
  if (copy == NULL) {
    fprintf(stderr, "Fatal error: unable to allocate memory for error string: %s\n", err_msg);
    exit(255);
  }

  strlcpy(copy, err_msg, len + 1);
  
  return copy;
}

int mport_set_err(int code, const char *msg) 
{
  err = code;
  if (msg != NULL) {
    strlcpy(err_msg, msg, sizeof(err_msg));
  } else {
    strlcpy(err_msg, mport_err_defaults[code], sizeof(mport_err_defaults[code]));
  }
  return code;
}

