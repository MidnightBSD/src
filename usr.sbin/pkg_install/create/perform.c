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
 * This is the main body of the create module.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "lib.h"
#include "create.h"

#include <err.h>
#include <libgen.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <sys/wait.h>
#include <unistd.h>

static void sanity_check(void);
static void make_dist(const char *, const char *, const char *, Package *);
static int create_from_installed_recursive(const char *, const char *);
static int create_from_installed(const char *, const char *, const char *);

int
pkg_perform(char **pkgs)
{
    static const char *home;
    char *pkg = *pkgs;		/* Only one arg to create */
    char *cp;
    FILE *pkg_in, *fp;
    Package plist;
    int len;
    const char *suf;

    /* Preliminary setup */
    if (InstalledPkg == NULL)
	sanity_check();
    if (Verbose && !PlistOnly)
	printf("Creating package %s\n", pkg);

    /* chop suffix off if already specified, remembering if we want to compress  */
    len = strlen(pkg);
    if (len > 4) {
	if (!strcmp(&pkg[len - 4], ".tbz")) {
	    Zipper = BZIP2;
	    pkg[len - 4] = '\0';
	}
	else if (!strcmp(&pkg[len - 4], ".tgz")) {
	    Zipper = GZIP;
	    pkg[len - 4] = '\0';
	}
	else if (!strcmp(&pkg[len - 4], ".txz")) {
	    Zipper = XZ;
	    pkg[len - 4] = '\0';
	}
	else if (!strcmp(&pkg[len - 4], ".tar")) {
	    Zipper = NONE;
	    pkg[len - 4] = '\0';
	}
    }
    if (Zipper == BZIP2) {
	suf = "tbz";
	setenv("BZIP2", "--best", 0);
    } else if (Zipper == GZIP) {
	suf = "tgz";
	setenv("GZIP", "-9", 0);
    } else if (Zipper == XZ) {
	suf = "txz";
    } else
	suf = "tar";

    if (InstalledPkg != NULL) {
	char *pkgglob[] = { InstalledPkg, NULL };
	char **matched, **pkgs;
	int i, error;

	pkgs = pkgglob;
	if (MatchType != MATCH_EXACT) {
		matched = matchinstalled(MatchType, pkgs, &error);
		if (!error && matched != NULL)
			pkgs = matched;
		else if (MatchType != MATCH_GLOB)
	    		errx(1, "no packages match pattern");
	}
	/*
	 * Is there is only one installed package matching the pattern,
	 * we need to respect the optional pkg-filename parameter.  If,
	 * however, the pattern matches several packages, this parameter
	 * makes no sense and is ignored.
	 */
	if (pkgs[1] == NULL) {
	    if (pkg == InstalledPkg)
		pkg = *pkgs;
	    InstalledPkg = *pkgs;
	    if (!Recursive)
		return (create_from_installed(InstalledPkg, pkg, suf));
	    return (create_from_installed_recursive(pkg, suf));
	}
	for (i = 0; pkgs[i] != NULL; i++) {
	    InstalledPkg = pkg = pkgs[i];
	    if (!Recursive)
		create_from_installed(pkg, pkg, suf);
	    else
	        create_from_installed_recursive(pkg, suf);
	}
	return TRUE;
    }

    get_dash_string(&Comment);
    get_dash_string(&Desc);
    if (!strcmp(Contents, "-"))
	pkg_in = stdin;
    else {
	pkg_in = fopen(Contents, "r");
	if (!pkg_in) {
	    cleanup(0);
	    errx(2, "%s: unable to open contents file '%s' for input",
		__func__, Contents);
	}
    }
    plist.head = plist.tail = NULL;

    /* Stick the dependencies, if any, at the top */
    if (Pkgdeps) {
	char **deps, *deporigin;
	int i;
	int ndeps = 0;

	if (Verbose && !PlistOnly)
	    printf("Registering depends:");

	/* Count number of dependencies */
	for (cp = Pkgdeps; cp != NULL && *cp != '\0';
			   cp = strpbrk(++cp, " \t\n")) {
	    ndeps++;
	}

	if (ndeps != 0) {
	    /* Create easy to use NULL-terminated list */
	    deps = alloca(sizeof(*deps) * ndeps + 1);
	    if (deps == NULL) {
		errx(2, "%s: alloca() failed", __func__);
		/* Not reached */
	    }
	    for (i = 0; Pkgdeps;) {
		cp = strsep(&Pkgdeps, " \t\n");
		if (*cp) {
		    deps[i] = cp;
		    i++;
		}
	    }
	    ndeps = i;
	    deps[ndeps] = NULL;

	    sortdeps(deps);
	    for (i = 0; i < ndeps; i++) {
		deporigin = strchr(deps[i], ':');
		if (deporigin != NULL) {
		    *deporigin = '\0';
		    add_plist_top(&plist, PLIST_DEPORIGIN, ++deporigin);
		}
		add_plist_top(&plist, PLIST_PKGDEP, deps[i]);
		if (Verbose && !PlistOnly)
		    printf(" %s", deps[i]);
	    }
	}

	if (Verbose && !PlistOnly)
	    printf(".\n");
    }

    /* Put the conflicts directly after the dependencies, if any */
    if (Conflicts) {
	if (Verbose && !PlistOnly)
	    printf("Registering conflicts:");
	while (Conflicts) {
	   cp = strsep(&Conflicts, " \t\n");
	   if (*cp) {
		add_plist(&plist, PLIST_CONFLICTS, cp);
		if (Verbose && !PlistOnly)
		    printf(" %s", cp);
	   }
	}
	if (Verbose && !PlistOnly)
	    printf(".\n");
    }

    /* If a SrcDir override is set, add it now */
    if (SrcDir) {
	if (Verbose && !PlistOnly)
	    printf("Using SrcDir value of %s\n", SrcDir);
	add_plist(&plist, PLIST_SRC, SrcDir);
    }

    /* Slurp in the packing list */
    read_plist(&plist, pkg_in);

    /* Prefix should add an @cwd to the packing list */
    if (Prefix) {
        char resolved_prefix[PATH_MAX];
        if (realpath(Prefix, resolved_prefix) == NULL)
	    err(EXIT_FAILURE, "couldn't resolve path for prefix: %s", Prefix);
	add_plist_top(&plist, PLIST_CWD, resolved_prefix);
    }

    /* Add the origin if asked, at the top */
    if (Origin)
	add_plist_top(&plist, PLIST_ORIGIN, Origin);

    /*
     * Run down the list and see if we've named it, if not stick in a name
     * at the top.
     */
    if (find_plist(&plist, PLIST_NAME) == NULL)
	add_plist_top(&plist, PLIST_NAME, basename(pkg));

    if (asprintf(&cp, "PKG_FORMAT_REVISION:%d.%d", PLIST_FMT_VER_MAJOR,
		 PLIST_FMT_VER_MINOR) == -1) {
	errx(2, "%s: asprintf() failed", __func__);
    }
    add_plist_top(&plist, PLIST_COMMENT, cp);
    free(cp);

    /*
     * We're just here for to dump out a revised plist for the FreeBSD ports
     * hack.  It's not a real create in progress.
     */
    if (PlistOnly) {
	check_list(home, &plist);
	write_plist(&plist, stdout);
	exit(0);
    }

    /* Make a directory to stomp around in */
    home = make_playpen(PlayPen, 0);
    signal(SIGINT, cleanup);
    signal(SIGHUP, cleanup);

    /* Make first "real contents" pass over it */
    check_list(home, &plist);
    (void) umask(022);	/*
			 * Make sure gen'ed directories, files don't have
			 * group or other write bits.
			 */
    /* copy_plist(home, &plist); */
    /* mark_plist(&plist); */

    /* Now put the release specific items in */
    if (!Prefix) {
	add_plist(&plist, PLIST_CWD, ".");
    }
    write_file(COMMENT_FNAME, Comment);
    add_plist(&plist, PLIST_IGNORE, NULL);
    add_plist(&plist, PLIST_FILE, COMMENT_FNAME);
    add_cksum(&plist, plist.tail, COMMENT_FNAME);
    write_file(DESC_FNAME, Desc);
    add_plist(&plist, PLIST_IGNORE, NULL);
    add_plist(&plist, PLIST_FILE, DESC_FNAME);
    add_cksum(&plist, plist.tail, DESC_FNAME);

    if (Install) {
	copy_file(home, Install, INSTALL_FNAME);
	add_plist(&plist, PLIST_IGNORE, NULL);
	add_plist(&plist, PLIST_FILE, INSTALL_FNAME);
	add_cksum(&plist, plist.tail, INSTALL_FNAME);
    }
    if (PostInstall) {
	copy_file(home, PostInstall, POST_INSTALL_FNAME);
	add_plist(&plist, PLIST_IGNORE, NULL);
	add_plist(&plist, PLIST_FILE, POST_INSTALL_FNAME);
	add_cksum(&plist, plist.tail, POST_INSTALL_FNAME);
    }
    if (DeInstall) {
	copy_file(home, DeInstall, DEINSTALL_FNAME);
	add_plist(&plist, PLIST_IGNORE, NULL);
	add_plist(&plist, PLIST_FILE, DEINSTALL_FNAME);
	add_cksum(&plist, plist.tail, DEINSTALL_FNAME);
    }
    if (PostDeInstall) {
	copy_file(home, PostDeInstall, POST_DEINSTALL_FNAME);
	add_plist(&plist, PLIST_IGNORE, NULL);
	add_plist(&plist, PLIST_FILE, POST_DEINSTALL_FNAME);
	add_cksum(&plist, plist.tail, POST_DEINSTALL_FNAME);
    }
    if (Require) {
	copy_file(home, Require, REQUIRE_FNAME);
	add_plist(&plist, PLIST_IGNORE, NULL);
	add_plist(&plist, PLIST_FILE, REQUIRE_FNAME);
	add_cksum(&plist, plist.tail, REQUIRE_FNAME);
    }
    if (Display) {
	copy_file(home, Display, DISPLAY_FNAME);
	add_plist(&plist, PLIST_IGNORE, NULL);
	add_plist(&plist, PLIST_FILE, DISPLAY_FNAME);
	add_cksum(&plist, plist.tail, DISPLAY_FNAME);
	add_plist(&plist, PLIST_DISPLAY, DISPLAY_FNAME);
    }
    if (Mtree) {
	copy_file(home, Mtree, MTREE_FNAME);
	add_plist(&plist, PLIST_IGNORE, NULL);
	add_plist(&plist, PLIST_FILE, MTREE_FNAME);
	add_cksum(&plist, plist.tail, MTREE_FNAME);
	add_plist(&plist, PLIST_MTREE, MTREE_FNAME);
    }

    /* Finally, write out the packing list */
    fp = fopen(CONTENTS_FNAME, "w");
    if (!fp) {
	cleanup(0);
	errx(2, "%s: can't open file %s for writing",
	    __func__, CONTENTS_FNAME);
    }
    write_plist(&plist, fp);
    if (fclose(fp)) {
	cleanup(0);
	errx(2, "%s: error while closing %s",
	    __func__, CONTENTS_FNAME);
    }

    /* And stick it into a tar ball */
    make_dist(home, pkg, suf, &plist);

    /* Cleanup */
    free(Comment);
    free(Desc);
    free_plist(&plist);
    leave_playpen();
    return TRUE;	/* Success */
}

