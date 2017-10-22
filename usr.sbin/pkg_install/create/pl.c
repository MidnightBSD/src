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
 * Routines for dealing with the packing list.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "lib.h"
#include "create.h"
#include <errno.h>
#include <err.h>
#include <md5.h>

/* Add an MD5 checksum entry for a file or link */
void
add_cksum(Package *pkg, PackingList p, const char *fname)
{
    char *cp = NULL, buf[33];

    if (issymlink(fname)) {
	int len;
	char lnk[FILENAME_MAX];

	if ((len = readlink(fname, lnk, FILENAME_MAX)) > 0)
	    cp = MD5Data((unsigned char *)lnk, len, buf);
    } else if (isfile(fname)) {
	/* Don't record MD5 checksum for device nodes and such */
	cp = MD5File(fname, buf);
    }

    if (cp != NULL) {
	PackingList tmp = new_plist_entry();

	tmp->name = copy_string(strconcat("MD5:", cp));
	tmp->type = PLIST_COMMENT;
	tmp->next = p->next;
	tmp->prev = p;
	p->next = tmp;
	if (pkg->tail == p)
	    pkg->tail = tmp;
    }
}

/* Check a list for files that require preconversion */
void
check_list(const char *home, Package *pkg)
{
    const char *where = home;
    const char *there = NULL;
    char name[FILENAME_MAX];
    char *prefix = NULL;
    PackingList p;

    for (p = pkg->head; p != NULL; p = p->next)
	switch (p->type) {
	case PLIST_CWD:
	    if (!prefix)
		prefix = p->name;
	    where = (p->name == NULL) ? prefix : p->name;
	    break;

	case PLIST_IGNORE:
	    p = p->next;
	    break;

	case PLIST_SRC:
	    there = p->name;
	    break;

	case PLIST_FILE:
	    if (there)
		snprintf(name, sizeof(name), "%s/%s", there, p->name);
	    else
		snprintf(name, sizeof(name), "%s%s/%s",
		    BaseDir && where && where[0] == '/' ? BaseDir : "", where, p->name);

	    add_cksum(pkg, p, name);
	    break;
	default:
	    break;
	}
}

static int
trylink(const char *from, const char *to)
{
    if (link(from, to) == 0)
	return 0;
    if (errno == ENOENT) {
	/* try making the container directory */
	char *cp = strrchr(to, '/');
	if (cp)
	    vsystem("/bin/mkdir -p %.*s", cp - to,
		    to);
	return link(from, to);
    }
    return -1;
}

#define STARTSTRING "/usr/bin/tar cf -"
#define TOOBIG(str) (int)strlen(str) + 6 + (int)strlen(home) + where_count > maxargs
#define PUSHOUT() /* push out string */ \
	if (where_count > (int)sizeof(STARTSTRING)-1) { \
		    strcat(where_args, "|/usr/bin/tar xpf -"); \
		    if (system(where_args)) { \
			cleanup(0); \
			errx(2, "%s: can't invoke tar pipeline", __func__); \
		    } \
		    memset(where_args, 0, maxargs); \
 		    last_chdir = NULL; \
		    strcpy(where_args, STARTSTRING); \
		    where_count = sizeof(STARTSTRING)-1; \
	}

/*
 * Copy unmarked files in packing list to playpen - marked files
 * have already been copied in an earlier pass through the list.
 */
