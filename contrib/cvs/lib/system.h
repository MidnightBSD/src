/* system-dependent definitions for CVS.
   Copyright (C) 1989-1992 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#include <sys/types.h>
#include <sys/stat.h>

#ifdef STAT_MACROS_BROKEN
#undef S_ISBLK
#undef S_ISCHR
#undef S_ISDIR
#undef S_ISREG
#undef S_ISFIFO
#undef S_ISLNK
#undef S_ISSOCK
#undef S_ISMPB
#undef S_ISMPC
#undef S_ISNWK
#endif

/* Not all systems have S_IFMT, but we want to use it if we have it.
   The S_IFMT code below looks right (it masks and compares).  The
   non-S_IFMT code looks bogus (are there really systems on which
   S_IFBLK, S_IFLNK, &c, each have their own bit?  I suspect it was
   written for OS/2 using the IBM C/C++ Tools 2.01 compiler).

   Of course POSIX systems will have S_IS*, so maybe the issue is
   semi-moot.  */

#if !defined(S_ISBLK) && defined(S_IFBLK)
# if defined(S_IFMT)
# define	S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
# else
# define S_ISBLK(m) ((m) & S_IFBLK)
# endif
#endif

#if !defined(S_ISCHR) && defined(S_IFCHR)
# if defined(S_IFMT)
# define	S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
# else
# define S_ISCHR(m) ((m) & S_IFCHR)
# endif
#endif

#if !defined(S_ISDIR) && defined(S_IFDIR)
# if defined(S_IFMT)
# define	S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
# else
# define S_ISDIR(m) ((m) & S_IFDIR)
# endif
#endif

#if !defined(S_ISREG) && defined(S_IFREG)
# if defined(S_IFMT)
# define	S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
# else
# define S_ISREG(m) ((m) & S_IFREG)
# endif
#endif

#if !defined(S_ISFIFO) && defined(S_IFIFO)
# if defined(S_IFMT)
# define	S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
# else
# define S_ISFIFO(m) ((m) & S_IFIFO)
# endif
#endif

#if !defined(S_ISLNK) && defined(S_IFLNK)
# if defined(S_IFMT)
# define	S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
# else
# define S_ISLNK(m) ((m) & S_IFLNK)
# endif
#endif

#ifndef S_ISSOCK
# if defined( S_IFSOCK )
#   ifdef S_IFMT
#     define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#   else
#     define S_ISSOCK(m) ((m) & S_IFSOCK)
#   endif /* S_IFMT */
# elif defined( S_ISNAM )
    /* SCO OpenServer 5.0.6a */
#   define S_ISSOCK S_ISNAM
# endif /* !S_IFSOCK && S_ISNAM */
#endif /* !S_ISSOCK */

#if !defined(S_ISMPB) && defined(S_IFMPB) /* V7 */
# if defined(S_IFMT)
# define S_ISMPB(m) (((m) & S_IFMT) == S_IFMPB)
# define S_ISMPC(m) (((m) & S_IFMT) == S_IFMPC)
# else
# define S_ISMPB(m) ((m) & S_IFMPB)
# define S_ISMPC(m) ((m) & S_IFMPC)
# endif
#endif

#if !defined(S_ISNWK) && defined(S_IFNWK) /* HP/UX */
# if defined(S_IFMT)
# define S_ISNWK(m) (((m) & S_IFMT) == S_IFNWK)
# else
# define S_ISNWK(m) ((m) & S_IFNWK)
# endif
#endif

#ifdef NEED_DECOY_PERMISSIONS        /* OS/2, really */

#define	S_IRUSR S_IREAD
#define	S_IWUSR S_IWRITE
#define	S_IXUSR S_IEXEC
#define	S_IRWXU	(S_IRUSR | S_IWUSR | S_IXUSR)
#define	S_IRGRP S_IREAD
#define	S_IWGRP S_IWRITE
#define	S_IXGRP S_IEXEC
#define	S_IRWXG	(S_IRGRP | S_IWGRP | S_IXGRP)
#define	S_IROTH S_IREAD
#define	S_IWOTH S_IWRITE
#define	S_IXOTH S_IEXEC
#define	S_IRWXO	(S_IROTH | S_IWOTH | S_IXOTH)

#else /* ! NEED_DECOY_PERMISSIONS */

#ifndef S_IRUSR
#define	S_IRUSR 0400
#define	S_IWUSR 0200
#define	S_IXUSR 0100
/* Read, write, and execute by owner.  */
#define	S_IRWXU	(S_IRUSR|S_IWUSR|S_IXUSR)

