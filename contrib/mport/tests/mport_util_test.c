#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libmport/mport.h"

/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullderef -nullpass -nullret -nullstate@*/
/*@-retvalint -retvalother -type -unrecog@*/

#define TEST_DIR "test_mport_mkdir_dir"

bool mport_check_answer_bool(char *);
int mport_count_spaces(const char *);
int mport_mkdir(const char *);
int mport_rmdir(const char *, int);
char *mport_directory(const char *);

static void
create_file(const char *filename, const void *data, size_t len)
{
	FILE *f;
	int close_result;

	f = fopen(filename, "wb");
	ATF_REQUIRE(f != NULL);
	if (f == NULL)
		return;
	if (len > 0)
		ATF_REQUIRE_EQ(len, fwrite(data, 1, len, f));
	close_result = fclose(f);
	ATF_REQUIRE_EQ(0, close_result);
}

ATF_TC(mport_check_answer_bool_null);
ATF_TC_HEAD(mport_check_answer_bool_null, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_check_answer_bool returns false for NULL");
}
ATF_TC_BODY(mport_check_answer_bool_null, tc)
{
	(void)tc;

	ATF_REQUIRE_EQ(false, mport_check_answer_bool(NULL));
}

ATF_TC(mport_check_answer_bool_true);
ATF_TC_HEAD(mport_check_answer_bool_true, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_check_answer_bool accepts true values");
}
ATF_TC_BODY(mport_check_answer_bool_true, tc)
{
	(void)tc;

	ATF_REQUIRE_EQ(true, mport_check_answer_bool("Y"));
	ATF_REQUIRE_EQ(true, mport_check_answer_bool("y"));
	ATF_REQUIRE_EQ(true, mport_check_answer_bool("T"));
	ATF_REQUIRE_EQ(true, mport_check_answer_bool("t"));
	ATF_REQUIRE_EQ(true, mport_check_answer_bool("1"));
	ATF_REQUIRE_EQ(true, mport_check_answer_bool("Yes"));
}

ATF_TC(mport_check_answer_bool_false);
ATF_TC_HEAD(mport_check_answer_bool_false, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_check_answer_bool rejects false values");
}
ATF_TC_BODY(mport_check_answer_bool_false, tc)
{
	(void)tc;

	ATF_REQUIRE_EQ(false, mport_check_answer_bool("N"));
	ATF_REQUIRE_EQ(false, mport_check_answer_bool("n"));
	ATF_REQUIRE_EQ(false, mport_check_answer_bool("F"));
	ATF_REQUIRE_EQ(false, mport_check_answer_bool("f"));
	ATF_REQUIRE_EQ(false, mport_check_answer_bool("0"));
	ATF_REQUIRE_EQ(false, mport_check_answer_bool("No"));
}

ATF_TC(mport_check_answer_bool_invalid);
ATF_TC_HEAD(mport_check_answer_bool_invalid, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_check_answer_bool rejects invalid values");
}
ATF_TC_BODY(mport_check_answer_bool_invalid, tc)
{
	(void)tc;

	ATF_REQUIRE_EQ(false, mport_check_answer_bool(""));
	ATF_REQUIRE_EQ(false, mport_check_answer_bool(" "));
	ATF_REQUIRE_EQ(false, mport_check_answer_bool(" Y"));
	ATF_REQUIRE_EQ(false, mport_check_answer_bool("X"));
	ATF_REQUIRE_EQ(false, mport_check_answer_bool("hello"));
}

ATF_TC_WITHOUT_HEAD(count_spaces_empty);
ATF_TC_BODY(count_spaces_empty, tc)
{
	(void)tc;

	ATF_REQUIRE_EQ(0, mport_count_spaces(""));
}

ATF_TC_WITHOUT_HEAD(count_spaces_none);
ATF_TC_BODY(count_spaces_none, tc)
{
	(void)tc;

	ATF_REQUIRE_EQ(0, mport_count_spaces("hello"));
	ATF_REQUIRE_EQ(0, mport_count_spaces("hello_world-123"));
}

