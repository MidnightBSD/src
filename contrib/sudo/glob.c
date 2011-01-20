/*
 * Copyright (c) 2008-2010 Todd C. Miller <Todd.Miller@courtesan.com>
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guido van Rossum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)glob.c	8.3 (Berkeley) 10/13/93
 */

/*
 * glob(3) -- a superset of the one defined in POSIX 1003.2.
 *
 * The [!...] convention to negate a range is supported (SysV, Posix, ksh).
 *
 * Optional extra services, controlled by flags not defined by POSIX:
 *
 * GLOB_MAGCHAR:
 *	Set in gl_flags if pattern contained a globbing character.
 * GLOB_TILDE:
 *	expand ~user/foo to the /home/dir/of/user/foo
 * GLOB_BRACE:
 *	expand {1,2}{a,b} to 1a 1b 2a 2b
 * gl_matchc:
 *	Number of matches in the current invocation of glob.
 */

#include <config.h>

#include <sys/param.h>
#include <sys/stat.h>

#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#if defined(HAVE_MALLOC_H) && !defined(STDC_HEADERS)
# include <malloc.h>
#endif /* HAVE_MALLOC_H && !STDC_HEADERS */
#ifdef HAVE_STRING_H
# include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif /* HAVE_STRINGS_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <ctype.h>
#ifdef HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif
#include <errno.h>
#include <limits.h>
#include <pwd.h>

#include <compat.h>
#include "emul/glob.h"
#include "emul/charclass.h"

#define	DOLLAR		'$'
#define	DOT		'.'
#define	EOS		'\0'
#define	LBRACKET	'['
#define	NOT		'!'
#define	QUESTION	'?'
#define	QUOTE		'\\'
#define	RANGE		'-'
#define	RBRACKET	']'
#define	SEP		'/'
#define	STAR		'*'
#define	TILDE		'~'
#define	UNDERSCORE	'_'
#define	LBRACE		'{'
#define	RBRACE		'}'
#define	SLASH		'/'
#define	COMMA		','

#ifndef DEBUG

#define	M_QUOTE		0x8000
#define	M_PROTECT	0x4000
#define	M_MASK		0xffff
#define	M_ASCII		0x00ff

typedef unsigned short Char;

#else

#define	M_QUOTE		0x80
#define	M_PROTECT	0x40
#define	M_MASK		0xff
#define	M_ASCII		0x7f

typedef char Char;

#endif


#define	CHAR(c)		((Char)((c)&M_ASCII))
#define	META(c)		((Char)((c)|M_QUOTE))
#define	M_ALL		META('*')
#define	M_END		META(']')
#define	M_NOT		META('!')
#define	M_ONE		META('?')
#define	M_RNG		META('-')
#define	M_SET		META('[')
#define	M_CLASS		META(':')
#define	ismeta(c)	(((c)&M_QUOTE) != 0)


static int	 compare __P((const void *, const void *));
static int	 g_Ctoc __P((const Char *, char *, unsigned int));
static int	 g_lstat __P((Char *, struct stat *, glob_t *));
static DIR	*g_opendir __P((Char *, glob_t *));
static Char	*g_strchr __P((const Char *, int));
static int	 g_strncmp __P((const Char *, const char *, size_t));
static int	 g_stat __P((Char *, struct stat *, glob_t *));
static int	 glob0 __P((const Char *, glob_t *));
static int	 glob1 __P((Char *, Char *, glob_t *));
static int	 glob2 __P((Char *, Char *, Char *, Char *, Char *, Char *,
		    glob_t *));
static int	 glob3 __P((Char *, Char *, Char *, Char *, Char *, Char *,
		    Char *, Char *, glob_t *));
static int	 globextend __P((const Char *, glob_t *));
static const Char *
		 globtilde __P((const Char *, Char *, size_t, glob_t *));
static int	 globexp1 __P((const Char *, glob_t *));
static int	 globexp2 __P((const Char *, const Char *, glob_t *, int *));
static int	 match __P((Char *, Char *, Char *));
#ifdef DEBUG
static void	 qprintf __P((const char *, Char *));
#endif

extern struct passwd *sudo_getpwnam __P((const char *));
extern struct passwd *sudo_getpwuid __P((uid_t));

