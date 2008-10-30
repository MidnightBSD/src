/*
 * Copyright (c) 1995 John Birrell <jb@cimlogic.com.au>.
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
 * $FreeBSD: src/lib/libc_r/uthread/uthread_spec.c,v 1.18 2007/01/12 07:25:26 imp Exp $
 */
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "pthread_private.h"

struct pthread_key {
	spinlock_t	lock;
	volatile int	allocated;
	volatile int	count;
	int		seqno;
	void            (*destructor) ();
};

/* Static variables: */
static	struct pthread_key key_table[PTHREAD_KEYS_MAX];

__weak_reference(_pthread_key_create, pthread_key_create);
__weak_reference(_pthread_key_delete, pthread_key_delete);
__weak_reference(_pthread_getspecific, pthread_getspecific);
__weak_reference(_pthread_setspecific, pthread_setspecific);


int
_pthread_key_create(pthread_key_t * key, void (*destructor) (void *))
{
	for ((*key) = 0; (*key) < PTHREAD_KEYS_MAX; (*key)++) {
		/* Lock the key table entry: */
		_SPINLOCK(&key_table[*key].lock);

		if (key_table[(*key)].allocated == 0) {
			key_table[(*key)].allocated = 1;
			key_table[(*key)].destructor = destructor;
			key_table[(*key)].seqno++;

			/* Unlock the key table entry: */
			_SPINUNLOCK(&key_table[*key].lock);
			return (0);
		}

		/* Unlock the key table entry: */
		_SPINUNLOCK(&key_table[*key].lock);
	}
	return (EAGAIN);
}

int
_pthread_key_delete(pthread_key_t key)
{
	int ret = 0;

	if (key < PTHREAD_KEYS_MAX) {
		/* Lock the key table entry: */
		_SPINLOCK(&key_table[key].lock);

		if (key_table[key].allocated)
			key_table[key].allocated = 0;
		else
			ret = EINVAL;

		/* Unlock the key table entry: */
		_SPINUNLOCK(&key_table[key].lock);
	} else
		ret = EINVAL;
	return (ret);
}

void 
_thread_cleanupspecific(void)
{
	struct pthread	*curthread = _get_curthread();
	void		*data = NULL;
	int		key;
	int		itr;
	void		(*destructor)( void *);

	for (itr = 0; itr < PTHREAD_DESTRUCTOR_ITERATIONS; itr++) {
		for (key = 0; key < PTHREAD_KEYS_MAX; key++) {
			if (curthread->specific_data_count > 0) {
				/* Lock the key table entry: */
				_SPINLOCK(&key_table[key].lock);
				destructor = NULL;

				if (key_table[key].allocated &&
				    (curthread->specific[key].data != NULL)) {
					if (curthread->specific[key].seqno ==
					    key_table[key].seqno) {
						data = (void *) curthread->specific[key].data;
						destructor = key_table[key].destructor;
					}
					curthread->specific[key].data = NULL;
					curthread->specific_data_count--;
				}

				/* Unlock the key table entry: */
				_SPINUNLOCK(&key_table[key].lock);

				/*
				 * If there is a destructore, call it
				 * with the key table entry unlocked:
				 */
				if (destructor)
					destructor(data);
			} else {
				free(curthread->specific);
				curthread->specific = NULL;
				return;
			}
		}
	}
	if (curthread->specific != NULL) {
		free(curthread->specific);
		curthread->specific = NULL;
	}
}

static inline struct pthread_specific_elem *
pthread_key_allocate_data(void)
{
	struct pthread_specific_elem *new_data;

	new_data = (struct pthread_specific_elem *)
	    malloc(sizeof(struct pthread_specific_elem) * PTHREAD_KEYS_MAX);
	if (new_data != NULL) {
		memset((void *) new_data, 0,
		    sizeof(struct pthread_specific_elem) * PTHREAD_KEYS_MAX);
	}
	return (new_data);
}

int 
_pthread_setspecific(pthread_key_t key, const void *value)
{
	struct pthread	*pthread;
	int		ret = 0;

	/* Point to the running thread: */
	pthread = _get_curthread();

	if ((pthread->specific) ||
	    (pthread->specific = pthread_key_allocate_data())) {
		if (key < PTHREAD_KEYS_MAX) {
			if (key_table[key].allocated) {
				if (pthread->specific[key].data == NULL) {
					if (value != NULL)
						pthread->specific_data_count++;
				} else {
					if (value == NULL)
						pthread->specific_data_count--;
				}
				pthread->specific[key].data = value;
				pthread->specific[key].seqno =
				    key_table[key].seqno;
				ret = 0;
			} else
				ret = EINVAL;
		} else
			ret = EINVAL;
	} else
		ret = ENOMEM;
	return (ret);
}

void *
_pthread_getspecific(pthread_key_t key)
{
	struct pthread	*pthread;
	void		*data;

	/* Point to the running thread: */
	pthread = _get_curthread();

	/* Check if there is specific data: */
	if (pthread->specific != NULL && key < PTHREAD_KEYS_MAX) {
		/* Check if this key has been used before: */
		if (key_table[key].allocated &&
		    (pthread->specific[key].seqno == key_table[key].seqno)) {
			/* Return the value: */
			data = (void *) pthread->specific[key].data;
		} else {
			/*
			 * This key has not been used before, so return NULL
			 * instead: 
			 */
			data = NULL;
		}
	} else
		/* No specific data has been created, so just return NULL: */
		data = NULL;
	return (data);
}
