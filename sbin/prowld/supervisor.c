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
#include <sys/procctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/un.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <libutil.h>
#include <limits.h>
#include <paths.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <ucl.h>

#include "prowld.h"

static pid_t	fork_launcher(job_t *);
static bool	not_exec_yet(pid_t);

static bool g_dnssd_available = true;
static char g_dnssd_path[PROWL_PATH_MAX];

#define ACTION_RUNS_MAX		8
#define ACTION_READ_MAX		65536

struct action_run {
	uintptr_t	tag;
	pid_t		pid;
	int		fd;
	int		code;
	struct timespec	deadline;
	char		label[PROWL_LABEL_MAX];
	size_t		len;
	char		out[PROWL_ACTION_OUT_MAX + 1];
};

static struct action_run action_runs[ACTION_RUNS_MAX];

/* Internal prototypes */
static void supervisor_mdns_register(job_t *job);
static void supervisor_mdns_deregister(job_t *job);

void
supervisor_init(void)
{
	/* 
	 * Simple check for dns-sd(1).  We assume it's in a standard path.
	 * If not found, mdns registration is disabled and we log a warning.
	 */
	const char *paths[] = { "/usr/bin/dns-sd", "/usr/local/bin/dns-sd", NULL };
	bool found = false;

	for (int i = 0; paths[i] != NULL; i++) {
		if (access(paths[i], X_OK) == 0) {
			strlcpy(g_dnssd_path, paths[i], sizeof(g_dnssd_path));
			found = true;
			break;
		}
	}


	if (!found) {
		g_dnssd_available = false;
		prowl_log(LOG_WARNING,
		    "dns-sd(1) not found in standard paths; mDNS discovery disabled");
	}
}

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


void
timer_state_load(void)
{
	struct ucl_parser *parser;
	ucl_object_t *root;
	const ucl_object_t *obj;
	job_t *job;

	if (access(PROWLD_TIMER_STATE_PATH, R_OK) != 0)
		return;

	parser = ucl_parser_new(UCL_PARSER_NO_FILEVARS);
	if (!ucl_parser_add_file(parser, PROWLD_TIMER_STATE_PATH)) {
		prowl_log(LOG_WARNING, "timer_state_load: %.128s",
		    ucl_parser_get_error(parser));
		ucl_parser_free(parser);
		return;
	}

	root = ucl_parser_get_object(parser);
	ucl_parser_free(parser);

	TAILQ_FOREACH(job, &g_jobs, entries) {
		obj = ucl_object_lookup(root, job->label);
		if (obj != NULL && ucl_object_type(obj) == UCL_OBJECT) {
			const ucl_object_t *lr = ucl_object_lookup(obj, "last_run");
			if (lr != NULL)
				job->last_run = (time_t)ucl_object_toint(lr);
		}
	}

	ucl_object_unref(root);
}

void
timer_state_save(void)
{
	ucl_object_t *root, *obj;
	job_t *job;
	char tmp_path[PROWL_PATH_MAX];
	int fd;
	FILE *fp;

	root = ucl_object_typed_new(UCL_OBJECT);

	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (job->type != JOB_TYPE_TIMER || job->last_run == 0)
			continue;

		obj = ucl_object_typed_new(UCL_OBJECT);
		ucl_object_insert_key(obj, ucl_object_fromint((int64_t)job->last_run),
		    "last_run", 0, false);
		ucl_object_insert_key(root, obj, job->label, 0, false);
	}

	snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", PROWLD_TIMER_STATE_PATH);
	fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0644);
	if (fd == -1) {
		prowl_log(LOG_ERR, "timer_state_save open: %m");
		ucl_object_unref(root);
		return;
	}

	fp = fdopen(fd, "w");
	if (fp == NULL) {
		close(fd);
		ucl_object_unref(root);
		return;
	}

	{
		unsigned char *res = ucl_object_emit(root, UCL_EMIT_CONFIG);
		if (res != NULL) {
			fprintf(fp, "%s", res);
			free(res);
			fflush(fp);
			fsync(fileno(fp));
			fclose(fp);
			if (rename(tmp_path, PROWLD_TIMER_STATE_PATH) == -1)
				prowl_log(LOG_ERR, "timer_state_save rename: %m");
		} else {
			prowl_log(LOG_ERR, "timer_state_save: ucl_object_emit OOM");
			fclose(fp);
			unlink(tmp_path);
		}
	}

	ucl_object_unref(root);
}

/*
 * Check if a calendar schedule matches the given broken-down time.
 */
static bool
calendar_match(const calendar_schedule_t *s, const struct tm *tm)
{
	if (s->minute != -1 && s->minute != tm->tm_min)
		return (false);
	if (s->hour != -1 && s->hour != tm->tm_hour)
		return (false);
	if (s->day != -1 && s->day != tm->tm_mday)
		return (false);
	if (s->month != -1 && s->month != tm->tm_mon + 1)
		return (false);
	if (s->weekday != -1 && s->weekday != tm->tm_wday)
		return (false);
	return (true);
}

/*
 * Iterate all jobs and launch any that match the current minute.
 */
void
supervisor_handle_calendar_tick(void)
{
	job_t *job;
	time_t now = time(NULL);
	struct tm tm;
	bool changed = false;

	localtime_r(&now, &tm);

	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (!job->masked && job->state != JOB_STATE_STOPPING &&
		    job->schedule.has_calendar &&
		    calendar_match(&job->schedule.calendar, &tm)) {
			prowl_log(LOG_INFO, "timer %s: calendar match",
			    job->label);
			job->last_run = now;
			changed = true;
			supervisor_start(job);
		}
	}

	if (changed)
		timer_state_save();
}

/*
 * Arm a recurring periodic timer.
 */
void
arm_periodic_timer(job_t *job)
{
	struct kevent kev;

	if (job->schedule.interval <= 0)
		return;

	EV_SET(&kev, timer_ident(job, TIMER_PERIODIC), EVFILT_TIMER,
	    EV_ADD, NOTE_SECONDS, job->schedule.interval, NULL);
	if (kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1)
		prowl_log(LOG_WARNING, "kevent periodic timer %s: %m",
		    job->label);
}

/*
 * Arm a one-shot boot delay timer.
 */
void
arm_boot_delay_timer(job_t *job)
{
	struct kevent kev;

	if (job->schedule.on_boot_delay <= 0)
		return;

	EV_SET(&kev, timer_ident(job, TIMER_BOOT_DELAY), EVFILT_TIMER,
	    EV_ADD | EV_ONESHOT, NOTE_SECONDS, job->schedule.on_boot_delay,
	    NULL);
	if (kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1)
		prowl_log(LOG_WARNING, "kevent boot delay timer %s: %m",
		    job->label);
}

/*
 * Register a one-shot timer to fire after throttle_interval seconds.
 * Low bit of ident is 0: throttle timer.
 */
static void
arm_throttle_timer_ms(job_t *job, int64_t ms)
{
	struct kevent kev;

	EV_SET(&kev, timer_ident(job, TIMER_THROTTLE), EVFILT_TIMER,
	    EV_ADD | EV_ONESHOT, NOTE_MSECONDS, ms, NULL);
	if (kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1)
		prowl_log(LOG_WARNING, "kevent throttle timer %s: %m",
		    job->label);
	job->throttle_timer_active = true;
}

static void
arm_throttle_timer(job_t *job, int secs)
{
	int64_t ms;

	ms = (int64_t)secs * 1000;
	if (ms <= 0)
		ms = DEFAULT_THROTTLE_INT * 1000;
	arm_throttle_timer_ms(job, ms);
}

static void
arm_stop_timer(job_t *job)
{
	struct kevent kev;
	int64_t ms;

	ms = (int64_t)job->exit_timeout * 1000;
	if (ms <= 0)
		ms = DEFAULT_EXIT_TIMEOUT * 1000;

	/* A previous timeout may still be in the current kevent batch. */
	job->stop_timer_generation++;
	EV_SET(&kev, timer_ident(job, TIMER_STOP), EVFILT_TIMER,
	    EV_ADD | EV_ONESHOT, NOTE_MSECONDS, ms,
	    (void *)job->stop_timer_generation);
	if (kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1)
		prowl_log(LOG_WARNING, "kevent stop timer %s: %m",
		    job->label);
	job->stop_timer_active = true;
}

