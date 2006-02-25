/*
 * Copyright (c) 2003, Trent Nelson, <trent@arpa.com>.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.bin/systat/ifcmds.c,v 1.2 2004/03/09 11:57:27 dwmalone Exp $
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <net/if.h>
#include <net/if_mib.h>
#include <net/if_types.h>	/* For IFT_ETHER */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <float.h>
#include <err.h>

#include "systat.h"
#include "extern.h"
#include "mode.h"
#include "convtbl.h"

int	curscale = SC_AUTO;

static	int selectscale(const char *);

int
ifcmd(const char *cmd, const char *args)
{
	if (prefix(cmd, "scale")) {
		if (*args != '\0' && selectscale(args) != -1)
			;
		else {
			move(CMDLINE, 0);
			clrtoeol();
			addstr("what scale? kbit, kbyte, mbit, mbyte, " \
			       "gbit, gbyte, auto");
		} 
	}
	return 1;
}

static int
selectscale(const char *args)
{
	int	retval = 0;

#define streq(a,b)	(strcmp(a,b) == 0)
	if (streq(args, "default") || streq(args, "auto"))
		curscale = SC_AUTO;
	else if (streq(args, "kbit"))
		curscale = SC_KILOBIT;
	else if (streq(args, "kbyte"))
		curscale = SC_KILOBYTE;
	else if (streq(args, "mbit"))
		curscale = SC_MEGABIT;
	else if (streq(args, "mbyte"))
		curscale = SC_MEGABYTE;
	else if (streq(args, "gbit"))
		curscale = SC_GIGABIT;
	else if (streq(args, "gbyte"))
		curscale = SC_GIGABYTE;
	else
		retval = -1;

	return retval;
}
