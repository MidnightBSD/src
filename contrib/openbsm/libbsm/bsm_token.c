/*
 * Copyright (c) 2004 Apple Computer, Inc.
 * Copyright (c) 2005 SPARTA, Inc.
 * All rights reserved.
 *
 * This code was developed in part by Robert N. M. Watson, Senior Principal
 * Scientist, SPARTA, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $P4: //depot/projects/trustedbsd/openbsm/libbsm/bsm_token.c#63 $
 */

#include <sys/types.h>

#include <config/config.h>
#ifdef HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#else /* !HAVE_SYS_ENDIAN_H */
#ifdef HAVE_MACHINE_ENDIAN_H
#include <machine/endian.h>
#else /* !HAVE_MACHINE_ENDIAN_H */
#ifdef HAVE_ENDIAN_H
#include <endian.h>
#else /* !HAVE_ENDIAN_H */
#error "No supported endian.h"
#endif /* !HAVE_ENDIAN_H */
#endif /* !HAVE_MACHINE_ENDIAN_H */
#include <compat/endian.h>
#endif /* !HAVE_SYS_ENDIAN_H */
#ifdef HAVE_FULL_QUEUE_H
#include <sys/queue.h>
#else /* !HAVE_FULL_QUEUE_H */
#include <compat/queue.h>
#endif /* !HAVE_FULL_QUEUE_H */

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>

#include <sys/ipc.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <bsm/audit_internal.h>
#include <bsm/libbsm.h>

#define	GET_TOKEN_AREA(t, dptr, length) do {				\
	(t) = malloc(sizeof(token_t));					\
	if ((t) != NULL) {						\
		(t)->len = (length);					\
		(dptr) = (t->t_data) = malloc((length) * sizeof(u_char)); \
		if ((dptr) == NULL) {					\
			free(t);					\
			(t) = NULL;					\
		} else							\
			memset((dptr), 0, (length));			\
	} else								\
		(dptr) = NULL;						\
	assert(t == NULL || dptr != NULL);				\
} while (0)

/*
 * token ID                1 byte
 * argument #              1 byte
 * argument value          4 bytes/8 bytes (32-bit/64-bit value)
 * text length             2 bytes
 * text                    N bytes + 1 terminating NULL byte
 */
token_t *
au_to_arg32(char n, char *text, u_int32_t v)
{
	token_t *t;
	u_char *dptr = NULL;
	u_int16_t textlen;

	textlen = strlen(text);
	textlen += 1;

	GET_TOKEN_AREA(t, dptr, 2 * sizeof(u_char) + sizeof(u_int32_t) +
	    sizeof(u_int16_t) + textlen);
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_ARG32);
	ADD_U_CHAR(dptr, n);
	ADD_U_INT32(dptr, v);
	ADD_U_INT16(dptr, textlen);
	ADD_STRING(dptr, text, textlen);

	return (t);

}

token_t *
au_to_arg64(char n, char *text, u_int64_t v)
{
	token_t *t;
	u_char *dptr = NULL;
	u_int16_t textlen;

	textlen = strlen(text);
	textlen += 1;

	GET_TOKEN_AREA(t, dptr, 2 * sizeof(u_char) + sizeof(u_int64_t) +
	    sizeof(u_int16_t) + textlen);
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_ARG64);
	ADD_U_CHAR(dptr, n);
	ADD_U_INT64(dptr, v);
	ADD_U_INT16(dptr, textlen);
	ADD_STRING(dptr, text, textlen);

	return (t);

}

token_t *
au_to_arg(char n, char *text, u_int32_t v)
{

	return (au_to_arg32(n, text, v));
}

#if defined(_KERNEL) || defined(KERNEL)
/*
 * token ID                1 byte
 * file access mode        4 bytes
 * owner user ID           4 bytes
 * owner group ID          4 bytes
 * file system ID          4 bytes
 * node ID                 8 bytes
 * device                  4 bytes/8 bytes (32-bit/64-bit)
 */
token_t *
au_to_attr32(struct vnode_au_info *vni)
{
	token_t *t;
	u_char *dptr = NULL;
	u_int16_t pad0_16 = 0;
	u_int16_t pad0_32 = 0;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + 2 * sizeof(u_int16_t) +
	    3 * sizeof(u_int32_t) + sizeof(u_int64_t) + sizeof(u_int32_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_ATTR32);

	/*
	 * Darwin defines the size for the file mode
	 * as 2 bytes; BSM defines 4 so pad with 0
	 */
	ADD_U_INT16(dptr, pad0_16);
	ADD_U_INT16(dptr, vni->vn_mode);

	ADD_U_INT32(dptr, vni->vn_uid);
	ADD_U_INT32(dptr, vni->vn_gid);
	ADD_U_INT32(dptr, vni->vn_fsid);

	/*
	 * Some systems use 32-bit file ID's, other's use 64-bit file IDs.
	 * Attempt to handle both, and let the compiler sort it out.  If we
	 * could pick this out at compile-time, it would be better, so as to
	 * avoid the else case below.
	 */
	if (sizeof(vni->vn_fileid) == sizeof(uint32_t)) {
		ADD_U_INT32(dptr, pad0_32);
		ADD_U_INT32(dptr, vni->vn_fileid);
	} else if (sizeof(vni->vn_fileid) == sizeof(uint64_t))
		ADD_U_INT64(dptr, vni->vn_fileid);
	else
		ADD_U_INT64(dptr, 0LL);

	ADD_U_INT32(dptr, vni->vn_dev);

	return (t);
}

