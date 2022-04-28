/*
 * Device driver for Specialix range (SLXOS) of serial line multiplexors.
 *	SLXOS configuration and debug interface
 *
 * Copyright (C) 1990, 1992 Specialix International,
 * Copyright (C) 1993, Andy Rutter <andy@acronym.co.uk>
 * Copyright (C) 1995, Peter Wemm
 *
 * Derived from:	SunOS 4.x version
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notices, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notices, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of Advanced Methods and Tools, nor Specialix
 *    International may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/tty.h>

#define SI_DEBUG
#include <dev/si/si.h>
#include <dev/si/sivar.h>

struct lv {
	char	*lv_name;
	int 	lv_bit;
} lv[] = {
	{"entry",	DBG_ENTRY},
	{"open",	DBG_OPEN},
	{"close",	DBG_CLOSE},
	{"read",	DBG_READ},
	{"write",	DBG_WRITE},
	{"param",	DBG_PARAM},
	{"modem",	DBG_MODEM},
	{"select",	DBG_SELECT},
	{"optim",	DBG_OPTIM},
	{"intr",	DBG_INTR},
	{"start",	DBG_START},
	{"lstart",	DBG_LSTART},
	{"ioctl",	DBG_IOCTL},
	{"fail",	DBG_FAIL},
	{"autoboot",	DBG_AUTOBOOT},
	{"download",	DBG_DOWNLOAD},
	{"drain",	DBG_DRAIN},
	{"poll",	DBG_POLL},
	{0,		0}
};

static int alldev = 0;

void ccb_stat(int, char **);
void port_stat(int, char **);
void debug(int, char **);
void dostat(void);
int getnum(char *);
int islevel(char *);
int lvls2bits(char *);
void mstate(int, char **);
void nport(int, char **);
void onoff(int, char **, int, char *, char *, int);
int opencontrol(void);
void prlevels(int);
void prusage(int, int);
void rxint(int, char **);
void txint(int, char **);

struct opt {
	char	*o_name;
	void	(*o_func)(int, char **);
} opt[] = {
	{"debug",		debug},
	{"rxint_throttle",	rxint},
	{"int_throttle",	txint},
	{"nport",		nport},
	{"mstate",		mstate},
	{"ccbstat",		ccb_stat},
	{"portstat",		port_stat},
	{0,			0}
};

struct stat_list {
	void (*st_func)(int, char **);
} stat_list[] = {
	{mstate},
	{0}
};

#define	U_DEBUG		0
#define	U_TXINT		1
#define	U_RXINT		2
#define	U_NPORT		3
#define	U_MSTATE	4
#define	U_STAT_CCB	5
#define	U_STAT_PORT	6

#define	U_MAX		7
#define	U_ALL		-1
char *usage[] = {
	"debug [[add|del|set debug_levels] | [off]]\n",
	"int_throttle [newvalue]\n",
	"rxint_throttle [newvalue]\n",
	"nport\n",
	"mstate\n",
	"ccbstat\n",
	"portstat\n",
	0
};

int ctlfd;
char *Devname;
struct si_tcsi tc;

int
main(int argc, char **argv)
{
	struct opt *op;
	void (*func)(int, char **) = NULL;

	if (argc < 2)
		prusage(U_ALL, 1);
	Devname = argv[1];
	if (strcmp(Devname, "-") == 0) {
		alldev = 1;
	} else {
		sidev_t dev;
		int n;
		int card, port;

		n = sscanf(Devname, "%d:%d", &card, &port);
		if (n != 2)
			errx(1, "Devname must be in form card:port.  eg: 0:7");
		dev.sid_card = card;
		dev.sid_port = port;
		tc.tc_dev = dev;
	}
	ctlfd = opencontrol();
	if (argc == 2) {
		dostat();
		exit(0);
	}

	argc--; argv++;
	for (op = opt; op->o_name; op++) {
		if (strcmp(argv[1], op->o_name) == 0) {
			func = op->o_func;
			break;
		}
	}
	if (func == NULL)
		prusage(U_ALL, 1);

	argc -= 2;
	argv += 2;
	(*func)(argc, argv);
	exit(0);
}

int
opencontrol(void)
{
	int fd;

	fd = open(CONTROLDEV, O_RDWR|O_NDELAY);
	if (fd < 0)
		err(1, "open on %s", CONTROLDEV);
	return(fd);
}

/*
 * Print a usage message - this relies on U_DEBUG==0 and U_BOOT==1.
 * Don't print the DEBUG usage string unless explicity requested.
 */
