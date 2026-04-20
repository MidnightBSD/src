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

#include <sys/param.h>
#include <sys/event.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
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
 * Timer ident encoding (low 2 bits of job pointer, which is >=4-byte aligned).
 */
static uintptr_t
timer_ident(const job_t *job, uintptr_t kind)
{
	return ((uintptr_t)job | kind);
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

	EV_SET(&kev, timer_ident(job, TIMER_THROTTLE), EVFILT_TIMER,
	    EV_ADD | EV_ONESHOT, NOTE_MSECONDS, ms, NULL);
	if (kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1)
		prowl_log(LOG_WARNING, "kevent throttle timer %s: %m",
		    job->label);
	job->throttle_timer_active = true;
}

static void
arm_stop_timer(job_t *job)
{
	struct kevent kev;
	int64_t ms;

	ms = (int64_t)job->exit_timeout * 1000;
	if (ms <= 0)
		ms = DEFAULT_EXIT_TIMEOUT * 1000;

	EV_SET(&kev, timer_ident(job, TIMER_STOP), EVFILT_TIMER,
	    EV_ADD | EV_ONESHOT, NOTE_MSECONDS, ms, NULL);
	if (kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1)
		prowl_log(LOG_WARNING, "kevent stop timer %s: %m",
		    job->label);
	job->stop_timer_active = true;
}

/*
 * Arm a one-shot timer to fire after exit_timeout seconds.
 * Used for notify jobs that must send READY=1 before this deadline.
 * Timer type: NOTIFY_TMO (bits 0b11).
 */
static void
arm_notify_timeout_timer(job_t *job)
{
	struct kevent kev;
	int64_t ms;

	ms = (int64_t)(job->exit_timeout > 0 ?
	    job->exit_timeout : DEFAULT_EXIT_TIMEOUT) * 1000;

	EV_SET(&kev, timer_ident(job, NOTIFY_TMO), EVFILT_TIMER,
	    EV_ADD | EV_ONESHOT, NOTE_MSECONDS, ms, NULL);
	if (kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1)
		prowl_log(LOG_WARNING, "kevent notify timeout timer %s: %m",
		    job->label);
}

static void
cancel_notify_timeout_timer(job_t *job)
{
	struct kevent kev;

	EV_SET(&kev, timer_ident(job, NOTIFY_TMO), EVFILT_TIMER,
	    EV_DELETE, 0, 0, NULL);
	kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL);
}

/*
 * Arm a recurring watchdog timer (fires every watchdog_sec seconds).
 * Timer type: TIMER_WATCHDOG (bits 0b10).
 */
static void
arm_watchdog_timer(job_t *job)
{
	struct kevent kev;
	int64_t ms;

	if (job->watchdog_sec <= 0)
		return;

	ms = (int64_t)job->watchdog_sec * 1000;
	EV_SET(&kev, timer_ident(job, TIMER_WATCHDOG), EVFILT_TIMER,
	    EV_ADD, NOTE_MSECONDS, ms, NULL);
	if (kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1)
		prowl_log(LOG_WARNING, "kevent watchdog timer %s: %m",
		    job->label);
	else
		job->watchdog_timer_active = true;
}

static void
cancel_watchdog_timer(job_t *job)
{
	struct kevent kev;

	if (!job->watchdog_timer_active)
		return;
	EV_SET(&kev, timer_ident(job, TIMER_WATCHDOG), EVFILT_TIMER,
	    EV_DELETE, 0, 0, NULL);
	kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL);
	job->watchdog_timer_active = false;
}

/*
 * Build the notify socket filesystem path for a job.
 * sun_path is 104 bytes on BSD; returns -1 if path would truncate.
 */
static int
notify_socket_path(const job_t *job, char *out, size_t outsz)
{
	int n;

	n = snprintf(out, outsz, "%s/%s", g_notify_dir, job->label);
	if (n < 0 || (size_t)n >= outsz || (size_t)n >= 104)
		return (-1);
	return (0);
}

