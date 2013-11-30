/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
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
 * $FreeBSD: src/lib/libc_r/uthread/uthread_readv.c,v 1.16 2007/01/12 07:25:26 imp Exp $
 *
 */
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include "pthread_private.h"

__weak_reference(__readv, readv);

ssize_t
_readv(int fd, const struct iovec * iov, int iovcnt)
{
	struct pthread	*curthread = _get_curthread();
	int	ret;
	int	type;

	/* Lock the file descriptor for read: */
	if ((ret = _FD_LOCK(fd, FD_READ, NULL)) == 0) {
		/* Get the read/write mode type: */
		type = _thread_fd_getflags(fd) & O_ACCMODE;

		/* Check if the file is not open for read: */
		if (type != O_RDONLY && type != O_RDWR) {
			/* File is not open for read: */
			errno = EBADF;
			_FD_UNLOCK(fd, FD_READ);
			return (-1);
		}

		/* Perform a non-blocking readv syscall: */
		while ((ret = __sys_readv(fd, iov, iovcnt)) < 0) {
			if ((_thread_fd_getflags(fd) & O_NONBLOCK) == 0 &&
			    (errno == EWOULDBLOCK || errno == EAGAIN)) {
				curthread->data.fd.fd = fd;
				_thread_kern_set_timeout(NULL);

				/* Reset the interrupted operation flag: */
				curthread->interrupted = 0;

				_thread_kern_sched_state(PS_FDR_WAIT,
				    __FILE__, __LINE__);

				/*
				 * Check if the operation was
				 * interrupted by a signal
				 */
				if (curthread->interrupted) {
					errno = EINTR;
					ret = -1;
					break;
				}
			} else {
				break;
			}
		}
		_FD_UNLOCK(fd, FD_READ);
	}
	return (ret);
}

ssize_t
__readv(int fd, const struct iovec *iov, int iovcnt)
{
	ssize_t ret;

	_thread_enter_cancellation_point();
	ret = _readv(fd, iov, iovcnt);
	_thread_leave_cancellation_point();

	return ret;
}
