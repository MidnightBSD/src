/* $MidnightBSD$ */
%{
/*-
 * Copyright (c) 2012 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
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
 * $FreeBSD: stable/10/usr.bin/iscsictl/parse.y 262841 2014-03-06 11:07:51Z trasz $
 */

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "iscsictl.h"

extern FILE *yyin;
extern char *yytext;
extern int lineno;

static struct conf *conf;
static struct target *target;

extern void	yyerror(const char *);
extern int	yylex(void);
extern void	yyrestart(FILE *);

%}

%token AUTH_METHOD HEADER_DIGEST DATA_DIGEST TARGET_NAME TARGET_ADDRESS
%token INITIATOR_NAME INITIATOR_ADDRESS INITIATOR_ALIAS USER SECRET
%token MUTUAL_USER MUTUAL_SECRET SEMICOLON SESSION_TYPE PROTOCOL IGNORED
%token EQUALS OPENING_BRACKET CLOSING_BRACKET

%union
{
	char *str;
}

%token <str> STR

%%

targets:
	|
	targets target
	;

target:		STR OPENING_BRACKET target_entries CLOSING_BRACKET
	{
		if (target_find(conf, $1) != NULL)
			errx(1, "duplicated target %s", $1);
		target->t_nickname = $1;
		target = target_new(conf);
	}
	;

target_entries:
	|
	target_entries target_entry
	|
	target_entries target_entry SEMICOLON
	;

target_entry:
	target_name
	|
	target_address
	|
	initiator_name
	|
	initiator_address
	|
	initiator_alias
	|
	user
	|
	secret
	|
	mutual_user
	|
	mutual_secret
	|
	auth_method
	|
	header_digest
	|
	data_digest
	|
	session_type
	|
	protocol
	|
	ignored
	;

target_name:	TARGET_NAME EQUALS STR
	{
		if (target->t_name != NULL)
			errx(1, "duplicated TargetName at line %d", lineno);
		target->t_name = $3;
	}
	;

target_address:	TARGET_ADDRESS EQUALS STR
	{
		if (target->t_address != NULL)
			errx(1, "duplicated TargetAddress at line %d", lineno);
		target->t_address = $3;
	}
	;

initiator_name:	INITIATOR_NAME EQUALS STR
	{
		if (target->t_initiator_name != NULL)
			errx(1, "duplicated InitiatorName at line %d", lineno);
		target->t_initiator_name = $3;
	}
	;

initiator_address:	INITIATOR_ADDRESS EQUALS STR
	{
		if (target->t_initiator_address != NULL)
			errx(1, "duplicated InitiatorAddress at line %d", lineno);
		target->t_initiator_address = $3;
	}
	;

initiator_alias:	INITIATOR_ALIAS EQUALS STR
	{
		if (target->t_initiator_alias != NULL)
			errx(1, "duplicated InitiatorAlias at line %d", lineno);
		target->t_initiator_alias = $3;
	}
	;

user:		USER EQUALS STR
	{
		if (target->t_user != NULL)
			errx(1, "duplicated chapIName at line %d", lineno);
		target->t_user = $3;
	}
	;

secret:		SECRET EQUALS STR
	{
		if (target->t_secret != NULL)
			errx(1, "duplicated chapSecret at line %d", lineno);
		target->t_secret = $3;
	}
	;

mutual_user:	MUTUAL_USER EQUALS STR
	{
		if (target->t_mutual_user != NULL)
			errx(1, "duplicated tgtChapName at line %d", lineno);
		target->t_mutual_user = $3;
	}
	;

mutual_secret:	MUTUAL_SECRET EQUALS STR
	{
		if (target->t_mutual_secret != NULL)
			errx(1, "duplicated tgtChapSecret at line %d", lineno);
		target->t_mutual_secret = $3;
	}
	;

