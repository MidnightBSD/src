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

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <netinet/in.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>

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
	dep_entry_t *dep;
	const char *s, *sp;

	if (arr == NULL || ucl_object_type(arr) != UCL_ARRAY)
		return;

	while ((item = ucl_object_iterate(arr, &it, true)) != NULL) {
		if (job->deps_count >= PROWL_DEPS_MAX)
			break;
		if (ucl_object_type(item) != UCL_STRING)
			continue;
		s = ucl_object_tostring(item);
		dep = &job->deps[job->deps_count];
		dep->cond[0] = '\0';
		dep->activate = false;
		/* "?name service": only when the rc.conf variable holds */
		if (prowl_is_cond(s) &&
		    (sp = strchr(s, ' ')) != NULL) {
			size_t cl = (size_t)(sp - s);

			if (cl < 2 || cl >= PROWL_COND_LEN)
				continue;
			memcpy(dep->cond, s, cl);
			dep->cond[cl] = '\0';
			s = sp + strspn(sp, " ");
		}
		strlcpy(dep->name, s, PROWL_LABEL_MAX);
		dep->hard = hard;
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
 * Parse mdns block.
 */
static void
parse_mdns(const ucl_object_t *obj, mdns_config_t *mdns)
{
	const ucl_object_t *f, *txt, *val;
	ucl_object_iter_t it;
	int n;

	memset(mdns, 0, sizeof(*mdns));
	strlcpy(mdns->domain, "local", sizeof(mdns->domain));

	if (obj == NULL || ucl_object_type(obj) != UCL_OBJECT)
		return;

	f = ucl_object_lookup(obj, "register");
	if (f != NULL)
		mdns->register_service = ucl_object_toboolean(f);

	f = ucl_object_lookup(obj, "name");
	if (f != NULL && ucl_object_type(f) == UCL_STRING)
		strlcpy(mdns->name, ucl_object_tostring(f), sizeof(mdns->name));

	f = ucl_object_lookup(obj, "type");
	if (f != NULL && ucl_object_type(f) == UCL_STRING)
		strlcpy(mdns->type, ucl_object_tostring(f), sizeof(mdns->type));

	f = ucl_object_lookup(obj, "port");
	if (f != NULL)
		mdns->port = (int)ucl_object_toint(f);

	f = ucl_object_lookup(obj, "domain");
	if (f != NULL && ucl_object_type(f) == UCL_STRING)
		strlcpy(mdns->domain, ucl_object_tostring(f), sizeof(mdns->domain));

	txt = ucl_object_lookup(obj, "txt_record");
	if (txt != NULL && ucl_object_type(txt) == UCL_OBJECT) {
		it = NULL;
		n = 0;
		while ((val = ucl_object_iterate(txt, &it, true)) != NULL) {
			char kvbuf[PROWL_PATH_MAX];
			if (n >= PROWL_ENV_MAX - 1)
				break;
			snprintf(kvbuf, sizeof(kvbuf), "%s=%s",
			    ucl_object_key(val), ucl_object_tostring_forced(val));
			mdns->txt_record[n++] = strdup(kvbuf);
		}
		mdns->txt_count = n;
	}
}

static bool
parse_identity(const ucl_object_t *root, const char *key, char *name,
    size_t namesz, const char *path)
{
	const ucl_object_t *obj;
	const char *value;
	size_t len;

	obj = ucl_object_lookup(root, key);
	if (obj == NULL)
		return (true);
	if (obj->next != NULL ||
	    !ucl_object_tolstring_safe(obj, &value, &len) || len == 0 ||
	    len >= namesz || memchr(value, '\0', len) != NULL) {
		prowl_log(LOG_ERR, "unit %s: invalid %s", path, key);
		return (false);
	}
	memcpy(name, value, len);
	name[len] = '\0';
	return (true);
}

static bool
parse_socket_access(const ucl_object_t *obj, prowl_socket_t *sock,
    const char *path)
{
	char owner[MAXLOGNAME] = "", group[PROWL_LABEL_MAX] = "", mode[5] = "";
	struct passwd *pw;
	struct group *gr;
	unsigned long bits;

	if (!parse_identity(obj, "owner", owner, sizeof(owner), path) ||
	    !parse_identity(obj, "group", group, sizeof(group), path) ||
	    !parse_identity(obj, "mode", mode, sizeof(mode), path))
		return (false);
	if (sock->family != AF_UNIX && (*owner || *group || *mode))
		return (false);
	if (*mode) {
		bits = strtoul(mode, NULL, 8);
		if (mode[strspn(mode, "01234567")] != '\0' || bits > 0777)
			return (false);
		sock->mode = (mode_t)bits;
		sock->mode_set = true;
	}
	if (*owner) {
		pw = getpwnam(owner);
		if (pw == NULL || pw->pw_uid == (uid_t)-1)
			return (false);
		sock->owner = pw->pw_uid;
	}
	if (*group) {
		gr = getgrnam(group);
		if (gr == NULL || gr->gr_gid == (gid_t)-1)
			return (false);
		sock->group = gr->gr_gid;
	}
	return (true);
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

	if (!parse_identity(root, "user", job->user, sizeof(job->user), path) ||
	    !parse_identity(root, "group", job->group, sizeof(job->group), path)) {
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
	else {
		if (obj != NULL)
			prowl_log(LOG_WARNING, "%s: rc_name is not a single "
			    "string (set twice?), ignoring it", job->label);
		derive_rc_name(job->label, job->rc_name, sizeof(job->rc_name));
	}

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

	/* stop_arguments: run in place of a signal when the job is stopped */
	arr = ucl_object_lookup(root, "stop_arguments");
	if (arr != NULL && ucl_object_type(arr) == UCL_ARRAY) {
		it = NULL;
		n = 0;
		while ((item = ucl_object_iterate(arr, &it, true)) != NULL) {
			if (n >= PROWL_ARGS_MAX - 1)
				break;
			if (ucl_object_type(item) != UCL_STRING)
				continue;
			job->stop_arguments[n] =
			    strdup(ucl_object_tostring(item));
			if (job->stop_arguments[n] == NULL) {
				prowl_log(LOG_ERR, "strdup: %m");
				job->stop_argc = n;
				job_free(job);
				return (NULL);
			}
			n++;
		}
		job->stop_argc = n;
	}

	/* reload_arguments, check_arguments, resync_arguments */
	{
		static const struct {
			const char	*key;
			int		 action;
		} actions[] = {
			{ "reload_arguments", PROWL_ACTION_RELOAD },
			{ "check_arguments", PROWL_ACTION_CHECK },
			{ "resync_arguments", PROWL_ACTION_RESYNC },
		};
		size_t k;

		for (k = 0; k < sizeof(actions) / sizeof(actions[0]); k++) {
			int act = actions[k].action;

			arr = ucl_object_lookup(root, actions[k].key);
			if (arr == NULL || ucl_object_type(arr) != UCL_ARRAY)
				continue;
			it = NULL;
			n = 0;
			while ((item = ucl_object_iterate(arr, &it, true)) !=
			    NULL) {
				if (n >= PROWL_ARGS_MAX - 1)
					break;
				if (ucl_object_type(item) != UCL_STRING)
					continue;
				job->action_args[act][n] =
				    strdup(ucl_object_tostring(item));
				if (job->action_args[act][n] == NULL) {
					prowl_log(LOG_ERR, "strdup: %m");
					job->action_argc[act] = n;
					job_free(job);
					return (NULL);
				}
				n++;
			}
			job->action_argc[act] = n;
		}
	}

	/*
	 * start_pre, start_post, stop_pre, stop_post: lists of commands, each an array holding the
	 * program (an absolute path, or @name@) and its arguments.  A "-" in
	 * front of the program means a failure is ignored.
	 */
	{
		static const struct {
			const char	*key;
			int		 list;
		} lists[] = {
			{ "start_pre", PROWL_CMD_PRE },
			{ "stop_post", PROWL_CMD_POST },
			{ "start_post", PROWL_CMD_START_POST },
			{ "stop_pre", PROWL_CMD_STOP_PRE },
		};
		const ucl_object_t *cmdobj, *word;
		ucl_object_iter_t wit;
		size_t k;
		int d;

		for (k = 0; k < sizeof(lists) / sizeof(lists[0]); k++) {
			int l = lists[k].list;

			arr = ucl_object_lookup(root, lists[k].key);
			if (arr == NULL)
				continue;
			if ((l == PROWL_CMD_START_POST ||
			    l == PROWL_CMD_STOP_PRE) &&
			    job->type != JOB_TYPE_ONESHOT) {
				prowl_log(LOG_ERR, "%s: %s is for "
				    "oneshot jobs", job->label,
				    lists[k].key);
				job_free(job);
				return (NULL);
			}
			it = NULL;
			while (arr != NULL &&
			    (cmdobj = ucl_object_iterate(arr, &it, true)) !=
			    NULL) {
				prowl_cmd_t *c;
				const char *p;

				if (job->cmd_count[l] >= PROWL_CMDS_MAX ||
				    ucl_object_type(cmdobj) != UCL_ARRAY) {
					prowl_log(LOG_ERR, "%s: bad %s",
					    job->label, lists[k].key);
					job_free(job);
					return (NULL);
				}
				c = &job->cmds[l][job->cmd_count[l]];
				wit = NULL;
				while ((word = ucl_object_iterate(cmdobj,
				    &wit, true)) != NULL) {
					if (c->argc >= PROWL_CMD_ARGS - 1 ||
					    ucl_object_type(word) !=
					    UCL_STRING ||
					    (c->argv[c->argc] = strdup(
					    ucl_object_tostring(word))) ==
					    NULL) {
						prowl_log(LOG_ERR, "%s: bad %s",
						    job->label, lists[k].key);
						job->cmd_count[l]++;
						job_free(job);
						return (NULL);
					}
					c->argc++;
				}
				job->cmd_count[l]++;
				for (d = 0; d < prowl_cmd_first(c); d++) {
					if (c->argv[d][1] == '\0')
						break;
				}
				if (d < prowl_cmd_first(c)) {
					prowl_log(LOG_ERR, "%s: empty "
					    "condition in %s", job->label,
					    lists[k].key);
					job_free(job);
					return (NULL);
				}
				d = prowl_cmd_first(c);
				p = d < c->argc ? c->argv[d] : "";
				if (*p == '-')
					p++;
				if (*p != '/' && *p != '@') {
					prowl_log(LOG_ERR, "%s: %s needs an "
					    "absolute program", job->label,
					    lists[k].key);
					job_free(job);
					return (NULL);
				}
			}
		}
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

	/* pid_file, pid_name: the daemon forks and its parent exits */
	obj = ucl_object_lookup(root, "pid_file");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING) {
		const char *pf = ucl_object_tostring(obj);

		if (pf[0] != '/' || strlcpy(job->pid_file, pf,
		    sizeof(job->pid_file)) >= sizeof(job->pid_file)) {
			prowl_log(LOG_ERR, "%s: bad pid_file", job->label);
			job_free(job);
			return (NULL);
		}
	}
	obj = ucl_object_lookup(root, "pid_name");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING) {
		if (strlcpy(job->pid_name, ucl_object_tostring(obj),
		    sizeof(job->pid_name)) >= sizeof(job->pid_name) ||
		    job->pid_name[0] == '\0') {
			prowl_log(LOG_ERR, "%s: bad pid_name", job->label);
			job_free(job);
			return (NULL);
		}
	}
	if (job->pid_file[0] != '\0' && job->pid_name[0] != '\0') {
		prowl_log(LOG_ERR, "%s: pid_file and pid_name are exclusive",
		    job->label);
		job_free(job);
		return (NULL);
	}

	/* stop_signal: a name (TERM, USR1, ...) with or without SIG */
	obj = ucl_object_lookup(root, "stop_signal");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING) {
		const char *sn = ucl_object_tostring(obj);
		int sg;

		if (strncasecmp(sn, "SIG", 3) == 0)
			sn += 3;
		for (sg = 1; sg < NSIG; sg++) {
			if (sys_signame[sg] != NULL &&
			    strcasecmp(sn, sys_signame[sg]) == 0)
				break;
		}
		if (sg >= NSIG || sg == SIGKILL || sg == SIGSTOP) {
			prowl_log(LOG_ERR, "%s: bad stop_signal", job->label);
			job_free(job);
			return (NULL);
		}
		job->stop_signal = sg;
	}

	/* exit_timeout */
	obj = ucl_object_lookup(root, "exit_timeout");
	if (obj != NULL)
		job->exit_timeout = (int)ucl_object_toint(obj);

	/* requires (hard deps) */
	parse_deps(ucl_object_lookup(root, "requires"), job, true);

	/* activates: a hard dep that is started if it is disabled */
	{
		int first = job->deps_count, d;

		parse_deps(ucl_object_lookup(root, "activates"), job, true);
		for (d = first; d < job->deps_count; d++)
			job->deps[d].activate = true;
	}

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

	/* required_files: all must be readable for the job to start */
	arr = ucl_object_lookup(root, "required_files");
	if (arr != NULL) {
		if (ucl_object_type(arr) != UCL_ARRAY) {
			prowl_log(LOG_ERR, "unit_load_file %s: required_files "
			    "must be an array of paths", path);
			job_free(job);
			return (NULL);
		}
		it = NULL;
		while ((item = ucl_object_iterate(arr, &it, true)) != NULL) {
			s = ucl_object_tostring(item);
			if (ucl_object_type(item) != UCL_STRING || s == NULL ||
			    (s[0] != '/' && s[0] != '@') ||
			    strlen(s) >= sizeof(job->required_files[0]) ||
			    job->required_files_count >= PROWL_REQFILES_MAX) {
				/*
				 * Dropping an entry would silently weaken the
				 * check, so refuse the whole unit.
				 */
				prowl_log(LOG_ERR, "unit_load_file %s: bad "
				    "required_files entry", path);
				job_free(job);
				return (NULL);
			}
			strlcpy(job->required_files[job->required_files_count],
			    s, sizeof(job->required_files[0]));
			job->required_files_count++;
		}
	}

	/* required_modules: "file" or "file:module", loaded before the start */
	arr = ucl_object_lookup(root, "required_modules");
	if (arr != NULL) {
		if (ucl_object_type(arr) != UCL_ARRAY) {
			prowl_log(LOG_ERR, "unit_load_file %s: "
			    "required_modules must be an array", path);
			job_free(job);
			return (NULL);
		}
		it = NULL;
		while ((item = ucl_object_iterate(arr, &it, true)) != NULL) {
			s = ucl_object_tostring(item);
			if (ucl_object_type(item) != UCL_STRING || s == NULL ||
			    s[0] == '\0' || s[0] == ':' || s[0] == '/' ||
			    strstr(s, "..") != NULL ||
			    strlen(s) >= sizeof(job->required_modules[0]) ||
			    job->required_modules_count >= PROWL_MODULES_MAX) {
				prowl_log(LOG_ERR, "unit_load_file %s: bad "
				    "required_modules entry", path);
				job_free(job);
				return (NULL);
			}
			strlcpy(job->required_modules[
			    job->required_modules_count], s,
			    sizeof(job->required_modules[0]));
			job->required_modules_count++;
		}
	}

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
		const ucl_object_t *rl;

		rl = ucl_object_lookup(obj, "max_open_files");
		if (rl != NULL) {
			job->rlimits.nofile = (rlim_t)ucl_object_toint(rl);
			job->rlimits.set_nofile = true;
		}

		rl = ucl_object_lookup(obj, "max_processes");
		if (rl != NULL) {
			job->rlimits.nproc = (rlim_t)ucl_object_toint(rl);
			job->rlimits.set_nproc = true;
		}
	}

	/* schedule */
	parse_schedule(ucl_object_lookup(root, "schedule"), &job->schedule);

	/* mdns */
	parse_mdns(ucl_object_lookup(root, "mdns"), &job->mdns);

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
			sock->owner = (uid_t)-1;
			sock->group = (gid_t)-1;

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

			if (!parse_socket_access(sobj, sock, path)) {
				prowl_log(LOG_ERR, "%s: invalid access settings "
				    "for socket %s", job->label, sname);
				job_free(job);
				return (NULL);
			}
			job->sockets_count++;
		}
	}

	/* Determine enabled state from rc.conf */
	job->enabled = rcconf_is_enabled(job->rc_name, job->run_at_load);

	/* nojail: cannot work inside a jail (devd, ntpd, ...); never start */
	obj = ucl_object_lookup(root, "nojail");
	if (obj != NULL && ucl_object_toboolean(obj) && g_jailed)
		job->enabled = false;

	return (job);
}

