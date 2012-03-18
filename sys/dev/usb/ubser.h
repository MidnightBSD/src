/* $MidnightBSD: src/sys/dev/usb/ubser.h,v 1.2 2008/12/02 22:43:15 laffer1 Exp $ */
/*-
 * Copyright (c) 2003 Bernd Walter <ticso@freebsd.org>
 *
 * $URL: https://devel.bwct.de/svn/projects/ubser/ubser.h $
 * $Date: 2012-03-18 16:23:09 $
 * $Author: laffer1 $
 * $Rev: 1127 $
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
 * $FreeBSD: src/sys/dev/usb/ubser.h,v 1.3.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef UBSER_H
#define UBSER_H

#define MAX_SER	32

/* Vendor Interface Requests */
#define VENDOR_GET_NUMSER		0x01
#define VENDOR_SET_BREAK		0x02
#define VENDOR_CLEAR_BREAK		0x03

#endif /* UBSER_H */
