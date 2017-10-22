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
 * Process user requests
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/netatm/ipatm/ipatm_usrreq.c 147256 2005-06-10 16:49:24Z brooks $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_vc.h>
#include <netatm/atm_ioctl.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <netatm/ipatm/ipatm_var.h>
#include <netatm/ipatm/ipatm_serv.h>

/*
 * Process IP PF_ATM ioctls
 * 
 * Called at splnet.
 *
 * Arguments:
 *	code	PF_ATM sub-operation code
 *	data	pointer to code specific parameter data area
 *	arg1	pointer to code specific argument
 *
 * Returns:
 *	0 	request procesed
 *	errno	error processing request - reason indicated
 *
 */
int
ipatm_ioctl(code, data, arg1)
	int		code;
	caddr_t		data;
	caddr_t		arg1;
{
	struct atmaddreq	*aap;
	struct atmdelreq	*adp;
	struct atminfreq	*aip;
	struct air_ip_vcc_rsp	aivr;
	struct atm_nif	*nip;
	struct ip_nif	*inp;
	struct ipvcc	*ivp;
	struct vccb	*vcp;
	struct ipatmpvc	pv;
	caddr_t		cp;
	struct in_addr	ip;
	int err = 0;
	size_t space;
	struct t_atm_traffic *traf;


	switch (code) {

	case AIOCS_ADD_PVC:
		/*
		 * Add an IP PVC
		 */
		aap = (struct atmaddreq *)data;

		/*
		 * Find the IP network interface
		 */
		if ((nip = atm_nifname(aap->aar_pvc_intf)) == NULL) {
			err = ENXIO;
			break;
		}

		for (inp = ipatm_nif_head; inp; inp = inp->inf_next) {
			if (inp->inf_nif == nip)
				break;
		}
		if (inp == NULL) {
			err = ENXIO;
			break;
		}

		/*
		 * Validate PVC params
		 */
		if (aap->aar_pvc_aal == ATM_AAL5) {
			if ((aap->aar_pvc_encaps != ATM_ENC_LLC) &&
			    (aap->aar_pvc_encaps != ATM_ENC_NULL)) {
				err = EINVAL;
				break;
			}
		} else if (aap->aar_pvc_aal == ATM_AAL3_4) {
			if (aap->aar_pvc_encaps != ATM_ENC_NULL) {
				err = EINVAL;
				break;
			}
		} else {
			err = EINVAL;
			break;
		}

		if (aap->aar_pvc_flags & PVC_DYN) {
			/*
			 * For dynamic PVC destination addressing, the
			 * network interface must have support for this
			 */
			if ((inp->inf_serv == NULL) ||
			    (inp->inf_serv->is_arp_pvcopen == NULL)) {
				err = EDESTADDRREQ;
				break;
			}
		} else {
			u_long	dst = ((struct sockaddr_in *)&aap->aar_pvc_dst)
						->sin_addr.s_addr;

			if (dst == INADDR_ANY) {
				err = EINVAL;
				break;
			}
		}

		/*
		 * Validate PVC traffic
		 */
#define MAXVAL(bits)	((1 << bits) - 1)
#define MAXMASK(bits)	(~MAXVAL(bits))
		traf = &aap->aar_pvc_traffic;
		switch (aap->aar_pvc_traffic_type) {

		  case T_ATM_CBR:
		  case T_ATM_UBR:
			/*
			 * PCR is a value between 0 to the PIF's PCR
			 */
			if (traf->forward.PCR_high_priority == T_ATM_ABSENT ||
			    (traf->forward.PCR_high_priority & MAXMASK(24))) {
				err = EINVAL;
				break;
			}
			if (traf->forward.PCR_all_traffic == T_ATM_ABSENT ||
			    (traf->forward.PCR_all_traffic & MAXMASK(24))) {
				err = EINVAL;
				break;
			}

			if (traf->backward.PCR_high_priority == T_ATM_ABSENT ||
			    (traf->backward.PCR_high_priority & MAXMASK(24))) {
				err = EINVAL;
				break;
			}
			if (traf->backward.PCR_all_traffic == T_ATM_ABSENT  ||
			    (traf->backward.PCR_all_traffic & MAXMASK(24))) {
				err = EINVAL;
				break;
			}
			break;

		  case T_ATM_VBR:
			/*
			 * PCR, SCR and MBS are required
			 */
			if (traf->forward.PCR_high_priority == T_ATM_ABSENT ||
			    (traf->forward.PCR_high_priority & MAXMASK(24)) ||
			    traf->forward.PCR_all_traffic == T_ATM_ABSENT ||
			    (traf->forward.PCR_all_traffic & MAXMASK(24))) {
				err = EINVAL;
				break;
			}
			if (traf->forward.SCR_high_priority == T_ATM_ABSENT ||
			    (traf->forward.SCR_high_priority & MAXMASK(24)) ||
			    traf->forward.SCR_all_traffic == T_ATM_ABSENT ||
			    (traf->forward.SCR_all_traffic & MAXMASK(24))) {
				err = EINVAL;
				break;
			}
			if (traf->forward.MBS_high_priority == T_ATM_ABSENT ||
			    (traf->forward.MBS_high_priority & MAXMASK(24)) ||
			    traf->forward.MBS_all_traffic == T_ATM_ABSENT ||
			    (traf->forward.MBS_all_traffic & MAXMASK(24))) {
				err = EINVAL;
				break;
			}

			if (traf->backward.PCR_high_priority == T_ATM_ABSENT ||
			    (traf->backward.PCR_high_priority & MAXMASK(24)) ||
			    traf->backward.PCR_all_traffic == T_ATM_ABSENT ||
			    (traf->backward.PCR_all_traffic & MAXMASK(24))) {
				err = EINVAL;
				break;
			}
			if (traf->backward.SCR_high_priority == T_ATM_ABSENT ||
			    (traf->backward.SCR_high_priority & MAXMASK(24)) ||
			    traf->backward.SCR_all_traffic == T_ATM_ABSENT ||
			    (traf->backward.SCR_all_traffic & MAXMASK(24))) {
				err = EINVAL;
				break;
			}
			if (traf->backward.MBS_high_priority == T_ATM_ABSENT ||
			    (traf->backward.MBS_high_priority & MAXMASK(24)) ||
			    traf->backward.MBS_all_traffic == T_ATM_ABSENT ||
			    (traf->backward.MBS_all_traffic & MAXMASK(24))) {
				err = EINVAL;
				break;
			}
			break;

		  case T_ATM_NULL:
			/*
			 * No PVC traffic type
			 */
			break;

		  default:
			err = EINVAL;
			break;
		}
		if (err != 0)
			break;

		/*
		 * Build connection request
		 */
		pv.ipp_ipnif = inp;
		pv.ipp_vpi = aap->aar_pvc_vpi;
		pv.ipp_vci = aap->aar_pvc_vci;
		pv.ipp_traffic_type = aap->aar_pvc_traffic_type;
		pv.ipp_traffic = aap->aar_pvc_traffic;
		pv.ipp_encaps = aap->aar_pvc_encaps;
		pv.ipp_aal = aap->aar_pvc_aal;
		if (aap->aar_pvc_flags & PVC_DYN) {
			pv.ipp_dst.sin_addr.s_addr = INADDR_ANY;
		} else
			pv.ipp_dst = *(struct sockaddr_in *)&aap->aar_pvc_dst;

		/*
		 * Open a new VCC
		 */
		err = ipatm_openpvc(&pv, &ivp);
		break;

	case AIOCS_ADD_ARP:
		/*
		 * Add an ARP mapping
		 */
		aap = (struct atmaddreq *)data;

		/*
		 * Validate IP address
		 */
		if (aap->aar_arp_dst.sa_family != AF_INET) {
			err = EAFNOSUPPORT;
			break;
		}
		ip = SATOSIN(&aap->aar_arp_dst)->sin_addr;

		if (aap->aar_arp_intf[0] == '\0') {
			/*
			 * Find the IP network interface associated with
			 * the supplied IP address
			 */
			for (inp = ipatm_nif_head; inp; inp = inp->inf_next) {
				if (ipatm_chknif(ip, inp) == 0)
					break;
			}
			if (inp == NULL) {
				err = EADDRNOTAVAIL;
				break;
			}
		} else {
			/*
			 * Find the specified IP network interface
			 */
			if ((nip = atm_nifname(aap->aar_arp_intf)) == NULL) {
				err = ENXIO;
				break;
			}
			for (inp = ipatm_nif_head; inp; inp = inp->inf_next) {
				if (inp->inf_nif == nip)
					break;
			}
			if (inp == NULL) {
				err = ENXIO;
				break;
			}
		}

		if ((ip.s_addr == INADDR_ANY) ||
		    in_broadcast(ip, ANIF2IFP(inp->inf_nif)) ||
		    IN_MULTICAST(ntohl(ip.s_addr))) {
			err = EADDRNOTAVAIL;
			break;
		}

		/*
		 * Notify the responsible ARP service
		 *
		 * XXX: if there is one.  No idea how this happens, but at
		 * least don't panic on a NULL pointer if it does.
		 */
		if (inp->inf_serv == NULL) {
			err = ENXIO;
			break;
		}
		err = (*inp->inf_serv->is_ioctl)(code, data, inp->inf_isintf);
		break;

	case AIOCS_DEL_ARP:
		/*
		 * Delete an ARP mapping
		 */
		adp = (struct atmdelreq *)data;

		/*
		 * Validate IP address
		 */
		if (adp->adr_arp_dst.sa_family != AF_INET) {
			err = EAFNOSUPPORT;
			break;
		}
		ip = SATOSIN(&adp->adr_arp_dst)->sin_addr;

		if (adp->adr_arp_intf[0] == '\0') {
			/*
			 * Find the IP network interface associated with
			 * the supplied IP address
			 */
			for (inp = ipatm_nif_head; inp; inp = inp->inf_next) {
				if (ipatm_chknif(ip, inp) == 0)
					break;
			}
			if (inp == NULL) {
				err = EADDRNOTAVAIL;
				break;
			}
		} else {
			/*
			 * Find the specified IP network interface
			 */
			if ((nip = atm_nifname(adp->adr_arp_intf)) == NULL) {
				err = ENXIO;
				break;
			}
			for (inp = ipatm_nif_head; inp; inp = inp->inf_next) {
				if (inp->inf_nif == nip)
					break;
			}
			if (inp == NULL) {
				err = ENXIO;
				break;
			}
		}

		if ((ip.s_addr == INADDR_ANY) ||
		    in_broadcast(ip, ANIF2IFP(inp->inf_nif)) ||
		    IN_MULTICAST(ntohl(ip.s_addr))) {
			err = EADDRNOTAVAIL;
			break;
		}

		/*
		 * Notify the responsible ARP service
		 */
		err = (*inp->inf_serv->is_ioctl)(code, data, inp->inf_isintf);
		break;

	case AIOCS_INF_IPM:
		/*
		 * Get IP VCC information
		 */
		aip = (struct atminfreq *)data;

		if (aip->air_ip_addr.sa_family != AF_INET)
			break;
		ip = SATOSIN(&aip->air_ip_addr)->sin_addr;

		cp = aip->air_buf_addr;
		space = aip->air_buf_len;

		/*
		 * Loop through all our interfaces
		 */
		for (inp = ipatm_nif_head; inp; inp = inp->inf_next) {
			/*
			 * Check out each VCC
			 */
			for (ivp = Q_HEAD(inp->inf_vcq, struct ipvcc); ivp;
				    ivp = Q_NEXT(ivp, struct ipvcc, iv_elem)) {

				if ((ip.s_addr != INADDR_ANY) &&
				    (ip.s_addr != ivp->iv_dst.s_addr))
					continue;

				/*
				 * Make sure there's room in user buffer
				 */
				if (space < sizeof(aivr)) {
					err = ENOSPC;
					break;
				}

				/*
				 * Fill in info to be returned
				 */
				bzero((caddr_t)&aivr, sizeof(aivr));
				SATOSIN(&aivr.aip_dst_addr)->sin_family = 
					AF_INET;
				SATOSIN(&aivr.aip_dst_addr)->sin_addr.s_addr = 
					ivp->iv_dst.s_addr;
				strlcpy(aivr.aip_intf,
				    ANIF2IFP(inp->inf_nif)->if_xname,
				    sizeof(aivr.aip_intf));
				if ((ivp->iv_conn) &&
				    (ivp->iv_conn->co_connvc) &&
				    (vcp = ivp->iv_conn->co_connvc->cvc_vcc)) {
					aivr.aip_vpi = vcp->vc_vpi;
					aivr.aip_vci = vcp->vc_vci;
					aivr.aip_sig_proto = vcp->vc_proto;
				} 
				aivr.aip_flags = ivp->iv_flags;
				aivr.aip_state = ivp->iv_state;

				/*
				 * Copy data to user buffer and 
				 * update buffer controls
				 */
				err = copyout((caddr_t)&aivr, cp, sizeof(aivr));
				if (err)
					break;
				cp += sizeof(aivr);
				space -= sizeof(aivr);
			}
			if (err)
				break;
		}

		/*
		 * Update buffer pointer/count
		 */
		aip->air_buf_addr = cp;
		aip->air_buf_len = space;
		break;

	default:
		err = EOPNOTSUPP;
	}

	return (err);
}


/*
 * Get Connection's Application/Owner Name
 * 
 * Arguments:
 *	tok	ipatm connection token (pointer to ipvcc)
 *
 * Returns:
 *	addr	pointer to string containing our name
 *
 */
caddr_t
ipatm_getname(tok)
	void		*tok;
{
	return ("IP");
}

