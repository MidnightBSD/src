/*	$OpenBSD: clockvar.h,v 1.1 1998/01/29 15:06:19 pefo Exp $	*/
/*	$NetBSD: clockvar.h,v 1.1 1995/06/28 02:44:59 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * Adopted for r4400: Per Fogelstrom
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 *	JNPR: clockvar.h,v 1.3 2006/08/07 05:38:57 katta
 * $FreeBSD$
 */

/*
 * Definitions for "cpu-independent" clock handling for the mips arc arch.
 */

/*
 * clocktime structure:
 *
 * structure passed to TOY clocks when setting them.  broken out this
 * way, so that the time_t -> field conversion can be shared.
 */
struct tod_time {
	int	year;		/* year - 1900 */
	int	mon;		/* month (1 - 12) */
	int	day;		/* day (1 - 31) */
	int	hour;		/* hour (0 - 23) */
	int	min;		/* minute (0 - 59) */
	int	sec;		/* second (0 - 59) */
	int	dow;		/* day of week (0 - 6; 0 = Sunday) */
};

int clockinitted;
