/*	$NetBSD: refresh.c,v 1.5 2005/06/09 16:48:58 lukem Exp $	*/
/*	from	NetBSD: refresh.c,v 1.26 2003/08/07 16:44:33 agc Exp	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Christos Zoulas of Cornell University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include "tnftp.h"
#include "sys.h"

/*
 * refresh.c: Lower level screen refreshing functions
 */
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>

#include "el.h"

private void	re_addc(EditLine *, int);
private void	re_update_line(EditLine *, char *, char *, int);
private void	re_insert (EditLine *, char *, int, int, char *, int);
private void	re_delete(EditLine *, char *, int, int, int);
private void	re_fastputc(EditLine *, int);
private void	re__strncopy(char *, char *, size_t);
private void	re__copy_and_pad(char *, const char *, size_t);

#ifdef DEBUG_REFRESH
private void	re_printstr(EditLine *, const char *, char *, char *);
#define	__F el->el_errfile
#define	ELRE_ASSERT(a, b, c)	do 				\
				    if (/*CONSTCOND*/ a) {	\
					(void) fprintf b;	\
					c;			\
				    }				\
				while (/*CONSTCOND*/0)
#define	ELRE_DEBUG(a, b)	ELRE_ASSERT(a,b,;)

/* re_printstr():
 *	Print a string on the debugging pty
 */
private void
re_printstr(EditLine *el, const char *str, char *f, char *t)
{

	ELRE_DEBUG(1, (__F, "%s:\"", str));
	while (f < t)
		ELRE_DEBUG(1, (__F, "%c", *f++ & 0177));
	ELRE_DEBUG(1, (__F, "\"\r\n"));
}
#else
#define	ELRE_ASSERT(a, b, c)
#define	ELRE_DEBUG(a, b)
#endif


/* re_addc():
 *	Draw c, expanding tabs, control chars etc.
 */
private void
re_addc(EditLine *el, int c)
{

	if (isprint(c)) {
		re_putc(el, c, 1);
		return;
	}
	if (c == '\n') {				/* expand the newline */
		int oldv = el->el_refresh.r_cursor.v;
		re_putc(el, '\0', 0);			/* assure end of line */
		if (oldv == el->el_refresh.r_cursor.v) { /* XXX */
			el->el_refresh.r_cursor.h = 0;	/* reset cursor pos */
			el->el_refresh.r_cursor.v++;
		}
		return;
	}
	if (c == '\t') {				/* expand the tab */
		for (;;) {
			re_putc(el, ' ', 1);
			if ((el->el_refresh.r_cursor.h & 07) == 0)
				break;			/* go until tab stop */
		}
	} else if (iscntrl(c)) {
		re_putc(el, '^', 1);
		if (c == '\177')
			re_putc(el, '?', 1);
		else
		    /* uncontrolify it; works only for iso8859-1 like sets */
			re_putc(el, (c | 0100), 1);
	} else {
		re_putc(el, '\\', 1);
		re_putc(el, (int) ((((unsigned int) c >> 6) & 07) + '0'), 1);
		re_putc(el, (int) ((((unsigned int) c >> 3) & 07) + '0'), 1);
		re_putc(el, (c & 07) + '0', 1);
	}
}


/* re_putc():
 *	Draw the character given
 */
protected void
re_putc(EditLine *el, int c, int shift)
{

	ELRE_DEBUG(1, (__F, "printing %3.3o '%c'\r\n", c, c));

	el->el_vdisplay[el->el_refresh.r_cursor.v][el->el_refresh.r_cursor.h] = c;
	if (!shift)
		return;

	el->el_refresh.r_cursor.h++;	/* advance to next place */
	if (el->el_refresh.r_cursor.h >= el->el_term.t_size.h) {
		el->el_vdisplay[el->el_refresh.r_cursor.v][el->el_term.t_size.h] = '\0';
		/* assure end of line */
		el->el_refresh.r_cursor.h = 0;	/* reset it. */

		/*
		 * If we would overflow (input is longer than terminal size),
		 * emulate scroll by dropping first line and shuffling the rest.
		 * We do this via pointer shuffling - it's safe in this case
		 * and we avoid memcpy().
		 */
		if (el->el_refresh.r_cursor.v + 1 >= el->el_term.t_size.v) {
			int i, lins = el->el_term.t_size.v;
			char *firstline = el->el_vdisplay[0];

			for(i=1; i < lins; i++)
				el->el_vdisplay[i-1] = el->el_vdisplay[i];

			firstline[0] = '\0';		/* empty the string */	
			el->el_vdisplay[i-1] = firstline;
		} else
			el->el_refresh.r_cursor.v++;

		ELRE_ASSERT(el->el_refresh.r_cursor.v >= el->el_term.t_size.v,
		    (__F, "\r\nre_putc: overflow! r_cursor.v == %d > %d\r\n",
		    el->el_refresh.r_cursor.v, el->el_term.t_size.v),
		    abort());
	}
}


