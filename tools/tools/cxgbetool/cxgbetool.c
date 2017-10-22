/*-
 * Copyright (c) 2011 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: Navdeep Parhar <np@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "t4_ioctl.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define	max(x, y) ((x) > (y) ? (x) : (y))

static const char *progname, *nexus;

struct reg_info {
	const char *name;
	uint32_t addr;
	uint32_t len;
};

struct mod_regs {
	const char *name;
	const struct reg_info *ri;
};

struct field_desc {
	const char *name;     /* Field name */
	unsigned short start; /* Start bit position */
	unsigned short end;   /* End bit position */
	unsigned char shift;  /* # of low order bits omitted and implicitly 0 */
	unsigned char hex;    /* Print field in hex instead of decimal */
	unsigned char islog2; /* Field contains the base-2 log of the value */
};

#include "reg_defs_t4.c"
#include "reg_defs_t4vf.c"

static void
usage(FILE *fp)
{
	fprintf(fp, "Usage: %s <nexus> [operation]\n", progname);
	fprintf(fp,
	    "\tcontext <type> <id>                 show an SGE context\n"
	    "\tfilter <idx> [<param> <val>] ...    set a filter\n"
	    "\tfilter <idx> delete|clear           delete a filter\n"
	    "\tfilter list                         list all filters\n"
	    "\tfilter mode [<match>] ...           get/set global filter mode\n"
	    "\tloadfw <fw-image.bin>               install firmware\n"
	    "\tmemdump <addr> <len>                dump a memory range\n"
	    "\treg <address>[=<val>]               read/write register\n"
	    "\treg64 <address>[=<val>]             read/write 64 bit register\n"
	    "\tregdump [<module>] ...              dump registers\n"
	    "\tstdio                               interactive mode\n"
	    "\ttcb <tid>                           read TCB\n"
	    );
}

static inline unsigned int
get_card_vers(unsigned int version)
{
	return (version & 0x3ff);
}

static int
real_doit(unsigned long cmd, void *data, const char *cmdstr)
{
	static int fd = -1;
	int rc = 0;

	if (fd == -1) {
		char buf[64];

		snprintf(buf, sizeof(buf), "/dev/%s", nexus);
		if ((fd = open(buf, O_RDWR)) < 0) {
			warn("open(%s)", nexus);
			rc = errno;
			return (rc);
		}
	}

	rc = ioctl(fd, cmd, data);
	if (rc < 0) {
		warn("%s", cmdstr);
		rc = errno;
	}

	return (rc);
}
#define doit(x, y) real_doit(x, y, #x)

static char *
str_to_number(const char *s, long *val, long long *vall)
{
	char *p;

	if (vall)
		*vall = strtoll(s, &p, 0);
	else if (val)
		*val = strtol(s, &p, 0);
	else
		p = NULL;

	return (p);
}

static int
read_reg(long addr, int size, long long *val)
{
	struct t4_reg reg;
	int rc;

	reg.addr = (uint32_t) addr;
	reg.size = (uint32_t) size;
	reg.val = 0;

	rc = doit(CHELSIO_T4_GETREG, &reg);

	*val = reg.val;

	return (rc);
}

static int
write_reg(long addr, int size, long long val)
{
	struct t4_reg reg;

	reg.addr = (uint32_t) addr;
	reg.size = (uint32_t) size;
	reg.val = (uint64_t) val;

	return doit(CHELSIO_T4_SETREG, &reg);
}

static int
register_io(int argc, const char *argv[], int size)
{
	char *p, *v;
	long addr;
	long long val;
	int w = 0, rc;

	if (argc == 1) {
		/* <reg> OR <reg>=<value> */

		p = str_to_number(argv[0], &addr, NULL);
		if (*p) {
			if (*p != '=') {
				warnx("invalid register \"%s\"", argv[0]);
				return (EINVAL);
			}

			w = 1;
			v = p + 1;
			p = str_to_number(v, NULL, &val);

			if (*p) {
				warnx("invalid value \"%s\"", v);
				return (EINVAL);
			}
		}

	} else if (argc == 2) {
		/* <reg> <value> */

		w = 1;

		p = str_to_number(argv[0], &addr, NULL);
		if (*p) {
			warnx("invalid register \"%s\"", argv[0]);
			return (EINVAL);
		}

		p = str_to_number(argv[1], NULL, &val);
		if (*p) {
			warnx("invalid value \"%s\"", argv[1]);
			return (EINVAL);
		}
	} else {
		warnx("reg: invalid number of arguments (%d)", argc);
		return (EINVAL);
	}

	if (w)
		rc = write_reg(addr, size, val);
	else {
		rc = read_reg(addr, size, &val);
		if (rc == 0)
			printf("0x%llx [%llu]\n", val, val);
	}

	return (rc);
}

static inline uint32_t
xtract(uint32_t val, int shift, int len)
{
	return (val >> shift) & ((1 << len) - 1);
}

static int
dump_block_regs(const struct reg_info *reg_array, const uint32_t *regs)
{
	uint32_t reg_val = 0;

	for ( ; reg_array->name; ++reg_array)
		if (!reg_array->len) {
			reg_val = regs[reg_array->addr / 4];
			printf("[%#7x] %-47s %#-10x %u\n", reg_array->addr,
			       reg_array->name, reg_val, reg_val);
		} else {
			uint32_t v = xtract(reg_val, reg_array->addr,
					    reg_array->len);

			printf("    %*u:%u %-47s %#-10x %u\n",
			       reg_array->addr < 10 ? 3 : 2,
			       reg_array->addr + reg_array->len - 1,
			       reg_array->addr, reg_array->name, v, v);
		}

	return (1);
}

static int
dump_regs_table(int argc, const char *argv[], const uint32_t *regs,
    const struct mod_regs *modtab, int nmodules)
{
	int i, j, match;

	for (i = 0; i < argc; i++) {
		for (j = 0; j < nmodules; j++) {
			if (!strcmp(argv[i], modtab[j].name))
				break;
		}

		if (j == nmodules) {
			warnx("invalid register block \"%s\"", argv[i]);
			fprintf(stderr, "\nAvailable blocks:");
			for ( ; nmodules; nmodules--, modtab++)
				fprintf(stderr, " %s", modtab->name);
			fprintf(stderr, "\n");
			return (EINVAL);
		}
	}

	for ( ; nmodules; nmodules--, modtab++) {

		match = argc == 0 ? 1 : 0;
		for (i = 0; !match && i < argc; i++) {
			if (!strcmp(argv[i], modtab->name))
				match = 1;
		}

		if (match)
			dump_block_regs(modtab->ri, regs);
	}

	return (0);
}

