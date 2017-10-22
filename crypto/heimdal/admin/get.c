/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska H�gskolan
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

#include "ktutil_locl.h"

RCSID("$Id: get.c,v 1.22.2.1 2004/06/21 10:55:46 lha Exp $");

static void*
open_kadmin_connection(char *principal,
		       const char *realm, 
		       char *admin_server, 
		       int server_port)
{
    static kadm5_config_params conf;
    krb5_error_code ret;
    void *kadm_handle;
    memset(&conf, 0, sizeof(conf));

    if(realm) {
	conf.realm = (char*)realm;
	conf.mask |= KADM5_CONFIG_REALM;
    }
    
    if (admin_server) {
	conf.admin_server = admin_server;
	conf.mask |= KADM5_CONFIG_ADMIN_SERVER;
    }

    if (server_port) {
	conf.kadmind_port = htons(server_port);
	conf.mask |= KADM5_CONFIG_KADMIND_PORT;
    }

    /* should get realm from each principal, instead of doing
       everything with the same (local) realm */

    ret = kadm5_init_with_password_ctx(context, 
				       principal,
				       NULL,
				       KADM5_ADMIN_SERVICE,
				       &conf, 0, 0, 
				       &kadm_handle);
    if(ret) {
	krb5_warn(context, ret, "kadm5_init_with_password");
	return NULL;
    }
    return kadm_handle;
}

