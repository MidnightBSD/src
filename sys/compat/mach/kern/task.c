/*
 * Copyright 1991-1998 by Open Software Foundation, Inc. 
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 */
/*
 * MkLinux
 */
/* CMU_HIST */
/*
 * Revision 2.16.5.3  92/09/15  17:22:54  jeffreyh
 * 	Deadlock fix: Don't hold task lock across pset lock in task_create.
 * 	[92/09/03            dlb]
 * 	Cleanup for profiling.
 * 	[92/07/24            bernadat]
 * 
 * Revision 2.16.5.2  92/05/27  00:46:20  jeffreyh
 * 	Added system calls within MCMSG ifdefs
 * 	[regnier@ssd.intel.com]
 * 
 * Revision 2.16.5.1  92/02/18  19:11:19  jeffreyh
 * 	Fixed Profiling code. [emcmanus@gr.osf.org]
 * 
 * Revision 2.14.2.1  91/09/26  04:48:26  bernadat
 * 	Task profiling fields initialization
 * 	(Bernard Tabib & Andrei Danes @ gr.osf.org)
 * 	[91/09/16            bernadat]
 * 
 * Revision 2.16  91/12/11  08:42:30  jsb
 * 	Fixed assert_wait/thread_wakeup rendezvous in task_assign.
 * 	[91/11/26            rpd]
 * 
 * Revision 2.15  91/11/15  14:11:59  rpd
 * 	NORMA_TASK: initialize new child_node field in task upon creation.
 * 	[91/09/23  09:20:23  jsb]
 * 
 * Revision 2.14  91/06/25  10:29:32  rpd
 * 	Updated convert_thread_to_port usage.
 * 	[91/05/27            rpd]
 * 
 * Revision 2.13  91/06/17  15:47:19  jsb
 * 	Added norma_task hooks. See norma/kern_task.c for code.
 * 	[91/06/17  10:53:30  jsb]
 * 
 * Revision 2.12  91/05/14  16:48:05  mrt
 * 	Correcting copyright
 * 
 * Revision 2.11  91/03/16  14:52:24  rpd
 * 	Can't use thread_dowait on the current thread now.
 * 	[91/01/20            rpd]
 * 
 * Revision 2.10  91/02/05  17:29:55  mrt
 * 	Changed to new Mach copyright
 * 	[91/02/01  16:19:00  mrt]
 * 
 * Revision 2.9  91/01/08  15:17:44  rpd
 * 	Added consider_task_collect, task_collect_scan.
 * 	[91/01/03            rpd]
 * 	Added continuation argument to thread_block.
 * 	[90/12/08            rpd]
 * 
 * Revision 2.8  90/10/25  14:45:26  rwd
 * 	From OSF: Add thread_block() to loop that forcibly terminates
 * 	threads in task_terminate() to fix livelock.  Also hold
 * 	reference to thread when calling thread_force_terminate().
 * 	[90/10/19            rpd]
 * 
 * Revision 2.7  90/06/19  22:59:41  rpd
 * 	Fixed task_info to return the correct base_priority.
 * 	[90/06/18            rpd]
 * 
 * Revision 2.6  90/06/02  14:56:40  rpd
 * 	Moved trap versions of kernel calls to kern/ipc_mig.c.
 * 	[90/05/31            rpd]
 * 
 * 	Removed references to kernel_vm_space, keep_wired_memory.
 * 	[90/04/29            rpd]
 * 	Converted to new IPC and scheduling technology.
 * 	[90/03/26  22:22:19  rpd]
 * 
 * Revision 2.5  90/05/29  18:36:51  rwd
 * 	Added trap versions of task routines from rfr.
 * 	[90/04/20            rwd]
 * 	Add TASK_THREAD_TIMES_INFO flavor to task_info, to get times for
 * 	all live threads.
 * 	[90/04/03            dbg]
 * 
 * 	Use kmem_alloc_wired instead of vm_allocate in task_threads.
 * 	[90/03/28            dbg]
 * 
 * Revision 2.4  90/05/03  15:46:58  dbg
 * 	Add TASK_THREAD_TIMES_INFO flavor to task_info, to get times for
 * 	all live threads.
 * 	[90/04/03            dbg]
 * 
 * 	Use kmem_alloc_wired instead of vm_allocate in task_threads.
 * 	[90/03/28            dbg]
 * 
 * Revision 2.3  90/01/11  11:44:17  dbg
 * 	Removed task_halt (unused).  De-linted.
 * 	[89/12/12            dbg]
 * 
 * Revision 2.2  89/09/08  11:26:37  dbg
 * 	Initialize keep_wired_memory in task_create.
 * 	[89/07/17            dbg]
 * 
 * 19-May-89  David Golub (dbg) at Carnegie-Mellon University
 *	Changed task_info to check for kernel_task, not first_task.
 *
 * 19-Oct-88  David Golub (dbg) at Carnegie-Mellon University
 *	Moved all syscall_emulation routine calls here.  Removed
 *	all non-MACH data structures.  Added routine to create
 *	new tasks running in the kernel.  Changed kernel_task
 *	creation to create it as a normal task.
 *
 * Revision 2.6  88/10/11  10:21:38  rpd
 * 	Changed includes to the new style.
 * 	Rewrote task_threads; the old version could return
 * 	an inconsistent picture of the task.
 * 	[88/10/05  10:28:13  rpd]
 * 
 * Revision 2.5  88/08/06  18:25:53  rpd
 * Changed to use ipc_task_lock/ipc_task_unlock macros.
 * Eliminated use of kern/mach_ipc_defs.h.
 * Enable kernel_task for IPC access.  (See hack in task_by_unix_pid to
 * allow a user to get the kernel_task's port.)
 * Made kernel_task's ref_count > 0, so that task_reference/task_deallocate
 * works on it.  (Previously the task_deallocate would try to destroy it.)
 * 
 * Revision 2.4  88/07/20  16:40:17  rpd
 * Removed task_ports (replaced by port_names).
 * Didn't leave xxx form, because it wasn't implemented.
 * 
 * Revision 2.3  88/07/17  17:55:52  mwyoung
 * Split up uses of task.kernel_only field.  Condensed history.
 * 
 * Revision 2.2.1.1  88/06/28  20:46:20  mwyoung
 * Split up uses of task.kernel_only field.  Condensed history.
 * 
 * 21-Jun-88  Michael Young (mwyoung) at Carnegie-Mellon University.
 *	Split up uses of task.kernel_only field.
 *
 * 21-Jun-88  David Golub (dbg) at Carnegie-Mellon University
 *	Loop in task_terminate to terminate threads was incorrect; if
 *	another component of the system had a reference to the thread,
 *	the thread would remain in the thread_list for the task, and the
 *	loop would never terminate.  Rewrote it to run down the list
 *	like task_hold.  Thread_create terminates new thread if
 *	task_terminate occurs simultaneously.
 *
 * 27-Jan-88  Douglas Orr (dorr) at Carnegie-Mellon University
 *	Init user space library structures.
 *
 * 21-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Task_create no longer returns the data port.  Task_status and
 *	task_set_notify are obsolete (use task_{get,set}_special_port).
 *
 * 21-Jan-88  Karl Hauth (hauth) at Carnegie-Mellon University
 *	task_info(kernel_task, ...) now looks explicitly in the
 *	kernel_map, so it actually returns useful numbers.
 *
 * 17-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Added new task interfaces: task_suspend, task_resume,
 *	task_info, task_get_special_port, task_set_special_port.
 *	Old interfaces remain (temporarily) for binary
 *	compatibility, prefixed with 'xxx_'.
 *
 * 29-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Delinted.
 *
 * 23-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Added task_halt to halt all threads in a task.
 *
 * 15-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Check for null task pointer in task_reference and
 *	task_deallocate.
 *
 *  9-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Removed extra thread reference from task_terminate for new thread
 *	termination code.
 *
 *  8-Dec-87  David Black (dlb) at Carnegie-Mellon University
 *	Added call to ipc_task_disable.
 *
 *  3-Dec-87  David Black (dlb) at Carnegie-Mellon University
 *	Implemented better task termination base on task active field:
 *		1.  task_terminate sets active field to false.
 *		2.  All but the most simple task operations check the
 *			active field and abort if it is false.
 *		3.  task_{hold, dowait, release} now return kern_return_t's.
 *		4.  task_dowait has a second parameter to ignore active
 *			field if called from task_terminate.
 *	Task terminate acquires extra reference to current thread before
 *	terminating it (see thread_terminate()).
 *
 * 19-Nov-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Eliminated TT conditionals.
 *
 * 13-Oct-87  David Black (dlb) at Carnegie-Mellon University
 *	Use counts for suspend and resume primitives.
 *
 * 13-Oct-87  David Golub (dbg) at Carnegie-Mellon University
 *	Added port reference counting to task_set_notify.
 *
 *  5-Oct-87  David Golub (dbg) at Carnegie-Mellon University
 *	Completely replaced old scheduling state machine.
 *
 * 14-Sep-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	De-linted.
 *
 * 25-Aug-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	Must initialize the kernel_task->lock (at least on the Sequent)
 *
 *  6-Aug-87  David Golub (dbg) at Carnegie-Mellon University
 *	Moved ipc_task_terminate to task_terminate, to shut down other
 *	threads that are manipulating the task via its task_port.
 *	Changed task_terminate to terminate all threads in the task.
 *
 * 29-Jul-87  David Golub (dbg) at Carnegie-Mellon University
 *	Fix task_suspend not to hold the task if the task has been
 *	resumed.  Change task_hold/task_wait so that if the current
 *	thread is in the task, it is not held until after all of the
 *	other threads in the task have stopped.  Make task_terminate be
 *	able to terminate the current task.
 *
 *  9-Jul-87  Karl Hauth (hauth) at Carnegie-Mellon University
 *	Modified task_statistics to reflect changes in the structure.
 *
 * 10-Jun-87  Karl Hauth (hauth) at Carnegie-Mellon University
 *	Added code to fill in the task_statistics structure with
 *	zeros and to make mig happier by returning something.
 *
 *  1-Jun-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Added task_statistics stub.
 *
 * 27-Apr-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Move ipc_task_init into task_create; it *should* return
 *	the data port (with a reference) at some point.
 *
 * 20-Apr-87  David Black (dlb) at Carnegie-Mellon University
 *	Fixed task_suspend to ignore multiple suspends.
 *	Fixed task_dowait to work if current thread is in the affected task.
 *
 * 24-Feb-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Rewrote task_suspend/task_hold and added task_wait for new user
 *	synchronization paradigm.
 *
 * 10-Feb-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Add task.kernel_only initialization.
 *
 * 31-Jan-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Merged in my changes for real thread implementation.
 *
 *  7-Nov-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Fixed up stubs for eventual task calls.
 *
 * 30-Sep-86  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Make floating u-area work, add all_task list management.
 *
 * 26-Sep-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added argument to ipc_task_init to get parent.
 *
 *  1-Aug-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added initialization for Mach IPC.
 *
 * 20-Jul-86  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Added kernel_task.
 */
