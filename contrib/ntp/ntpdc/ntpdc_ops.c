/*
 * ntpdc_ops.c - subroutines which are called to perform operations by xntpdc
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stddef.h>

#include "ntpdc.h"
#include "ntp_control.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

#include <ctype.h>
#ifdef HAVE_SYS_TIMEX_H
# include <sys/timex.h>
#endif
#if !defined(__bsdi__) && !defined(apollo)
#include <netinet/in.h>
#endif

#include <arpa/inet.h>

/*
 * Declarations for command handlers in here
 */
static	int	checkitems	P((int, FILE *));
static	int	checkitemsize	P((int, int));
static	int	check1item	P((int, FILE *));
static	void	peerlist	P((struct parse *, FILE *));
static	void	peers		P((struct parse *, FILE *));
static void	doconfig	P((struct parse *pcmd, FILE *fp, int mode, int refc));
static	void	dmpeers		P((struct parse *, FILE *));
static	void	dopeers		P((struct parse *, FILE *, int));
static	void	printpeer	P((struct info_peer *, FILE *));
static	void	showpeer	P((struct parse *, FILE *));
static	void	peerstats	P((struct parse *, FILE *));
static	void	loopinfo	P((struct parse *, FILE *));
static	void	sysinfo		P((struct parse *, FILE *));
static	void	sysstats	P((struct parse *, FILE *));
static	void	iostats		P((struct parse *, FILE *));
static	void	memstats	P((struct parse *, FILE *));
static	void	timerstats	P((struct parse *, FILE *));
static	void	addpeer		P((struct parse *, FILE *));
static	void	addserver	P((struct parse *, FILE *));
static	void	addrefclock	P((struct parse *, FILE *));
static	void	broadcast	P((struct parse *, FILE *));
static	void	doconfig	P((struct parse *, FILE *, int, int));
static	void	unconfig	P((struct parse *, FILE *));
static	void	set		P((struct parse *, FILE *));
static	void	sys_clear	P((struct parse *, FILE *));
static	void	doset		P((struct parse *, FILE *, int));
static	void	reslist		P((struct parse *, FILE *));
static	void	new_restrict	P((struct parse *, FILE *));
static	void	unrestrict	P((struct parse *, FILE *));
static	void	delrestrict	P((struct parse *, FILE *));
static	void	do_restrict	P((struct parse *, FILE *, int));
static	void	monlist		P((struct parse *, FILE *));
static	void	reset		P((struct parse *, FILE *));
static	void	preset		P((struct parse *, FILE *));
static	void	readkeys	P((struct parse *, FILE *));
static	void	trustkey	P((struct parse *, FILE *));
static	void	untrustkey	P((struct parse *, FILE *));
static	void	do_trustkey	P((struct parse *, FILE *, int));
static	void	authinfo	P((struct parse *, FILE *));
static	void	traps		P((struct parse *, FILE *));
static	void	addtrap		P((struct parse *, FILE *));
static	void	clrtrap		P((struct parse *, FILE *));
static	void	do_addclr_trap	P((struct parse *, FILE *, int));
static	void	requestkey	P((struct parse *, FILE *));
static	void	controlkey	P((struct parse *, FILE *));
static	void	do_changekey	P((struct parse *, FILE *, int));
static	void	ctlstats	P((struct parse *, FILE *));
static	void	clockstat	P((struct parse *, FILE *));
static	void	fudge		P((struct parse *, FILE *));
static	void	clkbug		P((struct parse *, FILE *));
static	void	kerninfo	P((struct parse *, FILE *));
static  void    get_if_stats    P((struct parse *, FILE *));
static  void    do_if_reload    P((struct parse *, FILE *));

/*
 * Commands we understand.  Ntpdc imports this.
 */
struct xcmd opcmds[] = {
	{ "listpeers",	peerlist,	{ OPT|IP_VERSION, NO, NO, NO },
	  { "-4|-6", "", "", "" },
	  "display list of peers the server knows about [IP Version]" },
	{ "peers",	peers,	{ OPT|IP_VERSION, NO, NO, NO },
	  { "-4|-6", "", "", "" },
	  "display peer summary information [IP Version]" },
	{ "dmpeers",	dmpeers,	{ OPT|IP_VERSION, NO, NO, NO },
	  { "-4|-6", "", "", "" },
	  "display peer summary info the way Dave Mills likes it (IP Version)" },
	{ "showpeer",	showpeer, 	{ NTP_ADD, OPT|NTP_ADD, OPT|NTP_ADD, OPT|NTP_ADD},
	  { "peer_address", "peer2_addr", "peer3_addr", "peer4_addr" },
	  "display detailed information for one or more peers" },
	{ "pstats",	peerstats,	{ NTP_ADD, OPT|NTP_ADD, OPT|NTP_ADD, OPT|NTP_ADD },
	  { "peer_address", "peer2_addr", "peer3_addr", "peer4_addr" },
	  "display statistical information for one or more peers" },
	{ "loopinfo",	loopinfo,	{ OPT|NTP_STR, NO, NO, NO },
	  { "oneline|multiline", "", "", "" },
	  "display loop filter information" },
	{ "sysinfo",	sysinfo,	{ NO, NO, NO, NO },
	  { "", "", "", "" },
	  "display local server information" },
	{ "sysstats",	sysstats,	{ NO, NO, NO, NO },
	  { "", "", "", "" },
	  "display local server statistics" },
	{ "memstats",	memstats,	{ NO, NO, NO, NO },
	  { "", "", "", "" },
	  "display peer memory usage statistics" },
	{ "iostats",	iostats,	{ NO, NO, NO, NO },
	  { "", "", "", "" },
	  "display I/O subsystem statistics" },
	{ "timerstats",	timerstats,	{ NO, NO, NO, NO },
	  { "", "", "", "" },
	  "display event timer subsystem statistics" },
	{ "addpeer",	addpeer,	{ NTP_ADD, OPT|NTP_STR, OPT|NTP_STR, OPT|NTP_STR },
	  { "addr", "keyid", "version", "minpoll#|prefer|burst|iburst|'minpoll N'|'maxpoll N'|'keyid N'|'version N' ..." },
	  "configure a new peer association" },
	{ "addserver",	addserver,	{ NTP_ADD, OPT|NTP_STR, OPT|NTP_STR, OPT|NTP_STR },
	  { "addr", "keyid", "version", "minpoll#|prefer|burst|iburst|'minpoll N'|'maxpoll N'|'keyid N'|'version N' ..." },
	  "configure a new server" },
	{ "addrefclock",addrefclock,	{ NTP_ADD, OPT|NTP_UINT, OPT|NTP_STR, OPT|NTP_STR },
	  { "addr", "mode", "minpoll|prefer", "minpoll|prefer" },
	  "configure a new server" },
	{ "broadcast",	broadcast,	{ NTP_ADD, OPT|NTP_STR, OPT|NTP_STR, OPT|NTP_STR },
	  { "addr", "keyid", "version", "minpoll" },
	  "configure broadcasting time service" },
	{ "unconfig",	unconfig,	{ NTP_ADD, OPT|NTP_ADD, OPT|NTP_ADD, OPT|NTP_ADD },
	  { "peer_address", "peer2_addr", "peer3_addr", "peer4_addr" },
	  "unconfigure existing peer assocations" },
	{ "enable",	set,		{ NTP_STR, OPT|NTP_STR, OPT|NTP_STR, OPT|NTP_STR },
	  { "auth|bclient|monitor|pll|kernel|stats", "...", "...", "..." },
	  "set a system flag (auth, bclient, monitor, pll, kernel, stats)" },
        { "disable",	sys_clear,      { NTP_STR, OPT|NTP_STR, OPT|NTP_STR, OPT|NTP_STR },
	  { "auth|bclient|monitor|pll|kernel|stats", "...", "...", "..." },
	  "clear a system flag (auth, bclient, monitor, pll, kernel, stats)" },
	{ "reslist",	reslist,	{OPT|IP_VERSION, NO, NO, NO },
	  { "-4|-6", "", "", "" },
	  "display the server's restrict list" },
	{ "restrict",	new_restrict,	{ NTP_ADD, NTP_ADD, NTP_STR, OPT|NTP_STR },
	  { "address", "mask",
	    "ntpport|ignore|noserve|notrust|noquery|nomodify|nopeer|version|kod",
	    "..." },
	  "create restrict entry/add flags to entry" },
	{ "unrestrict", unrestrict,	{ NTP_ADD, NTP_ADD, NTP_STR, OPT|NTP_STR },
	  { "address", "mask",
	    "ntpport|ignore|noserve|notrust|noquery|nomodify|nopeer|version|kod",
	    "..." },
	  "remove flags from a restrict entry" },
	{ "delrestrict", delrestrict,	{ NTP_ADD, NTP_ADD, OPT|NTP_STR, NO },
	  { "address", "mask", "ntpport", "" },
	  "delete a restrict entry" },
	{ "monlist",	monlist,	{ OPT|NTP_INT, NO, NO, NO },
	  { "version", "", "", "" },
	  "display data the server's monitor routines have collected" },
	{ "reset",	reset,		{ NTP_STR, OPT|NTP_STR, OPT|NTP_STR, OPT|NTP_STR },
	  { "io|sys|mem|timer|auth|allpeers", "...", "...", "..." },
	  "reset various subsystem statistics counters" },
	{ "preset",	preset,		{ NTP_ADD, OPT|NTP_ADD, OPT|NTP_ADD, OPT|NTP_ADD },
	  { "peer_address", "peer2_addr", "peer3_addr", "peer4_addr" },
	  "reset stat counters associated with particular peer(s)" },
	{ "readkeys",	readkeys,	{ NO, NO, NO, NO },
	  { "", "", "", "" },
	  "request a reread of the keys file and re-init of system keys" },
	{ "trustedkey",	trustkey,	{ NTP_UINT, OPT|NTP_UINT, OPT|NTP_UINT, OPT|NTP_UINT },
	  { "keyid", "keyid", "keyid", "keyid" },
	  "add one or more key ID's to the trusted list" },
	{ "untrustedkey", untrustkey,	{ NTP_UINT, OPT|NTP_UINT, OPT|NTP_UINT, OPT|NTP_UINT },
	  { "keyid", "keyid", "keyid", "keyid" },
	  "remove one or more key ID's from the trusted list" },
	{ "authinfo",	authinfo,	{ NO, NO, NO, NO },
	  { "", "", "", "" },
	  "display the state of the authentication code" },
	{ "traps",	traps,		{ NO, NO, NO, NO },
	  { "", "", "", "" },
	  "display the traps set in the server" },
	{ "addtrap",	addtrap,	{ NTP_ADD, OPT|NTP_UINT, OPT|NTP_ADD, NO },
	  { "address", "port", "interface", "" },
	  "configure a trap in the server" },
	{ "clrtrap",	clrtrap,	{ NTP_ADD, OPT|NTP_UINT, OPT|NTP_ADD, NO },
	  { "address", "port", "interface", "" },
	  "remove a trap (configured or otherwise) from the server" },
	{ "requestkey",	requestkey,	{ NTP_UINT, NO, NO, NO },
	  { "keyid", "", "", "" },
	  "change the keyid the server uses to authenticate requests" },
	{ "controlkey",	controlkey,	{ NTP_UINT, NO, NO, NO },
	  { "keyid", "", "", "" },
	  "change the keyid the server uses to authenticate control messages" },
	{ "ctlstats",	ctlstats,	{ NO, NO, NO, NO },
	  { "", "", "", "" },
	  "display packet count statistics from the control module" },
	{ "clockstat",	clockstat,	{ NTP_ADD, OPT|NTP_ADD, OPT|NTP_ADD, OPT|NTP_ADD },
	  { "address", "address", "address", "address" },
	  "display clock status information" },
	{ "fudge",	fudge,		{ NTP_ADD, NTP_STR, NTP_STR, NO },
	  { "address", "time1|time2|val1|val2|flags", "value", "" },
	  "set/change one of a clock's fudge factors" },
	{ "clkbug",	clkbug,		{ NTP_ADD, OPT|NTP_ADD, OPT|NTP_ADD, OPT|NTP_ADD },
	  { "address", "address", "address", "address" },
	  "display clock debugging information" },
	{ "kerninfo",	kerninfo,	{ NO, NO, NO, NO },
	  { "", "", "", "" },
	  "display the kernel pll/pps variables" },
	{ "ifstats",	get_if_stats,	{ NO, NO, NO, NO },
	  { "", "", "", "" },
	  "list interface statistics" },
	{ "ifreload",	do_if_reload,	{ NO, NO, NO, NO },
	  { "", "", "", "" },
	  "reload interface configuration" },
	{ 0,		0,		{ NO, NO, NO, NO },
	  { "", "", "", "" }, "" }
};

/*
 * For quick string comparisons
 */
#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)


/*
 * checkitems - utility to print a message if no items were returned
 */
