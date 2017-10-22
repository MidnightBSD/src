/*-
 * Copyright (c) 2000-2013 Mark R V Murray
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: release/10.0.0/sys/dev/random/randomdev_soft.h 256381 2013-10-12 15:31:36Z markm $
 */

#ifndef SYS_DEV_RANDOM_RANDOMDEV_SOFT_H_INCLUDED
#define SYS_DEV_RANDOM_RANDOMDEV_SOFT_H_INCLUDED

/* This header contains only those definitions that are global
 * and harvester-specific for the entropy processor
 */

/* #define ENTROPYSOURCE nn	   entropy sources (actually classes)
 *					This is properly defined in
 *					an enum in sys/random.h
 */

/* The ring size _MUST_ be a power of 2 */
#define HARVEST_RING_SIZE	1024	/* harvest ring buffer size */
#define HARVEST_RING_MASK	(HARVEST_RING_SIZE - 1)

#define HARVESTSIZE	16	/* max size of each harvested entropy unit */

/* These are used to queue harvested packets of entropy. The entropy
 * buffer size is pretty arbitrary.
 */
struct harvest {
	uintmax_t somecounter;		/* fast counter for clock jitter */
	uint8_t entropy[HARVESTSIZE];	/* the harvested entropy */
	u_int size, bits;		/* stats about the entropy */
	enum esource source;		/* origin of the entropy */
	STAILQ_ENTRY(harvest) next;	/* next item on the list */
};

void randomdev_init(void);
void randomdev_deinit(void);

void randomdev_init_harvester(void (*)(u_int64_t, const void *, u_int,
	u_int, enum esource), int (*)(void *, int));
void randomdev_deinit_harvester(void);

void random_set_wakeup_exit(void *);
void random_process_event(struct harvest *event);
void randomdev_unblock(void);

extern struct mtx random_reseed_mtx;

/* If this was C++, the macro below would be a template */
#define RANDOM_CHECK_UINT(name, min, max)				\
static int								\
random_check_uint_##name(SYSCTL_HANDLER_ARGS)				\
{									\
	if (oidp->oid_arg1 != NULL) {					\
		 if (*(u_int *)(oidp->oid_arg1) <= (min))		\
			*(u_int *)(oidp->oid_arg1) = (min);		\
		 else if (*(u_int *)(oidp->oid_arg1) > (max))		\
			*(u_int *)(oidp->oid_arg1) = (max);		\
	}								\
        return (sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2,	\
		req));							\
}

#endif
