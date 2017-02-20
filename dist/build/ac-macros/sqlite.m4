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
dnl   SVN_LIB_SQLITE(minimum_ver, recommended_ver, url)
dnl
dnl   Search for a suitable version of sqlite.  minimum_ver is a
dnl   version string which is the lowest suitable version we can use.
dnl   recommended_ver is the recommended version of sqlite, which is
dnl   not necessarily the latest version released.  url is the URL of
dnl   the recommended version of sqlite.
dnl
dnl   If a --with-sqlite=PREFIX option is passed, look for a suitable sqlite
dnl   either installed under the directory PREFIX or as an amalgamation file
dnl   at the path PREFIX.  In this case ignore any sqlite-amalgamation/ subdir
dnl   within the source tree.
dnl
dnl   If no --with-sqlite option is passed, look first for
dnl   sqlite-amalgamation/sqlite3.c which should be the amalgamated version of
dnl   the source distribution.  If the amalgamation exists and is the wrong
dnl   version, exit with a failure.  If no sqlite-amalgamation/ subdir is
dnl   present, search for a sqlite installed on the system.
dnl
dnl   If the search for sqlite fails, set svn_lib_sqlite to no, otherwise set
dnl   it to yes.

AC_DEFUN(SVN_LIB_SQLITE,
[
  SQLITE_MINIMUM_VER="$1"
  SQLITE_RECOMMENDED_VER="$2"
  SQLITE_URL="$3"
  SQLITE_PKGNAME="sqlite3"

  SVN_SQLITE_MIN_VERNUM_PARSE

  AC_MSG_NOTICE([checking sqlite library])

  AC_ARG_WITH(sqlite,
              AS_HELP_STRING([--with-sqlite=PREFIX],
                          [Use installed SQLite library or amalgamation file.]),
  [
    if test "$withval" = "yes" ; then
      AC_MSG_ERROR([--with-sqlite requires an argument.])
    else
      sqlite_dir="$withval"
    fi

    if test -d $sqlite_dir; then
      dnl pointed at an sqlite installation
      SVN_SQLITE_DIR_CONFIG($sqlite_dir)
    else
      dnl pointed at the amalgamation file
      SVN_SQLITE_FILE_CONFIG($sqlite_dir)
    fi

    if test -z "$svn_lib_sqlite"; then
      AC_MSG_WARN([no suitable sqlite found in $sqlite_dir])
      SVN_DOWNLOAD_SQLITE
    fi
  ],
  [
    dnl see if the sqlite amalgamation exists in the source tree
    SVN_SQLITE_FILE_CONFIG($abs_srcdir/sqlite-amalgamation/sqlite3.c)

    if test -z "$svn_lib_sqlite"; then
      SVN_SQLITE_FILE_CONFIG($abs_builddir/sqlite-amalgamation/sqlite3.c)
    fi

    if test -z "$svn_lib_sqlite"; then
      dnl check the "standard" location of /usr
      SVN_SQLITE_DIR_CONFIG()
    fi

    if test -z "$svn_lib_sqlite"; then
      dnl no --with-sqlite switch, and no sqlite subdir, look in PATH
      SVN_SQLITE_PKG_CONFIG
    fi

    if test -z "$svn_lib_sqlite"; then
      SVN_DOWNLOAD_SQLITE
    fi
  ])

  AC_SUBST(SVN_SQLITE_INCLUDES)
  AC_SUBST(SVN_SQLITE_LIBS)
])

