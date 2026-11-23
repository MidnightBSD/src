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
 * Add an "after X" edge to job by inserting X into job->deps[] as a soft dep
 * (ordering only: a provider that is disabled, masked or failed does not hold
 * the job back), unless an equivalent entry already exists.
 */
static void
dag_add_after_dep(job_t *job, const char *name, bool derived)
{
	int i;

	if (job->deps_count >= PROWL_DEPS_MAX)
		return;

	for (i = 0; i < job->deps_count; i++) {
		if (strcmp(job->deps[i].name, name) == 0)
			return;
	}
	/* A wait that was dropped to break a loop stays dropped. */
	for (i = 0; i < job->dag_ignored_count; i++) {
		if (strcmp(job->dag_ignored[i], name) == 0)
			return;
	}

	strlcpy(job->deps[job->deps_count].name, name, PROWL_LABEL_MAX);
	job->deps[job->deps_count].hard = false;
	job->deps[job->deps_count].derived = derived;
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
			    job->provides[0] : job->rc_name, true);
	}
}

/*
 * The job a dependency entry of job waits for, or NULL when it waits for
 * nothing: no provider, or a condition that does not hold.
 */
static job_t *
dag_dep_target(const job_t *job, const dep_entry_t *de)
{
	job_t *dep;

	if (de->cond[0] != '\0' && !prowl_cond_holds(job->rc_name, de->cond))
		return (NULL);
	dep = job_find_by_provides(de->name);
	if (dep == NULL)
		dep = job_find_by_label(de->name);
	return (dep);
}

/*
 * job waits for target through deps[idx], and target (through a chain of
 * jobs that led the search to job) waits for job: a dependency loop.  Break
 * it the way rcorder(8) does, by ignoring one wait, and keep every job in
 * it.  Prefer a wait that exists only because another job says "before":
 * that is the job that does not fit with the rest.
 */
static void
dag_break_loop(job_t *job, int idx, job_t *target)
{
	char path[512];
	job_t *chain[64], *victim = job, *n;
	int nchain = 0, vidx = idx, i;

	/* A job waits for its dag_parent's successor: walk up to target. */
	for (n = job; n != NULL && nchain < 64; n = n->dag_parent) {
		chain[nchain++] = n;
		if (n == target)
			break;
	}
	/* The back edge wins when it is the odd one out. */
	for (i = 0; i < nchain - 1 && !job->deps[idx].derived; i++) {
		job_t *p = chain[i]->dag_parent;

		if (p->deps[chain[i]->dag_parent_idx].derived) {
			victim = p;
			vidx = chain[i]->dag_parent_idx;
			break;
		}
	}

	/* "target waits for ... waits for job waits for target" */
	path[0] = '\0';
	for (i = nchain - 1; i >= 0; i--) {
		strlcat(path, chain[i]->label, sizeof(path));
		strlcat(path, " -> ", sizeof(path));
	}
	strlcat(path, target->label, sizeof(path));
	prowl_log(LOG_WARNING, "dag: dependency loop %s; ignoring that %s "
	    "waits for %s", path, victim->label, victim->deps[vidx].name);

	if (victim->dag_ignored_count < 8)
		strlcpy(victim->dag_ignored[victim->dag_ignored_count++],
		    victim->deps[vidx].name, PROWL_LABEL_MAX);
	memmove(&victim->deps[vidx], &victim->deps[vidx + 1],
	    (size_t)(victim->deps_count - vidx - 1) * sizeof(dep_entry_t));
	victim->deps_count--;
	memset(&victim->deps[victim->deps_count], 0, sizeof(dep_entry_t));
}

/*
 * Depth-first search for a dependency loop.  Breaks the first loop found
 * and returns true, so that the caller starts over; false if there is none
 * below job.
 */
