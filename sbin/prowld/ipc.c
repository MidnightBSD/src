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
 * IPC server: Unix domain socket, length-prefixed JSON protocol.
 *
 * Wire format (per spec §14.2):
 *   [4-byte big-endian length][JSON payload]
 *
 * Incoming JSON is parsed with libucl.  Responses are built with snprintf.
 */

#include <sys/event.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <arpa/inet.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <ucl.h>

#include "prowld.h"

/* Exported for prowld.c to register with kqueue */
int ipc_listen_fd = -1;

/* Per-client state */
static ipc_client_t g_clients[IPC_MAX_CLIENTS];

/* ------------------------------------------------------------------ */
/* JSON response helpers                                                */
/* ------------------------------------------------------------------ */

static void
ipc_send(int fd, const char *json, size_t len)
{
	uint32_t nlen = htonl((uint32_t)len);
	send(fd, &nlen, 4, MSG_NOSIGNAL);
	send(fd, json, len, MSG_NOSIGNAL);
}

static void
ipc_send_ok(int fd, const char *id, const char *result_json)
{
	char buf[IPC_MSG_MAX];
	int n;

	if (result_json != NULL)
		n = snprintf(buf, sizeof(buf),
		    "{\"id\":\"%s\",\"status\":\"ok\",\"result\":%s}",
		    id != NULL ? id : "", result_json);
	else
		n = snprintf(buf, sizeof(buf),
		    "{\"id\":\"%s\",\"status\":\"ok\"}",
		    id != NULL ? id : "");

	if (n > 0 && (size_t)n < sizeof(buf))
		ipc_send(fd, buf, (size_t)n);
}

static void
ipc_send_error(int fd, const char *id, const char *message)
{
	char buf[512];
	int n;

	n = snprintf(buf, sizeof(buf),
	    "{\"id\":\"%s\",\"status\":\"error\",\"message\":\"%s\"}",
	    id != NULL ? id : "", message);
	if (n > 0 && (size_t)n < sizeof(buf))
		ipc_send(fd, buf, (size_t)n);
}

/* ------------------------------------------------------------------ */
/* Job serialisation                                                    */
/* ------------------------------------------------------------------ */

static int
job_to_json(const job_t *job, char *buf, size_t bufsz)
{
	return snprintf(buf, bufsz,
	    "{"
	    "\"label\":\"%s\","
	    "\"description\":\"%s\","
	    "\"type\":\"%s\","
	    "\"state\":\"%s\","
	    "\"pid\":%d,"
	    "\"rc_name\":\"%s\","
	    "\"enabled\":%s,"
	    "\"restart_count\":%d"
	    "}",
	    job->label,
	    job->description,
	    job_type_str(job->type),
	    job_state_str(job->state),
	    (int)job->pid,
	    job->rc_name,
	    job->enabled ? "true" : "false",
	    job->restart_count);
}

/* ------------------------------------------------------------------ */
/* sysrc helper: call /usr/sbin/sysrc safely via fork+execvp           */
/* ------------------------------------------------------------------ */

static void
run_sysrc(const char *varname, const char *value)
{
	pid_t pid;
	char assignment[PROWL_LABEL_MAX + 8];
	int status;

	snprintf(assignment, sizeof(assignment), "%s=%s", varname, value);

	pid = fork();
	if (pid == -1) {
		prowl_log(LOG_ERR, "run_sysrc fork: %m");
		return;
	}

	if (pid == 0) {
		/* Child: close kqueue fd */
		if (g_kqueue_fd >= 0)
			close(g_kqueue_fd);
		char *argv[] = {
			(char *)(uintptr_t)"/usr/sbin/sysrc",
			assignment,
			NULL
		};
		execvp("/usr/sbin/sysrc", argv);
		_exit(127);
	}

	waitpid(pid, &status, 0);
}

/* ------------------------------------------------------------------ */
/* Resolve a target label/name to a job                                */
/* ------------------------------------------------------------------ */

