/*-
 * Copyright (c) 2004 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sbin/geom/core/geom.h,v 1.2 2005/03/14 14:24:46 pjd Exp $
 */

#ifndef _GEOM_H_
#define	_GEOM_H_
#define	G_LIB_VERSION	1

#define	G_FLAG_NONE	0x0000
#define	G_FLAG_VERBOSE	0x0001
#define	G_FLAG_LOADKLD	0x0002

#define	G_TYPE_NONE	0
#define	G_TYPE_STRING	1
#define	G_TYPE_NUMBER	2

#define	G_OPT_MAX	16
#define	G_OPT_DONE(opt)		(opt)->go_char = '\0'
#define	G_OPT_ISDONE(opt)	((opt)->go_char == '\0')

#define G_OPT_SENTINEL	{ '\0', NULL, NULL, G_TYPE_NONE }
#define G_NULL_OPTS	{ G_OPT_SENTINEL }
#define	G_CMD_SENTINEL	{ NULL, 0, NULL, G_NULL_OPTS, NULL }

struct g_option {
	char		 go_char;
	const char	*go_name;
	void		*go_val;
	unsigned	 go_type;
};

struct g_command {
	const char	*gc_name;
	unsigned	 gc_flags;
	void		(*gc_func)(struct gctl_req *, unsigned);
	struct g_option	gc_options[G_OPT_MAX];
	const char	*gc_usage;
};
#endif	/* !_GEOM_H_ */
