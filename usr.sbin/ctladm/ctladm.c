/*-
 * Copyright (c) 2003, 2004 Silicon Graphics International Corp.
 * Copyright (c) 1997-2007 Kenneth D. Merry
 * Copyright (c) 2012 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Edward Tomasz Napierala
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/users/kenm/FreeBSD-test2/usr.sbin/ctladm/ctladm.c#4 $
 */
/*
 * CAM Target Layer exercise program.
 *
 * Author: Ken Merry <ken@FreeBSD.org>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/callout.h>
#include <sys/sbuf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <ctype.h>
#include <bsdxml.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl_frontend_internal.h>
#include <cam/ctl/ctl_backend.h>
#include <cam/ctl/ctl_ioctl.h>
#include <cam/ctl/ctl_backend_block.h>
#include <cam/ctl/ctl_util.h>
#include <cam/ctl/ctl_scsi_all.h>
#include <camlib.h>
#include <libutil.h>
#include "ctladm.h"

#ifdef min
#undef min
#endif
#define min(x,y) (x < y) ? x : y

typedef enum {
	CTLADM_CMD_TUR,
	CTLADM_CMD_INQUIRY,
	CTLADM_CMD_REQ_SENSE,
	CTLADM_CMD_ARRAYLIST,
	CTLADM_CMD_REPORT_LUNS,
	CTLADM_CMD_HELP,
	CTLADM_CMD_DEVLIST,
	CTLADM_CMD_ADDDEV,
	CTLADM_CMD_RM,
	CTLADM_CMD_CREATE,
	CTLADM_CMD_READ,
	CTLADM_CMD_WRITE,
	CTLADM_CMD_PORT,
	CTLADM_CMD_READCAPACITY,
	CTLADM_CMD_MODESENSE,
	CTLADM_CMD_DUMPOOA,
	CTLADM_CMD_DUMPSTRUCTS,
	CTLADM_CMD_START,
	CTLADM_CMD_STOP,
	CTLADM_CMD_SYNC_CACHE,
	CTLADM_CMD_SHUTDOWN,
	CTLADM_CMD_STARTUP,
	CTLADM_CMD_LUNLIST,
	CTLADM_CMD_HARDSTOP,
	CTLADM_CMD_HARDSTART,
	CTLADM_CMD_DELAY,
	CTLADM_CMD_REALSYNC,
	CTLADM_CMD_SETSYNC,
	CTLADM_CMD_GETSYNC,
	CTLADM_CMD_ERR_INJECT,
	CTLADM_CMD_BBRREAD,
	CTLADM_CMD_PRES_IN,
	CTLADM_CMD_PRES_OUT,
	CTLADM_CMD_INQ_VPD_DEVID,
	CTLADM_CMD_RTPG,
	CTLADM_CMD_MODIFY
} ctladm_cmdfunction;

typedef enum {
	CTLADM_ARG_NONE		= 0x0000000,
	CTLADM_ARG_AUTOSENSE	= 0x0000001,
	CTLADM_ARG_DEVICE	= 0x0000002,
	CTLADM_ARG_ARRAYSIZE	= 0x0000004,
	CTLADM_ARG_BACKEND	= 0x0000008,
	CTLADM_ARG_CDBSIZE	= 0x0000010,
	CTLADM_ARG_DATALEN	= 0x0000020,
	CTLADM_ARG_FILENAME	= 0x0000040,
	CTLADM_ARG_LBA		= 0x0000080,
	CTLADM_ARG_PC		= 0x0000100,
	CTLADM_ARG_PAGE_CODE	= 0x0000200,
	CTLADM_ARG_PAGE_LIST	= 0x0000400,
	CTLADM_ARG_SUBPAGE	= 0x0000800,
	CTLADM_ARG_PAGELIST	= 0x0001000,
	CTLADM_ARG_DBD		= 0x0002000,
	CTLADM_ARG_TARG_LUN	= 0x0004000,
	CTLADM_ARG_BLOCKSIZE	= 0x0008000,
	CTLADM_ARG_IMMED	= 0x0010000,
	CTLADM_ARG_RELADR	= 0x0020000,
	CTLADM_ARG_RETRIES	= 0x0040000,
	CTLADM_ARG_ONOFFLINE	= 0x0080000,
	CTLADM_ARG_ONESHOT	= 0x0100000,
	CTLADM_ARG_TIMEOUT	= 0x0200000,
	CTLADM_ARG_INITIATOR 	= 0x0400000,
	CTLADM_ARG_NOCOPY	= 0x0800000,
	CTLADM_ARG_NEED_TL	= 0x1000000
} ctladm_cmdargs;

struct ctladm_opts {
	const char	*optname;
	uint32_t	cmdnum;
	ctladm_cmdargs	argnum;
	const char	*subopt;
};

typedef enum {
	CC_OR_NOT_FOUND,
	CC_OR_AMBIGUOUS,
	CC_OR_FOUND
} ctladm_optret;

static const char rw_opts[] = "Nb:c:d:f:l:";
static const char startstop_opts[] = "io";

struct ctladm_opts option_table[] = {
	{"adddev", CTLADM_CMD_ADDDEV, CTLADM_ARG_NONE, NULL},
	{"bbrread", CTLADM_CMD_BBRREAD, CTLADM_ARG_NEED_TL, "d:l:"},
	{"create", CTLADM_CMD_CREATE, CTLADM_ARG_NONE, "b:B:d:l:o:s:S:t:"},
	{"delay", CTLADM_CMD_DELAY, CTLADM_ARG_NEED_TL, "T:l:t:"},
	{"devid", CTLADM_CMD_INQ_VPD_DEVID, CTLADM_ARG_NEED_TL, NULL},
	{"devlist", CTLADM_CMD_DEVLIST, CTLADM_ARG_NONE, "b:vx"},
	{"dumpooa", CTLADM_CMD_DUMPOOA, CTLADM_ARG_NONE, NULL},
	{"dumpstructs", CTLADM_CMD_DUMPSTRUCTS, CTLADM_ARG_NONE, NULL},
	{"getsync", CTLADM_CMD_GETSYNC, CTLADM_ARG_NEED_TL, NULL},
	{"hardstart", CTLADM_CMD_HARDSTART, CTLADM_ARG_NONE, NULL},
	{"hardstop", CTLADM_CMD_HARDSTOP, CTLADM_ARG_NONE, NULL},
	{"help", CTLADM_CMD_HELP, CTLADM_ARG_NONE, NULL},
	{"inject", CTLADM_CMD_ERR_INJECT, CTLADM_ARG_NEED_TL, "cd:i:p:r:s:"},
	{"inquiry", CTLADM_CMD_INQUIRY, CTLADM_ARG_NEED_TL, NULL},
	{"lunlist", CTLADM_CMD_LUNLIST, CTLADM_ARG_NONE, NULL},
	{"modesense", CTLADM_CMD_MODESENSE, CTLADM_ARG_NEED_TL, "P:S:dlm:c:"},
	{"modify", CTLADM_CMD_MODIFY, CTLADM_ARG_NONE, "b:l:s:"},
	{"port", CTLADM_CMD_PORT, CTLADM_ARG_NONE, "lo:p:qt:w:W:x"},
	{"prin", CTLADM_CMD_PRES_IN, CTLADM_ARG_NEED_TL, "a:"},
	{"prout", CTLADM_CMD_PRES_OUT, CTLADM_ARG_NEED_TL, "a:k:r:s:"},
	{"read", CTLADM_CMD_READ, CTLADM_ARG_NEED_TL, rw_opts},
	{"readcapacity", CTLADM_CMD_READCAPACITY, CTLADM_ARG_NEED_TL, "c:"},
	{"realsync", CTLADM_CMD_REALSYNC, CTLADM_ARG_NONE, NULL},
	{"remove", CTLADM_CMD_RM, CTLADM_ARG_NONE, "b:l:o:"},
	{"reportluns", CTLADM_CMD_REPORT_LUNS, CTLADM_ARG_NEED_TL, NULL},
	{"reqsense", CTLADM_CMD_REQ_SENSE, CTLADM_ARG_NEED_TL, NULL},
	{"rtpg", CTLADM_CMD_RTPG, CTLADM_ARG_NEED_TL, NULL},
	{"setsync", CTLADM_CMD_SETSYNC, CTLADM_ARG_NEED_TL, "i:"},
	{"shutdown", CTLADM_CMD_SHUTDOWN, CTLADM_ARG_NONE, NULL},
	{"start", CTLADM_CMD_START, CTLADM_ARG_NEED_TL, startstop_opts},
	{"startup", CTLADM_CMD_STARTUP, CTLADM_ARG_NONE, NULL},
	{"stop", CTLADM_CMD_STOP, CTLADM_ARG_NEED_TL, startstop_opts},
	{"synccache", CTLADM_CMD_SYNC_CACHE, CTLADM_ARG_NEED_TL, "b:c:il:r"},
	{"tur", CTLADM_CMD_TUR, CTLADM_ARG_NEED_TL, NULL},
	{"write", CTLADM_CMD_WRITE, CTLADM_ARG_NEED_TL, rw_opts},
	{"-?", CTLADM_CMD_HELP, CTLADM_ARG_NONE, NULL},
	{"-h", CTLADM_CMD_HELP, CTLADM_ARG_NONE, NULL},
	{NULL, 0, 0, NULL}
};


ctladm_optret getoption(struct ctladm_opts *table, char *arg, uint32_t *cmdnum,
			ctladm_cmdargs *argnum, const char **subopt);
static int cctl_parse_tl(char *str, int *target, int *lun);
static int cctl_dump_ooa(int fd, int argc, char **argv);
static int cctl_port_dump(int fd, int quiet, int xml, int32_t fe_num,
			  ctl_port_type port_type);
static int cctl_port(int fd, int argc, char **argv, char *combinedopt);
static int cctl_do_io(int fd, int retries, union ctl_io *io, const char *func);
static int cctl_delay(int fd, int target, int lun, int argc, char **argv,
		      char *combinedopt);
static int cctl_lunlist(int fd);
static void cctl_cfi_mt_statusstr(cfi_mt_status status, char *str, int str_len);
static void cctl_cfi_bbr_statusstr(cfi_bbrread_status, char *str, int str_len);
static int cctl_hardstopstart(int fd, ctladm_cmdfunction command);
static int cctl_bbrread(int fd, int target, int lun, int iid, int argc,
			char **argv, char *combinedopt);
static int cctl_startup_shutdown(int fd, int target, int lun, int iid,
				 ctladm_cmdfunction command);
static int cctl_sync_cache(int fd, int target, int lun, int iid, int retries,
			   int argc, char **argv, char *combinedopt);
static int cctl_start_stop(int fd, int target, int lun, int iid, int retries,
			   int start, int argc, char **argv, char *combinedopt);
static int cctl_mode_sense(int fd, int target, int lun, int iid, int retries, 
			   int argc, char **argv, char *combinedopt);
static int cctl_read_capacity(int fd, int target, int lun, int iid,
			      int retries, int argc, char **argv,
			      char *combinedopt);
static int cctl_read_write(int fd, int target, int lun, int iid, int retries,
			   int argc, char **argv, char *combinedopt,
			   ctladm_cmdfunction command);
static int cctl_get_luns(int fd, int target, int lun, int iid, int retries,
			 struct scsi_report_luns_data **lun_data,
			 uint32_t *num_luns);
static int cctl_report_luns(int fd, int target, int lun, int iid, int retries);
static int cctl_tur(int fd, int target, int lun, int iid, int retries);
static int cctl_get_inquiry(int fd, int target, int lun, int iid, int retries,
			    char *path_str, int path_len,
			    struct scsi_inquiry_data *inq_data);
static int cctl_inquiry(int fd, int target, int lun, int iid, int retries);
static int cctl_req_sense(int fd, int target, int lun, int iid, int retries);
static int cctl_persistent_reserve_in(int fd, int target, int lun,
				      int initiator, int argc, char **argv,
				      char *combinedopt, int retry_count);
static int cctl_persistent_reserve_out(int fd, int target, int lun, 
				       int initiator, int argc, char **argv,
				       char *combinedopt, int retry_count);
static int cctl_create_lun(int fd, int argc, char **argv, char *combinedopt);
static int cctl_inquiry_vpd_devid(int fd, int target, int lun, int initiator);
static int cctl_report_target_port_group(int fd, int target, int lun,
					 int initiator);
static int cctl_modify_lun(int fd, int argc, char **argv, char *combinedopt);

ctladm_optret
getoption(struct ctladm_opts *table, char *arg, uint32_t *cmdnum,
	  ctladm_cmdargs *argnum, const char **subopt)
{
	struct ctladm_opts *opts;
	int num_matches = 0;

	for (opts = table; (opts != NULL) && (opts->optname != NULL);
	     opts++) {
		if (strncmp(opts->optname, arg, strlen(arg)) == 0) {
			*cmdnum = opts->cmdnum;
			*argnum = opts->argnum;
			*subopt = opts->subopt;

			if (strcmp(opts->optname, arg) == 0)
				return (CC_OR_FOUND);

			if (++num_matches > 1)
				return(CC_OR_AMBIGUOUS);
		}
	}

	if (num_matches > 0)
		return(CC_OR_FOUND);
	else
		return(CC_OR_NOT_FOUND);
}


static int
cctl_parse_tl(char *str, int *target, int *lun)
{
	char *tmpstr;
	int retval;

	retval = 0;

	while (isspace(*str) && (*str != '\0'))
		str++;

	tmpstr = (char *)strtok(str, ":");
	if ((tmpstr != NULL) && (*tmpstr != '\0')) {
		*target = strtol(tmpstr, NULL, 0);
		tmpstr = (char *)strtok(NULL, ":");
		if ((tmpstr != NULL) && (*tmpstr != '\0')) {
			*lun = strtol(tmpstr, NULL, 0);
		} else
			retval = -1;
	} else
		retval = -1;

	return (retval);
}

static int
cctl_dump_ooa(int fd, int argc, char **argv)
{
	struct ctl_ooa ooa;
	long double cmd_latency;
	int num_entries, len;
	int target = -1, lun = -1;
	int retval;
	unsigned int i;

	num_entries = 104;

	if ((argc > 2)
	 && (isdigit(argv[2][0]))) {
		retval = cctl_parse_tl(argv[2], &target, &lun);
		if (retval != 0)
			warnx("invalid target:lun argument %s", argv[2]);
	}
retry:

	len = num_entries * sizeof(struct ctl_ooa_entry);

	bzero(&ooa, sizeof(ooa));

	ooa.entries = malloc(len);

	if (ooa.entries == NULL) {
		warn("%s: error mallocing %d bytes", __func__, len);
		return (1);
	}

	if (argc > 2) {
		ooa.lun_num = lun;
	} else
		ooa.flags |= CTL_OOA_FLAG_ALL_LUNS;

	ooa.alloc_len = len;
	ooa.alloc_num = num_entries;
	if (ioctl(fd, CTL_GET_OOA, &ooa) == -1) {
		warn("%s: CTL_GET_OOA ioctl failed", __func__);
		retval = 1;
		goto bailout;
	}

	if (ooa.status == CTL_OOA_NEED_MORE_SPACE) {
		num_entries = num_entries * 2;
		free(ooa.entries);
		ooa.entries = NULL;
		goto retry;
	}

	if (ooa.status != CTL_OOA_OK) {
		warnx("%s: CTL_GET_OOA ioctl returned error %d", __func__,
		      ooa.status);
		retval = 1;
		goto bailout;
	}

	fprintf(stdout, "Dumping OOA queues\n");
	for (i = 0; i < ooa.fill_num; i++) {
		struct ctl_ooa_entry *entry;
		char cdb_str[(SCSI_MAX_CDBLEN * 3) +1];
		struct bintime delta_bt;
		struct timespec ts;

		entry = &ooa.entries[i];

		delta_bt = ooa.cur_bt;
		bintime_sub(&delta_bt, &entry->start_bt);
		bintime2timespec(&delta_bt, &ts);
		cmd_latency = ts.tv_sec * 1000;
		if (ts.tv_nsec > 0)
			cmd_latency += ts.tv_nsec / 1000000;
		
		fprintf(stdout, "LUN %jd tag 0x%04x%s%s%s%s%s: %s. CDB: %s "
			"(%0.0Lf ms)\n",
			(intmax_t)entry->lun_num, entry->tag_num,
			(entry->cmd_flags & CTL_OOACMD_FLAG_BLOCKED) ?
			 " BLOCKED" : "",
			(entry->cmd_flags & CTL_OOACMD_FLAG_DMA) ? " DMA" : "",
			(entry->cmd_flags & CTL_OOACMD_FLAG_DMA_QUEUED) ?
			 " DMAQUEUED" : "",
			(entry->cmd_flags & CTL_OOACMD_FLAG_ABORT) ?
			 " ABORT" : "",
			(entry->cmd_flags & CTL_OOACMD_FLAG_RTR) ? " RTR" :"",
			scsi_op_desc(entry->cdb[0], NULL),
			scsi_cdb_string(entry->cdb, cdb_str, sizeof(cdb_str)),
			cmd_latency);
	}
	fprintf(stdout, "OOA queues dump done\n");
#if 0
	if (ioctl(fd, CTL_DUMP_OOA) == -1) {
		warn("%s: CTL_DUMP_OOA ioctl failed", __func__);
		return (1);
	}
#endif

bailout:
	free(ooa.entries);

	return (0);
}

static int
cctl_dump_structs(int fd, ctladm_cmdargs cmdargs __unused)
{
	if (ioctl(fd, CTL_DUMP_STRUCTS) == -1) {
		warn(__func__);
		return (1);
	}
	return (0);
}

static int
cctl_port_dump(int fd, int quiet, int xml, int32_t targ_port,
	       ctl_port_type port_type)
{
	struct ctl_port_list port_list;
	struct ctl_port_entry *entries;
	struct sbuf *sb = NULL;
	int num_entries;
	int did_print = 0;
	unsigned int i;

	num_entries = 16;

retry:

	entries = malloc(sizeof(*entries) * num_entries);
	bzero(&port_list, sizeof(port_list));
	port_list.entries = entries;
	port_list.alloc_num = num_entries;
	port_list.alloc_len = num_entries * sizeof(*entries);
	if (ioctl(fd, CTL_GET_PORT_LIST, &port_list) != 0) {
		warn("%s: CTL_GET_PORT_LIST ioctl failed", __func__);
		return (1);
	}
	if (port_list.status == CTL_PORT_LIST_NEED_MORE_SPACE) {
		printf("%s: allocated %d, need %d, retrying\n", __func__,
		       num_entries, port_list.fill_num + port_list.dropped_num);
		free(entries);
		num_entries = port_list.fill_num + port_list.dropped_num;
		goto retry;
	}

	if ((quiet == 0)
	 && (xml == 0))
		printf("Port Online Type     Name         pp vp %-18s %-18s\n",
		       "WWNN", "WWPN");

	if (xml != 0) {
		sb = sbuf_new_auto();
		sbuf_printf(sb, "<ctlfelist>\n");
	}
	for (i = 0; i < port_list.fill_num; i++) {
		struct ctl_port_entry *entry;
		const char *type;

		entry = &entries[i];

		switch (entry->port_type) {
		case CTL_PORT_FC:
			type = "FC";
			break;
		case CTL_PORT_SCSI:
			type = "SCSI";
			break;
		case CTL_PORT_IOCTL:
			type = "IOCTL";
			break;
		case CTL_PORT_INTERNAL:
			type = "INTERNAL";
			break;
		case CTL_PORT_ISC:
			type = "ISC";
			break;
		default:
			type = "UNKNOWN";
			break;
		}

		/*
		 * If the user specified a frontend number or a particular
		 * frontend type, only print out that particular frontend
		 * or frontend type.
		 */
		if ((targ_port != -1)
		 && (targ_port != entry->targ_port))
			continue;
		else if ((port_type != CTL_PORT_NONE)
		      && ((port_type & entry->port_type) == 0))
			continue;

		did_print = 1;

