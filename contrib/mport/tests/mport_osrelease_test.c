#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libmport/mport.h"

#define TEST_ROOT "test-osrelease-root"
#define MPORT_SETTING_TARGET_OS "target_os"

int mport_db_do(sqlite3 *, const char *, ...);

/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullpass -nullret -nullstate -paramuse@*/
/*@-retvalint -retvalother -type -unrecog@*/

static void
cleanup_test_root(void)
{
	(void)unlink(TEST_ROOT "/var/db/mport/master.db-wal");
	(void)unlink(TEST_ROOT "/var/db/mport/master.db-shm");
	(void)unlink(TEST_ROOT "/var/db/mport/master.db");
	(void)rmdir(TEST_ROOT "/var/db/mport/infrastructure");
	(void)rmdir(TEST_ROOT "/var/db/mport");
	(void)rmdir(TEST_ROOT "/var/db");
	(void)rmdir(TEST_ROOT "/var");
	(void)rmdir(TEST_ROOT);
}

static mportInstance *
create_test_instance(void)
{
	mportInstance *mport;

	cleanup_test_root();
	ATF_REQUIRE_EQ(0, mkdir(TEST_ROOT, 0755));
	ATF_REQUIRE_EQ(0, mkdir(TEST_ROOT "/var", 0755));
	ATF_REQUIRE_EQ(0, mkdir(TEST_ROOT "/var/db", 0755));

	mport = mport_instance_new();
	ATF_REQUIRE(mport != NULL);
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_instance_init(mport, TEST_ROOT, "root", false, MPORT_VQUIET));

	return mport;
}

ATF_TC_WITH_CLEANUP(osrelease_from_settings);
ATF_TC_HEAD(osrelease_from_settings, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_get_osrelease honors MPORT_SETTING_TARGET_OS");
}
ATF_TC_BODY(osrelease_from_settings, tc)
{
	mportInstance *mport;
	char *version;

	mport = create_test_instance();

	ATF_REQUIRE_EQ(MPORT_OK, mport_setting_set(mport, MPORT_SETTING_TARGET_OS, "9.9-TEST"));

	version = mport_get_osrelease(mport);
	ATF_REQUIRE(version != NULL);
	ATF_REQUIRE_STREQ("9.9-TEST", version);

	free(version);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(osrelease_from_settings, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(osrelease_null_instance);
ATF_TC_HEAD(osrelease_null_instance, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_get_osrelease handles NULL instance (falls back to system tools)");
}
ATF_TC_BODY(osrelease_null_instance, tc)
{
	char *version;

	/* On a real MidnightBSD system this will return the OS release,
	 * on Linux/others it might return NULL.
	 * We just ensure it doesn't crash.
	 */
	version = mport_get_osrelease(NULL);
	if (version != NULL) {
		free(version);
	}
}
ATF_TC_CLEANUP(osrelease_null_instance, tc)
{
	(void)tc;
}

ATF_TC_WITH_CLEANUP(osrelease_settings_null);
ATF_TC_HEAD(osrelease_settings_null, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_get_osrelease handles missing MPORT_SETTING_TARGET_OS");
}
ATF_TC_BODY(osrelease_settings_null, tc)
{
	mportInstance *mport;
	char *version;

	mport = create_test_instance();

	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db, "DELETE FROM settings WHERE name=%Q", MPORT_SETTING_TARGET_OS));

	version = mport_get_osrelease(mport);
	if (version != NULL) {
		free(version);
	}

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(osrelease_settings_null, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, osrelease_from_settings);
	ATF_TP_ADD_TC(tp, osrelease_null_instance);
	ATF_TP_ADD_TC(tp, osrelease_settings_null);

	return atf_no_error();
}
