/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2007-2009 Chris Reinhardt
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

#include <sys/cdefs.h>

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <stdbool.h>
#include "mport.h"
#include "mport_private.h"

struct version {
	char *version;
	long long revision;
	long long epoch;
};

/* One dot-separated component of a version: a number, an optional letter
 * and an optional patch level after the letter, e.g. "2b3" is { 2, b, 3 }. */
struct version_component {
	long long n; /* numeric part; -1 when missing, -2 for '*' */
	int a; /* letter rank; 0 when missing */
	long long pl; /* patch level after the letter; -1 when missing */
};

static int parse_version(const char *, struct version *);
static long long parse_number(const char *);
static const char *get_component(const char *, struct version_component *);
static int cmp_versions(const char *, const char *);
static int cmp_ints(long long, long long);

/* mport_version_cmp(version1, version2)
 *
 * Compare two given version strings.  Returns 0 if the versions
 * are the same, -1 if version1 is less than version2, 1 otherwise.
 */
MPORT_PUBLIC_API int
mport_version_cmp(const char *astr, const char *bstr)
{
	struct version a;
	struct version b;
	int result;

	if (parse_version(astr, &a) != MPORT_OK)
		return 0;
	if (parse_version(bstr, &b) != MPORT_OK)
		return 0;

	/* remember that a.version/b.version are useless after calling
	   cmp_versions (but astr and bstr are unchanged.) */
	if ((result = cmp_ints(a.epoch, b.epoch)) == 0) {
		if ((result = cmp_versions(a.version, b.version)) == 0) {
			result = cmp_ints(a.revision, b.revision);
		}
	}

#ifdef DEBUG
	printf("Version a %s, Version b %s, result %d\n", a.version, b.version, result);
#endif

	free(a.version);
	a.version = NULL;
	free(b.version);
	b.version = NULL;

	return (result);
}

/* version of mport_version_cmp() that is bound to the sqlite3 database. */
void
mport_version_cmp_sqlite(sqlite3_context *context, int argc, sqlite3_value **argv)
{
	char *a = NULL;
	char *b = NULL;
	bool anull;
	bool bnull;

	assert(argc == 2);
	if (argc != 2) {
		sqlite3_result_int(context, 0);
		return;
	}

	/* sqlite3_value_text() returns NULL for SQL NULL columns (e.g. a
	   nullable version), which would crash strdup(); sort NULLs below any
	   real version instead. */
	anull = sqlite3_value_type(argv[0]) == SQLITE_NULL;
	bnull = sqlite3_value_type(argv[1]) == SQLITE_NULL;
	if (anull || bnull) {
		if (anull && bnull)
			sqlite3_result_int(context, 0);
		else
			sqlite3_result_int(context, anull ? -1 : 1);
		return;
	}

	a = strdup((const char *)sqlite3_value_text(argv[0]));
	b = strdup((const char *)sqlite3_value_text(argv[1]));
	if (a == NULL || b == NULL) {
		free(a);
		free(b);
		sqlite3_result_error_nomem(context);
		return;
	}

	sqlite3_result_int(context, mport_version_cmp(a, b));

	free(a);
	a = NULL;
	free(b);
	b = NULL;
}

/* Returns 0 if baseline meets the given requirement, -1 if the requirement
 * was not met, and a value greater than 0 on error (with the error set).
 * A requirement is one or more operator/version pairs that must all hold:
 *
 * mport_version_require_check("0.2.1", ">=2.0")       == -1
 * mport_version_require_check("4.1.2", ">5.1")        == -1
 * mport_version_require_check("2.0",   ">=1.0<4.0")   == 0
 * mport_version_require_check("4.0",   ">=1.0<4.0")   == -1
 * mport_version_require_check("3.1.4", "|")           > 0
 *
 * Operators are <, <=, >, >=, = and ==.  Whitespace between pairs is
 * ignored; the version text itself may not contain <, > or =.
 */
