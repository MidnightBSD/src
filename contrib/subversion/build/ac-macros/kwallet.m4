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
dnl  SVN_LIB_KWALLET
dnl
dnl  Check configure options and assign variables related to KWallet support
dnl

AC_DEFUN(SVN_LIB_KWALLET,
[
  AC_ARG_WITH(kwallet,
    [AS_HELP_STRING([[--with-kwallet[=PATH]]],
                    [Enable use of KWallet (KDE 4) for auth credentials])],
                    [svn_lib_kwallet="$withval"],
                    [svn_lib_kwallet=no])

  AC_MSG_CHECKING([whether to look for KWallet])
  if test "$svn_lib_kwallet" != "no"; then
    AC_MSG_RESULT([yes])
    if test "$svn_enable_shared" = "yes"; then
      if test "$APR_HAS_DSO" = "yes"; then
        if test -n "$PKG_CONFIG"; then
          if test "$HAVE_DBUS" = "yes"; then
            AC_MSG_CHECKING([for QtCore, QtDBus, QtGui])
            if $PKG_CONFIG --exists QtCore QtDBus QtGui; then
              AC_MSG_RESULT([yes])
              if test "$svn_lib_kwallet" != "yes"; then
                AC_MSG_CHECKING([for kde4-config])
                KDE4_CONFIG="$svn_lib_kwallet/bin/kde4-config"
                if test -f "$KDE4_CONFIG" && test -x "$KDE4_CONFIG"; then
                  AC_MSG_RESULT([yes])
                else
                  KDE4_CONFIG=""
                  AC_MSG_RESULT([no])
                fi
              else
                AC_PATH_PROG(KDE4_CONFIG, kde4-config)
              fi
              if test -n "$KDE4_CONFIG"; then
                AC_MSG_CHECKING([for KWallet])
                old_CXXFLAGS="$CXXFLAGS"
                old_LDFLAGS="$LDFLAGS"
                old_LIBS="$LIBS"
                for d in [`$PKG_CONFIG --cflags QtCore QtDBus QtGui`]; do
                  if test -n ["`echo "$d" | $EGREP -- '^-D[^[:space:]]*'`"]; then
                    CPPFLAGS="$CPPFLAGS $d"
                  fi
                done
                qt_include_dirs="`$PKG_CONFIG --cflags-only-I QtCore QtDBus QtGui`"
                kde_dir="`$KDE4_CONFIG --prefix`"
                SVN_KWALLET_INCLUDES="$DBUS_CPPFLAGS $qt_include_dirs -I$kde_dir/include"
                qt_libs_other_options="`$PKG_CONFIG --libs-only-other QtCore QtDBus QtGui`"
                SVN_KWALLET_LIBS="$DBUS_LIBS -lQtCore -lQtDBus -lQtGui -lkdecore -lkdeui $qt_libs_other_options"
                CXXFLAGS="$CXXFLAGS $SVN_KWALLET_INCLUDES"
                LIBS="$LIBS $SVN_KWALLET_LIBS"
                qt_lib_dirs="`$PKG_CONFIG --libs-only-L QtCore QtDBus QtGui`"
                kde_lib_suffix="`$KDE4_CONFIG --libsuffix`"
                LDFLAGS="$old_LDFLAGS `SVN_REMOVE_STANDARD_LIB_DIRS($qt_lib_dirs -L$kde_dir/lib$kde_lib_suffix)`"
                AC_LANG(C++)
                AC_LINK_IFELSE([AC_LANG_SOURCE([[
#include <kwallet.h>
int main()
{KWallet::Wallet::walletList();}]])], svn_lib_kwallet="yes", svn_lib_kwallet="no")
                AC_LANG(C)
                if test "$svn_lib_kwallet" = "yes"; then
                  AC_MSG_RESULT([yes])
                  CXXFLAGS="$old_CXXFLAGS"
                  LIBS="$old_LIBS"
                else
                  AC_MSG_RESULT([no])
                  AC_MSG_ERROR([cannot find KWallet])
                fi
              else
                AC_MSG_ERROR([cannot find kde4-config])
              fi
            else
              AC_MSG_RESULT([no])
              AC_MSG_ERROR([cannot find QtCore, QtDBus, QtGui])
            fi
          else
            AC_MSG_ERROR([cannot find D-Bus])
          fi
        else
          AC_MSG_ERROR([cannot find pkg-config])
        fi
      else
        AC_MSG_ERROR([APR does not have support for DSOs])
      fi
    else
      AC_MSG_ERROR([--with-kwallet conflicts with --disable-shared])
    fi
  else
    AC_MSG_RESULT([no])
  fi
  AC_SUBST(SVN_KWALLET_INCLUDES)
  AC_SUBST(SVN_KWALLET_LIBS)
])
