/*
 * Copyright (c) 1997 - 2003 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "gssapi_locl.h"

RCSID("$Id: context_time.c,v 1.7.2.1 2003/08/15 14:25:50 lha Exp $");

OM_uint32
gssapi_lifetime_left(OM_uint32 *minor_status, 
		     OM_uint32 lifetime,
		     OM_uint32 *lifetime_rec)
{
    krb5_timestamp timeret;
    krb5_error_code kret;

    kret = krb5_timeofday(gssapi_krb5_context, &timeret);
    if (kret) {
	*minor_status = kret;
	gssapi_krb5_set_error_string ();
	return GSS_S_FAILURE;
    }

    if (lifetime < timeret) 
	*lifetime_rec = 0;
    else
	*lifetime_rec = lifetime - timeret;

    return GSS_S_COMPLETE;
}


OM_uint32 gss_context_time
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            OM_uint32 * time_rec
           )
{
    OM_uint32 lifetime;
    OM_uint32 major_status;

    GSSAPI_KRB5_INIT ();

    lifetime = context_handle->lifetime;

    major_status = gssapi_lifetime_left(minor_status, lifetime, time_rec);
    if (major_status != GSS_S_COMPLETE)
	return major_status;

    *minor_status = 0;

    if (*time_rec == 0)
	return GSS_S_CONTEXT_EXPIRED;
	
    return GSS_S_COMPLETE;
}