#if 0
		printf("Num: %ju Type: %s (%#x) Name: %s Physical Port: %d "
		       "Virtual Port: %d\n", (uintmax_t)entry->fe_num, type,
		       entry->port_type, entry->fe_name, entry->physical_port,
		       entry->virtual_port);
		printf("WWNN %#jx WWPN %#jx Online: %s\n",
		       (uintmax_t)entry->wwnn, (uintmax_t)entry->wwpn,
		       (entry->online) ? "YES" : "NO" );
#endif
		if (xml == 0) {
			printf("%-4d %-6s %-8s %-12s %-2d %-2d %#-18jx "
			       "%#-18jx\n",
			       entry->targ_port, (entry->online) ? "YES" : "NO",
			       type, entry->port_name, entry->physical_port,
			       entry->virtual_port, (uintmax_t)entry->wwnn,
			       (uintmax_t)entry->wwpn);
		} else {
			sbuf_printf(sb, "<targ_port id=\"%d\">\n",
				    entry->targ_port);
			sbuf_printf(sb, "<online>%s</online>\n",
				    (entry->online) ? "YES" : "NO");
			sbuf_printf(sb, "<port_type>%s</port_type>\n", type);
			sbuf_printf(sb, "<port_name>%s</port_name>\n",
				    entry->port_name);
			sbuf_printf(sb, "<physical_port>%d</physical_port>\n",
				    entry->physical_port);
			sbuf_printf(sb, "<virtual_port>%d</virtual_port>\n",
				    entry->virtual_port);
			sbuf_printf(sb, "<wwnn>%#jx</wwnn>\n",
				    (uintmax_t)entry->wwnn);
			sbuf_printf(sb, "<wwpn>%#jx</wwpn>\n",
				    (uintmax_t)entry->wwpn);
			sbuf_printf(sb, "</targ_port>\n");
		}

	}
	if (xml != 0) {
		sbuf_printf(sb, "</ctlfelist>\n");
		sbuf_finish(sb);
		printf("%s", sbuf_data(sb));
		sbuf_delete(sb);
	}

	/*
	 * Give some indication that we didn't find the frontend or
	 * frontend type requested by the user.  We could print something
	 * out, but it would probably be better to hide that behind a
	 * verbose flag.
	 */
	if ((did_print == 0)
	 && ((targ_port != -1)
	  || (port_type != CTL_PORT_NONE)))
		return (1);
	else
		return (0);
}

typedef enum {
	CCTL_PORT_MODE_NONE,
	CCTL_PORT_MODE_LIST,
	CCTL_PORT_MODE_SET,
	CCTL_PORT_MODE_ON,
	CCTL_PORT_MODE_OFF
} cctl_port_mode;

struct ctladm_opts cctl_fe_table[] = {
	{"fc", CTL_PORT_FC, CTLADM_ARG_NONE, NULL},
	{"scsi", CTL_PORT_SCSI, CTLADM_ARG_NONE, NULL},
	{"internal", CTL_PORT_INTERNAL, CTLADM_ARG_NONE, NULL},
	{"all", CTL_PORT_ALL, CTLADM_ARG_NONE, NULL},
	{NULL, 0, 0, NULL}
};

static int
cctl_port(int fd, int argc, char **argv, char *combinedopt)
{
	int c;
	int32_t targ_port = -1;
	int retval = 0;
	int wwnn_set = 0, wwpn_set = 0;
	uint64_t wwnn = 0, wwpn = 0;
	cctl_port_mode port_mode = CCTL_PORT_MODE_NONE;
	struct ctl_port_entry entry;
	ctl_port_type port_type = CTL_PORT_NONE;
	int quiet = 0, xml = 0;

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'l':
			if (port_mode != CCTL_PORT_MODE_NONE)
				goto bailout_badarg;

			port_mode = CCTL_PORT_MODE_LIST;
			break;
		case 'o':
			if (port_mode != CCTL_PORT_MODE_NONE)
				goto bailout_badarg;
			
			if (strcasecmp(optarg, "on") == 0)
				port_mode = CCTL_PORT_MODE_ON;
			else if (strcasecmp(optarg, "off") == 0)
				port_mode = CCTL_PORT_MODE_OFF;
			else {
				warnx("Invalid -o argument %s, \"on\" or "
				      "\"off\" are the only valid args",
				      optarg);
				retval = 1;
				goto bailout;
			}
			break;
		case 'p':
			targ_port = strtol(optarg, NULL, 0);
			break;
		case 'q':
			quiet = 1;
			break;
		case 't': {
			ctladm_optret optret;
			ctladm_cmdargs argnum;
			const char *subopt;
			ctl_port_type tmp_port_type;

			optret = getoption(cctl_fe_table, optarg, &tmp_port_type,
					   &argnum, &subopt);
			if (optret == CC_OR_AMBIGUOUS) {
				warnx("%s: ambiguous frontend type %s",
				      __func__, optarg);
				retval = 1;
				goto bailout;
			} else if (optret == CC_OR_NOT_FOUND) {
				warnx("%s: invalid frontend type %s",
				      __func__, optarg);
				retval = 1;
				goto bailout;
			}

			port_type |= tmp_port_type;
			break;
		}
		case 'w':
			if ((port_mode != CCTL_PORT_MODE_NONE)
			 && (port_mode != CCTL_PORT_MODE_SET))
				goto bailout_badarg;

			port_mode = CCTL_PORT_MODE_SET;

			wwnn = strtoull(optarg, NULL, 0);
			wwnn_set = 1;
			break;
		case 'W':
			if ((port_mode != CCTL_PORT_MODE_NONE)
			 && (port_mode != CCTL_PORT_MODE_SET))
				goto bailout_badarg;

			port_mode = CCTL_PORT_MODE_SET;

			wwpn = strtoull(optarg, NULL, 0);
			wwpn_set = 1;
			break;
		case 'x':
			xml = 1;
			break;
		}
	}

	/*
	 * The user can specify either one or more frontend types (-t), or
	 * a specific frontend, but not both.
	 *
	 * If the user didn't specify a frontend type or number, set it to
	 * all.  This is primarily needed for the enable/disable ioctls.
	 * This will be a no-op for the listing code.  For the set ioctl,
	 * we'll throw an error, since that only works on one port at a time.
	 */
	if ((port_type != CTL_PORT_NONE) && (targ_port != -1)) {
		warnx("%s: can only specify one of -t or -n", __func__);
		retval = 1;
		goto bailout;
	} else if ((targ_port == -1) && (port_type == CTL_PORT_NONE))
		port_type = CTL_PORT_ALL;

	bzero(&entry, sizeof(&entry));

	/*
	 * These are needed for all but list/dump mode.
	 */
	entry.port_type = port_type;
	entry.targ_port = targ_port;

	switch (port_mode) {
	case CCTL_PORT_MODE_LIST:
		cctl_port_dump(fd, quiet, xml, targ_port, port_type);
		break;
	case CCTL_PORT_MODE_SET:
		if (targ_port == -1) {
			warnx("%s: -w and -W require -n", __func__);
			retval = 1;
			goto bailout;
		}

		if (wwnn_set) {
			entry.flags |= CTL_PORT_WWNN_VALID;
			entry.wwnn = wwnn;
		}
		if (wwpn_set) {
			entry.flags |= CTL_PORT_WWPN_VALID;
			entry.wwpn = wwpn;
		}

		if (ioctl(fd, CTL_SET_PORT_WWNS, &entry) == -1) {
			warn("%s: CTL_SET_PORT_WWNS ioctl failed", __func__);
			retval = 1;
			goto bailout;
		}
		break;
	case CCTL_PORT_MODE_ON:
		if (ioctl(fd, CTL_ENABLE_PORT, &entry) == -1) {
			warn("%s: CTL_ENABLE_PORT ioctl failed", __func__);
			retval = 1;
			goto bailout;
		}
		fprintf(stdout, "Front End Ports enabled\n");
		break;
	case CCTL_PORT_MODE_OFF:
		if (ioctl(fd, CTL_DISABLE_PORT, &entry) == -1) {
			warn("%s: CTL_DISABLE_PORT ioctl failed", __func__);
			retval = 1;
			goto bailout;
		}
		fprintf(stdout, "Front End Ports disabled\n");
		break;
	default:
		warnx("%s: one of -l, -o or -w/-W must be specified", __func__);
		retval = 1;
		goto bailout;
		break;
	}

bailout:

	return (retval);

bailout_badarg:
	warnx("%s: only one of -l, -o or -w/-W may be specified", __func__);
	return (1);
}

static int
cctl_do_io(int fd, int retries, union ctl_io *io, const char *func)
{
	do {
		if (ioctl(fd, CTL_IO, io) == -1) {
			warn("%s: error sending CTL_IO ioctl", func);
			return (-1);
		}
	} while (((io->io_hdr.status & CTL_STATUS_MASK) != CTL_SUCCESS)
	      && (retries-- > 0));

	return (0);
}

static int
cctl_delay(int fd, int target, int lun, int argc, char **argv,
	   char *combinedopt)
{
	struct ctl_io_delay_info delay_info;
	char *delayloc = NULL;
	char *delaytype = NULL;
	int delaytime = -1;
	int retval;
	int c;

	retval = 0;

	memset(&delay_info, 0, sizeof(delay_info));

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'T':
			delaytype = strdup(optarg);
			break;
		case 'l':
			delayloc = strdup(optarg);
			break;
		case 't':
			delaytime = strtoul(optarg, NULL, 0);
			break;
		}
	}

	if (delaytime == -1) {
		warnx("%s: you must specify the delaytime with -t", __func__);
		retval = 1;
		goto bailout;
	}

	if (strcasecmp(delayloc, "datamove") == 0)
		delay_info.delay_loc = CTL_DELAY_LOC_DATAMOVE;
	else if (strcasecmp(delayloc, "done") == 0)
		delay_info.delay_loc = CTL_DELAY_LOC_DONE;
	else {
		warnx("%s: invalid delay location %s", __func__, delayloc);
		retval = 1;
		goto bailout;
	}

	if ((delaytype == NULL)
	 || (strcmp(delaytype, "oneshot") == 0))
		delay_info.delay_type = CTL_DELAY_TYPE_ONESHOT;
	else if (strcmp(delaytype, "cont") == 0)
		delay_info.delay_type = CTL_DELAY_TYPE_CONT;
	else {
		warnx("%s: invalid delay type %s", __func__, delaytype);
		retval = 1;
		goto bailout;
	}

	delay_info.target_id = target;
	delay_info.lun_id = lun;
	delay_info.delay_secs = delaytime;

	if (ioctl(fd, CTL_DELAY_IO, &delay_info) == -1) {
		warn("%s: CTL_DELAY_IO ioctl failed", __func__);
		retval = 1;
		goto bailout;
	}
	switch (delay_info.status) {
	case CTL_DELAY_STATUS_NONE:
		warnx("%s: no delay status??", __func__);
		retval = 1;
		break;
	case CTL_DELAY_STATUS_OK:
		break;
	case CTL_DELAY_STATUS_INVALID_LUN:
		warnx("%s: invalid lun %d", __func__, lun);
		retval = 1;
		break;
	case CTL_DELAY_STATUS_INVALID_TYPE:
		warnx("%s: invalid delay type %d", __func__,
		      delay_info.delay_type);
		retval = 1;
		break;
	case CTL_DELAY_STATUS_INVALID_LOC:
		warnx("%s: delay location %s not implemented?", __func__,
		      delayloc);
		retval = 1;
		break;
	case CTL_DELAY_STATUS_NOT_IMPLEMENTED:
		warnx("%s: delay not implemented in the kernel", __func__);
		warnx("%s: recompile with the CTL_IO_DELAY flag set", __func__);
		retval = 1;
		break;
	default:
		warnx("%s: unknown delay return status %d", __func__,
		      delay_info.status);
		retval = 1;
		break;
	}
bailout:

	/* delayloc should never be NULL, but just in case...*/
	if (delayloc != NULL)
		free(delayloc);

	return (retval);
}

