#define PERL_EXT_POSIX

#ifdef NETWARE
	#define _POSIX_
	/*
	 * Ideally this should be somewhere down in the includes
	 * but putting it in other places is giving compiler errors.
	 * Also here I am unable to check for HAS_UNAME since it wouldn't have
	 * yet come into the file at this stage - sgp 18th Oct 2000
	 */
	#include <sys/utsname.h>
#endif	/* NETWARE */

#define PERL_NO_GET_CONTEXT

#include "EXTERN.h"
#define PERLIO_NOT_STDIO 1
#include "perl.h"
#include "XSUB.h"
#if defined(PERL_IMPLICIT_SYS)
#  undef signal
#  undef open
#  undef setmode
#  define open PerlLIO_open3
#endif
#include <ctype.h>
#ifdef I_DIRENT    /* XXX maybe better to just rely on perl.h? */
#include <dirent.h>
#endif
#include <errno.h>
#ifdef I_FLOAT
#include <float.h>
#endif
#ifdef I_LIMITS
#include <limits.h>
#endif
#include <locale.h>
#include <math.h>
#ifdef I_PWD
#include <pwd.h>
#endif
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>

#ifdef I_STDDEF
#include <stddef.h>
#endif

#ifdef I_UNISTD
#include <unistd.h>
#endif

/* XXX This comment is just to make I_TERMIO and I_SGTTY visible to
   metaconfig for future extension writers.  We don't use them in POSIX.
   (This is really sneaky :-)  --AD
*/
#if defined(I_TERMIOS)
#include <termios.h>
#endif
#ifdef I_STDLIB
#include <stdlib.h>
#endif
#ifndef __ultrix__
#include <string.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#ifdef I_UNISTD
#include <unistd.h>
#endif
#include <fcntl.h>

#ifdef HAS_TZNAME
#  if !defined(WIN32) && !defined(__CYGWIN__) && !defined(NETWARE) && !defined(__UWIN__)
extern char *tzname[];
#  endif
#else
#if !defined(WIN32) && !defined(__UWIN__) || (defined(__MINGW32__) && !defined(tzname))
char *tzname[] = { "" , "" };
#endif
#endif

#ifndef PERL_UNUSED_DECL
#  ifdef HASATTRIBUTE
#    if (defined(__GNUC__) && defined(__cplusplus)) || defined(__INTEL_COMPILER)
#      define PERL_UNUSED_DECL
#    else
#      define PERL_UNUSED_DECL __attribute__((unused))
#    endif
#  else
#    define PERL_UNUSED_DECL
#  endif
#endif

#ifndef dNOOP
#define dNOOP extern int Perl___notused PERL_UNUSED_DECL
#endif

#ifndef dVAR
#define dVAR dNOOP
#endif

#if defined(__VMS) && !defined(__POSIX_SOURCE)
#  include <libdef.h>       /* LIB$_INVARG constant */
#  include <lib$routines.h> /* prototype for lib$ediv() */
#  include <starlet.h>      /* prototype for sys$gettim() */
#  if DECC_VERSION < 50000000
#    define pid_t int       /* old versions of DECC miss this in types.h */
#  endif

#  undef mkfifo
#  define mkfifo(a,b) (not_here("mkfifo"),-1)
#  define tzset() not_here("tzset")

#if ((__VMS_VER >= 70000000) && (__DECC_VER >= 50200000)) || (__CRTL_VER >= 70000000)
#    define HAS_TZNAME  /* shows up in VMS 7.0 or Dec C 5.6 */
#    include <utsname.h>
#  endif /* __VMS_VER >= 70000000 or Dec C 5.6 */

   /* The POSIX notion of ttyname() is better served by getname() under VMS */
   static char ttnambuf[64];
#  define ttyname(fd) (isatty(fd) > 0 ? getname(fd,ttnambuf,0) : NULL)

   /* The non-POSIX CRTL times() has void return type, so we just get the
      current time directly */
   clock_t vms_times(struct tms *bufptr) {
	dTHX;
	clock_t retval;
	/* Get wall time and convert to 10 ms intervals to
	 * produce the return value that the POSIX standard expects */
#  if defined(__DECC) && defined (__ALPHA)
#    include <ints.h>
	uint64 vmstime;
	_ckvmssts(sys$gettim(&vmstime));
	vmstime /= 100000;
	retval = vmstime & 0x7fffffff;
#  else
	/* (Older hw or ccs don't have an atomic 64-bit type, so we
	 * juggle 32-bit ints (and a float) to produce a time_t result
	 * with minimal loss of information.) */
	long int vmstime[2],remainder,divisor = 100000;
	_ckvmssts(sys$gettim((unsigned long int *)vmstime));
	vmstime[1] &= 0x7fff;  /* prevent overflow in EDIV */
	_ckvmssts(lib$ediv(&divisor,vmstime,(long int *)&retval,&remainder));
#  endif
	/* Fill in the struct tms using the CRTL routine . . .*/
	times((tbuffer_t *)bufptr);
	return (clock_t) retval;
   }
#  define times(t) vms_times(t)
#else
#if defined (__CYGWIN__)
#    define tzname _tzname
#endif
#if defined (WIN32) || defined (NETWARE)
#  undef mkfifo
#  define mkfifo(a,b) not_here("mkfifo")
#  define ttyname(a) (char*)not_here("ttyname")
#  define sigset_t long
#  define pid_t long
#  ifdef __BORLANDC__
#    define tzname _tzname
#  endif
#  ifdef _MSC_VER
#    define mode_t short
#  endif
#  ifdef __MINGW32__
#    define mode_t short
#    ifndef tzset
#      define tzset()		not_here("tzset")
#    endif
#    ifndef _POSIX_OPEN_MAX
#      define _POSIX_OPEN_MAX	FOPEN_MAX	/* XXX bogus ? */
#    endif
#  endif
#  define sigaction(a,b,c)	not_here("sigaction")
#  define sigpending(a)		not_here("sigpending")
#  define sigprocmask(a,b,c)	not_here("sigprocmask")
#  define sigsuspend(a)		not_here("sigsuspend")
#  define sigemptyset(a)	not_here("sigemptyset")
#  define sigaddset(a,b)	not_here("sigaddset")
#  define sigdelset(a,b)	not_here("sigdelset")
#  define sigfillset(a)		not_here("sigfillset")
#  define sigismember(a,b)	not_here("sigismember")
#ifndef NETWARE
#  undef setuid
#  undef setgid
#  define setuid(a)		not_here("setuid")
#  define setgid(a)		not_here("setgid")
#endif	/* NETWARE */
#else

#  ifndef HAS_MKFIFO
#    if defined(OS2)
#      define mkfifo(a,b) not_here("mkfifo")
#    else	/* !( defined OS2 ) */
#      ifndef mkfifo
#        define mkfifo(path, mode) (mknod((path), (mode) | S_IFIFO, 0))
#      endif
#    endif
#  endif /* !HAS_MKFIFO */

#  ifdef I_GRP
#    include <grp.h>
#  endif
#  include <sys/times.h>
#  ifdef HAS_UNAME
#    include <sys/utsname.h>
#  endif
#  include <sys/wait.h>
#  ifdef I_UTIME
#    include <utime.h>
#  endif
#endif /* WIN32 || NETWARE */
#endif /* __VMS */

#ifdef WIN32
   /* Perl on Windows assigns WSAGetLastError() return values to errno
    * (in win32/win32sck.c).  Therefore we need to map these values
    * back to standard symbolic names, but only for those names having
    * no existing value or an existing value >= 100. (VC++ 2010 defines
    * a group of names with values >= 100 in its errno.h which we *do*
    * need to redefine.) The Errno.pm module does a similar mapping.
    */
