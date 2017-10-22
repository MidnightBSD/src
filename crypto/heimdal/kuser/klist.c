/*
 * Copyright (c) 1997-2004 Kungliga Tekniska H�gskolan
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

#include "kuser_locl.h"
#include "rtbl.h"

RCSID("$Id: klist.c 20516 2007-04-22 10:40:41Z lha $");

static char*
printable_time(time_t t)
{
    static char s[128];
    strlcpy(s, ctime(&t)+ 4, sizeof(s));
    s[15] = 0;
    return s;
}

static char*
printable_time_long(time_t t)
{
    static char s[128];
    strlcpy(s, ctime(&t)+ 4, sizeof(s));
    s[20] = 0;
    return s;
}

#define COL_ISSUED		"  Issued"
#define COL_EXPIRES		"  Expires"
#define COL_FLAGS		"Flags"
#define COL_PRINCIPAL		"  Principal"
#define COL_PRINCIPAL_KVNO	"  Principal (kvno)"
#define COL_CACHENAME		"  Cache name"

static void
print_cred(krb5_context context, krb5_creds *cred, rtbl_t ct, int do_flags)
{
    char *str;
    krb5_error_code ret;
    krb5_timestamp sec;

    krb5_timeofday (context, &sec);


    if(cred->times.starttime)
	rtbl_add_column_entry(ct, COL_ISSUED,
			      printable_time(cred->times.starttime));
    else
	rtbl_add_column_entry(ct, COL_ISSUED,
			      printable_time(cred->times.authtime));
    
    if(cred->times.endtime > sec)
	rtbl_add_column_entry(ct, COL_EXPIRES,
			      printable_time(cred->times.endtime));
    else
	rtbl_add_column_entry(ct, COL_EXPIRES, ">>>Expired<<<");
    ret = krb5_unparse_name (context, cred->server, &str);
    if (ret)
	krb5_err(context, 1, ret, "krb5_unparse_name");
    rtbl_add_column_entry(ct, COL_PRINCIPAL, str);
    if(do_flags) {
	char s[16], *sp = s;
	if(cred->flags.b.forwardable)
	    *sp++ = 'F';
	if(cred->flags.b.forwarded)
	    *sp++ = 'f';
	if(cred->flags.b.proxiable)
	    *sp++ = 'P';
	if(cred->flags.b.proxy)
	    *sp++ = 'p';
	if(cred->flags.b.may_postdate)
	    *sp++ = 'D';
	if(cred->flags.b.postdated)
	    *sp++ = 'd';
	if(cred->flags.b.renewable)
	    *sp++ = 'R';
	if(cred->flags.b.initial)
	    *sp++ = 'I';
	if(cred->flags.b.invalid)
	    *sp++ = 'i';
	if(cred->flags.b.pre_authent)
	    *sp++ = 'A';
	if(cred->flags.b.hw_authent)
	    *sp++ = 'H';
	*sp++ = '\0';
	rtbl_add_column_entry(ct, COL_FLAGS, s);
    }
    free(str);
}

static void
print_cred_verbose(krb5_context context, krb5_creds *cred)
{
    int j;
    char *str;
    krb5_error_code ret;
    int first_flag;
    krb5_timestamp sec;

    krb5_timeofday (context, &sec);

    ret = krb5_unparse_name(context, cred->server, &str);
    if(ret)
	exit(1);
    printf("Server: %s\n", str);
    free (str);

    ret = krb5_unparse_name(context, cred->client, &str);
    if(ret)
	exit(1);
    printf("Client: %s\n", str);
    free (str);

    {
	Ticket t;
	size_t len;
	char *s;

	decode_Ticket(cred->ticket.data, cred->ticket.length, &t, &len);
	ret = krb5_enctype_to_string(context, t.enc_part.etype, &s);
	printf("Ticket etype: ");
	if (ret == 0) {
	    printf("%s", s);
	    free(s);
	} else {
	    printf("unknown(%d)", t.enc_part.etype);
	}
	if(t.enc_part.kvno)
	    printf(", kvno %d", *t.enc_part.kvno);
	printf("\n");
	if(cred->session.keytype != t.enc_part.etype) {
	    ret = krb5_enctype_to_string(context, cred->session.keytype, &str);
	    if(ret)
		krb5_warn(context, ret, "session keytype");
	    else {
		printf("Session key: %s\n", str);
		free(str);
	    }
	}
	free_Ticket(&t);
	printf("Ticket length: %lu\n", (unsigned long)cred->ticket.length);
    }
    printf("Auth time:  %s\n", printable_time_long(cred->times.authtime));
    if(cred->times.authtime != cred->times.starttime)
	printf("Start time: %s\n", printable_time_long(cred->times.starttime));
    printf("End time:   %s", printable_time_long(cred->times.endtime));
    if(sec > cred->times.endtime)
	printf(" (expired)");
    printf("\n");
    if(cred->flags.b.renewable)
	printf("Renew till: %s\n", 
	       printable_time_long(cred->times.renew_till));
    printf("Ticket flags: ");
#define PRINT_FLAG2(f, s) if(cred->flags.b.f) { if(!first_flag) printf(", "); printf("%s", #s); first_flag = 0; }
#define PRINT_FLAG(f) PRINT_FLAG2(f, f)
    first_flag = 1;
    PRINT_FLAG(forwardable);
    PRINT_FLAG(forwarded);
    PRINT_FLAG(proxiable);
    PRINT_FLAG(proxy);
    PRINT_FLAG2(may_postdate, may-postdate);
    PRINT_FLAG(postdated);
    PRINT_FLAG(invalid);
    PRINT_FLAG(renewable);
    PRINT_FLAG(initial);
    PRINT_FLAG2(pre_authent, pre-authenticated);
    PRINT_FLAG2(hw_authent, hw-authenticated);
    PRINT_FLAG2(transited_policy_checked, transited-policy-checked);
    PRINT_FLAG2(ok_as_delegate, ok-as-delegate);
    PRINT_FLAG(anonymous);
    printf("\n");
    printf("Addresses: ");
    if (cred->addresses.len != 0) {
	for(j = 0; j < cred->addresses.len; j++){
	    char buf[128];
	    size_t len;
	    if(j) printf(", ");
	    ret = krb5_print_address(&cred->addresses.val[j], 
				     buf, sizeof(buf), &len);
	    
	    if(ret == 0)
		printf("%s", buf);
	}
    } else {
	printf("addressless");
    }
    printf("\n\n");
}

/*
 * Print all tickets in `ccache' on stdout, verbosily iff do_verbose.
 */

