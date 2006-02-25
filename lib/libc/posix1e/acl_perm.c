/*
 * Copyright (c) 2001-2002 Chris D. Faulhaber
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR THE VOICES IN HIS HEAD BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/posix1e/acl_perm.c,v 1.6 2002/03/22 21:52:38 obrien Exp $");

#include <sys/types.h>
#include "namespace.h"
#include <sys/acl.h>
#include "un-namespace.h"

#include <errno.h>
#include <string.h>

/*
 * acl_add_perm() (23.4.1): add the permission contained in perm to the
 * permission set permset_d
 */
int
acl_add_perm(acl_permset_t permset_d, acl_perm_t perm)
{

	if (permset_d) {
		switch(perm) {
		case ACL_READ:
		case ACL_WRITE:
		case ACL_EXECUTE:
			*permset_d |= perm;
			return (0);
		}
	}

	errno = EINVAL;
	return (-1);
}

/*
 * acl_clear_perms() (23.4.3): clear all permisions from the permission
 * set permset_d
 */
int
acl_clear_perms(acl_permset_t permset_d)
{

	if (permset_d == NULL) {
		errno = EINVAL;
		return (-1);
	}

	*permset_d = ACL_PERM_NONE;

	return (0);
}

/*
 * acl_delete_perm() (23.4.10): remove the permission in perm from the
 * permission set permset_d
 */
int
acl_delete_perm(acl_permset_t permset_d, acl_perm_t perm)
{

	if (permset_d) {
		switch(perm) {
		case ACL_READ:
		case ACL_WRITE:
		case ACL_EXECUTE:
			*permset_d &= ~(perm & ACL_PERM_BITS);
			return (0);
		}
	}

	errno = EINVAL;
	return (-1);
}
