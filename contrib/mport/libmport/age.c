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
 *  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.	IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <string.h>
#include <strings.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <libutil.h>
#include "mport.h"
#include "mport_private.h"

extern char **environ;

/*
 * Does an age bracket from agev_get_age_bracket(3), { lowest, highest } with
 * -1 for "no limit" or "unknown", meet a package's required age?  18,-1 is
 * 18 or older and meets any requirement: adults are never refused.  Other
 * brackets pass when the requirement is no higher than the top of the
 * bracket, so an unknown age (-1,-1) never does.  aged(8) answers -2,-2
 * where its region does not require age checks, which meets any requirement.
 */
bool
mport_age_bracket_allows(const int *bracket, int required_age)
{

	if (bracket[0] == -2 && bracket[1] == -2)
		return true;

	if (bracket[0] >= 18 && bracket[1] == -1)
		return true;

	return required_age <= bracket[1];
}

#if defined(__MidnightBSD__) && __MidnightBSD_version >= 400004
/*
 * Is aged(8) enabled in rc.conf?  Asks sysrc(8) for aged_enable and reads
 * it as rc.subr's checkyesno does.  Unset, or sysrc failing, means no.
 */
static bool
aged_enabled(void)
{
	posix_spawn_file_actions_t actions;
	char *argv[] = { "sysrc", "-qn", "aged_enable", NULL };
	char buf[32];
	size_t used = 0;
	ssize_t n;
	int fds[2];
	int status;
	pid_t pid;
	bool enabled = false;

	if (pipe(fds) == -1)
		return false;

	if (posix_spawn_file_actions_init(&actions) != 0) {
		(void)close(fds[0]);
		(void)close(fds[1]);
		return false;
	}
	if (posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0) !=
		0 ||
	    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0) !=
		0 ||
	    posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO) != 0 ||
	    posix_spawn_file_actions_addclose(&actions, fds[0]) != 0 ||
	    posix_spawn_file_actions_addclose(&actions, fds[1]) != 0 ||
	    posix_spawn(&pid, MPORT_SYSRC_BIN, &actions, NULL, argv, environ) != 0) {
		(void)posix_spawn_file_actions_destroy(&actions);
		(void)close(fds[0]);
		(void)close(fds[1]);
		return false;
	}
	(void)posix_spawn_file_actions_destroy(&actions);
	(void)close(fds[1]);

	/* Read to EOF so sysrc never blocks on a full pipe; keep only the start. */
	for (;;) {
		char discard[64];

		if (used < sizeof(buf) - 1)
			n = read(fds[0], buf + used, sizeof(buf) - 1 - used);
		else
			n = read(fds[0], discard, sizeof(discard));
		if (n == -1 && errno == EINTR)
			continue;
		if (n <= 0)
			break;
		if (used < sizeof(buf) - 1)
			used += (size_t)n;
	}
	buf[used] = '\0';
	(void)close(fds[0]);

	while (waitpid(pid, &status, 0) == -1) {
		if (errno != EINTR)
			return false;
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return false;

	buf[strcspn(buf, "\n")] = '\0';
	enabled = strcasecmp(buf, "yes") == 0 || strcasecmp(buf, "true") == 0 ||
	    strcasecmp(buf, "on") == 0 || strcmp(buf, "1") == 0;

	return enabled;
}
#endif

bool
mport_is_age_verified(mportInstance *mport, mportPackageMeta *pack)
{

#if defined(__MidnightBSD__) && __MidnightBSD_version >= 400004
	char *age_str = NULL;
	int age = 0;

	/* pack is a stub entry: its annotations are still in the bundle */
	if (mport_stub_annotation_get(mport, "age", &age_str) != MPORT_OK) {
		mport_call_msg_cb(
		    mport, "Could not read the age restriction: %s", mport_err_string());
		return false;
	}

	if (age_str == NULL) {
		return true; /* No age annotation means it's appropriate for all ages */
	}

	age = atoi(age_str);
	free(age_str);

	/*
	 * Without a login session (cron, rc scripts) there is no user to check,
	 * so install as do_pre_install() does.  Root set those up.
	 */
	const char *username = getlogin();
	if (username == NULL) {
		/* errno is only set on a lookup failure, not for "no login name" */
		mport_call_msg_cb(mport,
		    "Warning: %s-%s: could not determine the user name, skipping the age check",
		    pack->name, pack->version);
		return true;
	}

	/*
	 * aged(8) not answering is fine where it is disabled (no age checks
	 * are wanted), but where it is enabled the check must not be skipped.
	 */
	int *user_age_bracket = agev_get_age_bracket(username);
	if (user_age_bracket == NULL) {
		if (aged_enabled()) {
			mport_call_msg_cb(mport,
			    "%s-%s: aged is enabled but the age bracket of %s could not be read from it; check that aged is running (service aged status)",
			    pack->name, pack->version, username);
			return false;
		}
		mport_call_msg_cb(mport,
		    "Warning: %s-%s: could not determine the age bracket of %s, skipping the age check",
		    pack->name, pack->version, username);
		return true;
	}

	/* aged is answering, so ages are required: an unset one is refused */
	if (user_age_bracket[0] == -1 && user_age_bracket[1] == -1) {
		mport_call_msg_cb(mport,
		    "%s-%s: the age of %s is not set; as root, set it with \"agectl -a <age> %s\" or \"agectl -b <YYYY-MM-DD> %s\"",
		    pack->name, pack->version, username, username, username);
		free(user_age_bracket);
		return false;
	}

	bool verified = mport_age_bracket_allows(user_age_bracket, age);

	free(user_age_bracket);

	return verified;
#else
	return true;
#endif
}
