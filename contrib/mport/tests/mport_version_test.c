#include <sys/cdefs.h>

#include <atf-c.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../libmport/mport.h"

/* Internal libmport symbols (declared in mport_private.h). */
void mport_version_cmp_sqlite(sqlite3_context *, int, sqlite3_value **);
int mport_version_require_check(const char *, const char *);
int mport_set_err(int, const char *);

/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullpass -nullret -nullstate -paramuse@*/
/*@-retvalint -retvalother -type -unrecog -nullassign@*/

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

ATF_TC(version_cmp_letter_suffix);
ATF_TC_HEAD(version_cmp_letter_suffix, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "a letter after a number is a post-release; a letter starting a component is not");
}
ATF_TC_BODY(version_cmp_letter_suffix, tc)
{
	ATF_REQUIRE_EQ(1, mport_version_cmp("1.0a", "1.0"));
	ATF_REQUIRE_EQ(1, mport_version_cmp("1.0b", "1.0a"));
	ATF_REQUIRE_EQ(1, mport_version_cmp("1.0.2a", "1.0.2"));
	ATF_REQUIRE_EQ(-1, mport_version_cmp("1.0.a1", "1.0"));
	ATF_REQUIRE_EQ(0, mport_version_cmp("1.d2", "1.dev2"));
	ATF_REQUIRE_EQ(0, mport_version_cmp("1.dev2", "1.Development2"));
	ATF_REQUIRE_EQ(0, mport_version_cmp("10a1b2", "10a1.b2"));
}

ATF_TC(version_cmp_prerelease_keywords);
ATF_TC_HEAD(version_cmp_prerelease_keywords, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "pl, snap, alpha, beta, pre and rc sort below the release, in pkg(8) order");
}
ATF_TC_BODY(version_cmp_prerelease_keywords, tc)
{
	ATF_REQUIRE_EQ(-1, mport_version_cmp("1.0rc1", "1.0"));
	ATF_REQUIRE_EQ(-1, mport_version_cmp("1.0.rc1", "1.0"));
	ATF_REQUIRE_EQ(-1, mport_version_cmp("1.0pre1", "1.0"));
	ATF_REQUIRE_EQ(-1, mport_version_cmp("1.0snap1", "1.0"));
	ATF_REQUIRE_EQ(-1, mport_version_cmp("1.0rc1", "1.0rc2"));
	ATF_REQUIRE_EQ(-1, mport_version_cmp("1.0rc1", "1.0a"));
	ATF_REQUIRE_EQ(1, mport_version_cmp("1.0", "1.0rc9"));
	/* examples from pkg_version.c */
	ATF_REQUIRE_EQ(-1, mport_version_cmp("10pl11", "10alpha3"));
	ATF_REQUIRE_EQ(-1, mport_version_cmp("1.0alpha1", "1.0beta1"));
	ATF_REQUIRE_EQ(-1, mport_version_cmp("1.0beta1", "1.0pre1"));
	ATF_REQUIRE_EQ(-1, mport_version_cmp("1.0pre1", "1.0rc1"));
	ATF_REQUIRE_EQ(-1, mport_version_cmp("2.*", "2pl1"));
	ATF_REQUIRE_EQ(-1, mport_version_cmp("2pl1", "2alpha3"));
	ATF_REQUIRE_EQ(-1, mport_version_cmp("2alpha3", "2.9f7"));
	ATF_REQUIRE_EQ(-1, mport_version_cmp("2.9f7", "3"));
}

ATF_TC(version_cmp_separators);
ATF_TC_HEAD(version_cmp_separators, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "missing components are 0, repeated dots collapse, '+' starts a trailing block");
}
ATF_TC_BODY(version_cmp_separators, tc)
{
	ATF_REQUIRE_EQ(0, mport_version_cmp("1.0", "1.0.0"));
	ATF_REQUIRE_EQ(0, mport_version_cmp("10..1", "10.1"));
	ATF_REQUIRE_EQ(1, mport_version_cmp("1.0+1", "1.0"));
	ATF_REQUIRE_EQ(-1, mport_version_cmp("1.0+1", "1.0.1"));
}

