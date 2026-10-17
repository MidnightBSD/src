#include <sys/cdefs.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <elf.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libmport/mport.h"
#include "../libmport/mport_private.h"

/* SPLINT_SKIP_FILE: Splint cannot parse/model ATF test macros and fixture setup. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullpass -nullret -nullstate -paramuse@*/
/*@-retvalint -retvalother -type -unrecog@*/

#define TEST_ROOT_TEMPLATE "/tmp/mport-shlib-test-root.XXXXXX"
#define LIBZ "/lib/libz.so.6"
#define GZIP "/usr/bin/gzip"
#define LIB32_LIBZ "/usr/lib32/libz.so.6"

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
make_test_root(void)
{
	(void)strlcpy(test_root, TEST_ROOT_TEMPLATE, sizeof(test_root));
	ATF_REQUIRE(mkdtemp(test_root) != NULL);
}

static void
cleanup_test_root(void)
{
	if (test_root[0] != '\0' && access(test_root, F_OK) == 0)
		(void)mport_rmtree(test_root);
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

static void
copy_file(const char *from, const char *to)
{
	FILE *in, *out;
	char buf[8192];
	size_t n;

	/* ATF_REQUIRE aborts the case on a NULL stream; cppcheck cannot see that */
	/* cppcheck-suppress-begin nullPointerOutOfResources */
	in = fopen(from, "rb");
	ATF_REQUIRE(in != NULL);
	out = fopen(to, "wb");
	ATF_REQUIRE(out != NULL);
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
		ATF_REQUIRE_EQ(n, fwrite(buf, 1, n, out));
	ATF_REQUIRE_EQ(0, fclose(out));
	ATF_REQUIRE_EQ(0, fclose(in));
	/* cppcheck-suppress-end nullPointerOutOfResources */
}

/* contents of a small file, or "" if it cannot be read */
static const char *
read_file(const char *path)
{
	static char contents[1024];
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

static bool
list_has(const stringlist_t *list, const char *s)
{
	tll_foreach(*list, it)
	{
		if (strcmp(it->item, s) == 0)
			return true;
	}
	return false;
}

static void
clear_shlib_env(void)
{
	static const char *const names[] = { "SHLIB_PROVIDE_PATHS_NATIVE",
		"SHLIB_PROVIDE_PATHS_COMPAT_32", "SHLIB_PROVIDE_PATHS_COMPAT_LINUX",
		"SHLIB_PROVIDE_PATHS_COMPAT_LINUX_32", "SHLIB_PROVIDE_IGNORE_GLOB",
		"SHLIB_PROVIDE_IGNORE_REGEX", "SHLIB_REQUIRE_IGNORE_GLOB",
		"SHLIB_REQUIRE_IGNORE_REGEX" };

	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
		(void)unsetenv(names[i]);
}

/* ---- mport_shlib_analyse_elf ------------------------------------------ */

ATF_TC(analyse_shared_library);
ATF_TC_HEAD(analyse_shared_library, tc)
{
	atf_tc_set_md_var(tc, "require.files", LIBZ);
	atf_tc_set_md_var(tc, "descr", "a shared library yields its soname and its needs");
}
ATF_TC_BODY(analyse_shared_library, tc)
{
	char *provided = NULL;
	int flags = -1;
	stringlist_t required = tll_init();

	(void)tc;

	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_analyse_elf(LIBZ, &provided, &flags, &required));
	ATF_REQUIRE(provided != NULL);
	ATF_REQUIRE_STREQ("libz.so.6", provided);
	ATF_REQUIRE_EQ(MPORT_SHLIB_NATIVE, flags);
	ATF_REQUIRE(list_has(&required, "libc.so.7"));
	ATF_REQUIRE(!list_has(&required, "libz.so.6"));

	free(provided);
	tll_free_and_free(required, free);
}

ATF_TC(analyse_executable);
ATF_TC_HEAD(analyse_executable, tc)
{
	atf_tc_set_md_var(tc, "require.files", GZIP);
	atf_tc_set_md_var(tc, "descr", "an executable provides nothing and lists its needs");
}
ATF_TC_BODY(analyse_executable, tc)
{
	char *provided = NULL;
	int flags = -1;
	stringlist_t required = tll_init();

	(void)tc;

	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_analyse_elf(GZIP, &provided, &flags, &required));
	ATF_REQUIRE(provided == NULL);
	ATF_REQUIRE(list_has(&required, "libz.so.6"));
	ATF_REQUIRE(list_has(&required, "libc.so.7"));

	tll_free_and_free(required, free);
}

ATF_TC_WITH_CLEANUP(analyse_plain_file);
ATF_TC_HEAD(analyse_plain_file, tc)
{
	atf_tc_set_md_var(tc, "descr", "a non-ELF file is skipped without error");
}
ATF_TC_BODY(analyse_plain_file, tc)
{
	char *provided = NULL;
	int flags = -1;
	stringlist_t required = tll_init();

	(void)tc;

	make_test_root();
	write_file(test_path("/notes.txt"), "not an elf\n");
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_shlib_analyse_elf(test_path("/notes.txt"), &provided, &flags, &required));
	ATF_REQUIRE(provided == NULL);
	ATF_REQUIRE_EQ(0, tll_length(required));
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_shlib_analyse_elf(test_path("/absent"), &provided, &flags, &required));
	ATF_REQUIRE(provided == NULL);
}
ATF_TC_CLEANUP(analyse_plain_file, tc)
{
	(void)tc;

	cleanup_test_root();
}

#define LINUX_LIBZ "/compat/linux/lib64/libz.so.1"

ATF_TC(analyse_linux_object);
ATF_TC_HEAD(analyse_linux_object, tc)
{
	atf_tc_set_md_var(tc, "require.files", LINUX_LIBZ);
	atf_tc_set_md_var(tc, "descr", "an object not tagged for FreeBSD is a Linux object");
}
ATF_TC_BODY(analyse_linux_object, tc)
{
	char *provided = NULL;
	int flags = -1;
	stringlist_t required = tll_init();

	(void)tc;

	char target[PATH_MAX];

	/* the compat path is a symlink; analyse_elf reads regular files only */
	ATF_REQUIRE(realpath(LINUX_LIBZ, target) != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_analyse_elf(target, &provided, &flags, &required));
	ATF_REQUIRE(provided != NULL);
	ATF_REQUIRE_STREQ("libz.so.1", provided);
	ATF_REQUIRE_EQ(MPORT_SHLIB_LINUX, flags);
	ATF_REQUIRE(list_has(&required, "libc.so.6:Linux"));

	free(provided);
	tll_free_and_free(required, free);
}

