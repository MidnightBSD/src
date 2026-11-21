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
#include <grp.h>
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

/* Cached wheel group GID for IPC authorization */
static gid_t g_wheel_gid = 0;

/* Per-client state */
static ipc_client_t g_clients[IPC_MAX_CLIENTS];

/* Internal prototypes */
static ipc_client_t *ipc_get_client(int fd);
static void ipc_send_error(int fd, const char *id, const char *message);

/* ------------------------------------------------------------------ */
/* JSON helpers                                                          */
/* ------------------------------------------------------------------ */

/*
 * Write src as a JSON-escaped string (no surrounding quotes) into dst.
 * Handles \, ", \n, \r, \t, and other control chars as \uXXXX.
 * Returns the byte count written, or -1 on truncation.
 */
static int
json_escape_str(const char *src, char *dst, size_t dstsz)
{
	static const char hex[] = "0123456789abcdef";
	const unsigned char *p = (const unsigned char *)src;
	size_t i = 0;

	while (*p != '\0') {
		unsigned char c = *p++;

		if (c == '"' || c == '\\') {
			if (i + 2 >= dstsz)
				return (-1);
			dst[i++] = '\\';
			dst[i++] = (char)c;
		} else if (c == '\n') {
			if (i + 2 >= dstsz)
				return (-1);
			dst[i++] = '\\';
			dst[i++] = 'n';
		} else if (c == '\r') {
			if (i + 2 >= dstsz)
				return (-1);
			dst[i++] = '\\';
			dst[i++] = 'r';
		} else if (c == '\t') {
			if (i + 2 >= dstsz)
				return (-1);
			dst[i++] = '\\';
			dst[i++] = 't';
		} else if (c < 0x20) {
			if (i + 6 >= dstsz)
				return (-1);
			dst[i++] = '\\';
			dst[i++] = 'u';
			dst[i++] = '0';
			dst[i++] = '0';
			dst[i++] = hex[c >> 4];
			dst[i++] = hex[c & 0xf];
		} else {
			if (i + 1 >= dstsz)
				return (-1);
			dst[i++] = (char)c;
		}
	}
	dst[i] = '\0';
	return ((int)i);
}

/* Upper bound on queued output per client; slower readers lose messages. */
#define IPC_OUT_MAX	(IPC_REPLY_MAX * 2)

/*
 * Drop everything queued for a client and stop watching for writability.
 */
static void
ipc_out_reset(ipc_client_t *c)
{
	struct kevent kev;

	free(c->out);
	c->out = NULL;
	c->out_len = 0;
	c->out_off = 0;
	if (c->want_write) {
		EV_SET(&kev, (uintptr_t)c->fd, EVFILT_WRITE, EV_DELETE, 0, 0,
		    NULL);
		(void)kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL);
		c->want_write = false;
	}
}

/*
 * Write as much queued output as the socket accepts without blocking.  If
 * the socket is full, arm EVFILT_WRITE and resume from ipc_write_ready().
 * The fd is never closed here (callers may still be using the client);
 * on a fatal error the connection is shut down so the read side sees EOF
 * and closes the client through the normal path.
 */
static void
ipc_flush_client(ipc_client_t *c)
{
	struct kevent kev;
	ssize_t n;

	while (c->out_off < c->out_len) {
		n = send(c->fd, c->out + c->out_off, c->out_len - c->out_off,
		    MSG_NOSIGNAL | MSG_DONTWAIT);
		if (n == -1) {
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (c->want_write)
					return;
				EV_SET(&kev, (uintptr_t)c->fd, EVFILT_WRITE,
				    EV_ADD, 0, 0, NULL);
				if (kevent(g_kqueue_fd, &kev, 1, NULL, 0,
				    NULL) == 0) {
					c->want_write = true;
					return;
				}
				prowl_log(LOG_WARNING,
				    "ipc: kevent EVFILT_WRITE fd %d: %m",
				    c->fd);
			} else {
				prowl_log(LOG_DEBUG, "ipc send fd %d: %m",
				    c->fd);
			}
			/* A frame may be half sent: the stream is unusable. */
			(void)shutdown(c->fd, SHUT_RDWR);
			ipc_out_reset(c);
			return;
		}
		c->out_off += (size_t)n;
	}

	ipc_out_reset(c);
}

