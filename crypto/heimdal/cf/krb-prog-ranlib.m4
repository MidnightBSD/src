dnl $Id: krb-prog-ranlib.m4,v 1.1.1.3 2012-07-21 15:09:06 laffer1 Exp $
dnl
dnl
dnl Also look for EMXOMF for OS/2
dnl

AC_DEFUN([AC_KRB_PROG_RANLIB],
[AC_CHECK_PROGS(RANLIB, ranlib EMXOMF, :)])