static void
cancel_stop_timer(job_t *job)
{
	struct kevent kev;

	if (!job->stop_timer_active)
		return;
	EV_SET(&kev, timer_ident(job, TIMER_STOP), EVFILT_TIMER, EV_DELETE,
	    0, 0, NULL);
	(void)kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL);
	job->stop_timer_active = false;
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
	struct stat sb;
	char path[PROWL_PATH_MAX];
	uid_t uid;
	mode_t oldmask;
	int dirfd, fd, on, rc, saved_errno;

	if (notify_socket_path(job, path, sizeof(path)) == -1) {
		prowl_log(LOG_WARNING,
		    "notify socket path too long for job %s", job->label);
		return (-1);
	}
	/* Check every path component before unlinking or creating a socket. */
	dirfd = trusted_open_dir(g_notify_dir);
	if (dirfd == -1) {
		prowl_log(LOG_WARNING, "unsafe notify directory %s: %m",
		    g_notify_dir);
		return (-1);
	}
	if (fstat(dirfd, &sb) == -1 || (sb.st_mode & 0777) != 0711) {
		prowl_log(LOG_WARNING, "notify directory %s is not usable",
		    g_notify_dir);
		close(dirfd);
		return (-1);
	}
	if (unlinkat(dirfd, job->label, 0) == -1 && errno != ENOENT) {
		prowl_log(LOG_WARNING, "unlink notify socket %s: %m", path);
		close(dirfd);
		return (-1);
	}

	fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (fd == -1) {
		prowl_log(LOG_WARNING, "notify_socket_open socket: %m");
		close(dirfd);
		return (-1);
	}
	on = 1;
	if (setsockopt(fd, SOL_LOCAL, LOCAL_CREDS_PERSISTENT, &on,
	    sizeof(on)) == -1) {
		prowl_log(LOG_WARNING, "notify credentials: %m");
		close(fd);
		close(dirfd);
		return (-1);
	}
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, path, sizeof(sun.sun_path));
	oldmask = umask(0177);
	rc = bind(fd, (struct sockaddr *)&sun, sizeof(sun));
	saved_errno = errno;
	(void)umask(oldmask);
	if (rc == -1) {
		errno = saved_errno;
		prowl_log(LOG_WARNING, "notify_socket_open bind %s: %m", path);
		close(fd);
		close(dirfd);
		return (-1);
	}
	uid = (job->run_priv_set && job->run_uid != (uid_t)-1) ?
	    job->run_uid : 0;
	if (fchownat(dirfd, job->label, uid, (gid_t)-1,
	    AT_SYMLINK_NOFOLLOW) == -1 ||
	    fchmodat(dirfd, job->label, 0600, 0) == -1) {
		prowl_log(LOG_WARNING, "notify socket permissions %s: %m",
		    path);
		goto fail;
	}

	EV_SET(&kev, (uintptr_t)fd, EVFILT_READ, EV_ADD, 0, 0, (void *)job);
	if (kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1) {
		prowl_log(LOG_WARNING, "notify_socket_open kevent: %m");
		goto fail;
	}
	job->notify_uid = uid;
	close(dirfd);
	return (fd);

fail:
	(void)unlinkat(dirfd, job->label, 0);
	close(dirfd);
	close(fd);
	return (-1);
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
 * Undo the signal setup prowld needs for itself before exec'ing a child.
 * Ignored dispositions survive execve(2) and the signal mask is inherited,
 * so without this every service would start with SIGTERM, SIGINT, SIGHUP
 * and SIGCHLD ignored and could not be stopped with SIGTERM.
 * Async-signal-safe: only signal(2) and sigprocmask(2).
 */
void
prowl_reset_signals(void)
{
	sigset_t set;
	int sig;

	for (sig = 1; sig < NSIG; sig++) {
		if (sig == SIGKILL || sig == SIGSTOP)
			continue;
		(void)signal(sig, SIG_DFL);
	}
	sigemptyset(&set);
	(void)sigprocmask(SIG_SETMASK, &set, NULL);
}

/* Redirect a standard file descriptor in the child before exec. */
static void
child_redirect_fd(const job_t *job, int stdfd, const char *path, int flags)
{
	int fd;

	fd = open(path, flags, 0644);
	if (fd == -1 || (fd != stdfd && dup2(fd, stdfd) == -1)) {
		syslog(LOG_ERR, "prowld: %s: cannot redirect fd %d to %s: %m",
		    job->label, stdfd, path);
		_exit(1);
	}
	if (fd != stdfd)
		close(fd);
}

/*
 * Resolve user/group names to numeric IDs in the parent before fork().
 * getpwnam/getgrnam/getgrouplist are not async-signal-safe; calling them
 * here ensures the child only uses the safe setgroups/setgid/setuid calls.
 */
