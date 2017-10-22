 /*
  * refuse() reports a refused connection, and takes the consequences: in
  * case of a datagram-oriented service, the unread datagram is taken from
  * the input queue (or inetd would see the same datagram again and again);
  * the program is terminated.
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  *
  * $FreeBSD: release/7.0.0/contrib/tcp_wrappers/refuse.c 172506 2007-10-10 16:59:15Z cvs2svn $
  */

#ifndef lint
static char sccsid[] = "@(#) refuse.c 1.5 94/12/28 17:42:39";
#endif

/* System libraries. */

#include <stdio.h>
#include <syslog.h>

/* Local stuff. */

#include "tcpd.h"

/* refuse - refuse request */

void    refuse(request)
struct request_info *request;
{
#ifdef INET6
    syslog(deny_severity, "refused connect from %s (%s)",
	   eval_client(request), eval_hostaddr(request->client));
#else
    syslog(deny_severity, "refused connect from %s", eval_client(request));
#endif
    clean_exit(request);
    /* NOTREACHED */
}