auth_method:	AUTH_METHOD EQUALS STR
	{
		if (target->t_auth_method != AUTH_METHOD_UNSPECIFIED)
			errx(1, "duplicated AuthMethod at line %d", lineno);
		if (strcasecmp($3, "none") == 0)
			target->t_auth_method = AUTH_METHOD_NONE;
		else if (strcasecmp($3, "chap") == 0)
			target->t_auth_method = AUTH_METHOD_CHAP;
		else
			errx(1, "invalid AuthMethod at line %d; "
			    "must be either \"none\" or \"CHAP\"", lineno);
	}
	;

header_digest:	HEADER_DIGEST EQUALS STR
	{
		if (target->t_header_digest != DIGEST_UNSPECIFIED)
			errx(1, "duplicated HeaderDigest at line %d", lineno);
		if (strcasecmp($3, "none") == 0)
			target->t_header_digest = DIGEST_NONE;
		else if (strcasecmp($3, "CRC32C") == 0)
			target->t_header_digest = DIGEST_CRC32C;
		else
			errx(1, "invalid HeaderDigest at line %d; "
			    "must be either \"none\" or \"CRC32C\"", lineno);
	}
	;

data_digest:	DATA_DIGEST EQUALS STR
	{
		if (target->t_data_digest != DIGEST_UNSPECIFIED)
			errx(1, "duplicated DataDigest at line %d", lineno);
		if (strcasecmp($3, "none") == 0)
			target->t_data_digest = DIGEST_NONE;
		else if (strcasecmp($3, "CRC32C") == 0)
			target->t_data_digest = DIGEST_CRC32C;
		else
			errx(1, "invalid DataDigest at line %d; "
			    "must be either \"none\" or \"CRC32C\"", lineno);
	}
	;

session_type:	SESSION_TYPE EQUALS STR
	{
		if (target->t_session_type != SESSION_TYPE_UNSPECIFIED)
			errx(1, "duplicated SessionType at line %d", lineno);
		if (strcasecmp($3, "normal") == 0)
			target->t_session_type = SESSION_TYPE_NORMAL;
		else if (strcasecmp($3, "discovery") == 0)
			target->t_session_type = SESSION_TYPE_DISCOVERY;
		else
			errx(1, "invalid SessionType at line %d; "
			    "must be either \"normal\" or \"discovery\"", lineno);
	}
	;

protocol:	PROTOCOL EQUALS STR
	{
		if (target->t_protocol != PROTOCOL_UNSPECIFIED)
			errx(1, "duplicated protocol at line %d", lineno);
		if (strcasecmp($3, "iscsi") == 0)
			target->t_protocol = PROTOCOL_ISCSI;
		else if (strcasecmp($3, "iser") == 0)
			target->t_protocol = PROTOCOL_ISER;
		else
			errx(1, "invalid protocol at line %d; "
			    "must be either \"iscsi\" or \"iser\"", lineno);
	}
	;

ignored:	IGNORED EQUALS STR
	{
		warnx("obsolete statement ignored at line %d", lineno);
	}
	;

%%

void
yyerror(const char *str)
{

	errx(1, "error in configuration file at line %d near '%s': %s",
	    lineno, yytext, str);
}

static void
check_perms(const char *path)
{
	struct stat sb;
	int error;

	error = stat(path, &sb);
	if (error != 0) {
		warn("stat");
		return;
	}
	if (sb.st_mode & S_IWOTH) {
		warnx("%s is world-writable", path);
	} else if (sb.st_mode & S_IROTH) {
		warnx("%s is world-readable", path);
	} else if (sb.st_mode & S_IXOTH) {
		/*
		 * Ok, this one doesn't matter, but still do it,
		 * just for consistency.
		 */
		warnx("%s is world-executable", path);
	}

	/*
	 * XXX: Should we also check for owner != 0?
	 */
}

struct conf *
conf_new_from_file(const char *path)
{
	int error;

	conf = conf_new();
	target = target_new(conf);

	yyin = fopen(path, "r");
	if (yyin == NULL)
		err(1, "unable to open configuration file %s", path);
	check_perms(path);
	lineno = 1;
	yyrestart(yyin);
	error = yyparse();
	assert(error == 0);
	fclose(yyin);

	assert(target->t_nickname == NULL);
	target_delete(target);

	conf_verify(conf);

	return (conf);
}
