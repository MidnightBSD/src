dnl
dnl $Id: dlopen.m4,v 1.1.1.3 2012-07-21 15:09:06 laffer1 Exp $
dnl

AC_DEFUN([rk_DLOPEN], [
	AC_FIND_FUNC_NO_LIBS(dlopen, dl,[
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif],[0,0])
	AM_CONDITIONAL(HAVE_DLOPEN, test "$ac_cv_funclib_dlopen" != no)
])
