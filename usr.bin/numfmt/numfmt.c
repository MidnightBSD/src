/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The MidnightBSD Project
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
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/param.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <libutil.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum scale_mode {
	SCALE_NONE,
	SCALE_AUTO,
	SCALE_SI,
	SCALE_IEC,
	SCALE_IEC_I
};

enum invalid_mode {
	INVALID_ABORT,
	INVALID_FAIL,
	INVALID_WARN,
	INVALID_IGNORE
};

enum round_mode {
	ROUND_NEAREST,
	ROUND_UP,
	ROUND_DOWN,
	ROUND_FROM_ZERO,
	ROUND_TOWARDS_ZERO
};

#define	OPT_FORMAT		0x100
#define	OPT_FROM		0x101
#define	OPT_HEADER		0x102
#define	OPT_INVALID		0x103
#define	OPT_PADDING		0x104
#define	OPT_ROUND		0x105
#define	OPT_SUFFIX		0x106
#define	OPT_TO			0x107

struct options {
	enum scale_mode from;
	enum scale_mode to;
	enum invalid_mode invalid;
	enum round_mode round;
	const char *format;
	const char *suffix;
	long field;
	long header;
	int padding;
	int delimiter;
	bool zero;
};

static bool	convert_field(char **, const struct options *);
static bool	convert_line(char *, const struct options *);
static int	format_number(char *, size_t, long double,
		    const struct options *);
static long	get_long(const char *, const char *);
static int	get_scale(long double, int);
static bool	parse_number(const char *, const struct options *, long double *);
static enum scale_mode parse_scale(const char *, const char *);
static int	parse_unit(const char *, enum scale_mode, long double *,
		    const char **);
static long double round_number(long double, enum round_mode);
static char	*strip_suffix(char *, const char *);
static void	usage(void);

static const struct option longopts[] = {
	{ "delimiter",		required_argument,	NULL, 'd' },
	{ "field",		required_argument,	NULL, 'f' },
	{ "format",		required_argument,	NULL, OPT_FORMAT },
	{ "from",		required_argument,	NULL, OPT_FROM },
	{ "header",		optional_argument,	NULL, OPT_HEADER },
	{ "invalid",		required_argument,	NULL, OPT_INVALID },
	{ "padding",		required_argument,	NULL, OPT_PADDING },
	{ "round",		required_argument,	NULL, OPT_ROUND },
	{ "suffix",		required_argument,	NULL, OPT_SUFFIX },
	{ "to",			required_argument,	NULL, OPT_TO },
	{ "zero-terminated",	no_argument,		NULL, 'z' },
	{ NULL,			0,			NULL, 0 }
};

int
main(int argc, char *argv[])
{
	struct options opts;
	char *line;
	size_t linesz;
	ssize_t linelen;
	long lineno;
	int ch, errors;

	memset(&opts, 0, sizeof(opts));
	opts.field = 1;
	opts.delimiter = -1;
	opts.invalid = INVALID_ABORT;
	opts.round = ROUND_FROM_ZERO;

	while ((ch = getopt_long(argc, argv, "d:f:z", longopts, NULL)) != -1) {
		switch (ch) {
		case 'd':
			if (optarg[0] == '\0' || optarg[1] != '\0')
				errx(1, "delimiter must be a single character");
			opts.delimiter = (unsigned char)optarg[0];
			break;
		case 'f':
			opts.field = get_long(optarg, "field");
			if (opts.field < 1)
				errx(1, "field must be greater than zero");
			break;
		case 'z':
			opts.zero = true;
			break;
		case OPT_FORMAT:
			opts.format = optarg;
			break;
		case OPT_FROM:
			opts.from = parse_scale(optarg, "--from");
			break;
		case OPT_HEADER:
			opts.header = optarg == NULL ? 1 :
			    get_long(optarg, "header");
			if (opts.header < 0)
				errx(1, "header must not be negative");
			break;
		case OPT_INVALID:
			if (strcmp(optarg, "abort") == 0)
				opts.invalid = INVALID_ABORT;
			else if (strcmp(optarg, "fail") == 0)
				opts.invalid = INVALID_FAIL;
			else if (strcmp(optarg, "warn") == 0)
				opts.invalid = INVALID_WARN;
			else if (strcmp(optarg, "ignore") == 0)
				opts.invalid = INVALID_IGNORE;
			else
				errx(1, "invalid --invalid mode: %s", optarg);
			break;
		case OPT_PADDING:
			opts.padding = (int)get_long(optarg, "padding");
			break;
		case OPT_ROUND:
			if (strcmp(optarg, "nearest") == 0)
				opts.round = ROUND_NEAREST;
			else if (strcmp(optarg, "up") == 0)
				opts.round = ROUND_UP;
			else if (strcmp(optarg, "down") == 0)
				opts.round = ROUND_DOWN;
			else if (strcmp(optarg, "from-zero") == 0)
				opts.round = ROUND_FROM_ZERO;
			else if (strcmp(optarg, "towards-zero") == 0)
				opts.round = ROUND_TOWARDS_ZERO;
			else
				errx(1, "invalid --round mode: %s", optarg);
			break;
		case OPT_SUFFIX:
			opts.suffix = optarg;
			break;
		case OPT_TO:
			opts.to = parse_scale(optarg, "--to");
			break;
		default:
			usage();
		}
	}

	errors = 0;
	if (optind < argc) {
		for (; optind < argc; optind++)
			if (!convert_field(&argv[optind], &opts))
				errors = 1;
		return (errors);
	}

	line = NULL;
	linesz = 0;
	lineno = 0;
	while ((linelen = getdelim(&line, &linesz, opts.zero ? '\0' : '\n',
		    stdin)) != -1) {
		lineno++;
		if (linelen > 0 &&
		    line[linelen - 1] == (opts.zero ? '\0' : '\n'))
			line[linelen - 1] = '\0';
		if (lineno <= opts.header)
			(void)printf("%s%c", line, opts.zero ? '\0' : '\n');
		else if (convert_line(line, &opts))
			(void)printf("%c", opts.zero ? '\0' : '\n');
		else
			errors = 1;
	}
	free(line);
	if (ferror(stdin))
		err(1, "stdin");

	return (errors);
}

