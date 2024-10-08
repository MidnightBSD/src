/*
 * System call argument to DTrace register array converstion.
 *
 * DO NOT EDIT-- this file is automatically @generated.
 * This file is part of the DTrace syscall provider.
 */

static void
systrace_args(int sysnum, void *params, uint64_t *uarg, int *n_args)
{
	int64_t *iarg  = (int64_t *) uarg;
	switch (sysnum) {
	/* cloudabi_sys_clock_res_get */
	case 0: {
		struct cloudabi_sys_clock_res_get_args *p = params;
		iarg[0] = p->clock_id; /* cloudabi_clockid_t */
		*n_args = 1;
		break;
	}
	/* cloudabi_sys_clock_time_get */
	case 1: {
		struct cloudabi_sys_clock_time_get_args *p = params;
		iarg[0] = p->clock_id; /* cloudabi_clockid_t */
		iarg[1] = p->precision; /* cloudabi_timestamp_t */
		*n_args = 2;
		break;
	}
	/* cloudabi_sys_condvar_signal */
	case 2: {
		struct cloudabi_sys_condvar_signal_args *p = params;
		uarg[0] = (intptr_t) p->condvar; /* cloudabi_condvar_t * */
		iarg[1] = p->scope; /* cloudabi_scope_t */
		iarg[2] = p->nwaiters; /* cloudabi_nthreads_t */
		*n_args = 3;
		break;
	}
	/* cloudabi_sys_fd_close */
	case 3: {
		struct cloudabi_sys_fd_close_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		*n_args = 1;
		break;
	}
	/* cloudabi_sys_fd_create1 */
	case 4: {
		struct cloudabi_sys_fd_create1_args *p = params;
		iarg[0] = p->type; /* cloudabi_filetype_t */
		*n_args = 1;
		break;
	}
	/* cloudabi_sys_fd_create2 */
	case 5: {
		struct cloudabi_sys_fd_create2_args *p = params;
		iarg[0] = p->type; /* cloudabi_filetype_t */
		*n_args = 1;
		break;
	}
	/* cloudabi_sys_fd_datasync */
	case 6: {
		struct cloudabi_sys_fd_datasync_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		*n_args = 1;
		break;
	}
	/* cloudabi_sys_fd_dup */
	case 7: {
		struct cloudabi_sys_fd_dup_args *p = params;
		iarg[0] = p->from; /* cloudabi_fd_t */
		*n_args = 1;
		break;
	}
	/* cloudabi32_sys_fd_pread */
	case 8: {
		struct cloudabi32_sys_fd_pread_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		uarg[1] = (intptr_t) p->iovs; /* const cloudabi32_iovec_t * */
		uarg[2] = p->iovs_len; /* size_t */
		iarg[3] = p->offset; /* cloudabi_filesize_t */
		*n_args = 4;
		break;
	}
	/* cloudabi32_sys_fd_pwrite */
	case 9: {
		struct cloudabi32_sys_fd_pwrite_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		uarg[1] = (intptr_t) p->iovs; /* const cloudabi32_ciovec_t * */
		uarg[2] = p->iovs_len; /* size_t */
		iarg[3] = p->offset; /* cloudabi_filesize_t */
		*n_args = 4;
		break;
	}
	/* cloudabi32_sys_fd_read */
	case 10: {
		struct cloudabi32_sys_fd_read_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		uarg[1] = (intptr_t) p->iovs; /* const cloudabi32_iovec_t * */
		uarg[2] = p->iovs_len; /* size_t */
		*n_args = 3;
		break;
	}
	/* cloudabi_sys_fd_replace */
	case 11: {
		struct cloudabi_sys_fd_replace_args *p = params;
		iarg[0] = p->from; /* cloudabi_fd_t */
		iarg[1] = p->to; /* cloudabi_fd_t */
		*n_args = 2;
		break;
	}
	/* cloudabi_sys_fd_seek */
	case 12: {
		struct cloudabi_sys_fd_seek_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		iarg[1] = p->offset; /* cloudabi_filedelta_t */
		iarg[2] = p->whence; /* cloudabi_whence_t */
		*n_args = 3;
		break;
	}
	/* cloudabi_sys_fd_stat_get */
	case 13: {
		struct cloudabi_sys_fd_stat_get_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		uarg[1] = (intptr_t) p->buf; /* cloudabi_fdstat_t * */
		*n_args = 2;
		break;
	}
	/* cloudabi_sys_fd_stat_put */
	case 14: {
		struct cloudabi_sys_fd_stat_put_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		uarg[1] = (intptr_t) p->buf; /* const cloudabi_fdstat_t * */
		iarg[2] = p->flags; /* cloudabi_fdsflags_t */
		*n_args = 3;
		break;
	}
	/* cloudabi_sys_fd_sync */
	case 15: {
		struct cloudabi_sys_fd_sync_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		*n_args = 1;
		break;
	}
	/* cloudabi32_sys_fd_write */
	case 16: {
		struct cloudabi32_sys_fd_write_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		uarg[1] = (intptr_t) p->iovs; /* const cloudabi32_ciovec_t * */
		uarg[2] = p->iovs_len; /* size_t */
		*n_args = 3;
		break;
	}
	/* cloudabi_sys_file_advise */
	case 17: {
		struct cloudabi_sys_file_advise_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		iarg[1] = p->offset; /* cloudabi_filesize_t */
		iarg[2] = p->len; /* cloudabi_filesize_t */
		iarg[3] = p->advice; /* cloudabi_advice_t */
		*n_args = 4;
		break;
	}
	/* cloudabi_sys_file_allocate */
	case 18: {
		struct cloudabi_sys_file_allocate_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		iarg[1] = p->offset; /* cloudabi_filesize_t */
		iarg[2] = p->len; /* cloudabi_filesize_t */
		*n_args = 3;
		break;
	}
	/* cloudabi_sys_file_create */
	case 19: {
		struct cloudabi_sys_file_create_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		uarg[1] = (intptr_t) p->path; /* const char * */
		uarg[2] = p->path_len; /* size_t */
		iarg[3] = p->type; /* cloudabi_filetype_t */
		*n_args = 4;
		break;
	}
	/* cloudabi_sys_file_link */
	case 20: {
		struct cloudabi_sys_file_link_args *p = params;
		iarg[0] = p->fd1; /* cloudabi_lookup_t */
		uarg[1] = (intptr_t) p->path1; /* const char * */
		uarg[2] = p->path1_len; /* size_t */
		iarg[3] = p->fd2; /* cloudabi_fd_t */
		uarg[4] = (intptr_t) p->path2; /* const char * */
		uarg[5] = p->path2_len; /* size_t */
		*n_args = 6;
		break;
	}
	/* cloudabi_sys_file_open */
	case 21: {
		struct cloudabi_sys_file_open_args *p = params;
		iarg[0] = p->dirfd; /* cloudabi_lookup_t */
		uarg[1] = (intptr_t) p->path; /* const char * */
		uarg[2] = p->path_len; /* size_t */
		iarg[3] = p->oflags; /* cloudabi_oflags_t */
		uarg[4] = (intptr_t) p->fds; /* const cloudabi_fdstat_t * */
		*n_args = 5;
		break;
	}
	/* cloudabi_sys_file_readdir */
	case 22: {
		struct cloudabi_sys_file_readdir_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		uarg[1] = (intptr_t) p->buf; /* void * */
		uarg[2] = p->buf_len; /* size_t */
		iarg[3] = p->cookie; /* cloudabi_dircookie_t */
		*n_args = 4;
		break;
	}
	/* cloudabi_sys_file_readlink */
	case 23: {
		struct cloudabi_sys_file_readlink_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		uarg[1] = (intptr_t) p->path; /* const char * */
		uarg[2] = p->path_len; /* size_t */
		uarg[3] = (intptr_t) p->buf; /* char * */
		uarg[4] = p->buf_len; /* size_t */
		*n_args = 5;
		break;
	}
	/* cloudabi_sys_file_rename */
	case 24: {
		struct cloudabi_sys_file_rename_args *p = params;
		iarg[0] = p->fd1; /* cloudabi_fd_t */
		uarg[1] = (intptr_t) p->path1; /* const char * */
		uarg[2] = p->path1_len; /* size_t */
		iarg[3] = p->fd2; /* cloudabi_fd_t */
		uarg[4] = (intptr_t) p->path2; /* const char * */
		uarg[5] = p->path2_len; /* size_t */
		*n_args = 6;
		break;
	}
	/* cloudabi_sys_file_stat_fget */
	case 25: {
		struct cloudabi_sys_file_stat_fget_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		uarg[1] = (intptr_t) p->buf; /* cloudabi_filestat_t * */
		*n_args = 2;
		break;
	}
	/* cloudabi_sys_file_stat_fput */
	case 26: {
		struct cloudabi_sys_file_stat_fput_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		uarg[1] = (intptr_t) p->buf; /* const cloudabi_filestat_t * */
		iarg[2] = p->flags; /* cloudabi_fsflags_t */
		*n_args = 3;
		break;
	}
	/* cloudabi_sys_file_stat_get */
	case 27: {
		struct cloudabi_sys_file_stat_get_args *p = params;
		iarg[0] = p->fd; /* cloudabi_lookup_t */
		uarg[1] = (intptr_t) p->path; /* const char * */
		uarg[2] = p->path_len; /* size_t */
		uarg[3] = (intptr_t) p->buf; /* cloudabi_filestat_t * */
		*n_args = 4;
		break;
	}
	/* cloudabi_sys_file_stat_put */
	case 28: {
		struct cloudabi_sys_file_stat_put_args *p = params;
		iarg[0] = p->fd; /* cloudabi_lookup_t */
		uarg[1] = (intptr_t) p->path; /* const char * */
		uarg[2] = p->path_len; /* size_t */
		uarg[3] = (intptr_t) p->buf; /* const cloudabi_filestat_t * */
		iarg[4] = p->flags; /* cloudabi_fsflags_t */
		*n_args = 5;
		break;
	}
	/* cloudabi_sys_file_symlink */
	case 29: {
		struct cloudabi_sys_file_symlink_args *p = params;
		uarg[0] = (intptr_t) p->path1; /* const char * */
		uarg[1] = p->path1_len; /* size_t */
		iarg[2] = p->fd; /* cloudabi_fd_t */
		uarg[3] = (intptr_t) p->path2; /* const char * */
		uarg[4] = p->path2_len; /* size_t */
		*n_args = 5;
		break;
	}
	/* cloudabi_sys_file_unlink */
	case 30: {
		struct cloudabi_sys_file_unlink_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		uarg[1] = (intptr_t) p->path; /* const char * */
		uarg[2] = p->path_len; /* size_t */
		iarg[3] = p->flags; /* cloudabi_ulflags_t */
		*n_args = 4;
		break;
	}
	/* cloudabi_sys_lock_unlock */
	case 31: {
		struct cloudabi_sys_lock_unlock_args *p = params;
		uarg[0] = (intptr_t) p->lock; /* cloudabi_lock_t * */
		iarg[1] = p->scope; /* cloudabi_scope_t */
		*n_args = 2;
		break;
	}
	/* cloudabi_sys_mem_advise */
	case 32: {
		struct cloudabi_sys_mem_advise_args *p = params;
		uarg[0] = (intptr_t) p->mapping; /* void * */
		uarg[1] = p->mapping_len; /* size_t */
		iarg[2] = p->advice; /* cloudabi_advice_t */
		*n_args = 3;
		break;
	}
	/* cloudabi_sys_mem_map */
	case 33: {
		struct cloudabi_sys_mem_map_args *p = params;
		uarg[0] = (intptr_t) p->addr; /* void * */
		uarg[1] = p->len; /* size_t */
		iarg[2] = p->prot; /* cloudabi_mprot_t */
		iarg[3] = p->flags; /* cloudabi_mflags_t */
		iarg[4] = p->fd; /* cloudabi_fd_t */
		iarg[5] = p->off; /* cloudabi_filesize_t */
		*n_args = 6;
		break;
	}
	/* cloudabi_sys_mem_protect */
	case 34: {
		struct cloudabi_sys_mem_protect_args *p = params;
		uarg[0] = (intptr_t) p->mapping; /* void * */
		uarg[1] = p->mapping_len; /* size_t */
		iarg[2] = p->prot; /* cloudabi_mprot_t */
		*n_args = 3;
		break;
	}
	/* cloudabi_sys_mem_sync */
	case 35: {
		struct cloudabi_sys_mem_sync_args *p = params;
		uarg[0] = (intptr_t) p->mapping; /* void * */
		uarg[1] = p->mapping_len; /* size_t */
		iarg[2] = p->flags; /* cloudabi_msflags_t */
		*n_args = 3;
		break;
	}
	/* cloudabi_sys_mem_unmap */
	case 36: {
		struct cloudabi_sys_mem_unmap_args *p = params;
		uarg[0] = (intptr_t) p->mapping; /* void * */
		uarg[1] = p->mapping_len; /* size_t */
		*n_args = 2;
		break;
	}
	/* cloudabi32_sys_poll */
	case 37: {
		struct cloudabi32_sys_poll_args *p = params;
		uarg[0] = (intptr_t) p->in; /* const cloudabi32_subscription_t * */
		uarg[1] = (intptr_t) p->out; /* cloudabi_event_t * */
		uarg[2] = p->nsubscriptions; /* size_t */
		*n_args = 3;
		break;
	}
	/* cloudabi_sys_proc_exec */
	case 38: {
		struct cloudabi_sys_proc_exec_args *p = params;
		iarg[0] = p->fd; /* cloudabi_fd_t */
		uarg[1] = (intptr_t) p->data; /* const void * */
		uarg[2] = p->data_len; /* size_t */
		uarg[3] = (intptr_t) p->fds; /* const cloudabi_fd_t * */
		uarg[4] = p->fds_len; /* size_t */
		*n_args = 5;
		break;
	}
	/* cloudabi_sys_proc_exit */
	case 39: {
		struct cloudabi_sys_proc_exit_args *p = params;
		iarg[0] = p->rval; /* cloudabi_exitcode_t */
		*n_args = 1;
		break;
	}
	/* cloudabi_sys_proc_fork */
	case 40: {
		*n_args = 0;
		break;
	}
	/* cloudabi_sys_proc_raise */
	case 41: {
		struct cloudabi_sys_proc_raise_args *p = params;
		iarg[0] = p->sig; /* cloudabi_signal_t */
		*n_args = 1;
		break;
	}
	/* cloudabi_sys_random_get */
	case 42: {
		struct cloudabi_sys_random_get_args *p = params;
		uarg[0] = (intptr_t) p->buf; /* void * */
		uarg[1] = p->buf_len; /* size_t */
		*n_args = 2;
		break;
	}
	/* cloudabi32_sys_sock_recv */
	case 43: {
		struct cloudabi32_sys_sock_recv_args *p = params;
		iarg[0] = p->sock; /* cloudabi_fd_t */
		uarg[1] = (intptr_t) p->in; /* const cloudabi32_recv_in_t * */
		uarg[2] = (intptr_t) p->out; /* cloudabi32_recv_out_t * */
		*n_args = 3;
		break;
	}
	/* cloudabi32_sys_sock_send */
	case 44: {
		struct cloudabi32_sys_sock_send_args *p = params;
		iarg[0] = p->sock; /* cloudabi_fd_t */
		uarg[1] = (intptr_t) p->in; /* const cloudabi32_send_in_t * */
		uarg[2] = (intptr_t) p->out; /* cloudabi32_send_out_t * */
		*n_args = 3;
		break;
	}
	/* cloudabi_sys_sock_shutdown */
	case 45: {
		struct cloudabi_sys_sock_shutdown_args *p = params;
		iarg[0] = p->sock; /* cloudabi_fd_t */
		iarg[1] = p->how; /* cloudabi_sdflags_t */
		*n_args = 2;
		break;
	}
	/* cloudabi32_sys_thread_create */
	case 46: {
		struct cloudabi32_sys_thread_create_args *p = params;
		uarg[0] = (intptr_t) p->attr; /* cloudabi32_threadattr_t * */
		*n_args = 1;
		break;
	}
	/* cloudabi_sys_thread_exit */
	case 47: {
		struct cloudabi_sys_thread_exit_args *p = params;
		uarg[0] = (intptr_t) p->lock; /* cloudabi_lock_t * */
		iarg[1] = p->scope; /* cloudabi_scope_t */
		*n_args = 2;
		break;
	}
	/* cloudabi_sys_thread_yield */
	case 48: {
		*n_args = 0;
		break;
	}
	default:
		*n_args = 0;
		break;
	};
}
static void
systrace_entry_setargdesc(int sysnum, int ndx, char *desc, size_t descsz)
{
	const char *p = NULL;
	switch (sysnum) {
	/* cloudabi_sys_clock_res_get */
	case 0:
		switch(ndx) {
		case 0:
			p = "cloudabi_clockid_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_clock_time_get */
	case 1:
		switch(ndx) {
		case 0:
			p = "cloudabi_clockid_t";
			break;
		case 1:
			p = "cloudabi_timestamp_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_condvar_signal */
	case 2:
		switch(ndx) {
		case 0:
			p = "userland cloudabi_condvar_t *";
			break;
		case 1:
			p = "cloudabi_scope_t";
			break;
		case 2:
			p = "cloudabi_nthreads_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_fd_close */
	case 3:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_fd_create1 */
	case 4:
		switch(ndx) {
		case 0:
			p = "cloudabi_filetype_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_fd_create2 */
	case 5:
		switch(ndx) {
		case 0:
			p = "cloudabi_filetype_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_fd_datasync */
	case 6:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_fd_dup */
	case 7:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi32_sys_fd_pread */
	case 8:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "userland const cloudabi32_iovec_t *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "cloudabi_filesize_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi32_sys_fd_pwrite */
	case 9:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "userland const cloudabi32_ciovec_t *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "cloudabi_filesize_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi32_sys_fd_read */
	case 10:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "userland const cloudabi32_iovec_t *";
			break;
		case 2:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_fd_replace */
	case 11:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "cloudabi_fd_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_fd_seek */
	case 12:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "cloudabi_filedelta_t";
			break;
		case 2:
			p = "cloudabi_whence_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_fd_stat_get */
	case 13:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "userland cloudabi_fdstat_t *";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_fd_stat_put */
	case 14:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "userland const cloudabi_fdstat_t *";
			break;
		case 2:
			p = "cloudabi_fdsflags_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_fd_sync */
	case 15:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi32_sys_fd_write */
	case 16:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "userland const cloudabi32_ciovec_t *";
			break;
		case 2:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_file_advise */
	case 17:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "cloudabi_filesize_t";
			break;
		case 2:
			p = "cloudabi_filesize_t";
			break;
		case 3:
			p = "cloudabi_advice_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_file_allocate */
	case 18:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "cloudabi_filesize_t";
			break;
		case 2:
			p = "cloudabi_filesize_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_file_create */
	case 19:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "cloudabi_filetype_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_file_link */
	case 20:
		switch(ndx) {
		case 0:
			p = "cloudabi_lookup_t";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "cloudabi_fd_t";
			break;
		case 4:
			p = "userland const char *";
			break;
		case 5:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_file_open */
	case 21:
		switch(ndx) {
		case 0:
			p = "cloudabi_lookup_t";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "cloudabi_oflags_t";
			break;
		case 4:
			p = "userland const cloudabi_fdstat_t *";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_file_readdir */
	case 22:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "userland void *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "cloudabi_dircookie_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_file_readlink */
	case 23:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "userland char *";
			break;
		case 4:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_file_rename */
	case 24:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "cloudabi_fd_t";
			break;
		case 4:
			p = "userland const char *";
			break;
		case 5:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_file_stat_fget */
	case 25:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "userland cloudabi_filestat_t *";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_file_stat_fput */
	case 26:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "userland const cloudabi_filestat_t *";
			break;
		case 2:
			p = "cloudabi_fsflags_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_file_stat_get */
	case 27:
		switch(ndx) {
		case 0:
			p = "cloudabi_lookup_t";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "userland cloudabi_filestat_t *";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_file_stat_put */
	case 28:
		switch(ndx) {
		case 0:
			p = "cloudabi_lookup_t";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "userland const cloudabi_filestat_t *";
			break;
		case 4:
			p = "cloudabi_fsflags_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_file_symlink */
	case 29:
		switch(ndx) {
		case 0:
			p = "userland const char *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "cloudabi_fd_t";
			break;
		case 3:
			p = "userland const char *";
			break;
		case 4:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_file_unlink */
	case 30:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "userland const char *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "cloudabi_ulflags_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_lock_unlock */
	case 31:
		switch(ndx) {
		case 0:
			p = "userland cloudabi_lock_t *";
			break;
		case 1:
			p = "cloudabi_scope_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_mem_advise */
	case 32:
		switch(ndx) {
		case 0:
			p = "userland void *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "cloudabi_advice_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_mem_map */
	case 33:
		switch(ndx) {
		case 0:
			p = "userland void *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "cloudabi_mprot_t";
			break;
		case 3:
			p = "cloudabi_mflags_t";
			break;
		case 4:
			p = "cloudabi_fd_t";
			break;
		case 5:
			p = "cloudabi_filesize_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_mem_protect */
	case 34:
		switch(ndx) {
		case 0:
			p = "userland void *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "cloudabi_mprot_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_mem_sync */
	case 35:
		switch(ndx) {
		case 0:
			p = "userland void *";
			break;
		case 1:
			p = "size_t";
			break;
		case 2:
			p = "cloudabi_msflags_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_mem_unmap */
	case 36:
		switch(ndx) {
		case 0:
			p = "userland void *";
			break;
		case 1:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi32_sys_poll */
	case 37:
		switch(ndx) {
		case 0:
			p = "userland const cloudabi32_subscription_t *";
			break;
		case 1:
			p = "userland cloudabi_event_t *";
			break;
		case 2:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_proc_exec */
	case 38:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "userland const void *";
			break;
		case 2:
			p = "size_t";
			break;
		case 3:
			p = "userland const cloudabi_fd_t *";
			break;
		case 4:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_proc_exit */
	case 39:
		switch(ndx) {
		case 0:
			p = "cloudabi_exitcode_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_proc_fork */
	case 40:
		break;
	/* cloudabi_sys_proc_raise */
	case 41:
		switch(ndx) {
		case 0:
			p = "cloudabi_signal_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_random_get */
	case 42:
		switch(ndx) {
		case 0:
			p = "userland void *";
			break;
		case 1:
			p = "size_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi32_sys_sock_recv */
	case 43:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "userland const cloudabi32_recv_in_t *";
			break;
		case 2:
			p = "userland cloudabi32_recv_out_t *";
			break;
		default:
			break;
		};
		break;
	/* cloudabi32_sys_sock_send */
	case 44:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "userland const cloudabi32_send_in_t *";
			break;
		case 2:
			p = "userland cloudabi32_send_out_t *";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_sock_shutdown */
	case 45:
		switch(ndx) {
		case 0:
			p = "cloudabi_fd_t";
			break;
		case 1:
			p = "cloudabi_sdflags_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi32_sys_thread_create */
	case 46:
		switch(ndx) {
		case 0:
			p = "userland cloudabi32_threadattr_t *";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_thread_exit */
	case 47:
		switch(ndx) {
		case 0:
			p = "userland cloudabi_lock_t *";
			break;
		case 1:
			p = "cloudabi_scope_t";
			break;
		default:
			break;
		};
		break;
	/* cloudabi_sys_thread_yield */
	case 48:
		break;
	default:
		break;
	};
	if (p != NULL)
		strlcpy(desc, p, descsz);
}
static void
systrace_return_setargdesc(int sysnum, int ndx, char *desc, size_t descsz)
{
	const char *p = NULL;
	switch (sysnum) {
	/* cloudabi_sys_clock_res_get */
	case 0:
		if (ndx == 0 || ndx == 1)
			p = "cloudabi_timestamp_t";
		break;
	/* cloudabi_sys_clock_time_get */
	case 1:
		if (ndx == 0 || ndx == 1)
			p = "cloudabi_timestamp_t";
		break;
	/* cloudabi_sys_condvar_signal */
	case 2:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_fd_close */
	case 3:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_fd_create1 */
	case 4:
		if (ndx == 0 || ndx == 1)
			p = "cloudabi_fd_t";
		break;
	/* cloudabi_sys_fd_create2 */
	case 5:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_fd_datasync */
	case 6:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_fd_dup */
	case 7:
		if (ndx == 0 || ndx == 1)
			p = "cloudabi_fd_t";
		break;
	/* cloudabi32_sys_fd_pread */
	case 8:
		if (ndx == 0 || ndx == 1)
			p = "size_t";
		break;
	/* cloudabi32_sys_fd_pwrite */
	case 9:
		if (ndx == 0 || ndx == 1)
			p = "size_t";
		break;
	/* cloudabi32_sys_fd_read */
	case 10:
		if (ndx == 0 || ndx == 1)
			p = "size_t";
		break;
	/* cloudabi_sys_fd_replace */
	case 11:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_fd_seek */
	case 12:
		if (ndx == 0 || ndx == 1)
			p = "cloudabi_filesize_t";
		break;
	/* cloudabi_sys_fd_stat_get */
	case 13:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_fd_stat_put */
	case 14:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_fd_sync */
	case 15:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi32_sys_fd_write */
	case 16:
		if (ndx == 0 || ndx == 1)
			p = "size_t";
		break;
	/* cloudabi_sys_file_advise */
	case 17:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_file_allocate */
	case 18:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_file_create */
	case 19:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_file_link */
	case 20:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_file_open */
	case 21:
		if (ndx == 0 || ndx == 1)
			p = "cloudabi_fd_t";
		break;
	/* cloudabi_sys_file_readdir */
	case 22:
		if (ndx == 0 || ndx == 1)
			p = "size_t";
		break;
	/* cloudabi_sys_file_readlink */
	case 23:
		if (ndx == 0 || ndx == 1)
			p = "size_t";
		break;
	/* cloudabi_sys_file_rename */
	case 24:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_file_stat_fget */
	case 25:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_file_stat_fput */
	case 26:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_file_stat_get */
	case 27:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_file_stat_put */
	case 28:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_file_symlink */
	case 29:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_file_unlink */
	case 30:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_lock_unlock */
	case 31:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_mem_advise */
	case 32:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_mem_map */
	case 33:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_mem_protect */
	case 34:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_mem_sync */
	case 35:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_mem_unmap */
	case 36:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi32_sys_poll */
	case 37:
		if (ndx == 0 || ndx == 1)
			p = "size_t";
		break;
	/* cloudabi_sys_proc_exec */
	case 38:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_proc_exit */
	case 39:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_proc_fork */
	case 40:
	/* cloudabi_sys_proc_raise */
	case 41:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_random_get */
	case 42:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi32_sys_sock_recv */
	case 43:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi32_sys_sock_send */
	case 44:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_sock_shutdown */
	case 45:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi32_sys_thread_create */
	case 46:
		if (ndx == 0 || ndx == 1)
			p = "cloudabi_tid_t";
		break;
	/* cloudabi_sys_thread_exit */
	case 47:
		if (ndx == 0 || ndx == 1)
			p = "void";
		break;
	/* cloudabi_sys_thread_yield */
	case 48:
	default:
		break;
	};
	if (p != NULL)
		strlcpy(desc, p, descsz);
}
