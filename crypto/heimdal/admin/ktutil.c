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

#include "ktutil_locl.h"
#include <err.h>

RCSID("$Id: ktutil.c,v 1.36 2002/02/11 14:14:11 joda Exp $");

static int help_flag;
static int version_flag;
int verbose_flag;
char *keytab_string; 
static char keytab_buf[256];

static int help(int argc, char **argv);

static SL_cmd cmds[] = {
    { "add", 		kt_add,		"add",
      "adds key to keytab" },
    { "change",		kt_change,	"change [principal...]",
      "get new key for principals (all)" },
    { "copy",		kt_copy,	"copy src dst",
      "copy one keytab to another" },
    { "get", 		kt_get,		"get [principal...]",
      "create key in database and add to keytab" },
    { "list",		kt_list,	"list",
      "shows contents of a keytab" },
    { "purge",		kt_purge,	"purge",
      "remove old and superceeded entries" },
    { "remove", 	kt_remove,	"remove",
      "remove key from keytab" },
    { "rename", 	kt_rename,	"rename from to",
      "rename entry" },
    { "srvconvert",	srvconv,	"srvconvert [flags]",
      "convert v4 srvtab to keytab" },
    { "srv2keytab" },
    { "srvcreate",	srvcreate,	"srvcreate [flags]",
      "convert keytab to v4 srvtab" },
    { "key2srvtab" },
    { "help",		help,		"help",			"" },
    { NULL, 	NULL,		NULL, 			NULL }
};

static struct getargs args[] = {
    { 
	"version",
	0,
	arg_flag,
	&version_flag,
	NULL,
	NULL 
    },
    { 
	"help",	    
	'h',   
	arg_flag, 
	&help_flag, 
	NULL, 
	NULL
    },
    { 
	"keytab",	    
	'k',   
	arg_string, 
	&keytab_string, 
	"keytab", 
	"keytab to operate on" 
    },
    {
	"verbose",
	'v',
	arg_flag,
	&verbose_flag,
	"verbose",
	"run verbosely"
    }
};

static int num_args = sizeof(args) / sizeof(args[0]);

krb5_context context;

krb5_keytab
ktutil_open_keytab(void)
{
    krb5_error_code ret;
    krb5_keytab keytab;
    if (keytab_string == NULL) {
	ret = krb5_kt_default_name (context, keytab_buf, sizeof(keytab_buf));
	if (ret) {
	    krb5_warn(context, ret, "krb5_kt_default_name");
	    return NULL;
	}
	keytab_string = keytab_buf;
    }
    ret = krb5_kt_resolve(context, keytab_string, &keytab);
    if (ret) {
	krb5_warn(context, ret, "resolving keytab %s", keytab_string);
	return NULL;
    }
    if (verbose_flag)
	fprintf (stderr, "Using keytab %s\n", keytab_string);
	
    return keytab;
}

static int
help(int argc, char **argv)
{
    sl_help(cmds, argc, argv);
    return 0;
}

static void
usage(int status)
{
    arg_printusage(args, num_args, NULL, "command");
    exit(status);
}

int
main(int argc, char **argv)
{
    int optind = 0;
    krb5_error_code ret;
    setprogname(argv[0]);
    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);
    if(getarg(args, num_args, argc, argv, &optind))
	usage(1);
    if(help_flag)
	usage(0);
    if(version_flag) {
	print_version(NULL);
	exit(0);
    }
    argc -= optind;
    argv += optind;
    if(argc == 0)
	usage(1);
    ret = sl_command(cmds, argc, argv);
    if(ret == -1)
	krb5_warnx (context, "unrecognized command: %s", argv[0]);
    return ret;
}