static bool
resolve_job_privileges(job_t *job)
{
	struct passwd *pw;
	struct group *gr;
	uid_t uid;
	gid_t gid, groups[PROWL_GROUPS_MAX];
	int ngroups;

	if (job->user[0] == '\0' && job->group[0] == '\0') {
		job->run_priv_set = false;
		return (true);
	}

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	ngroups = 0;
	if (job->group[0] != '\0') {
		gr = getgrnam(job->group);
		if (gr == NULL || gr->gr_gid == (gid_t)-1) {
			prowl_log(LOG_WARNING, "job %s: cannot resolve group '%s'",
			    job->label, job->group);
			return (false);
		}
		gid = gr->gr_gid;
	}

	if (job->user[0] != '\0') {
		pw = getpwnam(job->user);
		if (pw == NULL || pw->pw_uid == (uid_t)-1 ||
		    (gid == (gid_t)-1 && pw->pw_gid == (gid_t)-1)) {
			prowl_log(LOG_WARNING, "job %s: cannot resolve user '%s'",
			    job->label, job->user);
			return (false);
		}
		uid = pw->pw_uid;
		if (gid == (gid_t)-1)
			gid = pw->pw_gid;
		ngroups = PROWL_GROUPS_MAX;
		if (getgrouplist(pw->pw_name, gid, groups, &ngroups) != 0 ||
		    ngroups < 1 || ngroups > PROWL_GROUPS_MAX ||
		    groups[0] != gid) {
			prowl_log(LOG_WARNING, "job %s: cannot resolve group "
			    "list for user '%s' (limit %d)", job->label,
			    job->user, PROWL_GROUPS_MAX);
			return (false);
		}
		memcpy(job->run_groups, groups, ngroups * sizeof(groups[0]));
	}

	job->run_uid = uid;
	job->run_gid = gid;
	job->run_ngroups = ngroups;
	job->run_priv_set = true;
	return (true);
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

	if (setgroups(job->run_ngroups,
	    job->run_ngroups == 0 ? NULL : job->run_groups) == -1) {
		syslog(LOG_ERR, "prowld: setgroups failed: %m");
		_exit(1);
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
 * Expand a start_pre or stop_post command into program and argv, with the
 * rc.conf variables filled in.  A "-" in front of the program, which is
 * dropped, means a failure of the command is ignored.  argv has room for
 * PROWL_ARGS_MAX + 2 words.  Returns 0, or -1 if it does not fit.
 */
static int
cmd_build(const job_t *job, const prowl_cmd_t *c, char *program, size_t psz,
    char **argv, bool *ignore)
{
	int first = prowl_cmd_first(c);
	const char *p = c->argv[first];
	int n;

	*ignore = false;
	if (*p == '-') {
		*ignore = true;
		p++;
	}
	if (rcconf_expand(job->rc_name, p, program, psz) == -1)
		return (-1);
	n = rcconf_expand_args(job->rc_name, c->argv + first + 1,
	    c->argc - first - 1, &argv[1], PROWL_ARGS_MAX);
	if (n == -1)
		return (-1);
	argv[0] = program;
	argv[1 + n] = NULL;
	return (0);
}

/*
 * Do the conditions in front of a command hold?  See prowl_cmd_first().
 */
static bool
cmd_enabled(const job_t *job, const prowl_cmd_t *c)
{
	int i, first = prowl_cmd_first(c);

	for (i = 0; i < first; i++) {
		if (!prowl_cond_holds(job->rc_name, c->argv[i]))
			return (false);
	}
	return (true);
}

/*
 * In the child, before it execs the job: run the commands of a list
 * (start_pre, or stop_pre when the job is stopped) one after the other.
 * The first that fails, unless it is marked with "-", ends the child, so
 * the job does not start or stop.
 */
static void
run_pre_list(const job_t *job, int list, char *const *envp)
{
	const char *lname = list == PROWL_CMD_PRE ? "start_pre" : "stop_pre";
	char program[PROWL_PATH_MAX];
	char *argv[PROWL_ARGS_MAX + 2];
	bool ignore;
	pid_t pid;
	int i, st;

	for (i = 0; i < job->cmd_count[list]; i++) {
		if (!cmd_enabled(job, &job->cmds[list][i]))
			continue;
		if (cmd_build(job, &job->cmds[list][i], program,
		    sizeof(program), argv, &ignore) == -1) {
			syslog(LOG_ERR, "prowld: %s: cannot expand %s",
			    job->label, lname);
			_exit(1);
		}
		pid = fork();
		if (pid == -1)
			_exit(1);
		if (pid == 0) {
			execve(argv[0], argv, envp);
			_exit(127);
		}
		while (waitpid(pid, &st, 0) == -1 && errno == EINTR)
			;
		if (!ignore && !(WIFEXITED(st) && WEXITSTATUS(st) == 0)) {
			syslog(LOG_ERR, "prowld: %s: %s %s failed",
			    job->label, lname, argv[0]);
			_exit(1);
		}
	}
}

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
	char program[PROWL_PATH_MAX];
	int i, argc, envc;

	prowl_reset_signals();

	/*
	 * Kernel modules, as rc.subr's required_modules: an entry is the
	 * file to load, or file:module when the module has another name.
	 * A module that is already there, or built in, is left alone.
	 */
	for (i = 0; job->run_cmd == NULL &&
	    i < job->required_modules_count; i++) {
		char ent[PROWL_MODULE_LEN];
		char *mod;

		strlcpy(ent, job->required_modules[i], sizeof(ent));
		/* "?name module": only when the rc.conf variable holds */
		if (prowl_is_cond(ent)) {
			char *sp = strchr(ent, ' ');

			if (sp == NULL)
				continue;
			*sp++ = '\0';
			if (!prowl_cond_holds(job->rc_name, ent))
				continue;
			sp += strspn(sp, " ");
			memmove(ent, sp, strlen(sp) + 1);
		}
		mod = strchr(ent, ':');
		if (mod != NULL)
			*mod++ = '\0';
		else
			mod = ent;
		if (modfind(mod) == -1 && kldload(ent) == -1) {
			syslog(LOG_ERR, "prowld: %s: cannot load kernel "
			    "module %s: %m", job->label, ent);
			_exit(1);
		}
	}

	/*
	 * PID 1 supplies a read-only null descriptor. Other invocations
	 * must set up default input before entering a unit's chroot.
	 */
	if (!g_is_init && job->stdin_path[0] == '\0')
		child_redirect_fd(job, STDIN_FILENO, _PATH_DEVNULL, O_RDONLY);

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
	else
		umask(0022);

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
		child_redirect_fd(job, STDIN_FILENO, job->stdin_path, O_RDONLY);

	/*
	 * A reload, check or resync command already has standard output
	 * and standard error on the pipe that supervisor_read_action()
	 * reads, so that prowlctl can show what it printed.
	 */
	if (job->action == PROWL_ACTION_NONE &&
	    job->stdout_path[0] != '\0')
		child_redirect_fd(job, STDOUT_FILENO, job->stdout_path,
		    O_WRONLY | O_CREAT | O_APPEND);

	if (job->action == PROWL_ACTION_NONE &&
	    job->stderr_path[0] != '\0')
		child_redirect_fd(job, STDERR_FILENO, job->stderr_path,
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
		char rcpid_env[32];

		/*
		 * rc-shim invocation.  When prowld is the init process it is
		 * doing what /etc/rc does at boot, so give the script the
		 * same context: autoboot=yes (fsck_start tests it), the
		 * "faststart" argument, and RC_PID naming the process that
		 * stop_boot() must SIGTERM to abort the boot.  Otherwise this
		 * is a manual start, which gets a plain "start".
		 */
		if (g_boot_mode && !job->rc_stopping &&
		    envc + 3 <= PROWL_ENV_MAX + 6) {
			snprintf(rcpid_env, sizeof(rcpid_env), "RC_PID=%d",
			    (int)getppid());
			if (!g_fastboot)
				envp[envc++] =
				    (char *)(uintptr_t)"autoboot=yes";
			envp[envc++] = (char *)(uintptr_t)"rc_fast=yes";
			envp[envc++] = rcpid_env;
		}
		envp[envc] = NULL;
		argv[0] = job->rcshim_path;
		if (job->rc_stopping)
			argv[1] = (char *)(uintptr_t)"stop";
		else
			argv[1] = (char *)(uintptr_t)(g_boot_mode ?
			    "faststart" : "start");
		argv[2] = NULL;
		rcshim_exec(argv, envp);
		_exit(127);
	}

	/* start_pre commands come before the job itself, stop_pre before stop */
	if (job->run_cmd == NULL && job->action == PROWL_ACTION_NONE) {
		int pl = job->rc_stopping ? PROWL_CMD_STOP_PRE : PROWL_CMD_PRE;

		if (job->cmd_count[pl] > 0) {
			envp[envc] = NULL;
			run_pre_list(job, pl, envp);
		}
	}

	/* A stop_post command */
	if (job->run_cmd != NULL) {
		bool ignore;

		if (cmd_build(job, job->run_cmd, program, sizeof(program),
		    argv, &ignore) == -1) {
			syslog(LOG_ERR, "prowld: %s: cannot expand stop_post",
			    job->label);
			_exit(1);
		}
		for (argc = 0; argv[argc] != NULL; argc++)
			;
	} else {
		/* Native unit: build argv */
		char *const *src;
		int nsrc, n;

		if (job->action != PROWL_ACTION_NONE) {
			src = job->action_args[job->action];
			nsrc = job->action_argc[job->action];
		} else if (job->rc_stopping && job->stop_argc > 0) {
			src = job->stop_arguments;
			nsrc = job->stop_argc;
		} else {
			src = job->arguments;
			nsrc = job->argc;
		}

		/* @name@ is an rc.conf variable, see prowl-unit(5) */
		if (rcconf_expand(job->rc_name, job->program, program,
		    sizeof(program)) == -1) {
			syslog(LOG_ERR, "prowld: %s: program too long",
			    job->label);
			_exit(1);
		}
		n = rcconf_expand_args(job->rc_name, src, nsrc, &argv[1],
		    PROWL_ARGS_MAX);
		if (n == -1) {
			syslog(LOG_ERR, "prowld: %s: cannot expand arguments",
			    job->label);
			_exit(1);
		}
		argv[0] = program;
		argc = 1 + n;
		argv[argc] = NULL;
	}

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
	execve(argv[0], argv, envp);
	_exit(127);
}

static void
release_start_slot(job_t *job)
{
	if (job->start_pending) {
		job->start_pending = false;
		g_current_starts--;
	}
}

/*
 * Fork and start a job.  Returns 0 on success, -1 if the job cannot start.
 */
int
supervisor_start(job_t *job)
{
	pid_t pid;
	bool use_notify;

	if (job->masked || job->state == JOB_STATE_STOPPING)
		return (-1);

	/* Leave even an explicit start queued until PID 1 finishes boot. */
	if (job->is_getty && !g_multi_user)
		return (0);

	if (job->state == JOB_STATE_RUNNING ||
	    job->state == JOB_STATE_STARTING)
		return (0);

	/* A missing configuration file keeps the job down, as rc.subr does */
	for (int i = 0; i < job->required_files_count; i++) {
		char rf[PROWL_PATH_MAX];

		if (rcconf_expand(job->rc_name, job->required_files[i], rf,
		    sizeof(rf)) == -1)
			strlcpy(rf, job->required_files[i], sizeof(rf));
		if (access(rf, R_OK) == -1) {
			prowl_log(LOG_WARNING, "job %s not started: required "
			    "file %s: %m", job->label, rf);
			job_set_state(job, JOB_STATE_FAILED);
			return (-1);
		}
	}

	if (!resolve_job_privileges(job)) {
		job_set_state(job, JOB_STATE_FAILED);
		return (-1);
	}

	prowl_log(LOG_NOTICE, "starting %s", job->label);
	job->started_at = time(NULL);
	job->restart_hold = false;
	job_set_state(job, JOB_STATE_STARTING);

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
		else {
			prowl_log(LOG_WARNING, "job %s: notify socket failed",
			    job->label);
			job_set_state(job, JOB_STATE_FAILED);
			return (-1);
		}
	}

	/* A pidfile is a hint; the private reaper verifies the launch. */
	job->adopted = false;
	job->launch_ready = false;
	job->launch_stop_signal = 0;
	job->launcher = job->pid_file[0] != '\0' || job->pid_name[0] != '\0';

	pid = job->launcher ? fork_launcher(job) : fork();
	if (pid == -1) {
		prowl_log(LOG_ERR, "fork %s: %m", job->label);
		notify_socket_close(job);
		job_set_state(job, JOB_STATE_FAILED);
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
	job->start_pending = true;
	g_current_starts++;
	watch_child(pid, job);

	if (use_notify) {
		/* Wait for READY=1; use exit_timeout as the deadline */
		arm_notify_timeout_timer(job);
	} else if (job->type != JOB_TYPE_ONESHOT &&
	    job->type != JOB_TYPE_RCSHIM && !job->launcher) {
		/*
		 * Promote to RUNNING once the process has survived a moment,
		 * so that jobs ordered after this one are not held back for
		 * a whole throttle_interval.  throttle_interval still governs
		 * the restart back-off.
		 */
		arm_throttle_timer(job, PROWL_READY_DELAY);
	}
	/*
	 * Oneshots and rc-shims have no readiness timer: they stay STARTING
	 * until they exit, so jobs that depend on them (an fsck, a mount)
	 * wait for the work to finish rather than for a timeout.
	 */

	if (job->watchdog_sec > 0) {
		job->watchdog_last_ping = time(NULL);
		arm_watchdog_timer(job);
	}

	prowl_log(LOG_INFO, "job %s started, pid %d", job->label, (int)pid);
	return (0);
}

/*
 * Run an rc-shim's "stop" in a child that is tracked like any other job
 * process, so that STOPPING ends when the script exits and the stop timer
 * bounds how long it may take.
 */
static int
supervisor_rc_stop(job_t *job)
{
	pid_t pid;

	if (!resolve_job_privileges(job))
		return (-1);

	if (job->type == JOB_TYPE_RCSHIM)
		prowl_log(LOG_NOTICE, "stopping %s via %s stop", job->label,
		    job->rcshim_path);
	else
		prowl_log(LOG_NOTICE, "stopping %s via its stop command",
		    job->label);
	job->rc_stopping = true;

	pid = fork();
	if (pid == -1) {
		prowl_log(LOG_ERR, "fork %s (stop): %m", job->label);
		job->rc_stopping = false;
		return (-1);
	}
	if (pid == 0) {
		if (g_kqueue_fd >= 0)
			close(g_kqueue_fd);
		child_setup_and_exec(job);
		/* NOTREACHED */
	}

	job->pid = pid;
	job_set_state(job, JOB_STATE_STOPPING);
	watch_child(pid, job);
	arm_stop_timer(job);
	return (0);
}

/* Keep a killed child's slot until SIGCHLD reaps it. */
static void
action_finish(struct action_run *run, int code, bool reply)
{
	uintptr_t tag = run->tag;

	if (run->fd >= 0) {
		close(run->fd);
		run->fd = -1;
	}
	if (run->pid > 0 && kill(run->pid, SIGKILL) == -1 && errno != ESRCH)
		prowl_log(LOG_WARNING, "kill action for %s: %m", run->label);
	run->tag = 0;
	if (reply)
		ipc_action_complete(tag, code, run->out);
}

static void
action_complete_if_ready(struct action_run *run)
{
	struct timespec now;

	if (run->fd >= 0 || run->pid > 0)
		return;
	if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
		action_finish(run, -1, true);
	else if (timespeccmp(&now, &run->deadline, >=))
		action_finish(run, 124, true);
	else
		action_finish(run, run->code, true);
}

/*
 * Fork an action using the unit's normal credentials and environment. The
 * pipe is watched independently of SIGCHLD: either can finish first.
 * Return 0 on success, EBUSY for this unit, EAGAIN at capacity, or -1.
 */
int
supervisor_start_action(job_t *job, int action, uintptr_t tag)
{
	struct action_run *run = NULL;
	struct timespec deadline;
	struct kevent kev;
	int fds[2];
	pid_t pid;

	if (g_shutdown || action <= PROWL_ACTION_NONE ||
	    action >= PROWL_ACTION_MAX || job->action_argc[action] == 0)
		return (-1);

	for (size_t i = 0; i < nitems(action_runs); i++) {
		struct action_run *active = &action_runs[i];

		if (active->tag == 0 && active->pid == 0) {
			if (run == NULL)
				run = active;
		} else if (strcmp(active->label, job->label) == 0)
			return (EBUSY);
	}
	if (run == NULL)
		return (EAGAIN);
	if (!resolve_job_privileges(job))
		return (-1);

	if (pipe2(fds, O_CLOEXEC) == -1) {
		prowl_log(LOG_ERR, "pipe %s: %m", job->label);
		return (-1);
	}
	EV_SET(&kev, (uintptr_t)fds[0], EVFILT_READ, EV_ADD, 0, 0,
	    (void *)tag);
	if (fcntl(fds[0], F_SETFL, O_NONBLOCK) == -1 ||
	    kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1 ||
	    clock_gettime(CLOCK_MONOTONIC, &deadline) == -1) {
		prowl_log(LOG_ERR, "action setup for %s: %m", job->label);
		goto fail;
	}
	deadline.tv_sec += PROWL_ACTION_TIMEOUT;
	pid = fork();
	if (pid == -1) {
		prowl_log(LOG_ERR, "fork %s: %m", job->label);
		goto fail;
	}
	if (pid == 0) {
		close(fds[0]);
		close(g_kqueue_fd);
		if (fcntl(fds[1], F_SETFD, 0) == -1 ||
		    dup2(fds[1], STDOUT_FILENO) == -1 ||
		    dup2(fds[1], STDERR_FILENO) == -1)
			_exit(127);
		if (fds[1] > STDERR_FILENO)
			close(fds[1]);
		job->action = action;
		child_setup_and_exec(job);
		_exit(127);
	}
	close(fds[1]);

	run->tag = tag;
	run->pid = pid;
	run->fd = fds[0];
	run->deadline = deadline;
	strlcpy(run->label, job->label, sizeof(run->label));
	run->len = 0;
	run->out[0] = '\0';
	return (0);

fail:
	close(fds[0]);
	close(fds[1]);
	return (-1);
}

void
supervisor_read_action(int fd, uintptr_t tag)
{
	struct action_run *run = NULL;
	char buf[4096];
	size_t drained, keep;
	ssize_t n;

	for (size_t i = 0; i < nitems(action_runs); i++) {
		if (action_runs[i].tag == tag && action_runs[i].fd == fd) {
			run = &action_runs[i];
			break;
		}
	}
	if (run == NULL)
		return;

	/* Drain excess output too, with bounded work per readiness event. */
	for (drained = 0; drained < ACTION_READ_MAX; drained += (size_t)n) {
		n = read(fd, buf, sizeof(buf));
		if (n == -1) {
			if (errno != EAGAIN && errno != EINTR)
				action_finish(run, -1, true);
			return;
		}
		if (n == 0) {
			close(fd);
			run->fd = -1;
			action_complete_if_ready(run);
			return;
		}
		keep = MIN((size_t)n, PROWL_ACTION_OUT_MAX - run->len);
		memcpy(run->out + run->len, buf, keep);
		run->len += keep;
		run->out[run->len] = '\0';
	}
}

/* Expire pending actions and shorten the next kevent wait as needed. */
void
supervisor_action_timeout(struct timespec *timeout)
{
	struct timespec now, remaining;
	bool have_time;

	have_time = clock_gettime(CLOCK_MONOTONIC, &now) == 0;
	for (size_t i = 0; i < nitems(action_runs); i++) {
		struct action_run *run = &action_runs[i];

		if (run->tag == 0)
			continue;
		if (!have_time)
			action_finish(run, -1, true);
		else if (timespeccmp(&now, &run->deadline, >=))
			action_finish(run, 124, true);
		else {
			timespecsub(&run->deadline, &now, &remaining);
			if (timespeccmp(&remaining, timeout, <))
				*timeout = remaining;
		}
	}
}

void
supervisor_cancel_actions(void)
{
	for (size_t i = 0; i < nitems(action_runs); i++) {
		if (action_runs[i].tag != 0)
			action_finish(&action_runs[i], -1, false);
	}
}

static bool
action_reap(pid_t pid, int status)
{
	for (size_t i = 0; i < nitems(action_runs); i++) {
		struct action_run *run = &action_runs[i];

		if (run->pid != pid)
			continue;
		run->pid = 0;
		if (run->tag != 0) {
			run->code = WIFEXITED(status) ? WEXITSTATUS(status) :
			    128 + WTERMSIG(status);
			action_complete_if_ready(run);
		}
		return (true);
	}
	return (false);
}

/*
 * Run the commands of a list (stop_post or start_post) of a job whose
 * process has ended, one after the other, each as the job's user and with
 * its environment.  A command that fails or takes longer than
 * PROWL_ACTION_TIMEOUT is logged; the rest still run.  Returns false if one
 * failed, unless it is marked with "-".  prowld does nothing else meanwhile.
 */
static bool
supervisor_run_list(job_t *job, int list)
{
	const char *lname = list == PROWL_CMD_POST ? "stop_post" : "start_post";
	bool ok = true;
	const prowl_cmd_t *c;
	struct timespec deadline, now;
	pid_t pid;
	int i, st;

	if (job->cmd_count[list] == 0)
		return (true);
	if (!resolve_job_privileges(job))
		return (false);
	for (i = 0; i < job->cmd_count[list]; i++) {
		bool succeeded = false;

		c = &job->cmds[list][i];
		if (!cmd_enabled(job, c))
			continue;
		if (clock_gettime(CLOCK_MONOTONIC, &deadline) == -1) {
			prowl_log(LOG_ERR, "clock_gettime for %s: %m", job->label);
			return (false);
		}
		deadline.tv_sec += PROWL_ACTION_TIMEOUT;
		pid = fork();
		if (pid == -1) {
			prowl_log(LOG_ERR, "fork %s: %m", job->label);
			return (false);
		}
		if (pid == 0) {
			if (g_kqueue_fd >= 0)
				close(g_kqueue_fd);
			job->run_cmd = c;
			child_setup_and_exec(job);
			/* NOTREACHED */
			_exit(127);
		}

		for (;;) {
			struct timespec ts = { 0, 20000000L };
			pid_t r = waitpid(pid, &st, WNOHANG);

			if (r == pid) {
				succeeded = WIFEXITED(st) && WEXITSTATUS(st) == 0;
				break;
			}
			if (r == -1 && errno != EINTR) {
				prowl_log(LOG_ERR, "waitpid for %s: %m", job->label);
				break;
			}
			if (clock_gettime(CLOCK_MONOTONIC, &now) == -1 ||
			    timespeccmp(&now, &deadline, >=)) {
				prowl_log(LOG_WARNING, "job %s: %s %s "
				    "timed out", job->label, lname,
				    c->argv[prowl_cmd_first(c)]);
				/* The main SIGCHLD handler reaps it on exit. */
				if (kill(pid, SIGKILL) == -1 && errno != ESRCH)
					prowl_log(LOG_ERR, "kill %s: %m", job->label);
				break;
			}
			nanosleep(&ts, NULL);
		}
		if (c->argv[prowl_cmd_first(c)][0] != '-' && !succeeded) {
			prowl_log(LOG_WARNING, "job %s: %s %s failed",
			    job->label, lname, c->argv[prowl_cmd_first(c)]);
			ok = false;
		}
	}
	return (ok);
}

/*
 * Stop a job.  Sends SIGTERM and arms the stop-timeout timer.
 * If force is true, sends SIGKILL directly.
 */
int
supervisor_stop(job_t *job, bool force)
{
	/*
	 * An rc-shim whose start has finished has no process of its own:
	 * the script daemonized the service and exited.  Stopping it means
	 * running the script's "stop", as rc.d scripts expect.
	 */
	if (job->pid <= 0 && job->state == JOB_STATE_COMPLETED &&
	    (job->type == JOB_TYPE_RCSHIM || job->stop_argc > 0))
		return (supervisor_rc_stop(job));

	/* Stopping a job that is waiting to be restarted cancels the restart */
	if (job->pid <= 0 && job->restart_hold) {
		struct kevent kev;

		if (job->throttle_timer_active) {
			EV_SET(&kev, timer_ident(job, TIMER_THROTTLE),
			    EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
			(void)kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL);
			job->throttle_timer_active = false;
		}
		job->restart_hold = false;
		job_set_state(job, JOB_STATE_STOPPED);
		return (0);
	}

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
	supervisor_mdns_deregister(job);

	if (job->launch_fd >= 0) {
		/* Keep the read side for a handoff already in flight. */
		job->launch_stop_signal = force ? SIGKILL :
		    (job->stop_signal > 0 ? job->stop_signal : SIGTERM);
		(void)shutdown(job->launch_fd, SHUT_WR);
		arm_stop_timer(job);
	} else if (force) {
		supervisor_signal(job, SIGKILL);
	} else {
		supervisor_signal(job, job->stop_signal > 0 ? job->stop_signal :
		    SIGTERM);
		arm_stop_timer(job);
	}

	return (0);
}

int
supervisor_mask(job_t *job)
{
	int err;

	job_set_masked(job, true);
	job->restart_after_stop = false;
	cancel_notify_timeout_timer(job);
	notify_socket_close(job);
	socket_close_all(job);
	/*
	 * A start wrapper may already have launched its daemon.  Stop the
	 * wrapper first, then run the stop command when it has been reaped.
	 */
	if (job->start_pending && !job->rc_stopping &&
	    (job->type == JOB_TYPE_RCSHIM ||
	    (job->type == JOB_TYPE_ONESHOT && job->stop_argc > 0)))
		job->stop_after_start = true;
	err = supervisor_stop(job, false);

	/* An idle listener has no service process to stop. */
	if (job->pid <= 0 && job->state == JOB_STATE_RUNNING)
		job_set_state(job, JOB_STATE_STOPPED);
	return (err);
}

void
supervisor_unmask(job_t *job)
{
	if (!job->masked)
		return;

	/* Keep supervising any process that is still stopping. */
	if (job->state == JOB_STATE_STOPPING)
		job->restart_after_stop = job->enabled && job->run_at_load;
	else
		job_set_state(job, job->enabled ? JOB_STATE_LOADED :
		    JOB_STATE_DISABLED);
	job_set_masked(job, false);
}

static void
load_masks_from(const char *dir)
{
	struct stat sb;
	job_t *job;
	int fd;

	fd = trusted_open_dir(dir);
	if (fd == -1) {
		if (errno != ENOENT)
			prowl_log(LOG_WARNING,
			    "cannot open mask directory %s: %m", dir);
		return;
	}
	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (job->masked)
			continue;
		if (fstatat(fd, job->label, &sb, AT_SYMLINK_NOFOLLOW) == 0)
			(void)supervisor_mask(job);
		else if (errno != ENOENT)
			prowl_log(LOG_WARNING, "cannot read mask %s/%s: %m",
			    dir, job->label);
	}
	close(fd);
}

