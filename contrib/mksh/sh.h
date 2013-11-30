/*	$OpenBSD: sh.h,v 1.29 2005/12/11 18:53:51 deraadt Exp $	*/
/*	$OpenBSD: shf.h,v 1.6 2005/12/11 18:53:51 deraadt Exp $	*/
/*	$OpenBSD: table.h,v 1.7 2005/12/11 20:31:21 otto Exp $	*/
/*	$OpenBSD: tree.h,v 1.10 2005/03/28 21:28:22 deraadt Exp $	*/
/*	$OpenBSD: expand.h,v 1.6 2005/03/30 17:16:37 deraadt Exp $	*/
/*	$OpenBSD: lex.h,v 1.11 2006/05/29 18:22:24 otto Exp $	*/
/*	$OpenBSD: proto.h,v 1.30 2006/03/17 16:30:13 millert Exp $	*/
/*	$OpenBSD: c_test.h,v 1.4 2004/12/20 11:34:26 otto Exp $	*/
/*	$OpenBSD: tty.h,v 1.5 2004/12/20 11:34:26 otto Exp $	*/

#define MKSH_SH_H_ID "$MirOS: src/bin/mksh/sh.h,v 1.204 2008/04/11 19:55:24 tg Exp $"
#define MKSH_VERSION "R33 2008/04/11"

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#if HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h>
#endif
#if HAVE_SYS_MKDEV_H
#include <sys/mkdev.h>
#endif
#if HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#if HAVE_LIBGEN_H
#include <libgen.h>
#endif
#if HAVE_LIBUTIL_H
#include <libutil.h>
#endif
#include <limits.h>
#if HAVE_PATHS_H
#include <paths.h>
#endif
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#if HAVE_STDBOOL_H
#include <stdbool.h>
#endif
#include <stddef.h>
#if HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#if HAVE_ULIMIT_H
#include <ulimit.h>
#endif
#include <unistd.h>
#if HAVE_VALUES_H
#include <values.h>
#endif

#if HAVE_ATTRIBUTE
#undef __attribute__
#else
#define __attribute__(x)	/* nothing */
#endif
#undef __unused
#define __unused		__attribute__((unused))
#if HAVE_ATTRIBUTE_BOUNDED
#define __bound_att__(x)	__attribute__(x)
#else
#define __bound_att__(x)	/* nothing */
#endif
#if HAVE_ATTRIBUTE_USED
#define __attribute____used__	__attribute__((used))
#else
#define __attribute____used__	/* nothing */
#endif

#undef __IDSTRING
#undef __IDSTRING_CONCAT
#undef __IDSTRING_EXPAND
#undef __RCSID
#undef __SCCSID
#define __IDSTRING_CONCAT(l,p)		__LINTED__ ## l ## _ ## p
#define __IDSTRING_EXPAND(l,p)		__IDSTRING_CONCAT(l,p)
#define __IDSTRING(prefix, string)				\
	static const char __IDSTRING_EXPAND(__LINE__,prefix) []	\
	    __attribute____used__ = "@(""#)" #prefix ": " string
#define __RCSID(x)	__IDSTRING(rcsid,x)
#define __SCCSID(x)	__IDSTRING(sccsid,x)

#ifndef MKSH_INCLUDES_ONLY

/* extra types */

#if !HAVE_RLIM_T
typedef long rlim_t;
#endif

#if !HAVE_SIG_T
#undef sig_t
typedef void (*sig_t)(int);
#endif

#if !HAVE_STDBOOL_H
/* kludge, but enough for mksh */
typedef int bool;
#define false 0
#define true 1
#endif

/* extra macros */

#ifndef timerclear
#define timerclear(tvp) do { (tvp)->tv_sec = (tvp)->tv_usec = 0; } while (0)
#endif
#ifndef timeradd
#define timeradd(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;	\
		if ((vvp)->tv_usec >= 1000000) {			\
			(vvp)->tv_sec++;				\
			(vvp)->tv_usec -= 1000000;			\
		}							\
	} while (0)
#endif
#ifndef timersub
#define timersub(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_usec += 1000000;			\
		}							\
	} while (0)
#endif

#define ksh_isdigit(c)	(((c) >= '0') && ((c) <= '9'))
#define ksh_islower(c)	(((c) >= 'a') && ((c) <= 'z'))
#define ksh_isupper(c)	(((c) >= 'A') && ((c) <= 'Z'))
#define ksh_tolower(c)	(((c) >= 'A') && ((c) <= 'Z') ? (c) - 'A' + 'a' : (c))
#define ksh_toupper(c)	(((c) >= 'a') && ((c) <= 'z') ? (c) - 'a' + 'A' : (c))
#define ksh_isdash(s)	(((s) != NULL) && ((s)[0] == '-') && ((s)[1] == '\0'))

#if HAVE_EXPSTMT
/* this macro must not evaluate its arguments several times */
#define ksh_isspace(c)	({					\
	unsigned ksh_isspace_c = (c);				\
	(ksh_isspace_c >= 0x09 && ksh_isspace_c <= 0x0D) ||	\
	    (ksh_isspace_c == 0x20);				\
})
#else
#define ksh_isspace(c)	ksh_isspace_((unsigned)(c))
#endif

#ifndef S_ISLNK
#define S_ISLNK(m)	((m & 0170000) == 0120000)
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(m)	((m & 0170000) == 0140000)
#endif
#ifndef DEFFILEMODE
#define DEFFILEMODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#endif

#if !defined(RLIMIT_VMEM) && defined(RLIMIT_AS)
#define RLIMIT_VMEM	RLIMIT_AS
#endif

#if !defined(MAP_FAILED)
#  if defined(__linux)
#define MAP_FAILED	((void *)-1)
#  elif defined(__bsdi__) || defined(__ultrix)
#define MAP_FAILED	((caddr_t)-1)
#  endif
#endif

#ifndef NSIG
#if defined(_NSIG)
#define NSIG		_NSIG
#elif defined(SIGMAX)
#define NSIG		(SIGMAX+1)
#endif
#endif

#undef BAD		/* AIX defines that somewhere */

/* OS-dependent additions (functions, variables, by OS) */

#if !HAVE_ARC4RANDOM_DECL
extern u_int32_t arc4random(void);
extern void arc4random_addrandom(unsigned char *, int)
    __bound_att__((bounded (string, 1, 2)));
#endif

#if !HAVE_ARC4RANDOM_PUSHB_DECL
extern uint32_t arc4random_pushb(void *, size_t);
#endif

#if !HAVE_FLOCK_DECL
extern int flock(int, int);
#endif

#if !HAVE_REVOKE_DECL
extern int revoke(const char *);
#endif

#if !HAVE_SETMODE
mode_t getmode(const void *, mode_t);
void *setmode(const char *);
#endif

#ifdef __ultrix
int strcasecmp(const char *, const char *);
#endif

#if !HAVE_STRCASESTR
const char *stristr(const char *, const char *);
#endif

#if !HAVE_STRLCPY
size_t strlcpy(char *, const char *, size_t);
#endif

#if !HAVE_SYS_SIGLIST_DECL
extern const char *const sys_siglist[];
#endif

#ifdef __INTERIX
#define makedev mkdev
extern int __cdecl seteuid(uid_t);
extern int __cdecl setegid(gid_t);
#endif

/* some useful #defines */
#ifdef EXTERN
# define I__(i) = i
#else
# define I__(i)
# define EXTERN extern
# define EXTERN_DEFINED
#endif

#define NELEM(a)	(sizeof (a) / sizeof ((a)[0]))
#define sizeofN(typ, n)	(sizeof (typ) * (n))
#define BIT(i)		(1 << (i))	/* define bit in flag */