ATF_TC(version_cmp_epoch_revision);
ATF_TC_HEAD(version_cmp_epoch_revision, tc)
{
	atf_tc_set_md_var(tc, "descr", "epoch outranks version, which outranks revision");
}
ATF_TC_BODY(version_cmp_epoch_revision, tc)
{
	ATF_REQUIRE_EQ(1, mport_version_cmp("1.0,1", "2.0"));
	ATF_REQUIRE_EQ(-1, mport_version_cmp("1.0", "1.0_1"));
	ATF_REQUIRE_EQ(-1, mport_version_cmp("1.0_1", "1.1"));
	ATF_REQUIRE_EQ(-1, mport_version_cmp("3.0.0", "3.0.0_2"));
}

ATF_TC(version_cmp_large_numbers);
ATF_TC_HEAD(version_cmp_large_numbers, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "numbers beyond 32 bits are not truncated and overflow saturates instead of wrapping");
}
ATF_TC_BODY(version_cmp_large_numbers, tc)
{
	/* 4294967297 == 2^32 + 1, which truncates to 1 in an int */
	ATF_REQUIRE_EQ(-1, mport_version_cmp("1", "4294967297"));
	ATF_REQUIRE_EQ(1, mport_version_cmp("1.0_4294967297", "1.0_1"));
	ATF_REQUIRE_EQ(1, mport_version_cmp("1.0,4294967297", "1.0,1"));
	/* larger than LLONG_MAX: saturates, so it still sorts above */
	ATF_REQUIRE_EQ(-1, mport_version_cmp("1.0", "99999999999999999999"));
	ATF_REQUIRE_EQ(-1, mport_version_cmp("1.0_1", "1.0_99999999999999999999"));
	/* a signed revision is not a number; it reads as revision 0 */
	ATF_REQUIRE_EQ(0, mport_version_cmp("1.0_-1", "1.0"));
}

ATF_TC(require_check_single);
ATF_TC_HEAD(require_check_single, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_version_require_check() honours each single operator");
}
ATF_TC_BODY(require_check_single, tc)
{
	ATF_REQUIRE_EQ(0, mport_version_require_check("1.0", ">=1.0"));
	ATF_REQUIRE_EQ(0, mport_version_require_check("1.1", ">=1.0"));
	ATF_REQUIRE_EQ(-1, mport_version_require_check("0.9", ">=1.0"));
	ATF_REQUIRE_EQ(0, mport_version_require_check("1.1", ">1.0"));
	ATF_REQUIRE_EQ(-1, mport_version_require_check("1.0", ">1.0"));
	ATF_REQUIRE_EQ(0, mport_version_require_check("1.0", "<=1.0"));
	ATF_REQUIRE_EQ(-1, mport_version_require_check("1.1", "<=1.0"));
	ATF_REQUIRE_EQ(0, mport_version_require_check("0.9", "<1.0"));
	ATF_REQUIRE_EQ(-1, mport_version_require_check("1.0", "<1.0"));
	ATF_REQUIRE_EQ(0, mport_version_require_check("1.0", "=1.0"));
	ATF_REQUIRE_EQ(0, mport_version_require_check("1.0", "==1.0"));
	ATF_REQUIRE_EQ(-1, mport_version_require_check("2.0", "==1.0"));
	/* the common "any version" forms in the registry */
	ATF_REQUIRE_EQ(0, mport_version_require_check("1.0", ">0"));
	ATF_REQUIRE_EQ(0, mport_version_require_check("0", ">=0"));
	/* revisions and epochs pass through to mport_version_cmp() */
	ATF_REQUIRE_EQ(0, mport_version_require_check("2.17.1_1,1", ">=2.17.1,1"));
	ATF_REQUIRE_EQ(-1, mport_version_require_check("2.17.1_1", ">=2.17.1,1"));
}

