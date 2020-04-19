/*	$NetBSD: bootparam.h,v 1.3 1998/01/05 19:19:41 perry Exp $	*/
/*	$FreeBSD: stable/11/stand/libsa/bootparam.h 344376 2019-02-20 19:05:58Z kevans $ */

int bp_whoami(int sock);
int bp_getfile(int sock, char *key, struct in_addr *addrp, char *path);

