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
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD: release/10.0.0/usr.sbin/pw/pwupd.c 244711 2012-12-26 18:28:17Z bapt $";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <pwd.h>
#include <libutil.h>
#include <errno.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/wait.h>

#include "pwupd.h"

#define HAVE_PWDB_C	1
#define	HAVE_PWDB_U	1

static char pathpwd[] = _PATH_PWD;
static char * pwpath = pathpwd;
 
int
setpwdir(const char * dir)
{
	if (dir == NULL)
		return -1;
	else
		pwpath = strdup(dir);
	if (pwpath == NULL)
		return -1;
	return 0;
}

char *
getpwpath(char const * file)
{
	static char pathbuf[MAXPATHLEN];

	snprintf(pathbuf, sizeof pathbuf, "%s/%s", pwpath, file);
	return pathbuf;
}

static int
pwdb(char *arg,...)
{
	int             i = 0;
	pid_t           pid;
	va_list         ap;
	char           *args[10];

	args[i++] = _PATH_PWD_MKDB;
	va_start(ap, arg);
	while (i < 6 && arg != NULL) {
		args[i++] = arg;
		arg = va_arg(ap, char *);
	}
	if (pwpath != pathpwd) {
		args[i++] = "-d";
		args[i++] = pwpath;
	}
	args[i++] = getpwpath(_MASTERPASSWD);
	args[i] = NULL;

	if ((pid = fork()) == -1)	/* Error (errno set) */
		i = errno;
	else if (pid == 0) {	/* Child */
		execv(args[0], args);
		_exit(1);
	} else {		/* Parent */
		waitpid(pid, &i, 0);
		if (WEXITSTATUS(i))
			i = EIO;
	}
	va_end(ap);
	return i;
}

static int
pw_update(struct passwd * pwd, char const * user)
{
	int             rc = 0;

	/*
	 * First, let's check the see if the database is alright
	 * Note: -C is only available in FreeBSD 2.2 and above
	 */
#ifdef HAVE_PWDB_C
	rc = pwdb("-C", (char *)NULL);	/* Check only */
	if (rc == 0) {
#else
	{				/* No -C */
#endif
		int pfd, tfd;
		struct passwd *pw = NULL;
		struct passwd *old_pw = NULL;

	       	if (pwd != NULL)
		       pw = pw_dup(pwd);

		if (user != NULL)
			old_pw = GETPWNAM(user);

		if (pw_init(pwpath, NULL))
			err(1, "pw_init()");
		if ((pfd = pw_lock()) == -1) {
			pw_fini();
			err(1, "pw_lock()");
		}
		if ((tfd = pw_tmp(-1)) == -1) {
			pw_fini();
			err(1, "pw_tmp()");
		}
		if (pw_copy(pfd, tfd, pw, old_pw) == -1) {
			pw_fini();
			err(1, "pw_copy()");
		}
		/*
		 * in case of deletion of a user, the whole database
		 * needs to be regenerated
		 */
		if (pw_mkdb(pw != NULL ? user : NULL) == -1) {
			pw_fini();
			err(1, "pw_mkdb()");
		}
		free(pw);
		pw_fini();
	}
	return 0;
}

int
addpwent(struct passwd * pwd)
{
	return pw_update(pwd, NULL);
}

int
chgpwent(char const * login, struct passwd * pwd)
{
	return pw_update(pwd, login);
}

int
delpwent(struct passwd * pwd)
{
	char login[MAXLOGNAME];
	
	strlcpy(login, pwd->pw_name, MAXLOGNAME);
	return pw_update(NULL, login);
}
