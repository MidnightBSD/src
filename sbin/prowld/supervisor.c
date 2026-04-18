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
 * Process supervision: fork/exec, privilege drop, I/O setup,
 * keep-alive restart, stop with timeout, and orderly shutdown.
 */

#include <sys/event.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "prowld.h"

/*
 * Register a EVFILT_PROC watcher for a child PID, associating the job
 * pointer as udata so the event loop can identify which job exited.
 */
static void
watch_child(pid_t pid, job_t *job)
{
	struct kevent kev;

	EV_SET(&kev, (uintptr_t)pid, EVFILT_PROC, EV_ADD | EV_ONESHOT,
	    NOTE_EXIT, 0, (void *)job);
	if (kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1)
		prowl_log(LOG_WARNING, "kevent EVFILT_PROC %d: %m", (int)pid);
}

/*
 * Register a one-shot timer to fire after throttle_interval seconds.
 * Low bit of ident is 0: throttle timer.
 */
static void
arm_throttle_timer(job_t *job)
{
	struct kevent kev;
	int64_t ms;

	ms = (int64_t)job->throttle_interval * 1000;
	if (ms <= 0)
		ms = DEFAULT_THROTTLE_INT * 1000;

	EV_SET(&kev, (uintptr_t)job, EVFILT_TIMER, EV_ADD | EV_ONESHOT,
	    NOTE_MSECONDS, ms, (void *)job);
	if (kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1)
		prowl_log(LOG_WARNING, "kevent throttle timer %s: %m",
		    job->label);
	job->throttle_timer_active = true;
}

/*
 * Register a one-shot timer for the stop (exit_timeout) deadline.
 * Low bit of ident is 1: stop-timeout timer.
 */
static void
arm_stop_timer(job_t *job)
{
	struct kevent kev;
	int64_t ms;
	uintptr_t ident;

	ms = (int64_t)job->exit_timeout * 1000;
	if (ms <= 0)
		ms = DEFAULT_EXIT_TIMEOUT * 1000;

	ident = (uintptr_t)job | 1UL;
	EV_SET(&kev, ident, EVFILT_TIMER, EV_ADD | EV_ONESHOT,
	    NOTE_MSECONDS, ms, (void *)job);
	if (kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1)
		prowl_log(LOG_WARNING, "kevent stop timer %s: %m",
		    job->label);
	job->stop_timer_active = true;
}

/*
 * Redirect a standard file descriptor to a path.
 * Called in the child after fork(), before the new program is loaded.
 * Must be async-signal-safe: no malloc, no printf.
 */
static void
redirect_fd(int stdfd, const char *path, int flags)
{
	int fd;

	if (path == NULL || path[0] == '\0')
		return;

	fd = open(path, flags, 0644);
	if (fd == -1)
		return;

	if (fd != stdfd) {
		dup2(fd, stdfd);
		close(fd);
	}
}

/*
 * Resolve user/group names to numeric IDs in the parent before fork().
 * getpwnam/getgrnam/getgrouplist are not async-signal-safe; calling them
 * here ensures the child only uses the safe setgroups/setgid/setuid calls.
 */
static void
resolve_job_privileges(job_t *job)
{
	struct passwd *pw;
	struct group *gr;
	int ngroups;

	job->run_priv_set = false;
	job->run_uid = (uid_t)-1;
	job->run_gid = (gid_t)-1;
	job->run_ngroups = 0;

	if (job->user[0] == '\0' && job->group[0] == '\0')
		return;

	if (job->group[0] != '\0') {
		gr = getgrnam(job->group);
		if (gr == NULL) {
			prowl_log(LOG_WARNING, "job %s: group '%s' not found",
			    job->label, job->group);
			return;
		}
		job->run_gid = gr->gr_gid;
	}

	if (job->user[0] != '\0') {
		pw = getpwnam(job->user);
		if (pw == NULL) {
			prowl_log(LOG_WARNING, "job %s: user '%s' not found",
			    job->label, job->user);
			return;
		}
		job->run_uid = pw->pw_uid;
		if (job->group[0] == '\0')
			job->run_gid = pw->pw_gid;

		ngroups = PROWL_GROUPS_MAX;
		getgrouplist(pw->pw_name, job->run_gid,
		    job->run_groups, &ngroups);
		if (ngroups < 1)
			ngroups = 1;
		job->run_ngroups = ngroups;
	}

	job->run_priv_set = true;
}

/*
 * Drop privileges in the child process.  Called post-fork, pre-exec.
 * Uses only async-signal-safe syscalls: setgroups, setgid, setuid.
 * Numeric IDs were resolved in the parent by resolve_job_privileges().
 * Exits the child immediately if any privilege call fails to ensure
 * the process never continues with unexpected (elevated) privileges.
 */