static int
checkitems(
	int items,
	FILE *fp
	)
{
	if (items == 0) {
		(void) fprintf(fp, "No data returned in response to query\n");
		return 0;
	}
	return 1;
}


/*
 * checkitemsize - utility to print a message if the item size is wrong
 */
static int
checkitemsize(
	int itemsize,
	int expected
	)
{
	if (itemsize != expected) {
		(void) fprintf(stderr,
			       "***Incorrect item size returned by remote host (%d should be %d)\n",
			       itemsize, expected);
		return 0;
	}
	return 1;
}


/*
 * check1item - check to make sure we have exactly one item
 */
static int
check1item(
	int items,
	FILE *fp
	)
{
	if (items == 0) {
		(void) fprintf(fp, "No data returned in response to query\n");
		return 0;
	}
	if (items > 1) {
		(void) fprintf(fp, "Expected one item in response, got %d\n",
			       items);
		return 0;
	}
	return 1;
}



/*
 * peerlist - get a short list of peers
 */
/*ARGSUSED*/
static void
peerlist(
	struct parse *pcmd,
	FILE *fp
	)
{
	struct info_peer_list *plist;
	struct sockaddr_storage paddr;
	int items;
	int itemsize;
	int res;

again:
	res = doquery(impl_ver, REQ_PEER_LIST, 0, 0, 0, (char *)NULL, &items,
		      &itemsize, (void *)&plist, 0, 
		      sizeof(struct info_peer_list));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res != 0)
	    return;

	if (!checkitems(items, fp))
	    return;

	if (!checkitemsize(itemsize, sizeof(struct info_peer_list)) &&
	    !checkitemsize(itemsize, v4sizeof(struct info_peer_list)))
	    return;

	while (items > 0) {
		memset((char *)&paddr, 0, sizeof(paddr));
		if (plist->v6_flag != 0) {
			GET_INADDR6(paddr) = plist->addr6;
			paddr.ss_family = AF_INET6;
		} else {
			GET_INADDR(paddr) = plist->addr;
			paddr.ss_family = AF_INET;
		}
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
		paddr.ss_len = SOCKLEN(&paddr);
#endif
		if ((pcmd->nargs == 0) ||
		    ((pcmd->argval->ival == 6) && (plist->v6_flag != 0)) ||
		    ((pcmd->argval->ival == 4) && (plist->v6_flag == 0)))
			(void) fprintf(fp, "%-9s %s\n",
				modetoa(plist->hmode),
				nntohost(&paddr));
		plist++;
		items--;
	}
}


/*
 * peers - show peer summary
 */
static void
peers(
	struct parse *pcmd,
	FILE *fp
	)
{
	dopeers(pcmd, fp, 0);
}

/*
 * dmpeers - show peer summary, Dave Mills style
 */
static void
dmpeers(
	struct parse *pcmd,
	FILE *fp
	)
{
	dopeers(pcmd, fp, 1);
}


/*
 * peers - show peer summary
 */
/*ARGSUSED*/
static void
dopeers(
	struct parse *pcmd,
	FILE *fp,
	int dmstyle
	)
{
	struct info_peer_summary *plist;
	struct sockaddr_storage dstadr;
	struct sockaddr_storage srcadr;
	int items;
	int itemsize;
	int ntp_poll;
	int res;
	int c;
	l_fp tempts;

again:
	res = doquery(impl_ver, REQ_PEER_LIST_SUM, 0, 0, 0, (char *)NULL,
		      &items, &itemsize, (void *)&plist, 0, 
		      sizeof(struct info_peer_summary));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res != 0)
	    return;

	if (!checkitems(items, fp))
	    return;

	if (!checkitemsize(itemsize, sizeof(struct info_peer_summary)) &&
	    !checkitemsize(itemsize, v4sizeof(struct info_peer_summary)))
		return;

	(void) fprintf(fp,
		       "     remote           local      st poll reach  delay   offset    disp\n");
	(void) fprintf(fp,
		       "=======================================================================\n");
	while (items > 0) {
		if (!dmstyle) {
			if (plist->flags & INFO_FLAG_SYSPEER)
			    c = '*';
			else if (plist->hmode == MODE_ACTIVE)
			    c = '+';
			else if (plist->hmode == MODE_PASSIVE)
			    c = '-';
			else if (plist->hmode == MODE_CLIENT)
			    c = '=';
			else if (plist->hmode == MODE_BROADCAST)
			    c = '^';
			else if (plist->hmode == MODE_BCLIENT)
			    c = '~';
			else
			    c = ' ';
		} else {
			if (plist->flags & INFO_FLAG_SYSPEER)
			    c = '*';
			else if (plist->flags & INFO_FLAG_SHORTLIST)
			    c = '+';
			else if (plist->flags & INFO_FLAG_SEL_CANDIDATE)
			    c = '.';
			else
			    c = ' ';
		}
		NTOHL_FP(&(plist->offset), &tempts);
		ntp_poll = 1<<max(min3(plist->ppoll, plist->hpoll, NTP_MAXPOLL),
				  NTP_MINPOLL);
		memset((char *)&dstadr, 0, sizeof(dstadr));
		memset((char *)&srcadr, 0, sizeof(srcadr));
		if (plist->v6_flag != 0) {
			GET_INADDR6(dstadr) = plist->dstadr6;
			GET_INADDR6(srcadr) = plist->srcadr6;
			srcadr.ss_family = AF_INET6;
			dstadr.ss_family = AF_INET6;
		} else {
			GET_INADDR(dstadr) = plist->dstadr;
			GET_INADDR(srcadr) = plist->srcadr;
			srcadr.ss_family = AF_INET;
			dstadr.ss_family = AF_INET;
		}
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
		srcadr.ss_len = SOCKLEN(&srcadr);
		dstadr.ss_len = SOCKLEN(&dstadr);
#endif
		if ((pcmd->nargs == 0) ||
		    ((pcmd->argval->ival == 6) && (plist->v6_flag != 0)) ||
		    ((pcmd->argval->ival == 4) && (plist->v6_flag == 0)))
			(void) fprintf(fp,
			    "%c%-15.15s %-15.15s %2d %4d  %3o %7.7s %9.9s %7.7s\n",
			    c, nntohost(&srcadr), stoa(&dstadr),
			    plist->stratum, ntp_poll, plist->reach,
			    fptoa(NTOHS_FP(plist->delay), 5),
			    lfptoa(&tempts, 6),
			    ufptoa(NTOHS_FP(plist->dispersion), 5));
		plist++;
		items--;
	}
}

/* Convert a refid & stratum (in host order) to a string */
static char*
refid_string(
	u_int32 refid,
	int stratum
	)
{
	if (stratum <= 1) {
		static char junk[5];
		junk[4] = 0;
		memmove(junk, (char *)&refid, 4);
		return junk;
	}

	return numtoa(refid);
}

static void
print_pflag(
	    FILE *fp,
	    u_int32 flags
	    )
{
     const char *str;

     if (flags == 0) {
		(void) fprintf(fp, " none\n");
	} else {
		str = "";
		if (flags & INFO_FLAG_SYSPEER) {
			(void) fprintf(fp, " system_peer");
			str = ",";
		}
		if (flags & INFO_FLAG_CONFIG) {
			(void) fprintf(fp, "%s config", str);
			str = ",";
		}
		if (flags & INFO_FLAG_REFCLOCK) {
			(void) fprintf(fp, "%s refclock", str);
			str = ",";
		}
		if (flags & INFO_FLAG_AUTHENABLE) {
			(void) fprintf(fp, "%s auth", str);
			str = ",";
		}
		if (flags & INFO_FLAG_BCLIENT) {
			(void) fprintf(fp, "%s bclient", str);
			str = ",";
		}
		if (flags & INFO_FLAG_PREFER) {
			(void) fprintf(fp, "%s prefer", str);
			str = ",";
		}
		if (flags & INFO_FLAG_IBURST) {
			(void) fprintf(fp, "%s iburst", str);
			str = ",";
		}
		if (flags & INFO_FLAG_BURST) {
			(void) fprintf(fp, "%s burst", str);
		}
		(void) fprintf(fp, "\n");
	}
}
/*
 * printpeer - print detail information for a peer
 */
static void
printpeer(
	register struct info_peer *pp,
	FILE *fp
	)
{
	register int i;
	l_fp tempts;
	struct sockaddr_storage srcadr, dstadr;
	
	memset((char *)&srcadr, 0, sizeof(srcadr));
	memset((char *)&dstadr, 0, sizeof(dstadr));
	if (pp->v6_flag != 0) {
		srcadr.ss_family = AF_INET6;
		dstadr.ss_family = AF_INET6;
		GET_INADDR6(srcadr) = pp->srcadr6;
		GET_INADDR6(dstadr) = pp->dstadr6;
	} else {
		srcadr.ss_family = AF_INET;
		dstadr.ss_family = AF_INET;
		GET_INADDR(srcadr) = pp->srcadr;
		GET_INADDR(dstadr) = pp->dstadr;
	}
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
	srcadr.ss_len = SOCKLEN(&srcadr);
	dstadr.ss_len = SOCKLEN(&dstadr);
#endif
	(void) fprintf(fp, "remote %s, local %s\n",
		       stoa(&srcadr), stoa(&dstadr));
	(void) fprintf(fp, "hmode %s, pmode %s, stratum %d, precision %d\n",
		       modetoa(pp->hmode), modetoa(pp->pmode),
		       pp->stratum, pp->precision);
	
	(void) fprintf(fp,
		       "leap %c%c, refid [%s], rootdistance %s, rootdispersion %s\n",
		       pp->leap & 0x2 ? '1' : '0',
		       pp->leap & 0x1 ? '1' : '0',
		       refid_string(pp->refid, pp->stratum), fptoa(NTOHS_FP(pp->rootdelay), 5),
		       ufptoa(NTOHS_FP(pp->rootdispersion), 5));
	
	(void) fprintf(fp,
		       "ppoll %d, hpoll %d, keyid %lu, version %d, association %u\n",
		       pp->ppoll, pp->hpoll, (u_long)pp->keyid, pp->version, ntohs(pp->associd));

	(void) fprintf(fp,
		       "reach %03o, unreach %d, flash 0x%04x, ",
		       pp->reach, pp->unreach, pp->flash2);

	(void) fprintf(fp, "boffset %s, ttl/mode %d\n",
		       fptoa(NTOHS_FP(pp->estbdelay), 5), pp->ttl);
	
	(void) fprintf(fp, "timer %lds, flags", (long)ntohl(pp->timer));
	print_pflag(fp, pp->flags); 

	NTOHL_FP(&pp->reftime, &tempts);
	(void) fprintf(fp, "reference time:      %s\n",
		       prettydate(&tempts));
	NTOHL_FP(&pp->org, &tempts);
	(void) fprintf(fp, "originate timestamp: %s\n",
		       prettydate(&tempts));
	NTOHL_FP(&pp->rec, &tempts);
	(void) fprintf(fp, "receive timestamp:   %s\n",
		       prettydate(&tempts));
	NTOHL_FP(&pp->xmt, &tempts);
	(void) fprintf(fp, "transmit timestamp:  %s\n",
		       prettydate(&tempts));
	
	(void) fprintf(fp, "filter delay: ");
	for (i = 0; i < NTP_SHIFT; i++) {
		(void) fprintf(fp, " %-8.8s",
			       fptoa(NTOHS_FP(pp->filtdelay[i]), 5));
		if (i == (NTP_SHIFT>>1)-1)
		    (void) fprintf(fp, "\n              ");
	}
	(void) fprintf(fp, "\n");

	(void) fprintf(fp, "filter offset:");
	for (i = 0; i < NTP_SHIFT; i++) {
		NTOHL_FP(&pp->filtoffset[i], &tempts);
		(void) fprintf(fp, " %-8.8s", lfptoa(&tempts, 6));
		if (i == (NTP_SHIFT>>1)-1)
		    (void) fprintf(fp, "\n              ");
	}
	(void) fprintf(fp, "\n");

	(void) fprintf(fp, "filter order: ");
	for (i = 0; i < NTP_SHIFT; i++) {
		(void) fprintf(fp, " %-8d", pp->order[i]);
		if (i == (NTP_SHIFT>>1)-1)
		    (void) fprintf(fp, "\n              ");
	}
	(void) fprintf(fp, "\n");
	

	NTOHL_FP(&pp->offset, &tempts);
	(void) fprintf(fp,
		       "offset %s, delay %s, error bound %s, filter error %s\n",
		       lfptoa(&tempts, 6), fptoa(NTOHS_FP(pp->delay), 5),
		       ufptoa(NTOHS_FP(pp->dispersion), 5),
		       ufptoa(NTOHS_FP(pp->selectdisp), 5));
}


/*
 * showpeer - show detailed information for a peer
 */
