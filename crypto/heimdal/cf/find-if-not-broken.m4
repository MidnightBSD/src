dnl $Id: find-if-not-broken.m4,v 1.1.1.3 2012-07-21 15:09:06 laffer1 Exp $
dnl
dnl
dnl Mix between AC_FIND_FUNC and AC_BROKEN
dnl

AC_DEFUN([AC_FIND_IF_NOT_BROKEN],
[AC_FIND_FUNC([$1], [$2], [$3], [$4])
if eval "test \"$ac_cv_func_$1\" != yes"; then 
	rk_LIBOBJ([$1])
fi
])
