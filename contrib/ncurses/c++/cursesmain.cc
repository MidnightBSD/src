// * this is for making emacs happy: -*-Mode: C++;-*-
/****************************************************************************
 * Copyright (c) 1998,2001 Free Software Foundation, Inc.                   *
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
 *   Author: Juergen Pfeifer <juergen.pfeifer@gmx.net> 1997                 *
 ****************************************************************************/

#include "internal.h"
#include "cursesapp.h"

MODULE_ID("$Id: cursesmain.cc,v 1.1.1.1 2006-02-25 02:26:08 laffer1 Exp $")

/* This is the default implementation of main() for a NCursesApplication.
 * You only have to instantiate a static NCursesApplication object in your
 * main application source file and link this module with your application.
 */ 
int main(int argc, char* argv[])
{
  NCursesApplication* A = NCursesApplication::getApplication();
  if (!A)
    return(1);
  else {
    int res;

    A->handleArgs(argc,argv);
    ::endwin();
    res = (*A)();
    ::endwin();
    return(res);
  }
}
