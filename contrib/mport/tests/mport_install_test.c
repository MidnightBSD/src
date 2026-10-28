#include <sys/cdefs.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <libutil.h>
#include <limits.h>
#include <spawn.h>
#include <stdbool.h>
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

#define TEST_ROOT_TEMPLATE "/tmp/mport-install-test-root.XXXXXX"
#define PKG_NAME "testpkg"
#define PKG_VERSION "1.0"
#define PKG_PREFIX "/usr/local"
#define PKG_FILE_REL "share/testpkg/catalog.mk"
#define PKG_FILE_ABS PKG_PREFIX "/" PKG_FILE_REL
#define PKG_DIR_REL "share/testpkg/emptydir"
#define PKG_DIR_ABS PKG_PREFIX "/" PKG_DIR_REL

static char test_root[PATH_MAX];

/* A "tag:value" annotation that create_package_message() adds when set. */
static const char *pkg_annotation;

/*
 * The install-time age check asks getlogin(2) and agev_get_age_bracket(3)
 * for the installing user's age.  These definitions interpose on libc and
 * libutil so a case can supply an age bracket without a login session or a
 * running aged(8).  While age_lookup is false there is no login session, and
 * while aged_down is true agev_get_age_bracket(3) fails as it does when
 * aged(8) cannot be reached; libmport skips the check in both cases.
 */
static bool age_lookup;
static bool aged_down;
static int age_bracket[2];

/*
 * Fill freed memory so a use after free shows up as a wrong value instead of
 * reading back the old contents.
 */
const char *malloc_conf = "junk:true";

char *
getlogin(void)
{
	static char login[] = "mport-test";

	return age_lookup ? login : NULL;
}

int *
agev_get_age_bracket(const char *username)
{
	int *ages;

	(void)username;
	if (!age_lookup || aged_down)
		return NULL;
	ages = malloc(sizeof(age_bracket));
	if (ages != NULL)
		memcpy(ages, age_bracket, sizeof(age_bracket));

	return ages;
}

/*
 * What "sysrc -qn aged_enable" prints, or NULL while aged_enable is unset.
 * posix_spawn() below runs echo (or false) in place of sysrc(8), so a case
 * does not depend on the host's rc.conf.
 */
static const char *aged_enable;

int
posix_spawn(pid_t *restrict pid, const char *restrict path,
    const posix_spawn_file_actions_t *file_actions, const posix_spawnattr_t *restrict attrp,
    char *const argv[], char *const envp[])
{
	static int (*real_spawn)(pid_t *restrict, const char *restrict,
	    const posix_spawn_file_actions_t *, const posix_spawnattr_t *restrict, char *const[],
	    char *const[]);

	if (real_spawn == NULL)
		real_spawn = (int (*)(pid_t *restrict, const char *restrict,
		    const posix_spawn_file_actions_t *, const posix_spawnattr_t *restrict,
		    char *const[], char *const[]))dlsym(RTLD_NEXT, "posix_spawn");

	if (strcmp(path, MPORT_SYSRC_BIN) == 0) {
		char *echo_argv[] = { "echo", (char *)aged_enable, NULL };
		char *false_argv[] = { "false", NULL };

		if (aged_enable == NULL)
			return real_spawn(
			    pid, "/usr/bin/false", file_actions, attrp, false_argv, envp);
		return real_spawn(pid, "/bin/echo", file_actions, attrp, echo_argv, envp);
	}

	return real_spawn(pid, path, file_actions, attrp, argv, envp);
}

/* Messages (pkg-message entries, warnings) a test package shows */
static char shown_messages[4096];

static void
capture_message(const char *msg)
{
	size_t len = strlen(shown_messages);

	(void)snprintf(shown_messages + len, sizeof(shown_messages) - len, "%s|", msg);
}

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
}

static void
write_file(const char *path, const char *contents)
{
	int fd;
	size_t len;

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	ATF_REQUIRE(fd >= 0);
	len = strlen(contents);
	ATF_REQUIRE_EQ((ssize_t)len, write(fd, contents, len));
	ATF_REQUIRE_EQ(0, close(fd));
}

static mportInstance *
create_test_instance(void)
{
	mportInstance *mport;

	(void)strlcpy(test_root, TEST_ROOT_TEMPLATE, sizeof(test_root));
	ATF_REQUIRE(mkdtemp(test_root) != NULL);
	ATF_REQUIRE_EQ(0, mkdir(test_path("/var"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/var/db"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/usr"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/usr/local"), 0755));

	mport = mport_instance_new();
	ATF_REQUIRE(mport != NULL);
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_instance_init(mport, test_root, "root", false, MPORT_VQUIET));

	return mport;
}

/*
 * Build a minimal one-file package named `name` under the test root and
 * return its path.  The package installs share/<name>/catalog.mk and an
 * empty share/<name>/emptydir.  `depend`, if not NULL, is another package
 * name recorded as a dependency with no version requirement.
 */
static const char *create_package_version(
    mportInstance *, const char *, const char *, const char *);
static const char *create_package_message(
    mportInstance *, const char *, const char *, const char *, const char *);

static const char *
create_package(mportInstance *mport, const char *name, const char *depend)
{
	return create_package_version(mport, name, PKG_VERSION, depend);
}

static const char *
create_package_version(
    mportInstance *mport, const char *name, const char *version, const char *depend)
{
	return create_package_message(mport, name, version, depend, NULL);
}

/* As create_package_version(), with `message`, if not NULL, as its pkg-message. */
static const char *
create_package_message(mportInstance *mport, const char *name, const char *version,
    const char *depend, const char *message)
{
	mportAssetList *assetlist;
	mportPackageMeta *pack;
	mportCreateExtras *extra;
	FILE *fp;
	char stage[PATH_MAX];
	char buf[PATH_MAX];
	char plist[PATH_MAX];

	(void)snprintf(stage, sizeof(stage), "%s/stage-%s-%s", test_root, name, version);
	ATF_REQUIRE_EQ(0, mkdir(stage, 0755));
	(void)snprintf(buf, sizeof(buf), "%s/usr", stage);
	ATF_REQUIRE_EQ(0, mkdir(buf, 0755));
	(void)snprintf(buf, sizeof(buf), "%s/usr/local", stage);
	ATF_REQUIRE_EQ(0, mkdir(buf, 0755));
	(void)snprintf(buf, sizeof(buf), "%s/usr/local/share", stage);
	ATF_REQUIRE_EQ(0, mkdir(buf, 0755));
	(void)snprintf(buf, sizeof(buf), "%s/usr/local/share/%s", stage, name);
	ATF_REQUIRE_EQ(0, mkdir(buf, 0755));
	(void)snprintf(buf, sizeof(buf), "%s/usr/local/share/%s/emptydir", stage, name);
	ATF_REQUIRE_EQ(0, mkdir(buf, 0755));
	(void)snprintf(buf, sizeof(buf), "%s/usr/local/share/%s/catalog.mk", stage, name);
	write_file(buf, "catalog\n");

	(void)snprintf(
	    plist, sizeof(plist), "share/%s/catalog.mk\n@dir share/%s/emptydir\n", name, name);
	(void)snprintf(buf, sizeof(buf), "%s/plist-%s-%s", test_root, name, version);
	write_file(buf, plist);

	assetlist = mport_assetlist_new();
	ATF_REQUIRE(assetlist != NULL);
	fp = fopen(buf, "r");
	ATF_REQUIRE(fp != NULL);
	ATF_REQUIRE_EQ(0, mport_parse_plistfile(fp, assetlist));
	(void)fclose(fp);

	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	pack->name = strdup(name);
	pack->version = strdup(version);
	pack->prefix = strdup(PKG_PREFIX);
	(void)snprintf(buf, sizeof(buf), "misc/%s", name);
	pack->origin = strdup(buf);
	pack->lang = strdup("");
	pack->comment = strdup("test package");
	pack->type = MPORT_TYPE_APP;

	extra = mport_createextras_new();
	ATF_REQUIRE(extra != NULL);
	(void)snprintf(extra->pkg_filename, sizeof(extra->pkg_filename), "%s/%s-%s.mport",
	    test_root, name, version);
	(void)strlcpy(extra->sourcedir, stage, sizeof(extra->sourcedir));
	if (depend != NULL) {
		/* name:origin, no version requirement */
		extra->depends = calloc(2, sizeof(char *));
		ATF_REQUIRE(extra->depends != NULL);
		(void)snprintf(buf, sizeof(buf), "%s:misc/%s", depend, depend);
		extra->depends[0] = strdup(buf);
		ATF_REQUIRE(extra->depends[0] != NULL);
		extra->depends_count = 1;
	}
	if (message != NULL) {
		(void)snprintf(buf, sizeof(buf), "%s/pkg-message-%s-%s", test_root, name, version);
		write_file(buf, message);
		extra->pkgmessage = strdup(buf);
		ATF_REQUIRE(extra->pkgmessage != NULL);
	}
	if (pkg_annotation != NULL) {
		char *annotation = strdup(pkg_annotation);

		ATF_REQUIRE(annotation != NULL);
		tll_push_back(extra->annotations, annotation);
	}

	ATF_REQUIRE_MSG(mport_create_primative(mport, assetlist, pack, extra) == MPORT_OK, "%s",
	    mport_err_string());

	mport_assetlist_free(assetlist);
	mport_pkgmeta_free(pack);
	mport_createextras_free(extra);

	(void)snprintf(buf, sizeof(buf), "/%s-%s.mport", name, version);
	return test_path(buf);
}

/* The default test package: no dependencies. */
static const char *
create_test_package(mportInstance *mport)
{
	return create_package(mport, PKG_NAME, NULL);
}

/* Number of registry rows for a package by name. */
static int
count_pkg(mportInstance *mport, const char *name)
{
	mportPackageMeta **found = NULL;
	int count;

	ATF_REQUIRE_EQ(MPORT_OK, mport_pkgmeta_search_master(mport, &found, "pkg=%Q", name));
	if (found == NULL)
		return 0;
	for (count = 0; found[count] != NULL; count++)
		;
	mport_pkgmeta_vec_free(found);

	return count;
}

/* Contents of a small file, or "" if it cannot be read. */
static const char *
read_file(const char *path)
{
	static char contents[256];
	FILE *fp;
	size_t n;

	contents[0] = '\0';
	fp = fopen(path, "r");
	if (fp == NULL)
		return contents;
	n = fread(contents, 1, sizeof(contents) - 1, fp);
	contents[n] = '\0';
	(void)fclose(fp);

	return contents;
}

/* Number of registry rows for the test package, and its os_release. */
static int
count_installed(mportInstance *mport, /*@out@*/ char *os_release, size_t os_release_len)
{
	mportPackageMeta **found = NULL;
	int count = 0;

	if (os_release != NULL)
		os_release[0] = '\0';

	ATF_REQUIRE_EQ(MPORT_OK, mport_pkgmeta_search_master(mport, &found, "pkg=%Q", PKG_NAME));
	if (found == NULL)
		return 0;

	for (count = 0; found[count] != NULL; count++) {
		if (count == 0 && os_release != NULL && found[0]->os_release != NULL)
			(void)strlcpy(os_release, found[0]->os_release, os_release_len);
	}

	mport_pkgmeta_vec_free(found);

	return count;
}

/*
 * Regression test for issue #180: a package installed under a previous OS
 * release must be replaced, not reported as a file conflict.
 */
ATF_TC_WITH_CLEANUP(install_replaces_previous_os_release);
ATF_TC_HEAD(install_replaces_previous_os_release, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "installing over a copy registered under an older os_release replaces it");
}
ATF_TC_BODY(install_replaces_previous_os_release, tc)
{
	mportInstance *mport;
	const char *pkgfile;
	char os_release[64];
	char *system_os_release;

	(void)tc;

	mport = create_test_instance();
	pkgfile = create_test_package(mport);

	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(0, access(test_path(PKG_FILE_ABS), F_OK));
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));

	/* pretend the install happened on the previous OS release */
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(
		mport->db, "UPDATE packages SET os_release='0.0-OLD' WHERE pkg=%Q", PKG_NAME));

	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());

	/* the stale copy is replaced, not duplicated */
	ATF_REQUIRE_EQ(1, count_installed(mport, os_release, sizeof(os_release)));
	system_os_release = mport_get_osrelease(mport);
	ATF_REQUIRE(system_os_release != NULL);
	ATF_REQUIRE_STREQ(system_os_release, os_release);
	free(system_os_release);
	ATF_REQUIRE_EQ(0, access(test_path(PKG_FILE_ABS), F_OK));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(install_replaces_previous_os_release, tc)
{
	(void)tc;

	cleanup_test_root();
}

