/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
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
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * Routines for managing the "play pen".
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/usr.sbin/pkg_install/lib/pen.c 132799 2004-07-28 16:03:13Z stefanf $");

#include "lib.h"
#include <err.h>
#include <libgen.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/mount.h>

/* For keeping track of where we are */
static char PenLocation[FILENAME_MAX];
static char Previous[FILENAME_MAX];

char *
where_playpen(void)
{
    return PenLocation;
}

/* Find a good place to play. */
static char *
find_play_pen(char *pen, off_t sz)
{
    char *cp;
    struct stat sb;

    if (pen[0] && isdir(dirname(pen)) == TRUE && (min_free(dirname(pen)) >= sz))
	return pen;
    else if ((cp = getenv("PKG_TMPDIR")) != NULL && stat(cp, &sb) != FAIL && (min_free(cp) >= sz))
	sprintf(pen, "%s/instmp.XXXXXX", cp);
    else if ((cp = getenv("TMPDIR")) != NULL && stat(cp, &sb) != FAIL && (min_free(cp) >= sz))
	sprintf(pen, "%s/instmp.XXXXXX", cp);
    else if (stat("/var/tmp", &sb) != FAIL && min_free("/var/tmp") >= sz)
	strcpy(pen, "/var/tmp/instmp.XXXXXX");
    else if (stat("/tmp", &sb) != FAIL && min_free("/tmp") >= sz)
	strcpy(pen, "/tmp/instmp.XXXXXX");
    else if ((stat("/usr/tmp", &sb) == SUCCESS || mkdir("/usr/tmp", 01777) == SUCCESS) && min_free("/usr/tmp") >= sz)
	strcpy(pen, "/usr/tmp/instmp.XXXXXX");
    else {
	cleanup(0);
	errx(2,
"%s: can't find enough temporary space to extract the files, please set your\n"
"PKG_TMPDIR environment variable to a location with at least %ld bytes\n"
"free", __func__, (long)sz);
	return NULL;
    }
    return pen;
}

#define MAX_STACK	20
static char *pstack[MAX_STACK];
static int pdepth = -1;

static void
pushPen(const char *pen)
{
    if (++pdepth == MAX_STACK)
	errx(2, "%s: stack overflow.\n", __func__);
    pstack[pdepth] = strdup(pen);
}

static void
popPen(char *pen)
{
    if (pdepth == -1) {
	pen[0] = '\0';
	return;
    }
    strcpy(pen, pstack[pdepth]);
    free(pstack[pdepth--]);
}
    
/*
 * Make a temporary directory to play in and chdir() to it, returning
 * pathname of previous working directory.
 */
char *
make_playpen(char *pen, off_t sz)
{
    if (!find_play_pen(pen, sz))
	return NULL;

    if (!mkdtemp(pen)) {
	cleanup(0);
	errx(2, "%s: can't mktemp '%s'", __func__, pen);
    }
    if (chmod(pen, 0700) == FAIL) {
	cleanup(0);
	errx(2, "%s: can't mkdir '%s'", __func__, pen);
    }

    if (Verbose) {
	if (sz)
	    fprintf(stderr, "Requested space: %d bytes, free space: %lld bytes in %s\n", (int)sz, (long long)min_free(pen), pen);
    }

    if (min_free(pen) < sz) {
	rmdir(pen);
	cleanup(0);
	errx(2, "%s: not enough free space to create '%s'.\n"
	     "Please set your PKG_TMPDIR environment variable to a location\n"
	     "with more space and\ntry the command again", __func__, pen);
    }

    if (!getcwd(Previous, FILENAME_MAX)) {
	upchuck("getcwd");
	return NULL;
    }

    if (chdir(pen) == FAIL) {
	cleanup(0);
	errx(2, "%s: can't chdir to '%s'", __func__, pen);
    }

    if (PenLocation[0])
	pushPen(PenLocation);

    strcpy(PenLocation, pen);
    return Previous;
}

/* Convenience routine for getting out of playpen */
void
leave_playpen()
{
    void (*oldsig)(int);

    /* Don't interrupt while we're cleaning up */
    oldsig = signal(SIGINT, SIG_IGN);
    if (Previous[0]) {
	if (chdir(Previous) == FAIL) {
	    cleanup(0);
	    errx(2, "%s: can't chdir back to '%s'", __func__, Previous);
	}
	Previous[0] = '\0';
    }
    if (PenLocation[0]) {
	if (PenLocation[0] == '/' && vsystem("/bin/rm -rf %s", PenLocation))
	    warnx("couldn't remove temporary dir '%s'", PenLocation);
	popPen(PenLocation);
    }
    signal(SIGINT, oldsig);
}

off_t
min_free(const char *tmpdir)
{
    struct statfs buf;

    if (statfs(tmpdir, &buf) != 0) {
	warn("statfs");
	return -1;
    }
    return (off_t)buf.f_bavail * (off_t)buf.f_bsize;
}
