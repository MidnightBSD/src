#include <sys/cdefs.h>

#include <atf-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libmport/mport.h"

/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullderef -nullpass -nullret -nullstate@*/
/*@-retvalint -retvalother -type -unrecog@*/

/*
 * Force stdin to a non-terminal so the confirmation/selection callbacks take
 * their non-interactive path deterministically (ATF runs each test case in its
 * own process, so reopening stdin here does not leak into other cases).
 */
static void
make_stdin_non_tty(void)
{
	ATF_REQUIRE(freopen("/dev/null", "r", stdin) != NULL);
	ATF_REQUIRE(!isatty(fileno(stdin)));
}

ATF_TC(confirm_assume_always_yes);
ATF_TC_HEAD(confirm_assume_always_yes, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_default_confirm_cb returns MPORT_OK when ASSUME_ALWAYS_YES is set");
}
ATF_TC_BODY(confirm_assume_always_yes, tc)
{
	(void)tc;

	(void)unsetenv("MAGUS");
	ATF_REQUIRE_EQ(0, setenv("ASSUME_ALWAYS_YES", "1", 1));
	ATF_REQUIRE_EQ(MPORT_OK, mport_default_confirm_cb("Proceed?", "Yes", "No", 0));
	(void)unsetenv("ASSUME_ALWAYS_YES");
}

ATF_TC(confirm_magus);
ATF_TC_HEAD(confirm_magus, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_default_confirm_cb returns MPORT_OK when MAGUS is set");
}
ATF_TC_BODY(confirm_magus, tc)
{
	(void)tc;

	(void)unsetenv("ASSUME_ALWAYS_YES");
	ATF_REQUIRE_EQ(0, setenv("MAGUS", "1", 1));
	ATF_REQUIRE_EQ(MPORT_OK, mport_default_confirm_cb("Proceed?", "Yes", "No", 0));
	(void)unsetenv("MAGUS");
}

ATF_TC(confirm_assume_always_yes_non_tty);
ATF_TC_HEAD(confirm_assume_always_yes_non_tty, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "mport_default_confirm_cb returns MPORT_OK with ASSUME_ALWAYS_YES even on non-tty stdin");
}
ATF_TC_BODY(confirm_assume_always_yes_non_tty, tc)
{
	(void)tc;

	(void)unsetenv("MAGUS");
	ATF_REQUIRE_EQ(0, setenv("ASSUME_ALWAYS_YES", "1", 1));
	make_stdin_non_tty();
	ATF_REQUIRE_EQ(MPORT_OK, mport_default_confirm_cb("Proceed?", "Yes", "No", 0));
	(void)unsetenv("ASSUME_ALWAYS_YES");
}

ATF_TC(confirm_magus_non_tty);
ATF_TC_HEAD(confirm_magus_non_tty, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "mport_default_confirm_cb returns MPORT_OK with MAGUS even on non-tty stdin");
}
ATF_TC_BODY(confirm_magus_non_tty, tc)
{
	(void)tc;

	(void)unsetenv("ASSUME_ALWAYS_YES");
	ATF_REQUIRE_EQ(0, setenv("MAGUS", "1", 1));
	make_stdin_non_tty();
	ATF_REQUIRE_EQ(MPORT_OK, mport_default_confirm_cb("Proceed?", "Yes", "No", 0));
	(void)unsetenv("MAGUS");
}

ATF_TC(confirm_non_tty_aborts);
ATF_TC_HEAD(confirm_non_tty_aborts, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "mport_default_confirm_cb refuses (non-OK) on a non-terminal without assume-yes");
}
ATF_TC_BODY(confirm_non_tty_aborts, tc)
{
	(void)tc;

	(void)unsetenv("ASSUME_ALWAYS_YES");
	(void)unsetenv("MAGUS");
	make_stdin_non_tty();

	/* Must not block on input and must not report confirmation. */
	ATF_REQUIRE(mport_default_confirm_cb("Proceed?", "Yes", "No", 0) != MPORT_OK);
	/* Default of 1 ("yes") must not flip a non-terminal into a confirmation. */
	ATF_REQUIRE(mport_default_confirm_cb("Proceed?", "Yes", "No", 1) != MPORT_OK);
}

ATF_TC(select_non_tty_aborts);
ATF_TC_HEAD(select_non_tty_aborts, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_default_select_cb returns -1 on a non-terminal without assume-yes");
}
ATF_TC_BODY(select_non_tty_aborts, tc)
{
	/*
	 * Two zeroed entries are enough: the assume-yes and non-tty paths both
	 * return before any entry field is dereferenced.
	 */
	mportIndexEntry entries[2];
	mportIndexEntry *choices[] = { &entries[0], &entries[1], NULL };

	(void)tc;
	(void)memset(entries, 0, sizeof(entries));

	(void)unsetenv("ASSUME_ALWAYS_YES");
	(void)unsetenv("MAGUS");
	make_stdin_non_tty();

	ATF_REQUIRE_EQ(-1, mport_default_select_cb("Pick one", choices, 0));
}

ATF_TC(select_assume_always_yes);
ATF_TC_HEAD(select_assume_always_yes, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_default_select_cb returns the default with ASSUME_ALWAYS_YES set");
}
ATF_TC_BODY(select_assume_always_yes, tc)
{
	mportIndexEntry entries[2];
	mportIndexEntry *choices[] = { &entries[0], &entries[1], NULL };

	(void)tc;
	(void)memset(entries, 0, sizeof(entries));

	(void)unsetenv("MAGUS");
	ATF_REQUIRE_EQ(0, setenv("ASSUME_ALWAYS_YES", "1", 1));
	ATF_REQUIRE_EQ(1, mport_default_select_cb("Pick one", choices, 1));
	(void)unsetenv("ASSUME_ALWAYS_YES");
}

ATF_TC(select_magus);
ATF_TC_HEAD(select_magus, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_default_select_cb returns the default with MAGUS set");
}
ATF_TC_BODY(select_magus, tc)
{
	mportIndexEntry entries[2];
	mportIndexEntry *choices[] = { &entries[0], &entries[1], NULL };

	(void)tc;
	(void)memset(entries, 0, sizeof(entries));

	(void)unsetenv("ASSUME_ALWAYS_YES");
	ATF_REQUIRE_EQ(0, setenv("MAGUS", "1", 1));
	ATF_REQUIRE_EQ(1, mport_default_select_cb("Pick one", choices, 1));
	(void)unsetenv("MAGUS");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, confirm_assume_always_yes);
	ATF_TP_ADD_TC(tp, confirm_magus);
	ATF_TP_ADD_TC(tp, confirm_assume_always_yes_non_tty);
	ATF_TP_ADD_TC(tp, confirm_magus_non_tty);
	ATF_TP_ADD_TC(tp, confirm_non_tty_aborts);
	ATF_TP_ADD_TC(tp, select_non_tty_aborts);
	ATF_TP_ADD_TC(tp, select_assume_always_yes);
	ATF_TP_ADD_TC(tp, select_magus);

	return atf_no_error();
}
