// * this is for making emacs happy: -*-Mode: C++;-*-
/****************************************************************************
 * Copyright (c) 1998,1999,2001 Free Software Foundation, Inc.              *
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

MODULE_ID("$Id: cursesapp.cc,v 1.1.1.1 2006-02-25 02:26:08 laffer1 Exp $")

void
NCursesApplication::init(bool bColors) {
  if (bColors)
    NCursesWindow::useColors();
  
  if (Root_Window->colors() > 1) {    
    b_Colors = TRUE;
    Root_Window->setcolor(1);
    Root_Window->setpalette(COLOR_YELLOW,COLOR_BLUE);
    Root_Window->setcolor(2);
    Root_Window->setpalette(COLOR_CYAN,COLOR_BLUE);
    Root_Window->setcolor(3);
    Root_Window->setpalette(COLOR_BLACK,COLOR_BLUE);
    Root_Window->setcolor(4);
    Root_Window->setpalette(COLOR_BLACK,COLOR_CYAN);
    Root_Window->setcolor(5);
    Root_Window->setpalette(COLOR_BLUE,COLOR_YELLOW);
    Root_Window->setcolor(6);
    Root_Window->setpalette(COLOR_BLACK,COLOR_GREEN);
  }
  else
    b_Colors = FALSE;

  Root_Window->bkgd(' '|window_backgrounds());
}

NCursesApplication* NCursesApplication::theApp = 0;
NCursesWindow* NCursesApplication::titleWindow = 0;
NCursesApplication::SLK_Link* NCursesApplication::slk_stack = 0;

NCursesApplication::~NCursesApplication() {
  Soft_Label_Key_Set* S;

  delete titleWindow;
  while( (S=top()) ) {
    pop();
    delete S;
  }
  delete Root_Window;
  ::endwin();
}

int NCursesApplication::rinit(NCursesWindow& w) {
  titleWindow = &w;
  return OK;
}

void NCursesApplication::push(Soft_Label_Key_Set& S) {
  SLK_Link* L = new SLK_Link;
  assert(L != 0);
  L->prev = slk_stack;
  L->SLKs = &S;
  slk_stack = L;
  if (Root_Window)
    S.show();
}

bool NCursesApplication::pop() {
  if (slk_stack) {
    SLK_Link* L = slk_stack;
    slk_stack = slk_stack->prev;
    delete L;
    if (Root_Window && top())
      top()->show();
  }
  return (slk_stack ? FALSE : TRUE);
}

Soft_Label_Key_Set* NCursesApplication::top() const {
  if (slk_stack)
    return slk_stack->SLKs;
  else
    return (Soft_Label_Key_Set*)0;
}

int NCursesApplication::operator()(void) {
  bool bColors = b_Colors;
  Soft_Label_Key_Set* S;

  int ts = titlesize();
  if (ts>0)
    NCursesWindow::ripoffline(ts,rinit);
  Soft_Label_Key_Set::Label_Layout fmt = useSLKs();
  if (fmt!=Soft_Label_Key_Set::None) {    
    S = new Soft_Label_Key_Set(fmt);
    assert(S != 0);
    init_labels(*S);
  }

  Root_Window = new NCursesWindow(::stdscr);
  init(bColors);

  if (ts>0)
    title();
  if (fmt!=Soft_Label_Key_Set::None) {
    push(*S);
  }

  return run();
}

NCursesApplication::NCursesApplication(bool bColors) {
  b_Colors = bColors;
  if (theApp)
    THROW(new NCursesException("Application object already created."));
  else
    theApp = this;
}
