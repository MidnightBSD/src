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
#include <fcntl.h>
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
	job->launch_fd = -1;
	job->enabled = true;

	return (job);
}

/*
 * Replace the heap string *dst with at most max - 1 bytes of src, the way
 * strlcpy() into a char[max] would.  An empty or NULL src leaves *dst
 * NULL, so a set string is never "".  Returns -1 if memory runs out.
 */
int
job_set_str(char **dst, const char *src, size_t max)
{
	char *s = NULL;

	if (src != NULL && src[0] != '\0' && max > 1) {
		s = strndup(src, max - 1);
		if (s == NULL) {
			prowl_log(LOG_ERR, "job_set_str: %m");
			return (-1);
		}
	}
	free(*dst);
	*dst = s;
	return (0);
}

/*
 * Grow an array of count elements of size elsz by one zeroed element.
 * Returns the new array, or NULL (arr left as it was) if memory runs out.
 */
void *
prowl_array_add(void *arr, int count, size_t elsz)
{
	char *n;

	if (count < 0)
		return (NULL);
	n = reallocarray(arr, (size_t)count + 1, elsz);
	if (n == NULL) {
		prowl_log(LOG_ERR, "prowl_array_add: %m");
		return (NULL);
	}
	memset(n + (size_t)count * elsz, 0, elsz);
	return (n);
}

static void
job_clear(job_t *job)
{
	int i, a;

	if (job == NULL)
		return;

	for (i = 0; i < job->argc; i++)
		free(job->arguments[i]);

	for (i = 0; i < job->stop_argc; i++)
		free(job->stop_arguments[i]);

	for (a = 0; a < PROWL_CMD_LISTS; a++) {
		for (i = 0; i < job->cmd_count[a]; i++)
			for (int w = 0; w < job->cmds[a][i].argc; w++)
				free(job->cmds[a][i].argv[w]);
		free(job->cmds[a]);
	}

	for (a = 0; a < PROWL_ACTION_MAX; a++) {
		for (i = 0; i < job->action_argc[a]; i++)
			free(job->action_args[a][i]);
		free(job->action_args[a]);
	}

	for (i = 0; i < job->envc; i++)
		free(job->envp[i]);

	if (job->mdns != NULL) {
		for (i = 0; i < job->mdns->txt_count; i++)
			free(job->mdns->txt_record[i]);
		free(job->mdns);
	}

	free(job->description);
	free(job->program);
	free(job->working_directory);
	free(job->root_directory);
	free(job->pid_file);
	free(job->stdin_path);
	free(job->stdout_path);
	free(job->stderr_path);
	free(job->rcshim_path);
	free(job->shim_pidfile);
	free(job->deps);
	free(job->before);
	free(job->after);
	free(job->conflicts);
	free(job->provides);
	free(job->required_files);
	free(job->dag_ignored);
	free(job->sockets);

	if (job->launch_fd >= 0)
		close(job->launch_fd);
	free(job->unit_config);
	free(job->template_instance);
}

void
job_free(job_t *job)
{
	job_clear(job);
	free(job);
}

/* Keep every kqueue and IPC reference to this job valid across a rescan. */
void
job_replace(job_t *old, job_t *replacement)
{
	replacement->entries = old->entries;
	/* A reload should not start an explicitly stopped or completed job. */
	if (old->state == JOB_STATE_STOPPED || old->state == JOB_STATE_COMPLETED)
		replacement->state = old->state;
	if (old->restart_after_reload) {
		replacement->state = JOB_STATE_LOADED;
		replacement->run_at_load = true;
		replacement->enabled = true;
	}
	replacement->masked = old->masked;
	replacement->restart_count = old->restart_count;
	replacement->last_exit_status = old->last_exit_status;
	replacement->last_run = old->last_run;
	replacement->started_at = old->started_at;
	replacement->exited_at = old->exited_at;
	replacement->stop_timer_generation = old->stop_timer_generation;
	job_clear(old);
	*old = *replacement;
	for (int i = 0; i < old->sockets_count; i++)
		old->sockets[i].job = old;
	free(replacement);
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

bool
job_launch_blocked(const job_t *job)
{
	return (job->masked || job->policy_blocked);
}

job_state_t
job_reported_state(const job_t *job)
{
	if (job->masked)
		return (JOB_STATE_MASKED);
	if (job->policy_blocked && job->pid <= 0)
		return (JOB_STATE_FAILED);
	return (job->state);
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
update_mask(const job_t *job, const char *dir, bool masked)
{
	struct stat sb;
	int fd, rc, error;

	fd = trusted_open_dir(dir);
	if (fd == -1)
		return (!masked && errno == ENOENT ? 0 : -1);

	if (masked) {
		rc = symlinkat("/dev/null", fd, job->label);
		if (rc == -1 && errno == EEXIST)
			rc = 0;
	} else {
		/* An absent marker needs no write on a read-only file system. */
		rc = fstatat(fd, job->label, &sb, AT_SYMLINK_NOFOLLOW);
		if (rc == 0)
			rc = unlinkat(fd, job->label, 0);
		if (rc == -1 && errno == ENOENT)
			rc = 0;
	}
	error = errno;
	close(fd);
	errno = error;
	return (rc);
}

int
job_save_mask(const job_t *job, bool masked)
{
	/* Keep the early-boot marker until any legacy copy is gone. */
	if (!masked && g_legacy_mask_dir != NULL &&
	    update_mask(job, g_legacy_mask_dir, false) == -1)
		return (-1);
	return (update_mask(job, g_mask_dir, masked));
}

/*
 * Is the dependency satisfied by this one provider? It is when the provider
 * is RUNNING or COMPLETED, or when its state does not hold the job back.
 */
static bool
dep_provider_satisfied(const job_t *job, const dep_entry_t *dep,
    const job_t *provider)
{
	job_state_t state;

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
 * Check whether a single dependency is satisfied.  Every job that provides
 * the name must be done: scripts such as ccd, gbde and geli all provide
 * "disks", and rcorder(8) runs the dependents after all of them.
 */
bool
job_dep_satisfied(const job_t *job, const dep_entry_t *dep)
{
	const job_t *provider;
	bool found = false;

	/* "?name service": nothing to wait for unless the variable holds */
	if (dep->cond[0] != '\0' && !prowl_cond_holds(job->rc_name, dep->cond))
		return (true);

	TAILQ_FOREACH(provider, &g_jobs, entries) {
		if (!job_provides(provider, dep->name))
			continue;
		found = true;
		if (!dep_provider_satisfied(job, dep, provider))
			return (false);
	}
	if (!found) {
		/*
		 * Soft dep: missing provider is OK.  rcorder(8) also ignores
		 * a REQUIRE that nothing provides, so rc-shims follow suit.
		 */
		return (!dep->hard || job->type == JOB_TYPE_RCSHIM);
	}
	return (true);
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
