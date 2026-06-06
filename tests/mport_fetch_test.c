#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <stdio.h>
#include <fetch.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libmport/mport.h"

/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullderef -nullpass -nullret -nullstate@*/
/*@-retvalint -retvalother -type -unrecog@*/

#define BUNDLE_NAME "testpkg-1.0.mport"
#define FETCH_DIR "fetch-output"
#define MIRROR_URL "https://packages.example.invalid"

static char last_fetch_url[512];
static int fetch_call_count;

static void
discard_msg(const char *msg)
{
	(void)msg;
}

static void
discard_progress_init(const char *msg)
{
	(void)msg;
}

static void
discard_progress_step(int current, int total, const char *msg)
{
	(void)current;
	(void)total;
	(void)msg;
}

static void
discard_progress_free(void)
{
}

int
mport_index_get_mirror_list(mportInstance *mport, char ***mirrors, int *mirrorCount)
{
	(void)mport;

	*mirrors = calloc(2, sizeof(char *));
	ATF_REQUIRE(*mirrors != NULL);

	(*mirrors)[0] = strdup(MIRROR_URL);
	ATF_REQUIRE((*mirrors)[0] != NULL);

	*mirrorCount = 1;
	return MPORT_OK;
}

FILE *
fetchXGetURL(const char *url, struct url_stat *ustat, const char *flags)
{
	FILE *remote;
	const char payload[] = "fake package payload\n";

	(void)flags;

	remote = tmpfile();
	if (remote == NULL)
		return NULL;
	if (fwrite(payload, 1, strlen(payload), remote) != strlen(payload)) {
		fclose(remote);
		return NULL;
	}
	rewind(remote);

	memset(ustat, 0, sizeof(*ustat));
	ustat->size = strlen(payload);

	snprintf(last_fetch_url, sizeof(last_fetch_url), "%s", url);
	fetch_call_count++;

	return remote;
}

static bool
ends_with(const char *value, const char *suffix)
{
	size_t value_len = strlen(value);
	size_t suffix_len = strlen(suffix);

	if (suffix_len > value_len)
		return false;

	return strcmp(value + value_len - suffix_len, suffix) == 0;
}

static void
init_test_instance(mportInstance *mport)
{
	memset(mport, 0, sizeof(*mport));
	mport->flags = MPORT_INST_HAVE_INDEX;
	mport->msg_cb = discard_msg;
	mport->progress_init_cb = discard_progress_init;
	mport->progress_step_cb = discard_progress_step;
	mport->progress_free_cb = discard_progress_free;
}

static void
check_bundle_fetch_url(const char *env_value, const char *expected_prefix)
{
	mportInstance mport;
	const char fetched_bundle[] = FETCH_DIR "/" BUNDLE_NAME;

	init_test_instance(&mport);
	last_fetch_url[0] = '\0';
	fetch_call_count = 0;

	(void)unlink(fetched_bundle);
	(void)rmdir(FETCH_DIR);

	if (env_value == NULL)
		unsetenv("MPORT_FORCE_HTTP");
	else
		ATF_REQUIRE_EQ(0, setenv("MPORT_FORCE_HTTP", env_value, 1));

	ATF_REQUIRE_EQ(MPORT_OK, mport_fetch_bundle(&mport, FETCH_DIR, BUNDLE_NAME));
	ATF_REQUIRE_EQ(1, fetch_call_count);
	ATF_REQUIRE(strncmp(last_fetch_url, expected_prefix, strlen(expected_prefix)) == 0);
	ATF_REQUIRE(ends_with(last_fetch_url, "/" BUNDLE_NAME));
	ATF_REQUIRE(access(fetched_bundle, F_OK) == 0);

	(void)unlink(fetched_bundle);
	(void)rmdir(FETCH_DIR);
	unsetenv("MPORT_FORCE_HTTP");
}

ATF_TC_WITH_CLEANUP(force_http_unset_keeps_https);
ATF_TC_HEAD(force_http_unset_keeps_https, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "HTTPS package URLs are unchanged unless MPORT_FORCE_HTTP is set");
}
ATF_TC_BODY(force_http_unset_keeps_https, tc)
{
	(void)tc;

	check_bundle_fetch_url(NULL, "https://packages.example.invalid/");
}
ATF_TC_CLEANUP(force_http_unset_keeps_https, tc)
{
	(void)tc;

	(void)unlink(FETCH_DIR "/" BUNDLE_NAME);
	(void)rmdir(FETCH_DIR);
	unsetenv("MPORT_FORCE_HTTP");
}

ATF_TC_WITH_CLEANUP(force_http_empty_keeps_https);
ATF_TC_HEAD(force_http_empty_keeps_https, tc)
{
	atf_tc_set_md_var(tc, "descr", "Empty MPORT_FORCE_HTTP values do not force HTTP");
}
ATF_TC_BODY(force_http_empty_keeps_https, tc)
{
	(void)tc;

	check_bundle_fetch_url("", "https://packages.example.invalid/");
}
ATF_TC_CLEANUP(force_http_empty_keeps_https, tc)
{
	(void)tc;

	(void)unlink(FETCH_DIR "/" BUNDLE_NAME);
	(void)rmdir(FETCH_DIR);
	unsetenv("MPORT_FORCE_HTTP");
}

ATF_TC_WITH_CLEANUP(force_http_set_rewrites_https);
ATF_TC_HEAD(force_http_set_rewrites_https, tc)
{
	atf_tc_set_md_var(tc, "descr", "MPORT_FORCE_HTTP rewrites HTTPS package URLs to HTTP");
}
ATF_TC_BODY(force_http_set_rewrites_https, tc)
{
	(void)tc;

	check_bundle_fetch_url("1", "http://packages.example.invalid/");
}
ATF_TC_CLEANUP(force_http_set_rewrites_https, tc)
{
	(void)tc;

	(void)unlink(FETCH_DIR "/" BUNDLE_NAME);
	(void)rmdir(FETCH_DIR);
	unsetenv("MPORT_FORCE_HTTP");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, force_http_unset_keeps_https);
	ATF_TP_ADD_TC(tp, force_http_empty_keeps_https);
	ATF_TP_ADD_TC(tp, force_http_set_rewrites_https);

	return atf_no_error();
}