token_t *
au_to_attr64(struct vnode_au_info *vni)
{
	token_t *t;
	u_char *dptr = NULL;
	u_int16_t pad0_16 = 0;
	u_int16_t pad0_32 = 0;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + 2 * sizeof(u_int16_t) +
	    3 * sizeof(u_int32_t) + sizeof(u_int64_t) * 2);
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_ATTR64);

	/*
	 * Darwin defines the size for the file mode
	 * as 2 bytes; BSM defines 4 so pad with 0
	 */
	ADD_U_INT16(dptr, pad0_16);
	ADD_U_INT16(dptr, vni->vn_mode);

	ADD_U_INT32(dptr, vni->vn_uid);
	ADD_U_INT32(dptr, vni->vn_gid);
	ADD_U_INT32(dptr, vni->vn_fsid);

	/*
	 * Some systems use 32-bit file ID's, other's use 64-bit file IDs.
	 * Attempt to handle both, and let the compiler sort it out.  If we
	 * could pick this out at compile-time, it would be better, so as to
	 * avoid the else case below.
	 */
	if (sizeof(vni->vn_fileid) == sizeof(uint32_t)) {
		ADD_U_INT32(dptr, pad0_32);
		ADD_U_INT32(dptr, vni->vn_fileid);
	} else if (sizeof(vni->vn_fileid) == sizeof(uint64_t))
		ADD_U_INT64(dptr, vni->vn_fileid);
	else
		ADD_U_INT64(dptr, 0LL);

	ADD_U_INT64(dptr, vni->vn_dev);

	return (t);
}

token_t *
au_to_attr(struct vnode_au_info *vni)
{

	return (au_to_attr32(vni));
}
#endif /* !(defined(_KERNEL) || defined(KERNEL) */

/*
 * token ID                1 byte
 * how to print            1 byte
 * basic unit              1 byte
 * unit count              1 byte
 * data items              (depends on basic unit)
 */
token_t *
au_to_data(char unit_print, char unit_type, char unit_count, char *p)
{
	token_t *t;
	u_char *dptr = NULL;
	size_t datasize, totdata;

	/* Determine the size of the basic unit. */
	switch (unit_type) {
	case AUR_BYTE:
	/* case AUR_CHAR: */
		datasize = AUR_BYTE_SIZE;
		break;

	case AUR_SHORT:
		datasize = AUR_SHORT_SIZE;
		break;

	case AUR_INT32:
	/* case AUR_INT: */
		datasize = AUR_INT32_SIZE;
		break;

	case AUR_INT64:
		datasize = AUR_INT64_SIZE;
		break;

	default:
		errno = EINVAL;
 		return (NULL);
	}

	totdata = datasize * unit_count;

	GET_TOKEN_AREA(t, dptr, 4 * sizeof(u_char) + totdata);
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_DATA);
	ADD_U_CHAR(dptr, unit_print);
	ADD_U_CHAR(dptr, unit_type);
	ADD_U_CHAR(dptr, unit_count);
	ADD_MEM(dptr, p, totdata);

	return (t);
}


/*
 * token ID                1 byte
 * status		   4 bytes
 * return value            4 bytes
 */
token_t *
au_to_exit(int retval, int err)
{
	token_t *t;
	u_char *dptr = NULL;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + 2 * sizeof(u_int32_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_EXIT);
	ADD_U_INT32(dptr, err);
	ADD_U_INT32(dptr, retval);

	return (t);
}

/*
 */
token_t *
au_to_groups(int *groups)
{

	return (au_to_newgroups(AUDIT_MAX_GROUPS, (gid_t*)groups));
}

/*
 * token ID                1 byte
 * number groups           2 bytes
 * group list              count * 4 bytes
 */
token_t *
au_to_newgroups(u_int16_t n, gid_t *groups)
{
	token_t *t;
	u_char *dptr = NULL;
	int i;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + sizeof(u_int16_t) +
	    n * sizeof(u_int32_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_NEWGROUPS);
	ADD_U_INT16(dptr, n);
	for (i = 0; i < n; i++)
		ADD_U_INT32(dptr, groups[i]);

	return (t);
}

/*
 * token ID                1 byte
 * internet address        4 bytes
 */
