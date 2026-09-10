/*	$NetBSD: flock.c,v 1.12 2019/10/04 16:27:00 mrg Exp $	*/

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/file.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <paths.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static const struct option flock_longopts[] = {
	{ "debug", no_argument, NULL, 'd' },
	{ "help", no_argument, NULL, 'h' },
	{ "nonblock", no_argument, NULL, 'n' },
	{ "nb", no_argument, NULL, 'n' },
	{ "close", no_argument, NULL, 'o' },
	{ "shared", no_argument, NULL, 's' },
	{ "exclusive", no_argument, NULL, 'x' },
	{ "unlock", no_argument, NULL, 'u' },
	{ "verbose", no_argument, NULL, 'v' },
	{ "wait", required_argument, NULL, 'w' },
	{ "timeout", required_argument, NULL, 'w' },
	{ NULL, 0, NULL, 0 },
};

static volatile sig_atomic_t timeout_expired;

static char	lockchar(int);
static const char *lockname(int);
static int	option_is_command(const char *);
static void	sigalrm(int);
static void	start_timer(double);
static void	stop_timer(void);
static void __dead2 __printflike(1, 2) usage(const char *, ...);

static void __dead2
usage(const char *fmt, ...)
{
	va_list ap;

	if (fmt != NULL) {
		va_start(ap, fmt);
		fprintf(stderr, "%s: ", getprogname());
		vfprintf(stderr, fmt, ap);
		fputc('\n', stderr);
		va_end(ap);
	}
	fprintf(stderr,
	    "usage: %s [-dhnosvx] [-w timeout] file|directory "
	    "command [args ...]\n"
	    "       %s [-dhnosvx] [-w timeout] file|directory "
	    "-c command\n"
	    "       %s [-dhnsuvx] [-w timeout] number\n",
	    getprogname(), getprogname(), getprogname());
	exit(EXIT_FAILURE);
}

static void
sigalrm(int signo)
{

	(void)signo;
	timeout_expired = 1;
}

static void
start_timer(double timeout)
{
	struct itimerval timer;
	struct sigaction sa;
	double fraction;

	timerclear(&timer.it_interval);
	timer.it_value.tv_sec = (time_t)timeout;
	fraction = timeout - (double)timer.it_value.tv_sec;
	timer.it_value.tv_usec = (suseconds_t)(fraction * 1000000.0);
	if (!timerisset(&timer.it_value))
		timer.it_value.tv_usec = 1;

	sa.sa_handler = sigalrm;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGALRM, &sa, NULL) == -1)
		err(EXIT_FAILURE, "sigaction");
	if (setitimer(ITIMER_REAL, &timer, NULL) == -1)
		err(EXIT_FAILURE, "setitimer");
}

static void
stop_timer(void)
{
	struct itimerval timer;

	timerclear(&timer.it_interval);
	timerclear(&timer.it_value);
	if (setitimer(ITIMER_REAL, &timer, NULL) == -1)
		err(EXIT_FAILURE, "setitimer");
}

static const char *
lockname(int lock)
{

	switch (lock & ~LOCK_NB) {
	case LOCK_SH:
		return ((lock & LOCK_NB) != 0 ? "shared, nonblocking" :
		    "shared");
	case LOCK_EX:
		return ((lock & LOCK_NB) != 0 ? "exclusive, nonblocking" :
		    "exclusive");
	case LOCK_UN:
		return ((lock & LOCK_NB) != 0 ? "unlock, nonblocking" :
		    "unlock");
	default:
		return ("invalid");
	}
}

static char
lockchar(int lock)
{

	switch (lock & ~LOCK_NB) {
	case LOCK_SH:
		return ('s');
	case LOCK_EX:
		return ('x');
	case LOCK_UN:
		return ('u');
	default:
		return ('?');
	}
}

static int
option_is_command(const char *arg)
{

	return ((arg[0] == '-' && arg[1] == 'c' && arg[2] == '\0') ||
	    (arg[0] == '-' && arg[1] == '-' && arg[2] == 'c' &&
	    arg[3] == 'o' && arg[4] == 'm' && arg[5] == 'm' &&
	    arg[6] == 'a' && arg[7] == 'n' && arg[8] == 'd' &&
	    arg[9] == '\0'));
}