ATF_TC(name_with_flags);
ATF_TC_HEAD(name_with_flags, tc)
{
	atf_tc_set_md_var(tc, "descr", "non-native objects carry the pkg(8) suffixes");
}
ATF_TC_BODY(name_with_flags, tc)
{
	char *s;

	(void)tc;

	s = mport_shlib_name_with_flags("libfoo.so.1", MPORT_SHLIB_NATIVE);
	ATF_REQUIRE_STREQ("libfoo.so.1", s);
	free(s);
	s = mport_shlib_name_with_flags("libfoo.so.1", MPORT_SHLIB_COMPAT_32);
	ATF_REQUIRE_STREQ("libfoo.so.1:32", s);
	free(s);
	s = mport_shlib_name_with_flags("libfoo.so.1", MPORT_SHLIB_LINUX);
	ATF_REQUIRE_STREQ("libfoo.so.1:Linux", s);
	free(s);
	s = mport_shlib_name_with_flags("libfoo.so.1", MPORT_SHLIB_LINUX | MPORT_SHLIB_COMPAT_32);
	ATF_REQUIRE_STREQ("libfoo.so.1:Linux:32", s);
	free(s);
}

#define LDSO "/usr/libexec/ld-elf.so.1"

static void stage_lib_and_user(const char **, const char **);

ATF_TC(analyse_object_without_soname);
ATF_TC_HEAD(analyse_object_without_soname, tc)
{
	atf_tc_set_md_var(tc, "require.files", LDSO);
	atf_tc_set_md_var(tc, "descr", "an ET_DYN object with no DT_SONAME provides nothing");
}
ATF_TC_BODY(analyse_object_without_soname, tc)
{
	char *provided = NULL;
	int flags = -1;
	stringlist_t required = tll_init();

	(void)tc;

	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_analyse_elf(LDSO, &provided, &flags, &required));
	ATF_REQUIRE(provided == NULL);
	tll_free_and_free(required, free);
}

ATF_TC_WITH_CLEANUP(analyse_refuses_symlink);
ATF_TC_HEAD(analyse_refuses_symlink, tc)
{
	atf_tc_set_md_var(tc, "require.files", LIBZ);
	atf_tc_set_md_var(tc, "descr", "analyse_elf does not follow a symlink itself");
}
ATF_TC_BODY(analyse_refuses_symlink, tc)
{
	char *provided = NULL;
	int flags = -1;
	stringlist_t required = tll_init();

	(void)tc;

	make_test_root();
	ATF_REQUIRE_EQ(0, symlink(LIBZ, test_path("/link.so")));
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_shlib_analyse_elf(test_path("/link.so"), &provided, &flags, &required));
	ATF_REQUIRE(provided == NULL);
	ATF_REQUIRE_EQ(0, tll_length(required));
}
ATF_TC_CLEANUP(analyse_refuses_symlink, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(scan_symlinks_stay_inside_the_stage);
ATF_TC_HEAD(scan_symlinks_stay_inside_the_stage, tc)
{
	atf_tc_set_md_var(tc, "require.files", LIBZ " " GZIP);
	atf_tc_set_md_var(tc, "descr",
	    "a link to a staged library is read; one pointing at the build host is ignored");
}
ATF_TC_BODY(scan_symlinks_stay_inside_the_stage, tc)
{
	mportShlibScan *scan;
	mportPackageMeta *pack;
	const char *lib, *bin;

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);
	/* the real library lives in a private directory; the provide path holds
	 * links: one relative into the stage, one absolute as the package would
	 * install it (resolved against the stage root), one to the host */
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage/usr/local/lib/private"), 0755));
	ATF_REQUIRE_EQ(0, rename(lib, test_path("/stage/usr/local/lib/private/libtestz.so.6")));
	ATF_REQUIRE_EQ(
	    0, symlink("private/libtestz.so.6", test_path("/stage/usr/local/lib/libz.so.6")));
	ATF_REQUIRE_EQ(0,
	    symlink("/usr/local/lib/private/libtestz.so.6",
		test_path("/stage/usr/local/lib/libabs.so")));
	ATF_REQUIRE_EQ(0, symlink("/lib/libc.so.7", test_path("/stage/usr/local/lib/libhost.so")));
	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_PATHS_NATIVE", "/usr/local/lib", 1));

	scan = mport_shlib_scan_new();
	ATF_REQUIRE(scan != NULL);
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_shlib_scan_file(scan, test_path("/stage/usr/local/lib/private/libtestz.so.6"),
		"/usr/local/lib/private/libtestz.so.6"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_shlib_scan_file(
		scan, test_path("/stage/usr/local/lib/libz.so.6"), "/usr/local/lib/libz.so.6"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_shlib_scan_file(
		scan, test_path("/stage/usr/local/lib/libabs.so"), "/usr/local/lib/libabs.so"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_shlib_scan_file(
		scan, test_path("/stage/usr/local/lib/libhost.so"), "/usr/local/lib/libhost.so"));

	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_finish(scan, pack));

	/* libz.so.6 is provided through the in-stage links; the host's libc
	 * never entered the picture */
	ATF_REQUIRE_EQ(1, tll_length(pack->shlibs_provided));
	ATF_REQUIRE(list_has(&pack->shlibs_provided, "libz.so.6"));
	ATF_REQUIRE(!list_has(&pack->shlibs_provided, "libc.so.7"));

	mport_pkgmeta_free(pack);
	mport_shlib_scan_free(scan);
	clear_shlib_env();
}
ATF_TC_CLEANUP(scan_symlinks_stay_inside_the_stage, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(scan_normalises_installed_path);
ATF_TC_HEAD(scan_normalises_installed_path, tc)
{
	atf_tc_set_md_var(tc, "require.files", LIBZ);
	atf_tc_set_md_var(tc, "descr",
	    "doubled slashes, dot and dot-dot components do not defeat the path filter");
}
ATF_TC_BODY(scan_normalises_installed_path, tc)
{
	mportShlibScan *scan;
	mportPackageMeta *pack;
	const char *lib, *bin;
	static const char *const spellings[] = { "/usr/local//lib/libtestz.so.6",
		"/usr/local/./lib/libtestz.so.6", "/usr/local/lib/../lib/libtestz.so.6",
		"usr/local/lib/libtestz.so.6", NULL };

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);
	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_PATHS_NATIVE", "/usr/local/lib", 1));

	for (const char *const *sp = spellings; *sp != NULL; sp++) {
		scan = mport_shlib_scan_new();
		ATF_REQUIRE(scan != NULL);
		ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_file(scan, lib, *sp));
		pack = mport_pkgmeta_new();
		ATF_REQUIRE(pack != NULL);
		ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_finish(scan, pack));
		ATF_REQUIRE_MSG(
		    list_has(&pack->shlibs_provided, "libz.so.6"), "not provided for %s", *sp);
		mport_pkgmeta_free(pack);
		mport_shlib_scan_free(scan);
	}

	clear_shlib_env();
}
ATF_TC_CLEANUP(scan_normalises_installed_path, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC(bad_ignore_regex_is_an_error);
ATF_TC_HEAD(bad_ignore_regex_is_an_error, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "a pattern that does not compile stops the scan, not silently");
}
ATF_TC_BODY(bad_ignore_regex_is_an_error, tc)
{
	mportShlibScan *scan;

	(void)tc;

	clear_shlib_env();
	ATF_REQUIRE_EQ(0, setenv("SHLIB_REQUIRE_IGNORE_REGEX", "lib(unclosed", 1));
	scan = mport_shlib_scan_new();
	ATF_REQUIRE(scan == NULL);
	ATF_REQUIRE_MSG(strstr(mport_err_string(), "SHLIB_REQUIRE_IGNORE_REGEX") != NULL, "%s",
	    mport_err_string());
	clear_shlib_env();

	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_IGNORE_REGEX", "^libz\\.so, ^libfoo", 1));
	scan = mport_shlib_scan_new();
	ATF_REQUIRE(scan != NULL);
	mport_shlib_scan_free(scan);
	clear_shlib_env();
}

