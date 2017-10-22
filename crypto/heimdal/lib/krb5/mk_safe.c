/*
 * Copyright (c) 1997 - 2002 Kungliga Tekniska H�gskolan
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

#include <krb5_locl.h>

RCSID("$Id: mk_safe.c,v 1.28.4.1 2004/03/07 12:46:43 lha Exp $");

krb5_error_code
krb5_mk_safe(krb5_context context,
	     krb5_auth_context auth_context,
	     const krb5_data *userdata,
	     krb5_data *outbuf,
	     /*krb5_replay_data*/ void *outdata)
{
  krb5_error_code ret;
  KRB_SAFE s;
  int32_t sec, usec;
  KerberosTime sec2;
  int usec2;
  u_char *buf = NULL;
  size_t buf_size;
  size_t len;
  u_int32_t tmp_seq;
  krb5_crypto crypto;
  krb5_keyblock *key;

  if (auth_context->local_subkey)
      key = auth_context->local_subkey;
  else if (auth_context->remote_subkey)
      key = auth_context->remote_subkey;
  else
      key = auth_context->keyblock;

  s.pvno = 5;
  s.msg_type = krb_safe;

  s.safe_body.user_data = *userdata;
  krb5_us_timeofday (context, &sec, &usec);

  sec2                   = sec;
  s.safe_body.timestamp  = &sec2;
  usec2                  = usec;
  s.safe_body.usec       = &usec2;
  if (auth_context->flags & KRB5_AUTH_CONTEXT_DO_SEQUENCE) {
      tmp_seq = auth_context->local_seqnumber;
      s.safe_body.seq_number = &tmp_seq;
  } else 
      s.safe_body.seq_number = NULL;

  s.safe_body.s_address = auth_context->local_address;
  s.safe_body.r_address = auth_context->remote_address;

  s.cksum.cksumtype       = 0;
  s.cksum.checksum.data   = NULL;
  s.cksum.checksum.length = 0;

  ASN1_MALLOC_ENCODE(KRB_SAFE, buf, buf_size, &s, &len, ret);
  if (ret)
      return ret;
  if(buf_size != len)
      krb5_abortx(context, "internal error in ASN.1 encoder");
  ret = krb5_crypto_init(context, key, 0, &crypto);
  if (ret) {
      free (buf);
      return ret;
  }
  ret = krb5_create_checksum(context, 
			     crypto,
			     KRB5_KU_KRB_SAFE_CKSUM,
			     0,
			     buf,
			     len,
			     &s.cksum);
  krb5_crypto_destroy(context, crypto);
  if (ret) {
      free (buf);
      return ret;
  }

  free(buf);
  ASN1_MALLOC_ENCODE(KRB_SAFE, buf, buf_size, &s, &len, ret);
  free_Checksum (&s.cksum);
  if(ret)
      return ret;
  if(buf_size != len)
      krb5_abortx(context, "internal error in ASN.1 encoder");

  outbuf->length = len;
  outbuf->data   = buf;
  if (auth_context->flags & KRB5_AUTH_CONTEXT_DO_SEQUENCE)
      auth_context->local_seqnumber =
	  (auth_context->local_seqnumber + 1) & 0xFFFFFFFF;
  return 0;
}