/* CMU_ENDHIST */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 *	File:	kern/task.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young, David Golub,
 *		David Black
 *
 *	Task management primitives implementation.
 */
/*
 * Copyright (c) 1993 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/mach/vm_types.h>

#include <sys/mach/task_info.h>
#include <sys/mach/task_special_ports.h>
#include <sys/mach/mach_types.h>
#include <sys/mach/rpc.h>
#include <sys/mach/ipc/ipc_space.h>
#include <sys/mach/ipc/ipc_entry.h>
#include <sys/mach/mach_param.h>

#include <sys/mach/task.h>
#include <sys/mach/ipc/ipc_kmsg.h>
#include <sys/mach/thread.h>

#include <sys/mach/sched_prim.h>	/* for thread_wakeup */
#include <sys/mach/ipc_tt.h>
#if 0
#include <sys/mach/ledger.h>
#endif
#include <sys/mach/host_special_ports.h>
#include <sys/mach/host.h>
#include <vm/vm_kern.h>		/* for kernel_map, ipc_kernel_map */
#include <vm/uma.h>
#if	MACH_KDB
#include <ddb/db_sym.h>
#endif	/* MACH_KDB */

#include <sys/mach/queue.h>
/*
 * Exported interfaces
 */
#include <sys/mach/task_server.h>
#include <sys/mach/mach_host_server.h>
#include <sys/mach/mach_port_server.h>