/* ---- the scan over a staged package ----------------------------------- */

/* stage a copy of libz as lib/libtestz.so.6 and gzip as bin/gzip */
static void
stage_lib_and_user(const char **libpath, const char **binpath)
{
	make_test_root();
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage/usr"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage/usr/local"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage/usr/local/lib"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage/usr/local/bin"), 0755));
	copy_file(LIBZ, test_path("/stage/usr/local/lib/libtestz.so.6"));
	copy_file(GZIP, test_path("/stage/usr/local/bin/gzip"));
	*libpath = test_path("/stage/usr/local/lib/libtestz.so.6");
	*binpath = test_path("/stage/usr/local/bin/gzip");
}

ATF_TC_WITH_CLEANUP(scan_provides_and_requires);
ATF_TC_HEAD(scan_provides_and_requires, tc)
{
	atf_tc_set_md_var(tc, "require.files", LIBZ " " GZIP);
	atf_tc_set_md_var(tc, "descr",
	    "a library in a provide path is provided and cancels the package's own need for it");
}
ATF_TC_BODY(scan_provides_and_requires, tc)
{
	mportShlibScan *scan;
	mportPackageMeta *pack;
	const char *lib, *bin;

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);
	ATF_REQUIRE_EQ(
	    0, setenv("SHLIB_PROVIDE_PATHS_NATIVE", "/usr/local/lib,/usr/local/lib/foo", 1));

	scan = mport_shlib_scan_new();
	ATF_REQUIRE(scan != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_file(scan, lib, "/usr/local/lib/libtestz.so.6"));
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_file(scan, bin, "/usr/local/bin/gzip"));

	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_finish(scan, pack));

	ATF_REQUIRE_EQ(1, tll_length(pack->shlibs_provided));
	ATF_REQUIRE(list_has(&pack->shlibs_provided, "libz.so.6"));
	/* gzip needs libz, but the package ships it: not a requirement */
	ATF_REQUIRE(!list_has(&pack->shlibs_required, "libz.so.6"));
	ATF_REQUIRE(list_has(&pack->shlibs_required, "libc.so.7"));
	ATF_REQUIRE_EQ(0, pack->no_provide_shlib);

	mport_pkgmeta_free(pack);
	mport_shlib_scan_free(scan);
	clear_shlib_env();
}
ATF_TC_CLEANUP(scan_provides_and_requires, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(scan_private_library);
ATF_TC_HEAD(scan_private_library, tc)
{
	atf_tc_set_md_var(tc, "require.files", LIBZ " " GZIP);
	atf_tc_set_md_var(tc, "descr",
	    "a library outside the provide paths is private: not provided, still not required");
}
ATF_TC_BODY(scan_private_library, tc)
{
	mportShlibScan *scan;
	mportPackageMeta *pack;
	const char *lib, *bin;

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);
	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_PATHS_NATIVE", "/usr/local/lib64", 1));

	scan = mport_shlib_scan_new();
	ATF_REQUIRE(scan != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_file(scan, lib, "/usr/local/lib/libtestz.so.6"));
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_file(scan, bin, "/usr/local/bin/gzip"));

	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_finish(scan, pack));

	ATF_REQUIRE_EQ(0, tll_length(pack->shlibs_provided));
	ATF_REQUIRE_EQ(1, pack->no_provide_shlib);
	ATF_REQUIRE(!list_has(&pack->shlibs_required, "libz.so.6"));
	ATF_REQUIRE(list_has(&pack->shlibs_required, "libc.so.7"));

	mport_pkgmeta_free(pack);
	mport_shlib_scan_free(scan);
	clear_shlib_env();
}
ATF_TC_CLEANUP(scan_private_library, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(scan_unset_paths_provide_everything);
ATF_TC_HEAD(scan_unset_paths_provide_everything, tc)
{
	atf_tc_set_md_var(tc, "require.files", LIBZ);
	atf_tc_set_md_var(tc, "descr", "with no provide paths every library is provided");
}
ATF_TC_BODY(scan_unset_paths_provide_everything, tc)
{
	mportShlibScan *scan;
	mportPackageMeta *pack;
	const char *lib, *bin;

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);

	scan = mport_shlib_scan_new();
	ATF_REQUIRE(scan != NULL);
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_shlib_scan_file(scan, lib, "/usr/local/libexec/odd/libtestz.so.6"));

	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_finish(scan, pack));
	ATF_REQUIRE(list_has(&pack->shlibs_provided, "libz.so.6"));

	mport_pkgmeta_free(pack);
	mport_shlib_scan_free(scan);
}
ATF_TC_CLEANUP(scan_unset_paths_provide_everything, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(scan_ignore_lists);
ATF_TC_HEAD(scan_ignore_lists, tc)
{
	atf_tc_set_md_var(tc, "require.files", LIBZ " " GZIP);
	atf_tc_set_md_var(
	    tc, "descr", "glob and regex ignore lists drop provided and required names");
}
ATF_TC_BODY(scan_ignore_lists, tc)
{
	mportShlibScan *scan;
	mportPackageMeta *pack;
	const char *lib, *bin;

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);
	ATF_REQUIRE_EQ(0, setenv("SHLIB_REQUIRE_IGNORE_GLOB", "libbz2.so.*, libc.so.*", 1));
	ATF_REQUIRE_EQ(0, setenv("SHLIB_REQUIRE_IGNORE_REGEX", "^liblzma\\.so\\.[0-9]+$", 1));
	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_IGNORE_REGEX", "^libz\\.so", 1));

	scan = mport_shlib_scan_new();
	ATF_REQUIRE(scan != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_file(scan, lib, "/usr/local/lib/libtestz.so.6"));
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_file(scan, bin, "/usr/local/bin/gzip"));

	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_finish(scan, pack));

	ATF_REQUIRE_EQ(0, tll_length(pack->shlibs_provided));
	ATF_REQUIRE_EQ(1, pack->no_provide_shlib);
	/* gzip needs libz (shipped), liblzma, libprivatezstd, libbz2 and libc;
	 * the glob drops libbz2 and libc, the regex drops liblzma */
	ATF_REQUIRE(!list_has(&pack->shlibs_required, "libz.so.6"));
	ATF_REQUIRE(!list_has(&pack->shlibs_required, "libc.so.7"));
	ATF_REQUIRE(!list_has(&pack->shlibs_required, "libbz2.so.4"));
	ATF_REQUIRE(!list_has(&pack->shlibs_required, "liblzma.so.5"));
	ATF_REQUIRE(list_has(&pack->shlibs_required, "libprivatezstd.so.5"));
	ATF_REQUIRE_EQ(1, tll_length(pack->shlibs_required));

	mport_pkgmeta_free(pack);
	mport_shlib_scan_free(scan);
	clear_shlib_env();
}
ATF_TC_CLEANUP(scan_ignore_lists, tc)
{
	(void)tc;

	cleanup_test_root();
}