/*
 * Create, bind, and kqueue-register the notify datagram socket for a job.
 * Returns the fd, or -1 on failure.  The kev udata is set to the job pointer
 * so the event loop can dispatch EVFILT_READ events to the right job.
 */
static int
notify_socket_open(job_t *job)
{
	struct sockaddr_un sun;
	struct kevent kev;
	char path[PROWL_PATH_MAX];
	int fd;

	if (notify_socket_path(job, path, sizeof(path)) == -1) {
		prowl_log(LOG_WARNING,
		    "notify socket path too long for job %s", job->label);
		return (-1);
	}

	unlink(path);

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd == -1) {
		prowl_log(LOG_WARNING, "notify_socket_open socket: %m");
		return (-1);
	}

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, path, sizeof(sun.sun_path));

	if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		prowl_log(LOG_WARNING, "notify_socket_open bind %s: %m", path);
		close(fd);
		return (-1);
	}

	chmod(path, 0600);

	/* udata = job pointer so the event loop knows which job to notify */
	EV_SET(&kev, (uintptr_t)fd, EVFILT_READ, EV_ADD, 0, 0, (void *)job);
	if (kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1) {
		prowl_log(LOG_WARNING, "notify_socket_open kevent: %m");
		close(fd);
		unlink(path);
		return (-1);
	}

	return (fd);
}

/*
 * Deregister, close, and unlink the notify socket for a job.
 */
static void
notify_socket_close(job_t *job)
{
	struct kevent kev;
	char path[PROWL_PATH_MAX];

	if (job->notify_fd < 0)
		return;

	EV_SET(&kev, (uintptr_t)job->notify_fd, EVFILT_READ, EV_DELETE,
	    0, 0, NULL);
	kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL);

	if (notify_socket_path(job, path, sizeof(path)) == 0)
		unlink(path);

	close(job->notify_fd);
	job->notify_fd = -1;
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
 * Minimal environment given to jobs that do not specify an explicit
 * environment block.  prowld must never expose its own ambient environment
 * (LD_PRELOAD, LD_LIBRARY_PATH, proxy credentials, debug vars, etc.) to
 * managed services; execve(2) is always used so the child env is explicit.
 */
static const char *minimal_env[] = {
	"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
	"HOME=/",
	NULL
};

/*
 * Child-side setup after fork().  Configures the execution environment
 * and replaces the process image via execve(2).
 * Must not return; calls _exit on failure.
 */