/*
 * Instance names end up in labels and in file names: keep them boring.
 */
static bool
instance_valid(const char *inst)
{
	const char *p;

	if (inst == NULL || *inst == '\0' || strlen(inst) >= PROWL_INSTANCE_MAX)
		return (false);
	if (inst[0] == '.')
		return (false);
	for (p = inst; *p != '\0'; p++) {
		unsigned char c = (unsigned char)*p;

		if (!isalnum(c) && c != '.' && c != '-' && c != '_')
			return (false);
	}
	return (strstr(inst, "..") == NULL);
}

/*
 * A root-owned sticky ancestor (such as /tmp) protects root-owned
 * entries from replacement. Every component is checked before use.
 */
static int
trusted_check_fd(int fd, bool directory, bool ancestor, bool executable)
{
	struct stat sb;
	int error;

	if (fd == -1)
		return (-1);
	if (fstat(fd, &sb) == -1)
		error = errno;
	else if (directory ? !S_ISDIR(sb.st_mode) : !S_ISREG(sb.st_mode))
		error = EFTYPE;
	else if (sb.st_uid != 0 ||
	    ((sb.st_mode & (S_IWGRP | S_IWOTH)) != 0 &&
	    !(ancestor && (sb.st_mode & S_ISVTX) != 0)))
		error = EPERM;
	else if (executable &&
	    (sb.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) == 0)
		error = ENOEXEC;
	else
		return (fd);
	close(fd);
	errno = error;
	return (-1);
}

