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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/sysctl.h>
#include <err.h>
#include <getopt.h>

#include "lib.h"
#include "add.h"

char	*Prefix		= NULL;
Boolean	PrefixRecursive	= FALSE;
char	*Chroot		= NULL;
Boolean	NoInstall	= FALSE;
Boolean	NoRecord	= FALSE;
Boolean Remote		= FALSE;
Boolean KeepPackage	= FALSE;
Boolean FailOnAlreadyInstalled	= TRUE;
Boolean IgnoreDeps	= FALSE;

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
	{ 410000, 410000, "/packages-4.1-release" },
	{ 420000, 420000, "/packages-4.2-release" },
	{ 430000, 430000, "/packages-4.3-release" },
	{ 440000, 440000, "/packages-4.4-release" },
	{ 450000, 450000, "/packages-4.5-release" },
	{ 460000, 460001, "/packages-4.6-release" },
	{ 460002, 460099, "/packages-4.6.2-release" },
	{ 470000, 470099, "/packages-4.7-release" },
	{ 480000, 480099, "/packages-4.8-release" },
	{ 490000, 490099, "/packages-4.9-release" },
	{ 491000, 491099, "/packages-4.10-release" },
	{ 492000, 492099, "/packages-4.11-release" },
	{ 500000, 500099, "/packages-5.0-release" },
	{ 501000, 501099, "/packages-5.1-release" },
	{ 502000, 502009, "/packages-5.2-release" },
	{ 502010, 502099, "/packages-5.2.1-release" },
	{ 503000, 503099, "/packages-5.3-release" },
	{ 504000, 504099, "/packages-5.4-release" },
	{ 505000, 505099, "/packages-5.5-release" },
	{ 600000, 600099, "/packages-6.0-release" },
	{ 601000, 601099, "/packages-6.1-release" },
	{ 602000, 602099, "/packages-6.2-release" },
	{ 603000, 603099, "/packages-6.3-release" },
	{ 604000, 604099, "/packages-6.4-release" },
	{ 700000, 700099, "/packages-7.0-release" },
	{ 701000, 701099, "/packages-7.1-release" },
	{ 702000, 702099, "/packages-7.2-release" },
	{ 703000, 703099, "/packages-7.3-release" },
	{ 704000, 704099, "/packages-7.4-release" },
	{ 800000, 800499, "/packages-8.0-release" },
	{ 801000, 801499, "/packages-8.1-release" },
	{ 802000, 802499, "/packages-8.2-release" },
	{ 803000, 803499, "/packages-8.3-release" },
	{ 900000, 900499, "/packages-9.0-release" },
	{ 901000, 901499, "/packages-9.1-release" },
	{ 300000, 399000, "/packages-3-stable" },
	{ 400000, 499000, "/packages-4-stable" },
	{ 502100, 502128, "/packages-5-current" },
	{ 503100, 599000, "/packages-5-stable" },
	{ 600100, 699000, "/packages-6-stable" },
	{ 700100, 799000, "/packages-7-stable" },
	{ 800500, 899000, "/packages-8-stable" },
	{ 900500, 999000, "/packages-9-stable" },
	{ 1000000, 1099000, "/packages-10-current" },
	{ 0, 9999999, "/packages-current" },
	{ 0, 0, NULL }
};

static char *getpackagesite(void);
int getosreldate(void);

static void usage(void);

static char opts[] = "hviIRfFnrp:P:SMt:C:K";
static struct option longopts[] = {
	{ "chroot",	required_argument,	NULL,		'C' },
	{ "dry-run",	no_argument,		NULL,		'n' },
	{ "force",	no_argument,		NULL,		'f' },
	{ "help",	no_argument,		NULL,		'h' },
	{ "keep",	no_argument,		NULL,		'K' },
	{ "master",	no_argument,		NULL,		'M' },
	{ "no-deps",	no_argument,		NULL,		'i' },
	{ "no-record",	no_argument,		NULL,		'R' },
	{ "no-script",	no_argument,		NULL,		'I' },
	{ "prefix",	required_argument,	NULL,		'p' },
	{ "remote",	no_argument,		NULL,		'r' },
	{ "template",	required_argument,	NULL,		't' },
	{ "slave",	no_argument,		NULL,		'S' },
	{ "verbose",	no_argument,		NULL,		'v' },
	{ NULL,		0,			NULL,		0 }
};

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
    while ((ch = getopt_long(argc, argv, opts, longopts, NULL)) != -1) {
	switch(ch) {
	case 'v':
	    Verbose++;
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

	case 'F':
	    FailOnAlreadyInstalled = FALSE;
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

	case 'i':
	    IgnoreDeps = TRUE;
	    break;

	case 'h':
	default:
	    usage();
	    break;
	}
    }
    argc -= optind;
    argv += optind;

    if (AddMode != SLAVE) {
	pkgs = (char **)malloc((argc+1) * sizeof(char *));
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
			(ptr[2] == 'b' || ptr[2] == 'g' || ptr[2] == 'x') &&
			ptr[3] == 'z' && !ptr[4])) {
    		    if (getenv("PACKAGESUFFIX")) {
		       if (strlcat(remotepkg, getenv("PACKAGESUFFIX"),
			   sizeof(temppackageroot)) >= sizeof(temppackageroot))
			   errx(1, "package name too long");
		    } else {
		       if (strlcat(remotepkg, ".tbz",
			   sizeof(temppackageroot)) >= sizeof(temppackageroot))
			   errx(1, "package name too long");
		    }
		}
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
    if (!ch) {
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
	   "/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin",
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
    int archmib[] = { CTL_HW, HW_MACHINE_ARCH };
    char arch[64];
    size_t archlen = sizeof(arch);

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
	if (strlcat(sitepath, "ftp://ftp.freebsd.org", sizeof(sitepath))
	    >= sizeof(sitepath))
	    return NULL;
    }

    if (strlcat(sitepath, "/pub/FreeBSD/ports/", sizeof(sitepath))
	>= sizeof(sitepath))
	return NULL;

    if (sysctl(archmib, 2, arch, &archlen, NULL, 0) == -1)
	return NULL;
    arch[archlen-1] = 0;
    if (strlcat(sitepath, arch, sizeof(sitepath)) >= sizeof(sitepath))
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
usage(void)
{
    fprintf(stderr, "%s\n%s\n",
	"usage: pkg_add [-viInfFrRMSK] [-t template] [-p prefix] [-P prefix] [-C chrootdir]",
	"               pkg-name [pkg-name ...]");
    exit(1);
}
