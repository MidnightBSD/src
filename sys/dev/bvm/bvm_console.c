/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: release/10.0.0/sys/dev/bvm/bvm_console.c 245678 2013-01-20 03:42:49Z neel $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/sys/dev/bvm/bvm_console.c 245678 2013-01-20 03:42:49Z neel $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/cons.h>
#include <sys/tty.h>
#include <sys/reboot.h>
#include <sys/bus.h>

#include <sys/kdb.h>
#include <ddb/ddb.h>

#ifndef	BVMCONS_POLL_HZ
#define	BVMCONS_POLL_HZ	4
#endif
#define BVMBURSTLEN	16	/* max number of bytes to write in one chunk */

static tsw_open_t bvm_tty_open;
static tsw_close_t bvm_tty_close;
static tsw_outwakeup_t bvm_tty_outwakeup;

static struct ttydevsw bvm_ttydevsw = {
	.tsw_flags	= TF_NOPREFIX,
	.tsw_open	= bvm_tty_open,
	.tsw_close	= bvm_tty_close,
	.tsw_outwakeup	= bvm_tty_outwakeup,
};

static int			polltime;
static struct callout_handle	bvm_timeouthandle
    = CALLOUT_HANDLE_INITIALIZER(&bvm_timeouthandle);

#if defined(KDB)
static int			alt_break_state;
#endif

#define	BVM_CONS_PORT	0x220
static int bvm_cons_port = BVM_CONS_PORT;

#define BVM_CONS_SIG	('b' << 8 | 'v')

static void	bvm_timeout(void *);

static cn_probe_t	bvm_cnprobe;
static cn_init_t	bvm_cninit;
static cn_term_t	bvm_cnterm;
static cn_getc_t	bvm_cngetc;
static cn_putc_t	bvm_cnputc;
static cn_grab_t 	bvm_cngrab;
static cn_ungrab_t 	bvm_cnungrab;

CONSOLE_DRIVER(bvm);

static int
bvm_rcons(u_char *ch)
{
	int c;

	c = inl(bvm_cons_port);
	if (c != -1) {
		*ch = (u_char)c;
		return (0);
	} else
		return (-1);
}

static void
bvm_wcons(u_char ch)
{

	outl(bvm_cons_port, ch);
}

static void
cn_drvinit(void *unused)
{
	struct tty *tp;

	if (bvm_consdev.cn_pri != CN_DEAD &&
	    bvm_consdev.cn_name[0] != '\0') {
		tp = tty_alloc(&bvm_ttydevsw, NULL);
		tty_makedev(tp, NULL, "bvmcons");
	}
}

static int
bvm_tty_open(struct tty *tp)
{
	polltime = hz / BVMCONS_POLL_HZ;
	if (polltime < 1)
		polltime = 1;
	bvm_timeouthandle = timeout(bvm_timeout, tp, polltime);

	return (0);
}

static void
bvm_tty_close(struct tty *tp)
{

	/* XXX Should be replaced with callout_stop(9) */
	untimeout(bvm_timeout, tp, bvm_timeouthandle);
}

static void
bvm_tty_outwakeup(struct tty *tp)
{
	int len, written;
	u_char buf[BVMBURSTLEN];

	for (;;) {
		len = ttydisc_getc(tp, buf, sizeof(buf));
		if (len == 0)
			break;

		written = 0;
		while (written < len)
			bvm_wcons(buf[written++]);
	}
}

static void
bvm_timeout(void *v)
{
	struct	tty *tp;
	int 	c;

	tp = (struct tty *)v;

	tty_lock(tp);
	while ((c = bvm_cngetc(NULL)) != -1)
		ttydisc_rint(tp, c, 0);
	ttydisc_rint_done(tp);
	tty_unlock(tp);

	bvm_timeouthandle = timeout(bvm_timeout, tp, polltime);
}

static void
bvm_cnprobe(struct consdev *cp)
{
	int disabled, port;

	disabled = 0;
	cp->cn_pri = CN_DEAD;

	resource_int_value("bvmconsole", 0, "disabled", &disabled);
	if (!disabled) {
		if (resource_int_value("bvmconsole", 0, "port", &port) == 0)
			bvm_cons_port = port;

		if (inw(bvm_cons_port) == BVM_CONS_SIG)
			cp->cn_pri = CN_REMOTE;
	}
}

static void
bvm_cninit(struct consdev *cp)
{
	int i;
	const char *bootmsg = "Using bvm console.\n";

	if (boothowto & RB_VERBOSE) {
		for (i = 0; i < strlen(bootmsg); i++)
			bvm_cnputc(cp, bootmsg[i]);
	}

	strcpy(cp->cn_name, "bvmcons");
}

static void
bvm_cnterm(struct consdev *cp)
{

}

static int
bvm_cngetc(struct consdev *cp)
{
	unsigned char ch;

	if (bvm_rcons(&ch) == 0) {
#if defined(KDB)
		kdb_alt_break(ch, &alt_break_state);
#endif
		return (ch);
	}

	return (-1);
}

static void
bvm_cnputc(struct consdev *cp, int c)
{

	bvm_wcons(c);
}

static void
bvm_cngrab(struct consdev *cp)
{
}

static void
bvm_cnungrab(struct consdev *cp)
{
}

SYSINIT(cndev, SI_SUB_CONFIGURE, SI_ORDER_MIDDLE, cn_drvinit, NULL);
