/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 */

#ifndef lint
static char sccsid[] = "@(#)wwgets.c	8.1 (Berkeley) 6/6/93";
static char rcsid[] =
  "$FreeBSD: release/7.0.0/usr.bin/window/wwgets.c 149456 2005-08-25 14:09:35Z roberto $";
#endif /* not lint */

#include "ww.h"
#include "char.h"

static void
rub(unsigned char c, struct ww *w)
{
	int i;

	for (i = isctrl(c) ? strlen(unctrl(c)) : 1; --i >= 0;)
		(void) wwwrite(w, "\b \b", 3);
}

void
wwgets(char *buf, int n, struct ww *w)
{
	register char *p = buf;
	register int c;
	char uc = w->ww_unctrl;

	w->ww_unctrl = 0;
	for (;;) {
		wwcurtowin(w);
		while ((c = wwgetc()) < 0)
			wwiomux();
#ifdef OLD_TTY
		if (c == wwoldtty.ww_sgttyb.sg_erase)
#else
		if (c == wwoldtty.ww_termios.c_cc[VERASE])
#endif
		{
			if (p > buf)
				rub(*--p, w);
		} else
#ifdef OLD_TTY
		if (c == wwoldtty.ww_sgttyb.sg_kill)
#else
		if (c == wwoldtty.ww_termios.c_cc[VKILL])
#endif
		{
			while (p > buf)
				rub(*--p, w);
		} else
#ifdef OLD_TTY
		if (c == wwoldtty.ww_ltchars.t_werasc)
#else
		if (c == wwoldtty.ww_termios.c_cc[VWERASE])
#endif
		{
			while (--p >= buf && (*p == ' ' || *p == '\t'))
				rub(*p, w);
			while (p >= buf && *p != ' ' && *p != '\t')
				rub(*p--, w);
			p++;
		} else if (c == '\r' || c == '\n') {
			break;
		} else {
			if (p >= buf + n - 1)
				wwputc(ctrl('g'), w);
			else
				if (isctrl(c))
  					wwputs(unctrl(*p++ = c), w);
				else
					wwputc(*p++ = c, w);
		}
	}
	*p = 0;
	w->ww_unctrl = uc;
}
