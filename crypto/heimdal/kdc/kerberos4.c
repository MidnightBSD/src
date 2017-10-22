/*
 * Copyright (c) 1997 - 2004 Kungliga Tekniska H�gskolan
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

#include "kdc_locl.h"

RCSID("$Id: kerberos4.c,v 1.45.2.1 2004/03/30 10:29:27 lha Exp $");

#ifdef KRB4

#ifndef swap32
static u_int32_t
swap32(u_int32_t x)
{
    return ((x << 24) & 0xff000000) |
	((x << 8) & 0xff0000) |
	((x >> 8) & 0xff00) |
	((x >> 24) & 0xff);
}
#endif /* swap32 */

int
maybe_version4(unsigned char *buf, int len)
{
    return len > 0 && *buf == 4;
}

static void
make_err_reply(krb5_data *reply, int code, const char *msg)
{
    KTEXT_ST er;

    /* name, instance and realm are not checked in most (all?)
       implementations; msg is also never used, but we send it anyway
       (for debugging purposes) */

    if(msg == NULL)
	msg = krb_get_err_text(code);
    cr_err_reply(&er, "", "", "", kdc_time, code, (char*)msg);
    krb5_data_copy(reply, er.dat, er.length);
}

static krb5_boolean
valid_princ(krb5_context context, krb5_principal princ)
{
    krb5_error_code ret;
    char *s;
    hdb_entry *ent;

    ret = krb5_unparse_name(context, princ, &s);
    if (ret)
	return FALSE;
    ret = db_fetch(princ, &ent);
    if (ret) {
	kdc_log(7, "Lookup %s failed: %s", s,
		krb5_get_err_text (context, ret));
	free(s);
	return FALSE;
    }
    kdc_log(7, "Lookup %s succeeded", s);
    free(s);
    free_ent(ent);
    return TRUE;
}

krb5_error_code
db_fetch4(const char *name, const char *instance, const char *realm,
	  hdb_entry **ent)
{
    krb5_principal p;
    krb5_error_code ret;
    
    ret = krb5_425_conv_principal_ext(context, name, instance, realm, 
				      valid_princ, 0, &p);
    if(ret)
	return ret;
    ret = db_fetch(p, ent);
    krb5_free_principal(context, p);
    return ret;
}

#define RCHECK(X, L) if(X){make_err_reply(reply, KFAILURE, "Packet too short"); goto L;}

/*
 * Process the v4 request in `buf, len' (received from `addr'
 * (with string `from').
 * Return an error code and a reply in `reply'.
 */

krb5_error_code
do_version4(unsigned char *buf,
	    size_t len,
	    krb5_data *reply,
	    const char *from,
	    struct sockaddr_in *addr)
{
    krb5_storage *sp;
    krb5_error_code ret;
    hdb_entry *client = NULL, *server = NULL;
    Key *ckey, *skey;
    int8_t pvno;
    int8_t msg_type;
    int lsb;
    char *name = NULL, *inst = NULL, *realm = NULL;
    char *sname = NULL, *sinst = NULL;
    int32_t req_time;
    time_t max_life, max_end, actual_end, issue_time;
    u_int8_t life;
    char client_name[256];
    char server_name[256];

    if(!enable_v4) {
	kdc_log(0, "Rejected version 4 request from %s", from);
	make_err_reply(reply, KDC_GEN_ERR, "function not enabled");
	return 0;
    }

