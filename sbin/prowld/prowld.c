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

static void
usage(void)
{
	fprintf(stderr, "usage: prowld [-d]\n");
	exit(1);
}

static void
write_pidfile(void)
{
	FILE *fp;
	char pidbuf[32];

	fp = fopen(PROWLD_PID_PATH, "w");
	if (fp == NULL) {
		prowl_log(LOG_WARNING, "cannot write pidfile %s: %m",
		    PROWLD_PID_PATH);
		return;
	}
	snprintf(pidbuf, sizeof(pidbuf), "%d\n", (int)getpid());
	fputs(pidbuf, fp);
	fclose(fp);
}

static void
remove_pidfile(void)
{
	unlink(PROWLD_PID_PATH);
}

static void
ensure_dirs(void)
{
	static const struct {
		const char *path;
		mode_t	    mode;
	} dirs[] = {
		{ PROWLD_RUN_DIR,    0755 },	/* world-searchable for socket */
		{ PROWLD_NOTIFY_DIR, 0700 },	/* root-only: readiness pipes */
		{ PROWLD_DB_DIR,     0755 },	/* world-readable for tooling */
		{ PROWLD_MASK_DIR,   0700 },	/* root-only: mask symlinks */
		{ PROWLD_LOG_DIR,    0750 },	/* prowld own logs */
		{ PROWLD_JOB_LOG_DIR, 0750 },	/* per-job log files */
		{ NULL, 0 }
	};
	int i;

	for (i = 0; dirs[i].path != NULL; i++) {
		if (mkdir(dirs[i].path, dirs[i].mode) == -1 &&
		    errno != EEXIST)
			prowl_log(LOG_WARNING, "mkdir %s: %m", dirs[i].path);
	}
}

static void
load_config(void)
{
	struct ucl_parser *parser;
	const ucl_object_t *root, *obj;
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
	uintptr_t kind = ident & 3UL;
	job_t *job = (job_t *)(ident & ~3UL);

	switch (kind) {
	case 0:
		supervisor_handle_throttle(job);
		break;
	case 1:
		supervisor_handle_stop_timeout(job);
		break;
	case 2:
		supervisor_handle_watchdog(job);
		break;
	case 3:
		supervisor_handle_notify_timeout(job);
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
	rcshim_scan_dir(RCD_DIR_BASE);
	rcshim_scan_dir(RCD_DIR_LOCAL);
	dag_build();
	dag_schedule_ready();
	g_reload = false;
}

static void
startup_load(void)
{
	prowl_log(LOG_NOTICE, "prowld starting, loading unit files");

	unit_load_dir(UNIT_DIR_BASE);
	unit_load_dir(UNIT_DIR_LOCAL);
	unit_load_dir(UNIT_DIR_OVERRIDE);

	prowl_log(LOG_NOTICE, "scanning rc.d directories");
	rcshim_scan_dir(RCD_DIR_BASE);
	rcshim_scan_dir(RCD_DIR_LOCAL);

	dag_build();
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
				if ((int)kev->ident == g_ipc_listen_fd)
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

	while ((ch = getopt(argc, argv, "d")) != -1) {
		switch (ch) {
		case 'd':
			debug = true;
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

	if (!debug && daemon(0, 0) == -1)
		err(1, "daemon");

	g_kqueue_fd = kqueue();
	if (g_kqueue_fd == -1)
		err(1, "kqueue");

	ensure_dirs();
	write_pidfile();
	setup_kqueue_signals();
	load_config();

	/* Command-line -d always enables debug regardless of config file. */
	if (debug)
		g_config.debug = true;

	if (ipc_init() == -1) {
		prowl_log(LOG_ERR, "IPC init failed, aborting");
		remove_pidfile();
		return (1);
	}

	/* Store IPC listen fd for the event loop */
	g_ipc_listen_fd = ipc_listen_fd;

	startup_load();
	main_loop();

	/* Orderly shutdown */
	prowl_log(LOG_NOTICE, "prowld stopping");
	supervisor_shutdown_all();
	ipc_shutdown();
	remove_pidfile();
	closelog();

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