dnl SVN_SQLITE_PKG_CONFIG
dnl
dnl Look for sqlite in PATH using pkg-config.
AC_DEFUN(SVN_SQLITE_PKG_CONFIG,
[
    if test -n "$PKG_CONFIG"; then
      AC_MSG_CHECKING([sqlite library version (via pkg-config)])
      sqlite_version=`$PKG_CONFIG $SQLITE_PKGNAME --modversion --silence-errors`

      if test -n "$sqlite_version"; then
        SVN_SQLITE_VERNUM_PARSE([$sqlite_version], [sqlite_ver_num])

        if test "$sqlite_ver_num" -ge "$sqlite_min_ver_num"; then
          AC_MSG_RESULT([$sqlite_version])
          svn_lib_sqlite="yes"
          SVN_SQLITE_INCLUDES="`$PKG_CONFIG $SQLITE_PKGNAME --cflags`"
          SVN_SQLITE_LIBS="`$PKG_CONFIG $SQLITE_PKGNAME --libs`"
        else
          AC_MSG_RESULT([none or unsupported $sqlite_version])
        fi
      fi
    fi

    if test -z "$svn_lib_sqlite"; then
      AC_MSG_RESULT(no)
    fi
])

dnl SVN_SQLITE_DIR_CONFIG(sqlite_dir)
dnl
dnl Check to see if we've got an appropriate sqlite library at sqlite_dir.
dnl If we don't, fail.
AC_DEFUN(SVN_SQLITE_DIR_CONFIG,
[
  if test -z "$1"; then
    sqlite_dir=""
    sqlite_include="sqlite3.h"
  else
    sqlite_dir="$1"
    sqlite_include="$1/include/sqlite3.h"
  fi

  save_CPPFLAGS="$CPPFLAGS"
  save_LDFLAGS="$LDFLAGS"

  if test ! -z "$1"; then
    CPPFLAGS="$CPPFLAGS -I$sqlite_dir/include"
    LDFLAGS="$LDFLAGS -L$sqlite_dir/lib"
  fi

  AC_CHECK_HEADER(sqlite3.h,
    [
      AC_MSG_CHECKING([sqlite library version (via header)])
      AC_EGREP_CPP(SQLITE_VERSION_OKAY,[
#include "$sqlite_include"
#if SQLITE_VERSION_NUMBER >= $sqlite_min_ver_num
SQLITE_VERSION_OKAY
#endif],
                  [AC_MSG_RESULT([okay])
                   AC_CHECK_LIB(sqlite3, sqlite3_close, [
                      svn_lib_sqlite="yes"
                      if test -z "$sqlite_dir" -o ! -d "$sqlite_dir"; then
                        SVN_SQLITE_LIBS="-lsqlite3"
                      else
                        SVN_SQLITE_INCLUDES="-I$sqlite_dir/include"
                        SVN_SQLITE_LIBS="`SVN_REMOVE_STANDARD_LIB_DIRS(-L$sqlite_dir/lib -lsqlite3)`"
                      fi
                  ])], [AC_MSG_RESULT([unsupported SQLite version])])
    ])

  CPPFLAGS="$save_CPPFLAGS"
  LDFLAGS="$save_LDFLAGS"
])

dnl SVN_SQLITE_FILE_CONFIG(sqlite_file)
dnl
dnl Check to see if we've got an appropriate sqlite amalgamation file
dnl at sqlite_file.  If not, fail.
AC_DEFUN(SVN_SQLITE_FILE_CONFIG,
[
  sqlite_amalg="$1"
  AC_MSG_CHECKING([sqlite amalgamation])
  if test ! -e $sqlite_amalg; then
    AC_MSG_RESULT([no])
  else
    AC_MSG_RESULT([yes])
    AC_MSG_CHECKING([sqlite amalgamation file version])
    AC_EGREP_CPP(SQLITE_VERSION_OKAY,[
#include "$sqlite_amalg"
#if SQLITE_VERSION_NUMBER >= $sqlite_min_ver_num
SQLITE_VERSION_OKAY
#endif],
                 [AC_MSG_RESULT([amalgamation found and is okay])
                  _SVN_SQLITE_DSO_LIBS
                  AC_DEFINE(SVN_SQLITE_INLINE, 1,
                  [Defined if svn should use the amalgamated version of sqlite])
                  SVN_SQLITE_INCLUDES="-I`dirname $sqlite_amalg`"
                  if test -n "$svn_sqlite_dso_ldflags"; then
                    SVN_SQLITE_LIBS="$svn_sqlite_dso_ldflags -lpthread"
                  else
                    SVN_SQLITE_LIBS="-lpthread"
                  fi
                  svn_lib_sqlite="yes"],
                 [AC_MSG_RESULT([unsupported amalgamation SQLite version])])
  fi
])

dnl SVN_SQLITE_VERNUM_PARSE(version_string, result_var)
dnl
dnl Parse a x.y[.z] version string version_string into a number result_var.
AC_DEFUN(SVN_SQLITE_VERNUM_PARSE,
[
  version_string="$1"
  
  major=`expr $version_string : '\([[0-9]]*\)'`
  minor=`expr $version_string : '[[0-9]]*\.\([[0-9]]*\)'`
  micro=`expr $version_string : '[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)'`
  if test -z "$micro"; then
    micro=0
  fi
  $2=`expr $major \* 1000000 \
        \+ $minor \* 1000 \
        \+ $micro`
])

dnl SVN_SQLITE_MIN_VERNUM_PARSE()
dnl
dnl Parse a x.y.z version string SQLITE_MINIMUM_VER into a number
dnl sqlite_min_ver_num.
AC_DEFUN(SVN_SQLITE_MIN_VERNUM_PARSE,
[
  SVN_SQLITE_VERNUM_PARSE([$SQLITE_MINIMUM_VER], [sqlite_min_ver_num])
])

dnl SVN_DOWNLOAD_SQLITE()
dnl no sqlite found, print out a message telling the user what to do
AC_DEFUN(SVN_DOWNLOAD_SQLITE,
[
  echo ""
  echo "An appropriate version of sqlite could not be found.  We recommmend"
  echo "${SQLITE_RECOMMENDED_VER}, but require at least ${SQLITE_MINIMUM_VER}."
  echo "Please either install a newer sqlite on this system"
  echo ""
  echo "or"
  echo ""
  echo "get the sqlite ${SQLITE_RECOMMENDED_VER} amalgamation from:"
  echo "    ${SQLITE_URL}"
  echo "unpack the archive using unzip and rename the resulting"
  echo "directory to:"
  echo "$abs_srcdir/sqlite-amalgamation"
  if test x"$abs_srcdir" != x"$abs_builddir"; then
    echo "or to:"
    echo "$abs_builddir/sqlite-amalgamation"
  fi
  echo ""
  AC_MSG_ERROR([Subversion requires SQLite])
])

dnl _SVN_SQLITE_DSO_LIBS() dnl Find additional libraries that the
dnl sqlite amalgamation code should link in order to load
dnl shared libraries.  Copied from build/libtool.m4
AC_DEFUN(_SVN_SQLITE_DSO_LIBS,
[
  case $host_os in
  beos* | mingw* | pw32* | cegcc* | cygwin*)
    svn_sqlite_dso_ldflags=
    ;;

  darwin*)
  # if libdl is installed we need to link against it
    AC_CHECK_LIB([dl], [dlopen],
                [lt_cv_dlopen="dlopen" svn_sqlite_dso_ldflags="-ldl"],[
    svn_sqlite_dso_ldflags=
    ])
    ;;

  *)
    AC_CHECK_FUNC([shl_load],
          [svn_sqlite_dso_ldflags=],
      [AC_CHECK_LIB([dld], [shl_load],
            [svn_sqlite_dso_ldflags="-ldld"],
        [AC_CHECK_FUNC([dlopen],
              [svn_sqlite_dso_ldflags=],
          [AC_CHECK_LIB([dl], [dlopen],
                [svn_sqlite_dso_ldflags="-ldl"],
            [AC_CHECK_LIB([svld], [dlopen],
                  [svn_sqlite_dso_ldflags="-lsvld"],
              [AC_CHECK_LIB([dld], [dld_link],
                    [svn_sqlite_dso_ldflags="-ldld"])
              ])
            ])
          ])
        ])
      ])
    ;;
  esac

  AC_MSG_CHECKING([additional libraries for sqlite])
  if test -n "$svn_sqlite_dso_ldflags"; then
    AC_MSG_RESULT(${svn_sqlite_dso_ldflags})
  else
    AC_MSG_RESULT(none)
  fi
])
