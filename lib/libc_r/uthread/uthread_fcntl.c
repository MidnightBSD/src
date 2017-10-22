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
 * $FreeBSD: release/7.0.0/lib/libc_r/uthread/uthread_fcntl.c 165967 2007-01-12 07:26:21Z imp $
 */
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include "pthread_private.h"

__weak_reference(__fcntl, fcntl);

int
_fcntl(int fd, int cmd,...)
{
	int             flags = 0;
	int		nonblock;
	int             oldfd;
	int             ret;
	va_list         ap;

	/* Lock the file descriptor: */
	if ((ret = _FD_LOCK(fd, FD_RDWR, NULL)) == 0) {
		/* Initialise the variable argument list: */
		va_start(ap, cmd);

		/* Process according to file control command type: */
		switch (cmd) {
		/* Duplicate a file descriptor: */
		case F_DUPFD:
			/*
			 * Get the file descriptor that the caller wants to
			 * use: 
			 */
			oldfd = va_arg(ap, int);

			/* Initialise the file descriptor table entry: */
			if ((ret = __sys_fcntl(fd, cmd, oldfd)) < 0) {
			}
			/* Initialise the file descriptor table entry: */
			else if (_thread_fd_table_init(ret) != 0) {
				/* Quietly close the file: */
				__sys_close(ret);

				/* Reset the file descriptor: */
				ret = -1;
			} else {
				/*
				 * Save the file open flags so that they can
				 * be checked later: 
				 */
				_thread_fd_setflags(ret,
				    _thread_fd_getflags(fd));
			}
			break;
		case F_SETFD:
			flags = va_arg(ap, int);
			ret = __sys_fcntl(fd, cmd, flags);
			break;
		case F_GETFD:
			ret = __sys_fcntl(fd, cmd, 0);
			break;
		case F_GETFL:
			ret = _thread_fd_getflags(fd);
			break;
		case F_SETFL:
			/*
			 * Get the file descriptor flags passed by the
			 * caller:
			 */
			flags = va_arg(ap, int);

			/*
			 * Check if the user wants a non-blocking file
			 * descriptor:
			 */
			nonblock = flags & O_NONBLOCK;

			/* Set the file descriptor flags: */
			if ((ret = __sys_fcntl(fd, cmd, flags | O_NONBLOCK)) != 0) {

			/* Get the flags so that we behave like the kernel: */
			} else if ((flags = __sys_fcntl(fd,
			    F_GETFL, 0)) == -1) {
				/* Error getting flags: */
				ret = -1;

			/*
			 * Check if the file descriptor is non-blocking
			 * with respect to the user:
			 */
			} else if (nonblock)
				/* A non-blocking descriptor: */
				_thread_fd_setflags(fd, flags | O_NONBLOCK);
			else
				/* Save the flags: */
				_thread_fd_setflags(fd, flags & ~O_NONBLOCK);
			break;
		default:
			/* Might want to make va_arg use a union */
			ret = __sys_fcntl(fd, cmd, va_arg(ap, void *));
			break;
		}

		/* Free variable arguments: */
		va_end(ap);

		/* Unlock the file descriptor: */
		_FD_UNLOCK(fd, FD_RDWR);
	}
	/* Return the completion status: */
	return (ret);
}

int
__fcntl(int fd, int cmd,...)
{
	int	ret;
	va_list	ap;
	
	_thread_enter_cancellation_point();

	va_start(ap, cmd);
	switch (cmd) {
		case F_DUPFD:
		case F_SETFD:
		case F_SETFL:
			ret = _fcntl(fd, cmd, va_arg(ap, int));
			break;
		case F_GETFD:
		case F_GETFL:
			ret = _fcntl(fd, cmd);
			break;
		default:
			ret = _fcntl(fd, cmd, va_arg(ap, void *));
	}
	va_end(ap);

	_thread_leave_cancellation_point();

	return ret;
}