/*
 * Queue one length-prefixed frame for a client and try to send it.  Frames
 * are queued whole, so the stream framing stays intact even when a frame
 * has to be dropped for lack of room.
 */
static void
ipc_send(int fd, const char *json, size_t len)
{
	ipc_client_t *c;
	uint32_t nlen;
	size_t pending, total;
	char *nb;

	c = ipc_get_client(fd);
	if (c == NULL) {
		prowl_log(LOG_DEBUG, "ipc_send: fd %d is not a client", fd);
		return;
	}
	if (len > UINT32_MAX - 4)
		return;

	total = 4 + len;
	pending = c->out_len - c->out_off;
	if (pending > IPC_OUT_MAX || total > IPC_OUT_MAX - pending) {
		prowl_log(LOG_WARNING,
		    "ipc: output queue full for fd %d, dropping message", fd);
		return;
	}

	/* Reclaim the already-sent head before growing the buffer. */
	if (c->out_off > 0) {
		memmove(c->out, c->out + c->out_off, pending);
		c->out_len = pending;
		c->out_off = 0;
	}

	nb = realloc(c->out, c->out_len + total);
	if (nb == NULL) {
		prowl_log(LOG_ERR, "ipc_send: out of memory");
		return;
	}
	c->out = nb;

	nlen = htonl((uint32_t)len);
	memcpy(c->out + c->out_len, &nlen, 4);
	memcpy(c->out + c->out_len + 4, json, len);
	c->out_len += total;

	/* If EVFILT_WRITE is armed, it will drain the queue in order. */
	if (!c->want_write)
		ipc_flush_client(c);
}

/*
 * EVFILT_WRITE fired: the socket has room again.
 */
void
ipc_write_ready(int fd)
{
	ipc_client_t *c;

	c = ipc_get_client(fd);
	if (c == NULL)
		return;
	ipc_flush_client(c);
}

static void
ipc_send_ok(int fd, const char *id, const char *result_json)
{
	char eid[512];
	char *buf;
	size_t cap;
	int n;

	json_escape_str(id != NULL ? id : "", eid, sizeof(eid));

	/* Room for the envelope plus the result; results can be large. */
	cap = strlen(eid) + 64 +
	    (result_json != NULL ? strlen(result_json) : 0);
	buf = malloc(cap);
	if (buf == NULL) {
		ipc_send_error(fd, id, "out of memory");
		return;
	}

	if (result_json != NULL)
		n = snprintf(buf, cap,
		    "{\"id\":\"%s\",\"status\":\"ok\",\"result\":%s}",
		    eid, result_json);
	else
		n = snprintf(buf, cap,
		    "{\"id\":\"%s\",\"status\":\"ok\"}",
		    eid);

	if (n > 0 && (size_t)n < cap)
		ipc_send(fd, buf, (size_t)n);
	else	/* never leave the client waiting for a reply that was lost */
		ipc_send_error(fd, id, "response too large");
	free(buf);
}

static void
ipc_send_error(int fd, const char *id, const char *message)
{
	char buf[512];
	char eid[256];
	int n;

	json_escape_str(id != NULL ? id : "", eid, sizeof(eid));

	n = snprintf(buf, sizeof(buf),
	    "{\"id\":\"%s\",\"status\":\"error\",\"message\":\"%s\"}",
	    eid, message);
	if (n > 0 && (size_t)n < sizeof(buf))
		ipc_send(fd, buf, (size_t)n);
}

/* ------------------------------------------------------------------ */
/* Job serialisation                                                    */
/* ------------------------------------------------------------------ */

static int
job_to_json(const job_t *job, char *buf, size_t bufsz)
{
	char elabel[PROWL_LABEL_MAX * 2 + 1];
	char edesc[PROWL_DESC_MAX * 2 + 1];
	char ercname[PROWL_LABEL_MAX * 2 + 1];
	char estdout[PROWL_PATH_MAX * 2 + 1];
	char estderr[PROWL_PATH_MAX * 2 + 1];

	json_escape_str(job->label, elabel, sizeof(elabel));
	json_escape_str(job->description, edesc, sizeof(edesc));
	json_escape_str(job->rc_name, ercname, sizeof(ercname));
	json_escape_str(job->stdout_path, estdout, sizeof(estdout));
	json_escape_str(job->stderr_path, estderr, sizeof(estderr));

	return snprintf(buf, bufsz,
	    "{"
	    "\"label\":\"%s\","
	    "\"description\":\"%s\","
	    "\"type\":\"%s\","
	    "\"state\":\"%s\","
	    "\"pid\":%d,"
	    "\"rc_name\":\"%s\","
	    "\"enabled\":%s,"
	    "\"restart_count\":%d,"
	    "\"stdout_path\":\"%s\","
	    "\"stderr_path\":\"%s\""
	    "}",
	    elabel,
	    edesc,
	    job_type_str(job->type),
	    job_state_str(job->state),
	    (int)job->pid,
	    ercname,
	    job->enabled ? "true" : "false",
	    job->restart_count,
	    estdout,
	    estderr);
}