static void
print_tickets (krb5_context context,
	       krb5_ccache ccache,
	       krb5_principal principal,
	       int do_verbose,
	       int do_flags,
	       int do_hidden)
{
    krb5_error_code ret;
    char *str;
    krb5_cc_cursor cursor;
    krb5_creds creds;
    int32_t sec, usec;

    rtbl_t ct = NULL;

    ret = krb5_unparse_name (context, principal, &str);
    if (ret)
	krb5_err (context, 1, ret, "krb5_unparse_name");

    printf ("%17s: %s:%s\n", 
	    "Credentials cache",
	    krb5_cc_get_type(context, ccache),
	    krb5_cc_get_name(context, ccache));
    printf ("%17s: %s\n", "Principal", str);
    free (str);
    
    if(do_verbose)
	printf ("%17s: %d\n", "Cache version",
		krb5_cc_get_version(context, ccache));
    
    krb5_get_kdc_sec_offset(context, &sec, &usec);

    if (do_verbose && sec != 0) {
	char buf[BUFSIZ];
	int val;
	int sig;

	val = sec;
	sig = 1;
	if (val < 0) {
	    sig = -1;
	    val = -val;
	}
	
	unparse_time (val, buf, sizeof(buf));

	printf ("%17s: %s%s\n", "KDC time offset",
		sig == -1 ? "-" : "", buf);
    }

    printf("\n");

    ret = krb5_cc_start_seq_get (context, ccache, &cursor);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_start_seq_get");

    if(!do_verbose) {
	ct = rtbl_create();
	rtbl_add_column(ct, COL_ISSUED, 0);
	rtbl_add_column(ct, COL_EXPIRES, 0);
	if(do_flags)
	    rtbl_add_column(ct, COL_FLAGS, 0);
	rtbl_add_column(ct, COL_PRINCIPAL, 0);
	rtbl_set_separator(ct, "  ");
    }
    while ((ret = krb5_cc_next_cred (context,
				     ccache,
				     &cursor,
				     &creds)) == 0) {
	const char *str;
	str = krb5_principal_get_comp_string(context, creds.server, 0);
	if (!do_hidden && str && str[0] == '@') {
	    ;
	}else if(do_verbose){
	    print_cred_verbose(context, &creds);
	}else{
	    print_cred(context, &creds, ct, do_flags);
	}
	krb5_free_cred_contents (context, &creds);
    }
    if(ret != KRB5_CC_END)
	krb5_err(context, 1, ret, "krb5_cc_get_next");
    ret = krb5_cc_end_seq_get (context, ccache, &cursor);
    if (ret)
	krb5_err (context, 1, ret, "krb5_cc_end_seq_get");
    if(!do_verbose) {
	rtbl_format(ct, stdout);
	rtbl_destroy(ct);
    }
}

