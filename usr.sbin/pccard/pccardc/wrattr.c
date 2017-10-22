/*
 * Copyright (c) 1995 Andrew McRae.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD: release/7.0.0/usr.sbin/pccard/pccardc/wrattr.c 50479 1999-08-28 01:35:59Z peter $";
#endif /* not lint */

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <pccard/cardinfo.h>

static void
usage()
{
	fprintf(stderr, "usage: pccardc wrattr slot offs value\n");
	exit(1);
}

int
wrattr_main(argc, argv)
	int     argc;
	char   *argv[];
{
	int     reg, value;
	char    name[64], c;
	int     fd;
	off_t   offs;

	if (argc != 4)
		usage();
	sprintf(name, CARD_DEVICE, atoi(argv[1]));
	fd = open(name, O_RDWR);
	if (fd < 0)
		err(1, "%s", name);

	reg = MDF_ATTR;
	if (ioctl(fd, PIOCRWFLAG, &reg))
		err(1, "ioctl (PIOCRWFLAG)");

	if (sscanf(argv[2], "%x", &reg) != 1 ||
	    sscanf(argv[3], "%x", &value) != 1)
		errx(1, "arg error");

	offs = reg;
	c = value;
	lseek(fd, offs, SEEK_SET);
	if (write(fd, &c, 1) != 1)
		err(1, "%s", name);
	return 0;
}