struct processor_set default_pset;
unsigned	int sched_ticks;
task_t	kernel_task;
uma_zone_t	task_zone;

/* Forwards */


kern_return_t	task_hold_locked(
			task_t		task);
void		task_wait_locked(
			task_t		task);
kern_return_t	task_release(
			task_t		task);
void		task_act_iterate(
			task_t		 task,
			kern_return_t	(*func)(thread_act_t inc));
void		task_free(
			task_t		task );
void		task_synchronizer_destroy_all(
			task_t		task);


kern_return_t
task_create(
	task_t				parent_task,
	__unused ledger_array_t	ledger_ports,
	__unused mach_msg_type_number_t	num_ledger_ports,
	__unused boolean_t		inherit_memory,
	__unused task_t			*child_task)	/* OUT */
{
	if (parent_task == TASK_NULL)
		return(KERN_INVALID_ARGUMENT);

	return(KERN_FAILURE);
}


static kern_return_t
task_create_internal(
	task_t		parent_task,
	task_t		new_task)
{
	register processor_set_t	pset;

	/* one ref for just being alive; one for our caller */
	new_task->ref_count = 2;
	new_task->semaphores_owned = 0;

	ipc_task_init(new_task, parent_task);

	if (parent_task != TASK_NULL) {
#ifdef notyet		
		pset = parent_task->processor_set;
		if (!pset->active)
			pset = &default_pset;
#endif
		set_security_token(new_task);
		new_task->policy = parent_task->policy;
	} else {
		pset = &default_pset;
		new_task->policy = POLICY_TIMESHARE;
		new_task->sec_token = KERNEL_SECURITY_TOKEN;
		new_task->audit_token = KERNEL_AUDIT_TOKEN;
	}
	ipc_task_enable(new_task);
	return(KERN_SUCCESS);
}



/*
 *	task_free:
 *
 *	Called by task_deallocate when the task's reference count drops to zero.
 *	Task is locked.
 */