/*
 * Append a JSON array of dependency names to buf.
 * Returns the number of bytes written, or -1 on truncation.
 */
static int
append_dep_array(const job_t *job, char *buf, size_t bufsz)
{
	size_t used = 0;
	bool first = true;
	char ename[PROWL_LABEL_MAX * 2 + 1];
	int i, n;

	n = snprintf(buf + used, bufsz - used, "[");
	if (n < 0 || (size_t)n >= bufsz - used)
		return (-1);
	used += (size_t)n;

	for (i = 0; i < job->deps_count; i++) {
		if (!first) {
			if (used + 1 >= bufsz)
				return (-1);
			buf[used++] = ',';
		}
		first = false;

		json_escape_str(job->deps[i].name, ename, sizeof(ename));
		n = snprintf(buf + used, bufsz - used,
		    "{\"name\":\"%s\",\"hard\":%s}",
		    ename, job->deps[i].hard ? "true" : "false");
		if (n < 0 || (size_t)n >= bufsz - used)
			return (-1);
		used += (size_t)n;
	}

	n = snprintf(buf + used, bufsz - used, "]");
	if (n < 0 || (size_t)n >= bufsz - used)
		return (-1);
	used += (size_t)n;

	return ((int)used);
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
		prowl_reset_signals();
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
		job = job_find_instance(label);
	if (job == NULL)
		job = job_find_by_provides(label);
	if (job == NULL)
		job = job_find_by_rcname(label);

	/* "unit@instance" of a template that was not listed in rc.conf */
	if (job == NULL && strchr(label, '@') != NULL) {
		job = unit_instantiate(label);
		if (job != NULL)
			dag_build();
	}

	if (job == NULL)
		ipc_send_error(fd, id, "job not found");

	return (job);
}

/* ------------------------------------------------------------------ */
/* Command handlers                                                     */
/* ------------------------------------------------------------------ */

/*
 * Append n bytes of s to a growing NUL-terminated buffer.  Returns -1 if
 * the result would exceed IPC_REPLY_MAX or memory is exhausted.
 */
static int
rbuf_append(char **buf, size_t *cap, size_t *used, const char *s, size_t n)
{
	char *nb;
	size_t ncap;

	if (n > IPC_REPLY_MAX || *used > IPC_REPLY_MAX - n)
		return (-1);
	if (*used + n + 1 > *cap) {
		ncap = *cap == 0 ? 4096 : *cap;
		while (ncap < *used + n + 1)
			ncap *= 2;
		nb = realloc(*buf, ncap);
		if (nb == NULL)
			return (-1);
		*buf = nb;
		*cap = ncap;
	}
	memcpy(*buf + *used, s, n);
	*used += n;
	(*buf)[*used] = '\0';
	return (0);
}