void
supervisor_load_masks(void)
{
	load_masks_from(g_mask_dir);
	if (g_legacy_mask_dir != NULL)
		load_masks_from(g_legacy_mask_dir);
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

	if (job->masked || job->state == JOB_STATE_STOPPING)
		return;

	if (!resolve_job_privileges(job)) {
		/* A pending connection would immediately retrigger a rearm. */
		job_set_state(job, JOB_STATE_FAILED);
		return;
	}

	prowl_log(LOG_NOTICE, "socket-activating %s", job->label);
	job->started_at = time(NULL);
	job_set_state(job, JOB_STATE_STARTING);

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
	supervisor_mdns_register(job);
	watch_child(pid, job);

	prowl_log(LOG_INFO, "job %s socket-activated, pid %d",
	    job->label, (int)pid);
}

/*
 * kinfo_proc reports start time relative to the wall clock.  Subtract the
 * kernel's boot time so setting the clock cannot invalidate an adopted PID.
 * Repeat the snapshot if the clock stepped while reading the process.
 */
static bool
child_start_time(pid_t pid, struct timeval *start)
{
	int mib[] = { CTL_KERN, KERN_BOOTTIME };
	struct timeval before, after;
	struct kinfo_proc *kp;
	size_t len;

	for (int i = 0; i < 2; i++) {
		len = sizeof(before);
		if (sysctl(mib, nitems(mib), &before, &len, NULL, 0) == -1)
			return (false);
		kp = kinfo_getproc(pid);
		if (kp == NULL)
			return (false);
		if (kp->ki_ppid != getpid() || kp->ki_stat == SZOMB ||
		    (kp->ki_flag & (P_WEXIT | P_TRACED)) != 0) {
			free(kp);
			errno = ESRCH;
			return (false);
		}
		*start = kp->ki_start;
		free(kp);
		if (sysctl(mib, nitems(mib), &after, &len, NULL, 0) == -1)
			return (false);
		if (timercmp(&before, &after, ==)) {
			timersub(start, &before, start);
			return (true);
		}
	}
	errno = EAGAIN;
	return (false);
}

