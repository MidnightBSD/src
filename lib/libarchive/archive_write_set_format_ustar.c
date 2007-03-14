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
__FBSDID("$FreeBSD: src/lib/libarchive/archive_write_set_format_ustar.c,v 1.12.2.2 2007/01/27 06:44:53 kientzle Exp $");

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#else
#ifdef MAJOR_IN_SYSMACROS
#include <sys/sysmacros.h>
#endif
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"

struct ustar {
	uint64_t	entry_bytes_remaining;
	uint64_t	entry_padding;
	char		written;
};

/*
 * Define structure of POSIX 'ustar' tar header.
 */
#define	USTAR_name_offset 0
#define	USTAR_name_size 100
#define	USTAR_mode_offset 100
#define	USTAR_mode_size 6
#define	USTAR_mode_max_size 8
#define	USTAR_uid_offset 108
#define	USTAR_uid_size 6
#define	USTAR_uid_max_size 8
#define	USTAR_gid_offset 116
#define	USTAR_gid_size 6
#define	USTAR_gid_max_size 8
#define	USTAR_size_offset 124
#define	USTAR_size_size 11
#define	USTAR_size_max_size 12
#define	USTAR_mtime_offset 136
#define	USTAR_mtime_size 11
#define	USTAR_mtime_max_size 11
#define	USTAR_checksum_offset 148
#define	USTAR_checksum_size 8
#define	USTAR_typeflag_offset 156
#define	USTAR_typeflag_size 1
#define	USTAR_linkname_offset 157
#define	USTAR_linkname_size 100
#define	USTAR_magic_offset 257
#define	USTAR_magic_size 6
#define	USTAR_version_offset 263
#define	USTAR_version_size 2
#define	USTAR_uname_offset 265
#define	USTAR_uname_size 32
#define	USTAR_gname_offset 297
#define	USTAR_gname_size 32
#define	USTAR_rdevmajor_offset 329
#define	USTAR_rdevmajor_size 6
#define	USTAR_rdevmajor_max_size 8
#define	USTAR_rdevminor_offset 337
#define	USTAR_rdevminor_size 6
#define	USTAR_rdevminor_max_size 8
#define	USTAR_prefix_offset 345
#define	USTAR_prefix_size 155
#define	USTAR_padding_offset 500
#define	USTAR_padding_size 12

/*
 * A filled-in copy of the header for initialization.
 */
static const char template_header[] = {
	/* name: 100 bytes */
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,
	/* Mode, space-null termination: 8 bytes */
	'0','0','0','0','0','0', ' ','\0',
	/* uid, space-null termination: 8 bytes */
	'0','0','0','0','0','0', ' ','\0',
	/* gid, space-null termination: 8 bytes */
	'0','0','0','0','0','0', ' ','\0',
	/* size, space termation: 12 bytes */
	'0','0','0','0','0','0','0','0','0','0','0', ' ',
	/* mtime, space termation: 12 bytes */
	'0','0','0','0','0','0','0','0','0','0','0', ' ',
	/* Initial checksum value: 8 spaces */
	' ',' ',' ',' ',' ',' ',' ',' ',
	/* Typeflag: 1 byte */
	'0',			/* '0' = regular file */
	/* Linkname: 100 bytes */
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,
	/* Magic: 6 bytes, Version: 2 bytes */
	'u','s','t','a','r','\0', '0','0',
	/* Uname: 32 bytes */
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	/* Gname: 32 bytes */
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	/* rdevmajor + space/null padding: 8 bytes */
	'0','0','0','0','0','0', ' ','\0',
	/* rdevminor + space/null padding: 8 bytes */
	'0','0','0','0','0','0', ' ','\0',
	/* Prefix: 155 bytes */
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,
	/* Padding: 12 bytes */
	0,0,0,0,0,0,0,0, 0,0,0,0
};

static ssize_t	archive_write_ustar_data(struct archive *a, const void *buff,
		    size_t s);
static int	archive_write_ustar_finish(struct archive *);
static int	archive_write_ustar_finish_entry(struct archive *);
static int	archive_write_ustar_header(struct archive *,
		    struct archive_entry *entry);
static int	format_256(int64_t, char *, int);
static int	format_number(int64_t, char *, int size, int max, int strict);
static int	format_octal(int64_t, char *, int);
static int	write_nulls(struct archive *a, size_t);

