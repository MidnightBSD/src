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

#include <sys/cdefs.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <sqlite3.h> /* for the sqlite3 type in the helper prototypes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libmport/mport.h"

/* Internal libmport symbols (declared in mport_private.h). */
int mport_db_do(sqlite3 *, const char *, ...);
int mport_db_count(sqlite3 *, int *, const char *, ...);
char *mport_registry_immutable_uri(const char *);

/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullderef -nullpass -nullret -nullstate -paramuse@*/
/*@-retvalint -retvalother -type -unrecog@*/

static char test_root[] = "/tmp/mport_instance_test.XXXXXX";

static void
make_root(void)
{
	char path[FILENAME_MAX];

	ATF_REQUIRE(mkdtemp(test_root) != NULL);
	(void)snprintf(path, sizeof(path), "%s/var", test_root);
	ATF_REQUIRE_EQ(0, mkdir(path, 0755));
	(void)snprintf(path, sizeof(path), "%s/var/db", test_root);
	ATF_REQUIRE_EQ(0, mkdir(path, 0755));
}

/* Create a registry as the current user, then take away write access to the
 * file and its directory.  Closing the creating connection checkpoints and
 * removes the WAL side files, which is how a desktop user sees /var/db/mport
 * after root has finished with it; the unlinks only make that explicit. */
static void
make_read_only_registry(void)
{
	mportInstance *mport;
	char path[FILENAME_MAX];

	mport = mport_instance_new();
	ATF_REQUIRE(mport != NULL);
	ATF_REQUIRE_EQ_MSG(MPORT_OK,
	    mport_instance_init(mport, test_root, "root", true, MPORT_VQUIET), "%s",
	    mport_err_string());
	ATF_REQUIRE(!mport->readOnly);
	ATF_REQUIRE_EQ(MPORT_OK, mport_instance_free(mport));

	(void)snprintf(path, sizeof(path), "%s/var/db/mport/master.db-wal", test_root);
	(void)unlink(path);
	(void)snprintf(path, sizeof(path), "%s/var/db/mport/master.db-shm", test_root);
	(void)unlink(path);
	(void)snprintf(path, sizeof(path), "%s/var/db/mport/master.db", test_root);
	ATF_REQUIRE_EQ(0, chmod(path, 0444));
	(void)snprintf(path, sizeof(path), "%s/var/db/mport", test_root);
	ATF_REQUIRE_EQ(0, chmod(path, 0555));
}

static void
restore_and_remove_root(void)
{
	char command[FILENAME_MAX + 32];

	if (strchr(test_root, 'X') != NULL)
		return;
	(void)snprintf(
	    command, sizeof(command), "chmod -R u+w '%s' && rm -rf '%s'", test_root, test_root);
	(void)system(command);
}

ATF_TC_WITH_CLEANUP(readonly_registry_queries);
ATF_TC_HEAD(readonly_registry_queries, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "An unwritable registry without WAL side files opens read-only and answers queries");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(readonly_registry_queries, tc)
{
	mportInstance *mport;
	int count = -1;

	(void)tc;
	make_root();
	make_read_only_registry();

	mport = mport_instance_new();
	ATF_REQUIRE(mport != NULL);
	ATF_REQUIRE_EQ_MSG(MPORT_OK,
	    mport_instance_init(mport, test_root, "root", true, MPORT_VQUIET), "%s",
	    mport_err_string());
	ATF_REQUIRE(mport->readOnly);
	ATF_REQUIRE(mport_require_writable(mport) != MPORT_OK);
	ATF_REQUIRE(strstr(mport_err_string(), "run mport as root") != NULL);
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_db_count(mport->db, &count, "SELECT count(*) FROM packages"));
	ATF_REQUIRE_EQ(0, count);
	/* writes are refused by SQLite rather than silently lost */
	ATF_REQUIRE(mport_db_do(mport->db, "INSERT INTO settings (name, val) VALUES ('x', 'y')") !=
	    MPORT_OK);
	ATF_REQUIRE_EQ(MPORT_OK, mport_instance_free(mport));
}
ATF_TC_CLEANUP(readonly_registry_queries, tc)
{
	(void)tc;
	restore_and_remove_root();
}