int
glob(pattern, flags, errfunc, pglob)
	const char *pattern;
	int flags, (*errfunc) __P((const char *, int));
	glob_t *pglob;
{
	const unsigned char *patnext;
	int c;
	Char *bufnext, *bufend, patbuf[PATH_MAX];

	patnext = (unsigned char *) pattern;
	if (!(flags & GLOB_APPEND)) {
		pglob->gl_pathc = 0;
		pglob->gl_pathv = NULL;
		if (!(flags & GLOB_DOOFFS))
			pglob->gl_offs = 0;
	}
	pglob->gl_flags = flags & ~GLOB_MAGCHAR;
	pglob->gl_errfunc = errfunc;
	pglob->gl_matchc = 0;

	bufnext = patbuf;
	bufend = bufnext + PATH_MAX - 1;
	if (flags & GLOB_NOESCAPE)
		while (bufnext < bufend && (c = *patnext++) != EOS)
			*bufnext++ = c;
	else {
		/* Protect the quoted characters. */
		while (bufnext < bufend && (c = *patnext++) != EOS)
			if (c == QUOTE) {
				if ((c = *patnext++) == EOS) {
					c = QUOTE;
					--patnext;
				}
				*bufnext++ = c | M_PROTECT;
			} else
				*bufnext++ = c;
	}
	*bufnext = EOS;

	if (flags & GLOB_BRACE)
		return globexp1(patbuf, pglob);
	else
		return glob0(patbuf, pglob);
}

/*
 * Expand recursively a glob {} pattern. When there is no more expansion
 * invoke the standard globbing routine to glob the rest of the magic
 * characters
 */
static int
globexp1(pattern, pglob)
	const Char *pattern;
	glob_t *pglob;
{
	const Char* ptr = pattern;
	int rv;

	/* Protect a single {}, for find(1), like csh */
	if (pattern[0] == LBRACE && pattern[1] == RBRACE && pattern[2] == EOS)
		return glob0(pattern, pglob);

	while ((ptr = (const Char *) g_strchr(ptr, LBRACE)) != NULL)
		if (!globexp2(ptr, pattern, pglob, &rv))
			return rv;

	return glob0(pattern, pglob);
}


/*
 * Recursive brace globbing helper. Tries to expand a single brace.
 * If it succeeds then it invokes globexp1 with the new pattern.
 * If it fails then it tries to glob the rest of the pattern and returns.
 */
static int
globexp2(ptr, pattern, pglob, rv)
	const Char *ptr, *pattern;
	glob_t *pglob;
	int *rv;
{
	int     i;
	Char   *lm, *ls;
	const Char *pe, *pm, *pl;
	Char    patbuf[PATH_MAX];

	/* copy part up to the brace */
	for (lm = patbuf, pm = pattern; pm != ptr; *lm++ = *pm++)
		continue;
	*lm = EOS;
	ls = lm;

	/* Find the balanced brace */
	for (i = 0, pe = ++ptr; *pe; pe++)
		if (*pe == LBRACKET) {
			/* Ignore everything between [] */
			for (pm = pe++; *pe != RBRACKET && *pe != EOS; pe++)
				continue;
			if (*pe == EOS) {
				/*
				 * We could not find a matching RBRACKET.
				 * Ignore and just look for RBRACE
				 */
				pe = pm;
			}
		} else if (*pe == LBRACE)
			i++;
		else if (*pe == RBRACE) {
			if (i == 0)
				break;
			i--;
		}

	/* Non matching braces; just glob the pattern */
	if (i != 0 || *pe == EOS) {
		*rv = glob0(patbuf, pglob);
		return 0;
	}

	for (i = 0, pl = pm = ptr; pm <= pe; pm++) {
		switch (*pm) {
		case LBRACKET:
			/* Ignore everything between [] */
			for (pl = pm++; *pm != RBRACKET && *pm != EOS; pm++)
				continue;
			if (*pm == EOS) {
				/*
				 * We could not find a matching RBRACKET.
				 * Ignore and just look for RBRACE
				 */
				pm = pl;
			}
			break;

		case LBRACE:
			i++;
			break;

		case RBRACE:
			if (i) {
				i--;
				break;
			}
			/* FALLTHROUGH */
		case COMMA:
			if (i && *pm == COMMA)
				break;
			else {
				/* Append the current string */
				for (lm = ls; (pl < pm); *lm++ = *pl++)
					continue;

				/*
				 * Append the rest of the pattern after the
				 * closing brace
				 */
				for (pl = pe + 1; (*lm++ = *pl++) != EOS; )
					continue;

				/* Expand the current pattern */
#ifdef DEBUG
				qprintf("globexp2:", patbuf);
#endif
				*rv = globexp1(patbuf, pglob);

				/* move after the comma, to the next string */
				pl = pm + 1;
			}
			break;

		default:
			break;
		}
	}
	*rv = 0;
	return 0;
}



