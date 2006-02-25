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
 * $FreeBSD: src/sbin/geom/misc/subr.h,v 1.3.8.1 2005/08/30 15:09:06 pjd Exp $
 */

#ifndef _SUBR_H_
#define	_SUBR_H_
unsigned g_lcm(unsigned a, unsigned b);
uint32_t bitcount32(uint32_t x);

off_t g_get_mediasize(const char *name);
unsigned g_get_sectorsize(const char *name);

int g_metadata_read(const char *name, u_char *md, size_t size,
    const char *magic);
int g_metadata_store(const char *name, u_char *md, size_t size);
int g_metadata_clear(const char *name, const char *magic);

void gctl_error(struct gctl_req *req, const char *error, ...);
void *gctl_get_param(struct gctl_req *req, const char *param, int *len);
char const *gctl_get_asciiparam(struct gctl_req *req, const char *param);
void *gctl_get_paraml(struct gctl_req *req, const char *param, int len);
#endif	/* !_SUBR_H_ */