ATF_TC_WITHOUT_HEAD(count_spaces_only);
ATF_TC_BODY(count_spaces_only, tc)
{
	(void)tc;

	ATF_REQUIRE_EQ(1, mport_count_spaces(" "));
	ATF_REQUIRE_EQ(3, mport_count_spaces("   "));
	ATF_REQUIRE_EQ(2, mport_count_spaces("\t\n"));
}

ATF_TC_WITHOUT_HEAD(count_spaces_mixed);
ATF_TC_BODY(count_spaces_mixed, tc)
{
	(void)tc;

	ATF_REQUIRE_EQ(1, mport_count_spaces("hello world"));
	ATF_REQUIRE_EQ(2, mport_count_spaces("hello  world"));
	ATF_REQUIRE_EQ(3, mport_count_spaces(" hello world "));
	ATF_REQUIRE_EQ(5, mport_count_spaces(" \thello \nworld "));
}

ATF_TC_WITHOUT_HEAD(mport_file_exists_existing_file);
ATF_TC_BODY(mport_file_exists_existing_file, tc)
{
	(void)tc;

	ATF_REQUIRE_EQ(1, mport_file_exists("/etc/passwd"));
}

ATF_TC_WITHOUT_HEAD(mport_file_exists_nonexistent_file);
ATF_TC_BODY(mport_file_exists_nonexistent_file, tc)
{
	(void)tc;

	ATF_REQUIRE_EQ(0, mport_file_exists("/this/file/does/not/exist/ever/12345"));
}

ATF_TC_WITHOUT_HEAD(mport_file_exists_directory);
ATF_TC_BODY(mport_file_exists_directory, tc)
{
	(void)tc;

	ATF_REQUIRE_EQ(1, mport_file_exists("/etc"));
}

ATF_TC_WITH_CLEANUP(is_elf_file_true);
ATF_TC_HEAD(is_elf_file_true, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_is_elf_file returns true for an ELF file");
}
ATF_TC_BODY(is_elf_file_true, tc)
{
	const char *filename;
	const char magic[] = "\x7F"
			     "ELF"
			     "some other data";

	(void)tc;

	filename = "test_elf_file";
	create_file(filename, magic, sizeof(magic) - 1);

	ATF_CHECK(mport_is_elf_file(filename));
}
ATF_TC_CLEANUP(is_elf_file_true, tc)
{
	(void)tc;

	(void)unlink("test_elf_file");
}

ATF_TC_WITH_CLEANUP(is_elf_file_false_text);
ATF_TC_HEAD(is_elf_file_false_text, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_is_elf_file returns false for a text file");
}
ATF_TC_BODY(is_elf_file_false_text, tc)
{
	const char *filename;
	const char content[] = "Hello World!";

	(void)tc;

	filename = "test_text_file.txt";
	create_file(filename, content, sizeof(content) - 1);

	ATF_CHECK(!mport_is_elf_file(filename));
}
ATF_TC_CLEANUP(is_elf_file_false_text, tc)
{
	(void)tc;

	(void)unlink("test_text_file.txt");
}

ATF_TC_WITH_CLEANUP(is_elf_file_false_small);
ATF_TC_HEAD(is_elf_file_false_small, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_is_elf_file returns false for a file smaller than ELF magic");
}
ATF_TC_BODY(is_elf_file_false_small, tc)
{
	const char *filename;
	const char content[] = "ELF";

	(void)tc;

	filename = "test_small_file";
	create_file(filename, content, sizeof(content) - 1);

	ATF_CHECK(!mport_is_elf_file(filename));
}
ATF_TC_CLEANUP(is_elf_file_false_small, tc)
{
	(void)tc;

	(void)unlink("test_small_file");
}

ATF_TC_WITHOUT_HEAD(is_elf_file_false_missing);
ATF_TC_BODY(is_elf_file_false_missing, tc)
{
	(void)tc;

	ATF_CHECK(!mport_is_elf_file("nonexistent_file_that_should_not_exist"));
}

