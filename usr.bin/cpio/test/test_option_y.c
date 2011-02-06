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
__FBSDID("$FreeBSD$");

DEFINE_TEST(test_option_y)
{
	char *p;
	int fd;
	int r;
	size_t s;

	/* Create a file. */
	fd = open("f", O_CREAT | O_WRONLY, 0644);
	assert(fd >= 0);
	assertEqualInt(1, write(fd, "a", 1));
	close(fd);

	/* Archive it with bzip2 compression. */
	r = systemf("echo f | %s -oy >archive.out 2>archive.err",
	    testprog);
	failure("-y (bzip) option seems to be broken");
	if (assertEqualInt(r, 0)) {
		p = slurpfile(&s, "archive.err");
		p[s] = '\0';
		if (strstr(p, "bzip2 compression not supported") != NULL) {
			skipping("This version of bsdcpio was compiled "
			    "without bzip2 support");
		} else {
			assertTextFileContents("1 block\n", "archive.err");
			/* Check that the archive file has a bzip2 signature. */
			p = slurpfile(&s, "archive.out");
			assert(s > 2);
			assertEqualMem(p, "BZh9", 4);
		}
	}
}