static void
cmd_list(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;
	char *buf = NULL;
	size_t cap = 0, used = 0;
	char jbuf[4096];
	bool first = true;
	const ucl_object_t *filt, *sf;
	const char *state_filter = NULL;
	const char *type_filter = NULL;
	bool enabled_only = false;

	filt = ucl_object_lookup(req, "filter");
	if (filt != NULL) {
		sf = ucl_object_lookup(filt, "state");
		if (sf != NULL)
			state_filter = ucl_object_tostring(sf);
		sf = ucl_object_lookup(filt, "type");
		if (sf != NULL)
			type_filter = ucl_object_tostring(sf);
		sf = ucl_object_lookup(filt, "enabled");
		if (sf != NULL)
			enabled_only = ucl_object_toboolean(sf);
	}

	/* An unknown type is an error, not an empty list. */
	if (type_filter != NULL && strcmp(type_filter, "daemon") != 0 &&
	    strcmp(type_filter, "oneshot") != 0 &&
	    strcmp(type_filter, "timer") != 0 &&
	    strcmp(type_filter, "socket") != 0 &&
	    strcmp(type_filter, "rcshim") != 0) {
		ipc_send_error(fd, id, "unknown job type");
		return;
	}

	if (rbuf_append(&buf, &cap, &used, "[", 1) == -1)
		goto toolarge;

	TAILQ_FOREACH(job, &g_jobs, entries) {
		int n;

		if (state_filter != NULL &&
		    strcmp(job_state_str(job->state), state_filter) != 0)
			continue;
		if (type_filter != NULL &&
		    strcmp(job_type_str(job->type), type_filter) != 0)
			continue;
		if (enabled_only && (!job->enabled ||
		    job->state == JOB_STATE_DISABLED ||
		    job->state == JOB_STATE_MASKED))
			continue;

		n = job_to_json(job, jbuf, sizeof(jbuf));
		if (n <= 0 || (size_t)n >= sizeof(jbuf)) {
			prowl_log(LOG_WARNING,
			    "cmd_list: job %s does not fit its buffer, "
			    "skipped", job->label);
			continue;
		}
		if (!first && rbuf_append(&buf, &cap, &used, ",", 1) == -1)
			goto toolarge;
		first = false;
		if (rbuf_append(&buf, &cap, &used, jbuf, (size_t)n) == -1)
			goto toolarge;
	}

	if (rbuf_append(&buf, &cap, &used, "]", 1) == -1)
		goto toolarge;

	ipc_send_ok(fd, id, buf);
	free(buf);
	return;

toolarge:
	free(buf);
	ipc_send_error(fd, id, "job list too large");
}

static void
cmd_status(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;
	char jbuf[4096];

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
		else {
			ipc_send_error(fd, id, "job failed to start "
			    "(see the system log)");
			return;
		}
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
	if (job->state == JOB_STATE_STOPPING) {
		/* started again by supervisor_reap() once it has stopped */
		job->restart_after_stop = true;
	} else if (job->state != JOB_STATE_RUNNING &&
	    job->state != JOB_STATE_STARTING &&
	    job->state != JOB_STATE_MASKED &&
	    job->state != JOB_STATE_DISABLED) {
		job_set_state(job, JOB_STATE_LOADED);
		dag_schedule_ready();
	}
	ipc_send_ok(fd, id, NULL);
}

/*
 * Run a unit's reload, check or resync command and send back its exit
 * status and output: {"exit":N,"output":"..."}.  Reload and resync only
 * make sense for a job that has been started; check can run at any time.
 */
static void
cmd_action(int fd, const char *id, job_t *job, int action, const char *name)
{
	char out[PROWL_ACTION_OUT_MAX];
	char msg[96];
	char *esc, *result;
	size_t cap;
	int code;

	if (job->action_argc[action] == 0) {
		snprintf(msg, sizeof(msg), "job has no %s command", name);
		ipc_send_error(fd, id, msg);
		return;
	}
	if (job->state == JOB_STATE_MASKED) {
		ipc_send_error(fd, id, "job is masked");
		return;
	}
	if (action != PROWL_ACTION_CHECK &&
	    job->state != JOB_STATE_RUNNING &&
	    job->state != JOB_STATE_COMPLETED) {
		ipc_send_error(fd, id, "job not running");
		return;
	}

	if (supervisor_run_action(job, action, out, sizeof(out), &code) == -1) {
		ipc_send_error(fd, id, "could not run the command");
		return;
	}

	/* Worst case every byte becomes a six byte \u00XX escape. */
	cap = strlen(out) * 6 + 1;
	esc = malloc(cap);
	if (esc == NULL) {
		ipc_send_error(fd, id, "out of memory");
		return;
	}
	if (json_escape_str(out, esc, cap) == -1) {
		free(esc);
		ipc_send_error(fd, id, "could not encode the output");
		return;
	}
	cap = strlen(esc) + 64;
	result = malloc(cap);
	if (result == NULL) {
		free(esc);
		ipc_send_error(fd, id, "out of memory");
		return;
	}
	snprintf(result, cap, "{\"exit\":%d,\"output\":\"%s\"}", code, esc);
	ipc_send_ok(fd, id, result);
	free(result);
	free(esc);
}

static void
cmd_check(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;

	job = resolve_target(req, fd, id);
	if (job != NULL)
		cmd_action(fd, id, job, PROWL_ACTION_CHECK, "check");
}

