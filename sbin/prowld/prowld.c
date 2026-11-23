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
 * prowld(8) - MidnightBSD Service Management Daemon
 *
 * Entry point, global state, and main event loop.
 */

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/event.h>
#include <sys/procctl.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <ucl.h>

#include "prowld.h"

/* Global state */
struct job_list		 g_jobs = TAILQ_HEAD_INITIALIZER(g_jobs);
prowld_config_t		 g_config;
int			 g_kqueue_fd = -1;
volatile bool		 g_shutdown = false;
volatile bool		 g_reload = false;
int			 g_current_starts = 0;

static int		 g_ipc_listen_fd = -1;
/* run dir, pidfile and IPC are set up */
static bool		 g_runtime_done = false;
static bool		 g_single_user = false;	/* PID 1: go to single-user */
static int		 g_howto = RB_AUTOBOOT;	/* PID 1: reboot(2) argument */
bool			 g_is_init = false;	/* running as PID 1 */
bool			 g_jailed = false;	/* running inside a jail */
bool			 g_reaper = false;	/* orphaned descendants are ours */
bool			 g_rundown = false;	/* rc.shutdown is running */
bool			 g_fastboot = false;	/* no "autoboot": skip fsck */
char			 g_unit_override[PROWL_PATH_MAX]; /* -u unit dir */
static bool		 g_reroot = false;	/* PID 1: reroot requested */
bool			 g_boot_mode = false;	/* we run rc.d */

/* Runtime path globals — defaults match compile-time macros */
char	g_run_dir[PROWL_PATH_MAX]      = PROWLD_RUN_DIR;
char	g_notify_dir[PROWL_PATH_MAX]   = PROWLD_NOTIFY_DIR;
char	g_sock_path[PROWL_PATH_MAX]    = PROWLD_SOCK_PATH;
char	g_pid_path[PROWL_PATH_MAX]     = PROWLD_PID_PATH;
char	g_db_dir[PROWL_PATH_MAX]       = PROWLD_DB_DIR;
char	g_mask_dir[PROWL_PATH_MAX]     = PROWLD_MASK_DIR;
char	g_log_dir[PROWL_PATH_MAX]      = PROWLD_LOG_DIR;
char	g_job_log_dir[PROWL_PATH_MAX]  = PROWLD_JOB_LOG_DIR;
char	g_generated_dir[PROWL_PATH_MAX] = UNIT_DIR_GENERATED;

static void
usage(void)
{
	fprintf(stderr, "usage: prowld [-d] [-s rundir] [-u unitdir]\n");
	exit(1);
}

/*
 * Rebuild all runtime paths from a single base directory.
 * Called when -s is given or when auto-detection falls back to /tmp/prowld.
 */
static void
setup_runtime_paths(const char *base)
{
	int n;

	if (strlcpy(g_run_dir, base, sizeof(g_run_dir)) >= sizeof(g_run_dir))
		goto toolong;

	n = snprintf(g_notify_dir, sizeof(g_notify_dir), "%s/notify", base);
	if (n < 0 || (size_t)n >= sizeof(g_notify_dir))
		goto toolong;

	n = snprintf(g_sock_path, sizeof(g_sock_path), "%s/prowld.sock", base);
	if (n < 0 || (size_t)n >= sizeof(g_sock_path))
		goto toolong;

	n = snprintf(g_pid_path, sizeof(g_pid_path), "%s/prowld.pid", base);
	if (n < 0 || (size_t)n >= sizeof(g_pid_path))
		goto toolong;

	n = snprintf(g_db_dir, sizeof(g_db_dir), "%s/db", base);
	if (n < 0 || (size_t)n >= sizeof(g_db_dir))
		goto toolong;

	n = snprintf(g_mask_dir, sizeof(g_mask_dir), "%s/db/masked.d", base);
	if (n < 0 || (size_t)n >= sizeof(g_mask_dir))
		goto toolong;

	n = snprintf(g_log_dir, sizeof(g_log_dir), "%s/log", base);
	if (n < 0 || (size_t)n >= sizeof(g_log_dir))
		goto toolong;

	n = snprintf(g_job_log_dir, sizeof(g_job_log_dir), "%s/log/jobs", base);
	if (n < 0 || (size_t)n >= sizeof(g_job_log_dir))
		goto toolong;

	n = snprintf(g_generated_dir, sizeof(g_generated_dir), "%s/generated.d",
	    base);
	if (n < 0 || (size_t)n >= sizeof(g_generated_dir))
		goto toolong;

	prowl_log(LOG_INFO, "runtime paths rooted at %s", base);
	return;

toolong:
	errx(1, "runtime path too long (base: %s)", base);
}

