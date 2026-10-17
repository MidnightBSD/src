#include <sys/cdefs.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libmport/mport.h"

#define TEST_ROOT "test-audit-root"

/* Internal libmport symbols used by the tests. */
int mport_db_do(sqlite3 *, const char *, ...);
int mport_audit_report(mportInstance *, mportPackageMeta *, const char *, bool, char **);

/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullpass -nullret -nullstate -paramuse@*/
/*@-retvalint -retvalother -type -unrecog@*/

/* CVE-0001 affects up to 1.2 and CVE-0002 affects every version. */
#define CVE_FEED                                                                          \
	"[{\"cveId\": \"CVE-0001\", \"description\": \"first\", \"severity\": \"HIGH\", " \
	"\"products\": [{\"version\": \"1.2\"}]}, "                                       \
	"{\"cveId\": \"CVE-0002\", \"description\": \"second\", \"severity\": \"LOW\", "  \
	"\"products\": [{\"version\": \"*\"}]}]"

/* CVE-0001 only, so a newer version fixes everything listed. */
#define CVE_FEED_FIXABLE                                                                  \
	"[{\"cveId\": \"CVE-0001\", \"description\": \"first\", \"severity\": \"HIGH\", " \
	"\"products\": [{\"version\": \"1.2\"}]}]"

/*
 * Products from other vendors and platforms mixed into each CVE, the way the
 * feed lists them: CVE-0001 and CVE-0002 match demo only through another
 * product, CVE-0003 lists demo (in another case) up to 1.2, CVE-0004 has an
 * entry without names, and CVE-0005 lists demo up to 1.2 next to another
 * product whose version would mark demo-1.5 as still affected.
 */
#define CVE_FEED_PRODUCTS                                                                     \
	"[{\"cveId\": \"CVE-0001\", \"products\": ["                                          \
	"{\"vendorName\": \"redhat\", \"name\": \"enterprise_linux\", \"version\": \"10\"}, " \
	"{\"vendorName\": \"demo\", \"name\": \"demo\", \"version\": \"-\"}]}, "              \
	"{\"cveId\": \"CVE-0002\", \"products\": ["                                           \
	"{\"vendorName\": \"demo\", \"name\": \"other\", \"version\": \"*\"}]}, "             \
	"{\"cveId\": \"CVE-0003\", \"products\": ["                                           \
	"{\"vendorName\": \"Demo\", \"name\": \"DEMO\", \"version\": \"1.2\"}]}, "            \
	"{\"cveId\": \"CVE-0004\", \"products\": [{\"version\": \"1.1\"}]}, "                 \
	"{\"cveId\": \"CVE-0005\", \"products\": ["                                           \
	"{\"vendorName\": \"netapp\", \"name\": \"ontap\", \"version\": \"9\"}, "             \
	"{\"vendorName\": \"demo\", \"name\": \"demo\", \"version\": \"1.2\"}]}]"

#define DEMO_CPE "cpe:2.3:a:demo:demo:1.0:::::midnightbsd4:x64:0"

/*
 * CVE-0001 plus an entry without a cveId that affects every version, so only
 * CVE-0001 can be listed.
 */
#define CVE_FEED_NO_ID                                                         \
	"[{\"cveId\": \"CVE-0001\", \"products\": [{\"version\": \"1.2\"}]}, " \
	"{\"description\": \"no id\", \"products\": [{\"version\": \"*\"}]}]"

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