static void
cmd_resync(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;

	job = resolve_target(req, fd, id);
	if (job != NULL)
		cmd_action(fd, id, job, PROWL_ACTION_RESYNC, "resync");
}

static void
cmd_reload(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;

	job = resolve_target(req, fd, id);
	if (job == NULL)
		return;

	/* A unit with a reload command has it run instead of a SIGHUP */
	if (job->action_argc[PROWL_ACTION_RELOAD] > 0) {
		cmd_action(fd, id, job, PROWL_ACTION_RELOAD, "reload");
		return;
	}

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

	if (job->rc_name[0] == '\0') {
		ipc_send_error(fd, id, "job has no rc_name");
		return;
	}

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

	if (job->rc_name[0] == '\0') {
		ipc_send_error(fd, id, "job has no rc_name");
		return;
	}

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

	snprintf(path, sizeof(path), "%s/%s", g_mask_dir, job->label);
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

	snprintf(path, sizeof(path), "%s/%s", g_mask_dir, job->label);
	unlink(path);

	if (job->state == JOB_STATE_MASKED)
		job_set_state(job, JOB_STATE_LOADED);

	ipc_send_ok(fd, id, NULL);
}

static void
cmd_show(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;
	char jbuf[4096];

	job = resolve_target(req, fd, id);
	if (job == NULL)
		return;

	job_to_json(job, jbuf, sizeof(jbuf));
	ipc_send_ok(fd, id, jbuf);
}

static void
cmd_dependencies(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;
	char depbuf[4096];

	job = resolve_target(req, fd, id);
	if (job == NULL)
		return;

	if (append_dep_array(job, depbuf, sizeof(depbuf)) < 0) {
		ipc_send_error(fd, id, "dependency list too large");
		return;
	}
	ipc_send_ok(fd, id, depbuf);
}

/*
 * cmd_graph: emit the full dependency graph as a JSON array of edges.
 * Each edge: {"from":"<label>","to":"<dep_name>","hard":<bool>}
 */
static void
cmd_graph(int fd, const char *id, const ucl_object_t *req)
{
	job_t *job;
	char buf[IPC_MSG_MAX];
	char efrom[PROWL_LABEL_MAX * 2 + 1];
	char eto[PROWL_LABEL_MAX * 2 + 1];
	size_t used = 0;
	bool first = true;
	int i, n;

	(void)req;

	n = snprintf(buf + used, sizeof(buf) - used, "[");
	if (n < 0 || (size_t)n >= sizeof(buf) - used)
		goto trunc;
	used += (size_t)n;

	TAILQ_FOREACH(job, &g_jobs, entries) {
		json_escape_str(job->label, efrom, sizeof(efrom));
		for (i = 0; i < job->deps_count; i++) {
			json_escape_str(job->deps[i].name, eto, sizeof(eto));
			if (!first) {
				if (used + 1 >= sizeof(buf))
					goto trunc;
				buf[used++] = ',';
			}
			first = false;
			n = snprintf(buf + used, sizeof(buf) - used,
			    "{\"from\":\"%s\",\"to\":\"%s\",\"hard\":%s}",
			    efrom, eto,
			    job->deps[i].hard ? "true" : "false");
			if (n < 0 || (size_t)n >= sizeof(buf) - used)
				goto trunc;
			used += (size_t)n;
		}
	}

	n = snprintf(buf + used, sizeof(buf) - used, "]");
	if (n < 0 || (size_t)n >= sizeof(buf) - used)
		goto trunc;
	used += (size_t)n;

	ipc_send_ok(fd, id, buf);
	return;

trunc:
	ipc_send_error(fd, id, "graph too large");
}

static void
cmd_subscribe(int fd, const char *id, const ucl_object_t *req)
{
	ipc_client_t *client = ipc_get_client(fd);
	const ucl_object_t *filter;

	if (client == NULL)
		return;

	client->is_subscriber = true;
	filter = ucl_object_lookup(req, "filter");
	if (filter != NULL && ucl_object_type(filter) == UCL_OBJECT) {
		const ucl_object_t *label = ucl_object_lookup(filter, "label");
		if (label != NULL && ucl_object_type(label) == UCL_STRING)
			strlcpy(client->event_filter, ucl_object_tostring(label),
			    sizeof(client->event_filter));
	}

	ipc_send_ok(fd, id, NULL);
}

