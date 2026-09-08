#include <sys/cdefs.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libmport/mport.h"

#define TEST_ROOT "test-index-root"

/* Internal libmport symbols used by the tests. */
int mport_db_do(sqlite3 *, const char *, ...);
void mport_index_moved_entry_free_vec(mportIndexMovedEntry **);
int mport_index_resolve_default_pkgname(mportInstance *, const char *, char **);

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

/*
 * Build an instance whose attached "idx" database mimics a corrupt or
 * hostile downloaded index: the nullable columns are populated with NULL.
 */
static mportInstance *
create_indexed_instance(void)
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

	ATF_REQUIRE_EQ(MPORT_OK, mport_db_do(mport->db, "ATTACH ':memory:' AS idx"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db, "CREATE TABLE idx.mirrors (country text, mirror text)"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"CREATE TABLE idx.moved (port text, moved_to text, why text, date text)"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(
		mport->db, "CREATE TABLE idx.aliases (alias text NOT NULL, pkg text NOT NULL)"));

	mport->flags |= MPORT_INST_HAVE_INDEX;

	return mport;
}

ATF_TC_WITH_CLEANUP(mirror_list_null_columns);
ATF_TC_HEAD(mirror_list_null_columns, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "mport_index_mirror_list tolerates NULL country/mirror columns without crashing");
}
ATF_TC_BODY(mirror_list_null_columns, tc)
{
	mportInstance *mport;
	mportMirrorEntry **vec = NULL;

	(void)tc;

	mport = create_indexed_instance();
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_db_do(mport->db, "INSERT INTO idx.mirrors VALUES (NULL, NULL)"));

	/* Pre-fix this dereferenced NULL inside strlcpy() and crashed. The
	   return value is not MPORT_OK here (the function returns SQLITE_DONE
	   on success), so validate the parsed data instead. */
	(void)mport_index_mirror_list(mport, &vec);

	ATF_REQUIRE(vec != NULL);
	ATF_REQUIRE(vec[0] != NULL);
	ATF_REQUIRE_STREQ("", vec[0]->country);
	ATF_REQUIRE_STREQ("", vec[0]->url);
	ATF_REQUIRE(vec[1] == NULL);

	mport_index_mirror_entry_free_vec(vec);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(mirror_list_null_columns, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(mirror_list_valid_row);
ATF_TC_HEAD(mirror_list_valid_row, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_index_mirror_list still copies non-NULL columns");
}
ATF_TC_BODY(mirror_list_valid_row, tc)
{
	mportInstance *mport;
	mportMirrorEntry **vec = NULL;

	(void)tc;

	mport = create_indexed_instance();
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO idx.mirrors VALUES ('us', 'http://mirror.example.org/pub')"));

	(void)mport_index_mirror_list(mport, &vec);

	ATF_REQUIRE(vec != NULL);
	ATF_REQUIRE(vec[0] != NULL);
	ATF_REQUIRE_STREQ("us", vec[0]->country);
	ATF_REQUIRE_STREQ("http://mirror.example.org/pub", vec[0]->url);
	ATF_REQUIRE(vec[1] == NULL);

	mport_index_mirror_entry_free_vec(vec);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(mirror_list_valid_row, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(moved_lookup_null_columns);
ATF_TC_HEAD(moved_lookup_null_columns, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "mport_moved_lookup tolerates NULL moved_to/why/date columns without crashing");
}
ATF_TC_BODY(moved_lookup_null_columns, tc)
{
	mportInstance *mport;
	mportIndexMovedEntry **vec = NULL;

	(void)tc;

	mport = create_indexed_instance();
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db, "INSERT INTO idx.moved VALUES ('foo', NULL, NULL, NULL)"));

	ATF_REQUIRE_EQ(MPORT_OK, mport_moved_lookup(mport, "foo", &vec));

	ATF_REQUIRE(vec != NULL);
	ATF_REQUIRE(vec[0] != NULL);
	ATF_REQUIRE_STREQ("foo", vec[0]->port);
	ATF_REQUIRE_STREQ("", vec[0]->moved_to);
	ATF_REQUIRE_STREQ("", vec[0]->why);
	ATF_REQUIRE_STREQ("", vec[0]->date);
	ATF_REQUIRE(vec[1] == NULL);

	mport_index_moved_entry_free_vec(vec);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(moved_lookup_null_columns, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(default_version_lookup);
ATF_TC_HEAD(default_version_lookup, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "default version lookup supports new and legacy index schemas");
}
ATF_TC_BODY(default_version_lookup, tc)
{
	mportInstance *mport;
	char *version = NULL;

	(void)tc;

	mport = create_indexed_instance();
	ATF_REQUIRE_EQ(MPORT_OK, mport_index_get_default_version(mport, "python", &version));
	ATF_REQUIRE(version == NULL);

	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"CREATE TABLE idx.default_versions "
		"(name text PRIMARY KEY, version text NOT NULL)"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db, "INSERT INTO idx.default_versions VALUES ('python', '3.12')"));

	ATF_REQUIRE_EQ(MPORT_OK, mport_index_get_default_version(mport, "python", &version));
	ATF_REQUIRE(version != NULL);
	ATF_REQUIRE_STREQ("3.12", version);
	free(version);
	version = NULL;

	ATF_REQUIRE_EQ(MPORT_OK, mport_index_get_default_version(mport, "perl5", &version));
	ATF_REQUIRE(version == NULL);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(default_version_lookup, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(default_package_resolution);
ATF_TC_HEAD(default_package_resolution, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "versioned package names resolve to available default-version packages");
}
ATF_TC_BODY(default_package_resolution, tc)
{
	mportInstance *mport;
	mportPackageMeta *pkg;
	char *resolved = NULL;

	(void)tc;

	mport = create_indexed_instance();
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"CREATE TABLE idx.default_versions "
		"(name text PRIMARY KEY, version text NOT NULL)"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO idx.default_versions VALUES "
		"('python', '3.12'), ('perl5', '5.40'), "
		"('ruby', '3.2'), ('php', '8.3')"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"CREATE TABLE idx.packages "
		"(pkg text NOT NULL, version text NOT NULL, license text NOT NULL, "
		"comment text NOT NULL, bundlefile text NOT NULL, hash text NOT NULL, "
		"type int NOT NULL)"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO idx.packages VALUES "
		"('py311-demo', '1', 'bsd', 'old demo', 'py311-demo-1.mport', 'hash', 0), "
		"('py312-demo', '1', 'bsd', 'demo', 'py312-demo-1.mport', 'hash', 0), "
		"('python312', '3.12', 'psf', 'python', 'python312-3.12.mport', 'hash', 0), "
		"('php83-demo', '1', 'php', 'demo', 'php83-demo-1.mport', 'hash', 0), "
		"('ruby', '3.2', 'ruby', 'ruby', 'ruby-3.2.mport', 'hash', 0), "
		"('ruby32-addon', '1', 'ruby', 'addon', 'ruby32-addon-1.mport', 'hash', 0)"));

	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_index_resolve_default_pkgname(mport, "py311-demo", &resolved));
	ATF_REQUIRE_STREQ("py312-demo", resolved);
	free(resolved);
	resolved = NULL;

	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_index_resolve_default_pkgname(mport, "python311", &resolved));
	ATF_REQUIRE_STREQ("python312", resolved);
	free(resolved);
	resolved = NULL;

	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_index_resolve_default_pkgname(mport, "php82-demo", &resolved));
	ATF_REQUIRE_STREQ("php83-demo", resolved);
	free(resolved);
	resolved = NULL;

	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_index_resolve_default_pkgname(mport, "ruby31-addon", &resolved));
	ATF_REQUIRE_STREQ("ruby32-addon", resolved);
	free(resolved);
	resolved = NULL;

	ATF_REQUIRE_EQ(MPORT_OK, mport_index_resolve_default_pkgname(mport, "ruby31", &resolved));
	ATF_REQUIRE_STREQ("ruby", resolved);
	free(resolved);
	resolved = NULL;

	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_index_resolve_default_pkgname(mport, "p5-Module-Build", &resolved));
	ATF_REQUIRE(resolved == NULL);
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_index_resolve_default_pkgname(mport, "py312-demo", &resolved));
	ATF_REQUIRE(resolved == NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_index_resolve_default_pkgname(mport, "python3", &resolved));
	ATF_REQUIRE(resolved == NULL);

	pkg = mport_pkgmeta_new();
	ATF_REQUIRE(pkg != NULL);
	free(pkg->name);
	pkg->name = strdup("py311-demo");
	ATF_REQUIRE(pkg->name != NULL);
	ATF_REQUIRE_EQ(2, mport_index_check(mport, pkg));
	mport_pkgmeta_free(pkg);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(default_package_resolution, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, mirror_list_null_columns);
	ATF_TP_ADD_TC(tp, mirror_list_valid_row);
	ATF_TP_ADD_TC(tp, moved_lookup_null_columns);
	ATF_TP_ADD_TC(tp, default_version_lookup);
	ATF_TP_ADD_TC(tp, default_package_resolution);

	return atf_no_error();
}
