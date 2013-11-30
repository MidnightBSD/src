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
 * $FreeBSD: src/usr.bin/make/GNode.h,v 1.4 2005/05/13 08:53:00 harti Exp $
 */

#ifndef GNode_h_39503bf2
#define	GNode_h_39503bf2

#include "lst.h"
#include "util.h"

struct Suff;

/*
 * The structure for an individual graph node. Each node has several
 * pieces of data associated with it.
 */
typedef struct GNode {
	char	*name;	/* The target's name */
	char	*path;	/* The full pathname of the target file */

	/*
	 * The type of operator used to define the sources (qv. parse.c)
	 *
	 * The OP_ constants are used when parsing a dependency line as a way of
	 * communicating to other parts of the program the way in which a target
	 * should be made. These constants are bitwise-OR'ed together and
	 * placed in the 'type' field of each node. Any node that has
	 * a 'type' field which satisfies the OP_NOP function was never never on
	 * the lefthand side of an operator, though it may have been on the
	 * righthand side...
	 */
	int	type;
#define	OP_DEPENDS	0x00000001	/* Execution of commands depends on
					 * kids (:) */
#define	OP_FORCE	0x00000002	/* Always execute commands (!) */
#define	OP_DOUBLEDEP	0x00000004	/* Execution of commands depends on
					 * kids per line (::) */
#define	OP_OPMASK	(OP_DEPENDS|OP_FORCE|OP_DOUBLEDEP)

#define	OP_OPTIONAL	0x00000008	/* Don't care if the target doesn't
					 * exist and can't be created */
#define	OP_USE		0x00000010	/*
					 * Use associated commands for
					 * parents
					 */
#define	OP_EXEC		0x00000020	/* Target is never out of date, but
					 * always execute commands anyway.
					 * Its time doesn't matter, so it has
					 * none...sort of
					 */
#define	OP_IGNORE	0x00000040	/*
					 * Ignore errors when creating the node
					 */
#define	OP_PRECIOUS	0x00000080	/* Don't remove the target when
					 * interrupted */
#define	OP_SILENT	0x00000100	/* Don't echo commands when executed */
#define	OP_MAKE		0x00000200	/*
					 * Target is a recurrsive make so its
					 * commands should always be executed
					 * when it is out of date, regardless
					 * of the state of the -n or -t flags
					 */
#define	OP_JOIN		0x00000400	/* Target is out-of-date only if any of
					 * its children was out-of-date */
#define	OP_INVISIBLE	0x00004000	/* The node is invisible to its parents.
					 * I.e. it doesn't show up in the
					 * parents's local variables. */
#define	OP_NOTMAIN	0x00008000	/* The node is exempt from normal 'main
					 * target' processing in parse.c */
#define	OP_PHONY	0x00010000	/* Not a file target; run always */
/* Attributes applied by PMake */
#define	OP_TRANSFORM	0x80000000	/* The node is a transformation rule */
#define	OP_MEMBER	0x40000000	/* Target is a member of an archive */
#define	OP_LIB		0x20000000	/* Target is a library */
#define	OP_ARCHV	0x10000000	/* Target is an archive construct */
#define	OP_HAS_COMMANDS	0x08000000	/* Target has all the commands it
					 * should.  Used when parsing to catch
					 * multiple commands for a target */
#define	OP_SAVE_CMDS	0x04000000	/* Saving commands on .END (Compat) */
#define	OP_DEPS_FOUND	0x02000000	/* Already processed by Suff_FindDeps */

/*
 * OP_NOP will return TRUE if the node with the given type was not the
 * object of a dependency operator
 */
#define	OP_NOP(t)	(((t) & OP_OPMASK) == 0x00000000)

	int	order;	/* Its wait weight */

	Boolean	make;	/* TRUE if this target needs to be remade */

	/* Set to reflect the state of processing on this node */
	enum {
		UNMADE,		/* Not examined yet */

		/*
		 * Target is already being made. Indicates a cycle in the graph.
		 * (compat mode only)
		 */
		BEINGMADE,

		MADE,		/* Was out-of-date and has been made */
		UPTODATE,	/* Was already up-to-date */

		/*
		 * An error occured while it was being
		 * made (used only in compat mode)
		 */
		ERROR,

		/*
		 * The target was aborted due to an
		 * error making an inferior (compat).
		 */
		ABORTED,

		/*
		 * Marked as potentially being part of a graph cycle.  If we
		 * come back to a node marked this way, it is printed and
		 * 'made' is changed to ENDCYCLE.
		 */
		CYCLE,

		/*
		 * The cycle has been completely printed.  Go back and
		 * unmark all its members.
		 */
		ENDCYCLE
	} made;

	/* TRUE if one of this target's children was made */
	Boolean	childMade;

	int	unmade;		/* The number of unmade children */
	int	mtime;		/* Its modification time */
	int	cmtime;		/* Modification time of its youngest child */

	/*
	 * Links to parents for which this is an implied source, if any. (nodes
	 * that depend on this, as gleaned from the transformation rules.
	 */
	Lst	iParents;

	/* List of nodes of the same name created by the :: operator */
	Lst	cohorts;

	/* Lst of nodes for which this is a source (that depend on this one) */
	Lst	parents;

	/* List of nodes on which this depends */
	Lst	children;

	/*
	 * List of nodes that must be made (if they're made) after this node is,
	 * but that do not depend on this node, in the normal sense.
	 */
	Lst	successors;

	/*
	 * List of nodes that must be made (if they're made) before this node
	 * can be, but that do no enter into the datedness of this node.
	 */
	Lst	preds;

	/*
	 * List of ``local'' variables that are specific to this target
	 * and this target only (qv. var.c [$@ $< $?, etc.])
	 */
	Lst	context;

	/*
	 * List of strings that are commands to be given to a shell
	 * to create this target.
	 */
	Lst	commands;

	/* current command executing in compat mode */
	LstNode	*compat_command;

	/*
	 * Suffix for the node (determined by Suff_FindDeps and opaque to
	 * everyone but the Suff module)
	 */
	struct Suff	*suffix;
} GNode;

#endif /* GNode_h_39503bf2 */
