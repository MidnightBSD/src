/*
 * bthidcontrol.h
 *
 * Copyright (c) 2004 Maksim Yevmenkin <m_evmenkin@yahoo.com>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: bthidcontrol.h,v 1.1.1.2 2006-02-25 02:38:24 laffer1 Exp $
 * $FreeBSD: src/usr.sbin/bluetooth/bthidcontrol/bthidcontrol.h,v 1.1 2004/04/10 00:18:00 emax Exp $
 */

#ifndef __BTHIDCONTROL_H__
#define __BTHIDCONTROL_H__

#define OK			0	/* everything was OK */
#define ERROR			1	/* could not execute command */
#define FAILED			2	/* error was reported */
#define USAGE			3	/* invalid parameters */

struct bthid_command {
	char const		*command;
	char const		*description;
	int			(*handler)(bdaddr_t *, int, char **);
};

extern struct bthid_command	hid_commands[];
extern struct bthid_command	sdp_commands[];

#endif /* __BTHIDCONTROL_H__ */