static void
child_setup_and_exec(job_t *job)
{
	/* +7: NOTIFY_SOCKET, WATCHDOG_USEC, WATCHDOG_PID,
	 *     LISTEN_FDS, LISTEN_PID, LISTEN_FDNAMES, sentinel NULL */
	char *argv[PROWL_ARGS_MAX + 2];
	char *envp[PROWL_ENV_MAX + 7];
	char notify_env[PROWL_PATH_MAX + 16];
	char watchdog_usec_env[64];
	char watchdog_pid_env[64];
	char listen_fds_env[32];
	char listen_pid_env[32];
	/* "LISTEN_FDNAMES=" + 8 names * 63 chars + 7 colons + NUL */
	char listen_fdnames_env[600];
	int i, argc, envc;

	/*
	 * chroot(2) must be called as root before drop_privileges().
	 * After chroot we always chdir("/") to ensure the process cannot
	 * access paths outside the new root via a lingering CWD reference.
	 * working_directory, if set, is then interpreted relative to the
	 * chroot.  On failure we _exit so the service never runs with a
	 * weaker-than-expected containment boundary.
	 */
	if (job->root_directory[0] != '\0') {
		if (chroot(job->root_directory) == -1) {
			syslog(LOG_ERR,
			    "prowld: chroot %s failed for job %s: %m",
			    job->root_directory, job->label);
			_exit(1);
		}
		if (chdir("/") == -1)
			_exit(1);
	}

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

	/*
	 * Build envp.  Start from the unit's explicit environment block, or
	 * fall back to the hardcoded minimal set.  execve(2) is always used so
	 * prowld's ambient environment is never inherited by child processes.
	 */
	envc = 0;
	if (job->envc > 0) {
		for (i = 0; i < job->envc && envc < PROWL_ENV_MAX; i++)
			envp[envc++] = job->envp[i];
	} else {
		for (i = 0; minimal_env[i] != NULL && envc < PROWL_ENV_MAX; i++)
			envp[envc++] = (char *)(uintptr_t)minimal_env[i];
	}

	if (job->type == JOB_TYPE_RCSHIM) {
		/* rc-shim invocation: script start */
		envp[envc] = NULL;
		argv[0] = job->rcshim_path;
		argv[1] = (char *)(uintptr_t)"start";
		argv[2] = NULL;
		execve(argv[0], argv, envp);
		_exit(127);
	}

	/* Native unit: build argv */
	argv[0] = job->program;
	argc = 1;
	for (i = 0; i < job->argc && argc < PROWL_ARGS_MAX; i++)
		argv[argc++] = job->arguments[i];
	argv[argc] = NULL;

	/* Inject daemon-protocol environment variables */
	if (job->notify_type == NOTIFY_NOTIFY) {
		char npath[PROWL_PATH_MAX];
		if (notify_socket_path(job, npath, sizeof(npath)) == 0) {
			snprintf(notify_env, sizeof(notify_env),
			    "NOTIFY_SOCKET=%s", npath);
			envp[envc++] = notify_env;
		}
	}

	if (job->watchdog_sec > 0) {
		snprintf(watchdog_usec_env, sizeof(watchdog_usec_env),
		    "WATCHDOG_USEC=%lld",
		    (long long)job->watchdog_sec * 1000000LL);
		snprintf(watchdog_pid_env, sizeof(watchdog_pid_env),
		    "WATCHDOG_PID=%d", (int)getpid());
		envp[envc++] = watchdog_usec_env;
		envp[envc++] = watchdog_pid_env;
	}

	if (job->sockets_count > 0) {
		int ns = job->sockets_count;
		int tmpfds[PROWL_SOCKETS_MAX];
		int j;
		size_t off;

		/*
		 * Safely move socket fds to positions 3, 4, 5, ... :
		 * First dup each to a position above the target range,
		 * then dup2 each copy to its target position.
		 */
		for (j = 0; j < ns; j++) {
			tmpfds[j] = fcntl(job->sockets[j].fd, F_DUPFD, 3 + ns);
			if (tmpfds[j] == -1)
				_exit(1);
		}
		for (j = 0; j < ns; j++) {
			close(job->sockets[j].fd);
			if (dup2(tmpfds[j], 3 + j) == -1)
				_exit(1);
			close(tmpfds[j]);
			/* Clear FD_CLOEXEC so the daemon inherits the fd */
			fcntl(3 + j, F_SETFD, 0);
		}

		/* Build LISTEN_FDNAMES=name0:name1:... */
		off = strlcpy(listen_fdnames_env, "LISTEN_FDNAMES=",
		    sizeof(listen_fdnames_env));
		for (j = 0; j < ns; j++) {
			if (j > 0 && off < sizeof(listen_fdnames_env) - 1)
				listen_fdnames_env[off++] = ':';
			off += strlcpy(listen_fdnames_env + off,
			    job->sockets[j].name,
			    sizeof(listen_fdnames_env) - off);
		}

		snprintf(listen_fds_env, sizeof(listen_fds_env),
		    "LISTEN_FDS=%d", ns);
		snprintf(listen_pid_env, sizeof(listen_pid_env),
		    "LISTEN_PID=%d", (int)getpid());

		if (envc + 3 <= PROWL_ENV_MAX + 6) {
			envp[envc++] = listen_fds_env;
			envp[envc++] = listen_pid_env;
			envp[envc++] = listen_fdnames_env;
		}
	}

	envp[envc] = NULL;
	execve(job->program, argv, envp);
	_exit(127);
}