    sp = krb5_storage_from_mem(buf, len);
    RCHECK(krb5_ret_int8(sp, &pvno), out);
    if(pvno != 4){
	kdc_log(0, "Protocol version mismatch (krb4) (%d)", pvno);
	make_err_reply(reply, KDC_PKT_VER, NULL);
	goto out;
    }
    RCHECK(krb5_ret_int8(sp, &msg_type), out);
    lsb = msg_type & 1;
    msg_type &= ~1;
    switch(msg_type){
    case AUTH_MSG_KDC_REQUEST:
	RCHECK(krb5_ret_stringz(sp, &name), out1);
	RCHECK(krb5_ret_stringz(sp, &inst), out1);
	RCHECK(krb5_ret_stringz(sp, &realm), out1);
	RCHECK(krb5_ret_int32(sp, &req_time), out1);
	if(lsb)
	    req_time = swap32(req_time);
	RCHECK(krb5_ret_int8(sp, &life), out1);
	RCHECK(krb5_ret_stringz(sp, &sname), out1);
	RCHECK(krb5_ret_stringz(sp, &sinst), out1);
	snprintf (client_name, sizeof(client_name),
		  "%s.%s@%s", name, inst, realm);
	snprintf (server_name, sizeof(server_name),
		  "%s.%s@%s", sname, sinst, v4_realm);
	
	kdc_log(0, "AS-REQ (krb4) %s from %s for %s",
		client_name, from, server_name);

	ret = db_fetch4(name, inst, realm, &client);
	if(ret) {
	    kdc_log(0, "Client not found in database: %s: %s",
		    client_name, krb5_get_err_text(context, ret));
	    make_err_reply(reply, KERB_ERR_PRINCIPAL_UNKNOWN, NULL);
	    goto out1;
	}
	ret = db_fetch4(sname, sinst, v4_realm, &server);
	if(ret){
	    kdc_log(0, "Server not found in database: %s: %s",
		    server_name, krb5_get_err_text(context, ret));
	    make_err_reply(reply, KERB_ERR_PRINCIPAL_UNKNOWN, NULL);
	    goto out1;
	}

	ret = check_flags (client, client_name,
			   server, server_name,
			   TRUE);
	if (ret) {
	    /* good error code? */
	    make_err_reply(reply, KERB_ERR_NAME_EXP, NULL);
	    goto out1;
	}

	/*
	 * There's no way to do pre-authentication in v4 and thus no
	 * good error code to return if preauthentication is required.
	 */

	if (require_preauth
	    || client->flags.require_preauth
	    || server->flags.require_preauth) {
	    kdc_log(0,
		    "Pre-authentication required for v4-request: "
		    "%s for %s",
		    client_name, server_name);
	    make_err_reply(reply, KERB_ERR_NULL_KEY, NULL);
	    goto out1;
	}

	ret = get_des_key(client, FALSE, FALSE, &ckey);
	if(ret){
	    kdc_log(0, "no suitable DES key for client");
	    make_err_reply(reply, KDC_NULL_KEY, 
			   "no suitable DES key for client");
	    goto out1;
	}

#if 0
	/* this is not necessary with the new code in libkrb */
	/* find a properly salted key */
	while(ckey->salt == NULL || ckey->salt->salt.length != 0)
	    ret = hdb_next_keytype2key(context, client, KEYTYPE_DES, &ckey);
	if(ret){
	    kdc_log(0, "No version-4 salted key in database -- %s.%s@%s", 
		    name, inst, realm);
	    make_err_reply(reply, KDC_NULL_KEY, 
			   "No version-4 salted key in database");
	    goto out1;
	}
#endif
	
	ret = get_des_key(server, TRUE, FALSE, &skey);
	if(ret){
	    kdc_log(0, "no suitable DES key for server");
	    /* XXX */
	    make_err_reply(reply, KDC_NULL_KEY, 
			   "no suitable DES key for server");
	    goto out1;
	}

	max_life = krb_life_to_time(0, life);
	if(client->max_life)
	    max_life = min(max_life, *client->max_life);
	if(server->max_life)
	    max_life = min(max_life, *server->max_life);

	life = krb_time_to_life(kdc_time, kdc_time + max_life);
    
	{
	    KTEXT_ST cipher, ticket;
	    KTEXT r;
	    des_cblock session;

	    des_new_random_key(&session);

	    krb_create_ticket(&ticket, 0, name, inst, v4_realm,
			      addr->sin_addr.s_addr, session, life, kdc_time, 
			      sname, sinst, skey->key.keyvalue.data);
	
	    create_ciph(&cipher, session, sname, sinst, v4_realm,
			life, server->kvno % 256, &ticket, kdc_time, 
			ckey->key.keyvalue.data);
	    memset(&session, 0, sizeof(session));
	    r = create_auth_reply(name, inst, realm, req_time, 0, 
				  client->pw_end ? *client->pw_end : 0, 
				  client->kvno % 256, &cipher);
	    krb5_data_copy(reply, r->dat, r->length);
	    memset(&cipher, 0, sizeof(cipher));
	    memset(&ticket, 0, sizeof(ticket));
	}
    out1:
	break;
    case AUTH_MSG_APPL_REQUEST: {
	int8_t kvno;
	int8_t ticket_len;
	int8_t req_len;
	KTEXT_ST auth;
	AUTH_DAT ad;
	size_t pos;
	krb5_principal tgt_princ = NULL;
	hdb_entry *tgt = NULL;
	Key *tkey;
	
	RCHECK(krb5_ret_int8(sp, &kvno), out2);
	RCHECK(krb5_ret_stringz(sp, &realm), out2);
	
	ret = krb5_425_conv_principal(context, "krbtgt", realm, v4_realm,
				      &tgt_princ);
	if(ret){
	    kdc_log(0, "Converting krbtgt principal (krb4): %s", 
		    krb5_get_err_text(context, ret));
	    make_err_reply(reply, KFAILURE, 
			   "Failed to convert v4 principal (krbtgt)");
	    goto out2;
	}

	ret = db_fetch(tgt_princ, &tgt);
	if(ret){
	    char *s;
	    s = kdc_log_msg(0, "Ticket-granting ticket not "
			    "found in database (krb4): krbtgt.%s@%s: %s", 
			    realm, v4_realm,
			    krb5_get_err_text(context, ret));
	    make_err_reply(reply, KFAILURE, s);
	    free(s);
	    goto out2;
	}
	
	if(tgt->kvno % 256 != kvno){
	    kdc_log(0, "tgs-req (krb4) with old kvno %d (current %d) for "
		    "krbtgt.%s@%s", kvno, tgt->kvno % 256, realm, v4_realm);
	    make_err_reply(reply, KDC_AUTH_EXP,
			   "old krbtgt kvno used");
	    goto out2;
	}

	ret = get_des_key(tgt, TRUE, FALSE, &tkey);
	if(ret){
	    kdc_log(0, "no suitable DES key for krbtgt (krb4)");
	    /* XXX */
	    make_err_reply(reply, KDC_NULL_KEY, 
			   "no suitable DES key for krbtgt");
	    goto out2;
	}

	RCHECK(krb5_ret_int8(sp, &ticket_len), out2);
	RCHECK(krb5_ret_int8(sp, &req_len), out2);
	
	pos = krb5_storage_seek(sp, ticket_len + req_len, SEEK_CUR);
	
	memset(&auth, 0, sizeof(auth));
	memcpy(&auth.dat, buf, pos);
	auth.length = pos;
	krb_set_key(tkey->key.keyvalue.data, 0);

	krb_ignore_ip_address = !check_ticket_addresses;

	ret = krb_rd_req(&auth, "krbtgt", realm, 
			 addr->sin_addr.s_addr, &ad, 0);
	if(ret){
	    kdc_log(0, "krb_rd_req: %s", krb_get_err_text(ret));
	    make_err_reply(reply, ret, NULL);
	    goto out2;
	}
	
	RCHECK(krb5_ret_int32(sp, &req_time), out2);
	if(lsb)
	    req_time = swap32(req_time);
	RCHECK(krb5_ret_int8(sp, &life), out2);
	RCHECK(krb5_ret_stringz(sp, &sname), out2);
	RCHECK(krb5_ret_stringz(sp, &sinst), out2);
	snprintf (server_name, sizeof(server_name),
		  "%s.%s@%s",
		  sname, sinst, v4_realm);

	kdc_log(0, "TGS-REQ (krb4) %s.%s@%s from %s for %s",
		ad.pname, ad.pinst, ad.prealm, from, server_name);
	
	if(strcmp(ad.prealm, realm)){
	    kdc_log(0, "Can't hop realms (krb4) %s -> %s", realm, ad.prealm);
	    make_err_reply(reply, KERB_ERR_PRINCIPAL_UNKNOWN, 
			   "Can't hop realms");
	    goto out2;
	}

	if (!enable_v4_cross_realm && strcmp(realm, v4_realm) != 0) {
	    kdc_log(0, "krb4 Cross-realm %s -> %s disabled", realm, v4_realm);
	    make_err_reply(reply, KERB_ERR_PRINCIPAL_UNKNOWN, 
			   "Can't hop realms");
	    goto out2;
	}

	if(strcmp(sname, "changepw") == 0){
	    kdc_log(0, "Bad request for changepw ticket (krb4)");
	    make_err_reply(reply, KERB_ERR_PRINCIPAL_UNKNOWN, 
			   "Can't authorize password change based on TGT");
	    goto out2;
	}
	
#if 0
	ret = db_fetch4(ad.pname, ad.pinst, ad.prealm, &client);
	if(ret){
	    char *s;
	    s = kdc_log_msg(0, "Client not found in database: (krb4) "
			    "%s.%s@%s: %s",
			    ad.pname, ad.pinst, ad.prealm,
			    krb5_get_err_text(context, ret));
	    make_err_reply(reply, KERB_ERR_PRINCIPAL_UNKNOWN, s);
	    free(s);
	    goto out2;
	}
#endif
	
	ret = db_fetch4(sname, sinst, v4_realm, &server);
	if(ret){
	    char *s;
	    s = kdc_log_msg(0, "Server not found in database (krb4): %s: %s",
			    server_name, krb5_get_err_text(context, ret));
	    make_err_reply(reply, KERB_ERR_PRINCIPAL_UNKNOWN, s);
	    free(s);
	    goto out2;
	}

	ret = check_flags (NULL, NULL,
			   server, server_name,
			   FALSE);
	if (ret) {
	    /* good error code? */
	    make_err_reply(reply, KERB_ERR_NAME_EXP, NULL);
	    goto out2;
	}

	ret = get_des_key(server, TRUE, FALSE, &skey);
	if(ret){
	    kdc_log(0, "no suitable DES key for server (krb4)");
	    /* XXX */
	    make_err_reply(reply, KDC_NULL_KEY, 
			   "no suitable DES key for server");
	    goto out2;
	}

	max_end = krb_life_to_time(ad.time_sec, ad.life);
	max_end = min(max_end, krb_life_to_time(kdc_time, life));
	life = min(life, krb_time_to_life(kdc_time, max_end));
	
	issue_time = kdc_time;
	actual_end = krb_life_to_time(issue_time, life);
	while (actual_end > max_end && life > 1) {
	    /* move them into the next earlier lifetime bracket */
	    life--;
	    actual_end = krb_life_to_time(issue_time, life);
	}
	if (actual_end > max_end) {
	    /* if life <= 1 and it's still too long, backdate the ticket */
	    issue_time -= actual_end - max_end;
	}

	{
	    KTEXT_ST cipher, ticket;
	    KTEXT r;
	    des_cblock session;
	    des_new_random_key(&session);

	    krb_create_ticket(&ticket, 0, ad.pname, ad.pinst, ad.prealm,
			      addr->sin_addr.s_addr, &session, life, 
			      issue_time,
			      sname, sinst, skey->key.keyvalue.data);
	    
	    create_ciph(&cipher, session, sname, sinst, v4_realm,
			life, server->kvno % 256, &ticket,
			issue_time, &ad.session);
	    
	    memset(&session, 0, sizeof(session));
	    memset(ad.session, 0, sizeof(ad.session));

	    r = create_auth_reply(ad.pname, ad.pinst, ad.prealm, 
				  req_time, 0, 0, 0, &cipher);
	    krb5_data_copy(reply, r->dat, r->length);
	    memset(&cipher, 0, sizeof(cipher));
	    memset(&ticket, 0, sizeof(ticket));
	}
    out2:
	if(tgt_princ)
	    krb5_free_principal(context, tgt_princ);
	if(tgt)
	    free_ent(tgt);
	break;
    }
    
    case AUTH_MSG_ERR_REPLY:
	break;
    default:
	kdc_log(0, "Unknown message type (krb4): %d from %s", 
		msg_type, from);
	
	make_err_reply(reply, KFAILURE, "Unknown message type");
    }
out:
    if(name)
	free(name);
    if(inst)
	free(inst);
    if(realm)
	free(realm);
    if(sname)
	free(sname);
    if(sinst)
	free(sinst);
    if(client)
	free_ent(client);
    if(server)
	free_ent(server);
    krb5_storage_free(sp);
    return 0;
}