#define T4_MODREGS(name) { #name, t4_##name##_regs }
static int
dump_regs_t4(int argc, const char *argv[], const uint32_t *regs)
{
	static struct mod_regs t4_mod[] = {
		T4_MODREGS(sge),
		{ "pci", t4_pcie_regs },
		T4_MODREGS(dbg),
		T4_MODREGS(mc),
		T4_MODREGS(ma),
		{ "edc0", t4_edc_0_regs },
		{ "edc1", t4_edc_1_regs },
		T4_MODREGS(cim), 
		T4_MODREGS(tp),
		T4_MODREGS(ulp_rx),
		T4_MODREGS(ulp_tx),
		{ "pmrx", t4_pm_rx_regs },
		{ "pmtx", t4_pm_tx_regs },
		T4_MODREGS(mps),
		{ "cplsw", t4_cpl_switch_regs },
		T4_MODREGS(smb),
		{ "i2c", t4_i2cm_regs },
		T4_MODREGS(mi),
		T4_MODREGS(uart),
		T4_MODREGS(pmu), 
		T4_MODREGS(sf),
		T4_MODREGS(pl),
		T4_MODREGS(le),
		T4_MODREGS(ncsi),
		T4_MODREGS(xgmac)
	};

	return dump_regs_table(argc, argv, regs, t4_mod, ARRAY_SIZE(t4_mod));
}
#undef T4_MODREGS

static int
dump_regs_t4vf(int argc, const char *argv[], const uint32_t *regs)
{
	static struct mod_regs t4vf_mod[] = {
		{ "sge", t4vf_sge_regs },
		{ "mps", t4vf_mps_regs },
		{ "pl", t4vf_pl_regs },
		{ "mbdata", t4vf_mbdata_regs },
		{ "cim", t4vf_cim_regs },
	};

	return dump_regs_table(argc, argv, regs, t4vf_mod,
	    ARRAY_SIZE(t4vf_mod));
}

static int
dump_regs(int argc, const char *argv[])
{
	int vers, revision, is_pcie, rc;
	struct t4_regdump regs;

	regs.data = calloc(1, T4_REGDUMP_SIZE);
	if (regs.data == NULL) {
		warnc(ENOMEM, "regdump");
		return (ENOMEM);
	}

	regs.len = T4_REGDUMP_SIZE;
	rc = doit(CHELSIO_T4_REGDUMP, &regs);
	if (rc != 0)
		return (rc);

	vers = get_card_vers(regs.version);
	revision = (regs.version >> 10) & 0x3f;
	is_pcie = (regs.version & 0x80000000) != 0;

	if (vers == 4) {
		if (revision == 0x3f)
			rc = dump_regs_t4vf(argc, argv, regs.data);
		else
			rc = dump_regs_t4(argc, argv, regs.data);
	} else {
		warnx("%s (type %d, rev %d) is not a T4 card.",
		    nexus, vers, revision);
		return (ENOTSUP);
	}

	free(regs.data);
	return (rc);
}

static void
do_show_info_header(uint32_t mode)
{
	uint32_t i;

	printf ("%4s %8s", "Idx", "Hits");
	for (i = T4_FILTER_FCoE; i <= T4_FILTER_IP_FRAGMENT; i <<= 1) {
		switch (mode & i) {
		case T4_FILTER_FCoE:
			printf (" FCoE");
			break;

		case T4_FILTER_PORT:
			printf (" Port");
			break;

		case T4_FILTER_VNIC:
			printf ("      vld:VNIC");
			break;

		case T4_FILTER_VLAN:
			printf ("      vld:VLAN");
			break;

		case T4_FILTER_IP_TOS:
			printf ("   TOS");
			break;

		case T4_FILTER_IP_PROTO:
			printf ("  Prot");
			break;

		case T4_FILTER_ETH_TYPE:
			printf ("   EthType");
			break;

		case T4_FILTER_MAC_IDX:
			printf ("  MACIdx");
			break;

		case T4_FILTER_MPS_HIT_TYPE:
			printf (" MPS");
			break;

		case T4_FILTER_IP_FRAGMENT:
			printf (" Frag");
			break;

		default:
			/* compressed filter field not enabled */
			break;
		}
	}
	printf(" %20s %20s %9s %9s %s\n",
	    "DIP", "SIP", "DPORT", "SPORT", "Action");
}

/*
 * Parse an argument sub-vector as a { <parameter name> <value>[:<mask>] }
 * ordered tuple.  If the parameter name in the argument sub-vector does not
 * match the passed in parameter name, then a zero is returned for the
 * function and no parsing is performed.  If there is a match, then the value
 * and optional mask are parsed and returned in the provided return value
 * pointers.  If no optional mask is specified, then a default mask of all 1s
 * will be returned.
 *
 * An error in parsing the value[:mask] will result in an error message and
 * program termination.
 */
static int
parse_val_mask(const char *param, const char *args[], uint32_t *val,
    uint32_t *mask)
{
	char *p;

	if (strcmp(param, args[0]) != 0)
		return (EINVAL);

	*val = strtoul(args[1], &p, 0);
	if (p > args[1]) {
		if (p[0] == 0) {
			*mask = ~0;
			return (0);
		}

		if (p[0] == ':' && p[1] != 0) {
			*mask = strtoul(p+1, &p, 0);
			if (p[0] == 0)
				return (0);
		}
	}

	warnx("parameter \"%s\" has bad \"value[:mask]\" %s",
	    args[0], args[1]);

	return (EINVAL);
}

