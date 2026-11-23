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
 * Job lifecycle management.
 */

#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>

#include "prowld.h"

job_t *
job_alloc(void)
{
	job_t *job;

	job = calloc(1, sizeof(*job));
	if (job == NULL) {
		prowl_log(LOG_ERR, "job_alloc: %m");
		return (NULL);
	}

	job->state = JOB_STATE_LOADED;
	job->throttle_interval = DEFAULT_THROTTLE_INT;
	job->exit_timeout = DEFAULT_EXIT_TIMEOUT;
	job->pid = -1;
	job->notify_fd = -1;
	job->enabled = true;

	for (int i = 0; i < PROWL_SOCKETS_MAX; i++) {
		job->sockets[i].fd  = -1;
		job->sockets[i].job = job;
	}

	return (job);
}

void
job_free(job_t *job)
{
	int i, a;

	if (job == NULL)
		return;

	for (i = 0; i < job->argc; i++)
		free(job->arguments[i]);

	for (i = 0; i < job->stop_argc; i++)
		free(job->stop_arguments[i]);

	for (a = 0; a < PROWL_CMD_LISTS; a++)
		for (i = 0; i < job->cmd_count[a]; i++)
			for (int w = 0; w < job->cmds[a][i].argc; w++)
				free(job->cmds[a][i].argv[w]);

	for (a = 0; a < PROWL_ACTION_MAX; a++)
		for (i = 0; i < job->action_argc[a]; i++)
			free(job->action_args[a][i]);

	for (i = 0; i < job->envc; i++)
		free(job->envp[i]);

	for (i = 0; i < job->mdns.txt_count; i++)
		free(job->mdns.txt_record[i]);

	free(job);
}

job_t *
job_find_by_label(const char *label)
{
	job_t *job;

	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (strcmp(job->label, label) == 0)
			return (job);
	}
	return (NULL);
}

/*
 * Find an instance of a template by its short name: "pflog@pf1" matches
 * the label "org.midnightbsd.pflog@pf1".
 */
job_t *
job_find_instance(const char *name)
{
	job_t *job;
	size_t llen, nlen = strlen(name);

	if (strchr(name, '@') == NULL)
		return (NULL);
	TAILQ_FOREACH(job, &g_jobs, entries) {
		llen = strlen(job->label);
		if (llen > nlen && job->label[llen - nlen - 1] == '.' &&
		    strcmp(job->label + llen - nlen, name) == 0)
			return (job);
	}
	return (NULL);
}

job_t *
job_find_by_rcname(const char *rcname)
{
	job_t *job;

	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (strcmp(job->rc_name, rcname) == 0)
			return (job);
	}
	return (NULL);
}

/*
 * Find any job that provides the named capability.
 * Returns the first match, or NULL.
 */
job_t *
job_find_by_provides(const char *name)
{
	job_t *job;

	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (job_provides(job, name))
			return (job);
	}
	return (NULL);
}

bool
job_provides(const job_t *job, const char *name)
{
	int i;

	/* rc_name counts as an implicit provide */
	if (strcmp(job->rc_name, name) == 0)
		return (true);

	for (i = 0; i < job->provides_count; i++) {
		if (strcmp(job->provides[i], name) == 0)
			return (true);
	}
	return (false);
}

job_state_t
job_reported_state(const job_t *job)
{
	return (job->masked ? JOB_STATE_MASKED : job->state);
}

static void
job_report_state_change(const job_t *job, job_state_t old)
{
	job_state_t state = job_reported_state(job);

	if (old == state)
		return;

	prowl_log(LOG_DEBUG, "job %s: %s -> %s",
	    job->label,
	    job_state_str(old),
	    job_state_str(state));

	ipc_broadcast_event(job->label, job_state_str(old),
	    job_state_str(state));
}

void
job_set_state(job_t *job, job_state_t state)
{
	job_state_t old = job_reported_state(job);

	job->state = state;
	job_report_state_change(job, old);
}

void
job_set_masked(job_t *job, bool masked)
{
	job_state_t old = job_reported_state(job);

	job->masked = masked;
	job_report_state_change(job, old);
}

const char *
job_state_str(job_state_t state)
{
	switch (state) {
	case JOB_STATE_UNKNOWN:		return ("unknown");
	case JOB_STATE_LOADED:		return ("loaded");
	case JOB_STATE_DISABLED:	return ("disabled");
	case JOB_STATE_MASKED:		return ("masked");
	case JOB_STATE_STARTING:	return ("starting");
	case JOB_STATE_RUNNING:		return ("running");
	case JOB_STATE_STOPPING:	return ("stopping");
	case JOB_STATE_STOPPED:		return ("stopped");
	case JOB_STATE_FAILED:		return ("failed");
	case JOB_STATE_COMPLETED:	return ("completed");
	default:			return ("unknown");
	}
}

const char *
job_type_str(job_type_t type)
{
	switch (type) {
	case JOB_TYPE_DAEMON:		return ("daemon");
	case JOB_TYPE_ONESHOT:		return ("oneshot");
	case JOB_TYPE_TIMER:		return ("timer");
	case JOB_TYPE_SOCKET:		return ("socket");
	case JOB_TYPE_RCSHIM:		return ("rcshim");
	default:			return ("unknown");
	}
}

static int
job_mask_path(const job_t *job, char *path, size_t size)
{
	int n;

	n = snprintf(path, size, "%s/%s", g_mask_dir, job->label);
	if (n < 0 || (size_t)n >= size) {
		errno = ENAMETOOLONG;
		return (-1);
	}
	return (0);
}

