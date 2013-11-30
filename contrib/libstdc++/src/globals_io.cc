// Copyright (C) 2001, 2002, 2003, 2004 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
// USA.

// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.

#include "bits/c++config.h"
#include "bits/gthr.h"
#include <fstream>
#include <istream>
#include <ostream>
#include <ext/stdio_filebuf.h>
#include <ext/stdio_sync_filebuf.h>

// On AIX, and perhaps other systems, library initialization order is
// not guaranteed.  For example, the static initializers for the main
// program might run before the static initializers for this library.
// That means that we cannot rely on static initialization in the
// library; there is no guarantee that things will get initialized in
// time.  This file contains definitions of all global variables that
// require initialization as arrays of characters.

// NB: asm directives can rename these non-exported, namespace
// __gnu_cxx symbols into exported, namespace std symbols with the
// appropriate symbol version name.
// The rename syntax is 
//   asm (".symver currentname,oldname@@GLIBCXX_3.2")
// In macro form:
// _GLIBCXX_ASM_SYMVER(currentname, oldname, GLIBCXX_3.2)

namespace std
{
  // Standard stream objects.
  // NB: Iff <iostream> is included, these definitions become wonky.
  typedef char fake_istream[sizeof(istream)]
  __attribute__ ((aligned(__alignof__(istream))));
  typedef char fake_ostream[sizeof(ostream)] 
  __attribute__ ((aligned(__alignof__(ostream))));
  fake_istream cin;
  fake_ostream cout;
  fake_ostream cerr;
  fake_ostream clog;

#ifdef _GLIBCXX_USE_WCHAR_T
  typedef char fake_wistream[sizeof(wistream)] 
  __attribute__ ((aligned(__alignof__(wistream))));
  typedef char fake_wostream[sizeof(wostream)] 
  __attribute__ ((aligned(__alignof__(wostream))));
  fake_wistream wcin;
  fake_wostream wcout;
  fake_wostream wcerr;
  fake_wostream wclog;
#endif
} // namespace std

namespace __gnu_internal
{
  using namespace std;
  using namespace __gnu_cxx;

  // We use different stream buffer types depending on whether
  // ios_base::sync_with_stdio(false) has been called.
  typedef char fake_stdiobuf[sizeof(stdio_sync_filebuf<char>)]
  __attribute__ ((aligned(__alignof__(stdio_sync_filebuf<char>))));
  fake_stdiobuf buf_cout_sync;
  fake_stdiobuf buf_cin_sync;
  fake_stdiobuf buf_cerr_sync;

  typedef char fake_filebuf[sizeof(stdio_filebuf<char>)]
  __attribute__ ((aligned(__alignof__(stdio_filebuf<char>))));
  fake_filebuf buf_cout;
  fake_filebuf buf_cin;
  fake_filebuf buf_cerr;

#ifdef _GLIBCXX_USE_WCHAR_T
  typedef char fake_wstdiobuf[sizeof(stdio_sync_filebuf<wchar_t>)]
  __attribute__ ((aligned(__alignof__(stdio_sync_filebuf<wchar_t>))));
  fake_wstdiobuf buf_wcout_sync;
  fake_wstdiobuf buf_wcin_sync;
  fake_wstdiobuf buf_wcerr_sync;

  typedef char fake_wfilebuf[sizeof(stdio_filebuf<wchar_t>)]
  __attribute__ ((aligned(__alignof__(stdio_filebuf<wchar_t>))));
  fake_wfilebuf buf_wcout;
  fake_wfilebuf buf_wcin;
  fake_wfilebuf buf_wcerr;
#endif

  // Globals for once-only runtime initialization of mutex objects.  This
  // allows static initialization of these objects on systems that need a
  // function call to initialize a mutex.  For example, see stl_threads.h.
#ifdef __GTHREAD_MUTEX_INIT
#elif defined(__GTHREAD_MUTEX_INIT_FUNCTION)
  __gthread_once_t _GLIBCXX_once = __GTHREAD_ONCE_INIT;
  __gthread_mutex_t _GLIBCXX_mutex;
  __gthread_mutex_t *_GLIBCXX_mutex_address;
  
  // Once-only initializer function for _GLIBCXX_mutex.  
  void
  _GLIBCXX_mutex_init ()
  { __GTHREAD_MUTEX_INIT_FUNCTION (&_GLIBCXX_mutex); }

  // Once-only initializer function for _GLIBCXX_mutex_address.  
  void
  _GLIBCXX_mutex_address_init ()
  { __GTHREAD_MUTEX_INIT_FUNCTION (_GLIBCXX_mutex_address); }
#endif
} // namespace __gnu_internal
