/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ncurses.h>
#include <dialog.h>
#include "sysinstall.h"

/* Macros and magic values */
#define MAX_MENU	12
#define _MAX_DESC	55

/* A structure holding the root, top and plist pointer at once */
struct ListPtrs
{
    PkgNodePtr root;	/* root of tree */
    PkgNodePtr top;	/* part of tree we handle */
    PkgNodePtr plist;	/* list of selected packages */
};
typedef struct ListPtrs* ListPtrsPtr;

static void	index_recorddeps(Boolean add, PkgNodePtr root, IndexEntryPtr ie);

/* Shared between index_initialize() and the various clients of it */
PkgNode Top, Plist;

/* Smarter strdup */
static inline char *
_strdup(char *ptr)
{
    return ptr ? strdup(ptr) : NULL;
}

static char *descrs[] = {
    "Package Selection", "To mark a package, move to it and press SPACE.  If the package is\n"
    "already marked, it will be unmarked or deleted (if installed).\n"
    "Items marked with a `D' are dependencies which will be auto-loaded.\n"
    "To search for a package by name, press ESC.  To select a category,\n"
    "press RETURN.  NOTE:  The All category selection creates a very large\n"
    "submenu!  If you select it, please be patient while it comes up.",
    "Package Targets", "These are the packages you've selected for extraction.\n\n"
    "If you're sure of these choices, select OK.\n"
    "If not, select Cancel to go back to the package selection menu.\n",
    "All", "All available packages in all categories.",
    "accessibility", "Ports to help disabled users.",
    "afterstep", "Ports to support the AfterStep window manager.",
    "arabic", "Ported software for Arab countries.",
    "archivers", "Utilities for archiving and unarchiving data.",
    "astro", "Applications related to astronomy.",
    "audio", "Audio utilities - most require a supported sound card.",
    "benchmarks", "Utilities for measuring system performance.",
    "biology", "Software related to biology.",
    "cad", "Computer Aided Design utilities.",
    "chinese", "Ported software for the Chinese market.",
    "comms", "Communications utilities.",
    "converters", "Format conversion utilities.",
    "databases", "Database software.",
    "deskutils", "Various Desktop utilities.",
    "devel", "Software development utilities and libraries.",
    "dns", "Domain Name Service tools.",
    "docs", "Meta-ports for FreeBSD documentation.",
    "editors", "Editors.",
    "elisp", "Things related to Emacs Lisp.",
    "emulators", "Utilities for emulating other operating systems.",
    "finance", "Monetary, financial and related applications.",
    "french", "Ported software for French countries.",
    "ftp", "FTP client and server utilities.",
    "games", "Various and sundry amusements.",
    "german", "Ported software for Germanic countries.",
    "geography", "Geography-related software.",
    "gnome", "Components of the Gnome Desktop environment.",
    "gnustep", "Software for GNUstep desktop environment.",
    "graphics", "Graphics libraries and utilities.",
    "haskell", "Software related to the Haskell language.",
    "hamradio", "Software for amateur radio.",
    "hebrew", "Ported software for Hebrew language.",
    "hungarian", "Ported software for the Hungarian market.",
    "ipv6", "IPv6 related software.",
    "irc", "Internet Relay Chat utilities.",
    "japanese", "Ported software for the Japanese market.",
    "java", "Java language support.",
    "kde", "Software for the K Desktop Environment.",
    "kld", "Kernel loadable modules",
    "korean", "Ported software for the Korean market.",
    "lang", "Computer languages.",
    "linux", "Linux programs that can run under binary compatibility.",
    "lisp", "Software related to the Lisp language.",
    "mail", "Electronic mail packages and utilities.",
    "math", "Mathematical computation software.",
    "mbone", "Applications and utilities for the MBONE.",
    "misc", "Miscellaneous utilities.",
    "multimedia", "Multimedia software.",
    "net", "Networking utilities.",
    "net-im", "Instant messaging software.",
    "net-mgmt", "Network management tools.",
    "net-p2p", "Peer to peer network applications.",
    "news", "USENET News support software.",
    "palm", "Software support for the Palm(tm) series.",
    "parallel", "Applications dealing with parallelism in computing.",
    "pear", "Software related to the Pear PHP framework.",
    "perl5", "Utilities/modules for the PERL5 language.",
    "plan9", "Software from the Plan9 operating system.",
    "polish", "Ported software for the Polish market.",
    "ports-mgmt", "Utilities for managing ports and packages.",
    "portuguese", "Ported software for the Portuguese market.",
    "print", "Utilities for dealing with printing.",
    "python", "Software related to the Python language.",
    "ruby", "Software related to the Ruby language.",
    "rubygems", "Ports of RubyGems packages.",
    "russian", "Ported software for the Russian market.",
    "scheme", "Software related to the Scheme language.",
    "science", "Scientific software.",
    "security", "System security software.",
    "shells", "Various shells (tcsh, bash, etc).",
    "spanish", "Ported software for the Spanish market.",
    "sysutils", "Various system utilities.",
    "tcl", "TCL and packages that depend on it.",
    "tcl80", "TCL v8.0 and packages that depend on it.",
    "tcl82", "TCL v8.2 and packages that depend on it.",
    "tcl83", "TCL v8.3 and packages that depend on it.",
    "tcl84", "TCL v8.4 and packages that depend on it.",
    "textproc", "Text processing/search utilities.",
    "tk", "Tk and packages that depend on it.",
    "tk80", "Tk8.0 and packages that depend on it.",
    "tk82", "Tk8.2 and packages that depend on it.",
    "tk83", "Tk8.3 and packages that depend on it.",
    "tk84", "Tk8.4 and packages that depend on it.",
    "tkstep80", "Ports to support the TkStep window manager.",
    "ukrainian", "Ported software for the Ukrainian market.",
    "vietnamese", "Ported software for the Vietnamese market.",
    "windowmaker", "Ports to support the WindowMaker window manager.",
    "www", "Web utilities (browsers, HTTP servers, etc).",
    "x11", "X Window System based utilities.",
    "x11-clocks", "X Window System based clocks.",
    "x11-drivers", "X Window System drivers.",
    "x11-fm", "X Window System based file managers.",
    "x11-fonts", "X Window System fonts and font utilities.",
    "x11-servers", "X Window System servers.",
    "x11-themes", "X Window System themes.",
    "x11-toolkits", "X Window System based development toolkits.",
    "x11-wm", "X Window System window managers.",
    "xfce", "Software related to the Xfce Desktop Environment.",
    "zope", "Software related to the Zope platform.",
    NULL, NULL,
};

