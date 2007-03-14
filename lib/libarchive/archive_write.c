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

#include "archive_platform.h"
__FBSDID("$FreeBSD: src/lib/libarchive/archive_write.c,v 1.15.2.3 2007/01/27 06:44:53 kientzle Exp $");

/*
 * This file contains the "essential" portions of the write API, that
 * is, stuff that will essentially always be used by any client that
 * actually needs to write a archive.  Optional pieces have been, as
 * far as possible, separated out into separate files to reduce
 * needlessly bloating statically-linked clients.
 */

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"

/*
 * Allocate, initialize and return an archive object.
 */
struct archive *
archive_write_new(void)
{
	struct archive *a;
	unsigned char *nulls;

	a = (struct archive *)malloc(sizeof(*a));
	if (a == NULL)
		return (NULL);
	memset(a, 0, sizeof(*a));
	a->magic = ARCHIVE_WRITE_MAGIC;
	a->user_uid = geteuid();
	a->bytes_per_block = ARCHIVE_DEFAULT_BYTES_PER_BLOCK;
	a->bytes_in_last_block = -1;	/* Default */
	a->state = ARCHIVE_STATE_NEW;
	a->pformat_data = &(a->format_data);

	/* Initialize a block of nulls for padding purposes. */
	a->null_length = 1024;
	nulls = (unsigned char *)malloc(a->null_length);
	if (nulls == NULL) {
		free(a);
		return (NULL);
	}
	memset(nulls, 0, a->null_length);
	a->nulls = nulls;
	/*
	 * Set default compression, but don't set a default format.
	 * Were we to set a default format here, we would force every
	 * client to link in support for that format, even if they didn't
	 * ever use it.
	 */
	archive_write_set_compression_none(a);
	return (a);
}

/*
 * Set the block size.  Returns 0 if successful.
 */
int
archive_write_set_bytes_per_block(struct archive *a, int bytes_per_block)
{
	__archive_check_magic(a, ARCHIVE_WRITE_MAGIC, ARCHIVE_STATE_NEW, "archive_write_set_bytes_per_block");
	a->bytes_per_block = bytes_per_block;
	return (ARCHIVE_OK);
}

/*
 * Get the current block size.  -1 if it has never been set.
 */
int
archive_write_get_bytes_per_block(struct archive *a)
{
	__archive_check_magic(a, ARCHIVE_WRITE_MAGIC, ARCHIVE_STATE_ANY, "archive_write_get_bytes_per_block");
	return (a->bytes_per_block);
}

/*
 * Set the size for the last block.
 * Returns 0 if successful.
 */
int
archive_write_set_bytes_in_last_block(struct archive *a, int bytes)
{
	__archive_check_magic(a, ARCHIVE_WRITE_MAGIC, ARCHIVE_STATE_ANY, "archive_write_set_bytes_in_last_block");
	a->bytes_in_last_block = bytes;
	return (ARCHIVE_OK);
}

/*
 * Return the value set above.  -1 indicates it has not been set.
 */
int
archive_write_get_bytes_in_last_block(struct archive *a)
{
	__archive_check_magic(a, ARCHIVE_WRITE_MAGIC, ARCHIVE_STATE_ANY, "archive_write_get_bytes_in_last_block");
	return (a->bytes_in_last_block);
}


/*
 * dev/ino of a file to be rejected.  Used to prevent adding
 * an archive to itself recursively.
 */
int
archive_write_set_skip_file(struct archive *a, dev_t d, ino_t i)
{
	__archive_check_magic(a, ARCHIVE_WRITE_MAGIC, ARCHIVE_STATE_ANY, "archive_write_set_skip_file");
	a->skip_file_dev = d;
	a->skip_file_ino = i;
	return (ARCHIVE_OK);
}


/*
 * Open the archive using the current settings.
 */
