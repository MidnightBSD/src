/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Lucas Holt
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

#include <sys/cdefs.h>

#include "mport.h"
#include "mport_private.h"

MPORT_PUBLIC_API int
mport_autoremove(mportInstance *mport) {
    mportPackageMeta **packs, **packs_start;
    mportPackageMeta **depends, **depends_start;

    if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
        RETURN_CURRENT_ERROR;
    }

    if (packs == NULL)
        return MPORT_OK;

    packs_start = packs;
    while (*packs != NULL) {
        if ((*packs)->automatic == MPORT_EXPLICIT) {
            packs++;
            continue;
        }

        if (mport_pkgmeta_get_updepends(mport, *packs, &depends) != MPORT_OK) {
            mport_call_msg_cb(mport, "Unable to evaluate package %s: %s", (*packs)->name, mport_err_string());
            packs++;
            continue;
        }

        if (depends == NULL) {
            packs++;
            continue;
        }

        depends_start = depends;
        bool found = false;
        while (*depends != NULL) {
            if ((*depends)->automatic == MPORT_EXPLICIT) {
                found = true;
                break;
            }
            depends++;
        }

        if (found) {
            packs++;
            continue;
        }

        depends = depends_start;
        while ((*depends) != NULL) {
            mport_call_msg_cb(mport, "Auto-removing %s", (*depends)->name);
            (*depends)->action = MPORT_ACTION_DELETE;
            if (mport_delete_primative(mport, *depends, true) != MPORT_OK) {
                mport_call_msg_cb(mport, "Unable to autoremove %s: %s", (*depends)->name, mport_err_string());
                continue;
            }

            depends++;
        }
        mport_pkgmeta_vec_free(depends_start);

        packs++;
    }
    mport_pkgmeta_vec_free(packs_start);

    return MPORT_OK;
}