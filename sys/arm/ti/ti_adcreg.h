/* $MidnightBSD$ */
/*-
 * Copyright 2014 Luiz Otavio O Souza <loos@freebsd.org>
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
 * $FreeBSD: stable/10/sys/arm/ti/ti_adcreg.h 270238 2014-08-20 18:10:12Z loos $
 */

#ifndef _TI_ADCREG_H_
#define _TI_ADCREG_H_

#define	ADC_REVISION		0x000
#define	ADC_REV_SCHEME_MSK		0xc0000000
#define	ADC_REV_SCHEME_SHIFT		30
#define	ADC_REV_FUNC_MSK		0x0fff0000
#define	ADC_REV_FUNC_SHIFT		16
#define	ADC_REV_RTL_MSK			0x0000f800
#define	ADC_REV_RTL_SHIFT		11
#define	ADC_REV_MAJOR_MSK		0x00000700
#define	ADC_REV_MAJOR_SHIFT		8
#define	ADC_REV_CUSTOM_MSK		0x000000c0
#define	ADC_REV_CUSTOM_SHIFT		6
#define	ADC_REV_MINOR_MSK		0x0000003f
#define	ADC_SYSCFG		0x010
#define	ADC_SYSCFG_IDLE_MSK		0x000000c0
#define	ADC_SYSCFG_IDLE_SHIFT		2
#define	ADC_IRQSTATUS_RAW	0x024
#define	ADC_IRQSTATUS		0x028
#define	ADC_IRQENABLE_SET	0x02c
#define	ADC_IRQENABLE_CLR	0x030
#define	ADC_IRQ_HW_PEN_SYNC		(1 << 10)
#define	ADC_IRQ_PEN_UP			(1 << 9)
#define	ADC_IRQ_OUT_RANGE		(1 << 8)
#define	ADC_IRQ_FIFO1_UNDR		(1 << 7)
#define	ADC_IRQ_FIFO1_OVERR		(1 << 6)
#define	ADC_IRQ_FIFO1_THRES		(1 << 5)
#define	ADC_IRQ_FIFO0_UNDR		(1 << 4)
#define	ADC_IRQ_FIFO0_OVERR		(1 << 3)
#define	ADC_IRQ_FIFO0_THRES		(1 << 2)
#define	ADC_IRQ_END_OF_SEQ		(1 << 1)
#define	ADC_IRQ_HW_PEN_ASYNC		(1 << 0)
#define	ADC_CTRL		0x040
#define	ADC_CTRL_STEP_WP		(1 << 2)
#define	ADC_CTRL_STEP_ID		(1 << 1)
#define	ADC_CTRL_ENABLE			(1 << 0)
#define	ADC_STAT		0x044
#define	ADC_CLKDIV		0x04c
#define	ADC_STEPENABLE		0x054
#define	ADC_IDLECONFIG		0x058
#define	ADC_STEPCFG1		0x064
#define	ADC_STEPDLY1		0x068
#define	ADC_STEPCFG2		0x06c
#define	ADC_STEPDLY2		0x070
#define	ADC_STEPCFG3		0x074
#define	ADC_STEPDLY3		0x078
#define	ADC_STEPCFG4		0x07c
#define	ADC_STEPDLY4		0x080
#define	ADC_STEPCFG5		0x084
#define	ADC_STEPDLY5		0x088
#define	ADC_STEPCFG6		0x08c
#define	ADC_STEPDLY6		0x090
#define	ADC_STEPCFG7		0x094
#define	ADC_STEPDLY7		0x098
#define	ADC_STEPCFG8		0x09c
#define	ADC_STEPDLY8		0x0a0
#define	ADC_STEP_DIFF_CNTRL		(1 << 25)
#define	ADC_STEP_RFM_MSK		0x01800000
#define	ADC_STEP_RFM_SHIFT		23
#define	ADC_STEP_RFM_VSSA		0
#define	ADC_STEP_RFM_XNUR		1
#define	ADC_STEP_RFM_YNLR		2
#define	ADC_STEP_RFM_VREFN		3
#define	ADC_STEP_INP_MSK		0x00780000
#define	ADC_STEP_INP_SHIFT		19
#define	ADC_STEP_INM_MSK		0x00078000
#define	ADC_STEP_INM_SHIFT		15
#define	ADC_STEP_IN_VREFN		8
#define	ADC_STEP_RFP_MSK		0x00007000
#define	ADC_STEP_RFP_SHIFT		12
#define	ADC_STEP_RFP_VDDA		0
#define	ADC_STEP_RFP_XPUL		1
#define	ADC_STEP_RFP_YPLL		2
#define	ADC_STEP_RFP_VREFP		3
#define	ADC_STEP_RFP_INTREF		4
#define	ADC_STEP_AVG_MSK		0x0000001c
#define	ADC_STEP_AVG_SHIFT		2
#define	ADC_STEP_MODE_MSK		0x00000003
#define	ADC_STEP_MODE_ONESHOT		0x00000000
#define	ADC_STEP_MODE_CONTINUOUS	0x00000001
#define	ADC_STEP_SAMPLE_DELAY		0xff000000
#define	ADC_STEP_OPEN_DELAY		0x0003ffff
#define	ADC_FIFO0COUNT		0x0e4
#define	ADC_FIFO0THRESHOLD	0x0e8
#define	ADC_FIFO0DATA		0x100
#define	ADC_FIFO_COUNT_MSK		0x0000007f
#define	ADC_FIFO_STEP_ID_MSK		0x000f0000
#define	ADC_FIFO_STEP_ID_SHIFT		16
#define	ADC_FIFO_DATA_MSK		0x00000fff

#endif /* _TI_ADCREG_H_ */