/* ---- the lists reach the package file --------------------------------- */

ATF_TC_WITH_CLEANUP(create_stores_shlib_tables);
ATF_TC_HEAD(create_stores_shlib_tables, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.files", LIBZ " " GZIP);
	atf_tc_set_md_var(
	    tc, "descr", "mport_create_primative records shlibs_provided and shlibs_required");
}
ATF_TC_BODY(create_stores_shlib_tables, tc)
{
	mportInstance *mport;
	mportAssetList *assetlist;
	mportPackageMeta *pack;
	mportCreateExtras *extra;
	mportBundleRead *bundle;
	const char *lib, *bin;
	FILE *fp;
	int count;

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);
	ATF_REQUIRE_EQ(0, mkdir(test_path("/var"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/var/db"), 0755));
	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_PATHS_NATIVE", "/usr/local/lib", 1));

	mport = mport_instance_new();
	ATF_REQUIRE(mport != NULL);
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_instance_init(mport, test_root, "root", false, MPORT_VQUIET));

	write_file(test_path("/plist"), "lib/libtestz.so.6\nbin/gzip\n");
	assetlist = mport_assetlist_new();
	ATF_REQUIRE(assetlist != NULL);
	/* cppcheck-suppress-begin nullPointerOutOfResources */
	fp = fopen(test_path("/plist"), "r");
	ATF_REQUIRE(fp != NULL);
	ATF_REQUIRE_EQ(0, mport_parse_plistfile(fp, assetlist));
	(void)fclose(fp);
	/* cppcheck-suppress-end nullPointerOutOfResources */

	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	pack->name = strdup("shlibpkg");
	pack->version = strdup("1.0");
	pack->prefix = strdup("/usr/local");
	pack->origin = strdup("misc/shlibpkg");
	pack->lang = strdup("");
	pack->comment = strdup("shared library test package");
	pack->type = MPORT_TYPE_APP;

	extra = mport_createextras_new();
	ATF_REQUIRE(extra != NULL);
	(void)strlcpy(
	    extra->pkg_filename, test_path("/shlibpkg-1.0.mport"), sizeof(extra->pkg_filename));
	(void)strlcpy(extra->sourcedir, test_path("/stage"), sizeof(extra->sourcedir));

	ATF_REQUIRE_MSG(mport_create_primative(mport, assetlist, pack, extra) == MPORT_OK, "%s",
	    mport_err_string());

	/* the caller's meta carries the settled lists */
	ATF_REQUIRE(list_has(&pack->shlibs_provided, "libz.so.6"));
	ATF_REQUIRE(list_has(&pack->shlibs_required, "libc.so.7"));
	ATF_REQUIRE(!list_has(&pack->shlibs_required, "libz.so.6"));
	ATF_REQUIRE_EQ(0, pack->no_provide_shlib);

	/* and so does the package file */
	bundle = mport_bundle_read_new();
	ATF_REQUIRE(bundle != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_bundle_read_init(bundle, test_path("/shlibpkg-1.0.mport")));
	ATF_REQUIRE_MSG(mport_bundle_read_prep_for_install(mport, bundle) == MPORT_OK, "%s",
	    mport_err_string());
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_count(mport->db, &count,
		"SELECT count(*) FROM stub.shlibs_provided WHERE pkg='shlibpkg' AND name='libz.so.6'"));
	ATF_REQUIRE_EQ(1, count);
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_count(mport->db, &count,
		"SELECT count(*) FROM stub.shlibs_required WHERE pkg='shlibpkg' AND name='libc.so.7'"));
	ATF_REQUIRE_EQ(1, count);
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_count(mport->db, &count,
		"SELECT count(*) FROM stub.shlibs_required WHERE pkg='shlibpkg' AND name='libz.so.6'"));
	ATF_REQUIRE_EQ(0, count);
	ATF_REQUIRE_EQ(MPORT_OK, mport_bundle_read_finish(mport, bundle));

	mport_assetlist_free(assetlist);
	mport_pkgmeta_free(pack);
	mport_createextras_free(extra);
	mport_instance_free(mport);
	clear_shlib_env();
}
ATF_TC_CLEANUP(create_stores_shlib_tables, tc)
{
	(void)tc;

	cleanup_test_root();
}