void
prusage(int strn, int eflag)
{
	char **cp;

	if (strn == U_ALL) {
		fprintf(stderr, "usage: sicontrol %s", usage[1]);
		fprintf(stderr, "       sicontrol %s", usage[2]);
		fprintf(stderr, "       sicontrol %s", usage[3]);
		fprintf(stderr, "       sicontrol devname %s", usage[4]);
		for (cp = &usage[5]; *cp; cp++)
			fprintf(stderr, "       sicontrol devname %s", *cp);
	}
	else if (strn >= 0 && strn <= U_MAX)
		fprintf(stderr, "usage: sicontrol devname %s", usage[strn]);
	else
		fprintf(stderr, "sicontrol: usage ???\n");
	exit(eflag);
}

/* print port status */
void
dostat(void)
{
	char *av[1], *acp;
	struct stat_list *stp;
	struct si_tcsi stc;
	int donefirst = 0;

	printf("%s: ", alldev ? "ALL" : Devname);
	acp = malloc(strlen(Devname) + 3);
	memset(acp, ' ', strlen(Devname));
	strcat(acp, "  ");
	stc = tc;
	for (stp = stat_list; stp->st_func != NULL; stp++) {
		if (donefirst)
			fputs(acp, stdout);
		else
			donefirst++;
		av[0] = NULL;
		tc = stc;
		(*stp->st_func)(-1, av);
	}
}

/*
 * debug
 * debug [[set|add|del debug_lvls] | [off]]
 */
void
debug(int ac, char **av)
{
	int level;

	if (ac > 2)
		prusage(U_DEBUG, 1);
	if (alldev) {
		if (ioctl(ctlfd, TCSIGDBG_ALL, &tc.tc_dbglvl) < 0)
			err(1, "TCSIGDBG_ALL on %s", Devname);
	} else {
		if (ioctl(ctlfd, TCSIGDBG_LEVEL, &tc) < 0)
			err(1, "TCSIGDBG_LEVEL on %s", Devname);
	}

	switch (ac) {
	case 0:
		printf("%s: debug levels - ", Devname);
		prlevels(tc.tc_dbglvl);
		return;
	case 1:
		if (strcmp(av[0], "off") == 0) {
			tc.tc_dbglvl = 0;
			break;
		}
		prusage(U_DEBUG, 1);
		/* no return */
	case 2:
		level = lvls2bits(av[1]);
		if (strcmp(av[0], "add") == 0)
			tc.tc_dbglvl |= level;
		else if (strcmp(av[0], "del") == 0)
			tc.tc_dbglvl &= ~level;
		else if (strcmp(av[0], "set") == 0)
			tc.tc_dbglvl = level;
		else
			prusage(U_DEBUG, 1);
	}
	if (alldev) {
		if (ioctl(ctlfd, TCSISDBG_ALL, &tc.tc_dbglvl) < 0)
			err(1, "TCSISDBG_ALL on %s", Devname);
	} else {
		if (ioctl(ctlfd, TCSISDBG_LEVEL, &tc) < 0)
			err(1, "TCSISDBG_LEVEL on %s", Devname);
	}
}

void
rxint(int ac, char **av)
{
	tc.tc_port = 0;
	switch (ac) {
	case 0:
		printf("%s: ", Devname);
	case -1:
		if (ioctl(ctlfd, TCSIGRXIT, &tc) < 0)
			err(1, "TCSIGRXIT");
		printf("RX interrupt throttle: %d msec\n", tc.tc_int*10);
		break;
	case 1:
		tc.tc_int = getnum(av[0]) / 10;
		if (tc.tc_int == 0)
			tc.tc_int = 1;
		if (ioctl(ctlfd, TCSIRXIT, &tc) < 0)
			err(1, "TCSIRXIT on %s at %d msec",
				Devname, tc.tc_int*10);
		break;
	default:
		prusage(U_RXINT, 1);
	}
}

