/* -*- buffer-read-only: t -*- vi: set ro: */
/* DO NOT EDIT! GENERATED AUTOMATICALLY! */
#line 1
/* Provide a more complete sys/stat header file.
   Copyright (C) 2005-2010 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Written by Eric Blake, Paul Eggert, and Jim Meyering.  */

/* This file is supposed to be used on platforms where <sys/stat.h> is
   incomplete.  It is intended to provide definitions and prototypes
   needed by an application.  Start with what the system provides.  */

#if __GNUC__ >= 3
@PRAGMA_SYSTEM_HEADER@
#endif

#if defined __need_system_sys_stat_h
/* Special invocation convention.  */

#@INCLUDE_NEXT@ @NEXT_SYS_STAT_H@

#else
/* Normal invocation convention.  */

#ifndef _GL_SYS_STAT_H

/* Get nlink_t.  */
#include <sys/types.h>

/* Get struct timespec.  */
#include <time.h>

/* The include_next requires a split double-inclusion guard.  */
#@INCLUDE_NEXT@ @NEXT_SYS_STAT_H@

#ifndef _GL_SYS_STAT_H
#define _GL_SYS_STAT_H

/* The definition of _GL_ARG_NONNULL is copied here.  */

/* The definition of _GL_WARN_ON_USE is copied here.  */

/* Before doing "#define mkdir rpl_mkdir" below, we need to include all
   headers that may declare mkdir().  */
#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
# include <io.h>
#endif

#ifndef S_IFMT
# define S_IFMT 0170000
#endif

#if STAT_MACROS_BROKEN
# undef S_ISBLK
# undef S_ISCHR
# undef S_ISDIR
# undef S_ISFIFO
# undef S_ISLNK
# undef S_ISNAM
# undef S_ISMPB
# undef S_ISMPC
# undef S_ISNWK
# undef S_ISREG
# undef S_ISSOCK
#endif

#ifndef S_ISBLK
# ifdef S_IFBLK
#  define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
# else
#  define S_ISBLK(m) 0
# endif
#endif

#ifndef S_ISCHR
# ifdef S_IFCHR
#  define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
# else
#  define S_ISCHR(m) 0
# endif
#endif

#ifndef S_ISDIR
# ifdef S_IFDIR
#  define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
# else
#  define S_ISDIR(m) 0
# endif
#endif

#ifndef S_ISDOOR /* Solaris 2.5 and up */
# define S_ISDOOR(m) 0
#endif

#ifndef S_ISFIFO
# ifdef S_IFIFO
#  define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
# else
#  define S_ISFIFO(m) 0
# endif
#endif

#ifndef S_ISLNK
# ifdef S_IFLNK
#  define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
# else
#  define S_ISLNK(m) 0
# endif
#endif

#ifndef S_ISMPB /* V7 */
# ifdef S_IFMPB
#  define S_ISMPB(m) (((m) & S_IFMT) == S_IFMPB)
#  define S_ISMPC(m) (((m) & S_IFMT) == S_IFMPC)
# else
#  define S_ISMPB(m) 0
#  define S_ISMPC(m) 0
# endif
#endif

#ifndef S_ISNAM /* Xenix */
# ifdef S_IFNAM
#  define S_ISNAM(m) (((m) & S_IFMT) == S_IFNAM)
# else
#  define S_ISNAM(m) 0
# endif
#endif

#ifndef S_ISNWK /* HP/UX */
# ifdef S_IFNWK
#  define S_ISNWK(m) (((m) & S_IFMT) == S_IFNWK)
# else
#  define S_ISNWK(m) 0
# endif
#endif

#ifndef S_ISPORT /* Solaris 10 and up */
# define S_ISPORT(m) 0
#endif

#ifndef S_ISREG
# ifdef S_IFREG
#  define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
# else
#  define S_ISREG(m) 0
# endif
#endif

#ifndef S_ISSOCK
# ifdef S_IFSOCK
#  define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
# else
#  define S_ISSOCK(m) 0
# endif
#endif


#ifndef S_TYPEISMQ
# define S_TYPEISMQ(p) 0
#endif