static char *
fetch_desc(char *name)
{
    int i;

    for (i = 0; descrs[i]; i += 2) {
	if (!strcmp(descrs[i], name))
	    return descrs[i + 1];
    }
    return "No description provided";
}

static PkgNodePtr
new_pkg_node(char *name, node_type type)
{
    PkgNodePtr tmp = safe_malloc(sizeof(PkgNode));

    tmp->name = _strdup(name);
    tmp->type = type;
    return tmp;
}

static char *
strip(char *buf)
{
    int i;

    for (i = 0; buf[i]; i++)
	if (buf[i] == '\t' || buf[i] == '\n')
	    buf[i] = ' ';
    return buf;
}

static IndexEntryPtr
new_index(char *name, char *pathto, char *prefix, char *comment, char *descr, char *maint, char *deps, int volume)
{
    IndexEntryPtr tmp = safe_malloc(sizeof(IndexEntry));

    tmp->name =		_strdup(name);
    tmp->path =		_strdup(pathto);
    tmp->prefix =	_strdup(prefix);
    tmp->comment =	_strdup(comment);
    tmp->descrfile =	strip(_strdup(descr));
    tmp->maintainer =	_strdup(maint);
    tmp->deps =		_strdup(deps);
    tmp->depc =		0;
    tmp->installed =	package_installed(name);
    tmp->vol_checked =	0;
    tmp->volume =	volume;
    if (volume != 0) {
	have_volumes = TRUE;
	if (low_volume == 0)
	    low_volume = volume;
	else if (low_volume > volume)
	    low_volume = volume;
	if (high_volume < volume)
	    high_volume = volume;
    }
    return tmp;
}