static void
make_dist(const char *homedir, const char *pkg, const char *suff, Package *plist)
{
    struct stat sb;
    char tball[FILENAME_MAX];
    PackingList p;
    int ret;
    const char *args[50];	/* Much more than enough. */
    int nargs = 0;
    int pipefds[2];
    FILE *totar;
    pid_t pid;
    const char *cname;
    char *prefix = NULL;


    args[nargs++] = "tar";	/* argv[0] */

    if (*pkg == '/')
	snprintf(tball, FILENAME_MAX, "%s.%s", pkg, suff);
    else
	snprintf(tball, FILENAME_MAX, "%s/%s.%s", homedir, pkg, suff);

    /*
     * If the package tarball exists already, and we are running in `no
     * clobber' mode, skip this package.
     */
    if (stat(tball, &sb) == 0 && Regenerate == FALSE) {
	if (Verbose)
	    printf("Skipping package '%s'.  It already exists.\n", tball);
	return;
    }

    args[nargs++] = "-c";
    args[nargs++] = "-f";
    args[nargs++] = tball;
    if (strchr(suff, 'z')) {	/* Compress/gzip/bzip2? */
	if (Zipper == BZIP2) {
	    args[nargs++] = "-j";
	    cname = "bzip'd ";
	}
	else if (Zipper == XZ) {
	    args[nargs++] = "-J";
	    cname = "xz'd ";
	}
	else {
	    args[nargs++] = "-z";
	    cname = "gzip'd ";
	}
    } else {
	cname = "";
    }
    if (Dereference)
	args[nargs++] = "-h";
    if (ExcludeFrom) {
	args[nargs++] = "-X";
	args[nargs++] = ExcludeFrom;
    }
    args[nargs++] = "-T";	/* Take filenames from file instead of args. */
    args[nargs++] = "-";	/* Use stdin for the file. */
    args[nargs] = NULL;

    if (Verbose)
	printf("Creating %star ball in '%s'\n", cname, tball);

    /* Set up a pipe for passing the filenames, and fork off a tar process. */
    if (pipe(pipefds) == -1) {
	cleanup(0);
	errx(2, "%s: cannot create pipe", __func__);
    }
    if ((pid = fork()) == -1) {
	cleanup(0);
	errx(2, "%s: cannot fork process for tar", __func__);
    }
    if (pid == 0) {	/* The child */
	dup2(pipefds[0], 0);
	close(pipefds[0]);
	close(pipefds[1]);
	execv("/usr/bin/tar", (char * const *)(uintptr_t)args);
	cleanup(0);
	errx(2, "%s: failed to execute tar command", __func__);
    }

    /* Meanwhile, back in the parent process ... */
    close(pipefds[0]);
    if ((totar = fdopen(pipefds[1], "w")) == NULL) {
	cleanup(0);
	errx(2, "%s: fdopen failed", __func__);
    }

    fprintf(totar, "%s\n", CONTENTS_FNAME);
    fprintf(totar, "%s\n", COMMENT_FNAME);
    fprintf(totar, "%s\n", DESC_FNAME);

    if (Install)
	fprintf(totar, "%s\n", INSTALL_FNAME);
    if (PostInstall)
	fprintf(totar, "%s\n", POST_INSTALL_FNAME);
    if (DeInstall)
	fprintf(totar, "%s\n", DEINSTALL_FNAME);
    if (PostDeInstall)
	fprintf(totar, "%s\n", POST_DEINSTALL_FNAME);
    if (Require)
	fprintf(totar, "%s\n", REQUIRE_FNAME);
    if (Display)
	fprintf(totar, "%s\n", DISPLAY_FNAME);
    if (Mtree)
	fprintf(totar, "%s\n", MTREE_FNAME);

    for (p = plist->head; p; p = p->next) {
	if (p->type == PLIST_FILE)
	    fprintf(totar, "%s\n", p->name);
	else if (p->type == PLIST_CWD && p->name == NULL)
	    fprintf(totar, "-C\n%s\n", prefix);
	else if (p->type == PLIST_CWD && BaseDir && p->name && p->name[0] == '/')
	    fprintf(totar, "-C\n%s%s\n", BaseDir, p->name);
	else if (p->type == PLIST_CWD || p->type == PLIST_SRC)
	    fprintf(totar, "-C\n%s\n", p->name);
	else if (p->type == PLIST_IGNORE)
	     p = p->next;
	if (p->type == PLIST_CWD && !prefix)
	    prefix = p->name;

    }

    fclose(totar);
    wait(&ret);
    /* assume either signal or bad exit is enough for us */
    if (ret) {
	cleanup(0);
	errx(2, "%s: tar command failed with code %d", __func__, ret);
    }
}

