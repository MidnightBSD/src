/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)process.c	8.2 (Berkeley) 11/16/93";
#endif
static const char rcsid[] =
  "$FreeBSD: release/7.0.0/libexec/talkd/process.c 145967 2005-05-06 15:28:54Z delphij $";
#endif /* not lint */

/*
 * process.c handles the requests, which can be of three types:
 *	ANNOUNCE - announce to a user that a talk is wanted
 *	LEAVE_INVITE - insert the request into the table
 *	LOOK_UP - look up to see if a request is waiting in
 *		  in the table for the local user
 *	DELETE - delete invitation
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <protocols/talkd.h>
#include <ctype.h>
#include <err.h>
#include <netdb.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "extern.h"

extern int debug;

void
process_request(CTL_MSG *mp, CTL_RESPONSE *rp)
{
	CTL_MSG *ptr;
	char *s;

	rp->vers = TALK_VERSION;
	rp->type = mp->type;
	rp->id_num = htonl(0);
	if (mp->vers != TALK_VERSION) {
		syslog(LOG_WARNING, "bad protocol version %d", mp->vers);
		rp->answer = BADVERSION;
		return;
	}
	mp->id_num = ntohl(mp->id_num);
	mp->addr.sa_family = ntohs(mp->addr.sa_family);
	if (mp->addr.sa_family != AF_INET) {
		syslog(LOG_WARNING, "bad address, family %d",
		    mp->addr.sa_family);
		rp->answer = BADADDR;
		return;
	}
	mp->ctl_addr.sa_family = ntohs(mp->ctl_addr.sa_family);
	if (mp->ctl_addr.sa_family != AF_INET) {
		syslog(LOG_WARNING, "bad control address, family %d",
		    mp->ctl_addr.sa_family);
		rp->answer = BADCTLADDR;
		return;
	}
	for (s = mp->l_name; *s; s++)
		if (!isprint(*s)) {
			syslog(LOG_NOTICE, "illegal user name. Aborting");
			rp->answer = FAILED;
			return;
		}
	mp->pid = ntohl(mp->pid);
	if (debug)
		print_request("process_request", mp);
	switch (mp->type) {

	case ANNOUNCE:
		do_announce(mp, rp);
		break;

	case LEAVE_INVITE:
		ptr = find_request(mp);
		if (ptr != (CTL_MSG *)0) {
			rp->id_num = htonl(ptr->id_num);
			rp->answer = SUCCESS;
		} else
			insert_table(mp, rp);
		break;

	case LOOK_UP:
		ptr = find_match(mp);
		if (ptr != (CTL_MSG *)0) {
			rp->id_num = htonl(ptr->id_num);
			rp->addr = ptr->addr;
			rp->addr.sa_family = htons(ptr->addr.sa_family);
			rp->answer = SUCCESS;
		} else
			rp->answer = NOT_HERE;
		break;

	case DELETE:
		rp->answer = delete_invite(mp->id_num);
		break;

	default:
		rp->answer = UNKNOWN_REQUEST;
		break;
	}
	if (debug)
		print_response("process_request", rp);
}

void
do_announce(CTL_MSG *mp, CTL_RESPONSE *rp)
{
	struct hostent *hp;
	CTL_MSG *ptr;
	int result;

	/* see if the user is logged */
	result = find_user(mp->r_name, mp->r_tty);
	if (result != SUCCESS) {
		rp->answer = result;
		return;
	}
#define	satosin(sa)	((struct sockaddr_in *)(sa))
	hp = gethostbyaddr((char *)&satosin(&mp->ctl_addr)->sin_addr,
		sizeof (struct in_addr), AF_INET);
	if (hp == (struct hostent *)0) {
		rp->answer = MACHINE_UNKNOWN;
		return;
	}
	ptr = find_request(mp);
	if (ptr == (CTL_MSG *) 0) {
		insert_table(mp, rp);
		rp->answer = announce(mp, hp->h_name);
		return;
	}
	if (mp->id_num > ptr->id_num) {
		/*
		 * This is an explicit re-announce, so update the id_num
		 * field to avoid duplicates and re-announce the talk.
		 */
		ptr->id_num = new_id();
		rp->id_num = htonl(ptr->id_num);
		rp->answer = announce(mp, hp->h_name);
	} else {
		/* a duplicated request, so ignore it */
		rp->id_num = htonl(ptr->id_num);
		rp->answer = SUCCESS;
	}
}

#include <utmp.h>

/*
 * Search utmp for the local user
 */
int
find_user(const char *name, char *tty)
{
	struct utmp ubuf;
	int status;
	FILE *fd;
	struct stat statb;
	time_t best = 0;
	char line[sizeof(ubuf.ut_line) + 1];
	char ftty[sizeof(_PATH_DEV) - 1 + sizeof(line)];

	if ((fd = fopen(_PATH_UTMP, "r")) == NULL) {
		warnx("can't read %s", _PATH_UTMP);
		return (FAILED);
	}
#define SCMPN(a, b)	strncmp(a, b, sizeof (a))
	status = NOT_HERE;
	(void) strcpy(ftty, _PATH_DEV);
	while (fread((char *) &ubuf, sizeof ubuf, 1, fd) == 1)
		if (SCMPN(ubuf.ut_name, name) == 0) {
			strncpy(line, ubuf.ut_line, sizeof(ubuf.ut_line));
			line[sizeof(ubuf.ut_line)] = '\0';
			if (*tty == '\0' || best != 0) {
				if (best == 0)
					status = PERMISSION_DENIED;
				/* no particular tty was requested */
				(void) strcpy(ftty + sizeof(_PATH_DEV) - 1,
				    line);
				if (stat(ftty, &statb) == 0) {
					if (!(statb.st_mode & 020))
						continue;
					if (statb.st_atime > best) {
						best = statb.st_atime;
						(void) strcpy(tty, line);
						status = SUCCESS;
						continue;
					}
				}
			}
			if (strcmp(line, tty) == 0) {
				status = SUCCESS;
				break;
			}
		}
	fclose(fd);
	return (status);
}
