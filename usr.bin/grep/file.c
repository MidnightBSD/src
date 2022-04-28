/*	$NetBSD: file.c,v 1.5 2011/02/16 18:35:39 joerg Exp $	*/
/*	$FreeBSD$	*/
/*	$OpenBSD: file.c,v 1.11 2010/07/02 20:48:48 nicm Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 1999 James Howard and Dag-Erling Coïdan Smørgrav
 * Copyright (C) 2008-2010 Gabor Kovesdan <gabor@FreeBSD.org>
 * Copyright (C) 2010 Dimitry Andric <dimitry@andric.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <zlib.h>

#ifndef WITHOUT_LZMA
#include <lzma.h>
#endif

#ifndef WITHOUT_BZIP2
#include <bzlib.h>
#endif

#include "grep.h"

#define	MAXBUFSIZ	(32 * 1024)
#define	LNBUFBUMP	80

static gzFile gzbufdesc;
#ifndef WITHOUT_LZMA
static lzma_stream lstrm = LZMA_STREAM_INIT;
static lzma_action laction;
static uint8_t lin_buf[MAXBUFSIZ];
#endif
#ifndef WITHOUT_BZIP2
static BZFILE* bzbufdesc;
#endif

static unsigned char *buffer;
static unsigned char *bufpos;
static size_t bufrem;
static size_t fsiz;

static unsigned char *lnbuf;
static size_t lnbuflen;

static inline int
grep_refill(struct file *f)
{
	ssize_t nr;
#ifndef WITHOUT_LZMA
	lzma_ret lzmaret;
#endif

	if (filebehave == FILE_MMAP)
		return (0);

	bufpos = buffer;
	bufrem = 0;

	switch (filebehave) {
	case FILE_GZIP:
		nr = gzread(gzbufdesc, buffer, MAXBUFSIZ);
		break;
#ifndef WITHOUT_BZIP2
	case FILE_BZIP:
		if (bzbufdesc != NULL) {
			int bzerr;

			nr = BZ2_bzRead(&bzerr, bzbufdesc, buffer, MAXBUFSIZ);
			switch (bzerr) {
			case BZ_OK:
			case BZ_STREAM_END:
				/* No problem, nr will be okay */
				break;
			case BZ_DATA_ERROR_MAGIC:
				/*
				 * As opposed to gzread(), which simply returns the
				 * plain file data, if it is not in the correct
				 * compressed format, BZ2_bzRead() instead aborts.
				 *
				 * So, just restart at the beginning of the file again,
				 * and use plain reads from now on.
				 */
				BZ2_bzReadClose(&bzerr, bzbufdesc);
				bzbufdesc = NULL;
				if (lseek(f->fd, 0, SEEK_SET) == -1)
					return (-1);
				nr = read(f->fd, buffer, MAXBUFSIZ);
				break;
			default:
				/* Make sure we exit with an error */
				nr = -1;
			}
		} else
			/*
			 * Also an error case; we should never have a scenario
			 * where we have an open file but no bzip descriptor
			 * at this point. See: grep_open
			 */
			nr = -1;
		break;
#endif
#ifndef WITHOUT_LZMA
	case FILE_XZ:
	case FILE_LZMA:
		lstrm.next_out = buffer;

		do {
			if (lstrm.avail_in == 0) {
				lstrm.next_in = lin_buf;
				nr = read(f->fd, lin_buf, MAXBUFSIZ);

				if (nr < 0)
					return (-1);
				else if (nr == 0)
					laction = LZMA_FINISH;

				lstrm.avail_in = nr;
			}

			lzmaret = lzma_code(&lstrm, laction);

			if (lzmaret != LZMA_OK && lzmaret != LZMA_STREAM_END)
				return (-1);

			if (lstrm.avail_out == 0 || lzmaret == LZMA_STREAM_END) {
				bufrem = MAXBUFSIZ - lstrm.avail_out;
				lstrm.next_out = buffer;
				lstrm.avail_out = MAXBUFSIZ;
			}
		} while (bufrem == 0 && lzmaret != LZMA_STREAM_END);

		return (0);
#endif	/* WITHOUT_LZMA */
	default:
		nr = read(f->fd, buffer, MAXBUFSIZ);
	}
	if (nr < 0)
		return (-1);

	bufrem = nr;
	return (0);
}

static inline int
grep_lnbufgrow(size_t newlen)
{

	if (lnbuflen < newlen) {
		lnbuf = grep_realloc(lnbuf, newlen);
		lnbuflen = newlen;
	}

	return (0);
}

