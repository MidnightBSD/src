#include <sys/cdefs.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libmport/mport.h"
#include "../libmport/mport_private.h"

/* SPLINT_SKIP_FILE: Splint cannot parse/model ATF test macros and fixture setup. */
/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullpass -nullret -nullstate -paramuse@*/
/*@-retvalint -retvalother -type -unrecog@*/

#define TEST_ROOT_TEMPLATE "/tmp/mport-precheck-test-root.XXXXXX"
#define CONFLICT_FILE "/usr/local/share/licenses/clamav-1.5.2,1/catalog.mk"

static char test_root[PATH_MAX];

static const char *
test_path(const char *suffix)
{
	static char paths[4][PATH_MAX];
	static unsigned int next_path;
	char *path;

	path = paths[next_path++ % 4];
	(void)snprintf(path, PATH_MAX, "%s%s", test_root, suffix);
	return path;
}

static void
cleanup_test_root(void)
{
	int cwd_fd;

	cwd_fd = open(".", O_RDONLY | O_DIRECTORY);
	if (test_root[0] != '\0' && access(test_root, F_OK) == 0)
		(void)mport_rmtree(test_root);
	if (cwd_fd >= 0) {
		(void)fchdir(cwd_fd);
		(void)close(cwd_fd);
	}
	test_root[0] = '\0';
}

static mportInstance *
create_test_instance(void)
{
	mportInstance *mport;

	cleanup_test_root();
	(void)strlcpy(test_root, TEST_ROOT_TEMPLATE, sizeof(test_root));
	ATF_REQUIRE(mkdtemp(test_root) != NULL);
	ATF_REQUIRE_EQ(0, mkdir(test_path("/var"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/var/db"), 0755));

	mport = mport_instance_new();
	ATF_REQUIRE(mport != NULL);
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_instance_init(mport, test_root, "root", false, MPORT_VQUIET));

	/* the file conflict check reads the incoming asset list from the stub db */
	ATF_REQUIRE_EQ(MPORT_OK, mport_attach_stub_db(mport->db, test_root));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"CREATE TABLE stub.assets (pkg text NOT NULL, type int NOT NULL, data text, "
		"checksum text, owner text, grp text, mode text)"));

	return mport;
}

static void
create_conflict_file(void)
{
	int fd;

	ATF_REQUIRE_EQ(0, mkdir(test_path("/usr"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/usr/local"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/usr/local/share"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/usr/local/share/licenses"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/usr/local/share/licenses/clamav-1.5.2,1"), 0755));

	fd = open(test_path(CONFLICT_FILE), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	ATF_REQUIRE(fd >= 0);
	ATF_REQUIRE_EQ(1, write(fd, "x", 1));
	ATF_REQUIRE_EQ(0, close(fd));
}

static void
insert_master_asset(mportInstance *mport, const char *pkg, const char *data)
{
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO assets (pkg, type, data, checksum, owner, grp, mode) "
		"VALUES (%Q, %d, %Q, '', NULL, NULL, NULL)",
		pkg, ASSET_FILE, data));
}

static void
insert_stub_asset(mportInstance *mport, const char *pkg, const char *data)
{
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO stub.assets (pkg, type, data, checksum, owner, grp, mode) "
		"VALUES (%Q, %d, %Q, '', NULL, NULL, NULL)",
		pkg, ASSET_FILE, data));
}

static mportPackageMeta *
create_pack(const char *name)
{
	mportPackageMeta *pack;

	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	pack->name = strdup(name);
	pack->version = strdup("1.5.2,1");
	pack->prefix = strdup("/usr/local");
	pack->origin = strdup("security/clamav");
	pack->lang = strdup("");
	pack->type = MPORT_TYPE_APP;
	ATF_REQUIRE(pack->name != NULL);
	ATF_REQUIRE(pack->version != NULL);
	ATF_REQUIRE(pack->prefix != NULL);
	ATF_REQUIRE(pack->origin != NULL);
	ATF_REQUIRE(pack->lang != NULL);

	return pack;
}

static void
insert_master_package(mportInstance *mport, const char *name, const char *os_release)
{
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO packages (pkg, version, origin, prefix, lang, os_release) VALUES "
		"(%Q, '1.5.2,1', 'security/clamav', '/usr/local', '', %Q)",
		name, os_release));
}

/*
 * The install path decides whether an installed copy is stale by comparing the
 * os_release the registry hands back against the running system's, so a search
 * that dropped the column would make it delete the wrong package.
 */
