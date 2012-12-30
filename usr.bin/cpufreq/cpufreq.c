/* $MidnightBSD: src/usr.bin/cpufreq/cpufreq.c,v 1.2 2012/12/30 20:20:10 laffer1 Exp $ */
/*- 
 * Copyright (c) 2008, 2011 Lucas Holt
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

#include <sys/types.h>
#include <sys/sysctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <unistd.h>

static void	usage(void);

int 
main(int argc, char *argv[])
{
	int freq;
	char levels[256];
	size_t len;
	int ch, vflag;

	vflag = 0;
	while ((ch = getopt(argc, argv, "v")) != -1) {
		switch (ch) {
			case 'v':
				vflag = 1;
				break;
			case '?': /* FALLTHROUGH */
			default:
				usage();
				/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	len = sizeof(freq);
	if (sysctlbyname("dev.cpu.0.freq", &freq, &len, NULL, 0) < 0)
		errx(1, "CPU frequency unknown");
	printf("CPU frequency: %d MHz\n", freq);

	if (vflag) {
		len = sizeof(levels);
        	if (sysctlbyname("dev.cpu.0.freq_levels", &levels, &len, NULL, 0) < 0)
                	errx(1, "Available CPU frequency levels unavailable");
		printf("Possible frequencies: %s\n", levels);
	}

	return (0);
}

static void
usage(void)
{

	fprintf(stderr, "usage: cpufreq [-v]\n");
	exit(1);
}