static void
index_register(PkgNodePtr top, char *where, IndexEntryPtr ptr)
{
    PkgNodePtr p, q;

    for (q = NULL, p = top->kids; p; p = p->next) {
	if (!strcmp(p->name, where)) {
	    q = p;
	    break;
	}
    }
    if (!p) {
	/* Add new category */
	q = new_pkg_node(where, PLACE);
	q->desc = fetch_desc(where);
	q->next = top->kids;
	top->kids = q;
    }
    p = new_pkg_node(ptr->name, PACKAGE);
    p->desc = ptr->comment;
    p->data = ptr;
    p->next = q->kids;
    q->kids = p;
}

static int
copy_to_sep(char *to, char *from, int sep)
{
    char *tok;

    tok = strchr(from, sep);
    if (!tok) {
	*to = '\0';
	return 0;
    }
    *tok = '\0';
    strcpy(to, from);
    return tok + 1 - from;
}

static int
skip_to_sep(char *from, int sep)
{
    char *tok;

    tok = strchr(from, sep);
    if (!tok)
	return 0;
    *tok = '\0';
    return tok + 1 - from;
}

static int
readline(FILE *fp, char *buf, int max)
{
    int rv, i = 0;
    char ch;

    while ((rv = fread(&ch, 1, 1, fp)) == 1 && ch != '\n' && i < max)
	buf[i++] = ch;
    if (i < max)
	buf[i] = '\0';
    return rv;
}

/*
 * XXX - this function should do error checking, and skip corrupted INDEX
 * lines without a set number of '|' delimited fields.
 */

static int
index_parse(FILE *fp, char *name, char *pathto, char *prefix, char *comment, char *descr, char *maint, char *cats, char *rdeps, int *volume)
{
    char line[10240 + 2048 * 7];
    char junk[2048];
    char volstr[2048];
    char *cp;
    int i;

    i = readline(fp, line, sizeof line);
    if (i <= 0)
	return EOF;
    cp = line;
    cp += copy_to_sep(name, cp, '|');		/* package name */
    cp += copy_to_sep(pathto, cp, '|');		/* ports directory */
    cp += copy_to_sep(prefix, cp, '|');		/* prefix */
    cp += copy_to_sep(comment, cp, '|');	/* comment */
    cp += copy_to_sep(descr, cp, '|');		/* path to pkg-descr */
    cp += copy_to_sep(maint, cp, '|');		/* maintainer */
    cp += copy_to_sep(cats, cp, '|');		/* categories */
    cp += skip_to_sep(cp, '|');			/* build deps - not used */
    cp += copy_to_sep(rdeps, cp, '|');		/* run deps */
    if (index(cp, '|'))
        cp += skip_to_sep(cp, '|');		/* url - not used */
    else {
	strncpy(junk, cp, 1023);
	*volume = 0;
	return 0;
    }
    if (index(cp, '|'))
	cp += skip_to_sep(cp, '|');		/* extract deps - not used */
    if (index(cp, '|'))
	cp += skip_to_sep(cp, '|');		/* patch deps - not used */
    if (index(cp, '|'))
	cp += skip_to_sep(cp, '|');		/* fetch deps - not used */
    if (index(cp, '|'))
        cp += copy_to_sep(volstr, cp, '|');	/* media volume */
    else {
	strncpy(volstr, cp, 1023);
    }
    *volume = atoi(volstr);
    return 0;
}

int
index_read(FILE *fp, PkgNodePtr papa)
{
    char name[127], pathto[255], prefix[255], comment[255], descr[127], maint[127], cats[511], deps[2048 * 8];
    int volume;
    PkgNodePtr i;

    while (index_parse(fp, name, pathto, prefix, comment, descr, maint, cats, deps, &volume) != EOF) {
	char *cp, *cp2, tmp[1024];
	IndexEntryPtr idx;

	idx = new_index(name, pathto, prefix, comment, descr, maint, deps, volume);
	/* For now, we only add things to menus if they're in categories.  Keywords are ignored */
	for (cp = strcpy(tmp, cats); (cp2 = strchr(cp, ' ')) != NULL; cp = cp2 + 1) {
	    *cp2 = '\0';
	    index_register(papa, cp, idx);
	}
	index_register(papa, cp, idx);

	/* Add to special "All" category */
	index_register(papa, "All", idx);
    }

    /* Adjust dependency counts */
    for (i = papa->kids; i != NULL; i = i->next)
	if (strcmp(i->name, "All") == 0)
	    break;
    for (i = i->kids; i != NULL; i = i->next)
	if (((IndexEntryPtr)i->data)->installed)
	    index_recorddeps(TRUE, papa, i->data);

    return 0;
}

