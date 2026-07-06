#
# Copyright (c) 2014 Brent Cook
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

AC_DEFUN([CHECK_OS_OPTIONS], [

CFLAGS="$CFLAGS -Wall -std=gnu99 -fno-strict-aliasing"

case $host_os in
	*aix*)
		HOST_OS=aix
		if test "`echo $CC | cut -d ' ' -f 1`" != "gcc" ; then
			CFLAGS="-qnoansialias $USER_CFLAGS"
		fi
		AC_SUBST([PLATFORM_LDADD], ['-lperfstat -lpthread'])
		;;
	*cygwin*)
		HOST_OS=cygwin
		;;
	*darwin*)
		HOST_OS=darwin
		HOST_ABI=macosx
		#
		# Don't use arc4random on systems before 10.12 because of
		# weak seed on failure to open /dev/random, based on latest
		# public source:
		# https://www.opensource.apple.com/source/Libc/Libc-997.90.3/gen/FreeBSD/arc4random.c
		#
		# We use the presence of getentropy() to detect 10.12. The
		# following check takes into account that:
		#
		#   - iOS <= 10.1 fails because of missing getentropy and
		#     hence they miss sys/random.h
		#
		#   - in macOS 10.12 getentropy is not tagged as introduced in
		#     10.12 so we cannot use it for target < 10.12
		#
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <AvailabilityMacros.h>
#include <unistd.h>
#include <sys/random.h>  /* Systems without getentropy() should die here */

/* Based on: https://gitweb.torproject.org/tor.git/commit/?id=16fcbd21 */
#ifndef MAC_OS_X_VERSION_10_12
#  define MAC_OS_X_VERSION_10_12 101200
#endif
#if defined(MAC_OS_X_VERSION_MIN_REQUIRED)
#  if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_12
#    error "Running on Mac OSX 10.11 or earlier"
#  endif
#endif
                       ]], [[
char buf[1]; getentropy(buf, 1);
					   ]])],
                       [ USE_BUILTIN_ARC4RANDOM=no ],
                       [ USE_BUILTIN_ARC4RANDOM=yes ]
		)
		AC_MSG_CHECKING([whether to use builtin arc4random])
		AC_MSG_RESULT([$USE_BUILTIN_ARC4RANDOM])
		AC_DEFINE(SETEUID_BREAKS_SETUID,[],[setuid after seteuid does not work])
		AC_DEFINE(BROKEN_SETREUID,[], [Broken setreuid])
		AC_DEFINE(BROKEN_SETREGID,[], [Broken setregid])
		AC_DEFINE(YYSTYPE_IS_DECLARED,[], [Broken bison])
		AC_DEFINE([SPT_TYPE], [SPT_REUSEARGV])
		AC_SUBST([PROG_LDADD], ['-lresolv'])
		;;
	*freebsd*)
		HOST_OS=freebsd
		HOST_ABI=elf
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/param.h>
#if __FreeBSD_version < 1200000
        undefined
#endif
                       ]], [[]])],
                       [ USE_BUILTIN_ARC4RANDOM=no ],
                       [ USE_BUILTIN_ARC4RANDOM=yes ]
		)
		AC_SUBST([PROG_LDADD], ['-lthr'])
		;;
	*hpux*)
		HOST_OS=hpux;
		if test "`echo $host_os | cut -c 1-4`" = "ia64" ; then
			if test "`echo $CC | cut -d ' ' -f 1`" = "gcc" ; then
				CFLAGS="$CFLAGS -mlp64"
			else
				CFLAGS="+DD64"
			fi
		fi
		if ! test "`echo $CC | cut -d ' ' -f 1`" = "gcc" ; then
			CFLAGS="-g -O2 +Otype_safety=off $CFLAGS $USER_CFLAGS"
		fi
		CPPFLAGS="$CPPFLAGS -D_XOPEN_SOURCE=600 -D__STRICT_ALIGNMENT"
		;;
	*linux*)
		HOST_OS=linux
		HOST_ABI=elf
		CPPFLAGS="$CPPFLAGS -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_POSIX_SOURCE -D_GNU_SOURCE"
		AC_DEFINE([SPT_TYPE], [SPT_REUSEARGV])
		AC_SUBST([PROG_LDADD], ['-lresolv'])
		;;
	*netbsd*)
		HOST_OS=netbsd
		HOST_ABI=elf
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/param.h>
#if __NetBSD_Version__ < 700000001
        undefined
#endif
                       ]], [[]])],
                       [ USE_BUILTIN_ARC4RANDOM=no ],
                       [ USE_BUILTIN_ARC4RANDOM=yes ]
		)
		CPPFLAGS="$CPPFLAGS -D_OPENBSD_SOURCE"
		;;
	*openbsd*)
		HOST_OS=openbsd
		HOST_ABI=elf
		AC_DEFINE([HAVE_ATTRIBUTE__BOUNDED__], [1], [OpenBSD gcc has bounded])
		AC_DEFINE([HAVE_ATTRIBUTE__DEAD], [1], [OpenBSD gcc has __dead])
		AC_DEFINE([HAVE_ATTRIBUTE__PACKED], [1], [OpenBSD gcc has __packed])
		;;
	*solaris*)
		HOST_OS=solaris
		HOST_ABI=elf
		CPPFLAGS="$CPPFLAGS -D__EXTENSIONS__ -D_XOPEN_SOURCE=600 -DBSD_COMP"
		AC_SUBST([PLATFORM_LDADD], ['-lresolv -lsocket -lnsl'])
		;;
	*) ;;
esac

AM_CONDITIONAL([HOST_AIX],     [test x$HOST_OS = xaix])
AM_CONDITIONAL([HOST_CYGWIN],  [test x$HOST_OS = xcygwin])
AM_CONDITIONAL([HOST_DARWIN],  [test x$HOST_OS = xdarwin])
AM_CONDITIONAL([HOST_FREEBSD], [test x$HOST_OS = xfreebsd])
AM_CONDITIONAL([HOST_HPUX],    [test x$HOST_OS = xhpux])
AM_CONDITIONAL([HOST_LINUX],   [test x$HOST_OS = xlinux])
AM_CONDITIONAL([HOST_NETBSD],  [test x$HOST_OS = xnetbsd])
AM_CONDITIONAL([HOST_OPENBSD], [test x$HOST_OS = xopenbsd])
AM_CONDITIONAL([HOST_SOLARIS], [test x$HOST_OS = xsolaris])
])