int
supervisor_signal(job_t *job, int sig)
{
	struct timeval start;

	if (job->pid <= 0) {
		errno = ESRCH;
		return (-1);
	}
	if (job->adopted) {
		if (!child_start_time(job->pid, &start))
			return (-1);
		if (timercmp(&start, &job->adopted_start, !=)) {
			errno = ESRCH;
			return (-1);
		}
	}
	return (kill(job->pid, sig));
}

void
supervisor_handle_periodic(job_t *job)
{
	if (job->masked || job->state == JOB_STATE_STOPPING)
		return;

	prowl_log(LOG_INFO, "timer %s: periodic interval reached", job->label);
	job->last_run = time(NULL);
	timer_state_save();
	supervisor_start(job);
}

void
supervisor_handle_boot_delay(job_t *job)
{
	if (job->masked || job->state == JOB_STATE_STOPPING)
		return;

	prowl_log(LOG_INFO, "timer %s: boot delay reached", job->label);
	job->last_run = time(NULL);
	timer_state_save();
	supervisor_start(job);
}

static void
supervisor_mdns_register(job_t *job)
{
	char hostname[256];
	char name[PROWL_LABEL_MAX];
	char portstr[16];
	char *args[PROWL_ARGS_MAX];
	int i, argc;
	pid_t pid;

#define MDNS_MIN_RESTART_SEC	10
	if (!g_dnssd_available || !job->mdns.register_service || job->mdns_pid > 0)
		return;

	{
		time_t now = time(NULL);
		if (job->mdns_last_register > 0 &&
		    now - job->mdns_last_register < MDNS_MIN_RESTART_SEC) {
			prowl_log(LOG_DEBUG,
			    "mdns %s: rate-limited, skipping re-register",
			    job->label);
			return;
		}
		job->mdns_last_register = now;
	}

	if (gethostname(hostname, sizeof(hostname)) == -1)
		strlcpy(hostname, "midnightbsd", sizeof(hostname));
	else {
		char *dot = strchr(hostname, '.');
		if (dot) *dot = '\0';
	}

	/* Substitute %h and %l */
	const char *src = job->mdns.name[0] != '\0' ? job->mdns.name : "%l";
	char *dst = name;
	while (*src && (dst - name < PROWL_LABEL_MAX - 1)) {
		if (*src == '%' && *(src+1) == 'h') {
			size_t len = strlen(hostname);
			if (dst - name + len < PROWL_LABEL_MAX - 1) {
				memcpy(dst, hostname, len);
				dst += len;
				src += 2;
			} else break;
		} else if (*src == '%' && *(src+1) == 'l') {
			size_t len = strlen(job->label);
			if (dst - name + len < PROWL_LABEL_MAX - 1) {
				memcpy(dst, job->label, len);
				dst += len;
				src += 2;
			} else break;
		} else {
			*dst++ = *src++;
		}
	}
	*dst = '\0';

	int port = job->mdns.port;
	if (port <= 0 && job->sockets_count > 0)
		port = job->sockets[0].port;

	if (port <= 0) {
		prowl_log(LOG_WARNING, "mdns %s: no port specified and no sockets",
		    job->label);
		return;
	}

	snprintf(portstr, sizeof(portstr), "%d", port);

	argc = 0;
	args[argc++] = g_dnssd_path;
	args[argc++] = (char *)(uintptr_t)"-R";
	args[argc++] = name;
	args[argc++] = job->mdns.type[0] != '\0' ? job->mdns.type : (char *)(uintptr_t)"_http._tcp";
	args[argc++] = job->mdns.domain;
	args[argc++] = portstr;
	for (i = 0; i < job->mdns.txt_count && argc < PROWL_ARGS_MAX - 1; i++)
		args[argc++] = job->mdns.txt_record[i];
	args[argc] = NULL;

	pid = fork();
	if (pid == -1) {
		prowl_log(LOG_ERR, "mdns %s fork: %m", job->label);
		return;
	}

	if (pid == 0) {
		/* Child */
		if (g_kqueue_fd >= 0) close(g_kqueue_fd);
		prowl_reset_signals();
		/* Redirect output to null to avoid cluttering logs. */
		if (!g_is_init)
			child_redirect_fd(job, STDIN_FILENO, _PATH_DEVNULL,
			    O_RDONLY);
		child_redirect_fd(job, STDOUT_FILENO, _PATH_DEVNULL, O_WRONLY);
		child_redirect_fd(job, STDERR_FILENO, _PATH_DEVNULL, O_WRONLY);
		execve(args[0], args, (char *const *)(uintptr_t)minimal_env);
		_exit(127);
	}

	/* Parent */
	job->mdns_pid = pid;
	watch_child(pid, job);
	prowl_log(LOG_INFO, "mdns %s registered as '%s' (pid %d)",
	    job->label, name, (int)pid);
}

