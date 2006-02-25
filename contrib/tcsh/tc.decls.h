/* $Header: /home/cvs/src/contrib/tcsh/tc.decls.h,v 1.1.1.2 2006-02-25 02:34:05 laffer1 Exp $ */
/*
 * tc.decls.h: Function declarations from all the tcsh modules
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef _h_tc_decls
#define _h_tc_decls

/*
 * tc.alloc.c
 */
#ifndef SYSMALLOC
#ifndef WINNT_NATIVE
extern	void		  free		__P((ptr_t));
extern	memalign_t	  malloc	__P((size_t));
extern	memalign_t	  realloc	__P((ptr_t, size_t));
extern	memalign_t	  calloc	__P((size_t, size_t));
#endif /* !WINNT_NATIVE */
#else /* SYSMALLOC */
extern	void		  sfree		__P((ptr_t));
extern	memalign_t	  smalloc	__P((size_t));
extern	memalign_t	  srealloc	__P((ptr_t, size_t));
extern	memalign_t	  scalloc	__P((size_t, size_t));
#endif /* SYSMALLOC */
extern	void		  showall	__P((Char **, struct command *));

/*
 * tc.bind.c
 */
extern	void		  dobindkey	__P((Char **, struct command *));
#ifdef OBSOLETE
extern	void		  dobind	__P((Char **, struct command *));
#endif /* OBSOLETE */

/*
 * tc.defs.c:
 */
extern	void		  getmachine	__P((void));


/*
 * tc.disc.c
 */
extern	int		  setdisc	__P((int));
extern	int		  resetdisc	__P((int));

/*
 * tc.func.c
 */
extern	Char		 *expand_lex	__P((Char *, size_t, struct wordent *, 
					     int, int));
extern	Char		 *sprlex	__P((Char *, size_t, struct wordent *));
extern	Char		 *Itoa		__P((int, Char *, int, int));
extern	void		  dolist	__P((Char **, struct command *));
extern	void		  dotermname	__P((Char **, struct command *));
extern	void		  dotelltc	__P((Char **, struct command *));
extern	void		  doechotc	__P((Char **, struct command *));
extern	void		  dosettc	__P((Char **, struct command *));
extern	int		  cmd_expand	__P((Char *, Char *));
extern	void		  dowhich	__P((Char **, struct command *));
extern	struct process	 *find_stop_ed	__P((void));
extern	void		  fg_proc_entry	__P((struct process *));
extern	RETSIGTYPE	  alrmcatch	__P((int));
extern	void		  precmd	__P((void));
extern	void		  postcmd	__P((void));
extern	void		  cwd_cmd	__P((void));
extern	void		  beep_cmd	__P((void));
extern	void		  period_cmd	__P((void));
extern	void		  job_cmd	__P((Char *));
extern	void		  aliasrun	__P((int, Char *, Char *));
extern	void		  setalarm	__P((int));
extern	void		  rmstar	__P((struct wordent *));
extern	void		  continue_jobs	__P((struct wordent *));
extern	Char		 *gettilde	__P((Char *));
extern	Char		 *getusername	__P((Char **));
#ifdef OBSOLETE
extern	void		  doaliases	__P((Char **, struct command *));
#endif /* OBSOLETE */
extern	void		  shlvl		__P((int));
extern	int		  fixio		__P((int, int));
extern	int		  collate	__P((const Char *, const Char *));
#ifdef HASHBANG
extern	int		  hashbang	__P((int, Char ***));
#endif /* HASHBANG */
#ifdef REMOTEHOST
extern	void		  remotehost	__P((void));
#endif /* REMOTEHOST */


/*
 * tc.os.c
 */
#ifdef MACH
extern	void		  dosetpath	__P((Char **, struct command *));
#endif /* MACH */

#ifdef TCF
extern	void		  dogetxvers	__P((Char **, struct command *));
extern	void		  dosetxvers	__P((Char **, struct command *));
extern	void		  dogetspath	__P((Char **, struct command *));
extern	void		  dosetspath	__P((Char **, struct command *));
extern	char		 *sitename	__P((pid_t));
extern	void		  domigrate	__P((Char **, struct command *));
#endif /* TCF */

#ifdef WARP
extern	void 		  dowarp	__P((Char **, struct command *));
#endif /* WARP */

#if defined(_CRAY) && !defined(_CRAYMPP)
extern	void 		  dodmmode	__P((Char **, struct command *));
#endif /* _CRAY && !_CRAYMPP */