void
ipc_broadcast_event(const char *label, const char *old_state, const char *new_state)
{
	char msg[1024];
	char elabel[PROWL_LABEL_MAX * 2 + 1];
	char eold[128];
	char enew[128];
	int i;

	json_escape_str(label != NULL ? label : "", elabel, sizeof(elabel));
	json_escape_str(old_state != NULL ? old_state : "", eold, sizeof(eold));
	json_escape_str(new_state != NULL ? new_state : "", enew, sizeof(enew));

	snprintf(msg, sizeof(msg),
	    "{\"verb\":\"event\",\"label\":\"%s\",\"old_state\":\"%s\",\"new_state\":\"%s\"}",
	    elabel, eold, enew);

	for (i = 0; i < IPC_MAX_CLIENTS; i++) {
		if (g_clients[i].active && g_clients[i].is_subscriber) {
			/* Check filter if set (very basic prefix match for now) */
			if (g_clients[i].event_filter[0] != '\0') {
				if (strncmp(label, g_clients[i].event_filter,
				    strlen(g_clients[i].event_filter)) != 0)
					continue;
			}
			
			/* Non-blocking send: if it fails, client is too slow */
			ipc_send(g_clients[i].fd, msg, strlen(msg));
		}
	}
}

static void
cmd_reload_config(int fd, const char *id, const ucl_object_t *req)
{
	(void)req;
	g_reload = true;
	ipc_send_ok(fd, id, NULL);
}

static void
cmd_shutdown(int fd, const char *id, const ucl_object_t *req)
{
	(void)req;
	if (g_is_init) {
		ipc_send_error(fd, id,
		    "prowld is init: use shutdown(8) or reboot(8)");
		return;
	}
	prowl_log(LOG_NOTICE, "shutdown requested over IPC");
	g_shutdown = true;
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
/* IPC authorization helpers                                            */
/* ------------------------------------------------------------------ */

static ipc_client_t *
ipc_get_client(int fd)
{
	int i;

	for (i = 0; i < IPC_MAX_CLIENTS; i++) {
		if (g_clients[i].active && g_clients[i].fd == fd)
			return (&g_clients[i]);
	}
	return (NULL);
}

static bool
ipc_peer_is_root(const ipc_client_t *client)
{
	return (client->peer_uid == 0);
}

/* Read-only verbs are permitted to root or any wheel-group member. */
static bool
ipc_peer_can_query(const ipc_client_t *client)
{
	return (client->peer_uid == 0 || client->peer_gid == g_wheel_gid);
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
		/*
		 * Log at DEBUG only: a misbehaving client can trigger this
		 * repeatedly, filling syslog.  Cap the error string to avoid
		 * leaking IPC payload content into the system log.
		 */
		prowl_log(LOG_DEBUG, "ipc: parse error from fd %d: %.128s",
		    fd, ucl_parser_get_error(parser));
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

	{
		ipc_client_t *client = ipc_get_client(fd);

		if (client == NULL) {
			ipc_send_error(fd, id, "internal error");
			ucl_object_unref(root);
			return;
		}

		/*
		 * Read-only verbs: wheel group or root.
		 * All state-changing verbs: root only.
		 */
		if (strcmp(verb, "list") == 0 ||
		    strcmp(verb, "status") == 0 ||
		    strcmp(verb, "show") == 0 ||
		    strcmp(verb, "dependencies") == 0 ||
		    strcmp(verb, "graph") == 0 ||
		    strcmp(verb, "subscribe") == 0) {

			if (!ipc_peer_can_query(client)) {
				ipc_send_error(fd, id,
				    "operation not permitted");
				ucl_object_unref(root);
				return;
			}
		} else {
			if (!ipc_peer_is_root(client)) {
				ipc_send_error(fd, id,
				    "operation not permitted");
				ucl_object_unref(root);
				return;
			}
		}
	}

	if (strcmp(verb, "list") == 0)
		cmd_list(fd, id, root);
	else if (strcmp(verb, "status") == 0)
		cmd_status(fd, id, root);
	else if (strcmp(verb, "subscribe") == 0)
		cmd_subscribe(fd, id, root);
	else if (strcmp(verb, "start") == 0)
		cmd_start(fd, id, root);
	else if (strcmp(verb, "stop") == 0)
		cmd_stop(fd, id, root);
	else if (strcmp(verb, "restart") == 0)
		cmd_restart(fd, id, root);
	else if (strcmp(verb, "reload") == 0)
		cmd_reload(fd, id, root);
	else if (strcmp(verb, "check") == 0)
		cmd_check(fd, id, root);
	else if (strcmp(verb, "resync") == 0)
		cmd_resync(fd, id, root);
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
	else if (strcmp(verb, "dependencies") == 0)
		cmd_dependencies(fd, id, root);
	else if (strcmp(verb, "graph") == 0)
		cmd_graph(fd, id, root);
	else if (strcmp(verb, "reload-config") == 0)
		cmd_reload_config(fd, id, root);
	else if (strcmp(verb, "daemon-reexec") == 0)
		cmd_daemon_reexec(fd, id, root);
	else if (strcmp(verb, "shutdown") == 0)
		cmd_shutdown(fd, id, root);
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

	/* A longer path would be silently truncated: bind the wrong socket. */
	if (strlen(g_sock_path) >= sizeof(sun.sun_path)) {
		prowl_log(LOG_ERR, "ipc socket path %s is too long",
		    g_sock_path);
		return (-1);
	}

	unlink(g_sock_path);

	fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (fd == -1) {
		prowl_log(LOG_ERR, "ipc socket: %m");
		return (-1);
	}

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, g_sock_path, sizeof(sun.sun_path));

	{
		struct group *gr = getgrnam("wheel");
		mode_t oumask;
		int bind_rc, bind_errno;

		if (gr != NULL)
			g_wheel_gid = gr->gr_gid;

		/*
		 * fchmod(2)/fchown(2) do not work on a socket descriptor
		 * (EBADF), so the mode is set through the umask: the node is
		 * created 0660 and is never more permissive than that.  The
		 * group is then set by path; the run directory is root-owned
		 * and not writable by others (secure_mkdir), so nobody else
		 * can replace the node, and lchown does not follow symlinks.
		 */
		oumask = umask(0117);
		bind_rc = bind(fd, (struct sockaddr *)&sun, sizeof(sun));
		bind_errno = errno;
		(void)umask(oumask);
		if (bind_rc == -1) {
			errno = bind_errno;
			prowl_log(LOG_ERR, "ipc bind %s: %m", g_sock_path);
			close(fd);
			return (-1);
		}
		if (lchown(g_sock_path, 0, g_wheel_gid) == -1)
			prowl_log(LOG_WARNING, "ipc_init: lchown %s: %m",
			    g_sock_path);
	}

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
	prowl_log(LOG_INFO, "IPC listening on %s", g_sock_path);
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

	unlink(g_sock_path);
}