/* Table flag type - needs > 16 and < 32 bits */
typedef int32_t Tflag;

#define NUFILE		32	/* Number of user-accessible files */
#define FDBASE		10	/* First file usable by Shell */

/* Make MAGIC a char that might be printed to make bugs more obvious, but
 * not a char that is used often. Also, can't use the high bit as it causes
 * portability problems (calling strchr(x, 0x80|'x') is error prone).
 */
#define MAGIC		(7)	/* prefix for *?[!{,} during expand */
#define ISMAGIC(c)	((unsigned char)(c) == MAGIC)
#define NOT		'!'	/* might use ^ (ie, [!...] vs [^..]) */

#define LINE		4096	/* input line size */
#ifndef PATH_MAX
#define PATH_MAX	1024	/* pathname size */
#endif

EXTERN const char *kshname;	/* $0 */
EXTERN pid_t kshpid;		/* $$, shell pid */
EXTERN pid_t procpid;		/* pid of executing process */
EXTERN uid_t ksheuid;		/* effective uid of shell */
EXTERN int exstat;		/* exit status */
EXTERN int subst_exstat;	/* exit status of last $(..)/`..` */
EXTERN const char *safe_prompt; /* safe prompt if PS1 substitution fails */
EXTERN const char initvsn[] I__("KSH_VERSION=@(#)MIRBSD KSH " MKSH_VERSION);
#define KSH_VERSION	(initvsn + /* "KSH_VERSION=@(#)" */ 16)

/*
 * Evil hack for const correctness due to API brokenness
 */
union mksh_cchack {
	char *rw;
	const char *ro;
};
union mksh_ccphack {
	char **rw;
	const char **ro;
};

/* for const debugging */
#if defined(DEBUG) && defined(__GNUC__) && !defined(__ICC) && \
    !defined(__INTEL_COMPILER) && !defined(__SUNPRO_C)
char *ucstrchr(char *, int);
char *ucstrstr(char *, const char *);
#define strchr ucstrchr
#define strstr ucstrstr
#define cstrchr(s,c) ({			\
	union mksh_cchack in, out;	\
					\
	in.ro = (s);			\
	out.rw = ucstrchr(in.rw, (c));	\
	(out.ro);			\
})
#define cstrstr(b,l) ({			\
	union mksh_cchack in, out;	\
					\
	in.ro = (b);			\
	out.rw = ucstrstr(in.rw, (l));	\
	(out.ro);			\
})
#define vstrchr(s,c)	(cstrchr((s), (c)) != NULL)
#define vstrstr(b,l)	(cstrstr((b), (l)) != NULL)
#else /* !DEBUG, !gcc */
#define cstrchr(s,c)	((const char *)strchr((s), (c)))
#define cstrstr(s,c)	((const char *)strstr((s), (c)))
#define vstrchr(s,c)	(strchr((s), (c)) != NULL)
#define vstrstr(b,l)	(strstr((b), (l)) != NULL)
#endif

/* use this ipv strchr(s, 0) but no side effects in s! */
#define strnul(s)	((s) + strlen(s))

#if HAVE_STRCASESTR
#define stristr(b,l)	((const char *)strcasestr((b), (l)))
#endif

#if defined(MKSH_SMALL) && !defined(MKSH_NOVI)
#define MKSH_NOVI
#endif

/*
 * Area-based allocation built on malloc/free
 */
typedef struct Area {
	struct link *freelist;	/* free list */
} Area;

EXTERN Area aperm;		/* permanent object space */
#define APERM	&aperm
#define ATEMP	&e->area

/*
 * parsing & execution environment
 */
EXTERN struct env {
	Area area;		/* temporary allocation area */
	struct block *loc;	/* local variables and functions */
	short *savefd;		/* original redirected fds */
	struct env *oenv;	/* link to previous environment */
	struct temp *temps;	/* temp files */
	sigjmp_buf jbuf;	/* long jump back to env creator */
	short type;		/* environment type - see below */
	short flags;		/* EF_* */
} *e;

/* struct env.type values */
#define E_NONE	0	/* dummy environment */
#define E_PARSE	1	/* parsing command # */
#define E_FUNC	2	/* executing function # */
#define E_INCL	3	/* including a file via . # */
#define E_EXEC	4	/* executing command tree */
#define E_LOOP	5	/* executing for/while # */
#define E_ERRH	6	/* general error handler # */
/* # indicates env has valid jbuf (see unwind()) */

/* struct env.flag values */
#define EF_FUNC_PARSE	BIT(0)	/* function being parsed */
#define EF_BRKCONT_PASS	BIT(1)	/* set if E_LOOP must pass break/continue on */
#define EF_FAKE_SIGDIE	BIT(2)	/* hack to get info from unwind to quitenv */

/* Do breaks/continues stop at env type e? */
#define STOP_BRKCONT(t)	((t) == E_NONE || (t) == E_PARSE \
			 || (t) == E_FUNC || (t) == E_INCL)
/* Do returns stop at env type e? */
#define STOP_RETURN(t)	((t) == E_FUNC || (t) == E_INCL)

/* values for siglongjmp(e->jbuf, 0) */
#define LRETURN	1	/* return statement */
#define LEXIT	2	/* exit statement */
#define LERROR	3	/* errorf() called */
#define LLEAVE	4	/* untrappable exit/error */
#define LINTR	5	/* ^C noticed */
#define LBREAK	6	/* break statement */
#define LCONTIN	7	/* continue statement */
#define LSHELL	8	/* return to interactive shell() */
#define LAEXPR	9	/* error in arithmetic expression */

/* option processing */
#define OF_CMDLINE	0x01	/* command line */
#define OF_SET		0x02	/* set builtin */
#define OF_SPECIAL	0x04	/* a special variable changing */
#define OF_INTERNAL	0x08	/* set internally by shell */
#define OF_FIRSTTIME	0x10	/* as early as possible, once */
#define OF_ANY		(OF_CMDLINE | OF_SET | OF_SPECIAL | OF_INTERNAL)

struct shoption {
	const char *name;	/* long name of option */
	char c;			/* character flag (if any) */
	unsigned char flags;	/* OF_* */
};
extern const struct shoption options[];

/*
 * flags (the order of these enums MUST match the order in misc.c(options[]))
 */
enum sh_flag {
	FEXPORT = 0,	/* -a: export all */
#if HAVE_ARC4RANDOM
	FARC4RANDOM,	/* use 0:rand(3) 1:arc4random(3) 2:switch on write */
#endif
	FBRACEEXPAND,	/* enable {} globbing */
	FBGNICE,	/* bgnice */
	FCOMMAND,	/* -c: (invocation) execute specified command */
	FEMACS,		/* emacs command editing */
	FERREXIT,	/* -e: quit on error */
	FGMACS,		/* gmacs command editing */
	FIGNOREEOF,	/* eof does not exit */
	FTALKING,	/* -i: interactive */
	FKEYWORD,	/* -k: name=value anywhere */
	FLOGIN,		/* -l: a login shell */
	FMARKDIRS,	/* mark dirs with / in file name completion */
	FMONITOR,	/* -m: job control monitoring */
	FNOCLOBBER,	/* -C: don't overwrite existing files */
	FNOEXEC,	/* -n: don't execute any commands */
	FNOGLOB,	/* -f: don't do file globbing */
	FNOHUP,		/* -H: don't kill running jobs when login shell exits */
	FNOLOG,		/* don't save functions in history (ignored) */
	FNOTIFY,	/* -b: asynchronous job completion notification */
	FNOUNSET,	/* -u: using an unset var is an error */
	FPHYSICAL,	/* -o physical: don't do logical cds/pwds */
	FPOSIX,		/* -o posix (try to be more compatible) */
	FPRIVILEGED,	/* -p: use suid_profile */
	FRESTRICTED,	/* -r: restricted shell */
	FSTDIN,		/* -s: (invocation) parse stdin */
	FTRACKALL,	/* -h: create tracked aliases for all commands */
	FUTFHACK,	/* -U: utf-8 hack for command line editing */
	FVERBOSE,	/* -v: echo input */
#ifndef MKSH_NOVI
	FVI,		/* vi command editing */
	FVIRAW,		/* always read in raw mode (ignored) */
	FVITABCOMPLETE,	/* enable tab as file name completion char */
	FVIESCCOMPLETE,	/* enable ESC as file name completion in command mode */
#endif
	FXTRACE,	/* -x: execution trace */
	FTALKING_I,	/* (internal): initial shell was interactive */
	FNFLAGS		/* (place holder: how many flags are there) */
};