/*
 * expand tilde from the passwd file.
 */
static const Char *
globtilde(pattern, patbuf, patbuf_len, pglob)
	const Char *pattern;
	Char *patbuf;
	size_t patbuf_len;
	glob_t *pglob;
{
	struct passwd *pwd;
	char *h;
	const Char *p;
	Char *b, *eb;

	if (*pattern != TILDE || !(pglob->gl_flags & GLOB_TILDE))
		return pattern;

	/* Copy up to the end of the string or / */
	eb = &patbuf[patbuf_len - 1];
	for (p = pattern + 1, h = (char *) patbuf;
	    h < (char *)eb && *p && *p != SLASH; *h++ = *p++)
		continue;

	*h = EOS;

	if (((char *) patbuf)[0] == EOS) {
		/*
		 * handle a plain ~ or ~/ by expanding $HOME
		 * first and then trying the password file
		 */
		if ((h = getenv("HOME")) == NULL) {
			if ((pwd = sudo_getpwuid(getuid())) == NULL)
				return pattern;
			else
				h = pwd->pw_dir;
		}
	} else {
		/*
		 * Expand a ~user
		 */
		if ((pwd = sudo_getpwnam((char*) patbuf)) == NULL)
			return pattern;
		else
			h = pwd->pw_dir;
	}

	/* Copy the home directory */
	for (b = patbuf; b < eb && *h; *b++ = *h++)
		continue;

	/* Append the rest of the pattern */
	while (b < eb && (*b++ = *p++) != EOS)
		continue;
	*b = EOS;

	return patbuf;
}

static int
g_strncmp(s1, s2, n)
	const Char *s1;
	const char *s2;
	size_t n;
{
	int rv = 0;

	while (n--) {
		rv = *(Char *)s1 - *(const unsigned char *)s2++;
		if (rv)
			break;
		if (*s1++ == '\0')
			break;
	}
	return rv;
}

static int
g_charclass(patternp, bufnextp)
	const Char **patternp;
	Char **bufnextp;
{
	const Char *pattern = *patternp + 1;
	Char *bufnext = *bufnextp;
	const Char *colon;
	struct cclass *cc;
	size_t len;

	if ((colon = g_strchr(pattern, ':')) == NULL || colon[1] != ']')
		return 1;	/* not a character class */

	len = (size_t)(colon - pattern);
	for (cc = cclasses; cc->name != NULL; cc++) {
		if (!g_strncmp(pattern, cc->name, len) && cc->name[len] == '\0')
			break;
	}
	if (cc->name == NULL)
		return -1;	/* invalid character class */
	*bufnext++ = M_CLASS;
	*bufnext++ = (Char)(cc - &cclasses[0]);
	*bufnextp = bufnext;
	*patternp += len + 3;

	return 0;
}

/*
 * The main glob() routine: compiles the pattern (optionally processing
 * quotes), calls glob1() to do the real pattern matching, and finally
 * sorts the list (unless unsorted operation is requested).  Returns 0
 * if things went well, nonzero if errors occurred.  It is not an error
 * to find no matches.
 */