/* Installing the same package twice under the current OS release is a no-op. */
ATF_TC_WITH_CLEANUP(install_same_os_release_is_rejected);
ATF_TC_HEAD(install_same_os_release_is_rejected, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "a package installed under the current os_release is left alone");
}
ATF_TC_BODY(install_same_os_release_is_rejected, tc)
{
	mportInstance *mport;
	const char *pkgfile;

	(void)tc;

	mport = create_test_instance();
	pkgfile = create_test_package(mport);

	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));

	/* the second install must not delete or duplicate the installed copy */
	(void)mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT);
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));
	ATF_REQUIRE_EQ(0, access(test_path(PKG_FILE_ABS), F_OK));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(install_same_os_release_is_rejected, tc)
{
	(void)tc;

	cleanup_test_root();
}

/* Number of rows for the test package in a registry table other than packages. */
static int
count_rows(mportInstance *mport, const char *table)
{
	int count = -1;

	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_count(
		mport->db, &count, "SELECT COUNT(*) FROM %s WHERE pkg=%Q", table, PKG_NAME));

	return count;
}

/*
 * Remove the packages and assets rows but leave the rest, as a broken
 * uninstall or a failed install can. The test package has no depends,
 * categories or conflicts of its own, so seed a stale row in each of those
 * tables too; the forced install must clear every one of them. The files stay
 * on disk.
 */
static void
orphan_registry_rows(mportInstance *mport)
{
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_db_do(mport->db, "DELETE FROM packages WHERE pkg=%Q", PKG_NAME));
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_db_do(mport->db, "DELETE FROM assets WHERE pkg=%Q", PKG_NAME));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO depends (pkg, depend_pkgname, depend_pkgversion, depend_port) VALUES (%Q, 'stale-dep', '1.0', 'misc/stale-dep')",
		PKG_NAME));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db, "INSERT INTO categories (pkg, category) VALUES (%Q, 'stale')",
		PKG_NAME));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO conflicts (pkg, conflict_pkg, conflict_version) VALUES (%Q, 'stale-conflict', '*')",
		PKG_NAME));

	ATF_REQUIRE_EQ(0, count_installed(mport, NULL, 0));
	ATF_REQUIRE(count_rows(mport, "annotation") > 0);
	ATF_REQUIRE_EQ(1, count_rows(mport, "depends"));
	ATF_REQUIRE_EQ(1, count_rows(mport, "categories"));
	ATF_REQUIRE_EQ(1, count_rows(mport, "conflicts"));
}

/*
 * A forced install over files left behind by a bad uninstall must succeed and
 * re-register the package, even when stale rows for it remain in the registry.
 */
ATF_TC_WITH_CLEANUP(force_reinstall_over_orphaned_rows);
ATF_TC_HEAD(force_reinstall_over_orphaned_rows, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "forced install re-registers a package whose stale rows were left behind");
}
ATF_TC_BODY(force_reinstall_over_orphaned_rows, tc)
{
	mportInstance *mport;
	const char *pkgfile;
	int annotations;

	(void)tc;

	mport = create_test_instance();
	pkgfile = create_test_package(mport);

	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	annotations = count_rows(mport, "annotation");
	orphan_registry_rows(mport);
	ATF_REQUIRE_EQ(0, access(test_path(PKG_FILE_ABS), F_OK));

	/* without -f the leftover file is reported as a conflict */
	ATF_REQUIRE(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) != MPORT_OK);
	ATF_REQUIRE(strstr(mport_err_string(), "use -f to overwrite") != NULL);
	ATF_REQUIRE_EQ(0, count_installed(mport, NULL, 0));

	mport->force = true;
	mport->offline = true;
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());

	/* exactly the rows this package declares: the seeded stale rows are gone
	 * and nothing was duplicated */
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));
	ATF_REQUIRE(count_rows(mport, "assets") > 0);
	ATF_REQUIRE_EQ(annotations, count_rows(mport, "annotation"));
	ATF_REQUIRE_EQ(0, count_rows(mport, "depends"));
	ATF_REQUIRE_EQ(0, count_rows(mport, "categories"));
	ATF_REQUIRE_EQ(0, count_rows(mport, "conflicts"));
	ATF_REQUIRE_EQ(0, access(test_path(PKG_FILE_ABS), F_OK));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(force_reinstall_over_orphaned_rows, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * An install that fails while deploying assets must not leave a packages row
 * behind, and the registry must stay usable for the next attempt.
 */
ATF_TC_WITH_CLEANUP(failed_install_registers_nothing);
ATF_TC_HEAD(failed_install_registers_nothing, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr", "a failed install rolls back every registry row it added");
}
ATF_TC_BODY(failed_install_registers_nothing, tc)
{
	mportInstance *mport;
	const char *pkgfile;

	(void)tc;

	mport = create_test_instance();
	pkgfile = create_test_package(mport);

	/* a symlink where the package expects a directory makes the asset
	 * loop fail after the packages row has been written */
	ATF_REQUIRE_EQ(0, mkdir(test_path(PKG_PREFIX "/share"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path(PKG_PREFIX "/share/testpkg"), 0755));
	ATF_REQUIRE_EQ(0, symlink("catalog.mk", test_path(PKG_DIR_ABS)));

	ATF_REQUIRE(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) != MPORT_OK);
	ATF_REQUIRE_EQ(0, count_installed(mport, NULL, 0));
	ATF_REQUIRE_EQ(0, count_rows(mport, "assets"));
	ATF_REQUIRE_EQ(0, count_rows(mport, "annotation"));
	ATF_REQUIRE_EQ(0, count_rows(mport, "depends"));
	ATF_REQUIRE_EQ(0, count_rows(mport, "categories"));

	/* the transaction was rolled back, so a corrected install goes through */
	ATF_REQUIRE_EQ(0, unlink(test_path(PKG_DIR_ABS)));
	mport->force = true;
	mport->offline = true;
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));
	ATF_REQUIRE_EQ(0, access(test_path(PKG_FILE_ABS), F_OK));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(failed_install_registers_nothing, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * An age annotation that is not a plain non-negative integer fails the
 * install with an error that names the annotation.  The annotation used to
 * be freed before the error message was formatted from it.
 */
ATF_TC_WITH_CLEANUP(invalid_age_annotation_is_reported);
ATF_TC_HEAD(invalid_age_annotation_is_reported, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "an invalid age annotation fails the install and is named in the error");
}
ATF_TC_BODY(invalid_age_annotation_is_reported, tc)
{
	mportInstance *mport;
	const char *pkgfile;
	int ret;

	(void)tc;

	mport = create_test_instance();
	pkg_annotation = "age:18x";
	pkgfile = create_test_package(mport);
	pkg_annotation = NULL;

	age_bracket[0] = 30;
	age_bracket[1] = -1;
	age_lookup = true;
	ret = mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT);
	age_lookup = false;

	ATF_REQUIRE(ret != MPORT_OK);
	ATF_REQUIRE_MSG(strstr(mport_err_string(),
			    "Invalid age annotation '18x' for package " PKG_NAME) != NULL,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(0, count_installed(mport, NULL, 0));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(invalid_age_annotation_is_reported, tc)
{
	(void)tc;

	age_lookup = false;
	cleanup_test_root();
}

/*
 * Annotations given to mport.create (-A tag:value) travel in the bundle and
 * are registered under the package name, so an age-restricted package
 * installs for an adult and records its "age" annotation.
 */
ATF_TC_WITH_CLEANUP(bundle_annotations_are_registered);
ATF_TC_HEAD(bundle_annotations_are_registered, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "annotations in a bundle are registered for the installed package");
}
ATF_TC_BODY(bundle_annotations_are_registered, tc)
{
	mportInstance *mport;
	const char *pkgfile;
	char *age = NULL;

	(void)tc;

	mport = create_test_instance();
	pkg_annotation = "age:18";
	pkgfile = create_test_package(mport);
	pkg_annotation = NULL;

	age_bracket[0] = 30;
	age_bracket[1] = -1;
	age_lookup = true;
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	age_lookup = false;

	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));
	ATF_REQUIRE_EQ(MPORT_OK, mport_annotation_get(mport, PKG_NAME, "age", &age));
	ATF_REQUIRE_STREQ("18", age);
	free(age);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(bundle_annotations_are_registered, tc)
{
	(void)tc;

	age_lookup = false;
	pkg_annotation = NULL;
	cleanup_test_root();
}