void
txint(int ac, char **av)
{

	tc.tc_port = 0;
	switch (ac) {
	case 0:
		printf("%s: ", Devname);
	case -1:
		if (ioctl(ctlfd, TCSIGIT, &tc) < 0)
			err(1, "TCSIGIT");
		printf("aggregate interrupt throttle: %d\n", tc.tc_int);
		break;
	case 1:
		tc.tc_int = getnum(av[0]);
		if (ioctl(ctlfd, TCSIIT, &tc) < 0)
			err(1, "TCSIIT on %s at %d", Devname, tc.tc_int);
		break;
	default:
		prusage(U_TXINT, 1);
	}
}

void
onoff(int ac, char **av, int cmd, char *cmdstr, char *prstr, int usage)
{
	if (ac > 1)
		prusage(usage, 1);
	if (ac == 1) {
		if (strcmp(av[0], "on") == 0)
			tc.tc_int = 1;
		else if (strcmp(av[0], "off") == 0)
			tc.tc_int = 0;
		else
			prusage(usage, 1);
	} else
		tc.tc_int = -1;
	if (ioctl(ctlfd, cmd, &tc) < 0)
		err(1, "%s on %s", cmdstr, Devname);
	switch (ac) {
	case 0:
		printf("%s: ", Devname);
	case -1:
		printf("%s ", prstr);
		if (tc.tc_int)
			printf("on\n");
		else
			printf("off\n");
	}
}

void
mstate(int ac, char **av)
{
	switch (ac) {
	case 0:
		printf("%s: ", Devname);
	case -1:
		break;
	default:
		prusage(U_MSTATE, 1);
	}
	if (ioctl(ctlfd, TCSISTATE, &tc) < 0)
		err(1, "TCSISTATE on %s", Devname);
	printf("modem bits state - (0x%x)", tc.tc_int);
	if (tc.tc_int & IP_DCD)	printf(" DCD");
	if (tc.tc_int & IP_DTR)	printf(" DTR");
	if (tc.tc_int & IP_RTS)	printf(" RTS");
	printf("\n");
}

void
nport(int ac, char **av)
{
	int ports;

	if (ac != 0)
		prusage(U_NPORT, 1);
	if (ioctl(ctlfd, TCSIPORTS, &ports) < 0)
		err(1, "TCSIPORTS on %s", Devname);
	printf("SLXOS: total of %d ports\n", ports);
}

const char *s_stat(int stat)
{
	switch (stat) {
	case IDLE_OPEN:	return "IDLE_OPEN";
	case LOPEN:	return "LOPEN";
	case MOPEN:	return "MOPEN";
	case MPEND:	return "MPEND";
	case CONFIG:	return "CONFIG";
	case CLOSE:	return "CLOSE";
	case SBREAK:	return "SBREAK";
	case EBREAK:	return "EBREAK";
	case IDLE_CLOSE:return "IDLE_CLOSE";
	case IDLE_BREAK:return "IDLE_BREAK";
	case FCLOSE:	return "FCLOSE";
	case RESUME:	return "RESUME";
	case WFLUSH:	return "WFLUSH";
	case RFLUSH:	return "RFLUSH";
	default: return "??";
	}
}
const char *s_mr1(int mr1)
{
	static char msg[200];

	sprintf(msg, "%dbit, %s, parity:[", 5 + (mr1 & MR1_8_BITS), mr1 & MR1_ODD ? "odd" : "even");
	if (mr1 & MR1_WITH)
		strcat(msg, "with;");
	if (mr1 & MR1_FORCE)
		strcat(msg, "force;");
	if (mr1 & MR1_NONE)
		strcat(msg, "none;");
	if (mr1 & MR1_SPECIAL)
		strcat(msg, "special;");
	strcpy(msg + strlen(msg) - 1, "]");
	sprintf(msg + strlen(msg), ", err: %s", mr1 & MR1_BLOCK ? "block" : "none");
	sprintf(msg + strlen(msg), ", cts: %s", mr1 & MR1_CTSCONT ? "auto" : "none");
	return (msg);
}
const char *s_mr2(int mr2)
{
	static char msg[200];

	switch (mr2 & 0xf) {
	case MR2_1_STOP: strcpy(msg, "1stop"); break;
	case MR2_2_STOP: strcpy(msg, "2stop"); break;
	default: sprintf(msg, "??stop (0x%x)", mr2 & 0xf); break;
	}
	if (mr2 & MR2_RTSCONT)	strcat(msg, ", rtscont");
	if (mr2 & MR2_CTSCONT)	strcat(msg, ", ctscont");
	switch (mr2 & 0xc0) {
	case MR2_NORMAL: strcat(msg, ", mode:normal"); break;
	case MR2_AUTO: strcat(msg, ", mode:auto"); break;
	case MR2_LOCAL: strcat(msg, ", mode:local"); break;
	case MR2_REMOTE: strcat(msg, ", mode:remote"); break;
	}
	return (msg);
}
const char *s_clk(int clk)
{
	switch (clk & 0xf) {
	case 0x0: return "75";
	case 0x1: return "110/115200";
	case 0x2: return "38400";
	case 0x3: return "150";
	case 0x4: return "300";
	case 0x5: return "600";
	case 0x6: return "1200";
	case 0x7: return "2000";
	case 0x8: return "2400";
	case 0x9: return "4800";
	case 0xa: return "7200";
	case 0xb: return "9600";
	case 0xc: return "19200";
	case 0xd: return "57600";
	case 0xe: return "?0xe";
	case 0xf: return "?0xf";
	}
	return ("gcc sucks");
}
const char *s_op(int op)
{
	static char msg[200];
	
	sprintf(msg, "cts:%s", (op & OP_CTS) ? "on" : "off");
	sprintf(msg + strlen(msg), ", dsr:%s", (op & OP_DSR) ? "on" : "off");
	return (msg);
}