/*
 * Fork and start a job.  Returns 0 on success, -1 on fork failure.
 */
int
supervisor_start(job_t *job)
{
	pid_t pid;
	bool use_notify;

	if (job->state == JOB_STATE_RUNNING ||
	    job->state == JOB_STATE_STARTING)
		return (0);

	prowl_log(LOG_NOTICE, "starting %s", job->label);
	job->started_at = time(NULL);
	job_set_state(job, JOB_STATE_STARTING);

	resolve_job_privileges(job);

	/*
	 * Create the notify socket before fork so it exists when the child
	 * execs and reads NOTIFY_SOCKET from its environment.
	 */
	use_notify = false;
	if (job->notify_type == NOTIFY_NOTIFY &&
	    job->type != JOB_TYPE_RCSHIM) {
		job->notify_fd = notify_socket_open(job);
		if (job->notify_fd >= 0)
			use_notify = true;
		else
			prowl_log(LOG_WARNING,
			    "job %s: notify socket failed, "
			    "falling back to throttle timer", job->label);
	}

	pid = fork();
	if (pid == -1) {
		prowl_log(LOG_ERR, "fork %s: %m", job->label);
		notify_socket_close(job);
		job_set_state(job, JOB_STATE_FAILED);
		if (g_current_starts > 0)
			g_current_starts--;
		return (-1);
	}

	if (pid == 0) {
		/* Child: close descriptors that must not be inherited */
		if (g_kqueue_fd >= 0)
			close(g_kqueue_fd);
		/* Close the server side of the notify socket in the child */
		if (job->notify_fd >= 0)
			close(job->notify_fd);
		child_setup_and_exec(job);
		/* NOTREACHED */
	}

	/* Parent */
	job->pid = pid;
	watch_child(pid, job);

	if (use_notify) {
		/* Wait for READY=1; use exit_timeout as the deadline */
		arm_notify_timeout_timer(job);
	} else {
		/* Promote to RUNNING after throttle_interval seconds alive */
		arm_throttle_timer(job);
	}

	if (job->watchdog_sec > 0) {
		job->watchdog_last_ping = time(NULL);
		arm_watchdog_timer(job);
	}

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

	cancel_watchdog_timer(job);
	job_set_state(job, JOB_STATE_STOPPING);

	if (force) {
		kill(job->pid, SIGKILL);
	} else {
		kill(job->pid, SIGTERM);
		arm_stop_timer(job);
	}

	return (0);
}

/*
 * Fork and exec a socket-activated daemon, passing the bound socket fds
 * (already in job->sockets[]) as file descriptors 3, 4, 5, ...
 * Called from socket_handle_activation() on the first incoming connection.
 */
