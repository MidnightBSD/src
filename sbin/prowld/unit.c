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
 * Unit file loading and parsing (UCL/JSON via libucl).
 */

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <netinet/in.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include <ucl.h>

#include "prowld.h"

/*
 * Parse a string array from a UCL node and fill a fixed-size char[][] array.
 * Returns the number of entries filled.
 */
static int
parse_str_array(const ucl_object_t *arr, char dest[][PROWL_LABEL_MAX],
    int maxn)
{
	const ucl_object_t *item;
	ucl_object_iter_t it = NULL;
	int n = 0;

	if (arr == NULL || ucl_object_type(arr) != UCL_ARRAY)
		return (0);

	while ((item = ucl_object_iterate(arr, &it, true)) != NULL) {
		if (n >= maxn)
			break;
		if (ucl_object_type(item) != UCL_STRING)
			continue;
		strlcpy(dest[n], ucl_object_tostring(item),
		    PROWL_LABEL_MAX);
		n++;
	}
	return (n);
}

/*
 * Derive rc_name from label: take the last dot-separated component.
 * e.g. "org.midnightbsd.sshd" -> "sshd"
 */
static void
derive_rc_name(const char *label, char *rc_name, size_t sz)
{
	const char *last;

	last = strrchr(label, '.');
	if (last != NULL)
		strlcpy(rc_name, last + 1, sz);
	else
		strlcpy(rc_name, label, sz);
}

/*
 * Validate an environment variable key: must be non-empty, and contain
 * only alphanumerics and underscores (POSIX convention).
 */
static bool
env_key_valid(const char *key)
{
	const char *p;

	if (key == NULL || *key == '\0')
		return (false);
	for (p = key; *p != '\0'; p++) {
		unsigned char c = (unsigned char)*p;
		if (!isalnum(c) && c != '_')
			return (false);
	}
	return (true);
}

/*
 * Parse job type string into job_type_t.
 */
static job_type_t
parse_job_type(const char *s)
{
	if (s == NULL || strcmp(s, "daemon") == 0)
		return (JOB_TYPE_DAEMON);
	if (strcmp(s, "oneshot") == 0)
		return (JOB_TYPE_ONESHOT);
	if (strcmp(s, "timer") == 0)
		return (JOB_TYPE_TIMER);
	if (strcmp(s, "socket") == 0)
		return (JOB_TYPE_SOCKET);
	return (JOB_TYPE_DAEMON);
}

/*
 * Parse keep_alive: can be a boolean or an object with sub-fields.
 */
static void
parse_keep_alive(const ucl_object_t *obj, keep_alive_t *ka)
{
	const ucl_object_t *field;

	memset(ka, 0, sizeof(*ka));

	if (obj == NULL)
		return;

	if (ucl_object_type(obj) == UCL_BOOLEAN) {
		ka->always = ucl_object_toboolean(obj);
		return;
	}

	if (ucl_object_type(obj) != UCL_OBJECT)
		return;

	field = ucl_object_lookup(obj, "successful_exit");
	if (field != NULL)
		ka->successful_exit = ucl_object_toboolean(field);

	field = ucl_object_lookup(obj, "crashed");
	if (field != NULL)
		ka->crashed = ucl_object_toboolean(field);

	field = ucl_object_lookup(obj, "always");
	if (field != NULL)
		ka->always = ucl_object_toboolean(field);
}

/*
 * Parse dependency list (requires or wants) into job->deps[].
 */
static void
parse_deps(const ucl_object_t *arr, job_t *job, bool hard)
{
	const ucl_object_t *item;
	ucl_object_iter_t it = NULL;

	if (arr == NULL || ucl_object_type(arr) != UCL_ARRAY)
		return;

	while ((item = ucl_object_iterate(arr, &it, true)) != NULL) {
		if (job->deps_count >= PROWL_DEPS_MAX)
			break;
		if (ucl_object_type(item) != UCL_STRING)
			continue;
		strlcpy(job->deps[job->deps_count].name,
		    ucl_object_tostring(item), PROWL_LABEL_MAX);
		job->deps[job->deps_count].hard = hard;
		job->deps_count++;
	}
}

/*
 * Parse a calendar field: integer or "*" (mapped to -1).
 */