/* ---- the registry --------------------------------------------------- */

/* a test instance rooted in test_root, with the stage already prepared */
static mportInstance *
registry_instance(void)
{
	mportInstance *mport;

	ATF_REQUIRE_EQ(0, mkdir(test_path("/var"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/var/db"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/usr"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/usr/local"), 0755));
	mport = mport_instance_new();
	ATF_REQUIRE(mport != NULL);
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_instance_init(mport, test_root, "root", false, MPORT_VQUIET));
	mport->offline = true;
	return mport;
}

static const char *build_shlibpkg_with(
    mportInstance *, const char *, const char *, const char *);

/* build shlibpkg-1.0.mport from the staged libz copy and gzip */
static const char *
build_shlibpkg(mportInstance *mport)
{
	return build_shlibpkg_with(mport, "shlibpkg", "lib/libtestz.so.6\nbin/gzip\n", NULL);
}

/* the same with a chosen name, plist and optional post-install Lua script */
static const char *
build_shlibpkg_with(
    mportInstance *mport, const char *name, const char *plist, const char *postinstall)
{
	mportAssetList *assetlist;
	mportPackageMeta *pack;
	mportCreateExtras *extra;
	FILE *fp;
	char buf[PATH_MAX];

	write_file(test_path("/plist"), plist);
	assetlist = mport_assetlist_new();
	ATF_REQUIRE(assetlist != NULL);
	fp = fopen(test_path("/plist"), "r");
	ATF_REQUIRE(fp != NULL);
	ATF_REQUIRE_EQ(0, mport_parse_plistfile(fp, assetlist));
	(void)fclose(fp);

	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	pack->name = strdup(name);
	pack->version = strdup("1.0");
	pack->prefix = strdup("/usr/local");
	(void)snprintf(buf, sizeof(buf), "misc/%s", name);
	pack->origin = strdup(buf);
	pack->lang = strdup("");
	pack->comment = strdup("shared library test package");
	pack->type = MPORT_TYPE_APP;

	extra = mport_createextras_new();
	ATF_REQUIRE(extra != NULL);
	(void)snprintf(extra->pkg_filename, sizeof(extra->pkg_filename), "%s/%s-1.0.mport",
	    test_root, name);
	(void)strlcpy(extra->sourcedir, test_path("/stage"), sizeof(extra->sourcedir));
	if (postinstall != NULL) {
		/* scripts are stored as a UCL array of chunks, one file per hook */
		(void)snprintf(buf, sizeof(buf), "%s/pkg-post-install.lua", test_root);
		write_file(buf, postinstall);
		extra->luapkgpostinstall = strdup(buf);
		ATF_REQUIRE(extra->luapkgpostinstall != NULL);
	}

	ATF_REQUIRE_MSG(mport_create_primative(mport, assetlist, pack, extra) == MPORT_OK, "%s",
	    mport_err_string());
	mport_assetlist_free(assetlist);
	mport_pkgmeta_free(pack);
	mport_createextras_free(extra);

	(void)snprintf(buf, sizeof(buf), "/%s-1.0.mport", name);
	return test_path(buf);
}

ATF_TC_WITH_CLEANUP(install_registers_and_delete_unregisters);
ATF_TC_HEAD(install_registers_and_delete_unregisters, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.files", LIBZ " " GZIP);
	atf_tc_set_md_var(tc, "descr", "the registry carries the package's lists while it is installed");
}
ATF_TC_BODY(install_registers_and_delete_unregisters, tc)
{
	mportInstance *mport;
	const char *pkgfile;
	const char *lib, *bin;
	stringlist_t prov = tll_init();
	stringlist_t req = tll_init();
	mportPackageMeta **packs = NULL;
	int count;

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);
	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_PATHS_NATIVE", "/usr/local/lib", 1));
	mport = registry_instance();
	pkgfile = build_shlibpkg(mport);

	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());

	/* the lists */
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlibs_get(mport, "shlibpkg", &prov, &req));
	ATF_REQUIRE_EQ(1, tll_length(prov));
	ATF_REQUIRE(list_has(&prov, "libz.so.6"));
	ATF_REQUIRE(list_has(&req, "libc.so.7"));
	ATF_REQUIRE(list_has(&req, "liblzma.so.5"));
	ATF_REQUIRE(!list_has(&req, "libz.so.6"));
	tll_free_and_free(prov, free);
	tll_free_and_free(req, free);

	/* who provides, who requires; a path is accepted */
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_providers(mport, "/usr/local/lib/libz.so.6", &packs));
	ATF_REQUIRE(packs != NULL && packs[0] != NULL && packs[1] == NULL);
	ATF_REQUIRE_STREQ("shlibpkg", packs[0]->name);
	mport_pkgmeta_vec_free(packs);
	packs = NULL;
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_requirers(mport, "libc.so.7", &packs));
	ATF_REQUIRE(packs != NULL && packs[0] != NULL);
	ATF_REQUIRE_STREQ("shlibpkg", packs[0]->name);
	mport_pkgmeta_vec_free(packs);
	packs = NULL;
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_providers(mport, "libnothere.so.9", &packs));
	ATF_REQUIRE(packs == NULL);

	/* delete takes the rows with it */
	ATF_REQUIRE_EQ(MPORT_OK, mport_pkgmeta_search_master(mport, &packs, "pkg=%Q", "shlibpkg"));
	ATF_REQUIRE(packs != NULL && packs[0] != NULL);
	ATF_REQUIRE_MSG(mport_delete_primative(mport, packs[0], 1) == MPORT_OK, "%s",
	    mport_err_string());
	mport_pkgmeta_vec_free(packs);
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_count(mport->db, &count, "SELECT count(*) FROM shlibs_provided"));
	ATF_REQUIRE_EQ(0, count);
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_count(mport->db, &count, "SELECT count(*) FROM shlibs_required"));
	ATF_REQUIRE_EQ(0, count);

	mport_instance_free(mport);
	clear_shlib_env();
}
ATF_TC_CLEANUP(install_registers_and_delete_unregisters, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC(base_system_libraries);
ATF_TC_HEAD(base_system_libraries, tc)
{
	atf_tc_set_md_var(tc, "require.files", "/lib/libc.so.7");
	atf_tc_set_md_var(tc, "descr", "libraries the base system ships are recognised by class");
}
ATF_TC_BODY(base_system_libraries, tc)
{
	(void)tc;

	ATF_REQUIRE(mport_shlib_in_base("libc.so.7"));
	ATF_REQUIRE(mport_shlib_in_base("/some/where/libc.so.7"));
	ATF_REQUIRE(!mport_shlib_in_base("libnothere.so.9"));
	ATF_REQUIRE(!mport_shlib_in_base(""));
	ATF_REQUIRE(!mport_shlib_in_base(NULL));
	/* a native name is not looked for under the Linux or 32-bit trees */
	ATF_REQUIRE(!mport_shlib_in_base("libc.so.7:Linux"));
	if (access("/compat/linux/lib64/libc.so.6", F_OK) == 0)
		ATF_REQUIRE(mport_shlib_in_base("libc.so.6:Linux"));
}

ATF_TC_WITH_CLEANUP(missing_shlib_check);
ATF_TC_HEAD(missing_shlib_check, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.files", LIBZ " " GZIP);
	atf_tc_set_md_var(tc, "descr",
	    "a requirement nothing provides is reported; base and package provided ones are not");
}
ATF_TC_BODY(missing_shlib_check, tc)
{
	mportInstance *mport;
	const char *pkgfile;
	const char *lib, *bin;

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);
	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_PATHS_NATIVE", "/usr/local/lib", 1));
	mport = registry_instance();
	pkgfile = build_shlibpkg(mport);
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());

	/* everything gzip needs is in base or in the package */
	ATF_REQUIRE_EQ(0, mport_check_missing_shlibs(mport));

	/* pretend one requirement is for a library nobody ships */
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"UPDATE shlibs_required SET name='libnothere.so.9' WHERE name='liblzma.so.5'"));
	ATF_REQUIRE_EQ(1, mport_check_missing_shlibs(mport));

	/* a second package providing it clears the report */
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO shlibs_provided (pkg, name) VALUES ('other', 'libnothere.so.9')"));
	ATF_REQUIRE_EQ(0, mport_check_missing_shlibs(mport));

	mport_instance_free(mport);
	clear_shlib_env();
}
ATF_TC_CLEANUP(missing_shlib_check, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(schema_upgrade_adds_tables);
ATF_TC_HEAD(schema_upgrade_adds_tables, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr", "a version 14 registry gains the shared library tables");
}
ATF_TC_BODY(schema_upgrade_adds_tables, tc)
{
	mportInstance *mport;
	int count;

	(void)tc;

	make_test_root();
	mport = registry_instance();

	/* a fresh registry has them */
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_count(mport->db, &count,
		"SELECT count(*) FROM sqlite_master WHERE type='table' AND name IN ('shlibs_provided', 'shlibs_required')"));
	ATF_REQUIRE_EQ(2, count);

	/* roll it back to 14 and upgrade again */
	ATF_REQUIRE_EQ(MPORT_OK, mport_db_do(mport->db, "DROP TABLE shlibs_provided"));
	ATF_REQUIRE_EQ(MPORT_OK, mport_db_do(mport->db, "DROP TABLE shlibs_required"));
	ATF_REQUIRE_EQ(MPORT_OK, mport_upgrade_master_schema(mport->db, 14));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_count(mport->db, &count,
		"SELECT count(*) FROM sqlite_master WHERE type='table' AND name IN ('shlibs_provided', 'shlibs_required')"));
	ATF_REQUIRE_EQ(2, count);
	ATF_REQUIRE_EQ(MPORT_MASTER_VERSION, mport_get_database_version(mport->db));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(schema_upgrade_adds_tables, tc)
{
	(void)tc;

	cleanup_test_root();
}