int
mport_version_require_check(const char *baseline, const char *require)
{
	const char *p;

	if (baseline == NULL || require == NULL) {
		RETURN_ERROR(MPORT_ERR_FATAL, "Version requirement is null");
	}

	p = require;
	while (isspace((unsigned char)*p))
		p++;

	if (*p == '\0') {
		RETURN_ERRORX(MPORT_ERR_FATAL, "Malformed version requirement: '%s'", require);
	}

	while (*p != '\0') {
		char op = *p;
		bool orequal = false;
		const char *start, *end;
		char *version;
		int cmp;
		bool ok;

		if (op != '<' && op != '>' && op != '=') {
			RETURN_ERRORX(
			    MPORT_ERR_FATAL, "Malformed version requirement: '%s'", require);
		}
		p++;
		if (*p == '=') {
			orequal = true;
			p++;
		}
		if (op == '=')
			orequal = true;

		while (isspace((unsigned char)*p))
			p++;

		/* the version runs up to the next operator or the end */
		start = p;
		p += strcspn(p, "<>=");
		end = p;

		/* trim trailing whitespace off the version */
		while (end > start && isspace((unsigned char)end[-1]))
			end--;

		if (end == start) {
			RETURN_ERRORX(
			    MPORT_ERR_FATAL, "Malformed version requirement: '%s'", require);
		}

		version = strndup(start, (size_t)(end - start));
		if (version == NULL) {
			RETURN_ERROR(MPORT_ERR_FATAL, "Memory allocation failed");
		}

		cmp = mport_version_cmp(baseline, version);
		free(version);
		version = NULL;

		if (op == '<')
			ok = orequal ? (cmp <= 0) : (cmp < 0);
		else if (op == '>')
			ok = orequal ? (cmp >= 0) : (cmp > 0);
		else
			ok = (cmp == 0);

		if (!ok)
			return (-1);

		while (isspace((unsigned char)*p))
			p++;
	}

	return (0);
}

static int
parse_version(const char *in, struct version *v)
{
	char *s = strdup(in);
	char *underscore;

	if (s == NULL)
		return MPORT_ERR_FATAL;

	char *comma;
	// greater than and less than prevent multiversion strings from getting parsed incorrectly
	// so 2.0<1.5 would just be 2.0. Ideally we need to catch this upstream and do the right
	// check.
	char *lessthan;
	char *greaterthan;

	underscore = rindex(s, '_');
	comma = rindex(s, ',');
	lessthan = rindex(s, '<');
	greaterthan = rindex(s, '>');

	if (lessthan != NULL) {
		*lessthan = '\0';
	}

	if (greaterthan != NULL) {
		*greaterthan = '\0';
	}

	if (comma == NULL) {
		v->epoch = 0;
	} else {
		*comma = '\0';
		v->epoch = parse_number(comma + 1);
	}

	if (underscore == NULL) {
		v->revision = 0;
	} else {
		*underscore = '\0';
		v->revision = parse_number(underscore + 1);
	}

	v->version = s;

	return MPORT_OK;
}

/* Parse a non-negative epoch or revision.  A value that does not start with
 * a digit (including a leading sign) counts as 0.  A value too large for a
 * long long saturates at LLONG_MAX rather than wrapping or truncating, so it
 * still sorts above every representable number. */
static long long
parse_number(const char *s)
{

	if (!isdigit((unsigned char)*s))
		return 0;

	return strtoll(s, NULL, 10);
}

static int
cmp_ints(long long a, long long b)
{

	if (a == b)
		return 0;
	if (a < b)
		return -1;

	return 1;
}

/*
 * Version ordering follows the FreeBSD ports convention implemented by
 * pkg(8) (libpkg/pkg_version.c, Oliver Eikemeier):
 *
 * - components are separated by dots; characters outside [a-zA-Z0-9.+*]
 *   are also separators, and consecutive separators collapse (10..1 = 10.1)
 * - a component is number, letter, patch level; missing separators are
 *   inserted (10a1b2 = 10a1.b2)
 * - missing components are 0 (10 = 10.0 = 10.0.0)
 * - a letter after a number sorts after the bare number (10 < 10a < 10b)
 * - a component that starts with a letter has number -1, so it sorts before
 *   the bare number (10.a < 10)
 * - the words "pl", "snap", "alpha", "beta", "pre" and "rc" start a new
 *   component, so 10rc1 = 10.rc1 < 10; "pl" sorts before every other letter
 *   and the rest sort as their first letter (pl < alpha < beta < pre < rc)
 * - other words use only their first letter, case-insensitively
 *   (1.d2 = 1.dev2 = 1.Development2)
 * - '*' is the smallest possible component (2.* < 2pl1 < 3)
 * - '+' separates blocks that are compared after everything before them
 */