static void
sanity_check()
{
    if (!Comment) {
	cleanup(0);
	errx(2, "%s: required package comment string is missing (-c comment)",
	    __func__);
    }
    if (!Desc) {
	cleanup(0);
	errx(2,	"%s: required package description string is missing (-d desc)",
	    __func__);
    }
    if (!Contents) {
	cleanup(0);
	errx(2,	"%s: required package contents list is missing (-f [-]file)",
	    __func__);
    }
}


/* Clean up those things that would otherwise hang around */
void
cleanup(int sig)
{
    int in_cleanup = 0;

    if (!in_cleanup) {
	in_cleanup = 1;
    	leave_playpen();
    }
    if (sig)
	exit(1);
}

static int
create_from_installed_recursive(const char *pkg, const char *suf)
{
    FILE *fp;
    Package plist;
    PackingList p;
    char tmp[PATH_MAX];
    int rval;

    if (!create_from_installed(InstalledPkg, pkg, suf))
	return FALSE;
    snprintf(tmp, sizeof(tmp), "%s/%s/%s", LOG_DIR, InstalledPkg, CONTENTS_FNAME);
    if (!fexists(tmp)) {
	warnx("can't find package '%s' installed!", InstalledPkg);
	return FALSE;
    }
    /* Suck in the contents list */
    plist.head = plist.tail = NULL;
    fp = fopen(tmp, "r");
    if (!fp) {
	warnx("unable to open %s file", tmp);
	return FALSE;
    }
    read_plist(&plist, fp);
    fclose(fp);
    rval = TRUE;
    for (p = plist.head; p ; p = p->next) {
	if (p->type != PLIST_PKGDEP)
	    continue;
	if (Verbose)
	    printf("Creating package %s\n", p->name);
	if (!create_from_installed(p->name, p->name, suf)) {
	    rval = FALSE;
	    break;
	}
    }
    free_plist(&plist);
    return rval;
}

