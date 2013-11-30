dnl
dnl $Id: dlopen.m4,v 1.1.1.2 2006-02-25 02:34:17 laffer1 Exp $
dnl

AC_DEFUN([rk_DLOPEN], [
	AC_FIND_FUNC_NO_LIBS(dlopen, dl)
	AM_CONDITIONAL(HAVE_DLOPEN, test "$ac_cv_funclib_dlopen" != no)
])
