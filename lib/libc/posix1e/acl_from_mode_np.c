/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Robert N M Watson, Gleb Popov
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
/*
 * acl_from_mode_np: Create an ACL from a mode_t.
 */

#include <sys/types.h>
#include <sys/acl.h>
#include <sys/stat.h>

#include <errno.h>

acl_t
acl_from_mode_np(mode_t mode)
{
	static const struct {
		acl_tag_t tag;
		mode_t read_mask;
		mode_t write_mask;
		mode_t execute_mask;
	} masks[] = {
		{ ACL_USER_OBJ, S_IRUSR, S_IWUSR, S_IXUSR },
		{ ACL_GROUP_OBJ, S_IRGRP, S_IWGRP, S_IXGRP },
		{ ACL_OTHER, S_IROTH, S_IWOTH, S_IXOTH },
	};
	acl_t acl;
	acl_entry_t entry;
	acl_permset_t perms;
	size_t i;
	int error;

	acl = acl_init(sizeof(masks) / sizeof(masks[0]));
	if (acl == NULL)
		return (NULL);
	for (i = 0; i < sizeof(masks) / sizeof(masks[0]); i++) {
		if (acl_create_entry(&acl, &entry) == -1 ||
		    acl_set_tag_type(entry, masks[i].tag) == -1 ||
		    acl_get_permset(entry, &perms) == -1 ||
		    acl_clear_perms(perms) == -1)
			goto fail;
		if (((mode & masks[i].read_mask) != 0 &&
		    acl_add_perm(perms, ACL_READ) == -1) ||
		    ((mode & masks[i].write_mask) != 0 &&
		    acl_add_perm(perms, ACL_WRITE) == -1) ||
		    ((mode & masks[i].execute_mask) != 0 &&
		    acl_add_perm(perms, ACL_EXECUTE) == -1))
			goto fail;
	}
	return (acl);
fail:
	error = errno;
	acl_free(acl);
	errno = error;
	return (NULL);
}
