/*-
 * Copyright (c) 2001 Charles Mott <cm@linktel.net>
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
 * $FreeBSD: release/7.0.0/sys/netinet/libalias/alias_local.h 165243 2006-12-15 12:50:06Z piso $
 */

/*
 * Alias_local.h contains the function prototypes for alias.c,
 * alias_db.c, alias_util.c and alias_ftp.c, alias_irc.c (as well
 * as any future add-ons).  It also includes macros, globals and
 * struct definitions shared by more than one alias*.c file.
 *
 * This include file is intended to be used only within the aliasing
 * software.  Outside world interfaces are defined in alias.h
 *
 * This software is placed into the public domain with no restrictions
 * on its distribution.
 *
 * Initial version:  August, 1996  (cjm)
 *
 * <updated several times by original author and Eivind Eklund>
 */

#ifndef _ALIAS_LOCAL_H_
#define	_ALIAS_LOCAL_H_

#include <sys/types.h>
#include <sys/sysctl.h>

#ifdef _KERNEL
#include <sys/malloc.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>

/* XXX: LibAliasSetTarget() uses this constant. */
#define	INADDR_NONE	0xffffffff
#endif

/* Sizes of input and output link tables */
#define LINK_TABLE_OUT_SIZE         101
#define LINK_TABLE_IN_SIZE         4001

struct proxy_entry;

struct libalias {
	LIST_ENTRY(libalias) instancelist;

	int		packetAliasMode;	/* Mode flags                      */
	/* - documented in alias.h  */

	struct in_addr	aliasAddress;	/* Address written onto source     */
	/* field of IP packet.           */

	struct in_addr	targetAddress;	/* IP address incoming packets     */
	/* are sent to if no aliasing    */
	/* link already exists           */

	struct in_addr	nullAddress;	/* Used as a dummy parameter for   */
	/* some function calls           */

			LIST_HEAD     (, alias_link) linkTableOut[LINK_TABLE_OUT_SIZE];
	/* Lookup table of pointers to     */
	/* chains of link records. Each  */

			LIST_HEAD     (, alias_link) linkTableIn[LINK_TABLE_IN_SIZE];
	/* link record is doubly indexed */
	/* into input and output lookup  */
	/* tables.                       */

	/* Link statistics                 */
	int		icmpLinkCount;
	int		udpLinkCount;
	int		tcpLinkCount;
	int		pptpLinkCount;
	int		protoLinkCount;
	int		fragmentIdLinkCount;
	int		fragmentPtrLinkCount;
	int		sockCount;

	int		cleanupIndex;	/* Index to chain of link table    */
	/* being inspected for old links   */

	int		timeStamp;	/* System time in seconds for      */
	/* current packet                  */

	int		lastCleanupTime;	/* Last time
						 * IncrementalCleanup()  */
	/* was called                      */

	int		houseKeepingResidual;	/* used by HouseKeeping()          */

	int		deleteAllLinks;	/* If equal to zero, DeleteLink()  */
	/* will not remove permanent links */
	
	/* log descriptor        */ 
#ifdef  _KERNEL
	char           *logDesc;        
#else 
	FILE           *logDesc;	
#endif
	/* statistics monitoring */

	int		newDefaultLink;	/* Indicates if a new aliasing     */
	/* link has been created after a   */
	/* call to PacketAliasIn/Out().    */

#ifndef NO_FW_PUNCH
	int		fireWallFD;	/* File descriptor to be able to   */
	/* control firewall.  Opened by    */
	/* PacketAliasSetMode on first     */
	/* setting the PKT_ALIAS_PUNCH_FW  */
	/* flag.                           */
	int		fireWallBaseNum;	/* The first firewall entry
						 * free for our use */
	int		fireWallNumNums;	/* How many entries can we
						 * use? */
	int		fireWallActiveNum;	/* Which entry did we last
						 * use? */
	char           *fireWallField;	/* bool array for entries */
#endif

	unsigned int	skinnyPort;	/* TCP port used by the Skinny     */
	/* protocol.                       */

	struct proxy_entry *proxyList;

	struct in_addr	true_addr;	/* in network byte order. */
	u_short		true_port;	/* in host byte order. */
#ifdef  _KERNEL
	/* 
	 * avoid races in libalias: every public function has to use it.
	 */
	struct mtx mutex;
#endif
};

/* Macros */

#ifdef _KERNEL
#define LIBALIAS_LOCK_INIT(l) \
        mtx_init(&l->mutex, "per-instance libalias mutex", NULL, MTX_DEF)