token_t *
au_to_in_addr(struct in_addr *internet_addr)
{
	token_t *t;
	u_char *dptr = NULL;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + sizeof(uint32_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_IN_ADDR);
	ADD_MEM(dptr, &internet_addr->s_addr, sizeof(uint32_t));

	return (t);
}

/*
 * token ID                1 byte
 * address type/length     4 bytes
 * Address                16 bytes
 */
token_t *
au_to_in_addr_ex(struct in6_addr *internet_addr)
{
	token_t *t;
	u_char *dptr = NULL;
	u_int32_t type = AF_INET6;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + 5 * sizeof(uint32_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_IN_ADDR_EX);
	ADD_U_INT32(dptr, type);
	ADD_MEM(dptr, internet_addr, 4 * sizeof(uint32_t));

	return (t);
}

/*
 * token ID                1 byte
 * ip header		   20 bytes
 *
 * The IP header should be submitted in network byte order.
 */
token_t *
au_to_ip(struct ip *ip)
{
	token_t *t;
	u_char *dptr = NULL;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + sizeof(struct ip));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_IP);
	ADD_MEM(dptr, ip, sizeof(struct ip));

	return (t);
}

/*
 * token ID                1 byte
 * object ID type          1 byte
 * object ID               4 bytes
 */
token_t *
au_to_ipc(char type, int id)
{
	token_t *t;
	u_char *dptr = NULL;

	GET_TOKEN_AREA(t, dptr, 2 * sizeof(u_char) + sizeof(u_int32_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_IPC);
	ADD_U_CHAR(dptr, type);
	ADD_U_INT32(dptr, id);

	return (t);
}

/*
 * token ID                1 byte
 * owner user ID           4 bytes
 * owner group ID          4 bytes
 * creator user ID         4 bytes
 * creator group ID        4 bytes
 * access mode             4 bytes
 * slot sequence #         4 bytes
 * key                     4 bytes
 */
token_t *
au_to_ipc_perm(struct ipc_perm *perm)
{
	token_t *t;
	u_char *dptr = NULL;
	u_int16_t pad0 = 0;

	GET_TOKEN_AREA(t, dptr, 12 * sizeof(u_int16_t) + sizeof(u_int32_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_IPC_PERM);

	/*
	 * Darwin defines the sizes for ipc_perm members
	 * as 2 bytes; BSM defines 4 so pad with 0
	 */
	ADD_U_INT16(dptr, pad0);
	ADD_U_INT16(dptr, perm->uid);

	ADD_U_INT16(dptr, pad0);
	ADD_U_INT16(dptr, perm->gid);

	ADD_U_INT16(dptr, pad0);
	ADD_U_INT16(dptr, perm->cuid);

	ADD_U_INT16(dptr, pad0);
	ADD_U_INT16(dptr, perm->cgid);

	ADD_U_INT16(dptr, pad0);
	ADD_U_INT16(dptr, perm->mode);

	ADD_U_INT16(dptr, pad0);

#ifdef HAVE_IPC_PERM___SEQ
	ADD_U_INT16(dptr, perm->__seq);
#else
	ADD_U_INT16(dptr, perm->seq);
#endif

#ifdef HAVE_IPC_PERM___KEY
	ADD_U_INT32(dptr, perm->__key);
#else
	ADD_U_INT32(dptr, perm->key);
#endif

	return (t);
}

/*
 * token ID                1 byte
 * port IP address         2 bytes
 */
token_t *
au_to_iport(u_int16_t iport)
{
	token_t *t;
	u_char *dptr = NULL;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + sizeof(u_int16_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_IPORT);
	ADD_U_INT16(dptr, iport);

	return (t);
}

/*
 * token ID                1 byte
 * size                    2 bytes
 * data                    size bytes
 */
token_t *
au_to_opaque(char *data, u_int16_t bytes)
{
	token_t *t;
	u_char *dptr = NULL;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + sizeof(u_int16_t) + bytes);
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_OPAQUE);
	ADD_U_INT16(dptr, bytes);
	ADD_MEM(dptr, data, bytes);

	return (t);
}

/*
 * token ID                1 byte
 * seconds of time         4 bytes
 * milliseconds of time    4 bytes
 * file name len           2 bytes
 * file pathname           N bytes + 1 terminating NULL byte
 */
token_t *
au_to_file(char *file, struct timeval tm)
{
	token_t *t;
	u_char *dptr = NULL;
	u_int16_t filelen;
	u_int32_t timems;

	filelen = strlen(file);
	filelen += 1;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + 2 * sizeof(u_int32_t) +
	    sizeof(u_int16_t) + filelen);
	if (t == NULL)
		return (NULL);

	timems = tm.tv_usec/1000;

	ADD_U_CHAR(dptr, AUT_OTHER_FILE32);
	ADD_U_INT32(dptr, tm.tv_sec);
	ADD_U_INT32(dptr, timems);	/* We need time in ms. */
	ADD_U_INT16(dptr, filelen);
	ADD_STRING(dptr, file, filelen);

	return (t);
}