int
trusted_open_dir(const char *path)
{
	char full[PROWL_PATH_MAX], cwd[PROWL_PATH_MAX];
	char *rest, *name;
	int error, fd, nextfd, n;

	if (path[0] == '/') {
		n = snprintf(full, sizeof(full), "%s", path);
	} else {
		if (getcwd(cwd, sizeof(cwd)) == NULL)
			return (-1);
		n = snprintf(full, sizeof(full), "%s/%s", cwd, path);
	}
	if (n < 0 || (size_t)n >= sizeof(full)) {
		errno = ENAMETOOLONG;
		return (-1);
	}

	rest = full;
	while (*rest == '/')
		rest++;
	fd = trusted_check_fd(open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC),
	    true, *rest != '\0', false);
	while (fd != -1 && *rest != '\0') {
		name = strsep(&rest, "/");
		if (rest == NULL)
			rest = name + strlen(name);
		while (*rest == '/')
			rest++;
		nextfd = openat(fd, name,
		    O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
		error = errno;
		close(fd);
		errno = error;
		fd = trusted_check_fd(nextfd, true, *rest != '\0', false);
	}
	return (fd);
}

int
trusted_open_file_at(int dirfd, const char *name, bool executable)
{
	if (strchr(name, '/') != NULL) {
		errno = EINVAL;
		return (-1);
	}
	return (trusted_check_fd(openat(dirfd, name,
	    O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK), false, false,
	    executable));
}

