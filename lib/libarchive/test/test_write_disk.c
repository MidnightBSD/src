/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"
__FBSDID("$FreeBSD: src/lib/libarchive/test/test_write_disk.c,v 1.6.2.5 2008/11/28 20:08:47 kientzle Exp $");

#if ARCHIVE_VERSION_STAMP >= 1009000

#define UMASK 022

static void create(struct archive_entry *ae, const char *msg)
{
	struct archive *ad;
	struct stat st;

	/* Write the entry to disk. */
	assert((ad = archive_write_disk_new()) != NULL);
	failure("%s", msg);
	assertEqualIntA(ad, 0, archive_write_header(ad, ae));
	assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
#if ARCHIVE_API_VERSION > 1
	assertEqualInt(0, archive_write_finish(ad));
#else
	archive_write_finish(ad);
#endif
	/* Test the entries on disk. */
	assert(0 == stat(archive_entry_pathname(ae), &st));
	failure("st.st_mode=%o archive_entry_mode(ae)=%o",
	    st.st_mode, archive_entry_mode(ae));
	/* When verifying a dir, ignore the S_ISGID bit, as some systems set
	 * that automatically. */
	if (archive_entry_filetype(ae) == AE_IFDIR)
		st.st_mode &= ~S_ISGID;
	assertEqualInt(st.st_mode, archive_entry_mode(ae) & ~UMASK);
}

static void create_reg_file(struct archive_entry *ae, const char *msg)
{
	static const char data[]="abcdefghijklmnopqrstuvwxyz";
	struct archive *ad;
	struct stat st;
	time_t now;

	/* Write the entry to disk. */
	assert((ad = archive_write_disk_new()) != NULL);
        archive_write_disk_set_options(ad, ARCHIVE_EXTRACT_TIME);
	failure("%s", msg);
	/*
	 * A touchy API design issue: archive_write_data() does (as of
	 * 2.4.12) enforce the entry size as a limit on the data
	 * written to the file.  This was not enforced prior to
	 * 2.4.12.  The change was prompted by the refined
	 * hardlink-restore semantics introduced at that time.  In
	 * short, libarchive needs to know whether a "hardlink entry"
	 * is going to overwrite the contents so that it can know
	 * whether or not to open the file for writing.  This implies
	 * that there is a fundamental semantic difference between an
	 * entry with a zero size and one with a non-zero size in the
	 * case of hardlinks and treating the hardlink case
	 * differently from the regular file case is just asking for
	 * trouble.  So, a zero size must always mean that no data
	 * will be accepted, which is consistent with the file size in
	 * the entry being a maximum size.
	 */
	archive_entry_set_size(ae, sizeof(data));
	archive_entry_set_mtime(ae, 123456789, 0);
	assertEqualIntA(ad, 0, archive_write_header(ad, ae));
	assertEqualInt(sizeof(data), archive_write_data(ad, data, sizeof(data)));
	assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
#if ARCHIVE_API_VERSION > 1
	assertEqualInt(0, archive_write_finish(ad));
#else
	archive_write_finish(ad);
#endif
	/* Test the entries on disk. */
	assert(0 == stat(archive_entry_pathname(ae), &st));
	failure("st.st_mode=%o archive_entry_mode(ae)=%o",
	    st.st_mode, archive_entry_mode(ae));
	assertEqualInt(st.st_mode, (archive_entry_mode(ae) & ~UMASK));
        assertEqualInt(st.st_size, sizeof(data));
        assertEqualInt(st.st_mtime, 123456789);
        failure("No atime was specified, so atime should get set to current time");
	now = time(NULL);
        assert(st.st_atime <= now && st.st_atime > now - 5);
}

static void create_reg_file2(struct archive_entry *ae, const char *msg)
{
	const int datasize = 100000;
	char *data;
	char *compare;
	struct archive *ad;
	struct stat st;
	int i, fd;

	data = malloc(datasize);
	for (i = 0; i < datasize; i++)
		data[i] = (char)(i % 256);

	/* Write the entry to disk. */
	assert((ad = archive_write_disk_new()) != NULL);
	failure("%s", msg);
	/*
	 * See above for an explanation why this next call
	 * is necessary.
	 */
	archive_entry_set_size(ae, datasize);
	assertEqualIntA(ad, 0, archive_write_header(ad, ae));
	for (i = 0; i < datasize - 999; i += 1000) {
		assertEqualIntA(ad, ARCHIVE_OK,
		    archive_write_data_block(ad, data + i, 1000, i));
	}
	assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
#if ARCHIVE_API_VERSION > 1
	assertEqualInt(0, archive_write_finish(ad));
#else
	archive_write_finish(ad);
#endif
	/* Test the entries on disk. */
	assert(0 == stat(archive_entry_pathname(ae), &st));
	failure("st.st_mode=%o archive_entry_mode(ae)=%o",
	    st.st_mode, archive_entry_mode(ae));
	assertEqualInt(st.st_mode, (archive_entry_mode(ae) & ~UMASK));
	assertEqualInt(st.st_size, i);

	compare = malloc(datasize);
	fd = open(archive_entry_pathname(ae), O_RDONLY);
	assertEqualInt(datasize, read(fd, compare, datasize));
	close(fd);
	assert(memcmp(compare, data, datasize) == 0);
	free(compare);
	free(data);
}