#define Flag(f)	(shell_flags[(int)(f)])

EXTERN char shell_flags[FNFLAGS];

/* null value for variable; comparision pointer for unset */
EXTERN char null[] I__("");
/* helpers for string pooling */
EXTERN const char T_synerr[] I__("syntax error");

enum temp_type {
	TT_HEREDOC_EXP,	/* expanded heredoc */
	TT_HIST_EDIT	/* temp file used for history editing (fc -e) */
};
typedef enum temp_type Temp_type;
/* temp/heredoc files. The file is removed when the struct is freed. */
struct temp {
	struct temp *next;
	struct shf *shf;
	int pid;	/* pid of process parsed here-doc */
	Temp_type type;
	char *name;
};

/*
 * stdio and our IO routines
 */

#define shl_spare	(&shf_iob[0])	/* for c_read()/c_print() */
#define shl_stdout	(&shf_iob[1])
#define shl_out		(&shf_iob[2])
EXTERN int shl_stdout_ok;

/*
 * trap handlers
 */
typedef struct trap {
	int signal;		/* signal number */
	const char *name;	/* short name */
	const char *mess;	/* descriptive name */
	char *trap;		/* trap command */
	volatile sig_atomic_t set; /* trap pending */
	int flags;		/* TF_* */
	sig_t cursig;		/* current handler (valid if TF_ORIG_* set) */
	sig_t shtrap;		/* shell signal handler */
} Trap;

/* values for Trap.flags */
#define TF_SHELL_USES	BIT(0)	/* shell uses signal, user can't change */
#define TF_USER_SET	BIT(1)	/* user has (tried to) set trap */
#define TF_ORIG_IGN	BIT(2)	/* original action was SIG_IGN */
#define TF_ORIG_DFL	BIT(3)	/* original action was SIG_DFL */
#define TF_EXEC_IGN	BIT(4)	/* restore SIG_IGN just before exec */
#define TF_EXEC_DFL	BIT(5)	/* restore SIG_DFL just before exec */
#define TF_DFL_INTR	BIT(6)	/* when received, default action is LINTR */
#define TF_TTY_INTR	BIT(7)	/* tty generated signal (see j_waitj) */
#define TF_CHANGED	BIT(8)	/* used by runtrap() to detect trap changes */
#define TF_FATAL	BIT(9)	/* causes termination if not trapped */

/* values for setsig()/setexecsig() flags argument */
#define SS_RESTORE_MASK	0x3	/* how to restore a signal before an exec() */
#define SS_RESTORE_CURR	0	/* leave current handler in place */
#define SS_RESTORE_ORIG	1	/* restore original handler */
#define SS_RESTORE_DFL	2	/* restore to SIG_DFL */
#define SS_RESTORE_IGN	3	/* restore to SIG_IGN */
#define SS_FORCE	BIT(3)	/* set signal even if original signal ignored */
#define SS_USER		BIT(4)	/* user is doing the set (ie, trap command) */
#define SS_SHTRAP	BIT(5)	/* trap for internal use (CHLD,ALRM,WINCH) */

#define SIGEXIT_	0	/* for trap EXIT */
#define SIGERR_		NSIG	/* for trap ERR */

EXTERN volatile sig_atomic_t trap;	/* traps pending? */
EXTERN volatile sig_atomic_t intrsig;	/* pending trap interrupts command */
EXTERN volatile sig_atomic_t fatal_trap;/* received a fatal signal */
extern	Trap	sigtraps[NSIG+1];

/*
 * TMOUT support
 */
/* values for ksh_tmout_state */
enum tmout_enum {
	TMOUT_EXECUTING = 0,	/* executing commands */
	TMOUT_READING,		/* waiting for input */
	TMOUT_LEAVING		/* have timed out */
};
EXTERN unsigned int ksh_tmout;
EXTERN enum tmout_enum ksh_tmout_state I__(TMOUT_EXECUTING);

/* For "You have stopped jobs" message */
EXTERN int really_exit;

/*
 * fast character classes
 */
#define C_ALPHA	 BIT(0)		/* a-z_A-Z */
#define C_DIGIT	 BIT(1)		/* 0-9 */
#define C_LEX1	 BIT(2)		/* \0 \t\n|&;<>() */
#define C_VAR1	 BIT(3)		/* *@#!$-? */
#define C_IFSWS	 BIT(4)		/* \t \n (IFS white space) */
#define C_SUBOP1 BIT(5)		/* "=-+?" */
#define C_QUOTE	 BIT(6)		/*  \n\t"#$&'()*;<>?[]\`| (needing quoting) */
#define C_IFS	 BIT(7)		/* $IFS */
#define C_SUBOP2 BIT(8)		/* "#%" (magic, see below) */

extern unsigned char chtypes[];

#define ctype(c, t)	!!( ((t) == C_SUBOP2) ?				\
			    (((c) == '#' || (c) == '%') ? 1 : 0) :	\
			    (chtypes[(unsigned char)(c)]&(t)) )
#define ksh_isalphx(c)	ctype((c), C_ALPHA)
#define ksh_isalnux(c)	ctype((c), C_ALPHA | C_DIGIT)

EXTERN int ifs0 I__(' ');	/* for "$*" */

/* Argument parsing for built-in commands and getopts command */

/* Values for Getopt.flags */
#define GF_ERROR	BIT(0)	/* call errorf() if there is an error */
#define GF_PLUSOPT	BIT(1)	/* allow +c as an option */
#define GF_NONAME	BIT(2)	/* don't print argv[0] in errors */

/* Values for Getopt.info */
#define GI_MINUS	BIT(0)	/* an option started with -... */
#define GI_PLUS		BIT(1)	/* an option started with +... */
#define GI_MINUSMINUS	BIT(2)	/* arguments were ended with -- */

typedef struct {
	int		optind;
	int		uoptind;/* what user sees in $OPTIND */
	const char	*optarg;
	int		flags;	/* see GF_* */
	int		info;	/* see GI_* */
	unsigned int	p;	/* 0 or index into argv[optind - 1] */
	char		buf[2];	/* for bad option OPTARG value */
} Getopt;

EXTERN Getopt builtin_opt;	/* for shell builtin commands */
EXTERN Getopt user_opt;		/* parsing state for getopts builtin command */

/* This for co-processes */

typedef int32_t Coproc_id; /* something that won't (realisticly) wrap */
struct coproc {
	int read;	/* pipe from co-process's stdout */
	int readw;	/* other side of read (saved temporarily) */
	int write;	/* pipe to co-process's stdin */
	Coproc_id id;	/* id of current output pipe */
	int njobs;	/* number of live jobs using output pipe */
	void *job;	/* 0 or job of co-process using input pipe */
};
EXTERN struct coproc coproc;

