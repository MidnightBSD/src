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

#define TEST_ROOT_TEMPLATE "/tmp/mport-message-test-root.XXXXXX"
#define MSG_PKG_NAME "msgpkg"
#define MSG_PKG_VERSION "2.0"

static char test_root[PATH_MAX];
static char shown[8192];

/*
 * The www/gitea pkg-message as installed (after SUB_FILES replaced
 * %%PREFIX%%): one unbounded upgrade entry and four bounded by
 * maximum_version.  The 1.7.6 entry uses the UCL ${PREFIX} variable.
 */
static const char gitea_message[] =
    "[\n"
    "{ type: upgrade\n"
    "  message: <<EOM\n"
    "Gitea 28.x is a large jump from the 1.18 series previously in mports.\n"
    "Before starting it for the first time:\n"
    "\n"
    "- Stop gitea and back up the database, /usr/local/etc/gitea and\n"
    "  /var/db/gitea.\n"
    "EOM\n"
    "}\n"
    "{ type: upgrade\n"
    "  maximum_version: 1.20.0\n"
    "  message: <<EOM\n"
    "Please make sure to empty or maintain the contents of the\n"
    "/usr/local/share/gitea folder between your upgrades of gitea.\n"
    "EOM\n"
    "}\n"
    "{ type: upgrade\n"
    "  maximum_version: 1.7.6\n"
    "  message: <<EOM\n"
    "When upgrading from Gitea 1.7 or earlier, Gitea requires the addition of\n"
    "another secret to the config file in order to start up.  Either manually add\n"
    "JWT_SECRET to the ${PREFIX}/etc/gitea/conf/app.ini config file.\n"
    "EOM\n"
    "}\n"
    "{ type: upgrade\n"
    "  maximum_version: 1.15.0\n"
    "  message: <<EOM\n"
    "Some config file options have been changed in Gitea 1.15.\n"
    "EOM\n"
    "}\n"
    "{ type: upgrade\n"
    "  maximum_version: 1.15.1\n"
    "  message: <<EOM\n"
    "We remind users that a bug was discovered with gitea dump in 1.14.3-1.14.6\n"
    "and 1.15.0.\n"
    "EOM\n"
    "}\n"
    "]\n";

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
capture_msg(const char *msg)
{
	size_t len = strlen(shown);

	(void)snprintf(shown + len, sizeof(shown) - len, "%s|", msg);
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
	mport->msg_cb = capture_msg;

	return mport;
}

/* Install contents as the pkg-message of msgpkg-2.0 and return its metadata. */
static mportPackageMeta *
message_package(const char *contents)
{
	mportPackageMeta *pkg;
	int fd;
	size_t len;

	/* mport_instance_init() made the infrastructure directory */
	ATF_REQUIRE_EQ(
	    0, mkdir(test_path(MPORT_INST_INFRA_DIR "/" MSG_PKG_NAME "-" MSG_PKG_VERSION), 0755));
	fd = open(test_path(MPORT_INST_INFRA_DIR "/" MSG_PKG_NAME "-" MSG_PKG_VERSION
						 "/" MPORT_MESSAGE_FILE),
	    O_WRONLY | O_CREAT | O_TRUNC, 0644);
	ATF_REQUIRE(fd >= 0);
	len = strlen(contents);
	ATF_REQUIRE_EQ((ssize_t)len, write(fd, contents, len));
	ATF_REQUIRE_EQ(0, close(fd));

	pkg = mport_pkgmeta_new();
	ATF_REQUIRE(pkg != NULL);
	pkg->name = strdup(MSG_PKG_NAME);
	pkg->version = strdup(MSG_PKG_VERSION);
	pkg->prefix = strdup("/usr/local");
	pkg->origin = strdup("misc/" MSG_PKG_NAME);
	ATF_REQUIRE(pkg->name != NULL && pkg->version != NULL && pkg->prefix != NULL &&
	    pkg->origin != NULL);

	return pkg;
}

/* Messages shown for one action, each followed by '|'. */
static const char *
display(mportInstance *mport, mportPackageMeta *pkg, pkg_message_t when, const char *prev)
{
	shown[0] = '\0';
	ATF_REQUIRE_EQ_MSG(
	    MPORT_OK, mport_pkg_message_display(mport, pkg, when, prev), "%s", mport_err_string());

	return shown;
}

