/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
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
 */

#include <gssapi/gssapi.h>
#include <stdlib.h>
#include <errno.h>

#include "mech_switch.h"
#include "context.h"
#include "utils.h"

OM_uint32
gss_delete_sec_context(OM_uint32 *minor_status,
    gss_ctx_id_t *context_handle,
    gss_buffer_t output_token)
{
	OM_uint32 major_status;
	struct _gss_context *ctx = (struct _gss_context *) *context_handle;

	if (output_token)
		_gss_buffer_zero(output_token);

	*minor_status = 0;
	if (ctx) {
		/*
		 * If we have an implementation ctx, delete it,
		 * otherwise fake an empty token.
		 */
		if (ctx->gc_ctx) {
			major_status = ctx->gc_mech->gm_delete_sec_context(
				minor_status, &ctx->gc_ctx, output_token);
		}
		free(ctx);
		*context_handle = GSS_C_NO_CONTEXT;
	}

	return (GSS_S_COMPLETE);
}
