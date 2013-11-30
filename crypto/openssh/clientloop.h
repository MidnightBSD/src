/* $OpenBSD: clientloop.h,v 1.22 2008/06/12 15:19:17 djm Exp $ */

/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */
/*
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
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

#include <termios.h>

/* Client side main loop for the interactive session. */
int	 client_loop(int, int, int);
void	 client_x11_get_proto(const char *, const char *, u_int,
	    char **, char **);
void	 client_global_request_reply_fwd(int, u_int32_t, void *);
void	 client_session2_setup(int, int, int, const char *, struct termios *,
	    int, Buffer *, char **);
int	 client_request_tun_fwd(int, int, int);

/* Escape filter for protocol 2 sessions */
void	*client_new_escape_filter_ctx(int);
void	 client_filter_cleanup(int, void *);
int	 client_simple_escape_filter(Channel *, char *, int);

/* Global request confirmation callbacks */
typedef void global_confirm_cb(int, u_int32_t seq, void *);
void	 client_register_global_confirm(global_confirm_cb *, void *);

/* Multiplexing protocol version */
#define SSHMUX_VER			2

/* Multiplexing control protocol flags */
#define SSHMUX_COMMAND_OPEN		1	/* Open new connection */
#define SSHMUX_COMMAND_ALIVE_CHECK	2	/* Check master is alive */
#define SSHMUX_COMMAND_TERMINATE	3	/* Ask master to exit */

#define SSHMUX_FLAG_TTY			(1)	/* Request tty on open */
#define SSHMUX_FLAG_SUBSYS		(1<<1)	/* Subsystem request on open */
#define SSHMUX_FLAG_X11_FWD		(1<<2)	/* Request X11 forwarding */
#define SSHMUX_FLAG_AGENT_FWD		(1<<3)	/* Request agent forwarding */

void	muxserver_listen(void);
int	muxserver_accept_control(void);
void	muxclient(const char *);
