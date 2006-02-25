/* $FreeBSD: src/gnu/usr.bin/patch/version.c,v 1.7 2002/04/28 01:33:45 gad Exp $
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.1.1.1  2005/12/24 02:43:05  laffer1
 * Imported from FreeBSD 6.0 sources
 *
 * Revision 2.0  86/09/17  15:40:11  lwall
 * Baseline for netwide release.
 *
 */

#include "EXTERN.h"
#include "common.h"
#include "util.h"
#include "INTERN.h"
#include "patchlevel.h"
#include "version.h"

void	my_exit(int _status);		/* in patch.c */

/* Print out the version number and die. */

void
version(void)
{
    fprintf(stderr, "Patch version %s\n", PATCH_VERSION);
    my_exit(0);
}
