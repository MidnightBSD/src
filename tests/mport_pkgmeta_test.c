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

#define TEST_ROOT_TEMPLATE "/tmp/mport-pkgmeta-test-root.XXXXXX"
#define SORT_TEST_PACKAGE_COUNT 4

static char test_root[PATH_MAX];

static const char *
test_path(const char *suffix)
{
	static char paths[8][PATH_MAX];
	static unsigned int next_path;
	char *path;

	path = paths[next_path++ % 8];
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
	(void)unsetenv("MPORT_MTREE_DIR");
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

	return mport;
}

static void
insert_package(mportInstance *mport, const char *name)
{
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO packages (pkg, version, origin, prefix, lang) VALUES "
		"(%Q, '1.0', %Q, '/usr/local', '')",
		name, name));
}

static void
create_dir(const char *path)
{
	ATF_REQUIRE_EQ(0, mkdir(path, 0755));
}

static void
create_file(const char *path)
{
	int fd;
	ssize_t written;

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	ATF_REQUIRE(fd >= 0);
	written = write(fd, "x", 1);
	ATF_REQUIRE_EQ(1, written);
	ATF_REQUIRE_EQ(0, close(fd));
}

static void
create_file_with_contents(const char *path, const char *contents)
{
	int fd;
	size_t len;
	ssize_t written;

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	ATF_REQUIRE(fd >= 0);
	len = strlen(contents);
	written = write(fd, contents, len);
	ATF_REQUIRE_EQ((ssize_t)len, written);
	ATF_REQUIRE_EQ(0, close(fd));
}

static mportPackageMeta *
create_delete_pack(const char *name)
{
	mportPackageMeta *pack;

	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	pack->name = strdup(name);
	pack->version = strdup("1.0");
	pack->prefix = strdup("/usr/local");
	pack->origin = strdup(name);
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
insert_asset(mportInstance *mport, const char *pkg, mportAssetListEntryType type, const char *data)
{
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO assets (pkg, type, data, checksum, owner, grp, mode) "
		"VALUES (%Q, %d, %Q, '', NULL, NULL, NULL)",
		pkg, type, data));
}

static void
insert_delete_package(mportInstance *mport, const char *name)
{
	insert_package(mport, name);
}

static void
create_local_prefix_dirs(void)
{
	create_dir(test_path("/usr"));
	create_dir(test_path("/usr/local"));
	create_dir(test_path("/usr/local/share"));
}