static int
create_from_installed(const char *ipkg, const char *pkg, const char *suf)
{
    FILE *fp;
    Package plist;
    char homedir[MAXPATHLEN], log_dir[FILENAME_MAX];

    snprintf(log_dir, sizeof(log_dir), "%s/%s", LOG_DIR, ipkg);
    if (!fexists(log_dir)) {
	warnx("can't find package '%s' installed!", ipkg);
	return FALSE;
    }
    getcwd(homedir, sizeof(homedir));
    if (chdir(log_dir) == FAIL) {
	warnx("can't change directory to '%s'!", log_dir);
	return FALSE;
    }
    /* Suck in the contents list */
    plist.head = plist.tail = NULL;
    fp = fopen(CONTENTS_FNAME, "r");
    if (!fp) {
	warnx("unable to open %s file", CONTENTS_FNAME);
	return FALSE;
    }
    read_plist(&plist, fp);
    fclose(fp);

    Install = isfile(INSTALL_FNAME) ? (char *)INSTALL_FNAME : NULL;
    PostInstall = isfile(POST_INSTALL_FNAME) ?
	(char *)POST_INSTALL_FNAME : NULL;
    DeInstall = isfile(DEINSTALL_FNAME) ? (char *)DEINSTALL_FNAME : NULL;
    PostDeInstall = isfile(POST_DEINSTALL_FNAME) ?
	(char *)POST_DEINSTALL_FNAME : NULL;
    Require = isfile(REQUIRE_FNAME) ? (char *)REQUIRE_FNAME : NULL;
    Display = isfile(DISPLAY_FNAME) ? (char *)DISPLAY_FNAME : NULL;
    Mtree = isfile(MTREE_FNAME) ?  (char *)MTREE_FNAME : NULL;

    make_dist(homedir, pkg, suf, &plist);

    free_plist(&plist);
    if (chdir(homedir) == FAIL) {
	warnx("can't change directory to '%s'!", homedir);
	return FALSE;
    }
    return TRUE;
}