void
index_init(PkgNodePtr top, PkgNodePtr plist)
{
    if (top) {
	top->next = top->kids = NULL;
	top->name = "Package Selection";
	top->type = PLACE;
	top->desc = fetch_desc(top->name);
	top->data = NULL;
    }
    if (plist) {
	plist->next = plist->kids = NULL;
	plist->name = "Package Targets";
	plist->type = PLACE;
	plist->desc = fetch_desc(plist->name);
	plist->data = NULL;
    }
}

void
index_print(PkgNodePtr top, int level)
{
    int i;

    while (top) {
	for (i = 0; i < level; i++) putchar('\t');
	printf("name [%s]: %s\n", top->type == PLACE ? "place" : "package", top->name);
	for (i = 0; i < level; i++) putchar('\t');
	printf("desc: %s\n", top->desc);
	if (top->kids)
	    index_print(top->kids, level + 1);
	top = top->next;
    }
}

/* Swap one node for another */
static void
swap_nodes(PkgNodePtr a, PkgNodePtr b)
{
    PkgNode tmp;

    tmp = *a;
    *a = *b;
    a->next = tmp.next;
    tmp.next = b->next;
    *b = tmp;
}

/* Use a disgustingly simplistic bubble sort to put our lists in order */
void
index_sort(PkgNodePtr top)
{
    PkgNodePtr p, q;

    /* Sort everything at the top level */
    for (p = top->kids; p; p = p->next) {
	for (q = top->kids; q; q = q->next) {
	    if (q->next && strcmp(q->name, q->next->name) > 0)
		swap_nodes(q, q->next);
	}
    }

    /* Now sub-sort everything n levels down */    
    for (p = top->kids; p; p = p->next) {
	if (p->kids)
	    index_sort(p);
    }
}

/* Delete an entry out of the list it's in (only the plist, at present) */
static void
index_delete(PkgNodePtr n)
{
    if (n->next) {
	PkgNodePtr p = n->next;

	*n = *(n->next);
	safe_free(p);
    }
    else /* Kludgy end sentinal */
	n->name = NULL;
}

/*
 * Search for a given node by name, returning the category in if
 * tp is non-NULL.
 */
PkgNodePtr
index_search(PkgNodePtr top, char *str, PkgNodePtr *tp)
{
    PkgNodePtr p, sp;

    for (p = top->kids; p && p->name; p = p->next) {
	if (p->type == PACKAGE) {
	    /* If tp == NULL, we're looking for an exact package match */
	    if (!tp && !strcmp(p->name, str))
		return p;

	    /* If tp, we're looking for both a package and a pointer to the place it's in */
	    if (tp && !strncmp(p->name, str, strlen(str))) {
		*tp = top;
		return p;
	    }
	}
	else if (p->kids) {
	    /* The usual recursion-out-of-laziness ploy */
	    if ((sp = index_search(p, str, tp)) != NULL)
		return sp;
	}
    }
    if (p && !p->name)
	p = NULL;
    return p;
}

static int
pkg_checked(dialogMenuItem *self)
{
    ListPtrsPtr lists = (ListPtrsPtr)self->aux;
    PkgNodePtr kp = self->data, plist = lists->plist;
    int i;

    i = index_search(plist, kp->name, NULL) ? TRUE : FALSE;
    if (kp->type == PACKAGE && plist) {
	IndexEntryPtr ie = kp->data;
	int markD, markX;

	markD = ie->depc > 0; /* needed as dependency */
	markX = i || ie->installed; /* selected or installed */
	self->mark = markX ? 'X' : 'D';
	return markD || markX;
    } else
	return FALSE;
}