static void
write_pidfile(void)
{
	char pidbuf[32];
	ssize_t n;
	int fd;

	/*
	 * O_NOFOLLOW refuses to open a symlink at g_pid_path, preventing a
	 * pre-placed symlink from redirecting this root write into an
	 * arbitrary file when the run directory is under /tmp.
	 */
	fd = open(g_pid_path,
	    O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0644);
	if (fd == -1) {
		prowl_log(LOG_WARNING, "cannot write pidfile %s: %m",
		    g_pid_path);
		return;
	}
	n = snprintf(pidbuf, sizeof(pidbuf), "%d\n", (int)getpid());
	if (n > 0 && (size_t)n < sizeof(pidbuf))
		(void)write(fd, pidbuf, (size_t)n);
	close(fd);
}

static void
remove_pidfile(void)
{
	unlink(g_pid_path);
}

/*
 * Create path with mode, or verify it is safe if it already exists.
 *
 * When mkdir(2) returns EEXIST we use lstat(2) — not stat(2) — so that a
 * symlink at the target path is treated as an error rather than silently
 * followed.  The existing entry must be:
 *   - a real directory (not a symlink, device, etc.)
 *   - owned by root (uid 0)
 *   - not writable by group or others
 *
 * Without these checks a local user who pre-creates /tmp/prowld keeps
 * ownership of the tree and can place symlinks (prowld.pid, prowld.sock,
 * notify/<label>, …) that redirect root writes to arbitrary paths.
 *
 * Returns 0 on success, -1 on failure (already logged at LOG_ERR).
 */
static int
secure_mkdir(const char *path, mode_t mode)
{
	struct stat sb;

	if (mkdir(path, mode) == 0)
		return (0);		/* freshly created by us (root) */

	if (errno != EEXIST) {
		prowl_log(LOG_WARNING, "mkdir %s: %m", path);
		return (-1);
	}

	/* Pre-existing entry: validate via lstat to detect symlinks */
	if (lstat(path, &sb) == -1) {
		prowl_log(LOG_ERR, "secure_mkdir: lstat %s: %m", path);
		return (-1);
	}
	if (!S_ISDIR(sb.st_mode)) {
		prowl_log(LOG_ERR,
		    "secure_mkdir: %s exists but is not a directory (mode %04o)",
		    path, (unsigned)(sb.st_mode & 0xffff));
		return (-1);
	}
	if (sb.st_uid != 0) {
		prowl_log(LOG_ERR,
		    "secure_mkdir: %s is owned by uid %u, not root — "
		    "possible hijack attack; refusing to use this path",
		    path, (unsigned)sb.st_uid);
		return (-1);
	}
	if (sb.st_mode & (S_IWGRP | S_IWOTH)) {
		prowl_log(LOG_ERR,
		    "secure_mkdir: %s has unsafe permissions %04o — "
		    "refusing to use attacker-writable directory",
		    path, (unsigned)(sb.st_mode & 0777));
		return (-1);
	}
	return (0);
}

static int
ensure_dirs(void)
{
	/*
	 * Try the primary run directory.  If it is unavailable (read-only
	 * media, EROFS, EACCES) fall back to /tmp/prowld so prowld can
	 * operate from a tmpfs before /var is mounted.
	 *
	 * The fallback is only accepted if it is freshly created by prowld
	 * (owned by root, no world-write).  A pre-existing /tmp/prowld that
	 * fails the ownership or permission check is treated as a hijack
	 * attempt and causes an immediate fatal exit.
	 */
	if (secure_mkdir(g_run_dir, 0755) == -1) {
		prowl_log(LOG_WARNING,
		    "run dir %s unavailable, falling back to /tmp/prowld",
		    g_run_dir);
		setup_runtime_paths("/tmp/prowld");
		if (secure_mkdir(g_run_dir, 0755) == -1) {
			prowl_log(LOG_ERR,
			    "cannot safely create or validate run directory "
			    "%s; refusing to start", g_run_dir);
			return (-1);
		}
	}

	/* Subdirectories — failures are logged but not fatal */
	if (secure_mkdir(g_notify_dir, 0711) == 0) {
		int fd = trusted_open_dir(g_notify_dir);

		if (fd == -1)
			prowl_log(LOG_ERR, "unsafe notify directory %s: %m",
			    g_notify_dir);
		else {
			/* Permit traversal but not listing or modification. */
			if (fchmod(fd, 0711) == -1)
				prowl_log(LOG_ERR, "chmod notify directory %s: %m",
				    g_notify_dir);
			close(fd);
		}
	}
	(void)secure_mkdir(g_db_dir,        0755); /* world-readable */
	(void)secure_mkdir(g_mask_dir,      0700); /* root-only: masks */
	(void)secure_mkdir(g_log_dir,       0750); /* daemon logs */
	(void)secure_mkdir(g_job_log_dir,   0750); /* per-job logs */
	(void)secure_mkdir(g_generated_dir, 0755); /* generated units */
	return (0);
}