/*
 * Parse an argument sub-vector as a { <parameter name> <addr>[/<mask>] }
 * ordered tuple.  If the parameter name in the argument sub-vector does not
 * match the passed in parameter name, then a zero is returned for the
 * function and no parsing is performed.  If there is a match, then the value
 * and optional mask are parsed and returned in the provided return value
 * pointers.  If no optional mask is specified, then a default mask of all 1s
 * will be returned.
 *
 * The value return parameter "afp" is used to specify the expected address
 * family -- IPv4 or IPv6 -- of the address[/mask] and return its actual
 * format.  A passed in value of AF_UNSPEC indicates that either IPv4 or IPv6
 * is acceptable; AF_INET means that only IPv4 addresses are acceptable; and
 * AF_INET6 means that only IPv6 are acceptable.  AF_INET is returned for IPv4
 * and AF_INET6 for IPv6 addresses, respectively.  IPv4 address/mask pairs are
 * returned in the first four bytes of the address and mask return values with
 * the address A.B.C.D returned with { A, B, C, D } returned in addresses { 0,
 * 1, 2, 3}, respectively.
 *
 * An error in parsing the value[:mask] will result in an error message and
 * program termination.
 */
static int
parse_ipaddr(const char *param, const char *args[], int *afp, uint8_t addr[],
    uint8_t mask[])
{
	const char *colon, *afn;
	char *slash;
	uint8_t *m;
	int af, ret;
	unsigned int masksize;

	/*
	 * Is this our parameter?
	 */
	if (strcmp(param, args[0]) != 0)
		return (EINVAL);

	/*
	 * Fundamental IPv4 versus IPv6 selection.
	 */
	colon = strchr(args[1], ':');
	if (!colon) {
		afn = "IPv4";
		af = AF_INET;
		masksize = 32;
	} else {
		afn = "IPv6";
		af = AF_INET6;
		masksize = 128;
	}
	if (*afp == AF_UNSPEC)
		*afp = af;
	else if (*afp != af) {
		warnx("address %s is not of expected family %s",
		    args[1], *afp == AF_INET ? "IP" : "IPv6");
		return (EINVAL);
	}

	/*
	 * Parse address (temporarily stripping off any "/mask"
	 * specification).
	 */
	slash = strchr(args[1], '/');
	if (slash)
		*slash = 0;
	ret = inet_pton(af, args[1], addr);
	if (slash)
		*slash = '/';
	if (ret <= 0) {
		warnx("Cannot parse %s %s address %s", param, afn, args[1]);
		return (EINVAL);
	}

	/*
	 * Parse optional mask specification.
	 */
	if (slash) {
		char *p;
		unsigned int prefix = strtoul(slash + 1, &p, 10);

		if (p == slash + 1) {
			warnx("missing address prefix for %s", param);
			return (EINVAL);
		}
		if (*p) {
			warnx("%s is not a valid address prefix", slash + 1);
			return (EINVAL);
		}
		if (prefix > masksize) {
			warnx("prefix %u is too long for an %s address",
			     prefix, afn);
			return (EINVAL);
		}
		memset(mask, 0, masksize / 8);
		masksize = prefix;
	}

	/*
	 * Fill in mask.
	 */
	for (m = mask; masksize >= 8; m++, masksize -= 8)
		*m = ~0;
	if (masksize)
		*m = ~0 << (8 - masksize);

	return (0);
}

/*
 * Parse an argument sub-vector as a { <parameter name> <value> } ordered
 * tuple.  If the parameter name in the argument sub-vector does not match the
 * passed in parameter name, then a zero is returned for the function and no
 * parsing is performed.  If there is a match, then the value is parsed and
 * returned in the provided return value pointer.
 */
static int
parse_val(const char *param, const char *args[], uint32_t *val)
{
	char *p;

	if (strcmp(param, args[0]) != 0)
		return (EINVAL);

	*val = strtoul(args[1], &p, 0);
	if (p > args[1] && p[0] == 0)
		return (0);

	warnx("parameter \"%s\" has bad \"value\" %s", args[0], args[1]);
	return (EINVAL);
}

static void
filters_show_ipaddr(int type, uint8_t *addr, uint8_t *addrm)
{
	int noctets, octet;

	printf(" ");
	if (type == 0) {
		noctets = 4;
		printf("%3s", " ");
	} else
	noctets = 16;

	for (octet = 0; octet < noctets; octet++)
		printf("%02x", addr[octet]);
	printf("/");
	for (octet = 0; octet < noctets; octet++)
		printf("%02x", addrm[octet]);
}