/*
 * token ID                1 byte
 * text length             2 bytes
 * text                    N bytes + 1 terminating NULL byte
 */
token_t *
au_to_text(char *text)
{
	token_t *t;
	u_char *dptr = NULL;
	u_int16_t textlen;

	textlen = strlen(text);
	textlen += 1;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + sizeof(u_int16_t) + textlen);
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_TEXT);
	ADD_U_INT16(dptr, textlen);
	ADD_STRING(dptr, text, textlen);

	return (t);
}

/*
 * token ID                1 byte
 * path length             2 bytes
 * path                    N bytes + 1 terminating NULL byte
 */
token_t *
au_to_path(char *text)
{
	token_t *t;
	u_char *dptr = NULL;
	u_int16_t textlen;

	textlen = strlen(text);
	textlen += 1;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + sizeof(u_int16_t) + textlen);
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_PATH);
	ADD_U_INT16(dptr, textlen);
	ADD_STRING(dptr, text, textlen);

	return (t);
}

/*
 * token ID                1 byte
 * audit ID                4 bytes
 * effective user ID       4 bytes
 * effective group ID      4 bytes
 * real user ID            4 bytes
 * real group ID           4 bytes
 * process ID              4 bytes
 * session ID              4 bytes
 * terminal ID
 *   port ID               4 bytes/8 bytes (32-bit/64-bit value)
 *   machine address       4 bytes
 */
token_t *
au_to_process32(au_id_t auid, uid_t euid, gid_t egid, uid_t ruid, gid_t rgid,
    pid_t pid, au_asid_t sid, au_tid_t *tid)
{
	token_t *t;
	u_char *dptr = NULL;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + 9 * sizeof(u_int32_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_PROCESS32);
	ADD_U_INT32(dptr, auid);
	ADD_U_INT32(dptr, euid);
	ADD_U_INT32(dptr, egid);
	ADD_U_INT32(dptr, ruid);
	ADD_U_INT32(dptr, rgid);
	ADD_U_INT32(dptr, pid);
	ADD_U_INT32(dptr, sid);
	ADD_U_INT32(dptr, tid->port);
	ADD_MEM(dptr, &tid->machine, sizeof(u_int32_t));

	return (t);
}

token_t *
au_to_process64(au_id_t auid, uid_t euid, gid_t egid, uid_t ruid, gid_t rgid,
    pid_t pid, au_asid_t sid, au_tid_t *tid)
{
	token_t *t;
	u_char *dptr = NULL;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + 8 * sizeof(u_int32_t) +
	    sizeof(u_int64_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_PROCESS64);
	ADD_U_INT32(dptr, auid);
	ADD_U_INT32(dptr, euid);
	ADD_U_INT32(dptr, egid);
	ADD_U_INT32(dptr, ruid);
	ADD_U_INT32(dptr, rgid);
	ADD_U_INT32(dptr, pid);
	ADD_U_INT32(dptr, sid);
	ADD_U_INT64(dptr, tid->port);
	ADD_MEM(dptr, &tid->machine, sizeof(u_int32_t));

	return (t);
}

token_t *
au_to_process(au_id_t auid, uid_t euid, gid_t egid, uid_t ruid, gid_t rgid,
    pid_t pid, au_asid_t sid, au_tid_t *tid)
{

	return (au_to_process32(auid, euid, egid, ruid, rgid, pid, sid,
	    tid));
}

/*
 * token ID                1 byte
 * audit ID                4 bytes
 * effective user ID       4 bytes
 * effective group ID      4 bytes
 * real user ID            4 bytes
 * real group ID           4 bytes
 * process ID              4 bytes
 * session ID              4 bytes
 * terminal ID
 *   port ID               4 bytes/8 bytes (32-bit/64-bit value)
 *   address type-len      4 bytes
 *   machine address      16 bytes
 */
token_t *
au_to_process32_ex(au_id_t auid, uid_t euid, gid_t egid, uid_t ruid,
    gid_t rgid, pid_t pid, au_asid_t sid, au_tid_addr_t *tid)
{
	token_t *t;
	u_char *dptr = NULL;

	if (tid->at_type == AU_IPv4)
		GET_TOKEN_AREA(t, dptr, sizeof(u_char) +
		    10 * sizeof(u_int32_t));
	else if (tid->at_type == AU_IPv6)
		GET_TOKEN_AREA(t, dptr, sizeof(u_char) +
		    13 * sizeof(u_int32_t));
	else {
		errno = EINVAL;
		return (NULL);
	}
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_PROCESS32_EX);
	ADD_U_INT32(dptr, auid);
	ADD_U_INT32(dptr, euid);
	ADD_U_INT32(dptr, egid);
	ADD_U_INT32(dptr, ruid);
	ADD_U_INT32(dptr, rgid);
	ADD_U_INT32(dptr, pid);
	ADD_U_INT32(dptr, sid);
	ADD_U_INT32(dptr, tid->at_port);
	ADD_U_INT32(dptr, tid->at_type);
	ADD_MEM(dptr, &tid->at_addr[0], sizeof(u_int32_t));
	if (tid->at_type == AU_IPv6) {
		ADD_MEM(dptr, &tid->at_addr[1], sizeof(u_int32_t));
		ADD_MEM(dptr, &tid->at_addr[2], sizeof(u_int32_t));
		ADD_MEM(dptr, &tid->at_addr[3], sizeof(u_int32_t));
	}

	return (t);
}