/*
 * Set output format to 'ustar' format.
 */
int
archive_write_set_format_ustar(struct archive *a)
{
	struct ustar *ustar;

	/* If someone else was already registered, unregister them. */
	if (a->format_finish != NULL)
		(a->format_finish)(a);

	/* Basic internal sanity test. */
	if (sizeof(template_header) != 512) {
		archive_set_error(a, ARCHIVE_ERRNO_MISC, "Internal: template_header wrong size: %d should be 512", sizeof(template_header));
		return (ARCHIVE_FATAL);
	}

	ustar = (struct ustar *)malloc(sizeof(*ustar));
	if (ustar == NULL) {
		archive_set_error(a, ENOMEM, "Can't allocate ustar data");
		return (ARCHIVE_FATAL);
	}
	memset(ustar, 0, sizeof(*ustar));
	a->format_data = ustar;

	a->pad_uncompressed = 1;	/* Mimic gtar in this respect. */
	a->format_write_header = archive_write_ustar_header;
	a->format_write_data = archive_write_ustar_data;
	a->format_finish = archive_write_ustar_finish;
	a->format_finish_entry = archive_write_ustar_finish_entry;
	a->archive_format = ARCHIVE_FORMAT_TAR_USTAR;
	a->archive_format_name = "POSIX ustar";
	return (ARCHIVE_OK);
}

static int
archive_write_ustar_header(struct archive *a, struct archive_entry *entry)
{
	char buff[512];
	int ret;
	struct ustar *ustar;

	ustar = (struct ustar *)a->format_data;
	ustar->written = 1;

	/* Only regular files (not hardlinks) have data. */
	if (archive_entry_hardlink(entry) != NULL ||
	    archive_entry_symlink(entry) != NULL ||
	    !S_ISREG(archive_entry_mode(entry)))
		archive_entry_set_size(entry, 0);

	ret = __archive_write_format_header_ustar(a, buff, entry, -1, 1);
	if (ret != ARCHIVE_OK)
		return (ret);
	ret = (a->compression_write)(a, buff, 512);
	if (ret != ARCHIVE_OK)
		return (ret);

	ustar->entry_bytes_remaining = archive_entry_size(entry);
	ustar->entry_padding = 0x1ff & (-(int64_t)ustar->entry_bytes_remaining);
	return (ARCHIVE_OK);
}

/*
 * Format a basic 512-byte "ustar" header.
 *
 * Returns -1 if format failed (due to field overflow).
 * Note that this always formats as much of the header as possible.
 * If "strict" is set to zero, it will extend numeric fields as
 * necessary (overwriting terminators or using base-256 extensions).
 *
 * This is exported so that other 'tar' formats can use it.
 */
int
__archive_write_format_header_ustar(struct archive *a, char h[512],
    struct archive_entry *entry, int tartype, int strict)
{
	unsigned int checksum;
	int i, ret;
	size_t copy_length;
	const char *p, *pp;
	const struct stat *st;
	int mytartype;

	ret = 0;
	mytartype = -1;
	/*
	 * The "template header" already includes the "ustar"
	 * signature, various end-of-field markers and other required
	 * elements.
	 */
	memcpy(h, &template_header, 512);

	/*
	 * Because the block is already null-filled, and strings
	 * are allowed to exactly fill their destination (without null),
	 * I use memcpy(dest, src, strlen()) here a lot to copy strings.
	 */

	pp = archive_entry_pathname(entry);
	if (strlen(pp) <= USTAR_name_size)
		memcpy(h + USTAR_name_offset, pp, strlen(pp));
	else {
		/* Store in two pieces, splitting at a '/'. */
		p = strchr(pp + strlen(pp) - USTAR_name_size - 1, '/');
		/*
		 * If there is no path separator, or the prefix or
		 * remaining name are too large, return an error.
		 */
		if (!p) {
			archive_set_error(a, ENAMETOOLONG,
			    "Pathname too long");
			ret = ARCHIVE_WARN;
		} else if (p  > pp + USTAR_prefix_size) {
			archive_set_error(a, ENAMETOOLONG,
			    "Pathname too long");
			ret = ARCHIVE_WARN;
		} else {
			/* Copy prefix and remainder to appropriate places */
			memcpy(h + USTAR_prefix_offset, pp, p - pp);
			memcpy(h + USTAR_name_offset, p + 1, pp + strlen(pp) - p - 1);
		}
	}

