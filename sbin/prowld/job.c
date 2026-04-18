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
	int i;

	if (job == NULL)
		return;

	for (i = 0; i < job->argc; i++)
		free(job->arguments[i]);

	for (i = 0; i < job->envc; i++)
		free(job->envp[i]);

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

void
job_set_state(job_t *job, job_state_t state)
{
	if (job->state == state)
		return;

	prowl_log(LOG_DEBUG, "job %s: %s -> %s",
	    job->label,
	    job_state_str(job->state),
	    job_state_str(state));

	job->state = state;
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

bool
job_is_masked(const job_t *job)
{
	char path[PROWL_PATH_MAX];
	struct stat sb;

	snprintf(path, sizeof(path), "%s/%s", g_mask_dir, job->label);
	return (lstat(path, &sb) == 0);
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

	(void)job;
	provider = job_find_by_provides(dep->name);
	if (provider == NULL)
		return (!dep->hard); /* soft dep: missing provider is OK */

	return (provider->state == JOB_STATE_RUNNING ||
	    provider->state == JOB_STATE_COMPLETED);
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
		if (!isalnum(c) && c != '.' && c != '-' && c != '_')
			return (false);
	}

	if (strstr(label, "..") != NULL)
		return (false);

	return (true);
}
