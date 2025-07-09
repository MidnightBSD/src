/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Lucas Holt
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

#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mport.h"
#include "mport_private.h"

extern char **environ;

static int run_service_cmd(mportInstance *mport, const char *rc_script, char *command);

int 
mport_start_stop_service(mportInstance *mport, mportPackageMeta *pack, service_action_t action) 
{
	sqlite3_stmt *stmt;
	int ret;
	char *service;
	const unsigned char *rc_script;
	char *handle_rc_script;


	// don't turn off services if MPORT_GUI is set.  This can drop someone out of an X session.
	// TODO: we should handle this with triggers eventually.
	if (getenv("MPORT_GUI") != NULL)
		return (MPORT_OK);

	// if handle rc scripts is disabled, we don't need to do anything
	handle_rc_script = mport_setting_get(mport, MPORT_SETTING_HANDLE_RC_SCRIPTS);
	if (getenv("HANDLE_RC_SCRIPTS") == NULL && !mport_check_answer_bool(handle_rc_script))
	    return (MPORT_OK);
	
	/* stop any services that might exist; this replaces @stopdaemon */
	if (mport_db_prepare(mport->db, &stmt,
		"select data from assets where data like '/usr/local/etc/rc.d/%%' and type=%i and pkg=%Q",
		ASSET_FILE, pack->name) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	while (1) {
		ret = sqlite3_step(stmt);
		if (ret != SQLITE_ROW) {
			break;
		}

		rc_script = sqlite3_column_text(stmt, 0);
		if (rc_script == NULL)
			continue;
		service = basename((char *)rc_script);

		if (action == SERVICE_START) {
			if (run_service_cmd(mport, service, "quietstart") != 0) {
				mport_call_msg_cb(mport, "Unable to start service %s\n", service);
			}
		} else if (action == SERVICE_STOP) {
			if (run_service_cmd(mport, service, "forcestop") != 0) {
				mport_call_msg_cb(mport, "Unable to stop service %s\n", service);
			}
		}	
	}

	return (MPORT_OK);
}

static int
run_service_cmd(mportInstance *mport, const char *rc_script, char *command)
{
	int error, pstat;
	pid_t pid;
	const char *argv[4];
	char *service;

	if (rc_script == NULL || command == NULL)
		return (0);

	argv[0] = "service";
	argv[1] = rc_script;
	argv[2] = command;
	argv[3] = NULL;

	service = basename((char *)rc_script);
	if ((error = posix_spawn(&pid, "/usr/sbin/service", NULL, NULL, (char **)argv, environ)) != 0) {
		errno = error;
		mport_call_msg_cb(mport, "Unable to %s service %s\n", command, service);
		return (-1);
	}

	while (waitpid(pid, &pstat, 0) == -1) {
		if (errno != EINTR) {
			return (-1);
		}
	}

	return (WEXITSTATUS(pstat));
}