int
trusted_open_file(const char *path, bool executable)
{
	char parent[PROWL_PATH_MAX];
	const char *name;
	int dirfd, fd, error;
	size_t len;

	name = strrchr(path, '/');
	if (name == NULL) {
		dirfd = trusted_open_dir(".");
		name = path;
	} else {
		len = name == path ? 1 : (size_t)(name - path);
		if (len >= sizeof(parent)) {
			errno = ENAMETOOLONG;
			return (-1);
		}
		memcpy(parent, path, len);
		parent[len] = '\0';
		dirfd = trusted_open_dir(parent);
		name++;
	}
	if (dirfd == -1)
		return (-1);
	fd = trusted_open_file_at(dirfd, name, executable);
	error = errno;
	close(dirfd);
	errno = error;
	return (fd);
}

bool
trusted_ucl_add_fd(struct ucl_parser *parser, int fd, const char *path)
{
	/* Preserve FILENAME and CURDIR for ordinary UCL files. */
	return (ucl_parser_set_filevars(parser, path, true) &&
	    ucl_parser_add_fd(parser, fd));
}

/*
 * Add the opened unit file to the parser.  For a template instance every
 * "%i" in the file is replaced by the instance name and "%%" by "%".
 */
static bool
add_unit_fd(struct ucl_parser *parser, int fd, const char *path,
    const char *inst)
{
	struct stat sb;
	char *in, *out, *o;
	const char *p;
	size_t ilen;
	ssize_t n;
	bool ok;

	if (inst == NULL)
		return (trusted_ucl_add_fd(parser, fd, path));

	if (fstat(fd, &sb) == -1 || sb.st_size < 0 ||
	    sb.st_size > UNIT_FILE_MAX)
		return (false);
	ilen = strlen(inst);
	in = malloc((size_t)sb.st_size + 1);
	out = malloc((size_t)sb.st_size * (ilen > 1 ? ilen : 1) + 1);
	if (in == NULL || out == NULL) {
		free(in);
		free(out);
		return (false);
	}
	n = pread(fd, in, (size_t)sb.st_size, 0);
	if (n < 0 || n != sb.st_size) {
		free(in);
		free(out);
		return (false);
	}
	in[n] = '\0';

	o = out;
	for (p = in; *p != '\0'; p++) {
		if (p[0] == '%' && p[1] == 'i') {
			memcpy(o, inst, ilen);
			o += ilen;
			p++;
		} else if (p[0] == '%' && p[1] == '%') {
			*o++ = '%';
			p++;
		} else
			*o++ = *p;
	}
	*o = '\0';
	ok = ucl_parser_add_chunk(parser, (const unsigned char *)out,
	    (size_t)(o - out));
	free(in);
	free(out);
	return (ok);
}

