/*
 * Copyright (c) Ian F. Darwin 1986-1995.
 * Software written by Ian F. Darwin and others;
 * maintained 1995-present by Christos Zoulas and others.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * compress routines:
 *	zmagic() - returns 0 if not recognized, uncompresses and prints
 *		   information if recognized
 *	uncompress(method, old, n, newch) - uncompress old into new, 
 *					    using method, return sizeof new
 */
#include "file.h"
#include "magic.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_LIBZ
#include <zlib.h>
#endif

#ifndef lint
FILE_RCSID("@(#)$Id: compress.c,v 1.1.1.2 2006-02-25 02:32:35 laffer1 Exp $")
#endif


private struct {
	const char *magic;
	size_t maglen;
	const char *const argv[3];
	int silent;
} compr[] = {
	{ "\037\235", 2, { "gzip", "-cdq", NULL }, 1 },		/* compressed */
	/* Uncompress can get stuck; so use gzip first if we have it
	 * Idea from Damien Clark, thanks! */
	{ "\037\235", 2, { "uncompress", "-c", NULL }, 1 },	/* compressed */
	{ "\037\213", 2, { "gzip", "-cdq", NULL }, 1 },		/* gzipped */
	{ "\037\236", 2, { "gzip", "-cdq", NULL }, 1 },		/* frozen */
	{ "\037\240", 2, { "gzip", "-cdq", NULL }, 1 },		/* SCO LZH */
	/* the standard pack utilities do not accept standard input */
	{ "\037\036", 2, { "gzip", "-cdq", NULL }, 0 },		/* packed */
	{ "BZh",      3, { "bzip2", "-cd", NULL }, 1 },		/* bzip2-ed */
};

private int ncompr = sizeof(compr) / sizeof(compr[0]);


private ssize_t swrite(int, const void *, size_t);
private ssize_t sread(int, void *, size_t);
private size_t uncompressbuf(struct magic_set *, size_t, const unsigned char *,
    unsigned char **, size_t);
#ifdef HAVE_LIBZ
private size_t uncompressgzipped(struct magic_set *, const unsigned char *,
    unsigned char **, size_t);
#endif

protected int
file_zmagic(struct magic_set *ms, const unsigned char *buf, size_t nbytes)
{
	unsigned char *newbuf = NULL;
	size_t i, nsz;
	int rv = 0;

	if ((ms->flags & MAGIC_COMPRESS) == 0)
		return 0;

	for (i = 0; i < ncompr; i++) {
		if (nbytes < compr[i].maglen)
			continue;
		if (memcmp(buf, compr[i].magic, compr[i].maglen) == 0 &&
		    (nsz = uncompressbuf(ms, i, buf, &newbuf, nbytes)) != 0) {
			ms->flags &= ~MAGIC_COMPRESS;
			rv = -1;
			if (file_buffer(ms, newbuf, nsz) == -1)
				goto error;
			if (file_printf(ms, " (") == -1)
				goto error;
			if (file_buffer(ms, buf, nbytes) == -1)
				goto error;
			if (file_printf(ms, ")") == -1)
				goto error;
			rv = 1;
			break;
		}
	}
error:
	if (newbuf)
		free(newbuf);
	ms->flags |= MAGIC_COMPRESS;
	return rv;
}

/*
 * `safe' write for sockets and pipes.
 */
private ssize_t
swrite(int fd, const void *buf, size_t n)
{
	int rv;
	size_t rn = n;

	do
		switch (rv = write(fd, buf, n)) {
		case -1:
			if (errno == EINTR)
				continue;
			return -1;
		default:
			n -= rv;
			buf = ((const char *)buf) + rv;
			break;
		}
	while (n > 0);
	return rn;
}


/*
 * `safe' read for sockets and pipes.
 */
private ssize_t
sread(int fd, void *buf, size_t n)
{
	int rv;
	size_t rn = n;

	do
		switch (rv = read(fd, buf, n)) {
		case -1:
			if (errno == EINTR)
				continue;
			return -1;
		case 0:
			return rn - n;
		default:
			n -= rv;
			buf = ((char *)buf) + rv;
			break;
		}
	while (n > 0);
	return rn;
}

protected int
file_pipe2file(struct magic_set *ms, int fd, const void *startbuf,
    size_t nbytes)
{
	char buf[4096];
	int r, tfd;

	(void)strcpy(buf, "/tmp/file.XXXXXX");
#ifndef HAVE_MKSTEMP
	{
		char *ptr = mktemp(buf);
		tfd = open(ptr, O_RDWR|O_TRUNC|O_EXCL|O_CREAT, 0600);
		r = errno;
		(void)unlink(ptr);
		errno = r;
	}
#else
	tfd = mkstemp(buf);
	r = errno;
	(void)unlink(buf);
	errno = r;
#endif
	if (tfd == -1) {
		file_error(ms, errno,
		    "cannot create temporary file for pipe copy");
		return -1;
	}

	if (swrite(tfd, startbuf, nbytes) != (ssize_t)nbytes)
		r = 1;
	else {
		while ((r = sread(fd, buf, sizeof(buf))) > 0)
			if (swrite(tfd, buf, (size_t)r) != r)
				break;
	}

	switch (r) {
	case -1:
		file_error(ms, errno, "error copying from pipe to temp file");
		return -1;
	case 0:
		break;
	default:
		file_error(ms, errno, "error while writing to temp file");
		return -1;
	}

	/*
	 * We duplicate the file descriptor, because fclose on a
	 * tmpfile will delete the file, but any open descriptors
	 * can still access the phantom inode.
	 */
	if ((fd = dup2(tfd, fd)) == -1) {
		file_error(ms, errno, "could not dup descriptor for temp file");
		return -1;
	}
	(void)close(tfd);
	if (lseek(fd, (off_t)0, SEEK_SET) == (off_t)-1) {
		file_badseek(ms);
		return -1;
	}
	return fd;
}