/* re_refresh():
 *	draws the new virtual screen image from the current input
 *  	line, then goes line-by-line changing the real image to the new
 *	virtual image. The routine to re-draw a line can be replaced
 *	easily in hopes of a smarter one being placed there.
 */
protected void
re_refresh(EditLine *el)
{
	int i, rhdiff;
	char *cp, *st;
	coord_t cur;
#ifdef notyet
	size_t termsz;
#endif

	ELRE_DEBUG(1, (__F, "el->el_line.buffer = :%s:\r\n",
	    el->el_line.buffer));

	/* reset the Drawing cursor */
	el->el_refresh.r_cursor.h = 0;
	el->el_refresh.r_cursor.v = 0;

	/* temporarily draw rprompt to calculate its size */
	prompt_print(el, EL_RPROMPT);

	/* reset the Drawing cursor */
	el->el_refresh.r_cursor.h = 0;
	el->el_refresh.r_cursor.v = 0;

	if (el->el_line.cursor >= el->el_line.lastchar) {
		if (el->el_map.current == el->el_map.alt
		    && el->el_line.lastchar != el->el_line.buffer)
			el->el_line.cursor = el->el_line.lastchar - 1;
		else
			el->el_line.cursor = el->el_line.lastchar;
	}

	cur.h = -1;		/* set flag in case I'm not set */
	cur.v = 0;

	prompt_print(el, EL_PROMPT);

	/* draw the current input buffer */
#if notyet
	termsz = el->el_term.t_size.h * el->el_term.t_size.v;
	if (el->el_line.lastchar - el->el_line.buffer > termsz) {
		/*
		 * If line is longer than terminal, process only part
		 * of line which would influence display.
		 */
		size_t rem = (el->el_line.lastchar-el->el_line.buffer)%termsz;

		st = el->el_line.lastchar - rem
			- (termsz - (((rem / el->el_term.t_size.v) - 1)
					* el->el_term.t_size.v));
	} else
#endif
		st = el->el_line.buffer;

	for (cp = st; cp < el->el_line.lastchar; cp++) {
		if (cp == el->el_line.cursor) {
			/* save for later */
			cur.h = el->el_refresh.r_cursor.h;
			cur.v = el->el_refresh.r_cursor.v;
		}
		re_addc(el, (unsigned char) *cp);
	}

	if (cur.h == -1) {	/* if I haven't been set yet, I'm at the end */
		cur.h = el->el_refresh.r_cursor.h;
		cur.v = el->el_refresh.r_cursor.v;
	}
	rhdiff = el->el_term.t_size.h - el->el_refresh.r_cursor.h -
	    el->el_rprompt.p_pos.h;
	if (el->el_rprompt.p_pos.h && !el->el_rprompt.p_pos.v &&
	    !el->el_refresh.r_cursor.v && rhdiff > 1) {
		/*
		 * have a right-hand side prompt that will fit
		 * on the end of the first line with at least
		 * one character gap to the input buffer.
		 */
		while (--rhdiff > 0)	/* pad out with spaces */
			re_putc(el, ' ', 1);
		prompt_print(el, EL_RPROMPT);
	} else {
		el->el_rprompt.p_pos.h = 0;	/* flag "not using rprompt" */
		el->el_rprompt.p_pos.v = 0;
	}

	re_putc(el, '\0', 0);	/* make line ended with NUL, no cursor shift */

	el->el_refresh.r_newcv = el->el_refresh.r_cursor.v;

	ELRE_DEBUG(1, (__F,
		"term.h=%d vcur.h=%d vcur.v=%d vdisplay[0]=\r\n:%80.80s:\r\n",
		el->el_term.t_size.h, el->el_refresh.r_cursor.h,
		el->el_refresh.r_cursor.v, el->el_vdisplay[0]));

	ELRE_DEBUG(1, (__F, "updating %d lines.\r\n", el->el_refresh.r_newcv));
	for (i = 0; i <= el->el_refresh.r_newcv; i++) {
		/* NOTE THAT re_update_line MAY CHANGE el_display[i] */
		re_update_line(el, el->el_display[i], el->el_vdisplay[i], i);

		/*
		 * Copy the new line to be the current one, and pad out with
		 * spaces to the full width of the terminal so that if we try
		 * moving the cursor by writing the character that is at the
		 * end of the screen line, it won't be a NUL or some old
		 * leftover stuff.
		 */
		re__copy_and_pad(el->el_display[i], el->el_vdisplay[i],
		    (size_t) el->el_term.t_size.h);
	}
	ELRE_DEBUG(1, (__F,
	"\r\nel->el_refresh.r_cursor.v=%d,el->el_refresh.r_oldcv=%d i=%d\r\n",
	    el->el_refresh.r_cursor.v, el->el_refresh.r_oldcv, i));

	if (el->el_refresh.r_oldcv > el->el_refresh.r_newcv)
		for (; i <= el->el_refresh.r_oldcv; i++) {
			term_move_to_line(el, i);
			term_move_to_char(el, 0);
			term_clear_EOL(el, (int) strlen(el->el_display[i]));
#ifdef DEBUG_REFRESH
			term_overwrite(el, "C\b", 2);
#endif /* DEBUG_REFRESH */
			el->el_display[i][0] = '\0';
		}

	el->el_refresh.r_oldcv = el->el_refresh.r_newcv; /* set for next time */
	ELRE_DEBUG(1, (__F,
	    "\r\ncursor.h = %d, cursor.v = %d, cur.h = %d, cur.v = %d\r\n",
	    el->el_refresh.r_cursor.h, el->el_refresh.r_cursor.v,
	    cur.h, cur.v));
	term_move_to_line(el, cur.v);	/* go to where the cursor is */
	term_move_to_char(el, cur.h);
}