static int
glob0(pattern, pglob)
	const Char *pattern;
	glob_t *pglob;
{
	const Char *qpatnext;
	int c, err, oldpathc;
	Char *bufnext, patbuf[PATH_MAX];

	qpatnext = globtilde(pattern, patbuf, PATH_MAX, pglob);
	oldpathc = pglob->gl_pathc;
	bufnext = patbuf;

	/* We don't need to check for buffer overflow any more. */
	while ((c = *qpatnext++) != EOS) {
		switch (c) {
		case LBRACKET:
			c = *qpatnext;
			if (c == NOT)
				++qpatnext;
			if (*qpatnext == EOS ||
			    g_strchr(qpatnext+1, RBRACKET) == NULL) {
				*bufnext++ = LBRACKET;
				if (c == NOT)
					--qpatnext;
				break;
			}
			*bufnext++ = M_SET;
			if (c == NOT)
				*bufnext++ = M_NOT;
			c = *qpatnext++;
			do {
				if (c == LBRACKET && *qpatnext == ':') {
					do {
						err = g_charclass(&qpatnext,
						    &bufnext);
						if (err)
							break;
						c = *qpatnext++;
					} while (c == LBRACKET && *qpatnext == ':');
					if (err == -1 &&
					    !(pglob->gl_flags & GLOB_NOCHECK))
						return GLOB_NOMATCH;
					if (c == RBRACKET)
						break;
				}
				*bufnext++ = CHAR(c);
				if (*qpatnext == RANGE &&
				    (c = qpatnext[1]) != RBRACKET) {
					*bufnext++ = M_RNG;
					*bufnext++ = CHAR(c);
					qpatnext += 2;
				}
			} while ((c = *qpatnext++) != RBRACKET);
			pglob->gl_flags |= GLOB_MAGCHAR;
			*bufnext++ = M_END;
			break;
		case QUESTION:
			pglob->gl_flags |= GLOB_MAGCHAR;
			*bufnext++ = M_ONE;
			break;
		case STAR:
			pglob->gl_flags |= GLOB_MAGCHAR;
			/* collapse adjacent stars to one,
			 * to avoid exponential behavior
			 */
			if (bufnext == patbuf || bufnext[-1] != M_ALL)
				*bufnext++ = M_ALL;
			break;
		default:
			*bufnext++ = CHAR(c);
			break;
		}
	}
	*bufnext = EOS;
#ifdef DEBUG
	qprintf("glob0:", patbuf);
#endif

	if ((err = glob1(patbuf, patbuf + PATH_MAX - 1, pglob)) != 0)
		return(err);

	/*
	 * If there was no match we are going to append the pattern
	 * if GLOB_NOCHECK was specified.
	 */
	if (pglob->gl_pathc == oldpathc) {
		if (pglob->gl_flags & GLOB_NOCHECK)
			return(globextend(pattern, pglob));
		else
			return(GLOB_NOMATCH);
	}
	if (!(pglob->gl_flags & GLOB_NOSORT))
		qsort(pglob->gl_pathv + pglob->gl_offs + oldpathc,
		    pglob->gl_pathc - oldpathc, sizeof(char *), compare);
	return(0);
}

static int
compare(p, q)
	const void *p, *q;
{
	return(strcmp(*(char **)p, *(char **)q));
}

static int
glob1(pattern, pattern_last,  pglob)
	Char *pattern, *pattern_last;
	glob_t *pglob;
{
	Char pathbuf[PATH_MAX];

	/* A null pathname is invalid -- POSIX 1003.1 sect. 2.4. */
	if (*pattern == EOS)
		return(0);
	return(glob2(pathbuf, pathbuf + PATH_MAX - 1,
	    pathbuf, pathbuf + PATH_MAX - 1,
	    pattern, pattern_last, pglob));
}

/*
 * The functions glob2 and glob3 are mutually recursive; there is one level
 * of recursion for each segment in the pattern that contains one or more
 * meta characters.
 */
static int
glob2(pathbuf, pathbuf_last, pathend, pathend_last, pattern, pattern_last, pglob)
	Char *pathbuf, *pathbuf_last;
	Char *pathend, *pathend_last;
	Char *pattern, *pattern_last;
	glob_t *pglob;
{
	struct stat sb;
	Char *p, *q;
	int anymeta;

	/*
	 * Loop over pattern segments until end of pattern or until
	 * segment with meta character found.
	 */
	for (anymeta = 0;;) {
		if (*pattern == EOS) {		/* End of pattern? */
			*pathend = EOS;
			if (g_lstat(pathbuf, &sb, pglob))
				return(0);

			if (((pglob->gl_flags & GLOB_MARK) &&
			    pathend[-1] != SEP) && (S_ISDIR(sb.st_mode) ||
			    (S_ISLNK(sb.st_mode) &&
			    (g_stat(pathbuf, &sb, pglob) == 0) &&
			    S_ISDIR(sb.st_mode)))) {
				if (pathend+1 > pathend_last)
					return (1);
				*pathend++ = SEP;
				*pathend = EOS;
			}
			++pglob->gl_matchc;
			return(globextend(pathbuf, pglob));
		}

		/* Find end of next segment, copy tentatively to pathend. */
		q = pathend;
		p = pattern;
		while (*p != EOS && *p != SEP) {
			if (ismeta(*p))
				anymeta = 1;
			if (q+1 > pathend_last)
				return (1);
			*q++ = *p++;
		}

		if (!anymeta) {		/* No expansion, do next segment. */
			pathend = q;
			pattern = p;
			while (*pattern == SEP) {
				if (pathend+1 > pathend_last)
					return (1);
				*pathend++ = *pattern++;
			}
		} else
			/* Need expansion, recurse. */
			return(glob3(pathbuf, pathbuf_last, pathend,
			    pathend_last, pattern, pattern_last,
			    p, pattern_last, pglob));
	}
	/* NOTREACHED */
}