#define	S_IRGRP	(S_IRUSR >> 3)	/* Read by group.  */
#define	S_IWGRP	(S_IWUSR >> 3)	/* Write by group.  */
#define	S_IXGRP	(S_IXUSR >> 3)	/* Execute by group.  */
/* Read, write, and execute by group.  */
#define	S_IRWXG	(S_IRWXU >> 3)

#define	S_IROTH	(S_IRGRP >> 3)	/* Read by others.  */
#define	S_IWOTH	(S_IWGRP >> 3)	/* Write by others.  */
#define	S_IXOTH	(S_IXGRP >> 3)	/* Execute by others.  */
/* Read, write, and execute by others.  */
#define	S_IRWXO	(S_IRWXG >> 3)
#endif /* !def S_IRUSR */
#endif /* NEED_DECOY_PERMISSIONS */

#if defined(POSIX) || defined(HAVE_UNISTD_H)
#include <unistd.h>
#include <limits.h>
#else
off_t lseek ();
char *getcwd ();
#endif

#include "xtime.h"

#ifdef HAVE_IO_H
#include <io.h>
#endif

#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif



/*
**  MAXPATHLEN and PATH_MAX
**
**     On most systems MAXPATHLEN is defined in sys/param.h to be 1024. Of
**     those that this is not true, again most define PATH_MAX in limits.h
**     or sys/limits.h which usually gets included by limits.h. On the few
**     remaining systems that neither statement is true, _POSIX_PATH_MAX 
**     is defined.
**
**     So:
**         1. If PATH_MAX is defined just use it.
**         2. If MAXPATHLEN is defined but not PATH_MAX, then define
**            PATH_MAX in terms of MAXPATHLEN.
**         3. If neither is defined, include limits.h and check for
**            PATH_MAX again.
**         3.1 If we now have PATHSIZE, define PATH_MAX in terms of that.
**             and ignore the rest.  Since _POSIX_PATH_MAX (checked for
**             next) is the *most* restrictive (smallest) value, if we
**             trust _POSIX_PATH_MAX, several of our buffers are too small.
**         4. If PATH_MAX is still not defined but _POSIX_PATH_MAX is,
**            then define PATH_MAX in terms of _POSIX_PATH_MAX.
**         5. And if even _POSIX_PATH_MAX doesn't exist just put in
**            a reasonable value.
**         *. All in all, this is an excellent argument for using pathconf()
**            when at all possible.  Or better yet, dynamically allocate
**            our buffers and use getcwd() not getwd().
**
**     This works on:
**         Sun Sparc 10        SunOS 4.1.3  &  Solaris 1.2
**         HP 9000/700         HP/UX 8.07   &  HP/UX 9.01
**         Tektronix XD88/10   UTekV 3.2e
**         IBM RS6000          AIX 3.2
**         Dec Alpha           OSF 1 ????
**         Intel 386           BSDI BSD/386
**         Intel 386           SCO OpenServer Release 5
**         Apollo              Domain 10.4
**         NEC                 SVR4
*/

/* On MOST systems this will get you MAXPATHLEN.
   Windows NT doesn't have this file, tho.  */
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifndef PATH_MAX  
#  ifdef MAXPATHLEN
#    define PATH_MAX                 MAXPATHLEN
#  else
#    include <limits.h>
#    ifndef PATH_MAX
#      ifdef PATHSIZE
#         define PATH_MAX               PATHSIZE
#      else /* no PATHSIZE */
#        ifdef _POSIX_PATH_MAX
#          define PATH_MAX             _POSIX_PATH_MAX
#        else
#          define PATH_MAX             1024
#        endif  /* no _POSIX_PATH_MAX */
#      endif  /* no PATHSIZE */
#    endif /* no PATH_MAX   */
#  endif  /* MAXPATHLEN */
#endif  /* PATH_MAX   */


/* The NeXT (without _POSIX_SOURCE, which we don't want) has a utime.h
   which doesn't define anything.  It would be cleaner to have configure
   check for struct utimbuf, but for now I'm checking NeXT here (so I don't
   have to debug the configure check across all the machines).  */
#if defined (HAVE_UTIME_H) && !defined (NeXT)
#  include <utime.h>
#else
#  if defined (HAVE_SYS_UTIME_H)
#    include <sys/utime.h>
#  else
#    ifndef ALTOS
struct utimbuf
{
  long actime;
  long modtime;
};
#    endif
int utime ();
#  endif
#endif

#include <string.h>

#ifndef ERRNO_H_MISSING
#include <errno.h>
#endif

/* Not all systems set the same error code on a non-existent-file
   error.  This tries to ask the question somewhat portably.
   On systems that don't have ENOTEXIST, this should behave just like
   x == ENOENT.  "x" is probably errno, of course. */