static int
parse_calendar_field(const ucl_object_t *obj)
{
	if (obj == NULL)
		return (-1);

	if (ucl_object_type(obj) == UCL_STRING) {
		const char *s = ucl_object_tostring(obj);
		if (strcmp(s, "*") == 0)
			return (-1);
		return ((int)strtol(s, NULL, 10));
	}

	return ((int)ucl_object_toint(obj));
}

/*
 * Parse schedule block.
 */
static void
parse_schedule(const ucl_object_t *obj, schedule_t *sched)
{
	const ucl_object_t *f, *cal;

	memset(sched, 0, sizeof(*sched));
	sched->calendar.minute = -1;
	sched->calendar.hour = -1;
	sched->calendar.day = -1;
	sched->calendar.month = -1;
	sched->calendar.weekday = -1;

	if (obj == NULL || ucl_object_type(obj) != UCL_OBJECT)
		return;

	f = ucl_object_lookup(obj, "interval");
	if (f != NULL)
		sched->interval = (int)ucl_object_toint(f);

	f = ucl_object_lookup(obj, "on_boot_delay");
	if (f != NULL)
		sched->on_boot_delay = (int)ucl_object_toint(f);

	f = ucl_object_lookup(obj, "persistent");
	if (f != NULL)
		sched->persistent = ucl_object_toboolean(f);

	cal = ucl_object_lookup(obj, "calendar");
	if (cal != NULL && ucl_object_type(cal) == UCL_OBJECT) {
		sched->has_calendar = true;
		sched->calendar.minute = parse_calendar_field(
		    ucl_object_lookup(cal, "minute"));
		sched->calendar.hour = parse_calendar_field(
		    ucl_object_lookup(cal, "hour"));
		sched->calendar.day = parse_calendar_field(
		    ucl_object_lookup(cal, "day"));
		sched->calendar.month = parse_calendar_field(
		    ucl_object_lookup(cal, "month"));
		sched->calendar.weekday = parse_calendar_field(
		    ucl_object_lookup(cal, "weekday"));
	}
}

/*
 * Parse a single unit file into a new job_t.
 * Returns NULL on error.
 */