/* re_goto_bottom():
 *	 used to go to last used screen line
 */
protected void
re_goto_bottom(EditLine *el)
{

	term_move_to_line(el, el->el_refresh.r_oldcv);
	term__putc('\n');
	re_clear_display(el);
	term__flush();
}


/* re_insert():
 *	insert num characters of s into d (in front of the character)
 *	at dat, maximum length of d is dlen
 */
private void
/*ARGSUSED*/
re_insert(EditLine *el __attribute__((__unused__)),
    char *d, int dat, int dlen, char *s, int num)
{
	char *a, *b;

	if (num <= 0)
		return;
	if (num > dlen - dat)
		num = dlen - dat;

	ELRE_DEBUG(1,
	    (__F, "re_insert() starting: %d at %d max %d, d == \"%s\"\n",
	    num, dat, dlen, d));
	ELRE_DEBUG(1, (__F, "s == \"%s\"n", s));

	/* open up the space for num chars */
	if (num > 0) {
		b = d + dlen - 1;
		a = b - num;
		while (a >= &d[dat])
			*b-- = *a--;
		d[dlen] = '\0';	/* just in case */
	}
	ELRE_DEBUG(1, (__F,
		"re_insert() after insert: %d at %d max %d, d == \"%s\"\n",
		num, dat, dlen, d));
	ELRE_DEBUG(1, (__F, "s == \"%s\"n", s));

	/* copy the characters */
	for (a = d + dat; (a < d + dlen) && (num > 0); num--)
		*a++ = *s++;

	ELRE_DEBUG(1,
	    (__F, "re_insert() after copy: %d at %d max %d, %s == \"%s\"\n",
	    num, dat, dlen, d, s));
	ELRE_DEBUG(1, (__F, "s == \"%s\"n", s));
}


/* re_delete():
 *	delete num characters d at dat, maximum length of d is dlen
 */
private void
/*ARGSUSED*/
re_delete(EditLine *el __attribute__((__unused__)),
    char *d, int dat, int dlen, int num)
{
	char *a, *b;

	if (num <= 0)
		return;
	if (dat + num >= dlen) {
		d[dat] = '\0';
		return;
	}
	ELRE_DEBUG(1,
	    (__F, "re_delete() starting: %d at %d max %d, d == \"%s\"\n",
	    num, dat, dlen, d));

	/* open up the space for num chars */
	if (num > 0) {
		b = d + dat;
		a = b + num;
		while (a < &d[dlen])
			*b++ = *a++;
		d[dlen] = '\0';	/* just in case */
	}
	ELRE_DEBUG(1,
	    (__F, "re_delete() after delete: %d at %d max %d, d == \"%s\"\n",
	    num, dat, dlen, d));
}