#if defined(masscomp) || defined(hcx)
extern	void		  douniverse	__P((Char **, struct command *));
#endif /* masscomp */

#if defined(_OSD_POSIX) /* BS2000 */
extern	void		  dobs2cmd	__P((Char **, struct command *));
#endif /* _OSD_POSIX */

#if defined(hcx)
extern	void		  doatt		__P((Char **, struct command *));
extern	void		  doucb		__P((Char **, struct command *));
#endif /* hcx */

#ifdef _SEQUENT_
extern	void	 	  pr_stat_sub	__P((struct process_stats *, 
					     struct process_stats *, 
					     struct process_stats *));
#endif /* _SEQUENT_ */

#ifdef NEEDtcgetpgrp
extern	int	 	  xtcgetpgrp	__P((int));
extern	int		  xtcsetpgrp	__P((int, int));
# undef tcgetpgrp
# define tcgetpgrp(a) 	  xtcgetpgrp(a)
# undef tcsetpgrp
# define tcsetpgrp(a, b)  xtcsetpgrp((a), (b))
#endif /* NEEDtcgetpgrp */

#ifdef YPBUGS
extern	void	 	  fix_yp_bugs	__P((void));
#endif /* YPBUGS */
#ifdef STRCOLLBUG
extern	void	 	  fix_strcoll_bug	__P((void));
#endif /* STRCOLLBUG */

extern	void	 	  osinit	__P((void));

#ifndef HAVE_MEMMOVE
extern ptr_t 		 xmemmove	__P((ptr_t, const ptr_t, size_t));
# define memmove(a, b, c) xmemmove(a, b, c)
#endif /* !HAVE_MEMMOVE */

#ifndef HAVE_MEMSET
extern ptr_t 		 xmemset	__P((ptr_t, int, size_t));
# define memset(a, b, c) xmemset(a, b, c)
#endif /* !HAVE_MEMSET */


#ifndef HAVE_GETCWD
extern	char		 *xgetcwd	__P((char *, size_t));
# undef getcwd
# define getcwd(a, b) xgetcwd(a, b)
#endif /* !HAVE_GETCWD */

#ifndef HAVE_GETHOSTNAME
extern	int	 	  xgethostname	__P((char *, int));
# undef gethostname
# define gethostname(a, b) xgethostname(a, b)
#endif /* !HAVE_GETHOSTNAME */

#ifndef HAVE_NICE
extern	int	 	  xnice	__P((int));
# undef nice
# define nice(a)	  xnice(a)
#endif /* !HAVE_NICE */

#ifndef HAVE_STRERROR
extern	char	 	 *xstrerror	__P((int));
# undef strerror
# define strerror(a) 	  xstrerror(a)
#endif /* !HAVE_STRERROR */

#ifdef apollo
extern	void		  doinlib	__P((Char **, struct command *));
extern	void		  dover		__P((Char **, struct command *));
extern	void		  dorootnode	__P((Char **, struct command *));
extern	int		  getv		__P((Char *));
#endif /* apollo */


/*
 * tc.printf.h
 */
extern	pret_t		  xprintf	__P((const char *, ...));
extern	pret_t		  xsnprintf	__P((char *, size_t, const char *, ...));
extern	pret_t		  xvprintf	__P((const char *, va_list));
extern	pret_t		  xvsnprintf	__P((char *, size_t, const char *,
					     va_list));

/*
 * tc.prompt.c
 */
extern	void		  dateinit	__P((void));
extern	void		  printprompt	__P((int, const char *));
extern  Char 		 *expdollar	__P((Char **, const Char **, size_t *,
					     int));
extern	void		  tprintf	__P((int, Char *, const Char *, size_t, 
					     const char *, time_t, ptr_t));

/*
 * tc.sched.c
 */
extern	time_t		  sched_next	__P((void));
extern	void		  dosched	__P((Char **, struct command *));
extern	void		  sched_run	__P((int));

/*
 * tc.sig.c
 */