static int
cctl_realsync(int fd, int argc, char **argv)
{
	int syncstate;
	int retval;
	char *syncarg;

	retval = 0;

	if (argc != 3) {
		warnx("%s %s takes exactly one argument", argv[0], argv[1]);
		retval = 1;
		goto bailout;
	}

	syncarg = argv[2];

	if (strncasecmp(syncarg, "query", min(strlen(syncarg),
			strlen("query"))) == 0) {
		if (ioctl(fd, CTL_REALSYNC_GET, &syncstate) == -1) {
			warn("%s: CTL_REALSYNC_GET ioctl failed", __func__);
			retval = 1;
			goto bailout;
		}
		fprintf(stdout, "SYNCHRONIZE CACHE support is: ");
		switch (syncstate) {
		case 0:
			fprintf(stdout, "OFF\n");
			break;
		case 1:
			fprintf(stdout, "ON\n");
			break;
		default:
			fprintf(stdout, "unknown (%d)\n", syncstate);
			break;
		}
		goto bailout;
	} else if (strcasecmp(syncarg, "on") == 0) {
		syncstate = 1;
	} else if (strcasecmp(syncarg, "off") == 0) {
		syncstate = 0;
	} else {
		warnx("%s: invalid realsync argument %s", __func__, syncarg);
		retval = 1;
		goto bailout;
	}

	if (ioctl(fd, CTL_REALSYNC_SET, &syncstate) == -1) {
		warn("%s: CTL_REALSYNC_SET ioctl failed", __func__);
		retval = 1;
		goto bailout;
	}
bailout:
	return (retval);
}

static int
cctl_getsetsync(int fd, int target, int lun, ctladm_cmdfunction command,
		int argc, char **argv, char *combinedopt)
{
	struct ctl_sync_info sync_info;
	uint32_t ioctl_cmd;
	int sync_interval = -1;
	int retval;
	int c;

	retval = 0;

	memset(&sync_info, 0, sizeof(sync_info));
	sync_info.target_id = target;
	sync_info.lun_id = lun;

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'i':
			sync_interval = strtoul(optarg, NULL, 0);
			break;
		default:
			break;
		}
	}

	if (command == CTLADM_CMD_SETSYNC) {
		if (sync_interval == -1) {
			warnx("%s: you must specify the sync interval with -i",
			      __func__);
			retval = 1;
			goto bailout;
		}
		sync_info.sync_interval = sync_interval;
		ioctl_cmd = CTL_SETSYNC;
	} else {
		ioctl_cmd = CTL_GETSYNC;
	}

	if (ioctl(fd, ioctl_cmd, &sync_info) == -1) {
		warn("%s: CTL_%sSYNC ioctl failed", __func__,
		     (command == CTLADM_CMD_SETSYNC) ? "SET" : "GET");
		retval = 1;
		goto bailout;
	}

	switch (sync_info.status) {
	case CTL_GS_SYNC_OK:
		if (command == CTLADM_CMD_GETSYNC) {
			fprintf(stdout, "%d:%d: sync interval: %d\n",
				target, lun, sync_info.sync_interval);
		}
		break;
	case CTL_GS_SYNC_NO_LUN:
		warnx("%s: unknown target:LUN %d:%d", __func__, target, lun);
		retval = 1;
		break;
	case CTL_GS_SYNC_NONE:
	default:
		warnx("%s: unknown CTL_%sSYNC status %d", __func__,
		      (command == CTLADM_CMD_SETSYNC) ? "SET" : "GET",
		      sync_info.status);
		retval = 1;
		break;
	}
bailout:
	return (retval);
}

struct ctladm_opts cctl_err_types[] = {
	{"aborted", CTL_LUN_INJ_ABORTED, CTLADM_ARG_NONE, NULL},
	{"mediumerr", CTL_LUN_INJ_MEDIUM_ERR, CTLADM_ARG_NONE, NULL},
	{"ua", CTL_LUN_INJ_UA, CTLADM_ARG_NONE, NULL},
	{"custom", CTL_LUN_INJ_CUSTOM, CTLADM_ARG_NONE, NULL},
	{NULL, 0, 0, NULL}

};

struct ctladm_opts cctl_err_patterns[] = {
	{"read", CTL_LUN_PAT_READ, CTLADM_ARG_NONE, NULL},
	{"write", CTL_LUN_PAT_WRITE, CTLADM_ARG_NONE, NULL},
	{"rw", CTL_LUN_PAT_READWRITE, CTLADM_ARG_NONE, NULL},
	{"readwrite", CTL_LUN_PAT_READWRITE, CTLADM_ARG_NONE, NULL},
	{"readcap", CTL_LUN_PAT_READCAP, CTLADM_ARG_NONE, NULL},
	{"tur", CTL_LUN_PAT_TUR, CTLADM_ARG_NONE, NULL},
	{"any", CTL_LUN_PAT_ANY, CTLADM_ARG_NONE, NULL},
#if 0
	{"cmd", CTL_LUN_PAT_CMD,  CTLADM_ARG_NONE, NULL},
#endif
	{NULL, 0, 0, NULL}
};

static int
cctl_error_inject(int fd, uint32_t target, uint32_t lun, int argc, char **argv, 
		  char *combinedopt)
{
	int retval;
	struct ctl_error_desc err_desc;
	uint64_t lba = 0;
	uint32_t len = 0;
	uint64_t delete_id = 0;
	int delete_id_set = 0;
	int continuous = 0;
	int sense_len = 0;
	int fd_sense = 0;
	int c;

	bzero(&err_desc, sizeof(err_desc));
	err_desc.target_id = target;
	err_desc.lun_id = lun;

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'c':
			continuous = 1;
			break;
		case 'd':
			delete_id = strtoull(optarg, NULL, 0);
			delete_id_set = 1;
			break;
		case 'i':
		case 'p': {
			ctladm_optret optret;
			ctladm_cmdargs argnum;
			const char *subopt;

			if (c == 'i') {
				ctl_lun_error err_type;

				if (err_desc.lun_error != CTL_LUN_INJ_NONE) {
					warnx("%s: can't specify multiple -i "
					      "arguments", __func__);
					retval = 1;
					goto bailout;
				}
				optret = getoption(cctl_err_types, optarg,
						   &err_type, &argnum, &subopt);
				err_desc.lun_error = err_type;
			} else {
				ctl_lun_error_pattern pattern;

				optret = getoption(cctl_err_patterns, optarg,
						   &pattern, &argnum, &subopt);
				err_desc.error_pattern |= pattern;
			}

			if (optret == CC_OR_AMBIGUOUS) {
				warnx("%s: ambiguous argument %s", __func__,
				      optarg);
				retval = 1;
				goto bailout;
			} else if (optret == CC_OR_NOT_FOUND) {
				warnx("%s: argument %s not found", __func__,
				      optarg);
				retval = 1;
				goto bailout;
			}
			break;
		}
		case 'r': {
			char *tmpstr, *tmpstr2;

			tmpstr = strdup(optarg);
			if (tmpstr == NULL) {
				warn("%s: error duplicating string %s",
				     __func__, optarg);
				retval = 1;
				goto bailout;
			}

			tmpstr2 = strsep(&tmpstr, ",");
			if (tmpstr2 == NULL) {
				warnx("%s: invalid -r argument %s", __func__,
				      optarg);
				retval = 1;
				free(tmpstr);
				goto bailout;
			}
			lba = strtoull(tmpstr2, NULL, 0);
			tmpstr2 = strsep(&tmpstr, ",");
			if (tmpstr2 == NULL) {
				warnx("%s: no len argument for -r lba,len, got"
				      " %s", __func__, optarg);
				retval = 1;
				free(tmpstr);
				goto bailout;
			}
			len = strtoul(tmpstr2, NULL, 0);
			free(tmpstr);
			break;
		}
		case 's': {
			struct get_hook hook;
			char *sensestr;

			sense_len = strtol(optarg, NULL, 0);
			if (sense_len <= 0) {
				warnx("invalid number of sense bytes %d",
				      sense_len);
				retval = 1;
				goto bailout;
			}

			sense_len = MIN(sense_len, SSD_FULL_SIZE);

			hook.argc = argc - optind;
			hook.argv = argv + optind;
			hook.got = 0;

			sensestr = cget(&hook, NULL);
			if ((sensestr != NULL)
			 && (sensestr[0] == '-')) {
				fd_sense = 1;
			} else {
				buff_encode_visit(
				    (uint8_t *)&err_desc.custom_sense,
				    sense_len, sensestr, iget, &hook);
			}
			optind += hook.got;
			break;
		}
		default:
			break;
		}
	}

	if (delete_id_set != 0) {
		err_desc.serial = delete_id;
		if (ioctl(fd, CTL_ERROR_INJECT_DELETE, &err_desc) == -1) {
			warn("%s: error issuing CTL_ERROR_INJECT_DELETE ioctl",
			     __func__);
			retval = 1;
		}
		goto bailout;
	}

	if (err_desc.lun_error == CTL_LUN_INJ_NONE) {
		warnx("%s: error injection command (-i) needed",
		      __func__);
		retval = 1;
		goto bailout;
	} else if ((err_desc.lun_error == CTL_LUN_INJ_CUSTOM)
		&& (sense_len == 0)) {
		warnx("%s: custom error requires -s", __func__);
		retval = 1;
		goto bailout;
	}

	if (continuous != 0)
		err_desc.lun_error |= CTL_LUN_INJ_CONTINUOUS;

	/*
	 * If fd_sense is set, we need to read the sense data the user
	 * wants returned from stdin.
	 */
        if (fd_sense == 1) {
		ssize_t amt_read;
		int amt_to_read = sense_len;
 		u_int8_t *buf_ptr = (uint8_t *)&err_desc.custom_sense;

		for (amt_read = 0; amt_to_read > 0;
		     amt_read = read(STDIN_FILENO, buf_ptr, amt_to_read)) {
			if (amt_read == -1) {
         			warn("error reading sense data from stdin");
				retval = 1;
				goto bailout;
			}
			amt_to_read -= amt_read;
			buf_ptr += amt_read;
		}
	}

	if (err_desc.error_pattern == CTL_LUN_PAT_NONE) {
		warnx("%s: command pattern (-p) needed", __func__);
		retval = 1;
		goto bailout;
	}

	if (len != 0) {
		err_desc.error_pattern |= CTL_LUN_PAT_RANGE;
		/*
		 * We could check here to see whether it's a read/write
		 * command, but that will be pointless once we allow
		 * custom patterns.  At that point, the user could specify
		 * a READ(6) CDB type, and we wouldn't have an easy way here
		 * to verify whether range checking is possible there.  The
		 * user will just figure it out when his error never gets
		 * executed.
		 */
#if 0
		if ((err_desc.pattern & CTL_LUN_PAT_READWRITE) == 0) {
			warnx("%s: need read and/or write pattern if range "
			      "is specified", __func__);
			retval = 1;
			goto bailout;
		}
#endif
		err_desc.lba_range.lba = lba;
		err_desc.lba_range.len = len;
	}

	if (ioctl(fd, CTL_ERROR_INJECT, &err_desc) == -1) {
		warn("%s: error issuing CTL_ERROR_INJECT ioctl", __func__);
		retval = 1;
	} else {
		printf("Error injection succeeded, serial number is %ju\n",
		       (uintmax_t)err_desc.serial);
	}
bailout:

	return (retval);
}

static int
cctl_lunlist(int fd)
{
	struct scsi_report_luns_data *lun_data;
	struct scsi_inquiry_data *inq_data;
	uint32_t num_luns;
	int target;
	int initid;
	unsigned int i;
	int retval;

	retval = 0;
	inq_data = NULL;

	target = 6;
	initid = 7;

	/*
	 * XXX KDM assuming LUN 0 is fine, but we may need to change this
	 * if we ever acquire the ability to have multiple targets.
	 */
	if ((retval = cctl_get_luns(fd, target, /*lun*/ 0, initid,
				    /*retries*/ 2, &lun_data, &num_luns)) != 0)
		goto bailout;

	inq_data = malloc(sizeof(*inq_data));
	if (inq_data == NULL) {
		warn("%s: couldn't allocate memory for inquiry data\n",
		     __func__);
		retval = 1;
		goto bailout;
	}
	for (i = 0; i < num_luns; i++) {
		char scsi_path[40];
		int lun_val;

		switch (lun_data->luns[i].lundata[0] & RPL_LUNDATA_ATYP_MASK) {
		case RPL_LUNDATA_ATYP_PERIPH:
			lun_val = lun_data->luns[i].lundata[1];
			break;
		case RPL_LUNDATA_ATYP_FLAT:
			lun_val = (lun_data->luns[i].lundata[0] &
				RPL_LUNDATA_FLAT_LUN_MASK) |
				(lun_data->luns[i].lundata[1] <<
				RPL_LUNDATA_FLAT_LUN_BITS);
			break;
		case RPL_LUNDATA_ATYP_LUN:
		case RPL_LUNDATA_ATYP_EXTLUN:
		default:
			fprintf(stdout, "Unsupported LUN format %d\n",
				lun_data->luns[i].lundata[0] &
				RPL_LUNDATA_ATYP_MASK);
			lun_val = -1;
			break;
		}
		if (lun_val == -1)
			continue;

		if ((retval = cctl_get_inquiry(fd, target, lun_val, initid,
					       /*retries*/ 2, scsi_path,
					       sizeof(scsi_path),
					       inq_data)) != 0) {
			goto bailout;
		}
		printf("%s", scsi_path);
		scsi_print_inquiry(inq_data);
	}
bailout:

	if (lun_data != NULL)
		free(lun_data);

	if (inq_data != NULL)
		free(inq_data);

	return (retval);
}

static void
cctl_cfi_mt_statusstr(cfi_mt_status status, char *str, int str_len)
{
	switch (status) {
	case CFI_MT_PORT_OFFLINE:
		snprintf(str, str_len, "Port Offline");
		break;
	case CFI_MT_ERROR:
		snprintf(str, str_len, "Error");
		break;
	case CFI_MT_SUCCESS:
		snprintf(str, str_len, "Success");
		break;
	case CFI_MT_NONE:
		snprintf(str, str_len, "None??");
		break;
	default:
		snprintf(str, str_len, "Unknown status: %d", status);
		break;
	}
}

static void
cctl_cfi_bbr_statusstr(cfi_bbrread_status status, char *str, int str_len)
{
	switch (status) {
	case CFI_BBR_SUCCESS:
		snprintf(str, str_len, "Success");
		break;
	case CFI_BBR_LUN_UNCONFIG:
		snprintf(str, str_len, "LUN not configured");
		break;
	case CFI_BBR_NO_LUN:
		snprintf(str, str_len, "LUN does not exist");
		break;
	case CFI_BBR_NO_MEM:
		snprintf(str, str_len, "Memory allocation error");
		break;
	case CFI_BBR_BAD_LEN:
		snprintf(str, str_len, "Length is not a multiple of blocksize");
		break;
	case CFI_BBR_RESERV_CONFLICT:
		snprintf(str, str_len, "Reservation conflict");
		break;
	case CFI_BBR_LUN_STOPPED:
		snprintf(str, str_len, "LUN is powered off");
		break;
	case CFI_BBR_LUN_OFFLINE_CTL:
		snprintf(str, str_len, "LUN is offline");
		break;
	case CFI_BBR_LUN_OFFLINE_RC:
		snprintf(str, str_len, "RAIDCore array is offline (double "
			 "failure?)");
		break;
	case CFI_BBR_SCSI_ERROR:
		snprintf(str, str_len, "SCSI Error");
		break;
	case CFI_BBR_ERROR:
		snprintf(str, str_len, "Error");
		break;
	default:
		snprintf(str, str_len, "Unknown status: %d", status);
		break;
	}
}

static int
cctl_hardstopstart(int fd, ctladm_cmdfunction command)
{
	struct ctl_hard_startstop_info hs_info;
	char error_str[256];
	int do_start;
	int retval;

	retval = 0;

	if (command == CTLADM_CMD_HARDSTART)
		do_start = 1;
	else
		do_start = 0;

	if (ioctl(fd, (do_start == 1) ? CTL_HARD_START : CTL_HARD_STOP,
		  &hs_info) == -1) {
		warn("%s: CTL_HARD_%s ioctl failed", __func__,
		     (do_start == 1) ? "START" : "STOP");
		retval = 1;
		goto bailout;
	}

	fprintf(stdout, "Hard %s Status: ", (command == CTLADM_CMD_HARDSTOP) ?
		"Stop" : "Start");
	cctl_cfi_mt_statusstr(hs_info.status, error_str, sizeof(error_str));
	fprintf(stdout, "%s\n", error_str);
	fprintf(stdout, "Total LUNs: %d\n", hs_info.total_luns);
	fprintf(stdout, "LUNs complete: %d\n", hs_info.luns_complete);
	fprintf(stdout, "LUNs failed: %d\n", hs_info.luns_failed);

bailout:
	return (retval);
}