static int
glob3(pathbuf, pathbuf_last, pathend, pathend_last, pattern, pattern_last,
    restpattern, restpattern_last, pglob)
	Char *pathbuf, *pathbuf_last, *pathend, *pathend_last;
	Char *pattern, *pattern_last, *restpattern, *restpattern_last;
	glob_t *pglob;
{
	struct dirent *dp;
	DIR *dirp;
	int err;
	char buf[PATH_MAX];

	if (pathend > pathend_last)
		return (1);
	*pathend = EOS;
	errno = 0;

	if ((dirp = g_opendir(pathbuf, pglob)) == NULL) {
		/* TODO: don't call for ENOENT or ENOTDIR? */
		if (pglob->gl_errfunc) {
			if (g_Ctoc(pathbuf, buf, sizeof(buf)))
				return(GLOB_ABORTED);
			if (pglob->gl_errfunc(buf, errno) ||
			    pglob->gl_flags & GLOB_ERR)
				return(GLOB_ABORTED);
		}
		return(0);
	}

	err = 0;

	/* Search directory for matching names. */
	while ((dp = readdir(dirp))) {
		unsigned char *sc;
		Char *dc;

		/* Initial DOT must be matched literally. */
		if (dp->d_name[0] == DOT && *pattern != DOT)
			continue;
		dc = pathend;
		sc = (unsigned char *) dp->d_name;
		while (dc < pathend_last && (*dc++ = *sc++) != EOS)
			continue;
		if (dc >= pathend_last) {
			*dc = EOS;
			err = 1;
			break;
		}

		if (!match(pathend, pattern, restpattern)) {
			*pathend = EOS;
			continue;
		}
		err = glob2(pathbuf, pathbuf_last, --dc, pathend_last,
		    restpattern, restpattern_last, pglob);
		if (err)
			break;
	}

	closedir(dirp);
	return(err);
}

/*
 * Extend the gl_pathv member of a glob_t structure to accommodate a new item,
 * add the new item, and update gl_pathc.
 *
 * This assumes the BSD realloc, which only copies the block when its size
 * crosses a power-of-two boundary; for v7 realloc, this would cause quadratic
 * behavior.
 *
 * Return 0 if new item added, error code if memory couldn't be allocated.
 *
 * Invariant of the glob_t structure:
 *	Either gl_pathc is zero and gl_pathv is NULL; or gl_pathc > 0 and
 *	gl_pathv points to (gl_offs + gl_pathc + 1) items.
 */
static int
globextend(path, pglob)
	const Char *path;
	glob_t *pglob;
{
	char **pathv;
	int i;
	unsigned int newsize, len;
	char *copy;
	const Char *p;

	newsize = sizeof(*pathv) * (2 + pglob->gl_pathc + pglob->gl_offs);
	pathv = pglob->gl_pathv ?
	    (char **)realloc((char *)pglob->gl_pathv, newsize) :
	    (char **)malloc(newsize);
	if (pathv == NULL) {
		if (pglob->gl_pathv) {
			free(pglob->gl_pathv);
			pglob->gl_pathv = NULL;
		}
		return(GLOB_NOSPACE);
	}

	if (pglob->gl_pathv == NULL && pglob->gl_offs > 0) {
		/* first time around -- clear initial gl_offs items */
		pathv += pglob->gl_offs;
		for (i = pglob->gl_offs; --i >= 0; )
			*--pathv = NULL;
	}
	pglob->gl_pathv = pathv;

	for (p = path; *p++;)
		continue;
	len = (size_t)(p - path);
	if ((copy = malloc(len)) != NULL) {
		if (g_Ctoc(path, copy, len)) {
			free(copy);
			return(GLOB_NOSPACE);
		}
		pathv[pglob->gl_offs + pglob->gl_pathc++] = copy;
	}
	pathv[pglob->gl_offs + pglob->gl_pathc] = NULL;

	return(copy == NULL ? GLOB_NOSPACE : 0);
}

