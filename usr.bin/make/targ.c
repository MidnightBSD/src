/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 * @(#)targ.c	8.2 (Berkeley) 3/19/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.bin/make/targ.c,v 1.41 2005/05/13 13:47:41 harti Exp $");

/*
 * Functions for maintaining the Lst allTargets. Target nodes are
 * kept in two structures: a Lst, maintained by the list library, and a
 * hash table, maintained by the hash library.
 *
 * Interface:
 *	Targ_Init	Initialization procedure.
 *
 *	Targ_NewGN	Create a new GNode for the passed target (string).
 *			The node is *not* placed in the hash table, though all
 *			its fields are initialized.
 *
 *	Targ_FindNode	Find the node for a given target, creating and storing
 *			it if it doesn't exist and the flags are right
 *			(TARG_CREATE)
 *
 *	Targ_FindList	Given a list of names, find nodes for all of them. If a
 *			name doesn't exist and the TARG_NOCREATE flag was given,
 *			an error message is printed. Else, if a name doesn't
 *			exist, its node is created.
 *
 *	Targ_Ignore	Return TRUE if errors should be ignored when creating
 *			the given target.
 *
 *	Targ_Silent	Return TRUE if we should be silent when creating the
 *			given target.
 *
 *	Targ_Precious	Return TRUE if the target is precious and should not
 *			be removed if we are interrupted.
 *
 * Debugging:
 *	Targ_PrintGraph	Print out the entire graphm all variables and statistics
 *			for the directory cache. Should print something for
 *			suffixes, too, but...
 */

#include <stdio.h>
#include <string.h>

#include "dir.h"
#include "globals.h"
#include "GNode.h"
#include "hash.h"
#include "make.h"
#include "suff.h"
#include "targ.h"
#include "util.h"
#include "var.h"

/* the list of all targets found so far */
static Lst allTargets = Lst_Initializer(allTargets);

static Hash_Table targets;	/* a hash table of same */

#define	HTSIZE	191		/* initial size of hash table */

/**
 * Targ_Init
 *	Initialize this module
 *
 * Side Effects:
 *	The allTargets list and the targets hash table are initialized
 */
void
Targ_Init(void)
{

	Hash_InitTable(&targets, HTSIZE);
}

/**
 * Targ_NewGN
 *	Create and initialize a new graph node
 *
 * Results:
 *	An initialized graph node with the name field filled with a copy
 *	of the passed name
 *
 * Side Effects:
 *	The gnode is added to the list of all gnodes.
 */
GNode *
Targ_NewGN(const char *name)
{
	GNode *gn;

	gn = emalloc(sizeof(GNode));
	gn->name = estrdup(name);
	gn->path = NULL;
	if (name[0] == '-' && name[1] == 'l') {
		gn->type = OP_LIB;
	} else {
		gn->type = 0;
	}
	gn->unmade = 0;
	gn->make = FALSE;
	gn->made = UNMADE;
	gn->childMade = FALSE;
	gn->order = 0;
	gn->mtime = gn->cmtime = 0;
	Lst_Init(&gn->iParents);
	Lst_Init(&gn->cohorts);
	Lst_Init(&gn->parents);
	Lst_Init(&gn->children);
	Lst_Init(&gn->successors);
	Lst_Init(&gn->preds);
	Lst_Init(&gn->context);
	Lst_Init(&gn->commands);
	gn->suffix = NULL;

	return (gn);
}

/**
 * Targ_FindNode
 *	Find a node in the list using the given name for matching
 *
 * Results:
 *	The node in the list if it was. If it wasn't, return NULL of
 *	flags was TARG_NOCREATE or the newly created and initialized node
 *	if it was TARG_CREATE
 *
 * Side Effects:
 *	Sometimes a node is created and added to the list
 */
GNode *
Targ_FindNode(const char *name, int flags)
{
	GNode		*gn;	/* node in that element */
	Hash_Entry	*he;	/* New or used hash entry for node */
	Boolean		isNew;	/* Set TRUE if Hash_CreateEntry had to create */
		      		/* an entry for the node */

	if (flags & TARG_CREATE) {
		he = Hash_CreateEntry(&targets, name, &isNew);
		if (isNew) {
			gn = Targ_NewGN(name);
			Hash_SetValue(he, gn);
			Lst_AtEnd(&allTargets, gn);
		}
	} else {
		he = Hash_FindEntry(&targets, name);
	}

	if (he == NULL) {
		return (NULL);
	} else {
		return (Hash_GetValue(he));
	}
}

/**
 * Targ_FindList
 *	Make a complete list of GNodes from the given list of names
 *
 * Results:
 *	A complete list of graph nodes corresponding to all instances of all
 *	the names in names.
 *
 * Side Effects:
 *	If flags is TARG_CREATE, nodes will be created for all names in
 *	names which do not yet have graph nodes. If flags is TARG_NOCREATE,
 *	an error message will be printed for each name which can't be found.
 */
void
Targ_FindList(Lst *nodes, Lst *names, int flags)
{
	LstNode	*ln;	/* name list element */
	GNode	*gn;	/* node in tLn */
	char	*name;

	for (ln = Lst_First(names); ln != NULL; ln = Lst_Succ(ln)) {
		name = Lst_Datum(ln);
		gn = Targ_FindNode(name, flags);
		if (gn != NULL) {
			/*
			 * Note: Lst_AtEnd must come before the Lst_Concat so
			 * the nodes are added to the list in the order in which
			 * they were encountered in the makefile.
			 */
			Lst_AtEnd(nodes, gn);
			if (gn->type & OP_DOUBLEDEP) {
				Lst_Concat(nodes, &gn->cohorts, LST_CONCNEW);
			}

		} else if (flags == TARG_NOCREATE) {
			Error("\"%s\" -- target unknown.", name);
		}
	}
}

/**
 * Targ_Ignore
 *	Return true if should ignore errors when creating gn
 *
 * Results:
 *	TRUE if should ignore errors
 */
Boolean
Targ_Ignore(GNode *gn)
{

	if (ignoreErrors || (gn->type & OP_IGNORE)) {
		return (TRUE);
	} else {
		return (FALSE);
	}
}

/**
 * Targ_Silent
 *	Return true if be silent when creating gn
 *
 * Results:
 *	TRUE if should be silent
 */
Boolean
Targ_Silent(GNode *gn)
{

	if (beSilent || (gn->type & OP_SILENT)) {
		return (TRUE);
	} else {
		return (FALSE);
	}
}

/**
 * Targ_Precious
 *	See if the given target is precious
 *
 * Results:
 *	TRUE if it is precious. FALSE otherwise
 */
Boolean
Targ_Precious(GNode *gn)
{

	if (allPrecious || (gn->type & (OP_PRECIOUS | OP_DOUBLEDEP))) {
		return (TRUE);
	} else {
		return (FALSE);
	}
}

static GNode	*mainTarg;	/* the main target, as set by Targ_SetMain */

/**
 * Targ_SetMain
 *	Set our idea of the main target we'll be creating. Used for
 *	debugging output.
 *
 * Side Effects:
 *	"mainTarg" is set to the main target's node.
 */
void
Targ_SetMain(GNode *gn)
{

	mainTarg = gn;
}

/**
 * Targ_FmtTime
 *	Format a modification time in some reasonable way and return it.
 *
 * Results:
 *	The time reformatted.
 *
 * Side Effects:
 *	The time is placed in a static area, so it is overwritten
 *	with each call.
 */
char *
Targ_FmtTime(time_t modtime)
{
	struct tm	*parts;
	static char	buf[128];

	parts = localtime(&modtime);

	strftime(buf, sizeof(buf), "%H:%M:%S %b %d, %Y", parts);
	buf[sizeof(buf) - 1] = '\0';
	return (buf);
}

/**
 * Targ_PrintType
 *	Print out a type field giving only those attributes the user can
 *	set.
 */
void
Targ_PrintType(int type)
{
	static const struct flag2str type2str[] = {
		{ OP_OPTIONAL,	".OPTIONAL" },
		{ OP_USE,	".USE" },
		{ OP_EXEC,	".EXEC" },
		{ OP_IGNORE,	".IGNORE" },
		{ OP_PRECIOUS,	".PRECIOUS" },
		{ OP_SILENT,	".SILENT" },
		{ OP_MAKE,	".MAKE" },
		{ OP_JOIN,	".JOIN" },
		{ OP_INVISIBLE,	".INVISIBLE" },
		{ OP_NOTMAIN,	".NOTMAIN" },
		{ OP_PHONY,	".PHONY" },
		{ OP_LIB,	".LIB" },
		{ OP_MEMBER,	".MEMBER" },
		{ OP_ARCHV,	".ARCHV" },
		{ 0,		NULL }
	};

	type &= ~OP_OPMASK;
	if (!DEBUG(TARG))
		type &= ~(OP_ARCHV | OP_LIB | OP_MEMBER);
	print_flags(stdout, type2str, type, 0);
}

/**
 * TargPrintNode
 *	print the contents of a node
 */
static int
TargPrintNode(const GNode *gn, int pass)
{
	const LstNode	*tln;

	if (!OP_NOP(gn->type)) {
		printf("#\n");
		if (gn == mainTarg) {
			printf("# *** MAIN TARGET ***\n");
		}
		if (pass == 2) {
			if (gn->unmade) {
				printf("# %d unmade children\n", gn->unmade);
			} else {
				printf("# No unmade children\n");
			}
			if (!(gn->type & (OP_JOIN | OP_USE | OP_EXEC))) {
				if (gn->mtime != 0) {
					printf("# last modified %s: %s\n",
					    Targ_FmtTime(gn->mtime),
					    gn->made == UNMADE ? "unmade" :
					    gn->made == MADE ? "made" :
					    gn->made == UPTODATE ? "up-to-date":
					    "error when made");
				} else if (gn->made != UNMADE) {
					printf("# non-existent (maybe): %s\n",
					    gn->made == MADE ? "made" :
					    gn->made == UPTODATE ? "up-to-date":
					    gn->made == ERROR?"error when made":
				 	    "aborted");
				} else {
					printf("# unmade\n");
				}
			}
			if (!Lst_IsEmpty(&gn->iParents)) {
				printf("# implicit parents: ");
				LST_FOREACH(tln, &gn->iParents)
					printf("%s ", ((const GNode *)
					    Lst_Datum(tln))->name);
				printf("\n");
			}
		}
		if (!Lst_IsEmpty(&gn->parents)) {
			printf("# parents: ");
			LST_FOREACH(tln, &gn->parents)
				printf("%s ", ((const GNode *)
				    Lst_Datum(tln))->name);
			printf("\n");
		}

		printf("%-16s", gn->name);
		switch (gn->type & OP_OPMASK) {
		  case OP_DEPENDS:
			printf(": ");
			break;
		  case OP_FORCE:
			printf("! ");
			break;
		  case OP_DOUBLEDEP:
			printf(":: ");
			break;
		  default:
			break;
		}
		Targ_PrintType(gn->type);
		LST_FOREACH(tln, &gn->children)
			printf("%s ", ((const GNode *)Lst_Datum(tln))->name);
		printf("\n");
		LST_FOREACH(tln, &gn->commands)
			printf("\t%s\n", (const char *)Lst_Datum(tln));
		printf("\n\n");
		if (gn->type & OP_DOUBLEDEP) {
			LST_FOREACH(tln, &gn->cohorts)
				TargPrintNode((const GNode *)Lst_Datum(tln),
				    pass);
		}
	}
	return (0);
}

/**
 * Targ_PrintGraph
 *	Print the entire graph.
 */
void
Targ_PrintGraph(int pass)
{
	const GNode	*gn;
	const LstNode	*tln;

	printf("#*** Input graph:\n");
	LST_FOREACH(tln, &allTargets)
		TargPrintNode((const GNode *)Lst_Datum(tln), pass);
	printf("\n\n");

	printf("#\n#   Files that are only sources:\n");
	LST_FOREACH(tln, &allTargets) {
		gn = Lst_Datum(tln);
		if (OP_NOP(gn->type))
			printf("#\t%s [%s]\n", gn->name,
			    gn->path ? gn->path : gn->name);
	}
	Var_Dump();
	printf("\n");
	Dir_PrintDirectories();
	printf("\n");
	Suff_PrintAll();
}
