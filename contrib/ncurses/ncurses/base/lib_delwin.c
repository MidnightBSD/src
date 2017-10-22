/****************************************************************************
 * Copyright (c) 1998,2000,2001 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/

/*
**	lib_delwin.c
**
**	The routine delwin().
**
*/

#include <curses.priv.h>

MODULE_ID("$Id: lib_delwin.c,v 1.13 2001/08/26 00:40:20 tom Exp $")

static bool
cannot_delete(WINDOW *win)
{
    WINDOWLIST *p;
    bool result = TRUE;

    for (p = _nc_windows; p != 0; p = p->next) {
	if (&(p->win) == win) {
	    result = FALSE;
	} else if ((p->win._flags & _SUBWIN) != 0
		   && p->win._parent == win) {
	    result = TRUE;
	    break;
	}
    }
    return result;
}

NCURSES_EXPORT(int)
delwin(WINDOW *win)
{
    T((T_CALLED("delwin(%p)"), win));

    if (win == 0
	|| cannot_delete(win))
	returnCode(ERR);

    if (win->_flags & _SUBWIN)
	touchwin(win->_parent);
    else if (curscr != 0)
	touchwin(curscr);

    returnCode(_nc_freewin(win));
}