token_t *
au_to_process64_ex(au_id_t auid, uid_t euid, gid_t egid, uid_t ruid,
    gid_t rgid, pid_t pid, au_asid_t sid, au_tid_addr_t *tid)
{
	token_t *t;
	u_char *dptr = NULL;

	if (tid->at_type == AU_IPv4)
		GET_TOKEN_AREA(t, dptr, sizeof(u_char) +
		    7 * sizeof(u_int32_t) + sizeof(u_int64_t) +
		    2 * sizeof(u_int32_t));
	else if (tid->at_type == AU_IPv6)
		GET_TOKEN_AREA(t, dptr, sizeof(u_char) +
		    7 * sizeof(u_int32_t) + sizeof(u_int64_t) +
		    5 * sizeof(u_int32_t));
	else {
		errno = EINVAL;
		return (NULL);
	}
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_PROCESS64_EX);
	ADD_U_INT32(dptr, auid);
	ADD_U_INT32(dptr, euid);
	ADD_U_INT32(dptr, egid);
	ADD_U_INT32(dptr, ruid);
	ADD_U_INT32(dptr, rgid);
	ADD_U_INT32(dptr, pid);
	ADD_U_INT32(dptr, sid);
	ADD_U_INT64(dptr, tid->at_port);
	ADD_U_INT32(dptr, tid->at_type);
	ADD_MEM(dptr, &tid->at_addr[0], sizeof(u_int32_t));
	if (tid->at_type == AU_IPv6) {
		ADD_MEM(dptr, &tid->at_addr[1], sizeof(u_int32_t));
		ADD_MEM(dptr, &tid->at_addr[2], sizeof(u_int32_t));
		ADD_MEM(dptr, &tid->at_addr[3], sizeof(u_int32_t));
	}

	return (t);
}

token_t *
au_to_process_ex(au_id_t auid, uid_t euid, gid_t egid, uid_t ruid,
    gid_t rgid, pid_t pid, au_asid_t sid, au_tid_addr_t *tid)
{

	return (au_to_process32_ex(auid, euid, egid, ruid, rgid, pid, sid,
	    tid));
}

/*
 * token ID                1 byte
 * error status            1 byte
 * return value            4 bytes/8 bytes (32-bit/64-bit value)
 */
