#include <sys/cdefs.h>
#include <sys/stat.h>
#include <sys/wait.h>

#if __has_include(<sys/capsicum.h>)
#include <sys/capsicum.h>
#define TEST_HAVE_CAPSICUM 1
#endif

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libmport/mport.h"
#include "../libmport/mport_private.h"
#include "../libmport/mport_lua.h"

/* SPLINT_SKIP_FILE: Splint cannot parse/model ATF test macros and fixture setup. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullpass -nullret -nullstate -paramuse@*/
/*@-retvalint -retvalother -type -unrecog@*/

#define TEST_ROOT_TEMPLATE "/tmp/mport-trigger-test-root.XXXXXX"
#define PKG_VERSION "1.0"
#define PKG_PREFIX "/usr/local"
#define SYS_TRIGGERS "/usr/share/mport/triggers"
#define LOCAL_TRIGGERS "/usr/local/share/pkg/triggers"

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
}

static void
write_file(const char *path, const char *contents)
{
	int fd;
	size_t len;

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	ATF_REQUIRE_MSG(fd >= 0, "open %s: %s", path, strerror(errno));
	len = strlen(contents);
	ATF_REQUIRE_EQ((ssize_t)len, write(fd, contents, len));
	ATF_REQUIRE_EQ(0, close(fd));
}

/* mkdir -p under the test root */
static void
mkdir_p(const char *rel)
{
	char path[PATH_MAX];
	char *p;

	(void)snprintf(path, sizeof(path), "%s%s", test_root, rel);
	for (p = path + strlen(test_root) + 1; *p != '\0'; p++) {
		if (*p == '/') {
			*p = '\0';
			(void)mkdir(path, 0755);
			*p = '/';
		}
	}
	(void)mkdir(path, 0755);
}

/* Contents of a small file under the test root, or "" if it cannot be read. */
static const char *
read_marker(const char *rel)
{
	static char contents[512];
	FILE *fp;
	size_t n;

	contents[0] = '\0';
	fp = fopen(test_path(rel), "r");
	if (fp == NULL)
		return contents;
	n = fread(contents, 1, sizeof(contents) - 1, fp);
	contents[n] = '\0';
	(void)fclose(fp);

	return contents;
}

static bool
marker_exists(const char *rel)
{
	return access(test_path(rel), F_OK) == 0;
}

static mportInstance *
create_test_instance(void)
{
	mportInstance *mport;

	(void)strlcpy(test_root, TEST_ROOT_TEMPLATE, sizeof(test_root));
	ATF_REQUIRE(mkdtemp(test_root) != NULL);
	mkdir_p("/var/db");
	mkdir_p(PKG_PREFIX);

	mport = mport_instance_new();
	ATF_REQUIRE(mport != NULL);
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_instance_init(mport, test_root, "root", false, MPORT_VQUIET));

	return mport;
}

/* Write a trigger file at <rel_dir>/<name> under the test root. */
static void
write_trigger(const char *rel_dir, const char *name, const char *contents)
{
	char rel[PATH_MAX];

	mkdir_p(rel_dir);
	(void)snprintf(rel, sizeof(rel), "%s/%s", rel_dir, name);
	write_file(test_path(rel), contents);
}

/*
 * Build a package named `name` installing each of `files` (prefix-relative,
 * NULL terminated) and return the bundle path.  `contents`, when not NULL,
 * gives the body of each file; a NULL entry or NULL array means a one-line
 * placeholder.
 */
static const char *
create_package_contents(
    mportInstance *mport, const char *name, const char *const *files, const char *const *contents)
{
	mportAssetList *assetlist;
	mportPackageMeta *pack;
	mportCreateExtras *extra;
	FILE *fp;
	char stage[PATH_MAX];
	char buf[PATH_MAX];
	char plist[4096];
	int i;

	(void)snprintf(stage, sizeof(stage), "%s/stage-%s", test_root, name);
	ATF_REQUIRE_EQ(0, mkdir(stage, 0755));

	plist[0] = '\0';
	for (i = 0; files[i] != NULL; i++) {
		char *slash;

		(void)snprintf(buf, sizeof(buf), "%s%s/%s", stage, PKG_PREFIX, files[i]);
		slash = strrchr(buf, '/');
		*slash = '\0';
		/* mkdir -p the staging parent */
		for (char *p = buf + strlen(stage) + 1; *p != '\0'; p++) {
			if (*p == '/') {
				*p = '\0';
				(void)mkdir(buf, 0755);
				*p = '/';
			}
		}
		(void)mkdir(buf, 0755);
		*slash = '/';
		write_file(
		    buf, (contents != NULL && contents[i] != NULL) ? contents[i] : "content\n");
		(void)strlcat(plist, files[i], sizeof(plist));
		(void)strlcat(plist, "\n", sizeof(plist));
	}
	(void)snprintf(buf, sizeof(buf), "%s/plist-%s", test_root, name);
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
	pack->version = strdup(PKG_VERSION);
	pack->prefix = strdup(PKG_PREFIX);
	(void)snprintf(buf, sizeof(buf), "misc/%s", name);
	pack->origin = strdup(buf);
	pack->lang = strdup("");
	pack->comment = strdup("test package");
	pack->type = MPORT_TYPE_APP;

	extra = mport_createextras_new();
	ATF_REQUIRE(extra != NULL);
	(void)snprintf(extra->pkg_filename, sizeof(extra->pkg_filename), "%s/%s-%s.mport",
	    test_root, name, PKG_VERSION);
	(void)strlcpy(extra->sourcedir, stage, sizeof(extra->sourcedir));

	ATF_REQUIRE_MSG(mport_create_primative(mport, assetlist, pack, extra) == MPORT_OK, "%s",
	    mport_err_string());

	mport_assetlist_free(assetlist);
	mport_pkgmeta_free(pack);
	mport_createextras_free(extra);

	(void)snprintf(buf, sizeof(buf), "/%s-%s.mport", name, PKG_VERSION);
	return test_path(buf);
}