static void
showpeer(
	struct parse *pcmd,
	FILE *fp
	)
{
	struct info_peer *pp;
	/* 4 is the maximum number of peers which will fit in a packet */
	struct info_peer_list *pl, plist[min(MAXARGS, 4)];
	int qitems;
	int items;
	int itemsize;
	int res;
	int sendsize;

again:
	if (impl_ver == IMPL_XNTPD)
		sendsize = sizeof(struct info_peer_list);
	else
		sendsize = v4sizeof(struct info_peer_list);

	for (qitems = 0, pl = plist; qitems < min(pcmd->nargs, 4); qitems++) {
		if (pcmd->argval[qitems].netnum.ss_family == AF_INET) {
			pl->addr = GET_INADDR(pcmd->argval[qitems].netnum);
			if (impl_ver == IMPL_XNTPD)
				pl->v6_flag = 0;
		} else {
			if (impl_ver == IMPL_XNTPD_OLD) {
				fprintf(stderr,
				    "***Server doesn't understand IPv6 addresses\n");
				return;
			}
			pl->addr6 = GET_INADDR6(pcmd->argval[qitems].netnum);
			pl->v6_flag = 1;
		}
		pl->port = (u_short)s_port;
		pl->hmode = pl->flags = 0;
		pl = (struct info_peer_list *)((char *)pl + sendsize);
	}

	res = doquery(impl_ver, REQ_PEER_INFO, 0, qitems,
		      sendsize, (char *)plist, &items,
		      &itemsize, (void *)&pp, 0, sizeof(struct info_peer));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res != 0)
	    return;

	if (!checkitems(items, fp))
	    return;

	if (!checkitemsize(itemsize, sizeof(struct info_peer)) &&
	    !checkitemsize(itemsize, v4sizeof(struct info_peer)))
	    return;

	while (items-- > 0) {
		printpeer(pp, fp);
		if (items > 0)
		    (void) fprintf(fp, "\n");
		pp++;
	}
}


/*
 * peerstats - return statistics for a peer
 */
static void
peerstats(
	struct parse *pcmd,
	FILE *fp
	)
{
	struct info_peer_stats *pp;
	/* 4 is the maximum number of peers which will fit in a packet */
	struct info_peer_list *pl, plist[min(MAXARGS, 4)];
	struct sockaddr_storage src, dst;
	int qitems;
	int items;
	int itemsize;
	int res;
	int sendsize;

again:
	if (impl_ver == IMPL_XNTPD)
		sendsize = sizeof(struct info_peer_list);
	else
		sendsize = v4sizeof(struct info_peer_list);

	memset((char *)plist, 0, sizeof(struct info_peer_list) * min(MAXARGS, 4));
	for (qitems = 0, pl = plist; qitems < min(pcmd->nargs, 4); qitems++) {
		if (pcmd->argval[qitems].netnum.ss_family == AF_INET) {
			pl->addr = GET_INADDR(pcmd->argval[qitems].netnum);
			if (impl_ver == IMPL_XNTPD)
				pl->v6_flag = 0;
		} else {
			if (impl_ver == IMPL_XNTPD_OLD) {
				fprintf(stderr,
				    "***Server doesn't understand IPv6 addresses\n");
				return;
			}
			pl->addr6 = GET_INADDR6(pcmd->argval[qitems].netnum);
			pl->v6_flag = 1;
		}
		pl->port = (u_short)s_port;
		pl->hmode = plist[qitems].flags = 0;
		pl = (struct info_peer_list *)((char *)pl + sendsize);
	}

	res = doquery(impl_ver, REQ_PEER_STATS, 0, qitems,
		      sendsize, (char *)plist, &items,
		      &itemsize, (void *)&pp, 0, 
		      sizeof(struct info_peer_stats));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res != 0)
	    return;

	if (!checkitems(items, fp))
	    return;

	if (!checkitemsize(itemsize, sizeof(struct info_peer_stats)) &&
	    !checkitemsize(itemsize, v4sizeof(struct info_peer_stats)))
	    return;

	while (items-- > 0) {
		memset((char *)&src, 0, sizeof(src));
		memset((char *)&dst, 0, sizeof(dst));
		if (pp->v6_flag != 0) {
			GET_INADDR6(src) = pp->srcadr6;
			GET_INADDR6(dst) = pp->dstadr6;
			src.ss_family = AF_INET6;
			dst.ss_family = AF_INET6;
		} else {
			GET_INADDR(src) = pp->srcadr;
			GET_INADDR(dst) = pp->dstadr;
			src.ss_family = AF_INET;
			dst.ss_family = AF_INET;
		}
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
		src.ss_len = SOCKLEN(&src);
		dst.ss_len = SOCKLEN(&dst);
#endif
		(void) fprintf(fp, "remote host:          %s\n",
			       nntohost(&src));
		(void) fprintf(fp, "local interface:      %s\n",
			       stoa(&dst));
		(void) fprintf(fp, "time last received:   %lds\n",
			       (long)ntohl(pp->timereceived));
		(void) fprintf(fp, "time until next send: %lds\n",
			       (long)ntohl(pp->timetosend));
		(void) fprintf(fp, "reachability change:  %lds\n",
			       (long)ntohl(pp->timereachable));
		(void) fprintf(fp, "packets sent:         %ld\n",
			       (long)ntohl(pp->sent));
		(void) fprintf(fp, "packets received:     %ld\n",
			       (long)ntohl(pp->processed));
		(void) fprintf(fp, "bad authentication:   %ld\n",
			       (long)ntohl(pp->badauth));
		(void) fprintf(fp, "bogus origin:         %ld\n",
			       (long)ntohl(pp->bogusorg));
		(void) fprintf(fp, "duplicate:            %ld\n",
			       (long)ntohl(pp->oldpkt));
		(void) fprintf(fp, "bad dispersion:       %ld\n",
			       (long)ntohl(pp->seldisp));
		(void) fprintf(fp, "bad reference time:   %ld\n",
			       (long)ntohl(pp->selbroken));
		(void) fprintf(fp, "candidate order:      %d\n",
			       (int)pp->candidate);
		if (items > 0)
		    (void) fprintf(fp, "\n");
		(void) fprintf(fp, "flags:	");
		print_pflag(fp, ntohs(pp->flags));
	        pp++;
	}
}


/*
 * loopinfo - show loop filter information
 */
static void
loopinfo(
	struct parse *pcmd,
	FILE *fp
	)
{
	struct info_loop *il;
	int items;
	int itemsize;
	int oneline = 0;
	int res;
	l_fp tempts;

	if (pcmd->nargs > 0) {
		if (STREQ(pcmd->argval[0].string, "oneline"))
		    oneline = 1;
		else if (STREQ(pcmd->argval[0].string, "multiline"))
		    oneline = 0;
		else {
			(void) fprintf(stderr, "How many lines?\n");
			return;
		}
	}

again:
	res = doquery(impl_ver, REQ_LOOP_INFO, 0, 0, 0, (char *)NULL,
		      &items, &itemsize, (void *)&il, 0, 
		      sizeof(struct info_loop));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res != 0)
	    return;

	if (!check1item(items, fp))
	    return;

	if (!checkitemsize(itemsize, sizeof(struct info_loop)))
	    return;

	if (oneline) {
		l_fp temp2ts;

		NTOHL_FP(&il->last_offset, &tempts);
		NTOHL_FP(&il->drift_comp, &temp2ts);

		(void) fprintf(fp,
			       "offset %s, frequency %s, time_const %ld, watchdog %ld\n",
			       lfptoa(&tempts, 6),
			       lfptoa(&temp2ts, 3),
			       (long)(int32_t)ntohl((u_long)il->compliance),
			       (u_long)ntohl((u_long)il->watchdog_timer));
	} else {
		NTOHL_FP(&il->last_offset, &tempts);
		(void) fprintf(fp, "offset:               %s s\n",
			       lfptoa(&tempts, 6));
		NTOHL_FP(&il->drift_comp, &tempts);
		(void) fprintf(fp, "frequency:            %s ppm\n",
			       lfptoa(&tempts, 3));
		(void) fprintf(fp, "poll adjust:          %ld\n",
			       (long)(int32_t)ntohl(il->compliance));
		(void) fprintf(fp, "watchdog timer:       %ld s\n",
			       (u_long)ntohl(il->watchdog_timer));
	}
}


/*
 * sysinfo - show current system state
 */
/*ARGSUSED*/
static void
sysinfo(
	struct parse *pcmd,
	FILE *fp
	)
{
	struct info_sys *is;
	struct sockaddr_storage peeraddr;
	int items;
	int itemsize;
	int res;
	l_fp tempts;

again:
	res = doquery(impl_ver, REQ_SYS_INFO, 0, 0, 0, (char *)NULL,
		      &items, &itemsize, (void *)&is, 0,
		      sizeof(struct info_sys));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res != 0)
	    return;

	if (!check1item(items, fp))
	    return;

	if (!checkitemsize(itemsize, sizeof(struct info_sys)) &&
	    !checkitemsize(itemsize, v4sizeof(struct info_sys)))
	    return;

	memset((char *)&peeraddr, 0, sizeof(peeraddr));
	if (is->v6_flag != 0) {
		GET_INADDR6(peeraddr) = is->peer6;
		peeraddr.ss_family = AF_INET6;
	} else {
		GET_INADDR(peeraddr) = is->peer;
		peeraddr.ss_family = AF_INET;
	}
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
	peeraddr.ss_len = SOCKLEN(&peeraddr);
#endif
	(void) fprintf(fp, "system peer:          %s\n", nntohost(&peeraddr));
	(void) fprintf(fp, "system peer mode:     %s\n", modetoa(is->peer_mode));
	(void) fprintf(fp, "leap indicator:       %c%c\n",
		       is->leap & 0x2 ? '1' : '0',
		       is->leap & 0x1 ? '1' : '0');
	(void) fprintf(fp, "stratum:              %d\n", (int)is->stratum);
	(void) fprintf(fp, "precision:            %d\n", (int)is->precision);
	(void) fprintf(fp, "root distance:        %s s\n",
		       fptoa(NTOHS_FP(is->rootdelay), 5));
	(void) fprintf(fp, "root dispersion:      %s s\n",
		       ufptoa(NTOHS_FP(is->rootdispersion), 5));
	(void) fprintf(fp, "reference ID:         [%s]\n",
		       refid_string(is->refid, is->stratum));
	NTOHL_FP(&is->reftime, &tempts);
	(void) fprintf(fp, "reference time:       %s\n", prettydate(&tempts));

	(void) fprintf(fp, "system flags:         ");
	if ((is->flags & (INFO_FLAG_BCLIENT | INFO_FLAG_AUTHENABLE |
	    INFO_FLAG_NTP | INFO_FLAG_KERNEL| INFO_FLAG_CAL |
	    INFO_FLAG_PPS_SYNC | INFO_FLAG_MONITOR | INFO_FLAG_FILEGEN)) == 0) {
		(void) fprintf(fp, "none\n");
	} else {
		if (is->flags & INFO_FLAG_BCLIENT)
		    (void) fprintf(fp, "bclient ");
		if (is->flags & INFO_FLAG_AUTHENTICATE)
		    (void) fprintf(fp, "auth ");
		if (is->flags & INFO_FLAG_MONITOR)
		    (void) fprintf(fp, "monitor ");
		if (is->flags & INFO_FLAG_NTP)
		    (void) fprintf(fp, "ntp ");
		if (is->flags & INFO_FLAG_KERNEL)
		    (void) fprintf(fp, "kernel ");
		if (is->flags & INFO_FLAG_FILEGEN)
		    (void) fprintf(fp, "stats ");
		if (is->flags & INFO_FLAG_CAL)
		    (void) fprintf(fp, "calibrate ");
		if (is->flags & INFO_FLAG_PPS_SYNC)
		    (void) fprintf(fp, "pps ");
		(void) fprintf(fp, "\n");
	}
	(void) fprintf(fp, "jitter:               %s s\n",
		       fptoa(ntohl(is->frequency), 6));
	(void) fprintf(fp, "stability:            %s ppm\n",
		       ufptoa(ntohl(is->stability), 3));
	(void) fprintf(fp, "broadcastdelay:       %s s\n",
		       fptoa(NTOHS_FP(is->bdelay), 6));
	NTOHL_FP(&is->authdelay, &tempts);
	(void) fprintf(fp, "authdelay:            %s s\n", lfptoa(&tempts, 6));
}


/*
 * sysstats - print system statistics
 */
