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
#include <sys/event.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/time.h>
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
static bool		 g_is_init = false;	/* running as PID 1 */

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
	fprintf(stderr, "usage: prowld [-d] [-s rundir]\n");
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

static void
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
			exit(1);
		}
	}

	/* Subdirectories — failures are logged but not fatal */
	(void)secure_mkdir(g_notify_dir,    0700); /* root-only: sd_notify */
	(void)secure_mkdir(g_db_dir,        0755); /* world-readable */
	(void)secure_mkdir(g_mask_dir,      0700); /* root-only: masks */
	(void)secure_mkdir(g_log_dir,       0750); /* daemon logs */
	(void)secure_mkdir(g_job_log_dir,   0750); /* per-job logs */
	(void)secure_mkdir(g_generated_dir, 0755); /* generated units */
}

static void
load_config(void)
{
	struct ucl_parser *parser;
	ucl_object_t *root;
	const ucl_object_t *obj;
	struct stat sb;

	/* Apply defaults first; debug is not reset here — it is set by
	 * the command-line -d flag in main() and must not be overridden. */
	g_config.max_concurrent_starts = DEFAULT_MAX_STARTS;
	g_config.shutdown_timeout      = DEFAULT_SHUTDOWN_TMO;

	if (stat(PROWLD_CONF_PATH, &sb) == -1) {
		if (errno != ENOENT)
			prowl_log(LOG_WARNING, "load_config stat %s: %m",
			    PROWLD_CONF_PATH);
		return;
	}

	/* Reject config files not owned by root or world-writable. */
	if (sb.st_uid != 0) {
		prowl_log(LOG_WARNING,
		    "%s: not owned by root, using defaults", PROWLD_CONF_PATH);
		return;
	}
	if (sb.st_mode & S_IWOTH) {
		prowl_log(LOG_WARNING,
		    "%s: world-writable, using defaults", PROWLD_CONF_PATH);
		return;
	}

	parser = ucl_parser_new(UCL_PARSER_NO_FILEVARS);
	if (parser == NULL) {
		prowl_log(LOG_ERR, "load_config: ucl_parser_new failed");
		return;
	}

	if (!ucl_parser_add_file(parser, PROWLD_CONF_PATH)) {
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
	int sigs[] = { SIGTERM, SIGINT, SIGHUP, SIGCHLD, 0 };

	for (int i = 0; sigs[i] != 0; i++) {
		signal(sigs[i], SIG_IGN);
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
	case SIGINT:
		prowl_log(LOG_NOTICE, "received signal %d, shutting down", sig);
		g_shutdown = true;
		break;
	case SIGHUP:
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
static void
reload_config(void)
{
	prowl_log(LOG_NOTICE, "reloading configuration");
	load_config();
	unit_load_dir(UNIT_DIR_BASE);
	unit_load_dir(UNIT_DIR_LOCAL);
	unit_load_dir(UNIT_DIR_OVERRIDE);
	unit_load_dir(g_generated_dir);
	rcshim_scan_dir(RCD_DIR_BASE);
	rcshim_scan_dir(RCD_DIR_LOCAL);
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
...
	rcshim_scan_dir(RCD_DIR_LOCAL);

	dag_build();

	/* Initialize job timers */
	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (!job->enabled)
			continue;
		if (job->type == JOB_TYPE_TIMER) {
			if (job->schedule.interval > 0)
				arm_periodic_timer(job);
			if (job->schedule.on_boot_delay > 0)
				arm_boot_delay_timer(job);
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

		/* After processing events, promote any newly-ready jobs */
		dag_schedule_ready();
	}
}

int
main(int argc, char *argv[])
{
	int ch;
	bool debug = false;
	const char *override_dir = NULL;

	while ((ch = getopt(argc, argv, "ds:")) != -1) {
		switch (ch) {
		case 'd':
			debug = true;
			break;
		case 's':
			override_dir = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	openlog("prowld", LOG_PID | LOG_NDELAY, LOG_DAEMON);

	/* Apply explicit runtime directory override before anything else. */
	if (override_dir != NULL)
		setup_runtime_paths(override_dir);

	/*
	 * If we are PID 1 (kernel executed us directly as init), skip
	 * daemon() — forking would leave an orphaned child and cause the
	 * kernel to panic when the parent exits.  Force debug/stderr output
	 * so early boot messages are visible on the console.
	 */
	g_is_init = (getpid() == 1);
	if (g_is_init) {
		prowl_log(LOG_NOTICE, "running as PID 1 (init)");
		debug = true;
	} else if (!debug && daemon(0, 0) == -1) {
		err(1, "daemon");
	}

	g_kqueue_fd = kqueue();
	if (g_kqueue_fd == -1)
		err(1, "kqueue");

	/*
	 * ensure_dirs() auto-detects read-only media and falls back to
	 * /tmp/prowld if the standard run dir is not writable.
	 */
	ensure_dirs();
	write_pidfile();
	setup_kqueue_signals();
	load_config();

	/* Command-line -d always enables debug regardless of config file. */
	if (debug)
		g_config.debug = true;

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

	startup_load();
	main_loop();

	/* Orderly shutdown */
	prowl_log(LOG_NOTICE, "prowld stopping");
	supervisor_shutdown_all();
	ipc_shutdown();
	remove_pidfile();
	closelog();

	/*
	 * If we are PID 1, exiting would panic the kernel.  Trigger a
	 * system halt instead.  This is the last resort — under normal
	 * operation g_shutdown is only set by SIGTERM/SIGINT which on a
	 * real system would come from a shutdown(8) invocation.
	 */
	if (g_is_init) {
		reboot(RB_HALT);
		/* NOTREACHED */
	}

	return (0);
}

/* Logging wrapper that goes to syslog and optionally stderr */
void
prowl_log(int priority, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(priority, fmt, ap);
	va_end(ap);

	if (g_config.debug) {
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
		fputc('\n', stderr);
	}
}