static char last_messages[4096];

static void
capture_msg(const char *msg)
{
	size_t len = strlen(last_messages);

	(void)snprintf(last_messages + len, sizeof(last_messages) - len, "%s\n", msg);
}

ATF_TC_WITH_CLEANUP(install_warns_about_unprovided_libraries);
ATF_TC_HEAD(install_warns_about_unprovided_libraries, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.files", LIBZ " " GZIP);
	atf_tc_set_md_var(tc, "descr",
	    "a required library no package provides and base lacks is warned about, not refused");
}
ATF_TC_BODY(install_warns_about_unprovided_libraries, tc)
{
	mportInstance *mport;
	mportBundleRead *bundle;
	mportPackageMeta *pack;
	const char *pkgfile;
	const char *lib, *bin;

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);
	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_PATHS_NATIVE", "/usr/local/lib", 1));
	mport = registry_instance();
	pkgfile = build_shlibpkg(mport);
	mport->msg_cb = capture_msg;

	/* everything gzip needs is in base or shipped: no warning on install */
	last_messages[0] = '\0';
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_MSG(strstr(last_messages, "no installed package provides") == NULL, "%s",
	    last_messages);

	/* open the same package file, rewrite one requirement in its stub to a
	 * library nobody ships, and run the install-time check against it */
	bundle = mport_bundle_read_new();
	ATF_REQUIRE(bundle != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_bundle_read_init(bundle, pkgfile));
	ATF_REQUIRE_MSG(mport_bundle_read_prep_for_install(mport, bundle) == MPORT_OK, "%s",
	    mport_err_string());
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"UPDATE stub.shlibs_required SET name='libnothere.so.9' WHERE name='liblzma.so.5'"));
	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	pack->name = strdup("shlibpkg");
	pack->version = strdup("1.0");

	last_messages[0] = '\0';
	ATF_REQUIRE_EQ(1, mport_shlibs_warn_missing(mport, pack));
	ATF_REQUIRE_MSG(strstr(last_messages, "Warning: shlibpkg-1.0 needs libnothere.so.9") != NULL,
	    "%s", last_messages);

	mport_pkgmeta_free(pack);
	ATF_REQUIRE_EQ(MPORT_OK, mport_bundle_read_finish(mport, bundle));
	mport_instance_free(mport);
	clear_shlib_env();
}
ATF_TC_CLEANUP(install_warns_about_unprovided_libraries, tc)
{
	(void)tc;

	cleanup_test_root();
}


