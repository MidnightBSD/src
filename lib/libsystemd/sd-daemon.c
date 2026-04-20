/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Lucas Holt
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
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <errno.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sd-daemon.h"

int
sd_listen_fds(int unset_environment)
{
	const char *e;
	int n;
	pid_t pid;

	e = getenv("LISTEN_PID");
	if (e == NULL)
		return (0);

	pid = (pid_t)strtol(e, NULL, 10);
	if (pid != getpid())
		return (0);

	e = getenv("LISTEN_FDS");
	if (e == NULL)
		return (0);

	n = (int)strtol(e, NULL, 10);
	if (n <= 0)
		return (0);

	if (unset_environment) {
		unsetenv("LISTEN_PID");
		unsetenv("LISTEN_FDS");
		unsetenv("LISTEN_FDNAMES");
	}

	return (n);
}

int
sd_listen_fds_with_names(int unset_environment, char ***names)
{
	int n;
	const char *e;

	n = sd_listen_fds(false);
	if (n <= 0)
		return (n);

	if (names) {
		char **l;
		char *s, *p, *tok;

		l = calloc((size_t)n + 1, sizeof(char *));
		if (!l)
			return (-ENOMEM);

		e = getenv("LISTEN_FDNAMES");
		if (e) {
			s = strdup(e);
			if (!s) {
				free(l);
				return (-ENOMEM);
			}

			p = s;
			for (int i = 0; i < n; i++) {
				tok = strsep(&p, ":");
				if (tok)
					l[i] = strdup(tok);
				else
					l[i] = strdup("unknown");
			}
			free(s);
		} else {
			for (int i = 0; i < n; i++)
				l[i] = strdup("unknown");
		}
		*names = l;
	}

	if (unset_environment) {
		unsetenv("LISTEN_PID");
		unsetenv("LISTEN_FDS");
		unsetenv("LISTEN_FDNAMES");
	}

	return (n);
}

int
sd_is_socket(int fd, int family, int type, int listening)
{
	struct stat st;
	int f;
	socklen_t l;

	if (fd < 0)
		return (-EBADF);

	if (fstat(fd, &st) < 0)
		return (-errno);

	if (!S_ISSOCK(st.st_mode))
		return (0);

	if (family != AF_UNSPEC) {
		l = sizeof(f);
		if (getsockopt(fd, SOL_SOCKET, SO_DOMAIN, &f, &l) < 0)
			return (-errno);
		if (f != family)
			return (0);
	}

	if (type != 0) {
		l = sizeof(f);
		if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &f, &l) < 0)
			return (-errno);
		if (f != type)
			return (0);
	}

	if (listening >= 0) {
		l = sizeof(f);
		if (getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &f, &l) < 0)
			return (-errno);
		if (!!f != !!listening)
			return (0);
	}

	return (1);
}

int
sd_is_socket_inet(int fd, int family, int type, int listening, uint16_t port)
{
	struct sockaddr_storage sa;
	socklen_t l = sizeof(sa);
	int r;

	r = sd_is_socket(fd, family, type, listening);
	if (r <= 0)
		return (r);

	if (family != AF_INET && family != AF_INET6 && family != AF_UNSPEC)
		return (0);

	if (getsockname(fd, (struct sockaddr *)&sa, &l) < 0)
		return (-errno);

	if (family != AF_UNSPEC && sa.ss_family != family)
		return (0);

	if (port > 0) {
		if (sa.ss_family == AF_INET) {
			if (ntohs(((struct sockaddr_in *)&sa)->sin_port) != port)
				return (0);
		} else if (sa.ss_family == AF_INET6) {
			if (ntohs(((struct sockaddr_in6 *)&sa)->sin6_port) != port)
				return (0);
		} else
			return (0);
	}

	return (1);
}

int
sd_is_socket_unix(int fd, int type, int listening, const char *path, size_t length)
{
	struct sockaddr_un sun;
	socklen_t l = sizeof(sun);
	int r;

	r = sd_is_socket(fd, AF_UNIX, type, listening);
	if (r <= 0)
		return (r);

	if (path) {
		if (getsockname(fd, (struct sockaddr *)&sun, &l) < 0)
			return (-errno);

		if (length == 0)
			length = strlen(path);

		if (l < offsetof(struct sockaddr_un, sun_path) + length)
			return (0);

		if (memcmp(sun.sun_path, path, length) != 0)
			return (0);
	}

	return (1);
}

int
sd_notify(int unset_environment, const char *state)
{
	return (sd_pid_notify(0, unset_environment, state));
}

int
sd_notifyf(int unset_environment, const char *format, ...)
{
	va_list ap;
	char *p = NULL;
	int r;

	va_start(ap, format);
	r = vasprintf(&p, format, ap);
	va_end(ap);

	if (r < 0 || !p)
		return (-ENOMEM);

	r = sd_notify(unset_environment, p);
	free(p);

	return (r);
}

int
sd_pid_notify(pid_t pid, int unset_environment, const char *state)
{
	const char *e;
	struct sockaddr_un sun;
	int fd, r;

	if (!state)
		return (-EINVAL);

	e = getenv("NOTIFY_SOCKET");
	if (!e)
		return (0);

	if (unset_environment)
		unsetenv("NOTIFY_SOCKET");

	/* Systemd supports @ for abstract namespace, MidnightBSD doesn't.
	 * We only support normal filesystem paths for now. */
	if (e[0] != '/')
		return (-EAFNOSUPPORT);

	fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return (-errno);

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, e, sizeof(sun.sun_path) - 1);

	/* If pid is 0, it means the current process */
	if (pid == 0)
		pid = getpid();

	/* In a real sd_notify, we'd prepend MAINPID=pid if pid != getpid()
	 * but prowld's wire protocol is simple for now. */

	if (sendto(fd, state, strlen(state), 0, (struct sockaddr *)&sun, sizeof(sun)) < 0) {
		r = -errno;
		goto out;
	}

	r = 1;

out:
	close(fd);
	return (r);
}

int
sd_booted(void)
{
	struct stat st;

	/* 
	 * Systemd checks for /run/systemd/system. 
	 * Prowld checks for its control socket.
	 */
	if (stat("/var/run/prowld/prowld.sock", &st) == 0 && S_ISSOCK(st.st_mode))
		return (1);

	return (0);
}

int
sd_watchdog_enabled(int unset_environment, uint64_t *usec)
{
	const char *e;
	uint64_t u;
	pid_t pid;

	e = getenv("WATCHDOG_PID");
	if (e) {
		pid = (pid_t)strtol(e, NULL, 10);
		if (pid != getpid())
			return (0);
	}

	e = getenv("WATCHDOG_USEC");
	if (!e)
		return (0);

	u = (uint64_t)strtoull(e, NULL, 10);
	if (u == 0)
		return (0);

	if (usec)
		*usec = u;

	if (unset_environment) {
		unsetenv("WATCHDOG_PID");
		unsetenv("WATCHDOG_USEC");
	}

	return (1);
}