/*ARGSUSED*/
static void
sysstats(
	struct parse *pcmd,
	FILE *fp
	)
{
	struct info_sys_stats *ss;
	int items;
	int itemsize;
	int res;

again:
	res = doquery(impl_ver, REQ_SYS_STATS, 0, 0, 0, (char *)NULL,
		      &items, &itemsize, (void *)&ss, 0, 
		      sizeof(struct info_sys_stats));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res != 0)
	    return;

	if (!check1item(items, fp))
	    return;

	if (itemsize != sizeof(struct info_sys_stats) &&
	    itemsize != sizeof(struct old_info_sys_stats)) {
		/* issue warning according to new structure size */
		checkitemsize(itemsize, sizeof(struct info_sys_stats));
		return;
	}
	fprintf(fp, "time since restart:     %ld\n",
	       (u_long)ntohl(ss->timeup));
	fprintf(fp, "time since reset:       %ld\n",
		(u_long)ntohl(ss->timereset));
        fprintf(fp, "packets received:       %ld\n",
		(u_long)ntohl(ss->received));
	fprintf(fp, "packets processed:      %ld\n",
		(u_long)ntohl(ss->processed));
	fprintf(fp, "current version:        %ld\n",
	       (u_long)ntohl(ss->newversionpkt));
	fprintf(fp, "previous version:       %ld\n",
	       (u_long)ntohl(ss->oldversionpkt));
	fprintf(fp, "bad version:            %ld\n",
	       (u_long)ntohl(ss->unknownversion));
	fprintf(fp, "access denied:          %ld\n",
		(u_long)ntohl(ss->denied));
	fprintf(fp, "bad length or format:   %ld\n",
	       (u_long)ntohl(ss->badlength));
	fprintf(fp, "bad authentication:     %ld\n",
	       (u_long)ntohl(ss->badauth));
	if (itemsize != sizeof(struct info_sys_stats))
	    return;
	
	fprintf(fp, "rate exceeded:          %ld\n",
	       (u_long)ntohl(ss->limitrejected));
}



/*
 * iostats - print I/O statistics
 */
/*ARGSUSED*/
static void
iostats(
	struct parse *pcmd,
	FILE *fp
	)
{
	struct info_io_stats *io;
	int items;
	int itemsize;
	int res;

again:
	res = doquery(impl_ver, REQ_IO_STATS, 0, 0, 0, (char *)NULL,
		      &items, &itemsize, (void *)&io, 0, 
		      sizeof(struct info_io_stats));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res != 0)
	    return;

	if (!check1item(items, fp))
	    return;

	if (!checkitemsize(itemsize, sizeof(struct info_io_stats)))
	    return;

	(void) fprintf(fp, "time since reset:     %ld\n",
		       (u_long)ntohl(io->timereset));
	(void) fprintf(fp, "receive buffers:      %d\n",
		       ntohs(io->totalrecvbufs));
	(void) fprintf(fp, "free receive buffers: %d\n",
		       ntohs(io->freerecvbufs));
	(void) fprintf(fp, "used receive buffers: %d\n",
		       ntohs(io->fullrecvbufs));
	(void) fprintf(fp, "low water refills:    %d\n",
		       ntohs(io->lowwater));
	(void) fprintf(fp, "dropped packets:      %ld\n",
		       (u_long)ntohl(io->dropped));
	(void) fprintf(fp, "ignored packets:      %ld\n",
		       (u_long)ntohl(io->ignored));
	(void) fprintf(fp, "received packets:     %ld\n",
		       (u_long)ntohl(io->received));
	(void) fprintf(fp, "packets sent:         %ld\n",
		       (u_long)ntohl(io->sent));
	(void) fprintf(fp, "packets not sent:     %ld\n",
		       (u_long)ntohl(io->notsent));
	(void) fprintf(fp, "interrupts handled:   %ld\n",
		       (u_long)ntohl(io->interrupts));
	(void) fprintf(fp, "received by int:      %ld\n",
		       (u_long)ntohl(io->int_received));
}


/*
 * memstats - print peer memory statistics
 */
/*ARGSUSED*/
static void
memstats(
	struct parse *pcmd,
	FILE *fp
	)
{
	struct info_mem_stats *mem;
	int i;
	int items;
	int itemsize;
	int res;

again:
	res = doquery(impl_ver, REQ_MEM_STATS, 0, 0, 0, (char *)NULL,
		      &items, &itemsize, (void *)&mem, 0, 
		      sizeof(struct info_mem_stats));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res != 0)
	    return;

	if (!check1item(items, fp))
	    return;

	if (!checkitemsize(itemsize, sizeof(struct info_mem_stats)))
	    return;

	(void) fprintf(fp, "time since reset:     %ld\n",
		       (u_long)ntohl(mem->timereset));
	(void) fprintf(fp, "total peer memory:    %d\n",
		       ntohs(mem->totalpeermem));
	(void) fprintf(fp, "free peer memory:     %d\n",
		       ntohs(mem->freepeermem));
	(void) fprintf(fp, "calls to findpeer:    %ld\n",
		       (u_long)ntohl(mem->findpeer_calls));
	(void) fprintf(fp, "new peer allocations: %ld\n",
		       (u_long)ntohl(mem->allocations));
	(void) fprintf(fp, "peer demobilizations: %ld\n",
		       (u_long)ntohl(mem->demobilizations));

	(void) fprintf(fp, "hash table counts:   ");
	for (i = 0; i < NTP_HASH_SIZE; i++) {
		(void) fprintf(fp, "%4d", (int)mem->hashcount[i]);
		if ((i % 8) == 7 && i != (NTP_HASH_SIZE-1)) {
			(void) fprintf(fp, "\n                     ");
		}
	}
	(void) fprintf(fp, "\n");
}



/*
 * timerstats - print timer statistics
 */
/*ARGSUSED*/
static void
timerstats(
	struct parse *pcmd,
	FILE *fp
	)
{
	struct info_timer_stats *tim;
	int items;
	int itemsize;
	int res;

again:
	res = doquery(impl_ver, REQ_TIMER_STATS, 0, 0, 0, (char *)NULL,
		      &items, &itemsize, (void *)&tim, 0, 
		      sizeof(struct info_timer_stats));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res != 0)
	    return;

	if (!check1item(items, fp))
	    return;

	if (!checkitemsize(itemsize, sizeof(struct info_timer_stats)))
	    return;

	(void) fprintf(fp, "time since reset:  %ld\n",
		       (u_long)ntohl(tim->timereset));
	(void) fprintf(fp, "alarms handled:    %ld\n",
		       (u_long)ntohl(tim->alarms));
	(void) fprintf(fp, "alarm overruns:    %ld\n",
		       (u_long)ntohl(tim->overflows));
	(void) fprintf(fp, "calls to transmit: %ld\n",
		       (u_long)ntohl(tim->xmtcalls));
}


/*
 * addpeer - configure an active mode association
 */
static void
addpeer(
	struct parse *pcmd,
	FILE *fp
	)
{
	doconfig(pcmd, fp, MODE_ACTIVE, 0);
}


/*
 * addserver - configure a client mode association
 */
static void
addserver(
	struct parse *pcmd,
	FILE *fp
	)
{
	doconfig(pcmd, fp, MODE_CLIENT, 0);
}

/*
 * addrefclock - configure a reference clock association
 */
static void
addrefclock(
	struct parse *pcmd,
	FILE *fp
	)
{
	doconfig(pcmd, fp, MODE_CLIENT, 1);
}

/*
 * broadcast - configure a broadcast mode association
 */
static void
broadcast(
	struct parse *pcmd,
	FILE *fp
	)
{
	doconfig(pcmd, fp, MODE_BROADCAST, 0);
}


/*
 * config - configure a new peer association
 */
static void
doconfig(
	struct parse *pcmd,
	FILE *fp,
	int mode,
        int refc
	)
{
	struct conf_peer cpeer;
	int items;
	int itemsize;
	char *dummy;
	u_long keyid;
	u_int version;
	u_char minpoll;
	u_char maxpoll;
	u_int flags;
	u_char cmode;
	int res;
	int sendsize;
	int numtyp;

again:
	keyid = 0;
	version = 3;
	flags = 0;
	res = 0;
	cmode = 0;
	minpoll = NTP_MINDPOLL;
	maxpoll = NTP_MAXDPOLL;
	numtyp = 1;
	if (refc)
	     numtyp = 5;

	if (impl_ver == IMPL_XNTPD)
		sendsize = sizeof(struct conf_peer);
	else
		sendsize = v4sizeof(struct conf_peer);

	items = 1;
	while (pcmd->nargs > items) {
		if (STREQ(pcmd->argval[items].string, "prefer"))
		    flags |= CONF_FLAG_PREFER;
		else if (STREQ(pcmd->argval[items].string, "burst"))
		    flags |= CONF_FLAG_BURST;
		else if (STREQ(pcmd->argval[items].string, "dynamic"))
		    (void) fprintf(fp, "Warning: the \"dynamic\" keyword has been obsoleted and will be removed in the next release\n"); 
		else if (STREQ(pcmd->argval[items].string, "iburst"))
		    flags |= CONF_FLAG_IBURST;
		else if (!refc && STREQ(pcmd->argval[items].string, "keyid"))
		    numtyp = 1;
		else if (!refc && STREQ(pcmd->argval[items].string, "version"))
		    numtyp = 2;
		else if (STREQ(pcmd->argval[items].string, "minpoll"))
		    numtyp = 3;
		else if (STREQ(pcmd->argval[items].string, "maxpoll"))
		    numtyp = 4;
		else {
		        long val;
			if (!atoint(pcmd->argval[items].string, &val))
			     numtyp = 0;				  
			switch (numtyp) {
			case 1:
			     keyid = val;
			     numtyp = 2;				  
			     break;
			     
			case 2:
			     version = (u_int) val;
			     numtyp = 0;				  
			     break;

			case 3:
			     minpoll = (u_char)val;
			     numtyp = 0;				  
			     break;

			case 4:
			     maxpoll = (u_char)val;
			     numtyp = 0;				  
			     break;

			case 5:
			     cmode = (u_char)val;
			     numtyp = 0;				  
			     break;

			default:
			     (void) fprintf(fp, "*** '%s' not understood\n",
					    pcmd->argval[items].string);
			     res++;
			     numtyp = 0;				  
			}
			if (val < 0) {
			     (void) fprintf(stderr,
				     "***Value '%s' should be unsigned\n",
				      pcmd->argval[items].string);
			     res++;
			}
		   }
	     items++;
	}
	if (keyid > 0)
	     flags |= CONF_FLAG_AUTHENABLE;
	if (version > NTP_VERSION ||
	    version < NTP_OLDVERSION) {
	     (void)fprintf(fp, "***invalid version number: %u\n",
			   version);
	     res++;
	}
	if (minpoll < NTP_MINPOLL || minpoll > NTP_MAXPOLL || 
	    maxpoll < NTP_MINPOLL || maxpoll > NTP_MAXPOLL || 
	    minpoll > maxpoll) {
	     (void) fprintf(fp, "***min/max-poll must be within %d..%d\n",
			    NTP_MINPOLL, NTP_MAXPOLL);
	     res++;
	}					

	if (res)
	    return;

	memset((void *)&cpeer, 0, sizeof(cpeer));

	if (pcmd->argval[0].netnum.ss_family == AF_INET) {
		cpeer.peeraddr = GET_INADDR(pcmd->argval[0].netnum);
		if (impl_ver == IMPL_XNTPD)
			cpeer.v6_flag = 0;
	} else {
		if (impl_ver == IMPL_XNTPD_OLD) {
			fprintf(stderr,
			    "***Server doesn't understand IPv6 addresses\n");
			return;
		}
		cpeer.peeraddr6 = GET_INADDR6(pcmd->argval[0].netnum);
		cpeer.v6_flag = 1;
	}
	cpeer.hmode = (u_char) mode;
	cpeer.keyid = keyid;
	cpeer.version = (u_char) version;
	cpeer.minpoll = minpoll;
	cpeer.maxpoll = maxpoll;
	cpeer.flags = (u_char)flags;
	cpeer.ttl = cmode;

	res = doquery(impl_ver, REQ_CONFIG, 1, 1,
		      sendsize, (char *)&cpeer, &items,
		      &itemsize, &dummy, 0, sizeof(struct conf_peer));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res == INFO_ERR_FMT) {
		(void) fprintf(fp,
		    "***Retrying command with old conf_peer size\n");
		res = doquery(impl_ver, REQ_CONFIG, 1, 1,
			      sizeof(struct old_conf_peer), (char *)&cpeer,
			      &items, &itemsize, &dummy, 0,
			      sizeof(struct conf_peer));
	}
	if (res == 0)
	    (void) fprintf(fp, "done!\n");
	return;
}


/*
 * unconfig - unconfigure some associations
 */