/*
 * pattern matching function for filenames.  Each occurrence of the *
 * pattern causes a recursion level.
 */
static int
match(name, pat, patend)
	Char *name, *pat, *patend;
{
	int ok, negate_range;
	Char c, k;

	while (pat < patend) {
		c = *pat++;
		switch (c & M_MASK) {
		case M_ALL:
			if (pat == patend)
				return(1);
			do {
			    if (match(name, pat, patend))
				    return(1);
			} while (*name++ != EOS);
			return(0);
		case M_ONE:
			if (*name++ == EOS)
				return(0);
			break;
		case M_SET:
			ok = 0;
			if ((k = *name++) == EOS)
				return(0);
			if ((negate_range = ((*pat & M_MASK) == M_NOT)) != EOS)
				++pat;
			while (((c = *pat++) & M_MASK) != M_END) {
				if ((c & M_MASK) == M_CLASS) {
					int idx = *pat & M_MASK;
					if (idx < NCCLASSES &&
					    cclasses[idx].isctype(k))
						ok = 1;
					++pat;
				}
				if ((*pat & M_MASK) == M_RNG) {
					if (c <= k && k <= pat[1])
						ok = 1;
					pat += 2;
				} else if (c == k)
					ok = 1;
			}
			if (ok == negate_range)
				return(0);
			break;
		default:
			if (*name++ != c)
				return(0);
			break;
		}
	}
	return(*name == EOS);
}

/* Free allocated data belonging to a glob_t structure. */
void
globfree(pglob)
	glob_t *pglob;
{
	int i;
	char **pp;

	if (pglob->gl_pathv != NULL) {
		pp = pglob->gl_pathv + pglob->gl_offs;
		for (i = pglob->gl_pathc; i--; ++pp)
			if (*pp)
				free(*pp);
		free(pglob->gl_pathv);
		pglob->gl_pathv = NULL;
	}
}

static DIR *
g_opendir(str, pglob)
	Char *str;
	glob_t *pglob;
{
	char buf[PATH_MAX];

	if (!*str) {
		buf[0] = '.';
		buf[1] = '\0';
	} else {
		if (g_Ctoc(str, buf, sizeof(buf)))
			return(NULL);
	}
	return(opendir(buf));
}

static int
g_lstat(fn, sb, pglob)
	Char *fn;
	struct stat *sb;
	glob_t *pglob;
{
	char buf[PATH_MAX];

	if (g_Ctoc(fn, buf, sizeof(buf)))
		return(-1);
	return(lstat(buf, sb));
}

static int
g_stat(fn, sb, pglob)
	Char *fn;
	struct stat *sb;
	glob_t *pglob;
{
	char buf[PATH_MAX];

	if (g_Ctoc(fn, buf, sizeof(buf)))
		return(-1);
	return(stat(buf, sb));
}

static Char *
g_strchr(str, ch)
	const Char *str;
	int ch;
{
	do {
		if (*str == ch)
			return ((Char *)str);
	} while (*str++);
	return (NULL);
}

static int
g_Ctoc(str, buf, len)
	const Char *str;
	char *buf;
	unsigned int len;
{

	while (len--) {
		if ((*buf++ = *str++) == EOS)
			return (0);
	}
	return (1);
}

#ifdef DEBUG
static void
qprintf(str, s)
	const char *str;
	Char *s;
{
	Char *p;

	(void)printf("%s:\n", str);
	for (p = s; *p; p++)
		(void)printf("%c", CHAR(*p));
	(void)printf("\n");
	for (p = s; *p; p++)
		(void)printf("%c", *p & M_PROTECT ? '"' : ' ');
	(void)printf("\n");
	for (p = s; *p; p++)
		(void)printf("%c", ismeta(*p) ? '_' : ' ');
	(void)printf("\n");
}
#endif