int
kt_get(int argc, char **argv)
{
    krb5_error_code ret = 0;
    krb5_keytab keytab;
    void *kadm_handle = NULL;
    char *principal = NULL;
    char *realm = NULL;
    char *admin_server = NULL;
    int server_port = 0;
    int help_flag = 0;
    int optind = 0;
    struct getarg_strings etype_strs = {0, NULL};
    krb5_enctype *etypes = NULL;
    size_t netypes = 0;
    
    struct getargs args[] = {
	{ "principal",	'p',	arg_string,   NULL, 
	  "admin principal", "principal" 
	},
	{ "enctypes",	'e',	arg_strings,	NULL,
	  "encryption types to use", "enctypes" },
	{ "realm",	'r',	arg_string,   NULL, 
	  "realm to use", "realm" 
	},
	{ "admin-server",	'a',	arg_string, NULL,
	  "server to contact", "host" 
	},
	{ "server-port",	's',	arg_integer, NULL,
	  "port to contact", "port number" 
	},
	{ "help",		'h',	arg_flag,    NULL }
    };
    int i = 0, j;

    args[i++].value = &principal;
    args[i++].value = &etype_strs;
    args[i++].value = &realm;
    args[i++].value = &admin_server;
    args[i++].value = &server_port;
    args[i++].value = &help_flag;

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optind)
       || help_flag) {
	arg_printusage(args, sizeof(args) / sizeof(args[0]), 
		       "ktutil get", "principal...");
	return 1;
    }
    if(optind == argc) {
	krb5_warnx(context, "no principals specified");
	arg_printusage(args, sizeof(args) / sizeof(args[0]), 
		       "ktutil get", "principal...");
	return 1;
    }
    
    if((keytab = ktutil_open_keytab()) == NULL)
	return 1;

    if(realm)
	krb5_set_default_realm(context, realm);

    if (etype_strs.num_strings) {
	int i;

	etypes = malloc (etype_strs.num_strings * sizeof(*etypes));
	if (etypes == NULL) {
	    krb5_warnx(context, "malloc failed");
	    goto out;
	}
	netypes = etype_strs.num_strings;
	for(i = 0; i < netypes; i++) {
	    ret = krb5_string_to_enctype(context, 
					 etype_strs.strings[i], 
					 &etypes[i]);
	    if(ret) {
		krb5_warnx(context, "unrecognized enctype: %s",
			   etype_strs.strings[i]);
		goto out;
	    }
	}
    }

    
    for(i = optind; i < argc; i++){
	krb5_principal princ_ent;
	kadm5_principal_ent_rec princ;
	int mask = 0;
	krb5_keyblock *keys;
	int n_keys;
	int created = 0;
	krb5_keytab_entry entry;

	ret = krb5_parse_name(context, argv[i], &princ_ent);
	if (ret) {
	    krb5_warn(context, ret, "can't parse principal %s", argv[i]);
	    continue;
	}
	memset(&princ, 0, sizeof(princ));
	princ.principal = princ_ent;
	mask |= KADM5_PRINCIPAL;
	princ.attributes |= KRB5_KDB_DISALLOW_ALL_TIX;
	mask |= KADM5_ATTRIBUTES;
	princ.princ_expire_time = 0;
	mask |= KADM5_PRINC_EXPIRE_TIME;

	if(kadm_handle == NULL) {
	    const char *r;
	    if(realm != NULL)
		r = realm;
	    else
		r = krb5_principal_get_realm(context, princ_ent);
	    kadm_handle = open_kadmin_connection(principal, 
						 r, 
						 admin_server, 
						 server_port);
	    if(kadm_handle == NULL) {
		break;
	    }
	}
	
	ret = kadm5_create_principal(kadm_handle, &princ, mask, "x");
	if(ret == 0)
	    created++;
	else if(ret != KADM5_DUP) {
	    krb5_warn(context, ret, "kadm5_create_principal(%s)", argv[i]);
	    krb5_free_principal(context, princ_ent);
	    continue;
	}
	ret = kadm5_randkey_principal(kadm_handle, princ_ent, &keys, &n_keys);
	if (ret) {
	    krb5_warn(context, ret, "kadm5_randkey_principal(%s)", argv[i]);
	    krb5_free_principal(context, princ_ent);
	    continue;
	}
	
	ret = kadm5_get_principal(kadm_handle, princ_ent, &princ, 
			      KADM5_PRINCIPAL | KADM5_KVNO | KADM5_ATTRIBUTES);
	if (ret) {
	    krb5_warn(context, ret, "kadm5_get_principal(%s)", argv[i]);
	    for (j = 0; j < n_keys; j++)
		krb5_free_keyblock_contents(context, &keys[j]);
	    krb5_free_principal(context, princ_ent);
	    continue;
	}
	princ.attributes &= (~KRB5_KDB_DISALLOW_ALL_TIX);
	mask = KADM5_ATTRIBUTES;
	if(created) {
	    princ.kvno = 1;
	    mask |= KADM5_KVNO;
	}
	ret = kadm5_modify_principal(kadm_handle, &princ, mask);
	if (ret) {
	    krb5_warn(context, ret, "kadm5_modify_principal(%s)", argv[i]);
	    for (j = 0; j < n_keys; j++)
		krb5_free_keyblock_contents(context, &keys[j]);
	    krb5_free_principal(context, princ_ent);
	    continue;
	}
	for(j = 0; j < n_keys; j++) {
	    int do_add = TRUE;

	    if (netypes) {
		int i;

		do_add = FALSE;
		for (i = 0; i < netypes; ++i)
		    if (keys[j].keytype == etypes[i]) {
			do_add = TRUE;
			break;
		    }
	    }
	    if (do_add) {
		entry.principal = princ_ent;
		entry.vno = princ.kvno;
		entry.keyblock = keys[j];
		entry.timestamp = time (NULL);
		ret = krb5_kt_add_entry(context, keytab, &entry);
		if (ret)
		    krb5_warn(context, ret, "krb5_kt_add_entry");
	    }
	    krb5_free_keyblock_contents(context, &keys[j]);
	}
	
	kadm5_free_principal_ent(kadm_handle, &princ);
	krb5_free_principal(context, princ_ent);
    }
 out:
    free_getarg_strings(&etype_strs);
    free(etypes);
    if (kadm_handle)
	kadm5_destroy(kadm_handle);
    krb5_kt_close(context, keytab);
    return ret != 0;
}