/* Used in jobs.c and by coprocess stuff in exec.c */
EXTERN sigset_t		sm_default, sm_sigchld;

/* name of called builtin function (used by error functions) */
EXTERN const char *builtin_argv0;
EXTERN Tflag builtin_flag;	/* flags of called builtin (SPEC_BI, etc.) */

/* current working directory, and size of memory allocated for same */
EXTERN char	*current_wd;
EXTERN size_t	current_wd_size;

/* Minimum required space to work with on a line - if the prompt leaves less
 * space than this on a line, the prompt is truncated.
 */
#define MIN_EDIT_SPACE	7
/* Minimum allowed value for x_cols: 2 for prompt, 3 for " < " at end of line
 */
#define MIN_COLS	(2 + MIN_EDIT_SPACE + 3)
EXTERN int	x_cols I__(80);	/* tty columns */

/* These to avoid bracket matching problems */
#define OPAREN	'('
#define CPAREN	')'
#define OBRACK	'['
#define CBRACK	']'
#define OBRACE	'{'
#define CBRACE	'}'

/* Determine the location of the system (common) profile */
#define KSH_SYSTEM_PROFILE "/etc/profile"

/* Used by v_evaluate() and setstr() to control action when error occurs */
#define KSH_UNWIND_ERROR	0	/* unwind the stack (longjmp) */
#define KSH_RETURN_ERROR	1	/* return 1/0 for success/failure */

/*
 * Shell file I/O routines
 */

#define SHF_BSIZE	512

#define shf_fileno(shf)	((shf)->fd)
#define shf_setfileno(shf,nfd)	((shf)->fd = (nfd))
#ifdef MKSH_SMALL
int shf_getc(struct shf *);
int shf_putc(int, struct shf *);
#else
#define shf_getc(shf) ((shf)->rnleft > 0 ? (shf)->rnleft--, *(shf)->rp++ : \
			shf_getchar(shf))
#define shf_putc(c, shf)	((shf)->wnleft == 0 ? shf_putchar((c), (shf)) : \
				    ((shf)->wnleft--, *(shf)->wp++ = (c)))
#endif
#define shf_eof(shf)		((shf)->flags & SHF_EOF)
#define shf_error(shf)		((shf)->flags & SHF_ERROR)
#define shf_errno(shf)		((shf)->errno_)
#define shf_clearerr(shf)	((shf)->flags &= ~(SHF_EOF | SHF_ERROR))

/* Flags passed to shf_*open() */
#define SHF_RD		0x0001
#define SHF_WR		0x0002
#define SHF_RDWR	(SHF_RD|SHF_WR)
#define SHF_ACCMODE	0x0003		/* mask */
#define SHF_GETFL	0x0004		/* use fcntl() to figure RD/WR flags */
#define SHF_UNBUF	0x0008		/* unbuffered I/O */
#define SHF_CLEXEC	0x0010		/* set close on exec flag */
#define SHF_MAPHI	0x0020		/* make fd > FDBASE (and close orig)
					 * (shf_open() only) */
#define SHF_DYNAMIC	0x0040		/* string: increase buffer as needed */
#define SHF_INTERRUPT	0x0080		/* EINTR in read/write causes error */
/* Flags used internally */
#define SHF_STRING	0x0100		/* a string, not a file */
#define SHF_ALLOCS	0x0200		/* shf and shf->buf were alloc()ed */
#define SHF_ALLOCB	0x0400		/* shf->buf was alloc()ed */
#define SHF_ERROR	0x0800		/* read()/write() error */
#define SHF_EOF		0x1000		/* read eof (sticky) */
#define SHF_READING	0x2000		/* currently reading: rnleft,rp valid */
#define SHF_WRITING	0x4000		/* currently writing: wnleft,wp valid */


struct shf {
	int flags;		/* see SHF_* */
	unsigned char *rp;	/* read: current position in buffer */
	int rbsize;		/* size of buffer (1 if SHF_UNBUF) */
	int rnleft;		/* read: how much data left in buffer */
	unsigned char *wp;	/* write: current position in buffer */
	int wbsize;		/* size of buffer (0 if SHF_UNBUF) */
	int wnleft;		/* write: how much space left in buffer */
	unsigned char *buf;	/* buffer */
	int fd;			/* file descriptor */
	int errno_;		/* saved value of errno after error */
	int bsize;		/* actual size of buf */
	Area *areap;		/* area shf/buf were allocated in */
};

extern struct shf shf_iob[];

struct table {
	Area *areap;		/* area to allocate entries */
	struct tbl **tbls;	/* hashed table items */
	short size, nfree;	/* hash size (always 2^^n), free entries */
};

struct tbl {			/* table item */
	Tflag flag;		/* flags */
	int type;		/* command type (see below), base (if INTEGER),
				 * or offset from val.s of value (if EXPORT) */
	Area *areap;		/* area to allocate from */
	union {
		char *s;		/* string */
		long i;			/* integer */
		int (*f)(const char **);/* int function */
		struct op *t;		/* "function" tree */
	} val;			/* value */
	uint32_t index;		/* index for an array */
	union {
		int field;	/* field with for -L/-R/-Z */
		int errno_;	/* CEXEC/CTALIAS */
	} u2;
	union {
		struct tbl *array;	/* array values */
		const char *fpath;	/* temporary path to undef function */
	} u;
	char name[4];		/* name -- variable length */
};

/* common flag bits */
#define ALLOC		BIT(0)	/* val.s has been allocated */
#define DEFINED		BIT(1)	/* is defined in block */
#define ISSET		BIT(2)	/* has value, vp->val.[si] */
#define EXPORT		BIT(3)	/* exported variable/function */
#define TRACE		BIT(4)	/* var: user flagged, func: execution tracing */
/* (start non-common flags at 8) */
/* flag bits used for variables */
#define SPECIAL		BIT(8)	/* PATH, IFS, SECONDS, etc */
#define INTEGER		BIT(9)	/* val.i contains integer value */
#define RDONLY		BIT(10)	/* read-only variable */
#define LOCAL		BIT(11)	/* for local typeset() */
#define ARRAY		BIT(13)	/* array */
#define LJUST		BIT(14)	/* left justify */
#define RJUST		BIT(15)	/* right justify */
#define ZEROFIL		BIT(16)	/* 0 filled if RJUSTIFY, strip 0s if LJUSTIFY */
#define LCASEV		BIT(17)	/* convert to lower case */
#define UCASEV_AL	BIT(18)/* convert to upper case / autoload function */
#define INT_U		BIT(19)	/* unsigned integer */
#define INT_L		BIT(20)	/* long integer (no-op) */
#define IMPORT		BIT(21)	/* flag to typeset(): no arrays, must have = */
#define LOCAL_COPY	BIT(22)	/* with LOCAL - copy attrs from existing var */
#define EXPRINEVAL	BIT(23)	/* contents currently being evaluated */
#define EXPRLVALUE	BIT(24)	/* useable as lvalue (temp flag) */
/* flag bits used for taliases/builtins/aliases/keywords/functions */
#define KEEPASN		BIT(8)	/* keep command assignments (eg, var=x cmd) */
#define FINUSE		BIT(9)	/* function being executed */
#define FDELETE		BIT(10)	/* function deleted while it was executing */
#define FKSH		BIT(11)	/* function defined with function x (vs x()) */
#define SPEC_BI		BIT(12)	/* a POSIX special builtin */
#define REG_BI		BIT(13)	/* a POSIX regular builtin */
/* Attributes that can be set by the user (used to decide if an unset param
 * should be repoted by set/typeset). Does not include ARRAY or LOCAL.
 */
