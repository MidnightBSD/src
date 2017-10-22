/*-
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
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

/*
 * IP Over ATM Support
 * -------------------
 *
 * Output IP packets across an ATM VCC
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/netatm/ipatm/ipatm_output.c 147256 2005-06-10 16:49:24Z brooks $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
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

#include <netatm/ipatm/ipatm_var.h>
#include <netatm/ipatm/ipatm_serv.h>

/*
 * Output an IP Packet
 * 
 * All IP packets output to an ATM interface will be directed here via
 * the atm_ifoutput() function.  If there is an ATM VCC already setup for
 * the destination IP address, then we'll just send the packet to that VCC.
 * Otherwise we will have to setup a new VCC, ARPing for the corresponding
 * destination ATM hardware address along the way.
 *
 * Arguments:
 *	ifp	pointer to ifnet structure
 *	m	pointer to packet buffer chain to be output
 *	dst	pointer to packet's IP destination address
 *
 * Returns:
 *	0 	packet "output" was successful 
 *	errno	output failed - reason indicated
 *
 */
int
ipatm_ifoutput(ifp, m, dst)
	struct ifnet	*ifp;
	KBuffer		*m;
	struct sockaddr	*dst;
{
	struct ipvcc	*ivp;
	int	err = 0;

	if (ipatm_print) {
		atm_pdu_print(m, "ipatm_ifoutput");
	}

	/*
	 * See if we've already got an appropriate VCC
	 */
	ivp = ipatm_iptovc((struct sockaddr_in *)dst, IFP2ANIF(ifp));
	if (ivp) {

		/*
		 * Reset idle timer
		 */
		ivp->iv_idle = 0;

		/*
		 * Can we use this VCC now??
		 */
		if ((ivp->iv_state == IPVCC_ACTIVE) && 
		    (ivp->iv_flags & IVF_MAPOK)) {

			/*
			 * OK, now send packet
			 */
			err = atm_cm_cpcs_data(ivp->iv_conn, m);
			if (err) {
				/*
				 * Output problem, drop packet
				 */
				KB_FREEALL(m);
			}
		} else {

			/*
			 * VCC is unavailable for data packets.  Queue packet
			 * for now, but only maintain a queue length of one.
			 */
			if (ivp->iv_queue)
				KB_FREEALL(ivp->iv_queue);

			ivp->iv_queue = m;
		}
	} else {
		struct in_ifaddr	*ia;

		/*
		 * No VCC to destination
		 */

		/*
		 * Is packet for our interface address?
		 */
		TAILQ_FOREACH(ia, &in_ifaddrhead, ia_link) {
			if (ia->ia_ifp != ifp)
				continue;
			if (((struct sockaddr_in *)dst)->sin_addr.s_addr == 
					IA_SIN(ia)->sin_addr.s_addr) {

				/*
				 * It's for us - hand packet to loopback driver
				 */
				(void) if_simloop(ifp, m, dst->sa_family, 0);
				goto done;
			}
		}

		/*
		 * Is this a broadcast packet ??
		 */
		if (in_broadcast(((struct sockaddr_in *)dst)->sin_addr, ifp)) {
			struct ip_nif	*inp;
			int	s;

			/*
			 * If interface server exists and provides broadcast 
			 * services, then let it deal with this packet
			 */
			s = splnet();
			for (inp = ipatm_nif_head; inp; inp = inp->inf_next) {
				if (inp->inf_nif == IFP2ANIF(ifp))
					break;
			}
			(void) splx(s);

			if ((inp == NULL) ||
			    (inp->inf_serv == NULL) ||
			    (inp->inf_serv->is_bcast_output == NULL)) {
				KB_FREEALL(m);
				err = EADDRNOTAVAIL;
				goto done;
			}

			err = (*inp->inf_serv->is_bcast_output)(inp, m);
			goto done;
		}

		/*
		 * How about a multicast packet ??
		 */
		if (IN_MULTICAST(ntohl(SATOSIN(dst)->sin_addr.s_addr))) {
			/* 
			 * Multicast isn't currently supported
			 */
			KB_FREEALL(m);
			err = EADDRNOTAVAIL;
			goto done;
		}

		/*
		 * Well, I guess we need to create an SVC to the destination
		 */
		if ((err = ipatm_createsvc(ifp, AF_INET,
				(caddr_t)&((struct sockaddr_in *)dst)->sin_addr,
				&ivp)) == 0) {
			/*
			 * SVC open is proceeding, queue packet 
			 */
			ivp->iv_queue = m;

		} else {
			/*
			 * SVC open failed, release buffers and return
			 */
			KB_FREEALL(m);
		}
	}

done:
	return (err);
}