/* ---- scripts and the plist --------------------------------------------- */

ATF_TC_WITH_CLEANUP(lua_scripts_see_shlib_lists);
ATF_TC_HEAD(lua_scripts_see_shlib_lists, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.files", LIBZ " " GZIP);
	atf_tc_set_md_var(
	    tc, "descr", "pkg_shlibs_provided and pkg_shlibs_required reach a post-install script");
}
ATF_TC_BODY(lua_scripts_see_shlib_lists, tc)
{
	mportInstance *mport;
	const char *pkgfile;
	const char *lib, *bin;
	const char *out;

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);
	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_PATHS_NATIVE", "/usr/local/lib", 1));
	mport = registry_instance();
	pkgfile = build_shlibpkg_with(mport, "shlibpkg", "lib/libtestz.so.6\nbin/gzip\n",
	    "[\"local f = io.open('/shlibs.txt', 'w') local nl = string.char(10) "
	    "for _, n in ipairs(pkg_shlibs_provided) do f:write('P ', n, nl) end "
	    "for _, n in ipairs(pkg_shlibs_required) do f:write('R ', n, nl) end "
	    "f:close()\"]");

	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());

	out = read_file(test_path("/shlibs.txt"));
	ATF_REQUIRE_MSG(strstr(out, "P libz.so.6\n") != NULL, "script output: %s", out);
	ATF_REQUIRE_MSG(strstr(out, "R libc.so.7\n") != NULL, "script output: %s", out);
	ATF_REQUIRE_MSG(strstr(out, "R libz.so.6") == NULL, "script output: %s", out);

	mport_instance_free(mport);
	clear_shlib_env();
}
ATF_TC_CLEANUP(lua_scripts_see_shlib_lists, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(create_warns_when_ldconfig_and_scan_disagree);
ATF_TC_HEAD(create_warns_when_ldconfig_and_scan_disagree, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.files", LIBZ);
	atf_tc_set_md_var(tc, "descr", "@ldconfig without a provided library, and the reverse, warn");
}
ATF_TC_BODY(create_warns_when_ldconfig_and_scan_disagree, tc)
{
	mportInstance *mport;
	const char *lib, *bin;

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);
	mport = registry_instance();
	mport->msg_cb = capture_msg;

	/* @ldconfig, but the library is not under the provide paths */
	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_PATHS_NATIVE", "/usr/local/lib64", 1));
	last_messages[0] = '\0';
	(void)build_shlibpkg_with(mport, "ldc1", "lib/libtestz.so.6\n@ldconfig\n", NULL);
	ATF_REQUIRE_MSG(strstr(last_messages, "plist has @ldconfig but no shared library") != NULL,
	    "%s", last_messages);

	/* a provided library, but no @ldconfig */
	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_PATHS_NATIVE", "/usr/local/lib", 1));
	last_messages[0] = '\0';
	(void)build_shlibpkg_with(mport, "ldc2", "lib/libtestz.so.6\n", NULL);
	ATF_REQUIRE_MSG(strstr(last_messages, "plist has no @ldconfig") != NULL, "%s",
	    last_messages);

	/* both agree: silence */
	last_messages[0] = '\0';
	(void)build_shlibpkg_with(mport, "ldc3", "lib/libtestz.so.6\n@ldconfig\n", NULL);
	ATF_REQUIRE_MSG(strstr(last_messages, "Warning") == NULL, "%s", last_messages);

	/* no libraries and no keyword: silence */
	last_messages[0] = '\0';
	(void)build_shlibpkg_with(mport, "ldc4", "bin/gzip\n", NULL);
	ATF_REQUIRE_MSG(strstr(last_messages, "Warning") == NULL, "%s", last_messages);

	mport_instance_free(mport);
	clear_shlib_env();
}
ATF_TC_CLEANUP(create_warns_when_ldconfig_and_scan_disagree, tc)
{
	(void)tc;

	cleanup_test_root();
}


ATF_TC_WITH_CLEANUP(plain_lua_file_runs_as_one_script);
ATF_TC_HEAD(plain_lua_file_runs_as_one_script, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.files", LIBZ " " GZIP);
	atf_tc_set_md_var(tc, "descr", "a hook file that is plain Lua, not a UCL array, runs too");
}
ATF_TC_BODY(plain_lua_file_runs_as_one_script, tc)
{
	mportInstance *mport;
	const char *pkgfile;
	const char *lib, *bin;
	const char *out;

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);
	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_PATHS_NATIVE", "/usr/local/lib", 1));
	mport = registry_instance();
	pkgfile = build_shlibpkg_with(mport, "shlibpkg", "lib/libtestz.so.6\nbin/gzip\n",
	    "-- plain lua, two lines\n"
	    "local f = io.open('/plain.txt', 'w')\n"
	    "f:write(pkg_name, ' ', #pkg_shlibs_provided, ' ', #pkg_shlibs_required > 0 and 'req' or 'none')\n"
	    "f:close()\n");

	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());

	out = read_file(test_path("/plain.txt"));
	ATF_REQUIRE_STREQ("shlibpkg 1 req", out);

	mport_instance_free(mport);
	clear_shlib_env();
}
ATF_TC_CLEANUP(plain_lua_file_runs_as_one_script, tc)
{
	(void)tc;

	cleanup_test_root();
}