#define USERATTRIB	(EXPORT|INTEGER|RDONLY|LJUST|RJUST|ZEROFIL\
			    |LCASEV|UCASEV_AL|INT_U|INT_L)

/* command types */
#define CNONE		0	/* undefined */
#define CSHELL		1	/* built-in */
#define CFUNC		2	/* function */
#define CEXEC		4	/* executable command */
#define CALIAS		5	/* alias */
#define CKEYWD		6	/* keyword */
#define CTALIAS		7	/* tracked alias */

/* Flags for findcom()/comexec() */
#define FC_SPECBI	BIT(0)	/* special builtin */
#define FC_FUNC		BIT(1)	/* function builtin */
#define FC_REGBI	BIT(2)	/* regular builtin */
#define FC_UNREGBI	BIT(3)	/* un-regular builtin (!special,!regular) */
#define FC_BI		(FC_SPECBI|FC_REGBI|FC_UNREGBI)
#define FC_PATH		BIT(4)	/* do path search */
#define FC_DEFPATH	BIT(5)	/* use default path in path search */


#define AF_ARGV_ALLOC	0x1	/* argv[] array allocated */
#define AF_ARGS_ALLOCED	0x2	/* argument strings allocated */
#define AI_ARGV(a, i)	((i) == 0 ? (a).argv[0] : (a).argv[(i) - (a).skip])
#define AI_ARGC(a)	((a).argc_ - (a).skip)

/* Argument info. Used for $#, $* for shell, functions, includes, etc. */
struct arg_info {
	int flags;	/* AF_* */
	const char **argv;
	int argc_;
	int skip;	/* first arg is argv[0], second is argv[1 + skip] */
};

/*
 * activation record for function blocks
 */
struct block {
	Area area;		/* area to allocate things */
	const char **argv;
	int argc;
	int flags;		/* see BF_* */
	struct table vars;	/* local variables */
	struct table funs;	/* local functions */
	Getopt getopts_state;
	char *error;		/* error handler */
	char *exit;		/* exit handler */
	struct block *next;	/* enclosing block */
};

/* Values for struct block.flags */
#define BF_DOGETOPTS	BIT(0)	/* save/restore getopts state */

/*
 * Used by ktwalk() and ktnext() routines.
 */
struct tstate {
	int left;
	struct tbl **next;
};

EXTERN struct table taliases;	/* tracked aliases */
EXTERN struct table builtins;	/* built-in commands */
EXTERN struct table aliases;	/* aliases */
EXTERN struct table keywords;	/* keywords */
#ifndef MKSH_SMALL
EXTERN struct table homedirs;	/* homedir() cache */
#endif

struct builtin {
	const char *name;
	int (*func)(const char **);
};

extern const struct builtin mkshbuiltins[];

/* var spec values */
#define V_NONE		0
#define V_PATH		1
#define V_IFS		2
#define V_SECONDS	3
#define V_OPTIND	4
#define V_RANDOM	8
#define V_HISTSIZE	9
#define V_HISTFILE	10
#define V_COLUMNS	13
#define V_TMOUT		15
#define V_TMPDIR	16
#define V_LINENO	17

/* values for set_prompt() */
#define PS1	0	/* command */
#define PS2	1	/* command continuation */

EXTERN char *path;		/* copy of either PATH or def_path */
EXTERN const char *def_path;	/* path to use if PATH not set */
EXTERN char *tmpdir;		/* TMPDIR value */
EXTERN const char *prompt;
EXTERN int cur_prompt;		/* PS1 or PS2 */
EXTERN int current_lineno;	/* LINENO value */

#define NOBLOCK	((struct op *)NULL)
#define NOWORD	((char *)NULL)
#define NOWORDS	((char **)NULL)

/*
 * Description of a command or an operation on commands.
 */
struct op {
	const char **args;		/* arguments to a command */
	char **vars;			/* variable assignments */
	struct ioword **ioact;		/* IO actions (eg, < > >>) */
	struct op *left, *right;	/* descendents */
	char *str;			/* word for case; identifier for for,
					 * select, and functions;
					 * path to execute for TEXEC;
					 * time hook for TCOM.
					 */
	int lineno;			/* TCOM/TFUNC: LINENO for this */
	short type;			/* operation type, see below */
	union { /* WARNING: newtp(), tcopy() use evalflags = 0 to clear union */
		short evalflags;	/* TCOM: arg expansion eval() flags */
		short ksh_func;		/* TFUNC: function x (vs x()) */
	} u;
};

/* Tree.type values */
#define TEOF		0
#define TCOM		1	/* command */
#define TPAREN		2	/* (c-list) */
#define TPIPE		3	/* a | b */
#define TLIST		4	/* a ; b */
#define TOR		5	/* || */
#define TAND		6	/* && */
#define TBANG		7	/* ! */
#define TDBRACKET	8	/* [[ .. ]] */
#define TFOR		9
#define TSELECT		10
#define TCASE		11
#define TIF		12
#define TWHILE		13
#define TUNTIL		14
#define TELIF		15
#define TPAT		16	/* pattern in case */
#define TBRACE		17	/* {c-list} */
#define TASYNC		18	/* c & */
#define TFUNCT		19	/* function name { command; } */
#define TTIME		20	/* time pipeline */
#define TEXEC		21	/* fork/exec eval'd TCOM */
#define TCOPROC		22	/* coprocess |& */

/*
 * prefix codes for words in command tree
 */
#define EOS	0	/* end of string */
#define CHAR	1	/* unquoted character */
#define QCHAR	2	/* quoted character */
#define COMSUB	3	/* $() substitution (0 terminated) */
#define EXPRSUB	4	/* $(()) substitution (0 terminated) */
#define OQUOTE	5	/* opening " or ' */
#define CQUOTE	6	/* closing " or ' */
#define OSUBST	7	/* opening ${ subst (followed by { or X) */
#define CSUBST	8	/* closing } of above (followed by } or X) */
#define OPAT	9	/* open pattern: *(, @(, etc. */
#define SPAT	10	/* separate pattern: | */
#define CPAT	11	/* close pattern: ) */
#define ADELIM	12	/* arbitrary delimiter: ${foo:2:3} ${foo/bar/baz} */

/*
 * IO redirection
 */
struct ioword {
	int	unit;	/* unit affected */
	int	flag;	/* action (below) */
	char	*name;	/* file name (unused if heredoc) */
	char	*delim;	/* delimiter for <<,<<- */
	char	*heredoc;/* content of heredoc */
};

/* ioword.flag - type of redirection */
#define IOTYPE	0xF	/* type: bits 0:3 */
#define IOREAD	0x1	/* < */
#define IOWRITE	0x2	/* > */
#define IORDWR	0x3	/* <>: todo */
#define IOHERE	0x4	/* << (here file) */
#define IOCAT	0x5	/* >> */
#define IODUP	0x6	/* <&/>& */
#define IOEVAL	BIT(4)	/* expand in << */
#define IOSKIP	BIT(5)	/* <<-, skip ^\t* */
#define IOCLOB	BIT(6)	/* >|, override -o noclobber */
#define IORDUP	BIT(7)	/* x<&y (as opposed to x>&y) */
#define IONAMEXP BIT(8)	/* name has been expanded */