/*
 * Check if there's a tgt for the realm of `principal' and ccache and
 * if so return 0, else 1
 */

static int
check_for_tgt (krb5_context context,
	       krb5_ccache ccache,
	       krb5_principal principal,
	       time_t *expiration)
{
    krb5_error_code ret;
    krb5_creds pattern;
    krb5_creds creds;
    krb5_realm *client_realm;
    int expired;

    krb5_cc_clear_mcred(&pattern);

    client_realm = krb5_princ_realm (context, principal);

    ret = krb5_make_principal (context, &pattern.server,
			       *client_realm, KRB5_TGS_NAME, *client_realm,
			       NULL);
    if (ret)
	krb5_err (context, 1, ret, "krb5_make_principal");
    pattern.client = principal;

    ret = krb5_cc_retrieve_cred (context, ccache, 0, &pattern, &creds);
    krb5_free_principal (context, pattern.server);
    if (ret) {
	if (ret == KRB5_CC_END)
	    return 1;
	krb5_err (context, 1, ret, "krb5_cc_retrieve_cred");
    }

    expired = time(NULL) > creds.times.endtime;

    if (expiration)
	*expiration = creds.times.endtime;

    krb5_free_cred_contents (context, &creds);

    return expired;
}

/*
 * Print a list of all AFS tokens
 */

static void
display_tokens(int do_verbose)
{
    uint32_t i;
    unsigned char t[4096];
    struct ViceIoctl parms;

    parms.in = (void *)&i;
    parms.in_size = sizeof(i);
    parms.out = (void *)t;
    parms.out_size = sizeof(t);

    for (i = 0;; i++) {
        int32_t size_secret_tok, size_public_tok;
        unsigned char *cell;
	struct ClearToken ct;
	unsigned char *r = t;
	struct timeval tv;
	char buf1[20], buf2[20];

	if(k_pioctl(NULL, VIOCGETTOK, &parms, 0) < 0) {
	    if(errno == EDOM)
		break;
	    continue;
	}
	if(parms.out_size > sizeof(t))
	    continue;
	if(parms.out_size < sizeof(size_secret_tok))
	    continue;
	t[min(parms.out_size,sizeof(t)-1)] = 0;
	memcpy(&size_secret_tok, r, sizeof(size_secret_tok));
	/* dont bother about the secret token */
	r += size_secret_tok + sizeof(size_secret_tok);
	if (parms.out_size < (r - t) + sizeof(size_public_tok))
	    continue;
	memcpy(&size_public_tok, r, sizeof(size_public_tok));
	r += sizeof(size_public_tok);
	if (parms.out_size < (r - t) + size_public_tok + sizeof(int32_t))
	    continue;
	memcpy(&ct, r, size_public_tok);
	r += size_public_tok;
	/* there is a int32_t with length of cellname, but we dont read it */
	r += sizeof(int32_t);
	cell = r;

	gettimeofday (&tv, NULL);
	strlcpy (buf1, printable_time(ct.BeginTimestamp),
		 sizeof(buf1));
	if (do_verbose || tv.tv_sec < ct.EndTimestamp)
	    strlcpy (buf2, printable_time(ct.EndTimestamp),
		     sizeof(buf2));
	else
	    strlcpy (buf2, ">>> Expired <<<", sizeof(buf2));

	printf("%s  %s  ", buf1, buf2);

	if ((ct.EndTimestamp - ct.BeginTimestamp) & 1)
	    printf("User's (AFS ID %d) tokens for %s", ct.ViceId, cell);
	else
	    printf("Tokens for %s", cell);
	if (do_verbose)
	    printf(" (%d)", ct.AuthHandle);
	putchar('\n');
    }
}

/*
 * display the ccache in `cred_cache'
 */

static int
display_v5_ccache (const char *cred_cache, int do_test, int do_verbose, 
		   int do_flags, int do_hidden)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_ccache ccache;
    krb5_principal principal;
    int exit_status = 0;

    ret = krb5_init_context (&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    if(cred_cache) {
	ret = krb5_cc_resolve(context, cred_cache, &ccache);
	if (ret)
	    krb5_err (context, 1, ret, "%s", cred_cache);
    } else {
	ret = krb5_cc_default (context, &ccache);
	if (ret)
	    krb5_err (context, 1, ret, "krb5_cc_resolve");
    }

    ret = krb5_cc_get_principal (context, ccache, &principal);
    if (ret) {
	if(ret == ENOENT) {
	    if (!do_test)
		krb5_warnx(context, "No ticket file: %s",
			   krb5_cc_get_name(context, ccache));
	    return 1;
	} else
	    krb5_err (context, 1, ret, "krb5_cc_get_principal");
    }
    if (do_test)
	exit_status = check_for_tgt (context, ccache, principal, NULL);
    else
	print_tickets (context, ccache, principal, do_verbose,
		       do_flags, do_hidden);

    ret = krb5_cc_close (context, ccache);
    if (ret)
	krb5_err (context, 1, ret, "krb5_cc_close");

    krb5_free_principal (context, principal);
    krb5_free_context (context);
    return exit_status;
}

