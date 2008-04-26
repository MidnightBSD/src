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
 * $MidnightBSD: src/lib/libmport/error.c,v 1.6 2008/01/05 22:18:20 ctriv Exp $
 */


#include "mport.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>


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
  "System call failed.",
  "libarchive error.",
  "Package already installed.",
  "Package conflicts with priviously installed package.",
  "A depend is missing.",
  "Malformed version.",
  "Malformed depend.",
  "No such package.",
  "Checksum mismatch.",
  "Packages depend on this package."
};
  

int mport_err_code() 
{
  return err;
}

const char * mport_err_string()
{
  return err_msg;
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

int mport_set_errx(int code, const char *fmt, ...) 
{
    va_list args;
    char *err;
    int ret;

    va_start(args, fmt);
    if (vasprintf(&err, fmt, args) == -1) {
	fprintf(stderr, "fatal error: mport_set_errx can't format the string.\n");
	exit(255);
    }
    ret = mport_set_err(code, err);
    
    free(err);
    
    va_end(args);
    
    return ret;
}