#ifndef S_TYPEISTMO
# define S_TYPEISTMO(p) 0
#endif


#ifndef S_TYPEISSEM
# ifdef S_INSEM
#  define S_TYPEISSEM(p) (S_ISNAM ((p)->st_mode) && (p)->st_rdev == S_INSEM)
# else
#  define S_TYPEISSEM(p) 0
# endif
#endif

#ifndef S_TYPEISSHM
# ifdef S_INSHD
#  define S_TYPEISSHM(p) (S_ISNAM ((p)->st_mode) && (p)->st_rdev == S_INSHD)
# else
#  define S_TYPEISSHM(p) 0
# endif
#endif

/* high performance ("contiguous data") */
#ifndef S_ISCTG
# define S_ISCTG(p) 0
#endif

/* Cray DMF (data migration facility): off line, with data  */
#ifndef S_ISOFD
# define S_ISOFD(p) 0
#endif

/* Cray DMF (data migration facility): off line, with no data  */
#ifndef S_ISOFL
# define S_ISOFL(p) 0
#endif

/* 4.4BSD whiteout */
#ifndef S_ISWHT
# define S_ISWHT(m) 0
#endif

/* If any of the following are undefined,
   define them to their de facto standard values.  */
#if !S_ISUID
# define S_ISUID 04000
#endif
#if !S_ISGID
# define S_ISGID 02000
#endif

/* S_ISVTX is a common extension to POSIX.  */
#ifndef S_ISVTX
# define S_ISVTX 01000
#endif

#if !S_IRUSR && S_IREAD
# define S_IRUSR S_IREAD
#endif
#if !S_IRUSR
# define S_IRUSR 00400
#endif
#if !S_IRGRP
# define S_IRGRP (S_IRUSR >> 3)
#endif
#if !S_IROTH
# define S_IROTH (S_IRUSR >> 6)
#endif

#if !S_IWUSR && S_IWRITE
# define S_IWUSR S_IWRITE
#endif
#if !S_IWUSR
# define S_IWUSR 00200
#endif
#if !S_IWGRP
# define S_IWGRP (S_IWUSR >> 3)
#endif
#if !S_IWOTH
# define S_IWOTH (S_IWUSR >> 6)
#endif

#if !S_IXUSR && S_IEXEC
# define S_IXUSR S_IEXEC
#endif
#if !S_IXUSR
# define S_IXUSR 00100
#endif
#if !S_IXGRP
# define S_IXGRP (S_IXUSR >> 3)
#endif
#if !S_IXOTH
# define S_IXOTH (S_IXUSR >> 6)
#endif

#if !S_IRWXU
# define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)
#endif
#if !S_IRWXG
# define S_IRWXG (S_IRGRP | S_IWGRP | S_IXGRP)
#endif
#if !S_IRWXO
# define S_IRWXO (S_IROTH | S_IWOTH | S_IXOTH)
#endif

/* S_IXUGO is a common extension to POSIX.  */
#if !S_IXUGO
# define S_IXUGO (S_IXUSR | S_IXGRP | S_IXOTH)
#endif

#ifndef S_IRWXUGO
# define S_IRWXUGO (S_IRWXU | S_IRWXG | S_IRWXO)
#endif

/* Macros for futimens and utimensat.  */
#ifndef UTIME_NOW
# define UTIME_NOW (-1)
# define UTIME_OMIT (-2)
#endif


