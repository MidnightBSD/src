/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The MidnightBSD Project. All rights reserved.
 */

#include <sys/wait.h>

#include <atf-c.h>

static int aged_main(void) __unused;

/* Exercise the real cleanup and signal handler without starting a daemon. */
#define main aged_main
#include "../aged.c"
#undef main

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

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, capability_cleanup);
	ATF_TP_ADD_TC(tp, missing_runtime_files);
	ATF_TP_ADD_TC(tp, signal_defers_cleanup);
	return (atf_no_error());
}