static void
unconfig(
	struct parse *pcmd,
	FILE *fp
	)
{
	/* 8 is the maximum number of peers which will fit in a packet */
	struct conf_unpeer *pl, plist[min(MAXARGS, 8)];
	int qitems;
	int items;
	int itemsize;
	char *dummy;
	int res;
	int sendsize;

again:
	if (impl_ver == IMPL_XNTPD)
		sendsize = sizeof(struct conf_unpeer);
	else
		sendsize = v4sizeof(struct conf_unpeer);

	for (qitems = 0, pl = plist; qitems < min(pcmd->nargs, 8); qitems++) {
		if (pcmd->argval[0].netnum.ss_family == AF_INET) {
			pl->peeraddr = GET_INADDR(pcmd->argval[qitems].netnum);
			if (impl_ver == IMPL_XNTPD)
				pl->v6_flag = 0;
		} else {
			if (impl_ver == IMPL_XNTPD_OLD) {
				fprintf(stderr,
				    "***Server doesn't understand IPv6 addresses\n");
				return;
			}
			pl->peeraddr6 =
			    GET_INADDR6(pcmd->argval[qitems].netnum);
			pl->v6_flag = 1;
		}
		pl = (struct conf_unpeer *)((char *)pl + sendsize);
	}

	res = doquery(impl_ver, REQ_UNCONFIG, 1, qitems,
		      sendsize, (char *)plist, &items,
		      &itemsize, &dummy, 0, sizeof(struct conf_unpeer));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res == 0)
	    (void) fprintf(fp, "done!\n");
}


/*
 * set - set some system flags
 */
static void
set(
	struct parse *pcmd,
	FILE *fp
	)
{
	doset(pcmd, fp, REQ_SET_SYS_FLAG);
}


/*
 * clear - clear some system flags
 */
static void
sys_clear(
	struct parse *pcmd,
	FILE *fp
	)
{
	doset(pcmd, fp, REQ_CLR_SYS_FLAG);
}


/*
 * doset - set/clear system flags
 */
static void
doset(
	struct parse *pcmd,
	FILE *fp,
	int req
	)
{
	/* 8 is the maximum number of peers which will fit in a packet */
	struct conf_sys_flags sys;
	int items;
	int itemsize;
	char *dummy;
	int res;

	sys.flags = 0;
	res = 0;
	for (items = 0; items < pcmd->nargs; items++) {
		if (STREQ(pcmd->argval[items].string, "auth"))
			sys.flags |= SYS_FLAG_AUTH;
		else if (STREQ(pcmd->argval[items].string, "bclient"))
			sys.flags |= SYS_FLAG_BCLIENT;
		else if (STREQ(pcmd->argval[items].string, "calibrate"))
			sys.flags |= SYS_FLAG_CAL;
		else if (STREQ(pcmd->argval[items].string, "kernel"))
			sys.flags |= SYS_FLAG_KERNEL;
		else if (STREQ(pcmd->argval[items].string, "monitor"))
			sys.flags |= SYS_FLAG_MONITOR;
		else if (STREQ(pcmd->argval[items].string, "ntp"))
			sys.flags |= SYS_FLAG_NTP;
		else if (STREQ(pcmd->argval[items].string, "pps"))
			sys.flags |= SYS_FLAG_PPS;
		else if (STREQ(pcmd->argval[items].string, "stats"))
			sys.flags |= SYS_FLAG_FILEGEN;
		else {
			(void) fprintf(fp, "Unknown flag %s\n",
			    pcmd->argval[items].string);
			res = 1;
		}
	}

	sys.flags = htonl(sys.flags);
	if (res || sys.flags == 0)
	    return;

again:
	res = doquery(impl_ver, req, 1, 1,
		      sizeof(struct conf_sys_flags), (char *)&sys, &items,
		      &itemsize, &dummy, 0, sizeof(struct conf_sys_flags));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res == 0)
	    (void) fprintf(fp, "done!\n");
}


/*
 * data for printing/interrpreting the restrict flags
 */
struct resflags {
  const char *str;
	int bit;
};

/* XXX: HMS: we apparently don't report set bits we do not recognize. */

static struct resflags resflagsV2[] = {
	{ "ignore",	0x001 },
	{ "noserve",	0x002 },
	{ "notrust",	0x004 },
	{ "noquery",	0x008 },
	{ "nomodify",	0x010 },
	{ "nopeer",	0x020 },
	{ "notrap",	0x040 },
	{ "lptrap",	0x080 },
	{ "limited",	0x100 },
	{ "",		0 }
};

static struct resflags resflagsV3[] = {
	{ "ignore",	RES_IGNORE },
	{ "noserve",	RES_DONTSERVE },
	{ "notrust",	RES_DONTTRUST },
	{ "noquery",	RES_NOQUERY },
	{ "nomodify",	RES_NOMODIFY },
	{ "nopeer",	RES_NOPEER },
	{ "notrap",	RES_NOTRAP },
	{ "lptrap",	RES_LPTRAP },
	{ "limited",	RES_LIMITED },
	{ "version",	RES_VERSION },
	{ "kod",	RES_DEMOBILIZE },
	{ "timeout",	RES_TIMEOUT },

	{ "",		0 }
};

static struct resflags resmflags[] = {
	{ "ntpport",	RESM_NTPONLY },
	{ "interface",	RESM_INTERFACE },
	{ "",		0 }
};


/*
 * reslist - obtain and print the server's restrict list
 */
/*ARGSUSED*/
static void
reslist(
	struct parse *pcmd,
	FILE *fp
	)
{
	struct info_restrict *rl;
	struct sockaddr_storage resaddr;
	struct sockaddr_storage maskaddr;
	int items;
	int itemsize;
	int res;
	int skip;
	char *addr;
	char *mask;
	struct resflags *rf;
	u_int32 count;
	u_short flags;
	u_short mflags;
	char flagstr[300];
	static const char *comma = ", ";

again:
	res = doquery(impl_ver, REQ_GET_RESTRICT, 0, 0, 0, (char *)NULL,
		      &items, &itemsize, (void *)&rl, 0, 
		      sizeof(struct info_restrict));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res != 0)
	    return;

	if (!checkitems(items, fp))
	    return;

	if (!checkitemsize(itemsize, sizeof(struct info_restrict)) &&
	    !checkitemsize(itemsize, v4sizeof(struct info_restrict)))
	    return;

	(void) fprintf(fp,
	       "   address          mask            count        flags\n");
	(void) fprintf(fp,
		       "=====================================================================\n");

	while (items > 0) {
		memset((char *)&resaddr, 0, sizeof(resaddr));
		memset((char *)&maskaddr, 0, sizeof(maskaddr));
		if (rl->v6_flag != 0) {
			GET_INADDR6(resaddr) = rl->addr6;
			GET_INADDR6(maskaddr) = rl->mask6;
			resaddr.ss_family = AF_INET6;
			maskaddr.ss_family = AF_INET6;
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
			resaddr.ss_len = SOCKLEN(&resaddr);
#endif
			addr = nntohost(&resaddr);
		} else {
			GET_INADDR(resaddr) = rl->addr;
			GET_INADDR(maskaddr) = rl->mask;
			resaddr.ss_family = AF_INET;
			maskaddr.ss_family = AF_INET;
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
			resaddr.ss_len = SOCKLEN(&resaddr);
#endif
			if ((rl->mask == (u_int32)0xffffffff))
		    		addr = nntohost(&resaddr);
			else
				addr = stoa(&resaddr);
		}
		mask = stoa(&maskaddr);
		skip = 1;
		if ((pcmd->nargs == 0) ||
		    ((pcmd->argval->ival == 6) && (rl->v6_flag != 0)) ||
		    ((pcmd->argval->ival == 4) && (rl->v6_flag == 0)))
			skip = 0;
		count = ntohl(rl->count);
		flags = ntohs(rl->flags);
		mflags = ntohs(rl->mflags);
		flagstr[0] = '\0';

		res = 1;
		rf = &resmflags[0];
		while (rf->bit != 0) {
			if (mflags & rf->bit) {
				if (!res)
				    (void) strcat(flagstr, comma);
				res = 0;
				(void) strcat(flagstr, rf->str);
			}
			rf++;
		}

		rf = (impl_ver == IMPL_XNTPD_OLD)
		     ? &resflagsV2[0]
		     : &resflagsV3[0]
		     ;
		while (rf->bit != 0) {
			if (flags & rf->bit) {
				if (!res)
				    (void) strcat(flagstr, comma);
				res = 0;
				(void) strcat(flagstr, rf->str);
			}
			rf++;
		}

		if (flagstr[0] == '\0')
		    (void) strcpy(flagstr, "none");

		if (!skip)
			(void) fprintf(fp, "%-15.15s %-15.15s %9ld  %s\n",
					addr, mask, (u_long)count, flagstr);
		rl++;
		items--;
	}
}



/*
 * new_restrict - create/add a set of restrictions
 */
static void
new_restrict(
	struct parse *pcmd,
	FILE *fp
	)
{
	do_restrict(pcmd, fp, REQ_RESADDFLAGS);
}


/*
 * unrestrict - remove restriction flags from existing entry
 */
static void
unrestrict(
	struct parse *pcmd,
	FILE *fp
	)
{
	do_restrict(pcmd, fp, REQ_RESSUBFLAGS);
}


/*
 * delrestrict - delete an existing restriction
 */
static void
delrestrict(
	struct parse *pcmd,
	FILE *fp
	)
{
	do_restrict(pcmd, fp, REQ_UNRESTRICT);
}


/*
 * do_restrict - decode commandline restrictions and make the request
 */
static void
do_restrict(
	struct parse *pcmd,
	FILE *fp,
	int req_code
	)
{
	struct conf_restrict cres;
	int items;
	int itemsize;
	char *dummy;
	u_int32 num;
	u_long bit;
	int i;
	int res;
	int err;
	int sendsize;

	/* Initialize cres */
	cres.addr = 0;
	cres.mask = 0;
	cres.flags = 0;
	cres.mflags = 0;
	cres.v6_flag = 0;

again:
	if (impl_ver == IMPL_XNTPD)
		sendsize = sizeof(struct conf_restrict);
	else
		sendsize = v4sizeof(struct conf_restrict);

	if (pcmd->argval[0].netnum.ss_family == AF_INET) {
		cres.addr = GET_INADDR(pcmd->argval[0].netnum);
		cres.mask = GET_INADDR(pcmd->argval[1].netnum);
		if (impl_ver == IMPL_XNTPD)
			cres.v6_flag = 0;
	} else {
		if (impl_ver == IMPL_XNTPD_OLD) {
			fprintf(stderr,
			    "***Server doesn't understand IPv6 addresses\n");
			return;
		}
		cres.addr6 = GET_INADDR6(pcmd->argval[0].netnum);
		cres.v6_flag = 1;
	}
	cres.flags = 0;
	cres.mflags = 0;
	err = 0;
	for (res = 2; res < pcmd->nargs; res++) {
		if (STREQ(pcmd->argval[res].string, "ntpport")) {
			cres.mflags |= RESM_NTPONLY;
		} else {
			for (i = 0; resflagsV3[i].bit != 0; i++) {
				if (STREQ(pcmd->argval[res].string,
					  resflagsV3[i].str))
				    break;
			}
			if (resflagsV3[i].bit != 0) {
				cres.flags |= resflagsV3[i].bit;
				if (req_code == REQ_UNRESTRICT) {
					(void) fprintf(fp,
						       "Flag %s inappropriate\n",
						       resflagsV3[i].str);
					err++;
				}
			} else {
				(void) fprintf(fp, "Unknown flag %s\n",
					       pcmd->argval[res].string);
				err++;
			}
		}
	}
	cres.flags = htons(cres.flags);
	cres.mflags = htons(cres.mflags);

	/*
	 * Make sure mask for default address is zero.  Otherwise,
	 * make sure mask bits are contiguous.
	 */
	if (pcmd->argval[0].netnum.ss_family == AF_INET) {
		if (cres.addr == 0) {
			cres.mask = 0;
		} else {
			num = ntohl(cres.mask);
			for (bit = 0x80000000; bit != 0; bit >>= 1)
			    if ((num & bit) == 0)
				break;
			for ( ; bit != 0; bit >>= 1)
			    if ((num & bit) != 0)
				break;
			if (bit != 0) {
				(void) fprintf(fp, "Invalid mask %s\n",
					       numtoa(cres.mask));
				err++;
			}
		}
	} else {
		/* XXX IPv6 sanity checking stuff */
	}

	if (err)
	    return;

	res = doquery(impl_ver, req_code, 1, 1,
		      sendsize, (char *)&cres, &items,
		      &itemsize, &dummy, 0, sizeof(struct conf_restrict));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res == 0)
	    (void) fprintf(fp, "done!\n");
	return;
}


/*
 * monlist - obtain and print the server's monitor data
 */