void
task_free( register task_t	task )
{
	register processor_set_t pset;

	/* tasks are tied to proc structures so should only be freed if proc goes away */
	task_unlock(task);
	return;
	/*
	 * Temporarily restore the reference we dropped above, then
	 * freeze the task so that the task->processor_set field
	 * cannot change. In the !MACH_HOST case, the logic can be
	 * simplified, since the default_pset is the only pset.
	 */
	++task->ref_count;
	task_unlock(task);
	pset = task->processor_set;
	task_lock(task);
	if (--task->ref_count > 0) {
		/*
		 * A new reference appeared (probably from the pset).
		 * Back out. Must unfreeze inline since we'already
		 * dropped our reference.
		 */
		task_unlock(task);
		return;
	}
	task_unlock(task);
	is_release(task->itk_space);
	uma_zfree(task_zone, task);
}


void
task_deallocate( task_t task )
{
	if (task != TASK_NULL) {
	    int x;
	    task_lock(task);
	    x = --task->ref_count;
	    if (x == 0)
		task_free(task);	/* unlocks task */
	    else
		task_unlock(task);
	}
}

void
task_reference( register task_t task )
{
	if (task != TASK_NULL) {
	    task_lock(task);
	    task->ref_count++;
	    task_unlock(task);
	}
}

/*
 *	task_terminate:
 *
 *	Terminate the specified task.  See comments on thread_terminate
 *	(kern/thread.c) about problems with terminating the "current task."
 */
kern_return_t
task_terminate(
	register task_t	task)
{
#ifdef notyet	
	register thread_t	thread, cur_thread;
#if 0
	register queue_head_t	*list;
#endif	
	register task_t		cur_task;
	thread_act_t		thr_act, cur_thr_act;

	if (task == TASK_NULL)
		return(KERN_INVALID_ARGUMENT);

	assert(task != kernel_task);
#if 0
	list = &task->thr_acts;
	cur_task = current_task();
	cur_thr_act = current_thread()->top_act;
#endif

	/*
	 *	Deactivate task so that it can't be terminated again,
	 *	and so lengthy operations in progress will abort.
	 *
	 *	If the current thread is in this task, remove it from
	 *	the task's thread list to keep the thread-termination
	 *	loop simple.
	 */
	if (task == cur_task) {
		task_lock(task);
		if (!task->active) {
			/*
			 *	Task is already being terminated.
			 */
			task_unlock(task);
			thread_block();
			return(KERN_FAILURE);
		}

		task_hold_locked(task);
#if 0
		task->active = FALSE;

		/*
		 *	Make sure current thread is not being terminated.
		 */
		mutex_lock(&task->act_list_lock);
		cur_thread = act_lock_thread(cur_thr_act);
		if (!cur_thr_act->active) {
			act_unlock_thread(cur_thr_act);
			mutex_unlock(&task->act_list_lock);
			task_unlock(task);
			thread_terminate(cur_thr_act);
			return(KERN_FAILURE);
		}

		/*
		 * make sure that this thread is the last one in the list
		 */
		queue_remove(list, cur_thr_act, thread_act_t, thr_acts);
		queue_enter(list, cur_thr_act, thread_act_t, thr_acts);
		act_unlock_thread(cur_thr_act);
		mutex_unlock(&task->act_list_lock);
		/*
		 *	Shut down this thread's ipc now because it must
		 *	be left alone to terminate the task.
		 */
		ipc_thr_act_disable(cur_thr_act);
		ipc_thr_act_terminate(cur_thr_act);
#endif
	}
	else {
		/*
		 *	Lock both current and victim task to check for
		 *	potential deadlock.
		 */
		if (task < cur_task) {
			task_lock(task);
			task_lock(cur_task);
		}
		else {
			task_lock(cur_task);
			task_lock(task);
		}
		/*
		 *	Check if current thread_act or task is being terminated.
		 */
		cur_thread = act_lock_thread(cur_thr_act);
		if ((!cur_task->active) || (!cur_thr_act->active)) {
			/*
			 * Current task or thread is being terminated.
			 */
			act_unlock_thread(cur_thr_act);
			task_unlock(task);
			task_unlock(cur_task);
			return(KERN_FAILURE);
		}
		act_unlock_thread(cur_thr_act);
		task_unlock(cur_task);

		if (!task->active) {
			/*
			 *	Task is already being terminated.
			 */
			task_unlock(task);
			thread_block();
			return(KERN_FAILURE);
		}
		task_hold_locked(task);
		task->active = FALSE;
	}

	/*
	 *	Prevent further execution of the task.  ipc_task_disable
	 *	prevents further task operations via the task port.
	 *	If this is the current task, the current thread will
	 *	be left running.
	 */
	ipc_task_disable(task);
	task_wait_locked(task);

	/*
	 *	Terminate each thread in the task.  Depending on the
	 *	state of the thread, this can mean a number of things.
	 *	However, we just call thread_terminate(), which
	 *	takes care of all cases (see that code for details).
	 *
         *      The task_port is closed down, so no more thread_create
         *      operations can be done.  Thread_terminate closes the
         *      thread port for each thread; when that is done, the
         *      thread will eventually disappear.  Thus the loop will
         *      terminate.
	 *	Need to call thread_block() inside loop because some
         *      other thread (e.g., the reaper) may have to run to get rid
         *      of all references to the thread; it won't vanish from
         *      the task's thread list until the last one is gone.
         */
        while (!queue_empty(list)) {
                thr_act = (thread_act_t) queue_first(list);
                act_reference(thr_act);
                task_unlock(task);
                thread_terminate(thr_act);
                act_deallocate(thr_act);
                task_lock(task);
        }
        task_unlock(task);
#endif
	/*
	 *	Destroy all synchronizers owned by the task.
	 */
	task_synchronizer_destroy_all(task);

	/*
	 *	Shut down IPC.
	 */
	ipc_task_terminate(task);

	/*
	 *	Deallocate the task's reference to itself.
	 */
	task_deallocate(task);

	return(KERN_SUCCESS);
}

