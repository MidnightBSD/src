/*-
 * Copyright (c) 2011 Nathan Whitehorn
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
 * $FreeBSD: stable/10/usr.sbin/bsdinstall/partedit/partedit_pc98.c 273831 2014-10-29 16:48:18Z nwhitehorn $
 */

#include <string.h>

#include "partedit.h"

const char *
default_scheme(void) {
	return ("PC98");
}

int
is_scheme_bootable(const char *part_type) {
	if (strcmp(part_type, "BSD") == 0)
		return (1);
	if (strcmp(part_type, "PC98") == 0)
		return (1);

	return (0);
}

int
is_fs_bootable(const char *part_type, const char *fs)
{
	if (strcmp(fs, "mnbsd-ufs") == 0)
		return (1);
	
	return (0);
}

size_t
bootpart_size(const char *part_type) {
	/* No boot partition */
	return (0);
}

const char *
bootpart_type(const char *scheme) {
	return ("mnbsd-boot");
}

const char *
bootcode_path(const char *part_type) {
	if (strcmp(part_type, "PC98") == 0)
		return ("/boot/pc98boot");
	if (strcmp(part_type, "BSD") == 0)
		return ("/boot/boot");

	return (NULL);
}
	
const char *
partcode_path(const char *part_type, const char *fs_type) {
	/* No partcode */
	return (NULL);
}