ATF_TC(require_check_compound);
ATF_TC_HEAD(require_check_compound, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "mport_version_require_check() ANDs every bound of a compound requirement");
}
ATF_TC_BODY(require_check_compound, tc)
{
	ATF_REQUIRE_EQ(0, mport_version_require_check("1.0", ">=1.0<4.0"));
	ATF_REQUIRE_EQ(0, mport_version_require_check("2.0", ">=1.0<4.0"));
	ATF_REQUIRE_EQ(-1, mport_version_require_check("0.5", ">=1.0<4.0"));
	ATF_REQUIRE_EQ(-1, mport_version_require_check("4.0", ">=1.0<4.0"));
	ATF_REQUIRE_EQ(-1, mport_version_require_check("5.0", ">=1.0<4.0"));

	ATF_REQUIRE_EQ(0, mport_version_require_check("2.0", ">1.0<=4.0"));
	ATF_REQUIRE_EQ(0, mport_version_require_check("4.0", ">1.0<=4.0"));
	ATF_REQUIRE_EQ(-1, mport_version_require_check("1.0", ">1.0<=4.0"));
	ATF_REQUIRE_EQ(-1, mport_version_require_check("4.1", ">1.0<=4.0"));

	/* bound order does not matter */
	ATF_REQUIRE_EQ(0, mport_version_require_check("2.0", "<4.0>=1.0"));
	ATF_REQUIRE_EQ(-1, mport_version_require_check("0.5", "<4.0>=1.0"));
	ATF_REQUIRE_EQ(-1, mport_version_require_check("5.0", "<4.0>=1.0"));

	/* whitespace between bounds is tolerated */
	ATF_REQUIRE_EQ(0, mport_version_require_check("2.0", ">=1.0 <4.0"));
	ATF_REQUIRE_EQ(-1, mport_version_require_check("0.5", ">=1.0 <4.0"));
	ATF_REQUIRE_EQ(-1, mport_version_require_check("5.0", " >= 1.0 < 4.0 "));

	/* more than two bounds */
	ATF_REQUIRE_EQ(0, mport_version_require_check("2.0", ">=1.0<4.0<3.0"));
	ATF_REQUIRE_EQ(-1, mport_version_require_check("3.5", ">=1.0<4.0<3.0"));
}

ATF_TC(require_check_malformed);
ATF_TC_HEAD(require_check_malformed, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "mport_version_require_check() rejects malformed requirements and sets an error");
}
ATF_TC_BODY(require_check_malformed, tc)
{
	const char *bad[] = { "", " ", "1.0", "|", ">", ">=", "<=1.0>", ">=1.0<", "1.0<2.0",
		"=>1.0", NULL };
	const char **r;

	for (r = bad; *r != NULL; r++) {
		mport_set_err(MPORT_OK, NULL);
		ATF_REQUIRE_MSG(
		    mport_version_require_check("1.0", *r) > 0, "'%s' was not rejected", *r);
		ATF_REQUIRE_MSG(mport_err_code() != MPORT_OK, "'%s' set no error code", *r);
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, version_cmp_null_first);
	ATF_TP_ADD_TC(tp, version_cmp_null_second);
	ATF_TP_ADD_TC(tp, version_cmp_null_both);
	ATF_TP_ADD_TC(tp, version_cmp_non_null);
	ATF_TP_ADD_TC(tp, version_cmp_letter_suffix);
	ATF_TP_ADD_TC(tp, version_cmp_prerelease_keywords);
	ATF_TP_ADD_TC(tp, version_cmp_separators);
	ATF_TP_ADD_TC(tp, version_cmp_epoch_revision);
	ATF_TP_ADD_TC(tp, version_cmp_large_numbers);
	ATF_TP_ADD_TC(tp, require_check_single);
	ATF_TP_ADD_TC(tp, require_check_compound);
	ATF_TP_ADD_TC(tp, require_check_malformed);

	return atf_no_error();
}