int
main(int argc, char *argv[])
{
	char *end;
	char *shell_argv[] = { _PATH_BSHELL, "-c", NULL, NULL };
	char **cmdargv;
	double timeout;
	long descriptor;
	int ch, closefd, debug, fd, lock, timer_started, timeout_set, verbose;

	closefd = debug = timeout_set = verbose = 0;
	cmdargv = NULL;
	fd = -1;
	lock = 0;
	timeout = 0.0;
	timer_started = 0;

	while ((ch = getopt_long(argc, argv, "+dhnosuvw:x",
	    flock_longopts, NULL)) != -1) {
		switch (ch) {
		case 'd':
			debug = 1;
			break;
		case 'h':
			usage(NULL);
		case 'n':
			lock |= LOCK_NB;
			break;
		case 'o':
			closefd = 1;
			break;
		case 's':
			if ((lock & ~LOCK_NB) != 0 &&
			    (lock & ~LOCK_NB) != LOCK_SH) {
				usage("-%c cannot be used with -s",
				    lockchar(lock));
			}
			lock |= LOCK_SH;
			break;
		case 'u':
			if ((lock & ~LOCK_NB) != 0 &&
			    (lock & ~LOCK_NB) != LOCK_UN) {
				usage("-%c cannot be used with -u",
				    lockchar(lock));
			}
			lock |= LOCK_UN;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'w':
			errno = 0;
			timeout = strtod(optarg, &end);
			if (errno == ERANGE || end == optarg || *end != '\0' ||
			    !isfinite(timeout) || timeout < 0.0 ||
			    timeout > (double)INT_MAX)
				usage("invalid timeout: %s", optarg);
			timeout_set = 1;
			break;
		case 'x':
			if ((lock & ~LOCK_NB) != 0 &&
			    (lock & ~LOCK_NB) != LOCK_EX) {
				usage("-%c cannot be used with -x",
				    lockchar(lock));
			}
			lock |= LOCK_EX;
			break;
		default:
			usage(NULL);
		}
	}
	argc -= optind;
	argv += optind;

	if ((lock & ~LOCK_NB) == 0)
		lock |= LOCK_EX;
	if (timeout_set && timeout == 0.0)
		lock |= LOCK_NB;

	if (argc == 0)
		usage("missing lock file or descriptor");
	if (argc == 1) {
		if (closefd)
			usage("-o is not valid for a descriptor");
		errno = 0;
		descriptor = strtol(argv[0], &end, 0);
		if (errno == ERANGE || end == argv[0] || *end != '\0' ||
		    descriptor < 0 || descriptor > INT_MAX)
			errx(EXIT_FAILURE, "invalid file descriptor: %s",
			    argv[0]);
		fd = (int)descriptor;
	} else {
		if ((lock & ~LOCK_NB) == LOCK_UN)
			usage("-u is valid only for a descriptor");
		if (option_is_command(argv[1])) {
			if (argc != 3)
				usage("-c requires one command argument");
			shell_argv[2] = argv[2];
			cmdargv = shell_argv;
		} else {
			cmdargv = argv + 1;
		}
		fd = open(argv[0], O_RDONLY);
		if (fd == -1 && errno == ENOENT)
			fd = open(argv[0], O_RDWR | O_CREAT, 0600);
		if (fd == -1)
			err(EXIT_FAILURE, "%s", argv[0]);
	}

	if (debug) {
		if (cmdargv == NULL)
			fprintf(stderr, "descriptor %d: %s lock\n", fd,
			    lockname(lock));
		else
			fprintf(stderr, "%s: %s lock; command %s\n", argv[0],
			    lockname(lock), cmdargv[0]);
	}

	if (timeout_set && timeout > 0.0 && (lock & LOCK_NB) == 0) {
		start_timer(timeout);
		timer_started = 1;
	}
	while (flock(fd, lock) == -1) {
		if (errno == EINTR && !timeout_expired)
			continue;
		if (verbose)
			err(EXIT_FAILURE, "flock(%d, %s)", fd, lockname(lock));
		return (EXIT_FAILURE);
	}
	if (timer_started)
		stop_timer();

	if (closefd && close(fd) == -1)
		err(EXIT_FAILURE, "close");
	if (cmdargv != NULL) {
		execvp(cmdargv[0], cmdargv);
		err(EXIT_FAILURE, "%s", cmdargv[0]);
	}
	return (EXIT_SUCCESS);
}