#  ifdef EWOULDBLOCK
#    undef EWOULDBLOCK
#  endif
#  define EWOULDBLOCK WSAEWOULDBLOCK
#  ifdef EINPROGRESS
#    undef EINPROGRESS
#  endif
#  define EINPROGRESS WSAEINPROGRESS
#  ifdef EALREADY
#    undef EALREADY
#  endif
#  define EALREADY WSAEALREADY
#  ifdef ENOTSOCK
#    undef ENOTSOCK
#  endif
#  define ENOTSOCK WSAENOTSOCK
#  ifdef EDESTADDRREQ
#    undef EDESTADDRREQ
#  endif
#  define EDESTADDRREQ WSAEDESTADDRREQ
#  ifdef EMSGSIZE
#    undef EMSGSIZE
#  endif
#  define EMSGSIZE WSAEMSGSIZE
#  ifdef EPROTOTYPE
#    undef EPROTOTYPE
#  endif
#  define EPROTOTYPE WSAEPROTOTYPE
#  ifdef ENOPROTOOPT
#    undef ENOPROTOOPT
#  endif
#  define ENOPROTOOPT WSAENOPROTOOPT
#  ifdef EPROTONOSUPPORT
#    undef EPROTONOSUPPORT
#  endif
#  define EPROTONOSUPPORT WSAEPROTONOSUPPORT
#  ifdef ESOCKTNOSUPPORT
#    undef ESOCKTNOSUPPORT
#  endif
#  define ESOCKTNOSUPPORT WSAESOCKTNOSUPPORT
#  ifdef EOPNOTSUPP
#    undef EOPNOTSUPP
#  endif
#  define EOPNOTSUPP WSAEOPNOTSUPP
#  ifdef EPFNOSUPPORT
#    undef EPFNOSUPPORT
#  endif
#  define EPFNOSUPPORT WSAEPFNOSUPPORT
#  ifdef EAFNOSUPPORT
#    undef EAFNOSUPPORT
#  endif
#  define EAFNOSUPPORT WSAEAFNOSUPPORT
#  ifdef EADDRINUSE
#    undef EADDRINUSE
#  endif
#  define EADDRINUSE WSAEADDRINUSE
#  ifdef EADDRNOTAVAIL
#    undef EADDRNOTAVAIL
#  endif
#  define EADDRNOTAVAIL WSAEADDRNOTAVAIL
#  ifdef ENETDOWN
#    undef ENETDOWN
#  endif
#  define ENETDOWN WSAENETDOWN
#  ifdef ENETUNREACH
#    undef ENETUNREACH
#  endif
#  define ENETUNREACH WSAENETUNREACH
#  ifdef ENETRESET
#    undef ENETRESET
#  endif
#  define ENETRESET WSAENETRESET
#  ifdef ECONNABORTED
#    undef ECONNABORTED
#  endif
#  define ECONNABORTED WSAECONNABORTED
#  ifdef ECONNRESET
#    undef ECONNRESET
#  endif
#  define ECONNRESET WSAECONNRESET
#  ifdef ENOBUFS
#    undef ENOBUFS
#  endif
#  define ENOBUFS WSAENOBUFS
#  ifdef EISCONN
#    undef EISCONN
#  endif
#  define EISCONN WSAEISCONN
#  ifdef ENOTCONN
#    undef ENOTCONN
#  endif
#  define ENOTCONN WSAENOTCONN
#  ifdef ESHUTDOWN
#    undef ESHUTDOWN
#  endif
#  define ESHUTDOWN WSAESHUTDOWN
#  ifdef ETOOMANYREFS
#    undef ETOOMANYREFS
#  endif
#  define ETOOMANYREFS WSAETOOMANYREFS
#  ifdef ETIMEDOUT
#    undef ETIMEDOUT
#  endif
#  define ETIMEDOUT WSAETIMEDOUT
#  ifdef ECONNREFUSED
#    undef ECONNREFUSED
#  endif
#  define ECONNREFUSED WSAECONNREFUSED
#  ifdef ELOOP
#    undef ELOOP
#  endif
#  define ELOOP WSAELOOP
#  ifdef EHOSTDOWN
#    undef EHOSTDOWN
#  endif
#  define EHOSTDOWN WSAEHOSTDOWN
#  ifdef EHOSTUNREACH
#    undef EHOSTUNREACH
#  endif
#  define EHOSTUNREACH WSAEHOSTUNREACH
#  ifdef EPROCLIM
#    undef EPROCLIM
#  endif
#  define EPROCLIM WSAEPROCLIM
#  ifdef EUSERS
#    undef EUSERS
#  endif
#  define EUSERS WSAEUSERS
#  ifdef EDQUOT
#    undef EDQUOT
#  endif
#  define EDQUOT WSAEDQUOT
#  ifdef ESTALE
#    undef ESTALE
#  endif
#  define ESTALE WSAESTALE
#  ifdef EREMOTE
#    undef EREMOTE
#  endif
#  define EREMOTE WSAEREMOTE
#  ifdef EDISCON
#    undef EDISCON
#  endif
#  define EDISCON WSAEDISCON
#endif

typedef int SysRet;
typedef long SysRetLong;
typedef sigset_t* POSIX__SigSet;
typedef HV* POSIX__SigAction;
#ifdef I_TERMIOS
typedef struct termios* POSIX__Termios;
#else /* Define termios types to int, and call not_here for the functions.*/
#define POSIX__Termios int
#define speed_t int
#define tcflag_t int
#define cc_t int
#define cfgetispeed(x) not_here("cfgetispeed")
#define cfgetospeed(x) not_here("cfgetospeed")
#define tcdrain(x) not_here("tcdrain")
#define tcflush(x,y) not_here("tcflush")
#define tcsendbreak(x,y) not_here("tcsendbreak")
#define cfsetispeed(x,y) not_here("cfsetispeed")
#define cfsetospeed(x,y) not_here("cfsetospeed")
#define ctermid(x) (char *) not_here("ctermid")
#define tcflow(x,y) not_here("tcflow")
#define tcgetattr(x,y) not_here("tcgetattr")
#define tcsetattr(x,y,z) not_here("tcsetattr")
#endif

/* Possibly needed prototypes */
#ifndef WIN32
double strtod (const char *, char **);
long strtol (const char *, char **, int);
unsigned long strtoul (const char *, char **, int);
#endif

#ifndef HAS_DIFFTIME
#ifndef difftime
#define difftime(a,b) not_here("difftime")
#endif
#endif
#ifndef HAS_FPATHCONF
#define fpathconf(f,n)	(SysRetLong) not_here("fpathconf")
#endif
#ifndef HAS_MKTIME
#define mktime(a) not_here("mktime")
#endif
#ifndef HAS_NICE
#define nice(a) not_here("nice")
#endif
#ifndef HAS_PATHCONF
#define pathconf(f,n)	(SysRetLong) not_here("pathconf")
#endif
#ifndef HAS_SYSCONF
#define sysconf(n)	(SysRetLong) not_here("sysconf")
#endif
#ifndef HAS_READLINK
#define readlink(a,b,c) not_here("readlink")
#endif
#ifndef HAS_SETPGID
#define setpgid(a,b) not_here("setpgid")
#endif
#ifndef HAS_SETSID
#define setsid() not_here("setsid")
#endif
#ifndef HAS_STRCOLL
#define strcoll(s1,s2) not_here("strcoll")
#endif
#ifndef HAS_STRTOD
#define strtod(s1,s2) not_here("strtod")
#endif
#ifndef HAS_STRTOL
#define strtol(s1,s2,b) not_here("strtol")
#endif
#ifndef HAS_STRTOUL
#define strtoul(s1,s2,b) not_here("strtoul")
#endif
#ifndef HAS_STRXFRM
#define strxfrm(s1,s2,n) not_here("strxfrm")
#endif
#ifndef HAS_TCGETPGRP
#define tcgetpgrp(a) not_here("tcgetpgrp")
#endif
#ifndef HAS_TCSETPGRP
#define tcsetpgrp(a,b) not_here("tcsetpgrp")
#endif
#ifndef HAS_TIMES
#ifndef NETWARE
#define times(a) not_here("times")
#endif	/* NETWARE */
#endif
#ifndef HAS_UNAME
#define uname(a) not_here("uname")
#endif
#ifndef HAS_WAITPID
#define waitpid(a,b,c) not_here("waitpid")
#endif

