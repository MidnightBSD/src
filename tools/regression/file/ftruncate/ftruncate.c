/*-
 * Copyright (c) 2006 Robert N. M. Watson
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
 *
 * $FreeBSD$
 */

/*
 * Very simple regression test.
 *
 * Future tests that might be of interest:
 *
 * - Make sure we get EISDIR on a directory.
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>

/*
 * Select various potentially interesting lengths at and around power of 2
 * edges.
 */
static off_t lengths[] = {0, 1, 2, 3, 4, 127, 128, 129, 511, 512, 513, 1023,
    1024, 1025, 2047, 2048, 2049, 4095, 4096, 4097, 8191, 8192, 8193, 16383,
    16384, 16385};
static int lengths_count = sizeof(lengths) / sizeof(off_t);

int
main(int argc, char *argv[])
{
	int error, fd, fds[2], i, read_only_fd;
	char path[PATH_MAX];
	struct stat sb;
	size_t size;
	off_t len;
	char ch;

	/*
	 * Tests using a writable temporary file: grow and then shrink a file
	 * using ftruncate and various lengths.  Make sure that a negative
	 * file length is rejected.  Make sure that when we grow the file,
	 * bytes now in the range of the file size return 0.
	 *
	 * Save a read-only reference to the file to use later for read-only
	 * descriptor tests.
	 */
	snprintf(path, PATH_MAX, "/tmp/ftruncate.XXXXXXXXXXXXX");
	fd = mkstemp(path);
	if (fd < 0)
		err(-1, "makestemp");
	read_only_fd = open(path, O_RDONLY);
	if (read_only_fd < 0) {
		error = errno;
		(void)unlink(path);
		errno = error;
		err(-1, "open(%s, O_RDONLY)", path);
	}
	(void)unlink(path);

	if (ftruncate(fd, -1) == 0)
		errx(-1, "ftruncate(fd, -1) succeeded");
	if (errno != EINVAL)
		err(-1, "ftruncate(fd, -1) returned wrong error");

	for (i = 0; i < lengths_count; i++) {
		len = lengths[i];
		if (ftruncate(fd, len) < 0)
			err(-1, "ftruncate(%llu) up", len);
		if (fstat(fd, &sb) < 0)
			err(-1, "stat");
		if (sb.st_size != len)
			errx(-1, "fstat(%llu) returned len %llu up", len,
			    sb.st_size);
		if (len != 0) {
			size = pread(fd, &ch, sizeof(ch), len - 1);
			if (size < 0)
				err(-1, "pread on len %llu up", len);
			if (size != sizeof(ch))
				errx(-1, "pread len %llu size %jd up",
				    len, (intmax_t)size);
			if (ch != 0)
				errx(-1,
				    "pread length %llu size %jd ch %d up",
				    len, (intmax_t)size, ch);
		}
	}

	for (i = lengths_count - 1; i >= 0; i--) {
		len = lengths[i];
		if (ftruncate(fd, len) < 0)
			err(-1, "ftruncate(%llu) down", len);
		if (fstat(fd, &sb) < 0)
			err(-1, "stat");
		if (sb.st_size != len)
			errx(-1, "fstat(%llu) returned %llu down", len,
			    sb.st_size);
	}
	close(fd);

	/*
	 * Make sure that a read-only descriptor can't be truncated.
	 */
	if (ftruncate(read_only_fd, 0) == 0)
		errx(-1, "ftruncate(read_only_fd) succeeded");
	if (errno != EINVAL)
		err(-1, "ftruncate(read_only_fd) returned wrong error");
	close(read_only_fd);

	/*
	 * Make sure that ftruncate on sockets doesn't work.
	 */
	fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		err(-1, "socket(PF_UNIX, SOCK_STREAM, 0)");
	if (ftruncate(fd, 0) == 0)
		errx(-1, "ftruncate(socket) succeeded");
	if (errno != EINVAL)
		err(-1, "ftruncate(socket) returned wrong error");
	close(fd);

	/*
	 * Make sure that ftruncate on pipes doesn't work.
	 */
	if (pipe(fds) < 0)
		err(-1, "pipe");
	if (ftruncate(fds[0], 0) == 0)
		errx(-1, "ftruncate(pipe) succeeded");
	if (errno != EINVAL)
		err(-1, "ftruncate(pipe) returned wrong error");
	close(fds[0]);
	close(fds[1]);

	/*
	 * Make sure that ftruncate on kqueues doesn't work.
	 */
	fd = kqueue();
	if (fd < 0)
		err(-1, "kqueue");
	if (ftruncate(fds[0], 0) == 0)
		errx(-1, "ftruncate(kqueue) succeeded");
	if (errno != EINVAL)
		err(-1, "ftruncate(kqueue) returned wrong error");
	close(fd);

	return (0);
}
