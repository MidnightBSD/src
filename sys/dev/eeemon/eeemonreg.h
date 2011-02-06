/*-
 * Copyright (c) 2008 Rui Paulo <rpaulo@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $MidnightBSD$
 */


/*
 * Port addresses.
 */
#define EEEMON_ADDRH	0x381
#define EEEMON_ADDRL	0x382
#define EEEMON_DATA	0x383

/*
 * Values writable to the EC port.
 */
	
#define EEEMON_TEMPVAL      0xf451          /* Temperature of CPU (C) */
#define EEEMON_FANSPEEDVAL  0xf463          /* Fan PWM duty cycle (%) */
#define EEEMON_FANHVAL      0xf466          /* High byte of fan speed (RPM) */
#define EEEMON_FANLVAL      0xf467          /* Low byte of fan speed (RPM) */
#define EEEMON_FANMANUALVAL 0xf4d3          /* Flag byte containing SF25 (FANctrl) */

