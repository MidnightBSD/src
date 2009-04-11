/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)back.h	8.1 (Berkeley) 5/31/93
 * $DragonFly: src/games/backgammon/common_source/back.h,v 1.2 2006/08/08 16:36:11 pavalos Exp $
 */

#include <sgtty.h>
#include <stdlib.h>
#include <unistd.h>

#define rnum(r)	(random()%r)
#define D0	dice[0]
#define D1	dice[1]
#define swap	{D0 ^= D1; D1 ^= D0; D0 ^= D1; d0 = 1-d0;}

/*
 *
 * Some numerical conventions:
 *
 *	Arrays have white's value in [0], red in [1].
 *	Numeric values which are one color or the other use
 *	-1 for white, 1 for red.
 *	Hence, white will be negative values, red positive one.
 *	This makes a lot of sense since white is going in decending
 *	order around the board, and red is ascending.
 *
 */

extern const char	EXEC[];	/* object for main program */
extern const char	TEACH[];/* object for tutorial program */

extern int	pnum;		/* color of player:
					-1 = white
					 1 = red
					 0 = both
					 2 = not yet init'ed */
extern char	*args[16];	/* args passed to teachgammon and back */
extern int	acnt;		/* number of args */
extern int	aflag;		/* flag to ask for rules or instructions */
extern int	bflag;		/* flag for automatic board printing */
extern int	cflag;		/* case conversion flag */
extern int	hflag;		/* flag for cleaning screen */
extern int	mflag;		/* backgammon flag */
extern int	raflag;		/* 'roll again' flag for recovered game */
extern int	rflag;		/* recovered game flag */
extern int	tflag;		/* cursor addressing flag */
extern int	rfl;		/* saved value of rflag */
extern int	iroll;		/* special flag for inputting rolls */
extern int	board[26];	/* board:  negative values are white,
				   positive are red */
extern int	dice[2];	/* value of dice */
extern int	mvlim;		/* 'move limit':  max. number of moves */
extern int	mvl;		/* working copy of mvlim */
extern int	p[5];		/* starting position of moves */
extern int	g[5];		/* ending position of moves (goals) */
extern int	h[4];		/* flag for each move if a man was hit */
extern int	cturn;		/* whose turn it currently is:
					-1 = white
					 1 = red
					 0 = just quitted
					-2 = white just lost
					 2 = red just lost */
extern int	d0;		/* flag if dice have been reversed from
				   original position */
extern int	table[6][6];	/* odds table for possible rolls */
extern int	rscore;		/* red's score */
extern int	wscore;		/* white's score */
extern int	gvalue;		/* value of game (64 max.) */
extern int	dlast;		/* who doubled last (0 = neither) */
extern int	bar;		/* position of bar for current player */
extern int	home;		/* position of home for current player */
extern int	off[2];		/* number of men off board */
extern int	*offptr;	/* pointer to off for current player */
extern int	*offopp;	/* pointer to off for opponent */
extern int	in[2];		/* number of men in inner table */
extern int	*inptr;		/* pointer to in for current player */
extern int	*inopp;		/* pointer to in for opponent */

extern int	ncin;		/* number of characters in cin */
extern char	cin[100];	/* input line of current move
				   (used for reconstructing input after
				   a backspace) */

extern const char	*const color[];
				/* colors as strings */
extern const char *const *colorptr;	/* color of current player */
extern const char *const *Colorptr;	/* color of current player, capitalized */
extern int	colen;		/* length of color of current player */

extern struct sgttyb	tty;	/* tty information buffer */
extern int		old;	/* original tty status */
extern int		noech;	/* original tty status without echo */
extern int		raw;	/* raw tty status, no echo */

extern int	curr;		/* row position of cursor */
extern int	curc;		/* column position of cursor */
extern int	begscr;		/* 'beginning' of screen
				   (not including board) */

int	movallow(void);

void	wrboard(void);

void	getmove(void);
int	movokay(int);

void	fboard(void);
void	refresh(void);
void	curmove(int, int);
void	newpos(void);
void	clear(void);
void	fancyc(char);
void	clend(void);
void	cline(void);
int	getcaps(const char *);

void	odds(int, int, int);
int	count(void);
int	canhit(int, int);

int	makmove(int);
void	moverr(int);
void	movback(int);
void	backone(int);

void	save(int);
void	recover(const char *);

void	errexit(const char *);
int	addbuf(int);
void	buflush(void);
char	readc(void);
void	writec(char);
void	writel(const char *);
void	proll(void);
void	wrint(int);
void	gwrite(void);
int	quit(void);
int	yorn(char);
void	wrhit(int);
void	nexturn(void);
void	getarg(int, char **);
void	init(void);
void	wrscore(void);
void	fixtty(int);
void	getout(void);		/* function to exit backgammon cleanly */
void	roll(void);

int	checkmove(int);

void	dble(void);
int	dblgood(void);
int	freemen(int);
int	trapped(int, int);

void	move(int);

#ifdef	TEACHGAMMON_TEXT
int	text(const char *const *);
#else
void	text(const char *const *);
#endif
