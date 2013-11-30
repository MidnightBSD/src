/*
 * Copyright (c) 1994 Christopher G. Demetriou
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.sbin/sa/extern.h,v 1.5 2002/07/11 22:11:20 alfred Exp $
 */

#include <sys/types.h>
#include <sys/param.h>
#include <db.h>

/* structures */

struct cmdinfo {
	char		ci_comm[MAXCOMLEN+2];	/* command name (+ '*') */
	u_long		ci_uid;			/* user id */
	u_quad_t	ci_calls;		/* number of calls */
	u_quad_t	ci_etime;		/* elapsed time */
	u_quad_t	ci_utime;		/* user time */
	u_quad_t	ci_stime;		/* system time */
	u_quad_t	ci_mem;			/* memory use */
	u_quad_t	ci_io;			/* number of disk i/o ops */
	u_int		ci_flags;		/* flags; see below */
};
#define	CI_UNPRINTABLE	0x0001			/* unprintable chars in name */

struct userinfo {
	u_long		ui_uid;			/* user id; for consistency */
	u_quad_t	ui_calls;		/* number of invocations */
	u_quad_t	ui_utime;		/* user time */
	u_quad_t	ui_stime;		/* system time */
	u_quad_t	ui_mem;			/* memory use */
	u_quad_t	ui_io;			/* number of disk i/o ops */
};

/* typedefs */

typedef	int (*cmpf_t)(const DBT *, const DBT *);

/* external functions in pdb.c */
int	pacct_init(void);
void	pacct_destroy(void);
int	pacct_add(const struct cmdinfo *);
int	pacct_update(void);
void	pacct_print(void);

/* external functions in usrdb.c */
int	usracct_init(void);
void	usracct_destroy(void);
int	usracct_add(const struct cmdinfo *);
int	usracct_update(void);
void	usracct_print(void);

/* variables */

extern int	aflag, bflag, cflag, dflag, Dflag, fflag, iflag, jflag, kflag;
extern int	Kflag, lflag, mflag, qflag, rflag, sflag, tflag, uflag, vflag;
extern u_quad_t	cutoff;
extern cmpf_t	sa_cmp;

/* some #defines to help with db's stupidity */

#define	DB_CLOSE(db) \
	((*(db)->close)(db))
#define	DB_GET(db, key, data, flags) \
	((*(db)->get)((db), (key), (data), (flags)))
#define	DB_PUT(db, key, data, flags) \
	((*(db)->put)((db), (key), (data), (flags)))
#define	DB_SYNC(db, flags) \
	((*(db)->sync)((db), (flags)))
#define	DB_SEQ(db, key, data, flags) \
	((*(db)->seq)((db), (key), (data), (flags)))
