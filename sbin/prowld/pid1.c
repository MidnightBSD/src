/*
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
 * pid1.c - what prowld must do when the kernel runs it as init(8).
 *
 * This follows sbin/init/init.c: open the console, shut the system down on
 * the signals init(8) honours, go to single-user mode on SIGTERM (which is
 * how rc.subr's stop_boot() aborts a boot) and respawn gettys from
 * /etc/ttys.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <ttyent.h>
#include <unistd.h>

#include "prowld.h"

#define PID1_PATH	"/sbin/prowld"
#define PID1_SHELL	"/bin/sh"
#define PID1_RUNDOWN	"/etc/rc.shutdown"
#define PID1_TERM_WAIT	5	/* seconds from SIGTERM to SIGKILL */
#define PID1_KILL_WAIT	5	/* seconds to wait after SIGKILL */
#define PID1_RUNDOWN_TMO 120	/* seconds rc.shutdown may take */

static char *const pid1_env[] = {
	(char *)(uintptr_t)"HOME=/",
	(char *)(uintptr_t)"PATH=/sbin:/bin:/usr/sbin:/usr/bin",
	NULL
};

/*
 * Become session leader and make stdin/stdout/stderr the console, as
 * init(8) does, so that anything we or the rc scripts print is visible
 * and logging to stderr works before syslogd is running.
 */
void
pid1_open_console(void)
{
	int fd, i;

	if (setsid() == -1 && (errno != EPERM || getsid(0) != 1))
		syslog(LOG_WARNING, "initial setsid() failed: %m");

	fd = open(_PATH_CONSOLE, O_RDWR | O_NOCTTY);
	if (fd == -1)
		fd = open(_PATH_DEVNULL, O_RDWR);
	if (fd == -1)
		return;
	for (i = 0; i < 3; i++) {
		if (fd != i)
			(void)dup2(fd, i);
	}
	if (fd > 2)
		close(fd);
}

/*
 * Reap every child that has exited without blocking, handing job
 * children to the supervisor.
 */
static void
pid1_reap_all(void)
{
	pid_t pid;
	int st;

	while ((pid = waitpid(-1, &st, WNOHANG)) > 0)
		supervisor_reap(pid, st);
}

/*
 * Wait up to secs seconds for all children to exit.  Returns true when no
 * children are left.
 */
static bool
pid1_wait_children(int secs)
{
	struct timespec ts = { 0, 50000000L };
	time_t end = time(NULL) + secs;
	pid_t pid;
	int st;

	for (;;) {
		pid = waitpid(-1, &st, WNOHANG);
		if (pid > 0) {
			supervisor_reap(pid, st);
			continue;
		}
		if (pid == -1 && errno == ECHILD)
			return (true);
		if (time(NULL) >= end)
			return (false);
		nanosleep(&ts, NULL);
	}
}

/*
 * Signal every process in the system (other than ourselves and kernel
 * processes): SIGTERM, then SIGKILL for whatever is left.
 */
static void
pid1_kill_all(void)
{
	static const int sigs[2] = { SIGTERM, SIGKILL };
	int i;

	for (i = 0; i < 2; i++) {
		if (kill(-1, sigs[i]) == -1 && errno == ESRCH)
			return;
		if (pid1_wait_children(i == 0 ? PID1_TERM_WAIT :
		    PID1_KILL_WAIT))
			return;
	}
	prowl_log(LOG_WARNING, "some processes would not die");
}

/*
 * Run /etc/rc.shutdown, which stops every service that has an rc.d script
 * with KEYWORD: shutdown - including daemons started through rc-shims that
 * prowld does not track itself.  Returns the script's exit status, or -1
 * if it could not be run or timed out.
 */
static int
pid1_run_rundown(const char *arg)
{
	struct timespec ts = { 0, 50000000L };
	time_t end;
	struct stat sb;
	pid_t pid, wpid;
	int st;

	if (stat(PID1_RUNDOWN, &sb) == -1 && errno == ENOENT)
		return (0);

	pid = fork();
	if (pid == -1) {
		prowl_log(LOG_ERR, "fork for %s: %m", PID1_RUNDOWN);
		return (-1);
	}
	if (pid == 0) {
		char *argv[4];

		prowl_reset_signals();
		argv[0] = (char *)(uintptr_t)"sh";
		argv[1] = (char *)(uintptr_t)PID1_RUNDOWN;
		argv[2] = (char *)(uintptr_t)arg;
		argv[3] = NULL;
		execve(PID1_SHELL, argv, pid1_env);
		_exit(1);
	}

	end = time(NULL) + PID1_RUNDOWN_TMO;
	for (;;) {
		wpid = waitpid(-1, &st, WNOHANG);
		if (wpid == pid)
			return (WIFEXITED(st) ? WEXITSTATUS(st) : -1);
		if (wpid > 0) {
			supervisor_reap(wpid, st);
			continue;
		}
		if (wpid == -1 && errno != EINTR)
			return (-1);
		if (time(NULL) >= end) {
			prowl_log(LOG_WARNING, "%s timed out", PID1_RUNDOWN);
			kill(pid, SIGTERM);
			return (-1);
		}
		nanosleep(&ts, NULL);
	}
}

/*
 * Take the system down: run rc.shutdown, stop our own jobs, terminate
 * everything that is left, and reboot(2) with howto.  Does not return.
 */
void
pid1_shutdown(int howto)
{
	prowl_log(LOG_NOTICE, "system going down (howto %#x)", howto);

	(void)pid1_run_rundown("reboot");
	supervisor_shutdown_all();
	ipc_shutdown();
	pid1_kill_all();
	pid1_reap_all();
	sync();

	if (reboot(howto) == -1)
		prowl_log(LOG_ERR, "reboot(%#x) failed: %m", howto);
	/* Returning from init panics the kernel, which reboots. */
	_exit(1);
}

