#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/endian.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libmport/mport.h"

#define TEST_ROOT "test-osrelease-root"
#define MPORT_SETTING_TARGET_OS "target_os"
#define ABI_SOURCE "/bin/sh"
#define LINUX_LIBZ "/compat/linux/lib64/libz.so.1"
#define ABI_FILE_PATH TEST_ROOT "/abi"
#define NOT_ELF_PATH TEST_ROOT "/not-elf"

int mport_db_do(sqlite3 *, const char *, ...);
char *mport_get_osreleasedate(void);

/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullpass -nullret -nullstate -paramuse@*/
/*@-retvalint -retvalother -type -unrecog@*/

static void
cleanup_test_root(void)
{
	(void)unlink(ABI_FILE_PATH);
	(void)unlink(NOT_ELF_PATH);
	(void)unlink(TEST_ROOT "/var/db/mport/master.db-wal");
	(void)unlink(TEST_ROOT "/var/db/mport/master.db-shm");
	(void)unlink(TEST_ROOT "/var/db/mport/master.db");
	(void)rmdir(TEST_ROOT "/var/db/mport/infrastructure");
	(void)rmdir(TEST_ROOT "/var/db/mport");
	(void)rmdir(TEST_ROOT "/var/db");
	(void)rmdir(TEST_ROOT "/var");
	(void)rmdir(TEST_ROOT);
}

static mportInstance *
create_test_instance(void)
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

	return mport;
}