/* Build an instance whose attached "idx" database offers demo at indexVersion. */
static mportInstance *
create_indexed_instance(const char *indexVersion)
{
	mportInstance *mport;

	cleanup_test_root();
	ATF_REQUIRE_EQ(0, mkdir(TEST_ROOT, 0755));
	ATF_REQUIRE_EQ(0, mkdir(TEST_ROOT "/var", 0755));
	ATF_REQUIRE_EQ(0, mkdir(TEST_ROOT "/var/db", 0755));

	mport = mport_instance_new();
	ATF_REQUIRE(mport != NULL);
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_instance_init(mport, TEST_ROOT, "root", false, MPORT_VNORMAL));

	ATF_REQUIRE_EQ(MPORT_OK, mport_db_do(mport->db, "ATTACH ':memory:' AS idx"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(
		mport->db, "CREATE TABLE idx.aliases (alias text NOT NULL, pkg text NOT NULL)"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"CREATE TABLE idx.packages "
		"(pkg text NOT NULL, version text NOT NULL, license text NOT NULL, "
		"comment text NOT NULL, bundlefile text NOT NULL, hash text NOT NULL, "
		"type int NOT NULL)"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO idx.packages VALUES "
		"('demo', %Q, 'bsd', 'demo', 'demo.mport', 'hash', 0)",
		indexVersion));

	mport->flags |= MPORT_INST_HAVE_INDEX;
	mport->verbosity = MPORT_VNORMAL;

	return mport;
}

static mportPackageMeta *
installed_demo(void)
{
	mportPackageMeta *pkg = mport_pkgmeta_new();

	ATF_REQUIRE(pkg != NULL);
	pkg->name = strdup("demo");
	pkg->version = strdup("1.0");
	pkg->origin = strdup("devel/demo");
	ATF_REQUIRE(pkg->name != NULL && pkg->version != NULL && pkg->origin != NULL);

	return pkg;
}

static char *
report_cpe(mportInstance *mport, const char *cpe, const char *feed)
{
	mportPackageMeta *pkg = installed_demo();
	char *out = NULL;

	if (cpe != NULL) {
		pkg->cpe = strdup(cpe);
		ATF_REQUIRE(pkg->cpe != NULL);
	}

	ATF_REQUIRE_EQ(MPORT_OK, mport_audit_report(mport, pkg, feed, false, &out));
	ATF_REQUIRE(out != NULL);
	mport_pkgmeta_free(pkg);

	return out;
}

static char *
report(mportInstance *mport, const char *feed)
{
	mportPackageMeta *pkg = installed_demo();
	char *out = NULL;

	ATF_REQUIRE_EQ(MPORT_OK, mport_audit_report(mport, pkg, feed, false, &out));
	ATF_REQUIRE(out != NULL);
	mport_pkgmeta_free(pkg);

	return out;
}

