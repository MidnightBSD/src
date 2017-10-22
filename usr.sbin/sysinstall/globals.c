/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $FreeBSD$
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
 */

#include "sysinstall.h"

/*
 * Various global variables and an initialization hook to set them to
 * whatever values we feel are appropriate.
 */

int		DebugFD;	/* Where diagnostic output goes */
Boolean		Fake;		/* Only pretend to be useful */
Boolean		RunningAsInit;	/* Are we running as init? */
Boolean		DialogActive;	/* Is libdialog initialized? */
Boolean		ColorDisplay;	/* Are we on a color display? */
Boolean		OnVTY;		/* Are we on a VTY? */
Boolean		Restarting;	/* Are we restarting sysinstall? */
Boolean		have_volumes;	/* Media has more than one volume. */
Variable	*VarHead;	/* The head of the variable chain */
Device		*mediaDevice;	/* Where we're installing from */
int		BootMgr;	/* Which boot manager we're using */
int		StatusLine;	/* Where to stick our status messages */
int		low_volume;	/* Lowest volume number */
int		high_volume;	/* Highest volume number */
jmp_buf		BailOut;	/* Beam me up, scotty! The natives are pissed! */

Chunk		*HomeChunk;
Chunk		*RootChunk;
Chunk		*SwapChunk;
Chunk		*TmpChunk;
Chunk		*UsrChunk;
Chunk		*VarChunk;
#ifdef __ia64__
Chunk		*EfiChunk;
#endif

/*
 * Yes, I know some of these are already automatically initialized as
 * globals.  I simply find it clearer to set everything explicitly.
 */
void
globalsInit(void)
{
    DebugFD = -1;
    ColorDisplay = FALSE;
    OnVTY = FALSE;
    DialogActive = FALSE;
    VarHead = NULL;
    mediaDevice = NULL;

    HomeChunk = NULL;
    RootChunk = NULL;
    SwapChunk = NULL;
    TmpChunk = NULL;
    UsrChunk = NULL;
    VarChunk = NULL;
#ifdef __ia64__
    EfiChunk = NULL;
#endif
}