ATF_TC_WITH_CLEANUP(osrelease_from_settings);
ATF_TC_HEAD(osrelease_from_settings, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_get_osrelease honors MPORT_SETTING_TARGET_OS");
}
ATF_TC_BODY(osrelease_from_settings, tc)
{
	mportInstance *mport;
	char *version;

	mport = create_test_instance();

	ATF_REQUIRE_EQ(MPORT_OK, mport_setting_set(mport, MPORT_SETTING_TARGET_OS, "9.9-TEST"));

	version = mport_get_osrelease(mport);
	ATF_REQUIRE(version != NULL);
	ATF_REQUIRE_STREQ("9.9-TEST", version);

	free(version);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(osrelease_from_settings, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(osrelease_null_instance);
ATF_TC_HEAD(osrelease_null_instance, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_get_osrelease handles NULL instance (falls back to system tools)");
}
ATF_TC_BODY(osrelease_null_instance, tc)
{
	char *version;

	/* On a real MidnightBSD system this will return the OS release,
	 * on Linux/others it might return NULL.
	 * We just ensure it doesn't crash.
	 */
	version = mport_get_osrelease(NULL);
	if (version != NULL) {
		free(version);
	}
}
ATF_TC_CLEANUP(osrelease_null_instance, tc)
{
	(void)tc;
}

ATF_TC_WITH_CLEANUP(osrelease_settings_null);
ATF_TC_HEAD(osrelease_settings_null, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_get_osrelease handles missing MPORT_SETTING_TARGET_OS");
}
ATF_TC_BODY(osrelease_settings_null, tc)
{
	mportInstance *mport;
	char *version;

	mport = create_test_instance();

	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db, "DELETE FROM settings WHERE name=%Q", MPORT_SETTING_TARGET_OS));

	version = mport_get_osrelease(mport);
	if (version != NULL) {
		free(version);
	}

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(osrelease_settings_null, tc)
{
	(void)tc;

	cleanup_test_root();
}

/*
 * Copy a MidnightBSD binary to path with its ABI tag replaced, so the
 * release can only have come from the file and not from the build host.
 */
static void
write_abi_file(const char *path, uint32_t tag)
{
	static const unsigned char note[] = { 12, 0, 0, 0, 4, 0, 0, 0, 1, 0, 0, 0, 'M', 'i', 'd',
		'n', 'i', 'g', 'h', 't', 'B', 'S', 'D', 0 };
	struct stat st;
	unsigned char *buf, *found;
	FILE *fp;
	size_t n;
	int closed;

	ATF_REQUIRE_EQ(0, stat(ABI_SOURCE, &st));
	buf = malloc((size_t)st.st_size);
	if (buf == NULL) {
		atf_tc_fail("out of memory");
		return;
	}
	fp = fopen(ABI_SOURCE, "rb");
	if (fp == NULL) {
		free(buf);
		atf_tc_fail("cannot read %s", ABI_SOURCE);
		return;
	}
	n = fread(buf, 1, (size_t)st.st_size, fp);
	(void)fclose(fp);
	ATF_REQUIRE_EQ((size_t)st.st_size, n);

	found = memmem(buf, n, note, sizeof(note));
	ATF_REQUIRE_MSG(found != NULL, "no MidnightBSD ABI note in %s", ABI_SOURCE);
	le32enc(found + sizeof(note), tag);

	fp = fopen(path, "wb");
	if (fp == NULL) {
		free(buf);
		atf_tc_fail("cannot create %s", path);
		return;
	}
	n = fwrite(buf, 1, (size_t)st.st_size, fp);
	closed = fclose(fp);
	free(buf);
	ATF_REQUIRE_EQ(0, closed);
	ATF_REQUIRE_EQ((size_t)st.st_size, n);
}

ATF_TC_WITH_CLEANUP(abi_file_release_from_note);
ATF_TC_HEAD(abi_file_release_from_note, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "ABI_FILE's MidnightBSD ABI note gives the release and version, not the host");
	atf_tc_set_md_var(tc, "require.files", ABI_SOURCE);
}
ATF_TC_BODY(abi_file_release_from_note, tc)
{
	mportInstance *mport;
	char *version;

	mport = create_test_instance();
	write_abi_file(ABI_FILE_PATH, 302005);
	ATF_REQUIRE_EQ(0, setenv("ABI_FILE", ABI_FILE_PATH, 1));

	version = mport_get_osrelease(mport);
	ATF_REQUIRE(version != NULL);
	ATF_REQUIRE_STREQ("3.2", version);
	free(version);

	version = mport_get_osreleasedate();
	ATF_REQUIRE(version != NULL);
	ATF_REQUIRE_STREQ("302005", version);
	free(version);

	/* ahead of the target_os setting */
	ATF_REQUIRE_EQ(MPORT_OK, mport_setting_set(mport, MPORT_SETTING_TARGET_OS, "9.9-TEST"));
	version = mport_get_osrelease(mport);
	ATF_REQUIRE(version != NULL);
	ATF_REQUIRE_STREQ("3.2", version);
	free(version);

	/* a two-digit minor is not truncated */
	write_abi_file(ABI_FILE_PATH, 410001);
	version = mport_get_osrelease(mport);
	ATF_REQUIRE(version != NULL);
	ATF_REQUIRE_STREQ("4.10", version);
	free(version);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(abi_file_release_from_note, tc)
{
	(void)tc;

	cleanup_test_root();
}

#if defined(__amd64__)
#define HOST_ARCH "amd64"
#define HOST_LEGACY_ARCH "x86:64"
#elif defined(__i386__)
#define HOST_ARCH "i386"
#define HOST_LEGACY_ARCH "x86:32"
#endif

ATF_TC_WITH_CLEANUP(abi_strings_from_settings);
ATF_TC_HEAD(abi_strings_from_settings, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "mport_get_abi/altabi use the target_os release and the host architecture");
}
ATF_TC_BODY(abi_strings_from_settings, tc)
{
	mportInstance *mport;
	char *value;

	mport = create_test_instance();
	ATF_REQUIRE_EQ(0, unsetenv("ABI_FILE"));
	ATF_REQUIRE_EQ(MPORT_OK, mport_setting_set(mport, MPORT_SETTING_TARGET_OS, "9.9"));

	value = mport_get_arch();
	ATF_REQUIRE(value != NULL);
	ATF_REQUIRE_STREQ(HOST_ARCH, value);
	free(value);

	value = mport_get_abi(mport);
	ATF_REQUIRE(value != NULL);
	ATF_REQUIRE_STREQ("MidnightBSD:9.9:" HOST_ARCH, value);
	free(value);

	value = mport_get_altabi(mport);
	ATF_REQUIRE(value != NULL);
	ATF_REQUIRE_STREQ("midnightbsd:9.9:" HOST_LEGACY_ARCH, value);
	free(value);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(abi_strings_from_settings, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(abi_strings_from_abi_file);
ATF_TC_HEAD(abi_strings_from_abi_file, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "ABI_FILE supplies the release and architecture of ABI and ALTABI, and a bad one is an error");
	atf_tc_set_md_var(tc, "require.files", ABI_SOURCE);
}
ATF_TC_BODY(abi_strings_from_abi_file, tc)
{
	mportInstance *mport;
	char *value;

	mport = create_test_instance();
	ATF_REQUIRE_EQ(MPORT_OK, mport_setting_set(mport, MPORT_SETTING_TARGET_OS, "9.9"));
	write_abi_file(ABI_FILE_PATH, 302005);
	ATF_REQUIRE_EQ(0, setenv("ABI_FILE", ABI_FILE_PATH, 1));

	/* /bin/sh is a host binary, so the arch is the host's; the release is the note's */
	value = mport_get_abi(mport);
	ATF_REQUIRE(value != NULL);
	ATF_REQUIRE_STREQ("MidnightBSD:3.2:" HOST_ARCH, value);
	free(value);

	value = mport_get_altabi(mport);
	ATF_REQUIRE(value != NULL);
	ATF_REQUIRE_STREQ("midnightbsd:3.2:" HOST_LEGACY_ARCH, value);
	free(value);

	ATF_REQUIRE_EQ(0, setenv("ABI_FILE", TEST_ROOT "/missing", 1));
	ATF_REQUIRE(mport_get_arch() == NULL);
	ATF_REQUIRE(mport_get_abi(mport) == NULL);
	ATF_REQUIRE(strstr(mport_err_string(), "ABI_FILE") != NULL);
	ATF_REQUIRE(mport_get_altabi(mport) == NULL);

	ATF_REQUIRE_EQ(0, unsetenv("ABI_FILE"));
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(abi_strings_from_abi_file, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(abi_file_without_note_falls_through);
ATF_TC_HEAD(abi_file_without_note_falls_through, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "an ABI_FILE without a MidnightBSD note leaves the release to target_os");
	atf_tc_set_md_var(tc, "require.files", LINUX_LIBZ);
}
ATF_TC_BODY(abi_file_without_note_falls_through, tc)
{
	mportInstance *mport;
	char *version;

	mport = create_test_instance();
	ATF_REQUIRE_EQ(MPORT_OK, mport_setting_set(mport, MPORT_SETTING_TARGET_OS, "9.9-TEST"));
	ATF_REQUIRE_EQ(0, setenv("ABI_FILE", LINUX_LIBZ, 1));

	version = mport_get_osrelease(mport);
	ATF_REQUIRE(version != NULL);
	ATF_REQUIRE_STREQ("9.9-TEST", version);
	free(version);

	/* an empty value is unset */
	ATF_REQUIRE_EQ(0, setenv("ABI_FILE", "", 1));
	version = mport_get_osrelease(mport);
	ATF_REQUIRE(version != NULL);
	ATF_REQUIRE_STREQ("9.9-TEST", version);
	free(version);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(abi_file_without_note_falls_through, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(abi_file_unreadable);
ATF_TC_HEAD(abi_file_unreadable, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "a missing or non-ELF ABI_FILE is an error, never the host's release");
}
ATF_TC_BODY(abi_file_unreadable, tc)
{
	mportInstance *mport;
	FILE *fp;
	int written, closed;

	mport = create_test_instance();
	ATF_REQUIRE_EQ(MPORT_OK, mport_setting_set(mport, MPORT_SETTING_TARGET_OS, "9.9-TEST"));

	ATF_REQUIRE_EQ(0, setenv("ABI_FILE", TEST_ROOT "/missing", 1));
	ATF_REQUIRE(mport_get_osrelease(mport) == NULL);
	ATF_REQUIRE(mport_err_code() != MPORT_OK);
	ATF_REQUIRE(mport_get_osreleasedate() == NULL);

	fp = fopen(NOT_ELF_PATH, "w");
	if (fp == NULL) {
		atf_tc_fail("cannot create %s", NOT_ELF_PATH);
		return;
	}
	written = fputs("#!/bin/sh\n", fp);
	closed = fclose(fp);
	ATF_REQUIRE(written >= 0);
	ATF_REQUIRE_EQ(0, closed);
	ATF_REQUIRE_EQ(0, setenv("ABI_FILE", NOT_ELF_PATH, 1));
	ATF_REQUIRE(mport_get_osrelease(mport) == NULL);
	ATF_REQUIRE(strstr(mport_err_string(), "not an ELF file") != NULL);

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(abi_file_unreadable, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, osrelease_from_settings);
	ATF_TP_ADD_TC(tp, osrelease_null_instance);
	ATF_TP_ADD_TC(tp, osrelease_settings_null);
	ATF_TP_ADD_TC(tp, abi_file_release_from_note);
	ATF_TP_ADD_TC(tp, abi_strings_from_settings);
	ATF_TP_ADD_TC(tp, abi_strings_from_abi_file);
	ATF_TP_ADD_TC(tp, abi_file_without_note_falls_through);
	ATF_TP_ADD_TC(tp, abi_file_unreadable);

	return atf_no_error();
}