/*
 *
 */

static int
list_caches(void)
{
    krb5_cc_cache_cursor cursor;
    krb5_context context;
    krb5_error_code ret;
    krb5_ccache id;
    rtbl_t ct;
    
    ret = krb5_init_context (&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    ret = krb5_cc_cache_get_first (context, NULL, &cursor);
    if (ret == KRB5_CC_NOSUPP)
	return 0;
    else if (ret)
	krb5_err (context, 1, ret, "krb5_cc_cache_get_first");

    ct = rtbl_create();
    rtbl_add_column(ct, COL_PRINCIPAL, 0);
    rtbl_add_column(ct, COL_CACHENAME, 0);
    rtbl_add_column(ct, COL_EXPIRES, 0);
    rtbl_set_prefix(ct, "   ");
    rtbl_set_column_prefix(ct, COL_PRINCIPAL, "");

    while ((ret = krb5_cc_cache_next (context, cursor, &id)) == 0) {
	krb5_principal principal;
	char *name;

	ret = krb5_cc_get_principal(context, id, &principal);
	if (ret == 0) {
	    time_t t;
	    int expired = check_for_tgt (context, id, principal, &t);

	    ret = krb5_unparse_name(context, principal, &name);
	    if (ret == 0) {
		rtbl_add_column_entry(ct, COL_PRINCIPAL, name);
		rtbl_add_column_entry(ct, COL_CACHENAME,
				      krb5_cc_get_name(context, id));
		rtbl_add_column_entry(ct, COL_EXPIRES,
				      expired ? ">>> Expired <<<" : 
				      printable_time(t));
		free(name);
		krb5_free_principal(context, principal);
	    }
	}
	krb5_cc_close(context, id);
    }

    krb5_cc_cache_end_seq_get(context, cursor);

    rtbl_format(ct, stdout);
    rtbl_destroy(ct);
    
    return 0;
}

/*
 *
 */

static int version_flag		= 0;
static int help_flag		= 0;
static int do_verbose		= 0;
static int do_list_caches	= 0;
static int do_test		= 0;
static int do_tokens		= 0;
static int do_v5		= 1;
static char *cred_cache;
static int do_flags	 	= 0;
static int do_hidden	 	= 0;

static struct getargs args[] = {
    { NULL, 'f', arg_flag, &do_flags },
    { "cache",			'c', arg_string, &cred_cache,
      "credentials cache to list", "cache" },
    { "test",			't', arg_flag, &do_test,
      "test for having tickets", NULL },
    { NULL,			's', arg_flag, &do_test },
    { "tokens",			'T',   arg_flag, &do_tokens,
      "display AFS tokens", NULL },
    { "v5",			'5',	arg_flag, &do_v5,
      "display v5 cred cache", NULL},
    { "list-caches",		'l', arg_flag, &do_list_caches,
      "verbose output", NULL },
    { "verbose",		'v', arg_flag, &do_verbose,
      "verbose output", NULL },
    { "hidden",			0,   arg_flag, &do_hidden,
      "display hidden credentials", NULL },
    { NULL,			'a', arg_flag, &do_verbose },
    { NULL,			'n', arg_flag, &do_verbose },
    { "version", 		0,   arg_flag, &version_flag, 
      "print version", NULL },
    { "help",			0,   arg_flag, &help_flag, 
      NULL, NULL}
};

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args)/sizeof(*args),
		    NULL,
		    "");
    exit (ret);
}

int
main (int argc, char **argv)
{
    int optidx = 0;
    int exit_status = 0;

    setprogname (argv[0]);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);
    
    if (help_flag)
	usage (0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    argc -= optidx;
    argv += optidx;

    if (argc != 0)
	usage (1);

    if (do_list_caches) {
	exit_status = list_caches();
	return exit_status;
    }

    if (do_v5)
	exit_status = display_v5_ccache (cred_cache, do_test, 
					 do_verbose, do_flags, do_hidden);

    if (!do_test) {
	if (do_tokens && k_hasafs ()) {
	    if (do_v5)
		printf ("\n");
	    display_tokens (do_verbose);
	}
    }

    return exit_status;
}
