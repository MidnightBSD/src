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
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <kenv.h>
#include <paths.h>
#include <pwd.h>
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
#define PID1_REROOT	"/dev/reroot"
#define PID1_REROOT_INIT PID1_REROOT "/init"
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

/* rc.shutdown must not stop prowld itself: rc.d/prowld checks this. */
static char *const rundown_env[] = {
	(char *)(uintptr_t)"HOME=/",
	(char *)(uintptr_t)"PATH=/sbin:/bin:/usr/sbin:/usr/bin",
	(char *)(uintptr_t)"PROWLD_RUNDOWN=1",
	NULL
};

/*
 * Mount devfs on /dev if the kernel has not.  That is the case after a
 * reroot, and when init is started with -d.  If / and /dev are on the same
 * device, /dev is just a directory.
 */
void
pid1_mount_devfs(bool force)
{
	struct iovec iov[4];
	struct stat st;
	dev_t root_dev;

	if (!force) {
		if (stat("/", &st) != 0)
			return;
		root_dev = st.st_dev;
		if (stat(_PATH_DEV, &st) != 0 || st.st_dev != root_dev)
			return;
	}

	iov[0].iov_base = (void *)(uintptr_t)"fstype";
	iov[0].iov_len = sizeof("fstype");
	iov[1].iov_base = (void *)(uintptr_t)"devfs";
	iov[1].iov_len = sizeof("devfs");
	iov[2].iov_base = (void *)(uintptr_t)"fspath";
	iov[2].iov_len = sizeof("fspath");
	iov[3].iov_base = (void *)(uintptr_t)"/dev";
	iov[3].iov_len = sizeof("/dev");
	(void)nmount(iov, 4, 0);
}

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
int
boot_run_rundown(const char *arg)
{
	struct timespec ts = { 0, 50000000L };
	time_t end;
	struct stat sb;
	pid_t pid, wpid;
	int st;

	if (stat(PID1_RUNDOWN, &sb) == -1 && errno == ENOENT)
		return (0);

	/* Daemons that rc.shutdown stops must stay down, not be restarted */
	g_rundown = true;

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
		execve(PID1_SHELL, argv, rundown_env);
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

	(void)boot_run_rundown("reboot");
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
 * Fetch a kernel environment (loader) variable, as init(8) does for
 * init_exec, init_script, init_chroot, init_shell and init_path.
 */
static bool
pid1_kenv(const char *name, char *buf, size_t len)
{
	return (kenv(KENV_GET, name, buf, (int)len) > 0);
}

static const char *
pid1_shell(void)
{
	static char shell[PATH_MAX];

	if (pid1_kenv("init_shell", shell, sizeof(shell)))
		return (shell);
	return (PID1_SHELL);
}

/*
 * Run a script the way init(8) does: execute it directly if it is
 * executable, otherwise hand it to the shell.  Only returns on failure.
 */
static void
pid1_exec_script(char *const argv[])
{
	const char *script = argv[1];

	if (access(script, X_OK) == 0) {
		execve(script, argv + 1, pid1_env);
		prowl_log(LOG_WARNING, "can't directly exec %s: %m", script);
	} else if (errno != EACCES) {
		prowl_log(LOG_WARNING, "can't access %s: %m", script);
	}
	execve(pid1_shell(), argv, pid1_env);
	prowl_log(LOG_ERR, "can't exec %s for %s: %m", pid1_shell(), script);
}

/*
 * loader variable init_exec: run this script in place of init.  It is
 * expected to end by rebooting the machine.  Does not return.
 */
void
pid1_init_exec(void)
{
	char path[PATH_MAX];
	char *argv[3];

	if (!pid1_kenv("init_exec", path, sizeof(path)))
		return;
	argv[0] = (char *)(uintptr_t)"sh";
	argv[1] = path;
	argv[2] = NULL;
	prowl_reset_signals();
	pid1_exec_script(argv);
	_exit(0);
}

/*
 * loader variable init_script: run a script before anything else.
 * Returns -1 if it failed, which sends us to single-user mode.
 */
int
pid1_init_script(void)
{
	char path[PATH_MAX];
	char *argv[4];
	pid_t pid, wpid;
	int st;

	if (!pid1_kenv("init_script", path, sizeof(path)))
		return (0);

	pid = fork();
	if (pid == -1) {
		prowl_log(LOG_ERR, "can't fork for %s: %m", path);
		return (-1);
	}
	if (pid == 0) {
		prowl_reset_signals();
		argv[0] = (char *)(uintptr_t)"sh";
		argv[1] = path;
		argv[2] = g_fastboot ? NULL : (char *)(uintptr_t)"autoboot";
		argv[3] = NULL;
		pid1_exec_script(argv);
		_exit(1);
	}
	do {
		wpid = waitpid(pid, &st, 0);
	} while (wpid == -1 && errno == EINTR);
	if (wpid != pid || !WIFEXITED(st) || WEXITSTATUS(st) != 0) {
		prowl_log(LOG_ERR, "%s failed", path);
		return (-1);
	}
	return (0);
}

/* loader variable init_chroot */
void
pid1_init_chroot(void)
{
	char path[PATH_MAX];

	if (!pid1_kenv("init_chroot", path, sizeof(path)))
		return;
	if (chdir(path) != 0 || chroot(".") != 0)
		prowl_log(LOG_WARNING, "can't chroot to %s: %m", path);
}

/*
 * The root password check init(8) makes before it starts the single-user
 * shell when the console is not "secure" in /etc/ttys.  Runs in the shell's
 * child, with the console as controlling terminal.  Exits 0 (go multi-user)
 * on an empty password or end of file.
 */
static void
pid1_single_user_auth(void)
{
	static const char banner[] =
	    "Enter root password, or ^D to go multi-user\n";
	static const char failed[] = "single-user login failed\n";
	struct ttyent *typ;
	struct passwd *pp;
	char *clear, *crypted;

	typ = getttynam("console");
	pp = getpwnam("root");
	if (typ == NULL || (typ->ty_status & TTY_SECURE) != 0 ||
	    pp == NULL || *pp->pw_passwd == '\0')
		return;

	(void)write(STDERR_FILENO, banner, sizeof(banner) - 1);
	for (;;) {
		clear = getpass("Password:");
		if (clear == NULL || *clear == '\0')
			_exit(0);
		crypted = crypt(clear, pp->pw_passwd);
		explicit_bzero(clear, _PASSWORD_LEN);
		if (crypted != NULL && strcmp(crypted, pp->pw_passwd) == 0)
			break;
		(void)write(STDERR_FILENO, failed, sizeof(failed) - 1);
	}
	endttyent();
	endpwent();
}

/*
 * Single-user mode: kill every process, run a shell on the console and,
 * when it exits, start over by re-executing prowld with -f, which is what
 * init(8) does by running /etc/rc again without "autoboot".  Does not
 * return.
 */
void
pid1_single_user(void)
{
	pid_t pid, wpid;
	char *argv[3];
	int fd, st;

	for (;;) {
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
			pid1_single_user_auth();
			argv[0] = (char *)(uintptr_t)"-sh";
			argv[1] = NULL;
			execve(pid1_shell(), argv, pid1_env);
			prowl_log(LOG_ERR, "can't exec %s: %m", pid1_shell());
			execve(PID1_SHELL, argv, pid1_env);
			_exit(1);
		}

		if (pid == -1) {
			prowl_log(LOG_ERR,
			    "cannot fork a single-user shell: %m");
			sleep(5);
			continue;
		}

		do {
			wpid = waitpid(-1, &st, 0);
		} while (wpid != pid && (wpid != -1 || errno == EINTR));

		if (wpid == pid && WIFSIGNALED(st)) {
			if (WTERMSIG(st) == SIGKILL) {
				/* reboot(8) killed the shell: let it finish */
				prowl_log(LOG_WARNING,
				    "single user shell terminated");
				sleep(30);
				_exit(0);
			}
			prowl_log(LOG_WARNING,
			    "single user shell terminated, restarting");
			continue;
		}
		break;
	}

	argv[0] = (char *)(uintptr_t)PID1_PATH;
	argv[1] = (char *)(uintptr_t)"-f";
	argv[2] = NULL;
	execve(PID1_PATH, argv, pid1_env);
	prowl_log(LOG_ERR, "cannot re-execute %s: %m", PID1_PATH);
	(void)reboot(RB_AUTOBOOT);
	_exit(1);
}

