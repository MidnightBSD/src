/*	$OpenBSD: ts.c,v 1.7 2022/07/06 07:59:03 claudio Exp $	*/
/*
 * Copyright (c) 2022 Job Snijders <job@openbsd.org>
 * Copyright (c) 2022 Claudio Jeker <claudio@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/time.h>

#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const char	*format = "%b %d %H:%M:%S";
static char		*buf;
static char		*outbuf;
static size_t		 bufsize;

static void		 fmtfmt(const struct timespec *);
static size_t		 format_length(const char *);
static void __dead2 usage(void);

int
main(int argc, char *argv[])
{
	int iflag, mflag, sflag;
	int ch, prev;
	struct timespec start, now, utc_offset, ts;
	clockid_t clock = CLOCK_REALTIME;

	iflag = mflag = sflag = 0;

	while ((ch = getopt(argc, argv, "ims")) != -1) {
		switch (ch) {
		case 'i':
			iflag = 1;
			format = "%H:%M:%S";
			clock = CLOCK_MONOTONIC;
			break;
		case 'm':
			mflag = 1;
			clock = CLOCK_MONOTONIC;
			break;
		case 's':
			sflag = 1;
			format = "%H:%M:%S";
			clock = CLOCK_MONOTONIC;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if ((iflag && sflag) || argc > 1)
		usage();

	if (argc == 1)
		format = *argv;

	bufsize = format_length(format) * 10;
	if (bufsize == 0)
		bufsize = 1;
	if ((buf = calloc(1, bufsize)) == NULL)
		err(1, NULL);
	if ((outbuf = calloc(1, bufsize)) == NULL)
		err(1, NULL);

	/* Force UTC for interval calculations. */
	if (iflag || sflag) {
		if (setenv("TZ", "UTC", 1) == -1)
			err(1, "setenv UTC");
	}

	if (clock_gettime(clock, &start) == -1)
		err(1, "clock_gettime");
	if (clock_gettime(CLOCK_REALTIME, &utc_offset) == -1)
		err(1, "clock_gettime");
	timespecsub(&utc_offset, &start, &utc_offset);

	for (prev = '\n'; (ch = getchar()) != EOF; prev = ch) {
		if (prev == '\n') {
			if (clock_gettime(clock, &now) == -1)
				err(1, "clock_gettime");
			if (iflag || sflag)
				timespecsub(&now, &start, &ts);
			else if (mflag)
				timespecadd(&now, &utc_offset, &ts);
			else
				ts = now;
			fmtfmt(&ts);
			if (iflag)
				start = now;
		}
		if (putchar(ch) == EOF)
			break;
	}
	if (ferror(stdin))
		err(1, "stdin");

	if (fclose(stdout) == EOF)
		err(1, "stdout");
	free(outbuf);
	free(buf);
	return (0);
}

static void __dead2
usage(void)
{

	fprintf(stderr, "usage: %s [-i | -s] [-m] [format]\n", getprogname());
	exit(1);
}

static void
fmtfmt(const struct timespec *ts)
{
	struct tm tm;
	char *f, us[7];
	long usec;
	size_t format_len, i;

	if (localtime_r(&ts->tv_sec, &tm) == NULL)
		err(1, "localtime");

	if (ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000L)
		errx(1, "invalid nanosecond value");
	usec = ts->tv_nsec / 1000;
	us[6] = '\0';
	for (i = 6; i > 0; i--) {
		us[i - 1] = (char)('0' + usec % 10);
		usec /= 10;
	}
	format_len = format_length(format);
	for (i = 0; i <= format_len; i++)
		buf[i] = format[i];
	f = buf;

	do {
		while (*f != '\0') {
			if (*f != '%') {
				f++;
				continue;
			}
			if (f[1] == '%') {
				f += 2;
				continue;
			}
			break;
		}
		if (*f == '\0')
			break;

		f++;
		if (f[0] == '.' &&
		    (f[1] == 'S' || f[1] == 's' || f[1] == 'T')) {
			size_t len, offset;

			f[0] = f[1];
			f[1] = '.';
			f += 2;
			len = format_length(f);
			offset = (size_t)(f - buf);
			if (offset > bufsize || bufsize - offset < len + 7)
				errx(1, "expanded format string too big");
			for (i = len + 1; i > 0; i--)
				f[i + 5] = f[i - 1];
			for (i = 0; i < 6; i++)
				f[i] = us[i];
			f += 6;
		}
	} while (*f != '\0');

	if (strftime(outbuf, bufsize, buf, &tm) == 0 && buf[0] != '\0')
		errx(1, "strftime");
	if (fprintf(stdout, "%s ", outbuf) < 0)
		err(1, "stdout");
}

static size_t
format_length(const char *str)
{
	size_t len;

	for (len = 0; str[len] != '\0'; len++) {
		if (len >= SIZE_MAX / 10)
			errx(1, "format string too big");
	}
	return (len);
}
