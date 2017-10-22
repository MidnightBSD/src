/* $FreeBSD$ */
/*-
 * Copyright (c) 2009 Sylvestre Gallon. All rights reserved.
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
 */

#ifndef __LIBUSB10_H__
#define	__LIBUSB10_H__

#include <sys/queue.h>

#define	GET_CONTEXT(ctx) (((ctx) == NULL) ? usbi_default_context : (ctx))
#define	UNEXPORTED __attribute__((__visibility__("hidden")))
#define	CTX_LOCK(ctx) pthread_mutex_lock(&(ctx)->ctx_lock)
#define	CTX_TRYLOCK(ctx) pthread_mutex_trylock(&(ctx)->ctx_lock)
#define	CTX_UNLOCK(ctx) pthread_mutex_unlock(&(ctx)->ctx_lock)

#define	DPRINTF(ctx, dbg, format, args...) do {	\
    if ((ctx)->debug == dbg) {			\
	switch (dbg) {				\
	case LIBUSB_DEBUG_FUNCTION:		\
		printf("LIBUSB_FUNCTION: "	\
		    format "\n", ## args);	\
		break;				\
	case LIBUSB_DEBUG_TRANSFER:		\
		printf("LIBUSB_TRANSFER: "	\
		    format "\n", ## args);	\
		break;				\
	default:				\
		break;				\
	}					\
    }						\
} while(0)

/* internal structures */

struct libusb_super_pollfd {
	TAILQ_ENTRY(libusb_super_pollfd) entry;
	struct libusb20_device *pdev;
	struct libusb_pollfd pollfd;
};

struct libusb_super_transfer {
	TAILQ_ENTRY(libusb_super_transfer) entry;
	uint8_t *curr_data;
	uint32_t rem_len;
	uint32_t last_len;
	uint8_t	state;
#define	LIBUSB_SUPER_XFER_ST_NONE 0
#define	LIBUSB_SUPER_XFER_ST_PEND 1
};

struct libusb_context {
	int	debug;
	int	debug_fixed;
	int	ctrl_pipe[2];
	int	tr_done_ref;
	int	tr_done_gen;

	pthread_mutex_t ctx_lock;
	pthread_cond_t ctx_cond;
	pthread_t ctx_handler;
#define	NO_THREAD ((pthread_t)-1)

	TAILQ_HEAD(, libusb_super_pollfd) pollfds;
	TAILQ_HEAD(, libusb_super_transfer) tr_done;

	struct libusb_super_pollfd ctx_poll;

	libusb_pollfd_added_cb fd_added_cb;
	libusb_pollfd_removed_cb fd_removed_cb;
	void   *fd_cb_user_data;
};

struct libusb_device {
	int	refcnt;

	uint32_t claimed_interfaces;

	struct libusb_super_pollfd dev_poll;

	struct libusb_context *ctx;

	TAILQ_HEAD(, libusb_super_transfer) tr_head;

	struct libusb20_device *os_priv;
};

extern struct libusb_context *usbi_default_context;

void	libusb10_add_pollfd(libusb_context *ctx, struct libusb_super_pollfd *pollfd, struct libusb20_device *pdev, int fd, short events);
void	libusb10_remove_pollfd(libusb_context *ctx, struct libusb_super_pollfd *pollfd);
void	libusb10_cancel_all_transfer(libusb_device *dev);

#endif					/* __LIBUSB10_H__ */