ATF_TC_WITH_CLEANUP(delete_removes_autodirs);
ATF_TC_HEAD(delete_removes_autodirs, tc)
{
	atf_tc_set_md_var(tc, "descr", "delete removes empty package-created auto directories");
}
ATF_TC_BODY(delete_removes_autodirs, tc)
{
	mportInstance *mport;
	mportPackageMeta *pack;

	(void)tc;

	mport = create_test_instance();
	create_local_prefix_dirs();
	create_dir(test_path("/usr/local/share/alpha"));
	create_dir(test_path("/usr/local/share/alpha/nested"));
	create_file(test_path("/usr/local/share/alpha/nested/file"));
	insert_delete_package(mport, "alpha");
	insert_asset(mport, "alpha", ASSET_FILE, "/usr/local/share/alpha/nested/file");
	insert_asset(mport, "alpha", ASSET_AUTODIR, "/usr/local/share/alpha/nested");
	insert_asset(mport, "alpha", ASSET_AUTODIR, "/usr/local/share/alpha");

	pack = create_delete_pack("alpha");
	ATF_REQUIRE_MSG(
	    mport_delete_primative(mport, pack, 1) == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE_EQ(-1, access(test_path("/usr/local/share/alpha"), F_OK));
	ATF_REQUIRE_EQ(0, access(test_path("/usr/local/share"), F_OK));

	mport_pkgmeta_free(pack);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(delete_removes_autodirs, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(delete_keeps_nonempty_autodirs);
ATF_TC_HEAD(delete_keeps_nonempty_autodirs, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "delete succeeds while preserving non-empty auto directories");
}
ATF_TC_BODY(delete_keeps_nonempty_autodirs, tc)
{
	mportInstance *mport;
	mportPackageMeta *pack;

	(void)tc;

	mport = create_test_instance();
	create_local_prefix_dirs();
	create_dir(test_path("/usr/local/share/alpha"));
	create_dir(test_path("/usr/local/share/alpha/nested"));
	create_file(test_path("/usr/local/share/alpha/nested/file"));
	create_file(test_path("/usr/local/share/alpha/nested/untracked"));
	insert_delete_package(mport, "alpha");
	insert_asset(mport, "alpha", ASSET_FILE, "/usr/local/share/alpha/nested/file");
	insert_asset(mport, "alpha", ASSET_AUTODIR, "/usr/local/share/alpha/nested");
	insert_asset(mport, "alpha", ASSET_AUTODIR, "/usr/local/share/alpha");

	pack = create_delete_pack("alpha");
	ATF_REQUIRE_MSG(
	    mport_delete_primative(mport, pack, 1) == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE_EQ(-1, access(test_path("/usr/local/share/alpha/nested/file"), F_OK));
	ATF_REQUIRE_EQ(0, access(test_path("/usr/local/share/alpha/nested/untracked"), F_OK));
	ATF_REQUIRE_EQ(0, access(test_path("/usr/local/share/alpha/nested"), F_OK));
	ATF_REQUIRE_EQ(0, access(test_path("/usr/local/share/alpha"), F_OK));

	mport_pkgmeta_free(pack);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(delete_keeps_nonempty_autodirs, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(delete_keeps_shared_autodirs);
ATF_TC_HEAD(delete_keeps_shared_autodirs, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "delete keeps auto directories still tracked by another package");
}
ATF_TC_BODY(delete_keeps_shared_autodirs, tc)
{
	mportInstance *mport;
	mportPackageMeta *alpha;
	mportPackageMeta *beta;

	(void)tc;

	mport = create_test_instance();
	create_local_prefix_dirs();
	create_dir(test_path("/usr/local/share/shared"));
	create_dir(test_path("/usr/local/share/shared/alpha"));
	create_dir(test_path("/usr/local/share/shared/beta"));
	create_file(test_path("/usr/local/share/shared/alpha/file"));
	create_file(test_path("/usr/local/share/shared/beta/file"));
	insert_delete_package(mport, "alpha");
	insert_delete_package(mport, "beta");
	insert_asset(mport, "alpha", ASSET_FILE, "/usr/local/share/shared/alpha/file");
	insert_asset(mport, "alpha", ASSET_AUTODIR, "/usr/local/share/shared/alpha");
	insert_asset(mport, "alpha", ASSET_AUTODIR, "/usr/local/share/shared");
	insert_asset(mport, "beta", ASSET_FILE, "/usr/local/share/shared/beta/file");
	insert_asset(mport, "beta", ASSET_AUTODIR, "/usr/local/share/shared/beta");
	insert_asset(mport, "beta", ASSET_AUTODIR, "/usr/local/share/shared");

	alpha = create_delete_pack("alpha");
	beta = create_delete_pack("beta");
	ATF_REQUIRE_MSG(
	    mport_delete_primative(mport, alpha, 1) == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE_EQ(-1, access(test_path("/usr/local/share/shared/alpha"), F_OK));
	ATF_REQUIRE_EQ(0, access(test_path("/usr/local/share/shared"), F_OK));

	ATF_REQUIRE_MSG(
	    mport_delete_primative(mport, beta, 1) == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE_EQ(-1, access(test_path("/usr/local/share/shared"), F_OK));

	mport_pkgmeta_free(alpha);
	mport_pkgmeta_free(beta);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(delete_keeps_shared_autodirs, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(delete_explicit_dirrmtry_still_removes);
ATF_TC_HEAD(delete_explicit_dirrmtry_still_removes, tc)
{
	atf_tc_set_md_var(tc, "descr", "explicit dirrmtry assets keep existing delete behavior");
}
ATF_TC_BODY(delete_explicit_dirrmtry_still_removes, tc)
{
	mportInstance *mport;
	mportPackageMeta *pack;

	(void)tc;

	mport = create_test_instance();
	create_local_prefix_dirs();
	create_dir(test_path("/usr/local/share/explicit"));
	create_file(test_path("/usr/local/share/explicit/file"));
	insert_delete_package(mport, "alpha");
	insert_asset(mport, "alpha", ASSET_FILE, "/usr/local/share/explicit/file");
	insert_asset(mport, "alpha", ASSET_DIRRMTRY, "/usr/local/share/explicit");

	pack = create_delete_pack("alpha");
	ATF_REQUIRE_MSG(
	    mport_delete_primative(mport, pack, 1) == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE_EQ(-1, access(test_path("/usr/local/share/explicit"), F_OK));

	mport_pkgmeta_free(pack);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(delete_explicit_dirrmtry_still_removes, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(mtree_fixture_protects_system_dirs);
ATF_TC_HEAD(mtree_fixture_protects_system_dirs, tc)
{
	atf_tc_set_md_var(tc, "descr", "mtree fixture protects system directories");
}
ATF_TC_BODY(mtree_fixture_protects_system_dirs, tc)
{
	mportInstance *mport;
	mportPackageMeta *pack;
	int fd;

	(void)tc;

	mport = create_test_instance();
	create_local_prefix_dirs();
	create_dir(test_path("/mtree"));
	fd = open(test_path("/mtree/BSD.local.dist"), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	ATF_REQUIRE(fd >= 0);
	const char mtree[] = ".\n/set type=dir\nbin\n..\nshare\n..\n";
	ATF_REQUIRE_EQ((ssize_t)strlen(mtree), write(fd, mtree, strlen(mtree)));
	ATF_REQUIRE_EQ(0, close(fd));
	ATF_REQUIRE_EQ(0, setenv("MPORT_MTREE_DIR", test_path("/mtree"), 1));
	create_dir(test_path("/usr/local/bin"));
	create_file(test_path("/usr/local/bin/tool"));
	insert_delete_package(mport, "alpha");
	insert_asset(mport, "alpha", ASSET_FILE, "/usr/local/bin/tool");
	insert_asset(mport, "alpha", ASSET_AUTODIR, "/usr/local/bin");

	ATF_REQUIRE(mport_is_system_mtree_dir("/usr/local/bin"));
	pack = create_delete_pack("alpha");
	ATF_REQUIRE_MSG(
	    mport_delete_primative(mport, pack, 1) == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE_EQ(0, access(test_path("/usr/local/bin"), F_OK));

	mport_pkgmeta_free(pack);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(mtree_fixture_protects_system_dirs, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(mtree_fallback_protects_system_dirs);
ATF_TC_HEAD(mtree_fallback_protects_system_dirs, tc)
{
	atf_tc_set_md_var(tc, "descr", "mtree fallback protects system directories");
}
ATF_TC_BODY(mtree_fallback_protects_system_dirs, tc)
{
	mportInstance *mport;
	mportPackageMeta *pack;

	(void)tc;

	mport = create_test_instance();
	create_local_prefix_dirs();
	ATF_REQUIRE_EQ(0, setenv("MPORT_MTREE_DIR", test_path("/missing-mtree"), 1));
	create_dir(test_path("/usr/local/bin"));
	create_file(test_path("/usr/local/bin/tool"));
	insert_delete_package(mport, "alpha");
	insert_asset(mport, "alpha", ASSET_FILE, "/usr/local/bin/tool");
	insert_asset(mport, "alpha", ASSET_AUTODIR, "/usr/local/bin");

	ATF_REQUIRE(mport_is_system_mtree_dir("/usr/local/bin"));
	pack = create_delete_pack("alpha");
	ATF_REQUIRE_MSG(
	    mport_delete_primative(mport, pack, 1) == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE_EQ(0, access(test_path("/usr/local/bin"), F_OK));

	mport_pkgmeta_free(pack);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(mtree_fallback_protects_system_dirs, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(delete_info_asset_keeps_post_uninstall_working);
ATF_TC_HEAD(delete_info_asset_keeps_post_uninstall_working, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "delete handles @info assets after unlinking the info file");
}
ATF_TC_BODY(delete_info_asset_keeps_post_uninstall_working, tc)
{
	mportInstance *mport;
	mportPackageMeta *pack;

	(void)tc;

	mport = create_test_instance();
	create_local_prefix_dirs();
	create_dir(test_path("/usr/local/share/info"));
	create_file(test_path("/usr/local/share/info/alpha.info"));
	insert_delete_package(mport, "alpha");
	insert_asset(mport, "alpha", ASSET_INFO, "/usr/local/share/info/alpha.info");

	pack = create_delete_pack("alpha");
	ATF_REQUIRE_MSG(
	    mport_delete_primative(mport, pack, 1) == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE_EQ(-1, access(test_path("/usr/local/share/info/alpha.info"), F_OK));

	mport_pkgmeta_free(pack);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(delete_info_asset_keeps_post_uninstall_working, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(rooted_infrastructure_path_uses_instance_root);
ATF_TC_HEAD(rooted_infrastructure_path_uses_instance_root, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "infrastructure helper resolves package files underneath the instance root");
}
ATF_TC_BODY(rooted_infrastructure_path_uses_instance_root, tc)
{
	mportInstance *mport;
	mportPackageMeta *pack;
	char path[PATH_MAX];

	(void)tc;

	mport = create_test_instance();
	pack = create_delete_pack("alpha");
	ATF_REQUIRE_EQ(0, mkdir(test_path("/var/db/mport/infrastructure/alpha-1.0"), 0755));
	create_file_with_contents(
	    test_path("/var/db/mport/infrastructure/alpha-1.0/pkg-deinstall"), "#!/bin/sh\nexit 0\n");

	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_build_infrastructure_path(mport, pack, MPORT_DEINSTALL_FILE, true,
			   path, sizeof(path)));
	ATF_REQUIRE_STREQ(test_path("/var/db/mport/infrastructure/alpha-1.0/pkg-deinstall"), path);
	ATF_REQUIRE_EQ(0, access(path, F_OK));
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_build_infrastructure_path(mport, pack, MPORT_DEINSTALL_FILE, false,
			   path, sizeof(path)));
	ATF_REQUIRE_STREQ("/var/db/mport/infrastructure/alpha-1.0/pkg-deinstall", path);

	mport_pkgmeta_free(pack);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(rooted_infrastructure_path_uses_instance_root, tc)
{
	(void)tc;

	cleanup_test_root();
}

static void
insert_query_package(mportInstance *mport, const char *name, const char *version,
    const char *origin, const char *comment, int automatic, int locked, int64_t flatsize)
{
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO packages (pkg, version, origin, prefix, lang, options, comment, "
		"os_release, cpe, locked, deprecated, expiration_date, no_provide_shlib, "
		"flavor, automatic, install_date, type, flatsize) VALUES "
		"(%Q, %Q, %Q, '/usr/local', '', 'DOCS=on EXAMPLES=off', %Q, '4.0', '', "
		"%d, '', 0, 0, '', %d, 1234, 0, %lld)",
		name, version, origin, comment, locked, automatic, (long long)flatsize));
}

static int
sorted_index(mportPackageMeta **sorted, int package_count, const char *name)
{
	int i;

	for (i = 0; i < package_count; i++) {
		if (strcmp(sorted[i]->name, name) == 0)
			return i;
	}

	return -1;
}

static void
require_before(mportPackageMeta **sorted, int package_count, const char *first, const char *second)
{
	int first_index = sorted_index(sorted, package_count, first);
	int second_index = sorted_index(sorted, package_count, second);

	ATF_REQUIRE_MSG(first_index >= 0, "missing package %s from sorted result", first);
	ATF_REQUIRE_MSG(second_index >= 0, "missing package %s from sorted result", second);
	ATF_REQUIRE_MSG(first_index < second_index, "expected %s before %s", first, second);
}

static void
populate_dependency_graph(mportInstance *mport)
{
	insert_package(mport, "a");
	insert_package(mport, "b");
	insert_package(mport, "c");
	insert_package(mport, "d");

	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO depends (pkg, depend_pkgname, depend_pkgversion, depend_port) VALUES "
		"('a', 'b', '1.0', 'ports/b'), "
		"('a', 'c', '1.0', 'ports/c'), "
		"('b', 'd', '1.0', 'ports/d')"));
}

ATF_TC_WITH_CLEANUP(sort_dependencies_dependency_first);
ATF_TC_HEAD(sort_dependencies_dependency_first, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "dependency-first sort updates lower-level dependencies before dependents");
}
ATF_TC_BODY(sort_dependencies_dependency_first, tc)
{
	mportInstance *mport;
	mportPackageMeta a = { .name = "a" };
	mportPackageMeta b = { .name = "b" };
	mportPackageMeta c = { .name = "c" };
	mportPackageMeta d = { .name = "d" };
	mportPackageMeta *flat[] = { &a, &b, &c, &d };
	mportPackageMeta **sorted;

	(void)tc;

	mport = create_test_instance();
	populate_dependency_graph(mport);

	sorted = mport_pkgmeta_sort_dependencies(mport, flat, SORT_TEST_PACKAGE_COUNT, true);
	ATF_REQUIRE(sorted != NULL);

	require_before(sorted, SORT_TEST_PACKAGE_COUNT, "d", "b");
	require_before(sorted, SORT_TEST_PACKAGE_COUNT, "b", "a");
	require_before(sorted, SORT_TEST_PACKAGE_COUNT, "c", "a");

	free(sorted);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(sort_dependencies_dependency_first, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(sort_dependencies_dependent_first);
ATF_TC_HEAD(sort_dependencies_dependent_first, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "dependent-first sort removes dependents before lower-level dependencies");
}
ATF_TC_BODY(sort_dependencies_dependent_first, tc)
{
	mportInstance *mport;
	mportPackageMeta a = { .name = "a" };
	mportPackageMeta b = { .name = "b" };
	mportPackageMeta c = { .name = "c" };
	mportPackageMeta d = { .name = "d" };
	mportPackageMeta *flat[] = { &a, &b, &c, &d };
	mportPackageMeta **sorted;

	(void)tc;

	mport = create_test_instance();
	populate_dependency_graph(mport);

	sorted = mport_pkgmeta_sort_dependencies(mport, flat, SORT_TEST_PACKAGE_COUNT, false);
	ATF_REQUIRE(sorted != NULL);

	require_before(sorted, SORT_TEST_PACKAGE_COUNT, "a", "b");
	require_before(sorted, SORT_TEST_PACKAGE_COUNT, "a", "c");
	require_before(sorted, SORT_TEST_PACKAGE_COUNT, "b", "d");

	free(sorted);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(sort_dependencies_dependent_first, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(query_formats_installed_metadata);
ATF_TC_HEAD(query_formats_installed_metadata, tc)
{
	atf_tc_set_md_var(tc, "descr", "query formatter prints installed package metadata");
}
ATF_TC_BODY(query_formats_installed_metadata, tc)
{
	mportInstance *mport;
	mportPackageMeta **packs = NULL;
	mportQueryOptions opts;
	char *buf = NULL;
	size_t len = 0;
	FILE *fp;

	(void)tc;

	mport = create_test_instance();
	insert_query_package(mport, "alpha", "1.2.3", "devel/alpha", "Alpha package", 0, 1, 4096);
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(
		mport->db, "INSERT INTO categories (pkg, category) VALUES ('alpha', 'devel')"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO depends (pkg, depend_pkgname, depend_pkgversion, depend_port) "
		"VALUES ('alpha', 'beta', '1.0', 'devel/beta')"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO assets (pkg, type, data, checksum, owner, grp, mode) VALUES "
		"('alpha', %d, '/usr/local/bin/alpha', '', '', '', '')",
		ASSET_FILE));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/var/db/mport/infrastructure/alpha-1.2.3"), 0755));
	int message_fd = open(test_path("/var/db/mport/infrastructure/alpha-1.2.3/pkg-message"),
	    O_WRONLY | O_CREAT | O_TRUNC, 0644);
	ATF_REQUIRE(message_fd >= 0);
	ssize_t write_result = write(message_fd, "Read alpha notes", strlen("Read alpha notes"));
	int close_result = close(message_fd);
	ATF_REQUIRE_EQ((ssize_t)strlen("Read alpha notes"), write_result);
	ATF_REQUIRE_EQ(0, close_result);
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_annotation_set(mport, "alpha", "maintainer", "ports@MidnightBSD.org"));
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_annotation_set(mport, "alpha", "www", "https://example.test"));

	memset(&opts, 0, sizeof(opts));
	opts.all = true;
	opts.case_sensitive = false;
	opts.match = MPORT_QUERY_MATCH_EXACT;
	ATF_REQUIRE_EQ(MPORT_OK, mport_query_installed(mport, &opts, &packs));
	ATF_REQUIRE(packs != NULL);

	fp = open_memstream(&buf, &len);
	ATF_REQUIRE(fp != NULL);
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_query_print(mport, packs, "%n|%v|%o|%m|%w|%c|%k|%s|%C|%d|%F|%#C|%M", fp));
	close_result = fclose(fp);
	fp = NULL;
	ATF_REQUIRE_EQ(0, close_result);

	ATF_REQUIRE_STREQ("alpha|1.2.3|devel/alpha|ports@MidnightBSD.org|"
			  "https://example.test|Alpha package|1|4096|devel|beta|"
			  "/usr/local/bin/alpha|1|Read alpha notes\n",
	    buf);

	free(buf);
	mport_pkgmeta_vec_free(packs);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(query_formats_installed_metadata, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(query_filters_patterns_and_expressions);
ATF_TC_HEAD(query_filters_patterns_and_expressions, tc)
{
	atf_tc_set_md_var(tc, "descr", "query filters by pattern and scalar expression");
}
ATF_TC_BODY(query_filters_patterns_and_expressions, tc)
{
	mportInstance *mport;
	mportPackageMeta **packs = NULL;
	mportQueryOptions opts;
	char *patterns[] = { "*" };

	(void)tc;

	mport = create_test_instance();
	insert_query_package(mport, "alpha", "1.2.3", "devel/alpha", "Alpha package", 0, 0, 4096);
	insert_query_package(mport, "beta", "0.9", "devel/beta", "Beta package", 1, 0, 1024);

	memset(&opts, 0, sizeof(opts));
	opts.case_sensitive = false;
	opts.match = MPORT_QUERY_MATCH_GLOB;
	opts.patterns = patterns;
	opts.pattern_count = 1;
	opts.expression = "%n='alpha'&&%v>=1.0||%n='missing'";

	ATF_REQUIRE_EQ(MPORT_OK, mport_query_installed(mport, &opts, &packs));
	ATF_REQUIRE(packs != NULL);
	ATF_REQUIRE(packs[0] != NULL);
	ATF_REQUIRE_STREQ("alpha", packs[0]->name);
	ATF_REQUIRE(packs[1] == NULL);

	mport_pkgmeta_vec_free(packs);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(query_filters_patterns_and_expressions, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, sort_dependencies_dependency_first);
	ATF_TP_ADD_TC(tp, sort_dependencies_dependent_first);
	ATF_TP_ADD_TC(tp, delete_removes_autodirs);
	ATF_TP_ADD_TC(tp, delete_keeps_nonempty_autodirs);
	ATF_TP_ADD_TC(tp, delete_keeps_shared_autodirs);
	ATF_TP_ADD_TC(tp, delete_explicit_dirrmtry_still_removes);
	ATF_TP_ADD_TC(tp, mtree_fixture_protects_system_dirs);
	ATF_TP_ADD_TC(tp, mtree_fallback_protects_system_dirs);
	ATF_TP_ADD_TC(tp, delete_info_asset_keeps_post_uninstall_working);
	ATF_TP_ADD_TC(tp, rooted_infrastructure_path_uses_instance_root);
	ATF_TP_ADD_TC(tp, query_formats_installed_metadata);
	ATF_TP_ADD_TC(tp, query_filters_patterns_and_expressions);

	return atf_no_error();
}
