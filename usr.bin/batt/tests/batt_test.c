/*-
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

#include <sys/types.h>
#include <sys/wait.h>

#include <atf-c.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct mock_sysctl {
	const char	*name;
	int		 value;
	size_t		 len;
	int		 error;
};

struct run_result {
	int		 exit_status;
	char		*stdout_data;
	char		*stderr_data;
};

static struct mock_sysctl mocks[8];
static size_t nmocks;

static void	add_mock(const char *, int, size_t, int);
static int	batt_main(int, char **);
static void	free_result(struct run_result *);
static char	*read_fd(int);
static void	reset_mocks(void);
static struct run_result run_batt(size_t, const char *const *);

int	sysctlbyname(const char *, void *, size_t *, const void *, size_t);

#define main batt_main
#include "../batt.c"
#undef main

static void
reset_mocks(void)
{

	nmocks = 0;
}

static void
add_mock(const char *name, int value, size_t len, int error)
{

	ATF_REQUIRE_MSG(nmocks < sizeof(mocks) / sizeof(mocks[0]),
	    "too many sysctl mocks");
	mocks[nmocks].name = name;
	mocks[nmocks].value = value;
	mocks[nmocks].len = len;
	mocks[nmocks].error = error;
	nmocks++;
}

int
sysctlbyname(const char *name, void *oldp, size_t *oldlenp, const void *newp,
    size_t newlen)
{
	size_t i;

	(void)newp;
	(void)newlen;

	for (i = 0; i < nmocks; i++) {
		if (strcmp(name, mocks[i].name) != 0)
			continue;
		if (mocks[i].error != 0) {
			errno = mocks[i].error;
			return (-1);
		}
		if (oldlenp != NULL) {
			if (oldp != NULL && *oldlenp >= sizeof(int) &&
			    mocks[i].len >= sizeof(int))
				memcpy(oldp, &mocks[i].value, sizeof(int));
			*oldlenp = mocks[i].len;
		}
		return (0);
	}

	errno = ENOENT;
	return (-1);
}

static char *
read_fd(int fd)
{
	char buf[128];
	char *data;
	ssize_t nread;
	size_t cap, len;

	cap = 1;
	len = 0;
	data = malloc(cap);
	ATF_REQUIRE(data != NULL);
	data[0] = '\0';

	for (;;) {
		nread = read(fd, buf, sizeof(buf));
		ATF_REQUIRE(nread >= 0);
		if (nread == 0)
			break;
		if (len + (size_t)nread + 1 > cap) {
			cap = len + (size_t)nread + 1;
			data = realloc(data, cap);
			ATF_REQUIRE(data != NULL);
		}
		memcpy(data + len, buf, (size_t)nread);
		len += (size_t)nread;
		data[len] = '\0';
	}

	return (data);
}

static struct run_result
run_batt(size_t argc, const char *const *args)
{
	struct run_result result;
	char **argv;
	int outpipe[2], errpipe[2];
	pid_t pid;
	size_t i;
	int status;

	ATF_REQUIRE(pipe(outpipe) == 0);
	ATF_REQUIRE(pipe(errpipe) == 0);

	argv = calloc(argc + 1, sizeof(*argv));
	ATF_REQUIRE(argv != NULL);
	for (i = 0; i < argc; i++) {
		argv[i] = strdup(args[i]);
		ATF_REQUIRE(argv[i] != NULL);
	}

	pid = fork();
	ATF_REQUIRE(pid >= 0);
	if (pid == 0) {
		close(outpipe[0]);
		close(errpipe[0]);
		if (dup2(outpipe[1], STDOUT_FILENO) < 0)
			_exit(127);
		if (dup2(errpipe[1], STDERR_FILENO) < 0)
			_exit(127);
		close(outpipe[1]);
		close(errpipe[1]);
		optind = 1;
		opterr = 1;
		_exit(batt_main((int)argc, argv));
	}

	for (i = 0; i < argc; i++)
		free(argv[i]);
	free(argv);
	close(outpipe[1]);
	close(errpipe[1]);
	ATF_REQUIRE(waitpid(pid, &status, 0) == pid);
	result.stdout_data = read_fd(outpipe[0]);
	result.stderr_data = read_fd(errpipe[0]);
	close(outpipe[0]);
	close(errpipe[0]);

	ATF_REQUIRE(WIFEXITED(status));
	result.exit_status = WEXITSTATUS(status);
	return (result);
}

static void
free_result(struct run_result *result)
{

	free(result->stdout_data);
	free(result->stderr_data);
}

ATF_TC_WITHOUT_HEAD(default_output);
ATF_TC_BODY(default_output, tc)
{
	const char *const argv[] = { "batt", NULL };
	struct run_result result;

	(void)tc;

	reset_mocks();
	add_mock("hw.acpi.battery.life", 88, sizeof(int), 0);
	add_mock("hw.acpi.battery.time", 47, sizeof(int), 0);

	result = run_batt(1, argv);
	ATF_CHECK_EQ(result.exit_status, 0);
	ATF_CHECK_STREQ(result.stdout_data,
	    "88% capacity\n47 minutes remaining\n");
	ATF_CHECK_STREQ(result.stderr_data, "");
	free_result(&result);
}

ATF_TC_WITHOUT_HEAD(concise_all_flags);
ATF_TC_BODY(concise_all_flags, tc)
{
	const char *const argv[] = { "batt", "-cltu", NULL };
	struct run_result result;

	(void)tc;

	reset_mocks();
	add_mock("hw.acpi.battery.life", 75, sizeof(int), 0);
	add_mock("hw.acpi.battery.time", 18, sizeof(int), 0);
	add_mock("hw.acpi.battery.units", 2, sizeof(int), 0);

	result = run_batt(2, argv);
	ATF_CHECK_EQ(result.exit_status, 0);
	ATF_CHECK_STREQ(result.stdout_data, "75 18 2");
	ATF_CHECK_STREQ(result.stderr_data, "");
	free_result(&result);
}

ATF_TC_WITHOUT_HEAD(acline_status);
ATF_TC_BODY(acline_status, tc)
{
	const char *const argv[] = { "batt", "-t", NULL };
	struct run_result result;

	(void)tc;

	reset_mocks();
	add_mock("hw.acpi.battery.time", 0, sizeof(int), 0);
	add_mock("hw.acpi.acline", 1, sizeof(int), 0);

	result = run_batt(2, argv);
	ATF_CHECK_EQ(result.exit_status, 0);
	ATF_CHECK_STREQ(result.stdout_data, "System plugged in\n");
	ATF_CHECK_STREQ(result.stderr_data, "");
	free_result(&result);
}

ATF_TC_WITHOUT_HEAD(extra_argument);
ATF_TC_BODY(extra_argument, tc)
{
	const char *const argv[] = { "batt", "unexpected", NULL };
	struct run_result result;

	(void)tc;

	reset_mocks();
	result = run_batt(2, argv);
	ATF_CHECK_EQ(result.exit_status, 1);
	ATF_CHECK_STREQ(result.stdout_data, "");
	ATF_CHECK_STREQ(result.stderr_data, "usage: batt [-cltu]\n");
	free_result(&result);
}

ATF_TC_WITHOUT_HEAD(short_life_sysctl);
ATF_TC_BODY(short_life_sysctl, tc)
{
	const char *const argv[] = { "batt", "-l", NULL };
	struct run_result result;

	(void)tc;

	reset_mocks();
	add_mock("hw.acpi.battery.life", 55, sizeof(int) - 1, 0);

	result = run_batt(2, argv);
	ATF_CHECK_EQ(result.exit_status, 1);
	ATF_CHECK_STREQ(result.stdout_data, "");
	ATF_CHECK(strstr(result.stderr_data,
	    "unexpected battery life value size") != NULL);
	free_result(&result);
}

ATF_TC_WITHOUT_HEAD(short_acline_sysctl);
ATF_TC_BODY(short_acline_sysctl, tc)
{
	const char *const argv[] = { "batt", "-t", NULL };
	struct run_result result;

	(void)tc;

	reset_mocks();
	add_mock("hw.acpi.battery.time", 0, sizeof(int), 0);
	add_mock("hw.acpi.acline", 1, sizeof(int) - 1, 0);

	result = run_batt(2, argv);
	ATF_CHECK_EQ(result.exit_status, 1);
	ATF_CHECK_STREQ(result.stdout_data, "");
	ATF_CHECK(strstr(result.stderr_data,
	    "unexpected AC line status value size") != NULL);
	free_result(&result);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, default_output);
	ATF_TP_ADD_TC(tp, concise_all_flags);
	ATF_TP_ADD_TC(tp, acline_status);
	ATF_TP_ADD_TC(tp, extra_argument);
	ATF_TP_ADD_TC(tp, short_life_sysctl);
	ATF_TP_ADD_TC(tp, short_acline_sysctl);
	return (atf_no_error());
}
