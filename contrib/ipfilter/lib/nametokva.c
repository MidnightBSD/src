/*	$FreeBSD$	*/

/*
 * Copyright (C) 2002 by Darren Reed.
 * 
 * See the IPFILTER.LICENCE file for details on licencing.  
 *   
 * $Id: nametokva.c,v 1.1.1.3 2012-07-21 15:01:08 laffer1 Exp $ 
 */     

#include "ipf.h"

#include <sys/ioctl.h>
#include <fcntl.h>

ipfunc_t nametokva(name, iocfunc)
char *name;
ioctlfunc_t iocfunc;
{
	ipfunc_resolve_t res;
	int fd;

	strncpy(res.ipfu_name, name, sizeof(res.ipfu_name));
	res.ipfu_addr = NULL;
	fd = -1;

	if ((opts & OPT_DONOTHING) == 0) {
		fd = open(IPL_NAME, O_RDONLY);
		if (fd == -1)
			return NULL;
	}
	(void) (*iocfunc)(fd, SIOCFUNCL, &res);
	if (fd >= 0)
		close(fd);
	if (res.ipfu_addr == NULL)
		res.ipfu_addr = (ipfunc_t)-1;
	return res.ipfu_addr;
}