ATF_TC_WITH_CLEANUP(index_fixes_all);
ATF_TC_HEAD(index_fixes_all, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "audit reports the index version that fixes every listed CVE");
}
ATF_TC_BODY(index_fixes_all, tc)
{
	mportInstance *mport;
	char *out;

	(void)tc;

	mport = create_indexed_instance("1.5");
	out = report(mport, CVE_FEED_FIXABLE);

	ATF_REQUIRE_MATCH("^demo-1\\.0 is vulnerable:", out);
	ATF_REQUIRE(strstr(out, "CVE-0001\n") != NULL);
	ATF_REQUIRE(strstr(out, "Fixed in: demo-1.5\n") != NULL);
	ATF_REQUIRE(strstr(out, "Not fixed in:") == NULL);
	ATF_REQUIRE(strstr(out, "Upgrade to demo-1.5 to fix this vulnerability.\n") != NULL);

	free(out);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(index_fixes_all, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(index_fixes_some);
ATF_TC_HEAD(index_fixes_some, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "audit marks which CVEs the newer index version still carries");
}
ATF_TC_BODY(index_fixes_some, tc)
{
	mportInstance *mport;
	char *out;

	(void)tc;

	mport = create_indexed_instance("1.5");
	out = report(mport, CVE_FEED);

	ATF_REQUIRE(strstr(out,
			"CVE-0001\nDescription: first\nSeverity: HIGH\n"
			"Fixed in: demo-1.5\n") != NULL);
	ATF_REQUIRE(strstr(out,
			"CVE-0002\nDescription: second\nSeverity: LOW\n"
			"Not fixed in: demo-1.5\n") != NULL);
	ATF_REQUIRE(strstr(out,
			"demo-1.5 is available in the index but is still affected by 1 of 2 "
			"vulnerabilities.\n") != NULL);
	ATF_REQUIRE(strstr(out, "Upgrade to") == NULL);

	free(out);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(index_fixes_some, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(index_not_newer);
ATF_TC_HEAD(index_not_newer, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "audit says so when the index offers nothing newer than installed");
}
ATF_TC_BODY(index_not_newer, tc)
{
	mportInstance *mport;
	char *out;

	(void)tc;

	mport = create_indexed_instance("1.0");
	out = report(mport, CVE_FEED);

	ATF_REQUIRE(strstr(out, "CVE-0001\n") != NULL);
	ATF_REQUIRE(strstr(out, "CVE-0002\n") != NULL);
	ATF_REQUIRE(strstr(out, "fixed in:") == NULL);
	ATF_REQUIRE(strstr(out, "Fixed in:") == NULL);
	ATF_REQUIRE(strstr(out, "No newer version of demo is available in the index.\n") != NULL);

	free(out);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(index_not_newer, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(no_index);
ATF_TC_HEAD(no_index, tc)
{
	atf_tc_set_md_var(tc, "descr", "audit omits fix status when the index is not loaded");
}
ATF_TC_BODY(no_index, tc)
{
	mportInstance *mport;
	char *out;

	(void)tc;

	mport = create_indexed_instance("1.5");
	mport->flags &= ~MPORT_INST_HAVE_INDEX;
	out = report(mport, CVE_FEED);

	ATF_REQUIRE(strstr(out, "CVE-0001\n") != NULL);
	ATF_REQUIRE(strstr(out, "fixed in:") == NULL);
	ATF_REQUIRE(strstr(out, "Fixed in:") == NULL);
	ATF_REQUIRE(strstr(out, "index") == NULL);

	free(out);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(no_index, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(quiet_unchanged);
ATF_TC_HEAD(quiet_unchanged, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "quiet audit still prints only the vulnerable package name-version");
}
ATF_TC_BODY(quiet_unchanged, tc)
{
	mportInstance *mport;
	char *out;

	(void)tc;

	mport = create_indexed_instance("1.5");
	mport->verbosity = MPORT_VQUIET;
	out = report(mport, CVE_FEED);

	ATF_REQUIRE_STREQ("demo-1.0\n", out);

	free(out);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(quiet_unchanged, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(not_vulnerable);
ATF_TC_HEAD(not_vulnerable, tc)
{
	atf_tc_set_md_var(tc, "descr", "audit prints nothing for a version past every CVE");
}
ATF_TC_BODY(not_vulnerable, tc)
{
	mportInstance *mport;
	char *out;

	(void)tc;

	mport = create_indexed_instance("1.5");
	out = report(mport, "[{\"cveId\": \"CVE-0003\", \"products\": [{\"version\": \"0.9\"}]}]");

	ATF_REQUIRE_STREQ("", out);

	free(out);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(not_vulnerable, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(missing_cve_id);
ATF_TC_HEAD(missing_cve_id, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "audit ignores CVE entries without an id in the listing and counts");
}
ATF_TC_BODY(missing_cve_id, tc)
{
	mportInstance *mport;
	char *out;

	(void)tc;

	mport = create_indexed_instance("1.5");
	out = report(mport, CVE_FEED_NO_ID);

	ATF_REQUIRE(strstr(out, "CVE-0001\nFixed in: demo-1.5\n") != NULL);
	ATF_REQUIRE(strstr(out, "no id") == NULL);
	ATF_REQUIRE(strstr(out, "Not fixed in:") == NULL);
	ATF_REQUIRE(strstr(out, "Upgrade to demo-1.5 to fix this vulnerability.\n") != NULL);
	free(out);

	/* with only the id-less entry, nothing is reported in either verbosity */
	out = report(mport, "[{\"products\": [{\"version\": \"*\"}]}]");
	ATF_REQUIRE_STREQ("", out);
	free(out);

	mport->verbosity = MPORT_VQUIET;
	out = report(mport, "[{\"products\": [{\"version\": \"*\"}]}]");
	ATF_REQUIRE_STREQ("", out);
	free(out);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(missing_cve_id, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(product_filter);
ATF_TC_HEAD(product_filter, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "audit compares only the versions of the product the installed CPE names");
}
ATF_TC_BODY(product_filter, tc)
{
	mportInstance *mport;
	char *out;

	(void)tc;

	mport = create_indexed_instance("1.5");
	out = report_cpe(mport, DEMO_CPE, CVE_FEED_PRODUCTS);

	ATF_REQUIRE(strstr(out, "CVE-0001") == NULL);
	ATF_REQUIRE(strstr(out, "CVE-0002") == NULL);
	ATF_REQUIRE(strstr(out, "CVE-0003\nFixed in: demo-1.5\n") != NULL);
	ATF_REQUIRE(strstr(out, "CVE-0004\nFixed in: demo-1.5\n") != NULL);
	ATF_REQUIRE(strstr(out, "CVE-0005\nFixed in: demo-1.5\n") != NULL);
	ATF_REQUIRE(strstr(out, "Upgrade to demo-1.5 to fix these vulnerabilities.\n") != NULL);
	free(out);

	/* a CPE with a quoted ':' still names the product */
	out = report_cpe(mport, "cpe:2.3:a:demo:de\\:mo:1.0:::::midnightbsd4:x64:0",
	    "[{\"cveId\": \"CVE-0006\", \"products\": ["
	    "{\"vendorName\": \"demo\", \"name\": \"de:mo\", \"version\": \"1.2\"}]}, "
	    "{\"cveId\": \"CVE-0007\", \"products\": ["
	    "{\"vendorName\": \"demo\", \"name\": \"de\", \"version\": \"1.2\"}]}]");
	ATF_REQUIRE(strstr(out, "CVE-0006\n") != NULL);
	ATF_REQUIRE(strstr(out, "CVE-0007") == NULL);
	free(out);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(product_filter, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(product_filter_without_cpe);
ATF_TC_HEAD(product_filter_without_cpe, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "audit compares every product when the installed CPE gives no vendor and product");
}
ATF_TC_BODY(product_filter_without_cpe, tc)
{
	mportInstance *mport;
	char *out;

	(void)tc;

	mport = create_indexed_instance("1.5");

	out = report_cpe(mport, NULL, CVE_FEED_PRODUCTS);
	ATF_REQUIRE(strstr(out, "CVE-0001\n") != NULL);
	ATF_REQUIRE(strstr(out, "CVE-0002\n") != NULL);
	ATF_REQUIRE(strstr(out, "CVE-0005\nNot fixed in: demo-1.5\n") != NULL);
	free(out);

	out = report_cpe(mport, "cpe:2.3:a:*:*:1.0:::::midnightbsd4:x64:0", CVE_FEED_PRODUCTS);
	ATF_REQUIRE(strstr(out, "CVE-0001\n") != NULL);
	free(out);

	out = report_cpe(mport, "cpe:/a:demo:demo:1.0", CVE_FEED_PRODUCTS);
	ATF_REQUIRE(strstr(out, "CVE-0001\n") != NULL);
	free(out);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(product_filter_without_cpe, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(bad_json);
ATF_TC_HEAD(bad_json, tc)
{
	atf_tc_set_md_var(tc, "descr", "audit fails without output on an unparsable feed");
}
ATF_TC_BODY(bad_json, tc)
{
	mportInstance *mport;
	mportPackageMeta *pkg;
	char *out = NULL;

	(void)tc;

	mport = create_indexed_instance("1.5");
	pkg = installed_demo();

	ATF_REQUIRE_EQ(MPORT_ERR_FATAL, mport_audit_report(mport, pkg, "[{", false, &out));
	ATF_REQUIRE(out == NULL);

	mport_pkgmeta_free(pkg);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(bad_json, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, index_fixes_all);
	ATF_TP_ADD_TC(tp, index_fixes_some);
	ATF_TP_ADD_TC(tp, index_not_newer);
	ATF_TP_ADD_TC(tp, no_index);
	ATF_TP_ADD_TC(tp, quiet_unchanged);
	ATF_TP_ADD_TC(tp, not_vulnerable);
	ATF_TP_ADD_TC(tp, missing_cve_id);
	ATF_TP_ADD_TC(tp, product_filter);
	ATF_TP_ADD_TC(tp, product_filter_without_cpe);
	ATF_TP_ADD_TC(tp, bad_json);

	return atf_no_error();
}
