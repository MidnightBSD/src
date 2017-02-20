dnl ===================================================================
dnl   Licensed to the Apache Software Foundation (ASF) under one
dnl   or more contributor license agreements.  See the NOTICE file
dnl   distributed with this work for additional information
dnl   regarding copyright ownership.  The ASF licenses this file
dnl   to you under the Apache License, Version 2.0 (the
dnl   "License"); you may not use this file except in compliance
dnl   with the License.  You may obtain a copy of the License at
dnl
dnl     http://www.apache.org/licenses/LICENSE-2.0
dnl
dnl   Unless required by applicable law or agreed to in writing,
dnl   software distributed under the License is distributed on an
dnl   "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
dnl   KIND, either express or implied.  See the License for the
dnl   specific language governing permissions and limitations
dnl   under the License.
dnl ===================================================================
dnl
dnl Macros to find an Apache installation
dnl
dnl This will find an installed Apache.
dnl
dnl Note: If we don't have an installed Apache, then we can't install the
dnl       (dynamic) mod_dav_svn.so module.
dnl

AC_DEFUN(SVN_FIND_APACHE,[
AC_REQUIRE([AC_CANONICAL_HOST])

HTTPD_WANTED_MMN="$1"

AC_MSG_CHECKING(for Apache module support via DSO through APXS)
AC_ARG_WITH(apxs,
            [AS_HELP_STRING([[--with-apxs[=FILE]]],
                            [Build shared Apache modules.  FILE is the optional
                             pathname to the Apache apxs tool; defaults to
                             "apxs".])],
[
    if test "$withval" = "yes"; then
      APXS=apxs
    else
      APXS="$withval"
    fi
    APXS_EXPLICIT=1
])

if test -z "$APXS"; then
  for i in /usr/sbin /usr/local/apache/bin /usr/local/apache2/bin /usr/bin ; do
    if test -f "$i/apxs2"; then
      APXS="$i/apxs2"
      break
    fi
    if test -f "$i/apxs"; then
      APXS="$i/apxs"
      break
    fi
  done
fi

if test -n "$APXS" && test "$APXS" != "no"; then
    APXS_INCLUDE="`$APXS -q INCLUDEDIR`"
    if test -r $APXS_INCLUDE/mod_dav.h; then
        AC_MSG_RESULT(found at $APXS)

        AC_MSG_CHECKING([httpd version])
        AC_EGREP_CPP(VERSION_OKAY,
        [
#include "$APXS_INCLUDE/ap_mmn.h"
#if AP_MODULE_MAGIC_AT_LEAST($HTTPD_WANTED_MMN,0)
VERSION_OKAY
#endif],
        [AC_MSG_RESULT([recent enough])],
        [AC_MSG_RESULT([apache too old:  mmn must be at least $HTTPD_WANTED_MMN])
         if test "$APXS_EXPLICIT" != ""; then
             AC_MSG_ERROR([Apache APXS build explicitly requested, but apache version is too old])
         fi
         APXS=""
        ])

    elif test "$APXS_EXPLICIT" != ""; then
        AC_MSG_ERROR([no - APXS refers to an old version of Apache
                      Unable to locate $APXS_INCLUDE/mod_dav.h])
    else
        AC_MSG_RESULT(no - Unable to locate $APXS_INCLUDE/mod_dav.h)
        APXS=""
    fi
else
    AC_MSG_RESULT(no)
fi

if test -n "$APXS" && test "$APXS" != "no"; then
  AC_MSG_CHECKING([whether Apache version is compatible with APR version])
  apr_major_version="${apr_version%%.*}"
  case "$apr_major_version" in
    0)
      apache_minor_version_wanted_regex="0"
      ;;
    1)
      apache_minor_version_wanted_regex=["[1-5]"]
      ;;
    2)
      apache_minor_version_wanted_regex=["[3-5]"]
      ;;
    *)
      AC_MSG_ERROR([unknown APR version])
      ;;
  esac
  old_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS $SVN_APR_INCLUDES"
  AC_EGREP_CPP([apache_minor_version= *\"$apache_minor_version_wanted_regex\"],
               [
#include "$APXS_INCLUDE/ap_release.h"
apache_minor_version=AP_SERVER_MINORVERSION],
               [AC_MSG_RESULT([yes])],
               [AC_MSG_RESULT([no])
                AC_MSG_ERROR([Apache version incompatible with APR version])])
  CPPFLAGS="$old_CPPFLAGS"
fi

# check for some busted versions of mod_dav
# in particular 2.2.25, 2.4.5, and 2.4.6 had the following bugs which are
# troublesome for Subversion:
# PR 55304: https://issues.apache.org/bugzilla/show_bug.cgi?id=55304
# PR 55306: https://issues.apache.org/bugzilla/show_bug.cgi?id=55306
# PR 55397: https://issues.apache.org/bugzilla/show_bug.cgi?id=55397
if test -n "$APXS" && test "$APXS" != "no"; then
  AC_MSG_CHECKING([mod_dav version])
  old_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS $SVN_APR_INCLUDES"
  blacklisted_versions_regex=["\"2\" \"\.\" (\"2\" \"\.\" \"25\"|\"4\" \"\.\" \"[56]\")"]
  AC_EGREP_CPP([apache_version= *$blacklisted_versions_regex],
               [
#include "$APXS_INCLUDE/ap_release.h"
apache_version=AP_SERVER_BASEREVISION],
               [AC_MSG_RESULT([broken])
                AC_MSG_ERROR([Apache httpd version includes a broken mod_dav; use a newer version of httpd])],
               [AC_MSG_RESULT([acceptable])])
  CPPFLAGS="$old_CPPFLAGS"
fi

AC_ARG_WITH(apache-libexecdir,
            [AS_HELP_STRING([[--with-apache-libexecdir[=PATH]]],
                            [Install Apache modules to Apache's configured
                             modules directory instead of LIBEXECDIR;
                             if PATH is given, install to PATH.])],
[APACHE_LIBEXECDIR="$withval"],[APACHE_LIBEXECDIR='no'])

INSTALL_APACHE_MODS=false
if test -n "$APXS" && test "$APXS" != "no"; then
    APXS_CC="`$APXS -q CC`"
    APACHE_INCLUDES="$APACHE_INCLUDES -I$APXS_INCLUDE"

    if test "$APACHE_LIBEXECDIR" = 'no'; then
        APACHE_LIBEXECDIR="$libexecdir"
    elif test "$APACHE_LIBEXECDIR" = 'yes'; then
        APACHE_LIBEXECDIR="`$APXS -q libexecdir`"
    fi

    BUILD_APACHE_RULE=apache-mod
    INSTALL_APACHE_RULE=install-mods-shared
    INSTALL_APACHE_MODS=true

    case $host in
      *-*-cygwin*)
        APACHE_LDFLAGS="-shrext .so"
        ;;
    esac
elif test x"$APXS" != x"no"; then
    echo "=================================================================="
    echo "WARNING: skipping the build of mod_dav_svn"
    echo "         try using --with-apxs"
    echo "=================================================================="
fi

AC_SUBST(APXS)
AC_SUBST(APACHE_LDFLAGS)
AC_SUBST(APACHE_INCLUDES)
AC_SUBST(APACHE_LIBEXECDIR)
AC_SUBST(INSTALL_APACHE_MODS)

# there aren't any flags that interest us ...
#if test -n "$APXS" && test "$APXS" != "no"; then
#  CFLAGS="$CFLAGS `$APXS -q CFLAGS CFLAGS_SHLIB`"
#fi

if test -n "$APXS_CC" && test "$APXS_CC" != "$CC" ; then
  echo "=================================================================="
  echo "WARNING: You have chosen to compile Subversion with a different"
  echo "         compiler than the one used to compile Apache."
  echo ""
  echo "    Current compiler:      $CC"
  echo "   Apache's compiler:      $APXS_CC"
  echo ""
  echo "This could cause some problems."
  echo "=================================================================="
fi

])
