/*	$FreeBSD: src/contrib/ipfilter/lib/checkrev.c,v 1.4 2007/06/04 02:54:32 darrenr Exp $	*/

/*
 * Copyright (C) 2000-2004 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: checkrev.c,v 1.1.1.2 2008-11-22 14:33:09 laffer1 Exp $
 */

#include <sys/ioctl.h>
#include <fcntl.h>

#include "ipf.h"
#include "netinet/ipl.h"

int checkrev(ipfname)
char *ipfname;
{
	static int vfd = -1;
	struct friostat fio, *fiop = &fio;
	ipfobj_t ipfo;

	bzero((caddr_t)&ipfo, sizeof(ipfo));
	ipfo.ipfo_rev = IPFILTER_VERSION;
	ipfo.ipfo_size = sizeof(*fiop);
	ipfo.ipfo_ptr = (void *)fiop;
	ipfo.ipfo_type = IPFOBJ_IPFSTAT;

	if ((vfd == -1) && ((vfd = open(ipfname, O_RDONLY)) == -1)) {
		perror("open device");
		return -1;
	}

	if (ioctl(vfd, SIOCGETFS, &ipfo)) {
		perror("ioctl(SIOCGETFS)");
		close(vfd);
		vfd = -1;
		return -1;
	}

	if (strncmp(IPL_VERSION, fio.f_version, sizeof(fio.f_version))) {
		return -1;
	}
	return 0;
}