/*ARGSUSED*/
static void
monlist(
	struct parse *pcmd,
	FILE *fp
	)
{
	char *struct_star;
	struct sockaddr_storage addr;
	struct sockaddr_storage dstadr;
	int items;
	int itemsize;
	int res;
	int version = -1;

	if (pcmd->nargs > 0) {
		version = pcmd->argval[0].ival;
	}

again:
	res = doquery(impl_ver,
		      (version == 1 || version == -1) ? REQ_MON_GETLIST_1 :
		      REQ_MON_GETLIST, 0, 0, 0, (char *)NULL,
		      &items, &itemsize, &struct_star,
		      (version < 0) ? (1 << INFO_ERR_REQ) : 0, 
		      sizeof(struct info_monitor_1));

	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res == INFO_ERR_REQ && version < 0) 
	    res = doquery(impl_ver, REQ_MON_GETLIST, 0, 0, 0, (char *)NULL,
			  &items, &itemsize, &struct_star, 0, 
			  sizeof(struct info_monitor));
	
	if (res != 0)
	    return;

	if (!checkitems(items, fp))
	    return;

	if (itemsize == sizeof(struct info_monitor_1) ||
	    itemsize == v4sizeof(struct info_monitor_1)) {
		struct info_monitor_1 *ml = (struct info_monitor_1 *) struct_star;

		(void) fprintf(fp,
			       "remote address          port local address      count m ver code avgint  lstint\n");
		(void) fprintf(fp,
			       "===============================================================================\n");
		while (items > 0) {
			memset((char *)&addr, 0, sizeof(addr));
			memset((char *)&dstadr, 0, sizeof(dstadr));
			if (ml->v6_flag != 0) {
				GET_INADDR6(addr) = ml->addr6;
				addr.ss_family = AF_INET6;
				GET_INADDR6(dstadr) = ml->daddr6;
				dstadr.ss_family = AF_INET6;
			} else {
				GET_INADDR(addr) = ml->addr;
				addr.ss_family = AF_INET;
				GET_INADDR(dstadr) = ml->daddr;
				dstadr.ss_family = AF_INET;
			}
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
			addr.ss_len = SOCKLEN(&addr);
			dstadr.ss_len = SOCKLEN(&dstadr);
#endif
			if ((pcmd->nargs == 0) ||
			    ((pcmd->argval->ival == 6) && (ml->v6_flag != 0)) ||
			    ((pcmd->argval->ival == 4) && (ml->v6_flag == 0)))
				(void) fprintf(fp, 
				    "%-22.22s %5d %-15s %8ld %1d %1d %6lx %6lu %7lu\n",
				    nntohost(&addr), 
				    ntohs(ml->port),
				    stoa(&dstadr),
				    (u_long)ntohl(ml->count),
				    ml->mode,
				    ml->version,
				    (u_long)ntohl(ml->lastdrop),
				    (u_long)ntohl(ml->lasttime),
				    (u_long)ntohl(ml->firsttime));
			ml++;
			items--;
		}
	} else if (itemsize == sizeof(struct info_monitor) ||
	    itemsize == v4sizeof(struct info_monitor)) {
		struct info_monitor *ml = (struct info_monitor *) struct_star;

		(void) fprintf(fp,
			       "     address               port     count mode ver code avgint  lstint\n");
		(void) fprintf(fp,
			       "===============================================================================\n");
		while (items > 0) {
			memset((char *)&dstadr, 0, sizeof(dstadr));
			if (ml->v6_flag != 0) {
				GET_INADDR6(dstadr) = ml->addr6;
				dstadr.ss_family = AF_INET6;
			} else {
				GET_INADDR(dstadr) = ml->addr;
				dstadr.ss_family = AF_INET;
			}
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
			dstadr.ss_len = SOCKLEN(&dstadr);
#endif
			if ((pcmd->nargs == 0) ||
			    ((pcmd->argval->ival == 6) && (ml->v6_flag != 0)) ||
			    ((pcmd->argval->ival == 4) && (ml->v6_flag == 0)))
				(void) fprintf(fp,
				    "%-25.25s %5d %9ld %4d %2d %9lx %9lu %9lu\n",
				    nntohost(&dstadr),
				    ntohs(ml->port),
				    (u_long)ntohl(ml->count),
				    ml->mode,
				    ml->version,
				    (u_long)ntohl(ml->lastdrop),
				    (u_long)ntohl(ml->lasttime),
				    (u_long)ntohl(ml->firsttime));
			ml++;
			items--;
		}
	} else if (itemsize == sizeof(struct old_info_monitor)) {
		struct old_info_monitor *oml = (struct old_info_monitor *)struct_star;
		(void) fprintf(fp,
			       "     address          port     count  mode version  lasttime firsttime\n");
		(void) fprintf(fp,
			       "======================================================================\n");
		while (items > 0) {
			memset((char *)&dstadr, 0, sizeof(dstadr));
			if (oml->v6_flag != 0) {
				GET_INADDR6(dstadr) = oml->addr6;
				dstadr.ss_family = AF_INET6;
			} else {
				GET_INADDR(dstadr) = oml->addr;
				dstadr.ss_family = AF_INET;
			}
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
			dstadr.ss_len = SOCKLEN(&dstadr);
#endif
			(void) fprintf(fp, "%-20.20s %5d %9ld %4d   %3d %9lu %9lu\n",
				       nntohost(&dstadr),
				       ntohs(oml->port),
				       (u_long)ntohl(oml->count),
				       oml->mode,
				       oml->version,
				       (u_long)ntohl(oml->lasttime),
				       (u_long)ntohl(oml->firsttime));
			oml++;
			items--;
		}
	} else {
		/* issue warning according to new info_monitor size */
		checkitemsize(itemsize, sizeof(struct info_monitor));
	}
}


/*
 * Mapping between command line strings and stat reset flags
 */
struct statreset {
  const char *str;
	int flag;
} sreset[] = {
	{ "io",		RESET_FLAG_IO },
	{ "sys",	RESET_FLAG_SYS },
	{ "mem",	RESET_FLAG_MEM },
	{ "timer",	RESET_FLAG_TIMER },
	{ "auth",	RESET_FLAG_AUTH },
	{ "allpeers",	RESET_FLAG_ALLPEERS },
	{ "",		0 }
};

/*
 * reset - reset statistic counters
 */
static void
reset(
	struct parse *pcmd,
	FILE *fp
	)
{
	struct reset_flags rflags;
	int items;
	int itemsize;
	char *dummy;
	int i;
	int res;
	int err;

	err = 0;
	rflags.flags = 0;
	for (res = 0; res < pcmd->nargs; res++) {
		for (i = 0; sreset[i].flag != 0; i++) {
			if (STREQ(pcmd->argval[res].string, sreset[i].str))
			    break;
		}
		if (sreset[i].flag == 0) {
			(void) fprintf(fp, "Flag %s unknown\n",
				       pcmd->argval[res].string);
			err++;
		} else {
			rflags.flags |= sreset[i].flag;
		}
	}
	rflags.flags = htonl(rflags.flags);

	if (err) {
		(void) fprintf(fp, "Not done due to errors\n");
		return;
	}

again:
	res = doquery(impl_ver, REQ_RESET_STATS, 1, 1,
		      sizeof(struct reset_flags), (char *)&rflags, &items,
		      &itemsize, &dummy, 0, sizeof(struct reset_flags));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res == 0)
	    (void) fprintf(fp, "done!\n");
	return;
}



/*
 * preset - reset stat counters for particular peers
 */
static void
preset(
	struct parse *pcmd,
	FILE *fp
	)
{
	/* 8 is the maximum number of peers which will fit in a packet */
	struct conf_unpeer *pl, plist[min(MAXARGS, 8)];
	int qitems;
	int items;
	int itemsize;
	char *dummy;
	int res;
	int sendsize;

again:
	if (impl_ver == IMPL_XNTPD)
		sendsize = sizeof(struct conf_unpeer);
	else
		sendsize = v4sizeof(struct conf_unpeer);

	for (qitems = 0, pl = plist; qitems < min(pcmd->nargs, 8); qitems++) {
		if (pcmd->argval[qitems].netnum.ss_family == AF_INET) {
			pl->peeraddr = GET_INADDR(pcmd->argval[qitems].netnum);
			if (impl_ver == IMPL_XNTPD)
				pl->v6_flag = 0;
		} else {
			if (impl_ver == IMPL_XNTPD_OLD) {
				fprintf(stderr,
				    "***Server doesn't understand IPv6 addresses\n");
				return;
			}
			pl->peeraddr6 =
			    GET_INADDR6(pcmd->argval[qitems].netnum);
			pl->v6_flag = 1;
		}
		pl = (struct conf_unpeer *)((char *)pl + sendsize);
	}

	res = doquery(impl_ver, REQ_RESET_PEER, 1, qitems,
		      sendsize, (char *)plist, &items,
		      &itemsize, &dummy, 0, sizeof(struct conf_unpeer));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res == 0)
	    (void) fprintf(fp, "done!\n");
}


/*
 * readkeys - request the server to reread the keys file
 */
/*ARGSUSED*/
static void
readkeys(
	struct parse *pcmd,
	FILE *fp
	)
{
	int items;
	int itemsize;
	char *dummy;
	int res;

again:
	res = doquery(impl_ver, REQ_REREAD_KEYS, 1, 0, 0, (char *)0,
		      &items, &itemsize, &dummy, 0, sizeof(dummy));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res == 0)
	    (void) fprintf(fp, "done!\n");
	return;
}


/*
 * trustkey - add some keys to the trusted key list
 */
static void
trustkey(
	struct parse *pcmd,
	FILE *fp
	)
{
	do_trustkey(pcmd, fp, REQ_TRUSTKEY);
}


/*
 * untrustkey - remove some keys from the trusted key list
 */
static void
untrustkey(
	struct parse *pcmd,
	FILE *fp
	)
{
	do_trustkey(pcmd, fp, REQ_UNTRUSTKEY);
}


/*
 * do_trustkey - do grunge work of adding/deleting keys
 */
static void
do_trustkey(
	struct parse *pcmd,
	FILE *fp,
	int req
	)
{
	u_long keyids[MAXARGS];
	int i;
	int items;
	int itemsize;
	char *dummy;
	int ritems;
	int res;

	ritems = 0;
	for (i = 0; i < pcmd->nargs; i++) {
		keyids[ritems++] = pcmd->argval[i].uval;
	}

again:
	res = doquery(impl_ver, req, 1, ritems, sizeof(u_long),
		      (char *)keyids, &items, &itemsize, &dummy, 0, 
		      sizeof(dummy));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res == 0)
	    (void) fprintf(fp, "done!\n");
	return;
}



/*
 * authinfo - obtain and print info about authentication
 */
/*ARGSUSED*/
static void
authinfo(
	struct parse *pcmd,
	FILE *fp
	)
{
	struct info_auth *ia;
	int items;
	int itemsize;
	int res;

again:
	res = doquery(impl_ver, REQ_AUTHINFO, 0, 0, 0, (char *)NULL,
		      &items, &itemsize, (void *)&ia, 0, 
		      sizeof(struct info_auth));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res != 0)
	    return;

	if (!check1item(items, fp))
	    return;

	if (!checkitemsize(itemsize, sizeof(struct info_auth)))
	    return;

	(void) fprintf(fp, "time since reset:     %ld\n",
	    (u_long)ntohl(ia->timereset));
	(void) fprintf(fp, "stored keys:          %ld\n",
	    (u_long)ntohl(ia->numkeys));
	(void) fprintf(fp, "free keys:            %ld\n",
	    (u_long)ntohl(ia->numfreekeys));
	(void) fprintf(fp, "key lookups:          %ld\n",
	    (u_long)ntohl(ia->keylookups));
	(void) fprintf(fp, "keys not found:       %ld\n",
	    (u_long)ntohl(ia->keynotfound));
	(void) fprintf(fp, "uncached keys:        %ld\n",
	    (u_long)ntohl(ia->keyuncached));
	(void) fprintf(fp, "encryptions:          %ld\n",
	    (u_long)ntohl(ia->encryptions));
	(void) fprintf(fp, "decryptions:          %ld\n",
	    (u_long)ntohl(ia->decryptions));
	(void) fprintf(fp, "expired keys:         %ld\n",
	    (u_long)ntohl(ia->expired));
}



/*
 * traps - obtain and print a list of traps
 */
