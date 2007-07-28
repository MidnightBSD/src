/* $MidnightBSD$ */
/*
 *
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
 * This is the add module.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.sbin/pkg_install/add/main.c,v 1.61.2.4 2006/01/16 19:48:17 flz Exp $");

#include <err.h>
#include <sys/param.h>
#include <sys/utsname.h>
#include "lib.h"
#include "add.h"

static char Options[] = "hvIRfnrp:P:SMt:C:K";

char	*Prefix		= NULL;
Boolean	PrefixRecursive	= FALSE;
char	*Chroot		= NULL;
Boolean	NoInstall	= FALSE;
Boolean	NoRecord	= FALSE;
Boolean Remote		= FALSE;
Boolean KeepPackage	= FALSE;

char	*Mode		= NULL;
char	*Owner		= NULL;
char	*Group		= NULL;
char	*PkgName	= NULL;
char	*PkgAddCmd	= NULL;
char	*Directory	= NULL;
char	FirstPen[FILENAME_MAX];
add_mode_t AddMode	= NORMAL;

char	**pkgs;

struct {
	int lowver;	/* Lowest version number to match */
	int hiver;	/* Highest version number to match */
	const char *directory;	/* Directory it lives in */
} releases[] = {
	{ 0, 9999999, "/current" },
	{ 0, 0, NULL }
};

static char *getpackagesite(void);
int getosreldate(void);

static void usage __P((void));

int
main(int argc, char **argv)
{
    int ch, error;
    char **start;
    char *cp, *packagesite = NULL, *remotepkg = NULL, *ptr;
    static char temppackageroot[MAXPATHLEN];
    static char pkgaddpath[MAXPATHLEN];

    if (*argv[0] != '/' && strchr(argv[0], '/') != NULL)
	PkgAddCmd = realpath(argv[0], pkgaddpath);
    else
	PkgAddCmd = argv[0];

    start = argv;
    while ((ch = getopt(argc, argv, Options)) != -1) {
	switch(ch) {
	case 'v':
	    Verbose = TRUE;
	    break;

	case 'p':
	    Prefix = optarg;
	    PrefixRecursive = FALSE;
	    break;

	case 'P':
	    Prefix = optarg;
	    PrefixRecursive = TRUE;
	    break;

	case 'I':
	    NoInstall = TRUE;
	    break;

	case 'R':
	    NoRecord = TRUE;
	    break;

	case 'f':
	    Force = TRUE;
	    break;

	case 'K':
	    KeepPackage = TRUE;
	    break;

	case 'n':
	    Fake = TRUE;
	    break;

	case 'r':
	    Remote = TRUE;
	    break;

	case 't':
	    if (strlcpy(FirstPen, optarg, sizeof(FirstPen)) >= sizeof(FirstPen))
		errx(1, "-t Argument too long.");
	    break;

	case 'S':
	    AddMode = SLAVE;
	    break;

	case 'M':
	    AddMode = MASTER;
	    break;

	case 'C':
	    Chroot = optarg;
	    break;

	case 'h':
	case '?':
	default:
	    usage();
	    break;
	}
    }
    argc -= optind;
    argv += optind;

    if (AddMode != SLAVE) {
	pkgs = (char **)malloc((argc + 1) * sizeof(char *));
	for (ch = 0; ch <= argc; pkgs[ch++] = NULL) ;

	/* Get all the remaining package names, if any */
	for (ch = 0; *argv; ch++, argv++) {
	    char temp[MAXPATHLEN];
    	    if (Remote) {
		if ((packagesite = getpackagesite()) == NULL)
		    errx(1, "package name too long");
		if (strlcpy(temppackageroot, packagesite,
		    sizeof(temppackageroot)) >= sizeof(temppackageroot))
		    errx(1, "package name too long");
		if (strlcat(temppackageroot, *argv, sizeof(temppackageroot))
		    >= sizeof(temppackageroot))
		    errx(1, "package name too long");
		remotepkg = temppackageroot;
		if (!((ptr = strrchr(remotepkg, '.')) && ptr[1] == 't' && 
			(ptr[2] == 'b' || ptr[2] == 'g') && ptr[3] == 'z' &&
			!ptr[4]))
		    if (strlcat(remotepkg,
			".tbz",
			sizeof(temppackageroot)) >= sizeof(temppackageroot))
			errx(1, "package name too long");
    	    }
	    if (!strcmp(*argv, "-"))	/* stdin? */
		pkgs[ch] = (char *)"-";
	    else if (isURL(*argv)) {  	/* preserve URLs */
		if (strlcpy(temp, *argv, sizeof(temp)) >= sizeof(temp))
		    errx(1, "package name too long");
		pkgs[ch] = strdup(temp);
	    }
	    else if ((Remote) && isURL(remotepkg)) {
	    	if (strlcpy(temp, remotepkg, sizeof(temp)) >= sizeof(temp))
		    errx(1, "package name too long");
		pkgs[ch] = strdup(temp);
	    } else {			/* expand all pathnames to fullnames */
		if (fexists(*argv)) /* refers to a file directly */
		    pkgs[ch] = strdup(realpath(*argv, temp));
		else {		/* look for the file in the expected places */
		    if (!(cp = fileFindByPath(NULL, *argv))) {
			/* let pkg_do() fail later, so that error is reported */
			if (strlcpy(temp, *argv, sizeof(temp)) >= sizeof(temp))
			    errx(1, "package name too long");
			pkgs[ch] = strdup(temp);
		    } else {
			if (strlcpy(temp, cp, sizeof(temp)) >= sizeof(temp))
			    errx(1, "package name too long");
			pkgs[ch] = strdup(temp);
		    }
		}
	    }
	    if (packagesite != NULL)
		packagesite[0] = '\0';
	}
    }
    /* If no packages, yelp */
    else if (!ch) {
	warnx("missing package name(s)");
	usage();
    }
    else if (ch > 1 && AddMode == MASTER) {
	warnx("only one package name may be specified with master mode");
	usage();
    }
    /* Perform chroot if requested */
    if (Chroot != NULL) {
	if (chroot(Chroot))
	    errx(1, "chroot to %s failed", Chroot);
    }
    /* Make sure the sub-execs we invoke get found */
    setenv("PATH", 
	   "/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin:/usr/X11R6/bin",
	   1);

    /* Set a reasonable umask */
    umask(022);

    if ((error = pkg_perform(pkgs)) != 0) {
	if (Verbose)
	    warnx("%d package addition(s) failed", error);
	return error;
    }
    else
	return 0;
}