static job_t *
resolve_target(const ucl_object_t *req, int fd, const char *id)
{
	const ucl_object_t *tgt;
	const char *label;
	job_t *job;

	tgt = ucl_object_lookup(req, "target");
	if (tgt == NULL || ucl_object_type(tgt) != UCL_STRING) {
		ipc_send_error(fd, id, "missing target");
		return (NULL);
	}
	label = ucl_object_tostring(tgt);

	job = job_find_by_label(label);
	if (job == NULL)
		job = job_find_by_provides(label);
	if (job == NULL)
		job = job_find_by_rcname(label);

	if (job == NULL)
		ipc_send_error(fd, id, "job not found");

	return (job);
}

/* ------------------------------------------------------------------ */
/* Command handlers                                                     */
/* ------------------------------------------------------------------ */

static void
cmd_list(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;
	char buf[IPC_MSG_MAX];
	char jbuf[512];
	size_t used = 0;
	bool first = true;
	const ucl_object_t *filt, *sf;
	const char *state_filter = NULL;

	filt = ucl_object_lookup(req, "filter");
	if (filt != NULL) {
		sf = ucl_object_lookup(filt, "state");
		if (sf != NULL)
			state_filter = ucl_object_tostring(sf);
	}

	used += (size_t)snprintf(buf + used, sizeof(buf) - used, "[");

	TAILQ_FOREACH(job, &g_jobs, entries) {
		int n;

		if (state_filter != NULL &&
		    strcmp(job_state_str(job->state), state_filter) != 0)
			continue;

		if (!first) {
			if (used + 1 >= sizeof(buf))
				break;
			buf[used++] = ',';
		}
		first = false;

		n = job_to_json(job, jbuf, sizeof(jbuf));
		if (n <= 0 || used + (size_t)n >= sizeof(buf))
			break;
		memcpy(buf + used, jbuf, (size_t)n);
		used += (size_t)n;
	}

	if (used + 1 < sizeof(buf))
		buf[used++] = ']';
	buf[used] = '\0';

	ipc_send_ok(fd, id, buf);
}

static void
cmd_status(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;
	char jbuf[512];

	job = resolve_target(req, fd, id);
	if (job == NULL)
		return;

	job_to_json(job, jbuf, sizeof(jbuf));
	ipc_send_ok(fd, id, jbuf);
}

static void
cmd_start(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;

	job = resolve_target(req, fd, id);
	if (job == NULL)
		return;

	if (job->state == JOB_STATE_MASKED) {
		ipc_send_error(fd, id, "job is masked");
		return;
	}

	job->run_at_load = true;
	job->enabled = true;
	if (job->state != JOB_STATE_RUNNING &&
	    job->state != JOB_STATE_STARTING) {
		job_set_state(job, JOB_STATE_LOADED);
		if (supervisor_start(job) == 0)
			g_current_starts++;
	}

	ipc_send_ok(fd, id, NULL);
}

static void
cmd_stop(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;

	job = resolve_target(req, fd, id);
	if (job == NULL)
		return;

	supervisor_stop(job, false);
	ipc_send_ok(fd, id, NULL);
}

static void
cmd_restart(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;

	job = resolve_target(req, fd, id);
	if (job == NULL)
		return;

	supervisor_stop(job, false);
	job->run_at_load = true;
	ipc_send_ok(fd, id, NULL);
}

static void
cmd_reload(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;

	job = resolve_target(req, fd, id);
	if (job == NULL)
		return;

	if (job->state != JOB_STATE_RUNNING) {
		ipc_send_error(fd, id, "job not running");
		return;
	}

	supervisor_signal(job, SIGHUP);
	ipc_send_ok(fd, id, NULL);
}

static void
cmd_enable(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;
	char varname[PROWL_LABEL_MAX + 8];

	job = resolve_target(req, fd, id);
	if (job == NULL)
		return;

	snprintf(varname, sizeof(varname), "%s_enable", job->rc_name);
	run_sysrc(varname, "YES");
	job->enabled = true;

	ipc_send_ok(fd, id, NULL);
}

static void
cmd_disable(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;
	char varname[PROWL_LABEL_MAX + 8];

	job = resolve_target(req, fd, id);
	if (job == NULL)
		return;

	snprintf(varname, sizeof(varname), "%s_enable", job->rc_name);
	run_sysrc(varname, "NO");
	job->enabled = false;

	ipc_send_ok(fd, id, NULL);
}