	p = archive_entry_hardlink(entry);
	if (p != NULL)
		mytartype = '1';
	else
		p = archive_entry_symlink(entry);
	if (p != NULL && p[0] != '\0') {
		copy_length = strlen(p);
		if (copy_length > USTAR_linkname_size) {
			archive_set_error(a, ENAMETOOLONG,
			    "Link contents too long");
			ret = ARCHIVE_WARN;
			copy_length = USTAR_linkname_size;
		}
		memcpy(h + USTAR_linkname_offset, p, copy_length);
	}

	p = archive_entry_uname(entry);
	if (p != NULL && p[0] != '\0') {
		copy_length = strlen(p);
		if (copy_length > USTAR_uname_size) {
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "Username too long");
			ret = ARCHIVE_WARN;
			copy_length = USTAR_uname_size;
		}
		memcpy(h + USTAR_uname_offset, p, copy_length);
	}

	p = archive_entry_gname(entry);
	if (p != NULL && p[0] != '\0') {
		copy_length = strlen(p);
		if (strlen(p) > USTAR_gname_size) {
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "Group name too long");
			ret = ARCHIVE_WARN;
			copy_length = USTAR_gname_size;
		}
		memcpy(h + USTAR_gname_offset, p, copy_length);
	}

	st = archive_entry_stat(entry);

	if (format_number(st->st_mode & 07777, h + USTAR_mode_offset, USTAR_mode_size, USTAR_mode_max_size, strict)) {
		archive_set_error(a, ERANGE, "Numeric mode too large");
		ret = ARCHIVE_WARN;
	}

	if (format_number(st->st_uid, h + USTAR_uid_offset, USTAR_uid_size, USTAR_uid_max_size, strict)) {
		archive_set_error(a, ERANGE, "Numeric user ID too large");
		ret = ARCHIVE_WARN;
	}

	if (format_number(st->st_gid, h + USTAR_gid_offset, USTAR_gid_size, USTAR_gid_max_size, strict)) {
		archive_set_error(a, ERANGE, "Numeric group ID too large");
		ret = ARCHIVE_WARN;
	}

	if (format_number(st->st_size, h + USTAR_size_offset, USTAR_size_size, USTAR_size_max_size, strict)) {
		archive_set_error(a, ERANGE, "File size out of range");
		ret = ARCHIVE_WARN;
	}

	if (format_number(st->st_mtime, h + USTAR_mtime_offset, USTAR_mtime_size, USTAR_mtime_max_size, strict)) {
		archive_set_error(a, ERANGE,
		    "File modification time too large");
		ret = ARCHIVE_WARN;
	}

	if (S_ISBLK(st->st_mode) || S_ISCHR(st->st_mode)) {
		if (format_number(major(st->st_rdev), h + USTAR_rdevmajor_offset,
			USTAR_rdevmajor_size, USTAR_rdevmajor_max_size, strict)) {
			archive_set_error(a, ERANGE,
			    "Major device number too large");
			ret = ARCHIVE_WARN;
		}

		if (format_number(minor(st->st_rdev), h + USTAR_rdevminor_offset,
			USTAR_rdevminor_size, USTAR_rdevminor_max_size, strict)) {
			archive_set_error(a, ERANGE,
			    "Minor device number too large");
			ret = ARCHIVE_WARN;
		}
	}

	if (tartype >= 0) {
		h[USTAR_typeflag_offset] = tartype;
	} else if (mytartype >= 0) {
		h[USTAR_typeflag_offset] = mytartype;
	} else {
		switch (st->st_mode & S_IFMT) {
		case S_IFREG: h[USTAR_typeflag_offset] = '0' ; break;
		case S_IFLNK: h[USTAR_typeflag_offset] = '2' ; break;
		case S_IFCHR: h[USTAR_typeflag_offset] = '3' ; break;
		case S_IFBLK: h[USTAR_typeflag_offset] = '4' ; break;
		case S_IFDIR: h[USTAR_typeflag_offset] = '5' ; break;
		case S_IFIFO: h[USTAR_typeflag_offset] = '6' ; break;
		case S_IFSOCK:
			archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
			    "tar format cannot archive socket");
			ret = ARCHIVE_WARN;
			break;
		default:
			archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
			    "tar format cannot archive this (mode=0%lo)",
			    (unsigned long)st->st_mode);
			ret = ARCHIVE_WARN;
		}
	}

	checksum = 0;
	for (i = 0; i < 512; i++)
		checksum += 255 & (unsigned int)h[i];
	h[USTAR_checksum_offset + 6] = '\0'; /* Can't be pre-set in the template. */
	/* h[USTAR_checksum_offset + 7] = ' '; */ /* This is pre-set in the template. */
	format_octal(checksum, h + USTAR_checksum_offset, 6);
	return (ret);
}