ATF_TC_WITH_CLEANUP(mport_mkdir_success);
ATF_TC_HEAD(mport_mkdir_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_mkdir creates a directory with mode 755");
}
ATF_TC_BODY(mport_mkdir_success, tc)
{
	struct stat sb;
	mode_t old_umask;

	(void)tc;

	(void)rmdir(TEST_DIR);
	old_umask = umask(0022);

	ATF_REQUIRE_EQ(MPORT_OK, mport_mkdir(TEST_DIR));
	umask(old_umask);

	ATF_REQUIRE_EQ(0, stat(TEST_DIR, &sb));
	ATF_REQUIRE(S_ISDIR(sb.st_mode));
	ATF_REQUIRE_EQ(0755, sb.st_mode & 0777);
}
ATF_TC_CLEANUP(mport_mkdir_success, tc)
{
	(void)tc;

	(void)rmdir(TEST_DIR);
}

ATF_TC_WITH_CLEANUP(mport_mkdir_existing);
ATF_TC_HEAD(mport_mkdir_existing, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_mkdir succeeds if the directory exists");
}
ATF_TC_BODY(mport_mkdir_existing, tc)
{
	struct stat sb;

	(void)tc;

	(void)rmdir(TEST_DIR);
	ATF_REQUIRE_EQ(0, mkdir(TEST_DIR, 0755));

	ATF_REQUIRE_EQ(MPORT_OK, mport_mkdir(TEST_DIR));
	ATF_REQUIRE_EQ(0, stat(TEST_DIR, &sb));
	ATF_REQUIRE(S_ISDIR(sb.st_mode));
}
ATF_TC_CLEANUP(mport_mkdir_existing, tc)
{
	(void)tc;

	(void)rmdir(TEST_DIR);
}

ATF_TC_WITH_CLEANUP(mport_mkdir_file_exists);
ATF_TC_HEAD(mport_mkdir_file_exists, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_mkdir succeeds if a file with the same name exists");
}
ATF_TC_BODY(mport_mkdir_file_exists, tc)
{
	(void)tc;

	(void)unlink(TEST_DIR);
	create_file(TEST_DIR, "test", 4);

	/*
	 * Current implementation returns MPORT_OK because errno is EEXIST.
	 * This is actually a bit questionable since it's not a directory,
	 * but we are testing current behavior.
	 */
	ATF_REQUIRE_EQ(MPORT_OK, mport_mkdir(TEST_DIR));
}
ATF_TC_CLEANUP(mport_mkdir_file_exists, tc)
{
	(void)tc;

	(void)unlink(TEST_DIR);
}

ATF_TC_WITHOUT_HEAD(mport_mkdir_fail_parent);
ATF_TC_BODY(mport_mkdir_fail_parent, tc)
{
	(void)tc;

	ATF_REQUIRE(mport_mkdir("no/such/dir") != MPORT_OK);
}

ATF_TC_WITH_CLEANUP(mport_rmdir_empty);
ATF_TC_HEAD(mport_rmdir_empty, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_rmdir removes an empty directory");
}
ATF_TC_BODY(mport_rmdir_empty, tc)
{
	const char *test_dir;

	(void)tc;

	test_dir = "test_empty_dir";
	ATF_REQUIRE_EQ(0, mkdir(test_dir, 0755));
	ATF_REQUIRE_EQ(MPORT_OK, mport_rmdir(test_dir, 0));
	ATF_REQUIRE(access(test_dir, F_OK) != 0);
}
ATF_TC_CLEANUP(mport_rmdir_empty, tc)
{
	(void)tc;

	(void)rmdir("test_empty_dir");
}

