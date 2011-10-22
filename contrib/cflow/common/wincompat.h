/*-
 * Copyright (c) 2009, Marcus von Appen
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *
 * $FreeBSD$
 *
 */

/* Win32/MSVC platform specific code */
#ifdef _MSC_VER

#include <ctype.h>

#ifndef inline
#define inline
#endif

#ifndef PATH_MAX
#define PATH_MAX 255
#endif

static int
strcasecmp (const char *text1, const char *text2)
{
    const char *p1 = text1;
    const char *p2 = text2;

    while (tolower (*p1) == tolower (*p2))
    {
        if (*p1++ == '\0' || *p2++ == '\0')
            return 0;
    }
    return 1;
};


/* PD implementation of getopt(), based on the code of the 1985 UNIFORUM conference */
static char *optarg = NULL;
static int optind = 1;
static int optopt = 0;

static int
getopt (int argc, const char *argv[], const char *opts)
{
	static int sp = 1;
	register int c;
	register char *cp;

	if (sp == 1) {
		
		/* If all args are processed, finish */
		if (optind >= argc) {
			return EOF;
		}
		if (argv[optind][0] != '-' || argv[optind][1] == '\0') {
			return EOF;
		}
		
	} else if (!strcmp(argv[optind], "--")) {
		
		/* No more options to be processed after this one */
		optind++;
		return EOF;
		
	}
	
	optopt = c = argv[optind][sp];

	/* Check for invalid option */
	if (c == ':' || (cp = strchr(opts, c)) == NULL) {
		
		fprintf(stderr,
			"%s: illegal option -- %c\n",
			argv[0],
			c);
		if (argv[optind][++sp] == '\0') {
			optind++;
			sp = 1;
		}
		
		return '?';
	}

	/* Does this option require an argument? */
	if (*++cp == ':') {

		/* If so, get argument; if none provided output error */
		if (argv[optind][sp+1] != '\0') {
			optarg = (char *) &argv[optind++][sp+1];
		} else if (++optind >= argc) {
			fprintf(stderr,
				"%s: option requires an argument -- %c\n",
				argv[0],
				c);
			sp = 1;
			return '?';
		} else {
			optarg = (char *) argv[optind++];
		}
		sp = 1;

	} else {
		if (argv[optind][++sp] == '\0') {
			sp = 1;
			optind++;
		}
		optarg = NULL;
	}
	
	return c;
};

#endif
