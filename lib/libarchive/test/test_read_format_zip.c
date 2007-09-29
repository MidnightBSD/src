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
__FBSDID("$FreeBSD: src/lib/libarchive/test/test_read_format_zip.c,v 1.2 2007/05/29 01:00:21 kientzle Exp $");

static unsigned char archive[] = {
'P','K',3,4,10,0,0,0,0,0,'Y','f',179,'6',0,0,0,0,0,0,0,0,0,0,0,0,4,0,21,0,
'd','i','r','/','U','T',9,0,3,25,'U','O','F',25,'U','O','F','U','x',4,0,232,
3,232,3,'P','K',3,4,20,0,0,0,8,0,'o','f',179,'6',':','7','f','=',10,0,0,0,
18,0,0,0,5,0,21,0,'f','i','l','e','1','U','T',9,0,3,'A','U','O','F',172,'[',
'O','F','U','x',4,0,232,3,232,3,203,'H',205,201,201,231,202,'@','"',1,'P',
'K',3,4,20,0,0,0,8,0,'Z','j',179,'6',':','7','f','=',10,0,0,0,18,0,0,0,5,
0,21,0,'f','i','l','e','2','U','T',9,0,3,172,'[','O','F',172,'[','O','F',
'U','x',4,0,232,3,232,3,203,'H',205,201,201,231,202,'@','"',1,'P','K',1,2,
23,3,10,0,0,0,0,0,'Y','f',179,'6',0,0,0,0,0,0,0,0,0,0,0,0,4,0,13,0,0,0,0,
0,0,0,16,0,237,'A',0,0,0,0,'d','i','r','/','U','T',5,0,3,25,'U','O','F','U',
'x',0,0,'P','K',1,2,23,3,20,0,0,0,8,0,'o','f',179,'6',':','7','f','=',10,
0,0,0,18,0,0,0,5,0,13,0,0,0,0,0,1,0,0,0,164,129,'7',0,0,0,'f','i','l','e',
'1','U','T',5,0,3,'A','U','O','F','U','x',0,0,'P','K',1,2,23,3,20,0,0,0,8,
0,'Z','j',179,'6',':','7','f','=',10,0,0,0,18,0,0,0,5,0,13,0,0,0,0,0,1,0,
0,0,164,129,'y',0,0,0,'f','i','l','e','2','U','T',5,0,3,172,'[','O','F','U',
'x',0,0,'P','K',5,6,0,0,0,0,3,0,3,0,191,0,0,0,187,0,0,0,0,0};

DEFINE_TEST(test_read_format_zip)
{
	struct archive_entry *ae;
	struct archive *a;
	char *buff[128];

	assert((a = archive_read_new()) != NULL);
	assertA(0 == archive_read_support_compression_all(a));
	assertA(0 == archive_read_support_format_all(a));
	assertA(0 == archive_read_open_memory(a, archive, sizeof(archive)));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("dir/", archive_entry_pathname(ae));
	assertEqualInt(1179604249, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_size(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("file1", archive_entry_pathname(ae));
	assertEqualInt(1179604289, archive_entry_mtime(ae));
	assertEqualInt(18, archive_entry_size(ae));
	assertEqualInt(18, archive_read_data(a, buff, 18));
	assert(0 == memcmp(buff, "hello\nhello\nhello\n", 18));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("file2", archive_entry_pathname(ae));
	assertEqualInt(1179605932, archive_entry_mtime(ae));
	assertEqualInt(18, archive_entry_size(ae));
	assertEqualInt(18, archive_read_data(a, buff, 18));
	assert(0 == memcmp(buff, "hello\nhello\nhello\n", 18));
	assertA(archive_compression(a) == ARCHIVE_COMPRESSION_NONE);
	assertA(archive_format(a) == ARCHIVE_FORMAT_ZIP);
	assert(0 == archive_read_close(a));
#if ARCHIVE_API_VERSION > 1
	assert(0 == archive_read_finish(a));
#else
	archive_read_finish(a);
#endif
}