int
archive_write_open(struct archive *a, void *client_data,
    archive_open_callback *opener, archive_write_callback *writer,
    archive_close_callback *closer)
{
	int ret;

	ret = ARCHIVE_OK;
	__archive_check_magic(a, ARCHIVE_WRITE_MAGIC, ARCHIVE_STATE_NEW, "archive_write_open");
	archive_string_empty(&a->error_string);
	a->state = ARCHIVE_STATE_HEADER;
	a->client_data = client_data;
	a->client_writer = writer;
	a->client_opener = opener;
	a->client_closer = closer;
	ret = (a->compression_init)(a);
	if (a->format_init && ret == ARCHIVE_OK)
		ret = (a->format_init)(a);
	return (ret);
}


/*
 * Close out the archive.
 *
 * Be careful: user might just call write_new and then write_finish.
 * Don't assume we actually wrote anything or performed any non-trivial
 * initialization.
 */
int
archive_write_close(struct archive *a)
{
	int r = ARCHIVE_OK, r1 = ARCHIVE_OK;

	__archive_check_magic(a, ARCHIVE_WRITE_MAGIC, ARCHIVE_STATE_ANY, "archive_write_close");

	/* Finish the last entry. */
	if (a->state & ARCHIVE_STATE_DATA)
		r = ((a->format_finish_entry)(a));

	/* Finish off the archive. */
	if (a->format_finish != NULL) {
		r1 = (a->format_finish)(a);
		if (r1 < r)
			r = r1;
	}

	/* Finish the compression and close the stream. */
	if (a->compression_finish != NULL) {
		r1 = (a->compression_finish)(a);
		if (r1 < r)
			r = r1;
	}

	a->state = ARCHIVE_STATE_CLOSED;
	return (r);
}

/*
 * Destroy the archive structure.
 */
void
archive_write_finish(struct archive *a)
{
	__archive_check_magic(a, ARCHIVE_WRITE_MAGIC, ARCHIVE_STATE_ANY, "archive_write_finish");
	if (a->state != ARCHIVE_STATE_CLOSED)
		archive_write_close(a);

	/* Release various dynamic buffers. */
	free((void *)(uintptr_t)(const void *)a->nulls);
	archive_string_free(&a->error_string);
	a->magic = 0;
	free(a);
}

/*
 * Write the appropriate header.
 */
int
archive_write_header(struct archive *a, struct archive_entry *entry)
{
	int ret, r2;

	__archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_DATA | ARCHIVE_STATE_HEADER, "archive_write_header");
	archive_string_empty(&a->error_string);

	/* In particular, "retry" and "fatal" get returned immediately. */
	ret = archive_write_finish_entry(a);
	if (ret < ARCHIVE_OK && ret != ARCHIVE_WARN)
		return (ret);

	if (a->skip_file_dev != 0 &&
	    archive_entry_dev(entry) == a->skip_file_dev &&
	    a->skip_file_ino != 0 &&
	    archive_entry_ino(entry) == a->skip_file_ino) {
		archive_set_error(a, 0, "Can't add archive to itself");
		return (ARCHIVE_WARN);
	}

	/* Format and write header. */
	r2 = ((a->format_write_header)(a, entry));
	if (r2 < ret)
		ret = r2;

	a->state = ARCHIVE_STATE_DATA;
	return (ret);
}

int
archive_write_finish_entry(struct archive * a)
{
	int ret = ARCHIVE_OK;

	__archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_HEADER | ARCHIVE_STATE_DATA,
	    "archive_write_finish_entry");
	if (a->state & ARCHIVE_STATE_DATA)
		ret = (a->format_finish_entry)(a);
	a->state = ARCHIVE_STATE_HEADER;
	return (ret);
}

/*
 * Note that the compressor is responsible for blocking.
 */
/* Should be "ssize_t", but that breaks the ABI.  <sigh> */
int
archive_write_data(struct archive *a, const void *buff, size_t s)
{
	__archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_DATA, "archive_write_data");
	archive_string_empty(&a->error_string);
	return ((a->format_write_data)(a, buff, s));
}