/*
 *	task_hold_locked:
 *
 *	Suspend execution of the specified task.
 *	This is a recursive-style suspension of the task, a count of
 *	suspends is maintained.
 *
 * 	CONDITIONS: the task is locked.
 */
kern_return_t
task_hold_locked(
	register task_t	task)
{
#if 0
	register queue_head_t	*list;
	register thread_act_t	thr_act, cur_thr_act;

	cur_thr_act = current_act();
#endif	

	if (!task->active) {
		return(KERN_FAILURE);
	}
#if 0

	task->suspend_count++;
	/*
	 *	Iterate through all the thread_act's and hold them.
	 *	Do not hold the current thread_act if it is within the
	 *	task.
	 */
	list = &task->thr_acts;
	thr_act = (thread_act_t) queue_first(list);
	while (!queue_end(list, (queue_entry_t) thr_act)) {
		(void)act_lock_thread(thr_act);
		thread_hold(thr_act);
		act_unlock_thread(thr_act);
		thr_act = (thread_act_t) queue_next(&thr_act->thr_acts);
	}
#endif	
	return(KERN_SUCCESS);
}


kern_return_t
task_release(
	register task_t	task)
{
#if 0
	register queue_head_t	*list;
	register thread_act_t	thr_act, next;

	task_lock(task);
	if (!task->active) {
		task_unlock(task);
		return(KERN_FAILURE);
	}

	task->suspend_count--;

	/*
	 *	Iterate through all the thread_act's and release them.
	 */
	list = &task->thr_acts;
	thr_act = (thread_act_t) queue_first(list);
	while (!queue_end(list, (queue_entry_t) thr_act)) {
		next = (thread_act_t) queue_next(&thr_act->thr_acts);
		(void)act_lock_thread(thr_act);
		thread_release(thr_act);
		act_unlock_thread(thr_act);
		thr_act = next;
	}
	task_unlock(task);
#endif	
	return(KERN_SUCCESS);
}

kern_return_t
task_threads(
	task_t			task,
	thread_act_array_t	*thr_act_list,
	mach_msg_type_number_t	*count)
{
#if 0
	unsigned int		actual;	/* this many thr_acts */
	thread_act_t		thr_act;
	thread_act_t		*thr_acts;
	thread_t		thread;
	int			i, j;
	boolean_t rt = FALSE; /* ### This boolean is FALSE, because there
			       * currently exists no mechanism to determine
			       * whether or not the reply port is an RT port
			       */


	vm_size_t size, size_needed;
	vm_offset_t addr;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	size = 0; addr = 0;

	for (;;) {
		task_lock(task);
		if (!task->active) {
			task_unlock(task);
			if (size != 0)
				KFREE(addr, size, rt);
			return KERN_FAILURE;
		}

		actual = task->thr_act_count;

		/* do we have the memory we need? */
		size_needed = actual * sizeof(mach_port_t);
		if (size_needed <= size)
			break;

		/* unlock the task and allocate more memory */
		task_unlock(task);

		if (size != 0)
			KFREE(addr, size, rt);

		assert(size_needed > 0);
		size = size_needed;

		addr = KALLOC(size, rt);
		if (addr == 0)
			return KERN_RESOURCE_SHORTAGE;
	}

	/* OK, have memory and the task is locked & active */
	thr_acts = (thread_act_t *) addr;

	for (i = j = 0, thr_act = (thread_act_t) queue_first(&task->thr_acts);
	     i < actual;
	     i++, thr_act = (thread_act_t) queue_next(&thr_act->thr_acts)) {
		act_reference(thr_act);
		thr_acts[j++] = thr_act;
	}
	assert(queue_end(&task->thr_acts, (queue_entry_t) thr_act));
	actual = j;

	/* can unlock task now that we've got the thr_act refs */
	task_unlock(task);

	if (actual == 0) {
		/* no thr_acts, so return null pointer and deallocate memory */

		*thr_act_list = 0;
		*count = 0;

		if (size != 0)
			KFREE(addr, size, rt);
	} else {
		/* if we allocated too much, must copy */

		if (size_needed < size) {
			vm_offset_t newaddr;

			newaddr = KALLOC(size_needed, rt);
			if (newaddr == 0) {
				for (i = 0; i < actual; i++)
					act_deallocate(thr_acts[i]);
				KFREE(addr, size, rt);
				return KERN_RESOURCE_SHORTAGE;
			}

			bcopy((char *) addr, (char *) newaddr, size_needed);
			KFREE(addr, size, rt);
			thr_acts = (thread_act_t *) newaddr;
		}

		*thr_act_list = (mach_port_t *) thr_acts;
		*count = actual;

		/* do the conversion that Mig should handle */

		for (i = 0; i < actual; i++)
			((ipc_port_t *) thr_acts)[i] =
				convert_act_to_port(thr_acts[i]);
	}
#endif
	return KERN_SUCCESS;
}