#ifndef HAS_MBLEN
#ifndef mblen
#define mblen(a,b) not_here("mblen")
#endif
#endif
#ifndef HAS_MBSTOWCS
#define mbstowcs(s, pwcs, n) not_here("mbstowcs")
#endif
#ifndef HAS_MBTOWC
#define mbtowc(pwc, s, n) not_here("mbtowc")
#endif
#ifndef HAS_WCSTOMBS
#define wcstombs(s, pwcs, n) not_here("wcstombs")
#endif
#ifndef HAS_WCTOMB
#define wctomb(s, wchar) not_here("wcstombs")
#endif
#if !defined(HAS_MBLEN) && !defined(HAS_MBSTOWCS) && !defined(HAS_MBTOWC) && !defined(HAS_WCSTOMBS) && !defined(HAS_WCTOMB)
/* If we don't have these functions, then we wouldn't have gotten a typedef
   for wchar_t, the wide character type.  Defining wchar_t allows the
   functions referencing it to compile.  Its actual type is then meaningless,
   since without the above functions, all sections using it end up calling
   not_here() and croak.  --Kaveh Ghazi (ghazi@noc.rutgers.edu) 9/18/94. */
#ifndef wchar_t
#define wchar_t char
#endif
#endif

#ifndef HAS_LOCALECONV
#define localeconv() not_here("localeconv")
#endif

#ifdef HAS_LONG_DOUBLE
#  if LONG_DOUBLESIZE > NVSIZE
#    undef HAS_LONG_DOUBLE  /* XXX until we figure out how to use them */
#  endif
#endif

#ifndef HAS_LONG_DOUBLE
#ifdef LDBL_MAX
#undef LDBL_MAX
#endif
#ifdef LDBL_MIN
#undef LDBL_MIN
#endif
#ifdef LDBL_EPSILON
#undef LDBL_EPSILON
#endif
#endif

/* Background: in most systems the low byte of the wait status
 * is the signal (the lowest 7 bits) and the coredump flag is
 * the eight bit, and the second lowest byte is the exit status.
 * BeOS bucks the trend and has the bytes in different order.
 * See beos/beos.c for how the reality is bent even in BeOS
 * to follow the traditional.  However, to make the POSIX
 * wait W*() macros to work in BeOS, we need to unbend the
 * reality back in place. --jhi */
/* In actual fact the code below is to blame here. Perl has an internal
 * representation of the exit status ($?), which it re-composes from the
 * OS's representation using the W*() POSIX macros. The code below
 * incorrectly uses the W*() macros on the internal representation,
 * which fails for OSs that have a different representation (namely BeOS
 * and Haiku). WMUNGE() is a hack that converts the internal
 * representation into the OS specific one, so that the W*() macros work
 * as expected. The better solution would be not to use the W*() macros
 * in the first place, though. -- Ingo Weinhold
 */
#if defined(__BEOS__) || defined(__HAIKU__)
#    define WMUNGE(x) (((x) & 0xFF00) >> 8 | ((x) & 0x00FF) << 8)
#else
#    define WMUNGE(x) (x)
#endif

static int
not_here(const char *s)
{
    croak("POSIX::%s not implemented on this architecture", s);
    return -1;
}

#include "const-c.inc"

static void
restore_sigmask(pTHX_ SV *osset_sv)
{
     /* Fortunately, restoring the signal mask can't fail, because
      * there's nothing we can do about it if it does -- we're not
      * supposed to return -1 from sigaction unless the disposition
      * was unaffected.
      */
     sigset_t *ossetp = (sigset_t *) SvPV_nolen( osset_sv );
     (void)sigprocmask(SIG_SETMASK, ossetp, (sigset_t *)0);
}

#ifdef WIN32

/*
 * (1) The CRT maintains its own copy of the environment, separate from
 * the Win32API copy.
 *
 * (2) CRT getenv() retrieves from this copy. CRT putenv() updates this
 * copy, and then calls SetEnvironmentVariableA() to update the Win32API
 * copy.
 *
 * (3) win32_getenv() and win32_putenv() call GetEnvironmentVariableA() and
 * SetEnvironmentVariableA() directly, bypassing the CRT copy of the
 * environment.
 *
 * (4) The CRT strftime() "%Z" implementation calls __tzset(). That
 * calls CRT tzset(), but only the first time it is called, and in turn
 * that uses CRT getenv("TZ") to retrieve the timezone info from the CRT
 * local copy of the environment and hence gets the original setting as
 * perl never updates the CRT copy when assigning to $ENV{TZ}.
 *
 * Therefore, we need to retrieve the value of $ENV{TZ} and call CRT
 * putenv() to update the CRT copy of the environment (if it is different)
 * whenever we're about to call tzset().
 *
 * In addition to all that, when perl is built with PERL_IMPLICIT_SYS
 * defined:
 *
 * (a) Each interpreter has its own copy of the environment inside the
 * perlhost structure. That allows applications that host multiple
 * independent Perl interpreters to isolate environment changes from
 * each other. (This is similar to how the perlhost mechanism keeps a
 * separate working directory for each Perl interpreter, so that calling
 * chdir() will not affect other interpreters.)
 *
 * (b) Only the first Perl interpreter instantiated within a process will
 * "write through" environment changes to the process environment.
 *
 * (c) Even the primary Perl interpreter won't update the CRT copy of the
 * the environment, only the Win32API copy (it calls win32_putenv()).
 *
 * As with CPerlHost::Getenv() and CPerlHost::Putenv() themselves, it makes
 * sense to only update the process environment when inside the main
 * interpreter, but we don't have access to CPerlHost's m_bTopLevel member
 * from here so we'll just have to check PL_curinterp instead.
 *
 * Therefore, we can simply #undef getenv() and putenv() so that those names
 * always refer to the CRT functions, and explicitly call win32_getenv() to
 * access perl's %ENV.
 *
 * We also #undef malloc() and free() to be sure we are using the CRT
 * functions otherwise under PERL_IMPLICIT_SYS they are redefined to calls
 * into VMem::Malloc() and VMem::Free() and all allocations will be freed
 * when the Perl interpreter is being destroyed so we'd end up with a pointer
 * into deallocated memory in environ[] if a program embedding a Perl
 * interpreter continues to operate even after the main Perl interpreter has
 * been destroyed.
 *
 * Note that we don't free() the malloc()ed memory unless and until we call
 * malloc() again ourselves because the CRT putenv() function simply puts its
 * pointer argument into the environ[] array (it doesn't make a copy of it)
 * so this memory must otherwise be leaked.
 */

#undef getenv
#undef putenv
#undef malloc
#undef free

static void
fix_win32_tzenv(void)
{
    static char* oldenv = NULL;
    char* newenv;
    const char* perl_tz_env = win32_getenv("TZ");
    const char* crt_tz_env = getenv("TZ");
    if (perl_tz_env == NULL)
        perl_tz_env = "";
    if (crt_tz_env == NULL)
        crt_tz_env = "";
    if (strcmp(perl_tz_env, crt_tz_env) != 0) {
        newenv = (char*)malloc((strlen(perl_tz_env) + 4) * sizeof(char));
        if (newenv != NULL) {
            sprintf(newenv, "TZ=%s", perl_tz_env);
            putenv(newenv);
            if (oldenv != NULL)
                free(oldenv);
            oldenv = newenv;
        }
    }
}

