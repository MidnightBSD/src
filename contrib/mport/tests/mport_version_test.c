#include <sys/cdefs.h>

#include <atf-c.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../libmport/mport.h"

/* Internal libmport symbol (declared in mport_private.h). */
void mport_version_cmp_sqlite(sqlite3_context *, int, sqlite3_value **);

/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullpass -nullret -nullstate -paramuse@*/
/*@-retvalint -retvalother -type -unrecog@*/

static sqlite3 *
open_db_with_cmp(void)
{
	sqlite3 *db = NULL;

	ATF_REQUIRE_EQ(SQLITE_OK, sqlite3_open(":memory:", &db));
	ATF_REQUIRE(db != NULL);
	ATF_REQUIRE_EQ(SQLITE_OK,
	    sqlite3_create_function(db, "mport_version_cmp", 2, SQLITE_UTF8, NULL,
		mport_version_cmp_sqlite, NULL, NULL));

	return db;
}

/* Evaluate a single-column, single-row integer SELECT. */
static int
eval_int(sqlite3 *db, const char *sql)
{
	sqlite3_stmt *stmt = NULL;
	int result;

	ATF_REQUIRE_EQ(SQLITE_OK, sqlite3_prepare_v2(db, sql, -1, &stmt, NULL));
	ATF_REQUIRE_EQ(SQLITE_ROW, sqlite3_step(stmt));
	result = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);

	return result;
}

ATF_TC(version_cmp_null_first);
ATF_TC_HEAD(version_cmp_null_first, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "mport_version_cmp() sorts a NULL version below a real one without crashing");
}
ATF_TC_BODY(version_cmp_null_first, tc)
{
	sqlite3 *db = open_db_with_cmp();

	(void)tc;

	ATF_REQUIRE_EQ(-1, eval_int(db, "SELECT mport_version_cmp(NULL, '1.0')"));

	sqlite3_close(db);
}

ATF_TC(version_cmp_null_second);
ATF_TC_HEAD(version_cmp_null_second, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "mport_version_cmp() sorts a real version above a NULL one without crashing");
}
ATF_TC_BODY(version_cmp_null_second, tc)
{
	sqlite3 *db = open_db_with_cmp();

	(void)tc;

	ATF_REQUIRE_EQ(1, eval_int(db, "SELECT mport_version_cmp('1.0', NULL)"));

	sqlite3_close(db);
}

ATF_TC(version_cmp_null_both);
ATF_TC_HEAD(version_cmp_null_both, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_version_cmp() treats two NULL versions as equal");
}
ATF_TC_BODY(version_cmp_null_both, tc)
{
	sqlite3 *db = open_db_with_cmp();

	(void)tc;

	ATF_REQUIRE_EQ(0, eval_int(db, "SELECT mport_version_cmp(NULL, NULL)"));

	sqlite3_close(db);
}

ATF_TC(version_cmp_non_null);
ATF_TC_HEAD(version_cmp_non_null, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_version_cmp() still orders real versions correctly");
}
ATF_TC_BODY(version_cmp_non_null, tc)
{
	sqlite3 *db = open_db_with_cmp();

	(void)tc;

	ATF_REQUIRE(eval_int(db, "SELECT mport_version_cmp('1.0', '2.0')") < 0);
	ATF_REQUIRE(eval_int(db, "SELECT mport_version_cmp('2.0', '1.0')") > 0);
	ATF_REQUIRE_EQ(0, eval_int(db, "SELECT mport_version_cmp('1.0', '1.0')"));

	sqlite3_close(db);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, version_cmp_null_first);
	ATF_TP_ADD_TC(tp, version_cmp_null_second);
	ATF_TP_ADD_TC(tp, version_cmp_null_both);
	ATF_TP_ADD_TC(tp, version_cmp_non_null);

	return atf_no_error();
}
