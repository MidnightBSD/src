/*-
 * Copyright (c) 2005 Doug Rabson
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
 *	$FreeBSD: release/7.0.0/lib/libgssapi/gss_utils.c 168338 2007-04-04 02:28:48Z kan $
 */

#include <gssapi/gssapi.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "utils.h"

int
_gss_oid_equal(const gss_OID oid1, const gss_OID oid2)
{
	if (oid1->length != oid2->length)
		return (0);
	if (memcmp(oid1->elements, oid2->elements, oid1->length))
		return (0);
	return (1);
}

OM_uint32
_gss_copy_oid(OM_uint32 *minor_status,
    const gss_OID from_oid, gss_OID to_oid)
{
	size_t len = from_oid->length;

	*minor_status = 0;
	to_oid->elements = malloc(len);
	if (!to_oid->elements) {
		*minor_status = ENOMEM;
		return GSS_S_FAILURE;
	}
	to_oid->length = len;
	memcpy(to_oid->elements, from_oid->elements, len);
	return (GSS_S_COMPLETE);
}


OM_uint32
_gss_copy_buffer(OM_uint32 *minor_status,
    const gss_buffer_t from_buf, gss_buffer_t to_buf)
{
	size_t len = from_buf->length;

	*minor_status = 0;
	to_buf->value = malloc(len);
	if (!to_buf->value) {
		*minor_status = ENOMEM;
		return GSS_S_FAILURE;
	}
	to_buf->length = len;
	memcpy(to_buf->value, from_buf->value, len);
	return (GSS_S_COMPLETE);
}