#ifdef HAVE_LIBZ

#define FHCRC		(1 << 1)
#define FEXTRA		(1 << 2)
#define FNAME		(1 << 3)
#define FCOMMENT	(1 << 4)

private size_t
uncompressgzipped(struct magic_set *ms, const unsigned char *old,
    unsigned char **newch, size_t n)
{
	unsigned char flg = old[3];
	size_t data_start = 10;
	z_stream z;
	int rc;

	if (flg & FEXTRA) {
		if (data_start+1 >= n)
			return 0;
		data_start += 2 + old[data_start] + old[data_start + 1] * 256;
	}
	if (flg & FNAME) {
		while(data_start < n && old[data_start])
			data_start++;
		data_start++;
	}
	if(flg & FCOMMENT) {
		while(data_start < n && old[data_start])
			data_start++;
		data_start++;
	}
	if(flg & FHCRC)
		data_start += 2;

	if (data_start >= n)
		return 0;
	if ((*newch = (unsigned char *)malloc(HOWMANY + 1)) == NULL) {
		return 0;
	}
	
	/* XXX: const castaway, via strchr */
	z.next_in = (Bytef *)strchr((const char *)old + data_start,
	    old[data_start]);
	z.avail_in = n - data_start;
	z.next_out = *newch;
	z.avail_out = HOWMANY;
	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	rc = inflateInit2(&z, -15);
	if (rc != Z_OK) {
		file_error(ms, 0, "zlib: %s", z.msg);
		return 0;
	}

	rc = inflate(&z, Z_SYNC_FLUSH);
	if (rc != Z_OK && rc != Z_STREAM_END) {
		file_error(ms, 0, "zlib: %s", z.msg);
		return 0;
	}

	n = (size_t)z.total_out;
	inflateEnd(&z);
	
	/* let's keep the nul-terminate tradition */
	(*newch)[n++] = '\0';

	return n;
}
#endif

private size_t
uncompressbuf(struct magic_set *ms, size_t method, const unsigned char *old,
    unsigned char **newch, size_t n)
{
	int fdin[2], fdout[2];
	int r;

	/* The buffer is NUL terminated, and we don't need that. */
	n--;
	 
#ifdef HAVE_LIBZ
	if (method == 2)
		return uncompressgzipped(ms, old, newch, n);
#endif

	if (pipe(fdin) == -1 || pipe(fdout) == -1) {
		file_error(ms, errno, "cannot create pipe");	
		return 0;
	}
	switch (fork()) {
	case 0:	/* child */
		(void) close(0);
		(void) dup(fdin[0]);
		(void) close(fdin[0]);
		(void) close(fdin[1]);

		(void) close(1);
		(void) dup(fdout[1]);
		(void) close(fdout[0]);
		(void) close(fdout[1]);
		if (compr[method].silent)
			(void) close(2);

		execvp(compr[method].argv[0],
		       (char *const *)compr[method].argv);
		exit(1);
		/*NOTREACHED*/
	case -1:
		file_error(ms, errno, "could not fork");
		return 0;

	default: /* parent */
		(void) close(fdin[0]);
		(void) close(fdout[1]);
		/* fork again, to avoid blocking because both pipes filled */
		switch (fork()) {
		case 0: /* child */
			(void)close(fdout[0]);
			if (swrite(fdin[1], old, n) != n)
				exit(1);
			exit(0);
			/*NOTREACHED*/

		case -1:
			exit(1);
			/*NOTREACHED*/

		default:  /* parent */
			break;
		}
		(void) close(fdin[1]);
		fdin[1] = -1;
		if ((*newch = (unsigned char *) malloc(HOWMANY + 1)) == NULL) {
			n = 0;
			goto err;
		}
		if ((r = sread(fdout[0], *newch, HOWMANY)) <= 0) {
			free(*newch);
			n = 0;
			newch[0] = '\0';
			goto err;
		} else {
			n = r;
		}
 		/* NUL terminate, as every buffer is handled here. */
 		(*newch)[n++] = '\0';
err:
		if (fdin[1] != -1)
			(void) close(fdin[1]);
		(void) close(fdout[0]);
#ifdef WNOHANG
		while (waitpid(-1, NULL, WNOHANG) != -1)
			continue;
#else
		(void)wait(NULL);
#endif
		return n;
	}
}
