/*-
 * Copyright (c) 2004 Poul-Henning Kamp.  All rights reserved.
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)tty_conf.c	8.4 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/kern/tty_conf.c,v 1.24 2004/07/15 20:47:40 phk Exp $");

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/conf.h>

#ifndef MAXLDISC
#define MAXLDISC 9
#endif

static l_open_t		l_noopen;
static l_close_t	l_noclose;
static l_rint_t		l_norint;
static l_start_t	l_nostart;

/*
 * XXX it probably doesn't matter what the entries other than the l_open
 * entry are here.  The l_nullioctl and ttymodem entries still look fishy.
 * Reconsider the removal of nullmodem anyway.  It was too much like
 * ttymodem, but a completely null version might be useful.
 */

static struct linesw nodisc = {
	.l_open = 	l_noopen,
	.l_close =	l_noclose,
	.l_read = 	l_noread,
	.l_write = 	l_nowrite, 
	.l_ioctl = 	l_nullioctl, 
	.l_rint = 	l_norint, 
	.l_start = 	l_nostart, 
	.l_modem = 	ttymodem
};

static struct linesw termios_disc = {
	.l_open = 	tty_open,
	.l_close =	ttylclose,
	.l_read = 	ttread,
	.l_write = 	ttwrite, 
	.l_ioctl = 	l_nullioctl, 
	.l_rint = 	ttyinput, 
	.l_start = 	ttstart, 
	.l_modem = 	ttymodem
};

#ifdef COMPAT_43
#  define ntty_disc		termios_disc
#else
#  define ntty_disc		nodisc
#endif

struct linesw *linesw[MAXLDISC] = {
	&termios_disc,		/* 0 - termios */
	&nodisc,		/* 1 - defunct */
	&ntty_disc,		/* 2 - NTTYDISC */
	&nodisc,		/* 3 - loadable */
	&nodisc,		/* 4 - SLIPDISC */
	&nodisc,		/* 5 - PPPDISC */
	&nodisc,		/* 6 - NETGRAPHDISC */
	&nodisc,		/* 7 - loadable */
	&nodisc,		/* 8 - loadable */
};

int	nlinesw = sizeof (linesw) / sizeof (linesw[0]);

#define LOADABLE_LDISC 7

/*
 * ldisc_register: Register a line discipline.
 *
 * discipline: Index for discipline to load, or LDISC_LOAD for us to choose.
 * linesw_p:   Pointer to linesw_p.
 *
 * Returns: Index used or -1 on failure.
 */

int
ldisc_register(int discipline, struct linesw *linesw_p)
{
	int slot = -1;

	if (discipline == LDISC_LOAD) {
		int i;
		for (i = LOADABLE_LDISC; i < MAXLDISC; i++)
			if (linesw[i] == &nodisc) {
				slot = i;
				break;
			}
	} else if (discipline >= 0 && discipline < MAXLDISC) {
		slot = discipline;
	}

	if (slot != -1 && linesw_p)
		linesw[slot] = linesw_p;

	return slot;
}

/*
 * ldisc_deregister: Deregister a line discipline obtained with
 * ldisc_register.
 *
 * discipline: Index for discipline to unload.
 */

void
ldisc_deregister(int discipline)
{

	if (discipline < MAXLDISC)
		linesw[discipline] = &nodisc;
}

/*
 * "no" and "null" versions of line discipline functions
 */

static int
l_noopen(struct cdev *dev, struct tty *tp)
{

	return (ENODEV);
}

static int
l_noclose(struct tty *tp, int flag)
{

	return (ENODEV);
}

int
l_noread(struct tty *tp, struct uio *uio, int flag)
{

	return (ENODEV);
}

int
l_nowrite(struct tty *tp, struct uio *uio, int flag)
{

	return (ENODEV);
}

static int
l_norint(int c, struct tty *tp)
{

	return (ENODEV);
}

static int
l_nostart(struct tty *tp)
{

	return (ENODEV);
}

int
l_nullioctl(struct tty *tp, u_long cmd, char *data, int flags, struct thread *td)
{

	return (ENOIOCTL);
}