void
supervisor_socket_activate(job_t *job)
{
	pid_t pid;

	prowl_log(LOG_NOTICE, "socket-activating %s", job->label);
	job->started_at = time(NULL);
	job_set_state(job, JOB_STATE_STARTING);

	resolve_job_privileges(job);

	pid = fork();
	if (pid == -1) {
		prowl_log(LOG_ERR, "fork %s (socket activate): %m",
		    job->label);
		job_set_state(job, JOB_STATE_FAILED);
		socket_rearm(job);
		return;
	}

	if (pid == 0) {
		/* Child: close descriptors that must not be inherited */
		if (g_kqueue_fd >= 0)
			close(g_kqueue_fd);
		child_setup_and_exec(job);
		/* NOTREACHED */
	}

	/* Parent */
	job->pid = pid;
	job->socket_activated = true;
	job_set_state(job, JOB_STATE_RUNNING);
	watch_child(pid, job);

	prowl_log(LOG_INFO, "job %s socket-activated, pid %d",
	    job->label, (int)pid);
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
 * Called when the notify-timeout timer fires.
 * The job never sent READY=1 within exit_timeout seconds.
 * Kill immediately; supervisor_reap will apply keep_alive policy.
 */
void
supervisor_handle_notify_timeout(job_t *job)
{
	if (job->state != JOB_STATE_STARTING)
		return;

	prowl_log(LOG_WARNING,
	    "job %s (pid %d): no READY=1 within timeout; killing",
	    job->label, (int)job->pid);

	notify_socket_close(job);

	if (job->pid > 0)
		kill(job->pid, SIGKILL);
}

/*
 * Called when the watchdog timer fires (recurring every watchdog_sec seconds).
 * If the last keepalive ping was too long ago, treat the service as hung.
 */
void
supervisor_handle_watchdog(job_t *job)
{
	time_t now = time(NULL);

	if (job->state != JOB_STATE_RUNNING)
		return;

	if (now - job->watchdog_last_ping < (time_t)job->watchdog_sec)
		return;

	prowl_log(LOG_WARNING,
	    "job %s (pid %d): watchdog timeout; sending SIGTERM",
	    job->label, (int)job->pid);

	supervisor_stop(job, false);
}

/*
 * Validate a MAINPID= value received on the notify socket.
 *
 * Three checks must all pass:
 *   1. new_pid exists in the kernel process table (sysctl succeeds).
 *   2. new_pid's effective UID matches the uid the job was configured to
 *      run as (or 0 when no user was specified, since prowld is root).
 *   3. new_pid is a descendant of job->pid: walking ki_ppid from new_pid
 *      must reach job->pid within MAX_PPID_DEPTH steps.
 *
 * Without these checks a compromised service could redirect prowld's
 * future stop/watchdog signals onto arbitrary PIDs.
 */
#define MAX_PPID_DEPTH	32

static bool
mainpid_valid(const job_t *job, pid_t new_pid)
{
	struct kinfo_proc kp;
	int mib[4];
	size_t len;
	uid_t expected_uid;
	pid_t cur;
	int depth;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = (int)new_pid;
	len = sizeof(kp);

	/* Check 1: PID must exist. */
	if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1 || len == 0) {
		prowl_log(LOG_WARNING,
		    "job %s: MAINPID=%d rejected: pid does not exist",
		    job->label, (int)new_pid);
		return (false);
	}

	/*
	 * Check 2: UID must match the job's configured run user.
	 * If the job has no explicit user, prowld keeps root privileges,
	 * so the child must also be root.
	 */
	expected_uid = (job->run_priv_set && job->run_uid != (uid_t)-1)
	    ? job->run_uid : 0;
	if (kp.ki_uid != expected_uid) {
		prowl_log(LOG_WARNING,
		    "job %s: MAINPID=%d rejected: uid %u != expected %u",
		    job->label, (int)new_pid,
		    (unsigned)kp.ki_uid, (unsigned)expected_uid);
		return (false);
	}

	/*
	 * Check 3: new_pid must be a descendant of the tracked service PID.
	 * Walk ki_ppid upward; stop at PID 1 or if sysctl fails.
	 */
	cur = new_pid;
	for (depth = 0; depth < MAX_PPID_DEPTH; depth++) {
		if (cur == job->pid)
			return (true);
		if (cur <= 1)
			break;
		mib[3] = (int)cur;
		len = sizeof(kp);
		if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1 || len == 0)
			break;
		cur = kp.ki_ppid;
	}

	prowl_log(LOG_WARNING,
	    "job %s: MAINPID=%d rejected: not a descendant of pid %d",
	    job->label, (int)new_pid, (int)job->pid);
	return (false);
}

/*
 * Called when the notify socket becomes readable.
 * Reads one datagram, parses KEY=value\n lines, and dispatches them.
 */