static int
cctl_bbrread(int fd, int target __unused, int lun, int iid __unused,
	     int argc, char **argv, char *combinedopt)
{
	struct ctl_bbrread_info bbr_info;
	char error_str[256];
	int datalen = -1;
	uint64_t lba = 0;
	int lba_set = 0;
	int retval;
	int c;

	retval = 0;

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'd':
			datalen = strtoul(optarg, NULL, 0);
			break;
		case 'l':
			lba = strtoull(optarg, NULL, 0);
			lba_set = 1;
			break;
		default:
			break;
		}
	}

	if (lba_set == 0) {
		warnx("%s: you must specify an LBA with -l", __func__);
		retval = 1;
		goto bailout;
	}

	if (datalen == -1) {
		warnx("%s: you must specify a length with -d", __func__);
		retval = 1;
		goto bailout;
	}

	bbr_info.lun_num = lun;
	bbr_info.lba = lba;
	/*
	 * XXX KDM get the blocksize first??
	 */
	if ((datalen % 512) != 0) {
		warnx("%s: data length %d is not a multiple of 512 bytes",
		     __func__, datalen);
		retval = 1;
		goto bailout;
	}
	bbr_info.len = datalen;

	if (ioctl(fd, CTL_BBRREAD, &bbr_info) == -1) {
		warn("%s: CTL_BBRREAD ioctl failed", __func__);
		retval = 1;
		goto bailout;
	}
	cctl_cfi_mt_statusstr(bbr_info.status, error_str, sizeof(error_str));
	fprintf(stdout, "BBR Read Overall Status: %s\n", error_str);
	cctl_cfi_bbr_statusstr(bbr_info.bbr_status, error_str,
			       sizeof(error_str));
	fprintf(stdout, "BBR Read Status: %s\n", error_str);
	/*
	 * XXX KDM should we bother printing out SCSI status if we get
	 * CFI_BBR_SCSI_ERROR back?
	 *
	 * Return non-zero if this fails?
	 */
bailout:
	return (retval);
}

static int
cctl_startup_shutdown(int fd, int target, int lun, int iid,
		      ctladm_cmdfunction command)
{
	union ctl_io *io;
	struct ctl_id id;
	struct scsi_report_luns_data *lun_data;
	struct scsi_inquiry_data *inq_data;
	uint32_t num_luns;
	unsigned int i;
	int retval;

	retval = 0;
	inq_data = NULL;

	/*
	 * - report luns
	 * - step through each lun, do an inquiry
	 * - check OOA queue on direct access luns
	 * - send stop with offline bit to each direct access device with a
	 *   clear OOA queue
	 *   - if we get a reservation conflict, reset the LUN to clear it
	 *     and reissue the stop with the offline bit set
	 */

	id.id = iid;

	io = ctl_scsi_alloc_io(id);
	if (io == NULL) {
		warnx("%s: can't allocate memory", __func__);
		return (1);
	}

	if ((retval = cctl_get_luns(fd, target, lun, iid, /*retries*/ 2,
				    &lun_data, &num_luns)) != 0)
		goto bailout;

	inq_data = malloc(sizeof(*inq_data));
	if (inq_data == NULL) {
		warn("%s: couldn't allocate memory for inquiry data\n",
		     __func__);
		retval = 1;
		goto bailout;
	}
	for (i = 0; i < num_luns; i++) {
		char scsi_path[40];
		int lun_val;

		/*
		 * XXX KDM figure out a way to share this code with
		 * cctl_lunlist()?
		 */
		switch (lun_data->luns[i].lundata[0] & RPL_LUNDATA_ATYP_MASK) {
		case RPL_LUNDATA_ATYP_PERIPH:
			lun_val = lun_data->luns[i].lundata[1];
			break;
		case RPL_LUNDATA_ATYP_FLAT:
			lun_val = (lun_data->luns[i].lundata[0] &
				RPL_LUNDATA_FLAT_LUN_MASK) |
				(lun_data->luns[i].lundata[1] <<
				RPL_LUNDATA_FLAT_LUN_BITS);
			break;
		case RPL_LUNDATA_ATYP_LUN:
		case RPL_LUNDATA_ATYP_EXTLUN:
		default:
			fprintf(stdout, "Unsupported LUN format %d\n",
				lun_data->luns[i].lundata[0] &
				RPL_LUNDATA_ATYP_MASK);
			lun_val = -1;
			break;
		}
		if (lun_val == -1)
			continue;

		if ((retval = cctl_get_inquiry(fd, target, lun_val, iid,
					       /*retries*/ 2, scsi_path,
					       sizeof(scsi_path),
					       inq_data)) != 0) {
			goto bailout;
		}
		printf("%s", scsi_path);
		scsi_print_inquiry(inq_data);
		/*
		 * We only want to shutdown direct access devices.
		 */
		if (SID_TYPE(inq_data) != T_DIRECT) {
			printf("%s LUN is not direct access, skipped\n",
			       scsi_path);
			continue;
		}

		if (command == CTLADM_CMD_SHUTDOWN) {
			struct ctl_ooa_info ooa_info;

			ooa_info.target_id = target;
			ooa_info.lun_id = lun_val;

			if (ioctl(fd, CTL_CHECK_OOA, &ooa_info) == -1) {
				printf("%s CTL_CHECK_OOA ioctl failed\n",
				       scsi_path);
				continue;
			}

			if (ooa_info.status != CTL_OOA_SUCCESS) {
				printf("%s CTL_CHECK_OOA returned status %d\n", 
				       scsi_path, ooa_info.status);
				continue;
			}
			if (ooa_info.num_entries != 0) {
				printf("%s %d entr%s in the OOA queue, "
				       "skipping shutdown\n", scsi_path,
				       ooa_info.num_entries,
				       (ooa_info.num_entries > 1)?"ies" : "y" );
				continue;
			}
		}
		
		ctl_scsi_start_stop(/*io*/ io,
				    /*start*/(command == CTLADM_CMD_STARTUP) ?
					      1 : 0,
				    /*load_eject*/ 0,
				    /*immediate*/ 0,
				    /*power_conditions*/ SSS_PC_START_VALID,
				    /*onoffline*/ 1,
				    /*ctl_tag_type*/
				    (command == CTLADM_CMD_STARTUP) ?
				    CTL_TAG_SIMPLE :CTL_TAG_ORDERED,
				    /*control*/ 0);

		io->io_hdr.nexus.targ_target.id = target;
		io->io_hdr.nexus.targ_lun = lun_val;
		io->io_hdr.nexus.initid = id;

		if (cctl_do_io(fd, /*retries*/ 3, io, __func__) != 0) {
			retval = 1;
			goto bailout;
		}

		if ((io->io_hdr.status & CTL_STATUS_MASK) != CTL_SUCCESS)
			ctl_io_error_print(io, inq_data, stderr);
		else {
			printf("%s LUN is now %s\n", scsi_path,
			       (command == CTLADM_CMD_STARTUP) ? "online" :
			       "offline");
		}
	}
bailout:
	if (lun_data != NULL)
		free(lun_data);

	if (inq_data != NULL)
		free(inq_data);

	if (io != NULL)
		ctl_scsi_free_io(io);

	return (retval);
}

static int
cctl_sync_cache(int fd, int target, int lun, int iid, int retries,
		int argc, char **argv, char *combinedopt)
{
	union ctl_io *io;
	struct ctl_id id;
	int cdb_size = -1;
	int retval;
	uint64_t our_lba = 0;
	uint32_t our_block_count = 0;
	int reladr = 0, immed = 0; 
	int c;

	id.id = iid;
	retval = 0;

	io = ctl_scsi_alloc_io(id);
	if (io == NULL) {
		warnx("%s: can't allocate memory", __func__);
		return (1);
	}

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'b':
			our_block_count = strtoul(optarg, NULL, 0);
			break;
		case 'c':
			cdb_size = strtol(optarg, NULL, 0);
			break;
		case 'i':
			immed = 1;
			break;
		case 'l':
			our_lba = strtoull(optarg, NULL, 0);
			break;
		case 'r':
			reladr = 1;
			break;
		default:
			break;
		}
	}

	if (cdb_size != -1) {
		switch (cdb_size) {
		case 10:
		case 16:
			break;
		default:
			warnx("%s: invalid cdbsize %d, valid sizes are 10 "
			      "and 16", __func__, cdb_size);
			retval = 1;
			goto bailout;
			break; /* NOTREACHED */
		}
	} else
		cdb_size = 10;

	ctl_scsi_sync_cache(/*io*/ io,
			    /*immed*/ immed,
			    /*reladr*/ reladr,
			    /*minimum_cdb_size*/ cdb_size,
			    /*starting_lba*/ our_lba,
			    /*block_count*/ our_block_count,
			    /*tag_type*/ CTL_TAG_SIMPLE,
			    /*control*/ 0);

	io->io_hdr.nexus.targ_target.id = target;
	io->io_hdr.nexus.targ_lun = lun;
	io->io_hdr.nexus.initid = id;

	if (cctl_do_io(fd, retries, io, __func__) != 0) {
		retval = 1;
		goto bailout;
	}

	if ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS) {
		fprintf(stdout, "Cache synchronized successfully\n");
	} else
		ctl_io_error_print(io, NULL, stderr);
bailout:
	ctl_scsi_free_io(io);

	return (retval);
}

static int
cctl_start_stop(int fd, int target, int lun, int iid, int retries, int start,
		int argc, char **argv, char *combinedopt)
{
	union ctl_io *io;
	struct ctl_id id;
	char scsi_path[40];
	int immed = 0, onoffline = 0;
	int retval, c;

	id.id = iid;
	retval = 0;

	io = ctl_scsi_alloc_io(id);
	if (io == NULL) {
		warnx("%s: can't allocate memory", __func__);
		return (1);
	}

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'i':
			immed = 1;
			break;
		case 'o':
			onoffline = 1;
			break;
		default:
			break;
		}
	}
	/*
	 * Use an ordered tag for the stop command, to guarantee that any
	 * pending I/O will finish before the stop command executes.  This
	 * would normally be the case anyway, since CTL will basically
	 * treat the start/stop command as an ordered command with respect
	 * to any other command except an INQUIRY.  (See ctl_ser_table.c.)
	 */
	ctl_scsi_start_stop(/*io*/ io,
			    /*start*/ start,
			    /*load_eject*/ 0,
			    /*immediate*/ immed,
			    /*power_conditions*/ SSS_PC_START_VALID,
			    /*onoffline*/ onoffline,
			    /*ctl_tag_type*/ start ? CTL_TAG_SIMPLE :
						     CTL_TAG_ORDERED,
			    /*control*/ 0);

	io->io_hdr.nexus.targ_target.id = target;
	io->io_hdr.nexus.targ_lun = lun;
	io->io_hdr.nexus.initid = id;

	if (cctl_do_io(fd, retries, io, __func__) != 0) {
		retval = 1;
		goto bailout;
	}

	ctl_scsi_path_string(io, scsi_path, sizeof(scsi_path));
	if ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS) {
		fprintf(stdout, "%s LUN %s successfully\n", scsi_path,
			(start) ?  "started" : "stopped");
	} else
		ctl_io_error_print(io, NULL, stderr);

bailout:
	ctl_scsi_free_io(io);

	return (retval);
}

static int
cctl_mode_sense(int fd, int target, int lun, int iid, int retries, 
		int argc, char **argv, char *combinedopt)
{
	union ctl_io *io;
	struct ctl_id id;
	uint32_t datalen;
	uint8_t *dataptr;
	int pc = -1, cdbsize, retval, dbd = 0, subpage = -1;
	int list = 0;
	int page_code = -1;
	int c;

	id.id = iid;
	cdbsize = 0;
	retval = 0;
	dataptr = NULL;

	io = ctl_scsi_alloc_io(id);
	if (io == NULL) {
		warn("%s: can't allocate memory", __func__);
		return (1);
	}

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'P':
			pc = strtoul(optarg, NULL, 0);
			break;
		case 'S':
			subpage = strtoul(optarg, NULL, 0);
			break;
		case 'd':
			dbd = 1;
			break;
		case 'l':
			list = 1;
			break;
		case 'm':
			page_code = strtoul(optarg, NULL, 0);
			break;
		case 'c':
			cdbsize = strtol(optarg, NULL, 0);
			break;
		default:
			break;
		}
	}

	if (((list == 0) && (page_code == -1))
	 || ((list != 0) && (page_code != -1))) {
		warnx("%s: you must specify either a page code (-m) or -l",
		      __func__);
		retval = 1;
		goto bailout;
	}

	if ((page_code != -1)
	 && ((page_code > SMS_ALL_PAGES_PAGE)
	  || (page_code < 0))) {
		warnx("%s: page code %d is out of range", __func__,
		      page_code);
		retval = 1;
		goto bailout;
	}

	if (list == 1) {
		page_code = SMS_ALL_PAGES_PAGE;
		if (pc != -1) {
			warnx("%s: arg -P makes no sense with -l",
			      __func__);
			retval = 1;
			goto bailout;
		}
		if (subpage != -1) {
			warnx("%s: arg -S makes no sense with -l", __func__);
			retval = 1;
			goto bailout;
		}
	}

	if (pc == -1)
		pc = SMS_PAGE_CTRL_CURRENT;
	else {
		if ((pc > 3)
		 || (pc < 0)) {
			warnx("%s: page control value %d is out of range: 0-3",
			      __func__, pc);
			retval = 1;
			goto bailout;
		}
	}


	if ((subpage != -1)
	 && ((subpage > 255)
	  || (subpage < 0))) {
		warnx("%s: subpage code %d is out of range: 0-255", __func__,
		      subpage);
		retval = 1;
		goto bailout;
	}
	if (cdbsize != 0) {
		switch (cdbsize) {
		case 6:
		case 10:
			break;
		default:
			warnx("%s: invalid cdbsize %d, valid sizes are 6 "
			      "and 10", __func__, cdbsize);
			retval = 1;
			goto bailout;
			break;
		}
	} else
		cdbsize = 6;

	if (subpage == -1)
		subpage = 0;

	if (cdbsize == 6)
		datalen = 255;
	else
		datalen = 65535;

	dataptr = (uint8_t *)malloc(datalen);
	if (dataptr == NULL) {
		warn("%s: can't allocate %d bytes", __func__, datalen);
		retval = 1;
		goto bailout;
	}

	memset(dataptr, 0, datalen);

	ctl_scsi_mode_sense(io,
			    /*data_ptr*/ dataptr,
			    /*data_len*/ datalen,
			    /*dbd*/ dbd,
			    /*llbaa*/ 0,
			    /*page_code*/ page_code,
			    /*pc*/ pc << 6,
			    /*subpage*/ subpage,
			    /*minimum_cdb_size*/ cdbsize,
			    /*tag_type*/ CTL_TAG_SIMPLE,
			    /*control*/ 0);

	io->io_hdr.nexus.targ_target.id = target;
	io->io_hdr.nexus.targ_lun = lun;
	io->io_hdr.nexus.initid = id;

	if (cctl_do_io(fd, retries, io, __func__) != 0) {
		retval = 1;
		goto bailout;
	}

	if ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS) {
		int pages_len, used_len;
		uint32_t returned_len;
		uint8_t *ndataptr;

		if (io->scsiio.cdb[0] == MODE_SENSE_6) {
			struct scsi_mode_hdr_6 *hdr6;
			int bdlen;

			hdr6 = (struct scsi_mode_hdr_6 *)dataptr;

			returned_len = hdr6->datalen + 1;
			bdlen = hdr6->block_descr_len;

			ndataptr = (uint8_t *)((uint8_t *)&hdr6[1] + bdlen);
		} else {
			struct scsi_mode_hdr_10 *hdr10;
			int bdlen;

			hdr10 = (struct scsi_mode_hdr_10 *)dataptr;

			returned_len = scsi_2btoul(hdr10->datalen) + 2;
			bdlen = scsi_2btoul(hdr10->block_descr_len);

			ndataptr = (uint8_t *)((uint8_t *)&hdr10[1] + bdlen);
		}
		/* just in case they can give us more than we allocated for */
		returned_len = min(returned_len, datalen);
		pages_len = returned_len - (ndataptr - dataptr);
#if 0
		fprintf(stdout, "returned_len = %d, pages_len = %d\n",
			returned_len, pages_len);
#endif
		if (list == 1) {
			fprintf(stdout, "Supported mode pages:\n");
			for (used_len = 0; used_len < pages_len;) {
				struct scsi_mode_page_header *header;

				header = (struct scsi_mode_page_header *)
					&ndataptr[used_len];
				fprintf(stdout, "%d\n", header->page_code);
				used_len += header->page_length + 2;
			}
		} else {
			for (used_len = 0; used_len < pages_len; used_len++) {
				fprintf(stdout, "0x%x ", ndataptr[used_len]);
				if (((used_len+1) % 16) == 0)
					fprintf(stdout, "\n");
			}
			fprintf(stdout, "\n");
		}
	} else
		ctl_io_error_print(io, NULL, stderr);
