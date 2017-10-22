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
 * $FreeBSD: release/10.0.0/usr.sbin/ctld/kernel.c 256189 2013-10-09 12:17:40Z trasz $
 */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/linker.h>
#include <sys/queue.h>
#include <sys/callout.h>
#include <sys/sbuf.h>
#include <sys/capability.h>
#include <assert.h>
#include <bsdxml.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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

#ifdef ICL_KERNEL_PROXY
#include <netdb.h>
#endif

#include "ctld.h"

static int	ctl_fd = 0;

void
kernel_init(void)
{
	int retval, saved_errno;

	ctl_fd = open(CTL_DEFAULT_DEV, O_RDWR);
	if (ctl_fd < 0 && errno == ENOENT) {
		saved_errno = errno;
		retval = kldload("ctl");
		if (retval != -1)
			ctl_fd = open(CTL_DEFAULT_DEV, O_RDWR);
		else
			errno = saved_errno;
	}
	if (ctl_fd < 0)
		log_err(1, "failed to open %s", CTL_DEFAULT_DEV);
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
	char *cfiscsi_target;
	char *cfiscsi_target_alias;
	int cfiscsi_lun;
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
	if ((u_int)devlist->level >= (sizeof(devlist->cur_sb) /
	    sizeof(devlist->cur_sb[0])))
		log_errx(1, "%s: too many nesting levels, %zd max", __func__,
		     sizeof(devlist->cur_sb) / sizeof(devlist->cur_sb[0]));

	devlist->cur_sb[devlist->level] = sbuf_new_auto();
	if (devlist->cur_sb[devlist->level] == NULL)
		log_err(1, "%s: unable to allocate sbuf", __func__);

	if (strcmp(name, "lun") == 0) {
		if (cur_lun != NULL)
			log_errx(1, "%s: improper lun element nesting",
			    __func__);

		cur_lun = calloc(1, sizeof(*cur_lun));
		if (cur_lun == NULL)
			log_err(1, "%s: cannot allocate %zd bytes", __func__,
			    sizeof(*cur_lun));

		devlist->num_luns++;
		devlist->cur_lun = cur_lun;

		STAILQ_INIT(&cur_lun->attr_list);
		STAILQ_INSERT_TAIL(&devlist->lun_list, cur_lun, links);

		for (i = 0; attr[i] != NULL; i += 2) {
			if (strcmp(attr[i], "id") == 0) {
				cur_lun->lun_id = strtoull(attr[i+1], NULL, 0);
			} else {
				log_errx(1, "%s: invalid LUN attribute %s = %s",
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
		log_errx(1, "%s: cur_lun == NULL! (name = %s)", __func__, name);

	if (devlist->cur_sb[devlist->level] == NULL)
		log_errx(1, "%s: no valid sbuf at level %d (name %s)", __func__,
		     devlist->level, name);

	sbuf_finish(devlist->cur_sb[devlist->level]);
	str = checked_strdup(sbuf_data(devlist->cur_sb[devlist->level]));

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
	} else if (strcmp(name, "cfiscsi_target") == 0) {
		cur_lun->cfiscsi_target = str;
		str = NULL;
	} else if (strcmp(name, "cfiscsi_target_alias") == 0) {
		cur_lun->cfiscsi_target_alias = str;
		str = NULL;
	} else if (strcmp(name, "cfiscsi_lun") == 0) {
		cur_lun->cfiscsi_lun = strtoul(str, NULL, 0);
	} else if (strcmp(name, "lun") == 0) {
		devlist->cur_lun = NULL;
	} else if (strcmp(name, "ctllunlist") == 0) {
		
	} else {
		struct cctl_lun_nv *nv;

		nv = calloc(1, sizeof(*nv));
		if (nv == NULL)
			log_err(1, "%s: can't allocate %zd bytes for nv pair",
			    __func__, sizeof(*nv));

		nv->name = checked_strdup(name);

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

struct conf *
conf_new_from_kernel(void)
{
	struct conf *conf = NULL;
	struct target *targ;
	struct lun *cl;
	struct lun_option *lo;
	struct ctl_lun_list list;
	struct cctl_devlist_data devlist;
	struct cctl_lun *lun;
	XML_Parser parser;
	char *lun_str = NULL;
	int lun_len;
	int retval;

	lun_len = 4096;

	bzero(&devlist, sizeof(devlist));
	STAILQ_INIT(&devlist.lun_list);

	log_debugx("obtaining previously configured CTL luns from the kernel");

retry:
	lun_str = realloc(lun_str, lun_len);
	if (lun_str == NULL)
		log_err(1, "realloc");

	bzero(&list, sizeof(list));
	list.alloc_len = lun_len;
	list.status = CTL_LUN_LIST_NONE;
	list.lun_xml = lun_str;

	if (ioctl(ctl_fd, CTL_LUN_LIST, &list) == -1) {
		log_warn("error issuing CTL_LUN_LIST ioctl");
		free(lun_str);
		return (NULL);
	}

	if (list.status == CTL_LUN_LIST_ERROR) {
		log_warnx("error returned from CTL_LUN_LIST ioctl: %s",
		    list.error_str);
		free(lun_str);
		return (NULL);
	}

	if (list.status == CTL_LUN_LIST_NEED_MORE_SPACE) {
		lun_len = lun_len << 1;
		goto retry;
	}

	parser = XML_ParserCreate(NULL);
	if (parser == NULL) {
		log_warnx("unable to create XML parser");
		free(lun_str);
		return (NULL);
	}

	XML_SetUserData(parser, &devlist);
	XML_SetElementHandler(parser, cctl_start_element, cctl_end_element);
	XML_SetCharacterDataHandler(parser, cctl_char_handler);

	retval = XML_Parse(parser, lun_str, strlen(lun_str), 1);
	XML_ParserFree(parser);
	free(lun_str);
	if (retval != 1) {
		log_warnx("XML_Parse failed");
		return (NULL);
	}

	conf = conf_new();

	STAILQ_FOREACH(lun, &devlist.lun_list, links) {
		struct cctl_lun_nv *nv;

		if (lun->cfiscsi_target == NULL) {
			log_debugx("CTL lun %ju wasn't managed by ctld; "
			    "ignoring", (uintmax_t)lun->lun_id);
			continue;
		}

		targ = target_find(conf, lun->cfiscsi_target);
		if (targ == NULL) {
#if 0
			log_debugx("found new kernel target %s for CTL lun %ld",
			    lun->cfiscsi_target, lun->lun_id);
#endif
			targ = target_new(conf, lun->cfiscsi_target);
			if (targ == NULL) {
				log_warnx("target_new failed");
				continue;
			}
		}

		cl = lun_find(targ, lun->cfiscsi_lun);
		if (cl != NULL) {
			log_warnx("found CTL lun %ju, backing lun %d, target "
			    "%s, also backed by CTL lun %d; ignoring",
			    (uintmax_t) lun->lun_id, cl->l_lun,
			    cl->l_target->t_iqn, cl->l_ctl_lun);
			continue;
		}

		log_debugx("found CTL lun %ju, backing lun %d, target %s",
		    (uintmax_t)lun->lun_id, lun->cfiscsi_lun, lun->cfiscsi_target);

		cl = lun_new(targ, lun->cfiscsi_lun);
		if (cl == NULL) {
			log_warnx("lun_new failed");
			continue;
		}
		lun_set_backend(cl, lun->backend_type);
		lun_set_blocksize(cl, lun->blocksize);
		lun_set_device_id(cl, lun->device_id);
		lun_set_serial(cl, lun->serial_number);
		lun_set_size(cl, lun->size_blocks * cl->l_blocksize);
		lun_set_ctl_lun(cl, lun->lun_id);

		STAILQ_FOREACH(nv, &lun->attr_list, links) {
			if (strcmp(nv->name, "file") == 0 ||
			    strcmp(nv->name, "dev") == 0) {
				lun_set_path(cl, nv->value);
				continue;
			}
			lo = lun_option_new(cl, nv->name, nv->value);
			if (lo == NULL)
				log_warnx("unable to add CTL lun option %s "
				    "for CTL lun %ju for lun %d, target %s",
				    nv->name, (uintmax_t) lun->lun_id,
				    cl->l_lun, cl->l_target->t_iqn);
		}
	}

	return (conf);
}

int
kernel_lun_add(struct lun *lun)
{
	struct lun_option *lo;
	struct ctl_lun_req req;
	char *tmp;
	int error, i, num_options;

	bzero(&req, sizeof(req));

	strlcpy(req.backend, lun->l_backend, sizeof(req.backend));
	req.reqtype = CTL_LUNREQ_CREATE;

	req.reqdata.create.blocksize_bytes = lun->l_blocksize;

	if (lun->l_size != 0)
		req.reqdata.create.lun_size_bytes = lun->l_size;

	req.reqdata.create.flags |= CTL_LUN_FLAG_DEV_TYPE;
	req.reqdata.create.device_type = T_DIRECT;

	if (lun->l_serial != NULL) {
		strlcpy(req.reqdata.create.serial_num, lun->l_serial,
			sizeof(req.reqdata.create.serial_num));
		req.reqdata.create.flags |= CTL_LUN_FLAG_SERIAL_NUM;
	}

	if (lun->l_device_id != NULL) {
		strlcpy(req.reqdata.create.device_id, lun->l_device_id,
			sizeof(req.reqdata.create.device_id));
		req.reqdata.create.flags |= CTL_LUN_FLAG_DEVID;
	}

	if (lun->l_path != NULL) {
		lo = lun_option_find(lun, "file");
		if (lo != NULL) {
			lun_option_set(lo, lun->l_path);
		} else {
			lo = lun_option_new(lun, "file", lun->l_path);
			assert(lo != NULL);
		}
	}

	lo = lun_option_find(lun, "cfiscsi_target");
	if (lo != NULL) {
		lun_option_set(lo, lun->l_target->t_iqn);
	} else {
		lo = lun_option_new(lun, "cfiscsi_target",
		    lun->l_target->t_iqn);
		assert(lo != NULL);
	}

	if (lun->l_target->t_alias != NULL) {
		lo = lun_option_find(lun, "cfiscsi_target_alias");
		if (lo != NULL) {
			lun_option_set(lo, lun->l_target->t_alias);
		} else {
			lo = lun_option_new(lun, "cfiscsi_target_alias",
			    lun->l_target->t_alias);
			assert(lo != NULL);
		}
	}

	asprintf(&tmp, "%d", lun->l_lun);
	if (tmp == NULL)
		log_errx(1, "asprintf");
	lo = lun_option_find(lun, "cfiscsi_lun");
	if (lo != NULL) {
		lun_option_set(lo, tmp);
		free(tmp);
	} else {
		lo = lun_option_new(lun, "cfiscsi_lun", tmp);
		free(tmp);
		assert(lo != NULL);
	}

	num_options = 0;
	TAILQ_FOREACH(lo, &lun->l_options, lo_next)
		num_options++;

	req.num_be_args = num_options;
	if (num_options > 0) {
		req.be_args = malloc(num_options * sizeof(*req.be_args));
		if (req.be_args == NULL) {
			log_warn("error allocating %zd bytes",
			    num_options * sizeof(*req.be_args));
			return (1);
		}

		i = 0;
		TAILQ_FOREACH(lo, &lun->l_options, lo_next) {
			 /*
			  * +1 for the terminating '\0'
			  */
			req.be_args[i].namelen = strlen(lo->lo_name) + 1;
			req.be_args[i].name = lo->lo_name;
			req.be_args[i].vallen = strlen(lo->lo_value) + 1;
			req.be_args[i].value = lo->lo_value;
			req.be_args[i].flags = CTL_BEARG_ASCII | CTL_BEARG_RD;
			i++;
		}
		assert(i == num_options);
	}

	error = ioctl(ctl_fd, CTL_LUN_REQ, &req);
	free(req.be_args);
	if (error != 0) {
		log_warn("error issuing CTL_LUN_REQ ioctl");
		return (1);
	}

	if (req.status == CTL_LUN_ERROR) {
		log_warnx("error returned from LUN creation request: %s",
		    req.error_str);
		return (1);
	}

	if (req.status != CTL_LUN_OK) {
		log_warnx("unknown LUN creation request status %d",
		    req.status);
		return (1);
	}

	lun_set_ctl_lun(lun, req.reqdata.create.req_lun_id);

	return (0);
}

int
kernel_lun_resize(struct lun *lun)
{
	struct ctl_lun_req req;

	bzero(&req, sizeof(req));

	strlcpy(req.backend, lun->l_backend, sizeof(req.backend));
	req.reqtype = CTL_LUNREQ_MODIFY;

	req.reqdata.modify.lun_id = lun->l_ctl_lun;
	req.reqdata.modify.lun_size_bytes = lun->l_size;

	if (ioctl(ctl_fd, CTL_LUN_REQ, &req) == -1) {
		log_warn("error issuing CTL_LUN_REQ ioctl");
		return (1);
	}

	if (req.status == CTL_LUN_ERROR) {
		log_warnx("error returned from LUN modification request: %s",
		    req.error_str);
		return (1);
	}

	if (req.status != CTL_LUN_OK) {
		log_warnx("unknown LUN modification request status %d",
		    req.status);
		return (1);
	}

	return (0);
}

int
kernel_lun_remove(struct lun *lun)
{
	struct ctl_lun_req req;

	bzero(&req, sizeof(req));

	strlcpy(req.backend, lun->l_backend, sizeof(req.backend));
	req.reqtype = CTL_LUNREQ_RM;

	req.reqdata.rm.lun_id = lun->l_ctl_lun;

	if (ioctl(ctl_fd, CTL_LUN_REQ, &req) == -1) {
		log_warn("error issuing CTL_LUN_REQ ioctl");
		return (1);
	}

	if (req.status == CTL_LUN_ERROR) {
		log_warnx("error returned from LUN removal request: %s",
		    req.error_str);
		return (1);
	}
	
	if (req.status != CTL_LUN_OK) {
		log_warnx("unknown LUN removal request status %d", req.status);
		return (1);
	}

	return (0);
}

void
kernel_handoff(struct connection *conn)
{
	struct ctl_iscsi req;

	bzero(&req, sizeof(req));

	req.type = CTL_ISCSI_HANDOFF;
	strlcpy(req.data.handoff.initiator_name,
	    conn->conn_initiator_name, sizeof(req.data.handoff.initiator_name));
	strlcpy(req.data.handoff.initiator_addr,
	    conn->conn_initiator_addr, sizeof(req.data.handoff.initiator_addr));
	if (conn->conn_initiator_alias != NULL) {
		strlcpy(req.data.handoff.initiator_alias,
		    conn->conn_initiator_alias, sizeof(req.data.handoff.initiator_alias));
	}
	strlcpy(req.data.handoff.target_name,
	    conn->conn_target->t_iqn, sizeof(req.data.handoff.target_name));
	req.data.handoff.socket = conn->conn_socket;
	req.data.handoff.portal_group_tag =
	    conn->conn_portal->p_portal_group->pg_tag;
	if (conn->conn_header_digest == CONN_DIGEST_CRC32C)
		req.data.handoff.header_digest = CTL_ISCSI_DIGEST_CRC32C;
	if (conn->conn_data_digest == CONN_DIGEST_CRC32C)
		req.data.handoff.data_digest = CTL_ISCSI_DIGEST_CRC32C;
	req.data.handoff.cmdsn = conn->conn_cmdsn;
	req.data.handoff.statsn = conn->conn_statsn;
	req.data.handoff.max_recv_data_segment_length =
	    conn->conn_max_data_segment_length;
	req.data.handoff.max_burst_length = conn->conn_max_burst_length;
	req.data.handoff.immediate_data = conn->conn_immediate_data;

	if (ioctl(ctl_fd, CTL_ISCSI, &req) == -1)
		log_err(1, "error issuing CTL_ISCSI ioctl; "
		    "dropping connection");

	if (req.status != CTL_ISCSI_OK)
		log_errx(1, "error returned from CTL iSCSI handoff request: "
		    "%s; dropping connection", req.error_str);
}

int
kernel_port_on(void)
{
	struct ctl_port_entry entry;
	int error;

	bzero(&entry, sizeof(entry));

	entry.port_type = CTL_PORT_ISCSI;
	entry.targ_port = -1;

	error = ioctl(ctl_fd, CTL_ENABLE_PORT, &entry);
	if (error != 0) {
		log_warn("CTL_ENABLE_PORT ioctl failed");
		return (-1);
	}

	return (0);
}

int
kernel_port_off(void)
{
	struct ctl_port_entry entry;
	int error;

	bzero(&entry, sizeof(entry));

	entry.port_type = CTL_PORT_ISCSI;
	entry.targ_port = -1;

	error = ioctl(ctl_fd, CTL_DISABLE_PORT, &entry);
	if (error != 0) {
		log_warn("CTL_DISABLE_PORT ioctl failed");
		return (-1);
	}

	return (0);
}

#ifdef ICL_KERNEL_PROXY
void
kernel_listen(struct addrinfo *ai, bool iser)
{
	struct ctl_iscsi req;

	bzero(&req, sizeof(req));

	req.type = CTL_ISCSI_LISTEN;
	req.data.listen.iser = iser;
	req.data.listen.domain = ai->ai_family;
	req.data.listen.socktype = ai->ai_socktype;
	req.data.listen.protocol = ai->ai_protocol;
	req.data.listen.addr = ai->ai_addr;
	req.data.listen.addrlen = ai->ai_addrlen;

	if (ioctl(ctl_fd, CTL_ISCSI, &req) == -1)
		log_warn("error issuing CTL_ISCSI_LISTEN ioctl");
}

int
kernel_accept(void)
{
	struct ctl_iscsi req;

	bzero(&req, sizeof(req));

	req.type = CTL_ISCSI_ACCEPT;

	if (ioctl(ctl_fd, CTL_ISCSI, &req) == -1) {
		log_warn("error issuing CTL_ISCSI_LISTEN ioctl");
		return (0);
	}

	return (req.data.accept.connection_id);
}

void
kernel_send(struct pdu *pdu)
{
	struct ctl_iscsi req;

	bzero(&req, sizeof(req));

	req.type = CTL_ISCSI_SEND;
	req.data.send.connection_id = pdu->pdu_connection->conn_socket;
	req.data.send.bhs = pdu->pdu_bhs;
	req.data.send.data_segment_len = pdu->pdu_data_len;
	req.data.send.data_segment = pdu->pdu_data;

	if (ioctl(ctl_fd, CTL_ISCSI, &req) == -1)
		log_err(1, "error issuing CTL_ISCSI ioctl; "
		    "dropping connection");

	if (req.status != CTL_ISCSI_OK)
		log_errx(1, "error returned from CTL iSCSI send: "
		    "%s; dropping connection", req.error_str);
}

void
kernel_receive(struct pdu *pdu)
{
	struct ctl_iscsi req;

	pdu->pdu_data = malloc(MAX_DATA_SEGMENT_LENGTH);
	if (pdu->pdu_data == NULL)
		log_err(1, "malloc");

	bzero(&req, sizeof(req));

	req.type = CTL_ISCSI_RECEIVE;
	req.data.receive.connection_id = pdu->pdu_connection->conn_socket;
	req.data.receive.bhs = pdu->pdu_bhs;
	req.data.receive.data_segment_len = MAX_DATA_SEGMENT_LENGTH;
	req.data.receive.data_segment = pdu->pdu_data;

	if (ioctl(ctl_fd, CTL_ISCSI, &req) == -1)
		log_err(1, "error issuing CTL_ISCSI ioctl; "
		    "dropping connection");

	if (req.status != CTL_ISCSI_OK)
		log_errx(1, "error returned from CTL iSCSI receive: "
		    "%s; dropping connection", req.error_str);

}

#endif /* ICL_KERNEL_PROXY */

/*
 * XXX: I CANT INTO LATIN
 */
void
kernel_capsicate(void)
{
	int error;
	cap_rights_t rights;
	const unsigned long cmds[] = { CTL_ISCSI };

	cap_rights_init(&rights, CAP_IOCTL);
	error = cap_rights_limit(ctl_fd, &rights);
	if (error != 0 && errno != ENOSYS)
		log_err(1, "cap_rights_limit");

	error = cap_ioctls_limit(ctl_fd, cmds,
	    sizeof(cmds) / sizeof(cmds[0]));
	if (error != 0 && errno != ENOSYS)
		log_err(1, "cap_ioctls_limit");

	error = cap_enter();
	if (error != 0 && errno != ENOSYS)
		log_err(1, "cap_enter");

	if (cap_sandboxed())
		log_debugx("Capsicum capability mode enabled");
	else
		log_warnx("Capsicum capability mode not supported");
}