/*ARGSUSED*/
static void
traps(
	struct parse *pcmd,
	FILE *fp
	)
{
	int i;
	struct info_trap *it;
	struct sockaddr_storage trap_addr, local_addr;
	int items;
	int itemsize;
	int res;

again:
	res = doquery(impl_ver, REQ_TRAPS, 0, 0, 0, (char *)NULL,
		      &items, &itemsize, (void *)&it, 0, 
		      sizeof(struct info_trap));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res != 0)
	    return;

	if (!checkitems(items, fp))
	    return;

	if (!checkitemsize(itemsize, sizeof(struct info_trap)) &&
	    !checkitemsize(itemsize, v4sizeof(struct info_trap)))
	    return;

	for (i = 0; i < items; i++ ) {
		if (i != 0)
		    (void) fprintf(fp, "\n");
		memset((char *)&trap_addr, 0, sizeof(trap_addr));
		memset((char *)&local_addr, 0, sizeof(local_addr));
		if (it->v6_flag != 0) {
			GET_INADDR6(trap_addr) = it->trap_address6;
			GET_INADDR6(local_addr) = it->local_address6;
			trap_addr.ss_family = AF_INET6;
			local_addr.ss_family = AF_INET6;
		} else {
			GET_INADDR(trap_addr) = it->trap_address;
			GET_INADDR(local_addr) = it->local_address;
			trap_addr.ss_family = AF_INET;
			local_addr.ss_family = AF_INET;
		}
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
		trap_addr.ss_len = SOCKLEN(&trap_addr);
		local_addr.ss_len = SOCKLEN(&local_addr);
#endif
		(void) fprintf(fp, "address %s, port %d\n",
				stoa(&trap_addr), 
				ntohs(it->trap_port));
		(void) fprintf(fp, "interface: %s, ",
				(it->local_address == 0)
				? "wildcard"
				: stoa(&local_addr));
		if (ntohl(it->flags) & TRAP_CONFIGURED)
		    (void) fprintf(fp, "configured\n");
		else if (ntohl(it->flags) & TRAP_NONPRIO)
		    (void) fprintf(fp, "low priority\n");
		else
		    (void) fprintf(fp, "normal priority\n");
		
		(void) fprintf(fp, "set for %ld secs, last set %ld secs ago\n",
			       (long)ntohl(it->origtime),
			       (long)ntohl(it->settime));
		(void) fprintf(fp, "sequence %d, number of resets %ld\n",
			       ntohs(it->sequence),
			       (long)ntohl(it->resets));
	}
}


/*
 * addtrap - configure a trap
 */
static void
addtrap(
	struct parse *pcmd,
	FILE *fp
	)
{
	do_addclr_trap(pcmd, fp, REQ_ADD_TRAP);
}


/*
 * clrtrap - clear a trap from the server
 */
static void
clrtrap(
	struct parse *pcmd,
	FILE *fp
	)
{
	do_addclr_trap(pcmd, fp, REQ_CLR_TRAP);
}


/*
 * do_addclr_trap - do grunge work of adding/deleting traps
 */
static void
do_addclr_trap(
	struct parse *pcmd,
	FILE *fp,
	int req
	)
{
	struct conf_trap ctrap;
	int items;
	int itemsize;
	char *dummy;
	int res;
	int sendsize;

again:
	if (impl_ver == IMPL_XNTPD)
		sendsize = sizeof(struct conf_trap);
	else
		sendsize = v4sizeof(struct conf_trap);

	if (pcmd->argval[0].netnum.ss_family == AF_INET) {
		ctrap.trap_address = GET_INADDR(pcmd->argval[0].netnum);
		if (impl_ver == IMPL_XNTPD)
			ctrap.v6_flag = 0;
	} else {
		if (impl_ver == IMPL_XNTPD_OLD) {
			fprintf(stderr,
			    "***Server doesn't understand IPv6 addresses\n");
			return;
		}
		ctrap.trap_address6 = GET_INADDR6(pcmd->argval[0].netnum);
		ctrap.v6_flag = 1;
	}
	ctrap.local_address = 0;
	ctrap.trap_port = htons(TRAPPORT);
	ctrap.unused = 0;

	if (pcmd->nargs > 1) {
		ctrap.trap_port
			= htons((u_short)(pcmd->argval[1].uval & 0xffff));
		if (pcmd->nargs > 2) {
			if (pcmd->argval[2].netnum.ss_family !=
			    pcmd->argval[0].netnum.ss_family) {
				fprintf(stderr,
				    "***Cannot mix IPv4 and IPv6 addresses\n");
				return;
			}
			if (pcmd->argval[2].netnum.ss_family == AF_INET)
				ctrap.local_address = GET_INADDR(pcmd->argval[2].netnum);
			else
				ctrap.local_address6 = GET_INADDR6(pcmd->argval[2].netnum);
		}
	}

	res = doquery(impl_ver, req, 1, 1, sendsize,
		      (char *)&ctrap, &items, &itemsize, &dummy, 0, 
		      sizeof(struct conf_trap));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res == 0)
	    (void) fprintf(fp, "done!\n");
	return;
}



/*
 * requestkey - change the server's request key (a dangerous request)
 */
static void
requestkey(
	struct parse *pcmd,
	FILE *fp
	)
{
	do_changekey(pcmd, fp, REQ_REQUEST_KEY);
}


/*
 * controlkey - change the server's control key
 */
static void
controlkey(
	struct parse *pcmd,
	FILE *fp
	)
{
	do_changekey(pcmd, fp, REQ_CONTROL_KEY);
}



/*
 * do_changekey - do grunge work of changing keys
 */
static void
do_changekey(
	struct parse *pcmd,
	FILE *fp,
	int req
	)
{
	u_long key;
	int items;
	int itemsize;
	char *dummy;
	int res;


	key = htonl((u_int32)pcmd->argval[0].uval);

again:
	res = doquery(impl_ver, req, 1, 1, sizeof(u_int32),
		      (char *)&key, &items, &itemsize, &dummy, 0, 
		      sizeof(dummy));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res == 0)
	    (void) fprintf(fp, "done!\n");
	return;
}



/*
 * ctlstats - obtain and print info about authentication
 */
/*ARGSUSED*/
static void
ctlstats(
	struct parse *pcmd,
	FILE *fp
	)
{
	struct info_control *ic;
	int items;
	int itemsize;
	int res;

again:
	res = doquery(impl_ver, REQ_GET_CTLSTATS, 0, 0, 0, (char *)NULL,
		      &items, &itemsize, (void *)&ic, 0, 
		      sizeof(struct info_control));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res != 0)
	    return;

	if (!check1item(items, fp))
	    return;

	if (!checkitemsize(itemsize, sizeof(struct info_control)))
	    return;

	(void) fprintf(fp, "time since reset:       %ld\n",
		       (u_long)ntohl(ic->ctltimereset));
	(void) fprintf(fp, "requests received:      %ld\n",
		       (u_long)ntohl(ic->numctlreq));
	(void) fprintf(fp, "responses sent:         %ld\n",
		       (u_long)ntohl(ic->numctlresponses));
	(void) fprintf(fp, "fragments sent:         %ld\n",
		       (u_long)ntohl(ic->numctlfrags));
	(void) fprintf(fp, "async messages sent:    %ld\n",
		       (u_long)ntohl(ic->numasyncmsgs));
	(void) fprintf(fp, "error msgs sent:        %ld\n",
		       (u_long)ntohl(ic->numctlerrors));
	(void) fprintf(fp, "total bad pkts:         %ld\n",
		       (u_long)ntohl(ic->numctlbadpkts));
	(void) fprintf(fp, "packet too short:       %ld\n",
		       (u_long)ntohl(ic->numctltooshort));
	(void) fprintf(fp, "response on input:      %ld\n",
		       (u_long)ntohl(ic->numctlinputresp));
	(void) fprintf(fp, "fragment on input:      %ld\n",
		       (u_long)ntohl(ic->numctlinputfrag));
	(void) fprintf(fp, "error set on input:     %ld\n",
		       (u_long)ntohl(ic->numctlinputerr));
	(void) fprintf(fp, "bad offset on input:    %ld\n",
		       (u_long)ntohl(ic->numctlbadoffset));
	(void) fprintf(fp, "bad version packets:    %ld\n",
		       (u_long)ntohl(ic->numctlbadversion));
	(void) fprintf(fp, "data in pkt too short:  %ld\n",
		       (u_long)ntohl(ic->numctldatatooshort));
	(void) fprintf(fp, "unknown op codes:       %ld\n",
		       (u_long)ntohl(ic->numctlbadop));
}


/*
 * clockstat - get and print clock status information
 */
static void
clockstat(
	struct parse *pcmd,
	FILE *fp
	)
{
	struct info_clock *cl;
	/* 8 is the maximum number of clocks which will fit in a packet */
	u_long clist[min(MAXARGS, 8)];
	int qitems;
	int items;
	int itemsize;
	int res;
	l_fp ts;
	struct clktype *clk;

	for (qitems = 0; qitems < min(pcmd->nargs, 8); qitems++)
	    clist[qitems] = GET_INADDR(pcmd->argval[qitems].netnum);

again:
	res = doquery(impl_ver, REQ_GET_CLOCKINFO, 0, qitems,
		      sizeof(u_int32), (char *)clist, &items,
		      &itemsize, (void *)&cl, 0, sizeof(struct info_clock));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res != 0)
	    return;

	if (!checkitems(items, fp))
	    return;

	if (!checkitemsize(itemsize, sizeof(struct info_clock)))
	    return;

	while (items-- > 0) {
		(void) fprintf(fp, "clock address:        %s\n",
			       numtoa(cl->clockadr));
		for (clk = clktypes; clk->code >= 0; clk++)
		    if (clk->code == cl->type)
			break;
		if (clk->code >= 0)
		    (void) fprintf(fp, "clock type:           %s\n",
				   clk->clocktype);
		else
		    (void) fprintf(fp, "clock type:           unknown type (%d)\n",
				   cl->type);
		(void) fprintf(fp, "last event:           %d\n",
			       cl->lastevent);
		(void) fprintf(fp, "current status:       %d\n",
			       cl->currentstatus);
		(void) fprintf(fp, "number of polls:      %lu\n",
			       (u_long)ntohl(cl->polls));
		(void) fprintf(fp, "no response to poll:  %lu\n",
			       (u_long)ntohl(cl->noresponse));
		(void) fprintf(fp, "bad format responses: %lu\n",
			       (u_long)ntohl(cl->badformat));
		(void) fprintf(fp, "bad data responses:   %lu\n",
			       (u_long)ntohl(cl->baddata));
		(void) fprintf(fp, "running time:         %lu\n",
			       (u_long)ntohl(cl->timestarted));
		NTOHL_FP(&cl->fudgetime1, &ts);
		(void) fprintf(fp, "fudge time 1:         %s\n",
			       lfptoa(&ts, 6));
		NTOHL_FP(&cl->fudgetime2, &ts);
		(void) fprintf(fp, "fudge time 2:         %s\n",
			       lfptoa(&ts, 6));
		(void) fprintf(fp, "stratum:              %ld\n",
			       (u_long)ntohl(cl->fudgeval1));
		(void) fprintf(fp, "reference ID:         %s\n",
			       refid_string(ntohl(cl->fudgeval2), 0));
		(void) fprintf(fp, "fudge flags:          0x%x\n",
			       cl->flags);

		if (items > 0)
		    (void) fprintf(fp, "\n");
		cl++;
	}
}


/*
 * fudge - set clock fudge factors
 */
static void
fudge(
	struct parse *pcmd,
	FILE *fp
	)
{
	struct conf_fudge fudgedata;
	int items;
	int itemsize;
	char *dummy;
	l_fp ts;
	int res;
	long val;
	u_long u_val;
	int err;


	err = 0;
	memset((char *)&fudgedata, 0, sizeof fudgedata);
	fudgedata.clockadr = GET_INADDR(pcmd->argval[0].netnum);

	if (STREQ(pcmd->argval[1].string, "time1")) {
		fudgedata.which = htonl(FUDGE_TIME1);
		if (!atolfp(pcmd->argval[2].string, &ts))
		    err = 1;
		else
		    NTOHL_FP(&ts, &fudgedata.fudgetime);
	} else if (STREQ(pcmd->argval[1].string, "time2")) {
		fudgedata.which = htonl(FUDGE_TIME2);
		if (!atolfp(pcmd->argval[2].string, &ts))
		    err = 1;
		else
		    NTOHL_FP(&ts, &fudgedata.fudgetime);
	} else if (STREQ(pcmd->argval[1].string, "val1")) {
		fudgedata.which = htonl(FUDGE_VAL1);
		if (!atoint(pcmd->argval[2].string, &val))
		    err = 1;
		else
		    fudgedata.fudgeval_flags = htonl(val);
	} else if (STREQ(pcmd->argval[1].string, "val2")) {
		fudgedata.which = htonl(FUDGE_VAL2);
		if (!atoint(pcmd->argval[2].string, &val))
		    err = 1;
		else
		    fudgedata.fudgeval_flags = htonl((u_int32)val);
	} else if (STREQ(pcmd->argval[1].string, "flags")) {
		fudgedata.which = htonl(FUDGE_FLAGS);
		if (!hextoint(pcmd->argval[2].string, &u_val))
		    err = 1;
		else
		    fudgedata.fudgeval_flags = htonl((u_int32)(u_val & 0xf));
	} else {
		(void) fprintf(stderr, "What fudge is %s?\n",
			       pcmd->argval[1].string);
		return;
	}

	if (err) {
		(void) fprintf(stderr, "Unknown fudge parameter %s\n",
			       pcmd->argval[2].string);
		return;
	}

again:
	res = doquery(impl_ver, REQ_SET_CLKFUDGE, 1, 1,
		      sizeof(struct conf_fudge), (char *)&fudgedata, &items,
		      &itemsize, &dummy, 0, sizeof(dummy));

	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res == 0)
	    (void) fprintf(fp, "done!\n");
	return;
}