/*
 * Create the run directory, write the pidfile and open the IPC socket.
 * Under init(8) this must wait until the root file system is writable and
 * the rc.d "var" and "cleanvar" scripts have finished: they would otherwise
 * hide or delete what is created here.  Returns -1 if no run directory
 * could be made.
 */
static int
runtime_up(void)
{
	if (g_runtime_done)
		return (0);
	g_runtime_done = true;

	if (ensure_dirs() == -1)
		return (-1);
	write_pidfile();

	/*
	 * IPC is non-fatal: on read-only or constrained media the socket
	 * directory may not be available.  prowld still manages services;
	 * management commands will be unavailable until the socket is up.
	 */
	if (ipc_init() == -1)
		prowl_log(LOG_WARNING,
		    "IPC socket unavailable; management commands disabled");
	else
		g_ipc_listen_fd = ipc_listen_fd;

	if (g_boot_mode)
		timer_state_load();
	return (0);
}

/*
 * PID 1 only: bring the runtime state up once FILESYSTEMS has run.
 */
static void
check_runtime(void)
{
	job_t *fs;

	if (!g_boot_mode || g_runtime_done)
		return;
	fs = job_find_by_provides("FILESYSTEMS");
	if (fs != NULL && (fs->state == JOB_STATE_LOADED ||
	    fs->state == JOB_STATE_STARTING))
		return;
	if (runtime_up() == -1)
		prowl_log(LOG_ERR, "running without a control socket");
}

static void
load_config(void)
{
	struct ucl_parser *parser;
	ucl_object_t *root;
	const ucl_object_t *obj;
	bool ok;
	int fd;

	/* Apply defaults first; debug is not reset here — it is set by
	 * the command-line -d flag in main() and must not be overridden. */
	g_config.max_concurrent_starts = DEFAULT_MAX_STARTS;
	g_config.shutdown_timeout      = DEFAULT_SHUTDOWN_TMO;

	fd = trusted_open_file(PROWLD_CONF_PATH, false);
	if (fd == -1) {
		if (errno != ENOENT)
			prowl_log(LOG_WARNING, "load_config %s: %m",
			    PROWLD_CONF_PATH);
		return;
	}

	parser = ucl_parser_new(UCL_PARSER_NO_FILEVARS);
	if (parser == NULL) {
		prowl_log(LOG_ERR, "load_config: ucl_parser_new failed");
		close(fd);
		return;
	}

	ok = trusted_ucl_add_fd(parser, fd, PROWLD_CONF_PATH);
	close(fd);
	if (!ok) {
		prowl_log(LOG_ERR, "load_config %s: %.128s",
		    PROWLD_CONF_PATH, ucl_parser_get_error(parser));
		ucl_parser_free(parser);
		return;
	}

	root = ucl_parser_get_object(parser);
	ucl_parser_free(parser);
	if (root == NULL)
		return;

	/* max_concurrent_starts: integer >= 0, or the string "auto" (= 0). */
	obj = ucl_object_lookup(root, "max_concurrent_starts");
	if (obj != NULL) {
		if (ucl_object_type(obj) == UCL_STRING &&
		    strcmp(ucl_object_tostring(obj), "auto") == 0) {
			g_config.max_concurrent_starts = DEFAULT_MAX_STARTS;
		} else {
			int64_t v = ucl_object_toint(obj);
			if (v >= 0)
				g_config.max_concurrent_starts = (int)v;
			else
				prowl_log(LOG_WARNING,
				    "load_config: max_concurrent_starts must "
				    "be >= 0, using default");
		}
	}

	/* shutdown_timeout: integer > 0 seconds. */
	obj = ucl_object_lookup(root, "shutdown_timeout");
	if (obj != NULL) {
		int64_t v = ucl_object_toint(obj);
		if (v > 0)
			g_config.shutdown_timeout = (int)v;
		else
			prowl_log(LOG_WARNING,
			    "load_config: shutdown_timeout must be > 0, "
			    "using default");
	}

	/* debug: boolean.  Config can enable debug; -d flag always wins. */
	obj = ucl_object_lookup(root, "debug");
	if (obj != NULL && ucl_object_type(obj) == UCL_BOOLEAN &&
	    ucl_object_toboolean(obj))
		g_config.debug = true;

	ucl_object_unref(root);

	prowl_log(LOG_INFO,
	    "config: max_concurrent_starts=%d shutdown_timeout=%d",
	    g_config.max_concurrent_starts, g_config.shutdown_timeout);
}