void
ipc_accept(void)
{
	struct sockaddr_un sun;
	socklen_t sunlen = sizeof(sun);
	struct kevent kev;
	int cfd, i;

	cfd = accept4(ipc_listen_fd, (struct sockaddr *)&sun, &sunlen,
	    SOCK_NONBLOCK | SOCK_CLOEXEC);
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

	{
		uid_t puid;
		gid_t pgid;
		if (getpeereid(cfd, &puid, &pgid) == -1) {
			prowl_log(LOG_WARNING, "ipc_accept getpeereid: %m");
			close(cfd);
			return;
		}
		g_clients[i].peer_uid = puid;
		g_clients[i].peer_gid = pgid;
	}

	g_clients[i].fd = cfd;
	g_clients[i].buf_used = 0;
	g_clients[i].is_subscriber = false;
	g_clients[i].event_filter[0] = '\0';
	g_clients[i].out = NULL;
	g_clients[i].out_len = 0;
	g_clients[i].out_off = 0;
	g_clients[i].want_write = false;
	g_clients[i].active = true;

	EV_SET(&kev, (uintptr_t)cfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
	if (kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1) {
		prowl_log(LOG_WARNING, "ipc_accept kevent: %m");
		close(cfd);
		return;
	}
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
			free(g_clients[i].out);
			g_clients[i].out = NULL;
			g_clients[i].out_len = 0;
			g_clients[i].out_off = 0;
			g_clients[i].want_write = false;
			g_clients[i].is_subscriber = false;
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
	    sizeof(client->buf) - client->buf_used, MSG_DONTWAIT);

	if (n == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return;
		ipc_close_client(fd);
		return;
	}
	if (n == 0) {
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