static const char *
create_package(mportInstance *mport, const char *name, const char *const *files)
{
	return create_package_contents(mport, name, files, NULL);
}

static void
install_package(mportInstance *mport, const char *pkgfile)
{
	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
}

static void
delete_package(mportInstance *mport, const char *name)
{
	mportPackageMeta **installed = NULL;

	ATF_REQUIRE_EQ(MPORT_OK, mport_pkgmeta_search_master(mport, &installed, "pkg=%Q", name));
	ATF_REQUIRE(installed != NULL && installed[0] != NULL);
	installed[0]->action = MPORT_ACTION_DELETE;
	ATF_REQUIRE_MSG(
	    mport_delete_primative(mport, installed[0], 1) == MPORT_OK, "%s", mport_err_string());
	mport_pkgmeta_vec_free(installed);
}

/* A script that writes every entry of arg, one per line, to /<marker>. */
#define ARG_WRITER_SCRIPT(marker)                                \
	"local f = io.open(\"/" marker "\", \"w\")\n"            \
	"for _, p in ipairs(arg) do f:write(p .. \"\\n\") end\n" \
	"f:close()\n"

static const char *const trigpkg_files[] = { "share/trigpkg/catalog.mk", NULL };
static const char *const otherpkg_files[] = { "share/otherpkg/data.txt", NULL };

/* --- pure helpers ------------------------------------------------------- */

ATF_TC_WITHOUT_HEAD(normalize_path);
ATF_TC_BODY(normalize_path, tc)
{
	char buf[64];

	(void)tc;

	(void)strlcpy(buf, "//usr/local//share/", sizeof(buf));
	mport_trigger_normalize_path(buf);
	ATF_REQUIRE_STREQ("/usr/local/share", buf);

	(void)strlcpy(buf, "/", sizeof(buf));
	mport_trigger_normalize_path(buf);
	ATF_REQUIRE_STREQ("/", buf);

	(void)strlcpy(buf, "/usr/local/share", sizeof(buf));
	mport_trigger_normalize_path(buf);
	ATF_REQUIRE_STREQ("/usr/local/share", buf);
}

/* --- loading ----------------------------------------------------------- */

