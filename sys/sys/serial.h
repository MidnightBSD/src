/*-
 * Copyright (c) 2004 Poul-Henning Kamp
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
 * This file contains definitions which pertain to serial ports as such,
 * (both async and sync), but which do not necessarily have anything to
 * do with tty processing.
 *
 * $FreeBSD: src/sys/sys/serial.h,v 1.2 2004/06/25 10:56:43 phk Exp $
 */

#ifndef _SYS_SERIAL_H_
#define	_SYS_SERIAL_H_


/*
 * Indentification of modem control signals.  These definitions match
 * the TIOCMGET definitions in <sys/ttycom.h> shifted a bit down, and
 * that identity is enforced with CTASSERT at the bottom of kern/tty.c
 * Both the modem bits and delta bits must fit in 16 bit.
 */
#define		SER_DTR	0x0001		/* data terminal ready */
#define		SER_RTS	0x0002		/* request to send */
#define		SER_STX	0x0004		/* secondary transmit */
#define		SER_SRX	0x0008		/* secondary receive */
#define		SER_CTS	0x0010		/* clear to send */
#define		SER_DCD	0x0020		/* data carrier detect */
#define		SER_RI 	0x0040		/* ring indicate */
#define		SER_DSR	0x0080		/* data set ready */

/* Delta bits, used to indicate which signals should/was affected */
#define		SER_DELTA(x)	((x) << 8)

#define		SER_DDTR SER_DELTA(SER_DTR)
#define		SER_DRTS SER_DELTA(SER_RTS)
#define		SER_DSTX SER_DELTA(SER_STX)
#define		SER_DSRX SER_DELTA(SER_SRX)
#define		SER_DCTS SER_DELTA(SER_CTS)
#define		SER_DDCD SER_DELTA(SER_DCD)
#define		SER_DRI  SER_DELTA(SER_RI)
#define		SER_DDSR SER_DELTA(SER_DSR)

#endif /* !_SYS_SERIAL_H_ */
