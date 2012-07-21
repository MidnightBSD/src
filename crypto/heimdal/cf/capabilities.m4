dnl
dnl $Id: capabilities.m4,v 1.1.1.3 2012-07-21 15:09:06 laffer1 Exp $
dnl

dnl
dnl Test SGI capabilities
dnl

AC_DEFUN([KRB_CAPABILITIES],[

AC_CHECK_HEADERS(capability.h sys/capability.h)

AC_CHECK_FUNCS(sgi_getcapabilitybyname cap_set_proc)
])