/*
 * The age check has to read the restriction from the bundle being
 * installed: the registry only holds it once the package is installed.
 */
ATF_TC_WITH_CLEANUP(underage_install_is_refused);
ATF_TC_HEAD(underage_install_is_refused, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "a package whose age annotation exceeds the user's age is not installed");
}
ATF_TC_BODY(underage_install_is_refused, tc)
{
	mportInstance *mport;
	const char *pkgfile;
	int ret;

	(void)tc;

	mport = create_test_instance();
	pkg_annotation = "age:18";
	pkgfile = create_test_package(mport);
	pkg_annotation = NULL;

	age_bracket[0] = 13;
	age_bracket[1] = 15;
	age_lookup = true;
	ret = mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT);
	age_lookup = false;

	ATF_REQUIRE(ret != MPORT_OK);
	ATF_REQUIRE_EQ(0, count_installed(mport, NULL, 0));
	ATF_REQUIRE_EQ(-1, access(test_path(PKG_FILE_ABS), F_OK));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(underage_install_is_refused, tc)
{
	(void)tc;

	age_lookup = false;
	pkg_annotation = NULL;
	cleanup_test_root();
}

/*
 * Install a package annotated "age:<required>" with the user in the
 * agev_get_age_bracket(3) bracket { low, high } and return the result.
 */
static int
install_with_age_bracket(mportInstance *mport, const char *annotation, int low, int high)
{
	const char *pkgfile;
	int ret;

	pkg_annotation = annotation;
	pkgfile = create_test_package(mport);
	pkg_annotation = NULL;

	age_bracket[0] = low;
	age_bracket[1] = high;
	age_lookup = true;
	ret = mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT);
	age_lookup = false;

	return ret;
}

/*
 * A requirement within the user's bracket is met: a 13 to 15 year old can
 * install a package for 15 and up.  The install step used to compare the
 * requirement with the bottom of the bracket and refuse it.
 */
ATF_TC_WITH_CLEANUP(age_within_bracket_is_installed);
ATF_TC_HEAD(age_within_bracket_is_installed, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "a package whose age annotation is within the user's bracket installs");
}
ATF_TC_BODY(age_within_bracket_is_installed, tc)
{
	mportInstance *mport;

	(void)tc;

	mport = create_test_instance();
	ATF_REQUIRE_MSG(install_with_age_bracket(mport, "age:15", 13, 15) == MPORT_OK, "%s",
	    mport_err_string());
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(age_within_bracket_is_installed, tc)
{
	(void)tc;

	age_lookup = false;
	pkg_annotation = NULL;
	cleanup_test_root();
}

/*
 * The adult bracket (18,-1) is 18 or older and is never refused, even for a
 * package annotated above 18.
 */
ATF_TC_WITH_CLEANUP(adult_install_is_never_refused);
ATF_TC_HEAD(adult_install_is_never_refused, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr", "an adult can install a package with any age annotation");
}
ATF_TC_BODY(adult_install_is_never_refused, tc)
{
	mportInstance *mport;

	(void)tc;

	mport = create_test_instance();
	ATF_REQUIRE_MSG(install_with_age_bracket(mport, "age:21", 18, -1) == MPORT_OK, "%s",
	    mport_err_string());
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(adult_install_is_never_refused, tc)
{
	(void)tc;

	age_lookup = false;
	pkg_annotation = NULL;
	cleanup_test_root();
}

/*
 * aged(8) answers -2,-2 when its region (e.g. US-MI) does not require age
 * checks; any package installs.
 */
ATF_TC_WITH_CLEANUP(age_not_required_by_region_installs);
ATF_TC_HEAD(age_not_required_by_region_installs, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr",
	    "an age-restricted package installs where aged's region requires no age checks");
}
ATF_TC_BODY(age_not_required_by_region_installs, tc)
{
	mportInstance *mport;

	(void)tc;

	mport = create_test_instance();
	ATF_REQUIRE_MSG(install_with_age_bracket(mport, "age:18", -2, -2) == MPORT_OK, "%s",
	    mport_err_string());
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(age_not_required_by_region_installs, tc)
{
	(void)tc;

	age_lookup = false;
	pkg_annotation = NULL;
	cleanup_test_root();
}

/*
 * With no login session (cron, rc scripts) there is no user to check, so an
 * age-restricted package installs.
 */
ATF_TC_WITH_CLEANUP(age_check_without_login_installs);
ATF_TC_HEAD(age_check_without_login_installs, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "an age-restricted package installs when there is no login session");
}
ATF_TC_BODY(age_check_without_login_installs, tc)
{
	mportInstance *mport;
	const char *pkgfile;

	(void)tc;

	mport = create_test_instance();
	pkg_annotation = "age:18";
	pkgfile = create_test_package(mport);
	pkg_annotation = NULL;

	age_lookup = false;
	mport->msg_cb = capture_message;
	shown_messages[0] = '\0';
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));
	ATF_REQUIRE_MSG(strstr(shown_messages, "could not determine the user name") != NULL &&
		strstr(shown_messages, "skipping the age check") != NULL,
	    "%s", shown_messages);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(age_check_without_login_installs, tc)
{
	(void)tc;

	pkg_annotation = NULL;
	cleanup_test_root();
}

/*
 * When aged(8) cannot be reached the user's age is not known, so an
 * age-restricted package installs.
 */
ATF_TC_WITH_CLEANUP(age_check_without_aged_installs);
ATF_TC_HEAD(age_check_without_aged_installs, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "an age-restricted package installs when aged cannot be reached");
}
ATF_TC_BODY(age_check_without_aged_installs, tc)
{
	mportInstance *mport;
	int ret;

	(void)tc;

	mport = create_test_instance();
	mport->msg_cb = capture_message;
	shown_messages[0] = '\0';
	aged_down = true;
	ret = install_with_age_bracket(mport, "age:18", 13, 15);
	aged_down = false;

	ATF_REQUIRE_MSG(ret == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));
	ATF_REQUIRE_MSG(
	    strstr(shown_messages,
		"could not determine the age bracket of mport-test, skipping the age check") !=
		NULL,
	    "%s", shown_messages);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(age_check_without_aged_installs, tc)
{
	(void)tc;

	age_lookup = false;
	aged_down = false;
	pkg_annotation = NULL;
	cleanup_test_root();
}

/*
 * Where aged_enable is set, aged not answering must not skip the check: the
 * install is refused and the user is told to check on aged.
 */