#endif

/*
 * my_tzset - wrapper to tzset() with a fix to make it work (better) on Win32.
 * This code is duplicated in the Time-Piece module, so any changes made here
 * should be made there too.
 */
static void
my_tzset(pTHX)
{
#ifdef WIN32
#if defined(USE_ITHREADS) && defined(PERL_IMPLICIT_SYS)
    if (PL_curinterp == aTHX)
#endif
        fix_win32_tzenv();
#endif
    tzset();
}

MODULE = SigSet		PACKAGE = POSIX::SigSet		PREFIX = sig

POSIX::SigSet
new(packname = "POSIX::SigSet", ...)
    const char *	packname
    CODE:
	{
	    int i;
	    Newx(RETVAL, 1, sigset_t);
	    sigemptyset(RETVAL);
	    for (i = 1; i < items; i++)
		sigaddset(RETVAL, SvIV(ST(i)));
	}
    OUTPUT:
	RETVAL

void
DESTROY(sigset)
	POSIX::SigSet	sigset
    CODE:
	Safefree(sigset);

SysRet
sigaddset(sigset, sig)
	POSIX::SigSet	sigset
	int		sig

SysRet
sigdelset(sigset, sig)
	POSIX::SigSet	sigset
	int		sig

SysRet
sigemptyset(sigset)
	POSIX::SigSet	sigset

SysRet
sigfillset(sigset)
	POSIX::SigSet	sigset

int
sigismember(sigset, sig)
	POSIX::SigSet	sigset
	int		sig

MODULE = Termios	PACKAGE = POSIX::Termios	PREFIX = cf

POSIX::Termios
new(packname = "POSIX::Termios", ...)
    const char *	packname
    CODE:
	{
#ifdef I_TERMIOS
	    Newx(RETVAL, 1, struct termios);
#else
	    not_here("termios");
        RETVAL = 0;
#endif
	}
    OUTPUT:
	RETVAL

void
DESTROY(termios_ref)
	POSIX::Termios	termios_ref
    CODE:
#ifdef I_TERMIOS
	Safefree(termios_ref);
#else
	    not_here("termios");
#endif

SysRet
getattr(termios_ref, fd = 0)
	POSIX::Termios	termios_ref
	int		fd
    CODE:
	RETVAL = tcgetattr(fd, termios_ref);
    OUTPUT:
	RETVAL

SysRet
setattr(termios_ref, fd = 0, optional_actions = 0)
	POSIX::Termios	termios_ref
	int		fd
	int		optional_actions
    CODE:
	RETVAL = tcsetattr(fd, optional_actions, termios_ref);
    OUTPUT:
	RETVAL

speed_t
cfgetispeed(termios_ref)
	POSIX::Termios	termios_ref

speed_t
cfgetospeed(termios_ref)
	POSIX::Termios	termios_ref

tcflag_t
getiflag(termios_ref)
	POSIX::Termios	termios_ref
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	RETVAL = termios_ref->c_iflag;
#else
     not_here("getiflag");
     RETVAL = 0;
#endif
    OUTPUT:
	RETVAL

tcflag_t
getoflag(termios_ref)
	POSIX::Termios	termios_ref
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	RETVAL = termios_ref->c_oflag;
#else
     not_here("getoflag");
     RETVAL = 0;
#endif
    OUTPUT:
	RETVAL

tcflag_t
getcflag(termios_ref)
	POSIX::Termios	termios_ref
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	RETVAL = termios_ref->c_cflag;
#else
     not_here("getcflag");
     RETVAL = 0;
#endif
    OUTPUT:
	RETVAL

tcflag_t
getlflag(termios_ref)
	POSIX::Termios	termios_ref
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	RETVAL = termios_ref->c_lflag;
#else
     not_here("getlflag");
     RETVAL = 0;
#endif
    OUTPUT:
	RETVAL

cc_t
getcc(termios_ref, ccix)
	POSIX::Termios	termios_ref
	unsigned int	ccix
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	if (ccix >= NCCS)
	    croak("Bad getcc subscript");
	RETVAL = termios_ref->c_cc[ccix];
#else
     not_here("getcc");
     RETVAL = 0;
#endif
    OUTPUT:
	RETVAL

SysRet
cfsetispeed(termios_ref, speed)
	POSIX::Termios	termios_ref
	speed_t		speed

SysRet
cfsetospeed(termios_ref, speed)
	POSIX::Termios	termios_ref
	speed_t		speed

void
setiflag(termios_ref, iflag)
	POSIX::Termios	termios_ref
	tcflag_t	iflag
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	termios_ref->c_iflag = iflag;
#else
	    not_here("setiflag");
#endif

void
setoflag(termios_ref, oflag)
	POSIX::Termios	termios_ref
	tcflag_t	oflag
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	termios_ref->c_oflag = oflag;
#else
	    not_here("setoflag");
#endif

void
setcflag(termios_ref, cflag)
	POSIX::Termios	termios_ref
	tcflag_t	cflag
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	termios_ref->c_cflag = cflag;
#else
	    not_here("setcflag");
#endif

void
setlflag(termios_ref, lflag)
	POSIX::Termios	termios_ref
	tcflag_t	lflag
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	termios_ref->c_lflag = lflag;
#else
	    not_here("setlflag");
#endif

void
setcc(termios_ref, ccix, cc)
	POSIX::Termios	termios_ref
	unsigned int	ccix
	cc_t		cc
    CODE:
#ifdef I_TERMIOS /* References a termios structure member so ifdef it out. */
	if (ccix >= NCCS)
	    croak("Bad setcc subscript");
	termios_ref->c_cc[ccix] = cc;
#else
	    not_here("setcc");
#endif


MODULE = POSIX		PACKAGE = POSIX

INCLUDE: const-xs.inc

int
WEXITSTATUS(status)
	int status
    ALIAS:
	POSIX::WIFEXITED = 1
	POSIX::WIFSIGNALED = 2
	POSIX::WIFSTOPPED = 3
	POSIX::WSTOPSIG = 4
	POSIX::WTERMSIG = 5
    CODE:
#if !defined(WEXITSTATUS) || !defined(WIFEXITED) || !defined(WIFSIGNALED) \
      || !defined(WIFSTOPPED) || !defined(WSTOPSIG) || !defined(WTERMSIG)
        RETVAL = 0; /* Silence compilers that notice this, but don't realise
		       that not_here() can't return.  */
#endif
	switch(ix) {
	case 0:
#ifdef WEXITSTATUS
	    RETVAL = WEXITSTATUS(WMUNGE(status));
#else
	    not_here("WEXITSTATUS");
#endif
	    break;
	case 1:
#ifdef WIFEXITED
	    RETVAL = WIFEXITED(WMUNGE(status));
#else
	    not_here("WIFEXITED");
#endif
	    break;
	case 2:
#ifdef WIFSIGNALED
	    RETVAL = WIFSIGNALED(WMUNGE(status));
#else
	    not_here("WIFSIGNALED");
#endif
	    break;
	case 3:
#ifdef WIFSTOPPED
	    RETVAL = WIFSTOPPED(WMUNGE(status));
#else
	    not_here("WIFSTOPPED");
#endif
	    break;
	case 4:
#ifdef WSTOPSIG
	    RETVAL = WSTOPSIG(WMUNGE(status));
#else
	    not_here("WSTOPSIG");
#endif
	    break;
	case 5:
#ifdef WTERMSIG
	    RETVAL = WTERMSIG(WMUNGE(status));
#else
	    not_here("WTERMSIG");
#endif
	    break;
	default:
	    Perl_croak(aTHX_ "Illegal alias %d for POSIX::W*", (int)ix);
	}
    OUTPUT:
	RETVAL

