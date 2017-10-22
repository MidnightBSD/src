/*
 *
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/lib/libatm/ioctl_subr.c 126069 2004-02-21 16:29:10Z harti $");

/*
 * User Space Library Functions
 * ----------------------------
 *
 * IOCTL subroutines
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h>
#include <netatm/atm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_ioctl.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libatm.h"

#ifndef	TRUE
#define	TRUE	1
#endif

#ifndef	FALSE
#define	FALSE	0
#endif

extern char	*prog;


/*
 * Issue an informational IOCTL
 * 
 * The user fills out the opcode and any subtype information.  This
 * routine will allocate a buffer and issue the IOCTL.  If the request
 * fails because the buffer wasn't big enough, this routine will double
 * the buffer size and retry the request repeatedly.  The buffer must
 * be freed by the caller.
 * 
 * Arguments:
 *	req	pointer to an ATM information request IOCTL structure
 *	buf_len	length of buffer to be allocated
 *
 * Returns:
 *	-1	error encountered (reason in errno)
 *	int 	length of the returned VCC information
 *
 */
ssize_t
do_info_ioctl(struct atminfreq *req, size_t buf_len)
{
	int	rc, s;
	caddr_t	buf;

	/*
	 * Open a socket for the IOCTL
	 */
	s = socket(AF_ATM, SOCK_DGRAM, 0);
	if (s < 0) {
		return(-1);
	}

	/*
	 * Get memory for returned information
	 */
mem_retry:
	buf = malloc(buf_len);
	if (buf == NULL) {
		errno = ENOMEM;
		return(-1);
	}
	bzero(buf, buf_len);

	/*
	 * Set the buffer address and length in the request
	 */
	req->air_buf_addr = buf;
	req->air_buf_len = buf_len;

	/*
	 * Issue the IOCTL
	 */
	rc = ioctl(s, AIOCINFO, (caddr_t)req);
	if (rc) {
		free(buf);
		if (errno == ENOSPC) {
			buf_len = buf_len * 2;
			goto mem_retry;
		}
		return(-1);
	}
	(void)close(s);
	/*
	 * Set a pointer to the returned info in the request
	 * and return its length
	 */
	req->air_buf_addr = buf;
	return(req->air_buf_len);
}


/*
 * Get VCC information
 * 
 * Arguments:
 *	intf	pointer to interface name (or null string)
 *	vccp	pointer to a pointer to a struct air_vcc_rsp for the
 *		address of the returned VCC information
 *
 * Returns:
 *	int 	length of the retuned VCC information
 *
 */
ssize_t
get_vcc_info(const char *intf, struct air_vcc_rsp **vccp)
{
	size_t buf_len = sizeof(struct air_vcc_rsp) * 100;
	struct atminfreq air;

	/*
	 * Initialize IOCTL request
	 */
	air.air_opcode = AIOCS_INF_VCC;
	bzero(air.air_vcc_intf, sizeof(air.air_vcc_intf));
	if (intf != NULL && strlen(intf) != 0)
		strncpy(air.air_vcc_intf, intf, IFNAMSIZ - 1);

	buf_len = do_info_ioctl(&air, buf_len);

	/*
	 * Return a pointer to the VCC info and its length
	 */
	*vccp = (struct air_vcc_rsp *)(void *)air.air_buf_addr;
	return(buf_len);
}


/*
 * Get subnet mask
 * 
 * Arguments:
 *	intf	pointer to an interface name
 *	mask	pointer to a struct sockaddr_in to receive the mask
 *
 * Returns:
 *	0 	good completion
 *	-1 	error
 *
 */
int
get_subnet_mask(const char *intf, struct sockaddr_in *mask)
{
	int			rc, s;
	struct ifreq		req;
	struct sockaddr_in	*ip_mask;

	/*
	 * Check parameters
	 */
	if (!intf || !mask ||
			strlen(intf) == 0 ||
			strlen(intf) > IFNAMSIZ-1)
		return(-1);

	/*
	 * Open a socket for the IOCTL
	 */
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return(-1);

	/*
	 * Set up and issue the IOCTL
	 */
	bzero(&req, sizeof(req));
	strcpy(req.ifr_name, intf);
	rc = ioctl(s, SIOCGIFNETMASK, (caddr_t)&req);
	(void)close(s);
	if (rc)
		return(-1);

	/*
	 * Give the answer back to the caller
	 */
	ip_mask = (struct sockaddr_in *)(void *)&req.ifr_addr;
	*mask = *ip_mask;
	mask->sin_family = AF_INET;

	return(0);
}


/*
 * Get an interface's MTU
 * 
 * Arguments:
 *	intf	pointer to an interface name
 *	mtu	pointer to an int to receive the MTU
 *
 * Returns:
 *	>= 0 	interface MTU
 *	-1 	error
 *
 */
int
get_mtu(const char *intf)
{
	int			rc, s;
	struct ifreq		req;

	/*
	 * Check parameters
	 */
	if (!intf || strlen(intf) == 0 ||
			strlen(intf) > IFNAMSIZ-1)
		return(-1);

	/*
	 * Open a socket for the IOCTL
	 */
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return(-1);

	/*
	 * Set up and issue the IOCTL
	 */
	bzero(&req, sizeof(req));
	strcpy(req.ifr_name, intf);
	rc = ioctl(s, SIOCGIFMTU, (caddr_t)&req);
	(void)close(s);

	/*
	 * Set the appropriate return value
	 */
	if (rc)
		return(-1);
	else
	return(req.ifr_mtu);
}