static void
supervisor_mdns_deregister(job_t *job)
{
	if (job->mdns_pid <= 0)
		return;

	prowl_log(LOG_DEBUG, "mdns %s deregistering (pid %d)",
	    job->label, (int)job->mdns_pid);
	kill(job->mdns_pid, SIGTERM);
	job->mdns_pid = -1;
}

/*
 * Called when the throttle timer fires.  If the process is still alive,
 * transition it to RUNNING.
 */
void
supervisor_handle_throttle(job_t *job)
{
	job->throttle_timer_active = false;

	if (job->masked)
		return;

	/* Restart hold-off over: the job may be scheduled again. */
	if (job->restart_hold) {
		job->restart_hold = false;
		dag_schedule_ready();
		return;
	}

	if (job->state != JOB_STATE_STARTING)
		return;

	/* The launcher itself is not a daemon that is up */
	if (job->launcher)
		return;

	/* Still running its start_pre commands: not up yet */
	if (job->cmd_count[PROWL_CMD_PRE] > 0 && job->pid > 0 &&
	    not_exec_yet(job->pid)) {
		arm_throttle_timer_ms(job, PROWL_ADOPT_MS);
		return;
	}

	if (job->pid > 0 && supervisor_signal(job, 0) == 0) {
		job_set_state(job, JOB_STATE_RUNNING);
		supervisor_mdns_register(job);
		release_start_slot(job);
		prowl_log(LOG_INFO, "job %s running (pid %d)",
		    job->label, (int)job->pid);
		dag_schedule_ready();
	}
}

/*
 * True while pid is still the copy of prowld that fork() made, that is,
 * before it has exec'd the job.
 */
static bool
not_exec_yet(pid_t pid)
{
	struct kinfo_proc *a, *b;
	bool r = false;

	a = kinfo_getproc(pid);
	b = kinfo_getproc(getpid());
	if (a != NULL && b != NULL)
		r = strcmp(a->ki_comm, b->ki_comm) == 0;
	free(a);
	free(b);
	return (r);
}

/*
 * Called when the stop-timeout timer fires.  Force-kill the process.
 */