token_t *
au_to_return32(char status, u_int32_t ret)
{
	token_t *t;
	u_char *dptr = NULL;

	GET_TOKEN_AREA(t, dptr, 2 * sizeof(u_char) + sizeof(u_int32_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_RETURN32);
	ADD_U_CHAR(dptr, status);
	ADD_U_INT32(dptr, ret);

	return (t);
}

token_t *
au_to_return64(char status, u_int64_t ret)
{
	token_t *t;
	u_char *dptr = NULL;

	GET_TOKEN_AREA(t, dptr, 2 * sizeof(u_char) + sizeof(u_int64_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_RETURN64);
	ADD_U_CHAR(dptr, status);
	ADD_U_INT64(dptr, ret);

	return (t);
}

token_t *
au_to_return(char status, u_int32_t ret)
{

	return (au_to_return32(status, ret));
}

/*
 * token ID                1 byte
 * sequence number         4 bytes
 */
token_t *
au_to_seq(long audit_count)
{
	token_t *t;
	u_char *dptr = NULL;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + sizeof(u_int32_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_SEQ);
	ADD_U_INT32(dptr, audit_count);

	return (t);
}

/*
 * token ID                1 byte
 * socket family           2 bytes
 * path                    104 bytes
 */
token_t *
au_to_sock_unix(struct sockaddr_un *so)
{
	token_t *t;
	u_char *dptr;

	GET_TOKEN_AREA(t, dptr, 3 * sizeof(u_char) + strlen(so->sun_path) + 1);
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AU_SOCK_UNIX_TOKEN);
	/* BSM token has two bytes for family */
	ADD_U_CHAR(dptr, 0);
	ADD_U_CHAR(dptr, so->sun_family);
	ADD_STRING(dptr, so->sun_path, strlen(so->sun_path) + 1);

	return (t);
}

/*
 * token ID                1 byte
 * socket family           2 bytes
 * local port              2 bytes
 * socket address          4 bytes
 */
token_t *
au_to_sock_inet32(struct sockaddr_in *so)
{
	token_t *t;
	u_char *dptr = NULL;
	uint16_t family;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + 2 * sizeof(uint16_t) +
	    sizeof(uint32_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_SOCKINET32);
	/*
	 * BSM defines the family field as 16 bits, but many operating
	 * systems have an 8-bit sin_family field.  Extend to 16 bits before
	 * writing into the token.  Assume that both the port and the address
	 * in the sockaddr_in are already in network byte order, but family
	 * is in local byte order.
	 *
	 * XXXRW: Should a name space conversion be taking place on the value
	 * of sin_family?
 	 */
	family = so->sin_family;
	ADD_U_INT16(dptr, family);
	ADD_MEM(dptr, &so->sin_port, sizeof(uint16_t));
	ADD_MEM(dptr, &so->sin_addr.s_addr, sizeof(uint32_t));

	return (t);

}

token_t *
au_to_sock_inet128(struct sockaddr_in6 *so)
{
	token_t *t;
	u_char *dptr = NULL;

	GET_TOKEN_AREA(t, dptr, 3 * sizeof(u_char) + sizeof(u_int16_t) +
	    4 * sizeof(u_int32_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_SOCKINET128);
	/*
	 * In Darwin, sin6_family is one octet, but BSM defines the token
 	 * to store two. So we copy in a 0 first.
 	 */
	ADD_U_CHAR(dptr, 0);
	ADD_U_CHAR(dptr, so->sin6_family);

	ADD_U_INT16(dptr, so->sin6_port);
	ADD_MEM(dptr, &so->sin6_addr, 4 * sizeof(uint32_t));

	return (t);

}

token_t *
au_to_sock_inet(struct sockaddr_in *so)
{

	return (au_to_sock_inet32(so));
}

/*
 * token ID                1 byte
 * audit ID                4 bytes
 * effective user ID       4 bytes
 * effective group ID      4 bytes
 * real user ID            4 bytes
 * real group ID           4 bytes
 * process ID              4 bytes
 * session ID              4 bytes
 * terminal ID
 *   port ID               4 bytes/8 bytes (32-bit/64-bit value)
 *   machine address       4 bytes
 */
token_t *
au_to_subject32(au_id_t auid, uid_t euid, gid_t egid, uid_t ruid, gid_t rgid,
    pid_t pid, au_asid_t sid, au_tid_t *tid)
{
	token_t *t;
	u_char *dptr = NULL;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + 9 * sizeof(u_int32_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_SUBJECT32);
	ADD_U_INT32(dptr, auid);
	ADD_U_INT32(dptr, euid);
	ADD_U_INT32(dptr, egid);
	ADD_U_INT32(dptr, ruid);
	ADD_U_INT32(dptr, rgid);
	ADD_U_INT32(dptr, pid);
	ADD_U_INT32(dptr, sid);
	ADD_U_INT32(dptr, tid->port);
	ADD_MEM(dptr, &tid->machine, sizeof(u_int32_t));

	return (t);
}

token_t *
au_to_subject64(au_id_t auid, uid_t euid, gid_t egid, uid_t ruid, gid_t rgid,
    pid_t pid, au_asid_t sid, au_tid_t *tid)
{
	token_t *t;
	u_char *dptr = NULL;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + 7 * sizeof(u_int32_t) +
	    sizeof(u_int64_t) + sizeof(u_int32_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_SUBJECT64);
	ADD_U_INT32(dptr, auid);
	ADD_U_INT32(dptr, euid);
	ADD_U_INT32(dptr, egid);
	ADD_U_INT32(dptr, ruid);
	ADD_U_INT32(dptr, rgid);
	ADD_U_INT32(dptr, pid);
	ADD_U_INT32(dptr, sid);
	ADD_U_INT64(dptr, tid->port);
	ADD_MEM(dptr, &tid->machine, sizeof(u_int32_t));

	return (t);
}

token_t *
au_to_subject(au_id_t auid, uid_t euid, gid_t egid, uid_t ruid, gid_t rgid,
    pid_t pid, au_asid_t sid, au_tid_t *tid)
{

	return (au_to_subject32(auid, euid, egid, ruid, rgid, pid, sid,
	    tid));
}

/*
 * token ID                1 byte
 * audit ID                4 bytes
 * effective user ID       4 bytes
 * effective group ID      4 bytes
 * real user ID            4 bytes
 * real group ID           4 bytes
 * process ID              4 bytes
 * session ID              4 bytes
 * terminal ID
 *   port ID               4 bytes/8 bytes (32-bit/64-bit value)
 *   address type/length   4 bytes
 *   machine address      16 bytes
 */
token_t *
au_to_subject32_ex(au_id_t auid, uid_t euid, gid_t egid, uid_t ruid,
    gid_t rgid, pid_t pid, au_asid_t sid, au_tid_addr_t *tid)
{
	token_t *t;
	u_char *dptr = NULL;

	if (tid->at_type == AU_IPv4)
		GET_TOKEN_AREA(t, dptr, sizeof(u_char) + 10 *
		    sizeof(u_int32_t));
	else if (tid->at_type == AU_IPv6)
		GET_TOKEN_AREA(t, dptr, sizeof(u_char) + 13 *
		    sizeof(u_int32_t));
	else {
		errno = EINVAL;
		return (NULL);
	}
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_SUBJECT32_EX);
	ADD_U_INT32(dptr, auid);
	ADD_U_INT32(dptr, euid);
	ADD_U_INT32(dptr, egid);
	ADD_U_INT32(dptr, ruid);
	ADD_U_INT32(dptr, rgid);
	ADD_U_INT32(dptr, pid);
	ADD_U_INT32(dptr, sid);
	ADD_U_INT32(dptr, tid->at_port);
	ADD_U_INT32(dptr, tid->at_type);
	if (tid->at_type == AU_IPv6)
		ADD_MEM(dptr, &tid->at_addr[0], 4 * sizeof(u_int32_t));
	else
		ADD_MEM(dptr, &tid->at_addr[0], sizeof(u_int32_t));

	return (t);
}

token_t *
au_to_subject64_ex(au_id_t auid, uid_t euid, gid_t egid, uid_t ruid,
    gid_t rgid, pid_t pid, au_asid_t sid, au_tid_addr_t *tid)
{
	token_t *t;
	u_char *dptr = NULL;

	if (tid->at_type == AU_IPv4)
		GET_TOKEN_AREA(t, dptr, sizeof(u_char) +
		    7 * sizeof(u_int32_t) + sizeof(u_int64_t) +
		    2 * sizeof(u_int32_t));
	else if (tid->at_type == AU_IPv6)
		GET_TOKEN_AREA(t, dptr, sizeof(u_char) +
		    7 * sizeof(u_int32_t) + sizeof(u_int64_t) +
		    5 * sizeof(u_int32_t));
	else {
		errno = EINVAL;
		return (NULL);
	}
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_SUBJECT64_EX);
	ADD_U_INT32(dptr, auid);
	ADD_U_INT32(dptr, euid);
	ADD_U_INT32(dptr, egid);
	ADD_U_INT32(dptr, ruid);
	ADD_U_INT32(dptr, rgid);
	ADD_U_INT32(dptr, pid);
	ADD_U_INT32(dptr, sid);
	ADD_U_INT64(dptr, tid->at_port);
	ADD_U_INT32(dptr, tid->at_type);
	if (tid->at_type == AU_IPv6)
		ADD_MEM(dptr, &tid->at_addr[0], 4 * sizeof(u_int32_t));
	else
		ADD_MEM(dptr, &tid->at_addr[0], sizeof(u_int32_t));

	return (t);
}

