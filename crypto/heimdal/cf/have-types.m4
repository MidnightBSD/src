dnl
dnl $Id: have-types.m4,v 1.1.1.3 2012-07-21 15:09:06 laffer1 Exp $
dnl

AC_DEFUN([AC_HAVE_TYPES], [
for i in $1; do
        AC_HAVE_TYPE($i)
done
if false;then
	AC_CHECK_FUNCS($1)
fi
])
