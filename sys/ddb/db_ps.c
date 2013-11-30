/*-
 * Copyright (c) 1993 The Regents of the University of California.
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
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/ddb/db_ps.c,v 1.55 2004/11/20 02:32:42 das Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/cons.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <ddb/ddb.h>

static void	dumpthread(volatile struct proc *p, volatile struct thread *td);

void
db_ps(dummy1, dummy2, dummy3, dummy4)
	db_expr_t	dummy1;
	boolean_t	dummy2;
	db_expr_t	dummy3;
	char *		dummy4;
{
	volatile struct proc *p, *pp;
	volatile struct thread *td;
	char *state;
	int np, quit;

	np = nprocs;
	quit = 0;

	/* sx_slock(&allproc_lock); */
	if (!LIST_EMPTY(&allproc))
		p = LIST_FIRST(&allproc);
	else
		p = &proc0;

	db_setup_paging(db_simple_pager, &quit, db_lines_per_page);
	db_printf("  pid   proc     uid  ppid  pgrp  flag   stat  wmesg    wchan  cmd\n");
	while (--np >= 0 && !quit) {
		if (p == NULL) {
			printf("oops, ran out of processes early!\n");
			break;
		}
		/* PROC_LOCK(p); */
		pp = p->p_pptr;
		if (pp == NULL)
			pp = p;


		switch(p->p_state) {
		case PRS_NORMAL:
			if (P_SHOULDSTOP(p))
				state = "stop";
			else
				state = "";
			break;
		case PRS_NEW:
			state = "new ";
			break;
		case PRS_ZOMBIE:
			state = "zomb";
			break;
		default:
			state = "Unkn";
			break;
		}
		db_printf("%5d %8p %4d %5d %5d %07x %s",
		    p->p_pid, (volatile void *)p,
		    p->p_ucred != NULL ? p->p_ucred->cr_ruid : 0, pp->p_pid,
		    p->p_pgrp != NULL ? p->p_pgrp->pg_id : 0, p->p_flag,
		    state);
		if (p->p_flag & P_HADTHREADS)
			db_printf("(threaded)  %s\n", p->p_comm);
		FOREACH_THREAD_IN_PROC(p, td) {
			dumpthread(p, td);
			if (quit)
				break;
		}
		/* PROC_UNLOCK(p); */

		p = LIST_NEXT(p, p_list);
		if (p == NULL && np > 0)
			p = LIST_FIRST(&zombproc);
    	}
	/* sx_sunlock(&allproc_lock); */
}

static void
dumpthread(volatile struct proc *p, volatile struct thread *td)
{

	if (p->p_flag & P_HADTHREADS)
		db_printf( "   thread %p ksegrp %p ", td, td->td_ksegrp);
	if (TD_ON_SLEEPQ(td))
		db_printf("[SLPQ %s %p]", td->td_wmesg, (void *)td->td_wchan);
	switch (td->td_state) {
	case TDS_INHIBITED:
		if (TD_ON_LOCK(td)) {
			db_printf("[LOCK %6s %8p]",
			    td->td_lockname,
			    (void *)td->td_blocked);
		}
		if (TD_IS_SLEEPING(td)) {
			db_printf("[SLP]");
		}  
		if (TD_IS_SWAPPED(td)) {
			db_printf("[SWAP]");
		}
		if (TD_IS_SUSPENDED(td)) {
			db_printf("[SUSP]");
		}
		if (TD_AWAITING_INTR(td)) {
			db_printf("[IWAIT]");
		}
		break;
	case TDS_CAN_RUN:
		db_printf("[Can run]");
		break;
	case TDS_RUNQ:
		db_printf("[RUNQ]");
		break;
	case TDS_RUNNING:
		db_printf("[CPU %d]", td->td_oncpu);
		break;
	case TDS_INACTIVE:
		db_printf("[INACTIVE]");
		break;
	default:
		db_printf("[UNK: %#x]", td->td_state);
	}
	if (p->p_flag & P_HADTHREADS) {
#ifdef KEF_DIDRUN
		if (td->td_kse)
			db_printf("[kse %p]", td->td_kse);
#endif
		db_printf("\n");
	} else
		db_printf(" %s\n", p->p_comm);
}