ATF_TC_WITH_CLEANUP(age_check_with_aged_enabled_but_down_is_refused);
ATF_TC_HEAD(age_check_with_aged_enabled_but_down_is_refused, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr",
	    "an age-restricted package is refused when aged is enabled but not answering");
}
ATF_TC_BODY(age_check_with_aged_enabled_but_down_is_refused, tc)
{
	mportInstance *mport;
	int ret;

	(void)tc;

	mport = create_test_instance();
	mport->msg_cb = capture_message;
	shown_messages[0] = '\0';
	aged_enable = "Yes";
	aged_down = true;
	ret = install_with_age_bracket(mport, "age:13", 18, -1);
	aged_down = false;
	aged_enable = NULL;

	ATF_REQUIRE(ret != MPORT_OK);
	ATF_REQUIRE_EQ(0, count_installed(mport, NULL, 0));
	ATF_REQUIRE_MSG(strstr(shown_messages,
			    "aged is enabled but the age bracket of mport-test "
			    "could not be read") != NULL,
	    "%s", shown_messages);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(age_check_with_aged_enabled_but_down_is_refused, tc)
{
	(void)tc;

	age_lookup = false;
	aged_down = false;
	aged_enable = NULL;
	pkg_annotation = NULL;
	cleanup_test_root();
}

/*
 * aged_enable=NO is the same as unset: aged not answering skips the check.
 */
ATF_TC_WITH_CLEANUP(age_check_with_aged_disabled_installs);
ATF_TC_HEAD(age_check_with_aged_disabled_installs, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr", "an age-restricted package installs when aged_enable is NO");
}
ATF_TC_BODY(age_check_with_aged_disabled_installs, tc)
{
	mportInstance *mport;
	int ret;

	(void)tc;

	mport = create_test_instance();
	aged_enable = "NO";
	aged_down = true;
	ret = install_with_age_bracket(mport, "age:18", 13, 15);
	aged_down = false;
	aged_enable = NULL;

	ATF_REQUIRE_MSG(ret == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(age_check_with_aged_disabled_installs, tc)
{
	(void)tc;

	age_lookup = false;
	aged_down = false;
	aged_enable = NULL;
	pkg_annotation = NULL;
	cleanup_test_root();
}

/*
 * A user whose age aged does not know (-1,-1) is refused and told how to set
 * it with agectl(8).
 */
ATF_TC_WITH_CLEANUP(unset_age_is_refused_with_agectl_hint);
ATF_TC_HEAD(unset_age_is_refused_with_agectl_hint, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr",
	    "an age-restricted package is refused for a user with no age set, naming agectl");
}
ATF_TC_BODY(unset_age_is_refused_with_agectl_hint, tc)
{
	mportInstance *mport;
	int ret;

	(void)tc;

	mport = create_test_instance();
	mport->msg_cb = capture_message;
	shown_messages[0] = '\0';
	ret = install_with_age_bracket(mport, "age:13", -1, -1);

	ATF_REQUIRE(ret != MPORT_OK);
	ATF_REQUIRE_EQ(0, count_installed(mport, NULL, 0));
	ATF_REQUIRE_MSG(strstr(shown_messages, "the age of mport-test is not set") != NULL &&
		strstr(shown_messages, "agectl -a <age> mport-test") != NULL,
	    "%s", shown_messages);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(unset_age_is_refused_with_agectl_hint, tc)
{
	(void)tc;

	age_lookup = false;
	pkg_annotation = NULL;
	cleanup_test_root();
}

/*
 * A @dir asset whose parent directory is a symlink to a directory elsewhere
 * (the bind chroot layout, /usr/local/etc/namedb -> /var/named/...) must be
 * created through the link rather than failing the install.
 */
ATF_TC_WITH_CLEANUP(dir_asset_through_symlinked_parent);
ATF_TC_HEAD(dir_asset_through_symlinked_parent, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr", "a @dir asset below a symlinked parent directory installs");
}
ATF_TC_BODY(dir_asset_through_symlinked_parent, tc)
{
	mportInstance *mport;
	const char *pkgfile;
	struct stat sb;

	(void)tc;

	mport = create_test_instance();
	pkgfile = create_test_package(mport);

	/* share/testpkg is a link into a tree outside the prefix */
	ATF_REQUIRE_EQ(0, mkdir(test_path("/var/chroot"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/var/chroot/testpkg"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path(PKG_PREFIX "/share"), 0755));
	ATF_REQUIRE_EQ(
	    0, symlink(test_path("/var/chroot/testpkg"), test_path(PKG_PREFIX "/share/testpkg")));

	mport->offline = true;
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));

	/* the directory landed behind the link, and the link itself survived */
	ATF_REQUIRE_EQ(0, lstat(test_path(PKG_PREFIX "/share/testpkg"), &sb));
	ATF_REQUIRE(S_ISLNK(sb.st_mode));
	ATF_REQUIRE_EQ(0, lstat(test_path("/var/chroot/testpkg/emptydir"), &sb));
	ATF_REQUIRE(S_ISDIR(sb.st_mode));
	ATF_REQUIRE_EQ(0, access(test_path("/var/chroot/testpkg/catalog.mk"), F_OK));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(dir_asset_through_symlinked_parent, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * mport add installs a missing dependency from a package file next to the one
 * named, and marks it automatic.
 */
ATF_TC_WITH_CLEANUP(add_installs_missing_dependency_from_sibling);
ATF_TC_HEAD(add_installs_missing_dependency_from_sibling, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "a dependency missing from the registry is installed from a sibling file");
}
ATF_TC_BODY(add_installs_missing_dependency_from_sibling, tc)
{
	mportInstance *mport;
	const char *pkgfile;
	mportPackageMeta **dep = NULL;

	(void)tc;

	mport = create_test_instance();
	(void)create_package(mport, "testdep", NULL);
	pkgfile = create_package(mport, PKG_NAME, "testdep");

	mport->offline = true;
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(1, count_pkg(mport, PKG_NAME));
	ATF_REQUIRE_EQ(1, count_pkg(mport, "testdep"));
	ATF_REQUIRE_EQ(0, access(test_path(PKG_PREFIX "/share/testdep/catalog.mk"), F_OK));

	ATF_REQUIRE_EQ(MPORT_OK, mport_pkgmeta_search_master(mport, &dep, "pkg=%Q", "testdep"));
	ATF_REQUIRE(dep != NULL && dep[0] != NULL);
	ATF_REQUIRE_EQ(MPORT_AUTOMATIC, dep[0]->automatic);
	mport_pkgmeta_vec_free(dep);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(add_installs_missing_dependency_from_sibling, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * A forced add repairs the package named and nothing else.  A dependency that
 * is installed for the running release must not be reinstalled from a sibling
 * file, and must not be needed as a sibling file at all.
 */
ATF_TC_WITH_CLEANUP(forced_add_leaves_installed_dependency_alone);
ATF_TC_HEAD(forced_add_leaves_installed_dependency_alone, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "--force does not cascade into dependencies that are already installed");
}
ATF_TC_BODY(forced_add_leaves_installed_dependency_alone, tc)
{
	mportInstance *mport;
	const char *depfile;
	const char *pkgfile;
	char dep_path[PATH_MAX];

	(void)tc;

	mport = create_test_instance();
	depfile = create_package(mport, "testdep", NULL);
	pkgfile = create_package(mport, PKG_NAME, "testdep");

	mport->offline = true;
	ATF_REQUIRE_MSG(mport_install_primative(mport, depfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());

	/* a reinstall of testdep would put "catalog" back and would need the
	 * sibling file, so change the one and remove the other */
	(void)strlcpy(
	    dep_path, test_path(PKG_PREFIX "/share/testdep/catalog.mk"), sizeof(dep_path));
	write_file(dep_path, "modified\n");
	ATF_REQUIRE_EQ(0, unlink(depfile));

	mport->force = true;
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());

	ATF_REQUIRE_EQ(1, count_pkg(mport, PKG_NAME));
	ATF_REQUIRE_EQ(1, count_pkg(mport, "testdep"));
	ATF_REQUIRE_STREQ("modified\n", read_file(dep_path));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(forced_add_leaves_installed_dependency_alone, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * A dependency installed for a previous OS release is replaced from a sibling
 * file, but without force even when the named package is forced.
 */
ATF_TC_WITH_CLEANUP(add_replaces_stale_release_dependency);
ATF_TC_HEAD(add_replaces_stale_release_dependency, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "a dependency registered under an older os_release is reinstalled");
}
ATF_TC_BODY(add_replaces_stale_release_dependency, tc)
{
	mportInstance *mport;
	const char *depfile;
	const char *pkgfile;
	mportPackageMeta **dep = NULL;
	char *system_os_release;

	(void)tc;

	mport = create_test_instance();
	depfile = create_package(mport, "testdep", NULL);
	pkgfile = create_package(mport, PKG_NAME, "testdep");

	mport->offline = true;
	ATF_REQUIRE_MSG(mport_install_primative(mport, depfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	/* from an older release and a library provider, so it must be replaced */
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"UPDATE packages SET os_release='0.0-OLD', no_provide_shlib=0 WHERE pkg='testdep'"));

	mport->force = true;
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());

	ATF_REQUIRE_EQ(1, count_pkg(mport, PKG_NAME));
	ATF_REQUIRE_EQ(1, count_pkg(mport, "testdep"));
	ATF_REQUIRE_EQ(MPORT_OK, mport_pkgmeta_search_master(mport, &dep, "pkg=%Q", "testdep"));
	ATF_REQUIRE(dep != NULL && dep[0] != NULL);
	system_os_release = mport_get_osrelease(mport);
	ATF_REQUIRE(system_os_release != NULL);
	ATF_REQUIRE_STREQ(system_os_release, dep[0]->os_release);
	free(system_os_release);
	mport_pkgmeta_vec_free(dep);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(add_replaces_stale_release_dependency, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * mport add -l (mport->noDepends) installs only the named file.  A sibling
 * dependency file is never opened, so a missing dependency fails the install.
 */
ATF_TC_WITH_CLEANUP(local_only_add_ignores_sibling_dependencies);
ATF_TC_HEAD(local_only_add_ignores_sibling_dependencies, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "noDepends installs the named package file and nothing else");
}
ATF_TC_BODY(local_only_add_ignores_sibling_dependencies, tc)
{
	mportInstance *mport;
	const char *depfile;
	const char *pkgfile;

	(void)tc;

	mport = create_test_instance();
	depfile = create_package(mport, "testdep", NULL);
	pkgfile = create_package(mport, PKG_NAME, "testdep");

	mport->offline = true;
	mport->noDepends = true;
	ATF_REQUIRE(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) != MPORT_OK);
	ATF_REQUIRE_EQ(0, count_pkg(mport, PKG_NAME));
	ATF_REQUIRE_EQ(0, count_pkg(mport, "testdep"));
	ATF_REQUIRE_EQ(-1, access(test_path(PKG_PREFIX "/share/testdep/catalog.mk"), F_OK));

	/* with the dependency installed by hand, the named file goes in */
	ATF_REQUIRE_MSG(mport_install_primative(mport, depfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(1, count_pkg(mport, PKG_NAME));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(local_only_add_ignores_sibling_dependencies, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * A forced reinstall whose replacement cannot pass its prechecks must leave
 * the installed copy exactly as it was.  The replacement fails because the
 * dependency is registered for another OS release and no sibling file can
 * replace it.
 */
ATF_TC_WITH_CLEANUP(forced_reinstall_failing_precheck_keeps_installed_copy);
ATF_TC_HEAD(forced_reinstall_failing_precheck_keeps_installed_copy, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "a forced reinstall is verified before the installed copy is removed");
}
ATF_TC_BODY(forced_reinstall_failing_precheck_keeps_installed_copy, tc)
{
	mportInstance *mport;
	const char *depfile;
	const char *pkgfile;
	char pkg_path[PATH_MAX];

	(void)tc;

	mport = create_test_instance();
	depfile = create_package(mport, "testdep", NULL);
	pkgfile = create_package(mport, PKG_NAME, "testdep");

	mport->offline = true;
	ATF_REQUIRE_MSG(mport_install_primative(mport, depfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());

	(void)strlcpy(pkg_path, test_path(PKG_FILE_ABS), sizeof(pkg_path));
	write_file(pkg_path, "modified\n");
	/* from an older release and a library provider, so it must be replaced */
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"UPDATE packages SET os_release='0.0-OLD', no_provide_shlib=0 WHERE pkg='testdep'"));
	ATF_REQUIRE_EQ(0, unlink(depfile));

	mport->force = true;
	ATF_REQUIRE(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) != MPORT_OK);

	/* nothing was removed: the package, its files and the dependency row */
	ATF_REQUIRE_EQ(1, count_pkg(mport, PKG_NAME));
	ATF_REQUIRE_EQ(1, count_pkg(mport, "testdep"));
	ATF_REQUIRE_STREQ("modified\n", read_file(pkg_path));
	ATF_REQUIRE(count_rows(mport, "assets") > 0);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(forced_reinstall_failing_precheck_keeps_installed_copy, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * Replacing a copy registered for a previous OS release follows the same
 * rule: the stale copy is removed only after the replacement passes its
 * prechecks.
 */
ATF_TC_WITH_CLEANUP(stale_release_replacement_failing_precheck_keeps_copy);
ATF_TC_HEAD(stale_release_replacement_failing_precheck_keeps_copy, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr",
	    "a copy from an older os_release survives a replacement that fails precheck");
}
ATF_TC_BODY(stale_release_replacement_failing_precheck_keeps_copy, tc)
{
	mportInstance *mport;
	const char *depfile;
	const char *pkgfile;
	char os_release[64];

	(void)tc;

	mport = create_test_instance();
	depfile = create_package(mport, "testdep", NULL);
	pkgfile = create_package(mport, PKG_NAME, "testdep");

	mport->offline = true;
	ATF_REQUIRE_MSG(mport_install_primative(mport, depfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());

	/* both came from the previous release; only the package has a file to
	 * replace it with */
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"UPDATE packages SET os_release='0.0-OLD', no_provide_shlib=0 WHERE pkg IN (%Q, 'testdep')",
		PKG_NAME));
	ATF_REQUIRE_EQ(0, unlink(depfile));

	ATF_REQUIRE(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) != MPORT_OK);

	ATF_REQUIRE_EQ(1, count_installed(mport, os_release, sizeof(os_release)));
	ATF_REQUIRE_STREQ("0.0-OLD", os_release);
	ATF_REQUIRE_EQ(1, count_pkg(mport, "testdep"));
	ATF_REQUIRE_EQ(0, access(test_path(PKG_FILE_ABS), F_OK));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(stale_release_replacement_failing_precheck_keeps_copy, tc)
{
	(void)tc;

	cleanup_test_root();
}

/* Version string of the single registered copy of a package, or "". */
static const char *
installed_version(mportInstance *mport, const char *name)
{
	static char version[64];
	mportPackageMeta **found = NULL;

	version[0] = '\0';
	ATF_REQUIRE_EQ(MPORT_OK, mport_pkgmeta_search_master(mport, &found, "pkg=%Q", name));
	if (found != NULL && found[0] != NULL && found[0]->version != NULL)
		(void)strlcpy(version, found[0]->version, sizeof(version));
	if (found != NULL)
		mport_pkgmeta_vec_free(found);

	return version;
}

/*
 * A package file built for another OS release is refused, and the refusal
 * names both releases.  --allow-old-release (mport->allowOldRelease) and
 * MPORT_ALLOW_OLD_RELEASE each let it through.
 */
ATF_TC_WITH_CLEANUP(bundle_from_other_release_is_refused_unless_allowed);
ATF_TC_HEAD(bundle_from_other_release_is_refused_unless_allowed, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "a package file from another os_release is refused without the override");
}
ATF_TC_BODY(bundle_from_other_release_is_refused_unless_allowed, tc)
{
	mportInstance *mport;
	const char *pkgfile;
	char os_release[64];

	(void)tc;

	mport = create_test_instance();

	/* stamp the package file with a release the host is not running */
	ATF_REQUIRE_EQ(MPORT_OK, mport_setting_set(mport, MPORT_SETTING_TARGET_OS, "0.0-OLD"));
	pkgfile = create_test_package(mport);
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db, "DELETE FROM settings WHERE name=%Q", MPORT_SETTING_TARGET_OS));

	mport->offline = true;
	ATF_REQUIRE(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) != MPORT_OK);
	ATF_REQUIRE_MSG(strstr(mport_err_string(), "0.0-OLD") != NULL, "%s", mport_err_string());
	ATF_REQUIRE_MSG(strstr(mport_err_string(), "older") != NULL, "%s", mport_err_string());
	ATF_REQUIRE_EQ(0, count_installed(mport, NULL, 0));
	ATF_REQUIRE_EQ(-1, access(test_path(PKG_FILE_ABS), F_OK));

	/* the flag */
	mport->allowOldRelease = true;
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(1, count_installed(mport, os_release, sizeof(os_release)));
	ATF_REQUIRE_STREQ("0.0-OLD", os_release);
	ATF_REQUIRE_EQ(0, access(test_path(PKG_FILE_ABS), F_OK));
	mport_instance_free(mport);

	/* the environment variable, picked up by instance init */
	ATF_REQUIRE_EQ(0, setenv(MPORT_ALLOW_OLD_RELEASE_ENV, "1", 1));
	mport = mport_instance_new();
	ATF_REQUIRE(mport != NULL);
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_instance_init(mport, test_root, "root", false, MPORT_VQUIET));
	ATF_REQUIRE_EQ(0, unsetenv(MPORT_ALLOW_OLD_RELEASE_ENV));
	ATF_REQUIRE(mport->allowOldRelease);
	mport->offline = true;
	mport->force = true;
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(bundle_from_other_release_is_refused_unless_allowed, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * When several versions of a dependency sit next to the package file, the
 * newest is used, and a package that merely shares the name prefix
 * (testdep-extra) is not mistaken for it.
 */
ATF_TC_WITH_CLEANUP(sibling_search_picks_newest_exact_match);
ATF_TC_HEAD(sibling_search_picks_newest_exact_match, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "dependency lookup in the package directory takes the newest version");
}
ATF_TC_BODY(sibling_search_picks_newest_exact_match, tc)
{
	mportInstance *mport;
	const char *pkgfile;

	(void)tc;

	mport = create_test_instance();
	(void)create_package_version(mport, "testdep", "1.0", NULL);
	(void)create_package_version(mport, "testdep", "2.0", NULL);
	(void)create_package_version(mport, "testdep", "10.0", NULL);
	(void)create_package_version(mport, "testdep-extra", "99.0", NULL);
	pkgfile = create_package(mport, PKG_NAME, "testdep");

	mport->offline = true;
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(1, count_pkg(mport, PKG_NAME));
	ATF_REQUIRE_EQ(1, count_pkg(mport, "testdep"));
	ATF_REQUIRE_STREQ("10.0", installed_version(mport, "testdep"));
	ATF_REQUIRE_EQ(0, count_pkg(mport, "testdep-extra"));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(sibling_search_picks_newest_exact_match, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * The upgrade pass for packages from an older release keys off the
 * registered os_release compared with the target.
 */
ATF_TC_WITH_CLEANUP(stale_release_detection);
ATF_TC_HEAD(stale_release_detection, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr", "a package registered for an older release is stale");
}
ATF_TC_BODY(stale_release_detection, tc)
{
	mportInstance *mport;
	const char *pkgfile;
	mportPackageMeta **found = NULL;
	mportPackageMeta none;

	(void)tc;

	mport = create_test_instance();
	pkgfile = create_test_package(mport);
	mport->offline = true;
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());

	/* current release: not stale */
	ATF_REQUIRE_EQ(MPORT_OK, mport_pkgmeta_search_master(mport, &found, "pkg=%Q", PKG_NAME));
	ATF_REQUIRE(found != NULL && found[0] != NULL);
	ATF_REQUIRE(!mport_pkgmeta_is_stale_release(mport, found[0]));
	mport_pkgmeta_vec_free(found);
	found = NULL;

	/* older release: stale */
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(
		mport->db, "UPDATE packages SET os_release='0.0-OLD' WHERE pkg=%Q", PKG_NAME));
	ATF_REQUIRE_EQ(MPORT_OK, mport_pkgmeta_search_master(mport, &found, "pkg=%Q", PKG_NAME));
	ATF_REQUIRE(found != NULL && found[0] != NULL);
	ATF_REQUIRE(mport_pkgmeta_is_stale_release(mport, found[0]));
	mport_pkgmeta_vec_free(found);
	found = NULL;

	/* newer release or no recorded release: not stale */
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(
		mport->db, "UPDATE packages SET os_release='999.0' WHERE pkg=%Q", PKG_NAME));
	ATF_REQUIRE_EQ(MPORT_OK, mport_pkgmeta_search_master(mport, &found, "pkg=%Q", PKG_NAME));
	ATF_REQUIRE(found != NULL && found[0] != NULL);
	ATF_REQUIRE(!mport_pkgmeta_is_stale_release(mport, found[0]));
	mport_pkgmeta_vec_free(found);
	memset(&none, 0, sizeof(none));
	ATF_REQUIRE(!mport_pkgmeta_is_stale_release(mport, &none));
	ATF_REQUIRE(!mport_pkgmeta_is_stale_release(mport, NULL));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(stale_release_detection, tc)
{
	(void)tc;

	cleanup_test_root();
}

/* Value of no_provide_shlib for the single registered copy of a package. */
static int
provides_no_shlib(mportInstance *mport, const char *name)
{
	mportPackageMeta **found = NULL;
	int value = -1;

	ATF_REQUIRE_EQ(MPORT_OK, mport_pkgmeta_search_master(mport, &found, "pkg=%Q", name));
	if (found != NULL && found[0] != NULL)
		value = found[0]->no_provide_shlib;
	if (found != NULL)
		mport_pkgmeta_vec_free(found);

	return value;
}

/* Copy a file byte for byte; used to stage a real shared library. */
static void
copy_file(const char *from, const char *to)
{
	FILE *in, *out;
	char buf[8192];
	size_t n;

	in = fopen(from, "rb");
	ATF_REQUIRE(in != NULL);
	out = fopen(to, "wb");
	ATF_REQUIRE(out != NULL);
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
		ATF_REQUIRE_EQ(n, fwrite(buf, 1, n, out));
	ATF_REQUIRE_EQ(0, fclose(out));
	ATF_REQUIRE_EQ(0, fclose(in));
}

/*
 * Package creation records whether the package ships a shared library: a
 * package of plain files is marked as providing none, one that stages a
 * real .so is not.
 */
ATF_TC_WITH_CLEANUP(create_records_shared_library_provision);
ATF_TC_HEAD(create_records_shared_library_provision, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.files", "/lib/libz.so.6");
	atf_tc_set_md_var(tc, "descr", "no_provide_shlib is set from the staged files");
}
ATF_TC_BODY(create_records_shared_library_provision, tc)
{
	mportInstance *mport;
	mportAssetList *assetlist;
	mportPackageMeta *pack;
	mportCreateExtras *extra;
	const char *plainfile;
	FILE *fp;

	(void)tc;

	mport = create_test_instance();
	plainfile = create_test_package(mport);
	mport->offline = true;
	ATF_REQUIRE_MSG(mport_install_primative(mport, plainfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(1, provides_no_shlib(mport, PKG_NAME));

	/* a second package whose only file is a copy of libz */
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage-libpkg"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage-libpkg/usr"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage-libpkg/usr/local"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage-libpkg/usr/local/lib"), 0755));
	copy_file("/lib/libz.so.6", test_path("/stage-libpkg/usr/local/lib/libtestz.so.6"));
	write_file(test_path("/plist-libpkg"), "lib/libtestz.so.6\n");

	assetlist = mport_assetlist_new();
	ATF_REQUIRE(assetlist != NULL);
	fp = fopen(test_path("/plist-libpkg"), "r");
	ATF_REQUIRE(fp != NULL);
	ATF_REQUIRE_EQ(0, mport_parse_plistfile(fp, assetlist));
	(void)fclose(fp);

	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	pack->name = strdup("libpkg");
	pack->version = strdup(PKG_VERSION);
	pack->prefix = strdup(PKG_PREFIX);
	pack->origin = strdup("misc/libpkg");
	pack->lang = strdup("");
	pack->comment = strdup("shared library test package");
	pack->type = MPORT_TYPE_APP;

	extra = mport_createextras_new();
	ATF_REQUIRE(extra != NULL);
	(void)strlcpy(
	    extra->pkg_filename, test_path("/libpkg-1.0.mport"), sizeof(extra->pkg_filename));
	(void)strlcpy(extra->sourcedir, test_path("/stage-libpkg"), sizeof(extra->sourcedir));

	ATF_REQUIRE_MSG(mport_create_primative(mport, assetlist, pack, extra) == MPORT_OK, "%s",
	    mport_err_string());
	ATF_REQUIRE_EQ(0, pack->no_provide_shlib);
	mport_assetlist_free(assetlist);
	mport_pkgmeta_free(pack);
	mport_createextras_free(extra);

	ATF_REQUIRE_MSG(mport_install_primative(mport, test_path("/libpkg-1.0.mport"), NULL,
			    MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(0, provides_no_shlib(mport, "libpkg"));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(create_records_shared_library_provision, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * A dependency registered for another release is refused when it may
 * provide shared libraries, and accepted when it is recorded as providing
 * none.
 */
ATF_TC_WITH_CLEANUP(old_release_dependency_without_shlibs_is_accepted);
ATF_TC_HEAD(old_release_dependency_without_shlibs_is_accepted, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr",
	    "the dependency release check is waived for packages that provide no shared library");
}
ATF_TC_BODY(old_release_dependency_without_shlibs_is_accepted, tc)
{
	mportInstance *mport;
	const char *depfile;
	const char *pkgfile;

	(void)tc;

	mport = create_test_instance();
	depfile = create_package(mport, "testdep", NULL);
	pkgfile = create_package(mport, PKG_NAME, "testdep");

	mport->offline = true;
	mport->noDepends = true; /* no sibling replacement: the check must decide */
	ATF_REQUIRE_MSG(mport_install_primative(mport, depfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());

	/* from another release and possibly a library provider: refused */
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"UPDATE packages SET os_release='0.0-OLD', no_provide_shlib=0 WHERE pkg='testdep'"));
	ATF_REQUIRE(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) != MPORT_OK);
	ATF_REQUIRE_MSG(strstr(mport_err_string(), "0.0-OLD") != NULL, "%s", mport_err_string());
	ATF_REQUIRE_EQ(0, count_pkg(mport, PKG_NAME));

	/* same, but known to ship no shared library: accepted */
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db, "UPDATE packages SET no_provide_shlib=1 WHERE pkg='testdep'"));
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(1, count_pkg(mport, PKG_NAME));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(old_release_dependency_without_shlibs_is_accepted, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * A dependency from another release that still provides shared libraries
 * is accepted once every one of them is also provided by a package built
 * for the running release.
 */
ATF_TC_WITH_CLEANUP(old_release_dependency_with_superseded_libraries_is_accepted);
ATF_TC_HEAD(old_release_dependency_with_superseded_libraries_is_accepted, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr",
	    "the dependency release check is waived when a current package provides the same sonames");
}
ATF_TC_BODY(old_release_dependency_with_superseded_libraries_is_accepted, tc)
{
	mportInstance *mport;
	const char *depfile;
	const char *pkgfile;
	char *system_os_release;

	(void)tc;

	mport = create_test_instance();
	depfile = create_package(mport, "testdep", NULL);
	pkgfile = create_package(mport, PKG_NAME, "testdep");
	system_os_release = mport_get_osrelease(mport);
	ATF_REQUIRE(system_os_release != NULL);

	mport->offline = true;
	mport->noDepends = true;
	ATF_REQUIRE_MSG(mport_install_primative(mport, depfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());

	/* from another release and providing two libraries: refused */
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"UPDATE packages SET os_release='0.0-OLD', no_provide_shlib=0 WHERE pkg='testdep'"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO shlibs_provided (pkg, name) VALUES ('testdep', 'libq.so.1'), ('testdep', 'libr.so.2')"));
	ATF_REQUIRE(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) != MPORT_OK);
	ATF_REQUIRE_EQ(0, count_pkg(mport, PKG_NAME));

	/* a current-release package providing only one of them is not enough */
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO packages (pkg, version, origin, prefix, lang, status, os_release, cpe, no_provide_shlib) "
		"VALUES ('newq', '2.0', 'misc/newq', '/usr/local', '', 'clean', %Q, '', 0)",
		system_os_release));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(
		mport->db, "INSERT INTO shlibs_provided (pkg, name) VALUES ('newq', 'libq.so.1')"));
	ATF_REQUIRE(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) != MPORT_OK);
	ATF_REQUIRE_EQ(0, count_pkg(mport, PKG_NAME));

	/* both superseded: accepted */
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(
		mport->db, "INSERT INTO shlibs_provided (pkg, name) VALUES ('newq', 'libr.so.2')"));
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(1, count_pkg(mport, PKG_NAME));

	free(system_os_release);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(old_release_dependency_with_superseded_libraries_is_accepted, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * A delete that fails partway through unregistering the package must roll
 * back, leaving the package fully registered and the connection free for the
 * next package in the same run.
 */
ATF_TC_WITH_CLEANUP(failed_delete_rolls_back);
ATF_TC_HEAD(failed_delete_rolls_back, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "a delete that fails inside its transaction leaves the registry intact");
}
ATF_TC_BODY(failed_delete_rolls_back, tc)
{
	mportInstance *mport;
	mportPackageMeta **installed = NULL;
	const char *pkgfile;
	int assets;

	(void)tc;

	mport = create_test_instance();
	pkgfile = create_test_package(mport);

	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	assets = count_rows(mport, "assets");
	ATF_REQUIRE(assets > 0);

	/* annotation is the last table the delete touches, so every other
	 * table has already been cleared when this fires */
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"CREATE TRIGGER fail_delete BEFORE DELETE ON annotation BEGIN SELECT RAISE(ABORT, 'forced failure'); END"));

	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_pkgmeta_search_master(mport, &installed, "pkg=%Q", PKG_NAME));
	ATF_REQUIRE(installed != NULL && installed[0] != NULL);
	ATF_REQUIRE(mport_delete_primative(mport, installed[0], 1) != MPORT_OK);
	ATF_REQUIRE(strstr(mport_err_string(), "forced failure") != NULL);

	/* nothing was unregistered */
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));
	ATF_REQUIRE_EQ(assets, count_rows(mport, "assets"));
	ATF_REQUIRE(count_rows(mport, "annotation") > 0);

	/* the transaction was rolled back, so the next delete can begin one */
	ATF_REQUIRE_EQ(MPORT_OK, mport_db_do(mport->db, "DROP TRIGGER fail_delete"));
	ATF_REQUIRE_MSG(
	    mport_delete_primative(mport, installed[0], 1) == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE_EQ(0, count_installed(mport, NULL, 0));
	ATF_REQUIRE_EQ(0, count_rows(mport, "assets"));
	ATF_REQUIRE_EQ(0, count_rows(mport, "annotation"));

	mport_pkgmeta_vec_free(installed);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(failed_delete_rolls_back, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * A schema upgrade that fails part way must leave the registry at the version
 * it started from with none of the earlier steps applied, and must leave the
 * connection usable so a retry can succeed.
 */
ATF_TC_WITH_CLEANUP(failed_schema_upgrade_rolls_back);
ATF_TC_HEAD(failed_schema_upgrade_rolls_back, tc)
{
	atf_tc_set_md_var(tc, "descr", "a failed schema upgrade rolls back every step");
}
ATF_TC_BODY(failed_schema_upgrade_rolls_back, tc)
{
	mportInstance *mport;
	int settings = -1;

	(void)tc;

	mport = create_test_instance();
	ATF_REQUIRE_EQ(MPORT_MASTER_VERSION, mport_get_database_version(mport->db));

	/* pretend the registry is at schema 11 and lacks the rows step 11to12 adds */
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db, "DELETE FROM settings WHERE name IN (%Q, %Q)",
		MPORT_SETTING_HANDLE_RC_SCRIPTS, MPORT_SETTING_REPO_AUTOUPDATE));
	ATF_REQUIRE_EQ(MPORT_OK, mport_db_do(mport->db, "PRAGMA user_version=11"));
	ATF_REQUIRE_EQ(11, mport_get_database_version(mport->db));

	/* step 12to13 creates this table, so its presence makes that step fail
	 * after 11to12 has already run */
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db, "CREATE TABLE temp_settings (rowid int, name text, val text)"));

	ATF_REQUIRE(mport_upgrade_master_schema(mport->db, 11) != MPORT_OK);

	/* version and the rows added by the earlier step are both rolled back */
	ATF_REQUIRE_EQ(11, mport_get_database_version(mport->db));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_count(mport->db, &settings,
		"SELECT COUNT(*) FROM settings WHERE name IN (%Q, %Q)",
		MPORT_SETTING_HANDLE_RC_SCRIPTS, MPORT_SETTING_REPO_AUTOUPDATE));
	ATF_REQUIRE_EQ(0, settings);

	/* no transaction is left open, so a retry goes through */
	ATF_REQUIRE_EQ(MPORT_OK, mport_db_do(mport->db, "DROP TABLE temp_settings"));
	ATF_REQUIRE_MSG(
	    mport_upgrade_master_schema(mport->db, 11) == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE_EQ(MPORT_MASTER_VERSION, mport_get_database_version(mport->db));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_count(mport->db, &settings,
		"SELECT COUNT(*) FROM settings WHERE name IN (%Q, %Q)",
		MPORT_SETTING_HANDLE_RC_SCRIPTS, MPORT_SETTING_REPO_AUTOUPDATE));
	ATF_REQUIRE_EQ(2, settings);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(failed_schema_upgrade_rolls_back, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * MidnightBSD does not expose arbitrary descriptors through /dev/fd/N.
 * Verify package installation can retain the verified descriptor instead of
 * reopening that unavailable path.
 */
ATF_TC_WITH_CLEANUP(install_from_verified_fd);
ATF_TC_HEAD(install_from_verified_fd, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr", "installs a hash-verified package from its open descriptor");
}
ATF_TC_BODY(install_from_verified_fd, tc)
{
	mportInstance *mport;
	const char *pkgfile;
	char *hash;
	int fd;

	(void)tc;

	mport = create_test_instance();
	pkgfile = create_test_package(mport);
	hash = mport_hash_file(pkgfile);
	ATF_REQUIRE(hash != NULL);
	fd = open(pkgfile, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
	ATF_REQUIRE(fd >= 0);
	ATF_REQUIRE_EQ(1, mport_verify_hash_fd(fd, hash));
	ATF_REQUIRE_MSG(mport_install_primative_fd(mport, fd, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(0, close(fd));
	free(hash);
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));
	ATF_REQUIRE_EQ(0, access(test_path(PKG_FILE_ABS), F_OK));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(install_from_verified_fd, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * Installing from a descriptor hands libarchive a duplicate.  Every install
 * must give that duplicate back, and it must never be inherited by hook
 * subprocesses, or long dependency transactions run out of descriptors.
 */
ATF_TC_WITH_CLEANUP(fd_install_releases_duplicate_descriptor);
ATF_TC_HEAD(fd_install_releases_duplicate_descriptor, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "fd-based bundle reads close their close-on-exec duplicate on finish");
}
ATF_TC_BODY(fd_install_releases_duplicate_descriptor, tc)
{
	mportInstance *mport;
	mportBundleRead *bundle;
	const char *pkgfile;
	int fd, probe, baseline, i;

	(void)tc;

	mport = create_test_instance();
	pkgfile = create_test_package(mport);
	fd = open(pkgfile, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
	ATF_REQUIRE(fd >= 0);

	/* The lowest free descriptor is the baseline; any leak raises it. */
	baseline = dup(fd);
	ATF_REQUIRE(baseline >= 0);
	ATF_REQUIRE_EQ(0, close(baseline));

	bundle = mport_bundle_read_new();
	ATF_REQUIRE(bundle != NULL);
	ATF_REQUIRE_EQ(-1, bundle->archive_fd);
	ATF_REQUIRE_MSG(
	    mport_bundle_read_init_fd(bundle, fd) == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE(bundle->archive_fd >= 0);
	ATF_REQUIRE(bundle->archive_fd != fd);
	ATF_REQUIRE_EQ(FD_CLOEXEC, fcntl(bundle->archive_fd, F_GETFD) & FD_CLOEXEC);
	ATF_REQUIRE_EQ(MPORT_OK, mport_bundle_read_finish(mport, bundle));

	probe = dup(fd);
	ATF_REQUIRE_EQ(baseline, probe);
	ATF_REQUIRE_EQ(0, close(probe));

	for (i = 0; i < 8; i++) {
		bundle = mport_bundle_read_new();
		ATF_REQUIRE(bundle != NULL);
		ATF_REQUIRE_MSG(
		    mport_bundle_read_init_fd(bundle, fd) == MPORT_OK, "%s", mport_err_string());
		ATF_REQUIRE_EQ(MPORT_OK, mport_bundle_read_finish(mport, bundle));
	}
	ATF_REQUIRE_MSG(mport_install_primative_fd(mport, fd, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));

	probe = dup(fd);
	ATF_REQUIRE_EQ(baseline, probe);
	ATF_REQUIRE_EQ(0, close(probe));
	ATF_REQUIRE_EQ(0, close(fd));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(fd_install_releases_duplicate_descriptor, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * An update that fails its preconditions must be reported to the caller,
 * not only through the message callback, and must leave the installed
 * copy alone.
 */
ATF_TC_WITH_CLEANUP(failed_update_returns_error);
ATF_TC_HEAD(failed_update_returns_error, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "mport_update_primative returns an error when an update fails");
}
ATF_TC_BODY(failed_update_returns_error, tc)
{
	mportInstance *mport;
	const char *oldfile;
	const char *newfile;

	(void)tc;

	mport = create_test_instance();
	oldfile = create_package_version(mport, PKG_NAME, "1.0", NULL);
	newfile = create_package_version(mport, PKG_NAME, "2.0", NULL);

	ATF_REQUIRE_MSG(mport_install_primative(mport, newfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_STREQ("2.0", installed_version(mport, PKG_NAME));

	/* 2.0 is installed, so the 1.0 bundle is not an upgrade */
	ATF_REQUIRE(mport_update_primative(mport, oldfile) != MPORT_OK);
	ATF_REQUIRE(mport_err_string() != NULL && mport_err_string()[0] != '\0');
	ATF_REQUIRE_STREQ("2.0", installed_version(mport, PKG_NAME));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(failed_update_returns_error, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(update_upgrades_installed_package);
ATF_TC_HEAD(update_upgrades_installed_package, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr", "mport_update_primative upgrades an older installed copy");
}
ATF_TC_BODY(update_upgrades_installed_package, tc)
{
	mportInstance *mport;
	const char *oldfile;
	const char *newfile;

	(void)tc;

	mport = create_test_instance();
	oldfile = create_package_version(mport, PKG_NAME, "1.0", NULL);
	newfile = create_package_version(mport, PKG_NAME, "2.0", NULL);

	ATF_REQUIRE_MSG(mport_install_primative(mport, oldfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_MSG(
	    mport_update_primative(mport, newfile) == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE_STREQ("2.0", installed_version(mport, PKG_NAME));
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));
	ATF_REQUIRE_EQ(0, access(test_path(PKG_FILE_ABS), F_OK));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(update_upgrades_installed_package, tc)
{
	(void)tc;

	cleanup_test_root();
}

/* A locked package is skipped on purpose; that is not a failure. */
ATF_TC_WITH_CLEANUP(update_skips_locked_package);
ATF_TC_HEAD(update_skips_locked_package, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr", "mport_update_primative leaves a locked package alone");
}
ATF_TC_BODY(update_skips_locked_package, tc)
{
	mportInstance *mport;
	mportPackageMeta **found = NULL;
	const char *oldfile;
	const char *newfile;

	(void)tc;

	mport = create_test_instance();
	oldfile = create_package_version(mport, PKG_NAME, "1.0", NULL);
	newfile = create_package_version(mport, PKG_NAME, "2.0", NULL);

	ATF_REQUIRE_MSG(mport_install_primative(mport, oldfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(MPORT_OK, mport_pkgmeta_search_master(mport, &found, "pkg=%Q", PKG_NAME));
	ATF_REQUIRE(found != NULL && found[0] != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_lock_lock(mport, found[0]));
	mport_pkgmeta_vec_free(found);

	ATF_REQUIRE_MSG(
	    mport_update_primative(mport, newfile) == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE_STREQ("1.0", installed_version(mport, PKG_NAME));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(update_skips_locked_package, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * An update shows the new package's upgrade entries whose version range
 * matches the version it replaces, and none of its install or remove
 * entries; a delete shows only the remove entries.
 */
ATF_TC_WITH_CLEANUP(update_shows_upgrade_messages_for_replaced_version);
ATF_TC_HEAD(update_shows_upgrade_messages_for_replaced_version, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "install, update and delete show the pkg-message entries for that action");
}
ATF_TC_BODY(update_shows_upgrade_messages_for_replaced_version, tc)
{
	mportInstance *mport;
	mportPackageMeta **found = NULL;
	const char *oldfile;
	const char *newfile;
	static const char new_message[] = "[\n"
					  "{ type: install, message: \"install-2.0\" }\n"
					  "{ type: upgrade, message: \"upgrade-any\" }\n"
					  "{ type: upgrade, maximum_version: \"1.5\",\n"
					  "  message: \"upgrade-from-before-1.5\" }\n"
					  "{ type: upgrade, maximum_version: \"0.5\",\n"
					  "  message: \"upgrade-from-before-0.5\" }\n"
					  "{ type: upgrade, minimum_version: \"1.0\",\n"
					  "  message: \"upgrade-from-after-1.0\" }\n"
					  "{ type: remove, message: \"remove-2.0\" }\n"
					  "]\n";

	(void)tc;

	mport = create_test_instance();
	mport->msg_cb = capture_message;
	oldfile = create_package_message(mport, PKG_NAME, "1.0", NULL,
	    "[\n{ type: install, message: \"install-1.0\" }\n"
	    "{ type: remove, message: \"remove-1.0\" }\n]\n");
	newfile = create_package_message(mport, PKG_NAME, "2.0", NULL, new_message);

	shown_messages[0] = '\0';
	ATF_REQUIRE_MSG(mport_install_primative(mport, oldfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE(strstr(shown_messages, "install-1.0|") != NULL);
	ATF_REQUIRE(strstr(shown_messages, "remove-1.0") == NULL);

	shown_messages[0] = '\0';
	ATF_REQUIRE_MSG(
	    mport_update_primative(mport, newfile) == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE_STREQ("2.0", installed_version(mport, PKG_NAME));
	ATF_REQUIRE_MSG(strstr(shown_messages, "upgrade-any|upgrade-from-before-1.5|") != NULL,
	    "%s", shown_messages);
	ATF_REQUIRE_MSG(
	    strstr(shown_messages, "upgrade-from-before-0.5") == NULL, "%s", shown_messages);
	ATF_REQUIRE_MSG(
	    strstr(shown_messages, "upgrade-from-after-1.0") == NULL, "%s", shown_messages);
	ATF_REQUIRE_MSG(strstr(shown_messages, "install-") == NULL, "%s", shown_messages);
	ATF_REQUIRE_MSG(strstr(shown_messages, "remove-") == NULL, "%s", shown_messages);

	shown_messages[0] = '\0';
	ATF_REQUIRE_EQ(MPORT_OK, mport_pkgmeta_search_master(mport, &found, "pkg=%Q", PKG_NAME));
	ATF_REQUIRE(found != NULL && found[0] != NULL);
	ATF_REQUIRE_MSG(
	    mport_delete_primative(mport, found[0], 1) == MPORT_OK, "%s", mport_err_string());
	mport_pkgmeta_vec_free(found);
	ATF_REQUIRE_MSG(strstr(shown_messages, "remove-2.0|") != NULL, "%s", shown_messages);
	ATF_REQUIRE_MSG(strstr(shown_messages, "install-") == NULL, "%s", shown_messages);
	ATF_REQUIRE_MSG(strstr(shown_messages, "upgrade-") == NULL, "%s", shown_messages);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(update_shows_upgrade_messages_for_replaced_version, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, install_replaces_previous_os_release);
	ATF_TP_ADD_TC(tp, install_same_os_release_is_rejected);
	ATF_TP_ADD_TC(tp, install_from_verified_fd);
	ATF_TP_ADD_TC(tp, fd_install_releases_duplicate_descriptor);
	ATF_TP_ADD_TC(tp, force_reinstall_over_orphaned_rows);
	ATF_TP_ADD_TC(tp, failed_install_registers_nothing);
	ATF_TP_ADD_TC(tp, invalid_age_annotation_is_reported);
	ATF_TP_ADD_TC(tp, bundle_annotations_are_registered);
	ATF_TP_ADD_TC(tp, underage_install_is_refused);
	ATF_TP_ADD_TC(tp, age_within_bracket_is_installed);
	ATF_TP_ADD_TC(tp, adult_install_is_never_refused);
	ATF_TP_ADD_TC(tp, age_not_required_by_region_installs);
	ATF_TP_ADD_TC(tp, age_check_without_login_installs);
	ATF_TP_ADD_TC(tp, age_check_without_aged_installs);
	ATF_TP_ADD_TC(tp, age_check_with_aged_enabled_but_down_is_refused);
	ATF_TP_ADD_TC(tp, age_check_with_aged_disabled_installs);
	ATF_TP_ADD_TC(tp, unset_age_is_refused_with_agectl_hint);
	ATF_TP_ADD_TC(tp, dir_asset_through_symlinked_parent);
	ATF_TP_ADD_TC(tp, add_installs_missing_dependency_from_sibling);
	ATF_TP_ADD_TC(tp, forced_add_leaves_installed_dependency_alone);
	ATF_TP_ADD_TC(tp, add_replaces_stale_release_dependency);
	ATF_TP_ADD_TC(tp, local_only_add_ignores_sibling_dependencies);
	ATF_TP_ADD_TC(tp, forced_reinstall_failing_precheck_keeps_installed_copy);
	ATF_TP_ADD_TC(tp, stale_release_replacement_failing_precheck_keeps_copy);
	ATF_TP_ADD_TC(tp, bundle_from_other_release_is_refused_unless_allowed);
	ATF_TP_ADD_TC(tp, sibling_search_picks_newest_exact_match);
	ATF_TP_ADD_TC(tp, stale_release_detection);
	ATF_TP_ADD_TC(tp, create_records_shared_library_provision);
	ATF_TP_ADD_TC(tp, old_release_dependency_without_shlibs_is_accepted);
	ATF_TP_ADD_TC(tp, old_release_dependency_with_superseded_libraries_is_accepted);
	ATF_TP_ADD_TC(tp, failed_delete_rolls_back);
	ATF_TP_ADD_TC(tp, failed_schema_upgrade_rolls_back);
	ATF_TP_ADD_TC(tp, failed_update_returns_error);
	ATF_TP_ADD_TC(tp, update_upgrades_installed_package);
	ATF_TP_ADD_TC(tp, update_skips_locked_package);
	ATF_TP_ADD_TC(tp, update_shows_upgrade_messages_for_replaced_version);

	return atf_no_error();
}