#define LIBALIAS_LOCK_ASSERT(l) mtx_assert(&l->mutex, MA_OWNED)
#define LIBALIAS_LOCK(l) mtx_lock(&l->mutex)
#define LIBALIAS_UNLOCK(l) mtx_unlock(&l->mutex)
#define LIBALIAS_LOCK_DESTROY(l)	mtx_destroy(&l->mutex)
#else
#define LIBALIAS_LOCK_INIT(l)
#define LIBALIAS_LOCK_ASSERT(l)
#define LIBALIAS_LOCK(l)
#define LIBALIAS_UNLOCK(l)
#define LIBALIAS_LOCK_DESTROY(l)
#endif

/*
 * The following macro is used to update an
 * internet checksum.  "delta" is a 32-bit
 * accumulation of all the changes to the
 * checksum (adding in new 16-bit words and
 * subtracting out old words), and "cksum"
 * is the checksum value to be updated.
 */
#define	ADJUST_CHECKSUM(acc, cksum) \
	do { \
		acc += cksum; \
		if (acc < 0) { \
			acc = -acc; \
			acc = (acc >> 16) + (acc & 0xffff); \
			acc += acc >> 16; \
			cksum = (u_short) ~acc; \
		} else { \
			acc = (acc >> 16) + (acc & 0xffff); \
			acc += acc >> 16; \
			cksum = (u_short) acc; \
		} \
	} while (0)


/* Prototypes */

/*
 * We do not calculate TCP checksums when libalias is a kernel
 * module, since it has no idea about checksum offloading.
 * If TCP data has changed, then we just set checksum to zero,
 * and caller must recalculate it himself.
 * In case if libalias will edit UDP data, the same approach
 * should be used.
 */
#ifndef _KERNEL
u_short		IpChecksum(struct ip *_pip);
u_short		TcpChecksum(struct ip *_pip);
#endif
void
DifferentialChecksum(u_short * _cksum, void * _new, void * _old, int _n);

/* Internal data access */
struct alias_link *
FindIcmpIn(struct libalias *la, struct in_addr _dst_addr, struct in_addr _alias_addr,
    u_short _id_alias, int _create);
struct alias_link *
FindIcmpOut(struct libalias *la, struct in_addr _src_addr, struct in_addr _dst_addr,
    u_short _id, int _create);
struct alias_link *
FindFragmentIn1(struct libalias *la, struct in_addr _dst_addr, struct in_addr _alias_addr,
    u_short _ip_id);
struct alias_link *
FindFragmentIn2(struct libalias *la, struct in_addr _dst_addr, struct in_addr _alias_addr,
    u_short _ip_id);
struct alias_link *
		AddFragmentPtrLink(struct libalias *la, struct in_addr _dst_addr, u_short _ip_id);
struct alias_link *
		FindFragmentPtr(struct libalias *la, struct in_addr _dst_addr, u_short _ip_id);
struct alias_link *
FindProtoIn(struct libalias *la, struct in_addr _dst_addr, struct in_addr _alias_addr,
    u_char _proto);
struct alias_link *
FindProtoOut(struct libalias *la, struct in_addr _src_addr, struct in_addr _dst_addr,
    u_char _proto);
struct alias_link *
FindUdpTcpIn(struct libalias *la, struct in_addr _dst_addr, struct in_addr _alias_addr,
    u_short _dst_port, u_short _alias_port, u_char _proto, int _create);
struct alias_link *
FindUdpTcpOut(struct libalias *la, struct in_addr _src_addr, struct in_addr _dst_addr,
    u_short _src_port, u_short _dst_port, u_char _proto, int _create);
struct alias_link *
AddPptp(struct libalias *la, struct in_addr _src_addr, struct in_addr _dst_addr,
    struct in_addr _alias_addr, u_int16_t _src_call_id);
struct alias_link *
FindPptpOutByCallId(struct libalias *la, struct in_addr _src_addr,
    struct in_addr _dst_addr, u_int16_t _src_call_id);
struct alias_link *
FindPptpInByCallId(struct libalias *la, struct in_addr _dst_addr,
    struct in_addr _alias_addr, u_int16_t _dst_call_id);
struct alias_link *
FindPptpOutByPeerCallId(struct libalias *la, struct in_addr _src_addr,
    struct in_addr _dst_addr, u_int16_t _dst_call_id);