static int
pkg_fire(dialogMenuItem *self)
{
    int ret;
    ListPtrsPtr lists = (ListPtrsPtr)self->aux;
    PkgNodePtr sp, kp = self->data, plist = lists->plist;

    if (!plist)
	ret = DITEM_FAILURE;
    else if (kp->type == PACKAGE) {
	IndexEntryPtr ie = kp->data;

	sp = index_search(plist, kp->name, NULL);
	/* Not already selected? */
	if (!sp) {
	    if (!ie->installed) {
		PkgNodePtr np = (PkgNodePtr)safe_malloc(sizeof(PkgNode));

		*np = *kp;
		np->next = plist->kids;
		plist->kids = np;
		index_recorddeps(TRUE, lists->root, ie);
		msgInfo("Added %s to selection list", kp->name);
	    }
	    else if (ie->depc == 0) {
		if (!msgNoYes("Do you really want to delete %s from the system?", kp->name)) {
		    if (vsystem("pkg_delete %s %s", isDebug() ? "-v" : "", kp->name)) {
			msgConfirm("Warning:  pkg_delete of %s failed.\n  Check debug output for details.", kp->name);
		    }
		    else {
			ie->installed = 0;
			index_recorddeps(FALSE, lists->root, ie);
		    }
		}
	    }
	    else
		msgConfirm("Warning: Package %s is needed by\n  %d other installed package%s.",
			   kp->name, ie->depc, (ie->depc != 1) ? "s" : "");
	}
	else {
	    index_recorddeps(FALSE, lists->root, ie);
	    msgInfo("Removed %s from selection list", kp->name);
	    index_delete(sp);
	}
	ret = DITEM_SUCCESS;
	/* Mark menu for redraw if we had dependencies */
	if (strlen(ie->deps) > 0)
	    ret |= DITEM_REDRAW;
    }
    else {	/* Not a package, must be a directory */
	int p, s;
		    
	p = s = 0;
	index_menu(lists->root, kp, plist, &p, &s);
	ret = DITEM_SUCCESS | DITEM_CONTINUE;
    }
    return ret;
}

static void
pkg_selected(dialogMenuItem *self, int is_selected)
{
    PkgNodePtr kp = self->data;

    if (!is_selected || kp->type != PACKAGE)
	return;
    msgInfo("%s", kp->desc);
}

int
index_menu(PkgNodePtr root, PkgNodePtr top, PkgNodePtr plist, int *pos, int *scroll)
{
    struct ListPtrs lists;
    size_t maxname;
    int n, rval;
    int curr, max;
    PkgNodePtr kp;
    dialogMenuItem *nitems;
    Boolean hasPackages;
    WINDOW *w;
    
    lists.root = root;
    lists.top = top;
    lists.plist = plist;

    hasPackages = FALSE;
    nitems = NULL;
    n = maxname = 0;

    /* Figure out if this menu is full of "leaves" or "branches" */
    for (kp = top->kids; kp && kp->name; kp = kp->next) {
	size_t len;

	++n;
	if (kp->type == PACKAGE && plist) {
	    hasPackages = TRUE;
	    if ((len = strlen(kp->name)) > maxname)
		maxname = len;
	}
    }
    if (!n && plist) {
	msgConfirm("The %s menu is empty.", top->name);
	return DITEM_LEAVE_MENU;
    }

    w = savescr();
    while (1) {
	n = 0;
	curr = max = 0;
	use_helpline(NULL);
	use_helpfile(NULL);
	kp = top->kids;
	if (!hasPackages && plist) {
	    nitems = item_add(nitems, "OK", NULL, NULL, NULL, NULL, NULL, NULL, &curr, &max);
	    nitems = item_add(nitems, "Install", NULL, NULL, NULL, NULL, NULL, NULL, &curr, &max);
	}
	while (kp && kp->name) {
	    char buf[256];
	    IndexEntryPtr ie = kp->data;

	    /* Brutally adjust description to fit in menu */
	    if (kp->type == PACKAGE)
		snprintf(buf, sizeof buf, "[%s]", ie->path ? ie->path : "External vendor");
	    else
		SAFE_STRCPY(buf, kp->desc);
	    if (strlen(buf) > (_MAX_DESC - maxname))
		buf[_MAX_DESC - maxname] = '\0';
	    nitems = item_add(nitems, kp->name, buf, pkg_checked, 
			      pkg_fire, pkg_selected, kp, &lists, 
			      &curr, &max);
	    ++n;
	    kp = kp->next;
	}
	/* NULL delimiter so item_free() knows when to stop later */
	nitems = item_add(nitems, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
			  &curr, &max);

recycle:
	dialog_clear_norefresh();
	if (hasPackages)
	    rval = dialog_checklist(top->name, top->desc, -1, -1, n > MAX_MENU ? MAX_MENU : n, -n, nitems, NULL);
	else
	    rval = dialog_menu(top->name, top->desc, -1, -1, n > MAX_MENU ? MAX_MENU : n, -n, nitems + (plist ? 2 : 0), (char *)plist, pos, scroll);
	if (rval == -1 && plist) {
	    static char *cp;
	    PkgNodePtr menu;

	    /* Search */
	    if ((cp = msgGetInput(cp, "Search by package name.  Please enter search string:")) != NULL) {
		PkgNodePtr p = index_search(top, cp, &menu);

		if (p) {
		    int pos, scroll;

		    /* These need to be set to point at the found item, actually.  Hmmm! */
		    pos = scroll = 0;
		    index_menu(root, menu, plist, &pos, &scroll);
		}
		else
		    msgConfirm("Search string: %s yielded no hits.", cp);
	    }
	    goto recycle;
	}
	items_free(nitems, &curr, &max);
	restorescr(w);
	return rval ? DITEM_FAILURE : DITEM_SUCCESS;
    }
}