ATF_TC_WITH_CLEANUP(load_parses_trigger_file);
ATF_TC_HEAD(load_parses_trigger_file, tc)
{
	atf_tc_set_md_var(tc, "descr", "a valid trigger file yields its paths and scripts");
}
ATF_TC_BODY(load_parses_trigger_file, tc)
{
	mportInstance *mport;
	mportTrigger *t;
	int dfd;

	(void)tc;

	mport = create_test_instance();
	write_trigger(SYS_TRIGGERS, "good.ucl",
	    "path: [ \"/usr/local/share/a\", /usr/local/share/b ]\n"
	    "path_glob: \"*/share/icons/*\"\n"
	    "cleanup: {\n"
	    "	type: lua\n"
	    "	script: <<EOS\n"
	    "print('cleanup')\n"
	    "EOS\n"
	    "}\n"
	    "trigger: {\n"
	    "	type: lua\n"
	    "	sandbox: false\n"
	    "	script: <<EOS\n"
	    "print('trigger')\n"
	    "EOS\n"
	    "}\n");

	dfd = open(test_path(SYS_TRIGGERS), O_DIRECTORY | O_RDONLY);
	ATF_REQUIRE(dfd >= 0);

	t = mport_trigger_load(mport, dfd, "good.ucl", false);
	ATF_REQUIRE_MSG(t != NULL, "%s", mport_err_string());
	ATF_REQUIRE_STREQ("good.ucl", t->name);
	ATF_REQUIRE(t->path != NULL);
	ATF_REQUIRE(t->path_glob != NULL);
	ATF_REQUIRE(t->path_regexp == NULL);
	ATF_REQUIRE(t->script.script != NULL);
	ATF_REQUIRE(strstr(t->script.script, "print('trigger')") != NULL);
	ATF_REQUIRE(!t->script.sandbox);
	/* the trigger load does not read the cleanup block */
	ATF_REQUIRE(t->cleanup.script == NULL);
	mport_trigger_free(t);

	/* the cleanup load reads only the cleanup block, sandboxed by default */
	t = mport_trigger_load(mport, dfd, "good.ucl", true);
	ATF_REQUIRE_MSG(t != NULL, "%s", mport_err_string());
	ATF_REQUIRE(t->cleanup.script != NULL);
	ATF_REQUIRE(t->cleanup.sandbox);
	ATF_REQUIRE(t->script.script == NULL);
	mport_trigger_free(t);

	(void)close(dfd);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(load_parses_trigger_file, tc)
{
	(void)tc;
	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(load_rejects_bad_files);
ATF_TC_HEAD(load_rejects_bad_files, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "files without a trigger block, without paths, with an unknown script type, "
	    "or with a non-.ucl name are not loaded");
}
ATF_TC_BODY(load_rejects_bad_files, tc)
{
	mportInstance *mport;
	mportTriggerList list = tll_init();
	int dfd;

	(void)tc;

	mport = create_test_instance();
	write_trigger(SYS_TRIGGERS, "notrigger.ucl", "path: \"/usr/local/share/a\"\n");
	write_trigger(SYS_TRIGGERS, "nopath.ucl", "trigger: { type: lua, script: \"print(1)\" }\n");
	write_trigger(SYS_TRIGGERS, "badtype.ucl",
	    "path: \"/usr/local/share/a\"\ntrigger: { type: sh, script: \"true\" }\n");
	write_trigger(SYS_TRIGGERS, "garbage.ucl", "{{{{ not ucl\n");
	write_trigger(SYS_TRIGGERS, "ignored.txt",
	    "path: \"/usr/local/share/a\"\ntrigger: { type: lua, script: \"print(1)\" }\n");
	write_trigger(SYS_TRIGGERS, "ok.ucl",
	    "path: \"/usr/local/share/a\"\ntrigger: { type: lua, script: \"print(1)\" }\n");

	dfd = open(test_path(SYS_TRIGGERS), O_DIRECTORY | O_RDONLY);
	ATF_REQUIRE(dfd >= 0);
	ATF_REQUIRE(mport_trigger_load(mport, dfd, "notrigger.ucl", false) == NULL);
	ATF_REQUIRE(mport_trigger_load(mport, dfd, "nopath.ucl", false) == NULL);
	ATF_REQUIRE(mport_trigger_load(mport, dfd, "badtype.ucl", false) == NULL);
	ATF_REQUIRE(mport_trigger_load(mport, dfd, "garbage.ucl", false) == NULL);
	ATF_REQUIRE(mport_trigger_load(mport, dfd, "missing.ucl", false) == NULL);
	/* a cleanup load of a file with no cleanup block has nothing to run */
	ATF_REQUIRE(mport_trigger_load(mport, dfd, "ok.ucl", true) == NULL);
	(void)close(dfd);

	/* the directory scan keeps only the one good .ucl file */
	ATF_REQUIRE_EQ(MPORT_OK, mport_triggers_load(mport, NULL, &list));
	ATF_REQUIRE_EQ(1, (int)tll_length(list));
	ATF_REQUIRE_STREQ("ok.ucl", tll_front(list)->name);
	tll_free_and_free(list, mport_trigger_free);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(load_rejects_bad_files, tc)
{
	(void)tc;
	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(dirs_setting_and_phase_subdirs);
ATF_TC_HEAD(dirs_setting_and_phase_subdirs, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "the triggers_dir setting names the directories searched; a phase loads its subdir");
}
ATF_TC_BODY(dirs_setting_and_phase_subdirs, tc)
{
	mportInstance *mport;
	mportTriggerList list = tll_init();
	stringlist_t dirs = tll_init();
	const char *good =
	    "path: \"/usr/local/share/a\"\ntrigger: { type: lua, script: \"x = 1\" }\n";
	char *val;

	(void)tc;

	mport = create_test_instance();

	/* a fresh registry carries the defaults */
	val = mport_setting_get(mport, MPORT_SETTING_TRIGGERS_ENABLE);
	ATF_REQUIRE(val != NULL);
	ATF_REQUIRE_STREQ("yes", val);
	free(val);
	val = mport_setting_get(mport, MPORT_SETTING_TRIGGERS_DIR);
	ATF_REQUIRE(val != NULL);
	ATF_REQUIRE_STREQ(MPORT_TRIGGERS_DIR_DEFAULT, val);
	free(val);
	ATF_REQUIRE(mport_triggers_enabled(mport));

	mport_triggers_get_dirs(mport, &dirs);
	ATF_REQUIRE_EQ(2, (int)tll_length(dirs));
	ATF_REQUIRE_STREQ(SYS_TRIGGERS, tll_front(dirs));
	ATF_REQUIRE_STREQ(LOCAL_TRIGGERS, tll_back(dirs));
	tll_free_and_free(dirs, free);

	write_trigger(SYS_TRIGGERS, "sys.ucl", good);
	write_trigger(LOCAL_TRIGGERS, "local.ucl", good);
	write_trigger("/opt/triggers", "opt.ucl", good);
	write_trigger(SYS_TRIGGERS "/post_install", "pp.ucl", good);

	/* both default directories, not the phase subdirectory */
	ATF_REQUIRE_EQ(MPORT_OK, mport_triggers_load(mport, NULL, &list));
	ATF_REQUIRE_EQ(2, (int)tll_length(list));
	tll_free_and_free(list, mport_trigger_free);

	/* a phase loads only its subdirectory */
	ATF_REQUIRE_EQ(MPORT_OK, mport_triggers_load(mport, "post_install", &list));
	ATF_REQUIRE_EQ(1, (int)tll_length(list));
	ATF_REQUIRE_STREQ("pp.ucl", tll_front(list)->name);
	tll_free_and_free(list, mport_trigger_free);

	/* the setting replaces the defaults */
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_setting_set(mport, MPORT_SETTING_TRIGGERS_DIR, "/opt/triggers"));
	ATF_REQUIRE_EQ(MPORT_OK, mport_triggers_load(mport, NULL, &list));
	ATF_REQUIRE_EQ(1, (int)tll_length(list));
	ATF_REQUIRE_STREQ("opt.ucl", tll_front(list)->name);
	tll_free_and_free(list, mport_trigger_free);

	ATF_REQUIRE_EQ(MPORT_OK, mport_setting_set(mport, MPORT_SETTING_TRIGGERS_ENABLE, "no"));
	ATF_REQUIRE(!mport_triggers_enabled(mport));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(dirs_setting_and_phase_subdirs, tc)
{
	(void)tc;
	cleanup_test_root();
}

/* --- matching ---------------------------------------------------------- */

ATF_TC_WITH_CLEANUP(match_exact_glob_regexp);
ATF_TC_HEAD(match_exact_glob_regexp, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "path matches exactly, path_glob by fnmatch, path_regexp by regex; matches are unique");
}
ATF_TC_BODY(match_exact_glob_regexp, tc)
{
	mportInstance *mport;
	mportTrigger *t;
	int dfd;

	(void)tc;

	mport = create_test_instance();
	write_trigger(SYS_TRIGGERS, "m.ucl",
	    "path: [ \"/usr/local/share/mime/packages/\" ]\n"
	    "path_glob: \"*/share/icons/*\"\n"
	    "path_regexp: \"^/usr/local/lib/gio/modules$\"\n"
	    "trigger: { type: lua, script: \"x = 1\" }\n");
	dfd = open(test_path(SYS_TRIGGERS), O_DIRECTORY | O_RDONLY);
	ATF_REQUIRE(dfd >= 0);
	t = mport_trigger_load(mport, dfd, "m.ucl", false);
	(void)close(dfd);
	ATF_REQUIRE_MSG(t != NULL, "%s", mport_err_string());

	/* the trailing slash in the file is normalized away */
	ATF_REQUIRE(mport_trigger_match(t, "/usr/local/share/mime/packages"));
	ATF_REQUIRE(!mport_trigger_match(t, "/usr/local/share/mime"));
	ATF_REQUIRE(mport_trigger_match(t, "/usr/local/share/icons/hicolor"));
	ATF_REQUIRE(mport_trigger_match(t, "/usr/local/share/icons/hicolor/48x48/apps"));
	ATF_REQUIRE(!mport_trigger_match(t, "/usr/local/share/icons"));
	ATF_REQUIRE(mport_trigger_match(t, "/usr/local/lib/gio/modules"));
	ATF_REQUIRE(!mport_trigger_match(t, "/usr/local/lib/gio/modules/sub"));

	/* a repeated match is recorded once */
	ATF_REQUIRE_EQ(4, (int)tll_length(t->matched));
	ATF_REQUIRE(mport_trigger_match(t, "/usr/local/lib/gio/modules"));
	ATF_REQUIRE_EQ(4, (int)tll_length(t->matched));

	mport_trigger_free(t);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(match_exact_glob_regexp, tc)
{
	(void)tc;
	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(touched_paths_are_dirs_and_unique);
ATF_TC_HEAD(touched_paths_are_dirs_and_unique, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "touching a file records its directory; repeats and slashes collapse");
}
ATF_TC_BODY(touched_paths_are_dirs_and_unique, tc)
{
	mportInstance *mport;

	(void)tc;

	mport = create_test_instance();
	ATF_REQUIRE(mport->triggers == NULL);

	mport_triggers_touch_file(mport, "/usr/local/share/a/one.txt");
	mport_triggers_touch_file(mport, "/usr/local/share/a//two.txt");
	mport_triggers_touch_dir(mport, "/usr/local/share/a/");
	mport_triggers_touch_dir(mport, "/usr/local/share/b");
	mport_triggers_touch_file(mport, "/rootfile");

	ATF_REQUIRE(mport->triggers != NULL);
	ATF_REQUIRE_EQ(3, (int)tll_length(mport->triggers->touched));
	ATF_REQUIRE_STREQ("/usr/local/share/a", tll_front(mport->triggers->touched));
	ATF_REQUIRE_STREQ("/", tll_back(mport->triggers->touched));

	/* executing with nothing to run clears the state */
	ATF_REQUIRE_EQ(MPORT_OK, mport_triggers_execute(mport));
	ATF_REQUIRE(mport->triggers == NULL);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(touched_paths_are_dirs_and_unique, tc)
{
	(void)tc;
	cleanup_test_root();
}

/* --- per-transaction triggers ------------------------------------------ */

ATF_TC_WITH_CLEANUP(transaction_trigger_runs_once_with_matched_dirs);
ATF_TC_HEAD(transaction_trigger_runs_once_with_matched_dirs, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr",
	    "installing packages records their directories; execute runs each matching "
	    "trigger once with the matches as arg, then forgets them");
}
ATF_TC_BODY(transaction_trigger_runs_once_with_matched_dirs, tc)
{
	mportInstance *mport;
	const char *pkgfile;

	(void)tc;

	mport = create_test_instance();
	write_trigger(SYS_TRIGGERS, "share.ucl",
	    "path_glob: \"/usr/local/share/*\"\n"
	    "trigger: {\n"
	    "	type: lua\n"
	    "	script: <<EOS\n" ARG_WRITER_SCRIPT("trigger-ran") "EOS\n"
								  "}\n");
	write_trigger(SYS_TRIGGERS, "unrelated.ucl",
	    "path: \"/usr/local/etc/nothing\"\n"
	    "trigger: { type: lua, script: \"io.open('/unrelated-ran', 'w'):close()\" }\n");

	pkgfile = create_package(mport, "trigpkg", trigpkg_files);
	install_package(mport, pkgfile);
	pkgfile = create_package(mport, "otherpkg", otherpkg_files);
	install_package(mport, pkgfile);

	/* nothing runs until the transaction ends */
	ATF_REQUIRE(!marker_exists("/trigger-ran"));
	ATF_REQUIRE_MSG(mport_triggers_execute(mport) == MPORT_OK, "%s", mport_err_string());

	ATF_REQUIRE(marker_exists("/trigger-ran"));
	ATF_REQUIRE_STREQ(
	    "/usr/local/share/trigpkg\n/usr/local/share/otherpkg\n", read_marker("/trigger-ran"));
	ATF_REQUIRE(!marker_exists("/unrelated-ran"));

	/* the matches were consumed: a second execute runs nothing */
	ATF_REQUIRE_EQ(0, unlink(test_path("/trigger-ran")));
	ATF_REQUIRE_EQ(MPORT_OK, mport_triggers_execute(mport));
	ATF_REQUIRE(!marker_exists("/trigger-ran"));

	/* removing a package touches its directories too */
	delete_package(mport, "otherpkg");
	ATF_REQUIRE_MSG(mport_triggers_execute(mport) == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE_STREQ("/usr/local/share/otherpkg\n", read_marker("/trigger-ran"));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(transaction_trigger_runs_once_with_matched_dirs, tc)
{
	(void)tc;
	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(disabled_setting_skips_triggers);
ATF_TC_HEAD(disabled_setting_skips_triggers, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr", "triggers_enable=no runs no trigger of either kind");
}
ATF_TC_BODY(disabled_setting_skips_triggers, tc)
{
	mportInstance *mport;
	const char *pkgfile;

	(void)tc;

	mport = create_test_instance();
	ATF_REQUIRE_EQ(MPORT_OK, mport_setting_set(mport, MPORT_SETTING_TRIGGERS_ENABLE, "no"));
	write_trigger(SYS_TRIGGERS, "share.ucl",
	    "path_glob: \"/usr/local/share/*\"\n"
	    "trigger: { type: lua, script: \"io.open('/trigger-ran', 'w'):close()\" }\n");
	write_trigger(SYS_TRIGGERS "/post_install", "pp.ucl",
	    "path_glob: \"/usr/local/share/*\"\n"
	    "trigger: { type: lua, script: \"io.open('/pp-ran', 'w'):close()\" }\n");

	pkgfile = create_package(mport, "trigpkg", trigpkg_files);
	install_package(mport, pkgfile);
	ATF_REQUIRE_EQ(MPORT_OK, mport_triggers_execute(mport));

	ATF_REQUIRE(!marker_exists("/trigger-ran"));
	ATF_REQUIRE(!marker_exists("/pp-ran"));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(disabled_setting_skips_triggers, tc)
{
	(void)tc;
	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(failing_transaction_trigger_reports_error);
ATF_TC_HEAD(failing_transaction_trigger_reports_error, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr",
	    "a trigger whose script errors makes execute fail, but the other triggers still run");
}
ATF_TC_BODY(failing_transaction_trigger_reports_error, tc)
{
	mportInstance *mport;
	const char *pkgfile;

	(void)tc;

	mport = create_test_instance();
	write_trigger(SYS_TRIGGERS, "aaa-bad.ucl",
	    "path_glob: \"/usr/local/share/*\"\n"
	    "trigger: { type: lua, script: \"error('boom')\" }\n");
	write_trigger(SYS_TRIGGERS, "zzz-good.ucl",
	    "path_glob: \"/usr/local/share/*\"\n"
	    "trigger: { type: lua, script: \"io.open('/good-ran', 'w'):close()\" }\n");

	pkgfile = create_package(mport, "trigpkg", trigpkg_files);
	install_package(mport, pkgfile);

	ATF_REQUIRE(mport_triggers_execute(mport) != MPORT_OK);
	ATF_REQUIRE(marker_exists("/good-ran"));
	ATF_REQUIRE(mport->triggers == NULL);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(failing_transaction_trigger_reports_error, tc)
{
	(void)tc;
	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(cleanup_runs_when_trigger_file_is_removed);
ATF_TC_HEAD(cleanup_runs_when_trigger_file_is_removed, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr",
	    "a package shipping its own trigger file fires it on install; deleting the "
	    "package runs the cleanup block and not the trigger");
}
ATF_TC_BODY(cleanup_runs_when_trigger_file_is_removed, tc)
{
	mportInstance *mport;
	const char *pkgfile;
	static const char *const owner_files[] = { "share/trigowner/catalog.mk",
		"share/pkg/triggers/owner.ucl", NULL };
	static const char *const owner_contents[] = { NULL,
		"path: \"/usr/local/share/trigowner\"\n"
		"cleanup: {\n"
		"	type: lua\n"
		"	script: <<EOS\n"
		"local f = io.open('/cleanup-ran', 'w')\n"
		"f:write(tostring(pkg.stat('/usr/local/share/pkg/triggers/owner.ucl') ~= nil))\n"
		"f:close()\n"
		"EOS\n"
		"}\n"
		"trigger: {\n"
		"	type: lua\n"
		"	script: <<EOS\n" ARG_WRITER_SCRIPT("trigger-ran") "EOS\n"
									  "}\n",
		NULL };

	(void)tc;

	mport = create_test_instance();
	pkgfile = create_package_contents(mport, "trigowner", owner_files, owner_contents);

	install_package(mport, pkgfile);
	ATF_REQUIRE(access(test_path(LOCAL_TRIGGERS "/owner.ucl"), F_OK) == 0);
	/* the trigger file the package just installed is live for its own install */
	ATF_REQUIRE_MSG(mport_triggers_execute(mport) == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE_STREQ("/usr/local/share/trigowner\n", read_marker("/trigger-ran"));
	ATF_REQUIRE(!marker_exists("/cleanup-ran"));
	ATF_REQUIRE_EQ(0, unlink(test_path("/trigger-ran")));

	delete_package(mport, "trigowner");
	ATF_REQUIRE(access(test_path(LOCAL_TRIGGERS "/owner.ucl"), F_OK) != 0);
	/* the cleanup was queued at removal time, while the file could be read */
	ATF_REQUIRE(mport->triggers != NULL);
	ATF_REQUIRE_EQ(1, (int)tll_length(mport->triggers->cleanups));

	ATF_REQUIRE_MSG(mport_triggers_execute(mport) == MPORT_OK, "%s", mport_err_string());
	/* the cleanup ran, after the trigger file was gone */
	ATF_REQUIRE_STREQ("false", read_marker("/cleanup-ran"));
	/* with the file gone, its trigger cannot fire for the removal */
	ATF_REQUIRE(!marker_exists("/trigger-ran"));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(cleanup_runs_when_trigger_file_is_removed, tc)
{
	(void)tc;
	cleanup_test_root();
}

/* --- per-package triggers ---------------------------------------------- */

ATF_TC_WITH_CLEANUP(perpackage_install_triggers_see_package);
ATF_TC_HEAD(perpackage_install_triggers_see_package, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr",
	    "pre_install and post_install triggers match the package's paths and run "
	    "at their phase with pkg_name/pkg_version/pkg_upgrade; a failing one "
	    "does not fail the install");
}
ATF_TC_BODY(perpackage_install_triggers_see_package, tc)
{
	mportInstance *mport;
	const char *pkgfile;

	(void)tc;

	mport = create_test_instance();
	write_trigger(SYS_TRIGGERS "/pre_install", "pre.ucl",
	    "path: \"/usr/local/share/trigpkg\"\n"
	    "trigger: {\n"
	    "	type: lua\n"
	    "	script: <<EOS\n"
	    "local f = io.open('/pre-ran', 'w')\n"
	    "f:write(pkg_name .. ' ' .. pkg_version .. ' ' .. tostring(pkg_upgrade) .. ' ' ..\n"
	    "    tostring(pkg.stat('/usr/local/share/trigpkg/catalog.mk') ~= nil) .. ' ' ..\n"
	    "    table.concat(arg, ','))\n"
	    "f:close()\n"
	    "EOS\n"
	    "}\n");
	write_trigger(SYS_TRIGGERS "/post_install", "post.ucl",
	    "path_glob: \"/usr/local/share/trigpkg/*\"\n"
	    "trigger: {\n"
	    "	type: lua\n"
	    "	script: <<EOS\n"
	    "local f = io.open('/post-ran', 'w')\n"
	    "f:write(tostring(pkg.stat('/usr/local/share/trigpkg/catalog.mk') ~= nil) .. ' ' ..\n"
	    "    table.concat(arg, ','))\n"
	    "f:close()\n"
	    "EOS\n"
	    "}\n");
	write_trigger(SYS_TRIGGERS "/post_install", "broken.ucl",
	    "path_glob: \"/usr/local/share/*\"\n"
	    "trigger: { type: lua, script: \"error('boom')\" }\n");
	write_trigger(SYS_TRIGGERS "/post_install", "unrelated.ucl",
	    "path: \"/usr/local/etc/nothing\"\n"
	    "trigger: { type: lua, script: \"io.open('/unrelated-ran', 'w'):close()\" }\n");

	pkgfile = create_package(mport, "trigpkg", trigpkg_files);
	install_package(mport, pkgfile);

	/* pre_install ran before the files existed, post_install after */
	ATF_REQUIRE_STREQ(
	    "trigpkg 1.0 false false /usr/local/share/trigpkg", read_marker("/pre-ran"));
	ATF_REQUIRE_STREQ("true /usr/local/share/trigpkg/catalog.mk", read_marker("/post-ran"));
	ATF_REQUIRE(!marker_exists("/unrelated-ran"));

	/* the package is registered despite the broken post_install trigger */
	{
		mportPackageMeta **found = NULL;
		ATF_REQUIRE_EQ(
		    MPORT_OK, mport_pkgmeta_search_master(mport, &found, "pkg=%Q", "trigpkg"));
		ATF_REQUIRE(found != NULL && found[0] != NULL);
		mport_pkgmeta_vec_free(found);
	}

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(perpackage_install_triggers_see_package, tc)
{
	(void)tc;
	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(perpackage_deinstall_triggers_see_package);
ATF_TC_HEAD(perpackage_deinstall_triggers_see_package, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr",
	    "pre_deinstall runs while the files exist and post_deinstall after they are gone");
}
ATF_TC_BODY(perpackage_deinstall_triggers_see_package, tc)
{
	mportInstance *mport;
	const char *pkgfile;

	(void)tc;

	mport = create_test_instance();
	write_trigger(SYS_TRIGGERS "/pre_deinstall", "pre.ucl",
	    "path: \"/usr/local/share/trigpkg\"\n"
	    "trigger: {\n"
	    "	type: lua\n"
	    "	script: <<EOS\n"
	    "local f = io.open('/pre-ran', 'w')\n"
	    "f:write(pkg_name .. ' ' .. tostring(pkg_upgrade) .. ' ' ..\n"
	    "    tostring(pkg.stat('/usr/local/share/trigpkg/catalog.mk') ~= nil))\n"
	    "f:close()\n"
	    "EOS\n"
	    "}\n");
	write_trigger(SYS_TRIGGERS "/post_deinstall", "post.ucl",
	    "path: \"/usr/local/share/trigpkg\"\n"
	    "trigger: {\n"
	    "	type: lua\n"
	    "	script: <<EOS\n"
	    "local f = io.open('/post-ran', 'w')\n"
	    "f:write(tostring(pkg.stat('/usr/local/share/trigpkg/catalog.mk') ~= nil))\n"
	    "f:close()\n"
	    "EOS\n"
	    "}\n");

	pkgfile = create_package(mport, "trigpkg", trigpkg_files);
	install_package(mport, pkgfile);
	ATF_REQUIRE(!marker_exists("/pre-ran"));

	delete_package(mport, "trigpkg");
	ATF_REQUIRE_STREQ("trigpkg false true", read_marker("/pre-ran"));
	ATF_REQUIRE_STREQ("false", read_marker("/post-ran"));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(perpackage_deinstall_triggers_see_package, tc)
{
	(void)tc;
	cleanup_test_root();
}

/* --- sandbox ----------------------------------------------------------- */

/* true when this kernel can enter capability mode */
static bool
capsicum_available(void)
{
#ifdef TEST_HAVE_CAPSICUM
	pid_t pid;
	int status;

	pid = fork();
	if (pid == 0) {
		if (cap_enter() < 0)
			_exit(errno == ENOSYS ? 2 : 1);
		_exit(0);
	}
	if (pid < 0 || waitpid(pid, &status, 0) != pid)
		return false;
	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#else
	return false;
#endif
}

ATF_TC_WITH_CLEANUP(sandboxed_trigger_cannot_exec);
ATF_TC_HEAD(sandboxed_trigger_cannot_exec, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(
	    tc, "descr", "pkg.exec fails inside the default sandbox and works with sandbox: false");
}
ATF_TC_BODY(sandboxed_trigger_cannot_exec, tc)
{
	mportInstance *mport;
	const char *pkgfile;

	(void)tc;

	if (!capsicum_available())
		atf_tc_skip("capability mode is not available on this kernel");

	mport = create_test_instance();
	write_trigger(SYS_TRIGGERS, "sandboxed.ucl",
	    "path_glob: \"/usr/local/share/*\"\n"
	    "trigger: {\n"
	    "	type: lua\n"
	    "	script: <<EOS\n"
	    "pkg.exec({\"/usr/bin/true\"})\n"
	    "io.open('/sandboxed-ran', 'w'):close()\n"
	    "EOS\n"
	    "}\n");

	pkgfile = create_package(mport, "trigpkg", trigpkg_files);
	install_package(mport, pkgfile);
	/* pkg.exec raises inside the sandbox, so the script never gets to its marker */
	ATF_REQUIRE(mport_triggers_execute(mport) != MPORT_OK);
	ATF_REQUIRE(!marker_exists("/sandboxed-ran"));

	write_trigger(SYS_TRIGGERS, "sandboxed.ucl",
	    "path_glob: \"/usr/local/share/*\"\n"
	    "trigger: {\n"
	    "	type: lua\n"
	    "	sandbox: false\n"
	    "	script: <<EOS\n"
	    "pkg.exec({\"/usr/bin/true\"})\n"
	    "io.open('/sandboxed-ran', 'w'):close()\n"
	    "EOS\n"
	    "}\n");
	mport_triggers_touch_dir(mport, "/usr/local/share/trigpkg");
	ATF_REQUIRE_MSG(mport_triggers_execute(mport) == MPORT_OK, "%s", mport_err_string());
	ATF_REQUIRE(marker_exists("/sandboxed-ran"));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(sandboxed_trigger_cannot_exec, tc)
{
	(void)tc;
	cleanup_test_root();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, normalize_path);
	ATF_TP_ADD_TC(tp, load_parses_trigger_file);
	ATF_TP_ADD_TC(tp, load_rejects_bad_files);
	ATF_TP_ADD_TC(tp, dirs_setting_and_phase_subdirs);
	ATF_TP_ADD_TC(tp, match_exact_glob_regexp);
	ATF_TP_ADD_TC(tp, touched_paths_are_dirs_and_unique);
	ATF_TP_ADD_TC(tp, transaction_trigger_runs_once_with_matched_dirs);
	ATF_TP_ADD_TC(tp, disabled_setting_skips_triggers);
	ATF_TP_ADD_TC(tp, failing_transaction_trigger_reports_error);
	ATF_TP_ADD_TC(tp, cleanup_runs_when_trigger_file_is_removed);
	ATF_TP_ADD_TC(tp, perpackage_install_triggers_see_package);
	ATF_TP_ADD_TC(tp, perpackage_deinstall_triggers_see_package);
	ATF_TP_ADD_TC(tp, sandboxed_trigger_cannot_exec);

	return atf_no_error();
}
