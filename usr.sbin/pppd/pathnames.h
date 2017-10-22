/*
 * define path names
 *
 * $FreeBSD: release/7.0.0/usr.sbin/pppd/pathnames.h 164222 2006-11-12 17:36:58Z ume $
 */

#ifdef HAVE_PATHS_H
#include <paths.h>

#else
#define _PATH_VARRUN 	"/etc/ppp/"
#define _PATH_DEVNULL	"/dev/null"
#endif

#define _PATH_UPAPFILE 	"/etc/ppp/pap-secrets"
#define _PATH_CHAPFILE 	"/etc/ppp/chap-secrets"
#define _PATH_SYSOPTIONS "/etc/ppp/options"
#define _PATH_IPUP	"/etc/ppp/ip-up"
#define _PATH_IPDOWN	"/etc/ppp/ip-down"
#define _PATH_AUTHUP	"/etc/ppp/auth-up"
#define _PATH_AUTHDOWN	"/etc/ppp/auth-down"
#define _PATH_TTYOPT	"/etc/ppp/options."
#define _PATH_CONNERRS	"/var/log/connect-errors"
#define _PATH_USEROPT	".ppprc"
#define _PATH_PEERFILES	"/etc/ppp/peers/"
#define _PATH_PPPDENY  "/etc/ppp/ppp.deny"
#define _PATH_PPPSHELLS	"/etc/ppp/ppp.shells"

#ifdef INET6
#define _PATH_IPV6UP	"/etc/ppp/ipv6-up"
#define _PATH_IPV6DOWN	"/etc/ppp/ipv6-down"
#endif

#ifdef IPX_CHANGE
#define _PATH_IPXUP	"/etc/ppp/ipx-up"
#define _PATH_IPXDOWN	"/etc/ppp/ipx-down"
#endif /* IPX_CHANGE */
