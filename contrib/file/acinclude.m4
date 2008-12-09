dnl cloned from autoconf 2.13 acspecific.m4
AC_DEFUN([AC_C_LONG_LONG],
[AC_CACHE_CHECK(for long long, ac_cv_c_long_long,
[if test "$GCC" = yes; then
  ac_cv_c_long_long=yes
else
AC_TRY_RUN([int main() {
long long foo = 0;
exit(sizeof(long long) < sizeof(long)); }],
ac_cv_c_long_long=yes, ac_cv_c_long_long=no)
fi])
if test $ac_cv_c_long_long = yes; then
  AC_DEFINE(HAVE_LONG_LONG)
fi
])

dnl from autoconf 2.13 acspecific.m4, with changes to check for daylight

AC_DEFUN([AC_STRUCT_TIMEZONE_DAYLIGHT],
[AC_REQUIRE([AC_STRUCT_TM])dnl
AC_CACHE_CHECK([for tm_zone in struct tm], ac_cv_struct_tm_zone,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <$ac_cv_struct_tm>], [struct tm tm; tm.tm_zone;],
  ac_cv_struct_tm_zone=yes, ac_cv_struct_tm_zone=no)])
if test "$ac_cv_struct_tm_zone" = yes; then
  AC_DEFINE(HAVE_TM_ZONE,1,[HAVE_TM_ZONE])
fi
AC_CACHE_CHECK(for tzname, ac_cv_var_tzname,
[AC_TRY_LINK(
changequote(<<, >>)dnl
<<#include <time.h>
#ifndef tzname /* For SGI.  */
extern char *tzname[]; /* RS6000 and others reject char **tzname.  */
#endif>>,
changequote([, ])dnl
[atoi(*tzname);], ac_cv_var_tzname=yes, ac_cv_var_tzname=no)])
  if test $ac_cv_var_tzname = yes; then
    AC_DEFINE(HAVE_TZNAME,1,[HAVE_TZNAME])
  fi

AC_CACHE_CHECK([for tm_isdst in struct tm], ac_cv_struct_tm_isdst,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <$ac_cv_struct_tm>], [struct tm tm; tm.tm_isdst;],
  ac_cv_struct_tm_isdst=yes, ac_cv_struct_tm_isdst=no)])
if test "$ac_cv_struct_tm_isdst" = yes; then
  AC_DEFINE(HAVE_TM_ISDST)
fi
AC_CACHE_CHECK(for daylight, ac_cv_var_daylight,
[AC_TRY_LINK(
changequote(<<, >>)dnl
<<#include <time.h>
#ifndef daylight /* In case IRIX #defines this, too  */
extern int daylight;
#endif>>,
changequote([, ])dnl
[atoi(daylight);], ac_cv_var_daylight=yes, ac_cv_var_daylight=no)])
  if test $ac_cv_var_daylight = yes; then
    AC_DEFINE(HAVE_DAYLIGHT)
  fi
])

dnl from autoconf 2.13 acgeneral.m4, with patch:
dnl Date: Fri, 15 Jan 1999 05:52:41 -0800
dnl Message-ID: <199901151352.FAA18237@shade.twinsun.com>
dnl From: eggert@twinsun.com (Paul Eggert)
dnl Subject: autoconf 2.13 AC_CHECK_TYPE doesn't allow shell vars
dnl Newsgroups: gnu.utils.bug
dnl
dnl now include <stdint.h> if available

