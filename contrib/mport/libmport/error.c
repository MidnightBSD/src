/*-
 * Copyright (c) 2007-2009 Chris Reinhardt
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
 */

#include "mport.h"
#include "mport_private.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static int mport_err;
static char err_msg[256];

/* This goes with the error codes in mport.h */
static char default_error_msg[] = "An error occurred.";


/* mport_err_code()
 *
 * Return the current numeric error code. 
 */
MPORT_PUBLIC_API int
mport_err_code(void) {
    return mport_err;
}

/* mport_err_string()
 *
 * Return the current error string (if any).  Do not free this memory, it is static. 
 */
MPORT_PUBLIC_API const char *
mport_err_string(void) {
    return err_msg;
}


/* In general, don't use these - use the macros in mport_private.h */

/* mport_set_error(code, msg)
 *
 * Set an error condition, with the given code and message.  A default message will
 * be used if msg is NULL
 */
int
mport_set_err(int code, const char *msg) {
    mport_err = code;

    if (code == MPORT_OK) {
        bzero(err_msg, sizeof(err_msg));
    } else {
        if (msg != NULL) {
            strlcpy(err_msg, msg, sizeof(err_msg));
        } else {
            strlcpy(err_msg, default_error_msg, sizeof(err_msg));
        }
    }
    return code;
}


/* mport_set_errx(code, fmt, arg1, arg2, ...)
 *
 * Like mport_set_error, but it has a printf() type formatting syntax.  
 * There is no way to access the default error messages with this function,
 * use mport_set_err() for that.
 */
int
mport_set_errx(int code, const char *fmt, ...) {
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