/* execute/exchild flags */
#define XEXEC	BIT(0)		/* execute without forking */
#define XFORK	BIT(1)		/* fork before executing */
#define XBGND	BIT(2)		/* command & */
#define XPIPEI	BIT(3)		/* input is pipe */
#define XPIPEO	BIT(4)		/* output is pipe */
#define XPIPE	(XPIPEI|XPIPEO)	/* member of pipe */
#define XXCOM	BIT(5)		/* `...` command */
#define XPCLOSE	BIT(6)		/* exchild: close close_fd in parent */
#define XCCLOSE	BIT(7)		/* exchild: close close_fd in child */
#define XERROK	BIT(8)		/* non-zero exit ok (for set -e) */
#define XCOPROC BIT(9)		/* starting a co-process */
#define XTIME	BIT(10)		/* timing TCOM command */

/*
 * flags to control expansion of words (assumed by t->evalflags to fit
 * in a short)
 */
#define DOBLANK	BIT(0)		/* perform blank interpretation */
#define DOGLOB	BIT(1)		/* expand [?* */
#define DOPAT	BIT(2)		/* quote *?[ */
#define DOTILDE	BIT(3)		/* normal ~ expansion (first char) */
#define DONTRUNCOMMAND BIT(4)	/* do not run $(command) things */
#define DOASNTILDE BIT(5)	/* assignment ~ expansion (after =, :) */
#define DOBRACE_ BIT(6)		/* used by expand(): do brace expansion */
#define DOMAGIC_ BIT(7)		/* used by expand(): string contains MAGIC */
#define DOTEMP_	BIT(8)		/* ditto : in word part of ${..[%#=?]..} */
#define DOVACHECK BIT(9)	/* var assign check (for typeset, set, etc) */
#define DOMARKDIRS BIT(10)	/* force markdirs behaviour */

/*
 * The arguments of [[ .. ]] expressions are kept in t->args[] and flags
 * indicating how the arguments have been munged are kept in t->vars[].
 * The contents of t->vars[] are stuffed strings (so they can be treated
 * like all other t->vars[]) in which the second character is the one that
 * is examined. The DB_* defines are the values for these second characters.
 */
#define DB_NORM	1	/* normal argument */
#define DB_OR	2	/* || -> -o conversion */
#define DB_AND	3	/* && -> -a conversion */
#define DB_BE	4	/* an inserted -BE */
#define DB_PAT	5	/* a pattern argument */

#define X_EXTRA	8	/* this many extra bytes in X string */

typedef struct XString {
	char *end, *beg;	/* end, begin of string */
	size_t len;		/* length */
	Area *areap;		/* area to allocate/free from */
} XString;

typedef char *XStringP;

/* initialise expandable string */
#define XinitN(xs, length, area) do {				\
	(xs).len = (length);					\
	(xs).areap = (area);					\
	(xs).beg = alloc((xs).len + X_EXTRA, (xs).areap);	\
	(xs).end = (xs).beg + (xs).len;				\
} while (0)
#define Xinit(xs, xp, length, area) do {			\
	XinitN((xs), (length), (area));				\
	(xp) = (xs).beg;					\
} while (0)

/* stuff char into string */
#define Xput(xs, xp, c)	(*xp++ = (c))

/* check if there are at least n bytes left */
#define XcheckN(xs, xp, n) do {					\
	int more = ((xp) + (n)) - (xs).end;			\
	if (more > 0)						\
		(xp) = Xcheck_grow_(&(xs), (xp), more);		\
} while (0)

/* check for overflow, expand string */
#define Xcheck(xs, xp)	XcheckN((xs), (xp), 1)

/* free string */
#define Xfree(xs, xp)	afree((void*)(xs).beg, (xs).areap)

/* close, return string */
#define Xclose(xs, xp)	(char*)aresize((void*)(xs).beg, \
			    (size_t)((xp) - (xs).beg), (xs).areap)
/* begin of string */
#define Xstring(xs, xp)	((xs).beg)

#define Xnleft(xs, xp) ((xs).end - (xp))	/* may be less than 0 */
#define Xlength(xs, xp) ((xp) - (xs).beg)
#define Xsize(xs, xp) ((xs).end - (xs).beg)
#define Xsavepos(xs, xp) ((xp) - (xs).beg)
#define Xrestpos(xs, xp, n) ((xs).beg + (n))

char *Xcheck_grow_(XString *, const char *, unsigned);

/*
 * expandable vector of generic pointers
 */

typedef struct XPtrV {
	void **cur;		/* next avail pointer */
	void **beg, **end;	/* begin, end of vector */
} XPtrV;

#define XPinit(x, n) do {					\
	void **vp__;						\
	vp__ = (void**) alloc(sizeofN(void*, (n)), ATEMP);	\
	(x).cur = (x).beg = vp__;				\
	(x).end = vp__ + (n);					\
} while (0)

#define XPput(x, p) do {					\
	if ((x).cur >= (x).end) {				\
		int n = XPsize(x);				\
		(x).beg = (void**) aresize((void*) (x).beg,	\
		    sizeofN(void *, n * 2), ATEMP);		\
		(x).cur = (x).beg + n;				\
		(x).end = (x).cur + n;				\
	}							\
	*(x).cur++ = (p);					\
} while (0)

#define XPptrv(x)	((x).beg)
#define XPsize(x)	((x).cur - (x).beg)

#define XPclose(x)	(void**)aresize((void*)(x).beg, \
			    sizeofN(void *, XPsize(x)), ATEMP)

#define XPfree(x)	afree((void *)(x).beg, ATEMP)

#define IDENT	64

typedef struct source Source;
struct source {
	const char *str;	/* input pointer */
	int	type;		/* input type */
	const char *start;	/* start of current buffer */
	union {
		const char **strv; /* string [] */
		struct shf *shf;   /* shell file */
		struct tbl *tblp;  /* alias (SALIAS) */
		char *freeme;	   /* also for SREREAD */
	} u;
	int	line;		/* line number */
	int	errline;	/* line the error occurred on (0 if not set) */
	const char *file;	/* input file name */
	int	flags;		/* SF_* */
	Area	*areap;
	XString	xs;		/* input buffer */
	Source *next;		/* stacked source */
	char	ugbuf[2];	/* buffer for ungetsc() (SREREAD) and
				 * alias (SALIAS) */
};

/* Source.type values */
#define SEOF		0	/* input EOF */
#define SFILE		1	/* file input */
#define SSTDIN		2	/* read stdin */
#define SSTRING		3	/* string */
#define SWSTR		4	/* string without \n */
#define SWORDS		5	/* string[] */
#define SWORDSEP	6	/* string[] separator */
#define SALIAS		7	/* alias expansion */
#define SREREAD		8	/* read ahead to be re-scanned */

/* Source.flags values */
#define SF_ECHO		BIT(0)	/* echo input to shlout */
#define SF_ALIAS	BIT(1)	/* faking space at end of alias */
#define SF_ALIASEND	BIT(2)	/* faking space at end of alias */
#define SF_TTY		BIT(3)	/* type == SSTDIN & it is a tty */
#define SF_FIRST	BIT(4)	/* initial state (to ignore UTF-8 BOM) */

typedef union {
	int i;
	char *cp;
	char **wp;
	struct op *o;
	struct ioword *iop;
} YYSTYPE;

/* If something is added here, add it to tokentab[] in syn.c as well */
#define LWORD		256
#define LOGAND		257	/* && */
#define LOGOR		258	/* || */
#define BREAK		259	/* ;; */
#define IF		260
#define THEN		261
#define ELSE		262
#define ELIF		263
#define FI		264
#define CASE		265
#define ESAC		266
#define FOR		267
#define SELECT		268
#define WHILE		269
#define UNTIL		270
#define DO		271
#define DONE		272
#define IN		273
#define FUNCTION	274
#define TIME		275
#define REDIR		276
#define MDPAREN		277	/* (( )) */
#define BANG		278	/* ! */
#define DBRACKET	279	/* [[ .. ]] */
#define COPROC		280	/* |& */
#define YYERRCODE	300

