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
 *	$FreeBSD: release/7.0.0/lib/libgssapi/gss_inquire_context.c 153838 2005-12-29 14:40:22Z dfr $
 */

#include <gssapi/gssapi.h>

#include "mech_switch.h"
#include "context.h"
#include "name.h"

OM_uint32
gss_inquire_context(OM_uint32 *minor_status,
    const gss_ctx_id_t context_handle,
    gss_name_t *src_name,
    gss_name_t *targ_name,
    OM_uint32 *lifetime_rec,
    gss_OID *mech_type,
    OM_uint32 *ctx_flags,
    int *locally_initiated,
    int *open)
{
	OM_uint32 major_status;
	struct _gss_context *ctx = (struct _gss_context *) context_handle;
	struct _gss_mech_switch *m = ctx->gc_mech;
	struct _gss_name *name;
	gss_name_t src_mn, targ_mn;

	major_status = m->gm_inquire_context(minor_status,
	    ctx->gc_ctx,
	    src_name ? &src_mn : 0,
	    targ_name ? &targ_mn : 0,
	    lifetime_rec,
	    mech_type,
	    ctx_flags,
	    locally_initiated,
	    open);

	if (src_name) *src_name = 0;
	if (targ_name) *targ_name = 0;

	if (major_status != GSS_S_COMPLETE) {
		return (major_status);
	}

	if (src_name) {
		name = _gss_make_name(m, src_mn);
		if (!name) {
			minor_status = 0;
			return (GSS_S_FAILURE);
		}
		*src_name = (gss_name_t) name;
	}

	if (targ_name) {
		name = _gss_make_name(m, targ_mn);
		if (!name) {
			minor_status = 0;
			return (GSS_S_FAILURE);
		}
		*targ_name = (gss_name_t) name;
	}

	return (GSS_S_COMPLETE);
}