static job_t *
unit_apply_dropins(job_t *job, int basefd, const char *path, const char *inst)
{
	struct ucl_parser *parser = NULL;
	ucl_object_t *root;
	struct dirent *de;
	DIR *dir;
	job_t *replacement;
	char dropin_dir[PROWL_PATH_MAX], dropin_path[PROWL_PATH_MAX];
	const char *base, *parent;
	size_t len, parent_len;
	int dfd, fd, n;
	bool ok;

	parent = path;
	base = strrchr(path, '/');
	if (base == NULL) {
		base = path;
		parent = ".";
		parent_len = 1;
	} else {
		parent_len = base == path ? 1 : (size_t)(base - path);
		base++;
	}
	if (inst != NULL)
		n = snprintf(dropin_dir, sizeof(dropin_dir), "%.*s/%.*s.d",
		    (int)parent_len, parent, (int)(strlen(base) - 5), base);
	else
		n = snprintf(dropin_dir, sizeof(dropin_dir), "%.*s/%s.d",
		    (int)parent_len, parent, job->label);
	if (n < 0 || (size_t)n >= sizeof(dropin_dir)) {
		prowl_log(LOG_WARNING, "drop-in directory for %s: path too long",
		    path);
		goto fail;
	}

	dfd = trusted_open_dir(dropin_dir);
	if (dfd == -1) {
		if (errno == ENOENT)
			return (job);
		prowl_log(LOG_WARNING, "drop-in directory %s: %m",
		    dropin_dir);
		goto fail;
	}
	dir = fdopendir(dfd);
	if (dir == NULL) {
		prowl_log(LOG_WARNING, "drop-in directory %s: %m", dropin_dir);
		close(dfd);
		goto fail;
	}

	for (;;) {
		errno = 0;
		de = readdir(dir);
		if (de == NULL) {
			if (errno == 0)
				break;
			prowl_log(LOG_WARNING, "read drop-in directory %s: %m",
			    dropin_dir);
			goto fail_scan;
		}
		len = strlen(de->d_name);

		if (len < 5 || strcmp(de->d_name + len - 5, ".unit") != 0)
			continue;
		n = snprintf(dropin_path, sizeof(dropin_path), "%s/%s",
		    dropin_dir, de->d_name);
		if (n < 0 || (size_t)n >= sizeof(dropin_path)) {
			prowl_log(LOG_WARNING, "drop-in %s/%s: path too long",
			    dropin_dir, de->d_name);
			goto fail_scan;
		}
		fd = trusted_open_file_at(dfd, de->d_name, false);
		if (fd == -1) {
			prowl_log(LOG_WARNING, "drop-in %s: %m", dropin_path);
			goto fail_scan;
		}
		if (parser == NULL) {
			parser = ucl_parser_new(UCL_PARSER_NO_FILEVARS);
			if (parser == NULL ||
			    !add_unit_fd(parser, basefd, path, inst)) {
				close(fd);
				prowl_log(LOG_WARNING,
				    "drop-in re-parse %s failed", path);
				goto fail_scan;
			}
		}
		ok = add_unit_fd(parser, fd, dropin_path, inst);
		close(fd);
		if (!ok) {
			prowl_log(LOG_WARNING, "drop-in %s: %.128s",
			    dropin_path, ucl_parser_get_error(parser));
			goto fail_scan;
		}
		prowl_log(LOG_DEBUG, "applied drop-in %s", dropin_path);
	}
	closedir(dir);
	if (parser == NULL)
		return (job);
	root = ucl_parser_get_object(parser);
	ucl_parser_free(parser);
	if (root == NULL)
		goto fail;
	replacement = unit_parse_obj(root, path);
	ucl_object_unref(root);
	job_free(job);
	return (replacement);

fail_scan:
	closedir(dir);
	if (parser != NULL)
		ucl_parser_free(parser);
fail:
	job_free(job);
	return (NULL);
}

