/*-
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/geom/geom_kern.c 170307 2007-06-05 00:00:57Z jeff $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/sched.h>
#include <sys/sx.h>
#include <geom/geom.h>
#include <geom/geom_int.h>

MALLOC_DEFINE(M_GEOM, "GEOM", "Geom data structures");

struct sx topology_lock;

static struct proc *g_up_proc;

int g_debugflags;
int g_collectstats = 1;
int g_shutdown;

/*
 * G_UP and G_DOWN are the two threads which push I/O through the
 * stack.
 *
 * Things are procesed in a FIFO order, but these threads could be
 * part of I/O prioritization by deciding which bios/bioqs to service
 * in what order.
 *
 * We have only one thread in each direction, it is belived that until
 * a very non-trivial workload in the UP/DOWN path this will be enough,
 * but more than one can actually be run without problems.
 *
 * Holding the "mymutex" is a debugging feature:  It prevents people
 * from sleeping in the UP/DOWN I/O path by mistake or design (doing
 * so almost invariably result in deadlocks since it stalls all I/O
 * processing in the given direction.
 */

static void
g_up_procbody(void)
{
	struct proc *p = g_up_proc;
	struct thread *tp = FIRST_THREAD_IN_PROC(p);

	mtx_assert(&Giant, MA_NOTOWNED);
	thread_lock(tp);
	sched_prio(tp, PRIBIO);
	thread_unlock(tp);
	for(;;) {
		g_io_schedule_up(tp);
	}
}

static struct kproc_desc g_up_kp = {
	"g_up",
	g_up_procbody,
	&g_up_proc,
};

static struct proc *g_down_proc;

static void
g_down_procbody(void)
{
	struct proc *p = g_down_proc;
	struct thread *tp = FIRST_THREAD_IN_PROC(p);

	mtx_assert(&Giant, MA_NOTOWNED);
	thread_lock(tp);
	sched_prio(tp, PRIBIO);
	thread_unlock(tp);
	for(;;) {
		g_io_schedule_down(tp);
	}
}

static struct kproc_desc g_down_kp = {
	"g_down",
	g_down_procbody,
	&g_down_proc,
};

static struct proc *g_event_proc;

static void
g_event_procbody(void)
{
	struct proc *p = g_event_proc;
	struct thread *tp = FIRST_THREAD_IN_PROC(p);

	mtx_assert(&Giant, MA_NOTOWNED);
	thread_lock(tp);
	sched_prio(tp, PRIBIO);
	thread_unlock(tp);
	for(;;) {
		g_run_events();
		tsleep(&g_wait_event, PRIBIO, "-", hz/10);
	}
}

static struct kproc_desc g_event_kp = {
	"g_event",
	g_event_procbody,
	&g_event_proc,
};

static void
geom_shutdown(void *foo __unused)
{

	g_shutdown = 1;
}

void
g_init(void)
{

	g_trace(G_T_TOPOLOGY, "g_ignition");
	sx_init(&topology_lock, "GEOM topology");
	g_io_init();
	g_event_init();
	g_ctl_init();
	mtx_lock(&Giant);
	kproc_start(&g_event_kp);
	kproc_start(&g_up_kp);
	kproc_start(&g_down_kp);
	mtx_unlock(&Giant);
	EVENTHANDLER_REGISTER(shutdown_pre_sync, geom_shutdown, NULL,
		SHUTDOWN_PRI_FIRST);
}

static int
sysctl_kern_geom_conftxt(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct sbuf *sb;

	sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	g_waitfor_event(g_conftxt, sb, M_WAITOK, NULL);
	error = SYSCTL_OUT(req, sbuf_data(sb), sbuf_len(sb) + 1);
	sbuf_delete(sb);
	return error;
}
 
static int
sysctl_kern_geom_confdot(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct sbuf *sb;

	sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	g_waitfor_event(g_confdot, sb, M_WAITOK, NULL);
	error = SYSCTL_OUT(req, sbuf_data(sb), sbuf_len(sb) + 1);
	sbuf_delete(sb);
	return error;
}
 
static int
sysctl_kern_geom_confxml(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct sbuf *sb;

	sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	g_waitfor_event(g_confxml, sb, M_WAITOK, NULL);
	error = SYSCTL_OUT(req, sbuf_data(sb), sbuf_len(sb) + 1);
	sbuf_delete(sb);
	return error;
}

SYSCTL_NODE(_kern, OID_AUTO, geom, CTLFLAG_RW, 0, "GEOMetry management");

SYSCTL_PROC(_kern_geom, OID_AUTO, confxml, CTLTYPE_STRING|CTLFLAG_RD,
	0, 0, sysctl_kern_geom_confxml, "",
	"Dump the GEOM config in XML");

SYSCTL_PROC(_kern_geom, OID_AUTO, confdot, CTLTYPE_STRING|CTLFLAG_RD,
	0, 0, sysctl_kern_geom_confdot, "",
	"Dump the GEOM config in dot");

SYSCTL_PROC(_kern_geom, OID_AUTO, conftxt, CTLTYPE_STRING|CTLFLAG_RD,
	0, 0, sysctl_kern_geom_conftxt, "",
	"Dump the GEOM config in txt");

TUNABLE_INT("kern.geom.debugflags", &g_debugflags);
SYSCTL_INT(_kern_geom, OID_AUTO, debugflags, CTLFLAG_RW,
	&g_debugflags, 0, "Set various trace levels for GEOM debugging");

SYSCTL_INT(_kern_geom, OID_AUTO, collectstats, CTLFLAG_RW,
	&g_collectstats, 0,
	"Control statistics collection on GEOM providers and consumers");

SYSCTL_INT(_debug_sizeof, OID_AUTO, g_class, CTLFLAG_RD,
	0, sizeof(struct g_class), "sizeof(struct g_class)");
SYSCTL_INT(_debug_sizeof, OID_AUTO, g_geom, CTLFLAG_RD,
	0, sizeof(struct g_geom), "sizeof(struct g_geom)");
SYSCTL_INT(_debug_sizeof, OID_AUTO, g_provider, CTLFLAG_RD,
	0, sizeof(struct g_provider), "sizeof(struct g_provider)");
SYSCTL_INT(_debug_sizeof, OID_AUTO, g_consumer, CTLFLAG_RD,
	0, sizeof(struct g_consumer), "sizeof(struct g_consumer)");
SYSCTL_INT(_debug_sizeof, OID_AUTO, g_bioq, CTLFLAG_RD,
	0, sizeof(struct g_bioq), "sizeof(struct g_bioq)");
