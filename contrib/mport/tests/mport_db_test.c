#include <sys/cdefs.h>

#include <atf-c.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../libmport/mport.h"

/* Internal libmport symbols (declared in mport_private.h). */
int mport_db_do(sqlite3 *, const char *, ...);
int mport_db_prepare(sqlite3 *, sqlite3_stmt **, const char *, ...);
int mport_db_count(sqlite3 *, int *, const char *, ...);

/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullpass -nullret -nullstate -paramuse@*/
/*@-retvalint -retvalother -type -unrecog@*/

static sqlite3 *
open_db(void)
{
	sqlite3 *db = NULL;

	ATF_REQUIRE_EQ(SQLITE_OK, sqlite3_open(":memory:", &db));
	ATF_REQUIRE(db != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_db_do(db, "CREATE TABLE t (x integer)"));
	ATF_REQUIRE_EQ(MPORT_OK, mport_db_do(db, "INSERT INTO t VALUES (1), (2), (3)"));

	return db;
}

ATF_TC(count_normal);
ATF_TC_HEAD(count_normal, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_db_count returns the row count for a COUNT query");
}
ATF_TC_BODY(count_normal, tc)
{
	sqlite3 *db = open_db();
	int count = -1;

	(void)tc;

	ATF_REQUIRE_EQ(MPORT_OK, mport_db_count(db, &count, "SELECT count(*) FROM t"));
	ATF_REQUIRE_EQ(3, count);

	sqlite3_close(db);
}

ATF_TC(count_empty_table);
ATF_TC_HEAD(count_empty_table, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_db_count reports 0 (not an error) for an empty table");
}
ATF_TC_BODY(count_empty_table, tc)
{
	sqlite3 *db = open_db();
	int count = -1;

	(void)tc;

	ATF_REQUIRE_EQ(MPORT_OK, mport_db_do(db, "DELETE FROM t"));
	ATF_REQUIRE_EQ(MPORT_OK, mport_db_count(db, &count, "SELECT count(*) FROM t"));
	ATF_REQUIRE_EQ(0, count);

	sqlite3_close(db);
}

ATF_TC(count_no_row);
ATF_TC_HEAD(count_no_row, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_db_count fails and zeroes *count when the query yields no row");
}
ATF_TC_BODY(count_no_row, tc)
{
	sqlite3 *db = open_db();
	int count = 12345; /* sentinel; must be overwritten */

	(void)tc;

	/* Prepares successfully but steps to SQLITE_DONE with no row. Pre-fix
	   this returned MPORT_OK leaving *count at the sentinel, which callers
	   used as an allocation size. */
	ATF_REQUIRE(mport_db_count(db, &count, "SELECT 42 WHERE 1 = 0") != MPORT_OK);
	ATF_REQUIRE_EQ(0, count);

	sqlite3_close(db);
}

ATF_TC(count_bad_sql);
ATF_TC_HEAD(count_bad_sql, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_db_count fails and zeroes *count on a prepare error");
}
ATF_TC_BODY(count_bad_sql, tc)
{
	sqlite3 *db = open_db();
	int count = 999; /* sentinel */

	(void)tc;

	ATF_REQUIRE(mport_db_count(db, &count, "SELECT count(*) FROM no_such_table") != MPORT_OK);
	ATF_REQUIRE_EQ(0, count);

	sqlite3_close(db);
}

ATF_TC(prepare_nulls_stmt_on_error);
ATF_TC_HEAD(prepare_nulls_stmt_on_error, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "mport_db_prepare leaves *stmt NULL on failure so error handlers may finalize it");
}
ATF_TC_BODY(prepare_nulls_stmt_on_error, tc)
{
	sqlite3 *db = open_db();
	sqlite3_stmt *stmt = (sqlite3_stmt *)0xdeadbeef; /* sentinel */

	(void)tc;

	ATF_REQUIRE(mport_db_prepare(db, &stmt, "SELECT * FROM no_such_table") != MPORT_OK);
	ATF_REQUIRE(stmt == NULL);
	/* Contract: finalizing the (NULL) statement is safe. */
	sqlite3_finalize(stmt);

	sqlite3_close(db);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, count_normal);
	ATF_TP_ADD_TC(tp, count_empty_table);
	ATF_TP_ADD_TC(tp, count_no_row);
	ATF_TP_ADD_TC(tp, count_bad_sql);
	ATF_TP_ADD_TC(tp, prepare_nulls_stmt_on_error);

	return atf_no_error();
}