const char *s_ip(int ip)
{
	static char msg[200];
	
	sprintf(msg, "rts:%s", (ip & IP_RTS) ? "on" : "off");
	sprintf(msg + strlen(msg), ", dcd:%s", (ip & IP_DCD) ? "on" : "off");
	sprintf(msg + strlen(msg), ", dtr:%s", (ip & IP_DTR) ? "on" : "off");
	sprintf(msg + strlen(msg), ", ri:%s", (ip & IP_RI) ? "on" : "off");
	return (msg);
}

const char *s_state(int state)
{
	return (state & ST_BREAK ? "break:on" : "break:off");
}

const char *s_prtcl(int pr)
{
	static char msg[200];

	sprintf(msg, "tx xon any:%s", (pr & SP_TANY) ? "on" : "off");
	sprintf(msg + strlen(msg), ", tx xon/xoff:%s", (pr & SP_TXEN) ? "on" : "off");
	sprintf(msg + strlen(msg), ", cooking:%s", (pr & SP_CEN) ? "on" : "off");
	sprintf(msg + strlen(msg), ", rx xon/xoff:%s", (pr & SP_RXEN) ? "on" : "off");
	sprintf(msg + strlen(msg), ", dcd/dsr check:%s", (pr & SP_DCEN) ? "on" : "off");
	sprintf(msg + strlen(msg), ", parity check:%s", (pr & SP_PAEN) ? "on" : "off");
	return (msg);
}
const char *s_break(int br)
{
	static char msg[200];

	sprintf(msg, "ignore rx brk:%s", (br & BR_IGN) ? "on" : "off");
	sprintf(msg + strlen(msg), ", brk interrupt:%s", (br & BR_INT) ? "on" : "off");
	sprintf(msg + strlen(msg), ", parmrking:%s", (br & BR_PARMRK) ? "on" : "off");
	sprintf(msg + strlen(msg), ", parign:%s", (br & BR_PARIGN) ? "on" : "off");
	return (msg);
}

const char *
s_xstat(int xs)
{
	static char msg[200];

	msg[0] = 0;
	/* MTA definitions, not TA */
	if (xs & 0x01) strcat(msg, "TION ");	/* Tx interrupts on (MTA only) */
	if (xs & 0x02) strcat(msg, "RTSEN ");	/* RTS FLOW enabled (MTA only) */
	if (xs & 0x04) strcat(msg, "RTSLOW ");	/* XOFF received (TA only) */
	if (xs & 0x08) strcat(msg, "RXEN ");	/* Rx XON/XOFF enabled */
	if (xs & 0x10) strcat(msg, "ANYXO ");	/* XOFF pending/sent or RTS dropped */
	if (xs & 0x20) strcat(msg, "RXSE ");	/* Rx XOFF sent */
	if (xs & 0x40) strcat(msg, "NPEND ");	/* Rx XON pending or XOFF pending */
	if (xs & 0x40) strcat(msg, "FPEND ");	/* Rx XOFF pending */
	return (msg);
}