/* re__strncopy():
 *	Like strncpy without padding.
 */
private void
re__strncopy(char *a, char *b, size_t n)
{

	while (n-- && *b)
		*a++ = *b++;
}


/*****************************************************************
    re_update_line() is based on finding the middle difference of each line
    on the screen; vis:

			     /old first difference
	/beginning of line   |              /old last same       /old EOL
	v		     v              v                    v
old:	eddie> Oh, my little gruntle-buggy is to me, as lurgid as
new:	eddie> Oh, my little buggy says to me, as lurgid as
	^		     ^        ^			   ^
	\beginning of line   |        \new last same	   \new end of line
			     \new first difference

    all are character pointers for the sake of speed.  Special cases for
    no differences, as well as for end of line additions must be handled.
**************************************************************** */

/* Minimum at which doing an insert it "worth it".  This should be about
 * half the "cost" of going into insert mode, inserting a character, and
 * going back out.  This should really be calculated from the termcap
 * data...  For the moment, a good number for ANSI terminals.
 */
#define	MIN_END_KEEP	4

private void
re_update_line(EditLine *el, char *old, char *new, int i)
{
	char *o, *n, *p, c;
	char *ofd, *ols, *oe, *nfd, *nls, *ne;
	char *osb, *ose, *nsb, *nse;
	int fx, sx;

	/*
         * find first diff
         */
	for (o = old, n = new; *o && (*o == *n); o++, n++)
		continue;
	ofd = o;
	nfd = n;

	/*
         * Find the end of both old and new
         */
	while (*o)
		o++;
	/*
         * Remove any trailing blanks off of the end, being careful not to
         * back up past the beginning.
         */
	while (ofd < o) {
		if (o[-1] != ' ')
			break;
		o--;
	}
	oe = o;
	*oe = '\0';

	while (*n)
		n++;

	/* remove blanks from end of new */
	while (nfd < n) {
		if (n[-1] != ' ')
			break;
		n--;
	}
	ne = n;
	*ne = '\0';

	/*
         * if no diff, continue to next line of redraw
         */
	if (*ofd == '\0' && *nfd == '\0') {
		ELRE_DEBUG(1, (__F, "no difference.\r\n"));
		return;
	}
	/*
         * find last same pointer
         */
	while ((o > ofd) && (n > nfd) && (*--o == *--n))
		continue;
	ols = ++o;
	nls = ++n;

	/*
         * find same begining and same end
         */
	osb = ols;
	nsb = nls;
	ose = ols;
	nse = nls;

	/*
         * case 1: insert: scan from nfd to nls looking for *ofd
         */
	if (*ofd) {
		for (c = *ofd, n = nfd; n < nls; n++) {
			if (c == *n) {
				for (o = ofd, p = n;
				    p < nls && o < ols && *o == *p;
				    o++, p++)
					continue;
				/*
				 * if the new match is longer and it's worth
				 * keeping, then we take it
				 */
				if (((nse - nsb) < (p - n)) &&
				    (2 * (p - n) > n - nfd)) {
					nsb = n;
					nse = p;
					osb = ofd;
					ose = o;
				}
			}
		}
	}
	/*
         * case 2: delete: scan from ofd to ols looking for *nfd
         */
	if (*nfd) {
		for (c = *nfd, o = ofd; o < ols; o++) {
			if (c == *o) {
				for (n = nfd, p = o;
				    p < ols && n < nls && *p == *n;
				    p++, n++)
					continue;
				/*
				 * if the new match is longer and it's worth
				 * keeping, then we take it
				 */
				if (((ose - osb) < (p - o)) &&
				    (2 * (p - o) > o - ofd)) {
					nsb = nfd;
					nse = n;
					osb = o;
					ose = p;
				}
			}
		}
	}
	/*
         * Pragmatics I: If old trailing whitespace or not enough characters to
         * save to be worth it, then don't save the last same info.
         */
	if ((oe - ols) < MIN_END_KEEP) {
		ols = oe;
		nls = ne;
	}
	/*
         * Pragmatics II: if the terminal isn't smart enough, make the data
         * dumber so the smart update doesn't try anything fancy
         */

	/*
         * fx is the number of characters we need to insert/delete: in the
         * beginning to bring the two same begins together
         */
	fx = (nsb - nfd) - (osb - ofd);
	/*
         * sx is the number of characters we need to insert/delete: in the
         * end to bring the two same last parts together
         */
	sx = (nls - nse) - (ols - ose);

	if (!EL_CAN_INSERT) {
		if (fx > 0) {
			osb = ols;
			ose = ols;
			nsb = nls;
			nse = nls;
		}
		if (sx > 0) {
			ols = oe;
			nls = ne;
		}
		if ((ols - ofd) < (nls - nfd)) {
			ols = oe;
			nls = ne;
		}
	}
	if (!EL_CAN_DELETE) {
		if (fx < 0) {
			osb = ols;
			ose = ols;
			nsb = nls;
			nse = nls;
		}
		if (sx < 0) {
			ols = oe;
			nls = ne;
		}
		if ((ols - ofd) > (nls - nfd)) {
			ols = oe;
			nls = ne;
		}
	}
	/*
         * Pragmatics III: make sure the middle shifted pointers are correct if
         * they don't point to anything (we may have moved ols or nls).
         */
	/* if the change isn't worth it, don't bother */
	/* was: if (osb == ose) */
	if ((ose - osb) < MIN_END_KEEP) {
		osb = ols;
		ose = ols;
		nsb = nls;
		nse = nls;
	}
	/*
         * Now that we are done with pragmatics we recompute fx, sx
         */
	fx = (nsb - nfd) - (osb - ofd);
	sx = (nls - nse) - (ols - ose);

	ELRE_DEBUG(1, (__F, "\n"));
	ELRE_DEBUG(1, (__F, "ofd %d, osb %d, ose %d, ols %d, oe %d\n",
		ofd - old, osb - old, ose - old, ols - old, oe - old));
	ELRE_DEBUG(1, (__F, "nfd %d, nsb %d, nse %d, nls %d, ne %d\n",
		nfd - new, nsb - new, nse - new, nls - new, ne - new));
	ELRE_DEBUG(1, (__F,
		"xxx-xxx:\"00000000001111111111222222222233333333334\"\r\n"));
	ELRE_DEBUG(1, (__F,
		"xxx-xxx:\"01234567890123456789012345678901234567890\"\r\n"));
#ifdef DEBUG_REFRESH
	re_printstr(el, "old- oe", old, oe);
	re_printstr(el, "new- ne", new, ne);
	re_printstr(el, "old-ofd", old, ofd);
	re_printstr(el, "new-nfd", new, nfd);
	re_printstr(el, "ofd-osb", ofd, osb);
	re_printstr(el, "nfd-nsb", nfd, nsb);
	re_printstr(el, "osb-ose", osb, ose);
	re_printstr(el, "nsb-nse", nsb, nse);
	re_printstr(el, "ose-ols", ose, ols);
	re_printstr(el, "nse-nls", nse, nls);
	re_printstr(el, "ols- oe", ols, oe);
	re_printstr(el, "nls- ne", nls, ne);
#endif /* DEBUG_REFRESH */

	/*
         * el_cursor.v to this line i MUST be in this routine so that if we
         * don't have to change the line, we don't move to it. el_cursor.h to
         * first diff char
         */
	term_move_to_line(el, i);

	/*
         * at this point we have something like this:
         *
         * /old                  /ofd    /osb               /ose    /ols     /oe
         * v.....................v       v..................v       v........v
         * eddie> Oh, my fredded gruntle-buggy is to me, as foo var lurgid as
         * eddie> Oh, my fredded quiux buggy is to me, as gruntle-lurgid as
         * ^.....................^     ^..................^       ^........^
         * \new                  \nfd  \nsb               \nse     \nls    \ne
         *
         * fx is the difference in length between the chars between nfd and
         * nsb, and the chars between ofd and osb, and is thus the number of
         * characters to delete if < 0 (new is shorter than old, as above),
         * or insert (new is longer than short).
         *
         * sx is the same for the second differences.
         */

	/*
         * if we have a net insert on the first difference, AND inserting the
         * net amount ((nsb-nfd) - (osb-ofd)) won't push the last useful
         * character (which is ne if nls != ne, otherwise is nse) off the edge
	 * of the screen (el->el_term.t_size.h) else we do the deletes first
	 * so that we keep everything we need to.
         */

	/*
         * if the last same is the same like the end, there is no last same
         * part, otherwise we want to keep the last same part set p to the
         * last useful old character
         */
	p = (ols != oe) ? oe : ose;

	/*
         * if (There is a diffence in the beginning) && (we need to insert
         *   characters) && (the number of characters to insert is less than
         *   the term width)
	 *	We need to do an insert!
	 * else if (we need to delete characters)
	 *	We need to delete characters!
	 * else
	 *	No insert or delete
         */
	if ((nsb != nfd) && fx > 0 &&
	    ((p - old) + fx <= el->el_term.t_size.h)) {
		ELRE_DEBUG(1,
		    (__F, "first diff insert at %d...\r\n", nfd - new));
		/*
		 * Move to the first char to insert, where the first diff is.
		 */
		term_move_to_char(el, nfd - new);
		/*
		 * Check if we have stuff to keep at end
		 */
		if (nsb != ne) {
			ELRE_DEBUG(1, (__F, "with stuff to keep at end\r\n"));
			/*
		         * insert fx chars of new starting at nfd
		         */
			if (fx > 0) {
				ELRE_DEBUG(!EL_CAN_INSERT, (__F,
				"ERROR: cannot insert in early first diff\n"));
				term_insertwrite(el, nfd, fx);
				re_insert(el, old, ofd - old,
				    el->el_term.t_size.h, nfd, fx);
			}
			/*
		         * write (nsb-nfd) - fx chars of new starting at
		         * (nfd + fx)
			 */
			term_overwrite(el, nfd + fx, (nsb - nfd) - fx);
			re__strncopy(ofd + fx, nfd + fx,
			    (size_t) ((nsb - nfd) - fx));
		} else {
			ELRE_DEBUG(1, (__F, "without anything to save\r\n"));
			term_overwrite(el, nfd, (nsb - nfd));
			re__strncopy(ofd, nfd, (size_t) (nsb - nfd));
			/*
		         * Done
		         */
			return;
		}
	} else if (fx < 0) {
		ELRE_DEBUG(1,
		    (__F, "first diff delete at %d...\r\n", ofd - old));
		/*
		 * move to the first char to delete where the first diff is
		 */
		term_move_to_char(el, ofd - old);
		/*
		 * Check if we have stuff to save
		 */
		if (osb != oe) {
			ELRE_DEBUG(1, (__F, "with stuff to save at end\r\n"));
			/*
		         * fx is less than zero *always* here but we check
		         * for code symmetry
		         */
			if (fx < 0) {
				ELRE_DEBUG(!EL_CAN_DELETE, (__F,
				    "ERROR: cannot delete in first diff\n"));
				term_deletechars(el, -fx);
				re_delete(el, old, ofd - old,
				    el->el_term.t_size.h, -fx);
			}
			/*
		         * write (nsb-nfd) chars of new starting at nfd
		         */
			term_overwrite(el, nfd, (nsb - nfd));
			re__strncopy(ofd, nfd, (size_t) (nsb - nfd));

		} else {
			ELRE_DEBUG(1, (__F,
			    "but with nothing left to save\r\n"));
			/*
		         * write (nsb-nfd) chars of new starting at nfd
		         */
			term_overwrite(el, nfd, (nsb - nfd));
			ELRE_DEBUG(1, (__F,
			    "cleareol %d\n", (oe - old) - (ne - new)));
			term_clear_EOL(el, (oe - old) - (ne - new));
			/*
		         * Done
		         */
			return;
		}
	} else
		fx = 0;

	if (sx < 0 && (ose - old) + fx < el->el_term.t_size.h) {
		ELRE_DEBUG(1, (__F,
		    "second diff delete at %d...\r\n", (ose - old) + fx));
		/*
		 * Check if we have stuff to delete
		 */
		/*
		 * fx is the number of characters inserted (+) or deleted (-)
		 */

		term_move_to_char(el, (ose - old) + fx);
		/*
		 * Check if we have stuff to save
		 */
		if (ols != oe) {
			ELRE_DEBUG(1, (__F, "with stuff to save at end\r\n"));
			/*
		         * Again a duplicate test.
		         */
			if (sx < 0) {
				ELRE_DEBUG(!EL_CAN_DELETE, (__F,
				    "ERROR: cannot delete in second diff\n"));
				term_deletechars(el, -sx);
			}
			/*
		         * write (nls-nse) chars of new starting at nse
		         */
			term_overwrite(el, nse, (nls - nse));
		} else {
			ELRE_DEBUG(1, (__F,
			    "but with nothing left to save\r\n"));
			term_overwrite(el, nse, (nls - nse));
			ELRE_DEBUG(1, (__F,
			    "cleareol %d\n", (oe - old) - (ne - new)));
			if ((oe - old) - (ne - new) != 0)
				term_clear_EOL(el, (oe - old) - (ne - new));
		}
	}
	/*
         * if we have a first insert AND WE HAVEN'T ALREADY DONE IT...
         */
	if ((nsb != nfd) && (osb - ofd) <= (nsb - nfd) && (fx == 0)) {
		ELRE_DEBUG(1, (__F, "late first diff insert at %d...\r\n",
		    nfd - new));

		term_move_to_char(el, nfd - new);
		/*
		 * Check if we have stuff to keep at the end
		 */
		if (nsb != ne) {
			ELRE_DEBUG(1, (__F, "with stuff to keep at end\r\n"));
			/*
		         * We have to recalculate fx here because we set it
		         * to zero above as a flag saying that we hadn't done
		         * an early first insert.
		         */
			fx = (nsb - nfd) - (osb - ofd);
			if (fx > 0) {
				/*
				 * insert fx chars of new starting at nfd
				 */
				ELRE_DEBUG(!EL_CAN_INSERT, (__F,
				 "ERROR: cannot insert in late first diff\n"));
				term_insertwrite(el, nfd, fx);
				re_insert(el, old, ofd - old,
				    el->el_term.t_size.h, nfd, fx);
			}
			/*
		         * write (nsb-nfd) - fx chars of new starting at
		         * (nfd + fx)
			 */
			term_overwrite(el, nfd + fx, (nsb - nfd) - fx);
			re__strncopy(ofd + fx, nfd + fx,
			    (size_t) ((nsb - nfd) - fx));
		} else {
			ELRE_DEBUG(1, (__F, "without anything to save\r\n"));
			term_overwrite(el, nfd, (nsb - nfd));
			re__strncopy(ofd, nfd, (size_t) (nsb - nfd));
		}
	}
	/*
         * line is now NEW up to nse
         */
	if (sx >= 0) {
		ELRE_DEBUG(1, (__F,
		    "second diff insert at %d...\r\n", nse - new));
		term_move_to_char(el, nse - new);
		if (ols != oe) {
			ELRE_DEBUG(1, (__F, "with stuff to keep at end\r\n"));
			if (sx > 0) {
				/* insert sx chars of new starting at nse */
				ELRE_DEBUG(!EL_CAN_INSERT, (__F,
				    "ERROR: cannot insert in second diff\n"));
				term_insertwrite(el, nse, sx);
			}
			/*
		         * write (nls-nse) - sx chars of new starting at
			 * (nse + sx)
		         */
			term_overwrite(el, nse + sx, (nls - nse) - sx);
		} else {
			ELRE_DEBUG(1, (__F, "without anything to save\r\n"));
			term_overwrite(el, nse, (nls - nse));

			/*
	                 * No need to do a clear-to-end here because we were
	                 * doing a second insert, so we will have over
	                 * written all of the old string.
		         */
		}
	}
	ELRE_DEBUG(1, (__F, "done.\r\n"));
}