static void
setup_kqueue_signals(void)
{
	struct kevent kev;
	int sigs[] = { SIGTERM, SIGINT, SIGHUP, SIGCHLD, SIGUSR1, SIGUSR2,
	    SIGEMT, SIGWINCH, 0 };

	for (int i = 0; sigs[i] != 0; i++) {
		/*
		 * SIGUSR1/SIGUSR2 (halt, power off), SIGEMT (reroot) and
		 * SIGWINCH (power cycle) are init(8)'s signals.
		 */
		if (!g_is_init && (sigs[i] == SIGUSR1 || sigs[i] == SIGUSR2 ||
		    sigs[i] == SIGEMT || sigs[i] == SIGWINCH))
			continue;
		/*
		 * EVFILT_SIGNAL sees a signal even when it is ignored, so the
		 * terminating signals are ignored to keep their default action
		 * from killing us.  SIGCHLD must not be SIG_IGN: that makes the
		 * kernel discard children's exit status (no zombie, ECHILD
		 * from waitpid), so supervisor_reap() would never run.  Its
		 * default action is already "ignore".
		 */
		signal(sigs[i], sigs[i] == SIGCHLD ? SIG_DFL : SIG_IGN);
		EV_SET(&kev, (uintptr_t)sigs[i], EVFILT_SIGNAL,
		    EV_ADD, 0, 0, NULL);
		if (kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1)
			prowl_log(LOG_ERR, "kevent EVFILT_SIGNAL %d: %m",
			    sigs[i]);
	}
}

static void
handle_signal(int sig)
{
	switch (sig) {
	case SIGTERM:
		/*
		 * Under init(8) SIGTERM means "go to single-user mode".  It
		 * is what rc.subr's stop_boot() sends to abort a boot.
		 */
		if (g_is_init) {
			prowl_log(LOG_NOTICE, "SIGTERM: single-user mode");
			g_single_user = true;
		} else
			prowl_log(LOG_NOTICE,
			    "received signal %d, shutting down", sig);
		g_shutdown = true;
		break;
	case SIGINT:
		prowl_log(LOG_NOTICE, "received signal %d, %s", sig,
		    g_is_init ? "rebooting" : "shutting down");
		g_howto = RB_AUTOBOOT;
		g_shutdown = true;
		break;
	case SIGUSR1:
		prowl_log(LOG_NOTICE, "SIGUSR1: halting");
		g_howto = RB_HALT;
		g_shutdown = true;
		break;
	case SIGUSR2:
		prowl_log(LOG_NOTICE, "SIGUSR2: powering off");
		g_howto = RB_HALT | RB_POWEROFF;
		g_shutdown = true;
		break;
	case SIGEMT:
		prowl_log(LOG_NOTICE, "SIGEMT: reroot");
		g_reroot = true;
		g_shutdown = true;
		break;
	case SIGWINCH:
		prowl_log(LOG_NOTICE, "SIGWINCH: power cycling");
		g_howto = RB_POWERCYCLE;
		g_shutdown = true;
		break;
	case SIGHUP:
		if (g_is_init) {
			/* init(8): rescan /etc/ttys */
			prowl_log(LOG_NOTICE, "SIGHUP: rescanning /etc/ttys");
			pid1_rescan_ttys();
			break;
		}
		prowl_log(LOG_NOTICE, "received SIGHUP, reloading config");
		g_reload = true;
		break;
	case SIGCHLD:
		/* Reap all available children */
		for (;;) {
			int status;
			pid_t pid = waitpid(-1, &status, WNOHANG);
			if (pid <= 0)
				break;
			supervisor_reap(pid, status);
		}
		break;
	default:
		break;
	}
}

