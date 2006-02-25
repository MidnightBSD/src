/*-
 * Copyright (c) 2003-2004 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
__FBSDID("$FreeBSD: src/lib/libarchive/archive_write_open_fd.c,v 1.4 2004/10/17 23:47:30 kientzle Exp $");

#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "archive.h"
#include "archive_private.h"

struct write_fd_data {
	off_t		offset;
	int		fd;
};

static int	file_close(struct archive *, void *);
static int	file_open(struct archive *, void *);
static ssize_t	file_write(struct archive *, void *, void *buff, size_t);

int
archive_write_open_fd(struct archive *a, int fd)
{
	struct write_fd_data *mine;

	mine = malloc(sizeof(*mine));
	if (mine == NULL) {
		archive_set_error(a, ENOMEM, "No memory");
		return (ARCHIVE_FATAL);
	}
	mine->fd = fd;
	return (archive_write_open(a, mine,
		    file_open, file_write, file_close));
}

static int
file_open(struct archive *a, void *client_data)
{
	struct write_fd_data *mine;
	struct stat st, *pst;

	pst = NULL;
	mine = client_data;

	/*
	 * If client hasn't explicitly set the last block handling,
	 * then set it here: If the output is a block or character
	 * device, pad the last block, otherwise leave it unpadded.
	 */
	if (mine->fd >= 0 && a->bytes_in_last_block < 0) {
		/* Last block will be fully padded. */
		if (fstat(mine->fd, &st) == 0) {
			pst = &st;
			if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode) ||
			    S_ISFIFO(st.st_mode))
				archive_write_set_bytes_in_last_block(a, 0);
			else
				archive_write_set_bytes_in_last_block(a, 1);
		}
	}

	if (mine->fd == 1) {
		if (a->bytes_in_last_block < 0) /* Still default? */
			/* Last block will be fully padded. */
			archive_write_set_bytes_in_last_block(a, 0);
	}

	if (mine->fd < 0) {
		archive_set_error(a, errno, "Failed to open");
		return (ARCHIVE_FATAL);
	}

	if (pst == NULL && fstat(mine->fd, &st) == 0)
		pst = &st;
	if (pst == NULL) {
		archive_set_error(a, errno, "Couldn't stat fd %d", mine->fd);
		return (ARCHIVE_FATAL);
	}

	return (ARCHIVE_OK);
}

static ssize_t
file_write(struct archive *a, void *client_data, void *buff, size_t length)
{
	struct write_fd_data	*mine;
	ssize_t	bytesWritten;

	mine = client_data;
	bytesWritten = write(mine->fd, buff, length);
	if (bytesWritten <= 0) {
		archive_set_error(a, errno, "Write error");
		return (-1);
	}
	return (bytesWritten);
}

static int
file_close(struct archive *a, void *client_data)
{
	struct write_fd_data	*mine = client_data;

	(void)a; /* UNUSED */
	free(mine);
	return (ARCHIVE_OK);
}