/*
 * Reroot (SIGEMT, reboot -r): stop everything, unmount the root file
 * system and mount it again, possibly from somewhere else
 * (vfs.root.mountfrom).  That needs prowld to live off the old root, so a
 * copy of it goes to a tmpfs on /dev/reroot, which re-executes itself with
 * -r (pid1_reroot_phase_two) to do the reboot(RB_REROOT) and then exec
 * the init of the new root.
 */
static int
pid1_mount_tmpfs(const char *fspath)
{
	struct iovec iov[6];
	char errmsg[255];

	memset(errmsg, 0, sizeof(errmsg));
	iov[0].iov_base = (void *)(uintptr_t)"fstype";
	iov[0].iov_len = sizeof("fstype");
	iov[1].iov_base = (void *)(uintptr_t)"tmpfs";
	iov[1].iov_len = sizeof("tmpfs");
	iov[2].iov_base = (void *)(uintptr_t)"fspath";
	iov[2].iov_len = sizeof("fspath");
	iov[3].iov_base = (void *)(uintptr_t)fspath;
	iov[3].iov_len = strlen(fspath) + 1;
	iov[4].iov_base = (void *)(uintptr_t)"errmsg";
	iov[4].iov_len = sizeof("errmsg");
	iov[5].iov_base = errmsg;
	iov[5].iov_len = sizeof(errmsg);

	if (nmount(iov, 6, 0) != 0) {
		prowl_log(LOG_ERR, "cannot mount tmpfs on %s: %s: %m",
		    fspath, errmsg);
		return (-1);
	}
	return (0);
}