/*
 * Single-user mode: kill every process, run a shell on the console and,
 * when it exits, start over by re-executing prowld, which is what init(8)
 * does by running /etc/rc again.  Does not return.
 */
void
pid1_single_user(void)
{
	pid_t pid, wpid;
	char *argv[2];
	int fd, st;

	prowl_log(LOG_NOTICE, "going to single-user mode");
	ipc_shutdown();
	pid1_kill_all();
	pid1_reap_all();

	pid = fork();
	if (pid == 0) {
		prowl_reset_signals();
		(void)setsid();
		fd = open(_PATH_CONSOLE, O_RDWR);
		if (fd != -1) {
			(void)ioctl(fd, TIOCSCTTY, 0);
			(void)dup2(fd, 0);
			(void)dup2(fd, 1);
			(void)dup2(fd, 2);
			if (fd > 2)
				close(fd);
		}
		argv[0] = (char *)(uintptr_t)"-sh";
		argv[1] = NULL;
		execve(PID1_SHELL, argv, pid1_env);
		_exit(1);
	}

	if (pid == -1) {
		prowl_log(LOG_ERR, "cannot fork a single-user shell: %m");
	} else {
		do {
			wpid = waitpid(-1, &st, 0);
		} while (wpid != pid && (wpid != -1 || errno == EINTR));
	}

	{
		char *nargv[2];

		nargv[0] = (char *)(uintptr_t)PID1_PATH;
		nargv[1] = NULL;
		execve(PID1_PATH, nargv, pid1_env);
	}
	prowl_log(LOG_ERR, "cannot re-execute %s: %m", PID1_PATH);
	(void)reboot(RB_AUTOBOOT);
	_exit(1);
}

/*
 * Create one keep-alive job running getty(8) for a /etc/ttys entry.  The
 * command line is split on white space and the device name appended, as
 * init(8) does; TERM comes from the entry's type.  Returns 0 if a job was
 * added.
 */
static int
pid1_add_getty(const struct ttyent *typ)
{
	char cmd[PROWL_PATH_MAX], devpath[PROWL_PATH_MAX], term[96];
	char *p, *tok;
	job_t *job;
	int i;

	if (!label_valid(typ->ty_name))
		return (-1);

	/* "onifexists" / "onifconsole": only when the device is there */
	if ((typ->ty_status & (TTY_IFEXISTS | TTY_IFCONSOLE)) != 0) {
		snprintf(devpath, sizeof(devpath), "%s%s", _PATH_DEV,
		    typ->ty_name);
		if (access(devpath, F_OK) == -1)
			return (-1);
	}

	job = job_alloc();
	if (job == NULL)
		return (-1);

	snprintf(job->label, sizeof(job->label), "org.midnightbsd.getty.%s",
	    typ->ty_name);
	if (job_find_by_label(job->label) != NULL) {
		job_free(job);
		return (-1);
	}
	snprintf(job->description, sizeof(job->description), "getty on %s",
	    typ->ty_name);
	strlcpy(job->rc_name, typ->ty_name, sizeof(job->rc_name));
	job->type = JOB_TYPE_DAEMON;

	if (strlcpy(cmd, typ->ty_getty, sizeof(cmd)) >= sizeof(cmd)) {
		job_free(job);
		return (-1);
	}
	p = cmd;
	while ((tok = strsep(&p, " \t")) != NULL) {
		if (*tok == '\0')
			continue;
		if (job->program[0] == '\0') {
			strlcpy(job->program, tok, sizeof(job->program));
			continue;
		}
		if (job->argc >= PROWL_ARGS_MAX - 2)
			break;
		job->arguments[job->argc] = strdup(tok);
		if (job->arguments[job->argc] == NULL) {
			job_free(job);
			return (-1);
		}
		job->argc++;
	}
	if (job->program[0] == '\0') {
		job_free(job);
		return (-1);
	}
	job->arguments[job->argc] = strdup(typ->ty_name);
	if (job->arguments[job->argc] == NULL) {
		job_free(job);
		return (-1);
	}
	job->argc++;

	/* An explicit environment replaces the default one. */
	job->envp[job->envc] = strdup("PATH=/sbin:/bin:/usr/sbin:/usr/bin");
	if (job->envp[job->envc] != NULL)
		job->envc++;
	if (typ->ty_type != NULL) {
		snprintf(term, sizeof(term), "TERM=%s", typ->ty_type);
		job->envp[job->envc] = strdup(term);
		if (job->envp[job->envc] != NULL)
			job->envc++;
	}

	/* Logins are offered once the rc.d boot sequence is complete. */
	i = 0;
	strlcpy(job->deps[i].name, "LOGIN", PROWL_LABEL_MAX);
	job->deps[i].hard = true;
	job->deps_count = 1;

	/* Respawn after every exit, as init(8) does. */
	job->keep_alive.always = true;
	job->throttle_interval = 1;
	job->enabled = true;
	job->run_at_load = true;

	TAILQ_INSERT_TAIL(&g_jobs, job, entries);
	return (0);
}

/*
 * Create getty jobs for every enabled /etc/ttys entry.  Returns the number
 * of jobs created.
 */
int
pid1_load_ttys(void)
{
	struct ttyent *typ;
	int count = 0;

	setttyent();
	while ((typ = getttyent()) != NULL) {
		if ((typ->ty_status & TTY_ON) == 0 ||
		    typ->ty_name == NULL || typ->ty_getty == NULL)
			continue;
		if (pid1_add_getty(typ) == 0)
			count++;
	}
	endttyent();

	if (count > 0)
		prowl_log(LOG_NOTICE, "loaded %d getty job(s) from /etc/ttys",
		    count);
	return (count);
}