static job_t *
unit_parse_obj(const ucl_object_t *root, const char *path)
{
	job_t *job;
	const ucl_object_t *obj, *arr;
	const ucl_object_t *item;
	ucl_object_iter_t it = NULL;
	const char *s;
	int n;

	obj = ucl_object_lookup(root, "label");
	if (obj == NULL || ucl_object_type(obj) != UCL_STRING) {
		prowl_log(LOG_ERR, "unit %s: missing required field 'label'",
		    path);
		return (NULL);
	}

	job = job_alloc();
	if (job == NULL)
		return (NULL);

	/* label */
	strlcpy(job->label, ucl_object_tostring(obj), sizeof(job->label));

	/* Reject labels that could escape the mask directory path */
	if (!label_valid(job->label)) {
		prowl_log(LOG_ERR, "unit %s: invalid label '%s', skipping",
		    path, job->label);
		job_free(job);
		return (NULL);
	}

	/* Check for duplicate */
	if (job_find_by_label(job->label) != NULL) {
		prowl_log(LOG_DEBUG, "unit %s: label %s already loaded, "
		    "skipping (override applied earlier)", path, job->label);
		job_free(job);
		return (NULL);
	}

	/* description */
	obj = ucl_object_lookup(root, "description");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING)
		strlcpy(job->description, ucl_object_tostring(obj),
		    sizeof(job->description));

	/* type */
	obj = ucl_object_lookup(root, "type");
	s = (obj != NULL && ucl_object_type(obj) == UCL_STRING) ?
	    ucl_object_tostring(obj) : NULL;
	job->type = parse_job_type(s);

	/* rc_name */
	obj = ucl_object_lookup(root, "rc_name");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING)
		strlcpy(job->rc_name, ucl_object_tostring(obj),
		    sizeof(job->rc_name));
	else
		derive_rc_name(job->label, job->rc_name, sizeof(job->rc_name));

	/* program */
	obj = ucl_object_lookup(root, "program");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING)
		strlcpy(job->program, ucl_object_tostring(obj),
		    sizeof(job->program));

	/* arguments */
	arr = ucl_object_lookup(root, "arguments");
	if (arr != NULL && ucl_object_type(arr) == UCL_ARRAY) {
		it = NULL;
		n = 0;
		while ((item = ucl_object_iterate(arr, &it, true)) != NULL) {
			if (n >= PROWL_ARGS_MAX - 1)
				break;
			if (ucl_object_type(item) != UCL_STRING)
				continue;
			job->arguments[n] = strdup(ucl_object_tostring(item));
			if (job->arguments[n] == NULL) {
				prowl_log(LOG_ERR, "strdup: %m");
				job_free(job);
				return (NULL);
			}
			n++;
		}
		job->argc = n;
	}

	/* environment */
	obj = ucl_object_lookup(root, "environment");
	if (obj != NULL && ucl_object_type(obj) == UCL_OBJECT) {
		const ucl_object_t *val;
		it = NULL;
		n = 0;
		while ((val = ucl_object_iterate(obj, &it, true)) != NULL) {
			char envbuf[PROWL_PATH_MAX];
			const char *key, *value;
			int nb;

			if (n >= PROWL_ENV_MAX - 1)
				break;

			key = ucl_object_key(val);
			if (!env_key_valid(key)) {
				prowl_log(LOG_WARNING,
				    "unit %s: invalid env key, skipping", path);
				continue;
			}

			value = ucl_object_tostring_forced(val);
			if (value == NULL)
				value = "";

			/*
			 * NEVER log env values: they may contain secrets
			 * (API keys, passwords, tokens).  Log only the key.
			 */
			nb = snprintf(envbuf, sizeof(envbuf), "%s=%s",
			    key, value);
			if (nb < 0 || (size_t)nb >= sizeof(envbuf)) {
				prowl_log(LOG_WARNING,
				    "unit %s: env var '%s' too long, skipping",
				    path, key);
				continue;
			}

			job->envp[n] = strdup(envbuf);
			if (job->envp[n] == NULL) {
				prowl_log(LOG_ERR, "strdup: %m");
				job_free(job);
				return (NULL);
			}
			n++;
		}
		job->envc = n;
	}

	/* working_directory */
	obj = ucl_object_lookup(root, "working_directory");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING)
		strlcpy(job->working_directory, ucl_object_tostring(obj),
		    sizeof(job->working_directory));

	/* root_directory */
	obj = ucl_object_lookup(root, "root_directory");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING)
		strlcpy(job->root_directory, ucl_object_tostring(obj),
		    sizeof(job->root_directory));

	/* user */
	obj = ucl_object_lookup(root, "user");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING)
		strlcpy(job->user, ucl_object_tostring(obj),
		    sizeof(job->user));

	/* group */
	obj = ucl_object_lookup(root, "group");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING)
		strlcpy(job->group, ucl_object_tostring(obj),
		    sizeof(job->group));

	/* umask */
	obj = ucl_object_lookup(root, "umask");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING) {
		job->umask_val = (mode_t)strtol(ucl_object_tostring(obj),
		    NULL, 8);
		job->umask_set = true;
	}

	/* run_at_load */
	obj = ucl_object_lookup(root, "run_at_load");
	if (obj != NULL)
		job->run_at_load = ucl_object_toboolean(obj);

	/* keep_alive */
	obj = ucl_object_lookup(root, "keep_alive");
	parse_keep_alive(obj, &job->keep_alive);

	/* throttle_interval */
	obj = ucl_object_lookup(root, "throttle_interval");
	if (obj != NULL)
		job->throttle_interval = (int)ucl_object_toint(obj);

	/* exit_timeout */
	obj = ucl_object_lookup(root, "exit_timeout");
	if (obj != NULL)
		job->exit_timeout = (int)ucl_object_toint(obj);

	/* requires (hard deps) */
	parse_deps(ucl_object_lookup(root, "requires"), job, true);

	/* wants (soft deps) */
	parse_deps(ucl_object_lookup(root, "wants"), job, false);

	/* before */
	arr = ucl_object_lookup(root, "before");
	job->before_count = parse_str_array(arr, job->before, PROWL_DEPS_MAX);

	/* after */
	arr = ucl_object_lookup(root, "after");
	job->after_count = parse_str_array(arr, job->after, PROWL_DEPS_MAX);

	/* conflicts */
	arr = ucl_object_lookup(root, "conflicts");
	job->conflicts_count = parse_str_array(arr, job->conflicts,
	    PROWL_DEPS_MAX);

	/* provides */
	arr = ucl_object_lookup(root, "provides");
	job->provides_count = parse_str_array(arr, job->provides,
	    PROWL_PROVIDES_MAX);

	/* I/O paths */
	obj = ucl_object_lookup(root, "standard_in_path");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING)
		strlcpy(job->stdin_path, ucl_object_tostring(obj),
		    sizeof(job->stdin_path));

	obj = ucl_object_lookup(root, "standard_out_path");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING)
		strlcpy(job->stdout_path, ucl_object_tostring(obj),
		    sizeof(job->stdout_path));

	obj = ucl_object_lookup(root, "standard_error_path");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING)
		strlcpy(job->stderr_path, ucl_object_tostring(obj),
		    sizeof(job->stderr_path));

	/* nice */
	obj = ucl_object_lookup(root, "nice");
	if (obj != NULL)
		job->nice_val = (int)ucl_object_toint(obj);

	/* notify_type */
	obj = ucl_object_lookup(root, "notify_type");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING) {
		s = ucl_object_tostring(obj);
		if (strcmp(s, "notify") == 0)
			job->notify_type = NOTIFY_NOTIFY;
	}

	/* watchdog_sec */
	obj = ucl_object_lookup(root, "watchdog_sec");
	if (obj != NULL)
		job->watchdog_sec = (int)ucl_object_toint(obj);

	/* shutdown_wait */
	obj = ucl_object_lookup(root, "shutdown_wait");
	if (obj != NULL)
		job->shutdown_wait = ucl_object_toboolean(obj);

	/* resource_limits */
	obj = ucl_object_lookup(root, "resource_limits");
	if (obj != NULL && ucl_object_type(obj) == UCL_OBJECT) {
...
			job->rlimits.set_nproc = true;
		}
	}

	/* schedule */
	parse_schedule(ucl_object_lookup(root, "schedule"), &job->schedule);

	/*
	 * sockets: object keyed by name, each value describes one socket.
	 * Example:
	 *   "sockets": {
	 *     "http": { "path": "/var/run/svc.sock", "type": "stream" },
	 *     "rpc":  { "port": 9000, "host": "127.0.0.1" }
	 *   }
	 */
	obj = ucl_object_lookup(root, "sockets");
	if (obj != NULL && ucl_object_type(obj) == UCL_OBJECT) {
		const ucl_object_t *sobj;
		ucl_object_iter_t sit = NULL;

		while ((sobj = ucl_object_iterate(obj, &sit, true)) != NULL) {
			const char *sname;
			const ucl_object_t *f;
			prowl_socket_t *sock;

			if (job->sockets_count >= PROWL_SOCKETS_MAX) {
				prowl_log(LOG_WARNING,
				    "unit %s: too many sockets (max %d), "
				    "ignoring remaining", path, PROWL_SOCKETS_MAX);
				break;
			}
			if (ucl_object_type(sobj) != UCL_OBJECT)
				continue;

			sname = ucl_object_key(sobj);
			sock = &job->sockets[job->sockets_count];

			strlcpy(sock->name, sname, sizeof(sock->name));

			/* Defaults */
			sock->socktype = SOCK_STREAM;
			sock->family   = AF_UNSPEC;
			sock->backlog  = 0;	/* SOMAXCONN */

			/* type: "stream" (default), "dgram", "seqpacket" */
			f = ucl_object_lookup(sobj, "type");
			if (f != NULL && ucl_object_type(f) == UCL_STRING) {
				const char *t = ucl_object_tostring(f);
				if (strcmp(t, "dgram") == 0)
					sock->socktype = SOCK_DGRAM;
				else if (strcmp(t, "seqpacket") == 0)
					sock->socktype = SOCK_SEQPACKET;
				/* else: stream (default) */
			}

			/* path → AF_UNIX */
			f = ucl_object_lookup(sobj, "path");
			if (f != NULL && ucl_object_type(f) == UCL_STRING) {
				strlcpy(sock->path, ucl_object_tostring(f),
				    sizeof(sock->path));
				sock->family = AF_UNIX;
			}

			/* port → AF_INET or AF_INET6 */
			f = ucl_object_lookup(sobj, "port");
			if (f != NULL) {
				sock->port = (int)ucl_object_toint(f);
				if (sock->family == AF_UNSPEC)
					sock->family = AF_INET;
			}

			/* host → refines family */
			f = ucl_object_lookup(sobj, "host");
			if (f != NULL && ucl_object_type(f) == UCL_STRING) {
				strlcpy(sock->host, ucl_object_tostring(f),
				    sizeof(sock->host));
				/* Auto-detect IPv6 if host contains ':' */
				if (strchr(sock->host, ':') != NULL)
					sock->family = AF_INET6;
				else if (sock->family == AF_UNSPEC)
					sock->family = AF_INET;
			}

			/* backlog */
			f = ucl_object_lookup(sobj, "backlog");
			if (f != NULL)
				sock->backlog = (int)ucl_object_toint(f);

			/* Require a usable address */
			if (sock->family == AF_UNSPEC) {
				prowl_log(LOG_WARNING,
				    "unit %s: socket '%s' has no path or port, "
				    "skipping", path, sname);
				continue;
			}

			job->sockets_count++;
		}
	}

	/* Determine enabled state from rc.conf */
	job->enabled = rcconf_is_enabled(job->rc_name, job->run_at_load);

	return (job);
}

