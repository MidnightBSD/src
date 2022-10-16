/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2015 Lucas Holt
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "mport.h"
#include "mport_private.h"

MPORT_PUBLIC_API int
mport_lock_lock(mportInstance *mport, mportPackageMeta *pkg)
{

	/* we are already locked, just return */
	if (mport_lock_islocked(pkg) == MPORT_LOCKED) {
		return (MPORT_OK);
	}

	if (mport_db_do(mport->db, "update packages set locked=1 where pkg=%Q", pkg->name) != MPORT_OK) {
		RETURN_CURRENT_ERROR;
	}

	return (MPORT_OK);
}

MPORT_PUBLIC_API int
mport_lock_unlock(mportInstance *mport, mportPackageMeta *pkg)
{
	if (mport_lock_islocked(pkg) == MPORT_LOCKED) {
		if (mport_db_do(mport->db, "update packages set locked=0 where pkg=%Q", pkg->name) != MPORT_OK) {
			RETURN_CURRENT_ERROR;
		}
	}

	return (MPORT_OK);
}

/*
 * check if a pkg is locked. We don't want someone to upgrade a package or remove it if it's locked.
 * MPORT_LOCKED is returned for an unknown package or a locked package as we can't do anything.
 */
MPORT_PUBLIC_API int
mport_lock_islocked(mportPackageMeta *pkg)
{
	if (pkg == NULL)
		return MPORT_LOCKED;

	if (pkg->locked == 1)
		return MPORT_LOCKED;

	return MPORT_UNLOCKED;
}