static void
drop_privileges(const job_t *job)
{
	if (!job->run_priv_set)
		return;

	if (job->run_ngroups > 0) {
		if (setgroups((size_t)job->run_ngroups,
		    job->run_groups) == -1) {
			syslog(LOG_ERR, "prowld: setgroups failed: %m");
			_exit(1);
		}
	}
	if (job->run_gid != (gid_t)-1) {
		if (setgid(job->run_gid) == -1) {
			syslog(LOG_ERR, "prowld: setgid %d failed: %m",
			    (int)job->run_gid);
			_exit(1);
		}
	}
	if (job->run_uid != (uid_t)-1) {
		if (setuid(job->run_uid) == -1) {
			syslog(LOG_ERR, "prowld: setuid %d failed: %m",
			    (int)job->run_uid);
			_exit(1);
		}
	}
}

/*
 * Child-side setup after fork().  Configures the execution environment
 * and replaces the process image via execv/execve.
 * Must not return; calls _exit on failure.
 */
static void
child_setup_and_exec(job_t *job)
{
	char *argv[PROWL_ARGS_MAX + 2];
	char *envp[PROWL_ENV_MAX + 1];
	int i, argc;

	/* Change working directory */
	if (job->working_directory[0] != '\0') {
		if (chdir(job->working_directory) == -1)
			_exit(1);
	}

	drop_privileges(job);

	if (job->umask_set)
		umask(job->umask_val);

	if (job->nice_val != 0)
		setpriority(PRIO_PROCESS, 0, job->nice_val);

	/* Resource limits */
	if (job->rlimits.set_nofile) {
		struct rlimit rl = {
			job->rlimits.nofile,
			job->rlimits.nofile
		};
		setrlimit(RLIMIT_NOFILE, &rl);
	}
	if (job->rlimits.set_nproc) {
		struct rlimit rl = {
			job->rlimits.nproc,
			job->rlimits.nproc
		};
		setrlimit(RLIMIT_NPROC, &rl);
	}

	/* Standard I/O */
	if (job->stdin_path[0] != '\0')
		redirect_fd(STDIN_FILENO, job->stdin_path, O_RDONLY);
	else
		redirect_fd(STDIN_FILENO, "/dev/null", O_RDONLY);

	if (job->stdout_path[0] != '\0')
		redirect_fd(STDOUT_FILENO, job->stdout_path,
		    O_WRONLY | O_CREAT | O_APPEND);

	if (job->stderr_path[0] != '\0')
		redirect_fd(STDERR_FILENO, job->stderr_path,
		    O_WRONLY | O_CREAT | O_APPEND);

	if (job->type == JOB_TYPE_RCSHIM) {
		/* rc-shim invocation: script start */
		argv[0] = job->rcshim_path;
		argv[1] = (char *)(uintptr_t)"start";
		argv[2] = NULL;
		execv(argv[0], argv);
		_exit(127);
	}

	/* Native unit */
	argv[0] = job->program;
	argc = 1;
	for (i = 0; i < job->argc && argc < PROWL_ARGS_MAX; i++)
		argv[argc++] = job->arguments[i];
	argv[argc] = NULL;

	if (job->envc > 0) {
		for (i = 0; i < job->envc && i < PROWL_ENV_MAX; i++)
			envp[i] = job->envp[i];
		envp[i] = NULL;
		execve(job->program, argv, envp);
	} else {
		execv(job->program, argv);
	}

	_exit(127);
}

/*
 * Fork and start a job.  Returns 0 on success, -1 on fork failure.
 */
int
supervisor_start(job_t *job)
{
	pid_t pid;

	if (job->state == JOB_STATE_RUNNING ||
	    job->state == JOB_STATE_STARTING)
		return (0);

	prowl_log(LOG_NOTICE, "starting %s", job->label);
	job->started_at = time(NULL);
	job_set_state(job, JOB_STATE_STARTING);

	resolve_job_privileges(job);

	pid = fork();
	if (pid == -1) {
		prowl_log(LOG_ERR, "fork %s: %m", job->label);
		job_set_state(job, JOB_STATE_FAILED);
		if (g_current_starts > 0)
			g_current_starts--;
		return (-1);
	}

	if (pid == 0) {
		/* Child: close kqueue fd before exec */
		if (g_kqueue_fd >= 0)
			close(g_kqueue_fd);
		child_setup_and_exec(job);
		/* NOTREACHED */
	}

	/* Parent */
	job->pid = pid;
	watch_child(pid, job);
	arm_throttle_timer(job);
	prowl_log(LOG_INFO, "job %s started, pid %d", job->label, (int)pid);
	return (0);
}

/*
 * Stop a job.  Sends SIGTERM and arms the stop-timeout timer.
 * If force is true, sends SIGKILL directly.
 */
int
supervisor_stop(job_t *job, bool force)
{
	if (job->pid <= 0)
		return (0);

	if (job->state == JOB_STATE_STOPPED ||
	    job->state == JOB_STATE_FAILED ||
	    job->state == JOB_STATE_STOPPING)
		return (0);

	prowl_log(LOG_NOTICE, "%s %s (pid %d)",
	    force ? "force-stopping" : "stopping",
	    job->label, (int)job->pid);

	job_set_state(job, JOB_STATE_STOPPING);

	if (force) {
		kill(job->pid, SIGKILL);
	} else {
		kill(job->pid, SIGTERM);
		arm_stop_timer(job);
	}

	return (0);
}