/* re__copy_and_pad():
 *	Copy string and pad with spaces
 */
private void
re__copy_and_pad(char *dst, const char *src, size_t width)
{
	size_t i;

	for (i = 0; i < width; i++) {
		if (*src == '\0')
			break;
		*dst++ = *src++;
	}

	for (; i < width; i++)
		*dst++ = ' ';

	*dst = '\0';
}


/* re_refresh_cursor():
 *	Move to the new cursor position
 */
protected void
re_refresh_cursor(EditLine *el)
{
	char *cp, c;
	int h, v, th;

	if (el->el_line.cursor >= el->el_line.lastchar) {
		if (el->el_map.current == el->el_map.alt
		    && el->el_line.lastchar != el->el_line.buffer)
			el->el_line.cursor = el->el_line.lastchar - 1;
		else
			el->el_line.cursor = el->el_line.lastchar;
	}

	/* first we must find where the cursor is... */
	h = el->el_prompt.p_pos.h;
	v = el->el_prompt.p_pos.v;
	th = el->el_term.t_size.h;	/* optimize for speed */

	/* do input buffer to el->el_line.cursor */
	for (cp = el->el_line.buffer; cp < el->el_line.cursor; cp++) {
		c = *cp;
		h++;		/* all chars at least this long */

		if (c == '\n') {/* handle newline in data part too */
			h = 0;
			v++;
		} else {
			if (c == '\t') {	/* if a tab, to next tab stop */
				while (h & 07) {
					h++;
				}
			} else if (iscntrl((unsigned char) c)) {
						/* if control char */
				h++;
				if (h > th) {	/* if overflow, compensate */
					h = 1;
					v++;
				}
			} else if (!isprint((unsigned char) c)) {
				h += 3;
				if (h > th) {	/* if overflow, compensate */
					h = h - th;
					v++;
				}
			}
		}

		if (h >= th) {	/* check, extra long tabs picked up here also */
			h = 0;
			v++;
		}
	}

	/* now go there */
	term_move_to_line(el, v);
	term_move_to_char(el, h);
	term__flush();
}