/*
 * Handle a EVFILT_PROC event: a watched child has exited.
 * The udata field carries the job pointer set when registering the filter.
 */
static void
handle_proc_event(struct kevent *kev)
{
	job_t *job = (job_t *)(uintptr_t)kev->udata;
	int status;
	pid_t pid;

	if (job == NULL)
		return;

	/* Reap via waitpid to get exit status */
	pid = waitpid((pid_t)kev->ident, &status, WNOHANG);
	if (pid > 0)
		supervisor_reap(pid, status);
	else if (pid == -1 && errno == ECHILD && job->pid == (pid_t)kev->ident)
		/* Not our child (no reaper): the exit status is unknown */
		supervisor_reap((pid_t)kev->ident, 1 << 8);
}

/*
 * Handle a EVFILT_TIMER event.
 * Low 2 bits of ident encode the timer type (job pointer is >=8-byte aligned):
 *   0b00  throttle / readiness timer
 *   0b01  stop-timeout (SIGKILL) timer
 *   0b10  watchdog timer (recurring)
 *   0b11  notify-timeout timer
 */
static void
handle_timer_event(struct kevent *kev)
{
	uintptr_t ident = kev->ident;

	/* Global (no job attached) timers */
	if (ident == TIMER_CALENDAR_TICK) {
		supervisor_handle_calendar_tick();
		return;
	}

	uintptr_t kind = ident & TIMER_MASK;
	job_t *job = (job_t *)(ident & ~TIMER_MASK);

	switch (kind) {
	case TIMER_THROTTLE:
		supervisor_handle_throttle(job);
		break;
	case TIMER_STOP:
		supervisor_handle_stop_timeout(job);
		break;
	case TIMER_WATCHDOG:
		supervisor_handle_watchdog(job);
		break;
	case NOTIFY_TMO:
		supervisor_handle_notify_timeout(job);
		break;
	case TIMER_PERIODIC:
		supervisor_handle_periodic(job);
		break;
	case TIMER_BOOT_DELAY:
		supervisor_handle_boot_delay(job);
		break;
	}
}

/*
 * Perform a configuration reload: re-read unit files and rc.d shims.
 * Existing running jobs are left untouched; new jobs become eligible.
 */
/*
 * Load unit files and rc-shims.  With -u only that directory is read and
 * rc.d is not scanned, which gives tests a self-contained prowld.
 */
static void
load_units(void)
{
	if (g_unit_override[0] != '\0') {
		unit_load_dir(g_unit_override);
		return;
	}
	unit_load_dir(UNIT_DIR_BASE);
	unit_load_dir(UNIT_DIR_LOCAL);
	unit_load_dir(UNIT_DIR_OVERRIDE);
	unit_load_dir(g_generated_dir);

	prowl_log(LOG_NOTICE, "scanning rc.d directories");
	rcshim_scan_dir(RCD_DIR_BASE);
	rcshim_scan_dir(RCD_DIR_LOCAL);
}

static void
reload_config(void)
{
	prowl_log(LOG_NOTICE, "reloading configuration");
	load_config();
	load_units();
	dag_build();
	dag_schedule_ready();
	g_reload = false;
}

static void
arm_calendar_tick(void)
{
	struct kevent kev;

	/* Fire every 60 seconds to check calendar jobs */
	EV_SET(&kev, TIMER_CALENDAR_TICK, EVFILT_TIMER,
	    EV_ADD, NOTE_SECONDS, 60, NULL);
	if (kevent(g_kqueue_fd, &kev, 1, NULL, 0, NULL) == -1)
		prowl_log(LOG_ERR, "kevent calendar tick: %m");
}

