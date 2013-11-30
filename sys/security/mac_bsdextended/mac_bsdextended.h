/*-
 * Copyright (c) 1999-2002 Robert N. M. Watson
 * Copyright (c) 2001-2004 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"),
 * as part of the DARPA CHATS research program.
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
 * $FreeBSD: src/sys/security/mac_bsdextended/mac_bsdextended.h,v 1.5 2004/10/21 11:29:56 rwatson Exp $
 */

#ifndef _SYS_SECURITY_MAC_BSDEXTENDED_H
#define	_SYS_SECURITY_MAC_BSDEXTENDED_H

#define	MBI_UID_DEFINED	0x00000001	/* uid field should be used */
#define	MBI_GID_DEFINED	0x00000002	/* gid field should be used */
#define	MBI_NEGATED	0x00000004	/* negate uid/gid matches */
#define	MBI_BITS	(MBI_UID_DEFINED | MBI_GID_DEFINED | MBI_NEGATED)

/*
 * Rights that can be represented in mbr_mode.  These have the same values
 * as the V* rights in vnode.h, but in order to avoid sharing user and
 * kernel constants, we define them here.  That will also improve ABI
 * stability if the in-kernel values change.
 */
#define	MBI_EXEC	000100
#define	MBI_WRITE	000200
#define	MBI_READ	000400
#define	MBI_ADMIN	010000
#define	MBI_STAT	020000
#define	MBI_APPEND	040000
#define	MBI_ALLPERM	(MBI_EXEC | MBI_WRITE | MBI_READ | MBI_ADMIN | \
			    MBI_STAT | MBI_APPEND)

struct mac_bsdextended_identity {
	int	mbi_flags;
	uid_t	mbi_uid;
	gid_t	mbi_gid;
};

struct mac_bsdextended_rule {
	struct mac_bsdextended_identity	mbr_subject;
	struct mac_bsdextended_identity	mbr_object;
	mode_t				mbr_mode;	/* maximum access */
};

#endif /* _SYS_SECURITY_MAC_BSDEXTENDED_H */