/* re_fastputc():
 *	Add a character fast.
 */
private void
re_fastputc(EditLine *el, int c)
{

	term__putc(c);
	el->el_display[el->el_cursor.v][el->el_cursor.h++] = c;
	if (el->el_cursor.h >= el->el_term.t_size.h) {
		/* if we must overflow */
		el->el_cursor.h = 0;

		/*
		 * If we would overflow (input is longer than terminal size),
		 * emulate scroll by dropping first line and shuffling the rest.
		 * We do this via pointer shuffling - it's safe in this case
		 * and we avoid memcpy().
		 */
		if (el->el_cursor.v + 1 >= el->el_term.t_size.v) {
			int i, lins = el->el_term.t_size.v;
			char *firstline = el->el_display[0];
	
			for(i=1; i < lins; i++)
				el->el_display[i-1] = el->el_display[i];

			re__copy_and_pad(firstline, "", 0);
			el->el_display[i-1] = firstline;
		} else {
			el->el_cursor.v++;
			el->el_refresh.r_oldcv++;
		}
		if (EL_HAS_AUTO_MARGINS) {
			if (EL_HAS_MAGIC_MARGINS) {
				term__putc(' ');
				term__putc('\b');
			}
		} else {
			term__putc('\r');
			term__putc('\n');
		}
	}
}