bailout:

	ctl_scsi_free_io(io);

	if (dataptr != NULL)
		free(dataptr);

	return (retval);
}

static int
cctl_read_capacity(int fd, int target, int lun, int iid, int retries, 
           	   int argc, char **argv, char *combinedopt)
{
	union ctl_io *io;
	struct ctl_id id;
	struct scsi_read_capacity_data *data;
	struct scsi_read_capacity_data_long *longdata;
	int cdbsize = -1, retval;
	uint8_t *dataptr;
	int c;

	cdbsize = 10;
	dataptr = NULL;
	retval = 0;
	id.id = iid;

	io = ctl_scsi_alloc_io(id);
	if (io == NULL) {
		warn("%s: can't allocate memory\n", __func__);
		return (1);
	}

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'c':
			cdbsize = strtol(optarg, NULL, 0);
			break;
		default:
			break;
		}
	}
	if (cdbsize != -1) {
		switch (cdbsize) {
		case 10:
		case 16:
			break;
		default:
			warnx("%s: invalid cdbsize %d, valid sizes are 10 "
			      "and 16", __func__, cdbsize);
			retval = 1;
			goto bailout;
			break; /* NOTREACHED */
		}
	} else
		cdbsize = 10;

	dataptr = (uint8_t *)malloc(sizeof(*longdata));
	if (dataptr == NULL) {
		warn("%s: can't allocate %zd bytes\n", __func__,
		     sizeof(*longdata));
		retval = 1;
		goto bailout;
	}
	memset(dataptr, 0, sizeof(*longdata));

retry:

	switch (cdbsize) {
	case 10:
		ctl_scsi_read_capacity(io,
				       /*data_ptr*/ dataptr,
				       /*data_len*/ sizeof(*longdata),
				       /*addr*/ 0,
				       /*reladr*/ 0,
				       /*pmi*/ 0,
				       /*tag_type*/ CTL_TAG_SIMPLE,
				       /*control*/ 0);
		break;
	case 16:
		ctl_scsi_read_capacity_16(io,
					  /*data_ptr*/ dataptr,
					  /*data_len*/ sizeof(*longdata),
					  /*addr*/ 0,
					  /*reladr*/ 0,
					  /*pmi*/ 0,
					  /*tag_type*/ CTL_TAG_SIMPLE,
					  /*control*/ 0);
		break;
	}

	io->io_hdr.nexus.initid = id;
	io->io_hdr.nexus.targ_target.id = target;
	io->io_hdr.nexus.targ_lun = lun;

	if (cctl_do_io(fd, retries, io, __func__) != 0) {
		retval = 1;
		goto bailout;
	}

	if ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS) {
		uint64_t maxlba;
		uint32_t blocksize;

		if (cdbsize == 10) {

			data = (struct scsi_read_capacity_data *)dataptr;

			maxlba = scsi_4btoul(data->addr);
			blocksize = scsi_4btoul(data->length);

			if (maxlba == 0xffffffff) {
				cdbsize = 16;
				goto retry;
			}
		} else {
			longdata=(struct scsi_read_capacity_data_long *)dataptr;

			maxlba = scsi_8btou64(longdata->addr);
			blocksize = scsi_4btoul(longdata->length);
		}

		fprintf(stdout, "Disk Capacity: %ju, Blocksize: %d\n",
			(uintmax_t)maxlba, blocksize);
	} else {
		ctl_io_error_print(io, NULL, stderr);
	}
bailout:
	ctl_scsi_free_io(io);

	if (dataptr != NULL)
		free(dataptr);

	return (retval);
}

static int
cctl_read_write(int fd, int target, int lun, int iid, int retries,
		int argc, char **argv, char *combinedopt,
		ctladm_cmdfunction command)
{
	union ctl_io *io;
	struct ctl_id id;
	int file_fd, do_stdio;
	int cdbsize = -1, databytes;
	uint8_t *dataptr;
	char *filename = NULL;
	int datalen = -1, blocksize = -1;
	uint64_t lba = 0;
	int lba_set = 0;
	int retval;
	int c;

	retval = 0;
	do_stdio = 0;
	dataptr = NULL;
	file_fd = -1;
	id.id = iid;

	io = ctl_scsi_alloc_io(id);
	if (io == NULL) {
		warn("%s: can't allocate memory\n", __func__);
		return (1);
	}

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'N':
			io->io_hdr.flags |= CTL_FLAG_NO_DATAMOVE;
			break;
		case 'b':
			blocksize = strtoul(optarg, NULL, 0);
			break;
		case 'c':
			cdbsize = strtoul(optarg, NULL, 0);
			break;
		case 'd':
			datalen = strtoul(optarg, NULL, 0);
			break;
		case 'f':
			filename = strdup(optarg);
			break;
		case 'l':
			lba = strtoull(optarg, NULL, 0);
			lba_set = 1;
			break;
		default:
			break;
		}
	}
	if (filename == NULL) {
		warnx("%s: you must supply a filename using -f", __func__);
		retval = 1;
		goto bailout;
	}

	if (datalen == -1) {
		warnx("%s: you must specify the data length with -d", __func__);
		retval = 1;
		goto bailout;
	}

	if (lba_set == 0) {
		warnx("%s: you must specify the LBA with -l", __func__);
		retval = 1;
		goto bailout;
	}

	if (blocksize == -1) {
		warnx("%s: you must specify the blocksize with -b", __func__);
		retval = 1;
		goto bailout;
	}

	if (cdbsize != -1) {
		switch (cdbsize) {
		case 6:
		case 10:
		case 12:
		case 16:
			break;
		default:
			warnx("%s: invalid cdbsize %d, valid sizes are 6, "
			      "10, 12 or 16", __func__, cdbsize);
			retval = 1;
			goto bailout;
			break; /* NOTREACHED */
		}
	} else
		cdbsize = 6;

	databytes = datalen * blocksize;
	dataptr = (uint8_t *)malloc(databytes);

	if (dataptr == NULL) {
		warn("%s: can't allocate %d bytes\n", __func__, databytes);
		retval = 1;
		goto bailout;
	}
	if (strcmp(filename, "-") == 0) {
		if (command == CTLADM_CMD_READ)
			file_fd = STDOUT_FILENO;
		else
			file_fd = STDIN_FILENO;
		do_stdio = 1;
	} else {
		file_fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
		if (file_fd == -1) {
			warn("%s: can't open file %s", __func__, filename);
			retval = 1;
			goto bailout;
		}
	}

	memset(dataptr, 0, databytes);

	if (command == CTLADM_CMD_WRITE) {
		int bytes_read;

		bytes_read = read(file_fd, dataptr, databytes);
		if (bytes_read == -1) {
			warn("%s: error reading file %s", __func__, filename);
			retval = 1;
			goto bailout;
		}
		if (bytes_read != databytes) {
			warnx("%s: only read %d bytes from file %s",
			      __func__, bytes_read, filename);
			retval = 1;
			goto bailout;
		}
	}
	ctl_scsi_read_write(io,
			    /*data_ptr*/ dataptr,
			    /*data_len*/ databytes,
			    /*read_op*/ (command == CTLADM_CMD_READ) ? 1 : 0,
			    /*byte2*/ 0,
			    /*minimum_cdb_size*/ cdbsize,
			    /*lba*/ lba,
			    /*num_blocks*/ datalen,
			    /*tag_type*/ CTL_TAG_SIMPLE,
			    /*control*/ 0);

	io->io_hdr.nexus.targ_target.id = target;
	io->io_hdr.nexus.targ_lun = lun;
	io->io_hdr.nexus.initid = id;

	if (cctl_do_io(fd, retries, io, __func__) != 0) {
		retval = 1;
		goto bailout;
	}

	if (((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS)
	 && (command == CTLADM_CMD_READ)) {
		int bytes_written;

		bytes_written = write(file_fd, dataptr, databytes);
		if (bytes_written == -1) {
			warn("%s: can't write to %s", __func__, filename);
			goto bailout;
		}
	} else if ((io->io_hdr.status & CTL_STATUS_MASK) != CTL_SUCCESS)
		ctl_io_error_print(io, NULL, stderr);


bailout:

	ctl_scsi_free_io(io);

	if (dataptr != NULL)
		free(dataptr);

	if ((do_stdio == 0)
	 && (file_fd != -1))
		close(file_fd);

	return (retval);
}

static int
cctl_get_luns(int fd, int target, int lun, int iid, int retries, struct
	      scsi_report_luns_data **lun_data, uint32_t *num_luns)
{
	union ctl_io *io;
	struct ctl_id id;
	uint32_t nluns;
	int lun_datalen;
	int retval;

	retval = 0;
	id.id = iid;

	io = ctl_scsi_alloc_io(id);
	if (io == NULL) {
		warnx("%s: can't allocate memory", __func__);
		return (1);
	}

	/*
	 * lun_data includes space for 1 lun, allocate space for 4 initially.
	 * If that isn't enough, we'll allocate more.
	 */
	nluns = 4;
retry:
	lun_datalen = sizeof(*lun_data) +
		(nluns * sizeof(struct scsi_report_luns_lundata));
	*lun_data = malloc(lun_datalen);

	if (*lun_data == NULL) {
		warnx("%s: can't allocate memory", __func__);
		ctl_scsi_free_io(io);
		return (1);
	}

	ctl_scsi_report_luns(io,
			     /*data_ptr*/ (uint8_t *)*lun_data,
			     /*data_len*/ lun_datalen,
			     /*select_report*/ RPL_REPORT_ALL,
			     /*tag_type*/ CTL_TAG_SIMPLE,
			     /*control*/ 0);

	io->io_hdr.nexus.initid = id;
	io->io_hdr.nexus.targ_target.id = target;
	io->io_hdr.nexus.targ_lun = lun;

	if (cctl_do_io(fd, retries, io, __func__) != 0) {
		retval = 1;
		goto bailout;
	}

	if ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS) {
		uint32_t returned_len, returned_luns;

		returned_len = scsi_4btoul((*lun_data)->length);
		returned_luns = returned_len / 8;
		if (returned_luns > nluns) {
			nluns = returned_luns;
			free(*lun_data);
			goto retry;
		}
		/* These should be the same */
		*num_luns = MIN(returned_luns, nluns);
	} else {
		ctl_io_error_print(io, NULL, stderr);
		retval = 1;
	}
bailout:
	ctl_scsi_free_io(io);

	return (retval);
}

static int
cctl_report_luns(int fd, int target, int lun, int iid, int retries)
{
	struct scsi_report_luns_data *lun_data;
	uint32_t num_luns, i;
	int retval;

	lun_data = NULL;

	if ((retval = cctl_get_luns(fd, target, lun, iid, retries, &lun_data,
				   &num_luns)) != 0)
		goto bailout;

	fprintf(stdout, "%u LUNs returned\n", num_luns);
	for (i = 0; i < num_luns; i++) {
		int lun_val;

		/*
		 * XXX KDM figure out a way to share this code with
		 * cctl_lunlist()?
		 */
		switch (lun_data->luns[i].lundata[0] & RPL_LUNDATA_ATYP_MASK) {
		case RPL_LUNDATA_ATYP_PERIPH:
			lun_val = lun_data->luns[i].lundata[1];
			break;
		case RPL_LUNDATA_ATYP_FLAT:
			lun_val = (lun_data->luns[i].lundata[0] &
				RPL_LUNDATA_FLAT_LUN_MASK) |
				(lun_data->luns[i].lundata[1] <<
				RPL_LUNDATA_FLAT_LUN_BITS);
			break;
		case RPL_LUNDATA_ATYP_LUN:
		case RPL_LUNDATA_ATYP_EXTLUN:
		default:
			fprintf(stdout, "Unsupported LUN format %d\n",
				lun_data->luns[i].lundata[0] &
				RPL_LUNDATA_ATYP_MASK);
			lun_val = -1;
			break;
		}
		if (lun_val == -1)
			continue;

		fprintf(stdout, "%d\n", lun_val);
	}

bailout:
	if (lun_data != NULL)
		free(lun_data);

	return (retval);
}

static int
cctl_tur(int fd, int target, int lun, int iid, int retries)
{
	union ctl_io *io;
	struct ctl_id id;

	id.id = iid;

	io = ctl_scsi_alloc_io(id);
	if (io == NULL) {
		fprintf(stderr, "can't allocate memory\n");
		return (1);
	}

	ctl_scsi_tur(io,
		     /* tag_type */ CTL_TAG_SIMPLE,
		     /* control */ 0);

	io->io_hdr.nexus.targ_target.id = target;
	io->io_hdr.nexus.targ_lun = lun;
	io->io_hdr.nexus.initid = id;

	if (cctl_do_io(fd, retries, io, __func__) != 0) {
		ctl_scsi_free_io(io);
		return (1);
	}

	if ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS)
		fprintf(stdout, "Unit is ready\n");
	else
		ctl_io_error_print(io, NULL, stderr);

	return (0);
}

static int
cctl_get_inquiry(int fd, int target, int lun, int iid, int retries, 
		 char *path_str, int path_len,
		 struct scsi_inquiry_data *inq_data)
{
	union ctl_io *io;
	struct ctl_id id;
	int retval;

	retval = 0;

	id.id = iid;

	io = ctl_scsi_alloc_io(id);
	if (io == NULL) {
		warnx("cctl_inquiry: can't allocate memory\n");
		return (1);
	}

	ctl_scsi_inquiry(/*io*/ io,
			 /*data_ptr*/ (uint8_t *)inq_data,
			 /*data_len*/ sizeof(*inq_data),
			 /*byte2*/ 0,
			 /*page_code*/ 0,
			 /*tag_type*/ CTL_TAG_SIMPLE,
			 /*control*/ 0);

	io->io_hdr.nexus.targ_target.id = target;
	io->io_hdr.nexus.targ_lun = lun;
	io->io_hdr.nexus.initid = id;

	if (cctl_do_io(fd, retries, io, __func__) != 0) {
		retval = 1;
		goto bailout;
	}

	if ((io->io_hdr.status & CTL_STATUS_MASK) != CTL_SUCCESS) {
		retval = 1;
		ctl_io_error_print(io, NULL, stderr);
	} else if (path_str != NULL)
		ctl_scsi_path_string(io, path_str, path_len);

bailout:
	ctl_scsi_free_io(io);

	return (retval);
}

static int
cctl_inquiry(int fd, int target, int lun, int iid, int retries)
{
	struct scsi_inquiry_data *inq_data;
	char scsi_path[40];
	int retval;

	retval = 0;

	inq_data = malloc(sizeof(*inq_data));
	if (inq_data == NULL) {
		warnx("%s: can't allocate inquiry data", __func__);
		retval = 1;
		goto bailout;
	}

	if ((retval = cctl_get_inquiry(fd, target, lun, iid, retries, scsi_path,
				       sizeof(scsi_path), inq_data)) != 0)
		goto bailout;

	printf("%s", scsi_path);
	scsi_print_inquiry(inq_data);

bailout:
	if (inq_data != NULL)
		free(inq_data);

	return (retval);
}