static void
startup_load(void)
{
	job_t *job;

	prowl_log(LOG_NOTICE, "prowld starting, loading unit files");

	load_units();

	/* init(8) respawns gettys from /etc/ttys; so do we */
	if (g_is_init)
		(void)pid1_load_ttys();

	dag_build();

	/* When booting the state directory is not ready yet: runtime_up() */
	if (!g_boot_mode)
		timer_state_load();

	/* Initialize job timers and handle catch-up */
	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (!job->enabled)
			continue;

		if (job->type == JOB_TYPE_TIMER) {
			if (job->schedule.interval > 0)
				arm_periodic_timer(job);
			if (job->schedule.on_boot_delay > 0)
				arm_boot_delay_timer(job);

			/* Catch-up logic (§12.3): if persistent and missed a run */
			if (job->schedule.persistent && job->last_run > 0) {
				/* Simplified: if last run was more than 1 interval ago
				 * or (for calendar) we have no better way to check yet,
				 * just trigger it once if some time has passed.
				 * Real calendar catch-up is complex, so we'll do a 
				 * basic implementation: if it hasn't run in the current 
				 * minute and it's persistent, we'll let the normal
				 * tick handle it, or trigger now if it was missed 
				 * during downtime.
				 */
				prowl_log(LOG_INFO, "timer %s: persistent catch-up check",
				    job->label);
				/* For now, just let the next tick handle it if matched,
				 * or we could force a start here if we were sure it missed.
				 */
			}
		}
	}
	arm_calendar_tick();
}

static void
main_loop(void)
{
	struct kevent events[64];
	int nev, i;
	struct timespec timeout;

	/* Start initial ready jobs */
	dag_schedule_ready();

	while (!g_shutdown) {
		if (g_reload)
			reload_config();

		timeout.tv_sec = 5;
		timeout.tv_nsec = 0;

		nev = kevent(g_kqueue_fd, NULL, 0, events,
		    (int)(sizeof(events) / sizeof(events[0])), &timeout);
		if (nev == -1) {
			if (errno == EINTR)
				continue;
			prowl_log(LOG_ERR, "kevent: %m");
			break;
		}

		for (i = 0; i < nev; i++) {
			struct kevent *kev = &events[i];

			switch (kev->filter) {
			case EVFILT_SIGNAL:
				handle_signal((int)kev->ident);
				break;
			case EVFILT_READ:
				if (g_ipc_listen_fd >= 0 &&
				    (int)kev->ident == g_ipc_listen_fd)
					ipc_accept();
				else if (kev->udata != NULL &&
				    ((uintptr_t)kev->udata & 1UL))
					/*
					 * Bit 0 set: socket activation fd.
					 * Decode the prowl_socket_t pointer.
					 */
					socket_handle_activation(
					    (prowl_socket_t *)
					    ((uintptr_t)kev->udata & ~1UL));
				else if (kev->udata != NULL)
					/*
					 * Non-NULL, bit 0 clear: notify fd
					 * registered by supervisor.c with the
					 * job pointer as udata.
					 */
					supervisor_handle_notify(
					    (job_t *)kev->udata);
				else
					ipc_read_client((int)kev->ident);
				break;
			case EVFILT_WRITE:
				/* IPC client socket has room again */
				ipc_write_ready((int)kev->ident);
				break;
			case EVFILT_PROC:
				handle_proc_event(kev);
				break;
			case EVFILT_TIMER:
				handle_timer_event(kev);
				break;
			default:
				break;
			}
		}

		ipc_expire_clients();

		/* After processing events, promote any newly-ready jobs */
		dag_schedule_ready();
		check_runtime();
	}
}