static void
do_show_one_filter_info(struct t4_filter *t, uint32_t mode)
{
	uint32_t i;

	printf("%4d", t->idx);
	if (t->hits == UINT64_MAX)
		printf(" %8s", "-");
	else
		printf(" %8ju", t->hits);

	/*
	 * Compressed header portion of filter.
	 */
	for (i = T4_FILTER_FCoE; i <= T4_FILTER_IP_FRAGMENT; i <<= 1) {
		switch (mode & i) {
		case T4_FILTER_FCoE:
			printf("  %1d/%1d", t->fs.val.fcoe, t->fs.mask.fcoe);
			break;

		case T4_FILTER_PORT:
			printf("  %1d/%1d", t->fs.val.iport, t->fs.mask.iport);
			break;

		case T4_FILTER_VNIC:
			printf(" %1d:%1x:%02x/%1d:%1x:%02x",
			    t->fs.val.vnic_vld, (t->fs.val.vnic >> 7) & 0x7,
			    t->fs.val.vnic & 0x7f, t->fs.mask.vnic_vld,
			    (t->fs.mask.vnic >> 7) & 0x7,
			    t->fs.mask.vnic & 0x7f);
			break;

		case T4_FILTER_VLAN:
			printf(" %1d:%04x/%1d:%04x",
			    t->fs.val.vlan_vld, t->fs.val.vlan,
			    t->fs.mask.vlan_vld, t->fs.mask.vlan);
			break;

		case T4_FILTER_IP_TOS:
			printf(" %02x/%02x", t->fs.val.tos, t->fs.mask.tos);
			break;

		case T4_FILTER_IP_PROTO:
			printf(" %02x/%02x", t->fs.val.proto, t->fs.mask.proto);
			break;

		case T4_FILTER_ETH_TYPE:
			printf(" %04x/%04x", t->fs.val.ethtype,
			    t->fs.mask.ethtype);
			break;

		case T4_FILTER_MAC_IDX:
			printf(" %03x/%03x", t->fs.val.macidx,
			    t->fs.mask.macidx);
			break;

		case T4_FILTER_MPS_HIT_TYPE:
			printf(" %1x/%1x", t->fs.val.matchtype,
			    t->fs.mask.matchtype);
			break;

		case T4_FILTER_IP_FRAGMENT:
			printf("  %1d/%1d", t->fs.val.frag, t->fs.mask.frag);
			break;

		default:
			/* compressed filter field not enabled */
			break;
		}
	}

	/*
	 * Fixed portion of filter.
	 */
	filters_show_ipaddr(t->fs.type, t->fs.val.dip, t->fs.mask.dip);
	filters_show_ipaddr(t->fs.type, t->fs.val.sip, t->fs.mask.sip);
	printf(" %04x/%04x %04x/%04x",
		 t->fs.val.dport, t->fs.mask.dport,
		 t->fs.val.sport, t->fs.mask.sport);

	/*
	 * Variable length filter action.
	 */
	if (t->fs.action == FILTER_DROP)
		printf(" Drop");
	else if (t->fs.action == FILTER_SWITCH) {
		printf(" Switch: port=%d", t->fs.eport);
	if (t->fs.newdmac)
		printf(
			", dmac=%02x:%02x:%02x:%02x:%02x:%02x "
			", l2tidx=%d",
			t->fs.dmac[0], t->fs.dmac[1],
			t->fs.dmac[2], t->fs.dmac[3],
			t->fs.dmac[4], t->fs.dmac[5],
			t->l2tidx);
	if (t->fs.newsmac)
		printf(
			", smac=%02x:%02x:%02x:%02x:%02x:%02x "
			", smtidx=%d",
			t->fs.smac[0], t->fs.smac[1],
			t->fs.smac[2], t->fs.smac[3],
			t->fs.smac[4], t->fs.smac[5],
			t->smtidx);
	if (t->fs.newvlan == VLAN_REMOVE)
		printf(", vlan=none");
	else if (t->fs.newvlan == VLAN_INSERT)
		printf(", vlan=insert(%x)", t->fs.vlan);
	else if (t->fs.newvlan == VLAN_REWRITE)
		printf(", vlan=rewrite(%x)", t->fs.vlan);
	} else {
		printf(" Pass: Q=");
		if (t->fs.dirsteer == 0) {
			printf("RSS");
			if (t->fs.maskhash)
				printf("(TCB=hash)");
		} else {
			printf("%d", t->fs.iq);
			if (t->fs.dirsteerhash == 0)
				printf("(QID)");
			else
				printf("(hash)");
		}
	}
	if (t->fs.prio)
		printf(" Prio");
	if (t->fs.rpttid)
		printf(" RptTID");
	printf("\n");
}

static int
show_filters(void)
{
	uint32_t mode = 0, header = 0;
	struct t4_filter t;
	int rc;

	/* Get the global filter mode first */
	rc = doit(CHELSIO_T4_GET_FILTER_MODE, &mode);
	if (rc != 0)
		return (rc);

	t.idx = 0;
	for (t.idx = 0; ; t.idx++) {
		rc = doit(CHELSIO_T4_GET_FILTER, &t);
		if (rc != 0 || t.idx == 0xffffffff)
			break;

		if (!header) {
			do_show_info_header(mode);
			header = 1;
		}
		do_show_one_filter_info(&t, mode);
	};

	return (rc);
}

static int
get_filter_mode(void)
{
	uint32_t mode = 0;
	int rc;

	rc = doit(CHELSIO_T4_GET_FILTER_MODE, &mode);
	if (rc != 0)
		return (rc);

	if (mode & T4_FILTER_IPv4)
		printf("ipv4 ");

	if (mode & T4_FILTER_IPv6)
		printf("ipv6 ");

	if (mode & T4_FILTER_IP_SADDR)
		printf("sip ");
	
	if (mode & T4_FILTER_IP_DADDR)
		printf("dip ");

	if (mode & T4_FILTER_IP_SPORT)
		printf("sport ");

	if (mode & T4_FILTER_IP_DPORT)
		printf("dport ");

	if (mode & T4_FILTER_MPS_HIT_TYPE)
		printf("matchtype ");

	if (mode & T4_FILTER_MAC_IDX)
		printf("macidx ");

	if (mode & T4_FILTER_ETH_TYPE)
		printf("ethtype ");

	if (mode & T4_FILTER_IP_PROTO)
		printf("proto ");

	if (mode & T4_FILTER_IP_TOS)
		printf("tos ");

	if (mode & T4_FILTER_VLAN)
		printf("vlan ");

	if (mode & T4_FILTER_VNIC)
		printf("vnic ");

	if (mode & T4_FILTER_PORT)
		printf("iport ");

	if (mode & T4_FILTER_FCoE)
		printf("fcoe ");

	printf("\n");

	return (0);
}

static int
set_filter_mode(int argc, const char *argv[])
{
	uint32_t mode = 0;

	for (; argc; argc--, argv++) {
		if (!strcmp(argv[0], "matchtype"))
			mode |= T4_FILTER_MPS_HIT_TYPE;

		if (!strcmp(argv[0], "macidx"))
			mode |= T4_FILTER_MAC_IDX;

		if (!strcmp(argv[0], "ethtype"))
			mode |= T4_FILTER_ETH_TYPE;

		if (!strcmp(argv[0], "proto"))
			mode |= T4_FILTER_IP_PROTO;

		if (!strcmp(argv[0], "tos"))
			mode |= T4_FILTER_IP_TOS;

		if (!strcmp(argv[0], "vlan"))
			mode |= T4_FILTER_VLAN;

		if (!strcmp(argv[0], "ovlan") ||
		    !strcmp(argv[0], "vnic"))
			mode |= T4_FILTER_VNIC;

		if (!strcmp(argv[0], "iport"))
			mode |= T4_FILTER_PORT;

		if (!strcmp(argv[0], "fcoe"))
			mode |= T4_FILTER_FCoE;
	}

	return doit(CHELSIO_T4_SET_FILTER_MODE, &mode);
}

static int
del_filter(uint32_t idx)
{
	struct t4_filter t;

	t.idx = idx;

	return doit(CHELSIO_T4_DEL_FILTER, &t);
}