static int
cctl_req_sense(int fd, int target, int lun, int iid, int retries)
{
	union ctl_io *io;
	struct scsi_sense_data *sense_data;
	struct ctl_id id;
	int retval;

	retval = 0;

	id.id = iid;

	io = ctl_scsi_alloc_io(id);
	if (io == NULL) {
		warnx("cctl_req_sense: can't allocate memory\n");
		return (1);
	}
	sense_data = malloc(sizeof(*sense_data));
	memset(sense_data, 0, sizeof(*sense_data));

	ctl_scsi_request_sense(/*io*/ io,
			       /*data_ptr*/ (uint8_t *)sense_data,
			       /*data_len*/ sizeof(*sense_data),
			       /*byte2*/ 0,
			       /*tag_type*/ CTL_TAG_SIMPLE,
			       /*control*/ 0);

	io->io_hdr.nexus.targ_target.id = target;
	io->io_hdr.nexus.targ_lun = lun;
	io->io_hdr.nexus.initid = id;

	if (cctl_do_io(fd, retries, io, __func__) != 0) {
		retval = 1;
		goto bailout;
	}

	if ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS) {
		bcopy(sense_data, &io->scsiio.sense_data, sizeof(*sense_data));
		io->scsiio.sense_len = sizeof(*sense_data);
		ctl_scsi_sense_print(&io->scsiio, NULL, stdout);
	} else
		ctl_io_error_print(io, NULL, stderr);

bailout:

	ctl_scsi_free_io(io);
	free(sense_data);

	return (retval);
}

static int
cctl_report_target_port_group(int fd, int target, int lun, int initiator)
{
	union ctl_io *io;
	struct ctl_id id;
	uint32_t datalen;
	uint8_t *dataptr;
	int retval;

	id.id = initiator;
	dataptr = NULL;
	retval = 0;

	io = ctl_scsi_alloc_io(id);
	if (io == NULL) {
		warn("%s: can't allocate memory", __func__);
		return (1);
	}

	datalen = 64;
	dataptr = (uint8_t *)malloc(datalen);
	if (dataptr == NULL) {
		warn("%s: can't allocate %d bytes", __func__, datalen);
	    	retval = 1;
		goto bailout;
	}

	memset(dataptr, 0, datalen);

	ctl_scsi_maintenance_in(/*io*/ io,
				/*data_ptr*/ dataptr,
				/*data_len*/ datalen,
				/*action*/ SA_RPRT_TRGT_GRP,
				/*tag_type*/ CTL_TAG_SIMPLE,
				/*control*/ 0);

	io->io_hdr.nexus.targ_target.id = target;
	io->io_hdr.nexus.targ_lun = lun;
	io->io_hdr.nexus.initid = id;

	if (cctl_do_io(fd, 0, io, __func__) != 0) {
		retval = 1;
		goto bailout;
	}

	if ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS) {
		int returned_len, used_len;

		returned_len = scsi_4btoul(&dataptr[0]) + 4;

		for (used_len = 0; used_len < returned_len; used_len++) {
			fprintf(stdout, "0x%02x ", dataptr[used_len]);
			if (((used_len+1) % 8) == 0)
				fprintf(stdout, "\n");
		}
		fprintf(stdout, "\n");
	} else
		ctl_io_error_print(io, NULL, stderr);

bailout:
	ctl_scsi_free_io(io);

	if (dataptr != NULL)
		free(dataptr);

	return (retval);
}

static int
cctl_inquiry_vpd_devid(int fd, int target, int lun, int initiator)
{
	union ctl_io *io;
	struct ctl_id id;
	uint32_t datalen;
	uint8_t *dataptr;
	int retval;

	id.id = initiator;
	retval = 0;
	dataptr = NULL;

	io = ctl_scsi_alloc_io(id);
	if (io == NULL) {
		warn("%s: can't allocate memory", __func__);
		return (1);
	}

	datalen = 256;
	dataptr = (uint8_t *)malloc(datalen);
	if (dataptr == NULL) {
		warn("%s: can't allocate %d bytes", __func__, datalen);
	    	retval = 1;
		goto bailout;
	}

	memset(dataptr, 0, datalen);

	ctl_scsi_inquiry(/*io*/        io,
			 /*data_ptr*/  dataptr,
			 /*data_len*/  datalen,
			 /*byte2*/     SI_EVPD,
			 /*page_code*/ SVPD_DEVICE_ID,
			 /*tag_type*/  CTL_TAG_SIMPLE,
			 /*control*/   0);

	io->io_hdr.nexus.targ_target.id = target;
	io->io_hdr.nexus.targ_lun = lun;
	io->io_hdr.nexus.initid = id;

	if (cctl_do_io(fd, 0, io, __func__) != 0) {
		retval = 1;
		goto bailout;
	}

	if ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS) {
		int returned_len, used_len;

		returned_len = scsi_2btoul(&dataptr[2]) + 4;

		for (used_len = 0; used_len < returned_len; used_len++) {
			fprintf(stdout, "0x%02x ", dataptr[used_len]);
			if (((used_len+1) % 8) == 0)
				fprintf(stdout, "\n");
		}
		fprintf(stdout, "\n");
	} else
		ctl_io_error_print(io, NULL, stderr);

bailout:
	ctl_scsi_free_io(io);

	if (dataptr != NULL)
		free(dataptr);

	return (retval);
}

static int
cctl_persistent_reserve_in(int fd, int target, int lun, int initiator, 
                           int argc, char **argv, char *combinedopt,
			   int retry_count)
{
	union ctl_io *io;
	struct ctl_id id;
	uint32_t datalen;
	uint8_t *dataptr;
	int action = -1;
	int retval;
	int c;

	id.id = initiator;
	retval = 0;
	dataptr = NULL;

	io = ctl_scsi_alloc_io(id);
	if (io == NULL) {
		warn("%s: can't allocate memory", __func__);
		return (1);
	}

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'a':
			action = strtol(optarg, NULL, 0);
			break;
		default:
			break;
		}
	}

	if (action < 0 || action > 2) {
		warn("action must be specified and in the range: 0-2");
		retval = 1;
		goto bailout;
	}


	datalen = 256;
	dataptr = (uint8_t *)malloc(datalen);
	if (dataptr == NULL) {
		warn("%s: can't allocate %d bytes", __func__, datalen);
	    	retval = 1;
		goto bailout;
	}

	memset(dataptr, 0, datalen);

	ctl_scsi_persistent_res_in(io,
				   /*data_ptr*/ dataptr,
				   /*data_len*/ datalen,
				   /*action*/   action,
				   /*tag_type*/ CTL_TAG_SIMPLE,
				   /*control*/  0);

	io->io_hdr.nexus.targ_target.id = target;
	io->io_hdr.nexus.targ_lun = lun;
	io->io_hdr.nexus.initid = id;

	if (cctl_do_io(fd, retry_count, io, __func__) != 0) {
		retval = 1;
		goto bailout;
	}

	if ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS) {
		int returned_len, used_len;

		returned_len = 0;

		switch (action) {
		case 0:
			returned_len = scsi_4btoul(&dataptr[4]) + 8;
			returned_len = min(returned_len, 256);
			break;
		case 1:
			returned_len = scsi_4btoul(&dataptr[4]) + 8;
			break;
		case 2:
			returned_len = 8;
			break;
		default:
			warnx("%s: invalid action %d", __func__, action);
			goto bailout;
			break; /* NOTREACHED */
		}

		for (used_len = 0; used_len < returned_len; used_len++) {
			fprintf(stdout, "0x%02x ", dataptr[used_len]);
			if (((used_len+1) % 8) == 0)
				fprintf(stdout, "\n");
		}
		fprintf(stdout, "\n");
	} else
		ctl_io_error_print(io, NULL, stderr);

bailout:
	ctl_scsi_free_io(io);

	if (dataptr != NULL)
		free(dataptr);

	return (retval);
}

static int
cctl_persistent_reserve_out(int fd, int target, int lun, int initiator, 
			    int argc, char **argv, char *combinedopt, 
			    int retry_count)
{
	union ctl_io *io;
	struct ctl_id id;
	uint32_t datalen;
	uint64_t key = 0, sa_key = 0;
	int action = -1, restype = -1;
	uint8_t *dataptr;
	int retval;
	int c;

	id.id = initiator;
	retval = 0;
	dataptr = NULL;

	io = ctl_scsi_alloc_io(id);
	if (io == NULL) {
		warn("%s: can't allocate memory", __func__);
		return (1);
	}

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'a':
			action = strtol(optarg, NULL, 0);
			break;
		case 'k':
			key = strtoull(optarg, NULL, 0);
			break;
		case 'r':
			restype = strtol(optarg, NULL, 0);
			break;
		case 's':
			sa_key = strtoull(optarg, NULL, 0);
			break;
		default:
			break;
		}
	}
	if (action < 0 || action > 5) {
		warn("action must be specified and in the range: 0-5");
		retval = 1;
		goto bailout;
	}

	if (restype < 0 || restype > 5) {
		if (action != 0 && action != 5 && action != 3) {
			warn("'restype' must specified and in the range: 0-5");
			retval = 1;
			goto bailout;
		}
	}

	datalen = 24;
	dataptr = (uint8_t *)malloc(datalen);
	if (dataptr == NULL) {
		warn("%s: can't allocate %d bytes", __func__, datalen);
		retval = 1;
		goto bailout;
	}

	memset(dataptr, 0, datalen);

	ctl_scsi_persistent_res_out(io,
				    /*data_ptr*/ dataptr,
				    /*data_len*/ datalen,
				    /*action*/   action,
				    /*type*/     restype,
				    /*key*/      key,
				    /*sa key*/   sa_key,
				    /*tag_type*/ CTL_TAG_SIMPLE,
				    /*control*/  0);

	io->io_hdr.nexus.targ_target.id = target;
	io->io_hdr.nexus.targ_lun = lun;
	io->io_hdr.nexus.initid = id;

	if (cctl_do_io(fd, retry_count, io, __func__) != 0) {
		retval = 1;
		goto bailout;
	}
	if ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS) {
		char scsi_path[40];
		ctl_scsi_path_string(io, scsi_path, sizeof(scsi_path));
		fprintf( stdout, "%sPERSISTENT RESERVE OUT executed "
			"successfully\n", scsi_path);
	} else
		ctl_io_error_print(io, NULL, stderr);

bailout:
	ctl_scsi_free_io(io);

	if (dataptr != NULL)
		free(dataptr);

	return (retval);
}

struct cctl_req_option {
	char			     *name;
	int			      namelen;
	char			     *value;
	int			      vallen;
	STAILQ_ENTRY(cctl_req_option) links;
};

static int
cctl_create_lun(int fd, int argc, char **argv, char *combinedopt)
{
	struct ctl_lun_req req;
	int device_type = -1;
	uint64_t lun_size = 0;
	uint32_t blocksize = 0, req_lun_id = 0;
	char *serial_num = NULL;
	char *device_id = NULL;
	int lun_size_set = 0, blocksize_set = 0, lun_id_set = 0;
	char *backend_name = NULL;
	STAILQ_HEAD(, cctl_req_option) option_list;
	int num_options = 0;
	int retval = 0, c;

	STAILQ_INIT(&option_list);

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'b':
			backend_name = strdup(optarg);
			break;
		case 'B':
			blocksize = strtoul(optarg, NULL, 0);
			blocksize_set = 1;
			break;
		case 'd':
			device_id = strdup(optarg);
			break;
		case 'l':
			req_lun_id = strtoul(optarg, NULL, 0);
			lun_id_set = 1;
			break;
		case 'o': {
			struct cctl_req_option *option;
			char *tmpstr;
			char *name, *value;

			tmpstr = strdup(optarg);
			name = strsep(&tmpstr, "=");
			if (name == NULL) {
				warnx("%s: option -o takes \"name=value\""
				      "argument", __func__);
				retval = 1;
				goto bailout;
			}
			value = strsep(&tmpstr, "=");
			if (value == NULL) {
				warnx("%s: option -o takes \"name=value\""
				      "argument", __func__);
				retval = 1;
				goto bailout;
			}
			option = malloc(sizeof(*option));
			if (option == NULL) {
				warn("%s: error allocating %zd bytes",
				     __func__, sizeof(*option));
				retval = 1;
				goto bailout;
			}
			option->name = strdup(name);
			option->namelen = strlen(name) + 1;
			option->value = strdup(value);
			option->vallen = strlen(value) + 1;
			free(tmpstr);

			STAILQ_INSERT_TAIL(&option_list, option, links);
			num_options++;
			break;
		}
		case 's':
			if (strcasecmp(optarg, "auto") != 0) {
				retval = expand_number(optarg, &lun_size);
				if (retval != 0) {
					warn("%s: invalid -s argument",
					    __func__);
					retval = 1;
					goto bailout;
				}
			}
			lun_size_set = 1;
			break;
		case 'S':
			serial_num = strdup(optarg);
			break;
		case 't':
			device_type = strtoul(optarg, NULL, 0);
			break;
		default:
			break;
		}
	}

	if (backend_name == NULL) {
		warnx("%s: backend name (-b) must be specified", __func__);
		retval = 1;
		goto bailout;
	}

	bzero(&req, sizeof(req));

	strlcpy(req.backend, backend_name, sizeof(req.backend));
	req.reqtype = CTL_LUNREQ_CREATE;

	if (blocksize_set != 0)
		req.reqdata.create.blocksize_bytes = blocksize;

	if (lun_size_set != 0)
		req.reqdata.create.lun_size_bytes = lun_size;

	if (lun_id_set != 0) {
		req.reqdata.create.flags |= CTL_LUN_FLAG_ID_REQ;
		req.reqdata.create.req_lun_id = req_lun_id;
	}

	req.reqdata.create.flags |= CTL_LUN_FLAG_DEV_TYPE;

	if (device_type != -1)
		req.reqdata.create.device_type = device_type;
	else
		req.reqdata.create.device_type = T_DIRECT;

	if (serial_num != NULL) {
		strlcpy(req.reqdata.create.serial_num, serial_num,
			sizeof(req.reqdata.create.serial_num));
		req.reqdata.create.flags |= CTL_LUN_FLAG_SERIAL_NUM;
	}

	if (device_id != NULL) {
		strlcpy(req.reqdata.create.device_id, device_id,
			sizeof(req.reqdata.create.device_id));
		req.reqdata.create.flags |= CTL_LUN_FLAG_DEVID;
	}

	req.num_be_args = num_options;
	if (num_options > 0) {
		struct cctl_req_option *option, *next_option;
		int i;

		req.be_args = malloc(num_options * sizeof(*req.be_args));
		if (req.be_args == NULL) {
			warn("%s: error allocating %zd bytes", __func__,
			     num_options * sizeof(*req.be_args));
			retval = 1;
			goto bailout;
		}

		for (i = 0, option = STAILQ_FIRST(&option_list);
		     i < num_options; i++, option = next_option) {
			next_option = STAILQ_NEXT(option, links);

			req.be_args[i].namelen = option->namelen;
			req.be_args[i].name = strdup(option->name);
			req.be_args[i].vallen = option->vallen;
			req.be_args[i].value = strdup(option->value);
			/*
			 * XXX KDM do we want a way to specify a writeable
			 * flag of some sort?  Do we want a way to specify
			 * binary data?
			 */
			req.be_args[i].flags = CTL_BEARG_ASCII | CTL_BEARG_RD;

			STAILQ_REMOVE(&option_list, option, cctl_req_option,
				      links);
			free(option->name);
			free(option->value);
			free(option);
		}
	}

	if (ioctl(fd, CTL_LUN_REQ, &req) == -1) {
		warn("%s: error issuing CTL_LUN_REQ ioctl", __func__);
		retval = 1;
		goto bailout;
	}

	if (req.status == CTL_LUN_ERROR) {
		warnx("%s: error returned from LUN creation request:\n%s",
		      __func__, req.error_str);
		retval = 1;
		goto bailout;
	} else if (req.status != CTL_LUN_OK) {
		warnx("%s: unknown LUN creation request status %d",
		      __func__, req.status);
		retval = 1;
		goto bailout;
	}

	fprintf(stdout, "LUN created successfully\n");
	fprintf(stdout, "backend:       %s\n", req.backend);
	fprintf(stdout, "device type:   %d\n",req.reqdata.create.device_type);
	fprintf(stdout, "LUN size:      %ju bytes\n",
		(uintmax_t)req.reqdata.create.lun_size_bytes);
	fprintf(stdout, "blocksize      %u bytes\n",
		req.reqdata.create.blocksize_bytes);
	fprintf(stdout, "LUN ID:        %d\n", req.reqdata.create.req_lun_id);
	fprintf(stdout, "Serial Number: %s\n", req.reqdata.create.serial_num);
	fprintf(stdout, "Device ID;     %s\n", req.reqdata.create.device_id);

