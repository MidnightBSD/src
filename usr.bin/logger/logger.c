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
static const char copyright[] =
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#if 0
#ifndef lint
static char sccsid[] = "@(#)logger.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <ctype.h>
#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	SYSLOG_NAMES
#include <syslog.h>

int	decode(char *, CODE *);
int	pencode(char *);
static void	logmessage(int, const char *, const char *, const char *,
			   const char *);
static void	usage(void);

struct socks {
    int sock;
    int addrlen;
    struct sockaddr_storage addr;
};

#ifdef INET6
int	family = PF_UNSPEC;	/* protocol family (IPv4, IPv6 or both) */
#else
int	family = PF_INET;	/* protocol family (IPv4 only) */
#endif
int	send_to_all = 0;	/* send message to all IPv4/IPv6 addresses */

/*
 * logger -- read and log utility
 *
 *	Reads from an input and arranges to write the result on the system
 *	log.
 */
int
main(int argc, char *argv[])
{
	int ch, logflags, pri;
	char *tag, *host, buf[1024];
	const char *svcname;

	tag = NULL;
	host = NULL;
	svcname = "syslog";
	pri = LOG_USER | LOG_NOTICE;
	logflags = 0;
	unsetenv("TZ");
	while ((ch = getopt(argc, argv, "46Af:h:iP:p:st:")) != -1)
		switch((char)ch) {
		case '4':
			family = PF_INET;
			break;
#ifdef INET6
		case '6':
			family = PF_INET6;
			break;
#endif
		case 'A':
			send_to_all++;
			break;
		case 'f':		/* file to log */
			if (freopen(optarg, "r", stdin) == NULL)
				err(1, "%s", optarg);
			setvbuf(stdin, 0, _IONBF, 0);
			break;
		case 'h':		/* hostname to deliver to */
			host = optarg;
			break;
		case 'i':		/* log process id also */
			logflags |= LOG_PID;
			break;
		case 'P':		/* service name or port number */
			svcname = optarg;
			break;
		case 'p':		/* priority */
			pri = pencode(optarg);
			break;
		case 's':		/* log to standard error */
			logflags |= LOG_PERROR;
			break;
		case 't':		/* tag */
			tag = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (tag == NULL)
		tag = getlogin();
	/* setup for logging */
	if (host == NULL)
		openlog(tag, logflags, 0);
	(void) fclose(stdout);

	/* log input line if appropriate */
	if (argc > 0) {
		char *p, *endp;
		size_t len;

		for (p = buf, endp = buf + sizeof(buf) - 2; *argv;) {
			len = strlen(*argv);
			if (p + len > endp && p > buf) {
				logmessage(pri, tag, host, svcname, buf);
				p = buf;
			}
			if (len > sizeof(buf) - 1)
				logmessage(pri, tag, host, svcname, *argv++);
			else {
				if (p != buf)
					*p++ = ' ';
				bcopy(*argv++, p, len);
				*(p += len) = '\0';
			}
		}
		if (p != buf)
			logmessage(pri, tag, host, svcname, buf);
	} else
		while (fgets(buf, sizeof(buf), stdin) != NULL)
			logmessage(pri, tag, host, svcname, buf);
	exit(0);
}

/*
 *  Send the message to syslog, either on the local host, or on a remote host
 */
void
logmessage(int pri, const char *tag, const char *host, const char *svcname,
	   const char *buf)
{
	static struct socks *socks;
	static int nsock = 0;
	struct addrinfo hints, *res, *r;
	char *line;
	int maxs, len, sock, error, i, lsent;

	if (host == NULL) {
		syslog(pri, "%s", buf);
		return;
	}

	if (nsock <= 0) {	/* set up socket stuff */
		/* resolve hostname */
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = family;
		hints.ai_socktype = SOCK_DGRAM;
		error = getaddrinfo(host, svcname, &hints, &res);
		if (error == EAI_SERVICE) {
			warnx("%s/udp: unknown service", svcname);
			error = getaddrinfo(host, "514", &hints, &res);
		}
		if (error)
			errx(1, "%s: %s", gai_strerror(error), host);
		/* count max number of sockets we may open */
		for (maxs = 0, r = res; r; r = r->ai_next, maxs++);
		socks = malloc(maxs * sizeof(struct socks));
		if (!socks)
			errx(1, "couldn't allocate memory for sockets");
		for (r = res; r; r = r->ai_next) {
			sock = socket(r->ai_family, r->ai_socktype,
				      r->ai_protocol);
			if (sock < 0)
				continue;
			memcpy(&socks[nsock].addr, r->ai_addr, r->ai_addrlen);
			socks[nsock].addrlen = r->ai_addrlen;
			socks[nsock++].sock = sock;
		}
		freeaddrinfo(res);
		if (nsock <= 0)
			errx(1, "socket");
	}

	if ((len = asprintf(&line, "<%d>%s: %s", pri, tag, buf)) == -1)
		errx(1, "asprintf");

	lsent = -1;
	for (i = 0; i < nsock; ++i) {
		lsent = sendto(socks[i].sock, line, len, 0,
			       (struct sockaddr *)&socks[i].addr,
			       socks[i].addrlen);
		if (lsent == len && !send_to_all)
			break;
	}
	if (lsent != len) {
		if (lsent == -1)
			warn ("sendto");
		else
			warnx ("sendto: short send - %d bytes", lsent);
	}

	free(line);
}

/*
 *  Decode a symbolic name to a numeric value
 */
int
pencode(char *s)
{
	char *save;
	int fac, lev;

	for (save = s; *s && *s != '.'; ++s);
	if (*s) {
		*s = '\0';
		fac = decode(save, facilitynames);
		if (fac < 0)
			errx(1, "unknown facility name: %s", save);
		*s++ = '.';
	}
	else {
		fac = 0;
		s = save;
	}
	lev = decode(s, prioritynames);
	if (lev < 0)
		errx(1, "unknown priority name: %s", save);
	return ((lev & LOG_PRIMASK) | (fac & LOG_FACMASK));
}

int
decode(char *name, CODE *codetab)
{
	CODE *c;

	if (isdigit(*name))
		return (atoi(name));

	for (c = codetab; c->c_name; c++)
		if (!strcasecmp(name, c->c_name))
			return (c->c_val);

	return (-1);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: %s\n",
	    "logger [-46Ais] [-f file] [-h host] [-P port] [-p pri] [-t tag]\n"
	    "              [message ...]"
	    );
	exit(1);
}