/*
 * clkbug - get and print clock debugging information
 */
static void
clkbug(
	struct parse *pcmd,
	FILE *fp
	)
{
	register int i;
	register int n;
	register u_int32 s;
	struct info_clkbug *cl;
	/* 8 is the maximum number of clocks which will fit in a packet */
	u_long clist[min(MAXARGS, 8)];
	u_int32 ltemp;
	int qitems;
	int items;
	int itemsize;
	int res;
	int needsp;
	l_fp ts;

	for (qitems = 0; qitems < min(pcmd->nargs, 8); qitems++)
	    clist[qitems] = GET_INADDR(pcmd->argval[qitems].netnum);

again:
	res = doquery(impl_ver, REQ_GET_CLKBUGINFO, 0, qitems,
		      sizeof(u_int32), (char *)clist, &items,
		      &itemsize, (void *)&cl, 0, sizeof(struct info_clkbug));
	
	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res != 0)
	    return;

	if (!checkitems(items, fp))
	    return;

	if (!checkitemsize(itemsize, sizeof(struct info_clkbug)))
	    return;

	while (items-- > 0) {
		(void) fprintf(fp, "clock address:        %s\n",
			       numtoa(cl->clockadr));
		n = (int)cl->nvalues;
		(void) fprintf(fp, "values: %d", n);
		s = ntohs(cl->svalues);
		if (n > NUMCBUGVALUES)
		    n = NUMCBUGVALUES;
		for (i = 0; i < n; i++) {
			ltemp = ntohl(cl->values[i]);
			ltemp &= 0xffffffff;	/* HMS: This does nothing now */
			if ((i & 0x3) == 0)
			    (void) fprintf(fp, "\n");
			if (s & (1 << i))
			    (void) fprintf(fp, "%12ld", (u_long)ltemp);
			else
			    (void) fprintf(fp, "%12lu", (u_long)ltemp);
		}
		(void) fprintf(fp, "\n");

		n = (int)cl->ntimes;
		(void) fprintf(fp, "times: %d", n);
		s = ntohl(cl->stimes);
		if (n > NUMCBUGTIMES)
		    n = NUMCBUGTIMES;
		needsp = 0;
		for (i = 0; i < n; i++) {
			if ((i & 0x1) == 0) {
			    (void) fprintf(fp, "\n");
			} else {
				for (;needsp > 0; needsp--)
				    putc(' ', fp);
			}
			NTOHL_FP(&cl->times[i], &ts);
			if (s & (1 << i)) {
				(void) fprintf(fp, "%17s",
					       lfptoa(&ts, 6));
				needsp = 22;
			} else {
				(void) fprintf(fp, "%37s",
					       uglydate(&ts));
				needsp = 2;
			}
		}
		(void) fprintf(fp, "\n");
		if (items > 0) {
			cl++;
			(void) fprintf(fp, "\n");
		}
	}
}


/*
 * kerninfo - display the kernel pll/pps variables
 */
static void
kerninfo(
	struct parse *pcmd,
	FILE *fp
	)
{
	struct info_kernel *ik;
	int items;
	int itemsize;
	int res;
	unsigned status;
	double tscale = 1e-6;

again:
	res = doquery(impl_ver, REQ_GET_KERNEL, 0, 0, 0, (char *)NULL,
		      &items, &itemsize, (void *)&ik, 0, 
		      sizeof(struct info_kernel));

	if (res == INFO_ERR_IMPL && impl_ver == IMPL_XNTPD) {
		impl_ver = IMPL_XNTPD_OLD;
		goto again;
	}

	if (res != 0)
	    return;
	if (!check1item(items, fp))
	    return;
	if (!checkitemsize(itemsize, sizeof(struct info_kernel)))
	    return;

	status = ntohs(ik->status) & 0xffff;
	/*
	 * pll variables. We know more than we should about the NANO bit.
	 */
#ifdef STA_NANO
	if (status & STA_NANO)
		tscale = 1e-9;
#endif
	(void)fprintf(fp, "pll offset:           %g s\n",
	    (int32_t)ntohl(ik->offset) * tscale);
	(void)fprintf(fp, "pll frequency:        %s ppm\n",
	    fptoa((s_fp)ntohl(ik->freq), 3));
	(void)fprintf(fp, "maximum error:        %g s\n",
	    (u_long)ntohl(ik->maxerror) * 1e-6);
	(void)fprintf(fp, "estimated error:      %g s\n",
	    (u_long)ntohl(ik->esterror) * 1e-6);
	(void)fprintf(fp, "status:               %04x ", status);
#ifdef STA_PLL
	if (status & STA_PLL) (void)fprintf(fp, " pll");
#endif
#ifdef STA_PPSFREQ
	if (status & STA_PPSFREQ) (void)fprintf(fp, " ppsfreq");
#endif
#ifdef STA_PPSTIME
	if (status & STA_PPSTIME) (void)fprintf(fp, " ppstime");
#endif
#ifdef STA_FLL
	if (status & STA_FLL) (void)fprintf(fp, " fll");
#endif
#ifdef STA_INS
	if (status & STA_INS) (void)fprintf(fp, " ins");
#endif
#ifdef STA_DEL
	if (status & STA_DEL) (void)fprintf(fp, " del");
#endif
#ifdef STA_UNSYNC
	if (status & STA_UNSYNC) (void)fprintf(fp, " unsync");
#endif
#ifdef STA_FREQHOLD
	if (status & STA_FREQHOLD) (void)fprintf(fp, " freqhold");
#endif
#ifdef STA_PPSSIGNAL
	if (status & STA_PPSSIGNAL) (void)fprintf(fp, " ppssignal");
#endif
#ifdef STA_PPSJITTER
	if (status & STA_PPSJITTER) (void)fprintf(fp, " ppsjitter");
#endif
#ifdef STA_PPSWANDER
	if (status & STA_PPSWANDER) (void)fprintf(fp, " ppswander");
#endif
#ifdef STA_PPSERROR
	if (status & STA_PPSERROR) (void)fprintf(fp, " ppserror");
#endif
#ifdef STA_CLOCKERR
	if (status & STA_CLOCKERR) (void)fprintf(fp, " clockerr");
#endif
#ifdef STA_NANO
	if (status & STA_NANO) (void)fprintf(fp, " nano");
#endif
#ifdef STA_MODE
	if (status & STA_MODE) (void)fprintf(fp, " mode=fll");
#endif
#ifdef STA_CLK
	if (status & STA_CLK) (void)fprintf(fp, " src=B");
#endif
	(void)fprintf(fp, "\n");
	(void)fprintf(fp, "pll time constant:    %ld\n",
	    (u_long)ntohl(ik->constant));
	(void)fprintf(fp, "precision:            %g s\n",
	    (u_long)ntohl(ik->precision) * tscale);
	(void)fprintf(fp, "frequency tolerance:  %s ppm\n",
	    fptoa((s_fp)ntohl(ik->tolerance), 0));

	/*
	 * For backwards compatibility (ugh), we find the pps variables
	 * only if the shift member is nonzero.
	 */
	if (!ik->shift)
	    return;

	/*
	 * pps variables
	 */
	(void)fprintf(fp, "pps frequency:        %s ppm\n",
	    fptoa((s_fp)ntohl(ik->ppsfreq), 3));
	(void)fprintf(fp, "pps stability:        %s ppm\n",
	    fptoa((s_fp)ntohl(ik->stabil), 3));
	(void)fprintf(fp, "pps jitter:           %g s\n",
	    (u_long)ntohl(ik->jitter) * tscale);
	(void)fprintf(fp, "calibration interval: %d s\n",
		      1 << ntohs(ik->shift));
	(void)fprintf(fp, "calibration cycles:   %ld\n",
		      (u_long)ntohl(ik->calcnt));
	(void)fprintf(fp, "jitter exceeded:      %ld\n",
		      (u_long)ntohl(ik->jitcnt));
	(void)fprintf(fp, "stability exceeded:   %ld\n",
		      (u_long)ntohl(ik->stbcnt));
	(void)fprintf(fp, "calibration errors:   %ld\n",
		      (u_long)ntohl(ik->errcnt));
}

#define IF_LIST_FMT     "%2d %c %48s %c %c %12.12s %03x %3d %2d %5d %5d %5d %2d %2d %3d %7d\n"
#define IF_LIST_FMT_STR "%2s %c %48s %c %c %12.12s %3s %3s %2s %5s %5s %5s %2s %2s %3s %7s\n"
#define IF_LIST_AFMT_STR "     %48s %c\n"
#define IF_LIST_LABELS  "#", 'A', "Address/Mask/Broadcast", 'T', 'E', "IF name", "Flg", "TL", "#M", "recv", "sent", "drop", "S", "IX", "PC", "uptime"
#define IF_LIST_LINE    "=====================================================================================================================\n"

static void
iflist(
	FILE *fp,
	struct info_if_stats *ifs,
	int items,
	int itemsize,
	int res
	)
{
	static char *actions = "?.+-";
	struct sockaddr_storage saddr;

	if (res != 0)
	    return;

	if (!checkitems(items, fp))
	    return;

	if (!checkitemsize(itemsize, sizeof(struct info_if_stats)))
	    return;

	fprintf(fp, IF_LIST_FMT_STR, IF_LIST_LABELS);
	fprintf(fp, IF_LIST_LINE);
	
	while (items > 0) {
		if (ntohl(ifs->v6_flag)) {
			memcpy((char *)&GET_INADDR6(saddr), (char *)&ifs->unaddr.addr6, sizeof(ifs->unaddr.addr6));
			saddr.ss_family = AF_INET6;
		} else {
			memcpy((char *)&GET_INADDR(saddr), (char *)&ifs->unaddr.addr, sizeof(ifs->unaddr.addr));
			saddr.ss_family = AF_INET;
		}
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
		saddr.ss_len = SOCKLEN(&saddr);
#endif
		fprintf(fp, IF_LIST_FMT,
			ntohl(ifs->ifnum),
			actions[(ifs->action >= 1 && ifs->action < 4) ? ifs->action : 0],
			stoa((&saddr)), 'A',
			ifs->ignore_packets ? 'D' : 'E',
			ifs->name,
			ntohl(ifs->flags),
			ntohl(ifs->last_ttl),
			ntohl(ifs->num_mcast),
			ntohl(ifs->received),
			ntohl(ifs->sent),
			ntohl(ifs->notsent),
			ntohl(ifs->scopeid),
			ntohl(ifs->ifindex),
			ntohl(ifs->peercnt),
			ntohl(ifs->uptime));

		if (ntohl(ifs->v6_flag)) {
			memcpy((char *)&GET_INADDR6(saddr), (char *)&ifs->unmask.addr6, sizeof(ifs->unmask.addr6));
			saddr.ss_family = AF_INET6;
		} else {
			memcpy((char *)&GET_INADDR(saddr), (char *)&ifs->unmask.addr, sizeof(ifs->unmask.addr));
			saddr.ss_family = AF_INET;
		}
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
		saddr.ss_len = SOCKLEN(&saddr);
#endif
		fprintf(fp, IF_LIST_AFMT_STR, stoa(&saddr), 'M');

		if (!ntohl(ifs->v6_flag) && ntohl(ifs->flags) & (INT_BCASTOPEN)) {
			memcpy((char *)&GET_INADDR(saddr), (char *)&ifs->unbcast.addr, sizeof(ifs->unbcast.addr));
			saddr.ss_family = AF_INET;
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
			saddr.ss_len = SOCKLEN(&saddr);
#endif
			fprintf(fp, IF_LIST_AFMT_STR, stoa(&saddr), 'B');

		}

		ifs++;
		items--;
	}
}

/*ARGSUSED*/
static void
get_if_stats(
	struct parse *pcmd,
	FILE *fp
	)
{
	struct info_if_stats *ifs;
	int items;
	int itemsize;
	int res;

	res = doquery(impl_ver, REQ_IF_STATS, 1, 0, 0, (char *)NULL, &items,
		      &itemsize, (void *)&ifs, 0, 
		      sizeof(struct info_if_stats));
	iflist(fp, ifs, items, itemsize, res);
}

/*ARGSUSED*/
static void
do_if_reload(
	struct parse *pcmd,
	FILE *fp
	)
{
	struct info_if_stats *ifs;
	int items;
	int itemsize;
	int res;

	res = doquery(impl_ver, REQ_IF_RELOAD, 1, 0, 0, (char *)NULL, &items,
		      &itemsize, (void *)&ifs, 0, 
		      sizeof(struct info_if_stats));
	iflist(fp, ifs, items, itemsize, res);
}
