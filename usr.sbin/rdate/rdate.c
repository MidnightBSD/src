/* 	$MidnightBSD: src/usr.sbin/rdate/rdate.c,v 1.6 2013/01/03 02:58:09 laffer1 Exp $ */
/*	$OpenBSD: rdate.c,v 1.24 2009/10/27 23:59:54 deraadt Exp $	*/
/*	$NetBSD: rdate.c,v 1.4 1996/03/16 12:37:45 pk Exp $	*/

/*
 * Copyright (c) 1994 Christos Zoulas
 * All rights reserved.
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
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * rdate.c: Set the date from the specified host
 *
 *	Uses the rfc868 time protocol at socket 37.
 *	Time is returned as the number of seconds since
 *	midnight January 1st 1900.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include <stdio.h>
#include <ctype.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* there are systems without libutil; for portability */
#ifndef NO_UTIL
#include <libutil.h>
#else
#define logwtmp(a,b,c)
#endif

void rfc868time_client (const char *, int, struct timeval *, struct timeval *, int);
void ntp_client (const char *, int, struct timeval *, struct timeval *, int);
static void usage(void);

extern char    *__progname;

static void
usage(void)
{
	(void) fprintf(stderr, "usage: %s [-46acnpsv] host\n", __progname);
	(void) fprintf(stderr,
	    "  -4: use IPv4 only\n"
	    "  -6: use IPv6 only\n"
	    "  -a: use adjtime instead of instant change\n"
	    "  -c: correct leap second count\n"
	    "  -n: use SNTP instead of RFC868 time protocol\n"
	    "  -p: just print, don't set\n"
	    "  -s: just set, don't print\n"
	    "  -v: verbose output\n");
}

int
main(int argc, char **argv)
{
	int             pr = 0, silent = 0, ntp = 0, verbose = 0;
	int		slidetime = 0, corrleaps = 0;
	char           *hname;
	int             c;
	int		family = PF_UNSPEC;

	struct timeval new, adjust;

	while ((c = getopt(argc, argv, "46psancv")) != -1)
		switch (c) {
		case '4':
			family = PF_INET;
			break;

		case '6':
			family = PF_INET6;
			break;

		case 'p':
			pr++;
			break;

		case 's':
			silent++;
			break;

		case 'a':
			slidetime++;
			break;

		case 'n':
			ntp++;
			break;

		case 'c':
			corrleaps = 1;
			break;

		case 'v':
			verbose++;
			break;

		default:
			usage();
			return 1;
		}

	if (argc - 1 != optind) {
		usage();
		return 1;
	}
	hname = argv[optind];

	if (ntp)
		ntp_client(hname, family, &new, &adjust, corrleaps);
	else
		rfc868time_client(hname, family, &new, &adjust, corrleaps);

	if (!pr) {
		if (!slidetime) {
			logwtmp("|", "date", "");
			if (settimeofday(&new, NULL) == -1)
				err(1, "Could not set time of day");
			logwtmp("{", "date", "");
		} else {
			if (adjtime(&adjust, NULL) == -1)
				err(1, "Could not adjust time of day");
		}
	}

	if (!silent) {
		struct tm      *ltm;
		char		buf[80];
		time_t		tim = new.tv_sec;
		double		adjsec;

		ltm = localtime(&tim);
		(void) strftime(buf, sizeof buf, "%a %b %e %H:%M:%S %Z %Y\n", ltm);
		(void) fputs(buf, stdout);

		adjsec  = adjust.tv_sec + adjust.tv_usec / 1.0e6;

		if (slidetime || verbose) {
			if (ntp)
				(void) fprintf(stdout,
				   "%s: adjust local clock by %.6f seconds\n",
				   __progname, adjsec);
			else
				(void) fprintf(stdout,
				   "%s: adjust local clock by %d seconds\n",
				   __progname, adjust.tv_sec);
		}
	}

	return 0;
}