char *
grep_fgetln(struct file *f, struct parsec *pc)
{
	unsigned char *p;
	char *ret;
	size_t len;
	size_t off;
	ptrdiff_t diff;

	/* Fill the buffer, if necessary */
	if (bufrem == 0 && grep_refill(f) != 0)
		goto error;

	if (bufrem == 0) {
		/* Return zero length to indicate EOF */
		pc->ln.len= 0;
		return (bufpos);
	}

	/* Look for a newline in the remaining part of the buffer */
	if ((p = memchr(bufpos, fileeol, bufrem)) != NULL) {
		++p; /* advance over newline */
		ret = bufpos;
		len = p - bufpos;
		bufrem -= len;
		bufpos = p;
		pc->ln.len = len;
		return (ret);
	}

	/* We have to copy the current buffered data to the line buffer */
	for (len = bufrem, off = 0; ; len += bufrem) {
		/* Make sure there is room for more data */
		if (grep_lnbufgrow(len + LNBUFBUMP))
			goto error;
		memcpy(lnbuf + off, bufpos, len - off);
		/* With FILE_MMAP, this is EOF; there's no more to refill */
		if (filebehave == FILE_MMAP) {
			bufrem -= len;
			break;
		}
		off = len;
		/* Fetch more to try and find EOL/EOF */
		if (grep_refill(f) != 0)
			goto error;
		if (bufrem == 0)
			/* EOF: return partial line */
			break;
		if ((p = memchr(bufpos, fileeol, bufrem)) == NULL)
			continue;
		/* got it: finish up the line (like code above) */
		++p;
		diff = p - bufpos;
		len += diff;
		if (grep_lnbufgrow(len))
		    goto error;
		memcpy(lnbuf + off, bufpos, diff);
		bufrem -= diff;
		bufpos = p;
		break;
	}
	pc->ln.len = len;
	return (lnbuf);

error:
	pc->ln.len = 0;
	return (NULL);
}

/*
 * Opens a file for processing.
 */
struct file *
grep_open(const char *path)
{
	struct file *f;
#ifndef WITHOUT_LZMA
	lzma_ret lzmaret;
#endif

	f = grep_malloc(sizeof *f);
	memset(f, 0, sizeof *f);
	if (path == NULL) {
		/* Processing stdin implies --line-buffered. */
		lbflag = true;
		f->fd = STDIN_FILENO;
	} else if ((f->fd = open(path, O_RDONLY)) == -1)
		goto error1;

	if (filebehave == FILE_MMAP) {
		struct stat st;

		if ((fstat(f->fd, &st) == -1) || (st.st_size > OFF_MAX) ||
		    (!S_ISREG(st.st_mode)))
			filebehave = FILE_STDIO;
		else {
			int flags = MAP_PRIVATE | MAP_NOCORE | MAP_NOSYNC;
#ifdef MAP_PREFAULT_READ
			flags |= MAP_PREFAULT_READ;
#endif
			fsiz = st.st_size;
			buffer = mmap(NULL, fsiz, PROT_READ, flags,
			     f->fd, (off_t)0);
			if (buffer == MAP_FAILED)
				filebehave = FILE_STDIO;
			else {
				bufrem = st.st_size;
				bufpos = buffer;
				madvise(buffer, st.st_size, MADV_SEQUENTIAL);
			}
		}
	}

	if ((buffer == NULL) || (buffer == MAP_FAILED))
		buffer = grep_malloc(MAXBUFSIZ);

	switch (filebehave) {
	case FILE_GZIP:
		if ((gzbufdesc = gzdopen(f->fd, "r")) == NULL)
			goto error2;
		break;
#ifndef WITHOUT_BZIP2
	case FILE_BZIP:
		if ((bzbufdesc = BZ2_bzdopen(f->fd, "r")) == NULL)
			goto error2;
		break;
#endif
#ifndef WITHOUT_LZMA
	case FILE_XZ:
	case FILE_LZMA:

		if (filebehave == FILE_XZ)
			lzmaret = lzma_stream_decoder(&lstrm, UINT64_MAX,
			    LZMA_CONCATENATED);
		else
			lzmaret = lzma_alone_decoder(&lstrm, UINT64_MAX);

		if (lzmaret != LZMA_OK)
			goto error2;

		lstrm.avail_in = 0;
		lstrm.avail_out = MAXBUFSIZ;
		laction = LZMA_RUN;
		break;
#endif
	}

	/* Fill read buffer, also catches errors early */
	if (bufrem == 0 && grep_refill(f) != 0)
		goto error2;

	/* Check for binary stuff, if necessary */
	if (binbehave != BINFILE_TEXT && fileeol != '\0' &&
	    memchr(bufpos, '\0', bufrem) != NULL)
		f->binary = true;

	return (f);

error2:
	close(f->fd);
error1:
	free(f);
	return (NULL);
}

/*
 * Closes a file.
 */
void
grep_close(struct file *f)
{

	close(f->fd);

	/* Reset read buffer and line buffer */
	if (filebehave == FILE_MMAP) {
		munmap(buffer, fsiz);
		buffer = NULL;
	}
	bufpos = buffer;
	bufrem = 0;

	free(lnbuf);
	lnbuf = NULL;
	lnbuflen = 0;
}
