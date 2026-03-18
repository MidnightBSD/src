/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Lucas Holt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.	IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <libutil.h>
#include "mport.h"
#include "mport_private.h"

bool 
mport_is_age_verified(mportInstance *mport, mportPackageMeta *pack) 
{

#if defined(__MidnightBSD__) && __MidnightBSD_version >= 400004    
    char *age_str = NULL;
    int age = 0;

    if (mport_annotation_get(mport, pack->origin, "age", &age_str) != MPORT_OK) {
        mport_call_msg_cb(mport, "Failed to get age annotation for package %s: %s", pack->origin, mport_err_string());
        return false;
    }

    if (age_str == NULL) {
        return true; /* No age annotation means it's appropriate for all ages */
    }

    age = atoi(age_str);
    free(age_str);

    const char *username = getlogin();
    if (username == NULL) {
        mport_call_msg_cb(mport, "Could not determine username to verify age: %s", strerror(errno));
        return false;
    }

    int *user_age_bracket = agev_get_age_bracket(username);
    if (user_age_bracket == NULL) {
        mport_call_msg_cb(mport, "Could not determine user age bracket.");
        return false;
    }

    bool verified = false;
    if (user_age_bracket[0] >= 18 && user_age_bracket[1] == -1) {
        verified = true;
    } else {
        verified = age <= user_age_bracket[1];
    }

    free(user_age_bracket);

    return verified;
#else
    return true;
#endif
}