#ifdef __cplusplus
extern "C" {
#endif


#if @GNULIB_FCHMODAT@
# if !@HAVE_FCHMODAT@
extern int fchmodat (int fd, char const *file, mode_t mode, int flag)
     _GL_ARG_NONNULL ((2));
# endif
#elif defined GNULIB_POSIXCHECK
# undef fchmodat
# if HAVE_RAW_DECL_FCHMODAT
_GL_WARN_ON_USE (fchmodat, "fchmodat is not portable - "
                 "use gnulib module openat for portability");
# endif
#endif


#if @REPLACE_FSTAT@
# define fstat rpl_fstat
extern int fstat (int fd, struct stat *buf) _GL_ARG_NONNULL ((2));
#endif


#if @GNULIB_FSTATAT@
# if @REPLACE_FSTATAT@
#  undef fstatat
#  define fstatat rpl_fstatat
# endif
# if !@HAVE_FSTATAT@ || @REPLACE_FSTATAT@
extern int fstatat (int fd, char const *name, struct stat *st, int flags)
     _GL_ARG_NONNULL ((2, 3));
# endif
#elif defined GNULIB_POSIXCHECK
# undef fstatat
# if HAVE_RAW_DECL_FSTATAT
_GL_WARN_ON_USE (fstatat, "fstatat is not portable - "
                 "use gnulib module openat for portability");
# endif
#endif


#if @GNULIB_FUTIMENS@
# if @REPLACE_FUTIMENS@
#  undef futimens
#  define futimens rpl_futimens
# endif
# if !@HAVE_FUTIMENS@ || @REPLACE_FUTIMENS@
extern int futimens (int fd, struct timespec const times[2]);
# endif
#elif defined GNULIB_POSIXCHECK
# undef futimens
# if HAVE_RAW_DECL_FUTIMENS
_GL_WARN_ON_USE (futimens, "futimens is not portable - "
                 "use gnulib module futimens for portability");
# endif
#endif


#if @GNULIB_LCHMOD@
/* Change the mode of FILENAME to MODE, without dereferencing it if FILENAME
   denotes a symbolic link.  */
# if !@HAVE_LCHMOD@
/* The lchmod replacement follows symbolic links.  Callers should take
   this into account; lchmod should be applied only to arguments that
   are known to not be symbolic links.  On hosts that lack lchmod,
   this can lead to race conditions between the check and the
   invocation of lchmod, but we know of no workarounds that are
   reliable in general.  You might try requesting support for lchmod
   from your operating system supplier.  */
#  define lchmod chmod
# endif
# if 0 /* assume already declared */
extern int lchmod (const char *filename, mode_t mode) _GL_ARG_NONNULL ((1));
# endif
#elif defined GNULIB_POSIXCHECK
# undef lchmod
# if HAVE_RAW_DECL_LCHMOD
_GL_WARN_ON_USE (lchmod, "lchmod is unportable - "
                 "use gnulib module lchmod for portability");
# endif
#endif


#if @GNULIB_LSTAT@
# if ! @HAVE_LSTAT@
/* mingw does not support symlinks, therefore it does not have lstat.  But
   without links, stat does just fine.  */
#  define lstat stat
# elif @REPLACE_LSTAT@
#  undef lstat
#  define lstat rpl_lstat
extern int rpl_lstat (const char *name, struct stat *buf)
     _GL_ARG_NONNULL ((1, 2));
# endif
#elif defined GNULIB_POSIXCHECK
# undef lstat
# if HAVE_RAW_DECL_LSTAT
_GL_WARN_ON_USE (lstat, "lstat is unportable - "
                 "use gnulib module lstat for portability");
# endif
#endif


#if @REPLACE_MKDIR@
# undef mkdir
# define mkdir rpl_mkdir
extern int mkdir (char const *name, mode_t mode) _GL_ARG_NONNULL ((1));
#else
/* mingw's _mkdir() function has 1 argument, but we pass 2 arguments.
   Additionally, it declares _mkdir (and depending on compile flags, an
   alias mkdir), only in the nonstandard <io.h>, which is included above.  */
# if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__

static inline int
rpl_mkdir (char const *name, mode_t mode)
{
  return _mkdir (name);
}

#  define mkdir rpl_mkdir
# endif
#endif


#if @GNULIB_MKDIRAT@
# if !@HAVE_MKDIRAT@
extern int mkdirat (int fd, char const *file, mode_t mode)
     _GL_ARG_NONNULL ((2));
# endif
#elif defined GNULIB_POSIXCHECK
# undef mkdirat
# if HAVE_RAW_DECL_MKDIRAT
_GL_WARN_ON_USE (mkdirat, "mkdirat is not portable - "
                 "use gnulib module openat for portability");
# endif
#endif


#if @GNULIB_MKFIFO@
# if @REPLACE_MKFIFO@
#  undef mkfifo
#  define mkfifo rpl_mkfifo
# endif
# if !@HAVE_MKFIFO@ || @REPLACE_MKFIFO@
extern int mkfifo (char const *file, mode_t mode) _GL_ARG_NONNULL ((1));
# endif
#elif defined GNULIB_POSIXCHECK
# undef mkfifo
# if HAVE_RAW_DECL_MKFIFO
_GL_WARN_ON_USE (mkfifo, "mkfifo is not portable - "
                 "use gnulib module mkfifo for portability");
# endif
#endif


#if @GNULIB_MKFIFOAT@
# if !@HAVE_MKFIFOAT@
extern int mkfifoat (int fd, char const *file, mode_t mode)
     _GL_ARG_NONNULL ((2));
# endif
#elif defined GNULIB_POSIXCHECK
# undef mkfifoat
# if HAVE_RAW_DECL_MKFIFOAT
_GL_WARN_ON_USE (mkfifoat, "mkfifoat is not portable - "
                 "use gnulib module mkfifoat for portability");
# endif
#endif


#if @GNULIB_MKNOD@
# if @REPLACE_MKNOD@
#  undef mknod
#  define mknod rpl_mknod
# endif
# if !@HAVE_MKNOD@ || @REPLACE_MKNOD@
extern int mknod (char const *file, mode_t mode, dev_t dev)
     _GL_ARG_NONNULL ((1));
# endif
#elif defined GNULIB_POSIXCHECK
# undef mknod
# if HAVE_RAW_DECL_MKNOD
_GL_WARN_ON_USE (mknod, "mknod is not portable - "
                 "use gnulib module mknod for portability");
# endif
#endif


#if @GNULIB_MKNODAT@
# if !@HAVE_MKNODAT@
extern int mknodat (int fd, char const *file, mode_t mode, dev_t dev)
     _GL_ARG_NONNULL ((2));
# endif
#elif defined GNULIB_POSIXCHECK
# undef mknodat
# if HAVE_RAW_DECL_MKNODAT
_GL_WARN_ON_USE (mknodat, "mknodat is not portable - "
                 "use gnulib module mkfifoat for portability");
# endif
#endif


#if @GNULIB_STAT@
# if @REPLACE_STAT@
/* We can't use the object-like #define stat rpl_stat, because of
   struct stat.  This means that rpl_stat will not be used if the user
   does (stat)(a,b).  Oh well.  */
#  undef stat
#  ifdef _LARGE_FILES
    /* With _LARGE_FILES defined, AIX (only) defines stat to stat64,
       so we have to replace stat64() instead of stat(). */
#   define stat stat64
#   undef stat64
#   define stat64(name, st) rpl_stat (name, st)
#  else /* !_LARGE_FILES */
#   define stat(name, st) rpl_stat (name, st)
#  endif /* !_LARGE_FILES */
extern int stat (const char *name, struct stat *buf) _GL_ARG_NONNULL ((1, 2));
# endif
#elif defined GNULIB_POSIXCHECK
# undef stat
# if HAVE_RAW_DECL_STAT
_GL_WARN_ON_USE (stat, "stat is unportable - "
                 "use gnulib module stat for portability");
# endif
#endif


#if @GNULIB_UTIMENSAT@
# if @REPLACE_UTIMENSAT@
#  undef utimensat
#  define utimensat rpl_utimensat
# endif
# if !@HAVE_UTIMENSAT@ || @REPLACE_UTIMENSAT@
   extern int utimensat (int fd, char const *name,
                         struct timespec const times[2], int flag)
        _GL_ARG_NONNULL ((2));
# endif
#elif defined GNULIB_POSIXCHECK
# undef utimensat
# if HAVE_RAW_DECL_UTIMENSAT
_GL_WARN_ON_USE (utimensat, "utimensat is not portable - "
                 "use gnulib module utimensat for portability");
# endif
#endif


#ifdef __cplusplus
}
#endif


#endif /* _GL_SYS_STAT_H */
#endif /* _GL_SYS_STAT_H */
#endif