void
supervisor_handle_stop_timeout(job_t *job, uintptr_t generation)
{
	if (!job->stop_timer_active ||
	    job->stop_timer_generation != generation)
		return;
	job->stop_timer_active = false;

	if (job->state != JOB_STATE_STOPPING)
		return;

	if (job->pid > 0) {
		prowl_log(LOG_WARNING,
		    "job %s (pid %d) timed out; sending SIGKILL",
		    job->label, (int)job->pid);
		supervisor_signal(job, SIGKILL);
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
	if (job->masked || job->state != JOB_STATE_STARTING)
		return;

	prowl_log(LOG_WARNING,
	    "job %s (pid %d): no READY=1 within timeout; killing",
	    job->label, (int)job->pid);

	notify_socket_close(job);

	if (job->launch_fd >= 0) {
		job->launch_stop_signal = SIGKILL;
		(void)shutdown(job->launch_fd, SHUT_WR);
	} else if (job->pid > 0)
		(void)supervisor_signal(job, SIGKILL);
}

/*
 * Called when the watchdog timer fires (recurring every watchdog_sec seconds).
 * If the last keepalive ping was too long ago, treat the service as hung.
 */
void
supervisor_handle_watchdog(job_t *job)
{
	time_t now = time(NULL);

	if (job->masked || job->state != JOB_STATE_RUNNING)
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
	expected_uid = job->notify_uid;
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
	union {
		struct cmsghdr align;
		char bytes[CMSG_SPACE(SOCKCRED2SIZE(CMGROUP_MAX)) +
		    CMSG_SPACE(sizeof(int) * 16)];
	} control;
	struct msghdr msg;
	struct iovec iov;
	struct cmsghdr *cmsg;
	struct sockcred2 *cred;
	char buf[4096];
	ssize_t n;
	char *p, *end, *eq, *nl;
	uid_t expected_uid;
	bool authenticated, rights;
	size_t count, i;
	int received_fd;

	if (job->masked || job->notify_fd < 0)
		return;
	memset(&msg, 0, sizeof(msg));
	iov.iov_base = buf;
	iov.iov_len = sizeof(buf) - 1;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control.bytes;
	msg.msg_controllen = sizeof(control.bytes);
	n = recvmsg(job->notify_fd, &msg, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
	if (n == -1)
		return;

	expected_uid = job->notify_uid;
	authenticated = false;
	rights = false;
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_len < CMSG_LEN(0) ||
		    (char *)cmsg + cmsg->cmsg_len >
		    (char *)msg.msg_control + msg.msg_controllen)
			break;
		if (cmsg->cmsg_level != SOL_SOCKET)
			continue;
		if (cmsg->cmsg_type == SCM_RIGHTS) {
			rights = true;
			count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
			for (i = 0; i < count; i++) {
				memcpy(&received_fd, CMSG_DATA(cmsg) +
				    i * sizeof(int), sizeof(received_fd));
				close(received_fd);
			}
		} else if (cmsg->cmsg_type == SCM_CREDS2 &&
		    cmsg->cmsg_len >= CMSG_LEN(sizeof(struct sockcred2))) {
			cred = (struct sockcred2 *)(void *)CMSG_DATA(cmsg);
			if (cred->sc_version == 0 &&
			    cred->sc_euid == expected_uid)
				authenticated = true;
		}
	}
	if (n == 0 || !authenticated || rights ||
	    (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) != 0)
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

			if (strcmp(key, "READY") == 0 && strcmp(val, "1") == 0) {
				if (job->launcher && job->state == JOB_STATE_STARTING) {
					job->launch_ready = true;
					cancel_notify_timeout_timer(job);
				} else if (job->state == JOB_STATE_STARTING) {
					cancel_notify_timeout_timer(job);
					job_set_state(job, JOB_STATE_RUNNING);
					supervisor_mdns_register(job);
					release_start_slot(job);

					prowl_log(LOG_INFO,
					    "job %s ready (READY=1)",
					    job->label);
					dag_schedule_ready();
				}
			} else if (strcmp(key, "STATUS") == 0) {
				strlcpy(job->status_msg, val,
				    sizeof(job->status_msg));
			} else if (!job->launcher && !job->adopted &&
			    strcmp(key, "MAINPID") == 0) {
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
#define EXTEND_TIMEOUT_MAX_USEC	(300LL * 1000000LL)
				long long usec = strtoll(val, NULL, 10);
				if (usec > EXTEND_TIMEOUT_MAX_USEC)
					usec = EXTEND_TIMEOUT_MAX_USEC;
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
 * A job ended when it should not have: restart it after a back-off if its
 * keep_alive policy says so, otherwise leave it failed.
 */
static void
job_restart_or_fail(job_t *job, int status)
{
	if (should_restart(job, status)) {
		int base, delay, shift;

		job->restart_count++;

		/*
		 * Hold the job off for throttle_interval seconds before it
		 * is eligible again.  A job that keeps dying within that
		 * interval backs off exponentially, up to 16 times the
		 * interval, so a broken service cannot respawn in a tight
		 * loop.  A run that lasted longer resets the backoff.
		 */
		base = job->throttle_interval > 0 ?
		    job->throttle_interval : DEFAULT_THROTTLE_INT;
		if (job->exited_at - job->started_at < base) {
			if (job->quick_fails < 4)
				job->quick_fails++;
		} else
			job->quick_fails = 0;
		shift = job->quick_fails > 0 ? job->quick_fails - 1 : 0;
		delay = base << shift;

		prowl_log(LOG_NOTICE,
		    "job %s will restart in %ds (attempt %d)",
		    job->label, delay, job->restart_count);
		job_set_state(job, JOB_STATE_LOADED);
		job->restart_hold = true;
		arm_throttle_timer(job, delay);
	} else {
		job_set_state(job, JOB_STATE_FAILED);
		prowl_log(LOG_ERR, "job %s entered failed state",
		    job->label);
	}
}

/*
 * libutil writes the decimal PID without a newline.  Other writers such
 * as sendmail include metadata after the first line.
 */
static pid_t
pid_from_file(const char *path)
{
	struct stat sb;
	char buf[32], *p, *end;
	size_t len;
	ssize_t n;
	long v;
	int fd, pathfd;

	/* Inspect the vnode without invoking a FIFO or device open routine. */
	pathfd = open(path, O_PATH | O_NOFOLLOW | O_CLOEXEC);
	if (pathfd == -1)
		return (-1);
	fd = -1;
	if (fstat(pathfd, &sb) == 0 && S_ISREG(sb.st_mode))
		fd = openat(pathfd, "", O_EMPTY_PATH | O_RDONLY | O_NONBLOCK |
		    O_CLOEXEC);
	close(pathfd);
	if (fd == -1)
		return (-1);
	len = 0;
	while (len < sizeof(buf)) {
		n = read(fd, buf + len, sizeof(buf) - len);
		if (n <= 0) {
			if (n == -1 && errno == EINTR)
				continue;
			if (n == -1)
				len = 0;
			break;
		}
		end = memchr(buf + len, '\n', (size_t)n);
		if (end != NULL) {
			len = (size_t)(end - buf);
			break;
		}
		len += (size_t)n;
	}
	(void)close(fd);
	if (len == 0 || len == sizeof(buf) || memchr(buf, '\0', len) != NULL)
		return (-1);
	buf[len] = '\0';

	p = buf;
	while (isspace((unsigned char)*p))
		p++;
	if (!isdigit((unsigned char)*p))
		return (-1);
	errno = 0;
	v = strtol(p, &end, 10);
	while (isspace((unsigned char)*end))
		end++;
	if (errno != 0 || v <= 1 || v > INT_MAX || *end != '\0')
		return (-1);
	if (kill((pid_t)v, 0) == -1 && errno != EPERM)
		return (-1);
	return ((pid_t)v);
}

/*
 * The daemon an rc-shim started: the process in the pidfile its rc.d script
 * names, if it is there.  Returns -1 if there is none.
 */
pid_t
supervisor_shim_pid(const job_t *job)
{
	if (job->type != JOB_TYPE_RCSHIM || job->shim_pidfile[0] == '\0')
		return (-1);
	return (pid_from_file(job->shim_pidfile));
}

struct launch_result {
	pid_t pid;
	struct timeval start;
};

/* Only this launch can leave children with the private reaper. */
static bool
find_daemon(const job_t *job, struct launch_result *result)
{
	struct kinfo_proc *kp;
	pid_t pid, self;
	int i, count;

	if (job->pid_file[0] != '\0') {
		pid = pid_from_file(job->pid_file);
		if (pid < 0)
			return (false);
		kp = kinfo_getproc(pid);
		count = 1;
	} else
		kp = kinfo_getallproc(&count);
	if (kp == NULL)
		return (false);

	result->pid = 0;
	self = getpid();
	for (i = 0; i < count; i++) {
		if (kp[i].ki_ppid != self || kp[i].ki_stat == SZOMB ||
		    (kp[i].ki_flag & (P_WEXIT | P_TRACED)) != 0 ||
		    (job->pid_file[0] == '\0' &&
		    strcmp(kp[i].ki_comm, job->pid_name) != 0))
			continue;
		if (result->pid == 0 ||
		    timercmp(&kp[i].ki_start, &result->start, <)) {
			result->pid = kp[i].ki_pid;
			result->start = kp[i].ki_start;
		}
	}
	free(kp);
	return (result->pid > 0 &&
	    child_start_time(result->pid, &result->start));
}

/*
 * Put a temporary reaper between prowld and the command.  A daemon can
 * setsid, change credentials, or fork twice and still belongs to this launch.
 * Only the trusted reaper holds the reporting socket.  On cancellation or
 * failure it cleans up its own descendants before exiting.
 */
static pid_t
fork_launcher(job_t *job)
{
	struct launch_result result = { 0 };
	struct procctl_reaper_kill rk = { .rk_sig = SIGKILL };
	struct pollfd control;
	int pair[2], status, tries, fd, error, n;
	pid_t reaper, child, waited;

	if (!g_reaper) {
		errno = EOPNOTSUPP;
		return (-1);
	}
	if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair) == -1)
		return (-1);
	reaper = fork();
	if (reaper != 0) {
		error = errno;
		close(pair[1]);
		if (reaper > 0)
			job->launch_fd = pair[0];
		else {
			close(pair[0]);
			errno = error;
		}
		return (reaper);
	}

	close(pair[0]);
	if (procctl(P_PID, getpid(), PROC_REAP_ACQUIRE, NULL) == -1) {
		prowl_log(LOG_ERR, "cannot acquire reaper for %s: %m", job->label);
		_exit(1);
	}
	child = fork();
	if (child == 0) {
		close(pair[1]);
		return (0);
	}
	if (child == -1) {
		prowl_log(LOG_ERR, "cannot fork launcher for %s: %m", job->label);
		_exit(1);
	}
	/* Keep only the control socket and the supervisor's standard streams. */
	error = 1;
	fd = STDERR_FILENO + 1;
	if (dup2(pair[1], fd) == -1)
		goto cancel;
	closefrom(fd + 1);
	control.fd = fd;
	control.events = POLLIN;
	setproctitle("adopting %s", job->label);
	tries = 0;
	for (;;) {
		if (child > 0) {
			waited = waitpid(child, &status, WNOHANG);
			if (waited == -1 && errno != EINTR)
				goto cancel;
			if (waited == child) {
				if (!WIFEXITED(status))
					goto cancel;
				if (WEXITSTATUS(status) != 0) {
					error = WEXITSTATUS(status);
					goto cancel;
				}
				child = -1;
			}
		}
		if (child < 0) {
			if (find_daemon(job, &result)) {
				if (send(fd, &result, sizeof(result), MSG_NOSIGNAL) ==
				    (ssize_t)sizeof(result))
					_exit(0);
				goto cancel;
			}
			if (++tries >= PROWL_ADOPT_TRIES)
				goto cancel;
		}
		/* Shutdown of the parent's write side cancels a pending launch. */
		n = poll(&control, 1, PROWL_ADOPT_MS);
		if (n > 0 || (n == -1 && errno != EINTR))
			goto cancel;
	}
cancel:
	if (procctl(P_PID, getpid(), PROC_REAP_KILL, &rk) == -1 &&
	    errno != ESRCH)
		prowl_log(LOG_ERR, "cannot clean up launch of %s: %m", job->label);
	_exit(error);
}

