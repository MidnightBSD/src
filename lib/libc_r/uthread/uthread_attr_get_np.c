/*
 * Copyright (c) 2002,2003 Alexey Zelkin <phantom@FreeBSD.org>
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
 *
 * $FreeBSD: src/lib/libc_r/uthread/uthread_attr_get_np.c,v 1.3 2003/02/03 10:08:45 phantom Exp $
 */
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <pthread_np.h>
#include "pthread_private.h"

__weak_reference(_pthread_attr_get_np, pthread_attr_get_np);

int
_pthread_attr_get_np(pthread_t pid, pthread_attr_t *dst)
{
	int	ret;

	if (pid == NULL || dst == NULL || *dst == NULL)
		return (EINVAL);

	if ((ret = _find_thread(pid)) != 0)
		return (ret);

	memcpy(*dst, &pid->attr, sizeof(struct pthread_attr));

	/*
	 * Special case, if stack address was not provided by caller
	 * of pthread_create(), then return address allocated internally
	 */
	if ((*dst)->stackaddr_attr == NULL)
		(*dst)->stackaddr_attr = pid->stack;

	return (0);
}