bool
job_mask_saved(const job_t *job)
{
	char path[PROWL_PATH_MAX];
	struct stat sb;

	if (job_mask_path(job, path, sizeof(path)) == 0) {
		if (lstat(path, &sb) == 0)
			return (true);
		if (errno == ENOENT)
			return (false);
	}
	prowl_log(LOG_WARNING, "cannot read mask for %s: %m", job->label);
	return (job->masked);
}

int
job_save_mask(const job_t *job, bool masked)
{
	char path[PROWL_PATH_MAX];

	if (job_mask_path(job, path, sizeof(path)) == -1)
		return (-1);
	if (masked) {
		if (symlink("/dev/null", path) == -1 && errno != EEXIST)
			return (-1);
	} else if (unlink(path) == -1 && errno != ENOENT) {
		return (-1);
	}
	return (0);
}

/*
 * Check whether a single dependency is satisfied.
 * A dependency on "name" is satisfied if any loaded job that provides
 * "name" is in the RUNNING or COMPLETED state.
 */
bool
job_dep_satisfied(const job_t *job, const dep_entry_t *dep)
{
	job_t *provider;
	job_state_t state;

	/* "?name service": nothing to wait for unless the variable holds */
	if (dep->cond[0] != '\0' && !prowl_cond_holds(job->rc_name, dep->cond))
		return (true);

	provider = job_find_by_provides(dep->name);
	if (provider == NULL) {
		/*
		 * Soft dep: missing provider is OK.  rcorder(8) also ignores
		 * a REQUIRE that nothing provides, so rc-shims follow suit.
		 */
		return (!dep->hard || job->type == JOB_TYPE_RCSHIM);
	}

	state = job_reported_state(provider);
	if (state == JOB_STATE_RUNNING || state == JOB_STATE_COMPLETED)
		return (true);

	/*
	 * rc(8) semantics: a REQUIRE only orders scripts.  A script that is
	 * disabled in rc.conf, masked, or that failed does not hold back the
	 * scripts that require it, whatever kind of job provides it, nor does
	 * one that is crash-looping and waiting out its restart back-off.  The
	 * same goes for a native unit's "wants".  A native unit's "requires"
	 * keeps the strict rule.
	 */
	if ((job->type == JOB_TYPE_RCSHIM || !dep->hard ||
	    (dep->activate && provider->type == JOB_TYPE_RCSHIM)) &&
	    (state == JOB_STATE_DISABLED ||
	    state == JOB_STATE_MASKED ||
	    state == JOB_STATE_FAILED ||
	    provider->restart_hold))
		return (true);

	return (false);
}

/*
 * Return true when a service this job activates is not up yet.
 */
bool
job_activations_pending(const job_t *job)
{
	int i;

	for (i = 0; i < job->deps_count; i++) {
		if (job->deps[i].activate &&
		    !job_dep_satisfied(job, &job->deps[i]))
			return (true);
	}
	return (false);
}

/*
 * Return true when all of this job's dependencies are satisfied.
 */
bool
job_all_deps_satisfied(const job_t *job)
{
	int i;

	for (i = 0; i < job->deps_count; i++) {
		if (!job_dep_satisfied(job, &job->deps[i]))
			return (false);
	}
	return (true);
}

/*
 * Does the string start a condition?  See prowl_cond_holds().
 */
bool
prowl_is_cond(const char *s)
{
	return (s[0] != '\0' && strchr("?!+~<", s[0]) != NULL);
}

/*
 * Does the condition hold?  "?name": the rc.conf variable is YES, TRUE, ON or
 * 1; "!name": it is not; "+name": it is not empty; "~name": it is empty;
 * "<name": it names a readable file; "!<name": it does not.  rc_name selects
 * the rc.conf.d file read after rc.conf.
 */
bool
prowl_cond_holds(const char *rc_name, const char *cond)
{
	char val[256];
	const char *name = cond + 1;
	char kind = cond[0];

	/* "!<name" is the opposite of "<name" */
	if (kind == '!' && name[0] == '<') {
		kind = '>';
		name++;
	}
	val[0] = '\0';
	(void)rcconf_job_var(rc_name, name, val, sizeof(val));
	switch (kind) {
	case '?':
		return (rcconf_value_yes(val));
	case '!':
		return (!rcconf_value_yes(val));
	case '+':
		return (val[0] != '\0');
	case '~':
		return (val[0] == '\0');
	case '<':
		return (val[0] != '\0' && access(val, R_OK) == 0);
	case '>':
		return (val[0] == '\0' || access(val, R_OK) != 0);
	}
	return (true);
}

/*
 * A start_pre, start_post or stop_post command may begin with conditions,
 * see prowl_cond_holds().
 * Returns the index of the program, after the conditions.
 */
int
prowl_cmd_first(const prowl_cmd_t *c)
{
	int i;

	for (i = 0; i < c->argc; i++) {
		if (!prowl_is_cond(c->argv[i]))
			break;
	}
	return (i);
}

/*
 * Validate a job label or rc_name against the allowed character set:
 * alphanumerics, dots, hyphens, underscores; no path separators, NUL
 * bytes, leading dots, or ".." components.
 *
 * Used at load time to prevent filesystem path abuse via crafted labels.
 */
bool
label_valid(const char *label)
{
	const char *p;
	size_t len;

	if (label == NULL || *label == '\0')
		return (false);

	len = strlen(label);
	if (len >= PROWL_LABEL_MAX)
		return (false);

	if (label[0] == '.')
		return (false);

	for (p = label; *p != '\0'; p++) {
		unsigned char c = (unsigned char)*p;
		if (!isalnum(c) && c != '.' && c != '-' && c != '_' &&
		    c != '@')
			return (false);
	}

	if (strstr(label, "..") != NULL)
		return (false);

	return (true);
}
