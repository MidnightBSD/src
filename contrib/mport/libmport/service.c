/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

#include "mport.h"
#include "mport_private.h"
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

int 
mport_start_stop_service(mportInstance *mport, mportPackageMeta *pack, service_action_t action) 
{
    sqlite3_stmt *stmt;
	int ret;
	char *service;
	const unsigned char *rc_script;
	char *handle_rc_script;

    // if handle rc scripts is disabled, we don't need to do anything
	handle_rc_script = mport_setting_get(mport, MPORT_SETTING_HANDLE_RC_SCRIPTS);
	if (getenv("HANDLE_RC_SCRIPTS") == NULL && !mport_check_answer_bool(handle_rc_script))
	    return MPORT_OK;
	
	/* stop any services that might exist; this replaces @stopdaemon */
	if (mport_db_prepare(mport->db, &stmt,
		"select * from assets where data like '/usr/local/etc/rc.d/%%' and type=%i and pkg=%Q",
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
			if (mport_xsystem(mport, "/usr/sbin/service %s quietstart", service) != 0) {
				mport_call_msg_cb(mport, "Unable to start service %s\n", service);
			}
		} else if (action == SERVICE_STOP) {
			if (mport_xsystem(mport, "/usr/sbin/service %s forcestop", service) != 0) {
				mport_call_msg_cb(mport, "Unable to stop service %s\n", service);
			}
		}	
	}

	return MPORT_OK;
}