kern_return_t
task_suspend(
	register task_t		task)
{
#if 0
	if (task == TASK_NULL)
		return (KERN_INVALID_ARGUMENT);

	task_lock(task);
	if (!task->active) {
		task_unlock(task);
		return (KERN_FAILURE);
	}
	if ((task->user_stop_count)++ > 0) {
		/*
		 *	If the stop count was positive, the task is
		 *	already stopped and we can exit.
		 */
		task_unlock(task);
		return (KERN_SUCCESS);
	}

	/*
	 *	Hold all of the threads in the task, and wait for
	 *	them to stop.  If the current thread is within
	 *	this task, hold it separately so that all of the
	 *	other threads can stop first.
	 */
	if (task_hold_locked(task) != KERN_SUCCESS) {
		task_unlock(task);
		return (KERN_FAILURE);
	}

	task_wait_locked(task);
	task_unlock(task);
#endif
	return (KERN_SUCCESS);
}

/*
 * Wait for all threads in task to stop.  Called with task locked.
 */
void
task_wait_locked(
	register task_t		task)
{
	#if 0
	register queue_head_t	*list;
	register thread_act_t	thr_act, refd_thr_act;
	register thread_t	thread, cur_thr;

	cur_thr = current_thread();
	/*
	 *	Iterate through all the thread's and wait for them to
	 *	stop.  Do not wait for the current thread if it is within
	 *	the task.
	 */
	list = &task->thr_acts;
	refd_thr_act = THR_ACT_NULL;
	while (1) {
		thr_act = (thread_act_t) queue_first(list);
		while (!queue_end(list, (queue_entry_t) thr_act)) {
			thread = act_lock_thread(thr_act);
			if (refd_thr_act != THR_ACT_NULL) {
				act_deallocate(refd_thr_act);
				refd_thr_act = THR_ACT_NULL;
			}
			if (thread &&
				thr_act == thread->top_act && thread != cur_thr) {
				refd_thr_act = thr_act;
				act_locked_act_reference(thr_act);
				act_unlock_thread(thr_act);
				task_unlock(task);
				(void)thread_wait(thread);
				task_lock(task);
				thread = act_lock_thread(thr_act);
				if (!thr_act->active) {
					act_unlock_thread(thr_act);
					break;
				}
			}
			act_unlock_thread(thr_act);
			thr_act = (thread_act_t) queue_next(&thr_act->thr_acts);
		}
	    	if (queue_end(list, (queue_entry_t)thr_act))
			break;
	}
	if (refd_thr_act != THR_ACT_NULL) {
		act_deallocate(refd_thr_act);
		refd_thr_act = THR_ACT_NULL;
	}
#endif
}

kern_return_t 
task_resume(register task_t task)
{
	register boolean_t	release;

	if (task == TASK_NULL)
		return(KERN_INVALID_ARGUMENT);

	release = FALSE;
#if 0	
	task_lock(task);
	if (!task->active) {
		task_unlock(task);
		return(KERN_FAILURE);
	}
	if (task->user_stop_count > 0) {
		if (--(task->user_stop_count) == 0)
	    		release = TRUE;
	}
	else {
		task_unlock(task);
		return(KERN_FAILURE);
	}
	task_unlock(task);
#endif
	/*
	 *	Release the task if necessary.
	 */
	if (release)
		return(task_release(task));

	return(KERN_SUCCESS);
}

kern_return_t
task_set_info(
	task_t		task,
	task_flavor_t	flavor,
	task_info_t	task_info_in __unused,		/* pointer to IN array */
	mach_msg_type_number_t	task_info_count __unused)
{

	if (task == TASK_NULL)
		return(KERN_INVALID_ARGUMENT);

	switch (flavor) {
	    default:
			return (KERN_INVALID_ARGUMENT);
	}
	return (KERN_SUCCESS);
}

