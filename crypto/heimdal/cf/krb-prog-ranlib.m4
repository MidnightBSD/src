dnl $Id: krb-prog-ranlib.m4,v 1.1.1.1 2006-02-25 02:26:29 laffer1 Exp $
dnl
dnl
dnl Also look for EMXOMF for OS/2
dnl

AC_DEFUN([AC_KRB_PROG_RANLIB],
[AC_CHECK_PROGS(RANLIB, ranlib EMXOMF, :)])
