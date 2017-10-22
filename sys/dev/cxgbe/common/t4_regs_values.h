/*-
 * Copyright (c) 2011 Chelsio Communications, Inc.
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
 * $FreeBSD$
 *
 */

#ifndef __T4_REGS_VALUES_H__
#define __T4_REGS_VALUES_H__

/*
 * This file contains definitions for various T4 register value hardware
 * constants.  The types of values encoded here are predominantly those for
 * register fields which control "modal" behavior.  For the most part, we do
 * not include definitions for register fields which are simple numeric
 * metrics, etc.
 *
 * These new "modal values" use a naming convention which matches the
 * currently existing macros in t4_reg.h.  For register field FOO which would
 * have S_FOO, M_FOO, V_FOO() and G_FOO() macros, we introduce X_FOO_{MODE}
 * definitions.  These can be used as V_FOO(X_FOO_MODE) or as (G_FOO(x) ==
 * X_FOO_MODE).
 *
 * Note that this should all be part of t4_regs.h but the toolset used to
 * generate that file doesn't [yet] have the capability of collecting these
 * constants.
 */

/*
 * SGE definitions.
 * ================
 */

/*
 * SGE register field values.
 */

/* CONTROL register */
#define X_FLSPLITMODE_FLSPLITMIN	0
#define X_FLSPLITMODE_ETHHDR		1
#define X_FLSPLITMODE_IPHDR		2
#define X_FLSPLITMODE_TCPHDR		3

#define X_DCASYSTYPE_FSB		0
#define X_DCASYSTYPE_CSI		1

#define X_EGSTATPAGESIZE_64B		0
#define X_EGSTATPAGESIZE_128B		1

#define X_RXPKTCPLMODE_DATA		0
#define X_RXPKTCPLMODE_SPLIT		1

#define X_INGPCIEBOUNDARY_SHIFT		5
#define X_INGPCIEBOUNDARY_32B		0
#define X_INGPCIEBOUNDARY_64B		1
#define X_INGPCIEBOUNDARY_128B		2
#define X_INGPCIEBOUNDARY_256B		3
#define X_INGPCIEBOUNDARY_512B		4
#define X_INGPCIEBOUNDARY_1024B		5
#define X_INGPCIEBOUNDARY_2048B		6
#define X_INGPCIEBOUNDARY_4096B		7

#define X_INGPADBOUNDARY_SHIFT		5
#define X_INGPADBOUNDARY_32B		0
#define X_INGPADBOUNDARY_64B		1
#define X_INGPADBOUNDARY_128B		2
#define X_INGPADBOUNDARY_256B		3
#define X_INGPADBOUNDARY_512B		4
#define X_INGPADBOUNDARY_1024B		5
#define X_INGPADBOUNDARY_2048B		6
#define X_INGPADBOUNDARY_4096B		7

#define X_EGRPCIEBOUNDARY_SHIFT		5
#define X_EGRPCIEBOUNDARY_32B		0
#define X_EGRPCIEBOUNDARY_64B		1
#define X_EGRPCIEBOUNDARY_128B		2
#define X_EGRPCIEBOUNDARY_256B		3
#define X_EGRPCIEBOUNDARY_512B		4
#define X_EGRPCIEBOUNDARY_1024B		5
#define X_EGRPCIEBOUNDARY_2048B		6
#define X_EGRPCIEBOUNDARY_4096B		7

/* GTS register */
#define SGE_TIMERREGS			6
#define X_TIMERREG_COUNTER0		0
#define X_TIMERREG_COUNTER1		1
#define X_TIMERREG_COUNTER2		2
#define X_TIMERREG_COUNTER3		3
#define X_TIMERREG_COUNTER4		4
#define X_TIMERREG_COUNTER5		5
#define X_TIMERREG_RESTART_COUNTER	6
#define X_TIMERREG_UPDATE_CIDX		7

/*
 * Egress Context field values
 */
#define EC_WR_UNITS			16

#define X_FETCHBURSTMIN_SHIFT		4
#define X_FETCHBURSTMIN_16B		0
#define X_FETCHBURSTMIN_32B		1
#define X_FETCHBURSTMIN_64B		2
#define X_FETCHBURSTMIN_128B		3

#define X_FETCHBURSTMAX_SHIFT		6
#define X_FETCHBURSTMAX_64B		0
#define X_FETCHBURSTMAX_128B		1
#define X_FETCHBURSTMAX_256B		2
#define X_FETCHBURSTMAX_512B		3

#define X_HOSTFCMODE_NONE		0
#define X_HOSTFCMODE_INGRESS_QUEUE	1
#define X_HOSTFCMODE_STATUS_PAGE	2
#define X_HOSTFCMODE_BOTH		3

#define X_HOSTFCOWNER_UP		0
#define X_HOSTFCOWNER_SGE		1

#define X_CIDXFLUSHTHRESH_1		0
#define X_CIDXFLUSHTHRESH_2		1
#define X_CIDXFLUSHTHRESH_4		2
#define X_CIDXFLUSHTHRESH_8		3
#define X_CIDXFLUSHTHRESH_16		4
#define X_CIDXFLUSHTHRESH_32		5
#define X_CIDXFLUSHTHRESH_64		6
#define X_CIDXFLUSHTHRESH_128		7

#define X_IDXSIZE_UNIT			64

#define X_BASEADDRESS_ALIGN		512

/*
 * Ingress Context field values
 */
#define X_UPDATESCHEDULING_TIMER	0
#define X_UPDATESCHEDULING_COUNTER_OPTTIMER	1

#define X_UPDATEDELIVERY_NONE		0
#define X_UPDATEDELIVERY_INTERRUPT	1
#define X_UPDATEDELIVERY_STATUS_PAGE	2
#define X_UPDATEDELIVERY_BOTH		3

#define X_INTERRUPTDESTINATION_PCIE	0
#define X_INTERRUPTDESTINATION_IQ	1

#define X_QUEUEENTRYSIZE_16B		0
#define X_QUEUEENTRYSIZE_32B		1
#define X_QUEUEENTRYSIZE_64B		2
#define X_QUEUEENTRYSIZE_128B		3

#define IC_SIZE_UNIT			16
#define IC_BASEADDRESS_ALIGN		512

#define X_RSPD_TYPE_FLBUF		0
#define X_RSPD_TYPE_CPL			1
#define X_RSPD_TYPE_INTR		2

/*
 * CIM definitions.
 * ================
 */

/*
 * CIM register field values.
 */
#define X_MBOWNER_NONE			0
#define X_MBOWNER_FW			1
#define X_MBOWNER_PL			2

#endif /* __T4_REGS_VALUES_H__ */