kern_return_t
task_info(
	task_t			task,
	task_flavor_t		flavor,
	task_info_t		task_info_out,
	mach_msg_type_number_t	*task_info_count)
{

	if (task == TASK_NULL)
		return(KERN_INVALID_ARGUMENT);

	switch (flavor) {
	    case TASK_BASIC_INFO:
	    {
		register task_basic_info_t	basic_info;

		if (*task_info_count < TASK_BASIC_INFO_COUNT) {
		    return(KERN_INVALID_ARGUMENT);
		}

		basic_info = (task_basic_info_t) task_info_out;
#ifdef notyet
		map = (task == kernel_task) ? kernel_map : task->map;

		basic_info->virtual_size  = map->size;
		basic_info->resident_size = pmap_resident_count(map->pmap)
						   * PAGE_SIZE;
		task_lock(task);
		basic_info->policy = task->policy;
		basic_info->suspend_count = task->user_stop_count;
		basic_info->user_time.seconds
				= task->total_user_time.seconds;
		basic_info->user_time.microseconds
				= task->total_user_time.microseconds;
		basic_info->system_time.seconds
				= task->total_system_time.seconds;
		basic_info->system_time.microseconds 
				= task->total_system_time.microseconds;
		task_unlock(task);
#endif

		*task_info_count = TASK_BASIC_INFO_COUNT;
		break;
	    }

	    case TASK_THREAD_TIMES_INFO:
	    {
		register task_thread_times_info_t times_info;

		if (*task_info_count < TASK_THREAD_TIMES_INFO_COUNT) {
		    return (KERN_INVALID_ARGUMENT);
		}

		times_info = (task_thread_times_info_t) task_info_out;
		times_info->user_time.seconds = 0;
		times_info->user_time.microseconds = 0;
		times_info->system_time.seconds = 0;
		times_info->system_time.microseconds = 0;

#ifdef notyet
		task_lock(task);
		queue_iterate(&task->thr_acts, thr_act,
			      thread_act_t, thr_acts)
		{
			thread_t thread;
		    time_value_t user_time, system_time;
		    spl_t	 s;

		    thread = act_lock_thread(thr_act);

		    /* Skip empty threads and threads that have migrated
		     * into this task:
		     */
		    if (thr_act->ith_object) {
				act_unlock_thread(thr_act);
				continue;
		    }
		    assert(thread);	/* Must have thread, if no thread_pool*/
		    s = splsched();
		    thread_lock(thread);

		    thread_read_times(thread, &user_time, &system_time);

		    thread_unlock(thread);
		    splx(s);
		    act_unlock_thread(thr_act);

		    time_value_add(&times_info->user_time, &user_time);
		    time_value_add(&times_info->system_time, &system_time);
		}
		task_unlock(task);
#endif
		*task_info_count = TASK_THREAD_TIMES_INFO_COUNT;
		break;
	    }

	    case TASK_SCHED_FIFO_INFO:
	    {
		register policy_fifo_base_t	fifo_base;

		if (*task_info_count < POLICY_FIFO_BASE_COUNT)
			return(KERN_INVALID_ARGUMENT);

		fifo_base = (policy_fifo_base_t) task_info_out;

		task_lock(task);
		if (task->policy != POLICY_FIFO) {
			task_unlock(task);
			return(KERN_INVALID_POLICY);
		}
		fifo_base->base_priority = task->priority;
		task_unlock(task);

		*task_info_count = POLICY_FIFO_BASE_COUNT;
		break;
	    }

	    case TASK_SCHED_RR_INFO:
	    {
		register policy_rr_base_t	rr_base;

		if (*task_info_count < POLICY_RR_BASE_COUNT)
			return(KERN_INVALID_ARGUMENT);

		rr_base = (policy_rr_base_t) task_info_out;

		task_lock(task);
		if (task->policy != POLICY_RR) {
			task_unlock(task);
			return(KERN_INVALID_POLICY);
		}
		rr_base->base_priority = task->priority;
		rr_base->quantum = (task->sched_data * ticks)/1000;
		task_unlock(task);

		*task_info_count = POLICY_RR_BASE_COUNT;
		break;
	    }

	    case TASK_SCHED_TIMESHARE_INFO:
	    {
		register policy_timeshare_base_t	ts_base;

		if (*task_info_count < POLICY_TIMESHARE_BASE_COUNT)
			return(KERN_INVALID_ARGUMENT);

		ts_base = (policy_timeshare_base_t) task_info_out;

		task_lock(task);
		if (task->policy != POLICY_TIMESHARE) {
			task_unlock(task);
			return(KERN_INVALID_POLICY);
		}
		ts_base->base_priority = task->priority;
		task_unlock(task);

		*task_info_count = POLICY_TIMESHARE_BASE_COUNT;
		break;
	    }

            case TASK_SECURITY_TOKEN:
	    {
                register security_token_t	*sec_token_p;

		if (*task_info_count < TASK_SECURITY_TOKEN_COUNT) {
		    return(KERN_INVALID_ARGUMENT);
		}

		sec_token_p = (security_token_t *) task_info_out;

		task_lock(task);
		*sec_token_p = task->sec_token;
		task_unlock(task);

		*task_info_count = TASK_SECURITY_TOKEN_COUNT;
                break;
            }
            
	    default:
		return (KERN_INVALID_ARGUMENT);
	}

	return(KERN_SUCCESS);
}

