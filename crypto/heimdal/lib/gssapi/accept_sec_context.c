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

RCSID("$Id: accept_sec_context.c,v 1.33.2.2 2003/12/19 00:37:06 lha Exp $");

krb5_keytab gssapi_krb5_keytab;

OM_uint32
gsskrb5_register_acceptor_identity (const char *identity)
{
    krb5_error_code ret;
    char *p;

    ret = gssapi_krb5_init();
    if(ret)
	return GSS_S_FAILURE;
    
    if(gssapi_krb5_keytab != NULL) {
	krb5_kt_close(gssapi_krb5_context, gssapi_krb5_keytab);
	gssapi_krb5_keytab = NULL;
    }
    asprintf(&p, "FILE:%s", identity);
    if(p == NULL)
	return GSS_S_FAILURE;
    ret = krb5_kt_resolve(gssapi_krb5_context, p, &gssapi_krb5_keytab);
    free(p);
    if(ret)
	return GSS_S_FAILURE;
    return GSS_S_COMPLETE;
}

OM_uint32
gss_accept_sec_context
           (OM_uint32 * minor_status,
            gss_ctx_id_t * context_handle,
            const gss_cred_id_t acceptor_cred_handle,
            const gss_buffer_t input_token_buffer,
            const gss_channel_bindings_t input_chan_bindings,
            gss_name_t * src_name,
            gss_OID * mech_type,
            gss_buffer_t output_token,
            OM_uint32 * ret_flags,
            OM_uint32 * time_rec,
            gss_cred_id_t * delegated_cred_handle
           )
{
    krb5_error_code kret;
    OM_uint32 ret = GSS_S_COMPLETE;
    krb5_data indata;
    krb5_flags ap_options;
    OM_uint32 flags;
    krb5_ticket *ticket = NULL;
    krb5_keytab keytab = NULL;
    krb5_data fwd_data;
    OM_uint32 minor;

    GSSAPI_KRB5_INIT();

    krb5_data_zero (&fwd_data);
    output_token->length = 0;
    output_token->value   = NULL;

    if (src_name != NULL)
	*src_name = NULL;
    if (mech_type)
	*mech_type = GSS_KRB5_MECHANISM;

    if (*context_handle == GSS_C_NO_CONTEXT) {
	*context_handle = malloc(sizeof(**context_handle));
	if (*context_handle == GSS_C_NO_CONTEXT) {
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}
    }

    (*context_handle)->auth_context =  NULL;
    (*context_handle)->source = NULL;
    (*context_handle)->target = NULL;
    (*context_handle)->flags = 0;
    (*context_handle)->more_flags = 0;
    (*context_handle)->ticket = NULL;
    (*context_handle)->lifetime = GSS_C_INDEFINITE;

    kret = krb5_auth_con_init (gssapi_krb5_context,
			       &(*context_handle)->auth_context);
    if (kret) {
	ret = GSS_S_FAILURE;
	*minor_status = kret;
	gssapi_krb5_set_error_string ();
	goto failure;
    }

    if (input_chan_bindings != GSS_C_NO_CHANNEL_BINDINGS
	&& input_chan_bindings->application_data.length ==
	2 * sizeof((*context_handle)->auth_context->local_port)
	) {
     
	/* Port numbers are expected to be in application_data.value,
	 * initator's port first */
     
	krb5_address initiator_addr, acceptor_addr;
     
	memset(&initiator_addr, 0, sizeof(initiator_addr));
	memset(&acceptor_addr, 0, sizeof(acceptor_addr));

	(*context_handle)->auth_context->remote_port = 
	    *(int16_t *) input_chan_bindings->application_data.value; 
     
	(*context_handle)->auth_context->local_port =
	    *((int16_t *) input_chan_bindings->application_data.value + 1);

     
	kret = gss_address_to_krb5addr(input_chan_bindings->acceptor_addrtype,
				       &input_chan_bindings->acceptor_address,
				       (*context_handle)->auth_context->local_port,
				       &acceptor_addr); 
	if (kret) {
	    gssapi_krb5_set_error_string ();
	    ret = GSS_S_BAD_BINDINGS;
	    *minor_status = kret;
	    goto failure;
	}
                             
	kret = gss_address_to_krb5addr(input_chan_bindings->initiator_addrtype,
				       &input_chan_bindings->initiator_address, 
				       (*context_handle)->auth_context->remote_port,
				       &initiator_addr); 
	if (kret) {
	    krb5_free_address (gssapi_krb5_context, &acceptor_addr);
	    gssapi_krb5_set_error_string ();
	    ret = GSS_S_BAD_BINDINGS;
	    *minor_status = kret;
	    goto failure;
	}
     
	kret = krb5_auth_con_setaddrs(gssapi_krb5_context,
				      (*context_handle)->auth_context,
				      &acceptor_addr,    /* local address */
				      &initiator_addr);  /* remote address */
     
	krb5_free_address (gssapi_krb5_context, &initiator_addr);
	krb5_free_address (gssapi_krb5_context, &acceptor_addr);
     
#if 0
	free(input_chan_bindings->application_data.value);
	input_chan_bindings->application_data.value = NULL;
	input_chan_bindings->application_data.length = 0;
#endif
     
	if (kret) {
	    gssapi_krb5_set_error_string ();
	    ret = GSS_S_BAD_BINDINGS;
	    *minor_status = kret;
	    goto failure;
	}
    }
  


    {
	int32_t tmp;

	krb5_auth_con_getflags(gssapi_krb5_context,
			       (*context_handle)->auth_context,
			       &tmp);
	tmp |= KRB5_AUTH_CONTEXT_DO_SEQUENCE;
	krb5_auth_con_setflags(gssapi_krb5_context,
			       (*context_handle)->auth_context,
			       tmp);
    }

    ret = gssapi_krb5_decapsulate (minor_status,
				   input_token_buffer,
				   &indata,
				   "\x01\x00");
    if (ret)
	goto failure;

    if (acceptor_cred_handle == GSS_C_NO_CREDENTIAL) {
	if (gssapi_krb5_keytab != NULL) {
	    keytab = gssapi_krb5_keytab;
	}
    } else if (acceptor_cred_handle->keytab != NULL) {
	keytab = acceptor_cred_handle->keytab;
    }

    kret = krb5_rd_req (gssapi_krb5_context,
			&(*context_handle)->auth_context,
			&indata,
			(acceptor_cred_handle == GSS_C_NO_CREDENTIAL) ? NULL 
			: acceptor_cred_handle->principal,
			keytab,
			&ap_options,
			&ticket);
    if (kret) {
	ret = GSS_S_FAILURE;
	*minor_status = kret;
	gssapi_krb5_set_error_string ();
	goto failure;
    }

    kret = krb5_copy_principal (gssapi_krb5_context,
				ticket->client,
				&(*context_handle)->source);
    if (kret) {
	ret = GSS_S_FAILURE;
	*minor_status = kret;
	gssapi_krb5_set_error_string ();
	goto failure;
    }

    kret = krb5_copy_principal (gssapi_krb5_context,
				ticket->server,
				&(*context_handle)->target);
    if (kret) {
	ret = GSS_S_FAILURE;
	*minor_status = kret;
	gssapi_krb5_set_error_string ();
	goto failure;
    }

    ret = _gss_DES3_get_mic_compat(minor_status, *context_handle);
    if (ret)
	goto failure;

    if (src_name != NULL) {
	kret = krb5_copy_principal (gssapi_krb5_context,
				    ticket->client,
				    src_name);
	if (kret) {
	    ret = GSS_S_FAILURE;
	    *minor_status = kret;
	    gssapi_krb5_set_error_string ();
	    goto failure;
	}
    }

    {
	krb5_authenticator authenticator;
      
	kret = krb5_auth_con_getauthenticator(gssapi_krb5_context,
					      (*context_handle)->auth_context,
					      &authenticator);
	if(kret) {
	    ret = GSS_S_FAILURE;
	    *minor_status = kret;
	    gssapi_krb5_set_error_string ();
	    goto failure;
	}

	ret = gssapi_krb5_verify_8003_checksum(minor_status,
					       input_chan_bindings,
					       authenticator->cksum,
					       &flags,
					       &fwd_data);
	krb5_free_authenticator(gssapi_krb5_context, &authenticator);
	if (ret)
	    goto failure;
    }

    if (fwd_data.length > 0 && (flags & GSS_C_DELEG_FLAG)) {
	krb5_ccache ccache;
	int32_t ac_flags;
      
	if (delegated_cred_handle == NULL)
	    /* XXX Create a new delegated_cred_handle? */
	    kret = krb5_cc_default (gssapi_krb5_context, &ccache);
	else if (*delegated_cred_handle == NULL) {
	    if ((*delegated_cred_handle =
		 calloc(1, sizeof(**delegated_cred_handle))) == NULL) {
		ret = GSS_S_FAILURE;
		*minor_status = ENOMEM;
		krb5_set_error_string(gssapi_krb5_context, "out of memory");
		gssapi_krb5_set_error_string();
		goto failure;
	    }
	    if ((ret = gss_duplicate_name(minor_status, ticket->client,
					  &(*delegated_cred_handle)->principal)) != 0) {
		flags &= ~GSS_C_DELEG_FLAG;
		free(*delegated_cred_handle);
		*delegated_cred_handle = NULL;
		goto end_fwd;
	    }
	}
	if (delegated_cred_handle != NULL &&
	    (*delegated_cred_handle)->ccache == NULL) {
            kret = krb5_cc_gen_new (gssapi_krb5_context,
                                    &krb5_mcc_ops,
                                    &(*delegated_cred_handle)->ccache);
	    ccache = (*delegated_cred_handle)->ccache;
	}
	if (delegated_cred_handle != NULL &&
	    (*delegated_cred_handle)->mechanisms == NULL) {
	    ret = gss_create_empty_oid_set(minor_status, 
					   &(*delegated_cred_handle)->mechanisms);
            if (ret)
		goto failure;
	    ret = gss_add_oid_set_member(minor_status, GSS_KRB5_MECHANISM,
					 &(*delegated_cred_handle)->mechanisms);
	    if (ret)
		goto failure;
	}

	if (kret) {
	    flags &= ~GSS_C_DELEG_FLAG;
	    goto end_fwd;
	}
      
	kret = krb5_cc_initialize(gssapi_krb5_context,
				  ccache,
				  *src_name);
	if (kret) {
	    flags &= ~GSS_C_DELEG_FLAG;
	    goto end_fwd;
	}
      
	krb5_auth_con_getflags(gssapi_krb5_context,
			       (*context_handle)->auth_context,
			       &ac_flags);
	krb5_auth_con_setflags(gssapi_krb5_context,
			       (*context_handle)->auth_context,
			       ac_flags & ~KRB5_AUTH_CONTEXT_DO_TIME);
	kret = krb5_rd_cred2(gssapi_krb5_context,
			     (*context_handle)->auth_context,
			     ccache,
			     &fwd_data);
	krb5_auth_con_setflags(gssapi_krb5_context,
			       (*context_handle)->auth_context,
			       ac_flags);
	if (kret) {
	    flags &= ~GSS_C_DELEG_FLAG;
	    goto end_fwd;
	}

      end_fwd:
	free(fwd_data.data);
    }
         

    flags |= GSS_C_TRANS_FLAG;

    if (ret_flags)
	*ret_flags = flags;
    (*context_handle)->lifetime = ticket->ticket.endtime;
    (*context_handle)->flags = flags;
    (*context_handle)->more_flags |= OPEN;

    if (mech_type)
	*mech_type = GSS_KRB5_MECHANISM;

    if (time_rec) {
	ret = gssapi_lifetime_left(minor_status,
				   (*context_handle)->lifetime,
				   time_rec);
	if (ret)
	    goto failure;
    }

    if(flags & GSS_C_MUTUAL_FLAG) {
	krb5_data outbuf;

	kret = krb5_mk_rep (gssapi_krb5_context,
			    (*context_handle)->auth_context,
			    &outbuf);
	if (kret) {
	    ret = GSS_S_FAILURE;
	    *minor_status = kret;
	    gssapi_krb5_set_error_string ();
	    goto failure;
	}
	ret = gssapi_krb5_encapsulate (minor_status,
				       &outbuf,
				       output_token,
				       "\x02\x00");
	krb5_data_free (&outbuf);
	if (ret)
	    goto failure;
    } else {
	output_token->length = 0;
	output_token->value = NULL;
    }

    (*context_handle)->ticket = ticket;
    ticket = NULL;

#if 0
    krb5_free_ticket (context, ticket);
#endif

    *minor_status = 0;
    return GSS_S_COMPLETE;

  failure:
    if (fwd_data.length > 0)
	free(fwd_data.data);
    if (ticket != NULL)
	krb5_free_ticket (gssapi_krb5_context, ticket);
    krb5_auth_con_free (gssapi_krb5_context,
			(*context_handle)->auth_context);
    if((*context_handle)->source)
	krb5_free_principal (gssapi_krb5_context,
			     (*context_handle)->source);
    if((*context_handle)->target)
	krb5_free_principal (gssapi_krb5_context,
			     (*context_handle)->target);
    free (*context_handle);
    if (src_name != NULL) {
	gss_release_name (&minor, src_name);
	*src_name = NULL;
    }
    *context_handle = GSS_C_NO_CONTEXT;
    return ret;
}
