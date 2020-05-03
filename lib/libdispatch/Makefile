
.PATH: ${SRCTOP}/contrib/libdispatch/src
SRCDIR=	${SRCTOP}/contrib/libdispatch/src
SHLIB_MAJOR=	0

LIB=		dispatch
SRCS=	apply.c benchmark.c legacy.c object.c once.c \
	queue.c queue_kevent.c semaphore.c source.c \
	source_kevent.c time.c 

WARNS?= 0
CSTD=	gnu99

CFLAGS+=	-fblocks -D__BLOCKS__=1

.PATH: ${SRCTOP}/contrib/libdispatch/src/shims
SRCS+=	tsd.c time_2.c

.PATH:  ${SRCTOP}/contrib/libdispatch/dispatch
INCS=	base.h dispatch.h group.h object.h once.h \
	queue.h semaphore.h source.h time.h
INCSDIR=	${INCLUDEDIR}/dispatch

.if ${MACHINE_ARCH}x == "amd64x" 
CFLAGS+=	-march=athlon64
.endif
.if ${MACHINE_ARCH}x == "i386x"
CFLAGS+=	-march=i586
.endif
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${SRCTOP}/contrib/libdispatch/
CFLAGS+=	-I${SRCTOP}/contrib/libdispatch/src

.PATH: ${SRCTOP}/contrib/libdispatch/man
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

LIBADD+=	pthread blocksruntime

.include <bsd.lib.mk>
