/*	$FreeBSD$	*/

/*
 * Copyright (C) 2003 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: remove_pool.c,v 1.1.1.3 2012-07-21 15:01:08 laffer1 Exp $
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include "ipf.h"
#include "netinet/ip_lookup.h"
#include "netinet/ip_htable.h"

static int poolfd = -1;


int remove_pool(poolp, iocfunc)
ip_pool_t *poolp;
ioctlfunc_t iocfunc;
{
	iplookupop_t op;
	ip_pool_t pool;

	if ((poolfd == -1) && ((opts & OPT_DONOTHING) == 0))
		poolfd = open(IPLOOKUP_NAME, O_RDWR);
	if ((poolfd == -1) && ((opts & OPT_DONOTHING) == 0))
		return -1;

	op.iplo_type = IPLT_POOL;
	op.iplo_unit = poolp->ipo_unit;
	strncpy(op.iplo_name, poolp->ipo_name, sizeof(op.iplo_name));
	op.iplo_size = sizeof(pool);
	op.iplo_struct = &pool;

	bzero((char *)&pool, sizeof(pool));
	pool.ipo_unit = poolp->ipo_unit;
	strncpy(pool.ipo_name, poolp->ipo_name, sizeof(pool.ipo_name));
	pool.ipo_flags = poolp->ipo_flags;

	if ((*iocfunc)(poolfd, SIOCLOOKUPDELTABLE, &op))
		if ((opts & OPT_DONOTHING) == 0) {
			perror("remove_pool:SIOCLOOKUPDELTABLE");
			return -1;
		}

	return 0;
}