static int
unit_load_file_inst(const char *path, const char *inst)
{
	struct ucl_parser *parser;
	ucl_object_t *root;
	job_t *job;
	int fd;

	fd = trusted_open_file(path, false);
	if (fd == -1) {
		if (errno != ENOENT)
			prowl_log(LOG_WARNING, "unit_load_file %s: %m", path);
		return (-1);
	}

	parser = ucl_parser_new(UCL_PARSER_NO_FILEVARS);
	if (parser == NULL) {
		prowl_log(LOG_ERR, "ucl_parser_new failed");
		close(fd);
		return (-1);
	}

	if (!add_unit_fd(parser, fd, path, inst)) {
		/*
		 * Log only a bounded prefix of the parser error.  UCL error
		 * strings can include surrounding token text from the file,
		 * which may contain secret values from environment= blocks.
		 */
		prowl_log(LOG_ERR, "unit_load_file %s: %.128s", path,
		    ucl_parser_get_error(parser));
		ucl_parser_free(parser);
		close(fd);
		return (-1);
	}

	root = ucl_parser_get_object(parser);
	ucl_parser_free(parser);
	if (root == NULL) {
		prowl_log(LOG_ERR, "unit_load_file %s: empty document", path);
		close(fd);
		return (-1);
	}

	job = unit_parse_obj(root, path);
	ucl_object_unref(root);
	if (job == NULL) {
		close(fd);
		return (-1);
	}

	job = unit_apply_dropins(job, fd, path, inst);
	close(fd);
	if (job == NULL)
		return (-1);
	TAILQ_INSERT_TAIL(&g_jobs, job, entries);
	prowl_log(LOG_DEBUG, "loaded unit %s from %s", job->label, path);
	return (0);
}

