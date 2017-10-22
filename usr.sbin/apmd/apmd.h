/*-
 * APM (Advanced Power Management) Event Dispatcher
 *
 * Copyright (c) 1999 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
 * Copyright (c) 1999 KOIE Hidetaka <koie@suri.co.jp>
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
 * $FreeBSD: release/7.0.0/usr.sbin/apmd/apmd.h 99800 2002-07-11 18:31:16Z alfred $
 */

#define APMD_CONFIGFILE		"/etc/apmd.conf"
#define APM_CTL_DEVICEFILE	"/dev/apmctl"
#define APM_NORM_DEVICEFILE	"/dev/apm"
#define APMD_PIDFILE		"/var/run/apmd.pid"
#define NICE_INCR		-20

enum {
	EVENT_NOEVENT,
	EVENT_STANDBYREQ,
	EVENT_SUSPENDREQ,
	EVENT_NORMRESUME,
	EVENT_CRITRESUME,
	EVENT_BATTERYLOW,
	EVENT_POWERSTATECHANGE,
	EVENT_UPDATETIME,
	EVENT_CRITSUSPEND,
	EVENT_USERSTANDBYREQ,
	EVENT_USERSUSPENDREQ,
	EVENT_STANDBYRESUME,
	EVENT_CAPABILITIESCHANGE,
	EVENT_MAX
};

struct event_cmd_op {
	int (* act)(void *this);
	void (* dump)(void *this, FILE * fp);
	struct event_cmd * (* clone)(void *this);
	void (* free)(void *this);
};
struct event_cmd {
	struct event_cmd * next;
	size_t len;
	char * name;
	struct event_cmd_op * op;
};
struct event_cmd_exec {
	struct event_cmd evcmd;
	char * line;		/* Command line */
};
struct event_cmd_reject {
	struct event_cmd evcmd;
};

struct event_config {
	const char *name;
	struct event_cmd * cmdlist;
	int rejectable;
};

struct battery_watch_event {
	struct battery_watch_event *next;
	int level;
	enum {
		BATTERY_CHARGING,
		BATTERY_DISCHARGING
	} direction;
	enum {
		BATTERY_MINUTES,
		BATTERY_PERCENT
	} type;
	int done;
	struct event_cmd *cmdlist;
};

	
extern struct event_cmd_op event_cmd_exec_ops;
extern struct event_cmd_op event_cmd_reject_ops;
extern struct event_config events[EVENT_MAX];
extern struct battery_watch_event *battery_watch_list;

extern int register_battery_handlers(
	int level, int direction,
	struct event_cmd *cmdlist);
extern int register_apm_event_handlers(
	bitstr_t bit_decl(evlist, EVENT_MAX),
	struct event_cmd *cmdlist);
extern void free_event_cmd_list(struct event_cmd *p);