int
isalnum(charstring)
	SV *	charstring
    PREINIT:
	STRLEN	len;
    CODE:
	unsigned char *s = (unsigned char *) SvPV(charstring, len);
	unsigned char *e = s + len;
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!isalnum(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
isalpha(charstring)
	SV *	charstring
    PREINIT:
	STRLEN	len;
    CODE:
	unsigned char *s = (unsigned char *) SvPV(charstring, len);
	unsigned char *e = s + len;
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!isalpha(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
iscntrl(charstring)
	SV *	charstring
    PREINIT:
	STRLEN	len;
    CODE:
	unsigned char *s = (unsigned char *) SvPV(charstring, len);
	unsigned char *e = s + len;
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!iscntrl(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
isdigit(charstring)
	SV *	charstring
    PREINIT:
	STRLEN	len;
    CODE:
	unsigned char *s = (unsigned char *) SvPV(charstring, len);
	unsigned char *e = s + len;
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!isdigit(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
isgraph(charstring)
	SV *	charstring
    PREINIT:
	STRLEN	len;
    CODE:
	unsigned char *s = (unsigned char *) SvPV(charstring, len);
	unsigned char *e = s + len;
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!isgraph(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
islower(charstring)
	SV *	charstring
    PREINIT:
	STRLEN	len;
    CODE:
	unsigned char *s = (unsigned char *) SvPV(charstring, len);
	unsigned char *e = s + len;
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!islower(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
isprint(charstring)
	SV *	charstring
    PREINIT:
	STRLEN	len;
    CODE:
	unsigned char *s = (unsigned char *) SvPV(charstring, len);
	unsigned char *e = s + len;
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!isprint(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
ispunct(charstring)
	SV *	charstring
    PREINIT:
	STRLEN	len;
    CODE:
	unsigned char *s = (unsigned char *) SvPV(charstring, len);
	unsigned char *e = s + len;
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!ispunct(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
isspace(charstring)
	SV *	charstring
    PREINIT:
	STRLEN	len;
    CODE:
	unsigned char *s = (unsigned char *) SvPV(charstring, len);
	unsigned char *e = s + len;
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!isspace(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
isupper(charstring)
	SV *	charstring
    PREINIT:
	STRLEN	len;
    CODE:
	unsigned char *s = (unsigned char *) SvPV(charstring, len);
	unsigned char *e = s + len;
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!isupper(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

int
isxdigit(charstring)
	SV *	charstring
    PREINIT:
	STRLEN	len;
    CODE:
	unsigned char *s = (unsigned char *) SvPV(charstring, len);
	unsigned char *e = s + len;
	for (RETVAL = 1; RETVAL && s < e; s++)
	    if (!isxdigit(*s))
		RETVAL = 0;
    OUTPUT:
	RETVAL

SysRet
open(filename, flags = O_RDONLY, mode = 0666)
	char *		filename
	int		flags
	Mode_t		mode
    CODE:
	if (flags & (O_APPEND|O_CREAT|O_TRUNC|O_RDWR|O_WRONLY|O_EXCL))
	    TAINT_PROPER("open");
	RETVAL = open(filename, flags, mode);
    OUTPUT:
	RETVAL


HV *
localeconv()
    CODE:
#ifdef HAS_LOCALECONV
	struct lconv *lcbuf;
	RETVAL = newHV();
	sv_2mortal((SV*)RETVAL);
	if ((lcbuf = localeconv())) {
	    /* the strings */
	    if (lcbuf->decimal_point && *lcbuf->decimal_point)
		(void) hv_store(RETVAL, "decimal_point", 13,
		    newSVpv(lcbuf->decimal_point, 0), 0);
	    if (lcbuf->thousands_sep && *lcbuf->thousands_sep)
		(void) hv_store(RETVAL, "thousands_sep", 13,
		    newSVpv(lcbuf->thousands_sep, 0), 0);
#ifndef NO_LOCALECONV_GROUPING
	    if (lcbuf->grouping && *lcbuf->grouping)
		(void) hv_store(RETVAL, "grouping", 8,
		    newSVpv(lcbuf->grouping, 0), 0);
#endif
	    if (lcbuf->int_curr_symbol && *lcbuf->int_curr_symbol)
		(void) hv_store(RETVAL, "int_curr_symbol", 15,
		    newSVpv(lcbuf->int_curr_symbol, 0), 0);
	    if (lcbuf->currency_symbol && *lcbuf->currency_symbol)
		(void) hv_store(RETVAL, "currency_symbol", 15,
		    newSVpv(lcbuf->currency_symbol, 0), 0);
	    if (lcbuf->mon_decimal_point && *lcbuf->mon_decimal_point)
		(void) hv_store(RETVAL, "mon_decimal_point", 17,
		    newSVpv(lcbuf->mon_decimal_point, 0), 0);
#ifndef NO_LOCALECONV_MON_THOUSANDS_SEP
	    if (lcbuf->mon_thousands_sep && *lcbuf->mon_thousands_sep)
		(void) hv_store(RETVAL, "mon_thousands_sep", 17,
		    newSVpv(lcbuf->mon_thousands_sep, 0), 0);
#endif
#ifndef NO_LOCALECONV_MON_GROUPING
	    if (lcbuf->mon_grouping && *lcbuf->mon_grouping)
		(void) hv_store(RETVAL, "mon_grouping", 12,
		    newSVpv(lcbuf->mon_grouping, 0), 0);
#endif
	    if (lcbuf->positive_sign && *lcbuf->positive_sign)
		(void) hv_store(RETVAL, "positive_sign", 13,
		    newSVpv(lcbuf->positive_sign, 0), 0);
	    if (lcbuf->negative_sign && *lcbuf->negative_sign)
		(void) hv_store(RETVAL, "negative_sign", 13,
		    newSVpv(lcbuf->negative_sign, 0), 0);
	    /* the integers */
	    if (lcbuf->int_frac_digits != CHAR_MAX)
		(void) hv_store(RETVAL, "int_frac_digits", 15,
		    newSViv(lcbuf->int_frac_digits), 0);
	    if (lcbuf->frac_digits != CHAR_MAX)
		(void) hv_store(RETVAL, "frac_digits", 11,
		    newSViv(lcbuf->frac_digits), 0);
	    if (lcbuf->p_cs_precedes != CHAR_MAX)
		(void) hv_store(RETVAL, "p_cs_precedes", 13,
		    newSViv(lcbuf->p_cs_precedes), 0);
	    if (lcbuf->p_sep_by_space != CHAR_MAX)
		(void) hv_store(RETVAL, "p_sep_by_space", 14,
		    newSViv(lcbuf->p_sep_by_space), 0);
	    if (lcbuf->n_cs_precedes != CHAR_MAX)
		(void) hv_store(RETVAL, "n_cs_precedes", 13,
		    newSViv(lcbuf->n_cs_precedes), 0);
	    if (lcbuf->n_sep_by_space != CHAR_MAX)
		(void) hv_store(RETVAL, "n_sep_by_space", 14,
		    newSViv(lcbuf->n_sep_by_space), 0);
	    if (lcbuf->p_sign_posn != CHAR_MAX)
		(void) hv_store(RETVAL, "p_sign_posn", 11,
		    newSViv(lcbuf->p_sign_posn), 0);
	    if (lcbuf->n_sign_posn != CHAR_MAX)
		(void) hv_store(RETVAL, "n_sign_posn", 11,
		    newSViv(lcbuf->n_sign_posn), 0);
	}
#else
	localeconv(); /* A stub to call not_here(). */
#endif
    OUTPUT:
	RETVAL

char *
setlocale(category, locale = 0)
	int		category
	char *		locale
    PREINIT:
	char *		retval;
    CODE:
	retval = setlocale(category, locale);
	if (retval) {
	    /* Save retval since subsequent setlocale() calls
	     * may overwrite it. */
	    RETVAL = savepv(retval);
#ifdef USE_LOCALE_CTYPE
	    if (category == LC_CTYPE
#ifdef LC_ALL
		|| category == LC_ALL
#endif
		)
	    {
		char *newctype;
#ifdef LC_ALL
		if (category == LC_ALL)
		    newctype = setlocale(LC_CTYPE, NULL);
		else
#endif
		    newctype = RETVAL;
		new_ctype(newctype);
	    }
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
	    if (category == LC_COLLATE
#ifdef LC_ALL
		|| category == LC_ALL
#endif
		)
	    {
		char *newcoll;
#ifdef LC_ALL
		if (category == LC_ALL)
		    newcoll = setlocale(LC_COLLATE, NULL);
		else
#endif
		    newcoll = RETVAL;
		new_collate(newcoll);
	    }
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
	    if (category == LC_NUMERIC
#ifdef LC_ALL
		|| category == LC_ALL
#endif
		)
	    {
		char *newnum;
#ifdef LC_ALL
		if (category == LC_ALL)
		    newnum = setlocale(LC_NUMERIC, NULL);
		else
#endif
		    newnum = RETVAL;
		new_numeric(newnum);
	    }
#endif /* USE_LOCALE_NUMERIC */
	}
	else
	    RETVAL = NULL;
    OUTPUT:
	RETVAL
    CLEANUP:
        if (RETVAL)
	    Safefree(RETVAL);

NV
acos(x)
	NV		x

NV
asin(x)
	NV		x

NV
atan(x)
	NV		x

NV
ceil(x)
	NV		x

NV
cosh(x)
	NV		x

NV
floor(x)
	NV		x

NV
fmod(x,y)
	NV		x
	NV		y

void
frexp(x)
	NV		x
    PPCODE:
	int expvar;
	/* (We already know stack is long enough.) */
	PUSHs(sv_2mortal(newSVnv(frexp(x,&expvar))));
	PUSHs(sv_2mortal(newSViv(expvar)));

NV
ldexp(x,exp)
	NV		x
	int		exp

NV
log10(x)
	NV		x

void
modf(x)
	NV		x
    PPCODE:
	NV intvar;
	/* (We already know stack is long enough.) */
	PUSHs(sv_2mortal(newSVnv(Perl_modf(x,&intvar))));
	PUSHs(sv_2mortal(newSVnv(intvar)));

NV
sinh(x)
	NV		x

NV
tan(x)
	NV		x

NV
tanh(x)
	NV		x

SysRet
sigaction(sig, optaction, oldaction = 0)
	int			sig
	SV *			optaction
	POSIX::SigAction	oldaction
    CODE:
#if defined(WIN32) || defined(NETWARE)
	RETVAL = not_here("sigaction");
#else
# This code is really grody because we're trying to make the signal
# interface look beautiful, which is hard.

	{
	    dVAR;
	    POSIX__SigAction action;
	    GV *siggv = gv_fetchpvs("SIG", GV_ADD, SVt_PVHV);
	    struct sigaction act;
	    struct sigaction oact;
	    sigset_t sset;
	    SV *osset_sv;
	    sigset_t osset;
	    POSIX__SigSet sigset;
	    SV** svp;
	    SV** sigsvp;

            if (sig < 0) {
                croak("Negative signals are not allowed");
            }

	    if (sig == 0 && SvPOK(ST(0))) {
	        const char *s = SvPVX_const(ST(0));
		int i = whichsig(s);

	        if (i < 0 && memEQ(s, "SIG", 3))
		    i = whichsig(s + 3);
	        if (i < 0) {
	            if (ckWARN(WARN_SIGNAL))
		        Perl_warner(aTHX_ packWARN(WARN_SIGNAL),
                                    "No such signal: SIG%s", s);
	            XSRETURN_UNDEF;
		}
	        else
		    sig = i;
            }
#ifdef NSIG
	    if (sig > NSIG) { /* NSIG - 1 is still okay. */
	        Perl_warner(aTHX_ packWARN(WARN_SIGNAL),
                            "No such signal: %d", sig);
	        XSRETURN_UNDEF;
	    }
#endif
	    sigsvp = hv_fetch(GvHVn(siggv),
			      PL_sig_name[sig],
			      strlen(PL_sig_name[sig]),
			      TRUE);

	    /* Check optaction and set action */
	    if(SvTRUE(optaction)) {
		if(sv_isa(optaction, "POSIX::SigAction"))
			action = (HV*)SvRV(optaction);
		else
			croak("action is not of type POSIX::SigAction");
	    }
	    else {
		action=0;
	    }

	    /* sigaction() is supposed to look atomic. In particular, any
	     * signal handler invoked during a sigaction() call should
	     * see either the old or the new disposition, and not something
	     * in between. We use sigprocmask() to make it so.
	     */
	    sigfillset(&sset);
	    RETVAL=sigprocmask(SIG_BLOCK, &sset, &osset);
	    if(RETVAL == -1)
               XSRETURN_UNDEF;
	    ENTER;
	    /* Restore signal mask no matter how we exit this block. */
	    osset_sv = newSVpvn((char *)(&osset), sizeof(sigset_t));
	    SAVEFREESV( osset_sv );
	    SAVEDESTRUCTOR_X(restore_sigmask, osset_sv);

	    RETVAL=-1; /* In case both oldaction and action are 0. */

	    /* Remember old disposition if desired. */
	    if (oldaction) {
		svp = hv_fetchs(oldaction, "HANDLER", TRUE);
		if(!svp)
		    croak("Can't supply an oldaction without a HANDLER");
		if(SvTRUE(*sigsvp)) { /* TBD: what if "0"? */
			sv_setsv(*svp, *sigsvp);
		}
		else {
			sv_setpvs(*svp, "DEFAULT");
		}
		RETVAL = sigaction(sig, (struct sigaction *)0, & oact);
		if(RETVAL == -1) {
                   LEAVE;
                   XSRETURN_UNDEF;
                }
		/* Get back the mask. */
		svp = hv_fetchs(oldaction, "MASK", TRUE);
		if (sv_isa(*svp, "POSIX::SigSet")) {
		    IV tmp = SvIV((SV*)SvRV(*svp));
		    sigset = INT2PTR(sigset_t*, tmp);
		}
		else {
		    Newx(sigset, 1, sigset_t);
		    sv_setptrobj(*svp, sigset, "POSIX::SigSet");
		}
		*sigset = oact.sa_mask;

		/* Get back the flags. */
		svp = hv_fetchs(oldaction, "FLAGS", TRUE);
		sv_setiv(*svp, oact.sa_flags);

		/* Get back whether the old handler used safe signals. */
		svp = hv_fetchs(oldaction, "SAFE", TRUE);
		sv_setiv(*svp,
		/* compare incompatible pointers by casting to integer */
		    PTR2nat(oact.sa_handler) == PTR2nat(PL_csighandlerp));
	    }

	    if (action) {
		/* Safe signals use "csighandler", which vectors through the
		   PL_sighandlerp pointer when it's safe to do so.
		   (BTW, "csighandler" is very different from "sighandler".) */
		svp = hv_fetchs(action, "SAFE", FALSE);
		act.sa_handler =
			DPTR2FPTR(
			    void (*)(int),
			    (*svp && SvTRUE(*svp))
				? PL_csighandlerp : PL_sighandlerp
			);

		/* Vector new Perl handler through %SIG.
		   (The core signal handlers read %SIG to dispatch.) */
		svp = hv_fetchs(action, "HANDLER", FALSE);
		if (!svp)
		    croak("Can't supply an action without a HANDLER");
		sv_setsv(*sigsvp, *svp);

		/* This call actually calls sigaction() with almost the
		   right settings, including appropriate interpretation
		   of DEFAULT and IGNORE.  However, why are we doing
		   this when we're about to do it again just below?  XXX */
		SvSETMAGIC(*sigsvp);

		/* And here again we duplicate -- DEFAULT/IGNORE checking. */
		if(SvPOK(*svp)) {
			const char *s=SvPVX_const(*svp);
			if(strEQ(s,"IGNORE")) {
				act.sa_handler = SIG_IGN;
			}
			else if(strEQ(s,"DEFAULT")) {
				act.sa_handler = SIG_DFL;
			}
		}

		/* Set up any desired mask. */
		svp = hv_fetchs(action, "MASK", FALSE);
		if (svp && sv_isa(*svp, "POSIX::SigSet")) {
		    IV tmp = SvIV((SV*)SvRV(*svp));
		    sigset = INT2PTR(sigset_t*, tmp);
		    act.sa_mask = *sigset;
		}
		else
		    sigemptyset(& act.sa_mask);

		/* Set up any desired flags. */
		svp = hv_fetchs(action, "FLAGS", FALSE);
		act.sa_flags = svp ? SvIV(*svp) : 0;

		/* Don't worry about cleaning up *sigsvp if this fails,
		 * because that means we tried to disposition a
		 * nonblockable signal, in which case *sigsvp is
		 * essentially meaningless anyway.
		 */
		RETVAL = sigaction(sig, & act, (struct sigaction *)0);
		if(RETVAL == -1) {
                    LEAVE;
		    XSRETURN_UNDEF;
                }
	    }

	    LEAVE;
	}
#endif
    OUTPUT:
	RETVAL

SysRet
sigpending(sigset)
	POSIX::SigSet		sigset

SysRet
sigprocmask(how, sigset, oldsigset = 0)
	int			how
	POSIX::SigSet		sigset = NO_INIT
	POSIX::SigSet		oldsigset = NO_INIT
INIT:
	if (! SvOK(ST(1))) {
	    sigset = NULL;
	} else if (sv_isa(ST(1), "POSIX::SigSet")) {
	    IV tmp = SvIV((SV*)SvRV(ST(1)));
	    sigset = INT2PTR(POSIX__SigSet,tmp);
	} else {
	    croak("sigset is not of type POSIX::SigSet");
	}

	if (items < 3 || ! SvOK(ST(2))) {
	    oldsigset = NULL;
	} else if (sv_isa(ST(2), "POSIX::SigSet")) {
	    IV tmp = SvIV((SV*)SvRV(ST(2)));
	    oldsigset = INT2PTR(POSIX__SigSet,tmp);
	} else {
	    croak("oldsigset is not of type POSIX::SigSet");
	}

SysRet
sigsuspend(signal_mask)
	POSIX::SigSet		signal_mask

void
_exit(status)
	int		status

SysRet
close(fd)
	int		fd

SysRet
dup(fd)
	int		fd

SysRet
dup2(fd1, fd2)
	int		fd1
	int		fd2

SV *
lseek(fd, offset, whence)
	int		fd
	Off_t		offset
	int		whence
    CODE:
	Off_t pos = PerlLIO_lseek(fd, offset, whence);
	RETVAL = sizeof(Off_t) > sizeof(IV)
		 ? newSVnv((NV)pos) : newSViv((IV)pos);
    OUTPUT:
	RETVAL

void
nice(incr)
	int		incr
    PPCODE:
	errno = 0;
	if ((incr = nice(incr)) != -1 || errno == 0) {
	    if (incr == 0)
		XPUSHs(newSVpvs_flags("0 but true", SVs_TEMP));
	    else
		XPUSHs(sv_2mortal(newSViv(incr)));
	}

void
pipe()
    PPCODE:
	int fds[2];
	if (pipe(fds) != -1) {
	    EXTEND(SP,2);
	    PUSHs(sv_2mortal(newSViv(fds[0])));
	    PUSHs(sv_2mortal(newSViv(fds[1])));
	}

SysRet
read(fd, buffer, nbytes)
    PREINIT:
        SV *sv_buffer = SvROK(ST(1)) ? SvRV(ST(1)) : ST(1);
    INPUT:
        int             fd
        size_t          nbytes
        char *          buffer = sv_grow( sv_buffer, nbytes+1 );
    CLEANUP:
        if (RETVAL >= 0) {
            SvCUR_set(sv_buffer, RETVAL);
            SvPOK_only(sv_buffer);
            *SvEND(sv_buffer) = '\0';
            SvTAINTED_on(sv_buffer);
        }

SysRet
setpgid(pid, pgid)
	pid_t		pid
	pid_t		pgid

pid_t
setsid()

pid_t
tcgetpgrp(fd)
	int		fd

SysRet
tcsetpgrp(fd, pgrp_id)
	int		fd
	pid_t		pgrp_id

void
uname()
    PPCODE:
#ifdef HAS_UNAME
	struct utsname buf;
	if (uname(&buf) >= 0) {
	    EXTEND(SP, 5);
	    PUSHs(newSVpvn_flags(buf.sysname, strlen(buf.sysname), SVs_TEMP));
	    PUSHs(newSVpvn_flags(buf.nodename, strlen(buf.nodename), SVs_TEMP));
	    PUSHs(newSVpvn_flags(buf.release, strlen(buf.release), SVs_TEMP));
	    PUSHs(newSVpvn_flags(buf.version, strlen(buf.version), SVs_TEMP));
	    PUSHs(newSVpvn_flags(buf.machine, strlen(buf.machine), SVs_TEMP));
	}
#else
	uname((char *) 0); /* A stub to call not_here(). */
#endif

SysRet
write(fd, buffer, nbytes)
	int		fd
	char *		buffer
	size_t		nbytes

SV *
tmpnam()
    PREINIT:
	STRLEN i;
	int len;
    CODE:
	RETVAL = newSVpvn("", 0);
	SvGROW(RETVAL, L_tmpnam);
	len = strlen(tmpnam(SvPV(RETVAL, i)));
	SvCUR_set(RETVAL, len);
    OUTPUT:
	RETVAL

void
abort()

int
mblen(s, n)
	char *		s
	size_t		n

size_t
mbstowcs(s, pwcs, n)
	wchar_t *	s
	char *		pwcs
	size_t		n

int
mbtowc(pwc, s, n)
	wchar_t *	pwc
	char *		s
	size_t		n

int
wcstombs(s, pwcs, n)
	char *		s
	wchar_t *	pwcs
	size_t		n

int
wctomb(s, wchar)
	char *		s
	wchar_t		wchar

int
strcoll(s1, s2)
	char *		s1
	char *		s2

void
strtod(str)
	char *		str
    PREINIT:
	double num;
	char *unparsed;
    PPCODE:
	SET_NUMERIC_LOCAL();
	num = strtod(str, &unparsed);
	PUSHs(sv_2mortal(newSVnv(num)));
	if (GIMME == G_ARRAY) {
	    EXTEND(SP, 1);
	    if (unparsed)
		PUSHs(sv_2mortal(newSViv(strlen(unparsed))));
	    else
		PUSHs(&PL_sv_undef);
	}

void
strtol(str, base = 0)
	char *		str
	int		base
    PREINIT:
	long num;
	char *unparsed;
    PPCODE:
	num = strtol(str, &unparsed, base);
#if IVSIZE <= LONGSIZE
	if (num < IV_MIN || num > IV_MAX)
	    PUSHs(sv_2mortal(newSVnv((double)num)));
	else
#endif
	    PUSHs(sv_2mortal(newSViv((IV)num)));
	if (GIMME == G_ARRAY) {
	    EXTEND(SP, 1);
	    if (unparsed)
		PUSHs(sv_2mortal(newSViv(strlen(unparsed))));
	    else
		PUSHs(&PL_sv_undef);
	}

void
strtoul(str, base = 0)
	const char *	str
	int		base
    PREINIT:
	unsigned long num;
	char *unparsed;
    PPCODE:
	num = strtoul(str, &unparsed, base);
#if IVSIZE <= LONGSIZE
	if (num > IV_MAX)
	    PUSHs(sv_2mortal(newSVnv((double)num)));
	else
#endif
	    PUSHs(sv_2mortal(newSViv((IV)num)));
	if (GIMME == G_ARRAY) {
	    EXTEND(SP, 1);
	    if (unparsed)
		PUSHs(sv_2mortal(newSViv(strlen(unparsed))));
	    else
		PUSHs(&PL_sv_undef);
	}

void
strxfrm(src)
	SV *		src
    CODE:
	{
          STRLEN srclen;
          STRLEN dstlen;
          char *p = SvPV(src,srclen);
          srclen++;
          ST(0) = sv_2mortal(newSV(srclen*4+1));
          dstlen = strxfrm(SvPVX(ST(0)), p, (size_t)srclen);
          if (dstlen > srclen) {
              dstlen++;
              SvGROW(ST(0), dstlen);
              strxfrm(SvPVX(ST(0)), p, (size_t)dstlen);
              dstlen--;
          }
          SvCUR_set(ST(0), dstlen);
	    SvPOK_only(ST(0));
	}

SysRet
mkfifo(filename, mode)
	char *		filename
	Mode_t		mode
    CODE:
	TAINT_PROPER("mkfifo");
	RETVAL = mkfifo(filename, mode);
    OUTPUT:
	RETVAL

SysRet
tcdrain(fd)
	int		fd


SysRet
tcflow(fd, action)
	int		fd
	int		action


SysRet
tcflush(fd, queue_selector)
	int		fd
	int		queue_selector

SysRet
tcsendbreak(fd, duration)
	int		fd
	int		duration

char *
asctime(sec, min, hour, mday, mon, year, wday = 0, yday = 0, isdst = -1)
	int		sec
	int		min
	int		hour
	int		mday
	int		mon
	int		year
	int		wday
	int		yday
	int		isdst
    CODE:
	{
	    struct tm mytm;
	    init_tm(&mytm);	/* XXX workaround - see init_tm() above */
	    mytm.tm_sec = sec;
	    mytm.tm_min = min;
	    mytm.tm_hour = hour;
	    mytm.tm_mday = mday;
	    mytm.tm_mon = mon;
	    mytm.tm_year = year;
	    mytm.tm_wday = wday;
	    mytm.tm_yday = yday;
	    mytm.tm_isdst = isdst;
	    RETVAL = asctime(&mytm);
	}
    OUTPUT:
	RETVAL

long
clock()

char *
ctime(time)
	Time_t		&time

void
times()
	PPCODE:
	struct tms tms;
	clock_t realtime;
	realtime = times( &tms );
	EXTEND(SP,5);
	PUSHs( sv_2mortal( newSViv( (IV) realtime ) ) );
	PUSHs( sv_2mortal( newSViv( (IV) tms.tms_utime ) ) );
	PUSHs( sv_2mortal( newSViv( (IV) tms.tms_stime ) ) );
	PUSHs( sv_2mortal( newSViv( (IV) tms.tms_cutime ) ) );
	PUSHs( sv_2mortal( newSViv( (IV) tms.tms_cstime ) ) );

double
difftime(time1, time2)
	Time_t		time1
	Time_t		time2

SysRetLong
mktime(sec, min, hour, mday, mon, year, wday = 0, yday = 0, isdst = -1)
	int		sec
	int		min
	int		hour
	int		mday
	int		mon
	int		year
	int		wday
	int		yday
	int		isdst
    CODE:
	{
	    struct tm mytm;
	    init_tm(&mytm);	/* XXX workaround - see init_tm() above */
	    mytm.tm_sec = sec;
	    mytm.tm_min = min;
	    mytm.tm_hour = hour;
	    mytm.tm_mday = mday;
	    mytm.tm_mon = mon;
	    mytm.tm_year = year;
	    mytm.tm_wday = wday;
	    mytm.tm_yday = yday;
	    mytm.tm_isdst = isdst;
	    RETVAL = (SysRetLong) mktime(&mytm);
	}
    OUTPUT:
	RETVAL

#XXX: if $xsubpp::WantOptimize is always the default
#     sv_setpv(TARG, ...) could be used rather than
#     ST(0) = sv_2mortal(newSVpv(...))
void
strftime(fmt, sec, min, hour, mday, mon, year, wday = -1, yday = -1, isdst = -1)
	SV *		fmt
	int		sec
	int		min
	int		hour
	int		mday
	int		mon
	int		year
	int		wday
	int		yday
	int		isdst
    CODE:
	{
	    char *buf = my_strftime(SvPV_nolen(fmt), sec, min, hour, mday, mon, year, wday, yday, isdst);
	    if (buf) {
		SV *const sv = sv_newmortal();
		sv_usepvn_flags(sv, buf, strlen(buf), SV_HAS_TRAILING_NUL);
		if (SvUTF8(fmt)) {
		    SvUTF8_on(sv);
		}
		ST(0) = sv;
	    }
	}

void
tzset()
  PPCODE:
    my_tzset(aTHX);

void
tzname()
    PPCODE:
	EXTEND(SP,2);
	PUSHs(newSVpvn_flags(tzname[0], strlen(tzname[0]), SVs_TEMP));
	PUSHs(newSVpvn_flags(tzname[1], strlen(tzname[1]), SVs_TEMP));

SysRet
access(filename, mode)
	char *		filename
	Mode_t		mode

char *
ctermid(s = 0)
	char *          s = 0;
    CODE:
#ifdef HAS_CTERMID_R
	s = (char *) safemalloc((size_t) L_ctermid);
#endif
	RETVAL = ctermid(s);
    OUTPUT:
	RETVAL
    CLEANUP:
#ifdef HAS_CTERMID_R
	Safefree(s);
#endif

char *
cuserid(s = 0)
	char *		s = 0;
    CODE:
#ifdef HAS_CUSERID
  RETVAL = cuserid(s);
#else
  RETVAL = 0;
  not_here("cuserid");
#endif
    OUTPUT:
  RETVAL

SysRetLong
fpathconf(fd, name)
	int		fd
	int		name

SysRetLong
pathconf(filename, name)
	char *		filename
	int		name

SysRet
pause()

SysRet
setgid(gid)
	Gid_t		gid
    CLEANUP:
#ifndef WIN32
	if (RETVAL >= 0) {
	    PL_gid  = getgid();
	    PL_egid = getegid();
	}
#endif

SysRet
setuid(uid)
	Uid_t		uid
    CLEANUP:
#ifndef WIN32
	if (RETVAL >= 0) {
	    PL_uid  = getuid();
	    PL_euid = geteuid();
	}
#endif

SysRetLong
sysconf(name)
	int		name

char *
ttyname(fd)
	int		fd

void
getcwd()
    PPCODE:
      {
	dXSTARG;
	getcwd_sv(TARG);
	XSprePUSH; PUSHTARG;
      }

SysRet
lchown(uid, gid, path)
       Uid_t           uid
       Gid_t           gid
       char *          path
    CODE:
#ifdef HAS_LCHOWN
       /* yes, the order of arguments is different,
        * but consistent with CORE::chown() */
       RETVAL = lchown(path, uid, gid);
#else
       RETVAL = not_here("lchown");
#endif
    OUTPUT:
       RETVAL
