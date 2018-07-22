/* $MidnightBSD$ */
/*
 * Copyright (C) 2013-2014 Michio Honda. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
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

/* $FreeBSD: stable/10/tools/tools/netmap/vale-ctl.c 270252 2014-08-20 23:34:36Z luigi $ */

#include <errno.h>
#include <stdio.h>
#include <inttypes.h>	/* PRI* macros */
#include <string.h>	/* strcmp */
#include <fcntl.h>	/* open */
#include <unistd.h>	/* close */
#include <sys/ioctl.h>	/* ioctl */
#include <sys/param.h>
#include <sys/socket.h>	/* apple needs sockaddr */
#include <net/if.h>	/* ifreq */
#include <net/netmap.h>
#include <net/netmap_user.h>
#include <libgen.h>	/* basename */
#include <stdlib.h>	/* atoi, free */

/* debug support */
#define ND(format, ...)	do {} while(0)
#define D(format, ...)					\
	fprintf(stderr, "%s [%d] " format "\n",		\
	__FUNCTION__, __LINE__, ##__VA_ARGS__)

/* XXX cut and paste from pkt-gen.c because I'm not sure whether this
 * program may include nm_util.h
 */
void parse_nmr_config(const char* conf, struct nmreq *nmr)
{
	char *w, *tok;
	int i, v;

	nmr->nr_tx_rings = nmr->nr_rx_rings = 0;
	nmr->nr_tx_slots = nmr->nr_rx_slots = 0;
	if (conf == NULL || ! *conf)
		return;
	w = strdup(conf);
	for (i = 0, tok = strtok(w, ","); tok; i++, tok = strtok(NULL, ",")) {
		v = atoi(tok);
		switch (i) {
		case 0:
			nmr->nr_tx_slots = nmr->nr_rx_slots = v;
			break;
		case 1:
			nmr->nr_rx_slots = v;
			break;
		case 2:
			nmr->nr_tx_rings = nmr->nr_rx_rings = v;
			break;
		case 3:
			nmr->nr_rx_rings = v;
			break;
		default:
			D("ignored config: %s", tok);
			break;
		}
	}
	D("txr %d txd %d rxr %d rxd %d",
			nmr->nr_tx_rings, nmr->nr_tx_slots,
			nmr->nr_rx_rings, nmr->nr_rx_slots);
	free(w);
}

static int
bdg_ctl(const char *name, int nr_cmd, int nr_arg, char *nmr_config)
{
	struct nmreq nmr;
	int error = 0;
	int fd = open("/dev/netmap", O_RDWR);

	if (fd == -1) {
		D("Unable to open /dev/netmap");
		return -1;
	}

	bzero(&nmr, sizeof(nmr));
	nmr.nr_version = NETMAP_API;
	if (name != NULL) /* might be NULL */
		strncpy(nmr.nr_name, name, sizeof(nmr.nr_name));
	nmr.nr_cmd = nr_cmd;
	parse_nmr_config(nmr_config, &nmr);

	switch (nr_cmd) {
	case NETMAP_BDG_DELIF:
	case NETMAP_BDG_NEWIF:
		error = ioctl(fd, NIOCREGIF, &nmr);
		if (error == -1) {
			ND("Unable to %s %s", nr_cmd == NETMAP_BDG_DELIF ? "delete":"create", name);
			perror(name);
		} else {
			ND("Success to %s %s", nr_cmd == NETMAP_BDG_DELIF ? "delete":"create", name);
		}
		break;
	case NETMAP_BDG_ATTACH:
	case NETMAP_BDG_DETACH:
		if (nr_arg && nr_arg != NETMAP_BDG_HOST)
			nr_arg = 0;
		nmr.nr_arg1 = nr_arg;
		error = ioctl(fd, NIOCREGIF, &nmr);
		if (error == -1) {
			ND("Unable to %s %s to the bridge", nr_cmd ==
			    NETMAP_BDG_DETACH?"detach":"attach", name);
			perror(name);
		} else
			ND("Success to %s %s to the bridge", nr_cmd ==
			    NETMAP_BDG_DETACH?"detach":"attach", name);
		break;

	case NETMAP_BDG_LIST:
		if (strlen(nmr.nr_name)) { /* name to bridge/port info */
			error = ioctl(fd, NIOCGINFO, &nmr);
			if (error) {
				ND("Unable to obtain info for %s", name);
				perror(name);
			} else
				D("%s at bridge:%d port:%d", name, nmr.nr_arg1,
				    nmr.nr_arg2);
			break;
		}

		/* scan all the bridges and ports */
		nmr.nr_arg1 = nmr.nr_arg2 = 0;
		for (; !ioctl(fd, NIOCGINFO, &nmr); nmr.nr_arg2++) {
			D("bridge:%d port:%d %s", nmr.nr_arg1, nmr.nr_arg2,
			    nmr.nr_name);
			nmr.nr_name[0] = '\0';
		}

		break;

	default: /* GINFO */
		nmr.nr_cmd = nmr.nr_arg1 = nmr.nr_arg2 = 0;
		error = ioctl(fd, NIOCGINFO, &nmr);
		if (error) {
			ND("Unable to get if info for %s", name);
			perror(name);
		} else
			D("%s: %d queues.", name, nmr.nr_rx_rings);
		break;
	}
	close(fd);
	return error;
}

int
main(int argc, char *argv[])
{
	int ch, nr_cmd = 0, nr_arg = 0;
	const char *command = basename(argv[0]);
	char *name = NULL, *nmr_config = NULL;

	if (argc > 3) {
usage:
		fprintf(stderr,
			"Usage:\n"
			"%s arguments\n"
			"\t-g interface	interface name to get info\n"
			"\t-d interface	interface name to be detached\n"
			"\t-a interface	interface name to be attached\n"
			"\t-h interface	interface name to be attached with the host stack\n"
			"\t-n interface	interface name to be created\n"
			"\t-r interface	interface name to be deleted\n"
			"\t-l list all or specified bridge's interfaces (default)\n"
			"\t-C string ring/slot setting of an interface creating by -n\n"
			"", command);
		return 0;
	}

	while ((ch = getopt(argc, argv, "d:a:h:g:l:n:r:C:")) != -1) {
		name = optarg; /* default */
		switch (ch) {
		default:
			fprintf(stderr, "bad option %c %s", ch, optarg);
			goto usage;
		case 'd':
			nr_cmd = NETMAP_BDG_DETACH;
			break;
		case 'a':
			nr_cmd = NETMAP_BDG_ATTACH;
			break;
		case 'h':
			nr_cmd = NETMAP_BDG_ATTACH;
			nr_arg = NETMAP_BDG_HOST;
			break;
		case 'n':
			nr_cmd = NETMAP_BDG_NEWIF;
			break;
		case 'r':
			nr_cmd = NETMAP_BDG_DELIF;
			break;
		case 'g':
			nr_cmd = 0;
			break;
		case 'l':
			nr_cmd = NETMAP_BDG_LIST;
			if (optind < argc && argv[optind][0] == '-')
				name = NULL;
			break;
		case 'C':
			nmr_config = strdup(optarg);
			break;
		}
		if (optind != argc) {
			// fprintf(stderr, "optind %d argc %d\n", optind, argc);
			goto usage;
		}
	}
	if (argc == 1)
		nr_cmd = NETMAP_BDG_LIST;
	return bdg_ctl(name, nr_cmd, nr_arg, nmr_config) ? 1 : 0;
}
