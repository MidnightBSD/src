/*	$FreeBSD: src/contrib/ipfilter/ip_fil.c,v 1.3 2005/04/26 15:18:45 darrenr Exp $	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#if !defined(lint)
static const char sccsid[] = "@(#)ip_fil.c	2.41 6/5/96 (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)$Id: ip_fil.c,v 1.1.1.2 2006-02-25 02:33:22 laffer1 Exp $";
#endif

#ifndef	SOLARIS
#define	SOLARIS	(defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif

#include <sys/param.h>
#if defined(__FreeBSD__) && !defined(__FreeBSD_version)
# if defined(IPFILTER_LKM)
#  ifndef __FreeBSD_cc_version
#   include <osreldate.h>
#  else
#   if __FreeBSD_cc_version < 430000
#    include <osreldate.h>
#   endif
#  endif
# endif
#endif
#include <sys/errno.h>
#if defined(__hpux) && (HPUXREV >= 1111) && !defined(_KERNEL)
# include <sys/kern_svcs.h>
#endif
#include <sys/types.h>
#define _KERNEL
#define KERNEL
#ifdef __OpenBSD__
struct file;
#endif
#include <sys/uio.h>
#undef _KERNEL
#undef KERNEL
#include <sys/file.h>
#include <sys/ioctl.h>
#ifdef __sgi
# include <sys/ptimers.h>
#endif
#include <sys/time.h>
#if !SOLARIS
# if (NetBSD > 199609) || (OpenBSD > 199603) || (__FreeBSD_version >= 300000)
#  include <sys/dirent.h>
# else
#  include <sys/dir.h>
# endif
#else
# include <sys/filio.h>
#endif
#ifndef linux
# include <sys/protosw.h>
#endif
#include <sys/socket.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <arpa/inet.h>

#ifdef __hpux
# define _NET_ROUTE_INCLUDED
#endif
#include <net/if.h>
#ifdef sun
# include <net/af.h>
#endif
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#ifdef __sgi
#include <sys/debug.h>
# ifdef IFF_DRVRLOCK /* IRIX6 */
#include <sys/hashing.h>
# endif
#endif
#if defined(__FreeBSD__)
# include "radix_ipf.h"
#endif
#include <net/route.h>
#include <netinet/in.h>
#if !(defined(__sgi) && !defined(IFF_DRVRLOCK)) /* IRIX < 6 */ && \
    !defined(__hpux) && !defined(linux)
# include <netinet/in_var.h>
#endif
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#if !defined(linux)
# include <netinet/ip_var.h>
#endif
#include <netinet/tcp.h>
#if defined(__osf__)
# include <netinet/tcp_timer.h>
#endif
#if defined(__osf__) || defined(__hpux) || defined(__sgi)
# include "radix_ipf_local.h"
# define _RADIX_H_
#endif
#include <netinet/udp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>
#include <syslog.h>
#ifdef __hpux
# undef _NET_ROUTE_INCLUDED
#endif
#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_state.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_auth.h"
#ifdef	IPFILTER_SYNC
#include "netinet/ip_sync.h"
#endif
#ifdef	IPFILTER_SCAN
#include "netinet/ip_scan.h"
#endif
#include "netinet/ip_pool.h"
#ifdef IPFILTER_COMPILED
# include "netinet/ip_rules.h"
#endif
#if defined(__FreeBSD_version) && (__FreeBSD_version >= 300000)
# include <sys/malloc.h>
#endif
#ifdef __hpux
struct rtentry;
#endif
#include "md5.h"


#if !defined(__osf__)
extern	struct	protosw	inetsw[];
#endif

#include "ipt.h"
static	struct	ifnet **ifneta = NULL;
static	int	nifs = 0;

static	int	frzerostats __P((caddr_t));
static	void	fr_setifpaddr __P((struct ifnet *, char *));
void	init_ifp __P((void));
#if defined(__sgi) && (IRIX < 60500)
static int 	no_output __P((struct ifnet *, struct mbuf *,
			       struct sockaddr *));
static int	write_output __P((struct ifnet *, struct mbuf *,
				  struct sockaddr *));
