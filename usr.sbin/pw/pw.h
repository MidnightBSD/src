/*-
 * Copyright (C) 1996
 *	David L. Nugent.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY DAVID L. NUGENT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID L. NUGENT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <pwd.h>
#include <grp.h>
#include <sys/queue.h>
#include <sysexits.h>

#include "psdate.h"
#include "pwupd.h"

enum _mode
{
        M_ADD,
        M_DELETE,
        M_UPDATE,
        M_PRINT,
	M_NEXT,
	M_LOCK,
	M_UNLOCK,
        M_NUM
};

enum _which
{
        W_USER,
        W_GROUP,
        W_NUM
};

struct carg
{
	int		  ch;
	char		  *val;
	LIST_ENTRY(carg)  list;
};

LIST_HEAD(cargs, carg);

struct userconf
{
	int	default_password;	/* Default password for new users? */
	int	reuse_uids;		/* Reuse uids? */
	int	reuse_gids;		/* Reuse gids? */
	char	*nispasswd;		/* Path to NIS version of the passwd file */
	char	*dotdir;		/* Where to obtain skeleton files */
	char	*newmail;		/* Mail to send to new accounts */
	char	*logfile;		/* Where to log changes */
	char	*home;			/* Where to create home directory */
	mode_t	homemode;		/* Home directory permissions */
	char	*shelldir;		/* Where shells are located */
	char	**shells;		/* List of shells */
	char	*shell_default;		/* Default shell */
	char	*default_group;		/* Default group number */
	char	**groups;		/* Default (additional) groups */
	char	*default_class;		/* Default user class */
	uid_t	min_uid, max_uid;	/* Allowed range of uids */
	gid_t	min_gid, max_gid;	/* Allowed range of gids */
	int	expire_days;		/* Days to expiry */
	int	password_days;		/* Days to password expiry */
	int	numgroups;		/* (internal) size of default_group array */
};

#define	_DEF_DIRMODE	(S_IRWXU | S_IRWXG | S_IRWXO)
#define _PATH_PW_CONF	"/etc/pw.conf"
#define _UC_MAXLINE	1024
#define _UC_MAXSHELLS	32

struct userconf *read_userconfig(char const * file);
int write_userconfig(char const * file);
struct carg *addarg(struct cargs * _args, int ch, char *argstr);
struct carg *getarg(struct cargs * _args, int ch);

int pw_user(struct userconf * cnf, int mode, struct cargs * _args);
int pw_group(struct userconf * cnf, int mode, struct cargs * _args);
char    *pw_checkname(u_char *name, int gecos);

int addpwent(struct passwd * pwd);
int delpwent(struct passwd * pwd);
int chgpwent(char const * login, struct passwd * pwd);
int fmtpwent(char *buf, struct passwd * pwd);

int addnispwent(const char *path, struct passwd *pwd);
int delnispwent(const char *path, const char *login);
int chgnispwent(const char *path, const char *login, struct passwd *pwd);

int addgrent(struct group * grp);
int delgrent(struct group * grp);
int chggrent(char const * login, struct group * grp);

int boolean_val(char const * str, int dflt);
char const *boolean_str(int val);
char *newstr(char const * p);

void pw_log(struct userconf * cnf, int mode, int which, char const * fmt,...) __printflike(4, 5);
char *pw_pwcrypt(char *password);

extern const char *Modes[];
extern const char *Which[];