/* re_fastaddc():
 *	we added just one char, handle it fast.
 *	Assumes that screen cursor == real cursor
 */
protected void
re_fastaddc(EditLine *el)
{
	char c;
	int rhdiff;

	c = el->el_line.cursor[-1];

	if (c == '\t' || el->el_line.cursor != el->el_line.lastchar) {
		re_refresh(el);	/* too hard to handle */
		return;
	}
	rhdiff = el->el_term.t_size.h - el->el_cursor.h -
	    el->el_rprompt.p_pos.h;
	if (el->el_rprompt.p_pos.h && rhdiff < 3) {
		re_refresh(el);	/* clear out rprompt if less than 1 char gap */
		return;
	}			/* else (only do at end of line, no TAB) */
	if (iscntrl((unsigned char) c)) {	/* if control char, do caret */
		char mc = (c == '\177') ? '?' : (c | 0100);
		re_fastputc(el, '^');
		re_fastputc(el, mc);
	} else if (isprint((unsigned char) c)) {	/* normal char */
		re_fastputc(el, c);
	} else {
		re_fastputc(el, '\\');
		re_fastputc(el, (int)(((((unsigned int)c) >> 6) & 3) + '0'));
		re_fastputc(el, (int)(((((unsigned int)c) >> 3) & 7) + '0'));
		re_fastputc(el, (c & 7) + '0');
	}
	term__flush();
}


/* re_clear_display():
 *	clear the screen buffers so that new new prompt starts fresh.
 */
protected void
re_clear_display(EditLine *el)
{
	int i;

	el->el_cursor.v = 0;
	el->el_cursor.h = 0;
	for (i = 0; i < el->el_term.t_size.v; i++)
		el->el_display[i][0] = '\0';
	el->el_refresh.r_oldcv = 0;
}


/* re_clear_lines():
 *	Make sure all lines are *really* blank
 */
protected void
re_clear_lines(EditLine *el)
{

	if (EL_CAN_CEOL) {
		int i;
		term_move_to_char(el, 0);
		for (i = 0; i <= el->el_refresh.r_oldcv; i++) {
			/* for each line on the screen */
			term_move_to_line(el, i);
			term_clear_EOL(el, el->el_term.t_size.h);
		}
		term_move_to_line(el, 0);
	} else {
		term_move_to_line(el, el->el_refresh.r_oldcv);
					/* go to last line */
		term__putc('\r');	/* go to BOL */
		term__putc('\n');	/* go to new line */
	}
}
