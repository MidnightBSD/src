/*-
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/hfa/fore_timer.c,v 1.9 2003/08/24 17:46:08 obrien Exp $");

/*
 * FORE Systems 200-Series Adapter Support
 * ---------------------------------------
 *
 * Timer processing
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>
#include <dev/pci/pcivar.h>
#include <dev/hfa/fore.h>
#include <dev/hfa/fore_aali.h>
#include <dev/hfa/fore_slave.h>
#include <dev/hfa/fore_stats.h>
#include <dev/hfa/fore_var.h>
#include <dev/hfa/fore_include.h>

#ifndef lint
__RCSID("@(#) $FreeBSD: src/sys/dev/hfa/fore_timer.c,v 1.9 2003/08/24 17:46:08 obrien Exp $");
#endif


/*
 * Process a Fore timer tick
 * 
 * This function is called every FORE_TIME_TICK seconds in order to update
 * all of the unit watchdog timers.  
 *
 * Called at splnet.
 *
 * Arguments:
 *	tip	pointer to fore timer control block
 *
 * Returns:
 *	none
 *
 */
void
fore_timeout(tip)
	struct atm_time	*tip;
{
	Fore_unit	*fup;
	int		i;


	/*
	 * Schedule next timeout
	 */
	atm_timeout(&fore_timer, ATM_HZ * FORE_TIME_TICK, fore_timeout);

	/*
	 * Run through all units, updating each active timer.
	 * If an expired timer is found, notify that unit.
	 */
	for (i = 0; i < fore_nunits; i++) {

		if ((fup = fore_units[i]) == NULL)
			continue;

		/*
		 * Decrement timer, if it's active
		 */
		if (fup->fu_timer && (--fup->fu_timer == 0)) {

			/*
			 * Timeout occurred - go check out the queues
			 */
			ATM_DEBUG0("fore_timeout\n");
			DEVICE_LOCK((Cmn_unit *)fup);
			fore_watchdog(fup);
			DEVICE_UNLOCK((Cmn_unit *)fup);
		}
	}
}