const char *
s_cstat(int cs)
{
	static char msg[200];

	msg[0] = 0;
	/* MTA definitions, not TA */
	if (cs & 0x01) strcat(msg, "TEMR ");	/* Tx empty requested (MTA only) */
	if (cs & 0x02) strcat(msg, "TEMA ");	/* Tx empty acked (MTA only) */
	if (cs & 0x04) strcat(msg, "EN ");	/* Cooking enabled (on MTA means port is also || */
	if (cs & 0x08) strcat(msg, "HIGH ");	/* Buffer previously hit high water */
	if (cs & 0x10) strcat(msg, "CTSEN ");	/* CTS automatic flow-control enabled */
	if (cs & 0x20) strcat(msg, "DCDEN ");	/* DCD/DTR checking enabled */
	if (cs & 0x40) strcat(msg, "BREAK ");	/* Break detected */
	if (cs & 0x80) strcat(msg, "RTSEN ");	/* RTS automatic flow control enabled (MTA only) */
	return (msg);
}

void
ccb_stat(int ac, char **av)
{
	struct si_pstat sip;
#define	CCB	sip.tc_ccb

	if (ac != 0)
		prusage(U_STAT_CCB, 1);
	sip.tc_dev = tc.tc_dev;
	if (ioctl(ctlfd, TCSI_CCB, &sip) < 0)
		err(1, "TCSI_CCB on %s", Devname);
	printf("%s: ", Devname);

							/* WORD	next - Next Channel */
							/* WORD	addr_uart - Uart address */
							/* WORD	module - address of module struct */
	printf("\tuart_type 0x%x\n", CCB.type);		/* BYTE type - Uart type */
							/* BYTE	fill - */
	printf("\tx_status 0x%x %s\n", CCB.x_status, s_xstat(CCB.x_status));	/* BYTE	x_status - XON / XOFF status */
	printf("\tc_status 0x%x %s\n", CCB.c_status, s_cstat(CCB.c_status));	/* BYTE	c_status - cooking status */
	printf("\thi_rxipos 0x%x\n", CCB.hi_rxipos);	/* BYTE	hi_rxipos - stuff into rx buff */
	printf("\thi_rxopos 0x%x\n", CCB.hi_rxopos);	/* BYTE	hi_rxopos - stuff out of rx buffer */
	printf("\thi_txopos 0x%x\n", CCB.hi_txopos);	/* BYTE	hi_txopos - Stuff into tx ptr */
	printf("\thi_txipos 0x%x\n", CCB.hi_txipos);	/* BYTE	hi_txipos - ditto out */
	printf("\thi_stat 0x%x %s\n", CCB.hi_stat, s_stat(CCB.hi_stat));/* BYTE	hi_stat - Command register */
	printf("\tdsr_bit 0x%x\n", CCB.dsr_bit);		/* BYTE	dsr_bit - Magic bit for DSR */
	printf("\ttxon 0x%x\n", CCB.txon);		/* BYTE	txon - TX XON char */
	printf("\ttxoff 0x%x\n", CCB.txoff);		/* BYTE	txoff - ditto XOFF */
	printf("\trxon 0x%x\n", CCB.rxon);		/* BYTE	rxon - RX XON char */
	printf("\trxoff 0x%x\n", CCB.rxoff);		/* BYTE	rxoff - ditto XOFF */
	printf("\thi_mr1 0x%x %s\n", CCB.hi_mr1, s_mr1(CCB.hi_mr1));		/* BYTE	hi_mr1 - mode 1 image */
	printf("\thi_mr2 0x%x %s\n", CCB.hi_mr2, s_mr2(CCB.hi_mr2));		/* BYTE	hi_mr2 - mode 2 image */
        printf("\thi_csr 0x%x in:%s out:%s\n", CCB.hi_csr, s_clk(CCB.hi_csr >> 4), s_clk(CCB.hi_csr));		/* BYTE	hi_csr - clock register */
	printf("\thi_op 0x%x %s\n", CCB.hi_op, s_op(CCB.hi_op));		/* BYTE	hi_op - Op control */
	printf("\thi_ip 0x%x %s\n", CCB.hi_ip, s_ip(CCB.hi_ip));		/* BYTE	hi_ip - Input pins */
	printf("\thi_state 0x%x %s\n", CCB.hi_state, s_state(CCB.hi_state));	/* BYTE	hi_state - status */
	printf("\thi_prtcl 0x%x %s\n", CCB.hi_prtcl, s_prtcl(CCB.hi_prtcl));	/* BYTE	hi_prtcl - Protocol */
	printf("\thi_txon 0x%x\n", CCB.hi_txon);		/* BYTE	hi_txon - host copy tx xon stuff */
	printf("\thi_txoff 0x%x\n", CCB.hi_txoff);	/* BYTE	hi_txoff - */
	printf("\thi_rxon 0x%x\n", CCB.hi_rxon);		/* BYTE	hi_rxon - */
	printf("\thi_rxoff 0x%x\n", CCB.hi_rxoff);	/* BYTE	hi_rxoff - */
	printf("\tclose_prev 0x%x\n", CCB.close_prev);	/* BYTE	close_prev - Was channel previously closed */
	printf("\thi_break 0x%x %s\n", CCB.hi_break, s_break(CCB.hi_break));	/* BYTE	hi_break - host copy break process */
	printf("\tbreak_state 0x%x\n", CCB.break_state);	/* BYTE	break_state - local copy ditto */
	printf("\thi_mask 0x%x\n", CCB.hi_mask);		/* BYTE	hi_mask - Mask for CS7 etc. */
	printf("\tmask_z280 0x%x\n", CCB.mask_z280);	/* BYTE	mask_z280 - Z280's copy */
							/* BYTE	res[0x60 - 36] - */
							/* BYTE	hi_txbuf[SLXOS_BUFFERSIZE] - */
							/* BYTE	hi_rxbuf[SLXOS_BUFFERSIZE] - */
							/* BYTE	res1[0xA0] - */
}

