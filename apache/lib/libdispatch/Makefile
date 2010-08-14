# $MidnightBSD: src/cddl/lib/libuutil/Makefile,v 1.2 2008/10/15 19:10:20 laffer1 Exp $

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../../contrib/libdispatch/src
SRCDIR=	${.CURDIR}/../../../contrib/libdispatch/src
SHLIB_MAJOR=	0

LIB=		dispatch
SRCS=	apply.c benchmark.c legacy.c object.c once.c \
	queue.c queue_kevent.c semaphore.c source.c \
	source_kevent.c time.c

.PATH:  ${.CURDIR}/../../../contrib/libdispatch/dispatch
INCS=	base.h dispatch.h group.h object.h once.h \
	queue.h semaphore.h source.h time.h
INCSDIR=	${INCLUDEDIR}/dispatch

CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../../contrib/libdispatch/
CFLAGS+=	-I${.CURDIR}/../../../contrib/libdispatch/src

.PATH: ${.CURDIR}/../../../contrib/libdispatch/man
MAN=	dispatch.3 dispatch_after.3 dispatch_api.3 dispatch_apply.3 \
	dispatch_async.3 dispatch_benchmark.3 dispatch_group_create.3 \
	dispatch_object.3 dispatch_once.3 dispatch_queue_create.3 \
	dispatch_semaphore_create.3 dispatch_source_create.3 dispatch_time.3

MLINKS= dispatch_after.3 dispatch_after_f.3 \
        dispatch_apply.3 dispatch_apply_f.3 \
        dispatch_async.3 dispatch_sync.3 \
        dispatch_async.3 dispatch_async_f.3 \
        dispatch_async.3 dispatch_sync_f.3 \
        dispatch_benchmark.3 dispatch_benchmark_f.3 \
        dispatch_group_create.3 dispatch_group_enter.3 \
        dispatch_group_create.3 dispatch_group_leave.3 \
        dispatch_group_create.3 dispatch_group_wait.3 \
        dispatch_group_create.3 dispatch_group_notify.3 \
        dispatch_group_create.3 dispatch_group_notify_f.3 \
        dispatch_group_create.3 dispatch_group_async.3 \
        dispatch_group_create.3 dispatch_group_async_f.3 \
        dispatch_object.3 dispatch_retain.3 \
        dispatch_object.3 dispatch_release.3 \
        dispatch_object.3 dispatch_suspend.3 \
        dispatch_object.3 dispatch_resume.3 \
        dispatch_object.3 dispatch_get_context.3 \
        dispatch_object.3 dispatch_set_context.3 \
        dispatch_object.3 dispatch_set_finalizer_f.3 \
        dispatch_once.3 dispatch_once_f.3 \
        dispatch_queue_create.3 dispatch_queue_get_label.3 \
        dispatch_queue_create.3 dispatch_get_current_queue.3 \
        dispatch_queue_create.3 dispatch_get_global_queue.3 \
        dispatch_queue_create.3 dispatch_get_main_queue.3 \
        dispatch_queue_create.3 dispatch_main.3 \
        dispatch_queue_create.3 dispatch_set_target_queue.3 \
        dispatch_semaphore_create.3 dispatch_semaphore_signal.3 \
        dispatch_semaphore_create.3 dispatch_semaphore_wait.3 \
        dispatch_source_create.3 dispatch_source_set_event_handler.3 \
        dispatch_source_create.3 dispatch_source_set_event_handler_f.3 \
        dispatch_source_create.3 dispatch_source_set_cancel_handler.3 \
        dispatch_source_create.3 dispatch_source_set_cancel_handler_f.3 \
        dispatch_source_create.3 dispatch_source_cancel.3 \
        dispatch_source_create.3 dispatch_source_testcancel.3 \
        dispatch_source_create.3 dispatch_source_get_handle.3 \
        dispatch_source_create.3 dispatch_source_get_mask.3 \
        dispatch_source_create.3 dispatch_source_get_data.3 \
        dispatch_source_create.3 dispatch_source_merge_data.3 \
        dispatch_source_create.3 dispatch_source_set_timer.3 \
        dispatch_time.3 dispatch_walltime.3

DPADD=          ${PTHREAD_DPADD}
LDADD=          ${PTHREAD_LDADD}

.include <bsd.lib.mk>