int
index_extract(Device *dev, PkgNodePtr top, PkgNodePtr who, Boolean depended,
    int current_volume)
{
    int status = DITEM_SUCCESS;
    Boolean notyet = FALSE;
    PkgNodePtr tmp2;
    IndexEntryPtr id = who->data;
    WINDOW *w;

    /* 
     * Short-circuit the package dependency checks.  We're already
     * maintaining a data structure of installed packages, so if a
     * package is already installed, don't try to check to make sure
     * that all of its dependencies are installed.  At best this
     * wastes a ton of cycles and can cause minor delays between
     * package extraction.  At worst it can cause an infinite loop with
     * a certain faulty INDEX file. 
     */

    if (id->installed == 1 || (have_volumes && id->vol_checked == current_volume))
	return DITEM_SUCCESS;

    w = savescr();
    if (id && id->deps && strlen(id->deps)) {
	char t[2048 * 8], *cp, *cp2;

	SAFE_STRCPY(t, id->deps);
	cp = t;
	while (cp && DITEM_STATUS(status) == DITEM_SUCCESS) {
	    if ((cp2 = index(cp, ' ')) != NULL)
		*cp2 = '\0';
	    if ((tmp2 = index_search(top, cp, NULL)) != NULL) {
		status = index_extract(dev, top, tmp2, TRUE, current_volume);
		if (DITEM_STATUS(status) != DITEM_SUCCESS) {
		    /* package probably on a future disc volume */
		    if (status & DITEM_CONTINUE) {
			status = DITEM_SUCCESS;
			notyet = TRUE;
		    } else if (variable_get(VAR_NO_CONFIRM))
			msgNotify("Loading of dependent package %s failed", cp);
		    else
			msgConfirm("Loading of dependent package %s failed", cp);
		}
	    }
	    else if (!package_installed(cp)) {
		if (variable_get(VAR_NO_CONFIRM))
		    msgNotify("Warning: %s is a required package but was not found.", cp);
		else
		    msgConfirm("Warning: %s is a required package but was not found.", cp);
	    }
	    if (cp2)
		cp = cp2 + 1;
	    else
		cp = NULL;
	}
    }

    /*
     * If iterating through disc volumes one at a time indicate failure if
     * dependency install failed due to package being on a higher volume
     * numbered disc, but that we should continue anyway.  Note that this
     * package has already been processed for this disc volume so we don't
     * need to do it again.
     */

    if (notyet) {
    	restorescr(w);
	id->vol_checked = current_volume;
	return DITEM_FAILURE | DITEM_CONTINUE;
    }

    /*
     * Done with the deps?  Try to load the real m'coy.  If iterating
     * through a multi-volume disc set fail the install if the package
     * is on a higher numbered volume to cut down on disc switches the
     * user needs to do, but indicate caller should continue processing
     * despite error return.  Note this package was processed for the
     * current disc being checked.
     */

    if (DITEM_STATUS(status) == DITEM_SUCCESS) {
	/* Prompt user if the package is not available on the current volume. */
	if(mediaDevice->type == DEVICE_TYPE_CDROM) {
	    if (current_volume != 0 && id->volume > current_volume) {
		restorescr(w);
		id->vol_checked = current_volume;
		return DITEM_FAILURE | DITEM_CONTINUE;
	    }
	    while (id->volume != dev->volume) {
		if (!msgYesNo("This is disc #%d.  Package %s is on disc #%d\n"
			  "Would you like to switch discs now?\n", dev->volume,
			  id->name, id->volume)) {
		    DEVICE_SHUTDOWN(mediaDevice);
		    msgConfirm("Please remove disc #%d from your drive, and add disc #%d\n",
			dev->volume, id->volume);
		    DEVICE_INIT(mediaDevice);
		} else {
		    restorescr(w);
		    return DITEM_FAILURE;
		}
	    }
	}
	status = package_extract(dev, who->name, depended);
	if (DITEM_STATUS(status) == DITEM_SUCCESS)
	    id->installed = 1;
    }
    restorescr(w);
    return status;
}

