/*	$KAME: rtsold.h,v 1.19 2003/04/16 09:48:15 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: release/7.0.0/usr.sbin/rtsold/rtsold.h 124525 2004-01-14 17:42:03Z ume $
 */

struct ifinfo {
	struct ifinfo *next;	/* pointer to the next interface */

	struct sockaddr_dl *sdl; /* link-layer address */
	char ifname[IF_NAMESIZE]; /* interface name */
	u_int32_t linkid;	/* link ID of this interface */
	int active;		/* interface status */
	int probeinterval;	/* interval of probe timer (if necessary) */
	int probetimer;		/* rest of probe timer */
	int mediareqok;		/* wheter the IF supports SIOCGIFMEDIA */
	int otherconfig;	/* need a separate protocol for the "other"
				 * configuration */
	int state;
	int probes;
	int dadcount;
	struct timeval timer;
	struct timeval expire;
	int errors;		/* # of errors we've got - detect wedge */

	int racnt;		/* total # of valid RAs it have got */

	size_t rs_datalen;
	u_char *rs_data;
};

/* per interface status */
#define IFS_IDLE	0
#define IFS_DELAY	1
#define IFS_PROBE	2
#define IFS_DOWN	3
#define IFS_TENTATIVE	4

/* rtsold.c */
extern struct timeval tm_max;
extern int dflag;
extern int aflag;
extern char *otherconf_script;
extern int ifconfig __P((char *));
extern void iflist_init __P((void));
struct ifinfo *find_ifinfo __P((int));
void rtsol_timer_update __P((struct ifinfo *));
extern void warnmsg __P((int, const char *, const char *, ...))
     __attribute__((__format__(__printf__, 3, 4)));
extern char **autoifprobe __P((void));

/* if.c */
extern int ifinit __P((void));
extern int interface_up __P((char *));
extern int interface_status __P((struct ifinfo *));
extern int lladdropt_length __P((struct sockaddr_dl *));
extern void lladdropt_fill __P((struct sockaddr_dl *, struct nd_opt_hdr *));
extern struct sockaddr_dl *if_nametosdl __P((char *));
extern int getinet6sysctl __P((int));
extern int setinet6sysctl __P((int, int));

/* rtsol.c */
extern int sockopen __P((void));
extern void sendpacket __P((struct ifinfo *));
extern void rtsol_input __P((int));

/* probe.c */
extern int probe_init __P((void));
extern void defrouter_probe __P((struct ifinfo *));

/* dump.c */
extern void rtsold_dump_file __P((char *));

/* rtsock.c */
extern int rtsock_open __P((void));
extern int rtsock_input __P((int));