struct alias_link *
FindPptpInByPeerCallId(struct libalias *la, struct in_addr _dst_addr,
    struct in_addr _alias_addr, u_int16_t _alias_call_id);
struct alias_link *
FindRtspOut(struct libalias *la, struct in_addr _src_addr, struct in_addr _dst_addr,
    u_short _src_port, u_short _alias_port, u_char _proto);
struct in_addr
		FindOriginalAddress(struct libalias *la, struct in_addr _alias_addr);
struct in_addr
		FindAliasAddress(struct libalias *la, struct in_addr _original_addr);

/* External data access/modification */
int
FindNewPortGroup(struct libalias *la, struct in_addr _dst_addr, struct in_addr _alias_addr,
    u_short _src_port, u_short _dst_port, u_short _port_count,
    u_char _proto, u_char _align);
void		GetFragmentAddr(struct alias_link *_lnk, struct in_addr *_src_addr);
void		SetFragmentAddr(struct alias_link *_lnk, struct in_addr _src_addr);
void		GetFragmentPtr(struct alias_link *_lnk, char **_fptr);
void		SetFragmentPtr(struct alias_link *_lnk, char *fptr);
void		SetStateIn(struct alias_link *_lnk, int _state);
void		SetStateOut(struct alias_link *_lnk, int _state);
int		GetStateIn (struct alias_link *_lnk);
int		GetStateOut(struct alias_link *_lnk);
struct in_addr
		GetOriginalAddress(struct alias_link *_lnk);
struct in_addr
		GetDestAddress(struct alias_link *_lnk);
struct in_addr
		GetAliasAddress(struct alias_link *_lnk);
struct in_addr
		GetDefaultAliasAddress(struct libalias *la);
void		SetDefaultAliasAddress(struct libalias *la, struct in_addr _alias_addr);
u_short		GetOriginalPort(struct alias_link *_lnk);
u_short		GetAliasPort(struct alias_link *_lnk);
struct in_addr
		GetProxyAddress(struct alias_link *_lnk);
void		SetProxyAddress(struct alias_link *_lnk, struct in_addr _addr);
u_short		GetProxyPort(struct alias_link *_lnk);
void		SetProxyPort(struct alias_link *_lnk, u_short _port);
void		SetAckModified(struct alias_link *_lnk);
int		GetAckModified(struct alias_link *_lnk);
int		GetDeltaAckIn(struct ip *_pip, struct alias_link *_lnk);
int		GetDeltaSeqOut(struct ip *_pip, struct alias_link *_lnk);
void		AddSeq    (struct ip *_pip, struct alias_link *_lnk, int _delta);
void		SetExpire (struct alias_link *_lnk, int _expire);
void		ClearCheckNewLink(struct libalias *la);
void		SetProtocolFlags(struct alias_link *_lnk, int _pflags);
int		GetProtocolFlags(struct alias_link *_lnk);
void		SetDestCallId(struct alias_link *_lnk, u_int16_t _cid);

#ifndef NO_FW_PUNCH
void		PunchFWHole(struct alias_link *_lnk);

#endif

/* Housekeeping function */
void		HouseKeeping(struct libalias *);

/* Tcp specfic routines */
/* lint -save -library Suppress flexelint warnings */

/* Transparent proxy routines */
int
ProxyCheck(struct libalias *la, struct ip *_pip, struct in_addr *_proxy_server_addr,
    u_short * _proxy_server_port);
void
ProxyModify(struct libalias *la, struct alias_link *_lnk, struct ip *_pip,
    int _maxpacketsize, int _proxy_type);

enum alias_tcp_state {
	ALIAS_TCP_STATE_NOT_CONNECTED,
	ALIAS_TCP_STATE_CONNECTED,
	ALIAS_TCP_STATE_DISCONNECTED
};

#if defined(_NETINET_IP_H_)
static __inline void *
ip_next(struct ip *iphdr)
{
	char *p = (char *)iphdr;
	return (&p[iphdr->ip_hl * 4]);
}
#endif

#if defined(_NETINET_TCP_H_)
static __inline void *
tcp_next(struct tcphdr *tcphdr)
{
	char *p = (char *)tcphdr;
	return (&p[tcphdr->th_off * 4]);
}
#endif

#if defined(_NETINET_UDP_H_)
static __inline void *
udp_next(struct udphdr *udphdr)
{
	return ((void *)(udphdr + 1));
}
#endif

#endif				/* !_ALIAS_LOCAL_H_ */
