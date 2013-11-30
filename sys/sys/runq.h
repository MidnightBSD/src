/*-
 * Copyright (c) 2001 Jake Burkholder <jake@FreeBSD.org>
 * All rights reserved.
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
 *
 * $FreeBSD: src/sys/sys/runq.h,v 1.6 2005/01/07 02:29:24 imp Exp $
 */

#ifndef	_RUNQ_H_
#define	_RUNQ_H_

#include <machine/runq.h>

struct kse;

/*
 * Run queue parameters.
 */

#define	RQ_NQS		(64)		/* Number of run queues. */
#define	RQ_PPQ		(4)		/* Priorities per queue. */

/*
 * Head of run queues.
 */
TAILQ_HEAD(rqhead, kse);

/*
 * Bit array which maintains the status of a run queue.  When a queue is
 * non-empty the bit corresponding to the queue number will be set.
 */
struct rqbits {
	rqb_word_t rqb_bits[RQB_LEN];
};

/*
 * Run queue structure.  Contains an array of run queues on which processes
 * are placed, and a structure to maintain the status of each queue.
 */
struct runq {
	struct	rqbits rq_status;
	struct	rqhead rq_queues[RQ_NQS];
};

void	runq_add(struct runq *, struct kse *, int flags);
int	runq_check(struct runq *);
struct	kse *runq_choose(struct runq *);
void	runq_init(struct runq *);
void	runq_remove(struct runq *, struct kse *);

#endif
