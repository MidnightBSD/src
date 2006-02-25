dnl
dnl $Id: capabilities.m4,v 1.1.1.2 2006-02-25 02:34:17 laffer1 Exp $
dnl

dnl
dnl Test SGI capabilities
dnl

AC_DEFUN([KRB_CAPABILITIES],[

AC_CHECK_HEADERS(capability.h sys/capability.h)

AC_CHECK_FUNCS(sgi_getcapabilitybyname cap_set_proc)
])