void
copy_plist(const char *home, Package *plist)
{
    PackingList p = plist->head;
    const char *where = home;
    const char *there = NULL, *mythere;
    char *where_args, *prefix = NULL;
    const char *last_chdir, *root = "/";
    int maxargs, where_count = 0, add_count;
    struct stat stb;
    dev_t curdir;

    maxargs = sysconf(_SC_ARG_MAX);
    maxargs -= 64;			/*
					 * Some slop for the tar cmd text,
					 * and sh -c
					 */
    where_args = malloc(maxargs);
    if (!where_args) {
	cleanup(0);
	errx(2, "%s: can't get argument list space", __func__);
    }

    memset(where_args, 0, maxargs);
    strcpy(where_args, STARTSTRING);
    where_count = sizeof(STARTSTRING)-1;
    last_chdir = 0;

    if (stat(".", &stb) == 0)
	curdir = stb.st_dev;
    else
	curdir = (dev_t) -1;		/*
					 * It's ok if this is a valid dev_t;
					 * this is just a hint for an
					 * optimization.
					 */

    while (p) {
	if (p->type == PLIST_CWD)
	{
	    if (!prefix)
		prefix = p->name;
	    where = p->name == NULL ? prefix : p->name;
	}
	else if (p->type == PLIST_SRC)
	    there = p->name;
	else if (p->type == PLIST_IGNORE)
	    p = p->next;
	else if (p->type == PLIST_FILE && !p->marked) {
	    char fn[FILENAME_MAX];


	    /* First, look for it in the "home" dir */
	    sprintf(fn, "%s/%s", home, p->name);
	    if (fexists(fn)) {
		if (lstat(fn, &stb) == 0 && stb.st_dev == curdir &&
		    S_ISREG(stb.st_mode)) {
		    /*
		     * If we can link it to the playpen, that avoids a copy
		     * and saves time.
		     */
		    if (p->name[0] != '/') {
			/*
			 * Don't link abspn stuff--it doesn't come from
			 * local dir!
			 */
			if (trylink(fn, p->name) == 0) {
			    p = p->next;
			    continue;
			}
		    }
		}
		if (TOOBIG(fn)) {
		    PUSHOUT();
		}
		if (p->name[0] == '/') {
		    add_count = snprintf(&where_args[where_count],
					 maxargs - where_count,
					 " %s %s",
					 last_chdir == root ? "" : "-C /",
					 p->name);
		    last_chdir = root;
		} else {
		    add_count = snprintf(&where_args[where_count],
					 maxargs - where_count,
					 " %s%s %s",
					 last_chdir == home ? "" : "-C ",
					 last_chdir == home ? "" : home,
					 p->name);
		    last_chdir = home;
		}
		if (add_count < 0 || add_count >= maxargs - where_count) {
		    cleanup(0);
		    errx(2, "%s: oops, miscounted strings!", __func__);
		}
		where_count += add_count;
	    }
	    /*
	     * Otherwise, try along the actual extraction path..
	     */
	    else {
		if (p->name[0] == '/')
		    mythere = root;
		else mythere = there;
		if (mythere)
		    snprintf(fn, sizeof(fn), "%s/%s", mythere, p->name);
		else
		    snprintf(fn, sizeof(fn), "%s%s/%s",
			BaseDir && where && where[0] == '/' ? BaseDir : "", where, p->name);
		if (lstat(fn, &stb) == 0 && stb.st_dev == curdir &&
		    S_ISREG(stb.st_mode)) {
		    /*
		     * If we can link it to the playpen, that avoids a copy
		     * and saves time.
		     */
		    if (trylink(fn, p->name) == 0) {
			p = p->next;
			continue;
		    }
		}
		if (TOOBIG(p->name)) {
		    PUSHOUT();
		}
		if (last_chdir == (mythere ? mythere : where))
		    add_count = snprintf(&where_args[where_count],
					 maxargs - where_count,
					 " %s", p->name);
		else
		    add_count = snprintf(&where_args[where_count],
					 maxargs - where_count,
					 " -C %s %s",
					 mythere ? mythere : where,
					 p->name);
		if (add_count < 0 || add_count >= maxargs - where_count) {
		    cleanup(0);
		    errx(2, "%s: oops, miscounted strings!", __func__);
		}
		where_count += add_count;
		last_chdir = (mythere ? mythere : where);
	    }
	}
	p = p->next;
    }
    PUSHOUT();
    free(where_args);
}