static int
set_filter(uint32_t idx, int argc, const char *argv[])
{
	int af = AF_UNSPEC, start_arg = 0;
	struct t4_filter t;

	if (argc < 2) {
		warnc(EINVAL, "%s", __func__);
		return (EINVAL);
	};
	bzero(&t, sizeof (t));
	t.idx = idx;

	for (start_arg = 0; start_arg + 2 <= argc; start_arg += 2) {
		const char **args = &argv[start_arg];
		uint32_t val, mask;

		if (!strcmp(argv[start_arg], "type")) {
			int newaf;
			if (!strcasecmp(argv[start_arg + 1], "ipv4"))
				newaf = AF_INET;
			else if (!strcasecmp(argv[start_arg + 1], "ipv6"))
				newaf = AF_INET6;
			else {
				warnx("invalid type \"%s\"; "
				    "must be one of \"ipv4\" or \"ipv6\"",
				    argv[start_arg + 1]);
				return (EINVAL);
			}

			if (af != AF_UNSPEC && af != newaf) {
				warnx("conflicting IPv4/IPv6 specifications.");
				return (EINVAL);
			}
			af = newaf;
		} else if (!parse_val_mask("fcoe", args, &val, &mask)) {
			t.fs.val.fcoe = val;
			t.fs.mask.fcoe = mask;
		} else if (!parse_val_mask("iport", args, &val, &mask)) {
			t.fs.val.iport = val;
			t.fs.mask.iport = mask;
		} else if (!parse_val_mask("ovlan", args, &val, &mask)) {
			t.fs.val.vnic = val;
			t.fs.mask.vnic = mask;
			t.fs.val.vnic_vld = 1;
			t.fs.mask.vnic_vld = 1;
		} else if (!parse_val_mask("vnic", args, &val, &mask)) {
			t.fs.val.vnic = val;
			t.fs.mask.vnic = mask;
			t.fs.val.vnic_vld = 1;
			t.fs.mask.vnic_vld = 1;
		} else if (!parse_val_mask("vlan", args, &val, &mask)) {
			t.fs.val.vlan = val;
			t.fs.mask.vlan = mask;
			t.fs.val.vlan_vld = 1;
			t.fs.mask.vlan_vld = 1;
		} else if (!parse_val_mask("tos", args, &val, &mask)) {
			t.fs.val.tos = val;
			t.fs.mask.tos = mask;
		} else if (!parse_val_mask("proto", args, &val, &mask)) {
			t.fs.val.proto = val;
			t.fs.mask.proto = mask;
		} else if (!parse_val_mask("ethtype", args, &val, &mask)) {
			t.fs.val.ethtype = val;
			t.fs.mask.ethtype = mask;
		} else if (!parse_val_mask("macidx", args, &val, &mask)) {
			t.fs.val.macidx = val;
			t.fs.mask.macidx = mask;
		} else if (!parse_val_mask("matchtype", args, &val, &mask)) {
			t.fs.val.matchtype = val;
			t.fs.mask.matchtype = mask;
		} else if (!parse_val_mask("frag", args, &val, &mask)) {
			t.fs.val.frag = val;
			t.fs.mask.frag = mask;
		} else if (!parse_val_mask("dport", args, &val, &mask)) {
			t.fs.val.dport = val;
			t.fs.mask.dport = mask;
		} else if (!parse_val_mask("sport", args, &val, &mask)) {
			t.fs.val.sport = val;
			t.fs.mask.sport = mask;
		} else if (!parse_ipaddr("dip", args, &af, t.fs.val.dip,
		    t.fs.mask.dip)) {
			/* nada */;
		} else if (!parse_ipaddr("sip", args, &af, t.fs.val.sip,
		    t.fs.mask.sip)) {
			/* nada */;
		} else if (!strcmp(argv[start_arg], "action")) {
			if (!strcmp(argv[start_arg + 1], "pass"))
				t.fs.action = FILTER_PASS;
			else if (!strcmp(argv[start_arg + 1], "drop"))
				t.fs.action = FILTER_DROP;
			else if (!strcmp(argv[start_arg + 1], "switch"))
				t.fs.action = FILTER_SWITCH;
			else {
				warnx("invalid action \"%s\"; must be one of"
				     " \"pass\", \"drop\" or \"switch\"",
				     argv[start_arg + 1]);
				return (EINVAL);
			}
		} else if (!parse_val("hitcnts", args, &val)) {
			t.fs.hitcnts = val;
		} else if (!parse_val("prio", args, &val)) {
			t.fs.prio = val;
		} else if (!parse_val("rpttid", args, &val)) {
			t.fs.rpttid = 1;
		} else if (!parse_val("queue", args, &val)) {
			t.fs.dirsteer = 1;
			t.fs.iq = val;
		} else if (!parse_val("tcbhash", args, &val)) {
			t.fs.maskhash = 1;
			t.fs.dirsteerhash = 1;
		} else if (!parse_val("eport", args, &val)) {
			t.fs.eport = val;
		} else if (!strcmp(argv[start_arg], "dmac")) {
			struct ether_addr *daddr;

			daddr = ether_aton(argv[start_arg + 1]);
			if (daddr == NULL) {
				warnx("invalid dmac address \"%s\"",
				    argv[start_arg + 1]);
				return (EINVAL);
			}
			memcpy(t.fs.dmac, daddr, ETHER_ADDR_LEN);
			t.fs.newdmac = 1;
		} else if (!strcmp(argv[start_arg], "smac")) {
			struct ether_addr *saddr;

			saddr = ether_aton(argv[start_arg + 1]);
			if (saddr == NULL) {
				warnx("invalid smac address \"%s\"",
				    argv[start_arg + 1]);
				return (EINVAL);
			}
			memcpy(t.fs.smac, saddr, ETHER_ADDR_LEN);
			t.fs.newsmac = 1;
		} else if (!strcmp(argv[start_arg], "vlan")) {
			char *p;
			if (!strcmp(argv[start_arg + 1], "none")) {
				t.fs.newvlan = VLAN_REMOVE;
			} else if (argv[start_arg + 1][0] == '=') {
				t.fs.newvlan = VLAN_REWRITE;
			} else if (argv[start_arg + 1][0] == '+') {
				t.fs.newvlan = VLAN_INSERT;
			} else {
				warnx("unknown vlan parameter \"%s\"; must"
				     " be one of \"none\", \"=<vlan>\" or"
				     " \"+<vlan>\"", argv[start_arg + 1]);
				return (EINVAL);
			}
			if (t.fs.newvlan == VLAN_REWRITE ||
			    t.fs.newvlan == VLAN_INSERT) {
				t.fs.vlan = strtoul(argv[start_arg + 1] + 1,
				    &p, 0);
				if (p == argv[start_arg + 1] + 1 || p[0] != 0) {
					warnx("invalid vlan \"%s\"",
					     argv[start_arg + 1]);
					return (EINVAL);
				}
			}
		} else {
			warnx("invalid parameter \"%s\"", argv[start_arg]);
			return (EINVAL);
		}
	}
	if (start_arg != argc) {
		warnx("no value for \"%s\"", argv[start_arg]);
		return (EINVAL);
	}

	/*
	 * Check basic sanity of option combinations.
	 */
	if (t.fs.action != FILTER_SWITCH &&
	    (t.fs.eport || t.fs.newdmac || t.fs.newsmac || t.fs.newvlan)) {
		warnx("prio, port dmac, smac and vlan only make sense with"
		     " \"action switch\"");
		return (EINVAL);
	}
	if (t.fs.action != FILTER_PASS &&
	    (t.fs.rpttid || t.fs.dirsteer || t.fs.maskhash)) {
		warnx("rpttid, queue and tcbhash don't make sense with"
		     " action \"drop\" or \"switch\"");
		return (EINVAL);
	}

	t.fs.type = (af == AF_INET6 ? 1 : 0); /* default IPv4 */
	return doit(CHELSIO_T4_SET_FILTER, &t);
}