static char *
getpackagesite(void)
{
    int reldate, i;
    static char sitepath[MAXPATHLEN];
    struct utsname u;

    if (getenv("PACKAGESITE")) {
	if (strlcpy(sitepath, getenv("PACKAGESITE"), sizeof(sitepath))
	    >= sizeof(sitepath))
	    return NULL;
	return sitepath;
    }

    if (getenv("PACKAGEROOT")) {
	if (strlcpy(sitepath, getenv("PACKAGEROOT"), sizeof(sitepath))
	    >= sizeof(sitepath))
	    return NULL;
    } else {
	if (strlcat(sitepath, "ftp://ftp.midnightbsd.org", sizeof(sitepath))
	    >= sizeof(sitepath))
	    return NULL;
    }

    if (strlcat(sitepath, "/pub/MidnightBSD/mports/packages/", sizeof(sitepath))
	>= sizeof(sitepath))
	return NULL;

    uname(&u);
    if (strlcat(sitepath, u.machine, sizeof(sitepath)) >= sizeof(sitepath))
	return NULL;

    reldate = getosreldate();
    for(i = 0; releases[i].directory != NULL; i++) {
	if (reldate >= releases[i].lowver && reldate <= releases[i].hiver) {
	    if (strlcat(sitepath, releases[i].directory, sizeof(sitepath))
		>= sizeof(sitepath))
		return NULL;
	    break;
	}
    }

    if (strlcat(sitepath, "/Latest/", sizeof(sitepath)) >= sizeof(sitepath))
	return NULL;

    return sitepath;

}

static void
usage()
{
    fprintf(stderr, "%s\n%s\n",
	"usage: pkg_add [-vInrfRMSK] [-t template] [-p prefix] [-P prefix] [-C chrootdir]",
	"               pkg-name [pkg-name ...]");
    exit(1);
}