/* Copy PID1_PATH to PID1_REROOT_INIT. */
static int
pid1_copy_self(void)
{
	char buf[8192];
	ssize_t n, w, off;
	int in, out, ret = -1;

	in = open(PID1_PATH, O_RDONLY);
	if (in == -1) {
		prowl_log(LOG_ERR, "%s: %m", PID1_PATH);
		return (-1);
	}
	out = open(PID1_REROOT_INIT, O_WRONLY | O_CREAT | O_EXCL, 0700);
	if (out == -1) {
		prowl_log(LOG_ERR, "%s: %m", PID1_REROOT_INIT);
		close(in);
		return (-1);
	}
	while ((n = read(in, buf, sizeof(buf))) > 0) {
		for (off = 0; off < n; off += w) {
			w = write(out, buf + off, (size_t)(n - off));
			if (w <= 0) {
				prowl_log(LOG_ERR, "%s: write: %m",
				    PID1_REROOT_INIT);
				goto out;
			}
		}
	}
	if (n == 0)
		ret = 0;
	else
		prowl_log(LOG_ERR, "%s: read: %m", PID1_PATH);
out:
	if (close(out) != 0 && ret == 0) {
		prowl_log(LOG_ERR, "%s: close: %m", PID1_REROOT_INIT);
		ret = -1;
	}
	close(in);
	return (ret);
}

void
pid1_reroot(void)
{
	char *argv[3];

	prowl_log(LOG_NOTICE, "rerooting");
	(void)boot_run_rundown("reboot");
	supervisor_shutdown_all();
	ipc_shutdown();

	if (kill(-1, SIGKILL) == -1 && errno != ESRCH) {
		prowl_log(LOG_ERR, "kill(2) failed: %m");
		goto fail;
	}
	pid1_reap_all();

	if (pid1_mount_tmpfs(PID1_REROOT) != 0 || pid1_copy_self() != 0)
		goto fail;

	argv[0] = (char *)(uintptr_t)PID1_REROOT_INIT;
	argv[1] = (char *)(uintptr_t)"-r";
	argv[2] = NULL;
	execve(PID1_REROOT_INIT, argv, pid1_env);
	prowl_log(LOG_ERR, "cannot exec %s: %m", PID1_REROOT_INIT);
fail:
	prowl_log(LOG_ERR, "reroot failed; going to single user mode");
	pid1_single_user();
}

void
pid1_reroot_phase_two(void)
{
	char init_path[PATH_MAX], *path, *component;
	size_t len;
	int fd;

	/*
	 * Nothing of the old root may stay open, or it cannot be unmounted
	 * and the kernel waits: the console, and the syslog socket.
	 */
	closelog();
	for (fd = 0; fd <= 2; fd++)
		close(fd);

	if (reboot(RB_REROOT) != 0) {
		prowl_log(LOG_ERR, "RB_REROOT failed: %m");
		goto fail;
	}

	if (!pid1_kenv("init_path", init_path, sizeof(init_path))) {
		len = sizeof(init_path);
		if (sysctlbyname("kern.init_path", init_path, &len, NULL, 0)
		    != 0) {
			prowl_log(LOG_ERR,
			    "failed to retrieve kern.init_path: %m");
			goto fail;
		}
	}

	/* The search logic of sys/kern/init_path.c */
	component = init_path;
	while ((path = strsep(&component, ":")) != NULL)
		execl(path, path, (char *)NULL);
	prowl_log(LOG_ERR, "cannot exec init from %s: %m", init_path);
fail:
	prowl_log(LOG_ERR, "reroot failed; going to single user mode");
	pid1_single_user();
}

