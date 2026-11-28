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
#include <sys/acl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <arpa/inet.h>
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
 * Parse a string array from a UCL node into *dest, grown to hold at most
 * maxn labels.  Returns the number of entries filled, or -1 if memory runs
 * out (*dest then holds what was filled so far, for job_free() to release).
 */
static int
parse_str_array(const ucl_object_t *arr, char (**dest)[PROWL_LABEL_MAX],
    int maxn)
{
	const ucl_object_t *item;
	ucl_object_iter_t it = NULL;
	char (*nd)[PROWL_LABEL_MAX];
	int n = 0;

	if (arr == NULL || ucl_object_type(arr) != UCL_ARRAY)
		return (0);

	while ((item = ucl_object_iterate(arr, &it, true)) != NULL) {
		if (n >= maxn)
			break;
		if (ucl_object_type(item) != UCL_STRING)
			continue;
		nd = prowl_array_add(*dest, n, sizeof(**dest));
		if (nd == NULL)
			return (-1);
		*dest = nd;
		strlcpy((*dest)[n], ucl_object_tostring(item),
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
 * Returns false if memory runs out.
 */
static bool
parse_deps(const ucl_object_t *arr, job_t *job, bool hard)
{
	const ucl_object_t *item;
	ucl_object_iter_t it = NULL;
	dep_entry_t *dep;
	const char *s, *sp;

	if (arr == NULL || ucl_object_type(arr) != UCL_ARRAY)
		return (true);

	while ((item = ucl_object_iterate(arr, &it, true)) != NULL) {
		if (job->deps_count >= PROWL_DEPS_MAX)
			break;
		if (ucl_object_type(item) != UCL_STRING)
			continue;
		s = ucl_object_tostring(item);
		/* "?name service": only when the rc.conf variable holds */
		sp = NULL;
		if (prowl_is_cond(s) &&
		    (sp = strchr(s, ' ')) != NULL) {
			size_t cl = (size_t)(sp - s);

			if (cl < 2 || cl >= PROWL_COND_LEN)
				continue;
		}
		dep = prowl_array_add(job->deps, job->deps_count,
		    sizeof(*job->deps));
		if (dep == NULL)
			return (false);
		job->deps = dep;
		dep = &job->deps[job->deps_count];
		if (sp != NULL) {
			size_t cl = (size_t)(sp - s);

			memcpy(dep->cond, s, cl);
			dep->cond[cl] = '\0';
			s = sp + strspn(sp, " ");
		}
		strlcpy(dep->name, s, PROWL_LABEL_MAX);
		dep->hard = hard;
		job->deps_count++;
	}
	return (true);
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
static bool
parse_mdns(const ucl_object_t *obj, mdns_config_t **mdnsp)
{
	const ucl_object_t *f, *txt, *val;
	mdns_config_t *mdns;
	ucl_object_iter_t it;
	int n;

	/* Only a unit with an mdns block pays for one */
	if (obj == NULL || ucl_object_type(obj) != UCL_OBJECT)
		return (true);
	mdns = calloc(1, sizeof(*mdns));
	if (mdns == NULL) {
		prowl_log(LOG_ERR, "parse_mdns: %m");
		return (false);
	}
	*mdnsp = mdns;
	strlcpy(mdns->domain, "local", sizeof(mdns->domain));

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
			mdns->txt_record[n] = strdup(kvbuf);
			if (mdns->txt_record[n] == NULL) {
				prowl_log(LOG_ERR, "parse_mdns: %m");
				mdns->txt_count = n;
				return (false);
			}
			n++;
		}
		mdns->txt_count = n;
	}
	return (true);
}

static bool
parse_optional_string(const ucl_object_t *root, const char *key, char *dest,
    size_t destsz, const char *path)
{
	const ucl_object_t *obj;
	const char *value;
	size_t len;

	obj = ucl_object_lookup(root, key);
	if (obj == NULL)
		return (true);
	if (obj->next != NULL ||
	    !ucl_object_tolstring_safe(obj, &value, &len) || len == 0 ||
	    len >= destsz || memchr(value, '\0', len) != NULL) {
		prowl_log(LOG_ERR, "unit %s: invalid %s", path, key);
		return (false);
	}
	memcpy(dest, value, len);
	dest[len] = '\0';
	return (true);
}

static bool
parse_umask(const ucl_object_t *root, job_t *job)
{
	const ucl_object_t *obj;
	const char *value;
	size_t len, i;
	unsigned int bits;

	obj = ucl_object_lookup(root, "umask");
	if (obj == NULL)
		return (true);
	if (obj->next != NULL ||
	    !ucl_object_tolstring_safe(obj, &value, &len) || len == 0)
		return (false);

	bits = 0;
	for (i = 0; i < len; i++) {
		if (value[i] < '0' || value[i] > '7')
			return (false);
		bits = (bits << 3) | (value[i] - '0');
		if (bits > 0777)
			return (false);
	}
	job->umask_val = (mode_t)bits;
	job->umask_set = true;
	return (true);
}

static bool
parse_socket_address(const ucl_object_t *obj, prowl_socket_t *sock,
    const char *path)
{
	const ucl_object_t *port;
	struct in_addr addr;
	struct in6_addr addr6;
	int64_t value;

	if (!parse_optional_string(obj, "path", sock->path,
	    sizeof(((struct sockaddr_un *)0)->sun_path), path) ||
	    !parse_optional_string(obj, "host", sock->host, sizeof(sock->host),
	    path))
		return (false);

	port = ucl_object_lookup(obj, "port");
	if (sock->path[0] != '\0') {
		if (port != NULL || sock->host[0] != '\0')
			return (false);
		sock->family = AF_UNIX;
		return (true);
	}
	if (port == NULL || port->next != NULL ||
	    ucl_object_type(port) != UCL_INT)
		return (false);
	value = ucl_object_toint(port);
	if (value < 0 || value > UINT16_MAX)
		return (false);

	sock->port = (int)value;
	if (sock->host[0] == '\0' || inet_pton(AF_INET, sock->host, &addr) == 1)
		sock->family = AF_INET;
	else if (inet_pton(AF_INET6, sock->host, &addr6) == 1)
		sock->family = AF_INET6;
	else
		return (false);
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

	if (!parse_optional_string(obj, "owner", owner, sizeof(owner), path) ||
	    !parse_optional_string(obj, "group", group, sizeof(group), path) ||
	    !parse_optional_string(obj, "mode", mode, sizeof(mode), path))
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
	if (!label_valid(ucl_object_tostring(obj))) {
		prowl_log(LOG_ERR, "unit %s: invalid label '%s', skipping",
		    path, job->label);
		job_free(job);
		return (NULL);
	}

	if (!parse_optional_string(root, "user", job->user, sizeof(job->user),
	    path) ||
	    !parse_optional_string(root, "group", job->group, sizeof(job->group),
	    path)) {
		job_free(job);
		return (NULL);
	}

	/* description */
	obj = ucl_object_lookup(root, "description");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING &&
	    job_set_str(&job->description, ucl_object_tostring(obj),
	    PROWL_DESC_MAX) == -1) {
		job_free(job);
		return (NULL);
	}

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
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING &&
	    job_set_str(&job->program, ucl_object_tostring(obj),
	    PROWL_PATH_MAX) == -1) {
		job_free(job);
		return (NULL);
	}

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
				char **v;

				if (n >= PROWL_ARGS_MAX - 1)
					break;
				if (ucl_object_type(item) != UCL_STRING)
					continue;
				/* Room for this word and a NULL after it */
				v = reallocarray(job->action_args[act],
				    (size_t)n + 2, sizeof(*v));
				if (v == NULL) {
					prowl_log(LOG_ERR, "reallocarray: %m");
					job->action_argc[act] = n;
					job_free(job);
					return (NULL);
				}
				job->action_args[act] = v;
				v[n + 1] = NULL;
				v[n] = strdup(ucl_object_tostring(item));
				if (v[n] == NULL) {
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
				c = prowl_array_add(job->cmds[l],
				    job->cmd_count[l], sizeof(*c));
				if (c == NULL) {
					job_free(job);
					return (NULL);
				}
				job->cmds[l] = c;
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
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING &&
	    job_set_str(&job->working_directory, ucl_object_tostring(obj),
	    PROWL_PATH_MAX) == -1) {
		job_free(job);
		return (NULL);
	}

	/* root_directory */
	{
		char rootdir[PROWL_PATH_MAX] = "";

		if (!parse_optional_string(root, "root_directory", rootdir,
		    sizeof(rootdir), path) ||
		    job_set_str(&job->root_directory, rootdir,
		    sizeof(rootdir)) == -1) {
			job_free(job);
			return (NULL);
		}
	}

	/* umask */
	if (!parse_umask(root, job)) {
		prowl_log(LOG_ERR, "unit %s: invalid umask", path);
		job_free(job);
		return (NULL);
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

		if (pf[0] != '/' || strlen(pf) >= PROWL_PATH_MAX ||
		    job_set_str(&job->pid_file, pf, PROWL_PATH_MAX) == -1) {
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
	if (job->pid_file != NULL && job->pid_name[0] != '\0') {
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
	if (!parse_deps(ucl_object_lookup(root, "requires"), job, true)) {
		job_free(job);
		return (NULL);
	}

	/* activates: a hard dep that is started if it is disabled */
	{
		int first = job->deps_count, d;

		if (!parse_deps(ucl_object_lookup(root, "activates"), job,
		    true)) {
			job_free(job);
			return (NULL);
		}
		for (d = first; d < job->deps_count; d++)
			job->deps[d].activate = true;
	}

	/* wants (soft deps) */
	if (!parse_deps(ucl_object_lookup(root, "wants"), job, false)) {
		job_free(job);
		return (NULL);
	}

	/* before, after, conflicts */
	job->before_count = parse_str_array(ucl_object_lookup(root, "before"),
	    &job->before, PROWL_DEPS_MAX);
	job->after_count = parse_str_array(ucl_object_lookup(root, "after"),
	    &job->after, PROWL_DEPS_MAX);
	job->conflicts_count = parse_str_array(ucl_object_lookup(root,
	    "conflicts"), &job->conflicts, PROWL_DEPS_MAX);
	if (job->before_count == -1 || job->after_count == -1 ||
	    job->conflicts_count == -1) {
		job_free(job);
		return (NULL);
	}

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
			{
				char (*rf)[PROWL_PATH_MAX];

				rf = prowl_array_add(job->required_files,
				    job->required_files_count,
				    sizeof(*job->required_files));
				if (rf == NULL) {
					job_free(job);
					return (NULL);
				}
				job->required_files = rf;
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
	job->provides_count = parse_str_array(arr, &job->provides,
	    PROWL_PROVIDES_MAX);
	if (job->provides_count == -1) {
		job_free(job);
		return (NULL);
	}

	/* Console input while PID 1 boots (passphrase prompts) */
	obj = ucl_object_lookup(root, "interactive");
	if (obj != NULL) {
		if (ucl_object_type(obj) != UCL_BOOLEAN) {
			prowl_log(LOG_ERR, "unit %s: invalid interactive",
			    path);
			job_free(job);
			return (NULL);
		}
		job->interactive = ucl_object_toboolean(obj);
	}

	/* I/O paths */
	obj = ucl_object_lookup(root, "standard_in_path");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING &&
	    job_set_str(&job->stdin_path, ucl_object_tostring(obj),
	    PROWL_PATH_MAX) == -1) {
		job_free(job);
		return (NULL);
	}

	obj = ucl_object_lookup(root, "standard_out_path");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING &&
	    job_set_str(&job->stdout_path, ucl_object_tostring(obj),
	    PROWL_PATH_MAX) == -1) {
		job_free(job);
		return (NULL);
	}

	obj = ucl_object_lookup(root, "standard_error_path");
	if (obj != NULL && ucl_object_type(obj) == UCL_STRING &&
	    job_set_str(&job->stderr_path, ucl_object_tostring(obj),
	    PROWL_PATH_MAX) == -1) {
		job_free(job);
		return (NULL);
	}

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
	if (!parse_mdns(ucl_object_lookup(root, "mdns"), &job->mdns)) {
		job_free(job);
		return (NULL);
	}

	/*
	 * sockets: object keyed by name, each value describes one socket.
	 * Example:
	 *   "sockets": {
	 *     "http": { "path": "/var/run/svc.sock", "type": "stream" },
	 *     "rpc":  { "port": 9000, "host": "127.0.0.1" }
	 *   }
	 */
	obj = ucl_object_lookup(root, "sockets");
	if (obj != NULL) {
		const ucl_object_t *sobj;
		ucl_object_iter_t sit;

		if (obj->next != NULL || ucl_object_type(obj) != UCL_OBJECT) {
			prowl_log(LOG_ERR, "unit %s: invalid sockets", path);
			job_free(job);
			return (NULL);
		}
		if (obj->len > PROWL_SOCKETS_MAX) {
			prowl_log(LOG_ERR, "unit %s: too many sockets "
			    "(max %d)", path, PROWL_SOCKETS_MAX);
			job_free(job);
			return (NULL);
		}
		/*
		 * Sized once: kqueue events point at these entries, so the
		 * array must not move after the sockets are bound.
		 */
		if (obj->len > 0) {
			job->sockets = calloc(obj->len, sizeof(*job->sockets));
			if (job->sockets == NULL) {
				prowl_log(LOG_ERR, "unit %s: %m", path);
				job_free(job);
				return (NULL);
			}
			for (unsigned int si = 0; si < obj->len; si++) {
				job->sockets[si].fd = -1;
				job->sockets[si].job = job;
			}
		}
		sit = ucl_object_iterate_new(obj);
		if (sit == NULL) {
			job_free(job);
			return (NULL);
		}

		while ((sobj = ucl_object_iterate_safe(sit, true)) != NULL) {
			const char *sname;
			const ucl_object_t *f;
			prowl_socket_t *sock;

			if ((unsigned int)job->sockets_count >= obj->len) {
				prowl_log(LOG_ERR, "unit %s: too many sockets",
				    path);
				break;
			}
			if (sobj->next != NULL ||
			    ucl_object_type(sobj) != UCL_OBJECT) {
				prowl_log(LOG_ERR, "unit %s: invalid socket "
				    "description", path);
				break;
			}

			sname = ucl_object_key(sobj);
			sock = &job->sockets[job->sockets_count];

			strlcpy(sock->name, sname, sizeof(sock->name));

			/* Defaults */
			sock->socktype = SOCK_STREAM;
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

			if (!parse_socket_address(sobj, sock, path)) {
				prowl_log(LOG_ERR, "unit %s: invalid address "
				    "for socket %s", path, sname);
				break;
			}

			/* backlog */
			f = ucl_object_lookup(sobj, "backlog");
			if (f != NULL)
				sock->backlog = (int)ucl_object_toint(f);

			if (!parse_socket_access(sobj, sock, path)) {
				prowl_log(LOG_ERR, "%s: invalid access settings "
				    "for socket %s", job->label, sname);
				break;
			}
			job->sockets_count++;
		}
		ucl_object_iterate_free(sit);
		if (obj->len != (unsigned int)job->sockets_count) {
			job_free(job);
			return (NULL);
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
 * NFSv4 ACLs can grant mutation rights that are absent from st_mode.
 * Check grants in order. An earlier deny for everyone or the same
 * principal masks a grant; membership in unrelated groups is optional.
 */
static int
trusted_acl_check(acl_t acl, const struct stat *sb, bool sticky)
{
	struct {
		acl_tag_t tag;
		uid_t id;
		acl_perm_t perms;
	} denies[ACL_MAX_ENTRIES];
	acl_entry_t ace;
	acl_entry_type_t type;
	acl_flagset_t flags;
	acl_permset_t perms;
	acl_perm_t mutation, rights;
	acl_tag_t tag;
	uid_t id, *qualifier;
	size_t i, ndenies;
	int entry;

	mutation = ACL_WRITE_ACL | ACL_WRITE_OWNER | ACL_WRITE_ATTRIBUTES |
	    ACL_WRITE_NAMED_ATTRS | ACL_DELETE;
	if (S_ISDIR(sb->st_mode))
		mutation |= ACL_DELETE_CHILD;
	if (!sticky)
		mutation |= ACL_WRITE_DATA | ACL_APPEND_DATA;

	ndenies = 0;
	for (entry = acl_get_entry(acl, ACL_FIRST_ENTRY, &ace); entry == 1;
	    entry = acl_get_entry(acl, ACL_NEXT_ENTRY, &ace)) {
		if (acl_get_entry_type_np(ace, &type) == -1 ||
		    acl_get_flagset_np(ace, &flags) == -1 ||
		    acl_get_permset(ace, &perms) == -1 ||
		    acl_get_tag_type(ace, &tag) == -1)
			return (errno);
		if ((type != ACL_ENTRY_TYPE_ALLOW && type != ACL_ENTRY_TYPE_DENY) ||
		    acl_get_flag_np(flags, ACL_ENTRY_INHERIT_ONLY) == 1)
			continue;
		rights = (acl_perm_t)*perms & mutation;
		if (rights == 0 || tag == ACL_USER_OBJ)
			continue;
		id = 0;
		if (tag == ACL_GROUP_OBJ) {
			tag = ACL_GROUP;
			id = sb->st_gid;
		} else if (tag == ACL_USER || tag == ACL_GROUP) {
			qualifier = acl_get_qualifier(ace);
			if (qualifier == NULL)
				return (errno);
			id = *qualifier;
			acl_free(qualifier);
			if (tag == ACL_USER && id == 0)
				continue;
		} else if (tag != ACL_EVERYONE)
			return (EINVAL);

		if (type == ACL_ENTRY_TYPE_DENY) {
			if (ndenies == nitems(denies))
				return (EINVAL);
			denies[ndenies].tag = tag;
			denies[ndenies].id = id;
			denies[ndenies++].perms = rights;
			continue;
		}
		for (i = 0; i < ndenies; i++)
			if (denies[i].tag == ACL_EVERYONE ||
			    (denies[i].tag == tag && denies[i].id == id))
				rights &= ~denies[i].perms;
		if (rights != 0)
			return (EPERM);
	}
	return (entry == -1 ? errno : 0);
}

static int
trusted_check_acl_fd(int fd, const struct stat *sb, bool sticky)
{
	acl_t acl;
	long supported;
	int error;

	errno = 0;
	supported = fpathconf(fd, _PC_ACL_NFS4);
	if (supported == 0)
		return (0);
	if (supported == -1) {
		if (errno == EINVAL || errno == EOPNOTSUPP)
			return (0);
		if (errno != 0)
			return (errno);
		/* An indeterminate limit does not establish that ACLs are absent. */
	}
	acl = acl_get_fd_np(fd, ACL_TYPE_NFS4);
	if (acl == NULL)
		return (errno);
	error = trusted_acl_check(acl, sb, sticky);
	acl_free(acl);
	return (error);
}

/*
 * A root-owned sticky ancestor (such as /tmp) protects root-owned
 * entries from replacement when deletion is not explicitly granted.
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
	else if ((error = trusted_check_acl_fd(fd, &sb,
	    ancestor && (sb.st_mode & S_ISVTX) != 0)) == 0)
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
	struct stat sb;
	int fd = -1, pathfd, error;

	if (strchr(name, '/') != NULL) {
		errno = EINVAL;
		return (-1);
	}
	/* Inspect the leaf without invoking a FIFO or device open routine. */
	pathfd = openat(dirfd, name, O_PATH | O_NOFOLLOW | O_CLOEXEC);
	if (pathfd == -1)
		return (-1);
	if (fstat(pathfd, &sb) == 0) {
		if (S_ISREG(sb.st_mode))
			fd = openat(pathfd, "", O_EMPTY_PATH | O_RDONLY | O_CLOEXEC);
		else
			errno = S_ISLNK(sb.st_mode) ? ELOOP : EFTYPE;
	}
	error = errno;
	close(pathfd);
	errno = error;
	return (trusted_check_fd(fd, false, false, executable));
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
    const char *inst, enum ucl_duplicate_strategy strategy)
{
	struct stat sb;
	char *in, *out, *o;
	const char *p;
	size_t ilen;
	ssize_t n;
	bool ok;

	if (!ucl_parser_set_filevars(parser, path, true))
		return (false);
	if (inst == NULL)
		return (ucl_parser_add_fd_full(parser, fd, 0, strategy,
		    UCL_PARSE_UCL));

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
	if (n < 0 || n != sb.st_size || memchr(in, '\0', (size_t)n) != NULL) {
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
	ok = ucl_parser_add_chunk_full(parser, (const unsigned char *)out,
	    (size_t)(o - out), 0, strategy, UCL_PARSE_UCL);
	free(in);
	free(out);
	return (ok);
}

/*
 * Rejected native policy must continue to shadow lower definitions and
 * rc.d scripts without exposing the rejected job to the scheduler or IPC.
 * Only trusted unit contents and directory entry names can make claims.
 */
#define CLAIM_FILE	0x01
#define CLAIM_LABEL	0x02
#define CLAIM_PROVIDER	0x04
#define CLAIM_SELECTED	0x08

struct unit_claim {
	SLIST_ENTRY(unit_claim) link;
	unsigned flags;
	char name[];
};
static SLIST_HEAD(, unit_claim) unit_claims =
    SLIST_HEAD_INITIALIZER(unit_claims);
/* Keep the winning template by declared label, independently of filename. */
struct unit_template {
	STAILQ_ENTRY(unit_template) link;
	char pattern[2 * PROWL_LABEL_MAX];
	char path[PROWL_PATH_MAX];	/* empty for rejected policy */
};
static STAILQ_HEAD(, unit_template) templates = STAILQ_HEAD_INITIALIZER(templates);
static bool scan_incomplete, admin_dropins, loading_all;

static const struct unit_template *template_for(const char *, const char *);

static unsigned
claims_for(const char *name)
{
	struct unit_claim *claim;

	SLIST_FOREACH(claim, &unit_claims, link) {
		if (strcmp(claim->name, name) == 0)
			return (claim->flags);
	}
	return (0);
}

static void
claim_name(const char *name, unsigned flags)
{
	struct unit_claim *claim;
	size_t size = strlen(name) + 1;

	SLIST_FOREACH(claim, &unit_claims, link) {
		if (strcmp(claim->name, name) == 0) {
			claim->flags |= flags;
			return;
		}
	}
	claim = malloc(sizeof(*claim) + size);
	if (claim == NULL) {
		prowl_log(LOG_ERR, "cannot reserve native unit name %s", name);
		scan_incomplete = true;
		return;
	}
	memcpy(claim->name, name, size);
	claim->flags = flags;
	SLIST_INSERT_HEAD(&unit_claims, claim, link);
}

bool
unit_label_reserved(const char *label)
{
	return (scan_incomplete ||
	    (claims_for(label) & (CLAIM_SELECTED | CLAIM_LABEL)) != 0);
}

bool
unit_provider_rejected(const char *name)
{
	return (scan_incomplete || (claims_for(name) & CLAIM_PROVIDER) != 0);
}

static void
reserve_identity(const char *label, const ucl_object_t *rc,
    const ucl_object_t *provides)
{
	const ucl_object_t *entry;
	ucl_object_iter_t iter = NULL;
	char rc_name[PROWL_LABEL_MAX];

	claim_name(label, CLAIM_LABEL);
	if (rc != NULL && ucl_object_type(rc) == UCL_STRING)
		claim_name(ucl_object_tostring(rc), CLAIM_PROVIDER);
	else {
		derive_rc_name(label, rc_name, sizeof(rc_name));
		claim_name(rc_name, CLAIM_PROVIDER);
	}
	if (provides != NULL && ucl_object_type(provides) == UCL_ARRAY) {
		while ((entry = ucl_object_iterate(provides, &iter, true)) != NULL) {
			if (ucl_object_type(entry) == UCL_STRING)
				claim_name(ucl_object_tostring(entry),
				    CLAIM_PROVIDER);
		}
	}
}

static int
is_unit_entry(const struct dirent *de)
{
	size_t len = strlen(de->d_name);

	return (len >= 5 && strcmp(de->d_name + len - 5, ".unit") == 0);
}

/* Each fragment is a complete UCL/JSON document. Replace whole fields. */
static bool
merge_dropin(ucl_object_t *root, int fd, const char *path, const char *inst)
{
	struct ucl_parser *parser;
	ucl_object_t *fragment, *value;
	const ucl_object_t *field;
	ucl_object_iter_t iter = NULL;
	const char *key;
	size_t len;
	bool ok;

	parser = ucl_parser_new(UCL_PARSER_NO_FILEVARS);
	if (parser == NULL)
		return (false);
	ok = add_unit_fd(parser, fd, path, inst, UCL_DUPLICATE_APPEND);
	if (!ok)
		prowl_log(LOG_ERR, "invalid drop-in %s at line %u, column %u",
		    path, ucl_parser_get_linenum(parser),
		    ucl_parser_get_column(parser));
	fragment = ok ? ucl_parser_get_object(parser) : NULL;
	ucl_parser_free(parser);
	if (fragment == NULL)
		return (ok);	/* empty fragment */
	ok = ucl_object_type(fragment) == UCL_OBJECT;
	if (!ok)
		prowl_log(LOG_ERR, "drop-in %s must contain an object", path);
	while (ok && (field = ucl_object_iterate(fragment, &iter, true)) != NULL) {
		key = ucl_object_keyl(field, &len);
		if (field->next != NULL) {
			prowl_log(LOG_ERR, "duplicate field in drop-in %s", path);
			ok = false;
			break;
		}
		value = ucl_object_ref(field);
		if (value == NULL) {
			ok = false;
			break;
		}
		/* replace_key also inserts absent keys and then returns false. */
		(void)ucl_object_replace_key(root, value, key, len, false);
	}
	ucl_object_unref(fragment);
	return (ok);
}

static bool
add_dropin(ucl_object_t *root, int dfd, const char *dir, const char *name,
    const char *inst)
{
	char path[PROWL_PATH_MAX];
	int fd;
	bool ok;

	if (snprintf(path, sizeof(path), "%s/%s", dir, name) >=
	    (int)sizeof(path)) {
		prowl_log(LOG_ERR, "drop-in path too long in %s", dir);
		return (false);
	}
	fd = trusted_open_file_at(dfd, name, false);
	if (fd == -1) {
		prowl_log(LOG_ERR, "drop-in %s: %m", path);
		return (false);
	}
	ok = merge_dropin(root, fd, path, inst);
	close(fd);
	return (ok);
}

/* Read fragments in filename order from the directory that was checked. */
static bool
add_dropin_dir(ucl_object_t *root, const char *parent, const char *key,
    const char *inst)
{
	struct dirent **names;
	char dir[PROWL_PATH_MAX];
	int dfd, count, i;
	bool ok;

	if (snprintf(dir, sizeof(dir), "%s/%s.d", parent, key) >=
	    (int)sizeof(dir)) {
		prowl_log(LOG_ERR, "drop-in path too long for %s", key);
		return (false);
	}
	dfd = trusted_open_dir(dir);
	if (dfd == -1) {
		if (errno == ENOENT)
			return (true);
		prowl_log(LOG_ERR, "drop-in directory %s: %m", dir);
		return (false);
	}
	count = fdscandir(dfd, &names, is_unit_entry, alphasort);
	ok = count >= 0;
	if (!ok)
		prowl_log(LOG_ERR, "read drop-in directory %s: %m", dir);
	for (i = 0; i < count; i++) {
		if (ok)
			ok = add_dropin(root, dfd, dir, names[i]->d_name, inst);
		free(names[i]);
	}
	if (count >= 0)
		free(names);
	close(dfd);
	return (ok);
}

static bool
add_dropins(ucl_object_t *root, const char *path, const char *label,
    const char *inst)
{
	char parent[PROWL_PATH_MAX], template[NAME_MAX + 1], *slash;
	const char *dirs[2], *base;
	size_t len;
	int i, n = 1;

	if (strlcpy(parent, path, sizeof(parent)) >= sizeof(parent))
		return (false);
	slash = strrchr(parent, '/');
	if (slash == NULL)
		return (false);
	base = slash + 1;
	len = strlen(base) - strlen(".unit");
	if (len >= sizeof(template))
		return (false);
	memcpy(template, base, len);
	template[len] = '\0';
	*slash = '\0';
	dirs[0] = parent;
	if (admin_dropins && strcmp(parent, UNIT_DIR_OVERRIDE) != 0)
		dirs[n++] = UNIT_DIR_OVERRIDE;
	for (i = 0; i < n; i++) {
		if (inst != NULL && !add_dropin_dir(root, dirs[i], template, inst))
			return (false);
		if (!add_dropin_dir(root, dirs[i], label, inst))
			return (false);
	}
	return (true);
}

/*
 * An active process retains its stop policy. Refresh the definition at the
 * next rescan after it exits. Keep this job's address stable for events and
 * queued IPC requests in all states.
 */
static bool
install_unit(job_t *job, ucl_object_t *root, job_t *existing, const char *inst)
{
	job_t *before;

	job->unit_config = ucl_object_emit(root, UCL_EMIT_JSON_COMPACT);
	if (inst != NULL)
		job->template_instance = strdup(inst);
	if (job->unit_config == NULL ||
	    (inst != NULL && job->template_instance == NULL)) {
		job_free(job);
		return (false);
	}
	if (existing == NULL) {
		TAILQ_FOREACH(before, &g_jobs, entries) {
			if (before->type == JOB_TYPE_RCSHIM)
				break;
		}
		if (before == NULL)
			TAILQ_INSERT_TAIL(&g_jobs, job, entries);
		else
			TAILQ_INSERT_BEFORE(before, job, entries);
	} else if (existing->unit_config != NULL &&
	    strcmp((const char *)existing->unit_config,
	    (const char *)job->unit_config) == 0) {
		existing->policy_blocked = false;
		existing->reload_on_exit = false;
		job_free(job);
	} else if (existing->pid > 0 || existing->start_pending ||
	    existing->mdns_pid > 0 || existing->state == JOB_STATE_STOPPING ||
	    (existing->state == JOB_STATE_COMPLETED &&
	    (existing->stop_argc > 0 || existing->type == JOB_TYPE_RCSHIM))) {
		existing->policy_blocked = true;
		existing->reload_on_exit = true;
		job_free(job);
	} else
		supervisor_replace_unit(existing, job);
	return (true);
}

/*
 * Get the identity from the base before adding any overrides. Reserve it
 * even if the service schema or a fragment is rejected. The parser owns
 * the merged object and can replace its fields as fragments are read.
 */
static int
unit_load_file_inst(int fd, const char *path, const char *inst, bool shadowed)
{
	struct ucl_parser *parser;
	ucl_object_t *root, *base_rc, *base_provides;
	const ucl_object_t *obj;
	const struct unit_template *source;
	char label[PROWL_LABEL_MAX];
	job_t *job, *existing;
	int ret = -1;

	parser = ucl_parser_new(UCL_PARSER_NO_FILEVARS);
	if (parser == NULL) {
		scan_incomplete = true;
		return (-1);
	}
	if (!add_unit_fd(parser, fd, path, inst, UCL_DUPLICATE_APPEND) ||
	    (root = ucl_parser_get_object(parser)) == NULL) {
		prowl_log(LOG_ERR, "cannot parse native unit %s", path);
		goto done;
	}
	obj = ucl_object_lookup(root, "label");
	if (ucl_object_type(root) != UCL_OBJECT || obj == NULL ||
	    ucl_object_type(obj) != UCL_STRING ||
	    !label_valid(ucl_object_tostring(obj))) {
		prowl_log(LOG_ERR, "missing or invalid unit label in %s", path);
		goto release;
	}
	strlcpy(label, ucl_object_tostring(obj), sizeof(label));
	/* A filename alias cannot instantiate a lower template indirectly. */
	source = inst != NULL ? template_for(label, inst) : NULL;
	if (!shadowed && source != NULL && strcmp(path, source->path) != 0)
		goto release;
	existing = job_find_by_label(label);
	if ((claims_for(label) & CLAIM_SELECTED) != 0 ||
	    (!loading_all && existing != NULL) ||
	    (existing != NULL && existing->unit_config == NULL &&
	    existing->type != JOB_TYPE_RCSHIM)) {
		ret = 0;
		goto release;
	}
	base_rc = ucl_object_ref(ucl_object_lookup(root, "rc_name"));
	base_provides = ucl_object_ref(ucl_object_lookup(root, "provides"));
	job = NULL;
	if (!shadowed && (claims_for(label) & CLAIM_LABEL) == 0 &&
	    add_dropins(root, path, label, inst)) {
		obj = ucl_object_lookup(root, "label");
		if (obj != NULL && ucl_object_type(obj) == UCL_STRING &&
		    strcmp(ucl_object_tostring(obj), label) == 0)
			job = unit_parse_obj(root, path);
		else
			prowl_log(LOG_ERR, "drop-in changed label of %s", path);
	}
	if (job == NULL || !install_unit(job, root, existing, inst)) {
		reserve_identity(label, base_rc, base_provides);
		reserve_identity(label, ucl_object_lookup(root, "rc_name"),
		    ucl_object_lookup(root, "provides"));
	} else {
		claim_name(label, CLAIM_SELECTED);
		prowl_log(LOG_DEBUG, "loaded unit %s from %s", label, path);
		ret = 0;
	}
	ucl_object_unref(base_rc);
	ucl_object_unref(base_provides);
release:
	ucl_object_unref(root);
done:
	ucl_parser_free(parser);
	return (ret);
}

static bool
is_template_name(const char *name)
{
	size_t len = strlen(name);

	return (len > 6 && strcmp(name + len - 6, "@.unit") == 0);
}

/* Apply the same substitution used when parsing the full template. */
static bool
template_label(const char *pattern, const char *inst, char *label, size_t size)
{
	size_t len = strlen(inst), n = 0;

	while (*pattern != '\0') {
		if (pattern[0] == '%' && pattern[1] == 'i') {
			if (len >= size - n)
				return (false);
			memcpy(label + n, inst, len);
			n += len;
			pattern += 2;
		} else {
			if (n + 1 >= size)
				return (false);
			if (pattern[0] == '%' && pattern[1] == '%')
				pattern++;
			label[n++] = *pattern++;
		}
	}
	label[n] = '\0';
	return (label_valid(label));
}

/*
 * Instantiate the template at path for the instances named by its
 * "instances_var" rc.conf variable, or for "default_instance" if the
 * variable is empty or unset.
 */
static int
unit_load_template(int fd, const char *path, bool shadowed)
{
	struct ucl_parser *parser;
	ucl_object_t *root;
	const ucl_object_t *obj;
	char list[1024], *p, *tok;
	struct unit_template *source;
	const char *def = NULL, *pattern;
	char label[PROWL_LABEL_MAX];
	int n = 0;
	bool ok;

	parser = ucl_parser_new(UCL_PARSER_NO_FILEVARS);
	if (parser == NULL) {
		scan_incomplete = true;
		return (-1);
	}
	ok = add_unit_fd(parser, fd, path, NULL, UCL_DUPLICATE_APPEND);
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

	obj = ucl_object_lookup(root, "label");
	pattern = obj != NULL && ucl_object_type(obj) == UCL_STRING ?
	    ucl_object_tostring(obj) : NULL;
	if (pattern == NULL || !template_label(pattern, "i", label, sizeof(label))) {
		prowl_log(LOG_ERR, "missing or invalid template label in %s", path);
		ucl_object_unref(root);
		return (-1);
	}
	STAILQ_FOREACH(source, &templates, link) {
		if (strcmp(source->pattern, pattern) == 0) {
			ucl_object_unref(root);
			return (0);
		}
	}
	source = calloc(1, sizeof(*source));
	if (source == NULL) {
		scan_incomplete = true;
		ucl_object_unref(root);
		return (-1);
	}
	strlcpy(source->pattern, pattern, sizeof(source->pattern));
	if (!shadowed)
		strlcpy(source->path, path, sizeof(source->path));
	STAILQ_INSERT_TAIL(&templates, source, link);

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
		if (unit_load_file_inst(fd, path, tok, shadowed) == 0)
			n++;
	}
	if (n == 0 && list[0] == '\0' && def != NULL && instance_valid(def)) {
		if (unit_load_file_inst(fd, path, def, shadowed) == 0)
			n++;
	}
	ucl_object_unref(root);
	return (0);
}

static void
reject_file(const char *name)
{
	char stem[NAME_MAX + 1], rc_name[PROWL_LABEL_MAX];
	size_t len = strlen(name) - strlen(".unit");

	claim_name(name, CLAIM_FILE);
	if (len >= sizeof(stem))
		return;
	memcpy(stem, name, len);
	stem[len] = '\0';
	if (label_valid(stem)) {
		claim_name(stem, CLAIM_LABEL);
		derive_rc_name(stem, rc_name, sizeof(rc_name));
		claim_name(rc_name, CLAIM_PROVIDER);
	}
}

/*
 * Known labels determine precedence even when filenames differ. When a
 * higher-priority file is unreadable, take identities from a matching
 * lower file solely to suppress fallback, never to launch it.
 */
static int
unit_load_file(const char *path, const char *name)
{
	unsigned claims = claims_for(name);
	int fd, ret;
	bool shadowed = (claims & CLAIM_FILE) != 0;

	fd = trusted_open_file(path, false);
	if (fd == -1) {
		prowl_log(LOG_WARNING, "cannot open native unit %s: %m", path);
		ret = -1;
	} else {
		if (is_template_name(name))
			ret = unit_load_template(fd, path, shadowed);
		else
			ret = unit_load_file_inst(fd, path, NULL, shadowed);
		close(fd);
	}
	if (ret == -1)
		reject_file(name);
	return (ret);
}

/* Highest-priority directory first, for boot and on-demand instances. */
static int
unit_dirs(const char **dirs)
{
	if (g_unit_override[0] != '\0') {
		dirs[0] = g_unit_override;
		return (1);
	}
	dirs[0] = g_generated_dir;
	dirs[1] = UNIT_DIR_OVERRIDE;
	dirs[2] = UNIT_DIR_LOCAL;
	dirs[3] = UNIT_DIR_BASE;
	return (4);
}

/* Resolve full labels first; short names follow directory priority. */
static const struct unit_template *
template_for(const char *requested, const char *instance)
{
	const struct unit_template *source, *short_match = NULL;
	char label[PROWL_LABEL_MAX];
	size_t len, requested_len = strlen(requested);

	STAILQ_FOREACH(source, &templates, link) {
		if (!template_label(source->pattern, instance, label, sizeof(label)))
			continue;
		if (strcmp(label, requested) == 0)
			return (source);
		len = strlen(label);
		if (short_match == NULL && len > requested_len &&
		    label[len - requested_len - 1] == '.' &&
		    strcmp(label + len - requested_len, requested) == 0)
			short_match = source;
	}
	return (short_match);
}

static bool
load_template_instance(const struct unit_template *source, const char *instance)
{
	int fd, ret;

	if (source->path[0] == '\0' || scan_incomplete)
		return (false);
	fd = trusted_open_file(source->path, false);
	if (fd == -1)
		return (false);
	ret = unit_load_file_inst(fd, source->path, instance, false);
	close(fd);
	return (ret == 0);
}

/* Stop at the first matching filename, including a rejected definition. */
static bool
load_named_instance(const char *name, const char *instance)
{
	const char *dirs[4];
	char path[PROWL_PATH_MAX];
	int dfd, error, fd, i, n, ret;

	n = unit_dirs(dirs);
	for (i = 0; i < n && !scan_incomplete; i++) {
		if (snprintf(path, sizeof(path), "%s/%s", dirs[i], name) >=
		    (int)sizeof(path))
			return (false);
		dfd = trusted_open_dir(dirs[i]);
		if (dfd == -1)
			continue;
		fd = trusted_open_file_at(dfd, name, false);
		error = errno;
		close(dfd);
		if (fd == -1) {
			if (error == ENOENT)
				continue;
			return (false);
		}
		ret = unit_load_file_inst(fd, path, instance, false);
		close(fd);
		return (ret == 0);
	}
	return (false);
}

/* Create an instance on demand, including lookup by its short name. */
job_t *
unit_instantiate(const char *label)
{
	char name[NAME_MAX + 1];
	const char *at, *dot;
	const struct unit_template *source;
	job_t *job;

	at = strchr(label, '@');
	if (at == NULL || !label_valid(label) || !instance_valid(at + 1))
		return (NULL);
	source = template_for(label, at + 1);
	if (source != NULL) {
		if (!load_template_instance(source, at + 1))
			return (NULL);
	} else {
		/* A newly created conventional template may predate the next scan. */
		dot = at;
		while (dot > label && dot[-1] != '.')
			dot--;
		if (dot == at || snprintf(name, sizeof(name), "%.*s@.unit",
		    (int)(at - dot), dot) >= (int)sizeof(name) ||
		    !load_named_instance(name, at + 1))
			return (NULL);
	}
	job = job_find_instance(label);
	return (job != NULL ? job : job_find_by_label(label));
}

/*
 * Load all .unit files from a directory.
 * Silently skips non-existent directories.
 */
static void
unit_load_dir(const char *dirpath)
{
	struct dirent **names;
	char path[PROWL_PATH_MAX];
	int fd, count, i, loaded = 0;

	fd = trusted_open_dir(dirpath);
	if (fd == -1) {
		if (errno != ENOENT)
			prowl_log(LOG_WARNING, "ignoring unit directory %s: %m", dirpath);
		return;
	}
	count = fdscandir(fd, &names, is_unit_entry, alphasort);
	close(fd);
	if (count == -1) {
		prowl_log(LOG_WARNING, "unit_load_dir %s: %m", dirpath);
		scan_incomplete = true;
		return;
	}
	for (i = 0; i < count; i++) {
		if (!scan_incomplete && snprintf(path, sizeof(path), "%s/%s",
		    dirpath, names[i]->d_name) < (int)sizeof(path)) {
			if (unit_load_file(path, names[i]->d_name) == 0)
				loaded++;
		}
		free(names[i]);
	}
	free(names);

	if (loaded > 0)
		prowl_log(LOG_NOTICE, "loaded %d unit(s) from %s",
		    loaded, dirpath);
}

void
unit_load_all(void)
{
	const char *dirs[4];
	struct unit_claim *claim;
	struct unit_template *template;
	const struct unit_template *source;
	job_t *job;
	int fd, i, n;

	while ((claim = SLIST_FIRST(&unit_claims)) != NULL) {
		SLIST_REMOVE_HEAD(&unit_claims, link);
		free(claim);
	}
	while ((template = STAILQ_FIRST(&templates)) != NULL) {
		STAILQ_REMOVE_HEAD(&templates, link);
		free(template);
	}
	scan_incomplete = false;
	admin_dropins = false;
	if (g_unit_override[0] == '\0') {
		fd = trusted_open_dir(UNIT_DIR_OVERRIDE);
		if (fd >= 0) {
			admin_dropins = true;
			close(fd);
		}
	}
	loading_all = true;
	n = unit_dirs(dirs);
	for (i = 0; i < n && !scan_incomplete; i++)
		unit_load_dir(dirs[i]);
	/* Lazy instances need the same policy rescan as boot instances. */
	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (job->template_instance == NULL ||
		    (claims_for(job->label) & (CLAIM_SELECTED | CLAIM_LABEL)) != 0)
			continue;
		source = template_for(job->label, job->template_instance);
		if (source != NULL)
			(void)load_template_instance(source, job->template_instance);
	}
	loading_all = false;

	/* Retain the last trusted identity of a now unreadable definition. */
	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (job->unit_config == NULL ||
		    (claims_for(job->label) & CLAIM_SELECTED) != 0)
			continue;
		job->policy_blocked = true;
		job->reload_on_exit = false;
		job->restart_after_reload = false;
		claim_name(job->label, CLAIM_LABEL);
		claim_name(job->rc_name, CLAIM_PROVIDER);
		for (i = 0; i < job->provides_count; i++)
			claim_name(job->provides[i], CLAIM_PROVIDER);
	}

	/* A shim from a previous scan may now be covered by native policy. */
	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (job->type == JOB_TYPE_RCSHIM) {
			job_t *provider = job_find_by_provides(job->rc_name);

			job->policy_blocked = unit_label_reserved(job->label) ||
			    unit_provider_rejected(job->rc_name) ||
			    (provider != NULL && provider->type != JOB_TYPE_RCSHIM);
			for (i = 0; i < job->provides_count; i++)
				if (unit_provider_rejected(job->provides[i]))
					job->policy_blocked = true;
		}
		if (job->policy_blocked && job->sockets_count > 0) {
			socket_close_all(job);
			if (job->pid <= 0 && job->state == JOB_STATE_RUNNING)
				job_set_state(job, JOB_STATE_LOADED);
		}
	}
}