bailout:
	return (retval);
}

static int
cctl_rm_lun(int fd, int argc, char **argv, char *combinedopt)
{
	struct ctl_lun_req req;
	uint32_t lun_id = 0;
	int lun_id_set = 0;
	char *backend_name = NULL;
	STAILQ_HEAD(, cctl_req_option) option_list;
	int num_options = 0;
	int retval = 0, c;

	STAILQ_INIT(&option_list);

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'b':
			backend_name = strdup(optarg);
			break;
		case 'l':
			lun_id = strtoul(optarg, NULL, 0);
			lun_id_set = 1;
			break;
		case 'o': {
			struct cctl_req_option *option;
			char *tmpstr;
			char *name, *value;

			tmpstr = strdup(optarg);
			name = strsep(&tmpstr, "=");
			if (name == NULL) {
				warnx("%s: option -o takes \"name=value\""
				      "argument", __func__);
				retval = 1;
				goto bailout;
			}
			value = strsep(&tmpstr, "=");
			if (value == NULL) {
				warnx("%s: option -o takes \"name=value\""
				      "argument", __func__);
				retval = 1;
				goto bailout;
			}
			option = malloc(sizeof(*option));
			if (option == NULL) {
				warn("%s: error allocating %zd bytes",
				     __func__, sizeof(*option));
				retval = 1;
				goto bailout;
			}
			option->name = strdup(name);
			option->namelen = strlen(name) + 1;
			option->value = strdup(value);
			option->vallen = strlen(value) + 1;
			free(tmpstr);

			STAILQ_INSERT_TAIL(&option_list, option, links);
			num_options++;
			break;
		}
		default:
			break;
		}
	}

	if (backend_name == NULL)
		errx(1, "%s: backend name (-b) must be specified", __func__);

	if (lun_id_set == 0)
		errx(1, "%s: LUN id (-l) must be specified", __func__);

	bzero(&req, sizeof(req));

	strlcpy(req.backend, backend_name, sizeof(req.backend));
	req.reqtype = CTL_LUNREQ_RM;

	req.reqdata.rm.lun_id = lun_id;

	req.num_be_args = num_options;
	if (num_options > 0) {
		struct cctl_req_option *option, *next_option;
		int i;

		req.be_args = malloc(num_options * sizeof(*req.be_args));
		if (req.be_args == NULL) {
			warn("%s: error allocating %zd bytes", __func__,
			     num_options * sizeof(*req.be_args));
			retval = 1;
			goto bailout;
		}

		for (i = 0, option = STAILQ_FIRST(&option_list);
		     i < num_options; i++, option = next_option) {
			next_option = STAILQ_NEXT(option, links);

			req.be_args[i].namelen = option->namelen;
			req.be_args[i].name = strdup(option->name);
			req.be_args[i].vallen = option->vallen;
			req.be_args[i].value = strdup(option->value);
			/*
			 * XXX KDM do we want a way to specify a writeable
			 * flag of some sort?  Do we want a way to specify
			 * binary data?
			 */
			req.be_args[i].flags = CTL_BEARG_ASCII | CTL_BEARG_RD;

			STAILQ_REMOVE(&option_list, option, cctl_req_option,
				      links);
			free(option->name);
			free(option->value);
			free(option);
		}
	}

	if (ioctl(fd, CTL_LUN_REQ, &req) == -1) {
		warn("%s: error issuing CTL_LUN_REQ ioctl", __func__);
		retval = 1;
		goto bailout;
	}

	if (req.status == CTL_LUN_ERROR) {
		warnx("%s: error returned from LUN removal request:\n%s",
		      __func__, req.error_str);
		retval = 1;
		goto bailout;
	} else if (req.status != CTL_LUN_OK) {
		warnx("%s: unknown LUN removal request status %d",
		      __func__, req.status);
		retval = 1;
		goto bailout;
	}

	printf("LUN %d deleted successfully\n", lun_id);

bailout:
	return (retval);
}

static int
cctl_modify_lun(int fd, int argc, char **argv, char *combinedopt)
{
	struct ctl_lun_req req;
	uint64_t lun_size = 0;
	uint32_t lun_id = 0;
	int lun_id_set = 0, lun_size_set = 0;
	char *backend_name = NULL;
	int retval = 0, c;

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'b':
			backend_name = strdup(optarg);
			break;
		case 'l':
			lun_id = strtoul(optarg, NULL, 0);
			lun_id_set = 1;
			break;
		case 's':
			if (strcasecmp(optarg, "auto") != 0) {
				retval = expand_number(optarg, &lun_size);
				if (retval != 0) {
					warn("%s: invalid -s argument",
					    __func__);
					retval = 1;
					goto bailout;
				}
			}
			lun_size_set = 1;
			break;
		default:
			break;
		}
	}

	if (backend_name == NULL)
		errx(1, "%s: backend name (-b) must be specified", __func__);

	if (lun_id_set == 0)
		errx(1, "%s: LUN id (-l) must be specified", __func__);

	if (lun_size_set == 0)
		errx(1, "%s: size (-s) must be specified", __func__);

	bzero(&req, sizeof(req));

	strlcpy(req.backend, backend_name, sizeof(req.backend));
	req.reqtype = CTL_LUNREQ_MODIFY;

	req.reqdata.modify.lun_id = lun_id;
	req.reqdata.modify.lun_size_bytes = lun_size;

	if (ioctl(fd, CTL_LUN_REQ, &req) == -1) {
		warn("%s: error issuing CTL_LUN_REQ ioctl", __func__);
		retval = 1;
		goto bailout;
	}

	if (req.status == CTL_LUN_ERROR) {
		warnx("%s: error returned from LUN modification request:\n%s",
		      __func__, req.error_str);
		retval = 1;
		goto bailout;
	} else if (req.status != CTL_LUN_OK) {
		warnx("%s: unknown LUN modification request status %d",
		      __func__, req.status);
		retval = 1;
		goto bailout;
	}

	printf("LUN %d modified successfully\n", lun_id);

bailout:
	return (retval);
}


/*
 * Name/value pair used for per-LUN attributes.
 */
struct cctl_lun_nv {
	char *name;
	char *value;
	STAILQ_ENTRY(cctl_lun_nv) links;
};

/*
 * Backend LUN information.  
 */
struct cctl_lun {
	uint64_t lun_id;
	char *backend_type;
	uint64_t size_blocks;
	uint32_t blocksize;
	char *serial_number;
	char *device_id;
	STAILQ_HEAD(,cctl_lun_nv) attr_list;
	STAILQ_ENTRY(cctl_lun) links;
};

struct cctl_devlist_data {
	int num_luns;
	STAILQ_HEAD(,cctl_lun) lun_list;
	struct cctl_lun *cur_lun;
	int level;
	struct sbuf *cur_sb[32];
};

static void
cctl_start_element(void *user_data, const char *name, const char **attr)
{
	int i;
	struct cctl_devlist_data *devlist;
	struct cctl_lun *cur_lun;

	devlist = (struct cctl_devlist_data *)user_data;
	cur_lun = devlist->cur_lun;
	devlist->level++;
	if ((u_int)devlist->level > (sizeof(devlist->cur_sb) /
	    sizeof(devlist->cur_sb[0])))
		errx(1, "%s: too many nesting levels, %zd max", __func__,
		     sizeof(devlist->cur_sb) / sizeof(devlist->cur_sb[0]));

	devlist->cur_sb[devlist->level] = sbuf_new_auto();
	if (devlist->cur_sb[devlist->level] == NULL)
		err(1, "%s: Unable to allocate sbuf", __func__);

	if (strcmp(name, "lun") == 0) {
		if (cur_lun != NULL)
			errx(1, "%s: improper lun element nesting", __func__);

		cur_lun = calloc(1, sizeof(*cur_lun));
		if (cur_lun == NULL)
			err(1, "%s: cannot allocate %zd bytes", __func__,
			    sizeof(*cur_lun));

		devlist->num_luns++;
		devlist->cur_lun = cur_lun;

		STAILQ_INIT(&cur_lun->attr_list);
		STAILQ_INSERT_TAIL(&devlist->lun_list, cur_lun, links);

		for (i = 0; attr[i] != NULL; i += 2) {
			if (strcmp(attr[i], "id") == 0) {
				cur_lun->lun_id = strtoull(attr[i+1], NULL, 0);
			} else {
				errx(1, "%s: invalid LUN attribute %s = %s",
				     __func__, attr[i], attr[i+1]);
			}
		}
	}
}

static void
cctl_end_element(void *user_data, const char *name)
{
	struct cctl_devlist_data *devlist;
	struct cctl_lun *cur_lun;
	char *str;

	devlist = (struct cctl_devlist_data *)user_data;
	cur_lun = devlist->cur_lun;

	if ((cur_lun == NULL)
	 && (strcmp(name, "ctllunlist") != 0))
		errx(1, "%s: cur_lun == NULL! (name = %s)", __func__, name);

	if (devlist->cur_sb[devlist->level] == NULL)
		errx(1, "%s: no valid sbuf at level %d (name %s)", __func__,
		     devlist->level, name);

	sbuf_finish(devlist->cur_sb[devlist->level]);
	str = strdup(sbuf_data(devlist->cur_sb[devlist->level]));
	if (str == NULL)
		err(1, "%s can't allocate %zd bytes for string", __func__,
		    sbuf_len(devlist->cur_sb[devlist->level]));

	if (strlen(str) == 0) {
		free(str);
		str = NULL;
	}

	sbuf_delete(devlist->cur_sb[devlist->level]);
	devlist->cur_sb[devlist->level] = NULL;
	devlist->level--;

	if (strcmp(name, "backend_type") == 0) {
		cur_lun->backend_type = str;
		str = NULL;
	} else if (strcmp(name, "size") == 0) {
		cur_lun->size_blocks = strtoull(str, NULL, 0);
	} else if (strcmp(name, "blocksize") == 0) {
		cur_lun->blocksize = strtoul(str, NULL, 0);
	} else if (strcmp(name, "serial_number") == 0) {
		cur_lun->serial_number = str;
		str = NULL;
	} else if (strcmp(name, "device_id") == 0) {
		cur_lun->device_id = str;
		str = NULL;
	} else if (strcmp(name, "lun") == 0) {
		devlist->cur_lun = NULL;
	} else if (strcmp(name, "ctllunlist") == 0) {
		
	} else {
		struct cctl_lun_nv *nv;

		nv = calloc(1, sizeof(*nv));
		if (nv == NULL)
			err(1, "%s: can't allocate %zd bytes for nv pair",
			    __func__, sizeof(*nv));

		nv->name = strdup(name);
		if (nv->name == NULL)
			err(1, "%s: can't allocated %zd bytes for string",
			    __func__, strlen(name));

		nv->value = str;
		str = NULL;
		STAILQ_INSERT_TAIL(&cur_lun->attr_list, nv, links);
	}

	free(str);
}

static void
cctl_char_handler(void *user_data, const XML_Char *str, int len)
{
	struct cctl_devlist_data *devlist;

	devlist = (struct cctl_devlist_data *)user_data;

	sbuf_bcat(devlist->cur_sb[devlist->level], str, len);
}

static int
cctl_devlist(int fd, int argc, char **argv, char *combinedopt)
{
	struct ctl_lun_list list;
	struct cctl_devlist_data devlist;
	struct cctl_lun *lun;
	XML_Parser parser;
	char *lun_str;
	int lun_len;
	int dump_xml = 0;
	int retval, c;
	char *backend = NULL;
	int verbose = 0;

	retval = 0;
	lun_len = 4096;

	bzero(&devlist, sizeof(devlist));
	STAILQ_INIT(&devlist.lun_list);

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'b':
			backend = strdup(optarg);
			break;
		case 'v':
			verbose++;
			break;
		case 'x':
			dump_xml = 1;
			break;
		default:
			break;
		}
	}

retry:
	lun_str = malloc(lun_len);

	bzero(&list, sizeof(list));
	list.alloc_len = lun_len;
	list.status = CTL_LUN_LIST_NONE;
	list.lun_xml = lun_str;

	if (ioctl(fd, CTL_LUN_LIST, &list) == -1) {
		warn("%s: error issuing CTL_LUN_LIST ioctl", __func__);
		retval = 1;
		goto bailout;
	}

	if (list.status == CTL_LUN_LIST_ERROR) {
		warnx("%s: error returned from CTL_LUN_LIST ioctl:\n%s",
		      __func__, list.error_str);
	} else if (list.status == CTL_LUN_LIST_NEED_MORE_SPACE) {
		lun_len = lun_len << 1;
		goto retry;
	}

	if (dump_xml != 0) {
		printf("%s", lun_str);
		goto bailout;
	}

	parser = XML_ParserCreate(NULL);
	if (parser == NULL) {
		warn("%s: Unable to create XML parser", __func__);
		retval = 1;
		goto bailout;
	}

	XML_SetUserData(parser, &devlist);
	XML_SetElementHandler(parser, cctl_start_element, cctl_end_element);
	XML_SetCharacterDataHandler(parser, cctl_char_handler);

	retval = XML_Parse(parser, lun_str, strlen(lun_str), 1);
	XML_ParserFree(parser);
	if (retval != 1) {
		retval = 1;
		goto bailout;
	}

	printf("LUN Backend  %18s %4s %-16s %-16s\n", "Size (Blocks)", "BS",
	       "Serial Number", "Device ID");
	STAILQ_FOREACH(lun, &devlist.lun_list, links) {
		struct cctl_lun_nv *nv;

		if ((backend != NULL)
		 && (strcmp(lun->backend_type, backend) != 0))
			continue;

		printf("%3ju %-8s %18ju %4u %-16s %-16s\n",
		       (uintmax_t)lun->lun_id,
		       lun->backend_type, (uintmax_t)lun->size_blocks,
		       lun->blocksize, lun->serial_number, lun->device_id);

		if (verbose == 0)
			continue;

		STAILQ_FOREACH(nv, &lun->attr_list, links) {
			printf("      %s=%s\n", nv->name, nv->value);
		}
	}
bailout:
	free(lun_str);

	return (retval);
}

