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

/*
 * Socket activation (§9): prowld binds and listens on configured sockets
 * at job load time.  On the first incoming connection or datagram, it
 * fork+execs the daemon and passes the bound socket fds via the
 * LISTEN_FDS / LISTEN_PID / LISTEN_FDNAMES protocol (systemd-compatible).
 *
 * udata encoding in kqueue EVFILT_READ events:
 *   bit 0 set  → prowl_socket_t * (socket activation fd)
 *   bit 0 clear, non-NULL → job_t * (sd_notify datagram socket)
 *   NULL → IPC client fd
 */

#include <sys/event.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "prowld.h"

/*
 * Bind and listen on a single socket entry.
 * Sets sock->fd on success.  Returns 0 on success, -1 on failure.
 */
static int
socket_bind_one(prowl_socket_t *sock)
{
	struct sockaddr_un	sun;
	struct sockaddr_in	sin;
	struct sockaddr_in6	sin6;
	struct sockaddr		*sa;
	socklen_t		salen;
	int			fd, on;

	fd = socket(sock->family, sock->socktype, 0);
	if (fd == -1) {
		prowl_log(LOG_WARNING, "socket_bind_one socket: %m");
		return (-1);
	}

	/*
	 * Set FD_CLOEXEC so this fd is not inherited by non-socket-activated
	 * children.  We explicitly dup2 it to 3+ in socket-activated children.
	 */
	fcntl(fd, F_SETFD, FD_CLOEXEC);

	on = 1;
	if (sock->family != AF_UNIX)
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	switch (sock->family) {
	case AF_UNIX:
		unlink(sock->path);
		memset(&sun, 0, sizeof(sun));
		sun.sun_family = AF_UNIX;
		strlcpy(sun.sun_path, sock->path, sizeof(sun.sun_path));
		sa    = (struct sockaddr *)&sun;
		salen = sizeof(sun);
		break;

	case AF_INET:
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port   = htons((uint16_t)sock->port);
		if (sock->host[0] != '\0')
			inet_pton(AF_INET, sock->host, &sin.sin_addr);
		else
			sin.sin_addr.s_addr = INADDR_ANY;
		sa    = (struct sockaddr *)&sin;
		salen = sizeof(sin);
		break;

	case AF_INET6:
		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_port   = htons((uint16_t)sock->port);
		if (sock->host[0] != '\0')
			inet_pton(AF_INET6, sock->host, &sin6.sin6_addr);
		else
			sin6.sin6_addr = in6addr_any;
		sa    = (struct sockaddr *)&sin6;
		salen = sizeof(sin6);
		break;

	default:
		close(fd);
		return (-1);
	}

	if (bind(fd, sa, salen) == -1) {
		prowl_log(LOG_WARNING, "socket_bind_one bind (%s): %m",
		    sock->name);
		close(fd);
		return (-1);
	}

	if (sock->socktype == SOCK_STREAM || sock->socktype == SOCK_SEQPACKET) {
		int bl = sock->backlog > 0 ? sock->backlog : SOMAXCONN;
		if (listen(fd, bl) == -1) {
			prowl_log(LOG_WARNING, "socket_bind_one listen (%s): %m",
			    sock->name);
			close(fd);
			return (-1);
		}
	}

	sock->fd = fd;
	return (0);
}

/*
 * Register EVFILT_READ for one socket fd.
 * udata has bit 0 set to distinguish from notify fds in the event loop.
 */
static void
socket_kqueue_add(prowl_socket_t *sock)
{
	struct kevent kev;
	uintptr_t udata = (uintptr_t)sock | 1UL;

	EV_SET(&kev, (uintptr_t)sock->fd, EVFILT_READ, EV_ADD,
	    0, 0, (void *)udata);
	if (kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1)
		prowl_log(LOG_WARNING, "socket_kqueue_add kevent (%s): %m",
		    sock->name);
}

/*
 * Remove EVFILT_READ for one socket fd.
 */
static void
socket_kqueue_del(prowl_socket_t *sock)
{
	struct kevent kev;

	if (sock->fd < 0)
		return;
	EV_SET(&kev, (uintptr_t)sock->fd, EVFILT_READ, EV_DELETE,
	    0, 0, NULL);
	kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL);
}

/*
 * Bind all sockets for a job and register them with kqueue.
 * On success, returns 0 and the job is ready for activation.
 * On failure, closes any already-bound fds, returns -1.
 */
int
socket_bind_all(job_t *job)
{
	int i;

	for (i = 0; i < job->sockets_count; i++) {
		if (socket_bind_one(&job->sockets[i]) == -1) {
			prowl_log(LOG_ERR,
			    "job %s: failed to bind socket '%s'",
			    job->label, job->sockets[i].name);
			/* Roll back already-bound fds */
			while (--i >= 0) {
				socket_kqueue_del(&job->sockets[i]);
				close(job->sockets[i].fd);
				job->sockets[i].fd = -1;
			}
			return (-1);
		}
		socket_kqueue_add(&job->sockets[i]);
	}

	prowl_log(LOG_INFO,
	    "job %s: %d socket(s) bound, waiting for first connection",
	    job->label, job->sockets_count);
	return (0);
}

/*
 * Called when any of a job's socket fds becomes readable.
 * Deregisters all socket watchers and launches the daemon.
 */
void
socket_handle_activation(prowl_socket_t *sock)
{
	job_t *job = sock->job;
	int i;

	if (job == NULL || job->socket_activated)
		return;

	/* Remove EVFILT_READ for all sockets — daemon takes them all over */
	for (i = 0; i < job->sockets_count; i++)
		socket_kqueue_del(&job->sockets[i]);

	supervisor_socket_activate(job);
}

/*
 * Re-register EVFILT_READ for all socket fds after the daemon exits.
 * Called from supervisor_reap when a socket-activated daemon exits
 * and keep_alive policy says to wait for the next connection.
 */
void
socket_rearm(job_t *job)
{
	int i;

	job->socket_activated = false;

	for (i = 0; i < job->sockets_count; i++) {
		if (job->sockets[i].fd >= 0)
			socket_kqueue_add(&job->sockets[i]);
	}

	prowl_log(LOG_INFO,
	    "job %s: socket re-armed, waiting for next connection",
	    job->label);
}

/*
 * Deregister, close, and unlink all sockets for a job.
 * Called when the job is permanently stopped or freed.
 */
void
socket_close_all(job_t *job)
{
	int i;

	for (i = 0; i < job->sockets_count; i++) {
		prowl_socket_t *s = &job->sockets[i];

		socket_kqueue_del(s);

		if (s->fd >= 0) {
			close(s->fd);
			s->fd = -1;
		}

		if (s->family == AF_UNIX && s->path[0] != '\0')
			unlink(s->path);
	}
}