static void
index_recorddeps(Boolean add, PkgNodePtr root, IndexEntryPtr ie)
{
   char depends[1024 * 16], *space, *todo;
   PkgNodePtr found;
   IndexEntryPtr found_ie;

   SAFE_STRCPY(depends, ie->deps);
   for (todo = depends; todo != NULL; ) {
      space = index(todo, ' ');
      if (space != NULL)
	 *space = '\0';

      if (strlen(todo) > 0) { /* only non-empty dependencies */
	  found = index_search(root, todo, NULL);
	  if (found != NULL) {
	      found_ie = found->data;
	      if (add)
		  ++found_ie->depc;
	      else
		  --found_ie->depc;
	  }
      }

      if (space != NULL)
	 todo = space + 1;
      else
	 todo = NULL;
   }
}

static Boolean index_initted;

/* Read and initialize global index */
int
index_initialize(char *path)
{
    FILE *fp;
    WINDOW *w = NULL;

    if (!index_initted) {
	w = savescr();
	dialog_clear_norefresh();
	have_volumes = FALSE;
	low_volume = high_volume = 0;

	/* Got any media? */
	if (!mediaVerify()) {
	    restorescr(w);
	    return DITEM_FAILURE;
	}

	/* Does it move when you kick it? */
	if (!DEVICE_INIT(mediaDevice)) {
	    restorescr(w);
	    return DITEM_FAILURE;
	}

	dialog_clear_norefresh();
	msgNotify("Attempting to fetch %s file from selected media.", path);
	fp = DEVICE_GET(mediaDevice, path, TRUE);
	if (!fp) {
	    msgConfirm("Unable to get packages/INDEX file from selected media.\n\n"
		       "This may be because the packages collection is not available\n"
		       "on the distribution media you've chosen, most likely an FTP site\n"
		       "without the packages collection mirrored.  Please verify that\n"
		       "your media, or your path to the media, is correct and try again.");
	    DEVICE_SHUTDOWN(mediaDevice);
	    restorescr(w);
	    return DITEM_FAILURE;
	}
	dialog_clear_norefresh();
	msgNotify("Located INDEX, now reading package data from it...");
	index_init(&Top, &Plist);
	if (index_read(fp, &Top)) {
	    msgConfirm("I/O or format error on packages/INDEX file.\n"
		       "Please verify media (or path to media) and try again.");
	    fclose(fp);
	    restorescr(w);
	    return DITEM_FAILURE;
	}
	fclose(fp);
	index_sort(&Top);
	index_initted = TRUE;
	restorescr(w);
    }
    return DITEM_SUCCESS;
}