#else /* KRB4 */

#include <krb5-v4compat.h>

#endif /* KRB4 */

krb5_error_code
encode_v4_ticket(void *buf, size_t len, const EncTicketPart *et,
		 const PrincipalName *service, size_t *size)
{
    krb5_storage *sp;
    krb5_error_code ret;
    char name[40], inst[40], realm[40];
    char sname[40], sinst[40];

    {
	krb5_principal princ;
	principalname2krb5_principal(&princ,
				     *service,
				     et->crealm);
	ret = krb5_524_conv_principal(context, 
				      princ,
				      sname,
				      sinst,
				      realm);
	krb5_free_principal(context, princ);
	if(ret)
	    return ret;

	principalname2krb5_principal(&princ,
				     et->cname,
				     et->crealm);
				     
	ret = krb5_524_conv_principal(context, 
				      princ,
				      name,
				      inst,
				      realm);
	krb5_free_principal(context, princ);
    }
    if(ret)
	return ret;

    sp = krb5_storage_emem();
    
    krb5_store_int8(sp, 0); /* flags */
    krb5_store_stringz(sp, name);
    krb5_store_stringz(sp, inst);
    krb5_store_stringz(sp, realm);
    {
	unsigned char tmp[4] = { 0, 0, 0, 0 };
	int i;
	if(et->caddr){
	    for(i = 0; i < et->caddr->len; i++)
		if(et->caddr->val[i].addr_type == AF_INET &&
		   et->caddr->val[i].address.length == 4){
		    memcpy(tmp, et->caddr->val[i].address.data, 4);
		    break;
		}
	}
	krb5_storage_write(sp, tmp, sizeof(tmp));
    }

    if((et->key.keytype != ETYPE_DES_CBC_MD5 &&
	et->key.keytype != ETYPE_DES_CBC_MD4 &&
	et->key.keytype != ETYPE_DES_CBC_CRC) || 
       et->key.keyvalue.length != 8)
	return -1;
    krb5_storage_write(sp, et->key.keyvalue.data, 8);
    
    {
	time_t start = et->starttime ? *et->starttime : et->authtime;
	krb5_store_int8(sp, krb_time_to_life(start, et->endtime));
	krb5_store_int32(sp, start);
    }

    krb5_store_stringz(sp, sname);
    krb5_store_stringz(sp, sinst);
    
    {
	krb5_data data;
	krb5_storage_to_data(sp, &data);
	krb5_storage_free(sp);
	*size = (data.length + 7) & ~7; /* pad to 8 bytes */
	if(*size > len)
	    return -1;
	memset((unsigned char*)buf - *size + 1, 0, *size);
	memcpy((unsigned char*)buf - *size + 1, data.data, data.length);
	krb5_data_free(&data);
    }
    return 0;
}