dnl AC_CHECK_TYPE2_STDC(TYPE, DEFAULT)
AC_DEFUN([AC_CHECK_TYPE2_STDC],
[AC_REQUIRE([AC_HEADER_STDC])dnl
AC_REQUIRE([AC_HEADER_STDINT])dnl
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL(ac_cv_type_$1,
[AC_EGREP_CPP(dnl
[(^|[^a-zA-Z_0-9])$1[^a-zA-Z_0-9]],
[#if HAVE_STDINT_H
#include <stdint.h>
#endif
#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif], eval "ac_cv_type_$1=yes", eval "ac_cv_type_$1=no")])dnl
if eval "test \"`echo '$ac_cv_type_'$1`\" = yes"; then
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
  AC_DEFINE_UNQUOTED($1, $2)
fi
])

dnl from autoconf 2.13 acgeneral.m4, with additional third argument
dnl AC_CHECK_SIZEOF_INCLUDES(TYPE [, CROSS-SIZE [, INCLUDES]])
AC_DEFUN([AC_CHECK_SIZEOF_INCLUDES],
[dnl The name to #define.
define([AC_TYPE_NAME], translit(sizeof_$1, [[[a-z *]]], [[[A-Z_P]]]))dnl
dnl The cache variable name.
define([AC_CV_NAME], translit(ac_cv_sizeof_$1, [[[ *]]], [[[_p]]]))dnl
AC_MSG_CHECKING(size of $1)
AC_CACHE_VAL(AC_CV_NAME,
[AC_TRY_RUN([$3
#include <stdio.h>
main()
{
  FILE *f=fopen("conftestval", "w");
  if (!f) exit(1);
  fprintf(f, "%d\n", sizeof($1));
  exit(0);
}], AC_CV_NAME=`cat conftestval`, AC_CV_NAME=0, ifelse([$2], , , AC_CV_NAME=$2))])dnl
AC_MSG_RESULT($AC_CV_NAME)
AC_DEFINE_UNQUOTED(AC_TYPE_NAME, $AC_CV_NAME)
undefine([AC_TYPE_NAME])dnl
undefine([AC_CV_NAME])dnl
])

dnl AC_CHECK_SIZEOF_STDC_HEADERS(TYPE [, CROSS_SIZE])
AC_DEFUN([AC_CHECK_SIZEOF_STDC_HEADERS],
[AC_REQUIRE([AC_HEADER_STDC])dnl
AC_REQUIRE([AC_HEADER_STDINT])dnl
AC_CHECK_SIZEOF_INCLUDES($1, $2,
[#if HAVE_STDINT_H
#include <stdint.h>
#endif
#include <sys/types.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif
])
])


dnl AC_CHECK_TYPE_STDC(TYPE, DEFAULT)
AC_DEFUN([AC_CHECK_TYPE_STDC],
[AC_REQUIRE([AC_HEADER_STDC])dnl
AC_REQUIRE([AC_HEADER_STDINT])dnl
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL(ac_cv_type_$1,
[AC_EGREP_CPP(dnl
[(^|[^a-zA-Z_0-9])$1[^a-zA-Z_0-9]],
[#if HAVE_STDINT_H
#include <stdint.h>
#endif
#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif], ac_cv_type_$1=yes, ac_cv_type_$1=no)])dnl
AC_MSG_RESULT($ac_cv_type_$1)
if test $ac_cv_type_$1 = no; then
  AC_DEFINE($1, $2)
fi
])

dnl AC_HEADER_STDINT
AC_DEFUN([AC_HEADER_STDINT], [AC_CHECK_HEADERS(stdint.h)])

dnl By default, many hosts won't let programs access large files;
dnl one must use special compiler options to get large-file access to work.
dnl For more details about this brain damage please see:
dnl http://www.sas.com/standards/large.file/x_open.20Mar96.html

dnl Written by Paul Eggert <eggert@twinsun.com>.

dnl Internal subroutine of AC_SYS_LARGEFILE.
dnl AC_SYS_LARGEFILE_TEST_INCLUDES
AC_DEFUN([AC_SYS_LARGEFILE_TEST_INCLUDES],
  [[#include <sys/types.h>
    /* Check that off_t can represent 2**63 - 1 correctly.
       We can't simply "#define LARGE_OFF_T 9223372036854775807",
       since some C++ compilers masquerading as C compilers
       incorrectly reject 9223372036854775807.  */
#   define LARGE_OFF_T (((off_t) 1 << 62) - 1 + ((off_t) 1 << 62))
    int off_t_is_large[(LARGE_OFF_T % 2147483629 == 721
			&& LARGE_OFF_T % 2147483647 == 1)
		       ? 1 : -1];
  ]])

dnl Internal subroutine of AC_SYS_LARGEFILE.
dnl AC_SYS_LARGEFILE_MACRO_VALUE(C-MACRO, VALUE, CACHE-VAR, COMMENT, INCLU=
DES, FUNCTION-BODY)
AC_DEFUN([AC_SYS_LARGEFILE_MACRO_VALUE],
  [AC_CACHE_CHECK([for $1 value needed for large files], $3,
     [$3=no
      AC_TRY_COMPILE([$5],
	[$6],
	,
	[AC_TRY_COMPILE([#define $1 $2]
[$5]
	   ,
	   [$6],
	   [$3=$2])])])
   if test "[$]$3" != no; then
     AC_DEFINE_UNQUOTED([$1], [$]$3, [$4])
   fi])

AC_DEFUN([AC_SYS_LARGEFILE],
  [AC_REQUIRE([AC_PROG_CC])
   AC_ARG_ENABLE(largefile,
     [  --disable-largefile     omit support for large files])
   if test "$enable_largefile" != no; then

     AC_CACHE_CHECK([for special C compiler options needed for large files=
],
       ac_cv_sys_largefile_CC,
       [ac_cv_sys_largefile_CC=no
        if test "$GCC" != yes; then
	  # IRIX 6.2 and later do not support large files by default,
	  # so use the C compiler's -n32 option if that helps.
	  AC_TRY_COMPILE(AC_SYS_LARGEFILE_TEST_INCLUDES, , ,
	    [ac_save_CC="$CC"
	     CC="$CC -n32"
	     AC_TRY_COMPILE(AC_SYS_LARGEFILE_TEST_INCLUDES, ,
	       ac_cv_sys_largefile_CC=' -n32')
	     CC="$ac_save_CC"])
        fi])
     if test "$ac_cv_sys_largefile_CC" != no; then
       CC="$CC$ac_cv_sys_largefile_CC"
     fi

     AC_SYS_LARGEFILE_MACRO_VALUE(_FILE_OFFSET_BITS, 64,
       ac_cv_sys_file_offset_bits,
       [Number of bits in a file offset, on hosts where this is settable.],
       AC_SYS_LARGEFILE_TEST_INCLUDES)
     AC_SYS_LARGEFILE_MACRO_VALUE(_LARGE_FILES, 1,
       ac_cv_sys_large_files,
       [Define for large files, on AIX-style hosts.],
       AC_SYS_LARGEFILE_TEST_INCLUDES)
   fi
  ])

AC_DEFUN([AC_FUNC_FSEEKO],
  [AC_SYS_LARGEFILE_MACRO_VALUE(_LARGEFILE_SOURCE, 1,
     ac_cv_sys_largefile_source,
     [Define to make fseeko visible on some hosts (e.g. glibc 2.2).],
     [#include <stdio.h>], [return !fseeko;])
   # We used to try defining _XOPEN_SOURCE=500 too, to work around a bug
   # in glibc 2.1.3, but that breaks too many other things.
   # If you want fseeko and ftello with glibc, upgrade to a fixed glibc.

   AC_CACHE_CHECK([for fseeko], ac_cv_func_fseeko,
     [ac_cv_func_fseeko=no
      AC_TRY_LINK([#include <stdio.h>],
        [return fseeko && fseeko (stdin, 0, 0);],
	[ac_cv_func_fseeko=yes])])
   if test $ac_cv_func_fseeko != no; then
     AC_DEFINE(HAVE_FSEEKO, 1,
       [Define if fseeko (and presumably ftello) exists and is declared.])
   fi])

# From Paul Eggert.

# BeOS 5 has <wchar.h> but does not define mbstate_t,
# so you can't declare an object of that type.
# Check for this incompatibility with Standard C.

# Include stdlib.h first, because otherwise this test would fail on Linux
# (at least glibc-2.1.3) because the "_XOPEN_SOURCE 500" definition elicits
# a syntax error in wchar.h due to the use of undefined __int32_t.
AC_DEFUN([AC_MBSTATE_T],
  [
   AC_CHECK_HEADERS(stdlib.h)
  
   AC_CACHE_CHECK([for mbstate_t], ac_cv_type_mbstate_t,
    [AC_TRY_COMPILE([
#if HAVE_STDLIB_H 
# include <stdlib.h>
#endif
#include <wchar.h>],
      [mbstate_t x; return sizeof x;],
      ac_cv_type_mbstate_t=yes,
      ac_cv_type_mbstate_t=no)])
   if test $ac_cv_type_mbstate_t = no; then
     AC_DEFINE(mbstate_t, int,
               [Define to a type if <wchar.h> does not define.])
   fi])


