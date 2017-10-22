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
 * Miscellaneous file access utilities.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <err.h>
#include "lib.h"
#include "add.h"

/*
 * Assuming dir is a desired directory name, make it and all intervening
 * directories necessary.
 */

int
make_hierarchy(char *dir, Boolean set_perm)
{
    char *cp1, *cp2;

    if (dir[0] == '/')
	cp1 = cp2 = dir + 1;
    else
	cp1 = cp2 = dir;
    while (cp2) {
	if ((cp2 = strchr(cp1, '/')) !=NULL )
	    *cp2 = '\0';
	if (fexists(dir)) {
	    if (!isdir(dir)) {
		if (cp2)
		    *cp2 = '/';
		return FAIL;
	    }
	}
	else {
	    if (mkdir(dir, 0777) < 0) {
		if (cp2)
		    *cp2 = '/';
		return FAIL;
	    }
	    if (set_perm)
		apply_perms(NULL, dir);
	}
	/* Put it back */
	if (cp2) {
	    *cp2 = '/';
	    cp1 = cp2 + 1;
	}
    }
    return SUCCESS;
}

/* Using permission defaults, apply them as necessary */
void
apply_perms(const char *dir, const char *arg)
{
    const char *cd_to;

    if (!dir || *arg == '/')	/* absolute path? */
	cd_to = "/";
    else
	cd_to = dir;

    if (Mode)
	if (vsystem("cd %s && /bin/chmod -R %s %s", cd_to, Mode, arg))
	    warnx("couldn't change modes of '%s' to '%s'", arg, Mode);
    if (Owner && Group) {
	if (vsystem("cd %s && /usr/sbin/chown -R %s:%s %s", cd_to, Owner, Group, arg))
	    warnx("couldn't change owner/group of '%s' to '%s:%s'",
		   arg, Owner, Group);
	return;
    }
    if (Owner) {
	if (vsystem("cd %s && /usr/sbin/chown -R %s %s", cd_to, Owner, arg))
	    warnx("couldn't change owner of '%s' to '%s'", arg, Owner);
	return;
    } else if (Group)
	if (vsystem("cd %s && /usr/bin/chgrp -R %s %s", cd_to, Group, arg))
	    warnx("couldn't change group of '%s' to '%s'", arg, Group);
}