static int
filter_cmd(int argc, const char *argv[])
{
	long long val;
	uint32_t idx;
	char *s;

	if (argc == 0) {
		warnx("filter: no arguments.");
		return (EINVAL);
	};

	/* list */
	if (strcmp(argv[0], "list") == 0) {
		if (argc != 1)
			warnx("trailing arguments after \"list\" ignored.");

		return show_filters();
	}

	/* mode */
	if (argc == 1 && strcmp(argv[0], "mode") == 0)
		return get_filter_mode();

	/* mode <mode> */
	if (strcmp(argv[0], "mode") == 0)
		return set_filter_mode(argc - 1, argv + 1);

	/* <idx> ... */
	s = str_to_number(argv[0], NULL, &val);
	if (*s || val > 0xffffffffU) {
		warnx("\"%s\" is neither an index nor a filter subcommand.",
		    argv[0]);
		return (EINVAL);
	}
	idx = (uint32_t) val;

	/* <idx> delete|clear */
	if (argc == 2 &&
	    (strcmp(argv[1], "delete") == 0 || strcmp(argv[1], "clear") == 0)) {
		return del_filter(idx);
	}

	/* <idx> [<param> <val>] ... */
	return set_filter(idx, argc - 1, argv + 1);
}

/*
 * Shows the fields of a multi-word structure.  The structure is considered to
 * consist of @nwords 32-bit words (i.e, it's an (@nwords * 32)-bit structure)
 * whose fields are described by @fd.  The 32-bit words are given in @words
 * starting with the least significant 32-bit word.
 */
static void
show_struct(const uint32_t *words, int nwords, const struct field_desc *fd)
{
	unsigned int w = 0;
	const struct field_desc *p;

	for (p = fd; p->name; p++)
		w = max(w, strlen(p->name));

	while (fd->name) {
		unsigned long long data;
		int first_word = fd->start / 32;
		int shift = fd->start % 32;
		int width = fd->end - fd->start + 1;
		unsigned long long mask = (1ULL << width) - 1;

		data = (words[first_word] >> shift) |
		       ((uint64_t)words[first_word + 1] << (32 - shift));
		if (shift)
		       data |= ((uint64_t)words[first_word + 2] << (64 - shift));
		data &= mask;
		if (fd->islog2)
			data = 1 << data;
		printf("%-*s ", w, fd->name);
		printf(fd->hex ? "%#llx\n" : "%llu\n", data << fd->shift);
		fd++;
	}
}

#define FIELD(name, start, end) { name, start, end, 0, 0, 0 }
#define FIELD1(name, start) FIELD(name, start, start)

