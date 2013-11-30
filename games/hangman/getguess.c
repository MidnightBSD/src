/*
 * Copyright (c) 1983, 1993
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
 * @(#)getguess.c	8.1 (Berkeley) 5/31/93
 * $FreeBSD: src/games/hangman/getguess.c,v 1.6 1999/12/10 03:22:59 billf Exp $
 * $DragonFly: src/games/hangman/getguess.c,v 1.3 2005/02/13 18:57:30 cpressey Exp $
 */

#include <sys/ttydefaults.h>
#include "hangman.h"

/*
 * getguess:
 *	Get another guess
 */
void
getguess(void)
{
	int	i;
	int	ch;
	bool	correct;

	leaveok(stdscr, FALSE);
	for (;;) {
		move(PROMPTY, PROMPTX + sizeof("Guess: "));
		refresh();
		ch = readch();
		if (isalpha(ch)) {
			if (isupper(ch))
				ch = tolower(ch);
			if (Guessed[ch - 'a'])
				mvprintw(MESGY, MESGX, "Already guessed '%c'", ch);
			else
				break;
		}
		else if (ch == CTRL('D'))
			die(0);
		else
			mvprintw(MESGY, MESGX, "Not a valid guess: '%s'",
				unctrl(ch));
	}
	leaveok(stdscr, TRUE);
	move(MESGY, MESGX);
	clrtoeol();

	Guessed[ch - 'a'] = TRUE;
	correct = FALSE;
	for (i = 0; Word[i] != '\0'; i++) {
		if (Word[i] == ch) {
			Known[i] = ch;
			correct = TRUE;
		}
	}
	if (!correct)
		Errors++;
}

/*
 * readch;
 *	Read a character from the input
 */
char
readch(void)
{
	int	cnt;
	char	ch;
	int	x, y;

	cnt = 0;
	for (;;) {
		if (read(0, &ch, sizeof(ch)) <= 0) {
			if (++cnt > 100)
				die(0);
		} else if (ch == CTRL('L')) {
			wrefresh(curscr);
			getyx(curscr, y, x);
			mvcur(0, 0, y, x);
		} else
			return ch;
	}
}