krb5_error_code
get_des_key(hdb_entry *principal, krb5_boolean is_server, 
	    krb5_boolean prefer_afs_key, Key **ret_key)
{
    Key *v5_key = NULL, *v4_key = NULL, *afs_key = NULL, *server_key = NULL;
    int i;
    krb5_enctype etypes[] = { ETYPE_DES_CBC_MD5, 
			      ETYPE_DES_CBC_MD4, 
			      ETYPE_DES_CBC_CRC };

    for(i = 0;
	i < sizeof(etypes)/sizeof(etypes[0])
	    && (v5_key == NULL || v4_key == NULL || 
		afs_key == NULL || server_key == NULL);
	++i) {
	Key *key = NULL;
	while(hdb_next_enctype2key(context, principal, etypes[i], &key) == 0) {
	    if(key->salt == NULL) {
		if(v5_key == NULL)
		    v5_key = key;
	    } else if(key->salt->type == hdb_pw_salt && 
		      key->salt->salt.length == 0) {
		if(v4_key == NULL)
		    v4_key = key;
	    } else if(key->salt->type == hdb_afs3_salt) {
		if(afs_key == NULL)
		    afs_key = key;
	    } else if(server_key == NULL)
		server_key = key;
	}
    }

    if(prefer_afs_key) {
	if(afs_key)
	    *ret_key = afs_key;
	else if(v4_key)
	    *ret_key = v4_key;
	else if(v5_key)
	    *ret_key = v5_key;
	else if(is_server && server_key)
	    *ret_key = server_key;
	else
	    return KERB_ERR_NULL_KEY;
    } else {
	if(v4_key)
	    *ret_key = v4_key;
	else if(afs_key)
	    *ret_key = afs_key;
	else  if(v5_key)
	    *ret_key = v5_key;
	else if(is_server && server_key)
	    *ret_key = server_key;
	else
	    return KERB_ERR_NULL_KEY;
    }

    if((*ret_key)->key.keyvalue.length == 0)
	return KERB_ERR_NULL_KEY;
    return 0;
}