ATF_TC_WITH_CLEANUP(plain_text_shown_for_every_action);
ATF_TC_HEAD(plain_text_shown_for_every_action, tc)
{
	atf_tc_set_md_var(tc, "descr", "a plain-text pkg-message is shown whole for every action");
}
ATF_TC_BODY(plain_text_shown_for_every_action, tc)
{
	mportInstance *mport;
	mportPackageMeta *pkg;
	char *query;

	(void)tc;

	mport = create_test_instance();
	pkg = message_package("Add msgpkg_enable=\"YES\" to /etc/rc.conf.\n[not UCL]\n");

	ATF_REQUIRE_STREQ("Add msgpkg_enable=\"YES\" to /etc/rc.conf.\n[not UCL]\n|",
	    display(mport, pkg, PKG_MESSAGE_INSTALL, NULL));
	ATF_REQUIRE_STREQ("Add msgpkg_enable=\"YES\" to /etc/rc.conf.\n[not UCL]\n|",
	    display(mport, pkg, PKG_MESSAGE_UPGRADE, "1.0"));
	ATF_REQUIRE_STREQ("Add msgpkg_enable=\"YES\" to /etc/rc.conf.\n[not UCL]\n|",
	    display(mport, pkg, PKG_MESSAGE_REMOVE, NULL));

	query = mport_query_pkg_message(mport, pkg);
	ATF_REQUIRE(query != NULL);
	ATF_REQUIRE_STREQ("Add msgpkg_enable=\"YES\" to /etc/rc.conf.\n[not UCL]\n", query);
	free(query);

	mport_pkgmeta_free(pkg);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(plain_text_shown_for_every_action, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(no_message_file);
ATF_TC_HEAD(no_message_file, tc)
{
	atf_tc_set_md_var(tc, "descr", "a package without a pkg-message shows nothing");
}
ATF_TC_BODY(no_message_file, tc)
{
	mportInstance *mport;
	mportPackageMeta *pkg;
	mportPackageMessage sentinel;
	mportPackageMessage *messages = &sentinel; /* load must reset it */
	char *query;

	(void)tc;

	mport = create_test_instance();
	pkg = message_package("");
	ATF_REQUIRE_EQ(0,
	    unlink(test_path(
		MPORT_INST_INFRA_DIR "/" MSG_PKG_NAME "-" MSG_PKG_VERSION "/" MPORT_MESSAGE_FILE)));

	ATF_REQUIRE_EQ(MPORT_OK, mport_pkg_message_load(mport, pkg, &messages));
	ATF_REQUIRE(messages == NULL);
	ATF_REQUIRE_STREQ("", display(mport, pkg, PKG_MESSAGE_INSTALL, NULL));

	query = mport_query_pkg_message(mport, pkg);
	ATF_REQUIRE(query != NULL);
	ATF_REQUIRE_STREQ("", query);
	free(query);

	mport_pkgmeta_free(pkg);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(no_message_file, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(single_object);
ATF_TC_HEAD(single_object, tc)
{
	atf_tc_set_md_var(tc, "descr", "a UCL array with one entry is shown for its action");
}
ATF_TC_BODY(single_object, tc)
{
	mportInstance *mport;
	mportPackageMeta *pkg;

	(void)tc;

	/* no type: always shown */
	mport = create_test_instance();
	pkg = message_package("[\n{ message: \"Read the manual.\" }\n]\n");
	ATF_REQUIRE_STREQ("Read the manual.|", display(mport, pkg, PKG_MESSAGE_INSTALL, NULL));
	ATF_REQUIRE_STREQ("Read the manual.|", display(mport, pkg, PKG_MESSAGE_UPGRADE, "1.0"));
	ATF_REQUIRE_STREQ("Read the manual.|", display(mport, pkg, PKG_MESSAGE_REMOVE, NULL));
	mport_pkgmeta_free(pkg);
	mport_instance_free(mport);

	/* typed: only for that action */
	mport = create_test_instance();
	pkg = message_package(
	    "[\n{\n  type: install\n  message: <<EOM\nFirst install only.\nEOM\n}\n]\n");
	ATF_REQUIRE_STREQ("First install only.|", display(mport, pkg, PKG_MESSAGE_INSTALL, NULL));
	ATF_REQUIRE_STREQ("", display(mport, pkg, PKG_MESSAGE_UPGRADE, "1.0"));
	ATF_REQUIRE_STREQ("", display(mport, pkg, PKG_MESSAGE_REMOVE, NULL));
	mport_pkgmeta_free(pkg);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(single_object, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(array_install_and_upgrade);
ATF_TC_HEAD(array_install_and_upgrade, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "every entry of a UCL array is loaded and shown for its own action");
}
ATF_TC_BODY(array_install_and_upgrade, tc)
{
	mportInstance *mport;
	mportPackageMeta *pkg;
	mportPackageMessage *messages = NULL;
	mportPackageMessage *msg;
	char *query;
	int count;

	(void)tc;

	mport = create_test_instance();
	pkg = message_package("[\n"
			      "{ type: install, message: \"install-a\" }\n"
			      "{ type: upgrade, message: \"upgrade-a\" }\n"
			      "{ message: \"always\" }\n"
			      "{ type: remove, message: \"remove-a\" }\n"
			      "{ type: install, message: \"install-b\" }\n"
			      "{ type: bogus, message: \"unknown type\" }\n"
			      "]\n");

	/* every entry, in file order */
	ATF_REQUIRE_EQ(MPORT_OK, mport_pkg_message_load(mport, pkg, &messages));
	count = 0;
	for (msg = messages; msg != NULL; msg = msg->next) {
		if (msg->prev != NULL)
			ATF_REQUIRE(msg->prev->next == msg);
		count++;
	}
	ATF_REQUIRE_EQ(6, count);
	ATF_REQUIRE_EQ(PKG_MESSAGE_INSTALL, messages->type);
	ATF_REQUIRE_EQ(PKG_MESSAGE_ALWAYS, messages->next->next->next->next->next->type);
	mport_pkg_message_free(messages);

	ATF_REQUIRE_STREQ("install-a|always|install-b|unknown type|",
	    display(mport, pkg, PKG_MESSAGE_INSTALL, NULL));
	ATF_REQUIRE_STREQ(
	    "upgrade-a|always|unknown type|", display(mport, pkg, PKG_MESSAGE_UPGRADE, "1.0"));
	ATF_REQUIRE_STREQ(
	    "always|remove-a|unknown type|", display(mport, pkg, PKG_MESSAGE_REMOVE, NULL));

	query = mport_query_pkg_message(mport, pkg);
	ATF_REQUIRE(query != NULL);
	ATF_REQUIRE_STREQ("install-a\nupgrade-a\nalways\nremove-a\ninstall-b\nunknown type", query);
	free(query);

	mport_pkgmeta_free(pkg);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(array_install_and_upgrade, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(upgrade_version_range);
ATF_TC_HEAD(upgrade_version_range, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "upgrade entries are filtered on the replaced version, exclusive at both ends");
}
ATF_TC_BODY(upgrade_version_range, tc)
{
	mportInstance *mport;
	mportPackageMeta *pkg;

	(void)tc;

	/* the porter's handbook example */
	mport = create_test_instance();
	pkg =
	    message_package("[\n"
			    "{ type: upgrade, message: \"any\" }\n"
			    "{ type: upgrade, maximum_version: \"1.0\", message: \"lt1\" }\n"
			    "{ type: upgrade, minimum_version: \"1.0\", message: \"gt1\" }\n"
			    "{ type: upgrade, maximum_version: \"3.0\", minimum_version: \"1.0\",\n"
			    "  message: \"gt1lt3\" }\n"
			    "{ type: install, maximum_version: \"1.0\", message: \"install\" }\n"
			    "]\n");

	ATF_REQUIRE_STREQ("any|lt1|", display(mport, pkg, PKG_MESSAGE_UPGRADE, "0.9"));
	ATF_REQUIRE_STREQ("any|", display(mport, pkg, PKG_MESSAGE_UPGRADE, "1.0"));
	ATF_REQUIRE_STREQ("any|gt1|gt1lt3|", display(mport, pkg, PKG_MESSAGE_UPGRADE, "1.5"));
	ATF_REQUIRE_STREQ("any|gt1|", display(mport, pkg, PKG_MESSAGE_UPGRADE, "3.0"));
	ATF_REQUIRE_STREQ("any|gt1|", display(mport, pkg, PKG_MESSAGE_UPGRADE, "3.0_1"));
	/* bounded entries need the replaced version */
	ATF_REQUIRE_STREQ("any|", display(mport, pkg, PKG_MESSAGE_UPGRADE, NULL));
	/* a range on a non-upgrade entry is ignored, as in pkg(8) */
	ATF_REQUIRE_STREQ("install|", display(mport, pkg, PKG_MESSAGE_INSTALL, NULL));

	mport_pkgmeta_free(pkg);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(upgrade_version_range, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(gitea_message);
ATF_TC_HEAD(gitea_message, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "the www/gitea pkg-message shows the entries for the "
	    "version being upgraded from");
}
ATF_TC_BODY(gitea_message, tc)
{
	mportInstance *mport;
	mportPackageMeta *pkg;
	const char *out;

	(void)tc;

	mport = create_test_instance();
	pkg = message_package(gitea_message);

	/* a new install has no install entries to show */
	ATF_REQUIRE_STREQ("", display(mport, pkg, PKG_MESSAGE_INSTALL, NULL));

	/* from 1.18: the general note and the < 1.20.0 one */
	out = display(mport, pkg, PKG_MESSAGE_UPGRADE, "1.18.1");
	ATF_REQUIRE(strstr(out, "Gitea 28.x is a large jump") == out);
	ATF_REQUIRE(strstr(out, "/usr/local/share/gitea folder") != NULL);
	ATF_REQUIRE(strstr(out, "JWT_SECRET") == NULL);
	ATF_REQUIRE(strstr(out, "Gitea 1.15.") == NULL);
	ATF_REQUIRE(strstr(out, "gitea dump") == NULL);

	/* from 1.15.0: also the < 1.15.1 entry, not the < 1.15.0 one */
	out = display(mport, pkg, PKG_MESSAGE_UPGRADE, "1.15.0");
	ATF_REQUIRE(strstr(out, "gitea dump") != NULL);
	ATF_REQUIRE(strstr(out, "Gitea 1.15.") == NULL);

	/* from 1.7: everything, with ${PREFIX} expanded */
	out = display(mport, pkg, PKG_MESSAGE_UPGRADE, "1.7.5");
	ATF_REQUIRE(strstr(out, "/usr/local/etc/gitea/conf/app.ini") != NULL);
	ATF_REQUIRE(strstr(out, "${PREFIX}") == NULL);
	ATF_REQUIRE(strstr(out, "Gitea 1.15.") != NULL);
	ATF_REQUIRE(strstr(out, "gitea dump") != NULL);

	/* from 28.1.0: the general note only */
	out = display(mport, pkg, PKG_MESSAGE_UPGRADE, "28.1.0");
	ATF_REQUIRE(strstr(out, "Gitea 28.x is a large jump") == out);
	ATF_REQUIRE(strchr(out, '|') == out + strlen(out) - 1);

	mport_pkgmeta_free(pkg);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(gitea_message, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(malformed_ucl_shown_as_text);
ATF_TC_HEAD(malformed_ucl_shown_as_text, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "a pkg-message that is not a valid UCL message is shown as it is");
}
ATF_TC_BODY(malformed_ucl_shown_as_text, tc)
{
	mportInstance *mport;
	mportPackageMeta *pkg;

	(void)tc;

	/* libucl accepts this as the array [ "NOTE" ] */
	mport = create_test_instance();
	pkg = message_package("[NOTE] this is { not UCL\n");
	ATF_REQUIRE_STREQ(
	    "[NOTE] this is { not UCL\n|", display(mport, pkg, PKG_MESSAGE_INSTALL, NULL));
	mport_pkgmeta_free(pkg);
	mport_instance_free(mport);

	/* not UCL at all */
	mport = create_test_instance();
	pkg = message_package("[\n{ message: \"unterminated\n]\n");
	ATF_REQUIRE_STREQ(
	    "[\n{ message: \"unterminated\n]\n|", display(mport, pkg, PKG_MESSAGE_INSTALL, NULL));
	mport_pkgmeta_free(pkg);
	mport_instance_free(mport);

	/* an entry without a message */
	mport = create_test_instance();
	pkg = message_package("[\n{ type: install, message: \"ok\" }\n{ type: upgrade }\n]\n");
	ATF_REQUIRE_STREQ("[\n{ type: install, message: \"ok\" }\n{ type: upgrade }\n]\n|",
	    display(mport, pkg, PKG_MESSAGE_INSTALL, NULL));
	mport_pkgmeta_free(pkg);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(malformed_ucl_shown_as_text, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, plain_text_shown_for_every_action);
	ATF_TP_ADD_TC(tp, no_message_file);
	ATF_TP_ADD_TC(tp, single_object);
	ATF_TP_ADD_TC(tp, array_install_and_upgrade);
	ATF_TP_ADD_TC(tp, upgrade_version_range);
	ATF_TP_ADD_TC(tp, gitea_message);
	ATF_TP_ADD_TC(tp, malformed_ucl_shown_as_text);

	return atf_no_error();
}
