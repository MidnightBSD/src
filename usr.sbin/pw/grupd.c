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
  "$FreeBSD: release/10.0.0/usr.sbin/pw/grupd.c 243898 2012-12-05 13:56:56Z eadler $";
#endif /* not lint */

#include <grp.h>
#include <libutil.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

#include "pwupd.h"

static char * grpath = _PATH_PWD;

int
setgrdir(const char * dir)
{
	if (dir == NULL)
		return -1;
	else
		grpath = strdup(dir);
	if (grpath == NULL)
		return -1;

	return 0;
}

char *
getgrpath(const char * file)
{
	static char pathbuf[MAXPATHLEN];

	snprintf(pathbuf, sizeof pathbuf, "%s/%s", grpath, file);
	return pathbuf;
}

static int
gr_update(struct group * grp, char const * group)
{
	int pfd, tfd;
	struct group *gr = NULL;
	struct group *old_gr = NULL;

	if (grp != NULL)
		gr = gr_dup(grp);

	if (group != NULL)
		old_gr = GETGRNAM(group);

	if (gr_init(grpath, NULL))
		err(1, "gr_init()");

	if ((pfd = gr_lock()) == -1) {
		gr_fini();
		err(1, "gr_lock()");
	}
	if ((tfd = gr_tmp(-1)) == -1) {
		gr_fini();
		err(1, "gr_tmp()");
	}
	if (gr_copy(pfd, tfd, gr, old_gr) == -1) {
		gr_fini();
		err(1, "gr_copy()");
	}
	if (gr_mkdb() == -1) {
		gr_fini();
		err(1, "gr_mkdb()");
	}
	free(gr);
	gr_fini();
	return 0;
}


int
addgrent(struct group * grp)
{
	return gr_update(grp, NULL);
}

int
chggrent(char const * login, struct group * grp)
{
	return gr_update(grp, login);
}

int
delgrent(struct group * grp)
{
	char group[MAXLOGNAME];

	strlcpy(group, grp->gr_name, MAXLOGNAME);

	return gr_update(NULL, group);
}