token_t *
au_to_subject_ex(au_id_t auid, uid_t euid, gid_t egid, uid_t ruid,
    gid_t rgid, pid_t pid, au_asid_t sid, au_tid_addr_t *tid)
{

	return (au_to_subject32_ex(auid, euid, egid, ruid, rgid, pid, sid,
	    tid));
}

#if !defined(_KERNEL) && !defined(KERNEL) && defined(HAVE_AUDIT_SYSCALLS)
/*
 * Collects audit information for the current process
 * and creates a subject token from it
 */
token_t *
au_to_me(void)
{
	auditinfo_t auinfo;

	if (getaudit(&auinfo) != 0)
		return (NULL);

	return (au_to_subject32(auinfo.ai_auid, geteuid(), getegid(),
	    getuid(), getgid(), getpid(), auinfo.ai_asid, &auinfo.ai_termid));
}
#endif

/*
 * token ID				1 byte
 * count				4 bytes
 * text					count null-terminated strings
 */
token_t *
au_to_exec_args(char **argv)
{
	token_t *t;
	u_char *dptr = NULL;
	const char *nextarg;
	int i, count = 0;
	size_t totlen = 0;

	nextarg = *argv;

	while (nextarg != NULL) {
		int nextlen;

		nextlen = strlen(nextarg);
		totlen += nextlen + 1;
		count++;
		nextarg = *(argv + count);
	}

	totlen += count * sizeof(char);	/* nul terminations. */
	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + sizeof(u_int32_t) + totlen);
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_EXEC_ARGS);
	ADD_U_INT32(dptr, count);

	for (i = 0; i < count; i++) {
		nextarg = *(argv + i);
		ADD_MEM(dptr, nextarg, strlen(nextarg) + 1);
	}

	return (t);
}

/*
 * token ID                1 byte
 * zonename length         2 bytes
 * zonename                N bytes + 1 terminating NULL byte
 */