static void
cmd_mask(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;
	char path[PROWL_PATH_MAX];

	job = resolve_target(req, fd, id);
	if (job == NULL)
		return;

	snprintf(path, sizeof(path), "%s/%s", PROWLD_MASK_DIR, job->label);
	if (symlink("/dev/null", path) == -1 && errno != EEXIST) {
		ipc_send_error(fd, id, "mask failed");
		return;
	}

	supervisor_stop(job, false);
	job_set_state(job, JOB_STATE_MASKED);
	ipc_send_ok(fd, id, NULL);
}

static void
cmd_unmask(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;
	char path[PROWL_PATH_MAX];

	job = resolve_target(req, fd, id);
	if (job == NULL)
		return;

	snprintf(path, sizeof(path), "%s/%s", PROWLD_MASK_DIR, job->label);
	unlink(path);

	if (job->state == JOB_STATE_MASKED)
		job_set_state(job, JOB_STATE_LOADED);

	ipc_send_ok(fd, id, NULL);
}

static void
cmd_show(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;
	char jbuf[512];

	job = resolve_target(req, fd, id);
	if (job == NULL)
		return;

	job_to_json(job, jbuf, sizeof(jbuf));
	ipc_send_ok(fd, id, jbuf);
}

static void
cmd_reload_config(int fd, const char *id, const ucl_object_t *req)
{
	(void)req;
	g_reload = true;
	ipc_send_ok(fd, id, NULL);
}

static void
cmd_daemon_reexec(int fd, const char *id, const ucl_object_t *req)
{
	(void)req;
	/* Phase 2: re-exec with state handoff file */
	ipc_send_error(fd, id, "daemon-reexec not yet implemented");
}

/* ------------------------------------------------------------------ */
/* Message dispatch                                                     */
/* ------------------------------------------------------------------ */

static void
ipc_dispatch_message(int fd, const char *json, size_t len)
{
	struct ucl_parser *parser;
	ucl_object_t *root;
	const ucl_object_t *obj;
	const char *verb = NULL;
	const char *id = "";

	parser = ucl_parser_new(UCL_PARSER_NO_FILEVARS);
	if (parser == NULL)
		return;

	if (!ucl_parser_add_chunk(parser, (const unsigned char *)json, len)) {
		prowl_log(LOG_WARNING, "ipc: parse error: %s",
		    ucl_parser_get_error(parser));
		ucl_parser_free(parser);
		ipc_send_error(fd, "", "JSON parse error");
		return;
	}

	root = ucl_parser_get_object(parser);
	ucl_parser_free(parser);

	if (root == NULL) {
		ipc_send_error(fd, "", "empty message");
		return;
	}

	obj = ucl_object_lookup(root, "id");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING)
		id = ucl_object_tostring(obj);

	obj = ucl_object_lookup(root, "verb");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING)
		verb = ucl_object_tostring(obj);

	if (verb == NULL) {
		ipc_send_error(fd, id, "missing verb");
		ucl_object_unref(root);
		return;
	}

	if (strcmp(verb, "list") == 0)
		cmd_list(fd, id, root);
	else if (strcmp(verb, "status") == 0)
		cmd_status(fd, id, root);
	else if (strcmp(verb, "start") == 0)
		cmd_start(fd, id, root);
	else if (strcmp(verb, "stop") == 0)
		cmd_stop(fd, id, root);
	else if (strcmp(verb, "restart") == 0)
		cmd_restart(fd, id, root);
	else if (strcmp(verb, "reload") == 0)
		cmd_reload(fd, id, root);
	else if (strcmp(verb, "enable") == 0)
		cmd_enable(fd, id, root);
	else if (strcmp(verb, "disable") == 0)
		cmd_disable(fd, id, root);
	else if (strcmp(verb, "mask") == 0)
		cmd_mask(fd, id, root);
	else if (strcmp(verb, "unmask") == 0)
		cmd_unmask(fd, id, root);
	else if (strcmp(verb, "show") == 0)
		cmd_show(fd, id, root);
	else if (strcmp(verb, "reload-config") == 0)
		cmd_reload_config(fd, id, root);
	else if (strcmp(verb, "daemon-reexec") == 0)
		cmd_daemon_reexec(fd, id, root);
	else
		ipc_send_error(fd, id, "unknown verb");

	ucl_object_unref(root);
}

/* ------------------------------------------------------------------ */
/* Socket lifecycle                                                     */
/* ------------------------------------------------------------------ */