#else
# if TRU64 >= 1885
static int 	no_output __P((struct ifnet *, struct mbuf *,
			       struct sockaddr *, struct rtentry *, char *));
static int	write_output __P((struct ifnet *, struct mbuf *,
				  struct sockaddr *, struct rtentry *, char *));
# else
static int 	no_output __P((struct ifnet *, struct mbuf *,
			       struct sockaddr *, struct rtentry *));
static int	write_output __P((struct ifnet *, struct mbuf *,
				  struct sockaddr *, struct rtentry *));
# endif
#endif


int iplattach()
{
	fr_running = 1;
	return 0;
}


int ipldetach()
{
	fr_running = -1;
	return 0;
}


static	int	frzerostats(data)
caddr_t	data;
{
	friostat_t fio;
	int error;

	fr_getstat(&fio);
	error = copyoutptr(&fio, data, sizeof(fio));
	if (error)
		return EFAULT;

	bzero((char *)frstats, sizeof(*frstats) * 2);

	return 0;
}


/*
 * Filter ioctl interface.
 */
int iplioctl(dev, cmd, data, mode)
int dev;
ioctlcmd_t cmd;
caddr_t data;
int mode;
{
	int error = 0, unit = 0, tmp;
	friostat_t fio;

	unit = dev;

	SPL_NET(s);

	if (unit == IPL_LOGNAT) {
		if (fr_running > 0)
			error = fr_nat_ioctl(data, cmd, mode);
		else
			error = EIO;
		SPL_X(s);
		return error;
	}
	if (unit == IPL_LOGSTATE) {
		if (fr_running > 0)
			error = fr_state_ioctl(data, cmd, mode);
		else
			error = EIO;
		SPL_X(s);
		return error;
	}
	if (unit == IPL_LOGAUTH) {
		if (fr_running > 0) {
			if ((cmd == (ioctlcmd_t)SIOCADAFR) ||
			    (cmd == (ioctlcmd_t)SIOCRMAFR)) {
				if (!(mode & FWRITE)) {
					error = EPERM;
				} else {
					error = frrequest(unit, cmd, data,
							  fr_active, 1);
				}
			} else {
				error = fr_auth_ioctl(data, mode, cmd);
			}
		} else
			error = EIO;
		SPL_X(s);
		return error;
	}
	if (unit == IPL_LOGSYNC) {
#ifdef	IPFILTER_SYNC
		if (fr_running > 0)
			error = fr_sync_ioctl(data, cmd, mode);
		else
#endif
			error = EIO;
		SPL_X(s);
		return error;
	}
	if (unit == IPL_LOGSCAN) {
#ifdef	IPFILTER_SCAN
		if (fr_running > 0)
			error = fr_scan_ioctl(data, cmd, mode);
		else
#endif
			error = EIO;
		SPL_X(s);
		return error;
	}
	if (unit == IPL_LOGLOOKUP) {
		if (fr_running > 0)
			error = ip_lookup_ioctl(data, cmd, mode);
		else
			error = EIO;
		SPL_X(s);
		return error;
	}

	switch (cmd)
	{
	case FIONREAD :
#ifdef IPFILTER_LOG
		error = COPYOUT(&iplused[IPL_LOGIPF], (caddr_t)data,
			       sizeof(iplused[IPL_LOGIPF]));
#endif
		break;
	case SIOCFRENB :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = COPYIN(data, &tmp, sizeof(tmp));
			if (error)
				break;
			if (tmp)
				error = iplattach();
			else
				error = ipldetach();
		}
		break;
	case SIOCIPFSET :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
	case SIOCIPFGETNEXT :
	case SIOCIPFGET :
		error = fr_ipftune(cmd, (void *)data);
		break;
	case SIOCSETFF :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = COPYIN(data, &fr_flags, sizeof(fr_flags));
		break;
	case SIOCGETFF :
		error = COPYOUT(&fr_flags, data, sizeof(fr_flags));
		break;
	case SIOCFUNCL :
		error = fr_resolvefunc(data);
		break;
	case SIOCINAFR :
	case SIOCRMAFR :
	case SIOCADAFR :
	case SIOCZRLST :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frrequest(unit, cmd, data, fr_active, 1);
		break;
	case SIOCINIFR :
	case SIOCRMIFR :
	case SIOCADIFR :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frrequest(unit, cmd, data, 1 - fr_active, 1);
		break;
	case SIOCSWAPA :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			bzero((char *)frcache, sizeof(frcache[0]) * 2);
			*(u_int *)data = fr_active;
			fr_active = 1 - fr_active;
		}
		break;
	case SIOCGETFS :
		fr_getstat(&fio);
		error = fr_outobj(data, &fio, IPFOBJ_IPFSTAT);
		break;
	case	SIOCFRZST :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frzerostats(data);
		break;
	case	SIOCIPFFL :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = COPYIN(data, &tmp, sizeof(tmp));
			if (!error) {
				tmp = frflush(unit, 4, tmp);
				error = COPYOUT(&tmp, data, sizeof(tmp));
			}
		}
		break;