token_t *
au_to_zonename(char *zonename)
{
	u_char *dptr = NULL;
	u_int16_t textlen;
	token_t *t;

	textlen = strlen(zonename);
	textlen += 1;
	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + sizeof(u_int16_t) + textlen);
	ADD_U_CHAR(dptr, AUT_ZONENAME);
	ADD_U_INT16(dptr, textlen);
	ADD_STRING(dptr, zonename, textlen);
	return (t);
}

/*
 * token ID				1 byte
 * count				4 bytes
 * text					count null-terminated strings
 */
token_t *
au_to_exec_env(char **envp)
{
	token_t *t;
	u_char *dptr = NULL;
	int i, count = 0;
	size_t totlen = 0;
	const char *nextenv;

	nextenv = *envp;

	while (nextenv != NULL) {
		int nextlen;

		nextlen = strlen(nextenv);
		totlen += nextlen + 1;
		count++;
		nextenv = *(envp + count);
	}

	totlen += sizeof(char) * count;
	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + sizeof(u_int32_t) + totlen);
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_EXEC_ENV);
	ADD_U_INT32(dptr, count);

	for (i = 0; i < count; i++) {
		nextenv = *(envp + i);
		ADD_MEM(dptr, nextenv, strlen(nextenv) + 1);
	}

	return (t);
}

/*
 * token ID                1 byte
 * record byte count       4 bytes
 * version #               1 byte    [2]
 * event type              2 bytes
 * event modifier          2 bytes
 * seconds of time         4 bytes/8 bytes (32-bit/64-bit value)
 * milliseconds of time    4 bytes/8 bytes (32-bit/64-bit value)
 */
token_t *
au_to_header32_tm(int rec_size, au_event_t e_type, au_emod_t e_mod,
    struct timeval tm)
{
	token_t *t;
	u_char *dptr = NULL;
	u_int32_t timems;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + sizeof(u_int32_t) +
	    sizeof(u_char) + 2 * sizeof(u_int16_t) + 2 * sizeof(u_int32_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_HEADER32);
	ADD_U_INT32(dptr, rec_size);
	ADD_U_CHAR(dptr, AUDIT_HEADER_VERSION_OPENBSM);
	ADD_U_INT16(dptr, e_type);
	ADD_U_INT16(dptr, e_mod);

	timems = tm.tv_usec/1000;
	/* Add the timestamp */
	ADD_U_INT32(dptr, tm.tv_sec);
	ADD_U_INT32(dptr, timems);	/* We need time in ms. */

	return (t);
}

token_t *
au_to_header64_tm(int rec_size, au_event_t e_type, au_emod_t e_mod,
    struct timeval tm)
{
	token_t *t;
	u_char *dptr = NULL;
	u_int32_t timems;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + sizeof(u_int32_t) +
	    sizeof(u_char) + 2 * sizeof(u_int16_t) + 2 * sizeof(u_int64_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_HEADER64);
	ADD_U_INT32(dptr, rec_size);
	ADD_U_CHAR(dptr, AUDIT_HEADER_VERSION_OPENBSM);
	ADD_U_INT16(dptr, e_type);
	ADD_U_INT16(dptr, e_mod);

	timems = tm.tv_usec/1000;
	/* Add the timestamp */
	ADD_U_INT64(dptr, tm.tv_sec);
	ADD_U_INT64(dptr, timems);	/* We need time in ms. */

	return (t);
}

#if !defined(KERNEL) && !defined(_KERNEL)
token_t *
au_to_header32(int rec_size, au_event_t e_type, au_emod_t e_mod)
{
	struct timeval tm;

	if (gettimeofday(&tm, NULL) == -1)
		return (NULL);
	return (au_to_header32_tm(rec_size, e_type, e_mod, tm));
}

token_t *
au_to_header64(__unused int rec_size, __unused au_event_t e_type,
    __unused au_emod_t e_mod)
{
	struct timeval tm;

	if (gettimeofday(&tm, NULL) == -1)
		return (NULL);
	return (au_to_header64_tm(rec_size, e_type, e_mod, tm));
}

token_t *
au_to_header(int rec_size, au_event_t e_type, au_emod_t e_mod)
{

	return (au_to_header32(rec_size, e_type, e_mod));
}
#endif

/*
 * token ID                1 byte
 * trailer magic number    2 bytes
 * record byte count       4 bytes
 */
token_t *
au_to_trailer(int rec_size)
{
	token_t *t;
	u_char *dptr = NULL;
	u_int16_t magic = TRAILER_PAD_MAGIC;

	GET_TOKEN_AREA(t, dptr, sizeof(u_char) + sizeof(u_int16_t) +
	    sizeof(u_int32_t));
	if (t == NULL)
		return (NULL);

	ADD_U_CHAR(dptr, AUT_TRAILER);
	ADD_U_INT16(dptr, magic);
	ADD_U_INT32(dptr, rec_size);

	return (t);
}