static bool
convert_field(char **field, const struct options *opts)
{
	char out[256];
	long double number;

	if (!parse_number(*field, opts, &number)) {
		if (opts->invalid == INVALID_IGNORE) {
			puts(*field);
			return (true);
		}
		return (false);
	}
	if (format_number(out, sizeof(out), number, opts) == -1)
		return (false);
	*field = strdup(out);
	if (*field == NULL)
		err(1, "strdup");
	puts(*field);
	return (true);
}

static bool
convert_line(char *line, const struct options *opts)
{
	char out[256], *field, *next, *p;
	long current;
	long double number;

	if (opts->delimiter != -1) {
		current = 1;
		for (field = line;; field = next + 1) {
			next = strchr(field, opts->delimiter);
			if (current == opts->field) {
				bool converted;

				if (next != NULL)
					*next = '\0';
				converted = parse_number(field, opts, &number);
				if (converted)
					converted = format_number(out,
					    sizeof(out), number, opts) != -1;
				if (!converted &&
				    opts->invalid == INVALID_IGNORE) {
					if (next != NULL)
						*next = opts->delimiter;
					(void)fputs(line, stdout);
					return (true);
				}
				if (!converted)
					return (false);
				(void)fwrite(line, 1, (size_t)(field - line),
				    stdout);
				(void)fputs(out, stdout);
				if (next != NULL)
					(void)printf("%c%s", opts->delimiter,
					    next + 1);
				return (true);
			}
			if (next == NULL) {
				(void)fputs(line, stdout);
				return (true);
			}
			current++;
		}
	}

	current = 0;
	for (p = line; *p != '\0';) {
		while (isspace((unsigned char)*p))
			p++;
		if (*p == '\0')
			break;
		field = p;
		while (*p != '\0' && !isspace((unsigned char)*p))
			p++;
		current++;
		if (current == opts->field) {
			if (*p != '\0')
				*p++ = '\0';
			if (!parse_number(field, opts, &number)) {
				if (opts->invalid == INVALID_IGNORE) {
					if (p > field && p[-1] == '\0')
						p[-1] = ' ';
					(void)fputs(line, stdout);
					return (true);
				}
				return (false);
			}
			if (format_number(out, sizeof(out), number, opts) == -1)
				return (false);
			(void)fwrite(line, 1, (size_t)(field - line), stdout);
			(void)printf("%s", out);
			if (*p != '\0')
				(void)printf(" %s", p);
			return (true);
		}
	}

	(void)fputs(line, stdout);
	return (true);
}

static int
format_number(char *buf, size_t len, long double number,
    const struct options *opts)
{
	char nbuf[128];
	long double rounded;
	size_t nlen;

	if (opts->to == SCALE_NONE) {
		rounded = round_number(number, opts->round);
		if (rounded > INT64_MAX || rounded < INT64_MIN) {
			warnx("number out of range");
			return (-1);
		}
		if (opts->format != NULL)
			(void)snprintf(nbuf, sizeof(nbuf), opts->format,
			    (double)rounded);
		else
			(void)snprintf(nbuf, sizeof(nbuf), "%" PRId64,
			    (int64_t)rounded);
	} else {
		int flags, scale;
		int64_t inum;

		rounded = round_number(number, opts->round);
		if (rounded > INT64_MAX || rounded < INT64_MIN) {
			warnx("number out of range");
			return (-1);
		}
		inum = (int64_t)rounded;
		flags = HN_NOSPACE | HN_DECIMAL;
		if (opts->to == SCALE_SI)
			flags |= HN_DIVISOR_1000;
		else if (opts->to == SCALE_IEC_I)
			flags |= HN_IEC_PREFIXES;
		scale = get_scale(rounded,
		    (flags & HN_DIVISOR_1000) ? 1000 : 1024);
		if (humanize_number(nbuf, sizeof(nbuf), inum,
		    opts->suffix == NULL ? "" : opts->suffix, scale,
		    flags) == -1) {
			warnx("cannot format number");
			return (-1);
		}
	}

	if (opts->padding != 0) {
		char pbuf[256];

		if (opts->padding > 0)
			(void)snprintf(pbuf, sizeof(pbuf), "%*s", opts->padding,
			    nbuf);
		else
			(void)snprintf(pbuf, sizeof(pbuf), "%-*s",
			    -opts->padding, nbuf);
		nlen = strlcpy(buf, pbuf, len);
	} else
		nlen = strlcpy(buf, nbuf, len);
	if (nlen >= len) {
		warnx("formatted number too long");
		return (-1);
	}
	return (0);
}