static bool
is_template_name(const char *name)
{
	size_t len = strlen(name);

	return (len > 6 && strcmp(name + len - 6, "@.unit") == 0);
}

/*
 * Instantiate the template at path for the instances named by its
 * "instances_var" rc.conf variable, or for "default_instance" if the
 * variable is empty or unset.
 */
static int
unit_load_template(const char *path)
{
	struct ucl_parser *parser;
	ucl_object_t *root;
	const ucl_object_t *obj;
	char list[1024], *p, *tok;
	const char *def = NULL;
	int fd, n = 0;
	bool ok;

	fd = trusted_open_file(path, false);
	if (fd == -1) {
		if (errno != ENOENT)
			prowl_log(LOG_WARNING, "unit_load_template %s: %m", path);
		return (-1);
	}
	parser = ucl_parser_new(UCL_PARSER_NO_FILEVARS);
	if (parser == NULL) {
		close(fd);
		return (-1);
	}
	ok = add_unit_fd(parser, fd, path, NULL);
	close(fd);
	if (!ok) {
		prowl_log(LOG_ERR, "unit_load_template %s: %.128s", path,
		    ucl_parser_get_error(parser));
		ucl_parser_free(parser);
		return (-1);
	}

	root = ucl_parser_get_object(parser);
	ucl_parser_free(parser);
	if (root == NULL)
		return (-1);

	list[0] = '\0';
	obj = ucl_object_lookup(root, "instances_var");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING)
		(void)rcconf_var_get(ucl_object_tostring(obj), list,
		    sizeof(list));
	obj = ucl_object_lookup(root, "default_instance");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING)
		def = ucl_object_tostring(obj);

	p = list;
	while ((tok = strsep(&p, " \t\n")) != NULL) {
		if (*tok == '\0')
			continue;
		if (!instance_valid(tok)) {
			prowl_log(LOG_WARNING,
			    "unit %s: invalid instance name '%.32s'",
			    path, tok);
			continue;
		}
		if (unit_load_file_inst(path, tok) == 0)
			n++;
	}
	if (n == 0 && list[0] == '\0' && def != NULL && instance_valid(def)) {
		if (unit_load_file_inst(path, def) == 0)
			n++;
	}
	ucl_object_unref(root);
	return (n > 0 ? 0 : -1);
}