ATF_TC_WITH_CLEANUP(mport_rmdir_nonempty_ignore);
ATF_TC_HEAD(mport_rmdir_nonempty_ignore, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_rmdir ignores non-empty directories when requested");
}
ATF_TC_BODY(mport_rmdir_nonempty_ignore, tc)
{
	const char *test_dir;
	const char *test_file;

	(void)tc;

	test_dir = "test_nonempty_dir_ignore";
	test_file = "test_nonempty_dir_ignore/file.txt";
	ATF_REQUIRE_EQ(0, mkdir(test_dir, 0755));
	create_file(test_file, "", 0);

	ATF_REQUIRE_EQ(MPORT_OK, mport_rmdir(test_dir, 1));
	ATF_REQUIRE(access(test_dir, F_OK) == 0);
}
ATF_TC_CLEANUP(mport_rmdir_nonempty_ignore, tc)
{
	(void)tc;

	(void)unlink("test_nonempty_dir_ignore/file.txt");
	(void)rmdir("test_nonempty_dir_ignore");
}

ATF_TC_WITH_CLEANUP(mport_rmdir_nonempty_noignore);
ATF_TC_HEAD(mport_rmdir_nonempty_noignore, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_rmdir fails for non-empty directories unless ignored");
}
ATF_TC_BODY(mport_rmdir_nonempty_noignore, tc)
{
	const char *test_dir;
	const char *test_file;

	(void)tc;

	test_dir = "test_nonempty_dir_noignore";
	test_file = "test_nonempty_dir_noignore/file.txt";
	ATF_REQUIRE_EQ(0, mkdir(test_dir, 0755));
	create_file(test_file, "", 0);

	ATF_REQUIRE(mport_rmdir(test_dir, 0) != MPORT_OK);
}
ATF_TC_CLEANUP(mport_rmdir_nonempty_noignore, tc)
{
	(void)tc;

	(void)unlink("test_nonempty_dir_noignore/file.txt");
	(void)rmdir("test_nonempty_dir_noignore");
}

ATF_TC_WITHOUT_HEAD(mport_rmdir_notfound_ignore);
ATF_TC_BODY(mport_rmdir_notfound_ignore, tc)
{
	const char *test_dir;

	(void)tc;

	test_dir = "test_notfound_dir";
	ATF_REQUIRE(access(test_dir, F_OK) != 0);
	ATF_REQUIRE_EQ(MPORT_OK, mport_rmdir(test_dir, 1));
}

ATF_TC_WITHOUT_HEAD(mport_directory_null);
ATF_TC_BODY(mport_directory_null, tc)
{
	(void)tc;

	ATF_REQUIRE_EQ(NULL, mport_directory(NULL));
}

ATF_TC_WITHOUT_HEAD(mport_directory_absolute);
ATF_TC_BODY(mport_directory_absolute, tc)
{
	char *dir;
	(void)tc;

	dir = mport_directory("/usr/local/bin/mport");
	ATF_REQUIRE_STREQ("/usr/local/bin", dir);
	free(dir);

	dir = mport_directory("/mport");
	ATF_REQUIRE_STREQ("/", dir);
	free(dir);

	dir = mport_directory("/");
	ATF_REQUIRE_STREQ("/", dir);
	free(dir);
}

ATF_TC_WITHOUT_HEAD(mport_directory_relative);
ATF_TC_BODY(mport_directory_relative, tc)
{
	char *dir;
	char cwd[PATH_MAX];
	char expected[PATH_MAX];
	(void)tc;

	ATF_REQUIRE(getcwd(cwd, sizeof(cwd)) != NULL);

	dir = mport_directory("mport");
	ATF_REQUIRE_STREQ(cwd, dir);
	free(dir);

	dir = mport_directory("bin/mport");
	snprintf(expected, sizeof(expected), "%s/bin", cwd);
	ATF_REQUIRE_STREQ(expected, dir);
	free(dir);
}

ATF_TC(mport_parselist_basic);
ATF_TC_HEAD(mport_parselist_basic, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_parselist splits a list on whitespace");
}
ATF_TC_BODY(mport_parselist_basic, tc)
{
	char in[] = "foo bar baz";
	char **list = NULL;
	size_t n = 0;

	(void)tc;

	mport_parselist(in, &list, &n);
	ATF_REQUIRE_EQ((size_t)3, n);
	ATF_REQUIRE(list != NULL);
	ATF_REQUIRE_STREQ("foo", list[0]);
	ATF_REQUIRE_STREQ("bar", list[1]);
	ATF_REQUIRE_STREQ("baz", list[2]);
	ATF_REQUIRE(list[3] == NULL);

	for (size_t i = 0; i < n; i++)
		free(list[i]);
	free(list);
}