const char *sp_state(int st)
{

	if (st & SS_LSTART)
		return("lstart ");
	else
		return("");
}

void
port_stat(int ac, char **av)
{
	struct si_pstat sip;
#define	PRT	sip.tc_siport

	if (ac != 0)
		prusage(U_STAT_PORT, 1);
	sip.tc_dev = tc.tc_dev;
	if (ioctl(ctlfd, TCSI_PORT, &sip) < 0)
		err(1, "TCSI_PORT on %s", Devname);
	printf("%s: ", Devname);

	printf("\tsp_pend 0x%x %s\n", PRT.sp_pend, s_stat(PRT.sp_pend));
	printf("\tsp_last_hi_ip 0x%x %s\n", PRT.sp_last_hi_ip, s_ip(PRT.sp_last_hi_ip));
	printf("\tsp_state 0x%x %s\n", PRT.sp_state, sp_state(PRT.sp_state));
	printf("\tsp_delta_overflows 0x%d\n", PRT.sp_delta_overflows);
}

int
islevel(char *tk)
{
	struct lv *lvp;
	char *acp;

	for (acp = tk; *acp; acp++)
		if (isupper(*acp))
			*acp = tolower(*acp);
	for (lvp = lv; lvp->lv_name; lvp++)
		if (strcmp(lvp->lv_name, tk) == 0)
			return(lvp->lv_bit);
	return(0);
}

/*
 * Convert a string consisting of tokens separated by white space, commas
 * or `|' into a bitfield - flag any unrecognised tokens.
 */
int
lvls2bits(char *str)
{
	int i, bits = 0;
	int errflag = 0;
	char token[20];

	while (sscanf(str, "%[^,| \t]", token) == 1) {
		str += strlen(token);
		while (isspace(*str) || *str==',' || *str=='|')
			str++;
		if (strcmp(token, "all") == 0)
			return(0xffffffff);
		if ((i = islevel(token)) == 0) {
			warnx("unknown token '%s'", token);
			errflag++;
		} else
			bits |= i;
	}
	if (errflag)
		exit(1);

	return(bits);
}

int
getnum(char *str)
{
	int x;
	char *acp = str;

	x = 0;
	while (*acp) {
		if (!isdigit(*acp))
			errx(1, "%s is not a number", str);
		x *= 10;
		x += (*acp - '0');
		acp++;
	}
	return(x);
}

void
prlevels(int x)
{
	struct lv *lvp;

	switch (x) {
	case 0:
		printf("(none)\n");
		break;
	case 0xffffffff:
		printf("all\n");
		break;
	default:
		for (lvp = lv; lvp->lv_name; lvp++)
			if (x & lvp->lv_bit)
				printf(" %s", lvp->lv_name);
		printf("\n");
	}
}
