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
 * Dependency graph construction, cycle detection, and parallel scheduler.
 *
 * The scheduler is event-driven: dag_schedule_ready() is called after every
 * state transition to promote newly-eligible jobs from LOADED to STARTING.
 * Jobs become eligible when all their hard (requires) and soft (wants)
 * dependencies are in the RUNNING or COMPLETED state.
 *
 * "after" edges derived from requires/wants/after fields all collapse into
 * a single dependency check via job_all_deps_satisfied().
 */

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "prowld.h"

/*
 * Add an "after X" edge to job by inserting X into job->deps[] as a hard dep,
 * unless an equivalent entry already exists.
 */
static void
dag_add_after_dep(job_t *job, const char *name)
{
	int i;

	if (job->deps_count >= PROWL_DEPS_MAX)
		return;

	for (i = 0; i < job->deps_count; i++) {
		if (strcmp(job->deps[i].name, name) == 0)
			return;
	}

	strlcpy(job->deps[job->deps_count].name, name, PROWL_LABEL_MAX);
	job->deps[job->deps_count].hard = true;
	job->deps_count++;
}

/*
 * For each "before: X" on this job, add a "after: this_job" to job X.
 */
static void
dag_resolve_before(job_t *job)
{
	int i;
	job_t *other;

	for (i = 0; i < job->before_count; i++) {
		other = job_find_by_label(job->before[i]);
		if (other == NULL)
			other = job_find_by_provides(job->before[i]);
		if (other != NULL)
			dag_add_after_dep(other, job->provides[0][0] != '\0' ?
			    job->provides[0] : job->rc_name);
	}
}

/*
 * Cycle detection via DFS.  Returns true if a cycle was found.
 * Sets dag_visited and dag_in_stack on visited nodes.
 */
static bool
dag_dfs(job_t *job)
{
	int i;

	if (job->dag_in_stack)
		return (true);
	if (job->dag_visited)
		return (false);

	job->dag_visited = true;
	job->dag_in_stack = true;

	for (i = 0; i < job->deps_count; i++) {
		job_t *dep = job_find_by_provides(job->deps[i].name);
		if (dep == NULL)
			dep = job_find_by_label(job->deps[i].name);
		if (dep == NULL)
			continue;
		if (dag_dfs(dep)) {
			prowl_log(LOG_ERR, "dag: cycle detected involving "
			    "%s -> %s", job->label, dep->label);
			return (true);
		}
	}

	job->dag_in_stack = false;
	return (false);
}

/*
 * Build the dependency graph from all loaded jobs:
 * 1. Resolve explicit "after" fields into deps entries.
 * 2. Resolve "before" fields by adding reverse edges.
 * 3. Detect cycles.
 * 4. Mark disabled/masked jobs.
 */
int
dag_build(void)
{
	job_t *job;
	int cycles = 0;

	/* Reset DAG state */
	TAILQ_FOREACH(job, &g_jobs, entries) {
		job->dag_visited = false;
		job->dag_in_stack = false;
		job->dag_indegree = 0;
	}

	/* Resolve explicit "after" fields into hard deps */
	TAILQ_FOREACH(job, &g_jobs, entries) {
		for (int i = 0; i < job->after_count; i++)
			dag_add_after_dep(job, job->after[i]);
	}

	/* Resolve "before" fields */
	TAILQ_FOREACH(job, &g_jobs, entries)
		dag_resolve_before(job);

	/* Cycle detection */
	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (!job->dag_visited) {
			if (dag_dfs(job)) {
				/*
				 * Mark the involved job as failed so it is
				 * excluded from scheduling.  Boot continues.
				 */
				job_set_state(job, JOB_STATE_FAILED);
				cycles++;
			}
		}
	}

	if (cycles > 0)
		prowl_log(LOG_ERR, "dag: %d cycle(s) detected; affected "
		    "services will not start", cycles);

	/* Apply disabled/masked states */
	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (job->state == JOB_STATE_FAILED)
			continue;
		if (job_is_masked(job)) {
			job_set_state(job, JOB_STATE_MASKED);
			continue;
		}
		if (!job->enabled) {
			job_set_state(job, JOB_STATE_DISABLED);
			continue;
		}
	}

	return (cycles == 0 ? 0 : -1);
}

/*
 * Determine the effective concurrency limit.
 */
static int
max_concurrent(void)
{
	if (g_config.max_concurrent_starts > 0)
		return (g_config.max_concurrent_starts);

	/* Default: ncpu * 2, with a floor of 4 */
	long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
	if (ncpu < 1)
		ncpu = 1;
	int limit = (int)(ncpu * 2);
	return (limit < 4 ? 4 : limit);
}

/*
 * Scan the job list and start any job that:
 *  - is in LOADED state
 *  - has run_at_load set
 *  - has all deps satisfied
 *  - does not push us over the concurrency limit
 *
 * Called after every state transition event.
 */
void
dag_schedule_ready(void)
{
	job_t *job;
	int limit = max_concurrent();

	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (g_current_starts >= limit)
			break;

		if (job->state != JOB_STATE_LOADED)
			continue;

		if (!job->run_at_load)
			continue;

		if (!job_all_deps_satisfied(job))
			continue;

		/* Check conflicts */
		bool conflicted = false;
		for (int i = 0; i < job->conflicts_count; i++) {
			job_t *c = job_find_by_provides(job->conflicts[i]);
			if (c == NULL)
				c = job_find_by_label(job->conflicts[i]);
			if (c != NULL && c->state == JOB_STATE_RUNNING) {
				conflicted = true;
				break;
			}
		}
		if (conflicted)
			continue;

		if (job->sockets_count > 0) {
			/*
			 * Socket-activated job: bind and listen now; the daemon
			 * is launched only on the first incoming connection.
			 * Counts as "running" from the scheduler's perspective
			 * so dependent jobs are not blocked.
			 */
			if (socket_bind_all(job) == 0) {
				job_set_state(job, JOB_STATE_RUNNING);
				dag_schedule_ready();
			} else {
				job_set_state(job, JOB_STATE_FAILED);
			}
		} else {
			if (supervisor_start(job) == 0)
				g_current_starts++;
		}
	}
}