static void
show_sge_context(const struct t4_sge_context *p)
{
	static struct field_desc egress[] = {
		FIELD1("StatusPgNS:", 180),
		FIELD1("StatusPgRO:", 179),
		FIELD1("FetchNS:", 178),
		FIELD1("FetchRO:", 177),
		FIELD1("Valid:", 176),
		FIELD("PCIeDataChannel:", 174, 175),
		FIELD1("DCAEgrQEn:", 173),
		FIELD("DCACPUID:", 168, 172),
		FIELD1("FCThreshOverride:", 167),
		FIELD("WRLength:", 162, 166),
		FIELD1("WRLengthKnown:", 161),
		FIELD1("ReschedulePending:", 160),
		FIELD1("OnChipQueue:", 159),
		FIELD1("FetchSizeMode", 158),
		{ "FetchBurstMin:", 156, 157, 4, 0, 1 },
		{ "FetchBurstMax:", 153, 154, 6, 0, 1 },
		FIELD("uPToken:", 133, 152),
		FIELD1("uPTokenEn:", 132),
		FIELD1("UserModeIO:", 131),
		FIELD("uPFLCredits:", 123, 130),
		FIELD1("uPFLCreditEn:", 122),
		FIELD("FID:", 111, 121),
		FIELD("HostFCMode:", 109, 110),
		FIELD1("HostFCOwner:", 108),
		{ "CIDXFlushThresh:", 105, 107, 0, 0, 1 },
		FIELD("CIDX:", 89, 104),
		FIELD("PIDX:", 73, 88),
		{ "BaseAddress:", 18, 72, 9, 1 },
		FIELD("QueueSize:", 2, 17),
		FIELD1("QueueType:", 1),
		FIELD1("CachePriority:", 0),
		{ NULL }
	};
	static struct field_desc fl[] = {
		FIELD1("StatusPgNS:", 180),
		FIELD1("StatusPgRO:", 179),
		FIELD1("FetchNS:", 178),
		FIELD1("FetchRO:", 177),
		FIELD1("Valid:", 176),
		FIELD("PCIeDataChannel:", 174, 175),
		FIELD1("DCAEgrQEn:", 173),
		FIELD("DCACPUID:", 168, 172),
		FIELD1("FCThreshOverride:", 167),
		FIELD("WRLength:", 162, 166),
		FIELD1("WRLengthKnown:", 161),
		FIELD1("ReschedulePending:", 160),
		FIELD1("OnChipQueue:", 159),
		FIELD1("FetchSizeMode", 158),
		{ "FetchBurstMin:", 156, 157, 4, 0, 1 },
		{ "FetchBurstMax:", 153, 154, 6, 0, 1 },
		FIELD1("FLMcongMode:", 152),
		FIELD("MaxuPFLCredits:", 144, 151),
		FIELD("FLMcontextID:", 133, 143),
		FIELD1("uPTokenEn:", 132),
		FIELD1("UserModeIO:", 131),
		FIELD("uPFLCredits:", 123, 130),
		FIELD1("uPFLCreditEn:", 122),
		FIELD("FID:", 111, 121),
		FIELD("HostFCMode:", 109, 110),
		FIELD1("HostFCOwner:", 108),
		{ "CIDXFlushThresh:", 105, 107, 0, 0, 1 },
		FIELD("CIDX:", 89, 104),
		FIELD("PIDX:", 73, 88),
		{ "BaseAddress:", 18, 72, 9, 1 },
		FIELD("QueueSize:", 2, 17),
		FIELD1("QueueType:", 1),
		FIELD1("CachePriority:", 0),
		{ NULL }
	};
	static struct field_desc ingress[] = {
		FIELD1("NoSnoop:", 145),
		FIELD1("RelaxedOrdering:", 144),
		FIELD1("GTSmode:", 143),
		FIELD1("ISCSICoalescing:", 142),
		FIELD1("Valid:", 141),
		FIELD1("TimerPending:", 140),
		FIELD1("DropRSS:", 139),
		FIELD("PCIeChannel:", 137, 138),
		FIELD1("SEInterruptArmed:", 136),
		FIELD1("CongestionMgtEnable:", 135),
		FIELD1("DCAIngQEnable:", 134),
		FIELD("DCACPUID:", 129, 133),
		FIELD1("UpdateScheduling:", 128),
		FIELD("UpdateDelivery:", 126, 127),
		FIELD1("InterruptSent:", 125),
		FIELD("InterruptIDX:", 114, 124),
		FIELD1("InterruptDestination:", 113),
		FIELD1("InterruptArmed:", 112),
		FIELD("RxIntCounter:", 106, 111),
		FIELD("RxIntCounterThreshold:", 104, 105),
		FIELD1("Generation:", 103),
		{ "BaseAddress:", 48, 102, 9, 1 },
		FIELD("PIDX:", 32, 47),
		FIELD("CIDX:", 16, 31),
		{ "QueueSize:", 4, 15, 4, 0 },
		{ "QueueEntrySize:", 2, 3, 4, 0, 1 },
		FIELD1("QueueEntryOverride:", 1),
		FIELD1("CachePriority:", 0),
		{ NULL }
	};
	static struct field_desc flm[] = {
		FIELD1("NoSnoop:", 79),
		FIELD1("RelaxedOrdering:", 78),
		FIELD1("Valid:", 77),
		FIELD("DCACPUID:", 72, 76),
		FIELD1("DCAFLEn:", 71),
		FIELD("EQid:", 54, 70),
		FIELD("SplitEn:", 52, 53),
		FIELD1("PadEn:", 51),
		FIELD1("PackEn:", 50),
		FIELD1("DBpriority:", 48),
		FIELD("PackOffset:", 16, 47),
		FIELD("CIDX:", 8, 15),
		FIELD("PIDX:", 0, 7),
		{ NULL }
	};
	static struct field_desc conm[] = {
		FIELD1("CngDBPHdr:", 6),
		FIELD1("CngDBPData:", 5),
		FIELD1("CngIMSG:", 4),
		FIELD("CngChMap:", 0, 3),
		{ NULL }
	};

	if (p->mem_id == SGE_CONTEXT_EGRESS)
		show_struct(p->data, 6, (p->data[0] & 2) ? fl : egress);
	else if (p->mem_id == SGE_CONTEXT_FLM)
		show_struct(p->data, 3, flm);
	else if (p->mem_id == SGE_CONTEXT_INGRESS)
		show_struct(p->data, 5, ingress);
	else if (p->mem_id == SGE_CONTEXT_CNM)
		show_struct(p->data, 1, conm);
}

#undef FIELD
#undef FIELD1

static int
get_sge_context(int argc, const char *argv[])
{
	int rc;
	char *p;
	long cid;
	struct t4_sge_context cntxt = {0};

	if (argc != 2) {
		warnx("sge_context: incorrect number of arguments.");
		return (EINVAL);
	}

	if (!strcmp(argv[0], "egress"))
		cntxt.mem_id = SGE_CONTEXT_EGRESS;
	else if (!strcmp(argv[0], "ingress"))
		cntxt.mem_id = SGE_CONTEXT_INGRESS;
	else if (!strcmp(argv[0], "fl"))
		cntxt.mem_id = SGE_CONTEXT_FLM;
	else if (!strcmp(argv[0], "cong"))
		cntxt.mem_id = SGE_CONTEXT_CNM;
	else {
		warnx("unknown context type \"%s\"; known types are egress, "
		    "ingress, fl, and cong.", argv[0]);
		return (EINVAL);
	}

	p = str_to_number(argv[1], &cid, NULL);
	if (*p) {
		warnx("invalid context id \"%s\"", argv[1]);
		return (EINVAL);
	}
	cntxt.cid = cid;

	rc = doit(CHELSIO_T4_GET_SGE_CONTEXT, &cntxt);
	if (rc != 0)
		return (rc);

	show_sge_context(&cntxt);
	return (0);
}