/*
 * Verify netif name
 * 
 * This routine issues an IOCTL to check whether the passed string is
 * a valid network interface name.
 * 
 * Arguments:
 *	req	pointer to an ATM information request IOCTL structure
 *
 * Returns:
 *	-1		error encountered
 *	FALSE (0) 	the string is not a NIF name
 *	TRUE (> 0) 	the string is a valid NIF name
 *
 */
int
verify_nif_name(const char *name)
{
	int	rc, s;
	struct atminfreq	air;
	struct air_netif_rsp    *nif_info;

	/*
	 * Check whether name is of a valid length
	 */
	if (strlen(name) > IFNAMSIZ - 1 ||
			strlen(name) < 1) {
		return(FALSE);
	}

	/*
	 * Open a socket for the IOCTL
	 */
	s = socket(AF_ATM, SOCK_DGRAM, 0);
	if (s < 0) {
		return(-1);
	}

	/*
	 * Get memory for returned information
	 */
	nif_info = malloc(sizeof(struct air_netif_rsp));
	if (nif_info == NULL) {
		errno = ENOMEM;
		return(-1);
	}
	bzero(nif_info, sizeof(struct air_netif_rsp));

	/*
	 * Set up the request
	 */
	air.air_opcode = AIOCS_INF_NIF;
	air.air_buf_addr = (caddr_t)nif_info;
	air.air_buf_len = sizeof(struct air_netif_rsp);
	bzero(air.air_netif_intf, sizeof(air.air_netif_intf));
	strcpy(air.air_netif_intf, name);

	/*
	 * Issue the IOCTL
	 */
	rc = ioctl(s, AIOCINFO, (caddr_t)&air);
	free(nif_info);
	(void)close(s);

	/*
	 * Base return value on IOCTL return code
	 */
	if (rc)
		return(FALSE);
	else
		return(TRUE);
}

/*
 * Get Config information
 *
 * Arguments:
 *      intf    pointer to interface name (or null string)
 *      cfgp    pointer to a pointer to a struct air_cfg_rsp for the
 *              address of the returned Config information
 *
 * Returns:
 *      int     length of returned Config information
 *
 */
ssize_t
get_cfg_info(const char *intf, struct air_cfg_rsp **cfgp)
{
        size_t buf_len = sizeof(struct air_cfg_rsp) * 4;
        struct atminfreq air;

        /*
         * Initialize IOCTL request
         */
        air.air_opcode = AIOCS_INF_CFG;
        bzero ( air.air_cfg_intf, sizeof(air.air_cfg_intf));
        if ( intf != NULL && strlen(intf) != 0 )
                strncpy(air.air_cfg_intf, intf, IFNAMSIZ - 1);

        buf_len = do_info_ioctl ( &air, buf_len );

        /*
         * Return a pointer to the Config info and its length
         */
        *cfgp = (struct air_cfg_rsp *)(void *)air.air_buf_addr;
        return ( buf_len );

}

/*
 * Get Physical Interface information
 *
 * Arguments:
 *      intf    pointer to interface name (or null string)
 *      intp    pointer to a pointer to a struct air_cfg_rsp for the
 *              address of the returned Config information
 *
 * Returns:
 *      int     length of returned Config information
 *
 */
ssize_t
get_intf_info(const char *intf, struct air_int_rsp **intp)
{
        size_t buf_len = sizeof(struct air_int_rsp) * 4;
        struct atminfreq air;

        /*
         * Initialize IOCTL request
         */
        air.air_opcode = AIOCS_INF_INT;
        bzero ( air.air_int_intf, sizeof(air.air_int_intf));
        if ( intf != NULL && strlen(intf) != 0 )
                strncpy(air.air_int_intf, intf, IFNAMSIZ - 1);

        buf_len = do_info_ioctl ( &air, buf_len );
 
        /*
         * Return a pointer to the Physical Interface info and its length
         */
        *intp = (struct air_int_rsp *)(void *)air.air_buf_addr;
        return ( buf_len );

}


/*
 * Get Netif information
 *
 * Arguments:
 *      intf    pointer to interface name (or null string)
 *      netp    pointer to a pointer to a struct air_netif_rsp for the
 *              address of the returned Netif information
 *
 * Returns:
 *      int     length of returned Netif information
 *
 */
ssize_t
get_netif_info(const char *intf, struct air_netif_rsp **netp)
{
        size_t buf_len = sizeof(struct air_netif_rsp) * 10;
        struct atminfreq air;

        /*
         * Initialize IOCTL request
         */
        air.air_opcode = AIOCS_INF_NIF;
        bzero ( air.air_int_intf, sizeof(air.air_int_intf) );
        if ( intf != NULL && strlen(intf) != 0 )
                strncpy(air.air_int_intf, intf, IFNAMSIZ - 1);

        buf_len = do_info_ioctl ( &air, buf_len );

        /*
         * Return a pointer to the Netif info and its length
         */
        *netp = (struct air_netif_rsp *) air.air_buf_addr;
        return ( buf_len );

}