#ifndef BSDSIGS
# ifdef UNRELSIGS
#  ifdef COHERENT
extern	RETSIGTYPE	(*xsignal	__P((int, RETSIGTYPE (*)(int)))) ();
#   define signal(x,y)	  xsignal(x,y)
#  endif /* COHERENT */
extern	RETSIGTYPE	(*xsigset	__P((int, RETSIGTYPE (*)(int)))) ();
#  define sigset(x,y)	  xsigset(x,y)
extern	void		  xsigrelse	__P((int));
#  define sigrelse(x)	  xsigrelse(x)
extern	void		  xsighold	__P((int));
#  define sighold(x)	  xsighold(x)
extern	void		  xsigignore	__P((int));
#  define sigignore(x)	  xsigignore(x)
extern	void 		  xsigpause	__P((int));
#  define sigpause(x)	  xsigpause(x)
extern	pid_t 		  ourwait	__P((int *));
# endif /* UNRELSIGS */
# ifdef SXA
extern	void 		  sigpause	__P((int));
# endif /* SXA */
#endif /* !BSDSIGS */

#ifdef NEEDsignal
extern	RETSIGTYPE	(*xsignal	__P((int, RETSIGTYPE (*)(int)))) ();
# define signal(a, b)	  xsignal(a, b)
#endif /* NEEDsignal */
#if defined(_SEQUENT_) || ((SYSVREL > 3 || defined(_DGUX_SOURCE)) && defined(POSIXSIGS)) || ((defined(_AIX) || defined(__CYGWIN__)) && defined(POSIXSIGS)) || defined(WINNT_NATIVE)
extern	sigmask_t	  sigsetmask	__P((sigmask_t));
# if !defined(DGUX) || (defined(DGUX) && defined(__ix86))
extern	sigmask_t	  sigblock	__P((sigmask_t));
# endif /* !DGUX */
extern	void		  bsd_sigpause	__P((sigmask_t));
extern  RETSIGTYPE        (*bsd_signal    __P((int, RETSIGTYPE (*)(int)))) __P((int));
#endif /* _SEQUENT_ */
#ifdef SIGSYNCH
extern	RETSIGTYPE	  synch_handler	__P((int));
#endif /* SIGSYNCH */


/*
 * tc.str.c:
 */
#ifdef WIDE_STRINGS
extern	size_t		  one_mbtowc	__P((wchar_t *, const char *, size_t));
extern	size_t		  one_wctomb	__P((char *, wchar_t));
#else
#define one_mbtowc(PWC, S, N) \
	((void)(N), *(PWC) = (unsigned char)*(S), (size_t)1)
#define one_wctomb(S, WCHAR) (*(S) = (WCHAR), (size_t)1)
#endif
#ifdef SHORT_STRINGS
extern  int		  rt_mbtowc	__P((wchar_t *, const char *, size_t));
extern	Char		 *s_strchr	__P((const Char *, int));
extern	Char		 *s_strrchr	__P((const Char *, int));
extern	Char		 *s_strcat	__P((Char *, const Char *));
# ifdef NOTUSED
extern	Char		 *s_strncat	__P((Char *, const Char *, size_t));
# endif /* NOTUSED */
extern	Char		 *s_strcpy	__P((Char *, const Char *));
extern	Char		 *s_strncpy	__P((Char *, const Char *, size_t));
extern	Char		 *s_strspl	__P((const Char *, const Char *));
extern	size_t		  s_strlen	__P((const Char *));
extern	int		  s_strcmp	__P((const Char *, const Char *));
extern	int		  s_strncmp	__P((const Char *, const Char *, 
					     size_t));
extern	int		  s_strcasecmp	__P((const Char *, const Char *));
extern	Char		 *s_strsave	__P((const Char *));
extern	Char		 *s_strend	__P((const Char *));
extern	Char		 *s_strstr	__P((const Char *, const Char *));
extern	Char		 *str2short	__P((const char *));
extern	Char		**blk2short	__P((char **));
extern	char		 *short2str	__P((const Char *));
extern	char		**short2blk	__P((Char **));
#endif /* SHORT_STRINGS */
extern	char		 *short2qstr	__P((const Char *));


/*
 * tc.vers.c:
 */
extern	void		  fix_version	__P((void));

/*
 * tc.who.c
 */
#if defined (HAVE_UTMP_H) || defined (HAVE_UTMPX_H) || defined (WINNT_NATIVE)
extern	void		  initwatch	__P((void));
extern	void		  resetwatch	__P((void));
extern	void		  watch_login	__P((int));
extern	const char 	 *who_info	__P((ptr_t, int, char *, size_t));
extern	void		  dolog		__P((Char **, struct command *));
# ifdef HAVE_STRUCT_UTMP_UT_HOST
extern	char		 *utmphost	__P((void));
extern	size_t		  utmphostsize	__P((void));
# endif /* HAVE_STRUCT_UTMP_UT_HOST */
#else
# define HAVENOUTMP
#endif

#endif /* _h_tc_decls */