static bool
adopt_daemon(job_t *job, const struct launch_result *result)
{
	struct timeval start;

	if (result->pid <= 1 || !child_start_time(result->pid, &start) ||
	    timercmp(&result->start, &start, !=))
		return (false);
	job->pid = result->pid;
	job->adopted = true;
	job->adopted_start = result->start;
	watch_child(job->pid, job);
	prowl_log(LOG_INFO, "job %s daemon is pid %d", job->label, (int)job->pid);
	return (true);
}

/*
 * Reap a child process and update job state.
 */
void
supervisor_reap(pid_t pid, int status)
{
	job_t *job;
	struct kevent kev;
	bool was_launcher, handoff;
	struct launch_result result;

	if (ipc_sysrc_reap(pid, status) || action_reap(pid, status))
		return;

	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (job->pid == pid || job->mdns_pid == pid)
			break;
	}

	if (job == NULL) {
		/*
		 * Orphans reparented to us end up here all the time; keep
		 * them in syslog at debug level, not on the console.
		 */
		syslog(LOG_DEBUG, "reap: unknown pid %d", (int)pid);
		return;
	}

	if (pid == job->mdns_pid) {
		if (WIFEXITED(status))
			prowl_log(LOG_NOTICE, "mdns %s (pid %d) exited with code %d",
			    job->label, (int)pid, WEXITSTATUS(status));
		else if (WIFSIGNALED(status))
			prowl_log(LOG_NOTICE, "mdns %s (pid %d) killed by signal %d",
			    job->label, (int)pid, WTERMSIG(status));
		job->mdns_pid = -1;
		
		/* Restart mdns if job is still running */
		if (!job->masked && job->state == JOB_STATE_RUNNING)
			supervisor_mdns_register(job);
		return;
	}

	was_launcher = job->launcher;
	handoff = false;
	if (job->launch_fd >= 0) {
		handoff = WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
		    recv(job->launch_fd, &result, sizeof(result), MSG_DONTWAIT) ==
		    (ssize_t)sizeof(result);
		close(job->launch_fd);
		job->launch_fd = -1;
	}
	job->launcher = false;
	job->adopted = false;
	/* A successful helper exit transfers the running job and its timers. */
	if (handoff && adopt_daemon(job, &result)) {
		if (job->launch_stop_signal != 0 || job->masked || g_shutdown) {
			(void)supervisor_signal(job, job->launch_stop_signal != 0 ?
			    job->launch_stop_signal : SIGTERM);
			if (job->state == JOB_STATE_STOPPING)
				arm_stop_timer(job);
		} else if (job->notify_fd < 0 || job->launch_ready)
			arm_throttle_timer(job, PROWL_READY_DELAY);
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
	}

	cancel_stop_timer(job);
	cancel_watchdog_timer(job);
	cancel_notify_timeout_timer(job);
	notify_socket_close(job);

	if (WIFEXITED(status))
		prowl_log(LOG_NOTICE, "job %s exited with code %d",
		    job->label, WEXITSTATUS(status));
	else if (WIFSIGNALED(status))
		prowl_log(LOG_NOTICE, "job %s killed by signal %d",
		    job->label, WTERMSIG(status));

	if (was_launcher && job->state == JOB_STATE_STARTING) {
		prowl_log(LOG_ERR, "job %s: could not adopt daemon", job->label);
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
			status = 1 << 8;
		job->last_exit_status = status;
	}
	release_start_slot(job);

	if (job->stop_after_start) {
		job->stop_after_start = false;
		if (supervisor_rc_stop(job) == 0)
			return;
		job->restart_after_stop = false;
		job_set_state(job, JOB_STATE_FAILED);
		dag_schedule_ready();
		return;
	}

	/* The job has ended: clean up after it */
	if (job->cmd_count[PROWL_CMD_POST] > 0 && !was_launcher &&
	    job->type != JOB_TYPE_RCSHIM &&
	    (job->type != JOB_TYPE_ONESHOT || job->rc_stopping))
		(void)supervisor_run_list(job, PROWL_CMD_POST);

	if (job->masked) {
		job->rc_stopping = false;
		job->restart_after_stop = false;
		job_set_state(job, JOB_STATE_STOPPED);
		dag_schedule_ready();
		return;
	}

	/*
	 * A oneshot that ended well is not done until its start_post
	 * commands have run; if one of them fails the job has failed.
	 */
	if (job->state != JOB_STATE_STOPPING && !job->rc_stopping &&
	    job->type == JOB_TYPE_ONESHOT &&
	    job->cmd_count[PROWL_CMD_START_POST] > 0 &&
	    WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
	    !supervisor_run_list(job, PROWL_CMD_START_POST))
		status = 1 << 8;

	/*
	 * Oneshot completes on clean exit.  So does an rc-shim start: an
	 * rc.d script runs "start" to completion (daemonizing the service
	 * if it has one), so a clean exit means dependents may proceed.
	 */
	if (job->state != JOB_STATE_STOPPING && !job->rc_stopping &&
	    (job->type == JOB_TYPE_ONESHOT ||
	    (job->type == JOB_TYPE_RCSHIM &&
	    job->state == JOB_STATE_STARTING)) &&
	    WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		job_set_state(job, JOB_STATE_COMPLETED);
		dag_schedule_ready();
		return;
	}

	/* Normal stop requested */
	if (job->state == JOB_STATE_STOPPING) {
		job->rc_stopping = false;
		if (job->restart_after_stop) {
			job->restart_after_stop = false;
			job_set_state(job, JOB_STATE_LOADED);
			job->run_at_load = true;
			dag_schedule_ready();
			return;
		}
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

	/* rc.shutdown is stopping services behind our back: let it */
	if (g_rundown) {
		job_set_state(job, JOB_STATE_STOPPED);
		return;
	}

	/* Unexpected exit */
	job_restart_or_fail(job, status);
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
			if ((job->launch_fd >= 0 || job->shutdown_wait ||
			    job->cmd_count[PROWL_CMD_POST] > 0) &&
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
			supervisor_signal(job, SIGKILL);
	}

	/*
	 * Final reap.  A killed process takes a moment to die: wait for each,
	 * so that none is left for init when we exit.
	 */
	TAILQ_FOREACH(job, &g_jobs, entries) {
		int st, tries;

		if (job->pid <= 0)
			continue;
		for (tries = 0; tries < 50; tries++) {
			struct timespec ts = { 0, 20000000L };

			pid_t waited = waitpid(job->pid, &st, WNOHANG);
			if (waited > 0 && job->launch_fd >= 0) {
				supervisor_reap(waited, st);
				if (job->adopted) {
					(void)supervisor_signal(job, SIGKILL);
					continue;
				}
			}
			if (waited != 0)
				break;
			nanosleep(&ts, NULL);
		}
	}
	for (;;) {
		int st;
		if (waitpid(-1, &st, WNOHANG) <= 0)
			break;
	}
}