static long
get_long(const char *s, const char *name)
{
	char *ep;
	long v;

	errno = 0;
	v = strtol(s, &ep, 10);
	if (errno != 0 || *s == '\0' || *ep != '\0')
		errx(1, "invalid %s: %s", name, s);
	return (v);
}

static int
get_scale(long double number, int divisor)
{
	long double n;
	int scale;

	n = fabsl(number);
	scale = 0;
	while (n >= divisor && scale < 6) {
		n /= divisor;
		scale++;
	}
	return (scale);
}

static bool
parse_number(const char *input, const struct options *opts, long double *number)
{
	char buf[256], *ep;
	const char *work;
	const char *unit;
	long double multiplier, value;
	size_t len;

	len = strlcpy(buf, input, sizeof(buf));
	if (len >= sizeof(buf)) {
		warnx("number too long: %s", input);
		return (false);
	}
	work = strip_suffix(buf, opts->suffix);
	errno = 0;
	value = strtold(work, &ep);
	if (errno != 0 || ep == work) {
		if (opts->invalid == INVALID_ABORT)
			errx(1, "invalid number: %s", input);
		if (opts->invalid == INVALID_WARN ||
		    opts->invalid == INVALID_FAIL)
			warnx("invalid number: %s", input);
		return (false);
	}
	while (isspace((unsigned char)*ep))
		ep++;
	if (parse_unit(ep, opts->from, &multiplier, &unit) == -1 ||
	    *unit != '\0') {
		if (opts->invalid == INVALID_ABORT)
			errx(1, "invalid suffix in number: %s", input);
		if (opts->invalid == INVALID_WARN ||
		    opts->invalid == INVALID_FAIL)
			warnx("invalid suffix in number: %s", input);
		return (false);
	}
	*number = value * multiplier;
	return (true);
}

static enum scale_mode
parse_scale(const char *s, const char *optname)
{
	if (strcmp(s, "none") == 0)
		return (SCALE_NONE);
	if (strcmp(s, "auto") == 0)
		return (SCALE_AUTO);
	if (strcmp(s, "si") == 0)
		return (SCALE_SI);
	if (strcmp(s, "iec") == 0)
		return (SCALE_IEC);
	if (strcmp(s, "iec-i") == 0)
		return (SCALE_IEC_I);
	errx(1, "invalid %s value: %s", optname, s);
}

static int
parse_unit(const char *s, enum scale_mode mode, long double *multiplier,
    const char **endp)
{
	static const char *prefixes = "kMGTPE";
	const char *p;
	int scale;

	*multiplier = 1;
	*endp = s;
	if (mode == SCALE_NONE || *s == '\0')
		return (0);

	p = strchr(prefixes, *s);
	if (p == NULL && *s == 'K')
		scale = 1;
	else if (p != NULL)
		scale = (int)(p - prefixes) + 1;
	else if (*s == 'B') {
		*endp = s + 1;
		return (0);
	} else
		return (-1);

	if (mode == SCALE_IEC_I) {
		if (s[1] != 'i')
			return (-1);
		s += 2;
	} else
		s++;
	if (*s == 'B')
		s++;
	*endp = s;

	while (scale-- > 0)
		*multiplier *= (mode == SCALE_SI ||
		    (mode == SCALE_AUTO && p != NULL && *p == 'k')) ?
		    1000 :
		    1024;
	return (0);
}

static long double
round_number(long double number, enum round_mode mode)
{
	switch (mode) {
	case ROUND_NEAREST:
		return (roundl(number));
	case ROUND_UP:
		return (ceill(number));
	case ROUND_DOWN:
		return (floorl(number));
	case ROUND_TOWARDS_ZERO:
		return (truncl(number));
	case ROUND_FROM_ZERO:
	default:
		return (number < 0 ? floorl(number) : ceill(number));
	}
}

static char *
strip_suffix(char *s, const char *suffix)
{
	size_t slen, suffixlen;

	if (suffix == NULL || *suffix == '\0')
		return (s);
	slen = strlen(s);
	suffixlen = strlen(suffix);
	if (slen >= suffixlen && strcmp(s + slen - suffixlen, suffix) == 0)
		s[slen - suffixlen] = '\0';
	return (s);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: numfmt [--from=UNIT] [--to=UNIT] [--field=N]\n"
	    "              [--format=FORMAT] [--padding=N] [--suffix=SUFFIX]\n"
	    "              [--round=METHOD] [--invalid=MODE] [number ...]\n");
	exit(1);
}