#ifdef ENOTEXIST
#  ifdef EOS2ERR
#    define existence_error(x) \
     (((x) == ENOTEXIST) || ((x) == ENOENT) || ((x) == EOS2ERR))
#  else
#    define existence_error(x) \
     (((x) == ENOTEXIST) || ((x) == ENOENT))
#  endif
#else
#  ifdef EVMSERR
#     define existence_error(x) \
((x) == ENOENT || (x) == EINVAL || (x) == EVMSERR)
#  else
#    define existence_error(x) ((x) == ENOENT)
#  endif
#endif


#ifdef STDC_HEADERS
# include <stdlib.h>
#else
char *getenv ();
char *malloc ();
char *realloc ();
char *calloc ();
extern int errno;
#endif

/* SunOS4 apparently does not define this in stdlib.h.  */
#ifndef EXIT_FAILURE
# define EXIT_FAILURE 1
#endif

/* check for POSIX signals */
#if defined(HAVE_SIGACTION) && defined(HAVE_SIGPROCMASK)
# define POSIX_SIGNALS
#endif

/* MINIX 1.6 doesn't properly support sigaction */
#if defined(_MINIX)
# undef POSIX_SIGNALS
#endif

/* If !POSIX, try for BSD.. Reason: 4.4BSD implements these as wrappers */
#if !defined(POSIX_SIGNALS)
# if defined(HAVE_SIGVEC) && defined(HAVE_SIGSETMASK) && defined(HAVE_SIGBLOCK)
#  define BSD_SIGNALS
# endif
#endif

/* Under OS/2, this must be included _after_ stdio.h; that's why we do
   it here. */
#ifdef USE_OWN_TCPIP_H
# include "tcpip.h"
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#else
# include <sys/file.h>
#endif

#ifndef SEEK_SET
# define SEEK_SET 0
# define SEEK_CUR 1
# define SEEK_END 2
#endif

#ifndef F_OK
# define F_OK 0
# define X_OK 1
# define W_OK 2
# define R_OK 4
#endif

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

/* Convert B 512-byte blocks to kilobytes if K is nonzero,
   otherwise return it unchanged. */
#define convert_blocks(b, k) ((k) ? ((b) + 1) / 2 : (b))

#ifndef S_ISLNK
# define lstat stat
#endif

/*
 * Some UNIX distributions don't include these in their stat.h Defined here
 * because "config.h" is always included last.
 */
#ifndef S_IWRITE
# define	S_IWRITE	0000200    /* write permission, owner */
#endif
#ifndef S_IWGRP
# define	S_IWGRP		0000020    /* write permission, grougroup */
#endif
#ifndef S_IWOTH
# define	S_IWOTH		0000002    /* write permission, other */
#endif

/* Under non-UNIX operating systems (MS-DOS, WinNT, MacOS), many filesystem
   calls take  only one argument; permission is handled very differently on
   those systems than in Unix.  So we leave such systems a hook on which they
   can hang their own definitions.  */

#ifndef CVS_ACCESS
# define CVS_ACCESS access
#endif

#ifndef CVS_CHDIR
# define CVS_CHDIR chdir
#endif

#ifndef CVS_CREAT
# define CVS_CREAT creat
#endif

#ifndef CVS_FOPEN
# define CVS_FOPEN fopen
#endif

#ifndef CVS_FDOPEN
# define CVS_FDOPEN fdopen
#endif

#ifndef CVS_MKDIR
# define CVS_MKDIR mkdir
#endif

#ifndef CVS_OPEN
# define CVS_OPEN open
#endif

#ifndef CVS_READDIR
# define CVS_READDIR readdir
#endif

#ifndef CVS_CLOSEDIR
# define CVS_CLOSEDIR closedir
#endif

#ifndef CVS_OPENDIR
# define CVS_OPENDIR opendir
#endif

#ifndef CVS_RENAME
# define CVS_RENAME rename
#endif

#ifndef CVS_RMDIR
# define CVS_RMDIR rmdir
#endif

#ifndef CVS_STAT
# define CVS_STAT stat
#endif

/* Open question: should CVS_STAT be lstat by default?  We need
   to use lstat in order to handle symbolic links correctly with
   the PreservePermissions option. -twp */
#ifndef CVS_LSTAT
# define CVS_LSTAT lstat
#endif

#ifndef CVS_UNLINK
# define CVS_UNLINK unlink
#endif

/* Wildcard matcher.  Should be case-insensitive if the system is.  */
#ifndef CVS_FNMATCH
# define CVS_FNMATCH fnmatch
#endif