void
supervisor_handle_notify(job_t *job)
{
	char buf[4096];
	ssize_t n;
	char *p, *end, *eq, *nl;

	if (job->notify_fd < 0)
		return;

	n = recv(job->notify_fd, buf, sizeof(buf) - 1, 0);
	if (n <= 0)
		return;
	buf[n] = '\0';

	p = buf;
	end = buf + n;

	while (p < end) {
		nl = memchr(p, '\n', (size_t)(end - p));
		if (nl != NULL)
			*nl = '\0';

		eq = strchr(p, '=');
		if (eq != NULL) {
			*eq = '\0';
			const char *key = p;
			const char *val = eq + 1;

			if (strcmp(key, "READY") == 0 &&
			    strcmp(val, "1") == 0) {
				if (job->state == JOB_STATE_STARTING) {
					cancel_notify_timeout_timer(job);
					job_set_state(job, JOB_STATE_RUNNING);
					if (g_current_starts > 0)
						g_current_starts--;
					prowl_log(LOG_INFO,
					    "job %s ready (READY=1)",
					    job->label);
					dag_schedule_ready();
				}
			} else if (strcmp(key, "STATUS") == 0) {
				strlcpy(job->status_msg, val,
				    sizeof(job->status_msg));
			} else if (strcmp(key, "MAINPID") == 0) {
				pid_t new_pid = (pid_t)strtol(val, NULL, 10);
				if (new_pid > 0 && new_pid != job->pid &&
				    mainpid_valid(job, new_pid)) {
					struct kevent kev;
					/* Drop watcher for the old pid */
					if (job->pid > 0) {
						EV_SET(&kev,
						    (uintptr_t)job->pid,
						    EVFILT_PROC, EV_DELETE,
						    0, 0, NULL);
						kevent(g_kqueue_fd, &kev,
						    1, NULL, 0, NULL);
					}
					job->pid = new_pid;
					watch_child(new_pid, job);
					prowl_log(LOG_INFO,
					    "job %s main pid → %d",
					    job->label, (int)new_pid);
				}
			} else if (strcmp(key, "WATCHDOG") == 0 &&
			    strcmp(val, "1") == 0) {
				job->watchdog_last_ping = time(NULL);
			} else if (strcmp(key, "RELOADING") == 0) {
				prowl_log(LOG_INFO,
				    "job %s signaled RELOADING", job->label);
			} else if (strcmp(key, "STOPPING") == 0) {
				prowl_log(LOG_INFO,
				    "job %s signaled STOPPING", job->label);
			} else if (strcmp(key, "ERRNO") == 0) {
				prowl_log(LOG_WARNING,
				    "job %s startup errno: %s",
				    job->label, val);
			} else if (strcmp(key, "EXTEND_TIMEOUT_USEC") == 0) {
				/* Extend the notify timeout by the given µs */
				long long usec = strtoll(val, NULL, 10);
				if (usec > 0) {
					struct kevent kev;
					int64_t ms_extra =
					    (int64_t)(usec / 1000);
					cancel_notify_timeout_timer(job);
					EV_SET(&kev,
					    timer_ident(job, NOTIFY_TMO),
					    EVFILT_TIMER,
					    EV_ADD | EV_ONESHOT,
					    NOTE_MSECONDS, ms_extra, NULL);
					kevent(g_kqueue_fd, &kev,
					    1, NULL, 0, NULL);
				}
			}
			/* FDSTORE, unknown keys: silently ignored */
		}

		if (nl == NULL)
			break;
		p = nl + 1;
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
		EV_SET(&kev, timer_ident(job, TIMER_THROTTLE), EVFILT_TIMER,
		    EV_DELETE, 0, 0, NULL);
		kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL);
		job->throttle_timer_active = false;
		if (g_current_starts > 0)
			g_current_starts--;
	}

	cancel_watchdog_timer(job);
	cancel_notify_timeout_timer(job);
	notify_socket_close(job);

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
		if (job->sockets_count > 0 && job->socket_activated)
			socket_rearm(job);
		else
			dag_schedule_ready();
		return;
	}

	/* Socket-activated daemon exit: re-arm sockets for next connection */
	if (job->sockets_count > 0 && job->socket_activated) {
		job_set_state(job, JOB_STATE_LOADED);
		socket_rearm(job);
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