ATF_TC_WITH_CLEANUP(search_master_returns_os_release);
ATF_TC_HEAD(search_master_returns_os_release, tc)
{
	atf_tc_set_md_var(tc, "descr", "package searches report the recorded os_release");
}
ATF_TC_BODY(search_master_returns_os_release, tc)
{
	mportInstance *mport;
	mportPackageMeta **found;
	char *system_os_release;

	(void)tc;

	mport = create_test_instance();
	system_os_release = mport_get_osrelease(mport);
	ATF_REQUIRE(system_os_release != NULL);

	insert_master_package(mport, "clamav", "0.0-OLD");
	insert_master_package(mport, "current", system_os_release);

	found = NULL;
	ATF_REQUIRE_EQ(MPORT_OK, mport_pkgmeta_search_master(mport, &found, "pkg=%Q", "clamav"));
	ATF_REQUIRE(found != NULL && found[0] != NULL);
	ATF_REQUIRE(found[0]->os_release != NULL);
	ATF_REQUIRE_STREQ("0.0-OLD", found[0]->os_release);
	ATF_REQUIRE(strcmp(found[0]->os_release, system_os_release) != 0);
	mport_pkgmeta_vec_free(found);

	found = NULL;
	ATF_REQUIRE_EQ(MPORT_OK, mport_pkgmeta_search_master(mport, &found, "pkg=%Q", "current"));
	ATF_REQUIRE(found != NULL && found[0] != NULL);
	ATF_REQUIRE(found[0]->os_release != NULL);
	ATF_REQUIRE_STREQ(system_os_release, found[0]->os_release);
	mport_pkgmeta_vec_free(found);

	free(system_os_release);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(search_master_returns_os_release, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(file_conflict_allows_same_package);
ATF_TC_HEAD(file_conflict_allows_same_package, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "a file owned by the same package (installed under another os_release) is not a conflict");
}
ATF_TC_BODY(file_conflict_allows_same_package, tc)
{
	mportInstance *mport;
	mportPackageMeta *pack;

	(void)tc;

	mport = create_test_instance();
	create_conflict_file();

	/* the copy registered under the previous OS release owns the file */
	insert_master_asset(mport, "clamav", CONFLICT_FILE);
	insert_stub_asset(mport, "clamav", CONFLICT_FILE);

	pack = create_pack("clamav");
	ATF_REQUIRE_MSG(
	    mport_check_preconditions(mport, pack, MPORT_PRECHECK_FILE_CONFLICTS) == MPORT_OK, "%s",
	    mport_err_string());

	mport_pkgmeta_free(pack);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(file_conflict_allows_same_package, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(file_conflict_rejects_other_package);
ATF_TC_HEAD(file_conflict_rejects_other_package, tc)
{
	atf_tc_set_md_var(tc, "descr", "a file owned by a different package is a conflict");
}
ATF_TC_BODY(file_conflict_rejects_other_package, tc)
{
	mportInstance *mport;
	mportPackageMeta *pack;

	(void)tc;

	mport = create_test_instance();
	create_conflict_file();

	insert_master_asset(mport, "clamav-milter", CONFLICT_FILE);
	insert_stub_asset(mport, "clamav", CONFLICT_FILE);

	pack = create_pack("clamav");
	ATF_REQUIRE(
	    mport_check_preconditions(mport, pack, MPORT_PRECHECK_FILE_CONFLICTS) != MPORT_OK);
	ATF_REQUIRE_MSG(
	    strstr(mport_err_string(), "owned by clamav-milter") != NULL, "%s", mport_err_string());

	mport_pkgmeta_free(pack);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(file_conflict_rejects_other_package, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(file_conflict_rejects_unmanaged_file);
ATF_TC_HEAD(file_conflict_rejects_unmanaged_file, tc)
{
	atf_tc_set_md_var(tc, "descr", "a file owned by no package at all is still a conflict");
}
ATF_TC_BODY(file_conflict_rejects_unmanaged_file, tc)
{
	mportInstance *mport;
	mportPackageMeta *pack;

	(void)tc;

	mport = create_test_instance();
	create_conflict_file();

	insert_stub_asset(mport, "clamav", CONFLICT_FILE);

	pack = create_pack("clamav");
	ATF_REQUIRE(
	    mport_check_preconditions(mport, pack, MPORT_PRECHECK_FILE_CONFLICTS) != MPORT_OK);
	ATF_REQUIRE_MSG(
	    strstr(mport_err_string(), "not managed by mport") != NULL, "%s", mport_err_string());

	mport_pkgmeta_free(pack);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(file_conflict_rejects_unmanaged_file, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, search_master_returns_os_release);
	ATF_TP_ADD_TC(tp, file_conflict_allows_same_package);
	ATF_TP_ADD_TC(tp, file_conflict_rejects_other_package);
	ATF_TP_ADD_TC(tp, file_conflict_rejects_unmanaged_file);

	return atf_no_error();
}
