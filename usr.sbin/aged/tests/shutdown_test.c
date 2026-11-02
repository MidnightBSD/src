/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The MidnightBSD Project. All rights reserved.
 */

#include <sys/wait.h>

#include <atf-c.h>
#include <unistd.h>

static int aged_main(void) __unused;
static int logging_probe_daemon(int, int);

/* Exercise the real cleanup and signal handler without starting a daemon. */
#define main aged_main
#define daemon logging_probe_daemon
#include "../aged.c"
#undef daemon
#undef main

/* Stop startup after exercising aged's real openlog() call. */
static int
logging_probe_daemon(int nochdir __unused, int noclose __unused)
{

	if (cap_enter() == -1)
		_exit(errno == ENOSYS ? 77 : 1);
	syslog(LOG_NOTICE, "aged shutdown logging probe");
	closelog();
	_exit(0);
}

static void
check_pending_shutdown(int signo)
{
	struct sigaction sa;
	struct pollfd pfd;
	struct timespec timeout = { 0, 0 };
	sigset_t blocked, wait_mask;
	pid_t child;
	int fds[2], result, status;

	ATF_REQUIRE_EQ(pipe(fds), 0);
	ATF_REQUIRE_EQ(write(fds[1], "x", 1), 1);
	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		stop_requested = 0;
		sigemptyset(&blocked);
		sigaddset(&blocked, SIGTERM);
		sigaddset(&blocked, SIGINT);
		if (sigprocmask(SIG_BLOCK, &blocked, &wait_mask) == -1)
			_exit(1);
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = request_shutdown;
		sa.sa_mask = blocked;
		if (sigaction(signo, &sa, NULL) == -1)
			_exit(2);
		sigdelset(&wait_mask, SIGTERM);
		sigdelset(&wait_mask, SIGINT);
		if (raise(signo) != 0)
			_exit(3);
		pfd.fd = fds[0];
		pfd.events = POLLIN;
		result = ppoll(&pfd, 1, &timeout, &wait_mask);
		if (result != 1 && !(result == -1 && errno == EINTR))
			_exit(4);
		/* A ready descriptor must not hide a pending stop signal. */
		_exit(shutdown_requested() == 1 ? 0 : 5);
	}
	ATF_REQUIRE_EQ(waitpid(child, &status, 0), child);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_CHECK_EQ(WEXITSTATUS(status), 0);
	close(fds[0]);
	close(fds[1]);
}

static void
check_cleanup(int missing)
{
	struct sockaddr_un addr;
	cap_rights_t rights;
	char path[1024];
	pid_t child;
	int fd, sock, status;

	ATF_REQUIRE_EQ(mkdir("run", 0700), 0);
	fd = open("run", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	ATF_REQUIRE(fd >= 0);
	ATF_REQUIRE(getcwd(path, sizeof(path)) != NULL);
	ATF_REQUIRE(
	    strlcat(path, "/run/aged.pid", sizeof(path)) < sizeof(path));
	if (!missing) {
		int pidfd = openat(fd, "aged.pid", O_CREAT | O_WRONLY, 0600);

		ATF_REQUIRE(pidfd >= 0);
		close(pidfd);
		sock = socket(AF_UNIX, SOCK_STREAM, 0);
		ATF_REQUIRE(sock >= 0);
		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_UNIX;
		strlcpy(addr.sun_path, "run/aged.sock", sizeof(addr.sun_path));
		ATF_REQUIRE_EQ(bind(sock, (struct sockaddr *)&addr,
				   sizeof(addr)),
		    0);
		close(sock);
	}
	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		run_dir_fd = fd;
		if (sqlite3_open(":memory:", &db) != SQLITE_OK ||
		    sqlite3_prepare_v2(db, "SELECT 1", -1, &stmt_set, NULL) !=
			SQLITE_OK ||
		    sqlite3_prepare_v2(db, "SELECT 1", -1, &stmt_reg, NULL) !=
			SQLITE_OK ||
		    sqlite3_prepare_v2(db, "SELECT 1", -1, &stmt_get, NULL) !=
			SQLITE_OK ||
		    sqlite3_prepare_v2(db, "SELECT 1", -1, &stmt_load_reg,
			NULL) != SQLITE_OK)
			_exit(1);
		current_region = strdup("parental");
		if (current_region == NULL)
			_exit(2);
		cap_rights_init(&rights, CAP_UNLINKAT);
		if (cap_rights_limit(fd, &rights) == -1 || cap_enter() == -1)
			_exit(errno == ENOSYS ? 77 : 3);
		/* Prove absolute unlink is still forbidden. */
		if (unlink(path) != -1 || errno != ECAPMODE)
			_exit(4);
		cleanup();
		if (run_dir_fd != -1 || fcntl(fd, F_GETFD) != -1 ||
		    errno != EBADF)
			_exit(5);
		_exit(0);
	}
	ATF_REQUIRE_EQ(waitpid(child, &status, 0), child);
	ATF_REQUIRE(WIFEXITED(status));
	if (WEXITSTATUS(status) == 77)
		atf_tc_skip("Capsicum is not available in this kernel");
	ATF_REQUIRE_EQ(WEXITSTATUS(status), 0);
	ATF_CHECK_EQ(access("run/aged.sock", F_OK), -1);
	ATF_CHECK_EQ(errno, ENOENT);
	ATF_CHECK_EQ(access("run/aged.pid", F_OK), -1);
	ATF_CHECK_EQ(errno, ENOENT);
	close(fd);
}