/* flags to yylex */
#define CONTIN		BIT(0)	/* skip new lines to complete command */
#define ONEWORD		BIT(1)	/* single word for substitute() */
#define ALIAS		BIT(2)	/* recognise alias */
#define KEYWORD		BIT(3)	/* recognise keywords */
#define LETEXPR		BIT(4)	/* get expression inside (( )) */
#define VARASN		BIT(5)	/* check for var=word */
#define ARRAYVAR	BIT(6)	/* parse x[1 & 2] as one word */
#define ESACONLY	BIT(7)	/* only accept esac keyword */
#define CMDWORD		BIT(8)	/* parsing simple command (alias related) */
#define HEREDELIM	BIT(9)	/* parsing <<,<<- delimiter */
#define LQCHAR		BIT(10)	/* source string contains QCHAR */
#define HEREDOC		BIT(11)	/* parsing a here document */
#define LETARRAY	BIT(12)	/* copy expression inside =( ) */

#define HERES	10		/* max << in line */

EXTERN Source *source;		/* yyparse/yylex source */
EXTERN YYSTYPE	yylval;		/* result from yylex */
EXTERN struct ioword *heres [HERES], **herep;
EXTERN char	ident [IDENT+1];

#define HISTORYSIZE	500	/* size of saved history */

EXTERN char **history;	/* saved commands */
EXTERN char **histptr;	/* last history item */
EXTERN int histsize;	/* history size */

/* user and system time of last j_waitjed job */
EXTERN struct timeval j_usrtime, j_systime;

/* alloc.c */
Area *ainit(Area *);
void afreeall(Area *);
void *alloc(size_t, Area *);
void *aresize(void *, size_t, Area *);
void afree(void *, Area *);
#define afreechk(s)	do {		\
	if (s)				\
		afree(s, ATEMP);	\
} while (0)
#define afreechv(v,s)	do {		\
	if (v)				\
		afree(s, ATEMP);	\
} while (0)
/* edit.c */
void x_init(void);
int x_read(char *, size_t);
int x_bind(const char *, const char *, int, int);
/* UTF-8 hack stuff */
int utf_widthadj(const char *, const char **);
/* eval.c */
char *substitute(const char *, int);
char **eval(const char **, int);
char *evalstr(const char *cp, int);
char *evalonestr(const char *cp, int);
char *debunk(char *, const char *, size_t);
void expand(const char *, XPtrV *, int);
int glob_str(char *, XPtrV *, int);
/* exec.c */
int execute(struct op * volatile, volatile int);
int shcomexec(const char **);
struct tbl *findfunc(const char *, unsigned int, int);
int define(const char *, struct op *);
void builtin(const char *, int (*)(const char **));
struct tbl *findcom(const char *, int);
void flushcom(int);
const char *search(const char *, const char *, int, int *);
int search_access(const char *, int, int *);
int pr_menu(const char *const *);
int pr_list(char *const *);
/* expr.c */
int evaluate(const char *, long *, int, bool);
int v_evaluate(struct tbl *, const char *, volatile int, bool);
/* funcs.c */
int c_hash(const char **);
int c_cd(const char **);
int c_pwd(const char **);
int c_print(const char **);
int c_whence(const char **);
int c_command(const char **);
int c_typeset(const char **);
int c_alias(const char **);
int c_unalias(const char **);
int c_let(const char **);
int c_jobs(const char **);
int c_fgbg(const char **);
int c_kill(const char **);
void getopts_reset(int);
int c_getopts(const char **);
int c_bind(const char **);
int c_label(const char **);
int c_shift(const char **);
int c_umask(const char **);
int c_dot(const char **);
int c_wait(const char **);
int c_read(const char **);
int c_eval(const char **);
int c_trap(const char **);
int c_brkcont(const char **);
int c_exitreturn(const char **);
int c_set(const char **);
int c_unset(const char **);
int c_ulimit(const char **);
int c_times(const char **);
int timex(struct op *, int);
void timex_hook(struct op *, char ** volatile *);
int c_exec(const char **);
int c_builtin(const char **);
int c_test(const char **);
#if HAVE_MKNOD
int c_mknod(const char **);
#endif
int c_rename(const char **);
/* histrap.c */
void init_histvec(void);
void hist_init(Source *);
#if HAVE_PERSISTENT_HISTORY
void hist_finish(void);
#endif
void histsave(int, const char *, int);
int c_fc(const char **);
void sethistsize(int);
#if HAVE_PERSISTENT_HISTORY
void sethistfile(const char *);
#endif
char **histpos(void);
int histnum(int);
int findhist(int, int, const char *, int);
int findhistrel(const char *);
char **hist_get_newest(int);
void inittraps(void);
void alarm_init(void);
Trap *gettrap(const char *, int);
void trapsig(int);
void intrcheck(void);
int fatal_trap_check(void);
int trap_pending(void);
void runtraps(int intr);
void runtrap(Trap *);
void cleartraps(void);
void restoresigs(void);
void settrap(Trap *, const char *);
int block_pipe(void);
void restore_pipe(int);
int setsig(Trap *, sig_t, int);
void setexecsig(Trap *, int);
/* jobs.c */
void j_init(int);
void j_exit(void);
void j_change(void);
int exchild(struct op *, int, int);
void startlast(void);
int waitlast(void);
int waitfor(const char *, int *);
int j_kill(const char *, int);
int j_resume(const char *, int);
int j_jobs(const char *, int, int);
int j_njobs(void);
void j_notify(void);
pid_t j_async(void);
int j_stopped_running(void);
/* lex.c */
int yylex(int);
void yyerror(const char *, ...)
    __attribute__((noreturn))
    __attribute__((format (printf, 1, 2)));
Source *pushs(int, Area *);
void set_prompt(int, Source *);
void pprompt(const char *, int);
int promptlen(const char *);
/* main.c */
int include(const char *, int, const char **, int);
int command(const char *);
int shell(Source *volatile, int volatile);
void unwind(int)
    __attribute__((noreturn));
void newenv(int);
void quitenv(struct shf *);
void cleanup_parents_env(void);
void cleanup_proc_env(void);
void errorf(const char *, ...)
    __attribute__((noreturn))
    __attribute__((format (printf, 1, 2)));
void warningf(bool, const char *, ...)
    __attribute__((format (printf, 2, 3)));
void bi_errorf(const char *, ...)
    __attribute__((format (printf, 1, 2)));
/*
 * circumvent compiler format string nonnull checking
 * we teach xlC to not bitch about zero-lengths, want
 * gcc to do it, and so gain double-checking benefits
 */
#if defined(__xlC__)
#define errorfz()	errorf("")
#define bi_errorfz()	bi_errorf("")
#else
#define errorfz()	errorf(null)
#define bi_errorfz()	bi_errorf(null)
#endif
void internal_errorf(const char *, ...)
    __attribute__((noreturn))
    __attribute__((format (printf, 1, 2)));
void internal_warningf(const char *, ...)
    __attribute__((format (printf, 1, 2)));
void error_prefix(bool);
void shellf(const char *, ...)
    __attribute__((format (printf, 1, 2)));
void shprintf(const char *, ...)
    __attribute__((format (printf, 1, 2)));