/*
 * After a reroot the old root's tmpfs copy of us is still mounted.
 */
void
pid1_reroot_cleanup(void)
{
	if (unmount(PID1_REROOT, MNT_FORCE) != 0 && errno != ENOENT &&
	    errno != EINVAL)
		prowl_log(LOG_WARNING, "cannot unmount %s: %m", PID1_REROOT);
}

#define GETTY_PREFIX	"org.midnightbsd.getty."

static job_t *
job_find_by_label_fmt(const char *prefix, const char *name)
{
	char label[PROWL_LABEL_MAX];

	snprintf(label, sizeof(label), "%s%s", prefix, name);
	return (job_find_by_label(label));
}

/*
 * Should /etc/ttys entry typ have a getty?  "on", with a command, and for
 * "onifexists"/"onifconsole" only when the device is there.
 */
static bool
getty_wanted(const struct ttyent *typ)
{
	char devpath[PROWL_PATH_MAX];

	if (typ == NULL || (typ->ty_status & TTY_ON) == 0 ||
	    typ->ty_name == NULL || typ->ty_getty == NULL)
		return (false);
	if ((typ->ty_status & (TTY_IFEXISTS | TTY_IFCONSOLE)) != 0) {
		snprintf(devpath, sizeof(devpath), "%s%s", _PATH_DEV,
		    typ->ty_name);
		if (access(devpath, F_OK) == -1)
			return (false);
	}
	return (true);
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
	char cmd[PROWL_PATH_MAX], term[96];
	char *p, *tok;
	job_t *job;
	int i;

	if (!label_valid(typ->ty_name))
		return (-1);

	if (!getty_wanted(typ))
		return (-1);

	job = job_alloc();
	if (job == NULL)
		return (-1);

	snprintf(job->label, sizeof(job->label), GETTY_PREFIX "%s",
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

/*
 * SIGHUP under init(8): re-read /etc/ttys.  Gettys on lines that were
 * switched off are stopped and not respawned, lines that were switched on
 * get one, and a line that was off and is on again is revived.
 */
void
pid1_rescan_ttys(void)
{
	job_t *job;
	struct ttyent *typ;
	int added = 0, stopped = 0, revived = 0;

	TAILQ_FOREACH(job, &g_jobs, entries) {
		if (strncmp(job->label, GETTY_PREFIX,
		    sizeof(GETTY_PREFIX) - 1) != 0)
			continue;

		setttyent();
		typ = getttynam(job->rc_name);
		if (getty_wanted(typ)) {
			if (!job->enabled) {
				job->enabled = true;
				job->keep_alive.always = true;
				job->run_at_load = true;
				job->restart_hold = false;
				if (job->state == JOB_STATE_STOPPED ||
				    job->state == JOB_STATE_FAILED)
					job_set_state(job, JOB_STATE_LOADED);
				revived++;
			}
			continue;
		}
		if (!job->enabled)
			continue;

		job->enabled = false;
		job->keep_alive.always = false;
		job->run_at_load = false;
		job->restart_hold = false;
		if (job->pid > 0)
			supervisor_stop(job, false);
		else
			job_set_state(job, JOB_STATE_STOPPED);
		stopped++;
	}
	endttyent();

	setttyent();
	while ((typ = getttyent()) != NULL) {
		if (getty_wanted(typ) &&
		    job_find_by_label_fmt(GETTY_PREFIX, typ->ty_name) == NULL &&
		    pid1_add_getty(typ) == 0)
			added++;
	}
	endttyent();

	prowl_log(LOG_NOTICE,
	    "rescanned /etc/ttys: %d added, %d stopped, %d revived",
	    added, stopped, revived);
	if (added > 0 || revived > 0) {
		(void)dag_build();
		dag_schedule_ready();
	}
}