ATF_TC_WITHOUT_HEAD(capability_cleanup);
ATF_TC_BODY(capability_cleanup, tc)
{

	check_cleanup(0);
}

ATF_TC_WITHOUT_HEAD(missing_runtime_files);
ATF_TC_BODY(missing_runtime_files, tc)
{

	check_cleanup(1);
}

ATF_TC_WITHOUT_HEAD(signal_defers_cleanup);
ATF_TC_BODY(signal_defers_cleanup, tc)
{
	int fd;

	fd = open("aged.pid", O_CREAT | O_WRONLY, 0600);
	ATF_REQUIRE(fd >= 0);
	close(fd);
	stop_requested = 0;
	request_shutdown(SIGTERM);
	ATF_CHECK_EQ(stop_requested, 1);
	ATF_CHECK_EQ(access("aged.pid", F_OK), 0);
	stop_requested = 0;
	request_shutdown(SIGINT);
	ATF_CHECK_EQ(stop_requested, 1);
	ATF_CHECK_EQ(access("aged.pid", F_OK), 0);
}

ATF_TC_WITHOUT_HEAD(pending_sigterm);
ATF_TC_BODY(pending_sigterm, tc)
{

	check_pending_shutdown(SIGTERM);
}

ATF_TC_WITHOUT_HEAD(pending_sigint);
ATF_TC_BODY(pending_sigint, tc)
{

	check_pending_shutdown(SIGINT);
}

ATF_TC(syslog_before_sandbox);
ATF_TC_HEAD(syslog_before_sandbox, tc)
{

	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(syslog_before_sandbox, tc)
{
	struct sockaddr_un addr;
	struct pollfd pfd;
	char buf[1024];
	pid_t child;
	ssize_t len;
	int sock, status;

	ATF_REQUIRE_EQ(mkdir("var", 0700), 0);
	ATF_REQUIRE_EQ(mkdir("var/run", 0700), 0);
	sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	ATF_REQUIRE(sock >= 0);
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strlcpy(addr.sun_path, "var/run/logpriv", sizeof(addr.sun_path));
	ATF_REQUIRE_EQ(bind(sock, (struct sockaddr *)&addr, sizeof(addr)), 0);
	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		close(sock);
		closelog();
		if (chroot(".") == -1 || chdir("/") == -1)
			_exit(2);
		aged_main();
		_exit(3);
	}
	ATF_REQUIRE_EQ(waitpid(child, &status, 0), child);
	ATF_REQUIRE(WIFEXITED(status));
	if (WEXITSTATUS(status) == 77)
		atf_tc_skip("Capsicum is not available in this kernel");
	ATF_REQUIRE_EQ(WEXITSTATUS(status), 0);
	pfd.fd = sock;
	pfd.events = POLLIN;
	ATF_REQUIRE_EQ(poll(&pfd, 1, 1000), 1);
	len = recv(sock, buf, sizeof(buf) - 1, 0);
	ATF_REQUIRE(len > 0);
	buf[len] = '\0';
	ATF_CHECK(strstr(buf, "aged shutdown logging probe") != NULL);
	close(sock);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, capability_cleanup);
	ATF_TP_ADD_TC(tp, missing_runtime_files);
	ATF_TP_ADD_TC(tp, signal_defers_cleanup);
	ATF_TP_ADD_TC(tp, pending_sigterm);
	ATF_TP_ADD_TC(tp, pending_sigint);
	ATF_TP_ADD_TC(tp, syslog_before_sandbox);
	return (atf_no_error());
}
