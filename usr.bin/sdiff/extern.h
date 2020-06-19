/*	$OpenBSD: extern.h,v 1.5 2009/06/07 13:29:50 ray Exp $ */
/*	$FreeBSD: stable/11/usr.bin/sdiff/extern.h 298823 2016-04-29 23:27:15Z bapt $	*/

/*
 * Written by Raymond Lai <ray@cyth.net>.
 * Public domain.
 */

extern FILE		*outfp;		/* file to save changes to */
extern const char	*tmpdir;

int eparse(const char *, const char *, const char *);