int
ipc_init(void)
{
	struct sockaddr_un sun;
	struct kevent kev;
	int fd;

	unlink(PROWLD_SOCK_PATH);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		prowl_log(LOG_ERR, "ipc socket: %m");
		return (-1);
	}

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, PROWLD_SOCK_PATH, sizeof(sun.sun_path));

	if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		prowl_log(LOG_ERR, "ipc bind %s: %m", PROWLD_SOCK_PATH);
		close(fd);
		return (-1);
	}

	chmod(PROWLD_SOCK_PATH, 0660);

	if (listen(fd, IPC_MAX_CLIENTS) == -1) {
		prowl_log(LOG_ERR, "ipc listen: %m");
		close(fd);
		return (-1);
	}

	EV_SET(&kev, (uintptr_t)fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
	if (kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1) {
		prowl_log(LOG_ERR, "kevent ipc listen: %m");
		close(fd);
		return (-1);
	}

	ipc_listen_fd = fd;
	memset(g_clients, 0, sizeof(g_clients));
	prowl_log(LOG_INFO, "IPC listening on %s", PROWLD_SOCK_PATH);
	return (0);
}

void
ipc_shutdown(void)
{
	int i;

	for (i = 0; i < IPC_MAX_CLIENTS; i++) {
		if (g_clients[i].active)
			ipc_close_client(g_clients[i].fd);
	}

	if (ipc_listen_fd >= 0) {
		close(ipc_listen_fd);
		ipc_listen_fd = -1;
	}

	unlink(PROWLD_SOCK_PATH);
}

void
ipc_accept(void)
{
	struct sockaddr_un sun;
	socklen_t sunlen = sizeof(sun);
	struct kevent kev;
	int cfd, i;

	cfd = accept(ipc_listen_fd, (struct sockaddr *)&sun, &sunlen);
	if (cfd == -1) {
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			prowl_log(LOG_WARNING, "ipc accept: %m");
		return;
	}

	for (i = 0; i < IPC_MAX_CLIENTS; i++) {
		if (!g_clients[i].active)
			break;
	}

	if (i == IPC_MAX_CLIENTS) {
		prowl_log(LOG_WARNING, "ipc: client limit reached");
		close(cfd);
		return;
	}

	g_clients[i].fd = cfd;
	g_clients[i].buf_used = 0;
	g_clients[i].active = true;

	EV_SET(&kev, (uintptr_t)cfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
	kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL);
}

void
ipc_close_client(int fd)
{
	struct kevent kev;
	int i;

	EV_SET(&kev, (uintptr_t)fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL);
	close(fd);

	for (i = 0; i < IPC_MAX_CLIENTS; i++) {
		if (g_clients[i].active && g_clients[i].fd == fd) {
			g_clients[i].active = false;
			g_clients[i].fd = -1;
			g_clients[i].buf_used = 0;
			break;
		}
	}
}

void
ipc_read_client(int fd)
{
	ipc_client_t *client = NULL;
	ssize_t n;
	int i;

	for (i = 0; i < IPC_MAX_CLIENTS; i++) {
		if (g_clients[i].active && g_clients[i].fd == fd) {
			client = &g_clients[i];
			break;
		}
	}

	if (client == NULL) {
		close(fd);
		return;
	}

	n = recv(fd, client->buf + client->buf_used,
	    sizeof(client->buf) - client->buf_used, 0);

	if (n <= 0) {
		ipc_close_client(fd);
		return;
	}

	client->buf_used += (size_t)n;

	for (;;) {
		uint32_t msglen;
		size_t total;

		if (client->buf_used < 4)
			break;

		memcpy(&msglen, client->buf, 4);
		msglen = ntohl(msglen);

		if (msglen == 0 || msglen > IPC_MSG_MAX - 4) {
			prowl_log(LOG_WARNING,
			    "ipc: invalid message length %u", msglen);
			ipc_close_client(fd);
			return;
		}

		total = 4 + (size_t)msglen;
		if (client->buf_used < total)
			break;

		ipc_dispatch_message(fd, client->buf + 4, (size_t)msglen);

		if (client->buf_used > total) {
			memmove(client->buf, client->buf + total,
			    client->buf_used - total);
		}
		client->buf_used -= total;
	}
}