static const struct stage {
	const char *name;
	size_t namelen;
	int value;
} stages[] = {
	{ "pl", 2, 0 },
#define ABASE 2 /* last special early-sorted prefix + 1 */
	{ "snap", 4, 's' - 'a' + ABASE },
	{ "alpha", 5, 'a' - 'a' + ABASE },
	{ "beta", 4, 'b' - 'a' + ABASE },
	{ "pre", 3, 'p' - 'a' + ABASE },
	{ "rc", 2, 'r' - 'a' + ABASE },
};

/* Read the next component starting at pos and return a pointer past it and
 * any trailing separators.  Always advances when *pos is not NUL or '+'. */
static const char *
get_component(const char *pos, struct version_component *component)
{
	bool hasstage = false;
	bool haspatchlevel = false;

	/* number */
	if (isdigit((unsigned char)*pos)) {
		char *endptr;

		component->n = strtoll(pos, &endptr, 10);
		pos = endptr;
	} else if (*pos == '*') {
		component->n = -2;
		do {
			pos++;
		} while (*pos != '\0' && *pos != '+');
	} else {
		component->n = -1;
		hasstage = true;
	}

	/* letter */
	if (isalpha((unsigned char)*pos)) {
		int c = tolower((unsigned char)*pos);

		haspatchlevel = true;
		if (isalpha((unsigned char)pos[1])) {
			for (size_t i = 0; i < sizeof(stages) / sizeof(stages[0]); i++) {
				const struct stage *stage = &stages[i];

				if (strncasecmp(pos, stage->name, stage->namelen) == 0 &&
				    !isalpha((unsigned char)pos[stage->namelen])) {
					if (hasstage) {
						/* stage word becomes the letter */
						component->a = stage->value;
						pos += stage->namelen;
					} else {
						/* number then stage word: split here */
						component->a = 0;
						haspatchlevel = false;
					}
					c = 0;
					break;
				}
			}
		}
		if (c != 0) {
			/* use the first letter and skip the rest of the word */
			component->a = c - 'a' + ABASE;
			do {
				pos++;
			} while (isalpha((unsigned char)*pos));
		}
	} else {
		component->a = 0;
		haspatchlevel = false;
	}

	/* patch level */
	if (haspatchlevel) {
		if (isdigit((unsigned char)*pos)) {
			char *endptr;

			component->pl = strtoll(pos, &endptr, 10);
			pos = endptr;
		} else {
			component->pl = -1;
		}
	} else {
		component->pl = 0;
	}

	/* trailing separators */
	while (*pos != '\0' && !isdigit((unsigned char)*pos) && !isalpha((unsigned char)*pos) &&
	    *pos != '+' && *pos != '*')
		pos++;

	return pos;
}

static int
cmp_versions(const char *a, const char *b)
{
	int result = 0;

	while (result == 0 && (*a != '\0' || *b != '\0')) {
		struct version_component ca = { 0, 0, 0 };
		struct version_component cb = { 0, 0, 0 };
		bool block_a = false;
		bool block_b = false;

		if (*a != '\0' && *a != '+')
			a = get_component(a, &ca);
		else
			block_a = true;

		if (*b != '\0' && *b != '+')
			b = get_component(b, &cb);
		else
			block_b = true;

		if (block_a && block_b) {
			/* both sides are at a '+' or at the end: step past it */
			if (*a != '\0')
				a++;
			if (*b != '\0')
				b++;
		} else if (ca.n != cb.n) {
			result = cmp_ints(ca.n, cb.n);
		} else if (ca.a != cb.a) {
			result = cmp_ints(ca.a, cb.a);
		} else if (ca.pl != cb.pl) {
			result = cmp_ints(ca.pl, cb.pl);
		}
	}

	return (result);
}
