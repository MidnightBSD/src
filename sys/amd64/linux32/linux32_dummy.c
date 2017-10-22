/*-
 * Copyright (c) 1994-1995 S�ren Schmidt
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <amd64/linux32/linux.h>
#include <amd64/linux32/linux32_proto.h>
#include <compat/linux/linux_util.h>

DUMMY(stime);
DUMMY(olduname);
DUMMY(syslog);
DUMMY(uname);
DUMMY(vhangup);
DUMMY(swapoff);
DUMMY(adjtimex);
DUMMY(create_module);
DUMMY(init_module);
DUMMY(delete_module);
DUMMY(get_kernel_syms);
DUMMY(quotactl);
DUMMY(bdflush);
DUMMY(sysfs);
DUMMY(query_module);
DUMMY(nfsservctl);
DUMMY(rt_sigqueueinfo);
DUMMY(sendfile);
DUMMY(setfsuid);
DUMMY(setfsgid);
DUMMY(pivot_root);
DUMMY(mincore);
DUMMY(ptrace);
DUMMY(lookup_dcookie);
DUMMY(epoll_create);
DUMMY(epoll_ctl);
DUMMY(epoll_wait);
DUMMY(remap_file_pages);
DUMMY(timer_create);
DUMMY(timer_settime);
DUMMY(timer_gettime);
DUMMY(timer_getoverrun);
DUMMY(timer_delete);
DUMMY(fstatfs64);
DUMMY(mbind);
DUMMY(get_mempolicy);
DUMMY(set_mempolicy);
DUMMY(mq_open);
DUMMY(mq_unlink);
DUMMY(mq_timedsend);
DUMMY(mq_timedreceive);
DUMMY(mq_notify);
DUMMY(mq_getsetattr);
DUMMY(kexec_load);
DUMMY(waitid);
/* linux 2.6.11: */
DUMMY(add_key);
DUMMY(request_key);
DUMMY(keyctl);
/* linux 2.6.13: */
DUMMY(ioprio_set);
DUMMY(ioprio_get);
DUMMY(inotify_init);
DUMMY(inotify_add_watch);
DUMMY(inotify_rm_watch);
/* linux 2.6.16: */
DUMMY(migrate_pages);
DUMMY(pselect6);
DUMMY(ppoll);
DUMMY(unshare);
/* linux 2.6.17: */
DUMMY(splice);
DUMMY(sync_file_range);
DUMMY(tee);
DUMMY(vmsplice);
/* linux 2.6.18: */
DUMMY(move_pages);
/* linux 2.6.19: */
DUMMY(getcpu);
DUMMY(epoll_pwait);
/* linux 2.6.22: */
DUMMY(utimensat);
DUMMY(signalfd);
DUMMY(timerfd_create);
DUMMY(eventfd);
/* linux 2.6.23: */
DUMMY(fallocate);
/* linux 2.6.25: */
DUMMY(timerfd_settime);
DUMMY(timerfd_gettime);
/* linux 2.6.27: */
DUMMY(signalfd4);
DUMMY(eventfd2);
DUMMY(epoll_create1);
DUMMY(dup3);
DUMMY(pipe2);
DUMMY(inotify_init1);
/* linux 2.6.30: */
DUMMY(preadv);
DUMMY(pwritev);
/* linux 2.6.31: */
DUMMY(rt_tsigqueueinfo);
DUMMY(perf_event_open);
/* linux 2.6.33: */
DUMMY(recvmmsg);
DUMMY(fanotify_init);
DUMMY(fanotify_mark);
/* linux 2.6.36: */
DUMMY(prlimit64);
/* later: */
DUMMY(name_to_handle_at);
DUMMY(open_by_handle_at);
DUMMY(clock_adjtime);
DUMMY(syncfs);
DUMMY(sendmmsg);
DUMMY(setns);
DUMMY(process_vm_readv);
DUMMY(process_vm_writev);

#define DUMMY_XATTR(s)						\
int								\
linux_ ## s ## xattr(						\
    struct thread *td, struct linux_ ## s ## xattr_args *arg)	\
{								\
								\
	return (ENOATTR);					\
}
DUMMY_XATTR(set);
DUMMY_XATTR(lset);
DUMMY_XATTR(fset);
DUMMY_XATTR(get);
DUMMY_XATTR(lget);
DUMMY_XATTR(fget);
DUMMY_XATTR(list);
DUMMY_XATTR(llist);
DUMMY_XATTR(flist);
DUMMY_XATTR(remove);
DUMMY_XATTR(lremove);
DUMMY_XATTR(fremove);
