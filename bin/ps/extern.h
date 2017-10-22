/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)extern.h	8.3 (Berkeley) 4/2/94
 * $FreeBSD: release/7.0.0/bin/ps/extern.h 130999 2004-06-23 23:48:09Z gad $
 */

struct kinfo;
struct nlist;
struct var;
struct varent;

extern fixpt_t ccpu;
extern int cflag, eval, fscale, nlistread, rawcpu;
extern unsigned long mempages;
extern time_t now;
extern int sumrusage, termwidth, totwidth;
extern STAILQ_HEAD(velisthead, varent) varlist;

__BEGIN_DECLS
void	 arguments(KINFO *, VARENT *);
void	 command(KINFO *, VARENT *);
void	 cputime(KINFO *, VARENT *);
int	 donlist(void);
void	 elapsed(KINFO *, VARENT *);
void	 emulname(KINFO *, VARENT *);
VARENT	*find_varentry(VAR *);
const	 char *fmt_argv(char **, char *, size_t);
double	 getpcpu(const KINFO *);
void	 kvar(KINFO *, VARENT *);
void	 label(KINFO *, VARENT *);
void	 logname(KINFO *, VARENT *);
void	 longtname(KINFO *, VARENT *);
void	 lstarted(KINFO *, VARENT *);
void	 maxrss(KINFO *, VARENT *);
void	 lockname(KINFO *, VARENT *);
void	 mwchan(KINFO *, VARENT *);
void	 nwchan(KINFO *, VARENT *);
void	 pagein(KINFO *, VARENT *);
void	 parsefmt(const char *, int);
void	 pcpu(KINFO *, VARENT *);
void	 pmem(KINFO *, VARENT *);
void	 pri(KINFO *, VARENT *);
void	 printheader(void);
void	 priorityr(KINFO *, VARENT *);
void	 rgroupname(KINFO *, VARENT *);
void	 runame(KINFO *, VARENT *);
void	 rvar(KINFO *, VARENT *);
int	 s_label(KINFO *);
int	 s_rgroupname(KINFO *);
int	 s_runame(KINFO *);
int	 s_uname(KINFO *);
void	 showkey(void);
void	 started(KINFO *, VARENT *);
void	 state(KINFO *, VARENT *);
void	 tdev(KINFO *, VARENT *);
void	 tname(KINFO *, VARENT *);
void	 ucomm(KINFO *, VARENT *);
void	 uname(KINFO *, VARENT *);
void	 upr(KINFO *, VARENT *);
void	 vsize(KINFO *, VARENT *);
void	 wchan(KINFO *, VARENT *);
__END_DECLS