#ifdef	USE_INET6
	case	SIOCIPFL6 :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = COPYIN(data, &tmp, sizeof(tmp));
			if (!error) {
				tmp = frflush(unit, 6, tmp);
				error = COPYOUT(&tmp, data, sizeof(tmp));
			}
		}
		break;
#endif
	case SIOCSTLCK :
		error = COPYIN(data, &tmp, sizeof(tmp));
		if (error == 0) {
			fr_state_lock = tmp;
			fr_nat_lock = tmp;
			fr_frag_lock = tmp;
			fr_auth_lock = tmp;
		} else
			error = EFAULT;
		break;
#ifdef	IPFILTER_LOG
	case	SIOCIPFFB :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			*(int *)data = ipflog_clear(unit);
		break;
#endif /* IPFILTER_LOG */
	case SIOCGFRST :
		error = fr_outobj(data, fr_fragstats(), IPFOBJ_FRAGSTAT);
		break;
	case SIOCFRSYN :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			frsync(NULL);
		}
		break;
	default :
		error = EINVAL;
		break;
	}
	SPL_X(s);
	return error;
}


void fr_forgetifp(ifp)
void *ifp;
{
	register frentry_t *f;

	WRITE_ENTER(&ipf_mutex);
	for (f = ipacct[0][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipacct[1][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipfilter[0][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipfilter[1][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
#ifdef	USE_INET6
	for (f = ipacct6[0][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipacct6[1][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipfilter6[0][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipfilter6[1][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
#endif
	RWLOCK_EXIT(&ipf_mutex);
	fr_natsync(ifp);
}


#if defined(__sgi) && (IRIX < 60500)
static int no_output(ifp, m, s)
#else
# if TRU64 >= 1885
static int no_output (ifp, m, s, rt, cp)
char *cp;
# else
static int no_output(ifp, m, s, rt)
# endif
struct rtentry *rt;
#endif
struct ifnet *ifp;
struct mbuf *m;
struct sockaddr *s;
{
	return 0;
}


#if defined(__sgi) && (IRIX < 60500)
static int write_output(ifp, m, s)
#else
# if TRU64 >= 1885
static int write_output (ifp, m, s, rt, cp)
char *cp;
# else
static int write_output(ifp, m, s, rt)
# endif
struct rtentry *rt;
#endif
struct ifnet *ifp;
struct mbuf *m;
struct sockaddr *s;
{
	char fname[32];
	mb_t *mb;
	ip_t *ip;
	int fd;

	mb = (mb_t *)m;
	ip = MTOD(mb, ip_t *);

#if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199606)) || \
    (defined(OpenBSD) && (OpenBSD >= 199603)) || defined(linux) || \
    (defined(__FreeBSD__) && (__FreeBSD_version >= 501113))
	sprintf(fname, "/tmp/%s", ifp->if_xname);
#else
	sprintf(fname, "/tmp/%s%d", ifp->if_name, ifp->if_unit);
#endif
	fd = open(fname, O_WRONLY|O_APPEND);
	if (fd == -1) {
		perror("open");
		return -1;
	}
	write(fd, (char *)ip, ntohs(ip->ip_len));
	close(fd);
	return 0;
}


static void fr_setifpaddr(ifp, addr)
struct ifnet *ifp;
char *addr;
{
#ifdef __sgi
	struct in_ifaddr *ifa;
#else
	struct ifaddr *ifa;
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__FreeBSD__)
	if (ifp->if_addrlist.tqh_first != NULL)
#else
# ifdef __sgi
	if (ifp->in_ifaddr != NULL)
# else
	if (ifp->if_addrlist != NULL)
# endif
#endif
		return;

	ifa = (struct ifaddr *)malloc(sizeof(*ifa));
#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__FreeBSD__)
	ifp->if_addrlist.tqh_first = ifa;
#else
# ifdef __sgi
	ifp->in_ifaddr = ifa;
# else
	ifp->if_addrlist = ifa;
# endif
#endif

	if (ifa != NULL) {
		struct sockaddr_in *sin;

#ifdef __sgi
		sin = (struct sockaddr_in *)&ifa->ia_addr;
#else
		sin = (struct sockaddr_in *)&ifa->ifa_addr;
#endif
		sin->sin_addr.s_addr = inet_addr(addr);
		if (sin->sin_addr.s_addr == 0)
			abort();
	}
}

struct ifnet *get_unit(name, v)
char *name;
int v;
{
	struct ifnet *ifp, **ifpp, **old_ifneta;
	char *addr;
#if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199606)) || \
    (defined(OpenBSD) && (OpenBSD >= 199603)) || defined(linux) || \
    (defined(__FreeBSD__) && (__FreeBSD_version >= 501113))

	if (name == NULL)
		name = "anon0";

	addr = strchr(name, '=');
	if (addr != NULL)
		*addr++ = '\0';

	for (ifpp = ifneta; ifpp && (ifp = *ifpp); ifpp++) {
		if (!strcmp(name, ifp->if_xname)) {
			if (addr != NULL)
				fr_setifpaddr(ifp, addr);
			return ifp;
		}
	}
#else
	char *s, ifname[LIFNAMSIZ+1];

	if (name == NULL)
		name = "anon0";

	addr = strchr(name, '=');
	if (addr != NULL)
		*addr++ = '\0';

	for (ifpp = ifneta; ifpp && (ifp = *ifpp); ifpp++) {
		COPYIFNAME(ifp, ifname);
		if (!strcmp(name, ifname)) {
			if (addr != NULL)
				fr_setifpaddr(ifp, addr);
			return ifp;
		}
	}
#endif

	if (!ifneta) {
		ifneta = (struct ifnet **)malloc(sizeof(ifp) * 2);
		if (!ifneta)
			return NULL;
		ifneta[1] = NULL;
		ifneta[0] = (struct ifnet *)calloc(1, sizeof(*ifp));
		if (!ifneta[0]) {
			free(ifneta);
			return NULL;
		}
		nifs = 1;
	} else {
		old_ifneta = ifneta;
		nifs++;
		ifneta = (struct ifnet **)realloc(ifneta,
						  (nifs + 1) * sizeof(ifp));
		if (!ifneta) {
			free(old_ifneta);
			nifs = 0;
			return NULL;
		}
		ifneta[nifs] = NULL;
		ifneta[nifs - 1] = (struct ifnet *)malloc(sizeof(*ifp));
		if (!ifneta[nifs - 1]) {
			nifs--;
			return NULL;
		}
	}
	ifp = ifneta[nifs - 1];

#if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199606)) || \
    (defined(OpenBSD) && (OpenBSD >= 199603)) || defined(linux) || \
    (defined(__FreeBSD__) && (__FreeBSD_version >= 501113))
	(void) strncpy(ifp->if_xname, name, sizeof(ifp->if_xname));
#else
	for (s = name; *s && !ISDIGIT(*s); s++)
		;
	if (*s && ISDIGIT(*s)) {
		ifp->if_unit = atoi(s);
		ifp->if_name = (char *)malloc(s - name + 1);
		(void) strncpy(ifp->if_name, name, s - name);
		ifp->if_name[s - name] = '\0';
	} else {
		ifp->if_name = strdup(name);
		ifp->if_unit = -1;
	}
#endif
	ifp->if_output = no_output;

	if (addr != NULL) {
		fr_setifpaddr(ifp, addr);
	}

	return ifp;
}


char *get_ifname(ifp)
struct ifnet *ifp;
{
	static char ifname[LIFNAMSIZ];

#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(linux) || \
    (defined(__FreeBSD__) && (__FreeBSD_version >= 501113))
	sprintf(ifname, "%s", ifp->if_xname);
#else
	sprintf(ifname, "%s%d", ifp->if_name, ifp->if_unit);
#endif
	return ifname;
}



void init_ifp()
{
	struct ifnet *ifp, **ifpp;
	char fname[32];
	int fd;

#if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199606)) || \
    (defined(OpenBSD) && (OpenBSD >= 199603)) || defined(linux) || \
    (defined(__FreeBSD__) && (__FreeBSD_version >= 501113))
	for (ifpp = ifneta; ifpp && (ifp = *ifpp); ifpp++) {
		ifp->if_output = write_output;
		sprintf(fname, "/tmp/%s", ifp->if_xname);
		fd = open(fname, O_WRONLY|O_CREAT|O_EXCL|O_TRUNC, 0600);
		if (fd == -1)
			perror("open");
		else
			close(fd);
	}
#else

	for (ifpp = ifneta; ifpp && (ifp = *ifpp); ifpp++) {
		ifp->if_output = write_output;
		sprintf(fname, "/tmp/%s%d", ifp->if_name, ifp->if_unit);
		fd = open(fname, O_WRONLY|O_CREAT|O_EXCL|O_TRUNC, 0600);
		if (fd == -1)
			perror("open");
		else
			close(fd);
	}
#endif
}


int fr_fastroute(m, mpp, fin, fdp)
mb_t *m, **mpp;
fr_info_t *fin;
frdest_t *fdp;
{
	struct ifnet *ifp = fdp->fd_ifp;
	ip_t *ip = fin->fin_ip;

	if (!ifp)
		return 0;	/* no routing table out here */

	ip->ip_len = htons((u_short)ip->ip_len);
	ip->ip_off = htons((u_short)(ip->ip_off | IP_MF));
	ip->ip_sum = 0;
#if defined(__sgi) && (IRIX < 60500)
	(*ifp->if_output)(ifp, (void *)ip, NULL);
# if TRU64 >= 1885
	(*ifp->if_output)(ifp, (void *)m, NULL, 0, 0);
# else
	(*ifp->if_output)(ifp, (void *)m, NULL, 0);
# endif
#endif
	return 0;
}


int fr_send_reset(fin)
fr_info_t *fin;
{
	verbose("- TCP RST sent\n");
	return 0;
}


int fr_send_icmp_err(type, fin, dst)
int type;
fr_info_t *fin;
int dst;
{
	verbose("- ICMP unreachable sent\n");
	return 0;
}


void frsync(ifp)
void *ifp;
{
	return;
}


void m_freem(m)
mb_t *m;
{
	return;
}


void m_copydata(m, off, len, cp)
mb_t *m;
int off, len;
caddr_t cp;
{
	bcopy((char *)m + off, cp, len);
}


int ipfuiomove(buf, len, rwflag, uio)
caddr_t buf;
int len, rwflag;
struct uio *uio;
{
	int left, ioc, num, offset;
	struct iovec *io;
	char *start;

	if (rwflag == UIO_READ) {
		left = len;
		ioc = 0;

		offset = uio->uio_offset;

		while ((left > 0) && (ioc < uio->uio_iovcnt)) {
			io = uio->uio_iov + ioc;
			num = io->iov_len;
			if (num > left)
				num = left;
			start = (char *)io->iov_base + offset;
			if (start > (char *)io->iov_base + io->iov_len) {
				offset -= io->iov_len;
				ioc++;
				continue;
			}
			bcopy(buf, start, num);
			uio->uio_resid -= num;
			uio->uio_offset += num;
			left -= num;
			if (left > 0)
				ioc++;
		}
		if (left > 0)
			return EFAULT;
	}
	return 0;
}


u_32_t fr_newisn(fin)
fr_info_t *fin;
{
	static int iss_seq_off = 0;
	u_char hash[16];
	u_32_t newiss;
	MD5_CTX ctx;

	/*
	 * Compute the base value of the ISS.  It is a hash
	 * of (saddr, sport, daddr, dport, secret).
	 */
	MD5Init(&ctx);

	MD5Update(&ctx, (u_char *) &fin->fin_fi.fi_src,
		  sizeof(fin->fin_fi.fi_src));
	MD5Update(&ctx, (u_char *) &fin->fin_fi.fi_dst,
		  sizeof(fin->fin_fi.fi_dst));
	MD5Update(&ctx, (u_char *) &fin->fin_dat, sizeof(fin->fin_dat));

	/* MD5Update(&ctx, ipf_iss_secret, sizeof(ipf_iss_secret)); */

	MD5Final(hash, &ctx);

	memcpy(&newiss, hash, sizeof(newiss));

	/*
	 * Now increment our "timer", and add it in to
	 * the computed value.
	 *
	 * XXX Use `addin'?
	 * XXX TCP_ISSINCR too large to use?
	 */
	iss_seq_off += 0x00010000;
	newiss += iss_seq_off;
	return newiss;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_nextipid                                                 */
/* Returns:     int - 0 == success, -1 == error (packet should be droppped) */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* Returns the next IPv4 ID to use for this packet.                         */
/* ------------------------------------------------------------------------ */
INLINE u_short fr_nextipid(fin)
fr_info_t *fin;
{
	static u_short ipid = 0;
	u_short id;

	MUTEX_ENTER(&ipf_rw);
	id = ipid++;
	MUTEX_EXIT(&ipf_rw);

	return id;
}


INLINE void fr_checkv4sum(fin)
fr_info_t *fin;
{
	if (fr_checkl4sum(fin) == -1)
		fin->fin_flx |= FI_BAD;
}


#ifdef	USE_INET6
INLINE void fr_checkv6sum(fin)
fr_info_t *fin;
{
	if (fr_checkl4sum(fin) == -1)
		fin->fin_flx |= FI_BAD;
}
#endif


/*
 * See above for description, except that all addressing is in user space.
 */
int copyoutptr(src, dst, size)
void *src, *dst;
size_t size;
{
	caddr_t ca;

	bcopy(dst, (char *)&ca, sizeof(ca));
	bcopy(src, ca, size);
	return 0;
}


/*
 * See above for description, except that all addressing is in user space.
 */
int copyinptr(src, dst, size)
void *src, *dst;
size_t size;
{
	caddr_t ca;

	bcopy(src, (char *)&ca, sizeof(ca));
	bcopy(ca, dst, size);
	return 0;
}


/*
 * return the first IP Address associated with an interface
 */
int fr_ifpaddr(v, atype, ifptr, inp, inpmask)
int v, atype;
void *ifptr;
struct in_addr *inp, *inpmask;
{
	struct ifnet *ifp = ifptr;
#ifdef __sgi
	struct in_ifaddr *ifa;
#else
	struct ifaddr *ifa;
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__FreeBSD__)
	ifa = ifp->if_addrlist.tqh_first;
#else
# ifdef __sgi
	ifa = (struct in_ifaddr *)ifp->in_ifaddr;
# else
	ifa = ifp->if_addrlist;
# endif
#endif
	if (ifa != NULL) {
		struct sockaddr_in *sin, mask;

		mask.sin_addr.s_addr = 0xffffffff;

#ifdef __sgi
		sin = (struct sockaddr_in *)&ifa->ia_addr;
#else
		sin = (struct sockaddr_in *)&ifa->ifa_addr;
#endif

		return fr_ifpfillv4addr(atype, sin, &mask, inp, inpmask);
	}
	return 0;
}
