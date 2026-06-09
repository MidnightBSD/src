/* $MidnightBSD: src/usr.bin/cpufreq/cpufreq.c,v 1.2 2012/12/30 20:20:10 laffer1
 * Exp $ */
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

#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int get_cpu_freq(int);
static void print_freq_levels(int);
static void usage(void);

int
main(int argc, char *argv[])
{
	const char *errstr;
	int cflag, ch, cpu, mflag, ncpu, vflag;
	size_t len;

	cflag = 0;
	cpu = 0;
	mflag = 0;
	vflag = 0;
	while ((ch = getopt(argc, argv, "c:mv")) != -1) {
		switch (ch) {
		case 'c':
			cflag = 1;
			cpu = (int)strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "cpu is %s: %s", errstr, optarg);
			break;
		case 'm':
			mflag = 1;
			break;
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
	if (argc != 0)
		usage();
	if (mflag && cflag)
		usage();

	if (mflag) {
		int i;
		long long total;

		len = sizeof(ncpu);
		if (sysctlbyname("hw.ncpu", &ncpu, &len, NULL, 0) < 0)
			errx(1, "CPU count unknown");
		if (ncpu <= 0)
			errx(1, "CPU count invalid");
		total = 0;
		for (i = 0; i < ncpu; i++)
			total += get_cpu_freq(i);
		printf("Average CPU frequency: %lld MHz\n", total / ncpu);
	} else {
		int freq;

		freq = get_cpu_freq(cpu);
		printf("CPU %d frequency: %d MHz\n", cpu, freq);
	}

	if (vflag)
		print_freq_levels(cpu);

	return (0);
}

static int
get_cpu_freq(int cpu)
{
	char name[32];
	int freq, n;
	size_t len;

	len = sizeof(freq);
	n = snprintf(name, sizeof(name), "dev.cpu.%d.freq", cpu);
	if (n < 0 || n >= (int)sizeof(name))
		errx(1, "CPU sysctl name too long");
	if (sysctlbyname(name, &freq, &len, NULL, 0) < 0)
		errx(1, "CPU %d frequency unknown", cpu);

	return (freq);
}

static void
print_freq_levels(int cpu)
{
	char levels[256];
	char name[32];
	int n;
	size_t len;

	len = sizeof(levels);
	n = snprintf(name, sizeof(name), "dev.cpu.%d.freq_levels", cpu);
	if (n < 0 || n >= (int)sizeof(name))
		errx(1, "CPU sysctl name too long");
	if (sysctlbyname(name, levels, &len, NULL, 0) < 0)
		errx(1, "Available CPU %d frequency levels unavailable", cpu);
	if (len >= sizeof(levels))
		len = sizeof(levels) - 1;
	levels[len] = '\0';
	printf("Possible frequencies: %s\n", levels);
}

static void
usage(void)
{

	fprintf(stderr, "usage: cpufreq [-c cpu] [-m] [-v]\n");
	exit(1);
}
