/****************************************************************************
 * Copyright (c) 1998-2002,2005 Free Software Foundation, Inc.              *
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
 *     and: Thomas E. Dickey                        1996-on                 *
 ****************************************************************************/

/*
 *	lib_tracemse.c - Tracing/Debugging routines (mouse events)
 */

#include <curses.priv.h>

MODULE_ID("$Id: lib_tracemse.c,v 1.12 2005/06/11 19:53:50 tom Exp $")

#ifdef TRACE

NCURSES_EXPORT(char *)
_tracemouse(MEVENT const *ep)
{
    /*
     * hmm - format is no longer than 80 columns, there are 5 numbers that
     * could at most have 10 digits, and the mask contains no more than 32 bits
     * with each bit representing less than 15 characters.  Usually the whole
     * string is less than 80 columns, but this buffer size is an absolute
     * limit.
     */
    static char buf[80 + (5 * 10) + (32 * 15)];

    (void) sprintf(buf, "id %2d  at (%2d, %2d, %2d) state %4lx = {",
		   ep->id,
		   ep->x,
		   ep->y,
		   ep->z,
		   (unsigned long) ep->bstate);

#define SHOW(m, s) if ((ep->bstate & m) == m) strcat(strcat(buf, s), ", ")

    SHOW(BUTTON1_RELEASED, "release-1");
    SHOW(BUTTON1_PRESSED, "press-1");
    SHOW(BUTTON1_CLICKED, "click-1");
    SHOW(BUTTON1_DOUBLE_CLICKED, "doubleclick-1");
    SHOW(BUTTON1_TRIPLE_CLICKED, "tripleclick-1");
#if NCURSES_MOUSE_VERSION == 1
    SHOW(BUTTON1_RESERVED_EVENT, "reserved-1");
#endif

    SHOW(BUTTON2_RELEASED, "release-2");
    SHOW(BUTTON2_PRESSED, "press-2");
    SHOW(BUTTON2_CLICKED, "click-2");
    SHOW(BUTTON2_DOUBLE_CLICKED, "doubleclick-2");
    SHOW(BUTTON2_TRIPLE_CLICKED, "tripleclick-2");
#if NCURSES_MOUSE_VERSION == 1
    SHOW(BUTTON2_RESERVED_EVENT, "reserved-2");
#endif

    SHOW(BUTTON3_RELEASED, "release-3");
    SHOW(BUTTON3_PRESSED, "press-3");
    SHOW(BUTTON3_CLICKED, "click-3");
    SHOW(BUTTON3_DOUBLE_CLICKED, "doubleclick-3");
    SHOW(BUTTON3_TRIPLE_CLICKED, "tripleclick-3");
#if NCURSES_MOUSE_VERSION == 1
    SHOW(BUTTON3_RESERVED_EVENT, "reserved-3");
#endif

    SHOW(BUTTON4_RELEASED, "release-4");
    SHOW(BUTTON4_PRESSED, "press-4");
    SHOW(BUTTON4_CLICKED, "click-4");
    SHOW(BUTTON4_DOUBLE_CLICKED, "doubleclick-4");
    SHOW(BUTTON4_TRIPLE_CLICKED, "tripleclick-4");
#if NCURSES_MOUSE_VERSION == 1
    SHOW(BUTTON4_RESERVED_EVENT, "reserved-4");
#endif

#if NCURSES_MOUSE_VERSION == 2
    SHOW(BUTTON5_RELEASED, "release-5");
    SHOW(BUTTON5_PRESSED, "press-5");
    SHOW(BUTTON5_CLICKED, "click-5");
    SHOW(BUTTON5_DOUBLE_CLICKED, "doubleclick-5");
    SHOW(BUTTON5_TRIPLE_CLICKED, "tripleclick-5");
#endif

    SHOW(BUTTON_CTRL, "ctrl");
    SHOW(BUTTON_SHIFT, "shift");
    SHOW(BUTTON_ALT, "alt");
    SHOW(ALL_MOUSE_EVENTS, "all-events");
    SHOW(REPORT_MOUSE_POSITION, "position");

#undef SHOW

    if (buf[strlen(buf) - 1] == ' ')
	buf[strlen(buf) - 2] = '\0';
    (void) strcat(buf, "}");
    return (buf);
}

#else /* !TRACE */
empty_module(_nc_lib_tracemouse)
#endif