int
unit_load_file(const char *path)
{
	struct ucl_parser *parser;
	ucl_object_t *root;
	struct stat sb;
	job_t *job;
	int ret = 0;

	/* Reject unit files not owned by root or writable by others */
	if (stat(path, &sb) == -1) {
		prowl_log(LOG_WARNING, "unit_load_file %s: stat: %m", path);
		return (-1);
	}
	if (sb.st_uid != 0) {
		prowl_log(LOG_WARNING,
		    "unit_load_file %s: not owned by root, skipping", path);
		return (-1);
	}
	if (sb.st_mode & S_IWOTH) {
		prowl_log(LOG_WARNING,
		    "unit_load_file %s: world-writable, skipping", path);
		return (-1);
	}

	parser = ucl_parser_new(UCL_PARSER_NO_FILEVARS);
	if (parser == NULL) {
		prowl_log(LOG_ERR, "ucl_parser_new failed");
		return (-1);
	}

	if (!ucl_parser_add_file(parser, path)) {
		/*
		 * Log only a bounded prefix of the parser error.  UCL error
		 * strings can include surrounding token text from the file,
		 * which may contain secret values from environment= blocks.
		 */
		prowl_log(LOG_ERR, "unit_load_file %s: %.128s", path,
		    ucl_parser_get_error(parser));
		ucl_parser_free(parser);
		return (-1);
	}

	root = ucl_parser_get_object(parser);
	ucl_parser_free(parser);

	if (root == NULL) {
		prowl_log(LOG_ERR, "unit_load_file %s: empty document", path);
		return (-1);
	}

	job = unit_parse_obj(root, path);
	ucl_object_unref(root);

	if (job == NULL)
		return (-1);

	TAILQ_INSERT_TAIL(&g_jobs, job, entries);
	prowl_log(LOG_DEBUG, "loaded unit %s from %s", job->label, path);

	return (ret);
}

/*
 * Load all .unit files from a directory.
 * Silently skips non-existent directories.
 */
int
unit_load_dir(const char *dirpath)
{
	DIR *dir;
	struct dirent *de;
	char path[PROWL_PATH_MAX];
	int loaded = 0;

	dir = opendir(dirpath);
	if (dir == NULL) {
		if (errno != ENOENT)
			prowl_log(LOG_WARNING, "unit_load_dir %s: %m",
			    dirpath);
		return (0);
	}

	while ((de = readdir(dir)) != NULL) {
		size_t namelen = strlen(de->d_name);

		/* Skip non-.unit files */
		if (namelen < 5 ||
		    strcmp(de->d_name + namelen - 5, ".unit") != 0)
			continue;

		snprintf(path, sizeof(path), "%s/%s", dirpath, de->d_name);

		if (unit_load_file(path) == 0)
			loaded++;
	}

	closedir(dir);

	if (loaded > 0)
		prowl_log(LOG_NOTICE, "loaded %d unit(s) from %s",
		    loaded, dirpath);

	return (loaded);
}