int
supervisor_signal(job_t *job, int sig)
{
	if (job->pid <= 0)
		return (-1);
	return (kill(job->pid, sig));
}

/*
 * Called when the throttle timer fires.  If the process is still alive,
 * transition it to RUNNING.
 */
void
supervisor_handle_throttle(job_t *job)
{
	job->throttle_timer_active = false;

	if (job->state != JOB_STATE_STARTING)
		return;

	if (job->pid > 0 && kill(job->pid, 0) == 0) {
		job_set_state(job, JOB_STATE_RUNNING);
		if (g_current_starts > 0)
			g_current_starts--;
		prowl_log(LOG_INFO, "job %s running (pid %d)",
		    job->label, (int)job->pid);
		dag_schedule_ready();
	}
}

/*
 * Called when the stop-timeout timer fires.  Force-kill the process.
 */
void
supervisor_handle_stop_timeout(job_t *job)
{
	job->stop_timer_active = false;

	if (job->state != JOB_STATE_STOPPING)
		return;

	if (job->pid > 0) {
		prowl_log(LOG_WARNING,
		    "job %s (pid %d) timed out; sending SIGKILL",
		    job->label, (int)job->pid);
		kill(job->pid, SIGKILL);
	}
}

/*
 * Determine restart policy from keep_alive settings and exit status.
 */
static bool
should_restart(const job_t *job, int status)
{
	if (job->keep_alive.always)
		return (true);

	if (WIFEXITED(status)) {
		int code = WEXITSTATUS(status);
		if (code == 0)
			return (job->keep_alive.successful_exit);
		return (job->keep_alive.crashed);
	}

	/* Killed by signal */
	return (job->keep_alive.crashed);
}

/*
 * Reap a child process and update job state.
 */
void
supervisor_reap(pid_t pid, int status)
{
	job_t *job;
	struct kevent kev;

	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (job->pid == pid)
			break;
	}

	if (job == NULL) {
		prowl_log(LOG_DEBUG, "reap: unknown pid %d", (int)pid);
		return;
	}

	job->last_exit_status = status;
	job->exited_at = time(NULL);
	job->pid = -1;

	/* Cancel throttle timer if still active */
	if (job->throttle_timer_active) {
		EV_SET(&kev, (uintptr_t)job, EVFILT_TIMER, EV_DELETE,
		    0, 0, NULL);
		kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL);
		job->throttle_timer_active = false;
		if (g_current_starts > 0)
			g_current_starts--;
	}

	if (WIFEXITED(status))
		prowl_log(LOG_NOTICE, "job %s exited with code %d",
		    job->label, WEXITSTATUS(status));
	else if (WIFSIGNALED(status))
		prowl_log(LOG_NOTICE, "job %s killed by signal %d",
		    job->label, WTERMSIG(status));

	/* Oneshot completes on clean exit */
	if (job->type == JOB_TYPE_ONESHOT &&
	    WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		job_set_state(job, JOB_STATE_COMPLETED);
		dag_schedule_ready();
		return;
	}

	/* Normal stop requested */
	if (job->state == JOB_STATE_STOPPING) {
		job_set_state(job, JOB_STATE_STOPPED);
		dag_schedule_ready();
		return;
	}

	/* Unexpected exit */
	job->restart_count++;

	if (should_restart(job, status)) {
		prowl_log(LOG_NOTICE,
		    "job %s will restart (attempt %d)",
		    job->label, job->restart_count);
		job_set_state(job, JOB_STATE_LOADED);
		dag_schedule_ready();
	} else {
		job_set_state(job, JOB_STATE_FAILED);
		prowl_log(LOG_ERR, "job %s entered failed state",
		    job->label);
	}
}

/*
 * Orderly shutdown of all managed services.
 */
void
supervisor_shutdown_all(void)
{
	job_t *job;
	time_t deadline;

	prowl_log(LOG_NOTICE, "orderly shutdown beginning");
	deadline = time(NULL) + g_config.shutdown_timeout;

	/* Send SIGTERM in reverse order */
	TAILQ_FOREACH_REVERSE(job, &g_jobs, job_list, entries) {
		if (job->state == JOB_STATE_RUNNING ||
		    job->state == JOB_STATE_STARTING)
			supervisor_stop(job, false);
	}

	/* Wait for shutdown-keyed services within the timeout budget */
	for (;;) {
		bool any_pending = false;
		int st;
		pid_t pid;

		if (time(NULL) >= deadline)
			break;

		TAILQ_FOREACH(job, &g_jobs, entries) {
			if (job->shutdown_wait &&
			    job->state == JOB_STATE_STOPPING) {
				any_pending = true;
				break;
			}
		}
		if (!any_pending)
			break;

		while ((pid = waitpid(-1, &st, WNOHANG)) > 0)
			supervisor_reap(pid, st);

		struct timespec ts = { 0, 100000000L };
		nanosleep(&ts, NULL);
	}

	/* Force-kill remaining processes */
	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (job->pid > 0)
			kill(job->pid, SIGKILL);
	}

	/* Final reap */
	for (;;) {
		int st;
		if (waitpid(-1, &st, WNOHANG) <= 0)
			break;
	}
}
