/*	$NetBSD: intr.c,v 1.12 2003/07/15 00:24:41 lukem Exp $	*/

/*-
 * Copyright (c) 2004 Olivier Houchard.
 * Copyright (c) 1994-1998 Mark Brinicombe.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Soft interrupt and other generic interrupt functions.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/sys/arm/arm/intr.c 246000 2013-01-27 20:16:50Z ian $");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/conf.h>
#include <machine/atomic.h>
#include <machine/intr.h>
#include <machine/cpu.h>

#define	INTRNAME_LEN	(MAXCOMLEN + 1)

typedef void (*mask_fn)(void *);

static struct intr_event *intr_events[NIRQ];

void	arm_handler_execute(struct trapframe *, int);

void (*arm_post_filter)(void *) = NULL;

/*
 * Pre-format intrnames into an array of fixed-size strings containing spaces.
 * This allows us to avoid the need for an intermediate table of indices into
 * the names and counts arrays, while still meeting the requirements and
 * assumptions of vmstat(8) and the kdb "show intrcnt" command, the two
 * consumers of this data.
 */
void
arm_intrnames_init(void)
{
	int i;

	for (i = 0; i < NIRQ; ++i) {
		snprintf(&intrnames[i * INTRNAME_LEN], INTRNAME_LEN, "%-*s",
		    INTRNAME_LEN - 1, "");
	}
}

void
arm_setup_irqhandler(const char *name, driver_filter_t *filt,
    void (*hand)(void*), void *arg, int irq, int flags, void **cookiep)
{
	struct intr_event *event;
	int error;

	if (irq < 0 || irq >= NIRQ)
		return;
	event = intr_events[irq];
	if (event == NULL) {
		error = intr_event_create(&event, (void *)irq, 0, irq,
		    (mask_fn)arm_mask_irq, (mask_fn)arm_unmask_irq,
		    arm_post_filter, NULL, "intr%d:", irq);
		if (error)
			return;
		intr_events[irq] = event;
		snprintf(&intrnames[irq * INTRNAME_LEN], INTRNAME_LEN, 
		    "irq%d: %-*s", irq, INTRNAME_LEN - 1, name);
	}
	intr_event_add_handler(event, name, filt, hand, arg,
	    intr_priority(flags), flags, cookiep);
}

int
arm_remove_irqhandler(int irq, void *cookie)
{
	struct intr_event *event;
	int error;

	event = intr_events[irq];
	arm_mask_irq(irq);
	
	error = intr_event_remove_handler(cookie);

	if (!TAILQ_EMPTY(&event->ie_handlers))
		arm_unmask_irq(irq);
	return (error);
}

void dosoftints(void);
void
dosoftints(void)
{
}

void
arm_handler_execute(struct trapframe *frame, int irqnb)
{
	struct intr_event *event;
	int i;

	PCPU_INC(cnt.v_intr);
	i = -1;
	while ((i = arm_get_next_irq(i)) != -1) {
		intrcnt[i]++;
		event = intr_events[i];
		if (intr_event_handle(event, frame) != 0) {
			/* XXX: Log stray IRQs */
			arm_mask_irq(i);
		}
	}
}