/*
 *	task_assign:
 *
 *	Change the assigned processor set for the task
 */
kern_return_t
task_assign(
	task_t				task __unused,
	processor_set_t			new_pset __unused,
	boolean_t			assign_threads __unused)
{

	return (KERN_FAILURE);
}

/*
 *	task_assign_default:
 *
 *	Version of task_assign to assign to default processor set.
 */
kern_return_t
task_assign_default(
	task_t		task,
	boolean_t	assign_threads)
{
    return (task_assign(task, &default_pset, assign_threads));
}

/*
 *	task_get_assignment
 *
 *	Return name of processor set that task is assigned to.
 */
kern_return_t
task_get_assignment(
	task_t		task,
	processor_set_t	*pset)
{
	if (!task->active)
		return(KERN_FAILURE);

	*pset = task->processor_set;
	pset_reference(*pset);
	return(KERN_SUCCESS);
}

/*
 * 	task_policy
 *
 *	Set scheduling policy and parameters, both base and limit, for
 *	the given task. Policy must be a policy which is enabled for the
 *	processor set. Change contained threads if requested. 
 */
kern_return_t
task_policy(
	task_t			task,
        policy_t		policy,
        policy_base_t		base,
	mach_msg_type_number_t	count,
        boolean_t		set_limit,
        boolean_t		change)
{

	return (KERN_FAILURE);
}

kern_return_t
task_set_policy(
	task_t			task __unused,
	processor_set_t		pset __unused,
	policy_t		policy __unused,
	policy_base_t		base __unused,
	mach_msg_type_number_t	base_count __unused,
	policy_limit_t		limit __unused,
	mach_msg_type_number_t	limit_count __unused,
	boolean_t		change __unused)
{

	return (KERN_FAILURE);
}


kern_return_t
task_set_ras_pc(
 	task_t		task __unused,
 	vm_offset_t	pc __unused,
 	vm_offset_t	endpc __unused)
{

	return (KERN_FAILURE);
}

void
task_synchronizer_destroy_all(task_t task)
{
	semaphore_t	semaphore;

	/*
	 *  Destroy owned semaphores
	 */

	while (!queue_empty(&task->semaphore_list)) {
		semaphore = (semaphore_t) queue_first(&task->semaphore_list);
		(void) semaphore_destroy(task, semaphore);
	}
}

static long task_uniqueid;

static void
mach_task_init(void *arg __unused, struct proc *p)
{
	task_t task;

	p->p_machdata = task = uma_zalloc(task_zone, M_WAITOK|M_ZERO);
	task->itk_p = p;

	mach_mutex_init(&task->lock, "ETAP_THREAD_TASK_NEW");
	mach_mutex_init(&task->itk_lock_data, "ETAP_THREAD_TASK_ITK");
	queue_init(&task->semaphore_list);

	if (p == &proc0) {
		kernel_task = task;
		task_create_internal(TASK_NULL, task);
	}
}

static void
mach_task_fork(void *arg __unused, struct proc *p1, struct proc *p2, int flags __unused)
{
	task_t task = p2->p_machdata;
	task_t parent_task = p1->p_machdata;

	atomic_add_long(&task_uniqueid, 1);
	task->itk_uniqueid = task_uniqueid;
	task->itk_puniqueid = parent_task->itk_uniqueid;
	task_create_internal(parent_task, task);
}

static int
uma_task_init(void *_thread, int a, int b)
{
	/* allocate task substructures */
	return (0);
}

static void
uma_task_fini(void *_thread, int a)
{
	/* deallocate task substructures */
}


static void
task_sysinit(void *arg __unused)
{
	task_zone = uma_zcreate("mach_task_zone",
							sizeof(struct mach_task),
							NULL, NULL, uma_task_init,
							uma_task_fini, 1, 0);

	EVENTHANDLER_REGISTER(process_init, mach_task_init, NULL, EVENTHANDLER_PRI_ANY);
	EVENTHANDLER_REGISTER(process_fork, mach_task_fork, NULL, EVENTHANDLER_PRI_ANY);
}

/* before SI_SUB_INTRINSIC and after SI_SUB_EVENTHANDLER */
SYSINIT(mach_thread, SI_SUB_KLD, SI_ORDER_ANY, task_sysinit, NULL);