static void create_reg_file3(struct archive_entry *ae, const char *msg)
{
	static const char data[]="abcdefghijklmnopqrstuvwxyz";
	struct archive *ad;
	struct stat st;

	/* Write the entry to disk. */
	assert((ad = archive_write_disk_new()) != NULL);
	failure("%s", msg);
	/* Set the size smaller than the data and verify the truncation. */
	archive_entry_set_size(ae, 5);
	assertEqualIntA(ad, 0, archive_write_header(ad, ae));
	assertEqualInt(5, archive_write_data(ad, data, sizeof(data)));
	assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
#if ARCHIVE_VERSION_NUMBER < 2000000
	archive_write_finish(ad);
#else
	assertEqualInt(0, archive_write_finish(ad));
#endif
	/* Test the entry on disk. */
	assert(0 == stat(archive_entry_pathname(ae), &st));
	failure("st.st_mode=%o archive_entry_mode(ae)=%o",
	    st.st_mode, archive_entry_mode(ae));
	assertEqualInt(st.st_mode, (archive_entry_mode(ae) & ~UMASK));
	assertEqualInt(st.st_size, 5);
}


static void create_reg_file4(struct archive_entry *ae, const char *msg)
{
	static const char data[]="abcdefghijklmnopqrstuvwxyz";
	struct archive *ad;
	struct stat st;

	/* Write the entry to disk. */
	assert((ad = archive_write_disk_new()) != NULL);
	/* Leave the size unset.  The data should not be truncated. */
	assertEqualIntA(ad, 0, archive_write_header(ad, ae));
	assertEqualInt(ARCHIVE_OK,
	    archive_write_data_block(ad, data, sizeof(data), 0));
	assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
#if ARCHIVE_VERSION_NUMBER < 2000000
	archive_write_finish(ad);
#else
	assertEqualInt(0, archive_write_finish(ad));
#endif
	/* Test the entry on disk. */
	assert(0 == stat(archive_entry_pathname(ae), &st));
	failure("st.st_mode=%o archive_entry_mode(ae)=%o",
	    st.st_mode, archive_entry_mode(ae));
	assertEqualInt(st.st_mode, (archive_entry_mode(ae) & ~UMASK));
	failure(msg);
	assertEqualInt(st.st_size, sizeof(data));
}
#endif

DEFINE_TEST(test_write_disk)
{
#if ARCHIVE_VERSION_STAMP < 1009000
	skipping("archive_write_disk interface");
#else
	struct archive_entry *ae;

	/* Force the umask to something predictable. */
	umask(UMASK);

	/* A regular file. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	create_reg_file(ae, "Test creating a regular file");
	archive_entry_free(ae);

	/* Another regular file. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file2");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	create_reg_file2(ae, "Test creating another regular file");
	archive_entry_free(ae);

	/* A regular file with a size restriction */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file3");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	create_reg_file3(ae, "Regular file with size restriction");
	archive_entry_free(ae);

	/* A regular file with an unspecified size */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file3");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	create_reg_file4(ae, "Regular file with unspecified size");
	archive_entry_free(ae);

	/* A regular file over an existing file */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, S_IFREG | 0724);
	create(ae, "Test creating a file over an existing file.");
	archive_entry_free(ae);

	/* A directory. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "dir");
	archive_entry_set_mode(ae, S_IFDIR | 0555);
	create(ae, "Test creating a regular dir.");
	archive_entry_free(ae);

	/* A directory over an existing file. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, S_IFDIR | 0742);
	create(ae, "Test creating a dir over an existing file.");
	archive_entry_free(ae);

	/* A file over an existing dir. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, S_IFREG | 0744);
	create(ae, "Test creating a file over an existing dir.");
	archive_entry_free(ae);
#endif
}