ATF_TC_WITH_CLEANUP(readonly_registry_old_schema);
ATF_TC_HEAD(readonly_registry_old_schema, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "A read-only registry with an older schema reports the needed upgrade instead of attempting it");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(readonly_registry_old_schema, tc)
{
	mportInstance *mport;
	char path[FILENAME_MAX];

	(void)tc;
	make_root();
	mport = mport_instance_new();
	ATF_REQUIRE(mport != NULL);
	ATF_REQUIRE_EQ_MSG(MPORT_OK,
	    mport_instance_init(mport, test_root, "root", true, MPORT_VQUIET), "%s",
	    mport_err_string());
	/* leave no WAL side files behind, then mark the registry as older */
	ATF_REQUIRE_EQ(MPORT_OK, mport_db_do(mport->db, "PRAGMA journal_mode=DELETE"));
	ATF_REQUIRE_EQ(MPORT_OK, mport_db_do(mport->db, "PRAGMA user_version=15"));
	ATF_REQUIRE_EQ(MPORT_OK, mport_instance_free(mport));

	(void)snprintf(path, sizeof(path), "%s/var/db/mport/master.db", test_root);
	ATF_REQUIRE_EQ(0, chmod(path, 0444));
	(void)snprintf(path, sizeof(path), "%s/var/db/mport", test_root);
	ATF_REQUIRE_EQ(0, chmod(path, 0555));

	mport = mport_instance_new();
	ATF_REQUIRE(mport != NULL);
	ATF_REQUIRE(mport_instance_init(mport, test_root, "root", true, MPORT_VQUIET) != MPORT_OK);
	ATF_REQUIRE(strstr(mport_err_string(), "run mport as root to upgrade") != NULL);
	ATF_REQUIRE(mport->db == NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_instance_free(mport));
}
ATF_TC_CLEANUP(readonly_registry_old_schema, tc)
{
	(void)tc;
	restore_and_remove_root();
}

ATF_TC_WITH_CLEANUP(writable_registry_unchanged);
ATF_TC_HEAD(writable_registry_unchanged, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "A writable registry still opens read-write and is not flagged read-only");
}
ATF_TC_BODY(writable_registry_unchanged, tc)
{
	mportInstance *mport;

	(void)tc;
	make_root();
	mport = mport_instance_new();
	ATF_REQUIRE(mport != NULL);
	ATF_REQUIRE_EQ_MSG(MPORT_OK,
	    mport_instance_init(mport, test_root, "root", true, MPORT_VQUIET), "%s",
	    mport_err_string());
	ATF_REQUIRE(!mport->readOnly);
	ATF_REQUIRE_EQ(MPORT_OK, mport_require_writable(mport));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db, "INSERT INTO settings (name, val) VALUES ('probe', '1')"));
	ATF_REQUIRE_EQ(MPORT_OK, mport_instance_free(mport));
}
ATF_TC_CLEANUP(writable_registry_unchanged, tc)
{
	(void)tc;
	restore_and_remove_root();
}

ATF_TC(immutable_uri);
ATF_TC_HEAD(immutable_uri, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "The immutable registry URI keeps a single leading slash and encodes URI metacharacters");
}
ATF_TC_BODY(immutable_uri, tc)
{
	char *uri;

	(void)tc;
	/* no chroot: root "" + "/var/db/mport/master.db" starts with "//" */
	uri = mport_registry_immutable_uri("//var/db/mport/master.db");
	ATF_REQUIRE(uri != NULL);
	ATF_REQUIRE_STREQ("file:/var/db/mport/master.db?immutable=1", uri);
	free(uri);

	uri = mport_registry_immutable_uri("/tmp/root//var/db/mport/master.db");
	ATF_REQUIRE(uri != NULL);
	ATF_REQUIRE_STREQ("file:/tmp/root//var/db/mport/master.db?immutable=1", uri);
	free(uri);

	uri = mport_registry_immutable_uri("/odd ?#%/master.db");
	ATF_REQUIRE(uri != NULL);
	ATF_REQUIRE_STREQ("file:/odd %3F%23%25/master.db?immutable=1", uri);
	free(uri);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, immutable_uri);
	ATF_TP_ADD_TC(tp, readonly_registry_queries);
	ATF_TP_ADD_TC(tp, readonly_registry_old_schema);
	ATF_TP_ADD_TC(tp, writable_registry_unchanged);

	return atf_no_error();
}