int can_seek(int);
void initio(void);
int ksh_dup2(int, int, bool);
short savefd(int);
void restfd(int, int);
void openpipe(int *);
void closepipe(int *);
int check_fd(const char *, int, const char **);
void coproc_init(void);
void coproc_read_close(int);
void coproc_readw_close(int);
void coproc_write_close(int);
int coproc_getfd(int, const char **);
void coproc_cleanup(int);
struct temp *maketemp(Area *, Temp_type, struct temp **);
unsigned int hash(const char *);
void ktinit(struct table *, Area *, int);
struct tbl *ktsearch(struct table *, const char *, unsigned int);
struct tbl *ktenter(struct table *, const char *, unsigned int);
#define ktdelete(p)	do { p->flag = 0; } while (0)
void ktwalk(struct tstate *, struct table *);
struct tbl *ktnext(struct tstate *);
struct tbl **ktsort(struct table *);
/* misc.c */
void setctypes(const char *, int);
void initctypes(void);
#ifdef MKSH_SMALL
char *str_save(const char *, Area *);
#else
#define str_save(s,ap) (str_nsave((s), (s) ? strlen(s) : 0, (ap)))
#endif
char *str_nsave(const char *, int, Area *);
size_t option(const char *);
char *getoptions(void);
void change_flag(enum sh_flag, int, char);
int parse_args(const char **, int, int *);
int getn(const char *, int *);
int bi_getn(const char *, int *);
int gmatchx(const char *, const char *, bool);
int has_globbing(const char *, const char *);
const unsigned char *pat_scan(const unsigned char *, const unsigned char *, int);
int xstrcmp(const void *, const void *);
void ksh_getopt_reset(Getopt *, int);
int ksh_getopt(const char **, Getopt *, const char *);
void print_value_quoted(const char *);
void print_columns(struct shf *, int,
    char *(*)(const void *, int, char *, int),
    const void *, int, int prefcol);
void strip_nuls(char *, int);
int blocking_read(int, char *, int)
    __bound_att__((bounded (buffer, 2, 3)));
int reset_nonblock(int);
char *ksh_get_wd(size_t *);
int make_path(const char *, const char *, char **, XString *, int *);
void simplify_path(char *);
char *get_phys_path(const char *);
void set_current_wd(char *);
#if !HAVE_EXPSTMT
bool ksh_isspace_(unsigned);
#endif
/* shf.c */
struct shf *shf_open(const char *, int, int, int);
struct shf *shf_fdopen(int, int, struct shf *);
struct shf *shf_reopen(int, int, struct shf *);
struct shf *shf_sopen(char *, int, int, struct shf *);
int shf_close(struct shf *);
int shf_fdclose(struct shf *);
char *shf_sclose(struct shf *);
int shf_flush(struct shf *);
int shf_read(char *, int, struct shf *);
char *shf_getse(char *, int, struct shf *);
int shf_getchar(struct shf *s);
int shf_ungetc(int, struct shf *);
int shf_putchar(int, struct shf *);
int shf_puts(const char *, struct shf *);
int shf_write(const char *, int, struct shf *);
int shf_fprintf(struct shf *, const char *, ...)
    __attribute__((format (printf, 2, 3)));
int shf_snprintf(char *, int, const char *, ...)
    __attribute__((format (printf, 3, 4)))
    __bound_att__((bounded (string, 1, 2)));
char *shf_smprintf(const char *, ...)
    __attribute__((format (printf, 1, 2)));
int shf_vfprintf(struct shf *, const char *, va_list)
    __attribute__((format (printf, 2, 0)));
/* syn.c */
void initkeywords(void);
struct op *compile(Source *);
/* tree.c */
int fptreef(struct shf *, int, const char *, ...);
char *snptreef(char *, int, const char *, ...);
struct op *tcopy(struct op *, Area *);
char *wdcopy(const char *, Area *);
const char *wdscan(const char *, int);
char *wdstrip(const char *, bool, bool);
void tfree(struct op *, Area *);
/* var.c */
void newblock(void);
void popblock(void);
void initvar(void);
struct tbl *global(const char *);
struct tbl *local(const char *, bool);
char *str_val(struct tbl *);
long intval(struct tbl *);
int setstr(struct tbl *, const char *, int);
struct tbl *setint_v(struct tbl *, struct tbl *, bool);
void setint(struct tbl *, long);
int getint(struct tbl *, long *, bool);
struct tbl *typeset(const char *, Tflag, Tflag, int, int);
void unset(struct tbl *, int);
const char *skip_varname(const char *, int);
const char *skip_wdvarname(const char *, int);
int is_wdvarname(const char *, int);
int is_wdvarassign(const char *);
char **makenv(void);
void change_random(unsigned long);
int array_ref_len(const char *);
char *arrayname(const char *);
void set_array(const char *, int, const char **);

enum Test_op {
	TO_NONOP = 0,	/* non-operator */
	/* unary operators */
	TO_STNZE, TO_STZER, TO_OPTION,
	TO_FILAXST,
	TO_FILEXST,
	TO_FILREG, TO_FILBDEV, TO_FILCDEV, TO_FILSYM, TO_FILFIFO, TO_FILSOCK,
	TO_FILCDF, TO_FILID, TO_FILGID, TO_FILSETG, TO_FILSTCK, TO_FILUID,
	TO_FILRD, TO_FILGZ, TO_FILTT, TO_FILSETU, TO_FILWR, TO_FILEX,
	/* binary operators */
	TO_STEQL, TO_STNEQ, TO_STLT, TO_STGT, TO_INTEQ, TO_INTNE, TO_INTGT,
	TO_INTGE, TO_INTLT, TO_INTLE, TO_FILEQ, TO_FILNT, TO_FILOT
};
typedef enum Test_op Test_op;

/* Used by Test_env.isa() (order important - used to index *_tokens[] arrays) */
enum Test_meta {
	TM_OR,		/* -o or || */
	TM_AND,		/* -a or && */
	TM_NOT,		/* ! */
	TM_OPAREN,	/* ( */
	TM_CPAREN,	/* ) */
	TM_UNOP,	/* unary operator */
	TM_BINOP,	/* binary operator */
	TM_END		/* end of input */
};
typedef enum Test_meta Test_meta;

#define TEF_ERROR	BIT(0)		/* set if we've hit an error */
#define TEF_DBRACKET	BIT(1)		/* set if [[ .. ]] test */

typedef struct test_env Test_env;
struct test_env {
	int flags;		/* TEF_* */
	union {
		const char **wp;/* used by ptest_* */
		XPtrV *av;	/* used by dbtestp_* */
	} pos;
	const char **wp_end;	/* used by ptest_* */
	int (*isa)(Test_env *, Test_meta);
	const char *(*getopnd) (Test_env *, Test_op, bool);
	int (*eval)(Test_env *, Test_op, const char *, const char *, bool);
	void (*error)(Test_env *, int, const char *);
};

extern const char *const dbtest_tokens[];

Test_op	test_isop(Test_meta, const char *);
int test_eval(Test_env *, Test_op, const char *, const char *, bool);
int test_parse(Test_env *);

EXTERN int tty_fd I__(-1);	/* dup'd tty file descriptor */
EXTERN int tty_devtty;		/* true if tty_fd is from /dev/tty */
EXTERN struct termios tty_state;	/* saved tty state */

extern void tty_init(int);
extern void tty_close(void);

/* be sure not to interfere with anyone else's idea about EXTERN */
#ifdef EXTERN_DEFINED
# undef EXTERN_DEFINED
# undef EXTERN
#endif
#undef I__

#endif /* !MKSH_INCLUDES_ONLY */