void
usage(int error)
{
	fprintf(error ? stderr : stdout,
"Usage:\n"
"Primary commands:\n"
"         ctladm tur         [dev_id][general options]\n"
"         ctladm inquiry     [dev_id][general options]\n"
"         ctladm devid       [dev_id][general options]\n"
"         ctladm reqsense    [dev_id][general options]\n"
"         ctladm reportluns  [dev_id][general options]\n"
"         ctladm read        [dev_id][general options] <-l lba> <-d len>\n"
"                            <-f file|-> <-b blocksize> [-c cdbsize][-N]\n"
"         ctladm write       [dev_id][general options] <-l lba> <-d len>\n"
"                            <-f file|-> <-b blocksize> [-c cdbsize][-N]\n"
"         ctladm readcap     [dev_id][general options] [-c cdbsize]\n"
"         ctladm modesense   [dev_id][general options] <-m page|-l> [-P pc]\n"
"                            [-d] [-S subpage] [-c cdbsize]\n"
"         ctladm prin        [dev_id][general options] <-a action>\n"
"         ctladm prout       [dev_id][general options] <-a action>\n"
"                            <-r restype] [-k key] [-s sa_key]\n"
"         ctladm rtpg        [dev_id][general options]\n"
"         ctladm start       [dev_id][general options] [-i] [-o]\n"
"         ctladm stop        [dev_id][general options] [-i] [-o]\n"
"         ctladm synccache   [dev_id][general options] [-l lba]\n"
"                            [-b blockcount] [-r] [-i] [-c cdbsize]\n"
"         ctladm create      <-b backend> [-B blocksize] [-d device_id]\n"
"                            [-l lun_id] [-o name=value] [-s size_bytes]\n"
"                            [-S serial_num] [-t dev_type]\n"
"         ctladm remove      <-b backend> <-l lun_id> [-o name=value]\n"
"         ctladm modify      <-b backend> <-l lun_id> <-s size_bytes>\n"
"         ctladm devlist     [-b][-v][-x]\n"
"         ctladm shutdown\n"
"         ctladm startup\n"
"         ctladm hardstop\n"
"         ctladm hardstart\n"
"         ctladm lunlist\n"
"         ctladm bbrread     [dev_id] <-l lba> <-d datalen>\n"
"         ctladm delay       [dev_id] <-l datamove|done> [-T oneshot|cont]\n"
"                            [-t secs]\n"
"         ctladm realsync    <on|off|query>\n"
"         ctladm setsync     [dev_id] <-i interval>\n"
"         ctladm getsync     [dev_id]\n"
"         ctladm inject      [dev_id] <-i action> <-p pattern> [-r lba,len]\n"
"                            [-s len fmt [args]] [-c] [-d delete_id]\n"
"         ctladm port        <-l | -o <on|off> | [-w wwnn][-W wwpn]>\n"
"                            [-p targ_port] [-t port_type] [-q] [-x]\n"
"         ctladm dumpooa\n"
"         ctladm dumpstructs\n"
"         ctladm help\n"
"General Options:\n"
"-I intiator_id           : defaults to 7, used to change the initiator id\n"
"-C retries               : specify the number of times to retry this command\n"
"-D devicename            : specify the device to operate on\n"
"                         : (default is %s)\n"
"read/write options:\n"
"-l lba                   : logical block address\n"
"-d len                   : read/write length, in blocks\n"
"-f file|-                : write/read data to/from file or stdout/stdin\n"
"-b blocksize             : block size, in bytes\n"
"-c cdbsize               : specify minimum cdb size: 6, 10, 12 or 16\n"
"-N                       : do not copy data to/from userland\n"
"readcapacity options:\n"
"-c cdbsize               : specify minimum cdb size: 10 or 16\n"
"modesense options:\n"
"-m page                  : specify the mode page to view\n"
"-l                       : request a list of supported pages\n"
"-P pc                    : specify the page control value: 0-3 (current,\n"
"                           changeable, default, saved, respectively)\n"
"-d                       : disable block descriptors for mode sense\n"
"-S subpage               : specify a subpage\n"
"-c cdbsize               : specify minimum cdb size: 6 or 10\n"
"persistent reserve in options:\n"
"-a action                : specify the action value: 0-2 (read key, read\n"
"                           reservation, read capabilities, respectively)\n"
"persistent reserve out options:\n"
"-a action                : specify the action value: 0-5 (register, reserve,\n"
"                           release, clear, preempt, register and ignore)\n"
"-k key                   : key value\n"
"-s sa_key                : service action value\n"
"-r restype               : specify the reservation type: 0-5(wr ex, ex ac,\n"
"                           wr ex ro, ex ac ro, wr ex ar, ex ac ar)\n"
"start/stop options:\n"
"-i                       : set the immediate bit (CTL does not support this)\n"
"-o                       : set the on/offline bit\n"
"synccache options:\n"
"-l lba                   : set the starting LBA\n"
"-b blockcount            : set the length to sync in blocks\n"
"-r                       : set the relative addressing bit\n"
"-i                       : set the immediate bit\n"
"-c cdbsize               : specify minimum cdb size: 10 or 16\n"
"create options:\n"
"-b backend               : backend name (\"block\", \"ramdisk\", etc.)\n"
"-B blocksize             : LUN blocksize in bytes (some backends)\n"
"-d device_id             : SCSI VPD page 0x83 ID\n"
"-l lun_id                : requested LUN number\n"
"-o name=value            : backend-specific options, multiple allowed\n"
"-s size_bytes            : LUN size in bytes (some backends)\n"
"-S serial_num            : SCSI VPD page 0x80 serial number\n"
"-t dev_type              : SCSI device type (0=disk, 3=processor)\n"
"remove options:\n"
"-b backend               : backend name (\"block\", \"ramdisk\", etc.)\n"
"-l lun_id                : LUN number to delete\n"
"-o name=value            : backend-specific options, multiple allowed\n"
"devlist options:\n"
"-b backend               : list devices from specified backend only\n"
"-v                       : be verbose, show backend attributes\n"
"-x                       : dump raw XML\n"
"delay options:\n"
"-l datamove|done         : delay command at datamove or done phase\n"
"-T oneshot               : delay one command, then resume normal completion\n"
"-T cont                  : delay all commands\n"
"-t secs                  : number of seconds to delay\n"
"inject options:\n"
"-i error_action          : action to perform\n"
"-p pattern               : command pattern to look for\n"
"-r lba,len               : LBA range for pattern\n"
"-s len fmt [args]        : sense data for custom sense action\n"
"-c                       : continuous operation\n"
"-d delete_id             : error id to delete\n"
"port options:\n"
"-l                       : list frontend ports\n"
"-o on|off                : turn frontend ports on or off\n"
"-w wwnn                  : set WWNN for one frontend\n"
"-W wwpn                  : set WWPN for one frontend\n"
"-t port_type             : specify fc, scsi, ioctl, internal frontend type\n"
"-p targ_port             : specify target port number\n"
"-q                       : omit header in list output\n"
"-x                       : output port list in XML format\n"
"bbrread options:\n"
"-l lba                   : starting LBA\n"
"-d datalen               : length, in bytes, to read\n",
CTL_DEFAULT_DEV);
}

int
main(int argc, char **argv)
{
	int c;
	ctladm_cmdfunction command;
	ctladm_cmdargs cmdargs;
	ctladm_optret optreturn;
	char *device;
	const char *mainopt = "C:D:I:";
	const char *subopt = NULL;
	char combinedopt[256];
	int target, lun;
	int optstart = 2;
	int retval, fd;
	int retries;
	int initid;

	retval = 0;
	cmdargs = CTLADM_ARG_NONE;
	command = CTLADM_CMD_HELP;
	device = NULL;
	fd = -1;
	retries = 0;
	target = 0;
	lun = 0;
	initid = 7;

	if (argc < 2) {
		usage(1);
		retval = 1;
		goto bailout;
	}

	/*
	 * Get the base option.
	 */
	optreturn = getoption(option_table,argv[1], &command, &cmdargs,&subopt);

	if (optreturn == CC_OR_AMBIGUOUS) {
		warnx("ambiguous option %s", argv[1]);
		usage(0);
		exit(1);
	} else if (optreturn == CC_OR_NOT_FOUND) {
		warnx("option %s not found", argv[1]);
		usage(0);
		exit(1);
	}

	if (cmdargs & CTLADM_ARG_NEED_TL) {
	 	if ((argc < 3)
		 || (!isdigit(argv[2][0]))) {
			warnx("option %s requires a target:lun argument",
			      argv[1]);
			usage(0);
			exit(1);
		}
		retval = cctl_parse_tl(argv[2], &target, &lun);
		if (retval != 0)
			errx(1, "invalid target:lun argument %s", argv[2]);

		cmdargs |= CTLADM_ARG_TARG_LUN;
		optstart++;
	}

	/*
	 * Ahh, getopt(3) is a pain.
	 *
	 * This is a gross hack.  There really aren't many other good
	 * options (excuse the pun) for parsing options in a situation like
	 * this.  getopt is kinda braindead, so you end up having to run
	 * through the options twice, and give each invocation of getopt
	 * the option string for the other invocation.
	 *
	 * You would think that you could just have two groups of options.
	 * The first group would get parsed by the first invocation of
	 * getopt, and the second group would get parsed by the second
	 * invocation of getopt.  It doesn't quite work out that way.  When
	 * the first invocation of getopt finishes, it leaves optind pointing
	 * to the argument _after_ the first argument in the second group.
	 * So when the second invocation of getopt comes around, it doesn't
	 * recognize the first argument it gets and then bails out.
	 *
	 * A nice alternative would be to have a flag for getopt that says
	 * "just keep parsing arguments even when you encounter an unknown
	 * argument", but there isn't one.  So there's no real clean way to
	 * easily parse two sets of arguments without having one invocation
	 * of getopt know about the other.
	 *
	 * Without this hack, the first invocation of getopt would work as
	 * long as the generic arguments are first, but the second invocation
	 * (in the subfunction) would fail in one of two ways.  In the case
	 * where you don't set optreset, it would fail because optind may be
	 * pointing to the argument after the one it should be pointing at.
	 * In the case where you do set optreset, and reset optind, it would
	 * fail because getopt would run into the first set of options, which
	 * it doesn't understand.
	 *
	 * All of this would "sort of" work if you could somehow figure out
	 * whether optind had been incremented one option too far.  The
	 * mechanics of that, however, are more daunting than just giving
	 * both invocations all of the expect options for either invocation.
	 *
	 * Needless to say, I wouldn't mind if someone invented a better
	 * (non-GPL!) command line parsing interface than getopt.  I
	 * wouldn't mind if someone added more knobs to getopt to make it
	 * work better.  Who knows, I may talk myself into doing it someday,
	 * if the standards weenies let me.  As it is, it just leads to
	 * hackery like this and causes people to avoid it in some cases.
	 *
	 * KDM, September 8th, 1998
	 */
	if (subopt != NULL)
		sprintf(combinedopt, "%s%s", mainopt, subopt);
	else
		sprintf(combinedopt, "%s", mainopt);

	/*
	 * Start getopt processing at argv[2/3], since we've already
	 * accepted argv[1..2] as the command name, and as a possible
	 * device name.
	 */
	optind = optstart;

	/*
	 * Now we run through the argument list looking for generic
	 * options, and ignoring options that possibly belong to
	 * subfunctions.
	 */
	while ((c = getopt(argc, argv, combinedopt))!= -1){
		switch (c) {
		case 'C':
			cmdargs |= CTLADM_ARG_RETRIES;
			retries = strtol(optarg, NULL, 0);
			break;
		case 'D':
			device = strdup(optarg);
			cmdargs |= CTLADM_ARG_DEVICE;
			break;
		case 'I':
			cmdargs |= CTLADM_ARG_INITIATOR;
			initid = strtol(optarg, NULL, 0);
			break;
		default:
			break;
		}
	}

	if ((cmdargs & CTLADM_ARG_INITIATOR) == 0)
		initid = 7;

	optind = optstart;
	optreset = 1;

	/*
	 * Default to opening the CTL device for now.
	 */
	if (((cmdargs & CTLADM_ARG_DEVICE) == 0)
	 && (command != CTLADM_CMD_HELP)) {
		device = strdup(CTL_DEFAULT_DEV);
		cmdargs |= CTLADM_ARG_DEVICE;
	}

	if ((cmdargs & CTLADM_ARG_DEVICE)
	 && (command != CTLADM_CMD_HELP)) {
		fd = open(device, O_RDWR);
		if (fd == -1) {
			fprintf(stderr, "%s: error opening %s: %s\n",
				argv[0], device, strerror(errno));
			retval = 1;
			goto bailout;
		}
	} else if ((command != CTLADM_CMD_HELP)
		&& ((cmdargs & CTLADM_ARG_DEVICE) == 0)) {
		fprintf(stderr, "%s: you must specify a device with the "
			"--device argument for this command\n", argv[0]);
		command = CTLADM_CMD_HELP;
		retval = 1;
	}

	switch (command) {
	case CTLADM_CMD_TUR:
		retval = cctl_tur(fd, target, lun, initid, retries);
		break;
	case CTLADM_CMD_INQUIRY:
		retval = cctl_inquiry(fd, target, lun, initid, retries);
		break;
	case CTLADM_CMD_REQ_SENSE:
		retval = cctl_req_sense(fd, target, lun, initid, retries);
		break;
	case CTLADM_CMD_REPORT_LUNS:
		retval = cctl_report_luns(fd, target, lun, initid, retries);
		break;
	case CTLADM_CMD_CREATE:
		retval = cctl_create_lun(fd, argc, argv, combinedopt);
		break;
	case CTLADM_CMD_RM:
		retval = cctl_rm_lun(fd, argc, argv, combinedopt);
		break;
	case CTLADM_CMD_DEVLIST:
		retval = cctl_devlist(fd, argc, argv, combinedopt);
		break;
	case CTLADM_CMD_READ:
	case CTLADM_CMD_WRITE:
		retval = cctl_read_write(fd, target, lun, initid, retries,
					 argc, argv, combinedopt, command);
		break;
	case CTLADM_CMD_PORT:
		retval = cctl_port(fd, argc, argv, combinedopt);
		break;
	case CTLADM_CMD_READCAPACITY:
		retval = cctl_read_capacity(fd, target, lun, initid, retries,
					    argc, argv, combinedopt);
		break;
	case CTLADM_CMD_MODESENSE:
		retval = cctl_mode_sense(fd, target, lun, initid, retries,
					 argc, argv, combinedopt);
		break;
	case CTLADM_CMD_START:
	case CTLADM_CMD_STOP:
		retval = cctl_start_stop(fd, target, lun, initid, retries,
					 (command == CTLADM_CMD_START) ? 1 : 0,
					 argc, argv, combinedopt);
		break;
	case CTLADM_CMD_SYNC_CACHE:
		retval = cctl_sync_cache(fd, target, lun, initid, retries,
					 argc, argv, combinedopt);
		break;
	case CTLADM_CMD_SHUTDOWN:
	case CTLADM_CMD_STARTUP:
		retval = cctl_startup_shutdown(fd, target, lun, initid,
					       command);
		break;
	case CTLADM_CMD_HARDSTOP:
	case CTLADM_CMD_HARDSTART:
		retval = cctl_hardstopstart(fd, command);
		break;
	case CTLADM_CMD_BBRREAD:
		retval = cctl_bbrread(fd, target, lun, initid, argc, argv,
				      combinedopt);
		break;
	case CTLADM_CMD_LUNLIST:
		retval = cctl_lunlist(fd);
		break;
	case CTLADM_CMD_DELAY:
		retval = cctl_delay(fd, target, lun, argc, argv, combinedopt);
		break;
	case CTLADM_CMD_REALSYNC:
		retval = cctl_realsync(fd, argc, argv);
		break;
	case CTLADM_CMD_SETSYNC:
	case CTLADM_CMD_GETSYNC:
		retval = cctl_getsetsync(fd, target, lun, command,
					 argc, argv, combinedopt);
		break;
	case CTLADM_CMD_ERR_INJECT:
		retval = cctl_error_inject(fd, target, lun, argc, argv,
					   combinedopt);
		break;
	case CTLADM_CMD_DUMPOOA:
		retval = cctl_dump_ooa(fd, argc, argv);
		break;
	case CTLADM_CMD_DUMPSTRUCTS:
		retval = cctl_dump_structs(fd, cmdargs);
		break;
	case CTLADM_CMD_PRES_IN:
		retval = cctl_persistent_reserve_in(fd, target, lun, initid, 
		                                    argc, argv, combinedopt,
						    retries);
		break;
	case CTLADM_CMD_PRES_OUT:
		retval = cctl_persistent_reserve_out(fd, target, lun, initid, 
						     argc, argv, combinedopt,
						     retries);
		break;
	case CTLADM_CMD_INQ_VPD_DEVID:
	        retval = cctl_inquiry_vpd_devid(fd, target, lun, initid);
		break;
	case CTLADM_CMD_RTPG:
	        retval = cctl_report_target_port_group(fd, target, lun, initid);
		break;
	case CTLADM_CMD_MODIFY:
	        retval = cctl_modify_lun(fd, argc, argv, combinedopt);
		break;
	case CTLADM_CMD_HELP:
	default:
		usage(retval);
		break;
	}
bailout:

	if (fd != -1)
		close(fd);

	exit (retval);
}

/*
 * vim: ts=8
 */