#ifdef WIN32
/*
 * According to GNU conventions, we should avoid referencing any macro
 * containing "WIN" as a reference to Microsoft Windows, as we would like to
 * avoid any implication that we consider Microsoft Windows any sort of "win".
 *
 * FIXME: As of 2003-06-09, folks on the GNULIB project were discussing
 * defining a configure macro to define WOE32 appropriately.  If they ever do
 * write such a beast, we should use it, though in most cases it would be
 * preferable to avoid referencing any OS or compiler anyhow, per Autoconf
 * convention, and reference only tested features of the system.
 */
# define WOE32 1
#endif /* WIN32 */


#ifdef WOE32
  /* Under Windows NT, filenames are case-insensitive.  */
# define FILENAMES_CASE_INSENSITIVE 1
#endif /* WOE32 */



#ifdef FILENAMES_CASE_INSENSITIVE

# if defined (__CYGWIN32__) || defined (WOE32)
    /* Under Windows, filenames are case-insensitive, and both / and \
       are path component separators.  */
#   define FOLD_FN_CHAR(c) (WNT_filename_classes[(unsigned char) (c)])
extern unsigned char WNT_filename_classes[];
    /* Is the character C a path name separator?  Under
       Windows NT, you can use either / or \.  */
#   define ISDIRSEP(c) (FOLD_FN_CHAR(c) == '/')
#   define ISABSOLUTE(s) (ISDIRSEP(s[0]) || FOLD_FN_CHAR(s[0]) >= 'a' && FOLD_FN_CHAR(s[0]) <= 'z' && s[1] == ':' && ISDIRSEP(s[2]))
# else /* !__CYGWIN32__ && !WOE32 */
  /* As far as I know, only Macintosh OS X & VMS make it here, but any
   * platform defining FILENAMES_CASE_INSENSITIVE which isn't WOE32 or
   * piggy-backing the same could, in theory.  Since the OS X fold just folds
   * A-Z into a-z, I'm just allowing it to be used for any case insensitive
   * system which we aren't yet making other specific folds or exceptions for.
   * WOE32 needs its own class since \ and C:\ style absolute paths also need
   * to be accounted for.
   */
#  if defined(USE_VMS_FILENAMES)
#   define FOLD_FN_CHAR(c) (VMS_filename_classes[(unsigned char) (c)])
extern unsigned char VMS_filename_classes[];
#  else
#   define FOLD_FN_CHAR(c) (OSX_filename_classes[(unsigned char) (c)])
extern unsigned char OSX_filename_classes[];
#  endif
# endif /* __CYGWIN32__ || WOE32 */

/* The following need to be declared for all case insensitive filesystems.
 * When not FOLD_FN_CHAR is not #defined, a default definition for these
 * functions is provided later in this header file.  */

/* Like strcmp, but with the appropriate tweaks for file names.  */
extern int fncmp (const char *n1, const char *n2);

/* Fold characters in FILENAME to their canonical forms.  */
extern void fnfold (char *FILENAME);

#endif /* FILENAMES_CASE_INSENSITIVE */



/* Some file systems are case-insensitive.  If FOLD_FN_CHAR is
   #defined, it maps the character C onto its "canonical" form.  In a
   case-insensitive system, it would map all alphanumeric characters
   to lower case.  Under Windows NT, / and \ are both path component
   separators, so FOLD_FN_CHAR would map them both to /.  */
#ifndef FOLD_FN_CHAR
# define FOLD_FN_CHAR(c) (c)
# define fnfold(filename) (filename)
# define fncmp strcmp
#endif

/* Different file systems have different path component separators.
   For the VMS port we might need to abstract further back than this.  */
#ifndef ISDIRSEP
# define ISDIRSEP(c) ((c) == '/')
#endif

/* Different file systems can have different naming patterns which designate
 * a path as absolute
 */
#ifndef ISABSOLUTE
# define ISABSOLUTE(s) ISDIRSEP(s[0])
#endif


/* On some systems, we have to be careful about writing/reading files
   in text or binary mode (so in text mode the system can handle CRLF
   vs. LF, VMS text file conventions, &c).  We decide to just always
   be careful.  That way we don't have to worry about whether text and
   binary differ on this system.  We just have to worry about whether
   the system has O_BINARY and "rb".  The latter is easy; all ANSI C
   libraries have it, SunOS4 has it, and CVS has used it unguarded
   some places for a while now without complaints (e.g. "rb" in
   server.c (server_updated), since CVS 1.8).  The former is just an
   #ifdef.  */

#define FOPEN_BINARY_READ ("rb")
#define FOPEN_BINARY_WRITE ("wb")
#define FOPEN_BINARY_READWRITE ("r+b")

#ifdef O_BINARY
#define OPEN_BINARY (O_BINARY)
#else
#define OPEN_BINARY (0)
#endif