/*
 * Format a number into a field, with some intelligence.
 */
static int
format_number(int64_t v, char *p, int s, int maxsize, int strict)
{
	int64_t limit;

	limit = ((int64_t)1 << (s*3));

	/* "Strict" only permits octal values with proper termination. */
	if (strict)
		return (format_octal(v, p, s));

	/*
	 * In non-strict mode, we allow the number to overwrite one or
	 * more bytes of the field termination.  Even old tar
	 * implementations should be able to handle this with no
	 * problem.
	 */
	if (v >= 0) {
		while (s <= maxsize) {
			if (v < limit)
				return (format_octal(v, p, s));
			s++;
			limit <<= 3;
		}
	}

	/* Base-256 can handle any number, positive or negative. */
	return (format_256(v, p, maxsize));
}

/*
 * Format a number into the specified field using base-256.
 */
static int
format_256(int64_t v, char *p, int s)
{
	p += s;
	while (s-- > 0) {
		*--p = (char)(v & 0xff);
		v >>= 8;
	}
	*p |= 0x80; /* Set the base-256 marker bit. */
	return (0);
}

/*
 * Format a number into the specified field.
 */
static int
format_octal(int64_t v, char *p, int s)
{
	int len;

	len = s;

	/* Octal values can't be negative, so use 0. */
	if (v < 0) {
		while (len-- > 0)
			*p++ = '0';
		return (-1);
	}

	p += s;		/* Start at the end and work backwards. */
	while (s-- > 0) {
		*--p = (char)('0' + (v & 7));
		v >>= 3;
	}

	if (v == 0)
		return (0);

	/* If it overflowed, fill field with max value. */
	while (len-- > 0)
		*p++ = '7';

	return (-1);
}

static int
archive_write_ustar_finish(struct archive *a)
{
	struct ustar *ustar;
	int r;

	r = ARCHIVE_OK;
	ustar = (struct ustar *)a->format_data;
	/*
	 * Suppress end-of-archive if nothing else was ever written.
	 * This fixes a problem where setting one format, then another
	 * ends up writing a gratuitous end-of-archive marker.
	 */
	if (ustar->written && a->compression_write != NULL)
		r = write_nulls(a, 512*2);
	free(ustar);
	a->format_data = NULL;
	return (r);
}

static int
archive_write_ustar_finish_entry(struct archive *a)
{
	struct ustar *ustar;
	int ret;

	ustar = (struct ustar *)a->format_data;
	ret = write_nulls(a,
	    ustar->entry_bytes_remaining + ustar->entry_padding);
	ustar->entry_bytes_remaining = ustar->entry_padding = 0;
	return (ret);
}

static int
write_nulls(struct archive *a, size_t padding)
{
	int ret, to_write;

	while (padding > 0) {
		to_write = padding < a->null_length ? padding : a->null_length;
		ret = (a->compression_write)(a, a->nulls, to_write);
		if (ret != ARCHIVE_OK)
			return (ret);
		padding -= to_write;
	}
	return (ARCHIVE_OK);
}

static ssize_t
archive_write_ustar_data(struct archive *a, const void *buff, size_t s)
{
	struct ustar *ustar;
	int ret;

	ustar = (struct ustar *)a->format_data;
	if (s > ustar->entry_bytes_remaining)
		s = ustar->entry_bytes_remaining;
	ret = (a->compression_write)(a, buff, s);
	ustar->entry_bytes_remaining -= s;
	if (ret != ARCHIVE_OK)
		return (ret);
	return (s);
}