/* ---- ABI_FILE: the target's word size, not the host's ------------------ */

ATF_TC(analyse_for_32bit_target);
ATF_TC_HEAD(analyse_for_32bit_target, tc)
{
	atf_tc_set_md_var(tc, "require.files", LIBZ " " LIB32_LIBZ);
	atf_tc_set_md_var(tc, "descr",
	    "a 32-bit object is native for a 32-bit target, and a 64-bit one is skipped");
}
ATF_TC_BODY(analyse_for_32bit_target, tc)
{
	char *provided = NULL;
	int flags = -1;
	stringlist_t required = tll_init();

	(void)tc;

#if defined(__LP64__)
	/* the host's view, unchanged */
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_analyse_elf(LIB32_LIBZ, &provided, &flags, &required));
	ATF_REQUIRE_STREQ("libz.so.6", provided);
	ATF_REQUIRE_EQ(MPORT_SHLIB_COMPAT_32, flags);
	ATF_REQUIRE(list_has(&required, "libc.so.7:32"));
	free(provided);
	tll_free_and_free(required, free);
#endif

	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_shlib_analyse_elf_for(ELFCLASS32, LIB32_LIBZ, &provided, &flags, &required));
	ATF_REQUIRE_STREQ("libz.so.6", provided);
	ATF_REQUIRE_EQ(MPORT_SHLIB_NATIVE, flags);
	ATF_REQUIRE(list_has(&required, "libc.so.7"));
	ATF_REQUIRE(!list_has(&required, "libc.so.7:32"));
	free(provided);
	tll_free_and_free(required, free);

	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_shlib_analyse_elf_for(ELFCLASS32, LIBZ, &provided, &flags, &required));
	ATF_REQUIRE(provided == NULL);
	ATF_REQUIRE_EQ(0, (int)tll_length(required));

	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_shlib_analyse_elf_for(ELFCLASS64, LIBZ, &provided, &flags, &required));
	ATF_REQUIRE_STREQ("libz.so.6", provided);
	ATF_REQUIRE_EQ(MPORT_SHLIB_NATIVE, flags);
	free(provided);
	tll_free_and_free(required, free);
}

ATF_TC(scan_uses_abi_file_class);
ATF_TC_HEAD(scan_uses_abi_file_class, tc)
{
	atf_tc_set_md_var(tc, "require.files", LIB32_LIBZ);
	atf_tc_set_md_var(tc, "descr", "the scan judges word size against ABI_FILE when it is set");
}
ATF_TC_BODY(scan_uses_abi_file_class, tc)
{
	mportShlibScan *scan;
	mportPackageMeta *pack;

	(void)tc;

	clear_shlib_env();
	ATF_REQUIRE_EQ(0, setenv("ABI_FILE", LIB32_LIBZ, 1));

	scan = mport_shlib_scan_new();
	ATF_REQUIRE(scan != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_file(scan, LIB32_LIBZ, "/usr/lib/libz.so.6"));
	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_finish(scan, pack));
	ATF_REQUIRE(list_has(&pack->shlibs_provided, "libz.so.6"));
	ATF_REQUIRE(!list_has(&pack->shlibs_provided, "libz.so.6:32"));
	ATF_REQUIRE(list_has(&pack->shlibs_required, "libc.so.7"));
	mport_pkgmeta_free(pack);
	mport_shlib_scan_free(scan);

	/* an ABI_FILE that cannot be read stops the scan rather than use the host */
	ATF_REQUIRE_EQ(0, setenv("ABI_FILE", "/nonexistent/abi-file", 1));
	ATF_REQUIRE(mport_shlib_scan_new() == NULL);

	ATF_REQUIRE_EQ(0, unsetenv("ABI_FILE"));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, analyse_for_32bit_target);
	ATF_TP_ADD_TC(tp, scan_uses_abi_file_class);
	ATF_TP_ADD_TC(tp, analyse_shared_library);
	ATF_TP_ADD_TC(tp, analyse_executable);
	ATF_TP_ADD_TC(tp, analyse_plain_file);
	ATF_TP_ADD_TC(tp, analyse_linux_object);
	ATF_TP_ADD_TC(tp, name_with_flags);
	ATF_TP_ADD_TC(tp, analyse_object_without_soname);
	ATF_TP_ADD_TC(tp, analyse_refuses_symlink);
	ATF_TP_ADD_TC(tp, scan_symlinks_stay_inside_the_stage);
	ATF_TP_ADD_TC(tp, scan_normalises_installed_path);
	ATF_TP_ADD_TC(tp, bad_ignore_regex_is_an_error);
	ATF_TP_ADD_TC(tp, scan_provides_and_requires);
	ATF_TP_ADD_TC(tp, scan_private_library);
	ATF_TP_ADD_TC(tp, scan_unset_paths_provide_everything);
	ATF_TP_ADD_TC(tp, scan_ignore_lists);
	ATF_TP_ADD_TC(tp, create_stores_shlib_tables);
	ATF_TP_ADD_TC(tp, install_registers_and_delete_unregisters);
	ATF_TP_ADD_TC(tp, base_system_libraries);
	ATF_TP_ADD_TC(tp, missing_shlib_check);
	ATF_TP_ADD_TC(tp, schema_upgrade_adds_tables);
	ATF_TP_ADD_TC(tp, install_warns_about_unprovided_libraries);
	ATF_TP_ADD_TC(tp, lua_scripts_see_shlib_lists);
	ATF_TP_ADD_TC(tp, plain_lua_file_runs_as_one_script);
	ATF_TP_ADD_TC(tp, create_warns_when_ldconfig_and_scan_disagree);

	return atf_no_error();
}