static bool
dag_dfs(job_t *job, job_t *parent, int parent_idx)
{
	int i;

	job->dag_visited = true;
	job->dag_in_stack = true;
	job->dag_parent = parent;
	job->dag_parent_idx = parent_idx;

	for (i = 0; i < job->deps_count; i++) {
		job_t *dep = dag_dep_target(job, &job->deps[i]);

		if (dep == NULL)
			continue;
		if (dep->dag_in_stack) {
			dag_break_loop(job, i, dep);
			return (true);
		}
		if (dep->dag_visited)
			continue;
		if (dag_dfs(dep, job, i))
			return (true);
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

	/* Resolve explicit "after" fields into hard deps */
	TAILQ_FOREACH(job, &g_jobs, entries) {
		for (int i = 0; i < job->after_count; i++)
			dag_add_after_dep(job, job->after[i], false);
	}

	/* Resolve "before" fields */
	TAILQ_FOREACH(job, &g_jobs, entries)
		dag_resolve_before(job);

	/*
	 * Loops.  Every pass that finds one ignores one wait, so this ends.
	 * The jobs in a loop are not failed: rc(8) scripts from ports are
	 * sometimes written so that they cannot be ordered, and rcorder(8)
	 * starts them all anyway.
	 */
	for (;;) {
		bool found = false;

		TAILQ_FOREACH(job, &g_jobs, entries) {
			job->dag_visited = false;
			job->dag_in_stack = false;
			job->dag_indegree = 0;
			job->dag_parent = NULL;
		}
		TAILQ_FOREACH(job, &g_jobs, entries) {
			if (!job->dag_visited && dag_dfs(job, NULL, 0)) {
				found = true;
				break;
			}
		}
		if (!found)
			break;
		cycles++;
	}

	/* Apply administrative masks, then mark disabled idle jobs. */
	supervisor_load_masks();
	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (!job->masked && !job->enabled &&
		    (job->state == JOB_STATE_LOADED ||
		    job->state == JOB_STATE_STOPPED))
			job_set_state(job, JOB_STATE_DISABLED);
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
 * Start the disabled services that job activates, the way "prowlctl start"
 * would.  Returns true if any was started.  A service that is masked, or an
 * rc-shim, is left alone.
 */
bool
dag_activate(job_t *job)
{
	job_t *p;
	bool any = false;
	int i;

	for (i = 0; i < job->deps_count; i++) {
		if (!job->deps[i].activate)
			continue;
		if (job->deps[i].cond[0] != '\0' &&
		    !prowl_cond_holds(job->rc_name, job->deps[i].cond))
			continue;
		p = job_find_by_provides(job->deps[i].name);
		if (p == NULL)
			p = job_find_by_label(job->deps[i].name);
		if (p == NULL || p == job || p->masked ||
		    p->type == JOB_TYPE_RCSHIM)
			continue;
		/*
		 * A provider that is disabled is enabled; one that is enabled
		 * but not started at boot (run_at_load is false) is started.
		 */
		if (p->state == JOB_STATE_DISABLED) {
			p->enabled = true;
			p->run_at_load = true;
			job_set_state(p, JOB_STATE_LOADED);
		} else if (p->state == JOB_STATE_LOADED && !p->run_at_load) {
			p->run_at_load = true;
		} else {
			continue;
		}
		prowl_log(LOG_NOTICE, "%s activates %s", job->label,
		    p->label);
		any = true;
	}
	return (any);
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
	bool rerun = false;

	/* Nothing new starts while the system is going down. */
	if (g_shutdown || g_rundown)
		return;

	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (g_current_starts >= limit)
			break;

		if (job->masked || job->state != JOB_STATE_LOADED)
			continue;

		if (!job->run_at_load)
			continue;

		/* Waiting out a restart back-off */
		if (job->restart_hold)
			continue;

		if (dag_activate(job))
			rerun = true;

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
			(void)supervisor_start(job);
		}
	}

	/* A service that was activated may be startable now */
	if (rerun)
		dag_schedule_ready();
}