static int
loadfw(int argc, const char *argv[])
{
	int rc, fd;
	struct t4_data data = {0};
	const char *fname = argv[0];
	struct stat st = {0};

	if (argc != 1) {
		warnx("loadfw: incorrect number of arguments.");
		return (EINVAL);
	}

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		warn("open(%s)", fname);
		return (errno);
	}

	if (fstat(fd, &st) < 0) {
		warn("fstat");
		close(fd);
		return (errno);
	}

	data.len = st.st_size;
	data.data = mmap(0, data.len, PROT_READ, 0, fd, 0);
	if (data.data == MAP_FAILED) {
		warn("mmap");
		close(fd);
		return (errno);
	}

	rc = doit(CHELSIO_T4_LOAD_FW, &data);
	munmap(data.data, data.len);
	close(fd);
	return (rc);
}

static int
read_mem(uint32_t addr, uint32_t len, void (*output)(uint32_t *, uint32_t))
{
	int rc;
	struct t4_mem_range mr;

	mr.addr = addr;
	mr.len = len;
	mr.data = malloc(mr.len);

	if (mr.data == 0) {
		warn("read_mem: malloc");
		return (errno);
	}

	rc = doit(CHELSIO_T4_GET_MEM, &mr);
	if (rc != 0)
		goto done;

	if (output)
		(*output)(mr.data, mr.len);
done:
	free(mr.data);
	return (rc);
}

/*
 * Display memory as list of 'n' 4-byte values per line.
 */
static void
show_mem(uint32_t *buf, uint32_t len)
{
	const char *s;
	int i, n = 8;

	while (len) {
		for (i = 0; len && i < n; i++, buf++, len -= 4) {
			s = i ? " " : "";
			printf("%s%08x", s, htonl(*buf));
		}
		printf("\n");
	}
}

static int
memdump(int argc, const char *argv[])
{
	char *p;
	long l;
	uint32_t addr, len;

	if (argc != 2) {
		warnx("incorrect number of arguments.");
		return (EINVAL);
	}

	p = str_to_number(argv[0], &l, NULL);
	if (*p) {
		warnx("invalid address \"%s\"", argv[0]);
		return (EINVAL);
	}
	addr = l;

	p = str_to_number(argv[1], &l, NULL);
	if (*p) {
		warnx("memdump: invalid length \"%s\"", argv[1]);
		return (EINVAL);
	}
	len = l;

	return (read_mem(addr, len, show_mem));
}

/*
 * Display TCB as list of 'n' 4-byte values per line.
 */
static void
show_tcb(uint32_t *buf, uint32_t len)
{
	const char *s;
	int i, n = 8;

	while (len) {
		for (i = 0; len && i < n; i++, buf++, len -= 4) {
			s = i ? " " : "";
			printf("%s%08x", s, htonl(*buf));
		}
		printf("\n");
	}
}

#define A_TP_CMM_TCB_BASE 0x7d10
#define TCB_SIZE 128
static int
read_tcb(int argc, const char *argv[])
{
	char *p;
	long l;
	long long val;
	unsigned int tid;
	uint32_t addr;
	int rc;

	if (argc != 1) {
		warnx("incorrect number of arguments.");
		return (EINVAL);
	}

	p = str_to_number(argv[0], &l, NULL);
	if (*p) {
		warnx("invalid tid \"%s\"", argv[0]);
		return (EINVAL);
	}
	tid = l;

	rc = read_reg(A_TP_CMM_TCB_BASE, 4, &val);
	if (rc != 0)
		return (rc);

	addr = val + tid * TCB_SIZE;

	return (read_mem(addr, TCB_SIZE, show_tcb));
}

static int
run_cmd(int argc, const char *argv[])
{
	int rc = -1;
	const char *cmd = argv[0];

	/* command */
	argc--;
	argv++;

	if (!strcmp(cmd, "reg") || !strcmp(cmd, "reg32"))
		rc = register_io(argc, argv, 4);
	else if (!strcmp(cmd, "reg64"))
		rc = register_io(argc, argv, 8);
	else if (!strcmp(cmd, "regdump"))
		rc = dump_regs(argc, argv);
	else if (!strcmp(cmd, "filter"))
		rc = filter_cmd(argc, argv);
	else if (!strcmp(cmd, "context"))
		rc = get_sge_context(argc, argv);
	else if (!strcmp(cmd, "loadfw"))
		rc = loadfw(argc, argv);
	else if (!strcmp(cmd, "memdump"))
		rc = memdump(argc, argv);
	else if (!strcmp(cmd, "tcb"))
		rc = read_tcb(argc, argv);
	else {
		rc = EINVAL;
		warnx("invalid command \"%s\"", cmd);
	}

	return (rc);
}

#define MAX_ARGS 15
static int
run_cmd_loop(void)
{
	int i, rc = 0;
	char buffer[128], *buf;
	const char *args[MAX_ARGS + 1];

	/*
	 * Simple loop: displays a "> " prompt and processes any input as a
	 * cxgbetool command.  You're supposed to enter only the part after
	 * "cxgbetool t4nexX".  Use "quit" or "exit" to exit.
	 */
	for (;;) {
		fprintf(stdout, "> ");
		fflush(stdout);
		buf = fgets(buffer, sizeof(buffer), stdin);
		if (buf == NULL) {
			if (ferror(stdin)) {
				warn("stdin error");
				rc = errno;	/* errno from fgets */
			}
			break;
		}

		i = 0;
		while ((args[i] = strsep(&buf, " \t\n")) != NULL) {
			if (args[i][0] != 0 && ++i == MAX_ARGS)
				break;
		}
		args[i] = 0;

		if (i == 0)
			continue;	/* skip empty line */

		if (!strcmp(args[0], "quit") || !strcmp(args[0], "exit"))
			break;

		rc = run_cmd(i, args);
	}

	/* rc normally comes from the last command (not including quit/exit) */
	return (rc);
}

int
main(int argc, const char *argv[])
{
	int rc = -1;

	progname = argv[0];

	if (argc == 2) {
		if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
			usage(stdout);
			exit(0);
		}
	}

	if (argc < 3) {
		usage(stderr);
		exit(EINVAL);
	}

	nexus = argv[1];

	/* progname and nexus */
	argc -= 2;
	argv += 2;

	if (argc == 1 && !strcmp(argv[0], "stdio"))
		rc = run_cmd_loop();
	else
		rc = run_cmd(argc, argv);

	return (rc);
}