int
main(int argc, char *argv[])
{
	int ch, i;
	bool debug = false;
	bool single = false;
	bool reroot_two = false;
	bool devfs = false;
	const char *override_dir = NULL;

	/*
	 * If we are PID 1 (kernel executed us directly as init), skip
	 * daemon() — forking would leave an orphaned child and cause the
	 * kernel to panic when the parent exits.  Force debug/stderr output
	 * so early boot messages are visible on the console.
	 */
	g_is_init = (getpid() == 1);
	{
		int jailed = 0;
		size_t jlen = sizeof(jailed);

		if (sysctlbyname("security.jail.jailed", &jailed, &jlen, NULL,
		    0) == 0)
			g_jailed = (jailed != 0);
	}
	/*
	 * As PID 1 or as the first process of a jail, prowld replaces rc(8)
	 * and runs the rc.d boot sequence itself.
	 */
	g_boot_mode = g_is_init || g_jailed;

	if (g_is_init) {
		/*
		 * The kernel passes init its boot flags ("-s" for single-user,
		 * "-v", ...).  They are not prowld options: -s here means
		 * single-user, not a run directory.
		 */
		for (i = 1; i < argc; i++) {
			if (strcmp(argv[i], "-s") == 0)
				single = true;
			else if (strcmp(argv[i], "-f") == 0)
				g_fastboot = true;
			else if (strcmp(argv[i], "-r") == 0)
				reroot_two = true;
			else if (strcmp(argv[i], "-d") == 0)
				devfs = true;
		}
		pid1_mount_devfs(devfs);
		pid1_open_console();
		debug = true;
		g_config.debug = true;
	} else {
		while ((ch = getopt(argc, argv, "ds:u:")) != -1) {
			switch (ch) {
			case 'd':
				debug = true;
				break;
			case 's':
				override_dir = optarg;
				break;
			case 'u':
				if (strlcpy(g_unit_override, optarg,
				    sizeof(g_unit_override)) >=
				    sizeof(g_unit_override))
					errx(1, "unit directory too long");
				break;
			default:
				usage();
			}
		}
		argc -= optind;
		argv += optind;

		if (argc != 0)
			usage();
	}

	openlog("prowld", LOG_PID | LOG_NDELAY, LOG_DAEMON);

	/* Apply explicit runtime directory override before anything else. */
	if (override_dir != NULL)
		setup_runtime_paths(override_dir);

	if (g_is_init) {
		prowl_log(LOG_NOTICE, "running as PID 1 (init)");
		if (setlogin("root") < 0)
			prowl_log(LOG_WARNING, "setlogin() failed: %m");

		/* After a reroot the new root's init is run by pid1_reroot */
		if (reroot_two)
			pid1_reroot_phase_two();	/* does not return */
		pid1_reroot_cleanup();

		/* Loader variables: init_exec, init_script, init_chroot */
		pid1_init_exec();
		if (pid1_init_script() == -1)
			single = true;
		pid1_init_chroot();

		if (single)
			pid1_single_user();	/* does not return */
	} else if (!debug && daemon(0, 0) == -1) {
		err(1, "daemon");
	}

	/*
	 * A daemon that forks and lets its parent exit is found afterwards
	 * and supervised as our own child.  As init it is reparented to us;
	 * otherwise ask the kernel to do the same.
	 */
	if (g_is_init)
		g_reaper = true;
	else if (procctl(P_PID, getpid(), PROC_REAP_ACQUIRE, NULL) == 0)
		g_reaper = true;
	else
		prowl_log(LOG_WARNING, "procctl reaper: %m");

	g_kqueue_fd = kqueue();
	if (g_kqueue_fd == -1) {
		/* Exiting as PID 1 would panic the kernel. */
		prowl_log(LOG_ERR, "kqueue: %m");
		if (g_is_init)
			pid1_single_user();	/* does not return */
		exit(1);
	}
	(void)fcntl(g_kqueue_fd, F_SETFD, FD_CLOEXEC);

	setup_kqueue_signals();
	load_config();
	supervisor_init();

	/* Command-line -d always enables debug regardless of config file. */
	if (debug)
		g_config.debug = true;

	/*
	 * As a daemon the run directory, pidfile and IPC socket come up
	 * now.  As init the root file system is still read-only and the
	 * rc.d scripts have yet to run, so main_loop() does this later,
	 * through check_runtime().
	 */
	if (!g_boot_mode && runtime_up() == -1)
		exit(1);

	startup_load();
	main_loop();

	prowl_log(LOG_NOTICE, "prowld stopping");

	/*
	 * As init, exiting would panic the kernel: take the system down,
	 * or go to single-user mode, instead.
	 */
	if (g_is_init) {
		remove_pidfile();
		if (g_reroot)
			pid1_reroot();		/* does not return */
		if (g_single_user)
			pid1_single_user();	/* does not return */
		pid1_shutdown(g_howto);		/* does not return */
	}

	/*
	 * Orderly shutdown.  In a jail run rc.shutdown first, as rc(8) would
	 * for exec.stop, so services that rc-shims started are stopped by
	 * their own scripts and in dependency order.
	 */
	if (g_jailed)
		(void)boot_run_rundown("jail");
	supervisor_shutdown_all();
	ipc_shutdown();
	remove_pidfile();
	closelog();

	return (0);
}

/*
 * Until syslogd is running there is nowhere for syslog(3) to send a message,
 * so what prowld logs while the system comes up is lost.  Keep it, and send
 * it on once /var/run/log exists.  What does not fit is dropped and counted.
 */