int
unit_load_file(const char *path)
{
	const char *base = strrchr(path, '/');

	base = base != NULL ? base + 1 : path;
	if (is_template_name(base))
		return (unit_load_template(path));
	return (unit_load_file_inst(path, NULL));
}

/*
 * Create the instance of a template named by label, e.g. the label
 * "org.midnightbsd.pflog@pflog1" from "pflog@.unit".  Returns the new job
 * or NULL if there is no such template.
 */
job_t *
unit_instantiate(const char *label)
{
	char stem[PROWL_LABEL_MAX], path[PROWL_PATH_MAX];
	const char *dirs[5], *at, *dot;
	size_t len;
	int i;

	at = strchr(label, '@');
	if (at == NULL || !label_valid(label) || !instance_valid(at + 1))
		return (NULL);
	dot = at;
	while (dot > label && dot[-1] != '.')
		dot--;
	len = (size_t)(at - dot);
	if (len == 0 || len >= sizeof(stem))
		return (NULL);
	memcpy(stem, dot, len);
	stem[len] = '\0';

	if (g_unit_override[0] != '\0') {
		dirs[0] = g_unit_override;
		dirs[1] = NULL;
	} else {
		dirs[0] = UNIT_DIR_BASE;
		dirs[1] = UNIT_DIR_LOCAL;
		dirs[2] = UNIT_DIR_OVERRIDE;
		dirs[3] = g_generated_dir;
		dirs[4] = NULL;
	}
	for (i = 0; dirs[i] != NULL; i++) {
		if (snprintf(path, sizeof(path), "%s/%s@.unit", dirs[i], stem) >=
		    (int)sizeof(path))
			continue;
		if (unit_load_file_inst(path, at + 1) == 0) {
			job_t *job = job_find_instance(label);

			return (job != NULL ? job : job_find_by_label(label));
		}
	}
	return (NULL);
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
	int fd, loaded = 0;

	fd = trusted_open_dir(dirpath);
	if (fd == -1) {
		if (errno != ENOENT)
			prowl_log(LOG_WARNING, "unit_load_dir %s: %m",
			    dirpath);
		return (0);
	}

	dir = fdopendir(fd);
	if (dir == NULL) {
		prowl_log(LOG_WARNING, "unit_load_dir %s: %m", dirpath);
		close(fd);
		return (0);
	}

	while ((de = readdir(dir)) != NULL) {
		size_t namelen = strlen(de->d_name);

		/* Skip non-.unit files */
		if (namelen < 5 ||
		    strcmp(de->d_name + namelen - 5, ".unit") != 0)
			continue;

		if (snprintf(path, sizeof(path), "%s/%s", dirpath, de->d_name) >=
		    (int)sizeof(path))
			continue;

		if (unit_load_file(path) == 0)
			loaded++;
	}

	closedir(dir);

	if (loaded > 0)
		prowl_log(LOG_NOTICE, "loaded %d unit(s) from %s",
		    loaded, dirpath);

	return (loaded);
}
