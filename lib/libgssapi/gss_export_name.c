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
 *	$FreeBSD: release/7.0.0/lib/libgssapi/gss_export_name.c 171112 2007-06-30 07:47:45Z dfr $
 */

#include <gssapi/gssapi.h>

#include "mech_switch.h"
#include "name.h"

OM_uint32
gss_export_name(OM_uint32 *minor_status,
    const gss_name_t input_name,
    gss_buffer_t exported_name)
{
	struct _gss_name *name = (struct _gss_name *) input_name;
	struct _gss_mechanism_name *mn;

	exported_name->value = NULL;
	exported_name->length = 0;

	/*
	 * If this name already has any attached MNs, export the first
	 * one, otherwise export based on the first mechanism in our
	 * list.
	 */
	mn = SLIST_FIRST(&name->gn_mn);
	if (!mn)
		mn = _gss_find_mn(name,
		    &SLIST_FIRST(&_gss_mechs)->gm_mech_oid);
	if (!mn) {
		*minor_status = 0;
		return (GSS_S_BAD_MECH);
	}

	return mn->gmn_mech->gm_export_name(minor_status,
	    mn->gmn_name, exported_name);
}