#define EARLY_LOG_SIZE	65536
#define EARLY_LOG_LINE	256
static char	early_log[EARLY_LOG_SIZE];
static size_t	early_len;
static int	early_dropped;
static bool	early_done;

static void
early_log_add(int priority, const char *msg)
{
	char line[EARLY_LOG_LINE];
	struct tm tm;
	time_t now = time(NULL);
	size_t n;
	int len;

	/* Debug output is for the console; it would only crowd the rest out */
	if (priority == LOG_DEBUG)
		return;
	if (localtime_r(&now, &tm) == NULL)
		return;
	len = snprintf(line, sizeof(line), "%d %02d:%02d:%02d] %s",
	    LOG_PRI(priority), tm.tm_hour, tm.tm_min, tm.tm_sec, msg);
	if (len < 0)
		return;
	n = (size_t)len < sizeof(line) ? (size_t)len : sizeof(line) - 1;
	/* Past three quarters full keep only the notable messages */
	if (early_len + n + 1 > sizeof(early_log) ||
	    (priority > LOG_NOTICE &&
	    early_len > sizeof(early_log) / 4 * 3)) {
		early_dropped++;
		return;
	}
	memcpy(early_log + early_len, line, n);
	early_len += n;
	early_log[early_len++] = '\n';
}

/*
 * Is syslogd listening?  /var/run may outlive a boot, so a socket file
 * left by the last one does not count: connect to it.
 */
static bool
syslog_ready(void)
{
	static const int types[2] = { SOCK_DGRAM, SOCK_STREAM };
	struct sockaddr_un sun;
	int fd, i;
	bool ok = false;

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, "/var/run/log", sizeof(sun.sun_path));
	for (i = 0; i < 2 && !ok; i++) {
		fd = socket(AF_UNIX, types[i], 0);
		if (fd == -1)
			continue;
		ok = connect(fd, (struct sockaddr *)&sun, sizeof(sun)) == 0;
		close(fd);
	}
	return (ok);
}

/* Send the saved messages to syslogd, if it is there now */
static void
early_log_flush(void)
{
	struct timespec pause = { 0, 5000000L };
	char *line, *next, *end;
	long pri;
	int sent = 0;

	if (!syslog_ready())
		return;
	early_done = true;
	for (line = early_log; line < early_log + early_len; line = next) {
		end = memchr(line, '\n', (size_t)(early_log + early_len - line));
		if (end == NULL)
			break;
		*end = '\0';
		next = end + 1;
		pri = strtol(line, &end, 10);
		if (end == line || *end != ' ' || pri < 0 || pri > LOG_DEBUG)
			continue;
		syslog((int)pri, "[early boot %s", end + 1);
		/*
		 * syslogd's socket holds a few dozen messages; give it time
		 * to read them or the rest are lost.
		 */
		if (++sent % 16 == 0)
			(void)nanosleep(&pause, NULL);
	}
	if (early_dropped > 0)
		syslog(LOG_WARNING, "%d early boot messages were dropped",
		    early_dropped);
	early_len = 0;
}

/* Logging wrapper that goes to syslog and optionally stderr */
void
prowl_log(int priority, const char *fmt, ...)
{
	char msg[EARLY_LOG_LINE];
	char line[1024];
	va_list ap;
	int saved_errno = errno;
	int n;

	if (!early_done)
		early_log_flush();
	if (!early_done) {
		va_start(ap, fmt);
		(void)vsnprintf(msg, sizeof(msg), fmt, ap);
		va_end(ap);
		early_log_add(priority, msg);
		errno = saved_errno;
	}

	va_start(ap, fmt);
	vsyslog(priority, fmt, ap);
	va_end(ap);

	if (g_config.debug) {
		/* syslog may have changed errno; %m must see the caller's */
		errno = saved_errno;
		va_start(ap, fmt);
		n = vsnprintf(line, sizeof(line) - 2, fmt, ap);
		va_end(ap);
		if (n < 0)
			n = 0;
		if ((size_t)n > sizeof(line) - 3)
			n = (int)sizeof(line) - 3;
		/*
		 * One write per line, so output from other processes cannot
		 * split it, and CR LF, so the lines do not stair-step when
		 * something left the console without output processing.
		 */
		line[n++] = '\r';
		line[n++] = '\n';
		(void)write(STDERR_FILENO, line, (size_t)n);
	}
}