ATF_TC(mport_parselist_trailing_ws);
ATF_TC_HEAD(mport_parselist_trailing_ws, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_parselist handles trailing whitespace without an invalid free");
}
ATF_TC_BODY(mport_parselist_trailing_ws, tc)
{
	char in[] = "foo bar \t";
	char **list = NULL;
	size_t n = 0;

	(void)tc;

	/* Pre-fix this freed a strsep-advanced interior pointer and corrupted
	   the heap; reaching the asserts proves it no longer does. */
	mport_parselist(in, &list, &n);
	ATF_REQUIRE_EQ((size_t)2, n);
	ATF_REQUIRE(list != NULL);
	ATF_REQUIRE_STREQ("foo", list[0]);
	ATF_REQUIRE_STREQ("bar", list[1]);

	for (size_t i = 0; i < n; i++)
		free(list[i]);
	free(list);
}

ATF_TC(mport_parselist_tll_no_dup);
ATF_TC_HEAD(mport_parselist_tll_no_dup, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_parselist_tll pushes each token exactly once");
}
ATF_TC_BODY(mport_parselist_tll_no_dup, tc)
{
	char in[] = "foo bar baz";
	stringlist_t list = tll_init();

	(void)tc;

	/* Pre-fix each token was pushed twice, so this was 6. */
	mport_parselist_tll(in, &list);
	ATF_REQUIRE_EQ((size_t)3, tll_length(list));

	tll_free_and_free(list, free);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, mport_check_answer_bool_null);
	ATF_TP_ADD_TC(tp, mport_check_answer_bool_true);
	ATF_TP_ADD_TC(tp, mport_check_answer_bool_false);
	ATF_TP_ADD_TC(tp, mport_check_answer_bool_invalid);
	ATF_TP_ADD_TC(tp, count_spaces_empty);
	ATF_TP_ADD_TC(tp, count_spaces_none);
	ATF_TP_ADD_TC(tp, count_spaces_only);
	ATF_TP_ADD_TC(tp, count_spaces_mixed);
	ATF_TP_ADD_TC(tp, mport_file_exists_existing_file);
	ATF_TP_ADD_TC(tp, mport_file_exists_nonexistent_file);
	ATF_TP_ADD_TC(tp, mport_file_exists_directory);
	ATF_TP_ADD_TC(tp, is_elf_file_true);
	ATF_TP_ADD_TC(tp, is_elf_file_false_text);
	ATF_TP_ADD_TC(tp, is_elf_file_false_small);
	ATF_TP_ADD_TC(tp, is_elf_file_false_missing);
	ATF_TP_ADD_TC(tp, mport_mkdir_success);
	ATF_TP_ADD_TC(tp, mport_mkdir_existing);
	ATF_TP_ADD_TC(tp, mport_mkdir_file_exists);
	ATF_TP_ADD_TC(tp, mport_mkdir_fail_parent);
	ATF_TP_ADD_TC(tp, mport_rmdir_empty);
	ATF_TP_ADD_TC(tp, mport_rmdir_nonempty_ignore);
	ATF_TP_ADD_TC(tp, mport_rmdir_nonempty_noignore);
	ATF_TP_ADD_TC(tp, mport_rmdir_notfound_ignore);
	ATF_TP_ADD_TC(tp, mport_directory_null);
	ATF_TP_ADD_TC(tp, mport_directory_absolute);
	ATF_TP_ADD_TC(tp, mport_directory_relative);
	ATF_TP_ADD_TC(tp, mport_parselist_basic);
	ATF_TP_ADD_TC(tp, mport_parselist_trailing_ws);
	ATF_TP_ADD_TC(tp, mport_parselist_tll_no_dup);

	return atf_no_error();
}
